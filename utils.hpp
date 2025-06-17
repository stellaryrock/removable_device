#include "pch.h"
#include "fileutils.hpp"
#define MAX_BUFFER 1024
using namespace fileutils;

namespace devutils
{
    // ��ġ �ν� �� ��ġ ������ WinAPI �� ���ؼ� �������� ���� �Լ�
    void GetDeviceInfo(char driveLetter, OUT struct removable_device* pDev);
    TCHAR GetDriveLetter(DWORD dMask);
    bool IsRemovableDevice(TCHAR driveLetter);
    HANDLE OpenDevice(const std::wstring& path);
    HANDLE OpenDeviceOverlapped(const std::wstring& path);
    bool GetDeviceNumber(HANDLE hDevice, OUT STORAGE_DEVICE_NUMBER* pDevNumber);
    void GetDeviceInstance(struct removable_device* pDev);
    void GetDeviceId(struct removable_device* pDev);
    bool GetDeviceInfoFromInterfaceList(struct removable_device* pDevice);
    bool GetDeviceDetail(
        IN HDEVINFO hInfoSet, 
        IN DWORD index, 
        OUT removable_device* pDevice,
        OUT PSP_DEVINFO_DATA pData
    );

    struct removable_device 
    {
        OVERLAPPED ol = { 0 };
        char driveLetter;
        DWORD nInterfaceIndex = 0;
        
        STORAGE_DEVICE_NUMBER deviceNumber;

        PSP_DEVINFO_DATA pData = nullptr;
        PSP_DEVICE_INTERFACE_DETAIL_DATA pDetail = nullptr;
        FILE_NOTIFY_INFORMATION tNotification[MAX_BUFFER];

        DEVINST deviceInstance = 0;

        std::wstring    devicePath;
        std::wstring    deviceType;
        std::wstring    basePath;
        std::wstring    drivePath;
        std::wstring    deviceID;
        std::wstring	vid;
        std::wstring    pid;
        std::wstring    serial;
        std::wstring    timestamp;

        ULONGLONG       capacity = 0;

        HANDLE hDevice = NULL;
        HANDLE hPhysicalDevice = NULL;
        HANDLE hDeviceOverlapped = NULL;

        // TEST
        /*HANDLE hTest = CreateFile(L"C:\\Sources", 
            FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 
            NULL, 
            OPEN_EXISTING, 
            FILE_FLAG_OVERLAPPED | FILE_FLAG_BACKUP_SEMANTICS, 
            0);*/

        // ������
        removable_device(char driveLetter) : driveLetter(driveLetter)
        {
            basePath = std::wstring(1, driveLetter) + L":\\";
            init();
        }

        // �Ҹ���
        ~removable_device()
        {
            free(pData);
            free(pDetail);

            CloseHandle(hDevice);
            CloseHandle(hPhysicalDevice);
        }

        BOOL init()
        {
            pData = new SP_DEVINFO_DATA;
            if (pData == nullptr)
            {
                std::cerr << "[init]Heap Alloc Failed" << std::endl;
            }

            ::ZeroMemory(pData, sizeof(SP_DEVINFO_DATA));
            pData->cbSize = sizeof(SP_DEVINFO_DATA);
            
            devutils::GetDeviceInfo(driveLetter, this);
            devutils::GetDeviceInstance(this);
            devutils::GetDeviceId(this);

            hDeviceOverlapped = devutils::OpenDeviceOverlapped(basePath);

            timestamp = std::to_wstring(std::time(nullptr));

            parseInfo();

            return TRUE;
        }

        void parseInfo()
        {
           std::vector<std::wstring> tokens = split(deviceID, L"\\");
           deviceType = tokens[0];

           std::wstring vpid = tokens[1];
           std::vector<std::wstring> vpid_tokens = split(vpid, L"&");
           vid = vpid_tokens[0];
           pid = vpid_tokens[1];

           std::vector<std::wstring> vid_tokens = split(vid, L"_");
           vid = L"0x" + vid_tokens[1];

           std::vector<std::wstring> pid_tokens = split(pid, L"_");
           pid = L"0x" + pid_tokens[1];

        }

        nlohmann::json toJson(std::wstring sEvent)
        {
            nlohmann::json ret;

            ret["event"] = convert(sEvent);
            ret["device"] = convert(pDetail->DevicePath);
            ret["vid"] = convert(vid);
            ret["pid"] = convert(pid);
            ret["drive"] = convert( basePath );

            return ret;
        }

