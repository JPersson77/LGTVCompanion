// LGTV Linux Companion — a Linux port of LGTV Companion.
//
// Copyright © 2021-2026 Jörgen Persson
// Licensed under the MIT License. See the LICENSE file at the repository root
// for the full license text, which must accompany all copies.

#pragma once
#include <string>
#include <vector>

// Strings are UTF-8 std::string throughout. The upstream widen()/narrow()
// helpers existed only to cross the windows UTF-16 boundary and are gone.
namespace tools
{
	std::string								tolower(std::string input);
	std::vector<std::string>				stringsplit(std::string, std::string);
	void									replaceAllInPlace(std::string& str, const std::string& from, const std::string& to);
	std::string								validateArgument(std::string argument, std::string validation_list);
	bool									compareUsingWildcard(const std::string& text, const std::string& pattern);

	// Network helpers, backed by getifaddrs(3).
	// Each entry is "a.b.c.d/cidr" for every non-loopback IPv4 interface.
	std::vector <std::string>				getLocalIP(void);
	std::string								getSubnetMask(std::string ip);
	bool									isSameSubnet(const char* ip1, const char* ip2, const char* subnetMask);

	// Source address the kernel would use to reach the given destination, or ""
	// on failure. Replaces GetBestRoute2(); a connect() on a UDP socket performs
	// the route lookup without sending anything.
	std::string								getSourceIPforDestination(const std::string& destination_ip);
	// Name of the interface owning the given local address, or "".
	std::string								getInterfaceForIP(const std::string& ip);
	// Broadcast address of the interface owning the given local address, or "".
	std::string								getBroadcastForIP(const std::string& ip);
}
