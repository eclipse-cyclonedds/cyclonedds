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

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.awt.print.PrinterJob;
import java.io.File;

import javax.print.attribute.HashPrintRequestAttributeSet;
import javax.print.attribute.PrintRequestAttributeSet;
import javax.print.attribute.standard.Copies;
import javax.print.attribute.standard.MediaSizeName;
import javax.swing.JFileChooser;
import javax.swing.JOptionPane;
import javax.swing.SwingUtilities;
import javax.swing.filechooser.FileFilter;

import org.eclipse.cyclonedds.common.util.Report;
import org.eclipse.cyclonedds.config.data.DataConfiguration;
import org.eclipse.cyclonedds.config.data.DataException;
import org.eclipse.cyclonedds.config.meta.MetaConfiguration;

public class ConfigWindowController implements ActionListener {


    private final ConfigWindow view;
    private HelpWindow helpView;
    private boolean closeInProgress;
    private boolean exitInProgress;
    private boolean newInProgress;
    private boolean openInProgress;
    private File curFile;
    private final JFileChooser openFileChooser;
    private final JFileChooser saveFileChooser;

    public ConfigWindowController(ConfigWindow view){
        this.view               = view;
        this.closeInProgress    = false;
        this.exitInProgress     = false;
        this.newInProgress      = false;
        this.openInProgress     = false;
        this.openFileChooser    = new JFileChooser();
        this.saveFileChooser    = new JFileChooser();
        this.curFile            = null;
        this.helpView           = null;

        // Initialize the MetaConfiguration. This is done so the Configurator can know for
        // which product its meant for, according to the meta config file.
        MetaConfiguration.getInstance();
        view.setWindowTitle();

        File f                  = null;
        String fileSep          = System.getProperty("file.separator");

        f = new File(System.getProperty("user.dir") + fileSep);
        this.openFileChooser.setCurrentDirectory(f);
        this.openFileChooser.setDialogTitle("Open configuration");
        this.openFileChooser.setMultiSelectionEnabled(false);
        this.openFileChooser.setFileFilter(new ConfigChooseFilter());
        this.openFileChooser.setAcceptAllFileFilterUsed(false);
        this.openFileChooser.setApproveButtonText("Open");

        this.saveFileChooser.setCurrentDirectory(f);
        this.saveFileChooser.setDialogTitle("Save configuration");
        this.saveFileChooser.setMultiSelectionEnabled(false);
        this.saveFileChooser.setFileFilter(new ConfigChooseFilter());
        this.saveFileChooser.setAcceptAllFileFilterUsed(false);
        this.saveFileChooser.setApproveButtonText("Save");
    }

    @Override
    public void actionPerformed(ActionEvent e) {
        String command = e.getActionCommand();

        try{
            if("exit".equals(command)){
                this.exitInProgress = true;
                this.handleConditionalSave();
            } else if("close".equals(command)){
                this.closeInProgress = true;
                this.handleConditionalSave();
            } else if("save".equals(command)){
                this.handleSave(false);
            } else if("save_as".equals(command)){
                this.handleSave(true);
            } else if("new".equals(command)){
                this.newInProgress = true;
                this.handleConditionalSave();
            } else if("open".equals(command)){
                this.openInProgress = true;
                this.handleConditionalSave();
            } else if("print".equals(command)){
                this.handlePrint();
            } else if("help".equals(command)){
                this.handleHelp();
            } else if("cancel".equals(command)){
                this.handleCancel();
            } else {
                this.handleSetStatus("Warning: Command '" + command + "' not implemented.", false, false);
            }
        } catch(Exception ex){
            this.handleSetStatus("Error: " + ex.getMessage(), false, false);
            this.printStackTrace(ex);
            this.handleSetEnabled(true);
        }
    }

    private void handleConditionalSave(){
        DataConfiguration config  = view.getDataConfiguration();

        if(config == null){
            this.handleNextAction();
        } else if(config.isUpToDate()){
            this.handleNextAction();
        } else {
            int answer = JOptionPane.showConfirmDialog(
                    this.view,
                    "Configuration has changed. Save changes?",
                    "Close configuration",
                    JOptionPane.YES_NO_CANCEL_OPTION);

            if(answer == JOptionPane.YES_OPTION){
                this.handleSave(false);
            } else if(answer == JOptionPane.NO_OPTION){
                this.handleNextAction();
            } else {
                this.handleCancel();
            }
        }
    }

