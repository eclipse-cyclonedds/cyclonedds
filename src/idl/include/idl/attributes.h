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
#ifndef IDL_ATTRIBUTES_H
#define IDL_ATTRIBUTES_H

#if defined(__has_attribute)
# define idl_has_attribute(params) __has_attribute(params)
#elif __GNUC__
# define idl_has_attribute(params) (1) /* GCC < 5 */
#else
# define idl_has_attribute(params) (0)
#endif

#if idl_has_attribute(format)
# define idl_attribute_format(params) __attribute__((__format__ params))
# if __MINGW32__
#   if !defined(__MINGW_PRINTF_FORMAT)
#     define __MINGW_PRINTF_FORMAT gnu_printf
#   endif
    /* GCC assumes printf MS style arguments on Windows */
#   define idl_attribute_format_printf(string_index, first_to_check) \
      idl_attribute_format((__MINGW_PRINTF_FORMAT, string_index, first_to_check))
# else
#   define idl_attribute_format_printf(string_index, first_to_check) \
      idl_attribute_format((printf, string_index, first_to_check))
# endif
#else
# define idl_attribute_format(params)
# define idl_attribute_format_printf(string_index, first_to_check)
#endif

#endif /* IDL_ATTRIBUTES_H */
