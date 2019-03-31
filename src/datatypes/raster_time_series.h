
#ifndef MAPPING_CORE_RASTERTIMESERIES_H
#define MAPPING_CORE_RASTERTIMESERIES_H

#include "descriptor.h"
#include "raster_time_series/operator_state/operator_state.h"
#include "operators/querytools.h"
#include "operators/queryrectangle.h"

namespace rts {

    class RasterTimeSeries;
    class RasterTimeSeriesOperator;
    class OperatorState;

    /**
     * Forward iterator for the tile descriptors of a raster time series. Lifetime must not exceed the RasterTimeSeries.
     * Should only be used in a for-each loop as it alters the state of the underlying RasterTimeSeries data structure.
     */
    class TileIterator : std::iterator<
            std::forward_iterator_tag,
            Descriptor,
            std::ptrdiff_t,
            Descriptor*,
            Descriptor&>
    {
    public:
        TileIterator(RasterTimeSeries *rts);
        static TileIterator createEndIterator();
        // Get the current descriptor
        Descriptor& operator*();
        Descriptor* operator->();

        // get next descriptor
        TileIterator& operator++();

        // Comparison operators
        bool operator== (const TileIterator &other) const;
        bool operator!= (const TileIterator &other) const;
    private:
        RasterTimeSeries *rasterTimeSeriesPtr;
        //the next descriptor will be stored in the iterator an referenced from here.
        OptionalDescriptor nextDescriptor;
    };


    /**
     * RasterTimeSeries class that provides an iterator over tile descriptors of a raster time series (nextDescriptor).
     * It is created and returned by an operator. It provides the tile descriptors through the combination of
     * an operator pointer and an operator state. The nextDescriptor method returns a descriptor by calling
     * nextDescriptor on the operator and passing it the operator state.
     */
    class RasterTimeSeries : public GridSpatioTemporalResult {
    public:
        RasterTimeSeries(RasterTimeSeriesOperator *op, std::unique_ptr<OperatorState> &&state,
                         const QueryRectangle &rect, const QueryTools &tools);

        /**
         * Get the next descriptor of this raster time series from the operator based on the operator state.
         */
        OptionalDescriptor nextDescriptor();

        TileIterator begin();
        TileIterator end();

        /**
         * For temporal order this skips the tiles of the current raster that are not yet processed, advancing time
         * and next tile will be the first of the next raster.
         * For spatial order this skips the tile of the current raster, staying at the same tile index as before.
         */
        void skipCurrentRaster(const uint32_t skipCount = 1);

        /**
         * For spatial order this skips the other rasters not yet processed for the current tile. For the next tile they will appear again.
         * For temporal order this skips a single tile of the raster.
         */
        void skipCurrentTile(const uint32_t skipCount = 1);


        /**
         * Indicates how getAsRaster reacts when the returned raster is not the only or last raster of the raster time series.
         */
        enum class MoreThanOneRasterHandling {
            THROW,
            IGNORE
        };

        /**
         * Returns the next raster of the time series as a complete complete raster instead of individual tile descriptors.
         * @param moreRasterHandling Indicates how to handle if the returned raster is not the last of the rts. Standard parameter is to throw.
         * @return An assembled raster.
         */
        std::unique_ptr<GenericRaster> getAsRaster(MoreThanOneRasterHandling moreRasterHandling = MoreThanOneRasterHandling::THROW);

    private:
        RasterTimeSeriesOperator *op;
        std::unique_ptr<OperatorState> state;
        QueryRectangle rect;
        const QueryTools &tools;
    };

}

#endif //MAPPING_CORE_RASTERTIMESERIES_H
