#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <SDKDDKVer.h>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <Windows.h>
#include <system_error>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <powerbase.h>
#include <powrprof.h>
#include <Shlobj_core.h>
#include <winevt.h>
#include <thread>
#include <mutex>
#include <AccCtrl.h>
#include <sddl.h>
#include <Aclapi.h>
#include <WinSock2.h>
#include <dbt.h>


#include "nlohmann/json.hpp"
#include "Handshake.h"

#pragma comment(lib, "Powrprof.lib")
#pragma comment(lib, "Wevtapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Advapi32.lib")


#define APPNAME						    L"LGTV Companion" 
#define APPVERSION					    L"1.5.0" 
#define SVCNAME						    L"LGTVsvc" 
#define SVCDISPLAYNAME				    L"LGTV Companion Service" 
#define SERVICE_PORT                    "3000"

#define JSON_PREFS_NODE                 "LGTV Companion"
#define JSON_EVENT_RESTART_STRINGS      "LocalEventLogRestartString"
#define JSON_EVENT_SHUTDOWN_STRINGS     "LocalEventLogShutdownString"
#define JSON_VERSION                    "Version"
#define JSON_LOGGING                    "ExtendedLog"
#define JSON_PWRONTIMEOUT               "PowerOnTimeOut"
#define DEFAULT_RESTART                 {"restart"}
#define DEFAULT_SHUTDOWN                {"shutdown","power off"}
#define JSON_IDLEBLANK                  "BlankWhenIdle"
#define JSON_IDLEBLANKDELAY             "BlankWhenIdleDelay"

#define SERVICE_DEPENDENCIES		    L"Dhcp\0Dnscache\0LanmanServer\0\0" 
#define SERVICE_ACCOUNT				    NULL //L"NT AUTHORITY\\LocalService" 
#define MUTEX_WAIT          		    10   // thread wait in ms
#define THREAD_WAIT          		    1    // wait to spawn new thread (seconds)
#define DIMMED_OFF_DELAY_WAIT           20    // delay after a screen dim request
#define MAX_RECORD_BUFFER_SIZE          0x10000  // 64K
#define SYSTEM_EVENT_SHUTDOWN           0x0001
#define SYSTEM_EVENT_REBOOT             0x0002
#define SYSTEM_EVENT_RESUME             0x0004
#define SYSTEM_EVENT_RESUMEAUTO         0x0008
#define SYSTEM_EVENT_SUSPEND            0x0010
#define SYSTEM_EVENT_DISPLAYOFF         0x0020
#define SYSTEM_EVENT_DISPLAYON          0x0040
#define SYSTEM_EVENT_UNSURE             0x0080
#define SYSTEM_EVENT_FORCEON            0x0100
#define SYSTEM_EVENT_FORCEOFF           0x0200
#define SYSTEM_EVENT_DISPLAYDIMMED      0x0400
#define SYSTEM_EVENT_FORCESCREENOFF     0x0800
#define SYSTEM_EVENT_USERBUSY           0x1000
#define SYSTEM_EVENT_USERIDLE           0x2000

#define APP_CMDLINE_ON                  1
#define APP_CMDLINE_OFF                 2
#define APP_CMDLINE_AUTOENABLE          3
#define APP_CMDLINE_AUTODISABLE         4
#define APP_CMDLINE_SCREENON            5
#define APP_CMDLINE_SCREENOFF           6
#define APP_IPC_DAEMON                  7

#define         WOL_NETWORKBROADCAST            1
#define         WOL_IPSEND                      2
#define         WOL_SUBNETBROADCAST             3

#define         WOL_DEFAULTSUBNET               L"255.255.255.0"


#define         PIPENAME                        TEXT("\\\\.\\pipe\\LGTVyolo")

#define         NEWRELEASELINK                  L"https://github.com/JPersson77/LGTVCompanion/releases"


#ifndef SERVICE_CONTROL_USERMODEREBOOT	//not defined. Unclear diz
#define SERVICE_CONTROL_USERMODEREBOOT	0x00000040
#endif
#ifndef SERVICE_ACCEPT_USERMODEREBOOT
#define SERVICE_ACCEPT_USERMODEREBOOT	0x00000800
#endif


struct PREFS {
    std::vector<std::string> EventLogRestartString = DEFAULT_RESTART;
    std::vector<std::string> EventLogShutdownString = DEFAULT_SHUTDOWN;
    bool Logging = false;
    int version = 2;
    int PowerOnTimeout = 40;
    bool BlankWhenIdle = false;
    int BlankScreenWhenIdleDelay = 10;
};

struct SESSIONPARAMETERS {
    std::string DeviceId;
    std::string IP;
    std::vector<std::string> MAC;
    std::string SessionKey;
    int PowerOnTimeout = 40;
    std::string Name;
    bool Enabled = true;
    std::string Subnet;
    int WOLtype = 1;
    bool HDMIinputcontrol = false;
    int OnlyTurnOffIfCurrentHDMIInputNumberIs = 1;
    bool BlankWhenIdle = false;
    int BlankScreenWhenIdleDelay = 10;
};

class CSession {

public:
    CSession( SESSIONPARAMETERS*);
    ~CSession();
    void Run();
    void Stop();
    void SystemEvent(DWORD);
    SESSIONPARAMETERS GetParams();
    bool IsBusy();
private:
    time_t ScreenDimmedRequestTime = 0;
    bool ThreadedOpDisplayOn = false;    
    bool ThreadedOpDisplayOff = false;
    time_t ThreadedOpDisplayOffTime = 0;
    void TurnOnDisplay(void);
    void TurnOffDisplay(bool forced, bool dimmed, bool blankscreen);
    SESSIONPARAMETERS   Parameters;
};


VOID SvcInstall(void);
VOID SvcUninstall(void);
DWORD SvcCtrlHandler(DWORD, DWORD, LPVOID, LPVOID);

VOID WINAPI SvcMain(DWORD, LPTSTR*);

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcReportEvent(WORD, std::wstring); 
std::wstring GetErrorMessage(DWORD dwErrorCode);
bool SetSessionKey(std::string Key, std::string deviceid);
bool ReadConfigFile();
void InitDeviceSessions();
bool  DispatchSystemPowerEvent(DWORD);
void Log(std::string);
DWORD WINAPI SubCallback(EVT_SUBSCRIBE_NOTIFY_ACTION Action, PVOID UserContext, EVT_HANDLE Event);
std::wstring widen(std::string);
std::string narrow(std::wstring);
void DisplayPowerOnThread(SESSIONPARAMETERS *, bool *, int);
void DisplayPowerOffThread(SESSIONPARAMETERS*, bool *, bool, bool);
void IPCThread(void);
void WOLthread(SESSIONPARAMETERS*, bool*, int);
std::vector<std::string> stringsplit(std::string str, std::string token);
std::vector<std::string> GetOwnIP(void);
