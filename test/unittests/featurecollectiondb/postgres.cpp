#include "featurecollectiondb/featurecollectiondb.h"
#include "util/configuration.h"
#include "util/exceptions.h"
#include "datatypes/simplefeaturecollections/wkbutil.h"
#include "unittests/simplefeaturecollections/util.h"

#include <gtest/gtest.h>
#include <vector>

static std::unique_ptr<PointCollection> createPointsWithAttributesAndTime(){
	std::string wkt = "GEOMETRYCOLLECTION(POINT(1 1), POINT(2 5), MULTIPOINT(8 6, 8 9, 88 99, 23 21), POINT(68 59), MULTIPOINT(42 6, 43 7))";
	auto points = WKBUtil::readPointCollection(wkt, SpatioTemporalReference::unreferenced());
	points->setTimeStamps({2, 4,  8, 16, 32}, {4, 8, 16, 32, 64});

	// TODO support global attributes
//	points->global_attributes.setTextual("info", "1234");
//	points->global_attributes.setNumeric("index", 42);

	points->feature_attributes.addNumericAttribute("value", Unit::unknown(), {0.0, 1.1, 2.2, 3.3, 4.4});
	points->feature_attributes.addTextualAttribute("label", Unit::unknown(), {"l0", "l1", "l2", "l3", "l4"});

	EXPECT_NO_THROW(points->validate());

	return points;
}


TEST(FeatureCollectionDB, testALL) {
	Configuration::loadFromDefaultPaths();
	FeatureCollectionDB::init("postgres", Configuration::get<std::string>("test.featurecollectiondb.postgres.dbcredentials"));

	UserDB::init("sqlite", ":memory:");

	// Create a user
	auto user = UserDB::createUser("testuser", "name", "email", "pass");

	QueryRectangle qrect (QueryRectangle::extent(CrsId::from_epsg_code(4326)), TemporalReference(timetype_t::TIMETYPE_UNIX), QueryResolution::none());

	auto points = createPointsWithAttributesAndTime();
	points->replaceSTRef(qrect);

	FeatureCollectionDB::DataSetMetaData dataset = FeatureCollectionDB::createPoints(*user, "test points", *points);


	auto loadedPoints = FeatureCollectionDB::loadPoints("testuser", "test points", qrect);

	CollectionTestUtil::checkEquality(*points, *loadedPoints);
}
