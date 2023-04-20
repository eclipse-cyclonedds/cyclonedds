// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#define _GNU_SOURCE

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "sockets_priv.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"

#if !LWIP_SOCKET
# if !defined(_WIN32)
#   include <arpa/inet.h>
#   include <netdb.h>
#   include <sys/socket.h>
#   if defined(__linux)
#     include <linux/if_packet.h> /* sockaddr_ll */
#   endif /* __linux */
# endif /* _WIN32 */
#endif /* LWIP_SOCKET */

#if defined __APPLE__
#include <net/if_dl.h>
#endif

extern inline struct timeval *
ddsrt_duration_to_timeval_ceil(dds_duration_t reltime, struct timeval *tv);

#if DDSRT_HAVE_IPV6
const struct in6_addr ddsrt_in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr ddsrt_in6addr_loopback = IN6ADDR_LOOPBACK_INIT;
#endif

const int afs[] = {
#if defined(__linux) && !LWIP_SOCKET
  AF_PACKET,
#endif /* __linux */
#if defined(__APPLE__)
  AF_LINK,
#endif
#if DDSRT_HAVE_IPV6
  AF_INET6,
#endif /* DDSRT_HAVE_IPV6 */
  AF_INET,
  DDSRT_AF_TERM /* Terminator */
};

const int *const os_supp_afs = afs;

socklen_t
ddsrt_sockaddr_get_size(const struct sockaddr *const sa)
{
  socklen_t sz;

  assert(sa != NULL);

  switch(sa->sa_family) {
#if DDSRT_HAVE_IPV6
    case AF_INET6:
      sz = sizeof(struct sockaddr_in6);
      break;
#endif /* DDSRT_HAVE_IPV6 */
#if defined(__linux) && !LWIP_SOCKET
    case AF_PACKET:
      sz = sizeof(struct sockaddr_ll);
      break;
#elif defined __APPLE__
    case AF_LINK:
      sz = sizeof(struct sockaddr_dl);
      break;
#endif /* __linux */
    default:
      assert(sa->sa_family == AF_INET);
      sz = sizeof(struct sockaddr_in);
      break;
  }

  return sz;
}

uint16_t ddsrt_sockaddr_get_port(const struct sockaddr *const sa)
{
  unsigned short port = 0;

  switch(sa->sa_family) {
#if DDSRT_HAVE_IPV6
    case AF_INET6:
      port = ntohs(((struct sockaddr_in6 *)sa)->sin6_port);
      break;
#endif /* DDSRT_HAVE_IPV6 */
    default:
      assert(sa->sa_family == AF_INET);
      port = ntohs(((struct sockaddr_in *)sa)->sin_port);
      break;
  }

  return port;
}

bool
ddsrt_sockaddr_isunspecified(const struct sockaddr *__restrict sa)
{
  assert(sa != NULL);

  switch(sa->sa_family) {
#if DDSRT_HAVE_IPV6
    case AF_INET6:
      return IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6*)sa)->sin6_addr);
#endif
    case AF_INET:
      return (((struct sockaddr_in *)sa)->sin_addr.s_addr == 0);
  }

  return false;
}

bool
ddsrt_sockaddr_isloopback(const struct sockaddr *__restrict sa)
{
  assert(sa != NULL);

  switch (sa->sa_family) {
#if DDSRT_HAVE_IPV6
    case AF_INET6:
      return IN6_IS_ADDR_LOOPBACK(
        &((const struct sockaddr_in6 *)sa)->sin6_addr);
#endif /* DDSRT_HAVE_IPV6 */
    case AF_INET:
      return (((const struct sockaddr_in *)sa)->sin_addr.s_addr
                  == htonl(INADDR_LOOPBACK));
  }

  return false;
}

bool
ddsrt_sockaddr_insamesubnet(
  const struct sockaddr *sa1,
  const struct sockaddr *sa2,
  const struct sockaddr *mask)
{
  bool eq = false;

  if (sa1->sa_family != sa2->sa_family ||
      sa1->sa_family != mask->sa_family)
  {
    return false;
  }

  switch (sa1->sa_family) {
    case AF_INET: {
      eq = ((((struct sockaddr_in *)sa1)->sin_addr.s_addr &
             ((struct sockaddr_in *)mask)->sin_addr.s_addr)
                 ==
            (((struct sockaddr_in *)sa2)->sin_addr.s_addr &
             ((struct sockaddr_in *)mask)->sin_addr.s_addr));
      } break;
#if DDSRT_HAVE_IPV6
    case AF_INET6: {
      struct sockaddr_in6 *sin61, *sin62, *mask6;
      size_t i, n = sizeof(sin61->sin6_addr.s6_addr);
      sin61 = (struct sockaddr_in6 *)sa1;
      sin62 = (struct sockaddr_in6 *)sa2;
      mask6 = (struct sockaddr_in6 *)mask;
      eq = true;
      for (i = 0; eq && i < n; i++) {
        eq = ((sin61->sin6_addr.s6_addr[i] &
               mask6->sin6_addr.s6_addr[i])
                 ==
              (sin62->sin6_addr.s6_addr[i] &
               mask6->sin6_addr.s6_addr[i]));
        }
      } break;
#endif
  }

  return eq;
}

