#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <array>
#include <boost/asio.hpp>
#include <boost/asio/windows/stream_handle.hpp>

class IpcServer2
{
public:
	IpcServer2(std::wstring name,
		void (*callback)(std::wstring, LPVOID),
		LPVOID object);
	~IpcServer2();
	bool send(std::wstring msg, int pipe = -1);
	bool terminate();

private:
	struct PipeInstance {
		HANDLE raw_handle = INVALID_HANDLE_VALUE;
		boost::asio::windows::stream_handle stream;
		boost::asio::strand<boost::asio::io_context::executor_type> strand;
		std::array<wchar_t, 1024> buffer{};
		bool connected = false;

		PipeInstance(boost::asio::io_context& io)
			: stream(io), strand(io.get_executor()) {
		}
	};
	std::atomic<HANDLE> pending_handle_{ INVALID_HANDLE_VALUE };

	void accept_loop();
	void start_read(std::shared_ptr<PipeInstance> pipe);
	void do_write(std::shared_ptr<PipeInstance> pipe, std::wstring msg);

private:
	boost::asio::io_context io_;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_;
	std::thread io_thread_;
	std::thread accept_thread_;

	std::wstring name_;
	void (*callback_)(std::wstring, LPVOID) = nullptr;
	LPVOID object_ = nullptr;

	std::mutex pipes_mutex_;
	std::vector<std::shared_ptr<PipeInstance>> pipes_;
	bool running_ = false;
};

class IpcClient2
{
public:
	IpcClient2(std::wstring name,
		void (*callback)(std::wstring, LPVOID),
		LPVOID object);
	~IpcClient2();
	bool send(std::wstring msg);
	bool terminate();

private:
	void connect_loop();
	void start_read();

private:
	boost::asio::io_context io_;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_;
	boost::asio::windows::stream_handle stream_;
	std::thread io_thread_;
	std::thread connect_thread_;

	std::wstring name_;
	void (*callback_)(std::wstring, LPVOID) = nullptr;
	LPVOID object_ = nullptr;
	std::atomic<bool> reconnect_{ false };

	HANDLE raw_ = INVALID_HANDLE_VALUE;
	std::array<wchar_t, 1024> buffer_{};
	bool running_ = false;
};
