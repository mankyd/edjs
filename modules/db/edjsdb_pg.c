#include "jsapi.h"
#include <libpq-fe.h>
#include <string.h>
#include <math.h>
#include "edjsdb.h"
#include "edjsdb_pg.h"

static void edjsdb_pg_finalize(JSContext *cx, JSObject *obj) {
    edjsdb_private *p;
    p = (edjsdb_private *) JS_GetPrivate(cx, obj);
    if (NULL == p) {
        //report error? shouldn't really happen
    }
    else if (p->db_connection != NULL) {
        PQfinish(p->db_connection);
        p->db_connection = NULL;
    }
}


static JSBool edjsdb_pg_connect(JSContext *cx, JSObject *obj, uintN argc, jsval *argv) {
    if (argc != 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p = (edjsdb_private *)JS_GetPrivate(cx, obj);
    if (NULL == p) {
        //report error
        return JS_FALSE;
    }
    
    JSString *connect_str = JS_ValueToString(cx, *argv);

    if (JS_FALSE == JS_AddRoot(cx, connect_str)) {
        //report error
        return JS_FALSE;
    }

    if (NULL == connect_str) {
        //report error
        return JS_FALSE;
    }

    p->db_connection = (void *)PQconnectdb(JS_GetStringBytes(connect_str));

    if (NULL == p->db_connection) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }

    if (CONNECTION_OK != PQstatus(p->db_connection)) {
        PQfinish(p->db_connection);
        p->db_connection = NULL;
        //report error
        return JS_FALSE;
    }

    if (JS_FALSE == JS_RemoveRoot(cx, connect_str)) {
        //report error
        return JS_FALSE;
    }

    return JS_TRUE;
}

static JSBool edjsdb_pg_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv) {
    if (argc != 0) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p = (edjsdb_private *)JS_GetPrivate(cx, obj);
    if (NULL == p) {
        //report error
        return JS_FALSE;
    }

    if (NULL == p->db_connection) {
        //report error
        return JS_FALSE;
    }

    PQfinish(p->db_connection);
    p->db_connection = NULL;

    return JS_TRUE;
}


static JSBool edjsdb_pg_query(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        return JS_FALSE;
    }

    JSObject *ret_obj = JS_NewArrayObject(cx, 0, NULL);
    if (NULL == ret_obj) {
        //report error
        return JS_FALSE;
    }

    if (JS_FALSE == JS_AddRoot(cx, ret_obj)) {
        //report error
        return JS_FALSE;
    }

    JSString *command_str;
    command_str = JS_ValueToString(cx, *argv);
    if (JS_FALSE == JS_AddRoot(cx, command_str)) {
        //report error
        return JS_FALSE;
    }

    PGresult *r = PQexec(p->db_connection, JS_GetStringBytes(command_str));

    if (JS_FALSE == JS_RemoveRoot(cx, command_str)) {
        //report error
        return JS_FALSE;
    }

    if (JS_FALSE == edjsdb_pg_handle_error(cx, r)) {
        PQclear(r);
        JS_RemoveRoot(cx, ret_obj);

        return JS_FALSE;
    }
    
    int num_fields = PQnfields(r);
    int num_rows = PQntuples(r);
    int i;
    int j;
    JSObject *row_obj;
    JSString *field_str;
    jsval field_val;
    
    //loop over all of our results
    //create a new object for each row
    for (i = 0; i < num_rows; i++) {
        row_obj = JS_NewObject(cx, NULL, NULL, NULL);
        if (JS_FALSE == JS_AddRoot(cx,  row_obj)) {
            PQclear(r);
            //report error
            JS_RemoveRoot(cx, ret_obj);
            return JS_FALSE;
        }

        for (j = 0; j < num_fields; j++) {
            if (1 == PQgetisnull(r, i, j)) {
                field_val = JSVAL_NULL;
            }
            else {
                field_str = JS_NewStringCopyN(cx, PQgetvalue(r, i, j), PQgetlength(r, i, j));
                if (NULL == field_str) {
                    PQclear(r);
                    JS_RemoveRoot(cx, row_obj);
                    JS_RemoveRoot(cx, ret_obj);
                    //report error
                    return JS_FALSE;
                }
                field_val = STRING_TO_JSVAL(field_str);
            }
            if (JS_FALSE == JS_SetProperty(cx, row_obj, PQfname(r, j), &field_val)) {
                PQclear(r);
                JS_RemoveRoot(cx, row_obj);
                JS_RemoveRoot(cx, ret_obj);
                //report error
                return JS_FALSE;
            }

        }

        if (JS_FALSE == JS_DefineElement(cx, ret_obj, i, OBJECT_TO_JSVAL(row_obj), NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) {
            PQclear(r);
            JS_RemoveRoot(cx, row_obj);
            JS_RemoveRoot(cx, ret_obj);
            //report error
            return JS_FALSE;            
        }

        if (JS_FALSE == JS_RemoveRoot(cx, row_obj)) {
            PQclear(r);
            JS_RemoveRoot(cx, ret_obj);
            //report error
            return JS_FALSE;
        }

    }

    PQclear(r);

    *rval = OBJECT_TO_JSVAL(ret_obj);
    if (JS_FALSE == JS_RemoveRoot(cx, ret_obj)) {
        //report error
        return JS_FALSE;
    }
        
    return JS_TRUE;
}



