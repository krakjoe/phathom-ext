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
#ifndef HAVE_PHATHOM_EXCEPTIONS_H
#define HAVE_PHATHOM_EXCEPTIONS_H

#include "phathom.h"

#include "zend_exceptions.h"

/* =========================================================================
 * Exception throwers — call the PHP factory methods, then throw the result.
 *
 *   AmbiguityException::range($context, $rule, $tokens, $origin, $end)
 *   ExecuteException::nomatch($context, $start, $tokens)
 * ======================================================================= */

static void php_phathom_exception_ambiguity_range(
    php_phathom_t           *phathom,
    zend_object             *context,
    zend_string             *rule,
    zend_long                origin,
    zend_long                end,
    HashTable               *tokens)
{
    zval args[5], retval;
    ZVAL_OBJ_COPY(&args[0], context);
    ZVAL_STR(&args[1],      rule);
    ZVAL_ARR(&args[2],      tokens);
    ZVAL_LONG(&args[3],     origin);
    ZVAL_LONG(&args[4],     end);

    zend_call_known_function(
        phathom->exception.ambiguity.range, NULL,
        phathom->class.exception.ambiguity,
        &retval, 5, args, NULL);
    zval_ptr_dtor(&args[0]);

    if (!EG(exception) && Z_TYPE(retval) == IS_OBJECT) {
        zend_throw_exception_object(&retval);
    } else {
        zval_ptr_dtor(&retval);
    }
}

static void php_phathom_exception_execute_nomatch(php_phathom_t* phathom, zend_object *context, zend_string *start, HashTable *tokens) {
    zval args[3], retval;
    ZVAL_OBJ_COPY(&args[0], context);
    ZVAL_STR(&args[1],      start);
    ZVAL_ARR(&args[2],      tokens);

    zend_call_known_function(
        phathom->exception.execute.nomatch, NULL,
        phathom->class.exception.execute,
        &retval, 3, args, NULL);
    zval_ptr_dtor(&args[0]);

    if (!EG(exception) && Z_TYPE(retval) == IS_OBJECT) {
        zend_throw_exception_object(&retval);
    } else {
        zval_ptr_dtor(&retval);
    }
}
#endif