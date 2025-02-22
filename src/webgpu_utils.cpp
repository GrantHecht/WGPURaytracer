
#include "webgpu_utils.hpp"

#include <iostream>
#include <vector>
#include <cassert>

// Define string literal for WGPUStringView
WGPUStringView operator""_wgpu(const char* str, size_t len) {
    return {str, len};
};

// Device error callback
void onDeviceError(
    WGPUDevice const* /* pDevice */, WGPUErrorType type, 
    WGPUStringView message, 
    WGPU_NULLABLE void* /* pUserData */, WGPU_NULLABLE void* /* pUserData */
) {
    std::cout << "Uncaptured device error: type " << type;
    if (message.data) std::cout << " (" << message.data << ")";
    std::cout << std::endl;
};

void onDeviceLost(
    WGPUDevice const* /* pDevice */, WGPUDeviceLostReason reason, 
    WGPUStringView message, 
    WGPU_NULLABLE void* /* pUserData */, WGPU_NULLABLE void* /* pUserData */
) {
    std::cout << "Device lost: reason " << reason;
    if (message.data) 
        std::cout << " (" << message.data << ")";
    std::cout << std::endl;

};

void onQueueWorkDone(
    WGPUQueueWorkDoneStatus status, 
    WGPU_NULLABLE void* /* pUserData1 */,
    WGPU_NULLABLE void* /* pUserData2 */
) {
    std::cout << "Queued work finished with status: " << status << std::endl;
};

// Utility function to request a WebGPU adapter
WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const * options) {
    // A simple structure holding the local information shared with the
    // onAdapterRequestEnded callback.
    struct UserData {
        WGPUAdapter adapter = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    // Callback called by wgpuInstanceRequestAdapter when the request returns
    // This is a C++ lambda function, but could be any function defined in the
    // global scope. It must be non-capturing (the brackets [] are empty) so
    // that it behaves like a regular C function pointer, which is what
    // wgpuInstanceRequestAdapter expects (WebGPU being a C API). The workaround
    // is to convey what we want to capture through the pUserData pointer,
    // provided as the last argument of wgpuInstanceRequestAdapter and received
    // by the callback as its last argument.
    auto onAdapterRequestEnded = [](
        WGPURequestAdapterStatus status, WGPUAdapter adapter, 
        WGPUStringView message, void * pUserData1, void * /* pUserData2 */
    ) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData1);
        if (status == WGPURequestAdapterStatus_Success) {
            userData.adapter = adapter;
        } else {
            std::cout << "Could not get WebGPU adapter: " << message.data << std::endl;
        }
        userData.requestEnded = true;
    };

    // Call to the WebGPU request adapter procedure
    WGPURequestAdapterCallbackInfo info = {
        .nextInChain = nullptr,
        .callback = onAdapterRequestEnded,
        .userdata1 = (void*)&userData
    };
    wgpuInstanceRequestAdapter(instance, options, info);

    assert(userData.requestEnded);

    return userData.adapter;
};

// Utility function to request a WebGPU device
WGPUDevice requestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor) {
    struct UserData {
        WGPUDevice device = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    auto onDeviceRequestEnded = [](
        WGPURequestDeviceStatus status, WGPUDevice device, 
        WGPUStringView message, void * pUserData1, void * /* pUserData2 */
    ) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData1);
        if (status == WGPURequestDeviceStatus_Success) {
            userData.device = device;
        } else {
            std::cout << "Could not get WebGPU device: " << message.data << std::endl;
        }
        userData.requestEnded = true;
    };

    WGPURequestDeviceCallbackInfo info = {
        .nextInChain = nullptr,
        .callback = onDeviceRequestEnded,
        .userdata1 = (void*)&userData
    };
    wgpuAdapterRequestDevice(adapter, descriptor, info);

    assert(userData.requestEnded);

    return userData.device;
};

// inspectAdapter function:
void inspectAdapter(WGPUAdapter adapter) {
    // Get adapter limits
    WGPULimits supportedLimits = {
        .nextInChain = nullptr,
    };

#ifdef WEBGPU_BACKEND_DAWN
    bool success = wgpuAdapterGetLimits(adapter, &supportedLimits) == WGPUStatus_Success;
#else
    bool success = wgpuAdapterGetLimits(adapter, &supportedLimits);
#endif
    if (success) {
        std::cout << "Adapter limits:" << std::endl;
        std::cout << " - maxTextureDimension1D: " << supportedLimits.maxTextureDimension1D << std::endl;
        std::cout << " - maxTextureDimension2D: " << supportedLimits.maxTextureDimension2D << std::endl;
        std::cout << " - maxTextureDimension3D: " << supportedLimits.maxTextureDimension3D << std::endl;
        std::cout << " - maxTextureArrayLayers: " << supportedLimits.maxTextureArrayLayers << std::endl;
    }
    std::cout << std::endl;
    
    // Get adapter features
    WGPUSupportedFeatures features{};
    wgpuAdapterGetFeatures(adapter, &features);

    std::cout << "Adapter features:" << std::endl;
    std::cout << std::hex; // Write integers as hexadecimal to ease comparison with webgpu.h literals
    for (size_t i = 0; i < features.featureCount; i++) 
        std::cout << " - 0x" << features.features[i] << std::endl;

    std::cout << std::dec; // Restore decimal numbers
    std::cout << std::endl;
};

// inspect device function:
void inspectDevice(WGPUDevice device) {
    WGPUSupportedFeatures features{};
    wgpuDeviceGetFeatures(device, &features);

    std::cout << "Device features:" << std::endl;
    std::cout << std::hex;
    for (size_t i = 0; i < features.featureCount; i++) {
        std::cout << " - 0x" << features.features[i] << std::endl;
    }
    std::cout << std::dec;

    WGPULimits limits = {.nextInChain = nullptr};

#ifdef WEBGPU_BACKEND_DAWN
    bool success = wgpuDeviceGetLimits(device, &limits) == WGPUStatus_Success;
#else
    bool success = wgpuDeviceGetLimits(device, &limits);
#endif

    if (success) {
        std::cout << "Device limits:" << std::endl;
        std::cout << " - maxTextureDimension1D: " << limits.maxTextureDimension1D << std::endl;
        std::cout << " - maxTextureDimension2D: " << limits.maxTextureDimension2D << std::endl;
        std::cout << " - maxTextureDimension3D: " << limits.maxTextureDimension3D << std::endl;
        std::cout << " - maxTextureArrayLayers: " << limits.maxTextureArrayLayers << std::endl;
    }
    std::cout << std::endl;
};