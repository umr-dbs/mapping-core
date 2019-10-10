#include "operators/operator.h"
#include "datatypes/plots/json.h"
#include "datatypes/raster.h"

#include <string>
#include <json/json.h>
#include <limits>
#include <vector>
#include <unordered_map>
#include <datatypes/plots/statistics.h>
#include <util/NumberStatistics.h>
#include "datatypes/pointcollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/polygoncollection.h"

/**
 * Operator that calculates and outputs statistics for all input layers
 * Parameters:
 *   - raster_width (optional) the resolution to calculate the raster statistics on
 *   - raster_height (optional)
 *
 *  Output example:
 *  {
 *    "data": {
 *      "points": [
 *        {
 *          "population": {
 *            "count": 100000,
 *            "max": 99998,
 *            "mean": 52040.203309998964,
 *            "min": 6,
 *            "nan_count": 0,
 *            "stddev": 26942.648100743056
 *          }
 *        }
 *      ],
 *      "rasters": [
 *        {
 *          "count": 50,
 *          "max": 0,
 *          "mean": 0,
 *          "min": 0,
 *          "nan_count": 0,
 *          "stddev": 0
 *        }
 *      ]
 *    },
 *    "type": "layer_statistics"
 *  }
 *
 */
class StatisticsOperator : public GenericOperator {
    public:
        StatisticsOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);

        ~StatisticsOperator() override;

#ifndef MAPPING_OPERATOR_STUBS

        std::unique_ptr<GenericPlot> getPlot(const QueryRectangle &rect, const QueryTools &tools) override;

#endif
    protected:
        void writeSemanticParameters(std::ostringstream &stream) override;

    private:
        uint32_t rasterWidth;
        uint32_t rasterHeight;

#ifndef MAPPING_OPERATOR_STUBS

        auto processRaster(LayerStatistics &result, const GenericRaster &raster) const -> void;

        auto processFeatureCollection(LayerStatistics &result,
                                      LayerStatistics::FeatureType feature_type,
                                      SimpleFeatureCollection &collection) const -> void;

#endif
};

REGISTER_OPERATOR(StatisticsOperator, "statistics");

StatisticsOperator::StatisticsOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params)
        : GenericOperator(sourcecounts, sources) {
    assumeSources(1);

    rasterWidth = params.get("raster_width", 256).asUInt();
    rasterHeight = params.get("raster_height", 256).asUInt();
}

StatisticsOperator::~StatisticsOperator() = default;


void StatisticsOperator::writeSemanticParameters(std::ostringstream &stream) {
    Json::Value params;

    Json::FastWriter writer;
    stream << writer.write(params);
}

#ifndef MAPPING_OPERATOR_STUBS

auto StatisticsOperator::getPlot(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<GenericPlot> {
    auto result = std::make_unique<LayerStatistics>();

    // TODO: compute statistics using OpenCL
    for (int i = 0; i < getRasterSourceCount(); ++i) {
        auto raster = getRasterFromSource(
                i,
                QueryRectangle(rect, rect, QueryResolution::pixels(rasterWidth, rasterHeight)),
                tools,
                RasterQM::EXACT
        );
        processRaster(*result, *raster);
    }

    for (int i = 0; i < getPointCollectionSourceCount(); ++i) {
        auto collection = getPointCollectionFromSource(i, rect, tools);
        processFeatureCollection(*result, LayerStatistics::FeatureType::POINTS, *collection);
    }

    for (int i = 0; i < getLineCollectionSourceCount(); ++i) {
        auto collection = getLineCollectionFromSource(i, rect, tools);
        processFeatureCollection(*result, LayerStatistics::FeatureType::LINES, *collection);
    }

    for (int i = 0; i < getPolygonCollectionSourceCount(); ++i) {
        auto collection = getPolygonCollectionFromSource(i, rect, tools);
        processFeatureCollection(*result, LayerStatistics::FeatureType::POLYGONS, *collection);
    }

    return std::unique_ptr<GenericPlot>(result.release());
}

auto StatisticsOperator::processRaster(LayerStatistics &result, const GenericRaster &raster) const -> void {
    NumberStatistics number_statistics;

    for (int x = 0; x < raster.width; ++x) {
        for (int y = 0; y < raster.height; ++y) {
            double value = raster.getAsDouble(x, y);
            if (!raster.dd.is_no_data(value)) {
                // TODO: combine no_data and NaNs in stats
                number_statistics.add(value);
            }
        }
    }

    result.addRasterStats(number_statistics);
}

auto StatisticsOperator::processFeatureCollection(LayerStatistics &result,
                                                  LayerStatistics::FeatureType feature_type,
                                                  SimpleFeatureCollection &collection) const -> void {
    result.startFeature(feature_type);

    size_t number_of_features = collection.getFeatureCount();

    for (auto &key : collection.feature_attributes.getNumericKeys()) {
        NumberStatistics number_statistics;

        auto &array = collection.feature_attributes.numeric(key);
        for (size_t i = 0; i < number_of_features; ++i) {
            number_statistics.add(array.get(i));
        }

        result.addFeatureNumericStats(key, number_statistics);
    }

    for (auto &key : collection.feature_attributes.getTextualKeys()) {
        std::unordered_map<std::string, size_t> value_map;

        auto &array = collection.feature_attributes.textual(key);
        for (size_t j = 0; j < number_of_features; ++j) {
            const std::string &value = array.get(j);

            auto iter = value_map.find(value);
            if (iter != value_map.end()) {
                iter->second += 1;
            } else {
                value_map.emplace(value, 1);
            }
        }

        std::vector<std::pair<std::string, std::size_t>> values(value_map.begin(), value_map.end());

        result.addFeatureTextStats(key, number_of_features, values.size(), values);
    }

    result.endFeature();
}

#endif
