#ifndef PCH_H
#define PCH_H
#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <dbt.h>
#include <initguid.h>
#include <usbiodef.h>
#include <SetupAPI.h>
#include <cfgmgr32.h>

#include <regex>
#include <tchar.h>
#include <thread>
#include <memory>
#include <functional>

#include <vector>
#include <map>

#include <nlohmann/json.hpp>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")


#define MAX_STRING 100
#endif //PCH_H
