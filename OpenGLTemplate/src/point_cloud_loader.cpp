// point_cloud_loader.cpp
#include "point_cloud_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <glad/glad.h>
#include <filesystem>
#include <future>
#include <map>
#include <omp.h>
#include <glm/gtx/norm.hpp>

#include <random>
#include <execution>
#include <algorithm>


namespace Engine {

    PointCloud PointCloudLoader::loadPointCloudFile(const std::string& filePath, size_t downsampleFactor) {
        PointCloud pointCloud;
        pointCloud.name = "PointCloud_" + std::filesystem::path(filePath).filename().string();
        pointCloud.position = glm::vec3(0.0f);
        pointCloud.rotation = glm::vec3(0.0f);
        pointCloud.scale = glm::vec3(1.0f);

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open point cloud file: " << filePath << std::endl;
            return pointCloud;
        }

        std::cout << "Loading point cloud from: " << filePath << std::endl;

        constexpr size_t bufferSize = 1024 * 1024; // 1 MB buffer
        std::vector<char> buffer(bufferSize);

        std::mutex pointCloudMutex;
        std::atomic<size_t> totalPointsProcessed(0);

        const size_t numThreads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;

        auto processChunk = [&](const std::vector<char>& chunk, size_t chunkSize) {
            std::vector<PointCloudPoint> localPoints;
            std::istringstream iss(std::string(chunk.data(), chunkSize));
            std::string line;
            size_t pointCounter = 0;

            while (std::getline(iss, line)) {
                if (!line.empty() && pointCounter % downsampleFactor == 0) {
                    float x, y, z, intensity;
                    int r, g, b;
                    if (sscanf_s(line.c_str(), "%f %f %f %f %d %d %d", &x, &y, &z, &intensity, &r, &g, &b) == 7) {
                        PointCloudPoint point;
                        point.position = glm::vec3(x, y, z);
                        point.intensity = 1.0f;
                        point.color = glm::vec3(r / 255.0f, g / 255.0f, b / 255.0f);
                        localPoints.push_back(point);
                    }
                }
                pointCounter++;
            }

            {
                std::lock_guard<std::mutex> lock(pointCloudMutex);
                pointCloud.points.insert(pointCloud.points.end(), localPoints.begin(), localPoints.end());
            }

            totalPointsProcessed += pointCounter;
            };

        while (file) {
            file.read(buffer.data(), bufferSize);
            std::streamsize bytesRead = file.gcount();

            if (bytesRead > 0) {
                threads.emplace_back(processChunk, buffer, bytesRead);

                if (threads.size() >= numThreads) {
                    for (auto& thread : threads) {
                        thread.join();
                    }
                    threads.clear();
                }
            }
        }

        for (auto& thread : threads) {
            thread.join();
        }

        file.close();

        std::cout << "Total points in file: " << totalPointsProcessed << std::endl;
        std::cout << "Points loaded after downsampling: " << pointCloud.points.size() << std::endl;

        setupPointCloudGLBuffers(pointCloud);

        generateChunks(pointCloud, 10.0f);

        pointCloud.instanceMatrices.reserve(pointCloud.points.size());
        for (const auto& point : pointCloud.points) {
            glm::mat4 instanceMatrix = glm::translate(glm::mat4(1.0f), point.position);
            // can add rotation and scaling here if needed
            pointCloud.instanceMatrices.push_back(instanceMatrix);
        }
        pointCloud.instanceCount = pointCloud.instanceMatrices.size();

        // Create and populate instance VBO
        glGenBuffers(1, &pointCloud.instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, pointCloud.instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, pointCloud.instanceCount * sizeof(glm::mat4),
            pointCloud.instanceMatrices.data(), GL_STATIC_DRAW);

