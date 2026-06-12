#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>
#include "Camera.h"

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
int setupShader();
GLuint loadSimpleOBJ(std::string filePATH, int &nVertices);
bool loadSceneConfig(const std::string& filePATH);
void applyScaleDelta(int objectIndex, const glm::vec3& delta);
void adjustMaterial(float direction);
void updateWindowTitle(GLFWwindow* window);
float clampFloat(float value, float minValue, float maxValue);
std::string defaultSceneConfigPath();
std::string resolveRelativePath(const std::string& baseFilePATH, const std::string& filePATH);
glm::vec3 readVec3(const nlohmann::json& jsonObject, const std::vector<std::string>& fieldNames, const glm::vec3& defaultValue);

const GLuint WIDTH = 800, HEIGHT = 600;

const GLchar* vertexShaderSource = R"glsl(#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec3 normal;
uniform mat4 model;
uniform mat4 projection;
uniform mat4 view;
out vec3 fragPos;
out vec2 fragTexCoord;
out vec3 scaledNormal;
void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
    fragPos = vec3(model * vec4(position, 1.0)); 
    fragTexCoord = texCoord;
    scaledNormal = mat3(transpose(inverse(model))) * normal;
}
)glsl";

const GLchar* fragmentShaderSource = R"glsl(#version 330 core
const int MAX_LIGHTS = 8;
struct PointLight
{
    vec3 position;
    vec3 color;
};
in vec3 fragPos;
in vec2 fragTexCoord;
in vec3 scaledNormal;
uniform float ka;
uniform float kd;
uniform float ks, q;
uniform int numLights;
uniform PointLight lights[MAX_LIGHTS];
uniform vec3 cameraPos;
uniform vec3 objectColor;
out vec4 color;
void main()
{
    vec3 N = normalize(scaledNormal);
    vec3 V = normalize(cameraPos - fragPos);
    vec3 result = vec3(0.0);

    for (int i = 0; i < numLights; i++)
    {
        vec3 ambient = ka * lights[i].color;
        vec3 L = normalize(lights[i].position - fragPos);
        float diff = max(dot(N,L),0.0);
        vec3 diffuse = kd * diff * lights[i].color;
        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(V, R), 0.0), q);
        vec3 specular = ks * spec * lights[i].color;
        result += ambient + diffuse + specular;
    }

    result *= objectColor;
    color = vec4(result, 1.0);
}
)glsl";

const int MAX_LIGHTS = 8;

struct Mesh
{
    GLuint VAO = 0;
    int nVertices = 0;
};

struct SceneObject
{
    std::string name;
    std::string filePATH;
    Mesh mesh;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec3 color = glm::vec3(0.5f);
    glm::vec3 selectedColor = glm::vec3(1.0f);
};

struct PointLight
{
    glm::vec3 position = glm::vec3(-2.0f, 5.0f, 2.0f);
    glm::vec3 color = glm::vec3(1.0f);
};

