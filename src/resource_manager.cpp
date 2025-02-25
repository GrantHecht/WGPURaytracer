
#include "resource_manager.hpp"

#include "stb_image.h"
#include "tiny_obj_loader.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <bit>

bool ResourceManager::loadGeometry(
    const std::filesystem::path& path,
    std::vector<float>& pointData,
    std::vector<uint16_t>& indexData,
    size_t dimensions
) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    pointData.clear();
    indexData.clear();

    enum class Section {
        None,
        Points,
        Indices,
    };
    Section currentSection = Section::None;

    float value;
    uint16_t index;
    std::string line;
    while (!file.eof()) {
        getline(file, line);

        // overcome the `CRLF` problem
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line == "[points]") {
            currentSection = Section::Points;
        }
        else if (line == "[indices]") {
            currentSection = Section::Indices;
        }
        else if (line[0] == '#' || line.empty()) {
            continue;
        }
        else {
            std::istringstream iss(line);
            switch (currentSection) {
                case Section::Points:
                    for (size_t i = 0; i < dimensions + 3; i++) {
                        iss >> value;
                        pointData.push_back(value);
                    }
                    break;
                case Section::Indices:
                    for (size_t i = 0; i < 3; i++) {
                        iss >> index;
                        indexData.push_back(index);
                    }
                    break;
                default:
                    break;
            }
        }
    }
    return true;
}

bool ResourceManager::loadGeometryFromObj(
    const std::filesystem::path& path,
    std::vector<VertexAttributes>& vertexData
) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());

    if (!warn.empty()) {
        std::cout << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << err << std::endl;
    }

    if (!ret) {
        return false;
    }

    // Fill in vertexData here
    vertexData.clear();
    for (const auto& shape : shapes) {
        size_t offset = vertexData.size();
        vertexData.resize(offset + shape.mesh.indices.size());
        for (size_t i = 0; i < shape.mesh.indices.size(); i++) {
            const tinyobj::index_t& idx = shape.mesh.indices[i];

            vertexData[offset + i].position = {
                attrib.vertices[3 * idx.vertex_index + 0],
                -attrib.vertices[3 * idx.vertex_index + 2],
                attrib.vertices[3 * idx.vertex_index + 1]
            };

            vertexData[offset + i].normal = {
                attrib.normals[3 * idx.normal_index + 0],
                -attrib.normals[3 * idx.normal_index + 2],
                attrib.normals[3 * idx.normal_index + 1]
            };

            vertexData[offset + i].color = {
                attrib.colors[3 * idx.vertex_index + 0],
                attrib.colors[3 * idx.vertex_index + 1],
                attrib.colors[3 * idx.vertex_index + 2]
            };

            vertexData[offset + i].uv = {
                attrib.texcoords[2 * idx.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * idx.texcoord_index + 1]
            };
        }
    }

    return true;
}

wgpu::Texture ResourceManager::loadTexture(
    const std::filesystem::path& path,
    wgpu::Device device,
    wgpu::TextureView* pTextureView
) {
    int width, height, channels;
    unsigned char* pixelData = stbi_load(path.string().c_str(), &width, &height, &channels, 4 /* force 4 channels */);
    if (nullptr == pixelData) return nullptr;

    wgpu::TextureDescriptor desc;
    desc.dimension = wgpu::TextureDimension::_2D;
    desc.format = wgpu::TextureFormat::RGBA8Unorm;
    desc.sampleCount = 1;
    desc.size = { (unsigned int)width, (unsigned int)height, 1 };
    desc.mipLevelCount = std::bit_width(std::max(desc.size.width, desc.size.height));
    desc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    desc.viewFormatCount = 0;
    desc.viewFormats = nullptr;
    wgpu::Texture texture = device.createTexture(desc);

    // Upload data to the GPU texture 
    writeMipMaps(device, texture, desc.size, desc.mipLevelCount, pixelData);

    stbi_image_free(pixelData);

    if (pTextureView) {
        wgpu::TextureViewDescriptor textureViewDesc;
        textureViewDesc.aspect = wgpu::TextureAspect::All;
        textureViewDesc.baseArrayLayer = 0;
        textureViewDesc.arrayLayerCount = 1;
        textureViewDesc.baseMipLevel = 0;
        textureViewDesc.mipLevelCount = desc.mipLevelCount;
        textureViewDesc.dimension = wgpu::TextureViewDimension::_2D;
        textureViewDesc.format = desc.format;
        *pTextureView = texture.createView(textureViewDesc);
    }

    return texture;
}

