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

public class UnionType extends AbstractType implements NamedType
{
  private static class Member
  {
    private Member (String n, Type t, String[] l)
    {
      name = n;
      type = t;
      labels = l;
    }

    private final String name;
    private final Type type;
    private final String[] labels;
  };

  public UnionType (ScopedName name, NamedType parent)
  {
    this.name = name;
    members = new LinkedList <Member> ();
    discriminant = null;
    cardinality = 0;
    hasdefault = false;
    this.parent = parent;
  }

  public void setDiscriminant (Type discriminant)
  {
    this.discriminant = discriminant;
  }

  public void addMember
    (String name, Type type, String[] labels, boolean isdefault)
  {
/* We add an extra label for 'default'. Given the different requirements
 * for meta-op gen and xml gen, it may be better not to, and remember that
 * if (hasdefault && we are on the last member), there should be a default.
 */
    if (isdefault)
    {
      String[] alllabels = new String[labels.length + 1];
      System.arraycopy (labels, 0, alllabels, 0, labels.length);
      alllabels [labels.length] = "0";
      members.addLast (new Member (name, type, alllabels));
      cardinality += alllabels.length;
      hasdefault = true;
    }
    else
    {
      if (hasdefault)
      {
        Member def = members.removeLast ();
        members.addLast (new Member (name, type, labels));
        members.addLast (def);
      }
      else
      {
        members.addLast (new Member (name, type, labels));
      }
      cardinality += labels.length;
    }
  }

  public ArrayList <String> getMetaOp (String myname, String structname)
  {
    ArrayList <String> result = new ArrayList <String> ();

    String mynamedot;
    if (structname == null)
    {
      structname = getCType ();
      mynamedot = "";
    }
    else
    {
      mynamedot = myname + ".";
    }

    /* pre-calculate offsets for JEQ instructions */

    int[] offsets = new int[getCardinality ()];
    int opcount = 0;
    int i = 0;
    int l = 0;
    for (Member m : members)
    {
      if (m.type instanceof BasicType)
      {
        for (l = 0; l < m.labels.length; l++)
        {
          offsets [i++] = 0;
        }
      }
      else
      {
        for (l = 0; l < m.labels.length; l++)
        {
          offsets [i] = 3 * (getCardinality () - i) + opcount;
          i++;
        }
        opcount += (m.type.getMetaOpSize () + 1);
      }
    }

    /* discriminant */

    result.add
    (
      "DDS_OP_ADR | DDS_OP_TYPE_UNI | " + discriminant.getSubOp () +
        (hasdefault ? " | DDS_OP_FLAG_DEF" : "") + ", " +
      "offsetof (" + structname + ", " + mynamedot + "_d), " +
      new Integer (getCardinality ()) + "u, " +
      "(" + new Integer (getMetaOpSize ()) + "u << 16) + 4u"
    );

    /* JEQs */

    i = 0;
    for (Member m : members)
    {
      for (l = 0; l < m.labels.length; l++)
      {
        result.add
        (
          "DDS_OP_JEQ | " + m.type.getOp () + " | " +
            new Integer (offsets [i++]) + ", " +
          m.labels[l] + ", " +
          "offsetof (" + structname + ", " + mynamedot + "_u." + m.name + ")"
        );
      }
    }

    /* Subroutines for nonbasic types */

    for (Member m : members)
    {
      if (!(m.type instanceof BasicType))
      {
        result.addAll (m.type.getMetaOp (null, null));
        result.add (new String ("DDS_OP_RTS"));
      }
    }

    /* Done */

    return result;
  }

  public String getSubOp ()
  {
    return "DDS_OP_SUBTYPE_UNI";
  }

  public String getOp ()
  {
    return "DDS_OP_TYPE_UNI";
  }

  public String getCType ()
  {
    return name.toString ("_");
  }

  public long getKeySize ()
  {
    return -1;
  }

  public int getMetaOpSize ()
  {
    int result = 4 + 3 * getCardinality ();

    for (Member m : members)
    {
      if (!(m.type instanceof BasicType))
      {
        result += (m.type.getMetaOpSize () + 1);
      }
    }

    return result;
  }

  public boolean canOptimize ()
  {
    return false;
    /* strictly speaking this could be true. condition would be, if all the
    members have the same size, and if the non-basetype members are all
    optimizable themselves, and the alignment of the discriminant is not
    less than the alignment of the members. */
  }

  public Alignment getAlignment ()
  {
    Alignment result = discriminant.getAlignment ();
    for (Member m : members)
    {
      result = result.maximum (m.type.getAlignment ());
    }
    return result;
  }

  public UnionType dup ()
  {
    UnionType result = new UnionType (name, parent);
    result.setDiscriminant (discriminant);
    result.members.addAll (members);
    result.hasdefault = hasdefault;
    result.cardinality = cardinality;
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
    str.append ("<Union name=\\\"");
    str.append (name.getLeaf ());
    str.append ("\\\"><SwitchType>");
    discriminant.getXML (str, mod);
    str.append ("</SwitchType>");
    for (Member m : members)
    {
      str.append ("<Case name=\\\"");
      str.append (m.name);
      str.append ("\\\">");
      m.type.getXML (str, mod);
      for (int l = 0; l < m.labels.length; l++)
      {
        if (!hasdefault || m != members.getLast () || l != m.labels.length - 1)
        {
          str.append ("<Label value=\\\"");
          str.append (mungeLabel (m.labels[l]));
          str.append ("\\\"/>");
        }
        if (hasdefault && m == members.getLast () && m.labels.length == 1)
        {
          str.append ("<Default/>");
        }
      }
      str.append ("</Case>");
    }
    str.append ("</Union>");
    mod.popStruct ();
  }

  private String mungeLabel (String orig)
  {
    Type rt = discriminant;
    while (rt instanceof TypedefType)
    {
      rt = ((TypedefType)rt).getRef ();
    }
    if (((BasicType)rt).type == BasicType.BT.CHAR)
    {
      return orig.substring (1, 2);
    }
    else if (((BasicType)rt).type == BasicType.BT.BOOLEAN)
    {
      return (orig.equals ("true") ? "True" : "False");
    }
    else if (discriminant instanceof EnumType)
    {
      return ((EnumType)discriminant).descope (orig);
    }
    else
    {
      return orig;
    }
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
      discriminant.populateDeps (depset, this);
      if (parent == null || current == null)
      {
        depset.add (name);
      }
    }
  }

  public boolean depsOK (Set <ScopedName> deps)
  {
    if (!TypeUtil.deptest (discriminant, deps, name))
    {
      return false;
    }

    for (Member m : members)
    {
      if (!TypeUtil.deptest (m.type, deps, name))
      {
        return false;
      }
    }
    return true;
  }

  private int getCardinality()
  {
    return cardinality;
  }

  public ScopedName getSN ()
  {
    return name;
  }

  private final NamedType parent;
  private final ScopedName name;
  private Type discriminant;
  private LinkedList <Member> members;
  private int cardinality;
  private boolean hasdefault;
}
