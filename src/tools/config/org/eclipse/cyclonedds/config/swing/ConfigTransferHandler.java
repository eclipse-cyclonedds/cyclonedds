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

import java.awt.datatransfer.DataFlavor;
import java.awt.datatransfer.Transferable;
import java.io.File;
import java.net.URI;
import java.util.List;

import javax.swing.JComponent;
import javax.swing.TransferHandler;

@SuppressWarnings("serial")
public class ConfigTransferHandler extends TransferHandler {
    private final ConfigWindow view;
    
    public ConfigTransferHandler(ConfigWindow view){
        this.view = view;
    }
    @Override
    public boolean canImport(JComponent comp, DataFlavor[] transferFlavors) {
        for(DataFlavor flavor: transferFlavors){
            if(flavor.equals(DataFlavor.javaFileListFlavor)){
                view.setStatus("Drag here to open " + 
                        flavor.getHumanPresentableName() + " file.", false);
                return true;
            } else if(flavor.equals(DataFlavor.stringFlavor)){
                view.setStatus("Drag here to open " + 
                        flavor.getHumanPresentableName() + " file.", false);
                return true;
            }
        }
        view.setStatus("Warning: Unsupported type.", false);
        
        return false;
    }
    
    @Override
    public boolean importData(JComponent comp, Transferable t) {
        if (!canImport(comp, t.getTransferDataFlavors())) {
            view.setStatus("Warning: Unsupported type", false);
            return false;
        }
        try{
            if(t.isDataFlavorSupported(DataFlavor.javaFileListFlavor)){
                @SuppressWarnings("unchecked")
                List<File> files = (List<File>) t.getTransferData(DataFlavor.javaFileListFlavor);
                
                if(files.size() == 1){
                    File file = files.get(0);
                    view.getController().handleOpen(file);
                } else {
                    return false;
                }
            } else if(t.isDataFlavorSupported(DataFlavor.stringFlavor)){
                String str = (String) t.getTransferData(DataFlavor.stringFlavor);
                
                if(str.startsWith("file:/")){
                    File f = new File(new URI(str));
                    view.getController().handleOpen(f);
                } else {
                    view.setStatus("Warning: Unsupported file.", false);
                }
            } else {
                view.setStatus("Warning: Unsupported drag-and-drop type", false);
            }
        } catch (Exception e) {
            view.setStatus("Warning: " + e.getMessage(), false);
            return false;
        }
        return true;
    }
}
