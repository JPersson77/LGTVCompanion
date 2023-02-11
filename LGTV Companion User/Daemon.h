#pragma once
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "User32.lib")

#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define _WINSOCK_DEPRECATED_NO_WARNINGS

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
#include <algorithm>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <filesystem>
#include <wintoast/wintoastlib.h>
#include "resource.h"
#include "../Common/Common.h"

#define			APPNAME_SHORT							L"LGTVdaemon"
#define			APPNAME_FULL							L"LGTV Companion Daemon"
#define			NOTIFY_NEW_PROCESS						1
#define         TIMER_MAIN								18
#define         TIMER_IDLE								19
#define         TIMER_TOPOLOGY							20
#define         TIMER_CHECK_PROCESSES					21
#define         TIMER_MAIN_DELAY_WHEN_BUSY				5000
#define         TIMER_MAIN_DELAY_WHEN_IDLE				100
#define         TIMER_REMOTE_DELAY						10000
#define         TIMER_TOPOLOGY_DELAY					8000
#define         TIMER_CHECK_PROCESSES_DELAY				5000
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
