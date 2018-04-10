// Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

dlldef (name, dllname, dllfile) ::= <<

<if(dllfile)>#include "<dllfile>"<else>
#undef DDS_EXPORT
#ifdef _WIN32_DLL_
  #ifdef _<name>_DLL_
    #define DDS_EXPORT extern __declspec (dllexport)
  #else
    #ifdef DDS_BUILD_<dllname>_DLL
      #define DDS_EXPORT extern
    #else
      #define DDS_EXPORT extern __declspec (dllimport)
    #endif
  #endif
#else
  #define DDS_EXPORT extern
#endif
<endif>
>>
