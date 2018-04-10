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

/*
 * Based on CppPreprocessor by Eric Mahurin
 */

header 
{
  package org.eclipse.cyclonedds.idt.imports.idl.internal.preprocessor;
}

options
{
  language="Java";
}

{
  import java.io.*;
  import java.util.*;
  import antlr.*;
  import org.eclipse.cyclonedds.idt.imports.idl.preprocessor.IIdlPreprocessorStatus;
}

class CppLexer extends Lexer;

options 
{
  charVocabulary = '\3'..'\377';
  testLiterals = false;
  caseSensitive = true;
  noConstructors = true;
  k = 4;
}

tokens 
{
  ENDIF ;
}

{
  private List<IIdlPreprocessorStatus> m_status = new ArrayList<IIdlPreprocessorStatus> ();
  
  protected TokenStreamSelector selector;
  protected Integer ifState = 1; // -1: no-else false, 0: false, 1: true
  protected List<Integer> ifStates = new ArrayList<Integer>(); // holds nested if conditions
  protected List<File> includePath = new ArrayList<File>();
  protected List<PreprocessorFile> openFiles = new ArrayList<PreprocessorFile>();
  protected Map<String, List<String>> defines    = new HashMap<String, List<String>>(); // holds the defines
  protected Map<String, List<String>> defineArgs = new HashMap<String, List<String>>(); // holds the args for a macro call
  protected Map<File, List<File>> dependencies   = new HashMap<File, List<File>>();
  protected boolean stripComments    = true;
  protected boolean generateHashLine = true;
  protected boolean processIncludes  = true;
  protected boolean inIncludeFile    = false;
  protected boolean concat = false;
  protected boolean inMacro = false;
  protected String escaped = "";
  protected String stripped = "";
  protected PreprocessorFile currentFile;
  protected File baseDir;
  protected CppLexer parent;
  
  public CppLexer(PreprocessorFile file, File baseDir, Map<File, List<File>> dependencies, boolean generateHashLine) throws FileNotFoundException
  {
    this(file.getInputStream());
    this.setFilename(file.getOriginalPath());
    this.currentFile = file;
    this.openFiles.add(currentFile);
    this.baseDir = baseDir;
    if(dependencies != null)
      this.dependencies = dependencies;
    this.dependencies.put(file.getOriginalFile(), new ArrayList<File>());
    this.generateHashLine = generateHashLine;

    selector = new TokenStreamSelector();
    selector.select(this);
    String line = "# " + getLine() + " \"" + getFilename() + "\"\n";
    CppLexer sublexer = new CppLexer(line, this);
    selector.push(sublexer);
  }
  
  private CppLexer(String str, CppLexer parent)
  {
    this(new ByteArrayInputStream(str.getBytes()));
    this.selector = parent.selector;
    this.ifState = parent.ifState;
    this.ifStates = parent.ifStates;
    this.defines = parent.defines;
    this.defineArgs = parent.defineArgs;
    this.stripComments = parent.stripComments;
    this.generateHashLine = parent.generateHashLine;
    this.processIncludes = parent.processIncludes;
    this.inIncludeFile = parent.inIncludeFile;
    this.inMacro = parent.inMacro;
    this.m_status = parent.m_status;
    selector.push(this);
  }
  
  private CppLexer(PreprocessorFile file, CppLexer parent) throws FileNotFoundException
  {
    this(file.getInputStream());
    this.setFilename(file.getOriginalPath());
    this.dependencies = parent.dependencies;
    dependencies.put(file.getOriginalFile(), new ArrayList<File>());
    this.currentFile = file;
    this.openFiles = parent.openFiles;
    this.openFiles.add(currentFile);
    this.baseDir = parent.baseDir;

    this.includePath = parent.includePath;
    this.selector = parent.selector;
    this.ifState = parent.ifState;
    this.ifStates = parent.ifStates;
    this.defines = parent.defines;
    this.stripComments = parent.stripComments;
    this.generateHashLine = parent.generateHashLine;
    this.processIncludes = parent.processIncludes;
    this.inIncludeFile = true;
    this.m_status = parent.m_status;
    this.parent = parent;
    
    if(cycleCheck(file))
    {
      m_status.add(new PreprocessorStatus(parent, IIdlPreprocessorStatus.ERROR, "Cycle in includes; " + file + " included with itself", null));
    }
    else  
    {
      selector.push(this);
      String line = "# " + getLine() + " \"" + getFilename() + "\" 1\n";
      new CppLexer(line, this);
    }
  }
  
  private boolean cycleCheck(PreprocessorFile file)
  {
    if(parent == null)
      return false;
    else if(parent.currentFile.equals(file))
      return true;
    else
      return parent.cycleCheck(file);
  }
  
  private void include(String name)
  {
    PreprocessorFile toInclude = null;
    File temp = null;
    File currentDir = null;
        
    //Evaluate included file against directory containing the current file
    String currentFilePath = currentFile.getOriginalPath();
    if(currentFilePath.contains("/"))
    {
      currentDir = new File(currentFilePath.substring(0, currentFilePath.lastIndexOf("/"))); 
    }
    temp = new File(currentDir, name);
    if(currentDir != null && currentDir.isAbsolute())
    {
      toInclude = new PreprocessorFile(null, temp.getPath());
    }
    else
    {
      toInclude = new PreprocessorFile(baseDir, temp.getPath());
    }
        
    //Search include paths for included file
    if(!toInclude.exists())
    {       
      for(File dir:includePath)
      {
        temp = new File(dir, name);
        if(dir.isAbsolute())
        {
          toInclude = new PreprocessorFile(null, temp.getPath());
        }
        else
        {
          toInclude = new PreprocessorFile(baseDir, temp.getPath());
        }
          
        if(toInclude.exists())
        {    
          break;
        }
      }
    }
 
    List<File> deps = dependencies.get(currentFile.getOriginalFile());
    deps.add(toInclude.getOriginalFile());        
    try
    {  
      String line = "# " + getLine() + " \"" + getFilename() + "\" 2\n";
      new CppLexer(line, this);
      new CppLexer(toInclude, this);
    } 
    catch (FileNotFoundException fnf) 
    {
      setLine(getLine()-1);
      reportError("Could not find file " + name);
      newline();
    }
  }
  
  public void close()
  {
    for(PreprocessorFile toClose : openFiles)
    {
      toClose.close();
    }
  }
  
  public void setTokenStreamSelector(TokenStreamSelector selector)
  {
    this.selector = selector;
  }

  public void setProcessIncludes(boolean processIncludes)
  {
    this.processIncludes = processIncludes;
  }
  
  public void setCurrentFile(PreprocessorFile currentFile)
  {
   this.currentFile = currentFile;
  }
  
  public void setDefines(Map<String, List<String>> defines)
  {
    this.defines = defines;
  }
  
  public void setDependencies(Map<File, List<File>> dependencies)
  {
    this.dependencies = dependencies;
  }
  
  public void setIncludePath(List<File> includePath)
  {
    this.includePath = includePath;
  }
  
  public void setBaseDir(File baseDir)
  {
   this.baseDir = baseDir;
  }
  
  public void setStripComments(boolean stripComments)
  {
    this.stripComments = stripComments;
  }
  
  public Map<File, List<File>> getDependencies()
  {
    return dependencies;
  }

  public boolean evaluateExpression(String expr) throws TokenStreamException, RecognitionException, PreprocessorException
  {
    //Perform macro substitution on the expression
    expr = subLex(expr);
    //Parse the expression
    ExpLexer lexer = new ExpLexer(new ByteArrayInputStream(expr.getBytes()));
    ExpParser parser = new ExpParser(this, lexer);
    IIdlPreprocessorStatus parseStatus = parser.parse();

    if(!parseStatus.isOK())
    {
      m_status.add(parseStatus);
      throw new PreprocessorException();
    }

    //Evaluate the expression
    ExpTreeParser treeParser = new ExpTreeParser(this);
    boolean result = treeParser.expression(parser.getAST());
    IIdlPreprocessorStatus treeParseStatus = treeParser.getStatus();

    if(!treeParseStatus.isOK())
    {
      m_status.add(treeParseStatus);
      throw new PreprocessorException();
    }

    return result;
  }
  
  public String subLex(String str) throws TokenStreamException, RecognitionException, PreprocessorException
  {
   CppLexer cppLexer = new CppLexer(new ByteArrayInputStream(str.getBytes()));
    cppLexer.defines = defines;
    cppLexer.selector = new TokenStreamSelector();
    cppLexer.selector.select(cppLexer);  
    
    IIdlPreprocessorStatus cppStatus = cppLexer.getStatus();
    if(!cppStatus.isOK())
    {
      m_status.add(cppStatus);
      throw new PreprocessorException();
    }

    str = "";
    for (;;)
    {
      Token t = cppLexer.getNextToken();
      if (t.getType() == Token.EOF_TYPE)
      {
        break;
      }
      str += t.getText();
    }
   return str;
  }
  
  public void consumeConditionBody() throws TokenStreamException
  {
   String consumed = "";
   int lineNo = getLine();
   for (;;) 
    {
      try 
      {
       consumeUntil('#');
        Token t = selector.nextToken();
        if (t.getType() == ENDIF || t.getType() == EOF) 
        {
          break;
        }
      }
      catch (ANTLRException r) 
      {
        // just continue if someone tried retry
      }
    } 
    for(int i = lineNo; i < getLine(); i++)
    {
     consumed += "\n";
    }
    new CppLexer(consumed, this);    
  }

  public Token getNextToken() throws TokenStreamException
  {
   try
   {
     Token t = selector.nextToken();
     return t;
   }
    catch (TokenStreamException e)
    {
      m_status.add(new PreprocessorStatus(this, IIdlPreprocessorStatus.ERROR, e.getMessage(), e));
      throw e;
    }
  }
  
  public void uponEOF() throws TokenStreamException
  {
    try 
    {
      selector.pop(); // return to old lexer/stream
      selector.retry();
    } 
    catch (NoSuchElementException e) 
    {
      // return a real EOF if nothing in stack
    }
    if(ifStates.size() != 0)
    {
      ifStates.remove(ifStates.size()-1);
      reportError("Reached EOF expecting #endif");
    }
  }
  
  public IIdlPreprocessorStatus getStatus()
  {
    switch (m_status.size())
    {
      case 0 :
      {
        return new PreprocessorStatus(IIdlPreprocessorStatus.OK);
      }
      case 1 :
      {
        return m_status.get(0);
      }
      default :
      {
        PreprocessorStatus status = new PreprocessorStatus();
        for (IIdlPreprocessorStatus s : m_status)
          status.add (s);
        return status;
      }
    }
  } 
  
  @Override
  public void reportError(RecognitionException recex)
  {
    m_status.add(new PreprocessorStatus(this, IIdlPreprocessorStatus.ERROR, recex.getMessage(), recex));
  }
  
  @Override
  public void reportError(String message)
  {
    m_status.add(new PreprocessorStatus(this, IIdlPreprocessorStatus.ERROR, message, null));
  }
  
  @Override
  public void reportWarning(String message)
  {
    m_status.add(new PreprocessorStatus(this, IIdlPreprocessorStatus.WARNING, message, null));
  }
}

