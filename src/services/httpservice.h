#ifndef SERVICES_HTTPSERVICES_H
#define SERVICES_HTTPSERVICES_H

#include "util/parameters.h"
#include "processing/queryprocessor.h"
#include "userdb/userdb.h"

#include <memory>
#include <string>
#include <map>
#include <iostream>

#include <json/json.h>

#include <fcgio.h>

/**
 * Base class for http webservices. Provides methods for parsing and output
 */
class HTTPService {
	public:
		// class ResponseStream
		class HTTPResponseStream : public std::ostream {
			public:
				HTTPResponseStream(std::streambuf *buf);
				virtual ~HTTPResponseStream();
				/**
				 * Sends a 500 Internal Server Error with the given message
				 */
				void send500(const std::string &message);
				/**
				 * Sends a HTTP header
				 */
				void sendHeader(const std::string &key, const std::string &value);
				/**
				 * Shorthand for sending a HTTP header indicating the content-type
				 */
				void sendContentType(const std::string &contenttype);
				void sendDebugHeader();
				/**
				 * Indicates that all headers have been sent, readying the stream for
				 * sending the content via operator<<
				 */
				void finishHeaders();

				bool hasSentHeaders();

				/**
				 * Sends appropriate headers followed by the serialized JSON object
				 */
				void sendJSON(const Json::Value &obj);
				/**
				 * Sends a json object with an additional value "result": true
				 *
				 * These are used for internal protocols. The result is guaranteed to be a JSON object
				 * with a "result" attribute, which is either true or a string containing an error message.
				 */
				void sendSuccessJSON(Json::Value &obj);
				void sendSuccessJSON();
				template<typename T>
				void sendSuccessJSON(const std::string &key, const T& value) {
					Json::Value obj(Json::ValueType::objectValue);
					obj[key] = value;
					sendSuccessJSON(obj);
				}
				/**
				 * Sends a json object indicating failure.
				 */
				void sendFailureJSON(const std::string &error);

			private:
				bool headers_sent;
		};

		HTTPService(const Parameters& params, HTTPResponseStream& response, std::ostream &error);
	protected:
		HTTPService(const HTTPService &other) = delete;
		HTTPService &operator=(const HTTPService &other) = delete;

		virtual void run() = 0;
		static std::unique_ptr<HTTPService> getRegisteredService(const std::string &name,const Parameters &params, HTTPResponseStream &response, std::ostream &error);

		/**
		 * Process the given query and validate the permissions of the user
		 */
		std::unique_ptr<QueryProcessor::QueryResult> processQuery(Query &query, std::shared_ptr<UserDB::Session> session);

		/**
		* Catches MappingExceptions and sends the exceptions nested structure as Json http response.
		*/
		static void catchExceptions(HTTPResponseStream& response, const MappingException &e);
		/**
		 * Writes info of the exception into the exceptionJson object and calls
		 * method recursively for nested exceptions.
		 */
		static void readNestedException(Json::Value &exceptionJson, const MappingException &me);
		/**
		 * Clears confidential exceptions from the response json recursively, including exceptions flagged as SAME_AS_NESTED when
		 * the child is CONFIDENTIAL. If root element is CONFIDENTIAL the calling function has to handle the removing.
		 * @return if the parameter Json::Value is CONFIDENTIAL or SAME_AS_NESTED and the nested is CONFIDENTIAL.
		 */
		static bool clearExceptionJsonFromConfidential(Json::Value &exceptionJson);

		const Parameters &params;
		HTTPResponseStream &response;
		std::ostream &error;
	public:
		virtual ~HTTPService() = default;

		static void run(std::streambuf *in, std::streambuf *out, std::streambuf *err);
		static void run(std::streambuf *in, std::streambuf *out, std::streambuf *err, FCGX_Request & request);

};



class HTTPServiceRegistration {
	public:
		HTTPServiceRegistration(const char *name, std::unique_ptr<HTTPService> (*constructor)(const Parameters &params, HTTPService::HTTPResponseStream &response, std::ostream &error));
};

#define REGISTER_HTTP_SERVICE(classname, name) static std::unique_ptr<HTTPService> create##classname(const Parameters &params, HTTPService::HTTPResponseStream &response, std::ostream &error) { return std::make_unique<classname>(params, response, error); } static HTTPServiceRegistration register_##classname(name, create##classname)


#endif
