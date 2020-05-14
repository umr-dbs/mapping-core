
#include "processing/queryprocessor_backend.h"

#include <unordered_map>


/*
 * Default implementations
 */
std::unique_ptr<QueryProcessor::QueryResult> QueryProcessor::QueryProcessorBackend::process(const Query &q, std::shared_ptr<UserDB::Session> session, bool includeProvenance) {
	auto progress = processAsync(q, session, includeProvenance);
	progress->wait();
	return progress->getResult();
}


/*
 * Backend registration
 */
typedef std::unique_ptr<QueryProcessor::QueryProcessorBackend> (*BackendConstructor)(const ConfigurationTable& params);

static std::unordered_map< std::string, BackendConstructor > *getRegisteredConstructorsMap() {
	static std::unordered_map< std::string, BackendConstructor > registered_constructors;
	return &registered_constructors;
}

QueryProcessorBackendRegistration::QueryProcessorBackendRegistration(const char *name, BackendConstructor constructor) {
	auto map = getRegisteredConstructorsMap();
	(*map)[std::string(name)] = constructor;
}

std::unique_ptr<QueryProcessor> QueryProcessor::create(const std::string &backend, const ConfigurationTable& params) {
	auto map = getRegisteredConstructorsMap();
	if (map->count(backend) != 1)
		throw ArgumentException(concat("Unknown QueryProcessor backend: ", backend), MappingExceptionType::TRANSIENT);

	auto constructor = map->at(backend);
	auto backend_instance = constructor(params);
	return std::unique_ptr<QueryProcessor>( new QueryProcessor(std::move(backend_instance)) );
}
