#include "gdal_timesnap.h"

#include <iostream>

tm snapToInterval(TimeUnit snapUnit, int intervalValue, tm startTime, tm wantedTime){
	
	tm diff 	= tmDifference(wantedTime, startTime);
	tm snapped 	= startTime;

	int unitDiffValue 	= getUnitDifference(diff, snapUnit);
	int unitDiffModulo 	= unitDiffValue % intervalValue;	
	unitDiffValue 		-= unitDiffModulo; 
	
	// add the units difference on the start value
	setTimeUnitValueInTm(snapped, snapUnit, getTimeUnitValueFromTm(snapped, snapUnit) + unitDiffValue);

	//automatically handles the overflow by making time_t from tm, than tm from that time_t. No performance difference noticeable
	if(snapUnit != TimeUnit::Day){
		//day being 1 based is always one of for the other units...?!?!
		setTimeUnitValueInTm(snapped, TimeUnit::Day, getTimeUnitValueFromTm(snapped, TimeUnit::Day) + 1);			
	}
	time_t snappedToTimeT = mktime(&snapped);
	snapped = *gmtime(&snappedToTimeT);		
	
	// handle the created overflow, old way
	//handleOverflow(snapped, snapUnit);		
	
	return snapped;
}

void handleOverflow(tm &snapped, TimeUnit snapUnit){
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

tm tmDifference(tm &first, tm &second){
	// igonres tm_wday and tm_yday and tm_isdst, because they are not needed
	tm diff;
	diff.tm_year 	= first.tm_year - second.tm_year;
	diff.tm_mon 	= first.tm_mon - second.tm_mon;
	diff.tm_mday 	= first.tm_mday - second.tm_mday;
	diff.tm_hour 	= first.tm_hour - second.tm_hour;
	diff.tm_min 	= first.tm_min - second.tm_min;
	diff.tm_sec 	= first.tm_sec - second.tm_sec;

	return diff;
}

int getUnitDifference(tm diff, TimeUnit snapUnit){
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
			unitDiff -= 1;
			break;
		}
	}

	return unitDiff;
}

std::string tmStructToString(tm *tm, std::string format){	
	char date[20];	//max length of a time string is 19 + zero termination
	strftime(date, sizeof(date), format.c_str(), tm);
	return std::string(date);
}

std::string unixTimeToString(double unix_time, std::string format){
	time_t tt = unix_time;
	struct tm *tm = gmtime(&tt);
	char date[20];	//max length of a time string is 19 + zero termination
	strftime(date, sizeof(date), format.c_str(), tm);
	return std::string(date);
}

TimeUnit createTimeUnit(std::string value) {
	return string_to_TimeUnit.at(value);
}

void setTimeUnitValueInTm(tm &time, TimeUnit unit, int value){
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

int getTimeUnitValueFromTm(tm &time, TimeUnit unit){	
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

int minValueForTimeUnit(TimeUnit part) {
	// based on tm struct: see http://www.cplusplus.com/reference/ctime/tm/ for value ranges of tm struct
	if(part == TimeUnit::Day)
		return 1;
	else 
		return 0;
}

int maxValueForTimeUnit(TimeUnit part) {
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