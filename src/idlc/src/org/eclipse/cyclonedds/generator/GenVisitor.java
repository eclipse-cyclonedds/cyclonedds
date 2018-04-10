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
package org.eclipse.cyclonedds.generator;

import java.io.*;
import java.util.*;

import org.antlr.v4.runtime.*;
import org.antlr.v4.runtime.tree.*;
import org.stringtemplate.v4.*;

import org.eclipse.cyclonedds.parser.IDLParser;
import org.eclipse.cyclonedds.*;

public class GenVisitor extends org.eclipse.cyclonedds.parser.IDLBaseVisitor <Void>
{
  private static class ParseState
  {
    private ParseState ()
    {
       decl_ctx = null;
       struct = null;
       member = null;
       seqtypedef = null;
       type = null;
       structMeta = null;
       s_union = null;
       s_unionST = null;
       decls = new ArrayList <String> ();
       arraydecls = new LinkedList <ArrayDeclarator> ();
       scopes = new LinkedList <String> ();
       literal = 0;
    }

    private ParseState (ParseState state)
    {
       decl_ctx = state.decl_ctx;
       struct = state.struct;
       member = state.member;
       s_union = state.s_union;
       s_unionST = state.s_unionST;
       seqtypedef = state.seqtypedef;
       type = state.type;
       structMeta = state.structMeta;
       decls = new ArrayList <String> (state.decls);
       arraydecls = new LinkedList <ArrayDeclarator> (state.arraydecls);
       scopes = new LinkedList <String> (state.scopes);
       literal = state.literal;
    }

    private void pushScope (String newscope)
    {
      scopes.addLast (newscope);
    }

    private String popScope ()
    {
      return scopes.removeLast ();
    }

    private void setScope (ST templ)
    {
      for (String element : scopes)
      {
        templ.add ("scope", element);
      }
    }

    private ScopedName getSN ()
    {
      return new ScopedName (scopes);
    }

    private ST decl_ctx, struct, member, s_unionST, seqtypedef;
    private LinkedList <String> scopes;
    private ArrayList <String> decls;
    private LinkedList <ArrayDeclarator> arraydecls;
    private Type type;
    private StructType structMeta;
    private UnionType s_union;
    private long literal;
  }

  public GenVisitor (IdlParams params, String templates)
  {
    this.params = params;
    String basesafe = params.basename.replace ('-', '_').replace (' ', '_');
    topics = new HashMap <ScopedName, ST> ();
    alltypes = new LinkedHashMap <ScopedName, NamedType> ();
    constants = new HashMap <ScopedName, Long> ();
    group = new STGroupFile (templates);
    file = group.getInstanceOf ("file");

    ST banner = group.getInstanceOf ("banner");
    banner.add ("file", params.basename);
    if (params.timestamp)
    {
      banner.add ("date", new Date ());
    }
    banner.add ("version", Project.version);

    file.add ("banner", banner);
    if (params.forcpp)
    {
      file.add ("name", params.basename + "-cyclonedds");
    }
    else
    {
      file.add ("name", params.basename);
    }
    file.add ("nameupper", basesafe.toUpperCase ());
    params.linetab.populateIncs (file);

    if (params.dllname != null)
    {
      ST dll = group.getInstanceOf ("dlldef");
      dll.add ("name", basesafe.toUpperCase ());
      dll.add ("dllname", params.dllname);
      if (params.dllfile != null)
      {
        dll.add ("dllfile", params.dllfile);
      }
      file.add ("dll", dll);
    }

    state = new ParseState ();
    state.decl_ctx = file;
  }

  public Void visitModule (IDLParser.ModuleContext ctx)
  {
    Void result;
    ParseState oldstate = new ParseState (state);

    state.decl_ctx = group.getInstanceOf ("module");
    state.setScope (state.decl_ctx);
    state.decl_ctx.add ("name", ctx.ID ().getText ());
    state.pushScope (ctx.ID ().getText ());

    oldstate.decl_ctx.add ("declarations", state.decl_ctx);

    result = super.visitModule (ctx);
    state = oldstate;
    return result;
  }

  public Void visitInterface_decl (IDLParser.Interface_declContext ctx)
  {
    Void result;
    String name = ctx.interface_header ().ID ().getText ();
    ParseState oldstate = new ParseState (state);
    state.decl_ctx = group.getInstanceOf ("module");
    state.setScope (state.decl_ctx);
    state.decl_ctx.add ("name", name);
    state.pushScope (name);

    oldstate.decl_ctx.add ("declarations", state.decl_ctx);

    result = super.visitInterface_body (ctx.interface_body ());
    state = oldstate;
    return result;
  }

