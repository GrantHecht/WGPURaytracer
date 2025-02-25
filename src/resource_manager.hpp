#ifndef _RESOURCE_MANAGER_H
#define _RESOURCE_MANAGER_H


#include <webgpu/webgpu.hpp>
#include <glm/glm.hpp>

#include <filesystem>
#include <vector>

struct VertexAttributes {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;
};

class ResourceManager {
public:
    /**
     * Load a file from `path` using our ad-hoc format and populate the `pointData`
     * and `indexData` vectors.
     */
    static bool loadGeometry(
        const std::filesystem::path& path,
        std::vector<float>& pointData,
        std::vector<uint16_t>& indexData,
        size_t dimensions
    );

    /**
     * Load an OBJ file from `path` and populate the std::vector<VertexAttributes>
     */
    static bool loadGeometryFromObj(
        const std::filesystem::path& path,
        std::vector<VertexAttributes>& vertexData
    );

    /**
     * Load an image file into a wgpu::Texture
     */
    static wgpu::Texture loadTexture(
        const std::filesystem::path& path, 
        wgpu::Device device,
        wgpu::TextureView* pTextureView = nullptr
    );


    static wgpu::ShaderModule loadShaderModule(
        const std::filesystem::path& path,
        wgpu::Device device
    );

private:

    static void writeMipMaps(
        wgpu::Device device, wgpu::Texture texture, 
        wgpu::Extent3D textureSize, uint32_t mipLevelCount,
        const unsigned char* pixelData
    );

    /**
     * Load a shader file from `path` and populate the `contents` string.
     */
    static bool readShaderFile(
        const std::filesystem::path& path,
        std::string& contents
    );

};

#endif