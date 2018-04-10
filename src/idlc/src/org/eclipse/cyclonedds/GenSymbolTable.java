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
package org.eclipse.cyclonedds;

import java.io.*;
import java.util.*;

import org.antlr.v4.runtime.*;
import org.antlr.v4.runtime.tree.TerminalNode;

import org.eclipse.cyclonedds.parser.IDLParser;

public class GenSymbolTable extends org.eclipse.cyclonedds.parser.IDLBaseVisitor <Void>
{
  private static class TD
  {
    private void invalidate ()
    {
      valid = false;
    }
    private boolean isValid ()
    {
      return valid;
    }
    private void setInteger ()
    {
      isint = true;
    }
    private boolean isInteger ()
    {
      return isint;
    }
    private void setNonintConst ()
    {
      isnonint = true;
    }
    private boolean isNonintConst ()
    {
      return isnonint;
    }

    private boolean isint = false;
    private boolean valid = true;
    private boolean isnonint = false;
  }

  public GenSymbolTable (IdlParams params)
  {
    currentscope = new ScopedName ();
    currentstruct = null;
    currentTD = null;
    inIntConst = false;
    inNonintConst = false;
    inSwitch = false;
    declarations = new HashSet <ScopedName> ();
    errorcount = 0;
    this.params = params;
  }

  public int getErrorCount ()
  {
    return errorcount;
  }

  public Void visitModule (IDLParser.ModuleContext ctx)
  {
    Void result;
    currentscope.addComponent (ctx.ID().getText());
    result = super.visitModule (ctx);
    currentscope.popComponent ();
    return result;
  }

  public Void visitInterface_decl (IDLParser.Interface_declContext ctx)
  {
    Void result;
    currentscope.addComponent (ctx.interface_header ().ID ().getText ());
    result = super.visitInterface_body (ctx.interface_body ());
    currentscope.popComponent ();
    return result;
  }

  public Void visitStruct_type (IDLParser.Struct_typeContext ctx)
  {
    Void result;
    boolean declared;
    StructSymbol oldstruct = currentstruct;
    currentscope.addComponent (ctx.ID ().getText ());
    currentstruct = new StructSymbol (new ScopedName (currentscope));
    result = super.visitStruct_type (ctx);
    declared = declarations.remove (currentscope);
    if (currentstruct.isValid ())
    {
      params.symtab.add (currentstruct);
    }
    else
    {
      if (params.lax && declared)
      {
        printErr
        (
          ctx.KW_STRUCT ().getSymbol ().getLine (),
          "Definition of predeclared struct " + currentscope +
            " contains unsupported data types"
        );
      }
    }
    currentstruct = oldstruct;
    currentscope.popComponent ();
    return result;
  }

  public Void visitSimple_declarator (IDLParser.Simple_declaratorContext ctx)
  {
    if (currentstruct != null)
    {
      StructSymbol stype = null;
      try
      {
        IDLParser.MemberContext mem = (IDLParser.MemberContext)ctx.getParent ().getParent ().getParent ();
        if (mem.type_spec ().simple_type_spec () != null)
        {
          IDLParser.Scoped_nameContext snctx = mem.type_spec ().simple_type_spec ().scoped_name ();
          if (snctx != null)
          {
            stype = (StructSymbol)stresolve (snfromctx (snctx), isRelative (snctx));
          }
        }
        else
        {
          if (mem.type_spec ().constr_type_spec () != null)
          {
            IDLParser.Struct_typeContext stctx = mem.type_spec ().constr_type_spec ().struct_type ();
            if (stctx != null)
            {
              ScopedName stname = new ScopedName (currentscope);
              stname.addComponent (stctx.ID ().getText ());
              stype = (StructSymbol)params.symtab.getSymbol (stname);
            }
          }
        }
      }
      catch (Exception ex)
      {
      }
      if (stype != null)
      {
        currentstruct.addStructMember (ctx.ID ().getText (), stype);
      }
      else
      {
        currentstruct.addMember (ctx.ID ().getText ());
      }
    }
    return super.visitSimple_declarator (ctx);
  }

  public Void visitArray_declarator (IDLParser.Array_declaratorContext ctx)
  {
    if (currentstruct != null)
    {
      currentstruct.addMember (ctx.ID ().getText ());
    }
    return super.visitArray_declarator (ctx);
  }

