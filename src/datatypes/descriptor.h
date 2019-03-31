
#ifndef MAPPING_CORE_DESCRIPTOR_H
#define MAPPING_CORE_DESCRIPTOR_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <gdal.h>
#include <boost/optional.hpp>

#include "datatypes/raster.h"

class GenericOperator;
class QueryTools;

namespace rts {

    class Descriptor;

    using UniqueRaster = std::unique_ptr<GenericRaster>;

    /**
     * A class representing the resolution of a raster or tile. The integer fields are signed integers because
     * negative values can be used in the context of tiles that cover space outside of the actual raster
     * because of the fixed tile alignment at the projections origin.
     */
    class Resolution {
    public:
        Resolution();
        Resolution(int resX, int resY);

        Resolution operator+(const Resolution &other) const;
        Resolution operator-(const Resolution &other) const;

        int resX;
        int resY;
    };

    /**
     * Class for representing pixel scale in two elements (x and y).
     */
    class Scale {
    public:
        Scale();

        Scale(double x, double y);

        double x, y;

    };

    /**
     * Alias for the Scale class representing the origin of a raster.
     */
    using Origin = Scale;


    /**
    * The order of returning the tile descriptors by an operator.
    * @Spatial Operator returns one tile for raster, then advances to the next tile and starts with the first raster again.
    * @Temporal Operator returns all tiles from a raster, then advances to return the tiles of the next raster, etc.
    */
    enum class ProcessingOrder {
        Spatial = 1,
        Temporal = 2
    };

    /**
    * A class for the metadata saved in a Descriptor. The Descriptor class inherits DescriptorInfo.
    * This allows only copying the metadata of a descriptor because the getRaster closure stored in the
    * descriptor class can not be copied.
    */
    class DescriptorInfo {
    public:
        DescriptorInfo(const TemporalReference &temporalInfo,
                       const SpatialReference &rasterSpatialInfo,
                       const SpatialReference &tileSpatialInfo,
                       const Resolution &rasterResolution,
                       const Resolution &tileResolution,
                       ProcessingOrder order,
                       uint32_t tileIndex,
                       Resolution rasterTileCountDimensional,
                       double nodata,
                       GDALDataType dataType,
                       GenericOperator *op);

        /**
         * Constructor to copy the metadata of an existing descriptor. The operator pointer has to be changed explicitly
         * so that it is not forgotten when copying the metadata of an input descriptor.
         * @param desc The tile descriptor to copy the metadata from.
         * @param newOperator Pointer to the operator creating the copy.
         */
        explicit DescriptorInfo(const std::optional<Descriptor> &desc, GenericOperator *newOperator);

        DescriptorInfo(const DescriptorInfo &desc) = default;

        DescriptorInfo &operator=(const DescriptorInfo &desc) = default;

        /**
         * The order in which the tiles are processed.
         */
        ProcessingOrder order;

        /**
         * The spatial extent and projection of the complete raster.
         */
        SpatialReference rasterSpatialInfo;

        /**
         * The spatial extent and projection of the described tile. This are the real coordinates of the tile, not
         * just for its valid data. Thus it can exceed the spatial extent of the complete raster.
         */
        SpatialReference tileSpatialInfo;

        /**
         * The temporal validity of both the complete raster and the tile.
         */
        TemporalReference temporalInfo;

        /**
         * The resolution of the described tile.
         */
        Resolution tileResolution;

        /**
         * The resolution of the complete raster.
         */
        Resolution rasterResolution;

        /**
         * Which tile of the raster this descriptor describes. For spatial ordering it also describes the tile of
         * the raster, not which tile of the the time series is described.
         */
        uint32_t tileIndex;

        /**
         * The number of tiles of the complete raster.
         */
        uint32_t rasterTileCount;

        /**
         * The number of tiles of the complete raster in each dimension. rasterTileCount is
         * the product of both dimensional fields.
         */
        Resolution rasterTileCountDimensional;

        /**
         * The nodata value. A double can contain values for all different dataTypes.
         */
        double nodata;

        /**
         * The data type of the tile.
         */
        GDALDataType dataType;

        /**
         * @return If all the data in this tile is nodata.
         */
        bool isOnlyNodata() const;

    protected:
        bool _isOnlyNodata;
        GenericOperator *op;
    };

    /**
     * Core class for descriptors, containing metadata about a tile through inheritance of DescriptorInfo and
     * a std::function that is used to load the raster in getRaster().
     */
    class Descriptor : public DescriptorInfo {
    public:
        Descriptor(std::function<UniqueRaster(const Descriptor &)> &&getter,
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
                   GenericOperator *op);

        Descriptor(std::function<UniqueRaster(const Descriptor &)> &&getter,
                   const DescriptorInfo &args);

        Descriptor(const Descriptor &other) = default;

        Descriptor &operator=(const Descriptor &other) = default;

        /**
         * Creates a descriptor that is only nodata. Adds a getter that fills the raster only with nodata.
         * Can be used for tile descriptors that do not contain any valid data.
         */
        static std::optional<Descriptor> createNodataDescriptor(const TemporalReference &temporalInfo,
                                                                const SpatialReference &rasterSpatialInfo,
                                                                const SpatialReference &tileSpatialInfo,
                                                                const Resolution &rasterResolution,
                                                                const Resolution &tileResolution,
                                                                ProcessingOrder order,
                                                                uint32_t tileIndex,
                                                                Resolution rasterTileCountDimensional,
                                                                double nodata,
                                                                GDALDataType dataType,
                                                                GenericOperator *op);

        /**
         * Call the saved getter closure to create the raster/tile described by this descriptor. Called multiple times
         * it would always create a new unique raster.
         * @return A unique pointer to the raster described by this descriptor.
         */
        std::unique_ptr<GenericRaster> getRaster() const;

        std::unique_ptr<GenericRaster> getRasterCached() const;

    private:
        /**
         * Member variable to save the getRaster closure created by the operator that created this descriptor.
         * The std::function saves the captured values of the closure in dynamic memory. Therefore moving a descriptor
         * is preferred when possible.
         */
        std::function<UniqueRaster(const Descriptor &)> getter;

        /**
         * Returns the information about the tile described by this descriptor as a query rectangle, used for caching.
         */
        QueryRectangle getAsQueryRectangle() const;
    };

    using OptionalDescriptor = std::optional<Descriptor>;
    using OptionalDescriptorVector = std::vector<std::optional<Descriptor>>;

}

#endif //MAPPING_CORE_DESCRIPTOR_H
