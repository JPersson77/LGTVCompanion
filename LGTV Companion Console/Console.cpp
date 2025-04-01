#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "console.h"
#include "../Common/lg_api.h"
#include "../Common/ipc.h"
#include "../Common/preferences.h"
#include "../Common/common_app_define.h"
#include "../Common/tools.h"
#include "../Common/event.h"
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
#include <netioapi.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#define			APPNAME_SHORT							"LGTVcli"
#define			APPNAME_FULL							"LGTV Companion CLI"

// connection port
#define			PORT									"3000"
#define			PORT_SSL								"3001"

#define			JSON_OUTPUT_DEFAULT						"JSON_OUTPUT_DEFAULT"
#define			JSON_OUTPUT_FRIENDLY					"JSON_OUTPUT_FRIENDLY"
#define			JSON_OUTPUT_FIELD						"JSON_OUTPUT_FIELD"

namespace						beast = boost::beast;					// from <boost/beast.hpp>
namespace						http = beast::http;						// from <boost/beast/http.hpp>
namespace						websocket = beast::websocket;			// from <boost/beast/websocket.hpp>
namespace						net = boost::asio;						// from <boost/asio.hpp>
using							tcp = boost::asio::ip::tcp;				// from <boost/asio/ip/tcp.hpp>
namespace						ssl = boost::asio::ssl;					// from <boost/asio/ssl.hpp>


Preferences						Prefs(CONFIG_FILE);
std::atomic_bool				bTerminateThread = { false };
WSADATA							WSAData;
bool							bOutputContainsManyResults = false;

