// rpccoms.cpp 

// rpccoms 0.3.1 - reset clock
// rpccoms 0.3.4 - escapeString rpc value

// JSON RPC https://www.jsonrpc.org/specification

// method params under implementation

// host
// device
// reset
// restart 
// 

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <optional>

//#include <nitrohost.h>
//#include "tools.h"

#include <vector>
#include <string>
#include <cstdint>

#include "json.h"

utf8 title="rpccoms 0.4.0";

JSONParser jsonParser;

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


int rpcCount=0;
std::string rpcMethod(std::string methodName,std::optional<std::string> params=std::nullopt){
	int id=++rpcCount;
	std::stringstream ss;
	if(params){
		ss << "{\"jsonrpc\":\"2.0\",\"method\":\"" << methodName << "\",\"id\":" << id << ",\"params\":{" << params.value() << "}}";
	}else{
		ss << "{\"jsonrpc\":\"2.0\",\"method\":\"" << methodName << "\",\"id\":" << id << "}";
	}
	return ss.str();
}

const std::string getInfo=rpcMethod("vidbit.info");

bool writeComLine(comHandle handle,const std::string &text);
void pollKeys();

#include <stdarg.h>

extern "C" void print(const char *format,...);

void print(const char *format,...){
	static char buffer[4096];
	va_list args;
	va_start(args,format);
	vsnprintf(buffer,4096,format,args);
	std::cout << buffer << std::endl;
}

struct comPort{
	std::string name;
	comHandle handle;
	comPort(std::string portName,comHandle portHandle): name(std::move(portName)), handle(portHandle){
	}
	void disconnect(){
		handle=nullptr;
	}
	void connect(comHandle portHandle){
		handle=portHandle;
	}
	void print(std::string line){
		if(handle){
			writeComLine(handle,line);
		}
	}
};

//bool anyKeyDown();
//void pollMessages(HWND targetWindow);

inline byteData toBytes(const std::string& str) {
	return byteData(str.begin(), str.end());
}

bool writeComLine(comHandle handle,const std::string &text){
	const byteData data=toBytes(text+"\r\n");
	bool success=writeComPort(handle,data);
	return success;
}

int readComPort(comHandle handle, uint8_t* buffer, size_t maxSize){
	HANDLE h = (HANDLE)handle;
	DWORD bytesRead = 0;
	if (!ReadFile(h, buffer, (DWORD)maxSize, &bytesRead, NULL)) return -1;
//	if(bytesRead) std::cout << "[RPC] readfile bytesRead:" << bytesRead << std::endl;
	return (int)bytesRead;
}

#include <mutex>
#include <optional>

class RpcFifo {
private:
	std::string buffer;
	std::mutex mutex;
public:
	void onReceive(uint8_t *bytes, int count) {
		std::lock_guard<std::mutex> lock(mutex);
		std::string payload=std::string(reinterpret_cast<char*>(bytes), count);
		buffer.append(payload);
//		std::cout << "[RPC] onReceive count:" << count << std::endl;		
	}

	std::optional<std::string> readLine() {
		std::lock_guard<std::mutex> lock(mutex);
		size_t pos = buffer.find("\n");
		if (pos != std::string::npos) {
			std::string line = buffer.substr(0, pos);
			buffer.erase(0, pos + 1);
			return line;
		}
		return std::nullopt;
	}

	void clear() {
		std::lock_guard<std::mutex> lock(mutex);
		buffer.clear();
	}
};

RpcFifo rpcFifo;

void rpcReceive(uint8_t *bytes,int count){
	rpcFifo.onReceive(bytes,count);
}

