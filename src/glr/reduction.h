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
#ifndef HAVE_PHATHOM_GLR_REDUCTION_H
#define HAVE_PHATHOM_GLR_REDUCTION_H

#include "phathom.h"

/*
 * GLR\Reduction:
 *   [0] string      $rule
 *   [1] int         $alt
 *   [2] int         $length
 *   [3] Alternative $alternative
 */
typedef struct {
    zval rule;
    zval alt;
    zval length;
    zval alternative;
} php_phathom_glr_reduction_t;

static zend_always_inline zend_string* php_phathom_glr_reduction_rule(zend_object *obj) {
    zval *m = php_phathom_fetch_member(
        obj, php_phathom_glr_reduction_t, rule);
    return Z_STR_P(m);
}

static zend_always_inline zend_long php_phathom_glr_reduction_alt(zend_object *obj) {
    zval *m = php_phathom_fetch_member(
        obj, php_phathom_glr_reduction_t, alt);
    return Z_LVAL_P(m);
}

static zend_always_inline zend_long php_phathom_glr_reduction_length(zend_object *obj) {
    zval *m = php_phathom_fetch_member(
        obj, php_phathom_glr_reduction_t, length);
    return Z_LVAL_P(m);
}

static zend_always_inline zend_object* php_phathom_glr_reduction_alternative(zend_object *obj) {
    zval *m = php_phathom_fetch_member(
        obj, php_phathom_glr_reduction_t, alternative);
    return Z_OBJ_P(m);
}
#endif