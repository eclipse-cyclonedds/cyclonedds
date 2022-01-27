/*
 * Copyright(c) 2022 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#if _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# include <winsock2.h>
# include <ws2tcpip.h>
# include <iphlpapi.h>
# include <netioapi.h>
#elif __APPLE__
# include <sys/ioctl.h>
# include <sys/kern_event.h>
# include <netinet/in_var.h> // struct kev_in_data
# include <netinet6/in6_var.h> // struct kev_in6_data
# include <net/if_var.h> // struct net_event_data
#elif __FreeBSD__
# include <sys/socket.h>
# include <sys/param.h>
# include <net/if.h>
# include <net/if_var.h>
# include <net/route.h>
#elif __linux
# include <asm/types.h>
# include <sys/socket.h>
# include <linux/netlink.h>
# include <linux/rtnetlink.h>
#endif

#include "event.h"
#include "dds/ddsrt/static_assert.h"

#if _WIN32
static int dgram_pipe(SOCKET sv[2])
{
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  SOCKET fds[2] = { INVALID_SOCKET, INVALID_SOCKET };

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if ((fds[0] = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
    goto err_socket_fd0;
  if (bind(fds[0], (struct sockaddr *)&addr, addrlen) == SOCKET_ERROR)
    goto err_bind;
  if (getsockname(fds[0], (struct sockaddr *)&addr, &addrlen) == SOCKET_ERROR)
    goto err_bind;
  if ((fds[1] = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
    goto err_socket_fd1;
  if (connect(fds[1], (struct sockaddr *)&addr, addrlen) == -1)
    goto err_connect;
  // equivalent to FD_CLOEXEC
  SetHandleInformation((HANDLE) fds[0], HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation((HANDLE) fds[1], HANDLE_FLAG_INHERIT, 0);
  sv[0] = fds[0];
  sv[1] = fds[1];
  return 0;
err_connect:
  closesocket(fds[1]);
err_socket_fd1:
err_bind:
  closesocket(fds[0]);
err_socket_fd0:
  return -1;
}

static dds_return_t
destroy_netlink_event(ddsrt_event_t *event)
{
  if (event->source.netlink.address_handle)
    CancelMibChangeNotify2(event->source.netlink.address_handle);
  event->source.netlink.address_handle = NULL;
  if (event->source.netlink.interface_handle)
    CancelMibChangeNotify2(event->source.netlink.interface_handle);
  event->source.netlink.interface_handle = NULL;
  closesocket(event->source.netlink.pipefds[0]);
  closesocket(event->source.netlink.pipefds[1]);
  event->source.netlink.pipefds[0] = INVALID_SOCKET;
  event->source.netlink.pipefds[1] = INVALID_SOCKET;
  return DDS_RETCODE_OK;
}

// use same structure for every ipchange event for convenience
struct netlink_message {
  uint32_t flags;
  NET_LUID luid;
  NET_IFINDEX index;
  ADDRESS_FAMILY family;
  SOCKADDR_INET address; // zeroed out on NotifyIpInterfaceChange
};

static inline int
read_netlink_message(SOCKET fd, struct netlink_message *message)
{
  int cnt, off = 0, len = sizeof(*message);
  uint8_t *buf = (uint8_t *)message;

  do {
    cnt = recv(fd, buf + off, len - off, 0);
    if (cnt == SOCKET_ERROR && WSAGetLastError() == WSAEINTR)
      continue;
    if (cnt == SOCKET_ERROR)
      return -1;
    assert(cnt >= 0);
    off += cnt;
  } while (off < len);

  assert(off == len);
  return 0;
}

static inline int
write_netlink_message(SOCKET fd, const struct netlink_message *message)
{
  int cnt, off = 0, len = sizeof(*message);
  uint8_t *buf = (void *)message;

  do {
    cnt = send(fd, buf + (size_t)off, len, 0);
    if (cnt == SOCKET_ERROR && WSAGetLastError() == WSAEINTR)
      continue;
    if (cnt == SOCKET_ERROR)
      return -1;
    assert(cnt >= 0);
    off += cnt;
  } while (off < len);

  assert(off == len);
  return 0;
}

static void
do_address_change(
  void *caller_context,
  MIB_UNICASTIPADDRESS_ROW *row,
  MIB_NOTIFICATION_TYPE notification_type)
{
  struct netlink_message msg;

  assert(caller_context);

  if (!row) // initial notification, unused
    return;
  assert(notification_type != MibInitialNotification);
  if (notification_type == MibParameterNotification)
    return;

  if (row->Address.si_family == AF_INET6)
    msg.flags = notification_type == MibAddInstance
      ? DDSRT_IPV6_ADDED : DDSRT_IPV6_DELETED;
  else
    msg.flags = notification_type == MibAddInstance
      ? DDSRT_IPV4_ADDED : DDSRT_IPV4_DELETED;
  msg.luid = row->InterfaceLuid;
  msg.index = row->InterfaceIndex;
  msg.family = row->Address.si_family;
  msg.address = row->Address;
  write_netlink_message((SOCKET)caller_context, &msg);
}

static void
do_interface_change(
  void *caller_context,
  MIB_IPINTERFACE_ROW *row,
  MIB_NOTIFICATION_TYPE notification_type)
{
  struct netlink_message msg;

  assert(caller_context);

  if (!row) // initial notification, unused
    return;
  assert(notification_type != MibInitialNotification);
  if (notification_type == MibParameterNotification)
    return;

  msg.flags = notification_type == MibAddInstance
    ? DDSRT_LINK_UP : DDSRT_LINK_DOWN;
  msg.luid = row->InterfaceLuid;
  msg.index = row->InterfaceIndex;
  msg.family = row->Family;
  memset(&msg.address, 0, sizeof(msg.address));

  write_netlink_message((SOCKET)caller_context, &msg);
}

static dds_return_t
create_netlink_event(
  ddsrt_event_t *event,
  uint32_t flags,
  ddsrt_event_callback_t callback,
  void *user_data)
{
  SOCKET fds[2];
  HANDLE addr_hdl = NULL, iface_hdl = NULL;
  bool ip4 = (flags & (DDSRT_IPV4_ADDED|DDSRT_IPV4_DELETED));
  bool ip6 = (flags & (DDSRT_IPV6_ADDED|DDSRT_IPV6_DELETED));
  ADDRESS_FAMILY af = (ip4 && ip6) ? AF_UNSPEC : (ip6 ? AF_INET6 : AF_INET);

  (void)socket;

  // use a SOCK_DGRAM socket pair to deal with partial writes
  if (dgram_pipe(fds) == -1)
    goto err_pipe;
  // register callbacks to send notifications over socket pair
  if ((flags & (DDSRT_LINK_UP|DDSRT_LINK_DOWN)) &&
      NO_ERROR != NotifyIpInterfaceChange(
        AF_UNSPEC, &do_interface_change, (void*)fds[1], false, &iface_hdl))
    goto err_iface;
  if ((ip4 || ip6) &&
      NO_ERROR != NotifyUnicastIpAddressChange(
        af, &do_address_change, (void*)fds[1], false, &addr_hdl))
    goto err_addr;

  event->flags = DDSRT_NETLINK | (flags & (DDSRT_LINK_UP|DDSRT_LINK_DOWN|
                                           DDSRT_IPV4_ADDED|DDSRT_IPV4_DELETED|
                                           DDSRT_IPV6_ADDED|DDSRT_IPV6_DELETED));
  event->loop = NULL;
  event->callback = callback;
  event->user_data = user_data;
  event->source.netlink.pipefds[0] = fds[0];
  event->source.netlink.pipefds[1] = fds[1];
  event->source.netlink.address_handle = addr_hdl;
  event->source.netlink.interface_handle = iface_hdl;
  return DDS_RETCODE_OK;
err_addr:
  if (iface_hdl)
    CancelMibChangeNotify2(iface_hdl);
err_iface:
  closesocket(fds[0]);
  closesocket(fds[1]);
err_pipe:
  return DDS_RETCODE_OUT_OF_RESOURCES;
}

static dds_return_t
proxy_netlink_event(ddsrt_event_t *event, void *user_data)
{
  struct netlink_message msg = { 0 };
  ddsrt_netlink_message_t nlmsg = { 0 };

  DDSRT_STATIC_ASSERT(sizeof(struct sockaddr_in) == sizeof(msg.address.Ipv4));
  DDSRT_STATIC_ASSERT(sizeof(struct sockaddr_in6) == sizeof(msg.address.Ipv6));

  if (read_netlink_message(event->source.netlink.pipefds[0], &msg) != 0)
    abort(); // never happens, presumably
  // discard unwanted events
  if (!(msg.flags & event->flags))
    return DDS_RETCODE_OK;

  //nlmsg.interface.luid = msg.luid; // FIXME: required?
  nlmsg.index = msg.index;
  if (msg.flags & (DDSRT_LINK_UP|DDSRT_LINK_DOWN))
    return event->callback(event, msg.flags | DDSRT_NETLINK, &nlmsg, user_data);
  if (msg.flags & (DDSRT_IPV4_ADDED|DDSRT_IPV4_DELETED))
    memmove((struct sockaddr_in*)&nlmsg.address, &msg.address.Ipv4, sizeof(msg.address.Ipv4));
  else if (msg.flags & (DDSRT_IPV6_ADDED|DDSRT_IPV6_DELETED))
    memmove((struct sockaddr_in6*)&nlmsg.address, &msg.address.Ipv6, sizeof(msg.address.Ipv6));
  else
    assert(0);
  return event->callback(event, DDSRT_NETLINK | msg.flags, &nlmsg, user_data);
}

#elif __APPLE__ || __FreeBSD__ || __linux__
static dds_return_t
create_netlink_event(
  ddsrt_event_t *event,
  uint32_t flags,
  ddsrt_event_callback_t callback,
  void *user_data)
{
  int fd;

# if __APPLE__
  struct kev_request req;
  if ((fd = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT)) == -1)
    goto err_socket;
  req.vendor_code = KEV_VENDOR_APPLE;
  req.kev_class = KEV_NETWORK_CLASS;
  req.kev_subclass = KEV_ANY_SUBCLASS;
  if (ioctl(fd, SIOCSKEVFILT, &req) == -1)
    goto err_ioctl;
# elif __FreeBSD__
  if ((fd = socket(AF_ROUTE, SOCK_RAW, AF_UNSPEC)) == -1)
    goto err_socket;
# elif __linux__
  struct sockaddr_nl sa;
  if ((fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) == -1)
    goto err_socket;
  memset(&sa, 0, sizeof(sa));
  sa.nl_family = AF_NETLINK;
  if (flags & (DDSRT_LINK_UP|DDSRT_LINK_DOWN))
    sa.nl_groups |= RTMGRP_LINK;
  if (flags & (DDSRT_IPV4_ADDED|DDSRT_IPV4_DELETED))
    sa.nl_groups |= RTMGRP_IPV4_IFADDR;
  if (flags & (DDSRT_IPV6_ADDED|DDSRT_IPV6_DELETED))
    sa.nl_groups |= RTMGRP_IPV6_IFADDR;
  if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
    goto err_bind;
# endif

  event->flags = DDSRT_NETLINK | (flags & (DDSRT_LINK_UP|DDSRT_LINK_DOWN|
                                           DDSRT_IPV4_ADDED|DDSRT_IPV4_DELETED|
                                           DDSRT_IPV6_ADDED|DDSRT_IPV6_DELETED));
  event->loop = NULL;
  event->callback = callback;
  event->user_data = user_data;
  event->source.netlink.socketfd = fd;
  return DDS_RETCODE_OK;
# if __APPLE__
err_ioctl:
# elif __linux__
err_bind:
# endif
  close(fd);
err_socket:
  return DDS_RETCODE_OUT_OF_RESOURCES;
}

static dds_return_t
destroy_netlink_event(ddsrt_event_t *event)
{
  close(event->source.netlink.socketfd);
  event->source.netlink.socketfd = -1;
  return DDS_RETCODE_OK;
}

# if __APPLE__
// macOS offers a bunch of mechanisms for notification on address changes
//  1. System Configuration framework
//  2. IOKit
//  3. PF_ROUTE socket
//  4. PF_SYSTEM socket
//
// the System Configuration framework allows the user to create notification
// ports (not a mach_port_t), but a CFRunLoop is required and therefore seems
// primarily intented to be used in Cocoa applications. IOKit allows for
// creation of an IONotificationPortRef from which a mach_port_t can be
// retrieved and which can be monitored by kqueue with EVFILTER_MACH, but no
// notifications were received on IP address changes in tests. PF_ROUTE
// sockets are frequently used on BSD systems to monitor for changes to the
// routing database, but notifications were kind of a hit and miss in tests.
// PF_SYSTEM (1) sockets provide exactly what is required.
//
// 1: http://newosxbook.com/bonus/vol1ch16.html
static dds_return_t
proxy_netlink_event(ddsrt_event_t *event, void *user_data)
{
  char buf[1024];
  const struct kern_event_msg *msg = (const struct kern_event_msg *)buf;
  ssize_t msglen = 0;

  do {
    msglen = read(event->source.socket.socketfd, buf, sizeof(buf));
    assert(msglen != -1 || errno == EINTR);
  } while (msglen == -1);

  assert((size_t)msglen == (size_t)msg->total_size);
  // discard non-networking events
  if (msg->kev_class != KEV_NETWORK_CLASS)
    return DDS_RETCODE_OK;

  unsigned int flags = 0u;
  ddsrt_netlink_message_t nlmsg = { 0 };

  switch (msg->kev_subclass) {
    case KEV_INET_SUBCLASS: {
      struct kev_in_data *in_data = (struct kev_in_data *)msg->event_data;
      if (msg->event_code == KEV_INET_NEW_ADDR)
        flags = DDSRT_IPV4_ADDED;
      else if (msg->event_code == KEV_INET_ADDR_DELETED)
        flags = DDSRT_IPV4_DELETED;
      else
        break;
      //nlmsg.interface.unit = in_data->link_data.if_unit;
      nlmsg.index = in_data->link_data.if_unit;
      struct sockaddr_in *sin = (struct sockaddr_in *)&nlmsg.address;
      sin->sin_family = AF_INET;
      assert(sizeof(sin->sin_addr) == sizeof(in_data->ia_addr));
      memmove(&sin->sin_addr, &in_data->ia_addr, sizeof(sin->sin_addr));
    } break;
    case KEV_INET6_SUBCLASS: {
      struct kev_in6_data *in6_data = (struct kev_in6_data *)msg->event_data;
      if (msg->event_code == KEV_INET6_NEW_USER_ADDR)
        flags = DDSRT_IPV6_ADDED;
      else if (msg->event_code == KEV_INET6_ADDR_DELETED)
        flags = DDSRT_IPV6_DELETED;
      else
        break;
      //nlmsg.interface.unit = in6_data->link_data.if_unit;
      nlmsg.index = in6_data->link_data.if_unit;
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&nlmsg.address;
      sin6->sin6_family = AF_INET6;
      assert(sizeof(sin6->sin6_addr) == sizeof(in6_data->ia_addr));
      memmove(&sin6->sin6_addr, &in6_data->ia_addr, sizeof(sin6->sin6_addr));
    } break;
    case KEV_DL_SUBCLASS: {
      struct net_event_data *data = (struct net_event_data *)msg->event_data;
      if (msg->event_code == KEV_DL_PROTO_ATTACHED)
        flags = DDSRT_LINK_UP;
      else if (msg->event_code == KEV_DL_PROTO_DETACHED)
        flags = DDSRT_LINK_DOWN;
      else
        break;
      nlmsg.index = data->if_unit;
    } break;
    default:
      break;
  }

  // discard unwanted events
  if (!(event->flags & flags))
    return DDS_RETCODE_OK;

  return event->callback(event, DDSRT_NETLINK | flags, &nlmsg, user_data);
}

# elif __FreeBSD__
// https://www.freebsd.org/cgi/man.cgi?query=route&apropos=0&sektion=4&manpath=FreeBSD+1.1-RELEASE&arch=default&format=html
// also see UNIX Network Programming volume 1 chapter 18

/*
 * Round up 'a' to next multiple of 'size', which must be a power of 2
 */
