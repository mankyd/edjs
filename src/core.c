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

#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>

#include "jsapi.h"

#include "edjs.h"
#include "core.h"
#include "edjs_error.h"

static JSBool edjs_Echo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    uintN i, n;
    JSString *str = NULL;
    edjs_private *p = (edjs_private *)JS_GetContextPrivate(cx);
    JSObject *global = JS_GetGlobalObject(cx);

    JS_BeginRequest(cx);

    //if echo was called on something other than our global object,
    //i.e. instance.echo();
    if (global != obj) {
        str = JS_ValueToString(cx, OBJECT_TO_JSVAL(obj));
        *rval = STRING_TO_JSVAL(str);

        if (NULL == str) {
            EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
            goto error;
        }
        if (NULL != p->echo_function) {
            if (JS_FALSE == p->echo_function(JS_GetStringBytes(str), JS_GetStringLength(str), p->private_data)) {
                EDJS_ERR(cx, EDJSERR_UNKNOWN_ERROR);
                goto error;
            }
        }
        else
            printf("%s", JS_GetStringBytes(str));
    }
    //else just echo out our arguments
    else {
        for (i = n = 0; i < argc; i++) {
            str = JS_ValueToString(cx, argv[i]);
            *rval = STRING_TO_JSVAL(str);
            if (NULL == str) {
                EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
                goto error;
            }
            if (NULL != p->echo_function) {
                if (JS_FALSE == p->echo_function(JS_GetStringBytes(str), JS_GetStringLength(str), p->private_data)) {
                    EDJS_ERR(cx, EDJSERR_UNKNOWN_ERROR);
                    goto error;
                }
            }
            else
                printf("%s", JS_GetStringBytes(str));
        }
    }

    *rval = OBJECT_TO_JSVAL(obj);

    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    JS_EndRequest(cx);

    return ok;
}

int edjs_imp_module_comparator(edjs_tree_node *na, edjs_tree_node *nb) {
    struct stat *a = (struct stat *)na->key;
    struct stat *b = (struct stat *)nb->key;

    if (a->st_dev == b->st_dev) {
        if (a->st_ino == b->st_ino)
            return 0;
        else if (a->st_ino > b->st_ino)
            return 1;
        return -1;
    }
    else if (a->st_dev > b->st_dev)
        return 1;
    return -1;
    //return strcmp(na->key, nb->key);
}

