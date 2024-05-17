#pragma once
#include <vector>
#include <string>

// system events
#define EVENT_UNDEFINED									0
#define EVENT_SYSTEM_SHUTDOWN							1	
#define EVENT_SYSTEM_REBOOT								2	
#define EVENT_SYSTEM_RESUME								3	
#define EVENT_SYSTEM_RESUMEAUTO							4	
#define EVENT_SYSTEM_SUSPEND							5	
#define EVENT_SYSTEM_DISPLAYOFF							6	
#define EVENT_SYSTEM_DISPLAYON							7	
#define EVENT_SYSTEM_UNSURE								8	
#define EVENT_SYSTEM_DISPLAYDIMMED						9	
#define EVENT_SYSTEM_USERBUSY							10	
#define EVENT_SYSTEM_USERIDLE							11	
#define EVENT_SYSTEM_BOOT								12	
#define EVENT_SYSTEM_TOPOLOGY							13	
#define EVENT_SYSTEM_BLANKSCREEN						14	

// user (forced) events 
#define EVENT_FORCE_DISPLAYON							30	
#define EVENT_FORCE_DISPLAYOFF							31	
#define EVENT_FORCE_BLANKSCREEN							32	

// generic request events
#define EVENT_REQUEST									60	
#define EVENT_LUNA_SYSTEMSET_BASIC						61	
#define EVENT_LUNA_SYSTEMSET_PAYLOAD					62	
#define EVENT_BUTTON									63	
#define EVENT_LUNA_DEVICEINFO							64	
#define EVENT_LUNA_GENERIC								65	

#define EVENT_SHUTDOWN_TYPE_UNDEFINED					90
#define EVENT_SHUTDOWN_TYPE_SHUTDOWN					91
#define EVENT_SHUTDOWN_TYPE_REBOOT						92
#define EVENT_SHUTDOWN_TYPE_UNSURE						93
#define EVENT_SHUTDOWN_TYPE_ERROR						94


// Transform a system-generated or user generated (forced) event into webOS json.
class Event {
private:
	std::vector<std::string>		devices_;
	int								type_ = EVENT_UNDEFINED;
//	inline static int				event_id_ = 0; 
	std::string						data_;
	std::string						log_message_;
	std::string						createRequest(std::string ep, std::string payload = "", bool = false);
	std::string						createLunaPayload(std::string, std::string);

public:
	Event() {};
	~Event() {};
	std::vector<std::string>		getDevices(void) ;
	int								getType(void) ;
	std::string						getData(void);
	std::string						getLogMessage(void);
//	void							set(int type) ;
	void							set(int type, std::vector<std::string> devices = {}, std::string arg_1 = "", std::string arg_2 = "", std::string arg_3 = "", std::string arg_4 = "", std::string arg_5 = "");
};
