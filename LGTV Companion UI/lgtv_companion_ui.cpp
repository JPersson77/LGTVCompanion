/*
DESCRIPTION
	<<<< LGTV Companion >>>>

	The application (UI and service) controls your WebOS (LG) Television.

	Using this application allows for your WebOS display to shut off and turn on
	in reponse to to the PC shutting down, rebooting, entering low power modes as well as
	and when user is afk (idle). Like a normal PC-monitor. Or, alternatively this application
	can be used as a command line tool to turn your displays on or off.

BACKGROUND
	With the rise in popularity of using OLED TVs as PC monitors, it is apparent that standard
	functionality of PC-monitors is missing. Particularly turning the display on or off in
	response to power events in windows. With OLED monitors this is particularly important to
	prevent "burn-in", or more accurately pixel-wear.

BUILD INSTRUCTIONS AND DEPENDENCIES
	First, to be able to build the projects (UI, Service, Daemon) in Visual Studio 2022 please 
	ensure that Vcpkg (https://github.com/microsoft/vcpkg) is installed. Vcpkg is a free 
	Library Manager for Windows. You should use Vcpkg in one of two ways:
	
	1) A Vcpkg manifest is included with the source code and the necessary dependencies
	will be automatically downloaded, configured and installed, if you choose to enable it.

	To enable the manifest please open the properties for each project in the solution, then 
	navigate to the vcpkg section and Select "Yes" for "Use Manifest". Do so for both the 
	"Debug" and "Release" project configurations.

	2) Alternatively, You can manually install the dependencies, with the following commands:

		vcpkg install nlohmann-json:x64-windows-static
		vcpkg install boost-asio:x64-windows-static
		vcpkg install boost-utility:x64-windows-static
		vcpkg install boost-date-time:x64-windows-static
		vcpkg install boost-beast:x64-windows-static
		vcpkg install wintoast:x64-windows-static
		vcpkg install openssl:x64-windows-static
	
	Secondly, to be able to build the setup package please ensure that the WiX Toolset is 
	installed (https://wixtoolset.org/) and that the WiX Toolset Visual Studio Extension 
	(WiX v3	Visual Studio 2022 Extension) is installed.

INSTALLATION, USAGE ETC

	https://github.com/JPersson77/LGTVCompanion

LICENSE
	
	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
	files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
	modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
	WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
	COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
	ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

COPYRIGHT
	Copyright (c) 2021-2026 Jörgen Persson
*/

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN
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

#include "lgtv_companion_ui.h"
#include "../Common/preferences.h"
#include "../Common/ipc_v2.h"
#include "../Common/common_app_define.h"
#include "../Common/tools.h"
#include "../Common/log.h"

#include <Shlobj_core.h>
#include <sstream>
#include <winevt.h>
#include <thread>
#include <shellapi.h>
#include <initguid.h>
#include <functiondiscoverykeys.h>
#include <SetupAPI.h>
#include <urlmon.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <mutex>
#include "resource.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "wevtapi.lib")
#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Shell32.lib")

#define									APPNAME_SHORT					L"LGTVcomp"
#define									DEVICEWINDOW_TITLE_ADD          L"Add device"
#define									DEVICEWINDOW_TITLE_MANAGE       L"Configure device"

#define									COLOR_STATIC					0x00555555
#define									COLOR_RED						0x00000099
#define									COLOR_GREEN						0x00009900
#define									COLOR_BLUE						0x00772222

// User defined messages
#define									APP_MESSAGE_ADD                 WM_USER+1
#define									APP_MESSAGE_MANAGE              WM_USER+2
#define									APP_MESSAGE_SCAN                WM_USER+3
#define									APP_MESSAGE_TEST                WM_USER+4
#define									APP_MESSAGE_TURNON              WM_USER+5
#define									APP_MESSAGE_TURNOFF             WM_USER+6
#define									APP_MESSAGE_REMOVE              WM_USER+7
#define									APP_MESSAGE_APPLY               WM_USER+8
#define									APP_NEW_VERSION                 WM_USER+9
#define									APP_TOP_PHASE_1		            WM_USER+10
#define									APP_TOP_PHASE_2	                WM_USER+11
#define									APP_TOP_PHASE_3                 WM_USER+12
#define									APP_TOP_NEXT_DISPLAY            WM_USER+13
#define									APP_LISTBOX_EDIT	            WM_USER+14
#define									APP_LISTBOX_ADD					WM_USER+15
#define									APP_LISTBOX_DELETE	            WM_USER+16
#define									APP_LISTBOX_REDRAW				WM_USER+17
#define									COPYDATA_MUTEX_WAIT				10

// Global Variables:
HINSTANCE								h_instance;  // current instance
HWND									h_main_wnd = NULL;
HWND									h_device_wnd = NULL;
HWND									h_options_wnd = NULL;
HWND									h_topology_wnd = NULL;
HWND									h_user_idle_mode_wnd = NULL;
HWND									h_whitelist_wnd = NULL;
HWND									h_custom_messagebox_wnd = NULL;
HBRUSH									h_backbrush;
HFONT									h_large_edit_font;
HFONT									h_edit_font;
HFONT									h_edit_medium_font;
HFONT									h_edit_small_font;
HFONT									h_edit_medium_bold_font;
HMENU									h_popup_menu;
HICON									h_icon_options;
HICON									h_icon_topology;
HICON									h_icon_cog;
WSADATA									WSAData;
int										i_top_configuration_phase;
int										i_top_configuration_display;
bool									reset_api_keys = false;
std::vector<Preferences::ProcessList>	process_list_temp;
std::shared_ptr<IpcClient2>				p_pipe_client;
std::shared_ptr<Logging>				logger;
UINT									custom_daemon_restart_message;
UINT									custom_daemon_idle_message;
UINT									custom_daemon_unidle_message;
UINT									custom_daemon_close_message;
UINT									custom_updater_close_message;
UINT									custom_UI_close_message;
inline static std::mutex				copydata_mutex_;

Preferences								Prefs(CONFIG_FILE);

struct DisplayInfo {					// Display info
	DISPLAYCONFIG_TARGET_DEVICE_NAME	target;
	HMONITOR							hMonitor;
	HDC									hdcMonitor;
	RECT								rcMonitor2;
	MONITORINFOEX						monitorinfo;
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
	std::wstring command_line;
	h_instance = Instance;

	custom_daemon_close_message = RegisterWindowMessage(CUSTOM_MESSAGE_CLOSE);
	custom_updater_close_message = RegisterWindowMessage(CUSTOM_MESSAGE_UPD_CLOSE);
	custom_daemon_restart_message = RegisterWindowMessage(CUSTOM_MESSAGE_RESTART);
	custom_daemon_idle_message = RegisterWindowMessage(CUSTOM_MESSAGE_IDLE);
	custom_daemon_unidle_message = RegisterWindowMessage(CUSTOM_MESSAGE_UNIDLE);
	custom_UI_close_message = RegisterWindowMessage(CUSTOM_MESSAGE_LGTVC_CLOSE);

	if (lpCmdLine)
		command_line = lpCmdLine;
	if (command_line == L"-prepare_for_uninstall")
	{
		prepareForUninstall();
		return false;
	}
	// if commandline directed towards daemon
	if (messageDaemon(command_line))
		return false;
	// if the app is already running as another process, send the command line parameters to that process and exit
	if (messageExistingProcess(command_line))
		return false;
	if(!Prefs.isInitialised())
	{
		MessageBox(NULL, L"Error when reading the configuration file.\n\nApplication terminated!", APPNAME, MB_OK | MB_ICONERROR);
		return false; 
	}
	// tweak prefs regarding windows topology
	bool bTop = false;
	for (auto m : Prefs.devices_)
	{
		if (m.uniqueDeviceKey != "")
			bTop = true;
	}
	Prefs.topology_support_ = bTop ? Prefs.topology_support_ : false;

	std::wstring file = tools::widen(Prefs.data_path_);
	file += L"ui_log.txt";
	logger = std::make_shared<Logging>(0, file);

	

	// Initiate PipeClient IPC
	p_pipe_client = std::make_shared<IpcClient2>(PIPENAME, ipcCallback, (LPVOID)NULL);
	h_backbrush = CreateSolidBrush(0x00ffffff);
	h_edit_small_font = CreateFont(18, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("Calibri"));

	//parse and execute command line parameters when applicable and then exit
	if (Prefs.devices_.size() > 0 && command_line.size() > 0)
	{
		communicateWithService(command_line);
//		Sleep(100);
//		p_pipe_client->terminate();
		DeleteObject(h_backbrush);
		DeleteObject(h_edit_small_font);

	
		return false;
	}

	h_large_edit_font = CreateFont(32, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("Calibri"));
	h_edit_font = CreateFont(26, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("Calibri"));
	h_edit_medium_font = CreateFont(22, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("Calibri"));
	h_edit_medium_bold_font = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH, TEXT("Calibri"));
	h_popup_menu = LoadMenu(h_instance, MAKEINTRESOURCE(IDR_BUTTONMENU));

	INITCOMMONCONTROLSEX icex;           // Structure for control initialization.
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_UPDOWN_CLASS | ICC_STANDARD_CLASSES | ICC_USEREX_CLASSES;
	InitCommonControlsEx(&icex);

	h_icon_options = (HICON)LoadImageW(h_instance, MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, 25, 25, LR_DEFAULTCOLOR | LR_DEFAULTSIZE | LR_SHARED);
	h_icon_topology = (HICON)LoadImageW(h_instance, MAKEINTRESOURCE(IDI_ICON4), IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR | LR_DEFAULTSIZE | LR_SHARED);
	h_icon_cog = (HICON)LoadImageW(h_instance, MAKEINTRESOURCE(IDI_ICON3), IMAGE_ICON, 25, 25, LR_DEFAULTCOLOR | LR_DEFAULTSIZE | LR_SHARED);

	WSAStartup(MAKEWORD(2, 2), &WSAData);

	// create main window (dialog)
	std::wstring window_title;
	window_title = APPNAME;
	window_title += L" v";
	window_title += APP_VERSION;
	h_main_wnd = CreateDialog(h_instance, MAKEINTRESOURCE(IDD_MAIN), NULL, (DLGPROC)WndMainProc);
	SetWindowText(h_main_wnd, window_title.c_str());
	ShowWindow(h_main_wnd, SW_SHOW);
	UpdateWindow(h_main_wnd);
	// spawn thread to check for updated version of the app.
	if (Prefs.updater_mode_ != PREFS_UPDATER_OFF)
	{
		std::thread thread_obj(threadVersionCheck, h_main_wnd);
		thread_obj.detach();
	}
	// message loop:
	// don't call DefWindowProc for modeless dialogs. Return true for handled messages or false otherwise
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage(h_main_wnd, &msg) &&
			!IsDialogMessage(h_device_wnd, &msg) &&
			!IsDialogMessage(h_options_wnd, &msg) &&
			!IsDialogMessage(h_topology_wnd, &msg) &&
			!IsDialogMessage(h_user_idle_mode_wnd, &msg) &&
			!IsDialogMessage(h_whitelist_wnd, &msg) &&
			!IsDialogMessage(h_custom_messagebox_wnd, &msg) &&
			!IsDialogMessage(h_user_idle_mode_wnd, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

//	p_pipe_client->terminate();

	//clean up
	DeleteObject(h_backbrush);
	DeleteObject(h_edit_medium_bold_font);
	DeleteObject(h_edit_font);
	DeleteObject(h_edit_medium_font);
	DeleteObject(h_edit_small_font);
	DestroyMenu(h_popup_menu);
	DestroyIcon(h_icon_options);
	DestroyIcon(h_icon_topology);
	DestroyIcon(h_icon_cog);

	WSACleanup();

	return (int)msg.wParam;
}

