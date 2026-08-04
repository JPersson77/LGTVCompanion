// LGTV Linux Companion — a Linux port of LGTV Companion.
//
// Copyright © 2021-2026 Jörgen Persson
// Licensed under the MIT License. See the LICENSE file at the repository root
// for the full license text, which must accompany all copies.

#pragma once
#include <string>

// XDG base directory resolution. Replaces the windows SHGetFolderPath/CSIDL
// lookups used by the upstream project.
namespace paths
{
	// ~/.config/lgtv-companion  ($XDG_CONFIG_HOME/lgtv-companion)
	std::string configDir(void);
	// ~/.local/state/lgtv-companion  ($XDG_STATE_HOME/lgtv-companion)
	std::string stateDir(void);
	// /run/user/<uid>/lgtv-companion  ($XDG_RUNTIME_DIR/lgtv-companion)
	std::string runtimeDir(void);

	// Full paths to the individual files.
	std::string configFile(void);
	std::string logFile(void);
	std::string ipcSocket(void);

	// Create a directory and any missing parents. Returns true if the directory
	// exists once the call returns.
	bool ensureDir(const std::string& dir);
}
