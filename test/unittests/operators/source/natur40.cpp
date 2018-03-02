#include <gtest/gtest.h>
#include <json/json.h>
#include <util/timeparser.h>

#include "operators/operator.h"
#include "operators/source/natur40.h"
#include "unittests/simplefeaturecollections/util.h"

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
                {},
                1514764800, 1514851200
        );

//        printf("%s", query.c_str());

        std::string expected = R"(
 SELECT
 CASE time_start
 WHEN '-infinity' THEN '-infinity'::FLOAT8
 ELSE EXTRACT(EPOCH FROM time_start)
 END AS time_start,
 CASE time_end
 WHEN 'infinity' THEN 'infinity'::FLOAT8
 ELSE EXTRACT(EPOCH FROM time_end)
 END AS time_end,
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
 WHERE (time_start, time_end) OVERLAPS (to_timestamp(1514764800.000000)::TIMESTAMP WITHOUT TIME ZONE, to_timestamp(1514851200.000000)::TIMESTAMP WITHOUT TIME ZONE)
 ORDER BY node, time_start ASC;
 )";

        ASSERT_STREQ(expected.c_str(), query.c_str());
    }
}

TEST(Natur40, merge) {
    std::vector<Natur40SourceOperator::Row> locations{
            {"sb00", 1519762314, 1600000000, {{"longitude", 8.81105}, {"latitude", 50.80935}, {"satellites", 4}}, {}}
    };
    std::vector<Natur40SourceOperator::Row> temperatures{
            {"sb00", 1519814952, 1600000000, {{"temperature", 23.86}}, {}}
    };

    std::vector<Natur40SourceOperator::Row> results;

    Natur40SourceOperator::merge_results(
            {{"locations",    locations},
             {"temperatures", temperatures}},
            [&](const Natur40SourceOperator::Row &row) {
                results.emplace_back(row);
            }
    );

    std::vector<Natur40SourceOperator::Row> expected = {
            {"sb00", 1519762314, 1600000000, {{"longitude",   8.81105}, {"latitude", 50.80935}, {"satellites", 4}}, {}},
            {"sb00", 1519814952, 1600000000, {{"temperature", 23.86}},                                              {}}
    };

    ASSERT_EQ(results.size(), expected.size());

    for (int i = 0; i < results.size(); ++i) {
        ASSERT_EQ(results[i].node, expected[i].node);
        ASSERT_EQ(results[i].time_start, expected[i].time_start);
        ASSERT_EQ(results[i].time_end, expected[i].time_end);
        ASSERT_EQ(results[i].numeric_values, expected[i].numeric_values);
        ASSERT_EQ(results[i].textual_values, expected[i].textual_values);
    }
}

TEST(Natur40, feature_collection1) {
    const auto time_parser = TimeParser::create(TimeParser::Format::ISO);

    std::vector<Natur40SourceOperator::Row> locations{
            {
                    "sb00",
                    time_parser->parse("2018-02-27T21:11:54"),
                    time_parser->parse("2018-02-28T12:00:09"),
                    {{"longitude", 8.81105}, {"latitude", 50.80935}, {"satellites", 4}},
                    {}
            }
    };
    std::vector<Natur40SourceOperator::Row> temperatures{
            {
                    "sb00",
                    time_parser->parse("2018-02-28T11:49:12"),
                    time_parser->parse("2018-02-28T11:49:44"),
                    {{"temperature", 23.86}},
                    {}
            }
    };

    QueryRectangle qrect{
            SpatialReference{CrsId::wgs84()},
            TemporalReference{
                    TIMETYPE_UNIX,
                    time_parser->parse("2018-02-28T11:49:12"),
                    time_parser->parse("2018-02-28T11:49:12") + 0.0001,
            },
            QueryResolution::none()
    };

    const auto feature_collection = Natur40SourceOperator::create_feature_collection(
            {{"locations",    locations},
             {"temperatures", temperatures}},
            {"longitude", "latitude", "satellites", "temperature"},
            {},
            qrect
    );

//    printf("%s\n", feature_collection->toCSV().c_str());

    std::unique_ptr<PointCollection> expected = make_unique<PointCollection>(qrect);
    expected->addSinglePointFeature(Coordinate{8.81105, 50.80935});
    expected->setTimeStamps({time_parser->parse("2018-02-28T11:49:12")}, {time_parser->parse("2018-02-28T11:49:44")});

    expected->feature_attributes.addTextualAttribute("node", Unit::unknown(), {"sb00"});
    expected->feature_attributes.addNumericAttribute("satellites", Unit::unknown(), {4});
    expected->feature_attributes.addNumericAttribute("temperature", Unit::unknown(), {23.86});
//    expected->feature_attributes.addNumericAttribute("time_start", Unit::unknown(), {1519762314});
//    expected->feature_attributes.addNumericAttribute("time_end", Unit::unknown(), {1519814984});

    CollectionTestUtil::checkEquality(*expected, *feature_collection);
}

TEST(Natur40, feature_collection2) {
    const auto time_parser = TimeParser::create(TimeParser::Format::ISO);

    std::vector<Natur40SourceOperator::Row> locations{
            {
                    "bb00",
                    time_parser->parse("2018-02-28T16:43:24"),
                    time_parser->parse("9999-12-31T23:59:59"),
                    {{"longitude", 8.81065333333333}, {"latitude", 50.80978}, {"satellites", 4}},
                    {}
            },
            {
                    "sb00",
                    time_parser->parse("2018-03-01T12:47:04"),
                    time_parser->parse("2018-03-01T13:44:27"),
                    {{"longitude", 8.8108},           {"latitude", 50.80932}, {"satellites", 4}},
                    {}
            },
    };

    QueryRectangle qrect{
            SpatialReference{CrsId::wgs84()},
            TemporalReference{
                    TIMETYPE_UNIX,
                    time_parser->parse("2018-03-01T12:49:12"),
                    time_parser->parse("2018-03-01T12:49:12") + 0.0001,
            },
            QueryResolution::none()
    };

    const auto feature_collection = Natur40SourceOperator::create_feature_collection(
            {{"locations", locations}},
            {"longitude", "latitude", "satellites"},
            {},
            qrect
    );

//    printf("%s\n", feature_collection->toCSV().c_str());

    std::unique_ptr<PointCollection> expected = make_unique<PointCollection>(qrect);

    expected->addSinglePointFeature(Coordinate{8.81065333333333, 50.80978});
    expected->addSinglePointFeature(Coordinate{8.8108, 50.80932});

    expected->setTimeStamps(
            {time_parser->parse("2018-02-28T16:43:24"), time_parser->parse("2018-03-01T12:47:04")},
            {time_parser->parse("9999-12-31T23:59:59"), time_parser->parse("2018-03-01T13:44:27")}
    );

    expected->feature_attributes.addTextualAttribute("node", Unit::unknown(), {"bb00", "sb00"});
    expected->feature_attributes.addNumericAttribute("satellites", Unit::unknown(), {4, 4});

    CollectionTestUtil::checkEquality(*expected, *feature_collection);
}
