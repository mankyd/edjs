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

#include "httpd.h"

#include "jsapi.h"

#include "apreq_param.h"
#include "apreq_cookie.h"
#include "apreq_module.h"
#include "apreq_util.h"
#include "apreq_parser.h"
#include "apreq_util.h"
#include "apreq_error.h"
#include "util_script.h"

#include "edjs.h"
#include "mod_edjs.h"
#include "reqdata.h"
#include "jsfile.h"

static JSClass edjs_CookieClass = {
    "EDJSCookie", 0,
    JS_PropertyStub, (JSPropertyOp)edjs_CookieDeleter, JS_PropertyStub, (JSPropertyOp)edjs_CookieSetter,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    NULL //JSCLASS_NO_OPTIONAL_MEMBERS
};

JSBool EDJS_AssignCookieData(JSContext *cx, request_rec *r) {
    JSBool ok = JS_TRUE;
    char *query_arg = NULL;
    char *query_var = NULL;
    char *query_val = NULL;
    size_t var_len = 0;
    JSBool is_array = JS_FALSE;
    const char *cookie_header;
    const char **data;
    char *temp;
    apr_pool_t *pool;
    edjs_request_data d;

    apr_pool_create_ex(&pool,r->pool, NULL, NULL);

    JS_BeginRequest(cx);
    d.r = r;
    d.cx = cx;
    d.read_only = JS_FALSE;
    d.pool = pool;
    d.container = JS_DefineObject(cx, JS_GetGlobalObject(cx), "COOKIE", &edjs_CookieClass, NULL,
                                  JSPROP_ENUMERATE | JSPROP_PERMANENT);

    JS_EndRequest(cx);

    if (NULL == d.container) {
        //report error
        goto error;
    }

    cookie_header = apr_table_get(r->headers_in, "Cookie");
    if (NULL == cookie_header) {
        goto finish;
    }

    temp = (char *)apr_palloc(pool, (strlen(cookie_header)+1) * sizeof(char));
    if (NULL == temp) {
        //report error;
        goto error;
    }

    data = (const char**)&temp;
    strcpy((char *)*data, cookie_header);

    apr_table_t *t = apr_table_make(pool, 1);
    if (NULL == t) {
        //report error;
        goto error;
    }

    while ('\0' != **data) {
        query_arg = ap_get_token(pool, (const char**)data, 0);
        if ('\0' != **data)
            *data = (*data)+1;
        query_var = ap_getword(pool, (const char**)&query_arg, '=');
        //fix me: check for unescape error
        ap_unescape_url(query_var);
        var_len = strlen(query_var);
        //only support array setting for values that have a variable name other than "[]"
        if (2 < var_len && '[' == query_var[var_len-2] && ']' == query_var[var_len-1]) {
            query_var[var_len-2] = '\0';
            is_array = JS_TRUE;
        }
        else
            is_array = JS_FALSE;

        query_val = query_arg; 
        //fix me: check for unescape error
        ap_unescape_url(query_arg);
        if (JS_TRUE == is_array)
            apr_table_addn(t, query_var, query_val);
        else
            apr_table_setn(t, query_var, query_val);
    }


    if (!apr_table_do(edjs_LoadParamsCallback, &d, t, NULL)) {
        //report error
        goto error;
    }
    

    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    apr_pool_destroy (pool);

    return ok;
}

JSBool EDJS_AssignEnvData(JSContext *cx, request_rec *r) {
    JSBool ok = JS_TRUE;

    apr_pool_t *pool = NULL;
    edjs_request_data d;

    apr_pool_create_ex(&pool,r->pool, NULL, NULL);

    JS_BeginRequest(cx);
    d.r = r;
    d.cx = cx;
    d.read_only = JS_TRUE;
    d.pool = pool;
    d.container = JS_DefineObject(cx, JS_GetGlobalObject(cx), "ENV", NULL, NULL,
                                  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);

    JS_EndRequest(cx);


    if (NULL == d.container) {
        //report error
        goto error;
    }

    ap_add_common_vars(r);
    ap_add_cgi_vars(r);

    if (!apr_table_do(edjs_LoadParamsCallback, &d, r->subprocess_env, NULL))
        goto error;

    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    apr_pool_destroy (pool);

    return ok;
}

