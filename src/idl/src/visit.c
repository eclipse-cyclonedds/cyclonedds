// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "idl/heap.h"
#include "idl/processor.h"

static idl_accept_t idl_accept(const void *node)
{
  idl_mask_t mask = idl_mask(node);
  if ((mask & IDL_SEQUENCE) == IDL_SEQUENCE)
    return IDL_ACCEPT_SEQUENCE;
  if ((mask & IDL_STRING) == IDL_STRING)
    return IDL_ACCEPT_STRING;
  if (mask & IDL_INHERIT_SPEC)
    return IDL_ACCEPT_INHERIT_SPEC;
  if (mask & IDL_SWITCH_TYPE_SPEC)
    return IDL_ACCEPT_SWITCH_TYPE_SPEC;
  if (mask & IDL_MODULE)
    return IDL_ACCEPT_MODULE;
  if (mask & IDL_CONST)
    return IDL_ACCEPT_CONST;
  if (mask & IDL_MEMBER)
    return IDL_ACCEPT_MEMBER;
  if (mask & IDL_FORWARD)
    return IDL_ACCEPT_FORWARD;
  if (mask & IDL_CASE)
    return IDL_ACCEPT_CASE;
  if (mask & IDL_CASE_LABEL)
    return IDL_ACCEPT_CASE_LABEL;
  if (mask & IDL_ENUMERATOR)
    return IDL_ACCEPT_ENUMERATOR;
  if (mask & IDL_DECLARATOR)
    return IDL_ACCEPT_DECLARATOR;
  if (mask & IDL_ANNOTATION)
    return IDL_ACCEPT_ANNOTATION;
  if (mask & IDL_ANNOTATION_APPL)
    return IDL_ACCEPT_ANNOTATION_APPL;
  if (mask & IDL_TYPEDEF)
    return IDL_ACCEPT_TYPEDEF;
  if (mask & IDL_STRUCT)
    return IDL_ACCEPT_STRUCT;
  if (mask & IDL_UNION)
    return IDL_ACCEPT_UNION;
  if (mask & IDL_ENUM)
    return IDL_ACCEPT_ENUM;
  if (mask & IDL_BITMASK)
    return IDL_ACCEPT_BITMASK;
  if (mask & IDL_BIT_VALUE)
    return IDL_ACCEPT_BIT_VALUE;
  return IDL_ACCEPT;
}

struct stack {
  size_t size; /**< available number of slots */
  size_t depth; /**< number of slots in use */
  idl_path_t path;
  uint32_t *flags;
};

static const idl_node_t *pop(struct stack *stack)
{
  const idl_node_t *node;

  assert(stack);
  assert(stack->depth && stack->depth == stack->path.length);
  /* FIXME: implement shrinking the stack */
  stack->path.length = --stack->depth;
  node = stack->path.nodes[stack->depth];
  stack->path.nodes[stack->depth] = NULL;
  stack->flags[stack->depth] = 0u;
  return node;
}

static const idl_node_t *push(struct stack *stack, const idl_node_t *node)
{
  assert(stack->depth == stack->path.length);

  /* grow stack if necessary */
  if (stack->depth == stack->size) {
    size_t size = stack->size + 10;
    uint32_t *flags = NULL;
    const idl_node_t **nodes = NULL;
    if (!(flags = idl_realloc(stack->flags, size*sizeof(*flags))))
      return NULL;
    stack->flags = flags;
#if _MSC_VER
__pragma(warning(push))
__pragma(warning(disable: 4090))
#endif
    if (!(nodes = idl_realloc(stack->path.nodes, size*sizeof(*nodes))))
      return NULL;
#if _MSC_VER
__pragma(warning(pop))
#endif
    stack->path.nodes = nodes;
    stack->size = size;
  }

  stack->flags[stack->depth] = 0;
  stack->path.nodes[stack->depth] = node;
  stack->path.length = ++stack->depth;
  return node;
}

#define YES (0)
#define NO (1)
#define MAYBE (2)

static const uint32_t recurse[] = {
  IDL_VISIT_RECURSE,
  IDL_VISIT_DONT_RECURSE,
  IDL_VISIT_RECURSE|IDL_VISIT_DONT_RECURSE
};

static uint32_t iterate[] = {
  IDL_VISIT_ITERATE,
  IDL_VISIT_DONT_ITERATE,
  IDL_VISIT_ITERATE|IDL_VISIT_DONT_ITERATE
};

static const uint32_t revisit[] = {
  IDL_VISIT_REVISIT,
  IDL_VISIT_DONT_REVISIT,
  IDL_VISIT_REVISIT|IDL_VISIT_DONT_REVISIT
};