static JSBool edjs_Import(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    //expects 1 argument
    //has 4 extra argv roots

    //FIX ME: this function is ridiculously long. break it up

    JSBool ok = JS_TRUE;
    jsval path_val = JSVAL_VOID;
    JSObject *path_obj = NULL;
    JSString *file_str = NULL;
    char *file_path = NULL;
    char *full_path = NULL;
    char *it = NULL;
    char *it2 = NULL;
    char *it3 = NULL;
    const char *extensions[] = {"ed","so", NULL};
    JSFunction *func = NULL;
    JSObject *constructed_obj = NULL;
    JSObject *import_obj = NULL;
    jsuint i = 0, import_len = 0;
    jsval import_val = JSVAL_VOID;
    jsval import_alias_val = JSVAL_VOID;
    JSString *import_str = NULL;
    JSString *import_alias_str = NULL;
    JSBool found_prop_b = JS_FALSE;
    JSIdArray *import_obj_ids = NULL;
    FILE * temp_file = NULL;
    long file_size = 0;
    char * buffer = NULL;
    size_t bytes_read = 0;
    edjs_private *p = NULL;
    edjs_runtime_private *rt_private = NULL;
    edjs_tree_node *module_node = NULL;
    edjs_tree_node *existing_node = NULL;
    edjs_list_node *module_list_node = NULL;
    edjs_imp_module *module_data = NULL;
    struct stat *file_stat = NULL;
    JSBool (*plugin_init)(JSContext *,JSObject **) = NULL;

    if (1 > argc) {
        EDJS_ERR(cx, EDJSERR_NUM_ARGS, "imp");
        goto error;
    }

    JS_BeginRequest(cx);
    //we haven to initialize these variables before we can assign them to the extra roots
    //argv[argc] = STRING_TO_JSVAL(file_str);
    //argv[argc+1] = OBJECT_TO_JSVAL(constructed_obj);
    //argv[argc+2] = STRING_TO_JSVAL(import_str);
    //argv[argc+3] = STRING_TO_JSVAL(import_alias_str);

    if (JS_FALSE == JS_AddRoot(cx, &func)) {
        EDJS_ERR(cx, EDJSERR_ADD_ROOT);
        goto error;
    }

    module_node = (edjs_tree_node *)EDJS_malloc(cx, sizeof(edjs_tree_node));//don't create nodes with the context
    if (NULL == module_node) {
        goto error;
    }
    module_node->key = NULL;
    module_node->value = NULL;

    p = (edjs_private *)JS_GetContextPrivate(cx);
    if (NULL == p) {
        EDJS_ERR(cx, EDJSERR_PRIVATE_DATA_NOT_SET);
        goto error;
    }

    if (JS_FALSE == JS_GetProperty(cx, p->settings_obj, "include_path", &path_val)) {
        EDJS_ERR(cx, EDJSERR_GET_PROPERTY, "EDJS", "include_path");
        goto error;
    }

    if (JS_FALSE == JSVAL_IS_OBJECT(path_val) || JS_TRUE == JSVAL_IS_NULL(path_val) ||
        JS_FALSE == JS_IsArrayObject(cx, JSVAL_TO_OBJECT(path_val))) {
        EDJS_ERR(cx, EDJSERR_VAR_TYPE, "EDJS.include_path", "Array");
        goto error;
    }


    path_obj = JSVAL_TO_OBJECT(path_val);

    file_str = JS_ValueToString(cx, argv[0]);
    if (NULL == file_str) {
        EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
        goto error;
    }
    argv[argc] = STRING_TO_JSVAL(file_str);


    file_path = EDJS_malloc(cx, (strlen(JS_GetStringBytes(file_str)) + 1) * sizeof(char)); //+1 for '\0'
    if (NULL == file_path) {
        goto error;
    }

    strcpy(file_path, JS_GetStringBytes(file_str));
    //convert all dots to '/'
    it = file_path;
    while ('\0' != *it) {
        if ('.' == *it)
            *it = '/';
        it++;
    }


    //note: EDJS_ResolveFile will JS_malloc() space for full_path.
    //we should not free this memory in this function. rather it will be
    //put into the module_node, whic will free it when it is destroyed
    if (JS_FALSE == EDJS_ResolveFile(cx, path_obj, extensions, file_path, &full_path, &file_stat)) {
        goto error;
    }

    if (NULL == full_path) {
        EDJS_ERR(cx, EDJSERR_FILENAME_RESOLVE, file_path);
        goto error;
    }

    module_node->key = (void *)file_stat;
    if (0 == edjs_TreeNodeLocate(p->imported_modules, module_node, &existing_node, edjs_imp_module_comparator) && 
        NULL != existing_node) {
        edjs_imp_module_destroy(cx, module_node->value);
        EDJS_free(cx, module_node);
        module_node = existing_node;
        module_data = (edjs_imp_module *)module_node->value;
        constructed_obj = module_data->constructed_obj;
    }
    else {
        module_data = (edjs_imp_module *)EDJS_malloc(cx, sizeof(edjs_imp_module));
        if (NULL == module_data) {
            goto error;
        }

        module_node->value = (void *)module_data;
        module_data->file_path = full_path;

        if (0 == strcmp (strrchr (full_path, '.' ) + 1, "ed" )) { //text file
            module_data->type = EDJS_TEXT;
            temp_file = fopen (full_path, "rb" );
            if (NULL == temp_file) {
                EDJS_ERR(cx, EDJSERR_OPENING_MODULE, JS_GetStringBytes(file_str));
                goto error;
            }
            // obtain file size:
            fseek (temp_file, 0, SEEK_END);
            file_size = ftell (temp_file);
            rewind (temp_file);
            buffer = (char*)EDJS_malloc (cx, sizeof(char)*file_size);
            if (NULL == buffer) {
                goto error;
            }

            bytes_read = fread (buffer, 1, file_size, temp_file);
            if (bytes_read != file_size) {
                EDJS_ERR(cx, EDJSERR_READING_MODULE, JS_GetStringBytes(file_str));
                goto error;
            }
            fclose (temp_file);
            temp_file = NULL;

            func = JS_CompileFunction(cx, NULL, NULL, 0, NULL, 
                                      buffer, file_size,
                                      full_path, 0);
            if (NULL == func) {
                EDJS_ERR(cx, EDJSERR_COMPILING_MODULE, JS_GetStringBytes(file_str));
                goto error;
            }

            EDJS_free(cx, buffer);
            buffer = NULL;

            if (JS_FALSE == JS_GetProperty(cx, JS_GetFunctionObject(func), "prototype", rval)) {
                EDJS_ERR(cx, EDJSERR_GET_PROPERTY, JS_GetStringBytes(file_str), "prototype");
                goto error;
            }

            constructed_obj = JS_ConstructObject(cx, JS_GET_CLASS(cx, JS_GetFunctionObject(func)), JSVAL_TO_OBJECT(*rval), NULL);
            if (NULL == constructed_obj) {
                EDJS_ERR(cx, EDJSERR_INITING_MODULE, JS_GetStringBytes(file_str));
                goto error;
            }
            argv[argc+1] = OBJECT_TO_JSVAL(constructed_obj);

            if (JS_FALSE == JS_CallFunction(cx, constructed_obj, func, 0, NULL, rval)) {
                EDJS_ERR(cx, EDJSERR_INITING_MODULE, JS_GetStringBytes(file_str));
                goto error;
            }

            module_data->dl_lib = NULL;
        }
        else { //binary file
            module_data->type = EDJS_BINARY;

            module_data->dl_lib = (void *)dlopen(full_path, RTLD_NOW);
            if (NULL == module_data->dl_lib) {
                EDJS_ERR(cx, EDJSERR_OPENING_MODULE, JS_GetStringBytes(file_str));
                goto error;
            }
            plugin_init = dlsym(module_data->dl_lib, "edjs_init");
            if (NULL == plugin_init) {
                EDJS_ERR(cx, EDJSERR_LOCATING_MODULE_INIT, JS_GetStringBytes(file_str));
                goto error;
            }


            constructed_obj = NULL;
            if (JS_FALSE == plugin_init(cx, &constructed_obj)) {
                EDJS_ERR(cx, EDJSERR_INITING_MODULE, JS_GetStringBytes(file_str));
                goto error;
            }
            argv[argc+1] = OBJECT_TO_JSVAL(constructed_obj);
        }
        module_data->constructed_obj = constructed_obj;
    }

    if (argc > 1) {
        if (JS_TRUE == JSVAL_IS_STRING(argv[1])) {
            if (JS_FALSE == JS_DefineProperty(cx, obj,
                                              JS_GetStringBytes(JSVAL_TO_STRING(argv[1])), OBJECT_TO_JSVAL(constructed_obj), 
                                              NULL, NULL, JSPROP_ENUMERATE)) {
                EDJS_ERR(cx, EDJSERR_SET_PROPERTY, "object", JS_GetStringBytes(JSVAL_TO_STRING(argv[1])));
                goto error;
            }
        }
        else if (JS_TRUE == JSVAL_IS_OBJECT(argv[1]) && JS_FALSE == JSVAL_IS_NULL(argv[1])) {
            import_obj = JSVAL_TO_OBJECT(argv[1]);
            if (JS_TRUE == JS_IsArrayObject(cx, import_obj)) {
                if (JS_FALSE == JS_GetArrayLength(cx, import_obj, &import_len)) {
                    EDJS_ERR(cx, EDJSERR_ARRAY_LEN);
                    goto error;
                }

                for(i = 0; i < import_len; i++) {
                    if (JS_FALSE == JS_GetElement(cx, import_obj, i, &import_val)) {
                        EDJS_ERR(cx, EDJSERR_ARRAY_RETRIEVAL);
                        goto error;
                    }

                    import_str = JS_ValueToString(cx, import_val);
                    if (NULL == import_str) {
                        EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
                        goto error;
                    }
                    argv[argc+2] = STRING_TO_JSVAL(import_str);
                    if (JS_FALSE == JS_HasProperty(cx, constructed_obj, JS_GetStringBytes(import_str), &found_prop_b)) {
                        EDJS_ERR(cx, EDJSERR_HAS_PROPERTY, JS_GetStringBytes(file_str), JS_GetStringBytes(import_str));
                        goto error;
                    }

                    if (JS_FALSE == found_prop_b) {
                        EDJS_ERR(cx, EDJSERR_IMPORTING_MODULE_PROP, JS_GetStringBytes(file_str), JS_GetStringBytes(import_str));
                        goto error;
                    }

                    if (JS_FALSE ==  JS_GetProperty(cx, constructed_obj, JS_GetStringBytes(import_str), &import_val)) {
                        EDJS_ERR(cx, EDJSERR_GET_PROPERTY, JS_GetStringBytes(file_str), JS_GetStringBytes(import_str));
                        goto error;
                    }

                    if (JS_FALSE == JS_DefineProperty(cx, obj,
                                                      JS_GetStringBytes(import_str), import_val, 
                                                      NULL, NULL, JSPROP_ENUMERATE)) {
                        EDJS_ERR(cx, EDJSERR_SET_PROPERTY, "object", JS_GetStringBytes(import_str));
                        goto error;
                    }
                }
            }
            else {
                import_obj_ids =  JS_Enumerate(cx, import_obj);
                if (NULL == import_obj_ids) {
                    EDJS_ERR(cx, EDJSERR_ENUMERATE_OBJ, JS_GetStringBytes(file_str));
                    goto error;
                }

                for (i = 0; i < import_obj_ids->length; i++) {
                    if (JS_FALSE == JS_IdToValue(cx, import_obj_ids->vector[i], &import_val)) {
                        EDJS_ERR(cx, EDJSERR_ID_TO_VAL);
                        goto error;
                    }

                    import_str = JS_ValueToString(cx, import_val);
                    if (NULL == import_str) {
                        EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
                        goto error;
                    }
                    argv[argc+2] = STRING_TO_JSVAL(import_str);

                    if (JS_FALSE == JS_HasProperty(cx, constructed_obj, JS_GetStringBytes(import_str), &found_prop_b)) {
                        EDJS_ERR(cx, EDJSERR_HAS_PROPERTY, JS_GetStringBytes(file_str), JS_GetStringBytes(import_str));
                        goto error;
                    }

                    if (JS_FALSE == found_prop_b) {
                        EDJS_ERR(cx, EDJSERR_IMPORTING_MODULE_PROP, JS_GetStringBytes(file_str), JS_GetStringBytes(import_str));
                        goto error;
                    }

                    if (JS_FALSE ==  JS_GetProperty(cx, constructed_obj, JS_GetStringBytes(import_str), &import_val)) {
                        EDJS_ERR(cx, EDJSERR_GET_PROPERTY, JS_GetStringBytes(file_str), JS_GetStringBytes(import_str));
                        goto error;
                    }

                    if (JS_FALSE == JS_GetProperty(cx, import_obj, JS_GetStringBytes(import_str), &import_alias_val)) {
                        EDJS_ERR(cx, EDJSERR_GET_PROPERTY, "AliasObj", JS_GetStringBytes(import_str));
                        goto error;
                    }

                    import_alias_str = JS_ValueToString(cx, import_alias_val);
                    if (NULL == import_alias_str) {
                        EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
                        goto error;
                    }
                    argv[argc+3] = STRING_TO_JSVAL(import_alias_str);

                    if (JS_FALSE == JS_DefineProperty(cx, obj,
                                                      JS_GetStringBytes(import_alias_str), import_val, 
                                                      NULL, NULL, JSPROP_ENUMERATE)) {
                        EDJS_ERR(cx, EDJSERR_SET_PROPERTY, "object", JS_GetStringBytes(import_alias_str));
                        goto error;
                    }
                }
            }
        }
        else {
            EDJS_ERR(cx, EDJSERR_VAR_TYPE, "imp() arg 2", "string, array, or object");
            goto error;
        }
    }
    else {
        if (NULL != buffer) {
            EDJS_free(cx, buffer);
            buffer = NULL;
        }

        buffer = EDJS_malloc(cx, (strlen(JS_GetStringBytes(file_str)) + 1) * sizeof(char)); //+1 for '\0'
        if (NULL == buffer) {
            goto error;
        }
        
        strcpy(buffer, JS_GetStringBytes(file_str));
        it3 = strrchr(buffer, '.'); //full_path is now a pointer to the last '.' in the string
        if (NULL == it3) {
            it3 = buffer;
        }
        else {
            it3 += sizeof(char); //move full_path to the last token in string
        }

        it = strtok_r(buffer, ".", &it2);

        import_obj = obj;

        while (NULL != it) {
            if (it == it3) {
                if (JS_FALSE == JS_DefineProperty(cx, import_obj,
                                                  it, OBJECT_TO_JSVAL(constructed_obj), 
                                                  NULL, NULL, JSPROP_ENUMERATE)) {
                    EDJS_ERR(cx, EDJSERR_SET_PROPERTY, "object", it);
                    goto error;
                }
            }
            else {
                if (JS_FALSE == JS_GetProperty(cx, import_obj, it, &import_val)) {
                    EDJS_ERR(cx, EDJSERR_GET_PROPERTY, "object", it);
                    goto error;
                }

                if (JS_FALSE == JSVAL_IS_OBJECT(import_val) || JS_TRUE == JSVAL_IS_NULL(import_val)) {
                    if (JS_FALSE == JS_DefineObject(cx, import_obj, it, NULL, NULL, JSPROP_ENUMERATE)) {
                        EDJS_ERR(cx, EDJSERR_SET_PROPERTY, "object", it);
                        goto error;
                    }

                    if (JS_FALSE == JS_GetProperty(cx, import_obj, it, &import_val)) {
                        EDJS_ERR(cx, EDJSERR_GET_PROPERTY, "object", it);
                        goto error;
                    }

                    if (JS_FALSE == JSVAL_IS_OBJECT(import_val) || JS_TRUE == JSVAL_IS_NULL(import_val)) {
                        EDJS_ERR(cx, EDJSERR_MODULE_CHAIN_TYPE, it);
                        goto error;
                    }
                    import_obj = JSVAL_TO_OBJECT(import_val);
                }
            }
            it = strtok_r(NULL, ".", &it2);
        }
    }

    if (existing_node != module_node) {
        p->imported_modules = edjs_TreeNodeInsert(existing_node, module_node, edjs_imp_module_comparator);
        rt_private = (edjs_runtime_private *)JS_GetRuntimePrivate(JS_GetRuntime(cx));
        module_list_node = (edjs_list_node *)EDJS_malloc(cx, sizeof(edjs_list_node));
        module_list_node->value = (void *)module_node;
        pthread_mutex_lock(&(rt_private->mutex));
        module_list_node->next = rt_private->imported_objects;
        rt_private->imported_objects = module_list_node;
        pthread_mutex_unlock(&(rt_private->mutex));
    }

    goto finish;
 error:
    ok = JS_FALSE;
    *rval = JSVAL_VOID;

    if (NULL != module_node) {
        edjs_imp_module_destroy(cx, module_node->value);
        EDJS_free(cx, module_node);
        module_node = NULL;
    }

 finish:
    if (NULL != buffer) {
        EDJS_free(cx, buffer);
        buffer = NULL;
    }

    if (NULL != temp_file) {
        fclose (temp_file);
        temp_file = NULL;
    }

    if (NULL != file_path) {
        EDJS_free(cx, file_path);
        file_path = NULL;
    }

    JS_RemoveRoot(cx, &func);

    JS_EndRequest(cx);

    return ok;
}

