#include "operators/operator.h"
#include "datatypes/plots/json.h"
#include "datatypes/raster.h"

#include <string>
#include <json/json.h>
#include <limits>
#include <vector>
#include <cmath>
#include <unordered_map>
#include <algorithm>
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
 *	{"data":{"lines":[],"points":[{"population":{"max":300001,"min":267000}}],"polygons":[],"rasters":[{"max":0,"min":0}]},"type":"json"}
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

        void processFeatureCollection(Json::Value &json, SimpleFeatureCollection &collection) const;

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

std::unique_ptr<GenericPlot> StatisticsOperator::getPlot(const QueryRectangle &rect, const QueryTools &tools) {
    Json::Value result(Json::objectValue);

    Json::Value rasters(Json::arrayValue);
    Json::Value points(Json::arrayValue);
    Json::Value lines(Json::arrayValue);
    Json::Value polygons(Json::arrayValue);

    // TODO: compute statistics using OpenCL
    for (int i = 0; i < getRasterSourceCount(); ++i) {
        auto raster = getRasterFromSource(i,
                                          QueryRectangle(rect, rect,
                                                         QueryResolution::pixels(rasterWidth, rasterHeight)), tools,
                                          RasterQM::EXACT);

        double min = std::numeric_limits<double>::max();
        double max = -std::numeric_limits<double>::max();
        for (int x = 0; x < raster->width; ++x) {
            for (int y = 0; y < raster->height; ++y) {
                double value = raster->getAsDouble(x, y);
                if (!raster->dd.is_no_data(value)) {
                    min = std::min(min, value);
                    max = std::max(min, value);
                }
            }
        }

        Json::Value rasterJson(Json::objectValue);
        rasterJson["min"] = min;
        rasterJson["max"] = max;

        rasters.append(rasterJson);
    }


    for (int i = 0; i < getPointCollectionSourceCount(); ++i) {
        auto collection = getPointCollectionFromSource(i, rect, tools);
        processFeatureCollection(points, *collection);
    }

    for (int i = 0; i < getLineCollectionSourceCount(); ++i) {
        auto collection = getLineCollectionFromSource(i, rect, tools);
        processFeatureCollection(lines, *collection);
    }

    for (int i = 0; i < getPolygonCollectionSourceCount(); ++i) {
        auto collection = getPolygonCollectionFromSource(i, rect, tools);
        processFeatureCollection(polygons, *collection);
    }


    result["rasters"] = rasters;
    result["points"] = points;
    result["lines"] = lines;
    result["polygons"] = polygons;

    return std::make_unique<JsonPlot>(result);
}

void StatisticsOperator::processFeatureCollection(Json::Value &json,
                                                  SimpleFeatureCollection &collection) const {
    size_t number_of_features = collection.getFeatureCount();

    Json::Value collectionJson(Json::objectValue);

    for (auto &key : collection.feature_attributes.getNumericKeys()) {
        double min = std::numeric_limits<double>::max();
        double max = -std::numeric_limits<double>::max();
        size_t count = 0;
        size_t nan_values = 0;
        double mean = 0;
        double delta, delta2;
        double M2 = 0;

        auto &array = collection.feature_attributes.numeric(key);
        for (size_t j = 0; j < number_of_features; ++j) {
            double value = array.get(j);

            if (std::isnan(value)) {
                nan_values += 1;
                continue;
            }

            min = std::min(min, value);
            max = std::max(max, value);

            // Welford's algorithm
            count += 1;
            delta = value - mean;
            mean += delta / count;
            delta2 = value - mean;
            M2 += delta * delta2;
        }

        Json::Value attributeJson(Json::objectValue);
        attributeJson["min"] = min;
        attributeJson["max"] = max;
        attributeJson["mean"] = mean;
        attributeJson["stddev"] = sqrt(M2 / count);
        attributeJson["count"] = count;
        attributeJson["nan_values"] = nan_values;

        collectionJson[key] = attributeJson;
    }

    for (auto &key : collection.feature_attributes.getTextualKeys()) {
        std::unordered_map<std::string, size_t> value_map;

        auto &array = collection.feature_attributes.textual(key);
        for (size_t j = 0; j < number_of_features; ++j) {
            const std::string& value = array.get(j);

            auto iter = value_map.find(value);
            if (iter != value_map.end()) {
                iter->second += 1;
            } else {
                value_map.emplace(value, 1);
            }
        }

        std::vector<std::pair<std::string, size_t>> values (value_map.begin(), value_map.end());

        std::sort(values.begin(), values.end(), [](auto &left, auto &right) {
            if (left.second != right.second) {
                return left.second > right.second;
            } else {
                return left.first <= right.first;
            }
        });

        Json::Value value_counts_json(Json::arrayValue);
        for (const auto &kv : values) {
            Json::Value kv_json(Json::arrayValue);
            kv_json.append(kv.first);
            kv_json.append(kv.second);
            value_counts_json.append(kv_json);
        }

        Json::Value attributeJson(Json::objectValue);
        attributeJson["count"] = number_of_features;
        attributeJson["distinct_values"] = values.size();
        attributeJson["value_counts"] = value_counts_json;

        collectionJson[key] = attributeJson;
    }

    json.append(collectionJson);
}

#endif
