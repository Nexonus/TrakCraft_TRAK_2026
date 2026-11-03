#include "mesh.hpp"
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

using namespace voxel;
namespace {
int checks=0;
void check(bool ok,const char* message) {
    ++checks; if(!ok) throw std::runtime_error(message);
}
void meshTests() {
    World w(2,1);
    check(buildMesh(w,0,MeshMode::Exposed).faces()==0,"empty mesh");
    w.set(15,10,5,Block::Stone);
    auto cube=buildMesh(w,0,MeshMode::Exposed);
    check(cube.faces()==6 && cube.vertices.size()==24,"isolated cube");
    for(std::size_t face=0;face<6;++face) {
        auto v=[&](int i) { const auto& p=cube.vertices[face*4+i]; return Vec3{p.x,p.y,p.z}; };
        Vec3 center{15.5f,10.5f,5.5f};
        check(dot(cross(v(1)-v(0),v(2)-v(0)),v(0)-center)>0,"outward face winding");
    }
    w.set(16,10,5,Block::Dirt);
    check(buildMesh(w,0,MeshMode::Exposed).faces()==5,"left chunk seam hidden");
    check(buildMesh(w,1,MeshMode::Exposed).faces()==5,"right chunk seam hidden");
    check(buildMesh(w,0,MeshMode::AllFaces).faces()==6,"naive retains interior face");
    w.set(16,10,5,Block::Air);
    check(buildMesh(w,0,MeshMode::Exposed).faces()==6,"remove restores seam face");
    w.clear();
    for(int z=0;z<w.depth();++z) for(int y=0;y<worldHeight;++y) for(int x=0;x<w.width();++x) w.set(x,y,z,Block::Stone);
    auto exposed=buildMesh(w,0,MeshMode::Exposed).faces()+buildMesh(w,1,MeshMode::Exposed).faces();
    std::size_t expected=2u*(w.width()*w.depth()+w.width()*worldHeight+w.depth()*worldHeight);
    check(exposed==expected,"solid two-chunk world emits only outer shell");
    auto naive=buildMesh(w,0,MeshMode::AllFaces).faces()+buildMesh(w,1,MeshMode::AllFaces).faces();
    check(naive==std::size_t(w.width()*w.depth()*worldHeight)*6,"naive emits all six faces");
    check(textureLayer(Block::Grass,2)!=textureLayer(Block::Grass,0),"grass side and top differ");
}
void revisionTests() {
    World w(3,3);
    auto before=w.chunks();
    w.set(15,10,15,Block::Stone);
    for(int i=0;i<9;++i) check(w.chunk(i).revision-before[i].revision==std::uint64_t(i==0||i==1||i==3),"corner updates three face neighbors only");
    before=w.chunks();
    check(!w.set(15,10,15,Block::Stone),"identical write is no-op");
    check(!w.set(-1,10,15,Block::Stone),"out-of-world write rejected");
    for(int i=0;i<9;++i) check(w.chunk(i).revision==before[i].revision,"no-op does not dirty chunks");
    w.set(20,10,20,Block::Dirt);
    for(int i=0;i<9;++i) check(w.chunk(i).revision-before[i].revision==std::uint64_t(i==4),"interior write updates one chunk");
    check(w.get(-1,0,0)==Block::Air && w.get(0,worldHeight,0)==Block::Air,"outside world is air");
}
void rayTests() {
    World w(2,1); w.set(16,8,5,Block::Stone);
    auto hit=w.raycast({14.5f,8.5f,5.5f},{1,0,0});
    check(hit && hit->cell==IVec3{16,8,5} && hit->adjacent==IVec3{15,8,5},"axis-aligned DDA crosses seam");
    check(hit->hasAdjacent && std::abs(hit->distance-1.5f)<1e-5f,"ray distance and adjacent cell");
    hit=w.raycast({18.5f,8.5f,5.5f},{-1,0,0});
    check(hit && hit->adjacent==IVec3{17,8,5},"negative DDA direction");
    check(!w.raycast({14.5f,8.5f,5.5f},{1,0,0},1),"reach limit");
    check(!w.raycast({14.5f,8.5f,5.5f},{0,0,0}),"zero direction");
    hit=w.raycast({16.5f,8.5f,5.5f},{1,0,0});
    check(hit && hit->distance==0 && !hit->hasAdjacent,"ray starts inside solid");
    w.set(0,8,5,Block::Dirt);
    hit=w.raycast({-2,8.5f,5.5f},{1,0,0});
    check(hit && hit->cell==IVec3{0,8,5} && !hit->hasAdjacent,"ray from outside cannot place outside");
    hit=w.raycast({16.5f,12.5f,5.5f},{0,-1,0});
    check(hit && hit->adjacent==IVec3{16,9,5},"vertical DDA");
}
void frustumTests() {
    Mat4 m=perspective(pi/2,1,0.1f,100)*lookAt({0,0,0},{0,0,-1}); Frustum f(m);
    check(f.intersects({-1,-1,-6},{1,1,-4}),"frustum front");
    check(!f.intersects({-1,-1,4},{1,1,6}),"frustum behind");
    check(!f.intersects({20,-1,-6},{22,1,-4}),"frustum side");
    check(!f.intersects({-1,-1,-120},{1,1,-110}),"frustum far plane");
    check(f.intersects({-1,-1,-1},{1,1,1}),"frustum containing camera");
    check(f.intersects({4,-1,-6},{7,1,-4}),"frustum intersecting side plane");
    check(!f.intersects({-0.01f,-0.01f,-0.05f},{0.01f,0.01f,-0.01f}),"frustum near plane");
    Camera camera; camera.position={0,0,0}; camera.yaw=0; camera.pitch=0;
    Frustum rotated(camera.matrix(1));
    check(rotated.intersects({4,-1,-1},{6,1,1}) && !rotated.intersects({-6,-1,-1},{-4,1,1}),"rotated camera");
}
void terrainTests() {
    World a(2,2),b(2,2),c(2,2); a.generate(1337); b.generate(1337); c.generate(1338);
    bool different=false; int materials=0;
    for(int i=0;i<4;++i) {
        check(a.chunk(i).blocks==b.chunk(i).blocks,"seed reproducibility");
        different=different || a.chunk(i).blocks!=c.chunk(i).blocks;
        for(auto block:a.chunk(i).blocks) materials|=1<<int(block);
    }
    check(different,"different seeds change terrain");
    check((materials & (1<<int(Block::Stone))) && (materials & (1<<int(Block::Dirt))) && (materials & (1<<int(Block::Grass))),"at least three solid materials");
    for(int z=0;z<a.depth();++z) for(int x=0;x<a.width();++x) check(a.get(x,0,z)==Block::Stone,"solid ground floor");
}
void streamingTests() {
    for(auto [block,chunk]:{std::pair{-33,-3},{-32,-2},{-17,-2},{-16,-1},{-1,-1},{0,0},{15,0},{16,1}})
        check(blockToChunk(block)==chunk,"negative coordinates use floor division");
    World w(2,2); w.startStreaming(1337,{8,43,8},2);
    check(w.chunks().size()==25 && w.pendingChunks()==0,"initial streaming window is complete");
    check(w.contains(-32,0,-32) && !w.contains(-33,0,0),"loaded window supports negative coordinates");
    auto original=w.findChunk(-1,-1)->blocks;
    auto originalId=w.findChunk(-1,-1)->generation;
    World reference(2,2); reference.generate(1337);
    check(w.findChunk(0,0)->blocks==reference.findChunk(0,0)->blocks,"generation is independent of finite world borders");
    reference.startStreaming(1337,{24,43,8},2);
    check(w.findChunk(-1,-1)->blocks==reference.findChunk(-1,-1)->blocks,"chunk generation is independent of window center");
    check(w.set(-1,46,-1,Block::Wood),"add voxel at negative coordinates");
    check(w.set(-2,0,-2,Block::Air),"remove underground voxel at negative coordinates");
    check(w.editedBlocks()==2,"only edited voxels are retained");
    auto ray=w.raycast({-1.5f,3.5f,-1.5f},{0,-1,0});
    check(ray && ray->cell==IVec3{-2,3,-2},"DDA queries negative world coordinates");
    w.settleStreaming({1024,43,-1024});
    check(!w.findChunk(-1,-1) && w.chunks().size()==25,"distant chunks are evicted");
    check(!w.set(-1,46,-1,Block::Stone),"unloaded terrain cannot be edited");
    w.settleStreaming({8,43,8});
    check(w.get(-1,46,-1)==Block::Wood && w.get(-2,0,-2)==Block::Air,"added and removed voxels survive unloading");
    check(w.findChunk(-1,-1)->generation!=originalId,"recreated chunk has a distinct generation");
    auto restored=w.findChunk(-1,-1)->blocks;
    restored[Chunk::offset(15,46,15)]=Block::Air;
    restored[Chunk::offset(14,0,14)]=Block::Stone;
    check(restored==original,"unedited terrain is identical after return");

    // Incremental worker path: one strip of new chunks, without a blocking settle.
    auto retained=w.findChunk(0,0)->generation;
    w.updateStreaming({24,43,8});
    check(w.pendingChunks()>0,"movement queues chunks for the worker");
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(w.pendingChunks()>0 && std::chrono::steady_clock::now()<deadline) {
        w.updateStreaming({24,43,8});
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(w.pendingChunks()==0 && w.findChunk(3,0),"background generation completes the new strip");
    check(w.findChunk(0,0)->generation==retained,"movement does not regenerate retained chunks");
    for(int step=0;step<8;++step) {
        w.settleStreaming({float(step*64),43,float(-step*48)});
        check(w.chunks().size()<=49,"resident memory is bounded by radius plus hysteresis");
        for(const auto& chunk:w.chunks())
            check(w.findChunk(chunk.x,chunk.z)==&chunk,"coordinate lookup survives swap-removal");
    }
    w.updateStreaming({-4096,43,4096}); // leave a job in flight, then change both seed and center
    w.startStreaming(2333,{8,43,8},2);
    reference.startStreaming(2333,{8,43,8},2);
    check(w.editedBlocks()==0,"new world resets old edits");
    for(const auto& chunk:w.chunks()) check(chunk.blocks==reference.findChunk(chunk.x,chunk.z)->blocks,"stale worker output cannot contaminate a new seed");
    w.setStreamingRadius(3); w.settleStreaming({8,43,8});
    check(w.chunks().size()==49,"increasing radius fills a wider window");
    w.setStreamingRadius(2); w.settleStreaming({1000,43,1000});
    check(w.chunks().size()==25,"shrinking radius and moving releases old chunks");

    World seam; seam.startStreaming(1337,{8,43,8},2);
    seam.set(47,46,1,Block::Stone);
    auto revision=seam.findChunk(2,0)->revision;
    seam.settleStreaming({24,43,8});
    check(seam.findChunk(2,0)->revision==revision+1,"loading neighbor invalidates the shared boundary");
    seam.set(48,46,1,Block::Stone);
    auto hasBoundaryFace=[&]() {
        auto mesh=buildMesh(seam,seam.chunkIndex(2,0),MeshMode::Exposed);
        for(std::size_t i=0;i<mesh.vertices.size();i+=4) {
            bool match=true;
            for(int j=0;j<4;++j) { const auto& v=mesh.vertices[i+j]; match=match && v.x==48 && v.y>=46 && v.y<=47 && v.z>=1 && v.z<=2; }
            if(match) return true;
        }
        return false;
    };
    check(!hasBoundaryFace(),"newly loaded neighbor hides its shared face");
    revision=seam.findChunk(2,0)->revision;
    seam.settleStreaming({-8,43,8});
    check(!seam.findChunk(3,0) && seam.findChunk(2,0)->revision==revision+1,"unloading neighbor invalidates retained boundary");
    check(hasBoundaryFace(),"unloading neighbor restores its shared face");
}
}
int main() {
    try { meshTests(); revisionTests(); rayTests(); frustumTests(); terrainTests(); streamingTests(); std::cout<<"PASS: "<<checks<<" checks\n"; return 0; }
    catch(const std::exception& e) { std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n'; return 1; }
}
