#pragma once
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <stdexcept>

class Shader{
    public:
        Shader(const std::string& vertPath, const std::string& fragPath){
            GLuint vs = compile(GL_VERTEX_SHADER, readFile(vertPath));
            GLuint fs = compile(GL_FRAGMENT_SHADER, readFile(fragPath));
            m_id = glCreateProgram();
            glAttachShader(m_id, vs); // Dane ID do vert shadera 
            glAttachShader(m_id, fs); // Podpinamy program
            glLinkProgram(m_id);
            GLint link_ok;
            glGetProgramiv(m_id, GL_LINK_STATUS, &link_ok);
            if (!link_ok) {
                char log[1024];
                glGetProgramInfoLog(m_id, sizeof(log), nullptr, log);
                std::cerr << "LINKING FAILED:\n" << log << std::endl;
            }
            glDeleteShader(vs);
            glDeleteShader(fs);
        }
        ~Shader() { glDeleteProgram(m_id);} // Destruktor programu.
        
        void use() const {glUseProgram(m_id);}
        void setFloat(const std::string& name, float v) const{
            glUniform1f(glGetUniformLocation(m_id, name.c_str()),v);
        }
    private:
        GLuint m_id;
        static std::string readFile(const std::string& path){
            std::ifstream file(path);
            if (!file) throw std::runtime_error("Unable to open: "+ path);
            std::stringstream ss;
            ss << file.rdbuf();
            return ss.str();
        }
        static GLuint compile(GLenum type, const std::string& src) {
            GLuint shader = glCreateShader(type);
            const char* source = src.c_str();
            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);

            GLint compile_ok;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_ok);
            if (!compile_ok){
                char log[1024];
                glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
                std::cerr<< "COMPILATION FAILED: \n" << log << std::endl;
            }
            return shader;
        }
};