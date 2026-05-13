
#include "common/app.h"
#include "common/D3DContext.h"

#include <windows.h>
#include <DirectXColors.h>

using namespace DirectX;


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int nCmdShow)
{
    D3DContext d3dContext;

    App app(hInstance,&d3dContext);

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