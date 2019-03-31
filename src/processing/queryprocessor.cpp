

#include "processing/queryprocessor.h"
#include "processing/queryprocessor_backend.h"
#include "util/configuration.h"


std::unique_ptr<QueryProcessor> QueryProcessor::default_instance;

QueryProcessor &QueryProcessor::getDefaultProcessor() {
	if (default_instance == nullptr) {
		auto name = Configuration::get<std::string>("processing.backend", "local");
		default_instance = create(name, Configuration::getSubTable("processing." + name));
	}
	return *default_instance;
}

std::unique_ptr<QueryProcessor::QueryResult> QueryProcessor::process(const Query &q, bool includeProvenance) {
	auto progress = processAsync(q, includeProvenance);
	progress->wait();
	return progress->getResult();
}


std::unique_ptr<QueryProcessor::QueryProgress> QueryProcessor::processAsync(const Query &q, bool includeProvenance) {
	return backend->processAsync(q, includeProvenance);
}



QueryProcessor::QueryProcessor(std::unique_ptr<QueryProcessorBackend> backend) : backend(std::move(backend)) {

}

QueryProcessor::~QueryProcessor() {

}


/*
 * QueryResult
 */
QueryProcessor::QueryResult::QueryResult(Query::ResultType result_type, std::unique_ptr<SpatioTemporalResult> result, std::unique_ptr<ProvenanceCollection> provenance, const std::string &result_plot, boost::optional<MappingException> exception, const QueryRectangle &qrect)
	: result_type(result_type), result(std::move(result)), provenance(std::move(provenance)), result_plot(result_plot), result_exception(std::move(exception)), qrect(qrect) {
}

std::unique_ptr<QueryProcessor::QueryResult> QueryProcessor::QueryResult::raster(std::unique_ptr<GenericRaster> result, const QueryRectangle &qrect, std::unique_ptr<ProvenanceCollection> provenance) {
	return std::unique_ptr<QueryResult>(new QueryResult(Query::ResultType::RASTER, std::move(result), std::move(provenance), "", boost::none, qrect));
}
std::unique_ptr<QueryProcessor::QueryResult> QueryProcessor::QueryResult::points(std::unique_ptr<PointCollection> result, const QueryRectangle &qrect, std::unique_ptr<ProvenanceCollection> provenance) {
	return std::unique_ptr<QueryResult>(new QueryResult(Query::ResultType::POINTS, std::move(result), std::move(provenance), "", boost::none, qrect));
}
std::unique_ptr<QueryProcessor::QueryResult> QueryProcessor::QueryResult::lines(std::unique_ptr<LineCollection> result, const QueryRectangle &qrect, std::unique_ptr<ProvenanceCollection> provenance) {
	return std::unique_ptr<QueryResult>(new QueryResult(Query::ResultType::LINES, std::move(result), std::move(provenance), "", boost::none, qrect));
}
std::unique_ptr<QueryProcessor::QueryResult> QueryProcessor::QueryResult::polygons(std::unique_ptr<PolygonCollection> result, const QueryRectangle &qrect, std::unique_ptr<ProvenanceCollection> provenance) {
	return std::unique_ptr<QueryResult>(new QueryResult(Query::ResultType::POLYGONS, std::move(result), std::move(provenance), "", boost::none, qrect));
}
std::unique_ptr<QueryProcessor::QueryResult> QueryProcessor::QueryResult::rasterTimeSeries(std::unique_ptr<rts::RasterTimeSeries> result, const QueryRectangle &qrect, std::unique_ptr<ProvenanceCollection> provenance) {
	return std::unique_ptr<QueryResult>(new QueryResult(Query::ResultType::RASTER_TIME_SERIES, std::move(result), std::move(provenance), "", boost::none, qrect));
}
std::unique_ptr<QueryProcessor::QueryResult> QueryProcessor::QueryResult::plot(const std::string &plot, const QueryRectangle &qrect, std::unique_ptr<ProvenanceCollection> provenance) {
	return std::unique_ptr<QueryResult>(new QueryResult(Query::ResultType::PLOT, nullptr, std::move(provenance), plot, boost::none, qrect));
}
std::unique_ptr<QueryProcessor::QueryResult> QueryProcessor::QueryResult::error(const MappingException &exception, const QueryRectangle &qrect) {
	return std::unique_ptr<QueryResult>(new QueryResult(Query::ResultType::ERROR, nullptr, nullptr, "", exception, qrect));
}



template<typename T>
std::unique_ptr<T> cast_unique_ptr(std::unique_ptr<SpatioTemporalResult> &src) {
	if (src == nullptr)
		throw std::runtime_error("Cannot dynamic_cast a nullptr");
	T *dest = dynamic_cast<T *>(src.get());
	if (dest == nullptr)
		throw std::runtime_error("dynamic_cast<> failed");
	src.release(); // make sure src doesn't free the object

	return std::unique_ptr<T>(dest);
}

