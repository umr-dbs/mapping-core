#ifndef MAPPING_CORE_RASTERIZE_POLYGONS_H
#define MAPPING_CORE_RASTERIZE_POLYGONS_H


#include "datatypes/polygoncollection.h"
#include "datatypes/raster/raster_priv.h"
#include "operators/operator.h"

/**
 * A class for the rasterization of a set of polygons.
 */
class RasterizePolygons {
    public:
        /**
         * Create a raster of a polygon collection.
         * @param rect
         * @param polygon_collection
         */
        RasterizePolygons(const QueryRectangle &rect, const PolygonCollection &polygon_collection);

        /**
         * Create a raster of a polygon feature.
         * @param rect
         * @param polygon_feature
         */
        RasterizePolygons(const QueryRectangle &rect,
                          const PolygonCollection::PolygonFeatureReference<const PolygonCollection> &polygon_feature);

        /**
         * Return the raster in boolean format (0 is no data).
         * @return a byte raster with zeros and ones.
         */
        auto get_raster() const -> std::unique_ptr<Raster2D<uint8_t>>;

    private:
        /**
         * A line that consists of two point, ordered by y-coordinate.
         */
        class Line {
            public:
                /**
                 * Construct a line by specifying the lower and the upper point.
                 * There is a check that swaps the points if necessary.
                 *
                 * @param upper_x x-coordinate of upper point
                 * @param upper_y y-coordinate of upper point
                 * @param lower_x x-coordinate of lower point
                 * @param lower_y y-coordinate of lower point
                 */
                Line(uint32_t upper_x, uint32_t upper_y, uint32_t lower_x, uint32_t lower_y);

                /**
                 * Slope of the line
                 * @return slope
                 */
                auto slope() const -> double;

                /**
                 * Inverse slope of the line
                 * @return inverse slope
                 */
                auto inverse_slope() const -> double;

                /**
                 * @return minimum x-coordinate of the line
                 */
                auto min_x() const -> uint32_t;

                /**
                 * @return maximum x-coordinate of the line
                 */
                auto max_x() const -> uint32_t;

                /**
                 * x-coordinate of upper point
                 */
                uint32_t upper_x;
                /**
                 * y-coordinate of upper point
                 */
                uint32_t upper_y;

                /**
                 * x-coordinate of lower point
                 */
                uint32_t lower_x;
                /**
                 * y-coordinate of lower point
                 */
                uint32_t lower_y;

            private:
                double slope_value;
                double inverse_slope_value;
        };

        /**
         * Returns all lines from a ring.
         * @param ring_reference
         * @return line vector
         */
        // TODO: change to iterator
        auto getLinePoints(
                const PolygonCollection::PolygonRingReference<const PolygonCollection> ring_reference) const -> const std::vector<Line>;

        /**
         * Flags (or inverts) a pixel with the polygon id
         * @param x
         * @param y
         * @param polygon_id
         */
        auto flag_pixel(uint32_t x, uint32_t y, uint32_t polygon_id) -> void;

        /**
         * Create a raster from a polygon connection's metadata.
         * @param rect
         * @param polygons
         * @return a blank raster
         */
        auto create_raster_for_polygon_collection(const QueryRectangle &rect,
                                                  const PolygonCollection &polygons) const -> std::unique_ptr<Raster2D<uint32_t>>;

        /**
         * Create a raster from a polygon feature's metadata.
         * @param rect
         * @param polygons
         * @return a blank raster
         */
        auto create_raster_for_polygon_feature(const QueryRectangle &rect,
                                               const PolygonCollection::PolygonFeatureReference<const PolygonCollection> &polygon_feature) const -> std::unique_ptr<Raster2D<uint32_t>>;

        /**
         * Create a 2d raster by specifications.
         * @param rect
         * @param minimum_bounding_rectangle
         * @param number_of_polygons
         * @return blank 2d raster
         */
        auto create_raster(const QueryRectangle &rect, SpatialReference &minimum_bounding_rectangle,
                           uint32_t number_of_polygons) const -> std::unique_ptr<Raster2D<uint32_t>>;

        /**
         * Draw a polygon into the raster image.
         * @param polygon
         */
        auto draw_polygon(const PolygonCollection::PolygonPolygonReference<const PolygonCollection> polygon) -> void;

        const QueryRectangle &rect;

        std::unique_ptr<Raster2D<uint32_t>> raster;
        uint32_t polygon_id = 0;
};


#endif //MAPPING_CORE_RASTERIZE_POLYGONS_H
