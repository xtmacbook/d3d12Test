//--------------------------------------------------------------------------------------
// File: Model.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// https://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "Model.h"
#include "Util.h"
#include "DDSTextureLoader.h"
#include "DirectXHelpers.h"

#include <set>


using namespace DirectX;
using Microsoft::WRL::ComPtr;

using namespace DirectX::DX12;

#if !defined(_CPPRTTI) && !defined(__GXX_RTTI)
#error Model requires RTTI
#endif


//--------------------------------------------------------------------------------------
// ModelMeshPart
//--------------------------------------------------------------------------------------

ModelMeshPart::ModelMeshPart(uint32_t ipartIndex) noexcept :
	partIndex(ipartIndex),
	materialIndex(0),
	indexCount(0),
	startIndex(0),
	vertexOffset(0),
	vertexStride(0),
	vertexCount(0),
	indexBufferSize(0),
	vertexBufferSize(0),
	primitiveType(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST),
	indexFormat(DXGI_FORMAT_R16_UINT)
{
}


ModelMeshPart::~ModelMeshPart()
{
}

void __cdecl  ModelMeshPart::Draw(ID3D12GraphicsCommandList* commandList) const
{
	if (!indexBufferSize || !vertexBufferSize)
	{
		DebugTrace("ERROR: Model part missing values for vertex and/or index buffer size (indexBufferSize %u, vertexBufferSize %u)!\n", indexBufferSize, vertexBufferSize);
		throw std::runtime_error("ModelMeshPart");
	}

	if (!staticIndexBuffer && !indexBuffer)
	{
		DebugTrace("ERROR: Model part missing index buffer!\n");
		throw std::runtime_error("ModelMeshPart");
	}

	if (!staticVertexBuffer && !vertexBuffer)
	{
		DebugTrace("ERROR: Model part missing vertex buffer!\n");
		throw std::runtime_error("ModelMeshPart");
	}

	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = staticVertexBuffer->GetGPUVirtualAddress();
	vbv.StrideInBytes = vertexStride;
	vbv.SizeInBytes = vertexBufferSize;
	commandList->IASetVertexBuffers(0, 1, &vbv);

	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = staticIndexBuffer->GetGPUVirtualAddress();
	ibv.SizeInBytes = indexBufferSize;
	ibv.Format = indexFormat;
	commandList->IASetIndexBuffer(&ibv);

	commandList->IASetPrimitiveTopology(primitiveType);

	commandList->DrawIndexedInstanced(indexCount, 1, startIndex, vertexOffset, 0);
}

void __cdecl ModelMeshPart::DrawMeshParts(ID3D12GraphicsCommandList* commandList, const Collection& meshParts)
{
	for (const auto& it : meshParts)
	{
		auto part = it.get();
		assert(part != nullptr);
		part->Draw(commandList);
	}
}

//--------------------------------------------------------------------------------------
// ModelMesh
//--------------------------------------------------------------------------------------

ModelMesh::ModelMesh() noexcept :
	boneIndex(ModelBone::c_Invalid)
{
}


ModelMesh::~ModelMesh()
{
}


void __cdecl  ModelMesh::DrawOpaque(ID3D12GraphicsCommandList* commandList) const
{
	ModelMeshPart::DrawMeshParts(commandList, opaqueMeshParts);
}

void __cdecl  ModelMesh::DrawAlpha(ID3D12GraphicsCommandList* commandList) const
{
	ModelMeshPart::DrawMeshParts(commandList, alphaMeshParts);
}


//--------------------------------------------------------------------------------------
// Model
//--------------------------------------------------------------------------------------

Model::Model() noexcept
{
}

Model::~Model()
{
}

const EffectInfo* DirectX::DX12::Model::GetMaterialInfo(const ModelMeshPart& part) const
{

	if (part.materialIndex >= materials.size())
		return nullptr;
	return &materials[part.materialIndex];
}

std::size_t Model::getTextureResouceSlotByNameIndex(int idx)
{
	auto textureName = textureNames[idx].c_str();
	auto textureEntiry = mTextureCache.find(textureName);
	return textureEntiry->second.slot;

}

