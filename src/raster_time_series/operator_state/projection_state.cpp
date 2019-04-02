
#include "projection_state.h"

using namespace rts;

ProjectionState::ProjectionState(std::unique_ptr<RasterTimeSeries> &&input, GDAL::CRSTransformer &&transformer, const CrsId &src_crsId, const CrsId &dest_crsId)
        : OperatorState(), transformer(std::move(transformer)), src_crsId(src_crsId), dest_crsId(dest_crsId), tileIndex(0)
{
    this->input.emplace_back(std::move(input));
}
