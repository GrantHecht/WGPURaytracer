#ifndef _WEBGPU_UTILS_H
#define _WEBGPU_UTILS_H

#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#  include <webgpu/wgpu.h>
#endif // WEBGPU_BACKEND_WGPU

// Define string literal for WGPUStringView
WGPUStringView operator""_wgpu(const char* str, size_t len);

// Devise error callback
void onDeviceError(
    WGPUDevice const* /* pDevice */, WGPUErrorType type, 
    WGPUStringView message, 
    WGPU_NULLABLE void* /* pUserData */, WGPU_NULLABLE void* /* pUserData */
);

void onDeviceLost(
    WGPUDevice const* /* pDevice */, WGPUDeviceLostReason reason, 
    WGPUStringView message, 
    WGPU_NULLABLE void* /* pUserData */, WGPU_NULLABLE void* /* pUserData */
);

void onQueueWorkDone(
    WGPUQueueWorkDoneStatus status, 
    WGPU_NULLABLE void* /* pUserData1 */,
    WGPU_NULLABLE void* /* pUserData2 */
);

// Utility function to request a WebGPU adapter
WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const * options);

// Utility function to request a WebGPU device
WGPUDevice requestDeviceSync(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor);

// Utility function to inspect a WebGPU adapter
void inspectAdapter(WGPUAdapter adapter);

// Utility function to inspect a WebGPU device
void inspectDevice(WGPUDevice device);

#endif // _WEBGPU_UTILS_H