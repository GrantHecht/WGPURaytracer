
#include "app.hpp"
#include "webgpu_utils.hpp"
#include "config.hpp"

#include <glfw3webgpu.h>

#include <iostream>
#include <vector>
#include <cassert>

const char* shaderSource = R"(
    @vertex
    fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
        var p = vec2f(0.0, 0.0);
        if (in_vertex_index == 0u) {
            p = vec2f(-0.5, -0.5);
        } else if (in_vertex_index == 1u) {
            p = vec2f(0.5, -0.5);
        } else {
            p = vec2f(0.0, 0.5);
        }
        return vec4f(p, 0.0, 1.0);
    }
    
    @fragment
    fn fs_main() -> @location(0) vec4f {
        return vec4f(0.0, 0.4, 1.0, 1.0);
    }
    )";

bool Application::Initialize() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        return false;
    }

    // Create the window
    CreateWindow();
    if (!window) {
        std::cerr << "Could not create GLFW window!" << std::endl;
        glfwTerminate();
        return false;
    }

    // Create WebGPU instance
    auto instance = CreateInstance();
    if (!instance) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        return false;
    }

    // Request WebGPU adapter
    auto adapter = RequestAdapter(instance);

    // Request WebGPU device
    RequestDevice(adapter);

    // Configure surface
    ConfigureSurface(instance, adapter);

    // Destroy WebGPU instance and adapter
    wgpuInstanceRelease(instance);
    wgpuAdapterRelease(adapter);

    // Get queue
    queue = wgpuDeviceGetQueue(device);

    // Initialize pipeline
    InitializePipline();

    return true;
};

void Application::Terminate() {
    pipeline.release();
    surface.unconfigure();
    surface.release();
    queue.release();
    device.release();
    glfwDestroyWindow(window);
    glfwTerminate();
};

void Application::MainLoop() {
    glfwPollEvents();

    // Get texture view
    auto targetView = GetNextSurfaceTextureView();
    if (!targetView) return;

    // Create a command encoder for the draw call 
    wgpu::CommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = "My command encoder"_wgpu;
    wgpu::CommandEncoder encoder = device.createCommandEncoder(encoderDesc);

    // Create render pass that clears the screen with our color
    wgpu::RenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = targetView;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
    renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
    renderPassColorAttachment.clearValue = wgpu::Color{0.9, 0.1, 0.2, 1.0};
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWrites = nullptr;

    // Create the render pass and end it immediately
    wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
    renderPass.setPipeline(pipeline);
    renderPass.draw(3, 1, 0, 0);
    renderPass.end();
    renderPass.release();

    // Finally encode and submit the render pass
    wgpu::CommandBufferDescriptor cmdBufferDesc = {};
    cmdBufferDesc.label = "Command buffer"_wgpu;
    wgpu::CommandBuffer command = encoder.finish(cmdBufferDesc);
    encoder.release();

    queue.submit(command);
    command.release();

    // Release texture view
    targetView.release();
    surface.present();

#if defined(WEBGPU_BACKEND_DAWN)
    device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
    device.poll(false, nullptr);
#endif
};

bool Application::IsRunning() {
    return !glfwWindowShouldClose(window);
};

void Application::CreateWindow() {
    // Create the window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // <-- extra info for glfwCreateWindow
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(width, height, "WebGPU", nullptr, nullptr);
};

wgpu::Instance Application::CreateInstance() {
    wgpu::InstanceDescriptor desc = {};
    return wgpu::createInstance(desc);
};

wgpu::Adapter Application::RequestAdapter(wgpu::Instance instance) {
#ifdef PRINT_ADAPTER_INFO
    std::cout << "Requesting WebGPU adapter..." << std::endl;
#endif

    wgpu::RequestAdapterOptions adapterOpts = wgpu::Default;
    wgpu::Adapter adapter = instance.requestAdapter(adapterOpts);

#ifdef PRINT_ADAPTER_INFO
    std::cout << "Got adapter: " << adapter << std::endl;
#endif

#ifdef PRINT_ADAPTER_INFO
    // Inspect the adapter
    inspectAdapter(adapter);
#endif

    return adapter;
};

