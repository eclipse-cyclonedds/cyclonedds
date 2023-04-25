// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef OPTIONS_H
#define OPTIONS_H

#include "idlc/options.h"

#define IDLC_BAD_INPUT (-5) /**< conflicting options or missing "-h" */

int parse_options(int argc, char **argv, idlc_option_t **options);
void print_help(const char *argv0, const char *rest, idlc_option_t **options);
void print_usage(const char *argv0, const char *rest);

#endif /* OPTIONS_H */