JSBool EDJS_ResolveFile(JSContext *cx, JSObject *path_array, const char **extension, const char *search, char **result, struct stat **file_stat) {

    JSBool ok = JS_TRUE;
    jsuint j, path_array_len;
    size_t include_len = 0;
    JSString *path_str = NULL;
    jsval path_val = JSVAL_VOID;
    const char **it = NULL;
    char *temp_c = NULL;
    char *search_path = NULL;

    *result = NULL;
    *file_stat = NULL;

    if (JS_FALSE == JS_AddRoot(cx, &path_str)) {
        EDJS_ERR(cx, EDJSERR_ADD_ROOT);
        goto error;
    }

    if (JS_FALSE == JS_GetArrayLength(cx, path_array, &path_array_len)) {
        EDJS_ERR(cx, EDJSERR_ARRAY_LEN);
        goto error;
    }

    if (NULL != extension) {
        it = extension;
        while (NULL != *it) {
            if (include_len < strlen(*it)) //find the max extension length
                include_len = strlen(*it);
            it++;
        }
    }
    include_len += strlen(search) + 3; //2 for '.', '/' and '\0'

    *file_stat = EDJS_malloc(cx, sizeof(struct stat));
    if (NULL == *file_stat) {
        goto error;
    }

    if ('/' == search[0]) { //absolute path requested
        *result = (char *)EDJS_malloc(cx, include_len);
        if (NULL == *result) {
            goto error;
        }

        strcpy(*result, search);

        if (NULL != extension) {
            temp_c = strrchr (*result, '\0'); //make temp_c point to the end of the string, where we will put the extension
            temp_c[0] = '.';

            it = extension;
            while (NULL != *it) {
                strcpy (temp_c+sizeof(char), *it);
                /*
                if (0 == access(*result, F_OK))
                    goto finish;
                */
                if (0 == stat(*result, *file_stat))
                    goto finish;
                it++;
            }
        }
        else {
            //            if (0 == access(*result, F_OK))
            if (0 == stat(*result, *file_stat))
                goto finish;
        }

        goto error;
    }
    else {
        for (j = 0; j < path_array_len; j++) {
            if (JS_FALSE == JS_GetElement(cx, path_array, j, &path_val)) {
                EDJS_ERR(cx, EDJSERR_ARRAY_RETRIEVAL);
                goto error;
            }

            path_str = JS_ValueToString(cx, path_val);
            if (NULL == path_str) {
                EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
                goto error;
            }

            //assumes that realpath with allocate a buff of proper size using malloc
            search_path = realpath(JS_GetStringBytes(path_str), NULL);
            if (NULL == search_path) {
                switch(errno) {
                case EACCES:
                case ENOENT:
                case ENOTDIR:
                    //simply means we couldn't resolve the file
                    break;

                case EINVAL:
                    EDJS_ERR(cx, EDJSERR_VAR_TYPE, "file name");
                    goto error;
                    break;

                case ENOMEM:
                    JS_ReportOutOfMemory(cx);
                    goto error;
                    break;

                case ENAMETOOLONG:
                    EDJS_ERR(cx, EDJSERR_FILENAME_LENGTH, JS_GetStringBytes(path_str));
                    goto error;
                    break;

                case EIO:
                    EDJS_ERR(cx, EDJSERR_DISK_IO);
                    goto error;
                    break;

                case ELOOP:
                    EDJS_ERR(cx, EDJSERR_FILENAME_LOOP, JS_GetStringBytes(path_str));
                    goto error;
                    break;

                default:
                    EDJS_ERR(cx, EDJSERR_UNKNOWN_ERROR);
                    goto error;
                }
            }
            else {
                *result = (char *)EDJS_malloc(cx, include_len + strlen(search_path));
                if (*result == NULL) {
                    goto error;
                }

                strcpy(*result, search_path);
                strcat(*result, "/");
                strcat(*result, search);
                if (NULL != extension) {
                    temp_c = strrchr (*result, '\0'); //make temp_c point to the end of the string, where we will put the extension
                    temp_c[0] = '.';

                    it = extension;
                    while (NULL != *it) {
                        strcpy (temp_c+sizeof(char), *it);
                        //                        if (0 == access(*result, F_OK))
                        if (0 == stat(*result, *file_stat))
                            goto finish;
                        it++;
                    }
                }
                else {
                    //                    if (0 == access(*result, F_OK))
                    if (0 == stat(*result, *file_stat))
                        goto finish;
                }
            }
        }
        goto error;
    }

    goto finish;
 error:
    ok = JS_FALSE;
    if (NULL != *result) {
        EDJS_free(cx, *result);
        *result = NULL;
    }

    if (NULL != *file_stat) {
        EDJS_free(cx, *file_stat);
        *file_stat = NULL;
    }

 finish:
    JS_RemoveRoot(cx, &path_str);
    if (NULL != search_path) {
        free(search_path);
        search_path = NULL;
    }

    return ok;

}

