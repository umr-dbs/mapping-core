# Project
project(mapping_core
        LANGUAGES CXX
)

# CMake Settings
cmake_minimum_required(VERSION 3.5)
SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

## Debug
set(CMAKE_VERBOSE_MAKEFILE OFF)

# C++ Standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# User Options
option(USE_OPENCL "Use OpenCL in MAPPING" ON)
set(MAPPING_MODULE_PATH .. CACHE PATH "A list of mapping modules to include in the build process")
set(MAPPING_MODULES "" CACHE STRING "A list of mapping modules to include in the build process")


# Executables
add_executable(mapping_cgi cgi.cpp)

add_executable(mapping_manager mapping_manager.cpp)

add_executable(mapping_unittests EXCLUDE_FROM_ALL test/unittests/init.cpp)


# Deploy
# TODO: specify install script


# Libraries
include(LinkLibrariesInternal)
add_library(mapping_core_base_lib
        datatypes/attributes.cpp
        datatypes/spatiotemporal.cpp
        datatypes/raster.h
        datatypes/raster/raster_priv.h
        datatypes/raster/import_gdal.cpp
        datatypes/raster/export_pgm.cpp
        datatypes/raster/export_yuv.cpp
        datatypes/raster/export_png.cpp
        datatypes/raster/export_jpeg.cpp
        datatypes/simplefeaturecollection.cpp
        datatypes/pointcollection.cpp
        datatypes/linecollection.cpp
        datatypes/polygoncollection.cpp
        datatypes/simplefeaturecollections/geosgeomutil.cpp
        datatypes/simplefeaturecollections/wkbutil.cpp
        datatypes/unit.cpp
        datatypes/colorizer.cpp
        datatypes/plot.cpp
        datatypes/plots/histogram.cpp
        datatypes/plots/text.cpp
        datatypes/plots/png.cpp
        rasterdb/rasterdb.cpp
        rasterdb/backend.cpp
        rasterdb/backend_local.cpp
        rasterdb/converters/converter.cpp
        rasterdb/converters/raw.cpp
        userdb/userdb.cpp
        userdb/backend_sqlite.cpp
        featurecollectiondb/featurecollectiondb.cpp
        featurecollectiondb/featurecollectiondbbackend_postgres.cpp
        util/gdal.cpp
        util/sha1.cpp
        util/curl.cpp
        util/sqlite.cpp
        util/binarystream.cpp
        util/csvparser.cpp
        util/base64.cpp
        util/configuration.cpp
        util/formula.cpp
        util/timemodification.cpp
        util/log.cpp
        util/timeparser.cpp
        util/server_nonblocking.cpp
        util/sizeutil.cpp
        util/stringsplit.h
        util/uriloader.cpp
        operators/operator.cpp
        operators/provenance.cpp
        operators/queryrectangle.cpp
        operators/queryprofiler.cpp
        processing/query.cpp
        processing/queryprocessor.cpp
        processing/queryprocessor_backend.cpp
        processing/backend_local.cpp
        cache/common.cpp
        cache/priv/shared.cpp
        cache/priv/requests.cpp
        cache/priv/connection.cpp
        cache/priv/redistribution.cpp
        cache/priv/cache_stats.cpp
        cache/priv/cache_structure.cpp
        cache/node/node_cache.cpp
        cache/manager.cpp
        cache/priv/caching_strategy.cpp
        cache/priv/cube.cpp
        cache/node/manager/local_manager.cpp
        cache/node/node_manager.cpp
        cache/node/manager/local_replacement.cpp
        cache/node/puzzle_util.cpp
        cache/node/nodeserver.cpp
        cache/node/delivery.cpp
        cache/node/node_config.cpp
        cache/node/manager/remote_manager.cpp
        cache/node/manager/hybrid_manager.cpp
        )
