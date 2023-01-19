// See LGTV Companion UI.cpp for additional details
#include "Service.h"
//#include <boost/beast/websocket/ssl.hpp>
//#include <boost/asio/ssl/stream.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
//#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

using namespace std;
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using json = nlohmann::json;



mutex mMutex;

namespace {
	class NetEntryDeleter final {
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

	boost::optional<NET_LUID> GetLocalInterface(SOCKADDR_INET destination, string sDev) {
		MIB_IPFORWARD_ROW2 row;
		SOCKADDR_INET bestSourceAddress;
		const auto result = GetBestRoute2(NULL, 0, NULL, &destination, 0, &row, &bestSourceAddress);

		std::stringstream log;
		log << sDev <<", GetBestRoute2()";

		if (result != NO_ERROR) {
			log << " failed with code " << result;
			Log(log.str());
			return boost::none;
		}

		log << " selected interface index " << row.InterfaceIndex << " LUID " << row.InterfaceLuid.Value << " route protocol " << row.Protocol;

		if (row.Protocol != MIB_IPPROTO_LOCAL) {
			log << "; route is not local, aborting";
			Log(log.str());
			return boost::none;
		}

		Log(log.str());
		return row.InterfaceLuid;
	}

	std::unique_ptr<NetEntryDeleter> CreateTransientLocalNetEntry(SOCKADDR_INET destination, unsigned char macAddress[6], string sDev) {
		const auto luid = GetLocalInterface(destination, sDev);
		if (!luid.has_value()) return nullptr;

		MIB_IPNET_ROW2 row;
		row.Address = destination;
		row.InterfaceLuid = *luid;
		memcpy(row.PhysicalAddress, macAddress, sizeof(macAddress));
		row.PhysicalAddressLength = sizeof(macAddress);
		const auto result = CreateIpNetEntry2(&row);

//		std::stringstream log;
//		log << "CreateIpNetEntry2() = " << result;
//		Log(log.str());

		if (result != NO_ERROR) return nullptr;
		return std::make_unique<NetEntryDeleter>(*luid, destination);
	}
}

CSession::CSession(SESSIONPARAMETERS* Params)
{
	Parameters = *Params;
}
CSession::~CSession()
{
}
void CSession::Run()
{
	//thread safe section
	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);
	Parameters.Enabled = true;
	mMutex.unlock();
}
void CSession::RemoteHostConnected()
{
	//thread safe section
	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);
	bRemoteHostConnected = true;
	mMutex.unlock();
}
void CSession::RemoteHostDisconnected()
{
	//thread safe section
	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);
	bRemoteHostConnected = false;
	mMutex.unlock();
}
void CSession::Stop()
{
	//thread safe section
	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);
	Parameters.Enabled = false;
	mMutex.unlock();
}
SESSIONPARAMETERS CSession::GetParams(void)
{
	SESSIONPARAMETERS copy;
	//thread safe section
	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);
	copy = Parameters;
	mMutex.unlock();
	return copy;
}
bool CSession::IsBusy(void)
{
	bool ret;
	//thread safe section
	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);
	ret = ThreadedOpDisplayOn || ThreadedOpDisplayOff || ThreadedOpDisplaySetHdmiInput;
	mMutex.unlock();
	return ret;
}
void CSession::SetTopology(bool bEnabled)
{
	//thread safe section
	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);
	TopologyEnabled = bEnabled;
	AdhereTopology = true;
	mMutex.unlock();
	return;
}
bool CSession::GetTopology(void)
{
	//thread safe section
	bool b;
	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);
	b = TopologyEnabled;
	mMutex.unlock();
	return b;
}
void CSession::SetDisplayHdmiInput(int HdmiInput)
{
	string s;

	//thread safe section
	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);

	if (!ThreadedOpDisplaySetHdmiInput)
	{
		ThreadedOpDisplaySetHdmiInput = true;
		s = Parameters.DeviceId;
		s += ", spawning DisplaySetHdmiInputThread().";

		thread thread_obj(SetDisplayHdmiInputThread, &Parameters, &ThreadedOpDisplaySetHdmiInput, Parameters.PowerOnTimeout, HdmiInput);
		thread_obj.detach();
	}
	else
	{
		s = Parameters.DeviceId;
		s += ", omitted DisplaySetHdmiInputThread().";
	}

	mMutex.unlock();
	Log(s);
	return;
}
void CSession::TurnOnDisplay(bool SendWOL)
{
	string s;

	//thread safe section
	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);

	ScreenDimmedRequestTime = 0;

	if (!ThreadedOpDisplayOn)
	{
		s = Parameters.DeviceId;
		s += ", spawning DisplayPowerOnThread().";

		ThreadedOpDisplayOn = true;
		//       ThreadedOperationsTimeStamp = time(0);

		thread thread_obj(DisplayPowerOnThread, &Parameters, &ThreadedOpDisplayOn, Parameters.PowerOnTimeout, SendWOL);
		thread_obj.detach();

	}
	else
	{
		s = Parameters.DeviceId;
		s += ", omitted DisplayPowerOnThread().";
	}
	mMutex.unlock();
	Log(s);
	return;
}
void CSession::TurnOffDisplay(bool forced, bool dimmed, bool blankscreen)
{
	string s;

	//thread safe section
	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);

	if (ThreadedOpDisplayOffTime == 0)
		ThreadedOpDisplayOffTime = time(0);

	if (
		(!ThreadedOpDisplayOff || (time(0) - ThreadedOpDisplayOffTime > THREAD_WAIT)) &&
		Parameters.SessionKey != "" &&
		(time(0) - ScreenDimmedRequestTime > DIMMED_OFF_DELAY_WAIT))
	{
		s = Parameters.DeviceId;
		s += ", spawning DisplayPowerOffThread().";

		ThreadedOpDisplayOff = true;
		ThreadedOpDisplayOffTime = time(0);
		thread thread_obj(DisplayPowerOffThread, &Parameters, &ThreadedOpDisplayOff, forced, blankscreen);
		thread_obj.detach();
	}
	else
	{
		s = Parameters.DeviceId;
		s += ", omitted DisplayPowerOffThread()";
		if (Parameters.SessionKey == "")
			s += " (no pairing key).";
		else
			s += ".";
	}
	if (dimmed)
		ScreenDimmedRequestTime = time(0);
	mMutex.unlock();
	Log(s);
	return;
}
//   This function contains the logic for when a display power event (on/off) has occured which the device shall respond to
void CSession::SystemEvent(DWORD dwMsg, int param)
{
	// forced events are always processed, i.e. the user has issued this command.
	if (dwMsg == SYSTEM_EVENT_FORCEON)
		TurnOnDisplay(true);
	if (dwMsg == SYSTEM_EVENT_FORCEOFF)
		TurnOffDisplay(true, false, false);
	if (dwMsg == SYSTEM_EVENT_FORCESCREENOFF)
		TurnOffDisplay(true, false, true);
	if (dwMsg == SYSTEM_EVENT_FORCESETHDMI)
		SetDisplayHdmiInput(param);

	//thread safe section
	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);
	if (!Parameters.Enabled)
	{
		mMutex.unlock();
		return;
	}
	mMutex.unlock();

	switch (dwMsg)
	{
	case SYSTEM_EVENT_REBOOT:
	{
		bRemoteHostConnected = false;
		ActivePowerState = false;
	}break;
	case SYSTEM_EVENT_UNSURE:
	case SYSTEM_EVENT_SHUTDOWN:
	{
		TurnOffDisplay(false, false, false);
		ActivePowerState = false;
		bRemoteHostConnected = false;
	}break;
	case SYSTEM_EVENT_SUSPEND:
	{
		bRemoteHostConnected = false;
	}break;
	case SYSTEM_EVENT_RESUME:
	{
		bRemoteHostConnected = false;
	}break;
	case SYSTEM_EVENT_RESUMEAUTO:
	{
		if (Parameters.SetHDMIInputOnResume)
		{
			if (AdhereTopology)
			{
				if (TopologyEnabled)
					SetDisplayHdmiInput(Parameters.SetHDMIInputOnResumeToNumber);
			}
			else
				SetDisplayHdmiInput(Parameters.SetHDMIInputOnResumeToNumber);
		}
		bRemoteHostConnected = false;
	}break;

	case SYSTEM_EVENT_DISPLAYON:
	{
		if (bRemoteHostConnected)
			break;
		if (AdhereTopology)
		{
			if (TopologyEnabled)
				TurnOnDisplay(true);
		}
		else
			TurnOnDisplay(true);
		ActivePowerState = true;
	}break;
	case SYSTEM_EVENT_DISPLAYOFF:
	{
		if (bRemoteHostConnected)
			break;
		TurnOffDisplay(false, false, false);
		ActivePowerState = false;
	}break;
	case SYSTEM_EVENT_DISPLAYDIMMED:
	{
		if (bRemoteHostConnected)
			break;
		TurnOffDisplay(false, true, false);
		ActivePowerState = false;
	}break;
	case SYSTEM_EVENT_USERIDLE:
	{
		if (bRemoteHostConnected)
			break;
		if (Parameters.BlankWhenIdle)
		{
			if (AdhereTopology)
			{
				if (TopologyEnabled)
					TurnOffDisplay(false, false, true);
			}
			else 
				TurnOffDisplay(false, false, true);
			ActivePowerState = false;
		}
	}break;
	case SYSTEM_EVENT_USERBUSY:
	{
		if (bRemoteHostConnected)
			break;
		if (Parameters.BlankWhenIdle)
		{
			if (AdhereTopology)
			{
				if (TopologyEnabled)
					TurnOnDisplay(true);
			}
			else
				TurnOnDisplay(true);
			ActivePowerState = true;
		}
	}break;
	case SYSTEM_EVENT_BOOT:
	{
		if (Parameters.SetHDMIInputOnResume)
		{
			if (AdhereTopology)
			{
				if(TopologyEnabled)
					SetDisplayHdmiInput(Parameters.SetHDMIInputOnResumeToNumber);
			}
			else
				SetDisplayHdmiInput(Parameters.SetHDMIInputOnResumeToNumber);
		}
	}break;
	case SYSTEM_EVENT_TOPOLOGY:
	{
		if (bRemoteHostConnected)
			break;
		if (!ActivePowerState)
			break;
		if (AdhereTopology)
		{
			if (TopologyEnabled)
				TurnOnDisplay(true);
			else
				TurnOffDisplay(false, false, false);
		}
	}break;
	default:break;
	}
}
//   THREAD: Spawned when the device should power ON. This thread manages the pairing key from the display and verifies that the display has been powered on
void DisplayPowerOnThread(SESSIONPARAMETERS* CallingSessionParameters, bool* CallingSessionThreadRunning, int Timeout, bool SendWOL)
{
	string screenonmess = R"({"id":"2","type" : "request","uri" : "ssap://com.webos.service.tvpower/power/turnOnScreen"})";
	string getpowerstatemess = R"({"id": "1", "type": "request", "uri": "ssap://com.webos.service.tvpower/power/getPowerState"})";

	if (!CallingSessionParameters)
		return;

	string hostip = CallingSessionParameters->IP;
	string key = CallingSessionParameters->SessionKey;
	string device = CallingSessionParameters->DeviceId;
	bool SSL = CallingSessionParameters->SSL;
	string logmsg;
	string ck = "CLIENTKEYx0x0x0";
	string handshake;
	time_t origtim = time(0);

	if (SendWOL)
	{
		thread wolthread(WOLthread, CallingSessionParameters, CallingSessionThreadRunning, Timeout);
		wolthread.detach();
	}

	// build the appropriate WebOS handshake
	if (key == "")
		handshake = narrow(HANDSHAKE_NOTPAIRED);
	else
	{
		handshake = narrow(HANDSHAKE_PAIRED);
		size_t ckf = handshake.find(ck);
		handshake.replace(ckf, ck.length(), key);
	}

	//try waking up the display ten times, but not longer than timeout user preference
	while (time(0) - origtim < (Timeout + 1))
	{
		time_t looptim = time(0);
		try
		{
			//BOOST::BEAST
			beast::flat_buffer buffer;
			net::io_context ioc;
			tcp::resolver resolver{ ioc };	
			
			//context holds certificates
			ssl::context ctx{ ssl::context::tlsv12_client };
			//load_root_certificates(ctx); 

		
			//SSL
			websocket::stream<beast::ssl_stream<tcp::socket>> wss{ ioc, ctx };	
			websocket::stream<tcp::socket> ws{ ioc };


			string host = hostip;

			// try communicating with the display
			auto const results = resolver.resolve(host, SSL?SERVICE_PORT_SSL:SERVICE_PORT);
			auto ep = net::connect(SSL? get_lowest_layer(wss):ws.next_layer(), results);

			//SSL set SNI Hostname
			if(SSL)
				SSL_set_tlsext_host_name(wss.next_layer().native_handle(), host.c_str());

			//build the host string for the decorator
			host += ':' + std::to_string(ep.port());
			
			//SSL handshake
			if(SSL)
				wss.next_layer().handshake(ssl::stream_base::client);

			if (SSL)
			{
				wss.set_option(websocket::stream_base::decorator(
					[](websocket::request_type& req)
					{
						req.set(http::field::user_agent,
							std::string(BOOST_BEAST_VERSION_STRING) +
							" websocket-client-LGTVsvc");
					}));
				wss.handshake(host, "/");
				wss.write(net::buffer(std::string(handshake)));
				wss.read(buffer); // read the first response
			}
			else
			{
				ws.set_option(websocket::stream_base::decorator(
					[](websocket::request_type& req)
					{
						req.set(http::field::user_agent,
							std::string(BOOST_BEAST_VERSION_STRING) +
							" websocket-client-LGTVsvc");
					}));
				ws.handshake(host, "/");
				ws.write(net::buffer(std::string(handshake)));
				ws.read(buffer); // read the first response
			}

			//debuggy
			{
				string tt = beast::buffers_to_string(buffer.data());
				string st = SSL ? "[DEBUG] (SSL) ON response 1: " : "[DEBUG] ON Response 1: ";
				st += tt;
				Log(st);
			}

			json j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
			boost::string_view check = j["type"];

			buffer.consume(buffer.size());

			// parse for pairing key if needed
			if (std::string(check) != "registered")
			{
				
				if (key != "")
				{
					logmsg = device;
					logmsg += ", WARNING! Pairing key is invalid. Re-pairing forced!";
					Log(logmsg);

					handshake = narrow(HANDSHAKE_NOTPAIRED);
					if (SSL)
					{
						wss.write(net::buffer(std::string(handshake)));
						wss.read(buffer); // read the first response
					}
					else
					{
						ws.write(net::buffer(std::string(handshake)));
						ws.read(buffer); // read the first response
					}
				}

				buffer.consume(buffer.size());

				if(SSL)
					wss.read(buffer);
				else
					ws.read(buffer); //read the second response which should now contain the session key
				string t = beast::buffers_to_string(buffer.data());
							
				//debuggy
				string ss = SSL?"[DEBUG] (SSL) ON response key: ": "[DEBUG] ON response key: ";
				ss += t;
				Log(ss);
				
				buffer.consume(buffer.size());

				size_t u = t.find("client-key\":\"");
				if (u != string::npos) // so did we get a session key?
				{
					size_t w = t.find("\"", u + 14);

					if (w != string::npos)
					{
						key = t.substr(u + 13, w - u - 13);

						//thread safe section
						while (!mMutex.try_lock())
							Sleep(MUTEX_WAIT);
						CallingSessionParameters->SessionKey = key;
						SetSessionKey(key, device);
						mMutex.unlock();
					}
				}
				else
				{
					logmsg = device;
					logmsg += ", WARNING! Pairing key expected but not received.";
					Log(logmsg);
				}
			}

			if (SSL)
			{
				wss.write(net::buffer(std::string(screenonmess)));
				wss.read(buffer);
			}
			else
			{
				ws.write(net::buffer(std::string(screenonmess)));
				ws.read(buffer);
			}
			//debuggy
			{
				string tt = beast::buffers_to_string(buffer.data());
				string st = SSL ? "[DEBUG] (SSL) ON response 2: " : "[DEBUG] ON response 2: ";
				st += tt;
				Log(st);
			}
			buffer.consume(buffer.size());
						
			//retreive power state from device to determine if the device is powered on
			if (SSL)
			{
				wss.write(net::buffer(getpowerstatemess));
				wss.read(buffer);
			}
			else
			{
				ws.write(net::buffer(getpowerstatemess));
				ws.read(buffer);
			}
			//debuggy
			{
				string tt = beast::buffers_to_string(buffer.data());
				string st = SSL ? "[DEBUG] (SSL) ON response 3: " : "[DEBUG] ON response 3: ";
				st += tt;
				Log(st);
			}

			j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());

			boost::string_view type = j["type"];
			boost::string_view state = j["payload"]["state"];
			if (std::string(type) == "response")
			{
				if (std::string(state) == "Active")
				{
					logmsg = device;
					logmsg += ", power state is: ON";
					Log(logmsg);

					buffer.consume(buffer.size());
					if(SSL)
						wss.close(websocket::close_code::normal);
					else
						ws.close(websocket::close_code::normal);
					break;
				}
			}
			buffer.consume(buffer.size());
			if (SSL)
				wss.close(websocket::close_code::normal);
			else
				ws.close(websocket::close_code::normal);

		}
		catch (std::exception const& e)
		{
			logmsg = device;
			logmsg += ", WARNING! DisplayPowerOnThread(): ";
			logmsg += e.what();
			Log(logmsg);
		}
		time_t endtim = time(0);
		time_t execution_time = endtim - looptim;
		if (execution_time < 6)
			Sleep((6 - (DWORD)execution_time) * 1000);
	}

	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);
	*CallingSessionThreadRunning = false;
	mMutex.unlock();
	return;
}
//   THREAD: Spawned to broadcast WOL (this is what actually wakes the display up)
void WOLthread(SESSIONPARAMETERS* CallingSessionParameters, bool* CallingSessionThreadRunning, int Timeout)
{
	if (!CallingSessionParameters)
		return;
	if (CallingSessionParameters->MAC.size() < 1)
		return;

	vector<string> MACs = CallingSessionParameters->MAC;
	string IP = CallingSessionParameters->IP;
	string device = CallingSessionParameters->DeviceId;
	string subnet = CallingSessionParameters->Subnet;
	int WOLtype = CallingSessionParameters->WOLtype;

	SOCKET WOLsocket = INVALID_SOCKET;
	string logmsg;

	try
	{
		//WINSOCKET
		struct sockaddr_in LANDestination {};
		LANDestination.sin_family = AF_INET;
		LANDestination.sin_port = htons(9);

		stringstream wolstr;

		if (WOLtype == WOL_SUBNETBROADCAST && subnet != "")
		{
			vector<string> vIP = stringsplit(IP, ".");
			vector<string> vSubnet = stringsplit(subnet, ".");
			stringstream broadcastaddress;

			if (vIP.size() == 4 && vSubnet.size() == 4)
			{
				for (int i = 0; i < 4; i++)
				{
					int a = atoi(vIP[i].c_str());
					int b = atoi(vSubnet[i].c_str());
					int c = 256 + (a | (~b));

					broadcastaddress << c;
					if (i < 3)
						broadcastaddress << ".";
				}

				stringstream ss;

				wolstr << " using broadcast address: " << broadcastaddress.str();

				LANDestination.sin_addr.s_addr = inet_addr(broadcastaddress.str().c_str());
			}
			else
			{
				stringstream ss;
				ss << device;
				ss << ", ERROR! WOLthread malformed subnet/IP";
				Log(ss.str());
				return;
			}

			//            LANDestination.sin_addr.s_addr = 0xFFFFFFFF;
		}
		else if (WOLtype == WOL_IPSEND)
		{
			LANDestination.sin_addr.s_addr = inet_addr(IP.c_str());
			wolstr << " using IP address: " << IP;
		}
		else
		{
			LANDestination.sin_addr.s_addr = 0xFFFFFFFF;
			wolstr << " using network broadcast: 255.255.255.255";
		}

		time_t origtim = time(0);

		WOLsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		if (WOLsocket == INVALID_SOCKET)
		{
			int dw = WSAGetLastError();
			stringstream ss;
			ss << device;
			ss << ", ERROR! WOLthread WS socket(): ";
			ss << dw;
			Log(ss.str());
			return;
		}
		else
		{
			const bool optval = TRUE;
			if (setsockopt(WOLsocket, SOL_SOCKET, SO_BROADCAST, (char*)&optval, sizeof(optval)) == SOCKET_ERROR)
			{
				closesocket(WOLsocket);
				int dw = WSAGetLastError();
				stringstream ss;
				ss << device;
				ss << ", ERROR! WOLthread WS setsockopt(): ";
				ss << dw;
				Log(ss.str());
				return;
			}
		}
		for (auto& MAC : MACs)
		{
			logmsg = device;
			logmsg += ", repeating WOL broadcast started to MAC: ";
			logmsg += MAC;
			if (wolstr.str() != "")
				logmsg += wolstr.str();
			Log(logmsg);

			//remove filling from MAC
			char CharsToRemove[] = ".:- ";
			for (int i = 0; i < strlen(CharsToRemove); ++i)
				MAC.erase(remove(MAC.begin(), MAC.end(), CharsToRemove[i]), MAC.end());
		}

		//send WOL packet every other second until timeout, or until the calling thread has ended
		while (time(0) - origtim < (Timeout + 1))
		{
			time_t looptim = time(0);
			if (WOLsocket != INVALID_SOCKET)
			{
				// build and broadcast magic packet(s) for every MAC
				for (auto& MAC : MACs)
				{
					//Build a magic packet (6 x 0xFF followed by 16 x MAC) and broadcast it
					if (MAC.length() == 12)
					{
						unsigned char Message[102];
						unsigned char MACstr[6];

						for (auto i = 0; i < 6; ++i)
							Message[i] = 0xFF;

						for (auto i = 0; i < 6; ++i)
							MACstr[i] = static_cast<unsigned char>(std::stoul(MAC.substr(i * 2, 2), nullptr, 16));

						for (auto i = 1; i <= 16; ++i)
							memcpy(&Message[i * 6], &MACstr, 6 * sizeof(unsigned char));

						std::unique_ptr<NetEntryDeleter> netEntryDeleter;
						if (WOLtype == WOL_IPSEND) netEntryDeleter = CreateTransientLocalNetEntry(reinterpret_cast<const SOCKADDR_INET&>(LANDestination), MACstr, device);

						// Send Wake On LAN packet
						if (sendto(WOLsocket, (char*)&Message, 102, 0, reinterpret_cast<sockaddr*>(&LANDestination), sizeof(LANDestination)) == SOCKET_ERROR)
						{
							int dw = WSAGetLastError();
							stringstream ss;
							ss << device;
							ss << ", WARNING! WOLthread WS sendto(): ";
							ss << dw;
							Log(ss.str());
						}
					}
					else
					{
						logmsg = device;
						logmsg += ", WARNING! WOLthread malformed MAC";
						Log(logmsg);
					}
				}
			}
			else
			{
				logmsg = device;
				logmsg += ", WARNING! WOLthread illegal socket reference";
				Log(logmsg);
			}

			time_t endtim = time(0);
			time_t execution_time = endtim - looptim;
			if (execution_time < 2)
				Sleep((2 - (DWORD)execution_time) * 1000);

			while (!mMutex.try_lock())
				Sleep(MUTEX_WAIT);
			if (*CallingSessionThreadRunning == false)
			{
				mMutex.unlock();
				break;
			}
			mMutex.unlock();
		}

		if (WOLsocket != INVALID_SOCKET)
		{
			closesocket(WOLsocket);
			WOLsocket = INVALID_SOCKET;
		}
	}
	catch (std::exception const& e)
	{
		logmsg = device;
		logmsg += ", ERROR! WOLthread: ";
		logmsg += e.what();
		Log(logmsg);
	}
	logmsg = device;
	logmsg += ", repeating WOL broadcast ended";
	Log(logmsg);

	if (WOLsocket != INVALID_SOCKET)
		closesocket(WOLsocket);
	return;
}
//   THREAD: Spawned when the device should power OFF.
void DisplayPowerOffThread(SESSIONPARAMETERS* CallingSessionParameters, bool* CallingSessionThreadRunning, bool UserForced, bool BlankScreen)
{
	time_t origtim = time(0);

	if (!CallingSessionParameters)
	{
		return;
	}
	if (CallingSessionParameters->SessionKey != "") 
	{
		string hostip = CallingSessionParameters->IP;
		string key = CallingSessionParameters->SessionKey;
		string device = CallingSessionParameters->DeviceId;
		int hdmi_number = CallingSessionParameters->OnlyTurnOffIfCurrentHDMIInputNumberIs;
		bool SSL = CallingSessionParameters->SSL;
		string logmsg;
		string getpowerstatemess = R"({"id": "1", "type": "request", "uri": "ssap://com.webos.service.tvpower/power/getPowerState"})";
		string getactiveinputmess = R"({"id": "2", "type": "request", "uri": "ssap://com.webos.applicationManager/getForegroundAppInfo"})";
		string poweroffmess = R"({"id": "3", "type" : "request", "uri" : "ssap://system/turnOff" })";
		string screenoffmess = R"({"id": "4", "type" : "request", "uri" : "ssap://com.webos.service.tvpower/power/turnOffScreen"})";
		string handshake = narrow(HANDSHAKE_PAIRED);
		string ck = "CLIENTKEYx0x0x0";
		json j;
		boost::string_view type;
		boost::string_view state;
		boost::string_view appId;

		try
		{
			//BOOST::BEAST
			beast::flat_buffer buffer;
			net::io_context ioc;
			tcp::resolver resolver{ ioc };

			//context holds certificates
			ssl::context ctx{ ssl::context::tlsv12_client };
			//load_root_certificates(ctx); 

			websocket::stream<beast::ssl_stream<tcp::socket>> wss{ ioc, ctx };
			websocket::stream<tcp::socket> ws{ ioc };

			string host = hostip;

			auto const results = resolver.resolve(host, SSL ? SERVICE_PORT_SSL : SERVICE_PORT);
			auto ep = net::connect(SSL ? get_lowest_layer(wss) : ws.next_layer(), results);

			//SSL set SNI Hostname
			if (SSL)
				SSL_set_tlsext_host_name(wss.next_layer().native_handle(), host.c_str());

			//build the host string for the decorator
			host += ':' + std::to_string(ep.port());

			//SSL handshake
			if (SSL)
				wss.next_layer().handshake(ssl::stream_base::client);


			if (time(0) - origtim > 10) // this thread should not run too long
			{
				logmsg = device;
				logmsg += ", WARNING! DisplayPowerOffThread() - forced exit";
				Log(logmsg);
				goto threadoffend;
			}
			// Set a decorator to change the User-Agent of the handshake
			if (SSL)
			{
				wss.set_option(websocket::stream_base::decorator(
					[](websocket::request_type& req)
					{
						req.set(http::field::user_agent,
							std::string(BOOST_BEAST_VERSION_STRING) +
							" websocket-client-LGTVsvc");
					}));
				wss.handshake(host, "/");
			}
			else
			{
				ws.set_option(websocket::stream_base::decorator(
					[](websocket::request_type& req)
					{
						req.set(http::field::user_agent,
							std::string(BOOST_BEAST_VERSION_STRING) +
							" websocket-client-LGTVsvc");
					}));
				ws.handshake(host, "/");
			}

			//send a handshake and check validity of the handshake pairing key.
			size_t ckf = handshake.find(ck);
			handshake.replace(ckf, ck.length(), key);
			if (SSL)
			{
				wss.write(net::buffer(std::string(handshake)));
				wss.read(buffer); // read the response
			}
			else
			{
				ws.write(net::buffer(std::string(handshake)));
				ws.read(buffer); // read the response
			}

			//debuggy
			{
				string tt = beast::buffers_to_string(buffer.data());
				string st = SSL ? "[DEBUG] (SSL) OFF response 1: " : "[DEBUG] OFF response 1: ";
				st += tt;
				Log(st);
			}

			j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
			type = j["type"];
			buffer.consume(buffer.size());
			if (std::string(type) != "registered")
			{
				logmsg = device;
				logmsg += ", WARNING! DisplayPowerOffThread() - Pairing key is invalid.";
				Log(logmsg);
				if(SSL)
					wss.close(websocket::close_code::normal);
				else
					ws.close(websocket::close_code::normal);
				goto threadoffend;
			}


			//retreive power state from device to determine if the device is powered on
			if (SSL)
			{
				wss.write(net::buffer(getpowerstatemess));
				wss.read(buffer);
			}
			else
			{
				ws.write(net::buffer(getpowerstatemess));
				ws.read(buffer);
			}

			//debuggy
			{
				string tt = beast::buffers_to_string(buffer.data());
				string st = SSL ? "[DEBUG] (SSL) OFF response 2: " : "[DEBUG] OFF response 2: ";
				st += tt;
				Log(st);
			}

			j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
			buffer.consume(buffer.size());
			
			type = j["type"]; 
			state = j["payload"]["state"];
			if (std::string(type) == "response")
			{
				if (std::string(state) != "Active")
				{
					logmsg = device;
					logmsg += ", power state is: ";
					logmsg += std::string(state);
					Log(logmsg);
					if(SSL)
						wss.close(websocket::close_code::normal);
					else
						ws.close(websocket::close_code::normal);
					goto threadoffend;
				}
			}
			else 
			{
				logmsg = device;
				logmsg += ", WARNING! DisplayPowerOffThread() - Invalid response from device - PowerState.";
				Log(logmsg);
				if(SSL)
					wss.close(websocket::close_code::normal);
				else
					ws.close(websocket::close_code::normal);
				goto threadoffend;
			}

			bool shouldPowerOff = true;

			// Only process power off events when device is set to a certain HDMI input
			if (!UserForced && CallingSessionParameters->HDMIinputcontrol) 
			{
				string app;
				shouldPowerOff = false;

				// get information about active input
				if (SSL)
				{
					wss.write(net::buffer(std::string(getactiveinputmess)));
					wss.read(buffer);
				}
				else
				{
					ws.write(net::buffer(std::string(getactiveinputmess)));
					ws.read(buffer);
				}

				//debuggy
				{
					string tt = beast::buffers_to_string(buffer.data());
					string st = SSL ? "[DEBUG] (SSL) OFF response 3: " : "[DEBUG] OFF response 3: ";
					st += tt;
					Log(st);
				}
				j = json::parse(static_cast<const char*>(buffer.data().data()), static_cast<const char*>(buffer.data().data()) + buffer.size());
				
				appId = j["payload"]["appId"];
				app = std::string("Current AppId: ") + std::string(appId);
				const boost::string_view prefix = "com.webos.app.hdmi";
				if (appId.starts_with(prefix))
				{
					appId.remove_prefix(prefix.size());
					if (std::to_string(hdmi_number) == appId) {
						logmsg = device;
						logmsg += ", HDMI";
						logmsg += std::string(appId);
						logmsg += " input active.";
						Log(logmsg);
						shouldPowerOff = true;
					}
					else
					{
						logmsg = device;
						logmsg += ", HDMI";
						logmsg += std::to_string(hdmi_number);
						logmsg += " input inactive.";
						Log(logmsg);
					}
				}
				else
				{
					logmsg = device;
					logmsg += ", ";
					logmsg += app;
					logmsg += " active.";
					logmsg += app;
					Log(logmsg);
				}

				buffer.consume(buffer.size());
			}

			if (shouldPowerOff)
			{
				// send device or screen off command to device
				if (SSL)
				{
					wss.write(net::buffer(std::string(BlankScreen ? screenoffmess : poweroffmess)));
					wss.read(buffer); // read the response
				}
				else
				{
					ws.write(net::buffer(std::string(BlankScreen ? screenoffmess : poweroffmess)));
					ws.read(buffer); // read the response
				}

				//debuggy
				{
					string tt = beast::buffers_to_string(buffer.data());
					string st = SSL ? "[DEBUG] (SSL) OFF response 4: " : "[DEBUG] OFF response 4: ";
					st += tt;
					Log(st);
				}

				logmsg = device;
				if (BlankScreen)
					logmsg += ", power state is: ON (blanked screen). ";
				else
					logmsg += ", power state is: OFF.";
				Log(logmsg);
				buffer.consume(buffer.size());
			}
			else
			{
				logmsg = device;
				logmsg += ", power state is: Unchanged.";
				Log(logmsg);
			}

			if(SSL)
				wss.close(websocket::close_code::normal);
			else
				ws.close(websocket::close_code::normal);
		}
		catch (std::exception const& e)
		{
			logmsg = device;
			logmsg += ", WARNING! DisplayPowerOffThread(): ";
			logmsg += e.what();
			Log(logmsg);
		}
	}
	else
	{
		string logmsg = CallingSessionParameters->DeviceId;
		logmsg += ", WARNING! DisplayPowerOffThread() - no pairing key";
		Log(logmsg);
	}

threadoffend:

	//thread safe section
	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);
	*CallingSessionThreadRunning = false;
	mMutex.unlock();
	return;
}

