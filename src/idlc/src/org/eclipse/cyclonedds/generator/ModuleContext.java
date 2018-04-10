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

public class ModuleContext
{
  public ModuleContext ()
  {
    current = new ScopedName ();
    struct = new ScopedName ();
  }

  public void enter (StringBuffer str, ScopedName target)
  {
    ScopedName common = new ScopedName (target);
    common.popComponent ();
    while (common.depth () > current.depth ())
    {
      common.popComponent ();
    }
    while (common.depth () < current.depth ())
    {
      current.popComponent ();
      str.append ("</Module>");
    }
    while (!common.equals (current))
    {
      current.popComponent ();
      common.popComponent ();
      str.append ("</Module>");
    }
    String[] submods = target.getComponents ();
    for (int i = current.depth (); i < submods.length - 1; i++)
    {
      current.addComponent (submods [i]);
      str.append ("<Module name=\\\"");
      str.append (submods [i]);
      str.append ("\\\">");
    }
  }

  public void exit (StringBuffer str)
  {
    for (int i = 0; i < current.depth (); i++)
    {
      str.append ("</Module>");
    }
    current.reset ();
  }

  public void pushStruct (String s)
  {
    struct.addComponent (s);
  }

  public void popStruct ()
  {
    struct.popComponent ();
  }

  public boolean isParent (ScopedName name)
  {
    ScopedName dup = new ScopedName (name);
    dup.popComponent ();
    return dup.equals (current.catenate (struct));
  }

  public String nameFrom (ScopedName to)
  {
    String[] toArr = to.getComponents ();
    String[] fromArr = current.getComponents ();

    if (toArr.length == fromArr.length + 1)
    {
      boolean same = true;
      for (int i = 0; i < fromArr.length; i++)
      {
        if (!toArr[i].equals (fromArr[i]))
        {
          same = false;
          break;
        }
      }
      if (same)
      {
        return to.getLeaf ();
      }
    }

    if (fromArr.length > 0 && toArr.length > 0 && fromArr[0].equals (toArr[0]))
    {
      return to.toString ("::");
    }
    else
    {
      return "::" + to.toString ("::");
    }
  }

  public ScopedName getScope ()
  {
    return new ScopedName (current);
  }

  private ScopedName current;
  private ScopedName struct;
}

