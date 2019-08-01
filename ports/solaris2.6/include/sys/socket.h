#ifndef DDSRT_FIXUP_SYS_SOCKET_H
#define DDSRT_FIXUP_SYS_SOCKET_H

#include "netinet/in.h"
#include_next "sys/socket.h"

typedef size_t socklen_t;

struct sockaddr_storage {
  sa_family_t ss_family;
  struct sockaddr_in stuff;
};

#endif /* DDSRT_FIXUP_SYS_SOCKET_H */
