
#include "common/util.h"
#include "common/GameTimer.h"

#include <vector>
#include <memory>
#include <functional>

class Camera;
class ShadowMapRes;
class FrameResourceInterface;

#define MAX_CASCADES 8

namespace SDKMesh
{
	struct Effect;
	struct SDKMeshModel;
}

struct CSMPassConstants
{
	DirectX::XMFLOAT4X4     m_WorldViewProj;
	DirectX::XMFLOAT4X4     m_World;
	DirectX::XMFLOAT4X4     m_WorldView;

	DirectX::XMFLOAT4X4     m_Shadow;

	DirectX::XMFLOAT4       m_vCascadeOffset[8];
	DirectX::XMFLOAT4       m_vCascadeScale[8];

	INT                     m_nCascadeLevels;       // Number of Cascades
	INT                     m_iVisualizeCascades; // 1 is to visualize the cascades in different colors. 0 is to just draw the scene.

	INT                     m_iPCFBlurForLoopStart; // For loop begin value. For a 5x5 kernal this would be -2.
	INT                     m_iPCFBlurForLoopEnd; // For loop end value. For a 5x5 kernel this would be 3.

	// For Map based selection scheme, this keeps the pixels inside of the the valid range.
	// When there is no boarder, these values are 0 and 1 respectivley.
	FLOAT                   m_fMinBorderPadding;
	FLOAT                   m_fMaxBorderPadding;
	FLOAT                   m_fShadowBiasFromGUI;  // A shadow map offset to deal with self shadow artifacts.  

	//These artifacts are aggravated by PCF.
	FLOAT                   m_fShadowPartitionSize; //1 / nCascadeLevels
	FLOAT                   m_fCascadeBlendArea; // Amount to overlap when blending between cascades.
	FLOAT                   m_fTexelSize; // 1/ buffersize
	FLOAT                   m_fNativeTexelSizeInX; //  texlSize / cascadeLevels
	FLOAT                   m_fPaddingForCB3;// Padding variables CBs must be a multiple of 16 bytes.

	FLOAT                   m_fCascadeFrustumsEyeSpaceDepths[8]; // The values along Z that seperate the cascades.
	DirectX::XMFLOAT4       m_fCascadeFrustumsEyeSpaceDepthsFloat4[8];// the values along Z that separte the cascades.  
	// Wastefully stored in float4 so they are array indexable :(
	DirectX::XMFLOAT4       m_vLightDir;

};

enum SHADOW_TEXTURE_FORMAT
{
	CASCADE_DXGI_FORMAT_R32_TYPELESS,
	CASCADE_DXGI_FORMAT_R24G8_TYPELESS,
	CASCADE_DXGI_FORMAT_R16_TYPELESS,
	CASCADE_DXGI_FORMAT_R8_TYPELESS
};

enum CASCADE_SELECTION
{
	CASCADE_SELECTION_MAP,
	CASCADE_SELECTION_INTERVAL
};

enum FIT_PROJECTION_TO_CASCADES
{
	FIT_TO_CASCADES,
	FIT_TO_SCENE
};

enum FIT_TO_NEAR_FAR
{
	FIT_NEARFAR_PANCAKING,
	FIT_NEARFAR_ZERO_ONE,
	FIT_NEARFAR_AABB,
	FIT_NEARFAR_SCENE_AABB
};

struct CSMConfig
{
	INT m_nCascadeLevels;
	SHADOW_TEXTURE_FORMAT m_ShadowBufferFormat;
	INT m_iBufferSize;
};

struct CSMShadowMapDrawData
{
	using DrawSceneForShadowMapFunc = std::function<void(ID3D12GraphicsCommandList*)>;

	DrawSceneForShadowMapFunc m_drawCb;
	UINT m_shadowPassRootParameterIndx;
	D3D12_GPU_VIRTUAL_ADDRESS m_shadowPassAddress;
};

struct EffectsCluster
{
	std::shared_ptr<SDKMesh::Effect> effectCluster [MAX_CASCADES][2][2][2];
};

class CascadedShadowsManager
{
public:

	bool init(ID3D12Device*,const DirectX::BoundingBox& sceneBox,
		const Camera* viewCamra, const Camera* shadowCamera, CSMConfig* config);

	void BuildShadowMap();

	void BuildPOS(SDKMesh::SDKMeshModel*, D3D12_GRAPHICS_PIPELINE_STATE_DESC);

	ID3D12PipelineState* GetDrawSceneToShadowMapPSO();

	std::vector< std::shared_ptr<SDKMesh::Effect> >& GetEffect();

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

	//根据cascade interval计算 project frumstum的8个角的位置
	void CreateFrustumPointsFromCascadeInterval(float fCascadeIntervalBegin,
		FLOAT fCascadeIntervalEnd,
		DirectX::CXMMATRIX vProjection,
		DirectX::XMVECTOR* pvCornerPointsWorld);