JSBool EDJS_AssignGetData(JSContext *cx, request_rec *r) {
    JSBool ok = JS_TRUE;

    edjs_request_data d;
    const char **data = NULL;
    apr_table_t *t = NULL;
    apr_pool_t *pool = NULL;

    apr_pool_create_ex(&pool, r->pool, NULL, NULL);

    JS_BeginRequest(cx);

    d.r = r;
    d.cx = cx;
    d.read_only = JS_TRUE;
    d.pool = pool;
    d.container = JS_DefineObject(cx, JS_GetGlobalObject(cx), "GET", NULL, NULL,
                                  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);

    JS_EndRequest(cx);

    if (r->args) {
        data = (const char**)&(r->args);

        t = apr_table_make(pool, 1);
        if (NULL == t) {
            //report error
            goto error;
        }
    
        if (APR_SUCCESS != apreq_parse_query_string(pool, t, r->args)) {
            //report error
            goto error;
        }
        if (!apr_table_do(edjs_LoadApreqParamsCallback, &d, t, NULL))
            goto error;
    }
    
    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    apr_pool_destroy (pool);
    return ok;
}

JSBool EDJS_AssignPostData(JSContext *cx, request_rec *r) {
    JSBool ok = JS_TRUE;

    apr_pool_t *pool = NULL;
    apr_bucket_alloc_t *ba = NULL;
    apr_bucket_brigade *bb = NULL;
    apr_bucket *b = NULL;
    apreq_parser_t *parser = NULL;
    const apreq_parser_function_t *parser_func = NULL;
    apr_table_t *body = NULL;
    char *buffer = NULL;
    const char *type = NULL;
    int rc;
    int rsize;
    int len_read;
    int rpos=0;
    long length = 0;

    //fix me: check for errors

    apr_pool_create_ex(&pool,r->pool, NULL, NULL);

    JS_BeginRequest(cx);

    edjs_request_data d;
    d.r =r;
    d.cx = cx;
    d.read_only = JS_TRUE;
    d.pool = pool;
    d.container = JS_DefineObject(cx, JS_GetGlobalObject(cx), "POST", NULL, NULL,
                                  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);

    JS_EndRequest(cx);

    if (NULL == d.container) {
        //report error
        goto error;
    }

    if (M_POST != r->method_number) {
        goto finish;
    }

    apreq_initialize(pool);

    type = apr_table_get(r->headers_in, "Content-Type");
    if (NULL == type || NULL == (parser_func = apreq_parser(type))) {
        goto finish;
    }

    rpos=0;
    length = r->remaining;

    if (OK != (rc = ap_setup_client_block(r, REQUEST_CHUNKED_ERROR))) {
        //report error
        goto finish;
    }
    if (!ap_should_client_block(r)) {
        //report error
        goto finish;
    }

    ba = apr_bucket_alloc_create(pool);
    bb = apr_brigade_create(pool, ba);
    buffer = (char *)apr_pcalloc(pool, HUGE_STRING_LEN);
    while ((len_read = ap_get_client_block(r, buffer, HUGE_STRING_LEN)) > 0) { 
        if ((rpos + len_read) > length) {
            rsize = length - rpos;
        }
        else {
            rsize = len_read;
        }
        b = apr_bucket_pool_create((const char*)buffer,len_read, pool,ba);
        APR_BRIGADE_INSERT_TAIL(bb, b);
        buffer = (char *)apr_pcalloc(pool, HUGE_STRING_LEN);
        rpos += rsize;
    }

    APR_BRIGADE_INSERT_TAIL(bb, apr_bucket_eos_create(ba));

    parser = apreq_parser_make(pool, ba, type, parser_func,
                               1024*1024, NULL, NULL, NULL);

    if (NULL == parser) {
        //report error
        goto error;
    }

    body = apr_table_make(pool, APREQ_DEFAULT_NELTS);
    if (NULL == body) {
        //report error
        goto error;
    }

    if (APR_SUCCESS != apreq_parser_run(parser, body, bb)) {
        //report error
        goto error;
    }

    if (!apr_table_do(edjs_LoadApreqParamsCallback, &d, body, NULL))
        goto error;

    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    apr_pool_destroy (pool);
    return ok;
}



