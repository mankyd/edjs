/*
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright 2009 Dave Mankoff (mankyd@gmail.com)
 * License Version: GPL 3.0
 *
 * This file is part of EDJS 0.x
 *
 * EDJS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EDJS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EDJS.  If not, see <http://www.gnu.org/licenses/>.
 * ***** END LICENSE BLOCK *****
 */

#include <unistd.h>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "jsapi.h"

#include "edjs.h"
#include "edjs_error.h"

static JSClass global_class = {
    "global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool branch_callback(JSContext *cx, JSScript *script) {
    edjs_private *p = (edjs_private *)JS_GetContextPrivate(cx);

    p->branch_count++;
    if (1==1 || p->branch_count == 1000) {
        p->branch_count = 0;
        
        JS_GC(cx);

        if (p->max_execution_time > 0) {
            if (((double) (clock() - p->start_time)) / CLOCKS_PER_SEC > p->max_execution_time) {
                p->max_execution_time_exceeded = JS_TRUE;
                return JS_FALSE;
            }
        }
    }

    return JS_TRUE;
}

static JSBool gc_callback(JSContext *cx, JSGCStatus status) {
    edjs_runtime_private *rt_private = JS_GetRuntimePrivate(JS_GetRuntime(cx));
    edjs_list_node *it = NULL;
    edjs_list_node *pit = NULL;
    edjs_imp_module *mod = NULL;

    switch(status) {
    case JSGC_MARK_END:
        pthread_mutex_lock(&(rt_private->mutex));

        for (it = rt_private->imported_objects; it != NULL; it = pit) {
            pit = it->next;
            mod = (edjs_imp_module *)((edjs_tree_node *)(it->value))->value;

            if (JS_IsAboutToBeFinalized(cx, mod->constructed_obj)) {
                if (rt_private->imported_objects == it) { //it is the head node
                    rt_private->imported_objects = it->next;
                }
                it->next = rt_private->gced_objects;
                rt_private->gced_objects = it;
            }
        }

        pthread_mutex_unlock(&(rt_private->mutex));
        break;

    case JSGC_END:
        pthread_mutex_lock(&(rt_private->mutex));

        for (it = rt_private->gced_objects, pit = NULL; it != NULL; it = pit) {
            pit = it->next;
            mod = (edjs_imp_module *)((edjs_tree_node *)(it->value))->value; 
            edjs_imp_module_destroy(cx, mod);
            EDJS_free(cx, it->value);
            EDJS_free(cx, it);
        }
        rt_private->gced_objects = NULL;

        pthread_mutex_unlock(&(rt_private->mutex));
        break;

    default:
        break;
    }

    return JS_TRUE;
}

// The error reporter callback.
void edjs_ReportJSError(JSContext *cx, const char *message, JSErrorReport *report) {
    edjs_private *p = (edjs_private *)JS_GetContextPrivate(cx);

    if (NULL != p->error_function) {
        p->error_function(cx, message, report);
    }
    else {
        fprintf(stderr,"%s\n", message);
    }
}


static JSBool edjs_SetMaxExecutionTime(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
    edjs_private *p = NULL;
    uint32 t = 0;

    p = (edjs_private *)JS_GetContextPrivate(cx);
    if (NULL == p) {
        EDJS_ERR(cx, EDJSERR_PRIVATE_DATA_NOT_SET);
        return JS_FALSE;
    }

    if (JS_FALSE == JS_ValueToECMAUint32(cx, *vp, &t)) {
        EDJS_ERR(cx, EDJSERR_INT_CONVERSION);
        return JS_FALSE;
    }

    p->max_execution_time = t;

    return JS_TRUE;
}

void *EDJS_malloc(JSContext *cx, size_t nbytes) {
    if (NULL == cx)
        return malloc(nbytes);

    return JS_malloc(cx, nbytes);
}

void * EDJS_realloc(JSContext *cx, void *p, size_t nbytes) {
    if (NULL == cx)
        return realloc(p, nbytes);

    return JS_realloc(cx, p, nbytes);
}

char * EDJS_strdup(JSContext *cx, const char *s) {
    if (NULL == cx)
        return strdup(s);

    return JS_strdup(cx, s);
}

void EDJS_free(JSContext *cx, void *p) {
    if (NULL == cx)
        return free(p);

    return JS_free(cx, p);
}


