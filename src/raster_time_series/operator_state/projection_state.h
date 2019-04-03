
#ifndef MAPPING_CORE_PROJECTION_STATE_H
#define MAPPING_CORE_PROJECTION_STATE_H

#include "operator_state.h"
#include "util/gdal.h"

namespace rts {

    /**
     * Class containing state information for the projection operator.
     */
    class ProjectionState : public OperatorState {
    public:
        ProjectionState(std::unique_ptr<RasterTimeSeries> &&input, GDAL::CRSTransformer &&transformer, const CrsId &src_crsId, const CrsId &dest_crsId);
        std::vector<OptionalDescriptor> currentRaster;
        uint32_t tileIndex;
        const CrsId &src_crsId;
        const CrsId &dest_crsId;
        GDAL::CRSTransformer transformer;
    };

}

#endif //MAPPING_CORE_PROJECTION_STATE_H
