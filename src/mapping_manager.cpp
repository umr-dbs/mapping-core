#include "datatypes/raster.h"
#include "datatypes/pointcollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/polygoncollection.h"
#include "datatypes/plot.h"
#include "datatypes/colorizer.h"
#include "rasterdb/rasterdb.h"
#include "raster/opencl.h"
#include "cache/manager.h"

#include "operators/operator.h"

#include "userdb/userdb.h"

#include "util/gdal_dataset_importer.h"

#include "util/binarystream.h"
#include "util/configuration.h"
#include "util/gdal.h"
#include "util/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <fstream>
#include <exception>
#include <memory>
#include <utility>

#include <json/json.h>
#include "datatypes/raster_time_series.h"

// This is a helper template, allowing much of the following functionality to be templated for all feature types
template<typename T> struct queryFeature {};

template<> struct queryFeature<PointCollection> {
	static std::unique_ptr<PointCollection> query(GenericOperator *graph, const QueryRectangle &rect) {
		QueryProfiler profiler;
		return graph->getCachedPointCollection(rect, QueryTools(profiler));
	}
};
template<> struct queryFeature<LineCollection> {
	static std::unique_ptr<LineCollection> query(GenericOperator *graph, const QueryRectangle &rect) {
		QueryProfiler profiler;
		return graph->getCachedLineCollection(rect, QueryTools(profiler));
	}
};
template<> struct queryFeature<PolygonCollection> {
	static std::unique_ptr<PolygonCollection> query(GenericOperator *graph, const QueryRectangle &rect) {
		QueryProfiler profiler;
		return graph->getCachedPolygonCollection(rect, QueryTools(profiler));
	}
};



static const char *program_name;
static void usage() {
		printf("Usage:\n");
		printf("%s convert <input_filename> <png_filename>\n", program_name);
		printf("%s createsource <crs> <channel1_example> <channel2_example> ...\n", program_name);
		printf("%s loadsource <sourcename>\n", program_name);
		printf("%s import <sourcename> <filename> <filechannel> <sourcechannel> <time_start> <duration> <compression>\n", program_name);
		printf("%s link <sourcename> <sourcechannel> <time_reference> <time_start> <duration>\n", program_name);
		printf("%s query <queryname> <png_filename>\n", program_name);
		printf("%s testquery <queryname> [S|F]\n", program_name);
		printf("%s showprovenance <queryname>\n", program_name);
		printf("%s enumeratesources [verbose]\n", program_name);
		printf("%s userdb ...\n", program_name);
		printf("%s importgdaldataset <dataset_name> <dataset_filename_with_placeholder> <dataset_file_path> <time_format> <time_start> <time_unit> <interval_value> [--unit <measurement> <unit> <interpolation>] [--citation|--c <provenance_citation>] [--license|--l <provenance_license>] [--uri|--u <provenence_uri>]\n", program_name);
		exit(5);
}

static void convert(int argc, char *argv[]) {
	if (argc < 3) {
		usage();
	}

	try {
		auto raster = GenericRaster::fromGDAL(argv[2], 1);
		auto c = Colorizer::greyscale(raster->dd.unit.getMin(), raster->dd.unit.getMax());
		std::ofstream output(argv[3]);
		raster->toPNG(output, *c);
	}
	catch (ImporterException &e) {
		printf("%s\n", e.what());
		exit(5);
	}
}

/**
 * Erstellt eine neue Rasterquelle basierend auf ein paar Beispielbildern
 */
