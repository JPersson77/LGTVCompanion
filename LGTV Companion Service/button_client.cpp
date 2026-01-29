#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "button_client.h"
#include "../Common/log.h"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio.hpp>
#include <queue>

// timers
#define			TIMER_ASYNC_TIMEOUT					3000	// milliseconds before async_connect and async_handshake SSL timeout
#define			TIMER_RETRY_WAIT					2000	// milliseconds to wait before retrying connection

// connection port
#define			PORT								"3000"
#define			PORT_SSL							"3001"

//socket_status
#define			SOCKET_DISCONNECTED					0
#define			SOCKET_CONNECTING					1
#define			SOCKET_CONNECTED					2

//work_type
#define			WORK_UNDEFINED						0
#define			WORK_BUTTON							5
#define			WORK_CLOSE							6
#define			WORK_KEEPALIVE						7

#define			LOG_KEEPALIVE						true

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

class ButtonWork {
public:
	int									type_ = WORK_UNDEFINED;
	std::string							data_;
	time_t								timestamp_enqueue_ = 0;
	time_t								timestamp_start_ = 0;
	time_t								timestamp_retry_ = 0;
	int									retry_count_ = 0;
	std::string							log_message = "";
	void clear(void)
	{
		type_ = WORK_UNDEFINED;
		data_ = "";
		timestamp_start_ = 0;
		timestamp_retry_ = 0;
		timestamp_enqueue_ = 0;
		retry_count_ = 0;
		log_message = "";
	}
};

class ButtonClient::Impl : public std::enable_shared_from_this<ButtonClient::Impl> {
private:
	Device device_settings_;
	ButtonWork work_;
	net::steady_timer keep_alive_timer_;
	net::steady_timer async_timer_;
	ssl::context* ctx_;
	tcp::resolver resolver_;
	std::optional<beast::websocket::stream<beast::tcp_stream>> ws_tcp_;
	std::optional<beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>> ws_;
	beast::flat_buffer buffer_;
	std::string host_;
	int socket_status_ = SOCKET_DISCONNECTED;
	std::string path_;
	std::list<ButtonWork> workQueue_;
	std::shared_ptr<Logging> log_;

	//functions running on strand
	void startNextWork(void);
	void workIsFinished(void);
	void doClose(void);
	void run(void);
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
	void onError(boost::beast::error_code&, std::string);
	void onRetryConnection(boost::beast::error_code);
	void onKeepAlive(boost::beast::error_code);
public:

