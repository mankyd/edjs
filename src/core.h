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

#ifndef EDJS_CORE_H
#define EDJS_CORE_H

#include "jsapi.h"
#include "time.h"

extern JSBool EDJS_InitFunctions(JSContext *cx);

static const char *edjs_ParseJSONValue(const char *str_char);
static const char *edjs_ParseJSONObject(const char *str_char);
static const char *edjs_ParseJSONArray(const char *str_char);
static const char *edjs_ParseJSONString(const char *str_char);
static const char *edjs_ParseJSONNumber(const char *str_char);
static const char *edjs_ParseJSONIdentifier(const char *str_char);
static const char *edjs_ParseJSONConstant(const char *str_char);

static JSBool edjs_StringToJSON(JSContext *cx, jsval arg, jsval *rval);
static JSBool edjs_ArrayToJSON(JSContext *cx, JSObject *obj, jsval *rval);
static JSBool edjs_ObjectToJSON(JSContext *cx, JSObject *obj, jsval *rval);

static JSBool edjs_Echo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_Import(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_Include(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_IsJSON(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_ExtractJSON(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_ToJSON(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_IsArray(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_IsBoolean(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_IsFunction(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_IsString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_IsNumber(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_IsUndefined(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);


static JSBool edjs_QuoteString(JSContext *cx, const char* input, char **ret, int *len);
static JSBool edjs_UnquoteString(JSContext *cx, const char* input, char **ret, int *len);

static int edjs_imp_module_comparator(edjs_tree_node *na, edjs_tree_node *nb);


#endif
