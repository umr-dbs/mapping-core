#ifndef PROCESSING_QUERYPROCESSOR_BACKEND_H
#define PROCESSING_QUERYPROCESSOR_BACKEND_H

#include "processing/queryprocessor.h"

#include "datatypes/raster.h"
#include "datatypes/pointcollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/polygoncollection.h"
#include "datatypes/plot.h"

class QueryProcessor::QueryProcessorBackend {
	public:
		QueryProcessorBackend() = default;
		virtual ~QueryProcessorBackend() {};
		virtual std::unique_ptr<QueryProcessor::QueryResult> process(const Query &q, std::shared_ptr<UserDB::Session> session, bool includeProvenance);
		virtual std::unique_ptr<QueryProcessor::QueryProgress> processAsync(const Query &q, std::shared_ptr<UserDB::Session> session,bool includeProvenance) = 0;
};

class QueryProcessorBackendRegistration {
	public:
		QueryProcessorBackendRegistration(const char *name, std::unique_ptr<QueryProcessor::QueryProcessorBackend> (*constructor)(const ConfigurationTable& params));
};

#define REGISTER_QUERYPROCESSOR_BACKEND(classname, name) static std::unique_ptr<QueryProcessor::QueryProcessorBackend> create##classname(const ConfigurationTable& params) { return std::make_unique<classname>(params); } static QueryProcessorBackendRegistration register_##classname(name, create##classname)


#endif

