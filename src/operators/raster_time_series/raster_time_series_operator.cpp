
#include "datatypes/raster.h"
#include "datatypes/raster/typejuggling.h"
#include "raster_time_series_operator.h"
#include "util/enumconverter.h"

#include <limits>
#include <memory>
#include <sstream>
#include <json/json.h>
#include <iostream>

using namespace rts;

RasterTimeSeriesOperator::RasterTimeSeriesOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params)
        : GenericOperator(sourcecounts, sources)
{
    assumeSources(1);

    std::string orderString = params["order"].asString();
    if(orderString == "Spatial" || orderString == "spatial"){
        order = ProcessingOrder::Spatial;
    }
    else if(orderString == "Temporal" || orderString == "temporal"){
        order = ProcessingOrder::Temporal;
    } else
        order = std::nullopt;
}

#ifndef MAPPING_OPERATOR_STUBS


std::unique_ptr<rts::RasterTimeSeries> RasterTimeSeriesOperator::getRasterTimeSeries(const QueryRectangle &rect, const QueryTools &tools) {
    if(!order)
        throw OperatorException("The operator did not have a processing order specified in the parameters, can not instantiate without passing an order.");
    return getRasterTimeSeries(rect, tools, order.value());
}

#endif
