
#include "util/exceptions.h"
#include "util/sqlite.h"

#include <sqlite3.h>

#include <sstream>
#include <string>


SQLite::SQLite() : db(nullptr) {

}

SQLite::~SQLite() {
	if (db) {
		sqlite3_close(db);
		db = nullptr;
	}
}

void SQLite::open(const char *filename, bool readonly) {
	if (db)
		throw SQLiteException("DB already open");

	int rc = sqlite3_open_v2(filename, &db, readonly ? (SQLITE_OPEN_READONLY) : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE), nullptr);
	if (rc != SQLITE_OK)
		throw SQLiteException(concat("Can't open database ", filename, ": ", sqlite3_errmsg(db)));
}


void SQLite::exec(const char *query) {
	char *error = 0;
	if (SQLITE_OK != sqlite3_exec(db, query, NULL, NULL, &error)) {
		auto msg = concat("Error on query ", query, ": ", error);
		sqlite3_free(error);
		throw SQLiteException(msg);
	}
}

int64_t SQLite::getLastInsertId() {
	return sqlite3_last_insert_rowid(db);
}




SQLiteStatement::SQLiteStatement(const SQLite &sqlite) : db(&sqlite), stmt(nullptr) {
}

SQLiteStatement::~SQLiteStatement() {
	finalize();
	db = nullptr;
}


void SQLiteStatement::prepare(const char *query) {
	if (stmt)
		throw SQLiteException("Statement already prepared");

	auto result = sqlite3_prepare_v2(
		db->db,
		query,
		-1, // read until '\0'
		&stmt,
		NULL
	);
	if (result != SQLITE_OK)
		throw SourceException(concat("Cannot prepare statement: ", result, ", error='", sqlite3_errmsg(db->db), "', query='", query, "'"));
}


void SQLiteStatement::bind(int idx, int32_t value) {
	if (!stmt)
		throw SQLiteException("Prepare before binding");

	if (SQLITE_OK != sqlite3_bind_int(stmt, idx, value) )
		throw SQLiteException("error binding int");
}
void SQLiteStatement::bind(int idx, int64_t value) {
	if (!stmt)
		throw SQLiteException("Prepare before binding");

	if (SQLITE_OK != sqlite3_bind_int64(stmt, idx, value) )
		throw SQLiteException("error binding int");
}
void SQLiteStatement::bind(int idx, double value) {
	if (!stmt)
		throw SQLiteException("Prepare before binding");

	if (SQLITE_OK != sqlite3_bind_double(stmt, idx, value) )
		throw SQLiteException("error binding double");
}
void SQLiteStatement::bind(int idx, const char *value) {
	if (!stmt)
		throw SQLiteException("Prepare before binding");

	if (SQLITE_OK != sqlite3_bind_text(stmt, idx, value, -1, SQLITE_TRANSIENT) )
		throw SQLiteException("error binding text");
}


void SQLiteStatement::exec() {
	if (!stmt)
		throw SQLiteException("Prepare before exec");

	if (SQLITE_DONE != sqlite3_step(stmt))
		throw SQLiteException("SQLiteStatement::exec() failed");
	if (SQLITE_OK != sqlite3_reset(stmt))
		throw SQLiteException("SQLiteStatement::reset failed");
}

bool SQLiteStatement::next() {
	if (!stmt)
		throw SQLiteException("Prepare before next");

	int res = sqlite3_step(stmt);

	if (res == SQLITE_ROW)
		return true;
	if (res == SQLITE_DONE) {
		if (SQLITE_OK != sqlite3_reset(stmt))
			throw SQLiteException("SQLiteStatement::reset failed");
		return false;
	}
	throw SQLiteException("SQLiteStatement::next failed");
}


int32_t SQLiteStatement::getInt(int column) {
	return sqlite3_column_int(stmt, column);
}

int64_t SQLiteStatement::getInt64(int column) {
	return sqlite3_column_int64(stmt, column);
}

double SQLiteStatement::getDouble(int column) {
	return sqlite3_column_double(stmt, column);
}

const char *SQLiteStatement::getString(int column) {
	return (const char *) sqlite3_column_text(stmt, column);
}


void SQLiteStatement::finalize() {
	if (stmt) {
		sqlite3_finalize(stmt);
		stmt = nullptr;
	}
}
