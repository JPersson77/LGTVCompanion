// LGTV Linux Companion — a Linux port of LGTV Companion.
//
// Copyright © 2021-2026 Jörgen Persson
// Licensed under the MIT License. See the LICENSE file at the repository root
// for the full license text, which must accompany all copies.

#include "ipc.h"
#include "paths.h"
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <thread>

namespace net = boost::asio;
using local_stream = net::local::stream_protocol;

namespace
{
	// Every message is one newline-terminated UTF-8 line.
	constexpr char MESSAGE_DELIMITER = '\n';
	constexpr int  RECONNECT_INTERVAL_MS = 1000;

	std::string sanitise(std::string message)
	{
		// A literal newline would split one message into two on the wire.
		for (auto& c : message)
			if (c == '\n' || c == '\r')
				c = ' ';
		return message;
	}
}

// ---------------------------------------------------------------- server ---

class IpcServer::Impl
{
public:
	// One connected peer.
	struct Session : public std::enable_shared_from_this<Session>
	{
		local_stream::socket socket;
		net::streambuf buffer;
		std::deque<std::string> write_queue;
		int id = 0;

		explicit Session(net::io_context& io) : socket(io) {}
	};

	Impl(std::string socket_path, IpcCallback callback)
		: socket_path_(std::move(socket_path))
		, callback_(std::move(callback))
		, acceptor_(io_)
	{
	}

	bool start(void);
	void stop(void);
	bool send(const std::string& message, int connection);

	void accept(void);
	void read(std::shared_ptr<Session> session);
	void write(std::shared_ptr<Session> session, std::string message);
	void drop(const std::shared_ptr<Session>& session);

	std::string socket_path_;
	IpcCallback callback_;
	net::io_context io_;
	local_stream::acceptor acceptor_;
	std::thread io_thread_;
	std::mutex sessions_mutex_;
	std::vector<std::shared_ptr<Session>> sessions_;
	std::atomic<bool> running_{ false };
	int next_id_ = 0;
};

bool IpcServer::Impl::start(void)
{
	std::error_code fs_ec;
	paths::ensureDir(std::filesystem::path(socket_path_).parent_path().string());
	// A socket file survives a crash and would make bind() fail with EADDRINUSE.
	std::filesystem::remove(socket_path_, fs_ec);

	boost::system::error_code ec;
	acceptor_.open(local_stream::acceptor::protocol_type(), ec);
	if (ec)
		return false;
	acceptor_.bind(local_stream::endpoint(socket_path_), ec);
	if (ec)
		return false;
	acceptor_.listen(net::socket_base::max_listen_connections, ec);
	if (ec)
		return false;

	// The socket lives in XDG_RUNTIME_DIR which is already 0700, but be explicit
	// so a fallback path under /tmp is not world-writable either.
	std::filesystem::permissions(socket_path_,
		std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
		fs_ec);

	running_ = true;
	accept();
	io_thread_ = std::thread([this] { io_.run(); });
	return true;
}
void IpcServer::Impl::stop(void)
{
	if (!running_.exchange(false))
		return;

	net::post(io_, [this] {
		boost::system::error_code ec;
		acceptor_.close(ec);
		std::lock_guard<std::mutex> lock(sessions_mutex_);
		for (auto& session : sessions_)
			session->socket.close(ec);
		sessions_.clear();
		});

	if (io_thread_.joinable())
		io_thread_.join();

	std::error_code fs_ec;
	std::filesystem::remove(socket_path_, fs_ec);
}
void IpcServer::Impl::accept(void)
{
	auto session = std::make_shared<Session>(io_);
	acceptor_.async_accept(session->socket, [this, session](boost::system::error_code ec) {
		if (ec)
		{
			if (running_)
				accept();
			return;
		}
		{
			std::lock_guard<std::mutex> lock(sessions_mutex_);
			session->id = next_id_++;
			sessions_.push_back(session);
		}
		read(session);
		accept();
		});
}
void IpcServer::Impl::read(std::shared_ptr<Session> session)
{
	net::async_read_until(session->socket, session->buffer, MESSAGE_DELIMITER,
		[this, session](boost::system::error_code ec, std::size_t bytes) {
			if (ec)
			{
				drop(session);
				return;
			}
			std::string message(
				net::buffers_begin(session->buffer.data()),
				net::buffers_begin(session->buffer.data()) + bytes - 1);
			session->buffer.consume(bytes);

			if (callback_ && !message.empty())
				callback_(message, session->id);

			read(session);
		});
}
void IpcServer::Impl::write(std::shared_ptr<Session> session, std::string message)
{
	bool idle = session->write_queue.empty();
	session->write_queue.push_back(std::move(message));
	if (!idle)
		return; // a write is already draining the queue

	auto drain = std::make_shared<std::function<void()>>();
	*drain = [this, session, drain]() {
		net::async_write(session->socket,
			net::buffer(session->write_queue.front()),
			[this, session, drain](boost::system::error_code ec, std::size_t) {
				if (ec)
				{
					drop(session);
					return;
				}
				session->write_queue.pop_front();
				if (!session->write_queue.empty())
					(*drain)();
			});
		};
	(*drain)();
}
void IpcServer::Impl::drop(const std::shared_ptr<Session>& session)
{
	boost::system::error_code ec;
	session->socket.close(ec);
	std::lock_guard<std::mutex> lock(sessions_mutex_);
	sessions_.erase(
		std::remove(sessions_.begin(), sessions_.end(), session),
		sessions_.end());
}
bool IpcServer::Impl::send(const std::string& message, int connection)
{
	if (!running_)
		return false;

	std::vector<std::shared_ptr<Session>> targets;
	{
		std::lock_guard<std::mutex> lock(sessions_mutex_);
		for (auto& session : sessions_)
			if (connection < 0 || session->id == connection)
				targets.push_back(session);
	}
	if (targets.empty())
		return false;

	std::string line = sanitise(message) + MESSAGE_DELIMITER;
	for (auto& session : targets)
		net::post(io_, [this, session, line] { write(session, line); });
	return true;
}

