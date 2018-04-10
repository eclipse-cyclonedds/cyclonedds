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
package org.eclipse.cyclonedds.idt.imports.idl.internal.preprocessor;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import antlr.Token;
import antlr.TokenStreamRecognitionException;

import org.eclipse.cyclonedds.idt.imports.idl.preprocessor.IIdlPreprocessor;
import org.eclipse.cyclonedds.idt.imports.idl.preprocessor.IIdlPreprocessorParams;
import org.eclipse.cyclonedds.idt.imports.idl.preprocessor.IIdlPreprocessorStatus;


/**
 * Public interface to the C IIdlPreprocessor. This preprocessor will produce <em>almost</em> the
 * same output as running the {@link http://gcc.gnu.org/onlinedocs/cpp/ gcc preprocessor}. Details
 * of the output format can be found
 * {@link http://gcc.gnu.org/onlinedocs/cpp/IIdlPreprocessor-Output.html#Preprocessor-Output here}.
 * <p>
 * This preprocessor differs from gcc in the way it deals with blank lines. The gcc preprocessor
 * removes long sequences of blank lines and replaces them with an appropriate <em>line marker</em>,
 * whereas this preprocessor preserves all blank lines..
 * 
 * <pre><code>
 * File input = ..;
 * StringWriter output = new StringWriter ();
 * 
 * IIdlPreprocessor preprocessor = new IIdlPreprocessor ();
 * 
 * preprocessor.setWorkingDirectory (new File (&quot;/home/ted/project1&quot;);
 * preprocessor.addIncludeDirectory (new File (&quot;includes&quot;));
 * preprocessor.setStripComments (false);
 * preprocessor.setStripIncludes (true);
 * 
 * IStatus status = preprocessor.preprocess (input, output, null);
 * </code></pre>
 */
public class IdlPreprocessor implements IIdlPreprocessor
{
   public IIdlPreprocessorParams createPreprocessorParams ()
   {
      return new IdlPreprocessorParams ();
   }

   /**
    * Runs the preprocessor on the specified {@link File file} and sends the resulting output to the
    * specified {@link Writer writer}. It also populates the optional <code>dependencies</code>
    * map, if one is provided.
    * 
    * @param input
    *           The file to be preprocessed
    * @param output
    *           Where to put the preprocessed output
    * @param dependencies
    *           An optional Map containing a key for each file encountered during preprocessing.
    *           Each key (file) entry lists the files that it included.
    */

   public IIdlPreprocessorStatus preprocess (IIdlPreprocessorParams params, File input, Writer output, Map<File, List<File>> dependencies)
   {
      CppLexer mainLexer = null;
      try
      {
         PreprocessorFile toProcess;
         if (input.isAbsolute ())
         {
            toProcess = new PreprocessorFile (null, input.getPath ());
         }
         else
         {
            toProcess = new PreprocessorFile (params.getWorkingDirectory (), input.getPath ());
         }

         List<File> includes = new ArrayList<File> ();
         includes.addAll (params.getIncludeDirectories ());
         String path = input.getPath ().substring (0, input.getPath ().length () - input.getName ().length ());
         if (!path.equals (""))
         {
            includes.add (new File (path));
         }

         Map<String, List<String>> defines = new HashMap<String, List<String>> ();
         defines.putAll (params.getMacroDefinitions ());

         mainLexer = new CppLexer (toProcess, params.getWorkingDirectory (), dependencies, !params.getStripLineMarkers ());
         mainLexer.setIncludePath (includes);
         mainLexer.setDefines (defines);
         mainLexer.setProcessIncludes (!params.getStripIncludes ());
         mainLexer.setStripComments (params.getStripComments ());

         for (;;)
         {
            Token t = mainLexer.getNextToken ();
            if (t.getType () == Token.EOF_TYPE)
            {
               break;
            }
            output.write (t.getText ());
            output.flush ();
         }

         IIdlPreprocessorStatus status = mainLexer.getStatus ();
         return status;
      }
      catch (TokenStreamRecognitionException e)
      {
         return new PreprocessorStatus (e);
      }
      catch (FileNotFoundException e)
      {
         return new PreprocessorStatus (e, input);
      }
      catch (Exception e)
      {
         String message = e.getClass ().getSimpleName () + " caught during preprocessing";
         return new PreprocessorStatus (mainLexer, IIdlPreprocessorStatus.ERROR, message, e);
      }
      finally
      {
         if (mainLexer != null)
            mainLexer.close ();
      }
   }

