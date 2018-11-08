/*
 * Log.h
 *
 *  Created on: 15.05.2015
 *      Author: mika
 */

#ifndef UTIL_LOG_H_
#define UTIL_LOG_H_

#include <string>
#include <vector>
#include <ostream>
#include <fstream>
#include <stdarg.h>


/**
 * Log functionality
 */
class Log {
public:
	enum class LogLevel {
		OFF, ERROR, WARN, INFO, DEBUG, TRACE
	};

	/**
	 * Logs to a file on disc. Parameters can be set in config (enable/disable, file location, log level).
	 * It will not be disabled with the streamAndMemoryOff() call [former off()].
	 * This allows logging on fcgi level without interfering with memory and stream logging.
	 */
	static void logToFile();

	/**
	 * Logs to a stream, usually std::cerr
	 * There can only be one stream at a time. Calling this again will remove the previous stream.
	 *
	 * Note that it is the caller's responsibility to ensure that the lifetime of the stream is larger than the lifetime of the log.
	 */
	static void logToStream(const std::string &level, std::ostream *stream);
	static void logToStream(LogLevel level, std::ostream *stream);
	/**
	 * Enables logging to memory. It is possible to log both to a stream and to memory at the same time, even with different loglevels.
	 */
	static void logToMemory(const std::string &level);
	static void logToMemory(LogLevel level);
	/**
	 * Retrieves all messages that were logged to memory and flushes the message buffer.
	 */
	static std::vector<std::string> getMemoryMessages();

	/**
	 * Turns stream and memory logging off.
	 */
	static void streamAndMemoryOff();

    /**
     * Turns file logging off.
     */
    static void fileOff();

	static void error(const char *msg, ...);
	static void error(const std::string &msg);
	static void warn(const char *msg, ...);
	static void warn(const std::string &msg);
	static void info(const char *msg, ...);
	static void info(const std::string &msg);
	static void debug(const char *msg, ...);
	static void debug(const std::string &msg);
	static void trace(const char *msg, ...);
	static void trace(const std::string &msg);

	/**
	 * Sets the thread_local variable current_request_id. If logRequestId is set to true
	 * the request id of the current thread will be written in front of the log messages.
	 */
	static void setThreadRequestId(int id);

    static bool log_request_id;

	/**
	 * Set if the current request id will be written before logs. Call setThreadRequestId
	 * to set the current id for the calling thread.
	 */
	static void logRequestId(bool value);

	static thread_local int current_request_id;
};

#endif /* LOG_H_ */