#define ROUNDUP(a, size) (((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))

/*
 * Step to next socket address structure;
 * if sa_len is 0, assume it is sizeof(u_long).
 */
#define NEXT_SA(ap) ap = (struct sockaddr *) \
     ((caddr_t) ap + (ap->sa_len ? ROUNDUP(ap->sa_len, sizeof (u_long)) : \
                                        sizeof(u_long)))

static void get_rtaddrs(int addrs, const struct sockaddr *sa, const struct sockaddr **rti_info)
{
  for (int i = 0; i < RTAX_MAX; i++) {
    if (addrs & (1 << i)) {
      rti_info[i] = sa;
      NEXT_SA(sa);
    } else {
      rti_info[i] = NULL;
    }
  }
}

static dds_return_t
proxy_netlink_event(ddsrt_event_t *event, void *user_data)
{
  char buf[ sizeof(struct rt_msghdr) + 512 ];
  const struct rt_msghdr *rtm = (struct rt_msghdr *)buf;
  ssize_t rtmlen;

  do {
    rtmlen = read(event->source.socket.socketfd, buf, sizeof(buf));
    if (rtmlen == -1 && errno != EINTR)
      return DDS_RETCODE_ERROR;
  } while (rtmlen == -1);

  uint32_t flags = 0u;
  ddsrt_netlink_message_t nlmsg = { 0 };

  assert((size_t)rtmlen == (size_t)rtm->rtm_msglen);
  switch (rtm->rtm_type) {
    case RTM_NEWADDR:
    case RTM_DELADDR: {
      const struct ifa_msghdr *ifam = (void *)buf;
      const struct sockaddr *sa, *rti_info[RTAX_MAX];
      sa = (const struct sockaddr *)(ifam + 1);
      get_rtaddrs(ifam->ifam_addrs, sa, rti_info);
      sa = rti_info[RTAX_IFA];
      nlmsg.index = ifam->ifam_index;
      if (sa->sa_family == AF_INET) {
        flags = (rtm->rtm_type == RTM_NEWADDR) ? DDSRT_IPV4_ADDED : DDSRT_IPV4_DELETED;
        memmove(&nlmsg.address, sa, sizeof(struct sockaddr_in));
      } else {
        flags = (rtm->rtm_type == RTM_NEWADDR) ? DDSRT_IPV6_ADDED : DDSRT_IPV6_DELETED;
        memmove(&nlmsg.address, sa, sizeof(struct sockaddr_in6));
      }
    } break;
    case RTM_IFINFO: {
      const struct if_msghdr *ifm = (void *)buf;
      flags = (ifm->ifm_flags & IFF_UP) ? DDSRT_LINK_UP : DDSRT_LINK_DOWN;
    } break;
    default:
      break;
  }

  // discard unwanted events
  if (!(event->flags & flags))
    return DDS_RETCODE_OK;
  return event->callback(event, DDSRT_NETLINK | flags, &nlmsg, user_data);
}

