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
import org.eclipse.cyclonedds.Project;

public class Idlcpp
{
  public static void main (String[] args)
  {
    IdlcppCmdOptions opts = null;
    int status = 0;

    try
    {
      opts = new IdlcppCmdOptions (args);
    }
    catch (CmdException ex)
    {
      System.exit (ex.retcode);
    }

    List <String> idlppcmd = new ArrayList <String> ();

    if (opts.version)
    {
      System.out.print ("Eclipse Cyclone DDS ");
      System.out.println ("IDL to C++ compiler v" + Project.version);
    }
    else
    {
      String FS = System.getProperty ("file.separator");
      String projecthome = System.getProperty (Project.nameCaps + "_HOME");
      String projecthost = System.getProperty (Project.nameCaps + "_HOST");

      opts.includes.add (projecthome + FS + "etc" + FS + "idl");

      IdlcCmdOptions idlcopts = new IdlcCmdOptions (opts);

      if (!opts.pponly)
      {
         idlppcmd.add (projecthome + FS + "bin" + FS + projecthost + FS + "idlpp");
         idlppcmd.add ("-S");
         idlppcmd.add ("-a");
         idlppcmd.add (projecthome + FS + "etc" + FS + "idlpp");
         idlppcmd.add ("-x");
         idlppcmd.add ("cyclone");

         idlppcmd.add ("-l");
         if (opts.language == null)
         {
           idlppcmd.add ("isoc++");
         }
         else
         {
           idlppcmd.add (opts.language);
         }

         if (opts.dllname != null)
         {
           idlppcmd.add ("-P");
           if (opts.dllfile != null)
           {
             idlppcmd.add (opts.dllname + "," + opts.dllfile);
           }
           else
           {
             idlppcmd.add (opts.dllname);
           }
         }
         if (opts.outputdir != null)
         {
           idlppcmd.add ("-d");
           idlppcmd.add (opts.outputdir);
         }
         for (String s : opts.includes)
         {
           idlppcmd.add ("-I");
           idlppcmd.add (s);
         }
         for (String s : opts.macros.keySet ())
         {
           idlppcmd.add ("-D");
           String val = opts.macros.get (s);
           if (!val.equals (""))
           {
             idlppcmd.add (s + "=" + val);
           }
           else
           {
             idlppcmd.add (s);
           }
         }

         if (opts.testmethods)
         {
           idlppcmd.add ("-T");
         }

         idlppcmd.addAll (opts.files);

         status = runcmd (idlppcmd);
      }
      org.eclipse.cyclonedds.Compiler.run (idlcopts);
    }
    System.exit (status);
  }

  private static int runcmd (List<String> cmdline)
  {
    int result;
    try
    {
      result = new ProcessBuilder (cmdline).inheritIO ().start (). waitFor ();
      if (result != 0)
      {
        System.err.print ("dds_idlcpp: nonzero return from");
        for (String s : cmdline)
        {
          System.err.print (" " + s);
        }
        System.err.println ();
      }
    }
    catch (Exception ex)
    {
      System.err.print ("dds_idlcpp: exception when running");
      for (String s : cmdline)
      {
        System.err.print (" " + s);
      }
      System.err.println ();
      System.err.println (ex);
      result = 1;
    }
    return result;
  }
}

