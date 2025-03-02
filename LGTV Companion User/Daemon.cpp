#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0603
#define _WIN32_WINNT 0x0603
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#include "daemon.h"
#include "../Common/common_app_define.h"
#include "../Common/ipc.h"
#include "../Common/preferences.h"
#include "../Common/tools.h"
#include <stdlib.h>
#include <fstream>
#include <thread>
#include <shellapi.h>
#include <urlmon.h>
#include <wtsapi32.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <hidusage.h>
#include <initguid.h>
#include <usbiodef.h>
#include <Dbt.h>
#include <Hidsdi.h>
#include <hidpi.h>
#include <unordered_map>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <wintoastlib.h>
#include "resource.h"

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "hid.lib")

#define			APPNAME_SHORT							L"LGTVdaemon"
#define			APPNAME_FULL							L"LGTV Companion Daemon"
#define			NOTIFY_NEW_PROCESS						1
#define         TIMER_MAIN								18
#define         TIMER_IDLE								19
#define         TIMER_TOPOLOGY							20
#define         TIMER_CHECK_PROCESSES					21
#define         TIMER_TOPOLOGY_COLLECTION				22
#define         TIMER_MAIN_DELAY_WHEN_BUSY				1000
#define         TIMER_MAIN_DELAY_WHEN_IDLE				50
#define         TIMER_REMOTE_DELAY						10000
#define         TIMER_TOPOLOGY_DELAY					8000
#define         TIMER_CHECK_PROCESSES_DELAY				5000
#define         TIMER_TOPOLOGY_COLLECTION_DELAY			3000
#define         APP_NEW_VERSION							WM_USER+9
#define         USER_DISPLAYCHANGE						WM_USER+10
#define         APP_USER_IDLE_ON						WM_USER+20
#define         APP_USER_IDLE_OFF						WM_USER+21
#define         TOPOLOGY_OK								1
#define         TOPOLOGY_ERROR							2
#define         TOPOLOGY_UNDETERMINED					3
#define         TOPOLOGY_OK_DISABLE						4
#define			HSHELL_UNDOCUMENTED_FULLSCREEN_EXIT		0x36
#define			REMOTE_CONNECT							1
#define			REMOTE_DISCONNECT						0
#define			REMOTE_STEAM_CONNECTED					0x0001
#define			REMOTE_STEAM_NOT_CONNECTED				0x0002
#define			REMOTE_NVIDIA_CONNECTED					0x0004
#define			REMOTE_NVIDIA_NOT_CONNECTED				0x0008
#define			REMOTE_RDP_CONNECTED					0x0010
#define			REMOTE_RDP_NOT_CONNECTED				0x0020
#define			REMOTE_SUNSHINE_CONNECTED				0x0040
#define			REMOTE_SUNSHINE_NOT_CONNECTED			0x0080
#define			SUNSHINE_REG							"SOFTWARE\\LizardByte\\Sunshine"
#define			SUNSHINE_FILE_CONF						"sunshine.conf"
#define			SUNSHINE_FILE_LOG						"sunshine.log"
#define			SUNSHINE_FILE_SVC						L"sunshine.exe"

struct DisplayInfo {					// Display info
	DISPLAYCONFIG_TARGET_DEVICE_NAME	target;
	HMONITOR							hMonitor;
	HDC									hdcMonitor;
	RECT								rcMonitor2;
	MONITORINFOEX						monitorinfo;
};
struct RemoteWrapper {					// Remote streaming info
	bool								bRemoteCurrentStatusNvidia = false;
	bool								bRemoteCurrentStatusSteam = false;
	bool								bRemoteCurrentStatusRDP = false;
	bool								bRemoteCurrentStatusSunshine = false;
	std::wstring						sCurrentlyRunningWhitelistedProcess = L"";
	std::wstring						sCurrentlyRunningFsExcludedProcess = L"";
	std::string							Sunshine_Log_File = "";
	uintmax_t							Sunshine_Log_Size = 0;

	const std::vector<std::wstring>		stream_proc_list{
	L"steam_monitor.exe"				//steam server
	};
	const std::vector<std::wstring>		stream_usb_list_gamestream{
	L"usb#vid_0955&pid_b4f0"			//nvidia
	};
} Remote;

struct DeviceInfo { // Cache for raw input data stuff
	HANDLE hDevice;
	PHIDP_PREPARSED_DATA ppd;
	HIDP_CAPS caps;
	std::vector<int> axes;
};

class WinToastHandler : public WinToastLib::IWinToastHandler
{
public:
	void toastActivated() const override {
	}
	void toastActivated(int actionIndex) const override {
		ShellExecute(0, 0, NEWRELEASELINK, 0, 0, SW_SHOW);
	}
	void toastDismissed(WinToastDismissalReason state) const override {}
	void toastFailed() const override {}
private:
};

// Globals:
HINSTANCE                       hInstance;  // current instance
HWND                            hMainWnd = NULL;
bool                            bIdle = false;
bool                            bDaemonVisible = true;
bool                            bFirstRun = false;
WinToastHandler                 m_WinToastHandler;
HANDLE                          hPipe = INVALID_HANDLE_VALUE;
INT64                           idToastFirstrun = NULL;
INT64                           idToastNewversion = NULL;
UINT							shellhookMessage;
DWORD							daemon_startup_user_input_time = 0;
UINT							ManualUserIdleMode = 0;
HBRUSH                          hBackbrush;
time_t							TimeOfLastTopologyChange = 0;
DWORD							ulLastRawInput = 0;
DWORD							ulLastControllerSample = 0;
DWORD							ulLastMouseSample = 0;
DWORD							dwGetLastInputInfoSave = 0;
int								iMouseChecksPerformed = 0;
bool							ToastInitialised = false;
std::shared_ptr<IpcClient>		pPipeClient;
Preferences						Prefs(CONFIG_FILE);
std::string						sessionID;
bool							isElevated = false;
std::unordered_map<std::wstring, DeviceInfo> g_deviceCache; // Key = device path

