#include "jsapi.h"
#include <sqlite3.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "edjsdb.h"
#include "edjsdb_sqlite3.h"

static void edjsdb_sqlite3_finalize(JSContext *cx, JSObject *obj) {
    edjsdb_private *p = NULL;
    p = (edjsdb_private *) JS_GetPrivate(cx, obj);
    if (NULL == p) {
        //report error? shouldn't really happen
    }
    else if (p->db_connection != NULL) {
        sqlite3_close(p->db_connection);
        p->db_connection = NULL;
    }
}


static JSBool edjsdb_sqlite3_connect(JSContext *cx, JSObject *obj, uintN argc, jsval *argv) {
    JSBool ok = JS_TRUE;
    JSString *connect_str = NULL;
    int ret = 0;
    char *connect_c = NULL;

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

    connect_c = JS_GetStringBytes(connect_str);
    if (JS_FALSE == edjsdb_sqlite3_handle_error(cx, p->db_connection, connect_c,
                                               sqlite3_open(connect_c, (sqlite3 **)&(p->db_connection))
                                              ))
        goto error;

    if (NULL == p->db_connection) {
        JS_ReportOutOfMemory(cx);
        goto error;
    }


    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    JS_RemoveRoot(cx, &connect_str);
    return ok;
}

static JSBool edjsdb_sqlite3_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv) {
    JSBool ok = JS_TRUE;
    edjsdb_private *p = NULL;

    if (argc != 0) {
        //report error
        goto error;
    }

    p = (edjsdb_private *)JS_GetPrivate(cx, obj);
    if (NULL == p) {
        //report error
        goto error;
    }

    if (NULL == p->db_connection) {
        //report error
        goto error;
    }

    sqlite3_close(p->db_connection);
    p->db_connection = NULL;
    

    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    return ok;
}

static JSBool edjsdb_sqlite3_query(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjsdb_private *p = NULL;
    JSObject *ret_obj = NULL;
    JSString *command_str = NULL;
    const char *command_c = NULL;
    JSObject *row_obj = NULL;
    JSString *field_str = NULL;
    jsval field_val = JSVAL_VOID;

    if (argc != 1) {
        //report error
        goto error;
    }

    if (JS_FALSE == JS_AddRoot(cx, &ret_obj)) {
        //report error
        goto error;
    }
    if (JS_FALSE == JS_AddRoot(cx, &command_str)) {
        //report error
        goto error;
    }
    if (JS_FALSE == JS_AddRoot(cx, &row_obj)) {
        //report error
        goto error;
    }
    if (JS_FALSE == JS_AddRoot(cx, &field_val)) {
        //report error
        goto error;
    }
    if (JS_FALSE == JS_AddRoot(cx, &field_str)) {
        //report error
        goto error;
    }

    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        goto error;
    }

    ret_obj = JS_NewArrayObject(cx, 0, NULL);
    if (NULL == ret_obj) {
        //report error
        goto error;
    }

    command_str = JS_ValueToString(cx, *argv);

    //FIX ME: figure out how to accept binary data
    command_c = JS_GetStringBytes(command_str);
    int num_rows = 0;
    int num_fields = 0;
    char **result_data = NULL;
    char *error_msg = NULL;

    if (JS_FALSE == edjsdb_sqlite3_handle_error(cx, p->db_connection, command_c,
                                               sqlite3_get_table(p->db_connection, command_c, 
                                                                 &result_data, &num_rows, 
                                                                 &num_fields, &error_msg))) {
        //report error
        goto error;
    }

    int i = num_fields; //offset to account for column names;
    int total_data = (num_rows+1) * num_fields;
    int col_count = 0;
    int row_count = 0;

    while (i < total_data) {
        row_obj = JS_NewObject(cx, NULL, NULL, NULL);
        for(col_count = 0; col_count < num_fields; col_count++, i++) {
            if (NULL == result_data[i]) {
                field_val = JSVAL_NULL;
            }
            else {
                field_str = JS_NewStringCopyZ(cx, result_data[i]);
                if (NULL == field_str) {
                    //report error
                    goto error;
                }
                field_val = STRING_TO_JSVAL(field_str);
            }
            if (JS_FALSE == JS_SetProperty(cx, row_obj, result_data[col_count], &field_val)) {
                //report error
                goto error;
            }
        }

        if (JS_FALSE == JS_DefineElement(cx, ret_obj, row_count, OBJECT_TO_JSVAL(row_obj), NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) {
            //report error
            goto error;
        }

        row_count++;
    }
    sqlite3_free_table(result_data);
    result_data = NULL;


    *rval = OBJECT_TO_JSVAL(ret_obj);

    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    if (NULL != result_data) {
        sqlite3_free_table(result_data);
        result_data = NULL;
    }

    JS_RemoveRoot(cx, &ret_obj);
    JS_RemoveRoot(cx, &command_str);
    JS_RemoveRoot(cx, &row_obj);
    JS_RemoveRoot(cx, &field_val);
    JS_RemoveRoot(cx, &field_str);

    return ok;
}

