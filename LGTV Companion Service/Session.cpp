// See LGTV Companion UI.cpp for additional details about this application
#include "Service.h"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>


namespace			beast = boost::beast;					// from <boost/beast.hpp>
namespace			http = beast::http;						// from <boost/beast/http.hpp>
namespace			websocket = beast::websocket;			// from <boost/beast/websocket.hpp>
namespace			net = boost::asio;						// from <boost/asio.hpp>
using				tcp = boost::asio::ip::tcp;				// from <boost/asio/ip/tcp.hpp>
namespace			ssl = boost::asio::ssl;					// from <boost/asio/ssl.hpp>
using				json = nlohmann::json;
namespace			common = jpersson77::common;
namespace			settings = jpersson77::settings;

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

	boost::optional<NET_LUID> GetLocalInterface(SOCKADDR_INET destination, std::string sDev, std::string * log_msg) {
		MIB_IPFORWARD_ROW2 row;
		SOCKADDR_INET bestSourceAddress;
		const auto result = GetBestRoute2(NULL, 0, NULL, &destination, 0, &row, &bestSourceAddress);

		std::stringstream log;
		log << sDev << ", Best route to IP -";

		if (result != NO_ERROR) {
			log << " failed with code " << result;
			*log_msg = log.str();
			return boost::none;
		}

		log << " interface index " << row.InterfaceIndex << " LUID " << row.InterfaceLuid.Value << " route protocol " << row.Protocol;

		if (row.Protocol != MIB_IPPROTO_LOCAL) {
			log << "; route is not local, aborting!";
			*log_msg = log.str();
			return boost::none;
		}
		*log_msg = log.str();

		return row.InterfaceLuid;
	}

	std::unique_ptr<NetEntryDeleter> CreateTransientLocalNetEntry(SOCKADDR_INET destination, unsigned char macAddress[6], std::string sDev, std::string * log_msg) {
		const auto luid = GetLocalInterface(destination, sDev, log_msg);
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
CSessionManager::CSessionManager()
{
}
CSessionManager::~CSessionManager()
{
	for (auto& pSess : Sessions)
		delete pSess;
	Sessions.clear();
}
void CSessionManager::NewEvent(EVENT& Event)
{	
	if (Sessions.size() == 0)
		return;
	
	SetThreadExecutionState(ES_AWAYMODE_REQUIRED | ES_CONTINUOUS);

	// process event for all devices
	if (Event.devices.size() == 0)			
		for (auto& Sess : Sessions)
			ProcessEvent(Event, *Sess);
	// process event only for matching devices
	else										
		for (auto& Sess : Sessions)
			for (auto& Dev : Event.devices)
			{
				std::string Dev_lowercase = Dev;
				transform(Dev_lowercase.begin(), Dev_lowercase.end(), Dev_lowercase.begin(), ::tolower);
				if (Dev_lowercase == Sess->DeviceID_lowercase || Dev_lowercase == Sess->Name_lowercase)
					ProcessEvent(Event, *Sess);
			}
	switch (Event.dwType)
	{
		case EVENT_SYSTEM_SHUTDOWN:
		case EVENT_SYSTEM_UNSURE:
		case EVENT_SYSTEM_REBOOT:
		case EVENT_SYSTEM_SUSPEND:
//			Log("[DEBUG] Entering 1000ms sleep - suspend/reboot/shutdown");
			Sleep(1000);
//			Log("[DEBUG] Exiting 1000ms sleep - suspend/reboot/shutdown");
			break;
		case EVENT_SYSTEM_DISPLAYDIMMED:
			// fix for only receiving DIMMED event when screensaver is active
			if (Event.ScreenSaverActiveWhileDimmed) 
			{
				bDisplaysCurrentlyPoweredOnByWindows = false;
				Sleep(200);
			}
			break;
		case EVENT_SYSTEM_DISPLAYOFF:
			bDisplaysCurrentlyPoweredOnByWindows = false;
			Sleep(200);
			break;
		case EVENT_SYSTEM_RESUME:
		case EVENT_SYSTEM_RESUMEAUTO:
		case EVENT_SYSTEM_DISPLAYON:
			bDisplaysCurrentlyPoweredOnByWindows = true;
			break;
		default:break;
	}
	SetThreadExecutionState(ES_CONTINUOUS);
	return;
}
void CSessionManager::ProcessEvent(EVENT& Event, CSession& Session)
{
	// Process user initiated events, i. e forced
	nlohmann::json params;
	std::string png;
	switch (Event.dwType)
	{
	case EVENT_USER_DISPLAYON:
		Session.PowerOnDisplay();
		break;
	case EVENT_USER_DISPLAYOFF:
		Session.PowerOffDisplay(true, false);
		break;
	case EVENT_USER_BLANKSCREEN:
		Session.PowerOffDisplay(true, true);
		break;
	case EVENT_REQUEST:
		Session.SendRequest(Event.request_uri, Event.request_payload_json, Event.log_message);
		break;
	case EVENT_LUNA_SYSTEMSET_BASIC:
		Session.SendLunaSystemSettingRequest(Event.luna_system_setting_setting, Event.luna_system_setting_value, Event.luna_system_setting_category, Event.log_message);
		break;
	case EVENT_LUNA_SYSTEMSET_PAYLOAD:
		try
		{
			params["category"] = Event.luna_system_setting_category;
			params["settings"] = nlohmann::json::parse(Event.luna_payload_json);
		}
		catch (std::exception const& e)
		{
			std::string ss = "ERROR! Invalid payload JSON.Error: ";
			ss += e.what();
			Log(ss);
			break;
		}
		Session.SendLunaRawRequest(LG_LUNA_SET_SYSTEM_SETT, params.dump(), Event.log_message);
		break;
	case EVENT_BUTTON:
		Session.SendButtonRequest(Event.button, Event.log_message);
		break;
	case EVENT_LUNA_DEVICEINFO:
		png = Event.luna_device_info_icon;
		png += ".png";
		params["id"] = Event.luna_device_info_input;
		params["icon"] = png;
		params["label"] = Event.luna_device_info_label;
		Session.SendLunaRawRequest(LG_LUNA_SET_DEVICE_INFO, params.dump(), Event.log_message);
		break;

	default:break;
	}

	if (!Session.isManagedAutomatically)
		return;

	// Process system events
	switch (Event.dwType)
	{
	case EVENT_SYSTEM_REBOOT:
		bRemoteClientIsConnected = false;
		break;
	case EVENT_SYSTEM_SHUTDOWN:
	case EVENT_SYSTEM_UNSURE:
		Session.PowerOffDisplay();
		bRemoteClientIsConnected = false;
		break;
	case EVENT_SYSTEM_SUSPEND:
		bRemoteClientIsConnected = false;
		break;
	case EVENT_SYSTEM_RESUME:
		bRemoteClientIsConnected = false;
		break;
	case EVENT_SYSTEM_RESUMEAUTO:
		bRemoteClientIsConnected = false;
		if (Prefs.AdhereTopology && !Session.TopologyEnabled)
			break;
		if (Session.Parameters.SetHDMIInputOnResume)
		{
			std::string payload = LG_URI_PAYLOAD_SETHDMI;
			std::string log = "set hdmi-input [#ARG#]";
			common::ReplaceAllInPlace(payload, "#ARG#", std::to_string(Session.Parameters.SetHDMIInputOnResumeToNumber));
			common::ReplaceAllInPlace(log, "#ARG#", std::to_string(Session.Parameters.SetHDMIInputOnResumeToNumber));
			Session.SendRequest(LG_URI_LAUNCH, payload, log, true);
		}
		break;
	case EVENT_SYSTEM_DISPLAYON:
		if (bRemoteClientIsConnected)
			break;
		if (Prefs.AdhereTopology && !Session.TopologyEnabled)
			break;
		Session.PowerOnDisplay();
		break;
	case EVENT_SYSTEM_DISPLAYDIMMED:
		//fix for only receiving DIMMED event when screensaver is active
		if (Event.ScreenSaverActiveWhileDimmed)
		{
			if (bRemoteClientIsConnected)
				break;
			if (GetWindowsPowerStatus() == true)
				Session.PowerOffDisplay();
		}
		break;
	case EVENT_SYSTEM_DISPLAYOFF:
		
		if (bRemoteClientIsConnected)
			break;
		if(GetWindowsPowerStatus() == true)
			Session.PowerOffDisplay();
		break;
	case EVENT_SYSTEM_USERIDLE:
		if (bRemoteClientIsConnected)
			break;
		if (Prefs.AdhereTopology && !Session.TopologyEnabled)
			break;
		if (Prefs.BlankScreenWhenIdle)
			Session.PowerOffDisplay(false, true);
		break;
	case EVENT_SYSTEM_USERBUSY:
		if (bRemoteClientIsConnected)
			break;
		if (Prefs.AdhereTopology && !Session.TopologyEnabled)
			break;
		if (Prefs.BlankScreenWhenIdle)
			Session.PowerOnDisplay();
		break;
	case EVENT_SYSTEM_BOOT:
		if (Prefs.AdhereTopology && !Session.TopologyEnabled)
			break;
		if (Session.Parameters.SetHDMIInputOnResume)
		{
			std::string payload = LG_URI_PAYLOAD_SETHDMI;
			std::string log = "set hdmi-input [#ARG#]";
			common::ReplaceAllInPlace(payload, "#ARG#", std::to_string(Session.Parameters.SetHDMIInputOnResumeToNumber));
			common::ReplaceAllInPlace(log, "#ARG#", std::to_string(Session.Parameters.SetHDMIInputOnResumeToNumber));
			Session.SendRequest(LG_URI_LAUNCH, payload, log, true);
		}
		break;
	case EVENT_SYSTEM_TOPOLOGY:
		if (bRemoteClientIsConnected)
			break;
		if (Prefs.AdhereTopology)
			if (Session.TopologyEnabled)
				Session.PowerOnDisplay();
			else
				Session.PowerOffDisplay();
		break;
	default:break;
	}

	return;
}
std::string CSessionManager::ValidateDevices(std::vector<std::string>& Devices)
{
	if (Sessions.size() == 0)
		return "No devices configured";

	std::string ret;
	if (Devices.size() == 0)
		ret = "All devices";
	else
		for (auto& Sess : Sessions)
			for (auto& Dev : Devices)
			{
				std::string Dev_lowercase = Dev;
				transform(Dev_lowercase.begin(), Dev_lowercase.end(), Dev_lowercase.begin(), ::tolower);
				if (Dev_lowercase == Sess->DeviceID_lowercase || Dev_lowercase == Sess->Name_lowercase)
				{
					ret += Sess->Parameters.DeviceId;
					ret += " ";
				}
			}
	if (ret == "")
		ret = "Invalid device id or name";
	return ret;
}
std::string CSessionManager::ValidateArgument(std::string Argument, std::string ValidationList)
{
	if (Argument == "")
		return "";
	transform(Argument.begin(), Argument.end(), Argument.begin(), ::tolower);
	std::vector<std::string> list = common::stringsplit(ValidationList, " ");

	for (auto& item : list)
	{
		std::string item_lowercase = item;
		transform(item_lowercase.begin(), item_lowercase.end(), item_lowercase.begin(), ::tolower);
		if (item_lowercase == Argument)
			return item;
	}
	return "";
}
void CSessionManager::AddSession(jpersson77::settings::DEVICE& Device)
{
	CSession* S = new CSession (Device, Prefs);
	Sessions.push_back(S);
}
void CSessionManager::SetPreferences(jpersson77::settings::PREFERENCES& pPrefs)
{
	Prefs = pPrefs;
	Prefs.AdhereTopology = false; // disable initially
}
std::string CSessionManager::LoadSavedTopologyConfiguration(void)
{
	std::vector<std::string> devices;
	std::string return_value;
	std::wstring file = Prefs.DataPath;
	file += TOPOLOGY_CONFIGURATION_FILE;
	std::ifstream i(file.c_str());
	if (i.is_open())
	{
		try
		{
			nlohmann::json j;
			nlohmann::json topology_json;
			i >> topology_json;
			i.close();
			// Read version of the preferences file. If this key is found it is assumed the config file has been populated
			j = topology_json[JSON_PREFS_NODE][JSON_VERSION];
			if (!j.empty() && j.is_number())
			{
				j = topology_json[JSON_PREFS_NODE][JSON_TOPOLOGY_NODE];
				if (!j.empty() && j.size() > 0)
				{
					for (auto& str : j.items())
					{
						if (str.value().is_string())
						{
							std::string temp = str.value().get<std::string>();
							devices.push_back(temp);
						}
					}
				}
				return SetTopology(devices);
			}
		}
		catch (std::exception const& e)
		{
			std::string s = "Error parsing topology configuration file: ";
			s += e.what();
			return s;
		}
		return "Invalid topology configuration file.";
	}
	return "No saved topology configuration.";
}
void CSessionManager::SaveTopologyConfiguration(void)
{
	if (Sessions.size() == 0)
		return;

	nlohmann::json topology_json;

	std::wstring file = Prefs.DataPath;
	CreateDirectory(Prefs.DataPath.c_str(), NULL);
	file += TOPOLOGY_CONFIGURATION_FILE;
	topology_json[JSON_PREFS_NODE][JSON_VERSION] = (int)Prefs.version;
	for (auto& item : Sessions)
		if(item->TopologyEnabled)
			topology_json[JSON_PREFS_NODE][JSON_TOPOLOGY_NODE].push_back(item->DeviceID_lowercase);
	
	if (!topology_json.empty())
	{
		std::ofstream i(file.c_str());
		if (i.is_open())
		{
			i << std::setw(4) << topology_json << std::endl;
			i.close();
		}
	}
}
void CSessionManager::TerminateAndWait(void)
{
	bool bWait;
	if (Sessions.size() == 0)
		return;

	for (auto& Sess : Sessions)
		Sess->Terminate();

	do
	{
		bWait = false;

		for (auto& dev : Sessions)
		{
			if (dev->IsBusy())
			{
				bWait = true;
			}
		}
		if (bWait)
			Sleep(20);
	} while (bWait);
	return;
}
void CSessionManager::Enable(std::vector<std::string>& Devices)
{
	if (Sessions.size() == 0)
		return;

	for (auto& Sess : Sessions)
		for (auto& dev : Devices)
		{
			std::string Dev_lowercase = dev;
			transform(Dev_lowercase.begin(), Dev_lowercase.end(), Dev_lowercase.begin(), ::tolower);
			if (Sess->DeviceID_lowercase == Dev_lowercase || Sess->Name_lowercase == Dev_lowercase)

				Sess->isManagedAutomatically = true;
		}
	return;
}
void CSessionManager::Disable(std::vector<std::string>& Devices)
{
	if (Sessions.size() == 0)
		return;

	for (auto& Sess : Sessions)
		for (auto& dev : Devices)
		{
			std::string Dev_lowercase = dev;
			transform(Dev_lowercase.begin(), Dev_lowercase.end(), Dev_lowercase.begin(), ::tolower);
			if (Sess->DeviceID_lowercase == Dev_lowercase || Sess->Name_lowercase == Dev_lowercase)
				Sess->isManagedAutomatically = false;
		}
	return;
}
void CSessionManager::RemoteClientIsConnected(bool bSet)
{
	bRemoteClientIsConnected = bSet;
}
std::string CSessionManager::SetTopology(std::vector<std::string>& Devices)
{
	if (Sessions.size() == 0)
		return "No devices configured";
	
	Prefs.AdhereTopology = true;
	
	std::string ret;
	for (auto& Sess : Sessions)
	{
		
		Sess->TopologyEnabled = false;
		for (auto& dev : Devices)
		{
			std::string Dev_lowercase = dev;
			transform(Dev_lowercase.begin(), Dev_lowercase.end(), Dev_lowercase.begin(), ::tolower);
			if (Sess->DeviceID_lowercase == Dev_lowercase || Sess->Name_lowercase == Dev_lowercase)
			{
				Sess->TopologyEnabled = true;
			}
		}
		ret += Sess->Parameters.DeviceId;
		ret += ":";
		ret += Sess->TopologyEnabled ? "ON " : "OFF ";
	}
	return ret;
}
bool CSessionManager::GetWindowsPowerStatus(void)
{
	return bDisplaysCurrentlyPoweredOnByWindows;
}
CSession::CSession(settings::DEVICE& pParams, settings::PREFERENCES& pPrefs)
{
	Parameters = pParams;
	Prefs = pPrefs;
	DeviceID_lowercase = Parameters.DeviceId;
	Name_lowercase = Parameters.Name;
	transform(DeviceID_lowercase.begin(), DeviceID_lowercase.end(), DeviceID_lowercase.begin(), ::tolower);
	transform(Name_lowercase.begin(), Name_lowercase.end(), Name_lowercase.begin(), ::tolower);

	// build the appropriate WebOS handshake
	if (Parameters.SessionKey == "")
		sHandshake = common::narrow(LG_HANDSHAKE_NOTPAIRED);
	else
	{
		sHandshake = common::narrow(LG_HANDSHAKE_PAIRED);
		common::ReplaceAllInPlace(sHandshake, "#CLIENTKEY#", Parameters.SessionKey);
	}
	isManagedAutomatically = Parameters.Enabled;
}
CSession::~CSession()
{
}
bool CSession::IsBusy(void)
{
	time_t now = time(0);

	
	//failsafe
	if (now - lastOnTime > 2)
		Thread_DisplayOn_isRunning = false;
	if (now - lastOffTime > 2)
		Thread_DisplayOff_isRunning = false;

	return Thread_DisplayOn_isRunning || Thread_DisplayOff_isRunning;
}
void CSession::Terminate(void)
{
	bTerminateThread = true;
}
void CSession::PowerOnDisplay(void)
{
	std::string logmsg;
	logmsg = Parameters.DeviceId;
	time_t now = time(0);

	//failsafe
	if(Thread_DisplayOn_isRunning)
		if (now - lastOnTime > Prefs.PowerOnTimeout)
			Thread_DisplayOn_isRunning = false;

	if (Thread_DisplayOn_isRunning)
	{
		logmsg += ", omitted Thread_DisplayOn().";
		Log(logmsg);
	}
	else
	{
		logmsg += ", spawning Thread_DisplayOn().";
		Log(logmsg);
		Thread_DisplayOn_isRunning = true;
		std::thread thread_obj(&CSession::Thread_DisplayOn, this);
		thread_obj.detach();
		lastOnTime = now;
	}

	
	return;
}
void CSession::PowerOffDisplay(bool forced, bool blankonly)
{

	std::string logmsg;
	logmsg = Parameters.DeviceId;
	time_t now = time(0);

	//failsafe
	if (Thread_DisplayOff_isRunning)
		if (now - lastOffTime > 5)
			Thread_DisplayOff_isRunning = false;

	if (Thread_DisplayOff_isRunning || Parameters.SessionKey == "")
	{
		logmsg += ", omitted Thread_DisplayOff().";
		if (Parameters.SessionKey == "")
			logmsg += " No pairing key.";
		Log(logmsg);
	}
	else
	{
	//	SetThreadExecutionState(ES_AWAYMODE_REQUIRED | ES_CONTINUOUS);
		logmsg += ", spawning Thread_DisplayOff().";
		Log(logmsg);
		Thread_DisplayOff_isRunning = true;
		std::thread thread_obj(&CSession::Thread_DisplayOff, this, forced, blankonly);
		thread_obj.detach();
		lastOffTime = now;
//		Sleep(200);
	//	SetThreadExecutionState(ES_CONTINUOUS);
	}
	return;
}
void CSession::SendRequest(std::string uri, std::string payload, std::string log_message, bool repeat)
{
	std::string logmsg = Parameters.DeviceId;
	
	if (Thread_SendRequest_isRunning > 5 || Parameters.SessionKey == "")
	{
		logmsg += ", omitted Thread_SendRequest(). ";
		if (Parameters.SessionKey == "")
			logmsg += "No pairing key.";
		else
			logmsg += "Rate limiter active.";
		Log(logmsg);
	}
	else
	{
		std::string basic_request_json = CreateRequestJson(uri, payload).dump();		
		logmsg += ", spawning Thread_SendRequest().";
		Log(logmsg);
		Thread_SendRequest_isRunning++;
		std::thread thread_obj(&CSession::Thread_SendRequest, this, basic_request_json, log_message, repeat, false);
		thread_obj.detach();
	}
	return;
}
void CSession::SendLunaSystemSettingRequest(std::string setting, std::string value, std::string category, std::string log_message)
{
	std::string logmsg = Parameters.DeviceId;
	if (Thread_SendRequest_isRunning > 5 || Parameters.SessionKey == "")
	{
		logmsg += ", omitted Thread_SendRequest(). ";
		if (Parameters.SessionKey == "")
			logmsg += "No pairing key.";
		else
			logmsg += "Rate limiter active.";
		Log(logmsg);
	}
	else
	{
		std::string luna_request = CreateLunaSystemSettingJson(setting, value, category).dump();

		logmsg += ", spawning Thread_SendRequest().";
		Log(logmsg);
		Thread_SendRequest_isRunning++;
		std::thread thread_obj(&CSession::Thread_SendRequest, this, luna_request, log_message, false, true);
		thread_obj.detach();
	}
	return;
}
void CSession::SendLunaRawRequest(std::string uri, std::string payload, std::string log_message)
{
	std::string logmsg = Parameters.DeviceId;
	if (Thread_SendRequest_isRunning > 5 || Parameters.SessionKey == "")
	{
		logmsg += ", omitted Thread_SendRequest(). ";
		if (Parameters.SessionKey == "")
			logmsg += "No pairing key.";
		else
			logmsg += "Rate limiter active.";
		Log(logmsg);
	}
	else
	{
		std::string luna_request = CreateRawLunaJson(uri, payload).dump();

		logmsg += ", spawning Thread_SendRequest().";
		Log(logmsg);
		Thread_SendRequest_isRunning++;
		std::thread thread_obj(&CSession::Thread_SendRequest, this, luna_request, log_message, false, true);
		thread_obj.detach();
	}
	return;
}
void CSession::SendButtonRequest(std::string button, std::string log_message)
{
	std::string logmsg = Parameters.DeviceId;

	if (Thread_SendRequest_isRunning > 5 || Parameters.SessionKey == "")
	{
		logmsg += ", omitted Thread_ButtonRequest(). ";
		if (Parameters.SessionKey == "")
			logmsg += "No pairing key.";
		else
			logmsg += "Rate limiter active.";
		Log(logmsg);
	}
	else
	{
		logmsg += ", spawning Thread_SendRequest().";
		Log(logmsg);
		Thread_SendRequest_isRunning++;
		std::thread thread_obj(&CSession::Thread_ButtonRequest, this, button, log_message);
		thread_obj.detach();
	}
	return;
}
json CSession::CreateRequestJson(std::string uri, std::string payload)
{
	json j, payl;
	j["id"] = requestId;
	j["type"] = "request";

	try
	{
		if (uri != "")
		{
			uri = "ssap://" + uri;
			j["uri"] = uri;
		}
		if (payload != "")
		{
			j["payload"] = json::parse(payload);
		}
	}
	catch (std::exception const& e)
	{
		std::string ss = Parameters.DeviceId;
		ss += ", ERROR! CreateBasicJsonRequest - Invalid JSON. Error: ";
		ss += e.what();
		Log(ss);
		return "";
	}
	requestId++;
	if (requestId > 50)
		requestId = 1;
	return j;

}
json CSession::CreateLunaSystemSettingJson(std::string setting, std::string value, std::string category)
{
	nlohmann::json payload, params, settings, button, event;
	settings[setting] = value;// json::parse(value);
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
json CSession::CreateRawLunaJson(std::string lunauri, std::string params)
{
	nlohmann::json payload, params_parsed, button, event;
	try
	{
		params_parsed = nlohmann::json::parse(params);
		button["label"] = "";
		button["onClick"] = lunauri;
		button["params"] = params_parsed;
		event["uri"] = lunauri;
		event["params"] = params_parsed;

		payload["message"] = " ";
		payload["buttons"].push_back(button);
		payload["onclose"] = event;
		payload["onfail"] = event;
	}
	catch (std::exception const& e)
	{
		std::string ss = Parameters.DeviceId;
		ss += ", ERROR! CreateRawLunaJsonRequest - Invalid JSON. Error: ";
		ss += e.what();
		Log(ss);
		return "";
	}

	return CreateRequestJson(LG_URI_CREATEALERT, payload.dump());
}
void CSession::Thread_DisplayOn(void)
{
	std::string host;
	std::string logmsg;
	time_t origtim = time(0);
	boost::string_view type;
	boost::string_view state;

	// Spawn the Wake-On-LAN thread
	std::thread wolthread(&CSession::Thread_WOL, this);
	wolthread.detach();
//	Sleep(500);

	//try waking up the display, but not longer than timeout user preference
	while (!bTerminateThread && (time(0) - origtim < (Prefs.PowerOnTimeout + 1)))
	{
		time_t looptim = time(0);
		try
		{
			host = Parameters.IP;
			bool bScreenWasBlanked = false;
			beast::flat_buffer buffer;
			net::io_context ioc;
			tcp::resolver resolver{ ioc };		
			ssl::context ctx{ ssl::context::tlsv12_client }; //context holds certificates
			//load_root_certificates(ctx);
			websocket::stream<beast::ssl_stream<tcp::socket>> wss{ ioc, ctx };
			websocket::stream<tcp::socket> ws{ ioc };
			auto const results = resolver.resolve(host, Parameters.SSL ? SERVICE_PORT_SSL : SERVICE_PORT);
			auto ep = net::connect(Parameters.SSL ? get_lowest_layer(wss) : ws.next_layer(), results);
			if (Parameters.SSL)
				SSL_set_tlsext_host_name(wss.next_layer().native_handle(), host.c_str()); 	//SSL set SNI Hostname
			host += ':' + std::to_string(ep.port()); //build the host string for the decorator		
			if (Parameters.SSL)
				wss.next_layer().handshake(ssl::stream_base::client); //SSL handshake
			if (Parameters.SSL)
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
		
			{	//debuggy
				std::string tt = beast::buffers_to_string(buffer.data());
				std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) ON response 1: " : ", [DEBUG] ON Response 1: ";
				st += tt;
				Log(st);
			}

			json j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
			boost::string_view check = j["type"];
			buffer.consume(buffer.size());			
			if (std::string(check) != "registered") // Device is unregistered. Parse for pairing key as needed
			{
				if (Parameters.SessionKey != "")
				{
					logmsg = Parameters.DeviceId;
					logmsg += ", WARNING! Pairing key is invalid. Re-pairing forced!";
					Log(logmsg);

					sHandshake = common::narrow(LG_HANDSHAKE_NOTPAIRED);
					if (Parameters.SSL)
					{
						wss.write(net::buffer(std::string(sHandshake)));
						wss.read(buffer); // read (again) the first response
					}
					else
					{
						ws.write(net::buffer(std::string(sHandshake)));
						ws.read(buffer); // read (again) the first response
					}
				}
				buffer.consume(buffer.size());
				if (Parameters.SSL) //read the second response which should now contain the session key
					wss.read(buffer);
				else
					ws.read(buffer); 
				std::string t = beast::buffers_to_string(buffer.data());

				{	//debuggy
					std::string ss = Parameters.DeviceId; ss += Parameters.SSL ? ", [DEBUG] (SSL) ON response key: " : ", [DEBUG] ON response key: ";
					ss += t;
					Log(ss);
				}

				buffer.consume(buffer.size());
				size_t u = t.find("client-key\":\"");
				if (u != std::string::npos) // so did we get a session key?
				{
					size_t w = t.find("\"", u + 14);
					if (w != std::string::npos)
					{
						Parameters.SessionKey = t.substr(u + 13, w - u - 13);
						sHandshake = common::narrow(LG_HANDSHAKE_PAIRED);
						common::ReplaceAllInPlace(sHandshake, "#CLIENTKEY#", Parameters.SessionKey);
						SetSessionKey(Parameters.SessionKey, Parameters.DeviceId);
					}
				}
				else
				{
					logmsg = Parameters.DeviceId;
					logmsg += ", WARNING! Pairing key expected but not received.";
					Log(logmsg);
				}
			}
			// unblank the screen
			std::string unblank_request = CreateRequestJson(LG_URI_SCREENON).dump();
			if (Parameters.SSL)			
			{
				wss.write(net::buffer(std::string(unblank_request)));
				wss.read(buffer);
			}
			else
			{
				ws.write(net::buffer(std::string(unblank_request)));
				ws.read(buffer);
			}

			{	//debuggy
				std::string tt = beast::buffers_to_string(buffer.data());
				std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) ON response 2: " : ", [DEBUG] ON response 2: ";
				st += tt;
				Log(st);
			}

			//Was the screen blanked
			j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
			boost::string_view type = j["type"];
			if (std::string(type) == "response")
			{
				bool success = false;
				json l = j["payload"]["returnValue"];
				if (!l.empty() && l.is_boolean())
					success = l.get<bool>();
				if (success)
					bScreenWasBlanked = true;
			}

			buffer.consume(buffer.size());
			//retreive power state from device to determine if the device is powered on
			std::string get_powerstate_request = CreateRequestJson(LG_URI_GETPOWERSTATE).dump();
			if (Parameters.SSL)			
			{
				wss.write(net::buffer(std::string(get_powerstate_request)));
				wss.read(buffer);
			}
			else
			{
				ws.write(net::buffer(std::string(get_powerstate_request)));
				ws.read(buffer);
			}

			{	//debuggy
				std::string tt = beast::buffers_to_string(buffer.data());
				std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) ON response 3: " : ", [DEBUG] ON response 3: ";
				st += tt;
				Log(st);
			}

			j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());

			type = j["type"];
			state = j["payload"]["state"];
			if (std::string(type) == "response")
			{
				if (std::string(state) == "Active")
				{
					logmsg = Parameters.DeviceId;
					logmsg += ", power state is: ON";
					Log(logmsg);

					buffer.consume(buffer.size());

					// unmute speakers
					std::string mute_request = CreateRequestJson(LG_URI_SETMUTE).dump();
					if (Prefs.MuteSpeakers && bScreenWasBlanked)
					{
						if (Parameters.SSL)
						{
							wss.write(net::buffer(std::string(mute_request)));
							wss.read(buffer);
						}
						else
						{
							ws.write(net::buffer(std::string(mute_request)));
							ws.read(buffer);
						}

						{	//debuggy
							std::string tt = beast::buffers_to_string(buffer.data());
							std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) ON response 4: " : ", [DEBUG] ON response 4: ";
							st += tt;
							Log(st);
						}
						buffer.consume(buffer.size());
					}

					if (Parameters.SSL)
						wss.close(websocket::close_code::normal);
					else
						ws.close(websocket::close_code::normal);
					break;
				}
			}
			buffer.consume(buffer.size());
			if (Parameters.SSL)
				wss.close(websocket::close_code::normal);
			else
				ws.close(websocket::close_code::normal);
		}
		catch (std::exception const& e)
		{
			logmsg = Parameters.DeviceId;
			logmsg += ", WARNING! Thread_DisplayOn(): ";
			logmsg += e.what();
			Log(logmsg);
		}
		time_t endtim = time(0);
		time_t execution_time = endtim - looptim;
		if (execution_time >= 0 && execution_time < 1)
			Sleep((1 - (DWORD)execution_time) * 1000);
	}
	Thread_DisplayOn_isRunning = false;
	return;
}
void CSession::Thread_WOL(void)
{
	if (Parameters.MAC.size() < 1)
		return;

	bool wol_log_message_performed = false;

	SOCKET WOLsocket = INVALID_SOCKET;
	std::string logmsg;
	std::string IPentry_saved;
	time_t origtim = time(0);

	//send WOL packet every second until timeout, or until the calling thread has ended
	while (!bTerminateThread && (time(0) - origtim < (Prefs.PowerOnTimeout + 1)))
	{
		time_t looptim = time(0);
		try
		{
			struct sockaddr_in LANDestination {};
			LANDestination.sin_family = AF_INET;
			LANDestination.sin_port = htons(9);
			std::stringstream wolstr;
			if (Parameters.WOLtype == WOL_SUBNETBROADCAST && Parameters.Subnet != "")
			{
				std::vector<std::string> vIP = common::stringsplit(Parameters.IP, ".");
				std::vector<std::string> vSubnet = common::stringsplit(Parameters.Subnet, ".");
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
					wolstr << " using broadcast address: " << broadcastaddress.str();
					LANDestination.sin_addr.s_addr = inet_addr(broadcastaddress.str().c_str());
				}
				else
				{
					std::stringstream ss;
					ss << Parameters.DeviceId;
					ss << ", ERROR! Thread_WOL malformed subnet/IP";
					Log(ss.str());
					return;
				}
			}
			else if (Parameters.WOLtype == WOL_IPSEND)
			{
				LANDestination.sin_addr.s_addr = inet_addr(Parameters.IP.c_str());
				wolstr << " using IP address: " << Parameters.IP;
			}
			else
			{
				LANDestination.sin_addr.s_addr = 0xFFFFFFFF;
				wolstr << " using network broadcast: 255.255.255.255";
			}

			WOLsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (WOLsocket == INVALID_SOCKET)
			{
				int dw = WSAGetLastError();
				std::stringstream ss;
				ss << Parameters.DeviceId;
				ss << ", ERROR! Thread_WOL WS socket(): ";
				ss << dw;
				Log(ss.str());
				goto loop_end;
			}
			else
			{
				const bool optval = TRUE;
				if (setsockopt(WOLsocket, SOL_SOCKET, SO_BROADCAST, (char*)&optval, sizeof(optval)) == SOCKET_ERROR)
				{
					closesocket(WOLsocket);
					int dw = WSAGetLastError();
					std::stringstream ss;
					ss << Parameters.DeviceId;
					ss << ", ERROR! Thread_WOL WS setsockopt(): ";
					ss << dw;
					Log(ss.str());
					goto loop_end;
				}
			}
			for (auto& MAC : Parameters.MAC)
			{
				//remove filling from MAC
				char CharsToRemove[] = ".:- ";
				for (int i = 0; i < strlen(CharsToRemove); ++i)
					MAC.erase(remove(MAC.begin(), MAC.end(), CharsToRemove[i]), MAC.end());

				logmsg = Parameters.DeviceId;
				logmsg += ", repeating WOL broadcast started to MAC: ";
				logmsg += MAC;
				if (wolstr.str() != "")
					logmsg += wolstr.str();
				if (wol_log_message_performed == false)
				{
					Log(logmsg);
					wol_log_message_performed = true;
				}
			}
			if (WOLsocket != INVALID_SOCKET)
			{
				// build and broadcast magic packet(s) for every MAC
				for (auto& MAC : Parameters.MAC)
				{
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
						if (Parameters.WOLtype == WOL_IPSEND)
						{
							std::string log_msg;
							netEntryDeleter = CreateTransientLocalNetEntry(reinterpret_cast<const SOCKADDR_INET&>(LANDestination), MACstr, Parameters.DeviceId, &log_msg);
							if (log_msg != IPentry_saved)
							{
								Log(log_msg);
								IPentry_saved = log_msg;
							}
						}
						// Send Wake On LAN packet
						if (sendto(WOLsocket, (char*)&Message, 102, 0, reinterpret_cast<sockaddr*>(&LANDestination), sizeof(LANDestination)) == SOCKET_ERROR)
						{
							int dw = WSAGetLastError();
							std::stringstream ss;
							ss << Parameters.DeviceId;
							ss << ", WARNING! Thread_WOL WS sendto(): ";
							ss << dw;
							Log(ss.str());
						}
					}
					else
					{
						logmsg = Parameters.DeviceId;
						logmsg += ", WARNING! Thread_WOL malformed MAC";
						Log(logmsg);
					}
				}
			}
			else
			{
				logmsg = Parameters.DeviceId;
				logmsg += ", WARNING! Thread_WOL illegal socket reference";
				Log(logmsg);
			}
		}
		catch (std::exception const& e)
		{

			logmsg = Parameters.DeviceId;
			logmsg += ", ERROR! Thread_WOL: ";
			logmsg += e.what();
			Log(logmsg);
		}
		if (WOLsocket != INVALID_SOCKET)
		{
			closesocket(WOLsocket);
			WOLsocket = INVALID_SOCKET;
		}
