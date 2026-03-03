# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

if (NOT TARGET stb)
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/External/stb)
endif()

target_link_libraries(${NOS_PLUGIN_TARGET} PRIVATE stb)