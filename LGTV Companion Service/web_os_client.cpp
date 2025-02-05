#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0603
#define _WIN32_WINNT 0x0603
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "web_os_client.h"
#include "transient_arp.h"
#include "../Common/common_app_define.h"
#include "../Common/lg_api.h" 
#include "../Common/log.h"
#include "../Common/tools.h"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio.hpp>
#include <queue>
#include <fstream>
#include <nlohmann/json.hpp>

// timers
#define			TIMER_LOG_MUTEX						5		// ms wait for thread sync
#define			TIMER_ASYNC_TIMEOUT					2000	// milliseconds before async_connect and async_handshake SSL timeout
#define			TIMER_RETRY_WAIT					2000	// milliseconds to wait before retrying connection
#define			TIMER_POWER_POLL_WAIT				1000	// milliseconds to wait before sending next power state poll
#define			TIMER_WOL_WAIT						1000	// milliseconds to  wait before sending next WOL magic packet

// connection port
#define			PORT								"3000"
#define			PORT_SSL							"3001"

//socket_status
#define			SOCKET_DISCONNECTED					0
#define			SOCKET_CONNECTING					1
#define			SOCKET_CONNECTED					2

//work_type
#define			WORK_UNDEFINED						0
#define			WORK_POWER_ON						1
#define			WORK_POWER_OFF						2
#define			WORK_BLANK_SCREEN					3
#define			WORK_REQUEST						4
#define			WORK_BUTTON							5
#define			WORK_CLOSE							6
#define			WORK_KEEPALIVE						7
#define			WORK_REQUEST_DELAYED				8

#define			LOG_KEEPALIVE						false

// logging macros
#define			INFO(...)							(log_->info(device_settings_.name, __VA_ARGS__))
#define			WARNING(...)						(log_->warning(device_settings_.name, __VA_ARGS__))
#define			ERR(...)							(log_->error(device_settings_.name, __VA_ARGS__))
#define			DEBUG(...)							(log_->debug(device_settings_.name, __VA_ARGS__))

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using udp = boost::asio::ip::udp;       // from <boost/asio/ip/udp.hpp>
using json = nlohmann::json;

class Work {						
public:
	int									type_ = WORK_UNDEFINED;
	std::string							data_;
	time_t								timestamp_enqueue_ = 0;
	time_t								timestamp_start_ = 0;
	time_t								timestamp_retry_ = 0;
	int									retry_count_ = 0;
	std::string							status_arp_override_for_wol_;
	bool								forced_ = false;
	std::string							log_message = "";
	int									delay_ = 0;
	void clear(void)
	{
		type_ = WORK_UNDEFINED;
		data_ = "";
		timestamp_start_ = 0;
		timestamp_retry_ = 0;
		timestamp_enqueue_ = 0;
		retry_count_ = 0;
		status_arp_override_for_wol_ = "";
		forced_ = false;
		log_message = "";
		delay_ = 0;
	}
};
class WebOsClient::Impl : public std::enable_shared_from_this<WebOsClient::Impl> {
private:
	Device device_settings_;
	Work work_;
	net::deadline_timer keep_alive_timer_;
	net::deadline_timer async_timer_;
	net::deadline_timer wol_timer_;
	net::deadline_timer delayed_request_timer_;
	ssl::context* ctx_;
	udp::socket udp_socket_;
	tcp::resolver resolver_;
	std::optional<beast::websocket::stream<beast::tcp_stream>> ws_tcp_;
	std::optional<beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>> ws_;
	beast::flat_buffer buffer_;
	std::string webos_handshake_;
	std::string host_;
	int socket_status_ = SOCKET_DISCONNECTED;
	bool isPoweredOn_ = false;
	std::list<Work> workQueue_;
	std::shared_ptr<Logging> log_;

	//functions running on strand
	bool setSessionKey(std::string, std::string);
	bool makeMagicPacket(std::string, BYTE*, unsigned char*);
	void startNextWork(void);
	void workIsFinished(void);
	void doClose(void);
	void run(void);
	void runWOL(void);
	void send(std::string data, std::string log_message = "");
	void read(void);

	//Handlers running on strand
	void onResolve(boost::beast::error_code, boost::asio::ip::tcp::resolver::results_type);
	void onConnect(boost::beast::error_code, boost::asio::ip::tcp::resolver::results_type::endpoint_type);
	void onSSLhandshake(boost::beast::error_code);
	void onWinsockHandshake(boost::beast::error_code);
	void onWrite(boost::beast::error_code, std::size_t);
	void onRead(boost::beast::error_code, std::size_t);
	void onClose(boost::beast::error_code);
	void onWait(boost::beast::error_code);
	void onDelayedRequest(boost::beast::error_code);
	void onError(boost::beast::error_code&, std::string);
	void onRetryConnection(boost::beast::error_code);
	void onWOL(boost::beast::error_code);
	void onKeepAlive(boost::beast::error_code);
public:

