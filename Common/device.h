#pragma once
#include <string>
#include <vector>


#define         WOL_TYPE_NETWORKBROADCAST           1
#define         WOL_TYPE_IP							2
#define         WOL_TYPE_SUBNETBROADCAST            3
#define			WOL_TYPE_AUTO						4
#define			WOL_DEFAULT_SUBNET					"255.255.255.0"

#define			PERSISTENT_CONNECTION_OFF			0
#define			PERSISTENT_CONNECTION_ON			1
#define			PERSISTENT_CONNECTION_KEEPALIVE		2

// Additional device flags and configuration 
struct DeviceAdditionalConf
{
	bool								user_idle_mode_mute_speakers;
	int									log_level;
	std::string							data_path;
	int									timeout;
};
// Properties, configuration and default values for a webOS device
struct Device {							
	std::string							id;
	std::string							ip;
	std::vector<std::string>			mac_addresses;
	std::string							session_key;
	std::string							name;
	std::string							uniqueDeviceKey;
	std::string							uniqueDeviceKeyTemporary;
	std::string							subnet = WOL_DEFAULT_SUBNET;
	bool								enabled = true;
	int									wake_method = WOL_TYPE_AUTO;
	int									sourceHdmiInput = 1;
	bool								check_hdmi_input_when_power_off = false;
	bool								set_hdmi_input_on_power_on = false;
	int									set_hdmi_input_on_power_on_delay = 1;
	bool								ssl = true;
	int									persistent_connection_level = PERSISTENT_CONNECTION_OFF;
	uint64_t							network_interface_luid = 0;
	DeviceAdditionalConf				extra;
};
