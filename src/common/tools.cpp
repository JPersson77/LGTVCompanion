// LGTV Linux Companion — a Linux port of LGTV Companion.
//
// Copyright © 2021-2026 Jörgen Persson
// Licensed under the MIT License. See the LICENSE file at the repository root
// for the full license text, which must accompany all copies.

#include "tools.h"
#include <algorithm>
#include <cstring>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
	// Run a callback over every non-loopback IPv4 interface. Centralises the
	// getifaddrs/freeifaddrs pairing so callers cannot leak the list.
	// The callback returns true to stop iterating.
	template <typename Fn>
	void forEachIPv4Interface(Fn&& fn)
	{
		struct ifaddrs* list = nullptr;
		if (getifaddrs(&list) != 0)
			return;

		for (struct ifaddrs* ifa = list; ifa != nullptr; ifa = ifa->ifa_next)
		{
			if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
				continue;
			if (ifa->ifa_flags & IFF_LOOPBACK)
				continue;
			if (!(ifa->ifa_flags & IFF_UP))
				continue;
			if (fn(ifa))
				break;
		}
		freeifaddrs(list);
	}
	std::string addrToString(const struct sockaddr* addr)
	{
		if (!addr || addr->sa_family != AF_INET)
			return "";
		char buffer[INET_ADDRSTRLEN] = {};
		const struct sockaddr_in* in = reinterpret_cast<const struct sockaddr_in*>(addr);
		if (!inet_ntop(AF_INET, &in->sin_addr, buffer, sizeof(buffer)))
			return "";
		return buffer;
	}
}

