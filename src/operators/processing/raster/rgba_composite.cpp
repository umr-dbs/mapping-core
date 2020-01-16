#include "datatypes/raster.h"
#include "raster/opencl.h"
#include "operators/operator.h"
#include "util/formula.h"
#include <memory>
#include <json/json.h>
#include <gdal_priv.h>
#include "rgba_composite.cl.h"

static const int EXPECTED_RASTER_INPUTS = 3;

/**
 * Operator that evaluates an expression on a given set of rasters
 * This operator combines three input rasters into an RGBA raster, consisting of four bytes (r, g, b, a) = uint32_t.
 *
 * Parameters:
 * - raster_r_min, raster_r_max - clamps the values for red in this range
 * - raster_r_scale - values in [0, 1] that scales red
 * - raster_g_min, raster_g_max - clamps the values for green in this range
 * - raster_g_scale - values in [0, 1] that scales green
 * - raster_b_min, raster_b_max - clamps the values for blue in this range
 * - raster_b_scale - values in [0, 1] that scales blues
 */
class RgbaCompositeOperator : public GenericOperator {
    public:
        RgbaCompositeOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);

        ~RgbaCompositeOperator() override;

#ifndef MAPPING_OPERATOR_STUBS

        auto getRaster(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<GenericRaster> override;

#endif
    protected:
        void writeSemanticParameters(std::ostringstream &stream) override;

    private:
        double raster_r_min, raster_r_max;
        double raster_r_scale;
        double raster_g_min, raster_g_max;
        double raster_g_scale;
        double raster_b_min, raster_b_max;
        double raster_b_scale;
};


RgbaCompositeOperator::RgbaCompositeOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params)
        : GenericOperator(sourcecounts, sources) {
    assumeSources(EXPECTED_RASTER_INPUTS); // TODO: make this work again?!
    if (getRasterSourceCount() != EXPECTED_RASTER_INPUTS) {
        throw OperatorException("`rgba_composite` operator requires three raster sources`");
    }

    raster_r_min = params.get("raster_r_min", std::numeric_limits<double>::quiet_NaN()).asDouble();
    raster_r_max = params.get("raster_r_max", std::numeric_limits<double>::quiet_NaN()).asDouble();
    raster_r_scale = params.get("raster_r_scale", 1.0).asDouble();
    raster_g_min = params.get("raster_g_min", std::numeric_limits<double>::quiet_NaN()).asDouble();
    raster_g_max = params.get("raster_g_max", std::numeric_limits<double>::quiet_NaN()).asDouble();
    raster_g_scale = params.get("raster_g_scale", 1.0).asDouble();
    raster_b_min = params.get("raster_b_min", std::numeric_limits<double>::quiet_NaN()).asDouble();
    raster_b_max = params.get("raster_b_max", std::numeric_limits<double>::quiet_NaN()).asDouble();
    raster_b_scale = params.get("raster_b_scale", 1.0).asDouble();

    if (std::isnan(raster_r_min) || std::isnan(raster_r_max) || std::isnan(raster_r_scale) ||
        std::isnan(raster_g_min) || std::isnan(raster_g_max) || std::isnan(raster_g_scale) ||
        std::isnan(raster_b_min) || std::isnan(raster_b_max) || std::isnan(raster_b_scale)) {
        throw OperatorException("All min/max parameters must be set and no parameter must be NaN");
    }
}

RgbaCompositeOperator::~RgbaCompositeOperator() = default;

REGISTER_OPERATOR(RgbaCompositeOperator, "rgba_composite");

void RgbaCompositeOperator::writeSemanticParameters(std::ostringstream &stream) {
    auto parameters = Json::Value(Json::objectValue);
    parameters["raster_r_min"] = raster_r_min;
    parameters["raster_r_max"] = raster_r_max;
    parameters["raster_r_scale"] = raster_r_scale;
    parameters["raster_g_min"] = raster_r_min;
    parameters["raster_g_max"] = raster_r_max;
    parameters["raster_g_scale"] = raster_r_scale;
    parameters["raster_b_min"] = raster_r_min;
    parameters["raster_b_max"] = raster_r_max;
    parameters["raster_b_scale"] = raster_r_scale;

    Json::FastWriter writer;
    stream << writer.write(parameters);
}

#ifndef MAPPING_OPERATOR_STUBS

auto RgbaCompositeOperator::getRaster(const QueryRectangle &rect,
                                      const QueryTools &tools) -> std::unique_ptr<GenericRaster> {
    std::unique_ptr<GenericRaster> raster_r = getRasterFromSource(0, rect, tools, RasterQM::LOOSE);

    // the "red" raster determines the exact query rectangle for loading the other two rasters
    QueryRectangle exact_rect(
            static_cast<const SpatialReference &>(raster_r->stref),
            static_cast<const TemporalReference &>(rect),
            QueryResolution::pixels(raster_r->width, raster_r->height)
    );

    std::unique_ptr<GenericRaster> raster_g = getRasterFromSource(1, exact_rect, tools, RasterQM::EXACT);
    std::unique_ptr<GenericRaster> raster_b = getRasterFromSource(2, exact_rect, tools, RasterQM::EXACT);

    RasterOpenCL::init(); // it is necessary to init it already here, otherwise the following statements would crash

    raster_r->setRepresentation(GenericRaster::OPENCL);
    raster_g->setRepresentation(GenericRaster::OPENCL);
    raster_b->setRepresentation(GenericRaster::OPENCL);

    // calculate as intersection of all trefs
    TemporalReference output_tref(static_cast<const TemporalReference &>(raster_r->stref));
    output_tref.intersect(raster_g->stref);
    output_tref.intersect(raster_b->stref);

    // create output raster
    DataDescription output_data_description(GDALDataType::GDT_UInt32, Unit::unknown(), true, 0);
    SpatioTemporalReference output_stref(raster_r->stref, output_tref);
    auto rgba_raster = GenericRaster::create(output_data_description, output_stref,
                                             raster_r->width, raster_r->height, 0,
                                             GenericRaster::Representation::OPENCL);

    // Run Kernel
    RasterOpenCL::CLProgram kernel;
    kernel.setProfiler(tools.profiler);

    kernel.addInRaster(raster_r.get());
    kernel.addInRaster(raster_g.get());
    kernel.addInRaster(raster_b.get());

    kernel.addOutRaster(rgba_raster.get());

    kernel.compile(operators_processing_raster_rgba_composite, "rgba_composite_kernel");

    kernel.addArg(raster_r_min);
    kernel.addArg(raster_r_max);
    kernel.addArg(raster_r_scale);
    kernel.addArg(raster_g_min);
    kernel.addArg(raster_g_max);
    kernel.addArg(raster_g_scale);
    kernel.addArg(raster_b_min);
    kernel.addArg(raster_b_max);
    kernel.addArg(raster_b_scale);

    kernel.run();

    return rgba_raster;
}

#endif
