# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

# nos.sys.track provides the Track type. nos.graphics owns the CoordinateFrame
# type and the shared CoordinateFrameConversion.hpp helpers used by the Convert/
# Track Transform and COLMAP nodes.
set(MODULE_DEPENDENCIES "nos.sys.track-1.1" "nos.graphics-0.5")
set(dep_idx 0)
foreach(module_name_version ${MODULE_DEPENDENCIES})
    # module_name_version: <MODULE_NAME>-<MODULE_VERSION>
    string(REPLACE "-" ";" module_name_version ${module_name_version})
    list(GET module_name_version 0 module_name)
    list(GET module_name_version 1 module_version)
    nos_get_module("${module_name}" "${module_version}" DEP_${dep_idx})
    list(APPEND MODULE_DEPENDENCIES_TARGETS ${DEP_${dep_idx}})
    math(EXPR dep_idx "${dep_idx}+1")
endforeach()

list(APPEND MODULE_DEPENDENCIES_TARGETS ${NOS_PLUGIN_SDK_TARGET})

nos_generate_flatbuffers("${CMAKE_CURRENT_SOURCE_DIR}/Config" "${CMAKE_CURRENT_SOURCE_DIR}/Source" "cpp" "" nosTrack_generated)
list(APPEND MODULE_DEPENDENCIES_TARGETS nosTrack_generated)

# Generate the C++ header for the CoordinateFrame enum from nos.graphics' schema;
# nos.graphics does not ship the generated header.
nos_find_module_path("nos.graphics" "0.5" NOS_GRAPHICS_PATH)
set(GENERATED_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Generated")
nos_generate_flatbuffers("${NOS_GRAPHICS_PATH}/Config/Graphics.fbs" "${GENERATED_OUTPUT_DIR}" "cpp" "${NOS_GRAPHICS_PATH}/Config" nosTrack_graphics_generated)
list(APPEND MODULE_DEPENDENCIES_TARGETS nosTrack_graphics_generated)

nos_add_plugin("nosTrack" "${MODULE_DEPENDENCIES_TARGETS}" "${CMAKE_CURRENT_LIST_DIR}/External/asio/asio/include;${GENERATED_OUTPUT_DIR}")
