// See LGTV Companion UI.cpp for additional details

#include "Service.h"
#include <powrprof.h>

namespace						common = jpersson77::common;
namespace						settings = jpersson77::settings;
namespace						ipc = jpersson77::ipc;

//GLOBALS
SERVICE_STATUS					gSvcStatus;
SERVICE_STATUS_HANDLE			gSvcStatusHandle;
HANDLE							ghSvcStopEvent = NULL;
DEV_BROADCAST_DEVICEINTERFACE	filter;
HDEVNOTIFY						gDevEvents;
HPOWERNOTIFY					gPs1;
DWORD							EventCallbackStatus = NULL;
WSADATA							WSAData;
std::mutex						log_mutex;
std::vector<std::string>		HostIPs;
bool							bIdleLog = true;
ipc::PipeServer*				pPipe;
nlohmann::json					lg_api_commands_json;
std::string						lg_api_buttons;
settings::Preferences			Settings;
CSessionManager					SessionManager;

wchar_t sddl[] = L"D:"
L"(A;;CCLCSWRPWPDTLOCRRC;;;SY)"           // default permissions for local system
L"(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;BA)"   // default permissions for administrators
L"(A;;CCLCSWLOCRRC;;;AU)"                 // default permissions for authenticated users
L"(A;;CCLCSWRPWPDTLOCRRC;;;PU)"           // default permissions for power users
L"(A;;RPWP;;;IU)"                         // added permission: start/stop service for interactive users
;