  public Void visitPragma_decl (IDLParser.Pragma_declContext ctx)
  {
    int line = ctx.LINE_PRAGMA().getSymbol ().getLine ();
    StringTokenizer pragma = new StringTokenizer (ctx.LINE_PRAGMA().getText ());
    pragma.nextToken (); /* Skip "#pragma" */
    if (pragma.hasMoreTokens () && pragma.nextToken ().equals ("keylist"))
    {
      String topicType = pragma.nextToken ();
      ScopedName topicSN = new ScopedName (topicType);
      Symbol topic;
      if (topicType.startsWith ("::"))
      {
        topic = params.symtab.resolve (null, topicSN);
      }
      else
      {
        topic = params.symtab.resolve (currentscope, topicSN);
      }
      if (topic == null || !(topic instanceof StructSymbol))
      {
        printErr
        (
          line,
          "unable to resolve topic " + topicType + " for key, in scope " +
          currentscope
        );
      }
      else
      {
        StructSymbol topicStruct = (StructSymbol)topic;
        String field;
        while (pragma.hasMoreTokens ())
        {
          field = pragma.nextToken ();
          if (!topicStruct.hasMember (field))
          {
            printErr
              (line, "keyfield " + field + " is either missing or unsupported");
          }
        }
      }
    }
    return super.visitPragma_decl (ctx);
  }

  public Void visitEnum_type (IDLParser.Enum_typeContext ctx)
  {
    Void result;
    currentscope.addComponent (ctx.ID ().getText ());
    params.symtab.add (new EnumSymbol (new ScopedName (currentscope)));
    result = super.visitEnum_type (ctx);
    currentscope.popComponent ();
    return result;
  }

  public Void visitEnumerator (IDLParser.EnumeratorContext ctx)
  {
    ScopedName SN = new ScopedName (currentscope);
    SN.popComponent ();
    SN.addComponent (ctx.ID ().getText ());
    params.symtab.add (new IntConstSymbol (SN));
    return super.visitEnumerator (ctx);
  }

  public Void visitType_declarator (IDLParser.Type_declaratorContext ctx)
  {
    ScopedName newscope;
    TypeDeclSymbol newTD;
    Void result;

    currentTD = new TD ();
    result = super.visitType_declarator (ctx);

    if (currentTD.isValid ())
    {
      for (IDLParser.DeclaratorContext dctx : ctx.declarators ().declarator ())
      {
        newscope = new ScopedName (currentscope);
        if (dctx.simple_declarator () != null)
        {
          newscope.addComponent (dctx.simple_declarator ().ID ().getText ());
          newTD = new TypeDeclSymbol
          (
            newscope,
            ctx.type_spec (),
            null,
            currentTD.isInteger (),
            currentTD.isNonintConst ()
          );
        }
        else
        {
          newscope.addComponent
            (dctx.complex_declarator ().array_declarator ().ID ().getText ());
          newTD = new TypeDeclSymbol
          (
            newscope,
            ctx.type_spec (),
            dctx.complex_declarator ().array_declarator ().fixed_array_size (),
            currentTD.isInteger (),
            currentTD.isNonintConst ()
          );
        }
        params.symtab.add (newTD);
      }
    }
    currentTD = null;
    return result;
  }

  private boolean isStructMember (IDLParser.Scoped_nameContext ctx)
  {
    try
    {
      return ctx.getParent ().getParent ().getParent () instanceof IDLParser.MemberContext;
    }
    catch (NullPointerException ex)
    {
    }
    return false;
  }

  private ScopedName snfromctx (IDLParser.Scoped_nameContext ctx)
  {
    ScopedName result = new ScopedName ();
    for (TerminalNode element : ctx.ID ())
    {
      result.addComponent (element.getSymbol ().getText ());
    }
    return result;
  }

  private boolean isRelative (IDLParser.Scoped_nameContext ctx)
  {
    return ctx.ID ().size () > ctx.DOUBLE_COLON ().size ();
  }

  private Symbol stresolve (ScopedName sn, boolean relative)
  {
    return params.symtab.resolve ((relative ? currentscope : null), sn);
  }

