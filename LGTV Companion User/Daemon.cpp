#include "Daemon.h"

namespace						common = jpersson77::common;
namespace						settings = jpersson77::settings;
namespace						ipc = jpersson77::ipc;

// Globals:
HINSTANCE                       hInstance;  // current instance
HWND                            hMainWnd = NULL;
bool                            bIdle = false;
bool                            bIdlePreventEarlyWakeup = false;
bool                            bIdlePreventFalseBusy = false;
bool                            bDaemonVisible = true;
bool                            bFirstRun = false;
DWORD                           dwLastInputTick = 0;
DWORD                           dwProcessedLastInputTick = 0;
WinToastHandler                 m_WinToastHandler;
HANDLE                          hPipe = INVALID_HANDLE_VALUE;
INT64                           idToastFirstrun = NULL;
INT64                           idToastNewversion = NULL;
UINT							shellhookMessage;
DWORD							daemon_startup_user_input_time = 0;
UINT							ManualUserIdleMode = 0;
HBRUSH                          hBackbrush;
time_t							TimeOfLastTopologyChange = 0;
ipc::PipeClient*				pPipeClient;
settings::Preferences			Settings;
std::string						sessionID;

//Application entry point
int APIENTRY wWinMain(_In_ HINSTANCE Instance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(nCmdShow);

	hInstance = Instance;
	MSG msg;
	std::wstring WindowTitle;
	WindowTitle = APPNAME_FULL;
	WindowTitle += L" v";
	WindowTitle += APP_VERSION;
	std::wstring CommandLineParameters;

	//commandline processing
	if (lpCmdLine)
	{
		std::wstring CommandLineParameters = lpCmdLine;
		transform(CommandLineParameters.begin(), CommandLineParameters.end(), CommandLineParameters.begin(), ::tolower);
		if (CommandLineParameters == L"-show")
			bDaemonVisible = true;
		else if (CommandLineParameters == L"-hide")
			bDaemonVisible = false;
		else if (CommandLineParameters == L"-firstrun")
		{
			bFirstRun = true;
			bDaemonVisible = false;
		}
		else
		{
			MessageBox(NULL, L"This application is the desktop user mode daemon for LGTV Companion, it runs in the background and provides additional features, e.g user idle detection. To manage your WebOS devices and options please instead start the 'LGTV Companion' application from your windows start menu.\n\nClick OK to close this message!", L"LGTV Companion (Daemon)", MB_OK);
			return 0;
		}
	}

	shellhookMessage = RegisterWindowMessageW(L"SHELLHOOK");
	//Initialize toast notifications
	WinToastLib::WinToast::WinToastError error;
	WinToastLib::WinToast::instance()->setAppName(L"LGTV Companion (Daemon)");
	const auto aumi = WinToastLib::WinToast::configureAUMI
	(L"LGTV Companion (Daemon)", L"", L"", L"");
	WinToastLib::WinToast::instance()->setAppUserModelId(aumi);
	if (!WinToastLib::WinToast::instance()->initialize(&error)) {
		wchar_t buf[250];
		swprintf_s(buf, L"Failed to initialize Toast Notifications :%d", error);
		Log(buf);
	}
	else
		Settings.ToastInitialised = true;

	// if the app is already running as another process, tell the other process to exit
	MessageExistingProcess();

	// Initiate PipeClient IPC
	ipc::PipeClient PipeCl(PIPENAME, NamedPipeCallback);
	pPipeClient = &PipeCl;

	// read the configuration file and init prefs
	try {
		Settings.Initialize();
	}
	catch (...) {
		CommunicateWithService("errorconfig");
		return false;
	}
	hBackbrush = CreateSolidBrush(0x00ffffff);

	// Get Session ID
	PULONG pSessionId = NULL;
	DWORD BytesReturned = 0;
	if (WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSSessionId, (LPSTR*)&pSessionId, &BytesReturned))
	{
		DWORD id = (DWORD)*pSessionId;
		WTSFreeMemory(pSessionId);
		sessionID = std::to_string(id);

	}

	// create main window (dialog)
	hMainWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)WndProc);
	SetWindowText(hMainWnd, WindowTitle.c_str());
	ShowWindow(hMainWnd, bDaemonVisible ? SW_SHOW : SW_HIDE);

	// spawn thread to check for updated version of the app.
	if (Settings.Prefs.AutoUpdate)
	{
		std::thread thread_obj(VersionCheckThread, hMainWnd);
		thread_obj.detach();
	}

	//Set a timer for the idle detection
	if (Settings.Prefs.BlankScreenWhenIdle)
	{
		SetTimer(hMainWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
		SetTimer(hMainWnd, TIMER_IDLE, Settings.Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
	}
	if (Settings.Prefs.AdhereTopology)
		SetTimer(hMainWnd, TIMER_TOPOLOGY, TIMER_TOPOLOGY_DELAY, (TIMERPROC)NULL);

	if (Settings.Prefs.RemoteStreamingCheck || Settings.Prefs.bIdleWhitelistEnabled)
		SetTimer(hMainWnd, TIMER_CHECK_PROCESSES, TIMER_CHECK_PROCESSES_DELAY, (TIMERPROC)NULL);

	std::wstring startupmess = WindowTitle;
	startupmess += L" is running.";
	Log(startupmess);
	CommunicateWithService("started");
	
	std::wstring sessionmessage = L"Session ID is: ";
	sessionmessage += common::widen(sessionID);
	Log(sessionmessage);

	HPOWERNOTIFY rsrn = RegisterSuspendResumeNotification(hMainWnd, DEVICE_NOTIFY_WINDOW_HANDLE);
	HPOWERNOTIFY rpsn = RegisterPowerSettingNotification(hMainWnd, &(GUID_CONSOLE_DISPLAY_STATE), DEVICE_NOTIFY_WINDOW_HANDLE);

	DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
	HDEVNOTIFY dev_notify = NULL;
	if (Settings.Prefs.RemoteStreamingCheck)
	{
		ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
		NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
		NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		memcpy(&(NotificationFilter.dbcc_classguid), &(GUID_DEVINTERFACE_USB_DEVICE), sizeof(struct _GUID));
		dev_notify = RegisterDeviceNotification(hMainWnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

		WTSRegisterSessionNotification(hMainWnd, NOTIFY_FOR_ALL_SESSIONS);

		Settings.Remote.Sunshine_Log_File = Sunshine_GetLogFile();
		if (Settings.Remote.Sunshine_Log_File != "")
		{
			std::filesystem::path p(Settings.Remote.Sunshine_Log_File);
			Settings.Remote.Sunshine_Log_Size = std::filesystem::file_size(p);
		}
	}

	// message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage(hMainWnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	PipeCl.Terminate();

	if (Settings.ToastInitialised)
	{
		if (idToastNewversion || idToastFirstrun)
		{
			if (idToastFirstrun)
				WinToastLib::WinToast::instance()->hideToast(idToastFirstrun);
			if (idToastNewversion)
				WinToastLib::WinToast::instance()->hideToast(idToastNewversion);
			Sleep(500);
			WinToastLib::WinToast::instance()->clear();
		}
	}
	DeleteObject(hBackbrush);
	UnregisterSuspendResumeNotification(rsrn);
	UnregisterPowerSettingNotification(rpsn);
	if (dev_notify)
		UnregisterDeviceNotification(dev_notify);
	if (Settings.Prefs.RemoteStreamingCheck)
		WTSUnRegisterSessionNotification(hMainWnd);
	return (int)msg.wParam;
}

//   Process messages for the main window.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	std::wstring str;
	switch (message)
	{
	case WM_INITDIALOG:
	{
		if (bFirstRun && Settings.ToastInitialised)
		{
			TCHAR buffer[MAX_PATH] = { 0 };
			GetModuleFileName(NULL, buffer, MAX_PATH);
			std::wstring imgpath = buffer;
			std::wstring::size_type pos = imgpath.find_last_of(L"\\/");
			imgpath = imgpath.substr(0, pos + 1);
			imgpath += L"mainicon.ico";

			std::wstring Toast;
			Toast = L"Good news! LGTV Companion v";
			Toast += APP_VERSION;
			Toast += L" is now installed.";

			WinToastLib::WinToastTemplate templ;
			templ = WinToastLib::WinToastTemplate(WinToastLib::WinToastTemplate::ImageAndText02);
			templ.setImagePath(imgpath);

			templ.setTextField(Toast, WinToastLib::WinToastTemplate::FirstLine);
			templ.setTextField(L"Designed to protect and extend the features of your WebOS devices when used as a PC monitor.", WinToastLib::WinToastTemplate::SecondLine);

			// Read the additional options section in the article
			templ.setDuration(WinToastLib::WinToastTemplate::Duration::Short);
			templ.setAudioOption(WinToastLib::WinToastTemplate::AudioOption::Default);
			idToastFirstrun = WinToastLib::WinToast::instance()->showToast(templ, &m_WinToastHandler);
			if (idToastFirstrun == -1L)
			{
				Log(L"Failed to show first run toast notification!");
			}
		}
	}break;
	case APP_NEW_VERSION:
	{
		if (!bFirstRun)
		{
			CommunicateWithService("newversion");

			std::wstring s = L"A new version of this app is available for download here: ";
			s += NEWRELEASELINK;
			Log(s);

			if (Settings.ToastInitialised)
			{
				TCHAR buffer[MAX_PATH] = { 0 };
				GetModuleFileName(NULL, buffer, MAX_PATH);
				std::wstring imgpath = buffer;
				std::wstring::size_type pos = imgpath.find_last_of(L"\\/");
				imgpath = imgpath.substr(0, pos + 1);
				imgpath += L"mainicon.ico";

				WinToastLib::WinToastTemplate templ;
				templ = WinToastLib::WinToastTemplate(WinToastLib::WinToastTemplate::ImageAndText02);
				templ.setImagePath(imgpath);

				templ.setTextField(L"Yay! A new version is available.", WinToastLib::WinToastTemplate::FirstLine);
				templ.setTextField(L"Please install the new version to keep up to date with bugfixes and features.", WinToastLib::WinToastTemplate::SecondLine);

				templ.addAction(L"Download");

				// Read the additional options section in the article
				templ.setDuration(WinToastLib::WinToastTemplate::Duration::Long);
				templ.setAudioOption(WinToastLib::WinToastTemplate::AudioOption::Default);
				idToastNewversion = WinToastLib::WinToast::instance()->showToast(templ, &m_WinToastHandler);
				if (idToastNewversion == -1L)
				{
					Log(L"Failed to show toast notification about updated version!");
				}
			}
		}
	}break;

	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDC_BUTTON1:
			{
				std::filesystem::path p(Settings.Remote.Sunshine_Log_File);
				uintmax_t len = std::filesystem::file_size(p);
			}break;
			default:break;
			}
		}break;
		default:break;
		}
	}break;

	case WM_NOTIFY:
	{
	}break;
	case APP_USER_IDLE_ON: //message sent from lgtv companion.exe
	{
		if (Settings.Prefs.BlankScreenWhenIdle)
		{
			ManualUserIdleMode = APP_USER_IDLE_ON;

			bIdle = true;
			bIdlePreventEarlyWakeup = false;
			bIdlePreventFalseBusy = false;
			SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_IDLE, (TIMERPROC)NULL);
			SetTimer(hWnd, TIMER_IDLE, Settings.Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
			Log(L"User forced user idle mode!");
			CommunicateWithService("useridle");
		}
		else
			Log(L"Can not force user idle mode, as the feature is not enabled in the global options!");
	}break;
	case APP_USER_IDLE_OFF: //message sent from lgtv companion.exe
	{
		if (Settings.Prefs.BlankScreenWhenIdle)
		{
			LASTINPUTINFO lii;
			lii.cbSize = sizeof(LASTINPUTINFO);
			if (GetLastInputInfo(&lii))
			{
				ManualUserIdleMode = 0;

				bIdle = false;
				bIdlePreventEarlyWakeup = false;
				bIdlePreventFalseBusy = false;
				SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
				SetTimer(hWnd, TIMER_IDLE, Settings.Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
				dwLastInputTick = lii.dwTime;
				Log(L"User forced unsetting user idle mode!");
				CommunicateWithService("userbusy");
			}
		}
		else
			Log(L"Can not force unset user idle mode, as the feature is not enabled in the global options!");
	}break;
	case WM_TIMER:
	{
		switch (wParam)
		{
		case TIMER_MAIN:
		{
			LASTINPUTINFO lii;
			lii.cbSize = sizeof(LASTINPUTINFO);
			if (GetLastInputInfo(&lii))
			{
				// do this first time the timer is triggered
				if (daemon_startup_user_input_time == 0)
					daemon_startup_user_input_time = lii.dwTime;

				//fix for the fullscreen idle detection on system startup because windows will return QUNS_BUSY until the user has interacted with the PC
				if (lii.dwTime != daemon_startup_user_input_time)
					daemon_startup_user_input_time = -1;

				if (bDaemonVisible)
				{
//					std::wstring tick = common::widen(std::to_string(lii.dwTime));
					DWORD time = (GetTickCount() - (dwProcessedLastInputTick != 0 ? dwProcessedLastInputTick : lii.dwTime)) / 1000;
					std::wstring ago = common::widen(std::to_string(time));

					//					SendMessage(GetDlgItem(hMainWnd, IDC_EDIT2), WM_SETTEXT, 0, (WPARAM)tick.c_str());
					SendMessage(GetDlgItem(hMainWnd, IDC_EDIT3), WM_SETTEXT, 0, (WPARAM)ago.c_str());
				}
				if (bIdle)
				{
					if (lii.dwTime != dwLastInputTick) // was there user input during the last check interval (100ms)?
					{
						if (!bIdlePreventEarlyWakeup) // don't do anything unless user input for two consective intervals
						{
							dwLastInputTick = lii.dwTime;
							bIdlePreventEarlyWakeup = true;
						}
						else
						{
							ManualUserIdleMode = 0;
							SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
							SetTimer(hWnd, TIMER_IDLE, Settings.Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
							dwLastInputTick = lii.dwTime;
							dwProcessedLastInputTick = dwLastInputTick;
							bIdlePreventEarlyWakeup = false;
							bIdle = false;
							CommunicateWithService("userbusy");
						}
					}
					else
						bIdlePreventEarlyWakeup = false;
					bIdlePreventFalseBusy = false;
				}
				else
				{
					if (lii.dwTime != dwLastInputTick) // was there user input during the last check interval (1000ms)?
					{

						if (!bIdlePreventFalseBusy) // don't do anything unless user input for two consective intervals
						{
							dwLastInputTick = lii.dwTime;
							bIdlePreventFalseBusy = true;
						}
						else
						{
							SetTimer(hWnd, TIMER_IDLE, Settings.Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
							dwLastInputTick = lii.dwTime;
							dwProcessedLastInputTick = dwLastInputTick;
							bIdlePreventFalseBusy = false;
						}
					}
					else
						bIdlePreventFalseBusy = false;
					bIdlePreventEarlyWakeup = false;
				}
			}
			return 0;
		}break;

		case TIMER_IDLE:
		{
			if (ManualUserIdleMode == 0)
			{
				//Is a whitelisted process running
				if (Settings.Prefs.bIdleWhitelistEnabled)
				{
					if (Settings.Remote.sCurrentlyRunningWhitelistedProcess != L"")
					{
						std::wstring mess = L"Whitelisted application is prohibiting user idle mode(";
						mess += Settings.Remote.sCurrentlyRunningWhitelistedProcess;
						mess += L")";
						Log(mess);
						return 0;
					}
				}
				// fullscreen fix for first boot
				// fullsceen detection routine
				if(daemon_startup_user_input_time == -1)
				{
					// UIM is disabled during fullsceen
					if (Settings.Prefs.bFullscreenCheckEnabled)
					{
						if (FullscreenApplicationRunning())
						{
							Log(L"Fullscreen application is currently prohibiting user idle mode");
							return 0;
						}
					}
					else // UIM is enabled during fullscreen
					{
						//Is an excluded fullscreen process running?
						if(Settings.Prefs.bIdleFsExclusionsEnabled && Settings.Remote.sCurrentlyRunningFsExcludedProcess != L"")
						{
							if (FullscreenApplicationRunning())
							{
								std::wstring mess = L"Fullscreen excluded process is currently prohibiting idle (";
								mess += Settings.Remote.sCurrentlyRunningFsExcludedProcess;
								mess += L")";
								Log(mess);
								return 0;
							}
						}					
					}
				}
			}

			bIdle = true;
			bIdlePreventEarlyWakeup = false;
			bIdlePreventFalseBusy = false;

			SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_IDLE, (TIMERPROC)NULL);

			CommunicateWithService("useridle");

			return 0;
		}break;
		case TIMER_CHECK_PROCESSES:
		{
			DWORD dw = CheckRemoteStreamingProcesses();
			if (dw)
				RemoteStreamingEvent(dw);
		}break;
		case REMOTE_CONNECT:
		{
			KillTimer(hWnd, (UINT_PTR)REMOTE_CONNECT);
			CommunicateWithService("remote_connect");
		}break;
		case REMOTE_DISCONNECT:
		{
			KillTimer(hWnd, (UINT_PTR)REMOTE_DISCONNECT);
			CommunicateWithService("remote_disconnect");
		}break;
		case TIMER_TOPOLOGY_COLLECTION:
		{
			KillTimer(hWnd, (UINT_PTR)TIMER_TOPOLOGY_COLLECTION);
			PostMessage(hWnd, USER_DISPLAYCHANGE, NULL, NULL);
		}break;
		case TIMER_TOPOLOGY:
		{
			KillTimer(hWnd, TIMER_TOPOLOGY);
			if (!Settings.Prefs.AdhereTopology)
				break;

			int result = VerifyTopology();

			if (result == TOPOLOGY_OK)
			{
				PostMessage(hWnd, USER_DISPLAYCHANGE, NULL, NULL);
			}
			else if (result == TOPOLOGY_UNDETERMINED)
			{
				Log(L"Warning! No active devices detected when verifying Windows Monitor Topology.");
				CommunicateWithService("topology undetermined");
//				Prefs.AdhereTopology = false;
			}
			else if (result == TOPOLOGY_OK_DISABLE)
			{
				Settings.Prefs.AdhereTopology = false;
			}
			else if (result == TOPOLOGY_ERROR)
			{
				Settings.Prefs.AdhereTopology = false;
				CommunicateWithService("topology invalid");

				std::wstring s = L"A change to the system has invalidated the monitor topology configuration and the feature has been disabled. "
					"Please run the configuration guide in the global options to ensure correct operation.";
				Log(s);

				if (Settings.ToastInitialised)
				{
					TCHAR buffer[MAX_PATH] = { 0 };
					GetModuleFileName(NULL, buffer, MAX_PATH);
					std::wstring imgpath = buffer;
					std::wstring::size_type pos = imgpath.find_last_of(L"\\/");
					imgpath = imgpath.substr(0, pos + 1);
					imgpath += L"mainicon.ico";

					WinToastLib::WinToastTemplate templ;
					templ = WinToastLib::WinToastTemplate(WinToastLib::WinToastTemplate::ImageAndText02);
					templ.setImagePath(imgpath);

					templ.setTextField(L"Invalidated monitor topology configuration!", WinToastLib::WinToastTemplate::FirstLine);
					templ.setTextField(L"A change to the system has invalidated the multi-monitor configuration. Please run the configuration guide in the global options.", WinToastLib::WinToastTemplate::SecondLine);

					// Read the additional options section in the article
					templ.setDuration(WinToastLib::WinToastTemplate::Duration::Long);
					templ.setAudioOption(WinToastLib::WinToastTemplate::AudioOption::Default);
					idToastNewversion = WinToastLib::WinToast::instance()->showToast(templ, &m_WinToastHandler);
					if (idToastNewversion == -1L)
					{
						Log(L"Failed to show toast notification about invalidated topology configuration!");
					}
				}
			}
			return 0;
		}break;
		default:break;
		}
	}break;
	case WM_POWERBROADCAST:
	{
		switch (wParam)
		{
		case PBT_APMSUSPEND:
		{
			KillTimer(hWnd, TIMER_MAIN);
			KillTimer(hWnd, TIMER_IDLE);
			bIdle = false;
			dwLastInputTick = 0;
			bIdlePreventEarlyWakeup = false;
			bIdlePreventFalseBusy = false;
			Log(L"Suspending system.");
			return true;
		}break;
		case PBT_APMRESUMEAUTOMATIC:
		{
			bIdle = false;
			dwLastInputTick = 0;
			bIdlePreventEarlyWakeup = false;
			bIdlePreventFalseBusy = false;
			if (Settings.Prefs.BlankScreenWhenIdle)
			{
				SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
				SetTimer(hWnd, TIMER_IDLE, Settings.Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
			}
			Log(L"Resuming system.");
			return true;
		}break;
		case PBT_POWERSETTINGCHANGE:
		{
			if (lParam)
			{
				POWERBROADCAST_SETTING* PBS = NULL;
				PBS = (POWERBROADCAST_SETTING*)lParam;
				if (PBS->PowerSetting == GUID_CONSOLE_DISPLAY_STATE)
				{
					if (PBS->Data[0] == 0)
					{
						Log(L"System requests displays OFF.");
					}
					else if (PBS->Data[0] == 2)
					{
						Log(L"System requests displays OFF(DIMMED).");
					}
					else
					{
						bIdle = false;
						dwLastInputTick = 0;
						bIdlePreventEarlyWakeup = false;
						bIdlePreventFalseBusy = false;
						if (Settings.Prefs.BlankScreenWhenIdle)
						{
							SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
							SetTimer(hWnd, TIMER_IDLE, Settings.Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
						}
						Log(L"System requests displays ON.");
					}
					return true;
				}
			}
		}break;

		default:break;
		}
	}break;
	case USER_DISPLAYCHANGE:
	{
		TimeOfLastTopologyChange = time(0);
		CheckDisplayTopology();
	}break;

	case WM_DISPLAYCHANGE:
	{
		if (Settings.Prefs.AdhereTopology)
		{
			time_t now = time(0);
			if (now - TimeOfLastTopologyChange > 10)
			{
				PostMessage(hWnd, USER_DISPLAYCHANGE, NULL, NULL);
			}
			else
			{
				TimeOfLastTopologyChange = now;
				SetTimer(hWnd, TIMER_TOPOLOGY_COLLECTION, TIMER_TOPOLOGY_COLLECTION_DELAY, (TIMERPROC)NULL);
			}
		}
	}break;

	case WM_DEVICECHANGE:
	{
		if (Settings.Prefs.RemoteStreamingCheck && lParam)
		{
			PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR)lParam;
			PDEV_BROADCAST_DEVICEINTERFACE lpdbv = (PDEV_BROADCAST_DEVICEINTERFACE)lpdb;
			std::wstring path;
			if (lpdb->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
			{
				path = std::wstring(lpdbv->dbcc_name);
				transform(path.begin(), path.end(), path.begin(), ::tolower);

				switch (wParam)
				{
				case DBT_DEVICEARRIVAL:
				{
					for (auto& dev : Settings.Remote.stream_usb_list_gamestream)
					{
						if (path.find(dev, 0) != std::wstring::npos)
						{
							RemoteStreamingEvent(REMOTE_NVIDIA_CONNECTED);
							return true;
						}
					}
				}break;
				case DBT_DEVICEREMOVECOMPLETE:
				{
					for (auto& dev : Settings.Remote.stream_usb_list_gamestream)
					{
						if (path.find(dev, 0) != std::wstring::npos)
						{
							RemoteStreamingEvent(REMOTE_NVIDIA_NOT_CONNECTED);
							return true;
						}
					}
				}break;
				default:break;
				}
			}
		}
	}break;

	case WM_WTSSESSION_CHANGE:
	{
		if (wParam == WTS_REMOTE_CONNECT)
		{
			RemoteStreamingEvent(REMOTE_RDP_CONNECTED);
		}
		else if (wParam == WTS_REMOTE_DISCONNECT)
		{
			RemoteStreamingEvent(REMOTE_RDP_NOT_CONNECTED);
		}
	}break;
	case WM_COPYDATA:
	{
		if (!lParam)
			return true;
		COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)lParam;
		if (pcds->dwData == NOTIFY_NEW_PROCESS)
		{
			SendMessage(hWnd, WM_CLOSE, NULL, NULL);
		}
		return true;
	}break;
	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
//		if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_ENABLE))
		{
			SetBkMode(hdcStatic, TRANSPARENT);
		}
		return(INT_PTR)hBackbrush;
	}break;
	case WM_PAINT:
	{
		RECT rc = { 0 };
		GetClientRect(hWnd, &rc);
		PAINTSTRUCT ps;
		//		PAINTSTRUCT psPaint;
		InvalidateRect(hWnd, NULL, false);

		//		GetClientRect(hWnd, &rc);
		int width = rc.right;
		int height = rc.bottom;
		HDC hdc = BeginPaint(hWnd, &ps);
		HDC backbuffDC = CreateCompatibleDC(hdc);
		HBITMAP backbuffer = CreateCompatibleBitmap(hdc, width, height);
		int savedDC = SaveDC(backbuffDC);
		SelectObject(backbuffDC, backbuffer);

		FillRect(backbuffDC, &rc, (HBRUSH)hBackbrush);

		BitBlt(hdc, 0, 0, width, height, backbuffDC, 0, 0, SRCCOPY);
		RestoreDC(backbuffDC, savedDC);

		DeleteObject(backbuffer);
		DeleteDC(backbuffDC);

		ReleaseDC(hWnd, hdc);
		EndPaint(hWnd, &ps);
		return 0;
	}break;
	case WM_ERASEBKGND:
	{
		return true;
	}break;
	case WM_CLOSE:
	{
		DestroyWindow(hWnd);
	}break;
	case WM_ENDSESSION: //PC is shutting down so let's do some cleaning up
	{
		if (Settings.ToastInitialised)
		{
			if (idToastNewversion || idToastFirstrun)
			{
				if (idToastFirstrun)
					WinToastLib::WinToast::instance()->hideToast(idToastFirstrun);
				if (idToastNewversion)
					WinToastLib::WinToast::instance()->hideToast(idToastNewversion);
				Sleep(500);
				WinToastLib::WinToast::instance()->clear();
			}
		}
	}break;

	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}break;
	default:
		return false;
	}
	return true;
}

