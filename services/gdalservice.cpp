#include "services/ogcservice.h"
#include "operators/operator.h"
#include "datatypes/raster.h"
#include "util/timeparser.h"

#include <iostream>
#include <fstream>
#include <json/json.h>

#include <dirent.h>

/**

 */
class GDALService : public OGCService {
	public:
		using OGCService::OGCService;
		virtual ~GDALService() = default;
		virtual void run();

	private:
		Json::Value writeFileToJson(std::string path, std::string filename, std::string description, double unixTime, int channel);
};

REGISTER_HTTP_SERVICE(GDALService, "GDAL");


void GDALService::run() {
	//TODO: Enable user login restriction
	//auto session = UserDB::loadSession(params.get("sessiontoken"));
	//auto user = session->getUser();
	

	const std::string std_file_path = "test/systemtests/data/ndvi";

    const std::string suffix(".json");
    const size_t suffix_length = suffix.length();
	const std::string path = "test/systemtests/data/gdal_files";

    struct dirent *entry;
	DIR *dir = opendir(path.c_str());

	if (dir == NULL) {
        return; //throw exception.
    }

    auto timeParser = TimeParser::createCustom("%Y-%m-%d"); //TimeParser::Format::ISO);

    //Json Array to write the file entries to
	Json::Value jsonFiles(Json::ValueType::arrayValue);

    while ((entry = readdir(dir)) != NULL) {

        std::string filename = entry->d_name;        
        size_t found = filename.find(suffix, filename.length() - suffix_length);	//check if file ends with the suffix

        if(found == suffix_length){ 	// could also check if found != std::string::npos, would mean the same here.

    		//open file then read json object from it
        	std::ifstream file(path + "/" + filename);
			if (!file.is_open()) {
				printf("unable to open query file %s\n", filename.c_str());
				exit(5);
			}

			Json::Reader reader(Json::Features::strictMode());
			Json::Value root;
			if (!reader.parse(file, root)) {
				printf("unable to read json\n%s\n", reader.getFormattedErrorMessages().c_str());
				exit(5);
			}				

			//read path, if no path given in file, that the standard file path is used
			std::string fpath = root.get("path", std_file_path).asString();
			std::string fname = root.get("filename", "").asString();
			std::string fdescription = root.get("description", "").asString();
			int channel = root.get("channel", 1).asInt();
			Json::Value timestamps = root.get("timestamps", NULL);

			if(timestamps != NULL && timestamps.isArray())
			{
				//for every timestamp in the array create write a single file
				for(unsigned int i = 0; i < timestamps.size(); i++)
				{
					std::string timestamp = timestamps[i].get("time", "").asString();
					
					double unixTime = timeParser->parse(timestamp);
					
					std::string concatName = fname;

					//find position where to insert timestamp string to filename
					size_t position = fname.length();
					for(int k = position - 1; k >= 0; k--)
					{
						if(fname[k] == '.')
						{
							position = k;
							break;
						}
					}

					if(position == fname.length()){
						throw OperatorException("GDAL Service: filename has no fileending");
					} else {
						concatName.insert(position, timestamp);
					}
				
					Json::Value entry = writeFileToJson(fpath, concatName, fdescription, unixTime, channel);
					jsonFiles.append(entry);
				}

			} 
			else 
			{
				double unixTime = 0;
				if(timestamps != NULL)
					unixTime = timeParser->parse(timestamps.asString());
				jsonFiles.append(writeFileToJson(fpath, fname, fdescription, unixTime, channel));
			} 
            
        }
    }   

    closedir(dir);

    //wrap files array in a json value
	Json::Value jsonOuterValue(Json::ValueType::objectValue);
	jsonOuterValue["files"] = jsonFiles;
	Json::FastWriter writer;
	std::cout << jsonOuterValue << std::endl;

    response.sendContentType("application/json; charset=utf-8");
	response.finishHeaders();
	response << writer.write(jsonOuterValue);		
}

Json::Value GDALService::writeFileToJson(std::string path, std::string filename, std::string description, double unixTime, int channel)
{
	Json::Value item(Json::ValueType::objectValue);
	item["file"] = path + "/" + filename;
	item["description"] = description;
	item["channel"] = channel;

	Json::Value tref(Json::ValueType::objectValue);
	tref["type"] = "Unix";
	tref["start"] = unixTime;
	
	item["temporal_reference"] = tref;

	return item;
}