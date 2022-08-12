// See LGTV Companion UI.cpp for additional details
#include "Service.h"

using namespace std;
namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
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

			std::stringstream log;
			log << "DeleteIpNetEntry2() = " << DeleteIpNetEntry2(&row);
			Log(log.str());
		}

	private:
		const NET_LUID luid;
		const SOCKADDR_INET address;
	};

	boost::optional<NET_LUID> GetLocalInterface(SOCKADDR_INET destination) {
		MIB_IPFORWARD_ROW2 row;
		SOCKADDR_INET bestSourceAddress;
		const auto result = GetBestRoute2(NULL, 0, NULL, &destination, 0, &row, &bestSourceAddress);

		std::stringstream log;
		log << "GetBestRoute2()";

		if (result != NO_ERROR) {
			std::stringstream log;
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

	std::unique_ptr<NetEntryDeleter> CreateTransientLocalNetEntry(SOCKADDR_INET destination, unsigned char macAddress[6]) {
		const auto luid = GetLocalInterface(destination);
		if (!luid.has_value()) return nullptr;

		MIB_IPNET_ROW2 row;
		row.Address = destination;
		row.InterfaceLuid = *luid;
		memcpy(row.PhysicalAddress, macAddress, sizeof(macAddress));
		row.PhysicalAddressLength = sizeof(macAddress);
		const auto result = CreateIpNetEntry2(&row);

		std::stringstream log;
		log << "CreateIpNetEntry2() = " << result;
		Log(log.str());

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
			s += " (no session key).";
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
	}break;
	case SYSTEM_EVENT_UNSURE:
	case SYSTEM_EVENT_SHUTDOWN:
	{
		TurnOffDisplay(false, false, false);
	}break;
	case SYSTEM_EVENT_SUSPEND:
	{
	}break;
	case SYSTEM_EVENT_RESUME:
	{
	}break;
	case SYSTEM_EVENT_RESUMEAUTO:
	{
		if (Parameters.SetHDMIInputOnResume)
			SetDisplayHdmiInput(Parameters.SetHDMIInputOnResumeToNumber);
	}break;

	case SYSTEM_EVENT_DISPLAYON:
	{
		TurnOnDisplay(true);
	}break;
	case SYSTEM_EVENT_DISPLAYOFF:
	{
		TurnOffDisplay(false, false, false);
	}break;
	case SYSTEM_EVENT_DISPLAYDIMMED:
	{
		TurnOffDisplay(false, true, false);
	}break;
	case SYSTEM_EVENT_USERIDLE:
	{
		if (Parameters.BlankWhenIdle)
			TurnOffDisplay(false, false, true);
	}break;
	case SYSTEM_EVENT_USERBUSY:
	{
		if (Parameters.BlankWhenIdle)
			TurnOnDisplay(true);
	}break;
	case SYSTEM_EVENT_BOOT:
	{
		if (Parameters.SetHDMIInputOnResume)
			SetDisplayHdmiInput(Parameters.SetHDMIInputOnResumeToNumber);
	}break;
	case SYSTEM_EVENT_UNBLANK:
	{
		TurnOnDisplay(false);
	}break;
	default:break;
	}
}
//   THREAD: Spawned when the device should power ON. This thread manages the pairing key from the display and verifies that the display has been powered on
void DisplayPowerOnThread(SESSIONPARAMETERS* CallingSessionParameters, bool* CallingSessionThreadRunning, int Timeout, bool SendWOL)
{
	string screenonmess = "{ \"id\":\"2\",\"type\" : \"request\",\"uri\" : \"ssap://com.webos.service.tvpower/power/turnOnScreen\"}";

	if (!CallingSessionParameters)
		return;

	string hostip = CallingSessionParameters->IP;
	string key = CallingSessionParameters->SessionKey;
	string device = CallingSessionParameters->DeviceId;
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
			net::io_context ioc;
			tcp::resolver resolver{ ioc };
			websocket::stream<tcp::socket> ws{ ioc };
			beast::flat_buffer buffer;
			string host = hostip;

			// try communicating with the display
			auto const results = resolver.resolve(host, SERVICE_PORT);
			auto ep = net::connect(ws.next_layer(), results);
			host += ':' + std::to_string(ep.port());

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

			// parse for pairing key if needed
			if (key == "")
			{
				ws.read(buffer); //read the second response which hopefully now contains the session key
				string t = beast::buffers_to_string(buffer.data());
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
			}

			ws.write(net::buffer(std::string(screenonmess)));
			//          ws.read(buffer);

			ws.close(websocket::close_code::normal);
			logmsg = device;
			logmsg += ", established contact: Display is ON.  ";
			Log(logmsg);
			break;
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
						if (WOLtype == WOL_IPSEND) netEntryDeleter = CreateTransientLocalNetEntry(reinterpret_cast<const SOCKADDR_INET&>(LANDestination), MACstr);

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

	//   Log("DEBUG INFO: DisplayPowerOffThread() running...");
	if (!CallingSessionParameters)
	{
		//        Log("DEBUG INFO: DisplayPowerOffThread() :: CallingSessionParameters is zero");
		return;
	}
	if (CallingSessionParameters->SessionKey != "") //assume we have paired here. Doe not make sense to try pairing when display shall turn off.
	{
		string host = CallingSessionParameters->IP;
		string key = CallingSessionParameters->SessionKey;
		string device = CallingSessionParameters->DeviceId;
		string logmsg;
		string poweroffmess = "{ \"id\":\"2\",\"type\" : \"request\",\"uri\" : \"ssap://system/turnOff\"}";
		string screenoffmess = "{ \"id\":\"2\",\"type\" : \"request\",\"uri\" : \"ssap://com.webos.service.tvpower/power/turnOffScreen\"}";

		string handshake = narrow(HANDSHAKE_PAIRED);
		string ck = "CLIENTKEYx0x0x0";

		try
		{
			//            Log("DEBUG INFO: DisplayPowerOffThread() creating websocket...");

			net::io_context ioc;

			size_t ckf = handshake.find(ck);
			handshake.replace(ckf, ck.length(), key);
			tcp::resolver resolver{ ioc };
			websocket::stream<tcp::socket> ws{ ioc };
			//            Log("DEBUG INFO: DisplayPowerOffThread() resolving...");

			auto const results = resolver.resolve(host, SERVICE_PORT);
			//           Log("DEBUG INFO: DisplayPowerOffThread() connecting...");

			auto ep = net::connect(ws.next_layer(), results);

			host += ':' + std::to_string(ep.port());
			if (time(0) - origtim > 10) // this thread should not run too long
			{
				Log("DisplayPowerOffThread() - forced exit");
				goto threadoffend;
			}
			//            Log("DEBUG INFO: DisplayPowerOffThread() setting options...");

						// Set a decorator to change the User-Agent of the handshake
			ws.set_option(websocket::stream_base::decorator(
				[](websocket::request_type& req)
				{
					req.set(http::field::user_agent,
						std::string(BOOST_BEAST_VERSION_STRING) +
						" websocket-client-LGTVsvc");
				}));
			if (time(0) - origtim > 10) // this thread should not run too long
			{
				Log("DisplayPowerOffThread() - forced exit");
				goto threadoffend;
			}

			//           Log("DEBUG INFO: DisplayPowerOffThread() handshake...");

			ws.handshake(host, "/");

			if (time(0) - origtim > 10) // this thread should not run too long
			{
				Log("WARNING! DisplayPowerOffThread() - forced exit");
				goto threadoffend;
			}

			//            Log("DEBUG INFO: DisplayPowerOffThread() sending...");

			beast::flat_buffer buffer;

			ws.write(net::buffer(std::string(handshake)));
			ws.read(buffer); // read the response

			bool shouldPowerOff = true;
			if (!UserForced && CallingSessionParameters->HDMIinputcontrol)
			{
				string app;
				shouldPowerOff = false;
				ws.write(net::buffer(std::string(R"({"id": "1", "type": "request", "uri": "ssap://com.webos.applicationManager/getForegroundAppInfo"})")));
				beast::flat_buffer response;
				ws.read(response);
				const json j = json::parse(static_cast<const char*>(response.data().data()), static_cast<const char*>(response.data().data()) + response.size());
				boost::string_view appId = j["payload"]["appId"];
				app = std::string("Current AppId: ") + std::string(appId);
				const boost::string_view prefix = "com.webos.app.hdmi";
				if (appId.starts_with(prefix))
				{
					appId.remove_prefix(prefix.size());
					if (std::to_string(CallingSessionParameters->OnlyTurnOffIfCurrentHDMIInputNumberIs) == appId) {
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
						logmsg += std::to_string(CallingSessionParameters->OnlyTurnOffIfCurrentHDMIInputNumberIs);
						logmsg += " input inactive.";
						Log(logmsg);
					}
				}
			}

			if (shouldPowerOff)
			{
				ws.write(net::buffer(std::string(BlankScreen ? screenoffmess : poweroffmess)));
				ws.read(buffer); // read the response
				logmsg = device;
				if (BlankScreen)
					logmsg += ", established contact: Screen is OFF. ";
				else
					logmsg += ", established contact: Device is OFF.";
				Log(logmsg);
			}

			//            Log("DEBUG INFO: DisplayPowerOffThread() closing...");
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
		Log("WARNING! DisplayPowerOffThread() - no pairing key");

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
	string setinputmess = "{ \"id\":\"3\",\"type\" : \"request\",\"uri\" : \"ssap://system.launcher/launch\", \"payload\" :{\"id\":\"com.webos.app.hdmiHDMIINPUT\"}}";

	if (!CallingSessionParameters)
		return;
	if (HdmiInput < 1 || HdmiInput>4)
		return;
	if (CallingSessionParameters->SessionKey == "")
		return;

	string hostip = CallingSessionParameters->IP;
	string key = CallingSessionParameters->SessionKey;
	string device = CallingSessionParameters->DeviceId;
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
			net::io_context ioc;
			tcp::resolver resolver{ ioc };
			websocket::stream<tcp::socket> ws{ ioc };
			beast::flat_buffer buffer;
			string host = hostip;

			// try communicating with the display
			auto const results = resolver.resolve(host, SERVICE_PORT);
			auto ep = net::connect(ws.next_layer(), results);
			host += ':' + std::to_string(ep.port());

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
			logmsg = device;
			logmsg += ", established contact. Set HDMI input: ";
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