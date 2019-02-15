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
package org.eclipse.cyclonedds.config.data;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.OutputKeys;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerConfigurationException;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;

import org.eclipse.cyclonedds.common.util.Report;
import org.eclipse.cyclonedds.config.meta.MetaAttribute;
import org.eclipse.cyclonedds.config.meta.MetaConfiguration;
import org.eclipse.cyclonedds.config.meta.MetaElement;
import org.eclipse.cyclonedds.config.meta.MetaNode;
import org.eclipse.cyclonedds.config.meta.MetaValue;
import org.w3c.dom.Attr;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.Text;
import org.xml.sax.ErrorHandler;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;

public class DataConfiguration {
    private DataElement rootElement;
    private ArrayList<DataElement> services;
    private ArrayList<DataValue>           serviceNames;
    private final ArrayList<Node>          commercialServices = new ArrayList<Node>();
    private Document document;
    private MetaConfiguration metadata;
    private Set<DataConfigurationListener> listeners;
    private File file;
    private boolean fileUpToDate;

    public DataConfiguration(File file, boolean repair) throws DataException {
        try {
            if (MetaConfiguration.getInstance() != null) {
                this.metadata = MetaConfiguration.getInstance();
                this.rootElement = null;
                this.services = new ArrayList<DataElement>();
                this.serviceNames = new ArrayList<DataValue>();

                DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
                DocumentBuilder builder = factory.newDocumentBuilder();

                builder.setErrorHandler(new ErrorHandler() {
                    @Override
                    public void warning(SAXParseException exception) throws SAXException {

                    }

                    @Override
                    public void error(SAXParseException exception) throws SAXException {
                        Report.getInstance().writeErrorLog("Parse error at line: " + exception.getLineNumber() + " column: "
                                + exception.getColumnNumber() + ".");

                    }

                    @Override
                    public void fatalError(SAXParseException exception) throws SAXException {
                        Report.getInstance().writeErrorLog("Parse error at line: " + exception.getLineNumber() + " column: "
                                + exception.getColumnNumber() + ".");
                    }
                });
                this.document = builder.parse(file);
                this.listeners = Collections.synchronizedSet(new HashSet<DataConfigurationListener>());
                if (repair) {
                    this.fileUpToDate = false;
                } else {
                    this.fileUpToDate = true;
                }
                this.initExisting(repair);
                this.file = file;
            }
        } catch (SAXException se) {
            throw new DataException(se.getMessage());
        } catch (IOException ie) {
            throw new DataException(ie.getMessage());
        } catch (ParserConfigurationException pe) {
            throw new DataException(pe.getMessage());
        }
    }

    public DataConfiguration() throws DataException {
        try {
            if (MetaConfiguration.getInstance() != null) {
                this.metadata = MetaConfiguration.getInstance();
                this.rootElement = null;
                this.services = new ArrayList<DataElement>();
                this.serviceNames = new ArrayList<DataValue>();
                this.listeners = Collections.synchronizedSet(new HashSet<DataConfigurationListener>());
                this.fileUpToDate = false;
                this.file = null;
                this.initDocument();
                this.init();
            } else {
                throw new DataException("Failed to get metaconfiguration instance");
            }
        } catch (NullPointerException npe){
            throw new DataException(npe.getMessage());
        }
    }

    public Document getDocument() {
        return this.document;
    }

    public MetaConfiguration getMetadata() {
        return this.metadata;
    }

    public DataElement getRootElement() {
        return this.rootElement;
    }


    public DataElement[] getServices() {
        return this.services.toArray(new DataElement[this.services.size()]);
    }

    public ArrayList<DataElement> getServicesObject() {
        return this.services;
    }

    public ArrayList<DataValue> getServiceNames() {
        return this.serviceNames;
    }

    public String getCommandforService(String name) {
        return metadata.getCommandForService(name);
    }

    public String getServiceForCommand(String name) {
        return metadata.getServiceForCommand(name);
    }

    public void addDataConfigurationListener(DataConfigurationListener listener){
        synchronized(this.listeners){
            if(this.listeners.contains(listener)){
                assert false;
            }

            this.listeners.add(listener);
        }
    }

    public void removeDataConfigurationListener(DataConfigurationListener listener){
        synchronized(listeners){
            listeners.remove(listener);
        }
    }

