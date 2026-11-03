#pragma once
#include "world.hpp"

namespace voxel {
enum class MeshMode { Exposed=0, AllFaces=1 };
struct Vertex { float x,y,z,u,v,light,layer; };
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::size_t faces() const { return indices.size()/6; }
};
Mesh buildMesh(const World& world,int chunkIndex,MeshMode mode);
int textureLayer(Block block,int face);
} // namespace voxel