static JSBool edjsdb_sqlite3_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjsdb_private *p = NULL;
    JSString *command_str = NULL;
    const char *command_c = NULL;

    if (JS_FALSE == JS_AddRoot(cx, &command_str)) {
        //report error
        goto error;
    }

    if (argc != 1) {
        //report error
        goto error;
    }

    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        goto error;
    }

    command_str = JS_ValueToString(cx, *argv);


    command_c = JS_GetStringBytes(command_str);

    int num_rows = 0;
    int num_fields = 0;
    char **result_data = NULL;
    char *error_msg = NULL;

    if (JS_FALSE == edjsdb_sqlite3_handle_error(cx, p->db_connection, command_c,
                                               sqlite3_get_table(p->db_connection, command_c, 
                                                                 &result_data, &num_rows, 
                                                                 &num_fields, &error_msg))) {
        goto error;
    }

    if (JS_FALSE == JS_NewNumberValue(cx, sqlite3_changes(p->db_connection), rval)) {
        //report error
        goto error;
    }


    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    JS_RemoveRoot(cx, &command_str);

    return ok;

}

static JSBool edjsdb_sqlite3_create_sequence(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    jsdouble start_d = 1;
    edjsdb_private *p = NULL;
    char *sequence_name_c = NULL;
    char *command_c = NULL;
    char *error_msg = NULL;

    *rval = JSVAL_TRUE; 

    if (argc == 0) {
        //report error
        goto error;
    }
    else if (argc == 1) {
    }
    else if (argc == 2) {
        if (JS_FALSE == JS_ValueToNumber(cx, *(argv+1), &start_d)) {
            //report error
            goto error;
        }
        start_d = floor(start_d);
    }
    else {
        //report error
        goto error;
    }

    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        goto error;
    }

    sequence_name_c = edjsdb_sqlite3_full_sequence_name(cx, argv);

    if (NULL == sequence_name_c) {
        //report error
        goto error;
    }

    command_c = sqlite3_mprintf(SQLITE3_SEQUENCE_CREATE_SQL, sequence_name_c, SQLITE3_SEQUENCE_COLNAME);
    if (NULL == command_c) {
        //report error
        goto error;
    }

    if (JS_FALSE == edjsdb_sqlite3_handle_error(cx, p->db_connection, command_c,
                                               sqlite3_exec(p->db_connection, command_c, 
                                                            NULL, NULL, &error_msg))) {
        goto error;
    }
    sqlite3_free(command_c);
    command_c = NULL;

    if (start_d > 1) {
        command_c = sqlite3_mprintf(SQLITE3_SEQUENCE_SET_START_SQL, sequence_name_c, SQLITE3_SEQUENCE_COLNAME, (int)(start_d-1));
        if (NULL == command_c) {
            //report error
            goto error;
        }

        if (JS_FALSE == edjsdb_sqlite3_handle_error(cx, p->db_connection, command_c,
                                                   sqlite3_exec(p->db_connection, command_c, 
                                                                NULL, NULL, &error_msg))) {
            goto error;
        }
        sqlite3_free(command_c);
        command_c = NULL;
    }

    goto finish;
 error:
    *rval = JS_FALSE;
    ok = JS_FALSE;

 finish: 
    if (NULL != command_c) {
        sqlite3_free(command_c);
        command_c = NULL;
    }
    if (NULL != sequence_name_c) {
        JS_free(cx, sequence_name_c);
        sequence_name_c = NULL;
    }
    return ok;
}