   /**
    * An example command line driver. This is not really part of the public API
    */
   public static void main (String[] args)
   {
      try
      {
         File input = null;
         IdlPreprocessor preprocessor = new IdlPreprocessor ();
         IIdlPreprocessorParams params = new IdlPreprocessorParams ();
         Map<File, List<File>> dependencies = null;
         Writer output = new OutputStreamWriter (System.out);

         for (int i = 0; i < args.length;)
         {
            String arg = args[i++];

            if (arg.equals ("-I"))
            {
               if (i >= args.length)
               {
                  usage ();
                  System.exit (-1);
               }
               else
               {
                  params.addIncludeDirectory (new File (args[i++]));
               }
            }
            else if (arg.equals ("-O"))
            {
               if (i >= args.length)
               {
                  usage ();
                  System.exit (-1);
               }
               else
               {
                  output = new FileWriter (args[i++]);
               }
            }
            else if (arg.equals ("-D"))
            {
               if (i >= args.length)
               {
                  usage ();
                  System.exit (-1);
               }
               else
               {
                  arg = args[i++];
                  int equals = arg.indexOf ('=');

                  if (equals == -1)
                  {
                     params.addMacroDefinition (arg);
                  }
                  else
                  {
                     String name = arg.substring (0, equals);
                     String value = arg.substring (equals + 1);

                     params.addMacroDefinition (name, value);
                  }
               }
            }
            else if (arg.equals ("-U"))
            {
               if (i >= args.length)
               {
                  usage ();
                  System.exit (-1);
               }
               else
               {
                  params.removeMacroDefinition (args[i++]);
               }
            }
            else if (arg.equals ("-B"))
            {
               if (i >= args.length)
               {
                  usage ();
                  System.exit (-1);
               }
               else
               {
                  params.setWorkingDirectory (new File (args[i++]));
               }
            }
            else if (arg.equals ("-M"))
            {
               dependencies = new HashMap<File, List<File>> ();
            }
            else if (arg.equals ("--LeaveComments"))
            {
               params.setStripComments (false);
            }
            else if (arg.equals ("--NoFileInline"))
            {
               params.setStripIncludes (true);
            }
            else if (arg.equals ("--NoLineMarkers"))
            {
               params.setStripLineMarkers (true);
            }
            else
            {
               if (i < args.length)
               {
                  usage ();
                  System.exit (-1);
               }
               else
               {
                  input = new File (arg);
               }
            }
         }

         if (input == null)
         {
            usage ();
            System.exit (-1);
         }

         IIdlPreprocessorStatus status = preprocessor.preprocess (params, input, output, dependencies);
         if (status.getChildStati ().length == 0)
            System.err.println (status);
         for (IIdlPreprocessorStatus child : status.getChildStati ())
         {
            System.err.println (child);
         }

         if (dependencies != null)
         {
            for (File file : dependencies.keySet ())
            {
               System.out.println ();
               System.out.println ("   " + file.getPath () + " :");

               for (File included : dependencies.get (file))
               {
                  System.out.println ("      " + included.getPath ());
               }
            }
         }
      }
      catch (Exception e)
      {
         e.printStackTrace ();
      }
   }

   private static void usage ()
   {
      System.err
            .println ("USAGE : "
                  + IdlPreprocessor.class.getName ()
                  + " [-I <dir>] [-B <working dir>] [-O <output filename] [--LeaveComments] [--NoFileInline] [--NoLineMarkers] [-D <name>[=<value>]] [-U <name>] <filename>");
   }
}
