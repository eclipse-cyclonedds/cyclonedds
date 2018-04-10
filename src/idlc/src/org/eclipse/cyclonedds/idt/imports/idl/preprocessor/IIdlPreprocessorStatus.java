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

public interface IIdlPreprocessorStatus
{
   public static final int OK = 0;

   public static final int INFO = 0x01;

   public static final int WARNING = 0x02;

   public static final int ERROR = 0x04;

   public static final int CANCEL = 0x08;

   public int getSeverity ();
   
   public String getMessage ();
   
   public String getFilename ();

   public int getLine ();

   public int getColumn ();

   public Throwable getException ();
   
   public IIdlPreprocessorStatus[] getChildStati();
   
   public boolean isOK();
   
   public boolean isMultiStatus();
}
