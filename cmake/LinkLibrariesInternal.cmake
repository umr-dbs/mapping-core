# Custom Macro for Internal Library Linking
macro(target_link_libraries_internal TARGET)
    target_link_libraries(${TARGET} -Wl,--whole-archive ${ARGN} -Wl,--no-whole-archive)
endmacro(target_link_libraries_internal)