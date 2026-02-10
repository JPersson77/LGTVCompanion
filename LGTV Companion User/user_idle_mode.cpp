#include <phnt_windows.h>
#define PHNT_VERSION PHNT_20H1

#include <phnt.h>
#include <subprocesstag.h>
#include <stdio.h>

#include <tlhelp32.h>
#include <shellapi.h>
#include "user_idle_mode.h"
#include "../Common/tools.h"

#pragma comment(lib, "Kernel32.lib")

struct FULLSCREEN_DETECTION
{
    bool found_fullscreen = false;
    DWORD target_PID = NULL;
    std::wstring process_name = L"";
};

bool UIM::isDisplayLock(bool& video_wake_lock, std::vector <std::wstring> & processes, std::wstring& message)
{
    NTSTATUS status;
    std::vector<BYTE> buf;
    ULONG buffer_length = 4096;
    video_wake_lock = false;

    do 
    {
        buf.resize(buffer_length);
        status = NtPowerInformation( (POWER_INFORMATION_LEVEL)GetPowerRequestList, NULL, 0, buf.data(), buffer_length);
        if (!NT_SUCCESS(status))
        {
            if (status == STATUS_BUFFER_TOO_SMALL) {
                buffer_length += 4096;
            }
            else {
                message = L"Failed to query power information";
                return false;
            }
        }
    } while (status == STATUS_BUFFER_TOO_SMALL);
    if (buf.empty())
    {
        message = L"Failed to allocate memory for power requests";
        return false;
    }
    PPOWER_REQUEST_LIST power_list = reinterpret_cast<PPOWER_REQUEST_LIST>(buf.data());
    for (ULONG i = 0; i < power_list->Count; i++)
    {
        PDIAGNOSTIC_BUFFER diagnosticBuffer;
        PPOWER_REQUEST Request = (PPOWER_REQUEST)RtlOffsetToPointer(power_list, power_list->PowerRequestOffsets[i]);
        if (!Request->V4.PowerRequestCount[PowerRequestDisplayRequiredInternal])
            continue;

        diagnosticBuffer = &Request->V4.DiagnosticBuffer;
        std::wstring requesterName = L"Unknown";
        std::wstring reason_message = L"";

        // Diagnostic info depends on the requester type
        if(diagnosticBuffer->CallerType == UserProcessRequester)
           if (diagnosticBuffer->ProcessImageNameOffset)
               requesterName = (PCWSTR)RtlOffsetToPointer(diagnosticBuffer, diagnosticBuffer->ProcessImageNameOffset);

        // The diagnostic buffer also stores the reason
        if (diagnosticBuffer->ReasonOffset)
        {
            PCOUNTED_REASON_CONTEXT_RELATIVE reason =
                (PCOUNTED_REASON_CONTEXT_RELATIVE)RtlOffsetToPointer(diagnosticBuffer, diagnosticBuffer->ReasonOffset);
            if (reason->Flags & POWER_REQUEST_CONTEXT_SIMPLE_STRING)
                reason_message = (PCWCHAR)RtlOffsetToPointer(reason, reason->SimpleStringOffset);
/*
            else if (reason->Flags & POWER_REQUEST_CONTEXT_DETAILED_STRING)
            {
                HMODULE hModule = LoadLibraryExW((PCWSTR)RtlOffsetToPointer(reason, reason->ResourceFileNameOffset), NULL,LOAD_LIBRARY_AS_DATAFILE);
                if (hModule)
                {
                    PCWSTR reasonString;
                    int reasonLength = LoadStringW(hModule,reason->ResourceReasonId,(LPWSTR)&reasonString, 0);
                    if (reasonLength)
                        reason_message = reasonString;
                     FreeLibrary(hModule);
                }
            }
*/
        }
        
        if (reason_message == L"Video Wake Lock") //most browsers
            video_wake_lock = true;
        if (requesterName != L"")
        {
            std::wstring processName;
            size_t lastSlash = requesterName.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos)
                processName = tools::tolower(requesterName.substr(lastSlash + 1));
            else
                processName = tools::tolower(requesterName);
            processes.push_back(processName);
            // Firefox
            if(processName == L"firefox.exe")
                if (reason_message == L"display request")
                    video_wake_lock = true;
            // Zen Browser (Firefox-based)
            if(processName == L"zen.exe")
                if (reason_message == L"display request")
                    video_wake_lock = true;
        }
    }
    return true;
}

