#include "Struct.h"
#include "Geometry.h"

void RenderItem::FillWithDrawArgs(const SubmeshGeometry* subGeometry)
{
	m_IndexCount = subGeometry->m_IndexCount;
	m_StartIndexLocation = subGeometry->m_StartIndexLocation;
	m_BaseVertexLocation = subGeometry->m_BaseVertexLocation;
}
