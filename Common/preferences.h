#pragma once
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../Common/device.h"

#define PREFS_SHUTDOWN_TIMING_DEFAULT		0
#define PREFS_SHUTDOWN_TIMING_EARLY			1
#define PREFS_SHUTDOWN_TIMING_DELAYED		2

#define PREFS_UPDATER_OFF					0
#define PREFS_UPDATER_NOTIFY				1
#define PREFS_UPDATER_SILENT				2

// Preferences management
class Preferences : public std::enable_shared_from_this<Preferences>
{
private: 
	std::wstring						configuration_file_;
	bool								initialised_ = false;
	bool								resetSessionKeys_ = false;
	std::string							json_string_;

public:
	struct ProcessList {
		std::wstring					binary;
		std::wstring					friendly_name;
		bool							process_control_disable_while_running = false;
		bool							process_control_disable_while_running_foreground = false;
		bool							process_control_disable_while_running_fullscreen = false;
		bool							process_control_disable_while_running_display_lock = false;
	};

	std::vector<std::string>			event_log_restart_strings_custom_; 
	std::vector<std::string>			event_log_shutdown_strings_custom_; 
	std::vector<std::string>			event_log_restart_strings_;
	std::vector<std::string>			event_log_shutdown_strings_;
	int									log_level_ = 0;
	int									version_ = 3;
	int									version_loaded_ = 0;
	int									power_on_timeout_ = 40;
	int									updater_mode_ = PREFS_UPDATER_OFF;
	bool								user_idle_mode_ = false;
	int									user_idle_mode_delay_ = 10;
	bool								user_idle_mode_mute_speakers_ = false;
/*
	bool								user_idle_mode_whitelist_ = false;
	std::vector<ProcessList>			user_idle_mode_whitelist_processes_;
	bool								user_idle_mode_exclude_fullscreen_ = false;
	bool								user_idle_mode_exclude_fullscreen_whitelist_ = false;
	std::vector<ProcessList>			user_idle_mode_exclude_fullscreen_whitelist_processes_;
*/
	bool								user_idle_mode_disable_while_fullscreen_ = false;
	bool								user_idle_mode_disable_while_video_wake_lock_ = false;
	bool								user_idle_mode_disable_while_video_wake_lock_foreground_ = false;
	bool								user_idle_mode_disable_while_video_wake_lock_fullscreen_ = false;
	bool								user_idle_mode_process_control_ = false;
	std::vector<ProcessList>			user_idle_mode_process_control_list_;
	bool								topology_support_ = false;
	bool								topology_keep_on_boot_ = false;
	bool								remote_streaming_host_support_ = false;
	bool								remote_streaming_host_prefer_power_off_ = true;
	bool								external_api_support_ = false;
	int									shutdown_timing_ = PREFS_SHUTDOWN_TIMING_DEFAULT;
	std::string							data_path_;
	std::vector<Device>					devices_;
	nlohmann::json						lg_api_commands_json;
	std::string							lg_api_buttons;

	bool								writeToDisk(void);
	std::string							getAsString(void);
	void								resetSessionKeys(bool);
	bool								isInitialised(void);
	Preferences(std::wstring configuration_file_name);
	~Preferences() {};
};

