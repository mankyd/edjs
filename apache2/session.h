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

#ifndef EDJS_SESSION_H
#define EDJS_SESSION_H

#include "mod_edjs.h"

static void edjs_GenerateRandomString( char *str, unsigned long len, const char *src);
static JSBool edjs_SessionFileName(mod_edjs_private *mp, const char **file_name);
static JSBool edjs_SessionInitialize(JSContext *cx, JSObject *obj);
static JSBool edjs_SessionResolve(JSContext *cx, JSObject *obj, jsval id);


static JSClass edjs_SessionClass = {
    "EDJSSession", 0,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, edjs_SessionResolve, JS_ConvertStub, JS_FinalizeStub,
    NULL
};

#endif