//Application entry point
int APIENTRY wWinMain(_In_ HINSTANCE Instance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(nCmdShow);

	hInstance = Instance;
	MSG msg;
	std::wstring WindowTitle;
	WindowTitle = APPNAME_FULL;
	WindowTitle += L" v";
	WindowTitle += APP_VERSION;
	std::wstring CommandLineParameters;

	//commandline processing
	if (lpCmdLine)
	{
		std::wstring CommandLineParameters = lpCmdLine;
		transform(CommandLineParameters.begin(), CommandLineParameters.end(), CommandLineParameters.begin(), ::tolower);
		if (CommandLineParameters == L"-show")
			bDaemonVisible = true;
		else if (CommandLineParameters == L"-hide")
			bDaemonVisible = false;
		else if (CommandLineParameters == L"-firstrun")
		{
			bFirstRun = true;
			bDaemonVisible = false;
		}
		else
		{
			MessageBox(NULL, L"This application is the desktop user mode daemon for LGTV Companion, it runs in the background and provides additional features, e.g user idle detection. To manage your WebOS devices and options please instead start the 'LGTV Companion' application from your windows start menu.\n\nClick OK to close this message!", L"LGTV Companion (Daemon)", MB_OK);
			return 0;
		}
	}

	shellhookMessage = RegisterWindowMessageW(L"SHELLHOOK");
	//Initialize toast notifications
	WinToastLib::WinToast::WinToastError error;
	WinToastLib::WinToast::instance()->setAppName(L"LGTV Companion (Daemon)");
	const auto aumi = WinToastLib::WinToast::configureAUMI
	(L"LGTV Companion (Daemon)", L"", L"", L"");
	WinToastLib::WinToast::instance()->setAppUserModelId(aumi);
	if (!WinToastLib::WinToast::instance()->initialize(&error)) {
		wchar_t buf[250];
		swprintf_s(buf, L"Failed to initialize Toast Notifications :%d", error);
		log(buf);
	}
	else
		ToastInitialised = true;

	// if the app is already running as another process, tell the other process to exit
	messageExistingProcess();

	// Initiate PipeClient IPC
	pPipeClient = std::make_shared<IpcClient>(PIPENAME, ipcCallback, (LPVOID)NULL);
//	ipc::PipeClient PipeCl(PIPENAME, NamedPipeCallback);
//	pPipeClient = &PipeCl;

	// read the configuration file and init prefs
	if(!Prefs.isInitialised())
	{
		communicateWithService("errorconfig");
		return false;
	}
	hBackbrush = CreateSolidBrush(0x00ffffff);

	// Get Session ID
	PULONG pSessionId = NULL;
	DWORD BytesReturned = 0;
	if (WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionId, (LPSTR*)&pSessionId, &BytesReturned))
	{
		DWORD id = (DWORD)*pSessionId;
		WTSFreeMemory(pSessionId);
		sessionID = std::to_string(id);

	}

	// create main window (dialog)
	hMainWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)WndProc);
	SetWindowText(hMainWnd, WindowTitle.c_str());
	ShowWindow(hMainWnd, bDaemonVisible ? SW_SHOW : SW_HIDE);

	// spawn thread to check for updated version of the app.
	if (Prefs.notify_update_)
	{
		std::thread thread_obj(threadVersionCheck, hMainWnd);
		thread_obj.detach();
	}

	if (Prefs.user_idle_mode_)
	{
		RAWINPUTDEVICE Rid[3];

		Rid[0].usUsagePage = 0x01;							// HID_USAGE_PAGE_GENERIC
		Rid[0].usUsage = 0x00;
		Rid[0].dwFlags = RIDEV_PAGEONLY | RIDEV_INPUTSINK;   // add entire page              
		Rid[0].hwndTarget = hMainWnd;

		Rid[1].usUsagePage = 0x01;							// HID_USAGE_PAGE_GENERIC
		Rid[1].usUsage = 0x02;								// HID_USAGE_GENERIC_MOUSE
		Rid[1].dwFlags = bDaemonVisible ? RIDEV_INPUTSINK : RIDEV_INPUTSINK | RIDEV_NOLEGACY;
		Rid[1].hwndTarget = hMainWnd;

		Rid[2].usUsagePage = 0x01;							// HID_USAGE_PAGE_GENERIC
		Rid[2].usUsage = 0x06;								// HID_USAGE_GENERIC_KEYBOARD
		Rid[2].dwFlags = bDaemonVisible ? RIDEV_INPUTSINK : RIDEV_INPUTSINK | RIDEV_NOLEGACY;
		Rid[2].hwndTarget = hMainWnd;
		if (RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE)
		{
			log(L"Failed to register for Raw Input!");
		}
		ulLastRawInput = GetTickCount();
		SetTimer(hMainWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
		SetTimer(hMainWnd, TIMER_IDLE, Prefs.user_idle_mode_delay_ * 60 * 1000, (TIMERPROC)NULL);
	}
	if (Prefs.topology_support_)
		SetTimer(hMainWnd, TIMER_TOPOLOGY, TIMER_TOPOLOGY_DELAY, (TIMERPROC)NULL);

	if (Prefs.remote_streaming_host_support_ || Prefs.user_idle_mode_whitelist_)
		SetTimer(hMainWnd, TIMER_CHECK_PROCESSES, TIMER_CHECK_PROCESSES_DELAY, (TIMERPROC)NULL);

	isElevated = IsWindowElevated(hMainWnd);
	std::wstring startupmess = WindowTitle;
	startupmess += L" is running. Process is ";
	startupmess += isElevated ? L"elevated." : L"not elevated.";
	log(startupmess);
	communicateWithService("started");
	
	std::wstring sessionmessage = L"Session ID is: ";
	sessionmessage += tools::widen(sessionID);
	log(sessionmessage);



	HPOWERNOTIFY rsrn = RegisterSuspendResumeNotification(hMainWnd, DEVICE_NOTIFY_WINDOW_HANDLE);
	HPOWERNOTIFY rpsn = RegisterPowerSettingNotification(hMainWnd, &(GUID_CONSOLE_DISPLAY_STATE), DEVICE_NOTIFY_WINDOW_HANDLE);

	DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
	HDEVNOTIFY dev_notify = NULL;
	if (Prefs.remote_streaming_host_support_)
	{
		ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
		NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
		NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		memcpy(&(NotificationFilter.dbcc_classguid), &(GUID_DEVINTERFACE_USB_DEVICE), sizeof(struct _GUID));
		dev_notify = RegisterDeviceNotification(hMainWnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

		WTSRegisterSessionNotification(hMainWnd, NOTIFY_FOR_ALL_SESSIONS);

		Remote.Sunshine_Log_File = sunshine_GetLogFile();
		if (Remote.Sunshine_Log_File != "")
		{
			std::filesystem::path p(Remote.Sunshine_Log_File);
			Remote.Sunshine_Log_Size = std::filesystem::file_size(p);
		}
	}

	// message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage(hMainWnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	pPipeClient->terminate();

	if (ToastInitialised)
	{
		if (idToastNewversion || idToastFirstrun)
		{
			if (idToastFirstrun)
				WinToastLib::WinToast::instance()->hideToast(idToastFirstrun);
			if (idToastNewversion)
				WinToastLib::WinToast::instance()->hideToast(idToastNewversion);
			Sleep(500);
			WinToastLib::WinToast::instance()->clear();
		}
	}
	DeleteObject(hBackbrush);
	UnregisterSuspendResumeNotification(rsrn);
	UnregisterPowerSettingNotification(rpsn);
	if (dev_notify)
		UnregisterDeviceNotification(dev_notify);
	if (Prefs.remote_streaming_host_support_)
		WTSUnRegisterSessionNotification(hMainWnd);
	RawInput_ClearCache();
	return (int)msg.wParam;
}

//   Process messages for the main window.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	std::wstring str;
	switch (message)
	{
	case WM_INPUT:
	{
		if (!lParam)
			return 0;
		HRAWINPUT hRawInput = reinterpret_cast<HRAWINPUT>(lParam);
		UINT size;
		GetRawInputData(hRawInput, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
		std::vector<BYTE> buffer(size);
		if (GetRawInputData(hRawInput, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) != size) 
		{
			log(L"Failed to get raw input data.");
			return 0;
		}
		DWORD tick_now = GetTickCount();
		RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer.data());

		// Was a key pressed?
		if (raw->header.dwType == RIM_TYPEKEYBOARD)
		{
			ulLastRawInput = tick_now;
			return 0;
		}
		// Was the mouse moved?
		else if (raw->header.dwType == RIM_TYPEMOUSE)
		{
			// avoid accidental mouse-wake
			if (tick_now - ulLastMouseSample < 50)
				break;
			if (tick_now - ulLastMouseSample > 500)
				iMouseChecksPerformed = 1;
			else
				iMouseChecksPerformed++;
			if (iMouseChecksPerformed >= 3)
				ulLastRawInput = tick_now;
			ulLastMouseSample = tick_now;
			return 0;
		}
		// If not mouse or keyboard, then a controller was used
		else
		{
			UINT nameSize = 0;
			if (GetRawInputDeviceInfo(raw->header.hDevice, RIDI_DEVICENAME, nullptr, &nameSize) != 0)
				return 0;
			std::vector<WCHAR> deviceName(nameSize);
			if (GetRawInputDeviceInfo(raw->header.hDevice, RIDI_DEVICENAME, deviceName.data(), &nameSize) < 0)
			{
				log(L"Failed to get device name.");
				return 0;
			}
			// Check if exists in cache or perform initialization
			std::wstring devicePath(deviceName.data());
			auto it = g_deviceCache.find(devicePath);
			if (it == g_deviceCache.end()) {
				RawInput_AddToCache(devicePath);
				it = g_deviceCache.find(devicePath); // Re-check after insertion attempt
				if (it == g_deviceCache.end())
				{
					log(L"Error in cache management.");
					return 0;
				}
			}
			// Check whether a digital button was pressed on the controller
			DeviceInfo& cache = it->second;
			if (cache.caps.NumberInputButtonCaps > 0)
			{
				std::vector<HIDP_BUTTON_CAPS> buttonCaps(cache.caps.NumberInputButtonCaps);
				USHORT buttonCapsLength = cache.caps.NumberInputButtonCaps;
				if (HidP_GetButtonCaps(HidP_Input, buttonCaps.data(), &buttonCapsLength, cache.ppd) == HIDP_STATUS_SUCCESS)
				{
					ULONG usageLength = buttonCaps[0].Range.UsageMax - buttonCaps[0].Range.UsageMin + 1;
					std::vector<USAGE> usages(usageLength);
					if (HidP_GetUsages(HidP_Input, buttonCaps[0].UsagePage, 0, usages.data(), &usageLength, cache.ppd,
						reinterpret_cast<PCHAR>(raw->data.hid.bRawData), raw->data.hid.dwSizeHid) == HIDP_STATUS_SUCCESS)
					{
						if (usageLength > 0)
							ulLastRawInput = tick_now;
					}					
				}
			}
			// Check whether any analog sticks were moved and avoid accidental wake
			if (tick_now - ulLastControllerSample < 50)
			{
				break;
			}
			if (cache.caps.NumberInputValueCaps > 0)
			{
				std::vector<HIDP_VALUE_CAPS> valueCaps(cache.caps.NumberInputValueCaps);
				USHORT valueCapsLength = cache.caps.NumberInputValueCaps;
				if (cache.axes.size() != valueCapsLength)
				{
					cache.axes.clear();
					cache.axes.assign(valueCapsLength, -1);
				}
				if (HidP_GetValueCaps(HidP_Input, valueCaps.data(), &valueCapsLength, cache.ppd) == HIDP_STATUS_SUCCESS) 
				{
					for (USHORT i = 0; i < valueCapsLength; ++i) 
					{
						ULONG value;
						if (HidP_GetUsageValue(HidP_Input, valueCaps[i].UsagePage, 0, valueCaps[i].Range.UsageMin, &value, cache.ppd,
							reinterpret_cast<PCHAR>(raw->data.hid.bRawData), raw->data.hid.dwSizeHid) == HIDP_STATUS_SUCCESS) 
						{
							// Scale to normalized range
							LONG scaledValue = value;
							if (valueCaps[i].LogicalMin < valueCaps[i].LogicalMax) 
							{
								// Example scaling (adjust based on your needs)
								LONG min = valueCaps[i].LogicalMin;
								LONG max = valueCaps[i].LogicalMax;
								scaledValue = (value - min) * 65535 / (max - min);
							}
							if (cache.axes[i] != -1)
							{
								if (abs(cache.axes[i] - scaledValue) > 0.012 * 65535) // sensitivity of the input detection
								{
									ulLastRawInput = tick_now;
									ulLastControllerSample = tick_now;
								}
							}
							cache.axes[i] = scaledValue;
						}
					}
				}
			}
		}
		return 0;
	}break;
	case WM_INITDIALOG:
	{
		if (bFirstRun && ToastInitialised)
		{
			TCHAR buffer[MAX_PATH] = { 0 };
			GetModuleFileName(NULL, buffer, MAX_PATH);
			std::wstring imgpath = buffer;
			std::wstring::size_type pos = imgpath.find_last_of(L"\\/");
			imgpath = imgpath.substr(0, pos + 1);
			imgpath += L"mainicon.ico";

			std::wstring Toast;
			Toast = L"Good news! LGTV Companion v";
			Toast += APP_VERSION;
			Toast += L" is now installed.";

			WinToastLib::WinToastTemplate templ;
			templ = WinToastLib::WinToastTemplate(WinToastLib::WinToastTemplate::ImageAndText02);
			templ.setImagePath(imgpath);

			templ.setTextField(Toast, WinToastLib::WinToastTemplate::FirstLine);
			templ.setTextField(L"Designed to protect and extend the features of your WebOS devices when used as a PC monitor.", WinToastLib::WinToastTemplate::SecondLine);

			// Read the additional options section in the article
			templ.setDuration(WinToastLib::WinToastTemplate::Duration::Short);
			templ.setAudioOption(WinToastLib::WinToastTemplate::AudioOption::Default);
			idToastFirstrun = WinToastLib::WinToast::instance()->showToast(templ, &m_WinToastHandler);
			if (idToastFirstrun == -1L)
			{
				log(L"Failed to show first run toast notification!");
			}
		}
	}break;
	case APP_NEW_VERSION:
	{
		if (!bFirstRun)
		{
			communicateWithService("newversion");

			std::wstring s = L"A new version of this app is available for download here: ";
			s += NEWRELEASELINK;
			log(s);

			if (ToastInitialised)
			{
				TCHAR buffer[MAX_PATH] = { 0 };
				GetModuleFileName(NULL, buffer, MAX_PATH);
				std::wstring imgpath = buffer;
				std::wstring::size_type pos = imgpath.find_last_of(L"\\/");
				imgpath = imgpath.substr(0, pos + 1);
				imgpath += L"mainicon.ico";

				WinToastLib::WinToastTemplate templ;
				templ = WinToastLib::WinToastTemplate(WinToastLib::WinToastTemplate::ImageAndText02);
				templ.setImagePath(imgpath);

				templ.setTextField(L"Yay! A new version is available.", WinToastLib::WinToastTemplate::FirstLine);
				templ.setTextField(L"Please install the new version to keep up to date with bugfixes and features.", WinToastLib::WinToastTemplate::SecondLine);

				templ.addAction(L"Download");

				// Read the additional options section in the article
				templ.setDuration(WinToastLib::WinToastTemplate::Duration::Long);
				templ.setAudioOption(WinToastLib::WinToastTemplate::AudioOption::Default);
				idToastNewversion = WinToastLib::WinToast::instance()->showToast(templ, &m_WinToastHandler);
				if (idToastNewversion == -1L)
				{
					log(L"Failed to show toast notification about updated version!");
				}
			}
		}
	}break;

	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDC_BUTTON1:
			{
				bDaemonVisible = false;
				ShowWindow(hWnd, SW_HIDE);
			}break;
			default:break;
			}
		}break;
		default:break;
		}
	}break;

	case WM_NOTIFY:
	{
	}break;
	case APP_USER_IDLE_ON: //message sent from lgtv companion.exe
	{
		if (Prefs.user_idle_mode_)
		{
			ManualUserIdleMode = APP_USER_IDLE_ON;
			bIdle = true;
			SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_IDLE, (TIMERPROC)NULL);
			SetTimer(hWnd, TIMER_IDLE, Prefs.user_idle_mode_delay_ * 60 * 1000, (TIMERPROC)NULL);
			log(L"User forced user idle mode!");
			communicateWithService("useridle");
		}
		else
			log(L"Can not force user idle mode, as the feature is not enabled in the global options!");
	}break;
	case APP_USER_IDLE_OFF: //message sent from lgtv companion.exe
	{
		if (Prefs.user_idle_mode_)
		{
			ManualUserIdleMode = 0;
			bIdle = false;
			SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
			SetTimer(hWnd, TIMER_IDLE, Prefs.user_idle_mode_delay_ * 60 * 1000, (TIMERPROC)NULL);
			ulLastRawInput = GetTickCount();
			log(L"User forced unsetting user idle mode!");
			communicateWithService("userbusy");
		}
		else
			log(L"Can not force unset user idle mode, as the feature is not enabled in the global options!");
	}break;
	case WM_TIMER:
	{
		switch (wParam)
		{
		case TIMER_MAIN:
		{
			DWORD time_now = GetTickCount();
			LASTINPUTINFO lii;
			lii.cbSize = sizeof(LASTINPUTINFO);
			GetLastInputInfo(&lii);

			// do this first time the timer is triggered
			if (daemon_startup_user_input_time == 0)
				daemon_startup_user_input_time = ulLastRawInput;

			//fix for the fullscreen idle detection on system startup because windows will return QUNS_BUSY until the user has interacted with the PC
			if (ulLastRawInput != daemon_startup_user_input_time)
				daemon_startup_user_input_time = -1;

			if ((lii.dwTime - dwGetLastInputInfoSave <= TIMER_MAIN_DELAY_WHEN_BUSY * 2)) // is the last input within two secs from the previous?
				if (lii.dwTime - dwGetLastInputInfoSave > 0) // is the last input more recent than the last one?
					if (lii.dwTime > ulLastRawInput) // is the last input more recent than what Raw Input determined?
						ulLastRawInput = time_now;
			dwGetLastInputInfoSave = lii.dwTime;
					
			// update the status window with last input
			if (bDaemonVisible)
			{
				DWORD time = (time_now - ulLastRawInput) / 1000;
				std::wstring ago = tools::widen(std::to_string(time));
				SendMessage(GetDlgItem(hMainWnd, IDC_EDIT3), WM_SETTEXT, 0, (WPARAM)ago.c_str());
//				ShowWindow(GetDlgItem(hWnd, IDC_STATIC_ELEVATED), SW_HIDE);
			}
			if (bIdle)
			{
				if (time_now - ulLastRawInput <= TIMER_MAIN_DELAY_WHEN_IDLE * 2) // was there user input during the interval?
				{
					ManualUserIdleMode = 0;
					SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
					SetTimer(hWnd, TIMER_IDLE, Prefs.user_idle_mode_delay_ * 60 * 1000, (TIMERPROC)NULL);
					bIdle = false;
					communicateWithService("userbusy");
				}
			}
			else
			{
				if (time_now - ulLastRawInput <= TIMER_MAIN_DELAY_WHEN_BUSY) // was there user input during the interval?
				{
					SetTimer(hWnd, TIMER_IDLE, Prefs.user_idle_mode_delay_ * 60 * 1000, (TIMERPROC)NULL);
				}
			}
			return 0;
		}break;

		case TIMER_IDLE:
		{
			if (ManualUserIdleMode == 0)
			{
				//Is a whitelisted process running
				if (Prefs.user_idle_mode_whitelist_)
				{
					if (Remote.sCurrentlyRunningWhitelistedProcess != L"")
					{
						std::wstring mess = L"Whitelisted application is prohibiting user idle mode(";
						mess += Remote.sCurrentlyRunningWhitelistedProcess;
						mess += L")";
						log(mess);
						return 0;
					}
				}
				// fullscreen fix for first boot
				// fullsceen detection routine
				if (daemon_startup_user_input_time == -1)
				{
					// UIM is disabled during fullsceen
					if (Prefs.user_idle_mode_exclude_fullscreen_)
					{
						if (isFullscreenApplicationRunning())
						{
							log(L"Fullscreen application is currently prohibiting user idle mode");
							return 0;
						}
					}
					else // UIM is enabled during fullscreen
					{
						//Is an excluded fullscreen process running?
						if (Prefs.user_idle_mode_exclude_fullscreen_whitelist_ && Remote.sCurrentlyRunningFsExcludedProcess != L"")
						{
							if (isFullscreenApplicationRunning())
							{
								std::wstring mess = L"Fullscreen excluded process is currently prohibiting idle (";
								mess += Remote.sCurrentlyRunningFsExcludedProcess;
								mess += L")";
								log(mess);
								return 0;
							}
						}
					}
				}
			}
			bIdle = true;
			SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_IDLE, (TIMERPROC)NULL);
			communicateWithService("useridle");
			return 0;
		}break;
		case TIMER_CHECK_PROCESSES:
		{
			DWORD dw = checkRemoteStreamingProcesses();
			if (dw)
				remoteStreamingEvent(dw);
		}break;
		case REMOTE_CONNECT:
		{
			KillTimer(hWnd, (UINT_PTR)REMOTE_CONNECT);
			communicateWithService("remote_connect");
		}break;
		case REMOTE_DISCONNECT:
		{
			KillTimer(hWnd, (UINT_PTR)REMOTE_DISCONNECT);
			communicateWithService("remote_disconnect");
		}break;
		case TIMER_TOPOLOGY_COLLECTION:
		{
			KillTimer(hWnd, (UINT_PTR)TIMER_TOPOLOGY_COLLECTION);
			PostMessage(hWnd, USER_DISPLAYCHANGE, NULL, NULL);
		}break;
		case TIMER_TOPOLOGY:
		{
			KillTimer(hWnd, TIMER_TOPOLOGY);
			if (!Prefs.topology_support_)
				break;

			int result = verifyTopology();

			if (result == TOPOLOGY_OK)
			{
				PostMessage(hWnd, USER_DISPLAYCHANGE, NULL, NULL);
			}
			else if (result == TOPOLOGY_UNDETERMINED)
			{
				log(L"Warning! No active devices detected when verifying Windows Monitor Topology.");
				communicateWithService("topology undetermined");
				//				Prefs.AdhereTopology = false;
			}
			else if (result == TOPOLOGY_OK_DISABLE)
			{
				Prefs.topology_support_ = false;
			}
			else if (result == TOPOLOGY_ERROR)
			{
				Prefs.topology_support_ = false;
				communicateWithService("topology invalid");

				std::wstring s = L"A change to the system has invalidated the monitor topology configuration and the feature has been disabled. "
					"Please run the configuration guide in the global options to ensure correct operation.";
				log(s);

				if (ToastInitialised)
				{
					TCHAR buffer[MAX_PATH] = { 0 };
					GetModuleFileName(NULL, buffer, MAX_PATH);
					std::wstring imgpath = buffer;
					std::wstring::size_type pos = imgpath.find_last_of(L"\\/");
					imgpath = imgpath.substr(0, pos + 1);
					imgpath += L"mainicon.ico";

					WinToastLib::WinToastTemplate templ;
					templ = WinToastLib::WinToastTemplate(WinToastLib::WinToastTemplate::ImageAndText02);
					templ.setImagePath(imgpath);

					templ.setTextField(L"Invalidated monitor topology configuration!", WinToastLib::WinToastTemplate::FirstLine);
					templ.setTextField(L"A change to the system has invalidated the multi-monitor configuration. Please run the configuration guide in the global options.", WinToastLib::WinToastTemplate::SecondLine);

					// Read the additional options section in the article
					templ.setDuration(WinToastLib::WinToastTemplate::Duration::Long);
					templ.setAudioOption(WinToastLib::WinToastTemplate::AudioOption::Default);
					idToastNewversion = WinToastLib::WinToast::instance()->showToast(templ, &m_WinToastHandler);
					if (idToastNewversion == -1L)
					{
						log(L"Failed to show toast notification about invalidated topology configuration!");
					}
				}
			}
			return 0;
		}break;
		default:break;
		}
	}break;
	case WM_QUERYENDSESSION:
	{
		if (Prefs.shutdown_timing_ == PREFS_SHUTDOWN_TIMING_DELAYED)
			ShutdownBlockReasonCreate(hWnd, L"Delaying shutdown...");
		return false; // because it is a dialog
	}break;
	case WM_POWERBROADCAST:
	{
		switch (wParam)
		{
		case PBT_APMSUSPEND:
		{
			KillTimer(hWnd, TIMER_MAIN);
			KillTimer(hWnd, TIMER_IDLE);
			bIdle = false;
			ulLastRawInput = GetTickCount();
			log(L"Suspending system.");
			RawInput_ClearCache();
			return true;
		}break;
		case PBT_APMRESUMEAUTOMATIC:
		{
			bIdle = false;
			ulLastRawInput = GetTickCount();
			if (Prefs.user_idle_mode_)
			{
				SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
				SetTimer(hWnd, TIMER_IDLE, Prefs.user_idle_mode_delay_ * 60 * 1000, (TIMERPROC)NULL);
			}
			log(L"Resuming system.");
			RawInput_ClearCache();
			return true;
		}break;
		case PBT_POWERSETTINGCHANGE:
		{
			if (lParam)
			{
				POWERBROADCAST_SETTING* PBS = NULL;
				PBS = (POWERBROADCAST_SETTING*)lParam;
				if (PBS->PowerSetting == GUID_CONSOLE_DISPLAY_STATE)
				{
					if (PBS->Data[0] == 0)
					{
						log(L"System requests displays OFF.");
					}
					else if (PBS->Data[0] == 2)
					{
						log(L"System requests displays OFF(DIMMED).");
					}
					else
					{
						bIdle = false;
						ulLastRawInput = GetTickCount();
						if (Prefs.user_idle_mode_)
						{
							SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
							SetTimer(hWnd, TIMER_IDLE, Prefs.user_idle_mode_delay_ * 60 * 1000, (TIMERPROC)NULL);
						}
						RawInput_ClearCache();
						log(L"System requests displays ON.");
					}
					return true;
				}
			}
		}break;

		default:break;
		}
	}break;
	case USER_DISPLAYCHANGE:
	{
		TimeOfLastTopologyChange = time(0);
		checkDisplayTopology();
	}break;
	case WM_DISPLAYCHANGE:
	{
		if (Prefs.topology_support_)
		{
			time_t now = time(0);
			if (now - TimeOfLastTopologyChange > 10)
			{
				PostMessage(hWnd, USER_DISPLAYCHANGE, NULL, NULL);
			}
			else
			{
				TimeOfLastTopologyChange = now;
				SetTimer(hWnd, TIMER_TOPOLOGY_COLLECTION, TIMER_TOPOLOGY_COLLECTION_DELAY, (TIMERPROC)NULL);
			}
		}
	}break;
	case WM_DEVICECHANGE:
	{
		//clear the raw input cache whenever a controller was connected/disconnected.
		RawInput_ClearCache();

		if (Prefs.remote_streaming_host_support_ && lParam)
		{
			PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR)lParam;
			PDEV_BROADCAST_DEVICEINTERFACE lpdbv = (PDEV_BROADCAST_DEVICEINTERFACE)lpdb;
			std::wstring path;
			if (lpdb->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
			{
				path = std::wstring(lpdbv->dbcc_name);
				transform(path.begin(), path.end(), path.begin(), ::tolower);

				switch (wParam)
				{
				case DBT_DEVICEARRIVAL:
				{
					for (auto& dev : Remote.stream_usb_list_gamestream)
					{
						if (path.find(dev, 0) != std::wstring::npos)
						{
							remoteStreamingEvent(REMOTE_NVIDIA_CONNECTED);
							return true;
						}
					}
				}break;
				case DBT_DEVICEREMOVECOMPLETE:
				{
					for (auto& dev : Remote.stream_usb_list_gamestream)
					{
						if (path.find(dev, 0) != std::wstring::npos)
						{
							remoteStreamingEvent(REMOTE_NVIDIA_NOT_CONNECTED);
							return true;
						}
					}
				}break;
				default:break;
				}
			}
		}
	}break;
	case WM_WTSSESSION_CHANGE:
	{
		if (wParam == WTS_REMOTE_CONNECT)
		{
			remoteStreamingEvent(REMOTE_RDP_CONNECTED);
		}
		else if (wParam == WTS_REMOTE_DISCONNECT)
		{
			remoteStreamingEvent(REMOTE_RDP_NOT_CONNECTED);
		}
	}break;
	case WM_COPYDATA:
	{
		if (!lParam)
			return true;
		COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)lParam;
		if (pcds->dwData == NOTIFY_NEW_PROCESS)
		{
			SendMessage(hWnd, WM_CLOSE, NULL, NULL);
		}
		return true;
	}break;
	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		//		if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_ENABLE))
		{
			SetBkMode(hdcStatic, TRANSPARENT);
		}
		return(INT_PTR)hBackbrush;
	}break;
	case WM_PAINT:
	{
		RECT rc = { 0 };
		GetClientRect(hWnd, &rc);
		PAINTSTRUCT ps;
		//		PAINTSTRUCT psPaint;
		InvalidateRect(hWnd, NULL, false);

		//		GetClientRect(hWnd, &rc);
		int width = rc.right;
		int height = rc.bottom;
		HDC hdc = BeginPaint(hWnd, &ps);
		HDC backbuffDC = CreateCompatibleDC(hdc);
		HBITMAP backbuffer = CreateCompatibleBitmap(hdc, width, height);
		int savedDC = SaveDC(backbuffDC);
		SelectObject(backbuffDC, backbuffer);

		FillRect(backbuffDC, &rc, (HBRUSH)hBackbrush);

		BitBlt(hdc, 0, 0, width, height, backbuffDC, 0, 0, SRCCOPY);
		RestoreDC(backbuffDC, savedDC);

		DeleteObject(backbuffer);
		DeleteDC(backbuffDC);

		ReleaseDC(hWnd, hdc);
		EndPaint(hWnd, &ps);
		return 0;
	}break;
	case WM_ERASEBKGND:
	{
		return true;
	}break;
	case WM_CLOSE:
	{
		DestroyWindow(hWnd);
	}break;
	case WM_ENDSESSION: //PC is shutting down so let's do some cleaning up
	{
		if (ToastInitialised)
		{
			if (idToastNewversion || idToastFirstrun)
			{
				if (idToastFirstrun)
					WinToastLib::WinToast::instance()->hideToast(idToastFirstrun);
				if (idToastNewversion)
					WinToastLib::WinToast::instance()->hideToast(idToastNewversion);
				Sleep(500);
				WinToastLib::WinToast::instance()->clear();
			}
		}
		if (Prefs.shutdown_timing_ == PREFS_SHUTDOWN_TIMING_DELAYED)
		{
			Sleep(7000);
			ShutdownBlockReasonDestroy(hWnd);
		}
	}break;
	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}break;
	default:
		return false;
	}
	return true;
}

