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
#ifndef __ospli_osplo__porting__
#define __ospli_osplo__porting__

#ifndef __GNUC__
#define __attribute__(x)
#endif

#if __SunOS_5_10 && ! defined NEED_STRSEP
#define NEED_STRSEP 1
#endif

#if NEED_STRSEP
char *strsep(char **str, const char *sep);
#endif

#endif /* defined(__ospli_osplo__porting__) */
