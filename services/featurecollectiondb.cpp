
#include "datatypes/plot.h"
#include "operators/operator.h"

#include "services/ogcservice.h"
#include "featurecollectiondb/featurecollectiondb.h"

/*
 * This service allow storing the result of a query in the featurecollectiondb
 *
 * Operations:
 * - request = save: save the results of a query in the db
 *   - parameters
 *     - query: the query string
 *     - time_start
 *     - time_stop
 *     - bbox
 *     - crs
 *     - type: points, lines, polyogons
 *     - name: the name of the data set
 * - request = list: list all data sets of the current user
 *
 */
class FeatureCollectionDBService : public OGCService {
public:
	using OGCService::OGCService;
	virtual ~FeatureCollectionDBService() = default;

	virtual void run();

private:
	Json::Value metaDataToJson(const FeatureCollectionDB::DataSetMetaData& metaData);
};

REGISTER_HTTP_SERVICE(FeatureCollectionDBService, "featurecollectiondb");


Json::Value FeatureCollectionDBService::metaDataToJson(const FeatureCollectionDB::DataSetMetaData& metaData) {
	Json::Value json(Json::objectValue);

	json["id"] = metaData.dataSetId;
	json["name"] = metaData.dataSetName;
	json["has_time"] = metaData.hasTime;

	std::string typeString;
	if(metaData.resultType == Query::ResultType::POINTS) {
		typeString = "points";
	} else if(metaData.resultType == Query::ResultType::LINES) {
		typeString = "lines";
	} else if(metaData.resultType == Query::ResultType::POLYGONS) {
		typeString = "polygons";
	}
	json["type"] = typeString;

	Json::Value numeric_attributes(Json::arrayValue);
	for(auto& attribute : metaData.numeric_attributes) {
		numeric_attributes.append(attribute.first);
	}
	json["numeric_attributes"] = numeric_attributes;

	Json::Value textual_attributes(Json::arrayValue);
	for(auto& attribute : metaData.textual_attributes) {
		textual_attributes.append(attribute.first);
	}
	json["textual_attributes"] = textual_attributes;

	return json;
}

void FeatureCollectionDBService::run() {
	auto session = UserDB::loadSession(params.get("sessiontoken"));

	try {
	std::string request = params.get("request");

	if(request == "save") {
		std::string query = params.get("query", "");
		if(query == "")
			throw ArgumentException("FeatureCollectionDBService: no query specified");

		if(!params.hasParam("crs"))
			throw ArgumentException("FeatureCollectionDBService: crs not specified");

		if(!params.hasParam("name"))
			throw ArgumentException("FeatureCollectionDBService: name of data set not specified");
		std::string name = params.get("name");

		epsg_t queryEpsg = parseEPSG(params, "crs");

		if(queryEpsg != epsg_t::EPSG_LATLON) {
			throw ArgumentException("FeatureCollectionDBService: only WGS84 supported");
		}

		SpatialReference sref(queryEpsg);
		if(params.hasParam("bbox")) {
			sref = parseBBOX(params.get("bbox"), queryEpsg);
		}

		TemporalReference tref = parseTime(params);

		auto graph = GenericOperator::fromJSON(query);

		QueryProfiler profiler;

		QueryResolution qres = QueryResolution::none();

		QueryRectangle rect(
			sref,
			tref,
			qres
		);

		std::string type = params.get("type", "");
		FeatureCollectionDB::DataSetMetaData metaData;
		if(type == "points") {
			auto points = graph->getCachedPointCollection(rect, QueryTools(profiler));
			metaData = FeatureCollectionDB::createPoints(session->getUser(), name, *points);
		} else if(type == "lines") {
			auto lines = graph->getCachedLineCollection(rect, QueryTools(profiler));
			metaData = FeatureCollectionDB::createLines(session->getUser(), name, *lines);
		} else if(type == "polygons") {
			auto polygons = graph->getCachedPolygonCollection(rect, QueryTools(profiler));
			metaData = FeatureCollectionDB::createPolygons(session->getUser(), name, *polygons);
		} else {
			throw ArgumentException("FeatureCollectionDBService: invalid type");
		}

		// TODO: set permissions in userdb

		Json::Value json = metaDataToJson(metaData);
		response.sendSuccessJSON(json);

	} else if(request == "list") {
		std::vector<FeatureCollectionDB::DataSetMetaData> dataSets = FeatureCollectionDB::loadDataSets(session->getUser());

		Json::Value json(Json::objectValue);
		Json::Value array(Json::arrayValue);

		for(auto& dataSet : dataSets) {
			array.append(metaDataToJson(dataSet));
		}
		json["data_sets"] = array;

		response.sendSuccessJSON(json);
	} else {
		throw ArgumentException("FeatureCollectionDBService: invalid request");
	}
	} catch (const std::exception &e) {
		response.send500(e.what());
	}
}
