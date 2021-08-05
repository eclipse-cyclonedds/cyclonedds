/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef IDL_MISC_H
#define IDL_MISC_H

#if defined(_MSC_VER)
# define DDSRT_WARNING_MSVC_OFF(x) \
    __pragma (warning(push)) \
    __pragma (warning(disable: ## x))
# define DDSRT_WARNING_MSVC_ON(x) \
    __pragma (warning(pop))
#else
# define DDSRT_WARNING_MSVC_OFF(x)
# define DDSRT_WARNING_MSVC_ON(x)
#endif

#endif /* IDL_MISC_H */