loop_end:
		time_t endtim = time(0);
		time_t execution_time = endtim - looptim;
		if (execution_time >= 0 && execution_time < 1)
			Sleep((1 - (DWORD)execution_time) * 1000);
		if (Thread_DisplayOn_isRunning == false)
			break;
	}
	logmsg = Parameters.DeviceId;
	logmsg += ", repeating WOL broadcast ended";
	Log(logmsg);

	return;
}
void CSession::Thread_DisplayOff(bool UserForced, bool BlankScreen)
{
	time_t origtim = time(0);
	std::string logmsg = Parameters.DeviceId;
	std::string host = Parameters.IP;
	json j;
	boost::string_view type;
	boost::string_view state;
	boost::string_view appId;
	
	SetThreadExecutionState(ES_AWAYMODE_REQUIRED | ES_CONTINUOUS);
	if (Parameters.SessionKey == "")
	{
			logmsg += ", WARNING! Thread_DisplayOff() - no pairing key";
			Log(logmsg);
			goto threadend;
	}
	try
	{
		beast::flat_buffer buffer;
		net::io_context ioc;
		tcp::resolver resolver{ ioc };
		ssl::context ctx{ ssl::context::tlsv12_client }; //context holds certificates
		//load_root_certificates(ctx);
		websocket::stream<beast::ssl_stream<tcp::socket>> wss{ ioc, ctx };
		websocket::stream<tcp::socket> ws{ ioc };
		auto const results = resolver.resolve(host, Parameters.SSL ? SERVICE_PORT_SSL : SERVICE_PORT);
		auto ep = net::connect(Parameters.SSL ? get_lowest_layer(wss) : ws.next_layer(), results);	
		if (Parameters.SSL)
			SSL_set_tlsext_host_name(wss.next_layer().native_handle(), host.c_str()); //SSL set SNI Hostname	
		host += ':' + std::to_string(ep.port()); //build the host string for the decorator	
		if (Parameters.SSL)
			wss.next_layer().handshake(ssl::stream_base::client); //SSL handshake

		if (time(0) - origtim > 10) // this thread should not run too long
		{
			logmsg += ", WARNING! Thread_DisplayOff() - forced exit";
			Log(logmsg);
			goto threadend;
		}
		// Set a decorator to change the User-Agent of the handshake
		if (Parameters.SSL)
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
		if (Parameters.SSL)
		{
			wss.write(net::buffer(std::string(sHandshake)));
			wss.read(buffer); // read the response
		}
		else
		{
			ws.write(net::buffer(std::string(sHandshake)));
			ws.read(buffer); // read the response
		}
		
		{	//debuggy
			std::string tt = beast::buffers_to_string(buffer.data());
			std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) OFF response 1: " : ", [DEBUG] OFF response 1: ";
			st += tt;
			Log(st);
		}

		j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
		type = j["type"];
		buffer.consume(buffer.size());
		if (std::string(type) != "registered")
		{
			logmsg += ", WARNING! Thread_DisplayOff() - Pairing key is invalid.";
			Log(logmsg);
			if (Parameters.SSL)
				wss.close(websocket::close_code::normal);
			else
				ws.close(websocket::close_code::normal);
			goto threadend;
		}
		//retreive power state from device to determine if the device is powered on
		std::string get_power_state_request = CreateRequestJson(LG_URI_GETPOWERSTATE).dump();
		if (Parameters.SSL)
		{
			wss.write(net::buffer(std::string(get_power_state_request)));
			wss.read(buffer);
		}
		else
		{
			ws.write(net::buffer(std::string(get_power_state_request)));
			ws.read(buffer);
		}

		//debuggy
		{
			std::string tt = beast::buffers_to_string(buffer.data());
			std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) OFF response 2: " : ", [DEBUG] OFF response 2: ";
			st += tt;
			Log(st);
		}

		j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
		buffer.consume(buffer.size());

		type = j["type"];
		state = j["payload"]["state"];
		if (std::string(type) == "response")
		{
			if ((!BlankScreen && std::string(state) != "Active" && std::string(state) != "Screen Off") ||
				(BlankScreen && std::string(state) != "Active"))
			{
				logmsg += ", power state is: ";
				logmsg += std::string(state);
				Log(logmsg);
				if (Parameters.SSL)
					wss.close(websocket::close_code::normal);
				else
					ws.close(websocket::close_code::normal);
				goto threadend;
			}
		}
		else
		{
			logmsg += ", WARNING! Thread_DisplayOff() - Invalid response from device - PowerState.";
			Log(logmsg);
			if (Parameters.SSL)
				wss.close(websocket::close_code::normal);
			else
				ws.close(websocket::close_code::normal);
			goto threadend;
		}

		bool shouldPowerOff = true;

		// Only process power off events when device is set to a certain HDMI input
		if (!UserForced && Parameters.HDMIinputcontrol)
		{
			std::string app;
			shouldPowerOff = false;

			// get information about active input
			std::string get_foreground_app_request = CreateRequestJson(LG_URI_GETFOREGROUNDAPP).dump();
			if (Parameters.SSL)
			{
				wss.write(net::buffer(std::string(get_foreground_app_request)));
				wss.read(buffer);
			}
			else
			{
				ws.write(net::buffer(std::string(get_foreground_app_request)));
				ws.read(buffer);
			}

			{	//debuggy
				std::string tt = beast::buffers_to_string(buffer.data());
				std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) OFF response 3: " : ", [DEBUG] OFF response 3: ";
				st += tt;
				Log(st);
			}
			j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());

			appId = j["payload"]["appId"];
			app = std::string("Current AppId: ") + std::string(appId);
			const boost::string_view prefix = "com.webos.app.hdmi";
			if (appId.starts_with(prefix))
			{
				appId.remove_prefix(prefix.size());
				if (std::to_string(Parameters.OnlyTurnOffIfCurrentHDMIInputNumberIs) == appId) {
					logmsg += ", HDMI";
					logmsg += std::string(appId);
					logmsg += " input active.";
					Log(logmsg);
					shouldPowerOff = true;
				}
				else
				{
					logmsg += ", HDMI";
					logmsg += std::to_string(Parameters.OnlyTurnOffIfCurrentHDMIInputNumberIs);
					logmsg += " input inactive.";
					Log(logmsg);
				}
			}
			else
			{
				logmsg += ", ";
				logmsg += app;
				logmsg += " active.";
				logmsg += app;
				Log(logmsg);
			}
			buffer.consume(buffer.size());
		}
		if (shouldPowerOff)
		{
			// send device or screen off command to device
			std::string off_request = CreateRequestJson(BlankScreen ? LG_URI_SCREENOFF : LG_URI_POWEROFF).dump();
			if (Parameters.SSL)
			{
				wss.write(net::buffer(std::string(off_request)));
				wss.read(buffer); // read the response
			}
			else
			{
				ws.write(net::buffer(std::string(off_request)));
				ws.read(buffer); // read the response
			}

			//debuggy
			{
				std::string tt = beast::buffers_to_string(buffer.data());
				std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) OFF response 4: " : ", [DEBUG] OFF response 4: ";
				st += tt;
				Log(st);
			}

			logmsg = Parameters.DeviceId;
			if (BlankScreen)
				logmsg += ", power state is: ON (blanked screen). ";
			else
				logmsg += ", power state is: OFF.";
			Log(logmsg);
			buffer.consume(buffer.size());

			// mute speakers
			if (Prefs.MuteSpeakers && BlankScreen)
			{
				std::string mute_request = CreateRequestJson(LG_URI_SETMUTE, "{\"mute\":\"true\"}").dump();
				if (Parameters.SSL)
				{
					wss.write(net::buffer(std::string(mute_request)));
					wss.read(buffer);
				}
				else
				{
					ws.write(net::buffer(std::string(mute_request)));
					ws.read(buffer);
				}
				//debuggy
				{
					std::string tt = beast::buffers_to_string(buffer.data());
					std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) ON response 5: " : ", [DEBUG] ON response 5: ";
					st += tt;
					Log(st);
				}
			}
			buffer.consume(buffer.size());
		}
		else
		{
			logmsg = Parameters.DeviceId;
			logmsg += ", power state is: Unchanged.";
			Log(logmsg);
		}

		if (Parameters.SSL)
			wss.close(websocket::close_code::normal);
		else
			ws.close(websocket::close_code::normal);
	}
	catch (std::exception const& e)
	{
		logmsg = Parameters.DeviceId;
		logmsg += ", WARNING! Thread_DisplayOff(): ";
		logmsg += e.what();
		Log(logmsg);
	}
