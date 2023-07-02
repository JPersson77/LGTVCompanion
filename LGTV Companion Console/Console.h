#pragma once
//#pragma comment(lib, "urlmon.lib")
//#pragma comment(lib, "Wtsapi32.lib")
//#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")


#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <WinSock2.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include "../Common/Common.h"
#include "../Common/LgApi.h"
#include <netioapi.h>
/*
#include <windows.h>
#include <stdlib.h>
#include <string>
#include <Shlobj_core.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <shellapi.h>
#include <strsafe.h>
#include <urlmon.h>
#include <wtsapi32.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <hidusage.h>
#include <initguid.h>
#include <usbiodef.h>
#include <Dbt.h>
#include <lmcons.h>
#include <algorithm>
#include <WinSock2.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <filesystem>
#include <wintoast/wintoastlib.h>
#include "resource.h"
#include "../Common/Common.h"
*/

#define			APPNAME_SHORT							"LGTVcli"
#define			APPNAME_FULL							"LGTV Companion CLI"

#define			JSON_OUTPUT_DEFAULT						"JSON_OUTPUT_DEFAULT"
#define			JSON_OUTPUT_FRIENDLY					"JSON_OUTPUT_FRIENDLY"
#define			JSON_OUTPUT_FIELD						"JSON_OUTPUT_FIELD"
/*
#define			NOTIFY_NEW_PROCESS						1
#define         TIMER_MAIN								18
#define         TIMER_IDLE								19
#define         TIMER_TOPOLOGY							20
#define         TIMER_CHECK_PROCESSES					21
#define         TIMER_TOPOLOGY_COLLECTION				22
#define         TIMER_MAIN_DELAY_WHEN_BUSY				100
#define         TIMER_MAIN_DELAY_WHEN_IDLE				100
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
*/

/*
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

// Forward declarations of functions included in this code module:
LRESULT CALLBACK		WndProc(HWND, UINT, WPARAM, LPARAM);

bool					MessageExistingProcess(void);
void					CommunicateWithService(std::string);
void					VersionCheckThread(HWND);
void					Log(std::wstring input);
std::vector<jpersson77::settings::DISPLAY_INFO> QueryDisplays();
static BOOL	CALLBACK	meproc(HMONITOR hMon, HDC hdc, LPRECT lprcMonitor, LPARAM pData);
bool					CheckDisplayTopology(void);
int						VerifyTopology();
DWORD					CheckRemoteStreamingProcesses(void);
bool					FullscreenApplicationRunning(void);
void					RemoteStreamingEvent(DWORD dwType);
void					WorkaroundFalseFullscreenWindows(void);
static BOOL	CALLBACK	EnumWindowsProc(HWND hWnd, LPARAM lParam);
DWORD					Sunshine_CheckLog(void);
std::string				Sunshine_GetConfVal(std::string, std::string);
std::string				Sunshine_GetLogFile();
void					NamedPipeCallback(std::wstring);
*/

std::string												ProcessCommand(std::vector<std::string>&);
std::vector<std::string>								_Devices(std::vector<std::string>, int);
std::string												ValidateArgument(std::string, std::string);
//														Create a power event for the session manager. Arg1: Event type, Arg2: vector of the devices 
std::string												CreateEvent_system(DWORD, std::vector<std::string>);
//														Create a URI event with optional payload for the Session Manager. Arg1: vector of the devices, Arg2: URI, Arg3: Json payload (optional). 
std::string												CreateEvent_request(std::vector<std::string>, std::string, std::string = "");
//														Create a basic luna event for the Session Manager. Arg1: vector of the devices, Arg2: Category, Arg3: Setting, Arg4: Value.
std::string												CreateEvent_luna_set_system_setting_basic(std::vector<std::string>, std::string, std::string, std::string);
//														Create a luna event with generic payload for the Session Manager. Arg1: vector of the devices, Arg2: Category, Arg3: Json payload.
std::string												CreateEvent_luna_set_system_setting_payload(std::vector<std::string>, std::string, std::string);
//														Create a luna set device info event for the Session Manager. Arg1: vector of the devices, Arg2: input, Arg3: icon, Arg4: label.
std::string												CreateEvent_luna_set_device_info(std::vector<std::string>, std::string, std::string, std::string);
//														Create a button-press event for the Session Manager. Arg1: vector of the devices, Arg2: button.
std::string												CreateEvent_button(std::vector<std::string>, std::string);
std::string												ProcessEvent(EVENT&);
nlohmann::json											CreateRequestJson(std::string, std::string = "");
nlohmann::json											CreateLunaSystemSettingJson(std::string, std::string, std::string);
nlohmann::json											CreateRawLunaJson(std::string, nlohmann::json);
nlohmann::json											PowerOnDevice(jpersson77::settings::DEVICE);
nlohmann::json											SendRequest(jpersson77::settings::DEVICE, nlohmann::json, bool);
nlohmann::json											SendButtonRequest(jpersson77::settings::DEVICE, std::string);
void													iterateJsonKeys(const nlohmann::json&, std::string );
void													Thread_WOL(jpersson77::settings::DEVICE);
std::string												helpText(void);