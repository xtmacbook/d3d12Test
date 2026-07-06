# Tessellation

<img src="https://img2024.cnblogs.com/blog/2317757/202607/2317757-20260706090738379-726594014.png">

Submit `patches` with a number of `control points`.

DX11/DX12 曲面细分管线分为三个阶段：
- Hull Shader（外壳着色器）
  - Constant Hull Shader
  - Control Point Hull Shader
- Tessellator（硬件细分器）
- Domain Shader（域着色器）

## Hull Shader

### Constant Hull Shader


- 输入: vecter shader的输出
- 输出: factors
- 每个Patch进行

他的任务是输出一个叫做`tessellation factors`的结果,这个factors控制着在tessellation阶段tessellate的细分程度.这个factors依赖与topology of patch。如下:

```cpp
struct PatchTess
{
    float EdgeTess[4] : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
    // Additional info you want associated per patch.
};

PatchTess ConstantHS (InputPatch<VertexOut, 4> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;
    // Uniformly tessellate the patch 3 times.
        pt.EdgeTess[0] = 3; // Left edge
        pt.EdgeTess[1] = 3; // Top edge
        pt.EdgeTess[2] = 3; // Right edge
        pt.EdgeTess[3] = 3; // Bottom edge
        pt.InsideTess[0] = 3; // u-axis (columns)
        pt.InsideTess[1] = 3; // v-axis (rows)
    return pt;
}

```

其中:
- EdgeTess factors: 控制了边上的tessellate
- interior factors: 控制如何去tessellate the patch

下面将知道怎么控制tessellate细化程度:
- Distance from the camera
- Screen area coverage
- Orientation
- Roughness

### Control Point Hull Shader

- 输入: control points
- 输出: control points.
- 每个control point 输出则会被调用一次

 `N-patches scheme` or `PN triangles scheme`
 只是添加了控制点来细化模型,并没有修改control points.

 ```cpp
struct HullOut
{
    float3 PosL : POSITION;
};

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 4> p,uint i : SV_OutputControlPointID, uint patchId : SV_PrimitiveID)
{
    HullOut hout;
    hout.PosL = p[i].PosL;
    return hout;
}
 ```

 其中参数: 
 - InputPatch:是这个patch所有的控制点

属性:
- domain: The patch type
- Specifies the `subdivision mode` of the tessellation
  - integer: New vertices are added/removed only at `integer` tessellation factor values
  - Fractional: New vertices are added/removed at integer tessellation factor values, but “slide” in gradually based on the fractional part of the tessellation factor.
- outputtopology : The winding order of the triangles created via subdivision
- outputcontrolpoints:The number of times the hull shader executes
- patchconstantfunc: `constant hull shader` function name

## Tessllation Stage

Tesslation Stage是硬件控制的,程序员是不需要控制的.
<img src = "https://img2024.cnblogs.com/blog/2317757/202607/2317757-20260706101428095-984878560.png"/>
<img src = "https://img2024.cnblogs.com/blog/2317757/202607/2317757-20260706101438597-1633551765.png"/>

## DOMAIN SHADER

- 输入：
  - tessellation factors,
  - 其他每个patch带有的信息(从 constant hull shader输出的)
  - $(u,v)$ coordinates of the tesselated vertex positions
  - 所有从control hull shader输出的控制点信息
- 输出：被新创建的vertex和triangles
- 每产生一个vertex被调用一次.

需要注意的是Domain shader输出的并不是tesslated vertex positons，而是$(u,v)$参数。

如:
```cpp
struct DomainOut
{
    float4 PosH : SV_POSITION;
};

// The domain shader is called for every vertex created by the tessellator.
// It is like the vertex shader after tessellation.
[domain("quad")]
DomainOut DS(PatchTess patchTess,
float2 uv : SV_DomainLocation,
const OutputPatch<HullOut, 4> quad)
{
    DomainOut dout;
// Bilinear interpolation.
    float3 v1 = lerp(quad[0].PosL, quad[1].PosL, uv.x);
    float3 v2 = lerp(quad[2].PosL, quad[3].PosL, uv.x);
    float3 p = lerp(v1, v2, uv.y);
    float4 posW = mul(float4(p, 1.0f), gWorld);
    dout.PosH = mul(posW, gViewProj);
    return dout;
}
```