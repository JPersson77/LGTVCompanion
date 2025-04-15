#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00

#include "companion.h"
#include "web_os_client.h"
#include "../Common/tools.h"
#include "../Common/log.h"
#include "../Common/lg_api.h"
#include "../Common/common_app_define.h"
#include "../Common//ipc.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <tlhelp32.h>

using			json = nlohmann::json;

#define			TOPOLOGY_CONFIG_FILE				L"topology_config.json"
#define         PREFS_JSON_NODE						"LGTV Companion"
#define         PREFS_JSON_VERSION					"Version"
#define			PREFS_JSON_TOPOLOGY_NODE			"Topology"

// logging macros
#define			INFO(...)							(log_->info("System", __VA_ARGS__))
#define			WARNING(...)						(log_->warning("System", __VA_ARGS__))
#define			ERR(...)							(log_->error("System", __VA_ARGS__))
#define			DEBUG(...)							(log_->debug("System", __VA_ARGS__))
#define			INFO_(t,...)						(log_->info(t, __VA_ARGS__))
#define			WARNING_(t,...)						(log_->warning(t, __VA_ARGS__))
#define			ERR_(t,...)							(log_->error(t, __VA_ARGS__))
#define			DEBUG_(t,...)						(log_->debug(t, __VA_ARGS__))

class SessionWrapper {
public:
	WebOsClient client_;
	Device device_;
	bool topology_enabled_ = false;
	SessionWrapper(boost::asio::io_context& ioc, boost::asio::ssl::context& ctx, Device& dev, Logging& log) 
		: client_(ioc, ctx, dev, log)
	{
		device_ = dev;
	};
	~SessionWrapper() {};
};
class Companion::Impl : public std::enable_shared_from_this<Companion::Impl> {
private:
	boost::asio::io_context								ioc_;
	std::atomic<bool>									ioc_started_;
	std::atomic<bool>									ioc_reset_;
	boost::asio::ssl::context							ctx_{ boost::asio::ssl::context::tlsv12_client };
	Preferences											prefs_;
	std::vector <std::shared_ptr<SessionWrapper>>		sessions_;								
	bool												windows_power_status_on_ = false;		
	bool												remote_client_connected_ = false;		
	bool												screensaver_active_ = false;
	time_t												time_last_resume_or_boot_time = 0;
	time_t												time_last_power_on = 0;
	time_t												time_last_suspend = 0;
	bool												user_idle_mode_log_ = true;
	int													event_log_callback_status_ = NULL;
	std::vector<std::string>							host_ips_;
	std::shared_ptr<Logging>							log_;
	std::shared_ptr<IpcServer>							ipc_server_;

	void												dispatchEvent(Event&);
	void												processEvent(Event&, SessionWrapper&);
	bool												isScreensaverActive(void);
	bool												setHdmiInput(Event&, SessionWrapper&);
	void												enableSession(std::vector<std::string>);
	void												disableSession(std::vector<std::string>);
	std::string											setTopology(std::vector<std::string>);
	void												clearTopology(void);
	std::string											loadSavedTopologyConfiguration(void);
	void												saveTopologyConfiguration(void);
	std::string											validateDevices(std::vector<std::string>);
	static void											ipcCallbackStatic(std::wstring message, LPVOID lpFunct);
	void												ipcCallback(std::wstring message, bool recursive = false);
	void												sendToIpc(DWORD);
	std::vector<std::string>							grabDevices(std::vector<std::string>, int);
	std::vector<std::string>							extractSeparateCommands(std::string);

	void												event(int type, std::vector<std::string> devices = {});
	void												eventRequest(std::vector<std::string> devices, std::string uri, std::string payload, std::string log_message);
	void												eventButton(std::vector<std::string> devices, std::string button, std::string log_message);
	void												eventLunaSystemSetting(std::vector<std::string> devices, std::string category, std::string setting, std::string value, std::string format, std::string log_message);
	void												eventLunaSystemSettingPayload(std::vector<std::string> devices, std::string category, std::string payload, std::string log_message);
	void												eventLunaGeneric(std::vector<std::string> devices, std::string luna, std::string payload, std::string log_message);
	void												eventLunaDeviceInfo(std::vector<std::string> devices, std::string input, std::string icon, std::string label, std::string log_message);

public:
	Impl(Preferences&);
	~Impl();
	void												shutdown(bool);
	void												systemEvent(int event, std::string data = "");
	bool												isBusy(void);
};

Companion::Impl::Impl(Preferences& settings) 
	: prefs_(settings)
{
	ioc_started_.store(false);
	ioc_reset_.store(true);
	prefs_.topology_support_ = false; //disable initially
	std::wstring file = tools::widen(prefs_.data_path_);
	file += LOG_FILE;
	log_ = std::make_shared<Logging>(settings.log_level_, file);
	ipc_server_ = std::make_shared<IpcServer>(PIPENAME, &ipcCallbackStatic, (LPVOID)this);
	if (prefs_.devices_.size() > 0)
	{
		for (auto& device : prefs_.devices_)
			sessions_.push_back(std::make_shared<SessionWrapper>(ioc_, ctx_, device, *log_));
		INFO(" ");
		INFO("--- LGTV Companion Service has started (v %1%) ---------------------------", tools::narrow(APP_VERSION));
		std::string line = prefs_.getAsString();
		if (!line.empty())
		{
			std::istringstream config(line);
			while (std::getline(config, line))
				DEBUG_(tools::narrow(CONFIG_FILE), line);
		}
		else
			DEBUG_(tools::narrow(CONFIG_FILE), "Default configuration loaded");

		if (settings.topology_support_)
		{
			if (prefs_.topology_keep_on_boot_)
			{
				std::string device_state = loadSavedTopologyConfiguration();
				if (device_state == "")
					WARNING("Attempting to restore monitor topology but no devices were configured");
				else
					INFO("Restoring monitor topology: %1%", device_state);
			}
			else
			{
				std::wstring file = tools::widen(prefs_.data_path_);
				file += TOPOLOGY_CONFIG_FILE;
				DeleteFile(file.c_str());
			}
		}
		host_ips_ = tools::getLocalIP();
		if (host_ips_.size() > 0)
		{
			std::string ips;
			for (auto& host_ip : host_ips_)
			{
				ips += host_ip;
				ips += " ";
			}
			DEBUG("Host IP: %1%", ips);
		}
	}
	else
		INFO("No devices has been configured");
}
Companion::Impl::~Impl(void)
{

}
bool Companion::Impl::isBusy(void)
{
	// if io context was never started
	if (!ioc_started_.load())
		return false;
	// if io context is currently in a reset state (this state means .stopped() return false)
	if (ioc_reset_.load())
		return false;
	// if io context is stopped (as a result of running out of work or manually stopped)
	if (ioc_.stopped())
		return false;
	return true;
}
void Companion::Impl::shutdown(bool stage){
	if(!stage)
		INFO("The service has terminated.");
	else
	{
		INFO("Service is shutting down. Finishing activities and closing connections");
		ipc_server_->terminate();
		if(isBusy())
			if (sessions_.size() > 0)
				for (auto& session : sessions_)
					session->client_.close(true);
	}
}
void Companion::Impl::enableSession(std::vector<std::string> device_names_or_ids){
	for (auto& session : sessions_)
	{
		if (device_names_or_ids.size() > 0)
		{
			for (auto& name_or_id : device_names_or_ids)
				if (tools::tolower(session->device_.name) == tools::tolower(name_or_id) || tools::tolower(session->device_.id) == tools::tolower(name_or_id))
					session->device_.enabled = true;
		}
		else
			session->device_.enabled = true;
	}
 }
