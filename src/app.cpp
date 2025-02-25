
#include "config.hpp"
#include "app.hpp"

#include "webgpu_utils.hpp"

#include <glfw3webgpu.h>
#include <magic_enum/magic_enum.hpp>

#include <iostream>
#include <vector>
#include <cassert>

#if defined(WEBGPU_BACKEND_DAWN)
#include <numeric_limits>
constexpr auto NaNf = std::numeric_limits<float>::quiet_NaN();
#endif


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
    instance.release();
    adapter.release();

    // Get queue
    queue = wgpuDeviceGetQueue(device);

    // Initialize buffers
    InitializeBuffers();

    // Initialize textures
    InitializeTextures();
    InitializeDepthTexture();

    // Initialize pipeline
    InitializePipline();

    // Initialize bind group
    InitializeBindGroups();

    return true;
};

void Application::Terminate() {
    bindGroup.release();
    pipeline.release();
    pipelineLayout.release();
    bindGroupLayout.release();
    textureView.release();
    texture.release();
    sampler.release();
    depthTexture.release();
    uniformBuffer.release();
    vertexBuffer.release();
    surface.unconfigure();
    surface.release();
    queue.release();
    device.release();
    glfwDestroyWindow(window);
    glfwTerminate();
};

void Application::MainLoop() {
    UpdateDragInertia();
    glfwPollEvents();

    // Update uniform buffer
    UpdateModelMatrix(glfwGetTime());
    queue.writeBuffer(uniformBuffer, 0, &uniforms, sizeof(MyUniforms));

    // Get texture view
    auto targetView = GetNextSurfaceTextureView();
    if (!targetView) return;

    // Get depth texture view
    auto depthTextureView = GetNextDepthTextureView();
    if (!depthTextureView) return;

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
    renderPassColorAttachment.clearValue = wgpu::Color{0.05, 0.05, 0.05, 1.0};
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

    wgpu::RenderPassDepthStencilAttachment depthStencilAttachment = {};
    depthStencilAttachment.view = depthTextureView;
    depthStencilAttachment.depthClearValue = 1.0f; // Initial value meaning "far"
    depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
    depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
    depthStencilAttachment.depthReadOnly = false; // Can turn off depth buffer globally here

    // Following unused but mandatory
    depthStencilAttachment.stencilClearValue = 0;
#if defined(WEBGPU_BACKEND_DAWN)
    depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
    depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
    depthStencilAttachment.clearDepth = NaNf;
#else
    depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Clear;
    depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Store;
#endif
    depthStencilAttachment.stencilReadOnly = false;

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
    renderPassDesc.timestampWrites = nullptr;

    // Create the render pass and end it immediately
    wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
    renderPass.setPipeline(pipeline);
    renderPass.setVertexBuffer(0, vertexBuffer, 0, vertexCount*sizeof(VertexAttributes));
    renderPass.setBindGroup(0, bindGroup, 0, nullptr);

    renderPass.draw(vertexCount, 1, 0, 0);

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
    depthTextureView.release();
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    width  = config::initial_width;
    height = config::initial_height;
    window = glfwCreateWindow(width, height, "WebGPU", nullptr, nullptr);

    // Set the window user pointer
    glfwSetWindowUserPointer(window, this);

    // Set the window callbacks
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int, int) {
        auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (that != nullptr) that->ResizeWindow();
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
        auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (that != nullptr) that->MouseMove(xpos, ypos);
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {
        auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (that != nullptr) that->MouseButton(button, action, mods);
    });
    glfwSetScrollCallback(window, [](GLFWwindow* window, double xoffset, double yoffset) {
        auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        if (that != nullptr) that->MouseScroll(xoffset, yoffset);
    });
};

void Application::ResizeWindow() {
    // Get the updated window width and height 
    glfwGetFramebufferSize(window, &width, &height);

    // Re-configure the surface
    wgpu::SurfaceConfiguration config;
    config.width = static_cast<uint32_t>(width);
    config.height = static_cast<uint32_t>(height);
    config.usage = wgpu::TextureUsage::RenderAttachment;
    config.format = surfaceFormat;

    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.device = device;
    config.presentMode = wgpu::PresentMode::Fifo;
    config.alphaMode = wgpu::CompositeAlphaMode::Auto;
    surface.configure(config);

    // Recreate the depth texture
    depthTexture.release();
    InitializeDepthTexture();
};

void Application::MouseMove(double xpos, double ypos) {
    if (dragState.active) {
        // Base move
        glm::vec2 currentMouse = glm::vec2(-(float)xpos, (float)ypos);
        glm::vec2 delta = (currentMouse - dragState.startMouse) * dragState.sensitivity;
        cameraState.angles = dragState.startCameraState.angles + delta;
        cameraState.angles.y = glm::clamp(cameraState.angles.y, -PI / 2.0f + 1e-5f, PI / 2.0f - 1e-5f);

        // Inertia
        dragState.velocity = delta - dragState.previousDelta;
        dragState.previousDelta = delta;

        UpdateViewMatrix();
    }
};