  public Void visitStruct_type (IDLParser.Struct_typeContext ctx)
  {
    Void result;
    NamedType parent =
      (state.structMeta != null) ? state.structMeta : state.s_union;
    boolean generate = params.linetab.inMain (ctx.ID ().getSymbol ().getLine ());
    ParseState oldstate = new ParseState (state);

    state.struct = group.getInstanceOf ("struct");
    state.decl_ctx = state.struct;
    state.setScope (state.struct);
    state.struct.add ("name", ctx.ID ().getText ());
    state.struct.add
      ("extern", (params.dllname != null) ? "extern DDS_EXPORT" : "extern");
    if (params.allstructs)
    {
      state.struct.add ("istopic", "true");
    }
    state.pushScope (ctx.ID ().getText ());
    state.structMeta = new StructType (state.getSN (), parent);

    result = super.visitStruct_type (ctx);

    if (state.structMeta.isInvalid ())
    {
      if (!params.quiet)
      {
        System.out.print
        (
          "Struct " + state.structMeta.getSN ().toString ("::") +
          " contains unsupported data types, skipped."
        );
      }
    }
    else
    {
      if (oldstate.member != null)
      {
        oldstate.type = state.structMeta;
        oldstate.decl_ctx.add ("declarations", state.struct);
        oldstate.member.add ("type", state.struct.getAttribute ("name"));
        oldstate.member.add ("scope", state.struct.getAttribute ("scope"));
      }
      else
      {
        if (generate)
        {
          oldstate.decl_ctx.add ("declarations", state.struct);
        }
      }
      alltypes.put (state.getSN (), state.structMeta);
      topics.put (state.getSN (), state.struct);
    }

    state = oldstate;
    return result;
  }

  public Void visitEnum_type (IDLParser.Enum_typeContext ctx)
  {
    Void result;
    EnumType etype;
    String enumname = ctx.ID ().getText ();
    boolean generate =
      params.linetab.inMain (ctx.ID ().getSymbol ().getLine ());

    ScopedName enumSN = new ScopedName (state.getSN ());
    enumSN.addComponent (enumname);
    etype = new EnumType
      (enumSN, (state.structMeta != null) ? state.structMeta : state.s_union);

    ST etmpl = group.getInstanceOf ("enum");
    state.setScope (etmpl);
    etmpl.add ("name", enumname);

    Type oldtype = state.type;
    state.type = etype;
    result = super.visitEnum_type (ctx);
    state.type = oldtype;

    for (String s : etype.getEnumerands ())
    {
      etmpl.add ("values", s);
    }

    if (state.member != null)
    {
      state.decl_ctx.add ("declarations", etmpl);
      state.member.add ("type", enumname);
      state.setScope (state.member);
      state.type = etype;
    }
    else
    {
      if (generate)
      {
        state.decl_ctx.add ("declarations", etmpl);
      }
    }
    alltypes.put (enumSN, etype);
    return result;
  }

  public Void visitEnumerator (IDLParser.EnumeratorContext ctx)
  {
    ((EnumType)state.type).addEnumerand (ctx.ID ().getText ());
    return super.visitEnumerator (ctx);
  }

  public Void visitMember (IDLParser.MemberContext ctx)
  {
    Void result;
    ST seqtd, newmember;
    String scopeprefix, seqtypename;
    Type realtype;
    boolean implicitTD;

    state.member = group.getInstanceOf ("member");
    state.decls.clear ();
    state.arraydecls.clear ();
    result = super.visitMember (ctx);
    if (state.type == null)
    {
      state.member = null;
      if (state.struct != null)
      {
        state.structMeta.invalidate ();
      }
      return result;
    }

    implicitTD =
    (
      state.seqtypedef != null &&
      state.seqtypedef.getName().equals ("/seqdef")
    );
    scopeprefix = state.getSN ().toString ("_");
    if (scopeprefix.length () > 0)
    {
      scopeprefix = scopeprefix + "_";
    }

    for (String simpledec : state.decls)
    {
      newmember = new ST (state.member);

      if (implicitTD)
      {
        seqtypename = simpledec + "_seq";
        seqtd = new ST (state.seqtypedef);
        seqtd.add ("name", seqtypename);
        state.decl_ctx.add ("declarations", seqtd);
        newmember.add ("type", seqtypename);
        realtype = ((SequenceType)state.type).clone (scopeprefix + seqtypename);
      }
      else
      {
        realtype = state.type;
      }

      newmember.add ("name", simpledec);
      state.decl_ctx.add ("fields", newmember);
      state.structMeta.addMember (simpledec, realtype);
    }

    for (ArrayDeclarator arraydec : state.arraydecls)
    {
      newmember = new ST (state.member);

      if (implicitTD)
      {
        seqtypename = arraydec.getName () + "_seq";
        seqtd = new ST (state.seqtypedef);
        seqtd.add ("name", seqtypename);
        state.decl_ctx.add ("declarations", seqtd);
        newmember.add ("type", seqtypename);
        realtype = ((SequenceType)state.type).clone (scopeprefix + seqtypename);
      }
      else
      {
        realtype = state.type;
      }
      newmember.add ("arraydim", arraydec.getDimString ());
      newmember.add ("name", arraydec.getName ());
      state.decl_ctx.add ("fields", newmember);
      state.structMeta.addMember
        (arraydec.getName (), new ArrayType (arraydec.getDims (), realtype));
    }

    state.seqtypedef = null;
    return result;
  }

