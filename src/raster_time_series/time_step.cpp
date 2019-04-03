
#include "time_step.h"

using namespace rts;
using namespace boost::posix_time;

TimeStep::TimeStep() : unit(TimeUnit::Year), value(0) { }

TimeStep::TimeStep(TimeUnit unit, uint32_t value) : unit(unit), value(value) { }

TimeStep::TimeStep(const Json::Value &json) {
    unit   = parseTimeUnit(json["unit"].asString());
    value = json["value"].asUInt();
}

TimeUnit TimeStep::parseTimeUnit(const std::string &input) const {
    if(input == "Year")
        return TimeUnit::Year;
    else if(input == "Month")
        return TimeUnit::Month;
    else if(input == "Day")
        return TimeUnit::Day;
    else if(input == "Hour")
        return TimeUnit::Hour;
    else if(input == "Minute")
        return TimeUnit::Minute;
    else if(input == "Second")
        return TimeUnit::Second;
    else
        throw std::runtime_error("Could not parse TimeUnit: " + input);
}


void TimeStep::increase(boost::posix_time::ptime &time, uint32_t times) const {
    switch(unit){
        case TimeUnit::Year:
            time += boost::gregorian::years(value * times);
            break;
        case TimeUnit::Month:
            time += boost::gregorian::months(value * times);
            break;
        case TimeUnit::Day:
            time += boost::gregorian::days(value * times);
            break;
        case TimeUnit::Hour:
            time += boost::posix_time::hours(value * times);
            break;
        case TimeUnit::Minute:
            time += boost::posix_time::minutes(value * times);
            break;
        case TimeUnit::Second:
            time += boost::posix_time::seconds(value * times);
            break;
    }
}

void TimeStep::decrease(boost::posix_time::ptime &time, uint32_t times) const {
    switch(unit){
        case TimeUnit::Year:
            time -= boost::gregorian::years(value * times);
            break;
        case TimeUnit::Month:
            time -= boost::gregorian::months(value * times);
            break;
        case TimeUnit::Day:
            time -= boost::gregorian::days(value * times);
            break;
        case TimeUnit::Hour:
            time -= boost::posix_time::hours(value * times);
            break;
        case TimeUnit::Minute:
            time -= boost::posix_time::minutes(value * times);
            break;
        case TimeUnit::Second:
            time -= boost::posix_time::seconds(value * times);
            break;
    }
}
