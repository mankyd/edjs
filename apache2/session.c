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
#include "apr_file_io.h"

#include "edjs.h"
#include "error.h"

#include "mod_edjs.h"
#include "session.h"


JSBool EDJS_AssignSessionData(JSContext *cx, request_rec *r) {
    JS_BeginRequest(cx);
    if (NULL == JS_DefineObject(cx, JS_GetGlobalObject(cx), "SESSION", &edjs_SessionClass, NULL,
                                JSPROP_ENUMERATE | JSPROP_PERMANENT)) {
        JS_EndRequest(cx);
        return JS_FALSE;
    }
    JS_EndRequest(cx);
    return JS_TRUE;
}


JSBool EDJS_CloseSession(JSContext *cx, request_rec *r) {
    JSBool ok = JS_TRUE;
    edjs_private *p = (edjs_private *)JS_GetContextPrivate(cx);
    mod_edjs_private *mp = (mod_edjs_private *)p->private_data;
    char *file_name = NULL;
    apr_file_t *session_file = NULL;
    apr_size_t prefix_len = 0;
    apr_size_t num_bytes_written = 0;
    JSObject *global = JS_GetGlobalObject(cx);
    jsval session_val;
    jsval json_val;
    JSString *json_str = NULL;
    char *json_char = NULL;
    apr_size_t json_len = 0;

    JS_BeginRequest(cx);
    if (JS_FALSE == JS_AddRoot(cx, &json_str))
        goto error;

    if (NULL == mp->session_id)
        goto finish;

    if (JS_FALSE == edjs_SessionFileName(mp, (const char **)&file_name)) {
        JS_ReportOutOfMemory(cx);
        goto error;
    }
   
    if (OK != apr_file_open (&session_file, file_name, APR_CREATE | APR_WRITE | APR_TRUNCATE, APR_OS_DEFAULT, r->pool)) {
        //report error
        session_file = NULL;
        goto error;
    }

    prefix_len = strlen(EDJS_SESSION_DATA_PREFIX);
    num_bytes_written = prefix_len;
    apr_file_write(session_file, EDJS_SESSION_DATA_PREFIX, &num_bytes_written);

    if (num_bytes_written != prefix_len) {
        //report error here
        goto error;
    }

    if (JS_FALSE == JS_GetProperty(cx, global, "SESSION", &session_val)) {
        //report error here
        goto error;
    }

    if (JS_FALSE == JSVAL_IS_OBJECT(session_val) || JS_TRUE == JSVAL_IS_NULL(session_val)) {
        //report error here
        goto error;
    }

    if (JS_FALSE == JS_CallFunctionName(cx, JSVAL_TO_OBJECT(session_val), "toJSON", 0, NULL, &json_val)) {
        //report error here
        goto error;
    }

    json_str = JS_ValueToString(cx, json_val);
    if (NULL == json_str) {
        //report error here
        goto error;
    }

    json_char = JS_GetStringBytes(json_str);
    json_len = strlen(json_char);
    if (0 == json_len) {
        //report error here
        goto error;
    }

    num_bytes_written = json_len;
    apr_file_write(session_file, json_char, &num_bytes_written);
    if (json_len != num_bytes_written) {
        //report error here
        goto error;
    }
    apr_file_close (session_file);

    goto finish;
error:
    ok = JS_FALSE;
    if (NULL != session_file) {
        apr_file_close (session_file);
        apr_file_remove(file_name, r->pool);
        session_file = NULL;
    }

finish:
    JS_RemoveRoot(cx, &json_str);
    if (NULL != mp->session_id) {
        JS_free(cx, mp->session_id);
        mp->session_id = NULL;
    }
    JS_EndRequest(cx);
    if (NULL != session_file)
        apr_file_close (session_file);

    return ok;

}


