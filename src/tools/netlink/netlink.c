#include <assert.h>
#include <stdio.h>
#if _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <arpa/inet.h>
# include <netinet/in.h>
#endif

#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/event.h"

static dds_return_t
callback(ddsrt_event_t *event, uint32_t flags, const void *data, void *user_data)
{
  char ip[INET6_ADDRSTRLEN + 1];
  const ddsrt_netlink_message_t *msg = data;

  assert(event && (event->flags & DDSRT_NETLINK));

  (void)event;
  (void)flags;
  (void)user_data;

  if (flags & (DDSRT_LINK_UP|DDSRT_LINK_DOWN)) {
    fprintf(stderr, "got link %s event\n", (flags & DDSRT_LINK_UP) ? "up" : "down");
  } else if (flags & (DDSRT_IPV4_ADDED|DDSRT_IPV4_DELETED)) {
    const char *ev = (flags & DDSRT_IPV4_ADDED) ? "added" : "deleted";
    inet_ntop(AF_INET, &((struct sockaddr_in *)&msg->address)->sin_addr, ip, sizeof(ip));
    fprintf(stderr, "got ip4 (%s) %s event\n", ip, ev);
  } else if (flags & (DDSRT_IPV6_ADDED|DDSRT_IPV6_DELETED)) {
    const char *ev = (flags & DDSRT_IPV6_ADDED) ? "added" : "deleted";
    inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&msg->address)->sin6_addr, ip, sizeof(ip));
    fprintf(stderr, "got ip6 (%s) %s event\n", ip, ev);
  } else {
    assert(0);
  }

  return DDS_RETCODE_OK;
}

int main(int argc, char *argv[])
{
  ddsrt_event_t event;
  ddsrt_loop_t loop;
  uint32_t flags = DDSRT_NETLINK | (DDSRT_LINK_UP    | DDSRT_LINK_DOWN    |
                                    DDSRT_IPV4_ADDED | DDSRT_IPV4_DELETED |
                                    DDSRT_IPV6_ADDED | DDSRT_IPV6_DELETED);

  (void)argc;
  (void)argv;

#if _WIN32
  WSADATA wsa_data;
  if (WSAStartup(MAKEWORD(2,0), &wsa_data) != 0)
    return 1;
#endif

  ddsrt_create_event(&event, DDSRT_INVALID_SOCKET, flags, &callback, NULL);
  ddsrt_create_loop(&loop);
  ddsrt_add_event(&loop, &event);

  ddsrt_run_loop(&loop, 0u, NULL);

  return 0;
}
