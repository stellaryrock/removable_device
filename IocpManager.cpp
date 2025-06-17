#include "pch.h"
#include "utils.hpp"
#include "IocpManager.h"

HANDLE IocpManager::hIocp = NULL;

void IocpManager::Init()
{
	hIocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hIocp == INVALID_HANDLE_VALUE)
	{
		std::cerr << "[IocpManager::Init()]CreateIoCompletionPort Failed" << std::endl;
	}
}

void IocpManager::Dispatch()
{
	DWORD cbBytes = 0;
	ULONG_PTR key = 0;
	LPOVERLAPPED ol = nullptr;

	if (::GetQueuedCompletionStatus(hIocp, &cbBytes, &key, &ol, 1000 * 3))
	{
		devutils::removable_device* dev = reinterpret_cast<devutils::removable_device*>(ol);
		
		dev->process_watch();
		dev->register_watch();
	}
	else
	{
		std::cerr << "GQCS Failed :" << GetLastError() << std::endl;
	}
}

void IocpManager::Register(HANDLE hFile)
{
	::CreateIoCompletionPort(hFile, hIocp, 0, 0);
}
