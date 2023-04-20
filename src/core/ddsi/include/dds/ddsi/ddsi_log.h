// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_LOG_H
#define DDSI_LOG_H

#include <stdarg.h>

#include "dds/ddsrt/log.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define GVTRACE(...)        DDS_CTRACE (&gv->logconfig, __VA_ARGS__)
#define GVLOG(cat, ...)     DDS_CLOG ((cat), &gv->logconfig, __VA_ARGS__)
#define GVWARNING(...)      DDS_CLOG (DDS_LC_WARNING, &gv->logconfig, __VA_ARGS__)
#define GVERROR(...)        DDS_CLOG (DDS_LC_ERROR, &gv->logconfig, __VA_ARGS__)

#define RSTTRACE(...)       DDS_CTRACE (&rst->gv->logconfig, __VA_ARGS__)

#define ETRACE(e_, ...)     DDS_CTRACE (&(e_)->e.gv->logconfig, __VA_ARGS__)
#define EETRACE(e_, ...)    DDS_CTRACE (&(e_)->gv->logconfig, __VA_ARGS__)
#define ELOG(cat, e_, ...)  DDS_CLOG ((cat), &(e_)->e.gv->logconfig, __VA_ARGS__)
#define EELOG(cat, e_, ...) DDS_CLOG ((cat), &(e_)->gv->logconfig, __VA_ARGS__)

/* There are quite a few places where discovery-related things are logged, so abbreviate those
   a bit */
#define GVLOGDISC(...)      DDS_CLOG (DDS_LC_DISCOVERY, &gv->logconfig, __VA_ARGS__)
#define ELOGDISC(e_,...)    DDS_CLOG (DDS_LC_DISCOVERY, &(e_)->e.gv->logconfig, __VA_ARGS__)
#define EELOGDISC(e_, ...)  DDS_CLOG (DDS_LC_DISCOVERY, &(e_)->gv->logconfig, __VA_ARGS__)

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_LOG_H */