DIRECTIVE
{
  List<String> args = new ArrayList<String>();
  boolean condition = true;
} 
: "#"! ( { inMacro && LA(1)=='#'}?
    {
      concat = true;
    }
    | { inMacro }? (WS)* hashExpr:EXPR!
    {
      if(ifState == 1)
      {
        if(concat)
        {
          concat = false;
          CppLexer subLexer = new CppLexer(hashExpr.getText(), this);
        }
        else
        {
          CppLexer subLexer = new CppLexer('"' + hashExpr.getText() + '"', this);
        }
      }     
    }
    | (WS)* ("include" (WS)+ includeFile:INCLUDE_STRING (WS)* DNL
            {
              if (ifState==1)
              {
                $setType(Token.SKIP);
                String name = includeFile.getText();
                name = name.substring(1,name.length()-1);
                include(name);        
                selector.retry();   
              }       
            }
            |"define" (WS)+ defineMacro:RAW_IDENTIFIER (WS)* {args.add("");}
            (
              ( '('
                (WS)* defineArg0:RAW_IDENTIFIER (WS)* {args.add(defineArg0.getText());}
                ( COMMA (WS)* defineArg1:RAW_IDENTIFIER (WS)* {args.add(defineArg1.getText());} )*
                ')'
                | WS
              )     
              (WS)* 
              defineText:MACRO_TEXT {args.set(0,defineText.getText());}
            )? NL
            { 
              if (ifState==1) 
              {
                defines.put( defineMacro.getText(), args );
              }
              $setText("\n" + escaped);
              escaped = "";
            }
            |"undef" (WS)+ undefMacro:RAW_IDENTIFIER (WS)* DNL
            { 
              if (ifState==1) 
              {
                defines.remove(undefMacro.getText());
              }
              $setText("\n");
            }
            |("ifdef"|"ifndef"{condition=false;}) (WS)+ ifMacro:RAW_IDENTIFIER (WS)* DNL
            {
              ifStates.add(ifState);
              if (ifState==1) 
              {
                condition = (defines.containsKey(ifMacro.getText())==condition);
                ifState = condition?1:0;
              }
              else 
              {
                ifState = -1;
              }
              if (ifState==1) 
              {
                $setText("\n");
              } 
              else 
              {
                // gobble up tokens until ENDIF (could be caused by else)
                consumeConditionBody();
              }
            }
            | "if" (WS)+ ifExpr:MACRO_TEXT NL
            {
              ifStates.add(ifState);
              if (ifState==1) 
              {
                try
                {
                  condition = evaluateExpression(ifExpr.getText());;
                  ifState = condition?1:0;
                }
                catch(PreprocessorException p)
                {
                  // failed to evaluate expression set ifState so the rest of the ifstatement is discarded 
                 ifState = -1;
                }
              }
              else 
              {
                ifState = -1;
              }
              if (ifState==1) 
              {
                $setText("\n");
              } 
              else 
              {
                // gobble up tokens until ENDIF (could be caused by else)
                consumeConditionBody();
              } 
            }
            | ( "else" {condition=true;} (WS)* DNL // treat like elif (true)
                |"elif" (WS)+ expr:MACRO_TEXT NL
                {
                  try
                  {
                    condition=evaluateExpression(expr.getText());
                  }
                  catch(PreprocessorException p)
                  {
                    // failed to evaluate expression set ifState so the rest of the ifstatement is discarded 
                    ifState = 1;
                  }
                }
              )
              {
                if (ifState==1) 
                {
                  // previous if/elif was taken - discard rest
                  ifState = -1;
                  consumeConditionBody();
                } 
                else if (ifState==0 && condition) 
                {
                  // "elif" (true) or "else"
                  $setText("\n");
                  $setType(ENDIF);
                  ifState = 1;
                }
              }
            | "endif" (WS)* DNL
            {
              condition = (ifState==1);
              try 
              {
                // return to previous if state
                ifState = ifStates.remove(ifStates.size()-1);
                $setText("\n");
                if (!condition)
                {
                  // tell if/else/elif to stop discarding tokens
                  $setType(ENDIF);
                }
              } 
              catch (ArrayIndexOutOfBoundsException e) 
              {
                reportError("#endif without matching #if");
              }
            }
            | "line" (WS)+ ln:NUMBER ((WS)+ file:STRING)? (WS)* DNL
            {
              if(ifState==1)
              {
                $setType(Token.SKIP);
                String line = "# " + ln.getText();
                if(file != null)
                {
                  String filename = file.getText();
                  line += " " + filename;
                  setFilename(filename.substring(1,filename.length()-1));
                }
                else
                {
                  line += " \"" + getFilename() + "\"";
                }
                line += "\n";
                setLine(Integer.parseInt(ln.getText()));
  
                CppLexer subLexer = new CppLexer(line, this);
                selector.retry();
              }
            }
            | "error" (WS)+ errMsg:MACRO_TEXT NL
            {
              if(ifState == 1)
              {
                reportError(errMsg.getText());
              }
            }
            | "warning" (WS)+ warnMsg:MACRO_TEXT NL
            {
              if(ifState == 1)
              {
                $setText("\n" + escaped);
                escaped = "";
                reportWarning(warnMsg.getText());
              }
            }
            | "pragma" (WS)+ pragma:MACRO_TEXT NL
            {
              if(inIncludeFile && !processIncludes)
              {
                $setType(Token.SKIP);
              }
              else
              {
               try
               {
                 String pText = pragma.getText() + '\n';
                 $setText("#pragma " + subLex(pText));
               }
               catch(PreprocessorException e)
               {
                 //Status already updated ignore error and continue
                 $setText('\n');
               }
              }
            }
            | l:NUMBER (WS)+ f:STRING ((WS)+ flag:NUMBER)? (WS)* DNL
            {
              if(!generateHashLine) 
              {
                $setText('\n');
              }
              else
              {
               String fl = (flag == null) ? "" : " " + flag.getText();
               $setText("# " + l.getText() + " " + f.getText() + fl + '\n');
              }
            }
            | DNL
            {
              $setText('\n');
            }
            )
  )
