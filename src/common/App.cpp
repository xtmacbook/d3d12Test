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
	if (m_d3d && m_d3d->InitDirect3D())
	{
		return true;

	}
	return false;
}

int App::Run()
{
	return 0;
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

void App::OnResize()
{
	if (m_d3d) m_d3d->OnResize();
}