void Application::MouseButton(int button, int action, [[maybe_unused]] int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        switch(action) {
            case GLFW_PRESS:
                dragState.active = true;
                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);
                dragState.startMouse = glm::vec2(-(float)xpos, (float)ypos);
                dragState.startCameraState = cameraState;
                break;
            case GLFW_RELEASE:
                dragState.active = false;
                break;
        }
    }
};

void Application::MouseScroll([[maybe_unused]] double xoffset, double yoffset) {
    cameraState.zoom += dragState.scrollSensitivity * static_cast<float>(yoffset);
    cameraState.zoom = glm::clamp(cameraState.zoom, -2.0f, 2.0f);
    UpdateViewMatrix();
};

void Application::UpdateDragInertia() {
    constexpr float eps = 1e-4f;
    // Apply inertia only when the user released the click
    if (!dragState.active) {
        // Avoid updating the matrix when the velocity is no longer noticeable
        if (std::abs(dragState.velocity.x) < eps && std::abs(dragState.velocity.y) < eps) {
            return;
        }
        cameraState.angles += dragState.velocity;
        cameraState.angles.y = glm::clamp(cameraState.angles.y, -PI / 2.0f + 1e-5f, PI / 2.0f - 1e-5f);

        // Damp velocity so it decreases exponentially 
        dragState.velocity *= dragState.inertia;
        UpdateViewMatrix();
    }
};

wgpu::Instance Application::CreateInstance() {
    wgpu::InstanceDescriptor desc = {};
    return wgpu::createInstance(desc);
};

wgpu::Adapter Application::RequestAdapter(wgpu::Instance instance) {
#ifdef PRINT_EXTRA_INFO
    std::cout << "Requesting WebGPU adapter..." << std::endl;
#endif

    wgpu::RequestAdapterOptions adapterOpts = wgpu::Default;
    wgpu::Adapter adapter = instance.requestAdapter(adapterOpts);

#ifdef PRINT_EXTRA_INFO
    std::cout << "Got adapter: " << adapter << std::endl;

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

#ifdef PRINT_EXTRA_INFO
    std::cout << "Surface format: " << magic_enum::enum_name<WGPUTextureFormat>(surfaceFormat) << std::endl;
    std::cout << std::endl;
#endif

    // Configure surface
    wgpu::SurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    config.width = static_cast<uint32_t>(width);
    config.height = static_cast<uint32_t>(height);
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
#ifdef PRINT_EXTRA_INFO
    std::cout << "Requesting device..." << std::endl;
#endif

    // Get required limits
    auto requiredLimits = GetRequiredLimits(adapter);

    // Request device
    wgpu::DeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "My Device"_wgpu;
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredLimits = &requiredLimits;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue"_wgpu;
    deviceDesc.deviceLostCallbackInfo.nextInChain = nullptr;
    deviceDesc.deviceLostCallbackInfo.callback = onDeviceLost;
    deviceDesc.uncapturedErrorCallbackInfo.nextInChain = nullptr;
    deviceDesc.uncapturedErrorCallbackInfo.callback = onDeviceError;
    device = adapter.requestDevice(deviceDesc);

#ifdef PRINT_EXTRA_INFO
    std::cout << "Got device: " << device << std::endl;

    // Inspect the device
    inspectDevice(device);
#endif
}

void Application::InitializeBuffers() {
    // Load geometry data
    std::vector<VertexAttributes> vertexData;
    if (!ResourceManager::loadGeometryFromObj(config::shapeModelFile, vertexData)) {
        std::cerr << "Could not load geometry file at: " << config::shapeModelFile << std::endl;
        exit(1);
    }

    // Create vertex buffer
    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.size = vertexData.size() * sizeof(VertexAttributes);
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
    bufferDesc.mappedAtCreation = false;
    vertexBuffer = device.createBuffer(bufferDesc);

    vertexCount = static_cast<uint32_t>(vertexData.size());
    queue.writeBuffer(vertexBuffer, 0, vertexData.data(), bufferDesc.size);

    // Create uniform buffer
    bufferDesc.size = sizeof(MyUniforms); 
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufferDesc.mappedAtCreation = false;
    uniformBuffer = device.createBuffer(bufferDesc);

    UpdateModelMatrix(0.0f);
    UpdateViewMatrix();
    UpdateProjectionMatrix();
    queue.writeBuffer(uniformBuffer, 0, &uniforms, sizeof(MyUniforms));
}

