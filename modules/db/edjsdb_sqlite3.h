#ifndef EDJSDB_SQLITE3_H
#define EDJSDB_SQLITE3_H

#include "jsapi.h"

//sequences
#define SQLITE3_SEQUENCE_PREFIX "seq_" //FIX ME: make this a configurable parameter?
#define SQLITE3_SEQUENCE_PREFIX_LEN 4
#define SQLITE3_SEQUENCE_COLNAME "id"
#define SQLITE3_SEQUENCE_COLNAME_LEN 2

#define SQLITE3_SEQUENCE_CREATE_SQL "CREATE TABLE %Q (%q INTEGER PRIMARY KEY AUTOINCREMENT)"
#define SQLITE3_SEQUENCE_CREATE_LEN 50 //length minus the %Q
#define SQLITE3_SEQUENCE_SET_START_SQL "INSERT INTO %Q (%q) VALUES (%d)"
#define SQLITE3_SEQUENCE_SET_START_LEN 25 //length minus the %Q
#define SQLITE3_SEQUENCE_DROP_SQL "DROP TABLE %Q"
#define SQLITE3_SEQUENCE_DROP_LEN 11
#define SQLITE3_SEQUENCE_NEXT_SQL "INSERT INTO %Q (%Q) VALUES (NULL)"
#define SQLITE3_SEQUENCE_NEXT_LEN 29 //length minus the %Q
#define SQLITE3_SEQUENCE_TRIM_SQL "DELETE FROM %Q WHERE %q < %d"
#define SQLITE3_SEQUENCE_TRIM_LEN 22 //length minus the %Q
#define SQLITE3_SEQUENCE_CURR_SQL "SELECT MAX(%q) AS c FROM %Q"
#define MYSQL_SEQUENCE_CURR_LEN 23 //length minus the %Q
#define SQLITE3_SEQUENCE_LIST_SQL "SELECT name FROM sqlite_master WHERE type='table' AND sql NOT NULL AND name LIKE '%q%%'"
#define MYSQL_SEQUENCE_LIST_LEN 20



static JSBool edjsdb_sqlite3_handle_error(JSContext *cx, sqlite3 *db, const char *sql_code, int ret);
static char *edjsdb_sqlite3_full_sequence_name(JSContext *cx, jsval *suffix);

#endif