    private void handlePrint(){
        final DataConfiguration config = view.getDataConfiguration();

        if(config == null){
            this.handleSetStatus("No configuration to print.", false);
        } else {
            this.handleSetStatus("Printing configuration...", true);
            this.handleSetEnabled(false);

            Runnable worker = new Runnable(){
                @Override
                public void run(){
                    try {
                        PrinterJob pjob = PrinterJob.getPrinterJob();
                        PrintRequestAttributeSet aset = new HashPrintRequestAttributeSet();
                        aset.add(MediaSizeName.ISO_A4);
                        aset.add(new Copies(1));
                        boolean doPrint = pjob.printDialog(aset);

                        if(doPrint){
                            /*
                            if(currentDialog != null){
                                currentDialog.setStatus(null, true);
                            }
                            DocFlavor flavor = DocFlavor.INPUT_STREAM.POSTSCRIPT;
                            Doc doc = new SimpleDoc(config.toString(), flavor, null);
                            DocPrintJob docPrintJob = pjob.getPrintService().createPrintJob();
                            docPrintJob.print(doc, aset);
                            handleSetStatus("Configuration printed.", true);
                            */
                            view.setStatus("Warning: Printing not supported yet.", false);
                            handleSetEnabled(true);
                        } else {
                            handleCancel();
                        }
                    } catch (Exception e) {
                        view.setStatus("Error:" + e.getMessage(), false);
                        handleSetEnabled(true);
                    }
                }
            };
            SwingUtilities.invokeLater(worker);
        }
    }

    private void handleExit(){
        this.exitInProgress = false;
        view.dispose();
        System.exit(0);
    }

    public void handleOpen(File file){
        this.curFile = file;
        this.openInProgress = true;
        this.handleConditionalSave();
    }

    private void handleOpen(){
        openInProgress = false;
        File file = null;

        if(this.curFile != null){
            file = this.curFile;
        } else {
            int returnVal = this.openFileChooser.showOpenDialog(this.view);

            if (returnVal == JFileChooser.APPROVE_OPTION) {
                file = this.openFileChooser.getSelectedFile();
            } else {
                this.handleSetStatus("No file opened", false, false);
                this.handleNextAction();
            }
        }

        if(file != null){
            final File f = file;
            this.handleSetEnabled(false);
            view.setStatus("Opening configuration from " + f.getAbsolutePath() + "...", true, true);
            view.repaint();

            Runnable worker = new Runnable(){
                @Override
                public void run(){
                    view.setDataConfiguration(null);
                    DataConfiguration config = null;

                    try {
                        config = new DataConfiguration(f, false);
                    } catch (DataException e) {
                        Report.getInstance().writeInfoLog("ConfigWindowController handleOpen\n" + e.getMessage());
                    }
                    if(config != null){
                        view.setDataConfiguration(config);
                        view.setStatus("Configuration opened.", false);
                    } else {
                        view.setStatus("Configuration could not be opened.", false);
                    }
                    handleNextAction();
                }
            };
            SwingUtilities.invokeLater(worker);
        }
        this.curFile = null;
    }

