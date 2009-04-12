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

#include "jsapi.h"
#include "edjs.h"
#include "memcached.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "edjs_mcd.h"

static JSBool edjs_mcd_SetException(JSContext *cx, edjs_mcd_private *p, memcached_return mcd_code) {
    JSObject *error_obj = NULL;
    JSString *msg_str = NULL;
    jsval val = JSVAL_VOID;

    if (MEMCACHED_SUCCESS == mcd_code) //no error
        return JS_TRUE;

    if (JS_FALSE == JS_AddRoot(cx, &error_obj)) {
        goto finish;
    }

    if (JS_FALSE == JS_AddRoot(cx, &val)) {
        goto finish;
    }

    error_obj = JS_NewObject(cx, NULL, NULL, NULL);
    if (NULL == error_obj) {
        goto finish;
    }

    if (JS_FALSE == JS_NewNumberValue(cx, mcd_code, &val)) {
        goto finish;
    }

    if (JS_FALSE == JS_SetProperty(cx, error_obj, "code", &val)) {
        goto finish;
    }

    msg_str = JS_NewStringCopyZ(cx, memcached_strerror (p->servers, mcd_code));
    if (NULL == msg_str) {
        goto finish;
    }
    val = STRING_TO_JSVAL(msg_str);

    if (JS_FALSE == JS_SetProperty(cx, error_obj, "message", &val)) {
        goto finish;
    }

    JS_SetPendingException(cx, OBJECT_TO_JSVAL(error_obj));

 finish:
    JS_RemoveRoot(cx, &error_obj);
    JS_RemoveRoot(cx, &val);

    return JS_FALSE;
}

static JSBool edjs_mcd_Get(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjs_mcd_private *p = NULL;
    JSString *key_str = NULL;
    JSString *val_str = NULL;
    char *val = NULL;
    memcached_return error;
    uint32_t flags = 0;
    size_t val_len = 0;


    JS_BeginRequest(cx);

    if (argc < 1) {
        //report error
        goto error;
    }

    p = (edjs_mcd_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->servers) {
        //report error
        goto error;
    }

    key_str = JS_ValueToString(cx, argv[0]);
    if (NULL == key_str) {
        //report error
        goto error;
    }
    argv[0] = STRING_TO_JSVAL(key_str);


    val = memcached_get (p->servers,
                         JS_GetStringBytes(key_str), JS_GetStringLength(key_str), 
                         &val_len, &flags, &error);

    if (MEMCACHED_SUCCESS != error) {
        if (MEMCACHED_NOTFOUND != error) {
            EDJS_MCD_ERR(cx, p, error);
            goto error;
        }
        else
            *rval = JSVAL_VOID;
    }
    else {
        val_str = JS_NewStringCopyN(cx, val, val_len);
        if (NULL == val_str) {
            //report error
            goto error;
        }
        
        *rval = STRING_TO_JSVAL(val_str);
    }

    goto finish;
 error:
    ok = JS_FALSE;

    *rval = JSVAL_VOID;

 finish:
    JS_EndRequest(cx);
    if (NULL != val)
        free(val);

    return ok;
}

static JSBool edjs_mcd_ValueToTime(JSContext *cx, jsval val, time_t *t) {
    JSBool ok = JS_TRUE;
    jsdouble double_expiration;

    if (JS_TRUE == JSVAL_IS_OBJECT(val) && JS_FALSE == JSVAL_IS_NULL(val) &&
        0 == strncmp(JS_GET_CLASS(cx, JSVAL_TO_OBJECT(val))->name, "Date", 4)) {
        if (JS_FALSE == JS_ValueToNumber(cx, val, &double_expiration)) {
            //report error
            goto error;
        }
        double_expiration /= 1000.; //convert to seconds
        *t = (time_t)double_expiration - time(NULL);
    }
    else {
        if (JS_FALSE == JS_ValueToNumber(cx, val, &double_expiration)) {
            //report error
            goto error;
        }
        *t = (time_t)double_expiration;
    }

    goto finish;
 error:
    ok = JS_FALSE;
    *t = 0;

 finish:
    return ok;    
}

