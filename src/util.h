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

#ifndef EDJS_UTIL_H
#define EDJS_UTIL_H

#include "jsapi.h"
#include "edjs.h"


static edjs_tree_node *edjs_tree_root(edjs_tree_node *n);
static edjs_tree_node *edjs_tree_insert_rb(edjs_tree_node *root, edjs_tree_node *n, int (*comparator)(edjs_tree_node *, edjs_tree_node *));
static edjs_tree_node *edjs_tree_grandparent(edjs_tree_node *n);
static edjs_tree_node *edjs_tree_uncle(edjs_tree_node *n);
static edjs_tree_node * edjs_tree_rotate_left(edjs_tree_node *n);
static edjs_tree_node * edjs_tree_rotate_right(edjs_tree_node *n);

#endif
