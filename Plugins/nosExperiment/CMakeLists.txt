# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

if(NOT TARGET glfw)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/External/glfw" ${CMAKE_CURRENT_BINARY_DIR}/External/glfw)	
    nos_get_targets(NOS_EXPERIMENT_EXTERNAL_TARGETS "${CMAKE_CURRENT_BINARY_DIR}/External/glfw")
    nos_group_targets("${NOS_EXPERIMENT_EXTERNAL_TARGETS}" "External")
endif()

target_link_libraries(${NOS_PLUGIN_TARGET} PRIVATE glfw)
target_include_directories(${NOS_PLUGIN_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/External/glfw)