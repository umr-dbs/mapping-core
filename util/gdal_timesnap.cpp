#include "gdal_timesnap.h"
#include <iostream>

const std::map<std::string, TimeUnit> GDALTimesnap::string_to_TimeUnit = {
			{"Second", 	TimeUnit::Second},
			{"Minute", 	TimeUnit::Minute},
			{"Hour", 	TimeUnit::Hour},
			{"Day", 	TimeUnit::Day},
			{"Month", 	TimeUnit::Month},
			{"Year", 	TimeUnit::Year}
		};

// snaps the wanted time to a interval timestamp from the startTime
tm GDALTimesnap::snapToInterval(TimeUnit snapUnit, int intervalValue, tm startTime, tm wantedTime){
	
	tm diff 	= tmDifference(wantedTime, startTime);	
	tm snapped 	= startTime;

	//get difference of the interval unit
	int unitDiffValue 	= getUnitDifference(diff, snapUnit);
	int unitDiffModulo 	= unitDiffValue % intervalValue;	
	unitDiffValue 		-= unitDiffModulo; 	
	
	// add the units difference on the start value
	setTimeUnitValueInTm(snapped, snapUnit, getTimeUnitValueFromTm(snapped, snapUnit) + unitDiffValue);	

	// for the smaller interval units like Hour, Minute, Second the handwritten handleOverflow function has to be used
	// for the rest it can be snapped by casting it to time_t and back to tm
	if(snapUnit == TimeUnit::Hour || snapUnit == TimeUnit::Minute || snapUnit == TimeUnit::Second){
		handleOverflow(snapped, snapUnit);
	} else {		
		time_t snappedToTimeT = mktime(&snapped) - timezone;	// because mktime depends on the timezone the timezone field of time.h has to be subtracted
		gmtime_r(&snappedToTimeT, &snapped);		
	}
		
	return snapped;
}

//cuts the time down to an interval value
void GDALTimesnap::handleOverflow(tm &snapped, TimeUnit snapUnit){
	const int snapUnitAsInt = (int)snapUnit;
	
	for(int i = snapUnitAsInt; i > 0; i--){
		TimeUnit tu = (TimeUnit)i;
		int value = getTimeUnitValueFromTm(snapped, tu);
		int maxValue = maxValueForTimeUnit(tu);
		if(tu == TimeUnit::Day)
			maxValue += 1;	//because day is 1 based, and next value > maxValue-1 is checked

		if(value > maxValue - 1){
			TimeUnit tuBefore = (TimeUnit)(i-1);
			int mod = value % maxValue;
			int div = value / maxValue;			
			setTimeUnitValueInTm(snapped, tu, mod);			
			setTimeUnitValueInTm(snapped, tuBefore, getTimeUnitValueFromTm(snapped, tuBefore) + div);						
		}
	}	
}

// takes two tm structs and gives back tm struct with the difference values. first - second.
tm GDALTimesnap::tmDifference(tm &first, tm &second){
	// ignores tm_wday and tm_yday and tm_isdst, because they are not needed
	tm diff = {};
	diff.tm_year 	= first.tm_year - second.tm_year;
	diff.tm_mon 	= first.tm_mon - second.tm_mon;	
	diff.tm_mday 	= first.tm_mday - second.tm_mday;
	diff.tm_hour 	= first.tm_hour - second.tm_hour;
	diff.tm_min 	= first.tm_min - second.tm_min;
	diff.tm_sec 	= first.tm_sec - second.tm_sec;

	return diff;
}

//calculates the difference if snapUnit from the tm struct diff by adding values for all bigger units together
int GDALTimesnap::getUnitDifference(tm diff, TimeUnit snapUnit){
	const int snapUnitAsInt = (int)snapUnit;	
	int unitDiff = getTimeUnitValueFromTm(diff, snapUnit);

	if(snapUnitAsInt > 0){
		// add all the bigger time units from diff together as the one unit above the snapUnit. eg 1 year -> 12 month		
		for(int i = 0; i < snapUnitAsInt - 1; i++){
			TimeUnit tu = (TimeUnit)i;
			int val = getTimeUnitValueFromTm(diff, tu);
			if(val > 0){
				TimeUnit nextTu = (TimeUnit)(i+1);
				int newValue = getTimeUnitValueFromTm(diff, nextTu) + val * maxValueForTimeUnit(nextTu);
				setTimeUnitValueInTm(diff, nextTu, newValue);
			}
		}		
		TimeUnit unitBefore = (TimeUnit)(snapUnitAsInt - 1);
		int valueBefore = getTimeUnitValueFromTm(diff, unitBefore);		
		unitDiff += valueBefore * maxValueForTimeUnit(snapUnit);
	}

	//if one of the smaller time units than snapUnit is negative -> unitdiff -= 1 because one part of the difference was not a whole unit
	for(int i = snapUnitAsInt+1; i <= (int)TimeUnit::Second; i++){
		TimeUnit tu = (TimeUnit)i;		
		if(getTimeUnitValueFromTm(diff, tu) < 0){
			if(unitDiff > 0) {
                unitDiff -= 1;
            }
		}
	}	
	return unitDiff;
}

std::string GDALTimesnap::tmStructToString(const tm *tm, std::string format){	
	char date[20];	//max length of a time string is 19 + zero termination
	strftime(date, sizeof(date), format.c_str(), tm);
	return std::string(date);
}

std::string GDALTimesnap::unixTimeToString(double unix_time, std::string format){
	time_t tt = unix_time;
	tm time;
	gmtime_r(&tt, &time);
	char date[20];	//max length of a time string is 19 + zero termination
	strftime(date, sizeof(date), format.c_str(), &time);
	return std::string(date);
}

