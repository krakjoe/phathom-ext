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
#include "evaluator.h"

#include "zend_exceptions.h"

/* =========================================================================
 * Exception throwers — call the PHP factory methods, then throw the result.
 *
 *   AmbiguityException::range($context, $rule, $tokens, $origin, $end)
 *   ExecuteException::nomatch($context, $start, $tokens)
 * ======================================================================= */

static void php_phathom_exception_ambiguity_range(
    php_phathom_t           *phathom,
    php_phathom_evaluator_t *eval,
    zend_string             *rule,
    zend_long                origin,
    zend_long                end)
{
    zval tokens = php_phathom_evaluator_tokens(eval);
    zval args[5], exc;
    ZVAL_OBJ_COPY(&args[0], eval->context);
    ZVAL_STR(&args[1], rule);
    ZVAL_COPY_VALUE(&args[2], &tokens);
    ZVAL_LONG(&args[3], origin);
    ZVAL_LONG(&args[4], end);

    zend_call_known_function(
        phathom->exception.ambiguity.range, NULL,
        phathom->class.exception.ambiguity,
        &exc, 5, args, NULL);
    zval_ptr_dtor(&tokens);
    zval_ptr_dtor(&args[0]);

    if (!EG(exception) && Z_TYPE(exc) == IS_OBJECT) {
        zend_throw_exception_object(&exc);
    } else {
        zval_ptr_dtor(&exc);
    }
}

static void php_phathom_exception_execute_nomatch(php_phathom_t* phathom, php_phathom_evaluator_t *eval) {
    zval tokens = php_phathom_evaluator_tokens(eval);
    zval args[3], exc;
    ZVAL_OBJ_COPY(&args[0], eval->context);
    ZVAL_STR(&args[1], eval->chart->grammar.start);
    ZVAL_COPY_VALUE(&args[2], &tokens);

    zend_call_known_function(
        phathom->exception.execute.nomatch, NULL,
        phathom->class.exception.execute,
        &exc, 3, args, NULL);
    zval_ptr_dtor(&tokens);
    zval_ptr_dtor(&args[0]);

    if (!EG(exception) && Z_TYPE(exc) == IS_OBJECT) {
        zend_throw_exception_object(&exc);
    } else {
        zval_ptr_dtor(&exc);
    }
}

#endif