threadend:
	Thread_DisplayOff_isRunning = false;
	SetThreadExecutionState(ES_CONTINUOUS);
	return;
}
void CSession::Thread_SendRequest(std::string input_json, std::string log_message, bool repeat, bool isLuna)
{
	time_t origtim = time(0);
	std::string log = Parameters.DeviceId;
	if (Parameters.SessionKey == "")
	{
		log += ", WARNING! Thread_SendRequest() - no pairing key";
		Log(log);
		goto threadend;
	}
	if (input_json == "")
	{
		log += ", WARNING! Thread_SendRequest() - invalid command";
		Log(log);
		goto threadend;
	}
	do
	{
		time_t looptim = time(0);
		try
		{
			std::string host = Parameters.IP;
			beast::flat_buffer buffer;
			net::io_context ioc;
			tcp::resolver resolver{ ioc };
			ssl::context ctx{ ssl::context::tlsv12_client }; //context holds certificates
			//load_root_certificates(ctx);
			websocket::stream<beast::ssl_stream<tcp::socket>> wss{ ioc, ctx };
			websocket::stream<tcp::socket> ws{ ioc };
			auto const results = resolver.resolve(host, Parameters.SSL ? SERVICE_PORT_SSL : SERVICE_PORT);
			auto ep = net::connect(Parameters.SSL ? get_lowest_layer(wss) : ws.next_layer(), results);
			if (Parameters.SSL)
				SSL_set_tlsext_host_name(wss.next_layer().native_handle(), host.c_str()); //SSL set SNI Hostname	
			host += ':' + std::to_string(ep.port()); //build the host string for the decorator	
			if (Parameters.SSL)
				wss.next_layer().handshake(ssl::stream_base::client); //SSL handshake

			if (!repeat && time(0) - origtim > 10) // this thread should not run too long
			{
				log = Parameters.DeviceId;
				log += ", WARNING! Thread_SendRequest() - forced exit";
				Log(log);
				goto threadend;
			}
			// Set a decorator to change the User-Agent of the handshake
			if (Parameters.SSL)
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
			if (Parameters.SSL)
			{
				wss.write(net::buffer(std::string(sHandshake)));
				wss.read(buffer); // read the response
			}
			else
			{
				ws.write(net::buffer(std::string(sHandshake)));
				ws.read(buffer); // read the response
			}

			{	//debuggy
				std::string tt = beast::buffers_to_string(buffer.data());
				std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) REQ response 1: " : ", [DEBUG] REQ response 1: ";
				st += tt;
				Log(st);
			}

			json j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
			boost::string_view type = j["type"];
			buffer.consume(buffer.size());
			if (std::string(type) != "registered")
			{
				log = Parameters.DeviceId;
				log += ", WARNING! Thread_SendRequest() - Pairing key is invalid.";
				Log(log);
				if (Parameters.SSL)
					wss.close(websocket::close_code::normal);
				else
					ws.close(websocket::close_code::normal);
				goto threadend;
			}
			// send the json
			if (Parameters.SSL)
			{
				wss.write(net::buffer(input_json));
				wss.read(buffer);
			}
			else
			{
				ws.write(net::buffer(input_json));
				ws.read(buffer);
			}

			//debuggy
			{
				std::string tt = beast::buffers_to_string(buffer.data());
				std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) REQ response 2: " : ", [DEBUG] REQ response 2: ";
				st += tt;
				Log(st);
			}

			j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
			buffer.consume(buffer.size());

			type = j["type"];

			if (std::string(type) == "response")
			{
				bool success = false;

				json l = j["payload"]["returnValue"];
				if (!l.empty() && l.is_boolean())
					success = l.get<bool>();
				if(success)
				{
					json k = j["payload"]["alertId"]; // Was it a LUNA request?
					if (isLuna && !k.empty() && k.is_string())
					{
						std::string id = k.get<std::string>();
						size_t start = id.find('-', 0);
						if (start != std::string::npos && start + 1 < id.size())
						{
							id = id.substr(start + 1);
							if (id.size() > 0)
							{
								std::string close_alert_request = CreateRequestJson(LG_URI_CLOSEALERT, LG_URI_PAYLOAD_CLOSEALERT).dump();
								common::ReplaceAllInPlace(close_alert_request, "#ARG#", id);
								//retreive power state from device to determine if the device is powered on
								if (Parameters.SSL)
								{
									wss.write(net::buffer(close_alert_request));
									wss.read(buffer);
								}
								else
								{
									ws.write(net::buffer(close_alert_request));
									ws.read(buffer);
								}

								//debuggy
								{
									std::string tt = beast::buffers_to_string(buffer.data());
									std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) REQ response 3: " : ", [DEBUG] REQ response 3: ";
									st += tt;
									Log(st);
								}
								buffer.consume(buffer.size());
							}
						}
					}
					log = Parameters.DeviceId;
					log += ", ";
					log += log_message;
					Log(log);

					if (Parameters.SSL)
						wss.close(websocket::close_code::normal);
					else
						ws.close(websocket::close_code::normal);
					goto threadend;
				}
				else
				{
					log = Parameters.DeviceId;
					log += ", WARNING! Thread_SendRequest() - returnValue is FALSE.";
					Log(log);
				}

			}
			else if (std::string(type) == "error")
			{
				nlohmann::json k = j["error"];
				if (!k.empty() && k.is_string())
				{
					log = Parameters.DeviceId;
					log += ", WARNING! Thread_SendRequest() - Error: ";
					log += k.get<std::string>();
					Log(log);
				}
				else
				{
					log = Parameters.DeviceId;
					log += ", WARNING! Thread_SendRequest() - Unknown response from device";
					Log(log);
				}
			}
			else
			{
				log = Parameters.DeviceId;
				log += ", WARNING! Thread_SendRequest() - Invalid response from device.";
				Log(log);
				if (Parameters.SSL)
					wss.close(websocket::close_code::normal);
				else
					ws.close(websocket::close_code::normal);
				goto threadend;
			}

			if (Parameters.SSL)
				wss.close(websocket::close_code::normal);
			else
				ws.close(websocket::close_code::normal);
		}	
		catch (std::exception const& e)
		{
			log = Parameters.DeviceId;
			log += ", WARNING! Thread_SendRequest(): ";
			log += e.what();
			Log(log);
		}
		if (repeat && !bTerminateThread)
		{
			time_t endtim = time(0);
			time_t execution_time = endtim - looptim;
			if (execution_time >= 0 && execution_time < 1)
				Sleep((1 - (DWORD)execution_time) * 1000);
		}
	} while (repeat && !bTerminateThread && (time(0) - origtim < (Prefs.PowerOnTimeout + 1)));

