
#pragma once

#include "MainApp.h"
#include "common/Geometry.h"
#include "common/FrameResource.h"
#include "context/WavesContext.h"

using namespace DirectX;


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int nCmdShow)
{
	WavesContext d3dContext;

	MainApp app(hInstance, &d3dContext);

	if (!app.Initialize()) return 0;

	app.Run();
	return 0;
}