#pragma once
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "urlmon.lib")

#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define _WINSOCK_DEPRECATED_NO_WARNINGS

// Windows Header Files
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <string>
#include <algorithm>
#include <Shlobj_core.h>
#include <fstream>
#include <sstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <CommCtrl.h>
#include <winevt.h>
#include <thread>
#include <shellapi.h>
#include <initguid.h>   
#include <functiondiscoverykeys.h>
#include <SetupAPI.h>
#include <devpkey.h>
#include <strsafe.h>
//#include <winsock.h>
#include <winsock2.h>
#include <urlmon.h>

#include "resource.h"
#include "nlohmann/json.hpp"


#define			APPNAME_SHORT					L"LGTVcomp"
#define			APPNAME_FULL					L"LGTV Companion"
#define         APP_VERSION                     L"1.4.3"
#define			WINDOW_CLASS_UNIQUE				L"YOLOx0x0x0181818"
#define			NOTIFY_NEW_COMMANDLINE			1

#define         JSON_PREFS_NODE                 "LGTV Companion"
#define         JSON_EVENT_RESTART_STRINGS      "LocalEventLogRestartString"
#define         JSON_EVENT_SHUTDOWN_STRINGS     "LocalEventLogShutdownString"
#define         JSON_VERSION                    "Version"
#define         JSON_LOGGING                    "ExtendedLog"
#define         JSON_AUTOUPDATE                 "AutoUpdate"
#define         JSON_PWRONTIMEOUT               "PowerOnTimeOut"
#define         DEFAULT_RESTART                 {"restart"}
#define         DEFAULT_SHUTDOWN                {"shutdown","power off"}

#define         APP_MESSAGE_ADD                 WM_USER+1
#define         APP_MESSAGE_MANAGE              WM_USER+2
#define         APP_MESSAGE_SCAN                WM_USER+3
#define         APP_MESSAGE_TEST                WM_USER+4
#define         APP_MESSAGE_TURNON              WM_USER+5
#define         APP_MESSAGE_TURNOFF             WM_USER+6
#define         APP_MESSAGE_REMOVE              WM_USER+7
#define         APP_MESSAGE_APPLY               WM_USER+8
#define         APP_NEW_VERSION                 WM_USER+9

#define         APP_CMDLINE_ON                  1
#define         APP_CMDLINE_OFF                 2
#define         APP_CMDLINE_AUTOENABLE          3
#define         APP_CMDLINE_AUTODISABLE         4

#define         WOL_NETWORKBROADCAST            1
#define         WOL_IPSEND                      2
#define         WOL_SUBNETBROADCAST             3

#define         WOL_DEFAULTSUBNET               L"255.255.255.0"

#define         DEVICEWINDOW_TITLE_ADD          L"Add device"
#define         DEVICEWINDOW_TITLE_MANAGE       L"Configure device"

#define         PIPENAME                        TEXT("\\\\.\\pipe\\LGTVyolo")
#define         NEWRELEASELINK                  L"https://github.com/JPersson77/LGTVCompanion/releases"
#define         VERSIONCHECKLINK                L"https://api.github.com/repos/JPersson77/LGTVCompanion/releases"

struct PREFS {
    std::vector<std::string> EventLogRestartString = DEFAULT_RESTART;
    std::vector<std::string> EventLogShutdownString = DEFAULT_SHUTDOWN;
    bool Logging = false;
    int version = 1;
    int PowerOnTimeout = 40;
    bool AutoUpdate = false;
};
struct SESSIONPARAMETERS {
    std::string DeviceId;
    std::string IP;
    std::vector<std::string> MAC;
    std::string SessionKey;
    std::string Name;
 //   bool PowerAuto = true;
 //   bool AwayAuto = true;
    bool Enabled = true;
    int PowerOnTimeout = 40;
    int WOLtype = WOL_NETWORKBROADCAST;
    std::string Subnet;
    bool HDMIinputcontrol = false;
    int OnlyTurnOffIfCurrentHDMIInputNumberIs = 1;

};

// Forward declarations of functions included in this code module:
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    DeviceWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    OptionsWndProc(HWND, UINT, WPARAM, LPARAM);
bool                MessageExistingProcess(std::wstring);
bool				ReadConfigFile();
void                ReadDeviceConfig();
std::wstring        widen(std::string);
std::string         narrow(std::wstring);
void                SvcReportEvent(WORD, std::wstring);
std::wstring        GetWndText(HWND);
std::vector<std::string> stringsplit(std::string, std::string);
// bool                ParseCommandline(std::wstring);

void                CommunicateWithService(std::string);
void                WriteConfigFile(void);
std::vector<std::string> GetOwnIP(void);
void                VersionCheckThread(HWND);
