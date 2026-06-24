# Copyright MediaZ Teknoloji A.S. All Rights Reserved.

# FreeType (submodule under External/freetype) is a self-contained SDF font
# rasterizer for the TextRender node. All optional codec deps are disabled so it
# builds with no external dependencies. Guarded so it never clashes with a
# "freetype" target another plugin may create.
if (NOT TARGET freetype)
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    add_subdirectory(External/freetype EXCLUDE_FROM_ALL)
    nos_group_targets("freetype" "External")
endif()

target_link_libraries(${NOS_PLUGIN_TARGET} PRIVATE freetype)
