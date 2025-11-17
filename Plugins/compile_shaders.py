#!/usr/bin/env python3
# Copyright MediaZ Teknoloji A.S. All Rights Reserved.


from subprocess import PIPE, run, call, Popen
import platform
from sys import stdout, stderr
import os
import sys
import threading
from loguru import logger

path = str(os.path.dirname(__file__))

def get_tools_dir():
    arch_mapping = {
        "amd64": "x86_64",
        "x86_64": "x86_64",
        "aarch64": "aarch64",
        "arm64": "aarch64",
    }
    arch_folder_name = arch_mapping.get(platform.machine().lower(), platform.machine().lower())
    return f"{path}/../Subsystems/nosShaderCompiler/Binaries/tools/{platform.system()}/{arch_folder_name}"
    

def embed_binary(filepath):
    result = run([f"{get_tools_dir()}/bin2header", filepath], stdout=PIPE, stderr=PIPE, universal_newlines=True)
    if result.returncode != 0:
        logger.warning(f"Failed to embed {filepath}")
        exit(result.returncode)
    else:
        os.replace(filepath + ".h", filepath + ".dat")

def compile_to_spv(filepath):
    logger.info(f"Compiling {filepath}")
    tools_dir = get_tools_dir()
    re = run([f"{tools_dir}/glslc", "-o",  f"{filepath}_.spv", filepath], stdout=stdout, stderr=stderr, universal_newlines=True)
    if re.returncode != 0:
        logger.error(f"Failed to compile {filepath}")
        return re.returncode
    else:
        re = run([f"{tools_dir}/spirv-opt", "-O", "-o",  f"{filepath}.spv", f"{filepath}_.spv"], stdout=stdout, stderr=stderr, universal_newlines=True)
        os.remove(f"{filepath}_.spv")
        if re.returncode != 0:
            logger.error(f"Failed to optimize {filepath}")
            return re.returncode
        else:
            embed_binary(f"{filepath}.spv")
            # os.remove(f"{filepath}.spv")
    return 0

def compile_shaders():
    is_any_compiled = False
    compile_threads = []
    for root, dirs, files in os.walk(path):
        for filepath in files:
            if filepath.endswith(".frag") or filepath.endswith(".vert") or filepath.endswith(".comp"):
                fullpath = os.path.join(root, filepath)
                compiled = fullpath + ".spv.dat"
                if os.path.exists(compiled) and os.stat(fullpath).st_mtime <= os.stat(compiled).st_mtime:
                    continue
                th = threading.Thread(target=compile_to_spv,args=(fullpath,))
                th.start()
                compile_threads.append(th)
                is_any_compiled = True
    if not is_any_compiled:
        logger.info("All shaders are up to date.")
    for th in compile_threads:
        th.join()

if __name__ == "__main__":
    logger.remove()
    logger.add(sys.stdout, format="<green>[Shader Compiler]</green> <level>{time:HH:mm:ss.SSS}</level> <level>{level}</level> <level>{message}</level>")
    compile_shaders()
