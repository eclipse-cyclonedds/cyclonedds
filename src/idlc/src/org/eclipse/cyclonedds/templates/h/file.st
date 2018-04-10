// Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

file (banner, name, nameupper, declarations, dll, includes) ::= <<
<banner>

#include "ddsc/dds_public_impl.h"
<includes:{n | #include "<n>.h"}; separator="\n">

#ifndef _DDSL_<nameupper>_H_
#define _DDSL_<nameupper>_H_

<if(dll)><dll><endif>

#ifdef __cplusplus
extern "C" {
#endif

<declarations; separator="\n">
#ifdef __cplusplus
}
#endif
#endif /* _DDSL_<nameupper>_H_ */

>>
