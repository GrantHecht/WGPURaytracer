# Add executable
add_executable(App 
    app.cpp
    resource_manager.cpp
    webgpu_utils.cpp
    wgpu_cpp_impl.cpp
    main.cpp
)

# Set compiler options
if (MSVC)
    target_compile_options(App
        PRIVATE 
            /W4
    )
else()
    target_compile_options(App
        PRIVATE 
            -Wall 
            -Wextra 
            -pedantic
    )
endif()

# Set build mode specific options/definitions
if (DEV_MODE)
    target_compile_definitions(App PRIVATE PRINT_EXTRA_INFO)
    set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    set(RESOURCE_DIR "${CMAKE_SOURCE_DIR}/resources")  
else()
    add_custom_command(
        TARGET App POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:App>/shaders
    )
    set(SHADER_DIR "shaders")
    set(RESOURCE_DIR "../resources")
endif()

# Generate configuration header
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.in ${CMAKE_BINARY_DIR}/generated/config.hpp @ONLY)
include_directories(${CMAKE_BINARY_DIR}/generated)

# Link libraries
target_link_libraries(App
    PRIVATE 
        webgpu
        glfw
        glfw3webgpu
        imgui
        glm::glm
        magic_enum::magic_enum

        # Header only libraries build as implementation 
        # static libraries
        tiny_obj_loader_impl
        stb_image_impl
)