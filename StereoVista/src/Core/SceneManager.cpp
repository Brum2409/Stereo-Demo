#include "Core/SceneManager.h"
#include <fstream>
#include <iostream>
#include <json.h>
#include "Loaders/PointCloudLoader.h"
#include <filesystem>
#include <unordered_set>
#include <utility>

using json = nlohmann::json;

namespace Engine {

    void saveScene(const std::string& filename, const Scene& scene, const Camera& camera) {
        try {
            // Ensure filename has .scene extension
            std::filesystem::path scenePath(filename);
            if (scenePath.extension() != ".scene") {
                scenePath.replace_extension(".scene");
            }

            std::string sceneName = scenePath.stem().string();
            std::filesystem::path sceneDir = scenePath.parent_path() / sceneName;

            // Create directory structure
            std::filesystem::create_directories(sceneDir / "models");
            std::filesystem::create_directories(sceneDir / "pointClouds");

            // Create scene data json
            json sceneJson;
            sceneJson["settings"]["separation"] = scene.settings.separation;
            sceneJson["settings"]["convergence"] = scene.settings.convergence;
            sceneJson["settings"]["nearPlane"] = scene.settings.nearPlane;
            sceneJson["settings"]["farPlane"] = scene.settings.farPlane;

            // Save current camera state
            auto cameraState = camera.GetState();
            sceneJson["camera"]["position"] = {cameraState.position.x, cameraState.position.y, cameraState.position.z};
            sceneJson["camera"]["front"] = {cameraState.front.x, cameraState.front.y, cameraState.front.z};
            sceneJson["camera"]["up"] = {cameraState.up.x, cameraState.up.y, cameraState.up.z};
            sceneJson["camera"]["yaw"] = cameraState.yaw;
            sceneJson["camera"]["pitch"] = cameraState.pitch;
            sceneJson["camera"]["zoom"] = cameraState.zoom;

            // Save models
            json modelsJson = json::array();
            for (const auto& model : scene.models) {
                json modelJson;
                modelJson["name"] = model.name;
                modelJson["path"] = model.path;
                modelJson["position"] = { model.position.x, model.position.y, model.position.z };
                modelJson["scale"] = { model.scale.x, model.scale.y, model.scale.z };
                modelJson["rotation"] = { model.rotation.x, model.rotation.y, model.rotation.z };
                modelJson["color"] = { model.color.r, model.color.g, model.color.b };
                modelJson["shininess"] = model.shininess;
                modelJson["emissive"] = model.emissive;
                modelJson["visible"] = model.visible;

                // Check if this is a file-based model (not a primitive)
                bool isPrimitive = (model.path == "cube" || model.path == "sphere" || 
                                  model.path == "cylinder" || model.path == "plane" || 
                                  model.path == "torus" || model.path.empty());
                
                // Save primitive type for proper reconstruction
                if (isPrimitive && !model.path.empty()) {
                    modelJson["primitiveType"] = model.path;
                }
                
                if (!model.path.empty() && !isPrimitive) {
                    // Create model-specific directory
                    std::filesystem::path modelDir = sceneDir / "models" / model.name;
                    std::filesystem::create_directories(modelDir);

                    // Copy original model file
                    std::filesystem::path originalModelPath(model.path);
                    std::string newModelFilename = originalModelPath.filename().string();
                    std::filesystem::path newModelPath = modelDir / newModelFilename;

                    try {
                        std::filesystem::copy_file(originalModelPath, newModelPath,
                            std::filesystem::copy_options::overwrite_existing);
                        modelJson["localPath"] = "models/" + model.name + "/" + newModelFilename;

                        // Process and save texture information
                        json texturesJson = json::array();
                        std::unordered_set<std::string> processedTextures;

                        // Process each mesh's textures
                        for (const auto& mesh : model.getMeshes()) {
                            for (const auto& texture : mesh.textures) {
                                if (texture.fullPath.empty()) {
                                    continue;
                                }

                                // Create unique identifier for texture
                                std::string textureIdentifier = texture.type + "|" + texture.path;
                                if (processedTextures.find(textureIdentifier) != processedTextures.end()) {
                                    continue;
                                }
                                processedTextures.insert(textureIdentifier);

                                // Copy texture file
                                try {
                                    std::filesystem::path texturePath(texture.fullPath);
                                    if (!std::filesystem::exists(texturePath)) {
                                        std::cerr << "Texture file not found: " << texturePath << std::endl;
                                        continue;
                                    }

                                    std::string newTextureName = texturePath.filename().string();
                                    std::filesystem::path newTexturePath = modelDir / newTextureName;

                                    std::filesystem::copy_file(texturePath, newTexturePath,
                                        std::filesystem::copy_options::overwrite_existing);

                                    json textureJson;
                                    textureJson["type"] = texture.type;
                                    textureJson["originalPath"] = texture.path;
                                    textureJson["localPath"] = (model.name + "/" + newTextureName);
                                    texturesJson.push_back(textureJson);
                                }
                                catch (const std::exception& e) {
                                    std::cerr << "Failed to copy texture " << texture.path << ": " << e.what() << std::endl;
                                    continue; // Continue with other textures even if one fails
                                }
                            }
                        }

                        // Only add textures array if we actually have textures
                        if (!texturesJson.empty()) {
                            modelJson["textures"] = texturesJson;
                        }
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Failed to process model " << model.name << ": " << e.what() << std::endl;
                        // Still add the model JSON even if texture processing fails
                    }
                }

                modelsJson.push_back(modelJson);
            }
            sceneJson["models"] = modelsJson;

            // Save point clouds
            json pointCloudsJson = json::array();
            for (const auto& pointCloud : scene.pointClouds) {
                json pointCloudJson;
                pointCloudJson["name"] = pointCloud.name;
                pointCloudJson["position"] = { pointCloud.position.x, pointCloud.position.y, pointCloud.position.z };
                pointCloudJson["rotation"] = { pointCloud.rotation.x, pointCloud.rotation.y, pointCloud.rotation.z };
                pointCloudJson["scale"] = { pointCloud.scale.x, pointCloud.scale.y, pointCloud.scale.z };

                std::string pcFilename = pointCloud.name + ".pcb";
                std::filesystem::path pcPath = sceneDir / "pointClouds" / pcFilename;
                Engine::PointCloudLoader::exportToBinary(pointCloud, pcPath.string());

                pointCloudJson["dataPath"] = "pointClouds/" + pcFilename;
                pointCloudsJson.push_back(pointCloudJson);
            }
            sceneJson["pointClouds"] = pointCloudsJson;

            // Write scene file with chunking support
            std::string jsonStr = sceneJson.dump(4);
            const size_t maxChunkSize = 100 * 1024 * 1024; // 100MB chunks

            if (jsonStr.size() > maxChunkSize) {
                // Split into multiple files if too large
                size_t numChunks = (jsonStr.size() + maxChunkSize - 1) / maxChunkSize;

                for (size_t i = 0; i < numChunks; i++) {
                    std::string chunkFilename = scenePath.string() + "." + std::to_string(i);
                    std::ofstream chunkFile(chunkFilename);
                    if (!chunkFile.is_open()) {
                        throw std::runtime_error("Failed to create scene chunk file: " + chunkFilename);
                    }

                    size_t start = i * maxChunkSize;
                    size_t length = std::min(maxChunkSize, jsonStr.size() - start);
                    chunkFile << jsonStr.substr(start, length);
                }

                // Write metadata file
                std::ofstream metaFile(scenePath);
                json metaJson;
                metaJson["numChunks"] = numChunks;
                metaFile << std::setw(4) << metaJson << std::endl;
            }
            else {
                // Write single file
                std::ofstream sceneFile(scenePath);
                if (!sceneFile.is_open()) {
                    throw std::runtime_error("Failed to create scene file: " + scenePath.string());
                }
                sceneFile << jsonStr;
            }
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Failed to save scene: " + std::string(e.what()));
        }
    }

