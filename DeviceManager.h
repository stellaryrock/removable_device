#pragma once

#include "utils.hpp"

using DevicePtr = std::shared_ptr<devutils::removable_device>;

class DeviceManager
{
public:
	DeviceManager();
	~DeviceManager();


	void AddDevice(TCHAR driveLetter, DevicePtr pDevice);
	void RemoveDevice(TCHAR);

	void Watch(DevicePtr pDevice);

private:

	std::map<TCHAR, DevicePtr> m_devices;

};