TimeUnit GDALTimesnap::createTimeUnit(std::string value) {
	return string_to_TimeUnit.at(value);
}

void GDALTimesnap::setTimeUnitValueInTm(std::tm &time, TimeUnit unit, int value) {
	switch(unit){
		case TimeUnit::Year:
			time.tm_year = value;
			return;
		case TimeUnit::Month:
			time.tm_mon = value;
			return;
		case TimeUnit::Day:
			time.tm_mday = value;
			return;
		case TimeUnit::Hour:
			time.tm_hour = value;
			return;
		case TimeUnit::Minute:
			time.tm_min = value;
			return;
		case TimeUnit::Second:
			time.tm_sec = value;
			return;
	}
}

int GDALTimesnap::getTimeUnitValueFromTm(const tm &time, TimeUnit unit){
	switch(unit){
		case TimeUnit::Year:
			return time.tm_year;
		case TimeUnit::Month:
			return time.tm_mon;
		case TimeUnit::Day:
			return time.tm_mday;
		case TimeUnit::Hour:
			return time.tm_hour;
		case TimeUnit::Minute:
			return time.tm_min;
		case TimeUnit::Second:
			return time.tm_sec;
	}
}

int GDALTimesnap::minValueForTimeUnit(TimeUnit part) {
	// based on tm struct: see http://www.cplusplus.com/reference/ctime/tm/ for value ranges of tm struct
	if(part == TimeUnit::Day)
		return 1;
	else 
		return 0;
}

int GDALTimesnap::maxValueForTimeUnit(TimeUnit part) {
	switch(part){
		case TimeUnit::Year:
			return 0;
		case TimeUnit::Month:
			return 12;			
		case TimeUnit::Day:
			return 31;			
		case TimeUnit::Hour:
			return 24;
		case TimeUnit::Minute:
			return 60;
		case TimeUnit::Second:
			return 60;
	}
}

// Takes the actual month and year numbers, not how they would be in tm struct.
int GDALTimesnap::daysOfMonth(int year, int month){
	if(month == 4 || month == 6 || month == 9 || month == 11)
		return 30;
	else if(month == 2){		
		if((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
			return 29;
		else 
			return 28;
	}
	else
		return 31;
}

// calculates the filename for queried time by snapping the wanted time to the 
// nearest smaller timestamp that exists for the dataset
GDALTimesnap::GDALDataLoadingInfo GDALTimesnap::getDataLoadingInfo(Json::Value datasetJson, int channel, double wantedTimeUnix)
{
    Json::Value channelJson = datasetJson["channels"][channel];


	std::string time_format = channelJson.get("time_format", datasetJson.get("time_format", "%Y-%m-%d")).asString();
	std::string time_start 	= channelJson.get("time_start", datasetJson.get("time_start", "0")).asString();
	std::string time_end 	= channelJson.get("time_end", datasetJson.get("time_end", "0")).asString();
    
    auto timeParser = TimeParser::create(TimeParser::Format::ISO);

    //check if requested time is in range of dataset timestamps    
    //a dataset only has start not end time. if wantedtime is past the last file of dataset, it can simply not be loaded.
	double startUnix 	= timeParser->parse(time_start);	
//	if(wantedTimeUnix < startUnix)
//		throw NoRasterForGivenTimeException("Requested time is not in range of dataset");
	
	Json::Value timeInterval = datasetJson.get("time_interval", Json::Value(Json::objectValue));
	TimeUnit intervalUnit 	 = GDALTimesnap::createTimeUnit(timeInterval.get("unit", "Month").asString());
	int intervalValue 		 = timeInterval.get("value", 1).asInt();
	
	//cast the unix time stamps into time_t structs (having a field for year, month, etc..)
	time_t wantedTimeTimet = wantedTimeUnix;	
	tm wantedTimeTm = {};
	gmtime_r(&wantedTimeTimet, &wantedTimeTm);

	time_t startTimeTimet = startUnix;				
	tm startTimeTm = {};
	gmtime_r(&startTimeTimet, &startTimeTm);

	//snap the time to the given interval
	tm snappedTimeStart = GDALTimesnap::snapToInterval(intervalUnit, intervalValue, startTimeTm, wantedTimeTm);

	tm snappedTimeEnd = snappedTimeStart;
    setTimeUnitValueInTm(snappedTimeEnd, intervalUnit, getTimeUnitValueFromTm(snappedTimeEnd, intervalUnit) + intervalValue);

	time_t snappedTimeStartUnix = timegm(&snappedTimeStart);
	time_t snappedTimeEndUnix = timegm(&snappedTimeEnd);
	
	// get string of snapped time and put the file path, name together
	std::string snappedTimeString  = GDALTimesnap::tmStructToString(&snappedTimeStart, time_format);

	std::string path 	 = channelJson.get("path", datasetJson.get("path", "")).asString();
	std::string fileName = channelJson.get("file_name", datasetJson.get("file_name", "")).asString();

    channel = channelJson.get("channel", channel).asInt();

	std::string placeholder = "%%%TIME_STRING%%%";
	size_t placeholderPos   = fileName.find(placeholder);

	fileName = fileName.replace(placeholderPos, placeholder.length(), snappedTimeString);

	return GDALDataLoadingInfo(path + "/" + fileName, channel, TemporalReference(TIMETYPE_UNIX, snappedTimeStartUnix, snappedTimeEndUnix));
}
