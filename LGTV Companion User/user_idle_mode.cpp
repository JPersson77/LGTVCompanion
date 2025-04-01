#include <phnt_windows.h>
#define PHNT_VERSION PHNT_20H1

#include <phnt.h>
#include <subprocesstag.h>
#include <stdio.h>
//#include "helper.h"

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
/*
ULONG IsSuccess(_In_ NTSTATUS Status, _In_ LPCWSTR Where)
{
    if (!NT_SUCCESS(Status))
    {
        return FALSE;
    }

    return TRUE;
}

ULONG IsWoW64(void)
{
#ifdef _WIN64
    return FALSE;
#else
    ULONG_PTR isWoW64 = FALSE;

    NTSTATUS status = NtQueryInformationProcess(
        NtCurrentProcess(),
        ProcessWow64Information,
        &isWoW64,
        sizeof(isWoW64),
        NULL
    );

    if (!IsSuccess(status, L"WoW64 check"))
        return TRUE; // Assume we are under WoW64

    if (isWoW64)
        wprintf_s(L"Cannot run under WoW64, use the 64-bit version instead.");

    return !!isWoW64;
#endif
}

ULONG SupportedModeCount = 0;

void InitializeSupportedModeCount(void)
{
    RTL_OSVERSIONINFOEXW versionInfo = { sizeof(RTL_OSVERSIONINFOEXW) };

    if (!NT_SUCCESS(RtlGetVersion(&versionInfo)))
        return;

    if (versionInfo.dwMajorVersion > 10 ||
        (versionInfo.dwMajorVersion == 10 && (versionInfo.dwMinorVersion > 0 ||
            (versionInfo.dwMinorVersion == 0 && versionInfo.dwBuildNumber >= 14393))))
    {
        // Windows 10 RS1+
        SupportedModeCount = POWER_REQUEST_SUPPORTED_TYPES_V4;
    }
    else if (versionInfo.dwMajorVersion > 6 ||
        (versionInfo.dwMajorVersion == 6 && versionInfo.dwMinorVersion >= 3))
    {
        // Windows 8.1+
        SupportedModeCount = POWER_REQUEST_SUPPORTED_TYPES_V3;
    }
    else if (versionInfo.dwMajorVersion == 6 && versionInfo.dwMinorVersion == 2)
    {
        // Windows 8
        SupportedModeCount = POWER_REQUEST_SUPPORTED_TYPES_V2;
    }
    else
    {
        // Windows 7
        SupportedModeCount = POWER_REQUEST_SUPPORTED_TYPES_V1;
    }
}
*/
/*
PQUERY_TAG_INFORMATION I_QueryTagInformationLoader()
{
    static PQUERY_TAG_INFORMATION I_QueryTagInformationCache = NULL;

    // Use the cached address
    if (I_QueryTagInformationCache)
        return I_QueryTagInformationCache;

    UNICODE_STRING dllName;
    PVOID hAdvApi32;

    RtlInitUnicodeString(&dllName, L"advapi32.dll");

    if (NT_SUCCESS(LdrLoadDll(NULL, NULL, &dllName, &hAdvApi32)))
    {
        ANSI_STRING functionName;

        RtlInitAnsiString(&functionName, "I_QueryTagInformation");

        // Locate the function
        NTSTATUS status = LdrGetProcedureAddress(
            hAdvApi32,
            &functionName,
            0,
            (PVOID*)&I_QueryTagInformationCache
        );

        if (!NT_SUCCESS(status))
            I_QueryTagInformationCache = NULL;

        // Do not unload the DLL
    }

    return I_QueryTagInformationCache;
}
*/
/*
ULONG DisplayRequest(
    _In_ PPOWER_REQUEST Request,
    _In_ POWER_REQUEST_TYPE RequestType,
    std::wstring& pOut
)
{
    PDIAGNOSTIC_BUFFER diagnosticBuffer;

    // Determine if the request matches the type
    if ((ULONG)RequestType >= SupportedModeCount || !Request->V4.PowerRequestCount[RequestType])
        return FALSE;

    // The location of the request's body depends on the supported modes
    switch (SupportedModeCount)
    {
    case POWER_REQUEST_SUPPORTED_TYPES_V1:
        diagnosticBuffer = &Request->V1.DiagnosticBuffer;
        break;

    case POWER_REQUEST_SUPPORTED_TYPES_V2:
        diagnosticBuffer = &Request->V2.DiagnosticBuffer;
        break;

    case POWER_REQUEST_SUPPORTED_TYPES_V3:
        diagnosticBuffer = &Request->V3.DiagnosticBuffer;
        break;

    case POWER_REQUEST_SUPPORTED_TYPES_V4:
        diagnosticBuffer = &Request->V4.DiagnosticBuffer;
        break;

    default:
        return FALSE;
    }

    PCWSTR requesterName = L"Unknown";
    PCWSTR requesterDetails = NULL;
    TAG_INFO_NAME_FROM_TAG serviceInfo = { 0 };

    // Diagnostic info depends on the requester type
    switch (diagnosticBuffer->CallerType)
    {
    case KernelRequester:
        pOut += L"[DRIVER] ";
 //       wprintf_s(L"[DRIVER] ");

        // For drivers, collecn the path and description
        if (diagnosticBuffer->DeviceDescriptionOffset)
            requesterName = (PCWSTR)RtlOffsetToPointer(diagnosticBuffer, diagnosticBuffer->DeviceDescriptionOffset);
        else
            requesterName = L"Legacy Kernel Caller";

        if (diagnosticBuffer->DevicePathOffset)
            requesterDetails = (PCWSTR)RtlOffsetToPointer(diagnosticBuffer, diagnosticBuffer->DevicePathOffset);

        break;

    case UserProcessRequester:
    case UserSharedServiceRequester:
        /*
        wprintf_s(diagnosticBuffer->CallerType == UserProcessRequester ?
            L"[PROCESS (PID %d)] " :
            L"[SERVICE (PID %d)] ",
            diagnosticBuffer->ProcessId);
        *//*
        pOut += UserProcessRequester ? L"[PROCESS (PID " : L"[SERVICE (PID ";
        pOut += std::to_wstring(diagnosticBuffer->ProcessId);
        pOut += L")] ";


        // Collect the process name for processes and services
        if (diagnosticBuffer->ProcessImageNameOffset)
            requesterName = (PCWSTR)RtlOffsetToPointer(diagnosticBuffer, diagnosticBuffer->ProcessImageNameOffset);

        // For services, convert their tags to names
        if (diagnosticBuffer->CallerType == UserSharedServiceRequester)
        {
            PQUERY_TAG_INFORMATION I_QueryTagInformation = I_QueryTagInformationLoader();

            serviceInfo.InParams.dwPid = diagnosticBuffer->ProcessId;
            serviceInfo.InParams.dwTag = diagnosticBuffer->ServiceTag;

            if (I_QueryTagInformation &&
                I_QueryTagInformation(NULL, eTagInfoLevelNameFromTag, &serviceInfo) == ERROR_SUCCESS)
                requesterDetails = serviceInfo.OutParams.pszName;
        }

        break;

    default:
        pOut += L"[UNKNOWN] ";
 //       wprintf_s(L"[UNKNOWN] ");
        break;
    }
/*
    // Power requests maintain a counter
    if (Request->V4.PowerRequestCount[RequestType] > 1)
        wprintf_s(L"[%d times] ", Request->V4.PowerRequestCount[RequestType]);
*//*
    if (requesterDetails)
    {
        pOut += requesterName;
        pOut += L" (";
        pOut += requesterDetails;
        pOut += L")\r\n";
    }
 //       wprintf_s(L"%s (%s)\r\n", requesterName, requesterDetails);
    else
    {
        pOut += requesterName;
        pOut += L"\r\n";
    }
 //       wprintf_s(L"%s\r\n", requesterName);

    // The diagnostic buffer also stores the reason
    if (diagnosticBuffer->ReasonOffset)
    {
        PCOUNTED_REASON_CONTEXT_RELATIVE reason =
            (PCOUNTED_REASON_CONTEXT_RELATIVE)RtlOffsetToPointer(diagnosticBuffer, diagnosticBuffer->ReasonOffset);

        if (reason->Flags & POWER_REQUEST_CONTEXT_SIMPLE_STRING)
        {
            // Simple strings are packed into the buffer
            pOut += (PCWCHAR)RtlOffsetToPointer(reason, reason->SimpleStringOffset);
            pOut += L"\r\n";
 //           wprintf_s(L"%s\r\n", (PCWCHAR)RtlOffsetToPointer(reason, reason->SimpleStringOffset));
        }
        else if (reason->Flags & POWER_REQUEST_CONTEXT_DETAILED_STRING)
        {
            // Detailed strings are located in an external module

            HMODULE hModule = LoadLibraryExW(
                (PCWSTR)RtlOffsetToPointer(reason, reason->ResourceFileNameOffset),
                NULL,
                LOAD_LIBRARY_AS_DATAFILE
            );

            if (hModule)
            {
                PCWSTR reasonString;
                int reasonLength = LoadStringW(
                    hModule,
                    reason->ResourceReasonId,
                    (LPWSTR)&reasonString,
                    0
                );

                // TODO: substitute caller-supplied parameters

                if (reasonLength)
                {
                    pOut += reasonString;
                    pOut += L"\r\n";
                }

//                    wprintf_s(L"%s\r\n", reasonString);

                // Clean-up
                FreeLibrary(hModule);
            }
        }
    }

    // Clean-up
    if (serviceInfo.OutParams.pszName)
        LocalFree(serviceInfo.OutParams.pszName);

    return TRUE;
}
*//*
std::wstring DisplayRequests(
    _In_ PPOWER_REQUEST_LIST RequestList,
    _In_ POWER_REQUEST_TYPE Condition,
    _In_ LPCWSTR Caption
)
{
    std::wstring out;
    out = Caption;
    out +=L"\r\n";

    ULONG found = FALSE;

    for (ULONG i = 0; i < RequestList->Count; i++)
    {
        found = DisplayRequest(
            (PPOWER_REQUEST)RtlOffsetToPointer(RequestList, RequestList->PowerRequestOffsets[i]),
            Condition,
            out
        ) || found;
    }

    if (!found)
        out += L"None.";
    out += L"\r\n";
    return out;
}
*/
/*
int doPR()
{
    // Do not allow running under WoW64
    if (IsWoW64())
        return 1;

    NTSTATUS status;
    PPOWER_REQUEST_LIST buffer;
    ULONG bufferLength = 4096;

    do
    {
        buffer = (PPOWER_REQUEST_LIST)RtlAllocateHeap(
            RtlGetCurrentPeb()->ProcessHeap,
            0,
            bufferLength
        );

        if (!buffer)
        {
            wprintf_s(L"Not enough memory");
            return 1;
        }

        // Query the power request list
        status = NtPowerInformation(
            (POWER_INFORMATION_LEVEL)GetPowerRequestList,
            NULL,
            0,
            buffer,
            bufferLength
        );

        if (!NT_SUCCESS(status))
        {
            RtlFreeHeap(RtlGetCurrentPeb()->ProcessHeap, 0, buffer);
            buffer = NULL;

            // Prepare for expansion
            bufferLength += 4096;
        }

    } while (status == STATUS_BUFFER_TOO_SMALL);

    if (!IsSuccess(status, L"Querying power request list") || !buffer)
        return 1;

    InitializeSupportedModeCount();


    DisplayRequests(buffer, PowerRequestDisplayRequiredInternal, L"DISPLAY");
    DisplayRequests(buffer, PowerRequestSystemRequiredInternal, L"SYSTEM");
    DisplayRequests(buffer, PowerRequestAwayModeRequiredInternal, L"AWAYMODE");
    DisplayRequests(buffer, PowerRequestExecutionRequiredInternal, L"EXECUTION");
    DisplayRequests(buffer, PowerRequestPerfBoostRequiredInternal, L"PERFBOOST");
    DisplayRequests(buffer, PowerRequestActiveLockScreenInternal, L"ACTIVELOCKSCREEN");

    DisplayRequests(buffer, (POWER_REQUEST_TYPE)PowerRequestDisplayRequiredInternal, L"DISPLAY");
    DisplayRequests(buffer, (POWER_REQUEST_TYPE)PowerRequestSystemRequiredInternal, L"SYSTEM");
    DisplayRequests(buffer, (POWER_REQUEST_TYPE)PowerRequestAwayModeRequiredInternal, L"AWAYMODE");
    DisplayRequests(buffer, (POWER_REQUEST_TYPE)PowerRequestExecutionRequiredInternal, L"EXECUTION");
    DisplayRequests(buffer, (POWER_REQUEST_TYPE)PowerRequestPerfBoostRequiredInternal, L"PERFBOOST");
    DisplayRequests(buffer, (POWER_REQUEST_TYPE)PowerRequestActiveLockScreenInternal, L"ACTIVELOCKSCREEN");
    RtlFreeHeap(RtlGetCurrentPeb()->ProcessHeap, 0, buffer);

    return 0;
}
*/
/*
std::wstring UIM::getPowerRequestList(void)
{
    std::wstring output;
    NTSTATUS status;
    PPOWER_REQUEST_LIST buffer;
    ULONG bufferLength = 4096;

    do
    {
        buffer = (PPOWER_REQUEST_LIST)RtlAllocateHeap(
            RtlGetCurrentPeb()->ProcessHeap,
            0,
            bufferLength
        );

        if (!buffer)
        {
            return L"Not enough memory";
        }

        // Query the power request list
        status = NtPowerInformation(
            (POWER_INFORMATION_LEVEL)GetPowerRequestList,
            NULL,
            0,
            buffer,
            bufferLength
        );

        if (!NT_SUCCESS(status))
        {
            RtlFreeHeap(RtlGetCurrentPeb()->ProcessHeap, 0, buffer);
            buffer = NULL;

            // Prepare for expansion
            bufferLength += 4096;
        }

    } while (status == STATUS_BUFFER_TOO_SMALL);

    if (!IsSuccess(status, L"Querying power request list") || !buffer)
        return L"failed allocating memory";

    InitializeSupportedModeCount();

    
    DisplayRequests(buffer, PowerRequestDisplayRequiredInternal, L"DISPLAY");
    DisplayRequests(buffer, PowerRequestSystemRequiredInternal, L"SYSTEM");
    DisplayRequests(buffer, PowerRequestAwayModeRequiredInternal, L"AWAYMODE");
    DisplayRequests(buffer, PowerRequestExecutionRequiredInternal, L"EXECUTION");
    DisplayRequests(buffer, PowerRequestPerfBoostRequiredInternal, L"PERFBOOST");
    DisplayRequests(buffer, PowerRequestActiveLockScreenInternal, L"ACTIVELOCKSCREEN");
    
    output += DisplayRequests(buffer, (POWER_REQUEST_TYPE)PowerRequestDisplayRequiredInternal, L"DISPLAY");
    output += DisplayRequests(buffer, (POWER_REQUEST_TYPE)PowerRequestSystemRequiredInternal, L"SYSTEM");
    output += DisplayRequests(buffer, (POWER_REQUEST_TYPE)PowerRequestAwayModeRequiredInternal, L"AWAYMODE");
    output += DisplayRequests(buffer, (POWER_REQUEST_TYPE)PowerRequestExecutionRequiredInternal, L"EXECUTION");
    output += DisplayRequests(buffer, (POWER_REQUEST_TYPE)PowerRequestPerfBoostRequiredInternal, L"PERFBOOST");
    output += DisplayRequests(buffer, (POWER_REQUEST_TYPE)PowerRequestActiveLockScreenInternal, L"ACTIVELOCKSCREEN");
    RtlFreeHeap(RtlGetCurrentPeb()->ProcessHeap, 0, buffer);
    return output;
}
*/
bool UIM::isDisplayLock(bool& video_wake_lock, std::vector <std::wstring> & processes, std::wstring& message)
{
    NTSTATUS status;
    PPOWER_REQUEST_LIST buffer;
    ULONG bufferLength = 4096;

    video_wake_lock = false;

    do //allocate sufficient memory and get power information
    {
        buffer = (PPOWER_REQUEST_LIST)RtlAllocateHeap(RtlGetCurrentPeb()->ProcessHeap, 0, bufferLength);
        if (!buffer)
        {
            message = L"Failed to allocate memory when iterating power requests";
            return false;
        }
        status = NtPowerInformation( (POWER_INFORMATION_LEVEL)GetPowerRequestList, NULL, 0, buffer, bufferLength );
        if (!NT_SUCCESS(status))
        {
            RtlFreeHeap(RtlGetCurrentPeb()->ProcessHeap, 0, buffer);
            buffer = NULL;
            bufferLength += 4096;
        }
    } while (status == STATUS_BUFFER_TOO_SMALL);
    if (!buffer)
    {
        message = L"Failed to allocate memory when checking power requests";
        return false;
    }

    for (ULONG i = 0; i < buffer->Count; i++)
    {
        PDIAGNOSTIC_BUFFER diagnosticBuffer;
        PPOWER_REQUEST Request = (PPOWER_REQUEST)RtlOffsetToPointer(buffer, buffer->PowerRequestOffsets[i]);
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
        }
        
        if (reason_message == L"Video Wake Lock")
            video_wake_lock = true;
        if (requesterName != L"")
        {
            size_t lastSlash = requesterName.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos)
                processes.push_back(tools::tolower(requesterName.substr(lastSlash + 1)));
            else
                processes.push_back(tools::tolower(requesterName));
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