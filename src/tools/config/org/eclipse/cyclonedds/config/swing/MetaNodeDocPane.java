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

import java.io.StringWriter;

import javax.swing.JEditorPane;

import org.eclipse.cyclonedds.config.meta.MetaAttribute;
import org.eclipse.cyclonedds.config.meta.MetaElement;
import org.eclipse.cyclonedds.config.meta.MetaNode;

@SuppressWarnings("serial")
public class MetaNodeDocPane extends JEditorPane {
    private boolean showElementName;
    private boolean showChildrenAttributes;
    private static final String fontStyle = "font-family:Sans-serif";
    
    public MetaNodeDocPane(){
        super("text/html", "No selection");
        this.init(true, true);
    }
    
    public MetaNodeDocPane(boolean showElementName, boolean showChildrenAttributes){
        super("text/html", "No selection");
        this.init(showElementName, showChildrenAttributes);
    }
    
    private void init(boolean showElementName, boolean showChildrenAttributes){
        this.setEditable(false);
        this.showElementName = showElementName;
        this.showChildrenAttributes = showChildrenAttributes;
    }
    
    public void setNode(MetaNode node){
        String doc;
        MetaNode[] children;
        MetaAttribute attribute;
        
        int attributeChildrenCount = 0;
        
        StringWriter writer = new StringWriter();
        
        if(node == null){
            writer.write("No element selected.");
        } else {
            doc = this.layoutDoc(node.getDoc());
            
            if(node instanceof MetaElement){
                if(this.showElementName){
                    int maxOcc = ((MetaElement)node).getMaxOccurrences();
                    int minOcc = ((MetaElement)node).getMinOccurrences();
                    String occ;
                    
                    if(maxOcc == Integer.MAX_VALUE){
                        occ = "occurrences: " + minOcc + " - " + "*";
                    } else {
                        occ = "occurrences: " + minOcc + " - " + maxOcc;
                    }
                    writer.write("<h1>" + ((MetaElement)node).getName());

                    String dimension = node.getDimension();

                    if (dimension != null) {
                        writer.write(" <font size=\"-1\">(" + occ
                                + " and dimension: " + dimension
                                + ")</font></h1>");
                    } else {
                        writer.write(" <font size=\"-1\">(" + occ
                                + ")</font></h1>");
                    }
                }
                writer.write("<font style=\"" + MetaNodeDocPane.fontStyle + "\">" + doc + "</font>");
                children = ((MetaElement)node).getChildren();
                
                for(MetaNode mn: children){
                    if(mn instanceof MetaAttribute){
                        attributeChildrenCount++;
                    }
                }
                if(this.showChildrenAttributes){
                    if(attributeChildrenCount > 0){
                        writer.write("<h2>Attributes</h2>");
                        
                        for(MetaNode mn: children){
                            if(mn instanceof MetaAttribute){
                                this.writeNode(writer, mn);
                            }
                        }
                    } else {
                        writer.write("<h2>Attributes</h2>Element has no attributes");
                    }
                }
            } else if(node instanceof MetaAttribute){
                attribute = (MetaAttribute)node;
                
                if(this.showElementName){
                    writer.write("<h1>" + attribute.getName() + "(");
                    
                    if(attribute.isRequired()){
                        writer.write("required");
                    } else {
                        writer.write("optional");
                    }
                    String dimension = attribute.getDimension();

                    if (dimension != null) {
                        writer.write(" and dimension: ");
                        writer.write(dimension);
                    }
                    writer.write(")</h1> -");
                }
                writer.write("<font style=\"" + MetaNodeDocPane.fontStyle + "\">" + doc + "</font>");
            } else {
                writer.write("<font style=\"" + MetaNodeDocPane.fontStyle + "\">" + doc + "</font>");
            }
        }
        this.setText(writer.toString());
        this.setCaretPosition(0);
    }
    
    
    private void writeNode(StringWriter writer, MetaNode node){
        MetaElement element;
        MetaAttribute attribute;
        String doc;
        String dimension = node.getDimension();
        
        if(node instanceof MetaElement){
            element = (MetaElement)node;
            writer.write("<h3>" + element.getName());
            writer.write(" (occurrences : " + element.getMinOccurrences()
                    + " - " + element.getMaxOccurrences());

            if (dimension != null) {
                writer.write(" dimension: " + dimension);
            }
            writer.write(")</h3>");
        } else if(node instanceof MetaAttribute){
            attribute = (MetaAttribute)node;
            writer.write("<h3>" + attribute.getName() + " ("); 
            
            if(attribute.isRequired()){
                writer.write("required");
            } else {
                writer.write("optional");
            }
            if (dimension != null) {
                writer.write(" and dimension: " + dimension);
            }
            writer.write(")</h3>");
        }
        doc = this.layoutDoc(node.getDoc());
        writer.write("<font style=\"" + MetaNodeDocPane.fontStyle + "\">" + doc + "</font>");
    }
    
    private String layoutDoc(String doc){
        String temp;
        char c;
        StringWriter writer = new StringWriter();
        
        if(doc == null){
            temp = "No description available.";
        } else {
            temp = doc;
        }
        temp = temp.trim();
        
        if(temp.length() == 0){
            temp = "No description available.";
        } else {
            if(temp.startsWith("<p>")){
                temp = temp.substring(3);
                temp = temp.replaceFirst("</p>", " ");
            }
            
            for(int i=0; i<temp.length(); i++){
               c = temp.charAt(i);
               
               if((c != '\n') && (c != '\t')){
                   if(c == ' '){
                       if((i+1)<temp.length()){
                           if(temp.charAt(i+1) != ' '){
                               writer.write(c);
                           }
                       } else {
                           writer.write(c);
                       }
                   } else {
                       writer.write(c);
                   }
               }
            }
        }
        return writer.toString();
    }
}
