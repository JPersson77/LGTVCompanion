// LGTV Linux Companion — a Linux port of LGTV Companion.
//
// Copyright © 2021-2026 Jörgen Persson
// Licensed under the MIT License. See the LICENSE file at the repository root
// for the full license text, which must accompany all copies.

#include "paths.h"
#include "app_define.h"
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <unistd.h>

namespace
{
	// Return $name if it is set to a non-empty absolute path, otherwise "".
	std::string envDir(const char* name)
	{
		const char* value = std::getenv(name);
		if (!value || value[0] != '/')
			return "";
		return value;
	}
	std::string homeDir(void)
	{
		std::string home = envDir("HOME");
		if (!home.empty())
			return home;
		return "/tmp";
	}
	// $xdg_var if usable, else $HOME/<fallback>. Suffixed with the app id.
	std::string appDir(const char* xdg_var, const std::string& fallback)
	{
		std::string base = envDir(xdg_var);
		if (base.empty())
			base = homeDir() + "/" + fallback;
		return base + "/" + APP_ID;
	}
}

std::string paths::configDir(void)
{
	return appDir("XDG_CONFIG_HOME", ".config");
}
std::string paths::stateDir(void)
{
	return appDir("XDG_STATE_HOME", ".local/state");
}
std::string paths::runtimeDir(void)
{
	std::string base = envDir("XDG_RUNTIME_DIR");
	if (!base.empty())
		return base + "/" + APP_ID;

	// No runtime dir (e.g. a bare ssh session). Fall back to a uid-qualified
	// path under /tmp so two users cannot collide on the same socket.
	return "/tmp/" + std::string(APP_ID) + "-" + std::to_string(getuid());
}
std::string paths::configFile(void)
{
	return configDir() + "/" + CONFIG_FILE;
}
std::string paths::logFile(void)
{
	return stateDir() + "/" + LOG_FILE;
}
std::string paths::ipcSocket(void)
{
	return runtimeDir() + "/" + IPC_SOCKET_NAME;
}
bool paths::ensureDir(const std::string& dir)
{
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	// create_directories reports false with no error when the directory already
	// existed, so test the end state rather than the return value.
	return std::filesystem::is_directory(dir, ec);
}
