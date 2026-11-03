#include "renderer.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace voxel;
namespace {
struct Options {
    std::uint32_t seed=1337;
    int chunks=6,radius=4,samples=120,frames=0;
    bool headless=false,benchmark=false,smoke=false,finite=false;
    std::string output="results/benchmark.csv",screenshot;
};
Options parseOptions(int argc,char** argv) {
    Options o;
    for(int i=1;i<argc;++i) {
        std::string arg=argv[i];
        auto value=[&]() { if(++i>=argc) throw std::runtime_error("Missing value for "+arg); return std::string(argv[i]); };
        if(arg=="--seed") o.seed=static_cast<std::uint32_t>(std::stoul(value()));
        else if(arg=="--chunks") o.chunks=std::stoi(value());
        else if(arg=="--radius") o.radius=std::stoi(value());
        else if(arg=="--finite") o.finite=true;
        else if(arg=="--samples") o.samples=std::stoi(value());
        else if(arg=="--frames") o.frames=std::stoi(value());
        else if(arg=="--output") o.output=value();
        else if(arg=="--screenshot") o.screenshot=value();
        else if(arg=="--benchmark") o.benchmark=true;
        else if(arg=="--headless") o.headless=true;
        else if(arg=="--smoke-test") { o.smoke=true; o.headless=true; o.frames=12; }
        else throw std::runtime_error("Unknown option: "+arg);
    }
    if(o.chunks<2 || o.chunks>12 || o.radius<2 || o.radius>6 || o.samples<20 || o.samples>10000 || o.frames<0) throw std::runtime_error("Use chunks 2..12, radius 2..6, samples 20..10000, frames >= 0");
    if(o.headless && !o.benchmark && o.frames==0) o.frames=60;
    return o;
}
struct Sample { double renderMs,gpuMs; };
struct Result {
    double renderMean=0,renderP95=0,gpuMean=0,gpuP95=0;
    std::size_t triangles=0,bytes=0;
    int visible=0;
};
struct Benchmark {
    bool active=false,complete=false;
    int mode=0,warmup=0,width=0,height=0;
    Camera camera;
    bool culling=true;
    std::array<std::vector<Sample>,2> samples;
    std::array<Result,2> results;
    void start(Renderer& renderer,World& world,const Camera& view,int w,int h,bool cull) {
        world.settleStreaming(view.position);
        renderer.prepare(world,MeshMode::Exposed); renderer.prepare(world,MeshMode::AllFaces);
        glFinish(); *this=Benchmark{}; active=true; camera=view; width=w; height=h; culling=cull;
        glfwSwapInterval(0);
    }
    void record(const RenderStats& stats,int sampleCount) {
        if(++warmup<=20) return;
        samples[mode].push_back({stats.cpuMs,stats.gpuMs});
        if(int(samples[mode].size())<sampleCount) return;
        auto& r=results[mode]; r.triangles=stats.triangles; r.visible=stats.visible; r.bytes=stats.meshBytes;
        std::vector<double> cpu,gpu;
        for(auto s:samples[mode]) { cpu.push_back(s.renderMs); gpu.push_back(s.gpuMs); }
        auto mean=[](const auto& v){ return std::accumulate(v.begin(),v.end(),0.0)/double(v.size()); };
        r.renderMean=mean(cpu); r.gpuMean=mean(gpu);
        std::sort(cpu.begin(),cpu.end()); std::sort(gpu.begin(),gpu.end());
        auto p95=std::size_t(std::ceil(cpu.size()*0.95))-1;
        r.renderP95=cpu[p95]; r.gpuP95=gpu[p95];
        if(mode==0) { mode=1; warmup=0; } else { active=false; complete=true; }
    }
    void save(const Options& options,const Renderer& renderer) const {
        auto parent=std::filesystem::path(options.output).parent_path(); if(!parent.empty()) std::filesystem::create_directories(parent);
        std::ofstream out(options.output);
        out<<"mode,seed,camera_x,camera_y,camera_z,yaw,pitch,fov,width,height,frustum_culling,backface_culling,samples,visible_chunks,triangles,mesh_bytes,render_mean_ms,render_p95_ms,gpu_mean_ms,gpu_p95_ms,gpu,streaming,radius\n";
        out<<std::fixed<<std::setprecision(6);
        for(int i=0;i<2;++i) {
            const auto& r=results[i];
            out<<(i==0?"exposed":"all_faces")<<','<<options.seed<<','<<camera.position.x<<','<<camera.position.y<<','<<camera.position.z<<','<<camera.yaw<<','<<camera.pitch<<",68,"<<width<<','<<height<<','<<culling<<",1,"<<samples[i].size()<<','<<r.visible<<','<<r.triangles<<','<<r.bytes<<','<<r.renderMean<<','<<r.renderP95<<','<<r.gpuMean<<','<<r.gpuP95<<",\""<<renderer.gpuName()<<"\","<<!options.finite<<','<<options.radius<<'\n';
        }
        if(!out) throw std::runtime_error("Cannot write "+options.output);
        auto rawPath=std::filesystem::path(options.output); rawPath.replace_filename(rawPath.stem().string()+"_samples.csv");
        std::ofstream raw(rawPath); raw<<"mode,sample,render_ms,gpu_ms\n"<<std::fixed<<std::setprecision(6);
        for(int m=0;m<2;++m) for(std::size_t i=0;i<samples[m].size();++i) raw<<(m==0?"exposed":"all_faces")<<','<<i<<','<<samples[m][i].renderMs<<','<<samples[m][i].gpuMs<<'\n';
        if(!raw) throw std::runtime_error("Cannot write benchmark samples");
    }
};
struct Controls {
    bool captured=false,firstMouse=true;
    double mouseX=0,mouseY=0;
    std::array<bool,GLFW_KEY_LAST+1> previousKeys{};
    std::array<bool,3> previousButtons{};
    bool pressed(GLFWwindow* w,int key) {
        bool down=glfwGetKey(w,key)==GLFW_PRESS,edge=down&&!previousKeys[key]; previousKeys[key]=down; return edge;
    }
    void capture(GLFWwindow* w,bool value) {
        captured=value; firstMouse=true; glfwSetInputMode(w,GLFW_CURSOR,value?GLFW_CURSOR_DISABLED:GLFW_CURSOR_NORMAL);
        if(glfwRawMouseMotionSupported()) glfwSetInputMode(w,GLFW_RAW_MOUSE_MOTION,value?GLFW_TRUE:GLFW_FALSE);
    }
};
void home(Camera& camera,const World& world) {
    if(world.streaming()) { camera.position={8,43,8}; camera.yaw=-115; camera.pitch=-22; return; }
    camera.position={world.width()*0.66f,43.f,world.depth()+22.f};
    Vec3 target{world.width()*0.5f,19.f,world.depth()*0.48f}; Vec3 d=normalize(target-camera.position);
    camera.yaw=std::atan2(d.z,d.x)*180/pi; camera.pitch=std::asin(d.y)*180/pi;
}
void styleUI() {
    auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.LogFilename=nullptr;
    static const ImWchar ranges[]={0x20,0x17f,0x2000,0x206f,0};
    const char* fonts[]={"C:/Windows/Fonts/segoeui.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf","/System/Library/Fonts/Supplemental/Arial.ttf"};
    for(auto font:fonts) if(std::filesystem::exists(font)) { io.Fonts->AddFontFromFileTTF(font,17,nullptr,ranges); break; }
    if(io.Fonts->Fonts.empty()) io.Fonts->AddFontDefault();
    auto& s=ImGui::GetStyle(); ImGui::StyleColorsDark();
    s.WindowRounding=12; s.FrameRounding=6; s.GrabRounding=5; s.PopupRounding=8;
    s.WindowBorderSize=1; s.FrameBorderSize=0; s.WindowPadding={18,16}; s.ItemSpacing={9,9}; s.FramePadding={10,6};
    s.Colors[ImGuiCol_WindowBg]={0.045f,0.065f,0.080f,0.94f};
    s.Colors[ImGuiCol_Border]={0.22f,0.30f,0.33f,0.6f};
    s.Colors[ImGuiCol_Button]={0.12f,0.20f,0.23f,1};
    s.Colors[ImGuiCol_ButtonHovered]={0.18f,0.37f,0.36f,1};
    s.Colors[ImGuiCol_ButtonActive]={0.20f,0.48f,0.40f,1};
    s.Colors[ImGuiCol_CheckMark]={0.50f,0.86f,0.64f,1};
    s.Colors[ImGuiCol_Header]={0.16f,0.29f,0.28f,1};
    s.Colors[ImGuiCol_Text]={0.90f,0.94f,0.94f,1};
    s.Colors[ImGuiCol_TextDisabled]={0.50f,0.61f,0.64f,1};
}
constexpr ImGuiWindowFlags panelFlags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings;
void label(const char* text) { ImGui::TextColored({0.50f,0.86f,0.64f,1},"%s",text); }
void metric(const char* name,const std::string& value) {
    ImGui::TextDisabled("%s",name); ImGui::SameLine(165); ImGui::TextUnformatted(value.c_str());
}
std::string number(double value,int decimals=2) { std::ostringstream out; out<<std::fixed<<std::setprecision(decimals)<<value; return out.str(); }
void selectionOutline(const RayHit& hit,const Camera& camera,int width,int height) {
    Mat4 vp=camera.matrix(float(width)/height); ImVec2 points[8];
    for(int i=0;i<8;++i) {
        Vec3 p{hit.cell.x+((i&1)?1.004f:-0.004f),hit.cell.y+((i&2)?1.004f:-0.004f),hit.cell.z+((i&4)?1.004f:-0.004f)};
        float w=vp.at(3,0)*p.x+vp.at(3,1)*p.y+vp.at(3,2)*p.z+vp.at(3,3); if(w<=0.01f) return;
        float x=vp.at(0,0)*p.x+vp.at(0,1)*p.y+vp.at(0,2)*p.z+vp.at(0,3);
        float y=vp.at(1,0)*p.x+vp.at(1,1)*p.y+vp.at(1,2)*p.z+vp.at(1,3);
        points[i]={(x/w*0.5f+0.5f)*width,(-y/w*0.5f+0.5f)*height};
    }
    auto* draw=ImGui::GetBackgroundDrawList();
    for(int i=0;i<8;++i) for(int axis=1;axis<=4;axis*=2) if(!(i&axis)) draw->AddLine(points[i],points[i|axis],IM_COL32(255,239,174,255),2);
}
void smokeTest(Renderer& renderer,World& world,GLFWwindow* window) {
    renderer.prepare(world,MeshMode::Exposed); renderer.prepare(world,MeshMode::AllFaces);
    Block old=world.get(15,46,15); world.set(15,46,15,Block::Stone);
    if(renderer.prepare(world,MeshMode::Exposed)!=3 || renderer.prepare(world,MeshMode::AllFaces)!=3) throw std::runtime_error("Smoke: boundary edit did not rebuild 3 chunks in each mode");
    world.set(15,46,15,old);
    if(renderer.prepare(world,MeshMode::Exposed)!=3 || renderer.prepare(world,MeshMode::AllFaces)!=3) throw std::runtime_error("Smoke: restore did not rebuild 3 chunks");
    if(renderer.prepare(world,MeshMode::Exposed)!=0) throw std::runtime_error("Smoke: unchanged world rebuilt");
    std::cout<<"PASS: GPU mesh uploads and boundary edit invalidation\n";
    int width,height; glfwGetFramebufferSize(window,&width,&height);
    Camera camera; home(camera,world);
    std::vector<unsigned char> exposed(std::size_t(width)*height*3),naive(exposed.size());
    std::vector<float> depthExposed(std::size_t(width)*height),depthNaive(depthExposed.size());
    glPixelStorei(GL_PACK_ALIGNMENT,1);
    auto stats=renderer.draw(world,camera,width,height,MeshMode::Exposed,true,false,true);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,exposed.data());
    glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depthExposed.data());
    renderer.draw(world,camera,width,height,MeshMode::AllFaces,true,false,true);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,naive.data());
    glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depthNaive.data());
    // Primitive order can change nearest-texel selection at subpixel boundaries.
    // Compare image error and depth rather than requiring bit-identical sampling.
    double colorError=0,depthError=0;
    for(std::size_t i=0;i<exposed.size();++i) colorError+=std::abs(int(exposed[i])-int(naive[i]));
    for(std::size_t i=0;i<depthExposed.size();++i) depthError+=std::abs(depthExposed[i]-depthNaive[i]);
    colorError/=double(exposed.size()); depthError/=double(depthExposed.size());
    if(colorError>0.1 || depthError>1e-7) throw std::runtime_error("Smoke: A/B image error too large (color="+std::to_string(colorError)+", depth="+std::to_string(depthError)+")");
    camera.position={100000,80,100000}; camera.yaw=90; camera.pitch=0;
    auto away=renderer.draw(world,camera,width,height,MeshMode::Exposed,true,false,true);
    if(away.visible!=0 || away.triangles!=0 || stats.visible==0) throw std::runtime_error("Smoke: frustum visibility is incorrect");
    auto uncull=renderer.draw(world,camera,width,height,MeshMode::Exposed,false,false,true);
    if(uncull.visible!=int(world.chunks().size())) throw std::runtime_error("Smoke: disabling frustum culling lost chunks");
    std::cout<<"PASS: A/B color MAE "<<colorError<<"/255, depth MAE "<<depthError<<", frustum rejection and culling toggle\n";
    if(world.streaming()) {
        world.set(-1,46,-1,Block::Wood);
        world.settleStreaming({512,43,-512});
        camera.position={512,43,-512}; camera.yaw=-115; camera.pitch=-22;
        auto moved=renderer.draw(world,camera,width,height,MeshMode::Exposed,true,false,true);
        if(!moved.visible || renderer.cachedChunks(MeshMode::AllFaces)!=0 || renderer.cachedChunks(MeshMode::Exposed)!=world.chunks().size())
            throw std::runtime_error("Smoke: moving did not replace/release GPU chunk caches");
        for(int step=0;step<48;++step) {
            camera.position.x-=2; camera.position.z-=1;
            world.updateStreaming(camera.position);
            renderer.draw(world,camera,width,height,MeshMode::Exposed,true,false,true);
            std::size_t limit=std::size_t(2*world.streamingRadius()+3);
            if(world.chunks().size()>limit*limit || renderer.cachedChunks(MeshMode::Exposed)>limit*limit)
                throw std::runtime_error("Smoke: continuous flight exceeded the chunk memory bound");
        }
        world.settleStreaming({8,43,8});
        if(world.get(-1,46,-1)!=Block::Wood) throw std::runtime_error("Smoke: edited voxel was lost on return");
        world.set(-1,46,-1,Block::Air);
        renderer.prepare(world,MeshMode::Exposed);
        std::cout<<"PASS: GPU chunk eviction, 48 streaming flight frames and edited voxel restored on return\n";
    }
}
int run(GLFWwindow* window,Options options) {
    World world(options.chunks,options.chunks);
    if(options.finite) world.generate(options.seed); else world.startStreaming(options.seed,{8,43,8},options.radius);
    Renderer renderer(world); Camera camera; home(camera,world); Controls controls;
    std::cout<<"GPU: "<<renderer.gpuName()<<"\nOpenGL: "<<glGetString(GL_VERSION)<<'\n';
    if(options.smoke) smokeTest(renderer,world,window);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); styleUI();
    ImGui_ImplGlfw_InitForOpenGL(window,true); ImGui_ImplOpenGL3_Init("#version 330");
    struct UiCleanup { ~UiCleanup() { ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext(); } } uiCleanup;
    bool culling=true,wireframe=false,vsync=true,showUI=true;
    MeshMode mode=MeshMode::Exposed; Block selected=Block::Grass;
    Benchmark benchmark; bool requestBenchmark=options.benchmark; int seedInput=int(options.seed),frame=0,afterBenchmark=0,lastRebuilt=0;
    bool autoExit=options.benchmark; std::string status="Scena gotowa. Tab przełącza sterowanie kamerą.";
    std::array<float,120> frameHistory{}; int historyOffset=0;
    double lastTime=glfwGetTime(),smoothMs=16.67; RenderStats stats;
    int screenshotRequest=0;
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        int width,height; glfwGetFramebufferSize(window,&width,&height);
        if(width<=0 || height<=0) { glfwWaitEventsTimeout(0.1); lastTime=glfwGetTime(); continue; }
        double now=glfwGetTime(),dt=now-lastTime; lastTime=now;
        smoothMs=smoothMs*0.92+dt*1000*0.08;
        frameHistory[historyOffset]=float(dt*1000); historyOffset=(historyOffset+1)%int(frameHistory.size());
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        auto& io=ImGui::GetIO();
        bool tab=controls.pressed(window,GLFW_KEY_TAB),escape=controls.pressed(window,GLFW_KEY_ESCAPE);
        bool f1=controls.pressed(window,GLFW_KEY_F1),f2=controls.pressed(window,GLFW_KEY_F2),f3=controls.pressed(window,GLFW_KEY_F3),f4=controls.pressed(window,GLFW_KEY_F4);
        bool f7=controls.pressed(window,GLFW_KEY_F7),f12=controls.pressed(window,GLFW_KEY_F12),homeKey=controls.pressed(window,GLFW_KEY_HOME);
        if(escape) controls.capture(window,false);
        if(!io.WantTextInput && !benchmark.active) {
            if(tab) controls.capture(window,!controls.captured);
            if(f1) showUI=!showUI;
            if(f2) mode=mode==MeshMode::Exposed?MeshMode::AllFaces:MeshMode::Exposed;
            if(f3) culling=!culling;
            if(f4) wireframe=!wireframe;
            if(f7) requestBenchmark=true;
            if(homeKey) home(camera,world);
            for(int i=0;i<7;++i) if(controls.pressed(window,GLFW_KEY_1+i)) selected=static_cast<Block>(i+1);
        }
        if(f12) screenshotRequest=1;
        double mouseX,mouseY; glfwGetCursorPos(window,&mouseX,&mouseY);
        if(controls.captured && !benchmark.active) {
            if(!controls.firstMouse) { camera.yaw+=float(mouseX-controls.mouseX)*0.1f; camera.pitch=std::clamp(camera.pitch-float(mouseY-controls.mouseY)*0.1f,-89.f,89.f); }
            Vec3 forward=camera.forward(),right=normalize(cross(forward,{0,1,0})),movement{};
            auto down=[&](int key){return glfwGetKey(window,key)==GLFW_PRESS;};
            if(down(GLFW_KEY_W)) movement=movement+forward;
            if(down(GLFW_KEY_S)) movement=movement-forward;
            if(down(GLFW_KEY_D)) movement=movement+right;
            if(down(GLFW_KEY_A)) movement=movement-right;
            if(down(GLFW_KEY_SPACE)) movement.y+=1;
            if(down(GLFW_KEY_LEFT_CONTROL)||down(GLFW_KEY_C)) movement.y-=1;
            float speed=down(GLFW_KEY_LEFT_SHIFT)?32.f:10.f;
            camera.position=camera.position+normalize(movement)*(float(std::min(dt,0.05))*speed);
            camera.position.y=std::clamp(camera.position.y,0.2f,160.f);
            if(!world.streaming()) {
                camera.position.x=std::clamp(camera.position.x,-100.f,float(world.width()+100));
                camera.position.z=std::clamp(camera.position.z,-100.f,float(world.depth()+100));
            }
        }
        controls.mouseX=mouseX; controls.mouseY=mouseY; controls.firstMouse=false;
        if(!benchmark.active) world.updateStreaming(camera.position);
        auto hit=world.raycast(camera.position,camera.forward());
        for(int button=0;button<3;++button) {
            bool down=glfwGetMouseButton(window,button)==GLFW_PRESS,pressed=down&&!controls.previousButtons[button]; controls.previousButtons[button]=down;
            if(!pressed || !controls.captured || benchmark.active || !hit) continue;
            if(button==GLFW_MOUSE_BUTTON_LEFT) { world.set(hit->cell.x,hit->cell.y,hit->cell.z,Block::Air); benchmark.complete=false; }
            if(button==GLFW_MOUSE_BUTTON_RIGHT && hit->hasAdjacent) {
                IVec3 p=hit->adjacent;
                if(!(int(std::floor(camera.position.x))==p.x && int(std::floor(camera.position.y))==p.y && int(std::floor(camera.position.z))==p.z)) {
                    world.set(p.x,p.y,p.z,selected); benchmark.complete=false;
                }
            }
            if(button==GLFW_MOUSE_BUTTON_MIDDLE) selected=world.get(hit->cell.x,hit->cell.y,hit->cell.z);
        }
        if(benchmark.active && (width!=benchmark.width || height!=benchmark.height)) {
            benchmark.active=false; glfwSwapInterval(vsync?1:0); status="Pomiar przerwany: zmiana rozmiaru okna. Uruchom ponownie F7.";
            if(autoExit) throw std::runtime_error("Benchmark framebuffer size changed");
        }
        if(requestBenchmark && !benchmark.active) {
            controls.capture(window,false);
            benchmark.start(renderer,world,camera,width,height,culling); requestBenchmark=false;
            status="Pomiar A/B: kamera i scena zablokowane, VSync wyłączony.";
        }
        bool measuring=benchmark.active;
        MeshMode renderMode=measuring?static_cast<MeshMode>(benchmark.mode):mode;
        const Camera& renderCamera=measuring?benchmark.camera:camera;
        stats=renderer.draw(world,renderCamera,width,height,renderMode,measuring?benchmark.culling:culling,measuring?false:wireframe,measuring);
        if(stats.rebuilt) lastRebuilt=stats.rebuilt;
        if(measuring) {
            benchmark.record(stats,options.samples);
            if(benchmark.complete) {
                benchmark.save(options,renderer); glfwSwapInterval(vsync?1:0);
                status="Zapisano porównanie: "+options.output;
                std::cout<<"Benchmark saved: "<<options.output<<'\n';
                for(int i=0;i<2;++i) std::cout<<(i==0?"exposed":"all_faces")<<": "<<benchmark.results[i].triangles<<" triangles, GPU "<<benchmark.results[i].gpuMean<<" ms\n";
            }
        }
        if(hit && controls.captured) selectionOutline(*hit,camera,int(io.DisplaySize.x),int(io.DisplaySize.y));
        if(showUI) {
            ImGui::SetNextWindowPos({20,20}); ImGui::SetNextWindowSize({300,0});
            ImGui::Begin("World",nullptr,panelFlags);
            label("V O X E L  /  L A B"); ImGui::TextDisabled("C++17  /  OPENGL 3.3");
            ImGui::Spacing(); ImGui::TextUnformatted("Renderer świata voxelowego");
            if(world.streaming()) {
                auto center=world.streamCenter();
                ImGui::TextDisabled("Streaming  |  chunk (%d, %d)",center.x,center.z);
                ImGui::TextDisabled("Pozycja: %.0f, %.0f, %.0f",camera.position.x,camera.position.y,camera.position.z);
            } else ImGui::TextDisabled("%d × %d × %d bloków  |  %zu chunków",world.width(),worldHeight,world.depth(),world.chunks().size());
            ImGui::Separator();
            ImGui::BeginDisabled(benchmark.active);
            if(ImGui::Button(controls.captured?"Uwolnij kursor  [Tab]":"Wejdź do świata  [Tab]",{-1,36})) controls.capture(window,!controls.captured);
            bool all=mode==MeshMode::AllFaces;
            if(ImGui::Checkbox("Wszystkie ściany  [F2]",&all)) mode=all?MeshMode::AllFaces:MeshMode::Exposed;
            ImGui::Checkbox("Frustum culling  [F3]",&culling);
            ImGui::Checkbox("Siatka trójkątów  [F4]",&wireframe);
            if(ImGui::Checkbox("VSync",&vsync)) glfwSwapInterval(vsync?1:0);
            ImGui::Spacing(); ImGui::TextDisabled("GENERATOR TERENU");
            if(world.streaming()) {
                int radius=world.streamingRadius(); ImGui::SetNextItemWidth(155);
                if(ImGui::SliderInt("Zasięg",&radius,2,6,"%d chunków")) { world.setStreamingRadius(radius); options.radius=radius; benchmark.complete=false; }
            }
            ImGui::SetNextItemWidth(130); ImGui::InputInt("Seed",&seedInput,0);
            if(ImGui::Button("Nowy teren",{125,0})) {
                options.seed=std::uint32_t(seedInput);
                if(world.streaming()) world.startStreaming(options.seed,{8,43,8},world.streamingRadius()); else world.generate(options.seed);
                home(camera,world); benchmark.complete=false;
                status="Wygenerowano teren, seed "+std::to_string(options.seed);
            }
            ImGui::SameLine(); if(ImGui::Button("Kamera [Home]",{-1,0})) home(camera,world);
            ImGui::EndDisabled();
            ImGui::Separator(); ImGui::TextDisabled("STEROWANIE");
            ImGui::TextUnformatted("WASD  ruch    ·    Mysz  rozglądanie");
            ImGui::TextUnformatted("Spacja / Ctrl  góra / dół");
            ImGui::TextUnformatted("Shift  szybciej    ·    1–7  materiał");
            ImGui::TextUnformatted("LPM  usuń    ·    PPM  dodaj");
            ImGui::TextUnformatted("ŚPM  pobierz materiał    ·    Esc  kursor");
            ImGui::TextDisabled("F1  interfejs    ·    F12  zrzut TGA");
            ImGui::End();

            ImGui::SetNextWindowPos({io.DisplaySize.x-320,20}); ImGui::SetNextWindowSize({300,0});
            ImGui::Begin("Performance",nullptr,panelFlags);
            label("STATYSTYKI NA ŻYWO");
            ImGui::Text("%.0f FPS",1000.0/std::max(smoothMs,0.01));
            metric("Klatka",number(smoothMs)+" ms");
            metric("Render CPU*",number(stats.cpuMs)+" ms");
            metric("Render GPU",number(stats.gpuMs)+" ms");
            ImGui::PlotLines("##frame",frameHistory.data(),int(frameHistory.size()),historyOffset,nullptr,0,40,{-1,46});
            ImGui::Separator();
            metric("Widoczne chunki",std::to_string(stats.visible)+" / "+std::to_string(world.chunks().size()));
            metric("Odrzucone",std::to_string(stats.culled));
            if(world.streaming()) {
                metric("Kolejka chunków",std::to_string(world.pendingChunks()));
                metric("Zmienione bloki",std::to_string(world.editedBlocks()));
            }
            metric("Trójkąty",std::to_string(stats.triangles));
            metric("Siatka w GPU",number(double(stats.meshBytes)/1048576,1)+" MiB");
            metric("Przebudowa teraz",std::to_string(stats.rebuilt)+" chunków");
            metric("Ostatnia zmiana",std::to_string(lastRebuilt)+" chunków");
            ImGui::TextDisabled("* CPU: %s",measuring?"z oczekiwaniem na GPU":"wysłanie poleceń");
            ImGui::TextDisabled("GPU: zapytanie GL_TIME_ELAPSED");
            ImGui::Separator(); label("PORÓWNANIE A/B");
            ImGui::TextWrapped("Ta sama scena i kamera. 20 klatek rozgrzewki + %d próbek na wariant.",options.samples);
            ImGui::BeginDisabled(benchmark.active);
            if(ImGui::Button("Porównaj warianty  [F7]",{-1,34})) requestBenchmark=true;
            ImGui::EndDisabled();
            if(benchmark.active) {
                float progress=float(benchmark.mode*options.samples+benchmark.samples[benchmark.mode].size())/(2*options.samples);
                ImGui::ProgressBar(progress,{-1,18});
                ImGui::TextDisabled("%s",benchmark.mode==0?"Tylko odkryte ściany":"Wszystkie ściany");
            }
            if(benchmark.complete) {
                if(ImGui::BeginTable("Comparison",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Wariant"); ImGui::TableSetupColumn("GPU ms"); ImGui::TableSetupColumn("Trójkąty"); ImGui::TableHeadersRow();
                    for(int i=0;i<2;++i) {
                        ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(i==0?"Odkryte":"Wszystkie");
                        ImGui::TableNextColumn(); ImGui::Text("%.2f",benchmark.results[i].gpuMean);
                        ImGui::TableNextColumn(); ImGui::Text("%zu",benchmark.results[i].triangles);
                    }
                    ImGui::EndTable();
                }
                double reduction=100*(1-double(benchmark.results[0].triangles)/std::max(double(benchmark.results[1].triangles),1.0));
                ImGui::TextColored({0.50f,0.86f,0.64f,1},"O %.1f%% mniej trójkątów",reduction);
                ImGui::TextDisabled("Wyniki i surowe próbki zapisane w CSV.");
            }
            ImGui::End();

            ImGui::SetNextWindowPos({io.DisplaySize.x*0.5f,io.DisplaySize.y-24},{}, {0.5f,1}); ImGui::SetNextWindowSize({540,0});
            ImGui::Begin("Materials",nullptr,panelFlags);
            static const ImVec4 colors[]={{.4f,.65f,.29f,1},{.53f,.36f,.22f,1},{.49f,.54f,.57f,1},{.81f,.74f,.52f,1},{.48f,.31f,.17f,1},{.25f,.47f,.24f,1},{.79f,.86f,.91f,1}};
            ImGui::BeginDisabled(benchmark.active);
            for(int i=0;i<7;++i) {
                if(i) ImGui::SameLine();
                ImGui::PushID(i); bool active=selected==static_cast<Block>(i+1);
                ImGui::PushStyleColor(ImGuiCol_Button,active?ImVec4{.25f,.47f,.38f,1}:ImVec4{.09f,.14f,.17f,1});
                std::string text=std::to_string(i+1); if(ImGui::Button(text.c_str(),{63,36})) selected=static_cast<Block>(i+1);
                ImVec2 p=ImGui::GetItemRectMin(); ImGui::GetWindowDrawList()->AddRectFilled({p.x+7,p.y+12},{p.x+17,p.y+23},ImGui::ColorConvertFloat4ToU32(colors[i]),2);
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",blockName(static_cast<Block>(i+1)));
                ImGui::PopStyleColor(); ImGui::PopID();
            }
            ImGui::EndDisabled(); ImGui::Text("Materiał: %s",blockName(selected));
            ImGui::End();
            ImGui::SetNextWindowPos({20,io.DisplaySize.y-20},{},{0,1}); ImGui::SetNextWindowBgAlpha(.7f);
            ImGui::Begin("Status",nullptr,panelFlags|ImGuiWindowFlags_NoInputs);
            ImGui::TextUnformatted(status.c_str()); ImGui::End();
        }
        if(controls.captured) {
            auto* draw=ImGui::GetForegroundDrawList(); ImVec2 c{io.DisplaySize.x/2,io.DisplaySize.y/2};
            draw->AddLine({c.x-7,c.y},{c.x+7,c.y},IM_COL32(255,255,255,230),1.5f);
            draw->AddLine({c.x,c.y-7},{c.x,c.y+7},IM_COL32(255,255,255,230),1.5f);
        }
        ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if(options.smoke) {
            GLenum error=glGetError(); if(error!=GL_NO_ERROR) throw std::runtime_error("OpenGL error: "+std::to_string(error));
        }
        ++frame;
        if(autoExit && benchmark.complete) ++afterBenchmark;
        bool exitNow=(options.frames>0 && frame>=options.frames) || afterBenchmark>=10;
        if(screenshotRequest || (exitNow && !options.screenshot.empty())) {
            std::string path=options.screenshot.empty()?"results/preview.tga":options.screenshot;
            renderer.screenshot(path,width,height); screenshotRequest=0; status="Zapisano zrzut: "+path;
        }
        glfwSwapBuffers(window);
        if(exitNow) break;
    }
    if(options.smoke) std::cout<<"PASS: "<<frame<<" OpenGL frames without errors\n";
    return 0;
}
}
int main(int argc,char** argv) {
    for(int i=1;i<argc;++i) if(std::string(argv[i])=="--help") {
        std::cout<<"Voxel Landscape: C++17 / OpenGL 3.3\n"
            "  --seed N --radius N (2..6, streaming default) --frames N\n"
            "  --finite --chunks N (2..12, fixed-size world)\n"
            "  --benchmark --samples N --output results/benchmark.csv\n"
            "  --headless --smoke-test --screenshot results/preview.tga\n"; return 0;
    }
    GLFWwindow* window=nullptr;
    try {
        Options options=parseOptions(argc,argv);
        glfwSetErrorCallback([](int code,const char* text){std::cerr<<"GLFW "<<code<<": "<<text<<'\n';});
        if(!glfwInit()) throw std::runtime_error("GLFW initialization failed");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
#endif
        glfwWindowHint(GLFW_VISIBLE,options.headless?GLFW_FALSE:GLFW_TRUE);
        glfwWindowHint(GLFW_SAMPLES,4);
        window=glfwCreateWindow(1440,900,"Voxel Landscape | C++ / OpenGL",nullptr,nullptr);
        if(!window) throw std::runtime_error("OpenGL 3.3 window creation failed; check the graphics driver");
        glfwMakeContextCurrent(window); glfwSwapInterval(options.headless?0:1);
        int result=run(window,options); glfwDestroyWindow(window); glfwTerminate(); return result;
    } catch(const std::exception& e) {
        std::cerr<<"ERROR: "<<e.what()<<'\n';
        if(window) glfwDestroyWindow(window);
        glfwTerminate(); return 1;
    }
}
