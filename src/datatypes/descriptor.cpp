
#include <iostream>
#include <cmath>
#include <cache/manager.h>

#include "descriptor.h"
#include "operators/operator.h"

using namespace rts;

// DescriptorInfo:

DescriptorInfo::DescriptorInfo(const TemporalReference &temporalInfo,
                               const SpatialReference &rasterSpatialInfo,
                               const SpatialReference &tileSpatialInfo,
                               const Resolution &rasterResolution,
                               const Resolution &tileResolution,
                               ProcessingOrder order,
                               uint32_t tileIndex,
                               Resolution rasterTileCountDimensional,
                               double nodata,
                               GDALDataType dataType,
                               GenericOperator *op)
        : temporalInfo(temporalInfo),
          rasterSpatialInfo(rasterSpatialInfo),
          tileSpatialInfo(tileSpatialInfo),
          rasterResolution(rasterResolution),
          tileResolution(tileResolution),
          order(order),
          tileIndex(tileIndex),
          rasterTileCountDimensional(rasterTileCountDimensional),
          rasterTileCount(rasterTileCountDimensional.resX * rasterTileCountDimensional.resY),
          nodata(nodata),
          dataType(dataType),
          op(op)
{

}

DescriptorInfo::DescriptorInfo(const OptionalDescriptor &desc, GenericOperator *newOperator)
        : temporalInfo(desc->temporalInfo),
          order(desc->order),
          rasterSpatialInfo(desc->rasterSpatialInfo),
          tileSpatialInfo(desc->tileSpatialInfo),
          rasterResolution(desc->rasterResolution),
          tileResolution(desc->tileResolution),
          tileIndex(desc->tileIndex),
          rasterTileCountDimensional(desc->rasterTileCountDimensional),
          rasterTileCount(desc->rasterTileCount),
          nodata(desc->nodata),
          _isOnlyNodata(desc->_isOnlyNodata),
          dataType(desc->dataType),
          op(newOperator) //assuming this copies an input descriptor, the operator pointer should not be copied.
{

}

bool DescriptorInfo::isOnlyNodata() const {
    return _isOnlyNodata;
}

// Descriptor:

Descriptor::Descriptor(std::function<UniqueRaster(const Descriptor&)> &&getter,
                       const TemporalReference &temporalInfo,
                       const SpatialReference &rasterSpatialInfo,
                       const SpatialReference &tileSpatialInfo,
                       const Resolution &rasterResolution,
                       const Resolution &tileResolution,
                       ProcessingOrder order,
                       uint32_t tileIndex,
                       Resolution rasterTileCountDimensional,
                       double nodata,
                       GDALDataType dataType,
                       GenericOperator *op)
        : getter(std::move(getter)),
          DescriptorInfo(temporalInfo, rasterSpatialInfo, tileSpatialInfo, rasterResolution, tileResolution, order, tileIndex, rasterTileCountDimensional, nodata, dataType, op)
{

}

std::unique_ptr<GenericRaster> Descriptor::getRaster() const {
    return getter(*this);
}


QueryRectangle Descriptor::getAsQueryRectangle() const {

    return QueryRectangle(
            tileSpatialInfo,
            temporalInfo,
            QueryResolution(QueryResolution::Type::PIXELS, tileResolution.resX, tileResolution.resY)
            );
}

std::unique_ptr<GenericRaster> Descriptor::getRasterCached() const {

    QueryRectangle rect = getAsQueryRectangle();

    QueryProfiler parent_profiler; // = tools.profiler; TODO: this profiler clock is already started.
    QueryProfilerSimpleGuard parent_guard(parent_profiler);

    op->validateQRect(rect, GenericOperator::ResolutionRequirement::REQUIRED);

    auto &cache = CacheManager::get_instance().get_raster_cache();
    std::unique_ptr<GenericRaster> result;

    try {
        result = cache.query( *op, rect, parent_profiler );
    }
    catch ( NoSuchElementException &nse ) {
        QueryProfilerStoppingGuard stop_guard(parent_profiler);
        QueryProfiler exec_profiler;
        {
            QueryProfilerRunningGuard guard(parent_profiler, exec_profiler);
            TIME_EXEC("Operator.getRaster");
            result = getRaster();
        }
        //TODO: not accessible here. could put the function into the operator class, curr. it is only in the operator.cpp
        //d_profile(op->depth, op->type, "raster", exec_profiler);
        if ( cache.put(op->getSemanticId(), result, rect, exec_profiler) ) {
            parent_profiler.cached(exec_profiler);
        }
    }
    op->validateResult(rect, result.get());
    result = result->fitToQueryRectangle(rect);
    return result;

}

std::optional<Descriptor> Descriptor::createNodataDescriptor(const TemporalReference &temporalInfo,
                                                             const SpatialReference &rasterSpatialInfo,
                                                             const SpatialReference &tileSpatialInfo,
                                                             const Resolution &rasterResolution,
                                                             const Resolution &tileResolution,
                                                             ProcessingOrder order,
                                                             uint32_t tileIndex,
                                                             Resolution rasterTileCountDimensional,
                                                             double nodata,
                                                             GDALDataType dataType,
                                                             GenericOperator *op)
{
    auto getter = [](const Descriptor &self) -> UniqueRaster {
        //TODO: implement
        //UniqueRaster raster = Raster::createRaster(self.dataType, self.tileResolution);
        //set all values to nodata with the AllValuesSetter
        //RasterOperations::callUnary<RasterOperations::AllValuesSetter>(raster.get(), self.nodata);
        return nullptr;
    };
    auto ret = std::make_optional<Descriptor>(std::move(getter), temporalInfo, rasterSpatialInfo, tileSpatialInfo, rasterResolution, tileResolution, order, tileIndex, rasterTileCountDimensional, nodata, dataType, op);
    ret->_isOnlyNodata = true;
    return ret;
}

Descriptor::Descriptor(std::function<UniqueRaster(const Descriptor &)> &&getter, const DescriptorInfo &args)
        : getter(std::move(getter)), DescriptorInfo(args)
{

}


// Definition of Resolution, Origin, Scale classes:

Resolution::Resolution() : resX(0), resY(0) {

}

Resolution::Resolution(int resX, int resY) : resX(resX), resY(resY) {

}

Resolution Resolution::operator+(const Resolution &other) const {
    return Resolution(this->resX + other.resX, this->resY + other.resY);
}

Resolution Resolution::operator-(const Resolution &other) const {
    return Resolution(this->resX - other.resX, this->resY - other.resY);
}


Scale::Scale() : x(0), y(0) {

}

Scale::Scale(double x, double y) : x(x), y(y) {

}
