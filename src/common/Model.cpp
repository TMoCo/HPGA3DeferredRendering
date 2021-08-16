//
// Definition of the model class
//



#include <array>
#include <string> // string class
#include <iostream>

#include <common/Model.h> // model class declaration

#include <utils/Utils.h>
#include <utils/Assert.h>

// model loading
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <glm/gtc/type_ptr.hpp> // construct vec from ptr
#include <glm/gtx/string_cast.hpp>

void Model::loadModel(const std::string& path) {
    FileExtension ext = getExtension(path);
    switch (ext) {
    case FileExtension::GLTF:
        loadGltfModel(path);
        return;
    case FileExtension::OBJ:
        loadObjModel(path);
        return;
    }
    throw std::runtime_error("Unsupported extension!");
}

void Model::loadObjModel(const std::string& path) {
    // setup variables to get model info
    tinyobj::attrib_t attrib; // contains all the positions, normals, textures and faces
    std::vector<tinyobj::shape_t> shapes; // all the separate objects and their faces
    std::vector<tinyobj::material_t> materials; // object materials
    std::string warn, err;

    // load the model, show error if not
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
        throw std::runtime_error(warn + err);
    }

    // combine all the shapes into a single model
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            // set vertex data
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.nor = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
            };

            vertex.tex = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };
            // add to the centre of gravity
            centre += vertex.pos;

            vertices.push_back(vertex);
            indices.push_back(static_cast<uint32_t>(indices.size()));
        }
    }
    // now compute the centre by dividing by the number of vertices in the model
    centre /= (float)vertices.size();
}

void Model::loadGltfModel(const std::string& path) {
    tinygltf::TinyGLTF loader;

    std::string err;
    std::string warn;

    // parse the gltf file
    loadStatus = loader.LoadASCIIFromFile(&model, &err, &warn, path);

    if (!warn.empty()) {
        std::cout << warn;
    }

    if (!err.empty()) {
        throw std::runtime_error(err);
    }
    
    if (!loadStatus) {
        throw std::runtime_error("Could not parse .gltf file");
    }

    // run some assertions to limit use for now
    m_assert(model.buffers.size() == 1, "Invalid number of buffers (only one buffer in a file supported)...");
    m_assert(model.meshes.size() == 1, "Invalid number of meshse (only single mesh models are currently supported)...");
    m_assert(model.meshes[0].primitives.size() == 1, "Invalid number of primitives (only single primitive meshes are currently supported)...");
}

Model::FileExtension Model::getExtension(const std::string& path) {
    auto pos = path.find_last_of('.');
    m_assert(pos != std::string::npos, "Invalid file path..."); // assert that the path name is valid
    std::string extension = path.substr(pos + 1);
    if (extension == "obj")
        return FileExtension::OBJ;
    else if (extension == "gltf")
        return FileExtension::GLTF;
    throw std::runtime_error("Unsupported extension!");
}

