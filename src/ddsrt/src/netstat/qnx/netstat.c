#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <devctl.h>
#include <sys/ioctl.h>
#include <sys/dcmd_io-net.h>
#include <net/if.h>
#include <net/ifdrvcom.h>

#include <dds/ddsrt/heap.h>
#include <dds/ddsrt/string.h>
#include <dds/ddsrt/netstat.h>
#include <dds/ddsrt/log.h>

struct ddsrt_netstat_control {
  char *name;
};

dds_return_t ddsrt_netstat_get (struct ddsrt_netstat_control *control, struct ddsrt_netstat *stats)
{
  struct drvcom_stats dstats;
  nic_stats_t *ifstats;
  int s;

  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    return DDS_RETCODE_ERROR;
  }

  (void)strncpy(dstats.dcom_cmd.ifdc_name, control->name, IFNAMSIZ);
  dstats.dcom_cmd.ifdc_cmd = DRVCOM_STATS;
  dstats.dcom_cmd.ifdc_len = sizeof(*ifstats);
  ifstats = &dstats.dcom_stats;

  if (devctl(s, SIOCGDRVCOM, &dstats, sizeof(dstats), 0) != EOK) {
    DDS_ERROR("devctl() failed for SIOCGDRVCOM for if '%s': %s\n", 
      dstats.dcom_cmd.ifdc_name, strerror(errno));
    close(s);
    return DDS_RETCODE_ERROR;
  }
  stats->ipkt = ifstats->rxed_ok;
  stats->opkt = ifstats->txed_ok;
  stats->ibytes = ifstats->octets_rxed_ok;
  stats->obytes = ifstats->octets_txed_ok;
  
  close(s);
  return DDS_RETCODE_OK;
}


dds_return_t ddsrt_netstat_new (struct ddsrt_netstat_control **control, const char *device)
{
  struct ddsrt_netstat_control *c = ddsrt_malloc (sizeof (*c));
  struct ddsrt_netstat dummy;
  c->name = ddsrt_strdup (device);
  if (ddsrt_netstat_get (c, &dummy) != DDS_RETCODE_OK)
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