# elif __linux
// inspired by get_rtaddrs and parse_rtaddrs
static void
get_rtattrs(
  struct rtattr *attrs,
  size_t len,
  struct rtattr *rta_info[],
  size_t max)
{
  memset(rta_info, 0, sizeof(*attrs) * max);
  for (struct rtattr *attr = attrs; RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
    assert(attr->rta_type <= max);
    rta_info[attr->rta_type] = attr;
  }
}

static dds_return_t
proxy_netlink_event(ddsrt_event_t *event, void *user_data)
{
  char buf[8192]; // opensplice uses 8k, seems a bit excessive?
  const struct nlmsghdr *nlm = (struct nlmsghdr *)buf;
  struct iovec iov = { buf, sizeof(buf) };
  struct sockaddr_nl sa;
  struct msghdr msg = { &sa, sizeof(sa), &iov, 1, NULL, 0, 0 };
  ssize_t nlmlen;

  do {
    nlmlen = recvmsg(event->source.netlink.socketfd, &msg, 0);
    if (nlmlen == -1 && errno != EINTR)
      return DDS_RETCODE_ERROR;
  } while (nlmlen == -1);

  for (; NLMSG_OK(nlm, nlmlen); nlm = NLMSG_NEXT(nlm, nlmlen)) {
    // end of multipart message
    if (nlm->nlmsg_type == NLMSG_DONE)
      break;

    unsigned int flags = 0u;
    ddsrt_netlink_message_t nlmsg = { 0 };

    switch (nlm->nlmsg_type) {
      case RTM_NEWADDR:
      case RTM_DELADDR: {
        const struct ifaddrmsg *ifa = NLMSG_DATA(nlm);
        struct rtattr *rta_info[IFA_MAX + 1];
        get_rtattrs(IFA_RTA(ifa), nlm->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa)), rta_info, IFA_MAX);
        const void *rta_data = RTA_DATA(rta_info[IFA_ADDRESS]);
        nlmsg.index = ifa->ifa_index;
        if (ifa->ifa_family == AF_INET) {
          struct sockaddr_in *sin = (struct sockaddr_in *)&nlmsg.address;
          flags = (nlm->nlmsg_type == RTM_NEWADDR) ? DDSRT_IPV4_ADDED : DDSRT_IPV4_DELETED;
          sin->sin_family = AF_INET;
          memmove(&sin->sin_addr, rta_data, sizeof(sin->sin_addr));
        } else {
          struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&nlmsg.address;
          flags = (nlm->nlmsg_type == RTM_NEWADDR) ? DDSRT_IPV6_ADDED : DDSRT_IPV6_DELETED;
          sin6->sin6_family = AF_INET6;
          memmove(&sin6->sin6_addr, rta_data, sizeof(sin6->sin6_addr));
        }
      } break;
      case RTM_NEWLINK:
      case RTM_DELLINK: {
        const struct ifinfomsg *ifi = NLMSG_DATA(nlm);
        flags = (ifi->ifi_flags & IFF_UP) ? DDSRT_LINK_UP : DDSRT_LINK_DOWN;
        nlmsg.index = (uint32_t)ifi->ifi_index;
      } break;
      default:
        break;
    }

    if (!(event->flags & flags))
      continue;

    dds_return_t ret;
    if ((ret = event->callback(event, DDSRT_NETLINK | flags, &nlmsg, user_data)))
      return ret;
  }

  return DDS_RETCODE_OK;
}
# endif
#endif

