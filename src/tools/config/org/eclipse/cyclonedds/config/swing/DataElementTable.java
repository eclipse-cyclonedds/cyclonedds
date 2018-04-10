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

import java.awt.Point;

import javax.swing.JPopupMenu;
import javax.swing.JTable;

import org.eclipse.cyclonedds.common.view.StatusPanel;
import org.eclipse.cyclonedds.config.data.DataElement;
import org.eclipse.cyclonedds.config.data.DataNode;

public class DataElementTable extends JTable implements DataNodePopupSupport {
    private static final long serialVersionUID = 3904871676109190600L;
    private final DataElementTableCellRenderer renderer;
    private final DataElementTableModel model;
    private final DataElementTableModelEditor editor;
    private DataNodePopup popupController;
    private final StatusPanel status;
    
    public DataElementTable(DataNodePopup popup, DataElement element, StatusPanel status){
        super();
        this.model = new DataElementTableModel();
        this.editor = new DataElementTableModelEditor(model);
        this.renderer = new DataElementTableCellRenderer(model);
        this.editor.setStatusListener(status);
        this.model.setElement(element);
        this.status = status;
        this.setModel(model);
        this.setSelectionMode(javax.swing.ListSelectionModel.SINGLE_SELECTION);
        this.getTableHeader().setReorderingAllowed(false);
        this.setSurrendersFocusOnKeystroke(true);
        this.getColumnModel().getColumn(1).setCellEditor(editor);
        this.getColumnModel().getColumn(1).setCellRenderer(renderer);
        
        if(popup == null){
            this.popupController = new DataNodePopup();
        } else {
            this.popupController = popup;
        }
        this.popupController.registerPopupSupport(this);
    }
    
    public void setStatusListener(StatusPanel  status){
        this.editor.setStatusListener(status);
    }
    
    public DataNode getDataNodeAt(int row){
        this.getSelectionModel().setSelectionInterval(row, row);
        return this.model.getNodeAt(row);
    }

    @Override
    public DataNode getDataNodeAt(int x, int y) {
        int row = this.rowAtPoint(new Point(x, y));
        return this.getDataNodeAt(row);
    }

    @Override
    public void showPopup(JPopupMenu popup, int x, int y) {
        popup.show(this, x, y);
    }

    @Override
    public void setStatus(String message, boolean persistent, boolean busy) {
        if(this.status != null){
            this.status.setStatus(message, persistent, busy);
        }
    }

    public DataElementTableModelEditor getEditor() {
        return this.editor;
    }
}