;

NON_DIRECTIVE
{
  if(inIncludeFile && !processIncludes)
    $setType(Token.SKIP);
}
: (STRING | NUMBER | COMMENT | LEFT | RIGHT | COMMA | OPERATOR | WS)
;

IDENTIFIER options {testLiterals=true;} 
{
  List<String> define = new ArrayList<String>();
  List<String> args = new ArrayList<String>();
  if(inIncludeFile && !processIncludes)
    $setType(Token.SKIP);
} 
: identifier:RAW_IDENTIFIER
  {
    define = defineArgs.get(identifier.getText());
    if (_createToken && define==null) 
    {
      define = defines.get(identifier.getText());
    }
  }
  ( { (define!=null) && (define.size()>1) }? (WS)*
    '(' (WS)*
    callArg0:EXPR {args.add(callArg0.getText());}
    ( (WS)* COMMA (WS)* callArg1:EXPR {args.add(callArg1.getText());} )*
    { args.size()==define.size()-1 }? // better have right amount
    (WS)* ')'
    | { !((define!=null) && (define.size()>1)) }?
  )
  { 
    if (define!=null) 
    {
      String defineText = define.get(0);
      if (!_createToken) 
      {
        $setText(defineText);
      } 
      else 
      {
        for (int i=0;i<args.size();++i) 
        {
          List<String> arg = new ArrayList<String>();
          arg.add(args.get(i));
          defineArgs.put(define.get(1+i), arg);
        }
        inMacro=true;
        CppLexer sublexer = new CppLexer(defineText, this);
        selector.retry();
        inMacro=false;
      }
    }
  }
