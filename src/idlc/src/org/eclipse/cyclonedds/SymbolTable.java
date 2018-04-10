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
package org.eclipse.cyclonedds;

import java.util.*;

public class SymbolTable
{
  public SymbolTable()
  {
    map = new HashMap <ScopedName, Symbol> ();
  }

  public void add (Symbol newsym)
  {
    Symbol oldval;
    oldval = map.put (newsym.name (), newsym);
    if (oldval != null)
    {
      System.err.println ("Internal inconsistency, multiple definition for " + newsym.name ());
      System.err.println ("Old value was " + oldval.toString ());
      System.err.println ("New value is " + newsym.toString ());
    }
  }

  public Symbol resolve (ScopedName current, ScopedName request)
  {
    Symbol result = null;
    if (current != null)
    {
      ScopedName searchscope = new ScopedName (current);
      do
      {
        result = map.get (searchscope.catenate (request));
      }
      while (result == null && ! searchscope.popComponent().equals (""));
    }
    if (result == null)
    {
      result = map.get (request);
    }
    return result;
  }

  public Symbol getSymbol (ScopedName request)
  {
    return map.get (request);
  }

  public void dump ()
  {
    for (Map.Entry <ScopedName, Symbol> sym : map.entrySet ())
    {
      System.out.println (sym.getKey () + " is a " + sym.getValue ());
    }
  }

  Map <ScopedName, Symbol> map;
}