        // ���丮 �� ������� �߻� �� �񵿱� ������ ���� ���
        void register_watch()
        {
            DWORD cbBytes = 0;
            DWORD dwNotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION;

            BOOL bRet = ReadDirectoryChangesW(
                hDeviceOverlapped,
                &tNotification,
                sizeof(tNotification),
                TRUE,
                dwNotifyFilter,
                &cbBytes,
                (LPOVERLAPPED)&ol,
                NULL);

            if (bRet == 0)
            {
                std::cerr << "[register_watch]ReadDirectoryChagesW Failed: " << GetLastError() << std::endl;
            }
        }

        void process_watch()
        {
            for (int idx = 0; idx < MAX_BUFFER; idx++)
            {
                ::Sleep(1);

                FILE_NOTIFY_INFORMATION* tInfo = &tNotification[idx];
                
                check_notify_info(tInfo);
            }
        }

        void check_notify_info(FILE_NOTIFY_INFORMATION* tInfo)
        {
            DWORD Action = tInfo->Action;
            if (Action < 0 || Action >4) return;

            std::wstring sPath(tInfo->FileName, tInfo->FileNameLength / sizeof(TCHAR));
            HANDLE hFile = CreateFile(
                (basePath + sPath).c_str(), 
                FILE_READ_ACCESS, 
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL, 
                OPEN_EXISTING, 
                FILE_FLAG_BACKUP_SEMANTICS, 
                NULL);

            if (GetLastError() != ERROR_SUCCESS)
            {
                std::cerr << "[check_notify_info]CreateFile Failed" << std::endl;
            }

            DWORD dwFileSizeHigh = 0;
            DWORD dwFileSize = ::GetFileSize(hFile, &dwFileSizeHigh);

            CloseHandle(hFile);

            // ���� ���� �� ���� �̺�Ʈ�� ó���մϴ�.
            // ���� ���� ������ �ٸ� �������,,,
            nlohmann::json obj;
            std::vector<std::wstring> filename_token = split(sPath, L".");

            switch (Action)
            {
            case FILE_ACTION_ADDED:
                obj = toJson(L"Create");

                obj["filename"] = convert(sPath);
                obj["extension"] = convert(filename_token.back());
                obj["timestamp"] = std::time(nullptr);

                appendLog(L"C:\\Logs\\dlp_file_action.log", wconvert( obj.dump(4) ) );

                break;
                
            case FILE_ACTION_REMOVED:
                obj = toJson(L"Delete");

                obj["filename"] = convert(sPath);
                obj["extension"] = convert(filename_token.back());
                obj["timestamp"] = std::time(nullptr);

                std::cout << obj.dump(4) << std::endl;

                appendLog(L"C:\\Logs\\dlp_file_action.log", wconvert(obj.dump(4)));

                break;
            }
        }
    };

	inline TCHAR GetDriveLetter(DWORD dMask)
	{
        TCHAR letter = 'A';

		while (dMask > 1)
		{
			dMask >>= 1;
			letter += 1;
		}

		return letter;
	}


	inline bool IsRemovableDevice(TCHAR driveLetter)
	{
        TCHAR path[5] = L"X:\\";
        path[0] = driveLetter;

		UINT type = ::GetDriveType(path);
		if (type == DRIVE_REMOVABLE)
		{
			std::cout << "Removable device connected" << std::endl;
			return TRUE;
		}

		return FALSE;
	}