target_include_directories(mapping_core_base_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries_internal(mapping_cgi mapping_core_base_lib)
target_link_libraries_internal(mapping_manager mapping_core_base_lib)

if (USE_OPENCL)
    target_sources(mapping_core_base_lib
            PRIVATE
            datatypes/raster/raster.cpp
            raster/opencl.cpp
            )
else (USE_OPENCL)
    target_sources(mapping_core_base_lib
            PRIVATE
            datatypes/raster/raster_nocl.cpp
            )
    target_compile_options(mapping_core_base_lib
            PUBLIC
            -DMAPPING_NO_OPENCL=1
            )
endif (USE_OPENCL)

add_library(mapping_core_services_lib
        services/httpservice.cpp
        services/httpparsing.cpp
        services/user.cpp
        services/ogcservice.cpp
        services/wms.cpp
        services/wcs.cpp
        services/wfs.cpp
        services/plot.cpp
        services/provenance.cpp
        services/artifact.cpp
        pointvisualization/BoundingBox.cpp
        pointvisualization/Circle.cpp
        pointvisualization/Coordinate.cpp
        pointvisualization/Dimension.cpp
        pointvisualization/FindResult.cpp
        pointvisualization/QuadTreeNode.cpp
        pointvisualization/CircleClusteringQuadTree.cpp
        services/featurecollectiondb.cpp
        )
target_link_libraries_internal(mapping_core_services_lib mapping_core_base_lib)
target_link_libraries_internal(mapping_cgi mapping_core_services_lib)
target_link_libraries_internal(mapping_unittests mapping_core_services_lib)

add_library(mapping_core_operators_lib
        operators/source/csv_source.cpp
        operators/source/postgres_source.cpp
        operators/source/rasterdb_source.cpp
        operators/source/wkt_source.cpp
        util/csv_source_util.cpp
        operators/processing/raster/matrixkernel.cpp
        operators/processing/raster/expression.cpp
        operators/processing/raster/classification.cpp
        operators/processing/raster/temporal_aggregation.cpp
        operators/processing/features/difference.cpp
        operators/processing/features/numeric_attribute_filter.cpp
        operators/processing/features/textual_attribute_filter.cpp
        operators/processing/features/point_in_polygon_filter.cpp
        operators/processing/combined/projection.cpp
        operators/processing/combined/raster_value_extraction.cpp
        operators/processing/combined/rasterization.cpp
        operators/processing/combined/rasterize_polygon.cpp
        operators/processing/combined/timeshift.cpp
        operators/processing/meteosat/temperature.cpp
        operators/processing/meteosat/reflectance.cpp
        operators/processing/meteosat/solarangle.cpp
        operators/processing/meteosat/radiance.cpp
        operators/processing/meteosat/pansharpening.cpp
        operators/processing/meteosat/gccthermthresholddetection.cpp
        operators/processing/meteosat/co2correction.cpp
        util/sunpos.cpp
        operators/plots/histogram.cpp
        operators/plots/feature_attributes_plot.cpp
        operators/source/featurecollectiondb_source.cpp
        util/rasterize_polygons.cpp util/rasterize_polygons.h)
target_link_libraries_internal(mapping_core_operators_lib mapping_core_base_lib)
target_link_libraries_internal(mapping_cgi mapping_core_operators_lib)
target_link_libraries_internal(mapping_manager mapping_core_operators_lib)
target_link_libraries_internal(mapping_unittests mapping_core_operators_lib)

if (USE_OPENCL)
    # Generate OpenCL headers
    add_custom_target(mapping_cl_header_generator)
    add_dependencies(mapping_core_operators_lib mapping_cl_header_generator)
    # TODO: make this generic for OS?
    foreach(cl_file IN ITEMS
            operators/processing/combined/points2raster_frequency.cl
            operators/processing/combined/points2raster_value.cl
            operators/processing/combined/raster_value_extraction.cl
            operators/processing/features/difference.cl
            operators/processing/meteosat/co2correction.cl
            operators/processing/meteosat/pansharpening_degenerate.cl
            operators/processing/meteosat/pansharpening_interpolate.cl
            operators/processing/meteosat/pansharpening_regression.cl
            operators/processing/meteosat/radiance.cl
            operators/processing/meteosat/reflectance.cl
            operators/processing/meteosat/solarangle.cl
            operators/processing/meteosat/temperature.cl
            operators/processing/raster/classification_kernels.cl
            operators/processing/raster/matrixkernel.cl
            )
        add_custom_command(TARGET mapping_cl_header_generator
                POST_BUILD
                # remove existing header file
                COMMAND rm -f ${cl_file}.h
                # create hexadecimal representation, surround it with an array declaration and a size variable
                COMMAND xxd -i ${cl_file} | sed 's/unsigned /static const /g' > ${cl_file}.h
                # create a static string of the previously created array and length
                COMMAND echo ${cl_file} | sed -e 's/\\.cl$$//' -e 's:/:_:g' -e 's/\\\(^.*$$\\\)/static const std::string \\1\(\\1_cl, \\1_cl_len\)\;/' >> ${cl_file}.h
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                )
    endforeach(cl_file)
endif (USE_OPENCL)

add_library(mapping_core_unittests_lib
        test/unittests/csvparser.cpp
        test/unittests/httpparsing.cpp
        test/unittests/parameters.cpp
        test/unittests/stref.cpp
        test/unittests/temporal
        test/unittests/units.cpp
        test/unittests/uriloader.cpp
        test/unittests/userdb.cpp
        test/unittests/featurecollectiondb/postgres.cpp
        #            test/unittests/ipc/countdownserver.cpp
        #            test/unittests/ipc/echoserver.cpp
        #            test/unittests/ipc/echoserver_mt.cpp
        test/unittests/ipc/serialization.cpp
        test/unittests/plots/plots.cpp
        test/unittests/pointvisualization/pointvisualization.cpp
        test/unittests/simplefeaturecollections/lines.cpp
        test/unittests/simplefeaturecollections/points.cpp
        test/unittests/simplefeaturecollections/polygons.cpp
        #            test/unittests/simplefeaturecollections/util.h
        test/unittests/temporal/timeparser.cpp
        test/unittests/temporal/timeshift.cpp
        test/unittests/util/formula.cpp
        test/unittests/util/sha1.cpp
        )
target_link_libraries_internal(mapping_core_unittests_lib mapping_core_operators_lib mapping_core_services_lib)
target_link_libraries_internal(mapping_unittests mapping_core_unittests_lib)


# External Dependencies
include(DownloadProject)

## Core
download_project(PROJ       jsoncpp
        GIT_REPOSITORY      https://github.com/open-source-parsers/jsoncpp.git
        GIT_TAG             1.7.7
        UPDATE_DISCONNECTED 1
        )
add_subdirectory(${jsoncpp_SOURCE_DIR} ${jsoncpp_BINARY_DIR} EXCLUDE_FROM_ALL)
target_link_libraries_internal(mapping_core_base_lib jsoncpp_lib_static)

find_package(Threads REQUIRED)
target_link_libraries(mapping_core_base_lib Threads::Threads)

find_package(BZip2 REQUIRED)
# target_link_libraries(mapping_core_base_lib BZip2::BZip2) # works only with CMAKE 3.7
target_link_libraries(mapping_core_base_lib ${BZIP2_LIBRARIES})
target_include_directories(mapping_core_base_lib PRIVATE ${BZIP2_INCLUDE_DIR})

find_package(JPEGTURBO REQUIRED)
target_link_libraries(mapping_core_base_lib ${JPEGTURBO_LIBRARIES})
target_include_directories(mapping_core_base_lib PRIVATE ${JPEGTURBO_INCLUDE_DIR})

find_package(GEOS REQUIRED)
target_link_libraries(mapping_core_base_lib ${GEOS_LIBRARY})
target_link_libraries(mapping_core_base_lib geos) # TODO: extend find file for c++ lib
target_include_directories(mapping_core_base_lib PRIVATE ${GEOS_INCLUDE_DIR})

find_package(Boost COMPONENTS date_time REQUIRED)
target_link_libraries(mapping_core_base_lib Boost::date_time)
target_include_directories(mapping_core_base_lib PRIVATE ${Boost_INCLUDE_DIRS})

find_package(GDAL REQUIRED)
target_link_libraries(mapping_core_base_lib ${GDAL_LIBRARY})
target_include_directories(mapping_core_base_lib PUBLIC ${GDAL_INCLUDE_DIR})

find_package(PNG REQUIRED)
target_link_libraries(mapping_core_base_lib PNG::PNG)
target_include_directories(mapping_core_base_lib PRIVATE ${PNG_INCLUDE_DIRS})

find_package(ZLIB REQUIRED)
target_link_libraries(mapping_core_base_lib ZLIB::ZLIB)
target_include_directories(mapping_core_base_lib PRIVATE ${ZLIB_INCLUDE_DIRS})

find_package(PQXX REQUIRED)
target_link_libraries(mapping_core_base_lib ${Pqxx_LIBRARIES})
target_include_directories(mapping_core_base_lib PRIVATE ${Pqxx_INCLUDE_DIRS})

find_package(CURL REQUIRED)
target_link_libraries(mapping_core_base_lib ${CURL_LIBRARIES})
target_include_directories(mapping_core_base_lib PRIVATE ${CURL_INCLUDE_DIRS})

find_package(SQLite3 REQUIRED)
target_link_libraries(mapping_core_base_lib ${SQLITE3_LIBRARIES})
target_include_directories(mapping_core_base_lib PRIVATE ${SQLITE3_INCLUDE_DIRS})

find_package(LibArchive REQUIRED)
target_link_libraries(mapping_core_base_lib ${LibArchive_LIBRARIES})
target_include_directories(mapping_core_base_lib PRIVATE ${LibArchive_INCLUDE_DIRS})

## OpenCl
if (USE_OPENCL)
    find_package(OpenCL REQUIRED)
    # target_link_libraries(mapping_core_base_lib OpenCL::OpenCL) # works only with CMAKE 3.7
    target_link_libraries(mapping_core_base_lib ${OpenCL_LIBRARIES})
    target_include_directories(mapping_core_base_lib PRIVATE ${OPENCL_INCLUDE_DIRS})
endif (USE_OPENCL)

## CGI
find_package(Fcgi REQUIRED)
target_link_libraries(mapping_cgi ${Fcgi_LIBRARIES})
target_link_libraries(mapping_unittests ${Fcgi_LIBRARIES})
target_include_directories(mapping_cgi PRIVATE ${Fcgi_INCLUDE_DIRS})

find_package(Fcgi++ REQUIRED)
target_link_libraries(mapping_cgi ${Fcgi++_LIBRARIES})
target_link_libraries(mapping_unittests ${Fcgi++_LIBRARIES})
target_include_directories(mapping_cgi PRIVATE ${Fcgi++_INCLUDE_DIRS})

## Unit Tests
download_project(PROJ       googletest
        GIT_REPOSITORY      https://github.com/google/googletest.git
        GIT_TAG             release-1.8.0
        UPDATE_DISCONNECTED 1
        )
# Prevent GoogleTest from overriding our compiler/linker options
# when building with Visual Studio
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
target_link_libraries(mapping_core_unittests_lib gtest)


# Testing
include(CTest)
enable_testing()

add_test(NAME
    all_tests COMMAND mapping_unittests --gtest_color=yes --gtest_output="xml:test/unittest_latest.xml")

# Modules
set(MAPPING_CORE_PATH ${CMAKE_SOURCE_DIR} CACHE PATH "This is the mapping-core directory." FORCE)
foreach (mapping_module IN ITEMS ${MAPPING_MODULES})
    message(STATUS "Loading Module `${mapping_module}`")
    add_subdirectory(${MAPPING_MODULE_PATH}/${mapping_module} ${CMAKE_BINARY_DIR}/${mapping_module})
endforeach (mapping_module)
## Base
foreach (mapping_core_addition IN ITEMS ${MAPPING_ADD_TO_BASE_OBJECTS})
    message(STATUS "Adding ${mapping_core_addition} to base.")
    target_sources(mapping_core_base_lib PRIVATE $<TARGET_OBJECTS:${mapping_core_addition}>)
endforeach(mapping_core_addition)
foreach (mapping_core_addition IN ITEMS ${MAPPING_ADD_TO_BASE_LIBRARIES})
    message(STATUS "Adding ${mapping_core_addition} to base.")
    target_link_libraries(mapping_core_operators_lib ${mapping_core_addition})
endforeach(mapping_core_addition)
## Operators
foreach (mapping_operators_addition IN ITEMS ${MAPPING_ADD_TO_OPERATORS_OBJECTS})
    message(STATUS "Adding ${mapping_operators_addition} to operators.")
    target_sources(mapping_core_operators_lib PRIVATE $<TARGET_OBJECTS:${mapping_operators_addition}>)
endforeach(mapping_operators_addition)
foreach (mapping_operators_addition IN ITEMS ${MAPPING_ADD_TO_OPERATORS_LIBRARIES})
    message(STATUS "Adding ${mapping_operators_addition} to operators.")
    target_link_libraries(mapping_core_operators_lib ${mapping_operators_addition})
endforeach(mapping_operators_addition)
## Services
foreach (mapping_services_addition IN ITEMS ${MAPPING_ADD_TO_SERVICES_OBJECTS})
    message(STATUS "Adding ${mapping_services_addition} to services.")
    target_sources(mapping_core_services_lib PRIVATE $<TARGET_OBJECTS:${mapping_services_addition}>)
endforeach(mapping_services_addition)
foreach (mapping_services_addition IN ITEMS ${MAPPING_ADD_TO_SERVICES_LIBRARIES})
    message(STATUS "Adding ${mapping_services_addition} to services.")
    target_link_libraries(mapping_core_services_lib ${mapping_services_addition})
endforeach(mapping_services_addition)
