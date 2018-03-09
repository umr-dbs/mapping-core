#include <json/json.h>
#include <algorithm>

#include "datatypes/raster.h"
#include "datatypes/raster/raster_priv.h"
#include "operators/operator.h"
#include "raster/opencl.h"
#include "operators/processing/combined/points2raster_frequency.cl.h"
#include "operators/processing/combined/points2raster_value.cl.h"

/**
 * Operator that rasterizes features.
 * It currently only suppors rendering a point collection as a heatmap
 *
 * Parameters:
 * - attribute: the name of the attribute whose frequency is counted for the heatmap
 *                    if no renderattribute is given, the location alone is taken for rendering
 * - radius: the radius for each point in the heatmap
 */
class RasterizationOperator : public GenericOperator {
    public:
        RasterizationOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);

        ~RasterizationOperator() override;

        auto getRaster(const QueryRectangle &rect,
                       const QueryTools &tools) -> std::unique_ptr<GenericRaster> override;

    protected:
        auto writeSemanticParameters(std::ostringstream &stream) -> void override;

    private:
        std::string attribute;
        double radius;
};

RasterizationOperator::RasterizationOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params)
        : GenericOperator(sourcecounts, sources) {
    assumeSources(1);
    this->attribute = params.get("attribute", "").asString();
    this->radius = params.get("radius", 8).asDouble();
}

RasterizationOperator::~RasterizationOperator() = default;

REGISTER_OPERATOR(RasterizationOperator, "rasterization"); /* NOLINT */

void RasterizationOperator::writeSemanticParameters(std::ostringstream &stream) {
    Json::Value semantic_parameters{Json::objectValue};
    semantic_parameters["attribute"] = this->attribute;
    semantic_parameters["radius"] = this->radius;

    Json::FastWriter writer;
    stream << writer.write(semantic_parameters);
}


std::unique_ptr<GenericRaster> RasterizationOperator::getRaster(const QueryRectangle &rect, const QueryTools &tools) {

    RasterOpenCL::init();

    QueryRectangle rect_larger = rect;
    rect_larger.enlargePixels(static_cast<int>(std::ceil(this->radius)));

    QueryRectangle rect_points{rect_larger, rect_larger, QueryResolution::none()};
    const auto points = getPointCollectionFromSource(0, rect_points, tools);

    if (this->attribute.empty()) {
        Unit unit_acc = Unit::unknown();
        uint32_t acc_max = std::numeric_limits<uint32_t>::max() - 1;
        unit_acc.setMinMax(0, acc_max);

        DataDescription dd_acc(GDT_UInt32, unit_acc, true, 0);
        auto accumulator = GenericRaster::create(dd_acc,
                                                 rect_larger,
                                                 rect_larger.xres, rect_larger.yres, 0 /* depth */,
                                                 GenericRaster::Representation::CPU);
        accumulator->clear(0);

        for (const auto &feature : *points) {
            for (auto &p : feature) {
                auto px = static_cast<int>(accumulator->WorldToPixelX(p.x));
                auto py = static_cast<int>(accumulator->WorldToPixelY(p.y));
                if (px < 0 || py < 0 || px >= accumulator->width || py >= accumulator->height)
                    continue;

                uint32_t value = dynamic_cast<Raster2D<uint32_t> *>(accumulator.get())->get(px, py);
                value = std::min(static_cast<uint32_t>(acc_max), value + 1);

                dynamic_cast<Raster2D<uint32_t> *>(accumulator.get())->set(px, py, value);
            }
        }

        Unit unit_blur = Unit("frequency", "heatmap");
        unit_blur.setMinMax(0, 255);
        unit_blur.setInterpolation(Unit::Interpolation::Continuous);
        DataDescription dd_blur(GDT_Byte, unit_blur, true, 0);
        auto blurred = GenericRaster::create(dd_blur, rect, rect.xres, rect.yres, 0,
                                             GenericRaster::Representation::OPENCL);

        if (points->getFeatureCount() > 0) {
            RasterOpenCL::CLProgram prog;
            prog.setProfiler(tools.profiler);
            prog.addInRaster(accumulator.get());
            prog.addOutRaster(blurred.get());
            prog.compile(operators_processing_combined_points2raster_frequency, "blur_frequency");
            prog.addArg(radius);
            prog.run();
        }

        return blurred;
    } else {
        const float MIN = 0, MAX = 10000;

        Unit unit_sum = Unit::unknown();
        unit_sum.setMinMax(MIN, MAX);

        DataDescription dd_sum(GDT_Float32, unit_sum, true, 0);
        DataDescription dd_count(GDT_UInt32, Unit::unknown(), true, 0);

        auto raster_sum = GenericRaster::create(dd_sum,
                                                rect_larger,
                                                rect_larger.xres, rect_larger.yres, 0 /* depth */,
                                                GenericRaster::Representation::CPU);
        auto raster_count = GenericRaster::create(dd_count,
                                                  rect_larger,
                                                  rect_larger.xres, rect_larger.yres, 0 /* depth */,
                                                  GenericRaster::Representation::CPU);
        raster_sum->clear(0);
        raster_count->clear(0);
        uint32_t count_max = std::numeric_limits<uint32_t>::max() - 1;

        const auto &vec = points->feature_attributes.numeric(attribute);
        for (const auto &feature : *points) {
            for (auto &p : feature) {
                auto px = static_cast<int>(raster_sum->WorldToPixelX(p.x));
                auto py = static_cast<int>(raster_sum->WorldToPixelY(p.y));
                if (px < 0 || py < 0 || px >= raster_sum->width || py >= raster_sum->height)
                    continue;

                auto attr = vec.get(feature);
                if (std::isnan(attr))
                    continue;

                auto sval = dynamic_cast<Raster2D<float> *>(raster_sum.get())->get(px, py);
                sval = static_cast<float>(sval + attr);
                dynamic_cast<Raster2D<float> *>(raster_sum.get())->set(px, py, sval);

                auto cval = dynamic_cast<Raster2D<uint32_t> *>(raster_count.get())->get(px, py);
                cval = std::min((decltype(cval + 1)) count_max, cval + 1);
                dynamic_cast<Raster2D<uint32_t> *>(raster_count.get())->set(px, py, cval);
            }
        }

        Unit unit_result = Unit("unknown", "heatmap"); // TODO: use measurement from the rendered attribute
        unit_result.setMinMax(MIN, MAX);
        unit_result.setInterpolation(Unit::Interpolation::Continuous);
        DataDescription dd_blur(GDT_Float32, unit_result, true, 0);
        auto blurred = GenericRaster::create(dd_blur, rect, rect.xres, rect.yres, 0,
                                             GenericRaster::Representation::OPENCL);

        if (points->getFeatureCount() > 0) {
            RasterOpenCL::CLProgram prog;
            prog.setProfiler(tools.profiler);
            prog.addInRaster(raster_count.get());
            prog.addInRaster(raster_sum.get());
            prog.addOutRaster(blurred.get());
            prog.compile(operators_processing_combined_points2raster_value, "blur_value");
            prog.addArg(radius);
            prog.run();
        }

        return blurred;
    }
}
