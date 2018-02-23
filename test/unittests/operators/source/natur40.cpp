#include <gtest/gtest.h>
#include <json/json.h>
#include <operators/operator.h>

#include "operators/source/natur40.h"

TEST(Natur40, sqlView) {
    {
        std::stringstream stream;
        Natur40SourceOperator::add_view(stream, "temperatures", {"temperature"}, 1514764800, 1514851200);

        std::string query = stream.str();
//        printf("%s", query.c_str());

        std::string expected = "\n temperatures AS (\n SELECT * FROM (\n SELECT\n node,\n \"time\" AS time_start,\n (\n SELECT COALESCE(MIN(\"time\"), 'infinity')\n FROM temperatures o\n WHERE o.node = t.node AND o.\"time\" > t.\"time\"\n ) AS time_end,\n temperature\n FROM (\n SELECT node, \"time\", temperature FROM temperatures\n UNION\n SELECT node, '-infinity' as \"time\", NULL AS temperature\n FROM temperatures\n GROUP BY node\n ) t\n ) r\n WHERE (time_start, time_end) OVERLAPS (to_timestamp(1514764800.000000), to_timestamp(1514851200.000000))\n )\n ";

        ASSERT_STREQ(expected.c_str(), query.c_str());
    }

    {
        std::stringstream stream;
        Natur40SourceOperator::add_view(stream, "locations", {"longitude", "latitude", "altitude"}, 1514764800,
                                        1514851200);

        std::string query = stream.str();
//        printf("%s", query.c_str());

        std::string expected = "\n locations AS (\n SELECT * FROM (\n SELECT\n node,\n \"time\" AS time_start,\n (\n SELECT COALESCE(MIN(\"time\"), 'infinity')\n FROM locations o\n WHERE o.node = t.node AND o.\"time\" > t.\"time\"\n ) AS time_end,\n longitude, latitude, altitude\n FROM (\n SELECT node, \"time\", longitude, latitude, altitude FROM locations\n UNION\n SELECT node, '-infinity' as \"time\", NULL AS longitude, NULL AS latitude, NULL AS altitude\n FROM locations\n GROUP BY node\n ) t\n ) r\n WHERE (time_start, time_end) OVERLAPS (to_timestamp(1514764800.000000), to_timestamp(1514851200.000000))\n )\n ";

        ASSERT_STREQ(expected.c_str(), query.c_str());
    }
}

TEST(Natur40, mainQuery) {
    {
        std::stringstream stream;
        Natur40SourceOperator::add_main_query(
                stream,
                {"locations", "temperatures"},
                {"longitude", "latitude", "altitude", "temperature"},
                1514764800, 1514851200
        );

        std::string query = stream.str();
//        printf("%s", query.c_str());

        std::string expected = "\n SELECT\n node,\n GREATEST(locations.time_start, temperatures.time_start) as time_start,\n LEAST(locations.time_end, temperatures.time_end) as time_end,\n longitude, latitude, altitude, temperature\n FROM\n locations FULL OUTER JOIN temperatures USING(node)\n WHERE\n NOT (longitude IS NULL AND latitude IS NULL AND altitude IS NULL AND temperature IS NULL)\n  AND ((locations.time_start, locations.time_end) OVERLAPS (temperatures.time_start, temperatures.time_end) OR locations.node IS NULL OR temperatures.node IS NULL) \n AND (GREATEST(locations.time_start, temperatures.time_start), LEAST(locations.time_end, temperatures.time_end)) OVERLAPS (to_timestamp(1514764800.000000), to_timestamp(1514851200.000000))\n ";

        ASSERT_STREQ(expected.c_str(), query.c_str());
    }
}

TEST(Natur40, sqlQuery) {
    {
        std::stringstream stream;
        std::function<const std::vector<std::string>(const std::string &)> column_getter =
                [](const std::string &table) {
                    if (table == "locations") {
                        return std::vector<std::string> {"longitude", "latitude", "altitude"};
                    } else {
                        return std::vector<std::string> {"temperature"};
                    }
                };

        Natur40SourceOperator::add_query(
                stream,
                {"locations", "temperatures"},
                column_getter,
                1514764800, 1514851200
        );

        std::string query = stream.str();
//        printf("%s", query.c_str());

        std::string expected = "WITH \n locations AS (\n SELECT * FROM (\n SELECT\n node,\n \"time\" AS time_start,\n (\n SELECT COALESCE(MIN(\"time\"), 'infinity')\n FROM locations o\n WHERE o.node = t.node AND o.\"time\" > t.\"time\"\n ) AS time_end,\n longitude, latitude, altitude\n FROM (\n SELECT node, \"time\", longitude, latitude, altitude FROM locations\n UNION\n SELECT node, '-infinity' as \"time\", NULL AS longitude, NULL AS latitude, NULL AS altitude\n FROM locations\n GROUP BY node\n ) t\n ) r\n WHERE (time_start, time_end) OVERLAPS (to_timestamp(1514764800.000000), to_timestamp(1514851200.000000))\n )\n , \n temperatures AS (\n SELECT * FROM (\n SELECT\n node,\n \"time\" AS time_start,\n (\n SELECT COALESCE(MIN(\"time\"), 'infinity')\n FROM temperatures o\n WHERE o.node = t.node AND o.\"time\" > t.\"time\"\n ) AS time_end,\n temperature\n FROM (\n SELECT node, \"time\", temperature FROM temperatures\n UNION\n SELECT node, '-infinity' as \"time\", NULL AS temperature\n FROM temperatures\n GROUP BY node\n ) t\n ) r\n WHERE (time_start, time_end) OVERLAPS (to_timestamp(1514764800.000000), to_timestamp(1514851200.000000))\n )\n \n SELECT\n node,\n GREATEST(locations.time_start, temperatures.time_start) as time_start,\n LEAST(locations.time_end, temperatures.time_end) as time_end,\n longitude, latitude, altitude, temperature\n FROM\n locations FULL OUTER JOIN temperatures USING(node)\n WHERE\n NOT (longitude IS NULL AND latitude IS NULL AND altitude IS NULL AND temperature IS NULL)\n  AND ((locations.time_start, locations.time_end) OVERLAPS (temperatures.time_start, temperatures.time_end) OR locations.node IS NULL OR temperatures.node IS NULL) \n AND (GREATEST(locations.time_start, temperatures.time_start), LEAST(locations.time_end, temperatures.time_end)) OVERLAPS (to_timestamp(1514764800.000000), to_timestamp(1514851200.000000))\n ";

        ASSERT_STREQ(expected.c_str(), query.c_str());
    }
}
