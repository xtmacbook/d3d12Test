
```cpp
for(UINT i = 0; i < numTriangles; ++i)
	OutputPrimitiveList = GeometryShader( T[i].vertexList );
```

Geometry shader is invoked `per primitive`.
The following code shows the general form:
```cpp
[maxvertexcount(N)] //attribute
void ShaderName (
PrimitiveType InputVertexType InputName [NumElements],
inout StreamOutputObject<OutputVertexType> OutputName)
{
// Geometry shader body...
}

```
每次调用geomery shader输出的vertex是不固定的,但是不能超过maxvertexcount.这里的maxvertexcount越小越好,

Vertex positions leaving the geometry shader must be transformed to
homogeneous `clip space`.