static JSBool edjsdb_pg_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        return JS_FALSE;
    }

    JSString *command_str;
    command_str = JS_ValueToString(cx, *argv);
    if (JS_FALSE == JS_AddRoot(cx, command_str)) {
        //report error
        return JS_FALSE;
    }

    PGresult *r = PQexec(p->db_connection, JS_GetStringBytes(command_str));

    if (JS_FALSE == JS_RemoveRoot(cx, command_str)) {
        PQclear(r);
        //report error
        return JS_FALSE;
    }

    JSBool ret;
    if (JS_TRUE == (ret = edjsdb_pg_handle_error(cx, r))) {
        JSString *ret_str = JS_NewStringCopyZ(cx, PQcmdTuples(r));
        if (NULL == ret_str) {
            PQclear(r);
            //report error
            return JS_FALSE;
        }

        *rval = STRING_TO_JSVAL(ret_str); 
    }
    PQclear(r);
    return ret;
}

static JSBool edjsdb_pg_create_sequence(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    jsdouble start_d = 1;
    size_t command_len = 1; //1 for the \0
    if (argc == 0) {
        //report error
        return JS_FALSE;
    }
    else if (argc == 1) {
        command_len++; //+1 for the 1
    }
    else if (argc == 2) {
        if (JS_FALSE == JS_ValueToNumber(cx, *(argv+1), &start_d)) {
            //report error
            return JS_FALSE;
        }
        start_d = floor(start_d);
        char start_buf[64]; //FIX ME: determine buf length programatically?
        sprintf(start_buf, "%d", (int)start_d);
        command_len += strlen(start_buf);
    }
    else {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        return JS_FALSE;
    }

    size_t len;
    char *escaped_sequence_name_c = edjsdb_pg_escape_jsval(cx, p->db_connection, *argv, JS_FALSE, JS_FALSE, &len);
    if (NULL == escaped_sequence_name_c) {
        //report error
        return JS_FALSE;
    }

    command_len += strlen("CREATE SEQUENCE  START ");
    command_len += strlen(escaped_sequence_name_c);

    char *command_c = JS_malloc(cx, command_len);
    sprintf(command_c, "CREATE SEQUENCE %s START %d", escaped_sequence_name_c, (int)start_d);
    JS_free(cx, escaped_sequence_name_c);

    PGresult *r = PQexec(p->db_connection, command_c);
    JS_free(cx, command_c);

    JSBool ret;
    if (JS_TRUE == (ret = edjsdb_pg_handle_error(cx, r))) {
        *rval = JSVAL_TRUE; 
    }
    PQclear(r);

    return ret;
}

