#include "renderer.hpp"
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace voxel {
namespace {
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point t) { return std::chrono::duration<double,std::milli>(Clock::now()-t).count(); }
const char* vertexShader=R"GLSL(#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aUV;
layout(location=2) in float aLight;
layout(location=3) in float aLayer;
uniform mat4 uVP;
out vec2 uv;
out float light;
flat out float layer;
out vec3 worldPosition;
void main() {
    uv=aUV; light=aLight; layer=aLayer; worldPosition=aPosition;
    gl_Position=uVP*vec4(aPosition,1.0);
})GLSL";
const char* fragmentShader=R"GLSL(#version 330 core
in vec2 uv;
in float light;
flat in float layer;
in vec3 worldPosition;
uniform sampler2DArray uTiles;
uniform vec3 uEye;
uniform vec2 uFogRange;
out vec4 color;
void main() {
    vec3 texel=texture(uTiles,vec3(uv,layer)).rgb;
    vec3 lit=texel*light;
    float distanceToCamera=length(worldPosition-uEye);
    float fog=smoothstep(uFogRange.x,uFogRange.y,distanceToCamera);
    color=vec4(mix(lit,vec3(0.56,0.72,0.82),fog),1.0);
})GLSL";
GLuint compile(GLenum kind,const char* source) {
    GLuint shader=gl::CreateShader(kind); gl::ShaderSource(shader,1,&source,nullptr); gl::CompileShader(shader);
    GLint ok=0; gl::GetShaderiv(shader,gl::CompileStatus,&ok);
    if(!ok) { char log[4096]{}; gl::GetShaderInfoLog(shader,sizeof(log),nullptr,log); gl::DeleteShader(shader); throw std::runtime_error(std::string("Shader: ")+log); }
    return shader;
}
GLuint createProgram() {
    GLuint vs=compile(gl::VertexShader,vertexShader),fs=compile(gl::FragmentShader,fragmentShader);
    GLuint p=gl::CreateProgram(); gl::AttachShader(p,vs); gl::AttachShader(p,fs); gl::LinkProgram(p);
    gl::DeleteShader(vs); gl::DeleteShader(fs);
    GLint ok=0; gl::GetProgramiv(p,gl::LinkStatus,&ok);
    if(!ok) { char log[4096]{}; gl::GetProgramInfoLog(p,sizeof(log),nullptr,log); gl::DeleteProgram(p); throw std::runtime_error(std::string("Program: ")+log); }
    return p;
}
GLuint createTextures() {
    constexpr int size=16,layers=9;
    std::vector<unsigned char> pixels(size*size*layers*3);
    const int palette[layers][3]={{105,153,72},{128,95,63},{128,91,61},{126,135,139},{214,196,145},{119,86,51},{168,132,79},{71,124,60},{226,236,238}};
    for(int layer=0;layer<layers;++layer) for(int y=0;y<size;++y) for(int x=0;x<size;++x) {
        std::uint32_t h=std::uint32_t(x*1973+y*9277+layer*26699+7717);
        h=(h^(h>>7))*15731u; int grain=int(h%23)-11;
        int r=palette[layer][0],g=palette[layer][1],b=palette[layer][2];
        if(layer==1 && y>=13-int(h%3)) { r=105;g=153;b=72; }
        if(layer==3 && ((x+2*y)%13==0 || (x-y+16)%19==0)) grain-=22;
        if(layer==5) grain+=((x/3)%2==0?12:-16);
        if(layer==6) { int ring=std::max(std::abs(x-7),std::abs(y-7)); grain+=(ring%3==0?-30:9); }
        if(layer==7) grain+=(h%5==0?-24:8);
        int offset=((layer*size+y)*size+x)*3;
        pixels[offset]=static_cast<unsigned char>(std::clamp(r+grain,0,255));
        pixels[offset+1]=static_cast<unsigned char>(std::clamp(g+grain,0,255));
        pixels[offset+2]=static_cast<unsigned char>(std::clamp(b+grain,0,255));
    }
    GLuint texture; glGenTextures(1,&texture); glBindTexture(gl::TextureArray,texture);
    glTexParameteri(gl::TextureArray,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(gl::TextureArray,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(gl::TextureArray,GL_TEXTURE_WRAP_S,gl::ClampToEdge);
    glTexParameteri(gl::TextureArray,GL_TEXTURE_WRAP_T,gl::ClampToEdge);
    gl::TexImage3D(gl::TextureArray,0,GL_RGB,size,size,layers,0,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    return texture;
}
}
Renderer::Renderer(const World& world) {
    gl::load(); program_=createProgram(); texture_=createTextures();
    for(auto& meshes:meshes_) meshes.reserve(world.chunks().size());
    vpLocation_=gl::GetUniformLocation(program_,"uVP"); eyeLocation_=gl::GetUniformLocation(program_,"uEye");
    fogLocation_=gl::GetUniformLocation(program_,"uFogRange");
    gl::UseProgram(program_); gl::Uniform1i(gl::GetUniformLocation(program_,"uTiles"),0);
    gl::GenQueries(int(queries_.size()),queries_.data());
}
Renderer::~Renderer() {
    for(auto& meshes:meshes_) for(auto& entry:meshes) release(entry.second);
    gl::DeleteQueries(int(queries_.size()),queries_.data());
    glDeleteTextures(1,&texture_); gl::DeleteProgram(program_);
}
void Renderer::release(GpuMesh& m) {
    if(m.vao) gl::DeleteVertexArrays(1,&m.vao);
    if(m.vbo) gl::DeleteBuffers(1,&m.vbo);
    if(m.ebo) gl::DeleteBuffers(1,&m.ebo);
    m=GpuMesh{};
}
void Renderer::upload(GpuMesh& gpu,const Mesh& mesh) {
    if(!gpu.vao) {
        gl::GenVertexArrays(1,&gpu.vao); gl::GenBuffers(1,&gpu.vbo); gl::GenBuffers(1,&gpu.ebo);
    }
    gl::BindVertexArray(gpu.vao);
    gl::BindBuffer(gl::ArrayBuffer,gpu.vbo);
    gl::BufferData(gl::ArrayBuffer,std::ptrdiff_t(mesh.vertices.size()*sizeof(Vertex)),mesh.vertices.data(),gl::StaticDraw);
    gl::BindBuffer(gl::ElementBuffer,gpu.ebo);
    gl::BufferData(gl::ElementBuffer,std::ptrdiff_t(mesh.indices.size()*sizeof(std::uint32_t)),mesh.indices.data(),gl::StaticDraw);
    for(GLuint i=0;i<4;++i) gl::EnableVertexAttribArray(i);
    gl::VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
    gl::VertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(3*sizeof(float)));
    gl::VertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(5*sizeof(float)));
    gl::VertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(6*sizeof(float)));
    gpu.indices=static_cast<GLsizei>(mesh.indices.size());
    gpu.bytes=mesh.vertices.size()*sizeof(Vertex)+mesh.indices.size()*sizeof(std::uint32_t);
    gl::BindVertexArray(0);
}
int Renderer::prepare(const World& world,MeshMode mode) {
    // Purge both variants, even when one is inactive: unloaded chunks must release GPU memory.
    for(auto& cache:meshes_) for(auto it=cache.begin();it!=cache.end();) {
        if(!world.findChunk(it->first.x,it->first.z)) { release(it->second); it=cache.erase(it); }
        else ++it;
    }
    auto& meshes=meshes_[static_cast<int>(mode)]; int rebuilt=0;
    for(std::size_t i=0;i<world.chunks().size();++i) {
        const auto& chunk=world.chunk(int(i)); auto& gpu=meshes[{chunk.x,chunk.z}];
        if(gpu.revision==chunk.revision && gpu.generation==chunk.generation) continue;
        auto mesh=buildMesh(world,int(i),mode); upload(gpu,mesh);
        gpu.revision=chunk.revision; gpu.generation=chunk.generation; ++rebuilt;
    }
    return rebuilt;
}
RenderStats Renderer::draw(const World& world,const Camera& camera,int width,int height,MeshMode mode,bool culling,bool wireframe,bool blocking) {
    RenderStats stats;
    auto rebuildStart=Clock::now(); stats.rebuilt=prepare(world,mode); stats.rebuildMs=elapsed(rebuildStart);
    Mat4 vp=camera.matrix(float(width)/float(height)); Frustum frustum(vp);
    if(blocking) glFinish();
    auto start=Clock::now();
    int q=queryCursor_;
    bool canQuery=!pending_[q];
    if(pending_[q]) {
        GLint available=0; gl::GetQueryObjectiv(queries_[q],gl::QueryAvailable,&available);
        if(available || blocking) {
            std::uint64_t ns=0; gl::GetQueryObjectui64v(queries_[q],gl::QueryResult,&ns);
            lastGpuMs_=double(ns)/1e6; pending_[q]=false; canQuery=true;
        }
    }
    if(canQuery) gl::BeginQuery(gl::TimeElapsed,queries_[q]);
    glViewport(0,0,width,height);
    glClearColor(0.56f,0.72f,0.82f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);
    glPolygonMode(GL_FRONT_AND_BACK,wireframe?GL_LINE:GL_FILL);
    gl::UseProgram(program_); gl::UniformMatrix4fv(vpLocation_,1,GL_FALSE,vp.m.data());
    gl::Uniform3f(eyeLocation_,camera.position.x,camera.position.y,camera.position.z);
    float fogEnd=world.streaming()?float(world.streamingRadius()*chunkSize-4):180.f;
    gl::Uniform2f(fogLocation_,world.streaming()?fogEnd*0.55f:65.f,fogEnd);
    gl::ActiveTexture(gl::Texture0); glBindTexture(gl::TextureArray,texture_);
    const auto& meshes=meshes_[static_cast<int>(mode)];
    for(const auto& chunk:world.chunks()) {
        const auto& mesh=meshes.at({chunk.x,chunk.z}); stats.meshBytes+=mesh.bytes;
        Vec3 lo{float(chunk.x*chunkSize),0,float(chunk.z*chunkSize)},hi=lo+Vec3{chunkSize,worldHeight,chunkSize};
        if(culling && !frustum.intersects(lo,hi)) { ++stats.culled; continue; }
        if(!mesh.indices) continue;
        ++stats.visible; stats.triangles+=std::size_t(mesh.indices)/3;
        gl::BindVertexArray(mesh.vao); glDrawElements(GL_TRIANGLES,mesh.indices,GL_UNSIGNED_INT,nullptr);
    }
    gl::BindVertexArray(0); glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    if(canQuery) {
        gl::EndQuery(gl::TimeElapsed); pending_[q]=true;
        if(blocking) {
            std::uint64_t ns=0; gl::GetQueryObjectui64v(queries_[q],gl::QueryResult,&ns);
            lastGpuMs_=double(ns)/1e6; pending_[q]=false;
        }
        queryCursor_=(q+1)%int(queries_.size());
    }
    if(blocking) glFinish();
    stats.cpuMs=elapsed(start); stats.gpuMs=lastGpuMs_;
    return stats;
}
const char* Renderer::gpuName() const { return reinterpret_cast<const char*>(glGetString(GL_RENDERER)); }
void Renderer::screenshot(const std::string& path,int width,int height) const {
    // Uncompressed 24-bit TGA. Write bottom-up, matching OpenGL's framebuffer origin.
    if(width<1 || height<1 || width>65535 || height>65535) throw std::runtime_error("Invalid screenshot size");
    auto parent=std::filesystem::path(path).parent_path(); if(!parent.empty()) std::filesystem::create_directories(parent);
    std::vector<unsigned char> pixels(std::size_t(width)*height*3);
    glPixelStorei(GL_PACK_ALIGNMENT,1); glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    for(std::size_t i=0;i<pixels.size();i+=3) std::swap(pixels[i],pixels[i+2]);
    unsigned char header[18]{}; header[2]=2; header[12]=width&255; header[13]=(width>>8)&255;
    header[14]=height&255; header[15]=(height>>8)&255; header[16]=24;
    std::ofstream out(path,std::ios::binary); out.write(reinterpret_cast<char*>(header),18);
    out.write(reinterpret_cast<char*>(pixels.data()),std::streamsize(pixels.size()));
    if(!out) throw std::runtime_error("Cannot save screenshot: "+path);
}
} // namespace voxel
