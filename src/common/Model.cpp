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
#include <set>
#include "DDSTextureLoader.h"

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
{}


ModelMeshPart::~ModelMeshPart()
{}

//--------------------------------------------------------------------------------------
// ModelMesh
//--------------------------------------------------------------------------------------

ModelMesh::ModelMesh() noexcept :
    boneIndex(ModelBone::c_Invalid)
{}


ModelMesh::~ModelMesh()
{}
 

//--------------------------------------------------------------------------------------
// Model
//--------------------------------------------------------------------------------------

Model::Model() noexcept
{}

Model::~Model()
{}

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

void  Model::LoadTextures(ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList,const wchar_t* texturesPath,
    int destinationDescriptorOffset ,D3D12_DESCRIPTOR_HEAP_FLAGS flags) 
{
	if (textureNames.empty())
		return;

    bool mSharing = false;

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
                        textureEntry.mResource, textureEntry.mUploadHeap,0,nullptr, 
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
                    TextureCache::value_type v(name, textureEntry);
                    mTextureCache.insert(v);
                }
                mResources.push_back(textureEntry);
            }
        }
	}
}


SharedGraphicsResource::SharedGraphicsResource() noexcept:mSize(0)
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

SharedGraphicsResource::SharedGraphicsResource(SharedGraphicsResource&&other) noexcept
	:mSize(other.mSize), bufferCPU(std::move(other.bufferCPU)), uploadBuffer(std::move(other.uploadBuffer))
{
}

SharedGraphicsResource&& SharedGraphicsResource::operator=(SharedGraphicsResource&&other) noexcept
{
	mSize = other.mSize;
	bufferCPU = std::move(other.bufferCPU);
	uploadBuffer = std::move(other.uploadBuffer);
	return std::move(*this);
}

SharedGraphicsResource::SharedGraphicsResource(const SharedGraphicsResource&other) noexcept
{
	mSize = other.mSize;
	bufferCPU = other.bufferCPU;
	uploadBuffer = other.uploadBuffer;
}

SharedGraphicsResource& SharedGraphicsResource::operator=(const SharedGraphicsResource&other) noexcept
{
    mSize = other.mSize;
    bufferCPU = other.bufferCPU;
    uploadBuffer = other.uploadBuffer;
    return *this;
}

Microsoft::WRL::ComPtr<ID3D12Resource>  SharedGraphicsResource::UpLoad(ID3D12Device* device, ID3D12GraphicsCommandList*cmdList)
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
