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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "dds/dds.h"

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/netstat.h"

#include "netload.h"

#if DDSRT_HAVE_NETSTAT

struct record_netload_state {
  struct ddsrt_netstat_control *ctrl;
  char *name;
  double bw;
  bool errored;
  bool data_valid;
  dds_time_t tprev;
  uint64_t ibytes;
  uint64_t obytes;
};

void record_netload (FILE *fp, struct record_netload_state *st, const char *prefix, dds_time_t tnow, bool cputime_changed, bool *netload_changed)
{
  *netload_changed = false;
  if (!cputime_changed)
    return;
  if (st && !st->errored)
  {
    struct ddsrt_netstat x;
    dds_return_t ret = ddsrt_netstat_get (st->ctrl, &x);
    st->errored = (ret == DDS_RETCODE_ERROR);
    if (ret == DDS_RETCODE_OK)
    {
      if (st->data_valid)
      {
        /* interface speeds are in bits/s, so convert bytes to bits */
        const double dt = (double) (tnow - st->tprev) / 1e9;
        const double dx = 8 * (double) (x.obytes - st->obytes) / dt;
        const double dr = 8 * (double) (x.ibytes - st->ibytes) / dt;
        if (st->bw > 0)
        {
          const double dxpct = 100.0 * dx / st->bw;
          const double drpct = 100.0 * dr / st->bw;
          //if (dxpct >= 0.5 || drpct >= 0.5)
          {
            printf ("%s %s: xmit %.2f%% recv %.2f%% [%"PRIu64" %"PRIu64"]\n",
                    prefix, st->name, dxpct, drpct, x.obytes, x.ibytes);
            if (fp != NULL)
            {
              fprintf(fp, ",%.2f,%.2f,%"PRIu64",%"PRIu64,
                          /* col50 : dxpct  */dxpct,
                          /* col51 : drpct  */drpct,
                          /* col52 : obytes */x.obytes,
                          /* col53 : ibytes */x.ibytes);
              fflush(fp);
              *netload_changed = true;
            }
          }
        }
        else //if (dx >= 1e5 || dr >= 1e5) // 100kb/s is arbitrary
        {
          printf ("%s %s: xmit %.2f Mb/s recv %.2f Mb/s [%"PRIu64" %"PRIu64"]\n",
                  prefix, st->name, dx / 1e6, dr / 1e6, x.obytes, x.ibytes);
          if (fp != NULL)
          {
            fprintf(fp, ",%.2f,%.2f,%"PRIu64",%"PRIu64,
                        /* col50 : dxpct  */dx / 1e6,
                        /* col51 : drpct  */dr / 1e6,
                        /* col52 : obytes */x.obytes,
                        /* col53 : ibytes */x.ibytes);
            fflush(fp);
            *netload_changed = true;
          }
        }
      }
      st->obytes = x.obytes;
      st->ibytes = x.ibytes;
      st->tprev = tnow;
      st->data_valid = true;
    }
  }
}

struct record_netload_state *record_netload_new (FILE *fp, const char *dev, double bw, bool cputime_changed, bool *netload_changed)
{
  struct record_netload_state *st = ddsrt_malloc (sizeof (*st));
  if (ddsrt_netstat_new (&st->ctrl, dev) != DDS_RETCODE_OK)
  {
    ddsrt_free (st);
    return NULL;
  }
  st->name = ddsrt_strdup (dev);
  st->bw = bw;
  st->data_valid = false;
  st->errored = false;
  record_netload (fp, st, "", dds_time (), cputime_changed, netload_changed);
  return st;
}

void record_netload_free (struct record_netload_state *st)
{
  if (st)
  {
    ddsrt_netstat_free (st->ctrl);
    ddsrt_free (st->name);
    ddsrt_free (st);
  }
}

#else

void record_netload (struct record_netload_state *st, const char *prefix, dds_time_t tnow)
{
  (void) st;
  (void) prefix;
  (void) tnow;
}

struct record_netload_state *record_netload_new (const char *dev, double bw)
{
  (void) dev;
  (void) bw;
  return NULL;
}

void record_netload_free (struct record_netload_state *st)
{
  (void) st;
}

#endif
