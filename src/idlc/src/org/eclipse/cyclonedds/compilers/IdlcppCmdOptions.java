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

public class IdlcppCmdOptions extends CmdOptions
{
  public IdlcppCmdOptions (String[] args) throws CmdException
  {
    super ("dds_idlcpp", args);
  }

  public void optHelp (java.io.PrintStream io)
  {
    super.optHelp (io);
    io.println ("   -l [isocpp|cpp]  Select ISO C++ or classic generation");
    io.println ("   -E               Preprocess only, to standard output");
    io.println ("   -nostamp         Do not timestamp generated code");
    io.println ("   -quiet           Suppress console output other than error messages");
  }

  public boolean process (String arg1, String arg2) throws CmdException
  {
    boolean result = false;
    if (arg1.equals ("-E"))
    {
      pponly = true;
    }
    else if (arg1.equals ("-nostamp"))
    {
      nostamp = true;
    }
    else if (arg1.equals ("-quiet") || arg1.equals ("-q"))
    {
      quiet = true;
    }
    else if (arg1.equals ("-T"))
    {
      testmethods = true;
    }
    else if (arg1.equals ("-l"))
    {
      if (arg2 == null || arg2.charAt (0) == '-')
      {
        System.err.println
          (name + ": target language expected following -l option");
        throw new CmdException (1);
      }
      if (arg2.equals ("cpp") || arg2.equals ("c++"))
      {
        language = "c++";
      }
      else if (arg2.equals ("isocpp") || arg2.equals ("isoc++"))
      {
        language = "isoc++";
      }
      else
      {
        System.err.println (name + ": supported target languages are isocpp (isoc++) and cpp (c++)");
        throw new CmdException (1);
      }
      result = true;
    }
    else
    {
      return super.process (arg1, arg2);
    }
    return result;
  }

  public boolean pponly;
  public boolean nostamp;
  public boolean quiet;
  public String language;
  public boolean testmethods;
}
