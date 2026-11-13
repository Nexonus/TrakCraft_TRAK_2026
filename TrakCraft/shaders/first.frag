#version 330 core
in vec2 uv;
uniform vec3 tint;
out vec4 FragColor;

void main() {
    FragColor = vec4(tint, 1.0);
}