// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>

#include "dds/dds.h"
#include "dds__entity.h"
#include "dds__subscriber.h"
#include "dds__publisher.h"

dds_return_t dds_begin_coherent (dds_entity_t entity)
{
  static const dds_entity_kind_t kinds[] = { DDS_KIND_READER, DDS_KIND_WRITER, DDS_KIND_PUBLISHER, DDS_KIND_SUBSCRIBER };
  return dds_generic_unimplemented_operation_manykinds (entity, sizeof (kinds) / sizeof (kinds[0]), kinds);
}

dds_return_t dds_end_coherent (dds_entity_t entity)
{
  static const dds_entity_kind_t kinds[] = { DDS_KIND_READER, DDS_KIND_WRITER, DDS_KIND_PUBLISHER, DDS_KIND_SUBSCRIBER };
  return dds_generic_unimplemented_operation_manykinds (entity, sizeof (kinds) / sizeof (kinds[0]), kinds);
}
