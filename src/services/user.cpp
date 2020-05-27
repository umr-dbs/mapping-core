
#include "services/httpservice.h"
#include "userdb/userdb.h"
#include "util/configuration.h"

#include "rasterdb/rasterdb.h"
#include "util/log.h"

#include <json/json.h>
#include <util/gdal_source_datasets.h>
#include <util/ogr_source_datasets.h>
#include <boost/filesystem.hpp>

/**
 * This class provides user specific methods
 *
 * Operations:
 * - request = login: login with credential, returns sessiontoken
 *   - parameters:
 *     - username
 *     - password
 * - request = logout: destroys current session
 * - request = sourcelist: get list of available raster sources
 * - request = info: get user information
 * - request = uploadlist: get list of files uploaded by the user
 *
 */
class UserService : public HTTPService {
	public:
		using HTTPService::HTTPService;
		virtual ~UserService() = default;
		virtual void run();
};
REGISTER_HTTP_SERVICE(UserService, "USER");



void UserService::run() {
	try {
		std::string request = params.get("request");

		if (request == "login") {
			auto session = UserDB::createSession(params.get("username"), params.get("password"), 8*3600);
			response.sendSuccessJSON("session", session->getSessiontoken());
			return;
		}

		// anything except login is only allowed with a valid session, so check for it.
		auto session = UserDB::loadSession(params.get("sessiontoken"));
		auto user = session->getUser();

		if (request == "logout") {
			session->logout();
			response.sendSuccessJSON();
			return;
		}

		if (request == "sourcelist") {
			Json::Value v(Json::ValueType::objectValue);

			// RasterDB
			auto sourcenames = RasterDB::getSourceNames();
			for (const auto &name : sourcenames) {
				// check permissions: only return rasters the user can access
				if(user.hasPermission("data.rasterdb_source." + name)) {
					auto description = RasterDB::getSourceDescription(name);
					std::istringstream iss(description);
					Json::Reader reader(Json::Features::strictMode());
					Json::Value root;
					if (!reader.parse(iss, root))
						continue;

					v[name] = root;
				}
			}

			// GDALSource
			auto dataSets= GDALSourceDataSets::getDataSetNames();
			// loop over all datasets
			for(const auto &dataSet : dataSets) {
                if (user.hasPermission("data.gdal_source." + dataSet)) {
					// wrap the description parsing into try/catch to skip broken descriptions
					try	{
						auto description = GDALSourceDataSets::getDataSetDescription(dataSet);
						description["operator"] = "gdal_source";

						// TODO: resolve name clashes
						v[dataSet] = description;
					} catch(const std::exception& e) {
						Log::warn("UserService GDALSourceDataSets: could not load dataset: " + dataSet + ". Exception: " + e.what());
					}
                }
            }

			// OGR File Source
			std::vector<std::string> ogr_source_names = OGRSourceDatasets::getDatasetNames();
			// loop over all datasets
			for(const auto &name : ogr_source_names){
				if(user.hasPermission("data.ogr_source." + name)){
					// wrap the description parsing into try/catch to skip broken descriptions
					try {
						Json::Value description = OGRSourceDatasets::getDatasetListing(name);
						description["operator"] = "ogr_source";

						// TODO: resolve name clashes
						v[name] = description;
					} catch(const std::exception& e) {
						Log::warn("UserService OGRSourceDatasets: could not load dataset: " + name + ". Exception: " + e.what());
					}				
				}
			}

			response.sendSuccessJSON("sourcelist", v);
			return;
		}

		if(request == "info") {
			UserDB::User &user = session->getUser();

			Json::Value json(Json::ValueType::objectValue);
			json["username"] = user.getUsername();
			json["realname"] = user.getRealname();
			json["email"] = user.getEmail();
			json["externalid"] = user.getExternalid();

			response.sendSuccessJSON(json);
			return;
		}

		if(request == "uploadlist"){
			namespace bf = boost::filesystem;
			std::string user_id = user.getUserIDString();
			Json::Value uploadlist_json(Json::ValueType::arrayValue);

			auto upload_dir = Configuration::get<std::string>("uploader.directory");
            bf::path base_path(upload_dir);
			base_path /= user_id;

			if(bf::exists(base_path) && bf::is_directory(base_path)){
				//in the users folder are folders again, one folder per upload.
				for(auto it = bf::directory_iterator(base_path); it != bf::directory_iterator{}; ++it)
				{
					auto upload_path = (*it).path();
					if(!bf::is_directory(upload_path))
						continue;

					Json::Value upload_json(Json::ValueType::objectValue);
					upload_json["upload_name"] = upload_path.filename().string();
					std::time_t write_time 	= bf::last_write_time(upload_path);
					upload_json["last_write_time"] = write_time; //boost does not support creation time?
					Json::Value upload_files_json(Json::ValueType::arrayValue);

					//iterate all files in upload folder
					for(auto it_inner = bf::directory_iterator(upload_path); it_inner != bf::directory_iterator{}; ++it_inner)
					{
						Json::Value file_json(Json::ValueType::objectValue);
						auto file_path 		= (*it_inner).path();
						if(!bf::is_regular_file(file_path))
							continue;

						file_json["name"] 	= file_path.filename().string();
						file_json["size"]	= bf::file_size(file_path);

						upload_files_json.append(file_json);
					}

					upload_json["files"] = upload_files_json;
					uploadlist_json.append(upload_json);
				}
                response.sendSuccessJSON("uploadlist", uploadlist_json);
                return;
			} else{
				response.sendFailureJSON("No user uploads found.");
				return;
			}

		}

		response.sendFailureJSON("unknown request");
	}
	catch (const std::exception &e) {
		response.sendFailureJSON(e.what());
	}
}
