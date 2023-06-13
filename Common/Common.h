#pragma once
#include <windows.h>
#include <fstream>
#include <string>
#include <vector>
#include <Shlobj_core.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

// common general application definitions
#define			APPNAME							L"LGTV Companion"
#define         APP_VERSION                     L"3.0.5"
#define			CONFIG_FILE						L"config.json"
#define			LOG_FILE						L"Log.txt"
#define			WINDOW_CLASS_UNIQUE				L"YOLOx0x0x0181818"
#define         PIPENAME                        TEXT("\\\\.\\pipe\\LGTVyolo")
#define         NEWRELEASELINK                  L"https://github.com/JPersson77/LGTVCompanion/releases"
#define         VERSIONCHECKLINK                L"https://api.github.com/repos/JPersson77/LGTVCompanion/releases"
#define         DONATELINK                      L"https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=jorgen.persson@gmail.com&lc=US&item_name=Friendly+gift+for+the+development+of+LGTV+Companion&no_note=0&cn=&currency_code=EUR&bn=PP-DonationsBF:btn_donateCC_LG.gif:NonHosted"
#define         DISCORDLINK                     L"https://discord.gg/7KkTPrP3fq"
#define			MUTEX_WAIT          		    10   // thread wait in ms

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
#define         JSON_IDLE_FS_EXCLUSIONS_ENABLE	"IdleFsExclusionsEnabled"
#define         JSON_IDLE_FS_EXCLUSIONS			"IdleFsExclusions"
#define         JSON_REMOTESTREAM				"RemoteStream"
#define         JSON_TOPOLOGYMODE				"TopologyPreferPowerEfficient"
#define         JSON_EXTERNAL_API				"ExternalAPI"
#define			JSON_MUTE_SPEAKERS				"MuteSpeakers"
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

// WOL types
#define			WOL_NETWORKBROADCAST			1
#define			WOL_IPSEND						2
#define			WOL_SUBNETBROADCAST				3

//IPC
#define PIPE_BUF_SIZE							1024
#define PIPE_INSTANCES							5
#define PIPE_NOT_RUNNING						99
#define PIPE_EVENT_ERROR						100
#define PIPE_EVENT_READ							101 // Data was read 
#define PIPE_EVENT_SEND							102 // Write data
#define PIPE_RUNNING							103 // Server is running
#define PIPE_EVENT_TERMINATE					120 // Terminate


namespace jpersson77 {
	namespace common {							// Common functions used in all modules
		std::wstring							widen(std::string sInput);
		std::string								narrow(std::wstring sInput);
		std::vector<std::string>				stringsplit(std::string, std::string);
		std::wstring							GetWndText(HWND);
		void									ReplaceAllInPlace(std::string& str, const std::string& from, const std::string& to);
		std::vector <std::string>				GetOwnIP(void);
	}
	namespace settings {
		struct DISPLAY_INFO {					// Display info
			DISPLAYCONFIG_TARGET_DEVICE_NAME	target;
			HMONITOR							hMonitor;
			HDC									hdcMonitor;
			RECT								rcMonitor2;
			MONITORINFOEX						monitorinfo;
		};
		struct REMOTE_STREAM {					// Remote streaming info
			bool								bRemoteCurrentStatusNvidia = false;
			bool								bRemoteCurrentStatusSteam = false;
			bool								bRemoteCurrentStatusRDP = false;
			bool								bRemoteCurrentStatusSunshine = false;
			std::wstring						sCurrentlyRunningWhitelistedProcess = L"";
			std::wstring						sCurrentlyRunningFsExcludedProcess = L"";
			std::string							Sunshine_Log_File = "";
			uintmax_t							Sunshine_Log_Size = 0;

