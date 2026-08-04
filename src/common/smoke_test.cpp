// LGTV Linux Companion — a Linux port of LGTV Companion.
//
// Copyright © 2021-2026 Jörgen Persson
// Licensed under the MIT License. See the LICENSE file at the repository root
// for the full license text, which must accompany all copies.

// Smoke test for the ported portable core. Exercises the pieces whose windows
// implementations were replaced outright: XDG paths, the getifaddrs-backed
// network helpers, config round-tripping and unix-socket IPC.
#include "app_define.h"
#include "event.h"
#include "ipc.h"
#include "log.h"
#include "paths.h"
#include "preferences.h"
#include "tools.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>

static int failures = 0;

static void check(bool condition, const std::string& what, const std::string& detail = "")
{
	std::cout << (condition ? "  ok   " : "  FAIL ") << what;
	if (!detail.empty())
		std::cout << "  [" << detail << "]";
	std::cout << "\n";
	if (!condition)
		failures++;
}

static void testPaths(void)
{
	std::cout << "paths\n";
	check(paths::configDir().find(APP_ID) != std::string::npos, "configDir carries app id", paths::configDir());
	check(paths::configFile().find(CONFIG_FILE) != std::string::npos, "configFile", paths::configFile());
	check(paths::logFile().find(LOG_FILE) != std::string::npos, "logFile", paths::logFile());
	check(paths::ipcSocket().find(IPC_SOCKET_NAME) != std::string::npos, "ipcSocket", paths::ipcSocket());
	check(paths::configDir()[0] == '/', "configDir is absolute");
}

static void testTools(void)
{
	std::cout << "tools\n";
	check(tools::tolower("AbC") == "abc", "tolower");
	check(tools::stringsplit("a b c", " ").size() == 3, "stringsplit");

	std::string s = "hello %1% world";
	tools::replaceAllInPlace(s, "%1%", "brave");
	check(s == "hello brave world", "replaceAllInPlace", s);

	check(tools::compareUsingWildcard("firefox.bin", "fire*"), "wildcard match");
	check(!tools::compareUsingWildcard("chromium", "fire*"), "wildcard non-match");
	check(tools::validateArgument("ON", "on off") == "on", "validateArgument");

	check(tools::isSameSubnet("192.168.1.5", "192.168.1.9", "255.255.255.0"), "isSameSubnet same");
	check(!tools::isSameSubnet("192.168.1.5", "10.0.0.9", "255.255.255.0"), "isSameSubnet different");

	auto ips = tools::getLocalIP();
	std::string joined;
	for (auto& ip : ips)
		joined += ip + " ";
	check(!ips.empty(), "getLocalIP found an interface", joined);

	if (!ips.empty())
	{
		std::string bare = ips[0].substr(0, ips[0].find('/'));
		check(!tools::getInterfaceForIP(bare).empty(), "getInterfaceForIP", tools::getInterfaceForIP(bare));
		check(tools::getSubnetMask(bare) != "", "getSubnetMask", tools::getSubnetMask(bare));
		// Route lookup toward a public address; no packet is sent.
		check(!tools::getSourceIPforDestination("1.1.1.1").empty(),
			"getSourceIPforDestination", tools::getSourceIPforDestination("1.1.1.1"));
	}
}

