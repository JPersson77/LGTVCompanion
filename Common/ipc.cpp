#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0603
#define _WIN32_WINNT 0x0603
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "ipc.h"
#include <atomic>
#include <thread>

// TODO: consider using boost::interprocess

#define PIPE_BUF_SIZE					1024
#define PIPE_INSTANCES					5
#define PIPE_NOT_RUNNING				99
#define PIPE_EVENT_ERROR				100
#define PIPE_EVENT_READ					101 // Data was read 
#define PIPE_EVENT_SEND					102 // Write data
#define PIPE_RUNNING					103 // Server is running
#define PIPE_EVENT_TERMINATE			120 // Terminate

class IpcServer::Impl : public std::enable_shared_from_this<IpcServer::Impl> {
private:
	HANDLE								pipeHandles_[PIPE_INSTANCES] = {};
	DWORD								bytes_transferred_ = 0;
	TCHAR								buffer_[PIPE_INSTANCES][PIPE_BUF_SIZE + 1] = {};
	OVERLAPPED							ovlap_[PIPE_INSTANCES] = {};
	HANDLE								events_[PIPE_INSTANCES + 2] = {}; // include termination and termination confirm events
	std::atomic_bool					writeData_[PIPE_INSTANCES] = {};
	std::wstring						pipeName_;
	std::atomic_int						state_ = PIPE_NOT_RUNNING;
	void								(*functionPtr_)(std::wstring, LPVOID);
	LPVOID								object_pointer_;

	void								onEvent(int, std::wstring sData = L"", int iPipe = -1);
	void								workerThread();
	bool								isRunning(void);
	bool								disconnectAndReconnect(int);

public:
	Impl(std::wstring, void (*fcnPtr)(std::wstring, LPVOID), LPVOID);
	~Impl() {};
	bool								terminate(void);
	bool								write(std::wstring&, int);
};

class IpcClient::Impl : public std::enable_shared_from_this<IpcClient::Impl> {
private:
	HANDLE								file_ = NULL;
//	HANDLE								pipe_handle_ = {};
	DWORD								bytes_transferred_ = 0;
	TCHAR								buffer_[PIPE_BUF_SIZE + 1] = {};
	OVERLAPPED							ovlap_ = {};
	HANDLE								events_[3] = {}; // overlapped event, terminate event, terminate accept event
	std::wstring						pipe_name_;
	std::atomic_int						state_ = PIPE_NOT_RUNNING;
	std::atomic_bool					write_data_ = {};
	void								(*function_ptr_)(std::wstring, LPVOID);
	LPVOID								object_pointer_;

	void								onEvent(int, std::wstring sData = L"");
	void								workerThread();
	bool								disconnectAndReconnect();

public:
	Impl(std::wstring, void (*fcnPtr)(std::wstring, LPVOID), LPVOID);
	~Impl(void);
	bool								init(void);
	bool								isRunning(void);
	bool								terminate(void);
	bool								write(std::wstring&);

};
IpcServer::IpcServer(std::wstring ipc_name, void (*fcnPtr)(std::wstring, LPVOID), LPVOID lpObject)
	: pimpl(std::make_shared<Impl>(ipc_name, fcnPtr, lpObject)) {}
IpcServer::~IpcServer()
{
	terminate();
}
IpcClient::~IpcClient()
{
	terminate();
}
IpcClient::IpcClient(std::wstring ipc_name, void (*fcnPtr)(std::wstring, LPVOID), LPVOID lpObject)
	: pimpl(std::make_shared<Impl>(ipc_name, fcnPtr, lpObject)) {}
