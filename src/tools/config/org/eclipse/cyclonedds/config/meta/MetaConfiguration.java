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
package org.eclipse.cyclonedds.config.meta;

import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;

import org.eclipse.cyclonedds.common.util.Report;
import org.w3c.dom.Attr;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.ErrorHandler;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;

public class MetaConfiguration {

    private final MetaElement rootElement;
    private final ArrayList<MetaElement> services;
    private static HashMap<String, String> serviceMapping = new HashMap<String, String>();
    private static MetaConfiguration instance = null;


    private MetaConfiguration(MetaElement rootElement, ArrayList<MetaElement> services) {
        this.rootElement = rootElement;
        this.services = services;
    }

    public MetaElement getRootElement() {
        return this.rootElement;
    }

    public boolean addService(MetaElement element){
        return this.services.add(element);
    }

    public MetaElement[] getServices() {
        return this.services.toArray(new MetaElement[this.services.size()]);
    }

    public String getCommandForService(String name) {
        return serviceMapping.get(name);
    }

    public String getServiceForCommand(String name) {
        String retVal = null;
        for (String service : serviceMapping.keySet()) {
            if (name.equals(serviceMapping.get(service))) {
                return service;
            }
        }
        return retVal;
    }

    private static MetaConfiguration load(String fileName){
        MetaConfiguration config = null;

        InputStream is = ClassLoader.getSystemResourceAsStream(fileName);

        if(is != null){
            config = load(is);
        }
        return config;
    }

    public static MetaConfiguration getInstance(){
        if (instance == null) {
            String fileName = "metaconfig.xml";
            instance = MetaConfiguration.load(fileName); 
        }
        return instance;
    }

    private static MetaConfiguration load(InputStream is){
        MetaConfiguration config = null;
        Document document;

        try {
            DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
            DocumentBuilder builder = factory.newDocumentBuilder();

            builder.setErrorHandler(new ErrorHandler(){
                @Override
                public void warning(SAXParseException exception) throws SAXException {

                }

                @Override
                public void error(SAXParseException exception) throws SAXException {
                    Report.getInstance().writeErrorLog("Parse error at line: " + exception.getLineNumber() +
                                        " column: " + exception.getColumnNumber() + ".");

                }

                @Override
                public void fatalError(SAXParseException exception) throws SAXException {
                    Report.getInstance().writeErrorLog("Parse error at line: " + exception.getLineNumber() +
                                        " column: " + exception.getColumnNumber() + ".");
                }
            });
            document = builder.parse(is);
            config = init(document);

        } catch (ParserConfigurationException exc) {
            Report.getInstance().writeErrorLog(exc.getMessage());
        } catch (SAXException exc) {
            Report.getInstance().writeErrorLog(exc.getMessage());
        } catch (IOException exc) {
            Report.getInstance().writeErrorLog(exc.getMessage());
        }
        return config;
    }



    @Override
    public String toString(){
        StringBuffer buf = new StringBuffer();
        buf.append("ROOT_ELEMENT:");
        buf.append(this.rootElement.toString().replaceAll("\n", "\n\t"));
        for(MetaElement me: this.services){
            buf.append("\nSERVICE:\n");
            buf.append(me.toString().replaceAll("\n", "\n\t"));
        }
        return buf.toString();
    }

    private static MetaConfiguration init(Document dom){
        Node childElement;
        String childName;
        MetaElement metaElement, rootMetaElement = null;
        MetaConfiguration configuration = null;
        boolean res = true;

        try{
            ArrayList<MetaElement> metaElements = new ArrayList<MetaElement>();
            Element rootElement = dom.getDocumentElement();

            NodeList children = rootElement.getChildNodes();

            for(int i=0; i<children.getLength(); i++){
                childElement = children.item(i);
                childName = childElement.getNodeName();

                if(childElement instanceof Element){
                    if("rootElement".equals(childName)){
                        rootMetaElement = parseElement((Element)childElement, true);
                    } else if("element".equals(childName)){
                        metaElement = parseElement((Element)childElement, false);
                        if(metaElement != null){
                            if(rootMetaElement == null){
                                throw new MetaException("Could not resolve meta configuration.");
                            }
                            rootMetaElement.addChild(metaElement);
                            metaElements.add(metaElement);
                        }
                    }
                }
            }

            if((rootMetaElement != null)){
                configuration = new MetaConfiguration(rootMetaElement, metaElements);

            }
        } catch(Exception exc){
            Report.getInstance().writeErrorLog("Exception occurred during initialization of meta configuration: " + exc.getMessage());
        }

        if (configuration == null) {
            Report.getInstance().writeErrorLog(
                    "Parsing of meta configuration failed.");
        }
        return configuration;
    }

