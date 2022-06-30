#include "Daemon.h"

using namespace std;
using namespace WinToastLib;
using json = nlohmann::json;

// Globals:
HINSTANCE                       hInstance;  // current instance
HWND                            hMainWnd;
bool                            bIdle = false;
bool                            bIdlePreventEarlyWakeup = false;
bool                            bDaemonVisible = true;
bool                            bFirstRun = false;
DWORD                           dwLastInputTick = 0;
WinToastHandler                 m_WinToastHandler;
wstring                         CommandLineParameters;
wstring                         DataPath;
json                            jsonPrefs;                          //contains the user preferences in json
PREFS                           Prefs;
HANDLE                          hPipe = INVALID_HANDLE_VALUE;
INT64                           idToastFirstrun = NULL;
INT64                           idToastNewversion = NULL;

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
        ReadConfigFile();
    }
    catch (...) {

        CommunicateWithService("-daemon errorconfig");
        return false;
    }
 
    // create main window (dialog)
    hMainWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)WndProc);
    SetWindowText(hMainWnd, WindowTitle.c_str());
    ShowWindow(hMainWnd, bDaemonVisible?SW_SHOW:SW_HIDE);

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
        
    wstring startupmess = WindowTitle;
    startupmess += L" is running.";
    Log(startupmess);

    HPOWERNOTIFY rsrn =RegisterSuspendResumeNotification(hMainWnd, DEVICE_NOTIFY_WINDOW_HANDLE);
    HPOWERNOTIFY rpsn = RegisterPowerSettingNotification(hMainWnd, &(GUID_CONSOLE_DISPLAY_STATE), DEVICE_NOTIFY_WINDOW_HANDLE);

    WTSRegisterSessionNotification(hMainWnd, NOTIFY_FOR_ALL_SESSIONS);

    CommunicateWithService("-daemon started");
 
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

    UnregisterSuspendResumeNotification(rsrn);
    UnregisterPowerSettingNotification(rpsn);
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
 
    }break;

    case WM_NOTIFY:
    {
     
    }break;
    case WM_TIMER:
    {
        switch (wParam)
        {
        case TIMER_MAIN:
        {
            LASTINPUTINFO lii;
            lii.cbSize = sizeof(LASTINPUTINFO);
            if(GetLastInputInfo(&lii))
            {
                if (bDaemonVisible)
                {
                    wstring tick = widen(to_string(lii.dwTime));
                    DWORD time = (GetTickCount() - lii.dwTime) / 1000;
                    wstring ago = widen(to_string(time));

                    SendMessage(GetDlgItem(hMainWnd, IDC_EDIT2), WM_SETTEXT, 0, (WPARAM)tick.c_str());
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
                            Prefs.DisableSendingViaIPC = false;
                        }
                        else
                        {
                            SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
                            SetTimer(hWnd, TIMER_IDLE, Prefs.BlankScreenWhenIdleDelay * 60 * 1000, (TIMERPROC)NULL);
                            dwLastInputTick = lii.dwTime;
                            bIdlePreventEarlyWakeup = false;
                            bIdle = false;
  //                        Log(L"Busy");
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
                        SetTimer(hWnd, TIMER_IDLE, Prefs.BlankScreenWhenIdleDelay*60*1000, (TIMERPROC)NULL);
                        dwLastInputTick = lii.dwTime;
                    }
                    bIdlePreventEarlyWakeup = false;
                }
            }
           
            return 0;
        }break;
        case TIMER_IDLE:
        {
            bIdle = true;
            bIdlePreventEarlyWakeup = false;
 //           KillTimer(hWnd, TIMER_IDLE);
            SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_IDLE, (TIMERPROC)NULL);
 //           Log(L"Idle");
            CommunicateWithService("-daemon useridle");

            return 0;
        }break;
        case TIMER_RDP:
        {
            KillTimer(hWnd, TIMER_RDP);
            if (Prefs.BlankScreenWhenIdle)
            {
                if (bIdle)
                    CommunicateWithService("-daemon remoteconnect_idle");
                else
                    CommunicateWithService("-daemon remoteconnect_busy");
            }
            else
                CommunicateWithService("-daemon remoteconnect");
            Prefs.DisableSendingViaIPC = true;
            
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
//            case PBT_APMRESUMESUSPEND:
            {
                bIdle = false;
                dwLastInputTick = 0;
                bIdlePreventEarlyWakeup = false;
                if (Prefs.BlankScreenWhenIdle)
                {
                    SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
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
                                SetTimer(hWnd, TIMER_MAIN, TIMER_MAIN_DELAY_WHEN_BUSY, (TIMERPROC)NULL);
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
    } 
    case WM_WTSSESSION_CHANGE:
    {
        if (wParam == WTS_REMOTE_CONNECT)
        {
            SetTimer(hMainWnd, TIMER_RDP, TIMER_RDP_DELAY, (TIMERPROC)NULL);

        }
        else if (wParam == WTS_REMOTE_DISCONNECT)
        {
            Prefs.DisableSendingViaIPC = false;                
            CommunicateWithService("-daemon remotedisconnect");
            Prefs.DisableSendingViaIPC = true; // to prevent user idle screen blanking on login screen, which cannot be unblanked without using the remote.
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
        break;
    }
    default:
        return 0;
    }
    return 0;
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
//   Read the configuration file into a json object and populate the preferences struct
bool ReadConfigFile()
{
    WCHAR szPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath)))
    {
        wstring path = szPath;
        path += L"\\";
        path += APP_PATH;
        path += L"\\";
        CreateDirectory(path.c_str(), NULL);

        DataPath = path;

        path += L"config.json";

        ifstream i(path.c_str());
        if (i.is_open())
        {
            json j;
            i >> jsonPrefs;
            i.close();

            j = jsonPrefs[JSON_PREFS_NODE][JSON_VERSION];
            if (!j.empty() && j.is_number())
                Prefs.version = j.get<int>();

            j = jsonPrefs[JSON_PREFS_NODE][JSON_LOGGING];
            if (!j.empty() && j.is_boolean())
                Prefs.Logging = j.get<bool>();

            j = jsonPrefs[JSON_PREFS_NODE][JSON_AUTOUPDATE];
            if (!j.empty() && j.is_boolean())
                Prefs.AutoUpdate = j.get<bool>();

            j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEBLANK];
            if (!j.empty() && j.is_boolean())
                Prefs.BlankScreenWhenIdle = j.get<bool>();

            j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEBLANKDELAY];
            if (!j.empty() && j.is_number())
                Prefs.BlankScreenWhenIdleDelay = j.get<int>();

            return true;
        }
    }
    return false;
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