//   If the application is already running, tell other process to exit
bool messageExistingProcess(void)
{
	std::wstring WindowTitle;
	WindowTitle = APPNAME_FULL;
	WindowTitle += L" v";
	WindowTitle += APP_VERSION;
	bool return_value = false;

	HWND Other_hWnd = FindWindow(NULL, WindowTitle.c_str());

	while (Other_hWnd)
	{
		COPYDATASTRUCT cds;
		cds.cbData = sizeof(WCHAR);
		cds.lpData = NULL;
		cds.dwData = NOTIFY_NEW_PROCESS;
		SendMessage(Other_hWnd, WM_COPYDATA, NULL, (LPARAM)&cds);
		Sleep(100);
		Other_hWnd = FindWindow(NULL, WindowTitle.c_str());
		return_value =  true;
	}
	return return_value;
}
//   communicate with service via IPC
void communicateWithService(std::string sData)
{
	if (sData.size() == 0)
		return;
	std::string message = "-daemon ";
	if (sessionID.size() > 0)
	{
		message += sessionID;
		message += " ";
	}
	else
	{
		message += "unknown_id";
		message += " ";
	}
	message += sData;
	if (!pPipeClient->send(tools::widen(message)))
		log(L"Failed to connect to named pipe. Service may be stopped.");
	else
		log(tools::widen(message));
	return;
}