static void edjs_GenerateRandomString( char *str, unsigned long len, const char *src) {
    unsigned int max = 0;
    unsigned long i = 0L;

    if (src == NULL) {
        src = EDJS_SESSION_VALID_CHARS;
        max = EDJS_SESSION_NUM_VALID_CHARS;
    }
    else
        max = strlen( src );
    
    for ( ; i < len - 1L; ++i ) {
        str[ i ] = src[ rand() % max ];
    }
    
    str[i] = '\0';
}

static JSBool edjs_SessionFileName(mod_edjs_private *mp, const char **file_name) {
    //FIX ME: have this take a pool as a first argument rather than mod_edjs_private
    char *temp_dir;
    int session_file_name_len = 0;
    apr_temp_dir_get((const char **)&temp_dir, mp->r->pool);

    session_file_name_len = strlen(temp_dir);
    session_file_name_len += strlen(EDJS_SESSION_FILE_PREFIX);
    session_file_name_len += strlen(mp->session_id);
    session_file_name_len += 2; //1 for '/' and one for '\0'

    *file_name = (char *)apr_palloc(mp->r->pool, session_file_name_len);
    if (NULL == *file_name)
        return JS_FALSE;

    strcpy((char *)*file_name, temp_dir);
    strcat((char *)*file_name, "/");
    strcat((char *)*file_name, EDJS_SESSION_FILE_PREFIX);
    strcat((char *)*file_name, mp->session_id);

    return JS_TRUE;
}

