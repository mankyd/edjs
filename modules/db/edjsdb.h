#ifndef EDJSDB_H
#define EDJSDB_H

#include "jsapi.h"

typedef struct edjsdb_private {
    void *db_connection;
    void *lib_handle;
    JSBool (*connect_func)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv);
    JSBool (*close_func)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv);
    JSBool (*query_func)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
    JSBool (*exec_func)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
    JSBool (*create_sequence_func)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
    JSBool (*drop_sequence_func)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
    JSBool (*list_sequences_func)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
    JSBool (*next_id_func)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
    JSBool (*current_id_func)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
    JSBool (*quote_func)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
    void (*finalize_func)(JSContext *cx, JSObject *obj);
} edjsdb_private;




#endif
