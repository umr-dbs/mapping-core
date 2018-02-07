#ifndef UTIL_GDAL_TIMESNAP_H_
#define UTIL_GDAL_TIMESNAP_H_

#include <map>
#include <ctime>
#include <json/json.h>
#include <dirent.h>
#include <sstream>
#include <fstream>
#include <gdal.h>
#include "util/exceptions.h"
#include "util/timeparser.h"
#include "datatypes/spatiotemporal.h"
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace boost::posix_time;

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
	public:
		class GDALDataLoadingInfo {
		public:
			GDALDataLoadingInfo(std::string fileName, int channel, const TemporalReference &tref, const CrsId &crsId,
                                double nodata, const Unit &unit):
					fileName(std::move(fileName)), channel(channel), tref(tref), crsId(crsId), nodata(nodata), unit(unit) {}

			std::string fileName;
            int channel;
			TemporalReference tref;

			CrsId crsId;
            double nodata;
            Unit unit;
		};

        static ptime snapToInterval(TimeUnit unit, int unitValue, ptime startTime, ptime wantedTime);

		static GDALDataLoadingInfo getDataLoadingInfo(Json::Value datasetJson, int channel, const TemporalReference &tref);
		
		static TimeUnit createTimeUnit(std::string value);
		static const std::map<std::string, TimeUnit> string_to_TimeUnit;
};

#endif