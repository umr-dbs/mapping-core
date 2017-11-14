#include "util/gdal_timesnap.h"

#include <gtest/gtest.h>

static void testSnap(TimeUnit timeUnit, int timeInterval, int startUnix, int wantedUnix, int expectedSnap) {
    tm start{};
    tm wanted{};

    time_t startTime = startUnix;
    time_t wantedTime = wantedUnix;

    gmtime_r(&startTime, &start);
    gmtime_r(&wantedTime, &wanted);

    tm snapped = GDALTimesnap::snapToInterval(timeUnit, timeInterval, start, wanted);

    time_t snappedUnix = timegm(&snapped);

    ASSERT_EQ(expectedSnap, snappedUnix);
}

TEST(GDALSource, TimeSnap) {
    testSnap(TimeUnit::Month, 1, 946684800, 941414400, 941414400);
}

TEST(GDALSource, TimeSnap2) {
    testSnap(TimeUnit::Month, 1, 946684800, 944006400, 944006400);
}

TEST(GDALSource, TimeSnap3) {
    testSnap(TimeUnit::Month, 1, 946684800, 973036800, 973036800);
}