static JSBool edjsdb_pg_drop_sequence(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        return JS_FALSE;
    }

    size_t len;
    char *escaped_sequence_name_c = edjsdb_pg_escape_jsval(cx, p->db_connection, *argv, JS_FALSE, JS_FALSE, &len);
    if (NULL == escaped_sequence_name_c) {
        //report error
        return JS_FALSE;
    }

    size_t command_len = 1; //1 for the \0
    command_len += strlen("DROP SEQUENCE ");
    command_len += strlen(escaped_sequence_name_c);

    char *command_c = JS_malloc(cx, command_len);
    sprintf(command_c, "DROP SEQUENCE %s", escaped_sequence_name_c);
    JS_free(cx, escaped_sequence_name_c);

    PGresult *r = PQexec(p->db_connection, command_c);
    JS_free(cx, command_c);

    JSBool ret;
    if (JS_TRUE == (ret = edjsdb_pg_handle_error(cx, r))) {
        *rval = JSVAL_TRUE; 
    }
    PQclear(r);

    return ret;
}

static JSBool edjsdb_pg_list_sequences(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 0) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        return JS_FALSE;
    }


    char *command_c = "SELECT relname "
                      "FROM pg_class "
                      "WHERE relkind = 'S'"
                      "      AND relnamespace IN (SELECT oid "
                      "                           FROM pg_namespace "
                      "                           WHERE nspname NOT LIKE 'pg_%' AND nspname != 'information_schema')";

    PGresult *r = PQexec(p->db_connection, command_c);

    if (JS_FALSE == edjsdb_pg_handle_error(cx, r)) {
        PQclear(r);

        return JS_FALSE;
    }
    
    int num_rows = PQntuples(r);
    int i;
    JSString *field_str;
    
    JSObject *ret_obj = JS_NewArrayObject(cx, 0, NULL);
    if (NULL == ret_obj) {
        //report error
        return JS_FALSE;
    }

    if (JS_FALSE == JS_AddRoot(cx, ret_obj)) {
        //report error
        return JS_FALSE;
    }

    //loop over all of our results
    //create a new entry for each row
    for (i = 0; i < num_rows; i++) {
        field_str = JS_NewStringCopyN(cx, PQgetvalue(r, i, 0), PQgetlength(r, i, 0));
        if (NULL == field_str) {
            PQclear(r);
            JS_RemoveRoot(cx, ret_obj);
            //report error
            return JS_FALSE;
        }
        if (JS_FALSE == JS_DefineElement(cx, ret_obj, i, STRING_TO_JSVAL(field_str), NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) {
            PQclear(r);
            JS_RemoveRoot(cx, ret_obj);
            //report error
            return JS_FALSE;            
        }

    }

    PQclear(r);

    *rval = OBJECT_TO_JSVAL(ret_obj);
    if (JS_FALSE == JS_RemoveRoot(cx, ret_obj)) {
        //report error
        return JS_FALSE;
    }
        
    return JS_TRUE;
}

static JSBool edjsdb_pg_next_id(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        return JS_FALSE;
    }

    size_t len;
    char *escaped_sequence_name_c = edjsdb_pg_escape_jsval(cx, p->db_connection, *argv, JS_FALSE, JS_FALSE, &len);
    if (NULL == escaped_sequence_name_c) {
        //report error
        return JS_FALSE;
    }

    size_t command_len = 1; //1 for the \0
    command_len += strlen(escaped_sequence_name_c);
    command_len += strlen("SELECT nextval('%s')");

    char *command_c = JS_malloc(cx, command_len);
    sprintf(command_c, "SELECT nextval('%s')", escaped_sequence_name_c);
    JS_free(cx, escaped_sequence_name_c);

    PGresult *r = PQexec(p->db_connection, command_c);
    JS_free(cx, command_c);

    JSBool ret;
    if (JS_TRUE == (ret = edjsdb_pg_handle_error(cx, r))) {
        JSString *nextval_str = JS_NewStringCopyN(cx, PQgetvalue(r, 0, 0), PQgetlength(r, 0, 0));
        if (NULL == nextval_str) {
            //report error
            ret = JS_FALSE;
        }
        else if (JS_FALSE == JS_ConvertValue(cx, STRING_TO_JSVAL(nextval_str), JSTYPE_NUMBER, rval)) {
            //report error
            ret = JS_FALSE;
        }
    }
    PQclear(r);
    return ret;
}