static JSBool edjs_mcd_Set(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    memcached_return result;
    edjs_mcd_private *p = NULL;
    JSString *key_str = NULL;
    JSString *val_str = NULL;
    time_t expiration = 0;

    if (argc < 2) {
        //report error
        goto error;
    }

    p = (edjs_mcd_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->servers) {
        //report error
        goto error;
    }

    key_str = JS_ValueToString(cx, argv[0]);
    if (NULL == key_str) {
        //report error
        goto error;
    }
    argv[0] = STRING_TO_JSVAL(key_str);

    if (argc > 2) {
        if (JS_FALSE == edjs_mcd_ValueToTime(cx, argv[2], &expiration)) {
            goto error;
        }

        if (0 > expiration) {
            argv[1] = INT_TO_JSVAL(0);
            return edjs_mcd_Delete(cx, obj, 2, argv, rval);
        }
    }

    val_str = JS_ValueToString(cx, argv[1]);
    if (NULL == val_str) {
        //report error
        goto error;
    }
    argv[1] = STRING_TO_JSVAL(val_str);
    

    result = memcached_set (p->servers,
                            JS_GetStringBytes(key_str), JS_GetStringLength(key_str), 
                            JS_GetStringBytes(val_str), JS_GetStringLength(val_str), 
                            expiration,
                            0);

    if (JS_FALSE == EDJS_MCD_ERR(cx, p, result))
        goto error;


    goto finish;
 error:
    ok = JS_FALSE;

 finish:
    *rval = OBJECT_TO_JSVAL(obj);

    return ok;

}

static JSBool edjs_mcd_Add(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjs_mcd_private *p = NULL;
    memcached_return result;
    JSString *key_str = NULL;
    JSString *val_str = NULL;
    time_t expiration = 0;


    if (argc < 2) {
        //report error
        goto error;
    }

    p = (edjs_mcd_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->servers) {
        //report error
        goto error;
    }

    key_str = JS_ValueToString(cx, argv[0]);
    if (NULL == key_str) {
        //report error
        goto error;
    }
    argv[0] = STRING_TO_JSVAL(key_str);

    if (argc > 2) {
        if (JS_FALSE == edjs_mcd_ValueToTime(cx, argv[2], &expiration)) {
            goto error;
        }

        if (0 > expiration) {
            argv[1] = INT_TO_JSVAL(0);
            return edjs_mcd_Delete(cx, obj, 2, argv, rval);
        }
    }

    val_str = JS_ValueToString(cx, argv[1]);
    if (NULL == val_str) {
        //report error
        goto error;
    }
    argv[1] = STRING_TO_JSVAL(val_str);

    result = memcached_add (p->servers,
                            JS_GetStringBytes(key_str), JS_GetStringLength(key_str), 
                            JS_GetStringBytes(val_str), JS_GetStringLength(val_str), 
                            expiration,
                            0);

    if (JS_FALSE == EDJS_MCD_ERR(cx, p, result))
        goto error;



    goto finish;
 error:
    ok = JS_FALSE;

 finish:
    *rval = OBJECT_TO_JSVAL(obj);

    return ok;

}

static JSBool edjs_mcd_Replace(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjs_mcd_private *p = NULL;
    memcached_return result;
    JSString *key_str = NULL;
    JSString *val_str = NULL;
    time_t expiration = 0;


    if (argc < 2) {
        //report error
        goto error;
    }

    p = (edjs_mcd_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->servers) {
        //report error
        goto error;
    }

    key_str = JS_ValueToString(cx, argv[0]);
    if (NULL == key_str) {
        //report error
        goto error;
    }
    argv[0] = STRING_TO_JSVAL(key_str);

    if (argc > 2) {
        if (JS_FALSE == edjs_mcd_ValueToTime(cx, argv[2], &expiration)) {
            goto error;
        }

        if (0 > expiration) {
            argv[1] = INT_TO_JSVAL(0);
            return edjs_mcd_Delete(cx, obj, 2, argv, rval);
        }
    }

    val_str = JS_ValueToString(cx, argv[1]);
    if (NULL == val_str) {
        //report error
        goto error;
    }
    argv[1] = STRING_TO_JSVAL(val_str);

    result = memcached_replace (p->servers,
                                JS_GetStringBytes(key_str), JS_GetStringLength(key_str), 
                                JS_GetStringBytes(val_str), JS_GetStringLength(val_str), 
                                expiration,
                                0);

    if (JS_FALSE == EDJS_MCD_ERR(cx, p, result))
        goto error;



    goto finish;
 error:
    ok = JS_FALSE;

 finish:
    *rval = OBJECT_TO_JSVAL(obj);

    return ok;

}

