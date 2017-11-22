#include "datatypes/raster.h"
#include "datatypes/raster/raster_priv.h"
#include "datatypes/pointcollection.h"
#include "raster/profiler.h"
#include "raster/opencl.h"
#include "operators/operator.h"
#include "util/rasterize_polygons.h"

#include <json/json.h>
#include <algorithm>

/**
 * Operator that extracts raster information and attaches them to features.
 * It currently only supports attaching raster values to (simple) point collections.
 *
 * Parameters:
 * - names: array of names for the new feature attributes that contain the raster values
 *          A name has to be specified for each input raster
 * - xResolution: the x resolution for the input rasters in pixels
 * - yResolution: the y resolution for the input rasters in pixels
 */
class RasterValueExtractionOperator : public GenericOperator {
    public:
        RasterValueExtractionOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);

        ~RasterValueExtractionOperator() override;

#ifndef MAPPING_OPERATOR_STUBS

        auto getPointCollection(const QueryRectangle &rect,
                                const QueryTools &tools) -> std::unique_ptr<PointCollection> override;

        auto getPolygonCollection(const QueryRectangle &rect,
                                  const QueryTools &tools) -> std::unique_ptr<PolygonCollection> override;

#endif
    protected:
        auto writeSemanticParameters(std::ostringstream &stream) -> void override;

    private:
        std::vector<std::string> names;
        uint32_t x_resolution;
        uint32_t y_resolution;
};

RasterValueExtractionOperator::RasterValueExtractionOperator(int sourcecounts[], GenericOperator *sources[],
                                                             Json::Value &params) : GenericOperator(sourcecounts,
                                                                                                    sources) {
    auto number_of_vector_sources =
            getPointCollectionSourceCount() + getLineCollectionSourceCount() + getPolygonCollectionSourceCount();

    if (getLineCollectionSourceCount() > 0) {
        throw OperatorException("raster_metadata_to_points: lines not supported");
    } else if (number_of_vector_sources != 1) {
        // highlander rule for point OR polygon source
        throw OperatorException("raster_metadata_to_points: there must be exactly one vector source specified");
    }

    names.clear();
    auto arr = params["names"];
    if (!arr.isArray())
        throw OperatorException("raster_metadata_to_points: names parameter invalid");

    auto len = arr.size();
    names.reserve(len);
    for (int i = 0; i < len; i++) {
        names.push_back(arr[i].asString());
    }

    if (!params["xResolution"].isInt() || !params["yResolution"].isInt()) {
        throw OperatorException("raster_metadata_to_points: there must be a valid x and y resolution.");
    } else {
        x_resolution = params["xResolution"].asUInt();
        y_resolution = params["yResolution"].asUInt();
    }
}

RasterValueExtractionOperator::~RasterValueExtractionOperator() = default;

REGISTER_OPERATOR(RasterValueExtractionOperator, "raster_value_extraction");

void RasterValueExtractionOperator::writeSemanticParameters(std::ostringstream &stream) {
    stream << "{\"names\":[";
    for (auto &name : names) {
        stream << "\"" << name << "\",";
    }
    stream.seekp(((long) stream.tellp()) - 1); // remove last comma
    stream << "],";
    stream << "\"x_resolution\": " << x_resolution << ",";
    stream << "\"y_resolution\": " << y_resolution << "}";
}


#ifndef MAPPING_OPERATOR_STUBS

template<typename T>
struct PointDataEnhancement {
    static void execute(Raster2D<T> *raster, PointCollection *points, const std::string &name) {
        raster->setRepresentation(GenericRaster::Representation::CPU);

        auto &attr_vec = points->feature_attributes.numeric(name);
        size_t attr_idx = 0;

        for (auto &point : points->coordinates) {
            auto rasterCoordinateX = static_cast<int>(floor(raster->WorldToPixelX(point.x)));
            auto rasterCoordinateY = static_cast<int>(floor(raster->WorldToPixelY(point.y)));

            double attr = std::numeric_limits<double>::quiet_NaN();
            if (rasterCoordinateX >= 0 && rasterCoordinateY >= 0 && rasterCoordinateX < raster->width &&
                rasterCoordinateY < raster->height) {
                T value = raster->get(rasterCoordinateX, rasterCoordinateY);
                if (!raster->dd.is_no_data(value))
                    attr = (double) value;
            }
            attr_vec.set(attr_idx++, attr);
        }
    }
};

