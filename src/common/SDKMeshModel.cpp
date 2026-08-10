#include "SDKMeshModel.h"
#include "Model.h"
#include "DirectXHelpers.h"
#include "Geometry.h"
#include "Struct.h"
#include "FrameResource.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

namespace SDKMesh
{

	void SDKMeshModel::BuildMaterialsFromSDKMesh(UINT frameResourceIndex)
	{
		const auto& materials = m_model->materials;
		UINT localFrameResourceIndex = frameResourceIndex;

		for (auto i = 0;i < materials.size();++i)
		{
			const auto& sdkMaterial = materials[i];

			auto material = std::make_unique<SDKMeshMaterial>();
			
			material->MaterialCBIndex = localFrameResourceIndex;
			material->Name = WStringToString(sdkMaterial.name);
			material->AmbientColor = sdkMaterial.ambientColor;
			material->DiffuseColor = sdkMaterial.diffuseColor;
			material->SpecularColor = sdkMaterial.specularColor;
			material->EmissiveColor = sdkMaterial.emissiveColor;
			material->SpecularPower = sdkMaterial.specularPower;

			material->DiffuseTextureHeapIndex =  (sdkMaterial.diffuseTextureIndex == -1)? -1 : m_model->getTextureResouceSlotByNameIndex( sdkMaterial.diffuseTextureIndex);
			material->SpecularTextureHeapIndex = (sdkMaterial.specularTextureIndex == -1)? -1 : m_model->getTextureResouceSlotByNameIndex( sdkMaterial.specularTextureIndex);
			material->NormalTextureHeapIndex = (sdkMaterial.normalTextureIndex == -1)? -1 : m_model->getTextureResouceSlotByNameIndex( sdkMaterial.normalTextureIndex);
			material->EmissiveTextureHeapIndex = (sdkMaterial.emissiveTextureIndex == -1)? -1 : m_model->getTextureResouceSlotByNameIndex( sdkMaterial.emissiveTextureIndex);
			m_Materials.emplace_back(std::move(material));

			localFrameResourceIndex++;
		}
	}

	void SDKMeshModel::BuildTextures(ID3D12GraphicsCommandList* mCommandList)
	{
		m_model->LoadTextures(m_device,
			mCommandList, (SourcePath() + L"Models/powerplant/").c_str(), 0);
	}

	void SDKMeshModel::BuildRenderItems(DirectX::XMMATRIX world, UINT frameResourceIndex)
	{
		UINT localFrameResourceIndex = frameResourceIndex;

		for (auto& geo : m_opaqueGeometries)
		{
			auto ritem = std::make_unique<SDKMeshRenderItemWithMaterial>();
			XMStoreFloat4x4(&ritem->m_World, world);
			ritem->m_ObjCBIndex = localFrameResourceIndex;
			ritem->m_Material = m_Materials[geo->m_DrawArgs["subMesh"].m_materialIndex].get();
			ritem->m_Geo = geo.get();
			ritem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			ritem->m_IndexCount = ritem->m_Geo->m_DrawArgs["subMesh"].m_IndexCount;
			ritem->m_StartIndexLocation = ritem->m_Geo->m_DrawArgs["subMesh"].m_StartIndexLocation;
			ritem->m_BaseVertexLocation = ritem->m_Geo->m_DrawArgs["subMesh"].m_BaseVertexLocation;
			ritem->m_vbDecl = geo->m_DrawArgs["subMesh"].m_vbDecl;
			m_opaqueRitems.push_back(std::move(ritem));
			localFrameResourceIndex++;
		}
		 
		for (auto& geo : m_alphaGeometries)
		{
			auto ritem = std::make_unique<SDKMeshRenderItemWithMaterial>();
			XMStoreFloat4x4(&ritem->m_World, world);
			ritem->m_ObjCBIndex = localFrameResourceIndex;
			ritem->m_Material = m_Materials[geo->m_DrawArgs["subMesh"].m_materialIndex].get();
			ritem->m_Geo = geo.get();
			ritem->m_PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			ritem->m_IndexCount = ritem->m_Geo->m_DrawArgs["subMesh"].m_IndexCount;
			ritem->m_StartIndexLocation = ritem->m_Geo->m_DrawArgs["subMesh"].m_StartIndexLocation;
			ritem->m_BaseVertexLocation = ritem->m_Geo->m_DrawArgs["subMesh"].m_BaseVertexLocation;
			ritem->m_vbDecl = geo->m_DrawArgs["subMesh"].m_vbDecl;
			m_alphaRitems.push_back(std::move(ritem));
			localFrameResourceIndex++;
		}
	}

