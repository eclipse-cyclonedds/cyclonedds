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

public class TypedefType extends AbstractType implements NamedType
{
  public TypedefType (ScopedName name, Type ref)
  {
    this.ref = ref.dup ();
    this.name = new ScopedName (name);
  }

  public Type dup ()
  {
    return new TypedefType (name, ref);
  }

  public ArrayList <String> getMetaOp (String myname, String structname)
  {
    return ref.getMetaOp (myname, structname);
  }

  public String getSubOp ()
  {
    return ref.getSubOp ();
  }

  public String getOp ()
  {
    return ref.getOp ();
  }

  public String getCType ()
  {
    return ref.getCType ();
  }

  public long getKeySize ()
  {
    return ref.getKeySize ();
  }

  public int getMetaOpSize ()
  {
    return ref.getMetaOpSize ();
  }

  public Alignment getAlignment ()
  {
    return ref.getAlignment ();
  }

  public void getToplevelXML (StringBuffer str, ModuleContext mod)
  {
    mod.enter (str, name);
    str.append ("<TypeDef name=\\\"");
    str.append (name.getLeaf ());
    str.append ("\\\">");
    ref.getXML (str, mod);
    str.append ("</TypeDef>");
  }

  public void getXML (StringBuffer str, ModuleContext mod)
  {
    str.append ("<Type name=\\\"");
    str.append (mod.nameFrom (name));
    str.append ("\\\"/>");
  }

  public void populateDeps (Set <ScopedName> depset, NamedType current)
  {
    ref.populateDeps (depset, this);
    depset.add (name);
  }

  public boolean depsOK (Set <ScopedName> deps)
  {
    return TypeUtil.deptest (ref, deps, name);
  }

  public ScopedName getSN ()
  {
    return name;
  }

  public Type getRef ()
  {
    return ref;
  }

  private final Type ref;
  private final ScopedName name;
}
