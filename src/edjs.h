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

#ifndef EDJS_EDJS_H
#define EDJS_EDJS_H

#include "jsapi.h"
#include "time.h"
#include "edjs_error.h"
#include <sys/stat.h>
#include <pthread.h>


#ifndef EDJS_SETTINGS_FILE_LOC
#define EDJS_SETTINGS_FILE_LOC "./settings.ed"
#endif

#ifndef EDJS_MOD_LOC
#define EDJS_MOD_LOC "./"
#endif

#ifndef EDJS_CORE_FILE_LOC
#define EDJS_CORE_FILE_LOC "./core.ed"
#endif

#define WHITESPACE_CHARS " \f\n\r\t\v"


typedef struct edjs_tree_node {
    void *key;

    void *value;

    int tree_data; //tree data stores meta_data for whatever tree implementation we're using
                   //red-black trees: tree_data==1 means the node is red, otherwise black
    struct edjs_tree_node* left;
    struct edjs_tree_node* right;
    struct edjs_tree_node* parent;
} edjs_tree_node;

typedef struct edjs_list_node {
    void *value;
    struct edjs_list_node* next;
} edjs_list_node;

typedef enum edjs_imp_module_type{EDJS_TEXT, EDJS_BINARY} edjs_imp_module_type;

typedef struct edjs_imp_module {
    edjs_imp_module_type type;
    char *file_path;
    JSObject *constructed_obj;
    void *dl_lib;
} edjs_imp_module;

typedef struct edjs_runtime_private {
    pthread_mutex_t mutex;
    JSScript *core_script;
    JSObject *core_script_obj;
    JSObject *settings_obj;
    edjs_list_node *imported_objects; //list of objects that need to be gc'ed before their corresponding shared libraries can be closed
    edjs_list_node *gced_objects; //objects that have been gced and are ready for cleanup

} edjs_runtime_private;

typedef struct edjs_private {
    edjs_runtime_private *rt_private;
    JSBool (*echo_function)(const char *, size_t len, void *);
    void (*error_function)(JSContext *, const char *, JSErrorReport *);
    JSBool random_seeded;
    void *private_data;
    uint32 branch_count;
    uint32 max_execution_time;
    clock_t start_time;
    JSBool max_execution_time_exceeded;
    edjs_tree_node *imported_modules;

    JSObject *settings_obj;
} edjs_private;

extern JSRuntime *EDJS_CreateRuntime(const char *settings_loc, EDJS_ErrNum *error_num);
extern JSContext *EDJS_CreateContext(JSRuntime *rt, edjs_private *p, JSBool edjs_standard_functions, EDJS_ErrNum *error_num);
extern void EDJS_CleanupContext(JSContext *cx);
extern void EDJS_CleanupRuntime(JSRuntime *rt);
extern JSBool EDJS_ExecuteFile(const char *file_name, JSRuntime *rt, void *private_data, EDJS_ErrNum *error_num, JSBool (*before_execute)(JSContext *),  JSBool (*after_execute)(JSContext *));

extern JSBool edjs_Clone(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
extern JSBool edjs_Copy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
extern JSBool EDJS_InitFunctions(JSContext *cx);

extern int edjs_TreeNodeLocate(edjs_tree_node *root, edjs_tree_node *n, edjs_tree_node **found, int (*comparator)(edjs_tree_node *, edjs_tree_node *));
extern edjs_tree_node *edjs_TreeNodeInsert(edjs_tree_node *root, edjs_tree_node *n, int (*comparator)(edjs_tree_node *, edjs_tree_node *));
extern void edjs_TreeDestroy(edjs_tree_node *root, void (*destroyer)(edjs_tree_node *));

void edjs_imp_module_destroy(JSContext *cx, edjs_imp_module *mod);

extern JSBool EDJS_ResolveFile(JSContext *cx, JSObject *path_array, const char **extension, const char *search, char **result, struct stat **file_stat);


extern void *EDJS_malloc(JSContext *cx, size_t nbytes);
extern void *EDJS_realloc(JSContext *cx, void *p, size_t nbytes);
extern char *EDJS_strdup(JSContext *cx, const char *s);
extern void EDJS_free(JSContext *cx, void *p);

extern JSBool edjs_GCCallback(JSContext *cx, JSGCStatus status);



#endif
