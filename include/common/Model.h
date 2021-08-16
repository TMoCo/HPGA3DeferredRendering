///////////////////////////////////////////////////////
// Model class declaration
///////////////////////////////////////////////////////

//
// A model class for handling operations on data loaded from a file. 
//

#ifndef MODEL_H
#define MODEL_H

#include <hpg/Buffers.h> // buffers containing data

#include <common/Texture.h> 

#include <string> // string for model path
#include <vector> // vector container

#include <vulkan/vulkan_core.h>

#include <tiny_gltf.h>

#include <glm/glm.hpp> // shader vectors, matrices ...


class Model {
public:
    //-Vertex POD------------------------------------------------------------------------------------------------//
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 nor;
        glm::vec4 tan;
        glm::vec2 tex;
    };

public:
    //-Supported file formats------------------------------------------------------------------------------------//
    enum class FileExtension : unsigned char {
        OBJ  = 0x0,
        GLTF = 0x1
    };

public:
    //-Load Model------------------------------------------------------------------------------------------------//
    void loadModel(const std::string& path);
    void loadObjModel(const std::string& path);
    void loadGltfModel(const std::string& path);

    //-File utils------------------------------------------------------------------------------------------------//
    FileExtension getExtension(const std::string& path);

    //-Get model data--------------------------------------------------------------------------------------------//
    static VkFormat getFormatFromType(uint32_t type);
    VkIndexType getIndexType(uint32_t primitiveNum);
    std::vector<VkDeviceSize> getBufferOffsets(uint32_t primitiveNum);
    uint32_t getNumVertices(uint32_t primitiveNum);
    uint32_t getNumIndices(uint32_t primitiveNum);
    VkFormat getImageFormat(uint32_t imgIdx);
    uint32_t getImageBitDepth(uint32_t imgIdx);

    //-Binding and attribute descriptions------------------------------------------------------------------------//
    VkVertexInputBindingDescription getBindingDescriptions(uint32_t primitiveNum);
    std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions(uint32_t primitiveNum);

    //-Get buffers-----------------------------------------------------------------------------------------------//
    std::vector<Vertex>* getVertexBuffer(uint32_t primitiveNum);
    std::vector<uint32_t>* getIndexBuffer(uint32_t primitiveNum);

    //-Get material textures-------------------------------------------------------------------------------------//
    const std::vector<Image>* getMaterialTextureData(uint32_t primitiveNum);

private:
    //-Members---------------------------------------------------------------------------------------------------//
    tinygltf::Model model;

    glm::vec3 centre;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Image> textures;

    FileExtension ext;

    bool loadStatus;
};

#endif // !MODEL_H
