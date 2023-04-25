// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/security/dds_security_api.h"
#include "cryptography.h"
#include "crypto_key_exchange.h"
#include "crypto_key_factory.h"
#include "crypto_transform.h"

/**
 * Implementation structure for storing encapsulated members of the instance
 * while giving only the interface definition to user
 */

typedef struct dds_security_cryptography_impl {
  dds_security_cryptography base;
  struct ddsi_domaingv *gv;
} dds_security_cryptography_impl;

dds_security_crypto_key_factory *cryptography_get_crypto_key_factory (const struct dds_security_cryptography *crypto)
{
  const dds_security_cryptography_impl *instance = (dds_security_cryptography_impl *) crypto;
  return instance->base.crypto_key_factory;
}

dds_security_crypto_key_exchange *cryptography_get_crypto_key_exchange (const struct dds_security_cryptography *crypto)
{
  const dds_security_cryptography_impl *instance = (dds_security_cryptography_impl *) crypto;
  return instance->base.crypto_key_exchange;
}

dds_security_crypto_transform *cryptography_get_crypto_transform (const struct dds_security_cryptography *crypto)
{
  const dds_security_cryptography_impl *instance = (dds_security_cryptography_impl *) crypto;
  return instance->base.crypto_transform;
}


int init_crypto (const char *argument, void **context, struct ddsi_domaingv *gv)
{
  dds_security_cryptography_impl *cryptography;
  dds_security_crypto_key_exchange *crypto_key_exchange;
  dds_security_crypto_key_factory *crypto_key_factory;
  dds_security_crypto_transform *crypto_transform;

  DDSRT_UNUSED_ARG (argument);

  /* allocate new instance */
  cryptography = ddsrt_malloc (sizeof(*cryptography));
  cryptography->base.gv = gv;

  /* assign the sub components */
  crypto_key_exchange = dds_security_crypto_key_exchange__alloc ((dds_security_cryptography *)cryptography);
  if (!crypto_key_exchange) goto err_exchange;

  crypto_key_factory = dds_security_crypto_key_factory__alloc ((dds_security_cryptography *)cryptography);
  if (!crypto_key_factory) goto err_factory;

  crypto_transform = dds_security_crypto_transform__alloc ((dds_security_cryptography *)cryptography);
  if (!crypto_transform) goto err_transform;

  cryptography->base.crypto_key_exchange = crypto_key_exchange;
  cryptography->base.crypto_key_factory = crypto_key_factory;
  cryptography->base.crypto_transform = crypto_transform;

  /* return the instance */
  *context = cryptography;
  return DDS_SECURITY_SUCCESS;

err_transform:
  dds_security_crypto_key_factory__dealloc (crypto_key_factory);
err_factory:
  dds_security_crypto_key_exchange__dealloc (crypto_key_exchange);
err_exchange:
  ddsrt_free (cryptography);
  *context = NULL;
  return DDS_SECURITY_FAILED;
}

int finalize_crypto (void *instance)
{
  dds_security_cryptography_impl* instance_impl = (dds_security_cryptography_impl*) instance;
  /* deallocate components */
  dds_security_crypto_key_exchange__dealloc (instance_impl->base.crypto_key_exchange);
  dds_security_crypto_key_factory__dealloc (instance_impl->base.crypto_key_factory);
  dds_security_crypto_transform__dealloc (instance_impl->base.crypto_transform);
  /* deallocate cryptography */
  ddsrt_free (instance_impl);
  return DDS_SECURITY_SUCCESS;
}
