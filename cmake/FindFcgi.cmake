# - Try to find Fcgi
# Once done, this will define
#
#  Fcgi_FOUND - system has Fcgi
#  Fcgi_INCLUDE_DIRS - the Fcgi include directories
#  Fcgi_LIBRARIES - link these to use Fcgi

include(LibFindMacros)

libfind_pkg_check_modules(Fcgi_PKGCONF fcgi)

find_path(Fcgi_INCLUDE_DIR NAMES fcgio.h PATHS ${Fcgi_PKGCONF_INCLUDE_DIRS} PATH_SUFFIXES "fastcgi")
find_library(Fcgi_LIBRARY NAMES fcgi libfcgi PATHS ${Fcgi_PKGCONF_LIBRARY_DIRS})

set(Fcgi_PROCESS_INCLUDES Fcgi_INCLUDE_DIR)
set(Fcgi_PROCESS_LIBS Fcgi_LIBRARY)

libfind_process(Fcgi)
