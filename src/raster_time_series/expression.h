
#ifndef RASTER_TIME_SERIES_EXPRESSION_H
#define RASTER_TIME_SERIES_EXPRESSION_H

#include <json/json.h>
#include <functional>
#include "datatypes/raster.h"
#include "datatypes/descriptor.h"

namespace rts {

    /**
     * Class for handling raster expressions. It supports unary and binary expressions.
     * It is created from a string like "A + B" or "A * 3.56".
     * Supported operators are: +,-,*,/,%
     * Raster are represented by the capital letters A and B.
     * A represents the first, B the second input raster.
     * Numeric values can not be 0.0, because of the double parsing with std::strtod
     * (returns 0.0, when string is not parseable).
     */
    class Expression {
    public:

        enum class Operator {
            ADD,
            SUB,
            DIV,
            MUL,
            MOD
        };

        enum class OperandType {
            Raster,
            Number
        };

        class Operand {
        public:
            static Operand createFromString(std::string &str);
            static Operand createRasterOperand(int index);
            static Operand createNumberOperand(double numericValue);

            OperandType type;
            union {
                double numericValue;
                int rasterIndex;
            };
        };

        explicit Expression(const Json::Value &def);
        explicit Expression(const std::string &expr);
        std::function<UniqueRaster(const Descriptor&)> createGetter(std::vector<OptionalDescriptor> &&inputs) const;
    private:
        Operator op;
        int expectedInputs;
        Operand firstOperand;
        Operand secondOperand;
    };

}

#endif //RASTER_TIME_SERIES_EXPRESSION_H
