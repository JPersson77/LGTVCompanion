#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_ARM64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='arm64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#include "updater.h"
#include "../Common/common_app_define.h"
#include "../Common/tools.h"
#include "WinToastLib/include/wintoastlib.h" // wait for vcpkg action to update and include v1.3.1
#include <urlmon.h>
#include <thread>
#include <vector>
#include <TlHelp32.h>
#include <ShObjIdl.h>
#include <shellapi.h>
#include <nlohmann/json.hpp>

#include "resource.h"

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "Shell32.lib")

using namespace WinToastLib;

#define			APPNAME_SHORT							L"LGTVupdater"
#define			APPNAME_FULL							L"LGTV Companion Updater"
#define			NOTIFY_NEW_PROCESS						1

#define			TOAST_NEW_VERSION_AVAILABLE				0
#define			TOAST_FIRST_RUN							1

#define			APP_FOCUS_WINDOW						(WM_USER+1)
#define			APP_NEW_VERSION							(WM_USER+2)
#define			APP_NO_NEW_VERSION						(WM_USER+3)
#define			APP_DO_UPGRADE							(WM_USER+4)
#define			APP_UPGRADE_FINISHED					(WM_USER+5)
#define			APP_UPGRADE_FAILED						(WM_USER+6)
#define			APP_CHECK_VERSION						(WM_USER+7)
#define			APP_VERSION_CHECK_FAIL					(WM_USER+8)
#define         APP_SET_MESSAGEFILTER					(WM_USER+11)

#define			TIMER_EXIT								10
#define			TIMER_EXIT_DELAY						30000

#define			SHOW_UI									1
#define			FIRST_RUN								2
#define			SILENT_INSTALL							3
#define			VERSION_CHECK							4

// Globals:
HINSTANCE                       h_instance;  // current instance
HWND                            h_main_wnd = NULL;
HBRUSH                          h_backbrush;
INT64                           id_toast = NULL;
int								work = 0;
std::string						downloadURL = "";
HFONT							h_edit_medium_font;
UINT							custom_updater_update_message;
UINT							custom_updater_focus_message;
UINT							custom_updater_close_message;

class WinToastHandler : public WinToastLib::IWinToastHandler
{
public:
	WinToastHandler(){}
	void toastActivated() const override {
	}
	void toastActivated(int actionIndex) const override {
		if(work == VERSION_CHECK)
		{
			ShowWindow(h_main_wnd, SW_SHOW);
			work = SHOW_UI;
			PostMessage(h_main_wnd, APP_NEW_VERSION, NULL, NULL);
			PostMessage(h_main_wnd, APP_FOCUS_WINDOW, NULL, NULL);
		}
	}
	void toastActivated(const char* response) const override {
		if (work == VERSION_CHECK)
		{
			ShowWindow(h_main_wnd, SW_SHOW);
			work = SHOW_UI;
			PostMessage(h_main_wnd, APP_NEW_VERSION, NULL, NULL);
			PostMessage(h_main_wnd, APP_FOCUS_WINDOW, NULL, NULL);
		}
	}
	void toastDismissed(WinToastDismissalReason state) const override {
	}
	void toastFailed() const override {
	}
private:
};

