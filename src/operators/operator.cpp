
#include "datatypes/raster.h"
#include "datatypes/plot.h"
#include "util/binarystream.h"
#include "util/sizeutil.h"
#include "util/log.h"

#include "operators/operator.h"
#include "cache/manager.h"


#include <unordered_map>
#include <chrono>


#include <json/json.h>
#include "datatypes/linecollection.h"
#include "datatypes/pointcollection.h"
#include "datatypes/polygoncollection.h"


// The magic of type registration, see REGISTER_OPERATOR in operator.h
typedef std::unique_ptr<GenericOperator> (*OPConstructor)(int sourcecounts[], GenericOperator *sources[], Json::Value &params);

static std::unordered_map< std::string, OPConstructor > *getRegisteredConstructorsMap() {
	static std::unordered_map< std::string, OPConstructor > registered_constructors;
	return &registered_constructors;
}

OperatorRegistration::OperatorRegistration(const char *name, OPConstructor constructor) {
	auto map = getRegisteredConstructorsMap();
	(*map)[std::string(name)] = constructor;
/*
	printf("Adding constructor for '%s'\n", name);
	for (auto i : *map) {
		printf(" contains %s\n", i.first.c_str());
	}
*/
}


/*
 * GenericOperator class
 */
GenericOperator::GenericOperator(int _sourcecounts[], GenericOperator *_sources[]) : type(), semantic_id(), depth(0) {
	for (int i=0;i<MAX_INPUT_TYPES;i++)
		sourcecounts[i] = _sourcecounts[i];

	for (int i=0;i<MAX_SOURCES;i++) {
		sources[i] = _sources[i];
		// we take ownership, so make sure that GenericOperator::fromJSON() doesn't try to free it in case of exceptions
		_sources[i] = nullptr;
	}
}


GenericOperator::~GenericOperator() {
	for (int i=0;i<MAX_SOURCES;i++) {
		delete sources[i];
		sources[i] = nullptr;
	}
}

void GenericOperator::writeSemanticParameters(std::ostringstream &stream) {
	stream << "{}";
}

void GenericOperator::assumeSources(int rasters, int pointcollections, int linecollections, int polygoncollections) {
	return;
	if (rasters >= 0 && sourcecounts[0] != rasters)
		throw OperatorException("Wrong amount of raster sources");
	if (pointcollections >= 0 && sourcecounts[1] !=pointcollections)
		throw OperatorException("Wrong amount of pointcollection sources");
	if (linecollections >= 0 && sourcecounts[2] != linecollections)
			throw OperatorException("Wrong amount of linecollection sources");
	if (polygoncollections >= 0 && sourcecounts[3] != polygoncollections)
		throw OperatorException("Wrong amount of polygoncollection sources");
}


void GenericOperator::validateQRect(const QueryRectangle &rect, GenericOperator::ResolutionRequirement res) {
	if (res == ResolutionRequirement::REQUIRED && rect.restype == QueryResolution::Type::NONE)
		throw OperatorException("Cannot query a raster without specifying a desired resolution");
	else if (res == ResolutionRequirement::FORBIDDEN && rect.restype != QueryResolution::Type::NONE)
		throw OperatorException("Cannot query a feature collection when specifying a desired resolution");

	if (rect.crsId == CrsId::unreferenced())
		throw OperatorException("Cannot query with EPSG_UNREFERENCED");
	if (rect.timetype == TIMETYPE_UNREFERENCED)
		throw OperatorException("Cannot query with TIMETYPE_UNREFERENCED");
}

void GenericOperator::validateResult(const QueryRectangle &rect, SpatioTemporalResult *result) {
	if (result->stref.crsId == CrsId::unreferenced())
		throw OperatorException(concat("Operator ", type, " returned result with EPSG_UNREFERENCED"));
	if (result->stref.timetype == TIMETYPE_UNREFERENCED)
		throw OperatorException(concat("Operator ", type, " returned result with TIMETYPE_UNREFERENCED"));

	if (!result->stref.SpatialReference::contains((SpatialReference) rect) || !result->stref.TemporalReference::contains((TemporalReference) rect))
		throw OperatorException(concat("Operator ", type, " returned a result which did not contain the given query rectangle. \nQuery: ",
				CacheCommon::qr_to_string(rect), "\nResult: ", CacheCommon::stref_to_string(result->stref)));
}