//   Application entrypoint
int wmain(int argc, wchar_t* argv[])
{
	if (lstrcmpi(argv[1], L"-install") == 0)
	{
		return SvcInstall() ? 0: 1;
	}
	if (lstrcmpi(argv[1], L"-uninstall") == 0)
	{
		return SvcUninstall() ? 0: 1;
	}

	SERVICE_TABLE_ENTRY DispatchTable[] =
	{
		{ (LPWSTR)SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
		{ NULL, NULL }
	};

	// This call returns when the service has stopped.
	if (!StartServiceCtrlDispatcher(DispatchTable))
		SvcReportEvent(EVENTLOG_ERROR_TYPE, L"StartServiceCtrlDispatcher");
	return 0;
}
//   Install service in the SCM database
bool SvcInstall()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	WCHAR szPath[MAX_PATH];

	if (!GetModuleFileNameW(NULL, szPath, MAX_PATH))
	{
		printf("Failed to install service (%d).\n", GetLastError());
		return false;
	}

	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database
		SC_MANAGER_ALL_ACCESS);  // full access rights

	if (!schSCManager)
	{
		printf("Failed to install service. Please run again as ADMIN. OpenSCManager failed (%d).\n", GetLastError());
		return false;
	}

	// Create the service

	schService = CreateService(
		schSCManager,              // SCM database
		SVCNAME,                   // name of service
		SVCDISPLAYNAME,            // service name to display
		SERVICE_ALL_ACCESS | WRITE_DAC,        // desired access
		SERVICE_WIN32_OWN_PROCESS, // service type
		SERVICE_AUTO_START,        // start type
		SERVICE_ERROR_NORMAL,      // error control type
		szPath,                    // path to service's binary
		NULL,                      // no load ordering group
		NULL,                      // no tag identifier
		SERVICE_DEPENDENCIES,      // dependencies
		SERVICE_ACCOUNT,           // LocalSystem account
		NULL);                     // no password

	if (schService == NULL)
	{
		printf("Failed to install service. CreateService failed (%d).\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return false;
	}
	else
	{
		PSECURITY_DESCRIPTOR sd;

		if (!ConvertStringSecurityDescriptorToSecurityDescriptor(sddl, SDDL_REVISION_1, &sd, NULL))
		{
			printf("Failed to set security descriptor (%d).\n", GetLastError());
		}

		if (!SetServiceObjectSecurity(schService, DACL_SECURITY_INFORMATION, sd))
		{
			printf("Failed to set security descriptor(%d).\n", GetLastError());
		}

		if (!StartService(
			schService,  // handle to service
			0,           // number of arguments
			NULL))       // no arguments
		{
			printf("Installation of service was successful but the service failed to start (%d).\n", GetLastError());
			return false;
		}
		else
		{
			printf("Service successfully installed and started.\n");
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return true;
		}
	}
	return false;
}
//   Uninstall service in the SCM database
bool SvcUninstall()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	SERVICE_STATUS_PROCESS ssp;
	DWORD dwStartTime = (DWORD)GetTickCount64();
	DWORD dwBytesNeeded;
	DWORD dwTimeout = 15000; // 15-second time-out

	// Get a handle to the SCM database.
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database
		SC_MANAGER_ALL_ACCESS);  // full access rights

	if (NULL == schSCManager)
	{
		printf("Failed to uninstall service. Please run again as ADMIN. OpenSCManager failed (%d).\n", GetLastError());
		return false;
	}

	// Get a handle to the service.

	schService = OpenService(
		schSCManager,       // SCM database
		SVCNAME,          // name of service
		SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);

	if (!schService)
	{
		printf("Failed to uninstall service. OpenService failed (%d).\n", GetLastError());
		CloseServiceHandle(schSCManager);
		return false;
	}

	if (!QueryServiceStatusEx(
		schService,
		SC_STATUS_PROCESS_INFO,
		(LPBYTE)&ssp,
		sizeof(SERVICE_STATUS_PROCESS),
		&dwBytesNeeded))
	{
		printf("QueryServiceStatusEx failed (%d).\n", GetLastError());
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return false;
	}

	ControlService(
		schService,
		SERVICE_CONTROL_STOP,
		(LPSERVICE_STATUS)&ssp);

	// Wait for the service to stop.
	while (ssp.dwCurrentState != SERVICE_STOPPED)
	{
		Sleep(ssp.dwWaitHint);
		if (QueryServiceStatusEx(
			schService,
			SC_STATUS_PROCESS_INFO,
			(LPBYTE)&ssp,
			sizeof(SERVICE_STATUS_PROCESS),
			&dwBytesNeeded))
		{
			if (ssp.dwCurrentState == SERVICE_STOPPED)
				break;

			if ((DWORD)GetTickCount64() - dwStartTime > dwTimeout)
			{
				printf("Failed to uninstall service. Timed out when stoppping service.\n");
				CloseServiceHandle(schService);
				CloseServiceHandle(schSCManager);
				return false;
			}
		}
		else
		{
			printf("Failed to uninstall service. Unable to query service status.\n");
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return false;
		}
	}

	if (!DeleteService(schService))
	{
		printf("Failed to uninstall service. DeleteService failed (%d)\n", GetLastError());
		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return false;
	}
	else
	{
		printf("Service uninstalled successfully\n");

		CloseServiceHandle(schService);
		CloseServiceHandle(schSCManager);
		return true;
	}
	return false;
}
//   Service entrypoint
VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
	//Register seervice handler
	gSvcStatusHandle = RegisterServiceCtrlHandlerExW(SVCNAME, SvcCtrlHandler, NULL);
	if (!gSvcStatusHandle)
	{
		SvcReportEvent(EVENTLOG_ERROR_TYPE, L"RegisterServiceCtrlHandler");
		return;
	}
	
	//spawn PipeServer for managing intra process communication
	ipc::PipeServer Pipe(PIPENAME, NamedPipeCallback);
	pPipe = &Pipe;
	
	// These SERVICE_STATUS members remain as set here
	gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	gSvcStatus.dwServiceSpecificExitCode = 0;

	// Report SERVICE_START_PENDING status to the SCM
	ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

	// need a stop event
	ghSvcStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!ghSvcStopEvent)
	{
		SvcReportEvent(EVENTLOG_ERROR_TYPE, L"CreateEvent");
		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		return;
	}

	// starting up winsock
	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
	{
		SvcReportEvent(EVENTLOG_ERROR_TYPE, L"WSAStartup");
		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		return;
	}

	//load the config file and initiate the device control sessions
	try {
		Settings.Initialize();
	}
	catch (std::exception const& e)
	{
		std::wstring s = L"ERROR! Failed to read the configuration file. LGTV service is terminating. Error: ";
		s += common::widen(e.what());
		SvcReportEvent(EVENTLOG_ERROR_TYPE, s);
		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		WSACleanup();
		return;
	}

	SessionManager.SetPreferences(Settings.Prefs);

	//Initialise CSession objects
	InitSessions();

	try {
		nlohmann::json lg_api_buttons_json;
		std::string json_str =
		#include "lg_api_commands.h"
			;
		lg_api_commands_json = nlohmann::json::parse(json_str);

		json_str =
		#include "lg_api_buttons.h"
			;
		lg_api_buttons_json = nlohmann::json::parse(json_str);
		nlohmann::json j = lg_api_buttons_json["Buttons"];
		if (!j.empty() && j.size() > 0)
		{
			for (auto& str : j.items())
			{
				lg_api_buttons += str.value().get<std::string>();
				lg_api_buttons += " ";
			}
		}	
	}
	catch (std::exception const& e)
	{
		std::wstring s = L"ERROR! Failed to initialize LG API JSON. LGTV service is terminating. Error: ";
		s += common::widen(e.what());
		SvcReportEvent(EVENTLOG_ERROR_TYPE, s);
		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		WSACleanup();
		return;
	}

	//get local host IPs, if possible at this time
	HostIPs = common::GetOwnIP();
	if (HostIPs.size() > 0)
	{
		std::string logmsg;
		if (HostIPs.size() == 1)
			logmsg = "Host IP detected: ";
		else
			logmsg = "Host IPs detected: ";

		for (int index = 0; index < HostIPs.size(); index++)
		{
			logmsg += HostIPs[index];
			if (index != HostIPs.size() - 1)
				logmsg += ", ";
		}
		Log(logmsg);
	}

	//subscribe to EventID 1074, which contain information about whether user/system triggered restart or shutdown
	EVT_HANDLE hSub = NULL;
	hSub = EvtSubscribe(NULL,                           //local computer
		NULL,                                           // no signalevent
		L"System",                                      //channel path
		L"Event/System[EventID=1074]",                  //query
		NULL,                                           //bookmark
		NULL,                                           //Context
		(EVT_SUBSCRIBE_CALLBACK)SubCallback,            //callback
		EvtSubscribeToFutureEvents);
	

	//make sure the process is shutdown as early as possible
	if (SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY))
		Log("Setting shutdown parameter level 0x100");
	else
		Log("Could not set shutdown parameter level");

	ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

	SvcReportEvent(EVENTLOG_INFORMATION_TYPE, L"The service has started.");

	CreateEvent_system(EVENT_SYSTEM_BOOT);

	//register to receive power notifications
	gPs1 = RegisterPowerSettingNotification(gSvcStatusHandle, &(GUID_CONSOLE_DISPLAY_STATE), DEVICE_NOTIFY_SERVICE_HANDLE);

	// Wait until service stops
	WaitForSingleObject(ghSvcStopEvent, INFINITE);

	Pipe.Terminate();
	Sleep(1000);
	UnregisterPowerSettingNotification(gPs1);
	WSACleanup();
	EvtClose(hSub);
	SessionManager.TerminateAndWait();
	Log("The service terminated.\n");
	SvcReportEvent(EVENTLOG_INFORMATION_TYPE, L"The service has ended.");
	ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
	return;
}
//   Sets the current service status and reports it to the SCM.
VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;

	// Fill in the SERVICE_STATUS structure.

	gSvcStatus.dwCurrentState = dwCurrentState;
	gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
	gSvcStatus.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		gSvcStatus.dwControlsAccepted = 0;
	else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PRESHUTDOWN | SERVICE_ACCEPT_POWEREVENT; //SERVICE_ACCEPT_PRESHUTDOWN | // | SERVICE_ACCEPT_USERMODEREBOOT; //does not work

	if ((dwCurrentState == SERVICE_RUNNING) ||
		(dwCurrentState == SERVICE_STOPPED))
		gSvcStatus.dwCheckPoint = 0;
	else gSvcStatus.dwCheckPoint = dwCheckPoint++;

	// Report the status of the service to the SCM.
	SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}
