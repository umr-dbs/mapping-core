#include "gdal_timesnap.h"
#include <boost/filesystem.hpp>

constexpr int MAX_FILE_NAME_LENGTH = 255;

const std::map<std::string, TimeUnit> GDALTimesnap::string_to_TimeUnit = {
        {"Second", 	TimeUnit::Second},
        {"Minute", 	TimeUnit::Minute},
        {"Hour", 	TimeUnit::Hour},
        {"Day", 	TimeUnit::Day},
        {"Month", 	TimeUnit::Month},
        {"Year", 	TimeUnit::Year}
};

TimeUnit GDALTimesnap::createTimeUnit(std::string value) {
    return string_to_TimeUnit.at(value);
}

// snaps the wanted time to a interval timestamp from the startTime
ptime GDALTimesnap::snapToInterval(TimeUnit snapUnit, int intervalValue, ptime start, ptime wanted){

	switch (snapUnit) {
		case TimeUnit::Year: {
            int diff = wanted.date().year() - start.date().year();
            diff = (diff / intervalValue) * intervalValue;
            boost::gregorian::date snapped(start.date());
            snapped += boost::gregorian::years(diff);

            return ptime(snapped, start.time_of_day());
        }
		case TimeUnit::Month: {
            int months = (wanted.date().year() - start.date().year())*12 + wanted.date().month() - start.date().month();
            months = (months / intervalValue) * intervalValue;
            boost::gregorian::date snapped(start.date());
            snapped += boost::gregorian::months(months);

            return ptime(snapped, start.time_of_day());
        }
		case TimeUnit::Day: {
            long days = (wanted.date() - start.date()).days();
            days = (days / intervalValue) * intervalValue;
            boost::gregorian::date snapped(start.date());
            snapped += boost::gregorian::days(days);

            return ptime(snapped, start.time_of_day());
        }
		case TimeUnit::Hour: {
            long hours = (wanted.date() - start.date()).days() * 24;
            hours += wanted.time_of_day().hours() - start.time_of_day().hours();
            hours = (hours / intervalValue) * intervalValue;

            long days = hours / 24;
            hours = hours - days * 24;

            boost::gregorian::date snapped(start.date());
            snapped += boost::gregorian::days(days);

            return ptime(snapped, start.time_of_day() + boost::posix_time::hours(hours));
        }
		case TimeUnit::Minute: {
            long minutes = (wanted.date() - start.date()).days() * 24 * 60;
            minutes += wanted.time_of_day().hours() * 60 - start.time_of_day().hours() * 60;
            minutes += wanted.time_of_day().minutes() - start.time_of_day().minutes();
            minutes = (minutes / intervalValue) * intervalValue;

            long hours = minutes / 60;
            minutes = minutes - hours * 60;

            long days = hours / 24;
            hours = hours - days * 24;

            boost::gregorian::date snapped(start.date());
            snapped += boost::gregorian::days(days);

            return ptime(snapped, start.time_of_day() + boost::posix_time::hours(hours) + boost::posix_time::minutes(minutes));
        }
		case TimeUnit::Second: {
            long seconds = (wanted.date() - start.date()).days() * 24 * 60 * 60;
            seconds += wanted.time_of_day().hours() * 60 * 60 - start.time_of_day().hours() * 60 * 60;
            seconds += wanted.time_of_day().minutes() * 60 - start.time_of_day().minutes() * 60;
            seconds += wanted.time_of_day().seconds() - start.time_of_day().seconds();
            seconds = (seconds / intervalValue) * intervalValue;

            long minutes = seconds / 60;
            seconds = seconds - minutes * 60;

            long hours = minutes / 60;
            minutes = minutes - hours * 60;

            long days = hours / 24;
            hours = hours - days * 24;

            boost::gregorian::date snapped(start.date());
            snapped += boost::gregorian::days(days);

            return ptime(snapped, start.time_of_day() + boost::posix_time::hours(hours)
                                  + boost::posix_time::minutes(minutes)
                                  +boost::posix_time::seconds(seconds)
            );
        }
	}
}