static void testPreferences(void)
{
	std::cout << "preferences\n";
	std::string tmp = std::filesystem::temp_directory_path() / "lgtvc-smoke-config.json";
	std::filesystem::remove(tmp);

	{
		Preferences prefs(tmp);
		check(prefs.isInitialised(), "constructs with no config file present");
		check(!prefs.lg_api_commands_json.empty(), "embedded lg api command table parsed");
		check(!prefs.lg_api_buttons.empty(), "embedded lg api button list parsed");

		Device d;
		d.name = "Living room";
		d.ip = "192.168.1.50";
		d.mac_addresses.push_back("aa:bb:cc:dd:ee:ff");
		d.network_interface = "enp5s0";
		d.windows_nic_luid = 1689399632855040ULL; // must survive a round-trip
		d.sourceHdmiInput = 2;
		prefs.devices_.push_back(d);
		prefs.user_idle_mode_ = true;
		prefs.user_idle_mode_delay_ = 15;
		check(prefs.writeToDisk(), "writeToDisk");
	}

	{
		Preferences prefs(tmp);
		check(prefs.isInitialised(), "reloads written config");
		check(prefs.devices_.size() == 1, "device count");
		if (prefs.devices_.size() == 1)
		{
			auto& d = prefs.devices_[0];
			check(d.name == "Living room", "device name", d.name);
			check(d.ip == "192.168.1.50", "device ip", d.ip);
			check(d.mac_addresses.size() == 1 && d.mac_addresses[0] == "aa:bb:cc:dd:ee:ff", "device mac");
			check(d.network_interface == "enp5s0", "linux interface name persisted", d.network_interface);
			check(d.windows_nic_luid == 1689399632855040ULL, "windows LUID round-tripped");
			check(d.sourceHdmiInput == 2, "hdmi input");
		}
		check(prefs.user_idle_mode_ == true, "idle mode flag");
		check(prefs.user_idle_mode_delay_ == 15, "idle delay");
	}
	std::filesystem::remove(tmp);
}

static void testEvent(void)
{
	std::cout << "event\n";
	Event e;
	e.set(EVENT_SYSTEM_SUSPEND, { "Device1" });
	check(e.getType() == EVENT_SYSTEM_SUSPEND, "event type preserved");
	check(e.getDevices().size() == 1, "event device list");

	Event button;
	button.set(EVENT_BUTTON, { "Device1" }, "HOME");
	check(button.getData().find("HOME") != std::string::npos, "button event carries payload", button.getData());
}

static void testLogging(void)
{
	std::cout << "logging\n";
	std::string tmp = std::filesystem::temp_directory_path() / "lgtvc-smoke-log.txt";
	std::filesystem::remove(tmp);
	{
		Logging log(LOG_LEVEL_DEBUG, tmp);
		log.info("Smoke", "value is %1% and %2%", "one", "two");
		log.error("Smoke", "an error");
	}
	std::ifstream in(tmp);
	std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	check(content.find("value is one and two") != std::string::npos, "info line with substitutions");
	check(content.find("[---E]") != std::string::npos, "error severity marker");
	std::filesystem::remove(tmp);
}

static void testIpc(void)
{
	std::cout << "ipc\n";
	std::string socket_path = (std::filesystem::temp_directory_path() / "lgtvc-smoke.sock").string();
	std::filesystem::remove(socket_path);

	std::atomic<int> server_received{ 0 };
	std::string server_saw;

	IpcServer server(socket_path, [&](const std::string& message, int connection) {
		server_saw = message;
		server_received++;
		server.send("pong:" + message, connection);
		});
	check(server.isRunning(), "server listening");
	check(std::filesystem::exists(socket_path), "socket file created");

	std::atomic<int> client_received{ 0 };
	std::string client_saw;
	{
		IpcClient client(socket_path, [&](const std::string& message, int) {
			client_saw = message;
			client_received++;
			});

		for (int i = 0; i < 100 && !client.isConnected(); i++)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		check(client.isConnected(), "client connected");

		client.send("ping");
		for (int i = 0; i < 100 && client_received == 0; i++)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	check(server_received == 1, "server got one message", server_saw);
	check(client_saw == "pong:ping", "client got the reply", client_saw);

	// One-shot path used by the CLI.
	std::string reply;
	bool ok = IpcClient::sendOneShot(socket_path, "oneshot", &reply, 2000);
	check(ok && reply == "pong:oneshot", "sendOneShot round-trip", reply);

	check(!IpcClient::sendOneShot("/tmp/lgtvc-does-not-exist.sock", "x", nullptr, 200),
		"sendOneShot fails cleanly with no daemon");
}

int main(void)
{
	testPaths();
	testTools();
	testPreferences();
	testEvent();
	testLogging();
	testIpc();

	std::cout << "\n" << (failures == 0 ? "ALL PASSED" : std::to_string(failures) + " FAILED") << "\n";
	return failures == 0 ? 0 : 1;
}
