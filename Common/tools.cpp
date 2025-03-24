#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0603
#define _WIN32_WINNT 0x0603
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "tools.h"
#include <Windows.h>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <algorithm>
//#include <MSTask.h>
#include <taskschd.h>
//#include <atlbase.h>
#include <comdef.h>
//#include <iostream>

//#pragma comment(lib, "mstask.lib")
#pragma comment(lib, "taskschd.lib")
//#pragma comment(lib, "comsupp.lib")

#pragma comment(lib, "Ws2_32.lib")

std::wstring tools::widen(std::string input) {
	if (input == "")
		return L"";
	// Calculate target buffer size
	long len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.c_str(), (int)input.size(), NULL, 0);
	if (len == 0)
		return L"";
	// Convert character sequence
	std::wstring out(len, 0);
	if (len != MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.c_str(), (int)input.size(), &out[0], (int)out.size()))
		return L"";
	return out;
}
std::string tools::narrow(std::wstring input) {
	// Calculate target buffer size
	long len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.c_str(), (int)input.size(),
		NULL, 0, NULL, NULL);
	if (len == 0)
		return "";
	// Convert character sequence
	std::string out(len, 0);
	if (len != WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.c_str(), (int)input.size(), &out[0], (int)out.size(), NULL, NULL))
		return "";
	return out;
}
std::string tools::tolower(std::string input)
{
	if (input.length() == 0)
		return "";
	transform(input.begin(), input.end(), input.begin(), ::tolower);
	return input;
}
std::wstring tools::tolower(std::wstring input)
{
	if (input.length() == 0)
		return L"";
	transform(input.begin(), input.end(), input.begin(), ::tolower);
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
					str = str.substr(next); //  str.substr(index + token.size());
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
					str = str.substr(next); //  str.substr(index + token.size());
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
std::vector <std::string> tools::getLocalIP(void)
{
	std::vector <std::string> IPs;
	std::string ip;

	// Get network interface info
	SOCKET socketObj = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketObj != INVALID_SOCKET)
	{
		INTERFACE_INFO adapterInfo[100] = {};
		DWORD bufferSize = sizeof(adapterInfo);
		DWORD dwBytesReturned = 0;
		DWORD dwNumInterfaces = 0;

		if (WSAIoctl(socketObj, SIO_GET_INTERFACE_LIST, NULL, 0, (void*)adapterInfo, bufferSize, &dwBytesReturned, nullptr, nullptr) != SOCKET_ERROR)
		{
			dwNumInterfaces = dwBytesReturned / sizeof(INTERFACE_INFO);
			for (DWORD index = 0; index < dwNumInterfaces; index++)
			{
				ip = "";
				char szIP[120] = {};
				char szNetMask[120] = {};
				if (adapterInfo[index].iiAddress.Address.sa_family == AF_INET)
				{
					sockaddr_in* pAddr4 = &adapterInfo[index].iiAddress.AddressIn;
					inet_ntop(AF_INET, &pAddr4->sin_addr, szIP, ARRAYSIZE(szIP));
					ip = std::string(szIP);
					pAddr4 = &adapterInfo[index].iiNetmask.AddressIn;
					inet_ntop(AF_INET, &pAddr4->sin_addr, szNetMask, ARRAYSIZE(szNetMask));

					if (ip != "127.0.0.1" && ip != "")
					{
						std::string v;
						int CIDR = 0;
						int m;
						inet_pton(AF_INET, szNetMask, &m);
						while (m > 0) {
							CIDR += (m & 0x01);
							m = m >> 1;
						}
						v = ip;
						v += "/";
						v += std::to_string(CIDR);
						IPs.push_back(v);
					}
				}

			}
		}
		closesocket(socketObj);
	}
	return IPs;
}
std::string	tools::getSubnetMask(std::string input_ip)
{


//	std::vector <std::string> IPs;
	std::string ip;
	std::string netmask;

	// Get network interface info
	SOCKET socketObj = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketObj != INVALID_SOCKET)
	{
		INTERFACE_INFO adapterInfo[100] = {};
		DWORD bufferSize = sizeof(adapterInfo);
		DWORD dwBytesReturned = 0;
		DWORD dwNumInterfaces = 0;

		if (WSAIoctl(socketObj, SIO_GET_INTERFACE_LIST, NULL, 0, (void*)adapterInfo, bufferSize, &dwBytesReturned, nullptr, nullptr) != SOCKET_ERROR)
		{
			dwNumInterfaces = dwBytesReturned / sizeof(INTERFACE_INFO);
			for (DWORD index = 0; index < dwNumInterfaces; index++)
			{
				ip = "";
				char szIP[120] = {};
				char szNetMask[120] = {};
				if (adapterInfo[index].iiAddress.Address.sa_family == AF_INET)
				{
					sockaddr_in* pAddr4 = &adapterInfo[index].iiAddress.AddressIn;
					inet_ntop(AF_INET, &pAddr4->sin_addr, szIP, ARRAYSIZE(szIP));
					ip = std::string(szIP);
					pAddr4 = &adapterInfo[index].iiNetmask.AddressIn;
					inet_ntop(AF_INET, &pAddr4->sin_addr, szNetMask, ARRAYSIZE(szNetMask));
					netmask = std::string(szNetMask);
					if (tools::isSameSubnet(input_ip.c_str(), ip.c_str(), netmask.c_str()))
					{
						closesocket(socketObj);
						return netmask;
					}
				}
			}
		}
		closesocket(socketObj);
	}
	return "255.255.255.0";
}
bool tools::isSameSubnet(const char* ip1, const char* ip2, const char* subnetMask)
{
	in_addr addr1, addr2, mask;
	inet_pton(AF_INET, ip1, &addr1);
	inet_pton(AF_INET, ip2, &addr2);
	inet_pton(AF_INET, subnetMask, &mask);

	return (addr1.s_addr & mask.s_addr) == (addr2.s_addr & mask.s_addr);
}
bool tools::startScheduledTask(std::wstring task_folder, std::wstring task_name)
{
	
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr))
		return false;

	ITaskService* pService = nullptr;
	ITaskFolder* pRootFolder = nullptr;
	IRegisteredTask* pTask = nullptr;
	IRunningTask* pRunningTask = nullptr;

	hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&pService);
	if (FAILED(hr)) {
		CoUninitialize();
		return false;
	}

	hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
	if (FAILED(hr)) {
		pService->Release();
		CoUninitialize();
		return false;
	}

	// Get the root folder of Task Scheduler	
	hr = pService->GetFolder(_bstr_t(task_folder.c_str()), &pRootFolder);
	if (FAILED(hr)) {
		pService->Release();
		CoUninitialize();
		return false;
	}

	// Activate the task by name
	hr = pRootFolder->GetTask(_bstr_t(task_name.c_str()), &pTask);
	if (FAILED(hr)) {
		pRootFolder->Release();
		pService->Release();
		CoUninitialize();
		return false;
	}

	// Run the task
	hr = pTask->Run(_variant_t(), &pRunningTask);
	if (FAILED(hr)) {
		pTask->Release();
		pRootFolder->Release();
		pService->Release();
		CoUninitialize();
		return false;
	}
	pRunningTask->Release();
	pTask->Release();
	pRootFolder->Release();
	pService->Release();
	CoUninitialize();
	return true;
}