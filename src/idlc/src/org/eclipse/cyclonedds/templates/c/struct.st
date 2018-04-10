// Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

struct (name, scope, extern, alignment, fields, keys, flags, declarations, marshalling, xml, istopic) ::= <<

<declarations>

<if(istopic)>
<if(keys)>
static const dds_key_descriptor_t <scopedname(...)>_keys[<length(keys)>] =
{
  <keys:{ k | { "<k.name>", <k.offset> }; separator="},\n">}
};
<endif>

static const uint32_t <scopedname(...)>_ops [] =
{
  <marshalling; separator=",\n">
};

const dds_topic_descriptor_t <scopedname(...)>_desc =
{
  sizeof (<scopedname(...)>),
  <alignment>,
  <if(flags)><flags; separator=" | "><else>0u<endif>,
  <if(keys)><length(keys)><else>0<endif>u,
  "<colonscopedname(...)>",
  <if(keys)><scopedname(...)>_keys<else>NULL<endif>,
  <length(marshalling)>,
  <scopedname(...)>_ops,
  <if(xml)>"\<MetaData version=\"1.0.0\"><xml>\</MetaData>"<else>NULL<endif>
};
<endif>
>>
