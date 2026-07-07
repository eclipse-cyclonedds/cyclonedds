// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

/**
 * @file regex.h
 * @brief Small regular expression matcher.
 */
#ifndef DDSRT_REGEX_H
#define DDSRT_REGEX_H

#include <stdbool.h>
#include <stddef.h>

#include "dds/export.h"
#include "dds/ddsrt/attributes.h"
#include "dds/ddsrt/retcode.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDSRT_REGEX_CLASS_BYTES 32

typedef enum ddsrt_regex_opcode {
  DDSRT_REGEX_OP_UNUSED,
  DDSRT_REGEX_OP_CHAR,
  DDSRT_REGEX_OP_ANY,
  DDSRT_REGEX_OP_CLASS,
  DDSRT_REGEX_OP_SPLIT,
  DDSRT_REGEX_OP_JUMP,
  DDSRT_REGEX_OP_BOL,
  DDSRT_REGEX_OP_EOL,
  DDSRT_REGEX_OP_MATCH
} ddsrt_regex_opcode_t;

typedef struct ddsrt_regex_state {
  ddsrt_regex_opcode_t op;
  int out1;
  int out2;
  unsigned char ch;
  bool invert_class;
  unsigned char cls[DDSRT_REGEX_CLASS_BYTES];
} ddsrt_regex_state_t;

typedef struct ddsrt_regex {
  ddsrt_regex_state_t *states;
  size_t nstates;
  size_t maxstates;
  int start;
  bool owns_states;
} ddsrt_regex_t;

#define DDSRT_REGEX_INITIALIZER { NULL, 0u, 0u, -1, false }

/**
 * @brief Compile a regular expression.
 *
 * Supported syntax is deliberately small: literals, escaping with `\`, `.`,
 * `^`, `$`, character classes with ranges and `^` negation, grouping with
 * parentheses, alternation with `|`, and the `*`, `+` and `?` postfix
 * operators. Matching is byte-oriented and does not implement Unicode
 * character semantics.
 *
 * @param[out] regex  Compiled regular expression. It must be finalized with
 *                    @ref ddsrt_regex_fini after a successful call.
 * @param[in] pattern Null-terminated pattern string.
 *
 * @returns @ref DDS_RETCODE_OK on success, @ref DDS_RETCODE_BAD_PARAMETER for
 *          invalid input or syntax, or @ref DDS_RETCODE_OUT_OF_RESOURCES.
 */
DDS_EXPORT dds_return_t
ddsrt_regex_compile(
  ddsrt_regex_t *regex,
  const char *pattern);

/**
 * @brief Compile a regular expression into caller-provided storage.
 *
 * This avoids allocation while compiling. The @ref ddsrt_regex_match and
 * @ref ddsrt_regex_search functions allocate temporary matching workspace.
 * A storage array of `2 * strlen(pattern) + 2` states is sufficient for any
 * valid pattern.
 *
 * @param[out] regex    Compiled regular expression. It may be finalized with
 *                      @ref ddsrt_regex_fini, but the state storage remains
 *                      owned by the caller.
 * @param[in] pattern   Null-terminated pattern string.
 * @param[in] states    Caller-provided state storage.
 * @param[in] nstates   Number of states in @p states.
 *
 * @returns @ref DDS_RETCODE_OK on success, @ref DDS_RETCODE_BAD_PARAMETER for
 *          invalid input or syntax, or @ref DDS_RETCODE_NOT_ENOUGH_SPACE.
 */
DDS_EXPORT dds_return_t
ddsrt_regex_compile_with_storage(
  ddsrt_regex_t *regex,
  const char *pattern,
  ddsrt_regex_state_t *states,
  size_t nstates);

/**
 * @brief Finalize a compiled regular expression.
 */
DDS_EXPORT void
ddsrt_regex_fini(
  ddsrt_regex_t *regex)
ddsrt_nonnull_all;

/**
 * @brief Match a regular expression against an entire string.
 */
DDS_EXPORT bool
ddsrt_regex_match(
  const ddsrt_regex_t *regex,
  const char *str)
ddsrt_nonnull_all;

/**
 * @brief Search for a regular expression match in a string.
 */
DDS_EXPORT bool
ddsrt_regex_search(
  const ddsrt_regex_t *regex,
  const char *str)
ddsrt_nonnull_all;

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_REGEX_H */