	explicit Impl(net::io_context&, ssl::context&, Device&, Logging&);
	~Impl() {};
	void close(void);
	bool isConnected();
	void setPath(std::string);
	void enqueueWork(ButtonWork&);
};
ButtonClient::Impl::Impl(net::io_context& ioc, ssl::context& ctx, Device& settings, Logging& log)
	: resolver_(net::make_strand(ioc))
	, async_timer_(net::make_strand(ioc))
	, keep_alive_timer_(net::make_strand(ioc))
{
	ctx_ = &ctx;
	device_settings_ = settings;
	log_ = std::make_shared<Logging>(log);
	if (device_settings_.ssl)
		ws_.emplace(resolver_.get_executor(), *ctx_);
	else
		ws_tcp_.emplace(resolver_.get_executor());
}
void ButtonClient::Impl::close(void) {
	net::dispatch(resolver_.get_executor(), beast::bind_front_handler(&Impl::doClose, shared_from_this()));
}
bool ButtonClient::Impl::isConnected(void) {
	return socket_status_ != SOCKET_DISCONNECTED;
}
void ButtonClient::Impl::setPath(std::string path) {
	path_ = path;
}
void ButtonClient::Impl::enqueueWork(ButtonWork& work)
{
	if (work.type_ != WORK_KEEPALIVE || LOG_KEEPALIVE)
		if (work.type_ == WORK_BUTTON)
			DEBUG("[B] Enqueueing virtual button press: %1%", work.data_);
		else if (work.type_ == WORK_KEEPALIVE)
			DEBUG("[B] Enqueueing KEEP ALIVE");
		else
			DEBUG("[B] Enqueueing CLOSE");
	work.timestamp_enqueue_ = time(0);
	net::dispatch(resolver_.get_executor(), [unit = work, self = shared_from_this()]() mutable
		{
			self->workQueue_.emplace_back(std::move(unit));
			self->startNextWork();
		});
}
void ButtonClient::Impl::send(std::string data, std::string log_message) {
	if (device_settings_.ssl)
	{
		if (work_.type_ != WORK_KEEPALIVE || LOG_KEEPALIVE)
		{
			if (log_message == "")
				DEBUG("[B] > > > SEND > > >: %1%", data);
			else
				DEBUG("[B] > > > SEND > > >: [%1%]", log_message);
		}
		ws_->async_write(net::buffer(data), beast::bind_front_handler(&Impl::onWrite, shared_from_this()));
	}
	else
	{
		if (work_.type_ != WORK_KEEPALIVE || LOG_KEEPALIVE)
		{
			if (log_message == "")
				DEBUG("[B] > > > SEND (non-ssl) > > >: %1%", data);
			else
				DEBUG("[B] > > > SEND (non-ssl) > > >: [%1%]", log_message);
		}
		ws_tcp_->async_write(net::buffer(data), beast::bind_front_handler(&Impl::onWrite, shared_from_this()));
	}
}
void ButtonClient::Impl::read(void) {
	if (device_settings_.ssl)
		ws_->async_read(buffer_, beast::bind_front_handler(&Impl::onRead, shared_from_this()));
	else
		ws_tcp_->async_read(buffer_, beast::bind_front_handler(&Impl::onRead, shared_from_this()));
}
void ButtonClient::Impl::startNextWork(void)
{
	if (work_.type_ != WORK_UNDEFINED) // work is already ongoing
	{
		time_t time_diff = time(0) - work_.timestamp_start_;
		if (time_diff > 10)
		{
			ERR("[B] Lingering work detected. Aborting lingering work!");
			work_.clear();
		}
		else
		{
			return; // wait for the current work to end
		}
	}
	keep_alive_timer_.cancel();
	if (socket_status_ == SOCKET_CONNECTING)
	{
		ERR("[B] Socket status is CONNECTING while starting new work, which is undefined. Closing connection and aborting work!");
		doClose();
	}
	else if (!workQueue_.empty()) 
	{
		work_ = std::move(workQueue_.front());
		workQueue_.pop_front();
		work_.timestamp_start_ = time(0);
		std::string button_command; 
		switch (work_.type_)
		{
		case WORK_KEEPALIVE:
			if (LOG_KEEPALIVE)
			{
				DEBUG("[B] ---  Starting work: KEEP ALIVE  -----------------");
			}
			button_command = "type:ping\n\n";
			if (socket_status_ == SOCKET_CONNECTED)
				send(button_command, "ping");
			else
				workIsFinished();
			break;
		case WORK_BUTTON:
			DEBUG("[B] ---  Starting work: BUTTON  -----------------");
			button_command = "type:button\nname:";
			button_command += work_.data_;
			button_command += "\n\n";
			if (socket_status_ == SOCKET_CONNECTED)
			{
				send(button_command, work_.data_);
			}
			else
				this->run();
			break;

		case WORK_CLOSE:
			doClose();
			break;

		default:
			ERR("[B] New work unit is undefined. Discarding work unit and starting the next one!");
			work_.clear();
			startNextWork();
			break;
		}
	}
	else
	{
		if (socket_status_ == SOCKET_CONNECTED)
		{
			if (device_settings_.persistent_connection_level == PERSISTENT_CONNECTION_KEEPALIVE)
			{
				keep_alive_timer_.expires_after(boost::asio::chrono::seconds(60));
				keep_alive_timer_.async_wait(beast::bind_front_handler(&Impl::onKeepAlive, shared_from_this()));
			}
			else 
			{
				DEBUG("[B] Work queue is empty");;
			}
		}
		else
		{
			DEBUG("[B] Work queue is empty");
		}
	}
}
void ButtonClient::Impl::workIsFinished(void)
{
	work_.clear();
	if(!workQueue_.empty())
		Sleep(250); //  webOS does not like requests to be sent too quickly
	startNextWork();
}
void ButtonClient::Impl::doClose(void) {
	workQueue_ = {}; //clear work queue
	work_.clear();
	switch (socket_status_)
	{
	case SOCKET_DISCONNECTED:
		DEBUG("[B] Closing connection. Socket is already closed");
		break;
	default:
		DEBUG("[B] Closing connection");
		if (device_settings_.ssl)
			ws_->async_close(websocket::close_code::normal, beast::bind_front_handler(&Impl::onClose, shared_from_this()));
		else
			ws_tcp_->async_close(websocket::close_code::normal, beast::bind_front_handler(&Impl::onClose, shared_from_this()));
		break;
	}
}
void ButtonClient::Impl::run(void) {
	if (path_ == "")
	{
		DEBUG("[B] Websocket path not set!");
		return;
	}

	socket_status_ = SOCKET_CONNECTING;
	if (device_settings_.ssl)
		ws_.emplace(resolver_.get_executor(), *ctx_);
	else
		ws_tcp_.emplace(resolver_.get_executor());
	host_ = device_settings_.ip;
	work_.timestamp_retry_ = time(0);
	resolver_.async_resolve(host_.c_str(), device_settings_.ssl ? PORT_SSL : PORT, beast::bind_front_handler(&Impl::onResolve, shared_from_this()));
}
void ButtonClient::Impl::onResolve(beast::error_code ec, tcp::resolver::results_type results) {
	if (ec)
		return onError(ec, "onResolve");
	socket_status_ = SOCKET_CONNECTING;
	if (device_settings_.ssl)
	{
		beast::get_lowest_layer(*ws_).expires_after(std::chrono::milliseconds(TIMER_ASYNC_TIMEOUT));
		beast::get_lowest_layer(*ws_).async_connect(results, beast::bind_front_handler(&Impl::onConnect, shared_from_this()));
	}
	else
	{
		beast::get_lowest_layer(*ws_tcp_).expires_after(std::chrono::milliseconds(TIMER_ASYNC_TIMEOUT));
		beast::get_lowest_layer(*ws_tcp_).async_connect(results, beast::bind_front_handler(&Impl::onConnect, shared_from_this()));
	}
}
void ButtonClient::Impl::onConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) {
	if (ec)
		return onError(ec, "onConnect");
	socket_status_ = SOCKET_CONNECTING;
	if (device_settings_.ssl)
	{
		beast::get_lowest_layer(*ws_).expires_after(std::chrono::milliseconds(TIMER_ASYNC_TIMEOUT));
		if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), device_settings_.ip.c_str())) // Set SNI Hostname
		{
			ec = beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category());
			return onError(ec, "[B] Failed to set SNI hostname");
		}
		host_ += ':' + std::to_string(ep.port());
		ws_->next_layer().async_handshake(ssl::stream_base::client, beast::bind_front_handler(&Impl::onSSLhandshake, shared_from_this()));
	}
	else
	{
		beast::get_lowest_layer(*ws_tcp_).expires_after(std::chrono::milliseconds(TIMER_ASYNC_TIMEOUT));
		ws_tcp_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
		ws_tcp_->set_option(websocket::stream_base::decorator(
			[](websocket::request_type& req)
			{
				req.set(http::field::user_agent,
					std::string(BOOST_BEAST_VERSION_STRING) +
					" websocket-client-async");
			}));
		host_ += ':' + std::to_string(ep.port());
		ws_tcp_->async_handshake(host_, path_, beast::bind_front_handler(&Impl::onWinsockHandshake, shared_from_this()));
	}
}
void ButtonClient::Impl::onSSLhandshake(beast::error_code ec) {
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
	ws_->async_handshake(host_, path_, beast::bind_front_handler(&Impl::onWinsockHandshake, shared_from_this()));
}
void ButtonClient::Impl::onWinsockHandshake(beast::error_code ec) {
	if (ec)
		return onError(ec, "onWinsockHandshake");
	socket_status_ = SOCKET_CONNECTED;
	//	read();
	std::string button_command = "type:button\nname:";
	button_command += work_.data_;
	button_command += "\n\n";
	send(button_command, work_.data_);
	read();
}
void ButtonClient::Impl::onKeepAlive(beast::error_code ec) {
	if (ec)
	{
		if (LOG_KEEPALIVE)
			DEBUG("[B] Error in onKeepAlive: %1%", ec.message());
		return;
	}
	if (socket_status_ == SOCKET_CONNECTED && work_.type_ == WORK_UNDEFINED)
	{
		ButtonWork work;
		work.type_ = WORK_KEEPALIVE;
		enqueueWork(work);
	}
	else
		DEBUG("[B] Terminating KEEP ALIVE");
}

