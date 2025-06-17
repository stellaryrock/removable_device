#pragma once
#include "pch.h"

namespace fileutils {
	inline std::vector<std::wstring> split(std::wstring input, std::wstring delimiter)
	{
		std::vector<std::wstring> ret;
		int start = 0;
		int end = 0;

		while ((end = input.find(delimiter, start)) != std::wstring::npos)
		{
			ret.push_back( input.substr(start, end - start) );
			start = end + 1;
		}

		ret.push_back(input.substr(start, input.length()));

		return ret;
	}


	inline std::string convert(std::wstring input)
	{
		std::string ret = "";

		ret.assign(input.begin(), input.end());

		return ret;
	}

	inline std::wstring wconvert(std::string input)
	{
		std::wstring ret = L"";
		
		ret.assign(input.begin(), input.end());

		return ret;
	}


	inline bool appendLog(const std::wstring sFilename, const std::wstring sLog)
	{
		HANDLE hFile = ::CreateFile(
			sFilename.c_str(), 
			FILE_APPEND_DATA, 
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hFile == INVALID_HANDLE_VALUE)
		{
			std::cerr << "[stringutils]CreateFile Failed" << std::endl;

			DWORD err = GetLastError();

			return FALSE;
		}
		
		DWORD dwHead = ::SetFilePointer(hFile, 0, NULL, FILE_END);
		if (dwHead == INVALID_SET_FILE_POINTER) {
			std::cerr << "[stringutils]SetFilePointer Failed" << std::endl;
			return FALSE;
		}


		DWORD dwBytes = 0;
		if (::WriteFile(
			hFile,
			sLog.c_str(),
			sLog.size(),
			&dwBytes,
			NULL
		) == FALSE) {
			std::cerr << "[stringutils]WriteFile Failed" << std::endl;

			CloseHandle(hFile);
			return FALSE;
		}

		CloseHandle(hFile);
		return TRUE;
	}
}