    private static String getDimension(Node node) {
        if (node == null) {
            return null;
        }
        if (!(node instanceof Element)) {
            return null;
        }
        NodeList children = node.getChildNodes();

        if (children == null) {
            return null;
        }
        if (children.getLength() == 0) {
            return null;
        }
        Node child = children.item(0);

        return child.getNodeValue();
    }

    private static String getDimensionChild(Element element) {
        if (element == null) {
            return null;
        }
        String dimension = null;
        NodeList children = element.getChildNodes();

        if (children != null) {
            for (int i = 0; i < children.getLength() && dimension == null; i++) {
                Node child = children.item(i);

                if ("dimension".equals(child.getNodeName())) {
                    dimension = getDimension(child);
                }
            }
        }
        return dimension;
    }

    public static long createLongValuefromSizeValue(String strValue) {
        long lValue;
        double base = 1;
        long tempValue = 0;
        int charPos =0;
        boolean found = false;

        /* find integers in string and parse them as long */
        Pattern p = Pattern.compile("\\d+");
        Matcher m = p.matcher(strValue);
        if(m.find()) {
            tempValue = Long.parseLong(m.group());
        }
        for (int i=0;i<strValue.length()&&!found;i++) {
            if (strValue.charAt(i) == 'K' || strValue.charAt(i) == 'k') {
                base =1024;
                found = true;
            }
            if (strValue.charAt(i) == 'M'  || strValue.charAt(i) == 'm') {
                base =Math.pow(1024,2);
                found = true;
            }
            if (strValue.charAt(i) == 'G'  || strValue.charAt(i) == 'g') {
                base =Math.pow(1024,3);
                found = true;
            }
            if (found) {
                charPos = i;
            }
        }
        lValue = (long)base*tempValue;

        /* check for multiple {KMG} characters, if present return 0 */
        if (charPos != (strValue.length()-1)&& base != 1) {
            lValue =0;
        }

        return lValue;
    }

    private static MetaElement parseElement(Element element, boolean isRootElement) throws MetaException{
        MetaElement result, tmp;
        MetaAttribute tmpAttr;
        ArrayList<MetaNode> metaChildren;
        NodeList children, nodes;
        Node node;
        String nodeName, name;
        int minOccurrences, maxOccurrences;
        String comment = null;
        String dimension = null;

        try{
            name           = element.getAttribute("name");
            
            //if(!isRootElement){
                minOccurrences = Integer.parseInt(element.getAttribute("minOccurrences"));
                maxOccurrences = Integer.parseInt(element.getAttribute("maxOccurrences"));

                if(maxOccurrences == 0){
                    maxOccurrences = Integer.MAX_VALUE;
                }
            //} else {
            //    minOccurrences = 0;
            //    maxOccurrences = Integer.MAX_VALUE;
            //}
            metaChildren   = new ArrayList<MetaNode>();
            children       = element.getChildNodes();

            for(int i=0; i<children.getLength(); i++){
                node = children.item(i);
                nodeName = node.getNodeName();

                if((node instanceof Element) && (nodeName != null)){
                    if("comment".equals(nodeName)){
                        nodes = node.getChildNodes();
                        comment = "";
                        StringBuffer buf = new StringBuffer();
                        for (int j = 0; j < nodes.getLength(); j++) {
                            node = nodes.item(j);
                            if (node != null) {
                                buf.append(node.getNodeValue());
                            }
                        }
                        comment = buf.toString();
                    } else if("element".equals(nodeName)){
                        if(!isRootElement){
                            tmp = parseElement((Element)node, isRootElement);

                            if(tmp != null){
                                metaChildren.add(tmp);
                            }
                        }
                    } else if(nodeName.startsWith("attribute")){
                        tmpAttr = parseAttribute((Element)node);

                        if(tmpAttr != null){
                            metaChildren.add(tmpAttr);
                        }
                    } else if(nodeName.startsWith("leaf")){
                        if(!isRootElement){
                            tmp = parseLeaf((Element)node);

                            if(tmp != null){
                                metaChildren.add(tmp);
                            }
                        }
                    } else if("dimension".equals(nodeName)){
                        dimension = getDimension(node);
                    }
                }
            }
            if(name != null){
                result = new MetaElement(comment, name, minOccurrences,
                        maxOccurrences, metaChildren, dimension);
            } else {
                throw getException(element, "No name found");
            }
        } catch(NumberFormatException nfe){
            throw getException(element, nfe.getMessage());
        }
        return result;
    }

