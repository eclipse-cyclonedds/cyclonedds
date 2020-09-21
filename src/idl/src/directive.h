/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DIRECTIVE_H
#define DIRECTIVE_H

#include <stdbool.h>

typedef struct idl_line idl_line_t;
struct idl_line {
  idl_symbol_t symbol;
  idl_literal_t *line;
  idl_literal_t *file;
  bool extra_tokens;
};

typedef struct idl_keylist idl_keylist_t;
struct idl_keylist {
  idl_symbol_t symbol;
  idl_name_t *data_type;
  idl_name_t **keys;
};

idl_retcode_t idl_parse_directive(idl_processor_t *proc, idl_token_t *tok);

#endif /* DIRECTIVE_H */
