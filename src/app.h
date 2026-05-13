#include <windows.h>

class App 
{
public:

	App(HINSTANCE hInstance);

	~App();

	static App* GetApp();

	HINSTANCE AppInst()const;
	HWND      MainWnd()const;


	virtual bool Initialize();

	int Run();
	

	bool InitWindow();

	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	 

protected:

	static App* g_app ;

	int m_width = 800;
	int m_height = 600;
private:
	HINSTANCE	m_hInstance;
	HWND		m_hWindow;
};