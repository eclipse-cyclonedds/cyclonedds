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
#if defined _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern void idl_create_locale(void);
extern void idl_delete_locale(void);

void WINAPI idl_cdtor(PVOID handle, DWORD reason, PVOID reserved)
{
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      /* fall through */
    case DLL_THREAD_ATTACH:
      idl_create_locale();
      break;
    case DLL_THREAD_DETACH:
      /* fall through */
    case DLL_PROCESS_DETACH:
      idl_delete_locale();
      break;
    default:
      break;
  }
}

#if defined _WIN64
  #pragma comment (linker, "/INCLUDE:_tls_used")
  #pragma comment (linker, "/INCLUDE:tls_callback_func")
  #pragma const_seg(".CRT$XLZ")
  EXTERN_C const PIMAGE_TLS_CALLBACK tls_callback_func = idl_cdtor;
  #pragma const_seg()
#else
  #pragma comment (linker, "/INCLUDE:__tls_used")
  #pragma comment (linker, "/INCLUDE:_tls_callback_func")
  #pragma data_seg(".CRT$XLZ")
  EXTERN_C PIMAGE_TLS_CALLBACK tls_callback_func = idl_cdtor;
  #pragma data_seg()
#endif /* _WIN64 */
#endif /* _WIN32 */
