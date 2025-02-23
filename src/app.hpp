#ifndef _APP_H
#define _APP_H

#include <webgpu/webgpu.hpp>
#include <GLFW/glfw3.h>

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
    // App configuration
    void CreateWindow(); 
    wgpu::Instance CreateInstance();
    wgpu::Adapter RequestAdapter(wgpu::Instance instance);
    void RequestDevice(wgpu::Adapter adapter);
    void ConfigureSurface(wgpu::Instance instance, wgpu::Adapter adapter);
    void InitializePipline();

    wgpu::ShaderModule LoadShaderModule();
    wgpu::TextureView GetNextSurfaceTextureView();

private:
    GLFWwindow *window;
    wgpu::Device device;
    wgpu::Surface surface;
    wgpu::Queue queue;
    wgpu::RenderPipeline pipeline;
    wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::Undefined;
};

#endif // _APP_H