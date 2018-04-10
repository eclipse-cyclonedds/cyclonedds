// Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

union (name, scope, disc, discscope, fields, declarations) ::= <<

<declarations; separator="\n">

typedef struct <scopedname(...)>
{
  <ctype(discscope, disc)> _d;
  union
  {
    <fields; separator="\n">
  } _u;
} <scopedname(...)>;

<allocs(...)>
>>
