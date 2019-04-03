
#ifndef RASTER_TIME_SERIES_RASTER_CALC_UTIL_H
#define RASTER_TIME_SERIES_RASTER_CALC_UTIL_H

#include "datatypes/spatiotemporal.h"
#include "operators/queryrectangle.h"
#include "datatypes/descriptor.h"

namespace rts {

    class RasterCalculations {
    public:

        static Resolution coordinateToPixel(const Scale& scale, const Origin &origin, double coord_x, double coord_y);

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
