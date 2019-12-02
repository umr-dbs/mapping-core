
#include "featurecollectiondb/featurecollectiondb.h"
#include "util/configuration.h"
#include "util/exceptions.h"
#include "datatypes/simplefeaturecollections/wkbutil.h"

#include <gtest/gtest.h>
#include <vector>

static std::unique_ptr<PointCollection> createPointsWithAttributesAndTime(){
    std::string wkt = "GEOMETRYCOLLECTION(POINT(1 1), POINT(2 5), POINT(8 6), POINT(68 59), POINT(42 6))";
    auto points = WKBUtil::readPointCollection(wkt, SpatioTemporalReference::unreferenced());
    points->setTimeStamps({2, 4, 8, 16, 32}, {4, 8, 16, 32, 64});

    points->feature_attributes.addNumericAttribute("value", Unit::unknown(), {0.0, 1.1, 2.2, 3.3, 4.4});
    points->feature_attributes.addTextualAttribute("label", Unit::unknown(), {"l0", "l1", "l2", "l3", "l4"});

    EXPECT_NO_THROW(points->validate());

    return points;
}


TEST(FeatureCollectionDB1, testALL) {
    Configuration::loadFromDefaultPaths();
    FeatureCollectionDB::init("chronicledb", "http://localhost:8080");

    UserDB::init("sqlite", ":memory:");

    // Create a user
    auto user = UserDB::createUser("testuser", "name", "email", "pass");

    QueryRectangle qrect (QueryRectangle::extent(CrsId::from_epsg_code(4326)), TemporalReference(timetype_t::TIMETYPE_UNIX), QueryResolution::none());

    auto points = createPointsWithAttributesAndTime();
    points->replaceSTRef(qrect);

    FeatureCollectionDB::DataSetMetaData dataset = FeatureCollectionDB::createPoints(*user, "testData", *points);

}
