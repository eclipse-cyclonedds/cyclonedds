#ifndef DDSRT_FIXUP_NETINET_IN_H
#define DDSRT_FIXUP_NETINET_IN_H

#include_next "netinet/in.h"

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

#endif /* DDSRT_FIXUP_NETINET_IN_H */
