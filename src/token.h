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
#ifndef HAVE_PHATHOM_TOKEN_H
#define HAVE_PHATHOM_TOKEN_H

#include "phathom.h"

/*
 * Token layout (abstract class Token):
 *   public int   $type     [0]
 *   public array $location [1]
 *   public mixed $value    [2]
 */
typedef struct _php_phathom_token_t {
    zval type;
    zval location;
    zval value;
} php_phathom_token_t;

static zend_always_inline zend_long php_phathom_token_type(zend_object *obj) {
    zval *member = php_phathom_fetch_member(
        obj, php_phathom_token_t, type);
    return Z_LVAL_P(member);
}
#endif