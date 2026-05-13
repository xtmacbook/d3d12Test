#include <windows.h>

class App 
{
public:

	App(HINSTANCE hInstance);

	~App();

	virtual bool Initialize();

	int Run();
	
private:
	HINSTANCE m_hInstance;
};