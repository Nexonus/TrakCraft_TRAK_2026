#include "world.hpp"
#include <chrono>
#include <limits>
#include <stdexcept>

namespace voxel {
const char* blockName(Block b) {
    static const char* names[]={"Powietrze","Trawa","Ziemia","Kamien","Piasek","Drewno","Liscie","Snieg"};
    return names[static_cast<int>(b)];
}
World::World(int nx,int nz):countX_(nx),countZ_(nz) {
    if(nx<1 || nz<1 || nx>32 || nz>32) throw std::invalid_argument("Invalid world dimensions");
    chunks_.reserve(nx*nz);
    for(int z=0;z<nz;++z) for(int x=0;x<nx;++x) addChunk(Chunk(x,z));
}
int World::chunkIndex(int cx,int cz) const {
    auto it=lookup_.find({cx,cz}); return it==lookup_.end()?-1:int(it->second);
}
const Chunk* World::findChunk(int cx,int cz) const {
    int i=chunkIndex(cx,cz); return i<0?nullptr:&chunks_[std::size_t(i)];
}
bool World::contains(int x,int y,int z) const { return y>=0 && y<worldHeight && findChunk(blockToChunk(x),blockToChunk(z)); }
Block World::get(int x,int y,int z) const {
    if(y<0 || y>=worldHeight) return Block::Air;
    int cx=blockToChunk(x),cz=blockToChunk(z); const auto* c=findChunk(cx,cz);
    return c?c->blocks[Chunk::offset(x-cx*chunkSize,y,z-cz*chunkSize)]:Block::Air;
}
bool World::set(int x,int y,int z,Block b) {
    if(!contains(x,y,z)) return false;
    int cx=blockToChunk(x),cz=blockToChunk(z),lx=x-cx*chunkSize,lz=z-cz*chunkSize;
    auto& c=chunks_[std::size_t(chunkIndex(cx,cz))]; int offset=Chunk::offset(lx,y,lz); auto& old=c.blocks[offset];
    if(old==b) return false;
    old=b; ++c.revision;
    // Only face-adjacent chunks depend on this voxel. No diagonal invalidation.
    auto dirty=[&](int nx,int nz){int i=chunkIndex(nx,nz); if(i>=0) ++chunks_[std::size_t(i)].revision;};
    if(lx==0) dirty(cx-1,cz);
    if(lx==chunkSize-1) dirty(cx+1,cz);
    if(lz==0) dirty(cx,cz-1);
    if(lz==chunkSize-1) dirty(cx,cz+1);
    if(streaming_) edits_[{cx,cz}][offset]=b;
    return true;
}
void World::clear() { for(auto& c:chunks_) { std::fill(c.blocks.begin(),c.blocks.end(),Block::Air); ++c.revision; } }
namespace {
std::uint32_t hash(int x,int z,std::uint32_t seed) {
    std::uint32_t h=std::uint32_t(x)*374761393u+std::uint32_t(z)*668265263u+seed*1442695041u;
    h=(h^(h>>13))*1274126177u; return h^(h>>16);
}
float noise(float x,float z,std::uint32_t seed) {
    int ix=int(std::floor(x)),iz=int(std::floor(z));
    float fx=x-ix,fz=z-iz; fx=fx*fx*(3-2*fx); fz=fz*fz*(3-2*fz);
    auto n=[&](int a,int b){return float(hash(a,b,seed)&0xffffff)/float(0xffffff);};
    float a=n(ix,iz)*(1-fx)+n(ix+1,iz)*fx, b=n(ix,iz+1)*(1-fx)+n(ix+1,iz+1)*fx;
    return a*(1-fz)+b*fz;
}
int terrainHeight(int x,int z,std::uint32_t seed) {
    float n=0,amplitude=1,frequency=0.018f,total=0;
    for(int octave=0;octave<5;++octave) {
        n+=noise(x*frequency,z*frequency,seed+octave*101)*amplitude;
        total+=amplitude; amplitude*=0.5f; frequency*=2;
    }
    return std::clamp(int(7+30*n/total),5,worldHeight-8);
}
}
Chunk World::generateChunk(ChunkCoord coord,std::uint32_t seed) {
    Chunk chunk(coord.x,coord.z); int originX=coord.x*chunkSize,originZ=coord.z*chunkSize;
    // A two-column halo includes every possible tree whose canopy reaches this chunk.
    constexpr int padded=chunkSize+4;
    std::array<int,padded*padded> heights{};
    for(int z=-2;z<chunkSize+2;++z) for(int x=-2;x<chunkSize+2;++x)
        heights[(z+2)*padded+x+2]=terrainHeight(originX+x,originZ+z,seed);
    auto height=[&](int x,int z){return heights[(z+2)*padded+x+2];};
    auto put=[&](int x,int y,int z,Block b) {
        if(x>=0 && x<chunkSize && z>=0 && z<chunkSize && y>=0 && y<worldHeight)
            chunk.blocks[Chunk::offset(x,y,z)]=b;
    };
    for(int z=0;z<chunkSize;++z) for(int x=0;x<chunkSize;++x) {
        int h=height(x,z);
        for(int y=0;y<=h;++y) {
            Block b=y<h-3?Block::Stone:Block::Dirt;
            if(h<=18 && y>=h-3) b=Block::Sand;
            else if(y==h) b=h>=29?Block::Snow:Block::Grass;
            put(x,y,z,b);
        }
    }
    // Trunks first, then leaves: output is independent of loading order and chunk borders.
    for(int pass=0;pass<2;++pass) for(int z=-2;z<chunkSize+2;++z) for(int x=-2;x<chunkSize+2;++x) {
        int h=height(x,z);
        if(h<20 || h>=29 || hash(originX+x,originZ+z,seed+900)%181!=0) continue;
        if(pass==0) { for(int y=h+1;y<=h+4;++y) put(x,y,z,Block::Wood); continue; }
        for(int dy=3;dy<=5;++dy) for(int dz=-2;dz<=2;++dz) for(int dx=-2;dx<=2;++dx) {
            if(std::abs(dx)+std::abs(dz)+(dy==5?1:0)>3) continue;
            int lx=x+dx,lz=z+dz,y=h+dy;
            if(lx>=0 && lx<chunkSize && lz>=0 && lz<chunkSize && y<worldHeight && chunk.blocks[Chunk::offset(lx,y,lz)]==Block::Air)
                put(lx,y,lz,Block::Leaves);
        }
    }
    return chunk;
}
void World::invalidateNeighbors(ChunkCoord coord) {
    for(auto d:{ChunkCoord{1,0},ChunkCoord{-1,0},ChunkCoord{0,1},ChunkCoord{0,-1}}) {
        int index=chunkIndex(coord.x+d.x,coord.z+d.z); if(index>=0) ++chunks_[std::size_t(index)].revision;
    }
}
void World::addChunk(Chunk chunk) {
    ChunkCoord coord{chunk.x,chunk.z};
    if(findChunk(coord.x,coord.z)) throw std::logic_error("Chunk is already loaded");
    auto changes=edits_.find(coord);
    if(changes!=edits_.end()) for(auto [offset,block]:changes->second) chunk.blocks[std::size_t(offset)]=block;
    chunk.generation=++nextGeneration_;
    lookup_[coord]=chunks_.size(); chunks_.push_back(std::move(chunk)); invalidateNeighbors(coord);
}
bool World::wanted(ChunkCoord coord,int extra) const {
    return std::abs(coord.x-center_.x)<=radius_+extra && std::abs(coord.z-center_.z)<=radius_+extra;
}
void World::unloadFarChunks() {
    for(std::size_t i=0;i<chunks_.size();) {
        ChunkCoord coord{chunks_[i].x,chunks_[i].z};
        if(wanted(coord,1)) { ++i; continue; } // one-chunk hysteresis prevents border thrashing
        lookup_.erase(coord);
        if(i+1!=chunks_.size()) {
            chunks_[i]=std::move(chunks_.back()); lookup_[{chunks_[i].x,chunks_[i].z}]=i;
        }
        chunks_.pop_back(); invalidateNeighbors(coord);
    }
}
void World::rebuildRequests() {
    requests_.clear();
    for(int z=center_.z-radius_;z<=center_.z+radius_;++z) for(int x=center_.x-radius_;x<=center_.x+radius_;++x) {
        ChunkCoord coord{x,z};
        if(!findChunk(x,z) && !(pending_.valid() && pendingSeed_==seed_ && coord==pendingCoord_)) requests_.push_back(coord);
    }
    std::sort(requests_.begin(),requests_.end(),[&](ChunkCoord a,ChunkCoord b){
        int ax=a.x-center_.x,az=a.z-center_.z,bx=b.x-center_.x,bz=b.z-center_.z;
        int da=ax*ax+az*az,db=bx*bx+bz*bz;
        return da!=db?da<db:a.z!=b.z?a.z<b.z:a.x<b.x;
    });
    refreshRequests_=false;
}
void World::finishPending() {
    if(!pending_.valid()) return;
    Chunk generated=pending_.get();
    if(pendingSeed_==seed_ && wanted({generated.x,generated.z}) && !findChunk(generated.x,generated.z)) addChunk(std::move(generated));
}
void World::startStreaming(std::uint32_t seed,Vec3 position,int radius) {
    if(radius<2 || radius>6) throw std::invalid_argument("Streaming radius must be 2..6");
    // The worker captures values only; reset safely waits for an old job, if any.
    if(pending_.valid()) { pending_.wait(); pending_=std::future<Chunk>{}; }
    streaming_=true; seed_=seed; radius_=radius;
    center_={blockToChunk(int(std::floor(position.x))),blockToChunk(int(std::floor(position.z)))};
    chunks_.clear(); lookup_.clear(); edits_.clear(); requests_.clear();
    settleStreaming(position);
}
void World::updateStreaming(Vec3 position) {
    if(!streaming_) return;
    ChunkCoord next{blockToChunk(int(std::floor(position.x))),blockToChunk(int(std::floor(position.z)))};
    if(!(next==center_) || refreshRequests_) {
        center_=next; unloadFarChunks(); rebuildRequests();
    }
    if(pending_.valid() && pending_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        finishPending(); rebuildRequests();
    }
    if(!pending_.valid() && !requests_.empty()) {
        pendingCoord_=requests_.front(); requests_.erase(requests_.begin()); pendingSeed_=seed_;
        ChunkCoord coord=pendingCoord_; std::uint32_t seed=seed_;
        pending_=std::async(std::launch::async,[coord,seed]{return generateChunk(coord,seed);});
    }
}
void World::settleStreaming(Vec3 position) {
    if(!streaming_) return;
    center_={blockToChunk(int(std::floor(position.x))),blockToChunk(int(std::floor(position.z)))};
    unloadFarChunks(); finishPending(); rebuildRequests();
    auto remaining=std::move(requests_); requests_.clear();
    for(auto coord:remaining) addChunk(generateChunk(coord,seed_));
}
void World::setStreamingRadius(int radius) {
    if(radius<2 || radius>6) throw std::invalid_argument("Streaming radius must be 2..6");
    radius_=radius; refreshRequests_=true;
}
std::size_t World::editedBlocks() const {
    std::size_t count=0; for(const auto& entry:edits_) count+=entry.second.size(); return count;
}
void World::generate(std::uint32_t seed) {
    if(streaming_) { startStreaming(seed,{float(center_.x*chunkSize),0,float(center_.z*chunkSize)},radius_); return; }
    seed_=seed;
    for(auto& c:chunks_) {
        auto generated=generateChunk({c.x,c.z},seed); generated.generation=++nextGeneration_; c=std::move(generated);
    }
}
int World::surface(int x,int z) const { for(int y=worldHeight-1;y>=0;--y) if(get(x,y,z)!=Block::Air) return y; return -1; }
std::optional<RayHit> World::raycast(Vec3 origin,Vec3 dir,float reach) const {
    if(dot(dir,dir)<1e-12f || reach<0) return std::nullopt;
    dir=normalize(dir);
    IVec3 cell{int(std::floor(origin.x)),int(std::floor(origin.y)),int(std::floor(origin.z))},previous=cell;
    const float inf=std::numeric_limits<float>::infinity();
    auto step=[](float d){return d>0?1:d<0?-1:0;};
    int sx=step(dir.x),sy=step(dir.y),sz=step(dir.z);
    auto delta=[&](float d){return d==0?inf:std::abs(1/d);};
    auto boundary=[&](float o,int c,float d,int s){return s==0?inf:(float(c+(s>0?1:0))-o)/d;};
    float tx=boundary(origin.x,cell.x,dir.x,sx),ty=boundary(origin.y,cell.y,dir.y,sy),tz=boundary(origin.z,cell.z,dir.z,sz);
    float dx=delta(dir.x),dy=delta(dir.y),dz=delta(dir.z),distance=0;
    bool advanced=false;
    while(distance<=reach) {
        if(get(cell.x,cell.y,cell.z)!=Block::Air) return RayHit{cell,previous,distance,advanced && contains(previous.x,previous.y,previous.z)};
        previous=cell; advanced=true;
        if(tx<=ty && tx<=tz) { cell.x+=sx; distance=tx; tx+=dx; }
        else if(ty<=tz) { cell.y+=sy; distance=ty; ty+=dy; }
        else { cell.z+=sz; distance=tz; tz+=dz; }
    }
    return std::nullopt;
}
} // namespace voxel