    private static boolean parseServiceMapping(Element element, boolean isRootElement)
            throws MetaException {
        String command = null;
        String name = null;
        boolean result = true;
        Node node = null;
        String nodeName = null;
        NodeList children = element.getChildNodes();
        try {
            for (int i = 0; i < children.getLength() && result; i++) {
                node = children.item(i);
                nodeName = node.getNodeName();

                if ((node instanceof Element) && (nodeName != null)) {
                    if ("element".equals(nodeName)) {
                        name = ((Element) node).getAttribute("name");
                        command = ((Element) node).getAttribute("command");
                        if (name != null && command != null) {
                            serviceMapping.put(name, command);
                        } else {
                            result = false;
                            throw getException(element, "No name and/or command found");
                        }
                    }
                }
            }
        } catch (NumberFormatException nfe) {
            result = false;
            throw getException(element, nfe.getMessage());
        }
        return result;
    }

    private static MetaElement parseLeaf(Element element) throws MetaException{
        MetaElement result;
        MetaAttribute tmpAttr;
        MetaValue data;
        ArrayList<MetaNode> metaChildren;
        NodeList children;
        String name, comment, nodeName, dimension;
        int minOccurrences, maxOccurrences;
        Node node;
        NodeList nodes;

        try{
            dimension = null;
            comment        = parseComment(element);
            name           = element.getAttribute("name");
            minOccurrences = Integer.parseInt(element.getAttribute("minOccurrences"));
            maxOccurrences = Integer.parseInt(element.getAttribute("maxOccurrences"));

            if(maxOccurrences == 0){
                maxOccurrences = Integer.MAX_VALUE;
            }

            if(name != null){
                data = parseValue(element, element.getNodeName().substring(4));

                if(data != null){
                    metaChildren = new ArrayList<MetaNode>();
                    metaChildren.add(data);
                    children   = element.getChildNodes();

                    for(int i=0; i<children.getLength(); i++){
                        node = children.item(i);
                        nodeName = node.getNodeName();

                        if((node instanceof Element) && (nodeName != null)){
                            if("comment".equals(nodeName)){
                                nodes = node.getChildNodes();
                                comment = "";
                                StringBuffer buf = new StringBuffer();
                                for (int j = 0; j < nodes.getLength(); j++) {
                                    node = nodes.item(j);
                                    if (node != null) {
                                        buf.append(node.getNodeValue());
                                    }
                                }
                                comment = buf.toString();
                            } else if(nodeName.startsWith("attribute")){
                                tmpAttr = parseAttribute((Element)node);

                                if(tmpAttr != null){
                                    metaChildren.add(tmpAttr);
                                }
                            } else if(nodeName.startsWith("leaf")){
                                throw getException(element, "Leaf is not allowed to contain leaf.");
                            } else if("element".equals(nodeName)){
                                throw getException(element, "Leaf is not allowed to contain element.");
                            } else if ("dimension".equals(nodeName)) {
                                dimension = getDimension(node);
                            }
                        }
                    }
                    result = new MetaElement(comment, name, minOccurrences,
                            maxOccurrences, metaChildren, dimension);
                } else {
                    throw getException(element, "No data found");
                }
            } else {
                throw getException(element, "No name found");
            }
        } catch(NumberFormatException nfe){
            throw getException(element, "NumberFormatException occurred: " + nfe.getMessage());
        }
        return result;
    }