void Companion::Impl::disableSession(std::vector<std::string> device_names_or_ids){
	for (auto& session : sessions_)
	{
		if(device_names_or_ids.size() > 0)
		{
			for (auto& name_or_id : device_names_or_ids)
				if (tools::tolower(session->device_.name) == tools::tolower(name_or_id) || tools::tolower(session->device_.id) == tools::tolower(name_or_id))
					session->device_.enabled = false;
		}
		else
			session->device_.enabled = false;

	}
}
std::string	Companion::Impl::setTopology(std::vector<std::string> device_names_or_ids){
	if (sessions_.size() == 0)
		return "No devices configured";

	prefs_.topology_support_ = true;

	std::string return_string;
	for (auto& session : sessions_)
	{
		session->topology_enabled_ = false;
		if (device_names_or_ids.size() > 0)
		{
			for (auto& name_or_id : device_names_or_ids)
			{
				if (tools::tolower(session->device_.name) == tools::tolower(name_or_id) || tools::tolower(session->device_.id) == tools::tolower(name_or_id))
					session->topology_enabled_ = true;
			}
		}
		return_string += session->device_.name;
		return_string += ":";
		return_string += session->topology_enabled_ ? "ON " : "OFF ";
	}
	return return_string;
}
void Companion::Impl::clearTopology(void){
	prefs_.topology_support_ = false;
	for (auto& session : sessions_)
		session->topology_enabled_ = false;
}
std::string	 Companion::Impl::loadSavedTopologyConfiguration(void){
	std::wstring file = tools::widen(prefs_.data_path_);
	file += TOPOLOGY_CONFIG_FILE;
	std::ifstream i(file.c_str());
	if (i.is_open())
	{
		try
		{
			nlohmann::json topology_json;
			std::vector<std::string> devices;
			i >> topology_json;		
			// Read version of the preferences file. If this key is found it is assumed the config file has been populated
			if (!topology_json[PREFS_JSON_NODE][PREFS_JSON_VERSION].empty() 
				&& topology_json[PREFS_JSON_NODE][PREFS_JSON_VERSION].is_number())
			{
				if (!topology_json[PREFS_JSON_NODE][PREFS_JSON_TOPOLOGY_NODE].empty()
					&& topology_json[PREFS_JSON_NODE][PREFS_JSON_TOPOLOGY_NODE].size() > 0)
				{
					for (auto& device : topology_json[PREFS_JSON_NODE][PREFS_JSON_TOPOLOGY_NODE].items())
					{
						if (device.value().is_string())
							devices.push_back(device.value().get<std::string>());
					}				
				}
				return setTopology(devices);
			}
		}
		catch (std::exception const& e)
		{
			std::string s = "Error parsing topology configuration file: ";
			s += e.what();
			return s;
		}
		i.close();
		return "Invalid topology configuration file.";
	}
	return "No saved topology configuration.";
}
void Companion::Impl::saveTopologyConfiguration(void){
	if (sessions_.size() == 0)
		return;
	nlohmann::json topology_json;
	std::wstring file = tools::widen(prefs_.data_path_);
	CreateDirectory(file.c_str(), NULL);
	file += TOPOLOGY_CONFIG_FILE;
	topology_json[PREFS_JSON_NODE][PREFS_JSON_VERSION] = (int)prefs_.version_;
	for (auto& session : sessions_)
		if (session->topology_enabled_)
			topology_json[PREFS_JSON_NODE][PREFS_JSON_TOPOLOGY_NODE].push_back(tools::tolower(session->device_.id));
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
std::string Companion::Impl::validateDevices(std::vector<std::string> devices){
	if (sessions_.size() == 0)
		return "no devices configured";

	std::string return_value;
	if (devices.size() == 0)
		return_value = "all devices";
	else
		for (auto& session : sessions_)
			for (auto& device : devices)
			{
				if (tools::tolower(device) == tools::tolower(session->device_.id) || tools::tolower(device) == tools::tolower(session->device_.name))
				{
					return_value += session->device_.id;
					return_value += " ";
				}
			}
	return return_value == "" ? "invalid device id or name" : return_value;
}
void Companion::Impl::dispatchEvent(Event& event) {
	if (sessions_.size() == 0)
		return;
	// fix for only receiving DIMMED event when screensaver is active
	if (event.getType() == EVENT_SYSTEM_DISPLAYDIMMED)
	{
		screensaver_active_ = isScreensaverActive();
		if(screensaver_active_)
			DEBUG("Screensaver is active during DIMMED event");
	}
/*
	//stop io_context when system is resuming when needed to purge lingering work
	if ((event.getType() == EVENT_SYSTEM_RESUME 
		|| event.getType() == EVENT_SYSTEM_RESUMEAUTO) 
		&& time(0) - time_last_resume_or_boot_time > 7
		&& time(0) - time_last_power_on > 7
		&& isBusy())
	{
		ERR("I/O Context did not finish work during the previous system suspend.");
		if (sessions_.size() > 0)
			for (auto& session : sessions_)
				session->client_.close(false);
		time_t entry = time(0);
		while (isBusy() && (time(0) - entry < 2))
			Sleep(200);
		ioc_.stop();
		DEBUG("I/O Context purged and stopped!");
	}
*/
	// process event for ALL devices
	if (event.getDevices().size() == 0)
		for (auto& session : sessions_)
			processEvent(event,  *session);
	// or process event only for matching devices
	else
		for (auto& session : sessions_)
			for (auto& device : event.getDevices())
				if (tools::tolower(device) == tools::tolower(session->device_.id) || tools::tolower(device) == tools::tolower(session->device_.name))
					processEvent(event, *session);
	//post processing stuff
	time_t pp_entry = time(0);
	switch (event.getType())
	{
	case EVENT_SYSTEM_REMOTE_CONNECT:
		remote_client_connected_ = true;
		break;
	case EVENT_SYSTEM_REMOTE_DISCONNECT:
		remote_client_connected_ = false;
		break;
	case EVENT_SYSTEM_SHUTDOWN:
	case EVENT_SYSTEM_UNSURE:
		remote_client_connected_ = false;
		break;

	case EVENT_SYSTEM_SUSPEND:
		remote_client_connected_ = false;
		windows_power_status_on_ = false;
		time_last_suspend = pp_entry;
		
		//buy some time during suspend. Hack which seems to preserve network connectivity during suspend
		SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_CONTINUOUS);
		Sleep(100);
		while (isBusy() && time(0) - pp_entry < 9)
		{
			SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_CONTINUOUS);
			Sleep(100);
		}
		SetThreadExecutionState(ES_CONTINUOUS);
		break;
	case EVENT_SYSTEM_REBOOT:
		remote_client_connected_ = false;
		break;

	case EVENT_SYSTEM_DISPLAYDIMMED:
		// fix for only receiving DIMMED event when screensaver is active
		if (screensaver_active_)
			windows_power_status_on_ = false;
		break;
	case EVENT_SYSTEM_DISPLAYOFF:
		windows_power_status_on_ = false;
		break;
	case EVENT_SYSTEM_RESUME:
		remote_client_connected_ = false;
		windows_power_status_on_ = true;
		user_idle_mode_log_ = true;
		time_last_resume_or_boot_time = pp_entry;
		break;
	case EVENT_SYSTEM_DISPLAYON:
		windows_power_status_on_ = true;
		time_last_power_on = pp_entry;
		break;
	case EVENT_SYSTEM_RESUMEAUTO:
		remote_client_connected_ = false;
		windows_power_status_on_ = true;
		time_last_resume_or_boot_time = pp_entry;
		user_idle_mode_log_ = true;
		break;
	case EVENT_SYSTEM_BOOT:
		time_last_resume_or_boot_time = pp_entry;
		break;
	default:break;
	}
	return;
}
bool Companion::Impl::isScreensaverActive(void)
{
	// Iterate over currently running processes
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (!Process32First(snapshot, &entry)) {
		CloseHandle(snapshot);
		ERR("Failed to iterate running processes in isScreensaverActive()");
		return false;
	}
	do
	{
		std::filesystem::path exe = entry.szExeFile;
		std::string ext = exe.extension().string();
		if (tools::tolower(ext) == ".scr")
		{
			CloseHandle(snapshot);
			return true;
		}
	} while (Process32Next(snapshot, &entry));
	CloseHandle(snapshot);
	return false;
}
bool Companion::Impl::setHdmiInput(Event& event, SessionWrapper& session)
{
	Event change_hdmi_input_event;
	std::string payload = LG_URI_PAYLOAD_SETHDMI;
	std::string log = "set hdmi-input [#ARG#]";
	tools::replaceAllInPlace(payload, "#ARG#", std::to_string(session.device_.sourceHdmiInput));
	tools::replaceAllInPlace(log, "#ARG#", std::to_string(session.device_.sourceHdmiInput));
	change_hdmi_input_event.set(EVENT_REQUEST, event.getDevices(), LG_URI_LAUNCH, payload, log);
	session.client_.sendRequest(change_hdmi_input_event.getData(), log, session.device_.set_hdmi_input_on_power_on_delay);
	return true;
}
void Companion::Impl::processEvent(Event& event, SessionWrapper& session)
{
	bool work_was_enqueued;
	do
	{
		work_was_enqueued = false;

		//reset io_context as needed
		if (ioc_.stopped())
		{
			DEBUG("I/O context is stopped. Resetting!");
			ioc_reset_.store(true);
			ioc_.restart();
		}
		// process forced and user initiated events
		switch (event.getType())
		{
		case EVENT_FORCE_DISPLAYON:
			work_was_enqueued = session.client_.powerOn();
			break;

		case EVENT_FORCE_DISPLAYOFF:
			work_was_enqueued = session.client_.powerOff(true);
			break;

		case EVENT_FORCE_BLANKSCREEN:
			work_was_enqueued = session.client_.blankScreen(true);
			break;

		case EVENT_BUTTON:
			work_was_enqueued = session.client_.sendButton(event.getData());
			break;

		case EVENT_REQUEST:
		case EVENT_LUNA_SYSTEMSET_BASIC:
		case EVENT_LUNA_SYSTEMSET_PAYLOAD:
		case EVENT_LUNA_DEVICEINFO:
		case EVENT_LUNA_GENERIC:
			work_was_enqueued = session.client_.sendRequest(event.getData(), event.getLogMessage());
			break;
		default:break;
		}
		// process events related to automatic management
		if (session.device_.enabled)
		{
			// Process system events
			switch (event.getType())
			{
			case EVENT_SYSTEM_REMOTE_CONNECT:
				if (remote_client_connected_)
					break;
				if (windows_power_status_on_ == true)
				{
					if (prefs_.remote_streaming_host_prefer_power_off_)
						work_was_enqueued = session.client_.powerOff();
					else
						work_was_enqueued = session.client_.blankScreen();
				}
				break;
			case EVENT_SYSTEM_REMOTE_DISCONNECT:
				if (!remote_client_connected_)
					break;
				if (windows_power_status_on_ == true)
				{
					work_was_enqueued = session.client_.powerOn();
					if (prefs_.remote_streaming_host_prefer_power_off_ && session.device_.set_hdmi_input_on_power_on)
						work_was_enqueued = setHdmiInput(event, session);
				}
				break;
			case EVENT_SYSTEM_REBOOT:
			case EVENT_SYSTEM_RESUME:
			case EVENT_SYSTEM_RESUMEAUTO:
			case EVENT_SYSTEM_BOOT:
				break;

			case EVENT_SYSTEM_SHUTDOWN:
			case EVENT_SYSTEM_UNSURE:
				work_was_enqueued = session.client_.powerOff();
				break;

			case EVENT_SYSTEM_DISPLAYON:
				if (remote_client_connected_)
					break;
				if (prefs_.topology_support_ && !session.topology_enabled_)
					break;
				work_was_enqueued = session.client_.powerOn();
				if (session.device_.set_hdmi_input_on_power_on) 
					work_was_enqueued = setHdmiInput(event, session);
				break;

			case EVENT_SYSTEM_SUSPEND:
				if (windows_power_status_on_ == true)
					work_was_enqueued = session.client_.powerOff();
				break;

			case EVENT_SYSTEM_DISPLAYDIMMED:
				//fix for only receiving DIMMED event when screensaver is active
				if (screensaver_active_)
				{
					if (remote_client_connected_)
						break;
					if (windows_power_status_on_ == true)
						work_was_enqueued = session.client_.powerOff();
				}
				break;

			case EVENT_SYSTEM_DISPLAYOFF:
				if (remote_client_connected_ && prefs_.remote_streaming_host_prefer_power_off_)
					break;
				if (windows_power_status_on_ == true)
					work_was_enqueued = session.client_.powerOff();
				break;

			case EVENT_SYSTEM_BLANKSCREEN:
				work_was_enqueued = session.client_.blankScreen();
				break;

			case EVENT_SYSTEM_USERIDLE:
				if (remote_client_connected_)
					break;
				if (prefs_.topology_support_ && !session.topology_enabled_)
					break;
				if (prefs_.user_idle_mode_ && windows_power_status_on_)
					work_was_enqueued = session.client_.blankScreen();
				break;

			case EVENT_SYSTEM_USERBUSY:
				if (remote_client_connected_)
					break;
				if (prefs_.topology_support_ && !session.topology_enabled_)
					break;
				if (prefs_.user_idle_mode_)
					work_was_enqueued = session.client_.powerOn();
				break;

			case EVENT_SYSTEM_TOPOLOGY:
				if (remote_client_connected_)
					break;
				if (prefs_.topology_support_)
					if (session.topology_enabled_)
					{
						work_was_enqueued = session.client_.powerOn();
						if (session.device_.set_hdmi_input_on_power_on)
							work_was_enqueued = setHdmiInput(event, session);
					}
					else
						work_was_enqueued = session.client_.powerOff();
				break;
			default:break;
			}
		}
	} while (ioc_.stopped());
	if (ioc_reset_.load() && work_was_enqueued)
	{
		int threads = 0;
		for (auto& dev : sessions_)
			if (dev->device_.enabled)
				threads++;
		if (threads < 2)
			threads = 2;
		else
			threads++;
		DEBUG("Creating a new thread pool - %1% threads", std::to_string(threads));
		std::vector<std::thread> thread_pool;
		for (int i = 0; i < threads; i++)
		{
			thread_pool.emplace_back(
				[self_ = shared_from_this()]
				{
					SetThreadUILanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
					self_->ioc_.run();
				});
			thread_pool[i].detach();
		}
		ioc_started_.store(true);
		ioc_reset_.store(false);
	}
}
void Companion::Impl::sendToIpc(DWORD dwEvent)
{
	std::wstring data = L"";
	if (!prefs_.external_api_support_)
		return;
	switch (dwEvent)
	{
	case EVENT_SYSTEM_DISPLAYOFF:
		data = L"SYSTEM_DISPLAYS_OFF";
		break;
	case EVENT_SYSTEM_DISPLAYDIMMED:
		data = L"SYSTEM_DISPLAYS_OFF_DIMMED";
		break;
	case EVENT_SYSTEM_DISPLAYON:
		data = L"SYSTEM_DISPLAYS_ON";
		break;
	case EVENT_SYSTEM_USERBUSY:
		data = L"SYSTEM_USER_BUSY";
		break;
	case EVENT_SYSTEM_USERIDLE:
		data = L"SYSTEM_USER_IDLE";
		break;
	case EVENT_SYSTEM_REBOOT:
		data = L"SYSTEM_REBOOT";
		break;
	case EVENT_SYSTEM_UNSURE:
	case EVENT_SYSTEM_SHUTDOWN:
		data = L"SYSTEM_SHUTDOWN";
		break;
	case EVENT_SYSTEM_RESUMEAUTO:
		data = L"SYSTEM_RESUME";
		break;
	case EVENT_SYSTEM_SUSPEND:
		data = L"SYSTEM_SUSPEND";
		break;
	default:break;
	}
	if (data.size() > 0)
		ipc_server_->send(data);
}
std::vector<std::string> Companion::Impl::extractSeparateCommands(std::string str) {
	std::vector<std::string>res;

	size_t f1 = str.find_first_not_of("-", 0);
	if (f1 != std::string::npos)
		str = str.substr(f1);
	else
		return res;

	while (str.size() > 0)
	{
		size_t index = str.find(" -");
		if (index != std::string::npos)
		{
			res.push_back(str.substr(0, index));

			size_t next = str.find_first_not_of(" -", index);
			if (next != std::string::npos)
				str = str.substr(next); //  str.substr(index + token.size());
			else
				str = "";
		}
		else {
			res.push_back(str);
			str = "";
		}
	}
	return res;
}
 void Companion::Impl::ipcCallbackStatic(std::wstring message, LPVOID lpFunct)
{
	 Companion::Impl* p = (Companion::Impl*)lpFunct;
	 p->ipcCallback(message);
}
void Companion::Impl::ipcCallback(std::wstring message, bool recursive)
{
	if (message.length() == 0)
	{
		ERR_("IPC", "Invalid zero length message received!");
		return;
	}

	std::string temp = tools::narrow(message);
	if(!recursive)
		DEBUG_("IPC", "Received IPC/CLI: %1%", temp);

	// remove leading spaces
	size_t first = temp.find_first_not_of(" ", 0);
	if (first != std::string::npos)
		temp = temp.substr(first);
	else
	{
		ERR_("CLI", "Invalid command line format.Zero length!");
		return;
	}
	if (temp.find('-') != 0)
	{
		WARNING_("CLI", "Invalid command line format.Please prefix commands with '-'");
		return;
	}
	// hack to allow for escaped double quotes in arguments
	tools::replaceAllInPlace(temp, "\\\"", "*#*#*#");
	tools::replaceAllInPlace(temp, "\"", "%�%�%�");
	// split into separate command lines and process each
	std::vector <std::string> commands = extractSeparateCommands(temp);
	if (commands.size() == 0)
	{
		WARNING_("CLI", "Invalid command line format. No command was input");
		return;
	}
	for (auto& commandline : commands)
	{
		tools::replaceAllInPlace(commandline, "%�%�%�", "\"");
		std::string log_message = "";
		// split command line into words
		std::vector <std::string> words = tools::stringsplit(commandline, " ");
		size_t nWords = words.size();
		if (nWords == 0)
		{
			WARNING_("CLI", "Invalid command line format (null length)");
			continue;
		}
		for (auto& wrd : words)
			tools::replaceAllInPlace(wrd, "*#*#*#", "\"");
		std::string command = tools::tolower(words[0]);
		//Daemon communication
		if (command == "daemon")										
		{
			if (nWords < 3)
			{
				ERR_("Daemon", "Invalid message. Too few parameters!");
				continue;
			}
			DWORD sessionid = WTSGetActiveConsoleSessionId();
			std::string physical_console = std::to_string(sessionid);
			std::string daemon_id = words[1];
			std::string daemon_command = tools::tolower(words[2]);
			std::string daemon_number = "Daemon ";
			daemon_number += daemon_id;
			
			if (daemon_command == "errorconfig")
			{
				ERR_(daemon_number, "Could not read configuration. Daemon is terminating!");
				continue;
			}
			else if (daemon_command == "started")
			{
				DEBUG_(daemon_number, "Daemon started!");
				continue;
			}
			else if (daemon_command == "remote_connect")
			{
				if (windows_power_status_on_)
					INFO_(daemon_number, "Remote streaming client connected. All managed devices will %1%", prefs_.remote_streaming_host_prefer_power_off_ ? "power OFF" : "be blanked");
				else
					INFO_(daemon_number, "Remote streaming client connected. Global power status is OFF.");
				event(EVENT_SYSTEM_REMOTE_CONNECT);
				continue;
			}
			else if (daemon_command == "remote_disconnect")
			{
				if (windows_power_status_on_)
					INFO_(daemon_number, "Remote streaming client disconnected. All managed devices will power ON");
				else
					INFO_(daemon_number, "Remote streaming client disconnected. Global power status is OFF.");
				event(EVENT_SYSTEM_REMOTE_DISCONNECT);
				continue;
			}
			else if (daemon_command == "newversion")
			{
				INFO_(daemon_number, "A new version of this app is available for download here: %1%", tools::narrow(NEWRELEASELINK));
				continue;
			}
			if (physical_console != daemon_id) // only allow process communications from physical console
			{
				DEBUG_("IPC", "Discarding messages from session: %1%", daemon_id);
				continue;
			}
			else if (daemon_command == "userbusy")
			{
				if (!user_idle_mode_log_)
				{
					INFO_(daemon_number, "User is not idle");
					user_idle_mode_log_ = true;
				}
				event(EVENT_SYSTEM_USERBUSY);
				continue;
			}
			else if (daemon_command == "useridle")
			{
				if (user_idle_mode_log_)
				{
					INFO_(daemon_number, "User is idle");
					user_idle_mode_log_ = false;
				}
				event(EVENT_SYSTEM_USERIDLE);
				continue;
			}
			else if (daemon_command == "topology")
			{
				if (nWords == 3)
				{
					ERR_(daemon_number, "Invalid message. Too few parameters for Topology!");
					continue;
				}
				std::string topology_command = tools::tolower(words[3]);
				if (topology_command == "invalid")
				{
					WARNING_(daemon_number, "A recent change to the system invalidated the monitor topology configuration and the feature has been disabled. "
						"Please run the configuration guide in the global options again to ensure correct operation");
					clearTopology();
					std::wstring file = tools::widen(prefs_.data_path_);
					file += TOPOLOGY_CONFIG_FILE;
					DeleteFile(file.c_str());
					continue;
				}
				else if (topology_command == "undetermined")
				{
					WARNING_(daemon_number, "No active devices detected when verifying Windows Monitor Topology");
					continue;
				}
				else if (topology_command == "state")
				{
					std::vector<std::string> devices = grabDevices(words, 4);
					std::string device_state = setTopology(devices);
					std::string log_message = "Monitor topology was changed - ";
					if (device_state == "")
						log_message += "no devices configured";
					else
						log_message += device_state;

					if (prefs_.topology_keep_on_boot_)
						saveTopologyConfiguration();

					if (windows_power_status_on_ == false)
					{
						log_message += " (monitors globally requested OFF. Not enforcing monitor topology)";
						INFO_(daemon_number, log_message);
						continue;
					}
					INFO_(daemon_number, log_message);
					event(EVENT_SYSTEM_TOPOLOGY);
				}
				else
				{
					ERR_(daemon_number, "Topology command is invalid");
					continue;
				}
			}
			else if (daemon_command == "gfe")
			{
				INFO_(daemon_number, "NVIDIA GFE overlay fullscreen compatibility set");
				continue;
			}
		}
		else if (command == "poweron")									// POWER ON
		{
			INFO_("CLI", "Force power on: %1%", validateDevices(grabDevices(words, 1)));
			event(EVENT_FORCE_DISPLAYON, grabDevices(words, 1));
			continue;
		}
		else if (command == "poweroff")									// POWER OFF
		{
			INFO_("CLI", "Force power off: %1%", validateDevices(grabDevices(words, 1)));
			event(EVENT_FORCE_DISPLAYOFF, grabDevices(words, 1));
			continue;
		}
		else if (command == "autoenable")								// AUTOMATIC MANAGEMENT ENABLED
		{
			INFO_("CLI", "Automatic management is temporarily enabled (effective until restart of service): %1%", validateDevices(grabDevices(words, 1)));
			enableSession(grabDevices(words, 1));
			continue;
		}
		else if (command == "autodisable")								// AUTOMATIC MANAGEMENT DISABLED
		{
			INFO_("CLI", "Automatic management is temporarily disabled (effective until restart of service): %1%", validateDevices(grabDevices(words, 1)));
			disableSession(grabDevices(words, 1));
			continue;
		}
		else if (command == "screenon")									// UNBLANK SCREEN
		{
			INFO_("CLI", "Force screen on: %1%", validateDevices(grabDevices(words, 1)));
			event(EVENT_FORCE_DISPLAYON, grabDevices(words, 1));
			continue;
		}
		else if (command == "screenoff")								// BLANK SCREEN
		{
			INFO_("CLI", "Force screen off: %1%", validateDevices(grabDevices(words, 1)));
			event(EVENT_FORCE_BLANKSCREEN, grabDevices(words, 1));
			continue;
		}
		else if (command.find("sethdmi") == 0)							// SET HDMI INPUT
		{
			int cmd_offset = 1;
			std::string argument;
			if (command.size() > 7)		// -sethdmiX ...
			{
				argument = command.substr(7, 1);
				cmd_offset = 1;
			}
			else if (nWords > 1)			// -sethdmi X ...
			{
				argument = words[1].substr(0, 1);
				cmd_offset = 2;
			}
			else
			{
				WARNING_("CLI", "Failed to set HDMI-input. Missing argument (input number)");
				continue;
			}
			argument = tools::validateArgument(argument, "1 2 3 4");
			if (argument != "")
			{
				std::string payload = LG_URI_PAYLOAD_SETHDMI;
				tools::replaceAllInPlace(payload, "#ARG#", argument);
				DEBUG_("CLI", "Set HDMI input %1%: %2%", argument, validateDevices(grabDevices(words, cmd_offset)));
				log_message = "Set HDMI-input ";
				log_message += argument;
				eventRequest(grabDevices(words, cmd_offset), LG_URI_LAUNCH, payload, log_message);
				continue;
			}
			else
			{
				WARNING_("CLI", "Failed to set HDMI input. Invalid argument (input number)");
				continue;
			}
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
				std::string vol = std::to_string(arg);
				nlohmann::json payload;
				payload["volume"] = arg;
				DEBUG_("CLI", "Volume [%1%]: %2%", vol, validateDevices(grabDevices(words, 2)));
				log_message = "Volume ";
				log_message += vol;
				eventRequest(grabDevices(words, 2), LG_URI_SETVOLUME, payload.dump(), log_message);
			}
			else
				WARNING_("CLI", "Too few arguments for -volume");
			continue;
		}
		else if (command == "mute")										// MUTE DEVICE SOUND
		{
			std::string payload = "{\"mute\":\"true\"}";
			DEBUG_("CLI", "Mute: %1%", validateDevices(grabDevices(words, 1)));
			eventRequest(grabDevices(words, 1), LG_URI_SETMUTE, payload, "Mute speakers");
			continue;
		}
		else if (command == "unmute")									// UNMUTE DEVICE SOUND
		{
			DEBUG_("CLI", "Unmute: %1%", validateDevices(grabDevices(words, 1)));
			eventRequest(grabDevices(words, 1), LG_URI_SETMUTE, "", "Unmute speakers");
			continue;
		}
		else if (command == "freesyncinfo")									// SHOW FREESYNC INFORMATION
		{
			std::vector<std::string> devices = grabDevices(words, 1);
			std::string cmd_line_start_app_with_param = "-start_app_with_param com.webos.app.tvhotkey \"{\\\"activateType\\\": \\\"freesync-info\\\"}\"";
			for (auto& dev : devices)
			{
				cmd_line_start_app_with_param += " ";
				cmd_line_start_app_with_param += dev;
			}
			ipcCallback(tools::widen(cmd_line_start_app_with_param), true);
			continue;
		}
		else if (command == "servicemenu")									// SHOW SERVICE MENU
		{
			std::vector<std::string> devices = grabDevices(words, 1);
			std::string cmd_line_service_menu = "-button IN_START";
			for (auto& dev : devices)
			{
				cmd_line_service_menu += " ";
				cmd_line_service_menu += dev;
			}
			INFO_("CLI", "Showing Service Menu (use code 0413");
			ipcCallback(tools::widen(cmd_line_service_menu), true);
			continue;
		}
		else if (command == "servicemenu_legacy_enable" || command == "servicemenu_legacy_disable")					// ENABLE FULL/LEGACY SERVICE MENU
		{
			std::vector<std::string> devices = grabDevices(words, 1);
			std::string cmd_line_legacy = "-settings_other \"{\\\"svcMenuFlag\\\":";
			cmd_line_legacy += command == "servicemenu_legacy_enable" ? "false}\"" : "true}\"";
			for (auto& dev : devices)
			{
				cmd_line_legacy += " ";
				cmd_line_legacy += dev;
			}
			ipcCallback(tools::widen(cmd_line_legacy), true);
			continue;
		}
		else if (command == "servicemenu_tpc_enable" || command == "servicemenu_tpc_disable")					// ENABLE FULL/LEGACY SERVICE MENU
		{
			nlohmann::json payload;
			payload["enable"] = command == "servicemenu_tpc_enable" ? true : false;
			if(command == "servicemenu_tpc_enable")
			{
				DEBUG_("CLI", "Enable Temporal Peak Contrl (TPC): %1%", validateDevices(grabDevices(words, 1)));
				log_message = "Enable TPC";
			}
			else
			{
				DEBUG_("CLI", "Disable Temporal Peak Contrl (TPC): %1%", validateDevices(grabDevices(words, 1)));
				log_message = "Disable TPC";
			}
			eventLunaGeneric(grabDevices(words, 1), LG_LUNA_SET_TPC, payload.dump(), log_message);
			continue;
		}
		else if (command == "servicemenu_gsr_enable" || command == "servicemenu_gsr_disable")					// ENABLE FULL/LEGACY SERVICE MENU
		{
			nlohmann::json payload;
			payload["enable"] = command == "servicemenu_gsr_enable" ? true : false;

			if (command == "servicemenu_gsr_enable")
			{
				DEBUG_("CLI", "Enable Global Stress Reduction (GSR): %1%", validateDevices(grabDevices(words, 1)));
				log_message = "Enable GSR";
			}
			else
			{
				DEBUG_("CLI", "Disable Global Stress Reduction (GSR): %1%", validateDevices(grabDevices(words, 1)));
				log_message = "Disable GSR";
			}
			eventLunaGeneric(grabDevices(words, 1), LG_LUNA_SET_GSR, payload.dump(), log_message);
			continue;
		}
		else if (command == "clearlog")									// CLEAR LOG
		{
			std::wstring log = tools::widen(prefs_.data_path_);
			log += LOG_FILE;
			DeleteFile(log.c_str());
			continue;
		}
		else if (command == "idle")										// USER IDLE MODE FORCED (INFO ONLY AS IT IS ENFORCED IN DAEMON)
		{
			if (prefs_.user_idle_mode_)
			{
				INFO_("CLI", "User Idle Mode forced");
			}
			else
				WARNING_("CLI", "Can not force user idle mode, as the feature is not enabled in the global options");
			continue;
		}
		else if (command == "unidle")									// USER IDLE MODE DEACTIVATED (INFO ONLY AS IT IS ENFORCED IN DAEMON)
		{
			if (prefs_.user_idle_mode_)
				INFO_("CLI", "User Idle Mode released");
			else
				WARNING_("CLI", "Can not unset User Idle Mode, as the feature is not enabled in the global options");
			continue;
		}
		else if (command == "button")
		{
			if (nWords > 1)
			{
				std::string button = tools::validateArgument(words[1], prefs_.lg_api_buttons);
				if (button != "")
				{
					DEBUG_("CLI", "Virtual remote button press %1%: %2%", button, validateDevices(grabDevices(words, 2)));
					log_message = "button press: ";
					log_message += button;
					eventButton(grabDevices(words, 2), button, log_message);
				}
				else
					WARNING_("CLI", "Invalid button name");
			}
			else
				WARNING_("CLI", "Too few arguments for -button");
			continue;
		}

		else if (command == "settings_picture")
		{
			if (nWords > 1)
			{
				std::string payload = words[1];
				DEBUG_("CLI", "Picture settings: %1%: %2%", payload, validateDevices(grabDevices(words, 2)));
				log_message = "Luna picture request: ";
				log_message += payload;
				eventLunaSystemSettingPayload(grabDevices(words, 2), "picture", payload, log_message);
			}
			else
				WARNING_("CLI", "Insufficient arguments for -settings_picture");
			continue;
		}
		else if (command == "settings_other")
		{
			if (nWords > 1)
			{
				std::string payload = words[1];
				DEBUG_("CLI", "Other settings: %1%: %2%", payload, validateDevices(grabDevices(words, 2)));
				log_message = "Luna other request: ";
				log_message += payload;
				eventLunaSystemSettingPayload(grabDevices(words, 2), "other", payload, log_message);
			}
			else
				WARNING_("CLI", "Insufficient arguments for -settings_other");
			continue;
		}
		else if (command == "settings_options")
		{
			if (nWords > 1)
			{
				std::string payload = words[1];
				DEBUG_("CLI", "Options settings: %1%: %2%", payload, validateDevices(grabDevices(words, 2)));
				log_message = "Luna options request: ";
				log_message += payload;
				eventLunaSystemSettingPayload(grabDevices(words, 2), "option", payload, log_message);
			}
			else
				WARNING_("CLI", "Insufficient arguments for -settings_options");
			continue;
		}
		else if (command == "request")
		{
			if (nWords > 1)
			{
				std::string uri = words[1];
				DEBUG_("CLI", "Request ssap://%1% : %2%", uri, validateDevices(grabDevices(words, 2)));
				log_message = "Request: ssap://";
				log_message += uri;
				eventRequest(grabDevices(words, 2), uri, "", log_message);
			}
			else
				WARNING_("CLI", "Insufficient arguments for -request");
			continue;
		}
		else if (command == "request_with_param")
		{
			if (nWords > 2)
			{
				std::string uri, payload;
				std::vector<std::string> devices;
				try
				{
					uri = words[1];
					payload = nlohmann::json::parse(words[2]).dump();
				}
				catch (std::exception const& e)
				{
					WARNING_("CLI", "Invalid payload JSON in -request_with_param: ", e.what());
					continue;
				}
				log_message = "Request: ssap://";
				log_message += uri;
				log_message += " - payload: ";
				log_message += payload;
				DEBUG_("CLI", "Request ssap://%1% with payload %2%: %3%", uri, payload, validateDevices(grabDevices(words, 3)));
				eventRequest(grabDevices(words, 3), uri, payload, log_message);
			}
			else
				WARNING_("CLI", "Insufficient arguments for -request_with_param");
			continue;
		}
		else if (command == "start_app")
		{
			if (nWords > 1)
			{
				std::string id = words[1];
				DEBUG_("CLI", "Start application %1% : %2%", id, validateDevices(grabDevices(words, 2)));
				log_message = "Start application: ";
				log_message += id;
				nlohmann::json j;
				j["id"] = id;
				eventRequest(grabDevices(words, 2), LG_URI_LAUNCH, j.dump(), log_message);
			}
			else
				WARNING_("CLI", "Insufficient arguments for -start_app");
			continue;
		}
		else if (command == "start_app_with_param")
		{
			if (nWords > 2)
			{
				std::string id = words[1];
				std::string params = words[2];
				nlohmann::json j;
				try
				{
					j["id"] = id;
					j["params"] = nlohmann::json::parse(params);
				}
				catch (std::exception const& e)
				{
					WARNING_("CLI", "Invalid JSON in -start_app_with_param: ", e.what());
					continue;
				}
				log_message = "Start app: ";
				log_message += id;
				log_message += " with params ";
				log_message += params;
				DEBUG_("CLI", "Start application %1% with params %2% : %3%", id, params, validateDevices(grabDevices(words, 3)));
				eventRequest(grabDevices(words, 3), LG_URI_LAUNCH, j.dump(), log_message);
			}
			else
				WARNING_("CLI", "Insufficient arguments for -start_app_with_param");
			continue;
		}
		else if (command == "close_app")
		{
			if (nWords > 1)
			{
				std::string id = words[1];
				DEBUG_("CLI", "Close application %1% : %2%", id, validateDevices(grabDevices(words, 2)));
				log_message = "Close application: ";
				log_message += id;
				nlohmann::json j;
				j["id"] = id;
				eventRequest(grabDevices(words, 2), LG_URI_CLOSE, j.dump(), log_message);
			}
			else
				WARNING_("CLI", "Insufficient arguments for -close_app");
			continue;
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
					DEBUG_("CLI", "Set curvature preset %1%: %2%", preset, validateDevices(grabDevices(words, 2)));
					log_message = "Set curvature: ";
					log_message += preset;
					eventLunaGeneric(grabDevices(words, 2), LG_LUNA_SET_CURVE_PRESET, payload.dump(), log_message);
				}
				else
					WARNING_("CLI", "Invalid arguments for -set_curve_preset");
			}
			else
				WARNING_("CLI", "Insufficient arguments for -set_curve_preset");
			continue;
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
					DEBUG_("CLI", "Adjust %1% to %2%: %3%", preset, argument, validateDevices(grabDevices(words, 3)));
					log_message = "Adjust ";
					log_message += preset;
					log_message += " to ";
					log_message += argument;
					eventLunaGeneric(grabDevices(words, 3), LG_LUNA_ADJUST_CURVE_PRESET, payload.dump(), log_message);
				}
				else
					WARNING_("CLI", "Invalid arguments for -adjust_curve_preset");
			}
			else
				WARNING_("CLI", "Insufficient arguments for -adjust_curve_preset");
			continue;
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
				std::vector<std::string> devices = grabDevices(words, 2);
				std::string cmd1, cmd2, cmd3, cmd4;

				if (argument == "flat" || argument == "0")
				{
					cmd1 = "-set_curve_preset flat";
					for (auto& dev : devices)
					{
						cmd1 += " ";
						cmd1 += dev;
					}
					ipcCallback(tools::widen(cmd1), true);
				}
				else
				{
					cmd1 = "-adjust_curve_preset 2 "; cmd1 += argument;
					cmd2 = "-adjust_curve_preset 3 "; cmd2 += argument;
					cmd3 = "-set_curve_preset 3";
					cmd4 = "-set_curve_preset 2";
					for (auto& dev : devices)
					{
						cmd1 += " ";
						cmd1 += dev;
						cmd2 += " ";
						cmd2 += dev;
						cmd3 += " ";
						cmd3 += dev;
						cmd4 += " ";
						cmd4 += dev;
					}
					ipcCallback(tools::widen(cmd1), true);
					ipcCallback(tools::widen(cmd2), true);
					ipcCallback(tools::widen(cmd3), true);
					ipcCallback(tools::widen(cmd4), true);
				}
			}
			else
				WARNING_("CLI", "Insufficient arguments for -set_curvature");
		}

		else if (command == "set_input_type") // HDMI_1 icon label
		{
			if (nWords > 3)
			{
				std::string input = words[1];
				std::string icon = words[2];
				std::string label = words[3];
				DEBUG_("CLI", "Set input type for input %1% : %2% %3%: %4%", input, icon, label, validateDevices(grabDevices(words, 4)));
				log_message = "Set input type for input ";
				log_message += input;
				log_message += " : ";
				log_message += icon;
				log_message += " ";
				log_message += label;
				eventLunaDeviceInfo(grabDevices(words, 4), input, icon, label, log_message);
			}
			else
				WARNING_("CLI", "Insufficient arguments for -set_input_type");
			continue;
		}
		else
		{
			if (nWords > 1)
			{
				bool found = false;
				for (const auto& item : prefs_.lg_api_commands_json.items())
				{
					if (item.key() == command) // SETTINGS
					{
						found = true;
						std::string category, setting, argument, arguments, logmessage, format;
						int max, min = -1;
						if (item.value()["Category"].is_string())
							category = item.value()["Category"].get<std::string>();
						if (item.value()["Setting"].is_string())
							setting = item.value()["Setting"].get<std::string>();
						if (item.value()["Argument"].is_string())
							arguments = item.value()["Argument"].get<std::string>();
						if (item.value()["LogMessage"].is_string())
							logmessage = item.value()["LogMessage"].get<std::string>();
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
							argument = tools::validateArgument(words[1], arguments);
						}

						if (argument != "")
						{
							tools::replaceAllInPlace(logmessage, "#ARG#", argument);
							log_message+= logmessage;
							log_message += ": %1%";
							DEBUG_("CLI", log_message, validateDevices(grabDevices(words, 2)));
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
								eventLunaSystemSettingPayload(grabDevices(words, 2), category, payload, logmessage);
							}
							else
								eventLunaSystemSetting(grabDevices(words, 2), category, setting, argument, format, logmessage);
						}
						else
						{
							log_message = " ";
							if (arguments != "")
							{
								log_message = "(valid arguments: ";
								log_message += arguments;
								log_message += ")";
							}
							WARNING_("CLI", "Insufficient argument for -%1% [%2%] %3%", command, words[1], log_message );
						}
					}
				}
				if (!found)
					WARNING_("CLI", "Invalid command: %1%", command);

			}
		}
	}
}
void Companion::Impl::event(int type, std::vector<std::string> devices)
{
	Event event;
	if (devices.size() > 0)
		event.set(type, devices);
	else
		event.set(type);
	dispatchEvent(event);
	return;
}
void Companion::Impl::eventRequest(std::vector<std::string> devices, std::string uri, std::string payload, std::string log_message)
{
	Event event;
	event.set(EVENT_REQUEST, devices, uri, payload, log_message);
	dispatchEvent(event);
	return;
}
void Companion::Impl::eventButton(std::vector<std::string> devices, std::string button, std::string log_message)
{
	Event event;
	event.set(EVENT_BUTTON, devices, button, log_message);
	dispatchEvent(event);
	return;
}
void Companion::Impl::eventLunaSystemSettingPayload(std::vector<std::string> devices, std::string category, std::string payload, std::string log_message)
{
	Event event;
	event.set(EVENT_LUNA_SYSTEMSET_PAYLOAD, devices, category, payload, log_message);
	dispatchEvent(event);
	return;
}
void Companion::Impl::eventLunaGeneric(std::vector<std::string> devices, std::string luna, std::string payload, std::string log_message)
{
	Event event;
	event.set(EVENT_LUNA_GENERIC, devices, luna, payload, log_message);
	dispatchEvent(event);
	return;
}
void Companion::Impl::eventLunaDeviceInfo(std::vector<std::string> devices, std::string input, std::string icon, std::string label, std::string log_message)
{
	Event  event;
	event.set(EVENT_LUNA_DEVICEINFO, devices, input, icon, label, log_message);
	dispatchEvent(event);
	return;
}
void Companion::Impl::eventLunaSystemSetting(std::vector<std::string> devices, std::string category, std::string setting, std::string value, std::string format, std::string log_message)
{
	Event event;
	event.set(EVENT_LUNA_SYSTEMSET_BASIC, devices, setting, value, category, format, log_message);
	dispatchEvent(event);
	return;
}
std::vector<std::string> Companion::Impl::grabDevices(std::vector<std::string> CommandWords, int Offset)
{
	if (Offset < CommandWords.size())
		for (int i = 0; i < Offset; i++)
			CommandWords.erase(CommandWords.begin());
	else
		CommandWords.clear();
	return CommandWords;
}
void Companion::Impl::systemEvent(int e, std::string data) {
	int dispatched_event_type = EVENT_UNDEFINED;
	switch (e)
	{
	case EVENT_SHUTDOWN_TYPE_SHUTDOWN:
		DEBUG_("System", "System shutdown detected (%1%)", data);
		event_log_callback_status_ = EVENT_SHUTDOWN_TYPE_SHUTDOWN;
		break;
	case EVENT_SHUTDOWN_TYPE_REBOOT:
		DEBUG_("System", "System restart detected (%1%)", data);
		event_log_callback_status_ = EVENT_SHUTDOWN_TYPE_REBOOT;
		break;
	case EVENT_SHUTDOWN_TYPE_UNSURE:
		WARNING_("System", "Could not detect whether system is shutting down or rebooting. Check 'localised restart strings' in the global options (%1%)", data);
		event_log_callback_status_ = EVENT_SHUTDOWN_TYPE_UNSURE;
		break;
	case EVENT_SHUTDOWN_TYPE_UNDEFINED:
		event_log_callback_status_ = NULL;
		break;
	case EVENT_SHUTDOWN_TYPE_ERROR:
		DEBUG_("System", "Invalid or unexpected XML received in Event Subscription!");
//		event_log_callback_status_ = NULL;
		break;
	case EVENT_SYSTEM_SHUTDOWN:
		if (event_log_callback_status_ == EVENT_SHUTDOWN_TYPE_REBOOT)
		{
			INFO_("PWR","*** System is restarting");
			dispatched_event_type = EVENT_SYSTEM_REBOOT;
		}
		else if (event_log_callback_status_ == EVENT_SHUTDOWN_TYPE_SHUTDOWN)
		{
			INFO_("PWR", "*** System is shutting down");
			dispatched_event_type = EVENT_SYSTEM_SHUTDOWN;
		}
		else if (event_log_callback_status_ == EVENT_SHUTDOWN_TYPE_UNSURE)
		{
			WARNING_("PWR", "Unable to determine whether shutting down or restarting, please check 'localised restart strings' in the UI");
			dispatched_event_type = EVENT_SYSTEM_UNSURE;
		}
		else
		{
			//This does happen sometimes, probably for timing reasons when shutting down the system.
			ERR_("PWR", "Did not receive the anticipated event subscription callback prior to shutting down. Unable to determine if system is shutting down or restarting!");
			dispatched_event_type = EVENT_SYSTEM_UNSURE;
		}
		break;
	case EVENT_SYSTEM_RESUME:
		INFO_("PWR", "*** System is resuming from low power state");
		dispatched_event_type = EVENT_SYSTEM_RESUME;
		break;
	case EVENT_SYSTEM_RESUMEAUTO:
		INFO_("PWR", "*** System is resuming from low power state (Automatic)");
		dispatched_event_type = EVENT_SYSTEM_RESUMEAUTO;
		break;
	case EVENT_SYSTEM_SUSPEND:
		if (event_log_callback_status_ == EVENT_SHUTDOWN_TYPE_REBOOT)
		{
			INFO_("PWR", "*** System is restarting");
			dispatched_event_type = EVENT_SYSTEM_REBOOT;
		}
		else if (event_log_callback_status_ == EVENT_SHUTDOWN_TYPE_SHUTDOWN)
		{
			INFO_("PWR", "*** System is shutting down (suspending to low power mode)");
			dispatched_event_type = EVENT_SYSTEM_SUSPEND;
		}
		else if (event_log_callback_status_ == EVENT_SHUTDOWN_TYPE_UNSURE)
		{
			WARNING_("PWR", "Unable to determine whether shutting down or restarting, please check 'localised restart strings' in the UI");
			dispatched_event_type = EVENT_SYSTEM_UNSURE;
		}
		else
		{
			INFO_("PWR", "*** System is suspending to a low power state (or event log entry is missing)");
			dispatched_event_type = EVENT_SYSTEM_SUSPEND;
		}
		break;
	case EVENT_SYSTEM_DISPLAYOFF:
		INFO_("PWR", "*** System requests displays to power off");
		dispatched_event_type = EVENT_SYSTEM_DISPLAYOFF;
		break;
	case EVENT_SYSTEM_DISPLAYON:
		INFO_("PWR", "*** System requests displays to power on");
		dispatched_event_type = EVENT_SYSTEM_DISPLAYON;
		break;
	case EVENT_SYSTEM_DISPLAYDIMMED:
		INFO_("PWR", "*** System requests displays to power off (dimmed)");
		dispatched_event_type = EVENT_SYSTEM_DISPLAYDIMMED;
		break;
	case EVENT_SYSTEM_BOOT:
		dispatched_event_type = EVENT_SYSTEM_BOOT;
		break;
	default:break;
	}

	if(dispatched_event_type != EVENT_UNDEFINED)
	{
		this->event(dispatched_event_type);
		sendToIpc(dispatched_event_type);
	}
	// Get host IP, if not already available
	if (host_ips_.size() == 0)
	{
		host_ips_ = tools::getLocalIP();
		if (host_ips_.size() > 0)
		{
			std::string ips;
			for (auto& host_ip : host_ips_)
			{
				ips += host_ip;
				ips += " ";
			}
			DEBUG("Host IP: %1%", ips);
		}
	}
	return;
}
Companion::Companion(Preferences& settings)
	: pimpl(std::make_shared<Impl>(settings)) {}
void Companion::systemEvent(int event, std::string data) {
	pimpl->systemEvent(event, data); }
void Companion::shutdown(bool stage) { pimpl->shutdown(stage); }
bool Companion::isBusy(void) { return pimpl->isBusy(); }
