#pragma once
#include <GLFW/glfw3.h>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#ifndef APIENTRY
#define APIENTRY
#endif

// Small, explicit loader for just the OpenGL 3.3 entry points used by the engine.
// Dear ImGui's backend has its own independent loader.
namespace voxel::gl {
constexpr GLenum ArrayBuffer=0x8892,ElementBuffer=0x8893,StaticDraw=0x88E4;
constexpr GLenum VertexShader=0x8B31,FragmentShader=0x8B30,CompileStatus=0x8B81,LinkStatus=0x8B82;
constexpr GLenum TextureArray=0x8C1A,Texture0=0x84C0,ClampToEdge=0x812F;
constexpr GLenum TimeElapsed=0x88BF,QueryAvailable=0x8867,QueryResult=0x8866;
#define VOXEL_GL_FUNCTIONS(X) \
    X(void,GenVertexArrays,(GLsizei,GLuint*)) \
    X(void,BindVertexArray,(GLuint)) \
    X(void,DeleteVertexArrays,(GLsizei,const GLuint*)) \
    X(void,GenBuffers,(GLsizei,GLuint*)) \
    X(void,BindBuffer,(GLenum,GLuint)) \
    X(void,BufferData,(GLenum,std::ptrdiff_t,const void*,GLenum)) \
    X(void,DeleteBuffers,(GLsizei,const GLuint*)) \
    X(void,EnableVertexAttribArray,(GLuint)) \
    X(void,VertexAttribPointer,(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*)) \
    X(GLuint,CreateShader,(GLenum)) \
    X(void,ShaderSource,(GLuint,GLsizei,const char* const*,const GLint*)) \
    X(void,CompileShader,(GLuint)) \
    X(void,GetShaderiv,(GLuint,GLenum,GLint*)) \
    X(void,GetShaderInfoLog,(GLuint,GLsizei,GLsizei*,char*)) \
    X(void,DeleteShader,(GLuint)) \
    X(GLuint,CreateProgram,()) \
    X(void,AttachShader,(GLuint,GLuint)) \
    X(void,LinkProgram,(GLuint)) \
    X(void,GetProgramiv,(GLuint,GLenum,GLint*)) \
    X(void,GetProgramInfoLog,(GLuint,GLsizei,GLsizei*,char*)) \
    X(void,UseProgram,(GLuint)) \
    X(void,DeleteProgram,(GLuint)) \
    X(GLint,GetUniformLocation,(GLuint,const char*)) \
    X(void,UniformMatrix4fv,(GLint,GLsizei,GLboolean,const GLfloat*)) \
    X(void,Uniform3f,(GLint,GLfloat,GLfloat,GLfloat)) \
    X(void,Uniform1i,(GLint,GLint)) \
    X(void,Uniform2f,(GLint,GLfloat,GLfloat)) \
    X(void,ActiveTexture,(GLenum)) \
    X(void,TexImage3D,(GLenum,GLint,GLint,GLsizei,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*)) \
    X(void,GenQueries,(GLsizei,GLuint*)) \
    X(void,DeleteQueries,(GLsizei,const GLuint*)) \
    X(void,BeginQuery,(GLenum,GLuint)) \
    X(void,EndQuery,(GLenum)) \
    X(void,GetQueryObjectiv,(GLuint,GLenum,GLint*)) \
    X(void,GetQueryObjectui64v,(GLuint,GLenum,std::uint64_t*))
#define DECLARE_GL(ret,name,args) inline ret (APIENTRY *name) args = nullptr;
VOXEL_GL_FUNCTIONS(DECLARE_GL)
#undef DECLARE_GL
inline void load() {
#define LOAD_GL(ret,name,args) name=reinterpret_cast<decltype(name)>(glfwGetProcAddress("gl" #name)); if(!name) throw std::runtime_error("Missing OpenGL function: gl" #name);
    VOXEL_GL_FUNCTIONS(LOAD_GL)
#undef LOAD_GL
}
#undef VOXEL_GL_FUNCTIONS
} // namespace voxel::gl
