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
package org.eclipse.cyclonedds.idt.imports.idl.preprocessor;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.List;
import java.util.Map;


/**
 * FIXME: Document it!
 */
public interface IIdlPreprocessorParams
{
   /**
    * Sets the <em>working directory</em> for this preprocessor. Any relative paths, such as the
    * input file, or include directories, are resolved relative to this directory. The default value
    * is taken as the value of the <code>user.dir</code> system property.
    * <p>
    * Setting this property on the preprocessor is equivalent to changing the current working
    * directory prior to running it.
    * 
    * @param directory
    *           The <em>workig directory</em> for this preprocessor.
    * @throws FileNotFoundException
    * @see System#getProperties()
    * @see #getWorkingDirectory()
    */
   public void setWorkingDirectory (File directory) throws FileNotFoundException;

   /**
    * Returns the preprocessors current <em>working directory</em>.
    * 
    * @return File
    * @see #setWorkingDirectory(File)
    */
   public File getWorkingDirectory ();

   /**
    * Determines whether to preserve comments in the preprocessed output. The default value is
    * <code>true</code>, meaning that comments will be stripped and therefore not present in the
    * output.
    * 
    * @param strip
    */
   public void setStripComments (boolean strip);

   /**
    * @return boolean
    * @see #setStripComments(boolean)
    */
   public boolean getStripComments ();

   /**
    * Controls whether the content of included files is passed through to the preprocessed output.
    * The default value is <code>false</code>, meaning that all content from the primary,
    * <em>and</em> included files is passed through to the preprocessed output. If set to
    * <code>true</code>, only the content from the primary file will be present in the
    * preprocessed output. In both cases, full preprocessing takes place. This option only controls
    * what is present in the output.
    * 
    * @param strip
    */
   public void setStripIncludes (boolean strip);

   /**
    * @return boolean
    * @see #setStripIncludes(boolean)
    */
   public boolean getStripIncludes ();

   /**
    * Controls whether <em>line markers</em> are produced in the output of this preprocessor. Line
    * markers are additional lines, introduced into the output, which look like this:
    * <p> # &lt;line&gt; &lt;filename&gt; &lt;flags&gt;
    * <p>
    * These lines are inserted as needed into the output (but never within a string or character
    * constant). They mean that the following line originated in file &lt;filename&gt; at line
    * &lt;line&gt;. &lt;filename&gt; will never contain any non-printing characters; they are
    * replaced with octal escape sequences.
    * 
    * @param strip
    * @see #getStripLineMarkers()
    */
   public void setStripLineMarkers (boolean strip);

   /**
    * @return boolean
    * @see #setStripLineMarkers(boolean)
    */
   public boolean getStripLineMarkers ();

   /**
    * Adds the specified <code>directory</code> to the include path (like the
    * <code>-I&lt;directory&gt;</code> flag). If the specified <code>directory</code> is
    * {@link File#isAbsolute() relative} then it will be interpreted relative to this preprocessors
    * current {@link #setWorkingDirectory(File) working directory}.
    * 
    * @param directory
    * @throws FileNotFoundException
    * @see #setWorkingDirectory(File)
    */
   public void addIncludeDirectory (File directory) throws FileNotFoundException;

   /**
    * Removes the specified <code>directory</code> from the include path.
    * 
    * @param directory
    */
   public void removeIncludeDirectory (File directory);

   /**
    * Returns the list of include directories that make up the include path
    * 
    * @return List
    */
   public List<File> getIncludeDirectories ();

   /**
    * Predefine the specified <code>name</code> as a macro, with the value <code>1</code>.
    * 
    * @param name
    */
   public void addMacroDefinition (String name);

   /**
    * Predefine the specified <code>name</code> as a macro, with the specified <code>value</code>
    * 
    * @param name
    * @param value
    */
   public void addMacroDefinition (String name, String value);

   /**
    * Cancel any previous definition of <code>name</code>, either built in or provided with
    * {@link #addMacroDefinition(String)}
    * 
    * @param name
    */
   public void removeMacroDefinition (String name);

   /**
    * Returns the list of macro definitions for this preprocessor
    * 
    * @return Map
    */
   public Map<String, List<String>> getMacroDefinitions ();
}
