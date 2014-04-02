/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

func_def.c -- Processing for function definitions (both user supplied and
              compiler generated).

*/

/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Additional header files. */
#include "class_decl.h"
#include "exprutil.h"
#if DO_IL_LOWERING
#include "il_walk.h"
#endif /* DO_IL_LOWERING */
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ms_attrib.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#include "statements.h"
#if USER_CONTROL_OF_STRUCT_PACKING
#include "layout.h"
#endif /* USER_CONTROL_OF_STRUCT_PACKING */

/* Forward declaration: */
static void define_special_member_function(a_routine_ptr rout_ptr);

#if ASM_FUNCTION_ALLOWED

static char *scan_asm_function(void)
/*
Scan the text between the opening and closing brace of an asm
function.  Proceed token by token until the matching right brace is
found.  For each token, copy the source text directly into the asm
function body buffer; when the entire text of the asm function body
has been copied, a string of appropriate size is allocated in the
memory region of the asm function and the buffer is copied to it.
Comments in asm functions are saved along with the normal tokens.
*/
{
  unsigned int     nbrace = 1;
  char             *body;

  db_enter(3, "scan_asm_function");
  /* Initialize variables used for building the string. */
  reset_asm_buffer();
  /* Initialize global variables used by lexical routines. */
  in_asm_function_body = TRUE;
  treat_newline_as_token = TRUE;
  fetch_pp_tokens = TRUE;
  /* Advance past the opening brace. */
  if (curr_token == tok_lbrace) {
    (void)get_token();
  } else {
    pos_error(ec_exp_lbrace, &pos_curr_token);
  }  /* if */
  /* Loop through the tokens and build the string token by token. */
  while (curr_token != tok_end_of_source) {
    /* Stop when a zero-level right brace is reached.
       Keep track of braces. */
    if (curr_token == tok_rbrace && --nbrace == 0) {
      /* This right brace matches the opening left brace, marking the end of
         the asm function body.  Copy white space up to the current token. */
      if (start_of_curr_token != NULL) {
        copy_from_source_to_asm_func_buffer(start_of_curr_token, (char *)NULL);
      } else {
        /* We can get here in severe error situations that caused tokens to be
           prematurely cached. */
        check_assertion(total_errors != 0);
      }  /* if */
      break;
    }  /* if */
    /* Special handling for a left brace embedded within the assembler
       code: assume it has a matching right brace. */
    if (curr_token == tok_lbrace) ++nbrace;
    /* Copy characters from the source line to the buffer, from
       last_stop_char through the end of the current token. */
    if (end_of_curr_token != NULL) {
      copy_from_source_to_asm_func_buffer(end_of_curr_token + 1, (char *)NULL);
    } else {
      /* We can get here in severe error situations that caused tokens to be
         prematurely cached. */
      check_assertion(total_errors != 0);
    }  /* if */
    /* Advance to the next token. */
    (void)get_token();
  }  /* while */
  fetch_pp_tokens = FALSE;
  in_asm_function_body = FALSE;
  treat_newline_as_token = FALSE;
  /* Allocate a block of the current IL memory region (the one established
     for the asm function) -- the asm buffer will be copied into it, along
     with a trailing null character. */
  body = alloc_asm_function_body(pos_in_asm_func_body_buffer + 1);
  (void)memcpy(body, asm_func_body_buffer,
               size_t_arg(pos_in_asm_func_body_buffer));
  /* Add a null terminator. */
  body[pos_in_asm_func_body_buffer] = '\0';
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("asm_function")) {
    fprintf(f_debug, "asm block: %s\n", body);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return body;
}  /* scan_asm_function */

static a_statement_ptr scan_asm_function_body(void)
/*
Scan the body of an asm function.  An stmk_asm_func_body statement is
returned to the caller.
*/
{
  a_statement_ptr    stmt;

  db_enter(3, "scan_asm_function_body");
  stmt = alloc_statement((a_statement_kind)stmk_asm_func_body);
  set_stmt_source_position(stmt->position, pos_curr_token);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  add_to_source_sequence_list((char *)stmt, (an_il_entry_kind)iek_statement);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  stmt->variant.asm_func_body = scan_asm_function();
  db_exit();
  return stmt;
}  /* scan_asm_function_body */

#endif /* ASM_FUNCTION_ALLOWED */

static void require_definitions_of_virtual_functions_on_routine_list(
                                                         a_type_ptr class_type)
