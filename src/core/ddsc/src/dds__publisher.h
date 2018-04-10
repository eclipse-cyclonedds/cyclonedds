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
#ifndef _DDS_PUBLISHER_H_
#define _DDS_PUBLISHER_H_

#include "ddsc/dds.h"

#if defined (__cplusplus)
extern "C" {
#endif

dds_return_t dds_publisher_begin_coherent (dds_entity_t e);
dds_return_t dds_publisher_end_coherent (dds_entity_t e);

#if defined (__cplusplus)
}
#endif
#endif /* _DDS_PUBLISHER_H_ */
