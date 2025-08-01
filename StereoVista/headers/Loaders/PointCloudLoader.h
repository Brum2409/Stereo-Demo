// point_cloud_loader.h
#pragma once
#include "../Engine/Data.h"
#include <sstream>

namespace Engine {

    class PointCloudLoader {
    public:
        static PointCloud loadPointCloudFile(const std::string& filePath, size_t downsampleFactor = 1);
        static bool exportToXYZ(const PointCloud& pointCloud, const std::string& filePath);
        static bool exportToBinary(const PointCloud& pointCloud, const std::string& filePath);
        static PointCloud loadFromBinary(const std::string& filePath);
        static PointCloud loadFromHDF5(const std::string& filePath, size_t downsampleFactor = 1);
        static bool exportToHDF5(const PointCloud& pointCloud, const std::string& filePath);

    private:
        static void setupPointCloudGLBuffers(PointCloud& pointCloud);
        static constexpr char BINARY_MAGIC_NUMBER[4] = { 'P', 'C', 'B', '1' };
        static std::string vec3_to_string(const glm::vec3& vec) {
            std::stringstream ss;
            ss << "(" << vec.x << ", " << vec.y << ", " << vec.z << ")";
            return ss.str();
        }
    };

    void generateChunks(PointCloud& pointCloud, float chunkSize);

    void generateLODLevels(PointCloudChunk& chunk);



    void generateChunkOutlineVertices(PointCloud& pointCloud);

    glm::vec3 calculateTransformedChunkPosition(const glm::vec3& chunkPos, const glm::mat4& modelMatrix);

}