#pragma once

#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <dbt.h>

#include <string>
#include <vector>

//#pragma comment(lib, "setupapi.lib")
//#pragma comment(lib, "ole32.lib")

class USBMonitor
{
public:
        USBMonitor();
        ~USBMonitor();

        bool initialize();
        void run();
        void shutdown();

private:
        static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK deviceChangeProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

        HWND createHiddenWindow();
        void registerDeviceNotification();
        void enumerateSerialDevices();
        std::string getDeviceName(HDEVINFO deviceInfo, PSP_DEVINFO_DATA deviceData);
        void onDeviceArrival(PDEV_BROADCAST_DEVICEINTERFACE deviceInterface);
        void onDeviceRemoval(PDEV_BROADCAST_DEVICEINTERFACE deviceInterface);

        HWND m_hwnd;
        HDEVNOTIFY m_deviceNotify;
        static USBMonitor* m_instance;
};
