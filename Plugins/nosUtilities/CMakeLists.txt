# Copyright MediaZ Teknoloji A.S. All Rights Reserved.
nos_generate_flatbuffers("${CMAKE_CURRENT_SOURCE_DIR}/Config" "${CMAKE_CURRENT_SOURCE_DIR}/Source" "cpp" "${NOS_SDK_DIR}/types" nosUtilities_generated)

set(MODULE_DEPENDENCIES "nos.sys.vulkan-6.6")
set(dep_idx 0)
foreach(module_name_version ${MODULE_DEPENDENCIES})
    # module_name_version: <MODULE_NAME>-<MODULE_VERSION>
    string(REPLACE "-" ";" module_name_version ${module_name_version})
    list(GET module_name_version 0 module_name)
    list(GET module_name_version 1 module_version)
    nos_get_module("${module_name}" "${module_version}" DEP_${dep_idx})
    list(APPEND MODULE_DEPENDENCIES_TARGETS ${DEP_${dep_idx}})
endforeach()

# FreeType is vendored as a submodule and built with no external dependencies
# so the text rendering node has a self-contained font rasterizer.
if (NOT TARGET freetype)
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    add_subdirectory(External/freetype EXCLUDE_FROM_ALL)
    nos_group_targets("freetype" "External")
endif()

list(APPEND DEPENDENCIES stb freetype nosUtilities_generated ${NOS_PLUGIN_SDK_TARGET} ${MODULE_DEPENDENCIES_TARGETS})
nos_add_plugin("nosUtilities" "${DEPENDENCIES}" "")