static JSBool edjs_AssignJsValToReqData(edjs_request_data *d, const char *key, jsval val) {
    //supports syntax along the lines of: key[subkey][][][sub-subkey][]
    JSBool ok = JS_TRUE;
    char *key_copy = NULL;
    char *it = NULL;
    char *it2 = NULL;
    const char *token = NULL;
    const char *next_token = NULL;
    JSObject *container = d->container;
    JSObject *sub_container = NULL;
    jsval prop_val = JSVAL_VOID;
    JSBool first = JS_TRUE;
    jsuint len = 0;
    uintN flags = JSPROP_ENUMERATE | JSPROP_PERMANENT | (JS_TRUE == d->read_only ? JSPROP_READONLY : 0);

    JS_BeginRequest(d->cx);
    key_copy = (char *)JS_malloc(d->cx, (strlen(key)+1) * sizeof(char));
    if (NULL == key_copy) {
        //report error
        goto error;
    }
    strcpy(key_copy, key);

    it = key_copy;
    token = key_copy;

    if ('\0' == *it) { //shouldn't happen, but better safe than sorry
        //report error
        goto error;
    }

    //first we parse out the top level name of the variable.
    do {
        it += sizeof(char);
    } while ('[' != *it && '\0' != *it);

    
    while ('[' == *it) {
        sub_container = NULL;
        if (JS_TRUE == first) {
            *it = '\0';
            first = JS_FALSE;
        }
        else {
            *(it-sizeof(char)) = '\0';
        }

        if (']' == *(it + sizeof(char))) { //array syntax
            //tokenize our string copy
            //need to act differently for the first key than all the rest

            next_token = it + sizeof(char); //this is where our token will start next
            it += (2*sizeof(char)); //advance to the next subkey

            sub_container = JS_NewArrayObject(d->cx, 0, NULL);
            if (NULL == sub_container) {
                //report error
                goto error;
            }
        }
        else {
            it2 = it; //make sure that there is a closing brace
            do {
                it2 += sizeof(char);
            } while (']' != *it2 && '\0' != *it2);

            if ('\0' != *it2) { //we found a closing bracket
                //tokenize our string copy
                //need to act differently for the first key than all the rest
                next_token = it + sizeof(char); //this is where our token will start next
                it = it2 + sizeof(char); //advance to the next subkey

                sub_container = JS_NewObject(d->cx, NULL, NULL, NULL);
                if (NULL == sub_container) {
                    //report error
                    goto error;
                }
            }
        }

        if (NULL != sub_container) {
            if (JS_TRUE == JS_IsArrayObject(d->cx, container)) {
                if (JS_FALSE == JS_GetArrayLength(d->cx, container, &len)) {
                    //report error
                    goto error;
                }

                if (JS_FALSE == JS_DefineElement(d->cx, container, len, OBJECT_TO_JSVAL(sub_container), 
                                                 NULL, NULL, flags)) {
                    //report error
                    goto error;
                }
            }
            else {
                if (JS_FALSE == JS_GetProperty(d->cx, container, token, &prop_val)) {
                    //report error
                    goto error;
                }

                if (JS_TRUE == JSVAL_IS_VOID(prop_val)) {
                    if (JS_FALSE == JS_DefineProperty(d->cx, container, token, OBJECT_TO_JSVAL(sub_container), 
                                                      NULL, NULL, flags)) {
                        //report error
                        goto error;
                    }
                }
                else {
                    sub_container = JSVAL_TO_OBJECT(prop_val); //the container already existed
                }
            }

            container = sub_container;
            token = next_token;
        }
    }

    if (JS_TRUE == first) {
        *it = '\0';
        first = JS_FALSE;
    }
    else {
        *(it-sizeof(char)) = '\0';
    }


    if (JS_TRUE == JS_IsArrayObject(d->cx, container)) {
        if (JS_FALSE == JS_GetArrayLength(d->cx, container, &len)) {
            //report error
            goto error;
        }
        
        if (JS_FALSE == JS_DefineElement(d->cx, container, len, val, 
                                         NULL, NULL, flags)) {
            //report error
            goto error;
        }
    }
    else {
        if (JS_FALSE == JS_DefineProperty(d->cx, container, token, val,
                                          NULL, NULL, flags)) {
            //report error
            goto error;
        }
    }
    
    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    if (NULL != key_copy)
        JS_free(d->cx, key_copy);
    JS_EndRequest(d->cx);

    return ok;
}