static void createsource(int argc, char *argv[]) {
	std::unique_ptr<GenericRaster> raster;

	Json::Value root(Json::objectValue);

	Json::Value channels(Json::arrayValue);

	std::unique_ptr<GDALCRS> lcrs;

	CrsId crsId = CrsId::from_epsg_code(atoi(argv[2]));

	for (int i=0;i<argc-3;i++) {
		try {
			raster = GenericRaster::fromGDAL(argv[i+3], 1, crsId);
		}
		catch (ImporterException &e) {
			printf("%s\n", e.what());
			exit(5);
		}

		if (i == 0) {
			lcrs.reset(new GDALCRS(*raster));

			Json::Value coords(Json::objectValue);
			Json::Value sizes(Json::arrayValue);
			Json::Value origins(Json::arrayValue);
			Json::Value scales(Json::arrayValue);
			for (int d=0;d<lcrs->dimensions;d++) {
				sizes[d] = lcrs->size[d];
				origins[d] = lcrs->origin[d];
				scales[d] = lcrs->scale[d];
			}
			coords["crs"] = lcrs->crsId.to_string();
			coords["size"] = sizes;
			coords["origin"] = origins;
			coords["scale"] = scales;

			root["coords"] = coords;
		}
		else {
			GDALCRS compare_crs(*raster);
			if (!(*lcrs == compare_crs)) {
				printf("Channel %d has a different coordinate system than the first channel\n", i);
				exit(5);
			}
		}

		Json::Value channel(Json::objectValue);
		channel["datatype"] = GDALGetDataTypeName(raster->dd.datatype);
		channel["unit"] = raster->dd.unit.toJsonObject();
		if (raster->dd.has_no_data)
			channel["nodata"] = raster->dd.no_data;

		channels.append(channel);
	}

	root["channels"] = channels;

	Json::StyledWriter writer;
	std::string json = writer.write(root);

	printf("%s\n", json.c_str());
}


static void loadsource(int argc, char *argv[]) {
	if (argc < 3) {
		usage();
	}
	try {
		auto db = RasterDB::open(argv[2]);
	}
	catch (std::exception &e) {
		printf("Failure: %s\n", e.what());
	}
}

// import <sourcename> <filename> <filechannel> <sourcechannel> <time_start> <duration> <compression>
static void import(int argc, char *argv[]) {
	if (argc < 9) {
		usage();
	}
	try {
		auto db = RasterDB::open(argv[2], RasterDB::READ_WRITE);
		const char *filename = argv[3];
		int sourcechannel = atoi(argv[4]);
		int channelid = atoi(argv[5]);
		double time_start = atof(argv[6]);
		double duration = atof(argv[7]);
		std::string compression = argv[8];
		db->import(filename, sourcechannel, channelid, time_start, time_start+duration, compression);
	}
	catch (std::exception &e) {
		printf("Failure: %s\n", e.what());
	}
}

// link <sourcename> <channel> <reference_time> <new_time_start> <new_duration>
static void link(int argc, char *argv[]) {
	if (argc < 7) {
		usage();
	}
	try {
		auto db = RasterDB::open(argv[2], RasterDB::READ_WRITE);
		int channelid = atoi(argv[3]);
		double time_reference = atof(argv[4]);
		double time_start = atof(argv[5]);
		double duration = atof(argv[6]);
		db->linkRaster(channelid, time_reference, time_start, time_start+duration);
	}
	catch (std::exception &e) {
		printf("Failure: %s\n", e.what());
	}
}

static SpatialReference sref_from_json(Json::Value &root, bool &flipx, bool &flipy){
	if(root.isMember("spatial_reference")){
		Json::Value& json = root["spatial_reference"];

		CrsId crsId = CrsId::from_srs_string(json.get("projection", "EPSG:4326").asString());

		double x1 = json.get("x1", -180).asDouble();
		double y1 = json.get("y1", -90).asDouble();
		double x2 = json.get("x2", 180).asDouble();
		double y2 = json.get("y2", 90).asDouble();

		return SpatialReference(crsId, x1, y1, x2, y2, flipx, flipy);
	}

	return SpatialReference::unreferenced();
}

static TemporalReference tref_from_json(Json::Value &root){
	if(root.isMember("temporal_reference")){
		Json::Value& json = root["temporal_reference"];

		std::string type = json.get("type", "UNIX").asString();

		timetype_t time_type;

		if(type == "UNIX")
			time_type = TIMETYPE_UNIX;
		else
			time_type = TIMETYPE_UNKNOWN;

		double start = json.get("start", 0).asDouble();
		if (json.isMember("end")) {
			double end = json.get("end", 0).asDouble();
			return TemporalReference(time_type, start, end);
		}

		return TemporalReference(time_type, start);
	}

	return TemporalReference::unreferenced();
}

