//--------------------------------------------------------------------------------------
// File: Model.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// https://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------

#pragma once

#ifdef _GAMING_XBOX_SCARLETT
#include <d3d12_xs.h>
#elif (defined(_XBOX_ONE) && defined(_TITLE)) || defined(_GAMING_XBOX)
#include <d3d12_x.h>
#elif defined(USING_DIRECTX_HEADERS)
#include <directx/d3d12.h>
#include <directx/dxgiformat.h>
#include <dxguids/dxguids.h>
#else
#include <d3d12.h>
#include <dxgiformat.h>
#endif

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <map>

#include <malloc.h>

#include <wrl/client.h>

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include "d3dx12.h"


namespace DirectX
{
    inline namespace DX12
    {
        struct  EffectInfo
        {
            std::wstring        name;
            bool                perVertexColor;

            bool                enableSkinning;
            bool                enableDualTexture;
            bool                enableNormalMaps;

            bool                biasedVertexNormals;

            float               specularPower;
            float               alphaValue;

            XMFLOAT3            ambientColor;
            XMFLOAT3            diffuseColor;
            XMFLOAT3            specularColor;
            XMFLOAT3            emissiveColor;

            int                 diffuseTextureIndex;
            int                 specularTextureIndex;
            int                 normalTextureIndex;
            int                 emissiveTextureIndex;

            int                 samplerIndex;
            int                 samplerIndex2;

            EffectInfo() noexcept
                : perVertexColor(false)
                , enableSkinning(false)
                , enableDualTexture(false)
                , enableNormalMaps(false)
                , biasedVertexNormals(false)
                , specularPower(0)
                , alphaValue(0)
                , ambientColor(0, 0, 0)
                , diffuseColor(0, 0, 0)
                , specularColor(0, 0, 0)
                , emissiveColor(0, 0, 0)
                , diffuseTextureIndex(-1)
                , specularTextureIndex(-1)
                , normalTextureIndex(-1)
                , emissiveTextureIndex(-1)
                , samplerIndex(-1)
                , samplerIndex2(-1)
            {
            }
        };


        class  SharedGraphicsResource
        {
        public:

            SharedGraphicsResource() noexcept;
            SharedGraphicsResource(ID3D12Device* device,const VOID* Source, size_t size);

            SharedGraphicsResource(SharedGraphicsResource&&) noexcept;
            SharedGraphicsResource&& operator= (SharedGraphicsResource&&) noexcept;

            SharedGraphicsResource(const SharedGraphicsResource&) noexcept;
            SharedGraphicsResource& operator= (const SharedGraphicsResource&) noexcept;

            explicit operator bool() const noexcept { return bufferCPU != nullptr; }

			size_t  Size() const noexcept { return mSize; }//byte size

            bool operator == (const SharedGraphicsResource& other) const noexcept { return bufferCPU.Get() == other.bufferCPU.Get(); }
            bool operator != (const SharedGraphicsResource& other) const noexcept { return bufferCPU.Get() != other.bufferCPU.Get(); }

            Microsoft::WRL::ComPtr<ID3D12Resource> UpLoad(ID3D12Device* device, ID3D12GraphicsCommandList*cmdList);

			void Reset() noexcept
			{
				bufferCPU.Reset();
				mSize = 0;
			}

            ~SharedGraphicsResource();

            Microsoft::WRL::ComPtr<ID3DBlob>        bufferCPU = nullptr;
			Microsoft::WRL::ComPtr<ID3D12Resource>  uploadBuffer = nullptr;
            size_t                                  mSize;
        };

        class ModelMesh;

        //------------------------------------------------------------------------------
        // Model loading options
        enum ModelLoaderFlags : uint32_t
        {
            ModelLoader_Default = 0x0,
            ModelLoader_MaterialColorsSRGB = 0x1,
            ModelLoader_AllowLargeModels = 0x2,
            ModelLoader_IncludeBones = 0x4,
            ModelLoader_DisableSkinning = 0x8,
        };

        //------------------------------------------------------------------------------
        // Frame hierarchy for rigid body and skeletal animation
        struct  ModelBone
        {
            ModelBone() noexcept :
                parentIndex(c_Invalid),
                childIndex(c_Invalid),
                siblingIndex(c_Invalid)
            {}

            ModelBone(uint32_t parent, uint32_t child, uint32_t sibling) noexcept :
                parentIndex(parent),
                childIndex(child),
                siblingIndex(sibling)
            {}

            uint32_t            parentIndex;
            uint32_t            childIndex;
            uint32_t            siblingIndex;
            std::wstring        name;

            using Collection = std::vector<ModelBone>;

            static constexpr uint32_t c_Invalid = uint32_t(-1);

            struct aligned_deleter { void operator()(void* p) noexcept { _aligned_free(p); } };

            using TransformArray = std::unique_ptr<XMMATRIX[], aligned_deleter>;

            static TransformArray MakeArray(size_t count)
            {
                void* temp = _aligned_malloc(sizeof(XMMATRIX) * count, 16);
                if (!temp)
                    throw std::bad_alloc();
                return TransformArray(static_cast<XMMATRIX*>(temp));
            }
        };

        //------------------------------------------------------------------------------
        // Each mesh part is a submesh with a single effect
        class  ModelMeshPart
        {
        public:
            ModelMeshPart(uint32_t partIndex) noexcept;

            ModelMeshPart(ModelMeshPart&&) = default;
            ModelMeshPart& operator= (ModelMeshPart&&) = default;

            ModelMeshPart(ModelMeshPart const&) = default;
            ModelMeshPart& operator= (ModelMeshPart const&) = default;

            virtual ~ModelMeshPart();

            using Collection = std::vector<std::unique_ptr<ModelMeshPart>>;
            using DrawCallback = std::function<void(_In_ ID3D12GraphicsCommandList* commandList, const ModelMeshPart& part)>;
            using InputLayoutCollection = std::vector<D3D12_INPUT_ELEMENT_DESC>;

            void __cdecl Draw(_In_ ID3D12GraphicsCommandList* commandList) const;
            static void __cdecl DrawMeshParts(_In_ ID3D12GraphicsCommandList* commandList, const Collection& meshParts);

            uint32_t                                                partIndex;      // Unique index assigned per-part in a model.
            
            uint32_t                                                materialIndex;  // Index of the material spec to use
            
            uint32_t                                                indexCount;
            uint32_t                                                startIndex;
            
            int32_t                                                 vertexOffset;
            uint32_t                                                vertexStride;
            uint32_t                                                vertexCount;
            
            uint32_t                                                indexBufferSize;
            uint32_t                                                vertexBufferSize; //size in byte

            D3D_PRIMITIVE_TOPOLOGY                                  primitiveType;
            DXGI_FORMAT                                             indexFormat;
            
            SharedGraphicsResource                                  indexBuffer;
            SharedGraphicsResource                                  vertexBuffer;
            
            Microsoft::WRL::ComPtr<ID3D12Resource>                  staticIndexBuffer;
            Microsoft::WRL::ComPtr<ID3D12Resource>                  staticVertexBuffer;
            
            std::shared_ptr<InputLayoutCollection>                  vbDecl;
        };


        //------------------------------------------------------------------------------
        // A mesh consists of one or more model mesh parts
        class  ModelMesh
        {
        public:
            ModelMesh() noexcept;

            ModelMesh(ModelMesh&&) = default;
            ModelMesh& operator= (ModelMesh&&) = default;

            ModelMesh(ModelMesh const&) = delete;
            ModelMesh& operator= (ModelMesh const&) = delete;

            virtual ~ModelMesh();


            void __cdecl DrawOpaque(_In_ ID3D12GraphicsCommandList* commandList) const;
            void __cdecl DrawAlpha(_In_ ID3D12GraphicsCommandList* commandList) const;

            BoundingSphere              boundingSphere;
            BoundingBox                 boundingBox;
            ModelMeshPart::Collection   opaqueMeshParts;
            ModelMeshPart::Collection   alphaMeshParts;
            uint32_t                    boneIndex;
            std::vector<uint32_t>       boneInfluences;
            std::wstring                name;

            using Collection = std::vector<std::shared_ptr<ModelMesh>>;
          
        };

        //------------------------------------------------------------------------------
        // A model consists of one or more meshes
        class  Model
        {
        public:
            Model() noexcept;

            Model(Model&&) = default;
            Model& operator= (Model&&) = default;

            Model(Model const& other);
            Model& operator= (Model const& rhs);

            virtual ~Model();

            using ModelMaterialInfo = EffectInfo;
            using ModelMaterialInfoCollection = std::vector<ModelMaterialInfo>;
            using TextureCollection = std::vector<std::wstring>;

            const EffectInfo* getMaterialInfo(const ModelMeshPart& part) const;

            std::size_t getTextureResouceSlotByNameIndex(int idx);

            // The Model::Draw* functions use variadic templates and perfect-forwarding in order to support future
            // overloads to the ModelMesh::Draw* family of functions. This means that a new ModelMesh overload can be
            // added, removed or altered, but the Model routines will still remain compatible. The correct ModelMesh
            // overload will be selected by the compiler depending on the arguments you provide to the Model method.

            static std::unique_ptr<Model> __cdecl CreateFromSDKMESH(
                _In_opt_ ID3D12Device* device,
                _In_z_ const wchar_t* szFileName,
                ModelLoaderFlags flags = ModelLoader_Default);

            static std::unique_ptr<Model> __cdecl CreateFromSDKMESH(
                _In_opt_ ID3D12Device* device,
                _In_reads_bytes_(dataSize) const uint8_t* meshData, _In_ size_t dataSize,
                ModelLoaderFlags flags = ModelLoader_Default);


            void __cdecl LoadStaticBuffers(
                _In_ ID3D12Device* device,
                _In_ ID3D12GraphicsCommandList* cmdList,
                bool keepMemory = false);

             void  __cdecl LoadTextures(
                _In_ ID3D12Device* device, ID3D12GraphicsCommandList* mCommandList,
                _In_opt_z_ const wchar_t* texturesPath = nullptr, int destinationDescriptorOffset = 0,
                D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) ;


            //test 测试这个model里的所有mesh是否material相同
             bool testEqualMaterial()const;

            ModelMesh::Collection           meshes;
            ModelMaterialInfoCollection     materials;
            TextureCollection               textureNames;
            ModelBone::Collection           bones;
            ModelBone::TransformArray       boneMatrices;
            ModelBone::TransformArray       invBindPoseMatrices;
            std::wstring                    name;

            struct TextureCacheEntry
            {
                Microsoft::WRL::ComPtr<ID3D12Resource> mResource;
                Microsoft::WRL::ComPtr<ID3D12Resource> mUploadHeap = nullptr;

                bool mIsCubeMap;
                size_t slot;

                TextureCacheEntry() noexcept : mIsCubeMap(false), slot(0) {}
            };

            using TextureCache = std::map< std::wstring, TextureCacheEntry >;
            TextureCache                   mTextureCache;

            std::vector<TextureCacheEntry> mResources; // flat list of unique resources so we can index into it

        };

 
        DEFINE_ENUM_FLAG_OPERATORS(ModelLoaderFlags)

    
    }
}