//   If the application is already running, tell other process to exit
bool MessageExistingProcess(void)
{
	std::wstring WindowTitle;
	WindowTitle = APPNAME_FULL;
	WindowTitle += L" v";
	WindowTitle += APP_VERSION;
	bool return_value = false;

	HWND Other_hWnd = FindWindow(NULL, WindowTitle.c_str());

	while (Other_hWnd)
	{
		COPYDATASTRUCT cds;
		cds.cbData = sizeof(WCHAR);
		cds.lpData = NULL;
		cds.dwData = NOTIFY_NEW_PROCESS;
		SendMessage(Other_hWnd, WM_COPYDATA, NULL, (LPARAM)&cds);
		Sleep(100);
		Other_hWnd = FindWindow(NULL, WindowTitle.c_str());
		return_value =  true;
	}
	return return_value;
}
//   communicate with service via IPC
void CommunicateWithService(std::string sData)
{
	if (sData.size() == 0)
		return;
	std::string message = "-daemon ";
	if (sessionID.size() > 0)
	{
		message += sessionID;
		message += " ";
	}
	else
	{
		message += "unknown_id";
		message += " ";
	}
	message += sData;
	if (!pPipeClient->Send(common::widen(message)))
		Log(L"Failed to connect to named pipe. Service may be stopped.");
	else
		Log(common::widen(message));
	return;
}

