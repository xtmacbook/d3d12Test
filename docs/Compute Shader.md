# Compute Shader


Using the GPU for non-graphical applications is called general purpose GPU (`GPGPU`) programming. For `GPGPU programming`, the user generally needs to access the computation results back on the CPU.

The `Compute Shader` is a programmable shader Direct3D exposes that is
not directly part of the rendering pipeline. Instead, it sits off to the side and can
read from GPU resources and write to GPU resources (Figure).

<img src = "https://img2024.cnblogs.com/blog/2317757/202606/2317757-20260625143842017-1738121243.png">

Essentially, the Compute Shader allows us to access the GPU to implement data-parallel
algorithms `without drawing anything`. As mentioned, this is useful for GPGPU
programming, but there are still many graphical effects that can be implemented
on the compute shader as well—so it is still very relevant for a graphics
programmer. And as already mentioned, because the Compute Shader is part
of Direct3D, it reads from and writes to Direct3D resources, which enables us to
bind the output of a compute shader directly to the rendering pipeline

For graphics, we typically use the computation result as an input to the rendering pipeline, so no transfer from GPU to CPU is needed. For example, we can blur a texture with
the compute shader, and then bind a shader resource view to that blurred texture
to a shader as input.

`Thread Groups`
在GPU programming中,线程被划分为格子状的`thread groups`.一个thread groups就是执行在一个`single multiprocessor`上.

The threads in a group can be arranged in a 1D, 2D, or 3D grid layout.

Each thread group gets shared memory that all threads in that group can
access; a thread cannot access shared memory in a different thread group. Thread
synchronization operations can take place amongst the threads in a thread group,
but different thread groups cannot be synchronized.

一个thread group 又n个线程组成.硬件会将这些线程再次划分为`warps`,每个warps里有32个线程.这个是navida显卡。(对于ATI显卡使用`wavefront`,每个wavefront有64个线程). 

A warp is processed by the multiprocessor in SIMD32(单指令同时并行执行 32 条线程). 

Each CUDA core processes a thread.

“Fermi” multiprocessor has 32 CUDA cores (so a CUDA core is like an SIMD “lane.”) 


In Direct3D, you can specify a thread group size with
dimensions that are not multiples of thirty-two, but for performance reasons, the
thread group dimensions should always be multiples of the warp size.

```hlsl


cbuffer cbSettings
{

};

Texture2D gInputA;//read only
Texture2D gInputB; //read only 

//float4 : specify the type and dimensions
RWTexture2D<float4> gOutput; //read and write

[numthreads(16, 16, 1)] //specifies the number of threads int the thread group
void CS(int3 dispatchThreadID : SV_DispatchThreadID)
{
    gOutput[dispatchThreadID.xy] = gInputA[dispatchThreadID.xy] + gInputB[dispatchThreadID.xy];
}****

```

## DATA INPUT AND OUTPUT RESOURCES
TOW TYPE:
- buffers
- textures

Outputs are treated special and have the special prefix to their type “RW,” which
stands for read-write, and as the name implies, you can read and write to elements in this resource in the compute shader.

对于输出的绑定和输入不同,我们需要一个新的view,就是`unordered access view (UAV)`

在compute shader里面不能使用`sample`函数,但可以使用`SampleLevel`函数

## THREAD IDENTIFICATION SYSTEM VALUES
<img src="https://img2024.cnblogs.com/blog/2317757/202606/2317757-20260626165727754-1843737325.png">

- `Thread Group ID` :SV_GroupID,相当于每个线程组都有一个id
- `Group Thread ID` : SV_GroupThreadID ,每个线程组里的单个线程相对于这个组有一个id
- `DispatchThreadID` : SV_DispatchThreadID,单个线程相对与所有的线程(所有组里的线程)有个id

## SHARED MEMORY AND SYNCHRONIZATION

Thread groups are given a section of so-called `shared memory` or `thread local storage`. Accessing this memory is fast and can be thought of being as fast as a
hardware cache. In the compute shader code, shared memory is declared like so:
```hlsl
    groupshared float4 gCache[256];
```
A common application of shared memory is to store `texture values` in it. Certain
algorithms, such as blurs, require fetching the same texel multiple times. Sampling
textures is actually one of the slower GPU operations. A thread group can avoid redundant texture fetches by preloading all the needed texture samples into the shared memory array.


