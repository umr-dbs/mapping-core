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
        throw MustNotHappenException("GDAL_Service: Directory for gdal dataset files could not be found.");
    }
    
    //Json Array to write the file entries to
	Json::Value jsonFiles(Json::ValueType::arrayValue);

    while ((entry = readdir(dir)) != NULL) {
        std::string filename = entry->d_name;                
        size_t found = filename.find(suffix, filename.length() - suffix_length);	//check if file ends with the suffix
		
        if(found != std::string::npos) {

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

			std::string timeFormat 	= root.get("time_format", "%Y-%m-%d").asString();
			auto timeParser 		= TimeParser::createCustom("%Y-%m-%d");
	
			std::string datasetName = root.get("dataset_name", "").asString();
			std::string description = root.get("description", "").asString();
			std::string timeStart 	= root.get("time_start", "0").asString();
			
			Json::Value item(Json::ValueType::objectValue);
			item["name"] 		= datasetName;
			item["description"] = description;			
			item["time_start"] 	= timeParser->parse(timeStart);
			jsonFiles.append(item);		
        }
    }   

    closedir(dir);

    //wrap files array in a json value
	Json::Value jsonOuterValue(Json::ValueType::objectValue);
	jsonOuterValue["datasets"] = jsonFiles;
	Json::FastWriter writer;	

    response.sendContentType("application/json; charset=utf-8");
	response.finishHeaders();
	response << writer.write(jsonOuterValue);		
}