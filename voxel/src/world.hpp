#pragma once
#include "math.hpp"
#include <cstdint>
#include <future>
#include <optional>
#include <unordered_map>
#include <vector>

namespace voxel {
constexpr int chunkSize=16, worldHeight=48;
enum class Block : std::uint8_t { Air, Grass, Dirt, Stone, Sand, Wood, Leaves, Snow };
const char* blockName(Block b);
struct ChunkCoord {
    int x=0,z=0;
    bool operator==(ChunkCoord other) const { return x==other.x && z==other.z; }
};
struct ChunkCoordHash {
    std::size_t operator()(ChunkCoord p) const {
        std::uint64_t h=(std::uint64_t(std::uint32_t(p.x))<<32)|std::uint32_t(p.z);
        h=(h^(h>>30))*0xbf58476d1ce4e5b9ULL;
        h=(h^(h>>27))*0x94d049bb133111ebULL;
        return std::size_t(h^(h>>31));
    }
};
inline int blockToChunk(int x) { int q=x/chunkSize; return x%chunkSize<0?q-1:q; }
struct Chunk {
    int x,z;
    std::vector<Block> blocks;
    std::uint64_t revision=1;
    std::uint64_t generation=0; // distinguishes unloaded/recreated chunks at the same coordinates
    Chunk(int cx,int cz):x(cx),z(cz),blocks(chunkSize*worldHeight*chunkSize,Block::Air){}
    static int offset(int x,int y,int z) { return (y*chunkSize+z)*chunkSize+x; }
};
struct RayHit { IVec3 cell, adjacent; float distance; bool hasAdjacent; };
class World {
    int countX_,countZ_;
    std::vector<Chunk> chunks_;
    std::unordered_map<ChunkCoord,std::size_t,ChunkCoordHash> lookup_;
    std::unordered_map<ChunkCoord,std::unordered_map<int,Block>,ChunkCoordHash> edits_;
    std::uint64_t nextGeneration_=0;
    std::uint32_t seed_=0,pendingSeed_=0;
    bool streaming_=false,refreshRequests_=false;
    int radius_=4;
    ChunkCoord center_{},pendingCoord_{};
    std::vector<ChunkCoord> requests_;
    std::future<Chunk> pending_;
    static Chunk generateChunk(ChunkCoord coord,std::uint32_t seed);
    void addChunk(Chunk chunk);
    void unloadFarChunks();
    void rebuildRequests();
    void invalidateNeighbors(ChunkCoord coord);
    bool wanted(ChunkCoord coord,int extra=0) const;
    void finishPending();
public:
    explicit World(int countX=6,int countZ=6);
    int width() const { return countX_*chunkSize; }
    int depth() const { return countZ_*chunkSize; }
    int countX() const { return countX_; }
    int countZ() const { return countZ_; }
    bool contains(int x,int y,int z) const;
    Block get(int x,int y,int z) const;
    bool set(int x,int y,int z,Block b);
    const std::vector<Chunk>& chunks() const { return chunks_; }
    const Chunk& chunk(int index) const { return chunks_.at(index); }
    const Chunk* findChunk(int cx,int cz) const;
    int chunkIndex(int cx,int cz) const;
    void startStreaming(std::uint32_t seed,Vec3 position,int radius=4);
    void updateStreaming(Vec3 position);
    void settleStreaming(Vec3 position);
    void setStreamingRadius(int radius);
    bool streaming() const { return streaming_; }
    int streamingRadius() const { return radius_; }
    std::size_t pendingChunks() const { return requests_.size()+(pending_.valid()?1:0); }
    std::size_t editedBlocks() const;
    ChunkCoord streamCenter() const { return center_; }
    void generate(std::uint32_t seed);
    void clear();
    int surface(int x,int z) const;
    std::optional<RayHit> raycast(Vec3 origin,Vec3 direction,float reach=9) const;
};
} // namespace voxel
