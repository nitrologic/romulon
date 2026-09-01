// RP2040 pico configuration

#pragma once

#include "picoincludes.h"
#include "pico2040.h"

// time setTime

#include <hardware/rtc.h>

extern"C"{
	#include "pico/util/datetime.h"
};

bool setTime(int64_t seconds);

// out cdcInit writeCDC cdcFlush cdcReadLine

#include <tusb.h>
#include <optional>
#include <sstream>

extern std::stringstream out;
void cdcInit();
void writeCDC(const char* str, size_t charCount);
void cdcFlush();
std::optional<std::string> cdcReadLine();

// readBootSelect shutdownSystem

bool readBootSelect();

extern bool shutdownSystem;

//static const int PICO_MASK=0x0fffffff;

char *hexout(char *p,int v);

void rpcSend(std::string name,std::string value);
