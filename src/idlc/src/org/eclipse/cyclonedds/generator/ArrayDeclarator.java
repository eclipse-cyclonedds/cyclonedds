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

public class ArrayDeclarator
{
  public ArrayDeclarator (String name)
  {
    this.name = name;
    this.dims = new ArrayList <Long> ();
    size = 1;
  }

  public void addDimension (long dim)
  {
    dims.add (new Long (dim));
    size *= dim;
  }

  public String getName ()
  {
    return name;
  }

  public long getSize ()
  {
    return size;
  }

  public String getDimString ()
  {
    StringBuffer result = new StringBuffer ();
    for (Long dim : dims)
    {
      result.append ("[");
      result.append (dim.toString ());
      result.append ("]");
    }
    return result.toString ();
  }

  public Collection <Long> getDims ()
  {
    return dims;
  }

  private final String name;
  private List<Long> dims;
  private long size;
}