std::size_t DirectX::DX12::Model::GetMeshPartCount() const
{
	std::size_t meshPartCount(0);
	for (const auto& mesh : meshes)
	{
		meshPartCount += mesh->opaqueMeshParts.size();
		meshPartCount += mesh->alphaMeshParts.size();
	}
	return meshPartCount;
}

Model::Model(Model const& other) :
	meshes(other.meshes),
	materials(other.materials),
	textureNames(other.textureNames),
	bones(other.bones),
	name(other.name)
{
	const size_t nbones = other.bones.size();
	if (nbones > 0)
	{
		if (other.boneMatrices)
		{
			boneMatrices = ModelBone::MakeArray(nbones);
			memcpy(boneMatrices.get(), other.boneMatrices.get(), sizeof(XMMATRIX) * nbones);
		}
		if (other.invBindPoseMatrices)
		{
			invBindPoseMatrices = ModelBone::MakeArray(nbones);
			memcpy(invBindPoseMatrices.get(), other.invBindPoseMatrices.get(), sizeof(XMMATRIX) * nbones);
		}
	}
}

Model& Model::operator= (Model const& rhs)
{
	if (this != &rhs)
	{
		Model tmp(rhs);
		std::swap(meshes, tmp.meshes);
		std::swap(materials, tmp.materials);
		std::swap(textureNames, tmp.textureNames);
		std::swap(bones, tmp.bones);
		std::swap(boneMatrices, tmp.boneMatrices);
		std::swap(invBindPoseMatrices, tmp.invBindPoseMatrices);
		std::swap(name, tmp.name);
	}
	return *this;
}

void __cdecl  Model::LoadStaticBuffers(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, bool keepMemory)
{
	if (!device)
		throw std::invalid_argument("Direct3D device is null");

	// Gather all unique parts
	std::set<ModelMeshPart*> uniqueParts;
	for (const auto& mesh : meshes)
	{
		for (const auto& part : mesh->opaqueMeshParts)
			uniqueParts.insert(part.get());
		for (const auto& part : mesh->alphaMeshParts)
			uniqueParts.insert(part.get());
	}

	for (auto it = uniqueParts.cbegin(); it != uniqueParts.cend(); ++it)
	{
		auto part = *it;
		// Convert dynamic VB to static VB
		if (!part->staticVertexBuffer)
		{
			if (!part->vertexBuffer)
			{
				DebugTrace("ERROR: Model part missing vertex buffer!\n");
				throw std::runtime_error("ModelMeshPart");
			}

			part->vertexBufferSize = static_cast<uint32_t>(part->vertexBuffer.Size());
			part->staticVertexBuffer = part->vertexBuffer.UpLoad(device, cmdList);

			// Scan for any other part with the same vertex buffer for sharing
			for (auto sit = std::next(it); sit != uniqueParts.cend(); ++sit)
			{
				auto sharePart = *sit;
				assert(sharePart != part);

				if (sharePart->staticVertexBuffer)
					continue;

				if (sharePart->vertexBuffer == part->vertexBuffer)
				{
					sharePart->vertexBufferSize = part->vertexBufferSize;
					sharePart->staticVertexBuffer = part->staticVertexBuffer;

					if (!keepMemory)
					{
						sharePart->vertexBuffer.Reset();
					}
				}
			}

			if (!keepMemory)
			{
				part->vertexBuffer.Reset();
			}
		}

		// Convert dynamic IB to static IB
		if (!part->staticIndexBuffer)
		{
			if (!part->indexBuffer)
			{
				DebugTrace("ERROR: Model part missing index buffer!\n");
				throw std::runtime_error("ModelMeshPart");
			}

			part->indexBufferSize = static_cast<uint32_t>(part->indexBuffer.Size());

			part->staticIndexBuffer = part->indexBuffer.UpLoad(device, cmdList);

			// Scan for any other part with the same index buffer for sharing
			for (auto sit = std::next(it); sit != uniqueParts.cend(); ++sit)
			{
				auto sharePart = *sit;
				assert(sharePart != part);

				if (sharePart->staticIndexBuffer)
					continue;

				if (sharePart->indexBuffer == part->indexBuffer)
				{
					sharePart->indexBufferSize = part->indexBufferSize;
					sharePart->staticIndexBuffer = part->staticIndexBuffer;

					if (!keepMemory)
					{
						sharePart->indexBuffer.Reset();
					}
				}
			}

			if (!keepMemory)
			{
				part->indexBuffer.Reset();
			}
		}
	}
}

