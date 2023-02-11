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
			// Multi-monitor topology support
			j = jsonPrefs[JSON_PREFS_NODE][JSON_ADHERETOPOLOGY];
			if (!j.empty() && j.is_boolean())
				AdhereTopology = j.get<bool>();
			// User idle mode whitelist enabled
			j = jsonPrefs[JSON_PREFS_NODE][JSON_IDLEWHITELIST];
			if (!j.empty() && j.is_boolean())
				bIdleWhitelistEnabled = j.get<bool>();
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
			// User idle mode whitelist
			j = jsonPrefs[JSON_PREFS_NODE][JSON_WHITELIST];
			if (!j.empty() && j.size() > 0)
			{
				for (auto& elem : j.items())
				{
					WHITELIST w;
					w.Application = common::widen(elem.value().get<std::string>());
					w.Name = common::widen(elem.key());
					WhiteList.push_back(w);
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

				if (item.value()[JSON_DEVICE_ENABLED].is_boolean())
					params.Enabled = item.value()[JSON_DEVICE_ENABLED].get<bool>();

				if (item.value()[JSON_DEVICE_SESSIONKEY].is_string())
					params.SessionKey = item.value()[JSON_DEVICE_SESSIONKEY].get<std::string>();

				if (item.value()[JSON_DEVICE_SUBNET].is_string())
					params.Subnet = item.value()[JSON_DEVICE_SUBNET].get<std::string>();

				if (item.value()[JSON_DEVICE_WOLTYPE].is_number())
					params.WOLtype = item.value()[JSON_DEVICE_WOLTYPE].get<int>();

				if (item.value()[JSON_DEVICE_SETHDMI].is_boolean())
					params.SetHDMIInputOnResume = item.value()[JSON_DEVICE_SETHDMI].get<bool>();

				if (item.value()[JSON_DEVICE_NEWSOCK].is_boolean())
					params.SSL = item.value()[JSON_DEVICE_NEWSOCK].get<bool>();

				if (item.value()[JSON_DEVICE_SETHDMINO].is_number())
					params.SetHDMIInputOnResumeToNumber = item.value()[JSON_DEVICE_SETHDMINO].get<int>();

				j = item.value()[JSON_DEVICE_MAC];
				if (!j.empty() && j.size() > 0)
				{
					for (auto& m : j.items())
					{
						params.MAC.push_back(m.value().get<std::string>());
					}
				}
				params.PowerOnTimeout = PowerOnTimeout;
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
	prefs[JSON_PREFS_NODE][JSON_REMOTESTREAM] = (bool)RemoteStreamingCheck;
	prefs[JSON_PREFS_NODE][JSON_TOPOLOGYMODE] = (bool)TopologyPreferPowerEfficiency;
	for (auto& item : EventLogRestartString)
		prefs[JSON_PREFS_NODE][JSON_EVENT_RESTART_STRINGS].push_back(item);
	for (auto& item : EventLogShutdownString)
		prefs[JSON_PREFS_NODE][JSON_EVENT_SHUTDOWN_STRINGS].push_back(item);
	if (WhiteList.size() > 0)
		for (auto& w : WhiteList)
			prefs[JSON_PREFS_NODE][JSON_WHITELIST][common::narrow(w.Name)] = common::narrow(w.Application);

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