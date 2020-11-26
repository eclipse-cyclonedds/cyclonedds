#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <ws2def.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <dds/ddsrt/heap.h>
#include <dds/ddsrt/string.h>
#include <dds/ddsrt/netstat.h>

struct ddsrt_netstat_control {
  wchar_t *name;
  bool errored;
  bool have_index;
  NET_IFINDEX index;
};

static void copy_data (struct ddsrt_netstat *dst, const MIB_IF_ROW2 *src)
{
  dst->ipkt = src->InUcastPkts + src->InNUcastPkts;
  dst->opkt = src->OutUcastPkts + src->OutNUcastPkts;
  dst->ibytes = src->InOctets;
  dst->obytes = src->OutOctets;
}

static bool is_desired_interface (const struct ddsrt_netstat_control *control, const MIB_IF_ROW2 *info)
{
  return wcscmp (control->name, info->Description) == 0 || wcscmp (control->name, info->Alias) == 0;
}

static dds_return_t ddsrt_netstat_get_int (struct ddsrt_netstat_control *control, struct ddsrt_netstat *stats)
{
  if (control->errored)
    return DDS_RETCODE_ERROR;

  if (control->have_index)
  {
    MIB_IF_ROW2 info;
    memset (&info, 0, sizeof (info));
    info.InterfaceIndex = control->index;
    if (GetIfEntry2 (&info) != NO_ERROR || !is_desired_interface (control, &info))
      control->have_index = false;
    else
    {
      copy_data (stats, &info);
      return DDS_RETCODE_OK;
    }
  }

  MIB_IF_TABLE2 *table;
  if (GetIfTable2 (&table) != NO_ERROR)
    goto error;
  control->have_index = false;
  for (ULONG row = 0; row < table->NumEntries; row++)
  {
    if (is_desired_interface (control, &table->Table[row]))
    {
      control->index = table->Table[row].InterfaceIndex;
      control->have_index = true;
      copy_data (stats, &table->Table[row]);
      break;
    }
  }
  FreeMibTable (table);
  return control->have_index ? DDS_RETCODE_OK : DDS_RETCODE_NOT_FOUND;

 error:
  control->errored = true;
  return DDS_RETCODE_ERROR;
}

dds_return_t ddsrt_netstat_new (struct ddsrt_netstat_control **control, const char *device)
{
  struct ddsrt_netstat_control *c = ddsrt_malloc (sizeof (*c));
  struct ddsrt_netstat dummy;
  size_t name_size = strlen (device) + 1;
  c->name = ddsrt_malloc (name_size * sizeof (*c->name));
  size_t cnt = 0;
  mbstowcs_s (&cnt, c->name, name_size, device, _TRUNCATE);
  c->have_index = false;
  c->errored = false;
  c->index = 0;
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
  return ddsrt_netstat_get_int (control, stats);
}
