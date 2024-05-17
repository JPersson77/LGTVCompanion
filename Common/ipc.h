#pragma once
#include <memory>
#include <string>
#include <Windows.h>

// Intra-process communications server. Callback address for receiving messages is passed via fcnPtr
class IpcServer : public std::enable_shared_from_this<IpcServer>
{
private:
	class Impl;
	std::shared_ptr<Impl> pimpl; //Pointer to IMPLementation
public:
	IpcServer(std::wstring ipc_name, void (*fcnPtr)(std::wstring, LPVOID), LPVOID lpObject);
	~IpcServer(void);
	bool								send(std::wstring data, int pipe = -1);
	bool								terminate(void);
};

// Intra-process communications client. Callback address for receiving messages is passed via fcnPtr
class IpcClient : public std::enable_shared_from_this<IpcClient>
{
private:
	class Impl;
	std::shared_ptr<Impl> pimpl; //Pointer to IMPLementation

public:
	IpcClient(std::wstring ipc_name, void (*fcnPtr)(std::wstring, LPVOID), LPVOID lpObject);
	~IpcClient(void);
	bool								init(void);
	bool								send(std::wstring data);
	bool								terminate(void);
	bool								isRunning(void);
};