//   Process messages for the main window.
LRESULT CALLBACK WndMainProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	std::wstring str;
	if (message == custom_UI_close_message)
	{
		if (h_custom_messagebox_wnd)
			DestroyWindow(h_custom_messagebox_wnd);
		if (h_whitelist_wnd)
			DestroyWindow(h_whitelist_wnd);
		if (h_user_idle_mode_wnd)
			DestroyWindow(h_user_idle_mode_wnd);
		if (h_topology_wnd)
			DestroyWindow(h_topology_wnd);
		if (h_options_wnd)
			DestroyWindow(h_options_wnd);
		if (h_device_wnd)
			DestroyWindow(h_device_wnd);
		if (h_main_wnd)
			DestroyWindow(h_main_wnd);
		PostQuitMessage(0);
	}
	
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SetCurrentProcessExplicitAppUserModelID(L"JPersson.LGTVCompanion.18");

		SendDlgItemMessage(hWnd, IDC_COMBO, WM_SETFONT, (WPARAM)h_large_edit_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_CHECK_ENABLE, WM_SETFONT, (WPARAM)h_edit_small_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_NEWVERSION, WM_SETFONT, (WPARAM)h_edit_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_DONATE, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_SPLIT, WM_SETFONT, (WPARAM)h_edit_small_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDOK, WM_SETFONT, (WPARAM)h_edit_small_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_OPTIONS, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));

		SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);

		if (Prefs.devices_.size() > 0)
		{
			for (const auto& item : Prefs.devices_)
			{
				SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)tools::widen(item.name).c_str());
			}
			SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)0, (LPARAM)0);

			CheckDlgButton(hWnd, IDC_CHECK_ENABLE, Prefs.devices_[0].enabled ? BST_CHECKED : BST_UNCHECKED);
			EnableWindow(GetDlgItem(hWnd, IDC_COMBO), true);
			EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), true);

			SetDlgItemText(hWnd, IDC_SPLIT, L"C&onfigure");
		}
		else
		{
			EnableWindow(GetDlgItem(hWnd, IDC_COMBO), false);
			SetDlgItemText(hWnd, IDC_SPLIT, L"&Scan");
		}
		SendMessageW(GetDlgItem(hWnd, IDC_OPTIONS), BM_SETIMAGE, IMAGE_ICON, (LPARAM)h_icon_options);
		SetForegroundWindow(hWnd);
	}break;
	case APP_NEW_VERSION:
	{
		ShowWindow(GetDlgItem(hWnd, IDC_NEWVERSION), SW_SHOW);
	}break;
	case APP_MESSAGE_ADD:
	{
		h_device_wnd = CreateDialog(h_instance, MAKEINTRESOURCE(IDD_DEVICE), hWnd, (DLGPROC)WndDeviceProc);
		SetWindowText(h_device_wnd, DEVICEWINDOW_TITLE_ADD);
		SetWindowText(GetDlgItem(h_device_wnd, IDOK), L"&Add");
		CheckDlgButton(h_device_wnd, IDC_RADIO2, BST_CHECKED);
		EnableWindow(GetDlgItem(h_device_wnd, IDC_SUBNET), false);
		SetWindowText(GetDlgItem(h_device_wnd, IDC_SUBNET), tools::widen(WOL_DEFAULT_SUBNET).c_str());

		CheckDlgButton(h_device_wnd, IDC_CHECK_HDMI_INPUT_CHECKBOX, BST_UNCHECKED);

		CheckDlgButton(h_device_wnd, IDC_SET_HDMI_INPUT_CHECKBOX, BST_UNCHECKED);
		EnableWindow(GetDlgItem(h_device_wnd, IDC_SET_HDMI_DELAY), false);
		SetWindowText(GetDlgItem(h_device_wnd, IDC_SET_HDMI_DELAY), L"1");
		SendMessage(GetDlgItem(h_device_wnd, IDC_COMBO_PERSISTENCE), (UINT)CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
		SendMessage(GetDlgItem(h_device_wnd, IDC_COMBO_WOL), (UINT)CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
		SendMessage(GetDlgItem(h_device_wnd, IDC_COMBO_SSL), (UINT)CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
		SendMessage(GetDlgItem(h_device_wnd, IDC_COMBO_SOURCE), (UINT)CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
		SendMessage(GetDlgItem(h_device_wnd, IDC_COMBO_NIC), (UINT)CB_SETCURSEL, (WPARAM)0, (LPARAM)0);

		EnableWindow(GetDlgItem(h_device_wnd, IDC_RADIO1), false);
		EnableWindow(GetDlgItem(h_device_wnd, IDC_RADIO2), false);
		EnableWindow(GetDlgItem(h_device_wnd, IDC_RADIO3), false);
		EnableWindow(GetDlgItem(h_device_wnd, IDC_SUBNET), false);

		EnableWindow(GetDlgItem(h_device_wnd, IDOK), false);

		EnableWindow(hWnd, false);
		ShowWindow(h_device_wnd, SW_SHOW);
	}break;
	case APP_MESSAGE_MANAGE:
	{
		int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
		if (sel == CB_ERR)
			break;

		h_device_wnd = CreateDialog(h_instance, MAKEINTRESOURCE(IDD_DEVICE), hWnd, (DLGPROC)WndDeviceProc);
		SetWindowText(h_device_wnd, DEVICEWINDOW_TITLE_MANAGE);
		SetWindowText(GetDlgItem(h_device_wnd, IDOK), L"&Save");
		SetWindowText(GetDlgItem(h_device_wnd, IDC_DEVICENAME), tools::widen(Prefs.devices_[sel].name).c_str());
		SetWindowText(GetDlgItem(h_device_wnd, IDC_DEVICEIP), tools::widen(Prefs.devices_[sel].ip).c_str());

		SendMessage(GetDlgItem(h_device_wnd, IDC_COMBO_SOURCE), (UINT)CB_SETCURSEL, (WPARAM)Prefs.devices_[sel].sourceHdmiInput -1, (LPARAM)0);

		CheckDlgButton(h_device_wnd, IDC_CHECK_HDMI_INPUT_CHECKBOX, Prefs.devices_[sel].check_hdmi_input_when_power_off);
	
		CheckDlgButton(h_device_wnd, IDC_SET_HDMI_INPUT_CHECKBOX, Prefs.devices_[sel].set_hdmi_input_on_power_on);
		EnableWindow(GetDlgItem(h_device_wnd, IDC_SET_HDMI_DELAY), Prefs.devices_[sel].set_hdmi_input_on_power_on ? true : false);
		SetWindowText(GetDlgItem(h_device_wnd, IDC_SET_HDMI_DELAY), tools::widen(std::to_string(Prefs.devices_[sel].set_hdmi_input_on_power_on_delay)).c_str());
		SendDlgItemMessage(h_device_wnd, IDC_SET_HDMI_DELAY_SPIN, UDM_SETPOS, (WPARAM)NULL, (LPARAM)Prefs.devices_[sel].set_hdmi_input_on_power_on_delay);

		str = L"";
		for (const auto& item : Prefs.devices_[sel].mac_addresses)
		{
			str += tools::widen(item);
			if (item != Prefs.devices_[sel].mac_addresses.back())
				str += L"\r\n";
		}
		SendMessage(GetDlgItem(h_device_wnd, IDC_COMBO_PERSISTENCE), (UINT)CB_SETCURSEL, (WPARAM)Prefs.devices_[sel].persistent_connection_level, (LPARAM)0);
		SendMessage(GetDlgItem(h_device_wnd, IDC_COMBO_SSL), (UINT)CB_SETCURSEL, (WPARAM)Prefs.devices_[sel].ssl ? 0 : 1, (LPARAM)0);
		SendMessage(GetDlgItem(h_device_wnd, IDC_COMBO_WOL), (UINT)CB_SETCURSEL, (WPARAM)Prefs.devices_[sel].wake_method == WOL_TYPE_AUTO ? 0 : 1, (LPARAM)0);

		SetWindowText(GetDlgItem(h_device_wnd, IDC_DEVICEMACS), str.c_str());
		EnableWindow(GetDlgItem(h_device_wnd, IDOK), false);

		switch (Prefs.devices_[sel].wake_method)
		{
		case WOL_TYPE_NETWORKBROADCAST:
		{
			CheckDlgButton(h_device_wnd, IDC_RADIO1, BST_CHECKED);
			EnableWindow(GetDlgItem(h_device_wnd, IDC_SUBNET), false);
		}break;
		case WOL_TYPE_IP:
		{
			CheckDlgButton(h_device_wnd, IDC_RADIO2, BST_CHECKED);
			EnableWindow(GetDlgItem(h_device_wnd, IDC_SUBNET), false);
		}break;
		case WOL_TYPE_SUBNETBROADCAST:
		{
			CheckDlgButton(h_device_wnd, IDC_RADIO3, BST_CHECKED);
			EnableWindow(GetDlgItem(h_device_wnd, IDC_SUBNET), true);
		}break;
		default:
		{
			EnableWindow(GetDlgItem(h_device_wnd, IDC_RADIO1), false);
			EnableWindow(GetDlgItem(h_device_wnd, IDC_RADIO2), false);
			EnableWindow(GetDlgItem(h_device_wnd, IDC_RADIO3), false);
			EnableWindow(GetDlgItem(h_device_wnd, IDC_SUBNET), false);
		}break;
		}
		SetWindowText(GetDlgItem(h_device_wnd, IDC_SUBNET), tools::widen(Prefs.devices_[sel].subnet).c_str());


		LRESULT itemCount = SendMessage(GetDlgItem(h_device_wnd, IDC_COMBO_NIC), CB_GETCOUNT, 0, 0);
		if (itemCount != CB_ERR)
		{
			// Iterate over all items
			bool found = false;
			for (int i = 0; i < itemCount; ++i) {
				// Retrieve the stored 64-bit NET_LUID value for the current item
				uint64_t itemLuid = static_cast<uint64_t>(SendMessage(GetDlgItem(h_device_wnd, IDC_COMBO_NIC), CB_GETITEMDATA, i, 0));
				if (itemLuid == CB_ERR)
					continue; // Skip this item
				if (itemLuid == Prefs.devices_[sel].network_interface_luid) {
					found = true;
					SendMessage(GetDlgItem(h_device_wnd, IDC_COMBO_NIC), CB_SETCURSEL, i, 0);
					break;
				}
			}
			if (!found)
			{
				SendMessage(GetDlgItem(h_device_wnd, IDC_COMBO_NIC), CB_SETCURSEL, 0, 0);
			}
		}
		EnableWindow(GetDlgItem(h_device_wnd, IDOK), false);
		EnableWindow(hWnd, false);
		ShowWindow(h_device_wnd, SW_SHOW);
	}break;
	case APP_MESSAGE_SCAN:
	{
		bool RemoveCurrentDevices = (wParam == 0) ? true : false;
		bool ChangesWereMade = false;
		int DevicesAdded = 0;

		HDEVINFO DeviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES);
		SP_DEVINFO_DATA DeviceInfoData;

		memset(&DeviceInfoData, 0, sizeof(SP_DEVINFO_DATA));
		DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		int DeviceIndex = 0;

		while (SetupDiEnumDeviceInfo(DeviceInfoSet, DeviceIndex, &DeviceInfoData))
		{
			PDEVPROPKEY     pDevPropKey;
			DEVPROPTYPE PropType;
			DWORD required_size = 0;
			DeviceIndex++;

			pDevPropKey = (PDEVPROPKEY)(&DEVPKEY_Device_FriendlyName);

			SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, nullptr, 0, &required_size, 0);
			if (required_size > 2)
			{
				std::vector<BYTE> unicode_buffer(required_size, 0);
				SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, unicode_buffer.data(), required_size, nullptr, 0);
				std::wstring FriendlyName;
				FriendlyName = ((PWCHAR)unicode_buffer.data()); //NAME
				if (FriendlyName.find(L"[LG]", 0) != std::wstring::npos)
				{
					pDevPropKey = (PDEVPROPKEY)(&PKEY_PNPX_IpAddress);
					SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, nullptr, 0, &required_size, 0);
					if (required_size > 2)
					{
						std::vector<BYTE> unicode_buffer2(required_size, 0);
						SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, unicode_buffer2.data(), required_size, nullptr, 0);
						std::wstring IP;
						IP = ((PWCHAR)unicode_buffer2.data()); //IP
						pDevPropKey = (PDEVPROPKEY)(&PKEY_PNPX_PhysicalAddress);
						SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, nullptr, 0, &required_size, 0);
						if (required_size >= 6)
						{
							std::vector<BYTE> unicode_buffer3(required_size, 0);
							SetupDiGetDeviceProperty(DeviceInfoSet, &DeviceInfoData, pDevPropKey, &PropType, unicode_buffer3.data(), required_size, nullptr, 0);

							std::stringstream ss;
							std::string MAC;
							ss << std::hex << std::setfill('0');
							for (int i = 0; i < 6; i++) // TODO, check here what happens if more than one MAC in vector.
							{
								ss << std::setw(2) << static_cast<unsigned> (unicode_buffer3[i]);
								if (i < 5)
									ss << ":";
							}
							MAC = ss.str();
							transform(MAC.begin(), MAC.end(), MAC.begin(), ::toupper);
							if (RemoveCurrentDevices)
							{
								Prefs.devices_.clear();
								RemoveCurrentDevices = false;
								ChangesWereMade = true;
							}
							bool DeviceExists = false;
							for (auto& item : Prefs.devices_)
								for (auto& m : item.mac_addresses)
									if (m == MAC)
									{
										if (tools::narrow(IP) != item.ip)
										{
											item.ip = tools::narrow(IP);
											ChangesWereMade = true;
										}
										DeviceExists = true;
									}

							if (!DeviceExists)
							{
								Device temp;
								temp.name = tools::narrow(FriendlyName);
								std::string prefix = "[LG] webOS TV ";
								if (temp.name.find(prefix) == 0)
								{
									if (temp.name.size() > 18)
										temp.name.erase(0, prefix.length());
								}
								temp.ip = tools::narrow(IP);
								temp.mac_addresses.push_back(MAC);
								temp.subnet = tools::getSubnetMask(temp.ip);
								temp.wake_method = WOL_TYPE_AUTO;
								Prefs.devices_.push_back(temp);
								ChangesWereMade = true;
								DevicesAdded++;
							}
						}
					}
				}
			}
		}
		if (ChangesWereMade)
		{
			SendMessage(hWnd, (UINT)WM_INITDIALOG, (WPARAM)0, (LPARAM)0);
			EnableWindow(GetDlgItem(hWnd, IDOK), true);
		}
		if (DeviceInfoSet)
			SetupDiDestroyDeviceInfoList(DeviceInfoSet);

		std::wstringstream mess;
		mess << DevicesAdded;
		mess << L" new devices found.";
		customMsgBox(hWnd, mess.str().c_str(), L"Scan results", MB_OK | MB_ICONINFORMATION);
	}break;
	case APP_MESSAGE_REMOVE:
	{
		int sel = CB_ERR;
		if (wParam == 1) //remove all
		{
			Prefs.devices_.clear();
			CheckDlgButton(hWnd, IDC_CHECK_ENABLE, BST_UNCHECKED);
			EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), false);
			SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
			SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);
			EnableWindow(GetDlgItem(hWnd, IDC_COMBO), false);
			SetDlgItemText(hWnd, IDC_SPLIT, L"&Scan");
		}
		else
		{
			int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
			if (sel == CB_ERR)
				break;
			Prefs.devices_.erase(Prefs.devices_.begin() + sel);

			int ind = (int)SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_DELETESTRING, (WPARAM)sel, (LPARAM)0);
			if (ind > 0)
			{
				int j = sel < ind ? sel : sel - 1;
				SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)j, (LPARAM)0);
				CheckDlgButton(hWnd, IDC_CHECK_ENABLE, Prefs.devices_[j].enabled ? BST_CHECKED : BST_UNCHECKED);
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), true);
				EnableWindow(GetDlgItem(hWnd, IDC_COMBO), true);
				SetDlgItemText(hWnd, IDC_SPLIT, L"C&onfigure");
			}
			else
			{
				CheckDlgButton(hWnd, IDC_CHECK_ENABLE, BST_UNCHECKED);
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), false);
				SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);
				EnableWindow(GetDlgItem(hWnd, IDC_COMBO), false);
				SetDlgItemText(hWnd, IDC_SPLIT, L"&Scan");
			}
		}
		EnableWindow(GetDlgItem(hWnd, IDOK), true);
	}break;

	case APP_MESSAGE_TURNON:
	{
		int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
		if (sel == CB_ERR)
			break;
		std::wstring s = L"-poweron ";
		s += tools::widen(Prefs.devices_[sel].id);
		communicateWithService(s);
	}break;
	case APP_MESSAGE_TURNOFF:
	{
		int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
		if (sel == CB_ERR)
			break;
		std::wstring s = L"-poweroff ";
		s += tools::widen(Prefs.devices_[sel].id);
		communicateWithService(s);
	}break;
	case APP_MESSAGE_APPLY:
	{
		std::vector<std::string> IPs = tools::getLocalIP();
		if(IPs.size()>0)
		{
			for (auto& Dev : Prefs.devices_)
			{
				bool bFound = false;
				for (auto& IP : IPs)
				{
					std::vector temp = tools::stringsplit(IP, "/");
					std::string IP, CIRD;
					if (temp.size() > 1)
					{
						std::stringstream subnet;
						IP = temp[0];
						CIRD = temp[1];
						unsigned long mask = (0xFFFFFFFF << (32 - atoi(CIRD.c_str())) & 0xFFFFFFFF);
						subnet << (mask >> 24) << "." << ((mask >> 16) & 0xFF) << "." << ((mask >> 8) & 0xFF) << "." << (mask & 0xFF);
						if (tools::isSameSubnet(Dev.ip.c_str(), IP.c_str(), subnet.str().c_str()))
							bFound = true;
					}
					else
						bFound = true;
				}
				if (!bFound)
				{
					std::string mess = Dev.id;
					mess += " with name \"";
					mess += Dev.name;
					mess += "\" and IP ";
					mess += Dev.ip;
					mess += " is not on the same subnet as the PC. Please note that this might cause problems with waking "
						"up the TV. Please check the documentation and the configuration.\n\n Do you want to continue anyway?";
					int mb = customMsgBox(hWnd, tools::widen(mess).c_str(), L"Warning", MB_YESNO | MB_ICONEXCLAMATION);
					if (mb == IDNO)
					{
						EnableWindow(hWnd, true);
						return 0;
					}
				}
			}
		}

		Prefs.writeToDisk();

		//restart the service
		SERVICE_STATUS_PROCESS status;
		DWORD bytesNeeded;
		SC_HANDLE serviceDbHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
		if (serviceDbHandle)
		{
			SC_HANDLE serviceHandle = OpenService(serviceDbHandle, L"LGTVsvc", SERVICE_QUERY_STATUS | SERVICE_STOP | SERVICE_START);
			if (serviceHandle)
			{
				QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded);
				ControlService(serviceHandle, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&status);
				while (status.dwCurrentState != SERVICE_STOPPED)
				{
					Sleep(100);
					if (QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded))
					{
						if (status.dwCurrentState == SERVICE_STOPPED)
							break;
					}
				}

				StartService(serviceHandle, NULL, NULL);

				CloseServiceHandle(serviceHandle);
			}
			else
				customMsgBox(hWnd, L"The LGTV Companion service is not installed. Please reinstall the application", L"Error", MB_OK | MB_ICONEXCLAMATION);
			CloseServiceHandle(serviceDbHandle);
		}

		//restart the daemon
		std::wstring window_title;
		window_title = APPNAME;
		window_title += L" Daemon v";
		window_title += APP_VERSION;
		std::wstring sWinSearch = window_title;
		HWND daemon_hWnd = FindWindow(NULL, sWinSearch.c_str());
		if (daemon_hWnd)
		{
			PostMessage(daemon_hWnd, custom_daemon_restart_message, NULL, NULL);
		}
		else
		{
			TCHAR buffer[MAX_PATH] = { 0 };
			if(GetModuleFileName(NULL, buffer, MAX_PATH))
			{
				std::wstring path = buffer;
				std::wstring exe;
				std::wstring::size_type pos = path.find_last_of(L"\\/");
				path = path.substr(0, pos + 1);
				exe = path;
				exe += L"LGTVdaemon.exe";
				ShellExecute(NULL, L"open", exe.c_str(), L"-restart", path.c_str(), SW_HIDE);
			}
		}
