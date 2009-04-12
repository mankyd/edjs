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

#ifndef EDJS_REQDATA_H
#define EDJS_REQDATA_H

#include "mod_edjs.h"

typedef struct edjs_request_data {
    request_rec *r;
    apr_pool_t *pool;
    JSContext *cx;
    JSObject *container;
    JSBool read_only;

} edjs_request_data;


static JSBool edjs_AssignJsValToReqData(edjs_request_data *d, const char *key, jsval val);
static int edjs_LoadParamsCallback(void *p, const char *key, const char *value);
static int edjs_LoadApreqParamsCallback(void *p, const char *key, const char *value);
static JSBool edjs_CookieHelper(JSContext *cx, const char* key, JSObject *c_obj);
static JSBool edjs_CookieDeleter(JSContext *cx, JSObject *obj, jsval id, jsval *vp);
static JSBool edjs_CookieSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

#endif
