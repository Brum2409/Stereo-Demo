#pragma once
#include "Core.h"

namespace Engine {

    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoords;
        glm::vec3 tangent;
        glm::vec3 bitangent;
        int materialID;
    };

    struct PointCloudPoint {
        glm::vec3 position;
        float intensity;
        glm::vec3 color;
    };

    struct PointCloudChunk {
        std::vector<PointCloudPoint> points;
        glm::vec3 centerPosition;
        float boundingRadius;
        std::vector<GLuint> lodVBOs;
        std::vector<size_t> lodPointCounts;
    };

    struct PointCloud {
        std::string name;
        std::string filePath;
        std::vector<PointCloudPoint> points;
        glm::vec3 position;
        glm::vec3 rotation;
        glm::vec3 scale;
        bool visible = true;
        GLuint vao;
        GLuint vbo;

        GLuint instanceVBO;
        std::vector<glm::mat4> instanceMatrices;
        size_t instanceCount;

        float basePointSize = 2.0f;

        std::vector<PointCloudChunk> chunks;
        float lodDistances[5] = { 5.0f, 15.0f, 23.0f, 30.0f, 50.0f };
        float chunkSize = 2.0f;
        float newChunkSize = 2.0f;

        GLuint chunkOutlineVAO;
        GLuint chunkOutlineVBO;
        std::vector<glm::vec3> chunkOutlineVertices;
        bool visualizeChunks;
    };



    struct Sun {
        glm::vec3 direction;
        glm::vec3 color;
        float intensity;
        bool enabled;
    };

    const int MAX_LIGHTS = 180;
    struct PointLight {
        glm::vec3 position;
        glm::vec3 color;
        float intensity;
        glm::mat4 lightSpaceMatrix;
    };

}