threadend:

	Thread_SendRequest_isRunning--;
	return;
}
void CSession::Thread_ButtonRequest(std::string button, std::string log_message)
{
	time_t origtim = time(0);
	std::string log = Parameters.DeviceId;
	if (Parameters.SessionKey == "")
	{
		log += ", WARNING! Thread_ButtonRequest() - no pairing key";
		Log(log);
		goto threadend;
	}
	if (button == "")
	{
		log += ", WARNING! Thread_ButtonRequest() - invalid command";
		Log(log);
		goto threadend;
	}

	try
	{
		std::string host = Parameters.IP;
		beast::flat_buffer buffer;
		net::io_context ioc;
		tcp::resolver resolver{ ioc };
		ssl::context ctx{ ssl::context::tlsv12_client }; //context holds certificates
		//load_root_certificates(ctx);
		websocket::stream<beast::ssl_stream<tcp::socket>> wss{ ioc, ctx };
		websocket::stream<tcp::socket> ws{ ioc };
		auto const results = resolver.resolve(host, Parameters.SSL ? SERVICE_PORT_SSL : SERVICE_PORT);
		auto ep = net::connect(Parameters.SSL ? get_lowest_layer(wss) : ws.next_layer(), results);
		if (Parameters.SSL)
			SSL_set_tlsext_host_name(wss.next_layer().native_handle(), host.c_str()); //SSL set SNI Hostname	
		host += ':' + std::to_string(ep.port()); //build the host string for the decorator	
		if (Parameters.SSL)
			wss.next_layer().handshake(ssl::stream_base::client); //SSL handshake

		if (time(0) - origtim > 10) // this thread should not run too long
		{
			log = Parameters.DeviceId;
			log += ", WARNING! Thread_ButtonRequest() - forced exit";
			Log(log);
			goto threadend;
		}
		// Set a decorator to change the User-Agent of the handshake
		if (Parameters.SSL)
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
		if (Parameters.SSL)
		{
			wss.write(net::buffer(std::string(sHandshake)));
			wss.read(buffer); // read the response
		}
		else
		{
			ws.write(net::buffer(std::string(sHandshake)));
			ws.read(buffer); // read the response
		}

		{	//debuggy
			std::string tt = beast::buffers_to_string(buffer.data());
			std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) BUT response 1: " : ", [DEBUG] BUT response 1: ";
			st += tt;
			Log(st);
		}

		json j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
		boost::string_view type = j["type"];
		buffer.consume(buffer.size());
		if (std::string(type) != "registered")
		{
			log = Parameters.DeviceId;
			log += ", WARNING! Thread_ButtonRequest() - Pairing key is invalid.";
			Log(log);
			if (Parameters.SSL)
				wss.close(websocket::close_code::normal);
			else
				ws.close(websocket::close_code::normal);
			goto threadend;
		}
		// request endpoint
		std::string input_socket_request = CreateRequestJson(LG_URI_GETINPUTSOCKET).dump();
		if (Parameters.SSL)
		{
			wss.write(net::buffer(input_socket_request));
			wss.read(buffer);
		}
		else
		{
			ws.write(net::buffer(input_socket_request));
			ws.read(buffer);
		}

		//debuggy
		{
			std::string tt = beast::buffers_to_string(buffer.data());
			std::string st = Parameters.DeviceId; st += Parameters.SSL ? ", [DEBUG] (SSL) BUT response 2: " : ", [DEBUG] BUT response 2: ";
			st += tt;
			Log(st);
		}

		j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
		buffer.consume(buffer.size());

		type = j["type"];

		if (std::string(type) == "response")
		{
			bool success = false;

			json l = j["payload"]["returnValue"];
			if (!l.empty() && l.is_boolean())
				success = l.get<bool>();
			if (success)
			{
				json k = j["payload"]["socketPath"]; 
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
						log = Parameters.DeviceId;
						log += ", WARNING! Thread_SendRequest() - no resource path received.";
						Log(log);
						goto threadend;
					}

					net::io_context iocx;
					tcp::resolver resolverx{ iocx };
					ssl::context ctxx{ ssl::context::tlsv12_client }; //context holds certificates
					//load_root_certificates(ctx);
					websocket::stream<beast::ssl_stream<tcp::socket>> wssx{ iocx, ctxx };
					websocket::stream<tcp::socket> wsx{ iocx };

					auto const resultsx = resolverx.resolve(Parameters.IP, Parameters.SSL ? SERVICE_PORT_SSL : SERVICE_PORT);
					auto epx = net::connect(Parameters.SSL ? get_lowest_layer(wssx) : wsx.next_layer(), resultsx);
					if (Parameters.SSL)
						SSL_set_tlsext_host_name(wssx.next_layer().native_handle(), Parameters.IP.c_str()); //SSL set SNI Hostname	
			//		host += ':' + std::to_string(ep.port()); //build the host string for the decorator	
					if (Parameters.SSL)
						wssx.next_layer().handshake(ssl::stream_base::client); //SSL handshake

					// Set a decorator to change the User-Agent of the handshake
					if (Parameters.SSL)
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
					if (Parameters.SSL)
					{
						wssx.write(net::buffer(std::string(button_command)));
//						wssx.read(buffer); // read the response
					}
					else
					{
						wsx.write(net::buffer(std::string(button_command)));
//						wsx.read(buffer); // read the response
					}
					log = Parameters.DeviceId;
					log += ", "; 
					log += log_message; 
					Log(log);

					if (Parameters.SSL)
						wssx.close(websocket::close_code::normal);
					else
						wsx.close(websocket::close_code::normal);
					if (Parameters.SSL)
						wss.close(websocket::close_code::normal);
					else
						ws.close(websocket::close_code::normal);
				}
			}
			else
			{
				log = Parameters.DeviceId;
				log += ", WARNING! Thread_SendRequest() - returnValue is FALSE.";
				Log(log);
			}
		}
		else
		{
			log = Parameters.DeviceId;
			log += ", WARNING! Thread_ButtonRequest() - Invalid response.";
			Log(log);
		}
	}
	catch (std::exception const& e)
	{
		log = Parameters.DeviceId;
		log += ", WARNING! Thread_ButtonRequest(): ";
		log += e.what();
		Log(log);
	}

threadend:

	Thread_SendRequest_isRunning--;
	return;
}