void log(std::wstring input)
{
	time_t rawtime;
	struct tm timeinfo;
	char buffer[80];

	if (!bDaemonVisible)
		return;

	std::time(&rawtime);
	localtime_s(&timeinfo, &rawtime);

	strftime(buffer, 80, "%a %H:%M:%S > ", &timeinfo);
	puts(buffer);

	std::wstring logmess = tools::widen(buffer);
	logmess += input;
	logmess += L"\r\n";

	int TextLen = (int)SendMessage(GetDlgItem(hMainWnd, IDC_EDIT), WM_GETTEXTLENGTH, 0, 0);
	SendMessage(GetDlgItem(hMainWnd, IDC_EDIT), EM_SETSEL, (WPARAM)TextLen, (LPARAM)TextLen);
	SendMessage(GetDlgItem(hMainWnd, IDC_EDIT), EM_REPLACESEL, FALSE, (LPARAM)logmess.c_str());

	return;
}

void threadVersionCheck(HWND hWnd)
{
	IStream* stream;
	char buff[100];
	std::string s;
	unsigned long bytesRead;

	if (URLOpenBlockingStream(0, VERSIONCHECKLINK, &stream, 0, 0))
		return;// error

	while (true)
	{
		stream->Read(buff, 100, &bytesRead);

		if (0U == bytesRead)
			break;
		s.append(buff, bytesRead);
	};

	stream->Release();

	size_t find = s.find("\"tag_name\":", 0);
	if (find != std::string::npos)
	{
		size_t begin = s.find_first_of("0123456789", find);
		if (begin != std::string::npos)
		{
			size_t end = s.find("\"", begin);
			std::string lastver = s.substr(begin, end - begin);

			std::vector <std::string> local_ver = tools::stringsplit(tools::narrow(APP_VERSION), ".");
			std::vector <std::string> remote_ver = tools::stringsplit(lastver, ".");

			if (local_ver.size() < 3 || remote_ver.size() < 3)
				return;
			int local_ver_major = atoi(local_ver[0].c_str());
			int local_ver_minor = atoi(local_ver[1].c_str());
			int local_ver_patch = atoi(local_ver[2].c_str());

			int remote_ver_major = atoi(remote_ver[0].c_str());
			int remote_ver_minor = atoi(remote_ver[1].c_str());
			int remote_ver_patch = atoi(remote_ver[2].c_str());

			if ((remote_ver_major > local_ver_major) ||
				(remote_ver_major == local_ver_major) && (remote_ver_minor > local_ver_minor) ||
				(remote_ver_major == local_ver_major) && (remote_ver_minor == local_ver_minor) && (remote_ver_patch > local_ver_patch))
			{
				PostMessage(hWnd, APP_NEW_VERSION, 0, 0);
			}
		}
	}
	return;
}

