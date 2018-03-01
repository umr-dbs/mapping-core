
#include "processing/queryprocessor_backend.h"
#include "util/make_unique.h"

class LocalQueryProcessor : public QueryProcessor::QueryProcessorBackend {
	public:
		LocalQueryProcessor();
		virtual ~LocalQueryProcessor();
		virtual std::unique_ptr<QueryProcessor::QueryResult> process(const Query &q, bool includeProvenance);
		virtual std::unique_ptr<QueryProcessor::QueryProgress> processAsync(const Query &q, bool includeProvenance);
};
REGISTER_QUERYPROCESSOR_BACKEND(LocalQueryProcessor, "local");

LocalQueryProcessor::LocalQueryProcessor() {

}

LocalQueryProcessor::~LocalQueryProcessor() {

}


std::unique_ptr<QueryProcessor::QueryResult> LocalQueryProcessor::process(const Query &q, bool includeProvenance) {
	try {
		auto op = GenericOperator::fromJSON(q.operatorgraph);

		QueryProfiler profiler;
		QueryTools tools(profiler);

		std::unique_ptr<ProvenanceCollection> provenance;
		if(includeProvenance) {
			provenance.reset(op->getCachedFullProvenance(q.rectangle, tools).release());
//			provenance.reset(op->getFullProvenance().release());
		}

		if (q.result == Query::ResultType::RASTER)
			return QueryProcessor::QueryResult::raster( op->getCachedRaster(q.rectangle, tools), q.rectangle, std::move(provenance) );
		else if (q.result == Query::ResultType::POINTS)
			return QueryProcessor::QueryResult::points( op->getCachedPointCollection(q.rectangle, tools), q.rectangle, std::move(provenance) );
		else if (q.result == Query::ResultType::LINES)
			return QueryProcessor::QueryResult::lines( op->getCachedLineCollection(q.rectangle, tools), q.rectangle, std::move(provenance) );
		else if (q.result == Query::ResultType::POLYGONS)
			return QueryProcessor::QueryResult::polygons( op->getCachedPolygonCollection(q.rectangle, tools), q.rectangle, std::move(provenance) );
		else if (q.result == Query::ResultType::PLOT) {
			auto plot = op->getCachedPlot(q.rectangle, tools);
			return QueryProcessor::QueryResult::plot(plot->toJSON(), q.rectangle, std::move(provenance));
		}
		else {
			throw ArgumentException("Unknown query type");
		}
	}
	catch (const std::exception &e) {
		return QueryProcessor::QueryResult::error(e.what(), q.rectangle);
	}
}

class LocalQueryProgress : public QueryProcessor::QueryProgress {
	public:
		virtual ~LocalQueryProgress() {};
		virtual bool isFinished() { return true; };
		virtual void wait() { };
		virtual std::unique_ptr<QueryProcessor::QueryResult> getResult() { return std::move(result); };
		virtual std::string getID() { return ""; };
		std::unique_ptr<QueryProcessor::QueryResult> result;
};

std::unique_ptr<QueryProcessor::QueryProgress> LocalQueryProcessor::processAsync(const Query &q, bool includeProvenance) {
	auto progress = make_unique<LocalQueryProgress>();
	progress->result = process(q, includeProvenance);
	return std::unique_ptr<QueryProcessor::QueryProgress>( progress.release() );
}
