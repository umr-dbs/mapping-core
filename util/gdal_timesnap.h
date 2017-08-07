#ifndef UTIL_GDAL_TIMESNAP_H_
#define UTIL_GDAL_TIMESNAP_H_

#include <map>
#include <time.h>
#include <json/json.h>
#include <dirent.h>
#include <sstream>
#include <fstream>
#include "util/exceptions.h"
#include "util/timeparser.h"

enum class TimeUnit {
    Year 	= 0,
    Month 	= 1,
    Day 	= 2,
    Hour 	= 3,
	Minute  = 4,
    Second  = 5
};

class GDALTimesnap {
	public:
		static TimeUnit createTimeUnit(std::string value);
		static const std::map<std::string, TimeUnit> string_to_TimeUnit;

		static tm snapToInterval(TimeUnit unit, int unitValue, tm startTime, tm wantedTime);
		static void handleOverflow(tm &snapped, TimeUnit intervalUnit);
		static tm tmDifference(tm &first, tm &second);
		static int getUnitDifference(tm diff, TimeUnit snapUnit);		

		static std::string tmStructToString(const tm *tm, std::string format);
		static std::string unixTimeToString(double unix_time, std::string format);

		static void setTimeUnitValueInTm(tm &time, TimeUnit unit, int value);
		static int getTimeUnitValueFromTm(tm &time, TimeUnit unit);
		static int minValueForTimeUnit(TimeUnit part);
		static int maxValueForTimeUnit(TimeUnit part);	
		static void printTime(tm &time);
		static int daysOfMonth(int year, int month);

		static Json::Value getDatasetJson(std::string wantedDatasetName, std::string datasetPath);
		static std::string getDatasetFilename(Json::Value datasetJson, double wantedTimeUnix);
};

#endif