// get a vector of all currently active LG displays
std::vector<DisplayInfo> queryDisplays()
{
	std::vector<DisplayInfo> targets;

	//populate targets struct with information about attached displays
	EnumDisplayMonitors(NULL, NULL, meproc, (LPARAM)&targets);

	return targets;
}
static BOOL CALLBACK meproc(HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM pData)
{
	if (!pData)
		return false;
	std::vector<DisplayInfo>* targets = (std::vector<DisplayInfo> *) pData;
	UINT32 requiredPaths, requiredModes;
	std::vector<DISPLAYCONFIG_PATH_INFO> paths;
	std::vector<DISPLAYCONFIG_MODE_INFO> modes;
	MONITORINFOEX mi;
	LONG isError = ERROR_INSUFFICIENT_BUFFER;

	ZeroMemory(&mi, sizeof(mi));
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);
	isError = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, &requiredModes);
	if (isError)
	{
		targets->clear();
		return false;
	}
	paths.resize(requiredPaths);
	modes.resize(requiredModes);

	isError = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, paths.data(), &requiredModes, modes.data(), NULL);
	if (isError)
	{
		targets->clear();
		return false;
	}
	paths.resize(requiredPaths);
	modes.resize(requiredModes);

	for (auto& p : paths)
	{
		DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
		sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
		sourceName.header.size = sizeof(sourceName);
		sourceName.header.adapterId = p.sourceInfo.adapterId;
		sourceName.header.id = p.sourceInfo.id;

		DisplayConfigGetDeviceInfo(&sourceName.header);
		if (wcscmp(mi.szDevice, sourceName.viewGdiDeviceName) == 0)
		{
			DISPLAYCONFIG_TARGET_DEVICE_NAME name;
			name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
			name.header.size = sizeof(name);
			name.header.adapterId = p.sourceInfo.adapterId;
			name.header.id = p.targetInfo.id;
			DisplayConfigGetDeviceInfo(&name.header);
			std::wstring FriendlyName = name.monitorFriendlyDeviceName;
			transform(FriendlyName.begin(), FriendlyName.end(), FriendlyName.begin(), ::tolower);
			if (FriendlyName.find(L"lg tv") != std::wstring::npos)
			{
				DisplayInfo di;
				di.monitorinfo = mi;
				di.hMonitor = hMonitor;
				di.hdcMonitor = hdc;
				di.rcMonitor2 = *(LPRECT)lprcMonitor;
				di.target = name;
				targets->push_back(di);
			}
		}
	}
	return true;
}
bool checkDisplayTopology(void)
{
	std::string top;
	top = "topology state ";
	if (Prefs.devices_.size() == 0)
		return false;
	std::vector<DisplayInfo> displays = queryDisplays();

	if (displays.size() > 0)
	{
		for (auto& disp : displays)
		{
			for (auto& dev : Prefs.devices_)
			{
				std::string ActiveDisplay = tools::tolower(tools::narrow(disp.target.monitorDevicePath));
				std::string DeviceString = tools::tolower(dev.uniqueDeviceKey);
				if(DeviceString != "")
				{
					if (ActiveDisplay == DeviceString)
					{
						top += dev.id;
						top += " ";
					}
				}
			}
		}
	}
	communicateWithService(top);
	return true;
}

