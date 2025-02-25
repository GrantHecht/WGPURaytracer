
add_library(tiny_obj_loader_impl STATIC tiny_obj_loader_impl.cpp)

add_library(tiny_obj_loader INTERFACE)
target_include_directories(tiny_obj_loader INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(tiny_obj_loader_impl PUBLIC tiny_obj_loader)

# Set compiler options
if (MSVC)
    target_compile_options(tiny_obj_loader_impl
        PRIVATE 
            /W4
    )
else()
    target_compile_options(tiny_obj_loader_impl
        PRIVATE 
            -Wall 
            -Wextra 
            -pedantic
    )
endif()