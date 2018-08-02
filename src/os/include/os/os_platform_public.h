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
#ifndef OS_PLATFORM_PUBLIC_H
#define OS_PLATFORM_PUBLIC_H

#if __linux__ == 1
  #include "os/posix/os_platform_public.h"
#elif defined(__VXWORKS__)
  #include "os/posix/os_platform_public.h"
#elif defined(_MSC_VER)
  #include "os/windows/os_platform_public.h"
#elif defined __APPLE__
  #include "os/posix/os_platform_public.h"
#elif defined __sun
  #include "os/posix/os_platform_public.h"
#else
  #error "Platform missing from os_public.h list"
#endif

#endif
