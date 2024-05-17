#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0603
#define _WIN32_WINNT 0x0603
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "event.h"
#include "../Common/lg_api.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::vector<std::string> Event::getDevices(void) { 
	return devices_; 
}
int	Event::getType(void) { 
	return type_; 
}
std::string	Event::getData(void) { 
	return data_; 
}
std::string	Event::getLogMessage(void) {
	return log_message_;
}
/*
void Event::set(int type) { 
	type_ = type; 
}
*/
void Event::set(int type, std::vector<std::string> devices, std::string arg_1, std::string arg_2, std::string arg_3, std::string arg_4, std::string arg_5)
{
	type_ = type;
	if(devices.size() > 0)
		devices_ = devices;
	json params, settings;
	switch (type)
	{
	case EVENT_BUTTON: // arg_1 = button, arg_2 = log
		data_ = arg_1;
		log_message_ = arg_2;
		break;

	case EVENT_REQUEST: // arg_1 = uri, arg_2 = payload, arg_3 = log
		data_ = createRequest(arg_1, arg_2);
		log_message_ = arg_3;
		break;

	case EVENT_LUNA_SYSTEMSET_BASIC: // arg_1 = setting, arg_2 = value, arg_3 = category, arg_4 = format, arg_5 = log)
		if (arg_4 == "int")
			settings[arg_1] = std::stoi(arg_2);
		else
			settings[arg_1] = arg_2;
		params["settings"] = settings;
		params["category"] = arg_3;
		data_ = createRequest(LG_URI_CREATEALERT, createLunaPayload(LG_LUNA_SET_SYSTEM_SETT, params.dump()), true);
		log_message_ = arg_5;
		break;

	case EVENT_LUNA_SYSTEMSET_PAYLOAD: // arg_1 = category, arg_2 = payload, arg_3 = log
		params["category"] = arg_1;
		try
		{
			params["settings"] = json::parse(arg_2);
		}
		catch (std::exception const&)
		{
			// FIXME logging
		}
		data_ = createRequest(LG_URI_CREATEALERT, createLunaPayload(LG_LUNA_SET_SYSTEM_SETT, params.dump()), true);
		log_message_ = arg_3;
		break;

	case EVENT_LUNA_DEVICEINFO: // arg_1 = input, arg_2 = icon, arg_3 = label, arg_4 = log_message
		arg_2 += ".png";
		params["id"] = arg_1;
		params["icon"] = arg_2;
		params["label"] = arg_3;
		data_ = createRequest(LG_URI_CREATEALERT, createLunaPayload(LG_LUNA_SET_DEVICE_INFO, params.dump()), true);
		log_message_ = arg_4;
		break;

	case EVENT_LUNA_GENERIC: // arg_1 = luna, arg_2 = params, arg_2 = log_message
		try
		{
			params = json::parse(arg_2);
		}
		catch (std::exception const&)
		{
			// FIXME logging
		}
		data_ = createRequest(LG_URI_CREATEALERT, createLunaPayload(arg_1, params.dump()), true);
		log_message_ = arg_3;
		break;

	default:break;
	}
}
std::string Event::createRequest(std::string ep, std::string payload, bool is_luna)
{
	std::string uri;
	json j, pl;
//	event_id_++;
	j["id"] = is_luna ? "luna_request" : "request"; // event_id_;
	j["type"] = "request";
	uri = "ssap://" + ep;
	j["uri"] = uri;
	if (payload != "")
	{
		try
		{
			j["payload"] = json::parse(payload);
		}
		catch (std::exception const&)
		{
			//FIXME logging
		}
	}
//	if (event_id_ >= 50)
//		event_id_ = 0;
	return j.dump();
}
std::string Event::createLunaPayload(std::string luna, std::string param)
{
	json payload, params, settings, button, req;
	try
	{
		params = json::parse(param);
	}
	catch (std::exception const&)
	{
		// FIXME logging
		return "";
	}
	button["label"] = "";
	button["onClick"] = luna;
	button["params"] = params;
	req["uri"] = luna;
	req["params"] = params;
	payload["message"] = " ";
	payload["buttons"].push_back(button);
	payload["onclose"] = req;
	payload["onfail"] = req;
	return payload.dump();
}
