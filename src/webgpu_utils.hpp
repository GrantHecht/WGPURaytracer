#ifndef _WEBGPU_UTILS_H
#define _WEBGPU_UTILS_H

#include <webgpu/webgpu.hpp>

// Define string literal for WGPUStringView
wgpu::StringView operator""_wgpu(const char* str, size_t len);

wgpu::StringView chars_to_wgpu(const char* str);

// Devise error callback
void onDeviceError(
    WGPUDevice const* device, WGPUErrorType type, 
    WGPUStringView message, 
    void* userdata1, void* userdata2
);

void onDeviceLost(
    WGPUDevice const* device, WGPUDeviceLostReason reason, 
    WGPUStringView message, 
    void* userdata1, void* userdata2
);

// Utility function to inspect a WebGPU adapter
void inspectAdapter(wgpu::Adapter adapter);

// Utility function to inspect a WebGPU device
void inspectDevice(wgpu::Device device);

#endif // _WEBGPU_UTILS_H