#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <map>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>
#include "../../common/Common.h"
#include "Camera.h"
#include "SceneLoader.h"

const GLuint WIDTH = 1920, HEIGHT = 1080;
const int MAX_LIGHTS = 8;

bool wireframeOverlay = false;
int selectedObject = 0;
std::vector<SceneObject> sceneObjects;
std::vector<PointLight> sceneLights;
ProjectionConfig projectionConfig;

const float TRANSFORM_STEP = 0.1f;
const float ROTATION_STEP = 5.0f;
const float MIN_SCALE = 0.1f;

Camera camera(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f);
float deltaTime = 0.0f;
float lastFrame = 0.0f; 
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;
bool firstMouse = true;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void applyScaleDelta(int objectIndex, const glm::vec3& delta);
void updateWindowTitle(GLFWwindow* window);
float clampFloat(float value, float minValue, float maxValue);

int main(int argc, char** argv)
{
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Trabalho Grau B", nullptr, nullptr);
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
        
        int numLights = std::min(static_cast<int>(sceneLights.size()), MAX_LIGHTS);
        glUniform1i(glGetUniformLocation(shaderID, "numLights"), numLights);
        for (int i = 0; i < numLights; i++)
        {
            std::string positionUniform = "lights[" + std::to_string(i) + "].position";
            std::string colorUniform = "lights[" + std::to_string(i) + "].color";
            glUniform3f(glGetUniformLocation(shaderID, positionUniform.c_str()), sceneLights[i].position.x, sceneLights[i].position.y, sceneLights[i].position.z);
            glUniform3f(glGetUniformLocation(shaderID, colorUniform.c_str()), sceneLights[i].color.r, sceneLights[i].color.g, sceneLights[i].color.b);
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

        auto drawObject = [&](SceneObject& object, int objectIndex)
        {
            if (object.meshes.empty()) return;
            
            glm::vec3 currentPosition = object.position;
            float currentYaw = object.rotation.y;
            float currentPitch = object.rotation.x;
            float currentRoll = object.rotation.z;

            if (object.trajectory.active) {
                float t = fmod(glfwGetTime(), object.trajectory.duration) / object.trajectory.duration;
                glm::vec3 tangent(0.0f);

                if (object.trajectory.type == "circular") {
                    float angle = t * 2.0f * glm::pi<float>();
                    currentPosition.x = object.trajectory.center.x + object.trajectory.radius * cos(angle);
                    currentPosition.z = object.trajectory.center.z + object.trajectory.radius * sin(angle);
                    currentPosition.y = object.position.y;
                    tangent = glm::vec3(-sin(angle), 0.0f, cos(angle));
                } 
                else if (object.trajectory.type == "bezier" && object.trajectory.controlPoints.size() >= 4) {
                    currentPosition = getBezierPoint(t, object.trajectory.controlPoints);
                    tangent = getBezierDerivative(t, object.trajectory.controlPoints);
                }
                else if (object.trajectory.type == "anchored") {
                    float time = glfwGetTime();
                    float phase = object.position.x + object.position.z; 
                    
                    currentPosition.y = object.position.y + sin(time * 2.0f + phase) * 0.06f;
                    currentPitch = object.rotation.x + sin(time * 1.5f + phase) * 2.5f; 
                    currentRoll = object.rotation.z + cos(time * 1.3f + phase) * 1.5f;
                }

                if (object.trajectory.type != "anchored" && glm::length(tangent) > 0.001f) {
                    tangent = glm::normalize(tangent);
                    float angleDeg = glm::degrees(atan2(tangent.x, tangent.z));
                    currentYaw = object.rotation.y + angleDeg;
                }
            }

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, currentPosition);
            model = glm::rotate(model, glm::radians(currentPitch), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(currentYaw), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(currentRoll), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, object.scale);
            glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(model));
            
            glm::vec3 color = selectedObject == objectIndex ? object.selectedColor : object.color;
            glUniform3f(glGetUniformLocation(shaderID, "objectColor"), color.r, color.g, color.b);
            
            for (const Mesh& mesh : object.meshes) {
                glUniform3f(glGetUniformLocation(shaderID, "ka"), mesh.material.ka.r, mesh.material.ka.g, mesh.material.ka.b);
                glUniform3f(glGetUniformLocation(shaderID, "kd"), mesh.material.kd.r, mesh.material.kd.g, mesh.material.kd.b);
                glUniform3f(glGetUniformLocation(shaderID, "ks"), mesh.material.ks.r, mesh.material.ks.g, mesh.material.ks.b);
                glUniform1f(glGetUniformLocation(shaderID, "q"), mesh.material.ns);

                if(mesh.material.diffuseMap > 0) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, mesh.material.diffuseMap);
                    glUniform1i(glGetUniformLocation(shaderID, "texture1"), 0);
                    glUniform1i(glGetUniformLocation(shaderID, "hasTexture"), 1);
                } else {
                    glUniform1i(glGetUniformLocation(shaderID, "hasTexture"), 0);
                }

                glBindVertexArray(mesh.VAO);
                if (wireframeOverlay) {
                    glEnable(GL_POLYGON_OFFSET_FILL);
                    glPolygonOffset(1.0f, 1.0f);
                }
                glDrawArrays(GL_TRIANGLES, 0, mesh.nVertices);

                if (wireframeOverlay) {
                    glDisable(GL_POLYGON_OFFSET_FILL);
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    glLineWidth(1.5f);
                    glUniform3f(glGetUniformLocation(shaderID, "objectColor"), 0.0f, 0.0f, 0.0f);
                    glUniform1i(glGetUniformLocation(shaderID, "hasTexture"), 0);
                    glDrawArrays(GL_TRIANGLES, 0, mesh.nVertices);
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                }
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
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS && !sceneObjects.empty()) {
        selectedObject = (selectedObject + 1) % static_cast<int>(sceneObjects.size());
        updateWindowTitle(window);
    }
    if (key == GLFW_KEY_F && action == GLFW_PRESS)
        wireframeOverlay = !wireframeOverlay;
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

void updateWindowTitle(GLFWwindow* window)
{
    std::ostringstream title;
    if (!sceneObjects.empty() && selectedObject >= 0 && selectedObject < static_cast<int>(sceneObjects.size())) {
        title << "Trabalho Grau B | Objeto: " << sceneObjects[selectedObject].name;
    } else {
        title << "Trabalho Grau B";
    }
    glfwSetWindowTitle(window, title.str().c_str());
}

float clampFloat(float value, float minValue, float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}