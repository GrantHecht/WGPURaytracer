# This file is part of the "Learn WebGPU for C++" book.
#   https://eliemichel.github.io/LearnWebGPU
# 
# MIT License
# Copyright (c) 2022-2024 Elie Michel and the wgpu-native authors
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

set(WEBGPU_BACKEND "WGPU" CACHE STRING "Backend implementation of WebGPU. Possible values are EMSCRIPTEN, WGPU, WGPU_STATIC and DAWN (it does not matter when using emcmake)")

if (NOT TARGET webgpu)
	string(TOUPPER ${WEBGPU_BACKEND} WEBGPU_BACKEND_U)

	if (WEBGPU_BACKEND_U STREQUAL "WGPU")
        # Determine the platform-specific library file
        # Construct the base URL and archive name based on the version.
        set(WGPU_NATIVE_BASE_URL "https://github.com/gfx-rs/wgpu-native/releases/download/${WGPU_NATIVE_VERSION}")

        # Determine platform, architecture
        if(WIN32)
            set(PLATFORM "windows")
            if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
                set(ARCH "x86_64")
            elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64")
                set(ARCH "aarch64")
            else()
                message(FATAL_ERROR "Unsupported Windows architecture: ${CMAKE_SYSTEM_PROCESSOR}")
            endif()
        elseif(APPLE)
            set(PLATFORM "macos")
            if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
                set(ARCH "x86_64")
            elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
                set(ARCH "aarch64")
            else()
                message(FATAL_ERROR "Unsupported macOS architecture: ${CMAKE_SYSTEM_PROCESSOR}")
            endif()
        else()  # Assume Linux.
            set(PLATFORM "linux")
            if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
                set(ARCH "x86_64")
            elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
                set(ARCH "aarch64")
            else()
                message(FATAL_ERROR "Unsupported Linux architecture: ${CMAKE_SYSTEM_PROCESSOR}")
            endif()
        endif()

        set(WGPU_NATIVE_RELEASE_FILE "wgpu-${PLATFORM}-${ARCH}-release.zip")
        set(WGPU_NATIVE_DEBUG_FILE "wgpu-${PLATFORM}-${ARCH}-debug.zip")

        set(WGPU_NATIVE_RELEASE_URL "${WGPU_NATIVE_BASE_URL}/${WGPU_NATIVE_RELEASE_FILE}")
        set(WGPU_NATIVE_DEBUG_URL "${WGPU_NATIVE_BASE_URL}/${WGPU_NATIVE_DEBUG_FILE}")

        # Declare the wgpu-native prebuilt library.
        FetchContent_Declare(
            wgpu_native_prebuilt_release
            URL "${WGPU_NATIVE_RELEASE_URL}"
        )
        FetchContent_Declare(
            wgpu_native_prebuilt_debug
            URL "${WGPU_NATIVE_DEBUG_URL}"
        )

        FetchContent_MakeAvailable(wgpu_native_prebuilt_release wgpu_native_prebuilt_debug)

        # Create an imported target for the prebuilt library
        add_library(webgpu SHARED IMPORTED GLOBAL)

        target_compile_definitions(webgpu INTERFACE WEBGPU_BACKEND_WGPU)

        if(WIN32)
            set(WGPU_LIB_NAME "wgpu_native.dll")
        elseif(APPLE)
            set(WGPU_LIB_NAME "libwgpu_native.dylib")
        else()
            set(WGPU_LIB_NAME "libwgpu_native.so")
        endif()

        set_target_properties(webgpu PROPERTIES
            IMPORTED_LOCATION_DEBUG "${wgpu_native_prebuilt_debug_SOURCE_DIR}/lib/${WGPU_LIB_NAME}" 
            IMPORTED_LOCATION_RELEASE "${wgpu_native_prebuilt_release_SOURCE_DIR}/lib/${WGPU_LIB_NAME}"
            INTERFACE_INCLUDE_DIRECTORIES "${wgpu_native_prebuilt_release_SOURCE_DIR}/include"
        )

        # Build C++ header
        set(py_cmd, "${CMAKE_CURRENT_SOURCE_DIR}/../utils/WebGPU-Cpp/generate.py -u ${wgpu_native_prebuilt_release_SOURCE_DIR}/include/webgpu.h -u ${wgpu_native_prebuilt_release_SOURCE_DIR}/include/wgpu.h -o ${wgpu_native_prebuilt_release_SOURCE_DIR}/include")
        add_custom_command(
            OUTPUT "${wgpu_native_prebuilt_release_SOURCE_DIR}/include/webgpu/webgpu.hpp"
            COMMAND pip install lxml
            COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/../utils/WebGPU-Cpp/fetch_default_values.py
                -d ${CMAKE_CURRENT_SOURCE_DIR}/../utils/WebGPU-Cpp/defaults.txt
            COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/../utils/WebGPU-Cpp/generate.py 
                -u ${wgpu_native_prebuilt_release_SOURCE_DIR}/include/webgpu/webgpu.h
                -u ${wgpu_native_prebuilt_release_SOURCE_DIR}/include/webgpu/wgpu.h
                -o ${wgpu_native_prebuilt_release_SOURCE_DIR}/include/webgpu/webgpu.hpp
        )
        add_custom_target(
            webgpu-cpp-header
            DEPENDS "${wgpu_native_prebuilt_release_SOURCE_DIR}/include/webgpu/webgpu.hpp"
        )
        add_dependencies(webgpu webgpu-cpp-header)

	elseif (WEBGPU_BACKEND_U STREQUAL "DAWN")

		FetchContent_DeclareShallowGit(
			webgpu-backend-dawn
			GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
			GIT_TAG        853fe9a472ad92f3323938c3c6bfdfedaed164ee # dawn-6512 + emscripten-v3.1.61
		)
		FetchContent_MakeAvailable(webgpu-backend-dawn)

	else()

		message(FATAL_ERROR "Invalid value for WEBGPU_BACKEND: possible values are EMSCRIPTEN, WGPU, WGPU_STATIC and DAWN, but '${WEBGPU_BACKEND_U}' was provided.")

	endif()
endif()