//		p_pipe_client->init();
		EnableWindow(hWnd, true);
		EnableWindow(GetDlgItem(hWnd, IDOK), false);
	}break;
	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDC_CHECK_ENABLE:
			{
				int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
				if (sel == CB_ERR)
					break;
				Prefs.devices_[sel].enabled = IsDlgButtonChecked(hWnd, IDC_CHECK_ENABLE) == BST_CHECKED ? true : false;
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDOK:
			{
				EnableWindow(hWnd, false);
				PostMessage(hWnd, APP_MESSAGE_APPLY, (WPARAM)NULL, NULL);
			}break;
			case IDC_TEST:
			{
//				MessageBox(NULL, std::to_wstring(copydata_count).c_str(), L"Message", MB_OK);
			}break;
			case IDC_SPLIT:
			{
				if (Prefs.devices_.size() > 0)
					SendMessage(hWnd, APP_MESSAGE_MANAGE, NULL, NULL);
				else
				{
					int ms = customMsgBox(hWnd, L"Scanning will discover and add any LG devices found.\n\nThe scan is performed locally by filtering the 'Digital Media Devices' category in the device manager for LG devices.\n\nClick OK to continue!", L"Scanning", MB_OKCANCEL | MB_ICONEXCLAMATION);
					if (ms == IDCANCEL)
						break;
					SendMessage(hWnd, APP_MESSAGE_SCAN, NULL, NULL);
				}
			}break;
			case IDC_OPTIONS:
			{
				h_options_wnd = CreateDialog(h_instance, MAKEINTRESOURCE(IDD_OPTIONS), hWnd, (DLGPROC)WndOptionsProc);
				SetWindowText(h_options_wnd, L"Global settings");
				EnableWindow(hWnd, false);
				ShowWindow(h_options_wnd, SW_SHOW);
			}break;
			default:break;
			}
		}break;
		case CBN_SELCHANGE:
		{
			if (Prefs.devices_.size() > 0)
			{
				int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
				if (sel == CB_ERR)
					break;

				CheckDlgButton(hWnd, IDC_CHECK_ENABLE, Prefs.devices_[sel].enabled ? BST_CHECKED : BST_UNCHECKED);
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_ENABLE), true);
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
			//download new version
			if (wParam == IDC_NEWVERSION)
			{
				TCHAR buffer[MAX_PATH] = { 0 };
				if(GetModuleFileName(NULL, buffer, MAX_PATH))
				{
					std::wstring path = buffer;
					std::wstring::size_type pos = path.find_last_of(L"\\/");
					path = path.substr(0, pos + 1);
					std::wstring exe = path;
					exe += L"LGTVupdater.exe";
					ShellExecute(NULL, L"open", exe.c_str(), NULL, path.c_str(), SW_SHOW);
				}
			}
			//care to support your coder
			if (wParam == IDC_DONATE)
			{
				if (customMsgBox(hWnd, L"This is free software, but your support is appreciated and there "
					"is a donation page set up over at PayPal. PayPal allows you to use a credit- or debit "
					"card or your PayPal balance to make a donation, even without a PayPal account.\n\n"
					"Click 'Yes' to continue to the PayPal donation web page (a PayPal account is not "
					"required to make a donation)!"
					"\n\nAlternatively you can go to the following URL in your web browser (a PayPal "
					"account is required for this link).\n\nhttps://paypal.me/jpersson77", L"Donate via PayPal?", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
					ShellExecute(0, 0, DONATELINK, 0, 0, SW_SHOW);
			}
		}break;
		case BCN_DROPDOWN:
		{
			NMBCDROPDOWN* pDropDown = (NMBCDROPDOWN*)lParam;
			if (pDropDown->hdr.hwndFrom == GetDlgItem(hWnd, IDC_SPLIT))
			{
				POINT pt;
				MENUITEMINFO mi;
				pt.x = pDropDown->rcButton.right;
				pt.y = pDropDown->rcButton.bottom;
				ClientToScreen(pDropDown->hdr.hwndFrom, &pt);

				mi.cbSize = sizeof(MENUITEMINFO);
				mi.fMask = MIIM_STATE;
				if (Prefs.devices_.size() > 0)
					mi.fState = MFS_ENABLED;
				else
					mi.fState = MFS_DISABLED;

				SetMenuItemInfo(h_popup_menu, ID_M_MANAGE, false, &mi);
				SetMenuItemInfo(h_popup_menu, ID_M_REMOVE, false, &mi);
				SetMenuItemInfo(h_popup_menu, ID_M_REMOVEALL, false, &mi);

				if (Prefs.devices_.size() > 0 && !IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
					mi.fState = MFS_ENABLED;
				else
					mi.fState = MFS_DISABLED;

				SetMenuItemInfo(h_popup_menu, ID_M_TEST, false, &mi);
				SetMenuItemInfo(h_popup_menu, ID_M_TURNON, false, &mi);
				SetMenuItemInfo(h_popup_menu, ID_M_TURNOFF, false, &mi);

				switch (TrackPopupMenu(GetSubMenu(h_popup_menu, 0), TPM_TOPALIGN | TPM_RIGHTALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, NULL))
				{
				case ID_M_REMOVE:
				{
					if (Prefs.devices_.size() > 0)
						if (customMsgBox(hWnd, L"You are about to remove this device.\n\nDo you want to continue?", L"Remove device", MB_YESNO | MB_ICONQUESTION) == IDYES)
							SendMessage(hWnd, (UINT)APP_MESSAGE_REMOVE, (WPARAM)0, (LPARAM)lParam);
				}break;
				case ID_M_REMOVEALL:
				{
					if (Prefs.devices_.size() > 0)
						if (customMsgBox(hWnd, L"You are about to remove ALL devices.\n\nDo you want to continue?", L"Remove all devices", MB_YESNO | MB_ICONQUESTION) == IDYES)
							SendMessage(hWnd, (UINT)APP_MESSAGE_REMOVE, (WPARAM)1, (LPARAM)lParam);
				}break;

				case ID_M_ADD:
				{
					SendMessage(hWnd, (UINT)APP_MESSAGE_ADD, (WPARAM)wParam, (LPARAM)lParam);
				}break;
				case ID_M_MANAGE:
				{
					SendMessage(hWnd, (UINT)APP_MESSAGE_MANAGE, (WPARAM)wParam, (LPARAM)lParam);
				}break;
				case ID_M_SCAN:
				{
					if (Prefs.devices_.size() > 0)
					{
						int ms = customMsgBox(hWnd, L"Scanning will discover and add any LG devices found.\n\nThe scan is performed locally by filtering the 'Digital Media Devices' category in the device manager for LG devices.\n\nDo you want to replace the current devices with any discovered devices?\n\nYES = clear current devices before adding, \n\nNO = add to current list of devices.", L"Scanning", MB_YESNOCANCEL | MB_ICONEXCLAMATION);

						if (ms == IDCANCEL)
							break;

						SendMessage(hWnd, (UINT)APP_MESSAGE_SCAN, (WPARAM)ms == IDYES ? 0 : 1, (LPARAM)lParam);
					}
					else
					{
						int ms = customMsgBox(hWnd, L"Scanning will discover and add any LG devices found.\n\nThe scan is performed locally by filtering the 'Digital Media Devices' category in the device manager for LG devices.\n\nClick OK to continue!", L"Scanning", MB_OKCANCEL| MB_ICONEXCLAMATION);
						if (ms == IDCANCEL)
							break;
						SendMessage(hWnd, (UINT)APP_MESSAGE_SCAN, (WPARAM)0, (LPARAM)lParam);
					}
				}break;
				case ID_M_TEST:
				{
					if (Prefs.devices_.size() > 0)
					{
						if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
							if (customMsgBox(hWnd, L"Please apply unsaved changes before attempting to control the device", L"Information", MB_OK | MB_ICONEXCLAMATION) == IDOK)
								break;
						int ms = customMsgBox(hWnd, L"You are about to test the ability to control this device?\n\nPlease click YES to power off the device. Then wait about 5 seconds, or until you hear an internal relay of the TV clicking, and press ENTER on your keyboard to power on the device again.", L"Test device", MB_YESNO | MB_ICONQUESTION);
						switch (ms)
						{
						case IDYES:
							SendMessage(hWnd, (UINT)APP_MESSAGE_TURNOFF, (WPARAM)wParam, (LPARAM)lParam);
							customMsgBox(hWnd, L"Please press ENTER on your keyboard to power on the device again.", L"Test device", MB_OK | MB_ICONEXCLAMATION);
							SendMessage(hWnd, (UINT)APP_MESSAGE_TURNON, (WPARAM)wParam, (LPARAM)lParam);
							break;
						default:break;
						}
					}
				}break;
				case ID_M_TURNON:
				{
					if (Prefs.devices_.size() > 0)
					{
						if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
							if (customMsgBox(hWnd, L"Please apply unsaved changes before attempting to control the device", L"Information", MB_OK | MB_ICONEXCLAMATION) == IDOK)
								break;
						SendMessage(hWnd, (UINT)APP_MESSAGE_TURNON, (WPARAM)wParam, (LPARAM)lParam);
					}
				}break;
				case ID_M_TURNOFF:
				{
					if (Prefs.devices_.size() > 0)
					{
						if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
							if (customMsgBox(hWnd, L"Please apply unsaved changes before attempting to control the device", L"Information", MB_OK | MB_ICONEXCLAMATION) == IDOK)
								break;
						SendMessage(hWnd, (UINT)APP_MESSAGE_TURNOFF, (WPARAM)wParam, (LPARAM)lParam);
					}
				}break;

				default:break;
				}
			}
		}	break;

		default:break;
		}
	}break;

	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_ENABLE))
		{
			SetBkMode(hdcStatic, TRANSPARENT);
		}
		return(INT_PTR)h_backbrush;
	}break;
	case WM_COPYDATA:
	{
		
		if (!lParam)
			return true;

		//thread safe
		while (!copydata_mutex_.try_lock())
			Sleep(COPYDATA_MUTEX_WAIT);
		
		std::wstring received_command_line;
		COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)lParam;

		if (pcds->dwData == NOTIFY_NEW_COMMANDLINE)
		{
			if (pcds->cbData == 0)
			{
				SetForegroundWindow(hWnd);
				copydata_mutex_.unlock();
				return true;
			}
			received_command_line = (WCHAR*)pcds->lpData;
			if (received_command_line.size() == 0)
			{
				copydata_mutex_.unlock();
				return true;
			}
			communicateWithService(received_command_line);
			Sleep(20);
		}
		copydata_mutex_.unlock();
		return true;
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
//		DrawIcon(backbuffDC, 0, 0, hCogIcon);
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
		if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
		{
			int mess = customMsgBox(hWnd, L"You have made changes which are not yet applied.\n\nDo you want to apply the changes before exiting?", L"Unsaved configuration", MB_YESNOCANCEL | MB_ICONEXCLAMATION);
			if (mess == IDCANCEL)
				break;
			if (mess == IDYES)
			{
				EnableWindow(hWnd, false);
				SendMessage(hWnd, APP_MESSAGE_APPLY, (WPARAM)NULL, NULL);
			}
		}
		DestroyWindow(hWnd);
	}break;

	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;
	}
	default:
		return false;
	}
	return true;
}

