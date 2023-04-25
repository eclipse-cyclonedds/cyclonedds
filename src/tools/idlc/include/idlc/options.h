// Copyright(c) 2020 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDLC_OPTIONS_H
#define IDLC_OPTIONS_H

#include <stdbool.h>

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

#if defined(__cplusplus)
}
#endif

#endif /* IDLC_OPTIONS_H */
