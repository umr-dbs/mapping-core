#include <gtest/gtest.h>
#include <raster/opencl.h>
#include <operators/processing/raster/rgba_composite.cl.h>
#include <util/log.h>
#include <util/configuration.h>

TEST(datatypes_raster_rgb, opencl) {
    Configuration::loadFromDefaultPaths();

    const uint8_t NO_DATA = 42;

    SpatioTemporalReference stref(SpatialReference(CrsId::from_epsg_code(4326)), TemporalReference::unreferenced());

    Raster2D<uint8_t> raster(
            DataDescription(GDALDataType::GDT_Byte, Unit::unknown(), true, NO_DATA),
            stref,
            2,
            2
    );

    raster.set(0, 0, 0);
    raster.set(0, 1, NO_DATA);
    raster.set(1, 0, 0xAA);
    raster.set(1, 1, 0xFF);

    ASSERT_EQ(raster.get(0, 0), 0);
    ASSERT_EQ(raster.get(0, 1), NO_DATA);
    ASSERT_EQ(raster.get(1, 0), 0xAA);
    ASSERT_EQ(raster.get(1, 1), 0xFF);

    RasterOpenCL::init(); // prevents SIGSEGV
    RasterOpenCL::CLProgram kernel;

    kernel.addInRaster(&raster);
    kernel.addInRaster(&raster);
    kernel.addInRaster(&raster);

    Raster2D<uint32_t> result(
            DataDescription(GDALDataType::GDT_UInt32, Unit::unknown()),
            stref,
            2,
            2
    );

    kernel.addOutRaster(&result);

    kernel.compile(operators_processing_raster_rgba_composite, "rgba_composite_kernel");

    kernel.addArg(0.);
    kernel.addArg(255.);
    kernel.addArg(1.);
    kernel.addArg(0.);
    kernel.addArg(255.);
    kernel.addArg(1.);
    kernel.addArg(0.);
    kernel.addArg(255.);
    kernel.addArg(1.);

    kernel.run();

    result.setRepresentation(GenericRaster::Representation::CPU); // retrieve values from GPU buffer

    EXPECT_EQ(result.get(0, 0), 0xFF000000);
    EXPECT_EQ(result.get(0, 1), 0);
    EXPECT_EQ(result.get(1, 0), 0xFFAAAAAA);
    EXPECT_EQ(result.get(1, 1), 0xFFFFFFFF);
}