void Application::InitializeTextures() {
    // Load texture  
    if (!(texture = ResourceManager::loadTexture(config::textureFile, device, &textureView))) {
        std::cerr << "Could not load texture at: " << config::textureFile << std::endl;
        exit(1);
    }

    // Create sampler
    wgpu::SamplerDescriptor samplerDesc;
    samplerDesc.addressModeU = wgpu::AddressMode::Repeat;
    samplerDesc.addressModeV = wgpu::AddressMode::Repeat;
    samplerDesc.addressModeW = wgpu::AddressMode::ClampToEdge;
    samplerDesc.magFilter = wgpu::FilterMode::Nearest;
    samplerDesc.minFilter = wgpu::FilterMode::Linear;
    samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 8.0f;
    samplerDesc.compare = wgpu::CompareFunction::Undefined;
    samplerDesc.maxAnisotropy = 1;
    sampler = device.createSampler(samplerDesc);
}

void Application::InitializeDepthTexture() {
    // Create depth texture
    wgpu::TextureDescriptor textureDesc;
    depthTextureFormat = wgpu::TextureFormat::Depth24Plus;
    textureDesc.format = depthTextureFormat;
    textureDesc.mipLevelCount = 1;
    textureDesc.sampleCount = 1;
    textureDesc.dimension = wgpu::TextureDimension::_2D;
    textureDesc.size = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
    textureDesc.usage = wgpu::TextureUsage::RenderAttachment;
    textureDesc.viewFormatCount = 1;
    textureDesc.viewFormats = (WGPUTextureFormat*)&depthTextureFormat;
    depthTexture = device.createTexture(textureDesc);
}

void Application::InitializePipline() {
    // Load the shader module
    auto shaderModule = ResourceManager::loadShaderModule(config::shaderSrcFile, device);

    // Create a bind group layouts
    std::vector<wgpu::BindGroupLayoutEntry> bindingLayouts(3);
    // === Uniform buffer binding
    wgpu::BindGroupLayoutEntry& uniformBindingLayout = bindingLayouts[0];
    uniformBindingLayout.binding = 0; // the @binding index used in the shader
    uniformBindingLayout.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    uniformBindingLayout.buffer.type = wgpu::BufferBindingType::Uniform;
    uniformBindingLayout.buffer.minBindingSize = sizeof(MyUniforms);

    // === Texture binding
    wgpu::BindGroupLayoutEntry& textureBindingLayout = bindingLayouts[1];
    textureBindingLayout.binding = 1; // the @binding index used in the shader
    textureBindingLayout.visibility = wgpu::ShaderStage::Fragment;
    textureBindingLayout.texture.sampleType = wgpu::TextureSampleType::Float;
    textureBindingLayout.texture.viewDimension = wgpu::TextureViewDimension::_2D;

    // === Sampler binding
    wgpu::BindGroupLayoutEntry& samplerBindingLayout = bindingLayouts[2];
    samplerBindingLayout.binding = 2; // the @binding index used in the shader
    samplerBindingLayout.visibility = wgpu::ShaderStage::Fragment;
    samplerBindingLayout.sampler.type = wgpu::SamplerBindingType::Filtering;

    wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc{};
    bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayouts.size();
    bindGroupLayoutDesc.entries = bindingLayouts.data();
    bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutDesc);

    // Create a pipeline layout
    wgpu::PipelineLayoutDescriptor pipelineLayoutDesc;
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
    pipelineLayout = device.createPipelineLayout(pipelineLayoutDesc);

    // Create vertex buffer layout
    wgpu::VertexBufferLayout vertexBufferLayout;

    std::vector<wgpu::VertexAttribute> vertexAttribs(4);
    // === Position
    vertexAttribs[0].shaderLocation = 0; // Corresponds to @location(...) in the shader
    vertexAttribs[0].format = wgpu::VertexFormat::Float32x3;
    vertexAttribs[0].offset = offsetof(VertexAttributes, position);

    // === Normal
    vertexAttribs[1].shaderLocation = 1;
    vertexAttribs[1].format = wgpu::VertexFormat::Float32x3;
    vertexAttribs[1].offset = offsetof(VertexAttributes, normal);

    // === Color
    vertexAttribs[2].shaderLocation = 2;
    vertexAttribs[2].format = wgpu::VertexFormat::Float32x3;
    vertexAttribs[2].offset = offsetof(VertexAttributes, color);

    // === UV
    vertexAttribs[3].shaderLocation = 3;
    vertexAttribs[3].format = wgpu::VertexFormat::Float32x2;
    vertexAttribs[3].offset = offsetof(VertexAttributes, uv);

    vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
    vertexBufferLayout.attributes = vertexAttribs.data();
    vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
    vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

    // Describe the pipeline
    wgpu::RenderPipelineDescriptor pipelineDesc;
    pipelineDesc.label = "render pipeline"_wgpu;
    pipelineDesc.layout = pipelineLayout;

    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;
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

    wgpu::DepthStencilState depthStencilState = wgpu::Default;
    depthStencilState.depthCompare = wgpu::CompareFunction::Less;
    depthStencilState.depthWriteEnabled = wgpu::OptionalBool::True;
    depthStencilState.format = depthTextureFormat;
    depthStencilState.stencilReadMask = 0;
    depthStencilState.stencilWriteMask = 0;
    pipelineDesc.depthStencil = &depthStencilState;

    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    pipeline = device.createRenderPipeline(pipelineDesc);

    shaderModule.release();
}