JSRuntime *EDJS_CreateRuntime(const char *settings_loc, EDJS_ErrNum *error_num) {
    //fix me: make sure runtime is destroyed and memory freed properly in error cases
    JSRuntime *rt = NULL;
    JSContext *cx = NULL; 
    JSObject *global = NULL;
    JSScript *settings_script = NULL;
    JSObject *settings_script_obj = NULL;

    JSObject *settings_obj = NULL;
    jsval settings_val = JSVAL_VOID;
    JSObject *array_obj = NULL;
    JSString *temp_str = NULL;
    jsval temp_val = JSVAL_VOID;

    struct edjs_private p;
    edjs_runtime_private *rt_private = NULL;

    *error_num = EDJSERR_NOT_AN_ERROR;

    rt_private = (edjs_runtime_private *)malloc(sizeof(edjs_runtime_private));
    if (NULL == rt_private) {
        *error_num = EDJSERR_OUT_OF_MEMORY;
        goto error;
    }

    pthread_mutex_init(&(rt_private->mutex), NULL);
    rt_private->core_script = NULL;
    rt_private->core_script_obj = NULL;
    rt_private->settings_obj = NULL;
    rt_private->imported_objects = NULL;
    rt_private->gced_objects = NULL;

    p.rt_private = rt_private;
    p.echo_function = NULL;
    p.error_function = NULL;
    p.random_seeded = JS_FALSE;
    p.private_data = NULL;
    p.branch_count = 0;
    p.imported_modules = NULL;
    p.settings_obj = NULL;
    

    rt = JS_NewRuntime(8L * 1024L * 1024L); //8megs
    if (NULL == rt) {
        *error_num = EDJSERR_CREATING_RUNTIME;
        goto error;
    }

    cx = EDJS_CreateContext(rt, &p, JS_FALSE, error_num);
    if (cx == NULL) {
        //error_num will be set
        goto error;
    }

    JS_BeginRequest(cx);

    if (JS_FALSE == JS_AddRoot(cx, &settings_script_obj)) {
        *error_num = EDJSERR_ADD_ROOT;
        goto error;
    }

    if (JS_FALSE == JS_AddRoot(cx, &array_obj)) {
        *error_num = EDJSERR_ADD_ROOT;
        goto error;
    }

    if (JS_FALSE == JS_AddRoot(cx, &temp_str)) {
        *error_num = EDJSERR_ADD_ROOT;
        goto error;
    }

    global = JS_GetGlobalObject(cx);

    rt_private->core_script = JS_CompileFile(cx, global, EDJS_CORE_FILE_LOC);
    if (rt_private->core_script == NULL) {
        *error_num = EDJSERR_COMPILING_CORE;
        goto error;
    }
    rt_private->core_script_obj = JS_NewScriptObject(cx, rt_private->core_script);
    if (rt_private->core_script_obj == NULL) {
        *error_num = EDJSERR_CREATING_SCRIPT_OBJ;
        JS_DestroyScript(cx, rt_private->core_script);
        goto error;
    }

    if (JS_FALSE == JS_AddNamedRootRT(rt, &(rt_private->core_script_obj), "core script object")) {
        *error_num = EDJSERR_ADD_ROOT;
        goto error;
    }

    //don't know why yet, but for some reason I have to execute the script in the original context
    jsval result = JSVAL_VOID;
    if (JS_FALSE == JS_ExecuteScript(cx, global, rt_private->core_script, &result)) {
        *error_num = EDJSERR_EXECUTING_SCRIPT;
        goto error;
    }

    JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_COMPILE_N_GO);
    if (NULL == settings_loc) {
        settings_script = JS_CompileFile(cx, global, EDJS_SETTINGS_FILE_LOC);
    }
    else {
        settings_script = JS_CompileFile(cx, global, settings_loc);
    }

    if (NULL == settings_script) {
        *error_num = EDJSERR_COMPILING_SETTINGS;
        goto error;
    }
    settings_script_obj = JS_NewScriptObject(cx, settings_script);
    if (NULL == settings_script_obj) {
        *error_num = EDJSERR_CREATING_SCRIPT_OBJ;
        JS_DestroyScript(cx, settings_script);
        goto error;
    }

    if (JS_FALSE == JS_ExecuteScript(cx, global, settings_script, &settings_val)) {
        *error_num = EDJSERR_EXECUTING_SCRIPT;
        return NULL;
    }

    if (JS_FALSE == JSVAL_IS_OBJECT(settings_val) || JS_TRUE == JSVAL_IS_NULL(settings_val)) {
        *error_num = EDJSERR_SETTINGS_NOT_OBJ;
        goto error;
    }

    settings_obj = JSVAL_TO_OBJECT(settings_val);
    rt_private->settings_obj = JS_NewObject(cx, NULL, NULL, NULL);

    if (NULL == rt_private->settings_obj) {
        *error_num = EDJSERR_CREATING_OBJECT;
        goto error;
    }

    if (JS_FALSE == JS_AddNamedRootRT(rt, &(rt_private->settings_obj), "Settings Object")) {
        *error_num = EDJSERR_ADD_ROOT;
        goto error;
    }

    if (JS_FALSE == JS_GetProperty(cx, settings_obj, "max_execution_time", &settings_val)) {
        *error_num = EDJSERR_GET_PROPERTY;
        goto error;
    }

    //if max execution time is undefined
    if (JS_TRUE == JSVAL_IS_VOID(settings_val) && 
        JS_FALSE == JS_NewNumberValue(cx, 0, &settings_val)) {
        *error_num = EDJSERR_CREATING_NUMBER;
        goto error;
    }

    if (JS_FALSE == JS_DefineProperty(cx, rt_private->settings_obj, "max_execution_time", 
                                      settings_val,
                                      NULL, (JSPropertyOp)edjs_SetMaxExecutionTime, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
        *error_num = EDJSERR_SET_PROPERTY;
        goto error;
    }

    if (JS_FALSE == JS_GetProperty(cx, settings_obj, "include_path", &settings_val)) {
        *error_num = EDJSERR_GET_PROPERTY;
        goto error;
    }

    //if include path is undefined
    if (JS_TRUE == JSVAL_IS_VOID(settings_val)) {
        array_obj = JS_NewArrayObject(cx, 0, NULL);
        if (NULL == array_obj) {
            *error_num = EDJSERR_CREATING_ARRAY;
            goto error;
        }

        temp_str = JS_NewStringCopyZ(cx, ".");
        if (NULL == temp_str) {
            *error_num = EDJSERR_CREATING_STRING;
            goto error;
        }

        temp_val = STRING_TO_JSVAL(temp_str);

        if (JS_FALSE == JS_SetElement(cx, array_obj, 0, &temp_val)) {
            *error_num = EDJSERR_SET_PROPERTY;
            goto error;
        }
    }

    if (JS_FALSE == JS_DefineProperty(cx, rt_private->settings_obj, "include_path", 
                                      settings_val, NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT)) {
        *error_num = EDJSERR_SET_PROPERTY;
        goto error;
    }

    JS_SetRuntimePrivate(rt, rt_private);
    JS_SetGCCallbackRT(rt, gc_callback);


    goto finish;
 error:
    if (EDJSERR_NOT_AN_ERROR == *error_num)
        *error_num = EDJSERR_UNKNOWN_ERROR;

 finish:
    JS_RemoveRoot(cx, &settings_script_obj);
    JS_RemoveRoot(cx, &array_obj);
    JS_RemoveRoot(cx, &temp_str);

    JS_EndRequest(cx);

    if (NULL != cx) {
        EDJS_CleanupContext(cx);
        cx = NULL;
    }

    if (EDJSERR_NOT_AN_ERROR != *error_num && NULL != rt) {
        EDJS_CleanupRuntime(rt);
        rt = NULL;
    }

    return rt;
}