//   Process messages for the add/manage devices window
LRESULT CALLBACK WndDeviceProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SetCurrentProcessExplicitAppUserModelID(L"JPersson.LGTVCompanion.18");
		SendDlgItemMessage(hWnd, IDC_DEVICENAME, WM_SETFONT, (WPARAM)h_edit_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_DEVICEIP, WM_SETFONT, (WPARAM)h_edit_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_SET_HDMI_DELAY, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_SET_HDMI_DELAY_SPIN, UDM_SETRANGE, (WPARAM)NULL, MAKELPARAM(30, 0));
		SendDlgItemMessage(hWnd, IDC_DEVICEMACS, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_SUBNET, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_COMBO_PERSISTENCE, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_COMBO_SSL, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_COMBO_WOL, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_COMBO_SOURCE, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_COMBO_NIC, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		std::wstring s;
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_WOL), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		s = L"Automatic";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_WOL), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		s = L"Manual";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_WOL), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SSL), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		s = L"Default";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SSL), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		s = L"Legacy";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SSL), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_PERSISTENCE), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		s = L"Off";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_PERSISTENCE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		s = L"Persistent";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_PERSISTENCE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		s = L"Persistent + Keep Alive";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_PERSISTENCE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SOURCE), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		s = L"HDMI 1";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SOURCE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		s = L"HDMI 2";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SOURCE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		s = L"HDMI 3";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SOURCE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		s = L"HDMI 4";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_SOURCE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_NIC), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		s = L"Automatic";
		LRESULT t = SendMessage(GetDlgItem(hWnd, IDC_COMBO_NIC), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)s.c_str());
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_NIC), CB_SETITEMDATA, static_cast<WPARAM>(t), static_cast<LPARAM>(0));

		ULONG buffer_len = 0;
		DWORD return_value = 0;
		PIP_ADAPTER_ADDRESSES p_addresses = nullptr;
		std::vector<BYTE> buffer;
		// Get the required buffer size
		return_value = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, NULL, p_addresses, &buffer_len);
		if (return_value == ERROR_BUFFER_OVERFLOW) 
		{
			buffer.resize(buffer_len);
			p_addresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
			if (!p_addresses) 
				break;
			// Retrieve the adapter addresses
			return_value = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, NULL, p_addresses, &buffer_len);
			if (return_value == ERROR_SUCCESS)
			{
				for (PIP_ADAPTER_ADDRESSES p_address = p_addresses; p_address != NULL; p_address = p_address->Next)
				{
					// Print adapter name
	//					std::wcout << L"Adapter: " << p_address->FriendlyName << std::endl;
					if (p_address->OperStatus != IfOperStatusUp)
						continue;

					bool has_ipv4_address = false;
					bool localhost = false;
					char ip_string_buffer[INET6_ADDRSTRLEN];
					for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = p_address->FirstUnicastAddress; pUnicast != NULL; pUnicast = pUnicast->Next)
					{
						LPSOCKADDR sockaddr = pUnicast->Address.lpSockaddr;
						DWORD ip_string_buffer_len = sizeof(ip_string_buffer);

						// Convert the address to a readable string
						if (sockaddr->sa_family == AF_INET) // IPv4
						{
							has_ipv4_address = true;
							sockaddr_in* ipv4 = (sockaddr_in*)sockaddr;
							InetNtopA(AF_INET, &(ipv4->sin_addr), ip_string_buffer, ip_string_buffer_len);
							if (strcmp(ip_string_buffer, "127.0.0.1") == 0)
								localhost = true;
						}
					}
					if (!has_ipv4_address || localhost)
						continue; // Skip adapters without IPv4 addresses

					std::wstring text = p_address->FriendlyName;
					uint64_t nl = static_cast<uint64_t>(p_address->Luid.Value);
					LRESULT index = SendMessage(GetDlgItem(hWnd, IDC_COMBO_NIC), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)text.c_str());
					SendMessage(GetDlgItem(hWnd, IDC_COMBO_NIC), CB_SETITEMDATA, static_cast<WPARAM>(index), static_cast<LPARAM>(nl));
				}
			}
		}
	}break;
	case WM_NOTIFY:
	{
		switch (((NMHDR*)lParam)->code)
		{
		case NM_CLICK:
		{
			// explain the device settings
			if (wParam == IDC_SYSLINK8)
			{
				customMsgBox(hWnd,
					L"Name your devices so you can easily distinguish them.\n\n"
					"The IP-address and MAC-address are the device's unique network identifiers. If the automatic scan did not determine the "
					"correct values please go into the network settings "
					"of your  WebOS-device to check on the correct values.\n\n"
					"Please note that both MAC address and IP are mandatory to set.",
					L"Device information", MB_OK | MB_ICONINFORMATION);
			}
			// explain the network settings
			else if (wParam == IDC_SYSLINK4)
			{
				customMsgBox(hWnd,
					L"Devices are powered on by means of wake-on lan (WOL) magic packets. The \"Automatic\" mode should work well on most systems, but depending on your network environment, device firmware and operating system you may need to "
					"go to \"Manual\" mode and adjust the settings.\n\n"
					"The manual method \"Send to device IP-address\" method (no 2) sends unicast packets directly to the device IP, while the other two send to the network broadcast address or subnet broadcast address respectively.\n\n"
					"Between the two broadcast options, the subnet approach is the most likely to work. The global network broadcast approach is prone to issues when multiple network interfaces are present (VPN, etc.), because the packet might be sent using the wrong interface.\n\n"
					"The current subnet mask of the subnet(s) of your PC can be found by using the \"IPCONFIG /all\" command in the command prompt if it is not correctly detected automatically.\r\n\r\n"
					"If needed, it is also possible to manually specify the Network Adapter to use for sending Wake-On-Lan packets",
					L"Network options", MB_OK | MB_ICONINFORMATION);
			}
			// explain the source input settings
			else if (wParam == IDC_SYSLINK9)
			{
				customMsgBox(hWnd,
					L"Please select the source input of the PC, i.e. the HDMI-input that the PC is "
					"connected to. \n\nThe option to prevent powering off the device during non-PC use will ensure that the device is not accidentally powered off or that the screen is turned off "
					"while you are using the device for watching other things, i.e. when you are busy watching Netflix or gaming on your console.\n\n"
					"The setting to switch to the HDMI input the PC is connected to when powering on can be used with a configurable delay to "
					"ensure that the HDMI input of the PC is switched to timely.",
					L"Information", MB_OK | MB_ICONINFORMATION);
			}
			// explain the connection settings
			else if (wParam == IDC_SYSLINK10)
			{
				customMsgBox(hWnd,
					L"The Firmware option should be set to \"Default\" for all newer devices which expect a secure connection using SSL/TLS. The \"Legacy\" option ensures compatibility with "
					"older devices or devices with a firmware version from 2022 or older.\n\nThe Persistence option determines whether the connection will remain active/connected from the device is "
					"powered on until it is powered off, for automatically managed devices. Otherwise the connection is disconnected immediately after each work. Persistent connections will generally offer improved response times over non-persistent connections"
					", but if your network is intermittently unavailable, for example due to using a laptop with disconnected modern standby or due "
					"to general network stability it is recommended to disable persistent connections. "
					"The Keep Alive option adds recurring ping-pong packets to the persistent connections which may help the app with determining network and device availability.\n\n"
					"The default option \"Off\" should work well for most systems, but it is recommended to set the option according to usage and what fits your system.",
					L"Information", MB_OK | MB_ICONINFORMATION);
			}
		}break;
		}
	}break;
	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDC_CHECK_HDMI_INPUT_CHECKBOX:
			{
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDC_SET_HDMI_INPUT_CHECKBOX:
			{
				EnableWindow(GetDlgItem(hWnd, IDC_SET_HDMI_DELAY), IsDlgButtonChecked(hWnd, IDC_SET_HDMI_INPUT_CHECKBOX) == BST_CHECKED);
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDC_RADIO1:
			case IDC_RADIO2:
			{
				EnableWindow(GetDlgItem(hWnd, IDC_SUBNET), false);
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDC_RADIO3:
			{
				EnableWindow(GetDlgItem(hWnd, IDC_SUBNET), true);
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDOK:
			{
				HWND hParentWnd = GetParent(hWnd);
				if (hParentWnd)
				{
					std::vector<std::string> maclines;
					std::wstring edittext = tools::getWndText(GetDlgItem(hWnd, IDC_DEVICEMACS));

					//verify the user supplied information
					if (edittext != L"")
					{
						transform(edittext.begin(), edittext.end(), edittext.begin(), ::toupper);

						//remove unecessary characters
						char CharsToRemove[] = ":- ,;.\n";
						for (int i = 0; i < strlen(CharsToRemove); ++i)
							edittext.erase(remove(edittext.begin(), edittext.end(), CharsToRemove[i]), edittext.end());

						//check on HEX
						if (edittext.find_first_not_of(L"0123456789ABCDEF\r") != std::string::npos)
						{
							customMsgBox(hWnd, L"One or several MAC addresses contain illegal caharcters.\n\nPlease correct before continuing.", L"Error", MB_OK | MB_ICONERROR);
							return 0;
						}
						//verify length of MACs
						maclines = tools::stringsplit(tools::narrow(edittext), "\r");
						for (auto& mac : maclines)
						{
							if (mac.length() != 12)
							{
								customMsgBox(hWnd, L"One or several MAC addresses is incorrect.\n\nPlease correct before continuing.", L"Error", MB_OK | MB_ICONERROR);
								maclines.clear();
								return 0;
							}
							else
								for (int ind = 4; ind >= 0; ind--)
									mac.insert(ind * 2 + 2, ":");
						}
						//verify IP
						if (tools::narrow(tools::getWndText(GetDlgItem(hWnd, IDC_DEVICEIP))) == "0.0.0.0")
						{
							customMsgBox(hWnd, L"The IP-address is invalid.\n\nPlease correct before continuing.", L"Error", MB_OK | MB_ICONERROR);
							return 0;
						}
					}

					if (maclines.size() > 0 && tools::getWndText(GetDlgItem(hWnd, IDC_DEVICENAME)) != L"" && tools::getWndText(GetDlgItem(hWnd, IDC_DEVICEIP)) != L"")
					{
						if (tools::getWndText(hWnd) == DEVICEWINDOW_TITLE_MANAGE) //configuring existing device
						{
							int sel = (int)(SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
							if (sel == CB_ERR)
								break;

							Prefs.devices_[sel].mac_addresses = maclines;
							Prefs.devices_[sel].name = tools::narrow(tools::getWndText(GetDlgItem(hWnd, IDC_DEVICENAME)));
							Prefs.devices_[sel].ip = tools::narrow(tools::getWndText(GetDlgItem(hWnd, IDC_DEVICEIP)));

							int source = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_SOURCE), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
							if (source != CB_ERR)
								Prefs.devices_[sel].sourceHdmiInput = source + 1;
							Prefs.devices_[sel].check_hdmi_input_when_power_off = IsDlgButtonChecked(hWnd, IDC_CHECK_HDMI_INPUT_CHECKBOX) == BST_CHECKED;

							Prefs.devices_[sel].set_hdmi_input_on_power_on = IsDlgButtonChecked(hWnd, IDC_SET_HDMI_INPUT_CHECKBOX) == BST_CHECKED;
							Prefs.devices_[sel].set_hdmi_input_on_power_on_delay = atoi(tools::narrow(tools::getWndText(GetDlgItem(hWnd, IDC_SET_HDMI_DELAY))).c_str());

							int wol_selection = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_WOL), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));

							if (wol_selection == 0)
								Prefs.devices_[sel].wake_method = WOL_TYPE_AUTO;
							else if (IsDlgButtonChecked(hWnd, IDC_RADIO3))
								Prefs.devices_[sel].wake_method = WOL_TYPE_SUBNETBROADCAST;
							else if (IsDlgButtonChecked(hWnd, IDC_RADIO2))
								Prefs.devices_[sel].wake_method = WOL_TYPE_IP;
							else
								Prefs.devices_[sel].wake_method = WOL_TYPE_NETWORKBROADCAST;
							
							if (wol_selection == 0)
								Prefs.devices_[sel].subnet = tools::getSubnetMask(Prefs.devices_[sel].ip);
							else
								Prefs.devices_[sel].subnet = tools::narrow(tools::getWndText(GetDlgItem(hWnd, IDC_SUBNET)));

							int SSL_selection = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_SSL), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
							if (SSL_selection != CB_ERR)
								Prefs.devices_[sel].ssl = SSL_selection == 0 ? true : false;
							int luid_selection = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_NIC), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
							if (luid_selection != CB_ERR)
								Prefs.devices_[sel].network_interface_luid = (uint64_t)SendMessage(GetDlgItem(hWnd, IDC_COMBO_NIC), (UINT)CB_GETITEMDATA, (WPARAM)luid_selection, (LPARAM)0);
							int persistence_selection = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_PERSISTENCE), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
							if (persistence_selection != CB_ERR)
								Prefs.devices_[sel].persistent_connection_level = persistence_selection;

							int ind = (int)SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_DELETESTRING, (WPARAM)sel, (LPARAM)0);
							SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_INSERTSTRING, (WPARAM)sel, (LPARAM)tools::widen(Prefs.devices_[sel].name).c_str());
							SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)sel, (LPARAM)0);
							EnableWindow(GetDlgItem(hParentWnd, IDC_COMBO), true);
							EnableWindow(GetDlgItem(hParentWnd, IDOK), true);
						}
						else //adding a new device
						{
							Device sess;
							std::stringstream strs;
							strs << "Device";
							strs << Prefs.devices_.size() + 1;
							sess.id = strs.str();
							sess.mac_addresses = maclines;
							sess.name = tools::narrow(tools::getWndText(GetDlgItem(hWnd, IDC_DEVICENAME)));
							sess.ip = tools::narrow(tools::getWndText(GetDlgItem(hWnd, IDC_DEVICEIP)));

							int wol_selection = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_WOL), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
							if (wol_selection == 0)
								sess.wake_method = WOL_TYPE_AUTO;
							else if (IsDlgButtonChecked(hWnd, IDC_RADIO3))
								sess.wake_method = WOL_TYPE_SUBNETBROADCAST;
							else if (IsDlgButtonChecked(hWnd, IDC_RADIO2))
								sess.wake_method = WOL_TYPE_IP;
							else
								sess.wake_method = WOL_TYPE_NETWORKBROADCAST;

							if (wol_selection == 0)
								sess.subnet = tools::getSubnetMask(sess.ip);
							else
								sess.subnet = tools::narrow(tools::getWndText(GetDlgItem(hWnd, IDC_SUBNET)));

							int source = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_SOURCE), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
							if (source != CB_ERR)
								sess.sourceHdmiInput = source + 1;

							sess.check_hdmi_input_when_power_off = IsDlgButtonChecked(hWnd, IDC_CHECK_HDMI_INPUT_CHECKBOX) == BST_CHECKED;
							sess.set_hdmi_input_on_power_on = IsDlgButtonChecked(hWnd, IDC_SET_HDMI_INPUT_CHECKBOX) == BST_CHECKED;
							sess.set_hdmi_input_on_power_on_delay = atoi(tools::narrow(tools::getWndText(GetDlgItem(hWnd, IDC_SET_HDMI_DELAY))).c_str());
							
							int luid_selection = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_NIC), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
							if (luid_selection != CB_ERR)
								sess.network_interface_luid = (uint64_t)SendMessage(GetDlgItem(hWnd, IDC_COMBO_NIC), (UINT)CB_GETITEMDATA, (WPARAM)luid_selection, (LPARAM)0);

							int SSL_selection = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_SSL), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
							if (SSL_selection != CB_ERR)
								sess.ssl = SSL_selection == 0 ? true : false;
							int persistence_selection = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_PERSISTENCE), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
							if (persistence_selection != CB_ERR)
								sess.persistent_connection_level = persistence_selection;
							Prefs.devices_.push_back(sess);

							int index = (int)SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)tools::widen(sess.name).c_str());
							if (index != CB_ERR)
							{
								SendMessage(GetDlgItem(hParentWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)index, (LPARAM)0);
								CheckDlgButton(hParentWnd, IDC_CHECK_ENABLE, BST_CHECKED);
								EnableWindow(GetDlgItem(hParentWnd, IDC_COMBO), true);
								EnableWindow(GetDlgItem(hParentWnd, IDC_CHECK_ENABLE), true);

								SetDlgItemText(hParentWnd, IDC_SPLIT, L"C&onfigure");
								EnableWindow(GetDlgItem(hParentWnd, IDOK), true);
							}
							else {
								customMsgBox(hWnd, L"Failed to add a new device.\n\nUnknown error.", L"Error", MB_OK | MB_ICONERROR);
							}
						}

						EndDialog(hWnd, 0);

						EnableWindow(GetParent(hWnd), true);
					}
					else
					{
						customMsgBox(hWnd, L"The configuration is incorrect or missing information.\n\nPlease correct before continuing.", L"Error", MB_OK | MB_ICONERROR);
						return 0;
					}
				}
			}break;

			case IDCANCEL:
			{
				if (IsWindowEnabled(GetDlgItem(hWnd, IDOK)))
				{
					if (customMsgBox(hWnd, L"You have made changes to the configuration.\n\nDo you want to discard the changes?", L"Error", MB_YESNOCANCEL | MB_ICONQUESTION) == IDYES)
					{
						EndDialog(hWnd, 0);
						EnableWindow(GetParent(hWnd), true);
					}
				}
				else
				{
					EndDialog(hWnd, 0);
					EnableWindow(GetParent(hWnd), true);
				}
			}break;
			default:break;
			}
		}break;
		case EN_CHANGE:
		{
			switch (LOWORD(wParam))
			{
			case IDC_DEVICENAME:
			case IDC_DEVICEIP:
			case IDC_SET_HDMI_DELAY:
			case IDC_DEVICEMACS:
			case IDC_SUBNET:

			{
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			default:break;
			}
		}break;
		case CBN_SELCHANGE:
		{
			switch (LOWORD(wParam))
			{
			case IDC_COMBO_WOL:
			{
				int selection = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_WOL), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
				if (selection == 0)
				{
					EnableWindow(GetDlgItem(hWnd, IDC_RADIO1), false );
					EnableWindow(GetDlgItem(hWnd, IDC_RADIO2), false );
					EnableWindow(GetDlgItem(hWnd, IDC_RADIO3), false );
					EnableWindow(GetDlgItem(hWnd, IDC_SUBNET), false);
					CheckDlgButton(hWnd, IDC_RADIO1, BST_UNCHECKED);
					CheckDlgButton(hWnd, IDC_RADIO2, BST_UNCHECKED);
					CheckDlgButton(hWnd, IDC_RADIO3, BST_UNCHECKED);
				}
				else
				{
					EnableWindow(GetDlgItem(hWnd, IDC_RADIO1), true);
					EnableWindow(GetDlgItem(hWnd, IDC_RADIO2), true);
					EnableWindow(GetDlgItem(hWnd, IDC_RADIO3), true);
					EnableWindow(GetDlgItem(hWnd, IDC_SUBNET), false);
					CheckDlgButton(hWnd, IDC_RADIO1, BST_UNCHECKED);
					CheckDlgButton(hWnd, IDC_RADIO2, BST_CHECKED);
					CheckDlgButton(hWnd, IDC_RADIO3, BST_UNCHECKED);
				}
			}break;
			default:break;
			}
			EnableWindow(GetDlgItem(hWnd, IDOK), true);
		}break;
		default:break;
		}
	}break;

	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_HDMI_INPUT_CHECKBOX)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_SET_HDMI_INPUT_CHECKBOX)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_RADIO1)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_RADIO2)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_RADIO3))
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
		EndDialog(hWnd, 0);
	}break;
	case WM_DESTROY:
	{
		h_device_wnd = NULL;
	}break;
	default:
		return false;
	}
	return true;
}

