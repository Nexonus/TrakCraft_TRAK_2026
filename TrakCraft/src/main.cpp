#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "shader.h"
#include "camera.h"
#include <string>
#include <fstream>
#include <sstream>
#include "voxel.h"

Camera camera;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    static bool firstMouse = true;
    static double lastX = 0, lastY = 0;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float dx = float(xpos - lastX);
    float dy = float(lastY - ypos);
    lastX = xpos;
    lastY = ypos;

    camera.processMouse(dx, dy);
}

void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    using M = Camera::Move;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.processKeyboard(M::Forward, dt);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.processKeyboard(M::Backward, dt);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.processKeyboard(M::Left, dt);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.processKeyboard(M::Right, dt);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) camera.processKeyboard(M::Up, dt);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camera.processKeyboard(M::Down, dt);
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Unable to initialize GLFW." << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "TrakCraft 0.1", NULL, NULL);
    if (!window) {
        std::cerr << "Unable to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window); // Po kontekście ładujemy GLAD jako loader do API OpenGL.
    
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback); 
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    { // ZASOBY GL tutaj:
        Shader shader("shaders/first.vert", "shaders/first.frag");
        VoxelMesh cube;
        Voxel voxel;
        voxel.position = {0, 0, 0};
        voxel.type = VoxelType::Grass;

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK); // Nieco rzeczy się pozmieniało tutaj dodajemy culling.
        glFrontFace(GL_CCW); 

        glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1920.0f / 1080.0f, 0.1f, 100.0f);

        float lastFrame = 0.0f;

        while (!glfwWindowShouldClose(window)) {
                float now = (float)glfwGetTime();
                float dt = now - lastFrame;
                lastFrame = now;

                processInput(window, dt);
                
                glClearColor(0.5f, 0.5f, 0.5f, 0.5f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                shader.use();
                shader.setMat4("view", camera.getViewMatrix());
                shader.setMat4("projection", projection);

                voxel.draw(cube, shader);

                glfwSwapBuffers(window);
                glfwPollEvents();
            }
        }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}