void Log(std::wstring input)
{
	time_t rawtime;
	struct tm timeinfo;
	char buffer[80];

	if (!bDaemonVisible)
		return;

	std::time(&rawtime);
	localtime_s(&timeinfo, &rawtime);

	strftime(buffer, 80, "%a %H:%M:%S > ", &timeinfo);
	puts(buffer);

	std::wstring logmess = common::widen(buffer);
	logmess += input;
	logmess += L"\r\n";

	int TextLen = (int)SendMessage(GetDlgItem(hMainWnd, IDC_EDIT), WM_GETTEXTLENGTH, 0, 0);
	SendMessage(GetDlgItem(hMainWnd, IDC_EDIT), EM_SETSEL, (WPARAM)TextLen, (LPARAM)TextLen);
	SendMessage(GetDlgItem(hMainWnd, IDC_EDIT), EM_REPLACESEL, FALSE, (LPARAM)logmess.c_str());

	return;
}

void VersionCheckThread(HWND hWnd)
{
	IStream* stream;
	char buff[100];
	std::string s;
	unsigned long bytesRead;

	if (URLOpenBlockingStream(0, VERSIONCHECKLINK, &stream, 0, 0))
		return;// error

	while (true)
	{
		stream->Read(buff, 100, &bytesRead);

		if (0U == bytesRead)
			break;
		s.append(buff, bytesRead);
	};

	stream->Release();

	size_t find = s.find("\"tag_name\":", 0);
	if (find != std::string::npos)
	{
		size_t begin = s.find_first_of("0123456789", find);
		if (begin != std::string::npos)
		{
			size_t end = s.find("\"", begin);
			std::string lastver = s.substr(begin, end - begin);

			std::vector <std::string> local_ver = common::stringsplit(common::narrow(APP_VERSION), ".");
			std::vector <std::string> remote_ver = common::stringsplit(lastver, ".");

			if (local_ver.size() < 3 || remote_ver.size() < 3)
				return;
			int local_ver_major = atoi(local_ver[0].c_str());
			int local_ver_minor = atoi(local_ver[1].c_str());
			int local_ver_patch = atoi(local_ver[2].c_str());

			int remote_ver_major = atoi(remote_ver[0].c_str());
			int remote_ver_minor = atoi(remote_ver[1].c_str());
			int remote_ver_patch = atoi(remote_ver[2].c_str());

			if ((remote_ver_major > local_ver_major) ||
				(remote_ver_major == local_ver_major) && (remote_ver_minor > local_ver_minor) ||
				(remote_ver_major == local_ver_major) && (remote_ver_minor == local_ver_minor) && (remote_ver_patch > local_ver_patch))
			{
				PostMessage(hWnd, APP_NEW_VERSION, 0, 0);
			}
		}
	}
	return;
}

