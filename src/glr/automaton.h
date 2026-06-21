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
#ifndef HAVE_PHATHOM_GLR_AUTOMATON_H
#define HAVE_PHATHOM_GLR_AUTOMATON_H

#include "phathom.h"

/*
 * GLR\Automaton:
 *   [0] int   $accept   — state_id of the accept state
 *   [1] array $reduces  — [state_id] => Reduction[]
 *   [2] array $shifts   — [state_id][terminal_id] => int
 *   [3] array $goto     — [state_id][rule_name] => int
 */
typedef struct {
    zval accept;
    zval reduces;
    zval shifts;
    zval goto_;
} php_phathom_glr_automaton_t;

static zend_always_inline zend_long php_phathom_glr_automaton_accept(zend_object *obj) {
    zval *m = php_phathom_fetch_member(
        obj, php_phathom_glr_automaton_t, accept);
    return Z_LVAL_P(m);
}

static zend_always_inline zend_array* php_phathom_glr_automaton_reduces(zend_object *obj) {
    zval *m = php_phathom_fetch_member(
        obj, php_phathom_glr_automaton_t, reduces);
    return Z_ARR_P(m);
}

static zend_always_inline zend_array* php_phathom_glr_automaton_shifts(zend_object *obj) {
    zval *m = php_phathom_fetch_member(
        obj, php_phathom_glr_automaton_t, shifts);
    return Z_ARR_P(m);
}

static zend_always_inline zend_array* php_phathom_glr_automaton_goto(zend_object *obj) {
    zval *m = php_phathom_fetch_member(
        obj, php_phathom_glr_automaton_t, goto_);
    return Z_ARR_P(m);
}
#endif