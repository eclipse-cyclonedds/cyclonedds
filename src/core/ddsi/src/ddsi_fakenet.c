// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dds/config.h"

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#endif

#if (defined (__unix__) || defined (__APPLE__)) && !LWIP_SOCKET
#include <fcntl.h>
#include <unistd.h>
#define DDSI_FAKENET_USE_PIPE 1
#else
#define DDSI_FAKENET_USE_PIPE 0
#endif

#if ((defined (__APPLE__) && defined (MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7)) || defined (__linux)) && !LWIP_SOCKET
#define DDSI_FAKENET_HAVE_IP_MREQN 1
#else
#define DDSI_FAKENET_HAVE_IP_MREQN 0
#endif

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/xmlparser.h"
#include "ddsi__eth.h"
#include "ddsi__fakenet.h"

static const char fakenet_default_topology[] =
  "<FakeNetwork>"
  "  <Host name=\"host0\">"
  "    <Interface name=\"lo\" index=\"1\" address=\"127.0.0.1\" netmask=\"255.0.0.0\" flags=\"up,loopback,multicast\" type=\"unknown\"/>"
  "    <Interface name=\"fake0\" index=\"2\" address=\"192.0.2.1\" netmask=\"255.255.255.0\" flags=\"up,multicast\" type=\"wired\"/>"
  "    <Interface name=\"fake1\" index=\"3\" address=\"198.51.100.1\" netmask=\"255.255.255.0\" flags=\"up,multicast\" type=\"wired\"/>"
  "  </Host>"
  "</FakeNetwork>";

struct fake_interface {
  char *name;
  uint32_t index;
  uint32_t flags;
  enum ddsrt_iftype type;
  struct sockaddr_storage addr;
  struct sockaddr_storage netmask;
  bool has_addr;
  bool has_netmask;
};

struct fake_host {
  char *name;
  struct fake_interface *interfaces;
  size_t n_interfaces;
};

struct fake_membership {
  struct in_addr group;
  struct in_addr interf;
  uint32_t if_index;
  struct fake_membership *next;
};

struct fake_packet {
  struct fake_packet *next;
  struct sockaddr_storage src;
  struct sockaddr_storage dst;
  uint32_t if_index;
  size_t size;
  unsigned char data[];
};

struct fake_socket {
  ddsrt_socket_t rd;
  ddsrt_socket_t wr;
  int domain;
  int type;
  int protocol;
  bool bound;
  bool reuse;
  bool multicast_loopback;
  bool multicast_if_valid;
  bool pktinfo_enabled;
  struct in_addr multicast_if;
  uint32_t multicast_if_index;
  struct sockaddr_storage addr;
  const struct fake_host *host;
  struct fake_membership *memberships;
  struct fake_packet *queue_head;
  struct fake_packet *queue_tail;
};

enum fake_topology_source {
  FAKE_TOPOLOGY_NONE,
  FAKE_TOPOLOGY_XML_STRING,
  FAKE_TOPOLOGY_XML_FILE,
  FAKE_TOPOLOGY_DEFAULT,
  FAKE_TOPOLOGY_REAL
};

static ddsrt_once_t fakenet_once = DDSRT_ONCE_INIT;
static ddsrt_mutex_t fakenet_lock;
static struct fake_host *fakenet_hosts;
static size_t fakenet_nhosts;
static struct fake_socket **fakenet_sockets;
static size_t fakenet_nsockets;
static const struct fake_host *fakenet_current_host;
static uint16_t fakenet_ephemeral_port = 49152;
static enum fake_topology_source fakenet_topology_source = FAKE_TOPOLOGY_NONE;
static char *fakenet_topology_id;

static void fakenet_init_once (void)
{
  ddsrt_mutex_init (&fakenet_lock);
}

static void fakenet_lock_init (void)
{
  ddsrt_once (&fakenet_once, fakenet_init_once);
}

static socklen_t fakenet_sockaddr_size (const struct sockaddr *addr)
{
  return ddsrt_sockaddr_get_size (addr);
}

static uint16_t sockaddr_port (const struct sockaddr *addr)
{
  return ddsrt_sockaddr_get_port (addr);
}

static void sockaddr_set_port (struct sockaddr *addr, uint16_t port)
{
  assert (addr->sa_family == AF_INET);
  ((struct sockaddr_in *) addr)->sin_port = htons (port);
}

static bool sockaddr_is_any (const struct sockaddr *addr)
{
  return addr->sa_family == AF_INET && ((const struct sockaddr_in *) addr)->sin_addr.s_addr == htonl (INADDR_ANY);
}

static bool sockaddr_addr_eq (const struct sockaddr *a, const struct sockaddr *b)
{
  return a->sa_family == AF_INET && b->sa_family == AF_INET &&
         ((const struct sockaddr_in *) a)->sin_addr.s_addr == ((const struct sockaddr_in *) b)->sin_addr.s_addr;
}

static bool sockaddr_endpoint_overlaps (const struct sockaddr *a, const struct sockaddr *b)
{
  return a->sa_family == b->sa_family &&
         sockaddr_port (a) == sockaddr_port (b) &&
         (sockaddr_is_any (a) || sockaddr_is_any (b) || sockaddr_addr_eq (a, b));
}

static bool sockaddr_is_multicast (const struct sockaddr *addr)
{
  if (addr->sa_family != AF_INET)
    return false;
  const uint32_t x = ntohl (((const struct sockaddr_in *) addr)->sin_addr.s_addr);
  return (x & 0xf0000000u) == 0xe0000000u;
}

static void free_memberships (struct fake_membership *m)
{
  while (m)
  {
    struct fake_membership *next = m->next;
    ddsrt_free (m);
    m = next;
  }
}

static void free_packets (struct fake_packet *p)
{
  while (p)
  {
    struct fake_packet *next = p->next;
    ddsrt_free (p);
    p = next;
  }
}

#if DDSI_FAKENET_USE_PIPE
static int set_fd_nonblocking (int fd)
{
  const int flags = fcntl (fd, F_GETFL, 0);
  if (flags < 0)
    return -1;
  return fcntl (fd, F_SETFL, flags | O_NONBLOCK);
}

static int make_wake_pair (ddsrt_socket_t handles[2])
{
  int fds[2] = { -1, -1 };
  if (pipe (fds) < 0)
    return -1;
  if (set_fd_nonblocking (fds[0]) < 0 || set_fd_nonblocking (fds[1]) < 0)
    goto fail;
  handles[0] = fds[0];
  handles[1] = fds[1];
  return 0;

fail:
  (void) close (fds[0]);
  (void) close (fds[1]);
  return -1;
}

static void close_wake_handle (ddsrt_socket_t handle)
{
  if (handle != DDSRT_INVALID_SOCKET)
    (void) close (handle);
}

static void signal_wake_handle (ddsrt_socket_t handle)
{
  char b = 0;
  ssize_t ret;
  do {
    ret = write (handle, &b, 1);
  } while (ret < 0 && errno == EINTR);
}

