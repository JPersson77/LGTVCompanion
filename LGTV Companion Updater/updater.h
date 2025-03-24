#pragma once
#include <Windows.h>
#include <string>

LRESULT CALLBACK			WndProc(HWND, UINT, WPARAM, LPARAM);
bool						isAnythingPreventingSilentUpdate(void);
bool						isOtherUpdaterWindowShown(bool focus = false, bool update = false);
bool						showWinNotification(int = 0);
void						log(std::wstring input, bool = false);
void						threadVersionCheck(HWND hWnd);
void						threadDownloadAndInstall(HWND hWnd);