void  Model::LoadTextures(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList, const wchar_t* texturesPath,
	int destinationDescriptorOffset, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
	if (textureNames.empty())
		return;

	bool mSharing = true;

	for (size_t i = 0;i < textureNames.size(); ++i)
	{
		const wchar_t* textureName = textureNames[i].c_str();

		if (textureName)
		{
			auto it = mTextureCache.find(textureName);

			TextureCacheEntry textureEntry = {};

			if (mSharing && it != mTextureCache.end())
			{
				textureEntry = it->second;
			}
			else
			{
				wchar_t fullName[MAX_PATH] = {};
				wcscpy_s(fullName, texturesPath);
				wcscat_s(fullName, textureName);

				WIN32_FILE_ATTRIBUTE_DATA fileAttr = {};
				if (!GetFileAttributesExW(fullName, GetFileExInfoStandard, &fileAttr))
				{

					if (!GetFileAttributesExW(fullName, GetFileExInfoStandard, &fileAttr))
					{
						DebugTrace("ERROR: EffectTextureFactory could not find texture file '%ls'\n", name);
						throw std::runtime_error("EffectTextureFactory::CreateTexture");
					}
				}

				wchar_t ext[_MAX_EXT] = {};
				_wsplitpath_s(textureName, nullptr, 0, nullptr, 0, nullptr, 0, ext, _MAX_EXT);
				const bool isdds = _wcsicmp(ext, L".dds") == 0;

				DDS_LOADER_FLAGS loadFlags = DDS_LOADER_DEFAULT;
				/* if (mForceSRGB)
					 loadFlags |= DDS_LOADER_FORCE_SRGB;
				 if (mAutoGenMips)
					 loadFlags |= DDS_LOADER_MIP_AUTOGEN;*/

				if (isdds)
				{
					ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(device,
						mCommandList, fullName,
						textureEntry.mResource, textureEntry.mUploadHeap, 0, nullptr,
						&textureEntry.mIsCubeMap));

				}
				else
				{
					DebugTrace("ERROR: CreateWICTextureFromFile failed (%08X) for '%ls'\n",
						static_cast<unsigned int>(0), fullName);
					throw std::runtime_error("EffectTextureFactory::CreateWICTextureFromFile");

					/* static_assert(static_cast<int>(DDS_LOADER_DEFAULT) == static_cast<int>(WIC_LOADER_DEFAULT), "DDS/WIC Load flags mismatch");
					 static_assert(static_cast<int>(DDS_LOADER_FORCE_SRGB) == static_cast<int>(WIC_LOADER_FORCE_SRGB), "DDS/WIC Load flags mismatch");
					 static_assert(static_cast<int>(DDS_LOADER_MIP_AUTOGEN) == static_cast<int>(WIC_LOADER_MIP_AUTOGEN), "DDS/WIC Load flags mismatch");
					 static_assert(static_cast<int>(DDS_LOADER_MIP_RESERVE) == static_cast<int>(WIC_LOADER_MIP_RESERVE), "DDS/WIC Load flags mismatch");

					 textureEntry.mIsCubeMap = false;

					 HRESULT hr = CreateWICTextureFromFileEx(
						 mDevice,
						 mResourceUploadBatch,
						 fullName,
						 0u,
						 D3D12_RESOURCE_FLAG_NONE,
						 static_cast<WIC_LOADER_FLAGS>(loadFlags),
						 textureEntry.mResource.ReleaseAndGetAddressOf());
					 if (FAILED(hr))
					 {
						 DebugTrace("ERROR: CreateWICTextureFromFile failed (%08X) for '%ls'\n",
							 static_cast<unsigned int>(hr), fullName);
						 throw std::runtime_error("EffectTextureFactory::CreateWICTextureFromFile");
					 }*/
				}

				textureEntry.slot = mResources.size();
				if (mSharing)
				{
					TextureCache::value_type v(textureName, textureEntry);
					mTextureCache.insert(v);
				}
				mResources.push_back(textureEntry);
			}
		}
	}
}

