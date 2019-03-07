#ifndef OPERATORS_QUERYTOOLS_H
#define OPERATORS_QUERYTOOLS_H

#include "userdb/userdb.h"
#include "operators/queryprofiler.h"

/*
 * This class contains references to a few useful things during query execution.
 * It is meant to be easily extendable to avoid changing the operator API every time a tool is added.
 */
class QueryTools {
	public:
        explicit QueryTools(QueryProfiler &profiler) : profiler(profiler) {};
		QueryTools(QueryProfiler &profiler, std::shared_ptr<UserDB::Session> session) : profiler(profiler), session(session) {};

		QueryProfiler &profiler;
		std::shared_ptr<UserDB::Session> session;
};


#endif
