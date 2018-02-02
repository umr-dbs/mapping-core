#include "datatypes/spatiotemporal.h"
#include "util/timeparser.h"
#include "datatypes/simplefeaturecollection.h"

#include <string>
#include <gtest/gtest.h>
#include <ctime>

TEST(STRef, toISOBeginOfTime) {
	TemporalReference tref(timetype_t::TIMETYPE_UNIX);

	std::string iso = tref.toIsoString(tref.beginning_of_time());
	EXPECT_EQ("0001-01-01T00:00:00", iso);
}

TEST(STRef, toISOnEndOfTime) {
	TemporalReference tref(timetype_t::TIMETYPE_UNIX);

	std::string iso = tref.toIsoString(tref.end_of_time());
	EXPECT_EQ("9999-12-31T23:59:59", iso);
}

TEST(STRef, temporalIntersectionWithIntervalsToEndOfTime) {
	auto parser = TimeParser::create(TimeParser::Format::ISO);

	double time = parser->parse("2014-11-24T16:23:57");
	double timeRef= parser->parse("2015-01-01T00:00:00");

	TemporalReference tref(timetype_t::TIMETYPE_UNIX);

	tref.t1 = timeRef;
	tref.t2 = tref.end_of_time();

	EXPECT_TRUE(tref.intersects(time, tref.end_of_time()));
}

void checkCoordinates(const std::vector<Coordinate> expected, const std::vector<Coordinate> actual) {
    EXPECT_EQ(actual.size(), expected.size());

    for(size_t i = 0; i < expected.size(); ++i) {
        EXPECT_TRUE(expected[i].almostEquals(actual[i]));
    }
}

TEST(SRef, sampleBorders) {
	SpatialReference sref(CrsId::unreferenced(), 0, 0, 10, 10);

	auto samples = sref.sample_borders(4);

	std::vector<Coordinate> expected {Coordinate(0, 0), Coordinate(10, 0), Coordinate(10, 10), Coordinate(0, 10)};

    checkCoordinates(expected, samples);
}

TEST(SRef, sampleBorders2) {
    SpatialReference sref(CrsId::unreferenced(), 0, 0, 10, 10);

    auto samples = sref.sample_borders(8);

    std::vector<Coordinate> expected {
            Coordinate(0, 0), Coordinate(10, 0), Coordinate(10, 10), Coordinate(0, 10),
            Coordinate(5, 0), Coordinate(10, 5), Coordinate(5, 10), Coordinate(0, 5)
    };

    checkCoordinates(expected, samples);
}
