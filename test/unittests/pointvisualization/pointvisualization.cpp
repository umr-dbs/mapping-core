#include <gtest/gtest.h>

#include "pointvisualization/CircleClusteringQuadTree.h"

TEST(CircleClusteringQuadTree, numeric_aggregate) {
    pv::Circle::CommonAttributes commonAttributes{5, 1};
    pv::Circle circle1{
            pv::Coordinate{0, 0},
            commonAttributes,
            {},
            {{"test_attribute", pv::Circle::NumericAttribute{10}}}
    };
    pv::Circle circle2{
            pv::Coordinate{10, 0},
            commonAttributes,
            {},
            {{"test_attribute", pv::Circle::NumericAttribute{20}}}
    };

    pv::Circle circleMerged = circle1.merge(circle2);

    EXPECT_FLOAT_EQ(15, circleMerged.getNumericAttributes().at("test_attribute").getAverage());
    EXPECT_FLOAT_EQ(25, circleMerged.getNumericAttributes().at("test_attribute").getVariance());
}

TEST(CircleClusteringQuadTree, text_aggregate) {
    pv::Circle::CommonAttributes commonAttributes{5, 1};
    pv::Circle circle1{
            pv::Coordinate{0, 0},
            commonAttributes,
            {{"test_attribute", pv::Circle::TextAttribute{"test1", pv::Coordinate{0, 0}, commonAttributes}}},
            {}
    };
    pv::Circle circle2{
            pv::Coordinate{10, 0},
            commonAttributes,
            {{"test_attribute", pv::Circle::TextAttribute{"test2", pv::Coordinate{10, 0}, commonAttributes}}},
            {}
    };

    pv::Circle circleMerged = circle1.merge(circle2);
    std::vector<std::string> texts = circleMerged.getTextAttributes().at("test_attribute").getTexts();
    std::sort(texts.begin(), texts.end());

    std::vector<std::string> textsExpected = {"test1", "test2"};

    EXPECT_EQ(textsExpected, texts);
}

TEST(CircleClusteringQuadTree, text_aggregate_large) {
    pv::Circle::CommonAttributes commonAttributes{5, 1};
    pv::Circle circle1{
            pv::Coordinate{0, 0},
            commonAttributes,
            {{"test_attribute", pv::Circle::TextAttribute{"test1", pv::Coordinate{0, 0}, commonAttributes}}},
            {}
    };
    pv::Circle circle2{
            pv::Coordinate{10, 0},
            commonAttributes,
            {{"test_attribute", pv::Circle::TextAttribute{"test2", pv::Coordinate{10, 0}, commonAttributes}}},
            {}
    };
    pv::Circle circle3{
            pv::Coordinate{20, 0},
            commonAttributes,
            {{"test_attribute", pv::Circle::TextAttribute{"test3", pv::Coordinate{20, 0}, commonAttributes}}},
            {}
    };
    pv::Circle circle4{
            pv::Coordinate{30, 0},
            commonAttributes,
            {{"test_attribute", pv::Circle::TextAttribute{"test4", pv::Coordinate{30, 0}, commonAttributes}}},
            {}
    };
    pv::Circle circle5{
            pv::Coordinate{40, 0},
            commonAttributes,
            {{"test_attribute", pv::Circle::TextAttribute{"test5", pv::Coordinate{40, 0}, commonAttributes}}},
            {}
    };
    pv::Circle circle6{
            pv::Coordinate{60, 0},
            commonAttributes,
            {{"test_attribute", pv::Circle::TextAttribute{"test6", pv::Coordinate{60, 0}, commonAttributes}}},
            {}
    };

    pv::Circle circleMerged = circle1.merge(circle2).merge(circle3).merge(circle4).merge(circle5).merge(circle6);
    std::vector<std::string> texts = circleMerged.getTextAttributes().at("test_attribute").getTexts();
    std::sort(texts.begin(), texts.end());

    std::vector<std::string> textsExpected = {"test1", "test2", "test3", "test4", "test5"};

    EXPECT_EQ(textsExpected, texts);
}