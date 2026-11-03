# Fails if any file in the core module includes a Hyprland header, directly
# or through the plugin module.
# The core module must build and test without a running Hyprland.
file(GLOB_RECURSE core_files "${CORE_DIR}/*.hpp" "${CORE_DIR}/*.cpp")
foreach(file IN LISTS core_files)
    file(STRINGS "${file}" includes REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"](hyprland|hyprutils|hyprlang|aquamarine|plugin)/")
    if(includes)
        message(FATAL_ERROR "${file} includes a Hyprland header: ${includes}")
    endif()
endforeach()
