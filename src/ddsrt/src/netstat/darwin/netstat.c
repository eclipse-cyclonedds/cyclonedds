#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if.h>
#include <net/if_mib.h>
#include <errno.h>

#include <dds/ddsrt/heap.h>
#include <dds/ddsrt/string.h>
#include <dds/ddsrt/netstat.h>

struct ddsrt_netstat_control {
  char *name;
  int cached_row;
};

static dds_return_t ddsrt_netstat_get_int (struct ddsrt_netstat_control *control, struct ddsrt_netstat *stats)
{
  int name[6];
  size_t len;
  int count;
  struct ifmibdata ifmd;

  if (control->cached_row > 0)
  {
    name[0] = CTL_NET;
    name[1] = PF_LINK;
    name[2] = NETLINK_GENERIC;
    name[3] = IFMIB_IFDATA;
    name[4] = control->cached_row;
    name[5] = IFDATA_GENERAL;
    len = sizeof (ifmd);
    if (sysctl (name, 6, &ifmd, &len, NULL, 0) != 0)
      control->cached_row = 0;
    else if (strcmp (ifmd.ifmd_name, control->name) != 0)
      control->cached_row = 0;
  }

  if (control->cached_row == 0)
  {
    name[0] = CTL_NET;
    name[1] = PF_LINK;
    name[2] = NETLINK_GENERIC;
    name[3] = IFMIB_SYSTEM;
    name[4] = IFMIB_IFCOUNT;
    len = sizeof (count);
    if (sysctl (name, 5, &count, &len, NULL, 0) != 0)
      goto error;
    for (int row = 1; row <= count; row++)
    {
      name[0] = CTL_NET;
      name[1] = PF_LINK;
      name[2] = NETLINK_GENERIC;
      name[3] = IFMIB_IFDATA;
      name[4] = row;
      name[5] = IFDATA_GENERAL;
      len = sizeof (ifmd);
      if (sysctl (name, 6, &ifmd, &len, NULL, 0) != 0)
      {
        if (errno != ENOENT)
          goto error;
      }
      else if (strcmp (control->name, ifmd.ifmd_name) == 0)
      {
        control->cached_row = row;
        break;
      }
    }
  }

  if (control->cached_row == 0)
    return DDS_RETCODE_NOT_FOUND;
  else
  {
    stats->ipkt = ifmd.ifmd_data.ifi_ipackets;
    stats->opkt = ifmd.ifmd_data.ifi_opackets;
    stats->ibytes = ifmd.ifmd_data.ifi_ibytes;
    stats->obytes = ifmd.ifmd_data.ifi_obytes;
    return DDS_RETCODE_OK;
  }

 error:
  control->cached_row = -1;
  return DDS_RETCODE_ERROR;
}

dds_return_t ddsrt_netstat_new (struct ddsrt_netstat_control **control, const char *device)
{
  struct ddsrt_netstat_control *c = ddsrt_malloc (sizeof (*c));
  struct ddsrt_netstat dummy;
  c->name = ddsrt_strdup (device);
  c->cached_row = 0;
  if (ddsrt_netstat_get_int (c, &dummy) != DDS_RETCODE_OK)
  {
    ddsrt_free (c->name);
    ddsrt_free (c);
    *control = NULL;
    return DDS_RETCODE_ERROR;
  }
  else
  {
    *control = c;
    return DDS_RETCODE_OK;
  }
}

dds_return_t ddsrt_netstat_free (struct ddsrt_netstat_control *control)
{
  ddsrt_free (control->name);
  ddsrt_free (control);
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_netstat_get (struct ddsrt_netstat_control *control, struct ddsrt_netstat *stats)
{
  if (control->cached_row < 0)
    return DDS_RETCODE_ERROR;
  else
    return ddsrt_netstat_get_int (control, stats);
}
