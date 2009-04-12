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

#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>

#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_log.h"

#include "jsapi.h"

#include "edjs.h"
#include "error.h"

#include "mod_edjs.h"
#include "core.h"

static command_rec edjs_cmds[] = {
    {
        "EDJSURLMapper",                              //directive name
        {(const char *(*)())edjs_CmdUrlMapper},       //config action routine
        NULL,                                         //argument to include in call
        OR_FILEINFO,                                  //where available
        TAKE1,                                        //arguments
        "EDJS file to handle requests instead of standard path mapping" //description
    },

    {
        "EDJSSettingsFile",                           //directive name
        {(const char *(*)())edjs_CmdSettingsFile},    //config action routine
        NULL,                                         //argument to include in call
        RSRC_CONF,                                    //where available
        TAKE1,                                        //arguments
        "Override for the EDJS settings file" //description
    },

    {NULL}
};

// Dispatch list for API hooks
module AP_MODULE_DECLARE_DATA edjs_module = {
    STANDARD20_MODULE_STUFF, 
    edjs_CreateDirConfig,    /* create per-dir    config structures */
    edjs_MergeDirConfig,     /* merge  per-dir    config structures */
    edjs_CreateServerConfig, /* create per-server config structures */
    edjs_MergeServerConfig,  /* merge  per-server config structures */
    edjs_cmds,               /* table of config file commands       */
    edjs_RegisterHooks       /* register hooks                      */
};