#include "operators/processing/combined/raster_value_extraction.cl.h"

static void enhance(PointCollection &points, GenericRaster &raster, const std::string &name, QueryProfiler &profiler) {
#ifdef MAPPING_NO_OPENCL
    auto &attr = points.feature_attributes.addNumericAttribute(name, raster.dd.unit);
    attr.reserve(points.getFeatureCount());
    callUnaryOperatorFunc<PointDataEnhancement>(&raster, &points, name);
#else
    RasterOpenCL::init();

    auto &vec = points.feature_attributes.addNumericAttribute(name, raster.dd.unit);
    vec.resize(points.getFeatureCount());
    try {
        RasterOpenCL::CLProgram prog;
        prog.setProfiler(profiler);
        prog.addPointCollection(&points);
        prog.addInRaster(&raster);
        prog.compile(operators_processing_combined_raster_value_extraction, "add_attribute");
        prog.addPointCollectionPositions(0);
        prog.addPointCollectionAttribute(0, name);
        prog.run();
    }
    catch (cl::Error &e) {
        printf("cl::Error %d: %s\n", e.err(), e.what());
        throw;
    }
#endif
}

std::unique_ptr<PointCollection>
RasterValueExtractionOperator::getPointCollection(const QueryRectangle &rect, const QueryTools &tools) {
    auto points = getPointCollectionFromSource(0, rect, tools, FeatureCollectionQM::SINGLE_ELEMENT_FEATURES);

    if (points->hasTime()) {
        // sort by time, iterate over all timestamps, fetch the correct raster, then add the attribute
        // TODO: currently, missing rasters will just throw an exception. Maybe we should add NaN instead?
        using p = std::pair<size_t, double>;
        std::vector<p> temporal_index;
        auto featurecount = points->getFeatureCount();
        temporal_index.reserve(featurecount);
        for (size_t i = 0; i < featurecount; i++)
            temporal_index.emplace_back(i, points->time[i].t1);

        std::sort(temporal_index.begin(), temporal_index.end(), [=](const p &a, const p &b) -> bool {
            return a.second < b.second;
        });

        auto rasters = getRasterSourceCount();
        TemporalReference tref = TemporalReference::unreferenced();
        for (int r = 0; r < rasters; r++) {
            auto &attributevector = points->feature_attributes.addNumericAttribute(names.at(r),
                                                                                   Unit::unknown()); // TODO: unit
            attributevector.resize(featurecount);
            // iterate over time
            size_t current_idx = 0;
            while (current_idx < featurecount) {
                // TODO: inprecise, the timestamps may not be [t1,t1).
                QueryRectangle rect2(rect,
                                     TemporalReference(rect.timetype, temporal_index[current_idx].second),
                                     QueryResolution::pixels(x_resolution, y_resolution));
                try {
                    auto raster = getRasterFromSource(r, rect2, tools);
                    while (current_idx < featurecount && temporal_index[current_idx].second < raster->stref.t2) {
                        // load point and add
                        auto featureidx = temporal_index[current_idx].first;
                        const auto &c = points->coordinates[featureidx];

                        auto rasterCoordinateX = raster->WorldToPixelX(c.x);
                        auto rasterCoordinateY = raster->WorldToPixelY(c.y);
                        if (rasterCoordinateX >= 0 && rasterCoordinateY >= 0 && rasterCoordinateX < raster->width &&
                            rasterCoordinateY < raster->height) {
                            double value = raster->getAsDouble(rasterCoordinateX, rasterCoordinateY);
                            if (!raster->dd.is_no_data(value))
                                attributevector.set(featureidx, value);
                        }

                        current_idx++;
                    }
                }
                catch (const SourceException &e) {
                    // no data available, keep it as NaN and continue
                    current_idx++;
                }
            }
        }
    } else {
        auto rasters = getRasterSourceCount();
        TemporalReference tref = TemporalReference::unreferenced();
        QueryRectangle rect2(rect, rect,
                             QueryResolution::pixels(x_resolution, y_resolution));
        for (int r = 0; r < rasters; r++) {
            auto raster = getRasterFromSource(r, rect2, tools);
            Profiler::Profiler p("RASTER_VALUE_TO_POINTS_OPERATOR");
            enhance(*points, *raster, names.at(r), tools.profiler);
            if (r == 0)
                tref = raster->stref;
            else
                tref.intersect(raster->stref);
        }
        points->addDefaultTimestamps(tref.t1, tref.t2);
    }
    return points;
}

