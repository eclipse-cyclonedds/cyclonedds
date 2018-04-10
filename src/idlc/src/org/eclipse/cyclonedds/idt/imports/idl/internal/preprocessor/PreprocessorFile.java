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
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;


public class PreprocessorFile extends File
{
   /**
  * 
  */
 private static final long serialVersionUID = 1L;

private String originalPath;

   private File originalFile;

   private FileInputStream inputStr;

   private boolean reachedEOF = false;

   public PreprocessorFile (File workingDir, String originalPath)
   {
      super (workingDir, originalPath.replace ('\\', '/'));

      this.originalPath = originalPath.replace ('\\', '/');
      this.originalFile = new File (this.originalPath);
   }

   public String getOriginalPath ()
   {
      return originalPath;
   }

   public File getOriginalFile ()
   {
      return originalFile;
   }

   public InputStream getInputStream () throws FileNotFoundException
   {
      if (inputStr == null)
      {
         inputStr = new FileInputStream (this)
         {
            @Override
            public int read () throws IOException
            {
               int i = super.read ();
               if (reachedEOF)
               {
                  return -1;
               }
               else if (i == -1)
               {
                  reachedEOF = true;
                  return '\n';
               }
               else
               {
                  return i;
               }
            }
         };
      }
      return inputStr;
   }

   public void close ()
   {
      try
      {
         inputStr.close ();
      }
      catch (Exception e)
      {
      }
   }
}
