#ifndef EDJSDB_MYSQL_H
#define EDJSDB_MYSQL_H

#include "jsapi.h"

#define MYSQL_SUCCESS 0

//sequences
#define MYSQL_SEQUENCE_CREATE_SQL "CREATE TABLE %s%s (%s INT NOT NULL AUTO_INCREMENT PRIMARY KEY)"
#define MYSQL_SEQUENCE_CREATE_LEN 56 //length minus the %s
#define MYSQL_SEQUENCE_SET_START_SQL "INSERT INTO %s%s (%s) VALUES (%s)"
#define MYSQL_SEQUENCE_SET_START_LEN 25 //length minus the %s
#define MYSQL_SEQUENCE_DROP_SQL "DROP TABLE %s%s"
#define MYSQL_SEQUENCE_DROP_LEN 11
#define MYSQL_SEQUENCE_NEXT_SQL "INSERT INTO %s%s (%s) VALUES (NULL)"
#define MYSQL_SEQUENCE_NEXT_LEN 29 //length minus the %s
#define MYSQL_SEQUENCE_TRIM_SQL "DELETE FROM %s%s WHERE %s < %s"
#define MYSQL_SEQUENCE_TRIM_LEN 22 //length minus the %s
#define MYSQL_SEQUENCE_CURR_SQL "SELECT MAX(%s) AS c FROM %s%s"
#define MYSQL_SEQUENCE_CURR_LEN 23 //length minus the %s
#define MYSQL_SEQUENCE_LIST_SQL "SHOW TABLES LIKE '%s%%'"
#define MYSQL_SEQUENCE_LIST_LEN 20


#define MYSQL_SEQUENCE_PREFIX "seq_" //FIX ME: make this a configurable parameter?
#define MYSQL_SEQUENCE_COLNAME "id" //FIX ME: make this configurable?

static char * edjsdb_mysql_escape_jsval(JSContext *cx, MYSQL *db_connection, jsval field_val, JSBool outer_quotes, JSBool wildcards, unsigned long *field_len);

#endif