	void UpdateFrame(const GameTimer& gt, Camera* viewCamera,Camera* lightCamer);

	void UpdateMainPassData(CSMPassConstants&constData);
	void UpdateShadowPassData(UINT iCascadeIndex, CSMPassConstants& constData);

	void RenderShadowsForAllCascades(ID3D12GraphicsCommandList* cmmandList, CSMShadowMapDrawData& data);

	void RenderScene(ID3D12GraphicsCommandList* cmmandList, CSMShadowMapDrawData& data);

	void ReleasePreFrameResource();

	FIT_PROJECTION_TO_CASCADES														m_eSelectedCascadesFit;
	FIT_TO_NEAR_FAR																	m_eSelectedNearFarFit;

	INT																				m_iCascadePartitionsMax;
	FLOAT																			m_fCascadePartitionsFrustum[MAX_CASCADES]; //程序计算的真是只 Values are  between near and far
	INT																				m_iCascadePartitionsZeroToOne[MAX_CASCADES]; //这个是用户设置的： Values are 0 to 100 and represent a percent of the frstum
	INT																				m_iPCFBlurSize;
	bool																			m_bMoveLightTexelSize = true;

private:
	
	DirectX::XMVECTOR CalculateCascadeFrumstum(int iCascadeIndex,Camera* viewCamera, Camera* lightCamer,
		DirectX::XMVECTOR& vLightCameraOrthographicMin,
		DirectX::XMVECTOR& vLightCameraOrthographicMax);

	void FrustumFitScene(int iCascadeIndex, FLOAT& fFrustumIntervalBegin,
		FLOAT& fFrustumIntervalEnd);

	void  FrustumFitCascade(int iCascadeIndex, FLOAT& fFrustumIntervalBegin,
		FLOAT& fFrustumIntervalEnd);

	DirectX::XMVECTOR UnShimmeringAndReturnWorldUnitsPerTexel(DirectX::XMVECTOR* vFrustumPoints, DirectX::XMVECTOR& vLightCameraOrthographicMin,
		DirectX::XMVECTOR& vLightCameraOrthographicMax);

	void CalculateCascadeNearAndFarPlane(DirectX::XMVECTOR* vSceneAABBPointsLightSpace,
		DirectX::XMVECTOR& vLightCameraOrthographicMin,
		DirectX::XMVECTOR& vLightCameraOrthographicMax,FLOAT&nearPlane,FLOAT&farPlane);
	
	void FitNearFarWithAABB(DirectX::XMVECTOR* vSceneAABBPointsLightSpace,
		DirectX::XMVECTOR& vLightCameraOrthographicMin,
		DirectX::XMVECTOR& vLightCameraOrthographicMax, FLOAT& nearPlane, FLOAT& farPlane);

	void FitNearFarWithScene(DirectX::XMVECTOR* vSceneAABBPointsLightSpace,
		DirectX::XMVECTOR& vLightCameraOrthographicMin,
		DirectX::XMVECTOR& vLightCameraOrthographicMax, FLOAT& nearPlane, FLOAT& farPlane);

	// Compute the near and far plane by intersecting an Ortho Projection with the Scenes AABB.
	void ComputeNearAndFar(FLOAT& fNearPlane,
		FLOAT& fFarPlane,
		DirectX::FXMVECTOR vLightCameraOrthographicMin,
		DirectX::FXMVECTOR vLightCameraOrthographicMax,
		DirectX::XMVECTOR* pvPointsInCameraView);

private:
	Microsoft::WRL::ComPtr<ID3DBlob> 												m_ShadowVSShader;

	Microsoft::WRL::ComPtr<ID3DBlob>												m_ppsRenderSceneVSShadersBlob[MAX_CASCADES];
	Microsoft::WRL::ComPtr<ID3DBlob>												m_ppsRenderScenePSShadersBlob[MAX_CASCADES][2][2][2];


	std::shared_ptr<ShadowMapRes>													m_ShadowMap;
	EffectsCluster																	m_effects; //目前所有的mesh中的part使用相同的pso，具体模型可能不同
	Microsoft::WRL::ComPtr<ID3D12PipelineState>										m_shadowDrawPSO = nullptr;

	DirectX::BoundingBox															m_sceneBox;

	CSMConfig*																		m_csmConfig;//用户可能修改了
	CSMConfig																		m_copyCsmConfig;
	ID3D12Device*																	m_d3dDevice = nullptr;

	//更新shader
	INT																				m_iDerivativeBasedOffset = 0; //是否使用梯度计算偏移
	INT																				m_iBlurBetweenCascades = 0;//是否blur
	CASCADE_SELECTION																m_eSelectedCascadeSelection = CASCADE_SELECTION_INTERVAL;
	
	DirectX::XMMATRIX																m_matShadowProj[MAX_CASCADES];
	DirectX::XMMATRIX																m_matShadowView;

};