    public void handleOpenFromUri(String uri) {
        openInProgress = false;
        File file = null;

        if (uri.startsWith("file://")) {
            /* strip off file:// */
            uri = uri.substring(7);
        }
        file = new File(uri);

        final File f = file;
        this.handleSetEnabled(false);
        view.setStatus("Opening configuration from " + f.getAbsolutePath()
                + "...", true, true);
        view.repaint();

        Runnable worker = new Runnable() {
            @Override
            public void run() {
                view.setDataConfiguration(null);
                DataConfiguration config = null;

                try {
                    config = new DataConfiguration(f, false);
                } catch (DataException e) {
                    Report.getInstance().writeInfoLog(
                            "ConfigWindowController handleOpen\n"
                                    + e.getMessage());
                    int answer = JOptionPane.showConfirmDialog(
                            view,
                            "The configuration is not valid.\nReason: "
                                    + e.getMessage()
                                    + "\nTry automatic repairing?",
                            "Invalid configuration", JOptionPane.YES_NO_OPTION);

                    if (answer == JOptionPane.YES_OPTION) {
                        try {
                            config = new DataConfiguration(f, true);
                            view.setStatus(
                                    "Configuration repaired successfully.",
                                    false);
                        } catch (DataException e1) {
                            JOptionPane.showMessageDialog(view,
                                    "Configuration could not be repaired.\nReason: '"
                                            + e.getMessage() + "'", "Error",
                                    JOptionPane.ERROR_MESSAGE);
                            handleSetStatus(
                                    "Configuration could not be repaired.",
                                    false);
                            handleNextAction();
                        }
                    } else if (answer == JOptionPane.NO_OPTION) {
                        handleSetStatus("Configuration not opened.", false);
                        handleNextAction();
                    }
                }
                if (config != null) {
                    view.setDataConfiguration(config);
                    view.setStatus("Configuration opened.", false);
                    handleNextAction();
                }
            }
        };
        SwingUtilities.invokeLater(worker);
        this.curFile = null;
    }

    private void handleSave(boolean alwaysAskFile){
        int returnVal;
        File file;
        int answer;
        String path;
        boolean proceed = false;

        final DataConfiguration config = view.getDataConfiguration();

        if(config == null){
            this.handleCancel("Warning: No configuration to save.");
            return;
        }

        this.handleSetEnabled(false);

        if((!alwaysAskFile) && (config.getFile() != null)){
            this.handleSetStatus("Saving configuration to: " + config.getFile().getAbsolutePath() + "...", true);

            Runnable worker = new Runnable(){
                @Override
                public void run(){
                    try {
                        config.store(true);
                        view.setStatus("Configuration saved.", false, false);
                        handleNextAction();
                    } catch (DataException e) {
                        handleSetStatus("Error: Cannot save configuration to: " +
                                            config.getFile().getAbsolutePath(),
                                            false);
                        handleNextAction();
                    }
                }
            };
            SwingUtilities.invokeLater(worker);
        } else {
            try{
                do {
                    answer = JOptionPane.CANCEL_OPTION;
                    saveFileChooser.setSelectedFile(config.getFile());
                    returnVal = this.saveFileChooser.showSaveDialog(this.view);

                    if (returnVal == JFileChooser.APPROVE_OPTION) {
                        file = this.saveFileChooser.getSelectedFile();
                        path = file.getAbsolutePath();

                        if(!path.endsWith(ConfigChooseFilter.CONFIG_SUFFIX)){
                            path = path + ConfigChooseFilter.CONFIG_SUFFIX;
                            file = new File(path);
                        }

                        this.handleSetStatus("Saving configuration to: " + file.getAbsolutePath() + "...", true);

                        if(file.equals(config.getFile())){
                            proceed = true;
                        } else if (file.exists()){
                            answer = JOptionPane.showConfirmDialog(
                                    this.view,
                                    "The file '" + path + "' already exists. Overwrite?",
                                    "File already exists",
                                    JOptionPane.YES_NO_CANCEL_OPTION);

                            if(answer == JOptionPane.YES_OPTION){
                                proceed = true;
                                config.setFile(file);
                            } else if(answer == JOptionPane.NO_OPTION){
                                proceed = false;
                            } else {
                                proceed = false;
                                this.handleNextAction();
                            }
                        } else {
                            config.setFile(file);
                            proceed = true;
                        }
                    } else {
                        proceed = false;
                        this.handleNextAction();
                    }
                } while(answer == JOptionPane.NO_OPTION);

                if(proceed){
                    this.handleSetStatus("Saving configuration to: " + config.getFile().getAbsolutePath() + "...", true);

                    Runnable worker = new Runnable(){
                        @Override
                        public void run(){
                            try {
                                config.store(true);
                                view.setStatus("Configuration saved.", false, false);
                                handleNextAction();
                            } catch (DataException e) {
                                view.setStatus("Error:" + e.getMessage(), false);
                                handleSetEnabled(true);
                            }
                        }
                    };
                    SwingUtilities.invokeLater(worker);
                }
            } catch (DataException e) {
                this.handleSetStatus("Error: " + e.getMessage(), false);
                handleSetEnabled(true);
            } catch(Exception exc){
                this.handleSetStatus("Error: " + exc.getMessage(), false);
                handleSetEnabled(true);
            }
        }
    }

