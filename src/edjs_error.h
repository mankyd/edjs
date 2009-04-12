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


#ifndef EDJS_ERROR_H
#define EDJS_ERROR_H

#include "jsapi.h"

typedef enum EDJS_ErrNum {
#define MSG_DEF(name, number, count, exception, format) name = number,
#include "edjs.msg"
#undef MSG_DEF
    EDJS_ErrLimit
} EDJS_ErrNum;

#define EDJS_ERR(cx, ...) if (JS_FALSE == JS_IsExceptionPending(cx)) JS_ReportErrorNumber(cx, EDJS_GetErrorMessage, NULL, __VA_ARGS__)
#define EDJS_FORCE_ERR(cx, ...) JS_ReportErrorNumber(cx, EDJS_GetErrorMessage, NULL, __VA_ARGS__)

extern const JSErrorFormatString *EDJS_GetErrorMessage(void *userRef, const char *locale, const uintN errorNumber);

#endif
