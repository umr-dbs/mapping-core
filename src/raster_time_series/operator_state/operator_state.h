
#ifndef MAPPING_CORE_RTS_OPERATOR_STATE_H
#define MAPPING_CORE_RTS_OPERATOR_STATE_H

#include <memory>
#include "datatypes/raster_time_series.h"

namespace rts {

    class RasterTimeSeries;

    /**
     * Base class that contains state information for a raster time series.
     * A RasterTimeSeries object contains of an operator state and a pointer to the operator.
     * The operator creates tile descriptors based on the operator state it gets passed by the RasterTimeSeries object.
     *
     * The default class contains a vector of input RasterTimeSeries and methods for skipping the current raster or tile
     * of the time series. The skipping methods must alter the state and the state of the inputs as needed.
     */
    class OperatorState {
    public:
        OperatorState() = default;

        explicit OperatorState(std::vector<std::unique_ptr<RasterTimeSeries>> &&input);

        virtual ~OperatorState() = default;

        /**
         * Method for casting this state to a sub-class and getting a reference.
         * @tparam T Sub-class to cast this state.
         * @return A reference to an object of the sub-class T.
         */
        template<class T>
        T &castTo();

        /**
         * For temporal order this skips the tiles of the current raster that are not yet processed, advancing time
         * and next tile will be the first of the next raster.
         * For spatial order this skips the tile of the current raster, staying at the same tile index as before.
         */
        virtual void skipCurrentRaster(const uint32_t skipCount);

        /**
         * For spatial order this skips the other rasters not yet processed for the current tile. For the next tile they will appear again.
         * For temporal order this skips a single tile of the raster.
         */
        virtual void skipCurrentTile(const uint32_t skipCount);

        std::vector<std::unique_ptr<RasterTimeSeries>> input;
    };

    template<class T>
    T &OperatorState::castTo() {
        return *dynamic_cast<T *>(this);
    }

}

#endif //MAPPING_CORE_RTS_OPERATOR_STATE_H
