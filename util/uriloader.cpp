#include "uriloader.h"
#include "util/exceptions.h"
#include "util/make_unique.h"

#include <fstream>

std::unique_ptr<std::istream> URILoader::loadFromURI(const std::string &uri) {

	std::string scheme;
	std::string path;
	size_t schemePos = uri.find("://");
	if (schemePos == std::string::npos) {
		if (uri.find("data:") == 0) {
			scheme = "data";
			// TODO: avoid copy
			path = uri.substr(5);
		} else {
			// fallback to file, should probably be removed
			scheme = "file";
			path = uri;
		}
	} else {
		scheme = uri.substr(0, uri.find(("://")));
		path = uri.substr(uri.find("://") + 3);
	}

	if (scheme == "file") {
		// load data from file
		std::unique_ptr<std::ifstream> data = make_unique<std::ifstream>(path);

		if (!data->is_open()) {
			throw OperatorException("URILoader: could not open file");
		}
		return std::unique_ptr<std::istream>(data.release());
	} else if (scheme == "data") {
		// parse data from string

		size_t dataSep = path.find(",");
		if (dataSep == std::string::npos) {
			throw ArgumentException("URILoader: malformed data URI");
		}

		// TODO: avoid copy
		std::string data = path.substr(dataSep + 1);

		std::string mediaType;
		bool base64 = false;
		if (dataSep > 0) {
			// TODO: character encoding
			// media type + base64 extension
			size_t mediaTypeSep = path.find(";");

			if (mediaTypeSep != std::string::npos && mediaTypeSep < dataSep) {
				std::string encoding = path.substr(mediaTypeSep, dataSep);
				if (encoding == "base64") {
					base64 = true;
				} else {
					throw ArgumentException("URILoader: malformed data URI");
				}
			}

			mediaType = path.substr(0, dataSep);
		}


		if (mediaType != "text/plain" || base64) {
			throw ArgumentException("URILoader: Media type not supported");
		}

		std::unique_ptr<std::istringstream> iss =
				make_unique<std::istringstream>(data);

		return std::unique_ptr<std::istream>(iss.release());

	} else {
		// HTTP/FTP ... not yet supported
		throw ArgumentException("URILoader: scheme not supported");
	}

}

