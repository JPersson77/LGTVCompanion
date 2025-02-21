#pragma once
#include "../Common/preferences.h"
#include "../Common/device.h"
#include "../Common/log.h"
#include <memory>
#include <vector>
#include <string>
#include "../Common/event.h"

// Implementes the core logic for processing system and user events, and feeding them into the webOS client
class Companion : public std::enable_shared_from_this<Companion>
{
private:
	class Impl;
	std::shared_ptr<Impl> pimpl; //Pointer to IMPLementation

public:
	Companion(Preferences&);
	~Companion() {};
	void					systemEvent(int event, std::string data = "");
	void					shutdown(void);
	bool					isBusy(void);
};

