
#ifndef MAPPING_CORE_RASTER_TIME_SERIES_OPERATOR_H
#define MAPPING_CORE_RASTER_TIME_SERIES_OPERATOR_H

#include "operators/operator.h"
#include "datatypes/descriptor.h"
#include "datatypes/raster_time_series.h"

class GenericOperator;

namespace rts {

    class Descriptor;
    class RasterTimeSeries;
    class TileIterator;

    /**
     * Abstract base class for operators that can return a RasterTimeSeries.
     * It implements the version getRasterTimeSeries
     */
    class RasterTimeSeriesOperator : public GenericOperator {
        friend class RasterTimeSeries;
    public:

        RasterTimeSeriesOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);
        virtual ~RasterTimeSeriesOperator() = default;

#ifndef MAPPING_OPERATOR_STUBS
        /**
         * This version of the getRasterTimeSeries method reads the processing order from the operator parameters and
         * calls the other version with it.
         * It does not have to be implemented by an explicit operator, the second version is enough.
         */
        std::unique_ptr<rts::RasterTimeSeries> getRasterTimeSeries(const QueryRectangle &rect, const QueryTools &tools) override;

        /**
         * Second version of the getRasterTimeSeries method. It has the processing order as a parameter and creates
         * a unique RasterTimeSeries object that can be used to iterate over descriptor tiles.
         * Must be implemented by raster time series operators.
         */
        std::unique_ptr<rts::RasterTimeSeries> getRasterTimeSeries(const QueryRectangle &rect, const QueryTools &tools, ProcessingOrder processingOrder) override = 0;
#endif

    protected:

        /**
         * The method of the operator that creates tile descriptors. The state for creating the descriptors is not
         * handled by the operator itself but a separate OperatorState object to allow multiple queries to an operator.
         * This method is called by the RasterTimeSeries object created by the getRasterTimeSeries method.
         * Must be implemented by raster time series operators.
         * @return The next tile descriptor of the raster time series.
         */
        virtual OptionalDescriptor nextDescriptor(OperatorState &state, const QueryRectangle &rect, const QueryTools &tools) = 0;

        /**
         * A method that indicates if the operator supports a tile processing order.
         * @param processingOrder The processing order in question.
         * @return If the operator supports the passed processing order.
         */
        virtual bool supportsProcessingOrder(ProcessingOrder processingOrder) const = 0;

        /**
         * As the parameters of the operator are not available anymore in the getRasterTimeSeries method, this
         * base class reads the potential order value in the constructor and stores it as an optional value.
         */
        std::optional<ProcessingOrder> order;
    };

}

#endif //MAPPING_CORE_RASTER_TIME_SERIES_OPERATOR_H