  public Void visitType_declarator (IDLParser.Type_declaratorContext ctx)
  {
    /* this is a typedef */
    Void result;
    ScopedName SN;
    ST td, basetd, newmember;
    ParseState oldstate = new ParseState (state);
    String[] scopearray = state.scopes.toArray (new String[0]);
    boolean generate = params.linetab.inMain (((IDLParser.Type_declContext)ctx.getParent ()).KW_TYPEDEF ().getSymbol ().getLine ());

    state.member = group.getInstanceOf ("member");
    state.decls.clear ();
    state.arraydecls.clear ();

    result = super.visitType_declarator (ctx);

    if (state.type == null)
    {
      if (!params.quiet)
      {
        System.out.print ("Typedef ");
        for (String s : oldstate.scopes)
        {
          System.out.print (s + "::");
        }
        System.out.println (state.member.getAttribute ("name") + " is an unsupported data type, skipped.");
      }
      state = oldstate;
      return result;
    }

    if (state.seqtypedef == null)
    {
      basetd = group.getInstanceOf ("scalar_typedef");
      for (String s : scopearray)
      {
        basetd.add ("namescope", s);
      }
      copyAttrIfSet (state.member, basetd, "type");
      copyAttrIfSet (state.member, basetd, "scope");
      copyAttrIfSet (state.member, basetd, "str_size");
    }
    else
    {
      basetd = state.seqtypedef;
    }

    for (String simpledec : state.decls)
    {
      SN = new ScopedName (scopearray);
      SN.addComponent (simpledec);
      if (state.type instanceof BasicType && !(state.type instanceof EnumType))
      {
        alltypes.put (SN, new BasicTypedefType (SN, (BasicType)state.type));
      }
      else
      {
        alltypes.put (SN, new TypedefType (SN, state.type));
      }
      if (generate)
      {
        td = new ST (basetd);
        td.add ("name", simpledec);
        state.decl_ctx.add ("declarations", td);
      }
    }

    for (ArrayDeclarator arraydec : state.arraydecls)
    {
      SN = new ScopedName (scopearray);
      SN.addComponent (arraydec.getName ());
      alltypes.put (SN, new TypedefType (SN, new ArrayType (arraydec.getDims (), state.type)));

      newmember = new ST (state.member);
      newmember.add ("arraydim", arraydec.getDimString ());
      newmember.add ("name", arraydec.getName ());
      if (generate)
      {
        td = new ST (basetd);
        td.add ("name", arraydec.getName ());
        td.add ("arraydim", arraydec.getDimString ());
        state.decl_ctx.add ("declarations", td);
      }
    }

    state = oldstate;
    return result;
  }

  public Void visitSimple_declarator (IDLParser.Simple_declaratorContext ctx)
  {
    state.decls.add (ctx.ID ().getText ());
    return super.visitSimple_declarator (ctx);
  }

  public Void visitArray_declarator (IDLParser.Array_declaratorContext ctx)
  {
    state.arraydecls.addLast (new ArrayDeclarator (ctx.ID ().getText ()));
    return super.visitArray_declarator (ctx);
  }

  public Void visitFixed_array_size (IDLParser.Fixed_array_sizeContext ctx)
  {
    Void result = super.visitFixed_array_size (ctx);
    state.arraydecls.getLast ().addDimension (state.literal);
    return result;
  }

  public Void visitConst_exp (IDLParser.Const_expContext ctx)
  {
    state.literal = 0L;
    return super.visitConst_exp (ctx);
  }

  public Void visitOr_expr (IDLParser.Or_exprContext ctx)
  {
    long result = 0L;
    for (IDLParser.Xor_exprContext child : ctx.xor_expr ())
    {
      state.literal = 0L;
      visitXor_expr (child);
      result |= state.literal;
    }
    state.literal = result;
    return null;
  }

  public Void visitXor_expr (IDLParser.Xor_exprContext ctx)
  {
    long result = 0L;
    for (IDLParser.And_exprContext child : ctx.and_expr ())
    {
      state.literal = 0L;
      visitAnd_expr (child);
      result ^= state.literal;
    }
    state.literal = result;
    return null;
  }

