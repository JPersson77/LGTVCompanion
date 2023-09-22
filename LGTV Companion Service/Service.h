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
#include <filesystem>
#include <powerbase.h>
#include <Shlobj_core.h>
#include <tlhelp32.h>
#include <winevt.h>
#include <thread>
#include <atomic>
#include <AccCtrl.h>
#include <sddl.h>
#include <Aclapi.h>
#include <WinSock2.h>
#include <dbt.h>
#include <boost/optional.hpp>
#include <boost/utility/string_view.hpp>
#include <Iphlpapi.h>
#include "../Common/Common.h"
#include "../Common/LgApi.h"

#pragma comment(lib, "Powrprof.lib")
#pragma comment(lib, "Wevtapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Iphlpapi.lib")

// global definitions for the service
#define SVCNAME											L"LGTVsvc"
#define SVCDISPLAYNAME									L"LGTV Companion Service"
#define SERVICE_DEPENDENCIES							L"Dhcp\0Dnscache\0LanmanServer\0\0"
#define SERVICE_ACCOUNT									NULL		
#define THREAD_WAIT          							1			// wait to spawn new thread (seconds)
#define MAX_RECORD_BUFFER_SIZE							0x10000		// 64K



// The CSession object represents a WebOS-device and manages network communication respectively
class CSession {
public:
	CSession(jpersson77::settings::DEVICE&, jpersson77::settings::PREFERENCES&);
	~CSession();
	//													Query whether the CSession object is busy, i.e whether a network communication thread is currently running
	bool												IsBusy();

	//													Terminate threads
	void												Terminate(void);

	//													Power on and unblank screen. 
	void												PowerOnDisplay(void);

	//													Power off or blank screen. Arg1: force regardless of HDMI-input selected, Arg2: should the screen be blanked only
	void												PowerOffDisplay(bool = false, bool = false);

	//													Send a basic request to the device. Arg1: uri, Arg2: payload, Arg3: log message, Arg2: repeat until success 
	void												SendRequest(std::string, std::string, std::string, bool = false);

	//													Send a system setting luna request to the device. Arg1: Setting, Arg2: value, Arg3: category, Arg4: log message 
	void												SendLunaSystemSettingRequest(std::string, std::string, std::string, std::string);

	//													Send a raw json luna request to the device. Arg1: luna uri, Arg2: payload json, Arg3: log message 
	void												SendLunaRawRequest(std::string, std::string, std::string);

	//													Send a button input request to the device. Arg1: button, Arg2: log message 
	void												SendButtonRequest(std::string, std::string);

	bool												isManagedAutomatically = false;		// Is automatic management enabled for the Session/device?
//	bool												isPoweredOn = false;				// Is the device powered on (according to latest power event)
	bool												TopologyEnabled = false;			// Is the device enabled in the windows monitor topology??
	jpersson77::settings::DEVICE						Parameters;							// CSession parameters
	jpersson77::settings::PREFERENCES					Prefs;								// Copy of global prefs
	std::string											DeviceID_lowercase;
	std::string											Name_lowercase;
private:
//														Arg1: URI, Arg2: Payload (optional)
	nlohmann::json										CreateRequestJson(std::string, std::string = "");
//														Arg1: setting, Arg2: value, Arg3: category
	nlohmann::json										CreateLunaSystemSettingJson(std::string, std::string, std::string = "picture");
//														Arg1: settings and values json, Arg2: category
	nlohmann::json										CreateRawLunaJson(std::string, std::string);

	void												Thread_DisplayOn();
	//													Arg1: Force display off regardless of HDMI input setting, Arg2: Blank screen instead of powering off
	void												Thread_DisplayOff(bool, bool);
	//													Arg1: json, Arg2: log message, Arg3: repeat until success
	void												Thread_SendRequest(std::string, std::string, bool, bool);
	//													Arg1: json, Arg2: log message, Arg3: repeat until success
	void												Thread_ButtonRequest(std::string, std::string);	
	void												Thread_WOL();
	
	//													Thread counters
	std::atomic_bool									Thread_DisplayOn_isRunning = { false };
	std::atomic_bool									Thread_DisplayOff_isRunning = { false };
	std::atomic_int										Thread_SendRequest_isRunning = { 0 };

	std::atomic_bool									bTerminateThread = { false };

	std::string											sHandshake;
	int                                                 requestId = 1;
	time_t												lastOnTime = 0;
	time_t												lastOffTime = 0;
};

// CSessionManager manages CSession objects and Events, i. e system power-events, events from the daemon as well as events triggered by command line
class CSessionManager {
public:
	CSessionManager();
	~CSessionManager();
	
