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
package org.eclipse.cyclonedds.config.swing;

import java.awt.event.MouseListener;

import javax.swing.JPopupMenu;
import org.eclipse.cyclonedds.config.data.DataNode;

public interface DataNodePopupSupport {
    public DataNode getDataNodeAt(int x, int y);
    
    public void showPopup(JPopupMenu popup, int x, int y);
    
    public void setStatus(String message, boolean persistent, boolean busy);
    
    public void addMouseListener(MouseListener listener);
    
    public void removeMouseListener(MouseListener listener);
}
