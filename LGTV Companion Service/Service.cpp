// See LGTV Companion UI.cpp for additional details

#include "Service.h"

using namespace std;
using json = nlohmann::json;

//globals
SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = NULL;
DEV_BROADCAST_DEVICEINTERFACE filter;
HDEVNOTIFY              gDevEvents;
HPOWERNOTIFY            gPs1;
json                    jsonPrefs;                          //contains the user preferences in json
PREFS                   Prefs;                              //App preferences
vector <CSession>       DeviceCtrlSessions;                 //CSession objects manage network connections with Display
DWORD                   EventCallbackStatus = NULL;
WSADATA                 WSAData;
mutex                   log_mutex;
wstring                 DataPath;
vector<string>          HostIPs;

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
		SvcInstall();
		return 0;
	}
	if (lstrcmpi(argv[1], L"-uninstall") == 0)
	{
		SvcUninstall();
		return 0;
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
VOID SvcInstall()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	WCHAR szPath[MAX_PATH];

	if (!GetModuleFileNameW(NULL, szPath, MAX_PATH))
	{
		printf("Failed to install service (%d).\n", GetLastError());
		return;
	}

	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database
		SC_MANAGER_ALL_ACCESS);  // full access rights

	if (!schSCManager)
	{
		printf("Failed to install service. Please run again as ADMIN. OpenSCManager failed (%d).\n", GetLastError());
		return;
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
		return;
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
		}
		else
			printf("Service successfully installed and started.\n");
	}

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}
//   Uninstall service in the SCM database
VOID SvcUninstall()
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
		return;
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
		return;
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
		return;
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
				return;
			}
		}
		else
		{
			printf("Failed to uninstall service. Unable to query service status.\n");
			CloseServiceHandle(schService);
			CloseServiceHandle(schSCManager);
			return;
		}
	}

	if (!DeleteService(schService))
	{
		printf("Failed to uninstall service. DeleteService failed (%d)\n", GetLastError());
	}
	else printf("Service uninstalled successfully\n");

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
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

	//load the config file into json::Prefs and initiate the device control sessions
	try {
		if (ReadConfigFile())
			InitDeviceSessions();
	}
	catch (std::exception const& e)
	{
		wstring s = L"ERROR! Failed to read the configuration file. LGTV service is terminating. Error: ";
		s += widen(e.what());
		SvcReportEvent(EVENTLOG_ERROR_TYPE, s);
		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		//       UnregisterPowerSettingNotification(gPs1);
		WSACleanup();
		return;
	}

	//get local host IPs, if possible at this time
	HostIPs = GetOwnIP();
	if (HostIPs.size() > 0)
	{
		string logmsg;
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

	//spawn a thread for managing intra process communication
	thread thread_obj(IPCThread);
	thread_obj.detach();

	//make sure the process is shutdown as early as possible
	if (SetProcessShutdownParameters(0x3FF, 0))
		Log("Setting shutdown parameter level 0x3FF");
	else
		Log("Could not set shutdown parameter level");

	ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

	//register to receive power notifications
	gPs1 = RegisterPowerSettingNotification(gSvcStatusHandle, &(GUID_CONSOLE_DISPLAY_STATE), DEVICE_NOTIFY_SERVICE_HANDLE);

	SvcReportEvent(EVENTLOG_INFORMATION_TYPE, L"The service has started.");

	DispatchSystemPowerEvent(SYSTEM_EVENT_BOOT);

	// Wait until service stops
	WaitForSingleObject(ghSvcStopEvent, INFINITE);

	//terminate the device control sessions
	DeviceCtrlSessions.clear();

	UnregisterPowerSettingNotification(gPs1);
	WSACleanup();

	EvtClose(hSub);
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
	bool bThreadNotFinished;

	switch (dwCtrl)
	{
	case SERVICE_CONTROL_STOP:
		ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 20000);

		do
		{
			bThreadNotFinished = false;

			for (auto& dev : DeviceCtrlSessions)
			{
				if (dev.IsBusy())
					bThreadNotFinished = true;
			}
			if (bThreadNotFinished)
				Sleep(100);
		} while (bThreadNotFinished);

		// Signal the service to stop.
		SetEvent(ghSvcStopEvent);
		ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

		break;
		//    case SERVICE_CONTROL_USERMODEREBOOT: // does not seem to work. Unfortunate! Need to investigate alternative means of detecting a system restart then.
		//        break;
	case SERVICE_CONTROL_POWEREVENT:
		switch (dwEventType)
		{
		case PBT_APMRESUMEAUTOMATIC:
			EventCallbackStatus = NULL;;
			Log("** System resumed from low power state (Automatic).");
			DispatchSystemPowerEvent(SYSTEM_EVENT_RESUMEAUTO);
			Prefs.DisplayIsCurrentlyRequestedPoweredOnByWindows = true;
			break;
		case PBT_APMRESUMESUSPEND:
			EventCallbackStatus = NULL;;
			Log("** System resumed from low power state.");
			DispatchSystemPowerEvent(SYSTEM_EVENT_RESUME);
			Prefs.DisplayIsCurrentlyRequestedPoweredOnByWindows = true;
			break;
		case PBT_APMSUSPEND:

			if (EventCallbackStatus == SYSTEM_EVENT_REBOOT)
			{
				Log("** System is restarting.");
				DispatchSystemPowerEvent(SYSTEM_EVENT_REBOOT);
			}
			else if (EventCallbackStatus == SYSTEM_EVENT_SHUTDOWN)
			{
				Log("** System is shutting down (low power mode).");
				DispatchSystemPowerEvent(SYSTEM_EVENT_SUSPEND);
				Prefs.DisplayIsCurrentlyRequestedPoweredOnByWindows = false;
			}
			else if (EventCallbackStatus == SYSTEM_EVENT_UNSURE)
			{
				Log("WARNING! Unable to determine if system is shutting down or restarting. Please check 'additional settings' in the UI.");
				DispatchSystemPowerEvent(SYSTEM_EVENT_UNSURE);
				Prefs.DisplayIsCurrentlyRequestedPoweredOnByWindows = false;
			}
			else
			{
				Log("** System is suspending to a low power state.");
				DispatchSystemPowerEvent(SYSTEM_EVENT_SUSPEND);
				Prefs.DisplayIsCurrentlyRequestedPoweredOnByWindows = false;
			}
			break;
		case PBT_POWERSETTINGCHANGE:
			if (lpEventData)
			{
				POWERBROADCAST_SETTING* PBS = NULL;
				string text;
				PBS = (POWERBROADCAST_SETTING*)lpEventData;
				if (PBS->PowerSetting == GUID_CONSOLE_DISPLAY_STATE)
				{
					if (PBS->Data[0] == 0)
					{
						Log("** System requests displays OFF.");
						DispatchSystemPowerEvent(SYSTEM_EVENT_DISPLAYOFF);
						Prefs.DisplayIsCurrentlyRequestedPoweredOnByWindows = false;
					}
					else if (PBS->Data[0] == 2)
					{
						Log("** System requests displays OFF(DIMMED).");
						DispatchSystemPowerEvent(SYSTEM_EVENT_DISPLAYDIMMED);
						Prefs.DisplayIsCurrentlyRequestedPoweredOnByWindows = false;
					}
					else
					{
						Log("** System requests displays ON.");
						DispatchSystemPowerEvent(SYSTEM_EVENT_DISPLAYON);
						Prefs.DisplayIsCurrentlyRequestedPoweredOnByWindows = true;
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
			break;

		default:break;
		}
		break;
	case SERVICE_CONTROL_PRESHUTDOWN:
		if (EventCallbackStatus == SYSTEM_EVENT_REBOOT)
		{
			Log("** System is restarting.");
			DispatchSystemPowerEvent(SYSTEM_EVENT_REBOOT);
		}
		else if (EventCallbackStatus == SYSTEM_EVENT_SHUTDOWN)
		{
			Log("** System is shutting down.");
			DispatchSystemPowerEvent(SYSTEM_EVENT_SHUTDOWN);
			Prefs.DisplayIsCurrentlyRequestedPoweredOnByWindows = false;
		}
		else if (EventCallbackStatus == SYSTEM_EVENT_UNSURE)
		{
			Log("WARNING! Unable to determine if system is shutting down or restarting. Please check 'additional settings in the UI.");
			DispatchSystemPowerEvent(SYSTEM_EVENT_UNSURE);
			Prefs.DisplayIsCurrentlyRequestedPoweredOnByWindows = false;
		}
		else
		{
			//This does happen sometimes, probably for timing reasons when shutting down the system.
			Log("WARNING! The application did not receive an Event Subscription Callback prior to system shutting down. Unable to determine if system is shutting down or restarting.");
			DispatchSystemPowerEvent(SYSTEM_EVENT_UNSURE);
			Prefs.DisplayIsCurrentlyRequestedPoweredOnByWindows = false;
		}

		ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 20000);

		do
		{
			bThreadNotFinished = false;

			for (auto& dev : DeviceCtrlSessions)
			{
				if (dev.IsBusy())
					bThreadNotFinished = true;
			}
			if (bThreadNotFinished)
				Sleep(100);
		} while (bThreadNotFinished);

		// Signal the service to stop.
		SetEvent(ghSvcStopEvent);
		ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
		break;

	case SERVICE_CONTROL_INTERROGATE:
		Log("SERVICE_CONTROL_INTERROGATE");
		break;
	default:break;
	}
	return NO_ERROR;
}
//   Add an event in the event log. Type can be: EVENTLOG_SUCCESS, EVENTLOG_ERROR_TYPE, EVENTLOG_INFORMATION_TYPE
VOID SvcReportEvent(WORD Type, wstring string)
{
	HANDLE hEventSource;
	LPCTSTR lpszStrings[2];

	hEventSource = RegisterEventSource(NULL, SVCNAME);

	if (hEventSource)
	{
		wstring s;
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
wstring GetErrorMessage(DWORD dwErrorCode)
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
			wstring result(lpMsgStr, lpMsgStr + bufLen);
			LocalFree(lpMsgBuf);
			return result;
		}
	}
	return L"";
}
//   Write key to configuration file when a thread has received a pairing key. Should be called in thread safe manner
bool SetSessionKey(string Key, string deviceid)
{
	if (Key.size() > 0 && deviceid.size() > 0)
	{
		wstring path = DataPath;
		path += L"config.json";

		ofstream i(path.c_str());
		if (i.is_open())
		{
			jsonPrefs[deviceid]["SessionKey"] = Key;
			i << setw(4) << jsonPrefs << endl;
			i.close();
			//          return true;
		}
	}
	string s = deviceid;
	s += ", pairing key received: ";
	s += Key;
	Log(s);
	return true;
}
//   Read the configuration file into a json object and populate the preferences struct
bool ReadConfigFile()
{
	WCHAR szPath[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath)))
	{
		string st = "LGTV Companion Service started (v ";
		st += narrow(APPVERSION);
		st += ") ---------------------------";
		string ty = "Data path: ";
		wstring path = szPath;
		path += L"\\";
		path += APPNAME;
		path += L"\\";
		CreateDirectory(path.c_str(), NULL);
		ty += narrow(path);
		Log(ty);

		DataPath = path;

		path += L"config.json";

		ifstream i(path.c_str());
		if (i.is_open())
		{
			json j;
			i >> jsonPrefs;
			i.close();

			j = jsonPrefs[JSON_PREFS_NODE][JSON_EVENT_RESTART_STRINGS];
			if (!j.empty() && j.size() > 0)
			{
				Prefs.EventLogRestartString.clear();
				for (auto& elem : j.items())
					Prefs.EventLogRestartString.push_back(elem.value().get<string>());
			}

			j = jsonPrefs[JSON_PREFS_NODE][JSON_EVENT_SHUTDOWN_STRINGS];
			if (!j.empty() && j.size() > 0)
			{
				Prefs.EventLogShutdownString.clear();
				for (auto& elem : j.items())
					Prefs.EventLogShutdownString.push_back(elem.value().get<string>());
			}

			j = jsonPrefs[JSON_PREFS_NODE][JSON_VERSION];
			if (!j.empty() && j.is_number())
				Prefs.version = j.get<int>();

			j = jsonPrefs[JSON_PREFS_NODE][JSON_PWRONTIMEOUT];
			if (!j.empty() && j.is_number())
				Prefs.PowerOnTimeout = j.get<int>();

			j = jsonPrefs[JSON_PREFS_NODE][JSON_LOGGING];
			if (!j.empty() && j.is_boolean())
				Prefs.Logging = j.get<bool>();

			j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEBLANK];
			if (!j.empty() && j.is_boolean())
				Prefs.BlankWhenIdle = j.get<bool>();

			j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEBLANKDELAY];
			if (!j.empty() && j.is_number())
				Prefs.BlankScreenWhenIdleDelay = j.get<int>();

			j = jsonPrefs[JSON_PREFS_NODE][JSON_RDP_POWEROFF];
			if (!j.empty() && j.is_boolean())
				Prefs.PowerOffDuringRDP = j.get<bool>();

			Log(st);
			Log("Configuration file successfully read");
			Log(ty);
			return true;
		}
	}
	return false;
}
//   Create and init device objects
void InitDeviceSessions()
{
	if (jsonPrefs.empty())
		return;

	//Clear current sessions.
	DeviceCtrlSessions.clear();

	//Iterate nodes
	for (const auto& item : jsonPrefs.items())
	{
		json j;
		if (item.key() == JSON_PREFS_NODE)
			break;
		stringstream s;

		SESSIONPARAMETERS params;

		//DEVICEID
		params.DeviceId = item.key();
		s << params.DeviceId << ", ";
		//NAME
		if (item.value()["Name"].is_string())
			params.Name = item.value()["Name"].get<string>();
		s << params.Name;
		//IP
		s << ", with IP ";
		if (item.value()["IP"].is_string())
			params.IP = item.value()["IP"].get<string>();
		s << params.IP << " initiated (";
		//ENABLED
		if (item.value()["Enabled"].is_boolean())
			params.Enabled = item.value()["Enabled"].get<bool>();
		s << "Enabled:" << (params.Enabled ? "yes" : "no") << ", ";
		//SUBNET AND WOL TYPE
		if (item.value()["Subnet"].is_string())
			params.Subnet = item.value()["Subnet"].get<string>();
		if (item.value()["WOL"].is_number())
			params.WOLtype = item.value()["WOL"].get<int>();
		s << "WOL:" << params.WOLtype << ", ";
		if (params.WOLtype == WOL_SUBNETBROADCAST && params.Subnet != "")
			s << "SubnetMask:" << params.Subnet << ", ";
		//PAIRING KEY
		if (item.value()["SessionKey"].is_string())
			params.SessionKey = item.value()["SessionKey"].get<string>();
		s << "PairingKey:" << (params.SessionKey == "" ? "n/a" : params.SessionKey);
		//MAC
		s << ", MAC: ";
		j = item.value()["MAC"];
		if (!j.empty() && j.size() > 0)
		{
			for (auto& m : j.items())
			{
				params.MAC.push_back(m.value().get<string>());
				s << m.value().get<string>() << " ";
			}
		}
		else
			s << "n/a";
		//POWER OFF HDMI INPUT VERIFICATION
		s << ", VerifyHdmiInput:";
		if (item.value()["HDMIinputcontrol"].is_boolean())
			params.HDMIinputcontrol = item.value()["HDMIinputcontrol"].get<bool>();
		if (item.value()["OnlyTurnOffIfCurrentHDMIInputNumberIs"].is_number())
			params.OnlyTurnOffIfCurrentHDMIInputNumberIs = item.value()["OnlyTurnOffIfCurrentHDMIInputNumberIs"].get<int>();
		if (params.HDMIinputcontrol) {
			s << params.OnlyTurnOffIfCurrentHDMIInputNumberIs;
		}
		else
			s << "off";
		//SET HDMI INPUT ON BOOT/RESUME
		s << ", SetHdmiInput:";
		if (item.value()["SetHDMIInputOnResume"].is_boolean())
			params.SetHDMIInputOnResume = item.value()["SetHDMIInputOnResume"].get<bool>();
		if (item.value()["SetHDMIInputOnResumeToNumber"].is_number())
			params.SetHDMIInputOnResumeToNumber = item.value()["SetHDMIInputOnResumeToNumber"].get<int>();
		if (params.SetHDMIInputOnResume)
		{
			s << params.SetHDMIInputOnResumeToNumber;
		}
		else
			s << "off";
		//SCREEN BLANKING ON USER IDLE
		s << ", BlankOnIdle:";
		params.BlankWhenIdle = Prefs.BlankWhenIdle;
		params.BlankScreenWhenIdleDelay = Prefs.BlankScreenWhenIdleDelay;
		if (params.BlankWhenIdle) {
			s << "on(";
			s << params.BlankScreenWhenIdleDelay;
			s << "m)";
		}
		else
			s << "off";

		s << ")";

		params.PowerOnTimeout = Prefs.PowerOnTimeout;

		CSession S(&params);
		S.DeviceID = params.DeviceId;
		DeviceCtrlSessions.push_back(S);
		Log(s.str());
	}
	return;
}
//   Broadcast power events (display on/off, resuming, rebooting etc) to the device objects
bool DispatchSystemPowerEvent(DWORD dwMsg)
{
	if (DeviceCtrlSessions.size() == 0)
	{
		Log("WARNING! No Devices in DispatchSystemPowerEvent().");
		return false;
	}
	for (auto& value : DeviceCtrlSessions)
		value.SystemEvent(dwMsg);

	//get local host IPs, if possible at this time
	if (HostIPs.size() == 0)
	{
		HostIPs = GetOwnIP();
		if (HostIPs.size() != 0)
		{
			string logmsg;
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
	return true;
}
//   Write to log file
void Log(string ss)
{
	if (!Prefs.Logging)
		return;
	//thread safe
	while (!log_mutex.try_lock())
		Sleep(MUTEX_WAIT);

	ofstream m;
	time_t rawtime;
	struct tm timeinfo;
	char buffer[80];
	wstring path = DataPath;

	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);

	strftime(buffer, 80, "%a %H:%M:%S > ", &timeinfo);
	puts(buffer);

	string s = buffer;
	s += ss;
	s += "\n";

	path += L"Log.txt";
	m.open(path.c_str(), ios::out | ios::app);
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
	wstring s;
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

	for (auto& str : Prefs.EventLogShutdownString)
	{
		wstring w = L">";
		w += widen(str);
		w += L"<";

		if (s.find(w) != wstring::npos)
		{
			EventCallbackStatus = SYSTEM_EVENT_SHUTDOWN;
			Log("Event subscription callback: system shut down detected.");
		}
	}
	for (auto& str : Prefs.EventLogRestartString)
	{
		wstring w = L">";
		w += widen(str);
		w += L"<";

		if (s.find(w) != wstring::npos)
		{
			EventCallbackStatus = SYSTEM_EVENT_REBOOT;
			Log("Event subscription callback: System restart detected.");
		}
	}
	if (EventCallbackStatus == NULL)
	{
		EventCallbackStatus = SYSTEM_EVENT_UNSURE;
		Log("WARNING! Event subscription callback: Could not detect whether system is shutting down or restarting. Please check 'additional settings' in the UI.");
	}

	return 0;
}
//   Convert UTF-8 to wide
wstring widen(string sInput) {
	if (sInput == "")
		return L"";

	// Calculate target buffer size
	long len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(), NULL, 0);
	if (len == 0)
		return L"";

	// Convert character sequence
	wstring out(len, 0);
	if (len != MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(), &out[0], (int)out.size()))
		return L"";

	return out;
}
//   Convert wide to UTF-8
string narrow(wstring sInput) {
	// Calculate target buffer size
	long len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(),
		NULL, 0, NULL, NULL);
	if (len == 0)
		return "";

	// Convert character sequence
	string out(len, 0);
	if (len != WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(), &out[0], (int)out.size(), NULL, NULL))
		return "";

	return out;
}
//   THREAD: The intra process communications (named pipe) thread. Runs for the duration of the service
void IPCThread(void)
{
	HANDLE hPipe;
	char buffer[1024];
	DWORD dwRead;

	PSECURITY_DESCRIPTOR psd = NULL;
	BYTE  sd[SECURITY_DESCRIPTOR_MIN_LENGTH];
	psd = (PSECURITY_DESCRIPTOR)sd;
	InitializeSecurityDescriptor(psd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(psd, TRUE, (PACL)NULL, FALSE);
	SECURITY_ATTRIBUTES sa = { sizeof(sa), psd, FALSE };

	hPipe = CreateNamedPipe(PIPENAME,
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,   // FILE_FLAG_FIRST_PIPE_INSTANCE is not needed but forces CreateNamedPipe(..) to fail if the pipe already exists...
		1,
		1024 * 16,
		1024 * 16,
		NMPWAIT_USE_DEFAULT_WAIT,
		&sa);
	while (hPipe != INVALID_HANDLE_VALUE)
	{
		if (ConnectNamedPipe(hPipe, NULL) != FALSE)   // wait for someone to connect to the pipe
		{
			while (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &dwRead, NULL) != FALSE)
			{
				/* add terminating zero */
				buffer[dwRead] = '\0';
				string t = buffer;
				transform(t.begin(), t.end(), t.begin(), ::tolower);

				vector <string> cmd = stringsplit(t, " ");
				if (cmd.size() > 0)
				{
					int param1 = 0;

					for (auto& param : cmd)
					{
						if (param == "-daemon")
							param1 = APP_IPC_DAEMON;
						if (param == "-poweron")
							param1 = APP_CMDLINE_ON;
						else if (param == "-poweroff")
							param1 = APP_CMDLINE_OFF;
						else if (param == "-autoenable")
							param1 = APP_CMDLINE_AUTOENABLE;
						else if (param == "-autodisable")
							param1 = APP_CMDLINE_AUTODISABLE;
						else if (param == "-screenon")
							param1 = APP_CMDLINE_SCREENON;
						else if (param == "-screenoff")
							param1 = APP_CMDLINE_SCREENOFF;
						else if (param == "-sethdmi1")
							param1 = APP_CMDLINE_SETHDMI1;
						else if (param == "-sethdmi2")
							param1 = APP_CMDLINE_SETHDMI2;
						else if (param == "-sethdmi3")
							param1 = APP_CMDLINE_SETHDMI3;
						else if (param == "-sethdmi4")
							param1 = APP_CMDLINE_SETHDMI4;
						else if (param == "-clearlog")
						{
							string w = "IPC, clear log  ";
							wstring log = DataPath;
							log += L"Log.txt";
							w += narrow(log);
							Log(w);
							DeleteFile(log.c_str());
						}

						else if (param1 > 0)
						{
							if (param1 == APP_IPC_DAEMON)
							{
								if (param == "errorconfig")
									Log("IPC, Daemon, Could not read configuration file. Daemon exiting!");
								else if (param == "started")
									Log("IPC, Daemon has started.");
								else if (param == "newversion")
								{
									wstring s = L"IPC, Daemon, a new version of this app is available for download here: ";
									s += NEWRELEASELINK;
									Log(narrow(s));
								}
								else if (param == "userbusy")
								{
									Log("IPC, User is not idle.");
									DispatchSystemPowerEvent(SYSTEM_EVENT_USERBUSY);
								}
								else if (param == "useridle")
								{
									Log("IPC, User is idle.");
									DispatchSystemPowerEvent(SYSTEM_EVENT_USERIDLE);
								}
								else if (param == "remoteconnect_busy")
								{
									if (Prefs.PowerOffDuringRDP)
									{
										Log("IPC, Remote session connected. User idle management disabled, Powering off managed displays.");
										DispatchSystemPowerEvent(SYSTEM_EVENT_DISPLAYOFF);
									}
									else
										Log("IPC, Remote session connected. User idle management disabled.");
								}
								else if (param == "remoteconnect_idle")
								{
									if (Prefs.PowerOffDuringRDP)
									{
										Log("IPC, Remote session connected. User idle management disabled. Powering off managed displays.");
										DispatchSystemPowerEvent(SYSTEM_EVENT_DISPLAYOFF);
									}
									else
									{
										Log("IPC, Remote session connected. User idle management disabled");
										DispatchSystemPowerEvent(SYSTEM_EVENT_UNBLANK);
									}
								}
								else if (param == "remoteconnect")
								{
									if (Prefs.PowerOffDuringRDP)
									{
										Log("IPC, Remote session connected. Powering off managed displays.");
										DispatchSystemPowerEvent(SYSTEM_EVENT_DISPLAYOFF);
									}
									else
										Log("IPC, Remote session connected.");
								}
								else if (param == "remotedisconnect")
								{
									if (Prefs.DisplayIsCurrentlyRequestedPoweredOnByWindows)
									{
										Log("IPC, Remote session disconnected. Powering on managed displays.");
										DispatchSystemPowerEvent(SYSTEM_EVENT_DISPLAYON);
									}
									else
									{
										Log("IPC, Remote session disconnected.");
									}
								}
								else if (param == "topology")
								{
									param1 = APP_IPC_DAEMON_TOPOLOGY;
									for (auto &d : DeviceCtrlSessions)
									{
										d.SetTopology(false);
									}
								}

							}
							else if (param1 == APP_IPC_DAEMON_TOPOLOGY)
							{
								if (param == "*")
								{
									stringstream s;
									s << "IPC, windows monitor topology was changed.";
									Log(s.str());
									DispatchSystemPowerEvent(SYSTEM_EVENT_TOPOLOGY);
								}
								else
								{
									for (auto &d : DeviceCtrlSessions)
									{
										string id = d.DeviceID;
										transform(id.begin(), id.end(), id.begin(), ::tolower);

										if (param == id)
										{
											stringstream s;
											s << "IPC, ";
											s << d.DeviceID;
											s << " - enabled in windows monitor topology.";
											Log(s.str());
											d.SetTopology(true);
										}
									}
								}
							}

							else
							{
								for (auto& device : DeviceCtrlSessions)
								{
									SESSIONPARAMETERS s = device.GetParams();
									string id = s.DeviceId;
									string name = s.Name;
									transform(id.begin(), id.end(), id.begin(), ::tolower);
									transform(name.begin(), name.end(), name.begin(), ::tolower);

									if (param1 == APP_CMDLINE_ON && (id == param || name == param))
									{
										string w = "IPC, force power ON: ";
										w += param;
										Log(w);
										device.SystemEvent(SYSTEM_EVENT_FORCEON);
									}
									else if (param1 == APP_CMDLINE_OFF && (id == param || name == param))
									{
										string w = "IPC, force power OFF: ";
										w += param;
										Log(w);
										device.SystemEvent(SYSTEM_EVENT_FORCEOFF);
									}
									else if (param1 == APP_CMDLINE_SCREENON && (id == param || name == param))
									{
										string w = "IPC, force screen ON: ";
										w += param;
										Log(w);
										device.SystemEvent(SYSTEM_EVENT_FORCEON);
									}
									else if (param1 == APP_CMDLINE_SCREENOFF && (id == param || name == param))
									{
										string w = "IPC, force screen OFF: ";
										w += param;
										Log(w);
										device.SystemEvent(SYSTEM_EVENT_FORCESCREENOFF);
									}
									else if (param1 == APP_CMDLINE_AUTOENABLE && (id == param || name == param))
									{
										string w = "IPC, automatic management is temporarily enabled (effective until restart of service): ";
										w += param;
										Log(w);

										device.Run();
									}
									else if (param1 == APP_CMDLINE_AUTODISABLE && (id == param || name == param))
									{
										string w = "IPC, automatic management is temporarily disabled (effective until restart of service): ";
										w += param;
										Log(w);
										device.Stop();
									}
									else if (param1 == APP_CMDLINE_SETHDMI1 && (id == param || name == param))
									{
										string w = "IPC, set HDMI input 1:";
										w += param;
										Log(w);
										device.SystemEvent(SYSTEM_EVENT_FORCESETHDMI, 1);
									}
									else if (param1 == APP_CMDLINE_SETHDMI2 && (id == param || name == param))
									{
										string w = "IPC, set HDMI input 2:";
										w += param;
										Log(w);
										device.SystemEvent(SYSTEM_EVENT_FORCESETHDMI, 2);
									}
									else if (param1 == APP_CMDLINE_SETHDMI3 && (id == param || name == param))
									{
										string w = "IPC, set HDMI input 3:";
										w += param;
										Log(w);
										device.SystemEvent(SYSTEM_EVENT_FORCESETHDMI, 3);
									}
									else if (param1 == APP_CMDLINE_SETHDMI4 && (id == param || name == param))
									{
										string w = "IPC, set HDMI input 4:";
										w += param;
										Log(w);
										device.SystemEvent(SYSTEM_EVENT_FORCESETHDMI, 4);
									}
								}
							}
						}
					}
				}
			}
		}
		DisconnectNamedPipe(hPipe);
	}
	return;
}
//   Split a string into a vector of strings, modified to accept quotation marks
vector<string> stringsplit(string str, string token) {
	vector<string>res;
	while (str.size() > 0)
	{
		size_t index;
		if (str[0] == '\"') // quotation marks
		{
			index = str.find("\"", 1);
			if (index != string::npos)
			{
				if (index - 2 > 0)
				{
					string temp = str.substr(1, index - 1);
					res.push_back(temp);
				}
				size_t next = str.find_first_not_of(token, index + token.size());
				if (next != string::npos)
					str = str.substr(next); //  str.substr(index + token.size());
				else
					str = "";
			}
			else
			{
				res.push_back(str);
				str = "";
			}
		}
		else // not quotation marks
		{
			index = str.find(token);
			if (index != string::npos)
			{
				res.push_back(str.substr(0, index));

				size_t next = str.find_first_not_of(token, index + token.size());
				if (next != string::npos)
					str = str.substr(next); //  str.substr(index + token.size());
				else
					str = "";
			}
			else {
				res.push_back(str);
				str = "";
			}
		}
	}

	return res;
}
// Get the local host ip, e.g 192.168.1.x
vector <string> GetOwnIP(void)
{
	vector <string> IPs;
	char host[256];
	if (gethostname(host, sizeof(host)) != SOCKET_ERROR)
	{
		struct hostent* phent = gethostbyname(host);
		if (phent != 0)
		{
			for (int i = 0; phent->h_addr_list[i] != 0; ++i)
			{
				string ip;
				struct in_addr addr;
				memcpy(&addr, phent->h_addr_list[i], sizeof(struct in_addr));
				ip = inet_ntoa(addr);
				if (ip != "127.0.0.1")
					IPs.push_back(ip);
			}
		}
	}
	return IPs;
}