static int edjs_LoadParamsCallback(void *p, const char *key, const char *value) {
    edjs_request_data *d = (edjs_request_data *)p;

    JSString *str = NULL;
    jsval tjsval = JSVAL_VOID;

    JS_BeginRequest(d->cx);
    str = JS_NewStringCopyZ(d->cx, value);
    JS_EndRequest(d->cx);

    if (str == NULL)
        return 0;
    
    tjsval = STRING_TO_JSVAL(str);

    if (JS_FALSE == edjs_AssignJsValToReqData(d, key, tjsval))
        return 0;
    return 1;
}

static int edjs_LoadApreqParamsCallback(void *p, const char *key, const char *value) {
    int ret = 1;

    edjs_request_data *d = (edjs_request_data *)p;
    apreq_param_t *param = NULL;
    JSString *str = NULL;
    JSObject *fobj = NULL;
    apr_file_t *f = NULL;
    apr_pool_t *pool = d->pool;
    jsval tval = JSVAL_VOID;
    apr_off_t len = 0;
    apr_finfo_t finfo;

    if (JS_FALSE == JS_AddRoot(d->cx, &str)) {
        //report error
        goto error;
    }

    if (JS_FALSE == JS_AddRoot(d->cx, &fobj)) {
        //report error
        goto error;
    }

    if (JS_FALSE == JS_AddRoot(d->cx, &tval)) {
        //report error
        goto error;
    }

    param = apreq_value_to_param(value);
    if (NULL == param) {//shouldn't theoretically happen
        //report error
        goto error;
    }

    if (NULL != param->upload) {
        if (APR_SUCCESS != apreq_file_mktemp(&f, pool, NULL)) {
            //report error
            goto error;
        }

        if (APR_SUCCESS != apreq_brigade_fwrite(f, &len, param->upload)) {
            //report error
            goto error;
        }
        apr_file_info_get(&finfo, APR_FINFO_NORM, f);

        str = JS_NewStringCopyZ(d->cx, (char*)finfo.fname);
        if (NULL == str) {
            //report error
            goto error;
        }

        tval = STRING_TO_JSVAL(str);
        fobj = JS_ConstructObjectWithArguments(d->cx, &js_FileClass,
                                               NULL, NULL, 1, &tval);

        if (NULL == fobj) {
            //report error
            goto error;
        }

        if (JS_FALSE == edjs_AssignJsValToReqData(d, key, OBJECT_TO_JSVAL(fobj))) {
            goto error;
        }

        apr_file_close(f);
        f = NULL;
    }
    else {
        str = JS_NewStringCopyZ(d->cx, value);
        if (NULL == str) {
            //report error
            goto error;
        }
        
        if (JS_FALSE == edjs_AssignJsValToReqData(d, key, STRING_TO_JSVAL(str))) {
            goto error;
        }
    }

    goto finish;
 error:
    ret = 0;
 finish:
    if (NULL != f) {
        apr_file_close(f);
        f = NULL;
    }

    JS_RemoveRoot(d->cx, &str);
    JS_RemoveRoot(d->cx, &fobj);
    JS_RemoveRoot(d->cx, &tval);
    return ret;
}

