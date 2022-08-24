#pragma once
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "Wtsapi32.lib")

#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
this is my change

//#include "targetver.h"
this
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define _WINSOCK_DEPRECATED_NO_WARNINGS

// Windows Header Files
#include <windows.h>
// C RunTime Header Files
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

#include "resource.h"
#include "nlohmann/json.hpp"
#include "WinToast-1.2.0/wintoastlib.h"

#define			APPNAME_SHORT					L"LGTVdaemon"
#define			APP_PATH					    L"LGTV Companion"
#define			APPNAME_FULL					L"LGTV Companion Daemon"
#define         APP_VERSION                     L"1.7.0"
#define			WINDOW_CLASS_UNIQUE				L"YOLOx0x0x0181818"
#define			NOTIFY_NEW_PROCESS			    1

#define         TIMER_MAIN                      18
#define         TIMER_IDLE                      19
#define         TIMER_RDP                       20

#define         TIMER_MAIN_DELAY_WHEN_BUSY      2000
#define         TIMER_MAIN_DELAY_WHEN_IDLE      100
#define         TIMER_RDP_DELAY                 10000

#define         APP_NEW_VERSION                 WM_USER+9

#define         JSON_PREFS_NODE                 "LGTV Companion"
#define         JSON_VERSION                    "Version"
#define         JSON_LOGGING                    "ExtendedLog"
#define         JSON_AUTOUPDATE                 "AutoUpdate"
#define         JSON_IDLEBLANK                  "BlankWhenIdle"
#define         JSON_IDLEBLANKDELAY             "BlankWhenIdleDelay"

#define         PIPENAME                        TEXT("\\\\.\\pipe\\LGTVyolo")
#define         NEWRELEASELINK                  L"https://github.com/JPersson77/LGTVCompanion/releases"
#define         VERSIONCHECKLINK                L"https://api.github.com/repos/JPersson77/LGTVCompanion/releases"
#define         DONATELINK                      L"https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=jorgen.persson@gmail.com&lc=US&item_name=Friendly+gift+for+the+development+of+LGTV+Companion&no_note=0&cn=&currency_code=EUR&bn=PP-DonationsBF:btn_donateCC_LG.gif:NonHosted"

struct PREFS {
	bool AutoUpdate = false;
	bool BlankScreenWhenIdle = false;
	int BlankScreenWhenIdleDelay = 10;
	bool Logging = false;
	int version = 2;
	bool ToastInitialised = false;
	bool DisableSendingViaIPC = false;
};

class WinToastHandler : public WinToastLib::IWinToastHandler
{
public:
	//    WinToastHandler() {}
		// Public interfaces
	void toastActivated() const override {
		//       ShellExecute(0, 0, NEWRELEASELINK, 0, 0, SW_SHOW);
	}
	void toastActivated(int actionIndex) const override {
		ShellExecute(0, 0, NEWRELEASELINK, 0, 0, SW_SHOW);
	}
	void toastDismissed(WinToastDismissalReason state) const override {}
	void toastFailed() const override {}
private:
};

// Forward declarations of functions included in this code module:
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
bool                MessageExistingProcess(void);
bool				ReadConfigFile();
std::wstring        widen(std::string);
std::string         narrow(std::wstring);
std::vector<std::string> stringsplit(std::string, std::string);
void                CommunicateWithService(std::string);
void                VersionCheckThread(HWND);
void                Log(std::wstring input);