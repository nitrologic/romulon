// romulon.cpp
// (c)2026 nitrologic
// mit license

#include <cstddef>
#include <cstdint>

#include <bitset>
#include <ostream>
#include <iostream>
#include <iomanip>
#include <string>

#include <sys/types.h>
#include <vector>
#include <optional>

#include "picosdk/picosdk.h"
#include "snoop.h"

// state vars modified by updateRPC

bool snoopConnected=false;	
bool snoopReady=false;

// incoming romulon.json
// std::string picoTitle = "romulon 0.9.1";

std::string stringify(std::string text){
	const char *hex = "0123456789abcdef";
	std::string out;
	out.reserve(1024);
	out.push_back('"');
	for (unsigned char c : text){
		switch (c){
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b";  break;
			case '\f': out += "\\f";  break;
			case '\n': out += "\\n";  break;
			case '\r': out += "\\r";  break;
			case '\t': out += "\\t";  break;
			default:
				if (c < 0x20){
					out += "\\u00";
					out += hex[(c >> 4) & 0x0F];
					out += hex[c & 0x0F];
				}else{
					out.push_back(static_cast<char>(c));
				}
			}
	}
	out.push_back('"');
	return out;
}

struct GPIOSample {
	uint32_t timestamp;
	uint32_t gpio;
};


//#define USE_SAMPLEROM
#define USE_SNOOP

std::vector<GPIOSample> pinBuffer;

std::string pinStamp(uint32_t t,uint32_t pins);

#define MAX_SAMPLES 4096

uint32_t captureBuffer[MAX_SAMPLES * 2];   // tick + pins pairs

uint32_t epoch=0; // pio timestamps decrement from here

void snoopRom(uint32_t trigger_mask, bool trigger_on_low) {
    pinBuffer.clear();
    pinBuffer.reserve(MAX_SAMPLES);
    if (trigger_mask != 0) {
        uint32_t pins;
        do {
            pins = gpio_get_all();
            sleep_us(1);
        } while (((pins & trigger_mask) == 0) == trigger_on_low);
    }
    if (!beginSnoop(captureBuffer, MAX_SAMPLES)) {
        rpcSend("error", "beginSnoop failure");
        return;
    }
    while (!snoopComplete()) {
        watchdog_update();
        tud_task();
        sleep_ms(1);
    }
//    watchdog_enable(WatchdogTimeout, true);
    size_t count = getCapturedSampleCount();
    if (count > MAX_SAMPLES) count = MAX_SAMPLES;
    // Process the captured data
    for (size_t i = 0; i < count; i++) {
        uint32_t tick  = captureBuffer[i * 2];
        uint32_t pins  = captureBuffer[i * 2 + 1];
        rpcSend("sample",pinStamp(tick,pins));
        if ((i & 3) == 3) {
            watchdog_update();
            tud_task();
            cdcFlush();
            sleep_ms(5);
        }
    }
    rpcSend("capture", "done " + std::to_string(count) + " samples");
}


const uint32_t UB3_CE = 0x04000000;

// pin      | gpio     | mask 
// ---------|----------|
// D0..D7   | 13..20
// A7..A0   | 5..12
// A11      | 21
// A10      | 22
// CS       | 26       | 0x04000000
// A12      | 27
// A8..A9   | 3..4
// A13..A15 | 0..2  * 
// RW       | 28		| 0x10000000
// * courtesy 6502 pins 23..25 34

int reverse8(int bits){
	return
		((bits&0x80)>>7)|
		((bits&0x40)>>5)|
		((bits&0x20)>>3)|
		((bits&0x10)>>1)|
		((bits&0x08)<<1)|
		((bits&0x04)<<3)|
		((bits&0x02)<<5)|
		((bits&0x01)<<7);
}

char stampBuffer[20]={32};

