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

import java.awt.Color;
import java.awt.Component;
import java.util.logging.Logger;

import javax.swing.JTable;
import javax.swing.table.DefaultTableCellRenderer;

import org.eclipse.cyclonedds.config.data.DataNode;

public class DataElementTableCellRenderer extends DefaultTableCellRenderer {
    private static final long serialVersionUID = 8091366819992773074L;
    private DataElementTableModel tableModel = null;

    public DataElementTableCellRenderer(DataElementTableModel tableModel){
        this.tableModel = tableModel;
        Logger.getLogger("org.eclipse.cyclonedds.config.swing");
    }

    @Override
    public Component getTableCellRendererComponent(JTable table,
                                                   Object value,
                                                   boolean isSelected,
                                                   boolean hasFocus,
                                                   int row,
                                                   int column){
        Component comp = super.getTableCellRendererComponent (table,
           value, isSelected, hasFocus, row, column);
        DataNode node = tableModel.getNodeAt(row);
        node = node.getParent();
        table.setToolTipText(null);
        
        comp.setBackground (Color.WHITE);
        comp.setForeground (Color.BLACK);
        table.setToolTipText(null);
        return comp;
    }
}