// calculates the filename for queried time by snapping the wanted time to the 
// nearest smaller timestamp that exists for the dataset
// TODO: move general parameter parsing to a more appropriate function
GDALTimesnap::GDALDataLoadingInfo GDALTimesnap::getDataLoadingInfo(Json::Value datasetJson, int channel, const TemporalReference &tref)
{
    // get parameters
    Json::Value channelJson = datasetJson["channels"][channel];

	std::string time_format = channelJson.get("time_format", datasetJson.get("time_format", "")).asString();
	std::string time_start 	= channelJson.get("time_start", datasetJson.get("time_start", "")).asString();
	std::string time_end 	= channelJson.get("time_end", datasetJson.get("time_end", "")).asString();

    std::string path 	 = channelJson.get("path", datasetJson.get("path", "")).asString();
    std::string fileName = channelJson.get("file_name", datasetJson.get("file_name", "")).asString();

    channel = channelJson.get("channel", channel).asInt();

    // resolve time
    auto timeParser = TimeParser::create(TimeParser::Format::ISO);

    double time_start_mapping;
    double time_end_mapping;

    if(time_start.empty()) {
        time_start_mapping = tref.beginning_of_time();
    } else {
        time_start_mapping = timeParser->parse(time_start);
    }

    if(time_end.empty()) {
        time_end_mapping = tref.end_of_time();
    } else {
        time_end_mapping = timeParser->parse(time_end);
    }

    double wantedTimeUnix = tref.t1;

    //check if requested time is in range of dataset timestamps
    //a dataset only has start not end time. if wantedtime is past the last file of dataset, it can simply not be loaded.
    if(wantedTimeUnix < time_start_mapping || wantedTimeUnix > time_end_mapping)
        throw NoRasterForGivenTimeException("Requested time is not in range of dataset");

    if(datasetJson.isMember("time_interval") || channelJson.isMember("time_interval")) {
        Json::Value timeInterval = channelJson.get("time_interval", datasetJson.get("time_interval", Json::Value(Json::objectValue)));
        TimeUnit intervalUnit 	 = GDALTimesnap::createTimeUnit(timeInterval.get("unit", "Month").asString());
        int intervalValue 		 = timeInterval.get("value", 1).asInt();


		ptime start = from_time_t(static_cast<time_t>(time_start_mapping));
        ptime wanted = from_time_t(static_cast<time_t>(wantedTimeUnix));

		//snap the time to the given interval
        ptime snappedTimeStart = GDALTimesnap::snapToInterval(intervalUnit, intervalValue, start, wanted);

        // snap end to the next interval
		ptime snappedTimeEnd(snappedTimeStart);
		switch (intervalUnit) {
			case TimeUnit::Year:
				snappedTimeEnd += boost::gregorian::years(intervalValue);
                break;
			case TimeUnit::Month:
				snappedTimeEnd += boost::gregorian::months(intervalValue);
                break;
			case TimeUnit::Day:
				snappedTimeEnd += boost::gregorian::days(intervalValue);
                break;
			case TimeUnit::Hour:
				snappedTimeEnd += boost::posix_time::hours(intervalValue);
                break;
			case TimeUnit::Minute:
				snappedTimeEnd += boost::posix_time::minutes(intervalValue);
                break;
			case TimeUnit::Second:
				snappedTimeEnd += boost::posix_time::seconds(intervalValue);
                break;
		}

        time_start_mapping = boost::posix_time::to_time_t(snappedTimeStart);
        time_end_mapping = boost::posix_time::to_time_t(snappedTimeEnd);

        // format date to determine file
        auto snappedTimeT = static_cast<time_t>(time_start_mapping);
        tm snappedTimeTm = {};
        gmtime_r(&snappedTimeT, &snappedTimeTm);

        char date[MAX_FILE_NAME_LENGTH] = {0};
        strftime(date, sizeof(date), time_format.c_str(), &snappedTimeTm);
        std::string snappedTimeString(date);

        std::string placeholder = "%%%TIME_STRING%%%";
        size_t placeholderPos = fileName.find(placeholder);

        fileName = fileName.replace(placeholderPos, placeholder.length(), snappedTimeString);
    }


    // other GDAL parameters
    Unit unit = Unit::unknown();
    if (channelJson.isMember("unit")) {
        unit = Unit(channelJson["unit"]);
    }

    double nodata = NAN;
    if (channelJson.isMember("nodata")) {
        nodata = channelJson["nodata"].asDouble();
    }

    auto coords = channelJson.get("coords", datasetJson["coords"]);
    CrsId crsId = CrsId::from_srs_string(coords.get("crs", "").asString());


    boost::filesystem::path file_path(path);
    file_path /= fileName;
	return GDALDataLoadingInfo(file_path.string(), channel,
                               TemporalReference(TIMETYPE_UNIX, time_start_mapping, time_end_mapping),
                               crsId, nodata, unit);
}