wgpu::ShaderModule ResourceManager::loadShaderModule(
    const std::filesystem::path& path,
    wgpu::Device device
) {
    std::string shaderSource;
    if (!readShaderFile(path, shaderSource)) {
        std::cerr << "Could not load shader file at: " << path << std::endl;
        return nullptr;
    }

    wgpu::ShaderModuleDescriptor desc;
    wgpu::ShaderSourceWGSL shaderSrc;
    shaderSrc.chain.next = nullptr;
    shaderSrc.chain.sType = wgpu::SType::ShaderSourceWGSL;
    shaderSrc.code = wgpu::StringView(shaderSource.c_str());
    desc.nextInChain = &shaderSrc.chain;
    return device.createShaderModule(desc);
}

void ResourceManager::writeMipMaps(
    wgpu::Device device,
    wgpu::Texture texture,
    wgpu::Extent3D textureSize,
    [[maybe_unused]] uint32_t mipLevelCount, // not used yet
    const unsigned char* pixelData)
{
    // Get device queue
    wgpu::Queue queue = device.getQueue();

    wgpu::TexelCopyTextureInfo destination;
    destination.texture = texture;
    destination.origin = { 0, 0, 0 };
    destination.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferLayout source;
    source.offset = 0;

    // Create image data
    wgpu::Extent3D mipLevelSize = textureSize;
    std::vector<unsigned char> previousLevelPixels;
    wgpu::Extent3D previousMipLevelSize;
    for (uint32_t level = 0; level < mipLevelCount; level++) {
        // Pixel data for the current level
        std::vector<unsigned char> pixels(4 * mipLevelSize.width * mipLevelSize.height);
        if (level == 0) {
            // We cannot really avoid this copy since we need this
            // in previousLevelPixels at the next iteration
            memcpy(pixels.data(), pixelData, pixels.size());
        }
        else {
            // Create mip level data
            for (uint32_t i = 0; i < mipLevelSize.width; ++i) {
                for (uint32_t j = 0; j < mipLevelSize.height; ++j) {
                    unsigned char* p = &pixels[4 * (j * mipLevelSize.width + i)];
                    // Get the corresponding 4 pixels from the previous level
                    unsigned char* p00 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 0))];
                    unsigned char* p01 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 1))];
                    unsigned char* p10 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 0))];
                    unsigned char* p11 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 1))];
                    // Average
                    p[0] = (p00[0] + p01[0] + p10[0] + p11[0]) / 4;
                    p[1] = (p00[1] + p01[1] + p10[1] + p11[1]) / 4;
                    p[2] = (p00[2] + p01[2] + p10[2] + p11[2]) / 4;
                    p[3] = (p00[3] + p01[3] + p10[3] + p11[3]) / 4;
                }
            }
        }

        // Upload data to the GPU texture
        destination.mipLevel = level;
        source.bytesPerRow = 4 * mipLevelSize.width;
        source.rowsPerImage = mipLevelSize.height;
        queue.writeTexture(destination, pixels.data(), pixels.size(), source, mipLevelSize);

        previousLevelPixels = std::move(pixels);
        previousMipLevelSize = mipLevelSize;
        mipLevelSize.width /= 2;
        mipLevelSize.height /= 2;
    }

    queue.release();
}

bool ResourceManager::readShaderFile(
    const std::filesystem::path& path,
    std::string& contents
) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    // Seek end of file to determine its size
    file.seekg(0, std::ios::end);
    contents.resize(file.tellg());
    file.seekg(0, std::ios::beg);

    // Read contents into string
    file.read(&contents[0], contents.size());
    file.close();

    return true;
}