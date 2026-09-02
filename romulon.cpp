// romulon.cpp

#include <bitset>
#include <ostream>
#include <iostream>
#include <iomanip>
#include <string>
#include <cstdint>
#include <sys/types.h>
#include <vector>
#include <optional>
#include "picosdk/picosdk.h"
//#include <pico/bootrom.h>
//#include "tusb.h"
//#include "device/usbd.h"
//#include "class/cdc/cdc_device.h"

std::string picoTitle = "romulon 0.5";

const uint32_t UB3_CE = 0x00400000;

// pin      | gpio     | mask 
// D0..D7   | 13..20
// A7..A0   | 5..12
// A8..A9   | 3..4
// A12      | 23
// CE       | 22       | 0x00400000
// A11..A10 | 11..10

char hexBuffer[8]={32};

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

std::string pinStamp(uint32_t t,uint32_t pins){
	int d=(pins>>13)&0xff;
	int a07=reverse8((pins>>5)&0xff);
	int a89=(pins>>3)&0x03;
	int a1011=(pins>>10)&0x03;
	int a12=(pins>>23)&1;
	int a=a07|(a89<<8)|(a1011<<10)|(a12<<12);
	char buffer[13]={32};
	hexout(buffer+0,a,4);
	buffer[5]=32;
	hexout(buffer+6,d,2);
	buffer[8]=32;
	hexout(buffer+9,t,4);
	return std::string(buffer,13);
}

int WatchdogTimeout=1200;
int ShutdownTimeout=400;

#define POWER_LED_PIN 25
#define UART_TX_PIN 0
#define UART_RX_PIN 1

void blink(){
	static int blink=0;
	blink=1-blink;
	gpio_put(POWER_LED_PIN, blink);
}

int sendCount=0;

void rpcSend(std::string name,std::string value){
	int id=2e5+(sendCount++);
	out << "{\"jsonrpc\":\"2.0\",\"result\":\""<<name<<"\"=\""<<value<<"\",\"id\":"<<id<<"}" << std::endl;
}


#include "json.h"

void rpcError(int code, std::string message, std::string data, int id=999){
	std::stringstream err;
	err << "{\"code\":" << code 
		<< ",\"message\":\"" << message 
		<< "\",\"data\":" << data << "}";
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
						bool success=false;//setTime(t);
						if(success){
							out << "{\"jsonrpc\":\"2.0\",\"result\":\"setTime to " << t << "\",\"id\":"<<id<<"}" << std::endl;
						}else{
							rpcError(-32501,"setTime failure","null",id);
						}
					}
					if(method=="vidbit.set"){
						std::string title=params->stringMember("title");
						std::string about=params->stringMember("about");
						out << "{\"jsonrpc\":\"2.0\",\"result\":\"vidbit.set title " << title << " about " << about << "\",\"id\":"<<id<<"}" << std::endl;
					}
					if(method=="vidbit.keys"){
						std::string s=params->stringMember("text");
						out << "{\"jsonrpc\":\"2.0\",\"result\":\"" << s << "\",\"id\":"<<id<<"}" << std::endl;
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

struct GPIOSample {
	uint32_t timestamp;
	uint32_t gpio;
};

std::vector<GPIOSample> pinBuffer;

void emitBuffer(){
}

void sampleRom(int enable){
	// active /CE means we write data on bus
	pinBuffer.clear();
	uint32_t pins=gpio_get_all()|0x02000000;
	while((pins&enable)==0){
		uint64_t t64=time_us_64();
		uint32_t t32=(uint32_t)t64;
		pinBuffer.push_back({t32,pins});		
		uint32_t oldPins=pins;
		if(pinBuffer.size()>1024) break;
		while(pins==oldPins){	
			sleep_us(100);
			pins=gpio_get_all()|0x02000000;
		}
	}
}

bool shellActive=false;	//active low 

int runShell(){
	watchdog_enable(WatchdogTimeout,false);
	while(!shutdownSystem) {
		int count=shellCount++;
		uint32_t pins=gpio_get_all()|0x02000000;
		if(pins!=shellPins){
			shellPins=pins;
/*
			if((pins&UB3_CE)==0){
				if(shellActive){
					sampleRom(UB3_CE);
					shellActive=false;
				}
			}else{
				shellActive=true;
			}
*/				
			sampleRom(0);
			size_t n=pinBuffer.size();
			for(size_t i=0;i<n;i++){
				GPIOSample s=pinBuffer[i];
				rpcSend("stamp",pinStamp(s.timestamp,s.gpio));
				cdcFlush();
			}
			uint64_t t64=time_us_64();
			uint32_t t32=(uint32_t)t64;
			rpcSend("pin",pinStamp(t32,pins));
			sleep_us(100);
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

uint32_t buffer[20000];

int runSample(){
    while(hitCount<1e4){
		uint32_t pins=gpio_get_all();
		currentCount++;
//		pins&=0x1fe0;	// d0..d7
//		pins&=0x1fe0;	// d0..d7

		uint64_t t64=time_us_64();
		uint16_t t32=(uint32_t)t64;

		if(pins!=currentPins){
			currentPins=pins;
			buffer[hitCount*2+0]=pins;
			buffer[hitCount*2+1]=t32;
			hitCount++;
		}
		if((currentCount&0xfff)==0){
			if(blinkCount!=hitCount){
				blinkCount=hitCount;
				blink();
			}
		}
		sleep_us(100);
	}
    return 1e4;   
}


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

#include <hardware/pio.h>
#include <hardware/rtc.h>
#include <sstream>

int initRomulus(){
	uint64_t t1=time_us_64();
	uint64_t t2=time_us_64();
	sleep_ms(400);
	for(uint pin=0;pin<23;pin++){
		gpio_init(pin);
		gpio_set_dir(pin,GPIO_IN);
		gpio_set_pulls(pin,false,false);
//		gpio_put(pin, 0);
	}
	for(uint pin=26;pin<28;pin++){
		gpio_init(pin);
		gpio_set_dir(pin,GPIO_IN);
		gpio_set_pulls(pin,false,false);
//		gpio_put(pin, 0);
	}
	gpio_init(POWER_LED_PIN);
	gpio_set_dir(POWER_LED_PIN,GPIO_OUT);
	return 0;
}

int runRomulus(){
//	startup();
	log(picoTitle.c_str());
//	testComplexInt();
//	testFFTInt(10);
//	watchdog_enable(WatchdogTimeout,false);
	int count=runSample();
	for(int i=0;i<count;i++){
		uint32_t bits=buffer[i*2];
		uint32_t t=buffer[i*2+1];
		out << std::bitset<32>(bits) << " " << std::setfill('0') << std::setw(5) << t <<std::endl;
		cdcFlush();
	}
	return 0;
}

/*
		bool cpress=c.set(bootSel);
		if(cpress){
			mode=(mode+1)&3;
			std::string setMode = "{\"jsonrpc\":\"2.0\",\"method\":\"romulus.set\",\"params\":{\"mode\":" + std::to_string(mode) + "},\"id\":2}";
			std::cout<<setMode<<std::endl;
		}

*/

int main(void){
	stdio_init_all();
	cdcInit();
	int res=initRomulus();
	int result=runShell();
//	int result=runRomulus();
	rom_reset_usb_boot(0, 0);
	return result;
}
