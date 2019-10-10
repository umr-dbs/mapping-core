#include <algorithm>
#include "statistics.h"

auto LayerStatistics::toJSON() const -> const std::string {
    Json::Value data(Json::objectValue);

    if (!this->rasters.empty()) data["rasters"] = this->rasters;
    if (!this->points.empty()) data["points"] = this->points;
    if (!this->lines.empty()) data["lines"] = this->lines;
    if (!this->polygons.empty()) data["polygons"] = this->polygons;

    Json::Value root(Json::objectValue);

    root["type"] = "layer_statistics";
    root["data"] = data;

    Json::FastWriter writer;
    return writer.write(root);
}

auto LayerStatistics::clone() const -> std::unique_ptr<GenericPlot> {
    std::unique_ptr<LayerStatistics> clone = std::make_unique<LayerStatistics>();

    clone->rasters = this->rasters;
    clone->points = this->points;
    clone->lines = this->lines;
    clone->polygons = this->polygons;

    return std::unique_ptr<GenericPlot>(clone.release());
}

auto LayerStatistics::addRasterStats(NumberStatistics &number_statistics) -> void {
    Json::Value json(Json::objectValue);
    json["count"] = number_statistics.count();
    json["nan_count"] = number_statistics.nan_count();
    json["min"] = number_statistics.min();
    json["max"] = number_statistics.max();
    json["mean"] = number_statistics.mean();
    json["stddev"] = number_statistics.std_dev();

    this->rasters.append(json);
}

auto LayerStatistics::addFeatureNumericStats(std::string &name, NumberStatistics &number_statistics) -> void {
    Json::Value json(Json::objectValue);
    json["count"] = number_statistics.count();
    json["nan_count"] = number_statistics.nan_count();
    json["min"] = number_statistics.min();
    json["max"] = number_statistics.max();
    json["mean"] = number_statistics.mean();
    json["stddev"] = number_statistics.std_dev();

    this->current_feature[name] = json;
}

auto LayerStatistics::addFeatureTextStats(std::string &name,
                                          std::size_t count,
                                          std::size_t distinct_count,
                                          std::vector<std::pair<std::string, std::size_t>> &value_counts) -> void {
    std::sort(value_counts.begin(), value_counts.end(), [](auto &left, auto &right) {
        if (left.second != right.second) {
            return left.second > right.second;
        } else {
            return left.first <= right.first;
        }
    });

    const double min_percentage_boundary = 0.001; // TODO: make a user parameter
    const size_t max_k = 20; // TODO: make a user parameter
    size_t k = 0;

    Json::Value value_counts_json(Json::arrayValue);
    for (const auto &kv : value_counts) {
        double percentage = ((double) kv.second) / ((double) count);
        if (percentage < min_percentage_boundary) {
            break;
        }

        k += 1;
        if (k > max_k) {
            break;
        }

        Json::Value kv_json(Json::arrayValue);
        kv_json.append(kv.first);
        kv_json.append(kv.second);
        value_counts_json.append(kv_json);
    }

    Json::Value json(Json::objectValue);
    json["count"] = count;
    json["distinct_values"] = distinct_count;
    json["value_counts"] = value_counts_json;

    this->current_feature[name] = json;
}

auto LayerStatistics::startFeature(LayerStatistics::FeatureType feature_type) -> void {
    this->current_feature_type = feature_type;
}

auto LayerStatistics::endFeature() -> void {
    switch (this->current_feature_type) {
        case FeatureType::POINTS:
            this->points.append(this->current_feature);
            break;
        case FeatureType::LINES:
            this->lines.append(this->current_feature);
            break;
        case FeatureType::POLYGONS:
            this->polygons.append(this->current_feature);
            break;
    }

    this->current_feature.clear();
}