std::unique_ptr<GenericRaster> QueryProcessor::QueryResult::getRaster(GenericOperator::RasterQM query_mode) {
	if (result_type == Query::ResultType::ERROR)
		throw result_exception.value();
	if (result_type != Query::ResultType::RASTER)
		throw ProcessingException("QueryResult::getRaster(): result is not a raster", MappingExceptionType::PERMANENT);

	auto raster = cast_unique_ptr<GenericRaster>(result);
	if (query_mode == GenericOperator::RasterQM::EXACT)
		raster = raster->fitToQueryRectangle(qrect);
	return raster;
}

std::unique_ptr<PointCollection> QueryProcessor::QueryResult::getPointCollection(GenericOperator::FeatureCollectionQM query_mode) {
	if (result_type == Query::ResultType::ERROR)
		throw result_exception.value();
	if (result_type != Query::ResultType::POINTS)
		throw ProcessingException("QueryResult::getPointCollection(): result is not a PointCollection", MappingExceptionType::PERMANENT);

	auto points = cast_unique_ptr<PointCollection>(result);
	if (query_mode == GenericOperator::FeatureCollectionQM::SINGLE_ELEMENT_FEATURES && !points->isSimple())
		throw OperatorException("Operator did not return Features consisting only of single points");
	return points;
}

std::unique_ptr<LineCollection> QueryProcessor::QueryResult::getLineCollection(GenericOperator::FeatureCollectionQM query_mode) {
	if (result_type == Query::ResultType::ERROR)
		throw result_exception.value();
	if (result_type != Query::ResultType::LINES)
		throw ProcessingException("QueryResult::getLineCollection(): result is not a LineCollection", MappingExceptionType::PERMANENT);

	auto lines = cast_unique_ptr<LineCollection>(result);
	if (query_mode == GenericOperator::FeatureCollectionQM::SINGLE_ELEMENT_FEATURES && !lines->isSimple())
		throw OperatorException("Operator did not return Features consisting only of single lines");
	return lines;
}

std::unique_ptr<PolygonCollection> QueryProcessor::QueryResult::getPolygonCollection(GenericOperator::FeatureCollectionQM query_mode) {
	if (result_type == Query::ResultType::ERROR)
		throw result_exception.value();
	if (result_type != Query::ResultType::POLYGONS)
		throw ProcessingException("QueryResult::getPolygonCollection(): result is not a PolygonCollection", MappingExceptionType::PERMANENT);

	auto polygons = cast_unique_ptr<PolygonCollection>(result);
	if (query_mode == GenericOperator::FeatureCollectionQM::SINGLE_ELEMENT_FEATURES && !polygons->isSimple())
		throw OperatorException("Operator did not return Features consisting only of single polygons");
	return polygons;
}

std::unique_ptr<SimpleFeatureCollection> QueryProcessor::QueryResult::getAnyFeatureCollection(GenericOperator::FeatureCollectionQM query_mode) {
	if (result_type == Query::ResultType::ERROR)
		throw result_exception.value();
	if (result_type != Query::ResultType::POINTS && result_type != Query::ResultType::LINES && result_type != Query::ResultType::POLYGONS)
		throw ProcessingException("QueryResult::getAnyFeatureCollection(): result is not a SimpleFeatureCollection", MappingExceptionType::PERMANENT);

	auto features = cast_unique_ptr<SimpleFeatureCollection>(result);
	if (query_mode == GenericOperator::FeatureCollectionQM::SINGLE_ELEMENT_FEATURES && !features->isSimple())
		throw OperatorException("Operator did not return Features consisting only of single features");
	return features;
}

std::unique_ptr<rts::RasterTimeSeries> QueryProcessor::QueryResult::getRasterTimeSeries(GenericOperator::RasterQM query_mode) {
	if (result_type == Query::ResultType::ERROR)
		throw result_exception.value();
	if (result_type != Query::ResultType::RASTER_TIME_SERIES)
		throw ProcessingException("QueryResult::getRasterTimeSeries(): result is not a raster time series", MappingExceptionType::PERMANENT);

	auto rts = cast_unique_ptr<rts::RasterTimeSeries>(result);

	return rts;
}

std::string QueryProcessor::QueryResult::getPlot() {
	if (result_type == Query::ResultType::ERROR)
		throw result_exception.value();
	if (result_type != Query::ResultType::PLOT)
		throw ProcessingException("QueryResult::getPlot(): result is not a plot", MappingExceptionType::PERMANENT);

	return std::move(result_plot);
}

ProvenanceCollection& QueryProcessor::QueryResult::getProvenance() {
	if (provenance.get() == nullptr) {
		throw ProcessingException("QueryProcessor: getProvenance not available", MappingExceptionType::PERMANENT);
	}
	return *provenance;
}

bool QueryProcessor::QueryResult::isError() {
	return result_type == Query::ResultType::ERROR;
}

boost::optional<MappingException> QueryProcessor::QueryResult::getErrorException(){
	return result_exception;
}

