#include "pch.h"
#include "fileutils.hpp"
#define MAX_BUFFER 1024
using namespace fileutils;

namespace devutils
{
    // 장치 인식 및 장치 정보를 WinAPI 를 통해서 가져오는 헬퍼 함수
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

        // 생성자
        removable_device(char driveLetter) : driveLetter(driveLetter)
        {
            basePath = std::wstring(1, driveLetter) + L":\\";
            init();
        }

        // 소멸자
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

        // 디렉토리 내 변경사항 발생 시 비동기 직업을 위해 등록
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

            // 파일 생성 및 삭제 이벤트를 처리합니다.
            // 파일 복사 감지는 다른 방법으로,,,
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

        // 적합한 nIndex 를 찾을 때까지 반복하여 검색합니다.
        while (!GetDeviceDetail(hDevInfoSet, nIndex++, pDevice, devData)) {}
        pDevice->nInterfaceIndex = nIndex;

        return TRUE;
    }

    inline void GetDeviceInstance(struct removable_device* pDevice)
    {
        // 상위 디바이스 노드 탐색
        CM_Get_Parent(&pDevice->deviceInstance, pDevice->pData->DevInst, 0);
    }

    inline void GetDeviceId(struct removable_device* pDevice)
    {
        wchar_t buff[MAX_BUFFER];
        CM_Get_Device_ID(pDevice->deviceInstance, buff, MAX_BUFFER, 0);

        pDevice->deviceID = std::wstring(buff);
    }

    // 디스크 드라이브 파티션 문자를 토대로 STORAGE_DEVICE_NUMBER 구조체를 만들고
    // 해당 드라이브 문자의 PhysicalDrive 번호와 일치하는 장치 인터페이스의 정보를 가져옵니다.
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
        
        // SetupDiGetDeviceInterfaceDetail 함수를 두번 호출해야 합니다.
        // https://learn.microsoft.com/ko-kr/windows/win32/api/setupapi/nf-setupapi-setupdigetdeviceinterfacedetailw#remarks
        // 할당할 메모리 크기를 얻기 위한 첫 번째 호출.
        DWORD cbDetail = 0;
        SetupDiGetDeviceInterfaceDetail(hInfoSet, &ifaceData, NULL, 0, OUT &cbDetail, NULL);

        PSP_DEVICE_INTERFACE_DETAIL_DATA pDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(cbDetail);
        pDevice->pDetail = pDetail;
        pDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        // 실제 데이터를 저장할 두 번째 호출.
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