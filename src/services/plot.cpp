
#include "datatypes/plot.h"
#include "operators/operator.h"
#include "util/log.h"
#include "services/ogcservice.h"

/*
 * This class serves results of plot queries. Although it does not follow any OGC standard we make use of common
 * functionality and inherit from the OGCService class
 *
 * Query pattern: mapping_url/?service=plot&query={QUERY_STRING}&time={ISO_TIME}&bbox={x1,y1,x2,y2}&crs={authority:code}
 * For plots containing at least one raster source, additionally parameters width and height have to be specified
 */
class PlotService : public OGCService {
public:
	using OGCService::OGCService;
	virtual ~PlotService() = default;

	virtual void run();
};

REGISTER_HTTP_SERVICE(PlotService, "plot");

void PlotService::run() {
	auto session = UserDB::loadSession(params.get("sessiontoken"));
	auto user = session->getUser();
	Log::debug("User: " + user.getUserIDString());

	std::string queryString = params.get("query", "");
	if(queryString == "")
		throw ArgumentException("PlotService: no query specified");

	if(!params.hasParam("crs"))
		throw ArgumentException("PlotService: crs not specified");

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

	Query query(queryString, Query::ResultType::PLOT, rect);
	auto plot = processQuery(query, user)->getPlot();

	// TODO: check permissions

	response.sendContentType("application/json");
	response.finishHeaders();

	response << plot;
}
