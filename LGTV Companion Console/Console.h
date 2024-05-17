#pragma once
#include <Windows.h>
#include <string>
#include <nlohmann/json.hpp>
#include "../Common/device.h"

struct EVENT {
	DWORD												dwType = NULL;
	std::string											request_uri;
	std::string											request_payload_json;
	std::string											luna_system_setting_category;
	std::string											luna_system_setting_setting;
	std::string											luna_system_setting_value;
	std::string											luna_system_setting_value_format;
	std::string											luna_payload_json;
	std::string											luna_device_info_input;
	std::string											luna_device_info_icon;
	std::string											luna_device_info_label;
	std::string											button;
	std::vector<std::string>							devices;
	std::string											log_message;
	bool												repeat = false;
	bool												ScreenSaverActiveWhileDimmed = false;
};

std::string												ProcessCommand(std::vector<std::string>&);
std::vector<std::string>								_Devices(std::vector<std::string>, int);
std::string												ValidateArgument(std::string, std::string);

std::string												CreateEvent_system(DWORD, std::vector<std::string>);
std::string												CreateEvent_request(std::vector<std::string>, std::string, std::string = "");
std::string												CreateEvent_luna_set_system_setting_basic(std::vector<std::string>, std::string, std::string, std::string, std::string);
std::string												CreateEvent_luna_set_system_setting_payload(std::vector<std::string>, std::string, std::string);
std::string												CreateEvent_luna_set_device_info(std::vector<std::string>, std::string, std::string, std::string);
std::string												CreateEvent_button(std::vector<std::string>, std::string);
std::string												CreateEvent_luna_generic(std::vector<std::string> devices, std::string luna, std::string payload);

std::string												ProcessEvent(EVENT&);
nlohmann::json											CreateRequestJson(std::string, std::string = "");
nlohmann::json											CreateLunaSystemSettingJson(std::string, std::string, std::string, std::string = "");
nlohmann::json											CreateRawLunaJson(std::string, nlohmann::json);
nlohmann::json											PowerOnDevice(Device);
nlohmann::json											SendRequest(Device, nlohmann::json, bool);
nlohmann::json											SendButtonRequest(Device, std::string);
void													iterateJsonKeys(const nlohmann::json&, std::string );
void													Thread_WOL(Device);
std::string												helpText(void);