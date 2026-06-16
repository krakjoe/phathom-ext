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
#ifndef HAVE_PHATHOM_SYMBOL_H
#define HAVE_PHATHOM_SYMBOL_H

#include "phathom.h"

/*
 * Symbol layout:
 *   public int|false $terminal   [0]
 *   public int       $type       [1]
 *   public string    $name       [2]
 *   public array     $location   [3]
 *   public Quantifier $quantifier [4]
 */

typedef struct _php_phathom_symbol_t {
    zval terminal;
    zval type;
    zval name;
    zval location;
    zval quantifier;
} php_phathom_symbol_t;

static zend_always_inline zend_string* php_phathom_symbol_name(zend_object *obj) {
    zval *member = php_phathom_fetch_member(
        obj, php_phathom_symbol_t, name);
    return Z_STR_P(member);
}

/* Returns -1 if not a terminal, otherwise the terminal id */
static zend_always_inline zend_long php_phathom_symbol_terminal(zend_object *obj) {
    zval *member = php_phathom_fetch_member(
        obj, php_phathom_symbol_t, terminal);
    return Z_TYPE_P(member) == IS_LONG ?
        Z_LVAL_P(member) : -1;
}
#endif