static void drain_wake_handle (ddsrt_socket_t handle)
{
  char b;
  ssize_t ret;
  do {
    ret = read (handle, &b, 1);
  } while (ret < 0 && errno == EINTR);
}
#else
static int make_wake_pair (ddsrt_socket_t handles[2])
{
  dds_return_t rc;
  ddsrt_socket_t listener = DDSRT_INVALID_SOCKET;
  struct sockaddr_in addr;
  socklen_t addrlen = (socklen_t) sizeof (addr);

  memset (&addr, 0, sizeof (addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  addr.sin_port = 0;

  if ((rc = ddsrt_socket (&listener, AF_INET, SOCK_STREAM, 0)) != DDS_RETCODE_OK)
    return -1;
  if ((rc = ddsrt_socket (&handles[1], AF_INET, SOCK_STREAM, 0)) != DDS_RETCODE_OK)
    goto fail;
  if ((rc = ddsrt_bind (listener, (struct sockaddr *) &addr, sizeof (addr))) != DDS_RETCODE_OK)
    goto fail;
  if ((rc = ddsrt_getsockname (listener, (struct sockaddr *) &addr, &addrlen)) != DDS_RETCODE_OK)
    goto fail;
  if ((rc = ddsrt_listen (listener, 1)) != DDS_RETCODE_OK)
    goto fail;
  if ((rc = ddsrt_connect (handles[1], (struct sockaddr *) &addr, sizeof (addr))) != DDS_RETCODE_OK)
    goto fail;
  if ((rc = ddsrt_accept (listener, NULL, NULL, &handles[0])) != DDS_RETCODE_OK)
    goto fail;
  if ((rc = ddsrt_setsocknonblocking (handles[0], true)) != DDS_RETCODE_OK)
    goto fail;
  (void) ddsrt_close (listener);
  return 0;

fail:
  if (listener != DDSRT_INVALID_SOCKET)
    (void) ddsrt_close (listener);
  if (handles[0] != DDSRT_INVALID_SOCKET)
    (void) ddsrt_close (handles[0]);
  if (handles[1] != DDSRT_INVALID_SOCKET)
    (void) ddsrt_close (handles[1]);
  return -1;
}

static void close_wake_handle (ddsrt_socket_t handle)
{
  if (handle != DDSRT_INVALID_SOCKET)
    (void) ddsrt_close (handle);
}

static void signal_wake_handle (ddsrt_socket_t handle)
{
  char b = 0;
  size_t n;
  (void) ddsrt_send (handle, &b, 1, 0, &n);
}

static void drain_wake_handle (ddsrt_socket_t handle)
{
  char b;
  size_t n;
  (void) ddsrt_recv (handle, &b, 1, 0, &n);
}
#endif

static void free_socket_locked (struct fake_socket *s)
{
  if (s == NULL)
    return;
  free_memberships (s->memberships);
  free_packets (s->queue_head);
  close_wake_handle (s->rd);
  close_wake_handle (s->wr);
  ddsrt_free (s);
}

static void free_interface (struct fake_interface *intf)
{
  ddsrt_free (intf->name);
}

static void free_host (struct fake_host *host)
{
  ddsrt_free (host->name);
  for (size_t i = 0; i < host->n_interfaces; i++)
    free_interface (&host->interfaces[i]);
  ddsrt_free (host->interfaces);
}

static void clear_locked (void)
{
  for (size_t i = 0; i < fakenet_nsockets; i++)
    free_socket_locked (fakenet_sockets[i]);
  ddsrt_free (fakenet_sockets);
  fakenet_sockets = NULL;
  fakenet_nsockets = 0;

  for (size_t i = 0; i < fakenet_nhosts; i++)
    free_host (&fakenet_hosts[i]);
  ddsrt_free (fakenet_hosts);
  fakenet_hosts = NULL;
  fakenet_nhosts = 0;
  fakenet_current_host = NULL;
  fakenet_ephemeral_port = 49152;
  ddsrt_free (fakenet_topology_id);
  fakenet_topology_id = NULL;
  fakenet_topology_source = FAKE_TOPOLOGY_NONE;
}

static int set_topology_locked (enum fake_topology_source source, const char *id)
{
  char *id_copy = NULL;
  if (id)
  {
    id_copy = ddsrt_strdup (id);
    if (id_copy == NULL)
      return -1;
  }
  ddsrt_free (fakenet_topology_id);
  fakenet_topology_id = id_copy;
  fakenet_topology_source = source;
  return 0;
}

static bool topology_is_loaded_locked (enum fake_topology_source source, const char *id)
{
  if (fakenet_topology_source != source)
    return false;
  if (id == NULL)
    return fakenet_topology_id == NULL;
  return fakenet_topology_id != NULL && strcmp (fakenet_topology_id, id) == 0;
}

static struct fake_socket *lookup_socket_locked (ddsrt_socket_t sock)
{
  for (size_t i = 0; i < fakenet_nsockets; i++)
    if (fakenet_sockets[i] && fakenet_sockets[i]->rd == sock)
      return fakenet_sockets[i];
  return NULL;
}

static const struct fake_host *find_host_locked (const char *name)
{
  for (size_t i = 0; i < fakenet_nhosts; i++)
    if (fakenet_hosts[i].name && strcmp (fakenet_hosts[i].name, name) == 0)
      return &fakenet_hosts[i];
  return NULL;
}

static const struct fake_interface *host_find_interface_by_addr (const struct fake_host *host, const struct sockaddr *addr)
{
  if (host == NULL || addr->sa_family != AF_INET)
    return NULL;
  for (size_t i = 0; i < host->n_interfaces; i++)
  {
    const struct fake_interface *intf = &host->interfaces[i];
    if (intf->has_addr && sockaddr_addr_eq ((const struct sockaddr *) &intf->addr, addr))
      return intf;
  }
  return NULL;
}

static const struct fake_interface *host_find_interface_by_in_addr (const struct fake_host *host, struct in_addr addr)
{
  if (host == NULL)
    return NULL;
  for (size_t i = 0; i < host->n_interfaces; i++)
  {
    const struct fake_interface *intf = &host->interfaces[i];
    if (intf->has_addr && ((const struct sockaddr_in *) &intf->addr)->sin_addr.s_addr == addr.s_addr)
      return intf;
  }
  return NULL;
}

static const struct fake_interface *host_find_interface_by_index (const struct fake_host *host, uint32_t if_index)
{
  if (host == NULL || if_index == 0)
    return NULL;
  for (size_t i = 0; i < host->n_interfaces; i++)
    if (host->interfaces[i].index == if_index)
      return &host->interfaces[i];
  return NULL;
}

static bool host_owns_addr (const struct fake_host *host, const struct sockaddr *addr)
{
  return sockaddr_is_any (addr) || host_find_interface_by_addr (host, addr) != NULL;
}

static const struct fake_interface *host_first_interface (const struct fake_host *host)
{
  if (host == NULL || host->n_interfaces == 0)
    return NULL;
  for (size_t i = 0; i < host->n_interfaces; i++)
    if ((host->interfaces[i].flags & IFF_LOOPBACK) == 0)
      return &host->interfaces[i];
  return &host->interfaces[0];
}

static const struct fake_interface *host_interface_for_destination (const struct fake_host *host, const struct sockaddr *dst)
{
  if (host == NULL || dst->sa_family != AF_INET)
    return NULL;
  const uint32_t dstaddr = ((const struct sockaddr_in *) dst)->sin_addr.s_addr;
  for (size_t i = 0; i < host->n_interfaces; i++)
  {
    const struct fake_interface *intf = &host->interfaces[i];
    if (!intf->has_addr || !intf->has_netmask)
      continue;
    const uint32_t ifaddr = ((const struct sockaddr_in *) &intf->addr)->sin_addr.s_addr;
    const uint32_t mask = ((const struct sockaddr_in *) &intf->netmask)->sin_addr.s_addr;
    if ((dstaddr & mask) == (ifaddr & mask))
      return intf;
  }
  return host_first_interface (host);
}

static bool interfaces_share_link (const struct fake_interface *a, const struct fake_interface *b)
{
  if (a == NULL || b == NULL || !a->has_addr || !b->has_addr)
    return true;
  if (!a->has_netmask || !b->has_netmask)
    return true;
  const uint32_t aaddr = ((const struct sockaddr_in *) &a->addr)->sin_addr.s_addr;
  const uint32_t baddr = ((const struct sockaddr_in *) &b->addr)->sin_addr.s_addr;
  const uint32_t amask = ((const struct sockaddr_in *) &a->netmask)->sin_addr.s_addr;
  const uint32_t bmask = ((const struct sockaddr_in *) &b->netmask)->sin_addr.s_addr;
  return (aaddr & amask) == (baddr & amask) && (aaddr & bmask) == (baddr & bmask);
}

static const struct fake_interface *socket_multicast_interface (const struct fake_socket *s)
{
  const struct fake_interface *intf = NULL;
  if (s->multicast_if_valid)
  {
    intf = host_find_interface_by_index (s->host, s->multicast_if_index);
    if (intf == NULL && s->multicast_if.s_addr != htonl (INADDR_ANY))
      intf = host_find_interface_by_in_addr (s->host, s->multicast_if);
  }
  return intf ? intf : host_first_interface (s->host);
}

static void select_source_address (const struct fake_socket *s, const struct sockaddr *dst, struct sockaddr_storage *src)
{
  memset (src, 0, sizeof (*src));
  if (s->bound && !sockaddr_is_any ((const struct sockaddr *) &s->addr))
  {
    memcpy (src, &s->addr, sizeof (*src));
    return;
  }

  const struct fake_interface *intf = sockaddr_is_multicast (dst) ? socket_multicast_interface (s) : host_interface_for_destination (s->host, dst);
  if (intf)
    memcpy (src, &intf->addr, sizeof (*src));
  else
    ((struct sockaddr_in *) src)->sin_family = AF_INET;
  sockaddr_set_port ((struct sockaddr *) src, s->bound ? sockaddr_port ((const struct sockaddr *) &s->addr) : 0);
}

static int append_host_locked (struct fake_host **host_out)
{
  struct fake_host *new_hosts = ddsrt_realloc (fakenet_hosts, (fakenet_nhosts + 1) * sizeof (*fakenet_hosts));
  if (new_hosts == NULL)
    return -1;
  fakenet_hosts = new_hosts;
  struct fake_host *host = &fakenet_hosts[fakenet_nhosts++];
  memset (host, 0, sizeof (*host));
  *host_out = host;
  return 0;
}

static int append_interface (struct fake_host *host, struct fake_interface *intf)
{
  struct fake_interface *new_interfaces = ddsrt_realloc (host->interfaces, (host->n_interfaces + 1) * sizeof (*host->interfaces));
  if (new_interfaces == NULL)
    return -1;
  host->interfaces = new_interfaces;
  host->interfaces[host->n_interfaces++] = *intf;
  memset (intf, 0, sizeof (*intf));
  return 0;
}

static int parse_uint32 (const char *value, uint32_t *out)
{
  char *endptr = NULL;
  errno = 0;
  unsigned long x = strtoul (value, &endptr, 0);
  if (errno != 0 || endptr == value || *endptr != 0 || x > UINT32_MAX)
    return -1;
  *out = (uint32_t) x;
  return 0;
}

static int parse_flags (const char *value, uint32_t *flags)
{
  char *copy = ddsrt_strdup (value);
  if (copy == NULL)
    return -1;
  char *cursor = copy;
  char *tok;
  uint32_t f = 0;
  while ((tok = ddsrt_strsep (&cursor, ",")) != NULL)
  {
    while (*tok == ' ' || *tok == '\t')
      tok++;
    if (*tok == 0)
      continue;
    if (ddsrt_strcasecmp (tok, "up") == 0)
      f |= IFF_UP;
    else if (ddsrt_strcasecmp (tok, "loopback") == 0)
      f |= IFF_LOOPBACK;
    else if (ddsrt_strcasecmp (tok, "multicast") == 0)
      f |= IFF_MULTICAST;
    else if (ddsrt_strcasecmp (tok, "pointtopoint") == 0 || ddsrt_strcasecmp (tok, "point-to-point") == 0)
      f |= IFF_POINTOPOINT;
    else if (ddsrt_strcasecmp (tok, "broadcast") == 0)
      f |= IFF_BROADCAST;
    else
    {
      ddsrt_free (copy);
      return -1;
    }
  }
  ddsrt_free (copy);
  *flags = f;
  return 0;
}

enum parse_element {
  PE_UNKNOWN,
  PE_ROOT,
  PE_HOST,
  PE_INTERFACE
};

struct fakenet_parse_state {
  struct fake_host *host;
  struct fake_interface intf;
  bool in_interface;
  bool error;
};

static int fakenet_xml_open (void *varg, uintptr_t parentinfo, uintptr_t *eleminfo, const char *name, int line)
{
  (void) line;
  struct fakenet_parse_state *st = varg;
  if (ddsrt_strcasecmp (name, "FakeNetwork") == 0)
  {
    *eleminfo = PE_ROOT;
    return 0;
  }
  else if (ddsrt_strcasecmp (name, "Host") == 0)
  {
    if (parentinfo != PE_ROOT || append_host_locked (&st->host) < 0)
      goto fail;
    *eleminfo = PE_HOST;
    return 0;
  }
  else if (ddsrt_strcasecmp (name, "Interface") == 0)
  {
    if (parentinfo != PE_HOST || st->host == NULL || st->in_interface)
      goto fail;
    memset (&st->intf, 0, sizeof (st->intf));
    st->intf.type = DDSRT_IFTYPE_UNKNOWN;
    st->in_interface = true;
    *eleminfo = PE_INTERFACE;
    return 0;
  }
  else if (ddsrt_strcasecmp (name, "Switch") == 0)
  {
    *eleminfo = parentinfo ? parentinfo : PE_ROOT;
    return 0;
  }

fail:
  st->error = true;
  return -1;
}

static int fakenet_xml_attr (void *varg, uintptr_t eleminfo, const char *name, const char *value, int line)
{
  (void) line;
  struct fakenet_parse_state *st = varg;
  if (eleminfo == PE_HOST)
  {
    if (ddsrt_strcasecmp (name, "name") == 0)
    {
      ddsrt_free (st->host->name);
      st->host->name = ddsrt_strdup (value);
      return st->host->name ? 0 : -1;
    }
  }
  else if (eleminfo == PE_INTERFACE)
  {
    struct fake_interface *intf = &st->intf;
    if (ddsrt_strcasecmp (name, "name") == 0)
    {
      ddsrt_free (intf->name);
      intf->name = ddsrt_strdup (value);
      return intf->name ? 0 : -1;
    }
    else if (ddsrt_strcasecmp (name, "index") == 0)
      return parse_uint32 (value, &intf->index);
    else if (ddsrt_strcasecmp (name, "address") == 0)
    {
      if (ddsrt_sockaddrfromstr (AF_INET, value, &intf->addr) != DDS_RETCODE_OK)
        return -1;
      intf->has_addr = true;
      return 0;
    }
    else if (ddsrt_strcasecmp (name, "netmask") == 0)
    {
      if (ddsrt_sockaddrfromstr (AF_INET, value, &intf->netmask) != DDS_RETCODE_OK)
        return -1;
      intf->has_netmask = true;
      return 0;
    }
    else if (ddsrt_strcasecmp (name, "flags") == 0)
      return parse_flags (value, &intf->flags);
    else if (ddsrt_strcasecmp (name, "type") == 0)
    {
      if (ddsrt_strcasecmp (value, "wired") == 0)
        intf->type = DDSRT_IFTYPE_WIRED;
      else if (ddsrt_strcasecmp (value, "wifi") == 0 || ddsrt_strcasecmp (value, "wireless") == 0)
        intf->type = DDSRT_IFTYPE_WIFI;
      else if (ddsrt_strcasecmp (value, "unknown") == 0)
        intf->type = DDSRT_IFTYPE_UNKNOWN;
      else
        return -1;
      return 0;
    }
  }
  return 0;
}

static int fakenet_xml_data (void *varg, uintptr_t eleminfo, const char *data, int line)
{
  (void) varg;
  (void) eleminfo;
  (void) data;
  (void) line;
  return 0;
}

static int fakenet_xml_close (void *varg, uintptr_t eleminfo, int line)
{
  (void) line;
  struct fakenet_parse_state *st = varg;
  if (eleminfo == PE_INTERFACE)
  {
    if (!st->in_interface || st->intf.name == NULL || !st->intf.has_addr || st->intf.index == 0)
      goto fail;
    if (append_interface (st->host, &st->intf) < 0)
      goto fail;
    st->in_interface = false;
  }
  else if (eleminfo == PE_HOST)
  {
    if (st->host == NULL || st->host->name == NULL)
      goto fail;
    st->host = NULL;
  }
  return 0;

fail:
  st->error = true;
  return -1;
}

static void fakenet_xml_error (void *varg, const char *msg, int line)
{
  (void) msg;
  (void) line;
  ((struct fakenet_parse_state *) varg)->error = true;
}

int ddsi_fakenet_load_xml_string (const char *xml)
{
  static const struct ddsrt_xmlp_callbacks cb = {
    fakenet_xml_open,
    fakenet_xml_attr,
    fakenet_xml_data,
    fakenet_xml_close,
    fakenet_xml_error
  };
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  clear_locked ();
  struct fakenet_parse_state pst;
  memset (&pst, 0, sizeof (pst));
  struct ddsrt_xmlp_state *xp = ddsrt_xmlp_new_string (xml, &pst, &cb);
  if (xp == NULL)
  {
    ddsrt_mutex_unlock (&fakenet_lock);
    return -1;
  }
  const int ret = ddsrt_xmlp_parse (xp);
  ddsrt_xmlp_free (xp);
  if (ret < 0 || pst.error)
  {
    if (pst.in_interface)
      free_interface (&pst.intf);
    clear_locked ();
    ddsrt_mutex_unlock (&fakenet_lock);
    return -1;
  }
  if (fakenet_current_host == NULL && fakenet_nhosts > 0)
    fakenet_current_host = &fakenet_hosts[0];
  if (set_topology_locked (FAKE_TOPOLOGY_XML_STRING, NULL) < 0)
  {
    clear_locked ();
    ddsrt_mutex_unlock (&fakenet_lock);
    return -1;
  }
  ddsrt_mutex_unlock (&fakenet_lock);
  return 0;
}

int ddsi_fakenet_load_xml_file (const char *path)
{
  static const struct ddsrt_xmlp_callbacks cb = {
    fakenet_xml_open,
    fakenet_xml_attr,
    fakenet_xml_data,
    fakenet_xml_close,
    fakenet_xml_error
  };
  FILE *fp = fopen (path, "r");
  if (fp == NULL)
    return -1;
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  clear_locked ();
  struct fakenet_parse_state pst;
  memset (&pst, 0, sizeof (pst));
  struct ddsrt_xmlp_state *xp = ddsrt_xmlp_new_file (fp, &pst, &cb);
  int ret = -1;
  if (xp)
  {
    ret = ddsrt_xmlp_parse (xp);
    ddsrt_xmlp_free (xp);
  }
  (void) fclose (fp);
  if (ret < 0 || pst.error)
  {
    if (pst.in_interface)
      free_interface (&pst.intf);
    clear_locked ();
    ddsrt_mutex_unlock (&fakenet_lock);
    return -1;
  }
  if (fakenet_current_host == NULL && fakenet_nhosts > 0)
    fakenet_current_host = &fakenet_hosts[0];
  if (set_topology_locked (FAKE_TOPOLOGY_XML_FILE, path) < 0)
  {
    clear_locked ();
    ddsrt_mutex_unlock (&fakenet_lock);
    return -1;
  }
  ddsrt_mutex_unlock (&fakenet_lock);
  return 0;
}

int ddsi_fakenet_load_default (void)
{
  const int ret = ddsi_fakenet_load_xml_string (fakenet_default_topology);
  if (ret == 0)
  {
    fakenet_lock_init ();
    ddsrt_mutex_lock (&fakenet_lock);
    if (set_topology_locked (FAKE_TOPOLOGY_DEFAULT, NULL) < 0)
    {
      clear_locked ();
      ddsrt_mutex_unlock (&fakenet_lock);
      return -1;
    }
    ddsrt_mutex_unlock (&fakenet_lock);
  }
  return ret;
}

static int append_real_interface (struct fake_host *host, const ddsrt_ifaddrs_t *ifa, uint32_t fallback_index)
{
  if (ifa->addr == NULL || ifa->addr->sa_family != AF_INET)
    return 0;

  struct fake_interface intf;
  memset (&intf, 0, sizeof (intf));
  intf.name = ddsrt_strdup (ifa->name);
  intf.index = ifa->index ? ifa->index : fallback_index;
  intf.flags = ifa->flags;
  intf.type = ifa->type;
  memcpy (&intf.addr, ifa->addr, fakenet_sockaddr_size (ifa->addr));
  intf.has_addr = true;
  if (ifa->netmask && ifa->netmask->sa_family == AF_INET)
  {
    memcpy (&intf.netmask, ifa->netmask, fakenet_sockaddr_size (ifa->netmask));
    intf.has_netmask = true;
  }

  if (intf.name == NULL || intf.index == 0 || append_interface (host, &intf) < 0)
  {
    free_interface (&intf);
    return -1;
  }
  return 1;
}

int ddsi_fakenet_load_real_interfaces (void)
{
  ddsrt_ifaddrs_t *ifs = NULL;
  if (ddsi_eth_enumerate_interfaces (NULL, DDSI_TRANS_UDP, &ifs) < 0)
    return -1;

  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  clear_locked ();

  struct fake_host *host = NULL;
  int ret = -1;
  if (append_host_locked (&host) < 0 || (host->name = ddsrt_strdup ("real")) == NULL)
    goto out;

  uint32_t fallback_index = 1;
  for (const ddsrt_ifaddrs_t *ifa = ifs; ifa != NULL; ifa = ifa->next, fallback_index++)
  {
    const int ares = append_real_interface (host, ifa, fallback_index);
    if (ares < 0)
      goto out;
  }
  if (host->n_interfaces == 0)
    goto out;

  fakenet_current_host = host;
  if (set_topology_locked (FAKE_TOPOLOGY_REAL, NULL) < 0)
    goto out;
  ret = 0;

out:
  if (ret < 0)
    clear_locked ();
  ddsrt_mutex_unlock (&fakenet_lock);
  ddsrt_freeifaddrs (ifs);
  return ret;
}

int ddsi_fakenet_ensure_xml_file (const char *path)
{
  if (path == NULL)
    return -1;
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  const bool loaded = topology_is_loaded_locked (FAKE_TOPOLOGY_XML_FILE, path);
  ddsrt_mutex_unlock (&fakenet_lock);
  return loaded ? 0 : ddsi_fakenet_load_xml_file (path);
}

int ddsi_fakenet_ensure_default (void)
{
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  const bool loaded = topology_is_loaded_locked (FAKE_TOPOLOGY_DEFAULT, NULL);
  ddsrt_mutex_unlock (&fakenet_lock);
  return loaded ? 0 : ddsi_fakenet_load_default ();
}

int ddsi_fakenet_ensure_real_interfaces (void)
{
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  const bool loaded = topology_is_loaded_locked (FAKE_TOPOLOGY_REAL, NULL);
  ddsrt_mutex_unlock (&fakenet_lock);
  return loaded ? 0 : ddsi_fakenet_load_real_interfaces ();
}

void ddsi_fakenet_clear (void)
{
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  clear_locked ();
  ddsrt_mutex_unlock (&fakenet_lock);
}

int ddsi_fakenet_set_host (const char *name)
{
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  const struct fake_host *host = find_host_locked (name);
  fakenet_current_host = host;
  ddsrt_mutex_unlock (&fakenet_lock);
  return host ? 0 : -1;
}

int ddsi_fakenet_enumerate_interfaces (struct ddsi_tran_factory *fact, enum ddsi_transport_selector transport_selector, ddsrt_ifaddrs_t **ifs)
{
  (void) fact;
  if (transport_selector != DDSI_TRANS_FAKEUDP)
    return -1;
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  if (fakenet_current_host == NULL)
  {
    ddsrt_mutex_unlock (&fakenet_lock);
    return -1;
  }
  ddsrt_ifaddrs_t *head = NULL;
  ddsrt_ifaddrs_t **tail = &head;
  for (size_t i = 0; i < fakenet_current_host->n_interfaces; i++)
  {
    const struct fake_interface *src = &fakenet_current_host->interfaces[i];
    ddsrt_ifaddrs_t *ifa = ddsrt_malloc (sizeof (*ifa));
    if (ifa == NULL)
      goto fail;
    memset (ifa, 0, sizeof (*ifa));
    ifa->name = ddsrt_strdup (src->name);
    ifa->index = src->index;
    ifa->flags = src->flags;
    ifa->type = src->type;
    ifa->addr = ddsrt_memdup (&src->addr, fakenet_sockaddr_size ((const struct sockaddr *) &src->addr));
    if (src->has_netmask)
      ifa->netmask = ddsrt_memdup (&src->netmask, fakenet_sockaddr_size ((const struct sockaddr *) &src->netmask));
    if (ifa->name == NULL || ifa->addr == NULL || (src->has_netmask && ifa->netmask == NULL))
    {
      ddsrt_freeifaddrs (ifa);
      goto fail;
    }
    *tail = ifa;
    tail = &ifa->next;
  }
  *ifs = head;
  ddsrt_mutex_unlock (&fakenet_lock);
  return 0;

fail:
  ddsrt_freeifaddrs (head);
  ddsrt_mutex_unlock (&fakenet_lock);
  return -1;
}

dds_return_t ddsi_fakenet_socket (ddsrt_socket_t *sockptr, int domain, int type, int protocol)
{
  if (domain != AF_INET || type != SOCK_DGRAM)
    return DDS_RETCODE_BAD_PARAMETER;
  fakenet_lock_init ();
  ddsrt_socket_t socks[2] = { DDSRT_INVALID_SOCKET, DDSRT_INVALID_SOCKET };
  if (make_wake_pair (socks) < 0)
    return DDS_RETCODE_ERROR;

  struct fake_socket *s = ddsrt_malloc (sizeof (*s));
  if (s == NULL)
  {
    close_wake_handle (socks[0]);
    close_wake_handle (socks[1]);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  memset (s, 0, sizeof (*s));
  s->rd = socks[0];
  s->wr = socks[1];
  s->domain = domain;
  s->type = type;
  s->protocol = protocol;
  s->multicast_loopback = true;

  ddsrt_mutex_lock (&fakenet_lock);
  if (fakenet_current_host == NULL)
  {
    ddsrt_mutex_unlock (&fakenet_lock);
    free_socket_locked (s);
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  }
  s->host = fakenet_current_host;
  struct fake_socket **new_sockets = ddsrt_realloc (fakenet_sockets, (fakenet_nsockets + 1) * sizeof (*fakenet_sockets));
  if (new_sockets == NULL)
  {
    ddsrt_mutex_unlock (&fakenet_lock);
    free_socket_locked (s);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  fakenet_sockets = new_sockets;
  fakenet_sockets[fakenet_nsockets++] = s;
  *sockptr = s->rd;
  ddsrt_mutex_unlock (&fakenet_lock);
  return DDS_RETCODE_OK;
}

static uint16_t alloc_ephemeral_port_locked (const struct fake_socket *s)
{
  for (uint32_t tries = 0; tries < 16384; tries++)
  {
    const uint16_t port = fakenet_ephemeral_port++;
    if (fakenet_ephemeral_port == 0)
      fakenet_ephemeral_port = 49152;
    bool in_use = false;
    for (size_t i = 0; i < fakenet_nsockets && !in_use; i++)
      if (fakenet_sockets[i] && fakenet_sockets[i] != s && fakenet_sockets[i]->bound &&
          fakenet_sockets[i]->host == s->host &&
          sockaddr_port ((const struct sockaddr *) &fakenet_sockets[i]->addr) == port)
        in_use = true;
    if (!in_use)
      return port;
  }
  return 0;
}

dds_return_t ddsi_fakenet_bind (ddsrt_socket_t sock, const struct sockaddr *addr, socklen_t addrlen)
{
  (void) addrlen;
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  struct fake_socket *s = lookup_socket_locked (sock);
  if (s == NULL || addr == NULL || addr->sa_family != AF_INET)
  {
    ddsrt_mutex_unlock (&fakenet_lock);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  if (!host_owns_addr (s->host, addr))
  {
    ddsrt_mutex_unlock (&fakenet_lock);
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  }
  struct sockaddr_storage bindaddr;
  memset (&bindaddr, 0, sizeof (bindaddr));
  memcpy (&bindaddr, addr, fakenet_sockaddr_size (addr));
  uint16_t port = sockaddr_port ((const struct sockaddr *) &bindaddr);
  if (port == 0)
  {
    port = alloc_ephemeral_port_locked (s);
    if (port == 0)
    {
      ddsrt_mutex_unlock (&fakenet_lock);
      return DDS_RETCODE_OUT_OF_RESOURCES;
    }
    sockaddr_set_port ((struct sockaddr *) &bindaddr, port);
  }

  for (size_t i = 0; i < fakenet_nsockets; i++)
  {
    const struct fake_socket *o = fakenet_sockets[i];
    if (o == NULL || o == s || !o->bound || o->host != s->host)
      continue;
    if (sockaddr_endpoint_overlaps ((const struct sockaddr *) &o->addr, (const struct sockaddr *) &bindaddr) &&
        !(o->reuse && s->reuse))
    {
      ddsrt_mutex_unlock (&fakenet_lock);
      return DDS_RETCODE_PRECONDITION_NOT_MET;
    }
  }

  s->addr = bindaddr;
  s->bound = true;
  ddsrt_mutex_unlock (&fakenet_lock);
  return DDS_RETCODE_OK;
}

dds_return_t ddsi_fakenet_getsockname (ddsrt_socket_t sock, struct sockaddr *addr, socklen_t *addrlen)
{
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  struct fake_socket *s = lookup_socket_locked (sock);
  if (s == NULL || addr == NULL || addrlen == NULL)
  {
    ddsrt_mutex_unlock (&fakenet_lock);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  const socklen_t sz = s->bound ? fakenet_sockaddr_size ((const struct sockaddr *) &s->addr) : (socklen_t) sizeof (struct sockaddr_in);
  if (*addrlen >= sz)
    memcpy (addr, s->bound ? (const void *) &s->addr : &(struct sockaddr_in){ .sin_family = AF_INET }, sz);
  *addrlen = sz;
  ddsrt_mutex_unlock (&fakenet_lock);
  return DDS_RETCODE_OK;
}

static int copy_iov_to_packet (struct fake_packet *p, const ddsrt_msghdr_t *msg)
{
  size_t off = 0;
  for (size_t i = 0; i < (size_t) msg->msg_iovlen; i++)
  {
    const size_t n = msg->msg_iov[i].iov_len;
    memcpy (p->data + off, msg->msg_iov[i].iov_base, n);
    off += n;
  }
  return 0;
}

static size_t msg_iov_size (const ddsrt_msghdr_t *msg)
{
  size_t n = 0;
  for (size_t i = 0; i < (size_t) msg->msg_iovlen; i++)
    n += msg->msg_iov[i].iov_len;
  return n;
}

static void copy_packet_to_iov (const struct fake_packet *p, ddsrt_msghdr_t *msg, size_t *copied)
{
  size_t off = 0;
  for (size_t i = 0; i < (size_t) msg->msg_iovlen && off < p->size; i++)
  {
    const size_t avail = p->size - off;
    const size_t n = msg->msg_iov[i].iov_len < avail ? msg->msg_iov[i].iov_len : avail;
    memcpy (msg->msg_iov[i].iov_base, p->data + off, n);
    off += n;
  }
  *copied = off;
}

static void set_recvmsg_control (const struct fake_socket *s, const struct fake_packet *p, ddsrt_msghdr_t *msg)
{
  (void) s;
  (void) p;
  if (msg->msg_control == NULL)
    return;
  const size_t controllen = msg->msg_controllen;
  msg->msg_controllen = 0;
#if defined (_WIN32) && !defined (CMSG_DATA) && defined (WSA_CMSG_DATA)
#define CMSG_DATA WSA_CMSG_DATA
#endif
#if defined (IP_PKTINFO) && defined (CMSG_SPACE) && defined (CMSG_LEN) && defined (CMSG_FIRSTHDR) && defined (CMSG_DATA)
  if (!s->pktinfo_enabled || controllen < CMSG_SPACE (sizeof (struct in_pktinfo)) ||
      ((const struct sockaddr *) &p->dst)->sa_family != AF_INET)
    return;
  memset (msg->msg_control, 0, CMSG_SPACE (sizeof (struct in_pktinfo)));
  msg->msg_controllen = CMSG_SPACE (sizeof (struct in_pktinfo));
#ifndef _WIN32
  ddsrt_msghdr_t *cmsg_msg = msg;
#else
  WSAMSG cmsg_storage = { .Control = { .len = (ULONG) msg->msg_controllen, .buf = (CHAR *) msg->msg_control } };
  WSAMSG *cmsg_msg = &cmsg_storage;
#endif
  struct cmsghdr *cmsg = CMSG_FIRSTHDR (cmsg_msg);
  if (cmsg == NULL)
  {
    msg->msg_controllen = 0;
    return;
  }
  cmsg->cmsg_len = CMSG_LEN (sizeof (struct in_pktinfo));
  cmsg->cmsg_level = IPPROTO_IP;
  cmsg->cmsg_type = IP_PKTINFO;
  struct in_pktinfo *pktinfo = (struct in_pktinfo *) CMSG_DATA (cmsg);
  memset (pktinfo, 0, sizeof (*pktinfo));
#ifdef __APPLE__
  pktinfo->ipi_ifindex = (unsigned int) p->if_index;
#else
  pktinfo->ipi_ifindex = (int) p->if_index;
#endif
  pktinfo->ipi_addr = ((const struct sockaddr_in *) &p->dst)->sin_addr;
#endif
}

static int enqueue_packet_locked (struct fake_socket *dstsock, const struct sockaddr_storage *src, const struct sockaddr_storage *dst, uint32_t if_index, const ddsrt_msghdr_t *msg)
{
  const size_t size = msg_iov_size (msg);
  struct fake_packet *p = ddsrt_malloc (sizeof (*p) + size);
  if (p == NULL)
    return -1;
  memset (p, 0, sizeof (*p));
  p->src = *src;
  p->dst = *dst;
  p->if_index = if_index;
  p->size = size;
  copy_iov_to_packet (p, msg);
  const bool was_empty = (dstsock->queue_head == NULL);
  if (dstsock->queue_tail)
    dstsock->queue_tail->next = p;
  else
    dstsock->queue_head = p;
  dstsock->queue_tail = p;
  if (was_empty)
    signal_wake_handle (dstsock->wr);
  return 0;
}

static bool socket_matches_unicast_destination (const struct fake_socket *s, const struct sockaddr *dst)
{
  return s->bound &&
         s->domain == dst->sa_family &&
         sockaddr_port ((const struct sockaddr *) &s->addr) == sockaddr_port (dst) &&
         host_owns_addr (s->host, dst) &&
         (sockaddr_is_any ((const struct sockaddr *) &s->addr) || sockaddr_addr_eq ((const struct sockaddr *) &s->addr, dst));
}

static const struct fake_interface *membership_interface (const struct fake_socket *s, const struct fake_membership *m)
{
  const struct fake_interface *intf = host_find_interface_by_index (s->host, m->if_index);
  if (intf == NULL && m->interf.s_addr != htonl (INADDR_ANY))
    intf = host_find_interface_by_in_addr (s->host, m->interf);
  return intf ? intf : host_first_interface (s->host);
}

static bool socket_has_membership (const struct fake_socket *s, const struct sockaddr *dst, const struct fake_interface *send_intf, uint32_t *if_index)
{
  const struct sockaddr_in *dst4 = (const struct sockaddr_in *) dst;
  for (const struct fake_membership *m = s->memberships; m; m = m->next)
  {
    if (m->group.s_addr == dst4->sin_addr.s_addr)
    {
      const struct fake_interface *recv_intf = membership_interface (s, m);
      if (interfaces_share_link (send_intf, recv_intf))
      {
        *if_index = recv_intf ? recv_intf->index : 0;
        return true;
      }
    }
  }
  return false;
}

dds_return_t ddsi_fakenet_sendmsg (ddsrt_socket_t sock, const ddsrt_msghdr_t *msg, int flags, size_t *sent)
{
  (void) flags;
  if (msg == NULL || msg->msg_name == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  const struct sockaddr *dst = msg->msg_name;
  if (dst->sa_family != AF_INET)
    return DDS_RETCODE_BAD_PARAMETER;

  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  struct fake_socket *s = lookup_socket_locked (sock);
  if (s == NULL)
  {
    ddsrt_mutex_unlock (&fakenet_lock);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  struct sockaddr_storage src;
  struct sockaddr_storage dststore;
  memset (&dststore, 0, sizeof (dststore));
  memcpy (&dststore, dst, fakenet_sockaddr_size (dst));
  select_source_address (s, dst, &src);
  const struct fake_interface *send_intf = sockaddr_is_multicast (dst) ? socket_multicast_interface (s) : NULL;

  for (size_t i = 0; i < fakenet_nsockets; i++)
  {
    struct fake_socket *o = fakenet_sockets[i];
    if (o == NULL || !o->bound)
      continue;
    if (sockaddr_is_multicast (dst))
    {
      uint32_t if_index = 0;
      if ((o != s || s->multicast_loopback) &&
          sockaddr_port ((const struct sockaddr *) &o->addr) == sockaddr_port (dst) &&
          socket_has_membership (o, dst, send_intf, &if_index))
        (void) enqueue_packet_locked (o, &src, &dststore, if_index, msg);
    }
    else if (socket_matches_unicast_destination (o, dst))
    {
      const struct fake_interface *intf = host_find_interface_by_addr (o->host, dst);
      (void) enqueue_packet_locked (o, &src, &dststore, intf ? intf->index : 0, msg);
    }
  }
  if (sent)
    *sent = msg_iov_size (msg);
  ddsrt_mutex_unlock (&fakenet_lock);
  return DDS_RETCODE_OK;
}

dds_return_t ddsi_fakenet_recvmsg (const ddsrt_socket_ext_t *sockext, ddsrt_msghdr_t *msg, int flags, size_t *rcvd)
{
  (void) flags;
  if (sockext == NULL || msg == NULL || rcvd == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  struct fake_socket *s = lookup_socket_locked (sockext->sock);
  if (s == NULL)
  {
    ddsrt_mutex_unlock (&fakenet_lock);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  struct fake_packet *p = s->queue_head;
  if (p == NULL)
  {
    ddsrt_mutex_unlock (&fakenet_lock);
    return DDS_RETCODE_TRY_AGAIN;
  }
  s->queue_head = p->next;
  if (s->queue_head == NULL)
    s->queue_tail = NULL;

  if (msg->msg_name && msg->msg_namelen >= fakenet_sockaddr_size ((const struct sockaddr *) &p->src))
    memcpy (msg->msg_name, &p->src, fakenet_sockaddr_size ((const struct sockaddr *) &p->src));
  if (msg->msg_name)
    msg->msg_namelen = fakenet_sockaddr_size ((const struct sockaddr *) &p->src);
  copy_packet_to_iov (p, msg, rcvd);
  set_recvmsg_control (s, p, msg);
#if DDSRT_MSGHDR_FLAGS
#ifdef MSG_TRUNC
  msg->msg_flags = (*rcvd < p->size) ? MSG_TRUNC : 0;
#else
  msg->msg_flags = 0;
#endif
#endif
  ddsrt_free (p);

  if (s->queue_head == NULL)
    drain_wake_handle (s->rd);
  ddsrt_mutex_unlock (&fakenet_lock);
  return DDS_RETCODE_OK;
}

dds_return_t ddsi_fakenet_getsockopt (ddsrt_socket_t sock, int32_t level, int32_t optname, void *optval, socklen_t *optlen)
{
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  const struct fake_socket *s = lookup_socket_locked (sock);
  ddsrt_mutex_unlock (&fakenet_lock);
  if (s == NULL || optval == NULL || optlen == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  if (level == SOL_SOCKET && (optname == SO_RCVBUF || optname == SO_SNDBUF))
  {
    if (*optlen < sizeof (uint32_t))
      return DDS_RETCODE_BAD_PARAMETER;
    *((uint32_t *) optval) = 1048576;
    *optlen = sizeof (uint32_t);
    return DDS_RETCODE_OK;
  }
  return DDS_RETCODE_UNSUPPORTED;
}

static void add_membership (struct fake_socket *s, const struct ip_mreq *mreq)
{
  struct fake_membership *m = ddsrt_malloc (sizeof (*m));
  if (m == NULL)
    return;
  memset (m, 0, sizeof (*m));
  m->group = mreq->imr_multiaddr;
  m->interf = mreq->imr_interface;
  if (m->interf.s_addr != htonl (INADDR_ANY))
  {
    struct sockaddr_in sa;
    memset (&sa, 0, sizeof (sa));
    sa.sin_family = AF_INET;
    sa.sin_addr = m->interf;
    const struct fake_interface *intf = host_find_interface_by_addr (s->host, (const struct sockaddr *) &sa);
    if (intf)
      m->if_index = intf->index;
  }
  m->next = s->memberships;
  s->memberships = m;
}

static void add_membership_addr_ifindex (struct fake_socket *s, struct in_addr group, struct in_addr interf, uint32_t if_index)
{
  struct ip_mreq mreq;
  memset (&mreq, 0, sizeof (mreq));
  mreq.imr_multiaddr = group;
  mreq.imr_interface = interf;
  add_membership (s, &mreq);
  if (s->memberships && if_index != 0)
    s->memberships->if_index = if_index;
}

static void drop_membership (struct fake_socket *s, const struct ip_mreq *mreq)
{
  struct fake_membership **pm = &s->memberships;
  while (*pm)
  {
    struct fake_membership *m = *pm;
    if (m->group.s_addr == mreq->imr_multiaddr.s_addr && m->interf.s_addr == mreq->imr_interface.s_addr)
    {
      *pm = m->next;
      ddsrt_free (m);
      return;
    }
    pm = &m->next;
  }
}

static void drop_membership_addr_ifindex (struct fake_socket *s, struct in_addr group, struct in_addr interf, uint32_t if_index)
{
  struct fake_membership **pm = &s->memberships;
  while (*pm)
  {
    struct fake_membership *m = *pm;
    if (m->group.s_addr == group.s_addr &&
        (m->interf.s_addr == interf.s_addr || (if_index != 0 && m->if_index == if_index)))
    {
      *pm = m->next;
      ddsrt_free (m);
      return;
    }
    pm = &m->next;
  }
}

static void set_multicast_if (struct fake_socket *s, const void *optval, socklen_t optlen)
{
  s->multicast_if_valid = false;
  s->multicast_if.s_addr = htonl (INADDR_ANY);
  s->multicast_if_index = 0;
#if DDSI_FAKENET_HAVE_IP_MREQN
  if (optlen >= (socklen_t) sizeof (struct ip_mreqn))
  {
    const struct ip_mreqn *mreqn = optval;
    s->multicast_if = mreqn->imr_address;
    s->multicast_if_index = (uint32_t) mreqn->imr_ifindex;
    if (s->multicast_if_index == 0 && s->multicast_if.s_addr != htonl (INADDR_ANY))
    {
      const struct fake_interface *intf = host_find_interface_by_in_addr (s->host, s->multicast_if);
      if (intf)
        s->multicast_if_index = intf->index;
    }
    if (s->multicast_if.s_addr == htonl (INADDR_ANY) && s->multicast_if_index != 0)
    {
      const struct fake_interface *intf = host_find_interface_by_index (s->host, s->multicast_if_index);
      if (intf)
        s->multicast_if = ((const struct sockaddr_in *) &intf->addr)->sin_addr;
    }
    s->multicast_if_valid = s->multicast_if_index != 0 || s->multicast_if.s_addr != htonl (INADDR_ANY);
    return;
  }
#endif
  if (optlen >= (socklen_t) sizeof (struct in_addr))
  {
    s->multicast_if = *((const struct in_addr *) optval);
    if (s->multicast_if.s_addr != htonl (INADDR_ANY))
    {
      const struct fake_interface *intf = host_find_interface_by_in_addr (s->host, s->multicast_if);
      if (intf)
        s->multicast_if_index = intf->index;
      s->multicast_if_valid = true;
    }
  }
}

dds_return_t ddsi_fakenet_setsockopt (ddsrt_socket_t sock, int32_t level, int32_t optname, const void *optval, socklen_t optlen)
{
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  struct fake_socket *s = lookup_socket_locked (sock);
  if (s == NULL)
  {
    ddsrt_mutex_unlock (&fakenet_lock);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  if (level == SOL_SOCKET || level == IPPROTO_IP)
  {
    if (level == IPPROTO_IP && optname == IP_MULTICAST_LOOP && optval && optlen > 0)
      s->multicast_loopback = *((const unsigned char *) optval) != 0;
    else if (level == IPPROTO_IP && optname == IP_MULTICAST_IF && optval)
      set_multicast_if (s, optval, optlen);
#ifdef IP_PKTINFO
    else if (level == IPPROTO_IP && optname == IP_PKTINFO && optval && optlen >= (socklen_t) sizeof (int))
      s->pktinfo_enabled = *((const int *) optval) != 0;
#endif
#ifdef IP_ADD_MEMBERSHIP
    else if (level == IPPROTO_IP && optname == IP_ADD_MEMBERSHIP && optval && optlen >= (socklen_t) sizeof (struct ip_mreq))
      add_membership (s, optval);
#endif
#ifdef IP_DROP_MEMBERSHIP
    else if (level == IPPROTO_IP && optname == IP_DROP_MEMBERSHIP && optval && optlen >= (socklen_t) sizeof (struct ip_mreq))
      drop_membership (s, optval);
#endif
#if defined (DDSRT_HAVE_SSM) && defined (IP_ADD_SOURCE_MEMBERSHIP)
    else if (level == IPPROTO_IP && optname == IP_ADD_SOURCE_MEMBERSHIP && optval && optlen >= (socklen_t) sizeof (struct ip_mreq_source))
    {
      const struct ip_mreq_source *mreq = optval;
      add_membership_addr_ifindex (s, mreq->imr_multiaddr, mreq->imr_interface, 0);
    }
#endif
#if defined (DDSRT_HAVE_SSM) && defined (IP_DROP_SOURCE_MEMBERSHIP)
    else if (level == IPPROTO_IP && optname == IP_DROP_SOURCE_MEMBERSHIP && optval && optlen >= (socklen_t) sizeof (struct ip_mreq_source))
    {
      const struct ip_mreq_source *mreq = optval;
      drop_membership_addr_ifindex (s, mreq->imr_multiaddr, mreq->imr_interface, 0);
    }
#endif
#if defined (DDSRT_HAVE_SSM) && defined (MCAST_JOIN_SOURCE_GROUP)
    else if (level == IPPROTO_IP && optname == MCAST_JOIN_SOURCE_GROUP && optval && optlen >= (socklen_t) sizeof (struct group_source_req))
    {
      const struct group_source_req *gsr = optval;
      if (((const struct sockaddr *) &gsr->gsr_group)->sa_family == AF_INET)
      {
        const struct sockaddr_in *group = (const struct sockaddr_in *) &gsr->gsr_group;
        const struct fake_interface *intf = host_find_interface_by_index (s->host, (uint32_t) gsr->gsr_interface);
        add_membership_addr_ifindex (s, group->sin_addr, intf ? ((const struct sockaddr_in *) &intf->addr)->sin_addr : (struct in_addr){ .s_addr = htonl (INADDR_ANY) }, (uint32_t) gsr->gsr_interface);
      }
    }
#endif
#if defined (DDSRT_HAVE_SSM) && defined (MCAST_LEAVE_SOURCE_GROUP)
    else if (level == IPPROTO_IP && optname == MCAST_LEAVE_SOURCE_GROUP && optval && optlen >= (socklen_t) sizeof (struct group_source_req))
    {
      const struct group_source_req *gsr = optval;
      if (((const struct sockaddr *) &gsr->gsr_group)->sa_family == AF_INET)
      {
        const struct sockaddr_in *group = (const struct sockaddr_in *) &gsr->gsr_group;
        const struct fake_interface *intf = host_find_interface_by_index (s->host, (uint32_t) gsr->gsr_interface);
        drop_membership_addr_ifindex (s, group->sin_addr, intf ? ((const struct sockaddr_in *) &intf->addr)->sin_addr : (struct in_addr){ .s_addr = htonl (INADDR_ANY) }, (uint32_t) gsr->gsr_interface);
      }
    }
#endif
    ddsrt_mutex_unlock (&fakenet_lock);
    return DDS_RETCODE_OK;
  }
  ddsrt_mutex_unlock (&fakenet_lock);
  return DDS_RETCODE_UNSUPPORTED;
}

dds_return_t ddsi_fakenet_setsockreuse (ddsrt_socket_t sock, bool reuse)
{
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  struct fake_socket *s = lookup_socket_locked (sock);
  if (s == NULL)
  {
    ddsrt_mutex_unlock (&fakenet_lock);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  s->reuse = reuse;
  ddsrt_mutex_unlock (&fakenet_lock);
  return DDS_RETCODE_OK;
}

dds_return_t ddsi_fakenet_close (ddsrt_socket_t sock)
{
  fakenet_lock_init ();
  ddsrt_mutex_lock (&fakenet_lock);
  for (size_t i = 0; i < fakenet_nsockets; i++)
  {
    if (fakenet_sockets[i] && fakenet_sockets[i]->rd == sock)
    {
      struct fake_socket *s = fakenet_sockets[i];
      fakenet_sockets[i] = fakenet_sockets[--fakenet_nsockets];
      free_socket_locked (s);
      ddsrt_mutex_unlock (&fakenet_lock);
      return DDS_RETCODE_OK;
    }
  }
  ddsrt_mutex_unlock (&fakenet_lock);
  return DDS_RETCODE_BAD_PARAMETER;
}
