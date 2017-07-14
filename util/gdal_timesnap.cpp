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

tm GDALTimesnap::snapToInterval(TimeUnit snapUnit, int intervalValue, tm startTime, tm wantedTime){
	
	std::cout << "HOURS start: " << getTimeUnitValueFromTm(startTime, TimeUnit::Hour) << ", wanted: " << getTimeUnitValueFromTm(wantedTime, TimeUnit::Hour) << std::endl;;

	tm diff 	= tmDifference(wantedTime, startTime);
	tm snapped 	= startTime;

	printTime(diff);

	std::cout << "StartTime: " << GDALTimesnap::tmStructToString(&startTime, "%Y-%m-%d") << std::endl;
	std::cout << "WantedTime: " << GDALTimesnap::tmStructToString(&wantedTime, "%Y-%m-%d") << std::endl;
	std::cout << "Diff Time: " << GDALTimesnap::tmStructToString(&diff, "%Y-%m-%d") << std::endl;

	int unitDiffValue 	= getUnitDifference(diff, snapUnit);
	int unitDiffModulo 	= unitDiffValue % intervalValue;	
	unitDiffValue 		-= unitDiffModulo; 	

	
	// add the units difference on the start value
	setTimeUnitValueInTm(snapped, snapUnit, getTimeUnitValueFromTm(snapped, snapUnit) + unitDiffValue);

	//automatically handles the overflow by making time_t from tm, than tm from that time_t. No performance difference noticeable
	if(snapUnit < TimeUnit::Day){
		//day being 1 based is always one of for the other units...?!?!
		//edit: only for "bigger units" like month, year, so its snapUnit < Day, not !=.
		setTimeUnitValueInTm(snapped, TimeUnit::Day, getTimeUnitValueFromTm(snapped, TimeUnit::Day) + 1);			
	}


	if(snapUnit == TimeUnit::Hour){
		handleOverflow(snapped, snapUnit);
	} else {
		time_t snappedToTimeT = mktime(&snapped);
		gmtime_r(&snappedToTimeT, &snapped);
	}
	
	
	return snapped;
}

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

int GDALTimesnap::getUnitDifference(tm diff, TimeUnit snapUnit){
	std::cout << "GEt UNit difference" << std::endl;
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

	std::cout << "Unit diff middle: " << unitDiff << std::endl;

	//if one of the smaller time units than snapUnit is negative -> unitdiff -= 1 because one part of the difference was not a whole unit
	for(int i = snapUnitAsInt+1; i <= (int)TimeUnit::Second; i++){
		TimeUnit tu = (TimeUnit)i;		
		if(getTimeUnitValueFromTm(diff, tu) < minValueForTimeUnit(tu)){
			std::cout << "Unit: " << (int)tu << ", unitvalue: " << getTimeUnitValueFromTm(diff, tu) << ", min value: " << minValueForTimeUnit(tu) << std::endl;
			if(unitDiff > 0)
				unitDiff -= 1;
			break;
		}
	}
	std::cout << "Unit diff end: " << unitDiff << std::endl;
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
		case TimeUnit::Day:	//TODO: how to handle 31, 28 day months?
			return 30;			
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