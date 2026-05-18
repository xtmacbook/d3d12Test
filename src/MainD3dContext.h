
#include "common/D3DContext.h"

class MainD3DContext :public D3DContext
{
public:

	virtual void Update(const GameTimer& gt);
	virtual void Draw(const GameTimer& gt);
};