static JSBool edjs_mcd_Delete(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjs_mcd_private *p = NULL;
    memcached_return result;
    JSString *key_str = NULL;
    time_t expiration = 0;

    if (argc < 1) {
        //report error
        goto error;
    }

    p = (edjs_mcd_private *)JS_GetPrivate(cx, obj);

    if (NULL == p->servers) {
        //report error
        goto error;
    }

    key_str = JS_ValueToString(cx, argv[0]);
    if (NULL == key_str) {
        //report error
        goto error;
    }
    argv[0] = STRING_TO_JSVAL(key_str);

    if (argc > 1) {
        if (JS_FALSE == edjs_mcd_ValueToTime(cx, argv[2], &expiration)) {
            goto error;
        }

        if (0 > expiration) {
            expiration = 0;
        }
    }

    result = memcached_delete (p->servers,
                               JS_GetStringBytes(key_str), JS_GetStringLength(key_str), 
                               expiration);

    if (MEMCACHED_NOTFOUND != result && JS_FALSE == EDJS_MCD_ERR(cx, p, result)) {
            goto error;
    }

    goto finish;
 error:
    ok = JS_FALSE;

 finish:
    *rval = OBJECT_TO_JSVAL(obj);

    return ok;

}

static JSBool edjs_mcd_AddServers(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjs_mcd_private *p = NULL;
    int i_buff_len = 0;
    char *i_buff = NULL;
    jsval a_val = JSVAL_VOID;
    jsval server_val = JSVAL_VOID;
    jsval port_val = JSVAL_VOID;
    JSString *server_str = NULL;
    jsuint a_len;
    jsuint i;
    JSObject *arg_obj = NULL;
    memcached_server_st *new_servers = NULL;
    memcached_server_st *temp_new_servers = NULL;
    memcached_return mcd_error;
    const char *connect_buff = NULL;
    char *it = NULL;
    char *server_buff = NULL;
    uint32 port;
    int num_matches;

    if (argc < 1) {
        //report error
        goto error;
    }


    if (JS_FALSE == JSVAL_IS_OBJECT(argv[0]) || JS_TRUE == JSVAL_IS_NULL(argv[0]) ||
        JS_FALSE == JS_IsArrayObject(cx, JSVAL_TO_OBJECT(argv[0]))) {
        //report error
        goto error;
    }

    arg_obj = JSVAL_TO_OBJECT(argv[0]);

    if (JS_FALSE == JS_GetArrayLength(cx, arg_obj, &a_len)) {
        //EDJS_ERR(cx, EDJSERR_ARRAY_LEN);
        goto error;
    }

    i = a_len;
    i_buff_len = 1;
    do {
        i_buff_len++;
        i /= 10;
    } while (i > 0);
    i_buff = EDJS_malloc(cx, (i_buff_len) * sizeof(char));

    for (i = 0; i < a_len; i++) {
        snprintf(i_buff, i_buff_len, "%d", i);

        if (JS_FALSE == JS_GetProperty(cx, arg_obj, i_buff, &a_val)) {
            goto error;
        }

        if (JS_TRUE == JSVAL_IS_STRING(a_val)) {
            connect_buff = JS_GetStringBytes(JSVAL_TO_STRING(a_val));
            for (it = (char *)connect_buff; '\0' != *it && ':' != *it; it++);
            server_buff = (char *)EDJS_malloc(cx, (it - connect_buff + 1) * sizeof(char));
            if (NULL == server_buff) {
                goto error;
            }
            strncpy(server_buff, connect_buff, it - connect_buff);
            server_buff[it - connect_buff] = '\0';

            if ('\0' != *it) {
                it++;
                if (0 == sscanf(it, "%d\n", &port)) {
                    //report error
                    goto error;
                }
            }
            else
                port = 0;
        }
        else if (JS_TRUE == JSVAL_IS_OBJECT(a_val)) {
            if (JS_TRUE == JS_IsArrayObject(cx, JSVAL_TO_OBJECT(a_val))) {
                if (JS_FALSE == JS_GetProperty(cx, JSVAL_TO_OBJECT(a_val), "0", &server_val)) {
                    //report error
                    goto error;
                }
                if (JS_TRUE == JSVAL_IS_VOID(server_val)) {
                    //report error
                    goto error;
                }
                server_str = JS_ValueToString(cx, server_val);
                if (NULL == server_str) {
                    //report error
                    goto error;
                }
                *rval = STRING_TO_JSVAL(server_str); //root server_str

                server_buff = (char *)EDJS_malloc(cx, (strlen(JS_GetStringBytes(server_str))+1) * sizeof(char));
                if (NULL == server_buff) {
                    goto error;
                }
                strcpy(server_buff, JS_GetStringBytes(server_str));
            
                if (JS_FALSE == JS_GetProperty(cx, JSVAL_TO_OBJECT(a_val), "1", &port_val)) {
                    //report error
                    goto error;
                }

                if (JS_TRUE == JSVAL_IS_VOID(port_val)) {
                    port = 0;
                }
                else if (JS_FALSE == JS_ValueToECMAUint32(cx, port_val, &port)) {
                    //report error
                    goto error;
                }
            }
            else {
                if (JS_FALSE == JS_GetProperty(cx, JSVAL_TO_OBJECT(a_val), "hostname", &server_val)) {
                    //report error
                    goto error;
                }
                if (JS_TRUE == JSVAL_IS_VOID(server_val)) {
                    //report error
                    goto error;
                }
                server_str = JS_ValueToString(cx, server_val);
                if (NULL == server_str) {
                    //report error
                    goto error;
                }
                *rval = STRING_TO_JSVAL(server_str); //root server_str

                server_buff = (char *)EDJS_malloc(cx, (strlen(JS_GetStringBytes(server_str))+1) * sizeof(char));
                if (NULL == server_buff) {
                    goto error;
                }
                strcpy(server_buff, JS_GetStringBytes(server_str));
            
                if (JS_FALSE == JS_GetProperty(cx, JSVAL_TO_OBJECT(a_val), "port", &port_val)) {
                    //report error
                    goto error;
                }

                if (JS_TRUE == JSVAL_IS_VOID(port_val)) {
                    port = 0;
                }
                else if (JS_FALSE == JS_ValueToECMAUint32(cx, port_val, &port)) {
                    //report error
                    goto error;
                }
            }

        }
        else {
            //report error
            goto error;
        }

        temp_new_servers = memcached_server_list_append(new_servers, 
                                                        server_buff,
                                                        port, 
                                                        &mcd_error);

        if (NULL == temp_new_servers) {
            //report error
            goto error;
        }
        new_servers = temp_new_servers;

        EDJS_free(cx, server_buff);
        server_buff = NULL;
    }

    p = (edjs_mcd_private *)JS_GetPrivate(cx, obj);
    if (NULL == p->servers) {
        //report error
        goto error;
    }


    //FIX ME: check for error
    memcached_server_push (p->servers, new_servers);

    goto finish;
 error:
    ok = JS_FALSE;

    if (NULL != new_servers) {
        memcached_server_list_free (new_servers);
        new_servers = NULL;
    }


 finish:
    if (NULL != server_buff) {
        EDJS_free(cx, server_buff);
        server_buff = NULL;
    }
    
    *rval = OBJECT_TO_JSVAL(obj);

    return ok;
}

