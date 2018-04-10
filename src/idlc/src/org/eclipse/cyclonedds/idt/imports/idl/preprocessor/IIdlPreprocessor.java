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
import java.io.Writer;
import java.util.List;
import java.util.Map;


/**
 * Public interface to the C Preprocessor. This preprocessor will produce <em>almost</em> the same
 * output as running the {@link http://gcc.gnu.org/onlinedocs/cpp/ gcc preprocessor}. Details of
 * the output format can be found
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
 * IIdlPreprocessor preprocessor = IdlPreprocessorFactory.create ();
 * 
 * preprocessor.setWorkingDirectory (new File (&quot;/home/ted/project1&quot;);
 * preprocessor.addIncludeDirectory (new File (&quot;includes&quot;));
 * preprocessor.setStripComments (false);
 * preprocessor.setStripIncludes (true);
 * 
 * IStatus status = preprocessor.preprocess (input, output, null);
 * </code></pre>
 */
public interface IIdlPreprocessor
{
   static final String PLUGIN_ID = IIdlPreprocessor.class.getPackage ().getName ();

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
   public IIdlPreprocessorStatus preprocess (IIdlPreprocessorParams params, File input, Writer output, Map<File, List<File>> dependencies);

   public IIdlPreprocessorParams createPreprocessorParams ();
}