static JSBool edjs_Include(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    //expects 1 argument
    //has 3 extra argv roots

    JSBool ok = JS_TRUE;
    jsuint i, includes_len;
    JSString *include_str = NULL;
    JSScript *script = NULL;
    JSObject *array_obj = NULL;
    JSObject *script_obj = NULL;
    jsval path_val = JSVAL_VOID;
    JSObject *path_obj = NULL;
    edjs_private *p = NULL;
    char *path = NULL;
    struct stat *file_stat = NULL;

    uint32 oldopts;

    if (argc < 1) {
        EDJS_ERR(cx, EDJSERR_NUM_ARGS, "include");
        goto error;
    }

    JS_BeginRequest(cx);

    p = (edjs_private *)JS_GetContextPrivate(cx);
    if (NULL == p) {
        EDJS_ERR(cx, EDJSERR_PRIVATE_DATA_NOT_SET);
        goto error;
    }


    oldopts = JS_GetOptions(cx);
    JS_SetOptions(cx, oldopts | JSOPTION_COMPILE_N_GO);


    //turn the first argument into an array if it isn't one
    if (JS_TRUE == JSVAL_IS_OBJECT(argv[0]) && JS_FALSE == JSVAL_IS_NULL(argv[0]) && 
        JS_IsArrayObject(cx, JSVAL_TO_OBJECT(argv[0]))) {

        array_obj = JSVAL_TO_OBJECT(argv[0]);
    }
    else {
        include_str = JS_ValueToString(cx, argv[0]);
        argv[argc] = STRING_TO_JSVAL(include_str);//roots str

        if (NULL == include_str) {
            EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
            goto error;
        }
        array_obj = JS_NewArrayObject(cx, 0, NULL);
        if (NULL == array_obj) {
            EDJS_ERR(cx, EDJSERR_CREATING_ARRAY);
            goto error;
        }
        argv[0] = OBJECT_TO_JSVAL(array_obj);

        if (JS_FALSE == JS_SetElement(cx, array_obj, 0, &(argv[argc]))) {
            EDJS_ERR(cx, EDJSERR_SET_PROPERTY, "ArrayObj");
            goto error;
        }
    }

    if (JS_FALSE == JS_GetArrayLength(cx, array_obj, &includes_len)) {
        EDJS_ERR(cx, EDJSERR_ARRAY_LEN);
        goto error;
    }

    if (JS_FALSE == JS_GetProperty(cx, p->settings_obj, "include_path", &path_val)) {
        EDJS_ERR(cx, EDJSERR_GET_PROPERTY, "EDJS", "include_path");
        goto error;
    }

    if (JS_FALSE == JSVAL_IS_OBJECT(path_val) || JS_TRUE == JSVAL_IS_NULL(path_val) ||
        JS_FALSE == JS_IsArrayObject(cx, JSVAL_TO_OBJECT(path_val))) {
        EDJS_ERR(cx, EDJSERR_VAR_TYPE, "include_path", "array");
        goto error;
    }

    path_obj = JSVAL_TO_OBJECT(path_val);

    for(i = 0; i < includes_len; i++) {
        if (JS_FALSE == JS_GetElement(cx, array_obj, i, &argv[argc])) {
            EDJS_ERR(cx, EDJSERR_ARRAY_RETRIEVAL);
            goto error;
        }

        include_str = JS_ValueToString(cx, argv[argc]);
        if (NULL == include_str) {
            EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
            goto error;
        }
        argv[argc] = STRING_TO_JSVAL(include_str); //roots str


        if (JS_FALSE == EDJS_ResolveFile(cx, path_obj, NULL, 
                                         JS_GetStringBytes(include_str), &path, &file_stat)) {
            goto error;
        }

        if (NULL == path) {
            EDJS_ERR(cx, EDJSERR_FILENAME_RESOLVE, JS_GetStringBytes(include_str));
            goto error;
        }

        script = JS_CompileFile(cx, obj, path);
        if (NULL == script) {
            goto error;
        }
        script_obj = JS_NewScriptObject(cx, script);
        if (NULL == script_obj) {
            JS_DestroyScript(cx, script);
            EDJS_ERR(cx, EDJSERR_CREATING_SCRIPT_OBJ);
            goto error;
        }
        argv[argc+2] = OBJECT_TO_JSVAL(script_obj);

        if (JS_FALSE == JS_ExecuteScript(cx, obj, script, &argv[argc])) {
            goto error;
        }
        
        JS_free(cx, path);
        path = NULL;
    }

    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    if (NULL != path) {
        JS_free(cx, path);
        path = NULL;
    }

    if (NULL != file_stat) {
        JS_free(cx, file_stat);
        file_stat = NULL;
    }

    JS_SetOptions(cx, oldopts);
    JS_EndRequest(cx);

    return ok;
}

static const char *edjs_ParseJSONValue(const char *str_char) {
    const char *it = str_char;
    int i;

    //assignment hides meaningless warning
    i = (str_char == (it = edjs_ParseJSONNumber(str_char))) &&
    (str_char == (it = edjs_ParseJSONString(str_char))) &&
    (str_char == (it = edjs_ParseJSONArray(str_char))) &&
    (str_char == (it = edjs_ParseJSONConstant(str_char))) &&
    (str_char == (it = edjs_ParseJSONObject(str_char)));

    return it;
}

static const char *edjs_ParseJSONObject(const char *str_char) {
    const char *it = str_char;
    const char *tmp;
    int i;

    if ('{' != *it)
        goto error;

    do {
        do {
            it += sizeof(char);
        } while (' ' == *it && '\0' != *it); //fix me: allow all whitespace characters

        if ('\0' == *it)
            goto error;

        tmp = it;
        //note: json spec has anything non-string as an error. we allow for non-quoted identifiers, constants and numbers
        //assignment hides meaningless warning
        i = (tmp != (it = edjs_ParseJSONString(it))) ||
        (tmp != (it = edjs_ParseJSONNumber(it))) ||
        (tmp != (it = edjs_ParseJSONConstant(it))) ||
        (tmp != (it = edjs_ParseJSONIdentifier(it)));
        if (tmp == it)
            goto error;

        while (' ' == *it && '\0' != *it) {//fix me: allow all whitespace characters
            it += sizeof(char);
        }

        if ('\0' == *it)
            goto error;

        if (':' != *it)
            goto error;
        
        do {
            it += sizeof(char);
        } while (' ' == *it && '\0' != *it); //fix me: allow all whitespace characters
            
        it = edjs_ParseJSONValue(it); //note: json spec does not allow empty elements. we do.
            
        while (' ' == *it && '\0' != *it) {//fix me: allow all whitespace characters
            it += sizeof(char);
        }
    } while (',' == *it && '\0' != *it);

    if ('}' != *it)
        goto error;

    it += sizeof(char);

    return it;
 error:
    return str_char;
}

