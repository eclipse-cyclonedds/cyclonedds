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
#include <ctype.h>
#include <stddef.h>

#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_lat_estim.h"
#include <stdlib.h>
#include <string.h>

void nn_lat_estim_init (struct nn_lat_estim *le)
{
  int i;
  le->index = 0;
  for (i = 0; i < NN_LAT_ESTIM_MEDIAN_WINSZ; i++)
    le->window[i] = 0;
  le->smoothed = 0;
}

void nn_lat_estim_fini (UNUSED_ARG (struct nn_lat_estim *le))
{
}

static int cmpfloat (const float *a, const float *b)
{
  return (*a < *b) ? -1 : (*a > *b) ? 1 : 0;
}

void nn_lat_estim_update (struct nn_lat_estim *le, int64_t est)
{
  const float alpha = 0.01f;
  float fest, med;
  float tmp[NN_LAT_ESTIM_MEDIAN_WINSZ];
  if (est <= 0)
    return;
  fest = (float) est / 1e3f; /* we do latencies in microseconds */
  le->window[le->index] = fest;
  if (++le->index == NN_LAT_ESTIM_MEDIAN_WINSZ)
    le->index = 0;
  memcpy (tmp, le->window, sizeof (tmp));
  qsort (tmp, NN_LAT_ESTIM_MEDIAN_WINSZ, sizeof (tmp[0]), (int (*) (const void *, const void *)) cmpfloat);
  med = tmp[NN_LAT_ESTIM_MEDIAN_WINSZ / 2];
  if (le->smoothed == 0 && le->index == 0)
    le->smoothed = med;
  else if (le->smoothed != 0)
    le->smoothed = (1.0f - alpha) * le->smoothed + alpha * med;
}

int nn_lat_estim_log (uint32_t logcat, const struct ddsrt_log_cfg *logcfg, const char *tag, const struct nn_lat_estim *le)
{
  if (le->smoothed == 0.0f)
    return 0;
  else
  {
    float tmp[NN_LAT_ESTIM_MEDIAN_WINSZ];
    int i;
    memcpy (tmp, le->window, sizeof (tmp));
    qsort (tmp, NN_LAT_ESTIM_MEDIAN_WINSZ, sizeof (tmp[0]), (int (*) (const void *, const void *)) cmpfloat);
    if (tag)
      DDS_CLOG (logcat, logcfg, " LAT(%s: %e {", tag, le->smoothed);
    else
      DDS_CLOG (logcat, logcfg, " LAT(%e {", le->smoothed);
    for (i = 0; i < NN_LAT_ESTIM_MEDIAN_WINSZ; i++)
      DDS_CLOG (logcat, logcfg, "%s%e", (i > 0) ? "," : "", tmp[i]);
    DDS_CLOG (logcat, logcfg, "})");
    return 1;
  }
}

#if 0 /* not implemented yet */
double nn_lat_estim_current (const struct nn_lat_estim *le)
{
}
#endif
