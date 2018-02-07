# Executables
add_executable(mapping_unittests EXCLUDE_FROM_ALL unittests/init.cpp)

add_library(mapping_core_unittests_lib
        unittests/csvparser.cpp
        unittests/httpparsing.cpp
        unittests/parameters.cpp
        unittests/stref.cpp
        unittests/temporal
        unittests/units.cpp
        unittests/uriloader.cpp
        unittests/userdb.cpp
        unittests/featurecollectiondb/postgres.cpp
        #            unittests/ipc/countdownserver.cpp
        #            unittests/ipc/echoserver.cpp
        #            unittests/ipc/echoserver_mt.cpp
        unittests/ipc/serialization.cpp
        unittests/plots/plots.cpp
        unittests/pointvisualization/pointvisualization.cpp
        unittests/simplefeaturecollections/lines.cpp
        unittests/simplefeaturecollections/points.cpp
        unittests/simplefeaturecollections/polygons.cpp
        #            unittests/simplefeaturecollections/util.h
        unittests/temporal/timeparser.cpp
        unittests/temporal/timeshift.cpp
        unittests/util/formula.cpp
        unittests/util/sha1.cpp
        unittests/gdal_source.cpp
        )

target_include_directories(mapping_core_unittests_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries_internal(mapping_core_unittests_lib mapping_core_operators_lib mapping_core_services_lib)
target_link_libraries_internal(mapping_unittests mapping_core_unittests_lib)
target_link_libraries_internal(mapping_unittests mapping_core_services_lib)
target_link_libraries_internal(mapping_unittests mapping_core_operators_lib)

target_link_libraries(mapping_unittests ${Fcgi_LIBRARIES})
target_link_libraries(mapping_unittests ${Fcgi++_LIBRARIES})

## Unit Tests
download_project(PROJ       googletest
        GIT_REPOSITORY      https://github.com/google/googletest.git
        GIT_TAG             release-1.8.0
        UPDATE_DISCONNECTED 1
        PREFIX CMakeFiles/Download
        )
# Prevent GoogleTest from overriding our compiler/linker options
# when building with Visual Studio
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR} EXCLUDE_FROM_ALL)
target_link_libraries(mapping_core_unittests_lib gtest)


add_test(NAME
    all_tests COMMAND mapping_unittests --gtest_color=yes --gtest_output="xml:unittest_latest.xml")

list(APPEND systemtests classification_ndvi)
list(APPEND systemtests csv_source_data_uri)
list(APPEND systemtests csv_source_load_lines)
list(APPEND systemtests csv_source_load_polygons)
list(APPEND systemtests csv_source_load_wkt_time)
list(APPEND systemtests csv_source_load_wkt_time_cut)
list(APPEND systemtests csv_source_load_xy_no_time)
list(APPEND systemtests csv_source_multiple_errors)
list(APPEND systemtests difference)
list(APPEND systemtests expression_add_two_rasters)
list(APPEND systemtests expression_identity)
list(APPEND systemtests gdal_source_1)
list(APPEND systemtests gdal_source_10)
list(APPEND systemtests gdal_source_11)
list(APPEND systemtests gdal_source_12)
list(APPEND systemtests gdal_source_2)
list(APPEND systemtests gdal_source_3)
list(APPEND systemtests gdal_source_4)
list(APPEND systemtests gdal_source_5)
list(APPEND systemtests gdal_source_6)
list(APPEND systemtests gdal_source_7)
list(APPEND systemtests gdal_source_8)
list(APPEND systemtests gdal_source_9)
list(APPEND systemtests gdal_source_TestDataset)
list(APPEND systemtests gdal_source_test)
list(APPEND systemtests gdal_source_test_comp)
list(APPEND systemtests gdal_source_test_overviews)
list(APPEND systemtests histogram_points_data)
list(APPEND systemtests histogram_points_estimate)
list(APPEND systemtests histogram_points_min_max)
list(APPEND systemtests histogram_raster_range_data)
list(APPEND systemtests histogram_raster_range_estimate)
list(APPEND systemtests histogram_raster_range_min_max)
list(APPEND systemtests histogram_raster_range_unit)
list(APPEND systemtests numeric_attribute_filter_max)
list(APPEND systemtests numeric_attribute_filter_min)
list(APPEND systemtests numeric_attribute_filter_min_max)
list(APPEND systemtests ogr_source_4)
list(APPEND systemtests ogr_source_5)
list(APPEND systemtests point_in_polygon_filter_no_time)
list(APPEND systemtests point_in_polygon_filter_time)
list(APPEND systemtests point_in_polygon_filter_time_merge)
list(APPEND systemtests point_in_polygon_filter_time_mix)
list(APPEND systemtests point_in_polygon_filter_time_split)
list(APPEND systemtests polygon_rasterization)
list(APPEND systemtests polygon_rasterization_hole)
list(APPEND systemtests polygon_rasterization_two)
list(APPEND systemtests polygon_rasterization_uneven)
list(APPEND systemtests projection_4326_to_3857_lines)
list(APPEND systemtests projection_4326_to_3857_points)
list(APPEND systemtests projection_4326_to_3857_polygons)
list(APPEND systemtests projection_4326_to_3857_raster)
list(APPEND systemtests raster_value_extraction_multiple)
list(APPEND systemtests raster_value_extraction_polygon)
list(APPEND systemtests raster_value_extraction_raster_timeseries)
list(APPEND systemtests rasterdb_source_timeseries)
list(APPEND systemtests rasterdb_source_world)
list(APPEND systemtests rasterdb_source_world_cut)
list(APPEND systemtests rasterdb_source_world_double_resolution)
list(APPEND systemtests rasterdb_source_world_half_resolution)
list(APPEND systemtests rasterdb_source_world_loose)
list(APPEND systemtests rasterdb_source_world_loose2)
list(APPEND systemtests rasterdb_source_world_test)
list(APPEND systemtests temporal_aggregation_avg_1)
list(APPEND systemtests temporal_aggregation_avg_2)
list(APPEND systemtests temporal_aggregation_min_1)
list(APPEND systemtests temporal_aggregation_min_2)
list(APPEND systemtests textual_attribute_filter_contains)
list(APPEND systemtests textual_attribute_filter_exact)
list(APPEND systemtests textual_attribute_filter_startswith)
list(APPEND systemtests wkt_source_lines)
list(APPEND systemtests wkt_source_points)
list(APPEND systemtests wkt_source_polygons)
list(APPEND systemtests wkt_source_polygons_time)

# TODO: replace with cmake command
add_test(SETUP_SYSTEMTESTS bash systemtests/setup.sh ${CMAKE_BINARY_DIR})
set_tests_properties(SETUP_SYSTEMTESTS PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

foreach(systemtest ${systemtests})
    add_test(SYSTEMTEST_${systemtest} "${CMAKE_BINARY_DIR}/mapping_manager" testquery systemtests/queries/${systemtest}.json)
    set_tests_properties(SYSTEMTEST_${systemtest} PROPERTIES FIXTURES_REQUIRED SYSTEMTESTS
                                                             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach(systemtest)
