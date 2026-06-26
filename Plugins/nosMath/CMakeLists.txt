# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 20)

set(TINYEXPR_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/External/tinyexpr-cpp/tinyexpr.cpp
                    ${CMAKE_CURRENT_SOURCE_DIR}/External/tinyexpr-cpp/tinyexpr.h)
add_library(tinyexpr_cpp STATIC ${TINYEXPR_SOURCES})
target_include_directories(tinyexpr_cpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/External/tinyexpr-cpp)
nos_group_targets("tinyexpr_cpp" "External")
target_link_libraries(${NOS_PLUGIN_TARGET} PRIVATE tinyexpr_cpp)

# Expose the public Include/ folder (CoordinateFrameConversion.hpp) + the shared
# coordinate/transform types (Types/Math.fbs) to this plugin and to dependents
# (nos.track, nos.graphics, nos.geometry).
target_include_directories(${NOS_PLUGIN_TARGET} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Include")
