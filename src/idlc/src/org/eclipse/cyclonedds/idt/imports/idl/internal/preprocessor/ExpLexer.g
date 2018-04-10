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

header 
{
  package org.eclipse.cyclonedds.idt.imports.idl.internal.preprocessor;
}

options
{
  language = "Java";
}

class ExpLexer extends Lexer;
options 
{
    k=2;
}

RAW_IDENTIFIER:  ('a'..'z'|'A'..'Z'|'_') ('a'..'z'|'A'..'Z'|'_'|'0'..'9')*;
protected INT_SUFFIX:  ("l"|"L"|"u"|"U"|"lu"|"Lu"|"lU"|"LU"|"ul"|"Ul"|"uL"|"UL");  
protected DEC_DIGITS:  ('1'..'9') ('0'..'9')*; 
protected HEX_PREFIX:  '0' ('x'|'X');
protected HEX_DIGITS:  ('0'..'9' | 'a'..'f' | 'A'..'F')+;
protected OCT_PREFIX:  '0';
protected OCT_DIGITS:  ('0'..'7')*;  
DEC_INT: DEC_DIGITS (INT_SUFFIX!)? ;
HEX_INT: HEX_PREFIX! HEX_DIGITS (INT_SUFFIX!)? ;  
OCT_INT: OCT_PREFIX OCT_DIGITS (INT_SUFFIX!)? ;

WS      : (' '|'\t'|'\r'|'\n') { $setType(Token.SKIP); } ;

LPAREN    : '('  ;
RPAREN    : ')'  ;
PLUS      : '+'  ;
MINUS     : '-'  ;
MULTIPLY  : '*'  ;
DIVIDE    : '/'  ;
REMAINDER : '%'  ;
COMPLIMENT: '~'  ;
LSHIFT    : "<<" ;
RSHIFT    : ">>" ;
BIT_AND   : "&"  ;
BIT_INC_OR: "|"  ;
BIT_EXC_OR: "^"  ;
OR        : "||" ;
AND       : "&&" ;
EQUAL     : "==" ;
NOT_EQUAL : "!=" ;
NOT       : '!'  ;
LT        : '<'  ;
LE        : "<=" ;
GE        : ">=" ;
GT        : '>'  ;
DEFINED   : "defined";


{
  import java.util.*;
  import org.eclipse.cyclonedds.idt.imports.idl.preprocessor.IIdlPreprocessorStatus;
}
class ExpParser extends Parser;
options 
{
    k=4;
    buildAST = true;
}
{
  private List<IIdlPreprocessorStatus> m_status = new ArrayList<IIdlPreprocessorStatus> ();
  CppLexer parent;

  public ExpParser(CppLexer parent, ExpLexer expLexer)
  {
   this(expLexer);
   this.parent = parent;
  }
  
  public IIdlPreprocessorStatus parse()
  {
    try
    {
      orExpr();
      if(LA(1) != EOF)
        m_status.add(new PreprocessorStatus(parent, IIdlPreprocessorStatus.ERROR, "Invalid token " + LA(1), null));
    }
    catch (RecognitionException e)
    {
      m_status.add(new PreprocessorStatus(parent, IIdlPreprocessorStatus.ERROR, e.getMessage(), e));
    }
    catch (TokenStreamException e)
    {
      m_status.add(new PreprocessorStatus(parent, IIdlPreprocessorStatus.ERROR, e.getMessage(), e));
    }

    return getStatus ();
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
   m_status.add(new PreprocessorStatus(parent, IIdlPreprocessorStatus.ERROR, recex.getMessage(), recex));
  }
  
  @Override
  public void reportError(String message)
  {
   m_status.add(new PreprocessorStatus(parent, IIdlPreprocessorStatus.ERROR, message, null));
  }
  
  @Override
  public void reportWarning(String message)
  {
   m_status.add(new PreprocessorStatus(parent, IIdlPreprocessorStatus.WARNING, message, null));
  } 
}

value
  : HEX_INT | DEC_INT | OCT_INT | RAW_IDENTIFIER
  ;
  
orExpr
  : andExpr (OR^ andExpr)*
  ;

andExpr
  : bitIncOr (AND^ bitIncOr)*
  ;