;

protected STRING
    : '"' ( '\\' . | ~('\\'|'"') )* '"' // double quoted string
    | '\'' ( '\\' . | ~('\\'|'\'') )* '\'' // single quoted string
    ;

protected INCLUDE_STRING
    : '<' (~'>')* '>'
    | STRING
    ;

protected MACRO_TEXT : ( options{greedy=false;} : . | ESC_NL )+;
    
protected WS: (' ' | '\t' | '\f');
protected DNL: (COMMENT | NL);
NL: (CR)? '\n' {newline();};
CR!: ('\r')+;
protected ESC_NL : '\\'! NL! {escaped += "\n";};

protected COMMENT 
  : ( "//" ( options{greedy=false;} : . )* NL {if(stripComments || (inIncludeFile && !processIncludes))stripped+="\n";} // single line comment
    | "/*" ( options{greedy=false;} : NL {if(stripComments || (inIncludeFile && !processIncludes))stripped+="\n";} 
                                    | . {if(stripComments || (inIncludeFile && !processIncludes))stripped+=" ";})* "*/" // multi-line comment
    )
    {
     if(stripComments || (inIncludeFile && !processIncludes))
     {
      $setText(stripped); 
      stripped="";
     }
    }
    ;

protected RAW_IDENTIFIER : ('a'..'z'|'A'..'Z'|'_') ('a'..'z'|'A'..'Z'|'_'|'0'..'9')* ;

