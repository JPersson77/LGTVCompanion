// LGTV Linux Companion — a Linux port of LGTV Companion.
//
// Copyright © 2021-2026 Jörgen Persson
// Licensed under the MIT License. See the LICENSE file at the repository root
// for the full license text, which must accompany all copies.

#pragma once
#include "device.h"
#include <memory>
#include <string>


#define			LOG_LEVEL_OFF			0
#define			LOG_LEVEL_INFO			1
#define			LOG_LEVEL_WARNING		2
#define			LOG_LEVEL_ERROR			3
#define			LOG_LEVEL_DEBUG			4

// Logging functionality
class Logging : public std::enable_shared_from_this<Logging>
{
private:
	class Impl;
	std::shared_ptr<Impl> pimpl; //Pointer to IMPLementation

public:
	Logging(int log_level, std::string log_file);
	~Logging() {};
	void info(std::string id, std::string message, std::string arg_1 = "", std::string arg_2 = "", std::string arg_3 = "", std::string arg_4 = "");
	void warning(std::string id, std::string message, std::string arg_1 = "", std::string arg_2 = "", std::string arg_3 = "", std::string arg_4 = "");
	void error(std::string id, std::string message, std::string arg_1 = "", std::string arg_2 = "", std::string arg_3 = "", std::string arg_4 = "");
	void debug(std::string id, std::string message, std::string arg_1 = "", std::string arg_2 = "", std::string arg_3 = "", std::string arg_4 = "");
};
