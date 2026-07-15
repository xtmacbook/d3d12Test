#pragma once
#include "common/D3DContext.h"
#include "common/FrameResource.h"

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

struct RenderItem;


class FrameResourceContextInterface 
{
public:

	virtual void Update(const GameTimer& gt, ID3D12Fence*);

	virtual void Draw(const GameTimer& gt, UINT64& CurrentFence, ID3D12Fence* fence, ID3D12CommandQueue*);

	static const int																m_NumFrameResources;

protected:
	virtual void BuildFrameResources() = 0;
	virtual void DrawFrameResource(ID3D12CommandAllocator*) = 0;

protected:

	std::vector< std::unique_ptr<FrameResourceInterface > >							m_frameResources;
	FrameResourceInterface*															m_currFrameResource;
	int																				m_CurrFrameResourceIndex = 0;
};
