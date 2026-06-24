# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

# OpenFBX (submodule under External/openFBX), used to read object transforms out of
# .fbx files. Built under a plugin-unique target name so it never clashes with an
# "openFBX" target another plugin may create.
if (NOT TARGET nosGeometry_openFBX)
    add_library(nosGeometry_openFBX STATIC
        External/openFBX/src/libdeflate.c
        External/openFBX/src/ofbx.cpp)
    target_include_directories(nosGeometry_openFBX PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/External/openFBX/src")
    nos_group_targets("nosGeometry_openFBX" "External")
endif()

target_link_libraries(${NOS_PLUGIN_TARGET} PRIVATE nosGeometry_openFBX)

# CoordinateFrameConversion.hpp helpers need C++20
set_target_properties(${NOS_PLUGIN_TARGET} PROPERTIES CXX_STANDARD 20)