int verifyTopology(void)
{
	bool match = false;

	if (!Prefs.topology_support_)
		return TOPOLOGY_OK_DISABLE;
	if (Prefs.devices_.size() == 0)
		return TOPOLOGY_OK_DISABLE;

	std::vector<DisplayInfo> displays = queryDisplays();
	if (displays.size() == 0)
		return TOPOLOGY_UNDETERMINED;

	for (auto& disp : displays)
	{
		match = false;

		for (auto& dev : Prefs.devices_)
		{
			std::string ActiveDisplay = tools::narrow(disp.target.monitorDevicePath);
			std::string DeviceString = dev.uniqueDeviceKey;
			transform(ActiveDisplay.begin(), ActiveDisplay.end(), ActiveDisplay.begin(), ::tolower);
			transform(DeviceString.begin(), DeviceString.end(), DeviceString.begin(), ::tolower);
			if (ActiveDisplay == DeviceString)
				match = true;
		}
		if (!match)
			return TOPOLOGY_ERROR;
	}
	return TOPOLOGY_OK;
}

DWORD checkRemoteStreamingProcesses(void)
{
	bool bWhiteListConfigured = Prefs.user_idle_mode_whitelist_processes_.size() > 0 ? true : false;
	bool bFsExclusionListConfigured = Prefs.user_idle_mode_exclude_fullscreen_whitelist_processes_.size() > 0 ? true : false;
	bool bWhitelistProcessFound = false;
	bool bFsExclusionProcessFound = false;
	bool bStreamingProcessFound = false;
	bool bSunshineSvcProcessFound = false;
	std::wstring sWhitelistProcessFound = L"";
	std::wstring sFsExclusionProcessFound = L"";
	DWORD ReturnValue = 0;

	// Iterate over currently running processes
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (!Process32First(snapshot, &entry)) {
		CloseHandle(snapshot);
		log(L"Failed to iterate running processes");
		return NULL;
	}
	do
	{
		// look for user idle mode whitelisted processes
		if (Prefs.user_idle_mode_whitelist_ && bWhiteListConfigured && !bWhitelistProcessFound)
			for (auto& w : Prefs.user_idle_mode_whitelist_processes_)
				if (w.binary != L"")
					if (!_tcsicmp(entry.szExeFile, w.binary.c_str()))
					{
						if (w.friendly_name != L"")
							sWhitelistProcessFound = w.friendly_name;
						else
							sWhitelistProcessFound = L"<unnamed>";
						bWhitelistProcessFound = true;
					}
		// look for user idle mode fullscreen excluded processes
		if (Prefs.user_idle_mode_exclude_fullscreen_whitelist_ && bFsExclusionListConfigured && !bFsExclusionProcessFound)
			for (auto& w : Prefs.user_idle_mode_exclude_fullscreen_whitelist_processes_)
				if (w.binary != L"")
					if (!_tcsicmp(entry.szExeFile, w.binary.c_str()))
					{
						if (w.friendly_name != L"")
							sFsExclusionProcessFound = w.friendly_name;
						else
							sFsExclusionProcessFound = L"<unnamed>";
						bFsExclusionProcessFound = true;
					}

		// look for currently running known streaming processes
		if (Prefs.remote_streaming_host_support_ && !bStreamingProcessFound)
			for (auto& w : Remote.stream_proc_list)
				if (!_tcsicmp(entry.szExeFile, w.c_str()))
					bStreamingProcessFound = true;

		// look for currently running sushine service process
		if (Prefs.remote_streaming_host_support_ && !bSunshineSvcProcessFound)
			if (!_tcsicmp(entry.szExeFile, SUNSHINE_FILE_SVC))
				bSunshineSvcProcessFound = true;
	} while (!(bStreamingProcessFound && bWhitelistProcessFound && bSunshineSvcProcessFound && bFsExclusionProcessFound) && Process32Next(snapshot, &entry));

	CloseHandle(snapshot);

	// was sunshine service found currently running?
	if (bSunshineSvcProcessFound && (Remote.Sunshine_Log_File != ""))
	{
		std::filesystem::path p(Remote.Sunshine_Log_File);
		uintmax_t Size = std::filesystem::file_size(p);
		if (Size != Remote.Sunshine_Log_Size)
		{
			ReturnValue ^= sunshine_CheckLog();
			Remote.Sunshine_Log_Size = Size;
		}
	}
	else
		ReturnValue ^= REMOTE_SUNSHINE_NOT_CONNECTED;

	// was a remote streaming process currently running?
	ReturnValue ^= bStreamingProcessFound ? REMOTE_STEAM_CONNECTED : REMOTE_STEAM_NOT_CONNECTED;

	// was a user idle mode whitelisted / FS excluded process currently running?
	Remote.sCurrentlyRunningWhitelistedProcess = sWhitelistProcessFound;
	Remote.sCurrentlyRunningFsExcludedProcess = sFsExclusionProcessFound;
	return ReturnValue;
}
bool isFullscreenApplicationRunning(void)
{
	workaroundFalseFullscreenWindows();

	QUERY_USER_NOTIFICATION_STATE pquns;

	if (SHQueryUserNotificationState(&pquns) == S_OK)
	{
		if (pquns == QUNS_RUNNING_D3D_FULL_SCREEN ||
			pquns == QUNS_PRESENTATION_MODE ||
			pquns == QUNS_BUSY ||
			pquns == QUNS_APP)
		{
			return true;
		}
	}
	return false;
}
void workaroundFalseFullscreenWindows(void)
{
	EnumWindows(enumWindowsProc, 0);
}
static BOOL	CALLBACK	enumWindowsProc(HWND hWnd, LPARAM lParam)
{
	if (!IsWindowVisible(hWnd))
		return true;

	const wchar_t* const nonRude = L"NonRudeHWND";
	{
		std::wstring window_name = getWndText(hWnd);
		transform(window_name.begin(), window_name.end(), window_name.begin(), ::tolower);
		if (window_name.find(L"nvidia geforce overlay") != std::wstring::npos)
		{
			if (GetProp(hWnd, nonRude) == NULL)
			{
				if (SetProp(hWnd, nonRude, INVALID_HANDLE_VALUE))
				{
					DWORD recipients = BSM_APPLICATIONS;
					if (BroadcastSystemMessage(BSF_POSTMESSAGE | BSF_IGNORECURRENTTASK, &recipients, shellhookMessage, HSHELL_UNDOCUMENTED_FULLSCREEN_EXIT, (LPARAM)hWnd) < 0)
					{
						log(L"BroadcastSystemMessage() failed");
					}
					else {
						communicateWithService("gfe");
						log(L"Unset NVIDIA GFE overlay fullscreen");
					}

					return false;
				}
			}
		}
	}
	return true;
}
void remoteStreamingEvent(DWORD dwType)
{
	if (Prefs.remote_streaming_host_support_)
	{
		bool bCurrentlyConnected = Remote.bRemoteCurrentStatusSteam || Remote.bRemoteCurrentStatusNvidia || Remote.bRemoteCurrentStatusRDP || Remote.bRemoteCurrentStatusSunshine;

		if (dwType & REMOTE_STEAM_CONNECTED)
		{
			if (!Remote.bRemoteCurrentStatusSteam)
			{
				log(L"Steam gamestream connected.");
				Remote.bRemoteCurrentStatusSteam = true;
			}
		}
		else if (dwType & REMOTE_STEAM_NOT_CONNECTED)
		{
			if (Remote.bRemoteCurrentStatusSteam)
			{
				log(L"Steam gamestream disconnected.");
				Remote.bRemoteCurrentStatusSteam = false;
			}
		}
		if (dwType & REMOTE_NVIDIA_CONNECTED)
		{
			if (!Remote.bRemoteCurrentStatusNvidia)
			{
				log(L"nVidia gamestream connected.");
				Remote.bRemoteCurrentStatusNvidia = true;
			}
		}
		else if (dwType & REMOTE_NVIDIA_NOT_CONNECTED)
		{
			if (Remote.bRemoteCurrentStatusNvidia)
			{
				log(L"nVidia gamestream disconnected.");
				Remote.bRemoteCurrentStatusNvidia = false;
			}
		}
		if (dwType & REMOTE_SUNSHINE_CONNECTED)
		{
			if (!Remote.bRemoteCurrentStatusSunshine)
			{
				log(L"Sunshine gamestream connected.");
				Remote.bRemoteCurrentStatusSunshine = true;
			}
		}
		else if (dwType & REMOTE_SUNSHINE_NOT_CONNECTED)
		{
			if (Remote.bRemoteCurrentStatusSunshine)
			{
				log(L"Sunshine gamestream disconnected.");
				Remote.bRemoteCurrentStatusSunshine = false;
			}
		}
		if (dwType & REMOTE_RDP_CONNECTED)
		{
			if (!Remote.bRemoteCurrentStatusRDP)
			{
				log(L"RDP connected.");
				Remote.bRemoteCurrentStatusRDP = true;
			}
		}
		else if (dwType & REMOTE_RDP_NOT_CONNECTED)
		{
			if (Remote.bRemoteCurrentStatusRDP)
			{
				log(L"RDP disconnected.");
				Remote.bRemoteCurrentStatusRDP = false;
			}
		}
		bool bConnect = Remote.bRemoteCurrentStatusSteam || Remote.bRemoteCurrentStatusNvidia || Remote.bRemoteCurrentStatusRDP || Remote.bRemoteCurrentStatusSunshine;

		if (bCurrentlyConnected && !bConnect)
			SetTimer(hMainWnd, (UINT_PTR)REMOTE_DISCONNECT, 1000, (TIMERPROC)NULL);
		else if (!bCurrentlyConnected && bConnect)
			SetTimer(hMainWnd, (UINT_PTR)REMOTE_CONNECT, TIMER_REMOTE_DELAY, (TIMERPROC)NULL);
	}
	return;
}

