#ifndef _APP_H
#define _APP_H

#include <webgpu/webgpu.hpp>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "resource_manager.hpp"

class Application {
public:
    // Initialize everything and return true if it went all right
    bool Initialize();

    // Uninitialize everything that was initialized
    void Terminate();

    // Draw a frame and handle events
    void MainLoop();

    // Return true as long as the main loop should keep on running
    bool IsRunning();

private:
    struct MyUniforms {
        glm::mat4x4 projectionMatrix;
        glm::mat4x4 viewMatrix;
        glm::mat4x4 modelMatrix;
        glm::vec4 color;
        float time;
        float _pad[3];
    };
    static_assert(sizeof(MyUniforms) % 16 == 0);

    struct CameraState {
        glm::vec2 angles = { 0.8f, 0.5f };
        float zoom = -1.2f;
    };

    struct DragState {
        // Whether a drag action is ongoing 
        bool active = false;
        // The position of the mouse at the beginning of the drag action
        glm::vec2 startMouse;
        // The camera state at the beginning of the drag action
        CameraState startCameraState;

        // Inertia
        glm::vec2 velocity = {0.0, 0.0};
        glm::vec2 previousDelta;
        float inertia = 0.9f;

        // Constant settings
        float sensitivity = 0.01f;
        float scrollSensitivity = 0.1f;
    };

private:
    // Window creation
    void CreateWindow(); 

    // Event handling
    void ResizeWindow();
    void MouseMove(double xpos, double ypos);
    void MouseButton(int button, int action, int mods);
    void MouseScroll(double xoffset, double yoffset);
    void UpdateDragInertia();

    // WebGPU initialization
    wgpu::Instance CreateInstance();

    wgpu::Adapter RequestAdapter(wgpu::Instance instance);
    void RequestDevice(wgpu::Adapter adapter);
    void ConfigureSurface(wgpu::Instance instance, wgpu::Adapter adapter);

    void InitializeBuffers();
    void InitializeTextures();
    void InitializeDepthTexture();
    void InitializePipline();
    void InitializeBindGroups();

    wgpu::Limits GetRequiredLimits(wgpu::Adapter adapter);

    // WebGPU rendering
    wgpu::TextureView GetNextSurfaceTextureView();

    wgpu::TextureView GetNextDepthTextureView();

    // Scene transformation
    void UpdateModelMatrix(float time);
    void UpdateViewMatrix();
    void UpdateProjectionMatrix();
    MyUniforms GetUniforms(float time);

private:
    // Window
    GLFWwindow *window;
    int width, height;

    // User interaction
    CameraState cameraState;
    DragState dragState;
    MyUniforms uniforms;

    // WebGPU
    wgpu::Device device;
    wgpu::Surface surface;
    wgpu::Queue queue;

    wgpu::Buffer uniformBuffer;
    wgpu::Buffer vertexBuffer;
    uint32_t vertexCount;

    wgpu::Texture texture;
    wgpu::Sampler sampler;

    wgpu::Texture depthTexture;

    wgpu::TextureView textureView;

    wgpu::PipelineLayout pipelineLayout;
    wgpu::BindGroupLayout bindGroupLayout;

    wgpu::BindGroup bindGroup;

    wgpu::RenderPipeline pipeline;

    wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
    wgpu::TextureFormat textureFormat = wgpu::TextureFormat::Undefined;
    wgpu::TextureFormat depthTextureFormat = wgpu::TextureFormat::Undefined;
};


#endif // _APP_H