//   THREAD: Spawned when the device should select an HDMI input.
void SetDisplayHdmiInputThread(SESSIONPARAMETERS* CallingSessionParameters, bool* CallingSessionThreadRunning, int Timeout, int HdmiInput)
{
	string setinputmess = R"({"id": "3", "type" : "request", "uri" : "ssap://system.launcher/launch", "payload" :{"id":"com.webos.app.hdmiHDMIINPUT"}})";

	if (!CallingSessionParameters)
		return;
	if (HdmiInput < 1 || HdmiInput>4)
		return;
	if (CallingSessionParameters->SessionKey == "")
		return;

	string hostip = CallingSessionParameters->IP;
	string key = CallingSessionParameters->SessionKey;
	string device = CallingSessionParameters->DeviceId;
	bool SSL = CallingSessionParameters->SSL;
	string logmsg;
	time_t origtim = time(0);
	string hdmino = "HDMIINPUT";
	string ck = "CLIENTKEYx0x0x0";
	string handshake;
	size_t ckf = NULL;

	//build handshake
	handshake = narrow(HANDSHAKE_PAIRED);
	ckf = handshake.find(ck);
	handshake.replace(ckf, ck.length(), key);

	// build the message to set HDMI input
	string inp = std::to_string(HdmiInput);
	ckf = setinputmess.find(hdmino);
	setinputmess.replace(ckf, hdmino.length(), inp);

	//try ten times, but not longer than timeout user preference
	while (time(0) - origtim < (Timeout + 1))
	{
		time_t looptim = time(0);
		try
		{

			//BOOST::BEAST
			beast::flat_buffer buffer;
			string host = hostip;
			net::io_context ioc;
			tcp::resolver resolver{ ioc };

			//context holds certificates
			ssl::context ctx{ ssl::context::tlsv12_client };
			//load_root_certificates(ctx); 

			websocket::stream<beast::ssl_stream<tcp::socket>> wss{ ioc, ctx };
			websocket::stream<tcp::socket> ws{ ioc };


			// try communicating with the display
			auto const results = resolver.resolve(host, SSL?SERVICE_PORT_SSL:SERVICE_PORT);
			auto ep = net::connect(SSL ? get_lowest_layer(wss) : ws.next_layer(), results);

			//SSL set SNI Hostname
			if (SSL)
				SSL_set_tlsext_host_name(wss.next_layer().native_handle(), host.c_str());

			//build the host string for the decorator
			host += ':' + std::to_string(ep.port());

			//SSL handshake
			if (SSL)
				wss.next_layer().handshake(ssl::stream_base::client);

			if (SSL)
			{
				wss.set_option(websocket::stream_base::decorator(
					[](websocket::request_type& req)
					{
						req.set(http::field::user_agent,
							std::string(BOOST_BEAST_VERSION_STRING) +
							" websocket-client-LGTVsvc");
					}));
				wss.handshake(host, "/");
				wss.write(net::buffer(std::string(handshake)));
				wss.read(buffer); // read the first response
				wss.write(net::buffer(std::string(setinputmess)));
				wss.read(buffer);
				wss.close(websocket::close_code::normal);
			}
			else
			{
				ws.set_option(websocket::stream_base::decorator(
					[](websocket::request_type& req)
					{
						req.set(http::field::user_agent,
							std::string(BOOST_BEAST_VERSION_STRING) +
							" websocket-client-LGTVsvc");
					}));
				ws.handshake(host, "/");
				ws.write(net::buffer(std::string(handshake)));
				ws.read(buffer); // read the first response
				ws.write(net::buffer(std::string(setinputmess)));
				ws.read(buffer);
				ws.close(websocket::close_code::normal);
			}
			logmsg = device;
			logmsg += ", Setting HDMI input: ";
			logmsg += std::to_string(HdmiInput);
			Log(logmsg);
			break;
		}
		catch (std::exception const& e)
		{
			logmsg = device;
			logmsg += ", WARNING! SetDisplayHdmiInputThread(): ";
			logmsg += e.what();
			Log(logmsg);
		}
		time_t endtim = time(0);
		time_t execution_time = endtim - looptim;
		if (execution_time < 6)
			Sleep((6 - (DWORD)execution_time) * 1000);
	}

	while (!mMutex.try_lock())
		Sleep(MUTEX_WAIT);
	*CallingSessionThreadRunning = false;
	mMutex.unlock();
	return;
}