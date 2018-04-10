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

public class ArrayType extends AbstractType
{
  public ArrayType (Collection <Long> dimensions, Type subtype)
  {
    realsub = subtype;
    while (realsub instanceof TypedefType)
    {
      realsub = ((TypedefType)realsub).getRef ();
    }

    this.dimensions = new ArrayList <Long> ();
    if (realsub instanceof ArrayType)
    {
      ArrayType subarray = (ArrayType)realsub;
      dimensions.addAll (subarray.dimensions);
      this.subtype = subarray.subtype;
      realsub = this.subtype;
      while (realsub instanceof TypedefType)
      {
        realsub = ((TypedefType)realsub).getRef ();
      }
    }
    else
    {
      this.subtype = subtype;
    }
    this.dimensions.addAll (dimensions);
  }

  public Type dup ()
  {
    return new ArrayType (dimensions, subtype);
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
      "DDS_OP_ADR | DDS_OP_TYPE_ARR | " + realsub.getSubOp () + (isKeyField () ? " | DDS_OP_FLAG_KEY" : "") + ", " +
      offset + ", " + Long.toString (size ())
    ));

    if (!(realsub instanceof BasicType))
    {
      if (realsub instanceof BoundedStringType)
      {
        result.add ("0, " + Long.toString (((BoundedStringType)realsub).getBound () + 1));
      }
      else
      {
        result.add (new String ("(" + new Integer (getMetaOpSize ()) + "u << 16u) + 5u, sizeof (" + realsub.getCType () + ")"));
        result.addAll (realsub.getMetaOp (null, null));
        result.add (new String ("DDS_OP_RTS"));
      }
    }
    return result;
  }

  public String getSubOp ()
  {
    return "DDS_OP_SUBTYPE_ARR";
  }

  public String getOp ()
  {
    return "DDS_OP_TYPE_ARR";
  }

  public String getCType ()
  {
    StringBuffer str = new StringBuffer (subtype.getCType ());
    for (Long d : dimensions)
    {
      str.append ("[");
      str.append (d.toString ());
      str.append ("]");
    }
    return str.toString ();
  }

  public int getMetaOpSize ()
  {
    if (realsub instanceof BasicType)
    {
      return 3;
    }
    else
    {
      if (realsub instanceof BoundedStringType)
      {
        return 5;
      }
      else
      {
        return 6 + realsub.getMetaOpSize ();
      }
    }
  }

  public Alignment getAlignment ()
  {
    return subtype.getAlignment ();
  }

  public long getKeySize ()
  {
    long keysize = subtype.getKeySize ();
    if (keysize == -1)
    {
      return -1;
    }
    return keysize * size ();
  }

  public void getXML (StringBuffer str, ModuleContext mod)
  {
    for (Long d : dimensions)
    {
      str.append ("<Array size=\\\"");
      str.append (d.toString ());
      str.append ("\\\">");
    }
    subtype.getXML (str, mod);
    for (Long d : dimensions)
    {
      str.append ("</Array>");
    }
  }

  public void populateDeps (Set <ScopedName> depset, NamedType current)
  {
    subtype.populateDeps (depset, current);
  }

  public boolean depsOK (Set <ScopedName> deps)
  {
    return TypeUtil.deptest (subtype, deps, null);
  }

  private long size()
  {
    long result = 1;
    for (Long d : dimensions)
    {
      result *= d.longValue ();
    }
    return result;
  }

  private final Type subtype;
  private Type realsub;
  private final List <Long> dimensions;
}
