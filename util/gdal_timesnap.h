#ifndef UTIL_GDAL_TIMESNAP_H_
#define UTIL_GDAL_TIMESNAP_H_

#include <map>
#include <ctime>

enum class TimeUnit {
    Year 	= 0,
    Month 	= 1,
    Day 	= 2,
    Hour 	= 3,
	Minute  = 4,
    Second  = 5
};

const std::map<std::string, TimeUnit> string_to_TimeUnit {
	{"Second", 	TimeUnit::Second},
	{"Minute", 	TimeUnit::Minute},
	{"Hour", 	TimeUnit::Hour},
	{"Day", 	TimeUnit::Day},
	{"Month", 	TimeUnit::Month},
	{"Year", 	TimeUnit::Year}
};

tm snapToInterval(TimeUnit unit, int unitValue, tm startTime, tm wantedTime);
void handleOverflow(tm &snapped, TimeUnit intervalUnit);
tm tmDifference(tm &first, tm &second);
int getUnitDifference(tm diff, TimeUnit snapUnit);		

std::string tmStructToString(tm *tm, std::string format);
std::string unixTimeToString(double unix_time, std::string format);

TimeUnit createTimeUnit(std::string value);
void setTimeUnitValueInTm(tm &time, TimeUnit unit, int value);
int getTimeUnitValueFromTm(tm &time, TimeUnit unit);
int minValueForTimeUnit(TimeUnit part);
int maxValueForTimeUnit(TimeUnit part);

#endif