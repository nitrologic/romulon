// tools.h - comport abstraction layer

#pragma once

#include <vector>
#include <string>
#include <cstdint>

struct ComPortInfo {
	std::string portName;
	std::string devicePath;
	std::string portDescription;
};

using byteData=std::vector<uint8_t> ;
using comHandle=void *;

std::vector<ComPortInfo> enumerateComPorts();
comHandle openComPort(const std::string& portName);
bool writeComPort(comHandle handle, const byteData &payload);
void closeComPort(comHandle handle);
