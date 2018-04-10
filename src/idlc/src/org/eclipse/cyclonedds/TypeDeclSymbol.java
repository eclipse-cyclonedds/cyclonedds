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

import java.util.List;

import org.eclipse.cyclonedds.parser.IDLParser;

public class TypeDeclSymbol extends TypeDefSymbol
{
  public TypeDeclSymbol
  (
    ScopedName name,
    IDLParser.Type_specContext definition,
    List<IDLParser.Fixed_array_sizeContext> dimensions,
    boolean isint,
    boolean isnonint
  )
  {
    super (name);
    def = definition.getText ();
    this.isint = isint;
    this.isnonint = isnonint;
  }

  public String toString ()
  {
    return "Typedef = " + def;
  }

  public boolean isInteger ()
  {
    return isint;
  }
  public boolean isNonintConst ()
  {
    return isnonint;
  }

  private String def;
  private boolean isint;
  private boolean isnonint;
}
