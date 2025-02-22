# Add executable
add_executable(App 
    app.cpp
    webgpu_utils.cpp
    main.cpp
)

add_subdirectory(glfw3webgpu)

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

# Link libraries
target_link_libraries(App
    PRIVATE 
        webgpu
        glfw
        glfw3webgpu
)

# Set extra printing
if (WGPU_INFO)
    add_compile_definitions(PRINT_ADAPTER_INFO)
    add_compile_definitions(PRINT_DEVICE_INFO)
endif()

# The application's binary must find wgpu.dll or libwgpu.so at runtime,
# so we automatically copy it (it's called WGPU_RUNTIME_LIB in general)
# next to the binary.
#target_copy_webgpu_binaries(App)