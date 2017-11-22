
#include "datatypes/raster.h"
#include "datatypes/raster/raster_priv.h"
#include "operators/operator.h"
#include "raster/opencl.h"
#include "datatypes/polygoncollection.h"
#include "util/rasterize_polygons.h"

#include <json/json.h>
#include <algorithm>
#include <cassert>

/**
 * Operator that rasterizes polygon collections.
 */
class RasterizePolygonOperator : public GenericOperator {
    public:
        RasterizePolygonOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);

        ~RasterizePolygonOperator() override;

#ifndef MAPPING_OPERATOR_STUBS

        auto getRaster(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<GenericRaster> override;

#endif

    protected:
        auto writeSemanticParameters(std::ostringstream &stream) -> void override;

    private:
};


RasterizePolygonOperator::RasterizePolygonOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params)
        : GenericOperator(sourcecounts, sources) {
    assumeSources(0, 0, 0, 1);
}

RasterizePolygonOperator::~RasterizePolygonOperator() = default;

REGISTER_OPERATOR(RasterizePolygonOperator, "rasterize_polygon");

void RasterizePolygonOperator::writeSemanticParameters(std::ostringstream &stream) {
    stream << "{}";
}

#ifndef MAPPING_OPERATOR_STUBS

auto RasterizePolygonOperator::getRaster(const QueryRectangle &rect,
                                         const QueryTools &tools) -> std::unique_ptr<GenericRaster> {
    // strip resolution from vector query
    QueryRectangle vector_rect{rect, rect, QueryResolution::none()};
    const auto polygon_collection = getPolygonCollectionFromSource(0, vector_rect, tools);

    RasterizePolygons rasterizePolygons {rect, *polygon_collection};

    return rasterizePolygons.get_raster();
}

#endif
