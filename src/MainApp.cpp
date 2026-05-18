#include "MainApp.h"
#include "common/D3DContext.h"
MainApp::MainApp(HINSTANCE hInstance, D3DContext* context)
	:App(hInstance,context)
{
}

void MainApp::Update(const GameTimer& gt)
{
	if (m_d3d) m_d3d->Update(gt);
}

void MainApp::Draw(const GameTimer& gt)
{
	if (m_d3d) m_d3d->Draw(gt);
}
