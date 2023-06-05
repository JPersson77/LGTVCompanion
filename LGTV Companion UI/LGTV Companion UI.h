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

#include <windows.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <string>
#include <algorithm>
#include <Shlobj_core.h>
#include <fstream>
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
#include <winsock2.h>
#include <urlmon.h>
#include "../Common/Common.h"
#include "resource.h"

#define			APPNAME_SHORT					L"LGTVcomp"
#define			NOTIFY_NEW_COMMANDLINE			1
#define			COLOR_STATIC					0x00555555
#define			COLOR_RED						0x00000099
#define			COLOR_GREEN						0x00009900
#define			COLOR_BLUE						0x00772222
#define         APP_MESSAGE_ADD                 WM_USER+1
#define         APP_MESSAGE_MANAGE              WM_USER+2
#define         APP_MESSAGE_SCAN                WM_USER+3
#define         APP_MESSAGE_TEST                WM_USER+4
#define         APP_MESSAGE_TURNON              WM_USER+5
#define         APP_MESSAGE_TURNOFF             WM_USER+6
#define         APP_MESSAGE_REMOVE              WM_USER+7
#define         APP_MESSAGE_APPLY               WM_USER+8
#define         APP_NEW_VERSION                 WM_USER+9
#define         APP_TOP_PHASE_1		            WM_USER+10
#define         APP_TOP_PHASE_2	                WM_USER+11
#define         APP_TOP_PHASE_3                 WM_USER+12
#define         APP_TOP_NEXT_DISPLAY            WM_USER+13
#define         APP_LISTBOX_EDIT	            WM_USER+14
#define         APP_LISTBOX_ADD					WM_USER+15
#define         APP_LISTBOX_DELETE	            WM_USER+16
#define         APP_LISTBOX_REDRAW				WM_USER+17
#define         APP_USER_IDLE_ON                WM_USER+20
#define         APP_USER_IDLE_OFF               WM_USER+21

#define         DEVICEWINDOW_TITLE_ADD          L"Add device"
#define         DEVICEWINDOW_TITLE_MANAGE       L"Configure device"

// Forward declarations of functions included in this code module:
LRESULT CALLBACK			WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK			DeviceWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK			OptionsWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK			ConfigureTopologyWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK			UserIdleConfWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK			WhitelistConfWndProc(HWND, UINT, WPARAM, LPARAM);

bool						MessageExistingProcess(std::wstring);
void						SvcReportEvent(WORD, std::wstring);
void						CommunicateWithService(std::wstring);
void						VersionCheckThread(HWND);
static BOOL CALLBACK		meproc(HMONITOR hMon, HDC hdc, LPRECT lprcMonitor, LPARAM pData);
std::vector<jpersson77::settings::DISPLAY_INFO> QueryDisplays();
bool						MessageDaemon(std::wstring);
void						NamedPipeCallback(std::wstring message);

