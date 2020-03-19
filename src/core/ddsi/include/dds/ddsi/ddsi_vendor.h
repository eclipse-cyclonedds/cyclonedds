/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI_VENDOR_H
#define DDSI_VENDOR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  uint8_t id[2];
} nn_vendorid_t;


/* All existing vendor codes have the major part equal to 1 (and this will probably be true for a long, long time) */
#define NN_VENDORID_MINOR_RTI                0x01
#define NN_VENDORID_MINOR_ADLINK_OSPL        0x02
#define NN_VENDORID_MINOR_OCI                0x03
#define NN_VENDORID_MINOR_MILSOFT            0x04
#define NN_VENDORID_MINOR_KONGSBERG          0x05
#define NN_VENDORID_MINOR_TWINOAKS           0x06
#define NN_VENDORID_MINOR_LAKOTA             0x07
#define NN_VENDORID_MINOR_ICOUP              0x08
#define NN_VENDORID_MINOR_ETRI               0x09
#define NN_VENDORID_MINOR_RTI_MICRO          0x0a
#define NN_VENDORID_MINOR_ADLINK_JAVA        0x0b
#define NN_VENDORID_MINOR_ADLINK_GATEWAY     0x0c
#define NN_VENDORID_MINOR_ADLINK_LITE        0x0d
#define NN_VENDORID_MINOR_TECHNICOLOR        0x0e
#define NN_VENDORID_MINOR_EPROSIMA           0x0f
#define NN_VENDORID_MINOR_ECLIPSE            0x10
#define NN_VENDORID_MINOR_ADLINK_CLOUD       0x20

#if defined(_WIN32) && defined(__cplusplus)
#define NN_VENDORID(vendor) {{ 0x01, NN_VENDORID_MINOR_##vendor }}
#define NN_VENDORID_UNKNOWN {{ 0x00, 0x00 }}
#else
#define NN_VENDORID(vendor) ((nn_vendorid_t) {{ 0x01, NN_VENDORID_MINOR_##vendor }})
#define NN_VENDORID_UNKNOWN ((nn_vendorid_t) {{ 0x00, 0x00 }})
#endif

#define NN_VENDORID_ECLIPSE NN_VENDORID (ECLIPSE)

#if defined (__cplusplus)
extern "C" {
#endif

inline bool vendor_equals (nn_vendorid_t a, nn_vendorid_t b) {
  return ((a.id[0] << 8) | a.id[1]) == ((b.id[0] << 8) | b.id[1]);
}
inline bool vendor_is_eclipse (nn_vendorid_t vendor) {
  return vendor_equals (vendor, NN_VENDORID (ECLIPSE));
}
inline bool vendor_is_rti (nn_vendorid_t vendor) {
  return vendor_equals (vendor, NN_VENDORID (RTI));
}
inline bool vendor_is_opensplice (nn_vendorid_t vendor) {
  return vendor_equals (vendor, NN_VENDORID (ADLINK_OSPL));
}
inline bool vendor_is_twinoaks (nn_vendorid_t vendor) {
  return vendor_equals (vendor, NN_VENDORID (TWINOAKS));
}
inline bool vendor_is_eprosima (nn_vendorid_t vendor) {
  return vendor_equals (vendor, NN_VENDORID (EPROSIMA));
}
inline bool vendor_is_cloud (nn_vendorid_t vendor) {
  return vendor_equals (vendor, NN_VENDORID (ADLINK_CLOUD));
}
inline bool vendor_is_eclipse_or_opensplice (nn_vendorid_t vendor) {
  return vendor_is_eclipse (vendor) | vendor_is_opensplice (vendor);
}
inline bool vendor_is_adlink (nn_vendorid_t vendor) {
  return (vendor_equals (vendor, NN_VENDORID (ADLINK_OSPL)) ||
          vendor_equals (vendor, NN_VENDORID (ADLINK_LITE)) ||
          vendor_equals (vendor, NN_VENDORID (ADLINK_GATEWAY)) ||
          vendor_equals (vendor, NN_VENDORID (ADLINK_JAVA)) ||
          vendor_equals (vendor, NN_VENDORID (ADLINK_CLOUD)));
}
inline bool vendor_is_eclipse_or_adlink (nn_vendorid_t vendor) {
  return vendor_is_eclipse (vendor) || vendor_is_adlink (vendor);
}

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_VENDOR_H */