static JSBool edjs_mcd_GetServers(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjs_mcd_private *p = NULL;
    JSObject *ret_obj = NULL;
    JSObject *element_obj = NULL;
    JSString *host_str = NULL;
    uint32_t i = 0;
    
    p = (edjs_mcd_private *)JS_GetPrivate(cx, obj);
    if (NULL == p->servers) {
        //report error
        goto error;
    }

    ret_obj = JS_NewArrayObject(cx, 0, NULL);
    if (NULL == ret_obj) {
        //report error
        goto error;
    }

    *rval = OBJECT_TO_JSVAL(ret_obj);
    for (i = 0; i < p->servers->number_of_hosts; i++) {
        element_obj = JS_NewObject(cx, NULL, NULL, NULL);
        if (NULL == element_obj) {
            //report error
            goto error;
        }
        argv[argc] = OBJECT_TO_JSVAL(element_obj);
        host_str = JS_NewStringCopyZ(cx, p->servers->hosts[i].hostname);
        if (NULL == host_str) {
            //report error
            goto error;
        }
        argv[argc+1] = STRING_TO_JSVAL(host_str);

        if (JS_FALSE == JS_SetProperty(cx, element_obj, "hostname", &(argv[argc+1]))) {
            //report error
            goto error;
        }

        if (JS_FALSE == JS_NewNumberValue(cx, p->servers->hosts[i].port, &(argv[argc+1]))) {
            //report error
            goto error;
        }

        if (JS_FALSE == JS_SetProperty(cx, element_obj, "port", &(argv[argc+1]))) {
            //report error
            goto error;
        }

        if (JS_FALSE == JS_SetElement(cx, ret_obj, i, &(argv[argc]))) {
            //report error
            goto error;
        }
    }

    goto finish;
 error:
    ok = JS_FALSE;
    *rval = JSVAL_VOID;

 finish:

    return ok;

}

