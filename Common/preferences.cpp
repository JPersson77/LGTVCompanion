#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0603
#define _WIN32_WINNT 0x0603
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "common_app_define.h"
#include "tools.h"
#include "preferences.h"
#include <Shlobj_core.h>
#include <nlohmann/json.hpp>
#include <fstream>

using			json = nlohmann::json;

#define         JSON_PREFS_NODE                 "LGTV Companion"
#define         JSON_EVENT_RESTART_STRINGS      "LocalEventLogRestartString"
#define         JSON_EVENT_SHUTDOWN_STRINGS     "LocalEventLogShutdownString"
#define         JSON_VERSION                    "Version"
#define         JSON_LOGGING                    "ExtendedLog"
#define         JSON_LOG_LEVEL                  "LogLevel"
#define         JSON_AUTOUPDATE                 "AutoUpdate"
#define         JSON_PWRONTIMEOUT               "PowerOnTimeOut"
#define         JSON_IDLEBLANK                  "BlankWhenIdle"
#define         JSON_IDLEBLANKDELAY             "BlankWhenIdleDelay"
#define         JSON_ADHERETOPOLOGY             "AdhereDisplayTopology"
#define         JSON_KEEPTOPOLOGYONBOOT			"KeepTopologyOnBoot"
#define         JSON_IDLEWHITELIST				"IdleWhiteListEnabled"
#define         JSON_IDLEFULLSCREEN				"IdleFullscreen"
#define         JSON_WHITELIST					"IdleWhiteList"
#define         JSON_IDLE_FS_EXCLUSIONS_ENABLE	"IdleFsExclusionsEnabled"
#define         JSON_IDLE_FS_EXCLUSIONS			"IdleFsExclusions"
#define         JSON_REMOTESTREAM				"RemoteStream"
#define         JSON_REMOTESTREAM_MODE			"RemoteStreamPowerOff"
#define         JSON_EXTERNAL_API				"ExternalAPI"
#define			JSON_MUTE_SPEAKERS				"MuteSpeakers"
#define			JSON_TIMING_PRESHUTDOWN			"TimingPreshutdown"
#define			JSON_TIMING_SHUTDOWN			"TimingShutdown"
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
#define			JSON_DEVICE_PERSISTENT			"PersistentConnectionLevel"