			const std::vector<std::wstring>		stream_proc_list{
			L"steam_monitor.exe"				//steam server
			};
			const std::vector<std::wstring>		stream_usb_list_gamestream{
			L"usb#vid_0955&pid_b4f0"			//nvidia
			};
		};
		struct PROCESSLIST {					// whitelist info
			std::wstring						Name;
			std::wstring						Application;
		};
		struct DEVICE {							// WebOS device settings
			std::string							DeviceId;
			std::string							IP;
			std::vector<std::string>			MAC;
			std::string							SessionKey;
			std::string							Name;
			std::string							UniqueDeviceKey;
			std::string							UniqueDeviceKey_Temp;
			bool								Enabled = true;
			int									PowerOnTimeout = 40;
			int									WOLtype = WOL_IPSEND;
			std::string							Subnet;
			bool								HDMIinputcontrol = false;
			int									OnlyTurnOffIfCurrentHDMIInputNumberIs = 1;
			bool								SetHDMIInputOnResume = false;
			int									SetHDMIInputOnResumeToNumber = 1;
			bool								SSL = true;
		};
		struct PREFERENCES {					// Global preferences and settings
			std::vector<std::string>			EventLogRestartString = DEFAULT_RESTART;
			std::vector<std::string>			EventLogShutdownString = DEFAULT_SHUTDOWN;
			bool								Logging = false;
			int									version = 2;
			int									PowerOnTimeout = 40;
			bool								AutoUpdate = false;
			bool								ResetAPIkeys = false;
			bool								BlankScreenWhenIdle = false;
			int									BlankScreenWhenIdleDelay = 10;
			bool								AdhereTopology = false;
			bool								bIdleWhitelistEnabled = false;
			bool								bFullscreenCheckEnabled = false;
			std::vector<PROCESSLIST>			WhiteList;
			bool								bIdleFsExclusionsEnabled = false;
			std::vector<PROCESSLIST>			FsExclusions;
			bool								RemoteStreamingCheck = false;
			bool								TopologyPreferPowerEfficiency = true;
			bool								ExternalAPI = false;
			bool								MuteSpeakers = false;
			std::wstring						DataPath;
		};
		class Preferences {
		public:
			Preferences();
			~Preferences();
			bool								Initialize(void);
			void								WriteToDisk(void);
			PREFERENCES							Prefs;
			std::vector <DEVICE>				Devices;

			//Daemon.h
			bool								ToastInitialised = false;
			REMOTE_STREAM						Remote;
		};
	}
	namespace ipc
	{
		class PipeServer
		{
		public:
			PipeServer(std::wstring, void (*fcnPtr)(std::wstring));
			~PipeServer(void);
			bool								Send(std::wstring sData, int iPipe = -1);
			bool								Terminate();
			bool								isRunning();
			void								Log(std::wstring);
			std::atomic_int						bLock2 = false;;

		private:
			void								OnEvent(int, std::wstring sData = L"", int iPipe = -1);
			void								WorkerThread();
			bool								DisconnectAndReconnect(int);
			bool								Write(std::wstring&, int);

			HANDLE								hPipeHandles[PIPE_INSTANCES] = {};
			DWORD								dwBytesTransferred = 0;
			TCHAR								Buffer[PIPE_INSTANCES][PIPE_BUF_SIZE + 1] = {};
			OVERLAPPED							Ovlap[PIPE_INSTANCES] = {};
			HANDLE								hEvents[PIPE_INSTANCES + 2] = {}; // include termination and termination confirm events
			std::atomic_bool					bWriteData[PIPE_INSTANCES] = {};
			void								(*FunctionPtr)(std::wstring);
			std::wstring						sPipeName;
			std::atomic_int						iState = PIPE_NOT_RUNNING;

		};
		
		class PipeClient
		{
		public:
			PipeClient(std::wstring, void (*fcnPtr)(std::wstring));
			~PipeClient(void);
			bool								Init();
			bool								Send(std::wstring);
			bool								Terminate();
			bool								isRunning();
			void								Log(std::wstring);
			std::atomic_int						bLock = false;

		private:
			void								OnEvent(int, std::wstring sData = L"");
			void								WorkerThread();
			bool								DisconnectAndReconnect();
			bool								Write(std::wstring&);
			HANDLE								hFile = NULL;
			HANDLE								hPipeHandle = {};
			DWORD								dwBytesTransferred = 0;
			TCHAR								Buffer[PIPE_BUF_SIZE + 1] = {};
			OVERLAPPED							Ovlap;
			HANDLE								hEvents[3] = {}; // overlapped event, terminate event, terminate accept event
			void								(*FunctionPtr)(std::wstring);
			std::wstring						sPipeName;
			std::atomic_int						iState = PIPE_NOT_RUNNING;
			std::atomic_bool					bWriteData = {};

		};
	}
}