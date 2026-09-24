// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include "idl/heap.h"
#include "idl/processor.h"
#include "idl/string.h"
#include "annotation.h"
#include "directive.h"
#include "scanner.h"
#include "tree.h"
#include "scope.h"
#include "keylist.h"

#include "parser_impl.h"

static const idl_file_t builtin_file =
  { NULL, "<builtin>" };
static const idl_source_t builtin_source =
  { NULL, NULL, NULL, NULL, true, &builtin_file, &builtin_file };
#define BUILTIN_POSITION { &builtin_source, &builtin_file, 1, 1 }
#define BUILTIN_LOCATION { BUILTIN_POSITION, BUILTIN_POSITION }
static const idl_name_t builtin_name =
  { { BUILTIN_LOCATION }, "", false };

static void
reset_builtin_input(idl_pstate_t *pstate, const char *syntax)
{
  pstate->parser.state = IDL_PARSE;
  pstate->annotation_scope = NULL;
  pstate->scanner.state = IDL_SCAN;
  pstate->buffer.data = (char *)syntax;
  pstate->buffer.size = pstate->buffer.used = strlen(syntax);
  pstate->scanner.cursor = pstate->buffer.data;
  pstate->scanner.limit = pstate->buffer.data + pstate->buffer.used;
  pstate->scanner.position = (idl_position_t)BUILTIN_POSITION;
}

static void
clear_builtin_input(idl_pstate_t *pstate)
{
  pstate->buffer.data = NULL;
  pstate->buffer.size = pstate->buffer.used = 0;
  pstate->scanner.cursor = pstate->scanner.limit = NULL;
  pstate->scanner.position.line = 0;
  pstate->scanner.position.column = 0;
}

static void
free_token_value(idl_token_t *tok)
{
  switch (tok->code) {
    case IDL_TOKEN_IDENTIFIER:
    case IDL_TOKEN_STRING_LITERAL:
    case IDL_TOKEN_PP_NUMBER:
    case IDL_TOKEN_COMMENT:
    case IDL_TOKEN_LINE_COMMENT:
      if (tok->value.str)
        idl_free(tok->value.str);
      break;
    default:
      break;
  }
  memset(tok, 0, sizeof(*tok));
}

static idl_retcode_t
find_builtin_annotation_name(
  idl_pstate_t *pstate,
  const char *syntax,
  idl_name_t *name,
  unsigned *seen)
{
  idl_token_t tok;
  idl_retcode_t ret = IDL_RETCODE_OK;
  bool after_at = false;
  bool after_annotation = false;

  memset(name, 0, sizeof(*name));
  memset(&tok, 0, sizeof(tok));
  *seen = 0;

  reset_builtin_input(pstate, syntax);
  do {
    int32_t code;
    if ((ret = idl_scan(pstate, &tok)) < 0)
      break;
    ret = IDL_RETCODE_OK;
    code = tok.code;

    if (after_annotation && tok.code == IDL_TOKEN_IDENTIFIER) {
      (*seen)++;
      if (*seen == 1) {
        name->symbol.location = tok.location;
        name->identifier = tok.value.str;
        name->is_annotation = true;
        tok.value.str = NULL;
      }
      after_annotation = false;
    } else if (after_at && tok.code == IDL_TOKEN_ANNOTATION) {
      after_at = false;
      after_annotation = true;
    } else {
      after_at = tok.code == IDL_TOKEN_ANNOTATION_SYMBOL;
      after_annotation = false;
    }

    free_token_value(&tok);
    if (code == '\0')
      break;
  } while (true);

  clear_builtin_input(pstate);
  return ret;
}

static idl_retcode_t
parse_builtin_annotations(
  idl_pstate_t *pstate,
  const idl_builtin_annotation_t *annotations)
{
  idl_retcode_t ret = IDL_RETCODE_OK;

  for (size_t i=0; annotations[i].syntax; i++) {
    idl_name_t name;
    unsigned seen;

    if ((ret = find_builtin_annotation_name(
          pstate, annotations[i].syntax, &name, &seen)) != IDL_RETCODE_OK) {
      idl_free(name.identifier);
      return ret;
    }

    reset_builtin_input(pstate, annotations[i].syntax);
    ret = idl_parse_hand_written(pstate);
    clear_builtin_input(pstate);
    if (ret != IDL_RETCODE_OK) {
      idl_free(name.identifier);
      return ret;
    }

    if (seen == 1) {
      idl_annotation_t *annotation;
      const idl_declaration_t *declaration;
      declaration = idl_find(
        pstate, pstate->global_scope, &name, IDL_FIND_ANNOTATION);
      if (declaration) {
        annotation = (idl_annotation_t *)declaration->node;
        /* multiple definitions of the same annotation may exist, provided
           they are consistent */
        if (!memcmp(&annotation->name->symbol.location, &name.symbol.location, sizeof(name.symbol.location)))
          annotation->callback = annotations[i].callback;
      }
    }

    if (name.identifier) {
      idl_free(name.identifier);
    }

    /* builtin annotations must not declare more than one annotation per block
       to avoid ambiguity in annotation-callback mapping */
    if (seen > 1) {
      idl_error(pstate, &name.symbol.location,
        "Multiple declarations of builtin annotations in same block");
      return IDL_RETCODE_SYNTAX_ERROR;
    }
  }

  return ret;
}

