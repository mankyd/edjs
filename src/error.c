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


#include "jsapi.h"
#include "edjs_error.h"

static JSErrorFormatString edjs_ErrorFormatString[EDJS_ErrLimit] = {
#define MSG_DEF(name, number, count, exception, format) { format, count, exception } ,
#include "edjs.msg"
#undef MSG_DEF
};

const JSErrorFormatString *EDJS_GetErrorMessage(void *userRef, const char *locale, const uintN errorNumber) {
    if ((errorNumber > 0) && (errorNumber < EDJS_ErrLimit)) {
        return &edjs_ErrorFormatString[errorNumber];
    }
    return NULL;
}

