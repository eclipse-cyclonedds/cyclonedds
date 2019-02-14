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
package org.eclipse.cyclonedds.compilers;

public class IdlcCmdOptions extends CmdOptions
{
  public IdlcCmdOptions (String[] args) throws CmdException
  {
    super ("dds_idlc", args);
  }

  public IdlcCmdOptions (IdlcppCmdOptions cppopts)
  {
    super (cppopts);
    forcpp = true;
    pponly = cppopts.pponly;
    quiet = cppopts.quiet;
    nostamp = cppopts.nostamp;
  }

  public void optHelp (java.io.PrintStream io)
  {
    super.optHelp (io);
    io.println ("   -E               Preprocess only, to standard output");
    io.println ("   -allstructs      All structs are Topics");
    io.println ("   -notopics        Generate type definitions only");
    io.println ("   -nostamp         Do not timestamp generated code");
    io.println ("   -lax             Skip over structs containing unsupported datatypes");
    io.println ("   -quiet           Suppress console output other than error messages (default)");
    io.println ("   -verbose         Enable console ouptut other than error messages");
    io.println ("   -map_wide        Map the unsupported wchar and wstring types to char and string");
    io.println ("   -map_longdouble  Map the unsupported long double type to double");
  }

  public boolean process (String arg1, String arg2) throws CmdException
  {
    if (arg1.equals ("-E"))
    {
      pponly = true;
    }
    else if (arg1.equals ("-allstructs"))
    {
      if (notopics)
      {
        System.err.println (name + ": -allstructs and -notopics are mutually exclusive options");
        throw new CmdException (1);
      }
      allstructs = true;
    }
    else if (arg1.equals ("-notopics"))
    {
      if (allstructs)
      {
        System.err.println (name + ": -allstructs and -notopics are mutually exclusive options");
        throw new CmdException (1);
      }
      notopics = true;
    }
    else if (arg1.equals ("-nostamp"))
    {
      nostamp = true;
    }
    else if (arg1.equals ("-quiet") || arg1.equals ("-q"))
    {
      quiet = true;
    }
    else if (arg1.equals ("-verbose") || arg1.equals ("-v"))
    {
      quiet = false;
    }
    else if (arg1.equals ("-lax"))
    {
      lax = true;
    }
    else if (arg1.equals ("-map_wide"))
    {
      mapwide = true;
    }
    else if (arg1.equals ("-map_longdouble"))
    {
      mapld = true;
    }
    else if (arg1.equals ("-dumptokens"))
    {
      dumptokens = true;
    }
    else if (arg1.equals ("-dumptree"))
    {
      dumptree = true;
    }
    else if (arg1.equals ("-dumpsymbols"))
    {
      dumpsymbols = true;
    }
    else if (arg1.equals ("-forcpp"))
    {
      forcpp = true;
    }
    else
    {
      return super.process (arg1, arg2);
    }
    return false;
  }

  public boolean pponly;
  public boolean allstructs;
  public boolean notopics;
  public boolean nostamp;
  public boolean quiet         = true;
  public boolean lax;
  public boolean mapwide;
  public boolean mapld;
  public boolean dumptokens;
  public boolean dumptree;
  public boolean dumpsymbols;
  public boolean forcpp;
}

