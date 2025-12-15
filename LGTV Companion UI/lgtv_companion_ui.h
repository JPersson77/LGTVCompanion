#pragma once
#include <Windows.h>
#include <string>
#include <vector>

struct DisplayInfo;
struct CUSTOMMSGBOXPARAMS
{
	LPCWSTR lpText;
	LPCWSTR lpCaption;
	UINT uType;
};

// UI dialogs
LRESULT CALLBACK									WndMainProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK									WndDeviceProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK									WndOptionsProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK									WndTopologyProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK									WndUserIdleProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK									WndWhitelistProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK									CustomMsgBoxProc(HWND, UINT, WPARAM, LPARAM);

// Supporting functions
bool												messageExistingProcess(std::wstring);
void												communicateWithService(std::wstring);
void												threadVersionCheck(HWND);
static BOOL CALLBACK								meproc(HMONITOR hMon, HDC hdc, LPRECT lprcMonitor, LPARAM pData);
std::vector<DisplayInfo>							queryDisplays();
bool												messageDaemon(std::wstring);
void												ipcCallback(std::wstring message, LPVOID pt);
void												prepareForUninstall(void);
int													customMsgBox(HWND hParent, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);


