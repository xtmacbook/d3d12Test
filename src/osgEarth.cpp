
#include "MainApp.h"
#include "MainD3dContext.h"

#include <windows.h>
#include <DirectXColors.h>

using namespace DirectX;


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int nCmdShow)
{
	MainD3DContext d3dContext;

	MainApp app(hInstance, &d3dContext);

	if (!app.Initialize()) return 0;

	app.Run();
	return 0;
}