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

# nos.sys.track provides the CoordinateFrame type and the shared
# CoordinateFrameConv.h helpers used to express FBX transforms in a frame
# convention that composes with nosTrack's Convert/Track Transform nodes.
set(MODULE_DEPENDENCIES "nos.sys.track-1.1")
set(dep_idx 0)
foreach(module_name_version ${MODULE_DEPENDENCIES})
    string(REPLACE "-" ";" module_name_version ${module_name_version})
    list(GET module_name_version 0 module_name)
    list(GET module_name_version 1 module_version)
    nos_get_module("${module_name}" "${module_version}" DEP_${dep_idx})
    list(APPEND MODULE_DEPENDENCIES_TARGETS ${DEP_${dep_idx}})
endforeach()

set(DEPENDENCIES ${NOS_PLUGIN_SDK_TARGET} nosGeometry_openFBX ${MODULE_DEPENDENCIES_TARGETS})
set(INCLUDE_FOLDERS "")

nos_add_plugin("nosGeometry" "${DEPENDENCIES}" "${INCLUDE_FOLDERS}")

# Helpers need C++20
set_target_properties("nosGeometry" PROPERTIES CXX_STANDARD 20)
