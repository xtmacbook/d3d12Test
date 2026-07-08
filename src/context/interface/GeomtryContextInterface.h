#pragma once

#include "common/Util.h"

#include <unordered_map>
#include <map>
#include <memory>
#include <string>
#include <array>

struct MeshGeometry;
class Waves;
class GameTimer;

struct BoxProfile
{
	float width = 1.0;
	float height = 1.0;
	float depth = 1.0;
};

class GeometryContextInterface 
{

public:

	GeometryContextInterface();

	virtual void BuildShapeGeometry(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	virtual void updateGeometry(const GameTimer& gt);

	void BuildBox(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList, BoxProfile);
	
	void BuildLand(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	void BuildWave(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	void BuildSkull(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	void BuildMirror(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	void BuildQuad(ID3D12Device*, ID3D12GraphicsCommandList* mCommandList);

	void BuildSprites(ID3D12Device*device, ID3D12GraphicsCommandList* mCommandList);

	Waves* GetWave();

protected:
	void updateWave(const GameTimer& gt);

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>					m_Geometries;
	std::unique_ptr<Waves>															m_Waves;

};
