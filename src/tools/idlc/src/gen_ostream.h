/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSTS_GEN_OSTREAM_H
#define DDSTS_GEN_OSTREAM_H

typedef struct ddsts_ostream_t ddsts_ostream_t;
struct ddsts_ostream_t {
  bool (*open)(ddsts_ostream_t *ostream, const char *name);
  void (*close)(ddsts_ostream_t *ostream);
  void (*put)(ddsts_ostream_t *ostream, char ch);
};

bool ddsts_ostream_open(ddsts_ostream_t *ostream, const char *name);
void ddsts_ostream_close(ddsts_ostream_t *ostream);
void ddsts_ostream_put(ddsts_ostream_t *ostream, char ch);
void ddsts_ostream_puts(ddsts_ostream_t *ostream, const char *str);

dds_return_t ddsts_create_ostream_to_null(ddsts_ostream_t **ref_ostream);
dds_return_t ddsts_create_ostream_to_files(ddsts_ostream_t **ref_ostream);
dds_return_t ddsts_create_ostream_to_buffer(char *buffer, size_t len, ddsts_ostream_t **ref_ostream);

#endif /* DDSTS_GEN_OSTREAM_H */