// get a vector of all currently active LG displays
std::vector<settings::DISPLAY_INFO> QueryDisplays()
{
	std::vector<settings::DISPLAY_INFO> targets;

	//populate targets struct with information about attached displays
	EnumDisplayMonitors(NULL, NULL, meproc, (LPARAM)&targets);

	return targets;
}
static BOOL CALLBACK meproc(HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM pData)
{
	if (!pData)
		return false;
	std::vector<settings::DISPLAY_INFO>* targets = (std::vector<settings::DISPLAY_INFO> *) pData;
	UINT32 requiredPaths, requiredModes;
	std::vector<DISPLAYCONFIG_PATH_INFO> paths;
	std::vector<DISPLAYCONFIG_MODE_INFO> modes;
	MONITORINFOEX mi;
	LONG isError = ERROR_INSUFFICIENT_BUFFER;

	ZeroMemory(&mi, sizeof(mi));
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);

	isError = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, &requiredModes);
	if (isError)
	{
		targets->clear();
		return false;
	}
	paths.resize(requiredPaths);
	modes.resize(requiredModes);

	isError = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, paths.data(), &requiredModes, modes.data(), NULL);
	if (isError)
	{
		targets->clear();
		return false;
	}
	paths.resize(requiredPaths);
	modes.resize(requiredModes);

	for (auto& p : paths)
	{
		DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
		sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
		sourceName.header.size = sizeof(sourceName);
		sourceName.header.adapterId = p.sourceInfo.adapterId;
		sourceName.header.id = p.sourceInfo.id;

		DisplayConfigGetDeviceInfo(&sourceName.header);
		if (wcscmp(mi.szDevice, sourceName.viewGdiDeviceName) == 0)
		{
			DISPLAYCONFIG_TARGET_DEVICE_NAME name;
			name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
			name.header.size = sizeof(name);
			name.header.adapterId = p.sourceInfo.adapterId;
			name.header.id = p.targetInfo.id;
			DisplayConfigGetDeviceInfo(&name.header);
			std::wstring FriendlyName = name.monitorFriendlyDeviceName;
			transform(FriendlyName.begin(), FriendlyName.end(), FriendlyName.begin(), ::tolower);
			if (FriendlyName.find(L"lg tv") != std::wstring::npos)
			{
				settings::DISPLAY_INFO di;
				di.monitorinfo = mi;
				di.hMonitor = hMonitor;
				di.hdcMonitor = hdc;
				di.rcMonitor2 = *(LPRECT)lprcMonitor;
				di.target = name;
				targets->push_back(di);
			}
		}
	}
	return true;
}
bool CheckDisplayTopology(void)
{
	std::stringstream s;
	s << "topology state ";
	std::vector<settings::DISPLAY_INFO> displays = QueryDisplays();
	if (Settings.Devices.size() == 0)
		return false;
	if (displays.size() > 0)
	{
		for (auto& disp : displays)
		{
			for (auto& dev : Settings.Devices)
			{
				std::string ActiveDisplay = common::narrow(disp.target.monitorDevicePath);
				std::string DeviceString = dev.UniqueDeviceKey;
				transform(ActiveDisplay.begin(), ActiveDisplay.end(), ActiveDisplay.begin(), ::tolower);
				transform(DeviceString.begin(), DeviceString.end(), DeviceString.begin(), ::tolower);
				if (ActiveDisplay == DeviceString)
				{
					s << dev.DeviceId << " ";
				}
			}
		}
	}
	CommunicateWithService(s.str());
	return true;
}

