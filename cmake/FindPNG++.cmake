# - Try to find PNG++
# Once done, this will define
#
#  PNG++_FOUND - system has PNG++
#  PNG++_INCLUDE_DIR - the PNG++ include directory

# unset(PNG++_INCLUDE_DIR CACHE)
find_path(
        PNG++_INCLUDE_DIR
        NAMES "png.hpp"
        PATHS "/usr/include/png++" "/usr/local/include/png++"
        PATH_SUFFIXES "png++"
)

if (EXISTS ${PNG++_INCLUDE_DIR})
    set(PNG++_FOUND ON)
    message(STATUS "Found PNG++: ${PNG++_INCLUDE_DIR}")
else ()
    MESSAGE(SEND_ERROR "Cannot find library `libpng++`.")
endif ()
