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
#ifndef _DDS_READCOND_H_
#define _DDS_READCOND_H_

#include "dds__entity.h"

_Must_inspect_result_ dds_readcond*
dds_create_readcond(
        _In_ dds_reader *rd,
        _In_ dds_entity_kind_t kind,
        _In_ uint32_t mask,
        _In_opt_ dds_querycondition_filter_fn filter);

#endif