idl_retcode_t
idl_create_pstate(
  uint32_t flags,
  const idl_builtin_annotation_t *annotations,
  idl_pstate_t **pstatep)
{
  idl_scope_t *scope = NULL;
  idl_pstate_t *pstate;

  (void)flags;
  if (!(pstate = idl_calloc(1, sizeof(*pstate))))
    goto err_pstate;
  if (idl_create_scope(pstate, IDL_GLOBAL_SCOPE, &builtin_name, NULL, &scope))
    goto err_scope;

  pstate->config.flags = flags;
  pstate->config.default_extensibility = IDL_DEFAULT_EXTENSIBILITY_UNDEFINED;
  pstate->config.default_nested = false;
  pstate->config.legacy_union_member_ids = false;
  pstate->global_scope = pstate->scope = scope;

  if (pstate->config.flags & IDL_FLAG_ANNOTATIONS) {
    idl_retcode_t ret;
    if ((ret = parse_builtin_annotations(pstate, builtin_annotations))) {
      idl_delete_pstate(pstate);
      return ret;
    }
    if (annotations && (ret = parse_builtin_annotations(pstate, annotations))) {
      idl_delete_pstate(pstate);
      return ret;
    }
  }

  pstate->keylists = false;
  pstate->annotations = false;
  pstate->parser.state = IDL_PARSE;
  pstate->scanner.state = IDL_SCAN;
  memset(&pstate->buffer, 0, sizeof(pstate->buffer));
  memset(&pstate->scanner, 0, sizeof(pstate->scanner));
  pstate->builtin_root = pstate->root;
  *pstatep = pstate;
  return IDL_RETCODE_OK;
err_scope:
  idl_free(pstate);
err_pstate:
  return IDL_RETCODE_NO_MEMORY;
}

static void delete_source(idl_source_t *src)
{
  if (!src)
    return;
  for (idl_source_t *n, *s=src; s; s = n) {
    n = s->next;
    delete_source(s->includes);
    idl_free(s);
  }
}

void idl_delete_pstate(idl_pstate_t *pstate)
{
  if (pstate) {
    idl_delete_node(pstate->builtin_root);
    /* directive */
    if (pstate->directive)
      idl_delete_directive(pstate);
    idl_delete_scope(pstate->global_scope);
    /* sources */
    delete_source(pstate->sources);
    /* files */
    for (idl_file_t *n, *f=pstate->files; f; f = n) {
      n = f->next;
      if (f->name)
        idl_free(f->name);
      idl_free(f);
    }
    /* paths */
    for (idl_file_t *n, *f=pstate->paths; f; f = n) {
      n = f->next;
      if (f->name)
        idl_free(f->name);
      idl_free(f);
    }
    /* buffer */
    if (pstate->buffer.data)
      idl_free(pstate->buffer.data);
    idl_free(pstate);
  }
}

static void
idl_log(
  const idl_pstate_t *pstate, uint32_t prio, const idl_location_t *loc, const char *fmt, va_list ap)
{
  char buf[1024];
  int cnt = 0;
  size_t off;

  buf[0] = '\0';
  (void)pstate;
  (void)prio;
  if (loc && loc->first.file)
    cnt = snprintf(
      buf, sizeof(buf)-1, "%s:%u:%u: ", loc->first.file->name, loc->first.line, loc->first.column);
  else if (loc)
    cnt = snprintf(
      buf, sizeof(buf)-1, "%u:%u: ", loc->first.line, loc->first.column);

  if (cnt == -1)
    return;

  off = ((size_t)cnt > sizeof(buf)) ? sizeof(buf) : (size_t)cnt;
  // coverity[overrun-local:FALSE]
  cnt = vsnprintf(buf+off, sizeof(buf)-off, fmt, ap);

  if (cnt == -1)
    return;

  fprintf(stderr, "%s\n", buf);
}

#define IDL_LC_ERROR 1
#define IDL_LC_WARNING 2