    private void handleHelp(){
        if(helpView != null){
            helpView.toFront();
        } else {
            view.setStatus("Opening help for configuration...", true, true);
            this.handleSetEnabled(false);

            Runnable worker = new Runnable(){
                @Override
                public void run(){
                    MetaConfiguration metaConfig = MetaConfiguration.getInstance();
                    if (metaConfig != null) {
                        helpView = new HelpWindow(metaConfig);
                        helpView.setLocationRelativeTo(view);
                        view.setStatus("Help pane openened.", false, false);
                        handleSetEnabled(true);
                        helpView.addWindowListener(new WindowAdapter(){
                            @Override
                            public void windowClosed(WindowEvent e) {
                                helpView = null;
                            }
                        });
                        helpView.setVisible(true);
                        helpView.toFront();
                    } else {
                        view.setStatus("Failed to open the configuration help", true, false);
                    }
                }
            };
            SwingUtilities.invokeLater(worker);
        }
    }

    private void handleNew(){
        view.setStatus("Creating new configuration...", true, true);
        this.handleSetEnabled(false);

        Runnable worker = new Runnable(){
            @Override
            public void run(){
                view.setDataConfiguration(null);
                newInProgress = false;
                try {
                    DataConfiguration config = new DataConfiguration();
                    view.setDataConfiguration(config);
                    view.setStatus("New configuration created.", false, false);
                } catch (DataException e) {

                    view.setStatus("Error: Could not create new configuration.", false, false);
                }
                handleSetEnabled(true);
            }
        };
        SwingUtilities.invokeLater(worker);
    }

    private void handleClose(){
        view.setStatus("Closing configuration...", true, true);
        this.handleSetEnabled(false);

        Runnable worker = new Runnable(){
            @Override
            public void run(){
                view.setDataConfiguration(null);
                closeInProgress = false;
                view.setStatus("Configuration closed.", false);
                handleSetEnabled(true);
            }
        };
        SwingUtilities.invokeLater(worker);
    }

    private void handleNextAction(){
        if(closeInProgress){
            handleClose();
        } else if(exitInProgress){
            exitInProgress = false;
            handleExit();
        } else if(newInProgress){
            handleNew();
        } else if(openInProgress){
            handleOpen();
        } else  {
            handleSetEnabled(true);
        }
    }

    private void handleCancel(){
        view.setStatus("Action cancelled.", false);
        this.handleSetEnabled(true);
    }

    private void handleCancel(String message){
        view.setStatus(message, false);
        this.handleSetEnabled(true);
    }

    private void handleSetEnabled(boolean enabled){
        if(enabled){
            this.view.enableView();
        } else {
            this.view.disableView();
        }
        this.view.setActionsEnabled(enabled);
    }

    private void handleSetStatus(String message, boolean persistent, boolean busy){
        this.view.setStatus(message, persistent, busy);

    }

    private void handleSetStatus(String message, boolean persistent){
        this.view.setStatus(message, persistent, false);
    }

    private void printStackTrace(Exception exception){
        StackTraceElement[] elements = exception.getStackTrace();
        StringBuffer buf = new StringBuffer();
        buf.append("The following exception occurred: \n");

        for(int i=0; i<elements.length; i++){
            buf.append(elements[i].getFileName() + ", " +
                    elements[i].getMethodName() + ", " +
                    elements[i].getLineNumber() + "\n");
        }
        String error = buf.toString();
        Report.getInstance().writeErrorLog(error);
    }

    private static class ConfigChooseFilter extends FileFilter{
        private static String description = null;
        public static final String CONFIG_SUFFIX = ".xml";

        public ConfigChooseFilter(){
          ConfigChooseFilter.description = "Eclipse Cyclone DDS config files (*" +
            ConfigChooseFilter.CONFIG_SUFFIX + ")";
        }

        @Override
        public boolean accept(File f) {
            if (f.isDirectory()) {
                return true;
            }

            if(f.getName().endsWith(ConfigChooseFilter.CONFIG_SUFFIX)){
                return true;
            }
            return false;
        }

        @Override
        public String getDescription() {
            return ConfigChooseFilter.description;

        }
    }
}
