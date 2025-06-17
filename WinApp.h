#pragma once
#include "DeviceManager.h"

#define MAX_STRING 100

class WinApp
{
public :
	WinApp();
	~WinApp();

	bool Init();
	void Run();
	void Clear();
	void Dispatch();
	void Join();


	std::unique_ptr<DeviceManager>& GetDeviceManager() { return m_deviceMan; }

private:

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
	BOOL InitInstance();
	BOOL InitWinClass();
	BOOL InitWindow();
	BOOL InitNotification();

	
	void HandleDeviceChange(WPARAM wParam, LPARAM lParam);

private:
	HWND m_hWnd = NULL;
	HINSTANCE m_hInstance = NULL;
	WNDCLASS m_cWnd = { 0 };

	BOOL m_bRunning = FALSE;

	std::vector<std::thread> m_threads;
	std::unique_ptr<DeviceManager> m_deviceMan = nullptr;
};

