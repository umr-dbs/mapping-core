# - Try to find PQXX++
# Once done, this will define
#
#  Pqxx_FOUND - system has PQXX
#  Pqxx_INCLUDE_DIRS - the PQXX include directories
#  Pqxx_LIBRARIES - link these to use PQXX

include(LibFindMacros)

libfind_pkg_check_modules(Pqxx_PKGCONF pqxx)

find_path(Pqxx_INCLUDE_DIR pqxx/pqxx PATHS ${Pqxx_PKGCONF_INCLUDE_DIRS} PATH_SUFFIXES "include")
find_library(Pqxx_LIBRARY NAMES pqxx libpqxx PATHS ${Pqxx_PKGCONF_LIBRARY_DIRS})

set(Pqxx_PROCESS_INCLUDES Pqxx_INCLUDE_DIR)
set(Pqxx_PROCESS_LIBS Pqxx_LIBRARY)

libfind_process(Pqxx)
