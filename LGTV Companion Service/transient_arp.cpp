#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "transient_arp.h"
#include <sstream>

std::optional<NET_LUID> TransientArp::getLocalInterface(SOCKADDR_INET destination, NET_LUID& nic_luid, std::string& arpStatus) {
	MIB_IPFORWARD_ROW2 row;
	SOCKADDR_INET bestSourceAddress;
	std::stringstream arp;
	const auto result = GetBestRoute2(&nic_luid, 0, NULL, &destination, 0, &row, &bestSourceAddress);

	if (result != NO_ERROR) {
		arp << "Failed to create transient ARP. ";
		arpStatus = arp.str();
		return {};
	}
	char* source_ip = inet_ntoa(bestSourceAddress.Ipv4.sin_addr);
	arp << "ARP override best source: " << source_ip << ", Interface index: " << row.InterfaceIndex << ", LUID: " << row.InterfaceLuid.Value << ", Route protocol: " << row.Protocol;

	if (row.Protocol != MIB_IPPROTO_LOCAL) {
		arp << ", Route is not local, aborting!";
		return {};
	}
	arpStatus = arp.str();

	return row.InterfaceLuid;
}
void TransientArp::createTransientLocalNetEntry(std::string ip, unsigned char mac[6], NET_LUID& nic_luid, std::string& arpStatus) {
	MIB_IPNET_ROW2 row;
	struct sockaddr_in destination {};
	destination.sin_family = AF_INET;
	destination.sin_port = htons(9);
	destination.sin_addr.s_addr = inet_addr(ip.c_str());
	const auto luid = getLocalInterface(reinterpret_cast<const SOCKADDR_INET&>(destination), nic_luid, arpStatus);
	if (!luid.has_value())
		return;
	
	row.Address = reinterpret_cast<const SOCKADDR_INET&>(destination);
	row.InterfaceLuid = *luid;
	memcpy(row.PhysicalAddress, mac, sizeof(mac));
	row.PhysicalAddressLength = sizeof(mac);
	const auto result = CreateIpNetEntry2(&row);
	if (result != NO_ERROR)
		return;
	net_entry_deleter_ = std::make_unique<NetEntryDeleter>(*luid, reinterpret_cast<const SOCKADDR_INET&>(destination));
	return;
}
