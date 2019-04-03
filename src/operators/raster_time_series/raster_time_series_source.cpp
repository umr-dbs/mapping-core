
#include "raster_time_series_operator.h"
#include "raster_time_series/source_backend.h"
#include "raster_time_series/operator_state/source_state.h"
#include <iostream>

using namespace rts;

/**
 * Source operator for raster time series. It provides common functionality like handling the temporal and spatial state
 * for creating the tile descriptors. Actual functionality of loading tile descriptors must be implemented in classes
 * inheriting from rts::SourceBackend. The OperatorState for the SourceOperator is implemented in rts::SourceState.
 *
 * Parameters:
 *  - "backend" : name of the backend to use (e.g. "gdal_source").
 *  - "tile_res" : json array containing two integer values for the width and height of the tile resolution.
 *  - additional parameters needed by the backend.
 */
class RasterTimeSeriesSource : public RasterTimeSeriesOperator {
public:
    RasterTimeSeriesSource(int sourcecounts[], GenericOperator *sources[], Json::Value &params);
    ~RasterTimeSeriesSource() override = default;
    void getProvenance(ProvenanceCollection &pc) override;

#ifndef MAPPING_OPERATOR_STUBS
    std::unique_ptr<RasterTimeSeries> getRasterTimeSeries(const QueryRectangle &rect, const QueryTools &tools, ProcessingOrder processingOrder) override;
#endif

    OptionalDescriptor nextDescriptor(OperatorState &state, const QueryRectangle &rect, const QueryTools &tools) override;
protected:
    void writeSemanticParameters(std::ostringstream &stream) override;
    bool supportsProcessingOrder(ProcessingOrder processingOrder) const override;
private:
    std::string backendName;
    Json::Value params;
};


//The actual source operator:
REGISTER_OPERATOR(RasterTimeSeriesSource, "raster_time_series_source");

RasterTimeSeriesSource::RasterTimeSeriesSource(int *sourcecounts, GenericOperator **sources, Json::Value &params)
        : RasterTimeSeriesOperator(sourcecounts, sources, params), params(params), backendName(params["backend"].asString())
{

}

void RasterTimeSeriesSource::getProvenance(ProvenanceCollection &pc) {
    //TODO: this is a point of friction because the provenance data must come from the backend but i don't have
    // access to the backend here (it is in the OperatorState).
    // Solution idea: make backend statically available by the backend name?
}

void RasterTimeSeriesSource::writeSemanticParameters(std::ostringstream &stream) {
    Json::Value params;
    params["sourcename"] = backendName;
    //TODO: similar problem to getProvenance: the info must come from the backend but it is
    // associated with the OperatorState, not with this operator.
    // Solution idea: make backend statically available by the backend name?
    Json::FastWriter writer;
    stream << writer.write(params);
}


OptionalDescriptor RasterTimeSeriesSource::nextDescriptor(OperatorState &state, const QueryRectangle &rect, const QueryTools &tools) {

    auto &s = state.castTo<SourceState>();

    //increaseDimensions will be true normally.
    //Except at the start when first raster will be returned and variables are set correctly or
    //skipCurrentRaster/Tile is called it will also be set already.
    //else it must be increased here and for next tile increaseDimensions will be set true.
    if(s.increaseDimensions){
        if(order == ProcessingOrder::Temporal){
            if(s.increaseSpatially()){
                s.increaseTemporally();
            }
        } else if(order == ProcessingOrder::Spatial){
            if(s.increaseTemporally()){
                s.increaseSpatially();
            }
        }
    }
    s.increaseDimensions = true;

    //if currTime is bigger than query or dataset and not reset to beginning, the end of time series is reached.
    if(s.currTime >= rect.t2 || s.currTime > s.backend->datasetEndTime){
        return std::nullopt;
    }

    //if currTileIndex is bigger than number of tiles in raster and not reset to 0, the end of time series is reached.
    if(s.currTileIndex >= s.tileCount.resX * s.tileCount.resY){
        return std::nullopt;
    }

    // starting a new raster sets pixelState variables to 0, but when raster does not start at sources origin,
    // the tile will start in negative space of the output raster. So subtract the starting pixel in world space
    // modulo the tile res to get the starting pixels in the tile that will be returned.
    if(s.pixelStateX == 0) {
        s.pixelStateX -= s.rasterWorldPixelStart.resX % s.tileRes.resX;
    }
    if(s.pixelStateY == 0) {
        s.pixelStateY -= s.rasterWorldPixelStart.resY % s.tileRes.resY;
    }

    auto ret = s.backend->createDescriptor(s, this, tools); //currTime, pixelStateX, pixelStateY, currTileIndex, rasterWorldPixelStart, scale, origin, tileCount);

    return ret;

}

#ifndef MAPPING_OPERATOR_STUBS

std::unique_ptr<RasterTimeSeries> RasterTimeSeriesSource::getRasterTimeSeries(const QueryRectangle &rect, const QueryTools &tools, ProcessingOrder processingOrder) {

    auto backend = rts::SourceBackend::create(backendName, rect, params);

    if(!supportsProcessingOrder(processingOrder) || !backend->supportsOrder(processingOrder))
        throw OperatorException(concat("RasterTimeSeries Source Operator does not support the processing order: ", processingOrder == rts::ProcessingOrder::Temporal ? "Temporal." : "Spatial."));

    auto state = std::make_unique<SourceState>(std::move(backend), rect, processingOrder);

    return std::make_unique<RasterTimeSeries>(this, std::move(state), rect, tools);
}

bool RasterTimeSeriesSource::supportsProcessingOrder(ProcessingOrder processingOrder) const {
    return processingOrder == ProcessingOrder::Temporal || processingOrder == ProcessingOrder::Spatial;
}

#endif