Preferences::Preferences(std::wstring configuration_file_name)
{
	TCHAR szPath[MAX_PATH];
	nlohmann::json restart_strings;
	try
	{

		std::string json_str =
#include "../Common/restart_strings.h"
			;
		restart_strings = nlohmann::json::parse(json_str);
		// Default shutdown and restart strings
		json node = restart_strings["Restart"];
		if (!node.empty() && node.size() > 0)
		{
			for (auto& str : node.items())
			{
				std::string temp = str.value().get<std::string>();
				event_log_restart_strings_.push_back(temp);
			}
		}
		node = restart_strings["Shutdown"];
		if (!node.empty() && node.size() > 0)
		{
			for (auto& str : node.items())
			{
				std::string temp = str.value().get<std::string>();
				event_log_shutdown_strings_.push_back(temp);
			}
		}
		json lg_api_buttons_json;
		json_str =
#include "../Common/lg_api_commands.h"
			;
		lg_api_commands_json = json::parse(json_str);

		json_str =
#include "../Common/lg_api_buttons.h"
			;
		lg_api_buttons_json = json::parse(json_str);
		json j = lg_api_buttons_json["Buttons"];
		for (auto& str : j.items())
		{
			lg_api_buttons += str.value().get<std::string>();
			lg_api_buttons += " ";
		}	
		if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath)))
		{
			std::wstring path = szPath;

			path += L"\\";
			path += APPNAME;
			path += L"\\";
			CreateDirectory(path.c_str(), NULL);
			data_path_ = tools::narrow(path);
			configuration_file_ = path;
			configuration_file_ += configuration_file_name;

			std::ifstream i(configuration_file_.c_str());
			if (i.is_open())
			{
				nlohmann::json j;
				nlohmann::json jsonPrefs;
				i >> jsonPrefs;
				i.close();
				// Read version of the preferences file. If this key is found it is assumed the config file has been populated
				j = jsonPrefs[JSON_PREFS_NODE][JSON_VERSION];
				if (!j.empty() && j.is_number())
				{
					json_string_ = jsonPrefs.dump(4);
					version_loaded_ = j.get<int>();
					if (version_loaded_ < 3)
					{
						// Logging
						bool logging = false;
							j = jsonPrefs[JSON_PREFS_NODE][JSON_LOGGING];
						if (!j.empty() && j.is_boolean())
							logging = j.get<bool>();
						log_level_ = logging ? 1 : 0;
					}
					else
					{
						// Logging
						j = jsonPrefs[JSON_PREFS_NODE][JSON_LOG_LEVEL];
						if (!j.empty() && j.is_number())
							log_level_ = j.get<int>();
					}
					// Localised shutdown and restart strings
					j = jsonPrefs[JSON_PREFS_NODE][JSON_EVENT_RESTART_STRINGS];
					if (!j.empty() && j.size() > 0)
					{
						for (auto& str : j.items())
						{
							std::string temp = str.value().get<std::string>();
							if (std::find(event_log_restart_strings_.begin(), event_log_restart_strings_.end(), temp) == event_log_restart_strings_.end())
								if (std::find(event_log_shutdown_strings_.begin(), event_log_shutdown_strings_.end(), temp) == event_log_shutdown_strings_.end())
									if (std::find(event_log_restart_strings_custom_.begin(), event_log_restart_strings_custom_.end(), temp) == event_log_restart_strings_custom_.end())
										event_log_restart_strings_custom_.push_back(temp);
						}
					}
					j = jsonPrefs[JSON_PREFS_NODE][JSON_EVENT_SHUTDOWN_STRINGS];
					if (!j.empty() && j.size() > 0)
					{
						for (auto& str : j.items())
						{
							std::string temp = str.value().get<std::string>();
							if (std::find(event_log_shutdown_strings_.begin(), event_log_shutdown_strings_.end(), temp) == event_log_shutdown_strings_.end())
								if (std::find(event_log_restart_strings_.begin(), event_log_restart_strings_.end(), temp) == event_log_restart_strings_.end())
									if (std::find(event_log_shutdown_strings_custom_.begin(), event_log_shutdown_strings_custom_.end(), temp) == event_log_shutdown_strings_custom_.end())
										event_log_shutdown_strings_custom_.push_back(temp);
						}
					}
					// Power On timeout
					j = jsonPrefs[JSON_PREFS_NODE][JSON_PWRONTIMEOUT];
					if (!j.empty() && j.is_number())
						power_on_timeout_ = j.get<int>();
					if (power_on_timeout_ < 5)
						power_on_timeout_ = 5;
					else if (power_on_timeout_ > 100)
						power_on_timeout_ = 100;
					// Update notifications
					j = jsonPrefs[JSON_PREFS_NODE][JSON_AUTOUPDATE];
					if (!j.empty() && j.is_boolean())
						notify_update_ = j.get<bool>();
					// User idle mode
					j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEBLANK];
					if (!j.empty() && j.is_boolean())
						user_idle_mode_ = j.get<bool>();
					// User idle mode delay
					j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEBLANKDELAY];
					if (!j.empty() && j.is_number())
						user_idle_mode_delay_ = j.get<int>();
					if (user_idle_mode_delay_ < 1)
						user_idle_mode_delay_ = 1;
					else if (user_idle_mode_delay_ > 240)
						user_idle_mode_delay_ = 240;
					// Multi-monitor topology support
					j = jsonPrefs[JSON_PREFS_NODE][JSON_ADHERETOPOLOGY];
					if (!j.empty() && j.is_boolean())
						topology_support_ = j.get<bool>();
					// Keep Multi-monitor topology support on boot
					j = jsonPrefs[JSON_PREFS_NODE][JSON_KEEPTOPOLOGYONBOOT];
					if (!j.empty() && j.is_boolean())
						topology_keep_on_boot_ = j.get<bool>();
					// User idle mode whitelist enabled
					j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEWHITELIST];
					if (!j.empty() && j.is_boolean())
						user_idle_mode_whitelist_ = j.get<bool>();
					// User idle mode fullscreen exclusions enabled
					j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLE_FS_EXCLUSIONS_ENABLE];
					if (!j.empty() && j.is_boolean())
						user_idle_mode_exclude_fullscreen_whitelist_ = j.get<bool>();
					// User idle mode, prohibit fullscreen
					j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEFULLSCREEN];
					if (!j.empty() && j.is_boolean())
						user_idle_mode_exclude_fullscreen_ = j.get<bool>();
					// Remote streaming host support
					j = jsonPrefs[JSON_PREFS_NODE][JSON_REMOTESTREAM];
					if (!j.empty() && j.is_boolean())
						remote_streaming_host_support_ = j.get<bool>();
					// Remote streaming power off mode
					j = jsonPrefs[JSON_PREFS_NODE][JSON_REMOTESTREAM_MODE];
					if (!j.empty() && j.is_boolean())
						remote_streaming_host_prefer_power_off_ = j.get<bool>();
					// External API
					j = jsonPrefs[JSON_PREFS_NODE][JSON_EXTERNAL_API];
					if (!j.empty() && j.is_boolean())
						external_api_support_ = j.get<bool>();
					// Mute Speakers
					j = jsonPrefs[JSON_PREFS_NODE][JSON_MUTE_SPEAKERS];
					if (!j.empty() && j.is_boolean())
						user_idle_mode_mute_speakers_ = j.get<bool>();
					// Shutdown timing
					j = jsonPrefs[JSON_PREFS_NODE][JSON_TIMING_PRESHUTDOWN];
					if (!j.empty() && j.is_boolean())
						shutdown_timing_ = j.get<bool>() ? 1 : 0;
					j = jsonPrefs[JSON_PREFS_NODE][JSON_TIMING_SHUTDOWN];
					if (!j.empty() && j.is_number())
						shutdown_timing_ = j.get<int>();
					// User idle mode whitelist
					j = jsonPrefs[JSON_PREFS_NODE][JSON_WHITELIST];
					if (!j.empty() && j.size() > 0)
					{
						for (auto& elem : j.items())
						{
							ProcessList w;
							w.binary = tools::widen(elem.value().get<std::string>());
							w.friendly_name = tools::widen(elem.key());
							user_idle_mode_whitelist_processes_.push_back(w);
						}
					}
					// User idle mode fullscreen exclusions
					j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLE_FS_EXCLUSIONS];
					if (!j.empty() && j.size() > 0)
					{
						for (auto& elem : j.items())
						{
							ProcessList w;
							w.binary = tools::widen(elem.value().get<std::string>());
							w.friendly_name = tools::widen(elem.key());
							user_idle_mode_exclude_fullscreen_whitelist_processes_.push_back(w);
						}
					}
					// initialize the configuration for WebOS devices
					for (const auto& item : jsonPrefs.items())
					{
						if (item.key() == JSON_PREFS_NODE)
							break;
						Device device;
						device.id = item.key();

						if (version_loaded_ < 3)
						{
							device.wake_method = WOL_TYPE_AUTO;
						}
						else
						{
							if (item.value()[JSON_DEVICE_WOLTYPE].is_number())
								device.wake_method = item.value()[JSON_DEVICE_WOLTYPE].get<int>();
							if (device.wake_method < 1)
								device.wake_method = 1;
							else if (device.wake_method > 4)
								device.wake_method = 4;
						}
						if (item.value()[JSON_DEVICE_SUBNET].is_string())
							device.subnet = item.value()[JSON_DEVICE_SUBNET].get<std::string>();

						if (item.value()[JSON_DEVICE_NAME].is_string())
							device.name = item.value()[JSON_DEVICE_NAME].get<std::string>();

						if (item.value()[JSON_DEVICE_IP].is_string())
							device.ip = item.value()[JSON_DEVICE_IP].get<std::string>();

						if (item.value()[JSON_DEVICE_UNIQUEKEY].is_string())
							device.uniqueDeviceKey = item.value()[JSON_DEVICE_UNIQUEKEY].get<std::string>();

						if (item.value()[JSON_DEVICE_HDMICTRL].is_boolean())
							device.input_control_hdmi = item.value()[JSON_DEVICE_HDMICTRL].get<bool>();

						if (item.value()[JSON_DEVICE_HDMICTRLNO].is_number())
							device.input_control_hdmi_number = item.value()[JSON_DEVICE_HDMICTRLNO].get<int>();
						if (device.input_control_hdmi_number < 1)
							device.input_control_hdmi_number = 1;
						else if (device.input_control_hdmi_number > 4)
							device.input_control_hdmi_number = 4;

						if (item.value()[JSON_DEVICE_ENABLED].is_boolean())
							device.enabled = item.value()[JSON_DEVICE_ENABLED].get<bool>();

						if (item.value()[JSON_DEVICE_SESSIONKEY].is_string())
							device.session_key = item.value()[JSON_DEVICE_SESSIONKEY].get<std::string>();

						if (item.value()[JSON_DEVICE_SETHDMI].is_boolean())
							device.set_hdmi_input_on_power_on = item.value()[JSON_DEVICE_SETHDMI].get<bool>();

						if (item.value()[JSON_DEVICE_NEWSOCK].is_boolean())
							device.ssl = item.value()[JSON_DEVICE_NEWSOCK].get<bool>();

						if (item.value()[JSON_DEVICE_PERSISTENT].is_number())
							device.persistent_connection_level = item.value()[JSON_DEVICE_PERSISTENT].get<int>();

						if (item.value()[JSON_DEVICE_SETHDMINO].is_number())
							device.set_hdmi_input_on_power_on_number = item.value()[JSON_DEVICE_SETHDMINO].get<int>();
						if (device.set_hdmi_input_on_power_on_number < 1)
							device.set_hdmi_input_on_power_on_number = 1;
						else if (device.set_hdmi_input_on_power_on_number > 4)
							device.set_hdmi_input_on_power_on_number = 4;

						j = item.value()[JSON_DEVICE_MAC];
						if (!j.empty() && j.size() > 0)
						{
							for (auto& m : j.items())
							{
								device.mac_addresses.push_back(m.value().get<std::string>());
							}
						}
						device.extra.data_path = data_path_;
						device.extra.timeout = power_on_timeout_;
						device.extra.log_level = log_level_;
						device.extra.user_idle_mode_mute_speakers = user_idle_mode_mute_speakers_;
						devices_.push_back(device);
					}
				}
			}
		}
		initialised_ = true;
	}
	catch (std::exception const&)
	{
		initialised_ = false;
	}
}
bool Preferences::isInitialised(void)
{
	return initialised_;
}
std::string Preferences::getAsString(void)
{
	return json_string_;
}
void Preferences::resetSessionKeys(bool reset)
{
	resetSessionKeys_ = reset;
}
bool Preferences::Preferences::writeToDisk(void)
{
	nlohmann::json prefs, p;
	CreateDirectory(tools::widen(data_path_).c_str(), NULL);
	
	// do we need to upgrade the api key version
	if (resetSessionKeys_)
	{
		for (auto& device : devices_)
		{
			device.session_key = "";
		}
		resetSessionKeys_ = false;
	}
	else //load sessionkeys from config.json and add it to the device list
	{
		try
		{
			std::ifstream i(configuration_file_.c_str());
			if (i.is_open())
			{
				i >> p;
				i.close();

				for (const auto& item : p.items())
				{
					if (item.key() == JSON_PREFS_NODE)
						break;
					nlohmann::json j;
					std::string key = "";
					if (item.value()[JSON_DEVICE_SESSIONKEY].is_string())
						key = item.value()[JSON_DEVICE_SESSIONKEY].get<std::string>();
					j = item.value()[JSON_DEVICE_MAC];
					if (!j.empty() && j.size() > 0)
						for (auto& item2 : j.items())
							for (auto& device : devices_)
								for (auto& mac : device.mac_addresses)
									if (mac == item2.value().get<std::string>())
										device.session_key = key;
				}
			}
		}
		catch (std::exception const&)
		{
			return false;
		}
	}
	prefs[JSON_PREFS_NODE][JSON_VERSION] = (int)version_;
	prefs[JSON_PREFS_NODE][JSON_PWRONTIMEOUT] = (int)power_on_timeout_;
	prefs[JSON_PREFS_NODE][JSON_LOG_LEVEL] = (int)log_level_;
	prefs[JSON_PREFS_NODE][JSON_AUTOUPDATE] = (bool)notify_update_;
	prefs[JSON_PREFS_NODE][JSON_IDLEBLANK] = (bool)user_idle_mode_;
	prefs[JSON_PREFS_NODE][JSON_IDLEBLANKDELAY] = (int)user_idle_mode_delay_;
	prefs[JSON_PREFS_NODE][JSON_ADHERETOPOLOGY] = (bool)topology_support_;
	prefs[JSON_PREFS_NODE][JSON_KEEPTOPOLOGYONBOOT] = (bool)topology_keep_on_boot_;
	prefs[JSON_PREFS_NODE][JSON_IDLEWHITELIST] = (bool) user_idle_mode_whitelist_;
	prefs[JSON_PREFS_NODE][JSON_IDLEFULLSCREEN] = (bool) user_idle_mode_exclude_fullscreen_;
	prefs[JSON_PREFS_NODE][JSON_IDLE_FS_EXCLUSIONS_ENABLE] = (bool) user_idle_mode_exclude_fullscreen_whitelist_;
	prefs[JSON_PREFS_NODE][JSON_REMOTESTREAM] = (bool)remote_streaming_host_support_;
	prefs[JSON_PREFS_NODE][JSON_REMOTESTREAM_MODE] = (bool)remote_streaming_host_prefer_power_off_;
	prefs[JSON_PREFS_NODE][JSON_EXTERNAL_API] = (bool)external_api_support_;
	prefs[JSON_PREFS_NODE][JSON_MUTE_SPEAKERS] = (bool)user_idle_mode_mute_speakers_;
	prefs[JSON_PREFS_NODE][JSON_TIMING_SHUTDOWN] = (int)shutdown_timing_;
	
	if (event_log_restart_strings_custom_.size() > 0)
		for (auto& item : event_log_restart_strings_custom_)
			prefs[JSON_PREFS_NODE][JSON_EVENT_RESTART_STRINGS].push_back(item);
	if (event_log_shutdown_strings_custom_.size() > 0)
		for (auto& item : event_log_shutdown_strings_custom_)
			prefs[JSON_PREFS_NODE][JSON_EVENT_SHUTDOWN_STRINGS].push_back(item);
	if (user_idle_mode_whitelist_processes_.size() > 0)
		for (auto& w : user_idle_mode_whitelist_processes_)
			prefs[JSON_PREFS_NODE][JSON_WHITELIST][tools::narrow(w.friendly_name)] = tools::narrow(w.binary);
	if (user_idle_mode_exclude_fullscreen_whitelist_processes_.size() > 0)
		for (auto& w : user_idle_mode_exclude_fullscreen_whitelist_processes_)
			prefs[JSON_PREFS_NODE][JSON_IDLE_FS_EXCLUSIONS][tools::narrow(w.friendly_name)] = tools::narrow(w.binary);

	//Iterate devices
	int deviceid = 1;
	for (auto& item : devices_)
	{
		std::string id = "Device";
		id += std::to_string(deviceid);
		item.id = id;
		//		prefs[dev.str()][JSON_DEVICE_NAME] = item.Name;
		if (item.name != "")
			prefs[id][JSON_DEVICE_NAME] = item.name;
		if (item.ip != "")
			prefs[id][JSON_DEVICE_IP] = item.ip;
		if (item.session_key != "")
			prefs[id][JSON_DEVICE_SESSIONKEY] = item.session_key;
		else
			prefs[id][JSON_DEVICE_SESSIONKEY] = "";
		if (item.uniqueDeviceKey != "")
			prefs[id][JSON_DEVICE_UNIQUEKEY] = item.uniqueDeviceKey;

		prefs[id][JSON_DEVICE_HDMICTRL] = (bool)item.input_control_hdmi;
		prefs[id][JSON_DEVICE_HDMICTRLNO] = item.input_control_hdmi_number;

		prefs[id][JSON_DEVICE_SETHDMI] = (bool)item.set_hdmi_input_on_power_on;
		prefs[id][JSON_DEVICE_SETHDMINO] = item.set_hdmi_input_on_power_on_number;

		prefs[id][JSON_DEVICE_NEWSOCK] = (bool)item.ssl;

		if (item.subnet != "")
			prefs[id][JSON_DEVICE_SUBNET] = item.subnet;

		prefs[id][JSON_DEVICE_WOLTYPE] = item.wake_method;
		prefs[id][JSON_DEVICE_PERSISTENT] = (int)item.persistent_connection_level;
		prefs[id][JSON_DEVICE_ENABLED] = (bool)item.enabled;

		for (auto& m : item.mac_addresses)
			prefs[id][JSON_DEVICE_MAC].push_back(m);

		deviceid++;
	}

	if (!prefs.empty())
	{
		std::ofstream i(configuration_file_.c_str());
		if (i.is_open())
		{
			i << std::setw(4) << prefs << std::endl;
			i.close();
		}
	}
	return true;
}