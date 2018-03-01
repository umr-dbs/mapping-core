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
		virtual std::unique_ptr<QueryProcessor::QueryResult> process(const Query &q, bool includeProvenance);
		virtual std::unique_ptr<QueryProcessor::QueryProgress> processAsync(const Query &q, bool includeProvenance) = 0;
};

class QueryProcessorBackendRegistration {
	public:
		QueryProcessorBackendRegistration(const char *name, std::unique_ptr<QueryProcessor::QueryProcessorBackend> (*constructor)(const std::shared_ptr<cpptoml::table> params));
};

#define REGISTER_QUERYPROCESSOR_BACKEND(classname, name) static std::unique_ptr<QueryProcessor::QueryProcessorBackend> create##classname(const std::shared_ptr<cpptoml::table> params) { return make_unique<classname>(params); } static QueryProcessorBackendRegistration register_##classname(name, create##classname)


#endif