auto RasterValueExtractionOperator::getPolygonCollection(const QueryRectangle &rect,
                                                         const QueryTools &tools) -> std::unique_ptr<PolygonCollection> {
    auto polygon_collection = getPolygonCollectionFromSource(0, rect, tools);

    // create raster query
    SpatialReference minimum_bounding_rectangle = polygon_collection->getCollectionMBR();
    // TODO: by now we query only the first raster, but this must change in the future
    TemporalReference temporal_reference{rect.timetype, rect.t1};
    QueryRectangle raster_rect{
            minimum_bounding_rectangle,
            temporal_reference,
            QueryResolution::pixels(this->x_resolution, this->y_resolution)
    };

    // loop through rasters
    for (int raster_source_id = 0; raster_source_id < this->names.size(); ++raster_source_id) {
        const std::string &name_prefix = this->names[raster_source_id];

        const auto &raster = getRasterFromSource(raster_source_id, raster_rect, tools, RasterQM::EXACT);

        for (const std::string &suffix : {"mean", "stdev", "min", "max"}) {
            polygon_collection->feature_attributes.addNumericAttribute(
                    concat(name_prefix, "_", suffix),
                    raster->dd.unit
            );
        }

        // loop through features
        for (const auto feature : const_cast<const PolygonCollection &>(*polygon_collection)) {
            const auto raster_stat_cells = RasterizePolygons{raster_rect, feature}.get_raster();

            // gather stats from raster (Welford's algorithm)
            size_t n = 0;
            double mean = 0;
            double M2 = 0;
            double min = std::numeric_limits<double>::infinity();
            double max = -std::numeric_limits<double>::infinity();

            bool ignore_nan = false; // TODO: extract as parameter
            bool has_nan = false;

            for (uint32_t x = 0; x < raster_stat_cells->width; ++x) {
                for (uint32_t y = 0; y < raster_stat_cells->height; ++y) {
                    if (raster_stat_cells->get(x, y) > 0) { // include into stats
                        double value = raster->getAsDouble(x, y);

                        if (std::isnan(value)) {
                            has_nan = true;
                            if (ignore_nan) {
                                continue;
                            } else {
                                min = max = std::numeric_limits<double>::quiet_NaN();
                                goto out_of_loop;
                            }
                        }

                        ++n;
                        double delta = value - mean;
                        mean += delta / n;
                        double delta2 = value - mean;
                        M2 += delta * delta2;

                        if (value > max) {
                            max = value;
                        }
                        if (value < min) {
                            min = value;
                        }
                    }
                }
            }
            out_of_loop:;

            polygon_collection->feature_attributes.numeric(concat(name_prefix, "_", "mean")).set(feature, mean);
            double variance = M2 / (n - 1);
            polygon_collection->feature_attributes.numeric(concat(name_prefix, "_", "stdev")).set(feature, std::sqrt(variance));
            polygon_collection->feature_attributes.numeric(concat(name_prefix, "_", "min")).set(feature, min);
            polygon_collection->feature_attributes.numeric(concat(name_prefix, "_", "max")).set(feature, max);
        }
    }

    return polygon_collection;
}

#endif