    private static MetaAttribute parseAttribute(Element element) throws MetaException{
        MetaAttribute result;
        MetaValue data;
        String name, comment;
        boolean required;
        String dimension;

        try{
            comment        = parseComment(element);
            name           = element.getAttribute("name");
            required       = Boolean.parseBoolean(element.getAttribute("required"));

            if(name != null){
                data = parseValue(element, element.getNodeName().substring(9));

                if(data != null){
                    dimension = getDimensionChild(element);
                    result = new MetaAttribute(comment, name, required, data, dimension);
                } else {
                    throw getException(element, "No data found");
                }
            } else {
                throw getException(element, "No name found");
            }

        } catch(NumberFormatException nfe){
            throw getException(element, "NumberFormatException occurred: " + nfe.getMessage());
        }
        return result;
    }

    private static MetaValue parseValue(Element element, String type) throws MetaException{
        MetaValue result;



        if(type.equalsIgnoreCase("int")) {
            result = parseMetaValueInt(element);
        } else if(type.equalsIgnoreCase("float")) {
            result  = parseMetaValueFloat(element);
        } else if(type.equalsIgnoreCase("boolean")) {
            result  = parseMetaValueBoolean(element);
        } else if(type.equalsIgnoreCase("string")) {
            result  = parseMetaValueString(element);
        } else if(type.equalsIgnoreCase("enum")) {
            result  = parseMetaValueEnum(element);
        } else if(type.equalsIgnoreCase("long")) {
            result  = parseMetaValueLong(element);
        } else if(type.equalsIgnoreCase("double")) {
            result  = parseMetaValueDouble(element);
        } else if(type.equalsIgnoreCase("size")) {
            result  = parseMetaValueSize(element);
        } else {
            result = null;
            Report.getInstance().writeErrorLog( "Unknown leaf or attribute type specified (" +
                                type + ").");
        }
        return result;
    }

    private static MetaValue parseMetaValueFloat(Element typeElement) throws MetaException {
        MetaValueFloat metaNode = null;
        NodeList list;
        Node node;
        String name;
        String dimension;
        float minimum, maximum, defaultValue;

        boolean foundDefault  = false;

        minimum      = Float.MIN_VALUE;
        maximum      = Float.MAX_VALUE;
        defaultValue = 0;
        dimension = null;

        list = typeElement.getChildNodes();

        for(int i=0; i<list.getLength(); i++) {
            node = list.item(i);
            name = node.getNodeName();
            node = node.getFirstChild();

            if ("minimum".equals(name)) {
                if (node == null) {
                    throw getException(typeElement,
                            "Invalid value of element 'minimum' found.");
                }
                try {
                    minimum = Float.parseFloat(node.getNodeValue());
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'minimum' found.");
                }

            } else if ("maximum".equals(name)) {
                if (node == null) {
                    throw getException(typeElement,
                            "Invalid value of element 'maximum' found.");
                }
                try {
                    maximum = Float.parseFloat(node.getNodeValue());
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'maximum' found.");
                }
            } else if ("default".equals(name)) {
                if (node == null) {
                    throw getException(typeElement,
                            "Invalid value of element 'default' found.");
                }
                try {
                    defaultValue = Float.parseFloat(node.getNodeValue());
                    foundDefault = true;
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'default' found.");
                }
            } else if ("dimension".equals(name)) {
                dimension = getDimension(node.getParentNode());
            }
        }
        if(foundDefault) {
            metaNode = new MetaValueFloat(null, defaultValue, maximum, minimum,
                    dimension);
        } else {
            throw getException(typeElement, "Element 'default' not found for contentType 'FLOAT'");
        }
        return metaNode;
    }