    Scene loadScene(const std::string& filename, Camera& camera) {
        Scene scene;
        json sceneJson;
        std::filesystem::path scenePath;
        std::filesystem::path sceneDir;

        try {
            // Check if this is a chunked file
            std::ifstream metaFile(filename);
            if (!metaFile.is_open()) {
                throw std::runtime_error("Failed to open scene file: " + filename);
            }

            if (metaFile.is_open()) {
                json metaJson;
                metaFile >> metaJson;

                if (metaJson.contains("numChunks")) {
                    // Load chunked file
                    std::string combinedJson;
                    size_t numChunks = metaJson["numChunks"];

                    for (size_t i = 0; i < numChunks; i++) {
                        std::string chunkFilename = filename + "." + std::to_string(i);
                        std::ifstream chunkFile(chunkFilename);
                        if (!chunkFile.is_open()) {
                            throw std::runtime_error("Failed to open scene chunk file: " + chunkFilename);
                        }

                        std::string chunk((std::istreambuf_iterator<char>(chunkFile)),
                            std::istreambuf_iterator<char>());
                        combinedJson += chunk;
                    }
                    sceneJson = json::parse(combinedJson);
                }
                else {
                    // Regular single file
                    metaFile.seekg(0);
                    metaFile >> sceneJson;
                }
            }

            // Get scene directory
            scenePath = std::filesystem::path(filename);
            sceneDir = scenePath.parent_path() / scenePath.stem();

            // Load scene settings if they exist
            if (sceneJson.contains("settings")) {
                const auto& settings = sceneJson["settings"];
                scene.settings.separation = settings.value("separation", 0.5f);
                scene.settings.convergence = settings.value("convergence", 2.6f);
                scene.settings.nearPlane = settings.value("nearPlane", 0.1f);
                scene.settings.farPlane = settings.value("farPlane", 200.0f);
            }

            // Load camera state if it exists in the scene
            if (sceneJson.contains("camera")) {
                const auto& cameraJson = sceneJson["camera"];
                
                // Position
                if (cameraJson.contains("position") && cameraJson["position"].is_array() && 
                    cameraJson["position"].size() == 3) {
                    scene.cameraState.position = {
                        cameraJson["position"][0].get<float>(),
                        cameraJson["position"][1].get<float>(),
                        cameraJson["position"][2].get<float>()
                    };
                }
                
                // Front vector
                if (cameraJson.contains("front") && cameraJson["front"].is_array() && 
                    cameraJson["front"].size() == 3) {
                    scene.cameraState.front = {
                        cameraJson["front"][0].get<float>(),
                        cameraJson["front"][1].get<float>(),
                        cameraJson["front"][2].get<float>()
                    };
                }
                
                // Up vector
                if (cameraJson.contains("up") && cameraJson["up"].is_array() && 
                    cameraJson["up"].size() == 3) {
                    scene.cameraState.up = {
                        cameraJson["up"][0].get<float>(),
                        cameraJson["up"][1].get<float>(),
                        cameraJson["up"][2].get<float>()
                    };
                }
                
                // Other camera properties
                if (cameraJson.contains("yaw")) {
                    scene.cameraState.yaw = cameraJson["yaw"].get<float>();
                    camera.Yaw = scene.cameraState.yaw;
                }
                if (cameraJson.contains("pitch")) {
                    scene.cameraState.pitch = cameraJson["pitch"].get<float>();
                    camera.Pitch = scene.cameraState.pitch;
                }
                if (cameraJson.contains("zoom")) {
                    scene.cameraState.zoom = cameraJson["zoom"].get<float>();
                    camera.Zoom = scene.cameraState.zoom;
                }
                
                // Update camera vectors
                camera.Position = scene.cameraState.position;
                camera.Front = scene.cameraState.front;
                camera.Up = scene.cameraState.up;
                camera.updateCameraVectors();
            }

            // Load models
            if (sceneJson.contains("models")) {
                for (const auto& modelJson : sceneJson["models"]) {
                    try {
                        Model model;
                        
                        // Check if this is a model with a local path or a cube
                        if (modelJson.contains("localPath")) {
                            std::filesystem::path modelPath = sceneDir / modelJson["localPath"].get<std::string>();
                            
                            // Load model from file
                            model = *Engine::loadModel(modelPath.string());
                            model.path = modelJson["path"].get<std::string>();
                            model.directory = modelPath.parent_path().string();

                            // Clear existing textures before loading new ones
                            for (auto& mesh : model.getMeshes()) {
                                mesh.textures.clear();
                            }

                            // Load textures if present
                            if (modelJson.contains("textures")) {
                                std::unordered_set<std::string> loadedTextures;

                                for (const auto& textureJson : modelJson["textures"]) {
                                    std::string identifier = textureJson["type"].get<std::string>() + "|" +
                                        textureJson["originalPath"].get<std::string>();

                                    if (loadedTextures.find(identifier) == loadedTextures.end()) {
                                        loadedTextures.insert(identifier);

                                        Texture texture;
                                        texture.type = textureJson["type"];
                                        texture.path = textureJson["originalPath"];

                                        std::filesystem::path texturePath = modelPath.parent_path() /
                                            textureJson["localPath"].get<std::string>();
                                        texture.fullPath = texturePath.string();

                                        texture.id = Model::TextureFromFile(
                                            texturePath.filename().string().c_str(),
                                            texturePath.parent_path().string(),
                                            texture.fullPath
                                        );

                                        // Get the mesh index if it exists
                                        size_t meshIndex = textureJson.value("meshIndex", 0);

                                        // Add the texture to specified mesh or all meshes if not specified
                                        if (meshIndex < model.getMeshes().size()) {
                                            model.getMeshes()[meshIndex].textures.push_back(texture);
                                        }
                                        else {
                                            // Add to all meshes if index is invalid
                                            for (auto& mesh : model.getMeshes()) {
                                                mesh.textures.push_back(texture);
                                            }
                                        }

                                        std::cout << "Loaded texture: " << texturePath.string() << std::endl;
                                    }
                                }
                            }
                        } else {
                            // Creating a primitive
                            glm::vec3 color = glm::vec3(
                                modelJson["color"][0].get<float>(),
                                modelJson["color"][1].get<float>(),
                                modelJson["color"][2].get<float>()
                            );
                            float shininess = modelJson.value("shininess", 1.0f);
                            float emissive = modelJson.value("emissive", 0.0f);
                            
                            // Check for primitiveType field first, then fall back to legacy cube detection
                            std::string primitiveType = modelJson.value("primitiveType", "cube");
                            
                            if (primitiveType == "sphere") {
                                model = Engine::createSphere(color, shininess, emissive);
                            }
                            else if (primitiveType == "cylinder") {
                                model = Engine::createCylinder(color, shininess, emissive);
                            }
                            else if (primitiveType == "plane") {
                                model = Engine::createPlane(color, shininess, emissive);
                            }
                            else if (primitiveType == "torus") {
                                model = Engine::createTorus(color, shininess, emissive);
                            }
                            else {
                                // Default to cube for legacy scenes or unknown types
                                model = Engine::createCube(color, shininess, emissive);
                            }

                            // Make sure color is explicitly set after creation
                            model.color = color;
                        }

                        // Set model properties
                        model.name = modelJson["name"];
                        model.position = glm::vec3(
                            modelJson["position"][0].get<float>(),
                            modelJson["position"][1].get<float>(),
                            modelJson["position"][2].get<float>()
                        );
                        model.scale = glm::vec3(
                            modelJson["scale"][0].get<float>(),
                            modelJson["scale"][1].get<float>(),
                            modelJson["scale"][2].get<float>()
                        );
                        model.rotation = glm::vec3(
                            modelJson["rotation"][0].get<float>(),
                            modelJson["rotation"][1].get<float>(),
                            modelJson["rotation"][2].get<float>()
                        );
                        model.color = glm::vec3(
                            modelJson["color"][0].get<float>(),
                            modelJson["color"][1].get<float>(),
                            modelJson["color"][2].get<float>()
                        );
                        model.shininess = modelJson.value("shininess", 1.0f);
                        model.emissive = modelJson.value("emissive", 0.0f);
                        model.visible = modelJson.value("visible", true);

                        scene.models.push_back(model);
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Failed to load model: " << e.what() << std::endl;
                    }
                }
            }

            // Load point clouds
            if (sceneJson.contains("pointClouds")) {
                for (const auto& pointCloudJson : sceneJson["pointClouds"]) {
                    try {
                        if (!pointCloudJson.contains("dataPath") ||
                            !pointCloudJson.contains("name") ||
                            !pointCloudJson.contains("position") ||
                            !pointCloudJson.contains("rotation") ||
                            !pointCloudJson.contains("scale")) {
                            throw std::runtime_error("Point cloud JSON missing required fields");
                        }

                        std::filesystem::path pcPath = sceneDir / pointCloudJson["dataPath"].get<std::string>();
                        PointCloud pointCloud = std::move(Engine::PointCloudLoader::loadFromBinary(pcPath.string()));

                        pointCloud.name = pointCloudJson["name"];
                        pointCloud.position = glm::vec3(
                            pointCloudJson["position"][0].get<float>(),
                            pointCloudJson["position"][1].get<float>(),
                            pointCloudJson["position"][2].get<float>()
                        );
                        pointCloud.rotation = glm::vec3(
                            pointCloudJson["rotation"][0].get<float>(),
                            pointCloudJson["rotation"][1].get<float>(),
                            pointCloudJson["rotation"][2].get<float>()
                        );
                        pointCloud.scale = glm::vec3(
                            pointCloudJson["scale"][0].get<float>(),
                            pointCloudJson["scale"][1].get<float>(),
                            pointCloudJson["scale"][2].get<float>()
                        );

                        scene.pointClouds.push_back(std::move(pointCloud));
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Failed to load point cloud: " << e.what() << std::endl;
                    }
                }
            }
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Failed to load scene: " + std::string(e.what()));
        }

        return scene;
    }

