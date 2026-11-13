#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class VoxelMesh{
    public:
        VoxelMesh(){
            // Array of vertex pos.
            float v[] = {
                // -Z
                -0.5f,-0.5f,-0.5f, 0,0,   0.5f, 0.5f,-0.5f, 1,1,   0.5f,-0.5f,-0.5f, 1,0,
                0.5f, 0.5f,-0.5f, 1,1,  -0.5f,-0.5f,-0.5f, 0,0,  -0.5f, 0.5f,-0.5f, 0,1,

                // +Z
                -0.5f,-0.5f, 0.5f, 0,0,   0.5f,-0.5f, 0.5f, 1,0,   0.5f, 0.5f, 0.5f, 1,1,
                0.5f, 0.5f, 0.5f, 1,1,  -0.5f, 0.5f, 0.5f, 0,1,  -0.5f,-0.5f, 0.5f, 0,0,

                // -X
                -0.5f, 0.5f, 0.5f, 1,0,  -0.5f, 0.5f,-0.5f, 1,1,  -0.5f,-0.5f,-0.5f, 0,1,
                -0.5f,-0.5f,-0.5f, 0,1,  -0.5f,-0.5f, 0.5f, 0,0,  -0.5f, 0.5f, 0.5f, 1,0,

                // +X
                0.5f, 0.5f, 0.5f, 1,0,   0.5f,-0.5f,-0.5f, 0,1,   0.5f, 0.5f,-0.5f, 1,1,
                0.5f,-0.5f,-0.5f, 0,1,   0.5f, 0.5f, 0.5f, 1,0,   0.5f,-0.5f, 0.5f, 0,0,

                // -Y
                -0.5f,-0.5f,-0.5f, 0,1,   0.5f,-0.5f,-0.5f, 1,1,   0.5f,-0.5f, 0.5f, 1,0,
                0.5f,-0.5f, 0.5f, 1,0,  -0.5f,-0.5f, 0.5f, 0,0,  -0.5f,-0.5f,-0.5f, 0,1,

                // +Y
                -0.5f, 0.5f,-0.5f, 0,1,   0.5f, 0.5f, 0.5f, 1,0,   0.5f, 0.5f,-0.5f, 1,1,
                0.5f, 0.5f, 0.5f, 1,0,  -0.5f, 0.5f,-0.5f, 0,1,  -0.5f, 0.5f, 0.5f, 0,0,
            };

            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO); // Używamy jednego VAO i VBO dla renderowania voxeli.
            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);

            glVertexAttribPointer(0,3,GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);

        }
        ~VoxelMesh(){
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);
        }
        VoxelMesh(const VoxelMesh&) = delete;
        VoxelMesh& operator = (const VoxelMesh&) = delete;

        void draw() const{
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        private:
            unsigned int VAO = 0, VBO = 0;
};

enum class VoxelType : unsigned char {Air = 0, Stone, Dirt, Grass};
struct Voxel{
    glm::ivec3 position{0};
    VoxelType type = VoxelType::Stone;

    glm::mat4 model() const{
        return glm::translate(glm::mat4(1.0f), glm::vec3(position));
    }
    // Kolory voxeli jako placeholder przed texture atlasem.
    glm::vec3 color() const{
        switch (type) {
            case VoxelType::Grass: return {0.30f, 0.70f, 0.25f};
            case VoxelType::Dirt:  return {0.45f, 0.30f, 0.15f};
            case VoxelType::Stone: return {0.50f, 0.50f, 0.50f};
            default:               return {1, 0, 1};
        }
    }
    void draw(const VoxelMesh& mesh, const Shader& shader) const {
        shader.setMat4("model", model());
        shader.setVec3("tint", color());
        mesh.draw();
    }
};