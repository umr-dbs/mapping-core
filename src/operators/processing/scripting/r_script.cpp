#include "datatypes/raster.h"
#include "datatypes/plots/text.h"
#include "datatypes/plots/png.h"
#include "datatypes/pointcollection.h"
#include "datatypes/linecollection.h"
#include "datatypes/polygoncollection.h"
#include "operators/operator.h"
#include "operators/processing/scripting/r_script.h"
#include "util/configuration.h"

#include <mutex>
#include <functional>
#include <json/json.h>

const size_t DEFAULT_PLOT_WIDTH_PX = 1000;
const size_t DEFAULT_PLOT_HEIGHT_PX = 1000;

/**
 * Operator that executes an R script on the R server and return the result
 *
 * Parameters:
 * - source: the source code of the R script
 * - result: the result of the R script
 *   - points
 *   - raster
 *   - text
 *   - plot
 * - plot_width: the width of created plots in pixels, optional, default = 1000
 * - plot_height: the height of created plots in pixels, optional, default = 1000
 */
class RScriptOperator : public GenericOperator {
	public:
		RScriptOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params);

        ~RScriptOperator() override;

#ifndef MAPPING_OPERATOR_STUBS

        auto getRaster(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<GenericRaster> override;

        auto getPointCollection(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<PointCollection> override;

        auto getLineCollection(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<LineCollection> override;

        auto getPolygonCollection(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<PolygonCollection> override;

        auto getPlot(const QueryRectangle &rect, const QueryTools &tools) -> std::unique_ptr<GenericPlot> override;

        auto runScript(const QueryRectangle &rect, char requested_type, const QueryTools &tools) -> std::unique_ptr<BinaryReadBuffer>;
#endif
	protected:
		auto writeSemanticParameters(std::ostringstream& stream) -> void override;
	private:
		std::string source;
		std::string result_type;

		size_t plot_width = DEFAULT_PLOT_WIDTH_PX;
		size_t plot_height = DEFAULT_PLOT_HEIGHT_PX;
};


static void replace_all(std::string &str, const std::string &search, const std::string &replace) {
    size_t start_pos = 0;
    while ((start_pos = str.find(search, start_pos)) != std::string::npos) {
        str.replace(start_pos, search.length(), replace);
        start_pos += replace.length();
    }
}

RScriptOperator::RScriptOperator(int sourcecounts[], GenericOperator *sources[], Json::Value &params) : GenericOperator(sourcecounts, sources) {
	source = params["source"].asString();
	replace_all(source, "\r\n", "\n");
	result_type = params["result"].asString();

	if(result_type == "plot") {
		plot_width = params.get("plot_width", DEFAULT_PLOT_WIDTH_PX).asUInt64();
		plot_height = params.get("plot_height", DEFAULT_PLOT_HEIGHT_PX).asUInt64();
	}
}
RScriptOperator::~RScriptOperator() = default;

REGISTER_OPERATOR(RScriptOperator, "r_script");

void RScriptOperator::writeSemanticParameters(std::ostringstream& stream) {
	Json::Value json(Json::objectValue);
	json["source"] = source;
	json["result"] = result_type;

	if(result_type == "plot") {
		json["plot_width"] = plot_width;
		json["plot_height"] = plot_height;
	}

	stream << json;
}

#ifndef MAPPING_OPERATOR_STUBS

auto RScriptOperator::runScript(const QueryRectangle &rect, char requested_type,
                                const QueryTools &tools) -> std::unique_ptr<BinaryReadBuffer> {

	auto host = Configuration::get<std::string>("operators.r.location");
	BinaryStream stream = BinaryStream::connectURL(host);

	BinaryWriteBuffer request;
	request.write<const int &>(RSERVER_MAGIC_NUMBER);
	request.write<char &>(requested_type);
	request.write<std::string &>(source);
	request.write<int>(getRasterSourceCount());
	request.write<int>(getPointCollectionSourceCount());
	request.write<int>(getLineCollectionSourceCount());
	request.write<int>(getPolygonCollectionSourceCount());
	request.write<const QueryRectangle &>(rect);
	request.write<int>(600); // timeout
	if(result_type == "plot") {
		request.write<size_t &>(plot_width);
		request.write<size_t &>(plot_height);
	}
	stream.write(request);

	while (true) {
		auto response = std::make_unique<BinaryReadBuffer>();
		stream.read(*response);
		auto type = response->read<char>();

		// fprintf(stderr, "Server got command %d\n", (int) type);

		if (type > 0) {
			auto childidx = response->read<int>();
			QueryRectangle qrect(*response);

            BinaryWriteBuffer requested_data;
            std::unique_ptr<SpatioTemporalResult> result; // to keep result alive
            switch (type) {
                case RSERVER_TYPE_RASTER: {
                    auto raster = getRasterFromSource(childidx, qrect, tools);
                    requested_data.write<GenericRaster &>(*raster, true);
                    result = std::move(raster);
                    break;
                }

                case RSERVER_TYPE_POINTS: {
                    auto points = getPointCollectionFromSource(childidx, qrect, tools);
                    requested_data.write<PointCollection &>(*points, true);
                    result = std::move(points);
                    break;
                }

                case RSERVER_TYPE_LINES: {
                    auto lines = getLineCollectionFromSource(childidx, qrect, tools);
                    requested_data.write<LineCollection &>(*lines, true);
                    result = std::move(lines);
                    break;
                }

                case RSERVER_TYPE_POLYGONS: {
                    auto polygons = getPolygonCollectionFromSource(childidx, qrect, tools);
                    requested_data.write<PolygonCollection &>(*polygons, true);
                    result = std::move(polygons);
                    break;
                }

                default: {
                    throw OperatorException("R: invalid data type requested by server");
                }
            }
            stream.write(requested_data);
		}
		else {
			if (type == -RSERVER_TYPE_ERROR) {
				std::string err;
				response->read(&err);
				throw OperatorException(concat("R exception: ", err));
			}

			if (type != -requested_type) {
                throw OperatorException("R: wrong data type returned by server");
            }

			return response; // the caller will read the result object from the stream
		}
	}
}


auto RScriptOperator::getRaster(const QueryRectangle &rect,
                                const QueryTools &tools) -> std::unique_ptr<GenericRaster> {
	if (result_type != "raster")
		throw OperatorException("This R script does not return rasters");

	auto response = runScript(rect, RSERVER_TYPE_RASTER, tools);

	auto raster = GenericRaster::deserialize(*(response.get()));
	return raster;
}

auto RScriptOperator::getPointCollection(const QueryRectangle &rect,
                                         const QueryTools &tools) -> std::unique_ptr<PointCollection> {
	if (result_type != "points")
		throw OperatorException("This R script does not return a point collection");

	auto response = runScript(rect, RSERVER_TYPE_POINTS, tools);

	auto points = std::make_unique<PointCollection>(*(response.get()));
	return points;
}

auto RScriptOperator::getLineCollection(const QueryRectangle &rect,
                                        const QueryTools &tools) -> std::unique_ptr<LineCollection> {
    if (result_type != "lines") {
        throw OperatorException("This R script does not return a polygon collection");
    }

    auto response = runScript(rect, RSERVER_TYPE_LINES, tools);

    return std::make_unique<LineCollection>(*response);
}

auto RScriptOperator::getPolygonCollection(const QueryRectangle &rect,
                                           const QueryTools &tools) -> std::unique_ptr<PolygonCollection> {
	if (result_type != "polygons") {
		throw OperatorException("This R script does not return a polygon collection");
	}

	auto response = runScript(rect, RSERVER_TYPE_POLYGONS, tools);

	return std::make_unique<PolygonCollection>(*response);
}

auto RScriptOperator::getPlot(const QueryRectangle &rect,
                              const QueryTools &tools) -> std::unique_ptr<GenericPlot> {
	bool wants_text = (result_type == "text");
	bool wants_plot = (result_type == "plot");
	if (!wants_text && !wants_plot)
		throw OperatorException("This R script does not return a plot");

	auto response = runScript(rect, wants_text ? RSERVER_TYPE_STRING : RSERVER_TYPE_PLOT, tools);

	std::string result;
	response->read(&result);
	if (wants_text)
		return std::unique_ptr<GenericPlot>(new TextPlot(result));
	else
		return std::unique_ptr<GenericPlot>(new PNGPlot(result));
}

#endif