static const char *edjs_ParseJSONArray(const char *str_char) {
    const char *it = str_char;

    if ('[' != *it)
        goto error;

    do {
        do {
            it += sizeof(char);
        } while (' ' == *it && '\0' != *it); //fix me: allow all whitespace characters

        if ('\0' == *it)
            goto error;

        it = edjs_ParseJSONValue(it); //note: json spec does not allow empty elements. we do.

        while (' ' == *it && '\0' != *it) {//fix me: allow all whitespace characters
            it += sizeof(char);
        }

        if ('\0' == *it)
            goto error;

    } while (',' == *it && '\0' != *it);

    if (']' != *it)
        goto error;

    it += sizeof(char);
    
    return it;
 error:
    return str_char;
}

static const char *edjs_ParseJSONString(const char *str_char) {
    const char *it = str_char;
    const char first = *it;

    if ('"' != first && '\'' != first) //note: json spec does not allow for strings that start with single quote
        goto error;

    do {
        it += sizeof(char);
        if ('\\' == *it) {
            it += sizeof(char);
            if ('"' != *it &&
                '\'' != *it &&  //note: json spec does not allow for single quote after backslash
                '\\' != *it &&
                '/' != *it &&
                'b' != *it &&
                'f' != *it &&
                'n' != *it &&
                'r' != *it &&
                't' != *it &&
                'u' != *it)
                goto error;
            if ('u' != *it) {
                it += sizeof(char);
            }
            else {
                it += sizeof(char);
                if (('0' > *it || '9' < *it) && ('a' > *it || 'f' < *it) && ('A' > *it || 'F' < *it)) //fix me: should not use a range like this
                    goto error;
                it += sizeof(char);
                if (('0' > *it || '9' < *it) && ('a' > *it || 'f' < *it) && ('A' > *it || 'F' < *it))
                    goto error;
                it += sizeof(char);
                if (('0' > *it || '9' < *it) && ('a' > *it || 'f' < *it) && ('A' > *it || 'F' < *it))
                    goto error;
                it += sizeof(char);
                if (('0' > *it || '9' < *it) && ('a' > *it || 'f' < *it) && ('A' > *it || 'F' < *it))
                    goto error;
                it += sizeof(char);                
            }
        }
    } while (first != *it && '\0' != *it);

    if (first != *it)
        goto error;
    it += sizeof(char);

    
    return it;
 error:
    return str_char;
}

static const char *edjs_ParseJSONNumber(const char *str_char) {
    const char *it = str_char;

    if ('-' == *it)
        it += sizeof(char);

    if ('0' <= *it && '9' >= *it) { //fix me: should not use a range like this
        if ('0' != *it) { //if it begins with 0, it has to be followed by a '.' or nothing
            do {
                it += sizeof(char);
            } while ('0' <= *it && '9' >= *it && '\0' != *it); //fix me: should not use a range like this
        }
        else 
            it += sizeof(char);

        if ('.' == *it) {
            do {
                it += sizeof(char);
            } while ('0' <= *it && '9' >= *it && '\0' != *it); //fix me: should not use a range like this
        }

        if ('e' == *it || 'E' == *it) {
            it += sizeof(char);
            if ('+' == *it || '-' == *it) {
                it += sizeof(char);
            }
            if ('0' <= *it && '9' >= *it) { //fix me: should not use a range like this
                do {
                    it += sizeof(char);
                } while ('0' <= *it && '9' >= *it && '\0' != *it); //fix me: should not use a range like this
            }
            else
                goto error;
        }
    }
    else
        goto error;

    return it;
 error:
    return str_char;
}


//note: json spec does not allow for identifiers like this
static const char *edjs_ParseJSONIdentifier(const char *str_char) {
    const char *it = str_char;


    if (('a' <= *it && 'z' >= *it) || ('A' <= *it && 'Z' >= *it) || '_' == *it || '$' == *it) {
        do {
            it += sizeof(char);
        } while ((('a' <= *it && 'z' >= *it) || ('A' <= *it && 'Z' >= *it) || 
                  ('0' <= *it && '9' >= *it) || '_' == *it || '$' == *it) && '\0' != *it);
    }

    //no real error case here. if it fails to find an identifier, it simply doesn't advance it

    return it;
}

static const char *edjs_ParseJSONConstant(const char *str_char) {
    const char *it = str_char;

    if (0 == strcmp (it, "true")) {
        it += 4 * sizeof(char);
    }
    else if (0 == strcmp(it, "false")) {
        it += 5 * sizeof(char);
    }
    else if (0 == strcmp(it, "null")) {
        it += 4 * sizeof(char);
    }
    //note: json spec doesn't allow for undefined as a constant. we do.
    else if (0 == strcmp(it, "undefined")) {
        it += 9 * sizeof(char);
    }

    //no real error case here. if it fails to find an identifier, it simply doesn't advance it

    return it;
}

static JSBool edjs_IsJSON(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    //expects 0-1 arguments
    //has 1 extra argv roots

    JSBool ok = JS_TRUE;
    JSString *str = NULL;
    JSObject *global = JS_GetGlobalObject(cx);
    const char *str_char = NULL;

    JS_BeginRequest(cx);

    *rval = JSVAL_TRUE;

    //if isJSON was called on something other than our global object,
    //i.e. 'asdf'.isJSON();
    if (global != obj) {
        str = JS_ValueToString(cx, OBJECT_TO_JSVAL(obj));
        if (NULL == str) {
            EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
            goto error;
        }

        argv[argc] = STRING_TO_JSVAL(str);
    }
    else if (0 == argc) {
        EDJS_ERR(cx, EDJSERR_NUM_ARGS, "isJSON");
        goto error;
    }
    else {
        if (JS_TRUE == JSVAL_IS_STRING(argv[0]))
            argv[argc] = argv[0];
        else {
            str = JS_ValueToString(cx, argv[0]);
            if (NULL == str) {
                EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
                goto error;
            }
            
            argv[argc] = STRING_TO_JSVAL(str);
        }
    }

    str_char = JS_GetStringBytes(JSVAL_TO_STRING(argv[argc]));
    if (NULL != str_char) {
        str_char = edjs_ParseJSONValue(str_char);
        if ('\0' != *str_char)
            *rval = JSVAL_FALSE;
    }
    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    JS_EndRequest(cx);

    return ok;
}

static JSBool edjs_ExtractJSON(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    //expects 0-1 arguments
    //has 1 extra argv roots

    JSBool ok = JS_TRUE;
    JSString *str = NULL;
    JSObject *global = JS_GetGlobalObject(cx);
    const char *str_char = NULL;
    const char *end_char = NULL;

    JS_BeginRequest(cx);

    //if isJSON was called on something other than our global object,
    //i.e. 'asdf'.isJSON();
    if (global != obj) {
        str = JS_ValueToString(cx, OBJECT_TO_JSVAL(obj));
        if (NULL == str) {
            EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
            goto error;
        }

        argv[argc] = STRING_TO_JSVAL(str);
    }
    else if (0 == argc) {
        EDJS_ERR(cx, EDJSERR_NUM_ARGS, "extractJSON");
        goto error;
    }
    else {
        if (JS_TRUE == JSVAL_IS_STRING(argv[0]))
            argv[argc] = argv[0];
        else {
            str = JS_ValueToString(cx, argv[0]);
            if (NULL == str) {
                EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
                goto error;
            }
            
            argv[argc] = STRING_TO_JSVAL(str);
        }
    }

    str_char = JS_GetStringBytes(JSVAL_TO_STRING(argv[argc]));
    end_char = edjs_ParseJSONValue(str_char);
    str = JS_NewStringCopyN(cx, str_char, end_char-str_char);

    *rval = STRING_TO_JSVAL(str);

    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    JS_EndRequest(cx);

    return ok;
}