  public Void visitScoped_name (IDLParser.Scoped_nameContext ctx)
  {
    Symbol target;
    ScopedName requestSN = snfromctx (ctx);
    boolean relative = isRelative (ctx);
    int line = ctx.ID (0).getSymbol ().getLine ();

    target = stresolve (requestSN, relative);

    if (target == null)
    {
      if (isStructMember (ctx) || inIntConst)
      {
        printErr (line, requestSN + " is not defined.");
      }
      else if (relative)
      {
        boolean found;
        ScopedName s = new ScopedName (currentscope);
        do
        {
          found = declarations.contains (s.catenate (requestSN));
        }
        while (!found && !s.popComponent().equals (""));
        if (!found)
        {
          printErr (line, "unable to resolve name " + requestSN + " in scope " + currentscope);
        }
      }
      else
      {
        if (!declarations.contains (requestSN))
        {
          printErr (line, "unable to resolve name " + requestSN);
        }
      }
    }
    else
    {
      boolean targetint = target instanceof IntConstSymbol;
      boolean targetnonint = target instanceof OtherConstSymbol;
      if (target instanceof TypeDeclSymbol)
      {
        targetint = ((TypeDeclSymbol)target).isInteger ();
        targetnonint = ((TypeDeclSymbol)target).isNonintConst ();
      }
      if (currentTD != null)
      {
        if (targetint)
        {
          currentTD.setInteger ();
        }
        if (targetnonint)
        {
          currentTD.setNonintConst ();
        }
      }

      boolean valid = true;
      if (inIntConst || inSwitch)
      {
        valid = targetint;
      }
      else
      if (inNonintConst)
      {
        valid = targetnonint;
      }
      if (!valid)
      {
        printErr (line, "scoped name " + target.name () + " does not refer to a valid type");
      }
    }

    return super.visitScoped_name (ctx);
  }

  public Void visitConstr_forward_decl (IDLParser.Constr_forward_declContext ctx)
  {
    ScopedName decl = new ScopedName (currentscope);
    decl.addComponent (ctx.ID ().getText ());
    declarations.add (decl);
    return super.visitConstr_forward_decl (ctx);
  }

  public Void visitCase_label (IDLParser.Case_labelContext ctx)
  {
    Void result;
    inSwitch = true;
    result = super.visitCase_label (ctx);
    inSwitch = false;
    return result;
  }

  public Void visitLiteral (IDLParser.LiteralContext ctx)
  {
    if (inIntConst)
    {
      if (ctx.INTEGER_LITERAL () == null && ctx.HEX_LITERAL () == null && ctx.OCTAL_LITERAL () == null)
      {
        printErr
        (
          ctx.getStart ().getLine (),
          "non-integer literal in integer constant definition"
        );
      }
    }
    return super.visitLiteral (ctx);
  }

  public Void visitConst_decl (IDLParser.Const_declContext ctx)
  {
    Void result;
    boolean iic = inIntConst;
    boolean inic = inNonintConst;
    boolean integral = false;
    boolean valid = true;

    IDLParser.Const_typeContext const_type = ctx.const_type ();
    currentscope.addComponent (ctx.ID ().getText ());
    if (const_type.integer_type() != null || const_type.octet_type () != null)
    {
      integral = true;
    }
    else
    {
      if (const_type.scoped_name() != null)
      {
        IDLParser.Scoped_nameContext snctx = const_type.scoped_name ();
        Symbol stype = stresolve (snfromctx (snctx), isRelative (snctx));
        if (stype instanceof TypeDeclSymbol)
        {
          if (((TypeDeclSymbol)stype).isInteger ())
          {
            integral = true;
          }
          else if (((TypeDeclSymbol)stype).isNonintConst ())
          {
            integral = false;
          }
          else
          {
            valid = false;
            printErr
            (
              ctx.KW_CONST ().getSymbol ().getLine (),
              "typedef " + stype.name () + " is not valid for const declaration"
            );
          }
        }
        else
        {
          // Error will be generated upstream
        }
      }
      else
      {
        integral = false;
      }
    }

    if (valid)
    {
      if (integral)
      {
        params.symtab.add (new IntConstSymbol (new ScopedName (currentscope)));
        inIntConst = true;
      }
      else
      {
        params.symtab.add
          (new OtherConstSymbol (new ScopedName (currentscope)));
        inNonintConst = true;
      }
    }

    result = super.visitConst_decl (ctx);
    currentscope.popComponent ();
    inIntConst = iic;
    inNonintConst = inic;
    return result;
  }

  public Void visitPositive_int_const (IDLParser.Positive_int_constContext ctx)
  {
    Void result;
    boolean iic = inIntConst;
    inIntConst = true;
    result = super.visitPositive_int_const (ctx);
    inIntConst = iic;
    return result;
  }

