# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

function(nos_plugin_common current_dir out_target_dependencies out_target_definitions)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
	nos_get_module("nos.sys.shaderc" "2.1" NOS_SHADERC_TARGET)
	nos_find_module_path("nos.sys.shaderc" "2.1" NOS_SHADERC_PATH)

    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${NOS_SHADERC_PATH}/Scripts/compile_shaders.py
        RESULT_VARIABLE COMPILE_SHADERS_RESULT
        WORKING_DIRECTORY ${current_dir}
    )
    if (NOT ${COMPILE_SHADERS_RESULT} EQUAL "0")
        message(FATAL_ERROR "Failed to compile shaders. Process returned ${COMPILE_SHADERS_RESULT}.")
    endif()

    set(${out_target_definitions} "NOS_DISABLE_DEPRECATED" PARENT_SCOPE)
endfunction()