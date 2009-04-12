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


#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include "edjs.h"
#include "util.h"

int edjs_TreeNodeLocate(edjs_tree_node *root, edjs_tree_node *n, edjs_tree_node **found, int (*comparator)(edjs_tree_node *, edjs_tree_node *)) {
    int comp = 0;

    if (NULL == root) {
        *found = NULL;
        return 0;
    }

    comp = comparator(root, n); //strcmp (root->file_location, n->file_location);
    while((comp > 0 && NULL != root->left) || (comp < 0 && NULL != root->right)) {
        if (comp > 0) {
            //returns NULL when root->left is null
            //ret = edjs_module_insert_helper(root->left, n);
            if (NULL != root->left)
                root = root->left;
        }
        else if (comp < 0) {
            //returns NULL when root->right is null
            //ret = edjs_module_insert_helper(root->right, n);
            if (NULL != root->right)
                root = root->right;
        }
        comp = comparator(root, n); //strcmp (root->file_location, n->file_location);
    } 

    *found = root;
    return comp;
}

edjs_tree_node *edjs_TreeNodeInsert(edjs_tree_node *root, edjs_tree_node *n, int (*comparator)(edjs_tree_node *, edjs_tree_node *)) {
    return edjs_tree_insert_rb(root, n, comparator);
}

void edjs_TreeDestroy(edjs_tree_node *root, void (*destroyer)(edjs_tree_node *)) {
    edjs_tree_node *parent = NULL;

    while (NULL != root) {
        if (NULL != root->left)
            root = root->left;
        else if (NULL != root->right)
            root = root->right;
        else {
            parent = root->parent;

            if (NULL != parent) {
                if (root == root->parent->left)
                    root->parent->left = NULL;
                else
                    root->parent->right = NULL;
            }
            destroyer(root);
            root = parent;
        }
    }
}
/*
static void edjs_module_destroy_node(JSContext *cx, edjs_tree_node *n) {
    if (NULL != n) {
        if (NULL != n->file_location)
            JS_free(cx, n->file_location);
        if (NULL != n->dl_lib)
            dlclose(n->dl_lib);

        JS_free(cx, n);
    }
}
*/
edjs_tree_node *edjs_tree_root(edjs_tree_node *n) {
    if (NULL == n)
        return NULL;
    while (NULL != n->parent)
        n = n->parent;

    return n;
}


edjs_tree_node *edjs_tree_insert_rb(edjs_tree_node *parent, edjs_tree_node *n, int (*comparator)(edjs_tree_node *, edjs_tree_node *)) {
    int comp = 0;
	edjs_tree_node *u = NULL;
    edjs_tree_node *g = NULL;
    edjs_tree_node *ret = parent;


    n->parent = NULL;
    n->left = NULL;
    n->right = NULL;
    n->tree_data = 1;

    comp = edjs_TreeNodeLocate(parent, n, &ret, comparator);

    //can not insert existing elements
    //locate returns null for ret when parent is null
    if (NULL != ret && comp == 0)
        return edjs_tree_root(parent);
    else if (comp > 0)
        ret->left = n;
    else if (comp < 0)
        ret->right = n;

    n->parent = ret;

    comp = 1;
    while(0 != comp) {
        comp = 0;
        //case 1
        if (NULL == n->parent) {
            n->tree_data = 0;
            ret = n;
        }
        else {

            //case 2
            if (n->parent->tree_data == 0)
                ret = n->parent;
            else {

                //case 3
                u = edjs_tree_uncle(n);
                g = NULL;
 
                if (NULL != u && u->tree_data == 1) {
                    n->parent->tree_data = 0;
                    u->tree_data = 0;
                    g = edjs_tree_grandparent(n);
                    g->tree_data = 1;
                    //return insert_case1(g);
                    n = g;
                    comp = 1; //recurse
                }
                else {

                    //case 4
                    g = edjs_tree_grandparent(n);
                    if (n == n->parent->right && n->parent == g->left) {
                        edjs_tree_rotate_left(n->parent);
                        n = n->left;
                    }
                    else if (n == n->parent->left && n->parent == g->right) {
                        edjs_tree_rotate_right(n->parent);
                        n = n->right;
                    }

                    //case 5 - note that case 4 always falls through
                    g = edjs_tree_grandparent(n);
                    n->parent->tree_data = 0;
                    g->tree_data = 1;
                    if (n == n->parent->left && n->parent == g->left) {
                        ret = edjs_tree_rotate_right(g);
                    }
                    else {
                        //n == n->parent->right && n->parent == g->right
                        ret = edjs_tree_rotate_left(g);
                    }
                }
            }
        }
    }

    if (NULL == parent) //if the node we just added becomes the root...
        return n;

    return edjs_tree_root(parent); //note that the root of the tree may have changed
}

edjs_tree_node *edjs_tree_grandparent(edjs_tree_node *n) {
	if ((n != NULL) && (n->parent != NULL))
		return n->parent->parent;
	else
		return NULL;
}
 
edjs_tree_node *edjs_tree_uncle(edjs_tree_node *n) {
	edjs_tree_node *g = edjs_tree_grandparent(n);
	if (g == NULL)
		return NULL; // No grandparent means no uncle
	if (n->parent == g->left)
		return g->right;
	else
		return g->left;
}

edjs_tree_node *edjs_tree_rotate_left(edjs_tree_node *n) {
    edjs_tree_node *r = n->right;

    if (NULL == r)
        return n;

    if (NULL != n->parent) {
        if (n->parent->left == n)
            n->parent->left = r;
        else
            n->parent->right = r;
    }
    r->parent = n->parent;

    n->right = r->left;
    if (NULL != n->right)
        n->right->parent = n;
    r->left = n;
    n->parent = r;

    return r;
}

edjs_tree_node *edjs_tree_rotate_right(edjs_tree_node *n) {
    edjs_tree_node *l = n->left;

    if (NULL == l)
        return n;

    if (NULL != n->parent) {
        if (n->parent->right == n)
            n->parent->right = l;
        else
            n->parent->left = l;
    }
    l->parent = n->parent;

    n->left = l->right;
    if (NULL != n->left)
        n->left->parent = n;
    l->right = n;
    n->parent = l;

    return l;
}