	UINT SDKMeshModel::GetTextureCount() const
	{
		if (m_model)
			return static_cast<UINT>(m_model->mResources.size());
		return 0;
	}

	UINT SDKMeshModel::GetRenderItemCount() const
	{
		return m_alphaRitems.size() + m_opaqueRitems.size();
	}

	UINT SDKMeshModel::GetMaterialCount() const
	{
		return m_Materials.size();
	}

	void SDKMeshModel::UpdateObjectCBs(const GameTimer& gt, FrameResourceInterface* frameResource)
	{
		for (auto& e : m_opaqueRitems)
		{
			SDKMeshRenderItemWithMaterial* itemWithM = static_cast<SDKMeshRenderItemWithMaterial*>(e.get());

			if (itemWithM->m_NumFramesDirty > 0)
			{
				XMMATRIX world = XMLoadFloat4x4(&e->m_World);

				SDKMeshObjectConstants objConstants;
				XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(world));
				XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

				frameResource->CopyConstData(itemWithM->m_ObjCBIndex, &objConstants);
				e->m_NumFramesDirty--;
			}
		}
	}

	void SDKMeshModel::UpdateMaterialCBs(const GameTimer& gt, FrameResourceInterface* frameResource)
	{
		for (auto& e : m_Materials)
		{
			SDKMeshMaterial* mat = static_cast<SDKMeshMaterial*>(e.get());
			if (mat->NumFramesDirty > 0)
			{
				SDKMeshMaterialConstants matConstants;
				matConstants.DiffuseAlbedo = mat->DiffuseColor;
				matConstants.EmissiveColor = mat->EmissiveColor;
				matConstants.SpecularColor = mat->SpecularColor;
				matConstants.SpecularPower = mat->SpecularPower;
				matConstants.ambientColor = mat->ambientColor;

				frameResource->CopyMaterialData(mat->MaterialCBIndex, &matConstants);

				mat->NumFramesDirty--;
			}
		}
	}

	void SDKMeshModel::BuildPSOs(D3D12_GRAPHICS_PIPELINE_STATE_DESC desc,Microsoft::WRL::ComPtr<ID3DBlob> standardVS,
		Microsoft::WRL::ComPtr<ID3DBlob> opaquesPS, 
		Microsoft::WRL::ComPtr<ID3DBlob> alphaPS)
	{
		for (auto& ritem : m_opaqueRitems)
		{
			auto& drawAble = ritem->m_Geo->m_DrawArgs["subMesh"];
			D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
			ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
			opaquePsoDesc = desc;

			opaquePsoDesc.InputLayout = { drawAble.m_vbDecl.get()->data(), (UINT)drawAble.m_vbDecl.get()->size() };
			opaquePsoDesc.VS =
			{
				reinterpret_cast<BYTE*>(standardVS->GetBufferPointer()),
				standardVS->GetBufferSize()
			};
			opaquePsoDesc.PS =
			{
				reinterpret_cast<BYTE*>(opaquesPS->GetBufferPointer()),
				opaquesPS->GetBufferSize()
			};
		
			ThrowIfFailed(m_device->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(& ritem->m_PSO)));
		}

		for (auto& ritem : m_alphaRitems)
		{
			auto& drawAble = ritem->m_Geo->m_DrawArgs["subMesh"];
			D3D12_GRAPHICS_PIPELINE_STATE_DESC alphPsoDesc;
			ZeroMemory(&alphPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
			alphPsoDesc = desc;

			alphPsoDesc.InputLayout = { drawAble.m_vbDecl.get()->data(), (UINT)drawAble.m_vbDecl.get()->size() };
			alphPsoDesc.VS =
			{
				reinterpret_cast<BYTE*>(standardVS->GetBufferPointer()),
				standardVS->GetBufferSize()
			};
			alphPsoDesc.PS =
			{
				reinterpret_cast<BYTE*>(opaquesPS->GetBufferPointer()),
				alphaPS->GetBufferSize()
			};

			ThrowIfFailed(m_device->CreateGraphicsPipelineState(&alphPsoDesc, IID_PPV_ARGS(&ritem->m_PSO)));
		}

	}
	
	SDKMeshModel::SDKMeshModel(ID3D12Device* device)
	{
		m_device = device;
	}

	bool SDKMeshModel::LoadModel(std::wstring filename)
	{
		m_model =
			DirectX::DX12::Model::CreateFromSDKMESH(m_device,
				(SourcePath() + L"Models/powerplant/powerplant.sdkmesh").c_str());
		return true;
	}

	void SDKMeshModel::BuildShapeGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList)
	{
		m_model->LoadStaticBuffers(device, mCommandList, false);

		SubmeshGeometry submesh;
		submesh.m_StartIndexLocation = 0;
		submesh.m_BaseVertexLocation = 0;

		for (const auto& mesh : m_model->meshes)
		{
			for (const auto& part : mesh->opaqueMeshParts)
			{
				auto geo = std::make_unique<MeshGeometry>();

				geo->m_VertexBufferGPU = part->staticVertexBuffer;
				geo->m_IndexBufferGPU = part->staticIndexBuffer;

				geo->m_VertexByteStride = part->vertexStride;
				geo->m_VertexBufferByteSize = part->vertexBufferSize;
				geo->m_IndexFormat = part->indexFormat;
				geo->m_IndexBufferByteSize = part->indexBufferSize;
				submesh.m_IndexCount = part->indexCount;
				submesh.m_materialIndex = part->materialIndex;
				submesh.m_vbDecl = part->vbDecl;
				geo->m_DrawArgs["subMesh"] = submesh;

				m_opaqueGeometries.emplace_back(std::move(geo));
			}

			for (const auto& part : mesh->alphaMeshParts)
			{
				auto geo = std::make_unique<MeshGeometry>();

				geo->m_VertexBufferGPU = part->staticVertexBuffer;
				geo->m_IndexBufferGPU = part->staticIndexBuffer;

				geo->m_VertexByteStride = part->vertexStride;
				geo->m_VertexBufferByteSize = part->vertexBufferSize;
				geo->m_IndexFormat = part->indexFormat;
				geo->m_IndexBufferByteSize = part->indexBufferSize;
				submesh.m_IndexCount = part->indexCount;
				submesh.m_materialIndex = part->materialIndex;
				submesh.m_vbDecl = part->vbDecl;
				geo->m_DrawArgs["subMesh"] = submesh;

				m_alphaGeometries.emplace_back(std::move(geo));
			}
		}

	}

	void SDKMeshModel::BuildDescriptorHeaps(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		UINT CbvSrvUavDescriptorSize)
	{
		m_HeapCPUSrv = hCpuSrv;
		m_HeapGpuSRrv = hGpuSrv;

		int idx = 0;
		for (auto iter = m_model->mResources.begin(); iter != m_model->mResources.end(); iter++)
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrvStart = hCpuSrv;
			hCpuSrvStart.Offset(idx, CbvSrvUavDescriptorSize);

			DirectX::CreateShaderResourceView(m_device,
				iter->mResource.Get(),
				hCpuSrvStart,
				iter->mIsCubeMap);

			idx++;
		}
	}

}

