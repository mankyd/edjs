#include "jsapi.h"
#include "edjs.h"
#include <dlfcn.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "edjsdb.h"
#include "edjsdb_core.h"
#include <stdio.h>

static void edjsdb_finalize(JSContext *cx, JSObject *obj) {
    edjsdb_private *p = NULL;
    p = (edjsdb_private *) JS_GetPrivate(cx, obj);
    if (p != NULL) {
        p->finalize_func(cx, obj);
        if (NULL != p->lib_handle) {
            dlclose(p->lib_handle);
            p->lib_handle = NULL;
        }
        JS_free(cx, p);
    }
}

static JSBool edjsdb_connect(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);
    if (NULL == p) {
        //report error
        return JS_FALSE;
    }
    return p->connect_func(cx, obj, argc, argv);
}

static JSBool edjsdb_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 0) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p = (edjsdb_private *)JS_GetPrivate(cx, obj);
    if (NULL == p) {
        //report error
        return JS_FALSE;
    }

    *rval = JS_TRUE;

    return p->close_func(cx, obj, argc, argv);
}

static JSBool edjsdb_query(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p) {
        //report error
        return JS_FALSE;
    }

    return p->query_func(cx, obj, argc, argv, rval);
}


static JSBool edjsdb_exec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p) {
        //report error
        return JS_FALSE;
    }

    return p->exec_func(cx, obj, argc, argv, rval);
}

static JSBool edjsdb_create_sequence(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc < 1 || argc > 2) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p) {
        //report error
        return JS_FALSE;
    }

    return p->create_sequence_func(cx, obj, argc, argv, rval);
}

static JSBool edjsdb_drop_sequence(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p) {
        //report error
        return JS_FALSE;
    }

    return p->drop_sequence_func(cx, obj, argc, argv, rval);
}


static JSBool edjsdb_list_sequences(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 0) {
        //report error
        return JS_FALSE;
    }
    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p) {
        //report error
        return JS_FALSE;
    }

    return p->list_sequences_func(cx, obj, argc, argv, rval);
}

static JSBool edjsdb_next_id(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p) {
        //report error
        return JS_FALSE;
    }

    return p->next_id_func(cx, obj, argc, argv, rval);
}

static JSBool edjsdb_current_id(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc != 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p) {
        //report error
        return JS_FALSE;
    }

    return p->current_id_func(cx, obj, argc, argv, rval);
}

static JSBool edjsdb_quote(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (argc < 1) {
        //report error
        return JS_FALSE;
    }

    edjsdb_private *p;
    p = (edjsdb_private *)JS_GetPrivate(cx, obj);

    if (NULL == p) {
        //report error
        return JS_FALSE;
    }

    return p->quote_func(cx, obj, argc, argv, rval);
}


