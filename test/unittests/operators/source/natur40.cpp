#include <gtest/gtest.h>
#include <json/json.h>
#include <operators/operator.h>

#include "operators/source/natur40.h"

TEST(Natur40, parsing) {
    std::string result = Natur40SourceOperator::parse_query("SELECT * FROM @table@;", {{"@table@", "foobar"}});

    std::string expected = "SELECT * FROM foobar;";

    ASSERT_STREQ(expected.c_str(), result.c_str());
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

        std::string query = Natur40SourceOperator::table_query(
                "locations",
                {"longitude", "latitude", "altitude"},
                1514764800, 1514851200
        );

//        printf("%s", query.c_str());

        std::string expected = R"(
 SELECT
 EXTRACT(EPOCH FROM time_start) AS time_start,
 EXTRACT(EPOCH FROM time_end) AS time_end,
 node,
 longitude, latitude, altitude
 FROM (
 SELECT
 time as time_start,
 COALESCE(
 LEAD(time) OVER (PARTITION BY node
 ORDER BY time ASC
 ROWS BETWEEN CURRENT ROW AND 1 FOLLOWING),
 'infinity'
 ) AS time_end,
 node,
 longitude, latitude, altitude
 FROM locations
 ) t
 WHERE (time_start, time_end) OVERLAPS (to_timestamp(1514764800.000000), to_timestamp(1514851200.000000))
 ORDER BY node, time_start ASC;
 )";

        ASSERT_STREQ(expected.c_str(), query.c_str());
    }
}
