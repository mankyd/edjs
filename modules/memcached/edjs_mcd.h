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

#ifndef EDJS_MEMCACHED_H
#define EDJS_MEMCACHED_H

#include "jsapi.h"
#include "edjs.h"
#include "memcached.h"

#define MCD_PROP_CONSTANT(name, value) {value, name, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT, {0, 0, 0} }
#define MCD_PROP_CONSTANT_END {0, 0, 0, {0, 0, 0} }

static JSBool edjs_mcd_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static void edjs_mcd_Finalize(JSContext *cx, JSObject *obj);

static JSBool edjs_mcd_Get(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_mcd_Set(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_mcd_Add(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_mcd_Replace(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_mcd_Delete(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_mcd_AddServers(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_mcd_GetServers(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

typedef struct edjs_mcd_private {
    memcached_st *servers;
} edjs_mcd_private;

struct JSClass edjs_mcd_class =  {
    "Memcached", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, edjs_mcd_Finalize,
    NULL, NULL, 
    NULL,
    NULL,
    NULL, NULL, NULL, NULL };

static JSBool edjs_mcd_SetException(JSContext *cx, edjs_mcd_private *p, memcached_return mcd_code);
#define EDJS_MCD_ERR(cx, edjs_mcd_private, mcd_code) edjs_mcd_SetException(cx, edjs_mcd_private, mcd_code)




#endif