  public Void visitAnd_expr (IDLParser.And_exprContext ctx)
  {
    long result = ~0L;
    for (IDLParser.Shift_exprContext child : ctx.shift_expr ())
    {
      state.literal = 0L;
      visitShift_expr (child);
      result &= state.literal;
    }
    state.literal = result;
    return null;
  }

  public Void visitShift_expr (IDLParser.Shift_exprContext ctx)
  {
    long result = 0L;
    int subex = 0;
    state.literal = 0L;

    visitAdd_expr (ctx.add_expr (subex++));
    result = state.literal;
    for (int i = 1; i < ctx.getChildCount (); i+=2)
    {
      state.literal = 0L;
      visitAdd_expr (ctx.add_expr (subex++));
      if (ctx.getChild (i).getText ().equals ("<<"))
      {
        result <<= state.literal;
      }
      else
      {
        result >>= state.literal;
      }
    }
    state.literal = result;
    return null;
  }

  public Void visitAdd_expr (IDLParser.Add_exprContext ctx)
  {
    long result = 0L;
    int subex = 0;
    state.literal = 0L;

    visitMult_expr (ctx.mult_expr (subex++));
    result = state.literal;
    for (int i = 1; i < ctx.getChildCount (); i+=2)
    {
      state.literal = 0L;
      visitMult_expr (ctx.mult_expr (subex++));
      if (ctx.getChild (i).getText ().equals ("+"))
      {
        result += state.literal;
      }
      else
      {
        result -= state.literal;
      }
    }
    state.literal = result;
    return null;
  }

  public Void visitMult_expr (IDLParser.Mult_exprContext ctx)
  {
    long result = 0L;
    int subex = 0;
    state.literal = 0L;

    visitUnary_expr (ctx.unary_expr (subex++));
    result = state.literal;
    for (int i = 1; i < ctx.getChildCount (); i+=2)
    {
      state.literal = 0L;
      visitUnary_expr (ctx.unary_expr (subex++));
      if (ctx.getChild (i).getText ().equals ("*"))
      {
        result *= state.literal;
      }
      else
      {
        if (state.literal == 0)
        {
          throw new NumberFormatException ("divide by zero");
        }
        if (ctx.getChild (i).getText ().equals ("/"))
        {
          result /= state.literal;
        }
        else
        {
          result %= state.literal;
        }
      }
    }
    state.literal = result;
    return null;
  }

  public Void visitUnary_expr (IDLParser.Unary_exprContext ctx)
  {
    IDLParser.Unary_operatorContext op = ctx.unary_operator ();
    visitPrimary_expr (ctx.primary_expr ());

    if (op != null)
    {
      if (op.MINUS () != null)
      {
        state.literal = -state.literal;
      }
      else if (op.TILDE () != null)
      {
        state.literal = ~state.literal;
      }
    }
    return null;
  }

  public Void visitPrimary_expr (IDLParser.Primary_exprContext ctx)
  {
    if (ctx.scoped_name () != null)
    {
      ScopedName sn = resolveName (ctx.scoped_name ().ID ());
      Long val = constants.get (sn);
      if (val != null)
      {
        state.literal = val.longValue ();
      }
      else
      {
        throw new NumberFormatException (sn.toString () + " - not integer");
      }
    }
    else
    {
      if (ctx.literal () != null)
      {
        String lit = ctx.literal ().getText ();
        if (lit.endsWith ("L") || lit.endsWith ("l"))
        {
          lit = lit.substring (0, lit.length () - 1);
        }
        state.literal = Long.decode (lit);
      }
      else
      {
        visitConst_exp (ctx.const_exp ());
      }
    }
    return null;
  }

  public Void visitOp_decl (IDLParser.Op_declContext ctx)
  {
    // Don't care about operations
    return null;
  }

  public Void visitConst_decl (IDLParser.Const_declContext ctx)
  {
    Void result = null;
    boolean integral;

    ST cdecl = group.getInstanceOf ("const");
    String constname = ctx.ID ().getText ();
    ScopedName constSN = new ScopedName (state.getSN ());
    constSN.addComponent (constname);
    state.setScope (cdecl);
    cdecl.add ("name", constname);

    integral =
      (ctx.const_type ().integer_type () != null) ||
      (ctx.const_type ().octet_type () != null);
    if (!integral)
    {
      if (ctx.const_type ().scoped_name () != null)
      {
        ScopedName typeSN =
          resolveName (ctx.const_type ().scoped_name ().ID ());
        Type ctype = alltypes.get (typeSN);
        integral = (ctype.getCType ().contains ("int"));
      }
    }
    if (integral)
    {
      visitConst_exp (ctx.const_exp ());
      cdecl.add ("expression", Long.toString (state.literal));
      constants.put (constSN, new Long (state.literal));
    }
    else
    {
      StringBuffer buf = new StringBuffer ("(");
      traverseExpression (ctx.const_exp (), buf);
      buf.append (")");
      cdecl.add ("expression", buf.toString ());
    }
    if (params.linetab.inMain (ctx.ID ().getSymbol ().getLine ()))
    {
      state.decl_ctx.add ("declarations", cdecl);
    }
    return result;
  }

