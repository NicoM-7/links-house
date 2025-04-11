#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include <string>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct VertexData
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 texCoords;
    VertexData() = default;
    VertexData(float x, float y, float z) : position(x, y, z) {}
    VertexData(float x, float y, float z, float nx, float ny, float nz, float u, float v)
        : position(x, y, z), normal(nx, ny, nz), texCoords(u, v) {}
    VertexData(float x, float y, float z, float nx, float ny, float nz,
               float r, float g, float b, float u, float v)
        : position(x, y, z), normal(nx, ny, nz), color(r, g, b), texCoords(u, v) {}
};

struct TriData
{
    unsigned int indices[3];
    TriData() = default;
    TriData(unsigned int i1, unsigned int i2, unsigned int i3)
    {
        indices[0] = i1;
        indices[1] = i2;
        indices[2] = i3;
    }
};

void readPLYFile(const std::string &filePath, std::vector<VertexData> &vertices, std::vector<TriData> &triangles)
{
    std::ifstream inFile(filePath);
    if (!inFile)
    {
        std::cerr << "Error: Could not open PLY file: " << filePath << std::endl;
        return;
    }

    std::string line;
    int numVertices = 0, numFaces = 0;
    std::vector<std::string> vertexProperties;
    while (std::getline(inFile, line))
    {
        const std::string vertexElementStr = "element vertex ";
        if (line.find(vertexElementStr) != std::string::npos)
            numVertices = std::stoi(line.substr(vertexElementStr.length()));
        const std::string propertyFloatStr = "property float ";
        if (line.find(propertyFloatStr) != std::string::npos && numVertices > 0)
        {
            std::string prop = line.substr(propertyFloatStr.length());
            vertexProperties.push_back(prop);
        }
        const std::string faceElementStr = "element face ";
        if (line.find(faceElementStr) != std::string::npos)
            numFaces = std::stoi(line.substr(faceElementStr.length()));
        if (line == "end_header")
            break;
    }

    for (int i = 0; i < numVertices; ++i)
    {
        std::getline(inFile, line);
        std::istringstream iss(line);
        VertexData v;
        int propIndex = 0;
        std::string token;
        while (iss >> token && propIndex < vertexProperties.size())
        {
            const std::string &prop = vertexProperties[propIndex];
            if (prop == "x")
                v.position.x = std::stof(token);
            else if (prop == "y")
                v.position.y = std::stof(token);
            else if (prop == "z")
                v.position.z = std::stof(token);
            else if (prop == "nx")
                v.normal.x = std::stof(token);
            else if (prop == "ny")
                v.normal.y = std::stof(token);
            else if (prop == "nz")
                v.normal.z = std::stof(token);
            else if (prop == "red")
                v.color.r = std::stof(token);
            else if (prop == "green")
                v.color.g = std::stof(token);
            else if (prop == "blue")
                v.color.b = std::stof(token);
            else if (prop == "u")
                v.texCoords.x = std::stof(token);
            else if (prop == "v")
                v.texCoords.y = std::stof(token);
            ++propIndex;
        }
        vertices.push_back(v);
    }

    for (int i = 0; i < numFaces; ++i)
    {
        std::getline(inFile, line);
        std::istringstream iss(line);
        int vertexCount;
        iss >> vertexCount;
        unsigned int idx0, idx1, idx2;
        iss >> idx0 >> idx1 >> idx2;
        triangles.push_back(TriData(idx0, idx1, idx2));
    }
    inFile.close();
}

void loadARGBBMP(const char *imagePath, unsigned char **data, unsigned int *width, unsigned int *height)
{
    unsigned char header[54];
    unsigned int dataOffset, imageSize;
    FILE *file = fopen(imagePath, "rb");
    if (!file)
    {
        std::printf("Error: Could not open BMP file: %s\n", imagePath);
        return;
    }
    if (fread(header, 1, 54, file) != 54)
    {
        std::printf("Error: Not a valid BMP file %s\n", imagePath);
        fclose(file);
        return;
    }
    if (header[0] != 'B' || header[1] != 'M')
    {
        std::printf("Error: File is not a BMP file: %s\n", imagePath);
        fclose(file);
        return;
    }
    dataOffset = *(int *)&(header[0x0A]);
    imageSize = *(int *)&(header[0x22]);
    *width = *(int *)&(header[0x12]);
    *height = *(int *)&(header[0x16]);
    if (*(int *)&(header[0x1E]) != 3)
    {
        std::printf("Error: BMP file is not 32bpp: %s\n", imagePath);
        fclose(file);
        return;
    }
    if (imageSize == 0)
        imageSize = (*width) * (*height) * 4;
    if (dataOffset == 0)
        dataOffset = 54;
    *data = new unsigned char[imageSize];
    if (dataOffset != 54)
        fseek(file, dataOffset, SEEK_SET);
    fread(*data, 1, imageSize, file);
    fclose(file);
}

class TexturedMesh
{
public:
    GLuint positionVBO, texCoordVBO, indexBuffer, textureID, vertexArray, shaderProgram;
    std::vector<VertexData> vertices;
    std::vector<TriData> triangles;

