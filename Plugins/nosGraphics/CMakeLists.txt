set(NOS_VULKAN_NAME "nos.sys.vulkan")
set(NOS_VULKAN_VERSION "6.0")

set(NOS_TRACK_NAME "nos.track")
set(NOS_TRACK_VERSION "1.9")

set(MODULE_DEPENDENCIES "${NOS_VULKAN_NAME}-${NOS_VULKAN_VERSION}" "${NOS_TRACK_NAME}-${NOS_TRACK_VERSION}")
set(dep_idx 0)
foreach(module_name_version ${MODULE_DEPENDENCIES})
    # module_name_version: <MODULE_NAME>-<MODULE_VERSION>
    string(REPLACE "-" ";" module_name_version ${module_name_version})
    list(GET module_name_version 0 module_name)
    list(GET module_name_version 1 module_version)
    nos_get_module("${module_name}" "${module_version}" DEP_${dep_idx})
    list(APPEND MODULE_DEPENDENCIES_TARGETS ${DEP_${dep_idx}})
endforeach()
list(APPEND MODULE_DEPENDENCIES_TARGETS ${NOS_PLUGIN_SDK_TARGET})

nos_find_module_path(${NOS_VULKAN_NAME} ${NOS_VULKAN_VERSION} NOS_VULKAN_PATH)

set(GENERATED_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/Generated")
nos_generate_flatbuffers("${CMAKE_CURRENT_SOURCE_DIR}/Config" "${GENERATED_OUTPUT_DIR}" "cpp" "${NOS_SDK_DIR}/types;${NOS_VULKAN_PATH}" nosGraphics_generated)

list(APPEND MODULE_DEPENDENCIES_TARGETS nosGraphics_generated)
set(INCLUDE_FOLDERS "${GENERATED_OUTPUT_DIR}")
nos_add_plugin("nosGraphics" "${MODULE_DEPENDENCIES_TARGETS}" "${INCLUDE_FOLDERS}")