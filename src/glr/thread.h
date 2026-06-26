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
#ifndef HAVE_PHATHOM_GLR_THREAD_H
#define HAVE_PHATHOM_GLR_THREAD_H

#include "phathom.h"
#include "slot.h"

typedef struct {
    zend_long              *states; /* arena: depth entries */
    php_phathom_glr_slot_t *nodes;  /* arena: depth entries */
    zend_long               depth;
    zend_long               pos;    /* tokens consumed so far */
} php_phathom_glr_thread_t;

/* {{{ thread operations (all arena-allocated, immutable)
 *
 * All thread structs and their states/nodes arrays live in chart->arena.
 * Threads are never freed individually -- the arena is destroyed with the chart.
 * -------------------------------------------------------------------- */
static zend_always_inline php_phathom_glr_thread_t*
php_phathom_glr_thread_new(
    zend_arena            **arena,
    zend_long              *states,
    php_phathom_glr_slot_t *nodes,
    zend_long               depth,
    zend_long               pos)
{
    php_phathom_glr_thread_t *t =
        zend_arena_alloc(arena, sizeof(*t));

    t->depth  = depth;
    t->pos    = pos;
    t->states = zend_arena_alloc(
        arena, (size_t) depth * sizeof(zend_long));
    t->nodes  = zend_arena_alloc(
        arena, (size_t) depth * sizeof(php_phathom_glr_slot_t));
    memcpy(t->states, states,
        (size_t) depth * sizeof(zend_long));
    memcpy(t->nodes,  nodes,
        (size_t) depth * sizeof(php_phathom_glr_slot_t));
    return t;
}

/* Thread::shift(state, ti) -- new thread with (state, token:ti) appended. */
static zend_always_inline php_phathom_glr_thread_t*
php_phathom_glr_thread_shift(
    zend_arena               **arena,
    php_phathom_glr_thread_t  *t,
    zend_long                  state,
    zend_long                  ti)
{
    zend_long               depth  = t->depth + 1;
    zend_long *states = zend_arena_alloc(arena,
        (size_t) depth * sizeof(zend_long));
    php_phathom_glr_slot_t *nodes  = zend_arena_alloc(arena,
        (size_t) depth * sizeof(php_phathom_glr_slot_t));

    memcpy(states, t->states,
        (size_t) t->depth * sizeof(zend_long));
    memcpy(nodes,  t->nodes,
        (size_t) t->depth * sizeof(php_phathom_glr_slot_t));
    states[t->depth] = state;
    nodes[t->depth]  = (php_phathom_glr_slot_t){ 
        .kind = PHP_PHATHOM_GLR_SLOT_TOKEN,
        .token = ti
    };

    php_phathom_glr_thread_t *nt = zend_arena_alloc(arena, sizeof(*nt));
    nt->depth  = depth;
    nt->pos    = t->pos + 1;
    nt->states = states;
    nt->nodes  = nodes;
    return nt;
}

/* Thread::push(state, node) -- new thread with (state, node) appended. */
static zend_always_inline php_phathom_glr_thread_t*
php_phathom_glr_thread_push(
    zend_arena               **arena,
    php_phathom_glr_thread_t  *t,
    zend_long                  state,
    php_phathom_glr_node_t    *node)
{
    zend_long               depth  = t->depth + 1;
    zend_long *states = zend_arena_alloc(arena,
        (size_t) depth * sizeof(zend_long));
    php_phathom_glr_slot_t *nodes = zend_arena_alloc(arena,
        (size_t) depth * sizeof(php_phathom_glr_slot_t));

    memcpy(states, t->states,
        (size_t) t->depth * sizeof(zend_long));
    memcpy(nodes,  t->nodes,
        (size_t) t->depth * sizeof(php_phathom_glr_slot_t));
    states[t->depth] = state;
    nodes[t->depth]  = (php_phathom_glr_slot_t){
        .kind = PHP_PHATHOM_GLR_SLOT_NODE,
        .node = node 
    };

    php_phathom_glr_thread_t *nt = zend_arena_alloc(arena, sizeof(*nt));
    nt->depth  = depth;
    nt->pos    = t->pos;
    nt->states = states;
    nt->nodes  = nodes;
    return nt;
}

/*
 * Thread::pop(count) -- return [base, children_slots].
 *
 * base has depth-count entries; children is arena-allocated from tmp_arena
 * in left-to-right order.  Caller does NOT own children.
 */
static zend_always_inline php_phathom_glr_thread_t*
php_phathom_glr_thread_pop(
    zend_arena               **arena,
    zend_arena               **tmp_arena,
    php_phathom_glr_thread_t  *t,
    zend_long                  count,
    php_phathom_glr_slot_t   **children_out)
{
    zend_long base_depth = t->depth - count;

    *children_out = zend_arena_alloc(tmp_arena,
        (size_t)(count ? count : 1) * sizeof(php_phathom_glr_slot_t));
    if (count > 0) {
        memcpy(*children_out,
            &t->nodes[base_depth],
            (size_t) count * sizeof(php_phathom_glr_slot_t));
    }

    /* base shares the prefix of t's states/nodes arrays (immutable) */
    php_phathom_glr_thread_t *base = zend_arena_alloc(arena, sizeof(*base));
    base->depth  = base_depth;
    base->pos    = t->pos;
    base->states = t->states;
    base->nodes  = t->nodes;
    return base;
} /* }}} */
#endif