static JSBool edjsdb_pg_current_id(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        return JS_FALSE;
    }

    size_t len;
    char *escaped_sequence_name_c = edjsdb_pg_escape_jsval(cx, p->db_connection, *argv, JS_FALSE, JS_FALSE, &len);
    if (NULL == escaped_sequence_name_c) {
        //report error
        return JS_FALSE;
    }

    size_t command_len = 1; //1 for the \0
    command_len += strlen(escaped_sequence_name_c);
    command_len += strlen("SELECT last_value FROM %s");

    char *command_c = JS_malloc(cx, command_len);
    sprintf(command_c, "SELECT last_value FROM %s", escaped_sequence_name_c);
    JS_free(cx, escaped_sequence_name_c);

    PGresult *r = PQexec(p->db_connection, command_c);
    JS_free(cx, command_c);

    JSBool ret;
    if (JS_TRUE == (ret = edjsdb_pg_handle_error(cx, r))) {
        JSString *nextval_str = JS_NewStringCopyN(cx, PQgetvalue(r, 0, 0), PQgetlength(r, 0, 0));
        if (NULL == nextval_str) {
            //report error
            ret = JS_FALSE;
        }
        else if (JS_FALSE == JS_ConvertValue(cx, STRING_TO_JSVAL(nextval_str), JSTYPE_NUMBER, rval)) {
            //report error
            ret = JS_FALSE;
        }
    }
    PQclear(r);
    return ret;
}

static JSBool edjsdb_pg_quote(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc < 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        return JS_FALSE;
    }

    size_t ret_len;
    if (JS_TRUE == JSVAL_IS_NUMBER(*argv))
        *rval = *argv;
    else {
        JSString *ret = NULL;
        if (JS_TRUE == JSVAL_IS_NULL(*argv) || JS_TRUE == JSVAL_IS_VOID(*argv)) {
            ret = JS_NewStringCopyZ(cx, "NULL");
        }
        else if (JS_TRUE == JSVAL_IS_BOOLEAN(*argv)) {
            if (JSVAL_TRUE == *argv)
                ret = JS_NewStringCopyZ(cx, "TRUE");
            else
                ret = JS_NewStringCopyZ(cx, "FALSE");
        }
        else {
            JSBool outer_quotes = JS_TRUE;
            JSBool wildcards = JS_FALSE;
            if (argc > 1 && JSVAL_FALSE == *(argv+1))
                outer_quotes = JS_FALSE;
            if (argc > 2 && JSVAL_TRUE == *(argv+2))
                wildcards = JS_TRUE;
            char *escaped_field_c = edjsdb_pg_escape_jsval(cx, p->db_connection, *argv, outer_quotes, wildcards, &ret_len);
            if (NULL == escaped_field_c) {
                //report error
                return JS_FALSE;
            }
            
            ret = JS_NewString(cx, escaped_field_c, ret_len); //JS Engine adopts escaped_field_c memory management
            if (NULL == ret)
                JS_free(cx, escaped_field_c);
        }
        if (NULL == ret) {
            //report error
            return JS_FALSE;
        }

        *rval = STRING_TO_JSVAL(ret);
    }

    return JS_TRUE;
}

