#include "MainApp.h"
#include "common/D3DContext.h"
MainApp::MainApp(HINSTANCE hInstance, D3DContext* context)
	:App(hInstance,context)
{
}

bool MainApp::Initialize()
{
	if (!App::Initialize())
		return false;

	return true;
}

void MainApp::Update(const GameTimer& gt)
{
	App::Update(gt);
}

void MainApp::Draw(const GameTimer& gt)
{
	App::Draw(gt);
}
