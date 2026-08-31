#include "FrameResourceContextInterface.h"

#include "common/FrameResource.h"
#include "common/GeometryGenerator.h"
#include "common/Geometry.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

const int	FrameResourceContextInterface::m_NumFrameResources = 3;

void FrameResourceContextInterface::Update(const GameTimer& gt, ID3D12Fence* fence)
{

	// Cycle through the circular frame resource array.
	m_CurrFrameResourceIndex = (m_CurrFrameResourceIndex + 1) %
		m_NumFrameResources;

	m_currFrameResource = m_frameResources[m_CurrFrameResourceIndex].get();

	/* Has the GPU finished processing the commands of the current frame
	 resource. If not, wait until the GPU has completed commands up to
	 this fence point.*/

	if (m_currFrameResource->m_Fence != 0 &&
		fence->GetCompletedValue() < m_currFrameResource->m_Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		ThrowIfFailed(fence->SetEventOnCompletion(m_currFrameResource->m_Fence, eventHandle));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void FrameResourceContextInterface::Draw(const GameTimer& gt, UINT64& CurrentFence,ID3D12Fence* fence, ID3D12CommandQueue*comQueue)
{
	auto cmdListAlloc = m_currFrameResource->m_CmdListAlloc;

	ThrowIfFailed(cmdListAlloc->Reset());

	DrawFrameResource(cmdListAlloc.Get());

	/* [...] Build and submit command lists for this frame.
	 Advance the fence value to mark commands up to this fence point.*/

	m_currFrameResource->m_Fence = ++CurrentFence;
	/* Add an instruction to the command queue to set a new fence point.
	 Because we are on the GPU timeline, the new fence point won’t be
	 set until the GPU finishes processing all the commands prior to
	 this Signal().*/
	comQueue->Signal(fence, CurrentFence);
	/* Note that GPU could still be working on commands from previous
	 frames, but that is okay, because we are not touching any frame
	 resources associated with those frames.*/
}