#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0603
#define _WIN32_WINNT 0x0603
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "main.h"
#include "companion.h"
#include "../Common/preferences.h"
#include "../Common/event.h"
#include "../Common/common_app_define.h"
#include "../Common/tools.h"
#include "../Common/lg_api.h"
#include <sddl.h>

#pragma comment(lib, "Wevtapi.lib")

// global definitions for the service
#define SVCNAME											L"LGTVsvc"
#define SVCDISPLAYNAME									L"LGTV Companion Service"
#define SERVICE_DEPENDENCIES							L"Dhcp\0Dnscache\0LanmanServer\0\0"
#define SERVICE_ACCOUNT									NULL		

struct Context
{
	Companion*			lgtv_companion;
	Preferences*			prefs;
	SERVICE_STATUS_HANDLE	service_status_handle;
	SERVICE_STATUS			service_status;
	HANDLE					service_stop_event;
};

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
		return SvcInstall() ? 0: 1;
	if (lstrcmpi(argv[1], L"-uninstall") == 0)
		return SvcUninstall() ? 0: 1;
	SERVICE_TABLE_ENTRY DispatchTable[] =
	{
		{ (LPWSTR)SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
		{ NULL, NULL }
	};
	if (!StartServiceCtrlDispatcher(DispatchTable))
		SvcReportEvent(EVENTLOG_ERROR_TYPE, L"StartServiceCtrlDispatcher");
	return 0;
}
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
			printf("Failed to set security descriptor (%d).\n", GetLastError());
		if (!SetServiceObjectSecurity(schService, DACL_SECURITY_INFORMATION, sd))
			printf("Failed to set security descriptor(%d).\n", GetLastError());
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
bool SvcUninstall()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	SERVICE_STATUS_PROCESS ssp;
	DWORD dwStartTime = (DWORD)GetTickCount64();
	DWORD dwBytesNeeded;
	DWORD dwTimeout = 15000; // 15-second time-out
	schSCManager = OpenSCManager(
		NULL,                    // local computer
		NULL,                    // ServicesActive database
		SC_MANAGER_ALL_ACCESS);  // full access rights
	if (NULL == schSCManager)
	{
		printf("Failed to uninstall service. Please run again as ADMIN. OpenSCManager failed (%d).\n", GetLastError());
		return false;
	}
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
DWORD  SvcCtrlHandler(DWORD dwCtrl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
	Context* context = (Context*)lpContext;
	switch (dwCtrl)
	{
	case SERVICE_CONTROL_STOP:
	{
		SvcReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 20000, *context);
		context->lgtv_companion->shutdown();
		// Signal the service to stop.
		SetEvent(context->service_stop_event);
		SvcReportStatus(context->service_status.dwCurrentState, NO_ERROR, 0, *context);
	}	break;
	case SERVICE_CONTROL_POWEREVENT:
	{
		switch (dwEventType)
		{
		case PBT_APMRESUMEAUTOMATIC:
		{
			context->lgtv_companion->systemEvent(EVENT_SYSTEM_RESUMEAUTO);
			context->lgtv_companion->systemEvent(EVENT_SHUTDOWN_TYPE_UNDEFINED);
		}	break;
		case PBT_APMRESUMESUSPEND:
		{
			context->lgtv_companion->systemEvent(EVENT_SHUTDOWN_TYPE_UNDEFINED);
			context->lgtv_companion->systemEvent(EVENT_SYSTEM_RESUME);
		}	break;
		case PBT_APMSUSPEND:
		{
			context->lgtv_companion->systemEvent(EVENT_SYSTEM_SUSPEND);
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
						context->lgtv_companion->systemEvent(EVENT_SYSTEM_DISPLAYOFF);
					else if (PBS->Data[0] == 2)
						context->lgtv_companion->systemEvent(EVENT_SYSTEM_DISPLAYDIMMED);
					else
						context->lgtv_companion->systemEvent(EVENT_SYSTEM_DISPLAYON);
				}
			}
		}	break;

		default:break;
		}
	}	break;
	case SERVICE_CONTROL_PRESHUTDOWN:
	case SERVICE_CONTROL_SHUTDOWN:
	{
		context->lgtv_companion->systemEvent(EVENT_SYSTEM_SHUTDOWN);
		SvcReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 20000, *context);
		context->lgtv_companion->shutdown();
		SetEvent(context->service_stop_event);
		SvcReportStatus(context->service_status.dwCurrentState, NO_ERROR, 0, *context);
	}	break;

	case SERVICE_CONTROL_INTERROGATE:
	{

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
			s += SvcGetErrorMessage(GetLastError());
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
std::wstring SvcGetErrorMessage(DWORD dwErrorCode)
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
VOID SvcReportStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint, Context& context)
{
	static DWORD dwCheckPoint = 1;
	context.service_status.dwCurrentState = dwCurrentState;
	context.service_status.dwWin32ExitCode = dwWin32ExitCode;
	context.service_status.dwWaitHint = dwWaitHint;
	if (dwCurrentState == SERVICE_START_PENDING)
		context.service_status.dwControlsAccepted = 0;
	else context.service_status.dwControlsAccepted = context.prefs->shutdown_timing_ == PREFS_SHUTDOWN_TIMING_EARLY ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PRESHUTDOWN | SERVICE_ACCEPT_POWEREVENT : SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_POWEREVENT;
	if ((dwCurrentState == SERVICE_RUNNING) ||
		(dwCurrentState == SERVICE_STOPPED))
		context.service_status.dwCheckPoint = 0;
	else 
		context.service_status.dwCheckPoint = dwCheckPoint++;
	SetServiceStatus(context.service_status_handle, &context.service_status);
}
VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
	EVT_HANDLE				event_subscribe_handle;
	HPOWERNOTIFY			power_notify_handle;
	Context context;
	Preferences prefs(CONFIG_FILE);
	if (!prefs.isInitialised())
	{
		SvcReportEvent(EVENTLOG_ERROR_TYPE, L"ERROR! Failed to read the configuration file. LGTV Companion service is terminating!");
		return;
	}
	Companion lgtv_companion(prefs);
	context.prefs = &prefs;
	context.lgtv_companion = &lgtv_companion; 
	context.service_status_handle = RegisterServiceCtrlHandlerExW(SVCNAME, SvcCtrlHandler, &context);
	if (!context.service_status_handle)
	{
		SvcReportEvent(EVENTLOG_ERROR_TYPE, L"RegisterServiceCtrlHandler");
		return;
	}
	context.service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	context.service_status.dwServiceSpecificExitCode = 0;
	SvcReportStatus(SERVICE_START_PENDING, NO_ERROR, 3000, context);
	context.service_stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!context.service_stop_event)
	{
		SvcReportEvent(EVENTLOG_ERROR_TYPE, L"CreateEvent");
		SvcReportStatus(SERVICE_STOPPED, NO_ERROR, 0, context);
		return;
	}
	event_subscribe_handle = EvtSubscribe(NULL,                           //local computer
		NULL,                                           // no signalevent
		L"System",                                      //channel path
		L"Event/System[EventID=1074]",                  //query
		NULL,                                           //bookmark
		&context,                                           //Context
		(EVT_SUBSCRIBE_CALLBACK)SvcEventLogSubscribeCallback,            //callback
		EvtSubscribeToFutureEvents);
	SetProcessShutdownParameters(prefs.shutdown_timing_ == PREFS_SHUTDOWN_TIMING_EARLY? 0x100 : 0x3FF, SHUTDOWN_NORETRY);
	SvcReportStatus(SERVICE_RUNNING, NO_ERROR, 0, context);
	SvcReportEvent(EVENTLOG_INFORMATION_TYPE, L"The service has started.");
	lgtv_companion.systemEvent(EVENT_SYSTEM_BOOT);
	power_notify_handle = RegisterPowerSettingNotification(context.service_status_handle, &(GUID_CONSOLE_DISPLAY_STATE), DEVICE_NOTIFY_SERVICE_HANDLE);

	// Wait until service stops
	WaitForSingleObject(context.service_stop_event, INFINITE);

	UnregisterPowerSettingNotification(power_notify_handle);
	EvtClose(event_subscribe_handle);
	SvcReportEvent(EVENTLOG_INFORMATION_TYPE, L"The service has ended.");
	SvcReportStatus(SERVICE_STOPPED, NO_ERROR, 0, context);
	return;
}
DWORD WINAPI SvcEventLogSubscribeCallback(EVT_SUBSCRIBE_NOTIFY_ACTION Action, PVOID UserContext, EVT_HANDLE hEvent)
{
	Context* context = (Context*)(UserContext);
	std::string type;
	std::wstring xml;
	int state = EVENT_SHUTDOWN_TYPE_UNDEFINED;
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
			xml = pRenderedContent;
			free(pRenderedContent);
		}
	}
	if (xml != L"")
	{
		std::wstring strfind = L"<Data Name='param5'>";
		size_t f = xml.find(strfind);
		if (f != std::wstring::npos)
		{
			size_t e = xml.find(L"<", f + 1);
			if (e != std::wstring::npos)
			{
				std::string sub = tools::narrow(xml.substr(f + strfind.length(), e - (f + strfind.length())));
				if(sub != "")
				{
					type = sub;
					if (std::find(context->prefs->event_log_shutdown_strings_.begin(), context->prefs->event_log_shutdown_strings_.end(), sub) != context->prefs->event_log_shutdown_strings_.end()
						|| std::find(context->prefs->event_log_shutdown_strings_custom_.begin(), context->prefs->event_log_shutdown_strings_custom_.end(), sub) != context->prefs->event_log_shutdown_strings_custom_.end())
					{
						state = EVENT_SHUTDOWN_TYPE_SHUTDOWN;
					}
					else if (std::find(context->prefs->event_log_restart_strings_.begin(), context->prefs->event_log_restart_strings_.end(), sub) != context->prefs->event_log_restart_strings_.end()
						|| std::find(context->prefs->event_log_restart_strings_custom_.begin(), context->prefs->event_log_restart_strings_custom_.end(), sub) != context->prefs->event_log_restart_strings_custom_.end())
					{
						state = EVENT_SHUTDOWN_TYPE_REBOOT;
					}
					else
					{
						state = EVENT_SHUTDOWN_TYPE_UNSURE;
					}
				}
			}
		}

	}

	if (state == EVENT_SHUTDOWN_TYPE_UNDEFINED)
	{
		state = EVENT_SHUTDOWN_TYPE_ERROR;
	}

	if(state == EVENT_SHUTDOWN_TYPE_SHUTDOWN || state == EVENT_SHUTDOWN_TYPE_REBOOT || state == EVENT_SHUTDOWN_TYPE_UNSURE)
	{
		context->lgtv_companion->systemEvent(state, type);
	}
	else
	{
		context->lgtv_companion->systemEvent(state);
	}
	return 0;
}
