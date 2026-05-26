
#pragma once

#include "MainApp.h"
#include "FrameResourceContext.h"
#include "Geometry.h"
#include "FrameResource.h"

using namespace DirectX;


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int nCmdShow)
{
	FrameResourceContext d3dContext;

	MainApp app(hInstance, &d3dContext);

	if (!app.Initialize()) return 0;

	app.Run();
	return 0;
}