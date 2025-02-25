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
option(DEV_MODE "Enable developer helper settings" ON)
option(WGPU_INFO "Print extra info from WGPU" ON)

# Setup build directory
if (DEV_MODE)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release)
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set wgpu backend ("WGPU" or "DAWN")
set(WEBGPU_BACKEND "WGPU") 
set(WGPU_NATIVE_VERSION "v24.0.0.1")

# Find the required packages
add_subdirectory(cmake)

# Add third_party
add_subdirectory(third_party)

# defines targets and sources
add_subdirectory(src)

# copy resources
if (NOT DEV_MODE)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/resources DESTINATION ${CMAKE_BINARY_DIR})
endif()