  private void traverseExpression (ParseTree exp, StringBuffer result)
  {
    ParseTree child;
    for (int i = 0; i < exp.getChildCount(); i++)
    {
      child = exp.getChild (i);
      if (child instanceof IDLParser.Scoped_nameContext)
      {
        ST sn = group.getInstanceOf ("scopedname");
        ScopedName actual =
          resolveName (((IDLParser.Scoped_nameContext)child).ID ());
        sn.add ("name", actual.getLeaf ());
        for (String s : actual.getPath())
        {
          sn.add ("scope", s);
        }
        result.append (sn.render ());
      }
      else if (child instanceof IDLParser.LiteralContext)
      {
        IDLParser.LiteralContext lit = (IDLParser.LiteralContext)child;
        if (lit.BOOLEAN_LITERAL () != null)
        {
          if (lit.BOOLEAN_LITERAL ().getText ().equals ("TRUE"))
          {
            result.append ("true");
          }
          else
          {
            result.append ("false");
          }
        }
        else
        {
          result.append (child.getText ());
        }
      }
      else if (child instanceof TerminalNode)
      {
        result.append (child.getText ());
      }
      else
      {
        traverseExpression (child, result);
      }
    }
  }

  public Void visitUnion_type (IDLParser.Union_typeContext ctx)
  {
    Void result;
    NamedType parent =
      (state.structMeta != null) ? state.structMeta : state.s_union;
    boolean generate = params.linetab.inMain (ctx.ID ().getSymbol ().getLine ());
    ParseState oldstate = new ParseState (state);

    String unionname = ctx.ID ().getText ();
    ScopedName unionSN = new ScopedName (state.getSN ());
    unionSN.addComponent (unionname);

    state.s_unionST = group.getInstanceOf ("union");
    state.setScope (state.s_unionST);
    state.s_unionST.add ("name", unionname);
    state.decl_ctx = state.s_unionST;
    state.pushScope (unionname);
    state.s_union = new UnionType (unionSN, parent);

    result = super.visitUnion_type (ctx);

    if (oldstate.member != null)
    {
      oldstate.decl_ctx.add ("declarations", state.s_unionST);
      oldstate.member.add ("type", unionname);
      oldstate.setScope (state.member);
      oldstate.type = state.s_union;
    }
    else
    {
      if (generate)
      {
        oldstate.decl_ctx.add ("declarations", state.s_unionST);
      }
    }

    alltypes.put (unionSN, state.s_union);
    state = oldstate;
    return result;
  }

  public Void visitSwitch_type_spec (IDLParser.Switch_type_specContext ctx)
  {
    Void result;
    ParseState oldstate = new ParseState (state);
    state.member = group.getInstanceOf ("member");
    result = super.visitSwitch_type_spec (ctx);
    oldstate.s_unionST.add ("disc", state.member.getAttribute ("type"));
    oldstate.s_unionST.add ("discscope", state.member.getAttribute ("scope"));
    oldstate.s_union.setDiscriminant (state.type);
    state = oldstate;
    return result;
  }

