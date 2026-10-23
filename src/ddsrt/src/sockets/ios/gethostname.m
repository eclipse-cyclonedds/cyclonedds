// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#import <UIKit/UIKit.h>
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/string.h"

#if DDSRT_HAVE_GETHOSTNAME
dds_return_t ddsrt_gethostname (char *hostname, size_t buffersize)
{
  @autoreleasepool {
    const char *name = [UIDevice currentDevice].name.UTF8String;
    if (name == NULL)
      return DDS_RETCODE_ERROR;
    if (ddsrt_strlcpy(hostname, name, buffersize) >= buffersize)
      return DDS_RETCODE_NOT_ENOUGH_SPACE;
    return DDS_RETCODE_OK;
  }
}
#endif
