#ifndef OPERATORS_OPERATOR_H
#define OPERATORS_OPERATOR_H

#include "datatypes/spatiotemporal.h"
#include "operators/provenance.h"
#include "operators/queryrectangle.h"
#include "operators/querytools.h"
#include "datatypes/raster_time_series.h"

#include <ctime>
#include <string>
#include <sstream>
#include <memory>

namespace Json {
	class Value;
}

class GenericRaster;
class PointCollection;
class LineCollection;
class PolygonCollection;
class GenericPlot;

/**
 * Base class for operators. It encapsulates cached access to results.
 *
 * Implementing operators have to provide
 *  - getRaster/(Point/Line/Polygon)Collection/Plot method, that return the result of a computation
 *    on given input data and a spatio-temporal query rectangle
 *  - semantic parameter string that gives a canonic representation of the operator
 *  - getProvenance method that attaches the source/license information of new data the operator introduces
 */
class GenericOperator {
	friend class PuzzleUtil;
	friend class GraphReorgStrategy;
	friend class QuerySpec;
	friend class rts::Descriptor;
	public:
		/*
		 * Restricts the spatial extent and resolution of a raster returned from an operator.
		 *
		 * A LOOSE result can contain pixels outside the query rectangle and can be in a
		 * resolution different from the resolution requested in the query rectangle.
		 *
		 * A EXACT raster has exactly the spatial extent and resolution requested. This can
		 * cause rescaling of the raster, so use it sparingly.
		 * The intended uses are for correlating multiple rasters (query the first one LOOSE,
		 * query the others EXACT with the sref of the first one) and for the root of the
		 * operator graph, because e.g. WMS needs to return images with just the right size and location.
		 *
		 * Note that this option only affects the spatial dimension. The temporal dimension will
		 * not be adjusted to the query rectangle under any circumstance.
		 */
		enum class RasterQM {
			EXACT,
			LOOSE
		};

		enum class FeatureCollectionQM {
			ANY_FEATURE,
			SINGLE_ELEMENT_FEATURES
		};

		static const int MAX_INPUT_TYPES = 5;
		static const int MAX_SOURCES = 20;
		static std::unique_ptr<GenericOperator> fromJSON(const std::string &json, int depth = 0);
		static std::unique_ptr<GenericOperator> fromJSON(Json::Value &json, int depth = 0);

		virtual ~GenericOperator();

		std::unique_ptr<GenericRaster> getCachedRaster(const QueryRectangle &rect, const QueryTools &tools, RasterQM query_mode = RasterQM::LOOSE);
		std::unique_ptr<PointCollection> getCachedPointCollection(const QueryRectangle &rect, const QueryTools &tools, FeatureCollectionQM query_mode = FeatureCollectionQM::ANY_FEATURE);
		std::unique_ptr<LineCollection> getCachedLineCollection(const QueryRectangle &rect, const QueryTools &tools, FeatureCollectionQM query_mode = FeatureCollectionQM::ANY_FEATURE);
		std::unique_ptr<PolygonCollection> getCachedPolygonCollection(const QueryRectangle &rect, const QueryTools &tools, FeatureCollectionQM query_mode = FeatureCollectionQM::ANY_FEATURE);
		std::unique_ptr<rts::RasterTimeSeries> getCachedRasterTimeSeries(const QueryRectangle &rect, const QueryTools &tools, rts::ProcessingOrder processingOrder, RasterQM query_mode = RasterQM::LOOSE);
	    std::unique_ptr<rts::RasterTimeSeries> getCachedRasterTimeSeries(const QueryRectangle &rect, const QueryTools &tools, RasterQM query_mode = RasterQM::LOOSE);
		std::unique_ptr<GenericPlot> getCachedPlot(const QueryRectangle &rect, const QueryTools &tools);

		std::unique_ptr<ProvenanceCollection> getFullProvenance();
		std::unique_ptr<ProvenanceCollection> getCachedFullProvenance(const QueryRectangle &rect, const QueryTools &tools);