  public Void visitCase_stmt (IDLParser.Case_stmtContext ctx)
  {
    boolean implicitTD;
    ParseState oldstate = new ParseState (state);
    state.member = group.getInstanceOf ("member");
    state.decls.clear ();
    state.arraydecls.clear ();

    ArrayList <String> labels = new ArrayList <String> ();
    boolean includedefault = false;

    for (IDLParser.Case_labelContext labelctx : ctx.case_label ())
    {
      if (labelctx.KW_DEFAULT () == null)
      {
        try
        {
          visitConst_exp (labelctx.const_exp ());
          labels.add (Long.toString (state.literal));
        }
        catch (NumberFormatException ex)
        {
          // could be a bool or enum discriminant
          StringBuffer buf = new StringBuffer ();
          traverseExpression (labelctx.const_exp (), buf);
          labels.add (buf.toString ());
        }
      }
      else
      {
        includedefault = true;
      }
    }

    String memname;
    Void result = visitElement_spec (ctx.element_spec ());

    implicitTD =
    (
      state.seqtypedef != null &&
      state.seqtypedef.getName().equals ("/seqdef")
    );

    if (!state.decls.isEmpty ())
    {
      memname = state.decls.get (0);
    }
    else
    {
      ArrayDeclarator dec = state.arraydecls.get (0);
      memname = dec.getName ();
      state.type = new ArrayType (dec.getDims (), state.type);
      state.member.add ("arraydim", dec.getDimString ());
    }
    state.member.add ("name", memname);

    if (implicitTD)
    {
       String seqtypename = memname + "_seq";
       String prefix = state.getSN ().toString ("_");
       if (prefix.length () > 0)
       {
         prefix = prefix + "_";
       }
       state.seqtypedef.add ("name", seqtypename);
       state.decl_ctx.add ("declarations", state.seqtypedef);
       state.member.add ("type", seqtypename);
       state.type = ((SequenceType)state.type).clone (prefix + seqtypename);
    }

    oldstate.s_unionST.add ("fields", state.member);
    oldstate.s_union.addMember (memname, state.type, labels.toArray (new String[labels.size ()]), includedefault);
    state = oldstate;
    return result;
  }

  private void copyAttrIfSet (ST from, ST to, String name)
  {
    Object attr = from.getAttribute (name);
    if (attr != null)
    {
      to.add (name, attr);
    }
  }

  private ScopedName resolveName (List<TerminalNode> query)
  {
    ScopedName current = new ScopedName ();
    ScopedName request = new ScopedName ();

    for (String s : state.scopes)
    {
      current.addComponent (s);
    }

    for (TerminalNode element : query)
    {
      request.addComponent (element.getSymbol ().getText ());
    }

    return params.symtab.resolve (current, request).name ();
  }

  public Void visitScoped_name (IDLParser.Scoped_nameContext ctx)
  {
    ScopedName resultSN;
    Type resultType;

    resultSN = resolveName (ctx.ID ());
    state.type = alltypes.get (resultSN);

    for (String s : resultSN.getPath())
    {
      state.member.add ("scope", s);
    }
    state.member.add ("type", resultSN.getLeaf());

    return super.visitScoped_name (ctx);
  }

  public Void visitSequence_type (IDLParser.Sequence_typeContext ctx)
  {
    if (ctx.positive_int_const () != null)
    {
      // Bounded sequence NYI
      return null;
    }

    Void result;
    boolean generate =
      params.linetab.inMain (ctx.KW_SEQUENCE ().getSymbol ().getLine ());
    ParseState oldstate = new ParseState (state);

    state.member = group.getInstanceOf ("member");
    result = super.visitSequence_type (ctx);

    if (state.type == null)
    {
      state = oldstate;
      return result;
    }

    oldstate.type = new SequenceType (state.type);

    Type realsub = state.type;
    while (realsub instanceof TypedefType)
    {
      realsub = ((TypedefType)realsub).getRef ();
    }

    boolean isbstring = realsub instanceof BoundedStringType;
    boolean isbasic = isbstring || (realsub instanceof BasicType);

    if (isbasic)
    {
      oldstate.member.add ("type", "dds_sequence_t");
    }
    else
    {
      for (String s : oldstate.scopes.toArray (new String[0]))
      {
        oldstate.member.add ("scope", s);
      }
    }

    if (generate)
    {
      if (isbasic)
      {
        oldstate.seqtypedef = group.getInstanceOf ("seqdef_base");
        oldstate.seqtypedef.add ("type", realsub.getCType ());
        if (isbstring)
        {
          oldstate.seqtypedef.add
            ("str_size", "[" + Long.toString (((BoundedStringType)realsub).getBound () + 1) + "]");
        }
      }
      else
      {
        oldstate.seqtypedef = group.getInstanceOf ("seqdef");
        oldstate.seqtypedef.add
          ("typescope", state.member.getAttribute ("scope"));
        oldstate.seqtypedef.add ("type", state.member.getAttribute ("type"));
      }
      for (String s : oldstate.scopes.toArray (new String[0]))
      {
        oldstate.seqtypedef.add ("scope", s);
      }
    }

    state = oldstate;
    return result;
  }

  public Void visitOctet_type (IDLParser.Octet_typeContext ctx)
  {
    state.member.add ("type", "octet");
    state.type = new BasicType (BasicType.BT.OCTET);
    return super.visitOctet_type (ctx);
  }

  public Void visitSigned_short_int (IDLParser.Signed_short_intContext ctx)
  {
    state.member.add ("type", "short");
    state.type = new BasicType (BasicType.BT.SHORT);
    return super.visitSigned_short_int (ctx);
  }

  public Void visitSigned_long_int (IDLParser.Signed_long_intContext ctx)
  {
    state.member.add ("type", "long");
    state.type = new BasicType (BasicType.BT.LONG);
    return super.visitSigned_long_int (ctx);
  }

