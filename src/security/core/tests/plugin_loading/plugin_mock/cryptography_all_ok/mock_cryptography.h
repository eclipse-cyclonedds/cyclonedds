/*
 * authentication.h
 *
 *  Created on: Jan 15, 2018
 *      Author: kurtulus oksuztepe
 */

#ifndef SECURITY_CRYPTO_OK_H_
#define SECURITY_CRYPTO_OK_H_

#include "dds/security/cryptography_all_ok_export.h"
#include "dds/security/dds_security_api.h"

SECURITY_EXPORT int32_t
init_crypto(const char *argument, void **context);

SECURITY_EXPORT int32_t
finalize_crypto(void *context);


#endif /* SECURITY_CRYPTO_OK_H_ */