bitIncOr
  : bitExcOr (BIT_INC_OR^ bitExcOr)*
  ;
  
bitExcOr
  : bitAnd (BIT_EXC_OR^ bitAnd)*  
  ;
  
bitAnd
  : equal (BIT_AND^ equal)*
  ;

equal
  : compare ((EQUAL^ | NOT_EQUAL^) compare)*
  ;
    
compare
  : shift ((LT^ | LE^ | GE^ | GT^) shift)*
  ;

shift
  : addition ((LSHIFT^|RSHIFT^) addition)*  
  ;
  
addition
  : multiplication ((PLUS^|MINUS^) multiplication)*
  ;
  
multiplication
  : unary ((MULTIPLY^|DIVIDE^|REMAINDER^) unary)*
  ;
  
unary
  : (PLUS^ | MINUS^ | COMPLIMENT^)? value
  | NOT^ unary
  | DEFINED^ LPAREN! RAW_IDENTIFIER RPAREN!
  | DEFINED^ RAW_IDENTIFIER
  | LPAREN! orExpr RPAREN!
  ;


{
  import java.util.*;
  import org.eclipse.cyclonedds.idt.imports.idl.preprocessor.IIdlPreprocessorStatus;
}

class ExpTreeParser extends TreeParser;
{
  private List<IIdlPreprocessorStatus> m_status = new ArrayList<IIdlPreprocessorStatus> ();
  CppLexer parent;
  
  public ExpTreeParser(CppLexer parent)
  {
   this.parent = parent;
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
   m_status.add(new PreprocessorStatus(parent, IIdlPreprocessorStatus.ERROR, recex.getMessage(), recex));
  }
  
  @Override
  public void reportError(String message)
  {
   m_status.add(new PreprocessorStatus(parent, IIdlPreprocessorStatus.ERROR, message, null));
  }
  
  @Override
  public void reportWarning(String message)
  {
   m_status.add(new PreprocessorStatus(parent, IIdlPreprocessorStatus.WARNING, message, null));
  }
}

value returns [ int i ]
{
  int x, y;
  i = 0;
}
: #(BIT_INC_OR x=value y=value { i = x|y; } )
| #(BIT_EXC_OR x=value y=value { i = x^y; } )
| #(BIT_AND x=value y=value { i = x&y; } )
| #(LSHIFT x=value y=value { i = x<<y; } )
| #(RSHIFT x=value y=value { i = x>>y; } )
| (#(PLUS x=value y=value)) => #(PLUS x=value y=value { i = x+y; } )
| #(PLUS x=value { i = x; } )
| (#(MINUS x=value y=value)) => #(MINUS x=value y=value { i = x-y; } )
| #(MINUS x=value { i = -1*x; } )
| #(MULTIPLY x=value y=value { i = x*y; } )
| #(DIVIDE x=value y=value { i = x/y; } )
| #(REMAINDER x=value y=value { i = x%y; } )
| #(COMPLIMENT x=value { i = ~x; } )
| d:DEC_INT { i = Integer.parseInt(d.getText(), 10); }
| h:HEX_INT { i = Integer.parseInt(h.getText(), 16); }
| o:OCT_INT { i = Integer.parseInt(o.getText(), 8); }
| r:RAW_IDENTIFIER { i = 0; }
;
  
identifier returns [ String s ]
{
  s = null;
}
: i:RAW_IDENTIFIER
{
  s = i.getText();
}
;
   
expression returns [ boolean e ]
{
  boolean a, b;
  int l, r;
  String i;

  e = false;
}
: #(AND a=expression b=expression { e = a && b; } )
| #(BAND a=expression b=expression { e = a & b; } )
| #(OR  a=expression b=expression { e = a || b; } )
| #(BOR  a=expression b=expression { e = a | b; } )
| #(NOT  a=expression { e = !a; } )
| #(EQUAL l=value r=value { e = l==r; } )
| #(NOT_EQUAL l=value r=value { e = l!=r; } )
| #(LT l=value r=value { e = l<r; } )
| #(LE l=value r=value { e = l<=r; } )
| #(GE l=value r=value { e = l>=r; } )
| #(GT l=value r=value { e = l>r; } )
| #(DEFINED i=identifier { e = parent.defines.containsKey(i); } )
;
