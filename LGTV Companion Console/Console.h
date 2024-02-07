#pragma once
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")


#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <WinSock2.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include "../Common/Common.h"
#include "../Common/LgApi.h"
#include <netioapi.h>

#define			APPNAME_SHORT							"LGTVcli"
#define			APPNAME_FULL							"LGTV Companion CLI"

#define			JSON_OUTPUT_DEFAULT						"JSON_OUTPUT_DEFAULT"
#define			JSON_OUTPUT_FRIENDLY					"JSON_OUTPUT_FRIENDLY"
#define			JSON_OUTPUT_FIELD						"JSON_OUTPUT_FIELD"

std::string												ProcessCommand(std::vector<std::string>&);
std::vector<std::string>								_Devices(std::vector<std::string>, int);
std::string												ValidateArgument(std::string, std::string);
//														Create a power event for the session manager. Arg1: Event type, Arg2: vector of the devices 
std::string												CreateEvent_system(DWORD, std::vector<std::string>);
//														Create a URI event with optional payload for the Session Manager. Arg1: vector of the devices, Arg2: URI, Arg3: Json payload (optional). 
std::string												CreateEvent_request(std::vector<std::string>, std::string, std::string = "");
//														Create a basic luna event for the Session Manager. Arg1: vector of the devices, Arg2: Category, Arg3: Setting, Arg4: Value, Arg5: Format of Value.
std::string												CreateEvent_luna_set_system_setting_basic(std::vector<std::string>, std::string, std::string, std::string, std::string);
//														Create a luna event with generic payload for the Session Manager. Arg1: vector of the devices, Arg2: Category, Arg3: Json payload.
std::string												CreateEvent_luna_set_system_setting_payload(std::vector<std::string>, std::string, std::string);
//														Create a luna set device info event for the Session Manager. Arg1: vector of the devices, Arg2: input, Arg3: icon, Arg4: label.
std::string												CreateEvent_luna_set_device_info(std::vector<std::string>, std::string, std::string, std::string);
//														Create a button-press event for the Session Manager. Arg1: vector of the devices, Arg2: button.
std::string												CreateEvent_button(std::vector<std::string>, std::string);
//														Create a generic luna event for the Session Manager. Arg1: vector of the devices, Arg2 : Luna URI, Arg3 : json payload.
std::string												CreateEvent_luna_generic(std::vector<std::string> devices, std::string luna, std::string payload);

std::string												ProcessEvent(EVENT&);
nlohmann::json											CreateRequestJson(std::string, std::string = "");
nlohmann::json											CreateLunaSystemSettingJson(std::string, std::string, std::string, std::string = "");
nlohmann::json											CreateRawLunaJson(std::string, nlohmann::json);
nlohmann::json											PowerOnDevice(jpersson77::settings::DEVICE);
nlohmann::json											SendRequest(jpersson77::settings::DEVICE, nlohmann::json, bool);
nlohmann::json											SendButtonRequest(jpersson77::settings::DEVICE, std::string);
void													iterateJsonKeys(const nlohmann::json&, std::string );
void													Thread_WOL(jpersson77::settings::DEVICE);
std::string												helpText(void);