//   Process messages for the options window
LRESULT CALLBACK WndOptionsProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SetCurrentProcessExplicitAppUserModelID(L"JPersson.LGTVCompanion.18");
		LVCOLUMN lvC;
		LVITEM lvi;
		DWORD status = ERROR_SUCCESS;
		EVT_HANDLE hResults = NULL;
		std::wstring path = L"System";
		std::wstring query = L"Event/System[EventID=1074]";
		std::vector<std::wstring> str;

		SendDlgItemMessage(hWnd, IDC_STATIC_C, WM_SETFONT, (WPARAM)h_edit_small_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_COMBO_MODE, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_COMBO_LOG, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_COMBO_UPDATE, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));

		SendDlgItemMessage(hWnd, IDC_TIMEOUT, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_LIST, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_COMBO_TIMING, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		std::wstring ss;
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_TIMING), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		ss = L"Default";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_TIMING), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ss.c_str());
		ss = L"Early";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_TIMING), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ss.c_str());
		ss = L"Delayed";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_TIMING), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ss.c_str());
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_TIMING), (UINT)CB_SETCURSEL, (WPARAM)Prefs.shutdown_timing_, (LPARAM)0);
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_UPDATE), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		ss = L"Off";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_UPDATE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ss.c_str());
		ss = L"Notify Only";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_UPDATE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ss.c_str());
		ss = L"Silent Install";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_UPDATE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ss.c_str());
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_UPDATE), (UINT)CB_SETCURSEL, (WPARAM)Prefs.updater_mode_, (LPARAM)0);

		SendMessage(GetDlgItem(hWnd, IDC_COMBO_LOG), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		ss = L"Off";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_LOG), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ss.c_str());
		ss = L"Info";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_LOG), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ss.c_str());
		ss = L"Warning";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_LOG), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ss.c_str());
		ss = L"Error";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_LOG), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ss.c_str());
		ss = L"Debug";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_LOG), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ss.c_str());
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_LOG), (UINT)CB_SETCURSEL, (WPARAM)Prefs.log_level_, (LPARAM)0);

		SendDlgItemMessage(hWnd, IDC_SPIN, UDM_SETRANGE, (WPARAM)NULL, MAKELPARAM(100, 5));
		SendDlgItemMessage(hWnd, IDC_SPIN, UDM_SETPOS, (WPARAM)NULL, (LPARAM)Prefs.power_on_timeout_);
	
		if (Prefs.event_log_restart_strings_custom_.size() > 0)
			for (auto& item : Prefs.event_log_restart_strings_custom_)
				str.push_back(tools::widen(item));
		if (Prefs.event_log_shutdown_strings_custom_.size() > 0)
			for (auto& item : Prefs.event_log_shutdown_strings_custom_)
				str.push_back(tools::widen(item));

		hResults = EvtQuery(NULL, path.c_str(), query.c_str(), EvtQueryChannelPath | EvtQueryReverseDirection);
		if (hResults)
		{
			EVT_HANDLE hEv[100];
			DWORD dwReturned = 0;

			if (EvtNext(hResults, 100, hEv, INFINITE, 0, &dwReturned))
			{
				for (DWORD i = 0; i < dwReturned; i++)
				{
					DWORD buffer_len = 0;
					DWORD buffer_used = 0;
					DWORD property_count = 0;
					LPWSTR pRenderedContent = NULL;
					std::vector<BYTE> buffer;
					std::wstring xml;

					if (!EvtRender(NULL, hEv[i], EvtRenderEventXml, buffer_len, pRenderedContent, &buffer_used, &property_count))
					{
						if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
						{
							buffer_len = buffer_used;
							buffer.resize(buffer_len);
							pRenderedContent = reinterpret_cast<LPWSTR>(buffer.data());
							if (pRenderedContent)
							{
								EvtRender(NULL, hEv[i], EvtRenderEventXml, buffer_len, pRenderedContent, &buffer_used, &property_count);
								xml = pRenderedContent;
							}
						}
					}

					if (xml != L"")
					{
						std::wstring strfind = L"<Data Name='param5'>";
						size_t f = xml.find(strfind);
						if (f != std::wstring::npos)
						{
							size_t e = xml.find(L"<", f + 1);
							if (e != std::wstring::npos)
							{
								std::wstring sub = xml.substr(f + strfind.length(), e - (f + strfind.length()));
								if (std::find(Prefs.event_log_shutdown_strings_.begin(), Prefs.event_log_shutdown_strings_.end(), tools::narrow(sub)) == Prefs.event_log_shutdown_strings_.end())
									if (std::find(Prefs.event_log_restart_strings_.begin(), Prefs.event_log_restart_strings_.end(), tools::narrow(sub)) == Prefs.event_log_restart_strings_.end())
										if (std::find(str.begin(), str.end(), sub) == str.end())
											str.push_back(sub);
							}
						}
					}
					EvtClose(hEv[i]);
					hEv[i] = NULL;
				}
			}
			EvtClose(hResults);
		}
		lvC.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		lvC.iSubItem = 0; lvC.cx = 140;	lvC.fmt = LVCFMT_LEFT;
		lvC.pszText = (LPWSTR)L"Eventlog 1074";
		ListView_InsertColumn(GetDlgItem(hWnd, IDC_LIST), 0, &lvC);
		if(str.size() > 0)
			ListView_SetExtendedListViewStyle(GetDlgItem(hWnd, IDC_LIST), LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);
		else
			ListView_SetExtendedListViewStyle(GetDlgItem(hWnd, IDC_LIST), LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
		memset(&lvi, 0, sizeof(LVITEM));
		lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
		lvi.state = 0;
		lvi.stateMask = 0;
		lvi.iItem = 1;
		lvi.iSubItem = 0;
		lvi.lParam = (LPARAM)0;
		int i = 0;

		if(str.size() > 0)
		{
			for (auto& item : str)
			{
				lvi.iItem = i;
				int row = ListView_InsertItem(GetDlgItem(hWnd, IDC_LIST), &lvi);
				ListView_SetItemText(GetDlgItem(hWnd, IDC_LIST), row, 0, (LPWSTR)item.c_str());
				if (std::find(Prefs.event_log_restart_strings_custom_.begin(), Prefs.event_log_restart_strings_custom_.end(), tools::narrow(item)) != Prefs.event_log_restart_strings_custom_.end())
					ListView_SetCheckState(GetDlgItem(hWnd, IDC_LIST), row, true);
				i++;
			}
		}
		else
		{
			lvi.iItem = 0;
			std::wstring s = L"Automatic";
			int row = ListView_InsertItem(GetDlgItem(hWnd, IDC_LIST), &lvi);
			ListView_SetItemText(GetDlgItem(hWnd, IDC_LIST), row, 0, (LPWSTR)s.c_str());
			EnableWindow(GetDlgItem(hWnd, IDC_LIST), false);
		}
		CheckDlgButton(hWnd, IDC_CHECK_BLANK, Prefs.user_idle_mode_ ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_REMOTE, Prefs.remote_streaming_host_support_ ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_TOPOLOGY, Prefs.topology_support_ ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_TOPOLOGY_LOGON, Prefs.topology_keep_on_boot_ ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_API, Prefs.external_api_support_ ? BST_CHECKED : BST_UNCHECKED);

		std::wstring ls;
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_MODE), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		ls = L"Display off";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_MODE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ls.c_str());
		ls = L"Display blanked";
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_MODE), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)ls.c_str());
		SendMessage(GetDlgItem(hWnd, IDC_COMBO_MODE), (UINT)CB_SETCURSEL, (WPARAM)Prefs.remote_streaming_host_prefer_power_off_ ? 0 : 1, (LPARAM)0);

		EnableWindow(GetDlgItem(hWnd, IDC_COMBO_MODE), Prefs.remote_streaming_host_support_ ? true :  false);
		EnableWindow(GetDlgItem(hWnd, IDC_CHECK_TOPOLOGY_LOGON), Prefs.topology_support_ ? true : false);
		EnableWindow(GetDlgItem(hWnd, IDOK), false);
	}break;
	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDC_CHECK_BLANK:
			{
				if (Prefs.version_loaded_ < 2 && !reset_api_keys)
				{
					int mess = customMsgBox(hWnd, L"Enabling this option will enforce re-pairing of all your devices.\n\n Do you want to enable this option?", L"Device pairing", MB_YESNO | MB_ICONQUESTION);
					if (mess == IDNO)
					{
						CheckDlgButton(hWnd, IDC_CHECK_BLANK, BST_UNCHECKED);
					}
					if (mess == IDYES)
					{
						CheckDlgButton(hWnd, IDC_CHECK_BLANK, BST_CHECKED);
						Prefs.resetSessionKeys(true);
						reset_api_keys = true;
						EnableWindow(GetDlgItem(hWnd, IDOK), true);
					}
				}
				else
				{
					EnableWindow(GetDlgItem(hWnd, IDOK), true);
				}
			}break;
			case IDC_CHECK_TOPOLOGY:
			{
				if (Prefs.version_loaded_ < 2 && !reset_api_keys)
				{
					int mess = customMsgBox(hWnd, L"Enabling this option will enforce re-pairing of all your devices.\n\n Do you want to enable this option?", L"Device pairing", MB_YESNO | MB_ICONQUESTION);
					if (mess == IDNO)
					{
						CheckDlgButton(hWnd, IDC_CHECK_TOPOLOGY, BST_UNCHECKED);
					}
					if (mess == IDYES)
					{
						CheckDlgButton(hWnd, IDC_CHECK_TOPOLOGY, BST_CHECKED);
						Prefs.resetSessionKeys(true);
						reset_api_keys = true;
						EnableWindow(GetDlgItem(hWnd, IDOK), true);
					}
				}
				else
				{
					bool found = false;
					if (Prefs.devices_.size() == 0)
					{
						customMsgBox(hWnd, L"Please configure all devices before enabling and configuring this option", L"Error", MB_ICONEXCLAMATION | MB_OK);
						CheckDlgButton(hWnd, IDC_CHECK_TOPOLOGY, BST_UNCHECKED);
					}
					else
					{
						for (auto& k : Prefs.devices_)
						{
							if (k.uniqueDeviceKey != "")
								found = true;
						}

						if (found)
						{
							EnableWindow(GetDlgItem(hWnd, IDOK), true);
						}
						else
						{
							h_topology_wnd = CreateDialog(h_instance, MAKEINTRESOURCE(IDD_CONFIGURE_TOPOLOGY), hWnd, (DLGPROC)WndTopologyProc);
							EnableWindow(hWnd, false);
							ShowWindow(h_topology_wnd, SW_SHOW);
						}
					}
				}
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_TOPOLOGY_LOGON), IsDlgButtonChecked(hWnd, IDC_CHECK_TOPOLOGY));
			}break;

			case IDC_AUTOUPDATE:
			case IDC_CHECK_REMOTE:
			case IDC_CHECK_API:
			case IDC_CHECK_TOPOLOGY_LOGON:

			{
				EnableWindow(GetDlgItem(hWnd, IDC_COMBO_MODE), IsDlgButtonChecked(hWnd, IDC_CHECK_REMOTE));
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;

			case IDOK:
			{
				HWND hParentWnd = GetParent(hWnd);
				if (hParentWnd)
				{
					Prefs.power_on_timeout_ = atoi(tools::narrow(tools::getWndText(GetDlgItem(hWnd, IDC_TIMEOUT))).c_str());
					int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_LOG), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
					Prefs.log_level_ = sel;

					int temp_updater_mode = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_UPDATE), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
					if (temp_updater_mode != PREFS_UPDATER_OFF && Prefs.updater_mode_ == PREFS_UPDATER_OFF)
					{
						std::thread thread_obj(threadVersionCheck, h_main_wnd);
						thread_obj.detach();
					}
					Prefs.updater_mode_ = temp_updater_mode;
					Prefs.user_idle_mode_ = IsDlgButtonChecked(hWnd, IDC_CHECK_BLANK) == BST_CHECKED;
					Prefs.remote_streaming_host_support_ = IsDlgButtonChecked(hWnd, IDC_CHECK_REMOTE) == BST_CHECKED;
					Prefs.topology_support_ = IsDlgButtonChecked(hWnd, IDC_CHECK_TOPOLOGY) == BST_CHECKED;
					Prefs.topology_keep_on_boot_ = IsDlgButtonChecked(hWnd, IDC_CHECK_TOPOLOGY_LOGON) == BST_CHECKED;
					Prefs.external_api_support_ = IsDlgButtonChecked(hWnd, IDC_CHECK_API) == BST_CHECKED;

					int selection_mode = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_MODE), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
					if (selection_mode == 1)
						Prefs.remote_streaming_host_prefer_power_off_ = false;
					else
						Prefs.remote_streaming_host_prefer_power_off_ = true;

					int selection_timing = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO_TIMING), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
					if (selection_timing == 2)
						Prefs.shutdown_timing_ = PREFS_SHUTDOWN_TIMING_DELAYED; 
					else if (selection_timing == 1)
						Prefs.shutdown_timing_ = PREFS_SHUTDOWN_TIMING_EARLY;
					else
						Prefs.shutdown_timing_ = PREFS_SHUTDOWN_TIMING_DEFAULT;

					int count = ListView_GetItemCount(GetDlgItem(hWnd, IDC_LIST));
					Prefs.event_log_restart_strings_custom_.clear();
					Prefs.event_log_shutdown_strings_custom_.clear();
					for (int i = 0; i < count; i++)
					{
						std::vector<wchar_t> bufText(256);
						std::wstring st;
						ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST), i, 0, &bufText[0], (int)bufText.size());
						st = &bufText[0];
						if(st != L"Automatic")
						{
							if (ListView_GetCheckState(GetDlgItem(hWnd, IDC_LIST), i))
							{
								Prefs.event_log_restart_strings_custom_.push_back(tools::narrow(st));
							}
							else
							{
								Prefs.event_log_shutdown_strings_custom_.push_back(tools::narrow(st));
							}
						}
					}
					EndDialog(hWnd, 0);
					EnableWindow(GetParent(hWnd), true);
					EnableWindow(GetDlgItem(GetParent(hWnd), IDOK), true);
				}
			}break;

			case IDCANCEL:
			{
				reset_api_keys = false;
				Prefs.resetSessionKeys(false);

				EndDialog(hWnd, 0);
				EnableWindow(GetParent(hWnd), true);
			}break;
			default:break;
			}
		}break;
		case EN_CHANGE:
		{
			switch (LOWORD(wParam))
			{
			case IDC_TIMEOUT:
			{
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			default:break;
			}
		}break;
		case CBN_SELCHANGE:
		{
			EnableWindow(GetDlgItem(hWnd, IDOK), true);
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
			//show log
			if (wParam == IDC_SYSLINK)
			{
				std::wstring str = tools::widen(Prefs.data_path_);
				str += LOG_FILE;
				ShellExecute(NULL, L"open", str.c_str(), NULL, tools::widen(Prefs.data_path_).c_str(), SW_SHOW);
			}
			//clear log
			else if (wParam == IDC_SYSLINK2)
			{
				std::wstring str = tools::widen(Prefs.data_path_);
				str += LOG_FILE;
				std::wstring mess = L"Do you want to clear the log?\n\n";
				mess += str;
				if (customMsgBox(hWnd, mess.c_str(), L"Clear log?", MB_YESNO | MB_ICONQUESTION) == IDYES)
				{
					communicateWithService(L"-clearlog");
				}
			}
			// explain the restart words
			else if (wParam == IDC_SYSLINK3)
			{
				bool bAuto = true;
				int count = ListView_GetItemCount(GetDlgItem(hWnd, IDC_LIST));
				if (count > 0)
				{
					std::vector<wchar_t> bufText(256);
					std::wstring st;
					ListView_GetItemText(GetDlgItem(hWnd, IDC_LIST), 0, 0, &bufText[0], (int)bufText.size());
					st = &bufText[0];
					if (st != L"Automatic")
						bAuto = false;
				}
				if(bAuto)
				{
					customMsgBox(hWnd, L"It is not necessary to manually configure localised restart strings on this system, as the application can automatically determine whether "
						"a reboot or shutdown has been initiated on this system. Consequently the option to manually configure \"localised restart strings\" have been disabled."
						"\n\nThe timing option determine the timing when managing system shutdown or restart. Depending on specifics of your system you may need to change this option.\n\n"
						"When set to \"Default\" the app will trigger the shutdown/restart routine "
						"as late as possible during system shutdown which for most systems will work fine. The \"Early\" method will "
						"trigger the shutdown/restart routine a bit earlier, which will consequently leave more time during shutdown for communicating with the devices, but with an "
						"increased risk of not properly detecting restarts. On the other hand, if the system has issues with detecting restarts properly then please consider changing to "
						"\"Delayed\" which will add a slight delay to the shutdown, thus allowing more time for the system to properly communicate with the app. ",

						L"Shutdown options", MB_OK | MB_ICONINFORMATION);
				}
				else
					if (customMsgBox(hWnd, L"This application depend on localised events in the windows event log to determine whether a reboot or shutdown was initiated by the user."
						"\n\nIt seems the OS on this system is localised in a language not yet managed by the application and the user must therefore assist with manually indicating which "
						"word or phrase that refers to the system restarting.\n\nPlease put a checkmark for every word or phrase that refers to 'restart' in the \"localised restart strings\" list and "
						"make sure that the other checkboxes in the list are unchecked. You can contribute to the automatic detection of this language in a future release - please read below!"
						"\n\nThe timing option determine the timing when managing system shutdown or restart. Depending on specifics of your system you may need to change this option.\n\n"
						"When set to \"Default\" the app will trigger the shutdown/restart routine "
						"as late as possible during system shutdown which for most systems will work fine. The \"Early\" method will "
						"trigger the shutdown/restart routine a bit earlier, which will consequently leave more time during shutdown for communicating with the devices, but with an "
						"increased risk of not properly detecting restarts. On the other hand, if the system has issues with detecting restarts properly then please consider changing to "
						"\"Delayed\" which will add a slight delay to the shutdown, thus allowing more time for the system to properly communicate with the app. "
						"\n\nDo you want to contribute to automatic detection of this language in a future release? Please press \"Yes\" to open a google sheet where you can submit your input or "
						"click \"No\" to close this information dialog.",

						L"Shutdown options", MB_YESNO | MB_ICONQUESTION) == IDYES)
					{
						ShellExecute(0, 0, RESTARTWORDSLINK, 0, 0, SW_SHOW);
					}
			}
			// explain the power on timeout, logging etc
			else if (wParam == IDC_SYSLINK7)
			{
				customMsgBox(hWnd, L"The power on timeout sets the maximum number of seconds the app will attempt to power on devices. "
					"Please consider increasing this value if your PC needs more time to establish contact with the WebOS-devices during system boot.\n\n"
					"The option to enable logging is very useful for troubleshooting issues. If you are experiencing issues with the operations of this app please configure the "
					"log level to \"Debug\" to properly capture any issues.\n\n"
					"The option to automatically notify of-, or silently install, new versions of the application ensure that LGTV Companion is up-to-date.",
					L"Global options", MB_OK | MB_ICONINFORMATION);
			}
			// explain the power saving options
			else if (wParam == IDC_SYSLINK5)
			{
				customMsgBox(hWnd, L"The user idle mode will automatically blank the screen, i e turn the transmitters off, in the absence of user input from keyboard, mouse and/or controllers. "
					"The difference, when compared to both the screensaver and windows power plan settings, is that those OS implemented power saving features "
					"utilize more obscured variables for determining user idle / busy states, and which can also be programmatically overridden f.e. by games, "
					"media players, production software or your web browser, In short, and simplified, this option is a more aggressively configured screen and "
					"power saver. \n\n"
					"The option to support remote streaming hosts will power off or blank the screen of managed devices while the system is acting as streaming host or being remoted into. Supported "
					"hosts include Nvidia gamestream, Moonlight, Sunshine, Apollo, Steam Link and RDP. \n\nPlease note that the devices will remain powered off / blanked until the remote connection is disconnected. \n\n"
					"NOTE! Support for detecting Sunshine/Apollo streaming host require Sunshine/Apollo to be installed (i.e. not portable install) and Sunshine/Apollo logging level be at minimum on level \"Info\" (default)",
					L"Andvanced power options", MB_OK | MB_ICONINFORMATION);
			}
			// explain the multi-monitor conf
			else if (wParam == IDC_SYSLINK6)
			{
				customMsgBox(hWnd, L"The option to support the windows multi-monitor topology ensures that the "
					"power state of individual devices will match the enabled or disabled state in the Windows monitor configuration, i e "
					"when an HDMI-output is disabled in the graphics card configuration the associated device will also power off. "
					"If you have a multi-monitor system and a compatible WebOS device it is recommended to configure and enable this feature. It may also be "
					"beneficial to enable the feature even for single-monitor systems. \n\n"
					"The option to restore the topology configuration at logon screen will apply the last known configuration immediately at the logon "
					"screen during system boot, instead of after the user logged in.\n\n"
					"PLEASE NOTE! Support for the monitor topology option is limited to models with the \"Always Ready\" feature, i.e. 2022 models C2, G2 etc and onwards. "
					"\n\n"
					"\"Always Ready\" must be enabled in the settings of a compatible WebOS device to ensure that "
					"the feature works properly.\n\n"
					"ALSO NOTE! A change of GPU or addition of more displays may invalidate the configuration. If so, please run the configuration guide "
					"again to ensure correct operation.",
					L"Multi-monitor support", MB_OK | MB_ICONINFORMATION);
			}
			
			// explain the external api
			else if (wParam == IDC_SYSLINK11)
			{
				if (customMsgBox(hWnd, L"Scripts or other applications can use the \"External API\" to be notified of power events and to send commands to the devices. The "
					"usability of this is up to the creativity of the user, but can for example be used to trigger power state changes "
					"of non-LG devices, execute scripts, trigger RGB profiles etc. The functionality and implementation is currently in BETA and "
					"is preferrably discussed in the Discord #external-api channel (https://discord.gg/7KkTPrP3fq)\n\n"
					"The current implementation implements intra-process-communication via a named pipe and example scripts are available "
					"in the #external-api channel.\n\nDo you wish to go to Discord? Please click \"Yes\" to go to Discord or click \"No\" "
					"to close this information window", L"External API", MB_YESNO | MB_ICONQUESTION) == IDYES)
				{
					ShellExecute(0, 0, DISCORDLINK, 0, 0, SW_SHOW);
				}
				
			}
			else if (wParam == IDC_SYSLINK12)
			{

				TCHAR buffer[MAX_PATH] = { 0 };
				if(GetModuleFileName(NULL, buffer, MAX_PATH))
				{
					std::wstring path = buffer;
					std::wstring::size_type pos = path.find_last_of(L"\\/");
					path = path.substr(0, pos + 1);
					std::wstring exe = path;
					exe += L"LGTVupdater.exe";
					ShellExecute(NULL, L"open", exe.c_str(), NULL, path.c_str(), SW_SHOW);
				}
			}
			else if (wParam == IDC_SYSLINK_CONF)
			{
				if (Prefs.devices_.size() == 0)
				{
					customMsgBox(hWnd, L"Please configure devices before enabling and configuring this option", L"Error", MB_ICONEXCLAMATION | MB_OK);
					CheckDlgButton(hWnd, IDC_CHECK_TOPOLOGY, BST_UNCHECKED);
				}
				else
				{
					h_topology_wnd = CreateDialog(h_instance, MAKEINTRESOURCE(IDD_CONFIGURE_TOPOLOGY), hWnd, (DLGPROC)WndTopologyProc);
					EnableWindow(hWnd, false);
					ShowWindow(h_topology_wnd, SW_SHOW);
				}
			}
			else if (wParam == IDC_SYSLINK_CONF2)
			{
				h_user_idle_mode_wnd = CreateDialog(h_instance, MAKEINTRESOURCE(IDD_ADVANCEDIDLE), hWnd, (DLGPROC)WndUserIdleProc);
				EnableWindow(hWnd, false);
				ShowWindow(h_user_idle_mode_wnd, SW_SHOW);
			}
		}break;
		case LVN_ITEMCHANGED:
		{
			if (wParam == IDC_LIST)
			{
				LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
				if (pnmv->uChanged & LVIF_STATE) // item state has been changed
				{
					bool bPrevState = (((pnmv->uOldState & LVIS_STATEIMAGEMASK) >> 12) - 1) == 1 ? true : false;   // Old check box state
					bool bChecked = (((pnmv->uNewState & LVIS_STATEIMAGEMASK) >> 12) - 1) == 1 ? true : false;

					if (bPrevState == bChecked) // No change in check box
						break;

					EnableWindow(GetDlgItem(hWnd, IDOK), true);
				}
			}
		}break;
		default:break;
		}
	}break;

	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		SetTextColor(hdcStatic, COLORREF(COLOR_STATIC));
		if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_BLANK)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_REMOTE)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_AUTOUPDATE))
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
		EndDialog(hWnd, 0);
	}break;
	case WM_DESTROY:
	{
		h_options_wnd = NULL;
	}	break;
	default:
		return false;
	}
	return true;
}

