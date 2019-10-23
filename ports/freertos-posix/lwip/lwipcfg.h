#define LWIP_CHARGEN_APP              0
#define LWIP_DNS_APP                  0
#define LWIP_HTTPD_APP                0
/*#define LWIP_HTTPD_APP_NETCONN     */
#define LWIP_NETBIOS_APP              0
#define LWIP_NETIO_APP                0
#define LWIP_MDNS_APP                 0
#define LWIP_MQTT_APP                 0
#define LWIP_PING_APP                 0
#define LWIP_RTP_APP                  0
#define LWIP_SHELL_APP                0
#define LWIP_SNMP_APP                 0
#define LWIP_SNTP_APP                 0
#define LWIP_SOCKET_EXAMPLES_APP      0
#define LWIP_TCPECHO_APP              0
/*#define LWIP_TCPECHO_APP_NETCONN   */
#define LWIP_TFTP_APP                 0
#define LWIP_UDPECHO_APP              0
#define LWIP_LWIPERF_APP              0

#define USE_DHCP    0
#define USE_AUTOIP  0

/* #define USE_PCAPIF 1 */
#define LWIP_PORT_INIT_IPADDR(addr)   IP4_ADDR((addr), 192,168,1,200)
#define LWIP_PORT_INIT_GW(addr)       IP4_ADDR((addr), 192,168,1,1)
#define LWIP_PORT_INIT_NETMASK(addr)  IP4_ADDR((addr), 255,255,255,0)

/* remember to change this MAC address to suit your needs!
   the last octet will be increased by netif->num for each netif */
#define LWIP_MAC_ADDR_BASE            {0x00,0x01,0x02,0x03,0x04,0x05}

/* #define USE_SLIPIF 0 */
/* #define SIO_USE_COMPORT 0 */
#ifdef USE_SLIPIF
#if USE_SLIPIF
#define LWIP_PORT_INIT_SLIP1_IPADDR(addr)   IP4_ADDR((addr), 192, 168,   2, 2)
#define LWIP_PORT_INIT_SLIP1_GW(addr)       IP4_ADDR((addr), 192, 168,   2, 1)
#define LWIP_PORT_INIT_SLIP1_NETMASK(addr)  IP4_ADDR((addr), 255, 255, 255, 0)
#if USE_SLIPIF > 1
#define LWIP_PORT_INIT_SLIP2_IPADDR(addr)   IP4_ADDR((addr), 192, 168,   2, 1)
#define LWIP_PORT_INIT_SLIP2_GW(addr)       IP4_ADDR((addr), 0,     0,   0, 0)
#define LWIP_PORT_INIT_SLIP2_NETMASK(addr)  IP4_ADDR((addr), 255, 255, 255, 0)
#endif /* USE_SLIPIF > 1 */
#endif /* USE_SLIPIF */
#endif /* USE_SLIPIF */


