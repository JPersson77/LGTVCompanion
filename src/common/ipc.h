// LGTV Linux Companion — a Linux port of LGTV Companion.
//
// Copyright © 2021-2026 Jörgen Persson
// Licensed under the MIT License. See the LICENSE file at the repository root
// for the full license text, which must accompany all copies.

#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Local IPC over a unix domain socket, replacing the windows named pipe
// (IpcServer2/IpcClient2) used upstream. Messages are UTF-8 and newline
// delimited; the upstream protocol was UTF-16 wchar_t buffers.
//
// The callback is invoked on the internal io thread. Keep it short and do not
// call back into send()/terminate() from inside it.
using IpcCallback = std::function<void(const std::string& message, int connection)>;

class IpcServer
{
public:
	// Binds and listens on socket_path, replacing any stale socket left behind
	// by a previous run.
	IpcServer(std::string socket_path, IpcCallback callback);
	~IpcServer();

	IpcServer(const IpcServer&) = delete;
	IpcServer& operator=(const IpcServer&) = delete;

	// Send to one connection, or to every connected client when connection < 0.
	bool send(const std::string& message, int connection = -1);
	bool terminate(void);
	bool isRunning(void) const;

private:
	class Impl;
	std::unique_ptr<Impl> pimpl;
};

class IpcClient
{
public:
	// Connects to socket_path, retrying in the background until the server
	// appears or terminate() is called.
	IpcClient(std::string socket_path, IpcCallback callback);
	~IpcClient();

	IpcClient(const IpcClient&) = delete;
	IpcClient& operator=(const IpcClient&) = delete;

	bool send(const std::string& message);
	bool terminate(void);
	bool isConnected(void) const;

	// One-shot helper for the CLI: connect, send, optionally wait for a single
	// reply, disconnect. Returns false if the daemon is not reachable.
	static bool sendOneShot(const std::string& socket_path,
		const std::string& message,
		std::string* reply = nullptr,
		int timeout_ms = 3000);

private:
	class Impl;
	std::unique_ptr<Impl> pimpl;
};
