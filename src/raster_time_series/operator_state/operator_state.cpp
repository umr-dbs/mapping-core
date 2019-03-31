
#include "operator_state.h"

using namespace rts;

OperatorState::OperatorState(std::vector<std::unique_ptr<RasterTimeSeries>> &&input) : input(std::move(input)) {

}

void OperatorState::skipCurrentRaster(const uint32_t skipCount) {
    for(auto &in : input){
        in->skipCurrentRaster(skipCount);
    }
}

void OperatorState::skipCurrentTile(const uint32_t skipCount) {
    for(auto &in : input){
        in->skipCurrentTile(skipCount);
    }
}
