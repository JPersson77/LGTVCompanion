#pragma once
#include <windows.h>
#include <fstream>
#include <string>
#include <vector>
#include <Shlobj_core.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <nlohmann/json.hpp>

// common general application definitions
#define			APPNAME							L"LGTV Companion"
#define         APP_VERSION                     L"2.1.5"
#define			CONFIG_FILE						L"config.json"
#define			LOG_FILE						L"Log.txt"
#define			WINDOW_CLASS_UNIQUE				L"YOLOx0x0x0181818"
#define         PIPENAME                        TEXT("\\\\.\\pipe\\LGTVyolo")
#define         NEWRELEASELINK                  L"https://github.com/JPersson77/LGTVCompanion/releases"
#define         VERSIONCHECKLINK                L"https://api.github.com/repos/JPersson77/LGTVCompanion/releases"
#define         DONATELINK                      L"https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=jorgen.persson@gmail.com&lc=US&item_name=Friendly+gift+for+the+development+of+LGTV+Companion&no_note=0&cn=&currency_code=EUR&bn=PP-DonationsBF:btn_donateCC_LG.gif:NonHosted"

// common preferences definitions
#define         JSON_PREFS_NODE                 "LGTV Companion"
#define         JSON_EVENT_RESTART_STRINGS      "LocalEventLogRestartString"
#define         JSON_EVENT_SHUTDOWN_STRINGS     "LocalEventLogShutdownString"
#define         JSON_VERSION                    "Version"
#define         JSON_LOGGING                    "ExtendedLog"
#define         JSON_AUTOUPDATE                 "AutoUpdate"
#define         JSON_PWRONTIMEOUT               "PowerOnTimeOut"
#define         JSON_IDLEBLANK                  "BlankWhenIdle"
#define         JSON_IDLEBLANKDELAY             "BlankWhenIdleDelay"
#define         JSON_ADHERETOPOLOGY             "AdhereDisplayTopology"
#define         JSON_IDLEWHITELIST				"IdleWhiteListEnabled"
#define         JSON_IDLEFULLSCREEN				"IdleFullscreen"
#define         JSON_WHITELIST					"IdleWhiteList"
#define         JSON_REMOTESTREAM				"RemoteStream"
#define         JSON_TOPOLOGYMODE				"TopologyPreferPowerEfficient"
#define			JSON_DEVICE_NAME				"Name"
#define			JSON_DEVICE_IP					"IP"
#define			JSON_DEVICE_UNIQUEKEY			"UniqueDeviceKey"
#define			JSON_DEVICE_HDMICTRL			"HDMIinputcontrol"
#define			JSON_DEVICE_HDMICTRLNO			"OnlyTurnOffIfCurrentHDMIInputNumberIs"
#define			JSON_DEVICE_ENABLED				"Enabled"
#define			JSON_DEVICE_SESSIONKEY			"SessionKey"
#define			JSON_DEVICE_SUBNET				"Subnet"
#define			JSON_DEVICE_WOLTYPE				"WOL"
#define			JSON_DEVICE_SETHDMI				"SetHDMIInputOnResume"
#define			JSON_DEVICE_NEWSOCK				"NewSockConnect"
#define			JSON_DEVICE_SETHDMINO			"SetHDMIInputOnResumeToNumber"
#define			JSON_DEVICE_MAC					"MAC"
#define         DEFAULT_RESTART                 {"restart"}
#define         DEFAULT_SHUTDOWN                {"shutdown","power off"}
#define         WOL_NETWORKBROADCAST            1
#define         WOL_IPSEND                      2
#define         WOL_SUBNETBROADCAST             3
#define         WOL_DEFAULTSUBNET               L"255.255.255.0"

#define			APP_CMDLINE_ON					1
#define			APP_CMDLINE_OFF					2
#define			APP_CMDLINE_AUTOENABLE			3
#define			APP_CMDLINE_AUTODISABLE			4
#define			APP_CMDLINE_SCREENON			5
#define			APP_CMDLINE_SCREENOFF			6
#define			APP_IPC_DAEMON					7
#define			APP_CMDLINE_SETHDMI1			8
#define			APP_CMDLINE_SETHDMI2			9
#define			APP_CMDLINE_SETHDMI3			10
#define			APP_CMDLINE_SETHDMI4			11

#define			WOL_NETWORKBROADCAST			1
#define			WOL_IPSEND						2
#define			WOL_SUBNETBROADCAST				3

// common forward function declarations
namespace jpersson77 {
	namespace common {
		std::wstring widen(std::string sInput);
		std::string narrow(std::wstring sInput);
		std::vector<std::string> stringsplit(std::string, std::string);
		std::wstring GetWndText(HWND);
	}
	namespace settings {
		struct DISPLAY_INFO {
			DISPLAYCONFIG_TARGET_DEVICE_NAME target;
			HMONITOR hMonitor;
			HDC hdcMonitor;
			RECT rcMonitor2;
			MONITORINFOEX monitorinfo;
		};
		struct REMOTE_STREAM {
			bool			bRemoteCurrentStatusNvidia = false;
			bool			bRemoteCurrentStatusSteam = false;
			bool			bRemoteCurrentStatusRDP = false;
			bool			bRemoteCurrentStatusSunshine = false;
			std::wstring	sCurrentlyRunningWhitelistedProcess = L"";
			std::string		Sunshine_Log_File = "";
			uintmax_t		Sunshine_Log_Size = 0;

			const std::vector<std::wstring> stream_proc_list{
			L"steam_monitor.exe" //steam server
			};
			const std::vector<std::wstring> stream_usb_list_gamestream{
			L"usb#vid_0955&pid_b4f0" //nvidia
			};
		};
		struct WHITELIST { // whitelist info
			std::wstring Name;
			std::wstring Application;
		};
		struct DEVICE { // WebOS device settings
			std::string DeviceId;
			std::string IP;
			std::vector<std::string> MAC;
			std::string SessionKey;
			std::string Name;
			std::string UniqueDeviceKey;
			std::string UniqueDeviceKey_Temp;
			bool Enabled = true;
			int PowerOnTimeout = 40;
			int WOLtype = WOL_IPSEND;
			std::string Subnet;
			bool HDMIinputcontrol = false;
			int OnlyTurnOffIfCurrentHDMIInputNumberIs = 1;
			bool SetHDMIInputOnResume = false;
			int SetHDMIInputOnResumeToNumber = 1;
			bool SSL = true;

			//service.h
			int BlankScreenWhenIdleDelay = 10;
			bool BlankWhenIdle = false;
		};
		class Preferences {
		public:
			Preferences();
			~Preferences();
			bool Initialize(void);
			void WriteToDisk(void);
			std::vector<std::string> EventLogRestartString = DEFAULT_RESTART;
			std::vector<std::string> EventLogShutdownString = DEFAULT_SHUTDOWN;
			bool Logging = false;
			int version = 2;
			int PowerOnTimeout = 40;
			bool AutoUpdate = false;
			bool ResetAPIkeys = false;
			bool BlankScreenWhenIdle = false;
			int BlankScreenWhenIdleDelay = 10;
			bool AdhereTopology = false;
			bool bIdleWhitelistEnabled = false;
			bool bFullscreenCheckEnabled = false;
			std::vector<WHITELIST> WhiteList;
			bool RemoteStreamingCheck = false;
			bool TopologyPreferPowerEfficiency = true;
			std::wstring DataPath;
			std::vector <DEVICE> Devices;

			// service.h
			bool DisplayIsCurrentlyRequestedPoweredOnByWindows = false;

			//Daemon.h
			bool ToastInitialised = false;
			REMOTE_STREAM Remote;
		};
	}
}