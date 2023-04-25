// Copyright(c) 2019 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <inttypes.h>

#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/ddsi_config.h"
#include "ddsi__portmapping.h"

static bool get_port_int (uint32_t *port, const struct ddsi_portmapping *map, enum ddsi_port which, uint32_t domain_id, int32_t participant_index, char *str_if_overflow, size_t strsize)
{
  uint32_t off = UINT32_MAX, ppidx = UINT32_MAX;

  assert (domain_id != UINT32_MAX);
  assert (participant_index >= 0 || participant_index == DDSI_PARTICIPANT_INDEX_NONE);

  switch (which)
  {
    case DDSI_PORT_MULTI_DISC:
      off = map->d0;
      /* multicast port numbers are not affected by participant index */
      ppidx = 0;
      break;
    case DDSI_PORT_MULTI_DATA:
      off = map->d2;
      /* multicast port numbers are not affected by participant index */
      ppidx = 0;
      break;
    case DDSI_PORT_UNI_DISC:
      if (participant_index == DDSI_PARTICIPANT_INDEX_NONE)
      {
        /* participant index "none" means unicast ports get chosen by the transport */
        *port = 0;
        return true;
      }
      off = map->d1;
      ppidx = (uint32_t) participant_index;
      break;
    case DDSI_PORT_UNI_DATA:
      if (participant_index == DDSI_PARTICIPANT_INDEX_NONE)
      {
        /* participant index "none" means unicast ports get chosen by the transport */
        *port = 0;
        return true;
      }
      off = map->d3;
      ppidx = (uint32_t) participant_index;
      break;
  }

  const uint64_t a = (uint64_t) map->dg * domain_id;
  const uint64_t b = map->base + (uint64_t) map->pg * ppidx + off;

  /* For the mapping to be valid, the port number must be in range of an unsigned 32 bit integer and must
     not be 0 (as that is used for indicating a random port should be selected by the transport).  The
     transports may limit this further, but at least we won't have to worry about overflow anymore. */
  *port = (uint32_t) (a + b);
  if (a <= UINT32_MAX && b <= UINT32_MAX - a && *port > 0)
    return true;
  else
  {
    /* a, b < 2^64 ~ 18e18; 2^32 <= a + b < 2^65 ~ 36e18
       2^32 ~ 4e9, so it can easily be split into (a+b) `div` 1e9 and (a+b) `mod` 1e9
       and then the most-significant part is guaranteed to be > 0 */
    const uint32_t billion = 1000000000;
    const uint32_t y = (uint32_t) (a % billion) + (uint32_t) (b % billion);
    const uint64_t x = (a / billion) + (b / billion) + (y / billion);
    snprintf (str_if_overflow, strsize, "%"PRIu64"%09"PRIu32, x, y % billion);
    return false;
  }
}

static const char *portname (enum ddsi_port which)
{
  const char *n = "?";
  switch (which)
  {
    case DDSI_PORT_MULTI_DISC: n = "multicast discovery"; break;
    case DDSI_PORT_MULTI_DATA: n = "multicast data"; break;
    case DDSI_PORT_UNI_DISC: n = "unicast discovery"; break;
    case DDSI_PORT_UNI_DATA: n = "unicast data"; break;
  }
  return n;
}

bool ddsi_valid_portmapping (const struct ddsi_config *config, int32_t participant_index, char *msg, size_t msgsize)
{
  DDSRT_STATIC_ASSERT (DDSI_PORT_MULTI_DISC >= 0 &&
                       DDSI_PORT_MULTI_DISC + 1 == DDSI_PORT_MULTI_DATA &&
                       DDSI_PORT_MULTI_DATA + 1 == DDSI_PORT_UNI_DISC &&
                       DDSI_PORT_UNI_DISC + 1 == DDSI_PORT_UNI_DATA &&
                       DDSI_PORT_UNI_DATA >= 0);
  uint32_t dummy_port;
  char str[32];
  bool ok = true;
  enum ddsi_port which = DDSI_PORT_MULTI_DISC;
  int n = snprintf (msg, msgsize, "port number(s) of out range:");
  size_t pos = (n >= 0 && (size_t) n <= msgsize) ? (size_t) n : msgsize;
  do {
    if (!get_port_int (&dummy_port, &config->ports, which, config->extDomainId.value, participant_index, str, sizeof (str)))
    {
      n = snprintf (msg + pos, msgsize - pos, "%s %s %s", ok ? "" : ",", portname (which), str);
      if (n >= 0 && (size_t) n <= msgsize - pos)
        pos += (size_t) n;
      ok = false;
    }
  } while (which++ != DDSI_PORT_UNI_DATA);
  return ok;
}

uint32_t ddsi_get_port (const struct ddsi_config *config, enum ddsi_port which, int32_t participant_index)
{
  /* Not supposed to come here if port mapping is invalid */
  uint32_t port;
  char str[32];
  bool ok = get_port_int (&port, &config->ports, which, config->extDomainId.value, participant_index, str, sizeof (str));
  assert (ok);
  (void) ok;
  return port;
}