	//													Supply a new Event to the Session Manager. Arg1: pointer to an EVENT struct. Return value: string of matching devices
	void												NewEvent(EVENT&);

	//													Add a CSession object to the Session Manager. Arg1; pointer to a DEVICE (settings) struct
	void												AddSession(jpersson77::settings::DEVICE&);

	//													Copy the global preferences into the Session Manager. Arg1: pointer to a PREFERENCES struct
	void												SetPreferences(jpersson77::settings::PREFERENCES&);

	//													Wait for all CSession spawned threads to exit
	void												TerminateAndWait(void);

	//													Enable automatic management for Session(s). Arg 1: pointer to a vector of device ID's
	void												Enable(std::vector<std::string>&);
	
	//													Disable automatic management for Session(s).Arg 1: pointer to a vector of device ID's
	void												Disable(std::vector<std::string>&);

	//													Set global presence of remote client (i.e Steam, Moonlight, RDP etc). Arg1: boolean to indicate that remote client has connected
	void												RemoteClientIsConnected(bool);

	//													Set Topology presence/status for CSession objects. Arg 1: pointer to a vector of device ID's
	std::string											SetTopology(std::vector<std::string>&);

	//													Are the displays globally requested to be powered on / active? 
	bool												GetWindowsPowerStatus(void);

	//													Validate the vector of device names. Arg 1: pointer to a vector of device ID's
	std::string											ValidateDevices(std::vector<std::string>&);

	//													Validate the argument(s) of the Event. Arg1: argument, Arg2: valid arguments
	std::string											ValidateArgument(std::string, std::string);

private:

	//													Core logic for processing Events and controlling CSession objects
	void												ProcessEvent(EVENT&, CSession&);

	std::vector <CSession*>								Sessions;												// Vector with pointers to the CSession objects
	jpersson77::settings::PREFERENCES					Prefs;													// Copy of the global preferences
	bool												bDisplaysCurrentlyPoweredOnByWindows = false;			// Current power status requested by Windows
	bool												bRemoteClientIsConnected = false;						// Indicates that a remote client is connected
};

// Service.cpp
bool													SvcInstall(void);
bool													SvcUninstall(void);
DWORD													SvcCtrlHandler(DWORD, DWORD, LPVOID, LPVOID);
VOID WINAPI												SvcMain(DWORD, LPTSTR*);
VOID													ReportSvcStatus(DWORD, DWORD, DWORD);
VOID													SvcReportEvent(WORD, std::wstring);
std::wstring											GetErrorMessage(DWORD dwErrorCode);
bool													SetSessionKey(std::string Key, std::string deviceid);
void													Log(std::string);
DWORD WINAPI											SubCallback(EVT_SUBSCRIBE_NOTIFY_ACTION Action, PVOID UserContext, EVT_HANDLE Event);
void													InitSessions(void);
void													NamedPipeCallback(std::wstring message);
void													SendToNamedPipe(DWORD);

//														Create a global power event for the session manager. Arg1: Event type. 
void													CreateEvent_system(DWORD); 
//														Create a power event for the session manager. Arg1: Event type, Arg2: vector of the devices 
void													CreateEvent_system(DWORD, std::vector<std::string>); 
//														Create a URI event with optional payload for the Session Manager. Arg1: vector of the devices, Arg2: URI, Arg3: Json payload (optional), Arg4: Log message for the spawned thread (optional). 
void													CreateEvent_request(std::vector<std::string>, std::string, std::string = "", std::string = "");
//														Create a basic luna event for the Session Manager. Arg1: vector of the devices, Arg2: Category, Arg3: Setting, Arg4: Value, Arg5: Log message for the spawned thread (optional).
void													CreateEvent_luna_set_system_setting_basic(std::vector<std::string>, std::string, std::string, std::string, std::string = "");
//														Create a luna event with generic payload for the Session Manager. Arg1: vector of the devices, Arg2: Category, Arg3: Json payload, Arg4: Log message for the spawned thread (optional).
void													CreateEvent_luna_set_system_setting_payload(std::vector<std::string>, std::string, std::string, std::string = "");
//														Create a luna set device info event for the Session Manager. Arg1: vector of the devices, Arg2: input, Arg3: icon, Arg4: label, Arg5: Log message for the spawned thread (optional).
void													CreateEvent_luna_set_device_info(std::vector<std::string>, std::string, std::string, std::string, std::string = "");
//														Create a button-press event for the Session Manager. Arg1: vector of the devices, Arg2: button, Arg3: Log message for the spawned thread (optional).
void													CreateEvent_button(std::vector<std::string>, std::string, std::string = "");
std::vector<std::string>								_Devices(std::vector<std::string>, int);
std::vector<std::string>								Extract_Commands(std::string);
bool													IsScreenSaverActive(void);