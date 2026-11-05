cmake_minimum_required(VERSION 3.10.0)
project(TrakCraft VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(TrakCraft 
    src/main.cpp
    src/glad.c
)
target_link_libraries(TrakCraft PRIVATE glfw OpenGL::GL)
target_include_directories(TrakCraft PRIVATE include)

add_custom_command(TARGET TrakCraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:TrakCraft>/shaders)
