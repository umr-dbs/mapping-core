#ifndef PROCESSING_QUERYPROCESSOR_H
#define PROCESSING_QUERYPROCESSOR_H

#include "processing/query.h"
#include "operators/provenance.h"
#include <memory>
#include <util/configuration.h>
#include <boost/optional.hpp>


class GenericRaster;
class PointCollection;
class LineCollection;
class PolygonCollection;
class SimpleFeatureCollection;


/**
 * This class takes an operator graph and executes it.
 *
 * You could just instantiate an operator graph and call ->getCachedX(), so what does this class do?
 *
 * - it can either execute the graph locally or connect to a processing service for distributed processing
 * - it can execute the graph asynchronously in a separate thread, or non-blocking for distributed processing
 * - it can set up a process-wide or even distributed cache
 *
 * All of these things can be controlled using the global configuration.
 */
class QueryProcessor {
	public:
		class QueryProcessorBackend;
		//class ResultCache;

		class QueryResult {
			public:
				std::unique_ptr<GenericRaster> getRaster(GenericOperator::RasterQM querymode = GenericOperator::RasterQM::LOOSE);
				std::unique_ptr<PointCollection> getPointCollection(GenericOperator::FeatureCollectionQM querymode = GenericOperator::FeatureCollectionQM::ANY_FEATURE);
				std::unique_ptr<LineCollection> getLineCollection(GenericOperator::FeatureCollectionQM querymode = GenericOperator::FeatureCollectionQM::ANY_FEATURE);
				std::unique_ptr<PolygonCollection> getPolygonCollection(GenericOperator::FeatureCollectionQM querymode = GenericOperator::FeatureCollectionQM::ANY_FEATURE);
				std::unique_ptr<SimpleFeatureCollection> getAnyFeatureCollection(GenericOperator::FeatureCollectionQM querymode = GenericOperator::FeatureCollectionQM::ANY_FEATURE);
				std::string getPlot();
				ProvenanceCollection& getProvenance();
				bool isError();
				boost::optional<MappingException> getErrorException();

			static std::unique_ptr<QueryResult> raster(std::unique_ptr<GenericRaster> result, const QueryRectangle &qrect, std::unique_ptr<ProvenanceCollection> provenance);
				static std::unique_ptr<QueryResult> points(std::unique_ptr<PointCollection> result, const QueryRectangle &qrect, std::unique_ptr<ProvenanceCollection> provenance);
				static std::unique_ptr<QueryResult> lines(std::unique_ptr<LineCollection> result, const QueryRectangle &qrect, std::unique_ptr<ProvenanceCollection> provenance);
				static std::unique_ptr<QueryResult> polygons(std::unique_ptr<PolygonCollection> result, const QueryRectangle &qrect, std::unique_ptr<ProvenanceCollection> provenance);
				static std::unique_ptr<QueryResult> plot(const std::string &plot, const QueryRectangle &qrect, std::unique_ptr<ProvenanceCollection> provenance);
				static std::unique_ptr<QueryResult> error(const MappingException &exception, const QueryRectangle &qrect);
			private:
				QueryResult(Query::ResultType result_type, std::unique_ptr<SpatioTemporalResult> result, std::unique_ptr<ProvenanceCollection> provenance, const std::string &result_plot, boost::optional<MappingException> exception, const QueryRectangle &qrect);
				Query::ResultType result_type;
				std::unique_ptr<SpatioTemporalResult> result;
				std::unique_ptr<ProvenanceCollection> provenance;
				std::string result_plot;
				QueryRectangle qrect;
				boost::optional<MappingException> result_exception;
		};

		class QueryProgress {
			public:
				virtual bool isFinished() = 0;
				virtual void wait() = 0;
				virtual std::unique_ptr<QueryResult> getResult() = 0;
				virtual std::string getID() = 0;
		};

		/**
		 * Returns a reference to a (shared) processor configured by the global configuration
		 */
		static QueryProcessor &getDefaultProcessor();
		/**
		 * Instantiate a new processor with the given configuration
		 */
		static std::unique_ptr<QueryProcessor> create(const std::string &backend, const ConfigurationTable& params);


		/**
		 * Starts processing a query, waits for the result and returns it.
		 */
		std::unique_ptr<QueryResult> process(const Query &q, std::shared_ptr<UserDB::Session> session, bool includeProvenance);
		/**
		 * Starts processing a query asynchronously. Returns an object that allows tracking progress and waiting on the result.
		 */
		std::unique_ptr<QueryProgress> processAsync(const Query &q, std::shared_ptr<UserDB::Session> session, bool includeProvenance);

	private:
		QueryProcessor(std::unique_ptr<QueryProcessorBackend> backend);
	public:
		~QueryProcessor();
		QueryProcessor(const QueryProcessor &) = delete; // no copies
		QueryProcessor(QueryProcessor &&) = delete; // no moves

	private:
		static std::unique_ptr<QueryProcessor> default_instance;
		std::unique_ptr<QueryProcessorBackend> backend;
};

#endif