int main(int argc, char* argv[])
{
	std::vector<std::vector<std::string>> CmdLine;
	nlohmann::json err;

	// Output some info if no arguments
	if (argc == 1)
	{
		std::cout << APPNAME_FULL << " v" << tools::narrow(APP_VERSION) << "\n\nUsage is "<< argv[0] << " [-command] [[Argument 1] ... [Argument X]] [[Device1|Name]...[DeviceX|Name]]\n\n";
		std::cout << "Use -help for an overview of commands, or for the full documentation please visit:\nhttps://github.com/JPersson77/LGTVCompanion/blob/master/Docs/Commandline.md" << std::endl;
		return 0;
	}
	// check that first argument is a command (prefixed with hyphen)
	if (argv[1][0] != '-')
	{
		err["error"] = "Invalid command line format.Please prefix command with '-'.";
		std::cout << err.dump() << std::endl;
		return 0;
	}
	// starting up winsock
	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
	{
		err["error"] = "WSAStartup error.";
		std::cout << err.dump() << std::endl;
		return 0;
	}
	// push all arguments into a vector of string vectors
	std::vector<std::string> words;
	for (int i = 1; i < argc; i++)
	{
		if(argv[i][0] == '-')
		{
			if (words.size() > 0)
			{
				CmdLine.push_back(words);
				words.clear();
			}
		}
		words.push_back(argv[i]);
		if (i >= argc - 1)
		{
			CmdLine.push_back(words);
		}
	}
	
	// load the configuration
	if(!Prefs.isInitialised())
	{ 
		err["error"] = "Could not load the configuration file.";
		std::cout << err.dump() << std::endl;
		return 0;
	}

	// check that devices are configured
	if (Prefs.devices_.size() == 0)
	{
		err["error"] = "No devices configured. Please start LGTV Companion UI to configure your devices.";
		std::cout << err.dump() << std::endl;
		return 0;
	}
	SetThreadExecutionState(ES_AWAYMODE_REQUIRED | ES_CONTINUOUS);
	std::string json_output_type = JSON_OUTPUT_DEFAULT;
	std::string field;
	for (auto& words : CmdLine)
	{
		
		std::string result = ProcessCommand(words);

		// SET OUTPUT FORMATTING
		if (result == JSON_OUTPUT_DEFAULT)
			json_output_type = JSON_OUTPUT_DEFAULT;
		else if (result == JSON_OUTPUT_FRIENDLY)
			json_output_type = JSON_OUTPUT_FRIENDLY;
		else if (result.find(JSON_OUTPUT_FIELD) != std::string::npos)
		{
			std::vector<std::string> r = tools::stringsplit(result, " ");
			if (r.size() > 1)
			{
				field = r[1];
				json_output_type = JSON_OUTPUT_FIELD;
			}
		}
		else
		{
			// OUTPUT THE RESULT
			if (json_output_type == JSON_OUTPUT_DEFAULT)
				std::cout << result;
			else if (json_output_type == JSON_OUTPUT_FRIENDLY)
			{
				try
				{
					std::cout << std::setw(4) << nlohmann::json::parse(result);
				}
				catch (std::exception)
				{
					std::cout << result;
				}
			}
			else if (json_output_type == JSON_OUTPUT_FIELD)
			{
				if (field != "")
				{
					try
					{
						bOutputContainsManyResults = false;
						iterateJsonKeys(nlohmann::json::parse(result), field);
					}
					catch (std::exception)
					{
						err["error"] = "Unknown error when iterating JSON. ";
						std::cout << err.dump();
					}
				}
				else
				{
					err["error"] = "Invalid field. ";
					std::cout << err.dump();
				}
			}
			else
			{
				err["error"] = "Unknown output type. ";
				std::cout << err.dump();
			}
			std::cout << std::endl;
		}

	}
	SetThreadExecutionState(ES_CONTINUOUS);
	WSACleanup();
	return 0;
}
void iterateJsonKeys(const nlohmann::json& json_obj, std::string key_value) 
{
	if (json_obj.is_object()) {
		for (auto it = json_obj.begin(); it != json_obj.end(); ++it) {
			const auto& key = it.key();
			const auto& value = it.value();

			if (value.is_string() && key == key_value)
			{
				if (!bOutputContainsManyResults)
				{
					std::cout << std::string(value);
					bOutputContainsManyResults = true;
				}
				else
				{
					std::cout << std::endl << std::string(value);
				}
			}

			// Recursively iterate over nested objects
			if (value.is_object() || value.is_array()) {
				iterateJsonKeys(value, key_value);
			}
		}
	}
	else if (json_obj.is_array()) {
		for (const auto& element : json_obj) {
			// Recursively iterate over nested objects in an array
			if (element.is_object() || element.is_array()) {
				iterateJsonKeys(element, key_value);
			}
		}
	}
}
std::string ProcessCommand(std::vector<std::string>& words)
{
	int iError = 0;
	size_t nWords = words.size();
	std::string command = words[0].substr(1);
	std:: string error;
	transform(command.begin(), command.end(), command.begin(), ::tolower);
	if (command == "help" || command == "?" || command == "h")	
		return helpText();
	else if (command == "output" )													// OUTPUT FORMATTING
	{
		if (nWords > 1)
		{
			std::string arg = words[1];
			transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
			if (arg == "friendly")
				return JSON_OUTPUT_FRIENDLY;
			else if (arg == "default")
				return JSON_OUTPUT_DEFAULT;
			else if (arg == "key")
			{
				if(nWords > 2)
				{
					std::string type = JSON_OUTPUT_FIELD;
					type += " ";
					type += words[2];
					return type;
				}
				else
					iError = 1;
			}
			else
			{
				iError = 3;
				error = "default friendly key";
			}
		}
		else
			iError = 1;
	}
	else if (command == "of")
		return JSON_OUTPUT_FRIENDLY;
	else if (command == "od")
		return JSON_OUTPUT_DEFAULT;
	else if (command == "ok")
	{
		if (nWords > 1)
		{
			std::string type = JSON_OUTPUT_FIELD;
			type += " ";
			type += words[1];
			return type;
		}
		else
			iError = 1;
	}
	else if (command == "poweron")												// POWER ON
		return CreateEvent_system(EVENT_FORCE_DISPLAYON, _Devices(words, 1));
	else if (command == "poweroff")												// POWER OFF
		return CreateEvent_system(EVENT_FORCE_DISPLAYOFF, _Devices(words, 1));
	else if (command == "screenon")												// UNBLANK SCREEN
		return CreateEvent_system(EVENT_FORCE_DISPLAYON, _Devices(words, 1));
	else if (command == "screenoff")											// BLANK SCREEN
		return CreateEvent_system(EVENT_FORCE_BLANKSCREEN, _Devices(words, 1));
	else if (command.find("sethdmi") == 0)										// SET HDMI INPUT
	{
		int cmd_offset = 1;
		std::string argument;
		if (command.size() > 7)					// -sethdmiX ...
		{
			argument = command.substr(7, 1);
			cmd_offset = 1;
		}
		else if (nWords > 1)					// -sethdmi X ...
		{
			argument = words[1].substr(0, 1);
			cmd_offset = 2;
		}
		if (argument != "")
		{
			argument = ValidateArgument(argument, "1 2 3 4");
			if (argument != "")
			{
				std::string payload = LG_URI_PAYLOAD_SETHDMI;
				tools::replaceAllInPlace(payload, "#ARG#", argument);
				return CreateEvent_request(_Devices(words, cmd_offset), LG_URI_LAUNCH, payload);
			}
			iError = 3;
			error = "1 2 3 4";
		}
		else
			iError = 1;
	}
	else if (command == "volume")										// SET SOUND VOLUME
	{
		if (nWords > 1)
		{
			int arg = atoi(words[1].c_str());
			if (arg < 0)
				arg = 0;
			if (arg > 100)
				arg = 100;
			nlohmann::json payload;
			payload["volume"] = arg;
			return CreateEvent_request(_Devices(words, 2), LG_URI_SETVOLUME, payload.dump());
		}
		else
			iError = 1;
	}
	else if (command == "mute")													// MUTE DEVICE SOUND
		return CreateEvent_request(_Devices(words, 1), LG_URI_SETMUTE, "{\"mute\":\"true\"}");
	else if (command == "unmute")												// UNMUTE DEVICE SOUND
		return CreateEvent_request(_Devices(words, 1), LG_URI_SETMUTE, "");
	else if (command == "servicemenu")									// SHOW SERVICE MENU
	{
		std::vector<std::string> devices = _Devices(words, 1);
		std::vector<std::string> newCmdLine;
		newCmdLine.push_back("-button");
		newCmdLine.push_back("IN_START");
		newCmdLine.insert(std::end(newCmdLine), std::begin(devices), std::end(devices));
		return ProcessCommand(newCmdLine);
	}
	else if (command == "servicemenu_legacy_enable" || command == "servicemenu_legacy_disable")					// ENABLE FULL/LEGACY SERVICE MENU
	{
		std::vector<std::string> devices = _Devices(words, 1);
		std::vector<std::string> newCmdLine;
		newCmdLine.push_back("-settings_other");
		if (command == "servicemenu_legacy_enable")
			newCmdLine.push_back("{\"svcMenuFlag\":false}");
		else
			newCmdLine.push_back("{\"svcMenuFlag\":true}");
		newCmdLine.insert(std::end(newCmdLine), std::begin(devices), std::end(devices));
		return ProcessCommand(newCmdLine);
	}
	else if (command == "servicemenu_tpc_enable" || command == "servicemenu_tpc_disable")					// ENABLE FULL/LEGACY SERVICE MENU
	{
		nlohmann::json payload;
		payload["enable"] = command == "servicemenu_tpc_enable" ? true : false;
		return CreateEvent_luna_generic(_Devices(words, 1), LG_LUNA_SET_TPC, payload.dump());
	}
	else if (command == "servicemenu_gsr_enable" || command == "servicemenu_gsr_disable")					// ENABLE FULL/LEGACY SERVICE MENU
	{
		nlohmann::json payload;
		payload["enable"] = command == "servicemenu_gsr_enable" ? true : false;
		return CreateEvent_luna_generic(_Devices(words, 1), LG_LUNA_SET_GSR, payload.dump());
	}
	else if (command == "freesyncinfo")											// SHOW FREESYNC INFORMATION
	{
		std::vector<std::string> devices = _Devices(words, 1);
		std::vector<std::string> newCmdLine;
		newCmdLine.push_back("-start_app_with_param");
		newCmdLine.push_back("com.webos.app.tvhotkey");
		newCmdLine.push_back("{\"activateType\":\"freesync-info\"}");
		newCmdLine.insert(std::end(newCmdLine), std::begin(devices), std::end(devices));
		return ProcessCommand(newCmdLine);
	}
	else if (command == "button")												// BUTTON PRESS
	{
		if (nWords > 1)
		{
			std::string validated_button = ValidateArgument(words[1], Prefs.lg_api_buttons);
			if (validated_button != "")
				return CreateEvent_button(_Devices(words, 2), validated_button);
			iError = 3;
			error = Prefs.lg_api_buttons;
		}
		else
			iError = 1;
	}
	else if (command == "button_nocheck")												// BUTTON PRESS
	{
		if (nWords > 1)
		{
			std::string non_validated_button = words[1];
			return CreateEvent_button(_Devices(words, 2), non_validated_button);
		}
		else
			iError = 1;
	}
	else if (command == "settings_picture")										// LUNA PICTURE SETTINGS
	{
		std::string parsed_json;
		if (nWords > 1)
		{
			try
			{
				parsed_json = nlohmann::json::parse(words[1]).dump();
				return CreateEvent_luna_set_system_setting_payload(_Devices(words, 2), "picture", parsed_json);
			}
			catch (std::exception const& e)
			{
				iError = 2;
				error = e.what();
			}
		}
		else
			iError = 1;
	}
	else if (command == "settings_other")										// LUNA OTHER SETTINGS
	{
		std::string parsed_json;
		if (nWords > 1)
		{
			try
			{
				parsed_json = nlohmann::json::parse(words[1]).dump();
				return CreateEvent_luna_set_system_setting_payload(_Devices(words, 2), "other", parsed_json);
			}
			catch (std::exception const& e)
			{
				iError = 2;
				error = e.what();
			}
		}
		else
			iError = 1;
	}
	else if (command == "settings_options")										// LUNA OPTIONS SETTINGS
	{
		std::string parsed_json;
		if (nWords > 1)
		{
			try
			{
				parsed_json = nlohmann::json::parse(words[1]).dump();
				return CreateEvent_luna_set_system_setting_payload(_Devices(words, 2), "option", parsed_json);
			}
			catch (std::exception const& e)
			{
				iError = 2;
				error = e.what();
			}
		}
		else
			iError = 1;
	}
	else if (command == "request")												// GENERIC REQUEST
	{
		if (nWords > 1)
			return CreateEvent_request(_Devices(words, 2), words[1], "");
		iError = 1;
	}
	else if (command == "request_with_param")									// GENERIC REQUEST WITH PARAM
	{
		std::string parsed_json;
		if (nWords > 2)
		{
			try
			{
				parsed_json = nlohmann::json::parse(words[2]).dump();
				return CreateEvent_request(_Devices(words, 3), words[1], parsed_json);
			}
			catch (std::exception const& e)
			{
				iError = 2;
				error = e.what();
			}
		}
		else
			iError = 1;
	}
	else if (command == "start_app")											// START APP
	{
		if (nWords > 1)
		{
			nlohmann::json j;
			j["id"] = words[1];
			return CreateEvent_request(_Devices(words, 2), LG_URI_LAUNCH, j.dump());
		}
		iError = 1;
	}
	else if (command == "start_app_with_param")									// START APP WITH PARAM
	{
		std::string parsed_json;
		if (nWords > 2)
		{
			try
			{
				nlohmann::json j;
				j["id"] = words[1];
				j["params"] = nlohmann::json::parse(words[2]);
				parsed_json = j.dump();
				return CreateEvent_request(_Devices(words, 3), LG_URI_LAUNCH, parsed_json);
			}
			catch (std::exception const& e)
			{
				iError = 2;
				error = e.what();
			}
		}
		else
			iError = 1;
	}
	else if (command == "close_app")											// CLOSE APP
	{
		if (nWords > 1)
		{
			nlohmann::json j;
			j["id"] = words[1];
			return CreateEvent_request(_Devices(words, 2), LG_URI_CLOSE, j.dump());
		}
		iError = 1;
	}
	else if (command == "set_input_type")										// LUNA SET HDMI INPUT LABEL
	{
		if (nWords > 3)
			return CreateEvent_luna_set_device_info(_Devices(words, 4), words[1], words[2], words[3]);
		iError = 1;
	}
	else if (command == "set_curve_preset")										// LUNA SET FLEX CURVE PRESET
	{
		if (nWords > 1)
		{
			std::string preset;
			if (words[1] == "0" || words[1] == "flat" || words[1] == "Flat") preset = "flat";
			else if (words[1] == "1" || words[1] == "2" || words[1] == "3")
			{
				preset = "curvature";
				preset += words[1];
			}
			if (preset != "")
			{
				nlohmann::json payload;
				payload["type"] = preset;
				payload["reason"] = "com.pal.app.settings";
				return CreateEvent_luna_generic(_Devices(words, 2), LG_LUNA_SET_CURVE_PRESET, payload.dump());
			}
			iError = 3;
			error = "Flat 1 2 3";
		}
		else
			iError = 1;
	}
	else if (command == "adjust_curve_preset")									// LUNA ADJUST PRESET CURVATURE
	{
		if (nWords > 2)
		{
			std::string preset;
			if (words[1] == "1" || words[1] == "2" || words[1] == "3")
			{
				preset = "curvature";
				preset += words[1];
			}
			if (preset != "")
			{
				int curve = atoi(words[2].c_str());
				if (curve > 100) curve = 100;
				else if (curve < 0) curve = 0;
				nlohmann::json payload;
				std::string argument;
				argument = std::to_string(curve);
				argument += "%";
				payload["type"] = preset;
				payload["value"] = argument;
				return CreateEvent_luna_generic(_Devices(words, 3), LG_LUNA_ADJUST_CURVE_PRESET, payload.dump());
			}
			iError = 3;
			error = "[1,2,3] [0-100]";
		}
		else
			iError = 1;
	}
	else if (command == "set_curvature")										// LUNA SET CURVATURE
	{
		if (nWords > 1)
		{
			
			std::string argument;
			if (words[1] == "flat" || words[1] == "Flat")
				argument = "flat";
			else
			{
				int curve = atoi(words[1].c_str());
				if (curve > 100) curve = 100;
				else if (curve < 0) curve = 0;
				argument = std::to_string(curve);
			}
			std::vector<std::string> devices = _Devices(words, 2);
			std::vector<std::string> newCmdLine;

			if (argument == "flat" || argument == "0")
			{
				newCmdLine.clear();
				newCmdLine.push_back("-set_curve_preset");
				newCmdLine.push_back("flat");
				newCmdLine.insert(std::end(newCmdLine), std::begin(devices), std::end(devices));
				return ProcessCommand(newCmdLine);
			}
			else
			{
				newCmdLine.push_back("-adjust_curve_preset");
				newCmdLine.push_back("2");
				newCmdLine.push_back(argument);
				newCmdLine.insert(std::end(newCmdLine), std::begin(devices), std::end(devices));
				ProcessCommand(newCmdLine);

				newCmdLine.clear();
				newCmdLine.push_back("-adjust_curve_preset");
				newCmdLine.push_back("3");
				newCmdLine.push_back(argument);
				newCmdLine.insert(std::end(newCmdLine), std::begin(devices), std::end(devices));
				ProcessCommand(newCmdLine);

				newCmdLine.clear();
				newCmdLine.push_back("-set_curve_preset");
				newCmdLine.push_back("3");
				newCmdLine.insert(std::end(newCmdLine), std::begin(devices), std::end(devices));
				ProcessCommand(newCmdLine);

				newCmdLine.clear();
				newCmdLine.push_back("-set_curve_preset");
				newCmdLine.push_back("2");
				newCmdLine.insert(std::end(newCmdLine), std::begin(devices), std::end(devices));
				return ProcessCommand(newCmdLine);
			}
		}
		else
			iError = 1;
	}
	else if (command == "get_system_settings")									// LUNA GET SYSTEM SETTINGS
	{
		nlohmann::json payload;
		if (nWords > 2)
		{
			try
			{
				payload["category"] = words[1];
				payload["keys"] = nlohmann::json::parse(words[2]);
				return CreateEvent_request(_Devices(words, 3), LG_URI_GET_SYSTEM_SETTINGS, payload.dump());
			}
			catch (std::exception const& e)
			{
				iError = 2;
				error = e.what();
			}
		}
		else
			iError = 1;
	}
	else
	{
		bool found = false;
		for (const auto& item : Prefs.lg_api_commands_json.items())
		{
			if (item.key() == command) 
			{
				found = true;
				if (nWords > 1)
				{
					std::string category, setting, argument, arguments, format;
					int max = -1;
					int min = -1;
					if (item.value()["Category"].is_string())
						category = item.value()["Category"].get<std::string>();
					if (item.value()["Setting"].is_string())
						setting = item.value()["Setting"].get<std::string>();
					if (item.value()["Argument"].is_string())
						arguments = item.value()["Argument"].get<std::string>();
					if (item.value()["Max"].is_number())
						max = item.value()["Max"].get<int>();
					if (item.value()["Min"].is_number())
						min = item.value()["Min"].get<int>();
					if (item.value()["ValFormat"].is_string())
						format = item.value()["ValFormat"].get<std::string>();

					if (min != -1)
					{
						int arg = atoi(words[1].c_str());
						if (arg < min)
							arg = min;
						if (arg > max)
							arg = max;
						argument = std::to_string(arg);
					}
					else
					{
						argument = ValidateArgument(words[1], arguments);
					}

					if (argument != "")
					{
						std::vector<std::string> devices = _Devices(words, 2);
						size_t hdmi_type_command = setting.find("_hdmi");
						if (hdmi_type_command != std::string::npos)
						{
							std::string payload;
							std::string command_ex = setting.substr(0, hdmi_type_command);
							std::string hdmi_input = setting.substr(hdmi_type_command + 5, 1);
							if (format == "int")
								payload = "{\"#CMD#\":{\"hdmi#INPUT#\":#ARG#}}";
							else
								payload = "{\"#CMD#\":{\"hdmi#INPUT#\":\"#ARG#\"}}";
							tools::replaceAllInPlace(payload, "#CMD#", command_ex);
							tools::replaceAllInPlace(payload, "#INPUT#", hdmi_input);
							tools::replaceAllInPlace(payload, "#ARG#", argument);
							return CreateEvent_luna_set_system_setting_payload(_Devices(words, 2), category, payload);
							
						}
						else
							return CreateEvent_luna_set_system_setting_basic(_Devices(words, 2), category, setting, argument, format);
					}
					else
					{
						iError = 3;
						error = arguments;
					}
				}
				else
					iError = 1;
			}
		}
		if (!found)
			iError = 4;
	}
	nlohmann::json err;
	if (iError == 1)
	{
		err["error"] = "Missing argument.";
		err["command"] = command;
		return err.dump() ;	
	}
	else if (iError == 2)
	{
		std::stringstream s;
		s << "Invalid JSON: " << error;
		err["error"] = s.str();
		err["command"] = command;
		return err.dump();
	}
	else if (iError == 3)
	{
		err["error"] = "Invalid argument.";
		err["command"] = command;
		err["ValidArgs"] = error;
		return err.dump();
	}
	else if (iError == 4)
	{
		err["error"] = "Invalid command.";
		err["command"] = command;
		return err.dump();
	}
	err["error"] = "Unknown parser error.";
	err["command"] = command;
	return err.dump();
}
std::vector<std::string> _Devices(std::vector<std::string> CommandWords, int Offset)
{
	if (Offset < CommandWords.size())
		for (int i = 0; i < Offset; i++)
			CommandWords.erase(CommandWords.begin());
	else
		CommandWords.clear();
	return CommandWords;
}
std::string ValidateArgument(std::string Argument, std::string ValidationList)
{
	if (Argument == "")
		return "";
	transform(Argument.begin(), Argument.end(), Argument.begin(), ::tolower);
	std::vector<std::string> list = tools::stringsplit(ValidationList, " ");

	for (auto& item : list)
	{
		std::string item_lowercase = item;
		transform(item_lowercase.begin(), item_lowercase.end(), item_lowercase.begin(), ::tolower);
		if (item_lowercase == Argument)
			return item;
	}
	return "";
}
std::string CreateEvent_system(DWORD dwEvent, std::vector<std::string> devices)
{
	EVENT event;
	event.dwType = dwEvent;
	event.devices = devices;
	return ProcessEvent(event);
}
std::string CreateEvent_request(std::vector<std::string> devices, std::string uri, std::string payload)
{
	EVENT event;
	event.dwType = EVENT_REQUEST;
	event.request_uri = uri;
	event.request_payload_json = payload;
	event.devices = devices;
	return ProcessEvent(event);
}
std::string CreateEvent_luna_set_system_setting_basic(std::vector<std::string> devices, std::string category, std::string setting, std::string value, std::string format)
{
	EVENT event;
	event.dwType = EVENT_LUNA_SYSTEMSET_BASIC;
	event.luna_system_setting_category = category;
	event.luna_system_setting_setting = setting;
	event.luna_system_setting_value = value;
	event.luna_system_setting_value_format = format;
	event.devices = devices;
	return ProcessEvent(event);
}
std::string CreateEvent_luna_set_system_setting_payload(std::vector<std::string> devices, std::string category, std::string payload)
{
	EVENT event;
	event.dwType = EVENT_LUNA_SYSTEMSET_PAYLOAD;
	event.luna_system_setting_category = category;
	event.luna_payload_json = payload;
	event.devices = devices;
	return ProcessEvent(event);
}
std::string CreateEvent_button(std::vector<std::string> devices, std::string button)
{
	EVENT event;
	event.dwType = EVENT_BUTTON;
	event.button = button;
	event.devices = devices;
	return ProcessEvent(event);
}
std::string CreateEvent_luna_set_device_info(std::vector<std::string> devices, std::string input, std::string icon, std::string label)
{
	EVENT event;
	event.dwType = EVENT_LUNA_DEVICEINFO;
	event.luna_device_info_input = input;
	event.luna_device_info_icon = icon;
	event.luna_device_info_label = label;
	event.devices = devices;
	return ProcessEvent(event);
}
std::string CreateEvent_luna_generic(std::vector<std::string> devices, std::string luna, std::string payload)
{
	EVENT event;
	event.dwType = EVENT_LUNA_GENERIC;
	event.request_uri = luna;
	event.luna_payload_json = payload;
	event.devices = devices;
	return ProcessEvent(event);
}
std::string	ProcessEvent(EVENT& event)
{
	nlohmann::json response;

	for (auto& dev : Prefs.devices_)
	{
		bool bMatch = false;
		if (event.devices.size() == 0)			// ALL DEVICES
			bMatch = true;
		else									// MATCHING DEVICES
		{
			std::string dev_id_lowercase = dev.id;
			std::string dev_name_lowercase = dev.name;
			transform(dev_id_lowercase.begin(), dev_id_lowercase.end(), dev_id_lowercase.begin(), ::tolower);
			transform(dev_name_lowercase.begin(), dev_name_lowercase.end(), dev_name_lowercase.begin(), ::tolower);
			for (auto& ev : event.devices)
			{
				std::string event_dev_lowercase = ev;
				transform(event_dev_lowercase.begin(), event_dev_lowercase.end(), event_dev_lowercase.begin(), ::tolower);
				if (event_dev_lowercase == dev_id_lowercase || event_dev_lowercase == dev_name_lowercase)
					bMatch = true;			
			}
		}
		if (bMatch)
		{
			nlohmann::json params;
			std::string png;
			switch (event.dwType)
			{
			case EVENT_FORCE_DISPLAYON:
				response[dev.id] = PowerOnDevice(dev);
				break;
			case EVENT_FORCE_DISPLAYOFF:
				response[dev.id] = SendRequest(dev, CreateRequestJson(LG_URI_POWEROFF), false);
				break;
			case EVENT_FORCE_BLANKSCREEN:
				response[dev.id] = SendRequest(dev, CreateRequestJson(LG_URI_SCREENOFF), false);
				break;
			case EVENT_REQUEST:
				response[dev.id] = SendRequest(dev, CreateRequestJson(event.request_uri, event.request_payload_json), false);
				break;
			case EVENT_LUNA_SYSTEMSET_BASIC:
				response[dev.id] = SendRequest(dev, CreateLunaSystemSettingJson(event.luna_system_setting_setting, event.luna_system_setting_value, event.luna_system_setting_category, event.luna_system_setting_value_format), true);
				break;
			case EVENT_LUNA_SYSTEMSET_PAYLOAD:
				params["category"] = event.luna_system_setting_category;
				params["settings"] = nlohmann::json::parse(event.luna_payload_json);
				response[dev.id] = SendRequest(dev, CreateRawLunaJson(LG_LUNA_SET_SYSTEM_SETT, params), true);
				break;
			case EVENT_BUTTON:
				response[dev.id] = SendButtonRequest(dev, event.button);
				break;
			case EVENT_LUNA_DEVICEINFO:
				png = event.luna_device_info_icon;
				png += ".png";
				params["id"] = event.luna_device_info_input;
				params["icon"] = png;
				params["label"] = event.luna_device_info_label;
				response[dev.id] = SendRequest(dev, CreateRawLunaJson(LG_LUNA_SET_DEVICE_INFO, params), true);
				break;
			case EVENT_LUNA_GENERIC:
				params = nlohmann::json::parse(event.luna_payload_json);
				response[dev.id] = SendRequest(dev, CreateRawLunaJson(event.request_uri, params), true);
				break;

			default:break;
			}
		}
	}
	if (!response.is_object())
	{
		response["error"] = "Mo matching devices.";
	}
	return response.dump();
}
nlohmann::json CreateRequestJson(std::string uri, std::string payload)
{
	nlohmann::json j;
	j["id"] = (int)33;
	j["type"] = "request";

	if (uri != "")
	{
		uri = "ssap://" + uri;
		j["uri"] = uri;
	}
	if (payload != "")
	{
		j["payload"] = nlohmann::json::parse(payload);
	}
	return j;
}
nlohmann::json CreateLunaSystemSettingJson(std::string setting, std::string value, std::string category, std::string format)
{
	nlohmann::json payload, params, settings, button, event;
	if(format == "int")
	{
		int val = atoi(value.c_str());
		settings[setting] = val;
	}
	else
		settings[setting] = value;
	params["settings"] = settings;
	params["category"] = category;
	button["label"] = "";
	button["onClick"] = LG_LUNA_SET_SYSTEM_SETT;
	button["params"] = params;
	event["uri"] = LG_LUNA_SET_SYSTEM_SETT;
	event["params"] = params;

	payload["message"] = " ";
	payload["buttons"].push_back(button);
	payload["onclose"] = event;
	payload["onfail"] = event;

	return CreateRequestJson(LG_URI_CREATEALERT, payload.dump());
}
nlohmann::json CreateRawLunaJson(std::string lunauri, nlohmann::json params)
{
	nlohmann::json payload, params_parsed, button, event;
	button["label"] = "";
	button["onClick"] = lunauri;
	button["params"] = params;
	event["uri"] = lunauri;
	event["params"] = params;
	payload["message"] = " ";
	payload["buttons"].push_back(button);
	payload["onclose"] = event;
	payload["onfail"] = event;
	return CreateRequestJson(LG_URI_CREATEALERT, payload.dump());
}
nlohmann::json SendRequest(Device device, nlohmann::json request, bool isLuna)
{
	nlohmann::json response;
	if (device.session_key == "")
	{
		response["error"] = "No pairing key. Check the device configuration in LGTV Companion UI";
		return response;
	}
	else
	{
		try
		{
			std::string host = device.ip;

			//build handshake
			std::string sHandshake = tools::narrow(LG_HANDSHAKE_PAIRED);
			tools::replaceAllInPlace(sHandshake, "#CLIENTKEY#", device.session_key);

			beast::flat_buffer buffer;
			net::io_context ioc;
			tcp::resolver resolver{ ioc };
			ssl::context ctx{ ssl::context::tlsv12_client }; //context holds certificates
			//load_root_certificates(ctx);
			websocket::stream<beast::ssl_stream<tcp::socket>> wss{ ioc, ctx };
			websocket::stream<tcp::socket> ws{ ioc };
			auto const results = resolver.resolve(host, device.ssl ? PORT_SSL : PORT);
			auto ep = net::connect(device.ssl ? get_lowest_layer(wss) : ws.next_layer(), results);
			if (device.ssl)
				SSL_set_tlsext_host_name(wss.next_layer().native_handle(), host.c_str()); //SSL set SNI Hostname	
			host += ':' + std::to_string(ep.port()); //build the host string for the decorator	
			if (device.ssl)
				wss.next_layer().handshake(ssl::stream_base::client); //SSL handshake

			// Set a decorator to change the User-Agent of the handshake
			if (device.ssl)
			{
				wss.set_option(websocket::stream_base::decorator(
					[](websocket::request_type& req)
					{
						req.set(http::field::user_agent,
						std::string(BOOST_BEAST_VERSION_STRING) +
						" websocket-client-LGTVsvc");
					}));
				wss.handshake(host, "/");
			}
			else
			{
				ws.set_option(websocket::stream_base::decorator(
					[](websocket::request_type& req)
					{
						req.set(http::field::user_agent,
						std::string(BOOST_BEAST_VERSION_STRING) +
						" websocket-client-LGTVsvc");
					}));
				ws.handshake(host, "/");
			}
			if (device.ssl)
			{
				wss.write(net::buffer(std::string(sHandshake)));
				wss.read(buffer); // read the response
			}
			else
			{
				ws.write(net::buffer(std::string(sHandshake)));
				ws.read(buffer); // read the response
			}

			// check that handshake was successful
			nlohmann::json j = nlohmann::json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
			boost::string_view type = j["type"];
			buffer.consume(buffer.size());
			if (std::string(type) != "registered")
			{
				response["error"] = "Invalid pairing key.";
				if (device.ssl)
					wss.close(websocket::close_code::normal);
				else
					ws.close(websocket::close_code::normal);
				goto fn_end;
			}

			// send the json request
			if (device.ssl)
			{
				wss.write(net::buffer(request.dump()));
				wss.read(buffer);
			}
			else
			{
				ws.write(net::buffer(request.dump()));
				ws.read(buffer);
			}

			// parse the response
			j = nlohmann::json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
			buffer.consume(buffer.size());

			type = j["type"];

			if (std::string(type) == "response")
			{
				// Is it a LUNA request?			
				if (isLuna)
				{
					nlohmann::json k = j["payload"]["alertId"];
					if(!k.empty() && k.is_string())
					{
						std::string id = k.get<std::string>();
						size_t start = id.find('-', 0);
						if (start != std::string::npos && start + 1 < id.size())
						{
							id = id.substr(start + 1);
							if (id.size() > 0)
							{
								std::string close_alert_request = CreateRequestJson(LG_URI_CLOSEALERT, LG_URI_PAYLOAD_CLOSEALERT).dump();
								tools::replaceAllInPlace(close_alert_request, "#ARG#", id);
								if (device.ssl)
								{
									wss.write(net::buffer(close_alert_request));
									wss.read(buffer);
								}
								else
								{
									ws.write(net::buffer(close_alert_request));
									ws.read(buffer);
								}
								buffer.consume(buffer.size());
								response["info"] = "Cannot relay luna response.";
							}
							else
								response["error"] = "Invalid LUNA response.";
						}
						else
							response["error"] = "Invalid LUNA response.";
					}
					else
						response["error"] = "Invalid LUNA response.";
				}
				else
					response["payload"] = j["payload"];
			}
			else if (std::string(type) == "error")
			{
				nlohmann::json k = j["error"];
				if (!k.empty() && k.is_string())
					response["error"] = k.get<std::string>();
				else
					response["error"] = "An unknown error occured";
			}
			else
				response["error"] = "Invalid response from device.";
			if (device.ssl)
				wss.close(websocket::close_code::normal);
			else
				ws.close(websocket::close_code::normal);
		}
		catch (std::exception const& e)
		{
			std::stringstream s;
			s << "exception: " << e.what();
			response["error"] = s.str();
		}
	}
fn_end:
	return response;
}
nlohmann::json SendButtonRequest(Device device, std::string button)
{
	nlohmann::json response;
	if (device.session_key == "")
	{
		response["error"] = "No pairing key. Check the device configuration in LGTV Companion UI";
		return response;
	}
	else
	{
		try
		{
			std::string host = device.ip;

			//build handshake
			std::string sHandshake = tools::narrow(LG_HANDSHAKE_PAIRED);
			tools::replaceAllInPlace(sHandshake, "#CLIENTKEY#", device.session_key);

			beast::flat_buffer buffer;
			net::io_context ioc;
			tcp::resolver resolver{ ioc };
			ssl::context ctx{ ssl::context::tlsv12_client }; //context holds certificates
			//load_root_certificates(ctx);
			websocket::stream<beast::ssl_stream<tcp::socket>> wss{ ioc, ctx };
			websocket::stream<tcp::socket> ws{ ioc };
			auto const results = resolver.resolve(host, device.ssl ? PORT_SSL : PORT);
			auto ep = net::connect(device.ssl ? get_lowest_layer(wss) : ws.next_layer(), results);
			if (device.ssl)
				SSL_set_tlsext_host_name(wss.next_layer().native_handle(), host.c_str()); //SSL set SNI Hostname	
			host += ':' + std::to_string(ep.port()); //build the host string for the decorator	
			if (device.ssl)
				wss.next_layer().handshake(ssl::stream_base::client); //SSL handshake

			// Set a decorator to change the User-Agent of the handshake
			if (device.ssl)
			{
				wss.set_option(websocket::stream_base::decorator(
					[](websocket::request_type& req)
					{
						req.set(http::field::user_agent,
						std::string(BOOST_BEAST_VERSION_STRING) +
						" websocket-client-LGTVsvc");
					}));
				wss.handshake(host, "/");
			}
			else
			{
				ws.set_option(websocket::stream_base::decorator(
					[](websocket::request_type& req)
					{
						req.set(http::field::user_agent,
						std::string(BOOST_BEAST_VERSION_STRING) +
						" websocket-client-LGTVsvc");
					}));
				ws.handshake(host, "/");
			}
			if (device.ssl)
			{
				wss.write(net::buffer(std::string(sHandshake)));
				wss.read(buffer); // read the response
			}
			else
			{
				ws.write(net::buffer(std::string(sHandshake)));
				ws.read(buffer); // read the response
			}
			nlohmann::json j = nlohmann::json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
			boost::string_view type = j["type"];
			buffer.consume(buffer.size());
			if (std::string(type) != "registered")
			{
				response["error"] = "Invalid pairing key.";
				if (device.ssl)
					wss.close(websocket::close_code::normal);
				else
					ws.close(websocket::close_code::normal);
				goto fn_end;
			}
			// request endpoint
			std::string input_socket_request = CreateRequestJson(LG_URI_GETINPUTSOCKET).dump();
			if (device.ssl)
			{
				wss.write(net::buffer(input_socket_request));
				wss.read(buffer);
			}
			else
			{
				ws.write(net::buffer(input_socket_request));
				ws.read(buffer);
			}

			j = nlohmann::json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
			buffer.consume(buffer.size());

			type = j["type"];

			if (std::string(type) == "response")
			{
				bool success = false;

				nlohmann::json l = j["payload"]["returnValue"];
				if (!l.empty() && l.is_boolean())
					success = l.get<bool>();
				if (success)
				{
					nlohmann::json k = j["payload"]["socketPath"];
					if (!k.empty() && k.is_string())
					{
						std::string path = k.get<std::string>();
						size_t pos = path.find("/resources");
						if (pos != std::string::npos)
						{
							path = path.substr(pos);
						}
						else
						{
							response["error"] = "Invalid resource path.";
							if (device.ssl)
								wss.close(websocket::close_code::normal);
							else
								ws.close(websocket::close_code::normal);
							goto fn_end;
						}

						net::io_context iocx;
						tcp::resolver resolverx{ iocx };
						ssl::context ctxx{ ssl::context::tlsv12_client }; //context holds certificates
						//load_root_certificates(ctx);
						websocket::stream<beast::ssl_stream<tcp::socket>> wssx{ iocx, ctxx };
						websocket::stream<tcp::socket> wsx{ iocx };

						auto const resultsx = resolverx.resolve(device.ip, device.ssl ? PORT_SSL : PORT);
						auto epx = net::connect(device.ssl ? get_lowest_layer(wssx) : wsx.next_layer(), resultsx);
						if (device.ssl)
							SSL_set_tlsext_host_name(wssx.next_layer().native_handle(), device.ip.c_str()); //SSL set SNI Hostname	
						//		host += ':' + std::to_string(ep.port()); //build the host string for the decorator	
						if (device.ssl)
							wssx.next_layer().handshake(ssl::stream_base::client); //SSL handshake

						// Set a decorator to change the User-Agent of the handshake
						if (device.ssl)
						{
							wssx.set_option(websocket::stream_base::decorator(
								[](websocket::request_type& reqx)
								{
									reqx.set(http::field::user_agent,
									std::string(BOOST_BEAST_VERSION_STRING) +
									" websocket-client-LGTVsvc");
								}));
							wssx.handshake(host, path);
						}
						else
						{
							wsx.set_option(websocket::stream_base::decorator(
								[](websocket::request_type& reqx)
								{
									reqx.set(http::field::user_agent,
									std::string(BOOST_BEAST_VERSION_STRING) +
									" websocket-client-LGTVsvc");
								}));
							wsx.handshake(host, path);
						}
						std::string button_command;
						button_command = "type:button\nname:";
						button_command += button;
						button_command += "\n\n";
						if (device.ssl)
							wssx.write(net::buffer(std::string(button_command)));
						else
							wsx.write(net::buffer(std::string(button_command)));

						response["button"] = button;

						if (device.ssl)
							wssx.close(websocket::close_code::normal);
						else
							wsx.close(websocket::close_code::normal);
					}
					else
						response["error"] = "Invalid response from device.";
				}
				else
					response["error"] = "Invalid response from device.";
			}
			else
				response["error"] = "Invalid response from device.";

			if (device.ssl)
				wss.close(websocket::close_code::normal);
			else
				ws.close(websocket::close_code::normal);
		}
		catch (std::exception const& e)
		{
			std::stringstream s;
			s << "exception: " << e.what();
			response["error"] = s.str();
		}
	}
fn_end:
	return response;
}
nlohmann::json PowerOnDevice(Device device)
{
	nlohmann::json response;
	if (device.session_key == "")
	{
		response["error"] = "No pairing key. Check the device configuration in LGTV Companion UI";
		return response;
	}
	else
	{
		//build handshake
		std::string sHandshake = tools::narrow(LG_HANDSHAKE_PAIRED);
		tools::replaceAllInPlace(sHandshake, "#CLIENTKEY#", device.session_key);

		std::string host;
		time_t origtim = time(0);
		boost::string_view type;
		boost::string_view state;
		bool bPoweredOn = false;

		// Spawn the Wake-On-LAN thread
		std::thread wolthread(Thread_WOL, device);
		wolthread.detach();

		//try waking up the display, but not longer than timeout user preference
		while (time(0) - origtim < (Prefs.power_on_timeout_ + 1))
		{
			time_t looptim = time(0);
			try
			{
				host = device.ip;
				bool bScreenWasBlanked = false;
				beast::flat_buffer buffer;
				net::io_context ioc;
				tcp::resolver resolver{ ioc };
				ssl::context ctx{ ssl::context::tlsv12_client }; //context holds certificates
				//load_root_certificates(ctx);
				websocket::stream<beast::ssl_stream<tcp::socket>> wss{ ioc, ctx };
				websocket::stream<tcp::socket> ws{ ioc };
				auto const results = resolver.resolve(host, device.ssl ? PORT_SSL : PORT);
				auto ep = net::connect(device.ssl ? get_lowest_layer(wss) : ws.next_layer(), results);
				if (device.ssl)
					SSL_set_tlsext_host_name(wss.next_layer().native_handle(), host.c_str()); 	//SSL set SNI Hostname
				host += ':' + std::to_string(ep.port()); //build the host string for the decorator		
				if (device.ssl)
					wss.next_layer().handshake(ssl::stream_base::client); //SSL handshake
				if (device.ssl)
				{
					wss.set_option(websocket::stream_base::decorator(
						[](websocket::request_type& req)
						{
							req.set(http::field::user_agent,
							std::string(BOOST_BEAST_VERSION_STRING) +
							" websocket-client-LGTVsvc");
						}));
					wss.handshake(host, "/");
					wss.write(net::buffer(std::string(sHandshake)));
					wss.read(buffer); // read the first response
				}
				else
				{
					ws.set_option(websocket::stream_base::decorator(
						[](websocket::request_type& req)
						{
							req.set(http::field::user_agent,
							std::string(BOOST_BEAST_VERSION_STRING) +
							" websocket-client-LGTVsvc");
						}));
					ws.handshake(host, "/");
					ws.write(net::buffer(std::string(sHandshake)));
					ws.read(buffer); // read the first response
				}

				nlohmann::json j = nlohmann::json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
				boost::string_view check = j["type"];
				buffer.consume(buffer.size());
				if (std::string(check) != "registered") // Device is unregistered.
				{
					response["error"] = "Invalid pairing key.";
					if (device.ssl)
						wss.close(websocket::close_code::normal);
					else
						ws.close(websocket::close_code::normal);
					goto fn_end;
				}
				// unblank the screen
				std::string unblank_request = CreateRequestJson(LG_URI_SCREENON).dump();
				if (device.ssl)
				{
					wss.write(net::buffer(std::string(unblank_request)));
					wss.read(buffer);
				}
				else
				{
					ws.write(net::buffer(std::string(unblank_request)));
					ws.read(buffer);
				}

				buffer.consume(buffer.size());
				//retreive power state from device to determine if the device is powered on
				std::string get_powerstate_request = CreateRequestJson(LG_URI_GETPOWERSTATE).dump();
				if (device.ssl)
				{
					wss.write(net::buffer(std::string(get_powerstate_request)));
					wss.read(buffer);
				}
				else
				{
					ws.write(net::buffer(std::string(get_powerstate_request)));
					ws.read(buffer);
				}

				j = nlohmann::json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());

				type = j["type"];
				state = j["payload"]["state"];
				if (std::string(type) == "response")
				{
					if (std::string(state) == "Active") // IT IS NOW POWERED ON
					{
						buffer.consume(buffer.size());
						response["payload"] = j["payload"];
						bPoweredOn = true;

						if (device.ssl)
							wss.close(websocket::close_code::normal);
						else
							ws.close(websocket::close_code::normal);
						break;
					}
				}
				buffer.consume(buffer.size());
				if (device.ssl)
					wss.close(websocket::close_code::normal);
				else
					ws.close(websocket::close_code::normal);
			}
			catch (std::exception)
			{
				
			}
			time_t endtim = time(0);
			time_t execution_time = endtim - looptim;
			if (execution_time >= 0 && execution_time < 1)
				Sleep((1 - (DWORD)execution_time) * 1000);
		}
		if(!bPoweredOn)
		{
			response["error"] = "Could not power on device.";
			response["timeOut"] = Prefs.power_on_timeout_;
		}
	}
	
