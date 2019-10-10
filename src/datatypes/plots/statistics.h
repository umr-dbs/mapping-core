#ifndef PLOT_STATISTICS_H
#define PLOT_STATISTICS_H

#include <string>
#include <json/json.h>
#include <util/NumberStatistics.h>

#include "datatypes/plot.h"

/**
 * This class models a one dimensional histograms with variable number of buckets
 */
class LayerStatistics : public GenericPlot {
    public:
        auto toJSON() const -> const std::string override;

        auto clone() const -> std::unique_ptr<GenericPlot> override;

        auto addRasterStats(NumberStatistics &number_statistics) -> void;

        enum class FeatureType {
                POINTS,
                LINES,
                POLYGONS,
        };

        auto startFeature(FeatureType feature_type) -> void;

        auto addFeatureTextStats(std::string &name,
                                 std::size_t count,
                                 std::size_t distinct_count,
                                 std::vector<std::pair<std::string, std::size_t>> &value_counts) -> void;

        auto addFeatureNumericStats(std::string &name, NumberStatistics &number_statistics) -> void;

        auto endFeature() -> void;

    private:
        Json::Value rasters = Json::Value(Json::arrayValue);
        Json::Value points = Json::Value(Json::arrayValue);
        Json::Value lines = Json::Value(Json::arrayValue);
        Json::Value polygons = Json::Value(Json::arrayValue);
        Json::Value current_feature = Json::Value(Json::objectValue);
        FeatureType current_feature_type;
};

#endif