//   Called by SCM whenever a control code is sent to the service using the ControlService function. dwCtrl - control code
DWORD  SvcCtrlHandler(DWORD dwCtrl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
	switch (dwCtrl)
	{
	case SERVICE_CONTROL_STOP:
	{
		ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 20000);

		SessionManager.TerminateAndWait();

		// Signal the service to stop.
		SetEvent(ghSvcStopEvent);
		ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
	}	break;

	case SERVICE_CONTROL_POWEREVENT:
	{
		switch (dwEventType)
		{
		case PBT_APMRESUMEAUTOMATIC:
		{
			EventCallbackStatus = NULL;
			Log("** System resumed from low power state (Automatic).");
			CreateEvent_system(EVENT_SYSTEM_RESUMEAUTO);
		}	break;
		case PBT_APMRESUMESUSPEND:
		{
			EventCallbackStatus = NULL;
			Log("** System resumed from low power state.");
			CreateEvent_system(EVENT_SYSTEM_RESUME);
		}	break;
		case PBT_APMSUSPEND:
		{
			if (EventCallbackStatus == EVENT_SYSTEM_REBOOT)
			{
				Log("** System is restarting.");
				CreateEvent_system(EVENT_SYSTEM_REBOOT);
			}
			else if (EventCallbackStatus == EVENT_SYSTEM_SHUTDOWN)
			{
				Log("** System is shutting down (low power mode).");
				CreateEvent_system(EVENT_SYSTEM_SUSPEND);
			}
			else if (EventCallbackStatus == EVENT_SYSTEM_UNSURE)
			{
				Log("WARNING! Unable to determine if system is shutting down or restarting. Please check 'additional settings' in the UI.");
				CreateEvent_system(EVENT_SYSTEM_UNSURE);
			}
			else
			{
				Log("** System is suspending to a low power state.");
				CreateEvent_system(EVENT_SYSTEM_SUSPEND);
			}
		}	break;
		case PBT_POWERSETTINGCHANGE:
		{
			if (lpEventData)
			{
				POWERBROADCAST_SETTING* PBS = NULL;
				std::string text;
				PBS = (POWERBROADCAST_SETTING*)lpEventData;
				if (PBS->PowerSetting == GUID_CONSOLE_DISPLAY_STATE)
				{
					if (PBS->Data[0] == 0)
					{
						SetThreadExecutionState(ES_AWAYMODE_REQUIRED | ES_CONTINUOUS);
						Log("** System requests displays OFF.");
						CreateEvent_system(EVENT_SYSTEM_DISPLAYOFF);
						SetThreadExecutionState(ES_CONTINUOUS);
					}
					else if (PBS->Data[0] == 2)
					{
						SetThreadExecutionState(ES_AWAYMODE_REQUIRED | ES_CONTINUOUS);
						Log("** System requests displays OFF(DIMMED).");
						CreateEvent_system(EVENT_SYSTEM_DISPLAYDIMMED);
						SetThreadExecutionState(ES_CONTINUOUS);
					}
					else
					{
						Log("** System requests displays ON.");
						CreateEvent_system(EVENT_SYSTEM_DISPLAYON);
					}
				}
				else
				{
					char guid_cstr[39];
					GUID guid = PBS->PowerSetting;

					snprintf(guid_cstr, sizeof(guid_cstr),
						"PBT_POWERSETTINGCHANGE unknown GUID:: {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
						guid.Data1, guid.Data2, guid.Data3,
						guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
						guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
					text = guid_cstr;
					Log(text);
				}
			}
		}	break;

		default:break;
		}
	}	break;
	case SERVICE_CONTROL_PRESHUTDOWN:
	{
		if (EventCallbackStatus == EVENT_SYSTEM_REBOOT)
		{
			Log("** System is restarting.");
			CreateEvent_system(EVENT_SYSTEM_REBOOT);
		}
		else if (EventCallbackStatus == EVENT_SYSTEM_SHUTDOWN)
		{
			Log("** System is shutting down.");
			CreateEvent_system(EVENT_SYSTEM_SHUTDOWN);
		}
		else if (EventCallbackStatus == EVENT_SYSTEM_UNSURE)
		{
			Log("WARNING! Unable to determine if system is shutting down or restarting. Please check 'additional settings in the UI.");
			CreateEvent_system(EVENT_SYSTEM_UNSURE);
		}
		else
		{
			//This does happen sometimes, probably for timing reasons when shutting down the system.
			Log("WARNING! The application did not receive an Event Subscription Callback prior to system shutting down. Unable to determine if system is shutting down or restarting.");
			CreateEvent_system(EVENT_SYSTEM_UNSURE);
		}

//		Sleep(5000);
		ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 20000);
		
		SessionManager.TerminateAndWait();

		// Signal the service to stop.
		SetEvent(ghSvcStopEvent);
		ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
	}	break;

	case SERVICE_CONTROL_INTERROGATE:
	{
		Log("SERVICE_CONTROL_INTERROGATE");
	}	break;
	default:
		return ERROR_CALL_NOT_IMPLEMENTED;
	}
	return NO_ERROR;
}
//   Add an event in the event log. Type can be: EVENTLOG_SUCCESS, EVENTLOG_ERROR_TYPE, EVENTLOG_INFORMATION_TYPE
VOID SvcReportEvent(WORD Type, std::wstring string)
{
	HANDLE hEventSource;
	LPCTSTR lpszStrings[2];

	hEventSource = RegisterEventSource(NULL, SVCNAME);

	if (hEventSource)
	{
		std::wstring s;
		switch (Type)
		{
		case EVENTLOG_ERROR_TYPE:
			s = string;
			s += L" failed with error: ";
			s += GetErrorMessage(GetLastError());
			break;
		case EVENTLOG_SUCCESS:
			s = string;
			s += L" succeeded.";
			break;
		case EVENTLOG_INFORMATION_TYPE:
		default:
			s = string;
			break;
		}

		lpszStrings[0] = SVCNAME;
		lpszStrings[1] = s.c_str();

		ReportEvent(hEventSource,        // event log handle
			Type,               // event type
			0,                   // event category
			0,                   // event identifier
			NULL,                // no security identifier
			2,                   // size of lpszStrings array
			0,                   // no binary data
			lpszStrings,         // array of strings
			NULL);               // no binary data

		DeregisterEventSource(hEventSource);
	}
}
//   Format the error message to readable form
std::wstring GetErrorMessage(DWORD dwErrorCode)
{
	if (dwErrorCode)
	{
		WCHAR* lpMsgBuf = NULL;
		DWORD bufLen = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			dwErrorCode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			lpMsgBuf,
			0, NULL);
		if (bufLen)
		{
			LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
			std::wstring result(lpMsgStr, lpMsgStr + bufLen);
			LocalFree(lpMsgBuf);
			return result;
		}
	}
	return L"";
}
//   Write key to configuration file when a thread has received a pairing key. Thread safe
bool SetSessionKey(std::string Key, std::string deviceid)
{
	if (Key.size() > 0 && deviceid.size() > 0)
	{
		std::wstring path = Settings.Prefs.DataPath;
		path += CONFIG_FILE;
		nlohmann::json jsonPrefs;

		//thread safe
		while (!log_mutex.try_lock())
			Sleep(MUTEX_WAIT);
		std::ifstream i(path.c_str());
		if (i.is_open())
		{
			i >> jsonPrefs;
			i.close();
			jsonPrefs[deviceid][JSON_DEVICE_SESSIONKEY] = Key;
		}

		std::ofstream o(path.c_str());
		if (o.is_open())
		{
			o << std::setw(4) << jsonPrefs << std::endl;
			o.close();
		}
		log_mutex.unlock();
	}
	std::string s = deviceid;
	s += ", pairing key received: ";
	s += Key;
	Log(s);
	return true;
}
//   Write to log file
void Log(std::string ss)
{
	if (!Settings.Prefs.Logging)
		return;
	//thread safe
	while (!log_mutex.try_lock())
		Sleep(MUTEX_WAIT);

	std::ofstream m;
	time_t rawtime;
	struct tm timeinfo;
	char buffer[80];
	std::wstring path = Settings.Prefs.DataPath;

	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);

	strftime(buffer, 80, "%a %H:%M:%S > ", &timeinfo);
	puts(buffer);

	std::string s = buffer;
	s += ss;
	s += "\n";

	path += LOG_FILE;
	m.open(path.c_str(), std::ios::out | std::ios::app);
	if (m.is_open())
	{
		m << s.c_str();
		m.close();
	}
	log_mutex.unlock();
}
//   Callback function for the event log to determine restart or power off, by asking for Events with EventID 1074 to be pushed to the application. Localisation is an issue though....
DWORD WINAPI SubCallback(EVT_SUBSCRIBE_NOTIFY_ACTION Action, PVOID UserContext, EVT_HANDLE hEvent)
{
	UNREFERENCED_PARAMETER(UserContext);
	std::wstring s;
	EventCallbackStatus = NULL;

	if (Action == EvtSubscribeActionDeliver)
	{
		DWORD dwBufferSize = 0;
		DWORD dwBufferUsed = 0;
		DWORD dwPropertyCount = 0;
		LPWSTR pRenderedContent = NULL;

		if (!EvtRender(NULL, hEvent, EvtRenderEventXml, dwBufferSize, pRenderedContent, &dwBufferUsed, &dwPropertyCount))
		{
			if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
			{
				dwBufferSize = dwBufferUsed;
				pRenderedContent = (LPWSTR)malloc(dwBufferSize);
				if (pRenderedContent)
				{
					EvtRender(NULL, hEvent, EvtRenderEventXml, dwBufferSize, pRenderedContent, &dwBufferUsed, &dwPropertyCount);
				}
			}
		}

		if (pRenderedContent)
		{
			s = pRenderedContent;
			free(pRenderedContent);
		}
	}

	for (auto& str : Settings.Prefs.EventLogShutdownString)
	{
		if(str != "")
		{
			std::wstring w = L">";
			w += common::widen(str);
			w += L"<";

			if (s.find(w) != std::wstring::npos)
			{
				EventCallbackStatus = EVENT_SYSTEM_SHUTDOWN;
				Log("Event subscription callback: system shut down detected.");
			}
		}
	}
	if (EventCallbackStatus == NULL)
	{
		for (auto& str : Settings.Prefs.EventLogRestartString)
		{
			if(str != "")
			{
				std::wstring w = L">";
				w += common::widen(str);
				w += L"<";

				if (s.find(w) != std::wstring::npos)
				{
					EventCallbackStatus = EVENT_SYSTEM_REBOOT;
					Log("Event subscription callback: System restart detected.");
				}
			}
		}
	}
	if (EventCallbackStatus == NULL)
	{
		EventCallbackStatus = EVENT_SYSTEM_UNSURE;
		Log("WARNING! Event subscription callback: Could not detect whether system is shutting down or restarting. Please check 'additional settings' in the UI.");
	}
	
	if(EventCallbackStatus)
		SendToNamedPipe(EventCallbackStatus);
	
	return 0;
}
void InitSessions(void)
{
	std::string str = "LGTV Companion Service started (v ";
	str += common::narrow(APP_VERSION);
	str += ") ---------------------------";
	Log(str);
	str = "Data path: ";
	str += common::narrow(Settings.Prefs.DataPath);
	Log(str);

	for (auto& dev : Settings.Devices)
	{
		std::stringstream s;
		s << dev.DeviceId << ", ";
		s << dev.Name;
		s << ", with IP ";
		s << dev.IP << " initiated (";
		s << "Enabled:" << (dev.Enabled ? "yes" : "no") << ", ";
		s << "NewConn:" << (dev.SSL ? "yes" : "no") << ", ";
		s << "WOL:" << dev.WOLtype << ", ";
		s << "SubnetMask:" << dev.Subnet << ", ";
		s << "PairingKey:" << (dev.SessionKey == "" ? "n/a" : dev.SessionKey);
		s << ", MAC: ";
		if (!dev.MAC.empty() && dev.MAC.size() > 0)
		{
			for (auto& m : dev.MAC)
			{
				s << m << " ";
			}
		}
		else
			s << "n/a";
		s << ", VerifyHdmiInput:";
		if (dev.HDMIinputcontrol)
			s << dev.OnlyTurnOffIfCurrentHDMIInputNumberIs;
		else
			s << "off";
		s << ", SetHdmiInput:";
		if (dev.SetHDMIInputOnResume)
			s << dev.SetHDMIInputOnResumeToNumber;
		else
			s << "off";

		s << ", BlankOnIdle:";
		if (Settings.Prefs.BlankScreenWhenIdle) {
			s << "on(";
			s << Settings.Prefs.BlankScreenWhenIdleDelay;
			s << "m)";
		}
		else
			s << "off";
		s << ")";

		SessionManager.AddSession(dev);
		Log(s.str());
	}
}
void NamedPipeCallback(std::wstring message)
{
	if (message.length() == 0)
		return;
	
//	//make lower case
	std::string t = common::narrow(message);
//	transform(t.begin(), t.end(), t.begin(), ::tolower);

	{ //debug remove me
		Log(t);
	}

	// remove leading spaces
	size_t first = t.find_first_not_of(" ", 0);
	if (first != std::string::npos)
		t = t.substr(first);
	else
	{
		Log("[IPC] Invalid command line format. Null length!");
		return;
	}

	if (t.find('-') != 0)
	{
		Log("[IPC] Invalid command line format. Please prefix command with '-'");
		return;
	}

	// hack to allow for escaped double quotes in arguments
	common::ReplaceAllInPlace(t, "\\\"", "*#*#*#"); 
	common::ReplaceAllInPlace(t, "\"", "%¤%¤%¤"); 

	// split into separate command lines and process each
	std::vector <std::string> commands = Extract_Commands(t);
	if (commands.size() == 0)
	{
		Log("[IPC] Invalid command line format. No command was input");
		return;
	}
	for (auto& commandline : commands)
	{
		common::ReplaceAllInPlace(commandline, "%¤%¤%¤", "\"");
		std::string sLogMessage = "";

		// split command line into words
		std::vector <std::string> words = common::stringsplit(commandline, " ");
		size_t nWords = words.size();

		if (nWords == 0)
		{
			Log("[IPC] Invalid command line format. Null length!");
			continue;
		}
		for (auto& wrd : words)
			common::ReplaceAllInPlace(wrd, "*#*#*#", "\"");

		std::string command = words[0];
		transform(command.begin(), command.end(), command.begin(), ::tolower);
		if (command == "daemon")										// COMMUNICATION FROM THE DAEMON
		{
			if (nWords < 3)
			{
				Log("[IPC] Invalid IPC from daemon");
				continue;
			}
			DWORD sessionid = WTSGetActiveConsoleSessionId();
			std::string physical_console = std::to_string(sessionid);
			std::string daemon_id = words[1];
			std::string daemon_command = words[2];
			transform(daemon_command.begin(), daemon_command.end(), daemon_command.begin(), ::tolower);
			std::string log = "[IPC Daemon ";
			log += daemon_id;
			log += "] ";

			if (daemon_command == "errorconfig")
			{
				log += "Could not read configuration file. Daemon exiting!";
				Log(log);
				continue;
			}
			else if (daemon_command == "started")
			{
				log += "Started.";
				Log(log);
				continue;
			}
			else if (daemon_command == "remote_connect")
			{
				log += "Remote streaming client connected. All managed devices will power off.";
				Log(log);
				CreateEvent_system(EVENT_SYSTEM_DISPLAYOFF);
				SessionManager.RemoteClientIsConnected(true);
				continue;
			}
			else if (daemon_command == "remote_disconnect")
			{
				SessionManager.RemoteClientIsConnected(false);
				if (SessionManager.GetWindowsPowerStatus())
				{
					log += "Remote streaming client disconnected. Powering on managed devices.";
					Log(log);
					CreateEvent_system(EVENT_SYSTEM_DISPLAYON);
					continue;
				}
				else
				{
					log += "Remote streaming client disconnected. Managed devices will remain powered off.";
					Log(log);
					continue;
				}
			}
			else if (daemon_command == "newversion")
			{
				log += "A new version of this app is available for download here : ";
				log += common::narrow(NEWRELEASELINK);
				Log(log);
				continue;
			}
			if (physical_console != daemon_id) // only allow process communications from physical console
			{ 
				{ //debug remove me
					std::string sm = "[IPC][DEBUG] omitting communications from session: ";
					sm += daemon_id;
					Log(sm);
				}
				continue;
			}
			else if (daemon_command == "userbusy")
			{
				if (!bIdleLog)
				{
					log += "User is not idle.";
					Log(log);
					bIdleLog = true;
				}
				CreateEvent_system(EVENT_SYSTEM_USERBUSY);
				continue;
			}
			else if (daemon_command == "useridle")
			{
				if (bIdleLog)
				{
					log += "User is idle.";
					Log(log);
					bIdleLog = false;
				}
				CreateEvent_system(EVENT_SYSTEM_USERIDLE);
				continue;
			}
			else if (daemon_command == "topology")
			{
				if (nWords == 3)
				{
					log += "Invalid IPC from daemon. Too few Topology parameters";
					Log(log);
					continue;
				}
				std::string topology_command = words[3];
				transform(topology_command.begin(), topology_command.end(), topology_command.begin(), ::tolower);
				if (topology_command == "invalid")
				{
					log += "A recent change to the system has invalidated the monitor topology configuration. "
						"Please run the configuration guide in the global options again to ensure correct operation.";
					Log(log);
					continue;
				}
				else if (topology_command == "undetermined")
				{
					log += "Warning! No active devices detected when verifying Windows Monitor Topology.";
					Log(log);
					continue;
				}
				else if (topology_command == "state")
				{
					std::vector<std::string> devices = _Devices(words, 4);
					std::string device_state = SessionManager.SetTopology(devices);
					log += "Windows monitor topology was changed. ";
					if (device_state == "")
						log += "No devices configured.";
					else
						log += device_state;
					Log(log);
					CreateEvent_system(EVENT_SYSTEM_TOPOLOGY);
				}
				else
				{
					log += "Invalid IPC from daemon. Topology command is invalid";
					Log(log);
					continue;
				}
			}
			else if (daemon_command == "gfe")
			{
				log += "NVIDIA GFE overlay fullscreen compatibility set.";
				Log(log);
				continue;
			}
		}
		else if (command == "poweron")									// POWER ON
		{
			std::vector<std::string> devices = _Devices(words, 1);
			sLogMessage = "[IPC] Force power ON: ";
			sLogMessage += SessionManager.ValidateDevices(devices);
			Log(sLogMessage);
			CreateEvent_system(EVENT_USER_DISPLAYON, devices);
			continue;
		}
		else if (command == "poweroff")									// POWER OFF
		{
			std::vector<std::string> devices = _Devices(words, 1);
			sLogMessage = "[IPC] Force power OFF: ";
			sLogMessage += SessionManager.ValidateDevices(devices);
			Log(sLogMessage);
			CreateEvent_system(EVENT_USER_DISPLAYOFF, devices);
			continue;
		}
		else if (command == "autoenable")								// AUTOMATIC MANAGEMENT ENABLED
		{
			std::vector<std::string> devices = _Devices(words, 1);
			sLogMessage = "[IPC] Automatic management is temporarily enabled (effective until restart of service): ";
			sLogMessage += SessionManager.ValidateDevices(devices);
			Log(sLogMessage);
			SessionManager.Enable(devices);
			continue;
		}
		else if (command == "autodisable")								// AUTOMATIC MANAGEMENT DISABLED
		{
			std::vector<std::string> devices = _Devices(words, 1);
			sLogMessage = "[IPC] Automatic management is temporarily disabled (effective until restart of service): ";
			sLogMessage += SessionManager.ValidateDevices(devices);
			Log(sLogMessage);
			SessionManager.Disable(devices);
			continue;
		}
		else if (command == "screenon")									// UNBLANK SCREEN
		{
			std::vector<std::string> devices = _Devices(words, 1);
			sLogMessage = "[IPC] Force screen ON: ";
			sLogMessage += SessionManager.ValidateDevices(devices);
			Log(sLogMessage);
			CreateEvent_system(EVENT_USER_DISPLAYON, devices);
			continue;
		}
		else if (command == "screenoff")								// BLANK SCREEN
		{
			std::vector<std::string> devices = _Devices(words, 1);
			sLogMessage = "[IPC] Force screen OFF: ";
			sLogMessage += SessionManager.ValidateDevices(devices);
			Log(sLogMessage);
			CreateEvent_system(EVENT_USER_BLANKSCREEN, devices);
			continue;
		}
		else if (command.find("sethdmi") == 0)							// SET HDMI INPUT
		{
			int cmd_offset = 1;
			std::string argument;
			if (command.size() > 7)		// -sethdmiX ...
			{
				argument = command.substr(7, 1);
				cmd_offset = 1;
			}
			else if (nWords > 1)			// -sethdmi X ...
			{
				argument = words[1].substr(0, 1);
				cmd_offset = 2;
			}
			else
			{
				Log("[IPC] Set HDMI input failed. Missing argument (input number)");
				continue;
			}
			argument = SessionManager.ValidateArgument(argument, "1 2 3 4");
			if (argument != "")
			{
				std::vector<std::string> devices = _Devices(words, cmd_offset);
				std::string payload = LG_URI_PAYLOAD_SETHDMI;

				common::ReplaceAllInPlace(payload, "#ARG#", argument);
				sLogMessage = "[IPC] Set HDMI input ";
				sLogMessage += argument;
				sLogMessage += ": ";
				sLogMessage += SessionManager.ValidateDevices(devices);
				Log(sLogMessage);
				sLogMessage = "set HDMI-input ";
				sLogMessage += argument;
				CreateEvent_request(devices, LG_URI_LAUNCH, payload, sLogMessage);
				continue;
			}
			else
			{
				Log("[IPC] Set HDMI input failed. Invalid argument (input number)");
				continue;
			}
		}
		else if (command == "mute")										// MUTE DEVICE SOUND
		{
			std::vector<std::string> devices = _Devices(words, 1);
			std::string payload = "{\"mute\":\"true\"}";
			sLogMessage = "[IPC] Mute: ";
			sLogMessage += SessionManager.ValidateDevices(devices);
			Log(sLogMessage);
			CreateEvent_request(devices, LG_URI_SETMUTE, payload, "muting speakers");
			continue;
		}
		else if (command == "unmute")									// UNMUTE DEVICE SOUND
		{
			std::vector<std::string> devices = _Devices(words, 1);
			sLogMessage = "[IPC] Unmute: ";
			sLogMessage += SessionManager.ValidateDevices(devices);
			Log(sLogMessage);
			CreateEvent_request(devices, LG_URI_SETMUTE,"", "unmuting speakers");
			continue;
		}
		else if (command == "freesyncinfo")									// SHOW FREESYNC INFORMATION
		{
			std::vector<std::string> devices = _Devices(words, 1);
			std::string cmd_line_start_app_with_param = "-start_app_with_param com.webos.app.tvhotkey \"{\\\"activateType\\\": \\\"freesync-info\\\"}\"";
			for (auto& dev : devices)
			{
				cmd_line_start_app_with_param += " ";
				cmd_line_start_app_with_param += dev;
			}
			NamedPipeCallback(common::widen(cmd_line_start_app_with_param));
			continue;
		}	
		else if (command == "clearlog")									// CLEAR LOG
		{
			std::wstring log = Settings.Prefs.DataPath;
			log += LOG_FILE;
			DeleteFile(log.c_str());
			continue;
		}
		else if (command == "idle")										// USER IDLE MODE FORCED (INFO ONLY AS IT IS ENFORCED IN DAEMON)
		{
			if (Settings.Prefs.BlankScreenWhenIdle)
			{
				Log("[IPC] User idle mode forced!");
			}
			else
				Log("[IPC] Can not force user idle mode, as the feature is not enabled in the global options!");
			continue;
		}
		else if (command == "unidle")									// USER IDLE MODE DEACTIVATED (INFO ONLY AS IT IS ENFORCED IN DAEMON)
		{
			if (Settings.Prefs.BlankScreenWhenIdle)
			{
				Log("[IPC] User idle mode released!");
			}
			else
				Log("[IPC] Can not unset user idle mode, as the feature is not enabled in the global options!");
			continue;
		}
		else if (command == "button")
		{
			if (nWords > 1)
			{
				std::string button = SessionManager.ValidateArgument(words[1], lg_api_buttons);
				if (button != "")
				{
					std::vector<std::string> devices = _Devices(words, 2);
					sLogMessage = "[IPC] press button [";
					sLogMessage += button;
					sLogMessage += "] : ";
					sLogMessage += SessionManager.ValidateDevices(devices);
					Log(sLogMessage);
					sLogMessage = "button press: ";
					sLogMessage += button;
					CreateEvent_button(devices, button, sLogMessage);
				}
				else
					Log("[IPC] Invalid button name.");
			}
			else
				Log("[IPC] Too few arguments for -button");
			continue;
		}

		else if (command == "settings_picture")
		{
			if (nWords > 1)
			{
				std::vector<std::string> devices = _Devices(words, 2);
				std::string payload = words[1];
				sLogMessage = "[IPC] settings_picture ";
				sLogMessage += payload;
				sLogMessage += " : ";
				sLogMessage += SessionManager.ValidateDevices(devices);
				Log(sLogMessage);
				sLogMessage = "luna picture request: ";
				sLogMessage += payload;
//				common::ReplaceAllInPlace(payload, "picturemode", "pictureMode");
				CreateEvent_luna_set_system_setting_payload(devices, "picture", payload, sLogMessage);
			}
			else
				Log("[IPC] Insufficient arguments for -settings_picture.");
			continue;
		}
		else if (command == "settings_other")
		{
			if (nWords > 1)
			{
				std::vector<std::string> devices = _Devices(words, 2);
				std::string payload = words[1];
				sLogMessage = "[IPC] settings_other ";
				sLogMessage += payload;
				sLogMessage += " : ";
				sLogMessage += SessionManager.ValidateDevices(devices);
				Log(sLogMessage);
				sLogMessage = "luna other request: ";
				sLogMessage += payload;
				CreateEvent_luna_set_system_setting_payload(devices, "other", payload, sLogMessage);
			}
			else
				Log("[IPC] Insufficient arguments for -settings_other.");
			continue;
		}
		else if (command == "settings_options")
		{
			if (nWords > 1)
			{
				std::vector<std::string> devices = _Devices(words, 2);
				std::string payload = words[1];
				sLogMessage = "[IPC] settings_options ";
				sLogMessage += payload;
				sLogMessage += " : ";
				sLogMessage += SessionManager.ValidateDevices(devices);
				Log(sLogMessage);
				sLogMessage = "luna options request: ";
				sLogMessage += payload;
				CreateEvent_luna_set_system_setting_payload(devices, "option", payload, sLogMessage);
			}
			else
				Log("[IPC] Insufficient arguments for -settings_options.");
			continue;
		}
		else if (command == "request")
		{
			if (nWords > 1)
			{
				std::vector<std::string> devices = _Devices(words, 2);
				std::string uri = words[1];
				sLogMessage = "[IPC] request ssap://";
				sLogMessage += uri;
				sLogMessage += " : ";
				sLogMessage += SessionManager.ValidateDevices(devices);
				Log(sLogMessage);
				sLogMessage = "request: ssap://";
				sLogMessage += uri;
//				common::ReplaceAllInPlace(uri, "setmute", "setMute");
				CreateEvent_request(devices, uri, "", sLogMessage);
			}		
			else
				Log("[IPC] Insufficient arguments for -request.");
			continue;
		}
		else if (command == "request_with_param")
		{
			if (nWords > 2)
			{
				std::string uri, payload;
				std::vector<std::string> devices;
				try
				{
					uri = words[1];
					payload = nlohmann::json::parse(words[2]).dump();
					devices = _Devices(words, 3);
					sLogMessage = "[IPC] request ssap://";
					sLogMessage += uri;
					sLogMessage += " with payload ";
					sLogMessage += payload;
					sLogMessage += " : ";
					sLogMessage += SessionManager.ValidateDevices(devices);
					Log(sLogMessage);
					sLogMessage = "request: ssap://";
					sLogMessage += uri;
					sLogMessage += " - payload: ";
					sLogMessage += payload;
				}
				catch (std::exception const& e)
				{
					std::string s = "ERROR! Invalid payload JSON in -request_with_param: ";
					s += e.what();
					Log(s);
					continue;
				}
				CreateEvent_request(devices, uri, payload, sLogMessage);
			}
			else
				Log("[IPC] Insufficient arguments for -request_with_param.");
			continue;
		}
		else if (command == "start_app")
		{
			if (nWords > 1)
			{
				std::vector<std::string> devices = _Devices(words, 2);
				std::string uri = LG_URI_LAUNCH;
				std::string id = words[1];
				sLogMessage = "[IPC] start app [";
				sLogMessage += id;
				sLogMessage += "] : ";
				sLogMessage += SessionManager.ValidateDevices(devices);
				Log(sLogMessage);
				sLogMessage = "start app: ";
				sLogMessage += id;
				nlohmann::json j;
				j["id"] = id;
				CreateEvent_request(devices, uri, j.dump(), sLogMessage);
			}
			else
				Log("[IPC] Insufficient arguments for -start_app.");
			continue;
		}
		else if (command == "start_app_with_param")
		{
			if (nWords > 2)
			{
				std::vector<std::string> devices = _Devices(words, 3);
				std::string uri = LG_URI_LAUNCH;
				std::string id = words[1];
				std::string params = words[2];
				nlohmann::json j;
				try
				{				
					j["id"] = id;
					j["params"] = nlohmann::json::parse(params);
					sLogMessage = "[IPC] start app [";
					sLogMessage += id;
					sLogMessage += "] with params [";
					sLogMessage += params;
					sLogMessage += "] : ";
					sLogMessage += SessionManager.ValidateDevices(devices);
					Log(sLogMessage);
					sLogMessage = "start app: ";
					sLogMessage += id;
					sLogMessage += " with params ";
					sLogMessage += params;
				}
				catch (std::exception const& e)
				{
					std::string s = "ERROR! Invalid JSON in -start_app_with_param: ";
					s += e.what();
					Log(s);
					continue;
				}
				CreateEvent_request(devices, uri, j.dump(), sLogMessage);
			}
			else
				Log("[IPC] Insufficient arguments for -start_app_with_param.");
			continue;
		}
		else if (command == "close_app")
		{
			if (nWords > 1)
			{
				std::vector<std::string> devices = _Devices(words, 2);
				std::string uri = LG_URI_CLOSE;
				std::string id = words[1];
				sLogMessage = "[IPC] close app [";
				sLogMessage += id;
				sLogMessage += "] : ";
				sLogMessage += SessionManager.ValidateDevices(devices);
				Log(sLogMessage);
				sLogMessage = "close app: ";
				sLogMessage += id;
				nlohmann::json j;
				j["id"] = id;
				CreateEvent_request(devices, uri, j.dump(), sLogMessage);
			}
			else
				Log("[IPC] Insufficient arguments for -close_app.");
			continue;
		}
		else if (command == "set_input_type") // HDMI_1 icon label
		{
			if (nWords > 3)
			{
				std::vector<std::string> devices = _Devices(words, 4);
				std::string input = words[1];
				std::string icon = words[2];
				std::string label = words[3];
				sLogMessage = "[IPC] set input type for input ";
				sLogMessage += input;
				sLogMessage += " : ";
				sLogMessage += icon;
				sLogMessage += " ";
				sLogMessage += label;
				sLogMessage += " : ";
				sLogMessage += SessionManager.ValidateDevices(devices);
				Log(sLogMessage);
				sLogMessage = "set input type for input ";
				sLogMessage += input;
				sLogMessage += " : ";
				sLogMessage += icon;
				sLogMessage += " ";
				sLogMessage += label;
				CreateEvent_luna_set_device_info(devices, input, icon, label, sLogMessage);
			}
			else
				Log("[IPC] Insufficient arguments for -set_input_type.");
			continue;
		}
		else 
		{
			if (nWords > 1)
			{
				bool found = false;
				for (const auto& item : lg_api_commands_json.items())
				{
					if (item.key() == command) // SETTINGS
					{
						found = true;
						std::string category, setting, argument, arguments, logmessage;
						int max, min = -1;
						if (item.value()["Category"].is_string())
							category = item.value()["Category"].get<std::string>();
						if (item.value()["Setting"].is_string())
							setting = item.value()["Setting"].get<std::string>();
						if (item.value()["Argument"].is_string())
							arguments = item.value()["Argument"].get<std::string>();
						if (item.value()["LogMessage"].is_string())
							logmessage = item.value()["LogMessage"].get<std::string>();
						if (item.value()["Max"].is_number())
							max = item.value()["Max"].get<int>();
						if (item.value()["Min"].is_number())
							min = item.value()["Min"].get<int>();

						if (min != -1)
						{
							int arg = atoi(words[1].c_str());
							if (arg < min)
								arg = min;
							if (arg > max)
								arg = max;
							argument = std::to_string(arg);
						}
						else
						{
							argument = SessionManager.ValidateArgument(words[1], arguments);
						}

						if (argument != "")
						{
							common::ReplaceAllInPlace(logmessage, "#ARG#", argument);

							std::vector<std::string> devices = _Devices(words, 2);
							sLogMessage = "[IPC] ";
							sLogMessage += logmessage;
							sLogMessage += ": ";
							sLogMessage += SessionManager.ValidateDevices(devices);
							Log(sLogMessage);
							size_t hdmi_type_command = setting.find("_hdmi");
							if (hdmi_type_command != std::string::npos) 
							{
								std::string command_ex = setting.substr(0, hdmi_type_command);
								std::string hdmi_input = setting.substr(hdmi_type_command+5, 1);
								std::string payload = "{\"#CMD#\":{\"hdmi#INPUT#\":\"#ARG#\"}}";
								common::ReplaceAllInPlace(payload, "#CMD#", command_ex);
								common::ReplaceAllInPlace(payload, "#INPUT#", hdmi_input);
								common::ReplaceAllInPlace(payload, "#ARG#", argument);
								CreateEvent_luna_set_system_setting_payload(devices, category, payload, logmessage);
							}
							else
								CreateEvent_luna_set_system_setting_basic(devices, category, setting, argument, logmessage);
						}
						else
						{
							sLogMessage = "[IPC] Insufficient argument for -";
							sLogMessage += command;
							sLogMessage += " [";
							sLogMessage += words[1];
							sLogMessage += "] ";
							if (arguments != "")
							{
								sLogMessage += "Valid arguments: ";
								sLogMessage += arguments;
							}
							Log(sLogMessage);

						}
					}
				}
				if (!found)
				{
					sLogMessage = "[IPC] Invalid command : ";
					sLogMessage += command;
					Log(sLogMessage);
				}

			}
		}
	}
}
void SendToNamedPipe(DWORD dwEvent)
{
	if (!Settings.Prefs.ExternalAPI)
		return;

	std::wstring sData = L"";
	switch (dwEvent)
	{
	case EVENT_SYSTEM_DISPLAYOFF:
		sData = L"SYSTEM_DISPLAYS_OFF";
		break;
	case EVENT_SYSTEM_DISPLAYDIMMED:
		sData = L"SYSTEM_DISPLAYS_OFF_DIMMED";
		break;
	case EVENT_SYSTEM_DISPLAYON:
		sData = L"SYSTEM_DISPLAYS_ON";
		break;
	case EVENT_SYSTEM_USERBUSY:
		sData = L"SYSTEM_USER_BUSY";
		break;
	case EVENT_SYSTEM_USERIDLE:
		sData = L"SYSTEM_USER_IDLE";
		break;
	case EVENT_SYSTEM_REBOOT:
		sData = L"SYSTEM_REBOOT";
		break;
	case EVENT_SYSTEM_UNSURE:
	case EVENT_SYSTEM_SHUTDOWN:
		sData = L"SYSTEM_SHUTDOWN";
		break;
	case EVENT_SYSTEM_RESUMEAUTO:
		sData = L"SYSTEM_RESUME";
		break;
	case EVENT_SYSTEM_SUSPEND:
		sData = L"SYSTEM_SUSPEND";
		break;
	default:break;
	}
	if(sData.size() > 0)
		pPipe->Send(sData);
}
void CreateEvent_system(DWORD dwEvent)
{
	EVENT event;
	event.dwType = dwEvent;
	SessionManager.NewEvent(event);

	// Send the global event to named pipe
	SendToNamedPipe(event.dwType);

	// Get host IP, if not already available
	if (HostIPs.size() == 0)
	{
		HostIPs = common::GetOwnIP();
		if (HostIPs.size() != 0)
		{
			std::string logmsg;
			if (HostIPs.size() == 1)
				logmsg = "Host IP detected: ";
			else
				logmsg = "Host IPs detected: ";

			for (int index = 0; index < HostIPs.size(); index++)
			{
				logmsg += HostIPs[index];
				if (index != HostIPs.size() - 1)
					logmsg += ", ";
			}
			Log(logmsg);
		}
	}
	return;
}
void CreateEvent_system(DWORD dwEvent, std::vector<std::string> devices)
{
	EVENT event;
	event.dwType = dwEvent;
	event.devices = devices;
	SessionManager.NewEvent(event);
	return;
}
void CreateEvent_request(std::vector<std::string> devices, std::string uri, std::string payload, std::string log_message)
{
	EVENT event;
	event.dwType = EVENT_REQUEST;
	event.request_uri = uri;
	event.request_payload_json = payload;
	event.log_message = log_message;
	event.devices = devices;
	SessionManager.NewEvent(event);
	return;
}
void CreateEvent_luna_set_system_setting_basic(std::vector<std::string> devices, std::string category, std::string setting, std::string value, std::string log_message)
{
	EVENT event;
	event.dwType = EVENT_LUNA_SYSTEMSET_BASIC;
	event.luna_system_setting_category = category;
	event.luna_system_setting_setting = setting;
	event.luna_system_setting_value = value;
	event.log_message = log_message;
	event.devices = devices;
	SessionManager.NewEvent(event);
	return;
}
void CreateEvent_luna_set_system_setting_payload(std::vector<std::string> devices, std::string category, std::string payload, std::string log_message)
{
	EVENT event;
	event.dwType = EVENT_LUNA_SYSTEMSET_PAYLOAD;
	event.luna_system_setting_category = category;
	event.luna_payload_json = payload;
	event.log_message = log_message;
	event.devices = devices;
	SessionManager.NewEvent(event);
	return;
}
void CreateEvent_button(std::vector<std::string> devices, std::string button, std::string log_message)
{
	EVENT event;
	event.dwType = EVENT_BUTTON;
	event.button = button;
	event.log_message = log_message;
	event.devices = devices;
	SessionManager.NewEvent(event);
	return;
}
void CreateEvent_luna_set_device_info(std::vector<std::string> devices, std::string input, std::string icon, std::string label, std::string log_message)
{
	EVENT event;
	event.dwType = EVENT_LUNA_DEVICEINFO;
	event.luna_device_info_input = input;
	event.luna_device_info_icon = icon;
	event.luna_device_info_label = label;
	event.log_message = log_message;
	event.devices = devices;
	SessionManager.NewEvent(event);
	return;
}
std::vector<std::string> _Devices(std::vector<std::string> CommandWords, int Offset)
{
	if (Offset < CommandWords.size())
		for (int i = 0; i < Offset; i++)
			CommandWords.erase(CommandWords.begin());
	else
		CommandWords.clear();
	return CommandWords;
}
//   Split the commandline into separate commands 
std::vector<std::string> Extract_Commands(std::string str) {
	std::vector<std::string>res;

	size_t f1 = str.find_first_not_of("-", 0);
	if (f1 != std::string::npos)
		str = str.substr(f1);
	else
		return res;

	while (str.size() > 0)
	{
		size_t index = str.find(" -");
		if (index != std::string::npos)
		{
			res.push_back(str.substr(0, index));

			size_t next = str.find_first_not_of(" -", index);
			if (next != std::string::npos)
				str = str.substr(next); //  str.substr(index + token.size());
			else
				str = "";
		}
		else {
			res.push_back(str);
			str = "";
		}		
	}
	return res;
}