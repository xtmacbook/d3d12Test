#pragma once

#include <memory>
#include <string>

#include "Util.h"


class MeshGeometry;

class ModelIO
{
public:
	void init(ID3D12Device*, ID3D12GraphicsCommandList*);

	std::unique_ptr< MeshGeometry> GetMeshFromTxt(std::string file, DirectX::BoundingBox& box);


private:

	ID3D12Device* m_device;
	ID3D12GraphicsCommandList* m_cmdList;
};