    public void addNode(DataElement parent, DataNode child) throws DataException {
        if((parent.getOwner().equals(this))){
            parent.addChild(child);

            if(parent.equals(this.rootElement)){
                if(child instanceof DataElement){
                    this.services.add((DataElement)child);
                }
            }
            this.fileUpToDate = false;
            this.notifyNodeAdded(parent, child);
        } else {
            throw new DataException("Parent node not in configuration.");
        }
    }

    public void addNode(DataElement parent, MetaNode child) throws DataException {
        if((parent.getOwner().equals(this))){
            DataNode added = this.createDataForMeta(parent, child);

            if(parent.equals(this.rootElement)){
                if(added instanceof DataElement){
                    this.services.add((DataElement)added);
                }
            }
            this.fileUpToDate = false;
            this.notifyNodeAdded(parent, added);
        } else {
            throw new DataException("Parent node not in configuration.");
        }
    }

    public DataNode addNodeWithDependency(DataElement parent, MetaNode child) throws DataException {
        DataNode result = null;
        if ((parent.getOwner().equals(this))) {
            DataNode added = this.createDataForMeta(parent, child);

            if (parent.equals(this.rootElement)) {
                if (added instanceof DataElement) {
                    this.services.add((DataElement) added);
                }
            }
            this.fileUpToDate = false;
            this.notifyNodeAdded(parent, added);
            result = added;
        } else {
            throw new DataException("Parent node not in configuration.");
        }
        return result;
    }

    public boolean isServiceElement(DataNode dn) throws DataException {
        boolean result = false;
        if (dn == null) {
            throw new DataException("Invalid node.");
        }
        if (dn instanceof DataElement) {
            result = this.services.contains(dn);
        }
        return result;
    }

    public boolean containsServiceName(String name) throws DataException {
        boolean result = false;
        for (DataElement de : services) {
            DataNode[] dn = de.getChildren();
            for (int i = 0; i < dn.length; i++) {
                if (dn[i] instanceof DataAttribute) {
                    if (((DataAttribute) dn[i]).getValue().equals(name)) {
                        result = true;
                    }
                }
            }
        }
        return result;
    }

    public void createDomainServiceForSerivce(DataNode dataNode, MetaNode metaNode, String serviceName)
            throws DataException {
        MetaElement metaDomainService = findMetaElement((MetaElement) rootElement.getMetadata(), "Service");
        MetaAttribute maDomainName = findMetaAttribute(metaDomainService, "name");
        MetaElement maDomainCommand = findMetaElement(metaDomainService, "Command");
        MetaAttribute maServiceName = findMetaAttribute((MetaElement) metaNode, "name");

        /*
         * find the parent dataElement of the new to be created domain service
         * DataElement
         */
        DataElement dataDomain = findDataElement(rootElement, "Domain");
        /* get the newly created domain element */
        dataDomain = (DataElement) addNodeWithDependency(dataDomain, metaDomainService);

        /*
         * find name and command DataValue objects for the domain element
         */
        if (maDomainName != null && maDomainCommand != null) {
            DataValue domainNameValue = findDataValueforMetaValue(dataDomain, maDomainName.getValue());
            DataValue domainCommandValue = findDataValueforMetaValue(dataDomain, ((MetaValue) maDomainCommand
                    .getChildren()[0]));
            /* set name and command for Domain element */
            domainNameValue.setValue(serviceName);
            domainCommandValue.setValue(getCommandforService(((MetaElement) metaNode).getName()));

            /* find name DataValue object for service element */
            DataValue serviceNameValue = findDataValueforMetaValue((DataElement) dataNode, maServiceName.getValue());

            /* set name for service Element */
            serviceNameValue.setValue(serviceName);

            getServiceNames().add(serviceNameValue);
            getServiceNames().add(domainNameValue);

            /* set dependencies */
            domainNameValue.addDataValueDependency(serviceNameValue);
            serviceNameValue.addDataValueDependency(domainNameValue);
            dataNode.addDependency(dataDomain);
            dataDomain.addDependency(dataNode);
        } else {
            throw new DataException("Failed to set the domain tag for service: " + serviceName);
        }
    }

