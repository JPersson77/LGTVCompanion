#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_ARM64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='arm64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#include "daemon.h"
#include "../Common/common_app_define.h"
#include "../Common/ipc_v2.h"
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
#include <Shobjidl.h>
#include <ctime>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "user_idle_mode.h"
#include "resource.h"

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "ntdll.lib")

#define			APPNAME_SHORT							L"LGTVdaemon"
#define			APPNAME_FULL							L"LGTV Companion Daemon"
#define         TIMER_MAIN								18
#define         TIMER_IDLE								19
#define         TIMER_TOPOLOGY							20
#define         TIMER_CHECK_PROCESSES					21
#define         TIMER_TOPOLOGY_COLLECTION				22
#define         TIMER_VERSIONCHECK						23
#define         TIMER_MAIN_DELAY_WHEN_BUSY				1000
#define         TIMER_MAIN_DELAY_WHEN_IDLE				50
#define         TIMER_REMOTE_DELAY						10000
#define         TIMER_TOPOLOGY_DELAY					8000
#define         TIMER_VERSIONCHECK_DELAY				30000
#define         TIMER_CHECK_PROCESSES_DELAY				5000
#define         TIMER_TOPOLOGY_COLLECTION_DELAY			3000
#define			COPYDATA_MUTEX_WAIT						10
#define         APP_DISPLAYCHANGE						WM_USER+10
#define         APP_SET_MESSAGEFILTER					WM_USER+11
#define         APP_USER_IDLE_ON						1
#define         APP_USER_IDLE_OFF						2
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
#define			REMOTE_APOLLO_CONNECTED					0x0100
#define			REMOTE_APOLLO_NOT_CONNECTED				0x0200
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
	bool								bRemoteCurrentStatusApollo = false;
	std::wstring						sCurrentlyRunningWhitelistedProcess = L"";
	std::wstring						sCurrentlyRunningFsExcludedProcess = L"";
	std::vector<std::string>			Sunshine_Log_Files;
	std::vector<uintmax_t>				Sunshine_Log_Sizes;

	const std::vector<std::string>		sunshine_list{
	"SOFTWARE\\LizardByte\\Sunshine",	//sunshine reg location
	"SOFTWARE\\SudoMaker\\Apollo"
	};
	const std::vector<std::wstring>		stream_proc_list{
	L"steam_monitor.exe"				//steam server
	};
	const std::vector<std::wstring>		stream_usb_list_gamestream{
	L"usb#vid_0955&pid_b4f0"			//nvidia
	};
} Remote;

struct ControllerInfo { // Cache for raw input data stuff
	HANDLE hDevice = NULL;
	PHIDP_PREPARSED_DATA ppd = NULL;
	HIDP_CAPS caps = { 0 };
	std::vector<int> axis_value;
};

// Globals:
HINSTANCE                       h_instance;  // current instance
HWND                            h_main_wnd = NULL;
bool                            user_is_idle = false;
bool                            daemon_is_visible = true;
HANDLE                          h_pipe = INVALID_HANDLE_VALUE;
UINT							custom_shellhook_message;
UINT							custom_daemon_restart_message;
UINT							custom_daemon_close_message;
UINT							custom_daemon_idle_message;
UINT							custom_daemon_unidle_message;
UINT							custom_updater_close_message;
UINT							manual_user_idle_mode = 0;
HBRUSH                          h_backbrush;
time_t							time_of_last_topology_change = 0;
DWORD							time_of_last_raw_input = 0;
DWORD							time_of_last_mouse_sample = 0;
DWORD							time_of_last_input_info = 0;
time_t							time_of_last_version_check = 0;
DWORD							last_input = 0;
DWORD							time_of_last_controller_tick = 0;
int								number_of_mouse_checks = 0;
inline static std::mutex		copydata_mutex_;

std::shared_ptr<IpcClient2>		p_pipe_client;
Preferences						Prefs(CONFIG_FILE);
std::string						session_id;
std::unordered_map<std::wstring, ControllerInfo> g_device_cache; // Key = device path