void Application::ConfigureSurface(wgpu::Instance instance, wgpu::Adapter adapter) {
    // Get surface
    surface = glfwGetWGPUSurface(instance, window);

    // Get surface capabilities
    wgpu::SurfaceCapabilities surfaceCapabilities;
    surface.getCapabilities(adapter, &surfaceCapabilities);

    // Get preferred format from capabilities
    surfaceFormat = surfaceCapabilities.formats[0];

    // Configure surface
    wgpu::SurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    config.width = width;
    config.height = height;
    config.format = surfaceFormat;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    config.device = device;
    config.presentMode = wgpu::PresentMode::Fifo;
    config.alphaMode = wgpu::CompositeAlphaMode::Auto;
    surface.configure(config);
};

void Application::RequestDevice(wgpu::Adapter adapter) {
#ifdef PRINT_DEVICE_INFO
    std::cout << "Requesting device..." << std::endl;
#endif

    wgpu::DeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "My Device"_wgpu;
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredLimits = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue"_wgpu;
    deviceDesc.deviceLostCallbackInfo.nextInChain = nullptr;
    deviceDesc.deviceLostCallbackInfo.callback = onDeviceLost;
    deviceDesc.uncapturedErrorCallbackInfo.nextInChain = nullptr;
    deviceDesc.uncapturedErrorCallbackInfo.callback = onDeviceError;
    device = adapter.requestDevice(deviceDesc);

#ifdef PRINT_DEVICE_INFO
    std::cout << "Got device: " << device << std::endl;
#endif

    // Inspect the device
#ifdef PRINT_DEVICE_INFO
    inspectDevice(device);
#endif
}

void Application::InitializePipline() {
    // Load the shader module
    auto shaderModule = LoadShaderModule();

    // Get pipeline layout
    wgpu::PipelineLayoutDescriptor pipelineLayoutDesc;
    auto pipelineLayout = device.createPipelineLayout(pipelineLayoutDesc);
    assert(pipelineLayout);

    // Describe the pipeline
    wgpu::RenderPipelineDescriptor pipelineDesc;
    pipelineDesc.label = "render pipeline"_wgpu;
    pipelineDesc.layout = pipelineLayout;

    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main"_wgpu;
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;

    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    pipelineDesc.primitive.cullMode = wgpu::CullMode::None;

    wgpu::BlendState blendState;
    blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = wgpu::BlendOperation::Add;
    blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blendState.alpha.dstFactor = wgpu::BlendFactor::One;
    blendState.alpha.operation = wgpu::BlendOperation::Add;

    wgpu::ColorTargetState colorTarget;
    colorTarget.format = surfaceFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragmentState;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main"_wgpu;
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    pipelineDesc.fragment = &fragmentState;

    pipelineDesc.depthStencil = nullptr;

    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    pipeline = device.createRenderPipeline(pipelineDesc);

    shaderModule.release();
}

wgpu::TextureView Application::GetNextSurfaceTextureView() {
    // Get surface texture
    wgpu::SurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);

    if (!(surfaceTexture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal || 
          surfaceTexture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal)) {
        return nullptr;
    }
    wgpu::Texture texture = surfaceTexture.texture;

    // Get texture view
    wgpu::TextureViewDescriptor viewDesc;
    viewDesc.label = "Surface texture view"_wgpu;
    viewDesc.format = texture.getFormat();
    viewDesc.dimension = wgpu::TextureViewDimension::_2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = wgpu::TextureAspect::All;
    wgpu::TextureView targetView = texture.createView(viewDesc);

#ifndef WEBGPU_BACKEND_WGPU
    // We no longer need the texture, only its view
    // (NB: with wgpu-native, surface textures must not be manually released)
    texture.release();
#endif

    return targetView;
}

wgpu::ShaderModule Application::LoadShaderModule() {
    // Load shader file
    auto shaderSource = readShaderFile("shaders/triangle.wgsl");

    // Describe shader
    wgpu::ShaderModuleDescriptor shaderDesc;
    wgpu::ShaderSourceWGSL shaderSrc;
    shaderSrc.chain.next = nullptr;
    shaderSrc.chain.sType = wgpu::SType::ShaderSourceWGSL;
    shaderSrc.code = wgpu::StringView(shaderSource);
    shaderDesc.nextInChain = &shaderSrc.chain;

    wgpu::ShaderModule shaderModule = device.createShaderModule(shaderDesc);
    assert(shaderModule);

    return shaderModule;
}