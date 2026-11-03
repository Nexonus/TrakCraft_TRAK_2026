#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace voxel {
constexpr float pi = 3.14159265358979323846f;
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec3 operator*(float s) const { return {x*s,y*s,z*s}; }
};
inline float dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Vec3 normalize(Vec3 v) { float n=std::sqrt(dot(v,v)); return n>0?v*(1/n):Vec3{}; }
struct IVec3 { int x=0,y=0,z=0; };
inline bool operator==(IVec3 a, IVec3 b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
struct Mat4 {
    std::array<float,16> m{}; // column-major, OpenGL convention
    float& at(int r,int c) { return m[c*4+r]; }
    float at(int r,int c) const { return m[c*4+r]; }
};
inline Mat4 operator*(const Mat4& a,const Mat4& b) {
    Mat4 c;
    for(int r=0;r<4;++r) for(int col=0;col<4;++col) for(int k=0;k<4;++k)
        c.at(r,col)+=a.at(r,k)*b.at(k,col);
    return c;
}
inline Mat4 perspective(float fov,float aspect,float nearPlane,float farPlane) {
    Mat4 m; float f=1/std::tan(fov*0.5f);
    m.at(0,0)=f/aspect; m.at(1,1)=f;
    m.at(2,2)=(farPlane+nearPlane)/(nearPlane-farPlane);
    m.at(2,3)=2*farPlane*nearPlane/(nearPlane-farPlane); m.at(3,2)=-1;
    return m;
}
inline Mat4 lookAt(Vec3 eye,Vec3 center,Vec3 up={0,1,0}) {
    Vec3 f=normalize(center-eye), s=normalize(cross(f,up)), u=cross(s,f);
    Mat4 m;
    m.at(0,0)=s.x; m.at(0,1)=s.y; m.at(0,2)=s.z; m.at(0,3)=-dot(s,eye);
    m.at(1,0)=u.x; m.at(1,1)=u.y; m.at(1,2)=u.z; m.at(1,3)=-dot(u,eye);
    m.at(2,0)=-f.x; m.at(2,1)=-f.y; m.at(2,2)=-f.z; m.at(2,3)=dot(f,eye);
    m.at(3,3)=1; return m;
}
class Frustum {
    std::array<std::array<float,4>,6> planes{};
public:
    explicit Frustum(const Mat4& clip) {
        for(int axis=0;axis<3;++axis) for(int sign=0;sign<2;++sign) {
            auto& p=planes[axis*2+sign]; float s=sign==0?1.f:-1.f;
            for(int c=0;c<4;++c) p[c]=clip.at(3,c)+s*clip.at(axis,c);
            float n=std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);
            for(float& v:p) v/=n;
        }
    }
    bool intersects(Vec3 lo,Vec3 hi) const {
        for(const auto& p:planes) {
            Vec3 v{p[0]>=0?hi.x:lo.x,p[1]>=0?hi.y:lo.y,p[2]>=0?hi.z:lo.z};
            if(p[0]*v.x+p[1]*v.y+p[2]*v.z+p[3]<0) return false;
        }
        return true;
    }
};
struct Camera {
    Vec3 position{48,40,105}; float yaw=-90, pitch=-24;
    Vec3 forward() const {
        float y=yaw*pi/180, p=pitch*pi/180;
        return {std::cos(y)*std::cos(p),std::sin(p),std::sin(y)*std::cos(p)};
    }
    Mat4 matrix(float aspect) const { return perspective(68*pi/180,aspect,0.08f,400)*lookAt(position,position+forward()); }
};
} // namespace voxel