    private static MetaValueLong parseMetaValueLong(Element typeElement) throws MetaException {
        MetaValueLong metaNode = null;
        NodeList list;
        Node node;
        String name, dimension;
        long minimum, maximum, defaultValue;

        boolean foundDefault  = false;

        minimum      = Long.MIN_VALUE;
        maximum      = Long.MAX_VALUE;
        defaultValue = 0;
        dimension = null;

        list = typeElement.getChildNodes();

        for(int i=0; i<list.getLength(); i++) {
            node = list.item(i);
            name = node.getNodeName();
            node = node.getFirstChild();

            if("minimum".equals(name)) {
                if (node == null) {
                    throw getException(typeElement, "Invalid value of element 'minimum' found.");
                }
                try {
                    minimum = Long.parseLong(node.getNodeValue());
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'minimum' found.");
                }
            } else if("maximum".equals(name)) {
                if (node == null) {
                    throw getException(typeElement, "Invalid value of element 'maximum' found.");
                }
                try {
                    maximum = Long.parseLong(node.getNodeValue());
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'maximum' found.");
                }
            } else if("default".equals(name)) {
                if (node == null) {
                    throw getException(typeElement, "Invalid value of element 'default' found.");
                }
                try {
                    defaultValue = Long.parseLong(node.getNodeValue());
                    foundDefault = true;
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'default' found.");
                }
            } else if ("dimension".equals(name)) {
                dimension = getDimension(node.getParentNode());
            }
        }
        if(foundDefault) {
            metaNode = new MetaValueLong(null, defaultValue, maximum, minimum,
                    dimension);
        } else {
            throw getException(typeElement, "Element 'default' not found for contentType 'LONG'");
        }
        return metaNode;
    }

    private static MetaValueSize parseMetaValueSize(Element typeElement) throws MetaException {
        MetaValueSize metaNode = null;
        NodeList list;
        Node node;
        String name, dimension;
        long minimum, maximum, defaultValue;

        boolean foundDefault  = false;

        minimum      = Long.MIN_VALUE;
        maximum      = Long.MAX_VALUE;
        defaultValue = 0;
        dimension = null;

        list = typeElement.getChildNodes();

        for(int i=0; i<list.getLength(); i++) {
            node = list.item(i);
            name = node.getNodeName();
            node = node.getFirstChild();


            if("minimum".equals(name)) {
                if (node == null) {
                    throw getException(typeElement, "Invalid value of element 'minimum' found.");
                }
                try {
                    minimum = createLongValuefromSizeValue(node.getNodeValue());
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'minimum' found.");
                }
            } else if("maximum".equals(name)) {
                if (node == null) {
                    throw getException(typeElement, "Invalid value of element 'maximum' found.");
                }
                try {
                    maximum = createLongValuefromSizeValue(node.getNodeValue());
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'maximum' found.");
                }
            } else if("default".equals(name)) {
                if (node == null) {
                    throw getException(typeElement, "Invalid value of element 'default' found.");
                }
                try {
                    defaultValue = createLongValuefromSizeValue(node
                            .getNodeValue());
                    foundDefault = true;
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'default' found.");
                }
            } else if ("dimension".equals(name)) {
                dimension = getDimension(node.getParentNode());
            }
        }
        if(foundDefault) {
            metaNode = new MetaValueSize(null, defaultValue, maximum, minimum,
                    dimension);
        } else {
            throw getException(typeElement, "Element 'default' not found for contentType 'SIZE'");
        }
        return metaNode;
    }

    private static MetaValueBoolean parseMetaValueBoolean(Element typeElement) throws MetaException {
        MetaValueBoolean metaNode = null;
        NodeList list;
        Node node;
        String name, dimension;
        boolean defaultValue;

        boolean foundDefault  = false;
        defaultValue          = false;
        dimension = null;


        list = typeElement.getChildNodes();

        for(int i=0; i<list.getLength(); i++) {
            node = list.item(i);
            name = node.getNodeName();
            node = node.getFirstChild();

            if("default".equals(name)) {
                if(node == null) {
                    throw getException(typeElement, "Invalid value of element 'default' found.");
                }
                try {
                    defaultValue = Boolean.parseBoolean(node.getNodeValue());
                    foundDefault = true;
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'default' found.");
                }
            } else if ("dimension".equals(name)) {
                dimension = getDimension(node.getParentNode());
            }
        }
        if(foundDefault) {
            metaNode = new MetaValueBoolean(null, defaultValue, dimension);
        } else {
            throw getException(typeElement, "Element 'default' not found for contentType 'BOOLEAN'");
        }
        return metaNode;
    }