    void loadModelData(Model& model, const std::string& filename) {
        try {
            std::ifstream file(filename);
            if (!file.is_open()) {
                throw std::runtime_error("Failed to open model data file: " + filename);
            }

            json modelJson;
            file >> modelJson;

            model.position = glm::vec3(
                modelJson["position"][0].get<float>(),
                modelJson["position"][1].get<float>(),
                modelJson["position"][2].get<float>()
            );
            model.rotation = glm::vec3(
                modelJson["rotation"][0].get<float>(),
                modelJson["rotation"][1].get<float>(),
                modelJson["rotation"][2].get<float>()
            );
            model.scale = glm::vec3(
                modelJson["scale"][0].get<float>(),
                modelJson["scale"][1].get<float>(),
                modelJson["scale"][2].get<float>()
            );
            model.visible = modelJson.value("visible", true);
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Failed to load model data: " + std::string(e.what()));
        }
    }

    void saveModelData(const Model& model, const std::string& filename) {
        try {
            json modelJson;
            modelJson["position"] = { model.position.x, model.position.y, model.position.z };
            modelJson["rotation"] = { model.rotation.x, model.rotation.y, model.rotation.z };
            modelJson["scale"] = { model.scale.x, model.scale.y, model.scale.z };
            modelJson["visible"] = model.visible;

            std::ofstream file(filename);
            if (!file.is_open()) {
                throw std::runtime_error("Failed to create model data file: " + filename);
            }
            file << std::setw(4) << modelJson << std::endl;
        }
        catch (const std::exception& e) {
            throw std::runtime_error("Failed to save model data: " + std::string(e.what()));
        }
    }

}  // namespace Engine