static QueryResolution qres_from_json(Json::Value &root){
	if(root.isMember("resolution")){
		Json::Value& json = root["resolution"];

		std::string type = json.get("type", "none").asString();

		if(type == "pixels"){
			int x = json.get("x", 1000).asInt();
			int y = json.get("y", 1000).asInt();

			return QueryResolution::pixels(x, y);
		} else if(type == "none")
			return QueryResolution::none();
		else {
			fprintf(stderr, "invalid query resolution");
			exit(5);
		}


	}

	return QueryResolution::none();
}

static QueryRectangle qrect_from_json(Json::Value &root, bool &flipx, bool &flipy) {
	return QueryRectangle (
		sref_from_json(root, flipx, flipy),
		tref_from_json(root),
		qres_from_json(root)
	);
}


template<typename T> void runfeaturequery(GenericOperator *graph, const QueryRectangle &qrect, const char *out_filename) {
	auto features = queryFeature<T>::query(graph, qrect);
	auto csv = features->toCSV();
	if (out_filename) {
		FILE *f = fopen(out_filename, "w");
		if (f) {
			fwrite(csv.c_str(), csv.length(), 1, f);
			fclose(f);
		}
	}
	else
		printf("No output filename given, discarding result\n");
}

static void runquery(int argc, char *argv[]) {
	if (argc < 3) {
		usage();
	}
	char *in_filename = argv[2];
	char *out_filename = nullptr;
	if (argc > 3)
		out_filename = argv[3];

	/*
	 * Step #1: open the query.json file and parse it
	 */
	std::ifstream file(in_filename);
	if (!file.is_open()) {
		printf("unable to open query file %s\n", in_filename);
		exit(5);
	}

	Json::Reader reader(Json::Features::strictMode());
	Json::Value root;
	if (!reader.parse(file, root)) {
		printf("unable to read json\n%s\n", reader.getFormattedErrorMessages().c_str());
		exit(5);
	}

	auto graph = GenericOperator::fromJSON(root["query"]);
	std::string result = root.get("query_result", "raster").asString();

	bool flipx, flipy;
	auto qrect = qrect_from_json(root, flipx, flipy);

	if (result == "raster") {
		QueryProfiler profiler;

		std::string queryModeParam = root.get("query_mode", "exact").asString();
		GenericOperator::RasterQM queryMode;

		if(queryModeParam == "exact")
			queryMode = GenericOperator::RasterQM::EXACT;
		else if(queryModeParam == "loose")
			queryMode = GenericOperator::RasterQM::LOOSE;
		else {
			fprintf(stderr, "invalid query mode");
			exit(5);
		}
		
		auto raster = graph->getCachedRaster(qrect, QueryTools(profiler), queryMode); 
		
		printf("flip: %d %d\n", flipx, flipy);
		printf("QRect(%f,%f -> %f,%f)\n", qrect.x1, qrect.y1, qrect.x2, qrect.y2);
		if (flipx || flipy)
			raster = raster->flip(flipx, flipy);

		if (out_filename) {
			{
				//Profiler::Profiler p("TO_GTIFF");
				raster->toGDAL((std::string(out_filename) + ".tif").c_str(), "GTiff", flipx, flipy);
			}
			{
				//Profiler::Profiler p("TO_PNG");
				auto colors = Colorizer::greyscale(raster->dd.unit.getMin(), raster->dd.unit.getMax());
				std::ofstream output(std::string(out_filename) + ".png");
				raster->toPNG(output, *colors);
			}
		}
		else
			printf("No output filename given, discarding result of size %d x %d\n", raster->width, raster->height);
	}
	else if(result == "raster_time_series"){
		QueryProfiler profiler;

		std::string queryModeParam = root.get("query_mode", "exact").asString();
		GenericOperator::RasterQM queryMode;

		if(queryModeParam == "exact")
			queryMode = GenericOperator::RasterQM::EXACT;
		else if(queryModeParam == "loose")
			queryMode = GenericOperator::RasterQM::LOOSE;
		else {
			fprintf(stderr, "invalid query mode");
			exit(5);
		}

		auto rts = graph->getCachedRasterTimeSeries(qrect, QueryTools(profiler), queryMode);
		auto raster = rts->getAsRaster(rts::RasterTimeSeries::MoreThanOneRasterHandling::THROW);

		printf("flip: %d %d\n", flipx, flipy);
		printf("QRect(%f,%f -> %f,%f)\n", qrect.x1, qrect.y1, qrect.x2, qrect.y2);
		if (flipx || flipy)
			raster = raster->flip(flipx, flipy);

		if (out_filename) {
			{
				//Profiler::Profiler p("TO_GTIFF");
				raster->toGDAL((std::string(out_filename) + ".tif").c_str(), "GTiff", flipx, flipy);
			}
			{
				//Profiler::Profiler p("TO_PNG");
				auto colors = Colorizer::greyscale(raster->dd.unit.getMin(), raster->dd.unit.getMax());
				std::ofstream output(std::string(out_filename) + ".png");
				raster->toPNG(output, *colors);
			}
		}
		else
			printf("No output filename given, discarding result of size %d x %d\n", raster->width, raster->height);
	}
	else if (result == "points") {
		runfeaturequery<PointCollection>(graph.get(), qrect, out_filename);
	}
	else if (result == "lines") {
		runfeaturequery<LineCollection>(graph.get(), qrect, out_filename);
	}
	else if (result == "polygons") {
		runfeaturequery<PolygonCollection>(graph.get(), qrect, out_filename);
	}
	else if (result == "plot") {
			QueryProfiler profiler;
			auto plot = graph->getCachedPlot(qrect, QueryTools(profiler));
			auto json = plot->toJSON();
			if (out_filename) {
				FILE *f = fopen(out_filename, "w");
				if (f) {
					fwrite(json.c_str(), json.length(), 1, f);
					fclose(f);
				}
			}
			else
				printf("No output filename given, discarding result\n");
		}
	else {
		printf("Unknown result type: %s\n", result.c_str());
		exit(5);
	}
}

