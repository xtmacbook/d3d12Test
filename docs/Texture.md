
# Texture array

<img src="https://img2024.cnblogs.com/blog/2317757/202606/2317757-20260622110328762-507512605.png">


The Direct3D API:
- `array slice` to refer to an element in a texture along with its complete mipmap chain.  
- `mip slice`  to refer to all the mipmaps at a particular level in the texture array.
-  `subresource` refers to a single mipmap level in a texture array element.
-  
Given the texture array index, and a mipmap level, we can access a subresource
in a texture array. 

However, the subresources can also be labeled by a `linear index`;
Direct3D uses a linear index ordered as shown in Figure.

The following utility function is used to compute the linear subresource index
given the mip level, array index, and the number of mipmap levels:
```cpp
inline UINT D3D12CalcSubresource( UINT MipSlice, UINT ArraySlice,
UINT PlaneSlice, UINT MipLevels, UINT ArraySize )
{
	return MipSlice + ArraySlice * MipLevels + PlaneSlice * MipLevels *ArraySize;
}
```
