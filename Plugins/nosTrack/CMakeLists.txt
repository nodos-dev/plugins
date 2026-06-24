# Copyright MediaZ Teknoloji A.S. All Rights Reserved.
target_include_directories(${NOS_PLUGIN_TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/External/asio/asio/include")

# Expose the public Include/ folder (CoordinateFrameConversion.hpp) to this plugin
# and to dependents (nos.graphics, nos.geometry) that consume the shared coordinate
# helpers. The coordinate/transform types live here too (Types/Coordinates.fbs).
target_include_directories(${NOS_PLUGIN_TARGET} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Include")