protected INT_SUFFIX:  ("l"|"L"|"u"|"U"|"lu"|"Lu"|"lU"|"LU"|"ul"|"Ul"|"uL"|"UL");  
protected DEC_DIGITS:  ('1'..'9') ('0'..'9')*; 
protected HEX_PREFIX:  '0' ('x'|'X');
protected HEX_DIGITS:  ('0'..'9' | 'a'..'f' | 'A'..'F')+;
protected OCT_PREFIX:  '0';
protected OCT_DIGITS:  ('0'..'7')*;  
protected DEC_INT: DEC_DIGITS (INT_SUFFIX)? ;
protected HEX_INT: HEX_PREFIX HEX_DIGITS (INT_SUFFIX)? ;  
protected OCT_INT: OCT_PREFIX OCT_DIGITS (INT_SUFFIX)? ;
protected NUMBER : (DEC_INT | HEX_INT | OCT_INT); // allow alpha suffixes on numbers (i.e. L:long)

// group symbols into categories to parse EXPR
protected LEFT  : '(' | '[' | '{' ;
protected RIGHT : ')' | ']' | '}' ;
protected COMMA : ',' ;
protected OPERATOR : '!' | '$' | '%' | '&' | '*' | '+' | '-' | '.' | '/' | ':' | ';' | '<' | '=' | '>' | '?' | '@' | '\\' | '^' | '`' | '|' | '~' ;
DEFINED : "defined" ((LEFT) RAW_IDENTIFIER (RIGHT)| (WS)+ RAW_IDENTIFIER);

protected EXPR // allow just about anything without being ambiguous
    : (WS)? (NUMBER|IDENTIFIER)?
        (
            ( LEFT EXPR ( COMMA EXPR )* RIGHT
            | STRING
            | OPERATOR // quotes, COMMA, LEFT, and RIGHT not in here
            | WS
            )
            EXPR
        )?
    ;
