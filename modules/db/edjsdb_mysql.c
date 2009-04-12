#include "jsapi.h"
#include <mysql.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "edjsdb.h"
#include "edjsdb_mysql.h"

void __attribute__ ((constructor)) edjsdb_mysql_init() {
    mysql_library_init(0, NULL, NULL);
}

void __attribute__ ((constructor)) edjsdb_mysql_fini() {
    mysql_library_end();
}


static void edjsdb_mysql_finalize(JSContext *cx, JSObject *obj) {
    edjsdb_private *p;
    p = (edjsdb_private *) JS_GetPrivate(cx, obj);
    if (NULL == p) {
        //report error? shouldn't really happen
    }
    else if (p->db_connection != NULL) {
        mysql_close(p->db_connection);
        p->db_connection = NULL;

    }
}


static JSBool edjsdb_mysql_connect(JSContext *cx, JSObject *obj, uintN argc, jsval *argv) {
    JSBool ok = JS_TRUE;
    JSString *connect_str = NULL;

    JS_AddRoot(cx, &connect_str);

    if (argc != 1) {
        //report error
        goto error;
    }

    edjsdb_private *p = (edjsdb_private *)JS_GetPrivate(cx, obj);
    if (NULL == p) {
        //report error
        goto error;
    }

    connect_str = JS_ValueToString(cx, *argv);

    if (NULL == connect_str) {
        //report error
        goto error;
    }

    p->db_connection = (void *)mysql_init(NULL);
    if (NULL == p->db_connection) {
        JS_ReportOutOfMemory(cx);
        goto error;
    }

    if (NULL == mysql_real_connect(p->db_connection, "", "", "", "test", 0, NULL, CLIENT_MULTI_STATEMENTS)) {
        JS_ReportError(cx, "Error Connecting");
        //report error
        goto error;
    }

    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    JS_RemoveRoot(cx, &connect_str);
    return ok;
}

static JSBool edjsdb_mysql_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv) {
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

    mysql_close(p->db_connection);
    p->db_connection = NULL;
    
    return JS_TRUE;
}

static JSBool edjsdb_mysql_query(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
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

    if (JS_FALSE == JS_AddRoot(cx, &ret_obj)) {
        //report error
        return JS_FALSE;
    }

    JSString *command_str;
    command_str = JS_ValueToString(cx, *argv);
    if (JS_FALSE == JS_AddRoot(cx, &command_str)) {
        //report error
        return JS_FALSE;
    }

    const char *command_c = JS_GetStringBytes(command_str);
    unsigned long len = strlen(command_c); //FIX ME: figure out how to accept binary data

    if (MYSQL_SUCCESS != mysql_real_query(p->db_connection, command_c, len)) {
        //report error
        return JS_FALSE;
    }

    if (JS_FALSE == JS_RemoveRoot(cx, &command_str)) {
        //report error
        return JS_FALSE;
    }

    MYSQL_RES *res = mysql_use_result(p->db_connection);
    MYSQL_ROW row;
    MYSQL_FIELD *fields = mysql_fetch_fields(res);

    int num_fields = mysql_num_fields(res);
    unsigned long *lengths;
    JSObject *row_obj;
    JSString *field_str;
    jsval field_val;

    int i;
    int row_count = 0;
    while (NULL != (row = mysql_fetch_row(res))) {
        row_obj = JS_NewObject(cx, NULL, NULL, NULL);
        if (JS_FALSE == JS_AddRoot(cx, &row_obj)) {
            //exhaust the remaining rows
            while (NULL != mysql_fetch_row(res));
            mysql_free_result(res);
            //report error
            JS_RemoveRoot(cx, &ret_obj);
            return JS_FALSE;
        }

        lengths = mysql_fetch_lengths(res);
        for(i = 0; i < num_fields; i++) {
            if (NULL == row[i]) {
                field_val = JSVAL_NULL;
            }
            else {
                field_str = JS_NewStringCopyN(cx, row[i], lengths[i]);
                if (NULL == field_str) {
                    //exhaust the remaining rows
                    while (NULL != mysql_fetch_row(res));
                    mysql_free_result(res);
                    JS_RemoveRoot(cx, &row_obj);
                    JS_RemoveRoot(cx, &ret_obj);
                    //report error
                    return JS_FALSE;
                }
                field_val = STRING_TO_JSVAL(field_str);
            }
            if (JS_FALSE == JS_SetProperty(cx, row_obj, fields[i].name, &field_val)) {
                //exhaust the remaining rows
                while (NULL != mysql_fetch_row(res));
                mysql_free_result(res);
                JS_RemoveRoot(cx, &row_obj);
                JS_RemoveRoot(cx, &ret_obj);
                //report error
                return JS_FALSE;
            }
        }

        if (JS_FALSE == JS_DefineElement(cx, ret_obj, row_count, OBJECT_TO_JSVAL(row_obj), NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) {
            //exhaust the remaining rows
            while (NULL != mysql_fetch_row(res));
            mysql_free_result(res);
            JS_RemoveRoot(cx, &row_obj);
            JS_RemoveRoot(cx, &ret_obj);
            //report error
            return JS_FALSE;            
        }

        if (JS_FALSE == JS_RemoveRoot(cx, &row_obj)) {
            //exhaust the remaining rows
            while (NULL != mysql_fetch_row(res));
            mysql_free_result(res);
            JS_RemoveRoot(cx, &ret_obj);
            //report error
            return JS_FALSE;
        }

        row_count++;
    }
    //FIX ME: check for error. mysql_fetch_row() returns NULL either when its done or when there is an error
    mysql_free_result(res);

    *rval = OBJECT_TO_JSVAL(ret_obj);
    if (JS_FALSE == JS_RemoveRoot(cx, &ret_obj)) {
        //report error
        return JS_FALSE;
    }
        
    return JS_TRUE;
}

static JSBool edjsdb_mysql_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
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
    if (JS_FALSE == JS_AddRoot(cx, &command_str)) {
        //report error
        return JS_FALSE;
    }


    const char *command_c = JS_GetStringBytes(command_str);
    unsigned long len = strlen(command_c); //FIX ME: figure out how to accept binary data

    if (MYSQL_SUCCESS != mysql_real_query(p->db_connection, command_c, len)) {
        JS_RemoveRoot(cx, &command_str);
        //report error
        return JS_FALSE;
    }

    if (JS_FALSE == JS_RemoveRoot(cx, &command_str)) {
        //report error
        return JS_FALSE;
    }

    return JS_NewNumberValue(cx, mysql_affected_rows(p->db_connection), rval);
}

