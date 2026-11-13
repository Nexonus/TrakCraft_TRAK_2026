#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

class Camera {
public:
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 front{0.0f, 0.0f, -1.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};

    float yaw = -90.0f;
    float pitch = 0.0f;
    float speed = 3.0f;
    float sensitivity = 0.1f;
    float fov = 45.0f;

    enum class Move { Forward, Backward, Left, Right, Up, Down };

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(cameraPos, cameraPos + front, up);
    }

    void processKeyboard(Move dir, float dt) {
        float v = speed * dt;
        switch (dir) {
            case Move::Forward:  cameraPos += front * v;   break;
            case Move::Backward: cameraPos -= front * v;   break;
            case Move::Left:     cameraPos -= right * v;   break;
            case Move::Right:    cameraPos += right * v;   break;
            case Move::Up:       cameraPos += worldUp * v; break;
            case Move::Down:     cameraPos -= worldUp * v; break;
        }
    }

    void processMouse(float dx, float dy) {
        yaw   += dx * sensitivity;
        pitch += dy * sensitivity;
        if (pitch >  89.0f) pitch =  89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        updateVectors();
    }

    void updateVectors() {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);
        right = glm::normalize(glm::cross(front, worldUp));
        up    = glm::normalize(glm::cross(right, front));
    }
};