static JSBool edjs_mcd_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    JSObject *proto_obj = NULL;
    JSObject *ret_obj = NULL;
    edjs_mcd_private *p = NULL;


    JSFunctionSpec my_methods[] = {
        JS_FS("get",        edjs_mcd_Get,        1, JSPROP_ENUMERATE, 0),
        JS_FS("set",        edjs_mcd_Set,        2, JSPROP_ENUMERATE, 0),
        JS_FS("add",        edjs_mcd_Add,        2, JSPROP_ENUMERATE, 0),
        JS_FS("replace",    edjs_mcd_Replace,    2, JSPROP_ENUMERATE, 0),
        JS_FS("delete",     edjs_mcd_Delete,     1, JSPROP_ENUMERATE, 0),
        JS_FS("addServers", edjs_mcd_AddServers, 1, JSPROP_ENUMERATE, 0),
        JS_FS("getServers", edjs_mcd_GetServers, 1, JSPROP_ENUMERATE, 2),
        JS_FS_END
    };


    if (JS_FALSE == JS_AddRoot(cx, &ret_obj)) {
        //report error
        goto error;
    }

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

    ret_obj = JS_NewObject(cx, &edjs_mcd_class, proto_obj, NULL);//JS_ConstructObjectWithArguments(cx, &edjsdb_class, NULL, NULL, argc, argv);

    if (NULL == ret_obj) {
        //throw error
        goto error;
    }

    p = (edjs_mcd_private *)EDJS_malloc(cx, sizeof(edjs_mcd_private));
    if (NULL == p) {
        goto error;
    }

    p->servers = memcached_create(NULL);
    if (NULL == p->servers) {
        //report error
        goto error;
    }

    if (JS_FALSE == JS_SetPrivate(cx, ret_obj, p)) {
        //report error
        goto error;
    }

    if (argc > 0 && JS_FALSE == edjs_mcd_AddServers(cx, ret_obj, argc, argv, rval)) {
        goto error;
    }
    *rval = OBJECT_TO_JSVAL(ret_obj);


    goto finish;
 error:
    ok = JS_FALSE;
    if (NULL != p) {
        if (NULL != p->servers) {
            //memcached_server_list_free (srvr_st);
            memcached_free(p->servers);
        }

        EDJS_free(cx, p);
        p = NULL;
        JS_SetPrivate(cx, ret_obj, NULL);
    }

    *rval = JSVAL_VOID;

 finish:
    JS_RemoveRoot(cx, &ret_obj);

    return ok;
}

static void edjs_mcd_Finalize(JSContext *cx, JSObject *obj) {
    edjs_mcd_private *p = NULL;

    p = (edjs_mcd_private *)JS_GetPrivate(cx, obj);

    if (NULL != p) {
        if (NULL != p->servers) {
            //memcached_server_list_free (srvr_st);
            memcached_free(p->servers);
        }

        EDJS_free(cx, p);
    }
}

