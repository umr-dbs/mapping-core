#ifndef DATATYPES_UNIT_H
#define DATATYPES_UNIT_H

#include <string>
#include <map>
#include <utility>
#include <vector>
#include "util/sizeutil.h"

// Forward declaration of Json::Value
namespace Json {
    class Value;
}


/**
 * @class Unit
 *
 * A Unit contains semantical information about a set of values (i.e. a Raster's pixels or an Attribute)
 * These are:
 *
 * - What is measured? e.g. Temperature, Elevation, Precipitation, ...
 * - What unit is the measurement in? e.g. Celsius, Kelvin, Meters, cm/day, ...
 * - Does it have a minimum or maximum value?
 * - is it a continuous or a discrete value (e.g. temperature vs. classification)?
 * - an optional set of parameters, e.g. names for a classification's classes
 *
 * Units can suggest a default colorization.
 */
class Unit {
    public:
        /**
         * A choice of pixel interpolation
         */
        enum class Interpolation {
                Unknown,
                Continuous,
                Discrete
        };
    protected:
        /**
         * A class value of a classification
         */
        class Class {
            public:
                /**
                 * Construct a Class by its name
                 */
                explicit Class(std::string name) : name(std::move(name)) {}

                /**
                 * Retrieve the name of the class
                 */
                const std::string &getName() const { return name; }

                /**
                 * Retrieve the number of bytes to store the class name
                 */
                size_t get_byte_size() const { return SizeUtil::get_byte_size(name); }

            private:
                std::string name;
        };

        class Uninitialized_t {
        };

    public:
        // No default constructor, because uninitialized Units are invalid and thus potentially dangerous
        Unit() = delete;

        /**
         * Construct a unit without initializing any values. This is only useful if you absolutely must
         * default construct a unit. For example, if a Unit is a member of your class, you can force it to
         * remain uninitialized in the initializer list. Then you can properly initialize it in the constructor
         * body.
         *
         * The only guaranteed way to turn an uninitialized unit into a valid unit is to copy-assign a valid unit.
         *
         * @param u pass the constant Unit::UNINITIALIZED
         */
        explicit Unit(Uninitialized_t u);

        static const Uninitialized_t UNINITIALIZED;

        /**
         * Construct a unit from its JSON representation
         *
         * @param json the JSON representation as a string
         */
        explicit Unit(const std::string &json);

        /**
         * Construct a unit from its JSON representation
         *
         * @param json the JSON representation as a Json::Value
         */
        explicit Unit(const Json::Value &json);

        /**
         * Construct a unit containing just the minimum information to be valid.
         *
         * @param measurement The measurement.
         * @param unit The unit.
         */
        Unit(std::string measurement, std::string unit);

        ~Unit() = default;

        /**
         * Verify if a Unit is considered valid. Throws an exception if it is not.
         */
        void verify() const;

        /**
         * A named constructor for an unknown unit.
         *
         * @return A valid Unit with unknown measurement, unit and interpolation
         */
        static Unit unknown();

        /**
         * Returns the unit's JSON representation
         * @return the unit's JSON representation as a string
         */
        std::string toJson() const;

        /**
         * Returns the unit's JSON representation
         * @return the unit's JSON representation as a Json::Value
         */
        Json::Value toJsonObject() const;

        /**
         * Returns whether the interpolation is continuous or not.
         * @return true if the interpolation is continuous, false otherwise
         */
        bool isContinuous() const { return interpolation == Interpolation::Continuous; }

        /**
         * Returns whether the interpolation is discrete or not.
         * @return true if the interpolation is discrete, false otherwise
         */
        bool isDiscrete() const { return interpolation == Interpolation::Discrete; }

        /**
         * Overrides the interpolation
         * @param i the new interpolation method
         */
        void setInterpolation(Interpolation i) { interpolation = i; }

        /**
         * Returns whether the unit is a classification
         * @return true if the unit is a classification, false otherwise
         */
        bool isClassification() const { return unit == "classification"; }

        /**
         * Returns the min value, defaults to -inf
         * @return the min value
         */
        double getMin() const { return min; }

        /**
         * Returns the max value, defaults to +inf
         * @return the max value
         */
        double getMax() const { return max; }

        /**
         * Returns true if both min and max are set and finite
         * @return true if both min and max are set and finite, false otherwise
         */
        bool hasMinMax() const;

        /**
         * Overrides the min and max values
         */
        void setMinMax(double min, double max);

        /**
         * returns the measurement (in lower case)
         * @return the measurement
         */
        const std::string &getMeasurement() const { return measurement; }

        /**
         * returns the unit (in lower case)
         * @return the unit
         */
        const std::string &getUnit() const { return unit; }

        /**
         * the size of this object in memory (in bytes)
         * @return the size of this object in bytes
         */
        size_t get_byte_size() const;

    private:
        void init(const Json::Value &json);

        std::string measurement;
        std::string unit;
        Interpolation interpolation;
        std::map<int32_t, Class> classes;
        double min{}, max{};
};


#endif
