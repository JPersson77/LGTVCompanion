#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <SDKDDKVer.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <Windows.h>
#include <system_error>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <powerbase.h>
#include <Shlobj_core.h>
#include <winevt.h>
#include <thread>
#include <mutex>
#include <AccCtrl.h>
#include <sddl.h>
#include <Aclapi.h>
#include <WinSock2.h>
#include <dbt.h>
#include <boost/optional.hpp>
#include <boost/utility/string_view.hpp>
#include <Iphlpapi.h>
#include "../Common/Common.h"
#include "Handshake.h"

#pragma comment(lib, "Powrprof.lib")
#pragma comment(lib, "Wevtapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#define SVCNAME						    L"LGTVsvc"
#define SVCDISPLAYNAME				    L"LGTV Companion Service"
#define SERVICE_PORT                    "3000"
#define SERVICE_PORT_SSL                "3001"
#define SERVICE_DEPENDENCIES		    L"Dhcp\0Dnscache\0LanmanServer\0\0"
#define SERVICE_ACCOUNT				    NULL //L"NT AUTHORITY\\LocalService"
#define MUTEX_WAIT          		    10   // thread wait in ms
#define THREAD_WAIT          		    1    // wait to spawn new thread (seconds)
#define DIMMED_OFF_DELAY_WAIT           20    // delay after a screen dim request
#define MAX_RECORD_BUFFER_SIZE          0x10000  // 64K
#define SYSTEM_EVENT_SHUTDOWN           1
#define SYSTEM_EVENT_REBOOT             2
#define SYSTEM_EVENT_RESUME             3
#define SYSTEM_EVENT_RESUMEAUTO         4
#define SYSTEM_EVENT_SUSPEND            5
#define SYSTEM_EVENT_DISPLAYOFF         6
#define SYSTEM_EVENT_DISPLAYON          7
#define SYSTEM_EVENT_UNSURE             8
#define SYSTEM_EVENT_FORCEON            9
#define SYSTEM_EVENT_FORCEOFF           10
#define SYSTEM_EVENT_DISPLAYDIMMED      11
#define SYSTEM_EVENT_FORCESCREENOFF     12
#define SYSTEM_EVENT_USERBUSY           13
#define SYSTEM_EVENT_USERIDLE           14
#define SYSTEM_EVENT_FORCESETHDMI       15
#define SYSTEM_EVENT_BOOT               16
#define SYSTEM_EVENT_TOPOLOGY           17
#define APP_IPC_DAEMON_TOPOLOGY         12

class CSession {
public:
	CSession(jpersson77::settings::DEVICE*);
	~CSession();
	void Run();
	void RemoteHostConnected();
	void RemoteHostDisconnected();
	void Stop();
	void SystemEvent(DWORD, int param = 0);
	jpersson77::settings::DEVICE GetParams();
	bool IsBusy();
	void SetTopology(bool);
	bool GetTopology();
	std::string DeviceID;
private:
	time_t ScreenDimmedRequestTime = 0;
	bool ActivePowerState = false;
	bool ThreadedOpDisplayOn = false;
	bool ThreadedOpDisplayOff = false;
	bool ThreadedOpDisplaySetHdmiInput = false;
	bool AdhereTopology = false;
	bool TopologyEnabled = false;
	time_t ThreadedOpDisplayOffTime = 0;
	void TurnOnDisplay(bool SendWOL);
	void TurnOffDisplay(bool forced, bool dimmed, bool blankscreen);
	void SetDisplayHdmiInput(int HdmiInput);
	bool bRemoteHostConnected = false;
	jpersson77::settings::DEVICE   Parameters;
};

VOID			SvcInstall(void);
VOID			SvcUninstall(void);
DWORD			SvcCtrlHandler(DWORD, DWORD, LPVOID, LPVOID);
VOID WINAPI		SvcMain(DWORD, LPTSTR*);
VOID			ReportSvcStatus(DWORD, DWORD, DWORD);
VOID			SvcReportEvent(WORD, std::wstring);
std::wstring	GetErrorMessage(DWORD dwErrorCode);
bool			SetSessionKey(std::string Key, std::string deviceid);
bool			DispatchSystemPowerEvent(DWORD);
void			Log(std::string);
DWORD WINAPI	SubCallback(EVT_SUBSCRIBE_NOTIFY_ACTION Action, PVOID UserContext, EVT_HANDLE Event);
void			DisplayPowerOnThread(jpersson77::settings::DEVICE*, bool*, int, bool);
void			DisplayPowerOffThread(jpersson77::settings::DEVICE*, bool*, bool, bool);
void			SetDisplayHdmiInputThread(jpersson77::settings::DEVICE*, bool*, int, int);
void			IPCThread(void);
void			WOLthread(jpersson77::settings::DEVICE*, bool*, int);
std::vector<std::string> GetOwnIP(void);
void			InitSessions(void);