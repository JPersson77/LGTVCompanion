#pragma once
#include <Windows.h>
#include <string>
#include <winevt.h>

struct Context;

// Service.cpp
bool													SvcInstall(void);
bool													SvcUninstall(void);
DWORD													SvcCtrlHandler(DWORD, DWORD, LPVOID, LPVOID);
VOID WINAPI												SvcMain(DWORD, LPTSTR*);
VOID													SvcReportStatus(DWORD, DWORD, DWORD, Context&);
VOID													SvcReportEvent(WORD, std::wstring);
std::wstring											SvcGetErrorMessage(DWORD dwErrorCode);
DWORD WINAPI											SvcEventLogSubscribeCallback(EVT_SUBSCRIBE_NOTIFY_ACTION Action, PVOID UserContext, EVT_HANDLE Event);
