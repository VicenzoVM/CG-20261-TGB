#ifndef SCENELOADER_H
#define SCENELOADER_H

#include "../../common/Common.h"
#include "Camera.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// "Importa" as variáveis globais exatas que existem no main.cpp
extern bool wireframeOverlay;
extern int selectedObject;
extern std::vector<SceneObject> sceneObjects;
extern std::vector<PointLight> sceneLights;
extern ProjectionConfig projectionConfig;
extern Camera camera;

// Funções copiadas exatamente do seu monolito
int setupShader();
bool loadSceneConfig(const std::string& filePATH);
std::string defaultSceneConfigPath();
std::string resolveRelativePath(const std::string& baseFilePATH, const std::string& filePATH);
glm::vec3 readVec3(const nlohmann::json& jsonObject, const std::vector<std::string>& fieldNames, const glm::vec3& defaultValue);
glm::vec3 readVec3Field(const nlohmann::json& value, const std::string& fieldName);
std::string readString(const nlohmann::json& jsonObject, const std::vector<std::string>& fieldNames, const std::string& defaultValue);
glm::vec3 getBezierPoint(float t, const std::vector<glm::vec3>& cp);
glm::vec3 getBezierDerivative(float t, const std::vector<glm::vec3>& cp);
GLuint loadTexture(char const * path);
std::vector<Mesh> loadComplexOBJ(std::string filePATH);

#endif