static JSBool edjs_CookieHelper(JSContext *cx, const char* key, JSObject *c_obj) {
    //apreq_cookie_t *c;

    edjs_private *p = (edjs_private *)JS_GetContextPrivate(cx);
    request_rec *r = ((mod_edjs_private *)p->private_data)->r;

    jsval value = JSVAL_VOID;
    jsval max_age = JSVAL_VOID;
    jsval secure = JSVAL_VOID;
    jsval domain = JSVAL_VOID;
    jsval path = JSVAL_VOID;
    jsval port = JSVAL_VOID;
    jsval comment = JSVAL_VOID;
    jsval commentURL = JSVAL_VOID;
    JSBool sec;
    uint32 max_age_int = 0;
    uint32 max_age_tmp_int = 0;

    if (JS_FALSE == JS_GetProperty(cx, c_obj, "value", &value))
        return JS_FALSE;

    //null values become empty strings
    if (JSVAL_IS_NULL(value))
        value = JS_GetEmptyStringValue(cx);
    
    //undefined values erase the cookie
    if (JSVAL_IS_VOID(value)) {
        if (JS_FALSE == JS_NewNumberValue(cx, 0, &max_age)) {
            //report error
            return JS_FALSE;
        }
    }
    else if (JS_FALSE == JS_GetProperty(cx, c_obj, "max_age", &max_age))
        return JS_FALSE;

    if (JS_FALSE == JS_GetProperty(cx, c_obj, "secure", &secure))
        return JS_FALSE;

    if (JS_FALSE == JS_ValueToBoolean(cx, secure, &sec))
        return JS_FALSE;

    if (JS_FALSE == JS_GetProperty(cx, c_obj, "path", &path))
        return JS_FALSE;
 
    if (JS_FALSE == JS_GetProperty(cx, c_obj, "domain", &domain))
        return JS_FALSE;

    if (JS_FALSE == JS_GetProperty(cx, c_obj, "port", &port))
        return JS_FALSE;

    if (JS_FALSE == JS_GetProperty(cx, c_obj, "comment", &comment))
        return JS_FALSE;

    if (JS_FALSE == JS_GetProperty(cx, c_obj, "commentURL", &commentURL))
        return JS_FALSE;


    size_t cookie_len =  strlen(key) + 2; //1 for \0 and =;
    if (cookie_len - 2 != strcspn (key, EDJS_REQDATA_SPECIAL_COOKIE_CHARS)) {
        //report error
        return JS_FALSE;
    }

    if (JS_FALSE == JSVAL_IS_VOID(max_age)) {        
        if (JS_FALSE == JS_ValueToECMAUint32(cx, max_age, &max_age_int)) {
            return JS_FALSE;
        }
        max_age_tmp_int = max_age_int;
        do { 
            cookie_len++;
        } while (0 != (max_age_tmp_int /= 10));
        cookie_len += 9; //for ";Max-Age="
    }
    else {
        //need to set this to non-zero value
        max_age_int = 1;
    }


    const char *val = JS_GetStringBytes(JS_ValueToString(cx, value));
    const char *it0 = val;
    const char *it1 = NULL;

    if (0 != max_age_int) {
        while ('\0' != *it0) {
            for (it1 = EDJS_REQDATA_SPECIAL_COOKIE_CHARS; '\0' != *it1; it1 += sizeof(char)) {
                if (*it0 == *it1) {
                    cookie_len += 2; //going to encode character as %##
                    break;
                }
            }
            cookie_len++;
            it0 += sizeof(char);
        }
    }

    if (JS_FALSE == JSVAL_IS_VOID(path)) {
        cookie_len += 6; // for ";Path="
        cookie_len += strlen(JS_GetStringBytes(JS_ValueToString(cx, path)));
    }
    if (JS_FALSE == JSVAL_IS_VOID(domain)) {
        cookie_len += 8; // for ";Domain="
        cookie_len += strlen(JS_GetStringBytes(JS_ValueToString(cx, domain)));
    }
    if (JS_TRUE == sec)
        cookie_len += 6; //for  "Secure"

    cookie_len += 10; //for ";Version=1"

    char *cookie_char = (char *)apr_palloc(r->pool, sizeof(char) * cookie_len);
    strcpy(cookie_char, key);
    strcat(cookie_char, "=");
    char *cookie_end_char = cookie_char+strlen(cookie_char);
    if (0 != max_age_int) {
        it0 = val;
        it1 = NULL;
        while ('\0' != *it0) {
            for (it1 = EDJS_REQDATA_SPECIAL_COOKIE_CHARS; '\0' != *it1; it1 += sizeof(char)) {
                if (*it0 == *it1) {
                    sprintf (cookie_end_char, "%%%X", *it0);
                    cookie_end_char += 3 * sizeof(char);
                    break;
                }
            }
            if ('\0' == *it1) {
                *cookie_end_char = *it0;
                cookie_end_char += sizeof(char);
            }
            it0 += sizeof(char);
        }
    }
    *cookie_end_char = '\0';
    if (JS_FALSE == JSVAL_IS_VOID(max_age))
        sprintf (cookie_end_char, ";Max-Age=%d", max_age_int);
    if (JS_FALSE == JSVAL_IS_VOID(path)) {
        strcat(cookie_char, ";Path=");
        strcat(cookie_char, JS_GetStringBytes(JS_ValueToString(cx, path)));
    }
    if (JS_FALSE == JSVAL_IS_VOID(domain)) {
        strcat(cookie_char, ";Domain=");
        strcat(cookie_char, JS_GetStringBytes(JS_ValueToString(cx, domain)));
    }
    if (JS_TRUE == sec)
        strcat(cookie_char, ";Secure");

    strcat(cookie_char, ";Version=1");

    apr_table_add(r->headers_out, "Set-Cookie", cookie_char);

    /*
    c = apreq_cookie_make(r->pool, key, strlen(key), val, strlen(val));

    if (JSVAL_IS_VOID(expires) == JS_FALSE)
        apreq_cookie_expires (c, JS_GetStringBytes(JS_ValueToString(cx, expires)));

    if (sec == JS_TRUE)
        apreq_cookie_secure_on(c);
    else
        apreq_cookie_secure_off(c);


    if (JSVAL_IS_VOID(path) == JS_FALSE)
        c->path = JS_GetStringBytes(JS_ValueToString(cx, path));
    if (JSVAL_IS_VOID(domain) == JS_FALSE)
        c->domain = JS_GetStringBytes(JS_ValueToString(cx, domain));


    if (JSVAL_IS_VOID(port) == JS_FALSE) {
        apreq_cookie_version_set(c, 1);
        c->port = JS_GetStringBytes(JS_ValueToString(cx, port));
    }
    if (JSVAL_IS_VOID(comment) == JS_FALSE) {
        apreq_cookie_version_set(c, 1);
        c->comment = JS_GetStringBytes(JS_ValueToString(cx, comment));
    }
    if (JSVAL_IS_VOID(commentURL) == JS_FALSE) {
        apreq_cookie_version_set(c, 1); 
        c->commentURL = JS_GetStringBytes(JS_ValueToString(cx, commentURL));
    }
    
    apr_table_add(r->headers_out, "Set-Cookie", apreq_cookie_as_string(c, r->pool));
    */

    return JS_TRUE;
}

