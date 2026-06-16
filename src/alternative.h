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
#ifndef HAVE_PHATHOM_ALTERNATIVE_H
#define HAVE_PHATHOM_ALTERNATIVE_H

#include "phathom.h"

/*
 * Alternative layout:
 *   public File       $file      [0]
 *   public array      $symbols   [1]
 *   public int|false  $priority  [2]
 *   public ?string    $action    [3]
 *   public Quantifier $synthetic [4]
 */
typedef struct php_phathom_alternative_t {
    zval file;
    zval symbols;
    zval priority;
    zval action;
    zval synthetic;
} php_phathom_alternative_t;

static zend_always_inline zend_array* php_phathom_alternative_symbols(zend_object *obj) {
    zval *member = php_phathom_fetch_member(
        obj, php_phathom_alternative_t, symbols);
    return Z_ARR_P(member);
}

/* Returns ZEND_LONG_MIN when priority === false (no priority annotation). */
static zend_always_inline zend_long php_phathom_alternative_priority(zend_object *obj) {
    zval *member = php_phathom_fetch_member(
        obj, php_phathom_alternative_t, priority);
    if (Z_TYPE_P(member) == IS_LONG) {
        return Z_LVAL_P(member);
    }
    return ZEND_LONG_MIN;
}

/* Returns true when action != null (ie, executable alternative). */
static zend_always_inline bool php_phathom_alternative_action(zend_object *obj) {
    zval *member = php_phathom_fetch_member(
        obj, php_phathom_alternative_t, action);
    return Z_TYPE_P(member) != IS_NULL;
}

/* Returns the Quantifier enum-case object for the synthetic field. */
static zend_always_inline zend_object* php_phathom_alternative_synthetic(zend_object *obj) {
    zval *member = php_phathom_fetch_member(
        obj, php_phathom_alternative_t, synthetic);
    return Z_OBJ_P(member);
}
#endif