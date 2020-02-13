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
#ifndef DDS__WHC_H
#define DDS__WHC_H

#include "dds/ddsi/q_whc.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;
struct whc_writer_info;
struct dds_writer;

struct whc *whc_new (struct ddsi_domaingv *gv, const struct whc_writer_info *wrinfo);
struct whc_writer_info *whc_make_wrinfo (struct dds_writer *wr, const dds_qos_t *qos);
void whc_free_wrinfo (struct whc_writer_info *);

#if defined (__cplusplus)
}
#endif

#endif /* Q_WHC_H */