struct ProjectionConfig
{
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

bool wireframeOverlay = false;
int selectedObject = 0;
std::vector<SceneObject> sceneObjects;
std::vector<PointLight> sceneLights;
ProjectionConfig projectionConfig;
float kaValue = 0.2f;
float kdValue = 0.7f;
float ksValue = 0.5f;
float qValue = 32.0f;
int selectedMaterial = 0;

const float TRANSFORM_STEP = 0.1f;
const float ROTATION_STEP = 5.0f;
const float MIN_SCALE = 0.1f;
const float MATERIAL_STEP = 0.05f;
const float SHININESS_STEP = 4.0f;

Camera camera(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f);
float deltaTime = 0.0f;
float lastFrame = 0.0f; 
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;
bool firstMouse = true;

int main(int argc, char** argv)
{
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Trabalho Grau A", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    updateWindowTitle(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_DEPTH_TEST);

    GLuint shaderID = setupShader();
    glUseProgram(shaderID);

    std::string sceneConfigPath = argc > 1 ? argv[1] : defaultSceneConfigPath();
    if (!loadSceneConfig(sceneConfigPath))
    {
        glfwTerminate();
        return -1;
    }
    updateWindowTitle(window);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if(glfwGetKey(window,GLFW_KEY_W) == GLFW_PRESS) camera.processKeyboard("FORWARD",deltaTime);
        if(glfwGetKey(window,GLFW_KEY_S) == GLFW_PRESS) camera.processKeyboard("BACKWARD",deltaTime);
        if(glfwGetKey(window,GLFW_KEY_A) == GLFW_PRESS) camera.processKeyboard("LEFT",deltaTime);
        if(glfwGetKey(window,GLFW_KEY_D) == GLFW_PRESS) camera.processKeyboard("RIGHT",deltaTime);

        glUniform1f(glGetUniformLocation(shaderID, "ka"), kaValue);
        glUniform1f(glGetUniformLocation(shaderID, "kd"), kdValue);
        glUniform1f(glGetUniformLocation(shaderID, "ks"), ksValue);
        glUniform1f(glGetUniformLocation(shaderID, "q"), qValue);

        int numLights = std::min(static_cast<int>(sceneLights.size()), MAX_LIGHTS);
        glUniform1i(glGetUniformLocation(shaderID, "numLights"), numLights);
        for (int i = 0; i < numLights; i++)
        {
            std::string positionUniform = "lights[" + std::to_string(i) + "].position";
            std::string colorUniform = "lights[" + std::to_string(i) + "].color";
            glUniform3f(glGetUniformLocation(shaderID, positionUniform.c_str()),
                        sceneLights[i].position.x, sceneLights[i].position.y, sceneLights[i].position.z);
            glUniform3f(glGetUniformLocation(shaderID, colorUniform.c_str()),
                        sceneLights[i].color.r, sceneLights[i].color.g, sceneLights[i].color.b);
        }

        glm::mat4 projection;
        if (projectionConfig.perspective)
            projection = glm::perspective(glm::radians(projectionConfig.fov), (float)WIDTH/(float)HEIGHT, projectionConfig.perspectiveNear, projectionConfig.perspectiveFar);
        else 
            projection = glm::ortho(projectionConfig.left, projectionConfig.right, projectionConfig.bottom, projectionConfig.top, projectionConfig.orthographicNear, projectionConfig.orthographicFar);
        
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glm::mat4 view = camera.getViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform3f(glGetUniformLocation(shaderID, "cameraPos"), camera.position.x, camera.position.y, camera.position.z);

        auto drawObject = [&](const SceneObject& object, int objectIndex)
        {
            if (object.mesh.nVertices <= 0) return;

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, object.position);
            model = glm::rotate(model, glm::radians(object.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(object.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(object.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, object.scale);
            glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(model));

            glm::vec3 color = selectedObject == objectIndex ? object.selectedColor : object.color;
            glUniform3f(glGetUniformLocation(shaderID, "objectColor"), color.r, color.g, color.b);

            glBindVertexArray(object.mesh.VAO);
            if (wireframeOverlay)
            {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(1.0f, 1.0f);
            }
            glDrawArrays(GL_TRIANGLES, 0, object.mesh.nVertices);

            if (wireframeOverlay)
            {
                glDisable(GL_POLYGON_OFFSET_FILL);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glLineWidth(1.5f);
                glUniform3f(glGetUniformLocation(shaderID, "objectColor"), 0.0f, 0.0f, 0.0f);
                glDrawArrays(GL_TRIANGLES, 0, object.mesh.nVertices);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
        };

        for (int i = 0; i < static_cast<int>(sceneObjects.size()); i++)
            drawObject(sceneObjects[i], i);

        glfwSwapBuffers(window);
    }
    glfwTerminate();
    return 0;
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos;
    lastY = ypos;

    camera.processMouseMovement(xoffset, yoffset);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (key == GLFW_KEY_P && action == GLFW_PRESS)
        projectionConfig.perspective = !projectionConfig.perspective;

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS && !sceneObjects.empty())
        selectedObject = (selectedObject + 1) % static_cast<int>(sceneObjects.size());

    if (key == GLFW_KEY_F && action == GLFW_PRESS)
        wireframeOverlay = !wireframeOverlay;

    if (key == GLFW_KEY_B && action == GLFW_PRESS)
    {
        selectedMaterial = (selectedMaterial + 1) % 4;
        updateWindowTitle(window);
    }

    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        bool shiftPressed = (mode & GLFW_MOD_SHIFT) != 0;

        if (shiftPressed)
        {
            if (sceneLights.empty()) return;

            if (key == GLFW_KEY_UP) sceneLights[0].position.y += TRANSFORM_STEP;
            if (key == GLFW_KEY_DOWN) sceneLights[0].position.y -= TRANSFORM_STEP;
            if (key == GLFW_KEY_LEFT) sceneLights[0].position.x -= TRANSFORM_STEP;
            if (key == GLFW_KEY_RIGHT) sceneLights[0].position.x += TRANSFORM_STEP;
            if (key == GLFW_KEY_O) sceneLights[0].position.z -= TRANSFORM_STEP;
            if (key == GLFW_KEY_L) sceneLights[0].position.z += TRANSFORM_STEP;
        }
        else
        {
            if (sceneObjects.empty() || selectedObject < 0 || selectedObject >= static_cast<int>(sceneObjects.size())) return;

            SceneObject& object = sceneObjects[selectedObject];

            if (key == GLFW_KEY_UP) object.position.y += TRANSFORM_STEP;
            if (key == GLFW_KEY_DOWN) object.position.y -= TRANSFORM_STEP;
            if (key == GLFW_KEY_LEFT) object.position.x -= TRANSFORM_STEP;
            if (key == GLFW_KEY_RIGHT) object.position.x += TRANSFORM_STEP;
            if (key == GLFW_KEY_O) object.position.z -= TRANSFORM_STEP;
            if (key == GLFW_KEY_L) object.position.z += TRANSFORM_STEP;

            if (key == GLFW_KEY_X) object.rotation.x += ROTATION_STEP;
            if (key == GLFW_KEY_Y) object.rotation.y += ROTATION_STEP;
            if (key == GLFW_KEY_Z) object.rotation.z += ROTATION_STEP;

            if (key == GLFW_KEY_E) applyScaleDelta(selectedObject, glm::vec3(TRANSFORM_STEP));
            if (key == GLFW_KEY_R) applyScaleDelta(selectedObject, glm::vec3(-TRANSFORM_STEP));
            if (key == GLFW_KEY_1) applyScaleDelta(selectedObject, glm::vec3(TRANSFORM_STEP, 0.0f, 0.0f));
            if (key == GLFW_KEY_2) applyScaleDelta(selectedObject, glm::vec3(-TRANSFORM_STEP, 0.0f, 0.0f));
            if (key == GLFW_KEY_3) applyScaleDelta(selectedObject, glm::vec3(0.0f, TRANSFORM_STEP, 0.0f));
            if (key == GLFW_KEY_4) applyScaleDelta(selectedObject, glm::vec3(0.0f, -TRANSFORM_STEP, 0.0f));
            if (key == GLFW_KEY_5) applyScaleDelta(selectedObject, glm::vec3(0.0f, 0.0f, TRANSFORM_STEP));
            if (key == GLFW_KEY_6) applyScaleDelta(selectedObject, glm::vec3(0.0f, 0.0f, -TRANSFORM_STEP));

            if (key == GLFW_KEY_N) adjustMaterial(-1.0f);
            if (key == GLFW_KEY_M) adjustMaterial(1.0f);

            if (key == GLFW_KEY_N || key == GLFW_KEY_M)
                updateWindowTitle(window);
        }
    }
}

