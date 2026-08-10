
#pragma once

#include "MathHelper.h"
#include "Util.h"
#include "Struct.h"
#include "GameTimer.h"

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
		int							 MaterialCBIndex = -1; //在frameResource里的material常量缓冲区的索引

        int                 DiffuseTextureHeapIndex;
        int                 SpecularTextureHeapIndex;
        int                 NormalTextureHeapIndex;
        int                 EmissiveTextureHeapIndex;
    };

    struct SDKMeshRenderItemWithMaterial : public RenderItem
    {
        SDKMeshMaterial* m_Material = nullptr;
        std::shared_ptr<std::vector<D3D12_INPUT_ELEMENT_DESC> >  m_vbDecl = nullptr;  
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO = nullptr;
    };

    //buffer
	struct SDKMeshObjectConstants
	{
		DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
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

    */

	struct alignas(16) SDKMeshMaterialConstants
	{
		DirectX::XMFLOAT4	DiffuseAlbedo = { 0.0f, 0.0f, 0.0f, 1.0f };
        
        DirectX::XMFLOAT3   EmissiveColor = { 0.0f, 0.0f, 0.0f };
        float               MaterialPad0;  

        DirectX::XMFLOAT3   SpecularColor = { 0.0f, 0.0f, 0.0f };
        float               SpecularPower = 0.0f;
	};

    struct SDKMeshModel
    {

        SDKMeshModel(ID3D12Device* device);

        bool LoadModel(std::wstring filename);

        void BuildShapeGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList);

        void BuildDescriptorHeaps(
            CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, UINT CbvSrvUavDescriptorSize);

        void BuildMaterialsFromSDKMesh(UINT frameResouceIndex);

        void BuildTextures(ID3D12GraphicsCommandList* mCommandList);

        void BuildRenderItems(DirectX::XMMATRIX world,UINT frameResourceIndex);

        UINT GetTextureCount()const;

		UINT GetRenderItemCount()const;

        UINT GetMaterialCount()const;

        void UpdateObjectCBs(const GameTimer& gt, FrameResourceInterface* frameResource);
        void UpdateMaterialCBs(const GameTimer& gt, FrameResourceInterface* frameResource);

        void BuildPSOs(D3D12_GRAPHICS_PIPELINE_STATE_DESC desc,Microsoft::WRL::ComPtr<ID3DBlob> standardVS,
            Microsoft::WRL::ComPtr<ID3DBlob> opaquesPS,
            Microsoft::WRL::ComPtr<ID3DBlob> alphaPS);

        ID3D12Device* m_device = nullptr;

        std::shared_ptr< DirectX::DX12::Model> m_model = nullptr;

        std::vector< std::unique_ptr<MeshGeometry> >					                m_opaqueGeometries;
        std::vector< std::unique_ptr<MeshGeometry> >					                m_alphaGeometries;

        std::vector<std::unique_ptr<SDKMeshRenderItemWithMaterial>>										m_opaqueRitems;
        std::vector<std::unique_ptr<SDKMeshRenderItemWithMaterial>>										m_alphaRitems;

        std::vector<std::unique_ptr<SDKMeshMaterial>>						            m_Materials;

        CD3DX12_GPU_DESCRIPTOR_HANDLE			                                        m_HeapGpuSRrv;
        CD3DX12_CPU_DESCRIPTOR_HANDLE			                                        m_HeapCPUSrv;

    }; 

	void BuildMaterialsFromSDKMesh(_In_ ID3D12Device* device, DirectX::DX12::Model* model);
}
