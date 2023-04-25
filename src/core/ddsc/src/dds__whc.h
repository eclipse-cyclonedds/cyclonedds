// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__WHC_H
#define DDS__WHC_H

#include "dds/ddsi/ddsi_whc.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;
struct whc_writer_info;
struct dds_writer;

/** @component whc */
struct ddsi_whc *dds_whc_new (struct ddsi_domaingv *gv, const struct whc_writer_info *wrinfo);

/** @component whc */
struct whc_writer_info *dds_whc_make_wrinfo (struct dds_writer *wr, const dds_qos_t *qos);

/** @component whc */
void dds_whc_free_wrinfo (struct whc_writer_info *);

#if defined (__cplusplus)
}
#endif

#endif /* DDS__WHC_H */
