
#include "app.hpp"
#include "webgpu_utils.hpp"

#include <glfw3webgpu.h>

#include <iostream>
#include <vector>
#include <cassert>

bool Application::Initialize() {
    // ===== Initialize Window
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        return false;
    }

    // Create the window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // <-- extra info for glfwCreateWindow
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(800, 600, "WebGPU", nullptr, nullptr);
    if (!window) {
        std::cerr << "Could not create GLFW window!" << std::endl;
        glfwTerminate();
        return false;
    }

    // ===== Initialize WebGPU
    // Create WebGPU instance
    WGPUInstanceDescriptor desc = {};
    desc.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
    // Make sure the uncaptured error callback is called as soon as an error
    // occurs rather than at the next call to "wgpuDeviceTick".
    WGPUDawnTogglesDescriptor toggles;
    toggles.chain.next = nullptr;
    toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
    toggles.disabledToggleCount = 0;
    toggles.enabledToggleCount = 1;
    const char* toggleName = "enable_immediate_error_handling";
    toggles.enabledToggles = &toggleName;

    desc.nextInChain = &toggles.chain;
#endif // WEBGPU_BACKEND_DAWN

    WGPUInstance instance = wgpuCreateInstance(&desc);

    // Check WebGPU instance
    if (!instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        return false;
    }

    // Request WebGPU adapter
#ifdef PRINT_ADAPTER_INFO
    std::cout << "Requesting WebGPU adapter..." << std::endl;
#endif
    WGPURequestAdapterOptions adapterOpts = {
        .nextInChain = nullptr,
    };
    WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);
#ifdef PRINT_ADAPTER_INFO
    std::cout << "Got adapter: " << adapter << std::endl;
#endif

#ifdef PRINT_ADAPTER_INFO
    // Inspect the adapter
    inspectAdapter(adapter);
#endif

    // Create surface
    surface = glfwGetWGPUSurface(instance, window);

    // Destroy WebGPU instance
    wgpuInstanceRelease(instance);

#ifdef PRINT_DEVICE_INFO
    std::cout << "Requesting device..." << std::endl;
#endif
    WGPUDeviceDescriptor deviceDesc = {
        .nextInChain = nullptr,
        .label = "My Device"_wgpu,
        .requiredFeatureCount = 0,
        .requiredLimits = nullptr,
        .defaultQueue = {
            .nextInChain = nullptr,
            .label = "The default queue"_wgpu,
        },
        .deviceLostCallbackInfo = {
            .nextInChain = nullptr,
            .callback = onDeviceLost,
        },
        .uncapturedErrorCallbackInfo = {
            .nextInChain = nullptr,
            .callback = onDeviceError,
        },
    };
    device = requestDeviceSync(adapter, &deviceDesc);
#ifdef PRINT_DEVICE_INFO
    std::cout << "Got device: " << device << std::endl;
#endif

    // Destroy WebGPU adapter
    wgpuAdapterRelease(adapter);

    // Inspect the device
#ifdef PRINT_DEVICE_INFO
    inspectDevice(device);
#endif

    // Get queue
    queue = wgpuDeviceGetQueue(device);

    // Set a callback for when the queue has finished processing all work
    wgpuQueueOnSubmittedWorkDone(
        queue,
        WGPUQueueWorkDoneCallbackInfo {
            .nextInChain = nullptr,
            .callback = onQueueWorkDone,
            .userdata1 = nullptr,
            .userdata2 = nullptr
        }
    );

    // Submit command queue
    // first create command encoder
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.nextInChain = nullptr;
    encoderDesc.label = "My command encoder"_wgpu;
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

    wgpuCommandEncoderInsertDebugMarker(encoder, "Do one thing"_wgpu);
    wgpuCommandEncoderInsertDebugMarker(encoder, "Do another thing"_wgpu);

    WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
    cmdBufferDescriptor.nextInChain = nullptr;
    cmdBufferDescriptor.label = "Command buffer"_wgpu;
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
    wgpuCommandEncoderRelease(encoder); // release encoder after it's finished

    // Finally submit the command queue
    std::cout << "Submitting command..." << std::endl;
    wgpuQueueSubmit(queue, 1, &command);
    wgpuCommandBufferRelease(command);
    std::cout << "Command submitted." << std::endl;

    for (int i = 0 ; i < 5 ; ++i) {
        std::cout << "Tick/Poll device..." << std::endl;
#if defined(WEBGPU_BACKEND_DAWN)
        wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
        wgpuDevicePoll(device, false, nullptr);
#endif
    }

    return true;
};

void Application::Terminate() {
    wgpuQueueRelease(queue);
    wgpuSurfaceRelease(surface);
    wgpuDeviceRelease(device);
    glfwDestroyWindow(window);
    glfwTerminate();
};

void Application::MainLoop() {
    glfwPollEvents();
};

bool Application::IsRunning() {
    return !glfwWindowShouldClose(window);
};