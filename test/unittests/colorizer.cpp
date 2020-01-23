#include <gtest/gtest.h>
#include <datatypes/colorizer.h>
#include <cmath>

TEST(Colorizer, logarithmic) {
    const Colorizer::ColorTable color_table = {
            Colorizer::Breakpoint(1, color_from_rgba(0, 0, 0)),
            Colorizer::Breakpoint(10, color_from_rgba(255, 0, 0)),
    };
    const Colorizer colorizer (color_table, Colorizer::Interpolation::LOGARITHMIC);

    std::vector<color_t> colors (3);
    colorizer.fillPalette(colors.data(), 3, 1, 10);

    EXPECT_EQ(r_from_color(colors[0]), 0); // 1
    EXPECT_EQ(r_from_color(colors[1]), (uint8_t) round(0.7403626894942438 * 255)); // 5.5 -> 189
    EXPECT_EQ(r_from_color(colors[2]), 255); // 10
}

TEST(Colorizer, logarithmic2) {
    const Colorizer::ColorTable color_table = {
            Colorizer::Breakpoint(1, color_from_rgba(0, 0, 0)),
            Colorizer::Breakpoint(51, color_from_rgba(100, 0, 0)),
            Colorizer::Breakpoint(101, color_from_rgba(255, 0, 0)),
    };
    const Colorizer colorizer (color_table, Colorizer::Interpolation::LOGARITHMIC);

    std::vector<color_t> colors (5);
    colorizer.fillPalette(colors.data(), 5, 1, 101);

    EXPECT_EQ(r_from_color(colors[0]), 0); // 1
    EXPECT_EQ(r_from_color(colors[1]), (uint8_t) round(0.8286472601695658 * 100)); // 26
    EXPECT_EQ(r_from_color(colors[2]), 100); // 51
    EXPECT_EQ(r_from_color(colors[3]), (uint8_t) round(0.5838002256925127 * 255 + (1-0.5838002256925127) * 100)); // 76
    EXPECT_EQ(r_from_color(colors[4]), 255); // 101
}
