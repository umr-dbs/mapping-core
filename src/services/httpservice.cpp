
#include "services/httpservice.h"
#include "services/httpparsing.h"
#include "util/exceptions.h"
#include "util/log.h"

#include <map>
#include <unordered_map>
#include <memory>
#include <algorithm>



// The magic of type registration, see REGISTER_SERVICE in httpservice.h
typedef std::unique_ptr<HTTPService> (*ServiceConstructor)(const Parameters& params, HTTPService::HTTPResponseStream& response, std::ostream &error);

static std::unordered_map< std::string, ServiceConstructor > *getRegisteredConstructorsMap() {
	static std::unordered_map< std::string, ServiceConstructor > registered_constructors;
	return &registered_constructors;
}

HTTPServiceRegistration::HTTPServiceRegistration(const char *name, ServiceConstructor constructor) {
	auto map = getRegisteredConstructorsMap();
	(*map)[std::string(name)] = constructor;
}


std::unique_ptr<HTTPService> HTTPService::getRegisteredService(const std::string &name, const Parameters& params, HTTPResponseStream& response, std::ostream &error) {
	auto map = getRegisteredConstructorsMap();
	auto it = map->find(name);
	if (it == map->end())
		throw ArgumentException(concat("No service named ", name, " is registered"), MappingExceptionType::PERMANENT);

	auto constructor = it->second;

	auto ptr = constructor(params, response, error);
	return ptr;
}

HTTPService::HTTPService(const Parameters& params, HTTPResponseStream& response, std::ostream &error)
	: params(params), response(response), error(error) {
}

void HTTPService::run(std::streambuf *in, std::streambuf *out, std::streambuf *err) {
	std::istream input(in);
	std::ostream error(err);
	HTTPResponseStream response(out);

	Log::logToStream(Log::LogLevel::WARN, &error);
	Log::logToMemory(Log::LogLevel::INFO);
	try {
		// Parse all entries
		Parameters params;
		parseGetData(params);
		parsePostData(params, input);

		auto servicename = params.get("service");
		auto service = HTTPService::getRegisteredService(servicename, params, response, error);

		Log::debug("Running new service: " + servicename);
		
		service->run();
	}
    catch(const MappingException &e){
        catchExceptions(response, e);
    }
	catch (const std::exception &e) {
		error << "Request failed with an exception: " << e.what() << "\n";
        Log::debug(e.what());
		if (Configuration::get<bool>("global.debug", false)) {
            response.send500(concat("invalid request: ", e.what()));
        } else {
            response.send500("invalid request");
        }

	}
	Log::streamAndMemoryOff();
}

/**
 * FastCGI
 */
void HTTPService::run(std::streambuf *in, std::streambuf *out, std::streambuf *err, FCGX_Request &request) {
	std::istream input(in);
	std::ostream error(err);
	HTTPResponseStream response(out);

	Log::logToStream(Log::LogLevel::WARN, &error);
	Log::logToMemory(Log::LogLevel::INFO);
	try {
		// Parse all entries
		Parameters params;
		parseGetData(params, request);
		parsePostData(params, input, request);

		auto servicename = params.get("service");
		auto service = HTTPService::getRegisteredService(servicename, params, response, error);

		Log::debug("Running new service: " + servicename);

		service->run();
	}
    catch(const MappingException &e){
        catchExceptions(response, e);
    }
	catch (const std::exception &e) {
		error << "Request failed with an exception: " << e.what() << "\n";
		Log::debug(e.what());
		if (Configuration::get<bool>("global.debug", false)) {
            response.send500(concat("invalid request: ", e.what()));
        } else {
            response.send500("invalid request");
        }
	}
	Log::streamAndMemoryOff();
}

void HTTPService::catchExceptions(HTTPResponseStream& response, const MappingException &me){
    auto exception_type = me.getExceptionType();
	const bool global_debug = Configuration::get<bool>("global.debug", false);

	if(global_debug || exception_type != MappingExceptionType::CONFIDENTIAL){
        Json::Value exceptionJson;
        readNestedException(exceptionJson, me);

        if(!global_debug){
            bool all_confidential = clearExceptionJsonFromConfidential(exceptionJson);
            if(all_confidential){
                response.send500("invalid request");
                return;
            }
        }
        response.sendHeader("Status", "500 Internal Server Error");
        response.sendJSON(exceptionJson);
        Log::debug(exceptionJson.asString());
    } else {
        response.send500("invalid request");
    }
}

