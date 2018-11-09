#include "util/configuration.h"
#include "cache/manager.h"
#include "util/timeparser.h"

#include "services/httpservice.h"
#include "userdb/userdb.h"
#include "util/log.h"
#include "featurecollectiondb/featurecollectiondb.h"

#include "cache/node/manager/local_manager.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include <json/json.h>
#include <fcgio.h>
#include <unistd.h>
#include <thread>


/*
A few benchmarks:
SAVE_PNG8:   0.052097
SAVE_PNG32:  0.249503
SAVE_JPEG8:  0.021444 (90%)
SAVE_JPEG32: 0.060772 (90%)
SAVE_JPEG8:  0.021920 (100%)
SAVE_JPEG32: 0.060187 (100%)

Sizes:
JPEG8:  200526 (100%)
PNG8:   159504
JPEG8:  124698 (95%)
JPEG8:   92284 (90%)

PNG32:  366925
JPEG32: 308065 (100%)
JPEG32: 168333 (95%)
JPEG32: 120703 (90%)
*/

/**
 * A thread function that handles fcgi request
 */
void fcgiThread(int fd) {
    std::stringstream id_stream;
    id_stream << std::this_thread::get_id();
    std::string thread_id(id_stream.str());
    Log::debug("Start of thread: " + thread_id);

    FCGX_Init();

	FCGX_Request request;

	FCGX_InitRequest(&request, fd, 0);

	while (FCGX_Accept_r(&request) == 0) {
		fcgi_streambuf streambuf_in(request.in);
		fcgi_streambuf streambuf_out(request.out);
		fcgi_streambuf streambuf_err(request.err);
		Log::setThreadRequestId(request.requestId);
        char * ip_ptr = FCGX_GetParam("REMOTE_ADDR", request.envp);
        std::string ip = ip_ptr != nullptr ? ip_ptr : "";
		Log::debug(concat("New request ", request.requestId, " from ip ", ip,  ", on thread ", thread_id));
		HTTPService::run(&streambuf_in, &streambuf_out, &streambuf_err, request);
		Log::debug(concat("Finished request ", request.requestId));
	}
    Log::debug("End of thread: " + thread_id);
}

int main() {
	Configuration::loadFromDefaultPaths();
	Log::streamAndMemoryOff();

	const bool isCgiMode = getenv("FCGI_WEB_SERVER_ADDRS") == nullptr;

	if(Configuration::get<bool>("log.logtofile")){
		Log::logToFile(isCgiMode);
		Log::logRequestId(true);
	}

	/*
	 * Initialize Cache
	 */
	bool cache_enabled = Configuration::get<bool>("cache.enabled",false);

	std::unique_ptr<CacheManager> cm;

	// Plug in Cache-Dummy if cache is disabled
	if ( !cache_enabled ) {
		cm = make_unique<NopCacheManager>();
	}
	else {
		std::string cacheType = Configuration::get<std::string>("cache.type");

		if(cacheType == "local") {
			cm = make_unique<LocalCacheManager>(Configuration::get<std::string>("cache.strategy"), Configuration::get<std::string>("cache.replacement"),
					Configuration::get<int>("cache.raster.size"),
					Configuration::get<int>("cache.points.size"),
					Configuration::get<int>("cache.lines.size"),
					Configuration::get<int>("cache.polygons.size"),
					Configuration::get<int>("cache.plots.size"),
					Configuration::get<int>("cache.provenance.size")
			);
		} else if(cacheType == "remote") {
			std::string host = Configuration::get<std::string>("indexserver.host");
			int port = Configuration::get<int>("indexserver.port");
			cm = make_unique<ClientCacheManager>(host,port);
		} else {
			throw ArgumentException("Invalid cache.type");
		}
	}
	CacheManager::init( cm.get() );

	/*
	 * Initialize UserDB
	 */
	UserDB::initFromConfiguration();

	/**
	 * Initialize FeatureCollectionDB
	 */
	FeatureCollectionDB::initFromConfiguration();


	if (isCgiMode) {
		// CGI mode
        long time_id = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
		Log::setThreadRequestId(time_id); //time as request id, to differentiate the requests
		Log::debug("New CGI request.");
		HTTPService::run(std::cin.rdbuf(), std::cout.rdbuf(), std::cerr.rdbuf());
        Log::debug("Finished Request.");
	}
	else {
		// FCGI mode
		// save stdin fd because of OpenCL Bug
		int fd = dup(0);

		size_t numberOfThreads = Configuration::get<size_t>("fcgi.threads", 1);

		// spawn threads
		std::vector<std::thread> threads;
		for(size_t i = 0; i < numberOfThreads; ++i) {
			threads.emplace_back(fcgiThread, fd);
		}

		// wait for finish
		for(size_t i = 0; i < numberOfThreads; ++i) {
			threads[i].join();
		}
	}
	Log::fileOff();
}
