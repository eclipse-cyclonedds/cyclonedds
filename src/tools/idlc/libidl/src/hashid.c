// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdint.h>
#include <string.h>

#include "idl/md5.h"

#include "hashid.h"
#include "fieldid.h"

uint32_t idl_hashid(const char *name)
{
  uint32_t id;

  idl_md5_state_t md5st;
  idl_md5_byte_t digest[16];

  idl_md5_init(&md5st);
  idl_md5_append(&md5st, (idl_md5_byte_t*)name, (unsigned int)strlen(name));
  idl_md5_finish(&md5st, digest);
  // Xtypes 1.3, 7.3.1.2.1.1:
  // 1. Compute a 4-byte hash of the string as specified in 7.2.2.4.4.4.5.
  // 2. Interpret the resulting 4-byte has as a Little Endian unsigned 32-bit integer.
  // 3. Perform a bitwise AND operation with the integer 0x0FFFFFFF to zero the most
  //    significant 4-bits of the integer.
  id = digest[0] | ((uint32_t)digest[1] << 8) | ((uint32_t)digest[2] << 16) | ((uint32_t)digest[3] << 24);
  return id & IDL_FIELDID_MASK;
}