static void testsemantic(GenericOperator *graph) {
	/*
	 * Make sure the graph's semantic id works
	 */
	try {
		auto semantic1 = graph->getSemanticId();
		std::unique_ptr<GenericOperator> graph2 = nullptr;
		try {
			graph2 = GenericOperator::fromJSON(semantic1);
		}
		catch (const std::exception &e) {
			printf("\nFAILED: semantic\nException parsing graph from semantic id: %s\n%s", e.what(), semantic1.c_str());
			return;
		}
		auto semantic2 = graph2->getSemanticId();
		if (semantic1 != semantic2) {
			printf("\nFAILED: semantic\nSemantic ID changes after reconstruction:\n%s\n%s\n", semantic1.c_str(), semantic2.c_str());
			return;
		}
	}
	catch (const std::exception &e) {
		printf("\nFAILED: semantic\nException during testsemantic: %s\n", e.what());
		return;
	}

	printf("\nPASSED: semantic\n");
	return;
}


template<typename T>
std::string getIPCHash(T &t) {
	BinaryWriteBuffer buf;
	buf << t;
	return buf.hash().asHex();
}

std::string getStringHash(const std::string &str) {
	SHA1 sha1;
	sha1.addBytes(str.data(), str.length());
	return sha1.digest().asHex();
}

template<typename T> std::pair<std::string, std::string> testfeaturequery(GenericOperator *graph, const QueryRectangle &qrect, bool full_test) {
	auto features = queryFeature<T>::query(graph, qrect);
	auto clone = features->clone();

	std::string real_hash = getIPCHash(*features);
	std::string real_hash2 = getIPCHash(*clone);

	if(full_test) {
		//run query again with upper half of MBR to check if operator correctly filters according to space
		auto mbr = features->getCollectionMBR();
		mbr.y2 -= (mbr.y2 - mbr.y1) / 2;
		auto qrect_cut = QueryRectangle(mbr, qrect, qrect);
		auto cut = queryFeature<T>::query(graph, qrect_cut);
		auto features_filtered = features->filterBySpatioTemporalReferenceIntersection(qrect_cut);

		std::string hash_cut = getIPCHash(*cut);
		std::string hash_filtered = getIPCHash(*features_filtered);

		if (hash_cut != hash_filtered) {
			printf("FAILED: hash\nHashes of result of query on subregion and filter on subregion of features differ. modified qrect: %s\nfilter on features:      %s\n", hash_cut.c_str(), hash_filtered.c_str());
			throw 1;
		}
	}

	return std::make_pair(real_hash, real_hash2);
}

