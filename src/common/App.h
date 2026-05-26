#pragma once

#include <windows.h>
#include <DirectXMath.h>
#include "GameTimer.h"

class D3DContext;

class App 
{
public:

	App(HINSTANCE hInstance, D3DContext*);

	~App();

	static App* GetApp();

	HINSTANCE AppInst()const;
	HWND      MainWnd()const;

	float       AspectRatio()const;

	int			Width()const;
	int			Height()const;

	virtual bool Initialize();

	int Run();

	bool InitWindow();

	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void CalculateFrameStats();

	void GetViewState(float& theta, float& phi, float& radius);

protected:
	virtual void OnMouseDown(WPARAM btnState, int x, int y);
	virtual void OnMouseUp(WPARAM btnState, int x, int y);
	virtual void OnMouseMove(WPARAM btnState, int x, int y);
	virtual void OnResize();
	void OnKeyboardInput(const GameTimer& gt);

	virtual void Update(const GameTimer& gt);
	virtual void Draw(const GameTimer& gt);

protected:

	static App*					g_app ;

	D3DContext*					m_d3d = nullptr;
	HINSTANCE					m_hInstance;
	HWND						m_hWindow;

	int							m_ClientWidth = 800;
	int							m_ClientHeight = 600;

	bool						m_AppPaused = false;  // is the application paused?
	bool						m_Minimized = false;  // is the application minimized?
	bool						m_Maximized = false;  // is the application maximized?
	bool						m_Resizing = false;   // are the resize bars being dragged?
	bool						m_FullscreenState = false;// fullscreen enabled

	GameTimer					m_Timer;

	float						m_Theta = 1.5f * DirectX::XM_PI;
	float						m_Phi = DirectX::XM_PIDIV4;
	float						m_Radius = 5.0f;
	POINT						m_LastMousePos;


};