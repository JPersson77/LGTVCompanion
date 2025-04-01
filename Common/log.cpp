#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "log.h"
#include "tools.h"
#include <Windows.h>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <sstream>

#define			LOG_INFO			1
#define			LOG_WARNING			2
#define			LOG_ERROR			3
#define			LOG_DEBUG			4

#define			LOG_MUTEX_WAIT		20
#define			LOG_GENERIC_ID		"System"
#define			ID_WIDTH			11

#define			MAX_LOG_SIZE		4000 //max size in kB before log will be trimmed
#define			MAX_LOG_SIZE_TRIM	3000 //max size of the log in kB after trimming.

class Logging::Impl : public std::enable_shared_from_this<Logging::Impl> 
{
public:
	std::string file_path_;
	int log_level_ = LOG_LEVEL_INFO;
	std::string last_log_time_ = "";
	std::string last_log_date_ = "";
	inline static std::mutex log_mutex_;

	Impl(int log_level, std::wstring file);
	~Impl() {};
	std::string parseMessage(std::string& message, std::string& arg_1, std::string& arg_2, std::string& arg_3, std::string& arg_4);
	void writeToDisk(int type, std::string id, std::string log_message);
};
Logging::Impl::Impl(int log_level, std::wstring file)
{
	log_level_ = log_level;
	file_path_ = tools::narrow(file);
	if(std::filesystem::exists(file_path_))
	{
		auto size = std::filesystem::file_size(file_path_);
		if (size > MAX_LOG_SIZE * 1000)
		{
			std::stringstream buffer;
			std::ifstream is(file_path_);
			if (is)
			{
				is.seekg(-(MAX_LOG_SIZE_TRIM*1000), is.end);
				std::stringstream buffer;
				buffer << is.rdbuf();
				std::string buf = buffer.str();
				is.close();
				if (buf.size() > 0)
				{
					size_t start = buf.find("\n \n");
					if (start != std::string::npos)
					{
						std::ofstream os;
						std::string out = "log trimmed... \n\n";
						out += buf.substr(start+3);
						os.open(file_path_, std::ios::out | std::ios::trunc);
						if (os.is_open())
						{
							os << out;
							os.close();
						}
					}
				}
			}

		}
	}
}
std::string Logging::Impl::parseMessage(std::string& message, std::string& arg_1, std::string& arg_2, std::string& arg_3, std::string& arg_4)
{
	if (arg_1 != "")
		tools::replaceAllInPlace(message, "%1%", arg_1);
	if (arg_2 != "")
		tools::replaceAllInPlace(message, "%2%", arg_2);
	if (arg_3 != "")
		tools::replaceAllInPlace(message, "%3%", arg_3);
	if (arg_4 != "")
		tools::replaceAllInPlace(message, "%4%", arg_4);
	return message;
}
void Logging::Impl::writeToDisk(int type, std::string id, std::string log_message)
{
	//thread safe
	while (!log_mutex_.try_lock())
		Sleep(LOG_MUTEX_WAIT);

	std::string out;
	std::ofstream m;
	time_t rawtime;
	struct tm timeinfo;
	char buffer[80];
	if (log_message != "" && log_message != " ")
	{
		time(&rawtime);
		localtime_s(&timeinfo, &rawtime);
		strftime(buffer, 80, "[%a %b %d ", &timeinfo);
		if (last_log_date_ != buffer)
			out = buffer;
		else
			out = "[     .     ";
		last_log_date_ = buffer;
//		puts(buffer);
		strftime(buffer, 80, "%H:%M:%S]", &timeinfo);
		if (last_log_time_ != buffer)
			out += buffer;
		else
			out += "   .    ]";
		last_log_time_ = buffer;
		if (type == LOG_INFO)
			out += "[-I--][";
		else if (type == LOG_WARNING)
			out += "[--W-][";
		else if (type == LOG_ERROR)
			out += "[---E][";
		else if (type == LOG_DEBUG)
			out += "[D---][";

		if (id == "")
			id = LOG_GENERIC_ID;
		
		int pad;
		if (ID_WIDTH - id.length() >= 0)
			pad = (ID_WIDTH - (int)id.length()) / 2;
		else
			pad = 0;

		for (int i = 0; i < pad; i++)
			id.insert(0, " ");
		id += "           ";
		id = id.substr(0, ID_WIDTH);
		out += id;
		out += "]  ";

		out += log_message;
		out += "\n";
	}
	else
		out = " \n";

	m.open(file_path_, std::ios::out | std::ios::app);
	if (m.is_open())
	{
		m << out;
		m.close();
	}
	log_mutex_.unlock();
}

Logging::Logging(int log_level, std::wstring log_file)
	: pimpl(std::make_shared <Impl>(log_level, log_file))
{
}
void Logging::debug(std::string id, std::string message, std::string arg_1, std::string arg_2, std::string arg_3, std::string arg_4)
{
	if (pimpl->log_level_ == LOG_LEVEL_DEBUG)
		pimpl->writeToDisk(LOG_DEBUG, id, pimpl->parseMessage(message, arg_1, arg_2, arg_3, arg_4));
}
void Logging::info(std::string id, std::string message, std::string arg_1, std::string arg_2, std::string arg_3, std::string arg_4)
{
	if (pimpl->log_level_ == LOG_LEVEL_INFO || pimpl->log_level_ == LOG_LEVEL_DEBUG)
		pimpl->writeToDisk(LOG_INFO, id, pimpl->parseMessage(message, arg_1, arg_2, arg_3, arg_4));
}
void Logging::warning(std::string id, std::string message, std::string arg_1, std::string arg_2, std::string arg_3, std::string arg_4)
{
	if (pimpl->log_level_ == LOG_LEVEL_INFO || pimpl->log_level_ == LOG_LEVEL_WARNING || pimpl->log_level_ == LOG_LEVEL_DEBUG)
		pimpl->writeToDisk(LOG_WARNING, id, pimpl->parseMessage(message, arg_1, arg_2, arg_3, arg_4));
}
void Logging::error(std::string id, std::string message, std::string arg_1, std::string arg_2, std::string arg_3, std::string arg_4)
{
	if (pimpl->log_level_ == LOG_LEVEL_INFO || pimpl->log_level_ == LOG_LEVEL_WARNING || pimpl->log_level_ == LOG_LEVEL_ERROR || pimpl->log_level_ == LOG_LEVEL_DEBUG)
		pimpl->writeToDisk(LOG_ERROR, id, pimpl->parseMessage(message, arg_1, arg_2, arg_3, arg_4));
}
