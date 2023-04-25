// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_LAT_ESTIM_H
#define DDSI_LAT_ESTIM_H

#include "dds/ddsi/ddsi_log.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDSI_LAT_ESTIM_MEDIAN_WINSZ 7

struct ddsi_lat_estim {
  /* median filtering with a small window in an attempt to remove the
     worst outliers */
  int index;
  float window[DDSI_LAT_ESTIM_MEDIAN_WINSZ];
  /* simple alpha filtering for smoothing */
  float smoothed;
};

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_LAT_ESTIM_H */
