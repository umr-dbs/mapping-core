
#include "services/ogcservice.h"
#include "operators/operator.h"

/*
 * This class serves provenance information of a query.
 *
 * Query pattern: mapping_url/?service=provenance
 *
 * Parameters:
 * - type: points/lines/polygons/raster
 * - query
 * - crs
 * - bbox
 * - time
 */
class ProvenanceService : public OGCService {
public:
	using OGCService::OGCService;
	virtual ~ProvenanceService() = default;

	virtual void run();
};

REGISTER_HTTP_SERVICE(ProvenanceService, "provenance");

void ProvenanceService::run() {
	auto session = UserDB::loadSession(params.get("sessiontoken"));
	auto user = session->getUser();

	std::string type = params.get("type", "");
	if(type != "points" && type != "lines" && type != "polygons" && type != "raster") {
		throw ArgumentException("ProvenanceService: invalid type");
	}

	std::string queryString = params.get("query", "");
	if(queryString == "")
		throw ArgumentException("ProvenanceService: no query specified");

	if(!params.hasParam("crs"))
		throw ArgumentException("ProvenanceService: crs not specified");

	CrsId queryEpsg = parseCrsId(params, "crs");

	SpatialReference sref(queryEpsg);
	if(params.hasParam("bbox")) {
		sref = parseBBOX(params.get("bbox"), queryEpsg);
	}

	TemporalReference tref = parseTime(params);

	QueryResolution qres = QueryResolution::none();

	if(params.hasParam("width") && params.hasParam("height")) {
		qres = QueryResolution::pixels(params.getInt("width"), params.getInt("height"));
	}
	QueryRectangle rect(
		sref,
		tref,
		qres
	);

	Query::ResultType resultType;
	if(type == "points") {
		resultType = Query::ResultType::POINTS;
	} else if(type == "lines") {
		resultType = Query::ResultType::LINES;
	} else if(type == "polygons") {
		resultType = Query::ResultType::POLYGONS;
	} else if(type == "raster") {
		resultType = Query::ResultType::RASTER;
	}

	Query query(queryString, resultType, rect);
	auto result = processQuery(query, session);

	response.sendContentType("application/json; charset=utf-8");
	response.finishHeaders();
	response << result->getProvenance().toJson();
}
