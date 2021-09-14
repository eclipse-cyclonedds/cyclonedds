/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef IDLC_H
#define IDLC_H

#include <stdbool.h>

#include "idl/processor.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define IDLC_NO_MEMORY (-1)
#define IDLC_BAD_OPTION (-2)
#define IDLC_NO_ARGUMENT (-3)
#define IDLC_BAD_ARGUMENT (-4)

typedef struct idlc_option idlc_option_t;
struct idlc_option {
  enum {
    IDLC_FLAG, /**< flag-only, i.e. (sub)option without argument */
    IDLC_STRING,
    IDLC_FUNCTION,
  } type;
  union {
    int *flag;
    const char **string;
    int (*function)(const idlc_option_t *, const char *);
  } store;
  char option; /**< option, i.e. "o" in "-o". "-h" is reserved */
  char *suboption; /**< name of suboption, i.e. "mount" in "-o mount" */
  char *argument;
  char *help;
};

typedef struct idlc_config idlc_config_t;
struct idlc_config {
  const char *file; /**< Path of input file or "-" for STDIN */
  const char *prefix; /**< Path to prefix output path with */
  const char *language; /**< Language e.g. cxx or path to generator library */
  int compile;
  int preprocess;
  int keylist;
  int case_sensitive;
  int help;
  int version;
  /* (emulated) command line options for mcpp (idlpp) */
  int argc;
  const char **argv;
};

/** Compiler options as used by idlc not specific to any backend */
extern const idlc_config_t *idlc_config;

#define IDLC_GENERATOR_OPTIONS generator_options
#define IDLC_GENERATOR_ANNOTATIONS generator_annotations
#define IDLC_GENERATE generate

typedef const idlc_option_t **(*idlc_generator_options_t)(void);
typedef const idl_builtin_annotation_t **(*idlc_generator_annotations_t)(void);
typedef int(*idlc_generate_t)(const idl_pstate_t *);

#if defined(__cplusplus)
}
#endif

#endif /* IDLC_H */