static JSBool edjsdb_mysql_create_sequence(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    jsdouble start_d = 1;
    unsigned long command_len = 1; //1 for the \0
    if (argc == 0) {
        //report error
        return JS_FALSE;
    }
    else if (argc == 1) {
    }
    else if (argc == 2) {
        if (JS_FALSE == JS_ValueToNumber(cx, *(argv+1), &start_d)) {
            //report error
            return JS_FALSE;
        }
        start_d = floor(start_d);
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

    unsigned long len;
    char *escaped_sequence_name_c = edjsdb_mysql_escape_jsval(cx, p->db_connection, *argv, JS_FALSE, JS_FALSE, &len);

    if (NULL == escaped_sequence_name_c) {
        //report error
        return JS_FALSE;
    }

    command_len += MYSQL_SEQUENCE_CREATE_LEN;
    command_len += strlen(MYSQL_SEQUENCE_PREFIX);
    command_len += len;//strlen(escaped_sequence_name_c);
    command_len += strlen(MYSQL_SEQUENCE_COLNAME);

    char *command_c = JS_malloc(cx, command_len);
    sprintf(command_c, MYSQL_SEQUENCE_CREATE_SQL, MYSQL_SEQUENCE_PREFIX, escaped_sequence_name_c, MYSQL_SEQUENCE_COLNAME);
    
    if (MYSQL_SUCCESS != mysql_real_query(p->db_connection, command_c, command_len)) {
        JS_ReportError(cx, mysql_error(p->db_connection));
        //report error
        return JS_FALSE;
    }
    JS_free(cx, command_c);
    

    if (start_d > 1) {
        char start_buf[64]; //FIX ME: determine buf length programatically?
        sprintf(start_buf, "%d", (int)(start_d-1));
        command_len = MYSQL_SEQUENCE_SET_START_LEN+1;
        command_len += strlen(MYSQL_SEQUENCE_PREFIX);
        command_len += len; //strlen(escaped_sequence_name_c);
        command_len += strlen(MYSQL_SEQUENCE_COLNAME);
        command_len += strlen(start_buf);

        command_c = JS_malloc(cx, command_len);
        sprintf(command_c, MYSQL_SEQUENCE_SET_START_SQL, MYSQL_SEQUENCE_PREFIX, escaped_sequence_name_c, MYSQL_SEQUENCE_COLNAME, start_buf);
        JS_free(cx, escaped_sequence_name_c);
        if (MYSQL_SUCCESS != mysql_real_query(p->db_connection, command_c, command_len)) {
            JS_ReportError(cx, mysql_error(p->db_connection));
            //report error
            return JS_FALSE;
        }
        JS_free(cx, command_c);
    }

    *rval = JSVAL_TRUE; 

    return JS_TRUE;
}

static JSBool edjsdb_mysql_drop_sequence(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
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

    unsigned long len;
    char *escaped_sequence_name_c = edjsdb_mysql_escape_jsval(cx, p->db_connection, *argv, JS_FALSE, JS_FALSE, &len);

    if (NULL == escaped_sequence_name_c) {
        //report error
        return JS_FALSE;
    }
    unsigned long command_len = 1; //1 for the \0
    command_len += MYSQL_SEQUENCE_DROP_LEN;
    command_len += strlen(MYSQL_SEQUENCE_PREFIX);
    command_len += len;

    char *command_c = JS_malloc(cx, command_len);
    sprintf(command_c, MYSQL_SEQUENCE_DROP_SQL, MYSQL_SEQUENCE_PREFIX, escaped_sequence_name_c);
    JS_free(cx, escaped_sequence_name_c);
    
    if (MYSQL_SUCCESS != mysql_real_query(p->db_connection, command_c, command_len)) {
        JS_ReportError(cx, mysql_error(p->db_connection));
        //report error
        return JS_FALSE;
    }
    JS_free(cx, command_c);
    
    *rval = JSVAL_TRUE; 

    return JS_TRUE;
}

static JSBool edjsdb_mysql_list_sequences(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
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

    JSObject *ret_obj = JS_NewArrayObject(cx, 0, NULL);
    if (NULL == ret_obj) {
        //report error
        return JS_FALSE;
    }

    if (JS_FALSE == JS_AddRoot(cx, &ret_obj)) {
        //report error
        return JS_FALSE;
    }

    unsigned long command_len = 1; //1 for the \0
    command_len += MYSQL_SEQUENCE_LIST_LEN;
    command_len += strlen(MYSQL_SEQUENCE_PREFIX);

    char *command_c = JS_malloc(cx, command_len);
    sprintf(command_c, MYSQL_SEQUENCE_LIST_SQL, MYSQL_SEQUENCE_PREFIX);

    if (MYSQL_SUCCESS != mysql_real_query(p->db_connection, command_c, command_len)) {
        //report error
        JS_ReportError(cx, "Bad List Sequences Query");
        return JS_FALSE;
    }
    JS_free(cx, command_c);

    MYSQL_RES *res = mysql_use_result(p->db_connection);
    MYSQL_ROW row;
    unsigned long *lengths;
    JSString *field_str;
    jsval field_val;

    int row_count = 0;

    while (NULL != (row = mysql_fetch_row(res))) {
        lengths = mysql_fetch_lengths(res);
        field_str = JS_NewStringCopyN(cx, row[0], lengths[0]);
        if (NULL == field_str) {

            //exhaust the remaining rows
            while (NULL != mysql_fetch_row(res));
            mysql_free_result(res);
            JS_RemoveRoot(cx, &ret_obj);
            //report error
            return JS_FALSE;
        }
        field_val = STRING_TO_JSVAL(field_str);

        if (JS_FALSE == JS_DefineElement(cx, ret_obj, row_count, field_val, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) {
            //exhaust the remaining rows
            while (NULL != mysql_fetch_row(res));
            mysql_free_result(res);
            JS_RemoveRoot(cx, &ret_obj);
            //report error
            return JS_FALSE;            
        }
        row_count++;
    }
    //FIX ME: check for error. mysql_fetch_row() returns NULL either when its done or when there is an error
    mysql_free_result(res);


    *rval = OBJECT_TO_JSVAL(ret_obj);
    if (JS_FALSE == JS_RemoveRoot(cx, &ret_obj)) {
        //report error
        return JS_FALSE;
    }

    return JS_TRUE;
}

static JSBool edjsdb_mysql_next_id(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
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

    unsigned long len;
    char *escaped_sequence_name_c = edjsdb_mysql_escape_jsval(cx, p->db_connection, *argv, JS_FALSE, JS_FALSE, &len);

    if (NULL == escaped_sequence_name_c) {
        //report error
        return JS_FALSE;
    }
    unsigned long command_len = 1; //1 for the \0
    command_len += MYSQL_SEQUENCE_NEXT_LEN;
    command_len += strlen(MYSQL_SEQUENCE_PREFIX);
    command_len += len;
    command_len += strlen(MYSQL_SEQUENCE_COLNAME);

    char *command_c = JS_malloc(cx, command_len);
    sprintf(command_c, MYSQL_SEQUENCE_NEXT_SQL, MYSQL_SEQUENCE_PREFIX, escaped_sequence_name_c, MYSQL_SEQUENCE_COLNAME);
    
    if (MYSQL_SUCCESS != mysql_real_query(p->db_connection, command_c, command_len)) {
        JS_ReportError(cx, mysql_error(p->db_connection));
        //report error
        return JS_FALSE;
    }
    JS_free(cx, command_c);

    my_ulonglong insert_id = mysql_insert_id(p->db_connection);
    if (0 == insert_id) {
        //report error
        return JS_FALSE;
    }

    if (JS_FALSE == JS_NewNumberValue(cx, (jsdouble)insert_id, rval)) {
        //report error
        return JS_FALSE;
    }

    char id_buf[64]; //FIX ME: determine buf length programatically?
    sprintf(id_buf, "%lu", (long unsigned int)insert_id);
    command_len = 1; //1 for the \0
    command_len += MYSQL_SEQUENCE_TRIM_LEN;
    command_len += strlen(MYSQL_SEQUENCE_PREFIX);
    command_len += len;
    command_len += strlen(MYSQL_SEQUENCE_COLNAME);
    command_len += strlen(id_buf);

    command_c = JS_malloc(cx, command_len);
    sprintf(command_c, MYSQL_SEQUENCE_TRIM_SQL, MYSQL_SEQUENCE_PREFIX, escaped_sequence_name_c, MYSQL_SEQUENCE_COLNAME, id_buf);
    JS_free(cx, escaped_sequence_name_c);
    
    if (MYSQL_SUCCESS != mysql_real_query(p->db_connection, command_c, command_len)) {
        JS_ReportError(cx, mysql_error(p->db_connection));
        //report error
        return JS_FALSE;
    }
    JS_free(cx, command_c);

    return JS_TRUE;
}

static JSBool edjsdb_mysql_current_id(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
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

    unsigned long len;
    char *escaped_sequence_name_c = edjsdb_mysql_escape_jsval(cx, p->db_connection, *argv, JS_FALSE, JS_FALSE, &len);

    if (NULL == escaped_sequence_name_c) {
        //report error
        return JS_FALSE;   
    }

    unsigned long command_len = 1; //1 for the \0
    command_len += MYSQL_SEQUENCE_CURR_LEN;
    command_len += strlen(MYSQL_SEQUENCE_COLNAME);
    command_len += strlen(MYSQL_SEQUENCE_PREFIX);
    command_len += len;

    char *command_c = JS_malloc(cx, command_len);
    sprintf(command_c, MYSQL_SEQUENCE_CURR_SQL, MYSQL_SEQUENCE_COLNAME, MYSQL_SEQUENCE_PREFIX, escaped_sequence_name_c);
    JS_free(cx, escaped_sequence_name_c);

    if (MYSQL_SUCCESS != mysql_real_query(p->db_connection, command_c, command_len)) {
        //report error
        return JS_FALSE;
    }
    JS_free(cx, command_c);

    MYSQL_RES *res = mysql_use_result(p->db_connection); //only expecting one row back
    MYSQL_ROW row;

    int num_fields = mysql_num_fields(res); //should be 1
    unsigned long *lengths;
    JSString *field_str;

    int i;
    while (NULL != (row = mysql_fetch_row(res))) {
        lengths = mysql_fetch_lengths(res);
        for(i = 0; i < num_fields; i++) {
            if (NULL == row[i]) { //should not happen
                *rval = JSVAL_NULL;
            }
            else {
                field_str = JS_NewStringCopyN(cx, row[i], lengths[i]);
                if (NULL == field_str) {
                    //exhaust the remaining rows
                    while (NULL != mysql_fetch_row(res));
                    mysql_free_result(res);
                    //report error
                    return JS_FALSE;
                }
                *rval = STRING_TO_JSVAL(field_str);
            }
        }
    }
    //FIX ME: check for error. mysql_fetch_row() returns NULL either when its done or when there is an error
    mysql_free_result(res);

    return JS_TRUE;
    return JS_TRUE;
}

static JSBool edjsdb_mysql_quote(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
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
            char *escaped_field_c = edjsdb_mysql_escape_jsval(cx, p->db_connection, *argv, outer_quotes, wildcards, &ret_len);
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

static char * edjsdb_mysql_escape_jsval(JSContext *cx, MYSQL *db_connection, jsval field_val, JSBool outer_quotes, JSBool wildcards, unsigned long *field_len) {
    JSString *field_str = JS_ValueToString(cx, field_val);
    if (JS_FALSE == JS_AddRoot(cx, &field_str)) {
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
        *field_len = mysql_real_escape_string(db_connection, escaped_field_c+1, field_c, strlen(field_c));
        *(escaped_field_c+(*field_len)+1) = '\'';
        *(escaped_field_c+(*field_len)+2) = '\0';
        (*field_len) += 2;
    }
    else {
        *field_len = mysql_real_escape_string(db_connection, escaped_field_c, field_c, strlen(field_c));
    }
    if (JS_TRUE == wildcards) {
        //start at the end of the string and move characters back to make
        //room for wildcard escape characters
        for (field_marker_c = (escaped_field_c + *field_len+num_wildcards); num_wildcards > 0 && field_marker_c != escaped_field_c; field_marker_c--) {
            *field_marker_c = *(field_marker_c-num_wildcards);
            if (*field_marker_c == '%' || *field_marker_c == '_' || *field_marker_c == '\\') {
                *(field_marker_c-1) = '\\';
                num_wildcards--;
                field_marker_c--;
                (*field_len)++;
            }
        }
    }

    if (e != 0) {
        //report error
        JS_free(cx, escaped_field_c);
        JS_RemoveRoot(cx, &field_str);
        return NULL;
    }


    if (JS_FALSE == JS_RemoveRoot(cx, &field_str)) {
        JS_free(cx, escaped_field_c);
        //report error
        return NULL;
    }

    return escaped_field_c;
}

/*
static JSBool boodbb_pg_handle_error(JSContext *cx, PGresult *r) {
    ExecStatusType pq_status = PQresultStatus(r);

    if (PGRES_COMMAND_OK == pq_status || PGRES_TUPLES_OK == pq_status) {
        return JS_TRUE;
    }

    if(PGRES_EMPTY_QUERY == pq_status) {
        JS_ReportError(cx, "BooDB EMPTY_QUERY ERROR: %s", PQresultErrorMessage(r));
    }
    else if(PGRES_FATAL_ERROR == pq_status) {
        JS_ReportError(cx, "BooDB FATAL ERROR: %s", PQresultErrorMessage(r));
    }
    else if(PGRES_BAD_RESPONSE == pq_status) {
        JS_ReportError(cx, "BooDB SERVER_RESPONSE ERROR: %s", PQresultErrorMessage(r));
    }
    //PGRES_NONFATAL_ERROR should never be returned, according to documenation
    else {
        JS_ReportError(cx, "BooDB UNKNOWN ERROR: %s", PQresultErrorMessage(r));
    }
    
    return JS_FALSE;

}
*/
JSBool edjsdb_driver_instance_init(JSContext *cx, JSObject *obj) {
    edjsdb_private *p = (edjsdb_private *)JS_GetPrivate(cx, obj);
    if (NULL == p) {
        //report error
        return JS_FALSE;
    }

    //FIX ME: not threadsafe. use a mutex.
    if (0 != mysql_library_init(0, NULL, NULL)) {
        //report error
        return JS_FALSE;
    }

    p->finalize_func = edjsdb_mysql_finalize;
    p->connect_func = edjsdb_mysql_connect;
    p->close_func = edjsdb_mysql_close;
    p->query_func = edjsdb_mysql_query;
    p->exec_func = edjsdb_mysql_exec;
    p->create_sequence_func = edjsdb_mysql_create_sequence;
    p->drop_sequence_func = edjsdb_mysql_drop_sequence;
    p->list_sequences_func = edjsdb_mysql_list_sequences;
    p->next_id_func = edjsdb_mysql_next_id;
    p->current_id_func = edjsdb_mysql_current_id;
    p->quote_func = edjsdb_mysql_quote;

    return JS_TRUE;
}