bool Model::testEqualMaterial() const
{
	if (materials.empty()) return false;

	bool                gperVertexColor = materials[0].perVertexColor;
	bool                genableSkinning = materials[0].enableSkinning;
	bool                genableDualTexture = materials[0].enableDualTexture;
	bool                genableNormalMaps = materials[0].enableNormalMaps;
	bool                gbiasedVertexNormals = materials[0].biasedVertexNormals;

	bool                gspecularPower = materials[0].specularPower == 0;
	bool                galphaValue = materials[0].alphaValue == 0;

	bool                gambientColor = materials[0].ambientColor.x == 0 && materials[0].ambientColor.y == 0 && materials[0].ambientColor.z == 0;
	bool                gdiffuseColor = materials[0].diffuseColor.x == 0 && materials[0].diffuseColor.y == 0 && materials[0].diffuseColor.z == 0;
	bool                gspecularColor = materials[0].specularColor.x == 0 && materials[0].specularColor.y == 0 && materials[0].specularColor.z == 0;
	bool                gemissiveColor = materials[0].emissiveColor.x == 0 && materials[0].emissiveColor.y == 0 && materials[0].emissiveColor.z == 0;

	bool                 gdiffuseTextureIndex = materials[0].diffuseTextureIndex == -1;
	bool                 gspecularTextureIndex = materials[0].specularTextureIndex == -1;
	bool                 gnormalTextureIndex = materials[0].normalTextureIndex == -1;
	bool                 gemissiveTextureIndex = materials[0].emissiveTextureIndex == -1;


	for (auto& mt : materials)
	{
		bool                perVertexColor = mt.perVertexColor;
		bool                enableSkinning = mt.enableSkinning;
		bool                enableDualTexture = mt.enableDualTexture;
		bool                enableNormalMaps = mt.enableNormalMaps;
		bool                biasedVertexNormals = mt.biasedVertexNormals;

		bool                specularPower = mt.specularPower == 0;
		bool                alphaValue = mt.alphaValue == 0;

		bool                ambientColor = mt.ambientColor.x == 0 && mt.ambientColor.y == 0 && mt.ambientColor.z == 0;
		bool                diffuseColor = mt.diffuseColor.x == 0 && mt.diffuseColor.y == 0 && mt.diffuseColor.z == 0;
		bool                specularColor = mt.specularColor.x == 0 && mt.specularColor.y == 0 && mt.specularColor.z == 0;
		bool                emissiveColor = mt.emissiveColor.x == 0 && mt.emissiveColor.y == 0 && mt.emissiveColor.z == 0;

		bool                 diffuseTextureIndex = mt.diffuseTextureIndex == -1;
		bool                 specularTextureIndex = mt.specularTextureIndex == -1;
		bool                 normalTextureIndex = mt.normalTextureIndex == -1;
		bool                 emissiveTextureIndex = mt.emissiveTextureIndex == -1;


		if (perVertexColor != gperVertexColor ||
			enableSkinning != genableSkinning ||
			enableDualTexture != genableDualTexture ||
			enableNormalMaps != genableNormalMaps ||
			biasedVertexNormals != gbiasedVertexNormals ||
			specularPower != gspecularPower ||
			alphaValue != galphaValue ||
			ambientColor != gambientColor ||
			diffuseColor != gdiffuseColor ||
			specularColor != gspecularColor ||
			emissiveColor != gemissiveColor ||
			diffuseTextureIndex != gdiffuseTextureIndex ||
			specularTextureIndex != gspecularTextureIndex ||
			normalTextureIndex != gnormalTextureIndex ||
			emissiveTextureIndex != gemissiveTextureIndex)
		{
			return false;
		}

	}

	return true;
}


