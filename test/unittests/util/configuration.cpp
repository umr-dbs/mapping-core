#include <gtest/gtest.h>
#include "util/configuration.h"


TEST(Configuration, mergeTest){
    Configuration::loadFromDefaultPaths();

    Configuration::loadFromString("test=123\ntest2=true\ntest3=1.56");
    Configuration::loadFromFile("../../test/unittests/test_config.toml");

    EXPECT_EQ(Configuration::get<int>("test"), 123);
    EXPECT_EQ(Configuration::get<bool>("test2"), 1);
    EXPECT_EQ(Configuration::get<double>("test3"), 1.56);
    EXPECT_EQ(Configuration::get<double>("notExists", 1.75), 1.75);

    //Variables from file :
    EXPECT_EQ(Configuration::get<bool>("someFlag"), false);
    //test subtable access:
    EXPECT_EQ(Configuration::get<std::string>("data.location"), "Some/Where");
    EXPECT_EQ(Configuration::get<int>("data.size"), 512);
}