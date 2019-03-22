#include "operators/operator.h"
#include "datatypes/plots/json.h"
#include "datatypes/raster.h"

#include <string>
#include <json/json.h>
#include <limits>
#include <vector>
#include <array>
#include <cmath>
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
class StatisticsOperator: public GenericOperator {
	public:
	    StatisticsOperator(int sourcecounts[], GenericOperator *sources[],	Json::Value &params);
		virtual ~StatisticsOperator();

#ifndef MAPPING_OPERATOR_STUBS
		virtual std::unique_ptr<GenericPlot> getPlot(const QueryRectangle &rect, const QueryTools &tools);
#endif
	protected:
		void writeSemanticParameters(std::ostringstream& stream);

	private:
		uint32_t rasterWidth;
		uint32_t rasterHeight;

#ifndef MAPPING_OPERATOR_STUBS
		void processFeatureCollection(Json::Value &json, SimpleFeatureCollection &collection) const;
#endif
};

REGISTER_OPERATOR(StatisticsOperator, "statistics");

StatisticsOperator::StatisticsOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
	assumeSources(1);

	rasterWidth = params.get("raster_width", 256).asUInt();
	rasterHeight = params.get("raster_height", 256).asUInt();
}

StatisticsOperator::~StatisticsOperator() {}


void StatisticsOperator::writeSemanticParameters(std::ostringstream& stream) {
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
		        QueryRectangle(rect, rect, QueryResolution::pixels(rasterWidth, rasterHeight)), tools, RasterQM::EXACT);

		double min = std::numeric_limits<double>::max();
		double max = -std::numeric_limits<double>::max();
		for (int x = 0; x < raster->width; ++x) {
			for (int y = 0; y < raster->height; ++y) {
				min = std::min(min, raster->getAsDouble(x, y));
				max = std::max(min, raster->getAsDouble(x, y));
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
    size_t count = collection.getFeatureCount();

    Json::Value collectionJson(Json::objectValue);

    for (auto &key : collection.feature_attributes.getNumericKeys()) {
            double min = std::numeric_limits<double>::max();
            double max = -std::numeric_limits<double>::max();

            auto &array = collection.feature_attributes.numeric(key);
            for (int j = 0; j < count; ++j) {
                double v = array.get(j);
                min = std::min(min, v);
                max = std::max(min, v);
            }

            Json::Value attributeJson(Json::objectValue);
            attributeJson["min"] = min;
            attributeJson["max"] = max;

            collectionJson[key] = attributeJson;
        }

    json.append(collectionJson);
}

#endif