dds_return_t
ddsrt_handle_event(
  ddsrt_event_t *event,
  uint32_t flags,
  void *user_data)
{
#if DDSRT_HAVE_NETLINK_EVENT
  if (event->flags & DDSRT_NETLINK)
    return proxy_netlink_event(event, user_data);
#endif
  return event->callback(event, flags, NULL, user_data);
}

ddsrt_socket_t
ddsrt_event_socket(
  ddsrt_event_t *event)
{
#if DDSRT_HAVE_NETLINK_EVENT
# if _WIN32
  if (event->flags & DDSRT_NETLINK)
    return event->source.netlink.pipefds[0];
# else
  if (event->flags & DDSRT_NETLINK)
    return event->source.netlink.socketfd;
# endif
#endif
  return event->source.socket.socketfd;
}

dds_return_t
ddsrt_create_event(
  ddsrt_event_t *event,
  ddsrt_socket_t socket,
  uint32_t flags,
  ddsrt_event_callback_t callback,
  void *user_data)
{
  assert(event);
  assert(callback);

#if DDSRT_HAVE_NETLINK_EVENT
  if (flags & DDSRT_NETLINK)
    return create_netlink_event(event, flags, callback, user_data);
#endif

  assert(flags & (DDSRT_READ|DDSRT_WRITE));
  assert(socket != DDSRT_INVALID_SOCKET);

  event->flags = flags & (DDSRT_READ|DDSRT_WRITE);
  event->callback = callback;
  event->loop = NULL;
  event->user_data = user_data;
  event->source.socket.socketfd = socket;
  return DDS_RETCODE_OK;
}

dds_return_t
ddsrt_destroy_event(ddsrt_event_t *event)
{
  assert(event && !event->loop);
#if DDSRT_HAVE_NETLINK_EVENT
  if (event->flags & DDSRT_NETLINK)
    return destroy_netlink_event(event);
#endif

  event->source.socket.socketfd = DDSRT_INVALID_SOCKET;
  return DDS_RETCODE_OK;
}

void ddsrt_trigger_loop(ddsrt_loop_t *loop)
{
  char buf[1] = { '\0' };
  write_pipe(loop->pipefds[1], buf, sizeof(buf));
}