static JSBool edjsdb_sqlite3_drop_sequence(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjsdb_private *p = NULL;
    char *sequence_name_c = NULL;
    char *command_c = NULL;
    char *error_msg = NULL;

    *rval = JS_TRUE;

    if (argc != 1) {
        //report error
        goto error;
    }

    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        goto error;
    }

    sequence_name_c = edjsdb_sqlite3_full_sequence_name(cx, argv);

    if (NULL == sequence_name_c) {
        //report error
        goto error;
    }

    command_c = sqlite3_mprintf(SQLITE3_SEQUENCE_DROP_SQL, sequence_name_c);
    if (NULL == command_c) {
        //report error
        goto error;
    }

    if (JS_FALSE == edjsdb_sqlite3_handle_error(cx, p->db_connection, command_c,
                                               sqlite3_exec(p->db_connection, command_c, 
                                                            NULL, NULL, &error_msg))) {
        goto error;
    }
    sqlite3_free(command_c);
    command_c = NULL;

    goto finish;
 error:
    *rval = JS_FALSE;
    ok = JS_FALSE;

 finish: 
    if (NULL != command_c) {
        sqlite3_free(command_c);
        command_c = NULL;
    }
    if (NULL != sequence_name_c) {
        JS_free(cx, sequence_name_c);
        sequence_name_c = NULL;
    }
    return ok;
}


static JSBool edjsdb_sqlite3_list_sequences(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjsdb_private *p = NULL;
    JSObject *ret_obj = NULL;
    char *command_c = NULL;
    JSString *field_str = NULL;
    jsval field_val = JSVAL_VOID;

    if (argc != 0) {
        //report error
        goto error;
    }

    if (JS_FALSE == JS_AddRoot(cx, &ret_obj)) {
        //report error
        goto error;
    }
    if (JS_FALSE == JS_AddRoot(cx, &field_val)) {
        //report error
        goto error;
    }

    if (JS_FALSE == JS_AddRoot(cx, &field_str)) {
        //report error
        goto error;
    }
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        goto error;
    }

    ret_obj = JS_NewArrayObject(cx, 0, NULL);
    if (NULL == ret_obj) {
        //report error
        goto error;
    }

    command_c = sqlite3_mprintf(SQLITE3_SEQUENCE_LIST_SQL, SQLITE3_SEQUENCE_PREFIX);
    if (NULL == command_c) {
        //report error
        goto error;
    }

    int num_rows = 0;
    int num_fields = 0;
    char **result_data = NULL;
    char *error_msg = NULL;

    if (JS_FALSE == edjsdb_sqlite3_handle_error(cx, p->db_connection, command_c,
                                               sqlite3_get_table(p->db_connection, command_c, 
                                                                 &result_data, &num_rows, 
                                                                 &num_fields, &error_msg))) {
        goto error;
    }

    sqlite3_free(command_c);
    command_c = NULL;

    int i = num_fields; //offset to account for column names. should be one
    int total_data = (num_rows+1) * num_fields;
    int row_count = 0;

    while (i < total_data) {
        //NOTE: this code assumes one column
        field_str = JS_NewStringCopyZ(cx, (result_data[i] + SQLITE3_SEQUENCE_PREFIX_LEN));
        if (NULL == field_str) {
            //report error
            goto error;
        }
        field_val = STRING_TO_JSVAL(field_str);

        if (JS_FALSE == JS_DefineElement(cx, ret_obj, row_count, field_val, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) {
            //report error
            goto error;
        }
        row_count++;
        i++;
    }
    sqlite3_free_table(result_data);
    result_data = NULL;

    *rval = OBJECT_TO_JSVAL(ret_obj);        

    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    if (NULL != result_data) {
        sqlite3_free_table(result_data);
        result_data = NULL;
    }
    if (NULL != command_c) {
        sqlite3_free(command_c);
        command_c = NULL;
    }

    JS_RemoveRoot(cx, &ret_obj);
    JS_RemoveRoot(cx, &field_str);
    JS_RemoveRoot(cx, &field_val);

    return ok;
}

