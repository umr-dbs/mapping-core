
#ifndef RASTER_TIME_SERIES_TIME_STEP_H
#define RASTER_TIME_SERIES_TIME_STEP_H

#include <boost/date_time/posix_time/posix_time.hpp>
#include <json/json.h>


namespace rts {

    /**
    * Enum representing the unit of a date.
    */
    enum class TimeUnit {
        Year 	= 0,
        Month 	= 1,
        Day 	= 2,
        Hour 	= 3,
        Minute  = 4,
        Second  = 5
    };

    /**
     * Class representing time step by a unit (year, month, day, ...) and a length. E.g. with the unit month
     * and the length 3 the time step represents time steps of three months.
     */
    class TimeStep {
    public:
        TimeStep();
        TimeStep(TimeUnit unit, uint32_t length);
        explicit TimeStep(const Json::Value &json);

        /**
         * The time unit of this step, e.g. Month or Hour.
         */
        TimeUnit unit;

        /**
         * How many units this step covers.
         */
        uint32_t value;

        /**
         * Increase the passed time by the step described by this object.
         * @param time Reference to the time that is to be increased by this time step.
         * @param times Factor to allow increasing multiple times with one call. Standard is 1.
         */
        void increase(boost::posix_time::ptime &time, uint32_t times = 1) const;

        /**
         * Decrease the passed time by the step described by this object.
         * @param time Reference to the time that is to be decreased by this time step.
         * @param times Factor to allow decreasing multiple times with one call. Standard is 1.
         */
        void decrease(boost::posix_time::ptime &time, uint32_t times = 1) const;

        TimeUnit parseTimeUnit(const std::string &input) const;
    };

}

#endif //RASTER_TIME_SERIES_TIME_STEP_H
