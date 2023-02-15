#include "Daemon.h"

using namespace std;
using namespace jpersson77;
using namespace WinToastLib;

// Globals:
HINSTANCE                       hInstance;  // current instance
HWND                            hMainWnd = NULL;
bool                            bIdle = false;
bool                            bIdlePreventEarlyWakeup = false;
bool                            bDaemonVisible = true;
bool                            bFirstRun = false;
DWORD                           dwLastInputTick = 0;
WinToastHandler                 m_WinToastHandler;
HANDLE                          hPipe = INVALID_HANDLE_VALUE;
INT64                           idToastFirstrun = NULL;
INT64                           idToastNewversion = NULL;
UINT							shellhookMessage;
DWORD							daemon_startup_user_input_time = 0;
UINT							ManualUserIdleMode = 0;
HBRUSH                          hBackbrush;
time_t							TimeOfLastTopologyChange = 0;

settings::Preferences			Prefs;

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
	wstring WindowTitle;
	WindowTitle = APPNAME_FULL;
	WindowTitle += L" v";
	WindowTitle += APP_VERSION;
	wstring CommandLineParameters;

	//commandline processing
	if (lpCmdLine)
	{
		wstring CommandLineParameters = lpCmdLine;
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
	WinToast::WinToastError error;
	WinToast::instance()->setAppName(L"LGTV Companion (Daemon)");
	const auto aumi = WinToast::configureAUMI
	(L"LGTV Companion (Daemon)", L"", L"", L"");
	WinToast::instance()->setAppUserModelId(aumi);
	if (!WinToast::instance()->initialize(&error)) {
		wchar_t buf[250];
		swprintf_s(buf, L"Failed to initialize Toast Notifications :%d", error);
		Log(buf);
	}
	else
		Prefs.ToastInitialised = true;

	// if the app is already running as another process, tell the other process to exit
	MessageExistingProcess();

	// read the configuration file and init prefs
	try {
		Prefs.Initialize();
	}
	catch (...) {
		CommunicateWithService("-daemon errorconfig");
		return false;
	}
	hBackbrush = CreateSolidBrush(0x00ffffff);

	// create main window (dialog)
	hMainWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)WndProc);
	SetWindowText(hMainWnd, WindowTitle.c_str());
	ShowWindow(hMainWnd, bDaemonVisible ? SW_SHOW : SW_HIDE);

	// spawn thread to check for updated version of the app.
	if (Prefs.AutoUpdate)
	{
		thread thread_obj(VersionCheckThread, hMainWnd);
		thread_obj.detach();
	}

	//Set a timer for the idle detection
	if (Prefs.BlankScreenWhenIdle)
	{
		SetTimer(hMainWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
		SetTimer(hMainWnd, TIMER_IDLE, Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
	}
	if (Prefs.AdhereTopology)
		SetTimer(hMainWnd, TIMER_TOPOLOGY, TIMER_TOPOLOGY_DELAY, (TIMERPROC)NULL);

	if (Prefs.RemoteStreamingCheck || Prefs.bIdleWhitelistEnabled)
		SetTimer(hMainWnd, TIMER_CHECK_PROCESSES, TIMER_CHECK_PROCESSES_DELAY, (TIMERPROC)NULL);

	wstring startupmess = WindowTitle;
	startupmess += L" is running.";
	Log(startupmess);
	CommunicateWithService("-daemon started");

	HPOWERNOTIFY rsrn = RegisterSuspendResumeNotification(hMainWnd, DEVICE_NOTIFY_WINDOW_HANDLE);
	HPOWERNOTIFY rpsn = RegisterPowerSettingNotification(hMainWnd, &(GUID_CONSOLE_DISPLAY_STATE), DEVICE_NOTIFY_WINDOW_HANDLE);

	DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
	HDEVNOTIFY dev_notify = NULL;
	if (Prefs.RemoteStreamingCheck)
	{
		ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
		NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
		NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		memcpy(&(NotificationFilter.dbcc_classguid), &(GUID_DEVINTERFACE_USB_DEVICE), sizeof(struct _GUID));
		dev_notify = RegisterDeviceNotification(hMainWnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);

		WTSRegisterSessionNotification(hMainWnd, NOTIFY_FOR_ALL_SESSIONS);

		Prefs.Remote.Sunshine_Log_File = Sunshine_GetLogFile();
		if (Prefs.Remote.Sunshine_Log_File != "")
		{
			std::filesystem::path p(Prefs.Remote.Sunshine_Log_File);
			Prefs.Remote.Sunshine_Log_Size = std::filesystem::file_size(p);
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

	if (Prefs.ToastInitialised)
	{
		if (idToastNewversion || idToastFirstrun)
		{
			if (idToastFirstrun)
				WinToast::instance()->hideToast(idToastFirstrun);
			if (idToastNewversion)
				WinToast::instance()->hideToast(idToastNewversion);
			Sleep(500);
			WinToast::instance()->clear();
		}
	}
	DeleteObject(hBackbrush);
	UnregisterSuspendResumeNotification(rsrn);
	UnregisterPowerSettingNotification(rpsn);
	if (dev_notify)
		UnregisterDeviceNotification(dev_notify);
	if (Prefs.RemoteStreamingCheck)
		WTSUnRegisterSessionNotification(hMainWnd);
	return (int)msg.wParam;
}

//   Process messages for the main window.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	wstring str;
	switch (message)
	{
	case WM_INITDIALOG:
	{
		if (bFirstRun && Prefs.ToastInitialised)
		{
			TCHAR buffer[MAX_PATH] = { 0 };
			GetModuleFileName(NULL, buffer, MAX_PATH);
			wstring imgpath = buffer;
			std::wstring::size_type pos = imgpath.find_last_of(L"\\/");
			imgpath = imgpath.substr(0, pos + 1);
			imgpath += L"mainicon.ico";

			wstring Toast;
			Toast = L"Good news! LGTV Companion v";
			Toast += APP_VERSION;
			Toast += L" is now installed.";

			WinToastTemplate templ;
			templ = WinToastTemplate(WinToastTemplate::ImageAndText02);
			templ.setImagePath(imgpath);

			templ.setTextField(Toast, WinToastTemplate::FirstLine);
			templ.setTextField(L"Designed to protect and extend the features of your WebOS devices when used as a PC monitor.", WinToastTemplate::SecondLine);

			// Read the additional options section in the article
			templ.setDuration(WinToastTemplate::Duration::Short);
			templ.setAudioOption(WinToastTemplate::AudioOption::Default);
			idToastFirstrun = WinToast::instance()->showToast(templ, &m_WinToastHandler);
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
			CommunicateWithService("-daemon newversion");

			wstring s = L"A new version of this app is available for download here: ";
			s += NEWRELEASELINK;
			Log(s);

			if (Prefs.ToastInitialised)
			{
				TCHAR buffer[MAX_PATH] = { 0 };
				GetModuleFileName(NULL, buffer, MAX_PATH);
				wstring imgpath = buffer;
				std::wstring::size_type pos = imgpath.find_last_of(L"\\/");
				imgpath = imgpath.substr(0, pos + 1);
				imgpath += L"mainicon.ico";

				WinToastTemplate templ;
				templ = WinToastTemplate(WinToastTemplate::ImageAndText02);
				templ.setImagePath(imgpath);

				templ.setTextField(L"Yay! A new version is available.", WinToastTemplate::FirstLine);
				templ.setTextField(L"Please install the new version to keep up to date with bugfixes and features.", WinToastTemplate::SecondLine);

				templ.addAction(L"Download");

				// Read the additional options section in the article
				templ.setDuration(WinToastTemplate::Duration::Long);
				templ.setAudioOption(WinToastTemplate::AudioOption::Default);
				idToastNewversion = WinToast::instance()->showToast(templ, &m_WinToastHandler);
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
				std::filesystem::path p(Prefs.Remote.Sunshine_Log_File);
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
		if (Prefs.BlankScreenWhenIdle)
		{
			ManualUserIdleMode = APP_USER_IDLE_ON;

			bIdle = true;
			bIdlePreventEarlyWakeup = false;
			SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_IDLE, (TIMERPROC)NULL);
			SetTimer(hWnd, TIMER_IDLE, Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
			Log(L"User forced user idle mode!");
			CommunicateWithService("-daemon useridle");
		}
		else
			Log(L"Can not force user idle mode, as the feature is not enabled in the global options!");
	}break;
	case APP_USER_IDLE_OFF: //message sent from lgtv companion.exe
	{
		if (Prefs.BlankScreenWhenIdle)
		{
			LASTINPUTINFO lii;
			lii.cbSize = sizeof(LASTINPUTINFO);
			if (GetLastInputInfo(&lii))
			{
				ManualUserIdleMode = 0;

				bIdle = false;
				bIdlePreventEarlyWakeup = false;
				SetTimer(hWnd, TIMER_MAIN, bDaemonVisible ? 1000 : TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
				SetTimer(hWnd, TIMER_IDLE, Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
				dwLastInputTick = lii.dwTime;
				Log(L"User forced unsetting user idle mode!");
				CommunicateWithService("-daemon userbusy");
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
					wstring tick = common::widen(to_string(lii.dwTime));
					DWORD time = (GetTickCount() - lii.dwTime) / 1000;
					wstring ago = common::widen(to_string(time));

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
							SetTimer(hWnd, TIMER_MAIN, bDaemonVisible ? 1000 : TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
							SetTimer(hWnd, TIMER_IDLE, Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
							dwLastInputTick = lii.dwTime;
							bIdlePreventEarlyWakeup = false;
							bIdle = false;
							CommunicateWithService("-daemon userbusy");
						}
					}
					else
						bIdlePreventEarlyWakeup = false;
				}
				else
				{
					if (lii.dwTime != dwLastInputTick)
					{
						SetTimer(hWnd, TIMER_IDLE, Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
						dwLastInputTick = lii.dwTime;
					}
					bIdlePreventEarlyWakeup = false;
				}
			}
			return 0;
		}break;

		case TIMER_IDLE:
		{
			if (ManualUserIdleMode == 0)
			{
				if (Prefs.bFullscreenCheckEnabled && (daemon_startup_user_input_time == -1))
					if (FullscreenApplicationRunning())
					{
						Log(L"Fullscreen application prohibiting idle");
						return 0;
					}
				if (Prefs.bIdleWhitelistEnabled)
				{
					if (Prefs.Remote.sCurrentlyRunningWhitelistedProcess != L"")
					{
						wstring mess = L"Whitelisted application prohibiting idle (";
						mess += Prefs.Remote.sCurrentlyRunningWhitelistedProcess;
						mess += L")";
						Log(mess);
						return 0;
					}
				}
			}

			bIdle = true;
			bIdlePreventEarlyWakeup = false;

			SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_IDLE, (TIMERPROC)NULL);

			CommunicateWithService("-daemon useridle");

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
			CommunicateWithService("-daemon remote_connect");
		}break;
		case REMOTE_DISCONNECT:
		{
			KillTimer(hWnd, (UINT_PTR)REMOTE_DISCONNECT);
			CommunicateWithService("-daemon remote_disconnect");
		}break;
		case TIMER_TOPOLOGY_COLLECTION:
		{
			KillTimer(hWnd, (UINT_PTR)TIMER_TOPOLOGY_COLLECTION);
			PostMessage(hWnd, USER_DISPLAYCHANGE, NULL, NULL);
		}break;
		case TIMER_TOPOLOGY:
		{
			KillTimer(hWnd, TIMER_TOPOLOGY);
			if (!Prefs.AdhereTopology)
				break;

			int result = VerifyTopology();

			if (result == TOPOLOGY_OK)
			{
				PostMessage(hWnd, USER_DISPLAYCHANGE, NULL, NULL);
			}
			else if (result == TOPOLOGY_UNDETERMINED)
			{
				Log(L"No active devices detected when verifying Windows Monitor Topology. Topology feature has been disabled");
				CommunicateWithService("-daemon topology undetermined");
				Prefs.AdhereTopology = false;
			}
			else if (result == TOPOLOGY_OK_DISABLE)
			{
				Prefs.AdhereTopology = false;
			}
			else if (result = TOPOLOGY_ERROR)
			{
				Prefs.AdhereTopology = false;
				CommunicateWithService("-daemon topology invalid");

				wstring s = L"A change to the system has invalidated the monitor topology configuration and the feature has been disabled. "
					"Please run the configuration guide in the global options to ensure correct operation.";
				Log(s);

				if (Prefs.ToastInitialised)
				{
					TCHAR buffer[MAX_PATH] = { 0 };
					GetModuleFileName(NULL, buffer, MAX_PATH);
					wstring imgpath = buffer;
					std::wstring::size_type pos = imgpath.find_last_of(L"\\/");
					imgpath = imgpath.substr(0, pos + 1);
					imgpath += L"mainicon.ico";

					WinToastTemplate templ;
					templ = WinToastTemplate(WinToastTemplate::ImageAndText02);
					templ.setImagePath(imgpath);

					templ.setTextField(L"Invalidated monitor topology configuration!", WinToastTemplate::FirstLine);
					templ.setTextField(L"A change to the system has invalidated the multi-monitor configuration. Please run the configuration guide in the global options.", WinToastTemplate::SecondLine);

					// Read the additional options section in the article
					templ.setDuration(WinToastTemplate::Duration::Long);
					templ.setAudioOption(WinToastTemplate::AudioOption::Default);
					idToastNewversion = WinToast::instance()->showToast(templ, &m_WinToastHandler);
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
			Log(L"Suspending system.");
			return true;
		}break;
		case PBT_APMRESUMEAUTOMATIC:
		{
			bIdle = false;
			dwLastInputTick = 0;
			bIdlePreventEarlyWakeup = false;
			if (Prefs.BlankScreenWhenIdle)
			{
				SetTimer(hWnd, TIMER_MAIN, bDaemonVisible ? (TIMER_MAIN_DELAY_WHEN_BUSY) / 10 : TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
				SetTimer(hWnd, TIMER_IDLE, Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
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
						if (Prefs.BlankScreenWhenIdle)
						{
							SetTimer(hWnd, TIMER_MAIN, bDaemonVisible ? (TIMER_MAIN_DELAY_WHEN_BUSY) / 10 : TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
							SetTimer(hWnd, TIMER_IDLE, Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
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
		if (Prefs.AdhereTopology)
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
		if (Prefs.RemoteStreamingCheck && lParam)
		{
			PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR)lParam;
			PDEV_BROADCAST_DEVICEINTERFACE lpdbv = (PDEV_BROADCAST_DEVICEINTERFACE)lpdb;
			wstring path;
			if (lpdb->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
			{
				path = wstring(lpdbv->dbcc_name);
				transform(path.begin(), path.end(), path.begin(), ::tolower);

				switch (wParam)
				{
				case DBT_DEVICEARRIVAL:
				{
					for (auto& dev : Prefs.Remote.stream_usb_list_gamestream)
					{
						if (path.find(dev, 0) != wstring::npos)
						{
							RemoteStreamingEvent(REMOTE_NVIDIA_CONNECTED);
							return true;
						}
					}
				}break;
				case DBT_DEVICEREMOVECOMPLETE:
				{
					for (auto& dev : Prefs.Remote.stream_usb_list_gamestream)
					{
						if (path.find(dev, 0) != wstring::npos)
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
		if (Prefs.ToastInitialised)
		{
			if (idToastNewversion || idToastFirstrun)
			{
				if (idToastFirstrun)
					WinToast::instance()->hideToast(idToastFirstrun);
				if (idToastNewversion)
					WinToast::instance()->hideToast(idToastNewversion);
				Sleep(500);
				WinToast::instance()->clear();
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
	wstring WindowTitle;
	WindowTitle = APPNAME_FULL;
	WindowTitle += L" v";
	WindowTitle += APP_VERSION;

	HWND Other_hWnd = FindWindow(NULL, WindowTitle.c_str());

	if (Other_hWnd)
	{
		COPYDATASTRUCT cds;
		cds.cbData = sizeof(WCHAR);
		cds.lpData = NULL;
		cds.dwData = NOTIFY_NEW_PROCESS;
		SendMessage(Other_hWnd, WM_COPYDATA, NULL, (LPARAM)&cds);
		return true;
	}
	return false;
}
//   communicate with service via IPC
void CommunicateWithService(string input)
{
	DWORD dwWritten;

	if (input == "")
		return;

	hPipe = CreateFile(PIPENAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (hPipe != INVALID_HANDLE_VALUE)
	{
		WriteFile(hPipe,
			input.c_str(),
			(DWORD)input.length() + 1,   // = length of string + terminating '\0' !!!
			&dwWritten,
			NULL);
		Log(common::widen(input));
	}
	else
		Log(L"Failed to connect to named pipe. Service may be stopped.");

	if (hPipe != INVALID_HANDLE_VALUE)
		CloseHandle(hPipe);
}

void Log(wstring input)
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

	wstring logmess = common::widen(buffer);
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
	string s;
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
	if (find != string::npos)
	{
		size_t begin = s.find_first_of("0123456789", find);
		if (begin != string::npos)
		{
			size_t end = s.find("\"", begin);
			string lastver = s.substr(begin, end - begin);

			vector <string> local_ver = common::stringsplit(common::narrow(APP_VERSION), ".");
			vector <string> remote_ver = common::stringsplit(lastver, ".");

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
vector<settings::DISPLAY_INFO> QueryDisplays()
{
	vector<settings::DISPLAY_INFO> targets;

	//populate targets struct with information about attached displays
	EnumDisplayMonitors(NULL, NULL, meproc, (LPARAM)&targets);

	return targets;
}
static BOOL CALLBACK meproc(HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM pData)
{
	if (!pData)
		return false;
	vector<settings::DISPLAY_INFO>* targets = (vector<settings::DISPLAY_INFO> *) pData;
	UINT32 requiredPaths, requiredModes;
	vector<DISPLAYCONFIG_PATH_INFO> paths;
	vector<DISPLAYCONFIG_MODE_INFO> modes;
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
			wstring FriendlyName = name.monitorFriendlyDeviceName;
			transform(FriendlyName.begin(), FriendlyName.end(), FriendlyName.begin(), ::tolower);
			if (FriendlyName.find(L"lg tv") != wstring::npos)
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
	stringstream s;
	s << "-daemon topology ";
	vector<settings::DISPLAY_INFO> displays = QueryDisplays();
	if (Prefs.Devices.size() == 0)
		return false;
	if (displays.size() > 0)
	{
		for (auto& disp : displays)
		{
			for (auto& dev : Prefs.Devices)
			{
				string ActiveDisplay = common::narrow(disp.target.monitorDevicePath);
				string DeviceString = dev.UniqueDeviceKey;
				transform(ActiveDisplay.begin(), ActiveDisplay.end(), ActiveDisplay.begin(), ::tolower);
				transform(DeviceString.begin(), DeviceString.end(), DeviceString.begin(), ::tolower);
				if (ActiveDisplay == DeviceString)
				{
					s << dev.DeviceId << " ";
				}
			}
		}
	}
	s << "*";
	CommunicateWithService(s.str());
	return true;
}

int VerifyTopology(void)
{
	bool match = false;

	if (!Prefs.AdhereTopology)
		return TOPOLOGY_OK_DISABLE;
	if (Prefs.Devices.size() == 0)
		return TOPOLOGY_OK_DISABLE;

	vector<settings::DISPLAY_INFO> displays = QueryDisplays();
	if (displays.size() == 0)
		return TOPOLOGY_UNDETERMINED;

	for (auto& disp : displays)
	{
		match = false;

		for (auto& dev : Prefs.Devices)
		{
			string ActiveDisplay = common::narrow(disp.target.monitorDevicePath);
			string DeviceString = dev.UniqueDeviceKey;
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
	bool bWhiteListConfigured = Prefs.WhiteList.size() > 0 ? true : false;
	bool bWhitelistProcessFound = false;
	bool bStreamingProcessFound = false;
	bool bSunshineSvcProcessFound = false;
	wstring sWhitelistProcessFound = L"";
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
		if (Prefs.bIdleWhitelistEnabled && bWhiteListConfigured && !bWhitelistProcessFound)
			for (auto& w : Prefs.WhiteList)
				if (w.Application != L"")
					if (!_tcsicmp(entry.szExeFile, w.Application.c_str()))
					{
						if (w.Name != L"")
							sWhitelistProcessFound = w.Name;
						else
							sWhitelistProcessFound = L"<unnamed>";
						bWhitelistProcessFound = true;
					}
		// look for currently running known streaming processes
		if (Prefs.RemoteStreamingCheck && !bStreamingProcessFound)
			for (auto& w : Prefs.Remote.stream_proc_list)
				if (!_tcsicmp(entry.szExeFile, w.c_str()))
					bStreamingProcessFound = true;

		// look for currently running sushine service process
		if (Prefs.RemoteStreamingCheck && !bSunshineSvcProcessFound)
			if (!_tcsicmp(entry.szExeFile, SUNSHINE_FILE_SVC))
				bSunshineSvcProcessFound = true;
	} while (!(bStreamingProcessFound && bWhitelistProcessFound && bSunshineSvcProcessFound) && Process32Next(snapshot, &entry));

	CloseHandle(snapshot);

	// was sunshine service found currently running?
	if (bSunshineSvcProcessFound && (Prefs.Remote.Sunshine_Log_File != ""))
	{
		std::filesystem::path p(Prefs.Remote.Sunshine_Log_File);
		uintmax_t Size = std::filesystem::file_size(p);
		if (Size != Prefs.Remote.Sunshine_Log_Size)
		{
			ReturnValue ^= Sunshine_CheckLog();
			Prefs.Remote.Sunshine_Log_Size = Size;
		}
	}
	else
		ReturnValue ^= REMOTE_SUNSHINE_NOT_CONNECTED;

	// was a remote streaming process currently running?
	ReturnValue ^= bStreamingProcessFound ? REMOTE_STEAM_CONNECTED : REMOTE_STEAM_NOT_CONNECTED;

	// was a user idle mode whitelisted process currently running?
	Prefs.Remote.sCurrentlyRunningWhitelistedProcess = sWhitelistProcessFound;
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
		wstring window_name = common::GetWndText(hWnd);
		transform(window_name.begin(), window_name.end(), window_name.begin(), ::tolower);
		if (window_name.find(L"nvidia geforce overlay") != wstring::npos)
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
						CommunicateWithService("-daemon gfe");
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
	if (Prefs.RemoteStreamingCheck)
	{
		bool bCurrentlyConnected = Prefs.Remote.bRemoteCurrentStatusSteam || Prefs.Remote.bRemoteCurrentStatusNvidia || Prefs.Remote.bRemoteCurrentStatusRDP || Prefs.Remote.bRemoteCurrentStatusSunshine;

		if (dwType & REMOTE_STEAM_CONNECTED)
		{
			if (!Prefs.Remote.bRemoteCurrentStatusSteam)
			{
				Log(L"Steam gamestream connected.");
				Prefs.Remote.bRemoteCurrentStatusSteam = true;
			}
		}
		else if (dwType & REMOTE_STEAM_NOT_CONNECTED)
		{
			if (Prefs.Remote.bRemoteCurrentStatusSteam)
			{
				Log(L"Steam gamestream disconnected.");
				Prefs.Remote.bRemoteCurrentStatusSteam = false;
			}
		}
		if (dwType & REMOTE_NVIDIA_CONNECTED)
		{
			if (!Prefs.Remote.bRemoteCurrentStatusNvidia)
			{
				Log(L"nVidia gamestream connected.");
				Prefs.Remote.bRemoteCurrentStatusNvidia = true;
			}
		}
		else if (dwType & REMOTE_NVIDIA_NOT_CONNECTED)
		{
			if (Prefs.Remote.bRemoteCurrentStatusNvidia)
			{
				Log(L"nVidia gamestream disconnected.");
				Prefs.Remote.bRemoteCurrentStatusNvidia = false;
			}
		}
		if (dwType & REMOTE_SUNSHINE_CONNECTED)
		{
			if (!Prefs.Remote.bRemoteCurrentStatusSunshine)
			{
				Log(L"Sunshine gamestream connected.");
				Prefs.Remote.bRemoteCurrentStatusSunshine = true;
			}
		}
		else if (dwType & REMOTE_SUNSHINE_NOT_CONNECTED)
		{
			if (Prefs.Remote.bRemoteCurrentStatusSunshine)
			{
				Log(L"Sunshine gamestream disconnected.");
				Prefs.Remote.bRemoteCurrentStatusSunshine = false;
			}
		}
		if (dwType & REMOTE_RDP_CONNECTED)
		{
			if (!Prefs.Remote.bRemoteCurrentStatusRDP)
			{
				Log(L"RDP connected.");
				Prefs.Remote.bRemoteCurrentStatusRDP = true;
			}
		}
		else if (dwType & REMOTE_RDP_NOT_CONNECTED)
		{
			if (Prefs.Remote.bRemoteCurrentStatusRDP)
			{
				Log(L"RDP disconnected.");
				Prefs.Remote.bRemoteCurrentStatusRDP = false;
			}
		}
		bool bConnect = Prefs.Remote.bRemoteCurrentStatusSteam || Prefs.Remote.bRemoteCurrentStatusNvidia || Prefs.Remote.bRemoteCurrentStatusRDP || Prefs.Remote.bRemoteCurrentStatusSunshine;

		if (bCurrentlyConnected && !bConnect)
			SetTimer(hMainWnd, (UINT_PTR)REMOTE_DISCONNECT, 1000, (TIMERPROC)NULL);
		else if (!bCurrentlyConnected && bConnect)
			SetTimer(hMainWnd, (UINT_PTR)REMOTE_CONNECT, TIMER_REMOTE_DELAY, (TIMERPROC)NULL);
	}
	return;
}

DWORD Sunshine_CheckLog(void)
{
	string log;
	ifstream t(Prefs.Remote.Sunshine_Log_File);
	if (t.is_open())
	{
		stringstream buffer;
		t.seekg(-5000, ios_base::end);
		buffer << t.rdbuf();
		string log = buffer.str();
		t.close();

		int type;
		size_t pos_connect = log.rfind("CLIENT CONNECTED");
		size_t pos_disconnect = log.rfind("CLIENT DISCONNECTED");
		if (pos_connect == string::npos && pos_disconnect == string::npos)
			return 0;
		else if (pos_connect == string::npos)
			type = REMOTE_SUNSHINE_NOT_CONNECTED;
		else if (pos_disconnect == string::npos)
			type = REMOTE_SUNSHINE_CONNECTED;
		else
			type = (pos_connect > pos_disconnect) ? REMOTE_SUNSHINE_CONNECTED : REMOTE_SUNSHINE_NOT_CONNECTED;

		size_t begin = log.rfind("[", type == REMOTE_SUNSHINE_CONNECTED ? pos_connect : pos_disconnect);
		string time = log.substr(begin + 1, 19); //2023:02:02:00:04:38
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

string Sunshine_GetLogFile()
{
	string Sunshine_Config;
	string configuration_path;
	string configuration_text;
	string log_file;

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
			ifstream t(Sunshine_Config);
			if (t.is_open())
			{
				stringstream buffer;
				buffer << t.rdbuf();
				configuration_text = buffer.str();
				t.close();

				string s = Sunshine_GetConfVal(configuration_text, "min_log_level");
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
					filesystem::path log_p(s);
					filesystem::current_path(configuration_path);
					filesystem::path log_abs(filesystem::absolute(log_p));
					log_file = log_abs.string();
				}
				return log_file;
			}
		}
	}
	return "";
}

string Sunshine_GetConfVal(string buf, string conf_item)
{
	conf_item += " = ";
	size_t pos = buf.find(conf_item);
	if (pos != string::npos)
	{
		size_t newl = buf.find("\n", pos);
		if (newl == string::npos)
			return buf.substr(pos + conf_item.length());
		else
			return buf.substr(pos + conf_item.length(), newl - (pos + conf_item.length()));
	}
	return "";
}