static JSBool edjs_StringToJSON(JSContext *cx, jsval arg, jsval *rval) {
    JSBool ok = JS_TRUE;
    JSString *str = NULL;
    char *buff = NULL;
    char *new_buff = NULL;
    int len = 0;

    if (JS_FALSE == JS_AddRoot(cx, &str)) {
        EDJS_ERR(cx, EDJSERR_ADD_ROOT);
        goto error;
    }

    str = JS_ValueToString(cx, arg);

    if (NULL == str) {
        EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
        goto error;
    }
    buff = JS_GetStringBytes(str);
    if (JS_FALSE == edjs_QuoteString(cx, buff, &new_buff, &len)) {
        goto error;
    }
    str = JS_NewString(cx, new_buff, len);
    if (NULL == str)
        goto error;
    *rval = STRING_TO_JSVAL(str);

    goto finish;
 error:
    ok = JS_FALSE;
    if (NULL != new_buff)
        JS_free(cx, new_buff);
 finish: 
    JS_RemoveRoot(cx, &str);
    return ok;
}

static JSBool edjs_ArrayToJSON(JSContext *cx, JSObject *obj, jsval *rval) {
    JSBool ok = JS_TRUE;

    char *result = NULL; //contains the resulting json string that we use as our return value
    char *tresult = NULL;
    char *tchar = NULL;
    char *i_buff = NULL;
    int i_buff_len = 32, i_buff_new_len = 0;
    jsuint json_length = 3; //length of the result. starts as "[]\0"
    jsuint a_len;
    jsuint i;
    jsval a_val = JSVAL_VOID;
    jsval cur_json_val = JSVAL_VOID;
    JSString *str = NULL;


    if (JS_FALSE == JS_AddRoot(cx, &a_val) ||
        //JS_FALSE == JS_AddRoot(cx, &a_val_obj) ||
        JS_FALSE == JS_AddRoot(cx, &cur_json_val) ||
        JS_FALSE == JS_AddRoot(cx, &str)) {
        EDJS_ERR(cx, EDJSERR_ADD_ROOT);
        goto error;
    }

       
    if (JS_FALSE == JS_GetArrayLength(cx, obj, &a_len)) {
        EDJS_ERR(cx, EDJSERR_ARRAY_LEN);
        goto error;
    }

    //allocate 3 bytes for []\0 as well as 2 bytes for each ", " separating elements
    json_length += (a_len == 0 ? 0 : (a_len - 1) * 2);
    result = JS_malloc(cx, json_length * sizeof(char));
    if (NULL == result) {
        goto error;
    }
    *(result) = '[';
    *(result+1) = '\0';
    *(result+2) = '\0';

    i_buff = JS_malloc(cx, (i_buff_len+1) * sizeof(char));
    if (NULL == i_buff) {
        goto error;
    }

    for (i = 0; i < a_len; i++) {
        do {
            if (i_buff_new_len > i_buff_len) {
                JS_free(cx, i_buff);
                i_buff = JS_malloc(cx, (i_buff_new_len+1) * sizeof(char));
                if (NULL == i_buff) {
                    goto error;
                }
                i_buff_len = i_buff_new_len;
            }

            i_buff_new_len = snprintf(i_buff, i_buff_len, "%d", i);
        } while (i_buff_new_len > i_buff_len);

        if (JS_FALSE == JS_GetProperty(cx, obj, i_buff, &a_val)) {
            goto error;
        }
        if (i != 0)
            strcat(result, ", ");

        if (JSVAL_VOID == a_val) //undefined means an empty entry
            continue;
        if (JS_FALSE == edjs_ToJSON(cx, obj, 1, &a_val, &cur_json_val)) {
            goto error;
        }
        
        str = JS_ValueToString(cx, cur_json_val);
        if (NULL == str) {
            goto error;
        }
        
        tchar = JS_GetStringBytes(str);
        json_length += (strlen(tchar) * sizeof(char));
        tresult = (char *)JS_realloc(cx, result, json_length * sizeof(char));
        if (tresult == NULL) {
            goto error;
        }
        result = tresult;
        strcat(result, tchar);
    }
    strcat(result, "]");
    *rval = STRING_TO_JSVAL(JS_NewString(cx, result, json_length - 1));

    goto finish;
 error:
    ok = JS_FALSE;
    if (NULL != result)
        JS_free(cx, result);
 finish:
    if (NULL != i_buff) {
        JS_free(cx, i_buff);
        i_buff = NULL;
    }
    JS_RemoveRoot(cx, &a_val);
    JS_RemoveRoot(cx, &cur_json_val);
    JS_RemoveRoot(cx, &str);

    return ok;
}

static JSBool edjs_ObjectToJSON(JSContext *cx, JSObject *obj, jsval *rval) {
    JSBool ok = JS_TRUE;

    char *result = NULL; //contains the resulting json string that we use as our return value
    char *tresult = NULL; //temporary holder for error checking
    char *tchar = NULL; //temporary holder for quoting strings
    int json_length = 3; //length of the result. starts as "{}\0"
    int i = 0; //the number or properties that we know about
    JSBool first_prop = JS_TRUE;
    JSObject *it = NULL;
    JSString *str = NULL;
    jsid prop_id = JSVAL_VOID;
    jsval prop_name = JSVAL_VOID;
    jsval prop_val = JSVAL_VOID;
    jsval json_val = JSVAL_VOID; //pointer to keep track of where we are in json_vals
    int len = 0; //used for holding misc string lengths
    JSIdArray *id_a = NULL;



    if (JS_FALSE == JS_AddRoot(cx, &str) ||
        JS_FALSE == JS_AddRoot(cx, &prop_name) ||
        JS_FALSE == JS_AddRoot(cx, &prop_val) ||
        //JS_FALSE == JS_AddRoot(cx, &prop_obj) ||
        JS_FALSE == JS_AddRoot(cx, &it) ||
        JS_FALSE == JS_AddRoot(cx, &json_val)) {
        EDJS_ERR(cx, EDJSERR_ADD_ROOT);
        goto error;
    }

    //allocate 3 bytes for {}\0
    result = JS_malloc(cx, json_length * sizeof(char));
    if (NULL == result) {
        goto error;
    }

    *(result) = '{';
    *(result+1) = '\0';
    *(result+2) = '\0';

    id_a = JS_Enumerate(cx, obj);
    if (NULL == id_a) {
        //report error
        goto error;
    }

    for(i = 0; i < id_a->length; i++) {
        prop_id = id_a->vector[i];
        //convert the property to a value
        if (JS_FALSE == JS_IdToValue(cx, prop_id, &prop_name)) {
            goto error;
        }

        str = JS_ValueToString(cx, prop_name);
        if (NULL == str) {
            goto error;
        }

        if (JS_FALSE == edjs_QuoteString(cx, JS_GetStringBytes(str), &tchar, &len)) {
            goto error;
        }

        json_length += (strlen(tchar) + 2 + (JS_TRUE != first_prop ? 2 : 0)); //+2 for the colon and space. +2 for comma and space
        tresult = (char *)JS_realloc(cx, result, json_length * sizeof(char));
        if (NULL == tresult) {
            goto error;
        }
        result = tresult;
        if (JS_TRUE != first_prop)
            strcat(result, ", ");
        else
            first_prop = JS_FALSE;
        strcat(result, tchar);
        strcat(result, ": ");
        JS_free(cx, tchar);
        tchar = NULL;

        if (JS_FALSE == JS_GetProperty(cx, obj, JS_GetStringBytes(str), &prop_val)) {
            goto error;
        }

        if (JS_FALSE == edjs_ToJSON(cx, obj, 1, &prop_val, &json_val))
            goto error;

        str = JS_ValueToString(cx, json_val);
        if (NULL == str) {
            goto error;
        }

        tchar = JS_GetStringBytes(str);
        json_length += strlen(tchar);
        tresult = (char *)JS_realloc(cx, result, json_length * sizeof(char));
        if (NULL == tresult) {
            tchar = NULL;
            goto error;
        }
        result = tresult;
        strcat(result, tchar);
        tchar = NULL;
    }

    strcat(result, "}");
        
    *rval = STRING_TO_JSVAL(JS_NewString(cx, result, json_length-1));

    goto finish;
 error:
    ok = JS_FALSE;
    if (NULL != result)
        JS_free(cx, result);
    if (NULL != tchar)
        JS_free(cx, tchar);

 finish: 
    JS_RemoveRoot(cx, &str);
    JS_RemoveRoot(cx, &prop_name);
    JS_RemoveRoot(cx, &prop_val);
    JS_RemoveRoot(cx, &it);
    JS_RemoveRoot(cx, &json_val);
    return ok;
}

