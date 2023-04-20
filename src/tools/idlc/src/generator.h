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
#include "idlc/generator.h"

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
    struct idlc_generator_config c;
    char *export_macro;
    bool generate_cdrstream_desc;
  } config;
};

int print_type(char *str, size_t len, const void *ptr, void *user_data);
int print_scoped_name(char *str, size_t len, const void *ptr, void *user_data);

#if _WIN32
__declspec(dllexport)
#endif
const idlc_option_t** idlc_generator_options(void);

#if _WIN32
__declspec(dllexport)
#endif
idl_retcode_t idlc_generate(const idl_pstate_t *pstate, const idlc_generator_config_t *config);

#if _WIN32
__declspec(dllexport)
#endif
idl_retcode_t generate_nosetup(const idl_pstate_t *pstate, struct generator *generator);

#endif /* GENERATOR_H */