void
idl_verror(
  const idl_pstate_t *pstate, const idl_location_t *loc, const char *fmt, va_list ap)
{
  idl_log(pstate, IDL_LC_ERROR, loc, fmt, ap);
}

void
idl_error(
  const idl_pstate_t *pstate, const idl_location_t *loc, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  idl_log(pstate, IDL_LC_ERROR, loc, fmt, ap);
  va_end(ap);
}

void
idl_warning(
  const idl_pstate_t *pstate, idl_warning_t warning, const idl_location_t *loc, const char *fmt, ...)
{
  va_list ap;

  if (pstate->track_warning && !pstate->track_warning(warning))
    return;

  va_start(ap, fmt);
  idl_log(pstate, IDL_LC_WARNING, loc, fmt, ap);
  va_end(ap);
}

static idl_retcode_t validate_forwards(idl_pstate_t *pstate, void *root)
{
  for (void *node = root; node; node = idl_next(node))
  {
    if (idl_mask(node) == IDL_MODULE) {
      idl_retcode_t ret;
      if ((ret = validate_forwards(pstate, ((idl_module_t *)node)->definitions)))
        return ret;
    } else if (idl_mask(node) & IDL_FORWARD) {
      idl_forward_t *forward = node;
      if (!forward->type_spec) {
        idl_error(pstate, idl_location(forward),
          "Forward declared type %s is incomplete", idl_identifier(forward));
        return IDL_RETCODE_SEMANTIC_ERROR;
      }
      /* if a structure or union is forward declared, a definition of that
         structure or union must follow the forward declaration in the same
         compilation unit */
      const idl_source_t *a, *b;
      a = forward->node.symbol.location.first.source;
      b = ((idl_node_t *)forward->type_spec)->symbol.location.first.source;
      if (a != b) {
        idl_error(pstate, idl_location(forward),
          "Forward declaration for %s does not reside in same compilation "
          "unit as definition", idl_identifier(forward));
        return IDL_RETCODE_SEMANTIC_ERROR;
      }
    }
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t
check_bitbound(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void) revisit;
  (void) path;
  (void) user_data;

  if (idl_is_bitmask(node)) {
    const idl_bit_value_t *_bit_value = node;
    const idl_bitmask_t *_bitmask = node;
    IDL_FOREACH(_bit_value, _bitmask->bit_values) {
      if (_bit_value->position.value >= _bitmask->bit_bound.value) {
        idl_error(pstate, idl_location(_bit_value),
          "Position overflow for bit value '%s' (%hu), bit_bound is %hu", idl_identifier(_bit_value), _bit_value->position.value, _bitmask->bit_bound.value);
        return IDL_RETCODE_OUT_OF_RANGE;
      }
    }
  } else if (idl_is_enum(node)) {
    const idl_enum_t *_enum = node;
    const idl_enumerator_t *_e = NULL;
    const int32_t min = (_enum->bit_bound.value >= 32) ? INT32_MIN : -(int32_t) (UINT32_C(1) << (_enum->bit_bound.value - 1));
    const int32_t max = (_enum->bit_bound.value >= 32) ? INT32_MAX : (int32_t) ((UINT32_C(1) << (_enum->bit_bound.value - 1)) - 1);
    IDL_FOREACH(_e, _enum->enumerators) {
      if (_e->value.value < min || _e->value.value > max) {
        idl_error(pstate, idl_location(_e),
          "Enumerator overflow for value '%s' (%" PRId32 "), allowed range is %" PRId32 "..%" PRId32 " (bit_bound %hu)",
          idl_identifier(_e), _e->value.value, min, max, _enum->bit_bound.value);
        return IDL_RETCODE_OUT_OF_RANGE;
      }
    }
  } else {
    assert(0);
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
validate_datarepresentation(
  const idl_pstate_t* pstate,
  const bool revisit,
  const idl_path_t* path,
  const void* node,
  void* user_data)
{
  (void) revisit;
  (void) path;
  (void) user_data;

  assert (idl_is_struct(node) || idl_is_union(node));
  allowable_data_representations_t val = idl_allowable_data_representations(node);
  idl_requires_xcdr2_t requires_xcdr2 = idl_is_struct(node) ? ((idl_struct_t *)node)->requires_xcdr2 : ((idl_union_t *)node)->requires_xcdr2;
  if (requires_xcdr2 == IDL_REQUIRES_XCDR2_TRUE && !(val & IDL_DATAREPRESENTATION_FLAG_XCDR2)) {
    idl_error(pstate, idl_location(node),
      "Datarepresentation does not support XCDR2, but mutable extensibility set.");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t validate_bitbound(idl_pstate_t *pstate)
{
  idl_visitor_t visitor;
  memset(&visitor, 0, sizeof(visitor));
  visitor.visit = IDL_BITMASK | IDL_ENUM;
  visitor.accept[IDL_ACCEPT_BITMASK] = &check_bitbound;
  visitor.accept[IDL_ACCEPT_ENUM] = &check_bitbound;

  return idl_visit(pstate, pstate->root, &visitor, NULL);
}

static void
set_nestedness(
  const idl_pstate_t* pstate,
  void* node,
  bool current_fallback)
{
  IDL_FOREACH(node, node) {
    if (idl_is_module(node)) {
      idl_module_t *mod = node;
      if (!mod->default_nested.annotation)
        mod->default_nested.value = current_fallback;
      else
        current_fallback = mod->default_nested.value;

      IDL_FOREACH(node, mod->definitions) {
        set_nestedness(pstate, node, current_fallback);
      }
    } else if (idl_is_union(node) && !((idl_union_t*)node)->nested.annotation) {
      ((idl_union_t*)node)->nested.value = current_fallback;
    } else if (idl_is_struct(node) && !((idl_struct_t*)node)->nested.annotation) {
      ((idl_struct_t*)node)->nested.value = current_fallback;
    }
  }
}

static idl_retcode_t set_type_extensibility(idl_pstate_t *pstate)
{
  if (pstate->config.default_extensibility == IDL_DEFAULT_EXTENSIBILITY_UNDEFINED && idl_has_unset_extensibility_r(pstate->root)) {
    idl_warning(pstate, IDL_WARN_IMPLICIT_EXTENSIBILITY, NULL,
      "No default extensibility provided. For one or more of the "
      "aggregated types in the IDL the extensibility is not explicitly set. "
      "Currently the default extensibility for these types is 'final', but this "
      "may change to 'appendable' in a future release because that is the "
      "default in the DDS XTypes specification.");
  }
  return IDL_RETCODE_OK;
}

idl_retcode_t idl_parse(idl_pstate_t *pstate)
{
  idl_retcode_t ret;

  if ((ret = idl_parse_hand_written(pstate)) != IDL_RETCODE_OK)
    goto err;

  pstate->builtin_root = pstate->root;
  for (idl_node_t *node = pstate->root; node; node = node->next) {
    if (node->symbol.location.first.file != &builtin_file) {
      pstate->root = node;
      break;
    }
  }

  if ((ret = idl_set_xcdr2_required(pstate->root) != IDL_RETCODE_OK))
    goto err;
  if ((ret = idl_set_xcdr2_is_default(pstate->root) != IDL_RETCODE_OK))
    goto err;

  idl_visitor_t visitor;
  memset(&visitor, 0, sizeof(visitor));
  visitor.visit = IDL_STRUCT | IDL_UNION;
  visitor.accept[IDL_ACCEPT_STRUCT] = &validate_datarepresentation;
  visitor.accept[IDL_ACCEPT_UNION] = &validate_datarepresentation;

  if ((ret = idl_visit(pstate, pstate->root, &visitor, NULL)))
    goto err;

  /* FIXME: combine these validations into a single pass, e.g. by using
     callback functions for node validation */
  if ((ret = set_type_extensibility(pstate)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = validate_forwards(pstate, pstate->root)))
    goto err;
  if ((ret = validate_bitbound(pstate)))
    goto err;
  if ((ret = idl_propagate_autoid(pstate, pstate->root, IDL_SEQUENTIAL)) != IDL_RETCODE_OK)
    goto err;

  set_nestedness(pstate, pstate->root, pstate->config.default_nested);

  if (pstate->keylists) {
    if ((ret = idl_validate_keylists(pstate)) != IDL_RETCODE_OK)
      goto err;
    idl_set_keylist_key_flags(pstate, pstate->root);
  }

err:
  return ret;
}

idl_retcode_t idl_parse_string(idl_pstate_t *pstate, const char *str)
{
  idl_retcode_t ret;

  assert(str);

  pstate->buffer.data = (char *)str;
  pstate->buffer.size = pstate->buffer.used = strlen(str);
  pstate->scanner.cursor = pstate->buffer.data;
  pstate->scanner.limit = pstate->buffer.data + pstate->buffer.used;
  pstate->scanner.position.file = NULL;
  pstate->scanner.position.source = NULL;
  pstate->scanner.position.line = 1;
  pstate->scanner.position.column = 1;

  if ((ret = idl_parse(pstate)) == IDL_RETCODE_OK) {
    assert(pstate->root);
  }

  pstate->buffer.data = NULL;
  pstate->buffer.size = pstate->buffer.used = 0;
  pstate->scanner.cursor = pstate->scanner.limit = NULL;
  pstate->scanner.position.line = 0;
  pstate->scanner.position.column = 0;

  return ret;
}