static JSBool edjs_ToJSON(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;

    JS_BeginRequest(cx);
    JS_YieldRequest(cx); //might be called recursively, so attempt to yield

    if (1 > argc) {
        EDJS_ERR(cx, EDJSERR_NUM_ARGS, "JSON.stringify");
        goto error;
    }

    switch(JS_TypeOfValue(cx, *argv)) {
    case JSTYPE_VOID:
    case JSTYPE_BOOLEAN:
    case JSTYPE_NUMBER:
    case JSTYPE_NULL:
        *rval = STRING_TO_JSVAL(JS_ValueToString(cx, *argv));
        break;

        
    case JSTYPE_XML:
    case JSTYPE_STRING:
        if (JS_FALSE == edjs_StringToJSON(cx, *argv, rval))
            goto error;

        break;
    case JSTYPE_FUNCTION:
    case JSTYPE_OBJECT:
    default:
        if (JS_TRUE == JSVAL_IS_NULL(*argv)) {
            *rval = STRING_TO_JSVAL(JS_ValueToString(cx, *argv));
        }
        else if (JS_TRUE == JS_IsArrayObject(cx, JSVAL_TO_OBJECT(*argv))) {
            if (JS_FALSE == edjs_ArrayToJSON(cx, JSVAL_TO_OBJECT(*argv), rval))
                goto error;
        }
        else if (JS_FALSE == edjs_ObjectToJSON(cx, JSVAL_TO_OBJECT(argv[0]), rval)) {
            goto error;
        }
        break;
    }

    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    JS_EndRequest(cx);
    return ok;
}

static JSBool edjs_IsBoolean(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSClass *cl = NULL;

    *rval = OBJECT_TO_JSVAL(obj);

    cl = JS_GET_CLASS(cx, obj);

    if (strcmp("Boolean", cl->name) == 0)
        *rval = JSVAL_TRUE;
    else
        *rval = JSVAL_FALSE;

    return JS_TRUE;
}

static JSBool edjs_IsString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSClass *cl = NULL;

    *rval = OBJECT_TO_JSVAL(obj);

    cl = JS_GET_CLASS(cx, obj);

    if (strcmp("String", cl->name) == 0)
        *rval = JSVAL_TRUE;
    else
        *rval = JSVAL_FALSE;

    return JS_TRUE;
}

static JSBool edjs_IsFunction(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSClass *cl = NULL;

    *rval = OBJECT_TO_JSVAL(obj);

    cl = JS_GET_CLASS(cx, obj);

    if (strcmp("Function", cl->name) == 0)
        *rval = JSVAL_TRUE;
    else
        *rval = JSVAL_FALSE;

    return JS_TRUE;
}

static JSBool edjs_IsArray(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSClass *cl = NULL;

    *rval = OBJECT_TO_JSVAL(obj);

    cl = JS_GET_CLASS(cx, obj);

    if (strcmp("Array", cl->name) == 0)
        *rval = JSVAL_TRUE;
    else
        *rval = JSVAL_FALSE;

    return JS_TRUE;
}

static JSBool edjs_IsNumber(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSClass *cl = NULL;

    *rval = OBJECT_TO_JSVAL(obj);

    cl = JS_GET_CLASS(cx, obj);

    if (strcmp("Number", cl->name) == 0)
        *rval = JSVAL_TRUE;
    else
        *rval = JSVAL_FALSE;

    return JS_TRUE;
}

static JSBool edjs_IsUndefined(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    if (0 == argc || JSVAL_IS_VOID(*argv))
        *rval = JSVAL_TRUE;
    else
        *rval = JSVAL_FALSE;

    return JS_TRUE;
}

JSBool edjs_Clone(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    JSObject *clone_obj = NULL;

    JS_BeginRequest(cx);

    clone_obj = JS_ConstructObject(cx, NULL, obj, NULL);

    if (NULL == clone_obj) {
        EDJS_ERR(cx, EDJSERR_CONSTRUCTING_OBJ);
        goto error;
    }

    *rval = OBJECT_TO_JSVAL(clone_obj);

    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    JS_EndRequest(cx);
    return ok;
}

JSBool edjs_Copy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
    JSBool ok = JS_TRUE;
    JSObject *copy_obj = NULL;
    JSIdArray *id_a = NULL;
    jsint i = 0;
    jsval prop_key = JSVAL_VOID;
    jsval prop_val = JSVAL_VOID;
    jsval copied_val = JSVAL_VOID;
    JSString *prop_str = NULL;
    uintN prop_attrs = 0;
    JSBool found_bool = JS_FALSE;
    JSPropertyOp getter_op;
    JSPropertyOp setter_op;


    JS_BeginRequest(cx);
    JS_YieldRequest(cx); //might be called recursively, so attempt to yield

    if (JS_FALSE == JS_AddRoot(cx, &prop_str) ||
        JS_FALSE == JS_AddRoot(cx, &copy_obj)) {
        EDJS_ERR(cx, EDJSERR_ADD_ROOT);
        goto error;
    }

    if (NULL == obj) {
        if (argc < 1) {
            EDJS_ERR(cx, EDJSERR_NUM_ARGS, "copy");
            goto error;
        }

        if (JS_FALSE == JSVAL_IS_OBJECT(argv[0]) || JS_TRUE == JSVAL_IS_NULL(argv[0])) {
            *rval = argv[0];
            goto finish;
        }
        obj = JSVAL_TO_OBJECT(argv[0]);
    }

    copy_obj = JS_ConstructObject(cx, JS_GET_CLASS(cx, obj), NULL, NULL);
    if (NULL == copy_obj) {
        EDJS_ERR(cx, EDJSERR_CONSTRUCTING_OBJ);
        goto error;
    }

    id_a = JS_Enumerate(cx, obj);
    if (NULL == id_a) {
        EDJS_ERR(cx, EDJSERR_ENUMERATE_OBJ, "(internal copy obj)");
        goto error;
    }

    for (i = 0; i < id_a->length; i++) {
        if (JS_FALSE == JS_IdToValue(cx, id_a->vector[i], &prop_key)) {
            EDJS_ERR(cx, EDJSERR_ID_TO_VAL);
            goto error;
        }
        
        prop_str = JS_ValueToString(cx, prop_key);
        if (NULL == prop_str) {
            EDJS_ERR(cx, EDJSERR_STRING_CONVERSION);
            goto error;
        }

        //note: would like to use JS_GetPropertyAttrsGetterAndSetterById
        //but only supported in spidermonkey 1.8
        if (JS_FALSE == JS_GetPropertyAttrsGetterAndSetter(cx, obj,
                                                           JS_GetStringBytes(prop_str), 
                                                           &prop_attrs, &found_bool,
                                                           &getter_op, &setter_op)) {
            EDJS_ERR(cx, EDJSERR_GET_GETTERS_SETTERS);
            goto error;
        }

        if (JS_FALSE == found_bool) { //shouldn't happen, but better safe than sorry
            EDJS_ERR(cx, EDJSERR_GET_PROPERTY, "(internal copy obj)", JS_GetStringBytes(prop_str));
            goto error;
        }

        if (JS_FALSE == JS_GetProperty(cx, obj, JS_GetStringBytes(prop_str), &prop_val)) {
            EDJS_ERR(cx, EDJSERR_GET_PROPERTY, "(internal copy obj)", JS_GetStringBytes(prop_str));
            goto error;
        }

        if (JS_FALSE == edjs_Copy(cx, NULL, 1, &prop_val, &copied_val)) {
            goto error;
        }

        if (JS_FALSE == JS_DefineProperty(cx, copy_obj, JS_GetStringBytes(prop_str), 
                                          copied_val, getter_op, setter_op, prop_attrs)) {
            EDJS_ERR(cx, EDJSERR_SET_PROPERTY, "(internal copy obj)", JS_GetStringBytes(prop_str));
            goto error;
        }
        
        getter_op = NULL;
        setter_op = NULL;
        prop_attrs = 0;
    }

    *rval = OBJECT_TO_JSVAL(copy_obj);

    goto finish;
 error:
    ok = JS_FALSE;

 finish: 
    JS_RemoveRoot(cx, &prop_str);
    JS_RemoveRoot(cx, &copy_obj);
    JS_EndRequest(cx);
    return ok;
}

