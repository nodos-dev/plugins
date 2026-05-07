# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

set(TINYEXPR_SOURCES External/tinyexpr-cpp/tinyexpr.cpp
                     External/tinyexpr-cpp/tinyexpr.h)
add_library(tinyexpr_cpp STATIC ${TINYEXPR_SOURCES})
target_include_directories(tinyexpr_cpp PUBLIC External/tinyexpr-cpp)
nos_group_targets("tinyexpr_cpp" "External")

set(GENERATED_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Generated")
nos_generate_flatbuffers("${CMAKE_CURRENT_SOURCE_DIR}/Config" "${GENERATED_OUTPUT_DIR}" "cpp" "${NOS_SDK_DIR}/Types" nosMath_generated)

set(DEPENDENCIES ${NOS_PLUGIN_SDK_TARGET} tinyexpr_cpp nosMath_generated)
set(INCLUDE_FOLDERS "${GENERATED_OUTPUT_DIR}")

nos_add_plugin("nosMath" "${DEPENDENCIES}" "${INCLUDE_FOLDERS}")
