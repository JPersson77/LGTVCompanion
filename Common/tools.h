#pragma once
#include <vector>
#include <string>

namespace tools
{
	std::wstring							widen(std::string input);
	std::string								narrow(std::wstring input);
	std::string								tolower(std::string input);
	std::wstring							tolower(std::wstring input);
	std::vector<std::string>				stringsplit(std::string, std::string);
	void									replaceAllInPlace(std::string& str, const std::string& from, const std::string& to);
	std::string								validateArgument(std::string argument, std::string validation_list);
	std::vector <std::string>				getLocalIP(void);
	std::string								getSubnetMask(std::string ip);
	bool									isSameSubnet(const char* ip1, const char* ip2, const char* subnetMask);
	bool									startScheduledTask(std::wstring task_folder, std::wstring task_name);
}