//   Process messages for the options window
LRESULT CALLBACK WndTopologyProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SetCurrentProcessExplicitAppUserModelID(L"JPersson.LGTVCompanion.18");
		SendDlgItemMessage(hWnd, IDC_COMBO, WM_SETFONT, (WPARAM)h_edit_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_NO_1, WM_SETFONT, (WPARAM)h_edit_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_NO_2, WM_SETFONT, (WPARAM)h_edit_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_NO_3, WM_SETFONT, (WPARAM)h_edit_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_NO_4, WM_SETFONT, (WPARAM)h_edit_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_NO_5, WM_SETFONT, (WPARAM)h_edit_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_NO_6, WM_SETFONT, (WPARAM)h_edit_font, MAKELPARAM(TRUE, 0));

		SendDlgItemMessage(hWnd, IDC_STATIC_T_11, WM_SETFONT, (WPARAM)h_edit_small_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_T_12, WM_SETFONT, (WPARAM)h_edit_small_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_T_13, WM_SETFONT, (WPARAM)h_edit_small_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_T_14, WM_SETFONT, (WPARAM)h_edit_small_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_T_15, WM_SETFONT, (WPARAM)h_edit_small_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_T_16, WM_SETFONT, (WPARAM)h_edit_small_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_STATUS_1, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_STATIC_STATUS_2, WM_SETFONT, (WPARAM)h_edit_medium_bold_font, MAKELPARAM(TRUE, 0));
		SetWindowText(GetDlgItem(hWnd, IDC_STATIC_STATUS_2), L"Not configured!");
		for (auto& k : Prefs.devices_)
		{
			if (k.uniqueDeviceKey != "")
			{
				SetWindowText(GetDlgItem(hWnd, IDC_STATIC_STATUS_2), L"Waiting for input...");
			}
		}

		SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
		SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)-1, (LPARAM)0);

		if (Prefs.devices_.size() > 0)
		{
			for (const auto& item : Prefs.devices_)
			{
				std::stringstream s;
				s << item.id << ": " << item.name << "(" << item.ip << ")";
				SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_ADDSTRING, (WPARAM)0, (LPARAM)tools::widen(s.str()).c_str());
			}
			SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
		}
		SendMessage(hWnd, APP_TOP_PHASE_1, NULL, NULL);
		i_top_configuration_display = 0;
	}break;
	case APP_TOP_PHASE_1:
	{
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_1), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_2), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_3), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_4), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_11), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_12), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_13), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_14), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_5), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_15), false);
		EnableWindow(GetDlgItem(hWnd, IDC_COMBO), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_6), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_16), false);
		SetWindowText(GetDlgItem(hWnd, IDOK), L"&Start");
		i_top_configuration_phase = 1;
	}break;
	case APP_TOP_PHASE_2:
	{
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_1), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_2), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_3), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_4), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_11), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_12), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_13), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_14), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_5), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_15), true);
		EnableWindow(GetDlgItem(hWnd, IDC_COMBO), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_6), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_16), false);
		SetWindowText(GetDlgItem(hWnd, IDC_STATIC_STATUS_2), L"Updating configuration!");
		SetWindowText(GetDlgItem(hWnd, IDOK), L"&Next");
		i_top_configuration_phase = 2;
	}break;
	case APP_TOP_PHASE_3:
	{
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_1), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_2), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_3), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_4), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_11), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_12), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_13), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_14), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_5), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_15), false);
		EnableWindow(GetDlgItem(hWnd, IDC_COMBO), false);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_NO_6), true);
		EnableWindow(GetDlgItem(hWnd, IDC_STATIC_T_16), true);
		SetWindowText(GetDlgItem(hWnd, IDOK), L"&Finish");
		i_top_configuration_phase = 3;
		for (auto& k : Prefs.devices_)
		{
			if (k.uniqueDeviceKeyTemporary != "")
			{
				SetWindowText(GetDlgItem(hWnd, IDC_STATIC_STATUS_2), L"All OK!");
			}
		}
	}break;
	case APP_TOP_NEXT_DISPLAY:
	{
		std::vector<DisplayInfo> displays = queryDisplays();
		if (displays.size() > i_top_configuration_display)
		{
			RECT DialogRect;
			RECT DisplayRect;
			GetWindowRect(hWnd, &DialogRect);
			DisplayRect = displays[i_top_configuration_display].monitorinfo.rcWork;

			int x = DisplayRect.left + (DisplayRect.right - DisplayRect.left) / 2 - (DialogRect.right - DialogRect.left) / 2;
			int y = DisplayRect.top + (DisplayRect.bottom - DisplayRect.top) / 2 - (DialogRect.bottom - DialogRect.top) / 2;
			int cx = DialogRect.right - DialogRect.left;
			int cy = DialogRect.bottom - DialogRect.top;
			MoveWindow(hWnd, x, y, cx, cy, true);
		}
	}break;

	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDOK:
			{
				switch (i_top_configuration_phase) // three phases - intro, match displays, finalise
				{
				case 1:
				{
					std::vector<DisplayInfo> displays = queryDisplays();
					// No WebOs devices attached
					if (displays.size() == 0)
					{
						customMsgBox(hWnd, L"To configure your devices, ensure that all your WebOS-devices are powered ON, connected to to your PC and enabled (with an extended desktop in case of multiple displays).", L"No WebOS devices detected", MB_OK | MB_ICONWARNING);
						break;
					}
					// If exactly one physical device connected/enabled and exactly one device cofigured it is considered an automatic match
					else if (Prefs.devices_.size() == 1 && displays.size() == 1)
					{
						if (customMsgBox(hWnd, L"Your device can be automatically configured.\n\nDo you want to accept the automatic configuration?", L"Automatic match", MB_YESNO) == IDYES)
						{
							Prefs.devices_[0].uniqueDeviceKeyTemporary = tools::narrow(displays[0].target.monitorDevicePath);
							SendMessage(hWnd, APP_TOP_PHASE_3, NULL, NULL);
							SetWindowText(GetDlgItem(hWnd, IDC_STATIC_STATUS_2), L"All OK!");
							break;
						}
					}
					SendMessage(hWnd, APP_TOP_PHASE_2, NULL, NULL);
					SendMessage(hWnd, APP_TOP_NEXT_DISPLAY, NULL, NULL);
				}break;
				case 2:
				{
					std::vector<DisplayInfo> displays = queryDisplays();
					int sel = (int)(SendMessage(GetDlgItem(hWnd, IDC_COMBO), (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0));
					if (sel == CB_ERR || Prefs.devices_.size() <= sel)
						break;
					Prefs.devices_[sel].uniqueDeviceKeyTemporary = tools::narrow(displays[i_top_configuration_display].target.monitorDevicePath);

					i_top_configuration_display++;
					if (i_top_configuration_display >= displays.size()) // all displays iterated
					{
						SendMessage(hWnd, APP_TOP_PHASE_3, NULL, NULL);
					}
					else // more displays are connected
					{
						SendMessage(hWnd, APP_TOP_NEXT_DISPLAY, NULL, NULL);
					}
				}break;
				case 3:
				{
					for (auto& k : Prefs.devices_)
					{
						k.uniqueDeviceKey = k.uniqueDeviceKeyTemporary;
					}
					EndDialog(hWnd, 0);
					EnableWindow(GetParent(hWnd), true);
					EnableWindow(GetDlgItem(GetParent(hWnd), IDOK), true);
				}break;
				default:break;
				}
			}break;
			case IDCANCEL:
			{
				bool conf = false;
				for (auto& k : Prefs.devices_)
				{
					if (k.uniqueDeviceKey != "")
					{
						conf = true;
						break;
					}
				}
				if (!conf)
				{
					CheckDlgButton(GetParent(hWnd), IDC_CHECK_TOPOLOGY, BST_UNCHECKED);
					EnableWindow(GetDlgItem(GetParent(hWnd), IDC_CHECK_TOPOLOGY_LOGON), false);
				}
				EndDialog(hWnd, 0);
				EnableWindow(GetParent(hWnd), true);
			}break;
			default:break;
			}
		}break;
		default:break;
		}
	}break;
	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		if ((HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_1)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_2)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_3)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_4)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_T_1)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_T_2)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_T_3)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_T_4)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_STATUS_1)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_STATUS_2))
		{
			SetBkMode(hdcStatic, TRANSPARENT);
		}
		if ((HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_STATUS_2))
		{
			std::wstring s = tools::getWndText(GetDlgItem(hWnd, IDC_STATIC_STATUS_2));
			if (s.find(L"OK!") != std::wstring::npos)
				SetTextColor(hdcStatic, COLORREF(COLOR_GREEN));
			else if (s.find(L"Updating") != std::wstring::npos)
				SetTextColor(hdcStatic, COLORREF(COLOR_BLUE));
			else if (s.find(L"Waiting") != std::wstring::npos)
				SetTextColor(hdcStatic, COLORREF(COLOR_BLUE));

			else
				SetTextColor(hdcStatic, COLORREF(COLOR_RED));
		}

		else if ((HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_1)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_2)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_3)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_STATIC_NO_4))
			SetTextColor(hdcStatic, COLORREF(COLOR_BLUE));
		else
			SetTextColor(hdcStatic, COLORREF(COLOR_STATIC));

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
		EndDialog(hWnd, 0);
		EnableWindow(GetParent(hWnd), true);
	}break;
	case  WM_DESTROY:
	{
		h_topology_wnd = NULL;
	}break;
	default:
		return false;
	}
	return true;
}

