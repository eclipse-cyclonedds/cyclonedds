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
import java.util.List;

import antlr.TokenStreamRecognitionException;

import org.eclipse.cyclonedds.idt.imports.idl.preprocessor.IIdlPreprocessorStatus;


public class PreprocessorStatus implements IIdlPreprocessorStatus
{
   private int m_severity;
   
   private String m_message;
   
   private String m_filename;

   private int m_line;

   private int m_column;
   
   private Throwable m_cause;

   private List<IIdlPreprocessorStatus> m_children;

   public PreprocessorStatus ()
   {
      m_children = new ArrayList<IIdlPreprocessorStatus>();
   }

   public PreprocessorStatus (int severity)
   {
      m_severity = severity;
      m_children = new ArrayList<IIdlPreprocessorStatus>();
   }

   public PreprocessorStatus (CppLexer lexer, int severity, String message, Throwable cause)
   {
      m_severity = severity;
      m_message = message;
      m_filename = lexer.getFilename ();
      m_line = lexer.getLine ();
      m_column = lexer.getColumn ();
      m_cause = cause;
      m_children = new ArrayList<IIdlPreprocessorStatus>();
   }

   public PreprocessorStatus (TokenStreamRecognitionException cause)
   {
      m_severity = IIdlPreprocessorStatus.ERROR;
      m_message = cause.getMessage ();
      m_filename = cause.recog.getFilename ();
      m_line = cause.recog.getLine ();
      m_column = cause.recog.getColumn ();
      m_cause = cause;
      m_children = new ArrayList<IIdlPreprocessorStatus>();
   }

   public PreprocessorStatus (FileNotFoundException cause, File file)
   {
      m_severity = IIdlPreprocessorStatus.ERROR;
      m_message = cause.getMessage ();
      m_filename = file.getName ();
      m_line = -1;
      m_column = -1;
      m_cause = cause;
      m_children = new ArrayList<IIdlPreprocessorStatus>();
   }

   public int getSeverity ()
   {
      return m_severity;
   }

   public String getMessage ()
   {
      return m_message;
   }

   public String getFilename ()
   {
      return m_filename;
   }

   public int getLine ()
   {
      return m_line;
   }

   public int getColumn ()
   {
      return m_column;
   }

   public Throwable getException ()
   {
      return m_cause;
   }

   public IIdlPreprocessorStatus[] getChildStati ()
   {
      return (IIdlPreprocessorStatus[]) m_children.toArray (new IIdlPreprocessorStatus[m_children.size ()]);
   }

   public void add (IIdlPreprocessorStatus status)
   {
      m_children.add (status);
      if(this.m_severity < status.getSeverity ())
      {
         this.m_severity = status.getSeverity ();
      }
   }

   public boolean isMultiStatus ()
   {
      return !m_children.isEmpty ();
   }

   public boolean isOK ()
   {
      if(isMultiStatus())
      {
         for(IIdlPreprocessorStatus child : m_children)
         {
            if(!child.isOK ())
            {
               return false;
            }
         }
      }
      return m_severity == IIdlPreprocessorStatus.OK;
   }

   public String toString ()
   {
      return m_filename + ':' + m_line + ':' + m_column + ':' + getMessage ();
   }
}