VkFormat Model::getFormatFromType(uint32_t type) {
    m_assert(type > 0 && type < 5, "Invalid type...");
    switch (type)
    {
    case 1:
        return VK_FORMAT_R32_SFLOAT;
    case 2:
        return VK_FORMAT_R32G32_SFLOAT;
    case 3:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case 4:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    return VK_FORMAT_MAX_ENUM; // keep compiler happy, never called
}

VkIndexType Model::getIndexType(uint32_t primitiveNum) {
    auto& primitive = model.meshes[0].primitives[0];

    tinygltf::Accessor indexAccessor = model.accessors[primitive.indices]; // get the indices for the primitive

    switch (indexAccessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        return VK_INDEX_TYPE_UINT16;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        return VK_INDEX_TYPE_UINT32;
    default:
        throw std::runtime_error("Invalid index type...");
    }
    return VK_INDEX_TYPE_MAX_ENUM; // keep compiler happy
}

std::vector<VkDeviceSize> Model::getBufferOffsets(uint32_t primitiveNum) {
    auto& primitive = model.meshes[0].primitives[0];

    // vector containing the offsets to each attribute
    std::vector<VkDeviceSize> offsets(primitive.attributes.size(), 0);
    return offsets;
}

uint32_t Model::getNumVertices(uint32_t primitiveNum) {
    tinygltf::Accessor accessor = model.accessors[model.meshes[0].primitives[primitiveNum].attributes.begin()->second];
    uint32_t numVerts = static_cast<uint32_t>(model.bufferViews[accessor.bufferView].byteLength / accessor.ByteStride(model.bufferViews[accessor.bufferView]));
    return numVerts;
}

uint32_t Model::getNumIndices(uint32_t primitiveNum) {
    tinygltf::Accessor idxAccessor = model.accessors[model.meshes[0].primitives[primitiveNum].indices];
    uint32_t numIndices = static_cast<uint32_t>(model.bufferViews[idxAccessor.bufferView].byteLength / idxAccessor.ByteStride(model.bufferViews[idxAccessor.bufferView]));
    return numIndices;
}

VkFormat Model::getImageFormat(uint32_t imgIdx) {
    // get the texture
    tinygltf::Image im = model.images[imgIdx];

    switch (getImageBitDepth(imgIdx)) {
    case 8:
        // component type = num channels
        switch (im.component) {
        case 1:
            return VK_FORMAT_R8_SRGB;
        case 2:
            return VK_FORMAT_R8G8_SRGB;
        case 3:
            return VK_FORMAT_R8G8B8_SRGB;
        case 4:
            return VK_FORMAT_R8G8B8A8_SRGB;
        }
        throw std::runtime_error("Could not determine image format (unsupported number of channels)!");
    // for now, assume that any higher format is in floating point
    case 16:
        switch (im.component) {
        case 1:
            return VK_FORMAT_R16_SFLOAT;
        case 2:
            return VK_FORMAT_R16G16_SFLOAT;
        case 3:
            return VK_FORMAT_R16G16B16_SFLOAT;
        case 4:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        }
        throw std::runtime_error("Could not determine image format (unsupported number of channels)!");
    case 32:
        switch (im.component) {
        case 1:
            return VK_FORMAT_R32_SFLOAT;
        case 2:
            return VK_FORMAT_R32G32_SFLOAT;
        case 3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case 4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        }
        throw std::runtime_error("Could not determine image format (unsupported number of channels)!");
    }
    throw std::runtime_error("Could not determine image format (unsupported bit depth)!");

    return VK_FORMAT_MAX_ENUM; // keep compiler happy
}
/* ********************************************** Debug Copy Paste **********************************************

#ifndef NDEBUG
    std::cout << "DEBUG\t" << __FUNCTION__ << '\n';
    std::cout << v << '\n';
    std::cout << std::endl;
    for (auto& v : arr) {

    }
#endif
    *****************************************************************************************************************/
uint32_t Model::getImageBitDepth(uint32_t imgIdx) {
    return model.images[imgIdx].bits;
}

VkVertexInputBindingDescription Model::getBindingDescriptions(uint32_t primitiveNum) {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding   = primitiveNum; // for now 1 primitive = 1 buffer
    bindingDescription.stride    = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 4> Model::getAttributeDescriptions(uint32_t primitiveNum) {
    auto& primitive = model.meshes[0].primitives[primitiveNum];

    m_assert(primitive.attributes.size() == 4, "Model must have four attributes...");
    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

    for (auto& attrib : primitive.attributes) {
        tinygltf::Accessor accessor = model.accessors[attrib.second];

        int size     = accessor.type != TINYGLTF_TYPE_SCALAR ? accessor.type : 1;
        int location = -1;
        int offset   = 0;
        if (attrib.first.compare("POSITION") == 0) {
            location = 0;
            offset   = offsetof(Vertex, pos);
        }
        else if (attrib.first.compare("NORMAL") == 0) {
            location = 1;
            offset   = offsetof(Vertex, nor);
        }
        else if (attrib.first.compare("TANGENT") == 0) {
            location = 2;
            offset   = offsetof(Vertex, tan);
        }
        else if (attrib.first.compare("TEXCOORD_0") == 0) {
            location = 3;
            offset   = offsetof(Vertex, tex);
        }
        else
            throw std::runtime_error("Invalid vertex attribute!");

        // use attrib num in map as index in array
        attributeDescriptions[attrib.second - 1].binding  = primitiveNum;
        attributeDescriptions[attrib.second - 1].location = location;
        attributeDescriptions[attrib.second - 1].format   = getFormatFromType(size); // returns the right format for appropriate type (size of vec)
        attributeDescriptions[attrib.second - 1].offset   = offset;
    }

    return attributeDescriptions;
}

std::vector<Model::Vertex>* Model::getVertexBuffer(uint32_t primitiveNum) {
    // create array for vertices
    vertices.resize(getNumVertices(primitiveNum));
    m_assert(vertices.size() > 0, "No vertex data in primitive...");

    // map from attribute index to buffer offset (assumes order: pos, norm, tan, tex)
    std::map<int, size_t> attributeOffsets;
    for (auto& attribute : model.meshes[0].primitives[primitiveNum].attributes) {
        
        attributeOffsets.insert({ attribute.second - 1, model.bufferViews[attribute.second].byteOffset });
    }

    tinygltf::Buffer buffer = model.buffers[0];

    // loop over each vertex and extract its attributes from the model buffer
    for (size_t i = 0; i < vertices.size(); i++) {
        vertices[i] = Vertex {
            glm::make_vec3(((float*)(buffer.data.data() + attributeOffsets[0] + (i * sizeof(glm::vec3))))), // pos
            glm::make_vec3(((float*)(buffer.data.data() + attributeOffsets[1] + (i * sizeof(glm::vec3))))), // norm
            glm::make_vec4(((float*)(buffer.data.data() + attributeOffsets[2] + (i * sizeof(glm::vec3))))), // tan
            glm::make_vec2(((float*)(buffer.data.data() + attributeOffsets[3] + (i * sizeof(glm::vec2)))))  // tex
        };
    }

    return &vertices;
}

std::vector<uint32_t>* Model::getIndexBuffer(uint32_t primitiveNum) {
    tinygltf::Accessor indexAccessor =  model.accessors[model.meshes[0].primitives[primitiveNum].indices];

    // get the number of indices 
    tinygltf::BufferView bView = model.bufferViews[indexAccessor.bufferView];
    indices.resize(getNumIndices(primitiveNum));

    tinygltf::Buffer buffer = model.buffers[bView.buffer];
    // copy the data into the indices buffer
    for (size_t i = 0; i < indices.size(); i++) {
        indices[i] = *((uint16_t*)(buffer.data.data() + bView.byteOffset + i * sizeof(uint16_t))); // cast UC to uint16, which is converted to uint32
    }

    return &indices;
}

const std::vector<Image>* Model::getMaterialTextureData(uint32_t primitiveNum) {
    tinygltf::Material material = model.materials[model.meshes[0].primitives[primitiveNum].material];

    m_assert((material.values.size() > 0) && (material.values.size() < 3), "Invalid number of textures in material (only support two)... ");
    textures.resize(material.values.size());

    // TODO: refactor away from dynamic vector with a material class
    auto texIter = textures.begin();
    for (auto& value : material.values) {
        int texIdx = value.second.TextureIndex();
        tinygltf::Image* im = &model.images[texIdx];
        // image info
        *texIter = { static_cast<uint32_t>(im->width), static_cast<uint32_t>(im->height), getImageFormat(texIdx), { im->image.data(), im->image.size() } };
        texIter++; // move on to next texture
    }

    return &textures;
}