std::unique_ptr<GenericRaster> GenericOperator::getRaster(const QueryRectangle &, const QueryTools &) {
	throw OperatorException("getRaster() called on an operator that doesn't return rasters");
}
std::unique_ptr<PointCollection> GenericOperator::getPointCollection(const QueryRectangle &, const QueryTools &) {
	throw OperatorException("getPointCollection() called on an operator that doesn't return points");
}
std::unique_ptr<LineCollection> GenericOperator::getLineCollection(const QueryRectangle &, const QueryTools &) {
	throw OperatorException("getLineCollection() called on an operator that doesn't return lines");
}
std::unique_ptr<PolygonCollection> GenericOperator::getPolygonCollection(const QueryRectangle &, const QueryTools &) {
	throw OperatorException("getPolygonCollection() called on an operator that doesn't return polygons");
}
std::unique_ptr<GenericPlot> GenericOperator::getPlot(const QueryRectangle &, const QueryTools &) {
	throw OperatorException("getPlot() called on an operator that doesn't return data vectors");
}

void GenericOperator::getProvenance(ProvenanceCollection &pc) {

}

static void d_profile(int depth, const std::string &type, const char *result, QueryProfiler &profiler, size_t bytes = 0) {
	std::ostringstream msg;
	msg.precision(4);
	for (int i=0;i<depth;i++)
		msg << " ";
	msg << std::fixed << "OP " << type << " " << result
		<< " CPU: " << profiler.self_cpu << "/" << profiler.all_cpu
		<< " GPU: " << profiler.self_gpu << "/" << profiler.all_gpu
		<< " I/O: " << profiler.self_io << "/" << profiler.all_io;
	if (bytes > 0) {
		// Estimate the costs to cache this item
		double cache_cpu = 0.000000005 * bytes;
		size_t cache_io = bytes;
		msg << "  Caching CPU: " << cache_cpu << " I/O: " << cache_io;
		if (2 * cache_cpu < (profiler.all_cpu + profiler.all_gpu) || 2 * cache_io < profiler.all_io)
			msg << " CACHE CANDIDATE";
	}
	Log::info(msg.str());
}

std::unique_ptr<GenericRaster> GenericOperator::getCachedRaster(const QueryRectangle &rect, const QueryTools &tools, RasterQM query_mode) {
	QueryProfiler &parent_profiler = tools.profiler;
	QueryProfilerSimpleGuard parent_guard(parent_profiler);

	validateQRect(rect, ResolutionRequirement::REQUIRED);
	auto &cache = CacheManager::get_instance().get_raster_cache();
	std::unique_ptr<GenericRaster> result;

	try {
		result = cache.query( *this, rect, parent_profiler );
	} catch ( NoSuchElementException &nse ) {
		QueryProfilerStoppingGuard stop_guard(parent_profiler);
		QueryProfiler exec_profiler;
		{
			QueryProfilerRunningGuard guard(parent_profiler, exec_profiler);
			TIME_EXEC("Operator.getRaster");
			result = getRaster(rect,QueryTools(exec_profiler, tools.session));
		}
		d_profile(depth, type, "raster", exec_profiler);
		if ( cache.put(semantic_id,result,rect,exec_profiler) ) {
			parent_profiler.cached(exec_profiler);
		}
	}
	validateResult(rect, result.get());

	if (query_mode == RasterQM::EXACT)
		result = result->fitToQueryRectangle(rect);
	return result;
}