    public void removeNode(DataNode child) throws DataException {
        if(child == null){
            throw new DataException("Invalid node.");
        }
        DataElement parent = (DataElement)child.getParent();

        if(parent == null){
            if((child.getOwner().equals(this))){
                parent = this.rootElement;
                parent.removeChild(child);

                if(child instanceof DataElement){
                    this.services.remove(child);
                }
                this.fileUpToDate = false;
                if (commercialServices.contains(child.getNode())) {
                    commercialServices.remove(child.getNode());
                }
                this.notifyNodeRemoved(parent, child);
            } else {
                throw new DataException("Node not in configuration.");
            }
        } else if(parent.getOwner().equals(this)){
            parent.removeChild(child);

            if(parent.equals(this.rootElement)){
                if(child instanceof DataElement){
                    this.services.remove(child);
                }
            }
            this.fileUpToDate = false;
            if (commercialServices.contains(child.getNode())) {
                commercialServices.remove(child.getNode());
            }
            this.notifyNodeRemoved(parent, child);
        } else {
            throw new DataException("Parent and/or child node not in configuration.");
        }
    }

    public void setFile(File file) throws DataException{
        if(file == null){
            throw new DataException("Invalid file pointer provided");
        }
        this.file = file;
    }

    public File getFile(){
        return this.file;
    }

    public boolean isUpToDate(){
        return this.fileUpToDate;
    }

    public void store(boolean replaceOld) throws DataException{
        OutputStreamWriter writer = null;
        if(this.file == null){
            throw new DataException("No file specified.");
        }
        if((this.file.exists()) && (replaceOld == false)){
            throw new DataException("File already exists.");
        }
        if(!this.file.exists()){
            try {
                this.file.createNewFile();
            } catch (IOException e) {
                throw new DataException("Cannot create file: " + file.getAbsolutePath());
            }
        } else if(!file.canWrite()){
            throw new DataException("Cannot write to: " + file.getAbsolutePath());
        }
        try {

            for (Node n : commercialServices) {
                n.getParentNode().removeChild(n);
            }
            writer = new OutputStreamWriter(new FileOutputStream(this.file, false), "UTF-8");
            writer.write(this.toString());
            writer.close();

        } catch (IOException e) {
            throw new DataException(e.getMessage());
        } finally {
            if (writer != null) {
                try {
                    writer.close();
                } catch (IOException e) {
                    throw new DataException(e.getMessage());
                }
            }
        }
        this.fileUpToDate = true;
    }

    public void setValue(DataValue dataValue, Object value) throws DataException {
        Object oldValue;

        if(dataValue.getOwner().equals(this)){
            oldValue = dataValue.getValue();
            dataValue.setValue(value);
            this.fileUpToDate = false;
            this.notifyValueChanged(dataValue, oldValue, value);
        } else {
            throw new DataException("Parent and/or child node not in configuration.");
        }
    }

    @Override
    public String toString(){
        String result = "";

        try{
            TransformerFactory tFactory = TransformerFactory.newInstance();
            tFactory.setAttribute("indent-number", Integer.valueOf(4));
            Transformer transformer = tFactory.newTransformer();
            transformer.setOutputProperty(OutputKeys.OMIT_XML_DECLARATION, "yes");
            transformer.setOutputProperty(OutputKeys.METHOD, "xml");
            transformer.setOutputProperty(OutputKeys.INDENT, "yes");

            this.beautify(this.document.getDocumentElement());

            DOMSource source = new DOMSource(this.document);

            StringWriter writer = new StringWriter();
            StreamResult sr = new StreamResult(writer);
            transformer.transform(source, sr);
            writer.flush();
            result = writer.toString();
        } catch (TransformerConfigurationException tce) {
            Report.getInstance().writeErrorLog(tce.getMessage());
        } catch (TransformerException te) {
            Report.getInstance().writeErrorLog(te.getMessage());
        }
        return result;
    }

