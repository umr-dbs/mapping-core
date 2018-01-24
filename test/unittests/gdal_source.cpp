#include "util/gdal_timesnap.h"

#include <gtest/gtest.h>

ptime ptime_from_iso_string(const std::string &string) {
    auto* f = new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%S");

    std::locale loc(std::locale(""), f);
    std::stringstream ss(string);
    ss.imbue(loc);

    ptime pt;
    ss >> pt;
    return pt;
}

static void testSnap(TimeUnit timeUnit, int timeInterval, const std::string &start, const std::string &wanted, const std::string &expected) {
    auto start_time = ptime_from_iso_string(start);
    auto wanted_time = ptime_from_iso_string(wanted);
    auto expected_time = ptime_from_iso_string(expected);


    ptime snapped = GDALTimesnap::snapToInterval(timeUnit, timeInterval, start_time, wanted_time);

    std::string string = to_iso_extended_string(snapped);

    ASSERT_EQ(expected, string);
}

TEST(GDALSource, TimeSnapMonthN1) {
    testSnap(TimeUnit::Month, 1, "2000-01-01T00:00:00", "1999-11-01T00:00:00", "1999-11-01T00:00:00");
}

TEST(GDALSource, TimeSnapMonth1) {
    testSnap(TimeUnit::Month, 1, "2000-01-01T00:00:00", "2000-11-11T11:11:11", "2000-11-01T00:00:00");
}

TEST(GDALSource, TimeSnapMonth3) {
    testSnap(TimeUnit::Month, 3, "2000-01-01T00:00:00", "2000-11-11T11:11:11", "2000-10-01T00:00:00");
}

TEST(GDALSource, TimeSnapMonth7) {
    testSnap(TimeUnit::Month, 7, "2000-01-01T00:00:00", "2001-01-01T11:11:11", "2000-08-01T00:00:00");
}

TEST(GDALSource, TimeSnapYear1) {
    testSnap(TimeUnit::Year, 1, "2010-01-01T00:00:00", "2014-01-03T01:01:00", "2014-01-01T00:00:00");
}

TEST(GDALSource, TimeSnapYear3) {
    testSnap(TimeUnit::Year, 3, "2010-01-01T00:00:00", "2014-01-03T01:01:00", "2013-01-01T00:00:00");
}

TEST(GDALSource, TimeSnapYear3_2) {
    testSnap(TimeUnit::Year, 3, "2010-01-01T00:02:00", "2014-01-03T01:01:00", "2013-01-01T00:02:00");
}

TEST(GDALSource, TimeSnapDay1) {
    testSnap(TimeUnit::Day, 1, "2010-01-01T00:00:00", "2013-01-01T01:00:00", "2013-01-01T00:00:00");
}

TEST(GDALSource, TimeSnapDay1_2) {
    testSnap(TimeUnit::Day, 1, "2010-01-01T00:02:03", "2013-01-01T00:00:00", "2013-01-01T00:02:03");
}

TEST(GDALSource, TimeSnapDay16) {
    testSnap(TimeUnit::Day, 16, "2018-01-01T00:00:00", "2018-02-16T01:00:00", "2018-02-02T00:00:00");
}

TEST(GDALSource, TimeSnapHour1) {
    testSnap(TimeUnit::Hour, 1, "2010-01-01T00:00:00", "2013-01-01T01:12:00", "2013-01-01T01:00:00");
}

TEST(GDALSource, TimeSnapHour13) {
    testSnap(TimeUnit::Hour, 13, "2010-01-01T00:00:00", "2010-01-02T04:00:00", "2010-01-02T02:00:00");
}

TEST(GDALSource, TimeSnapHour13_2) {
    testSnap(TimeUnit::Hour, 13, "2010-01-01T00:00:01", "2010-01-02T01:00:02", "2010-01-01T13:00:01");
}

TEST(GDALSource, TimeSnapMinute1) {
    testSnap(TimeUnit::Minute, 1, "2010-01-01T00:00:00", "2013-01-01T01:12:00", "2013-01-01T01:12:00");
}

TEST(GDALSource, TimeSnapMinute1_2) {
    testSnap(TimeUnit::Minute, 1, "2010-01-01T00:00:03", "2013-01-01T01:12:05", "2013-01-01T01:12:03");
}

TEST(GDALSource, TimeSnapMinute15) {
    testSnap(TimeUnit::Minute, 15, "2010-01-01T00:00:00", "2013-01-01T01:16:00", "2013-01-01T01:15:00");
}

TEST(GDALSource, TimeSnapMinute31) {
    testSnap(TimeUnit::Minute, 31, "2010-01-01T00:00:00", "2010-01-01T01:01:00", "2010-01-01T00:31:00");
}

TEST(GDALSource, TimeSnapSecond1) {
    testSnap(TimeUnit::Second, 1, "2010-01-01T00:00:00", "2010-01-01T01:01:12", "2010-01-01T01:01:12");
}

TEST(GDALSource, TimeSnapSecond15) {
    testSnap(TimeUnit::Second, 15, "2010-01-01T00:00:00", "2010-01-01T01:01:12", "2010-01-01T01:01:00");
}

TEST(GDALSource, TimeSnapSecond31) {
    testSnap(TimeUnit::Second, 31, "2010-01-01T23:59:00", "2010-01-02T00:00:02", "2010-01-02T00:00:02");
}

TEST(GDALSource, TimeSnapSecond31_2) {
    testSnap(TimeUnit::Second, 31, "2010-01-01T23:59:00", "2010-01-02T00:00:01", "2010-01-01T23:59:31");
}