
#include "source_state.h"
#include "datatypes/spatiotemporal.h"
#include "operators/queryrectangle.h"
#include "raster_time_series/raster_calculations.h"

using namespace rts;

SourceState::SourceState(std::unique_ptr<SourceBackend> &&backend, const QueryRectangle &qrect, ProcessingOrder order)
        : backend(std::move(backend)), qrect(qrect), order(order), increaseDimensions(false), tileRes(this->backend->tileRes), pixelStateX(0), pixelStateY(0), currTileIndex(0), currRasterIndex(0)
{
    this->backend->initialize();
    origin = this->backend->getOrigin();
    scale.x  = (qrect.x2 - qrect.x1) / (double)qrect.xres;
    scale.y  = (qrect.y2 - qrect.y1) / (double)qrect.yres;

    auto tileCountAndPixelStart = RasterCalculations::calculateTileCount(qrect, tileRes, origin, scale);
    tileCount = tileCountAndPixelStart.first;
    rasterWorldPixelStart = tileCountAndPixelStart.second;

    setCurrTimeToFirstRaster();
}

Resolution SourceState::tileIndexToStartPixel(int tileIndex) {
    //calculate where a tile starts.
    if(tileIndex < 0 || tileIndex >= tileCount.resX * tileCount.resY)
        throw std::runtime_error("Invalid Tile index");

    //raster does not start at (0,0) if it does not align with the tiles, see explanation in nextDescriptor()
    Resolution pixelStart;
    pixelStart.resX -= rasterWorldPixelStart.resX % tileRes.resX;
    pixelStart.resY -= rasterWorldPixelStart.resY % tileRes.resY;

    for(; tileIndex > 0; --tileIndex){
        pixelStart.resX += tileRes.resX;
        if(pixelStart.resX >= qrect.xres){
            pixelStart.resX = rasterWorldPixelStart.resX % tileRes.resX;
            pixelStart.resY += tileRes.resY;
        }
    }

    return pixelStart;
}

bool SourceState::increaseSpatially() {
    backend->beforeSpatialIncrease();
    //increasing to next tile
    currTileIndex += 1;
    pixelStateX += tileRes.resX;
    if(pixelStateX >= static_cast<int>(qrect.xres)){
        //next tile is in next line
        pixelStateX = 0;
        pixelStateY += tileRes.resY;
        if(pixelStateY >= static_cast<int>(qrect.yres)){
            //end of raster reached.
            if(order == ProcessingOrder::Temporal){
                //reset only if order is temporal to get to the next raster.
                //when spatial order the end of the time series is reached, dont reset variables as end condition.
                currTileIndex = 0;
                pixelStateY = 0;
                return true;
            }
        }
    }
    return false;
}

bool SourceState::increaseTemporally() {
    backend->beforeTemporalIncrease();
    //increasing time means going to next raster.
    currRasterIndex += 1;
    backend->increaseCurrentTime(currTime);
    if(currTime >= qrect.t2 || currTime > backend->datasetEndTime){
        //end of time series or query reached.
        if(order == ProcessingOrder::Spatial){
            //reset to beginning of time series only if order is spatial and return true to move on to next tile
            //else keep time_curr above t2/dataset_end_time as end condition at top of nextDescriptor() method
            currRasterIndex = 0;
            setCurrTimeToFirstRaster();
            return true;
        }
    }
    return false;
}

void SourceState::setCurrTimeToFirstRaster() {
    //advance start point for raster, until it is not smaller than t1.
    //if currTime is already bigger than t2, the query will not return any rasters in nextDescriptor() method.
    currTime = backend->datasetStartTime;
    while(currTime < qrect.t1 && backend->getCurrentTimeEnd(currTime) <= qrect.t1){
        backend->increaseCurrentTime(currTime);
    }
}

void SourceState::skipCurrentRaster(const uint32_t skipCount) {
    bool spatiallyEndNotReached = true;

    for(int i = 0; i < skipCount && spatiallyEndNotReached; ++i) {


        if (order == ProcessingOrder::Temporal) {
            //for temporal order: reset pixel state to beginning of raster
            pixelStateX = 0;
            pixelStateY = 0;
            currTileIndex = 0;
        }
        //for both orders: advance time to next raster.
        currRasterIndex += 1;
        backend->increaseCurrentTime(currTime);
        increaseDimensions = false;

        if (order == ProcessingOrder::Spatial && currTime >= qrect.t2) {
            //next tile reached.
            currRasterIndex = 0;
            currTileIndex += 1;
            pixelStateX += tileRes.resX;
            if (pixelStateX >= qrect.xres) {
                pixelStateX = 0;
                pixelStateY += tileRes.resY;
            }
            setCurrTimeToFirstRaster();
            spatiallyEndNotReached = false;

        }
    }
}

void SourceState::skipCurrentTile(const uint32_t skipCount) {
    bool rasterEndNotReached = true;

    for(int i = 0; i < skipCount && rasterEndNotReached; ++i) {
        if (order == ProcessingOrder::Spatial) {
            //for spatial order: reset raster to first raster of rts
            currRasterIndex = 0;
            setCurrTimeToFirstRaster();
        }
        //for both orders: skip the tile by advancing pixel state to next tile
        pixelStateX += tileRes.resX;
        if (pixelStateX >= qrect.xres) {
            pixelStateX = 0;
            pixelStateY += tileRes.resY;
        }
        currTileIndex += 1;
        increaseDimensions = false;

        if (order == ProcessingOrder::Temporal && pixelStateY >= qrect.yres) {
            pixelStateX = 0;
            pixelStateY = 0;
            currTileIndex = 0;
            currRasterIndex += 1;
            backend->increaseCurrentTime(currTime);
            rasterEndNotReached = false;
        }
    }
}