dds_return_t
ddsrt_sockaddrfromstr(int af, const char *str, void *sa)
{
  assert(str != NULL);
  assert(sa != NULL);

  switch (af) {
    case AF_INET: {
      struct in_addr buf;
#if DDSRT_HAVE_INET_PTON
      if (inet_pton(af, str, &buf) != 1) {
        return DDS_RETCODE_BAD_PARAMETER;
      }
#else
      buf.s_addr = inet_addr (str);
      if (buf.s_addr == (in_addr_t)-1) {
        return DDS_RETCODE_BAD_PARAMETER;
      }
#endif
      memset(sa, 0, sizeof(struct sockaddr_in));
      ((struct sockaddr_in *)sa)->sin_family = AF_INET;
      memcpy(&((struct sockaddr_in *)sa)->sin_addr, &buf, sizeof(buf));
    } break;
#if DDSRT_HAVE_IPV6
    case AF_INET6: {
      struct in6_addr buf;
      if (inet_pton(af, str, &buf) != 1) {
        return DDS_RETCODE_BAD_PARAMETER;
      } else {
        memset(sa, 0, sizeof(struct sockaddr_in6));
        ((struct sockaddr_in6 *)sa)->sin6_family = AF_INET6;
        memcpy(&((struct sockaddr_in6 *)sa)->sin6_addr, &buf, sizeof(buf));
      }
    } break;
#endif
    default:
      return DDS_RETCODE_BAD_PARAMETER;
  }

  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_sockaddrtostr(const void *sa, char *buf, size_t size)
{
  const char *ptr;

  assert(sa != NULL);
  assert(buf != NULL);

#if LWIP_SOCKET
DDSRT_WARNING_GNUC_OFF(sign-conversion)
#endif
  switch (((struct sockaddr *)sa)->sa_family) {
    case AF_INET:
#if DDSRT_HAVE_INET_NTOP
      ptr = inet_ntop(
        AF_INET, &((struct sockaddr_in *)sa)->sin_addr, buf, (uint32_t)size);
#else
      {
          in_addr_t x = ntohl(((struct sockaddr_in *)sa)->sin_addr.s_addr);
          snprintf(buf,size,"%u.%u.%u.%u",(x>>24),(x>>16)&0xff,(x>>8)&0xff,x&0xff);
          ptr = buf;
      }
#endif
      break;
#if DDSRT_HAVE_IPV6
    case AF_INET6:
      ptr = inet_ntop(
        AF_INET6, &((struct sockaddr_in6 *)sa)->sin6_addr, buf, (uint32_t)size);
      break;
#endif
    default:
      return DDS_RETCODE_BAD_PARAMETER;
  }
#if LWIP_SOCKET
DDSRT_WARNING_GNUC_ON(sign-conversion)
#endif

  if (ptr == NULL) {
    return DDS_RETCODE_NOT_ENOUGH_SPACE;
  }

  return DDS_RETCODE_OK;
}

#if DDSRT_HAVE_DNS
static bool
is_valid_hostname_char(char c)
{
  return
    (c >= 'a' && c <= 'z') ||
    (c >= 'A' && c <= 'Z') ||
    (c >= '0' && c <= '9') ||
    c == '-' ||
    c == '.' ||
    c == ':';
}

#if DDSRT_HAVE_GETADDRINFO
dds_return_t
ddsrt_gethostbyname(const char *name, int af, ddsrt_hostent_t **hentp)
{
  int gai_err = 0;
  struct addrinfo hints, *res = NULL;
  ddsrt_hostent_t *hent = NULL;

  assert(name != NULL);
  assert(hentp != NULL);

  switch (af) {
#if DDSRT_HAVE_IPV6
    case AF_INET6:
#endif
    case AF_INET:
    case AF_UNSPEC:
      break;
    default:
      return DDS_RETCODE_BAD_PARAMETER;
  }

  /* Windows returns all registered addresses on the local computer if the
     "nodename" parameter is an empty string. *NIX return HOST_NOT_FOUND.
     Deny empty hostnames to keep behavior across platforms consistent. */
  if (strlen(name) == 0) {
    return DDS_RETCODE_HOST_NOT_FOUND;
  }

  for (size_t i = 0; name[i]; i++) {
    if (!is_valid_hostname_char(name[i])) {
      return DDS_RETCODE_HOST_NOT_FOUND;
    }
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = af;

  gai_err = getaddrinfo(name, NULL, &hints, &res);
  /* gai_strerror cannot be used because Windows does not offer a thread-safe
     implementation and lwIP (there maybe others as well) does not offer an
     implementation at all.

     NOTE: Error codes returned by getaddrinfo map directly onto Windows
           Socket error codes and WSAGetLastError can be used instead. */
  switch (gai_err) {
#if defined(EAI_AGAIN)
    case EAI_AGAIN:
      /* Name server returned a temporary failure indication. */
      return DDS_RETCODE_TRY_AGAIN;
#endif
    case EAI_FAIL:
      /* Name server returned a permanent failure indication. */
      return DDS_RETCODE_ERROR;
/* Windows defines EAI_NODATA to EAI_NONAME. */
#if defined(EAI_NODATA) && (EAI_NODATA != EAI_NONAME)
    case EAI_NODATA:
      /* Host exists, but does not have any network addresses defined. */
      return DDS_RETCODE_NO_DATA;
#endif
#if defined(EAI_ADDRFAMILY)
    case EAI_ADDRFAMILY: /* Host has no addresses in requested family. */
#endif
#if defined(EAI_NOSECURENAME) /* Windows */
    case EAI_NOSECURENAME:
#endif
    case EAI_NONAME:
      /* Host does not exist. */
      return DDS_RETCODE_HOST_NOT_FOUND;
    case EAI_MEMORY:
      /* Out of memory. */
      return DDS_RETCODE_OUT_OF_RESOURCES;
#if defined(EAI_SYSTEM)
    case EAI_SYSTEM:
      /* Other system error. */
      return DDS_RETCODE_ERROR;
#endif
#if defined(EAI_BADFLAGS)
    case EAI_BADFLAGS: /* Invalid flags in hints.ai_flags. */
#endif
    case EAI_FAMILY: /* Address family not supported. */
    case EAI_SERVICE: /* Service not available for socket type. */
#if defined(EAI_SOCKTYPE)
    case EAI_SOCKTYPE: /* Socket type not supported. */
#endif
    case 0: {
      struct addrinfo *ai;
      size_t addrno, naddrs, size;

      assert(gai_err == 0);
      assert(res != NULL);

      naddrs = 0;
      for (ai = res; ai != NULL; ai = ai->ai_next) {
        naddrs++;
      }

      size = sizeof(*hent) + (naddrs * sizeof(hent->addrs[0]));
      if ((hent = ddsrt_calloc_s(1, size)) != NULL) {
           hent->naddrs = naddrs;
        for (addrno = 0, ai = res;
             addrno < naddrs && ai != NULL;
             addrno++, ai = ai->ai_next)
        {
          memcpy(&hent->addrs[addrno], res->ai_addr, res->ai_addrlen);
        }
      } else {
        return DDS_RETCODE_OUT_OF_RESOURCES;
      }

      freeaddrinfo(res);
    } break;
    default:
      DDS_ERROR ("getaddrinfo returned unknown error %d\n", gai_err);
      return DDS_RETCODE_ERROR;
  }

  *hentp = hent;
  return DDS_RETCODE_OK;
}
#elif DDSRT_HAVE_GETHOSTBYNAME_R
dds_return_t
ddsrt_gethostbyname(const char *name, int af, ddsrt_hostent_t **hentp)
{
  struct hostent hest, *he;
  char buf[256];
  int err;
  he = gethostbyname_r (name, &hest, buf, sizeof (buf), &err);
  if (he == NULL) {
    return DDS_RETCODE_HOST_NOT_FOUND;
  } else {
    size_t size = sizeof(**hentp) + (1 * sizeof((*hentp)->addrs[0]));
    *hentp = ddsrt_calloc_s(1, size);
    (*hentp)->naddrs = 1;
    memcpy(&(*hentp)->addrs[0], he->h_addr, he->h_length);
    return DDS_RETCODE_OK;
  }
}
#endif // DDSRT_HAVE_GETADDRINFO
#endif // DDSRT_HAVE_DNS

dds_return_t
ddsrt_setsockreuse(ddsrt_socket_t sock, bool reuse)
{
  int flags = reuse;
#ifdef SO_REUSEPORT
  const dds_return_t rc = ddsrt_setsockopt (sock, SOL_SOCKET, SO_REUSEPORT, &flags, sizeof (flags));
  switch (rc)
  {
    case DDS_RETCODE_UNSUPPORTED:
      // e.g. LWIP or Zephyr, which defines SO_REUSEPORT but doesn't implement it.
      //      note that we ignore this error because some systems use SO_REUSEADDR instead
    case DDS_RETCODE_OK:
      break;
    default:
      return rc;
  }
#endif
  return ddsrt_setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof (flags));
}
