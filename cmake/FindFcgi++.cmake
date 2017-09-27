# - Try to find Fcgi++
# Once done, this will define
#
#  Fcgi++_FOUND - system has Fcgi++
#  Fcgi++_INCLUDE_DIRS - the Fcgi++ include directories
#  Fcgi++_LIBRARIES - link these to use Fcgi++

include(LibFindMacros)

libfind_package(Fcgi++ Fcgi)

libfind_pkg_check_modules(Fcgi++_PKGCONF fcgi++)

find_library(Fcgi++_LIBRARY NAMES fcgi++ libfcgi++ PATHS ${Fcgi++_PKGCONF_LIBRARY_DIRS})

set(Fcgi++_PROCESS_INCLUDES Fcgi_INCLUDE_DIR)
set(Fcgi++_PROCESS_LIBS Fcgi++_LIBRARY Fcgi_LIBRARY)

libfind_process(Fcgi++)