		const std::string &getType() const { return type; }
		const std::string &getSemanticId() const { return semantic_id; }
		int getDepth() const { return depth; }

	protected:
		GenericOperator(int sourcecounts[], GenericOperator *sources[]);
		virtual void writeSemanticParameters(std::ostringstream &stream);
		virtual void getProvenance(ProvenanceCollection &pc);
		void assumeSources(int rasters, int pointcollections=0, int linecollections=0, int polygoncollections=0);

		int getRasterSourceCount() { return sourcecounts[0]; }
		int getPointCollectionSourceCount() { return sourcecounts[1]; }
		int getLineCollectionSourceCount() { return sourcecounts[2]; }
		int getPolygonCollectionSourceCount() { return sourcecounts[3]; }
		int getRasterTimeSeriesSourceCount() { return sourcecounts[4]; }
		virtual std::unique_ptr<GenericRaster> getRaster(const QueryRectangle &rect, const QueryTools &tools);
		virtual std::unique_ptr<PointCollection> getPointCollection(const QueryRectangle &rect, const QueryTools &tools);
        virtual std::unique_ptr<LineCollection> getLineCollection(const QueryRectangle &rect, const QueryTools &tools);
        virtual std::unique_ptr<PolygonCollection> getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools);
        virtual std::unique_ptr<rts::RasterTimeSeries> getRasterTimeSeries(const QueryRectangle &rect, const QueryTools &tools);
        virtual std::unique_ptr<rts::RasterTimeSeries> getRasterTimeSeries(const QueryRectangle &rect, const QueryTools &tools, rts::ProcessingOrder processingOrder);
        virtual std::unique_ptr<GenericPlot> getPlot(const QueryRectangle &rect, const QueryTools &tools);

		std::unique_ptr<GenericRaster> getRasterFromSource(int idx, const QueryRectangle &rect, const QueryTools &tools, RasterQM query_mode = RasterQM::LOOSE);
		std::unique_ptr<PointCollection> getPointCollectionFromSource(int idx, const QueryRectangle &rect, const QueryTools &tools, FeatureCollectionQM query_mode = FeatureCollectionQM::ANY_FEATURE);
		std::unique_ptr<LineCollection> getLineCollectionFromSource(int idx, const QueryRectangle &rect, const QueryTools &tools, FeatureCollectionQM query_mode = FeatureCollectionQM::ANY_FEATURE);
		std::unique_ptr<PolygonCollection> getPolygonCollectionFromSource(int idx, const QueryRectangle &rect, const QueryTools &tools, FeatureCollectionQM query_mode = FeatureCollectionQM::ANY_FEATURE);
		std::unique_ptr<rts::RasterTimeSeries> getRasterTimeSeriesFromSource(int idx, const QueryRectangle &rect, const QueryTools &tools, rts::ProcessingOrder processingOrder, RasterQM query_mode = RasterQM::LOOSE);
		// there is no getPlotFromSource, because plots are by definition the final step of a chain

	private:
		enum class ResolutionRequirement {
			REQUIRED,
			FORBIDDEN,
			OPTIONAL
		};
		void validateQRect(const QueryRectangle &rect, ResolutionRequirement res = ResolutionRequirement::OPTIONAL);
		void validateResult(const QueryRectangle &rect, SpatioTemporalResult *result);
		void getRecursiveProvenance(ProvenanceCollection &pc);

		int sourcecounts[MAX_INPUT_TYPES];
		GenericOperator *sources[MAX_SOURCES];

		std::string type;
		std::string semantic_id;
		int depth;

		void operator=(GenericOperator &) = delete;
};

class OperatorRegistration {
	public:
		OperatorRegistration(const char *name, std::unique_ptr<GenericOperator> (*constructor)(int sourcecounts[], GenericOperator *sources[], Json::Value &params));
};

#define REGISTER_OPERATOR(classname, name) static std::unique_ptr<GenericOperator> create##classname(int sourcecounts[], GenericOperator *sources[], Json::Value &params) { return std::make_unique<classname>(sourcecounts, sources, params); } static OperatorRegistration register_##classname(name, create##classname)


#endif
