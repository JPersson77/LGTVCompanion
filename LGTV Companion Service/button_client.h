#pragma once
#include <memory>
#include <vector>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include "../Common/device.h"
#include "../Common/log.h"

// Asynchronous websocket client to communicate and maintain the connection with the button interface
class ButtonClient : public std::enable_shared_from_this<ButtonClient>
{
private:
	class Impl;
	std::shared_ptr<Impl> pimpl; //Pointer to IMPLementation

public:
	explicit ButtonClient(boost::asio::io_context&, boost::asio::ssl::context&, Device&, Logging&);
	~ButtonClient();
	bool sendButton(std::string button);
	bool setPath(std::string path);
	bool isConnected();
	bool close(bool = false);
};

