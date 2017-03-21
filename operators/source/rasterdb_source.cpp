
#include "datatypes/raster.h"
#include "datatypes/raster/typejuggling.h"
#include "rasterdb/rasterdb.h"
#include "raster/opencl.h"
#include "operators/operator.h"
#include "util/configuration.h"

#include <memory>
#include <sstream>
#include <cmath>
#include <json/json.h>
#include <ctime>


/**
 * Operator that gets raster data from the rasterdb
 *
 * Parameters:
 * - sourcename: the name of the datasource
 * - channel: the index of the requested channel
 * - transform: true, if the specified transform function in the datasource should be applied
 */
class RasterDBSourceOperator : public GenericOperator {
	public:
		RasterDBSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);
		virtual ~RasterDBSourceOperator();

#ifndef MAPPING_OPERATOR_STUBS
		virtual void getProvenance(ProvenanceCollection &pc);
		virtual std::unique_ptr<GenericRaster> getRaster(const QueryRectangle &rect, const QueryTools &tools);
		virtual std::unique_ptr<GenericRaster> getCachedRaster(const QueryRectangle &rect, const QueryTools &tools, RasterQM query_mode);
#endif
	protected:
		void writeSemanticParameters(std::ostringstream &stream);
	private:
#ifndef MAPPING_OPERATOR_STUBS
		std::shared_ptr<RasterDB> rasterdb;
#endif
		std::string sourcename;
		int channel;
		bool transform;
};


// RasterSource Operator
RasterDBSourceOperator::RasterDBSourceOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
	assumeSources(0);
	sourcename = params.get("sourcename", "").asString();
	if (sourcename.length() == 0)
		throw OperatorException("SourceOperator: missing sourcename");

#ifndef MAPPING_OPERATOR_STUBS
	rasterdb = RasterDB::open(sourcename.c_str(), RasterDB::READ_ONLY);
#endif
	channel = params.get("channel", 0).asInt();
	transform = params.get("transform", true).asBool();
}

RasterDBSourceOperator::~RasterDBSourceOperator() {
}

REGISTER_OPERATOR(RasterDBSourceOperator, "rasterdb_source");


#ifndef MAPPING_OPERATOR_STUBS
void RasterDBSourceOperator::getProvenance(ProvenanceCollection &pc) {
	std::string local_identifier = "data." + getType() + "." + sourcename;

	auto sp = rasterdb->getProvenance();
	if (sp == nullptr)
		pc.add(Provenance("", "", "", local_identifier));
	else
		pc.add(Provenance(sp->citation, sp->license, sp->uri, local_identifier));
}

std::unique_ptr<GenericRaster> RasterDBSourceOperator::getRaster(const QueryRectangle &rect, const QueryTools &tools) {
	return rasterdb->query(rect, tools.profiler, channel, transform);
}
#endif

void RasterDBSourceOperator::writeSemanticParameters(std::ostringstream &stream) {
	std::string trans = transform ? "true" : "false";
	stream << "{\"sourcename\": \"" << sourcename << "\",";
	stream << " \"channel\": " << channel << ",";
	stream << " \"transform\": " << trans << "}";
}

std::unique_ptr<GenericRaster> RasterDBSourceOperator::getCachedRaster(const QueryRectangle &rect, const QueryTools &tools, RasterQM query_mode) {
	// this is an ugly hack to avoid loading the 12 worldclim raster hundreds of times for long range computations
	// it maps all requests to year 1970. as the rasters validity is periodical this does not change the result
	if(sourcename == "worldclim") {

		// change year to 1970
		QueryRectangle fakeQrect(rect);
		fakeQrect.epsg = rect.epsg;

		time_t time = rect.t1;
		std::tm tm;
		gmtime_r(&time, &tm);

		int originalYear = tm.tm_year;
		tm.tm_year = 70;

		time_t fakeTime = timegm(&tm);

		fakeQrect.t1 = fakeTime + (rect.t1 - time);
		fakeQrect.t2 = fakeQrect.t1 + fakeQrect.epsilon();

		auto result = GenericOperator::getCachedRaster(fakeQrect, tools, query_mode);
		SpatioTemporalReference stref(result->stref);
		stref.epsg = result->stref.epsg;

		// change year back
		time = result->stref.t1;
		gmtime_r(&time, &tm);
		tm.tm_year = originalYear;
		time = timegm(&tm);
		stref.t1 = time;

		time = result->stref.t2;
		gmtime_r(&time, &tm);
		tm.tm_year = originalYear;
		time = timegm(&tm);
		stref.t2 = time;

		result->replaceSTRef(stref);

		return result;
	} else {
		return GenericOperator::getCachedRaster(rect, tools, query_mode);
	}
}