        // Set up instanced array
        glBindVertexArray(pointCloud.vao);
        for (unsigned int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(3 + i);
            glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                (void*)(sizeof(glm::vec4) * i));
            glVertexAttribDivisor(3 + i, 1);
        }
        glBindVertexArray(0);

        return pointCloud;
    }

    bool PointCloudLoader::exportToXYZ(const PointCloud& pointCloud, const std::string& filePath) {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filePath << std::endl;
            return false;
        }

        // Create transformation matrix
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, pointCloud.position);
        transform = glm::rotate(transform, glm::radians(pointCloud.rotation.x), glm::vec3(1, 0, 0));
        transform = glm::rotate(transform, glm::radians(pointCloud.rotation.y), glm::vec3(0, 1, 0));
        transform = glm::rotate(transform, glm::radians(pointCloud.rotation.z), glm::vec3(0, 0, 1));
        transform = glm::scale(transform, pointCloud.scale);

        for (const auto& point : pointCloud.points) {
            // Apply transformation
            glm::vec4 transformedPos = transform * glm::vec4(point.position, 1.0f);

            file << std::fixed << std::setprecision(3)
                << transformedPos.x << " "
                << transformedPos.y << " "
                << transformedPos.z << " "
                << static_cast<int>(point.intensity * 1000) << " "
                << static_cast<int>(point.color.r * 255) << " "
                << static_cast<int>(point.color.g * 255) << " "
                << static_cast<int>(point.color.b * 255) << "\n";
        }

        file.close();
        return true;
    }

    bool PointCloudLoader::exportToBinary(const PointCloud& pointCloud, const std::string& filePath) {
        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filePath << std::endl;
            return false;
        }

        // Create transformation matrix
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, pointCloud.position);
        transform = glm::rotate(transform, glm::radians(pointCloud.rotation.x), glm::vec3(1, 0, 0));
        transform = glm::rotate(transform, glm::radians(pointCloud.rotation.y), glm::vec3(0, 1, 0));
        transform = glm::rotate(transform, glm::radians(pointCloud.rotation.z), glm::vec3(0, 0, 1));
        transform = glm::scale(transform, pointCloud.scale);

        // Write header
        file.write(BINARY_MAGIC_NUMBER, 4);
        uint32_t numPoints = pointCloud.points.size();
        file.write(reinterpret_cast<const char*>(&numPoints), sizeof(numPoints));

        // Write point data
        for (const auto& point : pointCloud.points) {
            // Apply transformation
            glm::vec4 transformedPos = transform * glm::vec4(point.position, 1.0f);
            glm::vec3 finalPos(transformedPos);

            file.write(reinterpret_cast<const char*>(&finalPos), sizeof(finalPos));

            uint32_t intensity = static_cast<uint32_t>(point.intensity * 1000);
            file.write(reinterpret_cast<const char*>(&intensity), sizeof(intensity));

            glm::u8vec3 color = glm::u8vec3(point.color * 255.0f);
            file.write(reinterpret_cast<const char*>(&color), sizeof(color));
        }

        file.close();
        return true;
    }

    struct IVec3Comparator{
    bool operator()(const glm::ivec3 & lhs, const glm::ivec3 & rhs) const {
        if (lhs.x != rhs.x) return lhs.x < rhs.x;
        if (lhs.y != rhs.y) return lhs.y < rhs.y;
        return lhs.z < rhs.z;
    }
    };

    PointCloud PointCloudLoader::loadFromBinary(const std::string& filePath) {
        PointCloud pointCloud;
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for reading: " << filePath << std::endl;
            return pointCloud;
        }

        try {
            // Read and verify header
            char magic[4];
            file.read(magic, 4);
            if (std::memcmp(magic, BINARY_MAGIC_NUMBER, 4) != 0) {
                throw std::runtime_error("Invalid binary point cloud file format");
            }

            uint32_t numPoints;
            file.read(reinterpret_cast<char*>(&numPoints), sizeof(numPoints));

            pointCloud.points.reserve(numPoints);

            const size_t pointSize = sizeof(glm::vec3) + sizeof(uint32_t) + sizeof(glm::u8vec3);
            constexpr size_t bufferSize = 1024 * 1024; // 1 MB buffer
            const size_t pointsPerBuffer = bufferSize / pointSize;

            std::vector<char> buffer(bufferSize);
            std::mutex pointCloudMutex;
            std::atomic<size_t> totalPointsProcessed(0);

            const size_t numThreads = std::thread::hardware_concurrency();
            std::vector<std::future<void>> futures;

            auto processChunk = [&](const std::vector<char>& chunk, size_t numPoints) {
                std::vector<PointCloudPoint> localPoints;
                localPoints.reserve(numPoints);

                const char* data = chunk.data();
                for (size_t i = 0; i < numPoints; ++i) {
                    PointCloudPoint point;
                    std::memcpy(&point.position, data, sizeof(point.position));
                    data += sizeof(point.position);

                    uint32_t intensity;
                    std::memcpy(&intensity, data, sizeof(intensity));
                    point.intensity = intensity / 1000.0f;
                    data += sizeof(intensity);

                    glm::u8vec3 color;
                    std::memcpy(&color, data, sizeof(color));
                    point.color = glm::vec3(color) / 255.0f;
                    data += sizeof(color);

                    localPoints.push_back(point);
                }

                {
                    std::lock_guard<std::mutex> lock(pointCloudMutex);
                    pointCloud.points.insert(pointCloud.points.end(), localPoints.begin(), localPoints.end());
                }

                totalPointsProcessed += localPoints.size();
                };

            while (totalPointsProcessed < numPoints) {
                size_t remainingPoints = numPoints - totalPointsProcessed;
                size_t pointsToRead = std::min(pointsPerBuffer, remainingPoints);
                size_t bytesToRead = pointsToRead * pointSize;

                buffer.resize(bytesToRead);
                file.read(buffer.data(), bytesToRead);
                std::streamsize bytesRead = file.gcount();

                if (bytesRead > 0) {
                    size_t actualPointsRead = bytesRead / pointSize;
                    futures.push_back(std::async(std::launch::async, processChunk, buffer, actualPointsRead));

                    if (futures.size() >= numThreads) {
                        for (auto& future : futures) {
                            future.wait();
                        }
                        futures.clear();
                    }
                }
            }

            for (auto& future : futures) {
                future.wait();
            }

            file.close();

            // Initialize transformation values
            pointCloud.position = glm::vec3(0.0f);
            pointCloud.rotation = glm::vec3(0.0f);
            pointCloud.scale = glm::vec3(1.0f);

            setupPointCloudGLBuffers(pointCloud);

            std::cout << "Successfully loaded point cloud from: " << filePath << std::endl;
            std::cout << "Loaded " << pointCloud.points.size() << " points" << std::endl;

        }
        catch (const std::exception& e) {
            std::cerr << "Error loading point cloud: " << e.what() << std::endl;
            pointCloud = PointCloud(); // Reset to empty point cloud
        }

        generateChunks(pointCloud, 10.0f);

        pointCloud.instanceMatrices.reserve(pointCloud.points.size());
        for (const auto& point : pointCloud.points) {
            glm::mat4 instanceMatrix = glm::translate(glm::mat4(1.0f), point.position);
            // You can add rotation and scaling here if needed
            pointCloud.instanceMatrices.push_back(instanceMatrix);
        }
        pointCloud.instanceCount = pointCloud.instanceMatrices.size();

        // Create and populate instance VBO
        glGenBuffers(1, &pointCloud.instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, pointCloud.instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, pointCloud.instanceCount * sizeof(glm::mat4),
            pointCloud.instanceMatrices.data(), GL_STATIC_DRAW);

        // Set up instanced array
        glBindVertexArray(pointCloud.vao);
        for (unsigned int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(3 + i);
            glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                (void*)(sizeof(glm::vec4) * i));
            glVertexAttribDivisor(3 + i, 1);
        }
        glBindVertexArray(0);

        return pointCloud;
    }


    void generateChunkOutlineVertices(PointCloud& pointCloud) {
        pointCloud.chunkOutlineVertices.clear();

        for (const auto& chunk : pointCloud.chunks) {
            glm::vec3 minBound = chunk.centerPosition - glm::vec3(pointCloud.chunkSize / 2.0f);
            glm::vec3 maxBound = chunk.centerPosition + glm::vec3(pointCloud.chunkSize / 2.0f);

            // Front face
            pointCloud.chunkOutlineVertices.push_back(minBound);
            pointCloud.chunkOutlineVertices.push_back(glm::vec3(maxBound.x, minBound.y, minBound.z));

            pointCloud.chunkOutlineVertices.push_back(glm::vec3(maxBound.x, minBound.y, minBound.z));
            pointCloud.chunkOutlineVertices.push_back(glm::vec3(maxBound.x, maxBound.y, minBound.z));

            pointCloud.chunkOutlineVertices.push_back(glm::vec3(maxBound.x, maxBound.y, minBound.z));
            pointCloud.chunkOutlineVertices.push_back(glm::vec3(minBound.x, maxBound.y, minBound.z));

            pointCloud.chunkOutlineVertices.push_back(glm::vec3(minBound.x, maxBound.y, minBound.z));
            pointCloud.chunkOutlineVertices.push_back(minBound);

            // Back face
            pointCloud.chunkOutlineVertices.push_back(glm::vec3(minBound.x, minBound.y, maxBound.z));
            pointCloud.chunkOutlineVertices.push_back(glm::vec3(maxBound.x, minBound.y, maxBound.z));

            pointCloud.chunkOutlineVertices.push_back(glm::vec3(maxBound.x, minBound.y, maxBound.z));
            pointCloud.chunkOutlineVertices.push_back(maxBound);

            pointCloud.chunkOutlineVertices.push_back(maxBound);
            pointCloud.chunkOutlineVertices.push_back(glm::vec3(minBound.x, maxBound.y, maxBound.z));

            pointCloud.chunkOutlineVertices.push_back(glm::vec3(minBound.x, maxBound.y, maxBound.z));
            pointCloud.chunkOutlineVertices.push_back(glm::vec3(minBound.x, minBound.y, maxBound.z));

            // Connecting lines
            pointCloud.chunkOutlineVertices.push_back(minBound);
            pointCloud.chunkOutlineVertices.push_back(glm::vec3(minBound.x, minBound.y, maxBound.z));

            pointCloud.chunkOutlineVertices.push_back(glm::vec3(maxBound.x, minBound.y, minBound.z));
            pointCloud.chunkOutlineVertices.push_back(glm::vec3(maxBound.x, minBound.y, maxBound.z));

            pointCloud.chunkOutlineVertices.push_back(glm::vec3(maxBound.x, maxBound.y, minBound.z));
            pointCloud.chunkOutlineVertices.push_back(maxBound);

            pointCloud.chunkOutlineVertices.push_back(glm::vec3(minBound.x, maxBound.y, minBound.z));
            pointCloud.chunkOutlineVertices.push_back(glm::vec3(minBound.x, maxBound.y, maxBound.z));
        }

        // Create and bind VAO and VBO for chunk outlines
        glGenVertexArrays(1, &pointCloud.chunkOutlineVAO);
        glGenBuffers(1, &pointCloud.chunkOutlineVBO);

        glBindVertexArray(pointCloud.chunkOutlineVAO);
        glBindBuffer(GL_ARRAY_BUFFER, pointCloud.chunkOutlineVBO);
        glBufferData(GL_ARRAY_BUFFER, pointCloud.chunkOutlineVertices.size() * sizeof(glm::vec3), pointCloud.chunkOutlineVertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    glm::vec3 calculateTransformedChunkPosition(const glm::vec3& chunkPos, const glm::mat4& modelMatrix) {
        glm::vec4 transformedPos = modelMatrix * glm::vec4(chunkPos, 1.0f);
        return glm::vec3(transformedPos);
    }

    void generateChunks(PointCloud& pointCloud, float chunkSize) {
        // Clean up old chunks
        for (auto& chunk : pointCloud.chunks) {
            for (GLuint vbo : chunk.lodVBOs) {
                if (vbo != 0) {
                    glDeleteBuffers(1, &vbo);
                }
            }
        }

        pointCloud.chunkSize = chunkSize;
        pointCloud.chunks.clear();

        // Use an unordered_map with a custom hash function for 3D indices
        struct ChunkIndexHash {
            std::size_t operator()(const glm::ivec3& index) const {
                std::size_t h1 = std::hash<int>()(index.x);
                std::size_t h2 = std::hash<int>()(index.y);
                std::size_t h3 = std::hash<int>()(index.z);
                return h1 ^ (h2 << 1) ^ (h3 << 2);
            }
        };

        std::unordered_map<glm::ivec3, std::vector<PointCloudPoint>, ChunkIndexHash> chunkMap;

        // Create transformation matrix
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, pointCloud.position);
        modelMatrix = glm::rotate(modelMatrix, glm::radians(pointCloud.rotation.x), glm::vec3(1, 0, 0));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(pointCloud.rotation.y), glm::vec3(0, 1, 0));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(pointCloud.rotation.z), glm::vec3(0, 0, 1));
        modelMatrix = glm::scale(modelMatrix, pointCloud.scale);

        // Populate the chunk map based on point positions
        for (const auto& point : pointCloud.points) {
            // Transform the point position
            glm::vec3 transformedPos = glm::vec3(modelMatrix * glm::vec4(point.position, 1.0f));
            glm::ivec3 chunkIndex = glm::floor(transformedPos / chunkSize);
            chunkMap[chunkIndex].push_back(point);
        }

        // Create chunks from the map
        for (auto& [chunkIndex, points] : chunkMap) {
            PointCloudChunk chunk;
            chunk.points = std::move(points);

            // Calculate chunk center position
            chunk.centerPosition = glm::vec3(
                (chunkIndex.x + 0.5f) * chunkSize,
                (chunkIndex.y + 0.5f) * chunkSize,
                (chunkIndex.z + 0.5f) * chunkSize
            );

            // Calculate bounding radius
            float maxDistSq = 0.0f;
            for (const auto& point : chunk.points) {
                glm::vec3 diff = point.position - chunk.centerPosition;
                float distSq = glm::dot(diff, diff);
                maxDistSq = std::max(maxDistSq, distSq);
            }
            chunk.boundingRadius = std::sqrt(maxDistSq);

            // Generate LOD levels for the chunk
            generateLODLevels(chunk);

            pointCloud.chunks.push_back(std::move(chunk));
        }

        generateChunkOutlineVertices(pointCloud);
    }


    void generateLODLevels(PointCloudChunk& chunk) {
        const int numLODLevels = 5;
        chunk.lodVBOs.resize(numLODLevels);
        chunk.lodPointCounts.resize(numLODLevels);

        std::vector<std::vector<PointCloudPoint>> lodPoints(numLODLevels);
        lodPoints[0] = chunk.points;

        const size_t thresholds[] = {
            std::numeric_limits<size_t>::max(),
            600000 * chunk.boundingRadius,
            100000 * chunk.boundingRadius,
            50000 * chunk.boundingRadius,
            30000 * chunk.boundingRadius
        };

        glm::vec3 chunkMin(std::numeric_limits<float>::max());
        glm::vec3 chunkMax(std::numeric_limits<float>::lowest());

        const int gridSize = 8;
        const int gridCells = gridSize * gridSize * gridSize;
        std::vector<std::vector<PointCloudPoint>> grid(gridCells);
        std::vector<std::mutex> mutexes(gridCells);

#pragma omp parallel
        {
#pragma omp for schedule(dynamic) reduction(min:chunkMin) reduction(max:chunkMax)
            for (int i = 0; i < chunk.points.size(); i++) {
                chunkMin = glm::min(chunkMin, chunk.points[i].position);
                chunkMax = glm::max(chunkMax, chunk.points[i].position);
            }

#pragma omp for schedule(dynamic)
            for (int i = 0; i < chunk.points.size(); i++) {
                const auto& point = chunk.points[i];
                glm::vec3 normalized = (point.position - chunkMin) / (chunkMax - chunkMin);
                int idx = glm::clamp(int(normalized.x * gridSize), 0, gridSize - 1) +
                    glm::clamp(int(normalized.y * gridSize), 0, gridSize - 1) * gridSize +
                    glm::clamp(int(normalized.z * gridSize), 0, gridSize - 1) * gridSize * gridSize;

                std::lock_guard<std::mutex> lock(mutexes[idx]);
                grid[idx].push_back(point);
            }
        }

        for (int level = 1; level < numLODLevels; level++) {
            if (chunk.points.size() <= thresholds[level]) {
                lodPoints[level] = chunk.points;
                continue;
            }

            size_t targetCount = std::min(thresholds[level], lodPoints[level - 1].size() / 2);
            std::vector<PointCloudPoint> selectedPoints;
            selectedPoints.reserve(targetCount);

            size_t basePointsPerCell = targetCount / gridCells;
            size_t remainder = targetCount % gridCells;

#pragma omp parallel for schedule(dynamic)
            for (int cell = 0; cell < gridCells; cell++) {
                std::vector<PointCloudPoint> cellPoints;
                size_t cellTargetCount = basePointsPerCell + (cell < remainder ? 1 : 0);

                if (grid[cell].size() <= cellTargetCount) {
                    cellPoints = grid[cell];
                }
                else {
                    cellPoints.reserve(cellTargetCount);
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::vector<size_t> indices(grid[cell].size());
                    std::iota(indices.begin(), indices.end(), 0);
                    std::shuffle(indices.begin(), indices.end(), gen);

                    for (size_t i = 0; i < cellTargetCount; i++) {
                        cellPoints.push_back(grid[cell][indices[i]]);
                    }
                }

#pragma omp critical
                {
                    selectedPoints.insert(selectedPoints.end(), cellPoints.begin(), cellPoints.end());
                }
            }

            lodPoints[level] = std::move(selectedPoints);
        }

        for (int i = 0; i < numLODLevels; i++) {
            chunk.lodPointCounts[i] = lodPoints[i].size();
            if (!lodPoints[i].empty()) {
                glGenBuffers(1, &chunk.lodVBOs[i]);
                glBindBuffer(GL_ARRAY_BUFFER, chunk.lodVBOs[i]);
                glBufferData(GL_ARRAY_BUFFER,
                    lodPoints[i].size() * sizeof(PointCloudPoint),
                    lodPoints[i].data(),
                    GL_STATIC_DRAW);
            }
            else {
                chunk.lodVBOs[i] = 0;
            }
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }


   

    void PointCloudLoader::setupPointCloudGLBuffers(PointCloud& pointCloud) {
        glGenVertexArrays(1, &pointCloud.vao);
        glGenBuffers(1, &pointCloud.vbo);

        glBindVertexArray(pointCloud.vao);
        glBindBuffer(GL_ARRAY_BUFFER, pointCloud.vbo);
        glBufferData(GL_ARRAY_BUFFER, pointCloud.points.size() * sizeof(PointCloudPoint), pointCloud.points.data(), GL_STATIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PointCloudPoint), (void*)0);
        glEnableVertexAttribArray(0);

        // Color attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(PointCloudPoint), (void*)offsetof(PointCloudPoint, color));
        glEnableVertexAttribArray(1);

        // Intensity attribute
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(PointCloudPoint), (void*)offsetof(PointCloudPoint, intensity));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }

} // namespace Engine