std::string tools::tolower(std::string input)
{
	if (input.length() == 0)
		return "";
	std::transform(input.begin(), input.end(), input.begin(),
		[](unsigned char c) { return static_cast<char>(::tolower(c)); });
	return input;
}
std::vector<std::string> tools::stringsplit(std::string str, std::string token) {
	std::vector<std::string>res;

	size_t f1 = str.find_first_not_of(token, 0);
	if (f1 != std::string::npos)
		str = str.substr(f1);

	while (str.size() > 0)
	{
		size_t index;
		if (str[0] == '\"') // quotation marks
		{
			index = str.find("\"", 1);
			if (index != std::string::npos)
			{
				if (index - 2 > 0)
				{
					std::string temp = str.substr(1, index - 1);
					res.push_back(temp);
				}
				size_t next = str.find_first_not_of(token, index + 1);
				if (next != std::string::npos)
					str = str.substr(next);
				else
					str = "";
			}
			else
			{
				res.push_back(str);
				str = "";
			}
		}
		else // not quotation marks
		{
			index = str.find_first_of(token, 0);
			if (index != std::string::npos)
			{
				res.push_back(str.substr(0, index));

				size_t next = str.find_first_not_of(token, index + 1);
				if (next != std::string::npos)
					str = str.substr(next);
				else
					str = "";
			}
			else {
				res.push_back(str);
				str = "";
			}
		}
	}
	return res;
}
void tools::replaceAllInPlace(std::string& str, const std::string& from, const std::string& to)
{
	if (from.empty())
		return;
	std::string wsRet;
	wsRet.reserve(str.length());
	size_t start_pos = 0, pos;
	while ((pos = str.find(from, start_pos)) != std::string::npos)
	{
		wsRet += str.substr(start_pos, pos - start_pos);
		wsRet += to;
		pos += from.length();
		start_pos = pos;
	}
	wsRet += str.substr(start_pos);
	str.swap(wsRet);
}
std::string	tools::validateArgument(std::string argument, std::string validation_list) {
	if (argument == "")
		return "";
	std::vector<std::string> list = tools::stringsplit(validation_list, " ");
	for (auto& item : list)
	{
		if (tools::tolower(item) == tools::tolower(argument))
			return item;
	}
	return "";
}
bool tools::compareUsingWildcard(const std::string& text, const std::string& pattern) {
	size_t text_index = 0;
	size_t pattern_index = 0;
	size_t star_position = std::string::npos;   // Position of '*' in pattern
	size_t text_backtrack = 0;                  // Position to backtrack in text

	while (text_index < text.size()) {
		if (pattern_index < pattern.size() &&
			(pattern[pattern_index] == '*')) {
			// Remember star position and text position
			star_position = pattern_index;
			text_backtrack = text_index;
			pattern_index++;
		}
		else if (pattern_index < pattern.size() &&
			(pattern[pattern_index] == text[text_index])) {
			// Simple match - advance both pointers
			text_index++;
			pattern_index++;
		}
		else {
			if (star_position == std::string::npos) {
				// No star to backtrack to - no match
				return false;
			}
			// Backtrack: use star to match one more character
			pattern_index = star_position + 1;
			text_index = ++text_backtrack;
		}
	}

	// Skip remaining '*' in pattern
	while (pattern_index < pattern.size() && pattern[pattern_index] == '*') {
		pattern_index++;
	}

	// Match is successful only if both reached end
	return pattern_index == pattern.size();
}
std::vector <std::string> tools::getLocalIP(void)
{
	std::vector <std::string> IPs;

	forEachIPv4Interface([&IPs](struct ifaddrs* ifa) {
		std::string ip = addrToString(ifa->ifa_addr);
		if (ip.empty() || ip == "127.0.0.1")
			return false;

		// Convert the netmask to a CIDR prefix length.
		int cidr = 0;
		if (ifa->ifa_netmask)
		{
			const struct sockaddr_in* mask = reinterpret_cast<const struct sockaddr_in*>(ifa->ifa_netmask);
			uint32_t m = mask->sin_addr.s_addr;
			while (m > 0) {
				cidr += (m & 0x01);
				m = m >> 1;
			}
		}
		IPs.push_back(ip + "/" + std::to_string(cidr));
		return false;
		});
	return IPs;
}
std::string	tools::getSubnetMask(std::string input_ip)
{
	std::string result = "255.255.255.0";

	forEachIPv4Interface([&result, &input_ip](struct ifaddrs* ifa) {
		std::string ip = addrToString(ifa->ifa_addr);
		std::string netmask = addrToString(ifa->ifa_netmask);
		if (ip.empty() || netmask.empty())
			return false;
		if (tools::isSameSubnet(input_ip.c_str(), ip.c_str(), netmask.c_str()))
		{
			result = netmask;
			return true;
		}
		return false;
		});
	return result;
}
bool tools::isSameSubnet(const char* ip1, const char* ip2, const char* subnetMask)
{
	struct in_addr addr1 {}, addr2 {}, mask {};
	if (inet_pton(AF_INET, ip1, &addr1) != 1)
		return false;
	if (inet_pton(AF_INET, ip2, &addr2) != 1)
		return false;
	if (inet_pton(AF_INET, subnetMask, &mask) != 1)
		return false;

	return (addr1.s_addr & mask.s_addr) == (addr2.s_addr & mask.s_addr);
}
std::string tools::getSourceIPforDestination(const std::string& destination_ip)
{
	struct sockaddr_in destination {};
	destination.sin_family = AF_INET;
	destination.sin_port = htons(9); // discard port; nothing is ever sent
	if (inet_pton(AF_INET, destination_ip.c_str(), &destination.sin_addr) != 1)
		return "";

	// connect() on a UDP socket only performs the route lookup, no traffic.
	int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return "";

	std::string result;
	if (::connect(fd, reinterpret_cast<struct sockaddr*>(&destination), sizeof(destination)) == 0)
	{
		struct sockaddr_in source {};
		socklen_t length = sizeof(source);
		if (::getsockname(fd, reinterpret_cast<struct sockaddr*>(&source), &length) == 0)
			result = addrToString(reinterpret_cast<struct sockaddr*>(&source));
	}
	::close(fd);
	return result;
}
std::string tools::getInterfaceForIP(const std::string& ip)
{
	std::string result;

	forEachIPv4Interface([&result, &ip](struct ifaddrs* ifa) {
		if (addrToString(ifa->ifa_addr) == ip && ifa->ifa_name)
		{
			result = ifa->ifa_name;
			return true;
		}
		return false;
		});
	return result;
}
std::string tools::getBroadcastForIP(const std::string& ip)
{
	std::string result;

	forEachIPv4Interface([&result, &ip](struct ifaddrs* ifa) {
		if (addrToString(ifa->ifa_addr) != ip)
			return false;
		if (ifa->ifa_flags & IFF_BROADCAST)
			result = addrToString(ifa->ifa_broadaddr);
		return true;
		});
	return result;
}
