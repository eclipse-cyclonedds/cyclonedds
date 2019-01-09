/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef __ospli_osplo__tglib__
#define __ospli_osplo__tglib__

#include <stddef.h>

struct tgtype;

struct tgtopic_key {
    char *name; /* field name */
    size_t off; /* from start of data */
    const struct tgtype *type; /* aliases tgtopic::type */
};

struct tgtopic {
    char *name;
    size_t size;
    struct tgtype *type;
    unsigned nkeys;
    struct tgtopic_key *keys;
};

enum tgprint_mode {
    TGPM_DENSE,
    TGPM_SPACE,
    TGPM_FIELDS,
    TGPM_MULTILINE
};

struct tgtopic *tgnew(dds_entity_t tp, int printtype);
void tgfree(struct tgtopic *tp);
void tgprint(FILE *fp, const struct tgtopic *tp, const void *data, enum tgprint_mode mode);
void tgprintkey(FILE *fp, const struct tgtopic *tp, const void *keydata, enum tgprint_mode mode);

void *tgscan(const struct tgtopic *tp, const char *src, char **endp);
void tgfreedata(const struct tgtopic *tp, void *data);

#endif /* defined(__ospli_osplo__tglib__) */