std::string pinStamp(uint32_t tick,uint32_t pins){
	char *buffer=stampBuffer;
	if(epoch==0) epoch=tick;
	int d=(pins>>13)&0xff;
	int a07=reverse8((pins>>5)&0xff);
	int a89=(pins>>3)&0x03;
	int a10=(pins>>22)&0x01;
	int a11=(pins>>21)&0x01;
	int a12=(pins>>27)&1;
	int a1315=(pins)&7;
	int cs=(pins>>26)&1;
	int rw=(pins>>28)&1;
	int a=a07|(a89<<8)|(a10<<10)|(a11<<11)|(a12<<12)|(a1315<<13);
	hexout(buffer+0,a,4);
	buffer[4]=',';
	hexout(buffer+5,d,2);
	buffer[7]=',';
	buffer[8]=rw?'R':'W';
	buffer[9]=',';
	buffer[10]=cs?'1':'0';
	buffer[11]=',';
	hexout(buffer+12,(epoch-tick),4);
	return std::string(buffer,16);
}

int WatchdogTimeout=1200;
int ShutdownTimeout=400;

#define POWER_LED_PIN 25

void blink(){
	static int blink=0;
	blink=1-blink;
	gpio_put(POWER_LED_PIN, blink);
}

int sendCount=0;

void rpcSend(std::string name,std::string value){
	int id=2e5+(sendCount++);
	out << "{\"jsonrpc\":\"2.0\",\"result\":{"<<stringify(name)<<":"<<stringify(value)<<"},\"id\":"<<id<<"}" << std::endl;
}

#include "json.h"

void rpcError(int code, std::string message, std::string data, int id=999){
	std::stringstream err;
	err << "{\"code\":" << code << ",\"message\":\"" << message << "\",\"data\":" << data << "}";
	out << "{\"jsonrpc\":\"2.0\",\"error\":" << err.str() << ",\"id\":" << id << "}"  <<std::endl;
}

JSONParser parser;

void updateRPC(){
	std::stringstream lines;
	while(std::optional<std::string> myline=cdcReadLine()){
		std::string line=myline.value();
//			if(line=="BOOT") shutdownSystem=true;
		if(!line.empty()){
			if(line=="BOOT") shutdownSystem=true;
			if(line.front() == '{'){
				JSValue *payload;
				int status=parser.parseJSON(line,&payload);
				std::string method=payload->stringMember("method");
				int id=(int)payload->integerMember("id");
				JSObject *params=payload->objectMember("params");
				if(params){
					if(method=="rtc.set"){
						int64_t t=params->integerMember("time");
						bool success=setTime(t);
						if(success){
							out << "{\"jsonrpc\":\"2.0\",\"result\":\"setTime to " << t << "\",\"id\":"<<id<<"}" << std::endl;
						}else{
							rpcError(-32501,"setTime failure","null",id);
						}
					}
					if(method=="vidbit.set"){
						std::string title=params->stringMember("title");
						std::string about=params->stringMember("about");
						utf8 result="vidbit.set title:" + title + " about:" + about;
						out << "{\"jsonrpc\":\"2.0\",\"result\":"<<stringify(result)<<",\"id\":"<<id<<"}" << std::endl;
						snoopConnected=true;
						snoopReady=true;
					}
					if(method=="vidbit.keys"){
						std::string s=params->stringMember("text");
						out << "{\"jsonrpc\":\"2.0\",\"result\":" << stringify(s) << ",\"id\":"<<id<<"}" << std::endl;
					}
				}
			}else{
				out<<"error:"<<line<<std::endl;
			}
		}
	}

}

bool readBootSelect();

void log(const char *ascii);

#define CDC_COMMANDS

bool shutdownSystem=false;

const char *nvm_header = "PICOTOOL";	// must be 8 chars

int shellCount=0;

uint32_t shellPins=0;

void emitBuffer(){
}

