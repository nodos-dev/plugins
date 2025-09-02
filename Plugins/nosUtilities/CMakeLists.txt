# Copyright MediaZ Teknoloji A.S. All Rights Reserved.
nos_generate_flatbuffers("${CMAKE_CURRENT_SOURCE_DIR}/Types" "${CMAKE_CURRENT_SOURCE_DIR}/Include/nosUtilities" "cpp" "${NOS_SDK_DIR}/Types" nosUtilities_generated)
list(APPEND DEPENDENCIES stb nosUtilities_generated ${NOS_SYS_VULKAN_TARGET} ${NOS_PLUGIN_SDK_TARGET})
nos_add_plugin("nosUtilities" "${DEPENDENCIES}" "")