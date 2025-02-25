add_library(stb_image_impl STATIC stb_image_impl.cpp)

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(stb_image_impl PUBLIC stb_image)

# Set compiler options
if (MSVC)
    target_compile_options(stb_image_impl
        PUBLIC
            /wd4244
        PRIVATE 
            /W4
    )
else()
    target_compile_options(stb_image_impl
        PRIVATE 
            -Wall 
            -Wextra 
            -pedantic
    )
endif()