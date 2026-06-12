#pragma once

#include "../../common/Util.h"

#include <unordered_map>
#include <map>
#include <memory>
#include <string>
#include <array>

struct MeshGeometry;
class Waves;
class GameTimer;

class GeometryContextInterface 
{

public:

	GeometryContextInterface();

	virtual void BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	virtual void updateGeometry(const GameTimer& gt);

	void buildBox(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);
	
	void buildLand(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	void buildWave(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	Waves* getWave();

protected:
	void updateWave(const GameTimer& gt);

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>					m_Geometries;
	std::unique_ptr<Waves>															m_Waves;

};
