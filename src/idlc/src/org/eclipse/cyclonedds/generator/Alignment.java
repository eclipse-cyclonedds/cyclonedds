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

public enum Alignment
{
  ONE         (1, 0, "1u"),
  BOOL        (0, 0, "sizeof(bool)"),
  ONE_OR_BOOL (0, 1, "(sizeof(bool)>1u)?sizeof(bool):1u"),
  TWO         (2, 2, "2u"),
  TWO_OR_BOOL (0, 3, "(sizeof(bool)>2u)?sizeof(bool):2u"),
  FOUR        (4, 4, "4u"),
  PTR         (0, 6, "sizeof (char *)"),
  EIGHT       (8, 8, "8u");

  private final int value;
  private final int ordering;
  private final String rendering;

  Alignment (int val, int order, String render)
  {
    value = val;
    ordering = order;
    rendering = render;
  }

  public String toString ()
  {
    return rendering;
  }

  public Alignment maximum (Alignment rhs)
  {
    if (rhs.equals (BOOL))
    {
      if (this.equals (ONE))
      {
        return ONE_OR_BOOL;
      }
      else if (this.equals (TWO))
      {
        return TWO_OR_BOOL;
      }
      else
      {
        return this;
      }
    }
    if (rhs.ordering > ordering)
    {
      return rhs;
    }
    else
    {
      return this;
    }
  }

  public boolean isUncertain ()
  {
    return (this.value == 0);
  }

  public int getValue ()
  {
    return value;
  }
}
