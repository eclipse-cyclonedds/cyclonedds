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
#ifndef CMP_H
#define CMP_H

#include <stdlib.h>
#include <string.h>

#define STR16 "abcdef0123456789"
#define STR128 STR16 STR16 STR16 STR16 STR16 STR16 STR16 STR16

#define SEQA(s,l) \
{ \
  s._length = s._maximum = l; \
  s._release = true; \
  s._buffer = dds_alloc (l * sizeof (*s._buffer)); \
}

#define CMP(a,b,f,n) { if ((a->f) != n) return -2; if ((a->f) != (b->f)) return (a->f) > (b->f) ? 1 : -1; }
#define CMPSTR(a,b,f,s) { if (strcmp(a->f, s)) return -2; if (strcmp((a->f), (b->f))) return strcmp((a->f), (b->f)); }

#endif /* CMP_H */