    public DataNode createDataForMeta(DataElement parent, MetaNode metaNode) throws DataException {
        DataNode node;
        Document dom;
        Node domNode;
        Text text;
        DataNode result = null;

        dom = this.getDocument();

        if(metaNode instanceof MetaElement){
            domNode = dom.createElement(((MetaElement)metaNode).getName());
            node = new DataElement((MetaElement)metaNode, (Element)domNode);
            result = parent.addChild(node);

            for(MetaNode mn: ((MetaElement)metaNode).getChildren()){
                if(mn instanceof MetaAttribute){
                    if(((MetaAttribute)mn).isRequired()){
                        this.createDataForMeta((DataElement) node, mn);
                    }
                } else if(mn instanceof MetaElement){
                    for(int i=0; i<((MetaElement)mn).getMinOccurrences(); i++){
                        this.createDataForMeta((DataElement)node, mn);
                    }
                } else if(mn instanceof MetaValue){
                    text = dom.createTextNode(((MetaValue)mn).getDefaultValue().toString());
                    DataValue dv = new DataValue((MetaValue)mn, text);
                    ((DataElement)node).addChild(dv);
                } else {
                    assert false;
                }
            }
        } else if(metaNode instanceof MetaAttribute){
            domNode = dom.createAttribute(((MetaAttribute)metaNode).getName());
            node = new DataAttribute((MetaAttribute)metaNode, (Attr)domNode);
            result = parent.addChild(node);
        } else if(metaNode instanceof MetaValue){
            domNode = dom.createTextNode(((MetaValue)metaNode).getDefaultValue().toString());
            node = new DataValue((MetaValue)metaNode, (Text)domNode);
            result = parent.addChild(node);
        } else {
            throw new DataException("Unknown meta type: " + metaNode.getClass());
        }
        return result;
    }

    private void initDocument() throws DataException {
        try {
            this.document    = DocumentBuilderFactory.newInstance().newDocumentBuilder().newDocument();
        } catch (ParserConfigurationException e) {
            throw new DataException(e.getMessage());
        } catch (NullPointerException npe){
            throw new DataException(npe.getMessage());
        }
    }

    private void initExisting(boolean repair) throws DataException{
        Element domElement = null;
        MetaElement metaElement = null;

        if (this.metadata != null && this.document != null) {
            metaElement = this.metadata.getRootElement();
            domElement = this.document.getDocumentElement();
        }

        if ((domElement != null) && (metaElement != null) && (metaElement.getName().equals(domElement.getNodeName()))) {
            this.rootElement = new DataElement(metaElement, domElement);
            this.rootElement.setOwner(this);
            this.initExistingDataElement(metaElement, this.rootElement, domElement, repair);

            for(DataNode dn: this.rootElement.getChildren()){
                if(dn instanceof DataElement){
                    this.services.add((DataElement)dn);
                }
            }
        } else if(repair && (domElement == null)){
            this.initDocument();
            this.init();
        } else if (repair && metaElement != null) {
            this.document.renameNode(domElement, null, metaElement.getName());
            this.initExisting(repair);
        } else {
            if (domElement != null && metaElement != null) {
                throw new DataException("RootElement should be: '" + metaElement.getName() + "', but is '"
                        +
                        domElement.getNodeName() + "'.");
            } else if (metaElement != null) {
                throw new DataException("RootElement should be: '" +
                        metaElement.getName() + "', but is 'null'.");
            } else {
                throw new DataException("RootElement is 'null'.");
            }
        }
    }

