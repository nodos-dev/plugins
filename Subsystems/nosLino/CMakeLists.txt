# Copyright MediaZ Teknoloji A.S. All Rights Reserved.
cmake_minimum_required(VERSION 3.24.2)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 20)

if (NOT WITH_NODOS_WORKSPACE)
    message(FATAL_ERROR "This repo currently does not support builds without Nodos workspace. "
    "Place this repo under nodos-workspace/Module folder and run cmake -S ./Toolchain/CMake -B Build from workspace root.")
endif()

# Dependencies
# ------------

add_compile_definitions(NOS_DISABLE_DEPRECATED)
# Nodos SDK
nos_find_sdk("1.3.0" NOS_PLUGIN_SDK_TARGET NOS_SUBSYSTEM_SDK_TARGET NOS_SDK_DIR)
set(FLATC_EXECUTABLE ${NOS_SDK_DIR}/bin/flatc${CMAKE_EXECUTABLE_SUFFIX})

nos_generate_flatbuffers("${CMAKE_CURRENT_SOURCE_DIR}/Types" "${CMAKE_CURRENT_SOURCE_DIR}/Include" "cpp" "${NOS_SDK_DIR}/types" generated_nosLinoSubsystem)

# Ensure the generated flatbuffers target is built
add_custom_target(build_nosLinoSubsystem ALL DEPENDS generated_nosLinoSubsystem)