    private static MetaValueString parseMetaValueString(Element typeElement) throws MetaException {
        MetaValueString metaNode = null;
        NodeList list;
        Node node;
        String name, defaultValue, dimension;
        int maxLength;

        boolean foundDefault   = false;
        boolean foundMaxLength = false;

        defaultValue = null;
        maxLength = 0;
        dimension = null;

        list = typeElement.getChildNodes();

        for(int i=0; i<list.getLength(); i++) {
            node = list.item(i);
            name = node.getNodeName();
            node = node.getFirstChild();

            if("maxLength".equals(name)) {
                if(node == null) {
                    throw getException(typeElement, "Invalid value of element 'maxLength' found.");
                }
                try {
                    maxLength = Integer.parseInt(node.getNodeValue());
                    foundMaxLength = true;
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'maxLength' found.");
                }
            } else if("default".equals(name)) {
                if(node == null) {
                    defaultValue = "";
                    foundDefault = true;
                } else {
                    try {
                        defaultValue = node.getNodeValue();
                        foundDefault = true;
                    } catch(NumberFormatException exc) {
                        throw getException(typeElement, "Invalid value of element 'default' found.");
                    }
                }
            } else if ("dimension".equals(name)) {
                dimension = getDimension(node.getParentNode());
            }
        }
        if(foundDefault && foundMaxLength) {
            metaNode = new MetaValueString(null, defaultValue, maxLength,
                    dimension);
        } else if(!foundDefault){
            throw getException(typeElement, "Element 'default' not found for contentType 'STRING'");
        } else {
            throw getException(typeElement, "Element 'maxLength' not found for contentType 'STRING'");
        }
        return metaNode;
    }

    private static MetaValueEnum parseMetaValueEnum(Element typeElement) throws MetaException {
        MetaValueEnum metaNode = null;
        String defaultValue = null;
        String dimension = null;
        ArrayList<String> values = new ArrayList<String>();
        NodeList list;
        Node node;
        String name;

        boolean foundDefault = false;
        list = typeElement.getChildNodes();

        for(int i=0; i<list.getLength(); i++) {
            node = list.item(i);
            name = node.getNodeName();
            node = node.getFirstChild();

            if("value".equals(name)) {
                if(node == null) {
                    throw getException(typeElement, "Invalid value of element 'value' found.");
                }
                try {
                    values.add(node.getNodeValue());
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'value' found.");
                }
            } else if("default".equals(name)) {
                if(node == null) {
                    throw getException(typeElement, "Invalid value of element 'default' found.");
                }
                try {
                    defaultValue = node.getNodeValue();
                    foundDefault = true;
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'default' found.");
                }
            } else if ("dimension".equals(name)) {
                dimension = getDimension(node.getParentNode());
            }
        }
       if(foundDefault) {
            metaNode = new MetaValueEnum(null, defaultValue, values, dimension);
       }  else {
           throw getException(typeElement, "Element 'default' not found for contentType 'ENUM'");
       }
       return metaNode;

    }

    private static MetaValueInt parseMetaValueInt(Element typeElement) throws MetaException {
        MetaValueInt metaNode = null;
        NodeList list;
        Node node;
        String name, dimension;
        int minimum, maximum, defaultValue;

        boolean foundDefault  = false;

        minimum      = Integer.MIN_VALUE;
        maximum      = Integer.MAX_VALUE;
        defaultValue = 0;
        dimension = null;

        list = typeElement.getChildNodes();

        for(int i=0; i<list.getLength(); i++) {
            node = list.item(i);
            name = node.getNodeName();
            node = node.getFirstChild();

            if("minimum".equals(name)) {
                if(node == null) {
                    throw getException(typeElement, "Invalid value of element 'minimum' found.");
                }
                try {
                    minimum = Integer.parseInt(node.getNodeValue());
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'minimum' found.");
                }
            } else if("maximum".equals(name)) {
                if(node == null) {
                    throw getException(typeElement, "Invalid value of element 'maximum' found.");
                }
                try {
                    maximum = Integer.parseInt(node.getNodeValue());
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'maximum' found.");
                }
            } else if("default".equals(name)) {
                if(node == null) {
                    throw getException(typeElement, "Invalid value of element 'default' found.");
                }
                try {
                    defaultValue = Integer.parseInt(node.getNodeValue());
                    foundDefault = true;
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'default' found.");
                }
            } else if ("dimension".equals(name)) {
                dimension = getDimension(node.getParentNode());
            }
        }
        if(foundDefault) {
            metaNode = new MetaValueInt(null, defaultValue, maximum, minimum,
                    dimension);
        } else {
            throw getException(typeElement, "Element 'default' not found for contentType 'INT'");
        }
        return metaNode;
    }

