# Project
project(mapping_core)

# CMake Settings
cmake_minimum_required(VERSION 3.8)
SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

## Debug
set(CMAKE_VERBOSE_MAKEFILE on)

# C++ Standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Flags
option(USE_OPENCL "Use OpenCL in MAPPING" ON)

# Executables
add_executable(mapping_cgi cgi.cpp)

add_executable(mapping_manager mapping_manager.cpp)

add_executable(mapping_unittests EXCLUDE_FROM_ALL test/unittests/init.cpp)

# Custom Macro for Internal Library Linking
macro(target_link_libraries_internal TARGET)
    target_link_libraries(${TARGET} -Wl,--whole-archive ${ARGN} -Wl,--no-whole-archive)
endmacro(target_link_libraries_internal)

# Libraries
add_library(core_lib
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
        util/sizeutil.cpp
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
target_include_directories(core_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries_internal(mapping_cgi core_lib)
target_link_libraries_internal(mapping_manager core_lib)
target_link_libraries_internal(mapping_unittests core_lib)

if (USE_OPENCL)
    target_sources(core_lib
            PRIVATE
            datatypes/raster/raster.cpp
            raster/opencl.cpp
            )
else (USE_OPENCL)
    target_sources(core_lib
            PRIVATE
            datatypes/raster/raster_nocl.cpp
            )
    target_compile_options(core_lib
            PUBLIC
            -DMAPPING_NO_OPENCL=1
            )
endif (USE_OPENCL)

add_library(services_lib
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
target_link_libraries_internal(services_lib core_lib)
target_link_libraries_internal(mapping_cgi services_lib)
target_link_libraries_internal(mapping_unittests services_lib)

add_library(operators_lib
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
        datatypes/plots/text.cpp
        datatypes/plots/png.cpp
        operators/source/featurecollectiondb_source.cpp
        )
target_link_libraries_internal(operators_lib core_lib)
target_link_libraries_internal(mapping_cgi operators_lib)
target_link_libraries_internal(mapping_manager operators_lib)
target_link_libraries_internal(mapping_unittests operators_lib)

add_library(extern_lib ext/jsoncpp/jsoncpp.cpp)
target_include_directories(extern_lib PUBLIC ext/jsoncpp)
target_link_libraries_internal(core_lib extern_lib)

# External Dependencies
## Core
find_package(Threads REQUIRED)
target_link_libraries(core_lib Threads::Threads)

find_package(BZip2 REQUIRED)
target_link_libraries(core_lib BZip2::BZip2)
target_include_directories(core_lib PRIVATE ${BZIP2_INCLUDE_DIR})

find_package(JPEGTURBO REQUIRED)
target_link_libraries(core_lib ${JPEGTURBO_LIBRARIES})
target_include_directories(core_lib PRIVATE ${JPEGTURBO_INCLUDE_DIR})

find_package(GEOS REQUIRED)
target_link_libraries(core_lib ${GEOS_LIBRARY})
target_link_libraries(core_lib geos) # TODO: extend find file for c++ lib
target_include_directories(core_lib PRIVATE ${GEOS_INCLUDE_DIR})

find_package(Boost COMPONENTS date_time REQUIRED)
target_link_libraries(core_lib Boost::date_time)
target_include_directories(core_lib PRIVATE ${Boost_INCLUDE_DIRS})

find_package(GDAL REQUIRED)
target_link_libraries(core_lib ${GDAL_LIBRARY})
target_include_directories(core_lib PUBLIC ${GDAL_INCLUDE_DIR})

find_package(PNG REQUIRED)
target_link_libraries(core_lib PNG::PNG)
target_include_directories(core_lib PRIVATE ${PNG_INCLUDE_DIRS})

find_package(ZLIB REQUIRED)
target_link_libraries(core_lib ZLIB::ZLIB)
target_include_directories(core_lib PRIVATE ${ZLIB_INCLUDE_DIRS})

find_package(PQXX REQUIRED)
target_link_libraries(core_lib ${Pqxx_LIBRARIES})
target_include_directories(core_lib PRIVATE ${Pqxx_INCLUDE_DIRS})

find_package(CURL REQUIRED)
target_link_libraries(core_lib ${CURL_LIBRARIES})
target_include_directories(core_lib PRIVATE ${CURL_INCLUDE_DIRS})

find_package(SQLite3 REQUIRED)
target_link_libraries(core_lib ${SQLITE3_LIBRARIES})
target_include_directories(core_lib PRIVATE ${SQLITE3_INCLUDE_DIRS})

find_package(LibArchive REQUIRED)
target_link_libraries(core_lib ${LibArchive_LIBRARIES})
target_include_directories(core_lib PRIVATE ${LibArchive_INCLUDE_DIRS})

## OpenCl
if (USE_OPENCL)
    find_package(OpenCL REQUIRED)
    target_link_libraries(core_lib OpenCL::OpenCL)
    target_include_directories(core_lib PRIVATE ${OPENCL_INCLUDE_DIRS})
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

# Testing
include(CTest)
enable_testing()

## Unit Tests
include(cmake/DownloadProject.cmake)
download_project(PROJ       googletest
        GIT_REPOSITORY      https://github.com/google/googletest.git
        GIT_TAG             release-1.8.0
        UPDATE_DISCONNECTED 1
        )

# Prevent GoogleTest from overriding our compiler/linker options
# when building with Visual Studio
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)

target_link_libraries(mapping_unittests gtest)

target_sources(mapping_unittests
        PRIVATE
            test/unittests/csvparser.cpp
            test/unittests/httpparsing.cpp
            test/unittests/init.cpp
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

add_test(NAME all_tests COMMAND mapping_unittests --gtest_color=yes --gtest_output="xml:test/unittest_latest.xml")
