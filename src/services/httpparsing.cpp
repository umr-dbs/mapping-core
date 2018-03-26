#include "services/httpparsing.h"
#include "util/exceptions.h"

#include <string>
#include <cstring>
#include <vector>
#include <algorithm>
#include <fstream>
#include "util/make_unique.h"
#include "util/base64.h"
#include <Poco/URI.h>
#include <Poco/Net/MultipartReader.h>
#include <Poco/Net/MessageHeader.h>


/**
 * std::string wrapper for getenv(). Throws exceptions if environment variable is not set.
 */
static std::string getenv_str(const std::string& varname, bool to_lower = false) {
	const char* val = getenv(varname.c_str());

	if (!val)
		throw ArgumentException(concat("Invalid HTTP request, missing environment variable ", varname));

	std::string result = val;
	if (to_lower)
		std::transform(result.begin(), result.end(), result.begin(), ::tolower);
	return result;
}


static std::string getFCGIParam(FCGX_Request &request, std::string key) {
	const char * uri = FCGX_GetParam(key.c_str(), request.envp);
	std::string string = std::string(uri, std::strlen(uri));

	return string;
}

/**
 * Gets the data from a POST request
 */
static std::string getPostData(std::istream& in, int content_length) {
	if (content_length < 0)
		throw ArgumentException("CONTENT_LENGTH is negative");

	char *buf = new char[content_length];
	in.read(buf, content_length);
	std::string query(buf, buf + content_length);
	delete[] buf;
	return query;
}

/**
 * Parses a query string into a given Params structure.
 */
void parseQuery(const std::string& query, Parameters &params) {
	if (query.length() == 0)
		return;

	Poco::URI uri;
    //setRawQuery instead of setQuery because setQuery will encode the given string, but here it is already encoded.
	uri.setRawQuery(query);
	auto queryParameters = uri.getQueryParameters();
	for(auto &parameter : queryParameters){
        std::transform(parameter.first.begin(), parameter.first.end(), parameter.first.begin(), ::tolower);
		params.insert(std::make_pair(parameter.first, parameter.second));
	}
}

/**
 * Parses a url encoded POST request
 */
static void parsePostUrlEncoded(Parameters &params, std::istream &in, int content_length) {
	std::string query = getPostData(in, content_length);
	parseQuery(query, params);
}

/**
 * Get Reader for multipart data.
 * Reading values from multipart data has to be done manually right now.
 */
std::unique_ptr<Poco::Net::MultipartReader> getMultipartPostDataReader(Parameters &params, std::istream &in) {

    return make_unique<Poco::Net::MultipartReader>(in);
}

/**
 * Parses POST data from a HTTP request
 */
void parsePostData(Parameters &params, std::istream &in) {

	// Methods are always case-sensitive (RFC 2616 ch. 5.1.1)
	std::string request_method = getenv_str("REQUEST_METHOD", false);

	if (request_method != "POST")
		return;

	// HTTP header fields are always case-insensitive (RFC 2616 ch. 4.2)
	std::string content_type = getenv_str("CONTENT_TYPE", true);

	std::string content_length_string = getenv_str("CONTENT_LENGTH", false);
	int content_length = std::stoi(content_length_string);

	if (content_type == "application/x-www-form-urlencoded") {
		parsePostUrlEncoded(params, in, content_length);
	} else if (content_type.find("multipart/form-data") != std::string::npos || content_type.find("multipart/mixed") != std::string::npos) {
        throw ArgumentException("For multipart POST request call getMultipartPostDataReader.");
	} else
		throw ArgumentException("Unknown content type in POST request.");
}

/**
 * Parses GET data from a HTTP request
 */
void parseGetData(Parameters &params) {
	std::string query_string = getenv_str("QUERY_STRING");
	parseQuery(query_string, params);
}

/**
 * Parses POST data from a HTTP request FCGI
 */
void parsePostData(Parameters &params, std::istream &in, FCGX_Request &request) {

	// Methods are always case-sensitive (RFC 2616 ch. 5.1.1)
	std::string request_method = getFCGIParam(request, "REQUEST_METHOD");

	if (request_method != "POST")
		return;

	// HTTP header fields are always case-insensitive (RFC 2616 ch. 4.2)
	std::string content_type = getFCGIParam(request, "CONTENT_TYPE");

	std::transform(content_type.begin(), content_type.end(), content_type.begin(), ::tolower);

	std::string content_length_string = getFCGIParam(request, "CONTENT_LENGTH");
	int content_length = std::stoi(content_length_string);

	if (content_type == "application/x-www-form-urlencoded") {
		parsePostUrlEncoded(params, in, content_length);
	} else if (content_type.find("multipart/form-data") != std::string::npos || content_type.find("multipart/mixed") != std::string::npos) {
        throw ArgumentException("For multipart POST request call getMultipartPostDataReader.");
	} else
		throw ArgumentException("Unknown content type in POST request.");
}

/**
 * Parses GET data from a HTTP request FCGI
 */
void parseGetData(Parameters &params, FCGX_Request &request) {
	std::string query_string = getFCGIParam(request, "QUERY_STRING");
	parseQuery(query_string, params);
}
