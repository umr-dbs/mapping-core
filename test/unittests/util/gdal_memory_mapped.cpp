#include <gtest/gtest.h>
#include <functional>
#include <ogrsf_frmts.h>

TEST(GdalOgr, MemoryMappedFile) {
    std::string filename = "/vsimem/123456789";

    std::string geojson = R"(
    {
      "type": "FeatureCollection",
      "features": [
        {
          "type": "Feature",
          "geometry": {
            "type": "Point",
            "coordinates": [
              -64.305718740625,
              15.73563441524621
            ]
          },
          "properties": {
            "id": 1
          }
        },
        {
          "type": "Feature",
          "geometry": {
            "type": "Point",
            "coordinates": [
              -64.87700780312498,
              13.246843143428464
            ]
          },
          "properties": {
            "id": 2
          }
        },
        {
          "type": "Feature",
          "geometry": {
            "type": "Point",
            "coordinates": [
              -56.043999990625,
              13.780948619881485
            ]
          },
          "properties": {
            "id": 3
          }
        },
        {
          "type": "Feature",
          "geometry": {
            "type": "Point",
            "coordinates": [
              -59.208062490625,
              15.693331633994802
            ]
          },
          "properties": {
            "id": 4
          }
        }
      ]
    }
    )";

    auto *data = const_cast<GByte *>(reinterpret_cast<const GByte *>(geojson.data()));
    auto data_len = geojson.length();

    auto *vsi_file = VSIFileFromMemBuffer(
            filename.c_str(),
            data,
            data_len,
            false
    );

    ASSERT_NE(vsi_file, nullptr); // NULL = invalid file handle

    GDALAllRegister(); // registers all drivers

    auto *gdal_raw_dataset = static_cast<GDALDataset *>(GDALOpenEx(
            filename.c_str(),
            GDAL_OF_VECTOR,
            nullptr,
            nullptr,
            nullptr
    ));

    ASSERT_NE(gdal_raw_dataset, nullptr); // NULL = could not load

    auto gdal_dataset = std::unique_ptr<GDALDataset, std::function<void(GDALDataset *)>>(
            gdal_raw_dataset,
            [](GDALDataset *dataset) { GDALClose(dataset); }
    );

    ASSERT_EQ(gdal_dataset->GetLayerCount(), 1);

    OGRLayer *layer = gdal_dataset->GetLayer(0);
    ASSERT_NE(layer, nullptr);

    layer->ResetReading();

    ASSERT_EQ(layer->GetFeatureCount(), 4);

    ASSERT_EQ(VSIUnlink(filename.c_str()), 0); // 0 = success
}
