#include "operators/operator.h"
#include "datatypes/simplefeaturecollections/wkbutil.h"
#include "util/exceptions.h"
#include "featurecollectiondb/featurecollectiondb.h"

#include <json/json.h>

/**
 * Operator that reads a collection from the FeatureCollectionDB
 *
 * Parameters:
 * - owner: the user name of the owner of the data set
 * - data_set_name: the name of the data set
 */
class FeatureCollectionDBSourceOperator : public GenericOperator {
	public:
		FeatureCollectionDBSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
			assumeSources(0);

			if(!FeatureCollectionDB::isAvailable()){
				throw SourceException("FeatureCollectionDB is not available.");
			}

			owner = params.get("owner", "").asString();
			dataSetName = params.get("data_set_name", "").asString();
		}

		void writeSemanticParameters(std::ostringstream& stream) {
			Json::Value json;
			json["owner"] = owner;
			json["data_set_name"] = dataSetName;

			Json::FastWriter writer;
			stream << writer.write(json);
		}

		virtual ~FeatureCollectionDBSourceOperator(){};

		#ifndef MAPPING_OPERATOR_STUBS
		virtual std::unique_ptr<PointCollection> getPointCollection(const QueryRectangle &rect, const QueryTools &tools);
		virtual std::unique_ptr<LineCollection> getLineCollection(const QueryRectangle &rect, const QueryTools &tools);
		virtual std::unique_ptr<PolygonCollection> getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools);
		virtual void getProvenance(ProvenanceCollection &pc);
		#endif

	private:
		std::string owner;
		std::string dataSetName;
};
REGISTER_OPERATOR(FeatureCollectionDBSourceOperator, "featurecollectiondb_source");

#ifndef MAPPING_OPERATOR_STUBS

std::unique_ptr<PointCollection> FeatureCollectionDBSourceOperator::getPointCollection(const QueryRectangle &rect, const QueryTools &tools){
	return FeatureCollectionDB::loadPoints(owner, dataSetName, rect);
}

std::unique_ptr<LineCollection> FeatureCollectionDBSourceOperator::getLineCollection(const QueryRectangle &rect, const QueryTools &tools){
	return FeatureCollectionDB::loadLines(owner, dataSetName, rect);
}

std::unique_ptr<PolygonCollection> FeatureCollectionDBSourceOperator::getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools){
	return FeatureCollectionDB::loadPolygons(owner, dataSetName, rect);
}

void FeatureCollectionDBSourceOperator::getProvenance(ProvenanceCollection &pc) {
	std::string local_identifier = concat("data.", getType(), ".", FeatureCollectionDB::loadDataSet(owner, dataSetName).dataSetId);

	// TODO: load provenance

	pc.add(Provenance("", "", "", local_identifier));
}


#endif



