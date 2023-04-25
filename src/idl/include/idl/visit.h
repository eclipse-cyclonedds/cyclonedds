// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDL_VISIT_H
#define IDL_VISIT_H

#include "idl/export.h"
#include "idl/tree.h"

struct idl_pstate;

typedef enum idl_accept idl_accept_t;
enum idl_accept {
  IDL_ACCEPT_INHERIT_SPEC,
  IDL_ACCEPT_SWITCH_TYPE_SPEC,
  IDL_ACCEPT_MODULE,
  IDL_ACCEPT_CONST,
  IDL_ACCEPT_MEMBER,
  IDL_ACCEPT_FORWARD,
  IDL_ACCEPT_CASE,
  IDL_ACCEPT_CASE_LABEL,
  IDL_ACCEPT_ENUMERATOR,
  IDL_ACCEPT_DECLARATOR,
  IDL_ACCEPT_ANNOTATION,
  IDL_ACCEPT_ANNOTATION_APPL,
  IDL_ACCEPT_TYPEDEF,
  IDL_ACCEPT_STRUCT,
  IDL_ACCEPT_UNION,
  IDL_ACCEPT_ENUM,
  IDL_ACCEPT_BITMASK,
  IDL_ACCEPT_BIT_VALUE,
  IDL_ACCEPT_SEQUENCE,
  IDL_ACCEPT_STRING,
  IDL_ACCEPT /**< generic callback, used if no specific callback exists */
};

/* generating native language representations for types is relatively
   straightforward, but generating serialization code requires more context.
   path objects form a simple stack that generators can iterate */

typedef idl_retcode_t(*idl_visitor_callback_t)(
  const struct idl_pstate *pstate,
  const bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data);

/* the visitor mechanism is a pragmatic combination based on the listener and
   visitor mechanisms in ANTLRv4. default behavior is to iterate depth-first
   over the (sub)tree, visiting each node once, but the visitor can be
   instructed to visit the node again by signalling IDL_VISIT_REVISIT or skip
   an entire subtree by signalling IDL_VISIT_DONT_RECURSE. the visitor can be
   instructed to visit the type specifier for a declarator by signalling
   IDL_VISIT_TYPE_SPEC, which is useful when generating serialization code.
   default behavior can be inverted by specifying IDL_VISIT_DONT_RECURSE,
   IDL_VISIT_DONT_ITERATE or IDL_VISIT_REVISIT in the visitor, callbacks can
   instruct the visitor to recurse, iterate and/or revisit by signalling the
   inverse */

typedef enum idl_visit_recurse idl_visit_recurse_t;
enum idl_visit_recurse {
  IDL_VISIT_RECURSE_BY_DEFAULT = 0,
  IDL_VISIT_RECURSE = (1<<0), /**< Recurse into subtree(s) */
  IDL_VISIT_DONT_RECURSE = (1<<1) /**< Do not recurse into subtree(s) */
};

/* FIXME: it now applies to the next level. instead, it should apply to the
        current level. in which case IDL_VISIT_ITERATE instructs the
        visitor to continue, IDL_VISIT_DONT_ITERATE does the inverse!
*/
typedef enum idl_visit_iterate idl_visit_iterate_t;
enum idl_visit_iterate {
  IDL_VISIT_ITERATE_BY_DEFAULT = 0,
  IDL_VISIT_ITERATE = (1<<2), /**< Iterate over subtree(s) */
  IDL_VISIT_DONT_ITERATE = (1<<3) /**< Do not iterate over subtree(s) */
};

typedef enum idl_visit_revisit idl_visit_revisit_t;
enum idl_visit_revisit {
  IDL_VISIT_DONT_REVISIT_BY_DEFAULT = 0,
  IDL_VISIT_REVISIT = (1<<4), /**< Revisit node(s) on exit */
  IDL_VISIT_DONT_REVISIT = (1<<5) /**< Do not revisit node(s) on exit */
};

/** Visit associated type specifier (callback signal) */
#define IDL_VISIT_TYPE_SPEC (1<<6)
/** Unalias associated type specifier (callback signal) */
#define IDL_VISIT_UNALIAS_TYPE_SPEC (1<<7)

/* FIXME: add IDL_VISIT_ARRAY that complements IDL_VISIT_TYPE_SPEC and takes
          into account array declarators, which is incredibly useful for
          backends, like the native generator for Cyclone DDS, that need to
          unroll for generating serialization code */

typedef struct idl_visitor idl_visitor_t;
struct idl_visitor {
  idl_mask_t visit;
  idl_visit_recurse_t recurse;
  idl_visit_iterate_t iterate;
  idl_visit_revisit_t revisit;
  idl_visitor_callback_t accept[IDL_ACCEPT + 1];
  const char **sources;
};

IDL_EXPORT idl_retcode_t
idl_visit(
  const struct idl_pstate *pstate,
  const void *node,
  const idl_visitor_t *visitor,
  void *user_data);

#endif /* IDL_VISIT_H */
