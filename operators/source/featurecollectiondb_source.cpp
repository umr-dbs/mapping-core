#include "operators/operator.h"
#include "datatypes/simplefeaturecollections/wkbutil.h"
#include "util/exceptions.h"
#include "featurecollectiondb/featurecollectiondb.h"

#include <json/json.h>

/**
 * Operator that reads a collection from the FeatureCollectionDB
 *
 * Parameters:
 * - id: the id of the data set
 */
class FeatureCollectionDBSourceOperator : public GenericOperator {
	public:
		FeatureCollectionDBSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
			assumeSources(0);
			id = params.get("id", "").asInt64();
		}

		void writeSemanticParameters(std::ostringstream& stream) {
			Json::Value json;
			json["id"] = id;

			Json::FastWriter writer;
			stream << writer.write(json);
		}

		virtual ~FeatureCollectionDBSourceOperator(){};

		#ifndef MAPPING_OPERATOR_STUBS
		virtual std::unique_ptr<PointCollection> getPointCollection(const QueryRectangle &rect, const QueryTools &tools);
		virtual std::unique_ptr<LineCollection> getLineCollection(const QueryRectangle &rect, const QueryTools &tools);
		virtual std::unique_ptr<PolygonCollection> getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools);
		#endif

	private:
		size_t id;
};
REGISTER_OPERATOR(FeatureCollectionDBSourceOperator, "featurecollectiondb_source");

#ifndef MAPPING_OPERATOR_STUBS

std::unique_ptr<PointCollection> FeatureCollectionDBSourceOperator::getPointCollection(const QueryRectangle &rect, const QueryTools &tools){
	return FeatureCollectionDB::loadPoints(id, rect);
}

std::unique_ptr<LineCollection> FeatureCollectionDBSourceOperator::getLineCollection(const QueryRectangle &rect, const QueryTools &tools){
	return FeatureCollectionDB::loadLines(id, rect);
}

std::unique_ptr<PolygonCollection> FeatureCollectionDBSourceOperator::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools){
	return FeatureCollectionDB::loadPolygons(id, rect);
}


#endif



