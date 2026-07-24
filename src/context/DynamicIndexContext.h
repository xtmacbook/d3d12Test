

/**
 * The idea of dynamic indexing is relatively straightforward. We dynamically index into an array of resources in a shader program;  The index can be specified in various ways:
1. The index can be an element in a constant buffer.
2. The index can be a system ID like SV_PrimitiveID, SV_VertexID, SV_DispatchThreadID, or SV_InstanceID.
3. The index can be the result of come calculation.
4. The index can come from a texture.
5. The index can come from a component of the vertex structure.

例如下面的代码:
cbuffer cbPerDrawIndex : register(b0)
{
    int gDiffuseTexIndex;
};
Texture2D gDiffuseMap[4] : register(t0);
float4 texValue = gDiffuseMap[gDiffuseTexIndex].Sample(gsamLinearWrap, pin.TexC);

gDiffuseTexIndex是在const buffer中的.为了减少每个render-item的descriptor设置数量.
之前我们的操作都是给每个render-item设置object constant buffer, material constant buffer, 和diffuse texture map SRV.

减少我们需要设置的描述符数量将使我们的root signature更小,这意味着每个draw call的开销更小;我们的策略如下:

我们可以将材质数据存储在结构化缓冲区中,而不是常量缓冲区中.结构化缓冲区可以在shader程序中进行索引.
我们可以在对象常量缓冲区中添加一个材质索引字段,并在每帧绑定场景中使用的所有纹理srv描述符,而不是为每个render-item绑定一个纹理srv.

 1. store material data in structed buffer instead of constant buffer. A structed buffer can be indexed in shader program.
 2. add a material index field to our object constant buffer
 3. Bind all of the texture srv descriptors used int the scene once per frame,instead of binding one texture srv per render-item.
 4. Add a DiffuseMapIndex field to the material data that specifies the texture map associated with the material. We use this to index into the array of textures we bound to the pipeline in the previous step
 * 
 */

 
#include "common/BufferStruct.h"
#include "DynamicIndexComContex.h"
#include "interface/ShadowInterface.h"

class ShadowInterface;

class DynamicIndexContext : public DynamicIndexComContext
{
public:

	enum class RenderLayer : int
	{
		Opaque = 0,
		Debug,
		Sky,
		Count
	};

	virtual bool InitDirect3D()override;
	virtual void BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList)override;
	virtual void BuildMaterials();
	virtual void BuildRenderItems();

	void BuildDescriptorHeaps();

	virtual void initTextures(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	virtual void DrawFrameResource(ID3D12CommandAllocator*) override;

	virtual void Update(const GameTimer& gt) override;

	virtual void BuildFrameResources()override;

	virtual void UpdateMainPassCB(const GameTimer& gt);

	virtual void BuildPSOs()override;

	//此时需要创建的dsvHeapDesc 需要2个Descriptors,因为有一个深度
	virtual void CreateRtvAndDsvDescriptorHeaps();

	//void DrawRenderItem(ID3D12GraphicsCommandList* cmdList, const RenderItem* ritems);

protected:
	float m_LightRotationAngle = 0.0f;

	DirectX::XMFLOAT3 m_BaseLightDirections[3] = {
		DirectX::XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
		DirectX::XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		DirectX::XMFLOAT3(0.0f, -0.707f, -0.707f)
	};
	DirectX::XMFLOAT3 m_RotatedLightDirections[3];

private:

	std::vector<RenderItem*>														m_OpaqueRitems;
	PassConstantsWithLightAndShadow													m_MainPassCB;

	std::shared_ptr< ShadowInterface>												m_shadow;

	CD3DX12_GPU_DESCRIPTOR_HANDLE													m_NullSrv;

	std::vector<RenderItem*>														m_RitemLayer[(int)RenderLayer::Count];

};