std::unique_ptr<PointCollection> GenericOperator::getCachedPointCollection(const QueryRectangle &rect, const QueryTools &tools, FeatureCollectionQM query_mode) {
	QueryProfiler &parent_profiler = tools.profiler;
	QueryProfilerSimpleGuard parent_guard(parent_profiler);

	validateQRect(rect, ResolutionRequirement::FORBIDDEN);
	auto &cache = CacheManager::get_instance().get_point_cache();
	std::unique_ptr<PointCollection> result;
	try {
		result = cache.query( *this, rect, parent_profiler );
		if (rect.t1 > result->stref.t1 || rect.t2 < result->stref.t2
				|| rect.x1 > result->stref.x1 || rect.x2 < result->stref.x2
				|| rect.y1 > result->stref.y1 || rect.y2 < result->stref.y2) {
			result->filterBySpatioTemporalReferenceIntersectionInPlace(rect);
		}
	} catch ( NoSuchElementException &nse ) {
		QueryProfilerStoppingGuard stop_guard(parent_profiler);
		QueryProfiler exec_profiler;
		{
			QueryProfilerRunningGuard guard(parent_profiler, exec_profiler);
			TIME_EXEC("Operator.getPointCollection");
			result = getPointCollection(rect,QueryTools(exec_profiler, tools.session));
		}
		d_profile(depth, type, "points", exec_profiler);
		if ( cache.put(semantic_id,result,rect,exec_profiler) )
			parent_profiler.cached(exec_profiler);
	}
	// validate the SimpleFeature data structure
	result->validate();
	// validate the invariants of the operator graph
	validateResult(rect, result.get());

	if (query_mode == FeatureCollectionQM::SINGLE_ELEMENT_FEATURES && !result->isSimple())
		throw OperatorException("Operator did not return Features consisting only of single points");
	return result;
}
std::unique_ptr<LineCollection> GenericOperator::getCachedLineCollection(const QueryRectangle &rect, const QueryTools &tools, FeatureCollectionQM query_mode) {
	QueryProfiler &parent_profiler = tools.profiler;
	QueryProfilerSimpleGuard parent_guard(parent_profiler);

	validateQRect(rect, ResolutionRequirement::FORBIDDEN);
	auto &cache = CacheManager::get_instance().get_line_cache();
	std::unique_ptr<LineCollection> result;
	try {
		result = cache.query( *this, rect, parent_profiler );
		if (rect.t1 > result->stref.t1 || rect.t2 < result->stref.t2
				|| rect.x1 > result->stref.x1 || rect.x2 < result->stref.x2
				|| rect.y1 > result->stref.y1 || rect.y2 < result->stref.y2) {
			result->filterBySpatioTemporalReferenceIntersectionInPlace(rect);
		}
	} catch ( NoSuchElementException &nse ) {
		QueryProfilerStoppingGuard stop_guard(parent_profiler);
		QueryProfiler exec_profiler;
		{
			QueryProfilerRunningGuard guard(parent_profiler, exec_profiler);
			TIME_EXEC("Operator.getLineCollection");
			result = getLineCollection(rect,QueryTools(exec_profiler, tools.session));
		}
		d_profile(depth, type, "lines", exec_profiler);
		if ( cache.put(semantic_id,result,rect,exec_profiler) )
				parent_profiler.cached(exec_profiler);
	}
	// validate the SimpleFeature data structure
	result->validate();
	// validate the invariants of the operator graph
	validateResult(rect, result.get());

	if (query_mode == FeatureCollectionQM::SINGLE_ELEMENT_FEATURES && !result->isSimple())
		throw OperatorException("Operator did not return Features consisting only of single lines");
	return result;
}
std::unique_ptr<PolygonCollection> GenericOperator::getCachedPolygonCollection(const QueryRectangle &rect, const QueryTools &tools, FeatureCollectionQM query_mode) {
	QueryProfiler &parent_profiler = tools.profiler;
	QueryProfilerSimpleGuard parent_guard(parent_profiler);

	validateQRect(rect, ResolutionRequirement::FORBIDDEN);
	auto &cache = CacheManager::get_instance().get_polygon_cache();
	std::unique_ptr<PolygonCollection> result;
	try {
		result = cache.query( *this, rect, parent_profiler );
		if (rect.t1 > result->stref.t1 || rect.t2 < result->stref.t2
				|| rect.x1 > result->stref.x1 || rect.x2 < result->stref.x2
				|| rect.y1 > result->stref.y1 || rect.y2 < result->stref.y2) {
			result->filterBySpatioTemporalReferenceIntersectionInPlace(rect);
		}
	} catch ( NoSuchElementException &nse ) {
		QueryProfilerStoppingGuard stop_guard(parent_profiler);
		QueryProfiler exec_profiler;
		{
			QueryProfilerRunningGuard guard(parent_profiler, exec_profiler);
			TIME_EXEC("Operator.getPolygonCollection");
			result = getPolygonCollection(rect,QueryTools(exec_profiler, tools.session));
		}
		d_profile(depth, type, "polygon", exec_profiler);
		if ( cache.put(semantic_id,result,rect,exec_profiler) )
			parent_profiler.cached(exec_profiler);
	}
	// validate the SimpleFeature data structure
	result->validate();
	// validate the invariants of the operator graph
	validateResult(rect, result.get());

	if (query_mode == FeatureCollectionQM::SINGLE_ELEMENT_FEATURES && !result->isSimple())
		throw OperatorException("Operator did not return Features consisting only of single polygons");
	return result;
}
std::unique_ptr<GenericPlot> GenericOperator::getCachedPlot(const QueryRectangle &rect, const QueryTools &tools) {
	QueryProfiler &parent_profiler = tools.profiler;
	QueryProfilerSimpleGuard parent_guard(parent_profiler);

	//	TODO: do we want plots to allow resolutions?
	validateQRect(rect, ResolutionRequirement::OPTIONAL);
	auto &cache = CacheManager::get_instance().get_plot_cache();
	std::unique_ptr<GenericPlot> result;
	try {
		result = cache.query( *this, rect, parent_profiler );
	} catch ( NoSuchElementException &nse ) {
		QueryProfilerStoppingGuard stop_guard(parent_profiler);
		QueryProfiler exec_profiler;
		{
			QueryProfilerRunningGuard guard(parent_profiler, exec_profiler);
			TIME_EXEC("Operator.getPlot");
			result = getPlot(rect,QueryTools(exec_profiler, tools.session));
		}
		d_profile(depth, type, "plot", exec_profiler);
		if ( cache.put(semantic_id,result,rect,exec_profiler) )
			parent_profiler.cached(exec_profiler);
	}
	return result;
}

