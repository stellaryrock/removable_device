#include "pch.h"
#include "WinApp.h"

// Main Entry 
int main() 
{
    // 한글 출력을 위한 코드
    // 콘솔 출력 코드 페이지 설정 (CP949)
    SetConsoleOutputCP(949);
    std::wcout.imbue(std::locale(""));

    // Win32 애플리케이션
    WinApp app;

    app.Init();
    app.Dispatch();
    app.Run();
    app.Join();

    return 0;
}
