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

/**
 * enum representing the unit of a date that is snapped to
 */
enum class TimeUnit {
    Year 	= 0,
    Month 	= 1,
    Day 	= 2,
    Hour 	= 3,
	Minute  = 4,
    Second  = 5
};

/*
 * Class encapsulating the time snapping functionality used for GDALSource.
 * Given a wanted time and the time snap values of the dataset it snaps the wanted time down to a time of an existing file.
 */
class GDALTimesnap {
	private:
		static void handleOverflow(tm &snapped, TimeUnit intervalUnit);
		static tm tmDifference(tm &first, tm &second);
		static int getUnitDifference(tm diff, TimeUnit snapUnit);		

		static std::string tmStructToString(const tm *tm, std::string format);
		static void setTimeUnitValueInTm(std::tm &time, TimeUnit unit, int value);
		static int getTimeUnitValueFromTm(const tm &time, TimeUnit unit);

		static int minValueForTimeUnit(TimeUnit part);
		static int maxValueForTimeUnit(TimeUnit part);	
		static int daysOfMonth(int year, int month);
	public:
		class GDALDataLoadingInfo {
		public:
			GDALDataLoadingInfo(const std::string &fileName, int channel, const TemporalReference &tref): fileName(fileName), channel(channel), tref(tref) {}

			std::string fileName;
            int channel;
			TemporalReference tref;
		};

        static tm snapToInterval(TimeUnit unit, int unitValue, tm startTime, tm wantedTime);

		static GDALDataLoadingInfo getDataLoadingInfo(Json::Value datasetJson, int channel, double wantedTimeUnix);
		
		static TimeUnit createTimeUnit(std::string value);
		static const std::map<std::string, TimeUnit> string_to_TimeUnit;

	std::string unixTimeToString(double unix_time, std::string format);
};

#endif