# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

function(nos_plugin_common current_dir out_target_dependencies out_target_definitions)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
	nos_get_module("nos.sys.shaderc" "2.1" NOS_SHADERC_TARGET)
	nos_find_module_path("nos.sys.shaderc" "2.1" NOS_SHADERC_PATH)

    set(compile_shaders_target_name "modules_shader_compilation")
    add_custom_target(${compile_shaders_target_name}
        ALL
        COMMAND ${Python3_EXECUTABLE} ${NOS_SHADERC_PATH}/Scripts/compile_shaders.py
        WORKING_DIRECTORY ${current_dir}
        COMMENT "Compiling shaders..."
        GLOBAL
    )

    set(${out_target_dependencies} "${compile_shaders_target_name}" PARENT_SCOPE)

    set(${out_target_definitions} "NOS_DISABLE_DEPRECATED" PARENT_SCOPE)
    nos_group_targets("${compile_shaders_target_name}" "Build Tasks")
    endfunction()