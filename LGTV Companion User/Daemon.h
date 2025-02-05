#pragma once
#include <Windows.h>
#include <string>
#include <vector>

struct DisplayInfo;
LRESULT CALLBACK			WndProc(HWND, UINT, WPARAM, LPARAM);

bool						messageExistingProcess(void);
void						communicateWithService(std::string);
void						threadVersionCheck(HWND);
void						log(std::wstring input);
std::vector<DisplayInfo>	queryDisplays();
static BOOL	CALLBACK		meproc(HMONITOR hMon, HDC hdc, LPRECT lprcMonitor, LPARAM pData);
bool						checkDisplayTopology(void);
int							verifyTopology();
DWORD						checkRemoteStreamingProcesses(void);
bool						isFullscreenApplicationRunning(void);
void						remoteStreamingEvent(DWORD dwType);
void						workaroundFalseFullscreenWindows(void);
static BOOL	CALLBACK		enumWindowsProc(HWND hWnd, LPARAM lParam);
DWORD						sunshine_CheckLog(void);
std::string					sunshine_GetConfVal(std::string, std::string);
std::string					sunshine_GetLogFile();
void						ipcCallback(std::wstring, LPVOID);
std::wstring				getWndText(HWND);
bool						IsWindowElevated(HWND);
void						RawInput_AddToCache(const std::wstring& devicePath);
void						RawInput_ClearCache();