    inline HANDLE OpenDevice(const std::wstring& path)
    {
        HANDLE hVolume = ::CreateFile(path.c_str(), NULL,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

        if (hVolume == INVALID_HANDLE_VALUE) {
            std::wcout << L"Failed to open device: " << path << std::endl;
            return INVALID_HANDLE_VALUE;
        }

        return hVolume;
    }

    inline HANDLE OpenDeviceOverlapped(const std::wstring& path)
    {
        HANDLE hVolume = ::CreateFile(path.c_str(), FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
            OPEN_EXISTING, 
            FILE_FLAG_OVERLAPPED | FILE_FLAG_BACKUP_SEMANTICS, 
            NULL);

        if (hVolume == INVALID_HANDLE_VALUE) 
        {
            std::wcout << L"Failed to open device: " << path << std::endl;
            return INVALID_HANDLE_VALUE;
        }

        return hVolume;
    }

    inline bool GetDeviceNumber(HANDLE hDevice, OUT STORAGE_DEVICE_NUMBER* pDevNumber)
    {
        DWORD bytesReturned;

        if (!DeviceIoControl(hDevice, IOCTL_STORAGE_GET_DEVICE_NUMBER,
            NULL, 0, pDevNumber, sizeof(*pDevNumber), &bytesReturned, NULL)) 
        {
            std::wcout << L"[GetDevNumber]DeviceIoControl failed." << std::endl;

            return FALSE;
        }

        return TRUE;
    }

    inline bool GetDeviceInfoFromInterfaceList(struct removable_device* pDevice)
    {
        HDEVINFO hDevInfoSet = SetupDiGetClassDevs(
            &GUID_DEVINTERFACE_DISK, 
            NULL, NULL,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
        );

        if (hDevInfoSet == INVALID_HANDLE_VALUE)
        {
            std::wcout << L"[FindDeviceFromInterfaceList]SetupDiGetClassDevs failed.\n";
            return FALSE;
        }

        PSP_DEVINFO_DATA devData = pDevice->pData;
        DWORD nIndex = 0;

        // ������ nIndex �� ã�� ������ �ݺ��Ͽ� �˻��մϴ�.
        while (!GetDeviceDetail(hDevInfoSet, nIndex++, pDevice, devData)) {}
        pDevice->nInterfaceIndex = nIndex;

        return TRUE;
    }

    inline void GetDeviceInstance(struct removable_device* pDevice)
    {
        // ���� ����̽� ��� Ž��
        CM_Get_Parent(&pDevice->deviceInstance, pDevice->pData->DevInst, 0);
    }

    inline void GetDeviceId(struct removable_device* pDevice)
    {
        wchar_t buff[MAX_BUFFER];
        CM_Get_Device_ID(pDevice->deviceInstance, buff, MAX_BUFFER, 0);

        pDevice->deviceID = std::wstring(buff);
    }

    // ��ũ ����̺� ��Ƽ�� ���ڸ� ���� STORAGE_DEVICE_NUMBER ����ü�� �����
    // �ش� ����̺� ������ PhysicalDrive ��ȣ�� ��ġ�ϴ� ��ġ �������̽��� ������ �����ɴϴ�.
    inline void GetDeviceInfo(char driveLetter, OUT struct removable_device* pDevice)
    {
       std::wstring path = L"\\\\.\\X:";
       path[4] = driveLetter;
       pDevice->drivePath = path;

       pDevice->hDevice = OpenDevice(path);

       STORAGE_DEVICE_NUMBER devNumber = { 0 };
       GetDeviceNumber(pDevice->hDevice, &devNumber);
       pDevice->deviceNumber = devNumber;

       GetDeviceInfoFromInterfaceList(pDevice);
    }

    inline bool GetDeviceDetail(
        IN HDEVINFO hInfoSet, 
        IN DWORD index, 
        OUT removable_device* pDevice, 
        OUT PSP_DEVINFO_DATA pData = NULL)
    {
        SP_DEVICE_INTERFACE_DATA ifaceData = { 0 };
        ifaceData.cbSize = sizeof(ifaceData);

        if (!SetupDiEnumDeviceInterfaces(hInfoSet, NULL, &GUID_DEVINTERFACE_DISK, index, &ifaceData))
        {
            return FALSE;
        }
        
        // SetupDiGetDeviceInterfaceDetail �Լ��� �ι� ȣ���ؾ� �մϴ�.
        // https://learn.microsoft.com/ko-kr/windows/win32/api/setupapi/nf-setupapi-setupdigetdeviceinterfacedetailw#remarks
        // �Ҵ��� �޸� ũ�⸦ ��� ���� ù ��° ȣ��.
        DWORD cbDetail = 0;
        SetupDiGetDeviceInterfaceDetail(hInfoSet, &ifaceData, NULL, 0, OUT &cbDetail, NULL);

        PSP_DEVICE_INTERFACE_DETAIL_DATA pDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(cbDetail);
        pDevice->pDetail = pDetail;
        pDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        // ���� �����͸� ������ �� ��° ȣ��.
        if (!SetupDiGetDeviceInterfaceDetail(hInfoSet, &ifaceData, pDetail, cbDetail, NULL, pData))
        {
            std::cerr << index << ": SetupDiGetDeviceInterfaceDetail Failed" << std::endl;
            std::cerr << "ERROR: " << GetLastError() << std::endl;

            free(pDetail);

            return FALSE;
        }

        HANDLE hDevice = OpenDevice(pDetail->DevicePath);
        if (hDevice == INVALID_HANDLE_VALUE)
        {
            std::cerr << "[GetDeviceDetail]OpenDevice(DevicePath) Failed" << std::endl;
            free(pDetail);

            return FALSE;
        }
            
        STORAGE_DEVICE_NUMBER storDeviceNumber = { 0 };
        GetDeviceNumber(hDevice, &storDeviceNumber);

        if (pDevice->deviceNumber.DeviceNumber != storDeviceNumber.DeviceNumber)
        {
            std::cerr << "[GetDeviceDetail]pDevice->deviceNumber != storDeviceNumber.DeviceNumber" << std::endl;
            free(pDetail);
            CloseHandle(hDevice);

            return FALSE;
        }

        pDevice->pDetail = pDetail;
        pDevice->devicePath = pDetail->DevicePath;
        pDevice->hPhysicalDevice = hDevice;

        return TRUE;
    }
}