DWORD sunshine_CheckLog(void)
{
	std::string log;
	std::ifstream t(Remote.Sunshine_Log_File);
	if (t.is_open())
	{
		std::stringstream buffer;
		t.seekg(-5000, std::ios_base::end);
		buffer << t.rdbuf();
		std::string log = buffer.str();
		t.close();

		int type;
		size_t pos_connect = log.rfind("CLIENT CONNECTED");
		size_t pos_disconnect = log.rfind("CLIENT DISCONNECTED");
		if (pos_connect == std::string::npos && pos_disconnect == std::string::npos)
			return 0;
		else if (pos_connect == std::string::npos)
			type = REMOTE_SUNSHINE_NOT_CONNECTED;
		else if (pos_disconnect == std::string::npos)
			type = REMOTE_SUNSHINE_CONNECTED;
		else
			type = (pos_connect > pos_disconnect) ? REMOTE_SUNSHINE_CONNECTED : REMOTE_SUNSHINE_NOT_CONNECTED;

		size_t begin = log.rfind("[", type == REMOTE_SUNSHINE_CONNECTED ? pos_connect : pos_disconnect);
		std::string time = log.substr(begin + 1, 19); //2023:02:02:00:04:38
		time[4] = '-';
		time[7] = '-';
		time[10] = ' ';
		time += ".000"; //2023-02-02 00:04:38.000
		std::tm tmTime = boost::posix_time::to_tm(boost::posix_time::time_from_string(time));

		time_t entry_time = mktime(&tmTime);
		time_t now_time = std::time(0);
		double seconds = difftime(now_time, entry_time);
		if (seconds < (DOUBLE)10)
			return type;
	}
	return 0;
}

