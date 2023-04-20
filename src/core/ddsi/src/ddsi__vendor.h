// Copyright(c) 2019 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__VENDOR_H
#define DDSI__VENDOR_H

#include <stdint.h>
#include <stdbool.h>

#include "dds/export.h"
#include "dds/ddsi/ddsi_protocol.h"

/* All existing vendor codes have the major part equal to 1 (and this will probably be true for a long, long time) */
#define DDSI_VENDORID_MINOR_RTI                0x01
#define DDSI_VENDORID_MINOR_ADLINK_OSPL        0x02
#define DDSI_VENDORID_MINOR_OCI                0x03
#define DDSI_VENDORID_MINOR_MILSOFT            0x04
#define DDSI_VENDORID_MINOR_KONGSBERG          0x05
#define DDSI_VENDORID_MINOR_TWINOAKS           0x06
#define DDSI_VENDORID_MINOR_LAKOTA             0x07
#define DDSI_VENDORID_MINOR_ICOUP              0x08
#define DDSI_VENDORID_MINOR_ETRI               0x09
#define DDSI_VENDORID_MINOR_RTI_MICRO          0x0a
#define DDSI_VENDORID_MINOR_ADLINK_JAVA        0x0b
#define DDSI_VENDORID_MINOR_ADLINK_GATEWAY     0x0c
#define DDSI_VENDORID_MINOR_ADLINK_LITE        0x0d
#define DDSI_VENDORID_MINOR_TECHNICOLOR        0x0e
#define DDSI_VENDORID_MINOR_EPROSIMA           0x0f
#define DDSI_VENDORID_MINOR_ECLIPSE            0x10
#define DDSI_VENDORID_MINOR_ADLINK_CLOUD       0x20

#define DDSI_VENDORID_INIT(vendor) {{ 0x01, DDSI_VENDORID_MINOR_##vendor }}
#define DDSI_VENDORID_INIT_UNKNOWN {{ 0x00, 0x00 }}

#define DDSI_VENDORID(vendor) ((ddsi_vendorid_t) DDSI_VENDORID_INIT(vendor))
#define DDSI_VENDORID_UNKNOWN ((ddsi_vendorid_t) DDSI_VENDORID_INIT_UNKNOWN)

#define DDSI_VENDORID_ECLIPSE DDSI_VENDORID(ECLIPSE)

#if defined (__cplusplus)
extern "C" {
#endif

/** @component vendor_codes */
inline bool ddsi_vendor_equals (ddsi_vendorid_t a, ddsi_vendorid_t b) {
  return ((a.id[0] << 8) | a.id[1]) == ((b.id[0] << 8) | b.id[1]);
}

/** @component vendor_codes */
inline bool ddsi_vendor_is_eclipse (ddsi_vendorid_t vendor) {
  const ddsi_vendorid_t x = DDSI_VENDORID_INIT (ECLIPSE);
  return ddsi_vendor_equals (vendor, x);
}

/** @component vendor_codes */
inline bool ddsi_vendor_is_rti (ddsi_vendorid_t vendor) {
  const ddsi_vendorid_t x = DDSI_VENDORID_INIT (RTI);
  return ddsi_vendor_equals (vendor, x);
}

/** @component vendor_codes */
inline bool ddsi_vendor_is_rti_micro (ddsi_vendorid_t vendor) {
  const ddsi_vendorid_t x = DDSI_VENDORID_INIT (RTI_MICRO);
  return ddsi_vendor_equals (vendor, x);
}

/** @component vendor_codes */
inline bool ddsi_vendor_is_opensplice (ddsi_vendorid_t vendor) {
  const ddsi_vendorid_t x = DDSI_VENDORID_INIT (ADLINK_OSPL);
  return ddsi_vendor_equals (vendor, x);
}

/** @component vendor_codes */
inline bool ddsi_vendor_is_twinoaks (ddsi_vendorid_t vendor) {
  const ddsi_vendorid_t x = DDSI_VENDORID_INIT (TWINOAKS);
  return ddsi_vendor_equals (vendor, x);
}

/** @component vendor_codes */
inline bool ddsi_vendor_is_eprosima (ddsi_vendorid_t vendor) {
  const ddsi_vendorid_t x = DDSI_VENDORID_INIT (EPROSIMA);
  return ddsi_vendor_equals (vendor, x);
}

/** @component vendor_codes */
inline bool ddsi_vendor_is_cloud (ddsi_vendorid_t vendor) {
  const ddsi_vendorid_t x = DDSI_VENDORID_INIT (ADLINK_CLOUD);
  return ddsi_vendor_equals (vendor, x);
}

/** @component vendor_codes */
inline bool ddsi_vendor_is_eclipse_or_opensplice (ddsi_vendorid_t vendor) {
  return ddsi_vendor_is_eclipse (vendor) || ddsi_vendor_is_opensplice (vendor);
}

/** @component vendor_codes */
inline bool ddsi_vendor_is_adlink (ddsi_vendorid_t vendor) {
  const ddsi_vendorid_t a = DDSI_VENDORID_INIT (ADLINK_OSPL);
  const ddsi_vendorid_t b = DDSI_VENDORID_INIT (ADLINK_LITE);
  const ddsi_vendorid_t c = DDSI_VENDORID_INIT (ADLINK_GATEWAY);
  const ddsi_vendorid_t d = DDSI_VENDORID_INIT (ADLINK_JAVA);
  const ddsi_vendorid_t e = DDSI_VENDORID_INIT (ADLINK_CLOUD);
  return (ddsi_vendor_equals (vendor, a) ||
          ddsi_vendor_equals (vendor, b) ||
          ddsi_vendor_equals (vendor, c) ||
          ddsi_vendor_equals (vendor, d) ||
          ddsi_vendor_equals (vendor, e));
}

/** @component vendor_codes */
inline bool ddsi_vendor_is_eclipse_or_adlink (ddsi_vendorid_t vendor) {
  return ddsi_vendor_is_eclipse (vendor) || ddsi_vendor_is_adlink (vendor);
}

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__VENDOR_H */
