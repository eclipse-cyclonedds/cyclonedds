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

public interface Type
{
  public ArrayList <String> getMetaOp (String myname, String structname);
  public String getSubOp ();
  public String getOp ();
  public String getCType ();
  public void getXML (StringBuffer str, ModuleContext mod);
  public void populateDeps (Set <ScopedName> depset, NamedType current);
  public boolean depsOK (Set <ScopedName> deps);
  public void makeKeyField ();
  public boolean isKeyField ();
  public long getKeySize ();
  public int getMetaOpSize ();
  public Alignment getAlignment ();
  public Type dup ();
}

