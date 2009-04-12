#ifndef EDJSDB_PG_H
#define EDJSDB_PG_H

#include "jsapi.h"

static JSBool edjsdb_pg_handle_error(JSContext *cx, PGresult *r);
static char * edjsdb_pg_escape_jsval(JSContext *cx, PGconn *db_connection, jsval field_val, JSBool outer_quotes, JSBool wildcards, size_t *field_len);

#endif
