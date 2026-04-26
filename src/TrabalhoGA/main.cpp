#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Camera.h"

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
int setupShader();
GLuint loadSimpleOBJ(std::string filePATH, int &nVertices);
void applyScaleDelta(int objectIndex, const glm::vec3& delta);
void adjustMaterial(float direction);
void updateWindowTitle(GLFWwindow* window);
float clampFloat(float value, float minValue, float maxValue);

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
in vec3 fragPos;
in vec2 fragTexCoord;
in vec3 scaledNormal;
uniform float ka;
uniform float kd;
uniform float ks, q;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 cameraPos;
uniform vec3 objectColor;
out vec4 color;
void main()
{
    vec3 ambient = ka * lightColor;
    vec3 N = normalize(scaledNormal);
    vec3 L = normalize(lightPos - fragPos);
    float diff = max(dot(N,L),0.0);
    vec3 diffuse = kd * diff * lightColor;
    vec3 V = normalize(cameraPos - fragPos);
    vec3 R = reflect(-L, N);
    float spec = pow(max(dot(V, R), 0.0), q);
    vec3 specular = ks * spec * lightColor;
    vec3 result = (ambient + diffuse + specular) * objectColor;
    color = vec4(result, 1.0);
}
)glsl";

bool perspective = true;
bool wireframeOverlay = false;
int selectedObject = 0;
glm::vec3 pos[2] = {glm::vec3(-1.5f, 0.0f, 0.0f), glm::vec3(1.5f, 0.0f, 0.0f)};
glm::vec3 scale[2] = {glm::vec3(1.0f), glm::vec3(1.0f)};
glm::vec3 rot[2] = {glm::vec3(0.0f), glm::vec3(0.0f)};
glm::vec3 lightPos(-2.0f, 5.0f, 2.0f);
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

struct Mesh 
{
    GLuint VAO; 
    int nVertices;
};

int main()
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

    Mesh suzanne, cube;
    suzanne.VAO = loadSimpleOBJ("../assets/Modelos3D/Suzanne.obj", suzanne.nVertices);
    cube.VAO = loadSimpleOBJ("../assets/Modelos3D/Cube.obj", cube.nVertices);

    glUniform3f(glGetUniformLocation(shaderID, "lightColor"), 1.0f, 1.0f, 1.0f);

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
        glUniform3f(glGetUniformLocation(shaderID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);

        glm::mat4 projection;
        if (perspective)
            projection = glm::perspective(glm::radians(45.0f),(float)WIDTH/(float)HEIGHT,0.1f,100.0f);
        else 
            projection = glm::ortho(-4.0f, 4.0f, -3.0f, 3.0f, 0.1f, 100.0f);
        
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glm::mat4 view = camera.getViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform3f(glGetUniformLocation(shaderID, "cameraPos"), camera.position.x, camera.position.y, camera.position.z);

        auto drawObject = [&](const Mesh& mesh, int objectIndex, const glm::vec3& selectedColor)
        {
            if (mesh.nVertices <= 0) return;

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, pos[objectIndex]);
            model = glm::rotate(model, glm::radians(rot[objectIndex].x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(rot[objectIndex].y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(rot[objectIndex].z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, scale[objectIndex]);
            glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(model));

            glm::vec3 color = selectedObject == objectIndex ? selectedColor : glm::vec3(0.5f);
            glUniform3f(glGetUniformLocation(shaderID, "objectColor"), color.r, color.g, color.b);

            glBindVertexArray(mesh.VAO);
            if (wireframeOverlay)
            {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(1.0f, 1.0f);
            }
            glDrawArrays(GL_TRIANGLES, 0, mesh.nVertices);

            if (wireframeOverlay)
            {
                glDisable(GL_POLYGON_OFFSET_FILL);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glLineWidth(1.5f);
                glUniform3f(glGetUniformLocation(shaderID, "objectColor"), 0.0f, 0.0f, 0.0f);
                glDrawArrays(GL_TRIANGLES, 0, mesh.nVertices);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
        };

        drawObject(suzanne, 0, glm::vec3(1.0f, 0.3f, 0.3f));
        drawObject(cube, 1, glm::vec3(0.3f, 0.3f, 1.0f));
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
        perspective = !perspective;

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
        selectedObject = (selectedObject + 1) % 2;

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
            if (key == GLFW_KEY_UP) lightPos.y += TRANSFORM_STEP;
            if (key == GLFW_KEY_DOWN) lightPos.y -= TRANSFORM_STEP;
            if (key == GLFW_KEY_LEFT) lightPos.x -= TRANSFORM_STEP;
            if (key == GLFW_KEY_RIGHT) lightPos.x += TRANSFORM_STEP;
            if (key == GLFW_KEY_O) lightPos.z -= TRANSFORM_STEP;
            if (key == GLFW_KEY_L) lightPos.z += TRANSFORM_STEP;
        }
        else
        {
            if (key == GLFW_KEY_UP) pos[selectedObject].y += TRANSFORM_STEP;
            if (key == GLFW_KEY_DOWN) pos[selectedObject].y -= TRANSFORM_STEP;
            if (key == GLFW_KEY_LEFT) pos[selectedObject].x -= TRANSFORM_STEP;
            if (key == GLFW_KEY_RIGHT) pos[selectedObject].x += TRANSFORM_STEP;
            if (key == GLFW_KEY_O) pos[selectedObject].z -= TRANSFORM_STEP;
            if (key == GLFW_KEY_L) pos[selectedObject].z += TRANSFORM_STEP;

            if (key == GLFW_KEY_X) rot[selectedObject].x += ROTATION_STEP;
            if (key == GLFW_KEY_Y) rot[selectedObject].y += ROTATION_STEP;
            if (key == GLFW_KEY_Z) rot[selectedObject].z += ROTATION_STEP;

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
    scale[objectIndex] += delta;
    scale[objectIndex].x = clampFloat(scale[objectIndex].x, MIN_SCALE, 100.0f);
    scale[objectIndex].y = clampFloat(scale[objectIndex].y, MIN_SCALE, 100.0f);
    scale[objectIndex].z = clampFloat(scale[objectIndex].z, MIN_SCALE, 100.0f);
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