JSContext *EDJS_CreateContext(JSRuntime *rt, edjs_private *p, JSBool edjs_standard_functions, EDJS_ErrNum *error_num) {
    JSContext *cx = NULL;
    jsval prop_val = JSVAL_VOID;
    JSObject  *global = NULL;

    *error_num = EDJSERR_NOT_AN_ERROR;

    if (JS_FALSE == p->random_seeded)
        srand((unsigned)time(NULL) * (unsigned)getpid());

    cx = JS_NewContext(rt, 8192);
    if (NULL == cx) {
        *error_num = EDJSERR_CREATING_CONTEXT;
        goto error;
    }

    JS_BeginRequest(cx);

    JS_SetGCZeal(cx, 2);

    p->branch_count = 0;
    p->max_execution_time = 0;
    p->max_execution_time_exceeded = JS_FALSE;
    p->start_time = clock();
    p->settings_obj = NULL;

    JS_SetContextPrivate(cx, (void *)p);


    JS_SetOptions(cx, JSOPTION_VAROBJFIX);
    JS_SetVersion(cx, 180);    

    JS_SetBranchCallback(cx, branch_callback);

    JS_SetErrorReporter(cx, edjs_ReportJSError);

    global = JS_NewObject(cx, &global_class, NULL, NULL);

    if (NULL == global) {
        *error_num = EDJSERR_CREATING_OBJECT;
        goto error;
    }

    if (JS_FALSE == JS_InitStandardClasses(cx, global)) { //sets global as the global object for cx
        *error_num = EDJSERR_INITING_STANDARD_CLASSES;
        goto error;
    }

    if (JS_TRUE == edjs_standard_functions && JS_FALSE == EDJS_InitFunctions(cx)) {
        *error_num = EDJSERR_INITING_EDJS_CLASSES;
        goto error;
    }

    
    if (NULL != p->rt_private->settings_obj) {
        if (JS_FALSE == edjs_Copy(cx, p->rt_private->settings_obj, 0, NULL, &prop_val)) {
            goto error;
        }
        p->settings_obj = JSVAL_TO_OBJECT(prop_val);
        if (JS_FALSE == JS_DefineProperty(cx, global, "EDJS", prop_val,
                                          NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
            *error_num = EDJSERR_SET_PROPERTY;
            goto error;
        }
        if (JS_FALSE == JS_GetProperty(cx, p->rt_private->settings_obj, "max_execution_time", &prop_val)) {
            *error_num = EDJSERR_GET_PROPERTY;
            goto error;
        }

        if (JS_FALSE == JS_ValueToECMAUint32(cx, prop_val, &(p->max_execution_time))) {
            *error_num = EDJSERR_CREATING_NUMBER;
            goto error;
        }
    }
    else {
        if (JS_FALSE == JS_DefineProperty(cx, global, "EDJS", OBJECT_TO_JSVAL(p->settings_obj),
                                          NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
            *error_num = EDJSERR_SET_PROPERTY;
            goto error;
        }
        p->max_execution_time = 30;
    }

    goto finish;
 error:

    if (EDJSERR_NOT_AN_ERROR == *error_num)
        *error_num = EDJSERR_UNKNOWN_ERROR;


 finish:
    if (NULL != cx) {
        JS_EndRequest(cx);

        if (EDJSERR_NOT_AN_ERROR != *error_num) {
            EDJS_CleanupContext(cx);
            cx = NULL;
        }
    }

    return cx;
}

void edjs_imp_module_destroy(JSContext *cx, edjs_imp_module *mod) {
    if (NULL != mod) {
        if (NULL != mod->file_path)
            EDJS_free(cx, mod->file_path);
        if (NULL != mod->dl_lib)
            dlclose(mod->dl_lib);
        EDJS_free(cx, mod);
    }
}

void EDJS_CleanupContext(JSContext *cx) {
    //edjs_private *p = (edjs_private *)JS_GetContextPrivate(cx);
    JS_SetContextPrivate(cx, NULL);
    JS_GC(cx);

    JS_DestroyContext(cx);

    //edjs_TreeDestroy(p->imported_modules, edjs_imp_module_destroy);
}

void EDJS_CleanupRuntime(JSRuntime *rt) {
    edjs_runtime_private *rt_private = NULL;
    rt_private = JS_GetRuntimePrivate(rt);

    JS_RemoveRootRT(rt, &(rt_private->core_script_obj));
    JS_RemoveRootRT(rt, &(rt_private->settings_obj));
    
    //FIX ME: free up imported_objects

    JS_DestroyRuntime(rt);

    pthread_mutex_destroy(&(rt_private->mutex));
}

JSBool EDJS_ExecuteFile(const char *file_name, JSRuntime *rt, void *private_data, EDJS_ErrNum *error_num, JSBool (*before_execute)(JSContext *),  JSBool (*after_execute)(JSContext *)) {
    JSBool ok = JS_TRUE;
    struct edjs_private p;
    JSContext *cx = NULL;
    JSObject *global = NULL;
    JSScript *script = NULL;
    JSObject *scriptObj = NULL;
    jsval result = JSVAL_VOID;
    JSBool request_started = JS_FALSE;

    //test file existence

    p.rt_private = JS_GetRuntimePrivate(rt);
    p.random_seeded = JS_FALSE;
    p.echo_function = NULL;
    p.imported_modules = NULL;
    p.private_data = private_data;

    cx = EDJS_CreateContext(rt, &p, JS_TRUE, error_num);
    if (cx == NULL) {
        goto error;
    }

    global = JS_GetGlobalObject(cx);

    before_execute(cx);

    JS_BeginRequest(cx);
    request_started = JS_TRUE;

    if (JS_FALSE == JS_AddRoot(cx, &scriptObj)) {
        *error_num = EDJSERR_ADD_ROOT;
        goto error;
    }

    script = JS_CompileFile(cx, global, file_name);
    if (NULL == script) {
        *error_num = EDJSERR_COMPILING_SCRIPT;
        goto error;
    }

    scriptObj = JS_NewScriptObject(cx, script);
    if (NULL == scriptObj) {
        *error_num = EDJSERR_CREATING_SCRIPT_OBJ;
        goto error;
    }

    if (JS_FALSE == JS_ExecuteScript(cx, global, script, &result)) {
        *error_num = EDJSERR_EXECUTING_SCRIPT;
        goto error;
    }
    if (p.max_execution_time_exceeded == JS_TRUE) {
        *error_num = EDJSERR_EXECUTION_TIME_EXCEEDED;
        goto error;
    }

    goto finish;
 error:
    ok = JS_FALSE;

 finish:

    after_execute(cx);

    if (NULL != cx) {
        if (JS_TRUE == request_started)
            JS_EndRequest(cx);
        EDJS_CleanupContext(cx);

        cx = NULL;
    }

    return ok;
}
