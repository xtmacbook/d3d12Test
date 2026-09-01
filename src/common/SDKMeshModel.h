
#pragma once

#include "MathHelper.h"
#include "Util.h"
#include "Struct.h"
#include "GameTimer.h"
#include "Model.h"
#include "SDKMeshEffect.h"

#include <memory>
#include <vector>

struct MeshGeometry;
class FrameResourceInterface;

namespace DirectX
{
    inline namespace DX12
    {
		class Model;
    }
}
namespace SDKMesh
{
    struct SDKMeshMaterial
    {
        std::string                  Name;

        DirectX::XMFLOAT3            AmbientColor;
        DirectX::XMFLOAT3            DiffuseColor;
        DirectX::XMFLOAT3            SpecularColor;
        DirectX::XMFLOAT3            EmissiveColor;
        float                        SpecularPower;
        
        int							 NumFramesDirty = 3;

        int                 DiffuseTextureHeapIndex;
        int                 SpecularTextureHeapIndex;
        int                 NormalTextureHeapIndex;
        int                 EmissiveTextureHeapIndex;
    };


    //buffer
	struct SDKMeshObjectConstants
	{
		DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	};


    /*
        A) Material color parameters. The desired lighting model is:
            ((ambientLightColor + sum(diffuse directional light)) * diffuseColor) + emissiveColor

        B) When lighting is disabled:
        ambient and directional lights are ignored, leaving:
          diffuseColor + emissiveColor
         We can save one shader instruction by precomputing 
         diffuse+emissive on the CPU, after which the shader can use diffuseColor directly,ignoring its emissive parameter.

        C) When lighting is enabled:
        
        we can merge the ambient and emissive settings. If we set our emissive parameter to emissive+(ambient*diffuse), 
        the shader no longer needs to bother adding the ambient contribution, simplifying its computation to:
        (sum(diffuse directional light) * diffuseColor) + emissiveColor
    
        For futher optimization goodness, we merge material alpha with the diffuse
        color parameter, and premultiply all color values by this alpha.

        所以的SDKMeshMaterialConstants没有ambient color，被算入到emissiveColor里了
    */

	struct alignas(16) SDKMeshMaterialConstants
	{
		DirectX::XMFLOAT4	DiffuseColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        
        DirectX::XMFLOAT3   EmissiveColor = { 0.0f, 0.0f, 0.0f };
        float               MaterialPad0;  

        DirectX::XMFLOAT3   SpecularColor = { 1.0f, 1.0f, 1.0f };
        float               SpecularPower = 16.0f;
	};

    struct SDKMeshModel
    {

        SDKMeshModel(ID3D12Device* device);

        bool LoadModel(std::wstring filename);

        void BuildShapeGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList);

        void BuildTextureResourceView(
            CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
            INT offset,
            UINT CbvSrvUavDescriptorSize);

        UINT getTextureOffset(int textureIndex);

        void BuildTextures(ID3D12GraphicsCommandList* mCommandList);

        void DrawRenderItems(ID3D12CommandAllocator* allocator,
            ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList, 
            FrameResourceInterface*, ID3D12DescriptorHeap*, UINT);

        UINT GetTextureCount()const;

		UINT GetRenderItemCount()const;

        UINT GetMaterialCount()const;

        void CreateEffect(SDKMesh::EffectPipelineStateDescription& pipeLineStateDescription);

        void UpdateObjectCBs(const GameTimer& gt, FrameResourceInterface* frameResource);
        void UpdateMaterialCBs(const GameTimer& gt, FrameResourceInterface* frameResource);

        ID3D12Device* m_device = nullptr;

        std::shared_ptr< DirectX::DX12::Model> m_model = nullptr;

        CD3DX12_GPU_DESCRIPTOR_HANDLE			                                        m_HeapGpuSRrv;
        CD3DX12_CPU_DESCRIPTOR_HANDLE			                                        m_HeapCPUSrv;
        INT                                                                            m_textureDescriptorOffset;
        std::vector< std::shared_ptr<SDKMesh::Effect> >                                          m_effects;

    }; 

	void BuildMaterialsFromSDKMesh(_In_ ID3D12Device* device, DirectX::DX12::Model* model);
}
