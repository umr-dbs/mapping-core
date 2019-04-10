#include <cassert>
#include <cmath>
#include "rasterize_polygons.h"

RasterizePolygons::RasterizePolygons(const QueryRectangle &rect, const PolygonCollection &polygon_collection) :
        raster(create_raster_for_polygon_collection(rect, polygon_collection)), rect(rect) {
    for (const auto &polygon_feature : polygon_collection) {
        for (const auto &polygon : polygon_feature) {
            draw_polygon(polygon);
        }
    }
}

RasterizePolygons::RasterizePolygons(const QueryRectangle &rect,
                                     const PolygonCollection::PolygonFeatureReference<const PolygonCollection> &polygon_feature)
        :
        raster(create_raster_for_polygon_feature(rect, polygon_feature)), rect(rect) {
    for (const auto &polygon : polygon_feature) {
        draw_polygon(polygon);
    }
}

auto RasterizePolygons::get_raster() const -> std::unique_ptr<Raster2D<uint8_t>> {
    // TODO: find better way to achieve cast
    auto fitted_raster = std::unique_ptr<Raster2D<uint32_t>>(
            dynamic_cast<Raster2D<uint32_t> *>(
                    this->raster->fitToQueryRectangle(this->rect).release()
            )
    );

    Unit boolean_unit = Unit::unknown();
    boolean_unit.setMinMax(0, 1); // reserve one value for each polygon in the collection
    DataDescription boolean_data_description {GDT_Byte, boolean_unit, true, 0};

    auto boolean_raster = std::make_unique<Raster2D<uint8_t>>(
            boolean_data_description,
            fitted_raster->stref,
            fitted_raster->width,
            fitted_raster->height
    );

    for (uint32_t x = 0; x < fitted_raster->width; ++x) {
        for (uint32_t y = 0; y < fitted_raster->height; ++y) {
            bool fill = fitted_raster->get(x, y) > 0;
            boolean_raster->set(x, y, static_cast<uint8_t>(fill));
        }
    }

    return boolean_raster;
}

auto RasterizePolygons::create_raster(const QueryRectangle &rect, SpatialReference &minimum_bounding_rectangle,
                                      uint32_t number_of_polygons) const -> std::unique_ptr<Raster2D<uint32_t>> {
    assert(number_of_polygons > 0);

    // update such that it is at least the size of the query rectangle
    minimum_bounding_rectangle.x1 = std::min(minimum_bounding_rectangle.x1, rect.x1);
    minimum_bounding_rectangle.x2 = std::max(minimum_bounding_rectangle.x2, rect.x2);
    minimum_bounding_rectangle.y1 = std::min(minimum_bounding_rectangle.y1, rect.y1);
    minimum_bounding_rectangle.y2 = std::max(minimum_bounding_rectangle.y2, rect.y2);

    // create spatio-temporal reference
    SpatioTemporalReference st_ref{minimum_bounding_rectangle, rect};

    // boolean unit and data description
    Unit unit = Unit::unknown();
    unit.setMinMax(0, number_of_polygons); // reserve one value for each polygon in the collection

    DataDescription data_description(GDT_UInt32, unit, true, 0);

    // calculate how much larger the minimum bounding rectangle is compared to the query rectangle
    auto x_span_query = rect.x2 - rect.x1;
    auto y_span_query = rect.y2 - rect.y1;

    auto x_span_mbr = minimum_bounding_rectangle.x2 - minimum_bounding_rectangle.x1;
    auto y_span_mbr = minimum_bounding_rectangle.y2 - minimum_bounding_rectangle.y1;

    // must be equal to or larger than the query due to previous update
    auto x_enlargement = x_span_mbr / x_span_query;
    auto y_enlargement = y_span_mbr / y_span_query;

    return std::make_unique<Raster2D<uint32_t>>(
            data_description,
            st_ref,
            static_cast<uint32_t>(std::ceil(rect.xres * x_enlargement)),
            static_cast<uint32_t>(std::ceil(rect.yres * y_enlargement))
    );
}

auto RasterizePolygons::create_raster_for_polygon_collection(const QueryRectangle &rect,
                                                             const PolygonCollection &polygon_collection) const -> std::unique_ptr<Raster2D<uint32_t>> {
    SpatialReference minimum_bounding_rectangle = polygon_collection.getCollectionMBR();

    uint32_t number_of_polygons = 0;
    for (const auto &feature : polygon_collection) {
        auto new_number_of_polygons = number_of_polygons + feature.size();

        if (new_number_of_polygons < number_of_polygons) {
            // OVERFLOW
            throw FeatureException("Too many polygons for this operator.");
        } else {
            number_of_polygons = static_cast<uint32_t>(new_number_of_polygons);
        }
    }

    return create_raster(rect, minimum_bounding_rectangle, number_of_polygons);
}