  public Void visitSigned_longlong_int (IDLParser.Signed_longlong_intContext ctx)
  {
    state.member.add ("type", "long long");
    state.type = new BasicType (BasicType.BT.LONGLONG);
    return super.visitSigned_longlong_int (ctx);
  }

  public Void visitUnsigned_short_int (IDLParser.Unsigned_short_intContext ctx)
  {
    state.member.add ("type", "unsigned short");
    state.type = new BasicType (BasicType.BT.USHORT);
    return super.visitUnsigned_short_int (ctx);
  }

  public Void visitUnsigned_long_int (IDLParser.Unsigned_long_intContext ctx)
  {
    state.member.add ("type", "unsigned long");
    state.type = new BasicType (BasicType.BT.ULONG);
    return super.visitUnsigned_long_int (ctx);
  }

  public Void visitUnsigned_longlong_int (IDLParser.Unsigned_longlong_intContext ctx)
  {
    state.member.add ("type", "unsigned long long");
    state.type = new BasicType (BasicType.BT.ULONGLONG);
    return super.visitUnsigned_longlong_int (ctx);
  }

  public Void visitBoolean_type (IDLParser.Boolean_typeContext ctx)
  {
    state.member.add ("type", "boolean");
    state.type = new BasicType (BasicType.BT.BOOLEAN);
    return super.visitBoolean_type (ctx);
  }

  private void charMember ()
  {
    state.member.add ("type", "char");
    state.type = new BasicType (BasicType.BT.CHAR);
  }

  public Void visitChar_type (IDLParser.Char_typeContext ctx)
  {
    charMember ();
    return super.visitChar_type (ctx);
  }

  public Void visitWide_char_type (IDLParser.Wide_char_typeContext ctx)
  {
    if (params.mapwide)
    {
      charMember ();
    }
    return super.visitWide_char_type (ctx);
  }

  public Void visitFloating_pt_type (IDLParser.Floating_pt_typeContext ctx)
  {
    if (ctx.KW_LONG () == null || params.mapld)
    {
      if (ctx.KW_FLOAT () != null)
      {
        state.member.add ("type", "float");
        state.type = new BasicType (BasicType.BT.FLOAT);
      }
      else
      {
        state.member.add ("type", "double");
        state.type = new BasicType (BasicType.BT.DOUBLE);
      }
    }
    return super.visitFloating_pt_type (ctx);
  }

  private void stringMember ()
  {
    state.member.add ("type", "string");
    state.type = new BasicType (BasicType.BT.STRING);
  }

  private void boundedStringMember (long length)
  {
    state.member.add ("type", "bstring");
    state.member.add ("str_size", "[" + Long.toString (length + 1) + "]");
    state.type = new BoundedStringType (length);
  }

  public Void visitString_type (IDLParser.String_typeContext ctx)
  {
    Void result = super.visitString_type (ctx);
    if (ctx.positive_int_const() == null)
    {
      stringMember ();
    }
    else
    {
      boundedStringMember (state.literal);
    }
    return result;
  }

  public Void visitWide_string_type (IDLParser.Wide_string_typeContext ctx)
  {
    Void result = super.visitWide_string_type (ctx);
    if (params.mapwide)
    {
      if (ctx.positive_int_const() == null)
      {
        stringMember ();
      }
      else
      {
        boundedStringMember (state.literal);
      }
    }
    return result;
  }

  public Void visitPragma_decl (IDLParser.Pragma_declContext ctx)
  {
    StringTokenizer pragma = new StringTokenizer (ctx.LINE_PRAGMA().getText ());
    pragma.nextToken (); /* Skip "#pragma" */
    if (pragma.hasMoreTokens () && pragma.nextToken ().equals ("keylist"))
    {
      ScopedName currentSN, requestSN, resultSN;
      String topicType = pragma.nextToken ();

      currentSN = new ScopedName ();
      for (String s : state.scopes)
      {
        currentSN.addComponent (s);
      }
      requestSN = new ScopedName (topicType);
      resultSN = params.symtab.resolve (currentSN, requestSN).name ();

      ST topic = topics.get (resultSN);
      StructType structMeta = (StructType)alltypes.get (resultSN);

      if (!params.allstructs && !params.notopics)
      {
        topic.add ("istopic", "true");
      }
      while (pragma.hasMoreTokens ())
      {
        String fieldname = pragma.nextToken ();
        ST field = group.getInstanceOf ("keyfield");
        field.add ("name", fieldname);
        field.add
          ("offset", Integer.toString (structMeta.addKeyField (fieldname)));
        topic.add ("keys", field);
      }
      long size = structMeta.getKeySize ();
      if (size > 0 && size <= MAX_KEYSIZE)
      {
        topic.add ("flags", "DDS_TOPIC_FIXED_KEY");
      }
    }
    return super.visitPragma_decl (ctx);
  }