void rpcThread(comPort &port){
	const size_t bufSize = 4096;
	auto buffer = std::make_unique<uint8_t[]>(bufSize);
	std::cout << "[thread] starting reader for " << port.name << std::endl;
	while (true){
		auto bptr=buffer.get();
		int n = readComPort(port.handle, bptr, bufSize - 1);
		if (n > 0){
			rpcReceive(bptr,n);
//			std::cout << "[RPC] rpcReceive n:" << n << std::endl;
		}
		else if (n == -1)
		{
			DWORD err = GetLastError();
			if(err=995) {
				// use ansi in place spinner
				std::cout << ".";
			}else{
				std::cout << port.name << " disconnected with error:" << err << std::endl;
//			exit(0);
				break;
			}
		}
		else
		{
//			std::cout << portName << " read error." << std::endl;
//			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}


void rpcThread2(comHandle h, const std::string portName){
	const size_t bufSize = 4096;
	auto buffer = std::make_unique<uint8_t[]>(bufSize);
	std::cout << "[thread] starting reader for " << portName << std::endl;
	while (true){
		auto bptr=buffer.get();
		int n = readComPort(h, bptr, bufSize - 1);
		if (n > 0){
			rpcReceive(bptr,n);
//			std::cout << "[RPC] rpcReceive n:" << n << std::endl;
		}
		else if (n == -1)
		{
			std::cout << portName << " disconnected." << std::endl;
//			exit(0);
			break;
		}
		else
		{
//			std::cout << portName << " read error." << std::endl;
//			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

std::vector<comPort> comHandles={};

int scanPorts(){	
	auto ports = enumerateComPorts();
	for (const auto& portinfo : ports) {
		const bool isPico=(portinfo.devicePath.rfind("USB\\VID_2E8A",0)==0);
		if(isPico){
			bool anon=true;
			std::string name=portinfo.portName;
			for(comPort &port:comHandles){
				if(port.name==name){
					anon=false;
					if(port.handle==nullptr){
						comHandle handle=openComPort(name);
						if(handle){
							port.handle=handle;
							std::cout << "[RPC] reconnected port:" << name << std::endl;
						}else{
							std::cout << "[RPC] scanPorts reconnect failed for " << name << std::endl;
						}
					}
					break;
				}
			}
			if(anon){
				comHandle handle=openComPort(name);
				if(handle){
					comHandles.emplace_back(name,handle);
					std::cout << "[RPC] reconnected port:" << name << std::endl;
				}else{
					std::cout << "[RPC] scanPorts connection fail name:" << name << std::endl;
				}
			}
		}
	}
	return 0;
}

void echoComPort(comHandle handle, const char *portName){
	std::string name=portName;
	comPort &port=comHandles.emplace_back(name,handle);
	std::thread reader(rpcThread, std::ref(port));//handle, portName);
	reader.detach();
//	printComPort(handle,"<ping>");
}

int enumeratePorts(){	
	auto ports = enumerateComPorts();
	for (const auto& port : ports) {
		const bool isPico=(port.devicePath.rfind("USB\\VID_2E8A",0)==0);
		if(isPico){
			comHandle h=openComPort(port.portName);
			if(h){
				std::cout << "[RPC] echoing " << port.portName << " handle:" << h << " path:" << port.devicePath << std::endl;
				echoComPort(h,port.portName.c_str());
			}else{
				std::cout << "[RPC] openComPort failure for " << port.portName << std::endl;
			}
		}else{
			std::cout << "[RPC] ignoring {";
			std::cout << "port:" << port.portName;
			std::cout << ",path:" << port.devicePath;
			std::cout << ",info:" << port.portDescription;
			std::cout << "}" << std::endl;
		}
	}
	return 0;
}
void printPorts(std::string line){
	for (auto& port : comHandles) {
		port.print(line);
	}
}

std::string escapeString(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 8);   // small extra capacity
    for (char c : value) {
        switch (c) {
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            default:   result += c;
        }
    }
    return result;
}
void onLine(utf8 json){
	JSValue *result;
//	int err=jsonParser.parseJSON(line,&result);
	size_t i=json.find("{\"sample\":\"");
	if(i!=std::string::npos){
		size_t j=json.find("\"},",i);
		if(j!=std::string::npos){
			i+=11;
			utf8 sample=json.substr(i,j-i);
			std::cout << "[RAW] line:" << sample << std::endl;	
		}else{
			std::cout << "[RAW] fail on close brace find:" << json.substr(i) << std::endl;	
		}
	}else{
		utf8 line=escapeString(json);
		std::cout << "[RX] line:" << line << std::endl;	
	}
}

int main() {
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

	HWND consoleWindow=GetConsoleWindow();
	std::cout << title << " looking for \"USB\\VID_2E8A\""<<std::endl;
	std::cout << " HWND:"<<((int64_t)consoleWindow)<<std::endl;
	enumeratePorts();

	std::this_thread::sleep_for(std::chrono::seconds(1));

	TIME_ZONE_INFORMATION timezoneInformation;
	DWORD result = GetTimeZoneInformation(&timezoneInformation);
	LONG offsetMinutes = -timezoneInformation.Bias;
	long timezoneDelta = offsetMinutes * 60;
//	std::cout << "timezoneDelta:" << timezoneDelta << std::endl;

// setRTC jsonrp method
	auto now = std::chrono::system_clock::now();
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
	std::string rtc = std::to_string(seconds+timezoneDelta);

	std::string setRTC=rpcMethod("rtc.set","\"time\":" + rtc);
	printPorts(setRTC);
	std::cout << "[TX] setRTC:" << setRTC << std::endl;

	printPorts(getInfo);
	std::cout << "[TX] info:" << getInfo << std::endl;

	std::string about="VIDBIT RPC 0.3.4";
	std::string setAbout = rpcMethod("vidbit.set","\"about\":\"" + about + "\"}");
//	std::cout << "[RPC] setAbout:" << setAbout << std::endl;
	printPorts(setAbout);

	std::this_thread::sleep_for(std::chrono::seconds(1));

	bool inReset=false;
	std::optional<std::string> lineValue;
	while(true){
		lineValue=rpcFifo.readLine();
		if(lineValue.has_value()){
//			std::cout << "[RX] line:" << escapeString(lineValue.value()) << std::endl;
			onLine(lineValue.value());
		}else{
			std::this_thread::sleep_for(std::chrono::milliseconds(5));			
//			pollMessages(consoleWindow);
			pollKeys();
		}
		bool escape=GetAsyncKeyState(VK_ESCAPE)<0;
		if(escape) break;
		bool reset=GetAsyncKeyState(VK_F10)<0;
		if(reset){
			if(!inReset){
				std::cout << "[RPC] reset" << std::endl;
				inReset=true;
				scanPorts();
				printPorts(getInfo);
			}
		}else{
			inReset=false;
		}
	}

	std::cout << "[RPC] done" << std::endl;

	return 0;
}


std::string toHex(int value) {
	std::stringstream ss;
	ss << std::setw(2) << std::setfill('0') << std::hex << (value&0xff);
	return ss.str();
}

#include <conio.h> 

void pollKeys(){
	std::string text;
	while(_kbhit()) {
		char key = _getch();
		switch(key){
			case 9:
				text+="\\t";
				break;
			case 10:
				text+="\\n";
				break;
			case 13:	// TODO: reconsider \\r\\n
				text+="\\n";
				break;
			case 27:
				text+="\\x1B";
				break;
			default:
				if(key<32||key>128){
					text+=toHex(key);
				}else{
					text+=key;
				}
		}
	}	
	if(!text.empty()){
		std::string keys = rpcMethod("vidbit.keys","\"text\":\"" + text + "\"}");
		std::cout << "[RPC] keys:" << keys << std::endl;
		printPorts(keys);
	}
}

// refuses to work as expected

void pollMessages(HWND hwnd){
	MSG msg;
	while (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
		std::cout << "[RPC] PeekMessage" << std::endl;
		TranslateMessage(&msg);
		DispatchMessage(&msg); 
	}
}

BYTE keys[256];
bool anyKeyDown(){
	if(!GetKeyboardState(keys)) return false;
	for(int i = 0; i < 256; i++){
		if(keys[i] & 0x80) return true;
	}
	return false;
}
