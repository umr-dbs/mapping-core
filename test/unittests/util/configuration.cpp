
#include <gtest/gtest.h>
#include "util/configuration.h"

TEST(Configuration, arrays){
    Configuration::loadFromString("doubleArray=[1.0,2.3,5.7]\n[sub]\nintArray=[1,2,3,10,20]\nboolarray=[false, true, false]\nstringarray=[\"first\",\"scnd\"]");

    std::vector<double> third = Configuration::getVector<double>("doubleArray");
    EXPECT_EQ(third.size(), 3);
    EXPECT_EQ(third[0], 1.0);
    EXPECT_EQ(third[2], 5.7);

    std::vector<int> first = Configuration::getVector<int>("sub.intArray");
    EXPECT_EQ(first.size(), 5);
    EXPECT_EQ(first[0], 1);
    EXPECT_EQ(first[4], 20);

    std::vector<bool> second = Configuration::getVector<bool>("sub.boolarray");
    EXPECT_EQ(second.size(), 3);
    EXPECT_EQ(second[0], false);
    EXPECT_EQ(second[1], true);

    std::vector<std::string> fourth = Configuration::getVector<std::string>("sub.stringarray");

    EXPECT_EQ(fourth.size(), 2);
    EXPECT_EQ(fourth[0], "first");
    EXPECT_EQ(fourth[1], "scnd");

}

TEST(Configuration, mergeTest){

    Configuration::loadFromDefaultPaths();
    Configuration::loadFromString("test=123\ntest2=true\ntest3=1.56\ntestOverride=\"First\"\ntestSizet=1231231244234\n[test4]\nsubTest=true");
    Configuration::loadFromFile("../../test/unittests/util/settings-test.toml");

    EXPECT_EQ(Configuration::get<int>("test"), 123);
    EXPECT_EQ(Configuration::get<bool>("test2"), 1);
    EXPECT_EQ(Configuration::get<double>("test3"), 1.56);
    EXPECT_EQ(Configuration::get<bool>("test4.subTest"), true);
    EXPECT_EQ(Configuration::get<double>("notExists", 1.75), 1.75);
    EXPECT_EQ(Configuration::get<std::string>("testOverride"), "Second");


    size_t t1 = Configuration::get<size_t>("testSizet");
    size_t exp = 1231231244234;
    EXPECT_EQ(t1,exp);

    //Variables from file :
    EXPECT_EQ(Configuration::get<bool>("someFlag"), false);
    //test subtable access :
    EXPECT_EQ(Configuration::get<std::string>("data.location"), "Some/Where");
    EXPECT_EQ(Configuration::get<int>("data.size"), 512);
}
