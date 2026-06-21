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