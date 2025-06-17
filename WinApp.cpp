#include "pch.h"
#include "IocpManager.h"
#include "WinApp.h"


WinApp::WinApp()
{
    IocpManager::Init();

    m_deviceMan = std::make_unique<DeviceManager>();
}

WinApp::~WinApp()
{
    Clear();
}

/// <summary>
/// Win32 ���ø����̼� ���� �ʱ�ȭ �� ������ ����
/// </summary>
/// <returns></returns>
bool WinApp::Init()
{
    if(!InitInstance())
    {
        return FALSE;
    }

    if(!InitWinClass()) 
    {
        return FALSE;
    }

    if(!InitWindow())
    {
        return FALSE;
    }

    if(!InitNotification())
    {
        return FALSE;
    }

    return TRUE;
}

void WinApp::Run()
{
    m_bRunning = TRUE;

    while (m_bRunning)
    {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}


void WinApp::Dispatch()
{
    // �񵿱� �۾� ó���� ���� ������ ����
    m_threads.emplace_back(
        []() {
            while (true) {
                IocpManager::Dispatch();
            }
        }
    );
}

/// <summary>
///  ��ġ ����� ������ �޽����� �� �����忡 �������� OS�� ��û�մϴ�.
/// </summary>
/// <returns></returns>
BOOL WinApp::InitNotification()
{
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter = {};
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    HDEVNOTIFY hDevNotify = RegisterDeviceNotification(
        m_hWnd,
        &NotificationFilter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );

    if (hDevNotify == NULL)
    {
        std::cerr << "[WinApp::InitNotification]RegisterDeviceNotification Failed" << std::endl;
        return FALSE;
    }

    return TRUE;
}



void WinApp::Clear()
{
    // unique_ptr ����
    GetDeviceManager().reset();
}

void WinApp::Join()
{
    for (auto& t : m_threads)
    {
        t.join();
    }
}

BOOL WinApp::InitInstance()
{
    m_hInstance = GetModuleHandle(nullptr);
    if (!m_hInstance)
    {
        std::cerr << "[WinApp::Init]GetModuleHandle Failed" << std::endl;
        return FALSE;
    }

    return TRUE;
}

BOOL WinApp::InitWinClass()
{
    m_cWnd.lpfnWndProc = WinApp::WndProc;
    m_cWnd.hInstance = m_hInstance;
    m_cWnd.lpszClassName = L"usb_detector";

    return RegisterClass(&m_cWnd);
}

BOOL WinApp::InitWindow()
{
    m_hWnd = CreateWindowEx(
        0, 
        L"usb_detector", 
        L"USBMonitorWindow", 
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 300,
        nullptr, nullptr, m_hInstance, this);

    if (!m_hWnd) {
        std::cerr << "[WinApp::Init]CreateWindowsEx Failed" << std::endl;
        return FALSE;
    }

    return TRUE;
}

LRESULT WinApp::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WinApp* pThis = nullptr;

    if (msg == WM_NCCREATE)
    {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = static_cast<WinApp*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<WinApp*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        return pThis->HandleMessage(hWnd, msg, wParam, lParam);
    }
}

LRESULT WinApp::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    case WM_DEVICECHANGE:
        HandleDeviceChange(wParam, lParam);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    return 0;
}

void WinApp::HandleDeviceChange(WPARAM wParam, LPARAM lParam)
{
    PDEV_BROADCAST_HDR hdr = (PDEV_BROADCAST_HDR)lParam;
    PDEV_BROADCAST_VOLUME vol = (PDEV_BROADCAST_VOLUME)hdr;

    if (vol == nullptr) return;

    // ��ũ ����̺� ���ڸ� �����ɴϴ�.
    TCHAR driveLetter = devutils::GetDriveLetter(vol->dbcv_unitmask);

    // ��ġ ���� �޽����� ó���մϴ�.
    if (wParam == DBT_DEVICEARRIVAL) 
    {
        if (hdr->dbch_devicetype == DBT_DEVTYP_VOLUME && devutils::IsRemovableDevice(driveLetter) )
        {
            // ��ũ ����̺� ���ڸ� ���� ��ġ ������ �����ͼ� ����ü�� �����մϴ�.
            DevicePtr dev = std::make_shared<devutils::removable_device>(driveLetter);
            GetDeviceManager()->AddDevice(driveLetter, dev);

            // JSON ���·� �α׸� ����մϴ�.
            nlohmann::json obj = dev->toJson(dev->deviceType + L" Connected");
            obj["timestamp"] = std::time(nullptr);
            appendLog(L"C:\\Logs\\dlp_device_access.log", wconvert( obj.dump(4) ));

            std::cout << obj.dump(4) << std::endl;

            // ���丮 ��������� �����ϵ��� ����մϴ�.
            IocpManager::Register(dev->hTest);
            dev->register_watch();
        }
    }
    // ��ġ ���� �޽����� ó���մϴ�.
    else if (wParam == DBT_DEVICEREMOVECOMPLETE) 
    {
        std::wcout << L"[DEVICECHANGE] Removed" << std::endl;
        GetDeviceManager()->RemoveDevice(driveLetter);
    }
}