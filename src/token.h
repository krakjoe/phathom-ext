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