/* visit iteratively to save stack space */
idl_retcode_t
idl_visit(
  const idl_pstate_t *pstate,
  const void *node,
  const idl_visitor_t *visitor,
  void *user_data)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  idl_accept_t accept;
  idl_visitor_callback_t callback;
  struct stack stack = { 0, 0, { 0, NULL }, NULL };
  uint32_t flags = 0u;
  bool walk = true;

  assert(pstate);
  assert(node);
  assert(visitor);

  flags |= recurse[ visitor->recurse == recurse[NO]  ];
  flags |= iterate[ visitor->iterate == iterate[NO]  ];
  flags |= revisit[ visitor->revisit != revisit[YES] ];

  if (!push(&stack, node))
    goto err_push;
  stack.flags[0] = flags;

  while (stack.depth > 0) {
    node = stack.path.nodes[stack.depth - 1];
    accept = idl_accept(node);
    if (visitor->accept[accept])
      callback = visitor->accept[accept];
    else
      callback = visitor->accept[IDL_ACCEPT];

    if (walk) {
      /* skip or visit */
      if (!callback || !(idl_mask(node) & visitor->visit)) {
        ret = IDL_RETCODE_OK;
      } else if (!visitor->sources) {
        if ((ret = callback(pstate, false, &stack.path, node, user_data)) < 0)
          goto err_visit;
      } else if (visitor->sources) {
        const char *source = ((const idl_node_t *)node)->symbol.location.first.source->path->name;

        ret = IDL_RETCODE_OK;
        for (size_t i=0; visitor->sources[i]; i++) {
          if (strcmp(source, visitor->sources[i]) != 0)
            continue;
          if ((ret = callback(pstate, false, &stack.path, node, user_data)) < 0)
            goto err_visit;
          break;
        }
      }

      /* override default flags */
      if (ret & (idl_retcode_t)recurse[MAYBE]) {
        stack.flags[stack.depth - 1] &= ~recurse[MAYBE];
        stack.flags[stack.depth - 1] |=  recurse[ ((unsigned)ret & recurse[NO]) != 0 ];
      }
      if (ret & (idl_retcode_t)iterate[MAYBE]) {
        stack.flags[stack.depth - 1] &= ~iterate[MAYBE];
        stack.flags[stack.depth - 1] |=  iterate[ ((unsigned)ret & iterate[NO]) != 0 ];
      }
      if (ret & (idl_retcode_t)revisit[MAYBE]) {
        stack.flags[stack.depth - 1] &= ~revisit[MAYBE];
        stack.flags[stack.depth - 1] |=  revisit[ ((unsigned)ret & revisit[NO]) != 0 ];
      }

      if (ret & IDL_VISIT_TYPE_SPEC) {
          node = idl_type_spec(node);
        if (ret & IDL_VISIT_UNALIAS_TYPE_SPEC)
          node = idl_strip(node, IDL_STRIP_ALIASES|IDL_STRIP_ALIASES_ARRAY);
        assert(node);
        if (!push(&stack, node))
          goto err_push;
        stack.flags[stack.depth - 1] = flags | IDL_VISIT_TYPE_SPEC;
        walk = true;
      } else if (stack.flags[stack.depth - 1] & IDL_VISIT_RECURSE) {
        node = idl_iterate(node, NULL);
        if (node) {
          if (!push(&stack, node))
            goto err_push;
          stack.flags[stack.depth - 1] = flags;
          walk = true;
        } else {
          walk = false;
        }
      } else {
        walk = false;
      }
    } else {
      if (callback && (stack.flags[stack.depth - 1] & IDL_VISIT_REVISIT)) {
        /* callback must exist if revisit is true */
        if ((ret = callback(pstate, true, &stack.path, node, user_data)) < 0)
          goto err_revisit;
      }
      if (stack.flags[stack.depth - 1] & (IDL_VISIT_TYPE_SPEC|IDL_VISIT_DONT_ITERATE)) {
        (void)pop(&stack);
      } else {
        (void)pop(&stack);
        if (stack.depth > 0)
          node = idl_iterate(stack.path.nodes[stack.depth - 1], node);
        else
          node = idl_next(node);
        if (node) {
          if (!push(&stack, node))
            goto err_push;
          stack.flags[stack.depth - 1] = flags;
          walk = true;
        }
      }
    }
  }

#if _MSC_VER
__pragma(warning(push))
__pragma(warning(disable: 4090))
#endif
  if (stack.flags)      idl_free(stack.flags);
  if (stack.path.nodes) idl_free(stack.path.nodes);
  return IDL_RETCODE_OK;
err_push:
  ret = IDL_RETCODE_NO_MEMORY;
err_visit:
err_revisit:
  if (stack.flags)      idl_free(stack.flags);
  if (stack.path.nodes) idl_free(stack.path.nodes);
  return ret;
#if _MSC_VER
__pragma(warning(pop))
#endif
}