void HTTPService::readNestedException(Json::Value &exceptionJson, const MappingException &me){
    auto type = me.getExceptionType();

    try {
        exceptionJson["message"] = me.what();
        switch(type){
            case MappingExceptionType::CONFIDENTIAL:
                exceptionJson["type"] = "CONFIDENTIAL";
                break;
            case MappingExceptionType::PERMANENT:
                exceptionJson["type"] = "PERMANENT";
                break;
            case MappingExceptionType::TRANSIENT:
                exceptionJson["type"] = "TRANSIENT";
                break;
            case MappingExceptionType::SAME_AS_NESTED:
                exceptionJson["type"] = "SAME_AS_NESTED";
                break;
        }

        std::rethrow_if_nested(me);

    } catch(MappingException &nested) {
        readNestedException(exceptionJson["nested_exception"], nested);
    } catch(std::runtime_error &e){
        Json::Value error_obj;
        error_obj["message"] = "non_mapping_exception";
        error_obj["type"] = "non_mapping_exception";
        exceptionJson["nested_exception"] = error_obj;
    }
}

bool HTTPService::clearExceptionJsonFromConfidential(Json::Value &exceptionJson){

    const std::string type = exceptionJson["type"].asString();

    if(type == "CONFIDENTIAL")
        return true;

    const bool hasNested = exceptionJson.isMember("nested_exception");

    if(type == "SAME_AS_NESTED"){
        if(hasNested)
            return clearExceptionJsonFromConfidential(exceptionJson["nested_exception"]);
        else
            return true; //SAME_AS_NESTED but no nested exception exists, so remove it just in case.
    } else {
        //TRANSIENT or PERMANENT as type. If a nested confidential exception exists, remove it.
        if(hasNested){
            const bool nestedIsConfidential = clearExceptionJsonFromConfidential(exceptionJson["nested_exception"]);
            if(nestedIsConfidential){
                exceptionJson.removeMember("nested_exception");
            }
        }
        return false;
    }

}

/*
 * Service::ResponseStream
 */
HTTPService::HTTPResponseStream::HTTPResponseStream(std::streambuf *buf) : std::ostream(buf), headers_sent(false) {
}

HTTPService::HTTPResponseStream::~HTTPResponseStream() {
}

void HTTPService::HTTPResponseStream::send500(const std::string &message) {
	sendHeader("Status", "500 Internal Server Error");
	sendContentType("text/plain");
	finishHeaders();
	*this << message;
}

void HTTPService::HTTPResponseStream::sendHeader(const std::string &key, const std::string &value) {
	*this << key << ": " << value << "\r\n";
}
void HTTPService::HTTPResponseStream::sendContentType(const std::string &contenttype) {
	sendHeader("Content-type", contenttype);
}

void HTTPService::HTTPResponseStream::sendDebugHeader() {
	const auto &msgs = Log::getMemoryMessages();
	*this << "Profiling-header: ";
	for (const auto &str : msgs) {
		*this << str << ", ";
	}
	*this << "\r\n";
}

void HTTPService::HTTPResponseStream::finishHeaders() {
	*this << "\r\n";
	headers_sent = true;
}
bool HTTPService::HTTPResponseStream::hasSentHeaders() {
	return headers_sent;
}


void HTTPService::HTTPResponseStream::sendJSON(const Json::Value &obj) {
	sendContentType("application/json; charset=utf-8");
	sendDebugHeader();
	finishHeaders();
	*this << obj;
}

void HTTPService::HTTPResponseStream::sendSuccessJSON(Json::Value &obj) {
	obj["result"] = true;
	sendJSON(obj);
}

void HTTPService::HTTPResponseStream::sendSuccessJSON() {
	Json::Value obj(Json::ValueType::objectValue);
	sendSuccessJSON(obj);
}

void HTTPService::HTTPResponseStream::sendFailureJSON(const std::string &error) {
	Json::Value obj(Json::ValueType::objectValue);
	obj["result"] = error;
	sendJSON(obj);
}

std::unique_ptr<QueryProcessor::QueryResult> HTTPService::processQuery(Query &query, UserDB::User &user) {
	auto queryResult = QueryProcessor::getDefaultProcessor().process(query, true);

	if(queryResult->isError()) {
	    //throw, directly catch and throw with nested to get a nested exception structure:
		try {
            throw queryResult->getErrorException().value();
        } catch (...){
		    std::throw_with_nested(OperatorException("HTTPService: query failed with error.", MappingExceptionType::SAME_AS_NESTED));
		}
	}

	ProvenanceCollection &provenance = queryResult->getProvenance();

	for(std::string identifier : provenance.getLocalIdentifiers()) {
		if(identifier != "" && !user.hasPermission(identifier)) {
			throw PermissionDeniedException("HTTPService: Permission denied for query result", MappingExceptionType::CONFIDENTIAL);
		}
	}

	return queryResult;
}
