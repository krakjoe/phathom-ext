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
#ifndef HAVE_PHATHOM_FRAME_H
#define HAVE_PHATHOM_FRAME_H

typedef enum _php_phathom_frame_kind_t {
    PHP_PHATHOM_FRAME_SELECT = 1,
    PHP_PHATHOM_FRAME_APPLY  = 2,
} php_phathom_frame_kind_t;

typedef struct {
    php_phathom_frame_kind_t kind;
    php_phathom_item_t      *item;
    /* APPLY only: */
    zval                    *partial;   /* owned zval[npartial] */
    zend_long               *slots;     /* owned long[nslots]   */
    zend_long                npartial;
    zend_long                nslots;
} php_phathom_frame_t;

typedef struct {
    php_phathom_frame_t *data;
    zend_long            used;
    zend_long            limit;
} php_phathom_frame_stack_t;

typedef struct {
    zval      *data;
    zend_long  used;
    zend_long  limit;
} php_phathom_frame_heap_t;

static zend_always_inline void php_phathom_frame_push(
    php_phathom_frame_stack_t *stack, php_phathom_frame_t frame)
{
    if (stack->used == stack->limit) {
        stack->limit = stack->limit ? stack->limit * 2 : 8;
        stack->data  = erealloc(stack->data,
            sizeof(php_phathom_frame_t) * stack->limit);
    }
    stack->data[stack->used++] = frame;
}

static zend_always_inline php_phathom_frame_t php_phathom_frame_pop(
    php_phathom_frame_stack_t *stack)
{
    return stack->data[--stack->used];
}

/* Heap takes ownership of *val — caller must NOT zval_ptr_dtor afterwards. */
static zend_always_inline void php_phathom_frame_heap_push(
    php_phathom_frame_heap_t *heap, zval *val)
{
    if (heap->used == heap->limit) {
        heap->limit = heap->limit ? heap->limit * 2 : 8;
        heap->data  = erealloc(
            heap->data, sizeof(zval) * heap->limit);
    }
    ZVAL_COPY_VALUE(&heap->data[heap->used++], val);
}

/* Caller takes ownership of the returned zval. */
static zend_always_inline zval php_phathom_frame_heap_pop(php_phathom_frame_heap_t *heap) {
    return heap->data[--heap->used];
}

/* Free a partial array (each entry was either ZVAL_COPY'd or UNDEF). */
static void php_phathom_frame_partial_free(zval *partial, zend_long n) {
    for (zend_long i = 0; i < n; i++) {
        zval_ptr_dtor(&partial[i]);
    }
    efree(partial);
}

/* Cleanup any remaining frames (called only on exception path). */
static void php_phathom_frame_stack_cleanup(php_phathom_frame_stack_t *stack) {
    for (zend_long i = 0; i < stack->used; i++) {
        php_phathom_frame_t *f = &stack->data[i];
        if (f->kind == PHP_PHATHOM_FRAME_APPLY && f->partial) {
            php_phathom_frame_partial_free(f->partial, f->npartial);
            efree(f->slots);
        }
    }
    if (stack->data) {
        efree(stack->data);
    }
}

/* Cleanup any remaining heap values (called only on exception path). */
static void php_phathom_frame_heap_cleanup(php_phathom_frame_heap_t *heap) {
    for (zend_long i = 0; i < heap->used; i++) {
        zval_ptr_dtor(&heap->data[i]);
    }
    if (heap->data) {
        efree(heap->data);
    }
}

#endif