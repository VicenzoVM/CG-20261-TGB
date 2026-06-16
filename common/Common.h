#ifndef COMMON_H
#define COMMON_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

struct Material {
    glm::vec3 ka = glm::vec3(0.2f);
    glm::vec3 kd = glm::vec3(0.8f);
    glm::vec3 ks = glm::vec3(0.5f);
    float ns = 32.0f;
    GLuint diffuseMap = 0; 
};

struct Mesh {
    GLuint VAO = 0;
    int nVertices = 0;
    Material material;
};

struct Trajectory {
    bool active = false;
    std::string type = "bezier"; 
    std::vector<glm::vec3> controlPoints; 
    glm::vec3 center = glm::vec3(0.0f);   
    float radius = 5.0f;                  
    float duration = 10.0f;
};

struct SceneObject {
    std::string name;
    std::string filePATH;
    std::vector<Mesh> meshes;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec3 color = glm::vec3(0.5f);
    glm::vec3 selectedColor = glm::vec3(1.0f);
    Trajectory trajectory;
};

struct PointLight {
    glm::vec3 position = glm::vec3(-2.0f, 5.0f, 2.0f);
    glm::vec3 color = glm::vec3(1.0f);
};

struct ProjectionConfig {
    bool perspective = true;
    float fov = 45.0f;
    float perspectiveNear = 0.1f;
    float perspectiveFar = 100.0f;
    float left = -4.0f;
    float right = 4.0f;
    float bottom = -3.0f;
    float top = 3.0f;
    float orthographicNear = 0.1f;
    float orthographicFar = 100.0f;
};

#endif