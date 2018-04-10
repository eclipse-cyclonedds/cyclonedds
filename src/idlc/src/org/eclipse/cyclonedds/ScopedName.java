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

public class ScopedName implements Comparable <ScopedName>
{
  public ScopedName ()
  {
    components = new ArrayList<String> ();
  }

  public ScopedName (ScopedName rhs)
  {
    components = new ArrayList <String> (rhs.components.size ());
    for (String s : rhs.components)
    {
      components.add (s);
    }
  }

  public ScopedName (String[] components)
  {
    this.components = new ArrayList<String> (components.length);
    for (String s : components)
    {
      if (!s.equals (""))
      {
        this.components.add (s);
      }
    }
  }

  public ScopedName (List <String> components)
  {
    this.components = new ArrayList<String> (components);
  }

  public ScopedName (String colonscopedname)
  {
    this (colonscopedname.split ("::"));
  }

  public void addComponent (String x)
  {
    components.add (x);
  }

  public String popComponent ()
  {
    if (components.isEmpty ())
    {
      return "";
    }
    else
    {
      return components.remove (components.size () - 1);
    }
  }

  public String getLeaf ()
  {
    if (components.isEmpty ())
    {
      return "";
    }
    else
    {
      return components.get (components.size () - 1);
    }
  }

  public String[] getPath ()
  {
    String[] result = new String[0];

    if (components.size () >= 2)
    {
      result = components.subList (0, components.size () - 1).toArray (result);
    }
    return result;
  }

  public String[] getComponents ()
  {
    return components.toArray (new String[0]);
  }

  public String toString (String scoper)
  {
    boolean first = true;
    StringBuffer result = new StringBuffer ("");
    for (String element: components)
    {
      if (first)
      {
        first = false;
      }
      else
      {
        result.append (scoper);
      }
      result.append (element);
    }
    return result.toString ();
  }

  public String toString ()
  {
    return toString ("::");
  }

  public ScopedName catenate (ScopedName more)
  {
    ScopedName result = new ScopedName ();
    result.components.addAll (components);
    result.components.addAll (more.components);
    return result;
  }

  public int compareTo (ScopedName rhs)
  {
    int result = 0, pos = 0;
    int ldepth = depth ();
    int rdepth = rhs.depth ();
    while (result == 0 && pos < ldepth && pos < rdepth)
    {
      result = components.get (pos).compareTo (rhs.components.get (pos));
      pos++;
    }
    if (result == 0)
    {
      result = Integer.compare (ldepth, rdepth);
    }
    return result;
  }

  public static Comparator<ScopedName> osplComparator
    = new Comparator<ScopedName> ()
  {
    public int compare(ScopedName lhs, ScopedName rhs)
    {
      int ldepth = lhs.depth ();
      int rdepth = rhs.depth ();
      if (ldepth == 0 || rdepth == 0)
      {
        return Integer.compare (ldepth, rdepth);
      }
      else
      {
        int result = 0, pos = 0;
        while (result == 0 && pos < ldepth && pos < rdepth)
        {
          result =
            lhs.components.get (pos).compareTo (rhs.components.get (pos));
          pos++;
        }
        if (result == 0)
        {
          result = Integer.compare (rdepth, ldepth);
        }
        return result;
      }
    }
  };

  public boolean isParentOf (ScopedName other)
  {
    if (other.components.size() <= components.size ())
    {
      return false;
    }
    for (int i = 0; i < components.size (); i++)
    {
      if (!components.get (i).equals (other.components.get (i)))
      {
        return false;
      }
    }
    return true;
  }

  public boolean equals (Object o)
  {
    return (o instanceof ScopedName && compareTo ((ScopedName)o) == 0);
  }

  public int hashCode ()
  {
    int result = 0;
    for (String c : components)
    {
      result ^= c.hashCode ();
    }
    return result;
  }

  public int depth ()
  {
    return components.size ();
  }

  public void reset ()
  {
    components.clear ();
  }

  private ArrayList<String> components;
}

