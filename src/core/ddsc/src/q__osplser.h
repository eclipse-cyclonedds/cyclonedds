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
#ifndef DDS_OSPLSER_H
#define DDS_OSPLSER_H

#include "ddsi/ddsi_ser.h"
#include "ddsi/q_xmsg.h"

#if defined (__cplusplus)
extern "C" {
#endif

int serdata_cmp (const struct serdata * a, const struct serdata * b);
uint32_t serdata_hash (const struct serdata *a);

serdata_t serialize (serstatepool_t pool, const struct sertopic * tp, const void * sample);
serdata_t serialize_key (serstatepool_t pool, const struct sertopic * tp, const void * sample);

void deserialize_into (void *sample, const struct serdata *serdata);
void free_deserialized (const struct serdata *serdata, void *vx);

void sertopic_free (struct sertopic * tp);
void serstate_set_key (serstate_t st, int justkey, const void *key);
void serstate_init (serstate_t st, const struct sertopic * topic);
void serstate_free (serstate_t st);

#if defined (__cplusplus)
}
#endif
#endif
