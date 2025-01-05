cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

# Boost Version 1.76
#set(CMAKE_GENERATOR_TOOLSET "ClangCL")
set(ALLOW_EXTERNAL_SPIRV_TOOLS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(tangram VERSION 0.1.0)
set(TANGRAM_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE HEADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
file(GLOB_RECURSE SOURCE_FILES_CC "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cc")
file(GLOB_RECURSE SOURCE_FILES_C "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c")
file(GLOB_RECURSE DECOMPRESSOR_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/decompressor/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/decompressor/decompressor.cpp")

add_subdirectory(thirdparty/glslang)

add_definitions(-D CEREAL_THREAD_SAFE=1)
add_definitions(-D TANGRAM_DIR=${TANGRAM_ROOT_DIR})
add_definitions(-D TANGRAM_INTERMEDIATE_DIR=${TANGRAM_ROOT_DIR}/intermediate/)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" FILES ${HEADER_FILES} ${SOURCE_FILES} ${SOURCE_FILES_CC} ${SOURCE_FILES_C})
add_executable(${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES} ${SOURCE_FILES_CC} ${SOURCE_FILES_C} ${DECOMPRESSOR_SOURCE_FILES})

target_include_directories(tangram PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_include_directories(tangram PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/decompressor)
target_include_directories(tangram PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)
target_include_directories(tangram PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glslang/glslang)
target_include_directories(tangram PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/eigen)
target_include_directories(tangram PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/spdlog)
target_include_directories(tangram PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/xxhash)

target_link_libraries(tangram PRIVATE glslang)
target_link_libraries(tangram PRIVATE MachineIndependent)
target_link_libraries(tangram PRIVATE GenericCodeGen)
target_link_libraries(tangram PRIVATE glslang-default-resource-limits)


add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/decompressor)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/benchmark)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/example)