// The return code is 0 for both success and failure, nonzero for any actual error (e.g. exception or crash)
static int testquery(int argc, char *argv[]) {
	if (argc < 3) {
		usage();
	}
	char *in_filename = argv[2];
	bool set_hash = false;
	bool full_test = false;
	if (argc >= 4) {
		if(argv[3][0] == 'S')
			set_hash = true;
		else if(argv[3][0] == 'F')
			full_test = true;
	}


	try {
		/*
		 * Step #1: open the query.json file and parse it
		 */
		std::ifstream file(in_filename);
		if (!file.is_open()) {
			printf("unable to open query file %s\n", in_filename);
			return 5;
		}

		Json::Reader reader(Json::Features::strictMode());
		Json::Value root;
		if (!reader.parse(file, root)) {
			printf("unable to read json\n%s\n", reader.getFormattedErrorMessages().c_str());
			return 5;
		}

		auto graph = GenericOperator::fromJSON(root["query"]);

		/*
		 * Step #2: test if the semantic ID of this query is working
		 */
		testsemantic(graph.get());

		/*
		 * Step #3: run the query and see if the results match
		 */
		std::string result = root.get("query_result", "raster").asString();
		std::string real_hash, real_hash2;

		bool flipx, flipy;
		auto qrect = qrect_from_json(root, flipx, flipy);
		if (result == "raster") {
			QueryProfiler profiler;

			std::string queryModeParam = root.get("query_mode", "exact").asString();
			GenericOperator::RasterQM queryMode;

			if(queryModeParam == "exact")
				queryMode = GenericOperator::RasterQM::EXACT;
			else if(queryModeParam == "loose")
				queryMode = GenericOperator::RasterQM::LOOSE;
			else {
				fprintf(stderr, "invalid query mode");
				return 5;
			}

			auto raster = graph->getCachedRaster(qrect, QueryTools(profiler), queryMode);
			if (flipx || flipy)
				raster = raster->flip(flipx, flipy);
			real_hash = getIPCHash(*raster);
			real_hash2 = getIPCHash(*raster->clone());
		}
		else if (result == "points") {
			auto res = testfeaturequery<PointCollection>(graph.get(), qrect, full_test);
			real_hash = res.first;
			real_hash2 = res.second;
		}
		else if (result == "lines") {
			auto res = testfeaturequery<LineCollection>(graph.get(), qrect, full_test);
			real_hash = res.first;
			real_hash2 = res.second;
		}
		else if (result == "polygons") {
			auto res = testfeaturequery<PolygonCollection>(graph.get(), qrect, full_test);
			real_hash = res.first;
			real_hash2 = res.second;
		}
		else if (result == "plot") {
			QueryProfiler profiler;
			auto plot = graph->getCachedPlot(qrect, QueryTools(profiler));
			real_hash = getStringHash(plot->toJSON());
			real_hash2 = getStringHash(plot->clone()->toJSON());
		}
		else {
			printf("Unknown result type: %s\n", result.c_str());
			return 5;
		}


		if (real_hash != real_hash2) {
			printf("FAILED: hash\nHashes of result and its clone differ, probably a bug in clone():original: %s\ncopy:      %s\n", real_hash.c_str(), real_hash2.c_str());
			return 0;
		}

		if (root.isMember("query_expected_hash")) {
			std::string expected_hash = root.get("query_expected_hash", "#").asString();
			if (expected_hash == real_hash) {
				printf("\nPASSED: hash\n");
				return 0;
			}
			else {
				printf("\nFAILED: hash\nExpected : %s\nResult   : %s\n", expected_hash.c_str(), real_hash.c_str());
				return 0;
			}
		}
		else if (set_hash) {
			root["query_expected_hash"] = real_hash;
			std::ofstream file(in_filename);
			file << root;
			file.close();
			printf("No hash in query file, added %s\n", real_hash.c_str());
			return 5;
		}
		else {
			printf("No hash in query file\n");
			return 5;
		}
	}
	catch (const std::exception &e) {
		printf("Exception: %s\n", e.what());
		return 5;
	}
	catch (...) {
		return 5;
	}
	return 10;
}