std::string sunshine_GetLogFile()
{
	std::string Sunshine_Config;
	std::string configuration_path;
	std::string configuration_text;
	std::string log_file;

	HKEY hKey = NULL;
	LPCSTR pszSubkey = SUNSHINE_REG;
	LPCSTR pszValueName = "";
	//Get sunshine install path from registry
	if (RegOpenKeyA(HKEY_LOCAL_MACHINE, pszSubkey, &hKey) == ERROR_SUCCESS)
	{
		// Buffer to store string read from registry
		CHAR szValue[MAX_PATH];
		DWORD cbValueLength = sizeof(szValue);

		// Query string value
		if (RegQueryValueExA(hKey, pszValueName, NULL, NULL, reinterpret_cast<LPBYTE>(&szValue), &cbValueLength) == ERROR_SUCCESS)
		{
			configuration_path = szValue;
			configuration_path += "\\config\\";
			Sunshine_Config = configuration_path;
			Sunshine_Config += SUNSHINE_FILE_CONF;

			//open the configuration file
			std::ifstream t(Sunshine_Config);
			if (t.is_open())
			{
				std::stringstream buffer;
				buffer << t.rdbuf();
				configuration_text = buffer.str();
				t.close();

				std::string s = sunshine_GetConfVal(configuration_text, "min_log_level");
				if (s != "" && s != "2" && s != "1" && s != "0")
				{
					log(L"Logging need to be at minimum on level \"info\" in Sunshine.");
					return "";
				}

				s = sunshine_GetConfVal(configuration_text, "log_path");
				if (s == "") //DEFAULT
				{
					log_file = configuration_path;
					log_file += SUNSHINE_FILE_LOG;
				}
				else
				{
					std::filesystem::path log_p(s);
					std::filesystem::current_path(configuration_path);
					std::filesystem::path log_abs(std::filesystem::absolute(log_p));
					log_file = log_abs.string();
				}
				return log_file;
			}
		}
	}
	return "";
}

std::string sunshine_GetConfVal(std::string buf, std::string conf_item)
{
	conf_item += " = ";
	size_t pos = buf.find(conf_item);
	if (pos != std::string::npos)
	{
		size_t newl = buf.find("\n", pos);
		if (newl == std::string::npos)
			return buf.substr(pos + conf_item.length());
		else
			return buf.substr(pos + conf_item.length(), newl - (pos + conf_item.length()));
	}
	return "";
}

void ipcCallback(std::wstring message, LPVOID pt)
{
	return;
}
std::wstring getWndText(HWND hWnd)
{
	int len = GetWindowTextLength(hWnd) + 1;
	std::vector<wchar_t> buf(len);
	GetWindowText(hWnd, &buf[0], len);
	std::wstring text = &buf[0];
	return text;
}

bool IsWindowElevated(HWND hWnd) {
	DWORD processId = 0;
	HANDLE hProcess = nullptr;
	HANDLE hToken = nullptr;
	bool isElevated = false;

	if (hWnd == NULL)
		return false;

	// Get process ID from window handle
	GetWindowThreadProcessId(hWnd, &processId);

	// Open the process with query limited information access
	hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
	if (!hProcess) {
		return false;
	}

	// Get the process token
	if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
		CloseHandle(hProcess);
		return false;
	}

	// Check token elevation status
	TOKEN_ELEVATION_TYPE elevationType;
	DWORD dwSize;
	if (GetTokenInformation(hToken, TokenElevationType,
		&elevationType, sizeof(elevationType), &dwSize)) {
		isElevated = (elevationType == TokenElevationTypeFull);
	}

	// Clean up handles
	CloseHandle(hToken);
	CloseHandle(hProcess);

	return isElevated;
}

void RawInput_AddToCache(const std::wstring& devicePath) {
	HANDLE hDevice = CreateFile(devicePath.c_str(), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) return;

	PHIDP_PREPARSED_DATA ppd = nullptr;
	if (!HidD_GetPreparsedData(hDevice, &ppd)) {
		CloseHandle(hDevice);
		return;
	}

	HIDP_CAPS caps;
	if (HidP_GetCaps(ppd, &caps) != HIDP_STATUS_SUCCESS) {
		HidD_FreePreparsedData(ppd);
		CloseHandle(hDevice);
		return;
	}

	g_deviceCache[devicePath] = { hDevice, ppd, caps };
}

void RawInput_ClearCache() {
	for (auto it = g_deviceCache.begin(); it != g_deviceCache.end(); ++it) {
		HidD_FreePreparsedData(it->second.ppd);
		CloseHandle(it->second.hDevice);
		it->second.axes.clear();
	}
	g_deviceCache.clear();
}