//   Send the commandline to the service
void CommunicateWithService(string input)
{
   
    DWORD dwWritten;

    if (input == "")
        return;
    if (!Prefs.DisableSendingViaIPC)
    {
        hPipe = CreateFile(PIPENAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

        if (hPipe != INVALID_HANDLE_VALUE)
        {
            WriteFile(hPipe,
                input.c_str(),
                (DWORD)input.length() + 1,   // = length of string + terminating '\0' !!!
                &dwWritten,
                NULL);
            Log(widen(input));
        }
        else
            Log(L"Failed to connect to named pipe. Service may be stopped.");

        if (hPipe != INVALID_HANDLE_VALUE)
            CloseHandle(hPipe);
    }
}

void Log(wstring input)
{
    time_t rawtime;
    struct tm timeinfo;
    char buffer[80];

    if (!bDaemonVisible)
        return;
    
    time(&rawtime);
    localtime_s(&timeinfo, &rawtime);

    strftime(buffer, 80, "%a %H:%M:%S > ", &timeinfo);
    puts(buffer);

    wstring logmess = widen(buffer);
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

            vector <string> local_ver = stringsplit(narrow(APP_VERSION), ".");
            vector <string> remote_ver = stringsplit(lastver, ".");

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
//   Split a string into a vecor of words
vector<string> stringsplit(string str, string token) {
    vector<string>res;
    while (str.size()) {
        int index = (int)str.find(token);
        if (index != string::npos) {
            res.push_back(str.substr(0, index));
            str = str.substr(index + token.size());
            if (str.size() == 0)res.push_back(str);
        }
        else {
            res.push_back(str);
            str = "";
        }
    }
    return res;
}
