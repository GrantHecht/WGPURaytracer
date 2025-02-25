
#include "webgpu_utils.hpp"

#include <string_view>
#include <cassert>

#include <magic_enum/magic_enum.hpp>

// Some string view utilities
wgpu::StringView operator""_wgpu(const char* str, size_t len) {
    std::string_view sv{str, len};
    return {sv};
};

wgpu::StringView chars_to_wgpu(const char* str) {
    return {std::string_view{str}};
};

// Device error callback
void onDeviceError(
    [[maybe_unused]] WGPUDevice const* device, WGPUErrorType type, 
    WGPUStringView message, 
    [[maybe_unused]] void* userdata1, [[maybe_unused]] void* userdata2
) {
    std::cout << "Uncaptured device error: type " << type;
    if (message.data) std::cout << " (" << message.data << ")";
    std::cout << std::endl;
};

void onDeviceLost(
    [[maybe_unused]] WGPUDevice const* device, WGPUDeviceLostReason reason, 
    WGPUStringView message, 
    [[maybe_unused]] void* userdata1, [[maybe_unused]] void* userdata2
) {
    std::cout << "Device lost: reason " << reason;
    if (message.data) std::cout << " (" << message.data << ")";
    std::cout << std::endl;
};

// inspectAdapter function:
void inspectAdapter(wgpu::Adapter adapter) {
    // Get adapter limits
    wgpu::Limits supportedLimits = wgpu::Default;

#ifdef WEBGPU_BACKEND_DAWN
    bool success = adapter.getLimits(&supportedLimits) == wgpu::Status::Success;
#else
    bool success = adapter.getLimits(&supportedLimits);
#endif
    if (success) {
        std::cout << "Adapter limits:" << std::endl;
        std::cout << " - maxTextureDimension1D: " << supportedLimits.maxTextureDimension1D << std::endl;
        std::cout << " - maxTextureDimension2D: " << supportedLimits.maxTextureDimension2D << std::endl;
        std::cout << " - maxTextureDimension3D: " << supportedLimits.maxTextureDimension3D << std::endl;
        std::cout << " - maxTextureArrayLayers: " << supportedLimits.maxTextureArrayLayers << std::endl;
        std::cout << " - maxVertexAttributes:   " << supportedLimits.maxVertexAttributes << std::endl;
        std::cout << " - maxBufferSize:         " << supportedLimits.maxBufferSize << std::endl;
    }
    std::cout << std::endl;
    
    // Get adapter features
    wgpu::SupportedFeatures features;
    adapter.getFeatures(&features);

    std::cout << "Adapter features:" << std::endl;
    std::string feature_name;
    for (size_t i = 0; i < features.featureCount; i++) {
        feature_name = magic_enum::enum_name(features.features[i]);
        if (!feature_name.empty())
            std::cout << feature_name << std::endl;
    }
    std::cout << std::endl;
};

// inspect device function:
void inspectDevice(wgpu::Device device) {
    // Get device limits
    wgpu::Limits limits = wgpu::Default;

#ifdef WEBGPU_BACKEND_DAWN
    bool success = device.getLimits(&limits) == wgpu::Status::Success;
#else
    bool success = device.getLimits(&limits);
#endif

    if (success) {
        std::cout << "Device limits:" << std::endl;
        std::cout << " - maxTextureDimension1D: " << limits.maxTextureDimension1D << std::endl;
        std::cout << " - maxTextureDimension2D: " << limits.maxTextureDimension2D << std::endl;
        std::cout << " - maxTextureDimension3D: " << limits.maxTextureDimension3D << std::endl;
        std::cout << " - maxTextureArrayLayers: " << limits.maxTextureArrayLayers << std::endl;
        std::cout << " - maxVertexAttributes:   " << limits.maxVertexAttributes << std::endl;
        std::cout << " - maxBufferSize:         " << limits.maxBufferSize << std::endl;
    }
    std::cout << std::endl;

    // Get device features
    wgpu::SupportedFeatures features{};
    device.getFeatures(&features);

    std::cout << "Device features:" << std::endl;
    std::string feature_name;
    for (size_t i = 0; i < features.featureCount; i++) {
        feature_name = magic_enum::enum_name(features.features[i]);
        if (!feature_name.empty())
            std::cout << feature_name << std::endl;
    }
    std::cout << std::endl;
};