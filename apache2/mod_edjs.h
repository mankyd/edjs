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

#ifndef MOD_EDJS_H
#define MOD_EDJS_H

#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "jsapi.h"

#define VERSION_COMPONENT "mod_edjs/1.0"

typedef struct mod_edjs_private {
    request_rec *r;
    char *session_id;
} mod_edjs_private;

typedef struct mod_edjs_runtime {
    pid_t pid;
    unsigned server_line;
    //JSRuntime *rt;
    //    struct mod_edjs_runtime *next;
} mod_edjs_runtime;

typedef struct mod_edjs_server_config {
    //mod_edjs_runtime **runtimes;
    edjs_tree_node **runtimes;
    char *settings_loc;
} mod_edjs_server_config;

typedef struct mod_edjs_dir_config {
    char *url_mapper;
} mod_edjs_dir_config;

#ifndef PATH_MAX
#define PATH_MAX 255 //fix this
#endif

#define EDJS_SESSION_KEY_LEN 32
#define EDJS_SESSION_COOKIE_NAME "EDJS_SESSION_ID"
#define EDJS_SESSION_FILE_PREFIX "edjssess_"
#define EDJS_SESSION_DATA_PREFIX "s="
#define EDJS_SESSION_VALID_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
#define EDJS_SESSION_NUM_VALID_CHARS 62

extern JSBool EDJS_AssignSessionData(JSContext *cx, request_rec *r);
extern JSBool EDJS_CloseSession(JSContext *cx, request_rec *r);


#define EDJS_REQDATA_SPECIAL_COOKIE_CHARS "=,; \t\r\n\013\014"

extern JSBool EDJS_AssignEnvData(JSContext *cx, request_rec *);
extern JSBool EDJS_AssignGetData(JSContext *cx, request_rec *);
extern JSBool EDJS_AssignCookieData(JSContext *cx, request_rec *r);
extern JSBool EDJS_AssignPostData(JSContext *cx, request_rec *r);


#endif