std::vector< std::shared_ptr<SDKMesh::Effect> > Model::CreateEffect(SDKMesh::EffectPipelineStateDescription& pipeLineStateDescription)
{
	uint32_t partCount = 0;
	for (const auto& mesh : meshes)
	{
		for (const auto& part : mesh->opaqueMeshParts)
			partCount = (std::max)(part->partIndex + 1, partCount);
		for (const auto& part : mesh->alphaMeshParts)
			partCount = (std::max)(part->partIndex + 1, partCount);
	}

	std::vector< std::shared_ptr<SDKMesh::Effect> > effects;
	effects.resize(partCount);


	auto CreateEffectForMeshPart = [&](ModelMeshPart*part, SDKMesh::EffectPipelineStateDescription& psd) {
		
		std::shared_ptr< SDKMesh::Effect> effect(new SDKMesh::Effect);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
		ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
		opaquePsoDesc = psd.desc;

		opaquePsoDesc.InputLayout = { part->vbDecl.get()->data(), (UINT)part->vbDecl.get()->size() };
		opaquePsoDesc.VS =
		{
			reinterpret_cast<BYTE*>(psd.standardVS->GetBufferPointer()),
			psd.standardVS->GetBufferSize()
		};
		opaquePsoDesc.PS =
		{
			reinterpret_cast<BYTE*>(psd.opaquesPS->GetBufferPointer()),
			psd.opaquesPS->GetBufferSize()
		};

		ThrowIfFailed(psd.device->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&(effect->m_PSO))));

		return effect;
	};

	for (const auto& mesh : meshes)
	{
		assert(mesh != nullptr);

		for (const auto& part : mesh->opaqueMeshParts)
		{
			assert(part != nullptr);

			if (part->materialIndex == uint32_t(-1))
				continue;

			// If this fires, you have multiple parts with the same unique ID
			assert(effects[part->partIndex] == nullptr);

			effects[part->partIndex] = CreateEffectForMeshPart(part.get(), pipeLineStateDescription);
		}

		for (const auto& part : mesh->alphaMeshParts)
		{
			assert(part != nullptr);

			if (part->materialIndex == uint32_t(-1))
				continue;

			// If this fires, you have multiple parts with the same unique ID
			assert(effects[part->partIndex] == nullptr);

			effects[part->partIndex] = CreateEffectForMeshPart(part.get(), pipeLineStateDescription);

		}
	}
	
	return effects;
}



SharedGraphicsResource::SharedGraphicsResource() noexcept :mSize(0)
{
}

SharedGraphicsResource::SharedGraphicsResource(ID3D12Device* device, const VOID* Source, size_t size) :mSize(size)
{
	ThrowIfFailed(D3DCreateBlob(size, &bufferCPU));
	CopyMemory(bufferCPU->GetBufferPointer(), Source, size);

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

}

SharedGraphicsResource::SharedGraphicsResource(SharedGraphicsResource&& other) noexcept
	:mSize(other.mSize), bufferCPU(std::move(other.bufferCPU)), uploadBuffer(std::move(other.uploadBuffer))
{
}

SharedGraphicsResource&& SharedGraphicsResource::operator=(SharedGraphicsResource&& other) noexcept
{
	mSize = other.mSize;
	bufferCPU = std::move(other.bufferCPU);
	uploadBuffer = std::move(other.uploadBuffer);
	return std::move(*this);
}

SharedGraphicsResource::SharedGraphicsResource(const SharedGraphicsResource& other) noexcept
{
	mSize = other.mSize;
	bufferCPU = other.bufferCPU;
	uploadBuffer = other.uploadBuffer;
}

SharedGraphicsResource& SharedGraphicsResource::operator=(const SharedGraphicsResource& other) noexcept
{
	mSize = other.mSize;
	bufferCPU = other.bufferCPU;
	uploadBuffer = other.uploadBuffer;
	return *this;
}

Microsoft::WRL::ComPtr<ID3D12Resource>  SharedGraphicsResource::UpLoad(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
	ComPtr<ID3D12Resource> defaultBuffer;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf())
	));

	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = bufferCPU->GetBufferPointer();
	subResourceData.RowPitch = mSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

	UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	return defaultBuffer;
}

SharedGraphicsResource::~SharedGraphicsResource()
{
}
