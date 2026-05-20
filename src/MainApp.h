
#include "common/App.h"

class D3DContext;

class MainApp : public App
{
public:
	MainApp(HINSTANCE hInstance, D3DContext*);

	virtual bool Initialize();

	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
};