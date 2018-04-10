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

public class BasicTypedefType extends BasicType implements NamedType
{
  public BasicTypedefType (ScopedName name, BasicType ref)
  {
    super (ref.type);
    this.name = new ScopedName (name);
  }

  public Type dup ()
  {
    return new TypedefType (name, new BasicType (type));
  }

  public void getToplevelXML (StringBuffer str, ModuleContext mod)
  {
    mod.enter (str, name);
    str.append ("<TypeDef name=\\\"");
    str.append (name.getLeaf ());
    str.append ("\\\">");
    super.getXML (str, mod);
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
    depset.add (name);
  }

  public boolean depsOK (Set <ScopedName> deps)
  {
    return true;
  }

  public ScopedName getSN ()
  {
    return name;
  }

  public boolean isInline ()
  {
    return false;
  }

  private final ScopedName name;
}