//   Process messages for the user idle conf window
LRESULT CALLBACK WndUserIdleProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SetCurrentProcessExplicitAppUserModelID(L"JPersson.LGTVCompanion.18");
		SendDlgItemMessage(hWnd, IDC_LIST, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_EDIT_TIME, WM_SETFONT, (WPARAM)h_edit_medium_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_SPIN, UDM_SETRANGE, (WPARAM)NULL, MAKELPARAM(240, 1));
		SendDlgItemMessage(hWnd, IDC_SPIN, UDM_SETPOS, (WPARAM)NULL, (LPARAM)Prefs.user_idle_mode_delay_);

		process_list_temp = Prefs.user_idle_mode_process_control_list_;
		SendMessage(hWnd, APP_LISTBOX_REDRAW, 1, 0);

		CheckDlgButton(hWnd, IDC_CHECK_PROCESS_CONTROL, Prefs.user_idle_mode_process_control_ ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_FULLSCREEN, Prefs.user_idle_mode_disable_while_fullscreen_ ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_VWL, Prefs.user_idle_mode_disable_while_video_wake_lock_ ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_VWL_FG, Prefs.user_idle_mode_disable_while_video_wake_lock_foreground_ ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_VWL_FULLSCREEN, Prefs.user_idle_mode_disable_while_video_wake_lock_fullscreen_ ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_CHECK_MUTE, Prefs.user_idle_mode_mute_speakers_ ? BST_CHECKED : BST_UNCHECKED);

		EnableWindow(GetDlgItem(hWnd, IDC_LIST), Prefs.user_idle_mode_process_control_);
		EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_ADD), Prefs.user_idle_mode_process_control_);
		EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_EDIR), Prefs.user_idle_mode_process_control_);
		EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_DELETE), Prefs.user_idle_mode_process_control_);
		EnableWindow(GetDlgItem(hWnd, IDC_CHECK_VWL_FG), Prefs.user_idle_mode_disable_while_video_wake_lock_);
		EnableWindow(GetDlgItem(hWnd, IDC_CHECK_VWL_FULLSCREEN), Prefs.user_idle_mode_disable_while_video_wake_lock_);

		EnableWindow(GetDlgItem(hWnd, IDOK), false);
	}break;
	case APP_LISTBOX_EDIT:
	{
		switch (wParam)
		{
		case 1:
		{
			int index = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETCURSEL, 0, 0);
			if (index != LB_ERR)
			{
				int data = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETITEMDATA, index, 0);
				if (data != LB_ERR && data < process_list_temp.size())
				{
					h_whitelist_wnd = CreateDialog(h_instance, MAKEINTRESOURCE(IDD_WHITELIST_EDIT), hWnd, (DLGPROC)WndWhitelistProc);
					SetWindowText(h_whitelist_wnd, L"Edit process control");
					SetWindowText(GetDlgItem(h_whitelist_wnd, IDOK), L"Change");
					SetWindowText(GetDlgItem(h_whitelist_wnd, IDC_EDIT_NAME), process_list_temp[data].friendly_name.c_str());
					SetWindowText(GetDlgItem(h_whitelist_wnd, IDC_EDIT_PROCESS), process_list_temp[data].binary.c_str());
					CheckDlgButton(h_whitelist_wnd, IDC_PC_CHECK_ISRUNNING, process_list_temp[data].process_control_disable_while_running ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(h_whitelist_wnd, IDC_PC_CHECK_ISFOREGROUND, process_list_temp[data].process_control_disable_while_running_foreground ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(h_whitelist_wnd, IDC_PC_CHECK_ISFULLSCREEN, process_list_temp[data].process_control_disable_while_running_fullscreen ? BST_CHECKED : BST_UNCHECKED);
					CheckDlgButton(h_whitelist_wnd, IDC_PC_CHECK_VWL, process_list_temp[data].process_control_disable_while_running_display_lock ? BST_CHECKED : BST_UNCHECKED);
					EnableWindow(GetDlgItem(h_whitelist_wnd, IDC_PC_CHECK_ISFOREGROUND), process_list_temp[data].process_control_disable_while_running);
					EnableWindow(GetDlgItem(h_whitelist_wnd, IDC_PC_CHECK_ISFULLSCREEN), process_list_temp[data].process_control_disable_while_running);
					EnableWindow(GetDlgItem(h_whitelist_wnd, IDC_PC_CHECK_VWL), process_list_temp[data].process_control_disable_while_running);
					EnableWindow(hWnd, false);
					ShowWindow(h_whitelist_wnd, SW_SHOW);
				}
			}
		}break;

		default:break;
		}
	}break;

	case APP_LISTBOX_ADD:
	{
		switch (wParam)
		{
		case 1:
		{
			h_whitelist_wnd = CreateDialog(h_instance, MAKEINTRESOURCE(IDD_WHITELIST_EDIT), hWnd, (DLGPROC)WndWhitelistProc);
			SetWindowText(h_whitelist_wnd, L"Add to process control");
			SetWindowText(GetDlgItem(h_whitelist_wnd, IDOK), L"Add");
			EnableWindow(hWnd, false);
			ShowWindow(h_whitelist_wnd, SW_SHOW);
		}break;

		default:break;
		}
	}break;
	case APP_LISTBOX_DELETE:
	{
		switch (wParam)
		{
		case 1:
		{
			int index = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETCURSEL, 0, 0);
			if (index != LB_ERR)
			{
				WCHAR* text;
				std::vector<WCHAR> buffer;
				int len = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETTEXTLEN, index, 0);
				buffer.resize(len + 1);
				text = reinterpret_cast<WCHAR*>(buffer.data());
				if (SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETTEXT, (WPARAM)index, (LPARAM)text) != LB_ERR)
				{
					std::wstring s = L"Do you want to delete '";
					s += text;
					s += L"' from process control?";
					if (customMsgBox(hWnd, s.c_str(), L"Delete item", MB_YESNO | MB_ICONQUESTION) == IDYES)
					{
						int data = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETITEMDATA, index, 0);
						if (data != LB_ERR && data < process_list_temp.size())
						{
							process_list_temp.erase(process_list_temp.begin() + data);
							SendMessage(hWnd, APP_LISTBOX_REDRAW, 1, 0);
							EnableWindow(GetDlgItem(hWnd, IDOK), true);
						}
						else
						{
							customMsgBox(hWnd, L"Could not delete item!", L"Error", MB_OK | MB_ICONINFORMATION);
						}
					}
				}
				else
				{
					customMsgBox(hWnd, L"There was an error when managing the listbox", L"Error", MB_OK | MB_ICONINFORMATION);
				}
			}
		}break;
		default:break;
		}
	}break;
	case APP_LISTBOX_REDRAW:
	{
		switch (wParam)
		{
		case 1:
		{
			int i = 0;
			SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_RESETCONTENT, 0, 0);
			for (auto& w : process_list_temp)
			{
				if (w.binary != L"" && w.friendly_name != L"")
				{
					int index = (int)SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_ADDSTRING, 0, (LPARAM)(w.friendly_name.c_str()));
					SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_SETITEMDATA, index, (LPARAM)i);
					i++;
				}
			}
			if (SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_GETCOUNT, 0, 0) > 0)
			{
				SendMessage(GetDlgItem(hWnd, IDC_LIST), LB_SETCURSEL, 0, 0);
				ShowWindow(GetDlgItem(hWnd, IDC_SYSLINK_EDIR), SW_SHOW);
				ShowWindow(GetDlgItem(hWnd, IDC_SYSLINK_DELETE), SW_SHOW);
			}
			else
			{
				ShowWindow(GetDlgItem(hWnd, IDC_SYSLINK_EDIR), SW_HIDE);
				ShowWindow(GetDlgItem(hWnd, IDC_SYSLINK_DELETE), SW_HIDE);
			}
		}break;
		default:break;
		}
	}break;
	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case LBN_DBLCLK:
		{
			switch (LOWORD(wParam))
			{
			case IDC_LIST:
			{
				PostMessage(hWnd, APP_LISTBOX_EDIT, 1, 0);
			}break;
			default:break;
			}		
		}break;

		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDC_CHECK_PROCESS_CONTROL:
			{
				EnableWindow(GetDlgItem(hWnd, IDC_LIST), IsDlgButtonChecked(hWnd, IDC_CHECK_PROCESS_CONTROL));
				EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_ADD), IsDlgButtonChecked(hWnd, IDC_CHECK_PROCESS_CONTROL));
				EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_EDIR), IsDlgButtonChecked(hWnd, IDC_CHECK_PROCESS_CONTROL));
				EnableWindow(GetDlgItem(hWnd, IDC_SYSLINK_DELETE), IsDlgButtonChecked(hWnd, IDC_CHECK_PROCESS_CONTROL));
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDC_CHECK_VWL:
			{
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_VWL_FG), IsDlgButtonChecked(hWnd, IDC_CHECK_VWL));
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_VWL_FULLSCREEN), IsDlgButtonChecked(hWnd, IDC_CHECK_VWL));
			}			
			case IDC_CHECK_FULLSCREEN:
			case IDC_CHECK_VWL_FG:
			case IDC_CHECK_VWL_FULLSCREEN:
			case IDC_CHECK_MUTE:
			{
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;
			case IDOK:
			{
				Prefs.user_idle_mode_delay_ = atoi(tools::narrow(tools::getWndText(GetDlgItem(hWnd, IDC_EDIT_TIME))).c_str());
				Prefs.user_idle_mode_process_control_ = IsDlgButtonChecked(hWnd, IDC_CHECK_PROCESS_CONTROL);
				Prefs.user_idle_mode_disable_while_fullscreen_ = IsDlgButtonChecked(hWnd, IDC_CHECK_FULLSCREEN);
				Prefs.user_idle_mode_disable_while_video_wake_lock_ = IsDlgButtonChecked(hWnd, IDC_CHECK_VWL);
				Prefs.user_idle_mode_disable_while_video_wake_lock_foreground_ = IsDlgButtonChecked(hWnd, IDC_CHECK_VWL_FG);
				Prefs.user_idle_mode_disable_while_video_wake_lock_fullscreen_ = IsDlgButtonChecked(hWnd, IDC_CHECK_VWL_FULLSCREEN);
				Prefs.user_idle_mode_mute_speakers_ = IsDlgButtonChecked(hWnd, IDC_CHECK_MUTE);
				Prefs.user_idle_mode_process_control_list_ = process_list_temp;
				EndDialog(hWnd, 0);
				EnableWindow(GetParent(hWnd), true);
				EnableWindow(GetDlgItem(GetParent(hWnd), IDOK), true);
			}break;
			case IDCANCEL:
			{
				EndDialog(hWnd, 0);
				EnableWindow(GetParent(hWnd), true);
			}break;
			default:break;
			}
		}break;
		case EN_CHANGE:
		{
			switch (LOWORD(wParam))
			{
			case IDC_EDIT_TIME:
			{
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
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
			if (wParam == IDC_SYSLINK_INFO_1)
			{
				customMsgBox(hWnd, L"Please configure the time, in minutes, without user input from keyboard, mouse or game controllers before "
					"User Idle Mode is triggered and the screen is blanked.\r\n\r\nScreen blanking can be globally disabled for "
					"fullscreen applications and for web-browsers while a video or a game is playing. \r\n\r\nAlso select whether the built-in speakers "
					"shall be muted while the screen is blanked", L"User Idle Mode configuration", MB_OK | MB_ICONINFORMATION);
			}
			else if (wParam == IDC_SYSLINK_INFO_2)
			{
				customMsgBox(hWnd, L"Enable detailed process control to set rules and conditions for a specific process. E.g. to prevent "
					"the screen blanking while running a specific movie player.", L"User Idle Mode process control", MB_OK | MB_ICONINFORMATION);
			}
			else if (wParam == IDC_SYSLINK_ADD)
			{
				PostMessage(hWnd, APP_LISTBOX_ADD, 1, 0);
			}
			else if (wParam == IDC_SYSLINK_EDIR)
			{
				PostMessage(hWnd, APP_LISTBOX_EDIT, 1, 0);
			}
			else if (wParam == IDC_SYSLINK_DELETE)
			{
				PostMessage(hWnd, APP_LISTBOX_DELETE, 1, 0);
			}
		}break;
		default:break;
		}
	}break;
	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		SetTextColor(hdcStatic, COLORREF(COLOR_STATIC));
		if ((HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_PROCESS_CONTROL)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_FULLSCREEN)
			|| (HWND)lParam == GetDlgItem(hWnd, IDC_CHECK_VWL))
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
		EndDialog(hWnd, 0);
		EnableWindow(GetParent(hWnd), true);
	}break;
	case WM_DESTROY:
	{
		h_user_idle_mode_wnd = NULL;
	}break;
	default:
		return false;
	}
	return true;
}