    private static MetaValueDouble parseMetaValueDouble(Element typeElement) throws MetaException {
        MetaValueDouble metaNode = null;
        NodeList list;
        Node node;
        String name, dimension;
        double minimum, maximum, defaultValue;

        boolean foundDefault  = false;

        minimum      = Double.MIN_VALUE;
        maximum      = Double.MAX_VALUE;
        defaultValue = 0;
        dimension = null;

        list = typeElement.getChildNodes();

        for(int i=0; i<list.getLength(); i++) {
            node = list.item(i);
            name = node.getNodeName();
            node = node.getFirstChild();

            if("minimum".equals(name)) {
                if(node == null) {
                    throw getException(typeElement, "Invalid value of element 'minimum' found.");
                }
                try {
                    minimum = Double.parseDouble(node.getNodeValue());
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'minimum' found.");
                }
            } else if("maximum".equals(name)) {
                if(node == null) {
                    throw getException(typeElement, "Invalid value of element 'maximum' found.");
                }
                try {
                    maximum = Double.parseDouble(node.getNodeValue());
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'maximum' found.");
                }
            } else if("default".equals(name)) {
                if(node == null) {
                    throw getException(typeElement, "Invalid value of element 'default' found.");
                }
                try {
                    defaultValue = Double.parseDouble(node.getNodeValue());
                    foundDefault = true;
                } catch (NumberFormatException exc) {
                    throw getException(typeElement,
                            "Invalid value of element 'default' found.");
                }
            } else if ("dimension".equals(name)) {
                dimension = getDimension(node.getParentNode());
            }
        }
        if(foundDefault) {
            metaNode = new MetaValueDouble(null, defaultValue, maximum,
                    minimum, dimension);
        } else {
            throw getException(typeElement, "Element 'default' not found for contentType 'DOUBLE'");
        }
        return metaNode;
    }


    private static String parseComment(Element parent) {
        String comment = null;
        Node commentNode = null;
        NodeList list = parent.getChildNodes();
        NodeList childs;
        StringBuffer buf;

        for(int i=0; (i<list.getLength()) && (commentNode == null); i++) {
            if("comment".equals(list.item(i).getNodeName())) {
                childs = list.item(i).getChildNodes();
                buf = new StringBuffer();
                for (int j=0; j<childs.getLength(); j++) {
                    commentNode = childs.item(j);
                    if (commentNode != null) {
                        buf.append(commentNode.getNodeValue().trim());
                    }
                }
                comment = buf.toString();
            }
        }
        return comment;
    }

    private static MetaException getException(Node node, String text) {
        String message;

        if(node instanceof Element) {
            message = "Error within element '" + node.getNodeName() + "'. ";
        } else if(node instanceof Attr) {
            message = "Error within attribute '" + node.getNodeName() + "', specified value '" + node.getNodeValue() + "'. ";
        } else {
            message = "";
        }
        return new MetaException(text + ". " + message + "Path: '"
                + getPath(node) + "'.",
                MetaExceptionType.META_CONFIG_PARSE_ERROR);
    }

    private static String getNodePlusNameAttr(Node node) {
        if (node == null) {
            return "";
        }
        String result = node.getNodeName();
        NamedNodeMap attrs = node.getAttributes();

        if (attrs != null) {
            Node attr = attrs.getNamedItem("name");

            if (attr != null) {
                result += "[@name=\"" + attr.getNodeValue() + "\"]";
            }
        }
        return result;
    }

    private static String getPath(Node node) {
        Node parent;
        String result = getNodePlusNameAttr(node);

        if(result == null) {
            result = "";
        }
        parent = node.getParentNode();

        while(parent != null) {
            if(parent instanceof Document) {
                result = "/" + result;
            } else {
                result = getNodePlusNameAttr(parent) + "/" + result;
            }
            parent = parent.getParentNode();
        }
        return result;
    }
}