void GenericOperator::getRecursiveProvenance(ProvenanceCollection &pc) {
	for (int i=0;i<MAX_SOURCES;i++) {
		if (sources[i])
			sources[i]->getRecursiveProvenance(pc);
	}
	getProvenance(pc);
}

std::unique_ptr<ProvenanceCollection> GenericOperator::getFullProvenance() {
	auto provenance = std::make_unique<ProvenanceCollection>();
	getRecursiveProvenance(*provenance);
	return provenance;
}

std::unique_ptr<ProvenanceCollection> GenericOperator::getCachedFullProvenance(const QueryRectangle &rect, const QueryTools &tools) {
	QueryProfiler &parent_profiler = tools.profiler;
	QueryProfilerSimpleGuard parent_guard(parent_profiler);

	// TODO: think about the semantics of provenance!
	QueryRectangle fullRect(SpatialReference::extent(rect.crsId), TemporalReference(rect.timetype), QueryResolution::none());

	auto &cache = CacheManager::get_instance().get_provenance_cache();
	std::unique_ptr<ProvenanceCollection> result;
	try {
		result = cache.query( *this, fullRect, parent_profiler );
	} catch ( NoSuchElementException &nse ) {
		QueryProfilerStoppingGuard stop_guard(parent_profiler);
		QueryProfiler exec_profiler;
		{
			QueryProfilerRunningGuard guard(parent_profiler, exec_profiler);
			TIME_EXEC("Operator.getPlot");
			result = getFullProvenance();
		}
		d_profile(depth, type, "provenance", exec_profiler);
		if ( cache.put(semantic_id,result, fullRect, exec_profiler) )
			parent_profiler.cached(exec_profiler);
	}
	return result;
}


std::unique_ptr<GenericRaster> GenericOperator::getRasterFromSource(int idx, const QueryRectangle &rect, const QueryTools &tools, RasterQM query_mode) {
	if (idx < 0 || idx >= sourcecounts[0])
		throw OperatorException("getChildRaster() called on invalid index");
	QueryProfilerStoppingGuard guard(tools.profiler);
	auto result = sources[idx]->getCachedRaster(rect, tools, query_mode);
	return result;
}
std::unique_ptr<PointCollection> GenericOperator::getPointCollectionFromSource(int idx, const QueryRectangle &rect, const QueryTools &tools, FeatureCollectionQM query_mode) {
	if (idx < 0 || idx >= sourcecounts[1])
		throw OperatorException("getChildPoints() called on invalid index");
	QueryProfilerStoppingGuard guard(tools.profiler);
	int offset = sourcecounts[0] + idx;
	auto result = sources[offset]->getCachedPointCollection(rect, tools, query_mode);
	return result;
}
std::unique_ptr<LineCollection> GenericOperator::getLineCollectionFromSource(int idx, const QueryRectangle &rect, const QueryTools &tools, FeatureCollectionQM query_mode) {
	if (idx < 0 || idx >= sourcecounts[2])
		throw OperatorException("getChildLines() called on invalid index");
	QueryProfilerStoppingGuard guard(tools.profiler);
	int offset = sourcecounts[0] + sourcecounts[1] + idx;
	auto result = sources[offset]->getCachedLineCollection(rect, tools, query_mode);
	return result;
}
std::unique_ptr<PolygonCollection> GenericOperator::getPolygonCollectionFromSource(int idx, const QueryRectangle &rect, const QueryTools &tools, FeatureCollectionQM query_mode) {
	if (idx < 0 || idx >= sourcecounts[3]){
		std::stringstream sstm;
		sstm << "getChildPolygons() called on invalid index: " << idx;
		throw OperatorException(sstm.str());
	}
	QueryProfilerStoppingGuard guard(tools.profiler);
	int offset = sourcecounts[0] + sourcecounts[1] + sourcecounts[2] + idx;
	auto result = sources[offset]->getCachedPolygonCollection(rect, tools, query_mode);
	return result;
}


