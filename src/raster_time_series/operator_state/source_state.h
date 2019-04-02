
#ifndef MAPPING_CORE_SOURCE_STATE_H
#define MAPPING_CORE_SOURCE_STATE_H


#include "raster_time_series/operator_state/operator_state.h"
#include "raster_time_series/source_backend.h"
#include "datatypes/spatiotemporal.h"
#include "operators/queryrectangle.h"

namespace rts {

    class SourceBackend;

    /**
     * The state class used by the RasterTimeSeries Source Operator and its backends.
     * This state handles the temporal and spatial information of the tile descriptors created by the source operator.
     */
    class SourceState : public rts::OperatorState {
    public:
        SourceState(std::unique_ptr <SourceBackend> &&backend, const QueryRectangle &qrect, ProcessingOrder order);
        virtual ~SourceState() = default;

        std::unique_ptr <SourceBackend> backend;

        /**
         * Increases the spatial position of the pixelState
         * @return true when increasing the spatial tile went over the end of the current raster.
         */
        bool increaseSpatially();

        /**
         * Increases the time to the next rasters starting time in the time series.
         * @return true when increasing the time to the next raster went over the end of the time series.
         */
        bool increaseTemporally();

        /**
         * In temopral order, skipCurrentRaster will set the spatial and temporal state variables to the start
         * of the next raster in temporal order. It will skip skipCount amount of rasters.
         * In spatial order, it will skip tiles of rasters. If the last raster of that tile is skipped by skipCount skips
         * it will reset to the first raster of the next tile but it does not skip any rasters of the next tile.
         */
        void skipCurrentRaster(const uint32_t skipCount) override;

        /**
         * In spatial order, skipCurrentTile will set the spatial and temporal state variables to the start of
         * the next tile and reset the the temporal state to the first raster of the output time series.
         * It will skip as many tiles as skipCount is set to.
         * In temporal order, it skips single tiles of the raster. When skipCount is so big that the end of
         * the raster is reached, it stays at the beginning of the next raster and does not skip tiles of it.
         */
        void skipCurrentTile(const uint32_t skipCount) override;

        /**
         * Calculates the pixel start position of a tile based on its index.
         * Will be used when getDescriptor(int index) is implemented.
         */
        Resolution tileIndexToStartPixel(int tileIndex);

        QueryRectangle qrect;

        const ProcessingOrder order;

        const Resolution tileRes;

        /**
         * If set true the operator will increase dimension on the next nextDescriptor() call.
         * Has to be false if the start state for the next descriptor is already set,
         * e.g. by calling intialize() or skippCurrentRaster()/-Tile().
         *
         */
        bool increaseDimensions;

        /**
         * The beginning pixel X position of the last returned tile.
         * Can be negative when the query bounding box does not align to tile positions.
         */
        int pixelStateX;
        /**
         * The beginning pixel Y position of the last returned tile.
         * Can be negative when the query bounding box does not align to tile positions.
         */
        int pixelStateY;
        /**
         * Two-dimensional count of the number of tiles in the returned rasters.
         */
        Resolution tileCount;
        /**
         * Start time of the last returned tile.
         */
        double currTime;
        /**
         * Index of the last returned tile in its raster.
         */
        int currTileIndex;
        /**
         * Index of the last returned raster of the returned time series.
         */
        int currRasterIndex;
        /**
         * The pixel position of the start of query bounding box in the extent of source raster,
         * based on the query resolution not the source rasters resolution.
         */
        Resolution rasterWorldPixelStart;
        /**
         * Contains pixel scale information of the outgoing rasters.
         */
        Scale scale;
        /**
         * The coordinate origin of the source raster.
         */
        Origin origin;
        /**
         * Sets internal currTime variable to the start of the first raster that is part of the query.
         */
        void setCurrTimeToFirstRaster();

    };

}

#endif //MAPPING_CORE_SOURCE_STATE_H