//Application entry point
int APIENTRY wWinMain(_In_ HINSTANCE Instance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(nCmdShow);
	MSG msg;
	HPOWERNOTIFY					rsrn;
	HPOWERNOTIFY					rpsn;
	HDEVNOTIFY						dev_notify = NULL;
	std::wstring window_title;
	h_instance = Instance;

	//commandline processing
	if (lpCmdLine)
	{
		std::wstring CommandLineParameters = lpCmdLine;
		transform(CommandLineParameters.begin(), CommandLineParameters.end(), CommandLineParameters.begin(), ::tolower);

		if (CommandLineParameters == L"-run_hidden") // launched from task scheduler as hidden
			daemon_is_visible = false;
		else if (CommandLineParameters == L"-run_visible") // launched from task scheduler as visible
			daemon_is_visible = true;
		else if (CommandLineParameters == L"-restart") // launched from UI when applying configuration
		{
			if (!tools::startScheduledTask(TASK_FOLDER, TASK_DAEMON))
				MessageBox(NULL, L"Failed to launch the Daemon. The Microsoft Task Scheduler service may be stopped", L"Error", MB_OK | MB_ICONEXCLAMATION);
			return 0;
		}
		else // launched by user
		{
			daemon_is_visible = true;
/*
			if(!tools::startScheduledTask(TASK_FOLDER, TASK_DAEMON_VISIBLE))
				MessageBox(NULL, L"Failed to launch the Daemon. The Microsoft Task Scheduler service may be stopped", L"Error", MB_OK | MB_ICONEXCLAMATION);
			return 0;
*/
		}
	}
	else 
	{
		daemon_is_visible = true;
/*
		if (!tools::startScheduledTask(TASK_FOLDER, TASK_DAEMON_VISIBLE))
			MessageBox(NULL, L"Failed to launch the Daemon. The Microsoft Task Scheduler service may be stopped", L"Error", MB_OK | MB_ICONEXCLAMATION);
		return 0;
*/
	}
	custom_shellhook_message = RegisterWindowMessage(L"SHELLHOOK");
	custom_daemon_restart_message = RegisterWindowMessage(CUSTOM_MESSAGE_RESTART);
	custom_daemon_close_message = RegisterWindowMessage(CUSTOM_MESSAGE_CLOSE);
	custom_daemon_idle_message = RegisterWindowMessage(CUSTOM_MESSAGE_IDLE);
	custom_daemon_unidle_message = RegisterWindowMessage(CUSTOM_MESSAGE_UNIDLE);
	custom_updater_close_message = RegisterWindowMessage(CUSTOM_MESSAGE_UPD_CLOSE);

	// if the app is already running as another process, tell the other process(es) to exit
	closeExistingProcess();

	// Initiate PipeClient IPC
	p_pipe_client = std::make_shared<IpcClient2>(PIPENAME, ipcCallback, (LPVOID)NULL);

	// read the configuration file and init prefs
	if(!Prefs.isInitialised())
	{
		communicateWithService(L"errorconfig", false);
		return false;
	}

	// Get Session ID
	PULONG pSessionId = NULL;
	DWORD BytesReturned = 0;
	if (WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionId, (LPSTR*)&pSessionId, &BytesReturned))
	{
		DWORD id = (DWORD)*pSessionId;
		WTSFreeMemory(pSessionId);
		session_id = std::to_string(id);
	}

	// create main window (dialog)
	h_backbrush = CreateSolidBrush(0x00ffffff);
	HICON h_icon = LoadIcon(h_instance, MAKEINTRESOURCE(IDI_ICON1));
	window_title = APPNAME_FULL;
	window_title += L" v";
	window_title += APP_VERSION;
	h_main_wnd = CreateDialog(h_instance, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)WndProc);
	SetWindowText(h_main_wnd, window_title.c_str());
	SendMessage(h_main_wnd, WM_SETICON, ICON_BIG, (LPARAM)h_icon);

	//allow WM_COPYDATA to elevated
	CHANGEFILTERSTRUCT cfs = { sizeof(cfs), 0 }; 
	ChangeWindowMessageFilterEx(h_main_wnd, WM_COPYDATA, MSGFLT_ALLOW, &cfs);

	// start timers
	if (Prefs.updater_mode_ != PREFS_UPDATER_OFF)
		SetTimer(h_main_wnd, TIMER_VERSIONCHECK, TIMER_VERSIONCHECK_DELAY, (TIMERPROC)NULL);
	if (Prefs.topology_support_)
		SetTimer(h_main_wnd, TIMER_TOPOLOGY, TIMER_TOPOLOGY_DELAY, (TIMERPROC)NULL);
	if (Prefs.remote_streaming_host_support_)
		SetTimer(h_main_wnd, TIMER_CHECK_PROCESSES, TIMER_CHECK_PROCESSES_DELAY, (TIMERPROC)NULL);
	if (Prefs.user_idle_mode_)
	{
		RAWINPUTDEVICE Rid[4];
		Rid[0].usUsagePage = 0x01;							// HID_USAGE_PAGE_GENERIC
		Rid[0].usUsage = 0x02;								// HID_USAGE_GENERIC_MOUSE
		Rid[0].dwFlags = daemon_is_visible ? RIDEV_INPUTSINK : RIDEV_INPUTSINK | RIDEV_NOLEGACY;
		Rid[0].hwndTarget = h_main_wnd;

		Rid[1].usUsagePage = 0x01;							// HID_USAGE_PAGE_GENERIC
		Rid[1].usUsage = 0x04;								// HID_USAGE_GENERIC_JOYSTICK
		Rid[1].dwFlags = RIDEV_INPUTSINK;
		Rid[1].hwndTarget = h_main_wnd;

		Rid[2].usUsagePage = 0x01;							// HID_USAGE_PAGE_GENERIC
		Rid[2].usUsage = 0x05;								// HID_USAGE_GENERIC_GAMEPAD
		Rid[2].dwFlags = RIDEV_INPUTSINK;
		Rid[2].hwndTarget = h_main_wnd;

		Rid[3].usUsagePage = 0x01;							// HID_USAGE_PAGE_GENERIC
		Rid[3].usUsage = 0x06;								// HID_USAGE_GENERIC_KEYBOARD
		Rid[3].dwFlags = daemon_is_visible ? RIDEV_INPUTSINK : RIDEV_INPUTSINK | RIDEV_NOLEGACY;
		Rid[3].hwndTarget = h_main_wnd;

		UINT deviceCount = sizeof(Rid) / sizeof(*Rid);
		if (RegisterRawInputDevices(Rid, deviceCount, sizeof(Rid[0])) == FALSE)
		{
			log(L"Failed to register for Raw Input!");
		}
		time_of_last_raw_input = GetTickCount();
		SetTimer(h_main_wnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
		SetTimer(h_main_wnd, TIMER_IDLE, Prefs.user_idle_mode_delay_ * 60 * 1000, (TIMERPROC)NULL);
	}

	// log some basic info
	std::wstring log_message;
	log_message = window_title;
	log_message += L" is running.";
	log(log_message);
	communicateWithService(L"started", false);
	log_message = L"Session ID is: ";
	log_message += tools::widen(session_id);
	log(log_message);

	// register to receive power notifications
	rsrn = RegisterSuspendResumeNotification(h_main_wnd, DEVICE_NOTIFY_WINDOW_HANDLE);
	rpsn = RegisterPowerSettingNotification(h_main_wnd, &(GUID_CONSOLE_DISPLAY_STATE), DEVICE_NOTIFY_WINDOW_HANDLE);

	// register to receive device attached/detached messages
	DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
	if (Prefs.remote_streaming_host_support_)
	{
		ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
		NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
		NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		memcpy(&(NotificationFilter.dbcc_classguid), &(GUID_DEVINTERFACE_USB_DEVICE), sizeof(struct _GUID));
		dev_notify = RegisterDeviceNotification(h_main_wnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

		WTSRegisterSessionNotification(h_main_wnd, NOTIFY_FOR_ALL_SESSIONS);

		Remote.Sunshine_Log_Files = sunshine_GetLogFiles();
		if (Remote.Sunshine_Log_Files.size() > 0)
		{
			for (auto& log : Remote.Sunshine_Log_Files) {
				std::filesystem::path p(log);
				Remote.Sunshine_Log_Sizes.push_back(std::filesystem::file_size(p));
			}
		}
	}


	// message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage(h_main_wnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	//clean up
	p_pipe_client->terminate();
	DeleteObject(h_backbrush);
	UnregisterSuspendResumeNotification(rsrn);
	UnregisterPowerSettingNotification(rpsn);
	if (dev_notify)
		UnregisterDeviceNotification(dev_notify);
	if (Prefs.remote_streaming_host_support_)
		WTSUnRegisterSessionNotification(h_main_wnd);
	RawInput_ClearCache();
	return (int)msg.wParam;
}

//   Process messages for the main window.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// handle intra-app custom messages
	if (message == custom_daemon_close_message)
		PostMessage(hWnd, WM_CLOSE, NULL, NULL);
	else if (message == custom_daemon_restart_message)
	{
		tools::startScheduledTask(TASK_FOLDER, TASK_DAEMON);
		PostMessage(hWnd, WM_CLOSE, NULL, NULL);
	}
	else if (message == custom_daemon_idle_message)
	{
		if (Prefs.user_idle_mode_)
		{
			manual_user_idle_mode = APP_USER_IDLE_ON;
			user_is_idle = true;
			SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_IDLE, (TIMERPROC)NULL);
			SetTimer(hWnd, TIMER_IDLE, Prefs.user_idle_mode_delay_ * 60 * 1000, (TIMERPROC)NULL);
			log(L"User forced user idle mode!");
			communicateWithService(L"useridle", false);
		}
		else
			log(L"Can not force user idle mode, as the feature is not enabled in the global options!");
	}
	else if (message == custom_daemon_unidle_message)
	{
		if (Prefs.user_idle_mode_)
		{
			manual_user_idle_mode = 0;
			user_is_idle = false;
			SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
			SetTimer(hWnd, TIMER_IDLE, Prefs.user_idle_mode_delay_ * 60 * 1000, (TIMERPROC)NULL);
			time_of_last_raw_input = GetTickCount();
			log(L"User forced unsetting user idle mode!");
			communicateWithService(L"userbusy", false);
		}
		else
			log(L"Can not force unset user idle mode, as the feature is not enabled in the global options!");
	}

	switch (message)
	{
	case WM_INITDIALOG:
	{
		SetCurrentProcessExplicitAppUserModelID(L"JPersson.LGTVCompanion.18");
		SetWindowPos(hWnd, NULL, 0, 0, 480, 300, SWP_NOMOVE );
		PostMessage(hWnd, APP_SET_MESSAGEFILTER, NULL, NULL);

	}break;
	case APP_SET_MESSAGEFILTER:
	{
		ChangeWindowMessageFilterEx(hWnd, custom_daemon_restart_message, MSGFLT_ALLOW, NULL);
		ChangeWindowMessageFilterEx(hWnd, custom_daemon_idle_message, MSGFLT_ALLOW, NULL);
		ChangeWindowMessageFilterEx(hWnd, custom_daemon_unidle_message, MSGFLT_ALLOW, NULL);
		ChangeWindowMessageFilterEx(hWnd, custom_daemon_close_message, MSGFLT_ALLOW, NULL);
		if (daemon_is_visible)
			SetWindowPos(hWnd, HWND_TOP, 0, 0, 481, 301, SWP_NOMOVE);
			ShowWindow(h_main_wnd, daemon_is_visible ? SW_SHOWNORMAL : SW_HIDE);
	}break;

	case WM_INPUT:
	{
		if (!lParam)
			return 0;
		DWORD tick_now = GetTickCount();
		if ((tick_now - time_of_last_raw_input) < TIMER_MAIN_DELAY_WHEN_BUSY)
		{
			return 0;
		}
		HRAWINPUT h_raw_input = reinterpret_cast<HRAWINPUT>(lParam);
		UINT size = 0;
		if (GetRawInputData(h_raw_input, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) == -1)
			return 0;
		if (size == 0)
			return 0;
		std::vector<BYTE> buffer(size);
		if (GetRawInputData(h_raw_input, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) != size) 
			return 0;
		PRAWINPUT raw = reinterpret_cast<PRAWINPUT>(buffer.data());

		// Was a key pressed?
		if (raw->header.dwType == RIM_TYPEKEYBOARD)
		{
			time_of_last_raw_input = tick_now;
			return 0;
		}
		// Was the mouse moved?
		else if (raw->header.dwType == RIM_TYPEMOUSE)
		{
			// avoid accidental mouse-wake
			if (tick_now - time_of_last_mouse_sample < 25)
				break;
			if (tick_now - time_of_last_mouse_sample > 500)
				number_of_mouse_checks = 1;
			else
				number_of_mouse_checks++;
			if (number_of_mouse_checks >= 3)
				time_of_last_raw_input = tick_now;
			time_of_last_mouse_sample = tick_now;
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
				return 0;
			// Check if controller exists in cache or perform initialization
			std::wstring devicePath(deviceName.data());
			auto it = g_device_cache.find(devicePath);
			if (it == g_device_cache.end()) {
				if (!RawInput_AddToCache(devicePath))
					return 0;
				it = g_device_cache.find(devicePath); // Re-check after insertion attempt
				if (it == g_device_cache.end())
					return 0;
			}
			// Check whether a digital button was pressed on the controller
			ControllerInfo& cache = it->second;
			if (cache.caps.NumberInputButtonCaps > 0)
			{
				std::vector<HIDP_BUTTON_CAPS> buttonCaps(cache.caps.NumberInputButtonCaps);
				USHORT buttonCapsLength = cache.caps.NumberInputButtonCaps;
				if (HidP_GetButtonCaps(HidP_Input, buttonCaps.data(), &buttonCapsLength, cache.ppd) == HIDP_STATUS_SUCCESS)
				{
					ULONG usageLength = buttonCaps[0].Range.UsageMax - buttonCaps[0].Range.UsageMin + 1;
					if(usageLength > 0)
					{
						std::vector<USAGE> usages(usageLength);
						if (HidP_GetUsages(HidP_Input, buttonCaps[0].UsagePage, 0, usages.data(), &usageLength, cache.ppd,
							reinterpret_cast<PCHAR>(raw->data.hid.bRawData), raw->data.hid.dwSizeHid) == HIDP_STATUS_SUCCESS)
						{
							if (usageLength > 0)
							{
								time_of_last_raw_input = tick_now;
								return 0;
							}
						}
					}
				}
			}
			// avoid accidental wake
			if (tick_now - time_of_last_controller_tick < 5)
			{
				return 0;
			}
			time_of_last_controller_tick = tick_now;
			if (cache.caps.NumberInputValueCaps > 0)
			{
				std::vector<HIDP_VALUE_CAPS> valueCaps(cache.caps.NumberInputValueCaps);
				USHORT valueCapsLength = cache.caps.NumberInputValueCaps;
				if (cache.axis_value.size() != valueCapsLength)
				{
					cache.axis_value.clear();
					cache.axis_value.assign(valueCapsLength, -1);
				}

				if (HidP_GetValueCaps(HidP_Input, valueCaps.data(), &valueCapsLength, cache.ppd) == HIDP_STATUS_SUCCESS) 
				{
					for (USHORT i = 0; i < valueCapsLength; ++i)
					{
						//ps5-fix
						if (i == 6 || i == 8)
							continue;
						ULONG value;
						if (HidP_GetUsageValue(HidP_Input, valueCaps[i].UsagePage, 0, valueCaps[i].Range.UsageMin, &value, cache.ppd,
							reinterpret_cast<PCHAR>(raw->data.hid.bRawData), raw->data.hid.dwSizeHid) == HIDP_STATUS_SUCCESS) 
						{
							// Scale to normalized range
							int scaledValue = value;
							if (valueCaps[i].LogicalMin < valueCaps[i].LogicalMax)
							{
								LONG min = valueCaps[i].LogicalMin;
								LONG max = valueCaps[i].LogicalMax;
								scaledValue = (int)((value - min) * 65535 / (max - min));
							}
							// check if sticks were moved
							if (cache.axis_value[i] != -1)
							{
								if (abs(cache.axis_value[i] - scaledValue) >= (int)(0.045 * 65535)) // sensitivity of the input detection
								{
									time_of_last_raw_input = tick_now;
								}
							}
							cache.axis_value[i] = scaledValue;
						}
					}
				}
			}
		}
		return 0;
	}break;

	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDC_BUTTON2:
			{
				daemon_is_visible = false;
				ShowWindow(hWnd, SW_HIDE);
			}break;
			case IDC_TESTBUTTON:
			{
				log(L"Attempting to unset Rude Overlay");
				workaroundFalseFullscreenWindows();
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
	case WM_TIMER:
	{
		switch (wParam)
		{
		case TIMER_MAIN:
		{
			static bool jitter_detected = false;
			static int jitter_count = 0;
			static bool using_fallback = false;
			static int fallback_count = 0;
			DWORD time_now = GetTickCount();
			LASTINPUTINFO lii;
			lii.cbSize = sizeof(LASTINPUTINFO);
			GetLastInputInfo(&lii);
			
			// prioritise Raw Input...
			if (time_of_last_raw_input + 1000 >= time_of_last_input_info)
			{
				using_fallback = false;
				fallback_count = 0;
				last_input = time_of_last_raw_input;
			}
			// but also use GetLastInputInfo() to determine user input (fallback method)
			else if((lii.dwTime > time_of_last_raw_input) && (lii.dwTime - time_of_last_raw_input > 1000 ))
			{
				// discover and discard controllers that are jittery (i.e. that send constant updates)
				if ((user_is_idle && abs((int)(lii.dwTime - time_of_last_input_info - (DWORD)TIMER_MAIN_DELAY_WHEN_IDLE)) <= 20)
					|| (!user_is_idle && abs((int)(lii.dwTime - time_of_last_input_info - (DWORD)TIMER_MAIN_DELAY_WHEN_BUSY)) <= 20))
					jitter_count++;
				else
					jitter_count--;
				if (jitter_count > 10)
				{
					jitter_count = 10;
					jitter_detected = true;
				}
				else if (jitter_count < 1)
				{
					jitter_count = 1;
					jitter_detected = false;
				}

				if (time_now - lii.dwTime <= (DWORD)(user_is_idle ? TIMER_MAIN_DELAY_WHEN_IDLE : TIMER_MAIN_DELAY_WHEN_BUSY))
					fallback_count++;
				if (time_now - lii.dwTime > 5000)
					fallback_count--;
				if (fallback_count < 0)
				{
					fallback_count = 0;
					using_fallback = false;
				}
				if (fallback_count > 10)
				{
					fallback_count = 10;
					using_fallback = true;
					if (!jitter_detected)
						last_input = time_now;
				}
			}

			time_of_last_input_info = lii.dwTime;
			

			if (daemon_is_visible)
			{
				DWORD time = (time_now - last_input) / 1000;
				std::wstring ago = std::to_wstring(time);
				SendMessage(GetDlgItem(hWnd, IDC_EDIT3), WM_SETTEXT, 0, (WPARAM)ago.c_str());
				ShowWindow(GetDlgItem(hWnd, IDC_STATIC_FALLBACK), using_fallback ? SW_SHOW : SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_STATIC_JITTER), (using_fallback && jitter_detected) ? SW_SHOW : SW_HIDE);
			}
			if (user_is_idle)
			{
				if (time_now - last_input <= TIMER_MAIN_DELAY_WHEN_IDLE * 2) // was there user input during the interval?
				{
					manual_user_idle_mode = 0;
					SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
					SetTimer(hWnd, TIMER_IDLE, Prefs.user_idle_mode_delay_ * 60 * 1000, (TIMERPROC)NULL);
					user_is_idle = false;
					communicateWithService(L"userbusy", false);
				}
			}
			else
			{
				if (time_now - last_input <= (TIMER_MAIN_DELAY_WHEN_BUSY+10)) // was there user input during the interval?
				{
					SetTimer(hWnd, TIMER_IDLE, Prefs.user_idle_mode_delay_ * 60 * 1000, (TIMERPROC)NULL);
				}
			}
			return 0;
		}break;

		case TIMER_IDLE:
		{
			if (manual_user_idle_mode == 0)
			{
				if(Prefs.user_idle_mode_disable_while_fullscreen_
					|| Prefs.user_idle_mode_process_control_)
					workaroundFalseFullscreenWindows();
				std::wstring log_message;
				if (last_input != 0 && UIM::preventUIM(Prefs, log_message))
				{
					std::wstring l = L"UIM block: ";
					l += log_message;
					log(l);
					return 0;
				}
			}
			user_is_idle = true;
			SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_IDLE, (TIMERPROC)NULL);
			communicateWithService(L"useridle", false);
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
			communicateWithService(L"remote_connect", false);
		}break;
		case REMOTE_DISCONNECT:
		{
			KillTimer(hWnd, (UINT_PTR)REMOTE_DISCONNECT);
			communicateWithService(L"remote_disconnect", false);
		}break;
		case TIMER_TOPOLOGY_COLLECTION:
		{
			KillTimer(hWnd, (UINT_PTR)TIMER_TOPOLOGY_COLLECTION);
			PostMessage(hWnd, APP_DISPLAYCHANGE, NULL, NULL);
		}break;
		case TIMER_VERSIONCHECK:
		{		
			TCHAR buffer[MAX_PATH] = { 0 };
			KillTimer(hWnd, TIMER_VERSIONCHECK);
			if (Prefs.updater_mode_ == PREFS_UPDATER_OFF)
				break;
			time_of_last_version_check = time(0);
			if (Prefs.updater_mode_ == PREFS_UPDATER_SILENT)
				tools::startScheduledTask(TASK_FOLDER, TASK_UPDATER_SILENT);
			else
			{
				if(GetModuleFileName(NULL, buffer, MAX_PATH))
				{
					std::wstring path = buffer;
					std::wstring::size_type pos = path.find_last_of(L"\\/");
					path = path.substr(0, pos + 1);
					std::wstring exe = path;
					exe += L"LGTVupdater.exe";
					ShellExecute(NULL, L"open", exe.c_str(), L"-versioncheck", path.c_str(), SW_NORMAL);
				}
			}
		}break;
		case TIMER_TOPOLOGY:
		{
			KillTimer(hWnd, TIMER_TOPOLOGY);
			if (!Prefs.topology_support_)
				break;

			int result = verifyTopology();

			if (result == TOPOLOGY_OK)
			{
				PostMessage(hWnd, APP_DISPLAYCHANGE, NULL, NULL);
			}
			else if (result == TOPOLOGY_UNDETERMINED)
			{
				log(L"Warning! No active devices detected when verifying Windows Monitor Topology.");
				communicateWithService(L"topology undetermined", false);
				//				Prefs.AdhereTopology = false;
			}
			else if (result == TOPOLOGY_OK_DISABLE)
			{
				Prefs.topology_support_ = false;
			}
			else if (result == TOPOLOGY_ERROR)
			{
				Prefs.topology_support_ = false;
				communicateWithService(L"topology invalid",false);

				std::wstring s = L"A change to the system has invalidated the monitor topology configuration and the feature has been disabled. "
					"Please run the configuration guide in the global options to ensure correct operation.";
				log(s);
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
			user_is_idle = false;
			time_of_last_raw_input = GetTickCount();
			log(L"Suspending system.");
			RawInput_ClearCache();
			return true;
		}break;
		case PBT_APMRESUMEAUTOMATIC:
		{
			user_is_idle = false;
			time_of_last_raw_input = GetTickCount();
			if (Prefs.user_idle_mode_)
			{
				SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
				SetTimer(hWnd, TIMER_IDLE, Prefs.user_idle_mode_delay_ * 60 * 1000, (TIMERPROC)NULL);
			}
			log(L"Resuming system.");
			RawInput_ClearCache();
			if (Prefs.updater_mode_ != PREFS_UPDATER_OFF)
				if (time(0) - time_of_last_version_check >= (24 * 60 * 60)) // check only once per 24h
					SetTimer(h_main_wnd, TIMER_VERSIONCHECK, TIMER_VERSIONCHECK_DELAY, (TIMERPROC)NULL);
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
						user_is_idle = false;
						time_of_last_raw_input = GetTickCount();
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
	case APP_DISPLAYCHANGE:
	{
		time_of_last_topology_change = time(0);
		checkDisplayTopology();
	}break;
	case WM_DISPLAYCHANGE:
	{
		if (Prefs.topology_support_)
		{
			time_t now = time(0);
			if (now - time_of_last_topology_change > 10)
			{
				PostMessage(hWnd, APP_DISPLAYCHANGE, NULL, NULL);
			}
			else
			{
				time_of_last_topology_change = now;
				SetTimer(hWnd, TIMER_TOPOLOGY_COLLECTION, TIMER_TOPOLOGY_COLLECTION_DELAY, (TIMERPROC)NULL);
			}
		}
		
	}break;
	case WM_DEVICECHANGE:
	{
		//clear the raw input cache whenever a controller was connected/disconnected.
		RawInput_ClearCache();

		if (Prefs.remote_streaming_host_support_ && lParam && wParam)
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
		if(!wParam)
			break;
		if (wParam == WTS_REMOTE_CONNECT)
		{
			remoteStreamingEvent(REMOTE_RDP_CONNECTED);
		}
		else if (wParam == WTS_REMOTE_DISCONNECT)
		{
			remoteStreamingEvent(REMOTE_RDP_NOT_CONNECTED);
		}
	}break;
	case WM_SIZE: {
		if (!lParam)
			break;
		int width = LOWORD(lParam);
		int height = HIWORD(lParam);

		// Adjust the size of the edit control
		SetWindowPos(GetDlgItem(hWnd, IDC_EDIT), nullptr, 0, 0, width, height - 64, SWP_NOZORDER);
		SetWindowPos(GetDlgItem(hWnd, IDC_EDIT3), nullptr, 10, height - 28, 120, 26, SWP_NOZORDER);
		SetWindowPos(GetDlgItem(hWnd, IDC_BUTTON2), nullptr, width - 100, height - 28, 90, 26, SWP_NOZORDER);
		SetWindowPos(GetDlgItem(hWnd, IDC_TESTBUTTON), nullptr, width - 100, height - 60, 90, 26, SWP_NOZORDER);
		SetWindowPos(GetDlgItem(hWnd, IDC_DESC), nullptr, 10, height - 60, 120, 32, SWP_NOZORDER);
		SetWindowPos(GetDlgItem(hWnd, IDC_STATIC_FALLBACK), nullptr, 140, height - 60, 160, 20, SWP_NOZORDER);
		SetWindowPos(GetDlgItem(hWnd, IDC_STATIC_JITTER), nullptr, 140, height - 35, 160, 20, SWP_NOZORDER);
		break;
	}
	case WM_GETMINMAXINFO: {
		MINMAXINFO* pMinMaxInfo = (MINMAXINFO*)lParam;

		// Set the minimum width and height
		pMinMaxInfo->ptMinTrackSize.x = 430;
		pMinMaxInfo->ptMinTrackSize.y = 250;
		return 0; // Return 0 to indicate the message was handled
	}
	case WM_COPYDATA:
	{

		if (!lParam)
			return true;

		//thread safe
		while (!copydata_mutex_.try_lock())
			Sleep(COPYDATA_MUTEX_WAIT);

		std::wstring received_command_line;
		COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)lParam;

		if (pcds->dwData == NOTIFY_NEW_COMMANDLINE)
		{
			if (pcds->cbData == 0)
			{
				SetForegroundWindow(hWnd);
				copydata_mutex_.unlock();
				return true;
			}
			received_command_line = (WCHAR*)pcds->lpData;
			if (received_command_line.size() == 0)
			{
				copydata_mutex_.unlock();
				return true;
			}
			communicateWithService(received_command_line, true);
			Sleep(20);
		}
		copydata_mutex_.unlock();
		return true;
	}break;
	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		//		if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_ENABLE))
		{
			SetBkMode(hdcStatic, TRANSPARENT);
		}
		return(INT_PTR)h_backbrush;
	}break;
	case WM_PAINT:
	{
		RECT rc = { 0 };
		GetClientRect(hWnd, &rc);
		PAINTSTRUCT ps;
		//		PAINTSTRUCT psPaint;
		InvalidateRect(hWnd, NULL, false);
		InvalidateRect(GetDlgItem(hWnd, IDC_DESC), NULL, true);
		InvalidateRect(GetDlgItem(hWnd, IDC_TESTBUTTON), NULL, true);
		//		GetClientRect(hWnd, &rc);
		int width = rc.right;
		int height = rc.bottom;
		HDC hdc = BeginPaint(hWnd, &ps);
		HDC backbuffDC = CreateCompatibleDC(hdc);
		HBITMAP backbuffer = CreateCompatibleBitmap(hdc, width, height);
		int savedDC = SaveDC(backbuffDC);
		SelectObject(backbuffDC, backbuffer);

		FillRect(backbuffDC, &rc, (HBRUSH)h_backbrush);

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
		if (Prefs.shutdown_timing_ == PREFS_SHUTDOWN_TIMING_DELAYED)
		{
			Sleep(7000);
			ShutdownBlockReasonDestroy(hWnd);
		}
	}break;
	case WM_DESTROY:
	{
		PostMessage(HWND_BROADCAST, custom_updater_close_message, NULL, (LPARAM)1);
		PostQuitMessage(0);
	}break;
	default:
		return false;
	}
	return true;
}

