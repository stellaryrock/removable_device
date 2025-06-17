#pragma once
class IocpManager
{
public:
	IocpManager() {}
	~IocpManager() {}

	static void Init();

	static void Dispatch();
	static void Register(HANDLE hFile);

private:
	static HANDLE hIocp;
};