void applyScaleDelta(int objectIndex, const glm::vec3& delta)
{
    if (objectIndex < 0 || objectIndex >= static_cast<int>(sceneObjects.size())) return;

    glm::vec3& objectScale = sceneObjects[objectIndex].scale;
    objectScale += delta;
    objectScale.x = clampFloat(objectScale.x, MIN_SCALE, 100.0f);
    objectScale.y = clampFloat(objectScale.y, MIN_SCALE, 100.0f);
    objectScale.z = clampFloat(objectScale.z, MIN_SCALE, 100.0f);
}

void adjustMaterial(float direction)
{
    if (selectedMaterial == 0)
        kaValue = clampFloat(kaValue + direction * MATERIAL_STEP, 0.0f, 1.0f);
    else if (selectedMaterial == 1)
        kdValue = clampFloat(kdValue + direction * MATERIAL_STEP, 0.0f, 1.0f);
    else if (selectedMaterial == 2)
        ksValue = clampFloat(ksValue + direction * MATERIAL_STEP, 0.0f, 1.0f);
    else if (selectedMaterial == 3)
        qValue = clampFloat(qValue + direction * SHININESS_STEP, 1.0f, 256.0f);
}

void updateWindowTitle(GLFWwindow* window)
{
    const char* materialNames[4] = {"ka", "kd", "ks", "q"};
    std::ostringstream title;
    title.precision(2);
    title << std::fixed
          << "Trabalho Grau A | Material: " << materialNames[selectedMaterial]
          << " | ka=" << kaValue
          << " kd=" << kdValue
          << " ks=" << ksValue
          << " q=" << qValue;
    glfwSetWindowTitle(window, title.str().c_str());
}

