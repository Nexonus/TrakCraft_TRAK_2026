#pragma once
#include "gl_api.hpp"
#include "mesh.hpp"
#include <array>
#include <string>

namespace voxel {
struct RenderStats {
    int visible=0,culled=0,rebuilt=0;
    std::size_t triangles=0,meshBytes=0;
    double cpuMs=0,gpuMs=0,rebuildMs=0;
};
class Renderer {
    struct GpuMesh {
        GLuint vao=0,vbo=0,ebo=0;
        GLsizei indices=0;
        std::size_t bytes=0;
        std::uint64_t revision=0;
        std::uint64_t generation=0;
    };
    std::array<std::unordered_map<ChunkCoord,GpuMesh,ChunkCoordHash>,2> meshes_;
    GLuint program_=0,texture_=0;
    GLint vpLocation_=-1,eyeLocation_=-1,fogLocation_=-1;
    std::array<GLuint,4> queries_{};
    std::array<bool,4> pending_{};
    int queryCursor_=0;
    double lastGpuMs_=0;
    void upload(GpuMesh& gpu,const Mesh& mesh);
    void release(GpuMesh& gpu);
public:
    explicit Renderer(const World& world);
    ~Renderer();
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    int prepare(const World& world,MeshMode mode);
    std::size_t cachedChunks(MeshMode mode) const { return meshes_[static_cast<int>(mode)].size(); }
    RenderStats draw(const World& world,const Camera& camera,int width,int height,MeshMode mode,bool culling,bool wireframe,bool blocking=false);
    const char* gpuName() const;
    void screenshot(const std::string& path,int width,int height) const;
};
} // namespace voxel