IpcServer::Impl::Impl(std::wstring ipc_name, void (*fcnPtr)(std::wstring, LPVOID), LPVOID lpObject)
{
	if (ipc_name == L"" || fcnPtr == NULL)
		return;
	object_pointer_ = lpObject;
	/*
	for (int i = 0; i < PIPE_INSTANCES; i++)
	{
		buffer_[i][PIPE_BUF_SIZE] = '\0';
		buffer_[i][0] = '\0';
	}
	*/
	pipeName_ = ipc_name;
	functionPtr_ = fcnPtr;

	PSECURITY_DESCRIPTOR psd = NULL;
	BYTE  sd[SECURITY_DESCRIPTOR_MIN_LENGTH];
	psd = (PSECURITY_DESCRIPTOR)sd;
	InitializeSecurityDescriptor(psd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(psd, TRUE, (PACL)NULL, FALSE);
	SECURITY_ATTRIBUTES sa = { sizeof(sa), psd, FALSE };
	for (int i = 0; i < PIPE_INSTANCES; i++)
	{
		// Create a named pipe instance
		if ((pipeHandles_[i] = CreateNamedPipe(pipeName_.c_str(),
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, PIPE_INSTANCES,
			PIPE_BUF_SIZE, PIPE_BUF_SIZE, 1000, &sa)) == INVALID_HANDLE_VALUE)
		{
			for (int j = 0; j < i; j++)
			{
				CloseHandle(pipeHandles_[j]);
				pipeHandles_[j] = NULL;
			}
			onEvent(PIPE_EVENT_ERROR, L"Failed to CreateNamedPipe()", i);
			return;
		}
		// Create an event handle for each pipe instance. This
		// will be used to monitor overlapped I/O activity on each pipe.
		if ((events_[i] = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
		{
			for (int j = 0; j < PIPE_INSTANCES; j++)
			{
				CloseHandle(pipeHandles_[j]);
				pipeHandles_[j] = NULL;
			}
			for (int j = 0; j < i; j++)
			{
				CloseHandle(events_[j]);
				events_[j] = NULL;
			}
			onEvent(PIPE_EVENT_ERROR, L"Failed to CreateEvent()", i);
			return;
		}

		ZeroMemory(&ovlap_[i], sizeof(OVERLAPPED));
		ovlap_[i].hEvent = events_[i];

		// Listen for client connections using ConnectNamedPipe()
		bool bConnected = ConnectNamedPipe(pipeHandles_[i], &ovlap_[i]) == 0 ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
		if (!bConnected)
		{
			if (GetLastError() != ERROR_IO_PENDING)
			{
				for (int j = 0; j < PIPE_INSTANCES; j++)
				{
					CloseHandle(pipeHandles_[j]);
					pipeHandles_[j] = NULL;
					CloseHandle(events_[j]);
					events_[j] = NULL;
				}
				onEvent(PIPE_EVENT_ERROR, L"Failed to ConnectNamedPipe() when initialising", i);
				return;
			}
		}
	}
	//termination event
	if ((events_[PIPE_INSTANCES] = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
	{
		for (int j = 0; j < PIPE_INSTANCES; j++)
		{
			CloseHandle(pipeHandles_[j]);
			pipeHandles_[j] = NULL;
			CloseHandle(events_[j]);
			events_[j] = NULL;
		}
		onEvent(PIPE_EVENT_ERROR, L"Failed to Ccreate termination event");
	}
	//termination confirm event
	if ((events_[PIPE_INSTANCES + 1] = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
	{
		for (int j = 0; j < PIPE_INSTANCES; j++)
		{
			CloseHandle(pipeHandles_[j]);
			pipeHandles_[j] = NULL;
			CloseHandle(events_[j]);
			events_[j] = NULL;
		}
		CloseHandle(pipeHandles_[PIPE_INSTANCES]); pipeHandles_[PIPE_INSTANCES] = NULL;
		onEvent(PIPE_EVENT_ERROR, L"Failed to Create termination confirm event");
	}
	std::thread thread_obj(&IpcServer::Impl::workerThread, this);
	thread_obj.detach();
}
bool IpcServer::Impl::terminate(void)
{
	if (!isRunning())
		return false;

	// Signal worker thread to exit
	SetEvent(events_[PIPE_INSTANCES]);
	//Create a new event and wait for the signal when worker thread is actually exiting
	WaitForSingleObject(events_[PIPE_INSTANCES + 1], 2000);
	//Close all handles
	for (int i = 0; i < PIPE_INSTANCES; i++)
	{
		if (pipeHandles_[i])
		{
			CloseHandle(pipeHandles_[i]);
			pipeHandles_[i] = NULL;
		}
		if (events_[i])
		{
			CloseHandle(events_[i]);
			events_[i] = NULL;
		}
	}
	return true;
}
bool IpcServer::Impl::isRunning()
{
	return state_ == PIPE_RUNNING ? true : false;
}
void IpcServer::Impl::workerThread()
{
	DWORD dwRet;
	DWORD dwPipe;
//	Log(L"Worker Thread started!");
	onEvent(PIPE_RUNNING);
	while (1)
	{
		if ((dwRet = WaitForMultipleObjects(PIPE_INSTANCES + 1, events_, FALSE, INFINITE)) == WAIT_FAILED)
		{
			onEvent(PIPE_EVENT_ERROR, L"WaitForMultipleObjects() returned WAIT_FAILED.");
			goto WORKER_THREAD_END;
		}

		dwPipe = dwRet - WAIT_OBJECT_0;
		ResetEvent(events_[dwPipe]);

		// Check if the termination event has been signalled
		if (dwPipe == PIPE_INSTANCES)
		{
			goto WORKER_THREAD_END;
		}

		// Check overlapped results, and if they fail, reestablish
		// communication for a new client; otherwise, process read and write operations with the client
		if (GetOverlappedResult(pipeHandles_[dwPipe], &ovlap_[dwPipe], &bytes_transferred_, TRUE) == 0)
		{
			if (!disconnectAndReconnect(dwPipe))
			{
				onEvent(PIPE_EVENT_ERROR, L"GetOverlappedResult failed. DisconnecAndReconnect() has Failed.", dwPipe);
				//				goto WORKER_THREAD_END;
			}
			else
				onEvent(PIPE_EVENT_ERROR, L"GetOverlappedResult failed. DisconnectAndReconnect() success.", dwPipe);
		}
		else
		{
			// Check the state of the pipe. If bWriteData equals
			// FALSE, post a read on the pipe for incoming data.			
			if (!writeData_[dwPipe]) // PERFORM READ
			{
				ZeroMemory(&ovlap_[dwPipe], sizeof(OVERLAPPED));
				ovlap_[dwPipe].hEvent = events_[dwPipe];
				if (ReadFile(pipeHandles_[dwPipe], buffer_[dwPipe], PIPE_BUF_SIZE, NULL, &ovlap_[dwPipe]) == 0)
				{
					if (GetLastError() != ERROR_IO_PENDING)
					{
						if (!disconnectAndReconnect(dwPipe))
						{
							onEvent(PIPE_EVENT_ERROR, L"ReadFile() failed. DisconnectAndReconnect() failed.", dwPipe);
							//							goto WORKER_THREAD_END;
						}
						else
							onEvent(PIPE_EVENT_ERROR, L"ReadFile() failed. DisconnectAndReconnect() success.", dwPipe);
					}
					else if (bytes_transferred_ >= 2 * sizeof(TCHAR))
					{					
						onEvent(PIPE_EVENT_READ, std::wstring(buffer_[dwPipe]), dwPipe);
					}
					ZeroMemory(buffer_[dwPipe], PIPE_BUF_SIZE * sizeof(TCHAR));
				}
			}
			else
				onEvent(PIPE_EVENT_SEND, L"", dwPipe);
		}
	}
WORKER_THREAD_END:
	onEvent(PIPE_EVENT_TERMINATE);
	SetEvent(events_[PIPE_INSTANCES + 1]);
}

void IpcServer::Impl::onEvent(int nEventID, std::wstring sData, int iPipe)
{
	std::wstring msg;
	switch (nEventID)
	{
	case PIPE_RUNNING:
		state_ = PIPE_RUNNING;
		break;
	case PIPE_EVENT_TERMINATE:
		state_ = PIPE_NOT_RUNNING;
		break;
	case PIPE_EVENT_SEND:
		writeData_[iPipe] = false;
//		ZeroMemory(buffer_[iPipe], PIPE_BUF_SIZE * sizeof(TCHAR));
//		buffer_[iPipe][0] = '/0';
		break;
	case PIPE_EVENT_READ:
		functionPtr_(sData, object_pointer_); // Send message to external function/callback
//		buffer_[iPipe][0] = '/0';
		break;
	case PIPE_EVENT_ERROR:
//		msg = L"[ERROR] ";
//		msg += sData;
//		Log(msg);

		break;
	default:break;
	}
}
bool IpcServer::Impl::disconnectAndReconnect(int pipe)
{
	if (DisconnectNamedPipe(pipeHandles_[pipe]) == 0)
	{
		onEvent(PIPE_EVENT_ERROR, L"DisconnectNamedPipe() failed", pipe);
		return false;
	}

	bool bConnected = ConnectNamedPipe(pipeHandles_[pipe], &ovlap_[pipe]) == 0 ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
	if (!bConnected)
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			onEvent(PIPE_EVENT_ERROR, L"ConnectNamedPipe() failed. Severe error on pipe. Close this handle forever.", pipe);
			CloseHandle(pipeHandles_[pipe]);
		}
	}
	return true;
}
bool IpcServer::Impl::write(std::wstring& sData, int pipe)
{
	if (!isRunning())
		return false;
	if (pipe >= 0 && pipe < PIPE_INSTANCES)
	{
		ZeroMemory(&ovlap_[pipe], sizeof(OVERLAPPED));
		ovlap_[pipe].hEvent = events_[pipe];
		if (WriteFile(pipeHandles_[pipe], sData.c_str(), (DWORD)(sData.size() + 1) * sizeof(TCHAR), NULL, &ovlap_[pipe]) == 0)
		{
			if (GetLastError() != ERROR_IO_PENDING)
			{
				onEvent(PIPE_EVENT_ERROR, L"Single pipe WriteFile() failed", pipe);
				return false;
			}
			else
				writeData_[pipe] = true;
		}
		else
			writeData_[pipe] = true;
	}
	else
	{
		for (int i = 0; i < PIPE_INSTANCES; i++)
		{
			ZeroMemory(&ovlap_[i], sizeof(OVERLAPPED));
			ovlap_[i].hEvent = events_[i];
			if (WriteFile(pipeHandles_[i], sData.c_str(), (DWORD)(sData.size() + 1) * sizeof(TCHAR), NULL, &ovlap_[i]) == 0)
			{
				if (GetLastError() == ERROR_IO_PENDING)
					writeData_[i] = true;
			}
			else
				writeData_[i] = true;
		}
	}
	return true;
}

IpcClient::Impl::Impl(std::wstring sName, void (*fcnPtr)(std::wstring, LPVOID), LPVOID lpFunct)
{
	if (sName == L"" || fcnPtr == NULL)
		return;
	object_pointer_ = lpFunct;

//	buffer_[PIPE_BUF_SIZE] = '\0';
	pipe_name_ = sName;
	function_ptr_ = fcnPtr;
	init();
}
IpcClient::Impl::~Impl(void)
{
	terminate();
}
bool IpcClient::Impl::init()
{
	if (isRunning())
		if (!terminate())
			return false;
	ZeroMemory(buffer_, PIPE_BUF_SIZE * sizeof(TCHAR));
//	buffer_[0] = '\0';

	while (1)
	{

		file_ = CreateFile(pipe_name_.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
			NULL);

		if (file_ != INVALID_HANDLE_VALUE)
			break;
		// Exit if an error other than ERROR_PIPE_BUSY occurs. 
		if (GetLastError() != ERROR_PIPE_BUSY)
		{
			file_ = NULL;
			onEvent(PIPE_EVENT_ERROR, L"Failed to open pipe with CreateFile()");
			return false;
		}
		// All pipe instances are busy, so wait for 2 seconds. 
		if (!WaitNamedPipe(pipe_name_.c_str(), 2000))
		{
			file_ = NULL;
			onEvent(PIPE_EVENT_ERROR, L"Failed to open pipe: 2 second wait timed out.");
			return false;
		}
	}
	// Create event handles (incl termination and termination accept event)
	for (int i = 0; i < 3; i++)
	{
		if ((events_[i] = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL)
		{
			CloseHandle(file_); file_ = NULL;
			for (int j = 0; j < i; j++)
			{
				CloseHandle(events_[j]);
				events_[j] = NULL;
			}
			onEvent(PIPE_EVENT_ERROR, L"Failed to CreateEvent()");
			return false;
		}
	}
	ZeroMemory(&ovlap_, sizeof(OVERLAPPED));
	ovlap_.hEvent = events_[0];
	if (ReadFile(file_, buffer_, PIPE_BUF_SIZE, NULL, &ovlap_) == 0)
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			CloseHandle(file_); file_ = NULL;
			CloseHandle(events_[0]); events_[0] = NULL;
			CloseHandle(events_[1]); events_[1] = NULL;
			CloseHandle(events_[2]); events_[2] = NULL;
			onEvent(PIPE_EVENT_ERROR, L"Failed to overlapped ReadFile() when initialising");
			CloseHandle(file_);
			return false;
		}
	}
	else
	{

	}
	std::thread thread_obj(&IpcClient::Impl::workerThread, this);
	thread_obj.detach();
	return true;
}
bool IpcClient::Impl::terminate()
{
	if (!isRunning())
		return false;
	// Signal worker thread to exit
	SetEvent(events_[1]);
	//Wait for the signal when worker thread is actually exiting (max 2s)
	WaitForSingleObject(events_[2], 2000);
	//Close handles
	if (file_)
	{
		CloseHandle(file_);
		file_ = NULL;
	}
	for (int i = 0; i < 3; i++)
	{
		if (events_[i])
		{
			CloseHandle(events_[i]);
			events_[i] = NULL;
		}
	}
	return true;
}
bool IpcClient::Impl::isRunning()
{
	return state_ == PIPE_RUNNING ? TRUE : FALSE;
}
void IpcClient::Impl::workerThread()
{
	DWORD dwRet;
	DWORD dwPipe;
	onEvent(PIPE_RUNNING);
	while (1)
	{
		if ((dwRet = WaitForMultipleObjects(2, events_, FALSE, INFINITE)) == WAIT_FAILED)
		{
			onEvent(PIPE_EVENT_ERROR, L"WaitForMultipleObjects() returned WAIT_FAILED.");
			goto WORKER_THREAD_END;
		}
		dwPipe = dwRet - WAIT_OBJECT_0;
		ResetEvent(events_[dwPipe]);
		// Check if the termination event has been signalled
		if (dwPipe == 1)
		{
			goto WORKER_THREAD_END;;
		}
		// Check overlapped results, and if they fail, reestablish
		// communication; otherwise, process read and write operations
		if (GetOverlappedResult(file_, &ovlap_, &bytes_transferred_, TRUE) == 0)
		{
			if (!disconnectAndReconnect())
			{
				onEvent(PIPE_EVENT_ERROR, L"GetOverlappedResult failed. DisconnectAndReconnect() failed.");
				goto WORKER_THREAD_END;;
			}
			else
				onEvent(PIPE_EVENT_ERROR, L"GetOverlappedResult failed. DisconnectAndReconnect() success.");
		}
		else
		{
			// Check the state of the pipe. If bWriteData equals
			// FALSE, post a read on the pipe for incoming data.			
			if (!write_data_) // PERFORM READ
			{
				ZeroMemory(&ovlap_, sizeof(OVERLAPPED));
				ovlap_.hEvent = events_[0];
				if (ReadFile(file_, buffer_, PIPE_BUF_SIZE, NULL, &ovlap_) == 0)
				{
					if (GetLastError() != ERROR_IO_PENDING)
					{
//						Log(L"ReadFile error. Disconnect and reconnect");
						if (!disconnectAndReconnect())
						{
							onEvent(PIPE_EVENT_ERROR, L"ReadFile() failed. DisconnectAndReconnect() failed.");
							goto WORKER_THREAD_END;;
						}
						else
							onEvent(PIPE_EVENT_ERROR, L"ReadFile() failed. DisconnectAndReconnect() success.");
					}
					else
					{
						onEvent(PIPE_EVENT_READ, std::wstring(buffer_));						
					}
					ZeroMemory(buffer_, PIPE_BUF_SIZE * sizeof(TCHAR));
				}
			}
			else
				onEvent(PIPE_EVENT_SEND);
		}
	}
WORKER_THREAD_END:
	onEvent(PIPE_EVENT_TERMINATE);
	SetEvent(events_[2]);
}

void IpcClient::Impl::onEvent(int nEventID, std::wstring sData)
{
	std::wstring msg;
	switch (nEventID)
	{
	case PIPE_RUNNING:
		state_ = PIPE_RUNNING;
		break;
	case PIPE_EVENT_TERMINATE:
		state_ = PIPE_NOT_RUNNING;
		break;
	case PIPE_EVENT_SEND:
		write_data_ = false;
		break;
	case PIPE_EVENT_READ:
		function_ptr_(sData, object_pointer_); // Send message to external function/callback
		break;
	case PIPE_EVENT_ERROR:
		break;
	default:break;
	}
}

bool IpcClient::Impl::disconnectAndReconnect()
{
	if (CloseHandle(file_) == 0)
	{
		onEvent(PIPE_EVENT_ERROR, L"CloseHandle() failed");
		return false;
	}
	while (1)
	{
		file_ = CreateFile(pipe_name_.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
			NULL);

		if (file_ != INVALID_HANDLE_VALUE)
			break;
		// Exit if an error other than ERROR_PIPE_BUSY occurs. 
		if (GetLastError() != ERROR_PIPE_BUSY)
		{
			onEvent(PIPE_EVENT_ERROR, L"DisconnectAndReconnect() - Failed to open pipe with CreateFile()");
			return false;
		}
		// All pipe instances are busy, so wait for 2 seconds. 
		if (!WaitNamedPipe(pipe_name_.c_str(), 2000))
		{
			onEvent(PIPE_EVENT_ERROR, L"DisconnectAndReconnect() - Failed to open pipe: 2 second wait timed out.");
			return false;
		}
	}
	ZeroMemory(&ovlap_, sizeof(OVERLAPPED));
	ovlap_.hEvent = events_[0];
	if (ReadFile(file_, buffer_, PIPE_BUF_SIZE, NULL, &ovlap_) == 0)
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			CloseHandle(file_); file_ = NULL;
			CloseHandle(events_[0]); events_[0] = NULL;
			CloseHandle(events_[1]); events_[1] = NULL;
			CloseHandle(events_[2]); events_[2] = NULL;
			onEvent(PIPE_EVENT_ERROR, L"Failed to overlapped ReadFile() when initialising");
			CloseHandle(file_);
			return false;
		}
	}
	return true;
}
bool IpcClient::Impl::write(std::wstring& sData)
{
	if (!isRunning())
		if (!init())
			return false;
	ZeroMemory(&ovlap_, sizeof(OVERLAPPED));
	ovlap_.hEvent = events_[0];
	if (WriteFile(file_, sData.c_str(), (DWORD)(sData.size() + 1) * sizeof(TCHAR), NULL, &ovlap_) == 0)
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			onEvent(PIPE_EVENT_ERROR, L"WriteFile() failed");
			return false;
		}
		else
			write_data_ = true;
	}
	else
		write_data_ = true;
	return true;
}
bool IpcServer::send(std::wstring sData, int iPipe)
{
	return pimpl->write(sData, iPipe);
}
bool IpcServer::terminate(void)
{
	return pimpl->terminate();
}
bool IpcClient::send(std::wstring sData)
{
	return pimpl->write(sData);
}
bool IpcClient::terminate(void)
{
	return pimpl->terminate();
}
bool IpcClient::init(void)
{
	return pimpl->init();
}
bool IpcClient::isRunning(void)
{
	return pimpl->isRunning();
}