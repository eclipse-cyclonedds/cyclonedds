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
import java.util.Set;

public class TypeUtil
{
  public static boolean deptest
    (Type t, Set <ScopedName> deps, ScopedName parent)

  /* t - the Type we're testing for
   * deps - the types that have been written
   * parent - the containing type, if any
   */
  {
    if (!t.depsOK (deps))
    {
      return false;
    }

    if (t instanceof NamedType)
    {
      NamedType nt = (NamedType)t;
      ScopedName sn;
      if (parent != null)
      {
        sn = new ScopedName (nt.getSN ());
        sn.popComponent ();
        if (sn.equals (parent)) // It's inline
        {
          return true;
        }
      }
      sn = new ScopedName (nt.getSN ());

      // deps only has toplevel containing types, so we may need to go up
      do
      {
        if (deps.contains (sn))
        {
          return true;
        }
      } while (!sn.popComponent ().equals (""));
      return false;
    }
    else
    {
      return true;
    }
  }
}
