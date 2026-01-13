# Copyright MediaZ Teknoloji A.S. All Rights Reserved.
if (NOT WIN32)
	nos_message(STATUS "nosPython plugin is currently only supported on Windows platform.")
	set_target_properties(${NOS_PLUGIN_TARGET} PROPERTIES EXCLUDE_FROM_ALL TRUE)
	return()
endif()

set(PYBIND11_NOPYTHON ON CACHE BOOL "" FORCE)

add_library(python3 STATIC IMPORTED)
set_target_properties(python3 PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../nosPythonDeps/3.12/libs/python3.lib)
set_target_properties(python3 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/../nosPythonDeps/3.12/include)

add_library(python312 STATIC IMPORTED)
set_target_properties(python312 PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/../nosPythonDeps/3.12/libs/python312.lib)
set_target_properties(python312 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/../nosPythonDeps/3.12/include)

# You must install Python with debug binaries checked
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/External/pybind11" "${CMAKE_CURRENT_BINARY_DIR}/External/pybind11" EXCLUDE_FROM_ALL)

nos_get_targets(NOSPY_EXTERNAL_TARGETS "${CMAKE_CURRENT_BINARY_DIR}/External/pybind11")
nos_group_targets("${NOSPY_EXTERNAL_TARGETS}" "External")

target_link_libraries(${NOS_PLUGIN_TARGET} PRIVATE python3 python312 pybind11::embed)