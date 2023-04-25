// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef SCANNER_H
#define SCANNER_H

#include "idl/processor.h"

typedef struct idl_lexeme idl_lexeme_t;
struct idl_lexeme {
  const char *marker;
  const char *limit;
  idl_location_t location;
};

typedef struct idl_token idl_token_t;
struct idl_token {
  int32_t code; /**< token identifier */
  union {
    char chr;
    unsigned long long ullng;
    long double ldbl;
    char *str;
  } value;
  idl_location_t location;
};

IDL_EXPORT idl_retcode_t idl_scan(idl_pstate_t *pstate, idl_token_t *tok);

#endif /* SCANNER_H */