void Application::InitializeBindGroups() {
    std::vector<wgpu::BindGroupEntry> bindings(3);

    bindings[0].binding = 0; // the @binding index used in the shader
    bindings[0].buffer = uniformBuffer;
    bindings[0].offset = 0;
    bindings[0].size = sizeof(MyUniforms);

    bindings[1].binding = 1; // the @binding index used in the shader
    bindings[1].textureView = textureView;

    bindings[2].binding = 2; // the @binding index used in the shader
    bindings[2].sampler = sampler;

    wgpu::BindGroupDescriptor bindGroupDesc;
    bindGroupDesc.label = "My bind group"_wgpu;
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = (uint32_t)bindings.size();
    bindGroupDesc.entries = bindings.data();
    bindGroup = device.createBindGroup(bindGroupDesc);
}

wgpu::Limits Application::GetRequiredLimits(wgpu::Adapter adapter) {
    // Get supported limits
    wgpu::Limits supportedLimits;
    adapter.getLimits(&supportedLimits);

    // Set required limits
    wgpu::Limits requiredLimits = wgpu::Default;

    requiredLimits.maxVertexAttributes = 4;
    requiredLimits.maxVertexBuffers = 1;

    requiredLimits.maxBufferSize = 1000000 * sizeof(VertexAttributes);
    requiredLimits.maxVertexBufferArrayStride = sizeof(VertexAttributes);

    requiredLimits.maxInterStageShaderVariables = 8;

    requiredLimits.maxBindGroups = 1;

    requiredLimits.maxUniformBuffersPerShaderStage = 1;
    requiredLimits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);

    requiredLimits.maxTextureDimension1D = supportedLimits.maxTextureDimension1D;
    requiredLimits.maxTextureDimension2D = supportedLimits.maxTextureDimension2D;
    requiredLimits.maxTextureArrayLayers = 1;
    requiredLimits.maxSampledTexturesPerShaderStage = 1;
    requiredLimits.maxSamplersPerShaderStage = 1;

    requiredLimits.minUniformBufferOffsetAlignment = supportedLimits.minUniformBufferOffsetAlignment;
    requiredLimits.minStorageBufferOffsetAlignment = supportedLimits.minStorageBufferOffsetAlignment;

    return requiredLimits;
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

wgpu::TextureView Application::GetNextDepthTextureView() {
    // Get depth texture view
    wgpu::TextureViewDescriptor viewDesc;
    viewDesc.aspect = wgpu::TextureAspect::DepthOnly;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.dimension = wgpu::TextureViewDimension::_2D;
    viewDesc.format = depthTextureFormat;
    wgpu::TextureView depthTextureView = depthTexture.createView(viewDesc);

    return depthTextureView;
}

void Application::UpdateViewMatrix() {
    float cx = glm::cos(cameraState.angles.x);
    float cy = glm::cos(cameraState.angles.y);
    float sx = glm::sin(cameraState.angles.x);
    float sy = glm::sin(cameraState.angles.y);

    glm::vec3 position = glm::vec3(cx * cy, sx * cy, sy) * std::exp(-cameraState.zoom);
    uniforms.viewMatrix = glm::lookAt(position, glm::vec3(0.0f), glm::vec3(0, 0, 1));
}

void Application::UpdateModelMatrix(float time) {
    float angle1 = time;
    auto M = glm::mat4x4(1.0f);
    M = glm::rotate(M, angle1, glm::vec3(0.0f, 0.0f, 1.0f));
    M = glm::translate(M, glm::vec3(0.0f, 0.0f, 0.0f));
    M = glm::scale(M, glm::vec3(0.3f));
    uniforms.modelMatrix = M;

    uniforms.modelMatrix = glm::mat4x4(1.0);
}

void Application::UpdateProjectionMatrix() {
    float ratio = static_cast<float>(width) / static_cast<float>(height);
    float focalLength = 2.0f;
    float near = 0.01f;
    float far = 100.0f;
    float fov = 2.0f * glm::atan(1.0f / focalLength);
    uniforms.projectionMatrix = glm::perspective(fov, ratio, near, far);

    uniforms.projectionMatrix = glm::perspective(45 * PI / 180, 640.0f / 480.0f, 0.01f, 100.0f);
}