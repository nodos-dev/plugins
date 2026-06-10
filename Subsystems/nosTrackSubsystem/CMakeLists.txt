# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

set(MODULE_DEPENDENCIES "nos.sys.animation-1.5")
set(dep_idx 0)
foreach(module_name_version ${MODULE_DEPENDENCIES})
    # module_name_version: <MODULE_NAME>-<MODULE_VERSION>
    string(REPLACE "-" ";" module_name_version ${module_name_version})
    list(GET module_name_version 0 module_name)
    list(GET module_name_version 1 module_version)
    nos_get_module("${module_name}" "${module_version}" DEP_${dep_idx})
    list(APPEND MODULE_DEPENDENCIES_TARGETS ${DEP_${dep_idx}})
endforeach()

nos_generate_flatbuffers("${CMAKE_CURRENT_SOURCE_DIR}/Config" "${CMAKE_CURRENT_SOURCE_DIR}/Include/nosSysTrack" "cpp" "${NOS_SDK_DIR}/types" nosTrackSubsystem_generated)

list(APPEND MODULE_DEPENDENCIES_TARGETS ${NOS_SUBSYSTEM_SDK_TARGET})
list(APPEND MODULE_DEPENDENCIES_TARGETS nosTrackSubsystem_generated)

list(APPEND INCLUDE_FOLDERS ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/Include")
nos_add_subsystem("nosTrackSubsystem" "${MODULE_DEPENDENCIES_TARGETS}" "${INCLUDE_FOLDERS}")
