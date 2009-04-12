#ifndef EDJSDB_CORE_H
#define EDJSDB_CORE_H

#include "jsapi.h"

static void edjsdb_finalize_stub(JSContext *cx, JSObject *obj) {}
static JSBool edjsdb_connect_stub(JSContext *cx, JSObject *obj, uintN argc, jsval *argv) { return JS_TRUE; }
static JSBool edjsdb_close_stub(JSContext *cx, JSObject *obj, uintN argc, jsval *argv) { return JS_TRUE; }
static JSBool edjsdb_func_stub(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) { *rval = JSVAL_VOID; return JS_TRUE;}

static JSBool edjsdb_construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static void edjsdb_finalize(JSContext *cx, JSObject *obj);
static JSBool edjsdb_connect(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjsdb_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjsdb_query(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjsdb_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjsdb_create_sequence(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjsdb_drop_sequence(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjsdb_list_sequences(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjsdb_next_id(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjsdb_current_id(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjsdb_quote(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

struct JSClass edjsdb_class =  {
    "EDJSDB", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, edjsdb_finalize,
    NULL, NULL, 
    NULL,
    NULL,
    NULL, NULL, NULL, NULL };

#endif
