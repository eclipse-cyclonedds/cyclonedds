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

import org.eclipse.cyclonedds.ScopedName;

import java.util.*;

public class StructType extends AbstractType implements NamedType
{
  private static class Member
  {
    private Member (String n, Type t)
    {
      name = n;
      type = t;
    }

    private final String name;
    private final Type type;
  };

  public StructType (ScopedName name, NamedType parent)
  {
    this.name = name;
    this.parent = parent;
    members = new ArrayList <Member> ();
  }

  public void addMember (String name, Type type)
  {
    members.add (new Member (name, type.dup ()));
  }

  public int addKeyField (String fieldname)
  {
    // returns the offset in metadata of the field

    int result = 0;
    String search;
    Type mtype = null;

    int dotpos = fieldname.indexOf ('.');
    if (dotpos == -1)
    {
      search = fieldname;
    }
    else
    {
      search = fieldname.substring (0, dotpos);
    }

    for (Member m : members)
    {
      mtype = m.type;
      while (mtype instanceof TypedefType)
      {
        mtype = ((TypedefType)mtype).getRef ();
      }
      if (m.name.equals (search))
      {
        if (dotpos != -1)
        {
          result +=
            ((StructType)mtype).addKeyField (fieldname.substring (dotpos + 1));
        }
        mtype.makeKeyField ();
        break;
      }
      else
      {
        result += mtype.getMetaOpSize ();
      }
    }

    return result;
  }

  public ArrayList <String> getMetaOp (String myname, String structname)
  {
    ArrayList <String> result = new ArrayList <String> ();

    for (Member m : members)
    {
      if (myname == null)
      {
        result.addAll (m.type.getMetaOp (m.name, getCType ()));
      }
      else
      {
        result.addAll (m.type.getMetaOp (myname + "." + m.name, structname));
      }
    }

    return result;
  }

  public String getSubOp ()
  {
    return "DDS_OP_SUBTYPE_STU";
  }

  public String getOp ()
  {
    return "DDS_OP_TYPE_STU";
  }

  public String getCType ()
  {
    return name.toString ("_");
  }

  public int getMetaOpSize ()
  {
    int result = 0;
    for (Member m : members)
    {
      result += m.type.getMetaOpSize ();
    }
    return result;
  }

  public Alignment getAlignment ()
  {
    Alignment result = Alignment.ONE;
    for (Member m : members)
    {
      result = result.maximum (m.type.getAlignment ());
    }
    return result;
  }

  public boolean isUnoptimizable ()
  {
    Type mtype;

    for (Member m : members)
    {
      mtype = m.type;
      while (mtype instanceof TypedefType)
      {
        mtype = ((TypedefType)mtype).getRef ();
      }

      if (mtype instanceof UnionType && !((UnionType)mtype).canOptimize ())
      {
        return true;
      }
      if (mtype instanceof BoundedStringType)
      {
        return true;
      }

      if (mtype instanceof StructType)
      {
        if (((StructType)mtype).isUnoptimizable ())
        {
          return true;
        }
      }

      if (mtype.getAlignment ().equals (Alignment.PTR))
      {
        return true;
      }
    }
    return false;
  }

  public long getKeySize ()
  {
    Type mtype;
    long result = 0;

    for (Member m : members)
    {
      mtype = m.type;
      while (mtype instanceof TypedefType)
      {
        mtype = ((TypedefType)mtype).getRef ();
      }
      if (mtype.isKeyField ())
      {
        long subresult = mtype.getKeySize ();
        if (subresult == -1)
        {
          return -1;
        }
        else
        {
          result += subresult;
        }
      }
    }
    return result;
  }

  public StructType dup ()
  {
    StructType result = new StructType (getSN (), parent);
    for (Member m : members)
    {
      result.addMember (m.name, m.type);
    }
    return result;
  }

  public void getXML (StringBuffer str, ModuleContext mod)
  {
    if (parent != null && mod.isParent (name))
    {
      getXMLbase (str, mod);
    }
    else
    {
      str.append ("<Type name=\\\"");
      str.append (mod.nameFrom (name));
      str.append ("\\\"/>");
    }
  }

  public void getToplevelXML (StringBuffer str, ModuleContext mod)
  {
    mod.enter (str, name);
    getXMLbase (str, mod);
  }

  private void getXMLbase (StringBuffer str, ModuleContext mod)
  {
    mod.pushStruct (name.getLeaf ());
    str.append ("<Struct name=\\\"");
    str.append (name.getLeaf ());
    str.append ("\\\">");
    for (Member m : members)
    {
      str.append ("<Member name=\\\"");
      str.append (m.name);
      str.append ("\\\">");
      m.type.getXML (str, mod);
      str.append ("</Member>");
    }
    str.append ("</Struct>");
    mod.popStruct ();
  }

  public void populateDeps (Set <ScopedName> depset, NamedType current)
  {
    if (parent != null && current != null && !(parent.getSN ().equals (current.getSN ())))
    {
      parent.populateDeps (depset, current);
    }
    else
    {
      for (Member m : members)
      {
        m.type.populateDeps (depset, this);
      }
      if (parent == null || current == null)
      {
        depset.add (name);
      }
    }
  }

  public boolean depsOK (Set <ScopedName> deps)
  {
    for (Member m : members)
    {
      if (!TypeUtil.deptest (m.type, deps, name))
      {
        return false;
      }
    }
    return true;
  }

  public ScopedName getSN ()
  {
    return new ScopedName (name);
  }

  public void invalidate ()
  {
    invalid = true;
  }

  public boolean isInvalid ()
  {
    return invalid;
  }

  private final NamedType parent;
  private final ScopedName name;
  private boolean invalid = false;
  private ArrayList <Member> members;
}