  public Void visitUnion_type (IDLParser.Union_typeContext ctx)
  {
    Void result;
    currentscope.addComponent (ctx.ID ().getText ());
    params.symtab.add (new UnionSymbol (new ScopedName (currentscope)));
    result = super.visitUnion_type (ctx);
    currentscope.popComponent ();
    return result;
  }

  /* Unsupported types */

  private void bogusType (String reason, TerminalNode where)
  {
    if (currentstruct != null)
    {
      if (params.lax)
      {
        currentstruct.invalidate ();
      }
      else
      {
        printErr (where.getSymbol ().getLine (), reason);
      }
    }
    if (currentTD != null)
    {
      if (params.lax)
      {
        currentTD.invalidate ();
      }
      else
      {
        printErr (where.getSymbol ().getLine (), reason);
      }
    }
  }

  /* Unsupported but mappable types */

  public Void visitWide_char_type (IDLParser.Wide_char_typeContext ctx)
  {
    if (!params.mapwide)
    {
      bogusType
        ("wide char data not supported in" + Project.name, ctx.KW_WCHAR ());
    }
    return super.visitWide_char_type (ctx);
  }

  public Void visitWide_string_type (IDLParser.Wide_string_typeContext ctx)
  {
    if (!params.mapwide)
    {
      bogusType
        ("wide string data not supported in " + Project.name, ctx.KW_WSTRING ());
    }
    return super.visitWide_string_type (ctx);
  }

  /* These next ones NYI or potentially so */

  public Void visitFixed_pt_type (IDLParser.Fixed_pt_typeContext ctx)
  {
    bogusType
      ("fixed point data not supported in " + Project.name, ctx.KW_FIXED ());
    return super.visitFixed_pt_type (ctx);
  }

  public Void visitSequence_type (IDLParser.Sequence_typeContext ctx)
  {
    if (ctx.positive_int_const() != null)
    {
      bogusType
        ("bounded sequences not supported in " + Project.name, ctx.KW_SEQUENCE ());
    }
    return super.visitSequence_type (ctx);
  }

  /* And these aren't DDS */

  public Void visitAny_type (IDLParser.Any_typeContext ctx)
  {
    bogusType ("any data not valid for DDS", ctx.KW_ANY ());
    return super.visitAny_type (ctx);
  }

  public Void visitObject_type (IDLParser.Object_typeContext ctx)
  {
    bogusType ("Object data not valid for DDS", ctx.KW_OBJECT ());
    return super.visitObject_type (ctx);
  }

  public Void visitValue_base_type (IDLParser.Value_base_typeContext ctx)
  {
    bogusType ("ValueBase data not valid for DDS", ctx.KW_VALUEBASE ());
    return super.visitValue_base_type (ctx);
  }

  /* Verification of const types */

  public Void visitInteger_type (IDLParser.Integer_typeContext ctx)
  {
    if (currentTD != null)
    {
      currentTD.setInteger ();
    }
    return super.visitInteger_type (ctx);
  }

  public Void visitFloating_pt_type (IDLParser.Floating_pt_typeContext ctx)
  {
    if (ctx.KW_LONG () != null && !params.mapld)
    {
      bogusType
        ("long double data not supported in " + Project.name, ctx.KW_LONG ());
    }
    if (currentTD != null)
    {
      currentTD.setNonintConst ();
    }
    return super.visitFloating_pt_type (ctx);
  }

  public Void visitBoolean_type (IDLParser.Boolean_typeContext ctx)
  {
    if (currentTD != null)
    {
      currentTD.setNonintConst ();
    }
    return super.visitBoolean_type (ctx);
  }

  public boolean unresolvedSymbols ()
  {
    if (declarations.isEmpty ())
    {
      return false;
    }
    else
    {
      System.err.print ("Error: The following declarations were not defined:");
      for (ScopedName decl : declarations)
      {
        System.err.print (" " + decl);
      }
      System.err.println ();
      return true;
    }
  }

  private void printErr (int line, String err)
  {
    System.err.println
      ("Error: At " + params.linetab.getRealPosition (line) + ", " + err);
    errorcount++;
  }

  private IdlParams params;
  private ScopedName currentscope;
  private StructSymbol currentstruct;
  private TD currentTD;
  private boolean inIntConst, inNonintConst, inSwitch;
  private Set <ScopedName> declarations;
  private int errorcount;
}
