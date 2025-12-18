# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

function(nos_plugin_common current_dir out_target_dependencies out_target_definitions)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    # Create target
    set(target_name "modules_shader_compilation")

    add_custom_target(${target_name}
        ALL
        COMMAND ${Python3_EXECUTABLE} ${current_dir}/Plugins/compile_shaders.py
        WORKING_DIRECTORY ${current_dir}
        COMMENT "Compiling shaders..."
        GLOBAL
    )

    # Append to list
    # Local list variable
    set(dep_list "${target_name}")

    # Export to parent
    set(${out_target_dependencies} "${dep_list}" PARENT_SCOPE)

    set(${out_target_definitions} "NOS_DISABLE_DEPRECATED" PARENT_SCOPE)

    nos_group_targets("${target_name}" "Build Tasks")
endfunction()