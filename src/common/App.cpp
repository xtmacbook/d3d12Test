#include "app.h"
#include "D3DContext.h"

#include <windowsx.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return App::GetApp()->MsgProc(hwnd, uMsg, wParam, lParam);
}

App* App::g_app = nullptr;

App::App(HINSTANCE hInstance, D3DContext*context) :
	m_hInstance(hInstance),
	m_d3d(context)
{
	g_app = this;
	context->setApp(this);
}

App::~App()
{
	if (m_d3d)
	{
		delete m_d3d;
		m_d3d = nullptr;
	}
}

App* App::GetApp()
{
	return g_app;
}

HINSTANCE App::AppInst() const
{
	return m_hInstance;
}

HWND App::MainWnd() const
{
	return m_hWindow;
}

int App::Width() const
{
	return m_ClientWidth;
}

int App::Height() const
{
	return m_ClientHeight;
}

bool App::Initialize()
{
	if (!InitWindow()) return false;
	
	if (m_d3d)
	{
		if (!m_d3d->InitDirect3D())
			return false;
	}

	OnResize();

	return true;
}

int App::Run()
{
	m_Timer.Reset();

	MSG msg = { };

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			m_Timer.Tick();

			CalculateFrameStats();
			Update(m_Timer);
			Draw(m_Timer);
		}
	}

	return (int)msg.wParam;
}

bool App::InitWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = m_hInstance;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	RegisterClass(&wc);

	m_hWindow = CreateWindow(
		L"MainWnd",
		L"osgEarth",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		m_ClientWidth,
		m_ClientHeight,
		0,
		0,
		m_hInstance,
		0);

	if (m_hWindow == NULL)
	{
		return false;
	}

	ShowWindow(m_hWindow, SW_SHOW);
	UpdateWindow(m_hWindow);

	return true;
}

LRESULT App::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			// All painting occurs here, between BeginPaint and EndPaint.

			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

			EndPaint(hwnd, &ps);
		}
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
		{
			OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		}
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		{
			OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		}
		case WM_MOUSEMOVE:
		{
			OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			return 0;
		}
		case WM_SIZE:
		{
			m_ClientWidth = LOWORD(lParam);
			m_ClientHeight = HIWORD(lParam);
			return 0;
		}
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void App::CalculateFrameStats()
{
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if ((m_Timer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mspfStr = std::to_wstring(mspf);

		std::wstring windowText =
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr;

		SetWindowText(m_hWindow, windowText.c_str());

		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

void App::OnResize()
{
	if (m_d3d) m_d3d->OnResize();
}
