// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_HBCONTROL_H
#define DDSI_HBCONTROL_H

#include "dds/features.h"
#include "dds/ddsrt/time.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_hbcontrol {
  ddsrt_mtime_t t_of_last_write;
  ddsrt_mtime_t t_of_last_hb;
  ddsrt_mtime_t t_of_last_ackhb;
  ddsrt_mtime_t tsched;
  uint32_t hbs_since_last_write;
  uint32_t last_packetid;
};

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_HBCONTROL_H */