// JSON constructor
static int parseSourcesFromJSON(Json::Value &sourcelist, GenericOperator *sources[GenericOperator::MAX_SOURCES], int &sourcecount, int depth) {
	if (!sourcelist.isArray() || sourcelist.size() <= 0)
		return 0;

	int newsources = sourcelist.size();

	if (sourcecount + newsources >= GenericOperator::MAX_SOURCES)
		throw OperatorException("Operator with more than MAX_SOURCES found; increase the constant and recompile");

	for (int i=0;i<newsources;i++) {
		sources[sourcecount+i] = GenericOperator::fromJSON(sourcelist[(Json::Value::ArrayIndex) i], depth).release();
	}

	sourcecount += newsources;
	return newsources;
}

std::unique_ptr<GenericOperator> GenericOperator::fromJSON(Json::Value &json, int depth) {
	std::string sourcetypes[] = { "raster", "points", "lines", "polygons" };

	// recursively create all sources
	Json::Value sourcelist = json["sources"];
	Json::Value params = json["params"];
	if (!params.isObject()) {
		params = Json::objectValue;
	}

	int sourcecount = 0;
	int sourcecounts[MAX_INPUT_TYPES] = {0};
	GenericOperator *sources[MAX_SOURCES] = {nullptr};
	try {
		// Instantiate all the sources
		if (sourcelist.isObject()) {
			for (int i=0;i<MAX_INPUT_TYPES;i++)
				sourcecounts[i] = parseSourcesFromJSON(sourcelist[sourcetypes[i]], sources, sourcecount, depth+1);
		}

		// now check the operator name and instantiate the correct class
		std::string type = json["type"].asString();

		auto map = getRegisteredConstructorsMap();
		if (map->count(type) != 1)
			throw OperatorException(std::string("Unknown operator type: '")+type+"'");

		auto constructor = map->at(type);
		auto op = constructor(sourcecounts, sources, params);
		op->type = type;
		op->depth = depth;

		// Finally construct the semantic id
		std::ostringstream semantic_id;
		semantic_id << "{ \"type\": \"" << type << "\", \"params\": ";
		op->writeSemanticParameters(semantic_id);
		semantic_id << ", \"sources\":{";
		int sourceidx = 0;
		bool first_sourcetype = true;
		for (int i=0;i<MAX_INPUT_TYPES;i++) {
			int sourcecount = op->sourcecounts[i];
			if (sourcecount > 0) {
				if (!first_sourcetype)
					semantic_id << ",";
				first_sourcetype = false;
				semantic_id << "\"" << sourcetypes[i] << "\": [";
				for (int j=0;j<sourcecount;j++) {
					if (j > 0)
						semantic_id << ",";
					semantic_id << op->sources[sourceidx++]->semantic_id;
				}
				semantic_id << "]";
			}
		}
		semantic_id << "}}";
		op->semantic_id = semantic_id.str();
		return op;
	}
	catch (const std::exception &e) {
		for (int i=0;i<MAX_SOURCES;i++) {
			if (sources[i]) {
				delete sources[i];
				sources[i] = nullptr;
			}
		}
		throw;
	}
}


std::unique_ptr<GenericOperator> GenericOperator::fromJSON(const std::string &json, int depth) {
	std::istringstream iss(json);
	Json::Reader reader(Json::Features::strictMode());
	Json::Value root;
	if (!reader.parse(iss, root))
		throw OperatorException("unable to parse json");

	return GenericOperator::fromJSON(root, depth);
}
