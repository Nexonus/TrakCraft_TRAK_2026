#include "mesh.hpp"

namespace voxel {
int textureLayer(Block b,int face) {
    // Array layers: grass top, grass side, dirt, stone, sand, bark, wood top, leaves, snow.
    switch(b) {
        case Block::Grass:return face==2?0:face==3?2:1;
        case Block::Dirt:return 2;
        case Block::Stone:return 3;
        case Block::Sand:return 4;
        case Block::Wood:return face==2||face==3?6:5;
        case Block::Leaves:return 7;
        case Block::Snow:return face==3?2:8;
        default:return 3;
    }
}
Mesh buildMesh(const World& world,int chunkIndex,MeshMode mode) {
    const auto& chunk=world.chunk(chunkIndex);
    static constexpr int normals[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    // Counter-clockwise as seen from outside. Top edge of textures has v=1.
    static constexpr float corners[6][4][3]={
        {{1,0,1},{1,0,0},{1,1,0},{1,1,1}},
        {{0,0,0},{0,0,1},{0,1,1},{0,1,0}},
        {{0,1,1},{1,1,1},{1,1,0},{0,1,0}},
        {{0,0,0},{1,0,0},{1,0,1},{0,0,1}},
        {{0,0,1},{1,0,1},{1,1,1},{0,1,1}},
        {{1,0,0},{0,0,0},{0,1,0},{1,1,0}}
    };
    static constexpr float brightness[]={0.78f,0.66f,1.0f,0.45f,0.88f,0.58f};
    static constexpr float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
    Mesh mesh;
    mesh.vertices.reserve(8192); mesh.indices.reserve(12288);
    for(int y=0;y<worldHeight;++y) for(int z=0;z<chunkSize;++z) for(int x=0;x<chunkSize;++x) {
        Block block=chunk.blocks[Chunk::offset(x,y,z)]; if(block==Block::Air) continue;
        int wx=chunk.x*chunkSize+x,wz=chunk.z*chunkSize+z;
        for(int face=0;face<6;++face) {
            const auto& n=normals[face];
            // Query world coordinates, including the neighbor across a chunk seam.
            if(mode==MeshMode::Exposed && world.get(wx+n[0],y+n[1],wz+n[2])!=Block::Air) continue;
            auto base=static_cast<std::uint32_t>(mesh.vertices.size());
            for(int i=0;i<4;++i) mesh.vertices.push_back({wx+corners[face][i][0],y+corners[face][i][1],wz+corners[face][i][2],uv[i][0],uv[i][1],brightness[face],float(textureLayer(block,face))});
            for(auto i:{0u,1u,2u,0u,2u,3u}) mesh.indices.push_back(base+i);
        }
    }
    return mesh;
}
} // namespace voxel
