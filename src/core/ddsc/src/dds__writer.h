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
#ifndef _DDS_WRITER_H_
#define _DDS_WRITER_H_

#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define dds_writer_lock(hdl, obj) dds_entity_lock(hdl, DDS_KIND_WRITER, (dds_entity**)obj)
#define dds_writer_unlock(obj)    dds_entity_unlock((dds_entity*)obj);

#if defined (__cplusplus)
}
#endif
#endif
