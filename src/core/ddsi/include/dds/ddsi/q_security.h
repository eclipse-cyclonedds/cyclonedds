/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifdef DDSI_INCLUDE_ENCRYPTION
#ifndef Q_SECURITY_H
#define Q_SECURITY_H

#include "c_typebase.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* Generic class */
C_CLASS(q_securityEncoderSet);
C_CLASS(q_securityDecoderSet);

/* Set of supported ciphers */
typedef enum 
{
  Q_CIPHER_UNDEFINED,
  Q_CIPHER_NULL,
  Q_CIPHER_BLOWFISH,
  Q_CIPHER_AES128,
  Q_CIPHER_AES192,
  Q_CIPHER_AES256,
  Q_CIPHER_NONE,
  Q_CIPHER_MAX
} q_cipherType;

void ddsi_security_plugin (void);

#if defined (__cplusplus)
}
#endif

#endif
#endif
