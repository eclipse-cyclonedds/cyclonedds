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

import org.eclipse.cyclonedds.compilers.IdlcCmdOptions;

public class IdlParams
{
  public IdlParams (IdlcCmdOptions opts)
  {
    timestamp = !opts.nostamp;
    quiet = opts.quiet;
    lax = opts.lax;
    mapwide = opts.mapwide;
    mapld = opts.mapld;
    forcpp = opts.forcpp;
    dllname = opts.dllname;
    dllfile = opts.dllfile;
    xmlgen = !opts.noxml;
    allstructs = opts.allstructs;
    notopics = opts.notopics;
  }

  public boolean timestamp;
  public boolean quiet;
  public boolean lax;
  public boolean mapwide;
  public boolean mapld;
  public boolean forcpp;
  public boolean xmlgen;
  public boolean allstructs;
  public boolean notopics;
  public String dllname;
  public String dllfile;
  public String basename = null;
  public SymbolTable symtab = null;
  public LineTable linetab = null;
}