    private void initExistingDataElement(MetaElement metadata, DataElement parent, Element element, boolean repair) throws DataException {
        int occurrences;
        NodeList children;
        NamedNodeMap attributes;
        Node child;
        Element childElement;
        Text childText;
        Text oldText;
        Attr childAttribute;
        MetaElement childMetaElement;
        MetaValue childMetaValue;
        MetaAttribute childMetaAttribute;
        DataElement childDataElement;
        DataValue childDataValue;
        DataAttribute childDataAttribute;

        children = element.getChildNodes();
        int count = children.getLength();
        for(int i=0; i<count; i++){
            child = children.item(i);

            if(child instanceof Element){
                childElement = (Element)child;
                childMetaElement = this.findChildElement(metadata, childElement.getNodeName());

                if(childMetaElement != null){
                    childDataElement = new DataElement(childMetaElement, childElement);
                    try {
                        parent.addChild(childDataElement, 0, null);
                    } catch (DataException de) {
                        if(!repair){
                            throw de;
                        }
                    }
                    childDataElement.setOwner(this);
                    this.initExistingDataElement(childMetaElement, childDataElement, childElement, repair);
                }
            } else if(child instanceof Text){
                childText = (Text)child;
                childMetaValue = this.findChildValue(metadata);
                oldText = childText;

                if(childMetaValue != null){
                    try {
                        childDataValue = new DataValue(childMetaValue, childText);
                    } catch (DataException de) {
                       if(repair){
                           childText = this.document.createTextNode(childMetaValue.getDefaultValue().toString());
                           childDataValue = new DataValue(childMetaValue, childText);
                       } else {
                           throw de;
                       }
                    }
                    try {
                        if (repair && childText != oldText) {
                            parent.addChild(childDataValue, 2, oldText.getWholeText());
                            this.fileUpToDate = false;
                        } else {
                            parent.addChild(childDataValue, 0, oldText.getWholeText());
                        }
                    } catch (DataException de) {
                        if(!repair){
                            throw de;
                        }
                    }
                }
            }
        }
        attributes = element.getAttributes();

        for(int i=0; i<attributes.getLength(); i++){
            child = attributes.item(i);

            if(child instanceof Attr){
                childAttribute = (Attr)child;
                childMetaAttribute = this.findChildAttribute(metadata, childAttribute.getName());

                if(childMetaAttribute != null){
                    try {
                        childDataAttribute = new DataAttribute(childMetaAttribute, childAttribute, childAttribute.getNodeValue());
                    } catch (DataException de) {
                        if(repair){
                            childDataAttribute = new DataAttribute(childMetaAttribute, childAttribute, childMetaAttribute.getValue().getDefaultValue());
                        } else {
                            throw de;
                        }
                    }
                    try {
                        parent.addChild(childDataAttribute, 0, null);
                    } catch (DataException de) {
                        if(!repair){
                            throw de;
                        }
                    }
                }
            }
        }
        /*All data has been added as far as it goes. Now check if data is missing*/
        for(MetaNode mn: metadata.getChildren()){
            if(mn instanceof MetaElement){
                childMetaElement = (MetaElement)mn;
                occurrences = this.countChildElementOccurrences((Element)parent.getNode(), childMetaElement.getName());

                if(occurrences < childMetaElement.getMinOccurrences()){
                    if(repair){
                        while(occurrences < childMetaElement.getMinOccurrences()){
                            childElement = this.document.createElement(childMetaElement.getName());
                            element.appendChild(childElement);
                            childDataElement = new DataElement(childMetaElement, childElement);
                            parent.addChild(childDataElement);
                            this.initDataElement(childDataElement);
                            occurrences++;
                        }
                        occurrences = 0;
                    } else {
                        throw new DataException("Found only " + occurrences + " occurrences for element '"
                                +
                                childMetaElement.getName() +
 "', but expected at least "
                                +
                                childMetaElement.getMinOccurrences());
                    }
                }
            } else if(mn instanceof MetaAttribute){
                childMetaAttribute = (MetaAttribute)mn;

                if(childMetaAttribute.isRequired()){
                    if(!element.hasAttribute(childMetaAttribute.getName())){
                        childAttribute = this.document.createAttribute(childMetaAttribute.getName());
                        childDataAttribute = new DataAttribute(childMetaAttribute, childAttribute,
                                childMetaAttribute.getValue().getDefaultValue());
                        parent.addChild(childDataAttribute);
                    }
                }
            } else if(mn instanceof MetaValue){
                childMetaValue = (MetaValue)mn;
                occurrences = this.countChildElementOccurrences((Element)parent.getNode(), null);

                if(occurrences == 0){
                    childText = this.document.createTextNode(childMetaValue.getDefaultValue().toString());
                    childDataValue = new DataValue(childMetaValue, childText);
                    parent.addChild(childDataValue);
                }
            }
        }
        assert parent.getOwner() != null: "Owner == null";

        for(DataNode dn: parent.getChildren()){
            assert dn.getOwner() != null: "Owner == null (2)";
        }
    }

    private void init() throws DataException{
        Element domElement, de;
        MetaElement metaElement;
        DataElement dataElement;
        int occurrences = 0;

        if (this.metadata != null && this.document != null) {
            metaElement = this.metadata.getRootElement();
            domElement = this.document.createElement(metaElement.getName());
        } else {
            throw new DataException("RootElement is 'null'.");
        }
        this.document.appendChild(domElement);

        this.rootElement = new DataElement(metaElement, domElement);
        this.rootElement.setOwner(this);
        //this.initDataElement(this.rootElement);

        for(MetaElement me: this.metadata.getServices()){
            while(occurrences < me.getMinOccurrences()){
                de = this.document.createElement(me.getName());
                domElement.appendChild(de);
                dataElement = new DataElement(me, de);
                this.rootElement.addChild(dataElement);
                this.initDataElement(dataElement);
                this.services.add(dataElement);
                occurrences++;
            }
            occurrences = 0;
        }
    }

