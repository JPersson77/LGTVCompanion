#pragma once
#include <string>
#include "../Common/preferences.h"

namespace UIM
{
//	std::wstring			getPowerRequestList(void);
	bool					isDisplayLock(bool& video_wake_lock, std::vector<std::wstring> &process, std::wstring& message);
	std::wstring			getProcessFromHWND(HWND);
	bool					preventUIM(Preferences &, std::wstring& message);
	bool					isFullscreenReported(void);
	bool					isProcessShowingFullscreen(DWORD, std::wstring = L"");
	bool					isWindowShowingFullscreen(HWND);
	static BOOL CALLBACK	enumWindowProc(HWND hWnd, LPARAM lParam);
}