  public void writeToFile (String filename) throws IOException
  {
    ST topicST;
    StructType topicmeta;
    BufferedOutputStream bos = null;

    for (ScopedName topicname : topics.keySet ())
    {
      topicmeta = (StructType)alltypes.get (topicname);
      topicST = topics.get (topicname);
      if (params.xmlgen)
      {
        StringBuffer str = new StringBuffer ();
        Set <ScopedName> depset =  new LinkedHashSet <ScopedName> ();
        topicmeta.populateDeps (depset, null);
        generateXMLospl (str, depset);
        topicST.add ("xml", str.toString ());
      }
      topicST.add ("marshalling", topicmeta.getMetaOp (null, null));
      topicST.add ("marshalling", "DDS_OP_RTS");
      if (topicmeta.isUnoptimizable ())
      {
        topicST.add ("flags", "DDS_TOPIC_NO_OPTIMIZE");
      }
      topicST.add ("alignment", topicmeta.getAlignment ());
    }

    try
    {
      bos = new BufferedOutputStream (new FileOutputStream (filename));
      bos.write (file.render ().getBytes ("UTF-8"));
    }
    finally
    {
      if (bos != null)
      {
        bos.close ();
      }
    }
  }

  private void generateXMLlite (StringBuffer str, Set <ScopedName> depset)
  {
    ModuleContext mod = new ModuleContext ();
    for (ScopedName sn : alltypes.keySet ())
    {
      if (depset.contains (sn))
      {
        alltypes.get (sn).getToplevelXML (str, mod);
      }
    }
    mod.exit (str);
  }

  private void generateXMLospl (StringBuffer str, Set <ScopedName> depset)
  {
    ScopedName snparent;
    ScopedName modname;
    Set <NamedType> module;
    Set <ScopedName> written = new HashSet <ScopedName> ();

    ModuleContext mod = new ModuleContext ();
    Map <ScopedName, Set <NamedType>> modules =
      new LinkedHashMap <ScopedName, Set <NamedType>> ();

    /* Re-order depset */

    Set <ScopedName> depsorder = new LinkedHashSet <ScopedName> ();
    Set <ScopedName> tmpwritten = new LinkedHashSet <ScopedName> ();

    while (depsorder.size () != depset.size ())
    {
      for (ScopedName s : depset)
      {
        NamedType nt = alltypes.get (s);
        if (nt.depsOK (depsorder))
        {
          tmpwritten.add (s);
        }
      }
      for (ScopedName s : tmpwritten)
      {
        depsorder.add (s);
      }
      tmpwritten.clear ();
    }

    /* Populate depset into a map keyed by module */

    for (ScopedName sn : depset)
    {
      snparent = new ScopedName (sn);
      snparent.popComponent ();
      module = modules.get (snparent);
      if (module == null)
      {
        module = new HashSet <NamedType> ();
        modules.put (snparent, module);
      }
      module.add ((NamedType)alltypes.get (sn));
    }

    while (!modules.isEmpty ())
    {
      module = null;
      modname = null;
      int bestscore = 0;

      /* Find module with the most types ready to be written */

      for (Map.Entry <ScopedName, Set <NamedType>> e : modules.entrySet ())
      {
        int score = 0;
        for (NamedType nt : e.getValue ())
        {
          if (nt.depsOK (written))
          {
            score++;
          }
        }
        if (score >= bestscore)
        {
          module = e.getValue ();
          modname = e.getKey ();
          bestscore = score;
        }
      }

      /* Write all the ready types in that module */
      if (module != null) {
        tmpwritten = new HashSet <ScopedName> ();
        do {
          tmpwritten.clear ();
          for (ScopedName sn : depsorder)
          {
            NamedType nt = alltypes.get (sn);
            if (module.contains (nt) && nt.depsOK (written))
            {
              nt.getToplevelXML (str, mod);
              module.remove (nt);
              tmpwritten.add (sn);
            }
          }
          written.addAll (tmpwritten);
        } while (!tmpwritten.isEmpty ());

        /* Terminate when there's nothing left */
        if (module.isEmpty ()) {
          modules.remove (modname);
        }
      }
    }

    mod.exit (str);
  }

  private ParseState state;
  private IdlParams params;
  private Map <ScopedName, ST> topics;
  private Map <ScopedName, NamedType> alltypes;
  private Map <ScopedName, Long> constants;
  private ST file;
  private STGroup group;
  private static final int MAX_KEYSIZE = 16;
}

