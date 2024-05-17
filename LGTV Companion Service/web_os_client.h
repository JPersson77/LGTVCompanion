#pragma once
#include <memory>
#include <vector>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include "../Common/device.h"
#include "../Common/log.h"

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
	bool sendRequest(std::string data, std::string log_message);
	bool sendButton(std::string button);
	bool close(bool = false);
};
