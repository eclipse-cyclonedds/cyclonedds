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

public class StructSymbol extends TypeDefSymbol
{
  public StructSymbol (ScopedName name)
  {
    super (name);
    members = new ArrayList <String> ();
    valid = true;
  }

  public String toString ()
  {
    StringBuffer result = new StringBuffer ("Struct, members are");
    for (String s : members)
    {
      result.append (' ');
      result.append (s);
    }
    return result.toString ();
  }

  public void addMember (String membername)
  {
    members.add (membername);
  }

  public void addStructMember (String membername, StructSymbol membertype)
  {
    for (String s : membertype.members)
    {
      members.add (membername + "." + s);
    }
  }

  public boolean hasMember (String membername)
  {
    return members.contains (membername);
  }

  public void invalidate ()
  {
    valid = false;
  }

  public boolean isValid ()
  {
    return valid;
  }

  private List <String> members;
  private boolean valid;
}