static JSBool edjs_SessionInitialize(JSContext *cx, JSObject *obj) {
    JSBool ok = JS_TRUE;
    JSObject *global = JS_GetGlobalObject(cx);
    jsval cookie_val;
    jsval session_id_val;
    jsval session_id_tmp_val;
    jsval val_session_data;
    edjs_private *p = (edjs_private *)JS_GetContextPrivate(cx);
    mod_edjs_private *mp = (mod_edjs_private *)p->private_data;
    char *file_name = NULL;
    uint32 oldopts;
    JSScript *script = NULL;
    JSObject *script_obj = NULL;
    JSObject *it = NULL;
    JSString *str = NULL;
    jsid prop;
    jsval prop_name;
    jsval prop_val;
    char *c_prop_name = NULL;

    if (JS_FALSE == JS_AddRoot(cx, &str))
        goto error;

    if (JS_FALSE == JS_AddRoot(cx, &script_obj))
        goto error;

    if (mp->session_id != NULL)
        goto finish;
    
    if (JS_FALSE == JS_GetProperty(cx, global, "COOKIE", &cookie_val)) {
        EDJS_ERR(cx, EDJSERR_COOKIE_UNSET);
        goto error;
    }
    
    if (JS_FALSE == JSVAL_IS_OBJECT(cookie_val) || JS_TRUE == JSVAL_IS_NULL(cookie_val)) {
        EDJS_ERR(cx, EDJSERR_COOKIE_UNSET);
        goto error;
    }
    
    if (JS_FALSE == JS_GetProperty(cx, JSVAL_TO_OBJECT(cookie_val), EDJS_SESSION_COOKIE_NAME, &session_id_val)) {
        EDJS_ERR(cx, EDJSERR_GET_PROPERTY, EDJS_SESSION_COOKIE_NAME, "COOKIE");
        goto error;
    }

    mp->session_id = (char *)JS_malloc(cx, sizeof(char) * (EDJS_SESSION_KEY_LEN + 1));
    if (JS_FALSE == JSVAL_IS_VOID(session_id_val)) {
        strncpy(mp->session_id, JS_GetStringBytes(JS_ValueToString(cx, session_id_val)), EDJS_SESSION_KEY_LEN);
        mp->session_id[EDJS_SESSION_KEY_LEN] = '\0';

        //verify the validity of the characters in the session
        if (EDJS_SESSION_NUM_VALID_CHARS == strcspn (EDJS_SESSION_VALID_CHARS, mp->session_id)) {
            mp->session_id[0] = '\0';
            session_id_val = JSVAL_VOID; //they passed us an invalid session. try again
        }
        else if (JS_FALSE == edjs_SessionFileName(mp, (const char **)&file_name)) {
            JS_ReportOutOfMemory(cx);
            goto error;
        }


        //exec the script in the context of the SESSION object
        //which is empty, so should be harmless.
        oldopts = JS_GetOptions(cx);
        JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO);
        script = JS_CompileFile(cx, obj, file_name);
        if (NULL == script) {
            //report error
            JS_SetOptions(cx, oldopts);
            goto error;
        }
        script_obj = JS_NewScriptObject(cx, script);
        if (NULL == script_obj) {
            //report error
            JS_DestroyScript(cx, script);

            goto error;
        }

        if (JS_FALSE == JS_ExecuteScript(cx, obj, script, &val_session_data)) {
            //report error
            JS_SetOptions(cx, oldopts);
            goto error;
        }
        JS_SetOptions(cx, oldopts);

    }

    if (JS_TRUE == JSVAL_IS_VOID(session_id_val)) {
        do {
            //loop trying to find a session that doesn't exist
            edjs_GenerateRandomString(mp->session_id, EDJS_SESSION_KEY_LEN+1, NULL);
            
            if (JS_FALSE == edjs_SessionFileName(mp, (const char **)&file_name)) {
                JS_ReportOutOfMemory(cx);
                goto error;
            }
            if (0 != access(file_name, F_OK)) {
                session_id_val = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, mp->session_id));
                session_id_tmp_val = session_id_val;

                if (JS_FALSE == JS_SetProperty(cx, JSVAL_TO_OBJECT(cookie_val),
                                               EDJS_SESSION_COOKIE_NAME, &session_id_tmp_val)) {
                    //report error
                    goto error;
                }
            }
        } while (JS_TRUE == JSVAL_IS_VOID(session_id_val));
        val_session_data = JSVAL_NULL;
    }


    if (JS_FALSE == JSVAL_IS_OBJECT(val_session_data) || JS_TRUE == JSVAL_IS_NULL(val_session_data)) {
        //don't mark the session data as uninitialized - its simply empty
        goto finish;
    }


    JSObject *obj_session_data = JSVAL_TO_OBJECT(val_session_data);    

    jsval val_count;

    if (JS_FALSE == JS_GetProperty(cx, obj_session_data, "__count__", &val_count)) {
        //don't mark the session data as uninitialized - its simply empty
        return JS_FALSE;
    }

    if (JSVAL_TO_INT(val_count) == 0) {
        //don't mark the session data as uninitialized - its simply empty
        goto finish;
    }

    it = JS_NewPropertyIterator(cx, obj_session_data);

    do {
        if (JS_FALSE == JS_NextProperty(cx, it, &prop))
            return JS_FALSE;

        //if we've reached the end of our property list
        if (JSVAL_VOID == prop)
            break;

        //convert the property to a value
        if (JS_FALSE == JS_IdToValue(cx, prop, &prop_name)) {
            //report error
            goto error;
        }

        str = JS_ValueToString(cx, prop_name);
        if (NULL == str) {
            //report error
            goto error;
        }

        c_prop_name = JS_GetStringBytes(str);

        if (JS_FALSE == JS_GetProperty(cx, obj_session_data, c_prop_name, &prop_val)) {
            //report error
            goto error;
        }

        if (JS_FALSE == JS_DefineProperty(cx, obj, c_prop_name, prop_val, 
                                          NULL, NULL, JSPROP_ENUMERATE)) {
            //report error
            goto error;
        }

    } while(JSVAL_VOID != prop);
    
    
    goto finish;
error:
    ok = JS_FALSE;
finish:
    JS_RemoveRoot(cx, &str);
    JS_RemoveRoot(cx, &script_obj); 
   
    return ok;
}

static JSBool edjs_SessionResolve(JSContext *cx, JSObject *obj, jsval id) {
    return edjs_SessionInitialize(cx, obj);
}