static JSBool edjsdb_sqlite3_next_id(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjsdb_private *p = NULL;
    char *sequence_name_c = NULL;
    char *command_c = NULL;
    char *error_msg = NULL;
    sqlite3_int64 insert_id = 0;

    if (argc != 1) {
        //report error
        goto error;
    }

    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        goto error;
    }

    sequence_name_c = edjsdb_sqlite3_full_sequence_name(cx, argv);
    if (NULL == sequence_name_c) {
        //report error
        goto error;
    }

    command_c = sqlite3_mprintf(SQLITE3_SEQUENCE_NEXT_SQL, sequence_name_c, SQLITE3_SEQUENCE_COLNAME);
    if (NULL == command_c) {
        //report error
        goto error;
    }

    if (JS_FALSE == edjsdb_sqlite3_handle_error(cx, p->db_connection, command_c,
                                               sqlite3_exec(p->db_connection, command_c, 
                                                            NULL, NULL, &error_msg))) {
        goto error;
    }
    sqlite3_free(command_c);
    command_c = NULL;

    insert_id = sqlite3_last_insert_rowid(p->db_connection);

    if (0 == insert_id) {
        //report error
        goto error;
    }

    if (JS_FALSE == JS_NewNumberValue(cx, (jsdouble)insert_id, rval)) {
        //report error
        goto error;
    }

    command_c = sqlite3_mprintf(SQLITE3_SEQUENCE_TRIM_SQL, sequence_name_c, SQLITE3_SEQUENCE_COLNAME, insert_id);
    if (NULL == command_c) {
        //report error
        goto error;
    }

    if (JS_FALSE == edjsdb_sqlite3_handle_error(cx, p->db_connection, command_c,
                                               sqlite3_exec(p->db_connection, command_c, 
                                                            NULL, NULL, &error_msg))) {
        goto error;
    }
    sqlite3_free(command_c);
    command_c = NULL;
    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    if (NULL != command_c) {
        sqlite3_free(command_c);
        command_c = NULL;
    }
    if (NULL != sequence_name_c) {
        JS_free(cx, sequence_name_c);
        sequence_name_c = NULL;
    }

    return ok;
}

static JSBool edjsdb_sqlite3_current_id(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjsdb_private *p = NULL;
    char *sequence_name_c = NULL;
    char *command_c = NULL;
    sqlite3_stmt *stmt = NULL;

    if (argc != 1) {
        //report error
        goto error;
    }

    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        goto error;
    }

    sequence_name_c = edjsdb_sqlite3_full_sequence_name(cx, argv);
    if (NULL == sequence_name_c) {
        //report error
        goto error;
    }

    command_c = sqlite3_mprintf(SQLITE3_SEQUENCE_CURR_SQL, SQLITE3_SEQUENCE_COLNAME, sequence_name_c);
    if (NULL == command_c) {
        //report error
        goto error;
    }

    if (JS_FALSE == edjsdb_sqlite3_handle_error(cx, p->db_connection, command_c,
                                               sqlite3_prepare_v2(p->db_connection, command_c, -1,
                                                                  &stmt, NULL))) {
        goto error;
    }

    sqlite3_free(command_c);
    command_c = NULL;

    if (SQLITE_ROW != sqlite3_step(stmt)) {
        //report error
        goto error;
    }

    if (JS_FALSE == JS_NewNumberValue(cx, (jsdouble)sqlite3_column_int(stmt, 0), rval)) {
        //report error
        goto error;
    }

    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    if (NULL != command_c) {
        sqlite3_free(command_c);
        command_c = NULL;
    }

    if (NULL != stmt) {
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    if (NULL != sequence_name_c) {
        JS_free(cx, sequence_name_c);
        sequence_name_c = NULL;
    }


    return ok;
}

