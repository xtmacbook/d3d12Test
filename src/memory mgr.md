Resource types

`committed resource`

The basic concept of a "committed resource" (creating both virtual and physical address spaces initialized in managed physical memory) has been around since Direct3D 9, 
though the virtual addressing (VA) and physical addressing can be teased apart in Direct3D 12 to allow the app to carefully manage physical memory.

heap:`placed` and `reserved`

In addition to committed resources, the heap construct of Direct3D 12 enables two other types of resource: "placed" and "reserved". 
In Direct3D 11 a "reserved" resource was known as a "tiled resource".

reserved resources have their own unique GPU virtual address space. 
This allows a large allocation of VA space up front and then enables mapping of VA pages to certain sections of the heap later, 
and the application reconfigures the arrangement on the fly. The VA space is contiguous, and can be sparsely mapped to.
The reserved resource can be made to reference regions in the heap with API calls such as UpdateTileMappings and they can be made resident by the app by updating page
tables on the fly. When a VA range is mapped to NULL or a non-resident heap, that portion of the resource is considered non-resident.
When a VA range is mapped to a resident heap, that portion of the resource is considered resident. Heaps are resident upon creation.


Placed resources are a much simpler design, being simply a pointer to a certain region of a heap (for example, a 1Mb region for a texture in a 5Mb heap). 
Aliasing barriers enable the use of overlapping placed resources (refer to CreatePlacedResource and ResourceBarrier).

Reserved resources are not available on all Direct3D 12 hardware, and placed resources are a reasonable fallback, though placed resources must be contiguous and cannot be partially resident.

In Direct3D 12 when you allocate a heap you are creating the physical memory aspect of a committed resource.