auto RasterizePolygons::create_raster_for_polygon_feature(const QueryRectangle &rect,
                                                          const PolygonCollection::PolygonFeatureReference<const PolygonCollection> &polygon_feature) const -> std::unique_ptr<Raster2D<uint32_t>> {
    SpatialReference minimum_bounding_rectangle = polygon_feature.getMBR();

    auto number_of_polygons_long = polygon_feature.size();
    if (number_of_polygons_long > std::numeric_limits<uint32_t>::max()) {
        // would overflow
        throw FeatureException("Too many polygons for this operator.");
    }
    auto number_of_polygons = static_cast<uint32_t>(number_of_polygons_long);

    return create_raster(rect, minimum_bounding_rectangle, number_of_polygons);
}

auto RasterizePolygons::flag_pixel(uint32_t x, uint32_t y, uint32_t polygon_id) -> void {
    assert(polygon_id > 0);

    uint32_t current_value = raster->get(x, y);

    raster->setSafe(x, y, (current_value == polygon_id) ? polygon_id - 1 : polygon_id);
}

const std::vector<RasterizePolygons::Line> RasterizePolygons::getLinePoints(
        const PolygonCollection::PolygonRingReference<const PolygonCollection> ring_reference) const {
    std::vector<RasterizePolygons::Line> lines;

    int64_t previous_px;
    int64_t previous_py;
    bool first_coordinate = true;

    for (const Coordinate &coordinate : ring_reference) {
        auto px = raster->WorldToPixelX(coordinate.x);
        auto py = raster->WorldToPixelY(coordinate.y);

        if (!first_coordinate) {
            lines.emplace_back(previous_px, previous_py, px, py);
        } else {
            first_coordinate = false;
        }

        previous_px = px;
        previous_py = py;
    }

    return lines;
}

auto RasterizePolygons::draw_polygon(
        const PolygonCollection::PolygonPolygonReference<const PolygonCollection> polygon) -> void {
    // edge flag algorithm

    // increment id to have distinguishable colors for filling per polygon
    ++polygon_id;

    // draw outline

    for (const auto &ring : polygon) {
        for (const Line &line : getLinePoints(ring)) {
            if (std::isinf(line.inverse_slope())) {
                // horizontal line (special case)

                auto y = line.lower_y;

                if (y >= raster->height) {
                    continue;
                }

                flag_pixel(line.min_x(), y, polygon_id);
                flag_pixel(line.max_x(), y, polygon_id);
            } else {
                for (uint32_t y = line.lower_y; y < std::min(line.upper_y, raster->height); ++y) {
                    // loop through rows (y)

                    double cut_y = y + 0.5;
                    double cut_x = line.lower_x + line.inverse_slope() * (cut_y - line.lower_y);

                    auto x = static_cast<uint32_t >(cut_x); // floor

                    x = ((x + 0.5) <= cut_x) ? x + 1 : x;

                    flag_pixel(x, y, polygon_id);
                }
            }
        }
    }

    // color raster (fill polygon)

    for (uint32_t y = 0; y < raster->height; ++y) { // loop through rows
        bool within = false;

        for (uint32_t x = 0; x < raster->width; ++x) { // loop through row values
            uint32_t current_value = raster->get(x, y);

            if (current_value == polygon_id) {
                within = !within;
            }

            if (within) {
                raster->set(x, y, polygon_id);
            } else {
                // TODO: un-color?
//                        raster_ref.set(x, y, (current_value > 0) ? current_value - 1 : 0);
            }
        }
    }
}

RasterizePolygons::Line::Line(uint32_t upper_x, uint32_t upper_y, uint32_t lower_x, uint32_t lower_y)
        : upper_x(upper_x), upper_y(upper_y), lower_x(lower_x), lower_y(lower_y) {
    if (this->lower_y > this->upper_y) {
        std::swap(this->upper_x, this->lower_x);
        std::swap(this->upper_y, this->lower_y);
    }

    if (this->lower_x == this->upper_x) {
        this->slope_value = std::numeric_limits<double>::infinity();
        this->inverse_slope_value = 0;
    } else if (this->lower_y == this->upper_y) {
        this->slope_value = 0;
        this->inverse_slope_value = std::numeric_limits<double>::infinity();
    } else {
        auto x_diff = static_cast<double>(this->lower_x) - static_cast<double>(this->upper_x);
        auto y_diff = static_cast<double>(this->lower_y) - static_cast<double>(this->upper_y);
        this->inverse_slope_value = x_diff / y_diff;
        this->slope_value = 1 / this->inverse_slope_value;
    }
}

auto RasterizePolygons::Line::slope() const -> double {
    return this->slope_value;
}

auto RasterizePolygons::Line::inverse_slope() const -> double {
    return this->inverse_slope_value;
}

auto RasterizePolygons::Line::min_x() const -> uint32_t {
    return std::min(this->lower_x, this->upper_x);
}

auto RasterizePolygons::Line::max_x() const -> uint32_t {
    return std::min(this->lower_x, this->upper_x);
}
