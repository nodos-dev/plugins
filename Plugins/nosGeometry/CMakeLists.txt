# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

# Vendored OpenFBX, used to read object transforms out of .fbx files.
# Built under a plugin-unique target name so it never clashes with the
# "openFBX" target the zd plugins create from their own copy.
if (NOT TARGET nosGeometry_openFBX)
    add_library(nosGeometry_openFBX STATIC
        External/openFBX/libdeflate.c
        External/openFBX/ofbx.cpp)
    target_include_directories(nosGeometry_openFBX PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/External/openFBX")
    nos_group_targets("nosGeometry_openFBX" "External")
endif()

# nos.graphics provides the nos.graphics.TransformQ output type and the
# CoordinateFrame enum used to express FBX transforms in a frame convention
# that composes with nosTrack's Convert/Track Transform nodes.
set(MODULE_DEPENDENCIES "nos.graphics-0.5")
set(dep_idx 0)
foreach(module_name_version ${MODULE_DEPENDENCIES})
    string(REPLACE "-" ";" module_name_version ${module_name_version})
    list(GET module_name_version 0 module_name)
    list(GET module_name_version 1 module_version)
    nos_get_module("${module_name}" "${module_version}" DEP_${dep_idx})
    list(APPEND MODULE_DEPENDENCIES_TARGETS ${DEP_${dep_idx}})
    math(EXPR dep_idx "${dep_idx}+1")
endforeach()

# Generate the C++ header for nos.graphics' custom types (TransformQ) from the
# schema shipped with that module; nos.graphics does not ship the generated header.
nos_find_module_path("nos.graphics" "0.5" NOS_GRAPHICS_PATH)
set(GENERATED_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Generated")
nos_generate_flatbuffers("${NOS_GRAPHICS_PATH}/Config/Graphics.fbs" "${GENERATED_OUTPUT_DIR}" "cpp" "${NOS_GRAPHICS_PATH}/Config" nosGeometry_generated)
list(APPEND MODULE_DEPENDENCIES_TARGETS nosGeometry_generated)

set(DEPENDENCIES ${NOS_PLUGIN_SDK_TARGET} nosGeometry_openFBX ${MODULE_DEPENDENCIES_TARGETS})
set(INCLUDE_FOLDERS "${GENERATED_OUTPUT_DIR}")

nos_add_plugin("nosGeometry" "${DEPENDENCIES}" "${INCLUDE_FOLDERS}")

# Helpers need C++20
set_target_properties("nosGeometry" PROPERTIES CXX_STANDARD 20)