//   Process messages for the user idle conf window
LRESULT CALLBACK WndWhitelistProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SetCurrentProcessExplicitAppUserModelID(L"JPersson.LGTVCompanion.18");
		SendDlgItemMessage(hWnd, IDC_EDIT_NAME, WM_SETFONT, (WPARAM)h_edit_font, MAKELPARAM(TRUE, 0));
		SendDlgItemMessage(hWnd, IDC_EDIT_PROCESS, WM_SETFONT, (WPARAM)h_edit_font, MAKELPARAM(TRUE, 0));
	}break;

	case WM_COMMAND:
	{
		switch (HIWORD(wParam))
		{
		case BN_CLICKED:
		{
			switch (LOWORD(wParam))
			{
			case IDC_PC_CHECK_ISRUNNING:
			{
				EnableWindow(GetDlgItem(hWnd, IDC_PC_CHECK_ISFULLSCREEN), IsDlgButtonChecked(hWnd, IDC_PC_CHECK_ISRUNNING));
				EnableWindow(GetDlgItem(hWnd, IDC_PC_CHECK_ISFOREGROUND), IsDlgButtonChecked(hWnd, IDC_PC_CHECK_ISRUNNING));
				EnableWindow(GetDlgItem(hWnd, IDC_PC_CHECK_VWL), IsDlgButtonChecked(hWnd, IDC_PC_CHECK_ISRUNNING));
			}
			case IDC_PC_CHECK_ISFULLSCREEN:
			case IDC_PC_CHECK_ISFOREGROUND:
			case IDC_PC_CHECK_VWL:
			{
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
			}break;


			case IDOK:
			{
				std::wstring name = tools::getWndText(GetDlgItem(hWnd, IDC_EDIT_NAME));
				std::wstring proc = tools::tolower(tools::getWndText(GetDlgItem(hWnd, IDC_EDIT_PROCESS)));

				if (name.find_last_of(L"\\/") != std::string::npos)
				{
					name = name.substr(name.find_last_of(L"\\/"));
					name.erase(0, 1);
				}
				if (proc.find_last_of(L"\\/") != std::string::npos)
				{
					proc = proc.substr(proc.find_last_of(L"\\/"));
					proc.erase(0, 1);
				}

				if (name != L"" && proc != L"")
				{
					if (tools::getWndText(GetDlgItem(hWnd, IDOK)) == L"Add") // add item
					{
						Preferences::ProcessList w;
						w.friendly_name = name;
						w.binary = proc;
						w.process_control_disable_while_running = IsDlgButtonChecked(hWnd, IDC_PC_CHECK_ISRUNNING);
						w.process_control_disable_while_running_foreground = IsDlgButtonChecked(hWnd, IDC_PC_CHECK_ISFOREGROUND);
						w.process_control_disable_while_running_fullscreen = IsDlgButtonChecked(hWnd, IDC_PC_CHECK_ISFULLSCREEN);
						w.process_control_disable_while_running_display_lock = IsDlgButtonChecked(hWnd, IDC_PC_CHECK_VWL);
						process_list_temp.push_back(w);
					}
					else //change item
					{
						if (h_user_idle_mode_wnd)
						{
							int index = (int)SendMessage(GetDlgItem(h_user_idle_mode_wnd, IDC_LIST), LB_GETCURSEL, 0, 0);
							if (index != LB_ERR)
							{
								int data = (int)SendMessage(GetDlgItem(h_user_idle_mode_wnd, IDC_LIST), LB_GETITEMDATA, index, 0);
								if (data != LB_ERR && data < process_list_temp.size())
								{
									process_list_temp[data].binary = proc;
									process_list_temp[data].friendly_name = name;
									process_list_temp[data].process_control_disable_while_running = IsDlgButtonChecked(hWnd, IDC_PC_CHECK_ISRUNNING);
									process_list_temp[data].process_control_disable_while_running_foreground = IsDlgButtonChecked(hWnd, IDC_PC_CHECK_ISFOREGROUND);
									process_list_temp[data].process_control_disable_while_running_fullscreen = IsDlgButtonChecked(hWnd, IDC_PC_CHECK_ISFULLSCREEN);
									process_list_temp[data].process_control_disable_while_running_display_lock = IsDlgButtonChecked(hWnd, IDC_PC_CHECK_VWL);
								}
							}
						}
					}

					EndDialog(hWnd, 0);
					SendMessage(GetParent(hWnd), APP_LISTBOX_REDRAW, 1, 0);
					EnableWindow(GetParent(hWnd), true);
					EnableWindow(GetDlgItem(GetParent(hWnd), IDOK), true);
				}
				else
				{
					customMsgBox(hWnd, L"Please ensure that both display name and process executable name are properly "
						"configured before continuing. Please note that process executable name should not include the path.",
						L"Invalid configuration", MB_OK | MB_ICONERROR);
				}
			}break;
			case IDCANCEL:
			{
				EndDialog(hWnd, 0);
				EnableWindow(GetParent(hWnd), true);
			}break;
			default:break;
			}
		}break;
		case EN_CHANGE:
		{
			switch (LOWORD(wParam))
			{
			case IDC_EDIT_NAME:
			case IDC_EDIT_PROCESS:
			{
				EnableWindow(GetDlgItem(hWnd, IDOK), true);
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
			if (wParam == IDC_SYSLINK7)
			{
				customMsgBox(hWnd, L"Please type display name and name of the executable for the process which should prevent Screen Blanking. "
					"The name of the executable can contain wildcard characters (*) to match multiple processes.",
					L"Process Control configuration", MB_OK | MB_ICONINFORMATION);

			}
			if (wParam == IDC_SYSLINK13)
			{
				customMsgBox(hWnd, L"Please select the specific rules for the process by selecting the appropriate checkboxes. \r\n\r\n"
					"Please note that Universal Windows Platform (UWP) apps are not always detected correctly.",
					L"Process Control configuration", MB_OK | MB_ICONINFORMATION);
			}
			if (wParam == IDC_SYSLINK_WILDCARD)
			{
				customMsgBox(hWnd, L"The name of the executable can contain wildcard characters (*) to match multiple processes.\r\n\r\n"
					"It is for example possible to set the executable name to \"*player*.exe\" to match multiple media pleyers, e.g. \"Microsoft.Media.Player.exe\", \"PotPlayerMini64.exe\" and \"KMPlayer64.exe\"",
					L"Process Control wildcards", MB_OK | MB_ICONINFORMATION);

			}
			else if (wParam == IDC_SYSLINK_BROWSE)
			{
				IFileOpenDialog* pfd;
				DWORD dwOptions;
				LPWSTR Path;
				IShellItem* psi;
				COMDLG_FILTERSPEC aFileTypes[] = {
					{ L"Executable files", L"*.exe" },
					{ L"All files", L"*.*" },
				};

				if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
					if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pfd)))) {
						if (SUCCEEDED(pfd->GetOptions(&dwOptions)))
						{
							pfd->SetOptions(dwOptions | FOS_FILEMUSTEXIST);
							pfd->SetFileTypes(_countof(aFileTypes), aFileTypes);
							if (SUCCEEDED(pfd->Show(hWnd)))
							{
								if (SUCCEEDED(pfd->GetResult(&psi))) {
									if (SUCCEEDED(psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &Path)))
									{
										TCHAR buffer[MAX_PATH] = { 0 };
										if (GetModuleFileName(NULL, buffer, MAX_PATH))
										{
											std::wstring path = Path;
											std::wstring exe, name;
											std::wstring::size_type pos = path.find_last_of(L"\\/");
											if (pos != std::wstring::npos)
											{
												exe = path.substr(pos);
												exe.erase(0, 1);
												pos = exe.find_last_of(L".");
												if (pos != std::wstring::npos)
												{
													name = exe.substr(0, pos);
												}
												if (tools::getWndText(GetDlgItem(hWnd, IDC_EDIT_NAME)) == L"")
													SetWindowText(GetDlgItem(hWnd, IDC_EDIT_NAME), name.c_str());
												SetWindowText(GetDlgItem(hWnd, IDC_EDIT_PROCESS), exe.c_str());
											}
											CoTaskMemFree(Path);
										}
									}
									psi->Release();
								}
							}
						}
						pfd->Release();
					}
					CoUninitialize();
				}
			}
		}break;
		default:break;
		}
	}break;
	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		SetTextColor(hdcStatic, COLORREF(COLOR_STATIC));
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
		EndDialog(hWnd, 0);
		EnableWindow(GetParent(hWnd), true);
	}break;
	case WM_DESTROY:
	{
		h_whitelist_wnd = NULL;
	}break;
	default:
		return false;
	}
	return true;
}
INT_PTR CALLBACK CustomMsgBoxProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SetCurrentProcessExplicitAppUserModelID(L"JPersson.LGTVCompanion.18");
		auto params = reinterpret_cast<CUSTOMMSGBOXPARAMS*>(lParam);
		h_custom_messagebox_wnd = hWnd;
		SetDlgItemText(hWnd, IDC_MB_STATIC, params->lpText);
		SetWindowText(hWnd, params->lpCaption);
		SendDlgItemMessage(hWnd, IDC_MB_STATIC, WM_SETFONT, (WPARAM)h_edit_small_font, MAKELPARAM(TRUE, 0));

		//measure size of text
		RECT rcText = { 0, 0, 400, 0 };

		HDC hdc = GetDC(GetDlgItem(hWnd, IDC_MB_STATIC));
		HFONT hFont = (HFONT)SendDlgItemMessage(hWnd, IDC_MB_STATIC, WM_GETFONT, 0, 0);
		HFONT hOld = (HFONT)SelectObject(hdc, hFont);

		DrawText(hdc, params->lpText, -1, &rcText, DT_WORDBREAK | DT_CALCRECT);

		SelectObject(hdc, hOld);
		ReleaseDC(GetDlgItem(hWnd, IDC_MB_STATIC), hdc);

		//size window and controls
		if (rcText.right < 200)
			rcText.right = 200;
		SetWindowPos(hWnd, NULL, 0, 0, rcText.right + 120, rcText.bottom + 100, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
		SetWindowPos(GetDlgItem(hWnd, IDB1), NULL, rcText.right - 10, rcText.bottom + 20, 90, 28, SWP_NOZORDER | SWP_FRAMECHANGED);
		SetWindowPos(GetDlgItem(hWnd, IDB2), NULL, rcText.right - 110, rcText.bottom + 20, 90, 28, SWP_NOZORDER | SWP_FRAMECHANGED);
		SetWindowPos(GetDlgItem(hWnd, IDB3), NULL, rcText.right - 210, rcText.bottom + 20, 90, 28, SWP_NOZORDER | SWP_FRAMECHANGED);
		SetWindowPos(GetDlgItem(hWnd, IDC_MB_STATIC), NULL,
			60, 10, rcText.right, rcText.bottom,
			SWP_NOZORDER);

		//reposition window
		HWND hParent = GetParent(hWnd);
		if (!hParent) {
			hParent = GetDesktopWindow(); // fallback if no parent
		}

		RECT rcParent, rcDlg;
		GetWindowRect(hParent, &rcParent);
		GetWindowRect(hWnd, &rcDlg);

		int dlgWidth = rcDlg.right - rcDlg.left;
		int dlgHeight = rcDlg.bottom - rcDlg.top;

		int parentWidth = rcParent.right - rcParent.left;
		int parentHeight = rcParent.bottom - rcParent.top;

		int x = rcParent.left + (parentWidth - dlgWidth) / 2;
		int y = rcParent.top + (parentHeight - dlgHeight) / 2;

		// Ensure dialog stays fully visible on screen
		SetWindowPos(hWnd, HWND_TOP, x, y, 0, 0,
			SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

		// icon
		SHSTOCKICONINFO sii = { sizeof(sii) };
		HRESULT hr = NULL;
		if ((params->uType & MB_ICONMASK) == MB_ICONERROR)
			hr = SHGetStockIconInfo(SIID_ERROR, SHGSI_ICON | SHGSI_LARGEICON, &sii);
		else if ((params->uType & MB_ICONMASK) == MB_ICONWARNING)
			hr = SHGetStockIconInfo(SIID_WARNING, SHGSI_ICON | SHGSI_LARGEICON, &sii);
		else if ((params->uType & MB_ICONMASK) == MB_ICONINFORMATION)
			hr = SHGetStockIconInfo(SIID_INFO, SHGSI_ICON | SHGSI_LARGEICON, &sii);
		else if ((params->uType & MB_ICONMASK) == MB_ICONQUESTION)
			hr = SHGetStockIconInfo(SIID_HELP, SHGSI_ICON | SHGSI_LARGEICON, &sii);

		if (hr == S_OK)
			SendDlgItemMessage(hWnd, IDC_MB_ICON, STM_SETICON, (WPARAM)sii.hIcon, 0);

		// button
		HWND hBtn1 = GetDlgItem(hWnd, IDB1);
		HWND hBtn2 = GetDlgItem(hWnd, IDB2);
		HWND hBtn3 = GetDlgItem(hWnd, IDB3);

		ShowWindow(hBtn1, SW_HIDE);
		ShowWindow(hBtn2, SW_HIDE);
		ShowWindow(hBtn3, SW_HIDE);

		if ((params->uType & 0x0000000F) == MB_OKCANCEL) {
			SetWindowText(hBtn2, L"OK");     SetWindowLongPtr(hBtn2, GWLP_ID, IDOK);     ShowWindow(hBtn2, SW_SHOW);
			SetWindowText(hBtn1, L"Cancel"); SetWindowLongPtr(hBtn1, GWLP_ID, IDCANCEL); ShowWindow(hBtn1, SW_SHOW);
		}
		else if ((params->uType & 0x0000000F) == MB_YESNO) {
			SetWindowText(hBtn2, L"Yes");    SetWindowLongPtr(hBtn2, GWLP_ID, IDYES);    ShowWindow(hBtn2, SW_SHOW);
			SetWindowText(hBtn1, L"No");     SetWindowLongPtr(hBtn1, GWLP_ID, IDNO);     ShowWindow(hBtn1, SW_SHOW);
		}
		else if ((params->uType & 0x0000000F) == MB_YESNOCANCEL) {
			SetWindowText(hBtn3, L"Yes");    SetWindowLongPtr(hBtn3, GWLP_ID, IDYES);    ShowWindow(hBtn3, SW_SHOW);
			SetWindowText(hBtn2, L"No");     SetWindowLongPtr(hBtn2, GWLP_ID, IDNO);     ShowWindow(hBtn2, SW_SHOW);
			SetWindowText(hBtn1, L"Cancel"); SetWindowLongPtr(hBtn1, GWLP_ID, IDCANCEL); ShowWindow(hBtn1, SW_SHOW);
		}
		else if ((params->uType & 0x0000000F) == MB_RETRYCANCEL) {
			SetWindowText(hBtn2, L"Retry");  SetWindowLongPtr(hBtn2, GWLP_ID, IDRETRY);  ShowWindow(hBtn2, SW_SHOW);
			SetWindowText(hBtn1, L"Cancel"); SetWindowLongPtr(hBtn1, GWLP_ID, IDCANCEL); ShowWindow(hBtn1, SW_SHOW);
		}
		else { // default MB_OK
			SetWindowText(hBtn1, L"OK");     SetWindowLongPtr(hBtn1, GWLP_ID, IDOK);     ShowWindow(hBtn1, SW_SHOW);
		}

		// default button
		int defButtonId = IDOK;
		if ((params->uType & 0x00000F00) == MB_DEFBUTTON2)
			defButtonId = GetDlgCtrlID(hBtn2);
		else if ((params->uType & 0x00000F00) == MB_DEFBUTTON3)
			defButtonId = GetDlgCtrlID(hBtn3);
		else 
			defButtonId = GetDlgCtrlID(hBtn1);

		HWND hDefBtn = GetDlgItem(hWnd, defButtonId);
		if (hDefBtn) {
			SendMessage(hWnd, DM_SETDEFID, defButtonId, 0);
			SetFocus(hDefBtn);
		}

		return FALSE;
	}break;
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
	case WM_COMMAND:
	{
		EndDialog(hWnd, LOWORD(wParam));
	}break;
	case WM_CLOSE:
	{
		EndDialog(hWnd, IDCANCEL);
	}break;
	case WM_DESTROY:
	{
		h_custom_messagebox_wnd = NULL;
	}break;
	default:
		return false;
	}

	return true;
}

//   If the application is already running, send the command line parameters to that other process
bool messageExistingProcess(std::wstring command_line)
{
	std::wstring window_title;
	std::wstring sWinSearch;
	HWND remote_hWnd;
	HWND daemon_hWnd;
	COPYDATASTRUCT cds;

	window_title = APPNAME;
	window_title += L" Daemon v";
	window_title += APP_VERSION;
	sWinSearch = window_title;
	daemon_hWnd = FindWindow(NULL, sWinSearch.c_str());

	window_title = APPNAME;
	window_title += L" v";
	window_title += APP_VERSION;
	sWinSearch = window_title;
	remote_hWnd = FindWindow(NULL, sWinSearch.c_str());


	if (command_line == L"")
	{
		if(remote_hWnd)
		{
			cds.cbData = 0;
			cds.lpData = NULL;
			cds.dwData = NOTIFY_NEW_COMMANDLINE;
			SendMessage(remote_hWnd, WM_COPYDATA, NULL, (LPARAM)&cds);
			return true;
		}
	}
	else
	{
		cds.cbData = (DWORD)(command_line.size() * sizeof(WCHAR) + sizeof(WCHAR));
		cds.lpData = (PVOID)command_line.data();
		cds.dwData = NOTIFY_NEW_COMMANDLINE;
		if(daemon_hWnd)
		{
			SendMessage(daemon_hWnd, WM_COPYDATA, NULL, (LPARAM)&cds);
			return true;
		}
		else if (remote_hWnd)
		{
			SendMessage(remote_hWnd, WM_COPYDATA, NULL, (LPARAM)&cds);
			return true;
		}
	}
	return false;
}
//   Send the commandline to the service
void communicateWithService(std::wstring sData)
{
	int max_wait = 1000;
	while (!p_pipe_client->send(sData) && max_wait > 0)
	{
		Sleep(25);
		max_wait -= 25;
	}
	if(max_wait <= 0)
		customMsgBox(h_main_wnd, L"Failed to connect to named pipe. Service may be stopped.", L"Error", MB_OK | MB_ICONEXCLAMATION);
}
void threadVersionCheck(HWND hWnd)
{
	IStream* stream;
	char buff[100];
	std::string s;
	unsigned long bytesRead;
	nlohmann::json json_data;

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

	try
	{
		json_data = nlohmann::json::parse(s);
	}
	catch (...)
	{
		return;
	}
	if (!json_data["tag_name"].empty() && json_data["tag_name"].is_string())
	{
		std::string remote = json_data["tag_name"];
		std::vector <std::string> local_ver = tools::stringsplit(tools::narrow(APP_VERSION), ".");
		std::vector <std::string> remote_ver = tools::stringsplit(remote, "v.");

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
	return;
}

std::vector<DisplayInfo> queryDisplays()
{
	std::string logmsg = "Enumerating active displays...";

	std::vector<DisplayInfo> targets;
	//populate targets struct with information about attached displays
	EnumDisplayMonitors(NULL, NULL, meproc, (LPARAM)&targets);
	return targets;
}
static BOOL CALLBACK meproc(HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM pData)
{
	if (!pData)
		return false;
	std::string logmsg;
	std::vector<DisplayInfo>* targets = (std::vector<DisplayInfo> *) pData;
	UINT32 requiredPaths, requiredModes;
	std::vector<DISPLAYCONFIG_PATH_INFO> paths;
	std::vector<DISPLAYCONFIG_MODE_INFO> modes;
	MONITORINFOEX mi;
	LONG isError = ERROR_INSUFFICIENT_BUFFER;

	ZeroMemory(&mi, sizeof(mi));
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(hMonitor, &mi);
	logger->debug("Topology", "callback...");
	isError = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, &requiredModes);
	if (isError)
	{
		targets->clear();
		return false;
	}
	paths.resize(requiredPaths);
	modes.resize(requiredModes);

	logger->debug("Topology", "QueryDisplayConfig");
	isError = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, paths.data(), &requiredModes, modes.data(), NULL);
	if (isError)
	{
		targets->clear();
		return false;
	}
	paths.resize(requiredPaths);
	modes.resize(requiredModes);
	logger->debug("Topology", "Iterate paths");

	for (auto& p : paths)
	{

		DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
		sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
		sourceName.header.size = sizeof(sourceName);
		sourceName.header.adapterId = p.sourceInfo.adapterId;
		sourceName.header.id = p.sourceInfo.id;

		DisplayConfigGetDeviceInfo(&sourceName.header);
		std::wstring logmessage = L"monitorinfo: ";
		logmessage += mi.szDevice;
		logger->debug("Topology", tools::narrow(logmessage));
		logmessage = L"source: ";
		logmessage += sourceName.viewGdiDeviceName;
		logger->debug("Topology", tools::narrow(logmessage));
		if (wcscmp(mi.szDevice, sourceName.viewGdiDeviceName) == 0)
		{
			DISPLAYCONFIG_TARGET_DEVICE_NAME name;
			name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
			name.header.size = sizeof(name);
			name.header.adapterId = p.sourceInfo.adapterId;
			name.header.id = p.targetInfo.id;
			DisplayConfigGetDeviceInfo(&name.header);
			std::wstring FriendlyName = tools::tolower(name.monitorFriendlyDeviceName);
			logmessage = L"friendly name: ";
			logmessage += FriendlyName;
			logger->debug("Topology", tools::narrow(logmessage));
			if (FriendlyName.find(L"lg tv") != std::wstring::npos)
			{
				logger->debug("Topology", "Match!");
				DisplayInfo di;
				di.monitorinfo = mi;
				di.hMonitor = hMonitor;
				di.hdcMonitor = hdc;
				di.rcMonitor2 = *(LPRECT)lprcMonitor;
				di.target = name;
				targets->push_back(di);
			}
		}
	}
	return true;
}
bool messageDaemon(std::wstring cmdline)
{
	std::wstring cmd = tools::tolower(cmdline);
	bool idle = (cmd.find(L"-idle")) != std::wstring::npos;
	bool busy = (cmd.find(L"-unidle")) != std::wstring::npos;

	if (idle && busy)
		return false;
	if (!(idle || busy))
		return false;

	std::wstring window_title;
	window_title = APPNAME;
	window_title += L" Daemon v";
	window_title += APP_VERSION;
	std::wstring sWinSearch = window_title;
	HWND daemon_hWnd = FindWindow(NULL, sWinSearch.c_str());
	if (daemon_hWnd)
	{
		if (idle)
			PostMessage(daemon_hWnd, custom_daemon_idle_message, NULL, NULL);
		else if (busy)
			PostMessage(daemon_hWnd, custom_daemon_unidle_message, NULL, NULL);
		return true;
	}

	return false;
}

void ipcCallback(std::wstring message, LPVOID pt)
{
	return;
}

void prepareForUninstall(void)
{
	PostMessage(HWND_BROADCAST, custom_daemon_close_message, NULL, NULL);
	PostMessage(HWND_BROADCAST, custom_updater_close_message, NULL, (LPARAM)1);
	PostMessage(HWND_BROADCAST, custom_UI_close_message, NULL, NULL);
	return;
}
int customMsgBox(HWND hParent, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
{
	CUSTOMMSGBOXPARAMS params{ lpText, lpCaption, uType };
	return (int)DialogBoxParam(
		GetModuleHandle(NULL),
		MAKEINTRESOURCE(IDD_MESSAGEBOX),
		hParent,
		CustomMsgBoxProc,
		(LPARAM)&params);
}