	explicit Impl(net::io_context&, ssl::context&, Device&, Logging&);
	~Impl() {};
	void close(void);
	void enqueueWork(Work&);
};
WebOsClient::Impl::Impl(net::io_context& ioc, ssl::context& ctx, Device& settings, Logging& log)
	: resolver_(net::make_strand(ioc))
	, async_timer_(net::make_strand(ioc))
	, wol_timer_(net::make_strand(ioc))
	, keep_alive_timer_(net::make_strand(ioc))
	, delayed_request_timer_(net::make_strand(ioc))
	, udp_socket_(net::make_strand(ioc))
{
	ctx_ = &ctx;
	device_settings_ = settings;
	std::string path = settings.extra.data_path;
	path += tools::narrow(LOG_FILE);
	log_ = std::make_shared<Logging>(log);
	if(device_settings_.ssl)
		ws_.emplace(resolver_.get_executor(), *ctx_);
	else
		ws_tcp_.emplace(resolver_.get_executor());
	if (device_settings_.session_key == "") // build the WebOS handshake
		webos_handshake_ = tools::narrow(LG_HANDSHAKE_NOTPAIRED);
	else
	{
		webos_handshake_ = tools::narrow(LG_HANDSHAKE_PAIRED);
		tools::replaceAllInPlace(webos_handshake_, "#CLIENTKEY#", device_settings_.session_key);
	}

}
void WebOsClient::Impl::close(void) {
	net::dispatch(resolver_.get_executor(), beast::bind_front_handler(&Impl::doClose, shared_from_this()));
}
void WebOsClient::Impl::enqueueWork(Work& work)
{
	if(work.type_ != WORK_KEEPALIVE || LOG_KEEPALIVE)
		DEBUG("Enqueueing work of type: %1%", std::to_string(work.type_));
	work.timestamp_enqueue_ = time(0);
	net::dispatch(resolver_.get_executor(), [unit = work, self = shared_from_this()]() mutable
		{
			if (unit.type_ == WORK_POWER_ON ) //&& !unit.forced_)
			{
				// dequeue all currently queued WORK_POWER_OFF
				self->workQueue_.erase(std::remove_if(self->workQueue_.begin(), self->workQueue_.end(), [](Work a) {
					return a.type_ == WORK_POWER_OFF; // && !a.forced_;
					}), self->workQueue_.end());
				
				//don't add WORK_POWER_ON twice in a row
				if (!self->workQueue_.empty() && self->workQueue_.back().type_ == WORK_POWER_ON ) //&& !self->workQueue_.back().forced_)
				{
					self->startNextWork();
					return;
				}
			}

			self->workQueue_.emplace_back(std::move(unit));
			self->startNextWork();
		});
}
void WebOsClient::Impl::send(std::string data, std::string log_message) {
	if (device_settings_.ssl)
	{
		if(work_.type_ != WORK_KEEPALIVE || LOG_KEEPALIVE)
		{
			if(log_message == "")
				DEBUG("> > > SEND > > >: %1%", data);
			else
				DEBUG("> > > SEND > > >: [%1%]", log_message);
		}
		ws_->async_write(net::buffer(data), beast::bind_front_handler(&Impl::onWrite, shared_from_this()));
	}
	else
	{
		if(work_.type_ != WORK_KEEPALIVE || LOG_KEEPALIVE)
		{
			if(log_message == "")
				DEBUG("> > > SEND (non-ssl) > > >: %1%", data);
			else
				DEBUG("> > > SEND (non-ssl) > > >: [%1%]", log_message);
		}
		ws_tcp_->async_write(net::buffer(data), beast::bind_front_handler(&Impl::onWrite, shared_from_this()));
	}	
}
void WebOsClient::Impl::read(void) {
	if(device_settings_.ssl)
		ws_->async_read(buffer_, beast::bind_front_handler(&Impl::onRead, shared_from_this()));
	else
		ws_tcp_->async_read(buffer_, beast::bind_front_handler(&Impl::onRead, shared_from_this()));
}
void WebOsClient::Impl::startNextWork(void)
{
	if (work_.type_ != WORK_UNDEFINED) // work is already ongoing
	{
		time_t time_diff = time(0) - work_.timestamp_start_;
		if (work_.type_ == WORK_POWER_ON && time_diff > (device_settings_.extra.timeout + 10)
			|| (work_.type_ != WORK_POWER_ON && time_diff > 10 + work_.delay_))
		{
			ERR("Lingering work of type %1% detected. Closing connection and aborting work!", std::to_string(work_.type_));
			doClose();
		}
		else
		{
			return; // wait for the current work to end
		}
	}
	keep_alive_timer_.cancel();
	if (socket_status_ == SOCKET_CONNECTING)
	{
		ERR("Socket status is CONNECTING while starting new work, which is undefined. Closing connection and aborting work!");
		doClose(); 
	}
	
	else if (!workQueue_.empty()) {
		work_ = std::move(workQueue_.front());
		workQueue_.pop_front();
		work_.timestamp_start_ = time(0);
		switch (work_.type_)
		{
		case WORK_KEEPALIVE:
			if(LOG_KEEPALIVE)
			{
				DEBUG("---  Starting work: KEEP ALIVE  -----------------");
			}
			if (socket_status_ == SOCKET_CONNECTED)
				send(JSON_GETPOWERSTATE);
			else
				workIsFinished();
			break;
		case WORK_REQUEST:
			DEBUG("---  Starting work: REQUEST  -----------------");
			if (socket_status_ == SOCKET_CONNECTED)
				send(work_.data_);
			else
				this->run();
			break;
	
		case WORK_REQUEST_DELAYED:		
			DEBUG("---  Starting work: REQUEST (Delayed %1% seconds) -----------------", std::to_string(work_.delay_));
			delayed_request_timer_.expires_from_now(boost::posix_time::seconds(work_.delay_));
			delayed_request_timer_.async_wait(beast::bind_front_handler(&Impl::onDelayedRequest, shared_from_this()));
			break;
		case WORK_BUTTON:
			DEBUG("---  Starting work: BUTTON  -----------------");
			if (socket_status_ == SOCKET_CONNECTED)
				send(JSON_GETBUTTONSOCKET);
			else
				this->run();
			break;

		case WORK_CLOSE:
			doClose();
			break;

		case WORK_POWER_ON:
			DEBUG("---  Starting work: POWER ON  -----------------");
			if (socket_status_ == SOCKET_CONNECTED)
				send(JSON_GETPOWERSTATE);
			else
				this->run();
			this->runWOL();
			break;
		case WORK_POWER_OFF:
			DEBUG("---  Starting work: POWER OFF  -----------------");
			if (socket_status_ == SOCKET_CONNECTED)
				send(JSON_GETPOWERSTATE);
			else
				this->run();
			break;

		case WORK_BLANK_SCREEN:
			DEBUG("---  Starting work: BLANK SCREEN  -----------------");
			if (socket_status_ == SOCKET_CONNECTED)
				send(JSON_GETPOWERSTATE);
			else
				this->run();
			break;

		default:
			ERR("New work unit is undefined. Discarding work unit and starting the next one!");
			work_.clear();
			startNextWork();
			break;
		}
	}
	else
	{
		if(device_settings_.enabled && socket_status_ == SOCKET_CONNECTED && isPoweredOn_)
		{
			if (device_settings_.persistent_connection_level == PERSISTENT_CONNECTION_ON)
			{
				DEBUG("Work queue is empty");
			}
			else if (device_settings_.persistent_connection_level == PERSISTENT_CONNECTION_KEEPALIVE)
			{
				keep_alive_timer_.expires_from_now(boost::posix_time::seconds(60));
				keep_alive_timer_.async_wait(beast::bind_front_handler(&Impl::onKeepAlive, shared_from_this()));
			}
			else // no persistence
			{
				DEBUG("Work queue is empty");;
				doClose();
			}
		}
		else
		{
			DEBUG("Work queue is empty");
			doClose();
		}
	}
}
void WebOsClient::Impl::workIsFinished(void)
{
	work_.clear();
//	Sleep(20); // potentially webOS does not like requests to be sent too quickly
	startNextWork();
}
void WebOsClient::Impl::doClose(void) {
	workQueue_ = {}; //clear work queue
	work_.clear();
	switch (socket_status_)
	{
	case SOCKET_DISCONNECTED:
		DEBUG("Closing connection. Socket is already closed");
		break;
	default:
		DEBUG("Closing connection");
		if(device_settings_.ssl)
			ws_->async_close(websocket::close_code::normal, beast::bind_front_handler(&Impl::onClose, shared_from_this()));
		else
			ws_tcp_->async_close(websocket::close_code::normal, beast::bind_front_handler(&Impl::onClose, shared_from_this()));
		break;
	}
}
void WebOsClient::Impl::run(void) {
	socket_status_ = SOCKET_CONNECTING;
	if(device_settings_.ssl)
		ws_.emplace(resolver_.get_executor(), *ctx_);
	else
		ws_tcp_.emplace(resolver_.get_executor());
	host_ = device_settings_.ip;
	work_.timestamp_retry_ = time(0);
	resolver_.async_resolve(host_.c_str(),device_settings_.ssl ? PORT_SSL : PORT, beast::bind_front_handler(&Impl::onResolve, shared_from_this()));
}
void WebOsClient::Impl::onResolve(beast::error_code ec, tcp::resolver::results_type results) {
	if (ec)
		return onError(ec, "onResolve");
	socket_status_ = SOCKET_CONNECTING;
	if(device_settings_.ssl)
	{
		beast::get_lowest_layer(*ws_).expires_after(std::chrono::milliseconds(work_.type_ == WORK_POWER_OFF ? 200 : TIMER_ASYNC_TIMEOUT));
		beast::get_lowest_layer(*ws_).async_connect(results, beast::bind_front_handler(&Impl::onConnect, shared_from_this()));
	}
	else
	{
		beast::get_lowest_layer(*ws_tcp_).expires_after(std::chrono::milliseconds(work_.type_ == WORK_POWER_OFF ? 200 : TIMER_ASYNC_TIMEOUT));
		beast::get_lowest_layer(*ws_tcp_).async_connect(results, beast::bind_front_handler(&Impl::onConnect, shared_from_this()));
	}
}
void WebOsClient::Impl::onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
	if (ec)
		return onError(ec, "onConnect");
	socket_status_ = SOCKET_CONNECTING;
	if(device_settings_.ssl)
	{
		beast::get_lowest_layer(*ws_).expires_after(std::chrono::milliseconds(work_.type_ == WORK_POWER_OFF ? 200 : TIMER_ASYNC_TIMEOUT));
		if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), device_settings_.ip.c_str())) // Set SNI Hostname
		{
			ec = beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
			return onError(ec, "Failed to set SNI hostname");
		}
		host_ += ':' + std::to_string(ep.port());
		ws_->next_layer().async_handshake(ssl::stream_base::client, beast::bind_front_handler(&Impl::onSSLhandshake, shared_from_this()));
	}
	else
	{
		beast::get_lowest_layer(*ws_tcp_).expires_after(std::chrono::milliseconds(work_.type_ == WORK_POWER_OFF ? 200 : TIMER_ASYNC_TIMEOUT));
		ws_tcp_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
		ws_tcp_->set_option(websocket::stream_base::decorator(
			[](websocket::request_type& req)
			{
				req.set(http::field::user_agent,
				std::string(BOOST_BEAST_VERSION_STRING) +
				" websocket-client-async");
			}));
		host_ += ':' + std::to_string(ep.port());
		ws_tcp_->async_handshake(host_, "/", beast::bind_front_handler(&Impl::onWinsockHandshake, shared_from_this()));
	}
}
void WebOsClient::Impl::onSSLhandshake(beast::error_code ec) {
	if (ec)
		return onError(ec, "onSSLhandshake");
	socket_status_ = SOCKET_CONNECTING;
	beast::get_lowest_layer(*ws_).expires_never();;
	ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
	ws_->set_option(websocket::stream_base::decorator(
		[](websocket::request_type& req)
		{
			req.set(http::field::user_agent,
			std::string(BOOST_BEAST_VERSION_STRING) +
			" websocket-client-async-ssl");
		}));
	ws_->async_handshake(host_, "/", beast::bind_front_handler(&Impl::onWinsockHandshake,shared_from_this()));
}
void WebOsClient::Impl::onWinsockHandshake(beast::error_code ec) {
	if (ec)
		return onError(ec, "onWinsockHandshake");
	socket_status_ = SOCKET_CONNECTING;
//	read();
	send(webos_handshake_, "webOS handshake");
}
void WebOsClient::Impl::onKeepAlive(beast::error_code ec) {
	if (ec)
	{
		if (LOG_KEEPALIVE)
			DEBUG("Error in onKeepAlive: %1%", ec.message());
		return;
	}
	if(socket_status_ == SOCKET_CONNECTED && work_.type_ == WORK_UNDEFINED)
	{
		Work work;
		work.type_ = WORK_KEEPALIVE;
		enqueueWork(work);
	}
	else
		DEBUG("Terminating KEEP ALIVE");
}

