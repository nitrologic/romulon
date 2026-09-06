#include "win32coms.h"
#include <iostream>
#include <sstream>
#include <ctime>

USBMonitor* USBMonitor::m_instance = nullptr;

USBMonitor::USBMonitor() : m_hwnd(nullptr), m_deviceNotify(nullptr)
{
	m_instance = this;
}

USBMonitor::~USBMonitor()
{
	shutdown();
}

LRESULT CALLBACK USBMonitor::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_CREATE)
	{
			CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
			USBMonitor* pThis = reinterpret_cast<USBMonitor*>(pCreate->lpCreateParams);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
	}

	USBMonitor* pThis = reinterpret_cast<USBMonitor*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (pThis)
	{
			return pThis->deviceChangeProc(hwnd, msg, wParam, lParam);
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK USBMonitor::deviceChangeProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_DEVICECHANGE)
	{
			PDEV_BROADCAST_HDR pHdr = reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);

			if (pHdr && pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
			{
					PDEV_BROADCAST_DEVICEINTERFACE pDev = reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(pHdr);

					switch (wParam)
					{
							case DBT_DEVICEARRIVAL:
									m_instance->onDeviceArrival(pDev);
									break;
							case DBT_DEVICEREMOVECOMPLETE:
									m_instance->onDeviceRemoval(pDev);
									break;
					}
			}
			return TRUE;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND USBMonitor::createHiddenWindow()
{
	const wchar_t className[] = L"USBMonitorHiddenWindow";

	WNDCLASS wc = {};
	wc.lpfnWndProc = windowProc;
	wc.lpszClassName = className;
	wc.hInstance = GetModuleHandle(nullptr);

	RegisterClass(&wc);

	HWND hwnd = CreateWindow(
			className,
			L"",
			0,
			0, 0, 0, 0,
			HWND_MESSAGE,
			nullptr,
			GetModuleHandle(nullptr),
			this
	);

	return hwnd;
}

void USBMonitor::registerDeviceNotification()
{
	DEV_BROADCAST_DEVICEINTERFACE notificationFilter = {};
	notificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	notificationFilter.dbcc_classguid = GUID_DEVINTERFACE_COMPORT;

	m_deviceNotify = RegisterDeviceNotification(
			m_hwnd,
			&notificationFilter,
			DEVICE_NOTIFY_WINDOW_HANDLE
	);

	if (!m_deviceNotify)
	{
			std::cerr << "Failed to register device notification" << std::endl;
	}
}

std::string USBMonitor::getDeviceName(HDEVINFO deviceInfo, PSP_DEVINFO_DATA deviceData)
{
	wchar_t buffer[256] = {};

	if (SetupDiGetDeviceRegistryProperty(
			deviceInfo,
			deviceData,
			SPDRP_FRIENDLYNAME,
			nullptr,
			reinterpret_cast<PBYTE>(buffer),
			sizeof(buffer),
			nullptr
	))
	{
			std::wstring wstr(buffer);
			return std::string(wstr.begin(), wstr.end());
	}

	return "Unknown Device";
}

void USBMonitor::enumerateSerialDevices()
{
	HDEVINFO deviceInfo = SetupDiGetClassDevs(
			&GUID_DEVINTERFACE_COMPORT,
			nullptr,
			nullptr,
			DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
	);

	if (deviceInfo == INVALID_HANDLE_VALUE)
			return;

	SP_DEVINFO_DATA deviceData = {};
	deviceData.cbSize = sizeof(SP_DEVINFO_DATA);

	for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfo, i, &deviceData); ++i)
	{
			std::string deviceName = getDeviceName(deviceInfo, &deviceData);
			std::cout << "  - " << deviceName << std::endl;
	}

	SetupDiDestroyDeviceInfoList(deviceInfo);
}

void USBMonitor::onDeviceArrival(PDEV_BROADCAST_DEVICEINTERFACE deviceInterface)
{
	std::time_t now = std::time(nullptr);
	char timeBuffer[26];
	ctime_s(timeBuffer, sizeof(timeBuffer), &now);
	timeBuffer[strcspn(timeBuffer, "\n")] = 0;

	std::cout << "[" << timeBuffer << "] USB Serial Device CONNECTED" << std::endl;
	std::cout << "  Device Path: " << deviceInterface->dbcc_name << std::endl;

	std::cout << "Available COM ports:" << std::endl;
	enumerateSerialDevices();
}

void USBMonitor::onDeviceRemoval(PDEV_BROADCAST_DEVICEINTERFACE deviceInterface)
{
	std::time_t now = std::time(nullptr);
	char timeBuffer[26];
	ctime_s(timeBuffer, sizeof(timeBuffer), &now);
	timeBuffer[strcspn(timeBuffer, "\n")] = 0;

	std::cout << "[" << timeBuffer << "] USB Serial Device DISCONNECTED" << std::endl;
	std::cout << "  Device Path: " << deviceInterface->dbcc_name << std::endl;
}

bool USBMonitor::initialize()
{
	m_hwnd = createHiddenWindow();
	if (!m_hwnd)
	{
			std::cerr << "Failed to create hidden window" << std::endl;
			return false;
	}

	registerDeviceNotification();

	std::cout << "USB Serial Monitor initialized" << std::endl;
	std::cout << "Current connected devices:" << std::endl;
	enumerateSerialDevices();
	std::cout << "\nListening for device changes... (Press Ctrl+C to exit)" << std::endl;

	return true;
}

void USBMonitor::run()
{
	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0) > 0)
	{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
	}
}

void USBMonitor::shutdown()
{
	if (m_deviceNotify)
	{
			UnregisterDeviceNotification(m_deviceNotify);
			m_deviceNotify = nullptr;
	}

	if (m_hwnd)
	{
			DestroyWindow(m_hwnd);
			m_hwnd = nullptr;
	}
}

//#include "USBMonitor.h"
//#include <iostream>

int main()
{
	USBMonitor monitor;

	if (!monitor.initialize())
	{
		std::cerr << "Failed to initialize USB monitor" << std::endl;
		return 1;
	}

	monitor.run();
	monitor.shutdown();

	return 0;
}
