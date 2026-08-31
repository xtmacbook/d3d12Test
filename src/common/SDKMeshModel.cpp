#include "SDKMeshModel.h"
#include "DirectXHelpers.h"
#include "Geometry.h"
#include "Struct.h"
#include "FrameResource.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

namespace SDKMesh
{

	void SDKMeshModel::BuildTextures(ID3D12GraphicsCommandList* mCommandList)
	{
		m_model->LoadTextures(m_device,
			mCommandList, (SourcePath() + L"Models/powerplant/").c_str(), 0);
	}

	void SDKMeshModel::DrawRenderItems(ID3D12CommandAllocator* allocator,
		ID3D12Device* device,
		ID3D12GraphicsCommandList* mCommandList, FrameResourceInterface * resouce,
		ID3D12DescriptorHeap* heapDescriptor, UINT CbvSrvUavDescriptorSize)
	{
		 
		CD3DX12_GPU_DESCRIPTOR_HANDLE descriptorStart(heapDescriptor->GetGPUDescriptorHandleForHeapStart());

		UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(SDKMesh::SDKMeshObjectConstants));
		UINT matCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(SDKMesh::SDKMeshMaterialConstants));

		int meshIdex = 0;
		int descriptorC = 0;
		for (auto& mesh : m_model->meshes)
		{
			int opaqueMPIndex = 0;
			for (auto& opaqueMP : mesh->opaqueMeshParts)
			{
				int partIndex = opaqueMP->partIndex;

				Effect* effect = m_effects[partIndex].get();

				mCommandList->SetPipelineState(effect->m_PSO.Get());
				//ThrowIfFailed(mCommandList->Reset(allocator, effect->m_PSO.Get()));

				//set obj const
				UINT64 offset = static_cast<UINT64>(partIndex) * objCBByteSize;
				D3D12_GPU_VIRTUAL_ADDRESS startAddress = resouce->getConstGpuAddress();

				mCommandList->SetGraphicsRootConstantBufferView(0, startAddress + offset);
				
				//set texture
				const auto& material =  m_model->materials[opaqueMP->materialIndex];

				INT diffusetLocalOffset = getTextureOffset(material.diffuseTextureIndex);
				INT normalLocalOffset =  getTextureOffset(material.normalTextureIndex);

				CD3DX12_GPU_DESCRIPTOR_HANDLE diffuseTextureHandle = descriptorStart;
				CD3DX12_GPU_DESCRIPTOR_HANDLE normalTextureHandle = descriptorStart;
				diffuseTextureHandle.Offset(m_textureDescriptorOffset + diffusetLocalOffset, CbvSrvUavDescriptorSize);
				normalTextureHandle.Offset(m_textureDescriptorOffset + normalLocalOffset, CbvSrvUavDescriptorSize);

				mCommandList->SetGraphicsRootDescriptorTable(3, diffuseTextureHandle);
				mCommandList->SetGraphicsRootDescriptorTable(4, normalTextureHandle);

				//set material
				D3D12_GPU_VIRTUAL_ADDRESS matCBAddress =
					resouce->getMaterialGpuAddress() +
					opaqueMP->materialIndex  * matCBByteSize;
				mCommandList->SetGraphicsRootConstantBufferView(1, matCBAddress);

				opaqueMP->Draw(mCommandList);

				opaqueMPIndex++;
				descriptorC++;
			}

			for (auto& alphaMP : mesh->alphaMeshParts)
			{
				alphaMP->Draw(mCommandList);
			}

			meshIdex++;
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
		return m_model->GetMeshPartCount();
	}

	UINT SDKMeshModel::GetMaterialCount() const
	{
		return m_model->materials.size();
	}

	void SDKMeshModel::UpdateObjectCBs(const GameTimer& gt, FrameResourceInterface* frameResource)
	{
		for (auto& mesh : m_model->meshes)
		{
			for (auto& opaqueMP : mesh->opaqueMeshParts)
			{
				XMMATRIX world= DirectX::XMMatrixIdentity();
				SDKMeshObjectConstants objConstants;
				XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
				frameResource->CopyConstData(opaqueMP->partIndex, &objConstants);
			}
			
			for (auto& alphaMP : mesh->alphaMeshParts)
			{
				XMMATRIX world = DirectX::XMMatrixIdentity();
				SDKMeshObjectConstants objConstants;
				XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
				frameResource->CopyConstData(alphaMP->partIndex, &objConstants);
			}
		}
	}

	void SDKMeshModel::UpdateMaterialCBs(const GameTimer& gt, FrameResourceInterface* frameResource)
	{
		int idx = 0;

		for (auto& mat : m_model->materials)
		{
			SDKMeshMaterialConstants matConstants;

			matConstants.DiffuseAlbedo.x = mat.diffuseColor.x;
			matConstants.DiffuseAlbedo.y = mat.diffuseColor.y;
			matConstants.DiffuseAlbedo.z = mat.diffuseColor.z;

			matConstants.SpecularColor = mat.specularColor;
			matConstants.EmissiveColor = mat.ambientColor;
			matConstants.SpecularPower = mat.specularPower;

			frameResource->CopyMaterialData(idx, &matConstants);

			idx++;
		}
	}

	void SDKMeshModel::CreateEffect(SDKMesh::EffectPipelineStateDescription& pipeLineStateDescription)
	{
		m_effects = m_model->CreateEffect(pipeLineStateDescription);
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
	}

	void SDKMeshModel::BuildTextureResourceView(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		INT offset,
		UINT CbvSrvUavDescriptorSize)
	{
		m_HeapCPUSrv = hCpuSrv;
		m_HeapGpuSRrv = hGpuSrv;
		m_textureDescriptorOffset = offset;

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

	UINT SDKMeshModel::getTextureOffset(int textureIndex)
	{
		auto& textureName =  m_model->textureNames[textureIndex];

		auto find = m_model->mTextureCache.find(textureName);
		if (find == m_model->mTextureCache.end()) return -1;

		return find->second.slot + m_textureDescriptorOffset;
	}

}

