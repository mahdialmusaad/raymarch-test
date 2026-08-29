cmake_minimum_required(VERSION 3.16)
project(raymarch LANGUAGES C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(GLFW_INSTALL OFF)
set(GLFW_BUILD_DOCS OFF)

add_subdirectory(external/glfw)

add_executable(raymarch
	src/main.c
	src/opengl.c
)

if(UNIX AND NOT APPLE)
	target_link_libraries(raymarch PRIVATE m)
endif()

target_include_directories(raymarch PRIVATE external)
target_link_libraries(raymarch PRIVATE glfw)
target_compile_definitions(raymarch PRIVATE GLFW_INCLUDE_NONE)

add_custom_target(copy_shaders ALL
	COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_CURRENT_SOURCE_DIR}/src/march.vs"
		"${CMAKE_CURRENT_BINARY_DIR}/march.vs"
	COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_CURRENT_SOURCE_DIR}/src/march.fs"
		"${CMAKE_CURRENT_BINARY_DIR}/march.fs"
)

add_dependencies(raymarch copy_shaders)