float clampFloat(float value, float minValue, float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

std::string defaultSceneConfigPath()
{
    std::vector<std::string> candidates = {
        "../assets/cena.json",
        "assets/cena.json"
    };

    for (const std::string& path : candidates)
    {
        std::ifstream file(path.c_str());
        if (file.is_open())
            return path;
    }

    return "../assets/cena.json";
}

std::string resolveRelativePath(const std::string& baseFilePATH, const std::string& filePATH)
{
    if (filePATH.empty())
        return filePATH;

    if (filePATH[0] == '/' || (filePATH.size() > 1 && filePATH[1] == ':'))
        return filePATH;

    std::size_t separator = baseFilePATH.find_last_of("/\\");
    if (separator == std::string::npos)
        return filePATH;

    return baseFilePATH.substr(0, separator + 1) + filePATH;
}

glm::vec3 readVec3Field(const nlohmann::json& value, const std::string& fieldName)
{
    if (!value.is_array() || value.size() != 3)
        throw std::runtime_error("Campo '" + fieldName + "' deve ser um array com 3 numeros.");

    return glm::vec3(value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>());
}

glm::vec3 readVec3(const nlohmann::json& jsonObject, const std::vector<std::string>& fieldNames, const glm::vec3& defaultValue)
{
    for (const std::string& fieldName : fieldNames)
    {
        if (jsonObject.contains(fieldName))
            return readVec3Field(jsonObject.at(fieldName), fieldName);
    }

    return defaultValue;
}

std::string readString(const nlohmann::json& jsonObject, const std::vector<std::string>& fieldNames, const std::string& defaultValue)
{
    for (const std::string& fieldName : fieldNames)
    {
        if (jsonObject.contains(fieldName))
            return jsonObject.at(fieldName).get<std::string>();
    }

    return defaultValue;
}

bool loadSceneConfig(const std::string& filePATH)
{
    std::ifstream sceneFile(filePATH.c_str());
    if (!sceneFile.is_open())
    {
        std::cerr << "Erro ao abrir arquivo de cena: " << filePATH << std::endl;
        return false;
    }

    try
    {
        nlohmann::json sceneJson;
        sceneFile >> sceneJson;

        sceneObjects.clear();
        sceneLights.clear();

        if (sceneJson.contains("camera"))
        {
            const nlohmann::json& cameraJson = sceneJson.at("camera");
            glm::vec3 cameraPosition = readVec3(cameraJson, {"position", "posicao", "trans", "translation"}, camera.position);
            glm::vec3 cameraUp = readVec3(cameraJson, {"up"}, camera.worldUp);
            float yaw = cameraJson.value("yaw", camera.yaw);
            float pitch = cameraJson.value("pitch", camera.pitch);
            camera.setPose(cameraPosition, cameraUp, yaw, pitch);
        }

        if (sceneJson.contains("projection"))
        {
            const nlohmann::json& projectionJson = sceneJson.at("projection");
            std::string type = projectionJson.value("type", "perspective");
            projectionConfig.perspective = type != "orthographic";

            const nlohmann::json& perspectiveJson = projectionJson.contains("perspective") ? projectionJson.at("perspective") : projectionJson;
            projectionConfig.fov = perspectiveJson.value("fov", projectionConfig.fov);
            projectionConfig.perspectiveNear = perspectiveJson.value("near", projectionConfig.perspectiveNear);
            projectionConfig.perspectiveFar = perspectiveJson.value("far", projectionConfig.perspectiveFar);

            const nlohmann::json& orthographicJson = projectionJson.contains("orthographic") ? projectionJson.at("orthographic") : projectionJson;
            projectionConfig.left = orthographicJson.value("left", projectionConfig.left);
            projectionConfig.right = orthographicJson.value("right", projectionConfig.right);
            projectionConfig.bottom = orthographicJson.value("bottom", projectionConfig.bottom);
            projectionConfig.top = orthographicJson.value("top", projectionConfig.top);
            projectionConfig.orthographicNear = orthographicJson.value("near", projectionConfig.orthographicNear);
            projectionConfig.orthographicFar = orthographicJson.value("far", projectionConfig.orthographicFar);
        }

        if (sceneJson.contains("lights"))
        {
            const nlohmann::json& lightsJson = sceneJson.at("lights");
            if (!lightsJson.is_array())
                throw std::runtime_error("Campo 'lights' deve ser um array.");

            for (const nlohmann::json& lightJson : lightsJson)
            {
                std::string type = lightJson.value("type", "point");
                if (type != "point")
                    throw std::runtime_error("Tipo de luz nao suportado: " + type);

                PointLight light;
                light.position = readVec3(lightJson, {"position", "posicao", "trans", "translation"}, light.position);
                light.color = readVec3(lightJson, {"color", "cor"}, light.color);
                sceneLights.push_back(light);
            }
        }

        if (sceneLights.empty())
            sceneLights.push_back(PointLight());

        if (!sceneJson.contains("objects") || !sceneJson.at("objects").is_array())
            throw std::runtime_error("Arquivo de cena deve conter o array 'objects'.");

        const nlohmann::json& objectsJson = sceneJson.at("objects");
        for (std::size_t i = 0; i < objectsJson.size(); i++)
        {
            const nlohmann::json& objectJson = objectsJson.at(i);
            std::string fileName = readString(objectJson, {"file", "arquivo"}, "");
            if (fileName.empty())
                throw std::runtime_error("Objeto sem campo 'file' no indice " + std::to_string(i) + ".");

            SceneObject object;
            object.name = readString(objectJson, {"name", "nome"}, "Object " + std::to_string(i));
            object.filePATH = resolveRelativePath(filePATH, fileName);
            object.position = readVec3(objectJson, {"position", "posicao", "trans", "translation"}, object.position);
            object.rotation = readVec3(objectJson, {"rotation", "rotacao", "rot"}, object.rotation);
            object.scale = readVec3(objectJson, {"scale", "escala"}, object.scale);
            object.color = readVec3(objectJson, {"color", "cor"}, object.color);
            object.selectedColor = readVec3(objectJson, {"selectedColor", "corSelecionado"}, object.selectedColor);
            object.mesh.VAO = loadSimpleOBJ(object.filePATH, object.mesh.nVertices);

            if (object.mesh.nVertices <= 0)
                throw std::runtime_error("Nao foi possivel carregar o OBJ: " + object.filePATH);

            sceneObjects.push_back(object);
        }

        if (sceneObjects.empty())
            throw std::runtime_error("Arquivo de cena nao possui objetos.");

        selectedObject = 0;
        return true;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Erro ao carregar cena '" << filePATH << "': " << error.what() << std::endl;
        sceneObjects.clear();
        sceneLights.clear();
        return false;
    }
}

int setupShader()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

GLuint loadSimpleOBJ(std::string filePATH, int &nVertices)
{
    struct ObjIndex
    {
        int vi;
        int ti;
        int ni;
    };

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> normals;
    std::vector<GLfloat> vBuffer;

    std::ifstream arqEntrada(filePATH.c_str());
    if (!arqEntrada.is_open()) 
    {
        nVertices = 0; 
        return 0;
    }

    std::string line;
    while (std::getline(arqEntrada, line)) 
    {
        std::istringstream ssline(line);
        std::string word;
        ssline >> word;

        if (word == "v") 
        {
            glm::vec3 vertice;
            ssline >> vertice.x >> vertice.y >> vertice.z;
            vertices.push_back(vertice);
        } 
        else if (word == "vt") 
        {
            glm::vec2 vt;
            ssline >> vt.s >> vt.t;
            texCoords.push_back(vt);
        } 
        else if (word == "vn") 
        {
            glm::vec3 normal;
            ssline >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        } 
        else if (word == "f")
        {
            std::vector<std::string> faceVertices;
            while (ssline >> word)
                faceVertices.push_back(word);

            if (faceVertices.size() != 3)
                continue;

            std::vector<ObjIndex> faceIndices;
            bool validFace = true;

            for (const std::string& faceVertex : faceVertices)
            {
                ObjIndex idx = {-1, -1, -1};
                std::istringstream ss(faceVertex);
                std::string index;

                if (std::getline(ss, index, '/')) idx.vi = !index.empty() ? std::stoi(index) - 1 : -1;
                if (std::getline(ss, index, '/')) idx.ti = !index.empty() ? std::stoi(index) - 1 : -1;
                if (std::getline(ss, index)) idx.ni = !index.empty() ? std::stoi(index) - 1 : -1;

                if (idx.vi < 0 || idx.vi >= static_cast<int>(vertices.size()))
                    validFace = false;

                faceIndices.push_back(idx);
            }

            if (!validFace)
                continue;

            for (const ObjIndex& idx : faceIndices)
            {
                glm::vec3 vertex = vertices[idx.vi];
                glm::vec2 texCoord = idx.ti >= 0 && idx.ti < static_cast<int>(texCoords.size()) ? texCoords[idx.ti] : glm::vec2(0.0f);
                glm::vec3 normal = idx.ni >= 0 && idx.ni < static_cast<int>(normals.size()) ? normals[idx.ni] : glm::vec3(0.0f, 0.0f, 1.0f);

                vBuffer.push_back(vertex.x);
                vBuffer.push_back(vertex.y);
                vBuffer.push_back(vertex.z);
                vBuffer.push_back(texCoord.x);
                vBuffer.push_back(texCoord.y);
                vBuffer.push_back(normal.x);
                vBuffer.push_back(normal.y);
                vBuffer.push_back(normal.z);
            }
        }
    }
    arqEntrada.close();

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);
    
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = vBuffer.size() / 8;

    return VAO;
}
