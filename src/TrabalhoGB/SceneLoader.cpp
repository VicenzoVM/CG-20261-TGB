#include "SceneLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

uniform vec3 ka;
uniform vec3 kd;
uniform vec3 ks;
uniform float q;

uniform int numLights;
uniform PointLight lights[MAX_LIGHTS];
uniform vec3 cameraPos;
uniform vec3 objectColor;

uniform sampler2D texture1;
uniform bool hasTexture;

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
    
    if(hasTexture) {
        vec4 texColor = texture(texture1, fragTexCoord);
        result *= texColor.rgb;
    } else {
        result *= objectColor;
    }
    
    color = vec4(result, 1.0);
}
)glsl";

glm::vec3 getBezierPoint(float t, const std::vector<glm::vec3>& cp) {
    if (cp.size() < 4) return glm::vec3(0.0f);
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;
    glm::vec3 p = uuu * cp[0];
    p += 3 * uu * t * cp[1];
    p += 3 * u * tt * cp[2];
    p += ttt * cp[3];
    return p;
}

glm::vec3 getBezierDerivative(float t, const std::vector<glm::vec3>& cp) {
    if (cp.size() < 4) return glm::vec3(0.0f);
    float u = 1.0f - t;
    glm::vec3 p = 3.0f * u * u * (cp[1] - cp[0]);
    p += 6.0f * u * t * (cp[2] - cp[1]);
    p += 3.0f * t * t * (cp[3] - cp[2]);
    return p;
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

GLuint loadTexture(char const * path) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true); 
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    
    if (data) {
        GLenum format = (nrComponents == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    } else {
        std::cerr << "Falha ao carregar textura no caminho: " << path << std::endl;
        stbi_image_free(data);
        return 0;
    }
    return textureID;
}

std::map<std::string, Material> loadMaterials(const std::string& mtlPATH) {
    std::map<std::string, Material> materials;
    std::ifstream file(mtlPATH);
    if (!file.is_open()) return materials;
    std::string line, currentMaterial;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string prefix;
        ss >> prefix;
        if (prefix == "newmtl") {
            ss >> currentMaterial;
        } else if (prefix == "Ka" && !currentMaterial.empty()) {
            ss >> materials[currentMaterial].ka.r >> materials[currentMaterial].ka.g >> materials[currentMaterial].ka.b;
        } else if (prefix == "Kd" && !currentMaterial.empty()) {
            ss >> materials[currentMaterial].kd.r >> materials[currentMaterial].kd.g >> materials[currentMaterial].kd.b;
        } else if (prefix == "Ks" && !currentMaterial.empty()) {
            ss >> materials[currentMaterial].ks.r >> materials[currentMaterial].ks.g >> materials[currentMaterial].ks.b;
        } else if (prefix == "Ns" && !currentMaterial.empty()) {
            ss >> materials[currentMaterial].ns;
        } else if (prefix == "map_Kd" && !currentMaterial.empty()) {
            std::string texFile;
            ss >> texFile;
            std::string texPath = resolveRelativePath(mtlPATH, texFile);
            materials[currentMaterial].diffuseMap = loadTexture(texPath.c_str());
        }
    }
    return materials;
}

Mesh createMeshFromBuffer(const std::vector<GLfloat>& vBuffer, const Material& mat) {
    Mesh mesh;
    mesh.material = mat;
    mesh.nVertices = vBuffer.size() / 8;
    GLuint VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);
    glGenVertexArrays(1, &mesh.VAO);
    glBindVertexArray(mesh.VAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return mesh;
}

std::vector<Mesh> loadComplexOBJ(std::string filePATH) {
    struct ObjIndex { int vi, ti, ni; };
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> texCoords;
    std::vector<glm::vec3> normals;
    std::vector<Mesh> meshes;
    std::map<std::string, Material> materials;
    std::vector<GLfloat> currentVBuffer;
    std::string currentMtl = "";
    std::ifstream arqEntrada(filePATH.c_str());
    if (!arqEntrada.is_open()) return meshes;
    std::string line;
    while (std::getline(arqEntrada, line)) {
        std::istringstream ssline(line);
        std::string word;
        ssline >> word;
        if (word == "mtllib") {
            std::string mtlFile;
            ssline >> mtlFile;
            std::string mtlPath = resolveRelativePath(filePATH, mtlFile);
            materials = loadMaterials(mtlPath);
        } else if (word == "usemtl") {
            if (!currentVBuffer.empty()) {
                meshes.push_back(createMeshFromBuffer(currentVBuffer, materials[currentMtl]));
                currentVBuffer.clear();
            }
            ssline >> currentMtl;
        } else if (word == "v") {
            glm::vec3 vertice;
            ssline >> vertice.x >> vertice.y >> vertice.z;
            vertices.push_back(vertice);
        } else if (word == "vt") {
            glm::vec2 vt;
            ssline >> vt.s >> vt.t;
            texCoords.push_back(vt);
        } else if (word == "vn") {
            glm::vec3 normal;
            ssline >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        } else if (word == "f") {
            std::vector<std::string> faceVertices;
            while (ssline >> word) faceVertices.push_back(word);
            if (faceVertices.size() != 3) continue;
            for (const std::string& faceVertex : faceVertices) {
                ObjIndex idx = {-1, -1, -1};
                std::istringstream ss(faceVertex);
                std::string index;
                if (std::getline(ss, index, '/')) idx.vi = !index.empty() ? std::stoi(index) - 1 : -1;
                if (std::getline(ss, index, '/')) idx.ti = !index.empty() ? std::stoi(index) - 1 : -1;
                if (std::getline(ss, index)) idx.ni = !index.empty() ? std::stoi(index) - 1 : -1;
                glm::vec3 vertex = vertices[idx.vi];
                glm::vec2 texCoord = idx.ti >= 0 && idx.ti < static_cast<int>(texCoords.size()) ? texCoords[idx.ti] : glm::vec2(0.0f);
                glm::vec3 normal = idx.ni >= 0 && idx.ni < static_cast<int>(normals.size()) ? normals[idx.ni] : glm::vec3(0.0f, 0.0f, 1.0f);
                currentVBuffer.push_back(vertex.x); currentVBuffer.push_back(vertex.y); currentVBuffer.push_back(vertex.z);
                currentVBuffer.push_back(texCoord.x); currentVBuffer.push_back(texCoord.y);
                currentVBuffer.push_back(normal.x); currentVBuffer.push_back(normal.y); currentVBuffer.push_back(normal.z);
            }
        }
    }
    arqEntrada.close();
    if (!currentVBuffer.empty()) {
        meshes.push_back(createMeshFromBuffer(currentVBuffer, materials[currentMtl]));
    }
    return meshes;
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
            object.meshes = loadComplexOBJ(object.filePATH);
            
            if (objectJson.contains("trajectory")) {
                const auto& trajJson = objectJson.at("trajectory");
                object.trajectory.active = true;
                object.trajectory.type = trajJson.value("type", "circular");
                object.trajectory.duration = trajJson.value("duration", 10.0f);

                if (object.trajectory.type == "bezier" && trajJson.contains("points")) {
                    for (const auto& pointJson : trajJson.at("points")) {
                        object.trajectory.controlPoints.push_back(glm::vec3(pointJson[0], pointJson[1], pointJson[2]));
                    }
                } else if (object.trajectory.type == "circular") {
                    object.trajectory.center = readVec3(trajJson, {"center", "centro"}, object.position);
                    object.trajectory.radius = trajJson.value("radius", 5.0f);
                }
            }

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