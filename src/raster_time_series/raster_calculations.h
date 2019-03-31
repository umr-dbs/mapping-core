
#ifndef RASTER_TIME_SERIES_RASTER_CALC_UTIL_H
#define RASTER_TIME_SERIES_RASTER_CALC_UTIL_H

#include "datatypes/spatiotemporal.h"
#include "operators/queryrectangle.h"
#include "datatypes/descriptor.h"

namespace rts {

    class RasterCalculations {
    public:
        /**
         * Calculates the pixel position of a coordinate in the given rasters extent and size.
         * @param rasterSize The pixel size of the raster.
         * @param rasterExtent The coordinate extent of the raster.
         * @param coord_x x part of the coordinate.
         * @param coord_y y part of the coordinate.
         * @return The pixel position of the coordinates.
         */
         /*
        static Resolution coordinateToPixel(const Resolution &rasterSize,
                                            const SpatialReference &rasterExtent,
                                            double coord_x,
                                            double coord_y);
        */
        /**
         * Calculates the pixel position of a coordinate in the given rasters extent and size.
         * @param rasterInfo SpatialTemporalReference defining the pixel size and coordinate extent of the raster.
         * @param coord_x x part of the coordinate.
         * @param coord_y y part of the coordinate.
         * @return The pixel position of the coordinates.
         */
        //static Resolution coordinateToPixel(const SpatioTemporalReference &rasterInfo, double coord_x, double coord_y);

        static Resolution coordinateToPixel(const Scale& scale, const Origin &origin, double coord_x, double coord_y);

        /*
        static std::pair<double,double> pixelToCoordinate(const Scale& scale, const Origin &origin, uint32_t pixelX, uint32_t pixelY);

        static std::pair<double,double> pixelToCoordinate(const Scale& scale, const Origin &origin, Resolution pixelCoord);
        */
        /**
         * Calculates the spatial coordinates of a pixel rectangle by locating it in a raster, defined by its total
         * pixel size and its coordinate extent.
         * @param rasterSize The size of the total raster.
         * @param rasterExtent The coordinate extent of the total raster.
         * @param pixelStart The smaller pixel position of the pixel rectangle.
         * @param pixelEnd The bigger pixel position of the pixel rectangle.
         * @return The coordinates of the pixel rectangle as spatial reference.
         */
        /*
        static SpatialReference pixelToSpatialRectangle(const Resolution &rasterSize,
                                                        const SpatialReference &rasterExtent,
                                                        Resolution pixelStart,
                                                        Resolution pixelEnd);
        */
        /**
         * Calculates the spatial coordinates of a pixel rectangle by locating it in a raster, defined by its total
         * pixel size and its coordinate extent.
         * @param rasterInfo SpatialTemporalReference of the total raster defining its pixel resolution and coordinate extent.
         * @param pixelStart The smaller pixel position of the pixel rectangle.
         * @param pixelEnd The bigger pixel position of the pixel rectangle.
         * @return The coordinates of the pixel rectangle as spatial reference.
         */
        /*
        static SpatialReference pixelToSpatialRectangle(const SpatioTemporalReference &rasterInfo,
                                                        Resolution pixelStart,
                                                        Resolution pixelEnd);
        */
        static SpatialReference pixelToSpatialRectangle(const CrsId &crsId,
                                                        const Scale &scale,
                                                        const Origin &origin,
                                                        Resolution pixelStart,
                                                        Resolution pixelEnd);

        static SpatialReference tileIndexToSpatialRectangle(const QueryRectangle &qrect, int tileIndex, const Resolution &tileRes);


        /**
         * Calculate the tile count of a query and the start position of the query raster in world pixel.
         * Both are Resolutions returned as a pair. The first item is the two dimensional tile count, the
         * second item is the "rasterWorldPixelStart" (how it is called in GDALSource).
         *
         * @param qrect
         * @param origin
         * @param scale
         * @return First item of the pair is the two-dimensional tile count, second item is the pixel start position in the world of the raster.
         */
        static std::pair<Resolution, Resolution> calculateTileCount(const QueryRectangle &qrect, const Resolution &tileRes, const Origin &origin, const Scale &scale);

    };

}

#endif //RASTER_TIME_SERIES_RASTER_CALC_UTIL_H
