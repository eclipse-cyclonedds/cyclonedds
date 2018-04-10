// Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

struct (name, scope, fields, extern, alignment, keys, flags, declarations, marshalling, xml, istopic) ::= <<

<declarations; separator="\n">

typedef struct <scopedname(...)>
{
  <fields; separator="\n">
} <scopedname(...)>;

<if(istopic)>
<extern> const dds_topic_descriptor_t <scopedname(...)>_desc;

<allocs(...)>

#define <scopedname(...)>_free(d,o) \
dds_sample_free ((d), &<scopedname(...)>_desc, (o))
<endif>
>>
