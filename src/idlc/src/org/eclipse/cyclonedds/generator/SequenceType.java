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
package org.eclipse.cyclonedds.generator;

import java.util.*;
import org.eclipse.cyclonedds.ScopedName;

public class SequenceType extends AbstractType
{
  public SequenceType (Type subtype)
  {
    this.subtype = subtype;
    realsub = subtype;
    while (realsub instanceof TypedefType)
    {
      realsub = ((TypedefType)realsub).getRef ();
    }
    ctype = "dds_sequence_t";
  }

  private SequenceType (Type subtype, String ctype)
  {
    this.subtype = subtype;
    realsub = subtype;
    while (realsub instanceof TypedefType)
    {
      realsub = ((TypedefType)realsub).getRef ();
    }
    this.ctype = ctype;
  }

  public Type dup ()
  {
    return new SequenceType (subtype.dup (), ctype);
  }

  public SequenceType clone (String name)
  {
    return new SequenceType (subtype.dup (), name);
  }

  public ArrayList <String> getMetaOp (String myname, String structname)
  {
    ArrayList <String> result = new ArrayList <String> ();
    String offset;

    if (myname != null)
    {
      offset = "offsetof (" + structname + ", " + myname + ")";
    }
    else
    {
      offset = "0u";
    }

    result.add (new String
    (
      "DDS_OP_ADR | DDS_OP_TYPE_SEQ | " + realsub.getSubOp () +  ", " + offset
    ));

    if (!(realsub instanceof BasicType))
    {
      if (realsub instanceof BoundedStringType)
      {
        result.add (Long.toString (((BoundedStringType)realsub).getBound () + 1));
      }
      else
      {
        result.add (new String ("sizeof (" + realsub.getCType () + "), (" + new Integer (getMetaOpSize ()) + "u << 16u) + 4u"));
        result.addAll (realsub.getMetaOp (null, null));
        result.add (new String ("DDS_OP_RTS"));
      }
    }
    return result;
  }

  public String getSubOp ()
  {
    return "DDS_OP_SUBTYPE_SEQ";
  }

  public String getOp ()
  {
    return "DDS_OP_TYPE_SEQ";
  }

  public String getCType ()
  {
    return ctype;
  }

  public long getKeySize ()
  {
    return -1;
  }

  public int getMetaOpSize ()
  {
    if (realsub instanceof BasicType)
    {
      return 2;
    }
    else
    {
      if (realsub instanceof BoundedStringType)
      {
        return 3;
      }
      else
      {
        return 5 + realsub.getMetaOpSize ();
      }
    }
  }

  public Alignment getAlignment ()
  {
    return Alignment.PTR;
  }

  public void getXML (StringBuffer str, ModuleContext mod)
  {
    str.append ("<Sequence>");
    subtype.getXML (str, mod);
    str.append ("</Sequence>");
  }

  public void populateDeps (Set <ScopedName> depset, NamedType current)
  {
    subtype.populateDeps (depset, current);
  }

  public boolean depsOK (Set <ScopedName> deps)
  {
    return TypeUtil.deptest (subtype, deps, null);
  }

  private final Type subtype;
  private Type realsub;
  private final String ctype;
}
