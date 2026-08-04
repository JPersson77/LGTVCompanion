// LGTV Linux Companion — a Linux port of LGTV Companion.
//
// Copyright © 2021-2026 Jörgen Persson
// Licensed under the MIT License. See the LICENSE file at the repository root
// for the full license text, which must accompany all copies.

#pragma once

// common application definitions
#define			APPNAME							"LGTV Companion"
#define			APP_ID							"lgtv-companion"
#define         APP_VERSION                     "5.6.0"
#define			CONFIG_FILE						"config.json"
#define			LOG_FILE						"log.txt"

// Unix domain socket name, created inside the runtime directory. Replaces the
// windows named pipe (\\.\pipe\LGTVyolo) used by the upstream project.
#define			IPC_SOCKET_NAME					"ipc.sock"

#define			NOTIFY_NEW_COMMANDLINE			1

#define         NEWRELEASELINK                  "https://github.com/JPersson77/LGTVCompanion/releases"
#define         DONATELINK                      "https://www.paypal.me/jpersson77"
#define         DISCORDLINK                     "https://discord.gg/7KkTPrP3fq"
