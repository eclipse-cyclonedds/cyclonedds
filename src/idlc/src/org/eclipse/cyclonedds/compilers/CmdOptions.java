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

import java.util.*;

public class CmdOptions
{
  public CmdOptions (String compilername, String[] args) throws CmdException
  {
    this.name = compilername;
    files = new ArrayList <String> ();
    includes = new ArrayList <String> ();
    macros = new HashMap <String, String> ();

    int i = 0;
    while (i < args.length && args[i].startsWith ("-"))
    {
      if (process (args[i], ((i + 1 < args.length) ? args[i + 1] : null)))
      {
        i++;
      }
      i++;
    }
    while (i < args.length)
    {
      files.add (args[i++]);
    }
  }

  public CmdOptions (CmdOptions rhs)
  {
    dllname = rhs.dllname;
    dllfile = rhs.dllfile;
    outputdir = rhs.outputdir;
    includes = new ArrayList <String> (rhs.includes);
    macros = new HashMap <String, String> (rhs.macros);
    version = rhs.version;
    files = new ArrayList <String> (rhs.files);
    name = rhs.name;
    noxml = rhs.noxml;
  }

  public void optHelp (java.io.PrintStream io)
  {
    io.println ("Usage: " + name + " [options] file(s).idl");
    io.println ();
    io.println (" options:");
    io.println ("   -help            This help screen");
    io.println ("   -version         Print compiler version");
    io.println ("   -d directory     Output directory for generated files");
    io.println ("   -I path          Add directory to #include search path");
    io.println ("   -D macro         Define conditional compilation symbol");
    io.println ("   -dll name[,file] Generate DLL linkage declarations");
    io.println ("   -noxml           Do not generate XML Topic descriptors");
  }

  public boolean process (String arg1, String arg2) throws CmdException
  {
    boolean result = false; // whether or not we used arg2
    if (arg1.equals ("-h") || arg1.equals ("-?") || arg1.equals ("-help"))
    {
      optHelp (System.out);
      throw new CmdException (0);
    }
    else if (arg1.equals ("-v") || arg1.equals ("-version"))
    {
      version = true;
    }
    else if (arg1.equals ("-d"))
    {
      if (arg2 == null || arg2.charAt (0) == '-')
      {
        System.err.println
          (name + ": Directory name expected following -d option");
        throw new CmdException (1);
      }
      outputdir = arg2;
      result = true;
    }
    else if (arg1.startsWith ("-dll") || arg1.startsWith ("-P"))
    {
      if (arg1.equals ("-dll") || arg1.equals ("-P"))
      {
        if (arg2 == null || arg2.charAt (0) == '-')
        {
          System.err.println
            (name + ": DLL name expected following " + arg1 + " option");
          throw new CmdException (1);
        }
        processDll (arg2);
        result = true;
      }
      else
      {
        if (arg1.startsWith ("-dll"))
        {
          processDll (arg2.substring (4));
        }
        else
        {
          processDll (arg2.substring (2));
        }
      }
    }
    else if (arg1.equals ("-noxml"))
    {
      noxml = true;
    }
    else if (arg1.startsWith ("-I"))
    {
      if (arg1.equals ("-I"))
      {
        if (arg2 == null || arg2.charAt (0) == '-')
        {
          System.err.println (name + ": Include search path directory expected following -I option");
          throw new CmdException (1);
        }
        includes.add (arg2);
        result = true;
      }
      else
      {
        includes.add (arg1.substring (2));
      }
    }
    else if (arg1.startsWith ("-D"))
    {
      if (arg1.equals ("-D"))
      {
        if (arg2 == null || arg2.charAt (0) == '-')
        {
          System.err.println (name + ": Conditional compilation identifier expected following -D option");
          throw new CmdException (1);
        }
        addMacro (arg2);
        result = true;
      }
      else
      {
        addMacro (arg1.substring (2));
      }
    }
    else
    {
      optHelp (System.out);
      throw new CmdException (1);
    }
    return result;
  }

  private void addMacro (String m)
  {
    int pos = m.indexOf ("=");
    if (pos > 0)
    {
      macros.put (m.substring (0, pos), m.substring (pos + 1));
    }
    else
    {
      macros.put (m, "");
    }
  }

  private void processDll (String arg)
  {
    int pos = arg.indexOf (",");
    if (pos > 0)
    {
      dllname = arg.substring (0, pos);
      dllfile = arg.substring (pos + 1);
    }
    else
    {
      dllname = arg;
    }
  }

  public String dllname = null;
  public String dllfile = null;
  public String outputdir = null;
  public boolean noxml;
  public List <String> includes;
  public Map <String, String> macros;
  public boolean version = false;
  public List <String> files;
  final String name;
}