/*
Require definitions for the virtual functions of the indicated class.
*/
{
  /* If this is a template class, instantiate all the virtual member
     functions.  When instantiating extern inline functions in a way similar
     to templates, do this for all classes. */
  if ((instantiate_extern_inline ||
       (class_type->variant.class_struct_union.is_template_class &&
        !class_type->variant.class_struct_union.is_specialized)) &&
      class_type->variant.class_struct_union.any_virtual_functions) {
    /* Look for virtual functions on the class routines list. */
    a_routine_ptr rp = class_type->variant.class_struct_union.extra_info->
                                                         assoc_scope->routines;
    for (; rp != NULL; rp = rp->next) {
      if (rp->is_virtual && !rp->pure_virtual) {
        a_symbol_ptr sym;
#if IA64_ABI && DO_IL_LOWERING
        /* Secondary entry points of constructors and destructors should
           not get here. */
        check_assertion(rp->primary_ctor_or_dtor == NULL);
#endif /* IA64_ABI && DO_IL_LOWERING */
        /* The function could be called, so mark it to be instantiated.  Note
           that the defer-inline flag is important here to prevent the actual
           instantiation of these functions from occurring earlier than is
           absolutely necessary. */
        sym = (a_symbol_ptr)rp->source_corresp.assoc_info;
        /* Set the instantiation_required flag for the virtual function. */
        set_instance_required(sym, /*value=*/TRUE, SIR_DEFER_INLINE);
#if DO_IL_LOWERING && MAINTAIN_NEEDED_FLAGS
        /* Force the class definition to be kept, because if it is removed the
           virtual function table variable will be detached, and later the
           instance-required flag will be cleared on the virtual functions of
           the class because there is no virtual function table. */
        set_class_keep_definition_in_il(class_type);
#endif /* DO_IL_LOWERING && MAINTAIN_NEEDED_FLAGS */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* require_definitions_of_virtual_functions_on_routine_list */


static void r_require_definitions_of_virtual_functions_in_class(
                                                         a_type_ptr class_type)
/*
Helper routine for require_definitions_of_virtual_functions_in_class
to handle the recursive walk through base classes.
*/
{
  if (!class_type->variant.class_struct_union.
                                        virtual_functions_marked_as_required &&
      class_type->variant.class_struct_union.
                             any_virtual_functions_including_in_base_classes) {
    a_base_class_ptr bcp;
    a_class_type_supplement_ptr
                     ctsp = class_type->variant.class_struct_union.extra_info;

    class_type->variant.class_struct_union.
                                   virtual_functions_marked_as_required = TRUE;
    /* Loop through the routines list and check the virtual functions. */
    require_definitions_of_virtual_functions_on_routine_list(class_type);
    /* Do the same for base class virtual functions. */
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      /* Handle only direct base classes, because the recursive call
         will handle that class's base classes. */
      if (bcp->direct) {
        r_require_definitions_of_virtual_functions_in_class(bcp->type);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* r_require_definitions_of_virtual_functions_in_class */


void require_definitions_of_virtual_functions_in_class(a_type_ptr class_type)
/*
Require definitions for all virtual functions in class_type (including
those from its base classes that are not overridden).  This includes
virtual destructors and instantiatable functions.  The definitions
are required in the overall program, not necessarily in the current
compilation.
*/
{
  class_type = skip_typerefs(class_type);
  if (class_type->variant.class_struct_union.
                             any_virtual_functions_including_in_base_classes) {
    a_class_symbol_supplement_ptr cssp =
                                       symbol_supplement_for_class(class_type);

    if (cssp->destructor != NULL) {
      a_routine_ptr dtor_rout = cssp->destructor->variant.routine.ptr;
      if (dtor_rout->compiler_generated && dtor_rout->is_virtual &&
          !routine_has_been_defined(dtor_rout)) {
        /* Force generation of a compiler-generated virtual destructor.
           This is done only at the top level because virtual destructors
           in base classes would be overridden and therefore would not be
           pointed to from the virtual function table in the derived class. */
        a_routine_ptr decider = vtbl_decider_function_for_class(
                                                            class_type,
                                                            (a_boolean *)NULL);
        if (decider != NULL && !routine_has_been_defined(decider)) {
          /* The vtable is not being put out in this compilation, so don't
             force the definition of the destructor here.  If the decider
             function gets defined later, we'll get back to this code and
             decide at that point to put out the destructor definition. */
        } else {
          define_special_member_function(dtor_rout);
        }  /* if */
      }  /* if */
    }  /* if */
    r_require_definitions_of_virtual_functions_in_class(class_type);
  }  /* if */
}  /* require_definitions_of_virtual_functions_in_class */


static a_boolean virtual_functions_needed_due_to_definition_of(
                                                         a_routine_ptr routine)
/*
Return TRUE if definitions of virtual functions of the class of which the
indicated routine is a member are needed (somewhere in the program, but
not necessarily in the current compilation).  The definition of the
indicated routine has just been processed.
*/
{
  a_boolean  needed = FALSE;
  a_type_ptr class_type = parent_class_of(routine);

  if (class_type->variant.class_struct_union.
                             any_virtual_functions_including_in_base_classes) {
    if (routine->special_kind == (a_special_function_kind)sfk_constructor ||
        routine->special_kind == (a_special_function_kind)sfk_destructor) {
      /* Constructor and destructor wrappers refer to the virtual function
         table and therefore the virtual functions are needed. */
      needed = TRUE;
    } else if (routine->is_virtual) {
      a_routine_ptr decider = vtbl_decider_function_for_class(
                                                            class_type,
                                                            (a_boolean *)NULL);
      if (decider != NULL ?
                    (decider == routine || routine_has_been_defined(decider)) :
                    routine->considered_decider_function_at_some_point) {
        /* This routine is the decider function for definition of the
           virtual function table.  Since it's defined, the virtual function
           table definition will be put out in this compilation and therefore
           the virtual functions are needed. */
        /* Also mark the virtual functions as needed if the decider function
           was previously defined at a point when it wasn't known to be
           the decider function, or was thought to be the decider function
           but is now known not to be.  This can happen only for ABIs (like
           the ARM EABI; see IA64_ABI_VARIANT_KEY_FUNCTION) where the
           decider function can be altered by an out-of-class definition
           that specifies "inline" for a function that otherwise would have
           been considered to be the decider function. */
        needed = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return needed;
}  /* virtual_functions_needed_due_to_definition_of */


static void require_definitions_of_virtual_functions_due_to_definition_of(
                                                         a_routine_ptr routine)
/*
The indicated routine (a member function) has just been defined.  If that
implies that definitions of virtual functions of the routine's class are
needed, go through and process the virtual functions accordingly.  Note that
the definitions are required in the overall program, not necessarily in the
current compilation.
*/
{
  if (virtual_functions_needed_due_to_definition_of(routine)) {
    a_type_ptr class_type = parent_class_of(routine);
    require_definitions_of_virtual_functions_in_class(class_type);
  }  /* if */
}  /* require_definitions_of_virtual_functions_due_to_definition_of */


a_boolean check_function_return_type(a_type_ptr         rout_type,
                                     a_source_position  *err_pos,
                                     a_boolean          is_expr_use,
                                     a_boolean          evaluated,
                                     a_boolean          incomplete_return_okay,
                                     a_routine_ptr      rout_ptr)
/*
Given a routine type, check that the return type is valid, issuing an error
if not (additional checks are performed by add_to_derived_type_list).
is_expr_use is TRUE if the function is being called or its address is being
taken; otherwise, the function is being defined (nondefining declarations are
checked by add_to_derived_type_list).  When is_expr_use is TRUE, evaluated is
TRUE if the expression is in an evaluated context.  incomplete_return_okay is
TRUE if no error should be issued for an incomplete (non-void) return type.
rout_ptr is a pointer to the routine that is being defined or called; may be
NULL.
*/
{
  a_type_ptr  return_type;
  a_boolean   err = FALSE;
  a_boolean   incomplete_type_error = FALSE;
  a_type_ptr  orig_return_type;

  rout_type = skip_typerefs(rout_type);
  orig_return_type = rout_type->variant.routine.return_type;
  return_type = skip_typerefs(orig_return_type);
  /* 3.7.1, constraints: The return type of a function shall be void
     or an object type other than array.  See also the constraints of
     3.5.4.3 on function declarators, enforced previously by
     add_to_derived_type_list.  In addition, a reference type (including a
     reference to an array or function) may also be returned (ARM 8.2.5). */
  if (is_void_type(return_type)) {
    if (is_qualified_type(orig_return_type) && !is_expr_use &&
        C_mode() && strict_ansi_mode) {
      /* In strict C mode a void return type on a function definition cannot
         have a qualifier. */
      err = (strict_ansi_error_severity == es_error);
      diagnostic(strict_ansi_error_severity,
                 ec_type_qualifier_on_void_return_type);
    } else {
      /* Okay. */
    }  /* if */
  } else if (is_error_type(return_type)) {
    /* No diagnostic this time. */
  } else if (!incomplete_return_okay) {
    /* If return_type is an uninstantiated template class, force its
       instantiation. */
    complete_type_is_needed(return_type);
    if (is_expr_use) {
      /* The type check is simpler on function calls, because function and
         array types have already been filtered out. */
      check_assertion(!is_array_type(return_type) &&
                      !is_function_type(return_type));
      if (is_incomplete_type(return_type)) {
        if (microsoft_bugs && !evaluated &&
            is_immediate_class_type(return_type)) {
          /* MSVC++ allows a function call returning an incomplete class type
             in a not-evaluated context. */
          pos_ty_warning(ec_incomplete_class_return_type, err_pos,
                         orig_return_type);
        } else {
          a_routine_type_supplement_ptr  rtsp = rout_type->
                                                 variant.routine.extra_info;

          if (!rtsp->suppress_diagnostic_on_incomplete_return_type) {
            /* If a diagnostic has already been issued on calling (or taking
               the address of) this routine.  No need to do it again. */
            incomplete_type_error = TRUE;
          }  /* if */
          rtsp->suppress_diagnostic_on_incomplete_return_type = TRUE;
          /* Note that err is set (for the return value) even if no diagnostic
             is actually issued. */
          err = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* Declaration case. */
      if ((is_complete_object_type(return_type) &&
           !is_array_type(return_type)) ||
          is_any_reference_type(return_type)) {
        /* err = FALSE; */
        if (gpp_mode && is_immediate_class_type(return_type) &&
            return_type->variant.class_struct_union.abstract) {
          /* In GNU C++ mode, we only check for abstract return types on
             function definitions.  In other C++ modes, this is done whenever
             a function type is created. */
          abstract_class_diagnostic(
                               es_error, ec_function_returning_abstract_class,
                               orig_return_type, err_pos);
          err = TRUE;
        }  /* if */
      } else {
        err = TRUE;
        if (is_immediate_class_type(return_type) &&
            is_incomplete_type(return_type)) {
          incomplete_type_error = TRUE;
        } else {
          pos_error(ec_bad_function_return_type, err_pos);
          rout_type->variant.routine.return_type = error_type();
        }  /* if */
      }  /* if */
    }  /* if */
    if (incomplete_type_error) {
      report_incomplete_function_return_type(orig_return_type, err_pos,
                                             rout_ptr);
    }  /* if */
  }  /* if */
  return !err;
}  /* check_function_return_type */


static a_variable_ptr make_param_variable(a_type_ptr       type_ptr,
                                          a_storage_class  storage_class)
/*
Allocate a variable entry with type type_ptr, set some of its fields, and
return a pointer to it.
*/
{
  a_variable_ptr vp;

  check_assertion(type_ptr != NULL);
  vp = make_variable(type_ptr, storage_class, NO_SCOPE_DEPTH);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  vp->declared_type = type_ptr;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  vp->is_parameter = TRUE;
  /* Parameter variables are always "local". */
  vp->source_corresp.is_local_to_function = TRUE;
  return(vp);
}  /* make_param_variable */


static a_variable_ptr make_implicit_this_param_variable(a_type_ptr  rout_type)
/*
Create a variable entry for an implicit-this parameter, using the indicated
routine type, and return a pointer to it.
*/
{
  a_variable_ptr vp;
  a_type_ptr     this_type = f_implicit_this_param_type_of(rout_type);

  this_type = make_qualified_type(this_type, TQ_CONST);
  vp = make_param_variable(this_type, (a_storage_class)sc_auto);
  vp->is_this_parameter = TRUE;
  set_parent_scope(&vp->source_corresp, iek_variable,
                   innermost_function_scope);
  return vp;
}  /* make_implicit_this_param_variable */


static void attach_param_variable_attributes(a_variable_ptr  vp)
/*
The given variable is a parameter variable.  If any attributes specified on
the parameter really apply to the underlying variable, apply them to the
variable.
*/
{
  a_param_type_ptr  ptp = vp->assoc_param_type;

  if (ptp->attributes != NULL) {
    an_attribute_ptr  vap = get_param_variable_attr_copies(ptp);
    attach_attributes(vap, (char*)vp, iek_variable);
  }  /*  */
}  /* attach_param_variable_attributes */


#if !GENERATE_SOURCE_SEQUENCE_LISTS
/* ARGSUSED */ /* <-- declared_type not used in that case. */
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS */
static void decl_parameter(a_param_id_ptr    param_id,
                           a_type_ptr        declared_type,
                           a_param_type_ptr  ptp,
                           a_boolean         function_instantiation,
                           a_boolean         non_initial_variadic_param)
/*
Enter the declaration of an identifier for a parameter.  The param_id
points to an sk_parameter symbol, which under ordinary circumstances, is
turned into an sk_variable symbol; but if function_instantiation is TRUE,
a new symbol is created and entered in the symbol table.  When declared
types are recorded, declared_type points to the type of this parameter as
it was originally declared (before any transformations such as array-to-
pointer decay).  non_initial_variadic_param is TRUE if this is a parameter
associated with a variadic parameter, but not the initial one.
*/
{
  a_symbol_ptr      sym;
  a_variable_ptr    vp;
  a_type_ptr        tp;
  a_symbol_locator  locator;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean         is_real_instantiation = function_instantiation &&
                   !scope_stack[depth_scope_stack].in_prototype_instantiation;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_enter(3, "decl_parameter");
  /* Choose the type to use, the one in the param-type entry or the one in
     the param-id entry.  Usually, they will be the same. */
  if (function_instantiation) {
    /* For template functions being instantiated the type pointed to by the
       param-id may include a template parameter, so use the type in
       param-type entry, which will be the result of the template arg
       substitution. */
    tp = ptp->type;
    if (remove_qualifiers_from_param_types) {
      /* If top-level qualifiers were stripped off the param-type type,
         restore them in the parameter variable's type. */
      tp = make_qualified_type(tp, ptp->qualifiers);
    }  /* if */
  } else {
    /* In cases other than template instantiations, use the param-id type,
       since it will be the one actually used in the function definition,
       whereas the type in the param-type entry may be a composite type, as
       in the following example:
         void f(int a[3]);
         void f(a) int a[]; { ... }
       In the second declaration the param-id type is int[], but the composite
       type produced for the routine's interface is int[3]. */
    tp = param_id->type;
#if CHECKING
    if (remove_qualifiers_from_param_types) {
      /* A top-level type qualifier may have been stripped off.  The type
         qualifier has been recorded in the param type entry; it should
         correspond to the parameter variable's type qualifier.  It is also
         possible that top-level qualifiers were present on a guiding
         declaration, but not on the corresponding template declaration. */
      check_assertion(guiding_decls_allowed ||
                      ptp->qualifiers == get_type_qualifiers(param_id->type));
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  complete_type_is_needed(tp);
  if (is_incomplete_type(tp)) {
    /* Incomplete type is not allowed. */
    pos_error(incomplete_type_err_code(tp), &param_id->type_pos);
    tp = ptp->type = error_type();
  }  /* if */
  /* Create the parameter variable. */
  vp = make_param_variable(tp, param_id->storage_class);
  vp->is_parameter_pack = ptp->is_parameter_pack;
  vp->is_pack_element = ptp->is_pack_element;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Record the type exactly as it was declared (before array-to-pointer
     decay, etc.). */
  vp->declared_type = declared_type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if VLA_ALLOWED
  vp->has_variably_modified_type = (vla_enabled &&
                                    is_variably_modified_type(tp));
#endif /* VLA_ALLOWED */
  add_to_parameters_list(vp);
  sym = param_id->symbol;
  if (gnu_mode && sym != NULL && sym->ambiguous) {
    /* In GNU C and C++ mode, a duplicate parameter name is only diagnosed in
       function definitions.  Any duplicate parameters were marked "ambiguous"
       when parsing the function declarator.  Issue an error and treat them
       as unnamed parameters in what follows. */
    pos_error(ec_dupl_param_name, &sym->decl_position);
    sym = NULL;
  }  /* if */
  if (sym == NULL) {
    /* This param_id entry represents an unnamed parameter (which is legal
       in function definitions in C++). */
    /* Clear the referenced flag, which is set by set_default_source_corresp
       when the variable is allocated. */
    vp->source_corresp.referenced = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if CHECKING
    if (!source_sequence_entries_disallowed &&
        depth_innermost_instantiation_scope == NO_SCOPE_DEPTH &&
        depth_template_declaration_scope == NO_SCOPE_DEPTH) {
      /* In nontemplate contexts, a source sequence entry should have been
         generated, but some template-related syntax errors may cause us to
         lose track of the fact that we're in a template context. */
      check_assertion(param_id->source_sequence_entry != NULL ||
                      total_errors > 0);
    }  /* if */
#endif /* CHECKING */
    update_source_sequence_list((char *)vp, (an_il_entry_kind)iek_variable,
                                param_id->source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (param_id->type_pos.seq != 0) {
      /* Since set_source_corresp is not called for unnamed entities, create
         the associated decl-pos supplement directly. */
      vp->source_corresp.decl_pos_info =
                     alloc_decl_position_supplement(in_file_scope(vp));
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  } else {
    make_locator_for_symbol(sym, &locator);
    if (function_instantiation) {
      sym = enter_local_symbol((a_symbol_kind)sk_variable, &locator,
                               decl_scope_level,
                               /*suppress_redecl_error=*/
                                                   non_initial_variadic_param);
      /* Mark all but the first pack element symbol as invisible. */
      sym->is_invisible = non_initial_variadic_param;
    } else {
      set_symbol_kind(sym, (a_symbol_kind)sk_variable);
      /* In some modes, the parameter symbols (in the prototype scopes) are
         invisible.  Ensure that they will be visible when copied to the
         function scope. */
      sym->is_invisible = FALSE;
    }  /* if */
    if (ptp->is_pack_element) sym->is_pack_element = TRUE;
    sym->variant.variable.ptr = vp;
    set_source_corresp(&(vp->source_corresp), sym);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Since real instantiations of a same template share the same param_id
       list, new source sequence entries should be created for the
       corresponding parameters (if source sequence entries are at all
       generated for instantiations). */
    record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, sym,
                              &sym->decl_position,
                              is_real_instantiation ?
                                      NULL : param_id->source_sequence_entry);
    if (param_id->is_decl_after_first_in_comma_list) {
      /* An old-style parameter definition that appears after the first in a
         comma-separated list. */
      vp->source_corresp.is_decl_after_first_in_comma_list = TRUE;
    }  /* if */
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
    mark_defined(sym, &sym->decl_position);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    mark_variable_value_set(sym);
#if DEBUG
    if (debug_level >= 3) {
      db_symbol(sym, "Changed from parameter symbol: ", 4);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  if (vp->is_pack_element) {
    /* The symbol for a non-initial variadic pack element is not in the symbol
       table and therefore has no associated scope depth.  As a result, the
       call to set_source_corresp may have produced invalid values for
       is_local_to_function and scope_depth.  Produce the correct values
       now. */
    vp->source_corresp.is_local_to_function = TRUE;
#if RECORD_SCOPE_DEPTH_IN_IL
    vp->source_corresp.scope_depth = decl_scope_level;
  } else if (sym == NULL) {
    /* scope_depth is normally recorded by a call to set_source_corresp, but
       that call didn't occur for unnamed parameters.  Record the depth now. */
    vp->source_corresp.scope_depth = decl_scope_level;
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
  }  /* if */
  vp->assoc_param_type = ptp;
  ptp->name = vp->source_corresp.name;
  attach_param_variable_attributes(vp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  {
  a_decl_position_supplement_ptr  dpsp = vp->source_corresp.decl_pos_info;
  if (dpsp != NULL) {
    dpsp->identifier_range = param_id->identifier_range;
    dpsp->specifiers_range = param_id->specifiers_range;
    dpsp->variant.declarator_range = param_id->declarator_range;
  }  /* if */
  }
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  db_exit();
}  /* decl_parameter */


static void process_vla_parameters(a_func_info_block              *func_info,
                                   a_routine_type_supplement_ptr  rtsp)
/*
A function declared with the properties described by *func_info and *rtsp is
being defined in C mode.  If the function prototype contains references to
variable length arrays (VLAs), the associated VLA entries are created (we
couldn't do so earlier because the function memory region was not yet
available).  Furthermore, the VLA dimensions must be associated with the
parameter variables (which are internal to the function), but not with the
the a_param_type entries of the function: In the function type itself, all
VLA types are therefore transformed into [*] VLAs.  Finally, explicit [*]
declarators are diagnosed (since they can only appear in function prototypes
that are not followed by a definition).
This function should not be called in C++ mode since VLAs are not permitted
in parameter types of C++ mode functions.
*/
{
  a_vla_fixup_ptr   vfp;
  a_param_id_ptr    param_id;
  a_param_type_ptr  ptp;

  check_assertion(C_mode());
  /* Do fixups on VLA declarations that appeared in the function prototype
     scopes.  They are required because the function memory region was not
     not yet available when the function prototype was scanned. */
  /* On the first pass over the fixup list, adjust parameter references
     in VLA dimension expressions.  Replace references to a dummy
     param variable with the references to the real param variable. */
  for (vfp = func_info->vla_fixup_list; vfp != NULL; vfp = vfp->next) {
    if (vfp->array_type == NULL) {
      /* This entry represents a parameter variable fixup. */
      check_assertion(vfp->param_sym != NULL &&
                      vfp->param_sym->kind == (a_symbol_kind)sk_variable);
      vfp->expr->variant.variable = vfp->param_sym->variant.variable.ptr;
    }  /* if */
  }  /* for */
  /* On the second pass over the fixup list, create the VLA dimension
     entries and add them to the vla_dimensions list for the routine's IL
     scope. */
  for (vfp = func_info->vla_fixup_list; vfp != NULL; vfp = vfp->next) {
    if (vfp->array_type != NULL) {
      /* This entry represents a dimension expression fixup.  Copy the
         expression list to the function memory region and then create
         the vla_dimension entry. */
      (void)make_vla_dimension(vfp->array_type,
                               copy_expr_tree(vfp->expr, CE_NO_OPTIONS),
                               /*in_prototype_scope=*/TRUE,
                               &vfp->position);
    }  /* if */
  }  /* for */
  free_vla_fixup_list(func_info->vla_fixup_list);
  func_info->vla_fixup_list = NULL;
  /* Check for VLA errors. */
  param_id = func_info->param_id_list;
  ptp = rtsp->param_type_list;
  for (; param_id != NULL; param_id = param_id->next, ptp = ptp->next) {
    check_assertion_str(param_id->declared_type != NULL,
                        "process_vla_parameters: NULL declared_type");
    if (is_or_contains_vla_type_with_unspecified_bound(
                                           param_id->declared_type)) {
      /* The [*] syntax for VLAs is not allowed for a parameter in a
         function definition.  When parsing a function declarator the [*]
         syntax is allowed because it is impossible to distinguish a
         function prototype and a function definition at that point.  Now
         that the opening brace has been seen, the presence of [*] can be
         detected as an error. */
      pos_error(ec_vla_with_unspecified_bound_not_allowed,
                &param_id->type_pos);
      param_id->type = ptp->type = error_type();
    } else if (is_variably_modified_type(ptp->type)) {
      /* The param-type entry describes the public interface of the
         routine, whereas the parameter variable contains its internal
         representation.  VLA dimensions expressions, which have already
         been recorded in the types of the parameter variables, cannot
         be part of the public interface (like top-level const qualifiers
         in C++), so remove them now.  This transformation has the effect
         of replacing the dimension expression with "*". */
      ptp->type = remove_assoc_vla_dimensions(ptp->type);
    }  /* if */
  }  /* for */
}  /* process_vla_parameters */


static void advance_param_id_and_param_type(
				a_param_id_ptr		*param_id,
				a_param_type_ptr	*ptp,
				a_routine_ptr		rout_ptr)
/*
Advance *param_id and *ptp to the next element in their lists.   If *ptp is
an element of a variadic parameter pack, don't advance *param_id until we
advance past the pack.  This special processing is suppressed for
specializations and lambdas.  rout_ptr is the routine being defined.
*/
{
  a_param_type_ptr	next_ptp = (*ptp)->next;

  if (rout_ptr->is_template_function && !rout_ptr->is_specialized &&
      next_ptp != NULL && next_ptp->param_num == (*ptp)->param_num &&
      next_ptp->param_num != 0) {
    /* The next parameter type entry is for the same variadic parameter.
       Don't advance the param_id. */
  } else {
    *param_id = (*param_id)->next;
  }  /* if */
  *ptp = next_ptp;
}  /* advance_param_id_and_param_type */


static void check_deduced_return_type(a_routine_ptr      rp,
                                      a_source_position  *diag_pos)
/*
The given routine did not include an explicitly specified return type.  If a
value-returning statement was encountered, the return type was set accordingly;
otherwise, this routine will set it to void.  If the return type is non-void
and this is a C++11-style lambda body, check that the body had the simple form
      { return <expression> ; }
and issue a diagnostic if that was not the case.
*/
{
  if (!rp->has_deduced_return_type) {
    /* No return type was specified, and no return type was deduced from a
       return statement.  Determine the return type as if "return (void)0;"
       had appeared. */
    deduce_return_type_from_void_operand(rp, !rp->is_lambda_body, diag_pos);
  }  /* if */
}  /* check_deduced_return_type */


static void set_routine_constexpr_info(a_scope_ptr scope,
                                       a_boolean   constexpr_ruled_out)
/*
scope is the function scope for a constexpr function or constructor.
Check to see if it is valid and record information used later when doing
a constexpr expansion of the routine.  When constexpr_ruled_out is TRUE,
the routine's body failed the criteria for a constexpr function or
constructor.
*/
{
  a_routine_ptr   routine;

  if (!constexpr_ruled_out) {
    check_assertion(scope->kind == (a_scope_kind)sck_function);
    routine = scope->variant.routine.ptr;
    check_assertion(routine->is_constexpr);
#if GNU_EXTENSIONS_ALLOWED
    if (routine->contains_statement_expression) {
      /* We can't expand the function if it contains statement expressions. */
    } else
#endif /* GNU_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    if (special_kind_is(routine, sfk_constructor)) {
      /* Constructor.  Must have an empty statement as the body, i.e.,
         an implicit return. */
      a_constructor_init_ptr ctor_init, *next_ptr_ptr;
      scope->is_constexpr_routine = TRUE;
      scope->variant.routine.variant.constexpr_constructor_inits = NULL;
      /* Make a copy of the constructor inits list so that if IL lowering is
         being done we preserve unlowered copies. */
      next_ptr_ptr =
                   &scope->variant.routine.variant.constexpr_constructor_inits;
      for (ctor_init = scope->variant.routine.constructor_inits;
           ctor_init != NULL;
           ctor_init = ctor_init->next) {
        a_constructor_init_ptr copy = copy_ctor_init(
                                         ctor_init,
                                         CE_COPYING_FOR_CONSTEXPR_MASTER_EXPR |
                                         CE_COPY_NOT_EVALUATED);
        *next_ptr_ptr = copy;
        next_ptr_ptr = &copy->next;
      }  /* for */
    } else {
      /* constexpr function.  A copy of the return expression or dynamic
         init is made (so that an unlowered version of the expression or
         dynamic init is preserved). */
      scope->is_constexpr_routine = TRUE;
      if (scope->has_constexpr_return_expr) {
        scope->variant.routine.variant.constexpr_return_expr = copy_expr_tree(
                          scope->variant.routine.variant.constexpr_return_expr,
                          CE_COPYING_FOR_CONSTEXPR_MASTER_EXPR |
                          CE_COPY_NOT_EVALUATED);
      } else {
        scope->variant.routine.variant.constexpr_return_dynamic_init =
              copy_dynamic_init(
                  scope->variant.routine.variant.constexpr_return_dynamic_init,
                  CE_COPYING_FOR_CONSTEXPR_MASTER_EXPR |
                  CE_COPY_NOT_EVALUATED);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* set_routine_constexpr_info */


void scan_function_body(a_routine_ptr     rout_ptr,
                        a_func_info_block *func_info,
                        a_decl_flag_set   flags)
/*
Scan the function body of the routine pointed to rout_ptr.  *func_info
contains information accumulated during the declaration.  The flags control
specific requirements of the scan, since this routine is called not only
for normal function definitions but also (in C++ mode only, of course) for the
delayed scan of cached tokens of member functions defined in a class
definition, for the instantiation of template functions, and for the bodies
of lambda expressions.
*/
{
  a_type_ptr                     class_type, rout_type;
  a_routine_type_supplement_ptr  rtsp;
  a_scope_number                 scope_number;
  a_param_id_ptr                 param_id;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_param_id_ptr                 orig_param_id = NULL;
  a_boolean                      is_real_instantiation;
  a_param_type_ptr               orig_ptp = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_scope_ptr                    scope_ptr;
  a_struct_stmt_stack_state      saved_sss_state;
  a_boolean                      is_instantiation;
  a_param_type_ptr               ptp;
  a_namespace_ptr                nsp = NULL;
  a_boolean                      is_function_try_block = FALSE;
#if USER_CONTROL_OF_STRUCT_PACKING
  a_pack_alignment_state         saved_pack_alignment_state;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  a_source_position              body_pos;

  db_enter(3, "scan_function_body");
  body_pos = pos_curr_token;
  if (rout_ptr->source_corresp.is_class_member) {
    class_type = parent_class_of(rout_ptr);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cli_or_cx_enabled) {
      /* Issue errors when attempting to define a abstract member of a C++/CLI
         managed class (a more specific message is used for a member of an
         interface class). */
      if (cli_class_type_kind_is(class_type, cctk_interface) &&
          routine_type_is_nonstatic_member_function(rout_ptr->type)) {
        pos_error(ec_cli_interface_member_function_definition,
                  &rout_ptr->source_corresp.decl_position);
      } else if (is_immediate_managed_class_type(class_type) &&
                 rout_ptr->pure_virtual) {
        pos_error(ec_cli_abstract_member_function_definition,
                  &rout_ptr->source_corresp.decl_position);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    class_type = NULL;
  }  /* if */
  /* Change the storage class to indicate that the function has a definition
     now.  An sc_static storage class is left alone. */
  if (rout_ptr->storage_class == (a_storage_class)sc_extern) {
    rout_ptr->storage_class = (a_storage_class)sc_unspecified;
  }  /* if */
  /* Instantiate any delayed exception specification arguments. */
  if (rout_ptr->is_template_function) {
    instantiate_exception_spec_if_needed(symbol_for(rout_ptr));
  }  /* if */
  rout_type = skip_typerefs(rout_ptr->type);
  /* Issue an error if this is an invalid return type. */
  (void)check_function_return_type(rout_type,
                                   &rout_ptr->source_corresp.decl_position,
                                   /*is_expr_use=*/FALSE, /*evaluated=*/FALSE,
                                   /*incomplete_return_okay=*/FALSE, rout_ptr);
  /* In certain very obscure cases, the routine type associated with
     rout_ptr may be replaced by an equivalent type entry.  Refetch the type,
     just in case. */
  rout_type = skip_typerefs(rout_ptr->type);
  rtsp = rout_type->variant.routine.extra_info;
  is_instantiation = (flags & SFB_IS_INSTANTIATION) != 0;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  is_real_instantiation = is_instantiation &&
                   !scope_stack[depth_scope_stack].in_prototype_instantiation;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (!C_mode() && !is_instantiation) {
    /* Reactivate the class and/or namespace of which the function body is
       a member.  For template instantiations this is done when the
       instantiation scope is pushed, so it should not be done here. */
    if (class_type != NULL) {
      /* Member function -- either an inline or "out-of-line" definition. */
      if (flags & SFB_NO_CLASS_REACTIVATION) {
        /* Inline.  Class has already been reactivated. */
      } else {
        /* Push a class symbol reactivation scope, to make class member names
           visible for processing the function definition.  If the class is a
           member of a namespace, reactivating the namespace is treated as a
           namespace extension. */
        a_boolean  reactivate_template_params = FALSE,
                   saved_use_microsoft_specialization_scope =
                                           use_microsoft_specialization_scope;
        if (microsoft_mode && rout_ptr->specialized_with_old_syntax) {
          /* In Microsoft mode, old-style specializations (those without the
             "template <>") can make use of the template parameters of the
             enclosing class template. */
          reactivate_template_params = TRUE;
          use_microsoft_specialization_scope = TRUE;
        }  /* if */
        push_class_and_template_reactivation_scope_full(
                               class_type, reactivate_template_params,
                               /*extend_namespace=*/TRUE,
                               /*force_new_context=*/TRUE);
        use_microsoft_specialization_scope =
                                     saved_use_microsoft_specialization_scope;
      }  /* if */
    } else {
      nsp = parent_namespace_or_null(rout_ptr);
      if (nsp != NULL) {
        a_scope_stack_entry_ptr  decl_ssep = &scope_stack[decl_scope_level];
        a_scope_stack_entry_ptr  curr_ssep = &scope_stack[depth_scope_stack];
        if (curr_ssep->kind == (a_scope_kind)sck_class_reactivation) {
          /* If the current scope is a class reactivation, use the previous
             scope for the following check for a template instantiation
             scope. */
          curr_ssep--;
        }  /* if */
        if (curr_ssep->kind == (a_scope_kind)sck_template_instantiation) {
          /* The namespace is pushed when the template instantiation
             scope is pushed.  Don't do it again now. */
          nsp = NULL;
        } else if ((decl_ssep->kind != (a_scope_kind)sck_namespace &&
                    decl_ssep->kind !=
                                     (a_scope_kind)sck_namespace_extension) ||
                   nsp != decl_ssep->il_scope->variant.assoc_namespace) {
          /* Push a namespace extension scope. */
          push_namespace_extension_scope(nsp);
        } else {
          /* Set the pointer to NULL to indicate there's no stack entry to
             pop. */
          nsp = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  scope_number = (is_instantiation) ?
                        NO_SCOPE_NUMBER : func_info->scope_number;
  /* Push the name scope for the routine body. */
  scope_ptr = push_scope((a_scope_kind)sck_function, scope_number,
                         (a_type_ptr)NULL, rout_ptr);
  scope_stack_top().lambda = func_info->lambda;
  /* Make sure the implicit_typename flag is FALSE during prototype
     instantiations.  It could be set if we are in a mode where
     nonclass prototype instantiations are not normally done, but we
     are processing a variadic template. */
  if (rout_ptr->is_prototype_instantiation && !force_implicit_typename) {
    scope_stack_top().implicit_typename = FALSE;
  }  /* if */
  /* Associate the scope to the routine entry and the routine entry to its
     type entry. */
  rout_ptr->assoc_scope = curr_il_region_number;
  rtsp->assoc_routine = rout_ptr;
  /* If return value optimization may be possible (i.e., if the routine
     returns a class value via a copy constructor) set the flag to TRUE.
     (It is also required that all the return statements return a single local
     variable -- if that turns out not to be the case, the flag will be
     cleared again.) */
  if (rtsp->value_returned_by_cctor) {
    scope_stack[depth_scope_stack].return_value_optimization_possible = TRUE;
  }  /* if */
  if (class_type != NULL && rtsp->this_class != NULL) {
    scope_ptr->variant.routine.this_param_variable =
                                 make_implicit_this_param_variable(rout_type);
  }  /* if */
  if (microsoft_bugs && microsoft_version == 1200 &&
      rout_ptr->defined && rout_ptr->is_specialized) {
    /* This is a duplicate definition of a template specialization.
       Microsoft Visual C++ 6.0 just discards these. */
    scope_stack[depth_scope_stack].discard_when_popped = TRUE;
  }  /* if */
  if (func_info->function_type_from_typedef) {
    /* An error was already issued on this.  Now, since no parameters were
       specified, skip the processing for parameter names. */
    check_assertion(func_info->param_id_list == NULL);
  } else {
    /* Correctly declared function type. */
    a_param_id_ptr               prev_param_id = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (func_info->prototype_scope_ss_list != NULL) {
      /* Step through the segment of file-scope source sequence entries
         generated when the parameter list of the function was scanned.
         Do necessary fixups for parameter entries, and build function-scope
         proxies where necessary. */
      a_scope_stack_entry_ptr      stack_ptr;
      a_source_sequence_entry_ptr  ssep, next_ssep;

      stack_ptr = &scope_stack[DEPTH_OF_FILE_SCOPE];
      ssep = func_info->prototype_scope_ss_list;
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
        if (ssep != NULL) {
          fputs("scan_function_body: fixing up func prototype ss list:\n",
                f_debug);
          db_ss_list(ssep);
        }  /* if */
      }  /* if */
#endif /* DEBUG */
      for (; ssep != NULL; ssep = next_ssep) {
        next_ssep = ssep->next;
        ssep->prev = ssep->next = NULL;
        switch (ss_entry_kind(ssep)) {
          case iek_none:
            /* An empty entry should be for a parameter (the entry could
               not be filled in when the parameter identifier appeared, because
               the variable entry does not get built at that time).  Find
               the corresponding parameter (the lists are not necessarily in
               the same order). */
            param_id = func_info->param_id_list;
            for (; param_id != NULL; param_id = param_id->next) {
              if (param_id->source_sequence_entry == ssep) break;
            }  /* for */
#if DEBUG
            if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
              fprintf(f_debug, "%sparam_id match for ",
                               param_id == NULL ? "no " : "");
              db_source_sequence_entry(ssep);
            }  /* if */
#endif /* DEBUG */
            if (param_id == NULL) {
              /* This source sequence entry is not associated with one of the
                 parameters.  For instance:
                   void f(a) int a(int); { ... } 
                 for which an empty source sequence entry will have been
                 created for the omitted parameter of function a. */
            } else {
              /* Take the entry off the file-scope list and add one (also
                 empty so far) to the function-scope list. */
              ssep->next = stack_ptr->source_sequence_avail_list;
              stack_ptr->source_sequence_avail_list = ssep;
              param_id->source_sequence_entry = NULL;
              if (!is_real_instantiation) {
                /* Instantiations share the same param_id list; so one should
                   not override the source sequence entry of another.  (Only
                   significant when source sequence entries are recorded for
                   instantiations.) */
                param_id->source_sequence_entry =
                                            add_empty_source_sequence_entry();
              }  /* if */
            }  /* if */
            break;
          default:
            /* For types (as well as other miscellany, such as fields in a
               C struct definition), add the entries to a sublist of the
               function scope list. */
            add_source_sequence_entry_to_list(ssep);
            break;
            /* No action. */
        }  /* switch */
      }  /* for */
      /* Just to be neat. */
      func_info->prototype_scope_ss_list = NULL;
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (func_info->any_prototype_names_omitted) {
      /* New-style (function prototype) for which at least one of the param
         names was omitted in the prototype.  In C this is not valid on a
         function definition; in C++ it's okay (see ARM 8.2.5, 8.3). */
      if (C_mode()) {
        diagnostic(es_discretionary_error, ec_all_proto_params_must_be_named);
      }  /* if */
    }  /* if */
    if (f_xref_info != NULL) {
      /* Cross reference info is being put out. */
      param_id = func_info->param_id_list;
      if (param_id != NULL &&
          param_id->old_style_id_pos.seq != 0) {
        /* Update the cross-reference output with entries for the comma-list
           of parameter names in the old-style parameter declaration format. */
        for (; param_id != NULL; param_id = param_id->next) {
          if (param_id->implicitly_declared) {
            /* Implicitly declared old-style parameters are recorded as
               definitions -- mark_defined is called in decl_parameter. */
          } else {
            /* The symbol pointed to by param_id is still an sk_parameter
               symbol.  However, its source position will have been modified,
               so use the original source position. */
            mark_declared(param_id->symbol, &param_id->old_style_id_pos);
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    param_id = func_info->param_id_list;
    ptp = rtsp->param_type_list;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* The declared type of parameters is recorded in a different locations
       depending on the nature of the function (template or nontemplate) and
       the way it was declared (through a typedef, using old-style C syntax,
       etc.).  Depending on the situation we may have to iterate over an
       a_param_type list or an a_param_id list. */
    if (param_id != NULL) {
      if (is_real_instantiation) {
        /* An instantiation of a function template does not always involve
           rescanning its declaration (only its body).  That means that the
           declared type is not recorded in *func_info; instead, it may have
           been reconstructed in the associated a_template_instance entity. */
        a_symbol_ptr             rout_sym = symbol_for(rout_ptr);
        a_template_instance_ptr  tip = rout_sym->variant.routine.instance_ptr;
        check_assertion(tip != NULL);
        if (tip->declared_type != NULL) {
          orig_ptp = skip_typerefs(tip->declared_type)
                                ->variant.routine.extra_info->param_type_list;
        }  /* if */
      } else if (func_info->declared_type != NULL) {
        orig_ptp = skip_typerefs(func_info->declared_type)
                                ->variant.routine.extra_info->param_type_list;
      }  /* if */
      if (orig_ptp == NULL) {
        /* The declared parameter types are not available from the routine's
           declared_type entry (e.g., when dealing with old-style C function
           definitions). */
        orig_param_id = func_info->param_id_list;
        check_assertion(orig_param_id != NULL);
      }  /* if */
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Be sure param-id and param-type lists are in sync. */
    if ((param_id == NULL) != (ptp == NULL)) {
      /* Getting here is unusual.  In most modes, it is the result of the
         param_id list being discarded because severe syntax errors made it
         look like the declarator did not appear at the top level (param_id is
         NULL in such cases).  In Microsoft mode, however, it can also occur
         when a single template-dependent parameter became "void" after
         instantiation (ptp is NULL in that case).  It can also occur for
         variadic templates that are instantiated on empty parameter packs. */
#if CHECKING
      if (param_id == NULL) {
        check_assertion(total_errors != 0);
      } else if (param_id->is_parameter_pack) {
        /* A variadic function can have a function parameter pack that ends
           up having no elements. */
      } else {
        check_assertion_or_expect_error(
                                  microsoft_mode && param_id->next == NULL &&
                                  param_id->type != NULL &&
                                  is_template_dependent_type(param_id->type));
      }  /* if */
#endif /* CHECKING */
      param_id = NULL;
      ptp = NULL;
    }  /* if */
    for (; param_id != NULL && ptp != NULL;
         advance_param_id_and_param_type(&param_id, &ptp, rout_ptr)) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
      a_type_ptr  declared_param_type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* In some cases with empty pack expansions there can be no param
         type entry for a given parameter.   Skip over those param_ids. */
      while (param_id != NULL && ptp != NULL &&
             param_id->param_num < ptp->param_num &&
             param_id->is_parameter_pack) {
        param_id = param_id->next;
      }  /* while */
      if (param_id == NULL) {
        /* This can happen with severe errors (particularly with variadic
           template instantiations). */
        expect_error();
        break;
      }  /* if */
      /* Declare each parameter identifier to have the associated type
         from the parameter type list. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (orig_ptp != NULL) {
        declared_param_type = orig_ptp->type;
        orig_ptp = orig_ptp->next;
      } else {
        /* coverity[var_deref_op] - orig_param_id can't be NULL. */
        declared_param_type = orig_param_id->declared_type;
        orig_param_id = orig_param_id->next;
      }  /* if */
      decl_parameter(param_id, declared_param_type, ptp, is_instantiation,
                     /*non_initial_variadic_param=*/param_id == prev_param_id);
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
      decl_parameter(param_id, (a_type_ptr)NULL, ptp, is_instantiation,
                     /*non_initial_variadic_param=*/param_id == prev_param_id);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      prev_param_id = param_id;
    }  /* for */
    if ((param_id == NULL) != (ptp == NULL) &&
        param_id != NULL && !param_id->is_parameter_pack) {
      /* Something went wrong while parsing the template, which caused us to
         miscount the number of parameters. */
      check_assertion(total_errors != 0);
    }  /* if */
    if (vla_enabled && C_mode()) {
      /* Some additional transformations and checks may be needed for
         parameters with variably-modified types.  (In C++ mode, such
         parameters are not allowed.) */
      process_vla_parameters(func_info, rtsp);
    }  /* if */
    if (rout_ptr->is_constexpr &&
        !check_constexpr_routine_def_type(
                         rout_ptr, &rout_ptr->source_corresp.decl_position)) {
      rout_ptr->is_constexpr = FALSE;
    }  /* if */
    if (!is_instantiation) {
      /* Parameter symbols that were created in the prototype scope (and then
         removed in pop_scope) have to be reentered in the function scope;
         they will be transformed into variable symbols.  Also, in C mode,
         types that were defined in the prototype scope are reactivated now
         so that they will be available in the current scope. */
      if (func_info->prototype_scope_symbols != NULL) {
        reactivate_prototype_scope_symbols(func_info->prototype_scope_symbols);
      }  /* if */
    }  /* if */
  }  /* if */
#if USER_CONTROL_OF_STRUCT_PACKING
  /* Change the defaults for packing class members in a struct definition.
     This is especially important when the definition is encountered "out
     of sequence" relative to the rest of the program (e.g., delayed
     processing of inline-defined member functions and instantiations of
     function templates), since the defaults at the point of definition
     (textual) may be different from the defaults at the point where the
     body is actually scanned (now). */
  if (flags & SFB_PRAGMA_PACK_IS_LOCAL) {
    reset_pack_alignment_state(func_info->max_member_alignment,
                               &saved_pack_alignment_state);
    scope_stack[depth_scope_stack].pragma_pack_is_local = TRUE;
  }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  if (flags & SFB_NEW_STRUCT_STMT_STACK_REQUIRED) {
    /* Save structured statement stack state before calling compound_statement
       (so that it can be restored upon return) and create a new structured
       statement stack.  This is required for function definitions in classes
       defined within a function definition.  An indefinite nesting depth is
       supported */
    new_struct_stmt_stack(&saved_sss_state);
  }  /* if */
  /* Start a new stop tokens set because the inside of a function is rather
     different than the outside. */
  push_stop_token_stack();
#if ASM_FUNCTION_ALLOWED
  if (rout_ptr->storage_class == (a_storage_class)sc_asm) {
    scope_ptr->assoc_block = scan_asm_function_body();
#if ONE_INSTANTIATION_PER_OBJECT
#if DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES
    if (one_instantiation_per_object) {
      /* In one-instantiation-per-object mode, asm functions should be
         duplicated in each slice. */
      rout_ptr->source_corresp.duplicate_static_in_instantiation_slices = TRUE;
    }  /* if */
#endif /* DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  } else
#endif /* ASM_FUNCTION_ALLOWED */
  /* Do not insert code here. */
  {
    a_boolean  explicit_return_type =
                         ((flags & SFB_IMPLICITLY_DECLARED_RETURN_TYPE) == 0);

    if (curr_token == tok_try && func_info->lambda == NULL) {
      /* This must be a function-try-block.  Do some initialization that has
         to be done before ctor-initializers are processed and bypass "try". */
      start_of_function_try_block();
      is_function_try_block = TRUE;
    }  /* if */
#if CHECKING
    if (total_errors == 0) {
      /* Except where there are invalid declarations, the flags in the types
         should be consistent with the special function kinds. */
      check_assertion((a_boolean)rtsp->assoc_routine_is_ctor ==
                    (rout_ptr->special_kind ==
                                   (a_special_function_kind)sfk_constructor));
      check_assertion((a_boolean)rtsp->assoc_routine_is_dtor ==
                    (rout_ptr->special_kind ==
                                   (a_special_function_kind)sfk_destructor));
    }  /* if */
#endif /* CHECKING */
    /* Enter the constructor initializers.  If the current token is a ":",
       explicit initialization for the constructor follows, but even without
       an explicit initializer, any implicit initializers should be
       recorded. */
    if (rout_ptr->special_kind == (a_special_function_kind)sfk_constructor) {
      scope_ptr->variant.routine.constructor_inits =
                                      ctor_initializer(rout_ptr,
                                                       /*user_defined=*/TRUE);
    } else if (rout_ptr->special_kind ==
                                   (a_special_function_kind)sfk_destructor) {
      scope_ptr->variant.routine.constructor_inits =
                                      dtor_initializer(rout_ptr);
    }  /* if */
    if (is_function_try_block) {
      /* Scan the function try block.  This includes scanning the catch
         clauses that follow the function body. */
      scope_ptr->assoc_block = function_try_block(explicit_return_type);
    } else {
      /* Scan the compound statement defining the function.  The closing "}"
         is not swallowed by compound_statement, so that the pop_scope call
         can be done to get any errors out right on the "}". */
      scope_ptr->assoc_block = compound_statement(/*at_function_level=*/TRUE,
                                                  explicit_return_type,
                                                  /*is_catch_clause=*/FALSE,
                                                  /*is_statement_expr=*/FALSE);
      if (rout_ptr->is_constexpr) {
        set_routine_constexpr_info(
              scope_ptr,
              scope_stack[depth_innermost_function_scope].constexpr_ruled_out);
      }  /* if */
    }  /* if */
  }  /* if */
#if USER_CONTROL_OF_STRUCT_PACKING
  /* Restore defaults for packing class members in a struct definition to
     what it was before the routine body was entered. */
  if (flags & SFB_PRAGMA_PACK_IS_LOCAL) {
    restore_pack_alignment_state(&saved_pack_alignment_state);
  }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  if (rout_ptr->has_deducible_return_type &&
      !rout_ptr->is_prototype_instantiation) {
    /* We're completing the body of a function with a deducible return type.
       Ensure that a type is established at this point. */
    check_deduced_return_type(rout_ptr, &body_pos);
  }  /* if */
  /* Pop the function scope. */
  pop_scope();
  if (flags & SFB_NEW_STRUCT_STMT_STACK_REQUIRED) {
    /* Restore the original structured statement stack.  This is done
       after popping the function scope because pop_scope calls
       wrapup_control_flow_processing. */
    restore_struct_stmt_stack(&saved_sss_state);
  }  /* if */
  if (class_type != NULL) {
    /* This is a member function.  See if the fact that it is defined
       forces definition of virtual functions of the class. */
    require_definitions_of_virtual_functions_due_to_definition_of(rout_ptr);
  }  /* if */
  if (!is_instantiation) {
    /* For templates, the class and/or namespace scopes are pushed and
       popped when the instantiation scope is pushed/popped. */
    if (class_type != NULL) {
      if (!(flags & SFB_NO_CLASS_REACTIVATION)) {
        /* Pop the class symbol reactivation scope. */
        pop_class_reactivation_scope();
      }  /* if */
    } else if (nsp != NULL) {
      if (gpp_mode) {
        pop_namespace_reactivation_scope();
      } else {
        pop_namespace_extension_scope();
      }
    }  /* if */  
  }  /* if */
  if (!is_function_try_block) {
    /* Check for the closing "}", not done in compound_statement.  Note that
       required_token is not called; if compound_statement returned on
       anything other than a right brace, it's because we should start parsing
       on this token. */
    if (curr_token != tok_rbrace) {
      pos_error(ec_exp_rbrace, &pos_curr_token);
    }  /* if */
  }  /* if */
  pop_stop_token_stack();
  if (instantiate_extern_inline && rout_ptr->is_inline) {
    /* When inline functions are instantiated like templates, add the function
       to the list of inline functions if it is inline. */
    add_to_inline_function_list(rout_ptr);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled && special_kind_is(rout_ptr, sfk_static_constructor)) {
    /* If this is the static constructor definition, ensure that the initonly
       static members have been initialized. */
    check_initonly_members(class_type, /*static_ctor_def_seen=*/TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DEBUG
  if (debug_level >= 4) {
    a_symbol_ptr  sym = (a_symbol_ptr)rout_ptr->source_corresp.assoc_info;
    if (sym != NULL) {
      db_symbol(sym, "finished scanning body for ", 2);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_function_body */


void adjust_member_routine_type(a_type_ptr	rout_type,
				a_type_ptr	prev_type)
/*
rout_type is the type of a member function or member function template
that is being defined.  prev_type is the type from the declaration in
the class.  rout_type is missing certain information, such as the
this_class information.  Update rout_type with information from prev_type.
*/
{
  prev_type = skip_typerefs(prev_type);
  rout_type = skip_typerefs(rout_type);
  rout_type->variant.routine.extra_info->this_class =
         prev_type->variant.routine.extra_info->this_class;
  rout_type->variant.routine.extra_info->qualifiers =
         prev_type->variant.routine.extra_info->qualifiers;
  rout_type->variant.routine.extra_info->routine_name_linkage =
         prev_type->variant.routine.extra_info->routine_name_linkage;
}  /* adjust_member_routine_type */


#if !DECL_MODIFIERS_IN_USE || !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_modifiers and decl_pos_block are not used in some
                configurations. */
#endif /* !DECL_MODIFIERS_IN_USE || !EXTRA_SOURCE_POSITIONS_IN_IL */
static void define_member_function(a_symbol_locator            *locator,
                                   a_decl_parse_state          *dps,
                                   a_func_info_block           *func_info,
                                   an_id_linkage_kind          *linkage_ptr,
                                   a_type_ptr                  *old_type,
                                   a_symbol_ptr                *ext_sym,
                                   a_decl_pos_block_ptr        decl_pos_block)
/*
This routine is called in the case of a member function definition.  Its
function is similar to that of decl_routine, which is called for
the definitions of ordinary functions.  After doing some error checking,
it calls reconcile_routine_types to merge the current type with the type
on a prior declaration.
The function's declaration is described by locator, dps, and func_info.
Existing type information (from the matching in-class declaration) is returned
through *old_type.  Extended position information is recorded in
*decl_pos_block.  *linkage_ptr is set to idl_external, and *ext_sym is set to
NULL.
This function is also called in the case of a nondefining out-of-class
member declaration (allowed in some Microsoft modes only).
*/
{
  a_type_ptr           type_ptr = dps->type;
  a_type_ptr           rout_type = skip_typerefs(type_ptr);
  a_routine_type_supplement_ptr
                       rtsp = rout_type->variant.routine.extra_info;
  a_symbol_ptr         sym = locator->specific_symbol;
  a_type_ptr           class_type = sym_parent_class(sym);
  a_routine_ptr        rp;
  a_scope_stack_entry  *ssep = &scope_stack[depth_scope_stack];
  a_boolean            microsoft_out_of_class_redecl = microsoft_mode &&
                                                  locator->is_class_member &&
                                                  curr_token == tok_semicolon;
  a_source_position    orig_pos = null_source_position, saved_pos;

  db_enter(3, "define_member_function");
  if (!is_member_function_symbol(sym)) {
    /* A nonfunction class member.  This is an error, so set sym to NULL to
       force the creation of a fake member function symbol. */
    if (sym->kind == (a_symbol_kind)sk_projection) {
      /* A member of a base class. */
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
    } else {
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, sym);
    }  /* if */
    sym = NULL;
  } else if (!namespace_is_enclosed_by_scope(sym, ssep) &&
             !gpp_mode && !microsoft_out_of_class_redecl) {
    /* This member function is being defined in a scope that does not enclose
       the scope in which the parent class was defined.  (This is not required
       in GNU C++ mode.  Neither is it required for Microsoft mode out-of-class
       member function redeclarations, which can appear in function scope.) */
    sym_error(ec_bad_scope_for_definition, sym);
    sym = NULL;
  } else {
    /* Look for a member function symbol of this type in the symbol table.
       It is an error if it is  not already there. */
    a_symbol_ptr  orig_sym = sym, other_match;
    sym = member_function_redecl_sym(orig_sym, dps, (a_template_param_ptr)NULL,
                                     &other_match);
    if (sym == NULL && any_cfront_mode()) {
      /* In cfront it's okay to put a function qualifier on a member function
         definition.  If it's inappropriate, it's just ignored.  Do the same
         in cfront mode -- but issue a diagnostic. */
      if (rtsp->this_class != NULL) {
        rtsp->this_class = NULL;
        sym = member_function_redecl_sym(
                                    locator->specific_symbol, dps,
                                    (a_template_param_ptr)NULL, &other_match);
        /* The qualifiers are cleared only after looking for a redeclaration
           symbol.  This ensures that we find the same declaration Cfront
           would find. */
        rtsp->qualifiers = TQ_NONE;
        if (sym != NULL) {
          pos_sy_warning(ec_not_compatible_with_previous_decl,
                         &locator->source_position, locator->specific_symbol);
        }  /* if */
      }  /* if */
    }  /* if */
    if (sym != NULL && sym->kind == (a_symbol_kind)sk_projection) {
      /* The matching declaration is a using-declaration: An error. */
      check_assertion(is_class_member_using_decl_symbol(sym));
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
      reduce_projection_symbol_to_fundamental_symbol(sym);
    }  /* if */
    if (sym == NULL) {
      /* No member function with a matching type was found.  Issue an error.
         If the type matches an instance of a member function template,
         then this is probably an attempt to define a function using
         the old specialization syntax.  Issue an error to that effect. */
      if (has_matching_template_instance(orig_sym, type_ptr,
                                         locator->template_arg_list)) {
        pos_sy_error(ec_old_specialization_not_allowed,
                     &locator->source_position, orig_sym);
      } else {
        pos_sy_error(locator->specific_symbol->kind ==
                                     (a_symbol_kind)sk_overloaded_function ?
                        ec_no_match_for_type_of_overloaded_function :
                        ec_not_compatible_with_previous_decl,
                   &locator->source_position, locator->specific_symbol);
      }  /* if */
      if (dps->has_deducible_return_type) {
        /* The mismatch was possibly due to this declaration having an "auto"
           or "decltype(auto)" return type.  For error recovery purposes,
           proceed with an error return type. */
        rout_type->variant.routine.return_type = error_type();
      }  /* if */
    } else if (sym->kind == (a_symbol_kind)sk_function_template) {
      /* A case like this:
           class A { template <class T> void f(int); };
           void A::f(int) { }
      */
      pos_sy_error(ec_old_specialization_not_allowed,
                   &locator->source_position, sym);
      sym = NULL;
    } else if (sym->variant.routine.ptr->compiler_generated) {
      /* Attempting to give a definition for a function that was implicitly
         declared. */
      pos_error(ec_definition_of_implicitly_declared_function,
                &locator->source_position);
      /* Unless a definition has already been generated, reset some flags
         so that that this routine will be treated as user-declared from
         now on. */
      if (!sym->defined) {
        sym->variant.routine.ptr->compiler_generated = FALSE;
        set_inline_flag(sym->variant.routine.ptr, FALSE);
      }  /* if */
    } else if (other_match != NULL) {
      /* Multiple matches were found.  E.g.,
           __interface I1 { int f(); };  __interface I2 { int f(); };
           struct D: I1, I2 { int I1::f(); int I2::f(); };
           int D::f() { return 0; }  // Ambiguous.
      */
      pos_sy_error(ec_ambiguous_name, &locator->source_position, sym);
      check_assertion(microsoft_mode);
      /* Proceed as if no match had been found at all. */
      sym = NULL;
    }  /* if */
  }  /* if */
  if (sym == NULL || (sym->defined && !microsoft_out_of_class_redecl)) {
    /* Error case. */
    a_routine_ptr        other_rp = NULL;
    a_symbol_header_ptr  hdr = locator->symbol_header;
    if (sym != NULL) {
      /* Type was okay, but this member function has a body. */
      pos_sy_error(ec_function_redefinition, &locator->source_position, sym);
      other_rp = sym->variant.routine.ptr;
      rtsp->this_class =
                       other_rp->type->variant.routine.extra_info->this_class;
      rtsp->qualifiers =
                       other_rp->type->variant.routine.extra_info->qualifiers;
      rtsp->this_qualifiers =
                  other_rp->type->variant.routine.extra_info->this_qualifiers;
    } else {
      /* In the error case assume the member function is nonstatic and give
         it an implicit this parameter type.  This will prevent an error from
         being issued on a direct reference to a nonstatic data member in the
         function body. */
      rtsp->this_class = class_type;
    }  /* if */
    /* An error has been detected.  Make a "fake" symbol and routine entry so
       that the routine definition can proceed. */
    /* "Enter" the symbol using an error locator -- this means a symbol
       entry will be created but it will not be added to any lists.  Then
       we'll restore the header to the new symbol, so that the correct name
       will be available in diagnostics. */
    set_to_error_locator(*locator);
    sym = enter_local_symbol((a_symbol_kind)sk_routine, locator,
                             DEPTH_OF_FILE_SCOPE,
                             /*suppress_redecl_error=*/TRUE);
    sym->header = hdr;
    rp = make_routine(type_ptr, (a_storage_class)sc_unspecified,
                      NO_SCOPE_DEPTH);
    sym->variant.routine.ptr = rp;
    set_source_corresp(&(rp->source_corresp), sym);
    set_class_membership(sym, &rp->source_corresp, class_type);
    if (other_rp != NULL) {
      set_routine_special_kind(rp, other_rp->special_kind);
      rp->variant = other_rp->variant;
    }  /* if */
    dps->prev_type = *old_type = type_ptr;
  } else {
    /* A member function symbol with a compatible type was found. */
    rp = sym->variant.routine.ptr;
    orig_pos = sym->decl_position;
    dps->prev_type = *old_type = routine_symbol_type(sym);
    if (rp->is_declared_constexpr != ((dps->dso_flags & DSO_CONSTEXPR) != 0)) {
      /* The previous declaration doesn't match the current one wrt. the
         "constexpr" specifier.  Issue an error. */
      pos_sy_error(rp->is_declared_constexpr ?
                                       ec_previous_constexpr_decl_conflict :
                                       ec_previous_nonconstexpr_decl_conflict,
                   rp->is_declared_constexpr ? &dps->constexpr_pos
                                             : &dps->declarator_pos,
                   sym);
      rp->is_declared_constexpr = TRUE;
      rp->is_constexpr = TRUE;
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      if (rtsp->calling_convention != (a_calling_convention)cc_default) {
        /* A calling convention was specified on the out-of-class
           definition. */
        if (rtsp->calling_convention == (a_calling_convention)cc_thiscall &&
            !routine_type_is_nonstatic_member_function(*old_type)) {
          /* The "__thiscall" calling convention can only be applied to
             nonstatic member functions. */
          pos_error(ec_thiscall_requires_nonstatic_member,
                    &locator->source_position);
        } else if (!calling_conventions_are_compatible(*old_type, rout_type)) {
          /* An out-of-class definition should not change the calling
             convention declared in the class definition (not specifying a
             calling convention never amounts to a change).  (A similar GNU-
             mode test is delayed until attributes are applied.) */
          pos_error(ec_conflicting_calling_conventions,
                    &locator->source_position);
        }  /* if */
      }  /* if */
      if (microsoft_out_of_class_redecl && microsoft_version >= 1310 &&
          !in_microsoft_implementation_key_mapping_region &&
          (!is_member_function_symbol(sym) || !rp->is_template_function)) {
        /* Recent microsoft compilers only accept the out-of-class
           redeclaration syntax for template specializations, or inside a
           region of code delimited by #pragma start_map_region and
           #pragma stop_map_region. */
        pos_sy_error(ec_member_function_redecl_outside_class,
                     &locator->source_position, sym);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* The types may be compatible but not identical.  Create (in rout_type)
       a composite type.  First copy the implicit this param type pointer
       into rout_type:  it is always wrong for nonstatic member functions.
       Also be sure the routine name linkage for the type is right. */
    adjust_member_routine_type(rout_type, *old_type);
    if (noexcept_enabled && rtsp->exception_specification == NULL) {
      /* For a destructor or an "operator delete" an exception specification
         may be generated.  Use the specification used in the in-class
         declaration. */
      if (special_kind_is(rp, sfk_destructor) ||
           (special_kind_is(rp, sfk_operator) &&
            is_delete_operator(rp->variant.opname_kind))) {
        rtsp->exception_specification =
                          skip_typerefs(*old_type)->variant.routine.extra_info
                                                  ->exception_specification;
      }  /* if */
    }  /* if */
    /* Do compatibility checking on the throw specification. */
    check_exception_specification(rout_type, sym, &func_info->throw_position,
                                  /*is_redecl=*/TRUE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Record the default arguments of the current declaration before
       reconcile_routine_types is called. */
    if (func_info->declared_type != NULL) {
      copy_routine_type_default_args(type_ptr, func_info->declared_type);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Note that type_ptr is passed to reconcile_routine_types instead of
       rout_type.  This is intended.  type_ptr should differ from rout_type
       only by the presence of a top-level type qualifiers.  These will only
       appear on a function type when support for near and far is enabled. */
#if CHECKING
    if (same_entities(rout_type, type_ptr)) {
      /* Okay. */
#if NEAR_AND_FAR_ALLOWED
    } else if (near_and_far_enabled()) {
      a_type_qualifier_set  qual = get_top_level_type_qualifiers(type_ptr);
      check_assertion(qual == TQ_NEAR || qual == TQ_FAR);
#endif /* NEAR_AND_FAR_ALLOWED */
    } else {
      unexpected_condition();
    }  /* if */
#endif /* CHECKING */
    rp = sym->variant.routine.ptr;
    /* Ordinarily, the current declaration is the definition, and
       reconcile_routine_types should therefore preserve the new type.
       However, for Microsoft out-of-class redeclarations this isn't an actual
       definition.  Furthermore, such out-of-class redeclarations may appear
       in local scopes, including the scope of the function itself:
            struct S { void f(int i); };
            void S::f(int i) { void S::f(int i); }
       Changing the type of the function while it is being defined would lead
       to subtle errors later on.  So the original type is preserved in that
       case. */
    reconcile_routine_types(
                        rp, type_ptr,
                        /*preserve_rout_type=*/microsoft_out_of_class_redecl,
                        /*preserve_type_ptr=*/!microsoft_out_of_class_redecl,
                        dps);
    if (rp->special_kind == (a_special_function_kind)sfk_constructor) {
      /* If the routine is a default constructor or a copy constructor, it may
         be that this has not yet been recorded in the symbol.  (This becomes
         possible if there are default arguments in the definition.) */
      a_class_symbol_supplement_ptr  cssp;
      cssp = symbol_supplement_for_class(class_type);
      if (!cssp->has_nontrivial_default_constructor &&
          is_default_constructor(rp, /*is_declarative_context=*/TRUE)) {
        /* This is a default constructor, so set the flag. */
        cssp->has_nontrivial_default_constructor = TRUE;
      }  /* if */
      /* There are three flags associated with copy constructors. */
      if (!cssp->has_copy_constructor_for_const_object ||
          cssp->construction_by_bitwise_copy_allowed) {
        a_type_qualifier_set  qualifiers;
        if (is_copy_constructor(rp, class_type, &qualifiers,
                                /*include_move_ctors=*/TRUE,
                                /*is_declarative_context=*/FALSE)) {
          /* This is a copy constructor.  Note that the presence of a user-
             defined copy constructor means that construction by bitwise
             copying is not done. */
          cssp->has_copy_constructor = TRUE;
          cssp->has_copy_constructor_for_const_object =
                                            ((qualifiers & TQ_CONST) != 0);
          cssp->construction_by_bitwise_copy_allowed = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
    /* If this is a member function of an instantiation of a class
       template, mark this as a specialization.  However, since the newer
       template<> syntax was not used, mark it as using the old syntax. */
    if (sym->variant.routine.instance_ptr != NULL) {
      check_old_specialization_allowed(sym, &locator->source_position);
      sym->variant.routine.ptr->is_specialized = TRUE;
      sym->variant.routine.ptr->specialized_with_old_syntax = TRUE;
      sym->variant.routine.instance_ptr->instantiation_required = FALSE;
    }  /* if */
    /* Mark the routine to indicate that, though really belonging to the
       scope of its parent class, it is defined elsewhere. */
    if (!microsoft_out_of_class_redecl) rp->defined_outside_of_parent = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (!microsoft_out_of_class_redecl) {
      record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, sym,
                                &locator->source_position,
                                func_info->declarator_ssep);
      set_routine_declared_type(rp, func_info->declared_type);
    } else {
      /* This is just a redeclaration, and hence the declared type is attached
         to a secondary source sequence entry (if one was created). */
      record_symbol_declaration(SRK_DECLARATION, sym,
                                &locator->source_position,
                                func_info->declarator_ssep);
      if (!source_sequence_entries_disallowed) {
        a_source_sequence_entry_ptr  decl_ssep = NULL;
        a_src_seq_secondary_decl_ptr  sssdp;
        decl_ssep = last_matching_source_sequence_entry((char *)rp);
        check_assertion(decl_ssep != NULL);
        sssdp = (a_src_seq_secondary_decl_ptr)decl_ssep->entity.ptr;
        sssdp->declared_type = func_info->declared_type;
      }  /* if */
    }  /* if */
    /* Set the declared-type in the routine entry. */
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
    if (!microsoft_out_of_class_redecl) {
      mark_defined(sym, &locator->source_position);
    } else {
      mark_declared(sym, &locator->source_position);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    copy_source_position(locator->source_position,
                         rp->source_corresp.decl_position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    update_decl_pos_info(&rp->source_corresp, decl_pos_block);
#if DEBUG
    if (decl_pos_block != NULL) {
      if (debug_level >= 3 || db_flag_is_set("dump_decl_pos_info")) {
        fprintf(f_debug, "decl-pos info for member function def\n");
        db_decl_pos_info(sym);
      }  /* if */
    }  /* if */
#endif /* DEBUG */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
  if (!exceptions_enabled) {
    /* Check whether a diagnostic should be issued on this exception
       specification, and issue one if needed. */
    issue_no_exception_support_diag_on_throw_spec(func_info);
  }  /* if */
  if (func_info->is_inline) {
    if (!rp->is_inline) {
      set_inline_flag(rp, TRUE);
      if (rp->called) {
        /* In the ARM, member functions could not be redeclared inline after
           being called.  This restriction has been eliminated in the
           standard. */
        pos_sy_remark(ec_called_function_redeclared_inline,
                      &locator->source_position, sym);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Reset the storage class and name linkage. */
  if (!extern_inline_allowed && rp->is_inline) {
    rp->storage_class = (a_storage_class)sc_static;
    rp->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
  } else if (rp->source_corresp.name_linkage ==
                           (a_name_linkage_kind)nlk_cplusplus_external &&
             !microsoft_out_of_class_redecl) {
    /* The routine will have been given a storage class of sc_extern when it
       was originally declared; change it to sc_unspecified now that the
       definition has been seen. */
    rp->storage_class = (a_storage_class)sc_unspecified;
    if (!rp->is_inline) {
      /* Also set the referenced flag, assuming a reference from another
         translation unit. */
      rp->source_corresp.referenced = TRUE;
    }  /* if */
  }  /* if */
#if ASM_FUNCTION_ALLOWED
  if (func_info->is_asm_function &&
      rp->storage_class == (a_storage_class)sc_unspecified) {
    rp->storage_class = (a_storage_class)sc_asm;
  }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
  dps->sym = sym;
#if GNU_EXTENSIONS_ALLOWED
  report_gnu_postfix_attributes_on_function_definition(dps);
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (!is_error_locator(*locator)) {
    /* Apply attributes. */
    attach_decl_attributes(dps, func_info->is_definition);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (dps->ms_attributes != NULL) {
      apply_microsoft_attributes_to_routine(&dps->ms_attributes, rp);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Temporarily restore the position of the original declaration in the
       associated symbol so that diagnostics come out right. */
    saved_pos = sym->decl_position;
    sym->decl_position = orig_pos;
    update_routine_decl_modifiers(rp, &dps->decl_modifiers,
                                  &locator->source_position,
                                  /*is_redecl=*/TRUE,
                                  !microsoft_out_of_class_redecl,
                                  (a_boolean)func_info->is_inline);
    sym->decl_position = saved_pos;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else {
    /* Clear the attributes pointer to avoid cascading errors. */
    dps->ms_attributes = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  if (any_deferred_access_checks()) {
    /* Now that we know which function has been declared, recheck any
       access errors that occurred while scanning the declaration. */
    check_assertion(rp != NULL);
    perform_deferred_access_checks_for_function(rp);
  }  /* if */
#if NEED_NAME_MANGLING
  if (func_info->any_default_args) {
    set_parent_routine_for_closure_types_in_default_args(type_ptr, sym);
  }  /* if */
#endif /* NEED_NAME_MANGLING */
  /* If a lint-style "argsused" or "varargs" comment appeared, record that in
     the function type.  That will suppress any warnings about unused
     parameters or variable arguments.  Note that this is done before calling
     process_curr_construct_pragmas; otherwise the pragmas we're interested
     in would have been disposed of. */
  record_lint_argsused_and_varargs_state(sym);
  /* Do processing required for the rest of the pragmas, if any, that are
     bound to the current declaration. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  *ext_sym = NULL;
  *linkage_ptr = idl_external;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 2);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* define_member_function */


void define_lambda_conversion_function(a_routine_ptr  conv_op)
/*
conv_op points to the entry for a conversion function of a closure type
(generated for a lambda expression).  conv_op->next points to a static member
that represents an alternate entry point for the closure's call operator.
Add a definition to conv_op that returns the address of the conv_op->next
routine.
*/
{
  a_routine_ptr    static_entry_pt = conv_op->next;
  a_statement_ptr  block_stmt, return_stmt;
  a_scope_ptr      fn_scope;

  conv_op->storage_class = (a_storage_class)sc_unspecified;
  fn_scope = push_scope((a_scope_kind)sck_function, NO_SCOPE_NUMBER,
                        (a_type_ptr)NULL, conv_op);
  fn_scope->variant.routine.this_param_variable =
                             make_implicit_this_param_variable(conv_op->type); 
  conv_op->assoc_scope = curr_il_region_number;
  conv_op->type->variant.routine.extra_info->assoc_routine = conv_op;
  check_assertion(special_kind_is(static_entry_pt, sfk_lambda_entry_point));
  return_stmt = alloc_statement((a_statement_kind)stmk_return);
  return_stmt->expr = function_addr_expr(static_entry_pt);
  mark_routine_referenced(static_entry_pt);
  block_stmt = alloc_statement((a_statement_kind)stmk_block);
  block_stmt->variant.block.statements = return_stmt;
  fn_scope->assoc_block = block_stmt;
  pop_scope();
  check_assertion(conv_op->is_inline);
  if (instantiate_extern_inline) add_to_inline_function_list(conv_op);
}  /* define_lambda_conversion_function */


void scan_defaulted_or_deleted_definition(a_decl_parse_state    *dps,
                                          a_func_info_block     *func_info)
/*
A "= default;" or "= delete;" definition is next (although the semicolon may
be missing) and func_info reflects this already.  Update the associated IL
entry and scan past the first two tokens.  This is called for non-member
functions and for out-of-class definitions of member functions, but not for
in-class definitions of member functions.
*/
{
  a_routine_ptr  routine_ptr = dps->sym->variant.routine.ptr;

  check_assertion(curr_token == tok_assign);
  (void)get_token();
  check_defaulted_or_deleted_function(dps, func_info, &pos_curr_token);
  force_definition_of_compiler_generated_routine(routine_ptr);
  check_assertion(curr_token == tok_delete || curr_token == tok_default ||
                  (microsoft_mode && microsoft_version >= 1400 &&
                   check_context_sensitive_keyword(tok_default, "default")));
  (void)get_token();
}  /* scan_defaulted_or_deleted_definition */


void function_definition(a_symbol_locator      *locator,
                         a_decl_parse_state    *dps,
                         a_func_info_block     *func_info,
                         a_decl_pos_block_ptr  decl_pos_block)
/*
Scan a function definition.  The declarator has already been scanned; the
old-style parameter declarations and the compound statement for the body are
still to come.  *locator is the locator to be used to enter the function
symbol; *dps describes various properties of the declaration, including the
type for the function; *func_info contains information about parameters, as
well as field function_type_from_typedef (when it is FALSE, the function type
came from the declarator; when it is TRUE an error is reported).
This function is also called in the case of a nondefining out-of-class
member declaration (allowed in Microsoft mode only).
*/
{
  a_symbol_ptr                   ext_sym;
  a_routine_ptr                  routine_ptr;
  a_param_id_ptr                 param_id;
  an_id_linkage_kind             linkage;
  a_type_ptr                     old_type, unqualified_rout_type;
  a_routine_type_supplement_ptr  extra_info;
  a_boolean                      prototyped;
  a_param_type_ptr               ptp;
  a_decl_flag_set                flags;

  db_enter(3, "function_definition");
  /* The top type (function) must have come from a declarator, not from a
     typedef (see constraints section of 3.7.1, and associated footnote). */
  if (func_info->function_type_from_typedef) {
    error(ec_function_type_must_come_from_declarator);
    /* Build a copy of the routine type that can be used below, to avoid
       further error recovery problems, and because we need a non-shared
       routine type entry that we can modify. */
    dps->type = copy_routine_type_with_param_types(dps->type,
                                                   /*copy_default_args=*/TRUE);
    unqualified_rout_type = skip_typerefs(dps->type);
  } else {
    unqualified_rout_type = skip_typerefs(dps->type);
    check_assertion(unqualified_rout_type->kind == (a_type_kind)tk_routine);
  }  /* if */
  extra_info = unqualified_rout_type->variant.routine.extra_info;
  prototyped = extra_info->prototyped;
  if (locator->specific_symbol != NULL &&
      locator->specific_symbol->is_class_member) {
    /* This is the definition of a member function. */
    check_assertion(prototyped);
    define_member_function(locator, dps, func_info, &linkage, &old_type,
                           &ext_sym, decl_pos_block);
  } else {
    if (!prototyped) {
      /* Old-style id list.  Before calling decl_routine scan the
         parameter declarations.  It is important for the routine type to
         include all the parameter information in order to do overloading
         involving both prototyped and old-style functions. */
      a_param_type_ptr   old_style_param_types = NULL;
      a_param_type_ptr   end_old_style_param_types = NULL;
      /* Push the name scope for the parameter declarations. */
      (void)push_scope((a_scope_kind)sck_func_prototype,
                       func_info->scope_number, dps->type,
                       (a_routine_ptr)NULL);
      /* Remember the scope number for later use when the body is scanned. */
      func_info->scope_number = scope_stack[depth_scope_stack].number;
#if ASM_FUNCTION_ALLOWED
      if (func_info->is_asm_function && curr_token != tok_lbrace) {
        /* If an asm function is not prototyped, all its old-style params
           have to be implicitly declared. */
        pos_error(ec_asm_func_must_be_prototyped, &pos_curr_token);
      }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
      while (curr_token == tok_identifier ||
             is_decl_start(IDS_REAL_DECLARATOR_ALLOWED)) {
        /* This declaration is checked to make sure the identifier is on the
           param_id_list. */
        declaration(/*function_definition_allowed=*/FALSE, 
                    /*is_old_style_param_decl=*/TRUE,
                    /*is_top_level_declaration=*/FALSE, 
                    /*marked_as_gnu_extension=*/FALSE, 
                    func_info->param_id_list, (a_source_range *)NULL);
      }  /* while */
#if GNU_EXTENSIONS_ALLOWED
      if (gcc_mode && curr_token == tok_ellipsis) {
        /* GNU C allows an old-style parameter list to be followed by
           an ellipsis ("...") to indicate varargs parameters. */
        extra_info->has_ellipsis = TRUE;
        (void)get_token();
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* Transfer the source sequence list in the function prototype scope
         over to the func_info block. */
      func_info->prototype_scope_ss_list =
                         scope_stack[depth_scope_stack].source_sequence_list;
      scope_stack[depth_scope_stack].source_sequence_list = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* Scan the list of identifiers, assigning types to any that remain
         undeclared, and create the param type entries. */
      for (param_id = func_info->param_id_list;
           param_id != NULL;
           param_id = param_id->next) {
        if (param_id->type == NULL) {
          a_symbol_ptr  param_sym = param_id->symbol;
          /* Enter any undeclared parameters with a type of int. */
          param_id->type = integer_type((an_integer_kind)ik_int);
          param_id->declared_type = param_id->type;
          param_id->storage_class = (a_storage_class)sc_auto;
          param_id->implicitly_declared = TRUE;
          copy_source_position(param_sym->decl_position, param_id->type_pos);
          /* Symbols for explicitly declared parameters will already have been
             entered into the symbol table; so the same for parameters that
             are implicitly declared.  In GNU modes, parameters may be marked
             "ambiguous" if they have a duplicate name.  Since such GNU cases
             are diagnosed elsewhere (in decl_parameter), we inhibit the
             redeclaration diagnostic here. */
          reenter_symbol(param_sym, decl_scope_level,
                         (a_boolean)param_sym->ambiguous);
          if (c99_mode) {
            /* In C99, implicit declarations are no longer allowed. */
            pos_sy_diagnostic(strict_ansi_mode ?
                               strict_ansi_discretionary_severity : es_warning,
                              ec_undeclared_parameter,
                              &param_sym->decl_position, param_sym);
          }  /* if */
        }  /* if */
        /* The param_type entry must be allocated in the file-scope
           region. */
        ptp = make_param_type(param_id->type, &param_id->type_pos);
        ptp->declared_type = param_id->declared_type;
        if (remove_qualifiers_from_param_types) {
          /* Strip off top-level type qualifiers.  They are not part of the
             type signature of a C++ function -- see 8.3.5 para 3.  We apply
             this rule even for old-style parameter declarations. */
          a_type_qualifier_set  qualifiers;

          check_assertion(!C_mode());
          qualifiers = get_type_qualifiers(ptp->type);
          if (qualifiers != TQ_NONE) {
            ptp->type = make_unqualified_type(ptp->type);
            /* Record the top-level type qualifiers that were declared for
               this parameter and then removed. */
            ptp->qualifiers = qualifiers;
          }  /* if */
        }  /* if */
        /* Now build the list of parameter types that is attached to the 
           routine type (needed for checking type compatibility -- see
           types_are_compatible). */
        if (old_style_param_types == NULL) {
          old_style_param_types = ptp;
        } else {
          end_old_style_param_types->next = ptp;
        }  /* if */
        end_old_style_param_types = ptp;
      }  /* for */
      /* Set the type to the new type information from the old-style
         parameters just scanned. */
      extra_info->param_type_list = old_style_param_types;
      if (C_mode()) {
        /* Set a flag indicating that old style params were scanned.  This
           is done in case, when this declaration is reconciled with other
           declarations, the prototyped flag is changed -- e.g.,
             void f(int,int);
             void f(i,j) int i; int j { ... }
           where the type associated with the routine entry is marked as
           prototyped but the defining declaration is old-style. */
        extra_info->old_style_params_scanned = TRUE;
      } else {
        /* In C++ mode old style parameter declarations are permitted as
           an anachronism.  However, the internal representation should be
           the same as for a prototyped param list. */
        extra_info->prototyped = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (func_info->declared_type != NULL) {
          /* Replace the declared_type that was recorded in the func-info
             block with one that records the param-type entries. */
          /* It doesn't make any difference how copy_default_args is set;
             there shouldn't be any on an old-style declaration. */
          func_info->declared_type =
              copy_routine_type_with_param_types(dps->type,
                                                 /*copy_default_args=*/FALSE);
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
      /* Parameter symbols are not actually entered in the function
         prototype scope, but other symbols (in consequence of an error or
         a type declaration) may be.  Record them so that they can be
         transferred to the function scope later. */
      func_info->prototype_scope_symbols =
            assoc_pointers_block_of(&scope_stack[depth_scope_stack])->symbols;
      /* Process pragmas associated with the opening brace before the current
         scope is popped.  This means, for old-style param lists, a pragma
         immediately preceding the left brace is interpreted as belonging to
         the function prototype scope; it's different for prototyped
         param lists. */
      process_curr_token_pragmas();
      /* Before popping the scope, move the vla_fixup_list from the
         scope_stack to func_info. */
      func_info->vla_fixup_list =
                         scope_stack[depth_scope_stack].vla_fixup_list;
      scope_stack[depth_scope_stack].vla_fixup_list = NULL;
      /* Pop the function prototype scope. */
      pop_scope();
    } else {
      /* Prototyped. */
      /* Process pragmas associated with the opening brace before pushing
         the function scope.  This means, for prototyped param lists, a pragma
         immediately preceding the left brace is interpreted as belonging to
         the file; it's different for old-style param lists. */
      process_curr_token_pragmas();
    }  /* if */
    /* Create the symbol entry and routine entry for the routine. */
    decl_routine(locator, dps, func_info, (SRK_DECLARATION | SRK_DEFINITION),
                 &linkage, &old_type, &ext_sym, decl_pos_block);
  }  /* if */
  routine_ptr = dps->sym->variant.routine.ptr;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  wrapup_sse_for_simple_decl(dps);
  routine_ptr->declared_storage_class = dps->declared_storage_class;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Run end-of-declaration-parsing actions prior to scanning the function
     body. */
  run_end_of_parse_actions(dps, /*more_declarators=*/FALSE);
  /* Now scan the function body, except if we're dealing with the special
     Microsoft and GNU extension case that allows a nondefining out-of-class
     member declaration. */
  if (curr_token == tok_semicolon &&
      (microsoft_mode || (gpp_mode && gnu_version < 30400)) &&
      (locator->is_class_member || locator->is_error)) {
    /* There is no definition. */
    check_assertion(!gpp_mode || routine_ptr->is_specialized ||
                    locator->is_error);
  } else if (func_info->is_deleted || func_info->is_defaulted) {
    scan_defaulted_or_deleted_definition(dps, func_info);
    (void)required_token(tok_semicolon, ec_exp_semicolon);
  } else {
    flags = SFB_NO_FLAGS;
    if ((dps->dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) == 0) {
      flags |= SFB_IMPLICITLY_DECLARED_RETURN_TYPE;
    }  /* if */
#if MODULE_ID_NEEDED
    if (!locator->is_error) {
      /* If a module id for the current translation unit has not yet been
         created, see if the name of this routine is suitable.  Do this before
         scanning the function body to increase the chance that a module id
         is in place early (as lowering is delayed until a module id has
         been selected). */
      use_variable_or_routine_for_module_id_if_needed(
                                                  &routine_ptr->source_corresp,
                                                  iek_routine);
    }  /* if */
#endif /* MODULE_ID_NEEDED */
    scan_function_body(routine_ptr, func_info, flags);
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
    /* Save the symbol associated with the most recent constructor or
       destructor for which a definition was supplied outside of the
       class definition.  Clear this value when any other member function
       is processed.  This is used to emulate a cfront name lookup bug.
       See check_for_cfront_name_lookup_bug in symbol_tbl.c for more
       information. */
    if (routine_ptr->source_corresp.is_class_member) {
      if (cfront_2_1_mode) {
        if (special_kind_is(routine_ptr, sfk_constructor) ||
            special_kind_is(routine_ptr, sfk_destructor)) {
          last_ctor_or_dtor_sym = dps->sym;
        } else {
          last_ctor_or_dtor_sym = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
  }  /* if */

  db_exit();
}  /* function_definition */


static a_variable_ptr implicitly_generated_param_variable(a_type_ptr  type)
/*
Allocate a parameter variable of the specified type and return a pointer
to it.
*/
{
  a_variable_ptr vp;

  vp = make_param_variable(type, (a_storage_class)sc_auto);
  add_to_parameters_list(vp);
  return(vp);
}  /* implicitly_generated_param_variable */


static void make_generated_constructor_body(a_scope_ptr  scope)
/*
Create the body for a generated constructor (this could be a default
constructor, a default copy constructor, or an inheriting constructor).
The heavy lifting for this function is mostly in ctor_initializer, which
generates the implicit mem-initializer constructs that do the actual member
construction (if any is needed).
*/
{
  a_routine_ptr                  rp;
  a_routine_type_supplement_ptr  rtsp;
  a_param_type_ptr               ptp;

  db_enter(4, "make_generated_constructor_body");
  rp = scope->variant.routine.ptr;
  /* Create the parameter variable -- needed for copy constructors only. */
  rtsp = (skip_typerefs(rp->type))->variant.routine.extra_info;
  for (ptp = rtsp->param_type_list; ptp != NULL; ptp = ptp->next) {
    a_variable_ptr  vp = implicitly_generated_param_variable(ptp->type);
    vp->assoc_param_type = ptp;
  }  /* if */    
  /* Create entries describing constructions to be done in the wrapper code. */
  scope->variant.routine.constructor_inits =
                                  ctor_initializer(rp, /*user_defined=*/FALSE);
  /* Create a statement block that is empty except for the return statement. */
  scope->assoc_block = alloc_statement((a_statement_kind)stmk_block);
  scope->assoc_block->variant.block.statements =
          alloc_statement((a_statement_kind)stmk_return);
  /* See if the fact that this constructor is defined forces definition
     of virtual functions of the class. */
  require_definitions_of_virtual_functions_due_to_definition_of(rp);
  db_exit();
}  /* make_generated_constructor_body */


static void make_default_destructor_body(a_scope_ptr  scope)
/*
Create the body for a default destructor.  It will return no value.
*/
{
  a_routine_ptr rp;

  db_enter(4, "make_default_destructor_body");
  rp = scope->variant.routine.ptr;
  /* Create entries describing destructions to be done in the wrapper code. */
  scope->variant.routine.constructor_inits = dtor_initializer(rp);
  /* Create a statement block that is empty except for the return
     statement. */
  scope->assoc_block = alloc_statement((a_statement_kind)stmk_block);
  scope->assoc_block->variant.block.statements =
          alloc_statement((a_statement_kind)stmk_return);
  /* See if the fact that this destructor is defined forces definition
     of virtual functions of the class. */
  require_definitions_of_virtual_functions_due_to_definition_of(rp);
  db_exit();
}  /* make_default_destructor_body */


static a_statement_ptr make_assignment_call(an_expr_node_ptr  source_expr,
                                            an_expr_node_ptr  dest_expr,
                                            a_routine_ptr     rp,
                                            a_source_position *err_pos)
/*
Return a statement pointer that represents a call to an assignment operator.
source_expr is an lvalue for the source of the assignment; dest_expr is an
lvalue for the destination.  rp is the pointer to the routine entry for
the operator= function.  source_expr and dest_expr will be converted as
necessary for use as the argument and the "this" parameter of the call.
*err_pos is the source position for diagnostics.
*/
{
  a_param_type_ptr  ptp;
  a_statement_ptr   sp;

  /* Get the first parameter of the assignment operator, which represents the
     source type. */
  ptp = skip_typerefs(rp->type)->variant.routine.extra_info->param_type_list;
  /* Convert the source for use as the argument, e.g., cast it to a base
     class or add a copy constructor call. */
  source_expr = prep_generated_arg_expr(source_expr, ptp, err_pos);
  /* Convert the lvalue for the destination into a pointer for the "this"
     argument. */
  dest_expr = add_address_of_to_node(dest_expr);
  /* Calls generated are non-virtual; see [class.copy]/13 in the C++
     standard. */
  sp = make_call_assignment_statement(rp, /*suppress_virtual=*/TRUE,
                                      dest_expr, source_expr, err_pos);
  return sp;
}  /* make_assignment_call */


static an_expr_node_ptr lvalue_for_source_param(a_variable_ptr source_var)
/*
source_var is the variable for the source parameter of an operator=
assignment function.  Create and return an lvalue that refers to the source
object, including for the case where the parameter has a reference type.
*/
{
  an_expr_node_ptr  source_expr;

  /* A compiler-generated operator= function always has a reference-typed
     source parameter, but that is not necessarily the case for a defaulted
     operator=. */
  if (is_any_reference_type(source_var->type)) {
    source_expr = var_rvalue_expr(source_var);
    source_expr = add_ref_indirection_to_node(source_expr);
  } else {
    source_expr = var_lvalue_expr(source_var);
  }  /* if */
  return source_expr;
}  /* lvalue_for_source_param */


static void make_default_assignment_body(a_scope_ptr  scope)
/*
Create the body for a default assignment operator.  Typically it will
entail a series of member-wise and base-class-wise assignment operations:
based on the properties of the subobject, it will either call an assignment
operator routine or do bitwise assignment.
*/
{
  a_type_ptr                     class_type, tp, array_type;
  a_routine_ptr                  rout;
  a_routine_type_supplement_ptr  rtsp;
  a_statement_ptr                sp;
  a_statement                    head_of_statement_list;
  a_variable_ptr                 source_var;
  an_expr_node_ptr               source_expr, dest_expr;
  a_base_class_ptr               bcp;
  a_field_ptr                    fp;
  a_routine_ptr                  rp;
  a_symbol_ptr                   sym;
  a_param_type_ptr               ptp;
  a_boolean                      bitwise_assign, move_assign;
  a_source_position              *err_pos;

  db_enter(4, "make_default_assignment_body");
  /* The source variable of the copy is the first parameter on the parameters
     list for the routine.  There must be exactly one parameter for an
     assignment function. */
  rout = scope->variant.routine.ptr;
  rtsp = (skip_typerefs(rout->type))->variant.routine.extra_info;
  ptp = rtsp->param_type_list;
  move_assign = is_rvalue_reference_type(ptp->type);
  source_var = implicitly_generated_param_variable(ptp->type);
  source_var->assoc_param_type = ptp;
  class_type =
          type_pointed_to(scope->variant.routine.this_param_variable->type);
  err_pos = &class_type->source_corresp.decl_position;
  /* "head_of_statement_list" is a local statement variable whose only
      interesting property is its "next" field, from which a linked list of
      allocated statement entries will be hung.  That list will eventually be
      transferred to the block statement that is created. */
  head_of_statement_list.next = NULL;
  sp = &head_of_statement_list;
  /* See if a bitwise copy is all that is called for. */
  if (rout->is_trivial_copy_function) {
    /* Yes.  (Then why are we defining a routine?  Probably because the
       address of the default assignment operator was taken, forcing the
       actual creation of the routine.) */
    /* Get the source and destination expressions to use as operands for an
       assignment statement. */
    source_expr = lvalue_for_source_param(source_var);
    source_expr = rvalue_expr_for_lvalue(source_expr);
    dest_expr = add_indirection_to_node(this_param_value_expr());
    sp = sp->next = make_assignment_statement(dest_expr, source_expr);
  } else {
    /* Memberwise copy is required.  That is, first do the appropriate
       operation on each direct base class (direct assignment or calling
       the base class's assignment function), and then do the appropriate
       copy of each member. */
    for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
      if (bcp->direct) {
        /* We are only interested in direct base classes. */
        if (bcp->is_virtual &&
            virtual_base_class_is_indirect(bcp, class_type)) {
          /* If it is also an indirect base class, it will be handled by
             the assignment function of some other base class. */
          continue;
        }  /* if */
        /* The destination is always the implicit "this" parameter cast to
           the appropriate base class. */
        dest_expr = base_class_selection_expr(this_param_value_expr(), bcp);
        dest_expr = add_indirection_to_node(dest_expr);
        /* The source is the first parameter cast to the same base class. */
        source_expr = lvalue_for_source_param(source_var);
        source_expr = add_address_of_to_node(source_expr);
        source_expr = base_class_selection_expr(source_expr, bcp);
        source_expr = add_indirection_to_node(source_expr);
        if (symbol_supplement_for_class(bcp->type)
                                       ->assignment_by_bitwise_copy_allowed) {
          /* A bitwise copy may be performed. */
          source_expr = rvalue_expr_for_lvalue(source_expr);
          /* Create the assignment statement.  The appropriate operator
             will be selected by the function. */
          sp = sp->next = make_assignment_statement(dest_expr, source_expr);
        } else {
          /* A bitwise copy may not be done.  Find the default assignment
             operator and put out a call to it. */
          if (move_assign) {
            /* For move assignment, do the assignment from an xvalue. */
            source_expr = xvalue_expr_for_lvalue(source_expr);
          }
          rp = find_assignment_operator_for_memberwise_copy(
                                                          bcp->type,
                                                          source_expr,
                                                          dest_expr,
                                                          &bcp->decl_position);
          if (rp == NULL) {
            /* Error has already been issued in the subroutine. */
            continue;
          }  /* if */
          sp = sp->next = make_assignment_call(source_expr, dest_expr, rp,
                                               err_pos);
        }  /* if */
      }  /* if */
      /* Advance to the next base class. */
    }  /* for */
    /* Now go through all the fields, copying them one at a time.  Use the
       symbol list rather than the field list to be sure we adhere to
       declaration order and to be sure only user defined fields are
       copied. */
    sym = ((a_symbol_ptr)class_type->source_corresp.assoc_info)->
                           variant.class_struct_union.extra_info->symbols;
    for (; sym != NULL; sym = sym->next_in_scope) {
      if (sym->kind == (a_symbol_kind)sk_field) {
        /* A field. */
        fp = sym->variant.field.ptr;
        tp = skip_typerefs(fp->type);
        if (field_is_nontrivial_property_or_event(fp)) {
          /* Property and event fields aren't really data members and thus are
             not copied.  (C++/CLI "trivial" properties and events do have
             associated storage represented by the field and hence are copied
             here.)*/
          continue;
        }  /* if */
        if (is_const_qualified_type(tp) || is_any_reference_type(tp)) {
          /* The error has already been issued for const and ref members.
             Don't bother trying to do the copy. */
          check_assertion(total_errors > 0);
          continue;
        }  /* if */
        /* If this is an array, we need the element type. */
        if (is_array_type(tp)) {
          array_type = tp;
          tp = f_skip_typerefs(underlying_array_element_type(tp));
        } else {
          array_type = NULL;
        }  /* if */
        /* The destination is the appropriate field of the class pointed to
           by the "this" parameter, as an lvalue. */
        dest_expr = fe_field_lvalue_selection_expr(this_param_value_expr(),
                                                   fp);
        /* The source will be the corresponding field of the class pointed to
           by the source parameter. */
        source_expr = lvalue_for_source_param(source_var);
        source_expr = fe_field_lvalue_selection_expr(source_expr, fp);
        if (is_class_struct_union_type(tp)) {
          /* It's a class type, so we may have to call an assignment operator
             function. */
          if (symbol_supplement_for_class(tp)
                                      ->assignment_by_bitwise_copy_allowed) {
            /* A bitwise copy can be performed. */
            bitwise_assign = TRUE;
          } else {
            a_statement_ptr call_stmt;
            /* A bitwise copy cannot be done.  Find the default assignment
               operator and put out a call to it. */
            bitwise_assign = FALSE;
            if (array_type != NULL) {
              /* Copying an array of classes.  Generate a loop around the
                 call of the assignment routine, like
                   tmp = 0;
                   do {
                     assignfunc(&dest[tmp], &src[tmp]);
                   } while (++tmp < num_elements);
                 Note that copying of a multidimensional array is done
                 as a single loop for all the elements, treating the array
                 as a single-dimensional array of the ultimate underlying
                 element type. */
              a_variable_ptr   temp_var;
              an_expr_node_ptr temp_node, temp_incr_node, compare_node;
              a_type_ptr       size_t_type;
              a_targ_size_t    num_elems;

              size_t_type = integer_type(targ_size_t_int_kind);
              temp_var = alloc_temporary_variable(size_t_type,
                                                  /*force_static=*/FALSE);
              /* Make "tmp = 0;" */
              temp_node = var_lvalue_expr(temp_var);
              sp = sp->next =
                make_assignment_statement(temp_node,
                                          node_for_integer_constant(
                                                    0L, targ_size_t_int_kind));
              /* Make "++tmp < num_elements". */
              temp_node = var_lvalue_expr(temp_var);
              temp_incr_node = make_operator_node(
                                          (an_expr_operator_kind)eok_pre_incr,
                                          size_t_type, temp_node);
              num_elems = skip_typerefs(array_type)->size / tp->size;
              temp_incr_node->next = node_for_host_large_integer(
                        (a_host_large_integer)num_elems, targ_size_t_int_kind);
              compare_node =
                      make_operator_node((an_expr_operator_kind)eok_lt,
                                         boolean_result_type(),
                                         temp_incr_node);
              /* Make the do-while statement. */
              sp = sp->next = alloc_statement(
                                        (a_statement_kind)stmk_end_test_while);
              sp->expr = compare_node;
              /* Convert the source and destination expressions from
                 array lvalue to pointer-to-array-element.  Loop if the
                 array is multidimensional. */
              for (;;) {
                source_expr = conv_array_expr_to_pointer(source_expr);
                if (!(is_pointer_type(source_expr->type) &&
                      is_array_type(type_pointed_to(source_expr->type)))) {
                  break;
                }  /* if */
                source_expr = add_indirection_to_node(source_expr);
              }  /* for */
              for (;;) {
                dest_expr = conv_array_expr_to_pointer(dest_expr);
                if (!(is_pointer_type(dest_expr->type) &&
                      is_array_type(type_pointed_to(dest_expr->type)))) {
                  break;
                }  /* if */
                dest_expr = add_indirection_to_node(dest_expr);
              }  /* for */
              /* Add the subscript to the source_expr. */
              source_expr->next = var_rvalue_expr(temp_var);
              source_expr =
                      make_operator_node((an_expr_operator_kind)eok_subscript,
                                         type_pointed_to(source_expr->type),
                                         source_expr);
              source_expr->is_lvalue = TRUE;
              /* Add the subscript to the dest_expr. */
              dest_expr->next = var_rvalue_expr(temp_var);
              dest_expr =
                      make_operator_node((an_expr_operator_kind)eok_subscript,
                                         type_pointed_to(dest_expr->type),
                                         dest_expr);
              dest_expr->is_lvalue = TRUE;
              /* Now that we have element lvalues, we can find the right
                 assignment operator. */
            }  /* if */
            if (move_assign) {
              /* For move assignment, do the assignment from an xvalue. */
              source_expr = xvalue_expr_for_lvalue(source_expr);
            }
            /* Find the assignment operator to do the copy. */
            rp = find_assignment_operator_for_memberwise_copy(
                                            tp, source_expr, dest_expr,
                                            &fp->source_corresp.decl_position);
            if (rp == NULL) {
              /* Error has already been issued in the subroutine. */
              continue;
            }  /* if */
            call_stmt = make_assignment_call(source_expr, dest_expr, rp,
                                             err_pos);
            if (array_type != NULL) {
              /* Array case; the call goes under the do-while. */
              sp->variant.loop_statement = call_stmt;
            } else {
              /* Non-array case; the call goes at the end of the statement
                 sequence. */
              sp = sp->next = call_stmt;
            }  /* if */
          }  /* if */
        } else {
          /* Not a class type.  Just do a bitwise copy. */
          bitwise_assign = TRUE;
        }  /* if */
        if (bitwise_assign) {
          /* Do a bitwise assignment. */
          if (array_type != NULL) {
            /* Array type.  Do a special assignment (source operand is an
               lvalue). */
            sp = sp->next =
                       make_array_assignment_statement(dest_expr, source_expr);
        
          } else {
            /* Not an array.  The appropriate IL operator will be selected
               by make_assignment_statement. */
            source_expr = rvalue_expr_for_lvalue(source_expr);
            sp = sp->next = make_assignment_statement(dest_expr, source_expr);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  /* Make the return statement.  A pointer to the variable assigned to is
     the return value. */
  sp = sp->next = alloc_statement((a_statement_kind)stmk_return);
  sp->expr = this_param_value_expr();
  /* We now have a list of one or more statements hanging off the local
     variable head_of_statement_list.  The start of the list is pointed to
     by the next field.  Create a block statement and attach the list to
     it. */
  scope->assoc_block = alloc_statement((a_statement_kind)stmk_block);
  scope->assoc_block->variant.block.statements = head_of_statement_list.next;
  db_exit();
  return;
}  /* make_default_assignment_body */


static void check_default_assignment_operator(a_type_ptr  class_type)
/*
Issue an error if a compiler-generated assignment operator is not allowed
because the class has a const or ref member (ARM 12.8).  The case of a
member or a base class with a nonpublic operator=() is handled elsewhere.
*/
{
  a_boolean     err, is_ref, is_const;
  a_symbol_ptr  sym;
  a_type_ptr    tp;

  db_enter(4, "check_default_assignment_operator");
  if (class_type->variant.class_struct_union.any_const_member ||
      symbol_supplement_for_class(class_type)->any_ref_member) {
    /* An error is issued only if a immediate member of the class is const or
       ref.  Those in base classes or embedded within members are diagnosed
       elsewhere. */
    err = FALSE;
    /* Go through all the fields, using the symbol list rather than the field
       list to be sure only user defined fields are checked and to be sure
       anonymous union fields are picked up. */
    sym = ((a_symbol_ptr)class_type->source_corresp.assoc_info)->
                           variant.class_struct_union.extra_info->symbols;
    for (; sym != NULL; sym = sym->next_in_scope) {
      if (sym->kind == (a_symbol_kind)sk_field) {
        tp = sym->variant.field.ptr->type;
        is_ref = is_const = FALSE;
        if (field_is_nontrivial_property_or_event(sym->variant.field.ptr)) {
          /* Property and event fields are not copied by the default
             assignment operator (C++/CLI "trivial" properties and events are
             an exception because the field also represents associated
             storage). */
        } else if (is_any_reference_type(tp)) {
          /* An assignment operator should not be generated if a member has a
             ref type. */
          is_ref = TRUE;
        } else if (is_const_qualified_type(tp)) {
          /* An assignment operator should not be generated if a member has a
             const type. */
          is_const = TRUE;
        }  /* if */
        if (is_ref || is_const) {
          if (!err) {
            /* Multi-line diagnostic has not been started yet. */
            pos_start_error(ec_bad_default_assignment,
                            &class_type->source_corresp.decl_position);
          }  /* if */
          sym_add_diag_info(is_ref ? ec_reference_member : ec_const_member,
                            sym);
          err = TRUE;
        }  /* if */
      }  /* if */
    }  /* for */
    if (err) end_error();
  }  /* if */
  db_exit();
}  /* check_default_assignment_operator */


static void define_special_member_function(a_routine_ptr  rout_ptr)
/*
Define a compiler generated routine for a member function (constructor or
destructor).  This entails creating a new memory region, a scope, and an
empty statement block.
*/
{
  a_scope_ptr                    scope;
  a_type_ptr                     class_type;
  a_routine_type_supplement_ptr  rtsp;

  db_enter(4, "define_special_member_function");
  class_type = parent_class_of(rout_ptr);
  if (class_type->variant.class_struct_union.is_nonreal_class) {
    /* Don't bother generating the definition for a member of an unreal
       instantiation of a template class. */
  } else {
    a_scope_depth                   saved_innermost_scope_that_affects_access;
    a_symbol_ptr                    rout_sym = symbol_for(rout_ptr);
    a_boolean                       trans_unit_pushed;
    check_assertion(class_type->variant.class_struct_union.extra_info
                              ->assoc_scope != NULL);
    /* Switch translation units if necessary. */
    trans_unit_pushed = push_translation_unit_if_needed(rout_sym);
    /* Reset the innermost scope that affects access control so that any
       existing context on the scope stack does not affect the generation
       of the function. */
    saved_innermost_scope_that_affects_access =
                         depth_of_innermost_scope_that_affects_access_control;
    depth_of_innermost_scope_that_affects_access_control = NO_SCOPE_DEPTH;
    /* Push a class symbol reactivation scope, to make class member names
       visible for processing the function definition. */
    push_class_and_template_reactivation_scope_full(
                                           class_type,
                                           /*reactivate_template_params=*/TRUE,
                                           /*extend_namespace=*/TRUE,
                                           /*force_new_context=*/TRUE);
    /* Push the scope for the new function itself. */
    scope = push_scope((a_scope_kind)sck_function, NO_SCOPE_NUMBER,
                       (a_type_ptr)NULL, rout_ptr);
    /* Associate the scope to the routine entry and the routine entry to its
       type entry. */
    rout_ptr->assoc_scope = curr_il_region_number;
    /* If this is an "extern inline" function, change its storage class. */
    if (rout_ptr->storage_class == (a_storage_class)sc_extern) {
      rout_ptr->storage_class = (a_storage_class)sc_unspecified;
    }  /* if */
    if (has_indeterminate_exception_spec(rout_ptr)) {
      /* A default constructor whose exception specification hasn't been
         determined yet because it depended on field initializers.  In GNU C++
         mode, all special member functions have their exception specification
         delayed this way. */
      resolve_indeterminate_exception_specification(rout_ptr);
    }  /* if */
    rtsp = skip_typerefs(rout_ptr->type)->variant.routine.extra_info;
    rtsp->assoc_routine = rout_ptr;
    if (rtsp->this_class != NULL) {
      scope->variant.routine.this_param_variable =
                            make_implicit_this_param_variable(rout_ptr->type);
    }  /* if */
    /* Enter the constructor and destructor initializers, to record possible
       implicit initializers. */
    if (rout_ptr->special_kind == (a_special_function_kind)sfk_constructor) {
      make_generated_constructor_body(scope);
    } else if (rout_ptr->special_kind ==
                                  (a_special_function_kind)sfk_destructor) {
      make_default_destructor_body(scope);
    } else {
      /* Assignment operator case. */
      check_assertion(rout_ptr->special_kind ==
                                   (a_special_function_kind)sfk_operator &&
                      rout_ptr->variant.opname_kind == 
		                   (an_opname_kind)onk_assign);
      check_default_assignment_operator(class_type);
      make_default_assignment_body(scope);
    }  /* if */
    /* End of statement block is unreachable because of the return
       statement. */
    check_assertion(scope->assoc_block->kind == (a_statement_kind)stmk_block);
    scope->assoc_block->
                   variant.block.extra_info->end_of_block_reachable = FALSE;
    if (rout_ptr->is_constexpr &&
        check_constexpr_routine_def_type(
                         rout_ptr, &rout_ptr->source_corresp.decl_position)) {
      /* A default constructor satisfies the rules for a constexpr
         constructor function body. */
      set_routine_constexpr_info(scope, /*constexpr_ruled_out=*/FALSE);
    }  /* if */
    /* Terminate the function scope. */
    pop_scope();
    /* Terminate the class reactivation scope. */
    pop_class_reactivation_scope();
    depth_of_innermost_scope_that_affects_access_control =
                                    saved_innermost_scope_that_affects_access;
    /* Mark the symbol for this routine "defined". */
    rout_sym->defined = TRUE;
    /* Notify the correspondence routines that a definition of this function
       is now present.  Note that this is done for both template classes
       and normal classes. */
    establish_function_instantiation_corresp(rout_ptr);
    /* If the translation unit stack was pushed above, pop it now. */
    if (trans_unit_pushed) pop_translation_unit_stack();
  }  /* if */
  db_exit();
}  /* define_special_member_function */


void force_definition_of_compiler_generated_routine(a_routine_ptr  rp)
/*
If rp points to a compiler-generated routine that is being referenced and
whose definition has not yet been generated (if it's marked as "deleted", it
is considered already defined), force the definition now.
*/
{
  a_special_function_kind  skind = rp->special_kind;

  if ((rp->compiler_generated || rp->is_defaulted) && !rp->is_deleted) {
    if (!routine_has_been_defined(rp)) {
      /* Only force a definition for constructors, destructors, and
         operator= functions.  In particular, do not try to define operator
         new and delete functions. */
      if (skind == (a_special_function_kind)sfk_constructor ||
          skind == (a_special_function_kind)sfk_destructor  ||
          (skind == (a_special_function_kind)sfk_operator &&
           rp->variant.opname_kind == (an_opname_kind)onk_assign)) {
        a_type_ptr  parent_type = parent_class_of(rp);
        a_class_symbol_supplement_ptr
                    cssp = symbol_supplement_for_class(parent_type);
        if (skind == (a_special_function_kind)sfk_constructor &&
            cssp->has_initializer_fixups &&
            !is_immediate_managed_class_type(parent_type) &&
            is_default_constructor(rp, /*is_declarative_context=*/TRUE)) {
          /* Don't generate the default constructor body at this time because
             required field initializers haven't been parsed yet.  Instead set
             a flag indicating that the definition has been delayed so that it
             can be generated once the field initializers have been parsed. */
          cssp->default_ctor_body_delayed = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (cli_or_cx_enabled &&
                   is_immediate_delegate_type(parent_type) &&
                   skind == (a_special_function_kind)sfk_constructor) {
          /* The generated constructor declaration of a delegate class type
             is not one whose body can be generated. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else {
          define_special_member_function(rp);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* force_definition_of_compiler_generated_routine */


#if !(DO_IL_LOWERING && ABI_COMPATIBILITY_VERSION < 238)
/* ARGSUSED */ /* <-- scope is not used in that case. */
#endif /* !(DO_IL_LOWERING && ABI_COMPATIBILITY_VERSION < 238) */
void generate_required_virtual_destructor_bodies(a_scope_ptr  scope)
/*
Go through the classes on the types list of the indicated scope and generate
bodies for virtual destructors, as required.  Then (if it is a file or
namespace scope) check the scopes for each namespace defined in the
indicated scope.  This is mostly vestigial, but it's been kept as a hook
in case it's useful.
*/
{
#if DO_IL_LOWERING && ABI_COMPATIBILITY_VERSION < 238
  /* The only case left is generation of a destructor if needed because
     a typeinfo variable points to it. */
  a_namespace_ptr                nsp;
  a_type_ptr                     tp;
  a_routine_ptr                  rp;
  a_class_symbol_supplement_ptr  cssp;
  a_class_type_supplement_ptr    ctsp;

  db_enter(3, "generate_required_virtual_destructor_bodies");
  /* First go through all the classes declared in the indicated scope. */
  for (tp = scope->types; tp != NULL; tp = tp->next) {
    if (is_immediate_class_type(tp)) {
      ctsp = tp->variant.class_struct_union.extra_info;
      if (ctsp->assoc_scope == NULL) {
        /* Class has no definition. */
      } else if (tp->source_corresp.assoc_info == NULL) {
        /* Class has no tag symbol.  This serves to eliminate types
           generated by IL lowering. */
      } else {
        cssp = symbol_supplement_for_class(tp);
        if (has_nontrivial_destructor(cssp)) {
          rp = cssp->destructor->variant.routine.ptr;
          if (external_typeinfo_will_be_defined_for_class(tp)) {
            /* The destructor for the current class is needed because it is
               referenced from the typeinfo variable.  Create a definition
               if needed, and force its instantiation if a template. */
            mark_routine_referenced(rp);
          }  /* if */
        }  /* if */
        /* Do the same check for nested classes, if any. */
        generate_required_virtual_destructor_bodies(ctsp->assoc_scope);
      }  /* if */
    }  /* if */
  }  /* for */
  if (scope->kind == (a_scope_kind)sck_file ||
      scope->kind == (a_scope_kind)sck_namespace) {
    /* Next go though all the namespaces, calling this routine recursively
       for each (excluding namespace aliases). */
    for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
      if (!nsp->is_namespace_alias) {
        generate_required_virtual_destructor_bodies(nsp->variant.assoc_scope);
      }  /* if */
    }  /* for */
  }  /* if */
  db_exit();
#endif /* DO_IL_LOWERING && ABI_COMPATIBILITY_VERSION < 238 */
}  /* generate_required_virtual_destructor_bodies */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2013 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