static JSBool edjsdb_sqlite3_quote(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    JSString *command_str = NULL;
    char *command_c = NULL;
    JSString *ret = NULL;
    int len = 0;
    int i = 0;
    int wild_count = 0;
    char *pos = NULL;
    JSBool outer_quotes = JS_TRUE;
    JSBool wildcards = JS_FALSE;

    if (JS_FALSE == JS_AddRoot(cx, &command_str)) {
        //report error
        goto error;
    }

    if (JS_FALSE == JS_AddRoot(cx, &ret)) {
        //report error
        goto error;
    }

    if (argc < 1) {
        //report error
        goto error;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->db_connection) {
        //report error
        goto error;
    }

    if (JS_TRUE == JSVAL_IS_NUMBER(*argv))
        *rval = *argv;
    else {
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
            if (argc > 1 && JSVAL_FALSE == *(argv+1))
                outer_quotes = JS_FALSE;
            if (argc > 2 && JSVAL_TRUE == *(argv+2))
                wildcards = JS_TRUE;

            command_str = JS_ValueToString(cx, *argv);
            if (NULL == command_str) {
                //report error
                goto error;
            }

            if (JS_TRUE == outer_quotes)
                command_c = sqlite3_mprintf("%Q", JS_GetStringBytes(command_str));
            else
                command_c = sqlite3_mprintf("%q", JS_GetStringBytes(command_str));

            if (NULL == command_c) {
                //report error
                goto error;
            }

            if (JS_TRUE == wildcards) {
                len = 0;
                for (pos = command_c; '\0' != *pos; pos++) {
                    if ('%' == *pos || '_' == *pos)
                        wild_count++;
                    len++;
                }
                len += wild_count;

                pos = sqlite3_realloc(command_c, len+1);
                if (NULL == pos) {
                    //report error
                    goto error;
                }
                command_c = pos;
                for(pos = command_c+len; wild_count > 0; pos--) {
                    *pos = *(pos-wild_count);
                    if ('%' == *pos || '_' == *pos) {
                        pos--;
                        *pos = '\\';
                        wild_count--;
                    }
                }
            }

            ret = JS_NewStringCopyZ(cx, command_c); //JS Engine adopts escaped_field_c memory management
            if (NULL == ret) {
                //report error
                goto error;
            }

            sqlite3_free(command_c);
            command_c = NULL;
        }

        *rval = STRING_TO_JSVAL(ret);
    }

    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    if (NULL != command_c) {
        sqlite3_free(command_c);
        command_c = NULL;
    }
    JS_RemoveRoot(cx, &ret);
    JS_RemoveRoot(cx, &command_str);

    return ok;
}

static JSBool edjsdb_sqlite3_handle_error(JSContext *cx, sqlite3 *db, const char *sql_code, int ret) {
    if (SQLITE_OK == ret)
        return JS_TRUE;


    sqlite3_mutex *m = sqlite3_mutex_alloc(SQLITE_MUTEX_FAST);
    sqlite3_mutex_enter(m);

    int error_code = sqlite3_errcode(db);
    const char *error_msg = sqlite3_errmsg(db);
    JS_ReportError(cx, "Edjsdb ERROR (%d): %s", error_code, error_msg);
    
    sqlite3_mutex_leave(m);
    sqlite3_mutex_free(m);

    return JS_FALSE;

}

static char *edjsdb_sqlite3_full_sequence_name(JSContext *cx, jsval *suffix) {
    char *result = NULL;
    JSString *suffix_str = NULL;
    char *suffix_c = NULL;
    size_t len = SQLITE3_SEQUENCE_PREFIX_LEN;

    if (JS_FALSE == JS_AddRoot(cx, &suffix_str)) {
        //report error
        goto error;
    }

    suffix_str = JS_ValueToString(cx, *suffix);
    if (NULL == suffix_str) {
        //report error
        goto error;
    }
    suffix_c = JS_GetStringBytes(suffix_str);
    len += strlen(suffix_c);

    result = JS_malloc(cx, len + 1); //+1 for '\0'
    result[0] = '\0';
    strcat(result, SQLITE3_SEQUENCE_PREFIX);
    strcat(result, suffix_c);

    goto finish;
 error:
    if (NULL != result) {
        JS_free(cx, result);
        result = NULL;
    }

 finish: 
    JS_RemoveRoot(cx, &suffix_str);

    return result;
}

JSBool edjsdb_driver_instance_init(JSContext *cx, JSObject *obj) {
    edjsdb_private *p = (edjsdb_private *)JS_GetPrivate(cx, obj);
    if (NULL == p) {
       //report error
        return JS_FALSE;
    }

    p->finalize_func = edjsdb_sqlite3_finalize;
    p->connect_func = edjsdb_sqlite3_connect;
    p->close_func = edjsdb_sqlite3_close;
    p->query_func = edjsdb_sqlite3_query;
    p->exec_func = edjsdb_sqlite3_exec;
    p->create_sequence_func = edjsdb_sqlite3_create_sequence;
    p->drop_sequence_func = edjsdb_sqlite3_drop_sequence;
    p->list_sequences_func = edjsdb_sqlite3_list_sequences;
    p->next_id_func = edjsdb_sqlite3_next_id;
    p->current_id_func = edjsdb_sqlite3_current_id;
    p->quote_func = edjsdb_sqlite3_quote;

    return JS_TRUE;
}