//   If the application is already running, tell other process to exit
void closeExistingProcess(void)
{
	PostMessage(HWND_BROADCAST, custom_daemon_close_message, NULL, NULL);
	return;
}
//   communicate with service via IPC
void communicateWithService(std::wstring sData, bool is_command_line)
{
	if (sData.size() == 0)
		return;
	std::wstring message;
	if(!is_command_line)
	{
		message = L"-daemon ";
		if (session_id.size() > 0)
		{
			message += tools::widen(session_id);
			message += L" ";
		}
		else
		{
			message += L"unknown_id";
			message += L" ";
		}
	}
	message += sData;

	int max_wait = 1000;
	while (!p_pipe_client->send(message) && max_wait > 0)
	{
		Sleep(25);
		max_wait -= 25;
	}
	if (max_wait <= 0)
		log(L"Failed to connect to named pipe. Service may be stopped.");
	else
		log(message);
	return;
}

void log(std::wstring input)
{
	time_t rawtime;
	struct tm timeinfo;
	char buffer[80];

	if (!daemon_is_visible)
		return;

	std::time(&rawtime);
	localtime_s(&timeinfo, &rawtime);

	strftime(buffer, 80, "%a %H:%M:%S > ", &timeinfo);
	puts(buffer);

	std::wstring logmess = tools::widen(buffer);
	logmess += input;
	logmess += L"\r\n";

	int text_len = (int)SendMessage(GetDlgItem(h_main_wnd, IDC_EDIT), WM_GETTEXTLENGTH, 0, 0);
	if (text_len > 20000)
	{
		std::wstring buf = tools::getWndText(GetDlgItem(h_main_wnd, IDC_EDIT));
		buf = buf.substr(10000);
		size_t find = buf.find_first_of('\n');
		if (find != std::wstring::npos)
			buf = buf.substr(find + 1);
		SetWindowText(GetDlgItem(h_main_wnd, IDC_EDIT), buf.c_str());
		text_len = (int)SendMessage(GetDlgItem(h_main_wnd, IDC_EDIT), WM_GETTEXTLENGTH, 0, 0);
	}
	SendMessage(GetDlgItem(h_main_wnd, IDC_EDIT), EM_SETSEL, (WPARAM)text_len, (LPARAM)text_len);
	SendMessage(GetDlgItem(h_main_wnd, IDC_EDIT), EM_REPLACESEL, FALSE, (LPARAM)logmess.c_str());
	InvalidateRect(GetDlgItem(h_main_wnd, IDC_EDIT), NULL, true);
	UpdateWindow(GetDlgItem(h_main_wnd, IDC_EDIT));
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
	if (!GetMonitorInfo(hMonitor, &mi))
		return false;
	if(GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, &requiredModes) != ERROR_SUCCESS)
	{
		targets->clear();
		return false;
	}

	paths.resize(requiredPaths);
	modes.resize(requiredModes);

	if(QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, paths.data(), &requiredModes, modes.data(), NULL) != ERROR_SUCCESS)
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

		if(DisplayConfigGetDeviceInfo(&sourceName.header) == ERROR_SUCCESS)
		{
			if (wcscmp(mi.szDevice, sourceName.viewGdiDeviceName) == 0)
			{
				DISPLAYCONFIG_TARGET_DEVICE_NAME name;
				name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
				name.header.size = sizeof(name);
				name.header.adapterId = p.sourceInfo.adapterId;
				name.header.id = p.targetInfo.id;
				if(DisplayConfigGetDeviceInfo(&name.header) == ERROR_SUCCESS)
				{
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
		}
	}
	return true;
}
bool checkDisplayTopology(void)
{
	std::wstring top;
	top = L"topology state ";
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
						top += tools::widen(dev.id);
						top += L" ";
					}
				}
			}
		}
	}
	communicateWithService(top, false);
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
	bool bStreamingProcessFound = false;
	bool bSunshineSvcProcessFound = false;
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
		// look for currently running known streaming processes
		if (Prefs.remote_streaming_host_support_ && !bStreamingProcessFound)
			for (auto& w : Remote.stream_proc_list)
				if (!_tcsicmp(entry.szExeFile, w.c_str()))
					bStreamingProcessFound = true;

		// look for currently running sushine service process
		if (Prefs.remote_streaming_host_support_ && !bSunshineSvcProcessFound)
			if (!_tcsicmp(entry.szExeFile, SUNSHINE_FILE_SVC))
				bSunshineSvcProcessFound = true;
	} while (!(bStreamingProcessFound && bSunshineSvcProcessFound) && Process32Next(snapshot, &entry));

	CloseHandle(snapshot);

	// was sunshine service found currently running?
	if (bSunshineSvcProcessFound && (Remote.Sunshine_Log_Files.size() > 0))
	{
		int index = 0;
//		std::vector<DWORD> val(Remote.Sunshine_Log_Files.size());
		for(auto& logfile : Remote.Sunshine_Log_Files)
		{
			std::filesystem::path p(logfile);
			uintmax_t Size = std::filesystem::file_size(p);
			if (Size != Remote.Sunshine_Log_Sizes[index])
			{
				Remote.Sunshine_Log_Sizes[index] = Size;
				if (index == 0)
				{
					ReturnValue |= sunshine_CheckLog(logfile);
				}
				else if (index == 1)
				{
					DWORD val = sunshine_CheckLog(logfile);
					if(val == REMOTE_SUNSHINE_CONNECTED)
						ReturnValue |= REMOTE_APOLLO_CONNECTED;
					else if (val == REMOTE_SUNSHINE_NOT_CONNECTED)
						ReturnValue |= REMOTE_APOLLO_NOT_CONNECTED;
				}
			}
			index++;
		}
	}
	else
		ReturnValue |= REMOTE_SUNSHINE_NOT_CONNECTED;

	// was a remote streaming process currently running?
	ReturnValue |= bStreamingProcessFound ? REMOTE_STEAM_CONNECTED : REMOTE_STEAM_NOT_CONNECTED;

	return ReturnValue;
}
bool isFullscreenApplicationRunning(void)
{
//	workaroundFalseFullscreenWindows();

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
		std::wstring window_name = tools::getWndText(hWnd);
		transform(window_name.begin(), window_name.end(), window_name.begin(), ::tolower);
		if (window_name.find(L"nvidia geforce overlay") != std::wstring::npos)
		{
			if (GetProp(hWnd, nonRude) == NULL)
			{
				if (SetProp(hWnd, nonRude, INVALID_HANDLE_VALUE))
				{
					DWORD recipients = BSM_APPLICATIONS;
					if (BroadcastSystemMessage(BSF_POSTMESSAGE | BSF_IGNORECURRENTTASK, &recipients, custom_shellhook_message, HSHELL_UNDOCUMENTED_FULLSCREEN_EXIT, (LPARAM)hWnd) < 0)
					{
						log(L"BroadcastSystemMessage() failed");
					}
					else {
						communicateWithService(L"gfe", false);
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
		bool bCurrentlyConnected = Remote.bRemoteCurrentStatusSteam || Remote.bRemoteCurrentStatusNvidia || Remote.bRemoteCurrentStatusRDP || Remote.bRemoteCurrentStatusSunshine || Remote.bRemoteCurrentStatusApollo;

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
		if (dwType & REMOTE_APOLLO_CONNECTED)
		{
			if (!Remote.bRemoteCurrentStatusApollo)
			{
				log(L"Apollo gamestream connected.");
				Remote.bRemoteCurrentStatusApollo = true;
			}
		}
		else if (dwType & REMOTE_APOLLO_NOT_CONNECTED)
		{
			if (Remote.bRemoteCurrentStatusApollo)
			{
				log(L"Apollo gamestream disconnected.");
				Remote.bRemoteCurrentStatusApollo = false;
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
		bool bConnect = Remote.bRemoteCurrentStatusSteam || Remote.bRemoteCurrentStatusNvidia || Remote.bRemoteCurrentStatusRDP || Remote.bRemoteCurrentStatusSunshine || Remote.bRemoteCurrentStatusApollo;

		if (bCurrentlyConnected && !bConnect)
			SetTimer(h_main_wnd, (UINT_PTR)REMOTE_DISCONNECT, 1000, (TIMERPROC)NULL);
		else if (!bCurrentlyConnected && bConnect)
			SetTimer(h_main_wnd, (UINT_PTR)REMOTE_CONNECT, TIMER_REMOTE_DELAY, (TIMERPROC)NULL);
	}
	return;
}

DWORD sunshine_CheckLog(std::string input_file)
{
	std::string log;
	std::ifstream t(input_file);
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

std::vector<std::string> sunshine_GetLogFiles()
{
	std::string Sunshine_Config;
	std::string configuration_path;
	std::string configuration_text;
	std::vector<std::string> log_files(Remote.sunshine_list.size(), "");

	for(int i = 0; i<Remote.sunshine_list.size();i++)
	{
		HKEY hKey = NULL;
		LPCSTR pszSubkey = Remote.sunshine_list[i].c_str();
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
						log(L"Logging need to be at minimum on level \"info\" in Sunshine/Apollo.");
						continue;
					}

					s = sunshine_GetConfVal(configuration_text, "log_path");
					if (s == "") //DEFAULT
					{
						std::string log_file = configuration_path;
						log_file += SUNSHINE_FILE_LOG;
						log_files[i] = log_file;
					}
					else
					{
						std::filesystem::path log_p(s);
						std::filesystem::current_path(configuration_path);
						std::filesystem::path log_abs(std::filesystem::absolute(log_p));
						std::string log_file = log_abs.string();
						log_files[i] = log_file;
					}
				}
			}
		}
	}
	return log_files;
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

bool RawInput_AddToCache(const std::wstring& devicePath) {
	HANDLE hDevice = CreateFile(devicePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE) 
		return false;

	PHIDP_PREPARSED_DATA ppd = nullptr;
	if (!HidD_GetPreparsedData(hDevice, &ppd)) {
		CloseHandle(hDevice);
		return false;
	}

	HIDP_CAPS caps;
	if (HidP_GetCaps(ppd, &caps) != HIDP_STATUS_SUCCESS) {
		HidD_FreePreparsedData(ppd);
		CloseHandle(hDevice);
		return false;
	}
	g_device_cache[devicePath] = { hDevice, ppd, caps };
	return true;
}

void RawInput_ClearCache() {
	try
	{
		for (auto it = g_device_cache.begin(); it != g_device_cache.end(); ++it) {
			if (it->second.ppd)
				HidD_FreePreparsedData(it->second.ppd);
			if (it->second.hDevice)
				CloseHandle(it->second.hDevice);
			it->second.axis_value.clear();
			it->second.caps = { 0 };
		}
	}
	catch (...)
	{
		log(L"Error in RawInput_ClearCache()");
	}
	g_device_cache.clear();
}