int VerifyTopology(void)
{
	bool match = false;

	if (!Settings.Prefs.AdhereTopology)
		return TOPOLOGY_OK_DISABLE;
	if (Settings.Devices.size() == 0)
		return TOPOLOGY_OK_DISABLE;

	std::vector<settings::DISPLAY_INFO> displays = QueryDisplays();
	if (displays.size() == 0)
		return TOPOLOGY_UNDETERMINED;

	for (auto& disp : displays)
	{
		match = false;

		for (auto& dev : Settings.Devices)
		{
			std::string ActiveDisplay = common::narrow(disp.target.monitorDevicePath);
			std::string DeviceString = dev.UniqueDeviceKey;
			transform(ActiveDisplay.begin(), ActiveDisplay.end(), ActiveDisplay.begin(), ::tolower);
			transform(DeviceString.begin(), DeviceString.end(), DeviceString.begin(), ::tolower);
			if (ActiveDisplay == DeviceString)
				match = true;
		}
		if (!match)
			return TOPOLOGY_ERROR;
	}
	return TOPOLOGY_OK;
}

DWORD CheckRemoteStreamingProcesses(void)
{
	bool bWhiteListConfigured = Settings.Prefs.WhiteList.size() > 0 ? true : false;
	bool bFsExclusionListConfigured = Settings.Prefs.FsExclusions.size() > 0 ? true : false;
	bool bWhitelistProcessFound = false;
	bool bFsExclusionProcessFound = false;
	bool bStreamingProcessFound = false;
	bool bSunshineSvcProcessFound = false;
	std::wstring sWhitelistProcessFound = L"";
	std::wstring sFsExclusionProcessFound = L"";
	DWORD ReturnValue = 0;

	// Iterate over currently running processes
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (!Process32First(snapshot, &entry)) {
		CloseHandle(snapshot);
		Log(L"Failed to iterate running processes");
		return NULL;
	}
	do
	{
		// look for user idle mode whitelisted processes
		if (Settings.Prefs.bIdleWhitelistEnabled && bWhiteListConfigured && !bWhitelistProcessFound)
			for (auto& w : Settings.Prefs.WhiteList)
				if (w.Application != L"")
					if (!_tcsicmp(entry.szExeFile, w.Application.c_str()))
					{
						if (w.Name != L"")
							sWhitelistProcessFound = w.Name;
						else
							sWhitelistProcessFound = L"<unnamed>";
						bWhitelistProcessFound = true;
					}
		// look for user idle mode fullscreen excluded processes
		if (Settings.Prefs.bIdleFsExclusionsEnabled && bFsExclusionListConfigured && !bFsExclusionProcessFound)
			for (auto& w : Settings.Prefs.FsExclusions)
				if (w.Application != L"")
					if (!_tcsicmp(entry.szExeFile, w.Application.c_str()))
					{
						if (w.Name != L"")
							sFsExclusionProcessFound = w.Name;
						else
							sFsExclusionProcessFound = L"<unnamed>";
						bFsExclusionProcessFound = true;
					}

		// look for currently running known streaming processes
		if (Settings.Prefs.RemoteStreamingCheck && !bStreamingProcessFound)
			for (auto& w : Settings.Remote.stream_proc_list)
				if (!_tcsicmp(entry.szExeFile, w.c_str()))
					bStreamingProcessFound = true;

		// look for currently running sushine service process
		if (Settings.Prefs.RemoteStreamingCheck && !bSunshineSvcProcessFound)
			if (!_tcsicmp(entry.szExeFile, SUNSHINE_FILE_SVC))
				bSunshineSvcProcessFound = true;
	} while (!(bStreamingProcessFound && bWhitelistProcessFound && bSunshineSvcProcessFound && bFsExclusionProcessFound) && Process32Next(snapshot, &entry));

	CloseHandle(snapshot);

	// was sunshine service found currently running?
	if (bSunshineSvcProcessFound && (Settings.Remote.Sunshine_Log_File != ""))
	{
		std::filesystem::path p(Settings.Remote.Sunshine_Log_File);
		uintmax_t Size = std::filesystem::file_size(p);
		if (Size != Settings.Remote.Sunshine_Log_Size)
		{
			ReturnValue ^= Sunshine_CheckLog();
			Settings.Remote.Sunshine_Log_Size = Size;
		}
	}
	else
		ReturnValue ^= REMOTE_SUNSHINE_NOT_CONNECTED;

	// was a remote streaming process currently running?
	ReturnValue ^= bStreamingProcessFound ? REMOTE_STEAM_CONNECTED : REMOTE_STEAM_NOT_CONNECTED;

	// was a user idle mode whitelisted / FS excluded process currently running?
	Settings.Remote.sCurrentlyRunningWhitelistedProcess = sWhitelistProcessFound;
	Settings.Remote.sCurrentlyRunningFsExcludedProcess = sFsExclusionProcessFound;
	return ReturnValue;
}
bool FullscreenApplicationRunning(void)
{
	WorkaroundFalseFullscreenWindows();

	QUERY_USER_NOTIFICATION_STATE pquns;

	if (SHQueryUserNotificationState(&pquns) == S_OK)
	{
		if (pquns == QUNS_RUNNING_D3D_FULL_SCREEN ||
			pquns == QUNS_PRESENTATION_MODE ||
			pquns == QUNS_BUSY ||
			pquns == QUNS_APP)
		{
			return true;
		}
	}
	return false;
}
void WorkaroundFalseFullscreenWindows(void)
{
	EnumWindows(EnumWindowsProc, 0);
}
static BOOL	CALLBACK	EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
	if (!IsWindowVisible(hWnd))
		return true;

	const wchar_t* const nonRude = L"NonRudeHWND";
	{
		std::wstring window_name = common::GetWndText(hWnd);
		transform(window_name.begin(), window_name.end(), window_name.begin(), ::tolower);
		if (window_name.find(L"nvidia geforce overlay") != std::wstring::npos)
		{
			if (GetProp(hWnd, nonRude) == NULL)
			{
				if (SetProp(hWnd, nonRude, INVALID_HANDLE_VALUE))
				{
					DWORD recipients = BSM_APPLICATIONS;
					if (BroadcastSystemMessage(BSF_POSTMESSAGE | BSF_IGNORECURRENTTASK, &recipients, shellhookMessage, HSHELL_UNDOCUMENTED_FULLSCREEN_EXIT, (LPARAM)hWnd) < 0)
					{
						Log(L"BroadcastSystemMessage() failed");
					}
					else {
						CommunicateWithService("gfe");
						Log(L"Unset NVIDIA GFE overlay fullscreen");
					}

					return false;
				}
			}
		}
	}
	return true;
}
void RemoteStreamingEvent(DWORD dwType)
{
	if (Settings.Prefs.RemoteStreamingCheck)
	{
		bool bCurrentlyConnected = Settings.Remote.bRemoteCurrentStatusSteam || Settings.Remote.bRemoteCurrentStatusNvidia || Settings.Remote.bRemoteCurrentStatusRDP || Settings.Remote.bRemoteCurrentStatusSunshine;

		if (dwType & REMOTE_STEAM_CONNECTED)
		{
			if (!Settings.Remote.bRemoteCurrentStatusSteam)
			{
				Log(L"Steam gamestream connected.");
				Settings.Remote.bRemoteCurrentStatusSteam = true;
			}
		}
		else if (dwType & REMOTE_STEAM_NOT_CONNECTED)
		{
			if (Settings.Remote.bRemoteCurrentStatusSteam)
			{
				Log(L"Steam gamestream disconnected.");
				Settings.Remote.bRemoteCurrentStatusSteam = false;
			}
		}
		if (dwType & REMOTE_NVIDIA_CONNECTED)
		{
			if (!Settings.Remote.bRemoteCurrentStatusNvidia)
			{
				Log(L"nVidia gamestream connected.");
				Settings.Remote.bRemoteCurrentStatusNvidia = true;
			}
		}
		else if (dwType & REMOTE_NVIDIA_NOT_CONNECTED)
		{
			if (Settings.Remote.bRemoteCurrentStatusNvidia)
			{
				Log(L"nVidia gamestream disconnected.");
				Settings.Remote.bRemoteCurrentStatusNvidia = false;
			}
		}
		if (dwType & REMOTE_SUNSHINE_CONNECTED)
		{
			if (!Settings.Remote.bRemoteCurrentStatusSunshine)
			{
				Log(L"Sunshine gamestream connected.");
				Settings.Remote.bRemoteCurrentStatusSunshine = true;
			}
		}
		else if (dwType & REMOTE_SUNSHINE_NOT_CONNECTED)
		{
			if (Settings.Remote.bRemoteCurrentStatusSunshine)
			{
				Log(L"Sunshine gamestream disconnected.");
				Settings.Remote.bRemoteCurrentStatusSunshine = false;
			}
		}
		if (dwType & REMOTE_RDP_CONNECTED)
		{
			if (!Settings.Remote.bRemoteCurrentStatusRDP)
			{
				Log(L"RDP connected.");
				Settings.Remote.bRemoteCurrentStatusRDP = true;
			}
		}
		else if (dwType & REMOTE_RDP_NOT_CONNECTED)
		{
			if (Settings.Remote.bRemoteCurrentStatusRDP)
			{
				Log(L"RDP disconnected.");
				Settings.Remote.bRemoteCurrentStatusRDP = false;
			}
		}
		bool bConnect = Settings.Remote.bRemoteCurrentStatusSteam || Settings.Remote.bRemoteCurrentStatusNvidia || Settings.Remote.bRemoteCurrentStatusRDP || Settings.Remote.bRemoteCurrentStatusSunshine;

		if (bCurrentlyConnected && !bConnect)
			SetTimer(hMainWnd, (UINT_PTR)REMOTE_DISCONNECT, 1000, (TIMERPROC)NULL);
		else if (!bCurrentlyConnected && bConnect)
			SetTimer(hMainWnd, (UINT_PTR)REMOTE_CONNECT, TIMER_REMOTE_DELAY, (TIMERPROC)NULL);
	}
	return;
}