    private void initDataElement(DataElement data) throws DataException{
        MetaAttribute ma;
        MetaElement me;
        MetaValue mv;
        DataAttribute da;
        DataElement de;
        DataValue dv;
        Attr attribute;
        Element element;
        Text text;
        int occurrences;

        MetaElement meta = (MetaElement)data.getMetadata();
        occurrences = 0;

        for(MetaNode mn: meta.getChildren()){
            if(mn instanceof MetaAttribute){
                ma = (MetaAttribute)mn;

                if(ma.isRequired()){
                    attribute = this.document.createAttribute(ma.getName());
                    da        = new DataAttribute(ma, attribute, ma.getValue().getDefaultValue());
                    data.addChild(da);
                }
            } else if(mn instanceof MetaElement){
                me = (MetaElement)mn;

                while(occurrences < me.getMinOccurrences()){
                    element = this.document.createElement(me.getName());
                    de      = new DataElement(me, element);
                    data.addChild(de);
                    this.initDataElement(de);
                    occurrences++;
                }
                occurrences = 0;
            } else if(mn instanceof MetaValue){
                mv   = (MetaValue)mn;
                text = this.document.createTextNode(mv.getDefaultValue().toString());
                dv   = new DataValue(mv, text);
                data.addChild(dv);
            }
        }
    }

    private MetaElement findChildElement(MetaElement element, String name) {
        MetaElement child = null;
        MetaNode[] children = element.getChildren();

        for(int i=0; (i<children.length) && (child==null); i++){
            if(children[i] instanceof MetaElement){
                if(((MetaElement)children[i]).getName().equals(name)){
                    child = (MetaElement)children[i];
                }
            }
        }
        return child;
    }

    public MetaElement findMetaElement(MetaElement element, String name) {
        MetaElement child = null;
        MetaNode[] children = element.getChildren();

        if (element.getName().equals(name)) {
            child = element;
        }

        for (int i = 0; (i < children.length) && (child == null); i++) {
            if (children[i] instanceof MetaElement) {
                child = findMetaElement((MetaElement) children[i], name);
            }
        }
        return child;
    }

    public MetaAttribute findMetaAttribute(MetaElement element, String name) {
        MetaAttribute result = null;
        MetaNode[] children = element.getChildren();

        for (int i = 0; (i < children.length) && (result == null); i++) {
            if (children[i] instanceof MetaAttribute) {
                if (((MetaAttribute) children[i]).getName().equals(name)) {
                    result = ((MetaAttribute) children[i]);
                }
            }
        }
        return result;
    }

    public boolean setMetaAttribute(MetaAttribute attr, String value) {
        boolean result = false;
        result = attr.getValue().setDefaultValue(value);
        return result;
    }

    public boolean getMetaElement(MetaElement element, String name, String value) {
        boolean result = false;
        MetaNode[] children = element.getChildren();

        for (int i = 0; (i < children.length) && (!result); i++) {
            if (children[i] instanceof MetaElement) {
                if (((MetaElement) children[i]).getName().equals(name)) {
                    MetaNode[] childs = ((MetaElement) children[i]).getChildren();
                    if (childs.length == 1) {
                        if (childs[0] instanceof MetaValue) {
                            ((MetaValue) childs[0]).setDefaultValue(value);
                            result = true;
                        }
                    }
                }
            }
        }
        return result;
    }

    public String getMetaAttributeValue(MetaElement element, String name) {
        String result = null;
        MetaNode[] children = element.getChildren();

        for (int i = 0; (i < children.length) && (result == null); i++) {
            if (children[i] instanceof MetaAttribute) {
                if (((MetaAttribute) children[i]).getName().equals(name)) {
                    result = (String) ((MetaAttribute) children[i]).getValue().getDefaultValue();
                }
            }
        }
        return result;
    }

    public DataElement findDataElement(DataElement element, String name) {
        DataElement child = null;
        DataNode[] children = element.getChildren();

        if (element.getNode().getNodeName().equals(name)) {
            child = element;
        }

        for (int i = 0; (i < children.length) && (child == null); i++) {
            if (children[i] instanceof DataElement) {
                child = findDataElement((DataElement) children[i], name);
            }
        }
        return child;
    }

