# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

# Dependencies
nos_get_plugin("nos.sync" "11.1" NOS_SYNC_TARGET)
nos_find_plugin_path("nos.sync" "11.1" NOS_SYNC_PATH)
message("**********************************************: ${NOS_SYS_VULKAN_PATH}")
nos_generate_flatbuffers("${CMAKE_CURRENT_SOURCE_DIR}/Types" "${CMAKE_CURRENT_SOURCE_DIR}/Include/nosUtilities"
    "cpp" "${NOS_SDK_DIR}/Types;${NOS_SYNC_PATH};${NOS_SYS_VULKAN_PATH}"
    nosUtilities_generated)

list(APPEND DEPENDENCIES stb nosUtilities_generated ${NOS_PLUGIN_SDK_TARGET} ${NOS_SYS_VULKAN_TARGET} ${NOS_SYNC_TARGET})
nos_add_plugin("nosUtilities" "${DEPENDENCIES}" "")