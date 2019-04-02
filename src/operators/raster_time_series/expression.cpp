
#include "raster_time_series_operator.h"
#include "raster_time_series/operator_state/operator_state.h"
#include "raster_time_series/expression.h"

namespace rts {

    /**
     * Simple expression operator for raster time series. This is ported from my bachelor thesis to test the
     * integration of raster time series. Should be integrated into the other expression operator.
     * It is based on the rts::Expression class that handles creation of getRaster closures based on an expression string.
     *
     * Parameters:
     *      - "expression" : string defining the expression.
     */
    class ExpressionOperator : public RasterTimeSeriesOperator {
    public:
        ExpressionOperator(int *sourcecounts, GenericOperator **sources, Json::Value &params);

        ~ExpressionOperator() override = default;

#ifndef MAPPING_OPERATOR_STUBS
        std::unique_ptr<RasterTimeSeries> getRasterTimeSeries(const QueryRectangle &rect, const QueryTools &tools, ProcessingOrder processingOrder) override;
#endif
        OptionalDescriptor nextDescriptor(OperatorState &state, const QueryRectangle &rect, const QueryTools &tools) override;
    protected:
        void writeSemanticParameters(std::ostringstream &stream) override;
        bool supportsProcessingOrder(ProcessingOrder processingOrder) const override;

        std::string expressionDefinition;
    };



    /**
     * State class for the RTS expression operator.
     */
    class ExpressionState : public OperatorState {
    public:
        ExpressionState(const std::string &expressionDefinition, std::vector<std::unique_ptr<RasterTimeSeries>> &&input)
                : OperatorState(std::move(input)), expression(expressionDefinition) { }
        /**
         * rts::Expression object that handles the parsing of an expression and creation of the getRaster closures.
         */
        Expression expression;
    };



}

using namespace rts;

REGISTER_OPERATOR(ExpressionOperator, "rts_expression");

ExpressionOperator::ExpressionOperator(int *sourcecounts, GenericOperator **sources, Json::Value &params)
        : RasterTimeSeriesOperator(sourcecounts, sources, params)
{
    expressionDefinition = params["expression"].asString();
}

void ExpressionOperator::writeSemanticParameters(std::ostringstream &stream) {
    Json::Value params;
    params["expression"] = expressionDefinition;
    Json::FastWriter writer;
    stream << writer.write(params);
}

std::unique_ptr<RasterTimeSeries> ExpressionOperator::getRasterTimeSeries(const QueryRectangle &rect, const QueryTools &tools, ProcessingOrder processingOrder) {

    if(!supportsProcessingOrder(processingOrder))
        throw OperatorException(concat("ExpressionOperator does not support the given processing order: ", processingOrder == rts::ProcessingOrder::Temporal ? "Temporal." : "Spatial."));

    int inputCount = getRasterTimeSeriesSourceCount();
    std::vector<std::unique_ptr<RasterTimeSeries>> inputs;
    inputs.reserve(inputCount);
    for (int i = 0; i < inputCount; ++i) {
        inputs.emplace_back(getRasterTimeSeriesFromSource(i, rect, tools, processingOrder));
    }
    auto state = std::make_unique<ExpressionState>(expressionDefinition, std::move(inputs));
    return std::make_unique<RasterTimeSeries>(this, std::move(state), rect, tools);
}

OptionalDescriptor ExpressionOperator::nextDescriptor(OperatorState &state, const QueryRectangle &rect, const QueryTools &tools) {

    auto &s = state.castTo<ExpressionState>();

    std::vector<OptionalDescriptor> inputDescs;
    inputDescs.reserve(s.input.size());
    for(int i = 0; i < s.input.size(); ++i){
        inputDescs.emplace_back(s.input[i]->nextDescriptor());
        //TODO: Can something be calculated when one of the inputs is nullopt?
        if(!inputDescs[i])
            return std::nullopt;
    }

    //TODO: what is spatial info, what is temporal info of result?
    //TODO: check if one operator is only nodata? have it behave like a 0 for add and mul?
    DescriptorInfo descInfo(inputDescs[0], this);

    auto getter = s.expression.createGetter(std::move(inputDescs));

    return std::make_optional<Descriptor>(std::move(getter), descInfo);
}

bool ExpressionOperator::supportsProcessingOrder(ProcessingOrder processingOrder) const {
    return processingOrder == ProcessingOrder::Temporal || processingOrder == ProcessingOrder::Spatial;
}