static int showprovenance(int argc, char *argv[]) {
	if (argc != 3) {
		usage();
	}
	char *in_filename = argv[2];

	try {
		/*
		 * Step #1: open the query.json file and parse it
		 */
		std::ifstream file(in_filename);
		if (!file.is_open()) {
			printf("unable to open query file %s\n", in_filename);
			return 5;
		}

		Json::Reader reader(Json::Features::strictMode());
		Json::Value root;
		if (!reader.parse(file, root)) {
			printf("unable to read json\n%s\n", reader.getFormattedErrorMessages().c_str());
			return 5;
		}

		auto graph = GenericOperator::fromJSON(root["query"]);
		auto p = graph->getFullProvenance();
		printf("Provenance: %s\n", p->toJson().c_str());
		return 0;
	}
	catch (const std::exception &e) {
		printf("Exception: %s\n", e.what());
		return 5;
	}
	catch (...) {
		return 5;
	}
}

static int userdb_usage() {
	printf("Commands for userdb:\n");
	printf("%s userdb adduser <username> <realname> <email> <password>\n", program_name);
	return 5;
}

static int userdb(int argc, char *argv[]) {
	if (argc < 3)
		return userdb_usage();

	try {
		UserDB::initFromConfiguration();

		std::string command = argv[2];
		if (command == "adduser" && argc == 7) {
			auto user = UserDB::createUser(argv[3], argv[4], argv[5], argv[6]);
			printf("ok\n");
			return 0;
		}

		return userdb_usage();
	}
	catch (const std::exception &e) {
		printf("ERROR: %s\n", e.what());
		return 5;
	}
}

// Imports a gdal dataset for use by source operator GDALSource
static int import_gdal_dataset(int argc, char *argv[]){
	
	if(argc < 9 || argc > 19){
		usage();
	}

	// Indexes:
	// program name:  		1
	// importgdaldataset: 	1
	// std parameter: 		7
	// ---------------------------
	// subtotal				9

	// opt:
	// provenance:			3 + 3	
	// unit					3 + 1
	// ---------------------------
	// total				19
	
	//importgdaldataset <dataset_name> <dataset_filename_with_placeholder> <dataset_file_path> <time_format> <time_start> <time_unit> <interval_value> 
	// 				[--unit <measurement> <unit> <interpolation>] 
	//				[--citation|--c <provenance_citation>] [--license|--l <provenance_license>] [--uri|--u <provenence_uri>]
	
	//check if any of the standard parameters is actually a optional parameter, so an actual value is missing	
	for(int i = 2; i < 9; i++){
		if(argv[i][0] == '-'){
			usage();			
		}
	}

	std::string dataset_name		= argv[2];
	std::string dataset_filename 	= argv[3];
	std::string dataset_file_path 	= argv[4];
	std::string time_format 		= argv[5];
	std::string time_start 			= argv[6];
	std::string time_unit 			= argv[7];
	std::string interval_value		= argv[8];

	std::string citation;
	std::string license;
	std::string uri;

	std::string unit;
	std::string measurement;
	std::string interpolation;

	//read the optional parameters
	int i = 9;
	while(i < argc)
	{
		std::string arg(argv[i]);

		if((arg == "--c" || arg == "--citation") && i+1 < argc){
			std::string param(argv[i + 1]);	
			if(param[0] == '-'){	//if parameter value is missing, meaning next argument starts with -
				usage();
			} else {
				i += 2;
				citation = param;
			}						
		} else if((arg == "--l" || arg == "--license") && i+1 < argc){			
			std::string param(argv[i + 1]);	
			if(param[0] == '-'){
				usage();
			} else {
				i += 2;
				license = param;
			}						
		} else if((arg == "--uri" || arg == "--u") && i+1 < argc){
			std::string param(argv[i + 1]);	
			if(param[0] == '-'){
				usage();
			} else {
				i += 2;
				uri = param;
			}						
		} else if(arg == "--unit" && i + 3 < argc)
		{	
			std::string param1(argv[i+1]);
			std::string param2(argv[i+2]);
			std::string param3(argv[i+3]);

			if(param1[0] == '-' || param2[0] == '-' || param3[0] == '-')
				usage();

			measurement 	= argv[i+1];
			unit 			= argv[i+2];
			interpolation 	= argv[i+3];

			i += 4;
		} else {
			usage();
		}
	}

	GDALDatasetImporter::importDataset(	dataset_name, 
									dataset_filename, 
									dataset_file_path, 
									time_format, 
									time_start, 
									time_unit, 
									interval_value, 
									citation, 
									license, 
									uri, 
									measurement, 
									unit, 
									interpolation);

	return 1;
}