JSBool edjs_init(JSContext *cx, JSObject **ret) {
    JSBool ok = JS_TRUE;
    JSFunction *func = NULL;
    func = JS_NewFunction(cx, edjs_mcd_Construct,
                          0, 0, NULL, NULL);

    *ret = (JSObject *)func;//JS_GetFunctionObject(func);
    //*ret = OBJECT_TO_JSVAL(func);

    JSConstDoubleSpec mcd_constants[] = {
        MCD_PROP_CONSTANT("SUCCESS", MEMCACHED_SUCCESS),
        MCD_PROP_CONSTANT("FAILURE", MEMCACHED_FAILURE),
        MCD_PROP_CONSTANT("HOST_LOOKUP_FAILURE", MEMCACHED_HOST_LOOKUP_FAILURE),
        MCD_PROP_CONSTANT("CONNECTION_FAILURE", MEMCACHED_CONNECTION_FAILURE),
        MCD_PROP_CONSTANT("CONNECTION_BIND_FAILURE", MEMCACHED_CONNECTION_BIND_FAILURE),
        MCD_PROP_CONSTANT("WRITE_FAILURE", MEMCACHED_WRITE_FAILURE),
        MCD_PROP_CONSTANT("READ_FAILURE", MEMCACHED_READ_FAILURE),
        MCD_PROP_CONSTANT("UNKNOWN_READ_FAILURE", MEMCACHED_UNKNOWN_READ_FAILURE),
        MCD_PROP_CONSTANT("PROTOCOL_ERROR", MEMCACHED_PROTOCOL_ERROR),
        MCD_PROP_CONSTANT("CLIENT_ERROR", MEMCACHED_CLIENT_ERROR),
        MCD_PROP_CONSTANT("SERVER_ERROR", MEMCACHED_SERVER_ERROR),
        MCD_PROP_CONSTANT("CONNECTION_SOCKET_CREATE_FAILURE", MEMCACHED_CONNECTION_SOCKET_CREATE_FAILURE),
        MCD_PROP_CONSTANT("DATA_EXISTS", MEMCACHED_DATA_EXISTS),
        MCD_PROP_CONSTANT("DATA_DOES_NOT_EXIST", MEMCACHED_DATA_DOES_NOT_EXIST),
        MCD_PROP_CONSTANT("NOTSTORED", MEMCACHED_NOTSTORED),
        MCD_PROP_CONSTANT("STORED", MEMCACHED_STORED),
        MCD_PROP_CONSTANT("NOTFOUND", MEMCACHED_NOTFOUND),
        MCD_PROP_CONSTANT("MEMORY_ALLOCATION_FAILURE", MEMCACHED_MEMORY_ALLOCATION_FAILURE),
        MCD_PROP_CONSTANT("PARTIAL_READ", MEMCACHED_PARTIAL_READ),
        MCD_PROP_CONSTANT("SOME_ERRORS", MEMCACHED_SOME_ERRORS),
        MCD_PROP_CONSTANT("NO_SERVERS", MEMCACHED_NO_SERVERS),
        MCD_PROP_CONSTANT("END", MEMCACHED_END),
        MCD_PROP_CONSTANT("DELETED", MEMCACHED_DELETED),
        MCD_PROP_CONSTANT("VALUE", MEMCACHED_VALUE),
        MCD_PROP_CONSTANT("STAT", MEMCACHED_STAT),
        MCD_PROP_CONSTANT("ERRNO", MEMCACHED_ERRNO),
        MCD_PROP_CONSTANT("FAIL_UNIX_SOCKET", MEMCACHED_FAIL_UNIX_SOCKET),
        MCD_PROP_CONSTANT("NOT_SUPPORTED", MEMCACHED_NOT_SUPPORTED),
        MCD_PROP_CONSTANT("FETCH_NOTFINISHED", MEMCACHED_FETCH_NOTFINISHED),
        MCD_PROP_CONSTANT("TIMEOUT", MEMCACHED_TIMEOUT),
        MCD_PROP_CONSTANT("BUFFERED", MEMCACHED_BUFFERED),
        MCD_PROP_CONSTANT("BAD_KEY_PROVIDED", MEMCACHED_BAD_KEY_PROVIDED),
        MCD_PROP_CONSTANT("INVALID_HOST_PROTOCOL", MEMCACHED_INVALID_HOST_PROTOCOL),
        MCD_PROP_CONSTANT("MAXIMUM_RETURN", MEMCACHED_MAXIMUM_RETURN),
        MCD_PROP_CONSTANT_END
    };


    if (JS_FALSE == JS_DefineConstDoubles(cx, *ret, mcd_constants)) {
        //report error
        goto error;
    }



    goto finish;
 error:

    ok = JS_FALSE;
 finish:

    return ok;
}
