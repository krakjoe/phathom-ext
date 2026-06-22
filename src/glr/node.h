/*
  +----------------------------------------------------------------------+
  | phathom                                                              |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2026                                       |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */
#ifndef HAVE_PHATHOM_GLR_NODE_H
#define HAVE_PHATHOM_GLR_NODE_H

#include "phathom.h"
#include "slot.h"

/*
 * end   = thread->pos at reduction time (exclusive token count).
 * start = first token index reachable through children (inclusive).
 */
struct _php_phathom_glr_node_t {
    zend_string             *rule;
    zend_long                alt;
    zend_object             *alternative;
    php_phathom_glr_slot_t  *children;
    zend_long                nchildren;
    zend_long                start;
    zend_long                end;
};

/*
 * leftEnd(node) — mirrors Node::leftEnd().
 * Returns the end of the first non-terminal (NODE) child, else node->end.
 */
static zend_always_inline zend_long php_phathom_glr_node_left_end(
    php_phathom_glr_node_t *node)
{
    for (zend_long i = 0; i < node->nchildren; i++) {
        if (node->children[i].kind == PHP_PHATHOM_GLR_SLOT_NODE) {
            return node->children[i].node->end;
        }
    }
    return node->end;
}

static zend_always_inline zend_long
php_phathom_glr_node_compute_start(
    php_phathom_glr_slot_t *children,
    zend_long               nchildren,
    zend_long               pos)
{
    for (zend_long i = 0; i < nchildren; i++) {
        if (children[i].kind == PHP_PHATHOM_GLR_SLOT_NODE) {
            return children[i].node->start;
        }
        if (children[i].kind == PHP_PHATHOM_GLR_SLOT_TOKEN) {
            return children[i].token;
        }
    }
    return pos;
}

static zend_always_inline php_phathom_glr_node_t*
php_phathom_glr_node_new(
    zend_arena             **arena,
    zend_string             *rule,
    zend_long                alt,
    zend_object             *alternative,
    php_phathom_glr_slot_t  *children,
    zend_long                nchildren,
    zend_long                pos)
{
    php_phathom_glr_node_t *n = zend_arena_alloc(arena, sizeof(*n));
    n->rule        = rule;
    n->alt         = alt;
    n->alternative = alternative;
    n->nchildren   = nchildren;
    n->end         = pos;
    n->start       = php_phathom_glr_node_compute_start(children, nchildren, pos);

    if (nchildren > 0) {
        n->children = zend_arena_alloc(arena,
            (size_t) nchildren * sizeof(*children));
        memcpy(n->children, children,
            (size_t) nchildren * sizeof(*children));
    } else {
        n->children = NULL;
    }
    return n;
}
#endif