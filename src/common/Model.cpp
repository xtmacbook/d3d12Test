//--------------------------------------------------------------------------------------
// File: Model.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// https://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#include "Model.h"


using namespace DirectX;

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



#if defined(_MSC_VER) && !defined(_NATIVE_WCHAR_T_DEFINED)

_Use_decl_annotations_
std::unique_ptr<EffectTextureFactory> Model::LoadTextures(
    ID3D12Device* device,
    ResourceUploadBatch& resourceUploadBatch,
    const __wchar_t* texturesPath,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags) const
{
    return LoadTextures(device, resourceUploadBatch, reinterpret_cast<const unsigned short*>(texturesPath), flags);
}

#endif
