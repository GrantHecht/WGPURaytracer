cmake_minimum_required(VERSION 3.0.0...3.24 FATAL_ERROR)

include(FetchContent)

# FetchContent's GIT_SHALLOW option is buggy and does not actually do a shallow
# clone. This macro takes care of it.
macro(FetchContent_DeclareShallowGit Name GIT_REPOSITORY GitRepository GIT_TAG GitTag)
	FetchContent_Declare(
		"${Name}"
		DOWNLOAD_COMMAND
			cd "${FETCHCONTENT_BASE_DIR}/${Name}-src" &&
			git init &&
			git fetch --depth=1 "${GitRepository}" "${GitTag}" &&
			git reset --hard FETCH_HEAD
	)
endmacro()

include(webgpu.cmake)
include(glfw.cmake)
include(glm.cmake)
include(magic_enum.cmake)