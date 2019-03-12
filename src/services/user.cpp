
#include "services/httpservice.h"
#include "userdb/userdb.h"
#include "util/configuration.h"

#include "rasterdb/rasterdb.h"

#include <json/json.h>
#include <util/gdal_source_datasets.h>
#include <util/ogr_source_datasets.h>
#include <boost/filesystem.hpp>
#include <util/curl.h>

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
			for(const auto &dataSet : dataSets) {
                if (user.hasPermission("data.gdal_source." + dataSet)) {
                    auto description = GDALSourceDataSets::getDataSetDescription(dataSet);
                    description["operator"] = "gdal_source";

                    // TODO: resolve name clashes
                    v[dataSet] = description;
                }
            }

			// OGR File Source
			std::vector<std::string> ogr_source_names = OGRSourceDatasets::getDatasetNames();
			for(const auto &name : ogr_source_names){
				if(user.hasPermission("data.ogr_source." + name)){
                    Json::Value description = OGRSourceDatasets::getDatasetListing(name);
                    description["operator"] = "ogr_source";

					// TODO: resolve name clashes
					v[name] = description;
				}
			}

			// Natur 4.0
			try {
				std::string jwt = user.loadArtifact(user.getUsername(), "jwt", "token")->getLatestArtifactVersion()->getValue();

				cURL curl;
				std::stringstream data;
				curl.setOpt(CURLOPT_PROXY, Configuration::get<std::string>("proxy", "").c_str());
				curl.setOpt(CURLOPT_URL, concat("http://137.248.186.133:62134/rasterdbs.json?bands&code&JWS=", jwt).c_str());
				curl.setOpt(CURLOPT_WRITEFUNCTION, cURL::defaultWriteFunction);
				curl.setOpt(CURLOPT_WRITEDATA, &data);
				curl.perform();

				Json::Reader reader(Json::Features::strictMode());
				Json::Value rasters;
				if (!reader.parse(data.str(), rasters))
					throw std::runtime_error("Could not parse rasters from Natur40 rasterdb");

				for (auto &raster : rasters["rasterdbs"]) {
					Json::Value source (Json::objectValue);

					std::string sourceName = raster["name"].asString();

					Json::Value channels(Json::arrayValue);

					std::string crs = raster["code"].asString();
					std::string epsg = crs.substr(5);

					Json::Value coords(Json::objectValue);
					coords["crs"] = crs;
					coords["epsg"] = epsg;

					source["coords"] = coords;


					for(auto &band : raster["bands"]) {
						Json::Value channel(Json::objectValue);

						channel["name"] = band["title"];
						channel["datatype"] = band["datatype"];

						channel["file_name"] = concat("http://137.248.186.133:62134/rasterdb/",
								sourceName,
								"/raster.tiff?band=",
								band["index"].asInt(),
								"&ext=%%%MINX%%%%20%%%MINY%%%%20%%%MAXX%%%%20%%%MAXY%%%",
								"&width=%%%WIDTH%%%&height=%%%HEIGHT%%%"
								"&JWS=%%%JWT%%%");

                        user.addPermission(concat("data.gdal_source.", channel["file_name"].asString()));

						channel["channel"] = 1;

						channels.append(channel);
					}

					source["channels"] = channels;

                    source["operator"] = "gdal_ext_source";

					v[sourceName] = source;
				}


			} catch (UserDB::artifact_error&) {}

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