static void edjs_RegisterHooks(apr_pool_t *p) {
    ap_hook_post_config(edjs_Init, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_child_init(edjs_ChildInit, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_handler(edjs_Handler, NULL, NULL, APR_HOOK_MIDDLE);
}

static void *edjs_CreateDirConfig(apr_pool_t *p, char *path) {
    mod_edjs_dir_config *cfg = (mod_edjs_dir_config *)apr_pcalloc(p, sizeof(mod_edjs_dir_config));
    
    cfg->url_mapper = NULL;

    return (void *)cfg;
}

static void *edjs_CreateServerConfig(apr_pool_t *p, server_rec *s) {
    mod_edjs_server_config *cfg = (mod_edjs_server_config *)apr_pcalloc(p, sizeof(mod_edjs_server_config));
    //cfg->runtimes = (mod_edjs_runtime **)apr_pcalloc(p, sizeof(mod_edjs_runtime *));
    cfg->runtimes = (edjs_tree_node **)apr_pcalloc(p, sizeof(edjs_tree_node *));
    *(cfg->runtimes) = NULL;
    cfg->settings_loc = NULL;

    return (void *)cfg;
}

static void *edjs_MergeServerConfig(apr_pool_t *p, void *base, void *add) {
    mod_edjs_server_config *cfg = (mod_edjs_server_config *)apr_pcalloc(p, sizeof(mod_edjs_server_config));
    mod_edjs_server_config *base_cfg = (mod_edjs_server_config *)base;
    mod_edjs_server_config *add_cfg = (mod_edjs_server_config *)add;

    //don't take any of the runtimes from base_cfg
    cfg->runtimes = add_cfg->runtimes;

    cfg->settings_loc = (NULL != add_cfg->settings_loc ? add_cfg->settings_loc : base_cfg->settings_loc);

    return (void *)cfg;
}

static void *edjs_MergeDirConfig(apr_pool_t *p, void *base, void *add) {
    mod_edjs_dir_config *cfg = (mod_edjs_dir_config *)apr_pcalloc(p, sizeof(mod_edjs_dir_config));
    mod_edjs_dir_config *base_cfg = (mod_edjs_dir_config *)base;
    mod_edjs_dir_config *add_cfg = (mod_edjs_dir_config *)add;
    cfg->url_mapper = (NULL != add_cfg->url_mapper ? add_cfg->url_mapper : base_cfg->url_mapper);

    return (void *)cfg;
    
}

static int edjs_Init(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s) {
    ap_add_version_component(p, VERSION_COMPONENT);

    return OK;
}

static void edjs_ChildInit(apr_pool_t *p, server_rec *s) {
    //can't create the runtime here because 
    //edjs_FindOrCreateRuntime(s->process->pool, ap_get_module_config(s->module_config, &edjs_module), JS_TRUE);//create our runtime for this pid
}

static apr_status_t edjs_CleanupRuntimes(void *me_rt) {
    /*
    mod_edjs_runtime *_me_rt = (mod_edjs_runtime *)me_rt;
    if (NULL != _me_rt->rt) {
        EDJS_CleanupRuntime(_me_rt->rt);
        _me_rt->rt = NULL;
        JS_ShutDown();
    }
    */
    return OK;
}

int mod_edjs_rt_comparator(edjs_tree_node *na, edjs_tree_node *nb) {
    mod_edjs_runtime *ka = na->key;
    mod_edjs_runtime *kb = na->key;

    if (ka->pid == kb->pid) {
        if (ka->server_line == kb->server_line)
            return 0;
        else if (ka->server_line > kb->server_line)
            return 1;
        return -1;
    }
    else if (ka->pid > kb->pid)
        return 1;
    return -1;
}

static JSRuntime *edjs_FindOrCreateRuntime(server_rec *r, void *module_config, JSBool should_create) {
    mod_edjs_server_config *cfg = module_config;
    edjs_tree_node *search_node = NULL; //*me_rts;
    edjs_tree_node *rt_node = NULL;
    mod_edjs_runtime *key = NULL;
    EDJS_ErrNum error_num;

    //we have one runtime per process per server

    rt_node = (edjs_tree_node *)malloc(sizeof(edjs_tree_node));
    if (NULL == rt_node)
        return NULL;

    key = malloc(sizeof(mod_edjs_runtime));
    if (NULL == key)
        return NULL;

    key->pid = getpid();
    key->server_line = r->defn_line_number;
    rt_node->key = key;

    if (0 == edjs_TreeNodeLocate(*(cfg->runtimes), rt_node, &search_node, mod_edjs_rt_comparator) && 
        NULL != search_node) {

        return (JSRuntime *)search_node->value;
    }

    if (JS_FALSE == should_create)
        return NULL;

    rt_node->value = EDJS_CreateRuntime(cfg->settings_loc, &error_num);
    if (NULL == rt_node->value)
        return NULL;
    
    *(cfg->runtimes) = edjs_TreeNodeInsert(*(cfg->runtimes), rt_node, mod_edjs_rt_comparator);

    return rt_node->value;
}

static const char *edjs_CmdUrlMapper(cmd_parms *params, void *mconfig, char *to) {
    mod_edjs_dir_config *cfg = (mod_edjs_dir_config *)mconfig;
    cfg->url_mapper = (char *)apr_pcalloc(params->pool, (strlen(to) + 1) * sizeof(char));
    strcpy(cfg->url_mapper, to);
    return NULL;
}

static const char *edjs_CmdSettingsFile(cmd_parms *params, void *mconfig, char *to) {
    mod_edjs_server_config *cfg = (mod_edjs_server_config *)ap_get_module_config(params->server->module_config, &edjs_module);
    cfg->settings_loc = (char *)apr_pcalloc(params->pool, (strlen(to) + 1) * sizeof(char));
    strcpy(cfg->settings_loc, to);

    return NULL;
}


// The error reporter callback.
void edjs_ReportJSError(JSContext *cx, const char *message, JSErrorReport *report) {
    edjs_private *p = (edjs_private *)JS_GetContextPrivate(cx);
    request_rec *r = ((mod_edjs_private *)p->private_data)->r;
    char *buff = NULL;
    unsigned int buff_len = 4; //4 for '#', ':', ' ' and '\0'
    unsigned int lineno = report->lineno;

    do {
        buff_len++;
        lineno /= 10;
    } while(lineno > 0);

    buff = EDJS_malloc(cx, buff_len * sizeof(char));
    ap_rputs("<br />ERROR: ", r);
    ap_rputs(report->filename ? report->filename : "<no filename>", r);
    if (NULL != buff) {
        sprintf(buff, "#%u: ", (unsigned int) report->lineno);
        ap_rputs(buff, r);
        EDJS_free(cx, buff);
    }
    ap_rputs(message, r);
    ap_rputs("<br />", r);
}

static JSBool edjs_SetResponseCode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    edjs_private *p = (edjs_private *)JS_GetContextPrivate(cx);
    request_rec *r = ((mod_edjs_private *)p->private_data)->r;

    if (argc != 1) {
        EDJS_ERR(cx, EDJSERR_NUM_ARGS, "HTTP.SetResponseCode");
        return JS_FALSE;
    }

    int32 code;
    if (JS_FALSE == JS_ValueToInt32(cx, *argv, &code)) {
        EDJS_ERR(cx, EDJSERR_INT_CONVERSION);
        return JS_FALSE;
    }

    r->status = code;

    return JS_TRUE;
}

static JSBool edjs_SetHeader(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjs_private *p = (edjs_private *)JS_GetContextPrivate(cx);
    request_rec *r = ((mod_edjs_private *)p->private_data)->r;
    JSString *str = NULL;
    char *header_key = NULL;

    JS_AddRoot(cx, &str);

    if (argc != 2) {
        EDJS_ERR(cx, EDJSERR_NUM_ARGS, "HTTP.SetHeader");
        goto error;
    }

    str = JS_ValueToString(cx, *argv);
    if (NULL == str) {
        EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
        goto error;
    }

    header_key = JS_GetStringBytes(str);
    if (0 == strcmp("Location", header_key) && 3 != r->status / 100)
        r->status = HTTP_MOVED_TEMPORARILY;

    str = JS_ValueToString(cx, *(argv+1));
    if (NULL == str) {
        EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
        goto error;
    }

    apr_table_set(r->headers_out, header_key, JS_GetStringBytes(str));

    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    JS_RemoveRoot(cx, &str);
    return ok;
}

static JSBool edjs_AddHeader(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjs_private *p = (edjs_private *)JS_GetContextPrivate(cx);
    request_rec *r = ((mod_edjs_private *)p->private_data)->r;
    JSString *str = NULL;
    char *header_key = NULL;

    JS_AddRoot(cx, &str);

    if (argc != 2) {
        EDJS_ERR(cx, EDJSERR_NUM_ARGS, "HTTP.AddHeader");
        goto error;
    }

    str = JS_ValueToString(cx, *argv);
    if (NULL == str) {
        EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
        goto error;
    }

    header_key = JS_GetStringBytes(str);
    if (strcmp("Location", header_key) == 0 && r->status / 100 != 3)
        r->status = HTTP_MOVED_TEMPORARILY;

    str = JS_ValueToString(cx, *(argv+1));
    if (NULL == str) {
        EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
        goto error;
    }

    apr_table_add(r->headers_out, header_key, JS_GetStringBytes(str));

    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    JS_RemoveRoot(cx, &str);
    return ok;
}

static JSBool edjs_RemoveHeader(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    edjs_private *p = (edjs_private *)JS_GetContextPrivate(cx);
    request_rec *r = ((mod_edjs_private *)p->private_data)->r;
    JSString *str = NULL;

    JS_AddRoot(cx, &str);

    if (argc != 1) {
        EDJS_ERR(cx, EDJSERR_NUM_ARGS, "HTTP.RemoveHeader");
        goto error;
    }

    str = JS_ValueToString(cx, *argv);
    if (NULL == str) {
        EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
        goto error;
    }

    apr_table_unset(r->headers_out, JS_GetStringBytes(str));

    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    JS_RemoveRoot(cx, &str);
    return ok;
}

static JSBool mod_edjs_Echo(const char *buff, size_t len, void *p) {
    request_rec *r = ((mod_edjs_private *)p)->r;

    if (r->header_only)
        return JS_TRUE;

    ap_rwrite(buff, len, r);

    return JS_TRUE;
}

static JSBool edjs_InitializeContext(JSContext *cx) {
    JSBool ok = JS_TRUE;
    JSObject *global = NULL;
    struct edjs_private *p = NULL;
    struct mod_edjs_private *mp = NULL;
    JSObject *js_http = NULL;
    jsval jsv_http = JSVAL_VOID;
    JSBool request_started = JS_FALSE;

    global = JS_GetGlobalObject(cx);
    p = JS_GetContextPrivate(cx);
    mp = p->private_data;
    p->echo_function = mod_edjs_Echo;
    p->error_function = edjs_ReportJSError;

    if (JS_FALSE == EDJS_AssignEnvData(cx, mp->r)) {
        goto error;
    }
    if (JS_FALSE == EDJS_AssignGetData(cx, mp->r)) {
        goto error;
    }
    if (JS_FALSE == EDJS_AssignCookieData(cx, mp->r)) {
        goto error;
    }
    if (JS_FALSE == EDJS_AssignPostData(cx, mp->r)) {
        goto error;
    }
    if (JS_FALSE == EDJS_AssignSessionData(cx, mp->r)) {
        goto error;
    }

    JS_BeginRequest(cx);
    request_started = JS_TRUE;

    js_http = JS_NewObject(cx, NULL, NULL, NULL);
    if (NULL == js_http) {
        /*
        ap_log_rerror(APLOG_MARK, APLOG_CRIT, DECLINED, r,
                     "Failed to create HTTP object: %s", request_file);
        r->status = HTTP_INTERNAL_SERVER_ERROR;
        */
        goto error;
    }

    jsv_http = OBJECT_TO_JSVAL(js_http);

    if (JS_FALSE == JS_SetProperty(cx, global, "HTTP", &jsv_http)) {
        /*
        ap_log_rerror(APLOG_MARK, APLOG_CRIT, DECLINED, r,
                     "Failed to create HTTP object: %s", request_file);

        r->status = HTTP_INTERNAL_SERVER_ERROR;
        */
        goto error;
    }

    JSFunctionSpec mod_edjs_functions[] = {
        JS_FS("setResponseCode", edjs_SetResponseCode, 1, 0, 0),
        JS_FS("setHeader",       edjs_SetHeader,       2, 0, 0),
        JS_FS("addHeader",       edjs_AddHeader,       2, 0, 0),
        JS_FS("removeHeader",    edjs_RemoveHeader,    1, 0, 0),
        JS_FS_END
    };

    if (JS_FALSE == JS_DefineFunctions(cx, js_http, mod_edjs_functions)) {
        /*
        ap_log_rerror(APLOG_MARK, APLOG_CRIT, DECLINED, r,
                     "Failed to define HTTP functions: %s", request_file);

        r->status = HTTP_INTERNAL_SERVER_ERROR;
        */
        goto error;
    }

    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    if (JS_TRUE == request_started)
        JS_EndRequest(cx);
    return ok;
}

static JSBool edjs_FinishContext(JSContext *cx) {
    JSBool ok = JS_TRUE;
    struct edjs_private *p = NULL;
    struct mod_edjs_private *mp = NULL;

    p = JS_GetContextPrivate(cx);
    if (NULL == p)
        goto error;

    mp = p->private_data;
    if (NULL == mp)
        goto error;

    if (NULL != cx)
        EDJS_CloseSession(cx, mp->r);

    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    return ok;
}

static int edjs_Handler(request_rec *r){

    if (strcmp(r->handler, "edjs")) {
        return DECLINED;
    }

    r->content_type = "text/html";

    JSRuntime *rt = NULL;
    mod_edjs_server_config *server_cfg = NULL;
    mod_edjs_dir_config *dir_cfg = NULL;
    struct mod_edjs_private mp;
    char *request_file = NULL;
    char *parent_dir = NULL;
    struct stat stat_info;

    server_cfg = (mod_edjs_server_config *)ap_get_module_config(r->server->module_config, &edjs_module);
    dir_cfg = (mod_edjs_dir_config *)ap_get_module_config(r->per_dir_config, &edjs_module);

    rt = edjs_FindOrCreateRuntime(r->server, server_cfg, JS_TRUE);
    if (NULL == rt) {
        ap_log_rerror(APLOG_MARK, APLOG_CRIT, DECLINED, r, 
                     "Could not create runtime");

        r->status = HTTP_INTERNAL_SERVER_ERROR;
        goto finish;
    }

    mp.session_id = NULL;
    mp.r = r;

    if (NULL != dir_cfg->url_mapper) {
        request_file = (char *)apr_pcalloc(r->pool, (strlen(dir_cfg->url_mapper) + 1) * sizeof(char));
        strcpy(request_file, dir_cfg->url_mapper); //assuming that this is null terminated
        ap_getparents(request_file);
    }
    else {
        request_file = (char *)apr_pcalloc(r->pool, (strlen(r->filename) + 1) * sizeof(char));
        strcpy(request_file, r->filename); //assuming that this is null terminated
    }

    //FIX ME: should this be access() instead of stat()?
    if (0 != stat(request_file, &stat_info)) {
        ap_log_rerror(APLOG_MARK, APLOG_NOTICE, DECLINED, r,
                     "Could not resolve requested file: %s", request_file);


        r->status = HTTP_NOT_FOUND;
        goto finish;
    }
    if (!S_ISREG(stat_info.st_mode) && !S_ISLNK(stat_info.st_mode)) {
        ap_log_rerror(APLOG_MARK, APLOG_WARNING, DECLINED, r,
                     "File type not understood: %s", request_file);
        r->status = HTTP_NOT_FOUND;
        goto finish;
    }

    parent_dir = ap_make_dirstr_parent (r->pool, request_file);
    //already know that this is a valid directory of some sort since the call to stat above worked
    if (0 != chdir(parent_dir)) {
        ap_log_rerror(APLOG_MARK, APLOG_NOTICE, DECLINED, r,
                     "Error changing working directories. Could not change to: %s", parent_dir);

        r->status = HTTP_NOT_FOUND;
        goto finish;
    }

    EDJS_ErrNum error_num;
    if (JS_FALSE == EDJS_ExecuteFile(request_file, rt, &mp, &error_num, edjs_InitializeContext,  edjs_FinishContext)) {
        switch(error_num) {
        case EDJSERR_CREATING_CONTEXT:
            ap_log_rerror(APLOG_MARK, APLOG_CRIT, DECLINED, r,
                          "Failed to create javascript context: %s", request_file);
            r->status = HTTP_INTERNAL_SERVER_ERROR;
            break;

        case EDJSERR_COMPILING_CORE:
            ap_log_rerror(APLOG_MARK, APLOG_CRIT, DECLINED, r,
                          "Failed to compile core file: %s", request_file);
            r->status = HTTP_INTERNAL_SERVER_ERROR;
            break;

        case EDJSERR_COMPILING_SETTINGS:
            ap_log_rerror(APLOG_MARK, APLOG_CRIT, DECLINED, r,
                          "Failed to compile settings file: %s", request_file);
            r->status = HTTP_INTERNAL_SERVER_ERROR;
            break;
            
        case EDJSERR_EXECUTION_TIME_EXCEEDED:
            ap_log_rerror(APLOG_MARK, APLOG_CRIT, DECLINED, r,
                          "Max execution time exceeded: %s", request_file);
            r->status = HTTP_INTERNAL_SERVER_ERROR;
            break;

        default:
            ap_log_rerror(APLOG_MARK, APLOG_CRIT, DECLINED, r,
                          "An unknown error has occured: %s", request_file);
            r->status = HTTP_INTERNAL_SERVER_ERROR;
            break;

        }

        goto finish;
    }


 finish:

    return OK;
}
