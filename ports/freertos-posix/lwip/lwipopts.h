#ifndef __USER_LWIPOPTS_H__
#define __USER_LWIPOPTS_H__

#include "lwip-contrib/ports/unix/lib/lwipopts.h"

/*#define SHOW_MESSAGE*/

#ifdef SHOW_MESSAGE
#define Stringize( L )     #L
#define MakeString( M, L ) M(L)

#define __LWIP_IGMP           MakeString( Stringize, LWIP_IGMP )
#define __LWIP_BROADCAST_PING MakeString( Stringize, LWIP_BROADCAST_PING )
#define __LWIP_MULTICAST_PING MakeString( Stringize, LWIP_MULTICAST_PING )
#define __SO_REUSE            MakeString( Stringize, SO_REUSE )
#define __LWIP_AUTOIP         MakeString( Stringize, LWIP_AUTOIP )
#define __LWIP_DHCP           MakeString( Stringize, LWIP_DHCP )
#endif

#if defined(LWIP_IGMP)
#ifdef SHOW_MESSAGE
#pragma message("lwip_igmp has been defined (" __LWIP_IGMP ")")
#endif
#undef LWIP_IGMP
#endif
#define LWIP_IGMP 1

#if defined(LWIP_BROADCAST_PING)
#ifdef SHOW_MESSAGE
#pragma message("lwip_broadcast_ping has been defined (" __LWIP_BROADCAST_PING ")")
#endif
#undef LWIP_BROADCAST_PING
#endif
#define LWIP_BROADCAST_PING 1

#if defined(LWIP_MULTICAST_PING)
#ifdef SHOW_MESSAGE
#pragma message("lwip_multicast_ping has been defined (" __LWIP_MULTICAST_PING ")")
#endif
#undef LWIP_MULTICAST_PING
#endif
#define LWIP_MULTICAST_PING 1

#if defined(SO_REUSE)
#ifdef SHOW_MESSAGE
#pragma message("so_reuse has been defined (" __SO_REUSE ")")
#endif
#undef SO_REUSE
#endif
#define SO_REUSE 1

#if defined(LWIP_AUTOIP)
#ifdef SHOW_MESSAGE
#pragma message("lwip_autoip has been defined (" __LWIP_AUTOIP ")")
#endif
#undef LWIP_AUTOIP
#endif
#define LWIP_AUTOIP 0

#if defined(LWIP_DHCP)
#ifdef SHOW_MESSAGE
#pragma message("lwip_dhcp has been defined (" __LWIP_DHCP ")")
#endif
#undef LWIP_DHCP
#endif
#define LWIP_DHCP 0

#endif
