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
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.cyclonedds.idt.imports.idl.preprocessor.IIdlPreprocessorParams;


public class IdlPreprocessorParams implements IIdlPreprocessorParams
{
   private List<File> includeDirectories = new ArrayList<File> ();

   private boolean stripComments = true;

   private boolean stripIncludes = false;

   private boolean stripLineMarkers = false;

   private File workingDirectory = new File (System.getProperty ("user.dir"));

   private Map<String, List<String>> defines = new HashMap<String, List<String>> ();

   public void addIncludeDirectory (File directory) throws FileNotFoundException
   {
      if (directory != null && directory.exists () && directory.isDirectory ())
      {
         includeDirectories.add (directory);
      }
   }

   public void addMacroDefinition (String name)
   {
      List<String> args = new ArrayList<String> ();
      args.add ("");
      defines.put (name, args);
   }

   public void addMacroDefinition (String name, String value)
   {
      List<String> args = new ArrayList<String> ();
      args.add (value);
      defines.put (name, args);
   }

   public List<File> getIncludeDirectories ()
   {
      return includeDirectories;
   }

   public Map<String, List<String>> getMacroDefinitions ()
   {
      return defines;
   }

   public boolean getStripComments ()
   {
      return stripComments;
   }

   public boolean getStripIncludes ()
   {
      return stripIncludes;
   }

   public boolean getStripLineMarkers ()
   {
      return stripLineMarkers;
   }

   public File getWorkingDirectory ()
   {
      return workingDirectory;
   }

   public void removeIncludeDirectory (File directory)
   {
      includeDirectories.remove (directory);
   }

   public void removeMacroDefinition (String name)
   {
      defines.remove (name);
   }

   public void setStripComments (boolean stripComments)
   {
      this.stripComments = stripComments;
   }

   public void setStripIncludes (boolean stripIncludes)
   {
      this.stripIncludes = stripIncludes;
   }

   public void setStripLineMarkers (boolean stripLineMarkers)
   {
      this.stripLineMarkers = stripLineMarkers;
   }

   public void setWorkingDirectory (File directory) throws FileNotFoundException
   {
      if (directory != null && directory.exists () && directory.isDirectory ())
      {
         this.workingDirectory = directory;
      }
      else
      {
         throw new FileNotFoundException ("Can't find directory " + directory.getPath ());
      }
   }

}
