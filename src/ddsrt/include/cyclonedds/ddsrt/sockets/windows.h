#ifndef DDSRT_WINDOWS_SOCKET_H
#define DDSRT_WINDOWS_SOCKET_H

#include <winsock2.h>
#include <ws2tcpip.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef SOCKET ddsrt_socket_t;
#define DDSRT_INVALID_SOCKET (INVALID_SOCKET)
#define PRIdSOCK PRIuPTR

#define DDSRT_HAVE_IPV6        1
#define DDSRT_HAVE_DNS         DDSRT_WITH_DNS
#define DDSRT_HAVE_GETADDRINFO DDSRT_WITH_DNS
#define DDSRT_HAVE_INET_NTOP   1
#define DDSRT_HAVE_INET_PTON   1

#if defined(NTDDI_VERSION) && \
    defined(_WIN32_WINNT_WS03) && \
    (NTDDI_VERSION >= _WIN32_WINNT_WS03)
#define DDSRT_HAVE_SSM 1
#else
#define DDSRT_HAVE_SSM 0
#endif

#define IFF_POINTOPOINT IFF_POINTTOPOINT

typedef unsigned ddsrt_iov_len_t;
typedef struct ddsrt_iovec {
  ddsrt_iov_len_t iov_len;
  void *iov_base;
} ddsrt_iovec_t;

typedef DWORD ddsrt_msg_iovlen_t;

typedef struct ddsrt_msghdr {
  void *msg_name;
  socklen_t msg_namelen;
  ddsrt_iovec_t *msg_iov;
  ddsrt_msg_iovlen_t msg_iovlen;
  void *msg_control;
  size_t msg_controllen;
  int msg_flags;
} ddsrt_msghdr_t;

#define DDSRT_MSGHDR_FLAGS 1

#if defined(__cplusplus)
}
#endif

#endif /* DDSRT_WINDOWS_SOCKET_H */