int main(int argc, char *argv[]) {

	program_name = argv[0];
	int returncode = 0;

	if (argc < 2) {
		usage();
	}

	Configuration::loadFromDefaultPaths();
	// FIXME
	NopCacheManager cm;
	CacheManager::init(&cm);

	Log::logToStream(Log::LogLevel::WARN, &std::cerr);

	const char *command = argv[1];

	if (strcmp(command, "convert") == 0) {
		convert(argc, argv);
	}
	else if (strcmp(command, "createsource") == 0) {
		createsource(argc, argv);
	}
	else if (strcmp(command, "loadsource") == 0) {
		loadsource(argc, argv);
	}
	else if (strcmp(command, "import") == 0) {
		import(argc, argv);
	}
	else if (strcmp(command, "link") == 0) {
		link(argc, argv);
	}
	else if (strcmp(command, "query") == 0) {
		runquery(argc, argv);
	}
	else if (strcmp(command, "testquery") == 0) {
		returncode = testquery(argc, argv);
	}
	else if (strcmp(command, "showprovenance") == 0) {
		returncode = showprovenance(argc, argv);
	}
	else if (strcmp(command, "enumeratesources") == 0) {
		bool verbose = false;
		if (argc > 2)
			verbose = true;
		auto names = RasterDB::getSourceNames();
		for (const auto &name : names) {
			printf("Source: %s\n", name.c_str());
			if (verbose) {
				printf("----------------------------------------------------------------------\n");
				auto json = RasterDB::getSourceDescription(name);
				printf("JSON: %s\n", json.c_str());
				printf("----------------------------------------------------------------------\n");
			}
		}
	}
	else if (strcmp(command, "userdb") == 0) {
		returncode = userdb(argc, argv);
	}
	else if (strcmp(command, "msgcoord") == 0) {
		GDAL::CRSTransformer t(CrsId::from_epsg_code(4326), CrsId::from_srs_string("SR-ORG:81"));
		auto f = [&] (double x, double y) -> void {
			double px = x, py = y;
			if (t.transform(px, py))
				printf("%f, %f -> %f, %f\n", x, y, px, py);
			else
				printf("%f, %f -> failed\n", x, y);
		};
		f(11, -16);
		f(36, -36);
		f(11, -36);
		f(36, -16);
	}
#ifndef MAPPING_NO_OPENCL
	else if (strcmp(command, "clinfo") == 0) {
		RasterOpenCL::init();
		auto mbs = RasterOpenCL::getMaxAllocSize();
		printf("maximum buffer size is %ud (%d MB)\n", mbs, mbs/1024/1024);
	}
#endif
	else if(strcmp(command, "importgdaldataset") == 0){
		import_gdal_dataset(argc, argv);
	}
	else {
		usage();
	}
#ifndef MAPPING_NO_OPENCL
	RasterOpenCL::free();
#endif
	return returncode;
}