void sampleRom(int enable){
	watchdog_enable(WatchdogTimeout,false);
	// active /CE means we write data on bus
	pinBuffer.clear();
	pinBuffer.reserve(2048*4*8);
	uint32_t pins=gpio_get_all();		//|0x02000000;
	while((pins&enable)==0){
		uint64_t t64=time_us_64();
		uint32_t t32=(uint32_t)t64;
		pinBuffer.push_back({t32,pins});		
		uint32_t oldPins=pins;
		if(pinBuffer.size()>2048*4*8) break;
		while(pins==oldPins){	
			sleep_us(100);
			pins=gpio_get_all()|0x02000000;
		}
	}
	watchdog_enable(WatchdogTimeout,true);
	size_t n=pinBuffer.size();
	for(size_t i=0;i<n;i++){
		const GPIOSample &s=pinBuffer[i];
		rpcSend("A D CS RW T",pinStamp(s.timestamp,s.gpio));
		if((i&7)==7){
			watchdog_update();
			tud_task();
			cdcFlush();
			sleep_ms(10);
		}
	}
}

bool shellActive=false;	//active low 

int runShell(){
	watchdog_enable(WatchdogTimeout,true);
	while(!shutdownSystem) {
		int status=cdcStatus();
		if(status&3){
			rpcSend("status&3 panic",std::to_string(status));	// booting with panic on 6
		}
		int count=shellCount++;
		uint32_t pins=gpio_get_all();//|0x02000000;
		uint32_t mask=0x02000007;		// 6502 lines float A29..A31 float
		if((pins|mask)!=(shellPins|mask)){
			shellPins=pins;

			if(snoopConnected && snoopReady){
				snoopReady=false;
				snoopRom(0,false);
			}
			uint64_t t64=time_us_64();
			uint32_t t32=(uint32_t)t64;
			rpcSend("pin",pinStamp(t32,pins));
			cdcFlush();
			sleep_ms(100);
		}else{
			sleep_us(500);
		}
		blink();
		bool bootSel=readBootSelect();
		if(!bootSel && !shutdownSystem){
			watchdog_update();
		}
		tud_task();
		updateRPC();
		cdcFlush();
	}
	return 0;
}

struct nonVolatile{
	uint8_t header[8];
	uint8_t sn[8];
	uint8_t flags[8];
	int pagecount;
};

nonVolatile *_nvm = (nonVolatile *)PICO_NVM;

void startup(){
	if(memcmp(_nvm,nvm_header,sizeof(nonVolatile))!=0){
		nonVolatile blank={0};
		memcpy(blank.header,nvm_header,8);
		// all zero nvm_mem
		log("factory defaults");
//		flash_erase(0);
//		log("installed");
//		flash_memory(0,(const uint8_t*)&blank,1);
	}else{
		log("startup");
	}
}

uint32_t currentPins=0;
uint32_t currentCount=0;
uint32_t blinkCount=0;

bool rtcGood;
int mode=0;

// picomain.cpp
// pico vidbit tool by simon
// sampler is off HAS_DISPLAY is what

const int dpins=0x1fe0;
const int apins=0x0018;

uint32_t hitCount=0;

typedef std::string utf8;

std::vector<utf8> locallog;
std::vector<utf8> commandQueue;

void log(const char *ascii){
	uint64_t t64=time_us_64();
	uint16_t t16=(uint16_t)t64;
#ifdef printf_log	
	printf("%u %s\r\n",t16,ascii);
#else
	static char buffer[1024];
	snprintf(buffer,1024,"%u %s",t16,ascii);
	utf8 line {buffer};
	locallog.push_back(line);
#endif
}

bool setTime(int64_t seconds);
std::string wallTime();

uint64_t microCount();
uint32_t cycleFrequency();

int initRomulus(){
	initSnoop();
	gpio_init(POWER_LED_PIN);
	gpio_set_dir(POWER_LED_PIN,GPIO_OUT);
	sleep_ms(40);
	return 0;
}

// PICO entry point for ondevice sampling

int main(void){
	stdio_init_all();
	cdcInit();
	int res=initRomulus();
	int result=runShell();
//	int result=runRomulus();
	rom_reset_usb_boot(0, 0);
	return result;
}