DWORD Sunshine_CheckLog(void)
{
	std::string log;
	std::ifstream t(Settings.Remote.Sunshine_Log_File);
	if (t.is_open())
	{
		std::stringstream buffer;
		t.seekg(-5000, std::ios_base::end);
		buffer << t.rdbuf();
		std::string log = buffer.str();
		t.close();

		int type;
		size_t pos_connect = log.rfind("CLIENT CONNECTED");
		size_t pos_disconnect = log.rfind("CLIENT DISCONNECTED");
		if (pos_connect == std::string::npos && pos_disconnect == std::string::npos)
			return 0;
		else if (pos_connect == std::string::npos)
			type = REMOTE_SUNSHINE_NOT_CONNECTED;
		else if (pos_disconnect == std::string::npos)
			type = REMOTE_SUNSHINE_CONNECTED;
		else
			type = (pos_connect > pos_disconnect) ? REMOTE_SUNSHINE_CONNECTED : REMOTE_SUNSHINE_NOT_CONNECTED;

		size_t begin = log.rfind("[", type == REMOTE_SUNSHINE_CONNECTED ? pos_connect : pos_disconnect);
		std::string time = log.substr(begin + 1, 19); //2023:02:02:00:04:38
		time[4] = '-';
		time[7] = '-';
		time[10] = ' ';
		time += ".000"; //2023-02-02 00:04:38.000
		std::tm tmTime = boost::posix_time::to_tm(boost::posix_time::time_from_string(time));

		time_t entry_time = mktime(&tmTime);
		time_t now_time = std::time(0);
		double seconds = difftime(now_time, entry_time);
		if (seconds < (DOUBLE)10)
			return type;
	}
	return 0;
}