fn_end:
	bTerminateThread = true;
	return response;
}
namespace {
	class NetEntryDeleter final {
	public:
		NetEntryDeleter(const NetEntryDeleter&) = delete;
		NetEntryDeleter& operator=(const NetEntryDeleter&) = delete;

		NetEntryDeleter(NET_LUID luid, SOCKADDR_INET address) : luid(luid), address(address) {}

		~NetEntryDeleter() {
			MIB_IPNET_ROW2 row;
			row.InterfaceLuid = luid;
			row.Address = address;
			DeleteIpNetEntry2(&row);
		}

	private:
		const NET_LUID luid;
		const SOCKADDR_INET address;
	};

	boost::optional<NET_LUID> GetLocalInterface(SOCKADDR_INET destination) {
		MIB_IPFORWARD_ROW2 row;
		SOCKADDR_INET bestSourceAddress;
		const auto result = GetBestRoute2(NULL, 0, NULL, &destination, 0, &row, &bestSourceAddress);

		if (result != NO_ERROR) {
			return boost::none;
		}

		if (row.Protocol != MIB_IPPROTO_LOCAL) {
			return boost::none;
		}
		return row.InterfaceLuid;
	}

	std::unique_ptr<NetEntryDeleter> CreateTransientLocalNetEntry(SOCKADDR_INET destination, unsigned char macAddress[6]) {
		const auto luid = GetLocalInterface(destination);
		if (!luid.has_value())
			return nullptr;

		MIB_IPNET_ROW2 row;
		row.Address = destination;
		row.InterfaceLuid = *luid;
		memcpy(row.PhysicalAddress, macAddress, sizeof(macAddress));
		row.PhysicalAddressLength = sizeof(macAddress);
		const auto result = CreateIpNetEntry2(&row);
		if (result != NO_ERROR)
			return nullptr;
		return std::make_unique<NetEntryDeleter>(*luid, destination);
	}
}
void Thread_WOL(Device device)
{
	if (device.mac_addresses.size() < 1)
		return;

	SOCKET WOLsocket = INVALID_SOCKET;
	time_t origtim = time(0);
	
	//send WOL packet every second until timeout, or until the calling thread has ended
	while (!bTerminateThread && (time(0) - origtim < (Prefs.power_on_timeout_ + 1)))
	{
		time_t looptim = time(0);
		try
		{
			struct sockaddr_in LANDestination {};
			LANDestination.sin_family = AF_INET;
			LANDestination.sin_port = htons(9);
			if (device.wake_method == WOL_TYPE_SUBNETBROADCAST && device.subnet != "")
			{
				std::vector<std::string> vIP = tools::stringsplit(device.ip, ".");
				std::vector<std::string> vSubnet = tools::stringsplit(device.subnet, ".");
				std::stringstream broadcastaddress;

				if (vIP.size() == 4 && vSubnet.size() == 4)
				{
					for (int i = 0; i < 4; i++)
					{
						int a = atoi(vIP[i].c_str());
						int b = atoi(vSubnet[i].c_str());
						int c = 256 + (a | (~b));

						broadcastaddress << c;
						if (i < 3)
							broadcastaddress << ".";
					}
					LANDestination.sin_addr.s_addr = inet_addr(broadcastaddress.str().c_str());
				}
				else
					return;
			}
			else if (device.wake_method == WOL_TYPE_IP || device.wake_method == WOL_TYPE_AUTO)
				LANDestination.sin_addr.s_addr = inet_addr(device.ip.c_str());
			else
				LANDestination.sin_addr.s_addr = 0xFFFFFFFF;

			WOLsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (WOLsocket != INVALID_SOCKET)
			{
				const bool optval = TRUE;
				if (setsockopt(WOLsocket, SOL_SOCKET, SO_BROADCAST, (char*)&optval, sizeof(optval)) == SOCKET_ERROR)
					closesocket(WOLsocket);
				else
				{
					for (auto& MAC : device.mac_addresses)
					{
						//remove filling from MAC
						char CharsToRemove[] = ".:- ";
						for (int i = 0; i < strlen(CharsToRemove); ++i)
							MAC.erase(remove(MAC.begin(), MAC.end(), CharsToRemove[i]), MAC.end());

						//Build a magic packet (6 x 0xFF followed by 16 x MAC) and broadcast it
						if (MAC.length() == 12)
						{
							unsigned char Message[102];
							unsigned char MACstr[6];
							for (auto i = 0; i < 6; ++i)
								Message[i] = 0xFF;
							for (auto i = 0; i < 6; ++i)
								MACstr[i] = static_cast<unsigned char>(std::stoul(MAC.substr(i * 2, 2), nullptr, 16));
							for (auto i = 1; i <= 16; ++i)
								memcpy(&Message[i * 6], &MACstr, 6 * sizeof(unsigned char));
							
							std::unique_ptr<NetEntryDeleter> netEntryDeleter;
							if (device.wake_method == WOL_TYPE_IP)
								netEntryDeleter = CreateTransientLocalNetEntry(reinterpret_cast<const SOCKADDR_INET&>(LANDestination), MACstr);

							// Send Wake On LAN packet
							sendto(WOLsocket, (char*)&Message, 102, 0, reinterpret_cast<sockaddr*>(&LANDestination), sizeof(LANDestination));
						}
					}
					if (WOLsocket != INVALID_SOCKET)
						closesocket(WOLsocket);
				}
			}

		}
		catch (std::exception)
		{

		}
		
		time_t endtim = time(0);
		time_t execution_time = endtim - looptim;
		if (execution_time >= 0 && execution_time < 1)
			Sleep((1 - (DWORD)execution_time) * 1000);
	}


	if (WOLsocket != INVALID_SOCKET)
		closesocket(WOLsocket);
	return;
}
std::string helpText(void)
{
	std::string response=
#include "../Common/help_commands.h"
			;
	std::string ver = "v ";
	ver += tools::narrow(APP_VERSION);
	tools::replaceAllInPlace(response, "%%VER%%", ver);
	return response;
}