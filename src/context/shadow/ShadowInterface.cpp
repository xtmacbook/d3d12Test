#include "ShadowInterface.h"
#include "common/ShadowMapRes.h"
#include "common/D3DContext.h"
#include "common/FrameResource.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

ShadowInterface::ShadowInterface(D3DContext* context) : m_d3dContext(context)
{
}

void ShadowInterface::InitSceneBounds()
{
}

void ShadowInterface::BuildDrawScenePSO(ID3D12RootSignature* rootSignature)
{
	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;

	auto shadowVS = D3DUtil::CompileShader(SourcePath() + L"/Shaders/ShadowMapDraw.hlsl", nullptr, "VS", "vs_5_1");
	auto shadowOpaquePS = D3DUtil::CompileShader(SourcePath() + L"/Shaders/ShadowMapDraw.hlsl", nullptr, "PS", "ps_5_1");

	//m_Shaders["debugVS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/ShadowDebug.hlsl", nullptr, "VS", "vs_5_1");
	//m_Shaders["debugPS"] = D3DUtil::CompileShader(SourcePath() + L"/Shaders/ShadowDebug.hlsl", nullptr, "PS", "ps_5_1");
	
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	//debug shadow map pso
	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowDebugPSDesc = m_d3dContext->GetDefaultPSODesc();
	shadowDebugPSDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };

	shadowDebugPSDesc.pRootSignature = rootSignature;

	shadowDebugPSDesc.VS =
	{
		reinterpret_cast<BYTE*>(shadowVS->GetBufferPointer()),
		shadowVS->GetBufferSize()
	};
	shadowDebugPSDesc.PS =
	{
		reinterpret_cast<BYTE*>(shadowOpaquePS->GetBufferPointer()),
		shadowOpaquePS->GetBufferSize()
	};

	//shadow map draw pso
	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowDrawPSDesc = shadowDebugPSDesc;
	shadowDrawPSDesc.VS =
	{
		reinterpret_cast<BYTE*>(shadowVS->GetBufferPointer()),
		shadowVS->GetBufferSize()
	};

	shadowDrawPSDesc.PS =
	{
		reinterpret_cast<BYTE*>(shadowOpaquePS->GetBufferPointer()),
		shadowOpaquePS->GetBufferSize()
	};

	shadowDrawPSDesc.RasterizerState.DepthBias = 100000;
	shadowDrawPSDesc.RasterizerState.DepthBiasClamp = 0.0f;
	shadowDrawPSDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;

	// Shadow map pass does not have a render target.
	shadowDrawPSDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	shadowDrawPSDesc.NumRenderTargets = 0;
	ThrowIfFailed(m_d3dContext->device()->CreateGraphicsPipelineState(&shadowDrawPSDesc, IID_PPV_ARGS(&(m_ShadowMapDrawPSO))));

}

void ShadowInterface::BuildShadowMap()
{
	m_ShadowMap = std::make_shared<ShadowMapRes>(m_d3dContext->device(), 2048, 2048);
}

void ShadowInterface::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
	m_ShadowMap->BuildDescriptors(hCpuSrv, hGpuSrv, hCpuDsv);
}

ID3D12PipelineState* ShadowInterface::GetDrawSceneToShadowMapPSO()
{
	return m_ShadowMapDrawPSO.Get();
}

void ShadowInterface::UpdateShadowTransform(ShadowMapUpdateData data)
{
	// Only the first "main" light casts a shadow.
	XMVECTOR lightDir = XMLoadFloat3(&data.m_lightDir);
	XMVECTOR lightPos = -2.0f * data.m_sceneBounds.Radius * lightDir;
	XMVECTOR targetPos = XMLoadFloat3(&data.m_sceneBounds.Center);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);
	XMStoreFloat3(&m_LightPosW, lightPos);

	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - data.m_sceneBounds.Radius;
	float b = sphereCenterLS.y - data.m_sceneBounds.Radius;
	float n = sphereCenterLS.z - data.m_sceneBounds.Radius;
	float r = sphereCenterLS.x + data.m_sceneBounds.Radius;
	float t = sphereCenterLS.y + data.m_sceneBounds.Radius;
	float f = sphereCenterLS.z + data.m_sceneBounds.Radius;

	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX S = lightView * lightProj * T;
	XMStoreFloat4x4(&m_LightView, lightView);
	XMStoreFloat4x4(&m_LightProj, lightProj);
	XMStoreFloat4x4(&m_ShadowTransform, S);
}

void ShadowInterface::DrawSceneToShadowMap(ID3D12GraphicsCommandList* mCommandList, ShadowMapDrawData data)
{
	if (!data.m_drawCb)
		return;

	mCommandList->RSSetViewports(1, &m_ShadowMap->Viewport());
	mCommandList->RSSetScissorRects(1, &m_ShadowMap->ScissorRect());

	// Change to DEPTH_WRITE.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_ShadowMap->Resource(),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearDepthStencilView(m_ShadowMap->Dsv(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);

	// Set null render target because we are only going to draw to
	// depth buffer. Setting a null render target will disable color writes.
	// Note the active PSO also must specify a render target count of 0.
	mCommandList->OMSetRenderTargets(0, nullptr, false, &m_ShadowMap->Dsv()); // 不给渲染目标,只给深度缓冲区

	//mCommandList->SetPipelineState(m_PSOs["shadow"].Get());

	mCommandList->SetGraphicsRootConstantBufferView(data.m_shadowPassRootParameterIndx,data.m_shadowPassAddress);

	data.m_drawCb(mCommandList);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_ShadowMap->Resource(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_GENERIC_READ));
}

void ShadowInterface::UpdateShadowPass(const GameTimer& gt, FrameResourceInterface*resource,int idx)
{
	XMMATRIX proj = XMLoadFloat4x4(&m_LightProj);
	XMMATRIX view = XMLoadFloat4x4(&m_LightView);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);
	
	XMStoreFloat4x4(&m_shadowPass.m_View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&m_shadowPass.m_InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&m_shadowPass.m_Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&m_shadowPass.m_InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&m_shadowPass.m_ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&m_shadowPass.m_InvViewProj, XMMatrixTranspose(invViewProj));

	m_shadowPass.m_EyePosW = m_LightPosW;
	m_shadowPass.m_RenderTargetSize = XMFLOAT2((float)m_ShadowMap->Width(), (float)m_ShadowMap->Height());
	m_shadowPass.m_InvRenderTargetSize = XMFLOAT2(1.0f / m_ShadowMap->Width(), 1.0f / m_ShadowMap->Height());
	m_shadowPass.m_NearZ = 1.0f;
	m_shadowPass.m_FarZ = 1000.0f;
	m_shadowPass.m_TotalTime = gt.TotalTime();
	m_shadowPass.m_DeltaTime = gt.DeltaTime();


	resource->CopyPassData(idx, &m_shadowPass);
}



