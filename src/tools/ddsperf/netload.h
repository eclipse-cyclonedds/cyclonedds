// Copyright(c) 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef NETLOAD_H
#define NETLOAD_H

#include <dds/dds.h>

struct record_netload_state;

void record_netload (struct record_netload_state *st, const char *prefix, dds_time_t tnow);
struct record_netload_state *record_netload_new (const char *dev, double bw);
void record_netload_free (struct record_netload_state *st);

#endif
