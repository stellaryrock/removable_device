#include "pch.h"
#include "DeviceManager.h"

DeviceManager::DeviceManager()
{

}
DeviceManager::~DeviceManager()
{
	m_devices.clear();
}

void DeviceManager::AddDevice(TCHAR driveLetter, DevicePtr pDevice)
{
	std::pair<TCHAR, DevicePtr> pair(driveLetter, pDevice);
	m_devices.insert(pair);
}


void DeviceManager::RemoveDevice(TCHAR driveLetter)
{
	m_devices.erase(driveLetter);
}

void DeviceManager::Watch(DevicePtr pDevice)
{
	DWORD dwNotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
		FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
		FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;

	DWORD cbBytes = 0;

	BOOL bRet = ReadDirectoryChangesW(
		pDevice->hDeviceOverlapped,
		&pDevice->tNotification, 
		sizeof(pDevice->tNotification), 
		TRUE, dwNotifyFilter,
		&cbBytes, 
		(LPOVERLAPPED) &pDevice,
		NULL);

	if (bRet == 0)
	{
		std::cerr << "[DeviceManager::Watch]ReadDirectoryChagesW Failed: " << GetLastError() << std::endl;
	}
}
