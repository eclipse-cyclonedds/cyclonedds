// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdio.h>

#include "idl/processor.h"
#include "libidlc_generator.h"

#include <stdlib.h>
#include <string.h>

struct generator {
  const char *path;
  struct {
    FILE *handle;
    char *path;
  } header;
  struct {
    FILE *handle;
    char *path;
  } source;
  struct {
    idlc_generator_config_t c;
    char *export_macro;
    char *guard_macro;
    bool generate_cdrstream_desc;
  } config;
};

#endif /* GENERATOR_H */
