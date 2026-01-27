#pragma once
#include <Windows.h>
#include <string>
#include <vector>

struct DisplayInfo;
LRESULT CALLBACK			WndProc(HWND, UINT, WPARAM, LPARAM);

void						closeExistingProcess(void);
void						communicateWithService(std::wstring, bool);
void						log(std::wstring input);
std::vector<DisplayInfo>	queryDisplays();
static BOOL	CALLBACK		meproc(HMONITOR hMon, HDC hdc, LPRECT lprcMonitor, LPARAM pData);
bool						checkDisplayTopology(void);
int							verifyTopology();
DWORD						checkRemoteStreamingProcesses(void);
void						remoteStreamingEvent(DWORD dwType);
DWORD						sunshine_CheckLog(std::string);
std::string					sunshine_GetConfVal(std::string, std::string);
std::vector<std::string>	sunshine_GetLogFiles();
void						ipcCallback(std::wstring, LPVOID);
bool						IsWindowElevated(HWND);
bool						RawInput_AddToCache(const std::wstring& devicePath);
void						RawInput_ClearCache();
bool						isFullscreenApplicationRunning(void);
void						workaroundFalseFullscreenWindows(void);
static BOOL	CALLBACK		enumWindowsProc(HWND hWnd, LPARAM lParam);