//Application entry point
int APIENTRY wWinMain(_In_ HINSTANCE Instance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(nCmdShow);
	
	MSG msg;
	h_instance = Instance;

	custom_updater_focus_message = RegisterWindowMessage(CUSTOM_MESSAGE_UPD_FOCUS);
	custom_updater_update_message = RegisterWindowMessage(CUSTOM_MESSAGE_UPD_REFRESH);
	custom_updater_close_message = RegisterWindowMessage(CUSTOM_MESSAGE_UPD_CLOSE);

	//commandline processing
	if (lpCmdLine)
	{
		std::wstring command_line = lpCmdLine;
		transform(command_line.begin(), command_line.end(), command_line.begin(), ::tolower);
		if (command_line == L"-firstrun")
			work = FIRST_RUN;
		else if (command_line == L"-silent") 
		{
			if (isAnythingPreventingSilentUpdate()) // don't perform silent install if application has open UI windows
				return 0;
			work = SILENT_INSTALL;
		}
		else if (command_line == L"-versioncheck")
		{
			if (isOtherUpdaterWindowShown(false, true)) // if another updater window is displayed, trigger update check in that window/process instead
				return 0;
			work = VERSION_CHECK;
		}
	}
	if (work == 0)
	{
		if (isOtherUpdaterWindowShown(true, false)) // if another updater window is displayed, focus that window/process instead
			return 0;
		work = SHOW_UI;
	}
	// Initialize WinToastLib
	WinToast::WinToastError error;
	WinToast::instance()->setAppName(APPNAME_FULL);
	WinToast::instance()->setAppUserModelId(L"JPersson.LGTVCompanion.19");
	WinToast::instance()->initialize(&error);
	WinToast::instance()->setShortcutPolicy(WinToast::SHORTCUT_POLICY_REQUIRE_NO_CREATE);
	
	// create main window (dialog)
	std::wstring window_title;
	window_title = APPNAME_FULL;
	window_title += L" v";
	window_title += APP_VERSION;
	h_backbrush = CreateSolidBrush(0x00ffffff);
	HICON h_icon = LoadIcon(h_instance, MAKEINTRESOURCE(IDI_ICON1));
	h_edit_medium_font = CreateFont(22, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("Calibri"));
	h_main_wnd = CreateDialog(h_instance, MAKEINTRESOURCE(IDD_UPDATER), NULL, (DLGPROC)WndProc);
	SendMessage(h_main_wnd, WM_SETICON, ICON_BIG, (LPARAM)h_icon);
	SetWindowText(h_main_wnd, window_title.c_str());
	
	// message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage(h_main_wnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	// clean up
	if (WinToastLib::WinToast::instance()->isInitialized())
	{
		if (id_toast)
		{
			WinToastLib::WinToast::instance()->hideToast(id_toast);
			WinToastLib::WinToast::instance()->clear();
			id_toast = NULL;
		}
	}
	DeleteObject(h_backbrush);
	DeleteObject(h_edit_medium_font);
	return (int)msg.wParam;
}