static JSBool edjs_CookieDeleter(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
    JSObject *c_obj = NULL;

    //prevent value from actually being deleted - cookies can't change value mid execution
    //note: despite what the documentation says, this doesn't work for some dumb reason
    *vp = JSVAL_FALSE;

    c_obj = JS_NewObject(cx, NULL, NULL, NULL);
    if (c_obj == NULL)
        return JS_FALSE;

    const char *key = JS_GetStringBytes(JS_ValueToString(cx, id));

    return edjs_CookieHelper(cx, key, c_obj);
}

static JSBool edjs_CookieSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp) {
    const char *key = NULL;
    JSObject *c_obj = NULL;
    jsval val = JSVAL_VOID;

    key = JS_GetStringBytes(JS_ValueToString(cx, id));

    val = *vp; //copy the value before we set it to something else;
    //restores the value to the old value - cookies can't change value mid execution
    if (JS_FALSE == JS_GetProperty(cx, obj, key, vp))
        return JS_FALSE;

    if (JS_FALSE == JSVAL_IS_OBJECT(val) || JS_TRUE == JSVAL_IS_NULL(val)) {
        //if (JS_TRUE == JSVAL_IS_VOID(val) || JS_TRUE == JSVAL_IS_NULL(val)) {
        c_obj = JS_NewObject(cx, NULL, NULL, NULL);
        if (c_obj == NULL)
            return JS_FALSE;

        if (JS_FALSE == JS_SetProperty(cx, c_obj, "value", &val))
            return JS_FALSE;
    }
    else if (JSVAL_IS_OBJECT(val)) {
        c_obj = JSVAL_TO_OBJECT(val);
    }

    return edjs_CookieHelper(cx, key, c_obj);
}

