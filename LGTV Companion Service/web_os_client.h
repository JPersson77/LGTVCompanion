#pragma once
#include <memory>
#include <vector>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include "../Common/device.h"
#include "../Common/log.h"

// TV power state captured at remote-stream start (returned by streamStartPower())
#define			STREAM_START_UNKNOWN				-1
#define			STREAM_START_OFF					0
#define			STREAM_START_ON						1

// Asynchronous websocket client to communicate and maintain the connection with a webOS device
class WebOsClient : public std::enable_shared_from_this<WebOsClient>
{
private:
	class Impl;
	std::shared_ptr<Impl> pimpl; //Pointer to IMPLementation

public:
	explicit WebOsClient(boost::asio::io_context&, boost::asio::ssl::context&, Device&, Logging&) ;
	~WebOsClient();
	bool powerOn(void);
	bool powerOff(bool = false);
	bool blankScreen(bool = false);
	void beginStreamStartCapture(void);	// arm one-shot capture of the TV power state at the next power-off query
	int streamStartPower(void);			// -1 unknown, 0 was off, 1 was on (see STREAM_START_* in web_os_client.cpp)
	bool sendRequest(std::string data, std::string log_message, int delay = 0);
	bool sendButton(std::string button);
	bool close(bool = false);
};
