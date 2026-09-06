// windows com port junk 
// thanks to gemini for assists

#include "tools.h"

#include <iostream>

#include <windows.h>
#include <setupapi.h>
#include <devguid.h>

#include <initguid.h>
#include <usbdi.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ole32.lib")

void *NULL_HANDLE=0;

bool initComPort(HANDLE hSerial){
	DCB dcb={sizeof(dcb)};
	if (!GetCommState(hSerial, &dcb)) {
		std::cerr << "[initComPort] GetCommState failed, err=" << GetLastError() << std::endl;
		return false;
	}
	dcb.fDtrControl = DTR_CONTROL_ENABLE;
	dcb.fRtsControl = RTS_CONTROL_ENABLE;
	BOOL ok=SetCommState(hSerial,&dcb);
	if(!ok){
		std::cerr << "[initComPort] SetCommState failed, err=" << GetLastError() << std::endl;
		return false;
	}
	COMMTIMEOUTS timeouts = { 0 };
//	timeouts.ReadIntervalTimeout = MAXDWORD;
	timeouts.ReadIntervalTimeout = 5000;
	timeouts.ReadTotalTimeoutConstant = 1000;
	timeouts.WriteTotalTimeoutConstant = 1000;
//	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 1000;
//	timeouts.WriteTotalTimeoutMultiplier = 0;
	if (!SetCommTimeouts(hSerial, &timeouts)) {
		std::cerr << "[initComPort] SetCommTimeouts failed, err=" << GetLastError() << std::endl;
		return false;
	}
	return true;
}

void closeComPort(HANDLE hSerial) {
	FlushFileBuffers(hSerial);
	CloseHandle(hSerial);
}

// TODO: accumulate bytesWritten until all sent

bool writeComPort(HANDLE hSerial,const byteData &byteData){
	int index=0;
	while(index<byteData.size()){
		DWORD bytesWritten = 0;
		DWORD count=byteData.size()-index;
		BOOL success = WriteFile(hSerial,byteData.data()+index,count,&bytesWritten,NULL);
		if(!success){
			std::cout << "[writeComPort] failure" << std::endl;
			return false;
		}
		index+=bytesWritten;
	}
	return true;
}

HANDLE openComPort(const std::string& portName) {
	// Format as "\\.\COM11" to handle port numbers > 9 safely
	std::string formattedPath = "\\\\.\\" + portName;
	HANDLE hSerial = CreateFileA(formattedPath.c_str(),GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
	if (hSerial == INVALID_HANDLE_VALUE) {
		DWORD error = GetLastError();
		std::cerr << "Error opening port " << portName << ". Code: " << error << std::endl;
		return NULL_HANDLE;
	}	

	bool init=initComPort(hSerial);
	return hSerial;
}

std::string ConvertToString(const wchar_t* wstr) {
	if (!wstr) return "";
	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
	std::string str(sizeNeeded - 1, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], sizeNeeded, NULL, NULL);
	return str;
}

std::vector<ComPortInfo> enumerateComPorts() {
	std::vector<ComPortInfo> ports;
	HDEVINFO deviceInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
	if (deviceInfo == INVALID_HANDLE_VALUE) {
		return ports;
	}
	SP_DEVINFO_DATA devInfoData;
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfo, i, &devInfoData); i++) {
		wchar_t instanceId[MAX_PATH];       
		if (SetupDiGetDeviceInstanceIdW(deviceInfo, &devInfoData, instanceId, MAX_PATH, NULL)) {
			ComPortInfo portInfo;
			portInfo.devicePath = ConvertToString(instanceId);
			wchar_t friendlyNameBuf[MAX_PATH];
			if (SetupDiGetDeviceRegistryPropertyW(deviceInfo, &devInfoData, SPDRP_FRIENDLYNAME, NULL, (PBYTE)friendlyNameBuf, sizeof(friendlyNameBuf), NULL)) {
				portInfo.portDescription = ConvertToString(friendlyNameBuf);
			}

			HKEY hDeviceKey = SetupDiOpenDevRegKey(deviceInfo, &devInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
			if (hDeviceKey != INVALID_HANDLE_VALUE) {
				wchar_t portNameBuf[MAX_PATH];
				DWORD dwSize = sizeof(portNameBuf);
				DWORD dwType = 0;			
				LSTATUS status = RegQueryValueExW(hDeviceKey, L"PortName", NULL, &dwType, (PBYTE)portNameBuf, &dwSize);
				if (status == ERROR_SUCCESS && dwType == REG_SZ) {
					portInfo.portName = ConvertToString(portNameBuf);
				}
				RegCloseKey(hDeviceKey);
			}
			ports.push_back(portInfo);
		}
	}
	SetupDiDestroyDeviceInfoList(deviceInfo);
	return ports;
}
