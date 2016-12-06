#include "datatypes/raster.h"
#include "datatypes/raster/typejuggling.h"
#include "raster/opencl.h"
#include "operators/operator.h"
#include "util/formula.h"
#include "util/enumconverter.h"

#include <limits>
#include <memory>
#include <sstream>
#include <json/json.h>
#include <iostream>

enum class AggregationType {
	MIN, MAX, AVG
};

const std::vector<std::pair<AggregationType, std::string> > AggregationTypeMap {
		std::make_pair(AggregationType::MIN, "min"), std::make_pair(
				AggregationType::MAX, "max"), std::make_pair(
				AggregationType::AVG, "avg") };

static EnumConverter<AggregationType> AggregationTypeConverter(
		AggregationTypeMap);

/**
 * Operator that aggregates a given input raster over a given time interval
 *
 * Parameters:
 * - duration: the length of the time interval in seconds as double
 * - aggregation: "min", "max", "avg"
 */
class TemporalAggregationOperator: public GenericOperator {
public:
	TemporalAggregationOperator(int sourcecounts[], GenericOperator *sources[],
			Json::Value &params);
	virtual ~TemporalAggregationOperator();

#ifndef MAPPING_OPERATOR_STUBS
	std::unique_ptr<Raster2D<double>> createAccumulator(GenericRaster &raster);
	virtual std::unique_ptr<GenericRaster> getRaster(const QueryRectangle &rect,
			const QueryTools &tools);
#endif
protected:
	void writeSemanticParameters(std::ostringstream& stream);
private:
	double duration;
	AggregationType aggregationType;
};

TemporalAggregationOperator::TemporalAggregationOperator(int sourcecounts[],
		GenericOperator *sources[], Json::Value &params) :
		GenericOperator(sourcecounts, sources) {
	assumeSources(1);
	if (!params.isMember("duration")) {
		throw new OperatorException(
				"TemporalAggregationOperator: Parameter duration is missing");
	}
	duration = params.get("duration", 0).asDouble();

	if (!params.isMember("aggregation")) {
		throw new OperatorException(
				"TemporalAggregationOperator: Parameter aggregation is missing");
	}
	aggregationType = AggregationTypeConverter.from_string(
			params.get("aggregation", 0).asString());
}

TemporalAggregationOperator::~TemporalAggregationOperator() {
}
REGISTER_OPERATOR(TemporalAggregationOperator, "temporal_aggregation");

void TemporalAggregationOperator::writeSemanticParameters(
		std::ostringstream& stream) {
	Json::Value json(Json::objectValue);
	json["duration"] = duration;
	json["aggregation"] = AggregationTypeConverter.to_string(aggregationType);

	stream << json;
}

#ifndef MAPPING_OPERATOR_STUBS

template<typename T>
struct Accumulate {
	static void execute(Raster2D<T> *raster, DataDescription &dd,
			Raster2D<double> *accumulator, AggregationType aggregationType) {
		raster->setRepresentation(GenericRaster::Representation::CPU);
		// accumulate
		for (int x = 0; x < raster->width; ++x) {
			for (int y = 0; y < raster->height; ++y) {
				T rasterValue = raster->get(x, y);
				double accValue = accumulator->get(x, y);
				double newValue;

				if (dd.is_no_data(rasterValue) || std::isnan(accValue)) {
					newValue = NAN;
				} else {

					switch (aggregationType) {
					case AggregationType::MIN:
						newValue = std::min(static_cast<double>(rasterValue), accValue);
						break;
					case AggregationType::MAX:
						newValue = std::max(static_cast<double>(rasterValue), accValue);
						break;
					case AggregationType::AVG:
						newValue = static_cast<double>(rasterValue) + accValue;
						break;
					}
				}
				accumulator->set(x, y, newValue);
			}
		}

	}
};

template<typename T>
struct Output {
	static std::unique_ptr<GenericRaster> execute(Raster2D<T> *raster,
			DataDescription &dd, Raster2D<double> *accumulator,
			AggregationType aggregationType, size_t n) {
		auto output = make_unique<Raster2D<T>>(raster->dd, raster->stref,
				raster->width, raster->height);


		for (int x = 0; x < raster->width; ++x) {
			for (int y = 0; y < raster->height; ++y) {
				double accValue = accumulator->get(x, y);
				T outputValue;

				if (std::isnan(accValue)) {
					if (!dd.has_no_data) {
						throw OperatorException(
								"Temporal_Aggregation: No data value in data without no data value");
					} else {
						outputValue = dd.no_data;
					}
				} else {
					switch (aggregationType) {
					case AggregationType::MIN:
						outputValue = static_cast<T>(accValue);
						break;
					case AggregationType::MAX:
						outputValue = static_cast<T>(accValue);
						break;
					case AggregationType::AVG:
						// TODO: solve for non-equi length time validities
						outputValue = static_cast<T>(accValue / n);
						break;
					}
				}
				output->set(x, y, outputValue);
			}
		}

		return std::unique_ptr<GenericRaster>(output.release());
	}
};

std::unique_ptr<Raster2D<double>> TemporalAggregationOperator::createAccumulator(
		GenericRaster &raster) {
	DataDescription accumulatorDD = raster.dd;
	accumulatorDD.datatype = GDT_Float64;

	auto accumulator = make_unique<Raster2D<double>>(accumulatorDD,
			raster.stref, raster.width, raster.height);

	// initialize accumulator
	double initValue;
	switch (aggregationType) {
	case AggregationType::MIN:
		initValue = std::numeric_limits<double>::max();
		break;
	case AggregationType::MAX:
		initValue = std::numeric_limits<double>::min();
		break;
	case AggregationType::AVG:
		initValue = 0;
		break;
	}

	for (int x = 0; x < accumulator->width; ++x) {
		for (int y = 0; y < accumulator->height; ++y) {
			accumulator->set(x, y, initValue);
		}
	}

	return accumulator;
}

std::unique_ptr<GenericRaster> TemporalAggregationOperator::getRaster(
		const QueryRectangle &rect, const QueryTools &tools) {
	// TODO: compute using OpenCL
	// TODO: allow LOOSE computations

	auto input = getRasterFromSource(0, rect, tools, RasterQM::EXACT);
	auto accumulator = createAccumulator(*input);

	size_t n = 0;
	GenericRaster* raster = input.get();
	QueryRectangle nextRect = rect;
	// TODO: what to do with rasters that are partially contained in timespan?
	while (nextRect.t1 < rect.t1 + duration) {
		std::unique_ptr<GenericRaster> rasterFromSource;
		if (nextRect.t1 > rect.t1) {
			rasterFromSource = getRasterFromSource(0, nextRect, tools,
					RasterQM::EXACT);
			raster = rasterFromSource.get();
		}

		// accumulate
		callUnaryOperatorFunc<Accumulate>(raster, rasterFromSource->dd,
				accumulator.get(), aggregationType);

		nextRect.t1 = raster->stref.t2;
		nextRect.t2 = nextRect.t1 + nextRect.epsilon();
		n += 1;
	}

	auto output = callUnaryOperatorFunc<Output>(input.get(), input->dd,
			accumulator.get(), aggregationType, n);

	return output;
}

#endif

