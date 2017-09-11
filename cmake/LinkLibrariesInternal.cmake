# Custom Macro for Internal Library Linking
macro(target_link_libraries_internal TARGET)
    target_link_libraries(${TARGET} -Wl,--whole-archive ${ARGN} -Wl,--no-whole-archive)
endmacro(target_link_libraries_internal)

macro(target_add_internal_linkage TARGET)
    if (WIN32)
        set_target_properties(${TARGET} PROPERTIES
                LINK_FLAGS "/WHOLEARCHIVE"
                )
    elseif (APPLE)
        set_target_properties(${TARGET} PROPERTIES
                LINK_FLAGS "-Wl,-all_load"
                )
    else ()
        set_target_properties(${TARGET} PROPERTIES
                LINK_FLAGS "-Wl,--whole-archive"
                )
    endif ()
endmacro(target_add_internal_linkage)