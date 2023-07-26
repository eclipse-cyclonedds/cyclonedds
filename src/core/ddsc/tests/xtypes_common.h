// Copyright(c) 2022 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef TEST_XTYPES_COMMON_H
#define TEST_XTYPES_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include "dds/dds.h"
#include "dds/version.h"
#include "ddsi__xt_impl.h"

typedef void (*typeobj_modify) (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *type_id_obj_seq, uint32_t kind);

void typeinfo_ser (struct dds_type_meta_ser *ser, DDS_XTypes_TypeInformation *ti);
void typeinfo_deser (DDS_XTypes_TypeInformation **ti, const struct dds_type_meta_ser *ser);
void typemap_ser (struct dds_type_meta_ser *ser, DDS_XTypes_TypeMapping *tmap);
void typemap_deser (DDS_XTypes_TypeMapping **tmap, const struct dds_type_meta_ser *ser);
void test_proxy_rd_create (struct ddsi_domaingv *gv, const char *topic_name, DDS_XTypes_TypeInformation *ti, dds_return_t exp_ret, const ddsi_guid_t *pp_guid, const ddsi_guid_t *rd_guid);
void test_proxy_rd_fini (struct ddsi_domaingv *gv, const ddsi_guid_t *pp_guid, const ddsi_guid_t *rd_guid);
void xtypes_util_modify_type_meta (dds_topic_descriptor_t *dst_desc, const dds_topic_descriptor_t *src_desc, typeobj_modify mod, bool update_typeinfo, uint32_t kind);

#endif /* TEST_XTYPES_COMMON_H */
