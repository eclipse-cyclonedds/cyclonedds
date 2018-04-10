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

public class EnumType extends BasicType implements NamedType
{
  public EnumType (ScopedName SN, NamedType parent)
  {
    super (BT.LONG);
    this.SN = SN;
    this.parent = parent;
    vals = new ArrayList<String> ();
    scopelen = SN.toString ("_").length () - SN.getLeaf ().length ();
  }

  public Type dup ()
  {
    EnumType res = new EnumType (SN, parent);
    for (String s : vals)
    {
      res.addEnumerand (s);
    }
    return res;
  }

  public String getCType ()
  {
    return SN.toString ("_");
  }

  public void addEnumerand (String val)
  {
    vals.add (val);
  }

  public Iterable<String> getEnumerands ()
  {
    return vals;
  }

  private void getXMLbase (StringBuffer str)
  {
    int val = 0;
    str.append ("<Enum name=\\\"");
    str.append (SN.getLeaf ());
    str.append ("\\\">");
    for (String s : vals)
    {
      str.append ("<Element name=\\\"");
      str.append (s);
      str.append ("\\\" value=\\\"");
      str.append (Integer.toString (val++));
      str.append ("\\\"/>");
    }
    str.append ("</Enum>");
  }

  public void getXML (StringBuffer str, ModuleContext mod)
  {
    if (parent != null && mod.isParent (SN))
    {
      getXMLbase (str);
    }
    else
    {
      str.append ("<Type name=\\\"");
      str.append (mod.nameFrom (SN));
      str.append ("\\\"/>");
    }
  }

  public void getToplevelXML (StringBuffer str, ModuleContext mod)
  {
    mod.enter (str, SN);
    getXMLbase (str);
  }

  public void populateDeps (Set <ScopedName> depset, NamedType current)
  {
    if (parent == null)
    {
      depset.add (SN);
    }
    else
    {
      if (!(parent.getSN ().equals (current.getSN ())))
      {
        parent.populateDeps (depset, current);
      }
    }
  }

  public boolean depsOK (Set <ScopedName> deps)
  {
    return true;
  }

  public String descope (String s)
  {
    return s.substring (scopelen);
  }

  public ScopedName getSN ()
  {
    return SN;
  }

  private final NamedType parent;
  private final ScopedName SN;
  private final int scopelen;
  private List<String> vals;
}
