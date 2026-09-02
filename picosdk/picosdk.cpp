#include "picosdk.h"
#include <sstream>
#include <vector>

std::string message;

std::stringstream out;

uint64_t microCount(){
	uint64_t t64 = time_us_64();
	return t64;
}

#include <iomanip>
#ifdef RP2350
#include <pico/sync.h>
#include <pico/aon_timer.h>

void setTime(int64_t seconds){
	timespec ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = 0;
//    aon_timer_set_time(&ts);
	aon_timer_start(&ts);
}
std::string wallTime(){
	timespec ts;
	if (!aon_timer_get_time(&ts)) return "";
	tm time;
	localtime_r(&ts.tv_sec, &time);
	std::stringstream sb;        
	sb << (time.tm_year + 1900) << "-" << (time.tm_mon + 1) << "-" << time.tm_mday << ","
		<< std::setfill('0') << std::setw(2) << time.tm_hour << ":"
		<< std::setfill('0') << std::setw(2) << time.tm_min << ":"
		<< std::setfill('0') << std::setw(2) << time.tm_sec << " "
		<< microCount();
	return sb.str();
}
#else
std::string wallTime() {
    datetime_t t;
    if (!rtc_get_datetime(&t)) {
        return "";
    }

    std::stringstream sb;
    sb << t.year << "-" 
       << std::setfill('0') << std::setw(2) << (int)(t.month) << "-" 
       << std::setfill('0') << std::setw(2) << (int)(t.day) << "," 
       << std::setfill('0') << std::setw(2) << (int)(t.hour) << ":" 
       << std::setfill('0') << std::setw(2) << (int)(t.min) << ":" 
       << std::setfill('0') << std::setw(2) << (int)(t.sec) << " " 
       << microCount();
       
    return sb.str();
}
bool setTime(int64_t seconds){
	time_t epoch_time = (time_t)seconds;
	struct tm *time_info = gmtime(&epoch_time);
	datetime_t ts;
	ts.year  = time_info->tm_year+1900;
	ts.month = time_info->tm_mon+1;
	ts.day   = time_info->tm_mday;
	// simon was here
	ts.dotw  = (time_info->tm_wday == 0) ? 6 : (time_info->tm_wday - 1);	
//	ts.dotw  = time_info->tm_wday;
	ts.hour  = time_info->tm_hour;
	ts.min   = time_info->tm_min;
	ts.sec   = time_info->tm_sec;

 datetime_t t = {
            .year  = 2026,
            .month = 06,
            .day   = 05,
            .dotw  = 5, // 0 is Sunday, so 5 is Friday
            .hour  = 15,
            .min   = 45,
            .sec   = 00
    };

	bool success = rtc_set_datetime(&ts);
	return success;
}
#endif

bool __no_inline_not_in_flash_func(ReadBootSel)() {
	const uint CS_PIN_INDEX = 1;
	uint32_t flags = save_and_disable_interrupts();
// Set chip select to Hi-Z
	hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl, GPIO_OVERRIDE_LOW<<IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB, IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
	for (volatile int i = 0; i < 1000; ++i);
// The HI GPIO registers in SIO can observe and control the 6 QSPI pins.
	bool isDown=!(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));
	hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl, GPIO_OVERRIDE_NORMAL<<IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB, IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
	restore_interrupts(flags);
	return isDown;
}

bool readBootSelect(){
	return ReadBootSel();
}

std::optional<std::string> cdcReadLine();

#include <tusb.h>
#include <device/usbd.h>
#include <class/cdc/cdc_device.h>

char cdcBuffer[4096];
int cdcCharCount=0;

void writeCDC(const char* str, size_t charCount) {
	if (tud_cdc_connected()) {
		size_t writePos=0;
		while(writePos<charCount){
			uint32_t writeCount = tud_cdc_write(str+writePos, charCount-writePos);
			writePos+=writeCount;
		}
		tud_cdc_write_flush();
	}
}

void cdcFlush(){
	if(!out.str().empty()){
		message=out.str();
		writeCDC(message.c_str(),message.length());
		out.str("");
		out.clear();
	}
}

static spin_lock_t *line_lock;
std::vector<std::string> lines;

std::optional<std::string> cdcReadLine() {
	std::optional<std::string> result = std::nullopt;
	uint32_t irq_status = spin_lock_blocking(line_lock);
	if (!lines.empty()) {
		result = std::move(lines.front());
		lines.erase(lines.begin());
	}
	spin_unlock(line_lock, irq_status);
	return result;
}

void cdcReceiveCallback(void *user){
	int c = getchar_timeout_us(0);
	int count=4096;
	while (count-- && c != PICO_ERROR_TIMEOUT) {
		if(c==10){
            uint32_t irq_status = spin_lock_blocking(line_lock);
            lines.push_back(std::string(cdcBuffer, cdcCharCount));
            spin_unlock(line_lock, irq_status);
			cdcCharCount=0;
		}else{
			cdcBuffer[cdcCharCount++]=(char)c;
			if(cdcCharCount==4096) break; //signal comms error
		}
		c = getchar_timeout_us(0);
    }
}

void cdcInit(){
	line_lock = spin_lock_init(spin_lock_claim_unused(true));
	stdio_set_chars_available_callback(cdcReceiveCallback, NULL);
}


// vm src system.cpp

const char *dex="0123456789abcdef";
const int divs[]={1000000000,100000000,10000000,1000000,100000,10000,1000,100,10,1};

char *hexout(char *p,int v,int digits){
	int shift=(digits-1)*4;
	for (int i=0;i<digits;i++) {
		*p++=dex[(v>>shift)&15];
		v=v<<4;
	}
	return p;
}

char *hexout2(char *p,int v){
	int o=0;
	for (int i=0;i<8;i++)
	{
		if (v&0xf0000000) o=1;
		if (i>5) o=1;				//==7
		if (o) *p++=dex[(v>>28)&15];
		v=v<<4;
	}
	return p;
}

char *decout(char *p,int v){
	unsigned int u,t;
	int		i,c,z;

	if (v==0) {*p++='0';return p;}
	if (v<0) {*p++='-';v=-v;}
	z=0;u=(unsigned int)v;
	for (i=0;i<10;i++) {t=divs[i];for (c=0;u>=t;c++) u-=t;z|=c;if (z) *p++=48+c;}
	return p;
}