void ButtonClient::Impl::onWrite(beast::error_code ec, std::size_t bytes_transferred) {
	boost::ignore_unused(bytes_transferred);
	if (ec)
		return onError(ec, "onWrite");
	if (socket_status_ == SOCKET_CONNECTING)
		read();
	else
		workIsFinished();

}
void ButtonClient::Impl::onRead(beast::error_code ec, std::size_t bytes_transferred) {
	boost::ignore_unused(bytes_transferred);
	if (ec)
		return onError(ec, "onRead");
	socket_status_ = SOCKET_CONNECTED;
	if (work_.type_ != WORK_KEEPALIVE || LOG_KEEPALIVE)
	{
		if (device_settings_.ssl)
			DEBUG("[B] < < < RECV < < <: %1%", beast::buffers_to_string(buffer_.data()));
		else
			DEBUG("[B] < < < RECV (non-ssl) < < <: %1%", beast::buffers_to_string(buffer_.data()));
	}
	buffer_.consume(buffer_.size());

	read();
}

void ButtonClient::Impl::onClose(beast::error_code ec) {
	if (ec)
		return onError(ec, "onClose");
	DEBUG("[B] Socket closed gracefully");
	socket_status_ = SOCKET_DISCONNECTED;
}
void ButtonClient::Impl::onError(beast::error_code& ec, std::string err) {
	if (ec && err != "")
	{
		DEBUG("[B] %1% (%2%)", ec.message(), err);
	}
	else
		ERR("[B] Invalid error code in onError");

	switch (work_.type_)
	{
	case WORK_BUTTON:
		if (socket_status_ == SOCKET_CONNECTED || socket_status_ == SOCKET_CONNECTING)
		{
			if (work_.retry_count_ < 2) // tried to write on invalid /closed socket
			{
				DEBUG("[B]  Failed to send data due to invalid socket / lost connection");
				async_timer_.expires_after(boost::asio::chrono::milliseconds(50));
				async_timer_.async_wait(beast::bind_front_handler(&Impl::onRetryConnection, shared_from_this()));
			}
			else
			{
				WARNING("[B] Retried twice but failed to perform work. Aborting!");
				socket_status_ = SOCKET_DISCONNECTED;
				workIsFinished();
			}
		}
		else
		{
			ERR("[B] Undefined socket status at onError. Aborting!");
			workIsFinished();
		}
		break;
	case WORK_KEEPALIVE:
		DEBUG("[B]  Ping - Pong failed.");
		socket_status_ = SOCKET_DISCONNECTED;
		workIsFinished();
		break;
	default:
		socket_status_ = SOCKET_DISCONNECTED;
		break;
	}
	return;
}
void ButtonClient::Impl::onRetryConnection(beast::error_code ec) {
	if (ec)
		return onError(ec, "onRetryConnection");
	DEBUG("[B] Retrying connection...");
	work_.retry_count_++;
	work_.timestamp_retry_ = time(0);
	this->run();
}

ButtonClient::ButtonClient(net::io_context& ioc, ssl::context& ctx, Device& settings, Logging& log)
	: pimpl(std::make_shared<Impl>(ioc, ctx, settings, log))
{
}
ButtonClient::~ButtonClient(void) = default;

bool ButtonClient::sendButton(std::string button) {
	ButtonWork work;
	work.data_ = button;
	work.type_ = WORK_BUTTON;
	pimpl->enqueueWork(work);
	return true;
}
bool ButtonClient::setPath(std::string path)
{
	pimpl->setPath(path);
	return true;
}
bool ButtonClient::isConnected()
{
	return pimpl->isConnected();
}
bool ButtonClient::close(bool enqueue) {
	
	if (enqueue)
	{
		ButtonWork work;
		work.type_ = WORK_CLOSE;
		pimpl->enqueueWork(work);
	}
	else
		pimpl->close();
	return true;
}