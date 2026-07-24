
#pragma once

#include "MainApp.h"
#include "common/Geometry.h"
#include "common/FrameResource.h"

#include "context/MainD3dContext.h"
#include "context/TestFrameResourceContext.h"
#include "context/WavesContext.h"
#include "context/TexContext.h"
#include "context/BlendContext.h"
#include "context//MirrorWithStencil.h"
#include "context/GSContext.h"
#include "context/ComputeShader.h"
#include "context/TessllationContext.h"
#include "context/InstanceContext.h"
#include "context/CubeMapContext.h"
#include "shadow/ShadowMap.h"
using namespace DirectX;



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int nCmdShow)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		ShadowMapContext d3dContext;
		
		MainApp app(hInstance, &d3dContext);

		app.setRadius(12.0f);
		app.setPixelUnitScale(0.2, 0.2);
		app.setSceneClampRange(5.0f, 150.f);
		
		if (!app.Initialize()) return 0;
		
		app.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.toString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
 
	return 0;
}
 