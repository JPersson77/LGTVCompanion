#include "../Common/Common.h"

using namespace jpersson77;

//   Convert UTF-8 to wide
std::wstring common::widen(std::string sInput) {
	if (sInput == "")
		return L"";
	// Calculate target buffer size
	long len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(), NULL, 0);
	if (len == 0)
		return L"";
	// Convert character sequence
	std::wstring out(len, 0);
	if (len != MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(), &out[0], (int)out.size()))
		return L"";
	return out;
}
//   Convert wide to UTF-8
std::string common::narrow(std::wstring sInput) {
	// Calculate target buffer size
	long len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(),
		NULL, 0, NULL, NULL);
	if (len == 0)
		return "";
	// Convert character sequence
	std::string out(len, 0);
	if (len != WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(), &out[0], (int)out.size(), NULL, NULL))
		return "";
	return out;
}

//   Split a string into a vector of strings, accepts quotation marks
std::vector<std::string> common::stringsplit(std::string str, std::string token) {
	std::vector<std::string>res;
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
				size_t next = str.find_first_not_of(token, index + token.size());
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
			index = str.find(token);
			if (index != std::string::npos)
			{
				res.push_back(str.substr(0, index));

				size_t next = str.find_first_not_of(token, index + token.size());
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
//   Get string from control
std::wstring common::GetWndText(HWND hWnd)
{
	int len = GetWindowTextLength(hWnd) + 1;
	std::vector<wchar_t> buf(len);
	GetWindowText(hWnd, &buf[0], len);
	std::wstring text = &buf[0];
	return text;
}
settings::Preferences::Preferences() {
}
settings::Preferences::~Preferences() {
}
bool settings::Preferences::Initialize() {
	TCHAR szPath[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath)))
	{
		std::wstring path = szPath;
		path += L"\\";
		path += APPNAME;
		path += L"\\";
		CreateDirectory(path.c_str(), NULL);
		DataPath = path;
		path += CONFIG_FILE;

		std::ifstream i(path.c_str());
		if (i.is_open())
		{
			nlohmann::json j;
			nlohmann::json jsonPrefs;
			i >> jsonPrefs;
			i.close();
			// The restart strings
			j = jsonPrefs[JSON_PREFS_NODE][JSON_EVENT_RESTART_STRINGS];
			if (!j.empty() && j.size() > 0)
			{
				for (auto& str : j.items())
				{
					std::string temp = str.value().get<std::string>();
					if (std::find(EventLogRestartString.begin(), EventLogRestartString.end(), temp) == EventLogRestartString.end())
						EventLogRestartString.push_back(temp);
				}
			}
			// The shutdown strings
			j = jsonPrefs[JSON_PREFS_NODE][JSON_EVENT_SHUTDOWN_STRINGS];
			if (!j.empty() && j.size() > 0)
			{
				//               Prefs.EventLogShutdownString.clear();
				for (auto& str : j.items())
				{
					std::string temp = str.value().get<std::string>();
					if (std::find(EventLogShutdownString.begin(), EventLogShutdownString.end(), temp) == EventLogShutdownString.end())
						EventLogShutdownString.push_back(temp);
				}
			}
			// Version (of the preferences file)
			j = jsonPrefs[JSON_PREFS_NODE][JSON_VERSION];
			if (!j.empty() && j.is_number())
				version = j.get<int>();
			// Power On timeout
			j = jsonPrefs[JSON_PREFS_NODE][JSON_PWRONTIMEOUT];
			if (!j.empty() && j.is_number())
				PowerOnTimeout = j.get<int>();
			if (PowerOnTimeout < 1)
				PowerOnTimeout = 1;
			else if (PowerOnTimeout > 100)
				PowerOnTimeout = 100;
			// Logging
			j = jsonPrefs[JSON_PREFS_NODE][JSON_LOGGING];
			if (!j.empty() && j.is_boolean())
				Logging = j.get<bool>();
			// Update notifications
			j = jsonPrefs[JSON_PREFS_NODE][JSON_AUTOUPDATE];
			if (!j.empty() && j.is_boolean())
				AutoUpdate = j.get<bool>();
			// User idle mode
			j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEBLANK];
			if (!j.empty() && j.is_boolean())
				BlankScreenWhenIdle = j.get<bool>();
			// User idle mode delay
			j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEBLANKDELAY];
			if (!j.empty() && j.is_number())
				BlankScreenWhenIdleDelay = j.get<int>();
			if (BlankScreenWhenIdleDelay < 1)
				BlankScreenWhenIdleDelay = 1;
			else if (BlankScreenWhenIdleDelay > 240)
				BlankScreenWhenIdleDelay = 240;
			// Multi-monitor topology support
			j = jsonPrefs[JSON_PREFS_NODE][JSON_ADHERETOPOLOGY];
			if (!j.empty() && j.is_boolean())
				AdhereTopology = j.get<bool>();
			// User idle mode whitelist enabled
			j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEWHITELIST];
			if (!j.empty() && j.is_boolean())
				bIdleWhitelistEnabled = j.get<bool>();
			// User idle mode fullscreen exclusions enabled
			j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLE_FS_EXCLUSIONS_ENABLE];
			if (!j.empty() && j.is_boolean())
				bIdleFsExclusionsEnabled = j.get<bool>();
			// User idle mode, prohibit fullscreen
			j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEFULLSCREEN];
			if (!j.empty() && j.is_boolean())
				bFullscreenCheckEnabled = j.get<bool>();
			// Remote streaming host support
			j = jsonPrefs[JSON_PREFS_NODE][JSON_REMOTESTREAM];
			if (!j.empty() && j.is_boolean())
				RemoteStreamingCheck = j.get<bool>();
			// Multi-monitor topology mode
			j = jsonPrefs[JSON_PREFS_NODE][JSON_TOPOLOGYMODE];
			if (!j.empty() && j.is_boolean())
				TopologyPreferPowerEfficiency = j.get<bool>();
			// External API
			j = jsonPrefs[JSON_PREFS_NODE][JSON_EXTERNAL_API];
			if (!j.empty() && j.is_boolean())
				ExternalAPI = j.get<bool>();
			// Mute Speakers
			j = jsonPrefs[JSON_PREFS_NODE][JSON_MUTE_SPEAKERS];
			if (!j.empty() && j.is_boolean())
				MuteSpeakers = j.get<bool>();
			// User idle mode whitelist
			j = jsonPrefs[JSON_PREFS_NODE][JSON_WHITELIST];
			if (!j.empty() && j.size() > 0)
			{
				for (auto& elem : j.items())
				{
					PROCESSLIST w;
					w.Application = common::widen(elem.value().get<std::string>());
					w.Name = common::widen(elem.key());
					WhiteList.push_back(w);
				}
			}
			// User idle mode fullscreen exclusions
			j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLE_FS_EXCLUSIONS];
			if (!j.empty() && j.size() > 0)
			{
				for (auto& elem : j.items())
				{
					PROCESSLIST w;
					w.Application = common::widen(elem.value().get<std::string>());
					w.Name = common::widen(elem.key());
					FsExclusions.push_back(w);
				}
			}

			// initialize the configuration for WebOS devices
			Devices.clear();
			for (const auto& item : jsonPrefs.items())
			{
				if (item.key() == JSON_PREFS_NODE)
					break;
				settings::DEVICE params;
				params.DeviceId = item.key();

				if (item.value()[JSON_DEVICE_NAME].is_string())
					params.Name = item.value()[JSON_DEVICE_NAME].get<std::string>();

				if (item.value()[JSON_DEVICE_IP].is_string())
					params.IP = item.value()[JSON_DEVICE_IP].get<std::string>();

				if (item.value()[JSON_DEVICE_UNIQUEKEY].is_string())
					params.UniqueDeviceKey = item.value()[JSON_DEVICE_UNIQUEKEY].get<std::string>();

				if (item.value()[JSON_DEVICE_HDMICTRL].is_boolean())
					params.HDMIinputcontrol = item.value()[JSON_DEVICE_HDMICTRL].get<bool>();

				if (item.value()[JSON_DEVICE_HDMICTRLNO].is_number())
					params.OnlyTurnOffIfCurrentHDMIInputNumberIs = item.value()[JSON_DEVICE_HDMICTRLNO].get<int>();
				if (params.OnlyTurnOffIfCurrentHDMIInputNumberIs < 1)
					params.OnlyTurnOffIfCurrentHDMIInputNumberIs = 1;
				else if (params.OnlyTurnOffIfCurrentHDMIInputNumberIs > 4)
					params.OnlyTurnOffIfCurrentHDMIInputNumberIs = 4;

				if (item.value()[JSON_DEVICE_ENABLED].is_boolean())
					params.Enabled = item.value()[JSON_DEVICE_ENABLED].get<bool>();

				if (item.value()[JSON_DEVICE_SESSIONKEY].is_string())
					params.SessionKey = item.value()[JSON_DEVICE_SESSIONKEY].get<std::string>();

				if (item.value()[JSON_DEVICE_SUBNET].is_string())
					params.Subnet = item.value()[JSON_DEVICE_SUBNET].get<std::string>();

				if (item.value()[JSON_DEVICE_WOLTYPE].is_number())
					params.WOLtype = item.value()[JSON_DEVICE_WOLTYPE].get<int>();
				if (params.WOLtype < 1)
					params.WOLtype = 1;
				else if (params.WOLtype > 3)
					params.WOLtype = 3;

				if (item.value()[JSON_DEVICE_SETHDMI].is_boolean())
					params.SetHDMIInputOnResume = item.value()[JSON_DEVICE_SETHDMI].get<bool>();

				if (item.value()[JSON_DEVICE_NEWSOCK].is_boolean())
					params.SSL = item.value()[JSON_DEVICE_NEWSOCK].get<bool>();

				if (item.value()[JSON_DEVICE_SETHDMINO].is_number())
					params.SetHDMIInputOnResumeToNumber = item.value()[JSON_DEVICE_SETHDMINO].get<int>();
				if (params.SetHDMIInputOnResumeToNumber < 1)
					params.SetHDMIInputOnResumeToNumber = 1;
				else if (params.SetHDMIInputOnResumeToNumber > 4)
					params.SetHDMIInputOnResumeToNumber = 4;

				j = item.value()[JSON_DEVICE_MAC];
				if (!j.empty() && j.size() > 0)
				{
					for (auto& m : j.items())
					{
						params.MAC.push_back(m.value().get<std::string>());
					}
				}
				params.PowerOnTimeout = PowerOnTimeout;
				params.MuteSpeakers = MuteSpeakers;
				Devices.push_back(params);
			}
			return true;
		}
	}
	return false;
}
void settings::Preferences::WriteToDisk(void)
{
	nlohmann::json prefs, p;
	std::wstring path = DataPath;
	CreateDirectory(DataPath.c_str(), NULL);
	path += CONFIG_FILE;

	// do we need to upgrade the api key version
	if (ResetAPIkeys)
	{
		for (auto& k : Devices)
		{
			k.SessionKey = "";
		}
		ResetAPIkeys = false;
		version = 2;
	}
	else //load sessionkeys from config.json and add it to the device list
	{
		std::ifstream i(path.c_str());
		if (i.is_open())
		{
			i >> p;
			i.close();

			for (const auto& item : p.items())
			{
				if (item.key() == JSON_PREFS_NODE)
					break;
				nlohmann::json j;
				std::string key = "";
				if (item.value()[JSON_DEVICE_SESSIONKEY].is_string())
					key = item.value()[JSON_DEVICE_SESSIONKEY].get<std::string>();
				j = item.value()[JSON_DEVICE_MAC];
				if (!j.empty() && j.size() > 0)
					for (auto& m : j.items())
						for (auto& k : Devices)
							for (auto& l : k.MAC)
								if (l == m.value().get<std::string>())
									k.SessionKey = key;
			}
		}
	}
	prefs[JSON_PREFS_NODE][JSON_VERSION] = (int)version;
	prefs[JSON_PREFS_NODE][JSON_PWRONTIMEOUT] = (int)PowerOnTimeout;
	prefs[JSON_PREFS_NODE][JSON_LOGGING] = (bool)Logging;
	prefs[JSON_PREFS_NODE][JSON_AUTOUPDATE] = (bool)AutoUpdate;
	prefs[JSON_PREFS_NODE][JSON_IDLEBLANK] = (bool)BlankScreenWhenIdle;
	prefs[JSON_PREFS_NODE][JSON_IDLEBLANKDELAY] = (int)BlankScreenWhenIdleDelay;
	prefs[JSON_PREFS_NODE][JSON_ADHERETOPOLOGY] = (bool)AdhereTopology;
	prefs[JSON_PREFS_NODE][JSON_IDLEWHITELIST] = (bool)bIdleWhitelistEnabled;
	prefs[JSON_PREFS_NODE][JSON_IDLEFULLSCREEN] = (bool)bFullscreenCheckEnabled;
	prefs[JSON_PREFS_NODE][JSON_IDLE_FS_EXCLUSIONS_ENABLE] = (bool)bIdleFsExclusionsEnabled;
	prefs[JSON_PREFS_NODE][JSON_REMOTESTREAM] = (bool)RemoteStreamingCheck;
	prefs[JSON_PREFS_NODE][JSON_TOPOLOGYMODE] = (bool)TopologyPreferPowerEfficiency;
	prefs[JSON_PREFS_NODE][JSON_EXTERNAL_API] = (bool)ExternalAPI;
	prefs[JSON_PREFS_NODE][JSON_MUTE_SPEAKERS] = (bool)MuteSpeakers;
	for (auto& item : EventLogRestartString)
		prefs[JSON_PREFS_NODE][JSON_EVENT_RESTART_STRINGS].push_back(item);
	for (auto& item : EventLogShutdownString)
		prefs[JSON_PREFS_NODE][JSON_EVENT_SHUTDOWN_STRINGS].push_back(item);
	if (WhiteList.size() > 0)
		for (auto& w : WhiteList)
			prefs[JSON_PREFS_NODE][JSON_WHITELIST][common::narrow(w.Name)] = common::narrow(w.Application);
	if (FsExclusions.size() > 0)
		for (auto& w : FsExclusions)
			prefs[JSON_PREFS_NODE][JSON_IDLE_FS_EXCLUSIONS][common::narrow(w.Name)] = common::narrow(w.Application);

	//Iterate devices
	int deviceid = 1;
	for (auto& item : Devices)
	{
		std::stringstream dev;
		dev << "Device";
		dev << deviceid;
		item.DeviceId = dev.str();
		//		prefs[dev.str()][JSON_DEVICE_NAME] = item.Name;
		if (item.Name != "")
			prefs[dev.str()][JSON_DEVICE_NAME] = item.Name;
		if (item.IP != "")
			prefs[dev.str()][JSON_DEVICE_IP] = item.IP;
		if (item.SessionKey != "")
			prefs[dev.str()][JSON_DEVICE_SESSIONKEY] = item.SessionKey;
		else
			prefs[dev.str()][JSON_DEVICE_SESSIONKEY] = "";
		if (item.UniqueDeviceKey != "")
			prefs[dev.str()][JSON_DEVICE_UNIQUEKEY] = item.UniqueDeviceKey;

		prefs[dev.str()][JSON_DEVICE_HDMICTRL] = (bool)item.HDMIinputcontrol;
		prefs[dev.str()][JSON_DEVICE_HDMICTRLNO] = item.OnlyTurnOffIfCurrentHDMIInputNumberIs;

		prefs[dev.str()][JSON_DEVICE_SETHDMI] = (bool)item.SetHDMIInputOnResume;
		prefs[dev.str()][JSON_DEVICE_SETHDMINO] = item.SetHDMIInputOnResumeToNumber;

		prefs[dev.str()][JSON_DEVICE_NEWSOCK] = (bool)item.SSL;

		if (item.Subnet != "")
			prefs[dev.str()][JSON_DEVICE_SUBNET] = item.Subnet;

		prefs[dev.str()][JSON_DEVICE_WOLTYPE] = item.WOLtype;

		prefs[dev.str()][JSON_DEVICE_ENABLED] = (bool)item.Enabled;

		for (auto& m : item.MAC)
			prefs[dev.str()][JSON_DEVICE_MAC].push_back(m);

		deviceid++;
	}

	if (!prefs.empty())
	{
		std::ofstream i(path.c_str());
		if (i.is_open())
		{
			i << std::setw(4) << prefs << std::endl;
			i.close();
		}
	}
}
// PIPESERVER Class Constructor. sName = name of Named Pipe, fcnPtr =  pointer to callback function
ipc::PipeServer::PipeServer(std::wstring sName, void (*fcnPtr)(std::wstring))
{
	if (sName == L"" || fcnPtr == NULL)
		return;

	Log(L" ");
	Log(L"-----------------------------");
	Log(L"PipeServer Started!");

	for (int i = 0; i < PIPE_INSTANCES; i++)
	{
		Buffer[i][PIPE_BUF_SIZE] = '\0';
		Buffer[i][0] = '\0';
	}

	sPipeName = sName;
	FunctionPtr = fcnPtr;

	//Security descriptor
	PSECURITY_DESCRIPTOR psd = NULL;
	BYTE  sd[SECURITY_DESCRIPTOR_MIN_LENGTH];
	psd = (PSECURITY_DESCRIPTOR)sd;
	InitializeSecurityDescriptor(psd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(psd, TRUE, (PACL)NULL, FALSE);
	SECURITY_ATTRIBUTES sa = { sizeof(sa), psd, FALSE };

	for (int i = 0; i < PIPE_INSTANCES; i++)
	{
		// Create a named pipe instance
		if ((hPipeHandles[i] = CreateNamedPipe(sPipeName.c_str(),
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, PIPE_INSTANCES,
			PIPE_BUF_SIZE, PIPE_BUF_SIZE, 1000, &sa)) == INVALID_HANDLE_VALUE)
		{
			for (int j = 0; j < i; j++)
			{
				CloseHandle(hPipeHandles[j]);
				hPipeHandles[j] = NULL;
			}
			OnEvent(PIPE_EVENT_ERROR, L"Failed to CreateNamedPipe()", i);
			return;
		}
		// Create an event handle for each pipe instance. This
		// will be used to monitor overlapped I/O activity on each pipe.
		if ((hEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
		{
			for (int j = 0; j < PIPE_INSTANCES; j++)
			{
				CloseHandle(hPipeHandles[j]);
				hPipeHandles[j] = NULL;
			}
			for (int j = 0; j < i; j++)
			{
				CloseHandle(hEvents[j]);
				hEvents[j] = NULL;
			}
			OnEvent(PIPE_EVENT_ERROR, L"Failed to CreateEvent()", i);
			return;
		}

		ZeroMemory(&Ovlap[i], sizeof(OVERLAPPED));
		Ovlap[i].hEvent = hEvents[i];

		// Listen for client connections using ConnectNamedPipe()
		bool bConnected = ConnectNamedPipe(hPipeHandles[i], &Ovlap[i]) == 0 ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
		if (!bConnected)
		{
			if (GetLastError() != ERROR_IO_PENDING)
			{
				for (int j = 0; j < PIPE_INSTANCES; j++)
				{
					CloseHandle(hPipeHandles[j]);
					hPipeHandles[j] = NULL;
					CloseHandle(hEvents[j]);
					hEvents[j] = NULL;
				}
				OnEvent(PIPE_EVENT_ERROR, L"Failed to ConnectNamedPipe() when initialising", i);
				return;
			}
		}
	}
	//termination event
	if((hEvents[PIPE_INSTANCES] = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
	{
		for (int j = 0; j < PIPE_INSTANCES; j++)
		{
			CloseHandle(hPipeHandles[j]);
			hPipeHandles[j] = NULL;
			CloseHandle(hEvents[j]);
			hEvents[j] = NULL;
		}
		OnEvent(PIPE_EVENT_ERROR, L"Failed to Ccreate termination event");
	}
	//termination confirm event
	if ((hEvents[PIPE_INSTANCES+1] = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
	{
		for (int j = 0; j < PIPE_INSTANCES; j++)
		{
			CloseHandle(hPipeHandles[j]);
			hPipeHandles[j] = NULL;
			CloseHandle(hEvents[j]);
			hEvents[j] = NULL;
		}
		CloseHandle(hPipeHandles[PIPE_INSTANCES]); hPipeHandles[PIPE_INSTANCES] = NULL;
		OnEvent(PIPE_EVENT_ERROR, L"Failed to Create termination confirm event");
	}
	std::thread thread_obj(&PipeServer::WorkerThread, this);
	thread_obj.detach();

}
// PIPESERVER Class Destructor. 
ipc::PipeServer::~PipeServer(void)
{
	Terminate();
}
// Terminate the PIPESERVER before it is deleted
bool ipc::PipeServer::Terminate()
{
	if (!isRunning())
		return false;

	// Signal worker thread to exit
	SetEvent(hEvents[PIPE_INSTANCES]); 
	//Create a new event and wait for the signal when worker thread is actually exiting
	WaitForSingleObject(hEvents[PIPE_INSTANCES+1], 2000);
	//Close all handles
	for (int i = 0; i < PIPE_INSTANCES; i++)
	{
		if (hPipeHandles[i])
		{
			CloseHandle(hPipeHandles[i]);
			hPipeHandles[i] = NULL;
		}
		if (hEvents[i])
		{
			CloseHandle(hEvents[i]);
			hEvents[i] = NULL;
		}
	}
	Log(L"PipeServer terminated!");
	return true;
}
// Is the server running
bool ipc::PipeServer::isRunning()
{
	return iState == PIPE_RUNNING ? TRUE : FALSE;
}
//Send message to named pipe(s). sData = message, iPipe(optional) = pipe instance or -1 to send to all instances
bool ipc::PipeServer::Send(std::wstring sData, int iPipe)
{
	return Write(sData, iPipe);
}

//The worker thread managing the overlapped pipes
void ipc::PipeServer::WorkerThread()
{
	DWORD dwRet;
	DWORD dwPipe;
	Log(L"Worker Thread started!");
	OnEvent(PIPE_RUNNING);
	while (1)
	{
		if ((dwRet = WaitForMultipleObjects(PIPE_INSTANCES+1, hEvents, FALSE, INFINITE)) == WAIT_FAILED)
		{
			OnEvent(PIPE_EVENT_ERROR, L"WaitForMultipleObjects() returned WAIT_FAILED.");
			goto WORKER_THREAD_END;
		}

		dwPipe = dwRet - WAIT_OBJECT_0;
		ResetEvent(hEvents[dwPipe]);

		// Check if the termination event has been signalled
		if (dwPipe == PIPE_INSTANCES) 
		{
			goto WORKER_THREAD_END;
		}

		// Check overlapped results, and if they fail, reestablish
		// communication for a new client; otherwise, process read and write operations with the client
		if (GetOverlappedResult(hPipeHandles[dwPipe], &Ovlap[dwPipe], &dwBytesTransferred, TRUE) == 0)
		{
			if (!DisconnectAndReconnect(dwPipe))
			{
				OnEvent(PIPE_EVENT_ERROR, L"GetOverlappedResult failed. DisconnecAndReconnect() has Failed.", dwPipe);
//				goto WORKER_THREAD_END;
			}
			else
				OnEvent(PIPE_EVENT_ERROR, L"GetOverlappedResult failed. DisconnectAndReconnect() success.", dwPipe);
		}
		else
		{
			// Check the state of the pipe. If bWriteData equals
			// FALSE, post a read on the pipe for incoming data.			
			if (!bWriteData[dwPipe]) // PERFORM READ
			{
				std::wstringstream ss;
				ss << L"READ! dwBytesTransferred = " << dwBytesTransferred;

				Log(ss.str());

				ZeroMemory(&Ovlap[dwPipe], sizeof(OVERLAPPED));
				Ovlap[dwPipe].hEvent = hEvents[dwPipe];
				if (ReadFile(hPipeHandles[dwPipe], Buffer[dwPipe], PIPE_BUF_SIZE, NULL, &Ovlap[dwPipe]) == 0)
				{
					if (GetLastError() != ERROR_IO_PENDING)
					{
						if (!DisconnectAndReconnect(dwPipe))
						{
							OnEvent(PIPE_EVENT_ERROR, L"ReadFile() failed. DisconnectAndReconnect() failed.", dwPipe);
//							goto WORKER_THREAD_END;
						}
						else
							OnEvent(PIPE_EVENT_ERROR, L"ReadFile() failed. DisconnectAndReconnect() success.", dwPipe);
					}
					else
					{
						if (dwBytesTransferred > 0)
						{
							std::wstring sss = L"Read contents: ";
							sss += Buffer[dwPipe];
							Log(sss);
							OnEvent(PIPE_EVENT_READ, Buffer[dwPipe], dwPipe);
						}
					}
				}
			}
			else
			{
				std::wstringstream ss;
				ss << L"WRITE! dwBytesTransferred: " << dwBytesTransferred;
				Log(ss.str());

				OnEvent(PIPE_EVENT_SEND, L"", dwPipe);
			}
		}
	}
WORKER_THREAD_END:
	Log(L"Worker Thread is exiting!");
	OnEvent(PIPE_EVENT_TERMINATE);
	SetEvent(hEvents[PIPE_INSTANCES + 1]);
}

void ipc::PipeServer::OnEvent(int nEventID, std::wstring sData, int iPipe)
{
	std::wstring msg;
	switch (nEventID)
	{
	case PIPE_RUNNING:
		iState = PIPE_RUNNING;
		break;
	case PIPE_EVENT_TERMINATE:
		iState = PIPE_NOT_RUNNING;
		break;
	case PIPE_EVENT_SEND:
		bWriteData[iPipe] = false;
		Buffer[iPipe][0] = '/0';
		break;
	case PIPE_EVENT_READ:
		FunctionPtr(sData); // Send message to external function/callback
		Buffer[iPipe][0] = '/0';
		break;
	case PIPE_EVENT_ERROR:
		msg = L"[ERROR] ";
		msg += sData;
		Log(msg);
//		FunctionPtr(msg); // Send message to externally defined function
		break;
	default:break;
	}
}

bool ipc::PipeServer::DisconnectAndReconnect(int iPipe)
{
	if (DisconnectNamedPipe(hPipeHandles[iPipe]) == 0)
	{
		OnEvent(PIPE_EVENT_ERROR, L"DisconnectNamedPipe() failed", iPipe);
		return false;
	}
	
	bool bConnected = ConnectNamedPipe(hPipeHandles[iPipe], &Ovlap[iPipe]) == 0 ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
	if (!bConnected)
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			OnEvent(PIPE_EVENT_ERROR, L"ConnectNamedPipe() failed. Severe error on pipe. Close this handle forever.", iPipe);
			CloseHandle(hPipeHandles[iPipe]);
		}
	}
	return true;
}
bool ipc::PipeServer::Write(std::wstring& sData, int iPipe)
{
	if (!isRunning())
			return false;

	// WRITE TO A SINGLE INSTANCE
	if (iPipe >= 0 && iPipe < PIPE_INSTANCES)
	{
		ZeroMemory(&Ovlap[iPipe], sizeof(OVERLAPPED));
		Ovlap[iPipe].hEvent = hEvents[iPipe];
		if (WriteFile(hPipeHandles[iPipe], sData.c_str(), (DWORD)(sData.size() + 1) * sizeof(TCHAR), NULL, &Ovlap[iPipe]) == 0)
		{
			if (GetLastError() != ERROR_IO_PENDING)
			{
				OnEvent(PIPE_EVENT_ERROR, L"Single pipe WriteFile() failed", iPipe);
				return false;
			}
			else
				bWriteData[iPipe] = true;
		}
		else
			bWriteData[iPipe] = true;
	}
	// WRITE TO ALL INSTANCES
	else
	{
		for (int i = 0; i < PIPE_INSTANCES; i++)
		{
			ZeroMemory(&Ovlap[i], sizeof(OVERLAPPED));
			Ovlap[i].hEvent = hEvents[i];
			if (WriteFile(hPipeHandles[i], sData.c_str(), (DWORD)(sData.size() + 1) * sizeof(TCHAR), NULL, &Ovlap[i]) == 0)
			{
				if (GetLastError() == ERROR_IO_PENDING)
					bWriteData[i] = true;
			}
			else
				bWriteData[i] = true;
		}
	}
	return true;
}

//   Write to log file
void ipc::PipeServer::Log(std::wstring ss)
{
	//disable
	return;

	while (bLock2)
		Sleep(MUTEX_WAIT);
	bLock2 = true;
	std::wstring path = L"c:/programdata/lgtv companion/log2.txt";
	std::ofstream m;

	char buffer[80];
	time_t rawtime;
	struct tm timeinfo;
	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);
	strftime(buffer, 80, "%a %H:%M:%S > ", &timeinfo);
	puts(buffer);
	std::wstring s = common::widen(buffer);
	s += ss;
	s += L"\n";

	m.open(path.c_str(), std::ios::out | std::ios::app);
	if (m.is_open())
	{
		m << common::narrow(s).c_str();
		m.close();
	}
	bLock2 = false;
}



// PIPECLIENT Class Constructor. sName = name of Named Pipe, fcnPtr =  pointer to callback function
ipc::PipeClient::PipeClient(std::wstring sName, void (*fcnPtr)(std::wstring))
{
	if (sName == L"" || fcnPtr == NULL)
		return;

	Buffer[PIPE_BUF_SIZE] = '\0'; 

	Log(L" ");
	Log(L"-----------------------------");
	Log(L"PipeClient Started!");

	sPipeName = sName;
	FunctionPtr = fcnPtr;

	if(!Init())
		Log(L"PipeClient Init() failed!");
}
// PIPECLIENT Class Destructor. 
ipc::PipeClient::~PipeClient(void)
{
	Terminate();
}
//Initialise the PipeClient, start WorkerThread etc
bool ipc::PipeClient::Init()
{
	if (isRunning())
		if(!Terminate())
			return false;
	
	Buffer[0] = '\0';

	while (1)
	{

		hFile = CreateFile(sPipeName.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
			NULL);

		if (hFile != INVALID_HANDLE_VALUE)
			break;
		// Exit if an error other than ERROR_PIPE_BUSY occurs. 
		if (GetLastError() != ERROR_PIPE_BUSY)
		{
			hFile = NULL;
			OnEvent(PIPE_EVENT_ERROR, L"Failed to open pipe with CreateFile()");
			return false;
		}
		// All pipe instances are busy, so wait for 2 seconds. 
		if (!WaitNamedPipe(sPipeName.c_str(), 2000))
		{
			hFile = NULL;
			OnEvent(PIPE_EVENT_ERROR, L"Failed to open pipe: 2 second wait timed out.");
			return false;
		}
	}

	// Create event handles (incl termination and termination accept event)
	for (int i = 0; i < 3; i++)
	{
		if ((hEvents[i] = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
		{
			CloseHandle(hFile); hFile = NULL;
			for (int j = 0; j < i; j++)
			{
				CloseHandle(hEvents[j]);
				hEvents[j] = NULL;
			}
			OnEvent(PIPE_EVENT_ERROR, L"Failed to CreateEvent()");
			return false;
		}
	}

	ZeroMemory(&Ovlap, sizeof(OVERLAPPED));
	Ovlap.hEvent = hEvents[0];
	if (ReadFile(hFile, Buffer, PIPE_BUF_SIZE, NULL, &Ovlap) == 0)
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			CloseHandle(hFile); hFile = NULL;
			CloseHandle(hEvents[0]); hEvents[0] = NULL;
			CloseHandle(hEvents[1]); hEvents[1] = NULL;
			CloseHandle(hEvents[2]); hEvents[2] = NULL;
			OnEvent(PIPE_EVENT_ERROR, L"Failed to overlapped ReadFile() when initialising");
			CloseHandle(hFile);
			return false;
		}
	}
	else
	{
		std::wstring msg = L"First call to ReadFile() is non-zero. Buffer = ";
		msg += Buffer;
		Log(msg);
	}

	std::thread thread_obj(&PipeClient::WorkerThread, this);
	thread_obj.detach();

	return true;
}
// Terminate the PIPECLIENT 
bool ipc::PipeClient::Terminate()
{
	if (!isRunning())
		return false;

	// Signal worker thread to exit
	SetEvent(hEvents[1]);
	//Wait for the signal when worker thread is actually exiting (max 2s)
	WaitForSingleObject(hEvents[2], 2000);
	
	//Close handles
	if(hFile)
	{
		CloseHandle(hFile);
		hFile = NULL;
	}
	for(int i = 0; i < 3; i++)
	{
		if(hEvents[i])
		{
			CloseHandle(hEvents[i]);
			hEvents[i] = NULL;
		}
	}

	Log(L"PipeClient terminated!");
	return true;
}
// Is the client running
bool ipc::PipeClient::isRunning()
{
	return iState == PIPE_RUNNING ? TRUE : FALSE;
}
//Send message to named pipe. sData = message
bool ipc::PipeClient::Send(std::wstring sData)
{
	return Write(sData);
}

//The worker thread managing the overlapped pipes
void ipc::PipeClient::WorkerThread()
{
	DWORD dwRet;
	DWORD dwPipe;
	Log(L"Worker Thread started!");
	OnEvent(PIPE_RUNNING);
	while (1)
	{
		if ((dwRet = WaitForMultipleObjects(2, hEvents, FALSE, INFINITE)) == WAIT_FAILED)
		{
			OnEvent(PIPE_EVENT_ERROR, L"WaitForMultipleObjects() returned WAIT_FAILED.");
			goto WORKER_THREAD_END;
		}
		dwPipe = dwRet - WAIT_OBJECT_0;
		ResetEvent(hEvents[dwPipe]);

		// Check if the termination event has been signalled
		if (dwPipe == 1)
		{		
			goto WORKER_THREAD_END;;
		}
		
		// Check overlapped results, and if they fail, reestablish
		// communication; otherwise, process read and write operations
		if (GetOverlappedResult(hFile, &Ovlap, &dwBytesTransferred, TRUE) == 0)
		{
			if (!DisconnectAndReconnect())
			{
				OnEvent(PIPE_EVENT_ERROR, L"GetOverlappedResult failed. DisconnectAndReconnect() failed.");
				goto WORKER_THREAD_END;;
			}
			else
				OnEvent(PIPE_EVENT_ERROR, L"GetOverlappedResult failed. DisconnectAndReconnect() success.");
		}
		else
		{
			// Check the state of the pipe. If bWriteData equals
			// FALSE, post a read on the pipe for incoming data.			
			if (!bWriteData) // PERFORM READ
			{
				std::wstringstream ss;
				ss << L"READ! dwBytesTransferred = " << dwBytesTransferred;
				Log(ss.str());

				ZeroMemory(&Ovlap, sizeof(OVERLAPPED));
				Ovlap.hEvent = hEvents[0];
				if (ReadFile(hFile, Buffer, PIPE_BUF_SIZE, NULL, &Ovlap) == 0)
				{
					if (GetLastError() != ERROR_IO_PENDING)
					{
						Log(L"ReadFile error. Disconnect and reconnect");
						if (!DisconnectAndReconnect())
						{
							OnEvent(PIPE_EVENT_ERROR, L"ReadFile() failed. DisconnectAndReconnect() failed.");
							goto WORKER_THREAD_END;;
						}
						else
							OnEvent(PIPE_EVENT_ERROR, L"ReadFile() failed. DisconnectAndReconnect() success.");
					}
					else
						OnEvent(PIPE_EVENT_READ, Buffer);
				}
			}
			else
			{
				std::wstringstream ss;
				ss << L"WRITE! dwBytesTransferred: " << dwBytesTransferred;
				Log(ss.str());

				OnEvent(PIPE_EVENT_SEND);
			}
		}
	}
WORKER_THREAD_END:
	Log(L"Worker Thread is exiting!");
	OnEvent(PIPE_EVENT_TERMINATE);
	SetEvent(hEvents[2]);
}

void ipc::PipeClient::OnEvent(int nEventID, std::wstring sData)
{
	std::wstring msg;
	switch (nEventID)
	{
	case PIPE_RUNNING:
		iState = PIPE_RUNNING;
		break;
	case PIPE_EVENT_TERMINATE:
		iState = PIPE_NOT_RUNNING;
		break;
	case PIPE_EVENT_SEND:
		bWriteData = false;
		break;
	case PIPE_EVENT_READ:
		FunctionPtr(sData); // Send message to external function/callback
		break;
	case PIPE_EVENT_ERROR:
		msg = L"[ERROR] ";
		msg += sData;
		Log(msg);
		//		FunctionPtr(msg); // Send message to externally defined function
		break;
	default:break;
	}
}

bool ipc::PipeClient::DisconnectAndReconnect()
{
	if (CloseHandle(hFile) == 0)
	{
		OnEvent(PIPE_EVENT_ERROR, L"CloseHandle() failed");
		return false;
	}

	while (1)
	{
		hFile = CreateFile(sPipeName.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
			NULL);

		if (hFile != INVALID_HANDLE_VALUE)
			break;
		// Exit if an error other than ERROR_PIPE_BUSY occurs. 
		if (GetLastError() != ERROR_PIPE_BUSY)
		{
			OnEvent(PIPE_EVENT_ERROR, L"DisconnectAndReconnect() - Failed to open pipe with CreateFile()");
			return false;
		}
		// All pipe instances are busy, so wait for 2 seconds. 
		if (!WaitNamedPipe(sPipeName.c_str(), 2000))
		{
			OnEvent(PIPE_EVENT_ERROR, L"DisconnectAndReconnect() - Failed to open pipe: 2 second wait timed out.");
			return false;
		}
	}
	ZeroMemory(&Ovlap, sizeof(OVERLAPPED));
	Ovlap.hEvent = hEvents[0];
	if (ReadFile(hFile, Buffer, PIPE_BUF_SIZE, NULL, &Ovlap) == 0)
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			CloseHandle(hFile); hFile = NULL;
			CloseHandle(hEvents[0]); hEvents[0] = NULL;
			CloseHandle(hEvents[1]); hEvents[1] = NULL;
			CloseHandle(hEvents[2]); hEvents[2] = NULL;
			OnEvent(PIPE_EVENT_ERROR, L"Failed to overlapped ReadFile() when initialising");
			CloseHandle(hFile);
			return false;
		}
	}
	return true;
}
bool ipc::PipeClient::Write(std::wstring& sData)
{
	if (!isRunning())
		if (!Init())
			return false;

	// WRITE TO THE PIPE INSTANCE
	ZeroMemory(&Ovlap, sizeof(OVERLAPPED));
	Ovlap.hEvent = hEvents[0];
	if (WriteFile(hFile, sData.c_str(), (DWORD)(sData.size()+1)*sizeof(TCHAR), NULL, &Ovlap) == 0)
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			OnEvent(PIPE_EVENT_ERROR, L"WriteFile() failed");
			return false;
		}
		else
			bWriteData = true;
	}
	else
		bWriteData = true;
	return true;
}

//   Write to log file
void ipc::PipeClient::Log(std::wstring ss)
{
	//disable
	return;

	while (bLock)
		Sleep(MUTEX_WAIT);
	bLock = true;
	std::wstring path = L"c:/programdata/lgtv companion/log3.txt";
	std::ofstream m;

	char buffer[80];
	time_t rawtime;
	struct tm timeinfo;
	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);
	strftime(buffer, 80, "%a %H:%M:%S > ", &timeinfo);
	puts(buffer);
	std::wstring s = common::widen(buffer);
	WCHAR szPath[MAX_PATH];

	GetModuleFileNameW(NULL, szPath, MAX_PATH);
	s += L"["; s += szPath; s += L"] ";
	s += ss;
	s += L"\n";


	m.open(path.c_str(), std::ios::out | std::ios::app);
	if (m.is_open())
	{
		m << common::narrow(s).c_str();
		m.close();
	}
	bLock = false;
}
