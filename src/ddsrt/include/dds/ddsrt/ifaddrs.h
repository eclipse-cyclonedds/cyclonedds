// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_IFADDRS_H
#define DDSRT_IFADDRS_H

#include "dds/ddsrt/sockets.h"

/**
  @file ifaddrs.h
  @brief This header deals with interface addresses
*/

#if defined (__cplusplus)
extern "C" {
#endif

/** @brief The interface type */
enum ddsrt_iftype {
  DDSRT_IFTYPE_UNKNOWN, ///< An unknown interface type
  DDSRT_IFTYPE_WIRED, ///< A wired interface (e.g. ethernet)
  DDSRT_IFTYPE_WIFI ///< A wifi (IEEE 802.11) interface
};

/** @brief Linked list of interface addresses */
struct ddsrt_ifaddrs {
  struct ddsrt_ifaddrs *next; ///< pointer to the next interface in the returned list of interfaces
  char *name; ///< name of the interface
  uint32_t index; ///< a unique integer associated with the interface
  uint32_t flags; ///< interface flags e.g. IFF_LOOPBACK, IFF_MASTER, IFF_MULTICAST
  enum ddsrt_iftype type; ///< @see ddsrt_iftype
  struct sockaddr *addr; ///< network address of this interface
  struct sockaddr *netmask; ///< netmask of this interface (or NULL if it doesn't have one)
  struct sockaddr *broadaddr; ///< broadcast address of this interface (or NULL if it doesn't have one)
};

typedef struct ddsrt_ifaddrs ddsrt_ifaddrs_t;

/**
 * @brief Get the interface addresses (@ref ddsrt_ifaddrs) (and interface information) for specific address families
 * 
 * A few points worth mentioning:
 * - Array 'afs' must be terminated with an element 'DDSRT_AF_TERM'.
 * - Only the interface addresses matching an address family in 'afs' are returned.
 * - In case of any error, '*ifap' is left untouched.
 * - In case of success, a new linked list @ref ddsrt_ifaddrs is allocated, and '*ifap' points to the first element.
 *   The user is responsible for freeing the memory at a later time using @ref ddsrt_freeifaddrs.
 * - Multiple calls are allowed (with different 'afs' or not).
 * 
 * @param[out] ifap interface address pointer
 * @param[in] afs an array of address families
 * @return a DDS_RETCODE (OK, ERROR, OUT_OF_RESOURCES, NOT_ALLOWED)
 */
DDS_EXPORT dds_return_t
ddsrt_getifaddrs(
  ddsrt_ifaddrs_t **ifap,
  const int *afs);

/**
 * @brief Free the interface addresses returned by @ref ddsrt_getifaddrs
 * 
 * @param[in] ifa the interface addresses to free
 */
DDS_EXPORT void
ddsrt_freeifaddrs(
  ddsrt_ifaddrs_t *ifa);

/**
 * @brief Get the mac address for a given interface name
 * 
 * Copies the first 6 bytes of the mac address into the buffer 'mac_addr'.
 * Specifying an unknown interface results in DDS_RETCODE_ERROR.
 * If the interface exists and doesn't actually have a MAC address, like a loopback interface,
 * the behaviour is platform-dependent. (On Linux, you get all 0, but that may not be universally true.)
 * 
 * @param[in] interface_name the name of the interface
 * @param[out] mac_addr a buffer to copy the mac address into
 * @return a DDS_RETCODE (OK, ERROR)
 */
dds_return_t
ddsrt_eth_get_mac_addr(
  char *interface_name,
  unsigned char *mac_addr);

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_IFADDRS_H */
