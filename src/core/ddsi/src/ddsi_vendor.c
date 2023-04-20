// Copyright(c) 2019 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>

#include "ddsi__vendor.h"

extern inline bool ddsi_vendor_equals (ddsi_vendorid_t a, ddsi_vendorid_t b);
extern inline bool ddsi_vendor_is_rti (ddsi_vendorid_t vendor);
extern inline bool ddsi_vendor_is_rti_micro (ddsi_vendorid_t vendor);
extern inline bool ddsi_vendor_is_twinoaks (ddsi_vendorid_t vendor);
extern inline bool ddsi_vendor_is_eprosima (ddsi_vendorid_t vendor);
extern inline bool ddsi_vendor_is_adlink (ddsi_vendorid_t vendor);
extern inline bool ddsi_vendor_is_opensplice (ddsi_vendorid_t vendor);
extern inline bool ddsi_vendor_is_cloud (ddsi_vendorid_t vendor);
extern inline bool ddsi_vendor_is_eclipse (ddsi_vendorid_t vendor);
extern inline bool ddsi_vendor_is_eclipse_or_opensplice (ddsi_vendorid_t vendor);
extern inline bool ddsi_vendor_is_eclipse_or_adlink (ddsi_vendorid_t vendor);
