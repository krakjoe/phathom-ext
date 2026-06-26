/*
  +----------------------------------------------------------------------+
  | phathom                                                              |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2026                                       |
  +----------------------------------------------------------------------+
  | This source file is subject to the BSD 3-Clause License bundled     |
  | with this package in the file LICENSE.                               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */
#ifndef HAVE_PHATHOM_ENGINE_H
#define HAVE_PHATHOM_ENGINE_H

#include "phathom.h"

/*
 * Grammar\Interface\Engine:
 *   [0] Automaton $automaton
 *   [1] array     $optimiations
 */
typedef struct {
    zval automaton;
    zval optimizations;
} php_phathom_engine_t;

static zend_always_inline zend_object* php_phathom_engine_automaton(zend_object *obj) {
    zval *member = php_phathom_fetch_member(
        obj, php_phathom_engine_t, automaton);
    return Z_OBJ_P(Z_UNWRAP_P(member));
}
#endif