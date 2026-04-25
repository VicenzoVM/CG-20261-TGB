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
int loadSimpleOBJ(std::string filePATH, int &nVertices);

const GLuint WIDTH = 800, HEIGHT = 600;

const GLchar* vertexShaderSource = R"glsl(#version 330 core
layout (location = 0) in vec3 position;
layout (location = 2) in vec3 normal;
uniform mat4 model;
uniform mat4 projection;
uniform mat4 view;
out vec3 fragPos;
out vec3 scaledNormal;
void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
    fragPos = vec3(model * vec4(position, 1.0)); 
    scaledNormal = mat3(transpose(inverse(model))) * normal;
}
)glsl";

const GLchar* fragmentShaderSource = R"glsl(#version 330 core
in vec3 fragPos;
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
int selectedObject = 0;
glm::vec3 pos[2] = {glm::vec3(-1.5f, 0.0f, 0.0f), glm::vec3(1.5f, 0.0f, 0.0f)};
glm::vec3 scale[2] = {glm::vec3(1.0f), glm::vec3(1.0f)};
glm::vec3 rot[2] = {glm::vec3(0.0f), glm::vec3(0.0f)};

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

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_DEPTH_TEST);

    GLuint shaderID = setupShader();
    glUseProgram(shaderID);

    Mesh suzanne, cube;
    suzanne.VAO = loadSimpleOBJ("../assets/Modelos3D/Suzanne.obj", suzanne.nVertices);
    cube.VAO = loadSimpleOBJ("../assets/Modelos3D/Cube.obj", cube.nVertices);

    glUniform1f(glGetUniformLocation(shaderID, "ka"), 0.2f);
    glUniform1f(glGetUniformLocation(shaderID, "kd"), 0.7f);
    glUniform1f(glGetUniformLocation(shaderID, "ks"), 0.5f);
    glUniform1f(glGetUniformLocation(shaderID, "q"), 32.0f);
    glUniform3f(glGetUniformLocation(shaderID, "lightPos"), -2.0f, 5.0f, 2.0f);
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

        glm::mat4 projection;
        if (perspective)
            projection = glm::perspective(glm::radians(45.0f),(float)WIDTH/(float)HEIGHT,0.1f,100.0f);
        else 
            projection = glm::ortho(-4.0f, 4.0f, -3.0f, 3.0f, 0.1f, 100.0f);
        
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        glm::mat4 view = camera.getViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform3f(glGetUniformLocation(shaderID, "cameraPos"), camera.position.x, camera.position.y, camera.position.z);

        if (suzanne.nVertices > 0) 
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, pos[0]);
            model = glm::rotate(model, glm::radians(rot[0].x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(rot[0].y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(rot[0].z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, scale[0]);
            glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(model));
            
            if (selectedObject == 0) glUniform3f(glGetUniformLocation(shaderID, "objectColor"), 1.0f, 0.3f, 0.3f);
            else glUniform3f(glGetUniformLocation(shaderID, "objectColor"), 0.5f, 0.5f, 0.5f);

            glBindVertexArray(suzanne.VAO);
            glDrawArrays(GL_TRIANGLES, 0, suzanne.nVertices);
        }

        if (cube.nVertices > 0) 
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, pos[1]);
            model = glm::rotate(model, glm::radians(rot[1].x), glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(rot[1].y), glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, glm::radians(rot[1].z), glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, scale[1]);
            glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(model));

            if (selectedObject == 1) glUniform3f(glGetUniformLocation(shaderID, "objectColor"), 0.3f, 0.3f, 1.0f);
            else glUniform3f(glGetUniformLocation(shaderID, "objectColor"), 0.5f, 0.5f, 0.5f);

            glBindVertexArray(cube.VAO);
            glDrawArrays(GL_TRIANGLES, 0, cube.nVertices);
        }

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

    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        if (key == GLFW_KEY_UP) pos[selectedObject].y += 0.1f;
        if (key == GLFW_KEY_DOWN) pos[selectedObject].y -= 0.1f;
        if (key == GLFW_KEY_LEFT) pos[selectedObject].x -= 0.1f;
        if (key == GLFW_KEY_RIGHT) pos[selectedObject].x += 0.1f;
        if (key == GLFW_KEY_O) pos[selectedObject].z -= 0.1f;
        if (key == GLFW_KEY_L) pos[selectedObject].z += 0.1f;

        if (key == GLFW_KEY_X) rot[selectedObject].x += 5.0f;
        if (key == GLFW_KEY_Y) rot[selectedObject].y += 5.0f;
        if (key == GLFW_KEY_Z) rot[selectedObject].z += 5.0f;

        if (key == GLFW_KEY_E) scale[selectedObject] += 0.1f;
        if (key == GLFW_KEY_R) scale[selectedObject] -= 0.1f;
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

int loadSimpleOBJ(std::string filePATH, int &nVertices)
{
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> normals;
    std::vector<GLfloat> vBuffer;

    std::ifstream arqEntrada(filePATH.c_str());
    if (!arqEntrada.is_open()) 
    {
        nVertices = 0; 
        return -1; 
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
            while (ssline >> word) 
            {
                int vi = 0, ti = 0, ni = 0;
                std::istringstream ss(word);
                std::string index;

                if (std::getline(ss, index, '/')) vi = !index.empty() ? std::stoi(index) - 1 : 0;
                if (std::getline(ss, index, '/')) ti = !index.empty() ? std::stoi(index) - 1 : 0;
                if (std::getline(ss, index)) ni = !index.empty() ? std::stoi(index) - 1 : 0;

                vBuffer.push_back(vertices[vi].x);
                vBuffer.push_back(vertices[vi].y);
                vBuffer.push_back(vertices[vi].z);
                vBuffer.push_back(0.0f);
                vBuffer.push_back(0.0f);
                vBuffer.push_back(0.0f);
                vBuffer.push_back(normals[ni].x);
                vBuffer.push_back(normals[ni].y);
                vBuffer.push_back(normals[ni].z);
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
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = vBuffer.size() / 9;  

    return VAO;
}