    TexturedMesh(const std::string &plyPath, const std::string &bmpPath)
    {
        readPLYFile(plyPath, vertices, triangles);
        unsigned char *imageData = nullptr;
        unsigned int texWidth = 0, texHeight = 0;
        loadARGBBMP(bmpPath.c_str(), &imageData, &texWidth, &texHeight);

        glGenVertexArrays(1, &vertexArray);
        glBindVertexArray(vertexArray);

        std::vector<float> positions;
        std::vector<float> texCoords;
        for (const auto &v : vertices)
        {
            positions.push_back(v.position.x);
            positions.push_back(v.position.y);
            positions.push_back(v.position.z);
            texCoords.push_back(v.texCoords.x);
            texCoords.push_back(v.texCoords.y);
        }

        glGenBuffers(1, &positionVBO);
        glBindBuffer(GL_ARRAY_BUFFER, positionVBO);
        glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

        glGenBuffers(1, &texCoordVBO);
        glBindBuffer(GL_ARRAY_BUFFER, texCoordVBO);
        glBufferData(GL_ARRAY_BUFFER, texCoords.size() * sizeof(float), texCoords.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);

        std::vector<unsigned int> indices;
        for (const auto &tri : triangles)
        {
            indices.push_back(tri.indices[0]);
            indices.push_back(tri.indices[1]);
            indices.push_back(tri.indices[2]);
        }
        glGenBuffers(1, &indexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, imageData);
        glGenerateMipmap(GL_TEXTURE_2D);
        delete[] imageData;

        compileShaders();
        glBindVertexArray(0);
    }

    void draw(const glm::mat4 &MVP)
    {
        glUseProgram(shaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        GLuint mvpLocation = glGetUniformLocation(shaderProgram, "MVP");
        glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, glm::value_ptr(MVP));
        glBindVertexArray(vertexArray);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(triangles.size() * 3), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

private:
    void compileShaders()
    {
        const char *vertexShaderSrc = R"(
            #version 440 core
            layout(location = 0) in vec3 inPosition;
            layout(location = 1) in vec2 inTexCoords;
            out vec2 fragTexCoords;
            uniform mat4 MVP;
            void main() {
                gl_Position = MVP * vec4(inPosition, 1.0);
                fragTexCoords = inTexCoords;
            }
        )";

        const char *fragmentShaderSrc = R"(
            #version 440 core
            in vec2 fragTexCoords;
            uniform sampler2D textureSampler;
            out vec4 finalColor;
            void main() {
                finalColor = texture(textureSampler, fragTexCoords);
            }
        )";

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSrc, nullptr);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSrc, nullptr);
        glCompileShader(fragmentShader);

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }
};

int main()
{
    if (!glfwInit())
    {
        std::fprintf(stderr, "Error: Failed to initialize GLFW.\n");
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    const int windowWidth = 1400, windowHeight = 900;
    GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, "Links House", nullptr, nullptr);
    if (!window)
    {
        std::fprintf(stderr, "Error: Failed to create GLFW window.\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glewExperimental = true;
    if (glewInit() != GLEW_OK)
    {
        std::fprintf(stderr, "Error: Failed to initialize GLEW.\n");
        glfwTerminate();
        return -1;
    }
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glClearColor(0.2f, 0.2f, 0.3f, 0.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    TexturedMesh meshFloor("./LinksHouse/Floor.ply", "./LinksHouse/floor.bmp");
    TexturedMesh meshMetalObjects("./LinksHouse/MetalObjects.ply", "./LinksHouse/metalobjects.bmp");
    TexturedMesh meshPatio("./LinksHouse/Patio.ply", "./LinksHouse/patio.bmp");
    TexturedMesh meshTable("./LinksHouse/Table.ply", "./LinksHouse/table.bmp");
    TexturedMesh meshWalls("./LinksHouse/Walls.ply", "./LinksHouse/walls.bmp");
    TexturedMesh meshWindowBG("./LinksHouse/WindowBG.ply", "./LinksHouse/windowbg.bmp");
    TexturedMesh meshWoodObjects("./LinksHouse/WoodObjects.ply", "./LinksHouse/woodobjects.bmp");
    TexturedMesh meshBottles("./LinksHouse/Bottles.ply", "./LinksHouse/bottles.bmp");
    TexturedMesh meshCurtains("./LinksHouse/Curtains.ply", "./LinksHouse/curtains.bmp");
    TexturedMesh meshDoorBG("./LinksHouse/DoorBG.ply", "./LinksHouse/doorbg.bmp");

    glm::vec3 cameraPosition(0.5f, 0.4f, 0.5f);
    glm::vec3 cameraDirection(0.0f, 0.0f, -1.0f);
    const float movementSpeed = 0.03f;
    const float rotationSpeedDegrees = 2.0f;

    while (!glfwWindowShouldClose(window) && glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            cameraPosition += movementSpeed * cameraDirection;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            cameraPosition -= movementSpeed * cameraDirection;
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        {
            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f),
                                             glm::radians(rotationSpeedDegrees),
                                             glm::vec3(0.0f, 1.0f, 0.0f));
            cameraDirection = glm::normalize(glm::vec3(rotation * glm::vec4(cameraDirection, 0.0f)));
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        {
            glm::mat4 rotation = glm::rotate(glm::mat4(1.0f),
                                             glm::radians(-rotationSpeedDegrees),
                                             glm::vec3(0.0f, 1.0f, 0.0f));
            cameraDirection = glm::normalize(glm::vec3(rotation * glm::vec4(cameraDirection, 0.0f)));
        }
        glm::mat4 Projection = glm::perspective(glm::radians(45.0f),
                                                static_cast<float>(windowWidth) / windowHeight,
                                                0.001f, 1000.0f);
        glm::mat4 View = glm::lookAt(cameraPosition, cameraPosition + cameraDirection, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 Model = glm::mat4(1.0f);
        glm::mat4 MVP = Projection * View * Model;
        meshFloor.draw(MVP);
        meshPatio.draw(MVP);
        meshTable.draw(MVP);
        meshWalls.draw(MVP);
        meshWindowBG.draw(MVP);
        meshWoodObjects.draw(MVP);
        meshBottles.draw(MVP);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        meshDoorBG.draw(MVP);
        meshCurtains.draw(MVP);
        meshMetalObjects.draw(MVP);
        glDisable(GL_BLEND);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}