static JSBool edjs_QuoteString(JSContext *cx, const char* input, char **ret, int *len) {
    const char *cp0;
    char *cp1;
    *len = strlen(input) + 2; //+2 for the quotes at the beginning at end
    //now loop over the input. for any " we find, add one to the length for an escape char

    for (cp0 = input; *cp0 != '\0'; cp0++) {
        if (*cp0 == '"' || *cp0 == '\'' || *cp0 == '\\')
            (*len)++;
    }
    *ret = (char *)JS_malloc(cx, (*len)+1); //+1 for \0
    if (NULL == *ret)
        return JS_FALSE;

    **ret = '"';

    for (cp0 = input, cp1 = (*ret)+1; *cp0 != '\0'; cp0++, cp1++) {
        if (*cp0 == '"' || *cp0 == '\'' || *cp0 == '\\') {
            *cp1 = '\\';
            cp1++;
        }
        *cp1 = *cp0;
    }

    *cp1 = '"';
    *(cp1+1) = '\0';

    return JS_TRUE;
}

static JSBool edjs_UnquoteString(JSContext *cx, const char* input, char **ret, int *len) {
    const char *cp0;
    char *cp1;
    *len = strlen(input); 
    if ((*input == '"' && *(input+(*len)-1) != '"') ||
        (*input == '\'' && *(input+(*len)-1) != '\'') ||
        (*input != '"' && *input != '\'')) {

        *ret = (char *)JS_malloc(cx, (*len)+1); //+1 for \0
        if (NULL == *ret)
            return JS_FALSE;

        strncpy(*ret, input, (*len)+1);
        return JS_TRUE;
    }
    
    *len -= 2; //-2 for the quotes at the beginning at end
    //now loop over the input. for any '\'  we find, remove one from the length for an escape char

    for (cp0 = input; *cp0 != '\0'; cp0++) {
        if (*cp0 == '\\')
            (*len)--;
    }
    *ret = (char *)JS_malloc(cx, (*len)+1); //+1 for \0
    if (NULL == *ret)
        return JS_FALSE;

    cp0 = input+1;
    cp1 = *ret; 
    do {
        if (*cp0 == '\\') {
            cp0++;
        }
        *cp1 = *cp0;

        cp0++;
        cp1++;
    } while (*cp0 != '"' && *cp0 != '\'');

    *cp1 = '\0';

    return JS_TRUE;
}

static JSFunctionSpec edjs_global_functions[] = {
    //JS_FS("load",          edjs_Load,          1, JSPROP_ENUMERATE, 1),
    JS_FS("imp",           edjs_Import,        1, JSPROP_ENUMERATE, 4),
    JS_FS("include",       edjs_Include,       1, JSPROP_ENUMERATE, 3),
    JS_FS("echo",          edjs_Echo,          1, JSPROP_ENUMERATE, 0),
    JS_FS("isUndefined",   edjs_IsUndefined,   0, JSPROP_ENUMERATE, 0),
    //JS_FS("isJSON",        edjs_IsJSON,        1, JSPROP_ENUMERATE, 1),
    //JS_FS("extractJSON",   edjs_ExtractJSON,   1, JSPROP_ENUMERATE, 1),
    JS_FS_END
};

static JSFunctionSpec edjs_object_functions[] = {
    JS_FS("echo",       edjs_Echo,       0, 0, 0),
    //JS_FS("toJSON",     edjs_ToJSON,     0, JSPROP_ENUMERATE, 0),
    JS_FS("clone",      edjs_Clone,      0, 0, 0),
    JS_FS("copy",       edjs_Copy,       0, 0, 0),
    /*
    JS_FS("isBoolean",  edjs_IsBoolean,  0, JSPROP_ENUMERATE, 0),
    JS_FS("isArray",    edjs_IsArray,    0, JSPROP_ENUMERATE, 0),
    JS_FS("isFunction", edjs_IsFunction, 0, JSPROP_ENUMERATE, 0),
    JS_FS("isString",   edjs_IsString,   0, JSPROP_ENUMERATE, 0),
    JS_FS("isNumber",   edjs_IsNumber,   0, JSPROP_ENUMERATE, 0),
    */
    JS_FS_END
};

static JSFunctionSpec edjs_string_functions[] = {
    JS_FS("isJSON",      edjs_IsJSON,      0, JSPROP_ENUMERATE, 1),
    JS_FS("extractJSON", edjs_ExtractJSON, 0, JSPROP_ENUMERATE, 1),
    JS_FS_END
};

static JSFunctionSpec edjs_json_functions[] = {
    JS_FS("is",          edjs_IsJSON,      1, JSPROP_ENUMERATE, 1),
    JS_FS("extract",     edjs_ExtractJSON, 1, JSPROP_ENUMERATE, 1),
    JS_FS("stringify",   edjs_ToJSON,      1, JSPROP_ENUMERATE, 1),
    //    JS_FS("parse",       edjs_EvalJSON,    1, JSPROP_ENUMERATE, 1),
    JS_FS_END
};

JSBool EDJS_InitFunctions(JSContext *cx) {
    JSBool ok = JS_TRUE;
    JSRuntime *rt = NULL;
    edjs_runtime_private *rt_private = NULL;
    JSObject *global = NULL; 
    JSObject *object = NULL;
    JSObject *object_prototype = NULL;
    JSObject *str_object = NULL;
    JSObject *str_prototype = NULL;
    JSObject *json_obj = NULL;
    jsval result = JSVAL_VOID;

    rt = JS_GetRuntime(cx);
    rt_private = JS_GetRuntimePrivate(rt);

    global = JS_GetGlobalObject(cx);
    if (JS_FALSE == JS_DefineFunctions(cx, global, edjs_global_functions)) {
        //report error
        goto error;
    }

    object = JS_NewObject(cx, NULL, NULL, NULL);
    object_prototype = JS_GetPrototype(cx, object);

    if (JS_FALSE == JS_DefineFunctions(cx, object_prototype, edjs_object_functions)) {
        //report error
        goto error;
    }

    if (JS_FALSE == JS_ValueToObject(cx, JS_GetEmptyStringValue(cx), &str_object))
        return JS_FALSE;
    str_prototype = JS_GetPrototype(cx, str_object);
    if (JS_FALSE == JS_DefineFunctions(cx, str_prototype, edjs_string_functions)) {
        //report error
        goto error;
    }

    json_obj = JS_NewObject(cx, NULL, NULL, NULL);
    result = OBJECT_TO_JSVAL(json_obj);
    if (JS_FALSE == JS_SetProperty(cx, global, "JSON", &result)) {
        //report error
        goto error;
    }

    if (JS_FALSE == JS_DefineFunctions(cx, json_obj, edjs_json_functions)) {
        //report error
        goto error;
    }
    

    if (JS_FALSE == JS_ExecuteScript(cx, global, rt_private->core_script, &result)) {
        //report error
        goto error;
    }


    goto finish;
 error:
    ok = JS_FALSE;
 finish:
    return ok;
}