    public DataValue findDataValueforMetaValue(DataElement element, MetaValue mv) {
        DataValue child = null;
        DataNode[] children = element.getChildren();

        if (element.getNode() instanceof DataValue) {
            if (((MetaValue) element.getMetadata()).getDefaultValue().equals(mv.getDefaultValue())) {
                child = (DataValue) element.getNode();
            }
        }

        for (int i = 0; (i < children.length) && (child == null); i++) {
            if (children[i] instanceof DataElement) {
                child = findDataValueforMetaValue((DataElement) children[i], mv);
            } else if (children[i] instanceof DataValue) {
                if (((MetaValue) children[i].getMetadata()).getDefaultValue().equals(mv.getDefaultValue())) {
                    child = (DataValue) children[i];
                }
            } else if (children[i] instanceof DataAttribute) {
                if (((MetaAttribute) children[i].getMetadata()).getValue() == mv) {
                    child = ((DataAttribute) children[i]).getDataValue();
                }
            }
        }
        return child;
    }

    public DataValue findDataValueforMetaValueInCurrentElement(DataElement element, MetaValue mv) {
        DataValue child = null;
        DataNode[] children = element.getChildren();

        if (element.getNode() instanceof DataValue) {
            if (((MetaValue) element.getMetadata()).getDefaultValue().equals(mv.getDefaultValue())) {
                child = (DataValue) element.getNode();
            }
        }

        for (int i = 0; (i < children.length) && (child == null); i++) {
            if (children[i] instanceof DataValue) {
                if (((MetaValue) children[i].getMetadata()).getDefaultValue().equals(mv.getDefaultValue())) {
                    child = (DataValue) children[i];
                }
            } else if (children[i] instanceof DataAttribute) {
                if (((MetaAttribute) children[i].getMetadata()).getValue() == mv) {
                    child = ((DataAttribute) children[i]).getDataValue();
                }
            }
        }
        return child;
    }

    public DataElement findServiceDataElement(DataElement root, String name, DataElement element) {
        DataElement child = null;
        DataNode[] children = root.getChildren();

        if (root.getNode().getNodeName().equals(name)) {
            if (((MetaAttribute) root.getMetadata()).getName()
                    .equals(((MetaAttribute) element.getMetadata()).getName())) {
                child = root;
            }
        }

        for (int i = 0; (i < children.length) && (child == null); i++) {
            if (children[i] instanceof DataElement) {
                child = findDataElement((DataElement) children[i], name);
            }
        }
        return child;
    }

    private MetaValue findChildValue(MetaElement element){
        MetaValue child = null;
        MetaNode[] children = element.getChildren();

        for(int i=0; (i<children.length) && (child==null); i++){
            if(children[i] instanceof MetaValue){
                child = (MetaValue)children[i];
            }
        }
        return child;
    }

    private MetaAttribute findChildAttribute(MetaElement element, String name){
        MetaAttribute child = null;
        MetaNode[] children = element.getChildren();

        for(int i=0; (i<children.length) && (child==null); i++){
            if(children[i] instanceof MetaAttribute){
                if(((MetaAttribute)children[i]).getName().equals(name)){
                    child = (MetaAttribute)children[i];
                }
            }
        }
        return child;
    }

    private int countChildElementOccurrences(Element element, String childName){
        int occurrences = 0;
        NodeList children = element.getChildNodes();

        for(int i=0; i<children.getLength(); i++){
            if(childName == null){
                if(children.item(i).getNodeName().equals("#text")){
                    occurrences++;
                }
            } else if(childName.equals(children.item(i).getNodeName())){
                occurrences++;
            }
        }
        return occurrences;
    }

    private void notifyNodeAdded(DataElement parent, DataNode addedNode){
        synchronized(this.listeners){
            for(DataConfigurationListener listener: this.listeners){
                listener.nodeAdded(parent, addedNode);
            }
        }
    }

    private void notifyNodeRemoved(DataElement parent, DataNode removedNode){
        synchronized(this.listeners){
            for(DataConfigurationListener listener: this.listeners){
                listener.nodeRemoved(parent, removedNode);
            }
        }
    }

    public void notifyValueChanged(DataValue data, Object oldValue, Object newValue) {
        synchronized(this.listeners){
            for(DataConfigurationListener listener: this.listeners){
                listener.valueChanged(data, oldValue, newValue);
            }
        }
    }

    private void beautify(Node node){
        NodeList children;
        String value;

        if(node instanceof Element){
            children = ((Element)node).getChildNodes();

            for(int i=0; i<children.getLength(); i++){
                this.beautify(children.item(i));
            }
        } else if(node instanceof Text){
            value = node.getNodeValue();
            node.setNodeValue(value.trim());
        }
        return;
    }

    public ArrayList<Node> getCommercialServices() {
        return commercialServices;
    }
}
