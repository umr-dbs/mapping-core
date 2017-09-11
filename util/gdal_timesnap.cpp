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
		time_t snappedToTimeT = mktime(&snapped) - timezone;	// because mktime depends on the timezone the timezone field of time.h has to be substracted
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

// takes two tm structs and gives back tm struct with the differnce values. first - second.
tm GDALTimesnap::tmDifference(tm &first, tm &second){
	// igonres tm_wday and tm_yday and tm_isdst, because they are not needed
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
		if(getTimeUnitValueFromTm(diff, tu) < minValueForTimeUnit(tu)){			
			if(unitDiff > 0)
				unitDiff -= 1;
			break;
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

void GDALTimesnap::setTimeUnitValueInTm(tm &time, TimeUnit unit, int value){
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

int GDALTimesnap::getTimeUnitValueFromTm(tm &time, TimeUnit unit){	
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

void GDALTimesnap::printTime(tm &time){
	std::cout << "Y: " << time.tm_year << ", M: " << time.tm_mon << ", D: " << time.tm_mday << ", H: " << time.tm_hour << ", M: " << time.tm_min << ", S: " << time.tm_sec << std::endl;
}

// Takes the acutal month and year numbers, not how they would be in tm struct.
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

//Loads the json file of the wanted dataset from disk
Json::Value GDALTimesnap::getDatasetJson(std::string wantedDatasetName, std::string datasetPath)
{
	//opens the standard path for datasets and returns the dataset with the name datasetName as Json::Value
	struct dirent *entry;
	DIR *dir = opendir(datasetPath.c_str());

	if (dir == NULL) {
        throw OperatorException("GDAL Source: directory for dataset json files does not exist.");
    }

    //iterate files in directory
	while ((entry = readdir(dir)) != NULL) {
        std::string filename = entry->d_name;
        std::string withoutExtension = filename.substr(0, filename.length() - 5);	//extension of wanted file is .json, so 5 chars.
        
        if(withoutExtension == wantedDatasetName){
        	
        	//open file
        	std::ifstream file(datasetPath + filename);
			if (!file.is_open()) {
			    closedir(dir);
				throw OperatorException("GDAL Source Operator: unable to dataset file " + wantedDatasetName);
			}

			//read json object
			Json::Reader reader(Json::Features::strictMode());
			Json::Value json;
			if (!reader.parse(file, json)) {
			    closedir(dir);
				throw OperatorException("GDAL Source Operator: unable to read json" + reader.getFormattedErrorMessages());				
			}				

		    closedir(dir);
        	return json;
        }
    }

    //file not found
    closedir(dir);
    throw OperatorException("GDAL Source: Dataset " + wantedDatasetName + " does not exist.");
}

// calculates the filename for queried time by snapping the wanted time to the 
// nearest smaller timestamp that exists for the dataset
std::string GDALTimesnap::getDatasetFilename(Json::Value datasetJson, double wantedTimeUnix)
{
	std::string time_format = datasetJson.get("time_format", "%Y-%m-%d").asString();
	std::string time_start 	= datasetJson.get("time_start", "0").asString();
	std::string time_end 	= datasetJson.get("time_end", "0").asString();
    
    auto timeParser = TimeParser::createCustom(time_format); 

    //check if requested time is in range of dataset timestamps    
    //a dataset only has start not end time. if wantedtime is past the last file of dataset, it can simply not be loaded.
	double startUnix 	= timeParser->parse(time_start);	
	if(wantedTimeUnix < startUnix)
		throw NoRasterForGivenTimeException("Requested time is not in range of dataset");
	
	Json::Value timeInterval = datasetJson.get("time_interval", NULL);
	TimeUnit intervalUnit 	 = GDALTimesnap::createTimeUnit(timeInterval.get("unit", "Month").asString());
	int intervalValue 		 = timeInterval.get("value", 1).asInt();
	
	//cast the unix time stamps into time_t structs (having a field for year, month, etc..)
	time_t wantedTimeTimet = wantedTimeUnix;	
	tm wantedTimeTm = {};
	gmtime_r(&wantedTimeTimet, &wantedTimeTm);

	time_t startTimeTimet = startUnix;				
	tm startTimeTm = {};
	gmtime_r(&startTimeTimet, &startTimeTm);	

	// this is a workaround for time formats not containing days. because than the date is reliably the last day before the wanted date
	if(time_format.find("%d") == std::string::npos){
		int currDayValue  = GDALTimesnap::getTimeUnitValueFromTm(startTimeTm, TimeUnit::Day);		
		int yearValue  	  = GDALTimesnap::getTimeUnitValueFromTm(startTimeTm, TimeUnit::Year);
		int monthValue 	  = GDALTimesnap::getTimeUnitValueFromTm(startTimeTm, TimeUnit::Month);
			
		if(currDayValue == GDALTimesnap::daysOfMonth(yearValue + 1900, monthValue + 1))
		{			
			GDALTimesnap::setTimeUnitValueInTm(startTimeTm, TimeUnit::Day, currDayValue + 1);
			time_t overflow = mktime(&startTimeTm) - timezone;	// because mktime depends on the timezone the timezone field of time.h has to be substracted
			gmtime_r(&overflow, &startTimeTm);			
		}
	}	

	//snap the time to the given interval
	tm snappedTime = GDALTimesnap::snapToInterval(intervalUnit, intervalValue, startTimeTm, wantedTimeTm);
	
	// get string of snapped time and put the file path, name together
	std::string snappedTimeString  = GDALTimesnap::tmStructToString(&snappedTime, time_format);

	std::string path 	 = datasetJson.get("path", "").asString();
	std::string fileName = datasetJson.get("file_name", "").asString();

	std::string placeholder = "%%%TIME_STRING%%%";
	size_t placeholderPos   = fileName.find(placeholder);

	fileName = fileName.replace(placeholderPos, placeholder.length(), snappedTimeString);
	return path + "/" + fileName;
}
