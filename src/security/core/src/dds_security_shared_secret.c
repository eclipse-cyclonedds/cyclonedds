// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/security/core/dds_security_shared_secret.h"

const DDS_Security_octet*
DDS_Security_get_challenge1_from_secret_handle (DDS_Security_SharedSecretHandle handle)
{
  DDS_Security_SharedSecretHandleImpl *secret = (DDS_Security_SharedSecretHandleImpl *)(uintptr_t)handle;
  return secret->challenge1;
}

const DDS_Security_octet*
DDS_Security_get_challenge2_from_secret_handle (DDS_Security_SharedSecretHandle handle)
{
  DDS_Security_SharedSecretHandleImpl *secret = (DDS_Security_SharedSecretHandleImpl *)(uintptr_t)handle;
  return secret->challenge2;
}

const DDS_Security_octet*
DDS_Security_get_secret_from_secret_handle (DDS_Security_SharedSecretHandle handle)
{
  DDS_Security_SharedSecretHandleImpl *secret = (DDS_Security_SharedSecretHandleImpl *)(uintptr_t)handle;
  return secret->shared_secret;
}

size_t
DDS_Security_get_secret_size_from_secret_handle (DDS_Security_SharedSecretHandle handle)
{
  DDS_Security_SharedSecretHandleImpl *secret = (DDS_Security_SharedSecretHandleImpl *)(uintptr_t)handle;
  return (size_t) secret->shared_secret_size;
}