static char * edjsdb_pg_escape_jsval(JSContext *cx, PGconn *db_connection, jsval field_val, JSBool outer_quotes, JSBool wildcards, size_t *field_len) {
    JSString *field_str = JS_ValueToString(cx, field_val);
    if (JS_FALSE == JS_AddRoot(cx, field_str)) {
        //report error
        return NULL;
    }

    size_t num_wildcards = 0; //the number of wildcards we need to escape
    char *field_c = JS_GetStringBytes(field_str);
    char *field_marker_c;
    if (JS_TRUE == wildcards) {
        //loop over the string and count the number of wildcards
        for (field_marker_c = field_c; *field_marker_c != '\0'; field_marker_c++) {
            if (*field_marker_c == '%' || *field_marker_c == '_')
                num_wildcards++;
            else if (*field_marker_c == '\\')
                num_wildcards += 2;
        }
    }

    *field_len = strlen(field_c) * 2 + 1; //twice the length +1 as defined by pg documentation
    *field_len += num_wildcards;
    if (JS_TRUE == outer_quotes)
        *field_len += 2;
    char *escaped_field_c = JS_malloc(cx, *field_len);
    int e = 0;

    if (JS_TRUE == outer_quotes) {
        *escaped_field_c = '\'';
        *field_len = PQescapeStringConn (db_connection, 
                                         escaped_field_c+1, field_c,
                                         *field_len, &e);
        *(escaped_field_c+(*field_len)+1) = '\'';
        *(escaped_field_c+(*field_len)+2) = '\0';
        (*field_len) += 2;
    }
    else {
        *field_len = PQescapeStringConn (db_connection, 
                                         escaped_field_c, field_c,
                                         *field_len, &e);
    }
    if (JS_TRUE == wildcards) {
        //start at the end of the string and move characters back to make
        //room for wildcard escape characters
        for (field_marker_c = (escaped_field_c + *field_len+num_wildcards); num_wildcards > 0 && field_marker_c != escaped_field_c; field_marker_c--) {
            *field_marker_c = *(field_marker_c-num_wildcards);
            if (*field_marker_c == '%' || *field_marker_c == '_') {
                *(field_marker_c-1) = '\\';
                num_wildcards--;
                field_marker_c--;
            }
            else if (*field_marker_c == '\\') {
                *(field_marker_c-1) = '\\';
                num_wildcards--;
                field_marker_c--;
            }
        }
    }

    if (e != 0) {
        //report error
        JS_free(cx, escaped_field_c);
        JS_RemoveRoot(cx, field_str);
        return NULL;
    }


    if (JS_FALSE == JS_RemoveRoot(cx, field_str)) {
        JS_free(cx, escaped_field_c);
        //report error
        return NULL;
    }

    return escaped_field_c;
}

static JSBool edjsdb_pg_handle_error(JSContext *cx, PGresult *r) {
    ExecStatusType pq_status = PQresultStatus(r);

    if (PGRES_COMMAND_OK == pq_status || PGRES_TUPLES_OK == pq_status) {
        return JS_TRUE;
    }

    if(PGRES_EMPTY_QUERY == pq_status) {
        JS_ReportError(cx, "EDJSDB EMPTY_QUERY ERROR: %s", PQresultErrorMessage(r));
    }
    else if(PGRES_FATAL_ERROR == pq_status) {
        JS_ReportError(cx, "EDJSDB FATAL ERROR: %s", PQresultErrorMessage(r));
    }
    else if(PGRES_BAD_RESPONSE == pq_status) {
        JS_ReportError(cx, "EDJSDB SERVER_RESPONSE ERROR: %s", PQresultErrorMessage(r));
    }
    //PGRES_NONFATAL_ERROR should never be returned, according to documenation
    else {
        JS_ReportError(cx, "EDJSDB UNKNOWN ERROR: %s", PQresultErrorMessage(r));
    }
    
    return JS_FALSE;

}

JSBool edjsdb_driver_instance_init(JSContext *cx, JSObject *obj) {
    edjsdb_private *p = (edjsdb_private *)JS_GetPrivate(cx, obj);
    if (NULL == p) {
        //report error
        return JS_FALSE;
    }

    p->finalize_func = edjsdb_pg_finalize;
    p->connect_func = edjsdb_pg_connect;
    p->close_func = edjsdb_pg_close;
    p->query_func = edjsdb_pg_query;
    p->exec_func = edjsdb_pg_exec;
    p->create_sequence_func = edjsdb_pg_create_sequence;
    p->drop_sequence_func = edjsdb_pg_drop_sequence;
    p->list_sequences_func = edjsdb_pg_list_sequences;
    p->next_id_func = edjsdb_pg_next_id;
    p->current_id_func = edjsdb_pg_current_id;
    p->quote_func = edjsdb_pg_quote;

    return JS_TRUE;
}