static JSBool edjsdb_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    JSObject *proto_obj = NULL;
    JSObject *ret_obj = NULL;
    edjsdb_private *p = NULL;
    JSString *type = NULL;
    char *mod_name = NULL;
    char *mod_path = NULL;
    struct stat *file_stat = NULL;
    JSBool (*db_driver_init)(JSContext *, JSObject *) = NULL;
    edjs_private *edjs_p = NULL;
    jsval path_val = JSVAL_VOID;
    JSObject *path_obj = NULL;


    JSFunctionSpec my_methods[] = {
        {"connect", edjsdb_connect, 1, JSPROP_ENUMERATE, 0},
        {"close", edjsdb_close, 0, 0, 0},
        {"query", edjsdb_query, 1, 0, 0},
        {"exec", edjsdb_exec, 1, 0, 0},
        {"createSequence", edjsdb_create_sequence, 1, 0, 0},
        {"dropSequence", edjsdb_drop_sequence, 1, 0, 0}, 
        {"listSequences", edjsdb_list_sequences, 0, 0, 0}, 
        {"nextID", edjsdb_next_id, 1, 0, 0},
        {"currentID", edjsdb_current_id, 1, 0, 0},
        {"quote", edjsdb_quote, 1, 0, 0},
        {0, 0, 0, 0, 0}
    };

    if (argc != 1) {
        //report error
        goto error;
    }

    JS_AddRoot(cx, &type);

    proto_obj = JS_NewObject(cx, NULL, NULL, NULL);
    if (NULL == proto_obj) {
        //throw error
        goto error;
    }
    *rval = OBJECT_TO_JSVAL(proto_obj); //root proto_obj

    if (JS_FALSE == JS_DefineFunctions(cx, proto_obj, my_methods)) {
        //report error
        goto error;
    }

    ret_obj = JS_NewObject(cx, &edjsdb_class, proto_obj, NULL);//JS_ConstructObjectWithArguments(cx, &edjsdb_class, NULL, NULL, argc, argv);

    if (NULL == ret_obj) {
        //throw error
        goto error;
    }
    *rval = OBJECT_TO_JSVAL(ret_obj); //root ret_obj

    p = (edjsdb_private *)EDJS_malloc(cx, sizeof(edjsdb_private));
    if (NULL == p) {
        goto error;
    }

    p->db_connection = NULL;
    p->lib_handle = NULL;
    p->finalize_func = edjsdb_finalize_stub;
    p->connect_func = edjsdb_connect_stub;
    p->close_func = edjsdb_close_stub;
    p->query_func = edjsdb_func_stub;
    p->exec_func = edjsdb_func_stub;
    p->create_sequence_func = edjsdb_func_stub;
    p->drop_sequence_func = edjsdb_func_stub;
    p->list_sequences_func = edjsdb_func_stub;
    p->next_id_func = edjsdb_func_stub;
    p->current_id_func = edjsdb_func_stub;
    p->quote_func = edjsdb_func_stub;


    if (JS_FALSE == JS_SetPrivate(cx, ret_obj, p)) {
        //report error
        goto error;
    }

    if (JS_FALSE == JS_DefineProperty(cx, ret_obj, "type", argv[0], NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY)) {
        //report error
        goto error;
    }

    type = JS_ValueToString(cx, *argv);
    if (NULL == type) {
        //report error
        goto error;
    }

    mod_name = (char *)EDJS_malloc(cx, (7 + strlen(JS_GetStringBytes(type))) * sizeof(char)); //7 for db_.so\0
    strcpy(mod_name, "db_");
    strcat(mod_name, JS_GetStringBytes(type));
    strcat(mod_name, ".so");

    edjs_p = (edjs_private *)JS_GetContextPrivate(cx);

    if (JS_FALSE == JS_GetProperty(cx, edjs_p->settings_obj, "include_path", &path_val)) {
        //EDJS_ERR(cx, EDJSERR_GET_PROPERTY, "EDJS", "include_path");
        goto error;
    }

    path_obj = JSVAL_TO_OBJECT(path_val);

    if (JS_FALSE == EDJS_ResolveFile(cx, path_obj, NULL, mod_name, &mod_path, &file_stat)) {
        goto error;
    }

    if (NULL == mod_path) {
        //EDJS_ERR(cx, EDJSERR_FILENAME_RESOLVE, JS_GetStringBytes(include_str));
        goto error;
    }

    p->lib_handle = dlopen(mod_path, RTLD_NOW);
    char *dl_error = NULL;

    //if (NULL == p->lib_handle) {
    if ((dl_error = dlerror()) != NULL) {
        p->lib_handle = NULL;
        JS_ReportError(cx, dl_error);
        goto error;
    }


    db_driver_init = dlsym(p->lib_handle, "edjsdb_driver_instance_init");

    //    if (NULL == db_driver_init) {
    if ((dl_error = dlerror()) != NULL) {
        JS_ReportError(cx, dl_error);
        //JS_ReportError(cx, "failed to load edjsdb_driver_instance_init");
        goto error;
    }

    ok = db_driver_init(cx, ret_obj);
    if (JS_FALSE == ok) {
        JS_ReportError(cx, "driver failed its init method");
        goto error;
    }


    goto finish;
 error:
    ok = JS_FALSE;
    *rval = JSVAL_VOID;

 finish:
    JS_RemoveRoot(cx, &type);
    return ok;
}



JSBool edjs_init(JSContext *cx, JSObject **ret) {
    JSBool ok = JS_TRUE;
    JSFunction *func = NULL;

    func = JS_NewFunction(cx, edjsdb_Construct,
                          0, 0, NULL, NULL);

    *ret = (JSObject *)func;//JS_GetFunctionObject(func);

    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    return ok;
}
