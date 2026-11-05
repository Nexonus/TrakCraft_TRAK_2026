#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "shader.h"
#include <string>
#include <fstream>
#include <sstream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}
void processInput(GLFWwindow *window){
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
 }

int main() {
    if (!glfwInit()) {
        std::cerr << "Unable to initialize GLFW window" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "GLFW Example Window", NULL, NULL);
    if (!window) {
        std::cerr << "Unable to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window); // Po kontekście ładujemy GLAD jako loader do API OpenGL.
    
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback); 

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    { // ZASOBY GL tutaj:
        Shader shader("shaders/first.vert", "shaders/first.frag");
        float vertices[] ={
            // WIERZCHOŁKI          // KOLOR
            -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 0.0f,
             0.0f, 0.5f, 0.0f,   1.0f, 1.0f, 1.0f
        };
    
    unsigned int VBO, VAO; // VertexArrayObject, VertexBufferObject (przechowywanie vertexów w GPU)
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW); // VertexData -> Buffora
    // STATIC_DRAW daje dostęp do GPU wielokrotnie, STREAM_DRAW raz zapisujemy i tylko kilka razy używamy.
    // DYNAMIC_DRAW wielokrotnie zapisujemy i wielokrotnie czytamy dane.

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    // Pozycja początkowa, [X,Y,Z], Float, bez normalizacji, STRIDE (czyli dziel dane co 3 np. |X,Y,Z| |X,Y,Z| |X,Y,Z| odpowiadające 0.5, -0.5, 0.5 etc. na końcu mamy offset 0.)
    glEnableVertexAttribArray(0); // Wierzchołki

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1); // Kolory

    while (!glfwWindowShouldClose(window)) {
        processInput(window); // Obsługa klawiszy
        glClearColor(0.8f, 0.5f, 0.48f, 1.0f);

        glClear(GL_COLOR_BUFFER_BIT);
        
        shader.use();
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES,0,3);

        // Tutaj obsługujemy rendering
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}