//   Process messages for the main window.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// handle intra-app custom messages
	if (message == custom_updater_focus_message)
	{
		if (IsWindowVisible(hWnd))
			SendMessage(hWnd, APP_FOCUS_WINDOW, NULL, NULL);
	}
	else if (message == custom_updater_update_message)
	{
		if(IsWindowEnabled(GetDlgItem(hWnd, IDC_REFRESH)))
			PostMessage(hWnd, APP_CHECK_VERSION, NULL, NULL);
	}
	else if (message == custom_updater_close_message)
	{
		if(lParam == NULL)
		{
			if (!IsWindowVisible(hWnd))
				PostMessage(hWnd, WM_CLOSE, NULL, NULL);
		}
		else
			PostMessage(hWnd, WM_CLOSE, NULL, NULL);
	}

	switch (message)
	{
	case WM_INITDIALOG:
	{
		SetCurrentProcessExplicitAppUserModelID(L"JPersson.LGTVCompanion.19");
		SendDlgItemMessage(hWnd, IDC_LOG, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SetWindowPos(hWnd, NULL, 0, 0, 600, 300, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
		PostMessage(hWnd, APP_SET_MESSAGEFILTER, NULL, NULL);
		switch (work)
		{
		case FIRST_RUN:
		{
			showWinNotification(TOAST_FIRST_RUN);
			SetTimer(hWnd, TIMER_EXIT, TIMER_EXIT_DELAY, (TIMERPROC)NULL);
		}break;
		case SHOW_UI:
			ShowWindow(hWnd, SW_SHOW);
		case VERSION_CHECK:
		case SILENT_INSTALL:
			PostMessage(hWnd, APP_CHECK_VERSION, NULL, NULL);
			break;
		default:break;
		}	
	}break;
	case APP_SET_MESSAGEFILTER:
	{
		ChangeWindowMessageFilterEx(hWnd, custom_updater_focus_message, MSGFLT_ALLOW, NULL);
		ChangeWindowMessageFilterEx(hWnd, custom_updater_update_message, MSGFLT_ALLOW, NULL);
		ChangeWindowMessageFilterEx(hWnd, custom_updater_close_message, MSGFLT_ALLOW, NULL);
	}break;
	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{

			case IDC_BUTTON:
			{
				PostMessage(hWnd, APP_DO_UPGRADE, NULL, NULL);
			}break;
			default:break;
			}
		}break;
		default:break;
		}
	}break;

	case WM_NOTIFY:
	{
		switch (((NMHDR*)lParam)->code)
		{
		case NM_CLICK:
		{
			if (wParam == IDC_REFRESH)
				PostMessage(hWnd, APP_CHECK_VERSION, NULL, NULL);
			else if (wParam == IDC_GITHUB)
				ShellExecute(0, 0, NEWRELEASELINK, 0, 0, SW_SHOW);
		}break;
		default:break;
		}
	}break;
	case WM_TIMER:
	{
		switch (wParam)
		{
		case TIMER_EXIT:
		{
			PostMessage(hWnd, WM_CLOSE, NULL, NULL);
		}break;
		default:break;
		}
	}break;
	case APP_CHECK_VERSION:
	{
		EnableWindow(GetDlgItem(hWnd, IDC_BUTTON), false);
		EnableWindow(GetDlgItem(hWnd, IDC_REFRESH), false);
		std::thread thread_obj(threadVersionCheck, hWnd);
		thread_obj.detach();
	}break;
	case APP_VERSION_CHECK_FAIL:
	{
		switch (work)
		{
		case SILENT_INSTALL:
		{
			PostMessage(hWnd, WM_CLOSE, NULL, NULL);
		}break;
		case VERSION_CHECK:
		{
			PostMessage(hWnd, WM_CLOSE, NULL, NULL);
		}break;
		case SHOW_UI:
		{
			EnableWindow(GetDlgItem(hWnd, IDC_BUTTON), false);
			EnableWindow(GetDlgItem(hWnd, IDC_REFRESH), true);
		}break;
		default:break;
		}
	}break;
	case APP_FOCUS_WINDOW:
	{
		SetForegroundWindow(hWnd);
	}break;
	case APP_NEW_VERSION:
	{
		switch (work)
		{
		case SILENT_INSTALL:
		{
			PostMessage(hWnd, APP_DO_UPGRADE, NULL, NULL);
		}break;
		case VERSION_CHECK:
		{
			showWinNotification(TOAST_NEW_VERSION_AVAILABLE);
			EnableWindow(GetDlgItem(hWnd, IDC_BUTTON), true);
			EnableWindow(GetDlgItem(hWnd, IDC_REFRESH), true);
		}break;
		case SHOW_UI:
		{
			EnableWindow(GetDlgItem(hWnd, IDC_BUTTON), true);
			EnableWindow(GetDlgItem(hWnd, IDC_REFRESH), true);
		}break;
		default:break;
		}
	}break;
	case APP_NO_NEW_VERSION:
	{
		switch (work)
		{
		case SILENT_INSTALL:
		{
			PostMessage(hWnd, WM_CLOSE, NULL, NULL);
		}break;
		case VERSION_CHECK:
		{
			PostMessage(hWnd, WM_CLOSE, NULL, NULL);
		}break;
		case SHOW_UI:
		{
			log(L"There is currently no updated version available", true);
			EnableWindow(GetDlgItem(hWnd, IDC_BUTTON), false);
			EnableWindow(GetDlgItem(hWnd, IDC_REFRESH), true);
		}break;
		default:break;
		}
	}break;
	case APP_DO_UPGRADE:
	{
		EnableWindow(GetDlgItem(hWnd, IDC_BUTTON), false);
		EnableWindow(GetDlgItem(hWnd, IDC_REFRESH), false);
		std::thread thread_obj(threadDownloadAndInstall, hWnd);
		thread_obj.detach();
	}break;
	case APP_UPGRADE_FINISHED:
	{
		EnableWindow(GetDlgItem(hWnd, IDC_BUTTON), false);
		EnableWindow(GetDlgItem(hWnd, IDC_REFRESH), true);
	}break;
	case APP_UPGRADE_FAILED:
	{
		switch (work)
		{
		case SILENT_INSTALL:
		{
			PostMessage(hWnd, WM_CLOSE, NULL, NULL);
		}break;
		case VERSION_CHECK:
		case SHOW_UI:
		{
			std::wstring logmsg = APPNAME;
			logmsg += L" was NOT updated to the latest version";
			log(logmsg);
			EnableWindow(GetDlgItem(hWnd, IDC_BUTTON), false);
			EnableWindow(GetDlgItem(hWnd, IDC_REFRESH), true);
		}break;
		default:break;
		}
	}break;
	case WM_SIZE: {
		if (!lParam)
			break;
		int width = LOWORD(lParam);
		int height = HIWORD(lParam);

		// Adjust the size of the edit control
		SetWindowPos(GetDlgItem(hWnd, IDC_LOG), nullptr, 0, 0, width, height - 30, SWP_NOZORDER);
		SetWindowPos(GetDlgItem(hWnd, IDC_BUTTON), nullptr, width - 150, height - 28, 140, 26, SWP_NOZORDER);
		SetWindowPos(GetDlgItem(hWnd, IDC_REFRESH), nullptr, width - 310, height - 23, 150, 14, SWP_NOZORDER);
		SetWindowPos(GetDlgItem(hWnd, IDC_GITHUB), nullptr, 10, height - 23, 162, 14, SWP_NOZORDER);
		break;
	}
	case WM_GETMINMAXINFO: {
		MINMAXINFO* pMinMaxInfo = (MINMAXINFO*)lParam;

		// Set the minimum width and height
		pMinMaxInfo->ptMinTrackSize.x = 460;
		pMinMaxInfo->ptMinTrackSize.y = 250;
		return 0; // Return 0 to indicate the message was handled
	}
	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		//		if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_ENABLE))
		{
			SetBkMode(hdcStatic, TRANSPARENT);
		}
		return(INT_PTR)h_backbrush;
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

		FillRect(backbuffDC, &rc, (HBRUSH)h_backbrush);

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
		if (WinToastLib::WinToast::instance()->isInitialized())
		{
			if (id_toast)
			{
				WinToastLib::WinToast::instance()->hideToast(id_toast);
				WinToastLib::WinToast::instance()->clear();
				id_toast = NULL;
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

bool showWinNotification(int type)
{
	if (WinToastLib::WinToast::instance()->isInitialized())
	{
		TCHAR buffer[MAX_PATH] = { 0 };
		if (!GetModuleFileName(NULL, buffer, MAX_PATH))
			return false;
		std::wstring imgpath = buffer;
		std::wstring::size_type pos = imgpath.find_last_of(L"\\/");
		imgpath = imgpath.substr(0, pos + 1);
		imgpath += L"mainicon.ico";
		WinToastLib::WinToastTemplate templ(WinToastTemplate::ImageAndText02);
		switch (type)
		{
		case TOAST_NEW_VERSION_AVAILABLE:
		{
			templ.setImagePath(imgpath);
			templ.setTextField(L"Yay! A new version is available", WinToastLib::WinToastTemplate::FirstLine);
			templ.setTextField(L"Please install the new version to keep up to date with bugfixes and features", WinToastLib::WinToastTemplate::SecondLine);
			templ.addAction(L"Check it out!");
			templ.setDuration(WinToastLib::WinToastTemplate::Duration::System);
		}break;
		case TOAST_FIRST_RUN:
		{
			std::wstring toast;
			toast = L"Good news! LGTV Companion v";
			toast += APP_VERSION;
			toast += L" is now installed.";
			templ.setImagePath(imgpath);
			templ.setTextField(toast, WinToastLib::WinToastTemplate::FirstLine);
			templ.setTextField(L"Designed to protect and extend the features of your WebOS devices when used as a PC monitor", WinToastLib::WinToastTemplate::SecondLine);
			templ.setDuration(WinToastLib::WinToastTemplate::Duration::Short);
		}break;
		default:break;
		}

		if (id_toast > 1)
		{
			WinToastLib::WinToast::instance()->hideToast(id_toast);
			WinToastLib::WinToast::instance()->clear();
		}

		id_toast = WinToastLib::WinToast::instance()->showToast(templ, new WinToastHandler);
		if (id_toast == -1L)
			return false;
	}
	else
		return false;
	return true;
}
void log(std::wstring input, bool clear)
{
	if (clear)
		SetWindowText(GetDlgItem(h_main_wnd, IDC_LOG), L"");

	std::wstring log_message = input;
	log_message += L"\r\n";

	int TextLen = (int)SendMessage(GetDlgItem(h_main_wnd, IDC_LOG), WM_GETTEXTLENGTH, 0, 0);
	SendMessage(GetDlgItem(h_main_wnd, IDC_LOG), EM_SETSEL, (WPARAM)TextLen, (LPARAM)TextLen);
	SendMessage(GetDlgItem(h_main_wnd, IDC_LOG), EM_REPLACESEL, FALSE, (LPARAM)log_message.c_str());
	InvalidateRect(GetDlgItem(h_main_wnd, IDC_LOG), NULL, true);
	UpdateWindow(GetDlgItem(h_main_wnd, IDC_LOG));
	return;
}
void threadVersionCheck(HWND hWnd)
{
	IStream* stream;
	char buff[100];
	std::string s;
	unsigned long bytesRead;
	nlohmann::json json_data;

	if (work == SHOW_UI)
	{
		Sleep(50);
		log(L"Looking for updated version...", true);
		Sleep(1500);
	}
	downloadURL = "";
	if (URLOpenBlockingStream(0, VERSIONCHECKLINK, &stream, 0, 0))
	{
		log(L"Failed to open update URL", false);
		log(VERSIONCHECKLINK);
		PostMessage(hWnd, APP_VERSION_CHECK_FAIL, NULL, NULL);
		return;
	}
	while (true)
	{
		stream->Read(buff, 100, &bytesRead);

		if (0U == bytesRead)
			break;
		s.append(buff, bytesRead);
	};
	stream->Release();
	if (s.length() < 20)
	{
		if(s.length() == 0)
			log(L"No data received", false);
		else
		{
			log(L"Invalid data received", false);
			log(tools::widen(s));
		}
		PostMessage(hWnd, APP_VERSION_CHECK_FAIL, NULL, NULL);
		return;
	}

	try
	{
		json_data = nlohmann::json::parse(s);
	}
	catch (...)
	{
		log(L"Invalid JSON received", false);
		log(tools::widen(s));
		PostMessage(hWnd, APP_VERSION_CHECK_FAIL, NULL, NULL);
		return;
	}
	if (!json_data["tag_name"].empty() && json_data["tag_name"].is_string())
	{
		std::string remote = json_data["tag_name"];
		std::vector <std::string> local_ver = tools::stringsplit(tools::narrow(APP_VERSION), ".");
		std::vector <std::string> remote_ver = tools::stringsplit(remote, "v.");

		if (local_ver.size() < 3 || remote_ver.size() < 3)
		{
			log(L"Invalid version numbering - expected X.X.X", false);
			PostMessage(hWnd, APP_VERSION_CHECK_FAIL, NULL, NULL);
			return;
		}
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
			
			for (const auto& asset : json_data["assets"])
			{
				if (!asset["browser_download_url"].empty() && asset["browser_download_url"].is_string())
				{

					std::string url = asset["browser_download_url"];
					url = tools::tolower(url);
#if defined _M_ARM64
					if (url.find("arm64") != std::string::npos)
#else
					if (url.find("x64") != std::string::npos)
#endif				
						downloadURL = asset["browser_download_url"];
				}
			}
			if (downloadURL == "")
			{
				log(L"Invalid or no download link received", false);
				PostMessage(hWnd, APP_VERSION_CHECK_FAIL, NULL, NULL);
				return;
			}


			std::wstring log_msg = APPNAME;
			log_msg += L" ";
			log_msg += tools::widen(remote);
			log_msg += L" is available for download\r\n";
			log(log_msg, true);

			if (!json_data["body"].empty() && json_data["body"].is_string())
			{
				std::string patch_log = json_data["body"];
				log(tools::widen(patch_log));
			}
			PostMessage(hWnd, APP_NEW_VERSION, 0, 0);
		}
		else
			PostMessage(hWnd, APP_NO_NEW_VERSION, 0, 0);
	}
	return;
}
void threadDownloadAndInstall(HWND hWnd)
{
	log(L"Downloading installer - please wait...", true);
	// Get temp path
	wchar_t temp_path[MAX_PATH];
	if (!GetTempPath(MAX_PATH, temp_path)) {
		log(L"Could not download the installer - failed to get TEMP directory", false);
		PostMessage(hWnd, APP_UPGRADE_FAILED, NULL, NULL);
		return;
	}
	// Create temp file path
	std::wstring file_name = L"LGTVCinstaller.";
	file_name += std::to_wstring(time(0));
	file_name += L".msi";
	std::wstring full_path = std::wstring(temp_path) + file_name;

	HRESULT hr = URLDownloadToFile(NULL, tools::widen(downloadURL).c_str(), full_path.c_str(), 0, NULL);
	if (FAILED(hr)) 
	{
		std::wstring logmsg = L"Failed to download the installer: ";
		logmsg += std::to_wstring(hr);
		log(logmsg);
		PostMessage(hWnd, APP_UPGRADE_FAILED, NULL, NULL);
		return ;
	}
	log(L"Installer downloaded - installing...", false);

	// Build the installer command line
	std::wstring command;
	if(work == SILENT_INSTALL)
		command = L"/i \"" + full_path + L"\" /qn /norestart";
	else
		command = L"/i \"" + full_path + L"\"";

	ShellExecute(NULL, L"open", L"msiexec", command.c_str(), temp_path, (work == SILENT_INSTALL) ? SW_HIDE : SW_NORMAL);
	PostMessage(hWnd, APP_UPGRADE_FINISHED, NULL, NULL);
	return;
}

bool isAnythingPreventingSilentUpdate(void)
{
	std::vector<std::wstring> app_windows;
	app_windows.push_back(L"LGTV Companion Updater");
	app_windows.push_back(L"LGTV Companion Daemon");
	app_windows.push_back(L"LGTV Companion");
	for (auto& win : app_windows)
	{
		win.append(L" v");
		win.append(APP_VERSION);
		HWND h_win = FindWindow(NULL, win.c_str());
		if(h_win && IsWindowVisible(h_win))
			return true;
	}
	return false;
}
bool isOtherUpdaterWindowShown(bool focus, bool update )
{
	//close Updater windows that are not shown
	PostMessage(HWND_BROADCAST, custom_updater_close_message, NULL, NULL);
	Sleep(100);
	std::wstring window_title;
	window_title = APPNAME_FULL;
	window_title += L" v";
	window_title += APP_VERSION;
	HWND h_wnd = FindWindow(NULL, window_title.c_str());

	if(h_wnd)
	{
		if (focus)
		{
			AllowSetForegroundWindow(ASFW_ANY);
			PostMessage(HWND_BROADCAST, custom_updater_focus_message, NULL, NULL);
		}
		if (update)
			PostMessage(HWND_BROADCAST, custom_updater_update_message, NULL, NULL);
		return true;
	}
	return false;
}