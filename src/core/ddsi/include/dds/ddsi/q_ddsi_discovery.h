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
#ifndef NN_DDSI_DISCOVERY_H
#define NN_DDSI_DISCOVERY_H

#include "dds/ddsi/q_unused.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct participant;
struct writer;
struct reader;
struct nn_rsample_info;
struct nn_rdata;
struct ddsi_plist;

struct participant_builtin_topic_data_locators {
  struct nn_locators_one def_uni_loc_one, def_multi_loc_one, meta_uni_loc_one, meta_multi_loc_one;
};

void get_participant_builtin_topic_data (const struct participant *pp, ddsi_plist_t *dst, struct participant_builtin_topic_data_locators *locs);

int spdp_write (struct participant *pp);
int spdp_dispose_unregister (struct participant *pp);

int sedp_write_writer (struct writer *wr);
int sedp_write_reader (struct reader *rd);
int sedp_dispose_unregister_writer (struct writer *wr);
int sedp_dispose_unregister_reader (struct reader *rd);

int builtins_dqueue_handler (const struct nn_rsample_info *sampleinfo, const struct nn_rdata *fragchain, const ddsi_guid_t *rdguid, void *qarg);

#if defined (__cplusplus)
}
#endif

#endif /* NN_DDSI_DISCOVERY_H */
