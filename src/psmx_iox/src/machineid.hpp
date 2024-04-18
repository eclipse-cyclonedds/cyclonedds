// Copyright(c) 2024 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef MACHINEID_HPP
#define MACHINEID_HPP

#include <cstdint>
#include <array>
#include <optional>

#include "dds/ddsc/dds_psmx.h"

std::optional<dds_psmx_node_identifier_t> get_machineid ();

#endif /* MACHINEID_HPP */
