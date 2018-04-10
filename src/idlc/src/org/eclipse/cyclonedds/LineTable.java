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
package org.eclipse.cyclonedds;

import java.util.*;
import java.io.File;
import org.antlr.v4.runtime.*;
import org.stringtemplate.v4.ST;

import org.eclipse.cyclonedds.parser.IDLParser;

public class LineTable
{
  private static class Entry
  {
    int position;
    int line;
    String file;
  }

  public LineTable (String filename, Iterator<Token> it, boolean forcpp)
  {
    Token t;
    Entry e;
    StringTokenizer lineinfo;
    StringBuffer realfile;
    String basename;
    int depth = 0;

    this.filename = filename;
    if (forcpp)
    {
      suffix = "-cyclone";
    }
    else
    {
      suffix = "";
    }
    table = new LinkedList <Entry> ();
    subfiles = new ArrayList <String> ();

    while (it.hasNext ())
    {
      t = it.next ();
      if (t.getType () == IDLParser.CODEPOS)
      {
        e = new Entry ();
        e.position = t.getLine ();
        lineinfo = new StringTokenizer (t.getText ());
        lineinfo.nextToken ();  // Skip '#'
        e.line = Integer.parseInt (lineinfo.nextToken ());
        realfile = new StringBuffer (lineinfo.nextToken ());
        while (realfile.charAt (realfile.length () - 1) != '"')
        {
          realfile.append (" " + lineinfo.nextToken ());
        }
        e.file = realfile.substring (1, realfile.length () - 1); // Strip quotes

        table.add (e);

        if (t.getText ().endsWith (" 1"))
        {
          if (depth++ == 0)
          {
            subfiles.add
              (e.file.substring (0, e.file.lastIndexOf (".")) + suffix);
          }
        }
        else if (t.getText ().endsWith (" 2"))
        {
          depth--;
        }
      }
    }
  }

  private Entry getRelevantEntry (int pos)
  {
    Entry e;
    Iterator <Entry> iter = table.descendingIterator ();

    do
    {
      e = iter.next ();
    } while (e.position > pos);

    return e;
  }

  public String getRealPosition (int pos)
  {
    Entry e = getRelevantEntry (pos);
    return e.file + ":" + (pos + e.line - e.position - 1);
  }

  public boolean inMain (int pos)
  {
    return getRelevantEntry (pos).file.equals (filename);
  }

  public void populateIncs (ST template)
  {
    for (String s : subfiles)
    {
      template.add ("includes", new File (s).getName ());
    }
  }

  private String filename;
  private String suffix;
  private LinkedList <Entry> table;
  private ArrayList <String> subfiles;
}
