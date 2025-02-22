cmake_minimum_required(VERSION 3.27)

project(
    WGPURaytracer
    VERSION 1.0
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

# Set WGPU info messages
set(WGPU_INFO OFF)

# Set wgpu backend ("WGPU" or "DAWN")
set(WEBGPU_BACKEND "WGPU") 
set(WGPU_NATIVE_VERSION "v24.0.0.1")

# Find the required packages
add_subdirectory(cmake)

# defines targets and sources
add_subdirectory(src)