std::wstring UIM::getProcessFromHWND(HWND hwnd)
{
    DWORD processId = 0;
    HANDLE hProcess = nullptr;
    wchar_t filename[MAX_PATH] = { 0 };

    // Get the process ID associated with the window
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == 0) {
        return L"";
    }

    // Open the process with query limited information access
    hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess == nullptr) {
        return L"";
    }

    // Get the executable path
    DWORD size = MAX_PATH;
    if (!QueryFullProcessImageNameW(hProcess, 0, filename, &size)) {
        CloseHandle(hProcess);
        return L"";
    }
    CloseHandle(hProcess);

    // Extract just the filename from the full path
    std::wstring fullPath(filename);
    size_t lastSlash = fullPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return tools::tolower(fullPath.substr(lastSlash + 1));
    }
    return tools::tolower(fullPath);
}

bool UIM::preventUIM(Preferences& Prefs, std::wstring& message)
{
    std::wstring foreground_process_name;

    if (!Prefs.user_idle_mode_)
        return false;
    if (Prefs.user_idle_mode_disable_while_fullscreen_)
        if (isFullscreenReported())
        {
            message = L"an application is showing fullscreen";
            return true;
        }
    if (Prefs.user_idle_mode_disable_while_video_wake_lock_)
    {
        bool video_wake_lock = false;
        std::vector<std::wstring> power_request_process_names;
        std::wstring disp_lock_msg;
        if (isDisplayLock(video_wake_lock, power_request_process_names, disp_lock_msg)
            && video_wake_lock)
        {
            if (!Prefs.user_idle_mode_disable_while_video_wake_lock_foreground_
                && !Prefs.user_idle_mode_disable_while_video_wake_lock_fullscreen_)
            {
                message = L"Video Wake Lock";
                return true;
            }
            if (Prefs.user_idle_mode_disable_while_video_wake_lock_foreground_)
            {
                foreground_process_name = tools::tolower(getProcessFromHWND(GetForegroundWindow()));
                for(auto& process : power_request_process_names)
                {
                    if (process == foreground_process_name)
                    {
                        message = L"Video Wake Lock - ";
                        message += process;
                        message += L" is in the foreground";
                        return true;
                    }
                }
            }
            if (Prefs.user_idle_mode_disable_while_video_wake_lock_fullscreen_)
            {
                if(isFullscreenReported())
                {
                    for (auto& process : power_request_process_names)
                    {
                        if (process != L"" && isProcessShowingFullscreen(NULL, process))
                        {
                            message = L"Video Wake Lock - ";
                            message += process;
                            message += L" is showing fullscreen";
                            return true;
                        }
                    }
                }
            }
        }
    }
    if (!Prefs.user_idle_mode_process_control_)
        return false;

    //do a few checks
    bool fullscreen_reported = isFullscreenReported();
    bool dummy = false;
    std::vector<std::wstring> power_request_process_names;
    std::wstring log_message;
    isDisplayLock(dummy, power_request_process_names, log_message);

    if (foreground_process_name == L"")
        foreground_process_name = tools::tolower(getProcessFromHWND(GetForegroundWindow()));

    // Iterate over currently running processes
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (!Process32First(snapshot, &entry)) {
        CloseHandle(snapshot);
        return false;
    }
    do
    {
        std::wstring exe = tools::tolower(entry.szExeFile);
        for (auto& proc : Prefs.user_idle_mode_process_control_list_)
        {
            if (proc.process_control_disable_while_running && tools::compareUsingWildcard(exe, proc.binary))
            {
                if (!proc.process_control_disable_while_running_display_lock
                    && !proc.process_control_disable_while_running_foreground
                    && !proc.process_control_disable_while_running_fullscreen)
                {
                    message = proc.friendly_name;
                    message += L" (";
                    message += entry.szExeFile;
                    message += L") is running";
                    CloseHandle(snapshot);
                    return true;
                }

                bool result_fg = false;
                bool result_fs = false;
                bool result_dl = false;
                if (proc.process_control_disable_while_running_foreground)
                {
                    if (foreground_process_name == exe)
                    {
                        result_fg = true;
                    }
                }
                if (proc.process_control_disable_while_running_fullscreen)
                {
                    if (fullscreen_reported)
                    {
                        if(isProcessShowingFullscreen(entry.th32ProcessID))
                        {
                            result_fs = true;
                        }
                    }
                }
                if (proc.process_control_disable_while_running_display_lock)
                {
                    for(auto& process : power_request_process_names)
                    {
                        if (process == exe)
                        {
                            result_dl = true;
                        }
                    }
                }
                bool result = true; // Start with a default "true" value

                if (proc.process_control_disable_while_running_foreground) result = result && result_fg; 
                if (proc.process_control_disable_while_running_fullscreen) result = result && result_fs; 
                if (proc.process_control_disable_while_running_display_lock) result = result && result_dl;
                
                if(result)
                {
                    message = proc.friendly_name;
                    message += L" (";
                    message += entry.szExeFile;
                    message += L") is";
                    if (proc.process_control_disable_while_running_fullscreen)
                    {
                        message += L" fullscreen";
                        if (proc.process_control_disable_while_running_foreground || proc.process_control_disable_while_running_display_lock)
                            message += L",";
                    }
                    if (proc.process_control_disable_while_running_foreground)
                    {
                        message += L" foreground";
                        if (proc.process_control_disable_while_running_display_lock)
                            message += L",";
                    }
                    if (proc.process_control_disable_while_running_display_lock)
                    {
                        message += L" requesting display lock";
                    }
                    CloseHandle(snapshot);
                    return true;
                }
            }
        }
    } while (Process32Next(snapshot, &entry));
    CloseHandle(snapshot);
    return false;
}
bool UIM::isFullscreenReported(void)
{
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
bool UIM::isProcessShowingFullscreen(DWORD targetPID, std::wstring process_name)
{
    FULLSCREEN_DETECTION fd;
    fd.target_PID = targetPID;
    fd.process_name = tools::tolower(process_name);
    EnumWindows(UIM::enumWindowProc, reinterpret_cast<LPARAM>(&fd));
    return fd.found_fullscreen;
}
static BOOL CALLBACK UIM::enumWindowProc(HWND hWnd, LPARAM lParam) {
    FULLSCREEN_DETECTION* pfd = reinterpret_cast<FULLSCREEN_DETECTION*>(lParam);
    if (pfd->found_fullscreen)
        return false;
    if (!IsWindowVisible(hWnd)) 
        return true;
    if(pfd->target_PID != NULL)   
    {
        DWORD processId;
        GetWindowThreadProcessId(hWnd, &processId);
        if (processId == 0)
            return true;
        if (processId != pfd->target_PID)
            return true;
    }
    else if (pfd->process_name != L"")
    {
        std::wstring p = getProcessFromHWND(hWnd);
        if (pfd->process_name != p)
              return true;
    }
    
    bool is_window_fullscreen = isWindowShowingFullscreen(hWnd);
    if(is_window_fullscreen)
    {
        pfd->found_fullscreen = true;
        return false;
    }
    return true;
 
}
bool UIM::isWindowShowingFullscreen(HWND hWnd)
{
    // Get window rect and monitor info
    RECT window_rect;
    HMONITOR h_monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW mi = { sizeof(MONITORINFOEXW) };

    if (!GetWindowRect(hWnd, &window_rect) || !GetMonitorInfoW(h_monitor, &mi))
        return false;

    // Check fullscreen coverage
    const RECT& monitor_rect = mi.rcMonitor;
    bool covers_entire_monitor =
        window_rect.left == monitor_rect.left &&
        window_rect.top == monitor_rect.top &&
        window_rect.right == monitor_rect.right &&
        window_rect.bottom == monitor_rect.bottom;
    if (!covers_entire_monitor)
        return false;

    // Check for window chrome
    /*
    const LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
    constexpr LONG_PTR chromeFlags = WS_CAPTION | WS_THICKFRAME | WS_SYSMENU;
    bool is_borderless = (style & chromeFlags) == 0;
    if (!is_borderless)
        return false;
        */
    /*
    // Check for maximized state
    WINDOWPLACEMENT placement = { sizeof(WINDOWPLACEMENT) };
    GetWindowPlacement(hWnd, &placement);
    bool is_maximized = placement.showCmd == SW_SHOWMAXIMIZED;
    if (is_maximized)
        return false;
        */
    return true;
}