std::string Sunshine_GetLogFile()
{
	std::string Sunshine_Config;
	std::string configuration_path;
	std::string configuration_text;
	std::string log_file;

	HKEY hKey = NULL;
	LPCSTR pszSubkey = SUNSHINE_REG;
	LPCSTR pszValueName = "";
	//Get sunshine install path from registry
	if (RegOpenKeyA(HKEY_LOCAL_MACHINE, pszSubkey, &hKey) == ERROR_SUCCESS)
	{
		// Buffer to store string read from registry
		CHAR szValue[MAX_PATH];
		DWORD cbValueLength = sizeof(szValue);

		// Query string value
		if (RegQueryValueExA(hKey, pszValueName, NULL, NULL, reinterpret_cast<LPBYTE>(&szValue), &cbValueLength) == ERROR_SUCCESS)
		{
			configuration_path = szValue;
			configuration_path += "\\config\\";
			Sunshine_Config = configuration_path;
			Sunshine_Config += SUNSHINE_FILE_CONF;

			//open the configuration file
			std::ifstream t(Sunshine_Config);
			if (t.is_open())
			{
				std::stringstream buffer;
				buffer << t.rdbuf();
				configuration_text = buffer.str();
				t.close();

				std::string s = Sunshine_GetConfVal(configuration_text, "min_log_level");
				if (s != "" && s != "2" && s != "1" && s != "0")
				{
					Log(L"Logging need to be at minimum on level \"info\" in Sunshine.");
					return "";
				}

				s = Sunshine_GetConfVal(configuration_text, "log_path");
				if (s == "") //DEFAULT
				{
					log_file = configuration_path;
					log_file += SUNSHINE_FILE_LOG;
				}
				else
				{
					std::filesystem::path log_p(s);
					std::filesystem::current_path(configuration_path);
					std::filesystem::path log_abs(std::filesystem::absolute(log_p));
					log_file = log_abs.string();
				}
				return log_file;
			}
		}
	}
	return "";
}

std::string Sunshine_GetConfVal(std::string buf, std::string conf_item)
{
	conf_item += " = ";
	size_t pos = buf.find(conf_item);
	if (pos != std::string::npos)
	{
		size_t newl = buf.find("\n", pos);
		if (newl == std::string::npos)
			return buf.substr(pos + conf_item.length());
		else
			return buf.substr(pos + conf_item.length(), newl - (pos + conf_item.length()));
	}
	return "";
}

void NamedPipeCallback(std::wstring message)
{
	return;
}