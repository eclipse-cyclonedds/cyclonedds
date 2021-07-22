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
#include <string.h>

#include "dds/ddsi/ddsi_vendor.h"

DDS_EXPORT extern inline bool vendor_equals (nn_vendorid_t a, nn_vendorid_t b);
DDS_EXPORT extern inline bool vendor_is_rti (nn_vendorid_t vendor);
DDS_EXPORT extern inline bool vendor_is_twinoaks (nn_vendorid_t vendor);
DDS_EXPORT extern inline bool vendor_is_eprosima (nn_vendorid_t vendor);
DDS_EXPORT extern inline bool vendor_is_adlink (nn_vendorid_t vendor);
DDS_EXPORT extern inline bool vendor_is_opensplice (nn_vendorid_t vendor);
DDS_EXPORT extern inline bool vendor_is_cloud (nn_vendorid_t vendor);
DDS_EXPORT extern inline bool vendor_is_eclipse (nn_vendorid_t vendor);
DDS_EXPORT extern inline bool vendor_is_eclipse_or_opensplice (nn_vendorid_t vendor);
DDS_EXPORT extern inline bool vendor_is_eclipse_or_adlink (nn_vendorid_t vendor);
