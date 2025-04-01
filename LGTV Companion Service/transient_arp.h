#pragma once
#include <optional>
#include <memory>
#include <string>
#include <WS2tcpip.h>
#include <Iphlpapi.h>

#pragma comment(lib, "Iphlpapi.lib")

class NetEntryDeleter
{
public:
	NetEntryDeleter(const NetEntryDeleter&) = delete;
	NetEntryDeleter& operator=(const NetEntryDeleter&) = delete;

	NetEntryDeleter(NET_LUID luid, SOCKADDR_INET address) : luid(luid), address(address) {}

	~NetEntryDeleter() {
		MIB_IPNET_ROW2 row;
		row.InterfaceLuid = luid;
		row.Address = address;
		DeleteIpNetEntry2(&row);
	}
private:
	const NET_LUID luid;
	const SOCKADDR_INET address;
};
class TransientArp
{
private:
	std::unique_ptr<NetEntryDeleter> net_entry_deleter_;
	std::optional<NET_LUID> getLocalInterface(SOCKADDR_INET, NET_LUID&, std::string&);
public:
	TransientArp() {};
	~TransientArp() {};
	void createTransientLocalNetEntry(std::string, unsigned char mac[6], NET_LUID&, std::string&);
};

