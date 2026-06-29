# Copyright MediaZ Teknoloji A.S. All Rights Reserved.
target_include_directories(${NOS_PLUGIN_TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/External/asio/asio/include")

# Expose the public Include/ folder (generated Track / Guidance headers) to this
# plugin and to dependents (e.g. nos.graphics) that consume nos.track types.
target_include_directories(${NOS_PLUGIN_TARGET} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Include")