void WebOsClient::Impl::onWrite(beast::error_code ec, std::size_t bytes_transferred) {
	boost::ignore_unused(bytes_transferred);
	if (ec)
		return onError(ec, "onWrite");
	if (socket_status_ == SOCKET_CONNECTING)
		read();
//	socket_status_ = SOCKET_CONNECTED;
}
void WebOsClient::Impl::onRead(beast::error_code ec, std::size_t bytes_transferred) {
	std::stringstream logmsg;
	json response;
	json payload;
	std::string response_id;
	std::string response_type;
	boost::ignore_unused(bytes_transferred);
	if (ec)
		return onError(ec, "onRead");
	socket_status_ = SOCKET_CONNECTED;
	if(work_.type_ != WORK_KEEPALIVE || LOG_KEEPALIVE)
	{
		if (device_settings_.ssl)
			DEBUG("< < < RECV < < <: %1%", beast::buffers_to_string(buffer_.data()));
		else
			DEBUG("< < < RECV (non-ssl) < < <: %1%", beast::buffers_to_string(buffer_.data()));
	}
	try
	{
		response = json::parse(static_cast<const char*>(buffer_.data().data()), static_cast<const char*>(buffer_.data().data()) + buffer_.size());
		payload = response["payload"];
		if (!response["id"].empty())
			if(response["id"].is_number())
			{
				int id = response["id"].get<int>();
				response_id = std::to_string(id);
			}
			else
				response_id = response["id"];
		response_type = response["type"];
	}
	catch (std::exception const& e)
	{
		buffer_.consume(buffer_.size());
		ERR("Aborting work unit due to invalid JSON received: %1%", e.what());
		ERR("Invalid data received: %1%", beast::buffers_to_string(buffer_.data()));
		workIsFinished();
		read();
		return;
	}
	buffer_.consume(buffer_.size());
	if(response_id == "register_0") // WebOS Handshake
	{ 
		if(response_type == "registered") // pairing is OK
		{
			if (device_settings_.session_key == "" && !payload["client-key"].empty() && payload["client-key"].is_string()) // Received a new pairing key
			{
				device_settings_.session_key = payload["client-key"];
				INFO("Pairing key received: %1%", device_settings_.session_key);
				webos_handshake_ = tools::narrow(LG_HANDSHAKE_PAIRED);
				tools::replaceAllInPlace(webos_handshake_, "#CLIENTKEY#", device_settings_.session_key);
				setSessionKey(device_settings_.session_key, device_settings_.id); // Save session key to config file
				// enable WOL after pairing has succeded
				Work work;
				work.type_ = WORK_REQUEST;
				work.data_ = JSON_LUNA_SET_WOL;
				work.log_message = "WOL has been automatically enabled on the device";
				enqueueWork(work);

			}
			if (work_.type_ == WORK_POWER_ON)
				send(JSON_GETPOWERSTATE);
			else if (work_.type_ == WORK_POWER_OFF)
				send(JSON_GETPOWERSTATE);
			else if (work_.type_ == WORK_BLANK_SCREEN)
				send(JSON_GETPOWERSTATE);
			else if (work_.type_ == WORK_KEEPALIVE)
				send(JSON_GETPOWERSTATE);
			else if (work_.type_ == WORK_REQUEST)
				send(work_.data_);
			else if (work_.type_ == WORK_BUTTON)
				send(JSON_GETBUTTONSOCKET);
			else
			{
				ERR("Undefined work unit. Cancelling current work unit!");
				workIsFinished();
			}
		}
		else if (response_type == "error")
		{
			INFO("User rejected or cancelled the pairing prompt");
			workIsFinished();
		}
		else // Device is unregistered or pairing key is invalid.
		{
			if(device_settings_.session_key != "") // Pairing key is invalid. Force re-pairing
			{
				WARNING("Pairing key was invalid. Re-pairing...");
				device_settings_.session_key = "";
				webos_handshake_ = tools::narrow(LG_HANDSHAKE_NOTPAIRED);
				send(webos_handshake_);
			}
		}
	}
	else if (response_type == "response")
	{
		switch (work_.type_)
		{
		case WORK_POWER_ON:
		{
			if (response_id == "getPowerState" && !payload["state"].empty() && payload["state"].is_string()) // query power state
			{
				if (payload["state"] == "Active" && payload["processing"].empty()) // Device is ON
				{
					if (device_settings_.extra.user_idle_mode_mute_speakers)
						send(JSON_GETAUDIOSTATUS);
					else
					{
						INFO("Power state is ON");
						isPoweredOn_ = true;
						workIsFinished();
					}
				}
				else if (payload["state"] == "Screen Off") // Device is ON, but screen is blanked
					send(JSON_UNBLANK);
				else if (payload["state"] == "Active Standby" && payload["processing"].empty()) // Device is in standby and can be powered on
					send(JSON_POWERTOGGLE);
				else
				{
					async_timer_.expires_from_now(boost::posix_time::milliseconds(TIMER_POWER_POLL_WAIT));
					async_timer_.async_wait(beast::bind_front_handler(&Impl::onWait, shared_from_this()));
				}
			}
			else if (response_id == "unblankScreen")
			{
				if (device_settings_.extra.user_idle_mode_mute_speakers && !payload["returnValue"].empty() && payload["returnValue"].is_boolean() && payload["returnValue"].get<bool>())
					send(JSON_GETAUDIOSTATUS);
				else
				{
					INFO("Power state is ON");
					isPoweredOn_ = true;
					workIsFinished();
				}
			}
			else if (response_id == "powerToggle")
			{
				async_timer_.expires_from_now(boost::posix_time::milliseconds(TIMER_POWER_POLL_WAIT));
				async_timer_.async_wait(beast::bind_front_handler(&Impl::onWait, shared_from_this()));
			}
			else if (response_id == "unmute")
			{
				INFO("Power state is ON");
				isPoweredOn_ = true;
				workIsFinished();
			}
			else if (response_id == "getAudioStatus")
			{
				if (!payload["mute"].empty() && payload["mute"].is_boolean() && payload["mute"].get<bool>())
					send(JSON_UNMUTE);
				else
				{
					INFO("Power state is ON");
					isPoweredOn_ = true;
					workIsFinished();
				}
			}
			else
			{
				ERR("Unmanaged response during POWER ON: %1%. Aborting!", response_id);
				workIsFinished();
			}
		}break;
		case WORK_POWER_OFF:
		{
			if (response_id == "getPowerState" && !payload["state"].empty() && payload["state"].is_string()) // query power state
			{

				if (payload["state"] != "Active" && payload["state"] != "Screen Off") // Device is not ON
				{
					INFO("Power state is OFF");
					isPoweredOn_ = false;
					workIsFinished();
				}
				else
				{
					if(device_settings_.check_hdmi_input_when_power_off && !work_.forced_)
						send(JSON_GETFOREGROUNDAPP);
					else
						send(JSON_POWERTOGGLE);
				}
			}
			else if (response_id == "getForegroundApp" && !payload["appId"].empty())
			{
				boost::string_view foregroundApp = payload["appId"];
				boost::string_view prefix = "com.webos.app.hdmi";
				std::stringstream logmsg;
				if (foregroundApp.starts_with(prefix))
				{
					foregroundApp.remove_prefix(prefix.size());
					if (std::to_string(device_settings_.sourceHdmiInput) == foregroundApp)
					{
						INFO("HDMI input %1% is active. Device will be turned off", std::string(foregroundApp));
						send(JSON_POWERTOGGLE);
					}
					else
					{
						INFO("HDMI input %1% is active. Device will not be turned off", std::string(foregroundApp));
						workIsFinished();
					}
				}
				else
				{
					INFO("Foreground app is %1%. Device will not be turned off", std::string(foregroundApp));
					workIsFinished();
				}
			}
			else if (response_id == "powerToggle")
			{
				INFO("Power state is OFF");
				isPoweredOn_ = false;
				workIsFinished();
			}
			else
			{
				ERR("Unmanaged response during POWER OFF: %1%. Aborting!", response_id);
				workIsFinished();
			}
		}break;
		case WORK_BLANK_SCREEN:
		{
			if (response_id == "getPowerState" && !payload["state"].empty() && payload["state"].is_string()) // query power state
			{
				if (payload["state"] == "Active") // Device is ON and not blanked
				{
					if (device_settings_.check_hdmi_input_when_power_off && !work_.forced_)
						send(JSON_GETFOREGROUNDAPP);
					else
						send(JSON_BLANK);
				}
				else
				{
					INFO("Power state is ON (screen is blanked)");
					workIsFinished();
				}
			}
			else if (response_id == "getForegroundApp" && !payload["appId"].empty())
			{
				boost::string_view foregroundApp = payload["appId"];
				boost::string_view prefix = "com.webos.app.hdmi";
				std::stringstream logmsg;
				if (foregroundApp.starts_with(prefix))
				{
					foregroundApp.remove_prefix(prefix.size());
					if (std::to_string(device_settings_.sourceHdmiInput) == foregroundApp)
					{
						INFO("HDMI input %1% is active. Screen will be blanked", std::string(foregroundApp));
						send(JSON_BLANK);
					}
					else
					{
						INFO("HDMI input %1% is active. Screen will not be blanked", std::string(foregroundApp));
						workIsFinished();
					}
				}
				else
				{
					INFO("Foreground app is %1%. Screen will not be blanked", std::string(foregroundApp));
					workIsFinished();
				}
			}
			else if (response_id == "blankScreen")
			{
				if (device_settings_.extra.user_idle_mode_mute_speakers && !payload["returnValue"].empty() && payload["returnValue"].is_boolean() && payload["returnValue"].get<bool>())
				{

					send(JSON_MUTE);
				}
				else
				{
					INFO("Power state is ON (screen is blanked)");
					workIsFinished();
				}
			}
			else if (response_id == "mute")
			{
				INFO("Power state is ON (screen is blanked)");
				workIsFinished();
			}
			else
			{
				ERR("Unmanaged response during BLANK_SCREEN: %1% . Aborting!", response_id);
				workIsFinished();
			}
		}break;
		case WORK_REQUEST:
		{
			if (response_id == "luna_request")
			{
				if (!payload["returnValue"].empty()
					&& payload["returnValue"].is_boolean()
					&& payload["returnValue"].get<bool>()
					&& !payload["alertId"].empty()
					&& payload["alertId"].is_string())
				{
					std::string id = payload["alertId"].get<std::string>();
					size_t start = id.find('-', 0);
					if (start != std::string::npos && start + 1 < id.size())
					{

						id = id.substr(start + 1);
						if (id.size() > 0)
						{
							std::string close_alert_request = JSON_LUNA_CLOSE_ALERT;
							tools::replaceAllInPlace(close_alert_request, "#ARG#", id);
							send(close_alert_request);
						}
						else
						{
							ERR("Invalid response to LUNA REQUEST. Aborting!");
							workIsFinished();
						}
					}
					else
					{
						ERR("Invalid response to LUNA REQUEST. Aborting!");
						workIsFinished();
					}
				}
				else
				{ 
					ERR("Invalid response to LUNA REQUEST. Aborting!");
					workIsFinished();
				}
			}
			else if (response_id == "closeLunaAlert")
			{
				INFO(work_.log_message);
				workIsFinished();
			}
			else if (response_id != "request")
			{
				ERR("Unmanaged response during REQUEST: %1%", response_id);
				workIsFinished();
			}
			else
			{
				INFO(work_.log_message);
				workIsFinished();
			}

		}break;
		case WORK_BUTTON:
		{
			if (response_id == "getButtonSocket")
			{
				if (!payload["returnValue"].empty() 
					&& payload["returnValue"].is_boolean() 
					&& payload["returnValue"].get<bool>() 
					&& !payload["socketPath"].empty()
					&& payload["socketPath"].is_string() )
				{
					std::string path = payload["socketPath"];
					size_t pos = path.find("/resources");
					if (pos != std::string::npos)
					{
						path = path.substr(pos);
						std::string button_command = "type:button\nname:";
						button_command += work_.data_;
						button_command += "\n\n";
	
						beast::error_code ec;

						beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_button(resolver_.get_executor(), *ctx_);
						beast::websocket::stream<beast::tcp_stream> ws_button_tcp(resolver_.get_executor());
						auto const results = resolver_.resolve(device_settings_.ip, device_settings_.ssl ? PORT_SSL : PORT, ec);
						if(!ec)
						{
							if (device_settings_.ssl)
								beast::get_lowest_layer(ws_button).connect(results, ec);
							else
								beast::get_lowest_layer(ws_button_tcp).connect(results, ec);
						}
						if(!ec)
						{
							if(device_settings_.ssl)
							{
								if (!SSL_set_tlsext_host_name(ws_button.next_layer().native_handle(), device_settings_.ip.c_str()))
									ec = beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
								if (!ec)
									ws_button.next_layer().handshake(ssl::stream_base::client, ec); //SSL handshake
								if (!ec)
									ws_button.set_option(websocket::stream_base::decorator(
										[](websocket::request_type& req)
										{
											req.set(http::field::user_agent,
											std::string(BOOST_BEAST_VERSION_STRING) +
											" websocket-client-async-ssl");
										}));
							}
							else
							{
								if (!ec)
									ws_button_tcp.set_option(websocket::stream_base::decorator(
										[](websocket::request_type& req)
										{
											req.set(http::field::user_agent,
											std::string(BOOST_BEAST_VERSION_STRING) +
											" websocket-client-async");
										}));
							}

							if (!ec)
							{
								if(device_settings_.ssl)
									ws_button.handshake(host_, path, ec);
								else
									ws_button_tcp.handshake(host_, path, ec);
							}
							if (!ec)
							{
								if (device_settings_.ssl)
									ws_button.write(net::buffer(std::string(button_command)), ec);
								else
									ws_button_tcp.write(net::buffer(std::string(button_command)), ec);
							}
							if(device_settings_.ssl)
							{
								if (ws_button.is_open())
									ws_button.close(websocket::close_code::normal);
							}
							else
							{
								if (ws_button_tcp.is_open())
									ws_button_tcp.close(websocket::close_code::normal);
							}
							INFO("Virtual button press %1% was sent", work_.data_);
						}
						if (ec)
							ERR("Failed to send button command : %1%", ec.message());								
					}
					else
						ERR("Did not receive resource path for BUTTON. Aborting!");
				}
				else
					ERR("Did not receive resource path for BUTTON. Aborting!");

				workIsFinished();
			}
			else
			{
				ERR("Unmanaged command during BUTTON: %1%. Aborting!", response_id);
				workIsFinished();
			}
		}break;
		case WORK_KEEPALIVE:
		{
			if (!(response_id == "getPowerState" && !payload["state"].empty() && payload["state"].is_string())) 
				ERR("Unmanaged command during Ping - Pong");
			workIsFinished();
		}break;
		default:break;
		}
	}
	else if (response_type == "error")
	{
		if (work_.type_ == WORK_POWER_ON)
		{
			if(response_id == "unblankScreen")
			{
				isPoweredOn_ = true;
				INFO("Power state is ON");
			}
			else
				ERR("Unmanaged command during POWER ON: %1%. Aborting!", response_id);
		}
		else if (work_.type_ == WORK_REQUEST && !response["error"].empty() && response["error"].is_string())
				WARNING("Error message received:%1%", response["error"]);
		else
			WARNING("Unmanaged error response id: %1%", response_id);
		workIsFinished();
	}
	else
	{
		DEBUG("Unmanaged response id: %1% and/or type: %2%", response_id, response_type);
		workIsFinished();
	}
	read();
}
void WebOsClient::Impl::onWait(beast::error_code ec) {
	if (ec)
		return onError(ec, "onWait");
	send(JSON_GETPOWERSTATE);
}
void WebOsClient::Impl::onDelayedRequest(beast::error_code ec) {
	if (ec)
		return onError(ec, "onDelayedRequest");
	work_.type_ = WORK_REQUEST;
	if (socket_status_ == SOCKET_CONNECTED)
		send(work_.data_);
	else
		this->run();
}
void WebOsClient::Impl::onClose(beast::error_code ec) {
	if (ec) 
		return onError(ec, "onClose");
	DEBUG("Socket closed gracefully");
	socket_status_ = SOCKET_DISCONNECTED;
}
void WebOsClient::Impl::onError(beast::error_code& ec, std::string err) {

	if (ec && err != "")
	{
		DEBUG("%1% (%2%)", ec.message(), err);
	}
	else
		ERR("Invalid error code in onError");
	
	switch (work_.type_)
	{
	case WORK_POWER_ON:
		if (socket_status_ == SOCKET_CONNECTING)
		{
			if (time(0) - work_.timestamp_start_ < device_settings_.extra.timeout && work_.retry_count_ < device_settings_.extra.timeout / (TIMER_RETRY_WAIT / 1000))
			{
				if (time(0) - work_.timestamp_retry_ <= (TIMER_RETRY_WAIT / 1000))
					async_timer_.expires_from_now(boost::posix_time::milliseconds(TIMER_RETRY_WAIT));
				else
					async_timer_.expires_from_now(boost::posix_time::milliseconds(50));
				async_timer_.async_wait(beast::bind_front_handler(&Impl::onRetryConnection, shared_from_this()));
			}
			else
			{
				WARNING("Finished retrying connection");
				socket_status_ = SOCKET_DISCONNECTED;
				workIsFinished();
			}
		}
		else if (socket_status_ == SOCKET_CONNECTED) // tried to write on invalid/closed socket
		{ 
			if(work_.retry_count_ < device_settings_.extra.timeout / (TIMER_RETRY_WAIT / 1000))
			{
				DEBUG("Failed to send data during POWER ON due to invalid socket / lost connection.");
				async_timer_.expires_from_now(boost::posix_time::milliseconds(50));
				async_timer_.async_wait(beast::bind_front_handler(&Impl::onRetryConnection, shared_from_this()));
			}
			else
			{
				WARNING("Finished retrying connection");
				socket_status_ = SOCKET_DISCONNECTED;
				workIsFinished();
			}
		}
		else
		{
			ERR("Undefined socket status during POWER ON. Aborting!");
			workIsFinished();
		}
		break;
	case WORK_POWER_OFF:
	case WORK_BLANK_SCREEN:
	case WORK_REQUEST:
	case WORK_BUTTON:
		if (socket_status_ == SOCKET_CONNECTED || socket_status_ == SOCKET_CONNECTING)
		{
			if (work_.retry_count_ < 2) // tried to write on invalid /closed socket
			{
				DEBUG("Failed to send data due to invalid socket / lost connection");
				async_timer_.expires_from_now(boost::posix_time::milliseconds(50));
				async_timer_.async_wait(beast::bind_front_handler(&Impl::onRetryConnection, shared_from_this()));
			}
			else
			{
				WARNING("Retried twice but failed to perform work of type %1%. Aborting!", std::to_string(work_.type_));
				socket_status_ = SOCKET_DISCONNECTED;
				workIsFinished();
			}		
		}
		else
		{
			ERR("Undefined socket status at onError. Aborting!");
			workIsFinished();
		}
		break;
	case WORK_KEEPALIVE:
		DEBUG("Ping - Pong failed.");
		socket_status_ = SOCKET_DISCONNECTED;
		workIsFinished();
		break;
	default:
		socket_status_ = SOCKET_DISCONNECTED;
		break;
	}
	return;
}
void WebOsClient::Impl::onRetryConnection(beast::error_code ec) {
	if (ec)
		return onError(ec, "onRetryConnection");
	DEBUG("Retrying connection...");
	work_.retry_count_++;
	work_.timestamp_retry_ = time(0);
	this->run();
	if (work_.type_ == WORK_POWER_ON)
		this->runWOL();
}
void WebOsClient::Impl::runWOL(void) {

	boost::system::error_code error;
	if (!udp_socket_.is_open())
	{
//		DEBUG("Starting to send magic packets (WOL)");
		udp_socket_.open(udp::v4(), error);
		if (error)
			DEBUG("Failed to open UDP socket for WOL");
		udp_socket_.set_option(boost::asio::socket_base::broadcast(true), error);
		if (error)
			DEBUG("Failed to set options for UDP socket");
		wol_timer_.expires_from_now(boost::posix_time::milliseconds(200));
		wol_timer_.async_wait(beast::bind_front_handler(&Impl::onWOL, shared_from_this()));
	}
	else
		DEBUG("Magic packets (WOL) are already being sent out"); //  i.e. WOL routine is already running 


}
void WebOsClient::Impl::onWOL(beast::error_code ec) {
	BYTE buf[102];
	unsigned char mac[6];
	boost::system::error_code error;
	if (ec)
	{
		ERR("Function call onWOL failed (%1%), Closing UDP socket!", ec.message());
		if (udp_socket_.is_open())
			udp_socket_.close();
		return;
	}
	
	if(work_.type_ == WORK_POWER_ON && time(0) - work_.timestamp_start_ < device_settings_.extra.timeout)
	{			
		for (auto& mac_str : device_settings_.mac_addresses)
		{
			if (makeMagicPacket(mac_str, buf, mac))
			{
				TransientArp transient_arp;
				
				std::vector<std::string> endpoint_ips;
				if (device_settings_.wake_method == WOL_TYPE_NETWORKBROADCAST || device_settings_.wake_method == WOL_TYPE_AUTO)
					endpoint_ips.push_back("255.255.255.255");
				if (device_settings_.wake_method == WOL_TYPE_IP || device_settings_.wake_method == WOL_TYPE_AUTO)
				{
					std::string arp_status;
					endpoint_ips.push_back(device_settings_.ip);
					transient_arp.createTransientLocalNetEntry(device_settings_.ip, mac, arp_status);
					if(arp_status != work_.status_arp_override_for_wol_)
					{
						work_.status_arp_override_for_wol_ = arp_status;
						DEBUG(arp_status);
					}
				}
				if (device_settings_.wake_method == WOL_TYPE_SUBNETBROADCAST || device_settings_.wake_method == WOL_TYPE_AUTO)
				{
					std::vector<std::string> vIP = tools::stringsplit(device_settings_.ip, ".");					
					std::vector<std::string> vSubnet;
					vSubnet = tools::stringsplit(device_settings_.subnet, ".");
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
						endpoint_ips.push_back(broadcastaddress.str());
					}	
				}
				for (auto& ip : endpoint_ips)
				{
					std::string logmsg;
					try
					{
						udp::endpoint endpoint_(boost::asio::ip::make_address(ip, error), 9);
						if (error)
							logmsg = "Function call make_address failed in WOL routine";
						else
						{
							size_t n = udp_socket_.send_to(boost::asio::buffer(buf, 102), endpoint_);
							if (n == 102)
							{
								logmsg = "WOL-tastic packet > ";
								logmsg += ip;
								while (logmsg.size() < 36)
									logmsg += " ";
								logmsg += " - OK";
							}
							else
							{
								logmsg = "WOL-tastic packet > ";
								logmsg += ip;
								while (logmsg.size() < 36)
									logmsg += " ";
								logmsg += " - FAIL";
							}
						}
					}
					catch (std::exception const& e)
					{
						logmsg = "Exception raised in WOL routine - ";
						logmsg += e.what();
					}
					DEBUG(logmsg);
				}
			}
			else
				ERR("Failed to create magic packet");
		}
		wol_timer_.expires_from_now(boost::posix_time::milliseconds(TIMER_WOL_WAIT));
		wol_timer_.async_wait(beast::bind_front_handler(&Impl::onWOL, shared_from_this()));
	}
	else
	{
//		DEBUG("Stopped sending magic packets (WOL)");
		udp_socket_.close();	
	}
}
//   Write key to configuration file when a thread has received a pairing key. Thread safe
bool WebOsClient::Impl::setSessionKey(std::string Key, std::string deviceid)
{
	if (Key.size() > 0 && deviceid.size() > 0)
	{
		std::wstring path = tools::widen(device_settings_.extra.data_path);
		path += CONFIG_FILE;
		nlohmann::json jsonPrefs;

		//thread safe
		static std::mutex prefslock;
		while (!prefslock.try_lock())
			Sleep(TIMER_LOG_MUTEX);
		std::ifstream i(path.c_str());
		if (i.is_open())
		{
			i >> jsonPrefs;
			i.close();
			jsonPrefs[deviceid]["SessionKey"] = Key;
		}
		else
			ERR("Failed to open config file to save new session key");

		std::ofstream o(path.c_str());
		if (o.is_open())
		{
			o << std::setw(4) << jsonPrefs << std::endl;
			o.close();
		}
		else
			ERR("Failed to save new session key in config file");
		prefslock.unlock();
	}
	return true;
}
bool WebOsClient::Impl::makeMagicPacket(std::string mac_, BYTE* buf, unsigned char* mac)
{
	if (mac_ == "")
		false;

	//remove filling from input MAC
	char CharsToRemove[] = ".:- ";
	for (int i = 0; i < strlen(CharsToRemove); ++i)
		mac_.erase(remove(mac_.begin(), mac_.end(), CharsToRemove[i]), mac_.end());

	if (mac_.length() == 12)
	{
		for (int i = 0; i < 6; ++i)
			buf[i] = 0xFF;
		for (int i = 0; i < 6; ++i)
			buf[i + 6] = static_cast<BYTE>(std::stoul(mac_.substr(i * 2, 2), nullptr, 16));
		for (int i = 2; i <= 16; ++i)
			memcpy(&buf[i * 6], &buf[6], 6 * sizeof(BYTE));
		memcpy(mac, &buf[6], 6 * sizeof(char));
		return true;
	}
	return false;
}
WebOsClient::WebOsClient(net::io_context& ioc, ssl::context& ctx, Device& settings, Logging& log)
	: pimpl(std::make_shared<Impl>(ioc, ctx, settings, log))
{
}
WebOsClient::~WebOsClient(void) = default;
bool WebOsClient::powerOn(void) {
	Work work;
	work.type_ = WORK_POWER_ON;
	pimpl->enqueueWork(work);
	return true;
}
bool WebOsClient::powerOff(bool forced) {
	Work work;
	work.type_ = WORK_POWER_OFF;
	work.forced_ = forced;
	pimpl->enqueueWork(work);
	return true;
}
bool WebOsClient::blankScreen(bool forced) {
	Work work;
	work.type_ = WORK_BLANK_SCREEN;
	work.forced_ = forced;
	pimpl->enqueueWork(work);
	return true;
}
bool WebOsClient::sendRequest(std::string data, std::string log_message, int delay) {
	Work work;
	work.data_ = data;
	work.type_ = delay == 0 ? WORK_REQUEST : WORK_REQUEST_DELAYED;
	work.log_message = log_message;
	work.delay_ = delay;
	pimpl->enqueueWork(work);
	return true;
}
bool WebOsClient::sendButton(std::string button) {
	Work work;
	work.data_ = button;
	work.type_ = WORK_BUTTON;
	pimpl->enqueueWork(work);
	return true;
}

bool WebOsClient::close(bool enqueue) {
	if(enqueue)
	{
		Work work;
		work.type_ = WORK_CLOSE;
		pimpl->enqueueWork(work);
	}
	else
		pimpl->close();
	return true;
}