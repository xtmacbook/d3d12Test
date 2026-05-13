
#include "app.h"
#include <windows.h>
#include <DirectXColors.h>

using namespace DirectX;


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int nCmdShow)
{

    App app(hInstance);

    if (!app.Initialize()) return 0;

    app.Run();
     
    // Run the message loop.

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;

} 