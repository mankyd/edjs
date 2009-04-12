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

#ifndef MOD_EDJS_CORE_H
#define MOD_EDJS_CORE_H

//apache hooks
static void *edjs_CreateDirConfig(apr_pool_t *p, char *path);
static void *edjs_MergeDirConfig(apr_pool_t *p, void *base, void *add);
static void *edjs_CreateServerConfig(apr_pool_t *p, server_rec *s);
static void *edjs_MergeServerConfig(apr_pool_t *p, void *base, void *add);
static int edjs_Init(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s);
static void edjs_ChildInit(apr_pool_t *p, server_rec *s);
static void edjs_RegisterHooks(apr_pool_t *p);
static apr_status_t edjs_CleanupRuntimes(void *me_rt);
static const char *edjs_CmdUrlMapper(cmd_parms *params, void *mconfig, char *to);
static const char *edjs_CmdSettingsFile(cmd_parms *params, void *mconfig, char *to);
static int edjs_Handler(request_rec *r);

//utility functions
static JSBool edjs_Echo(JSString *str, void *p);
static JSRuntime *edjs_FindOrCreateRuntime(server_rec *r, void *module_config, JSBool should_create);
static void edjs_ReportJSError(JSContext *cx, const char *message, JSErrorReport *report);

//js functions
static JSBool edjs_SetResponseCode(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_SetHeader(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_AddHeader(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
static JSBool edjs_RemoveHeader(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

#endif
