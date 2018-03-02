#include <gtest/gtest.h>
#include "util/configuration.h"


TEST(Configuration, mergeTest){

    Configuration::loadFromDefaultPaths();
    Configuration::loadFromString("test=123\ntest2=true\ntest3=1.56\ntestSizet=1231231244234\n[test4]\nsubTest=true");
    Configuration::loadFromFile("../../test/unittests/util/test_config.toml");

    EXPECT_EQ(Configuration::get<int>("test"), 123);
    EXPECT_EQ(Configuration::get<bool>("test2"), 1);
    EXPECT_EQ(Configuration::get<double>("test3"), 1.56);
    EXPECT_EQ(Configuration::get<bool>("test4.subTest"), true);
    EXPECT_EQ(Configuration::get<double>("notExists", 1.75), 1.75);

    size_t t1 = Configuration::get<size_t>("testSizet");
    size_t exp = 1231231244234;
    EXPECT_EQ(t1,exp);

    //Variables from file :
    EXPECT_EQ(Configuration::get<bool>("someFlag"), false);
    //test subtable access :
    EXPECT_EQ(Configuration::get<std::string>("data.location"), "Some/Where");
    EXPECT_EQ(Configuration::get<int>("data.size"), 512);
}