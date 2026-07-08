#pragma once
#include "common/D3DContext.h"
#include "common/FrameResource.h"
#include "common/BufferStruct.h"


#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

struct RenderItem;

class FrameResourceContextInterface : public D3DContext
{
public:

	virtual void Update(const GameTimer& gt);

	virtual void Draw(const GameTimer& gt);

	static const int																m_NumFrameResources;

protected:
	virtual void BuildFrameResources() = 0;
	virtual void DrawFrameResource(ID3D12CommandAllocator*) = 0;

protected:

	std::vector< std::unique_ptr<FrameResourceInterface > >							m_frameResources;
	FrameResourceInterface*															m_currFrameResource;
	int																				m_CurrFrameResourceIndex = 0;
};
