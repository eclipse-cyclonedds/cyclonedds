/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef CPUTIME_H
#define CPUTIME_H

#include "ddsperf_types.h"

struct record_cputime_state;

struct record_cputime_state *record_cputime_new (dds_entity_t wr);
void record_cputime_free (struct record_cputime_state *state);
bool record_cputime (FILE *fp, struct record_cputime_state *state, const char *prefix, dds_time_t tnow, bool data_changed, bool *cputime_changed);
double record_cputime_read_rss (const struct record_cputime_state *state);
bool print_cputime (FILE *fp, const struct CPUStats *s, struct record_cputime_state *cputime_state, const char *prefix, bool print_host, bool is_fresh, bool data_changed, bool *cputime_changed, bool read);

#endif
