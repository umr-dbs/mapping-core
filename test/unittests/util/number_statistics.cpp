#include <gtest/gtest.h>
#include <util/NumberStatistics.h>

TEST(NumberStatistics, example_data) {
    NumberStatistics number_statistics;
    for (double v : {2, 4, 4, 4, 5, 5, 7, 9}) {
        number_statistics.add(v);
    }

    EXPECT_EQ(number_statistics.count(), 8);
    EXPECT_EQ(number_statistics.nan_count(), 0);
    EXPECT_FLOAT_EQ(number_statistics.min(), 2);
    EXPECT_FLOAT_EQ(number_statistics.max(), 9);
    EXPECT_FLOAT_EQ(number_statistics.mean(), 5);
    EXPECT_FLOAT_EQ(number_statistics.var(), 4);
    EXPECT_FLOAT_EQ(number_statistics.std_dev(), 2);
    EXPECT_FLOAT_EQ(number_statistics.sample_std_dev(), 2.1380899353);
}

TEST(NumberStatistics, nan_data) {
    NumberStatistics number_statistics;
    for (double v : {1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::signaling_NaN()}) {
        number_statistics.add(v);
    }

    EXPECT_EQ(number_statistics.count(), 1);
    EXPECT_EQ(number_statistics.nan_count(), 2);
}
