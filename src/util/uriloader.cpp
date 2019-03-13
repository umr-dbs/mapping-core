#include "uriloader.h"
#include "util/exceptions.h"

#include <fstream>
#include <string.h>
#include <algorithm>

std::unique_ptr<std::istream> URILoader::loadFromURI(const std::string &uri) {

	std::string scheme;
	std::string path;
	if (uri.find("data:") == 0) {
		scheme = "data";
		// TODO: avoid copy
		path = uri.substr(5);
	} else {
		scheme = uri.substr(0, uri.find(("://")));
		path = uri.substr(uri.find("://") + 3);
	}

	if (scheme == "file") {
		// load data from file
		std::unique_ptr<std::ifstream> data = std::make_unique<std::ifstream>(path);

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

		std::string mediaType;
		std::string charset;
		bool base64 = false;
		if (dataSep > 0) {
			// media type + base64 extension

			// character encoding
			size_t charsetSep = path.find(";charset=");
			size_t encodingSep;
			if(charsetSep != std::string::npos) {
				size_t startPos = charsetSep + strlen(";charset=");
				std::string rest = path.substr(startPos, dataSep - startPos);

				encodingSep = rest.find(";");

				if(encodingSep != std::string::npos) {
					charset = rest.substr(0, encodingSep);
				} else {
					charset = rest;
				}
			} else {
				encodingSep = path.find(";");
			}

			if (encodingSep != std::string::npos && encodingSep < dataSep) {
				std::string encoding = path.substr(encodingSep + 1, dataSep - encodingSep - 1);
				if (encoding == "base64") {
					base64 = true;
				} else {
					throw ArgumentException("URILoader: malformed data URI");
				}
			}

			mediaType = path.substr(0, std::min(dataSep, std::min(charsetSep, encodingSep)));
		}

		// TODO: avoid copy
		std::string data = path.substr(dataSep + 1);


		if (mediaType != "text/plain" || base64) {
			throw ArgumentException("URILoader: Media type not supported");
		}

		std::unique_ptr<std::istringstream> iss =
				std::make_unique<std::istringstream>(data);

		return std::unique_ptr<std::istream>(iss.release());

	} else {
		// HTTP/FTP ... not yet supported
		throw ArgumentException("URILoader: scheme not supported");
	}

}