IpcServer::IpcServer(std::string socket_path, IpcCallback callback)
	: pimpl(std::make_unique<Impl>(std::move(socket_path), std::move(callback)))
{
	pimpl->start();
}
IpcServer::~IpcServer() { pimpl->stop(); }
bool IpcServer::send(const std::string& message, int connection) { return pimpl->send(message, connection); }
bool IpcServer::terminate(void) { pimpl->stop(); return true; }
bool IpcServer::isRunning(void) const { return pimpl->running_; }

// ---------------------------------------------------------------- client ---

class IpcClient::Impl
{
public:
	Impl(std::string socket_path, IpcCallback callback)
		: socket_path_(std::move(socket_path))
		, callback_(std::move(callback))
		, socket_(io_)
		, retry_timer_(io_)
	{
	}

	void start(void);
	void stop(void);
	bool send(const std::string& message);

	void connect(void);
	void read(void);

	std::string socket_path_;
	IpcCallback callback_;
	net::io_context io_;
	local_stream::socket socket_;
	net::steady_timer retry_timer_;
	net::streambuf buffer_;
	std::thread io_thread_;
	std::atomic<bool> running_{ false };
	std::atomic<bool> connected_{ false };
};

void IpcClient::Impl::start(void)
{
	running_ = true;
	connect();
	io_thread_ = std::thread([this] { io_.run(); });
}
void IpcClient::Impl::stop(void)
{
	if (!running_.exchange(false))
		return;
	net::post(io_, [this] {
		boost::system::error_code ec;
		retry_timer_.cancel();
		socket_.close(ec);
		});
	if (io_thread_.joinable())
		io_thread_.join();
	connected_ = false;
}
void IpcClient::Impl::connect(void)
{
	socket_.async_connect(local_stream::endpoint(socket_path_),
		[this](boost::system::error_code ec) {
			if (!running_)
				return;
			if (ec)
			{
				// Daemon not up yet. Retry until it is.
				boost::system::error_code ignored;
				socket_.close(ignored);
				retry_timer_.expires_after(std::chrono::milliseconds(RECONNECT_INTERVAL_MS));
				retry_timer_.async_wait([this](boost::system::error_code timer_ec) {
					if (!timer_ec && running_)
						connect();
					});
				return;
			}
			connected_ = true;
			read();
		});
}
void IpcClient::Impl::read(void)
{
	net::async_read_until(socket_, buffer_, MESSAGE_DELIMITER,
		[this](boost::system::error_code ec, std::size_t bytes) {
			if (ec)
			{
				connected_ = false;
				boost::system::error_code ignored;
				socket_.close(ignored);
				if (running_)
					connect();
				return;
			}
			std::string message(
				net::buffers_begin(buffer_.data()),
				net::buffers_begin(buffer_.data()) + bytes - 1);
			buffer_.consume(bytes);

			if (callback_ && !message.empty())
				callback_(message, 0);

			read();
		});
}
bool IpcClient::Impl::send(const std::string& message)
{
	if (!connected_)
		return false;
	auto line = std::make_shared<std::string>(sanitise(message) + MESSAGE_DELIMITER);
	net::post(io_, [this, line] {
		net::async_write(socket_, net::buffer(*line),
			[this, line](boost::system::error_code ec, std::size_t) {
				if (ec)
					connected_ = false;
			});
		});
	return true;
}

IpcClient::IpcClient(std::string socket_path, IpcCallback callback)
	: pimpl(std::make_unique<Impl>(std::move(socket_path), std::move(callback)))
{
	pimpl->start();
}
IpcClient::~IpcClient() { pimpl->stop(); }
bool IpcClient::send(const std::string& message) { return pimpl->send(message); }
bool IpcClient::terminate(void) { pimpl->stop(); return true; }
bool IpcClient::isConnected(void) const { return pimpl->connected_; }

bool IpcClient::sendOneShot(const std::string& socket_path,
	const std::string& message,
	std::string* reply,
	int timeout_ms)
{
	try
	{
		net::io_context io;
		local_stream::socket socket(io);
		boost::system::error_code ec;

		socket.connect(local_stream::endpoint(socket_path), ec);
		if (ec)
			return false;

		std::string line = sanitise(message) + MESSAGE_DELIMITER;
		net::write(socket, net::buffer(line), ec);
		if (ec)
			return false;

		if (!reply)
			return true;

		// Bound the wait so the CLI cannot hang on an unresponsive daemon.
		bool done = false;
		bool ok = false;
		net::streambuf buffer;
		net::steady_timer deadline(io);
		deadline.expires_after(std::chrono::milliseconds(timeout_ms));
		deadline.async_wait([&socket](boost::system::error_code timer_ec) {
			if (!timer_ec)
			{
				boost::system::error_code ignored;
				socket.close(ignored);
			}
			});
		net::async_read_until(socket, buffer, MESSAGE_DELIMITER,
			[&](boost::system::error_code read_ec, std::size_t bytes) {
				done = true;
				deadline.cancel();
				if (read_ec)
					return;
				reply->assign(
					net::buffers_begin(buffer.data()),
					net::buffers_begin(buffer.data()) + bytes - 1);
				ok = true;
			});
		io.run();
		return done && ok;
	}
	catch (std::exception const&)
	{
		return false;
	}
}
