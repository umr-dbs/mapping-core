
#include "raster_time_series.h"
#include "operators/raster_time_series/raster_time_series_operator.h"
#include "raster_time_series/operator_state/operator_state.h"
#include "raster_time_series/raster_calculations.h"
#include "datatypes/raster/typejuggling.h"

using namespace rts;

RasterTimeSeries::RasterTimeSeries(RasterTimeSeriesOperator *op, std::unique_ptr<OperatorState> &&state, const QueryRectangle &rect, const QueryTools &tools)
        : GridSpatioTemporalResult(SpatioTemporalReference(rect, rect), rect.xres, rect.yres), op(op), state(std::move(state)), rect(rect), tools(tools)
{

}

OptionalDescriptor RasterTimeSeries::nextDescriptor() {
    return op->nextDescriptor(*state, rect, tools);
}

TileIterator RasterTimeSeries::begin() {
    return TileIterator(this);
}

TileIterator RasterTimeSeries::end() {
    return TileIterator::createEndIterator();
}

void RasterTimeSeries::skipCurrentRaster(const uint32_t skipCount) {
    state->skipCurrentRaster(skipCount);
}

void RasterTimeSeries::skipCurrentTile(const uint32_t skipCount) {
    state->skipCurrentTile(skipCount);
}

template<class T1, class T2>
struct RasterWriter {
    /**
     * Simply writes a tile into the complete raster
     */
    static void execute(Raster2D<T1> *complete, Raster2D<T2> *tile, Resolution startIn, Resolution endIn, Resolution startOut){
        for(int y = startIn.resY; y <= endIn.resY; ++y){
            for(int x = startIn.resX; x <= endIn.resX; ++x){
                complete->setSafe(startOut.resX + x - startIn.resX, startOut.resY + y - startIn.resY, tile->getSafe(x, y));
            }
        }
    }
};

std::unique_ptr<GenericRaster> RasterTimeSeries::getAsRaster(MoreThanOneRasterHandling moreRasterHandling) {

    //TODO: support spatial tile order. (would require storing skipped descriptors when more than one raster should be returned)

    auto input = this->nextDescriptor();
    if(!input)
        return nullptr;

    const int count = input->rasterTileCount;
    std::vector<OptionalDescriptor> descriptors;
    descriptors.reserve(count);
    descriptors.push_back(std::move(input));

    for (int i = 1; i < count; ++i) {
        descriptors.emplace_back(this->nextDescriptor());
    }

    //throw exception if the MoreThanOneRasterHandling is set to THROW. it only expects one raster from the time series.
    if(moreRasterHandling == MoreThanOneRasterHandling::THROW){
        if(this->nextDescriptor()){
            //if the operator contains another descriptor, it contains more than one raster
            throw OperatorException("The raster time series for this query rectangle covers more than one raster.");
        }
    }

    auto out = GenericRaster::create(DataDescription(descriptors[0]->dataType, Unit::unknown()), rect, rect.xres, rect.yres);
    Resolution startOut(0, 0);
    Scale pixelScale((rect.x2 - rect.x1) / rect.xres, (rect.y2 - rect.y1) / rect.yres);

    for(int i = 0; i < count; ++i){
        auto &desc = descriptors[i];
        UniqueRaster tile = desc->getRasterCached();

        //calc start and end pixel
        Resolution startIn(0,0);
        Resolution endIn = desc->tileResolution;
        if(desc->tileSpatialInfo.x1 < desc->rasterSpatialInfo.x1 || desc->tileSpatialInfo.y1 < desc->rasterSpatialInfo.y1)
        {
            startIn = RasterCalculations::coordinateToPixel(pixelScale,
                    Origin(desc->tileSpatialInfo.x1, desc->tileSpatialInfo.y1),
                    desc->tileSpatialInfo.x1 < desc->rasterSpatialInfo.x1 ? desc->rasterSpatialInfo.x1 : desc->tileSpatialInfo.x1,
                    desc->tileSpatialInfo.y1 < desc->rasterSpatialInfo.y1 ? desc->rasterSpatialInfo.y1 : desc->tileSpatialInfo.y1);
        }
        if(desc->tileSpatialInfo.x2 > desc->rasterSpatialInfo.x2 || desc->tileSpatialInfo.y2 > desc->rasterSpatialInfo.y2)
        {
            endIn = RasterCalculations::coordinateToPixel(pixelScale,
                    Origin(desc->tileSpatialInfo.x1, desc->tileSpatialInfo.y1),
                    desc->tileSpatialInfo.x2 > desc->rasterSpatialInfo.x2 ? desc->rasterSpatialInfo.x2 : desc->tileSpatialInfo.x2,
                    desc->tileSpatialInfo.y2 > desc->rasterSpatialInfo.y2 ? desc->rasterSpatialInfo.y2 : desc->tileSpatialInfo.y2);
        }


        callBinaryOperatorFunc<RasterWriter>(out.get(), tile.get(), startIn, endIn, startOut);

        //advance pixel start position for out raster.
        startOut.resX += endIn.resX - startIn.resX;
        if(startOut.resX >= rect.xres){
            startOut.resX = 0;
            startOut.resY += endIn.resY - startIn.resY;
        }
    }

    return out;
}


// Tile Iterator:

TileIterator::TileIterator(RasterTimeSeries *rts) : rasterTimeSeriesPtr(rts) {
    nextDescriptor = rasterTimeSeriesPtr == nullptr ? std::nullopt : rasterTimeSeriesPtr->nextDescriptor();
}

TileIterator TileIterator::createEndIterator() {
    return TileIterator(nullptr);
}

Descriptor& TileIterator::operator*() {
    return *nextDescriptor;
}

Descriptor* TileIterator::operator->() {
    return &(*nextDescriptor);
}

TileIterator& TileIterator::operator++() {
    nextDescriptor = rasterTimeSeriesPtr == nullptr ? std::nullopt : rasterTimeSeriesPtr->nextDescriptor();
    return *this;
}

bool TileIterator::operator== (const TileIterator &other) const {
    if(!nextDescriptor && !other.nextDescriptor)
        return true;
    if(!nextDescriptor || !other.nextDescriptor)
        return false;
    if(other.rasterTimeSeriesPtr != rasterTimeSeriesPtr)
        return false;
    return nextDescriptor->temporalInfo.t1 == other.nextDescriptor->temporalInfo.t1 &&
           nextDescriptor->temporalInfo.t2 == other.nextDescriptor->temporalInfo.t2 &&
           nextDescriptor->tileIndex     == other.nextDescriptor->tileIndex;
}

bool TileIterator::operator!= (const TileIterator &other) const {
    return !(*this == other);
}