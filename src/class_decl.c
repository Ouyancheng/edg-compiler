/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

class_decl.c -- Scanning of class declarations.

*/

#include "basics.h"
#include "class_decl.h"
#include "decls.h"
#include "il.h"
#include "symbol_tbl.h"
#include "statements.h"
#include "lexical.h"
#include "error.h"
#include "expr.h"
#include "exprutil.h"
#include "cmd_line.h"
#include "types.h"
#include "mem_tables.h"
#include "mem_manage.h"
#include "expr.h"
#include "target.h"
#include "decl_inits.h"
#include "preproc.h"
#if ASM_FUNCTION_ALLOWED
#include "asm_func.h"
#endif /* ASM_FUNCTION_ALLOWED */


/*
Structure for keeping track of token caches and associated IL entities
when scanning must be delayed till the end of a class definition.
*/
typedef struct a_delayed_scan_fixup {
  a_delayed_scan_fixup_ptr
		next;	/* Next in a linked list of entries representing
			   token sequences to be rescanned and IL entities
			   to be updated. */
  a_byte_boolean
		is_arg_default_value;
			/* TRUE when the IL entity is a default value in an
			   argument declaration;  FALSE when the it is a
			   routine body, optionally including constructor
			   initializers. */
  a_token_cache token_cache;
			/* The structure containing a linked list of cached
			   tokens, representing the tokens to be rescanned. */
  union {
    /* When is_arg_default_value is TRUE: */
    a_param_type_ptr
		param_type;
			/* Pointer to the parameter type entry whose default
			   value is specified by the tokens in token_cache. */
    /* When is_arg_default_value is FALSE: */
    struct {
      a_routine_ptr
		routine;
			/* Pointer to the routine entry whose body is
			   specified by the tokens in token_cache. */
      a_func_info_block
		extra_info;
			/* Information saved by declarator processing for
			   use in function definition processing. */
    } inline_func;
  } variant;
} a_delayed_scan_fixup;


/* Previously allocated delayed-scan-fixup entries available for reuse. */
static a_delayed_scan_fixup_ptr avail_delayed_scan_fixup;


static a_delayed_scan_fixup_ptr alloc_delayed_scan_fixup(a_boolean is_arg_def)
/*
Allocate and initialize a delayed-scan-fixup entry.
*/
{
  a_delayed_scan_fixup_ptr  dsfp;

  if (avail_delayed_scan_fixup != NULL) {
    /* Reuse a previously allocated entity. */
    dsfp = avail_delayed_scan_fixup;
    avail_delayed_scan_fixup = dsfp->next;
  } else {
    /* Allocate memory for a new entity. */
    dsfp = (a_delayed_scan_fixup_ptr)alloc_fe(sizeof(a_delayed_scan_fixup));
#if 0
#if DEBUG
    num_delayed_scan_fixups_allocated++;
#endif /* DEBUG */
#endif /* if 0 */
  }  /* if */
  /* Clear the entity. */
  dsfp->next = NULL;
  dsfp->is_arg_default_value = is_arg_def;
  clear_token_cache(&dsfp->token_cache);
  if (is_arg_def) {
    dsfp->variant.param_type = NULL;
  } else {
    dsfp->variant.inline_func.routine = NULL;
    clear_func_info(&dsfp->variant.inline_func.extra_info);
  }  /* if */
  return dsfp;
}  /* alloc_delayed_scan_fixup */


static void free_delayed_scan_fixup(a_delayed_scan_fixup_ptr  dsfp)
/*
Return a delayed-scan-fixup entry to the available list.
*/
{
  dsfp->next = avail_delayed_scan_fixup;
  avail_delayed_scan_fixup = dsfp;
}  /* free_delayed_scan_fixup */


static void add_to_delayed_scan_fixup_list(a_delayed_scan_fixup_ptr dsfp,
                                           a_scope_stack_entry_ptr  ssep)
/*
Add a delayed-scan-fixup entry to the end of the list for the class
associated with the indicated scope stack entry.
*/
{
#if CHECKING
  if (ssep->il_scope->kind != (a_scope_kind)sck_class_struct_union) {
    internal_error("add_to_delayed_scan_fixup_list: bad scope kind");
  }  /* if */
#endif /* CHECKING */
  if (ssep->last_delayed_scan_fixup == NULL) {
    (symbol_supplement_for_class(ssep->assoc_type))->
                                    delayed_scan_fixup_list = dsfp;
  } else {
    ssep->last_delayed_scan_fixup->next = dsfp;
  }  /* if */
  ssep->last_delayed_scan_fixup = dsfp;
}  /* add_to_delayed_scan_fixup_list */


static a_boolean prescan_function_definition(a_routine_ptr     rout_ptr,
                                             a_func_info_block *extra_info_ptr)
/*
Place the tokens for a function definition (including, perhaps, the
constructor initializer) into a token cache, to await actual processing
at a later point.  The current token is either a left brace or, when a
constructor initializer is present, a colon.
*/
{
  a_delayed_scan_fixup_ptr  dsfp;
  a_stop_token_array        save_stop_token_array;
  a_boolean                 success = FALSE;
  a_token_kind              save_curr_token;

  db_enter(3, "prescan_function_definition");
  /* Allocate a delayed scan fixup entry.  The subroutine performs a clear
     cache operation on the token_cache field, so there's no need to do it
     again. */
  dsfp = alloc_delayed_scan_fixup(/*is_arg_def=*/FALSE);
  /* Associate the routine with the entry just created. */
  dsfp->variant.inline_func.routine = rout_ptr;
  memcpy((char *)&dsfp->variant.inline_func.extra_info,
         (char *)extra_info_ptr, sizeof(a_func_info_block));
  /* Save the current stop token state, and reinitialize it. */
  copy_stop_tokens(stop_token_array, save_stop_token_array);
  clear_stop_tokens();
  add_stop_token(tok_rbrace);
  if (curr_token == tok_colon) {
    /* A colon marks the start of a constructor initializer list.  Scan it,
       stopping at the left brace, where the function body is expected to
       start.  Just in case the function body is missing, also stop when a
       semicolon is seen. */
    add_stop_token(tok_semicolon);
    add_stop_token(tok_lbrace);
    cache_token_stream(&dsfp->token_cache);
    remove_stop_token(tok_lbrace);
    remove_stop_token(tok_semicolon);
  }  /* if */
  if (curr_token == tok_lbrace) {
    /* The left brace marks the start of the function body.  Cache all the
       tokens up to the right brace. */
    cache_curr_token(&dsfp->token_cache);
    (void)get_token();
    cache_token_stream(&dsfp->token_cache);
  }  /* if */
  remove_stop_token(tok_rbrace);
  if (curr_token == tok_rbrace) {
    cache_curr_token(&dsfp->token_cache);
    success = TRUE;
  }  /* if */
  /* Add an end-of-source token to the end of the token cache.  This assures
     that we won't scan past the end of the cache in the actual scan. */
  save_curr_token = curr_token;
  curr_token = tok_end_of_source;
  cache_curr_token(&dsfp->token_cache);
  curr_token = save_curr_token;
  /* Restore the original stop token state. */
  copy_stop_tokens(save_stop_token_array, stop_token_array);
  /* Add the delayed scan fixup entry to the list for the class. */
  add_to_delayed_scan_fixup_list(dsfp, &scope_stack[depth_scope_stack]);
  db_exit();
  return success;
}  /* prescan_function_definition */


void prescan_default_arg_expr(a_param_type_ptr  ptp)
/*
Place the tokens for a default argument expression into a token cache, to
await actual processing at a later point.
*/
{
  a_delayed_scan_fixup_ptr  dsfp;
  a_stop_token_array        save_stop_token_array;
  a_token_kind              save_curr_token;

  db_enter(3, "prescan_default_arg_expr");
  /* Allocate a delayed scan fixup entry.  The subroutine performs a clear
     cache operation on the token_cache field, so there's no need to do it
     again. */
  dsfp = alloc_delayed_scan_fixup(/*is_arg_def=*/TRUE);
  dsfp->variant.param_type = ptp;
  /* Save the current stop token state, and reinitialize it. */
  copy_stop_tokens(stop_token_array, save_stop_token_array);
  clear_stop_tokens();
  /* In the normal case we will scan an expression and encounter a comma
     or right parenthesis.  If both of these are omitted, terminate the token
     stream when some likely delimiter is reached. */
  add_stop_token(tok_comma);
  add_stop_token(tok_rparen);
  add_stop_token(tok_semicolon);
  add_stop_token(tok_lbrace);
  add_stop_token(tok_rbrace);
  cache_token_stream(&dsfp->token_cache);
  /* Note that the terminating token (comma, rparen, etc.) is not added to
     the cache. */
  /* Add an end-of-source token to the end of the token cache.  This assures
     that we won't scan past the end of the cache in the actual scan. */
  save_curr_token = curr_token;
  curr_token = tok_end_of_source;
  cache_curr_token(&dsfp->token_cache);
  curr_token = save_curr_token;
  /* Restore the original stop token state. */
  copy_stop_tokens(save_stop_token_array, stop_token_array);
  /* Add the delayed scan fixup entry to the list for the class. */
  add_to_delayed_scan_fixup_list(dsfp, &scope_stack[depth_scope_stack-1]);
  db_exit();
}  /* prescan_default_arg_expr */


static void delayed_scan_of_default_arg_expr(a_param_type_ptr param_type_entry)
/*
Do the delayed scan of the default argument expression for a parameter.  The
cache has just been reactivated, so curr_token should represent the first
token in the cache.  Before doing the scan check that default expressions have
been declared for all successor arguments.
*/
{
  a_param_type_ptr  ptp;
  a_boolean         err = FALSE;

  db_enter(3, "delayed_scan_of_default_arg_expr");
#if CHECKING
  if (param_type_entry->default_arg_expr != NULL) {
    internal_error(
                "delayed_scan_of_default_arg_expr: default arg already there");
  }  /* if */
#endif /* CHECKING */
  /* Make a pass over all the param type entries that follow the current one.
     It is an error if there are any without a default argument. */
  for (ptp = param_type_entry->next; ptp != NULL; ptp = ptp->next) {
#if CHECKING
    if (ptp->default_arg_expr != NULL) {
      internal_error("delayed_scan_of_default_arg_expr: bad param order");
    }  /* if */
#endif /* CHECKING */
    if (!ptp->has_default_arg) {
      /* Issue an error on the first successor in the parameter list that does
         not have a default argument. */
      if (!err) {
        pos_error(ec_default_arg_not_at_end, &pos_curr_token);
        err = TRUE;
      }  /* if */
      ptp->has_default_arg = TRUE;
      ptp->default_arg_expr = error_node();
    }  /* if */
  }  /* for */
  /* We scan the expression whether an error was detected or not. */
  scan_default_arg_expr(param_type_entry);
  db_exit();
}  /* delayed_scan_of_default_arg_expr */


static void delayed_scan_fixup_for_class(a_symbol_ptr  class_sym)
/*
Process the default argument expressions and inline function definitions
for the indicated class.  If the class contains nested classes, call this
routine recursively for each nested class.
*/
{
  a_delayed_scan_fixup_ptr       dsfp, next_dsfp;
  a_type_ptr                     class_type;
  a_class_symbol_supplement_ptr  cssp;
  a_symbol_ptr                   sym;

  db_enter(3, "delayed_scan_fixup_for_class");
  cssp = class_sym->variant.class_struct_union.extra_info;
  /* Process nested classes first. */
  if (cssp->any_nested_classes) {
    for (sym = cssp->symbols; sym != NULL; sym = sym->next_in_scope) {
      if (sym->kind == (a_symbol_kind)sk_class_or_struct_tag ||
          sym->kind == (a_symbol_kind)sk_union_tag) {
        delayed_scan_fixup_for_class(sym);
      }  /* if */
    }  /* for */
  }  /* if */
  /* Do processing for the current class only if there are tokens cached for
     delayed scanning ("rewriting"). */
  dsfp = cssp->delayed_scan_fixup_list;
  if (dsfp != NULL) {
    class_type = skip_typerefs(class_sym->variant.class_struct_union.type);
#if DEBUG
    if (debug_level >= 3) {
      fputs("delayed scan fixup for ", f_debug);
      db_name(&class_type->source_corresp);
      fputc('\n', f_debug);
    }  /* if */
#endif /* DEBUG */
    push_class_reactivation_scope(class_type);
    /* Each delayed-scan-fixup entry contains the cache for a token stream,
       either for a default arg expression or for an inline function
       definition. */
    for (; dsfp != NULL; dsfp = next_dsfp) {
      /* Let get_token know about the cache. */
      rescan_cached_tokens(&dsfp->token_cache);
      if (dsfp->is_arg_default_value) {
        /* It's a default arg expression that needs to be rescanned. */
        delayed_scan_of_default_arg_expr(dsfp->variant.param_type);
        /* In the normal case the current token should be end_of_source,
           which was inserted to mark the end of the cached token stream. */
        if (curr_token != tok_end_of_source) {
          pos_error(ec_exp_comma, &pos_curr_token);
          /* If necessary, keep flushing until end-of-source is found. */
          while (curr_token != tok_end_of_source) (void)get_token();
        }  /* if */
      } else {
        /* An inline function definition. */
        inline_function_definition(dsfp->variant.inline_func.routine,
                                   &dsfp->variant.inline_func.extra_info);
        /* In the normal case the current token should be end_of_source,
           which was inserted to mark the end of the cached token stream.
           If necessary, keep flushing until end-of-source is found. */
        while (curr_token != tok_end_of_source) (void)get_token();
      }  /* if */
      next_dsfp = dsfp->next;
      free_delayed_scan_fixup(dsfp);
      /* Advance past the end-of-source token, which was added in
         the prescan routine. */
      (void)get_token();
    }  /* for */
    /* The delayed scan fixup entries have been freed, so clear the
       pointer in the class symbol supplement. */
    cssp->delayed_scan_fixup_list = NULL;
    pop_class_reactivation_scope();
  }  /* if */
  db_exit();
}  /* delayed_scan_fixup_for_class */


#if DEBUG
static db_virtual_function_override(an_overriding_virtual_function_ptr ovfp)
/*
Dump a virtual function override entry, for debug purposes.
*/
{
  fputs("virtual function ", f_debug);
  db_name(&ovfp->primary_function->source_corresp);
  fputs(" overridden by ", f_debug);
  db_name(&ovfp->overriding_function->source_corresp);
  fputs(", type =\n  ", f_debug);
  db_type(ovfp->overriding_function->type);
  (void)fputc('\n', f_debug);
}  /* db_virtual_function_override */


static db_virtual_function_number_sequence(a_base_class_ptr  bcp)
/*
Dump a sequence of virtual function numbers, for debug purposes.
*/
{
  an_overriding_virtual_function_ptr ovfp = bcp->overriding_virtual_functions;
  for (; ovfp != NULL; ovfp = ovfp->next) {
    fprintf(f_debug, " %d", ovfp->primary_function->virtual_function_number);
  }  /* for */
}  /* db_virtual_function_number_sequence */
#endif /* DEBUG */


a_boolean simplify_curr_class_qualified_name(void)
/*

If the current token is the start of a qualified name in which the class
name component is the name of a class currently being defined, advance past
the class name and the "::" so that the current token is a non-qualified
name.  Return TRUE if such a modification is done and FALSE otherwise.
This routine is called in C++ only.

This functionality is provided to deal with declarations of class members
where a qualified name is used instead of a simple name, e.g., when a
constructor for class A is declared A::A() rather than A().  The ARM does
not specifically allow this syntax, but it is supported by cfront.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack[decl_scope_level];
  a_symbol_ptr             class_sym;
  a_token_cache            cache;
  a_boolean                is_member_id = FALSE;

  db_enter(3, "simplify_curr_class_qualified_name");
  if (curr_token == tok_identifier && next_token() == tok_colon_colon &&
      ssep->kind == (a_scope_kind)sck_class_struct_union) {
    class_sym = (a_symbol_ptr)ssep->assoc_type->source_corresp.assoc_info;
    if (locator_for_curr_id.symbol_header == class_sym->header) {
      /* We are inside a class declaration and the name is a qualified
         name starting with the name of the class being declared.  Advance
         to the member name, but cache the tokens so they are not lost. */
      clear_token_cache(&cache);
      /* Put the class name token in the cache. */
      cache_curr_token(&cache);
      /* Advance to the "::" and put it in the cache, too. */
      (void)get_token();
      cache_curr_token(&cache);
      /* Now get the next token. */
      (void)get_token();
      if (curr_token == tok_identifier || curr_token == tok_compl ||
          curr_token == tok_operator) {
        /* We specifically check for A::<name> and A::~ and A::operator
           to be sure we don't have a pointer-to-member. */
        is_member_id = TRUE;
      }  /* if */
      rescan_cached_tokens(&cache);
      if (is_member_id) {
        /* Advance past the class name and the "::". */
        (void)get_token();
        (void)get_token();
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
  return is_member_id;
}  /* simplify_curr_class_qualified_name */


static void report_virtual_function_ambiguities(a_type_ptr class_type)
/* 
Report errors in virtual function declarations that result from the failure
to redeclare a virtual function originally declared in a virtual base class.

The situation we are looking for (discussed in ARM 10.10c) is of this sort:
	class A { virtual int f(); };
        class B : virtual A { int f(); };
        class C : virtual A { int f(); };
        class D : A, B { };
or in graphical terms:
           A          virtual function A::f() is declared
          / \ 
         B   C        B::f() overrides A::f()
          \ /         C::f() overrides A::f()
           D          No D::f() was declared.
The problem is, what function should replace A::f() in the virtual function
table for base class A of class D, B::f() or C::f()?  This ambiguity must be
reported to the user.

The technique is as follows.  A list of overriding virtual functions is
maintained for each base class.  For instance, for base class A in class C
entries would have been created for both B::f() and C::f() when the base
classes were declared; both would have indicated that A::f() was being
overridden.  If D::f() were then defined, it would be added to the list
and both B::f() and C::f() removed, but otherwise the two overriding
functions would remain.  When this routine finds for two overriding
virtual function entries that override the same function, it reports the
ambiguity.
*/
{
  a_base_class_ptr                    bcp;
  an_overriding_virtual_function_ptr  ovfp;
  a_routine_ptr                       vfp;

  db_enter(4, "report_virtual_function_ambiguities");
  /* Make a pass over all the base classes (direct and indirect both) of
     the class indicated by class_type. */
  bcp = class_type->variant.class_struct_union.extra_info->base_classes;
  for (; bcp != NULL; bcp = bcp->next) {
    /* See if there any overriding virtual functions.  If there are,
       scan the list to look for duplicate primary functions. */
    ovfp = bcp->overriding_virtual_functions;
#if DEBUG
    if (debug_level >= 4) {
      if (ovfp != NULL) {
        fprintf(f_debug, "vfnum sequence (base class %s of class %s): ",
                bcp->type->source_corresp.name,
                class_type->source_corresp.name);
        db_virtual_function_number_sequence(bcp);
        (void)fputc('\n', f_debug);
      }  /* if */
    }  /* if */
#endif /* DEBUG */
    while (ovfp != NULL) {
      /* Compare the primary function for the current overriding virtual
         function entry to that of the its successor. */
      if (ovfp->next == NULL) break;
      vfp = ovfp->primary_function;
      if (ovfp->next->primary_function == vfp) {
        /* The virtual functions (member functions of the base class to which
           bcp refers) are the same -- i.e., both ovfp and ovfp->next
           represent an override of the same function. */
        a_symbol_ptr sym = (a_symbol_ptr)vfp->source_corresp.assoc_info;
        str_error(ec_ambiguous_virtual_function_override, name_of_symbol(sym));
        /* Remove the next entry and any successors that also have the same
           virtual function number. */
        for (;;) {
          ovfp->next = ovfp->next->next;
          if (ovfp->next == NULL ||
              ovfp->next->primary_function != vfp) {
            /* No more duplicates on the list. */
            break;
          }  /* if */
        }  /* for */
#if DEBUG
        if (debug_level >= 4) {
          fputs("  vfnum sequence after pruning: ", f_debug);
          db_virtual_function_number_sequence(bcp);
          (void)fputc('\n', f_debug);
        }  /* if */
#endif /* DEBUG */
      }  /* if */
      /* Advance to the next entry on the list. */
      ovfp = ovfp->next;
    }  /* while */
  }  /* for */
  db_exit();
} /* report_virtual_function_ambiguities */


static void check_abstract_class(a_type_ptr  class_type)
/*
If the current class is not already marked as "abstract", run through its
base classes to determine whether it is "abstract by inheritance" (i.e.,
if it inherits any pure virtual functions which are not redeclared in the
current class -- see ARM 10.3).  If it is, mark the class accordingly.
*/
{
  a_base_class_ptr                    bcp;
  a_routine_ptr                       rp;
  an_overriding_virtual_function_ptr  ovfp;

  db_enter(4, "check_abstract_class");
  if (class_type->variant.class_struct_union.abstract) {
    /* The class is already marked "abstract", presumably as a result of
       having one or more pure virtual member functions. */
  } else {
    /* The class was not already marked "abstract".  Go through its base
       classes to look for a pure virtual function that is inherited without
       an intervening declaration that overrides it. */
    bcp = class_type->variant.class_struct_union.extra_info->base_classes;
    for (; bcp != NULL; bcp = bcp->next) {
      if (bcp->type->variant.class_struct_union.abstract) {
        /* This base class *is* abstract.  Go through its routines and look
           for pure virtual functions.  At the same time, make a pass over
           the list of virtual function override entries for this class;
           each such entry will have a primary_function pointer referring to
           one of the routines of the base class, and the list's order is
           the same as that of the routines. */
        rp = bcp->type->variant.class_struct_union.extra_info->
                                                      assoc_scope->routines;
        ovfp = bcp->overriding_virtual_functions;
        for (; rp != NULL; rp = rp->next) {
          if (rp->pure_virtual) {
            /* Found a pure virtual function among the routines.  Advance
               through the overriding virtual function list, passing over
               entries in which the virtual function number of the primary
               function is lower than that of the current routine. */
            while (ovfp != NULL &&
                   ovfp->primary_function->virtual_function_number <
                                              rp->virtual_function_number) {
              ovfp = ovfp->next;
            } /* while */
            if (ovfp == NULL || ovfp->primary_function != rp) {
              /* No overriding virtual function entry was found that refers to
                 the pure virtual function routine entry.  The pure virtual
                 function is therefore inherited, and so the derived class
                 is also abstract. */
              class_type->variant.class_struct_union.abstract = TRUE;
              goto done;
            }  /* if */
            /* An overriding virtual function was found, so the pure
               virtual function is not inherited. */
          }  /* if */
          /* Get the next routine on the list. */
        }  /* for */
      }  /* if */
      /* Get the next base class. */
    }  /* for */
  }  /* if */
done:;
  db_exit();
}  /* check_abstract_class */


static void insert_in_virtual_function_override_list(
                                a_base_class_ptr                   base_class,
                                an_overriding_virtual_function_ptr new_ovfp)
/*
Add new overriding-virtual-function entry ovfp in the proper location in
the list pointed to from base_class.  The list should be maintained in the
order of the virtual function numbers of the functions being overridden,
which is the order of the routine entries in the routines list on the scope
for the class to which they belong.
*/
{
  an_overriding_virtual_function_ptr  ovfp;

  db_enter(4, "insert_in_virtual_function_override_list");
  /* Add it to the base class's list, observing the order dictated by the
     virtual function numbers in the primary functions. */
  ovfp = base_class->overriding_virtual_functions;
  if (ovfp == NULL ||
      new_ovfp->primary_function->virtual_function_number <
                        ovfp->primary_function->virtual_function_number) {
    /* Add the new entry to the start of the list. */
    base_class->overriding_virtual_functions = new_ovfp;
    new_ovfp->next = ovfp;
  } else {
    /* It must be inserted somewhere beyond the start of the list.  Look
       for the insert point by continuing through the list as long as the
       number in the current function exceeds the number in the next item
       on the list. */
    while (ovfp->next != NULL) {
      if (new_ovfp->primary_function->virtual_function_number <
                  ovfp->next->primary_function->virtual_function_number) {
        /* Found the insert location. */
        break;
      }  /* if */
      ovfp = ovfp->next;
    }  /* while */
    new_ovfp->next = ovfp->next;
    ovfp->next = new_ovfp;
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fputs("virtual function sequence for base class ", f_debug);
    db_name(&base_class->type->source_corresp);
    fputs(": ", f_debug);
    db_virtual_function_number_sequence(base_class);
    (void)fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* insert_in_virtual_function_override_list */


static void copy_virtual_function_override_list(
                             an_overriding_virtual_function_ptr list,
                             a_base_class_ptr                   base_class,
                             a_base_class_ptr                   new_base_class)
/*
Copy the list of overriding virtual functions from base class and add the
new list to new_base_class.  base_class and new_base_class are assumed to
refer to the same class type entry.
*/
{
  an_overriding_virtual_function_ptr  ovfp, new_ovfp;

  db_enter(4, "copy_virtual_function_override_list");
  /* Make a pass over the existing list. */
  for (ovfp = list; ovfp != NULL; ovfp = ovfp->next) {
    /* Allocate a new entry and copy fields from the original. */
    new_ovfp = alloc_overriding_virtual_function();
    new_ovfp->primary_function = ovfp->primary_function;
    new_ovfp->overriding_function = ovfp->overriding_function;
    /* The base_class field should indicate the base class in which the
       overriding function was declared.  When the the original has no
       base_class pointer, it means the the overriding function was a
       function declared in that class rather than inherited.  The field
       may not be left NULL in the copy. */
    new_ovfp->base_class = (ovfp->base_class == NULL) ?
                                  base_class : ovfp->base_class;
#if DEBUG
    if (debug_level >= 4) {
      fputs("copy for base class ", f_debug);
      db_name(&base_class->type->source_corresp);
      fputs(": ", f_debug);
      db_virtual_function_override(ovfp);
    }  /* if */
#endif /* DEBUG */
    /* Add it to the new list. */
    insert_in_virtual_function_override_list(new_base_class, new_ovfp);
  }  /* for */
  db_exit();
}  /* copy_virtual_function_override_list */


static void record_virtual_function_override(a_base_class_ptr  base_class,
                                             a_routine_ptr     primary_func,
                                             a_routine_ptr     overriding_func)
/*
Record the overriding of virtual function "primary_func", which was
declared in a base class ("base_class") of the current class, by function
"overriding_func", which was declared in the current class.  The override
entry appears on a linked list pointed to from base_class.
*/
{
  an_overriding_virtual_function_ptr  ovfp;

  db_enter(4, "record_virtual_function_override");
  /* If there is already an override entry, created when the primary routine
     was overridden by a function in another base class (one on the path
     between the current class and the class of which the primary function
     is a member), we can simply reuse that entry. */
  ovfp = base_class->overriding_virtual_functions;
  for (; ovfp != NULL; ovfp = ovfp->next) {
    if (ovfp->primary_function == primary_func) {
      /* There is an entry that can be reused.  Modify it as required. */
#if DEBUG
      if (debug_level >= 4) {
        fputs("existing entry: ", f_debug);
        db_virtual_function_override(ovfp);
      }  /* if */
#endif /* DEBUG */
      ovfp->overriding_function = overriding_func;
      ovfp->base_class = NULL;
#if DEBUG
      if (debug_level >= 4) {
        fputs("after modification: ", f_debug);
        db_virtual_function_override(ovfp);
      }  /* if */
#endif /* DEBUG */
      /* There can be more than one override entry from previous base classes
         in a case like this:
                        A
                      /   \
                     B     C
                      \   /
                        D
         B and C are virtually derived from A, and all three declare a given
         virtual function.  The declarations in B and C both override the
         declaration in A; if there is no redeclaration in D, the overriding
         declarations in B and C would be ambiguous (see ARM 10.10c), but if
         there is a redeclaration in D, it overrides the declarations in B
         and C and thereby eliminates the ambiguity.  Look for other
         redeclarations (previously entered in the overriding list to mark
         potential ambiguities) and remove them. */
      while (ovfp->next != NULL &&
             ovfp->next->primary_function == primary_func) {
#if DEBUG
        if (debug_level >= 4) {
          fputs("removing: ", f_debug);
          db_virtual_function_override(ovfp->next);
        }  /* if */
#endif /* DEBUG */
        ovfp->next = ovfp->next->next;
      }  /* while */
      break;
    }
  }  /* if */
  if (ovfp == NULL) {
    /* No previous virtual function override entry. */
    ovfp = alloc_overriding_virtual_function();
    ovfp->primary_function = primary_func;
    ovfp->overriding_function = overriding_func;
#if DEBUG
    if (debug_level >= 4) {
      fputs("newly created: ", f_debug);
      db_virtual_function_override(ovfp);
    }  /* if */
#endif /* DEBUG */
    insert_in_virtual_function_override_list(base_class, ovfp);
  }  /* if */
  db_exit();
}  /* record_virtual_function_override */


static a_boolean check_for_virtual_function(a_boolean        virtual_specified,
                                            a_symbol_ptr     rout_sym,
                                            a_type_ptr       class_type,
                                            a_source_position *source_pos)
/*
A nonstatic member function, represented by rout_sym, has been declared
and, depending on the value of virtual_specified, may have been explicitly
declared to be a virtual function.  Even if it has not, it will need to be
marked as virtual if it overrides a virtual function (i.e., if a function
with the same name and type signature was declared virtual in a base class
of the current class).  In addition, information about base class virtual
functions that are overridden by the current declaration is recorded to
allow for appropriate processing later (e.g., the construction of virtual
function tables).  If the current routine is a virtual function either
from explicit specification or from "inheriting" its virtualness, mark the
routine entry and return TRUE; otherwise return FALSE.
*/
{
  a_boolean                    is_virtual, overloaded;
  a_base_class_ptr             bcp;
  a_symbol_ptr                 symbol_list, sym, sym_next;
  a_routine_ptr                rout, rp;
  a_scope_number               base_class_scope_number;
  a_class_type_supplement_ptr  ctsp;

  db_enter(4, "check_for_virtual_function");
  rout = rout_sym->variant.routine;
  is_virtual = virtual_specified;
  /* We scan symbols on the inactive list, since we are only interested in
     base classes symbols. */
  symbol_list = rout_sym->header->inactive_symbols;
  /* Outer loop:  go through the base classes of the current class. */
  bcp = class_type->variant.class_struct_union.extra_info->base_classes;
  for (; bcp != NULL; bcp = bcp->next) {
    /* Pull out the unique scope identifier for this base class. */
    base_class_scope_number =
        bcp->type->variant.class_struct_union.extra_info->assoc_scope->number;
    /* Inner loop:  go thorough all the symbols for this name, looking for
       one which represents a member function (overloaded or simple) from
       the base class under examination. */
    for (sym = symbol_list; sym != NULL; sym = sym_next) {
      sym_next = sym->next;
      if (sym->decl_scope == base_class_scope_number) {
        /* Symbol represents a member of bcp's class. */
        if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
          overloaded = TRUE;
          sym = sym->variant.overloaded_function.symbols;
        } else if (sym->kind == (a_symbol_kind)sk_member_function) {
          overloaded = FALSE;
        } else {
          /* It's a symbol for neither a simple function nor an overloaded
             function.  If it's in the same name space with member
             functions, we've looked far enough for this base class.  If
             its a typedef name, say, we can keep scanning. */
          if (sym->kind != (a_symbol_kind)sk_field &&
              sym->kind != (a_symbol_kind)sk_static_data_member) continue;
          goto next_base_class;
        }  /* if */
        /* Innermost loop is run only once for simple functions but more
           for overloaded functions.  This is a do-while loop instead of a
           for loop because we can be sure of the initial conditions on the
           first iteration. */
        do {
          rp = sym->variant.routine;
          /* We are only interested in virtual functions with the same
             type signature.  Look first at the arg types only. */
          if (rp->is_virtual &&
              arg_types_are_compatible(rout->type, rp->type)) {
            /* Now compare the return types. */
            if (types_are_compatible(rout->type->variant.routine.return_type,
                                     rp->type->variant.routine.return_type)) {
              /* Match */
              is_virtual = TRUE;
              /* Record the virtual function override in the base class
                 entry.  It can be used later, e.g., for building a virtual
                 function table. */
              record_virtual_function_override(bcp, rp, rout);
            } else {
              /* Error -- cannot differ in return type only (ARM 10.2). */
              pos_error(ec_bad_return_type_on_virtual_function_override,
                        source_pos);
            }  /* if */
            goto next_base_class;                                       
          }  /* if */
          if (!overloaded) break;
          sym = sym->next;
        } while (sym != NULL);
      }  /* if */
    }  /* for */
next_base_class:;
  }  /* for */
  if (is_virtual) {
    /* Mark the routine entry. */
    rout_sym->variant.routine->is_virtual = TRUE;
    /* The number of virtual functions declared so far in this routine has
       is recorded in the class type supplement.  Increment that number and
       enter it in the routine entry.  It is used by the front end in
       managing virtual function override entries and can be used by the
       back end for indexing into a virtual function table. */
    ctsp = class_type->variant.class_struct_union.extra_info;
    if (ctsp->virtual_function_count >= MAX_VIRTUAL_FUNCTIONS_PER_CLASS) {
      pos_error(ec_too_many_virtual_functions, source_pos);
      /* Reset to zero, to avoid more such messages. */
      ctsp->virtual_function_count = 0;
    }  /* if */
    rout_sym->variant.routine->virtual_function_number = 
                                      ++(ctsp->virtual_function_count);
  }  /* if */
  db_exit();
  return is_virtual;
}  /* check_for_virtual_function */


static void scan_path(a_derivation_step_ptr  step,
                      a_derivation_step_ptr  *root,
                      a_derivation_step_ptr  *tail)
/*
Scan a derivation path from start to end, saving in *tail a pointer to the
last step in the chain and saving in *root a pointer either to the start of
the path or, if there is one, to the derivation step most remote from the
start (closest to the tail) that is associated with a virtual base class.
If the derivation path consists of a single step or if its end is a virtual
base class, *root and *tail will point to the same step.
*/
{
  a_derivation_step_ptr dsp = step;

  /* *root is initialized to the start of the path. */
  *root = step;
  for (;;) {
    if (dsp->base_class->is_virtual) *root = dsp;
    if (dsp->next == NULL) {
      *tail = dsp;
      break;
    }  /* if */
    dsp = dsp->next;
  }  /* for */
}  /* scan_path */


/* Previously allocated derivation-step entries available for reuse. */
static a_derivation_step_ptr avail_derivation_steps;

void free_derivation_step(a_derivation_step_ptr  step)
/*
Return a derivation step entry (or a list of them) to the free list for
reuse at another time.
*/
{
  /* If there is a next step entry, free it first by making a recursive
     call. */
  if (step->next != NULL) free_derivation_step(step->next);
  step->base_class = NULL;
  step->next = avail_derivation_steps;
  avail_derivation_steps = step;
}  /* free_derivation_step */


a_derivation_step_ptr make_derivation_step(a_base_class       *base_class,
                                           a_derivation_step  *existing_step)
/*
Allocate a derivation step entry, set its fields as specified, and return
a pointer to it.
*/
{
  a_derivation_step_ptr new_step;

  if (avail_derivation_steps == NULL) {
    new_step = alloc_derivation_step();
  } else {
    new_step = avail_derivation_steps;
    avail_derivation_steps = new_step->next;
  }  /* if */
  new_step->base_class = base_class;
  new_step->next = existing_step;

  return new_step;
}  /* make_derivation_step */


#if DEBUG
static void db_path(a_derivation_step_ptr dsp,
                    a_boolean             show_offset)
/*
Dump a linked list of derivation steps, for debug purposes.
*/
{
  if (dsp == NULL) {
    fputs("<null path>", f_debug);
  } else {
    for (; dsp != NULL; dsp = dsp->next) {
      fprintf(f_debug, "==>%s", dsp->base_class->is_virtual ? "[v]" : "");
      db_name(&dsp->base_class->type->source_corresp);
      if (show_offset) {
        fprintf(f_debug, "@%ld", dsp->base_class->offset);
        if (dsp->base_class->is_virtual) {
          fprintf(f_debug, "(ptr @%ld)", dsp->base_class->pointer_offset);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* db_path */


void db_base_class(a_base_class_ptr  bcp,
                   a_boolean         show_offset)
/*
Dump a base class entry, for debug purposes.
*/
{
  (void)fputc('"', f_debug);
  db_name(&bcp->type->source_corresp);
  fputs("\": ", f_debug);
  if (show_offset) {
    fprintf(f_debug, "(%ld bytes): offset = %ld, ",
                     bcp->type->size, bcp->offset);
  }  /* if */
  fprintf(f_debug, "%sdirect, ",
                     bcp->direct ? "" : "in");
  db_access_control(bcp->access);
  if (bcp->is_virtual) {
    fputs(", virtual", f_debug);
    if (show_offset) {
      fprintf(f_debug, " (ptr offset = %ld)", bcp->pointer_offset);
    }  /* if */
  } else if (bcp->any_virtual_steps_in_derivation) {
    fputs(", vsteps", f_debug);
  }  /* if */
  if (bcp->ambiguous) fputs(", ambig", f_debug);
  fputs(", deriv = ", f_debug);
  db_path(bcp->derivation, show_offset);
  (void)fputc('\n', f_debug);
}  /* db_base_class */


static void db_base_class_list(a_type_ptr tp)
/*
Dump a linked list of base class entries, for debug purposes.
*/
{
  a_base_class_ptr       bcp;

  if (is_class_struct_union_type(tp)) {
    fputs("base classes for ", f_debug);
    db_name(&tp->source_corresp);
    fputs(":", f_debug);
    bcp = tp->variant.class_struct_union.extra_info->base_classes;
    if (bcp == NULL) {
      fputs(": <null list>\n", f_debug);
    } else {
      fputs(":\n", f_debug);
      for (; bcp != NULL; bcp = bcp->next) {
        fputs("  ", f_debug);
        db_base_class(bcp, /*show_offset=*/TRUE);
      }  /* for */
    }  /* if */
  }  /* if */
}  /* db_base_class_list */
#endif /* DEBUG */


#if CHECKING
static void verify_path_consistency(a_type_ptr        class_type,
                                    a_base_class_ptr  base_class)
/*
Check for consistency of a derivation path.  Specifically, base_class is
a base class of class_type.  It contains a pointer to a derivation path,
represented as a linked list of entries that point to base classes.  We
want to be sure that the base classes pointed to from the derivation
path entries are also on the base classes list of class_type.
*/
{
  a_derivation_step_ptr        dsp;
  a_class_type_supplement_ptr  ctsp;
  a_base_class_ptr             bcp;

#if DEBUG
  if (debug_level >= 3) {
    fputs("path consistency check: base class \"", f_debug);
    db_name(&base_class->type->source_corresp);
    fputs("\", path ", f_debug);
    db_path(base_class->derivation, /*show_offset=*/FALSE);
    (void)fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  ctsp = class_type->variant.class_struct_union.extra_info;
  /* Go through each step of the path. */
  for (dsp = base_class->derivation; dsp != NULL; dsp = dsp->next) {
    /* Be sure the base class entry pointed to from the step entry is
       actually on the class type's list of base classes. */
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      if (bcp == dsp->base_class) break;
      if (bcp->next == NULL) {
        internal_error("verify_path_consistency: base class inconsistency");
      }  /* if */
    }  /* if */
    if (!dsp->base_class->is_virtual) {
      if (dsp == base_class->derivation) {
        if (!dsp->base_class->direct) {
          internal_error(
                     "verify_path_consistency: expected direct base class");
        }  /* if */
      } else {
        if (dsp->base_class->direct) {
          internal_error(
                     "verify_path_consistency: expected indirect base class");
        }  /* if */
      }  /* if */
    }  /* if */
    if (dsp->next == NULL) {
      if (dsp->base_class != base_class) {
        internal_error("verify_path_consistency: bad end-of-path base class");
      }  /* if */
    }  /* if */
  }  /* for */
}  /* verify_path_consistency */


static void verify_virt_func_override_list(a_type_ptr        class_type,
                                           a_base_class_ptr  base_class,
                                           a_boolean         null_allowed)
/*
Verify that the base classes pointed to from overriding virtual function
entries associated with base_class are on the base_classes list of class_type.
*/
{
  a_class_type_supplement_ptr         ctsp;
  a_base_class_ptr                    bcp;
  an_overriding_virtual_function_ptr  ovfp;

  ctsp = class_type->variant.class_struct_union.extra_info;
  ovfp = base_class->overriding_virtual_functions;
  for (; ovfp != NULL; ovfp = ovfp->next) {
    if (ovfp->base_class == NULL) {
      if (!null_allowed) {
        internal_error("verify_virt_func_override_list: NULL base class");
      }  /* if */
    } else if (ovfp->base_class != base_class) {
      for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
        if (bcp == ovfp->base_class) break;
      }  /* for */
      if (bcp == NULL) {
        internal_error("verify_virt_func_override_list: base class not found");
      }  /* if */
    }  /* if */
  }  /* for */
}  /* verify_virt_func_override_list */
#endif /* CHECKING */


a_boolean congruent_paths(a_derivation_step_ptr  dsp1,
                          a_derivation_step_ptr  dsp2)
/*
Return TRUE if the class sequence signatures of the paths headed by dsp1 and
dsp2 are identical.
*/
{
  a_boolean  congruent;

  db_enter(4, "congruent_paths");
#if DEBUG
  if (debug_level >= 4) {
    fputs("comparing ", f_debug);
    db_path(dsp1, /*show_offset=*/FALSE);
    fputs(" and ", f_debug);
    db_path(dsp2, /*show_offset=*/FALSE);
  }  /* if */
#endif /* DEBUG */
  /* Loop through both derivation paths in tandem, comparing the corresponding
     step entries along the way.  An incongruence is detected when two paths
     are of different lengths, when two corresponding steps refer to different
     classes, or when one of the steps represents a virtual derivation and
     the other does not. */
  congruent = TRUE;
  for (; dsp1 != NULL || dsp2 != NULL; dsp1 = dsp1->next, dsp2 = dsp2->next) {
    if (dsp1 == NULL || dsp2 == NULL ||
        dsp1->base_class->type != dsp2->base_class->type ||
        dsp1->base_class->is_virtual != dsp2->base_class->is_virtual) {
      congruent = FALSE;
      break;
    }  /* if */
  }  /* for */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, " : %scongruent\n", congruent ? "" : "not ");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return congruent;
}  /* congruent_paths */


a_boolean equivalent_paths(a_derivation_step_ptr  path1,
                           a_derivation_step_ptr  path2)
/*
Two paths are equivalent if they lead to the same object (i.e., the
same member of the same instance of a class).  They need not be
step-for-step identical if both classes pass through the same virtual
base class.  Return TRUE if path1 and path2 are equivalent.  The
algorithm assumes equivalence and then searches for indications to the
contrary.
*/
{
  a_derivation_step_ptr root1, root2, tail1, tail2;
  a_boolean             equiv;

  db_enter(4, "equivalent_paths");
#if DEBUG
  if (debug_level >= 4) {
    fputs("comparing ", f_debug);
    db_path(path1, /*show_offset=*/FALSE);
    fputs(" and ", f_debug);
    db_path(path2, /*show_offset=*/FALSE);
    (void)fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  /* Traverse each path to find the terminal entry of each as well as the
     most remote virtual base class entry. */
  scan_path(path1, &root1, &tail1);
  scan_path(path2, &root2, &tail2);
  equiv = TRUE;

  /* If two paths have a virtual base class in common, they are equivalent
     if everything beyond the virtual base class is the same, even if what
     preceded it is different.  For instance,
                     A
                     |
                     B
                   /   \
                  X     Y
                   \   /
                     S
     Here there are two paths from S to A, but they are equivalent since
     once you get to B (however you get there) there is only one A object
     reachable from B.  Conversely, if one path has a virtual base class on
     it and the other does not, they cannot be equivalent, e.g.,
                     A
                     |
                     B       A
                   /   \     |
                  X     Y    B
                   \   /     |
                     S       T
                       \   /
                         Z
       A member of A inherited by Z along the path that goes through T is a
       different object from the one inherited through S. */
  if (tail1->base_class->type != tail2->base_class->type) {
    /* Different tail base class types mean different paths -- though the same
       type doesn't guarantee that the paths are the same. */
    equiv = FALSE;
  } else {
    equiv = congruent_paths(root1, root2);
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "paths are %sequivalent\n", equiv ? "" : "not ");
  }  /* if */
#endif /* DEBUG */
  db_exit()
  return equiv;
}  /* equivalent_paths */


#define is_more_accessible(access1, access2)    \
    ((int)(access1) < (int)(access2))


a_boolean check_for_dominance(a_symbol_ptr          sym1,
                              a_symbol_ptr          sym2,
                              a_derivation_step_ptr path_to_sym2)
/*
This routine returns TRUE if sym2 is on a path dominated by sym1.

Dominance in discussed (rather imprecisely) in ARM 10.1.1.  Briefly, if the
declaration of a name in a virtual base class is hidden/overridden by a
redeclaration along one of the paths from the virtual base class, an ambiguity
between the initial declaration and the redeclaration is resolved in favor of
the latter.  Consider, for example,
    class A {public: int i; };
    class B : virtual public A {public: int i; };
    class C : virtual public A {};
    class D : public B, public C {};
which graphically looks like this:
          A{i}
         /   \
        B{i}  C
         \   /
           D
Within the scope of D, where one derivation path for i leads to B::i and the
other leads to A::i, there is in fact no ambiguity, since the declaration of
i in B dominates all other paths from A.  Thus an unqualified reference to i
within the scope of D unambiguously refers to B::i (though of course a
qualified reference either to A::i or to C::i will pick up A::i).
*/
{
  a_boolean              dominated;
  a_derivation_step_ptr  dsp;
  a_base_class_ptr       bcp;

  dominated = FALSE;
  /* If sym1, the candidate dominating symbol, is a projection symbol, find
     its fundamental symbol. */
  reduce_projection_symbol_to_fundamental_symbol(sym1);
  /* Loop through the base classes of the class of which sym1 is a member. */
  bcp = sym1->class_of_which_a_member->
                         variant.class_struct_union.extra_info->base_classes;
  for (; bcp != NULL; bcp = bcp->next) {
    /* We are interested only in virtual base classes. */
    if (bcp->is_virtual) {
      /* Examine the virtual base classes on the path to sym2.  In the example
         above, sym2 is A::i, and the path to sym2 (from D) is ==>C==>A.  The
         search will stop when virtual base class A is found, since it is also
         a virtual base class of B, the class of which sym1 is a member. */
      dsp = path_to_sym2;
      for (; dsp != NULL; dsp = dsp->next) {
        if (dsp->base_class->is_virtual &&
            dsp->base_class->type == bcp->type) {
          /* A match is found.  The path to sym2 is dominated by sym1. */
          dominated = TRUE;
          goto done;
        }  /* if */
      }  /* for */
      /* No match yet.  If sym2 happens to be a projection symbol, also
         check the path between sym2 (the projection) and the fundamental
         symbol of which it is a projection.  E.g., if sym2 were a projection
         symbol for C::i (the projection of A::i into C), we would want to
         examine the path between C and A, the class of which i is actually
         a member. */
      if (sym2->kind == (a_symbol_kind)sk_projection) {
        dsp = sym2->variant.projection.extra_info->
                                          fundamental_base_class->derivation;
        for (; dsp != NULL; dsp = dsp->next) {
          if (dsp->base_class->is_virtual &&
              dsp->base_class->type == bcp->type) {
            /* A match is found.  The path to the fundamental symbol of
               which sym2 is a projection is dominated by sym1. */
            dominated = TRUE;
            goto done;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* for */
done:
  return dominated;
}  /* check_for_dominance */       


static a_derivation_step_ptr copy_and_extend_path(
                                             a_derivation_step_ptr path,
                                             a_derivation_step_ptr step,
                                             a_base_class_ptr      base_class)
/*
Given a derivation path "path", make a copy of it and extend by another step
represented by "step".  Return a pointer to the new path.  When path is
NULL, a pointer to step is returned.
*/
{
  a_derivation_step_ptr  dsp, new_dsp;
  a_derivation_step_ptr  new_path = NULL, end_of_new_path = NULL;

  if (path == NULL) {
    new_path = step;
  } else {
    for (dsp = path; dsp != NULL; dsp = dsp->next) {
      new_dsp = make_derivation_step(dsp->base_class,
                                     (a_derivation_step_ptr)NULL);
      if (new_dsp->base_class->is_virtual) {
        base_class->any_virtual_steps_in_derivation = TRUE;
      }  /* if */
      if (new_path == NULL) {
        new_path = new_dsp;
      } else {
        end_of_new_path->next = new_dsp;
      }  /* if */
      end_of_new_path = new_dsp;
    }  /* if */
    end_of_new_path->next = step;
  }  /* if */
  return new_path;
}  /* copy_and_extend_path */


#define base_classes_of(tp) \
  ((tp)->variant.class_struct_union.extra_info->base_classes)


static void fixup_virtual_base_class(a_base_class_ptr               base_class,
                                     an_overriding_virtual_function *ovf_list,
                                     a_derivation_step_ptr          path,
                                     an_access_specifier            new_access)
/*

When a class is derived from a virtual base class by more than one
derivation path, only one instance of the virtual base class will actually
show up in the base class graph.  For instance, if V is a virtually base
class of A and B, the base class graphs for A and B will be identical:
                X   Y     X   Y
                 \ /       \ /
                  V         V
                  |         |
                  A         B
but when they are both declared as base classes of C, the resulting graph
looks like this:
                     X   Y
                      \ /
                       V
                      / \
                     A   B
                      \ /
                       C
This routine is called when a virtual base class is already in the base
classes list being built, and a second virtual base class is encountered
that refers to the same class.  For instance, the base classes list for
class C already contains A, V, X, Y, and B (in that order) when a second V
is encountered.  It is not sufficient to simply ignore the second V (and
its own base classes X and Y) since it has declaration information that
may need to merged with that of the other instance of V or that supersedes
it.

Specifically, we need to preserve the declaration order (depth first, left
to right) in our linear list of direct and indirect base classes (useful
for initialization: ARM 12.6.2), but at the same time we want to preserve
the accessibility and path information along the derivation path with the
least access restriction (ARM 11.7).  And we must keep track of overriding
virtual functions as though all paths were available (ARM 10.10.c).

What this routine does is modify the virtual base class entry already in
place (and all of its base classes) to incorporate additional information
from the second declaration.

Parameter base_class is the base class entry to be modified.  ovf_list is
a pointer to the linked list of entries representing virtual function
overrides along the *other* path to the base class (e.g., along ==>B==>V);
it is always NULL when the other instance of the base class is a direct
base class.  path is the derivation path leading up to the other instance
of the base class (e.g., ==>B==>V); it is always NULL when the other
instance is a direct base class.  access is the accessibility of the
other instance of the base class.
*/
{
  a_derivation_step_ptr  dsp, prev_dsp;
  a_base_class_ptr       bcp, other_bcp;
  a_boolean              recompute_path_and_access = FALSE;
  an_access_specifier    base_class_access;

  db_enter(3, "fixup_virtual_base_class");
#if DEBUG
  if (debug_level >= 3) {
    fputs("base class ", f_debug);
    db_base_class(base_class, /*show_offset=*/FALSE);
    fputs("new access = ", f_debug);
    db_access_control(new_access);
    fputs(", new path = ", f_debug);
    db_path(path, /*show_offset=*/FALSE);
    (void)fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  base_class_access = normal_access_to_end_of_path(base_class->derivation);
  if (path == NULL) {
    /* When path is NULL we have a direct base class. */
    base_class->direct = TRUE;
    /* Give preference to the path of the direct base class unless the path
       of the other instance gives more access. */
    if (is_more_accessible(base_class_access, new_access)) {
      /* The indirect derivation gives greater access, so it's the one whose
         path we use (following ARM 11.7), even though it is not the
         derivation one would expect for a base class marked "direct". */
      str_warning(ec_direct_derivation_less_accessible,
                  base_class->type->source_corresp.name);
    } else {
      /* The direct derivation gives at least as much access as the indirect
         derivation. */
      recompute_path_and_access = TRUE;
    }  /* if */
  } else {
    /* Give preference to the path of the previously declared base class
       unless the accessibility of the new declaration is greated. */
    if (is_more_accessible(compute_access(normal_access_to_end_of_path(path),
                                          new_access),
                           base_class_access)) {
      recompute_path_and_access = TRUE;
      if (base_class->direct) {
        /* Since the other declaration, whose path will be superseded by the
           present one, was for a direct base class, we again have the
           situation where a base class is marked "direct" but has a longer
           path. */
        str_warning(ec_direct_derivation_less_accessible,
                    base_class->type->source_corresp.name);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Make a copy of each item on the the override list base class and merge
     it into the list of base_class. */
  copy_virtual_function_override_list(ovf_list, base_class, base_class);
  if (recompute_path_and_access) {
    base_class->access = new_access;
    /* Reset the path of base_class.  It should just be the path passed in
       plus one more step to the base class itself. */
    free_derivation_step(base_class->derivation);
    dsp = make_derivation_step(base_class, (a_derivation_step_ptr)NULL);
    base_class->derivation = copy_and_extend_path(path, dsp, base_class);
#if DEBUG
    if (debug_level >= 3) {
      fputs("  modifed ", f_debug);
      db_base_class(base_class, /*show_offset=*/FALSE);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  /* Go through all the base classes for the class to which base_class
     corresponds.  They should map precisely to the list of base classes
     linked to base_class via the next field.  Each of the base class entries
     in the modify list should have its virtual function override list updated
     and its path modified (if required).  The access field should not be
     changed. */
  other_bcp = base_classes_of(base_class->type);
  for (bcp = base_class->next; bcp != NULL; bcp = bcp->next) {
    /* When we reach the end of the base class tree, we should also be at the
       end of the flattened list. */
    if (other_bcp == NULL) break;
#if CHECKING
    if (bcp->type != other_bcp->type) {
      internal_error("fixup_virtual_base_class: base classes out of sync");
    }  /* if */
#endif /* CHECKING */
    copy_virtual_function_override_list(
                     other_bcp->overriding_virtual_functions, bcp, bcp);
    other_bcp = other_bcp->next;
    if (recompute_path_and_access) {
#if DEBUG
      if (debug_level >= 3) {
        fputs("also needing fixup ", f_debug);
        db_base_class(bcp, /*show_offset=*/FALSE);
      }  /* if */
#endif /* DEBUG */
      /* Find the segment of bcp's derivation path that is to be replaced. */
      dsp = bcp->derivation;
      if (dsp->base_class == base_class) {
        /* Previous base class declaration must have been direct.  No portion
           of its path needs to be removed. */
      } else {
        prev_dsp = dsp;
        for (dsp = dsp->next; dsp != NULL; dsp = dsp->next) {
          if (dsp->base_class == base_class) {
            /* Throw away the segment of the derivation path preceding the step
               that points to base_class. */
            prev_dsp->next = NULL;
            free_derivation_step(bcp->derivation);
            break;
          }  /* if */
          prev_dsp = dsp;
        }  /* for */
#if CHECKING
        if (dsp == NULL) {
          internal_error("fixup_virtual_base_class: no base class match");
        }  /* if */
#endif /* CHECKING */
      }  /* if */
      /* Replace what was just thrown away (if anything) with the steps
         represented by "path". */
      bcp->derivation = copy_and_extend_path(path, dsp, bcp);
#if DEBUG
      if (debug_level >= 3) {
        fputs("  modified ", f_debug);
        db_base_class(bcp, /*show_offset=*/FALSE);
      }  /* if */
#endif /* DEBUG */
    }  /* for */
  }  /* if */
#if CHECKING
  if (other_bcp != NULL) {
    /* Modify list terminated before the copy list did. */
    internal_error(
               "fixup_virtual_base_class: not all base classes accounted for");
  }  /* if */
#endif /* CHECKING */
  db_exit();
}  /* fixup_virtual_base_class */


static a_base_class_ptr add_indirect_base_class(
                                    a_base_class_ptr      base_class_to_copy,
                                    a_base_class_ptr      add_list,
                                    a_base_class_ptr      *end_of_add_list,
                                    a_derivation_step_ptr path)
/*
Create a new indirect base class based on base_class_to_copy and, typically,
add it to the end of add_list.  "path" is the derivation path from the most
derived class to the class that is directly derived from the new base class,
and it is copied and extended to produce the new base class's derivation.
In addition, check for ambiguity and duplicate paths.
*/
{
  a_base_class_ptr       new_bcp, bcp;
  a_derivation_step_ptr  step;
  a_boolean              is_virtual_duplicate = FALSE;

  db_enter(3, "add_indirect_base_class");
  if (base_class_to_copy->is_virtual) {
    for (bcp = add_list; bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual && bcp->type == base_class_to_copy->type) {
        is_virtual_duplicate = TRUE;
        fixup_virtual_base_class(
                         bcp, base_class_to_copy->overriding_virtual_functions,
                         path, base_class_to_copy->access);
      }  /* if */
    }  /* for */
  }  /* if */
  if (!is_virtual_duplicate) {
#if DEBUG
    if (debug_level >= 3) {
      fputs("  creating indirect base class \"", f_debug);
      db_name(&base_class_to_copy->type->source_corresp);
      fputs("\"\n", f_debug);
    }  /* if */
#endif /* DEBUG */
    /* Create a new base class entry. */
    new_bcp = alloc_base_class();
    new_bcp->type = base_class_to_copy->type;
    new_bcp->direct = FALSE;
    /* Retain the access of the original derivation from this base class. */
    new_bcp->access = base_class_to_copy->access;
    new_bcp->is_virtual = base_class_to_copy->is_virtual;
    step = make_derivation_step(new_bcp, (a_derivation_step_ptr)NULL);
    new_bcp->derivation = copy_and_extend_path(path, step, new_bcp);
    /* Add this to the end of add_list, unless its derivation is equivalent to
       that of some other base class entry on the same list. */
    for (bcp = add_list; bcp != NULL; bcp = bcp->next) {
      if (bcp->type == new_bcp->type) {
        /* Ambiguous base class. */
        bcp->ambiguous = TRUE;
        new_bcp->ambiguous = TRUE;
      }  /* if */
    }  /* for */
    *end_of_add_list = (*end_of_add_list)->next = new_bcp;
    /* Add the base classes of the current nonvirtual indirect base class
       to the base classes list of the most-derived-class. */
    bcp = new_bcp->type->
                    variant.class_struct_union.extra_info->base_classes;
    for (; bcp != NULL; bcp = bcp->next) {
      if (bcp->direct) {
        (void)add_indirect_base_class(bcp, add_list, end_of_add_list,
                                      new_bcp->derivation);
      }  /* if */
    }  /* for */
  }  /* if */
  db_exit();
  return new_bcp;
}  /* add_indirect_base_class */


static void scan_base_specifier_list(a_type_ptr         type_ptr)
/*
Scan a list of base class specifiers, which may appear only on a class
or struct definition.  The syntax is

10
        base-spec:
                : base-list

        base-list:
                base-specifier
                base-list , base-specifier

        base-specifier:
                class_name
                virtual access-specifier    complete-class-name
                                        opt
                access-specifier virtual    complete-class-name
                                        opt
*/
{
  a_class_type_supplement_ptr   ctsp;
  a_base_class_ptr              bcp, new_bcp, end_of_base_classes_list = NULL;
  an_access_specifier           access;
  a_boolean                     is_virtual;
  a_boolean                     access_already_specified;
  char                          *default_access_str;
  a_symbol_ptr                  sym;
  a_type_ptr                    base_class_type;
  a_boolean                     ambiguous;
  a_class_symbol_supplement_ptr cssp, bcp_cssp;

  db_enter(3, "scan_base_specifier_list");
  if (type_ptr->kind == (a_type_kind)tk_union) {
    /* Unions cannot have base classes.  Issue an error, but go ahead and scan
       the base class specifiers (without updating the type supplement). */
    error(ec_base_class_not_allowed_for_union);
    ctsp = NULL;
  } else {
    /* Get the class type supplement entry for this class or struct. */
    ctsp = type_ptr->variant.class_struct_union.extra_info;
  }  /* if */
  /* Advance past the colon. */
  (void)get_token();
  do {
    add_stop_token(tok_comma);
    /* Set the defaults. */
    if (type_ptr->kind == (a_type_kind)tk_class) {
      access = (an_access_specifier)as_private;
      default_access_str = "private";
    } else {
      access = (an_access_specifier)as_public;
      default_access_str = "public";
    }  /* if */
    is_virtual = FALSE;
    access_already_specified = FALSE;
    /* Scan a single base specification, first looping through the specifying
       keywords virtual, public, private, and protected. */
    for (;;) {
      if (curr_token == tok_virtual) {
        if (is_virtual) error(ec_dupl_decl_specifier);
        is_virtual = TRUE;
      } else if (curr_token == tok_public || curr_token == tok_protected ||
                 curr_token == tok_private) {
        if (access_already_specified) {
          error(ec_access_already_specified);
        } else {
          switch (curr_token) {
            case tok_public:
              access = (an_access_specifier)as_public;
              break;
            case tok_protected:
              access = (an_access_specifier)as_protected;
              break;
            case tok_private:
              access = (an_access_specifier)as_private;
              break;
          }  /* switch */
          access_already_specified = TRUE;
        }  /* if */
      } else {
        /* Leave the loop and scan the class name. */
        break;
      }  /* if */
      (void)get_token();
    }  /* for */
    /* Test for identifier or "::" next. */
    if (!is_qualified_name_start()) {
      syntax_error(ec_exp_identifier);
    } else {
      /* Scan the base class name. */
      sym = get_normal_id_or_qualified_name(IDL_NO_OPTIONS);
      if (sym == NULL || !is_class_symbol(sym)) {
        error(ec_not_a_class_or_struct_name);
        goto skip_base_class;
      }  /* if */
      if (type_ptr->kind == (a_type_kind)tk_union) {
        /* We just ignore the base classes declared for a union.  The error
           has already been issued. */
        goto skip_base_class;
      }  /* if */
      /* Record the symbol as referenced. */
      mark_referenced(sym, &pos_curr_token);
      /* Do ambiguity and access control checking for the symbol. */
      check_ambiguity_and_verify_access(&locator_for_curr_id);
      /* Get the type entry for the base class name.  The symbol's type entry
         could be a "tag typeref".  If so, get the type entry at file scope
         that it points to. */
      base_class_type = type_symbol_type(sym);
      if (base_class_type->kind == (a_type_kind)tk_typeref &&
          base_class_type->variant.typeref.is_function_scope_tag) {
        base_class_type = base_class_type->variant.typeref.type;
      }  /* if */
      base_class_type->source_corresp.referenced = TRUE;
      /* If it is a const or volatile qualified type name (where in the ARM is
         this required???) or if it is the the class now being defined or if
         it is a union or if it has been declared but not yet defined (ARM
         10, p. 196), issue an error and skip over this class: it is not a
         valid base class name. */
      if (is_qualified_type(base_class_type) ||
          (base_class_type = skip_typerefs(base_class_type)) == type_ptr ||
          base_class_type->kind == (a_type_kind)tk_union ||
          !is_complete_class_struct_union_type(base_class_type)) {
        error(ec_bad_base_class);
        goto skip_base_class;
      }  /* if */
#if CHECKING
      if (ctsp == NULL) internal_error("scan_base_specifier_list: NULL ctsp");
#endif
      /* Before creating the base class entry and adding it to the list of
         base classes, go through the list looking for conflicts. */
      ambiguous = FALSE;
      for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
        if (bcp->type == base_class_type) {
          /* There is already a base class entry in the list that represents
             the same class. */
          if (bcp->direct) {
            /* It too is a directly derived base class.  This is an error. */
            error(ec_dupl_base_class_name);
            goto skip_base_class;
          } else if (bcp->is_virtual && is_virtual) {
            /* Both the current one and the one already on the list (which
               is an indirect base class) are virtual.  Only one needs to be
               on the list, and preference is given to the direct base class
               (unless the other gives greater access).  However, the position
               of the other must be preserved, so that we will get the order
               of initializers right (ARM 12.6.2).  Modify the other base class
               (and any base classes from which it is derived) in place. */
            fixup_virtual_base_class(bcp,
                                     (an_overriding_virtual_function_ptr)NULL,
                                     (a_derivation_step_ptr)NULL, access);
            goto skip_base_class;
          } else {
            /* At least one is non-virtual, so there is an ambiguity.  Mark
               both as ambiguous.  */
            ambiguous = bcp->ambiguous = TRUE;
          }  /* if */
        }  /* if */
      }  /* for */
      /* Issue a warning if an explicit access specifier was not provided
         (as per the recommendation on p. 243 of the ARM). */
      if (!access_already_specified) {
        str_warning(ec_missing_access_specifier, default_access_str);
      }  /* if */
      /* The current class will have to have a constructor if any of its base
         classes is virtual or itself has a constructor; it requires a
         destructor if any of its base classes has a destructor.  Record such
         requirements, if any, at this time. */
      cssp = symbol_supplement_for_class(type_ptr);
      bcp_cssp = symbol_supplement_for_class(base_class_type);
      if (is_virtual || bcp_cssp->constructor != NULL) {
        cssp->constructor_required = TRUE;
      }  /* if */
      if (bcp_cssp->destructor != NULL) {
        cssp->destructor_required = TRUE;
      }  /* if */
      /* Update the flag indicating whether there are any virtual base
         classes. */
      if (is_virtual || base_class_type->
                         variant.class_struct_union.any_virtual_base_classes) {
        type_ptr->variant.class_struct_union.any_virtual_base_classes = TRUE;
      }  /* if */
      /* Now create the new base class entry and add it to the end of the
         base classes list. */
      new_bcp = alloc_base_class();
      new_bcp->type = base_class_type;
      new_bcp->access = access;
      if (is_virtual) {
        new_bcp->is_virtual = TRUE;
        new_bcp->any_virtual_steps_in_derivation = TRUE;
      }  /* if */
      new_bcp->direct = TRUE;
      new_bcp->ambiguous = ambiguous;
      new_bcp->derivation = make_derivation_step(new_bcp,
                                                 (a_derivation_step_ptr)NULL);
      /* Offset is updated in merge_field_lists. */
      new_bcp->offset = 0;
      /* Enter the base name on the base class list in the derived class's
         class-supplement entry. */
      if (ctsp->base_classes == NULL) {
        ctsp->base_classes = new_bcp;
      } else {
        end_of_base_classes_list->next = new_bcp;
      }  /* if */
      end_of_base_classes_list = new_bcp;
      /* Add base classes derived from this base class to the current class's
         base class list.  They are marked as indirect. */
      bcp = new_bcp->type->variant.class_struct_union.extra_info->base_classes;
      for (; bcp != NULL; bcp = bcp->next) {
        if (bcp->direct) {
          /* Add the direct base class and all *its* base classes to the
             base class list for the derived class. */
          new_bcp = add_indirect_base_class(bcp, ctsp->base_classes,
                                            &end_of_base_classes_list,
                                            new_bcp->derivation);
        } else {
          /* Indirect base classes must have their virtual function override
             lists copied. */
          if (new_bcp->next == NULL) continue;
          if (bcp->type == new_bcp->next->type) {
            new_bcp = new_bcp->next;
          }  /* if */
        }  /* if */
        copy_virtual_function_override_list(bcp->overriding_virtual_functions,
                                            new_bcp, new_bcp);
      }  /* for */
skip_base_class:
      /* Advance past the base class name to the comma or right brace. */
      (void)get_token();
    }  /* if */
    /* Advance past the next comma, if any, and scan the next base class
       specifier. */
    remove_stop_token(tok_comma);
  } while (loop_token(tok_comma));
#if CHECKING
  if (type_ptr->kind != (a_type_kind)tk_union) {
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      verify_path_consistency(type_ptr, bcp);
      verify_virt_func_override_list(type_ptr, bcp, /*null_allowed=*/FALSE);
    }  /* for */
  }  /* if */
#endif /* CHECKING */
  db_exit();
}  /* scan_base_specifier_list */


static void check_enum_type_for_bit_field(a_type_ptr bit_field_type,
                                          long       bit_field_size,
                                          a_boolean *need_signed_type)
/*
Check to see that the values of the enumerated type bit_field_type will all
fit in a bit field of size bit_field_size.  If not, give a warning.  Return
*need_signed_type TRUE if the bit field type must be signed, FALSE if it
must be unsigned.
*/
{
  long           smallest, largest, smallest_possible, largest_possible,
                 enum_val;
  a_constant_ptr enum_con;
  a_boolean      use_signed;

  /* Check the constants on the list.  Start by finding the largest and
     smallest constants.  All constants are "int", so no consideration of
     unsigned constants is necessary.  We are assuming most enum type lists
     won't be too long, and there won't be too many bit fields with enum
     type, so a linear search should be acceptable.  Furthermore,
     the usual case is that the bit field is big enough, so we're probably
     going to scan the whole constant list; therefore it's okay to always
     scan the whole list even though some errors could be detected during
     the scan. */
  smallest = largest = 0;
  for (enum_con = bit_field_type->variant.integer.enum_constant_list;
       enum_con != NULL;
       enum_con = enum_con->next) {
    enum_val = enum_con->variant.integer_value;
    if (enum_val < smallest) smallest = enum_val;
    if (enum_val >  largest) largest  = enum_val;
  }  /* for */
  /* Determine the proper signedness for the bit field.  One can't
     simply use the signedness of the enum type, since that was chosen
     for efficiency reasons: if the enum values just fit in the bit
     field size, an unsigned field might be necessary even though a 
     signed type was a good choice for the enum type. */
  if (smallest < 0) {
    /* Some enum values are negative, so a signed type is required.
       The enum type must already be signed. */
    use_signed = TRUE;
  } else if ((((unsigned long)1 << (bit_field_size-1)) & largest) &&
             largest >= 0) {
    /* The largest value is positive, and it's so big it would require
       a "1" in the sign bit.  Therefore, an unsigned type is required. */
    use_signed = FALSE;
  } else {
    /* The signedness is not forced by the enum values, so use the 
       target preference.  Make a one-bit field always unsigned. */
    use_signed = (bit_field_size != 1 &&
                  !TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED);
  }  /* if */
  /* Make the largest and smallest values that will fit in the
     bit field, given the signedness selected. */
  /* Make the largest possible signed value, i.e., all 1's below the
     sign bit. */
  largest_possible = ((unsigned long)1 << (bit_field_size-1)) - 1;
  if (use_signed) {
    /* Signed bit field. */
    smallest_possible = ~largest_possible;
  } else {
    /* Unsigned bit field. */
    smallest_possible = 0;
    /* Turn "all 1's below the sign bit" into "all 1's including the sign 
       bit".  Watch out for integer overflow. */
    largest_possible = ((unsigned long)largest_possible << 1) | 1;
  }  /* if */
  /* Check that the enum values will fit in the bit field. */
  if (smallest < smallest_possible || largest > largest_possible) {
    warning(ec_enum_bit_field_too_small);
  }  /* if */
  *need_signed_type = use_signed;
}  /* check_enum_type_for_bit_field */


static void scan_bit_field_size(a_boolean  unnamed_bit_field,
                                a_type_ptr *p_base_type,
                                long       *p_bit_field_size)
/*
Scan the size in a bit-field declaration:

    unsigned int j: 5 ;
                    ^---- this size.

The current token is the colon preceding the size.  If unnamed_bit_field
is TRUE, the bit-field is unnamed.  *p_base_type gives the base type
of the declaration (unsigned int in the above example); it may be updated
on return.  *p_bit_field_size is set to the bit field size in bits.
*/
{
  long       bit_field_size;
  a_type_ptr base_type = *p_base_type;
  a_constant constant;
  a_type_ptr bit_field_type;

  /* Bit field.  ANSI says the type of a bit-field must be int, unsigned int,
     or signed int, but we also allow enums and integral types (see A.6.5.8
     in the Common Extensions appendix).  pcc allows those same things, so
     the ANSI and pcc behaviors are the same. */
  bit_field_type = skip_typerefs(base_type);
  if (!is_integral_type(bit_field_type)) {
    /* Error, not an integral type. */
    if (!is_error_type(bit_field_type)) error(ec_bad_bit_field_type);
    bit_field_type = integer_type((an_integer_kind)ik_int);
  } else {
    /* Integral base type.  In strict ANSI mode, give a warning about a
       nonstandard base type (anything other than int, unsigned int, and
       signed int).  In C++, however, any integer type is allowed (ARM 9.6). */
    if (C_dialect != C_dialect_cplusplus && strict_ansi_mode) {
      if (bit_field_type->variant.integer.enum_type ||
          (bit_field_type->variant.integer.int_kind !=
                                                     (an_integer_kind)ik_int &&
           bit_field_type->variant.integer.int_kind !=
                                           (an_integer_kind)ik_unsigned_int)) {
        warning(ec_nonstd_bit_field_type);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Advance past the colon. */
  (void)get_token();
  /* Scan the integral size in bits of the bit-field. */
  scan_integral_constant_expression(&constant);
  if (is_error_constant(&constant)) {
    /* Use small value to avoid more errors, but not 1 which is special. */
    bit_field_size = TARG_CHAR_BIT;
  } else {
#if CHECKING
    if (constant.kind != (a_constant_repr_kind)ck_integer) {
      internal_error("scan_bit_field_size: size not int");
    }  /* if */
#endif /* CHECKING */
    /* The size of the bit field must be non-negative and must not exceed
       the size of the underlying type (except for enums, whose type was
       picked by the front end) or the target maximum bit field size.
       Note that this also catches very large unsigned values (they look
       negative). */
    bit_field_size = constant.variant.integer_value;
    if (bit_field_size < 0 ||
        (!bit_field_type->variant.integer.enum_type &&
         bit_field_size > (bit_field_type->size*TARG_CHAR_BIT))) {
      error(ec_bad_bit_field_size);
      bit_field_size = bit_field_type->size*TARG_CHAR_BIT;
    } else if (bit_field_size > TARG_MAX_BIT_FIELD_SIZE) {
      error(ec_bad_bit_field_size);
      bit_field_size = TARG_MAX_BIT_FIELD_SIZE;
    } else if (bit_field_size == 0) {
      /* The bit-field size is zero, so the field must be unnamed. */
      if (!unnamed_bit_field) {
        error(ec_zero_length_bit_field_must_be_unnamed);
        bit_field_size = 1;
      }  /* if */
    } else if (bit_field_type->variant.integer.enum_type) {
      /* The integral type is an enum type.  Give a warning if any of the
         enumeration's constants will not fit in the bit field, and determine
         whether the bit field should be signed or unsigned. */
      a_boolean need_signed_type;
      check_enum_type_for_bit_field(bit_field_type, bit_field_size,
                                    &need_signed_type);
      /* Change the base type for the bit field if necessary to get the
         right signedness. */
      if (need_signed_type !=
                int_kind_is_signed(bit_field_type->variant.integer.int_kind)) {
        /* Change to a signed or unsigned int type with the enum type
           indicated in it.  Note that this is a new and unshared type. */
        a_type_ptr new_enum_type = alloc_type((a_type_kind)tk_integer);
        new_enum_type->variant.integer.int_kind =
                           need_signed_type ? (an_integer_kind)ik_int :
                                              (an_integer_kind)ik_unsigned_int;
        new_enum_type->variant.integer.enum_type = TRUE;
        new_enum_type->variant.integer.enum_constant_list =
                            bit_field_type->variant.integer.enum_constant_list;
        set_type_size(new_enum_type);
        bit_field_type = new_enum_type;
      }  /* if */
    } else if (bit_field_type->variant.integer.explicitly_signed) {
      /* The integral type was explicitly signed in the source.  (This
         information comes from the type entry itself.)  The integral type
         stays what it is (specifically, "int" stays "int" and therefore
         signed).  Give a warning for an explicitly signed one-bit field;
         ANSI allows it, but it's strange. */
      if (bit_field_size == 1) warning(ec_signed_one_bit_field);
    } else if (bit_field_type->variant.integer.int_kind ==
                                                     (an_integer_kind)ik_int) {
      /* The integral type is a "plain" int, i.e., it's int, it's not
         explicitly signed, and it's not an enum type.  This is converted
         to the target preference with regard to signedness.  A one-bit field
         is probably not intended to be signed, so make it unsigned. */
      if (bit_field_size == 1 || TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED) {
        bit_field_type =integer_type((an_integer_kind)ik_unsigned_int);
      }  /* if */
    }  /* if */
  }  /* if */
  if (bit_field_type == skip_typerefs(base_type)) {
    /* The original type, base_type, has turned out to be correct after all.
       Use it directly to avoid wasting the type qualifiers, if any. */
  } else {
    /* Build a type with the right qualifiers. */
    base_type = make_qualified_type(bit_field_type,
                                    is_const_qualified_type(base_type),
                                    is_volatile_qualified_type(base_type));
  }  /* if */
  *p_base_type = base_type;
  *p_bit_field_size = bit_field_size;
}  /* scan_bit_field_size */


static a_boolean is_member_decl_start(void)
/*
Return TRUE if the current token looks like the start a member declaration
of a C++ class, struct, or union or a C struct or union.
*/
{
  a_boolean     is_start = FALSE;

  if (is_type_start()) {
    /* This is the only check required for C struct/union fields. */
    is_start = TRUE;
  } else if (C_dialect == C_dialect_cplusplus) {
    /* C++ class/struct/union members can also start with one of the following
       keywords. */
    is_start = (curr_token == tok_static || curr_token == tok_typedef ||
                curr_token == tok_private || curr_token == tok_protected ||
                curr_token == tok_public || curr_token == tok_compl);

  }  /* if */
  return is_start;
}  /* is_member_decl_start */


static void decl_friend_class(a_type_ptr          class_type,
                              a_type_ptr          friend_class_type)
/*
Do processing for declaring an entire class (friend_class_type) friend of
the current class (class_type).
*/
{
  a_class_list_entry_ptr      clep;
  a_class_type_supplement_ptr ctsp;

  ctsp = friend_class_type->variant.class_struct_union.extra_info;
  /* Issue a warning if this is a duplicate friend declaration. */
  for (clep = ctsp->befriending_classes; clep != NULL; clep = clep->next) {
    if (clep->class_type == class_type) {
      warning(ec_duplicate_friend_decl);
      break;
    }  /* if */
  }  /* for */
  if (clep == NULL) {
    /* No duplication was detected. */
    clep = alloc_list_entry_for_class();
    clep->class_type = class_type;
    clep->next = ctsp->befriending_classes;
    ctsp->befriending_classes = clep;
  }  /* if */
}  /* decl_friend_class */


a_symbol_ptr member_function_redecl_sym(a_symbol_ptr  sym,
                                        a_type_ptr    new_type)
/*
sym is a member function symbol or overloaded function symbol from a
previous declaration.  new_type is the type from the current declaration.
Check the type for compatibility with sym or, if sym represents an
overloaded function, with any of the instances.  If a match is found,
return a pointer to the symbol.  If not, return NULL.

If the routine type from the current declaration or one from the original
declaration indicates that the function as a whole was qualified (e.g.,
int f(int) const), then the type compatibility check must take the
const qualification into account when seeking a match.  In other words,
if one of the functions was so qualified, both must be for them to have
compatible types.  The qualification is indicated on the type pointed to
by the implicit "this" param type:  without a const qualifier, say, it would
be "const pointer to class-type", but with one it would be "const pointer
to const class-type".

Otherwise, the type compatibility check is done based only on the return
type and parameters; the type of the implicit "this" parameter, if any, is
ignored.  This if for two reasons.  (1) When we are looking for a
redeclaration within a class definition, two declarations with otherwise
identical type signatures are not considered distinct because in one
"static" is present and in the other it is not.  (2) When we are checking
a member function definition that appears separately from the declaration
in the class definition, there is no way for the user to specify whether
it is a static or nonstatic member function; that can be determined only
by looking back at the original declaration.  So, new_type cannot yet
have a non-NULL implicit_this_param_type and the type match must be done
without it.
*/
{
  a_boolean                      is_overloaded_function, match;
  a_type_ptr                     orig_type, orig_this_type, new_this_type;
  a_routine_type_supplement_ptr  orig_rts, new_rts;
  a_boolean                      orig_function_is_qualified;
  a_boolean                      new_function_is_qualified;

  /* Get the symbol list if this is an overloaded function. */
  if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    sym = sym->variant.overloaded_function.symbols;
    is_overloaded_function = TRUE;
  } else {
    is_overloaded_function = FALSE;
  }  /* if */
#if CHECKING
  if (sym->kind != (a_symbol_kind)sk_member_function) {
    internal_error("member_function_redecl_sym: bad sym kind");
  }  /* if */
#endif /* CHECKING */
  /* Make the check without regard to the presence of an implicit "this"
     parameter.  types_are_compatible should do the comparison based only
     on the return type and parameters. */
  new_rts = new_type->variant.routine.extra_info;
  new_this_type = new_rts->implicit_this_param_type;
  new_function_is_qualified =
                 (new_this_type != NULL &&
                  is_qualified_type(type_pointed_to(new_this_type)));
  /* Go through the symbol list and look for an instance in which the
     types are compatible with the current type. */
  for (; sym != NULL; sym = is_overloaded_function ? sym->next : NULL) {
    orig_type = sym->variant.routine->type;
    orig_rts = (skip_typerefs(orig_type))->variant.routine.extra_info;
    orig_this_type = orig_rts->implicit_this_param_type;
    orig_function_is_qualified =
                 (orig_this_type != NULL &&
                  is_qualified_type(type_pointed_to(orig_this_type)));
    if (new_function_is_qualified != orig_function_is_qualified) {
      /* No match is possible.  Don't bother calling types_are_compatible. */
    } else {
      if (new_function_is_qualified) {
        /* Both routines are qualified.  Use the "this" param type as part
           of the compatibility check. */
      } else {
        /* Neither routine is qualified.  Save away the "this" param types,
           do the compatibility check without them, and then restore them. */
        new_rts->implicit_this_param_type = NULL;
        orig_rts->implicit_this_param_type = NULL;
      }  /* if */
      match = types_are_compatible(orig_type, new_type);
      if (!new_function_is_qualified) {
        /* Restore the implicit "this" parameter types in orig_type and
           new_type. */
        new_rts->implicit_this_param_type = new_this_type;
        orig_rts->implicit_this_param_type = orig_this_type;
      }  /* if */
      /* If a match was found by types_are_compatible, break out of the
         loop. */
      if (match) break;
    }  /* if */
  }  /* for */
  return sym;
}  /* member_function_redecl_sym */


static a_symbol_ptr decl_friend_function(a_symbol_locator    *locator,
                                         a_type_ptr          class_type,
                                         a_type_ptr          function_type,
                                         a_boolean           is_inline)
/*
Do processing for declaring a function (identified by *locator and with
a type of function_type) friend of the current class (class_type).  Getting
the correct symbol of a previously declared function means taking overloading
into account.  For nonmember functions, this could be the initial declaration
of the function, and again overloading is a possibility.
*/
{
  a_symbol_ptr            sym, ext_sym;
  an_id_linkage_kind      linkage;
  a_type_ptr              old_type;
  a_class_list_entry_ptr  clep;
  a_boolean               is_overloaded_function;
  a_boolean               is_function_def_with_body;
  a_storage_class         storage_class;

  db_enter(3, "decl_friend_function");
  if (!is_error_locator(*locator)) {
    is_function_def_with_body = (curr_token == tok_lbrace);
    sym = locator->specific_symbol;
    if (sym != NULL && sym->class_of_which_a_member != NULL &&
        !is_member_function_symbol(sym)) {
      /* sym represents a member of a class, but it is not a member function.
         Issue an error. */
      pos_error(ec_not_compatible_with_previous_decl,
                &locator->source_position);
      sym = NULL;
      set_to_error_locator(*locator);
    }  /* if */
    if (sym == NULL || !is_member_function_symbol(sym)) {
      /* Not a member function.  Get the symbol -- the rest of what's
         returned from decl_var_or_routine is not relevant for processing
         in this context. */
      /* If the friend function is defined in this declaration or if it was
         specified as inline, that information should be passed on to
         decl_var_or_routine. */
      if (is_function_def_with_body) is_inline = TRUE;
      if (is_inline) {
        storage_class = (a_storage_class)sc_static;
      } else {
        storage_class = (a_storage_class)sc_extern;
      }  /* if */
      decl_var_or_routine(locator, storage_class, function_type,
                          /*is_implicit_function=*/FALSE,
                          is_function_def_with_body, is_inline,
                          &sym, &linkage, &old_type, &ext_sym);
    } else {
      /* It's a member function.  Find the right type signature for this
         member function name.  If none can be found, NULL is returned. */
      is_overloaded_function =
                        sym->kind == (a_symbol_kind)sk_overloaded_function;
      sym = member_function_redecl_sym(sym, function_type);
      if (sym == NULL) {
        if (is_overloaded_function) {
          str_error(ec_overloaded_function_incompatible_type,
                    locator->symbol_header->identifier);
        } else {
          error(ec_not_compatible_with_previous_decl);
        }  /* if */
        set_to_error_locator(*locator);
      } else {
        if (is_inline && !is_function_def_with_body &&
            !sym->variant.routine->is_inline) {
          error(ec_inline_not_allowed);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (is_error_locator(*locator)) {
    /* Create a dummy symbol to return when there's been an error.  This is
       required for further processing, in case there's a definition of the
       the routine body. */
    sym = enter_symbol((a_symbol_kind)sk_routine, locator,
                       /*at_file_scope=*/FALSE,
                       /*suppress_redecl_error=*/FALSE);
    sym->variant.routine = make_routine(function_type,
                                        (a_storage_class)sc_static,
                                        /*at_file_scope=*/TRUE);
    /* Set the source correspondence. */
    set_source_corresp(&sym->variant.routine->source_corresp, sym);
  } else {
    clep = sym->variant.routine->befriending_classes;
    /* Issue a warning if this is a duplicate friend declaration. */
    for (; clep != NULL; clep = clep->next) {
      if (clep->class_type == class_type) {
        warning(ec_duplicate_friend_decl);
        break;
      }  /* if */
    }  /* for */
    if (clep == NULL) {
      /* No duplication was detected. */
      clep = alloc_list_entry_for_class();
      clep->class_type = class_type;
      clep->next = sym->variant.routine->befriending_classes;
      sym->variant.routine->befriending_classes = clep;
    }  /* if */
  }  /* if */
  db_exit();
  return sym;
}  /* decl_friend_function */


a_boolean is_default_constructor(a_routine_ptr  ctor_rout)
/*
Return TRUE if ctor_rout points to a default constructor routine entry.
*/
{
  a_param_type_ptr  ptp;

#if CHECKING
  if (ctor_rout->special_kind != (a_special_function_kind)sfk_constructor) {
    internal_error("is_default_constructor: expected a constructor");
  }  /* if */
#endif /* CHECKING */
  ptp = ctor_rout->type->variant.routine.extra_info->param_type_list;
  /* There are no parameters or if the first (and therefore its successors,
     if any) has a default argument expression, then this is a default
     constructor. */
  return (ptp == NULL || ptp->has_default_arg);
}  /* is_default_constructor */


a_boolean is_copy_constructor(a_routine_ptr  ctor_rout,
                              a_type_ptr     class_of_which_a_member,
                              a_boolean      *const_object_okay,
                              a_boolean      *volatile_object_okay)
/*
Return TRUE if ctor_rout points to a copy constructor routine entry for
class_of_which_a_member; if it does, also set and return *const_object_okay
and/or *volatile_object_okay, depending on whether the type of the copy
constructor's first parameter is const or volatile qualified (or both).
*/
{
  a_param_type_ptr  ptp;
  a_type_ptr        tp;
  a_boolean         is_cctor = FALSE;

#if CHECKING
  if (ctor_rout->special_kind != (a_special_function_kind)sfk_constructor) {
    internal_error("is_copy_constructor: expected a constructor");
  }  /* if */
#endif /* CHECKING */
  *const_object_okay = FALSE;
  *volatile_object_okay = FALSE;
  /* A constructor is deemed a copy constructor if (1) the type of the first
     parameter is reference-to-class or reference-to-const-class where
     "class" is the class of which it is a member function, and (2) where
     the function can be called with only one argument. */
  ptp = ctor_rout->type->variant.routine.extra_info->param_type_list;
  /* If the param type entry is non-NULL there is at least one argument.  If
     there is a second argument and it has a default expression, the function
     call need not explicitly mention the second argument. */
  if (ptp != NULL && is_reference_type(ptp->type) &&
      (ptp->next == NULL || ptp->next->has_default_arg)) {
    tp = type_pointed_to(ptp->type);
    if (skip_typerefs(tp) == class_of_which_a_member) {
      /* It is a copy constructor. */
      is_cctor = TRUE;
      /* See if the object being copied is const qualified. */
      if (is_const_qualified_type(tp)) *const_object_okay = TRUE;
      if (is_volatile_qualified_type(tp)) *volatile_object_okay = TRUE;
    }  /* if */
  }  /* if */
  return is_cctor;
}  /* is_copy_constructor */


static a_boolean is_operator_delete_symbol(a_symbol_ptr  sym)
/*
Return TRUE is sym is a symbol for an operator delete() function.
*/
{
  a_routine_ptr  rp;
  a_boolean      is_operator_delete;

  if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    is_operator_delete = FALSE;
  } else {
    rp = sym->variant.routine;
    is_operator_delete =
               (rp->special_kind == (a_special_function_kind)sfk_operator &&
                rp->opname_kind == (an_opname_kind)onk_delete);
  }  /* if */
  return is_operator_delete;
}  /* is_operator_delete_symbol */


static a_symbol_ptr symbol_for_member_function(a_symbol_locator  *locator,
                                               a_type_ptr        type,
                                               a_symbol_ptr      *overload_sym)
/*
Return a pointer to an sk_member_function symbol to represent a function
of a given type.  If this is a redeclaration, the existing symbol is
returned.  If this declaration overloads a function name, the symbol
returned will be on the sk_overloaded_function symbol's list.  If there
is an error in attempting to overload the function name, a new symbol
is returned nonetheless, but it is not added to the list of overloaded
function symbols.
*/
{
  a_symbol_ptr  sym, new_sym = NULL;
  an_error_code error_code;
  a_boolean     suppress_redecl_error = FALSE;

  db_enter(4, "symbol_for_member_function");
  *overload_sym = NULL;
  if (is_error_locator(*locator)) {
    sym = NULL;
  } else {
    /* See if there's already a member function with this name. */
    for (sym = locator->symbol_header->symbol; sym != NULL; sym = sym->next) {
      if (sym->decl_scope != scope_stack[decl_scope_level].number) {
        /* Symbols are added to the head of the list to hide symbols of the
           same name from containing scopes.  Thus, once we find a symbol that
           belongs to another scope, we can be sure there are none following
           it that might belong to the current class. */
        sym = NULL;
        break;
      } else if (name_space_for_symbol_kind[(int)sym->kind] == nsk_other) {
        /* Matches a name in the current scope. */
        if (sym->kind == (a_symbol_kind)sk_member_function ||
            sym->kind == (a_symbol_kind)sk_overloaded_function) {
          /* Remember sym -- it represents a member function. */
        } else {
          /* Found the name in the current class, but it is not a member
             function.  The error will be detected again and reported in
             enter_symbol, called below. */
          sym = NULL;
        }  /* if */
        break;
      }  /* if */
    }  /* for */
    if (sym != NULL) {
      /* A member function by this name has already been entered into the
         symbol table.  This could be a redeclaration, which is illegal for
         class members.  Check for that first by looking for a type match. */
      if (member_function_redecl_sym(sym, type) == NULL) {
        /* The previously declared function with the same name (or, if it is
           already overloaded, any instance of it) does not have a matching
           type, so sym remains a candidate for overloading. */
      } else {
        /* Force enter_symbol to be called by setting sym to NULL. */
        sym = NULL;
      }  /* if */
    }  /* if */
    if (sym != NULL) {
      /* Not a redeclaration, so it is probably the overloading of a function
         name.  The routine overload_distinguishable returns TRUE if the
         routine types are candidates for overloading; if it returns FALSE
         it also returns the error code for a diagnostic explaining why. */
      if (is_operator_delete_symbol(sym)) {
        /* Overloading is not allowed for operator delete() (ARM 12.5). */
        pos_error(ec_delete_already_declared, &locator->source_position);
        suppress_redecl_error = TRUE;
      } else if (!overload_distinguishable(sym, type, &error_code)) {
        pos_error(error_code, &locator->source_position);
        suppress_redecl_error = TRUE;
      } else {
        /* Enter this symbol as an instance of overloading. */
        new_sym = enter_overloaded_symbol((a_symbol_kind)sk_member_function,
                                          locator, sym, overload_sym);
      }  /* if */
    }  /* if */
  }  /* if */
  if (new_sym == NULL) {
    /* No member function symbol with this name exists yet, or else this is a
       redeclaration which will cause an error to be issued.  Create a new
       member function symbol. */
    new_sym = enter_local_symbol((a_symbol_kind)sk_member_function,
                                 locator, decl_scope_level,
                                 suppress_redecl_error);
  }  /* if */
  db_exit()
  return new_sym;
}  /* symbol_for_member_function */

                                               
static a_symbol_ptr decl_member_function(a_symbol_locator        *locator,
                                         a_type_ptr              class_type,
                                         a_type_ptr              member_type,
                                         an_access_specifier     access,
                                         a_boolean               is_inline,
                                         a_boolean               is_virtual,
                                         a_special_function_kind spec_kind)
/*
For a member function declaration:  create a symbol entry and a routine entry
for the member function, add the symbol to the symbol table, and append the
routine entry to the routines list for the current class.  *locator give the
source locator of the declaration.  class_type points to the type entry of the
class of which the function is a member, and member_type points to the type
entry of the function itself.  access is the access control governing the
specification.  is_inline and is_virtual indicate whether the inline and
virtual keywords were specified in the declaration.  spec_kind identifies the
special function kind (e.g., constructor, destructor), if any.
*/
{
  a_symbol_ptr                   sym, overload_sym;
  a_routine_ptr                  rtn;
  a_class_symbol_supplement_ptr  cssp;
  a_boolean                      const_object_okay, dummy_flag;
  a_type_ptr                     tp;
  a_conversion_list_entry_ptr    clep;

  db_enter(3, "decl_member_function");
#if 0
  /* We have at least one unresolved problem when a member function is
     declared using a typedef name.  The referenced type will have no
     implicit "this" param pointer, so by default such functions will be
     static.  But when no static specifier appears, member_type could
     have a "this" param type pointer added (this would involve making a
     copy of member type), or if all functions are treated as static, a
     warning should probably be issued.  The ARM is silent on how such
     declarations should be handled. */
#endif /* if 0 */
  /* Look for a prior declaration or function overloading. */
  sym = symbol_for_member_function(locator, member_type, &overload_sym);
  if (sym->variant.routine != NULL) {
    /* symbol_for_member_function has returned a symbol that has already
       been declared.  No further action is necessary. */
  } else {
    sym->class_of_which_a_member = class_type;
    /* Create the routine entry for the member function. */
    /* The routine is allocated in the current memory region, as indicated
       by curr_il_region_number -- i.e., in the memory region of the scope in
       which its class is declared. */
    /* Member functions are static by default. */
    sym->variant.routine = rtn = make_routine(member_type,
                                              (a_storage_class)sc_static,
                                              /*at_file_scope=*/FALSE);
    /* Set the source correspondence, including the access specifier. */
    set_source_corresp(&rtn->source_corresp, sym);
    rtn->source_corresp.class_of_which_a_member = class_type;
    /* Member functions should have the same name linkage as the class of
       which they are members.  For now, the class will have internal or no
       linkage.  If and when its linkage is promoted to C++, the linkage of
       the member functions will also be changed. */
    rtn->source_corresp.name_linkage = class_type->source_corresp.name_linkage;
    rtn->source_corresp.access = access;
    rtn->is_inline = is_inline;
    cssp = symbol_supplement_for_class(class_type);
    /* If "virtual" was specified in the declaration, mark the routine as
       virtual.  Even if it wasn't, its virtualness can be inherited.  In
       either case record the relationship between the current routine and
       its appearance in the base classes of the current class. */
    if (check_for_virtual_function(is_virtual, sym, class_type,
                                   &locator->source_position)) {
      /* Classes with virtual functions require constructors. */
      cssp->constructor_required = TRUE;
    }  /* if */
    /* Do processing for special member functions, including assignment
       operators, constructors and destructors. */
    if (locator->is_operator_name) {
      rtn->special_kind = (a_special_function_kind)sfk_operator;
      rtn->opname_kind = locator->variant.opname;
      /* If this is an assignment operator, record a pointer to it in the
         symbol -- to facilitate generating default assignment operators. */
      if (rtn->opname_kind == (an_opname_kind)onk_assign) {
        if (cssp->assignment_operator == NULL) {
          cssp->assignment_operator = sym;
        } else if (cssp->assignment_operator->kind ==
                                    (a_symbol_kind)sk_overloaded_function) {
          /* The overloaded function symbol is already registered. */
        } else {
          /* The overloaded function symbol was just created. */
          cssp->assignment_operator = overload_sym;
        }  /* if */
      }  /* if */
    } else if (locator->is_conversion_name) {
      /* User-defined conversion function. */
      rtn->special_kind = (a_special_function_kind)sfk_conversion;
      /* Create a conversion list entry.  This list provides an alternative
         to traversing the entire symbols list for a class to find its
         conversion functions. */
      clep = alloc_conversion_list_entry();
      clep->symbol = sym;
      clep->next = cssp->conversion_list;
      cssp->conversion_list = clep;
      /* If the return type of the conversion is a class type (without const
         or volatile qualifier) set a flag to mark it as target of a
         conversion. */
      tp = rtn->type->variant.routine.return_type;
      if (!is_qualified_type(tp) && is_class_struct_union_type(tp) ) {
        (symbol_supplement_for_class(skip_typerefs(tp)))->
                  target_of_conversion_function = TRUE;
      }  /* if */
    } else {
      rtn->special_kind = spec_kind;
    }  /* if */
    /* If this is a user-defined conversion or an overloaded operator,
       check for errors in the argument list. */
    check_operator_function_params(rtn, &locator->source_position);
    if (spec_kind == (a_special_function_kind)sfk_constructor) {
      /* Set the pointer to the constructor symbol in the class symbol
         supplement. */
      if (cssp->constructor == NULL) {
	cssp->constructor = sym;
      } else if (cssp->constructor->kind ==
				  (a_symbol_kind)sk_overloaded_function) {
	/* The overloaded function symbol is already registered. */
      } else {
	/* The overloaded function symbol was just created. */
	cssp->constructor = overload_sym;
      }  /* if */
      /* Determine if this is a default constructor. */
      if (is_default_constructor(rtn)) {
        cssp->has_default_constructor = TRUE;
      }  /* if */
      /* Determine if this is a copy constructor.  If so, set the class symbol
         supplement flags appropriately. */
      if (is_copy_constructor_symbol(sym, &const_object_okay, &dummy_flag)) {
        cssp->has_copy_constructor = TRUE;
        cssp->has_copy_constructor_for_const_object |= const_object_okay;
      }  /* if */
    } else if (spec_kind == (a_special_function_kind)sfk_destructor) {
      /* Set the pointer to the destructor symbol in the class symbol
         supplement. */
      cssp->destructor = sym;
    }  /* if */
    /* Do checking associated with function overloading. */
    if (overload_sym != NULL) {
      a_symbol_ptr  other_sym = sym->next;
#if CHECKING
      if (sym != overload_sym->variant.overloaded_function.symbols ||
          other_sym == NULL ||
          other_sym->kind != (a_symbol_kind)sk_member_function) {
        internal_error("decl_member_function:  bad overloading");
      }  /* if */
#endif /* CHECKING */
      /* Set a flag in overload_sym if the instances of an overloaded function
         are a mixture of static and nonstatic member functions. */
      if (!overload_sym->variant.overloaded_function.mixed_static_nonstatic) {
        if (routine_type_is_nonstatic_member_function(member_type) !=
            routine_type_is_nonstatic_member_function(
                                          routine_symbol_type(other_sym))) {
          overload_sym->
                   variant.overloaded_function.mixed_static_nonstatic = TRUE;
        }  /* if */
      }  /* if */
      /* Mark the overload symbol if the new symbol is a virtual function. */
      if (rtn->is_virtual) {
        overload_sym->variant.overloaded_function.any_virtual_functions = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) db_symbol(sym, "", 4);
#endif /* DEBUG */

  db_exit();
  return sym;
}  /* decl_member_function */


static void scan_pure_specifier(a_symbol_ptr  rout_sym,
                                a_type_ptr    class_type,
                                a_boolean     suppress_error)
/*
The current token is an "=", encountered just after the scanning of a
member or friend function declarator.  A pure specifier is defined as "= 0",
and it is legal for virtual member functions only.
*/
{
  a_boolean          pure_specifier_allowed;

  db_enter(4, "scan_pure_specifier");
  /* A pure specifier is allowed for virtual functions only.  (Check the
     class_of_which_a_member to exclude friend declarations.) */
  pure_specifier_allowed = (rout_sym->class_of_which_a_member == class_type &&
                            rout_sym->variant.routine->is_virtual);
  if (!pure_specifier_allowed && !suppress_error) {
    pos_error(ec_pure_specifier_on_nonvirtual_function, &pos_curr_token);
  }  /* if */
  /* Advance past the "=". */
  (void)get_token();
  if (curr_token == tok_int_constant &&
      const_for_curr_token.variant.integer_value == 0) {
    /* Token following "=" is "0". */
    if (pure_specifier_allowed) {
      /* Update the routine and class type enties. */
      rout_sym->variant.routine->pure_virtual = TRUE;
      class_type->variant.class_struct_union.abstract = TRUE;
    }  /* if */
    /* Advance past the "0". */
    (void)get_token();
  } else {
    copy_source_position(pos_curr_token, error_position);
    /* Invalid pure specifier:  something other than "0" follows the "=". */
    syntax_error(ec_bad_pure_specifier);
  }  /* if */
  db_exit();
}  /* scan_pure_specifier */


static void decl_member_constant(a_symbol_locator    *locator,
                                 a_type_ptr          class_type,
                                 a_type_ptr          member_type,
                                 an_access_specifier access)
/*
Do processing for a member constant, including scanning the initializer
constant and entering the name in the symbol table.  This construct is
not supported in the ARM.  The syntax we allow is:

  "const" simple-type-name    constant-member-name "=" constant-expression
                          opt

where the type specifier includes no storage class.  When simple-type-name
is omitted, the type defaults to "int".
*/
{
  a_symbol_ptr     sym;
  a_constant_ptr   cp;

  db_enter(3, "decl_member_constant");
  /* The current token is the "=".  Pointing to it issue a warning that this
     is a nonstandard construct. */
  if (strict_ansi_mode) {
    warning(ec_nonstd_const_member);
  } else {
    remark(ec_nonstd_const_member);
  }  /* if */
  /* Advance past the "=". */
  (void)get_token();
  /* Scan the constant expression. */
  cp = alloc_constant((a_constant_repr_kind)ck_error);
  scan_constant_initializer_expression(member_type, cp);
  add_to_constants_list(cp);
  /* Enter the constant name in the symbol table.  Do this after scanning
     the expression to avoid problems with a recursive reference, though
     it may mean the order in which errors are issued is a little strange. */
  sym = enter_local_symbol((a_symbol_kind)sk_constant, locator,
                           decl_scope_level, /*suppress_redecl_error=*/FALSE);
  /* Update the symbol and the constant entry. */
  sym->variant.constant = cp;
  set_source_corresp(&(cp->source_corresp), sym);
  cp->source_corresp.access = access;
  cp->source_corresp.class_of_which_a_member =
                          sym->class_of_which_a_member = class_type;
  db_exit();
}  /* decl_member_constant */


static void decl_static_data_member(a_symbol_locator    *locator,
                                    a_type_ptr          class_type,
                                    a_type_ptr          member_type,
                                    an_access_specifier access)
/*
Do processing for a static data member, including entering it in the symbol
table.
*/
{
  a_symbol          *sym;
  a_variable        *var;

  db_enter(3, "decl_static_data_member");
  /* Enter a new symbol in the symbol table. */
  sym = enter_local_symbol((a_symbol_kind)sk_static_data_member,
                           locator, decl_scope_level,
                           /*suppress_redecl_error=*/FALSE);
  sym->class_of_which_a_member = class_type;
  /* Create the variable entry for the static data member. */
  /* The storage class of static data members is sc_static until they are
     promoted to external linkage, at which time the storage class will
     become sc_extern or sc_unspecified (depending on whether or not a
     definition is provided).  All static data member variables are allocated
     in the file scope memory region and put on the variables list for the
     current class. */
  var = make_variable(member_type, (a_storage_class)sc_static,
                      /*at_file_scope=*/FALSE);
  sym->variant.variable = var;
  /* Set the source correspondence fields of the variable. */
  set_source_corresp(&var->source_corresp, sym);
  var->source_corresp.class_of_which_a_member = class_type;
  /* Static data members will have the same name linkage as the class of
     which they are members.  For now, the class will have internal linkage.
     If and when its linkage is promoted to C++, the linkage of the static
     data members will also be changed. */
  var->source_corresp.name_linkage = class_type->source_corresp.name_linkage;
  var->source_corresp.access = access;
#if DEBUG
  if (debug_level >= 3) db_symbol(sym, "", 4);
#endif /* DEBUG */
  db_exit();
}  /* decl_static_data_member */


static a_boolean increment_field_offsets(a_targ_size_t *byte_offset,
                                         int           *bit_offset,
                                         a_targ_size_t byte_incr,
                                         int           bit_incr)
/*
Increment the byte and bit offsets by the indicated amount, checking for
overflow.  Return TRUE if the update is successful, FALSE if there was an
overflow error.
*/
{
  /* The ULTRIX C compiler has trouble with the type of folded compile-time
     unsigned expressions, so we use a variable for this value. */
  a_targ_size_t max_byte_offset = TARG_SIZE_T_MAX / TARG_CHAR_BIT;
  a_targ_size_t extra_byte_offset;
  a_boolean     overflow = FALSE;

  db_enter(4, "increment_field_offsets");
  /* The offset will eventually go into the field as a bit offset, and
     therefore the maximum byte offset is somewhat smaller than one might
     expect. */
  if (byte_incr >= max_byte_offset ||
      *byte_offset > (max_byte_offset - byte_incr)) {
    overflow = TRUE;
  } else {
    *byte_offset += byte_incr;
  }  /* if */
  if (bit_incr != 0) {
    if (*bit_offset > INT_MAX-bit_incr) {
      overflow = TRUE;
    } else {
      *bit_offset += bit_incr;
    }  /* if */
    /* If the bit offset has gone into the next byte, transfer some of the
       bit offset over to the byte offset. */
    if (*bit_offset >= TARG_CHAR_BIT) {
      extra_byte_offset = *bit_offset / TARG_CHAR_BIT;
      if (*byte_offset > max_byte_offset-extra_byte_offset) {
        overflow = TRUE;
      } else {
        *byte_offset += extra_byte_offset;
      }  /* if */
      *bit_offset = *bit_offset % TARG_CHAR_BIT;
    }  /* if */
  }  /* if */
  db_exit();
  return !overflow;
}  /* increment_field_offsets */


a_boolean do_alignment(a_targ_size_t    *byte_offset,
                       int              *bit_offset,
                       a_targ_alignment alignment)
/*
Increment the byte and bit offsets to align them with the indicated 
byte-multiple boundary.  Return TRUE if the update is successful, FALSE if
there was an overflow error.
*/
{
  a_targ_size_t byte_mod;
  a_boolean	overflow = FALSE;

  if (*bit_offset != 0) {
    /* If the bit offset indicates a partial storage byte, round the offsets
       to the next byte. */
    overflow = !increment_field_offsets(byte_offset, bit_offset,
				       (a_targ_size_t)0,
                                       (int)(TARG_CHAR_BIT - *bit_offset));
  }  /* if */
  if (!overflow) {
    byte_mod = *byte_offset % alignment;
    if (byte_mod != 0) {
      /* Increment the byte offset to make it a multiple of the required
         alignment. */
      overflow = !increment_field_offsets(byte_offset, bit_offset,
                                         (a_targ_size_t)(alignment - byte_mod),
					 0);
    }  /* if */
  }  /* if */
  return !overflow;
}  /* do_alignment */


#if TARG_BIT_FIELD_CONTAINER_SIZE >= 0
/*ARGSUSED*/ /* <-- base_type is not used. */
#endif /* TARG_BIT_FIELD_CONTAINER_SIZE >= 0 */
static a_boolean align_offsets_for_bit_field(int              bit_size,
                                             a_targ_size_t    *byte_offset,
                                             int              *bit_offset,
					     a_targ_alignment *p_alignment,
                                             a_type_ptr       base_type)
/*
As part of maintaining field offsets while processing fields of a struct
definition, update *byte_offset and *bit_offset to indicate the position
(after alignment if necessary) of a bit-field of size bit_size.  If
bit_size == 0, this forces some kind of bit-field alignment.  See 3.5.2.1.
base_type is the integral base type for the bit field (e.g., int, unsigned
int).  Return the effective alignment for the field, i.e., the alignment
for the container used, in *p_alignment.  If any overflow was detected in
computing the alignment, FALSE is returned; if there's no overflow TRUE is
returned.
*/
{
  a_targ_size_t    container_size;
  a_targ_alignment container_alignment;
  a_boolean	   overflow = FALSE;

  db_enter(4, "align_offsets_for_bit_field");

/*
Useful macro that determines whether a field of size bit_size at the
current offset will fit into a container of size container_size (in bytes)
aligned according to container_alignment.
*/
#define fits_in_container(container_size, container_alignment)        \
 (((*byte_offset % (container_alignment))*TARG_CHAR_BIT + *bit_offset) + \
                                    bit_size <= (container_size)*TARG_CHAR_BIT)

  /* TARG_BIT_FIELD_CONTAINER_SIZE is
       >  0 to indicate a particular size for the bit-field container.
       == 0 to indicate "use the smallest integral type into which the
            bit-field will fit".
       < 0  to indicate "use the base type from the declaration as
            the container type".
  */
#if TARG_BIT_FIELD_CONTAINER_SIZE > 0
  /* Use a fixed size container.  TARG_BIT_FIELD_CONTAINER_SIZE indicates the
     size in bytes. */
  container_size = TARG_BIT_FIELD_CONTAINER_SIZE;
#if TARG_BIT_FIELD_CONTAINER_SIZE == 1
  container_alignment = 1;
#else
#if TARG_BIT_FIELD_CONTAINER_SIZE == TARG_SIZEOF_SHORT
  container_alignment = TARG_ALIGNOF_SHORT;
#else
#if TARG_BIT_FIELD_CONTAINER_SIZE == TARG_SIZEOF_INT
  container_alignment = TARG_ALIGNOF_INT;
#else
#if TARG_BIT_FIELD_CONTAINER_SIZE == TARG_SIZEOF_LONG
  container_alignment = TARG_ALIGNOF_LONG;
#else
error -- TARG_BIT_FIELD_CONTAINER_SIZE in target.h is set wrong.
#endif
#endif
#endif
#endif

#else
#if TARG_BIT_FIELD_CONTAINER_SIZE == 0
  /* Use the smallest integral type into which the field will fit as
     the container.  Try first to find such a type for the current
     position (where the field may start off a byte boundary, and
     may therefore require a larger container than it would if optimally
     aligned). */
  container_size = 0;  /* Meaning not set yet. */
  if (bit_size > 0) {
    if (fits_in_container(1, 1)) {
      /* Char. */
      container_size      = 1;
      container_alignment = 1;
    } else if (fits_in_container(TARG_SIZEOF_SHORT, TARG_ALIGNOF_SHORT)) {
      /* Short. */
      container_size      = TARG_SIZEOF_SHORT;
      container_alignment = TARG_ALIGNOF_SHORT;
    } else if (fits_in_container(TARG_SIZEOF_INT, TARG_ALIGNOF_INT)) {
      /* Int. */
      container_size      = TARG_SIZEOF_INT;
      container_alignment = TARG_ALIGNOF_INT;
    } else if (fits_in_container(TARG_SIZEOF_LONG, TARG_ALIGNOF_LONG)) {
      /* Long. */
      container_size      = TARG_SIZEOF_LONG;
      container_alignment = TARG_ALIGNOF_LONG;
    }  /* if */
  }  /* if */
  if (container_size == 0) {
    /* The field can't be made to fit at the current position, so alignment
       will have to be done.  A smaller container size might now apply,
       since the field will be optimally aligned. */
    container_size = (bit_size + (TARG_CHAR_BIT-1)) / TARG_CHAR_BIT;
    if (container_size <= 1) {
      /* Char. */
      container_size      = 1;
      container_alignment = 1;
    } else if (container_size <= TARG_SIZEOF_SHORT) {
      /* Short. */
      container_size      = TARG_SIZEOF_SHORT;
      container_alignment = TARG_ALIGNOF_SHORT;
    } else if (container_size <= TARG_SIZEOF_INT) {
      /* Int. */
      container_size      = TARG_SIZEOF_INT;
      container_alignment = TARG_ALIGNOF_INT;
    } else if (container_size <= TARG_SIZEOF_LONG) {
      /* Long. */
      container_size      = TARG_SIZEOF_LONG;
      container_alignment = TARG_ALIGNOF_LONG;
#if CHECKING
    } else {
      internal_error("align_offsets_for_bit_field: size is too big");
#endif /* CHECKING */
    }  /* if */
  }  /* if */
#else /* TARG_BIT_FIELD_CONTAINER_SIZE < 0 */
  /* Always use the base type size and alignment. */
  base_type = skip_typerefs(base_type);
  container_size      = base_type->size;
  container_alignment = base_type->alignment;
#endif /* TARG_BIT_FIELD_CONTAINER == 0 */
#endif /* TARG_BIT_FIELD_CONTAINER > 0 */

  /* We want to make sure that the bit field can be grabbed using one
     load of the size of the container aligned the way the container
     must be. */
  if (bit_size == 0 ||
      !fits_in_container(container_size, container_alignment)) {
    /* It can't be, so force alignment. */
    overflow = !do_alignment(byte_offset, bit_offset, container_alignment);
  }  /* if */
  *p_alignment = container_alignment;
  db_exit();
  return !overflow;
}  /* align_offsets_for_bit_field */
                                      

a_boolean set_field_size_and_offset(a_field_ptr      field,
                                    a_targ_size_t    *p_byte_offset,
                                    int              *p_bit_offset,
                                    a_targ_alignment *p_alignment)
/*
field points to a new field of a structure.  So far in the structure, the
byte/bit offsets are as given by *p_byte_offset and *p_bit_offset.  Set the
field's type size and alignment, and update *p_byte_offset and *p_bit_offset.
*p_alignment contains the maximum alignment required so far in the structure,
and is updated if the new field requires a larger alignment value.  If any
overflow was detected in computing the byte or bit offset, FALSE is returned;
if there's no overflow TRUE is returned.
*/
{
  a_type_ptr       field_type;
  a_targ_alignment field_alignment;
  a_boolean	   overflow;
  a_targ_size_t    save_byte_offset;
  int		   save_bit_offset;

  db_enter(4, "set_field_size_and_offset");
  /* Set the size and alignment for the field's type, if necessary. */
  field_type = skip_typerefs(field->type);
  set_type_size(field_type);
  /* Check for a bit-field. */
  if (field->bit_size != 0) {
    /* Do any necessary alignment for a bit-field. */
    overflow = !align_offsets_for_bit_field((int)field->bit_size,
                                            p_byte_offset, p_bit_offset,
					    &field_alignment, field_type);
  } else {
    /* Do any necessary alignment for a normal field. */
    field_alignment = field_type->alignment;
    overflow = !do_alignment(p_byte_offset, p_bit_offset, field_alignment);
  }  /* if */
  if (!overflow) {
    /* Remember the most stringent alignment requirement as the alignment
       requirement for the overall struct. */
    if (field_alignment > *p_alignment) {
      *p_alignment = field_alignment;
    }  /* if */
    /* Save the current byte_offset and bit_offset values.  The bit_offset
       value for the field is not updated until after increment_field_offsets
       is called because the latter performs overflow checking. */
    save_byte_offset = *p_byte_offset;
    save_bit_offset = *p_bit_offset;
    /* Increment the current offsets to account for the field. */
    if (field->bit_size != 0) {
      /* For a bit-field. */
      overflow = !increment_field_offsets(p_byte_offset, p_bit_offset,
                                          (a_targ_size_t)0,
			 		  (int)field->bit_size);
    } else {
      /* For a normal field. */
      overflow = !increment_field_offsets(p_byte_offset, p_bit_offset,
                                          (a_targ_size_t)field_type->size, 0);
    }  /* if */
    if (!overflow) {
      /* Now compute the field's bit offset within the struct.  We know the
         sum will fit in the bit_offset field because increment_field_offsets
         did not report overflow. */
      field->bit_offset = (save_byte_offset * TARG_CHAR_BIT) + save_bit_offset;
    }  /* if */
  }  /* if */
  db_exit();
  return !overflow;
}  /* set_field_size_and_offset */


static a_boolean assignment_operator_for_copy_exists(a_symbol_ptr  sym,
                                                     a_boolean     *const_okay)
/*

Return TRUE if sym is not NULL and qualifies as an assignment operator that
can copy a class object (ARM 12.8).  It qualifies if its first parameter
has a type of "const A&" or "A&", where "A" is the class of which it is a
member.  (The logic also supports a more relaxed (and dubious) reading of
the ARM whereby a first parameter involving type B also qualifies if B is a
base class of A; this interpretation is supported to provide compatibility
with other C++ compilers, which work this way.)  If sym is an overloaded
function, return TRUE if at least one of the functions qualifies.  Set
*const_okay TRUE if a const object can be copied.

*/
{
  a_type_ptr   tp, class_type;
  a_boolean    sym_is_overloaded;
  a_boolean    found_assignment_operator_for_copy = FALSE;

  db_enter(4, "assignment_operator_for_copy_exists");
  *const_okay = FALSE;
  if (sym != NULL) {
    class_type = sym->class_of_which_a_member;
    sym_is_overloaded = (sym->kind == (a_symbol_kind)sk_overloaded_function);
    if (sym_is_overloaded) sym = sym->variant.overloaded_function.symbols;
    /* Loop through the one or more symbols looking for one with the right
       argument type. */
    for (; sym != NULL; sym = sym_is_overloaded ? sym->next : NULL) {
      tp = skip_typerefs(routine_symbol_type(sym)->
                            variant.routine.extra_info->param_type_list->type);
      /* We are looking for a reference to the current class. */
      if (is_reference_type(tp)) {
        tp = type_pointed_to(tp);
        /* Look for an exact match between tp and either the parent class or
           a base class of the parent class.  (A strict reading of the ARM
           seems to disallow the base class match.) */
        if (is_class_struct_union_type(tp) &&
            is_same_class_or_base_class_thereof(class_type, tp)) {
          /* Found it. */
          found_assignment_operator_for_copy = TRUE;
          /* Now see if it a const qualified object can be copied.  If not
             keep looping in case there's another that accepts a const
             object. */
          if (is_const_qualified_type(tp)) {
            *const_okay = TRUE;
            break;
          }  /* if */
        }  /* if */
      } else if (is_class_struct_union_type(tp) &&
                 is_same_class_or_base_class_thereof(class_type, tp)) {
        /* The argument is not the class object by reference but rather
           the class object by value.  We accept this, but it presents a
           special set of problems. */
        a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(tp);
        found_assignment_operator_for_copy = TRUE;
        /* Since passing a class object by value involves a copy constructor
           call if a copy constructor exists, the logic for setting *const_okay
           is more complicated. */
        if (cssp->constructor == NULL) {
          /* No constructor exists. */
          *const_okay = TRUE;
        } else if (cssp->has_copy_constructor_for_const_object) {
          /* A copy constructor exists that can copy an object without
             modifying it. */
          *const_okay = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  db_exit();
  return found_assignment_operator_for_copy;
}  /* assignment_operator_for_copy_exists */


static a_boolean is_valid_union_field(a_type_ptr        field_type,
                                      a_source_position *pos)
/*
Nonstatic data members of a union may not be objects with a constructor,
a destructor, or a user-defined assignment operator.  Return FALSE if any
such member functions are present.
*/
{
  a_boolean                      is_valid = TRUE;
  a_type_ptr                     tp = skip_typerefs(field_type);
  a_class_symbol_supplement_ptr  cssp;
  a_symbol_ptr                   sym;
  a_boolean                      is_overloaded;

  db_enter(4, "is_valid_union_field");
  if (is_array_type(tp)) tp = skip_typerefs(underlying_array_element_type(tp));
  if (is_class_struct_union_type(tp)) {
    cssp = symbol_supplement_for_class(tp);
    if (cssp->constructor != NULL || cssp->destructor != NULL) {
      is_valid = FALSE;
    } else {
      /* Check for existence of a user defined assignment operator function.
         If there is no compiler-generated assignment operator, then it must
         be user-defined. */
      sym = cssp->assignment_operator;
      if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
        is_overloaded = TRUE;
        sym = sym->variant.overloaded_function.symbols;
      } else {
        is_overloaded = FALSE;
      }  /* if */
      for (; sym != NULL; sym = (is_overloaded ? sym->next : NULL)) {
        if (sym->variant.routine->compiler_generated) break;
      }  /* if */
      if (sym == NULL) {
#if CHECKING
        /* Confirm that there is a default assignment operator. */
        a_boolean  dummy_flag;
        if (!assignment_operator_for_copy_exists(cssp->assignment_operator,
                                                 &dummy_flag)) {
          internal_error("is_valid_union_field: missing default operator=");
        }  /* if */
#endif /* CHECKING */
        /* No compiler-generated default assignment operator was found, and
           there must be one, so it must be user-defined. */
        is_valid = FALSE;
      }  /* if */
    }  /* if */
    if (!is_valid) {
      pos_st_error(ec_bad_union_field, pos, tp->source_corresp.name);
    }  /* if */
  }  /* if */

  db_exit();
  return is_valid;
}  /* is_valid_union_field */


void check_anonymous_union_symbols(a_type_ptr     class_type,
                                   a_field_ptr    assoc_field_object,
                                   a_variable_ptr assoc_var_object)
/*
Do processing for an anonymous union that is declared within a class (when
class_type is non-NULL) or outside a class (when class_type is NULL).
Specifically, make a pass over all the members of the anonymous union, do
error checking, and promote each field from the anonymous union to its
containing scope.  When class_type is non-NULL, the containing scope is a
class, and the anonymous union as a whole is represented as a field of that
class (assoc_field_object).  When class_type is NULL, the containing scope
is the file scope, a routine scope, or a block scope, and the anonymous
union as a whole is represented as a variable (assoc_var_object).  Only one
of assoc_field_object and assoc_var_object is defined.
*/
{
  a_symbol_ptr                   sym, next_sym, mf_sym;
  a_class_symbol_supplement_ptr  cssp;
  a_class_type_supplement_ptr    ctsp;
  an_access_specifier            access;
  a_boolean                      access_error_already_issued = FALSE;
  a_boolean                      member_function_error_already_issued = FALSE;
  a_boolean                      is_overloaded;
  a_type_ptr                     object_type;

  db_enter(4, "check_anonymous_union_symbols");
  object_type = (class_type == NULL) ? assoc_var_object->type :
                                       assoc_field_object->type;
  ctsp = object_type->variant.class_struct_union.extra_info;
  if (class_type == NULL) {
    ctsp->anonymous_union_kind = (an_anonymous_union_kind)auk_variable;
  } else {
    ctsp->anonymous_union_kind = (an_anonymous_union_kind)auk_field;
    ctsp->anonymous_union_field = assoc_field_object;
  }  /* if */
  /* The symbols list for the anonymous union will be eliminated.  Its
     field symbols are promoted to the scope of the containing class. */
  cssp = symbol_supplement_for_class(object_type);
  sym = cssp->symbols;
  cssp->symbols = NULL;
  /* Go through each of the symbols on the list. */
  for (; sym != NULL; sym = next_sym) {
    next_sym = sym->next_in_scope;
    sym->next_in_scope = NULL;
    /* Private and protected members are not allowed in an anonymous union
       (ARM 9.5). */
    access = access_for_symbol(sym);
    if (access == (an_access_specifier)as_private ||
        access == (an_access_specifier)as_protected) {
      if (!access_error_already_issued) {
        error(ec_anon_union_member_access);
        access_error_already_issued = TRUE;
      }  /* if */
    }  /* if */
    if (sym->kind == (a_symbol_kind)sk_field) {
      /* Unlink the symbol from the inactive list and link it back into the
         symbol table in the current scope. */
      sym->class_of_which_a_member = class_type;
      remove_from_inactive_symbols_list(sym);
      reenter_symbol(sym, decl_scope_level, /*suppress_error=*/FALSE);
      /* Update the IL. */
      if (class_type == NULL) {
        sym->variant.field.anonymous_union_variable = assoc_var_object;
      }  /* if */
    } else if (is_member_function_symbol(sym)) {
      /* This may be a compiler generated default assignment operator, which
         is okay.  Any user-defined member function is illegal. */
      if (!member_function_error_already_issued) {
        if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
          mf_sym = sym->variant.overloaded_function.symbols;
          is_overloaded = TRUE;
        } else {
          mf_sym = sym;
          is_overloaded = FALSE;
        }  /* if */
        for (; mf_sym != NULL; mf_sym = is_overloaded ? mf_sym->next : NULL) {
          if (!mf_sym->variant.routine->compiler_generated) {
            error(ec_anon_union_member_function);
            member_function_error_already_issued = TRUE;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    } else {
      /* A member that is a type?  Ignore it. */
    }  /* if */
  }  /* for */
  db_exit();
}  /* check_anonymous_union_symbols */


static void decl_nonstatic_data_member(a_symbol_locator    *locator,
                                       a_type_ptr          class_type,
                                       a_type_ptr          member_type,
                                       an_access_specifier access,
				       a_boolean	   unnamed_field,
				       a_boolean	   is_anonymous_union,
                                       a_targ_size_t       *p_byte_offset,
                                       int                 *p_bit_offset,
                                       a_targ_alignment    *p_alignment,
                                       a_field_ptr         *end_of_list,
				       a_boolean	   *any_overflow)
/*
Scan a nonstatic data member of a class, struct, or union, create a field
entry to represent it in the IL, and create an entry in the symbol table
for it if it has a name.  class_type is a pointer to the tk_class,
tk_struct, or tk_union type entry for the entity of which the member is a
member.  *locator and member_type describe what is so far known about the
member, and access specifies whether it is a public, protected, or private
member.  It it is unnamed, unnamed_field will be TRUE.  The field entry
that is created is added to the end of a list in a structure pointed to by
field_list; this list will be transferred to the list on the class_type
later.  If the field is too large to fit in the struct and *any_overflow
is FALSE, an error is issued and *any_overflow is set to TRUE; this
technique is used to assure that only one such error is put out on a given
class, struct, or union.
*/
{
  a_targ_size_t                  local_byte_offset;
  int                            local_bit_offset;
  long                           bit_field_size = 0;
  a_field_ptr		         field;
  a_symbol_ptr		         member_sym = NULL;
  a_class_symbol_supplement_ptr  cssp;

  db_enter(3, "decl_nonstatic_data_member");
  if (class_type->kind == (a_type_kind)tk_union) {
    /* All fields in a union have offset zero. */
    local_byte_offset = 0;
    local_bit_offset = 0;
    if (C_dialect == C_dialect_cplusplus) {
      /* An object of a class with a constructor, a destructor, or a user-
         defined assignment operator cannot be a member of a union. */
      if (!is_valid_union_field(member_type, &locator->source_position)) {
        member_type = error_type();
      }  /* if */
    }  /* if */
  } else {
    local_byte_offset = *p_byte_offset;
    local_bit_offset = *p_bit_offset;
  }  /* if */
  /* A colon next indicates a bit-field. */
  if (curr_token == tok_colon) {
    /* Scan the bit-field size and determine the bit-field type. */
    scan_bit_field_size(unnamed_field, &member_type, &bit_field_size);
  }  /* if */
  /* Create the field entry.  For unnamed fields it will not actually become
     part of the IL. */
  field = alloc_field();
  field->type = member_type;
  field->bit_size = bit_field_size;
  /* For an unnamed field, do not create the field symbol. */
  if (!unnamed_field) {
    if (!is_anonymous_union) {
      /* Create the field symbol. */
      member_sym = enter_local_symbol((a_symbol_kind)sk_field, locator,
                                      depth_scope_stack,
                                      /*suppress_redecl_error=*/FALSE);
      member_sym->class_of_which_a_member = class_type;
      member_sym->variant.field.ptr = field;
      member_sym->defined = TRUE;
      set_source_corresp(&(field->source_corresp), member_sym);
    }  /* if */
    field->source_corresp.class_of_which_a_member = class_type;
    field->source_corresp.access = access;
    /* Add the field to the temporary list for this class/struct/union. */
    if (*end_of_list == NULL) {
      class_type->variant.class_struct_union.field_list = field;
    } else {
      (*end_of_list)->next = field;
    }  /* if */
    *end_of_list = field;
  }  /* if */
#if TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE
  /* Fields are allocated in the class object in exactly the same order as
     their declaration. */
#else
  /* Fields are allocated in groups based on access.  Only public fields are
     allocated at this time.  The rest are handled after all the fields have
     been seen. */
  if (access == (an_access_specifier)as_public) {
#endif /* !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE */
    if (!set_field_size_and_offset(field, &local_byte_offset,
                                   &local_bit_offset, p_alignment)) {
      /* FALSE was returned, which means an overflow error was encountered in
	 computing the new size of the struct -- i.e., this field will not
	 fit.  Remember it, so that only one such error is put out. */
      if (!*any_overflow) {
        error(ec_struct_too_large);
	*any_overflow = TRUE;
      }  /* if */
    } else {
      /* Offset values were modified.  Save highest offset for unions, last
         offset for structs and classes, for use in establishing the size of
         the overall aggregate. */
      if (class_type->kind != (a_type_kind)tk_union ||
          local_byte_offset > *p_byte_offset ||
          (local_byte_offset == *p_byte_offset &&
           local_bit_offset > *p_bit_offset)) {
        *p_byte_offset = local_byte_offset;
        *p_bit_offset = local_bit_offset;
      }  /* if */
    }  /* if */
#if !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE
  } else {
#if CHECKING
    /* No access other than public should be possible in non-C++ modes. */
    if (C_dialect != C_dialect_cplusplus) {
      internal_error("decl_nonstatic_data_member: unexpected access");
    }  /* if */
#endif /* CHECKING */
    if (unnamed_field) {
      /* Unnamed fields that have not been allocated yet should be added to the
         field list so they can be allocated later. */
      if (*end_of_list == NULL) {
        class_type->variant.class_struct_union.field_list = field;
      } else {
        (*end_of_list)->next = field;
      }  /* if */
      *end_of_list = field;
    }  /* if */
  }  /* if */
#endif /* !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE */
  /* Remember if any member of the class, struct, or union is const-
     qualified, including recursively the members of any contained
     classes, structs, or unions.  This is useful for determination of
     modifiable lvalues (see 3.2.2.1). */
  if (type_or_element_type_is_const_qualified(member_type) ||
      (is_class_struct_union_type(member_type) &&
       skip_typerefs(member_type)->
                            variant.class_struct_union.any_const_member)) {
    class_type->variant.class_struct_union.any_const_member = TRUE;
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    /* In C++ we also need to keep track of whether any members have
       reference type. */
    cssp = symbol_supplement_for_class(class_type);
    if (is_reference_type(member_type)) {
      cssp->any_ref_member = TRUE;
    }  /* if */
  }  /* if */
  if (is_aggregate_or_union_type(member_type)) {
    /* If the member's type is class, struct, or union -- or array of class,
       struct, or union -- there is additional checking to be done. */
    a_type_ptr  tp = skip_typerefs(member_type);
    if (is_array_type(tp)) {
      tp = skip_typerefs(underlying_array_element_type(tp));
    }  /* if */
    if (is_class_struct_union_type(tp)) {
      /* If the member type has const-qualified fields, propagate the flag
         to the parent type. */
      if (tp->variant.class_struct_union.any_const_member) {
        class_type->variant.class_struct_union.any_const_member = TRUE;
      }  /* if */
      if (C_dialect == C_dialect_cplusplus) {
        a_class_symbol_supplement_ptr  member_cssp;

        member_cssp = symbol_supplement_for_class(tp);
        /* If the member type has any members of ref type, propagate the
           flag to the parent type. */
        if (member_cssp->any_ref_member) cssp->any_ref_member = TRUE;
        if (is_anonymous_union) {
          /* Constructor and destructor are not allowed, but other checking
             is required. */
          check_anonymous_union_symbols(class_type, field,
                                        (a_variable_ptr)NULL);
        } else {
          /* If a nonstatic data member of a class is itself a class object
             (or an array whose elements are class objects) and the subobject
             has a constructor and/or destructor, the containing class itself
             is required to have a constructor and/or destructor.  Do the
             check at this time, and record the requirement, if any. */
          if (member_cssp->constructor != NULL) {
            cssp->constructor_required = TRUE;
          }  /* if */
          if (member_cssp->destructor != NULL) {
            cssp->destructor_required = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    if (member_sym != NULL) {
      db_symbol(member_sym, "", 2);
    }  /* if */
    fprintf(f_debug, "final byte offset = %lu", *p_byte_offset);
    if (*p_bit_offset > 0) {
      fprintf(f_debug, ", final bit offset = %d", *p_bit_offset);
    }  /* if */
    fprintf(f_debug, ", max alignment = %d\n", (int)*p_alignment);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_nonstatic_data_member */


static void set_offsets_for_nonvirtual_base_classes(a_type_ptr class_type,
                                                    a_boolean  *any_overflow)
/*
Lay out the class_type object to store the nonvirtual direct base classes.
(They will precede the fields of the current class.)  Do this by going
through the base classes list for the current class and reserving enough
space for each base class that is directly and nonvirtually inherited;
storage is reserved in the order in which the base classes appear.
(Virtual base classes, both direct and indirect, are dealt with after the
fields of the current class are allocated; indirect nonvirtual base classes
are just subobjects of the direct nonvirtual base classes.)  If *any_overflow
is TRUE upon entry, nothing is done; if overflow is detected in computing
offsets and alignments, *any_overflow is set and returned to the caller.
*/
{
  a_targ_size_t         	byte_offset, size;
  int                   	bit_offset;
  a_targ_alignment		overall_alignment, alignment;
  a_base_class_ptr              bcp;
  
  db_enter(4, "set_offsets_for_nonvirtual_base_classes");
  /* Traverse the list of base classes. */
  bcp = class_type->variant.class_struct_union.extra_info->base_classes;
  if (bcp != NULL) {
    overall_alignment = class_type->alignment;
    byte_offset = class_type->size;
    bit_offset = 0;
    for (; bcp != NULL; bcp = bcp->next) {
      if (!*any_overflow && bcp->direct) {
        /* Note that virtual and nonvirtual base classes are handled
           differently. */
        if (bcp->is_virtual) {
          /* For virtual base classes we only reserve enough space for a
             pointer to the actual data section.  The latter is added at the
             end of the storage. */
#if TARG_ALL_POINTERS_SAME_SIZE
          /* All pointers are the same size. */
          alignment = (a_targ_alignment)TARG_ALIGNOF_POINTER;
          size = (a_targ_size_t)TARG_SIZEOF_POINTER;
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
??=error set_offsets_for_nonvirtual_base_clases: different sized pointers.
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
        } else {
          /* For a nonvirtual base classes reserve space for all the base class
             fields, excluding space required for its own virtual base classes.
             The latter will be added at the end of the storage. */
          alignment = bcp->type->variant.class_struct_union.extra_info->
                                      alignment_without_virtual_base_classes;
          size = bcp->type->variant.class_struct_union.extra_info->
                                      size_without_virtual_base_classes;
        }  /* if */
        /* Adjust the current offset to ensure that the base class is
           properly aligned. */
        if (!do_alignment(&byte_offset, &bit_offset, alignment)) {
          error(ec_struct_too_large);
          *any_overflow = TRUE;
        } else {
          /* No error, so increment the offset to allow for the base class. */
          if (bcp->is_virtual) {
            bcp->pointer_offset = byte_offset;
          } else {
            bcp->offset = byte_offset;
          }  /* if */
          if (!increment_field_offsets(&byte_offset, &bit_offset, size, 0)) {
            error(ec_struct_too_large);
            *any_overflow = TRUE;
          }  /* if */
          /* Adjust the overall alignment requirement for the current class, if
             necessary. */
          if (overall_alignment < alignment) {
            overall_alignment = alignment;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    /* Store the size and alignment. */
    class_type->size = byte_offset;
    class_type->alignment = overall_alignment;
  }  /* if */
  db_exit();
}  /* set_offsets_for_nonvirtual_base_classes */


static void set_offset_for_virtual_function_info(a_type_ptr     class_type,
                                                 a_targ_size_t  *p_byte_offset,
                                                 int            *p_bit_offset,
                                                 a_targ_alignment *p_alignment,
                                                 a_boolean       *any_overflow)
/*
In C++ classes with virtual functions provide a special mechanism for
dynamic function binding.  Typically, this is a virtual function table,
and each object of the class contains a pointer to the table.  For each
class with virtual functions this routine allocates a field to contain
such a pointer -- or other data as required by a given implementation.
The size and alignment of such a field are defined by constants that can
be redefined for various implementation strategies.  If *any_overflow
is TRUE upon entry, nothing is done; if overflow is detected in computing
offsets and alignments, *any_overflow is set and returned to the caller.
*/
{
  a_class_type_supplement_ptr	ctsp;

  db_enter(4, "set_offset_for_virtual_function_info");
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (!*any_overflow && ctsp->virtual_function_count > 0) {
    if (!do_alignment(p_byte_offset, p_bit_offset,
                      TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO)) {
      error(ec_struct_too_large);
      *any_overflow = TRUE;
    } else {
      ctsp->virtual_function_info_offset = *p_byte_offset;
      if (*p_alignment < TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO) {
        *p_alignment = TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO;
      }  /* if */
      if (!increment_field_offsets(
                        p_byte_offset, p_bit_offset,
                        (a_targ_size_t)TARG_SIZEOF_VIRTUAL_FUNCTION_INFO, 0)) {
        error(ec_struct_too_large);
        *any_overflow = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* set_offset_for_virtual_function_info */


static void set_offsets_for_virtual_base_classes(a_type_ptr     class_type,
                                                 a_targ_size_t  *p_byte_offset,
                                                 int            *p_bit_offset,
                                                 a_targ_alignment *p_alignment,
                                                 a_boolean       *any_overflow)
/*
Reserve space at the end of the class object for virtual base classes.
*p_byte_offset and *p_bit_offset represent the space already reserved; they
are updated to reflect the increase in the size of the object when the
virtual base classes are added.  *p_alignment is the overall alignment
required for the object -- i.e., the maximum alignment required for any of
its components; it too is updated if the alignment required for one of the
virtual subobjects exceeds the current maximum.  *any_overflow is TRUE if
the object has already been reported to be too large.
*/
{
  a_targ_size_t         	size;
  a_targ_alignment		alignment;
  a_class_type_supplement_ptr	ctsp;
  a_base_class_ptr              bcp;
  
  db_enter(4, "set_offsets_for_virtual_base_classes");

  ctsp = class_type->variant.class_struct_union.extra_info;
  /* Record the size and alignment of the class before space is added for
     virtual base classes. */
  /* The size will never be set to zero, even if the class's actual size
     is zero.  This is to assure that it will be given a unique location
     when incorporated as a subobject of some other class. */
  if (*p_byte_offset == 0) *p_byte_offset = 1;
  /* We add padding (if needed) to the part of the object preceding
     the virtual base classes based on the alignment requirements for
     the portion of the class processed thus far.  In odd cases this
     can result in suboptimal packing, but it permits treating the
     class-without-virtual-base-classes as a subobject whose size is
     consistent with its alignment. */
  if (!do_alignment(p_byte_offset, p_bit_offset, *p_alignment)) {
    if (!*any_overflow) {
      error(ec_struct_too_large);
      *any_overflow = TRUE;
    }  /* if */
  }  /* if */
  ctsp->size_without_virtual_base_classes = *p_byte_offset;
  ctsp->alignment_without_virtual_base_classes = *p_alignment;
  if (class_type->variant.class_struct_union.any_virtual_base_classes &&
      !*any_overflow) {
    bcp = ctsp->base_classes;
    if (bcp != NULL) {
      /* Now add the virtual base classes to the storage.  This is done
         almost exactly as for nonvirtual base classes. */
      for (; bcp != NULL; bcp = bcp->next) {
        if (bcp->is_virtual) {
          alignment = bcp->type->variant.class_struct_union.extra_info->
                                        alignment_without_virtual_base_classes;
          size = bcp->type->variant.class_struct_union.extra_info->
                                        size_without_virtual_base_classes;
          if (!do_alignment(p_byte_offset, p_bit_offset, alignment)) {
            error(ec_struct_too_large);
            *any_overflow = TRUE;
            break;
          } else {
            /* Record the current offset in the data_section_offset of the
               virtual base class entry.  This allows for direct access of
               its fields (rather than through a pointer) as an optimization
               under certain circumstances. */
            bcp->offset = *p_byte_offset;
            if (*p_alignment < alignment) {
              *p_alignment = alignment;
            }  /* if */
            if (!increment_field_offsets(p_byte_offset, p_bit_offset,
                                         size, 0)) {
              error(ec_struct_too_large);
              *any_overflow = TRUE;
              break;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  db_exit();
}  /* set_offsets_for_virtual_base_classes */


#if !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE
static void set_offsets_for_remaining_fields(a_type_ptr       class_type,
                                             a_targ_size_t    *p_byte_offset,
                                             int              *p_bit_offset,
                                             a_targ_alignment *p_alignment,
                                             a_boolean        *any_overflow)
/*
Set the sizes and offsets of protected and private fields that were not
processed in decl_nonstatic_data_member.  This processing is required only
if fields are grouped by access before being allocated.  The public fields
will already have been done.
*/
{
  a_field_ptr          field, prev_field, next_field;
  a_targ_size_t        local_byte_offset;
  int                  local_bit_offset, count;
  an_access_specifier  access;

  db_enter(4, "set_offsets_for_remaining_fields");
  /* Make two passes over the field list, one for protected fields and the
     other for private fields. */
  for (count = 1; count >= 0; count--) {
    access = count ? (an_access_specifier)as_protected :
                     (an_access_specifier)as_private;
    prev_field = NULL;
    /* Traverse the field list. */
    for (field = class_type->variant.class_struct_union.field_list;
         field != NULL;
         field = next_field) {
      /* Save a pointer to the next field, since field may be removed. */
      next_field = field->next;
      if (field->source_corresp.access == access) {
#if CHECKING
        if (field->bit_offset != 0) {
          internal_error(
                 "set_offsets_for_remaining_fields: field already allocated");
        }  /* if */
#endif
        /* This field has the access specification for which allocation is
           now being done.  Note that the code that follows is based on
           decl_nonstatic_data_member. */
        if (class_type->kind == (a_type_kind)tk_union) {
          /* All fields in a union have offset zero. */
          local_byte_offset = 0;
          local_bit_offset = 0;
        } else {
          local_byte_offset = *p_byte_offset;
          local_bit_offset = *p_bit_offset;
        }  /* if */
        if (!set_field_size_and_offset(field, &local_byte_offset,
                                       &local_bit_offset, p_alignment)) {
          /* Overflow error. */
          if (!*any_overflow) {
            error(ec_struct_too_large);
            *any_overflow = TRUE;
          }  /* if */
        }  /* if */
        /* Offset values were modified.  Save highest offset for unions, last
           offset for structs and classes, for use in establishing the size of
           the overall aggregate. */
        if (class_type->kind != (a_type_kind)tk_union ||
            local_byte_offset > *p_byte_offset ||
            (local_byte_offset == *p_byte_offset &&
             local_bit_offset > *p_bit_offset)) {
          *p_byte_offset = local_byte_offset;
          *p_bit_offset = local_bit_offset;
        }  /* if */
        if (field->source_corresp.assoc_info == NULL) {
          /* No symbol created for this field, so it must be unnamed.  Remove
             it from the field list. */
          if (prev_field == NULL) {
            class_type->variant.class_struct_union.field_list = next_field;
          } else {
            prev_field->next = next_field;
          }  /* if */
          field = NULL;
        }  /* if */
      }  /* if */
      /* If the current field was not removed from the field list, remember it
         for the next iteration of the loop. */
      if (field != NULL) prev_field = field;
    }  /* for */
  }  /* for */
  db_exit();
}  /* set_offsets_for_remaining_fields */
#endif /* !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE */


static void set_base_class_offsets(a_base_class_ptr       proximate_derivation,
                                   a_base_class_ptr       list_to_search,
                                   a_derivation_step_ptr  path,
                                   a_derivation_step_ptr  end_of_path)
/*
The offset of base class proximate_derivation has been computed, but the
offsets of its own base classes have not.  The confusing part of this
processing is that two different base class entries are involved.  First,
the "most derived class" contains a list of all its base classes, direct
and indirect.  But each class from which it is derived has its own base
class list, too.  For example:

          A    A       class B points to base class A (path ==>A)
          |    |
          B    C       class C points to base class A (path ==>A)
           \  /
             D         class D points to base classes B (path ==>B)
                                                      A (path ==>B==>A)
                                                      C (path ==>C)
                                                      A (path ==>C==>A)

Thus, while the base classes for D, the "most derived class", are
represented by only 3 type entries (A, B, and C), there are 4 base class
entries involved, since 2 are associated with class A.  As for offsets, B's
"A" base class entry indicates the offset of the "A" data section within
the block occupied by class "B" entities, whereas the two "A" base classes
associated with class D should have offsets representing their locations
within a "D" object.  Computing the offset of an indirect base class (e.g.,
A) within a most derived class (e.g., D) requires adding the offset of the
base class immediately derived from it (e.g., B) to its own offset within
that class (e.g., A's offset within B); in other words, the D::A offset
equals the D::B offset plus the B::A offset.

Suppose that proximate_derivation is the D::B base class entry.
list_to_search includes all 4 base classes from the most derived class.
path will be simply ==>B, and end_of_path will be the same.  The algorithm
involves going through B's direct base classes (there's only one in this
case), finding the corresponding base class entry on list_to_search, and
setting the offset field in the latter.
*/
{
  a_base_class_ptr  ref_bcp, bcp;

  db_enter(4, "set_base_class_offsets");
  /* Get the first "reference" base class of the root class, which is itself
     a base class of the most derived class.  It is called a reference base
     class because it contains an offset relative to the root base class.
     It is the offset value relative to the most derived class that we need
     to determine and record. */
  ref_bcp = proximate_derivation->type->
                variant.class_struct_union.extra_info->base_classes;
#if DEBUG
  if (debug_level >= 4) {
    if (ref_bcp != NULL) {
      fputs("root base class ", f_debug);
      db_name(&proximate_derivation->type->source_corresp);
      fprintf(f_debug, " at offset %ld\n", proximate_derivation->offset);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Loop through the reference base classes, the direct base classes of
     the proximate_derivation base class. */
  for (; ref_bcp != NULL; ref_bcp = ref_bcp->next) {
    if (ref_bcp->direct) {
#if DEBUG
      if (debug_level >= 4) {
        fputs("reference base class ", f_debug);
        db_name(&ref_bcp->type->source_corresp);
        fprintf(f_debug, " at offset %ld\n", ref_bcp->offset);
      }  /* if */
#endif /* DEBUG */
      /* Look for a match in the base classes list of the most derived
         class. */
      for (bcp = list_to_search; bcp != NULL; bcp = bcp->next) {
        /* Look for an indirect base class that is of the same type. */
        if (!bcp->direct && bcp->type == ref_bcp->type &&
            bcp->is_virtual == ref_bcp->is_virtual) {
          /* We seem to have found a match, but we have to check the path
             as well as the type (e.g., to distinguish B::A from C::A in the
             example above. */
          end_of_path->next = make_derivation_step(bcp,
                                                  (a_derivation_step_ptr)NULL);
          if (congruent_paths(path, bcp->derivation)) {
            /* It is a match.  If this is a virtual base class, it is the
               pointer_offset field that needs to be updated. */
            if (!bcp->is_virtual) {
              bcp->offset = proximate_derivation->offset + ref_bcp->offset;
            } else {
              bcp->pointer_offset = proximate_derivation->offset +
                                    ref_bcp->pointer_offset;
            }  /* if */
#if DEBUG
            if (debug_level >= 4) {
              fputs("base class ", f_debug);
              db_name(&bcp->type->source_corresp);
              fprintf(f_debug, ": setting %soffset to %ld\n",
                               bcp->is_virtual ? "pointer " : "",
                               bcp->is_virtual ? bcp->pointer_offset :
                                                 bcp->offset);
            }  /* if */
#endif /* DEBUG */
            /* Make a recursive call to apply this processing to the next
               level of base classes. */
            set_base_class_offsets(bcp, list_to_search,
                                   path, end_of_path->next);
          }  /* if */
          free_derivation_step(end_of_path->next);
          end_of_path->next = NULL;
          break;
        }  /* if */
      }  /* for */
#if CHECKING
      if (bcp == NULL && !ref_bcp->is_virtual) {
        /* Normal exit from loop means no match was found.  This is only
           possible when we're dealing with virtual base classes. */
        internal_error(
                 "set_base_class_offsets: no base class matches reference");
      }  /* if */
#endif /* CHECKING */
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_base_class_offsets */


static void set_offsets_for_indirect_base_classes(a_type_ptr  class_type)
/*
Compute the offsets from the start of the object described by class_type
of each of its indirect base classes.  The direct base classes and the
data sections of all virtual base classes, direct or indirect, have already
been handled.  The processing of this routine and its subroutines is
addressed to indirect base classes.
*/
{
  a_base_class_ptr      bcp, bcp_list;
  a_derivation_step_ptr end_of_path;

  db_enter(4, "set_offsets_for_indirect_base_classes");
  bcp_list = class_type->variant.class_struct_union.extra_info->base_classes;
#if DEBUG
  if (debug_level >= 4) {
    if (bcp_list != NULL) {
      fputs("before setting offsets: ", f_debug);
      db_base_class_list(class_type);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  /* Loop through the list of base classes, direct and indirect, that are
     defined for the class, but ignore all but the direct base classes.  The
     rest are handled by recursively scanning the base class tree. */
  for (bcp = bcp_list; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct) {
      end_of_path = bcp->derivation;
      /* We need to search for the end of the path (rather than assume, as
         is generally the case, that direct base classes have one-step
         derivations, to cover the case of virtual base classes that are
         more accessible along an indirect than the direct path. */
      while (end_of_path->next != NULL) end_of_path = end_of_path->next;
      set_base_class_offsets(bcp, bcp_list, bcp->derivation, end_of_path);
    }  /* if */
  }  /* for */
  db_exit();
}  /* set_offsets_for_indirect_base_classes */


static void finish_laying_out_class(a_type_ptr       class_type,
                                    a_targ_size_t    byte_offset,
                                    int              bit_offset,
                                    a_targ_alignment alignment,
                                    a_boolean        any_overflow)
/*
Complete laying out the object specified by class_type.  This includes
allocating any remaining fields whose allocation may have been delayed and
making room for virtual base classes, which appear at the end of the layout.
*/
{
  db_enter(3, "finish_laying_out_class");
  if (C_dialect == C_dialect_cplusplus) {
#if !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE
    /* Only public fields were given an offset in decl_nonstatic_data_member.
       Now that all the fields have been seen and added to the class's field
       list, traverse the field list again and allocate protected and private
       fields. */
    set_offsets_for_remaining_fields(class_type, &byte_offset, &bit_offset,
                                     &alignment, &any_overflow);
#endif /* !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE */
    set_offset_for_virtual_function_info(class_type, &byte_offset, &bit_offset,
                                         &alignment, &any_overflow);
    set_offsets_for_virtual_base_classes(class_type, &byte_offset, &bit_offset,
                                         &alignment, &any_overflow);
  }  /* if */
  if (!do_alignment(&byte_offset, &bit_offset, alignment)) {
    if (!any_overflow) error(ec_struct_too_large);
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    set_offsets_for_indirect_base_classes(class_type);
  }  /* if */
  class_type->size = byte_offset;
  class_type->alignment = alignment;
  /* Avoid a zero-sized structure (as in "struct {int : 0;}" for C and in
     "class {}" for C++). */
  if (class_type->size == 0) class_type->size = 1;
#if DEBUG
  if (debug_level >= 3) {
    if (C_dialect == C_dialect_cplusplus) db_base_class_list(class_type);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* finish_laying_out_class */


static void generate_special_function(a_type_ptr               class_type,
                                      a_param_type_ptr         ptp,
                                      a_special_function_kind  sfkind)
/*
Create a routine entry for a compiler generated constructor, destructor, or
assignment operator.  The created routine is a member function of the class
specified by class_type.  If it has any parameter besides the implicit
"this" parameter (i.e., for a copy constructor or assignment operator), a
non-NULL param type pointer is passed in as ptp.  sfkind indicates whether
a constructor, destructor, or assignment operator should be created.  No
routine body is generated at this time.
*/
{
  a_type_ptr                rout_type;
  a_symbol_ptr              rout_sym;
  a_routine_type_supplement *extra_info;
  a_symbol_locator          locator;

  db_enter(3, "generate_special_function");
  /* Allocate and initialize the routine type entry for the function. */
  rout_type = alloc_type((a_type_kind)tk_routine);
  if (sfkind == (a_special_function_kind)sfk_destructor) {
    /* Destructors are given a return type of void. */
    rout_type->variant.routine.return_type = void_type();
  } else {
    /* Constructors and default assignment operators are given a return type
       of reference to class-type. */
    rout_type->variant.routine.return_type = make_reference_type(class_type);
  }  /* if */
  extra_info = rout_type->variant.routine.extra_info;
  extra_info->param_type_list = ptp;
  extra_info->implicit_this_param_type =
           make_qualified_type(make_pointer_type(class_type),
                               /*is_const=*/TRUE, /*is_volatile=*/FALSE);
  extra_info->prototyped = TRUE;
  /* Check whether the routine needs special support for returning a class
     object by value.  This call should be superfluous; it is included just
     to be safe, in case the rules change on when the flag needs to be set. */
  set_routine_calling_method_flag(rout_type);
  if (sfkind != (a_special_function_kind)sfk_operator) {
    extra_info->constructor_or_destructor = TRUE;
  }  /* if */
  /* Create a locator for the symbol that will be created. */
  if (sfkind == (a_special_function_kind)sfk_operator) {
    make_opname_locator(tok_assign, (an_opname_kind)onk_assign, &locator,
                        &class_type->source_corresp.decl_position);
  } else {
    make_locator_for_symbol(
                        (a_symbol_ptr)class_type->source_corresp.assoc_info,
                        &locator);
    if (sfkind == (a_special_function_kind)sfk_constructor) {
      change_class_locator_into_constructor_locator(&locator);
    } else {
      tildize_locator(&locator);
    }  /* if */
  }  /* if */
  /* Create a symbol and enter it in the symbol table, and create a routine
     entry and add it to the routines list for the current scope. */
  rout_sym = decl_member_function(&locator, class_type, rout_type,
                                  (an_access_specifier)as_public,
                                  /*is_inline=*/TRUE, /*is_virtual=*/FALSE,
                                  sfkind);
  rout_sym->variant.routine->compiler_generated = TRUE;
  db_exit();
}  /* generate_special_function */



static void make_default_constructor_body(a_scope_ptr  scope)
/*
Create the body for a default constructor or a default copy constructor.  It
will return a pointer to the constructed object.
*/
{
  a_statement_ptr                sp;
  a_routine_type_supplement_ptr  rtsp;
  a_variable_ptr                 vp;
  a_param_type_ptr               ptp;

  db_enter(4, "make_default_constructor_body");
  /* Create the parameter variable -- needed for copy constructors only. */
  rtsp = (skip_typerefs(scope->variant.routine.ptr->type))->
                                                 variant.routine.extra_info;
  ptp = rtsp->param_type_list;
  if (ptp != NULL) {
    vp = make_parameter(ptp->type, (a_storage_class)sc_auto,
                        (a_symbol_ptr)NULL);
    vp->assoc_param_type = ptp;
  }  /* if */    
  /* Create an statement block that is empty except for the return
     statement. */
  scope->assoc_block = alloc_statement((a_statement_kind)stmk_block);
  scope->assoc_block->variant.block.statements = sp =
          alloc_statement((a_statement_kind)stmk_return);
  sp->expr = this_param_value_expr();
  db_exit();
  return;
}  /* make_default_constructor_body */


static void make_default_destructor_body(a_scope_ptr  scope)
/*
Create the body for a default destructor.  It will return no value.
*/
{
  db_enter(4, "make_default_destructor_body");
  /* Create a statement block that is empty except for the return
     statement. */
  scope->assoc_block = alloc_statement((a_statement_kind)stmk_block);
  scope->assoc_block->variant.block.statements =
          alloc_statement((a_statement_kind)stmk_return);
  db_exit();
  return;
}  /* make_default_destructor_body */


static a_boolean is_virtual_base_class_of(a_type_ptr  base_class_type,
                                          a_type_ptr  derived_type)
/*
Return TRUE if base_class_type is a virtual base class of derived_type.
*/
{
  a_base_class_ptr  bcp;

  /* Loop through the base classes. */
  bcp = derived_type->variant.class_struct_union.extra_info->base_classes;
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->type == base_class_type) {
      /* Found it if it's virtual. */
      if (!bcp->is_virtual) bcp = NULL;
      break;
    }  /* if */
  }  /* for */
  /* Return TRUE if we found it. */
  return (bcp != NULL);
}  /* is_virtual_base_class_of */


static a_boolean virtual_base_class_is_indirect(a_base_class_ptr  vbcp,
                                                a_type_ptr        class_type)
/*
vbcp points to a virtual direct base class of the current class (class_type).
Return TRUE if it is also an indirect base class of the current class -- i.e.,
if at least one direct base class of the current class is virtually derived
from the same class as the one with which vbcp is associated.
*/
{
  a_base_class_ptr  bcp;
  a_boolean         is_indirect = FALSE;

  bcp = class_type->variant.class_struct_union.extra_info->base_classes;
  for (; bcp != NULL; bcp = bcp->next) {
    if (is_virtual_base_class_of(vbcp->type, bcp->type)) {
      is_indirect = TRUE;
      break;
    }  /* if */
  }  /* for */
  return is_indirect;
}  /* virtual_base_class_is_indirect */


#if 0
static void check_access_on_assignment_operator(a_type_ptr  class_type,
                                                a_boolean   const_required)
/*
Issue an error if we have no access to the assignment operator for the
class.
*/
{
  a_symbol_ptr  sym, opass_sym = NULL;
  a_boolean     is_overloaded_function;
  a_type_ptr    tp;

  db_enter(4, "check_access_on_assignment_operator");
  sym = symbol_supplement_for_class(class_type)->assignment_operator;
  /* If sym is an overloaded function symbol we need to go through the whole
     list. */
  if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    is_overloaded_function = TRUE;
    sym = sym->variant.overloaded_function.symbols;
  } else {
    is_overloaded_function = FALSE;
  }  /* if */
  /* Find an assignment operator. */
  for (; sym != NULL; sym = (is_overloaded_function ? sym->next : NULL)) {
    tp = routine_symbol_type(sym)->
                          variant.routine.extra_info->param_type_list->type;
    if (is_reference_type(tp)) tp = type_pointed_to(tp);
    if (skip_typerefs(tp) != class_type) {
      /* Not an assignment operator that can be used for copying.  Keep
         looking. */
    } else {
      /* We have a match. */
      opass_sym = sym;
      if (is_const_qualified_type(tp) == const_required) {
        /* We have an exact match. */
        break;
      }  /* if */
    }  /* if */
  }  /* for */
#if CHECKING
  if (opass_sym == NULL) {
    internal_error("check_access_on_assignment_operator: not found");
  }  /* if */
#endif /* CHECKING */
  if (!have_access_to_symbol(opass_sym)) {
    str_error(ec_inaccessible_assignment_operator, name_of_symbol(opass_sym));
  }  /* if */
  db_exit();
}  /* check_access_on_assignment_operator */
#endif /* if 0 */


static a_routine_ptr select_assignment_operator(
                                    a_type_ptr        class_type,
                                    a_boolean         const_object_required,
                                    a_boolean         volatile_object_required,
                                    a_source_position *err_pos,
                                    a_boolean         *pass_by_value)
/*
Return a pointer to the routine entry for the current class's default
assignment operator.
*/
{
  a_symbol_ptr    sym, opass_sym = NULL;
  a_boolean       is_overloaded_function;
  a_type_ptr      arg_type, tp;
  a_boolean       const_object_okay, volatile_object_okay, ambiguous = FALSE;
  a_boolean       sym_matches_exactly, opass_sym_matches_exactly = FALSE;
  a_routine_ptr   opass_routine;

  db_enter(4, "select_assignment_operator");
  sym = symbol_supplement_for_class(class_type)->assignment_operator;
  /* If sym is an overloaded function symbol we need to go through the whole
     list. */
  if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    is_overloaded_function = TRUE;
    sym = sym->variant.overloaded_function.symbols;
  } else {
    is_overloaded_function = FALSE;
  }  /* if */
  /* Find an assignment operator whose argument is ref-class (pass by
     reference) or class (pass_by_value). */
  for (; sym != NULL; sym = (is_overloaded_function ? sym->next : NULL)) {
    arg_type = routine_symbol_type(sym)->
                          variant.routine.extra_info->param_type_list->type;
    tp = arg_type;
    if (is_reference_type(tp)) tp = type_pointed_to(tp);
    if (skip_typerefs(tp) != class_type) {
      /* No match -- keep looking. */
    } else {
      const_object_okay = is_const_qualified_type(tp);
      volatile_object_okay = is_volatile_qualified_type(tp);
      if ((const_object_required && !const_object_okay) ||
          (volatile_object_required && !volatile_object_okay)) {
        /* No match -- keep looking. */
      } else {
        sym_matches_exactly =
                           (const_object_okay == const_object_required &&
                            volatile_object_okay == volatile_object_required);
        if (!sym_matches_exactly && opass_sym != NULL) {
          /* A suitable default assignment operator had already been found,
             but we'll use the one that provides the exact match. */
          if (!opass_sym_matches_exactly) ambiguous = TRUE;
        } else {
          opass_sym = sym;
          opass_sym_matches_exactly = sym_matches_exactly;
          ambiguous = FALSE;
          *pass_by_value = (tp == arg_type);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  opass_routine = NULL;
  if (opass_sym == NULL) {
    /* No applicable copy constructor. */
    if (const_object_required && !volatile_object_required) {
      /* The common case:  missing const copy constructor. */
      pos_st_error(ec_missing_const_assignment_operator, err_pos,
                   class_type->source_corresp.name);
    } else {
      /* Unusual case: volatile or const-volatile expected. */
      pos_st_error(ec_no_suitable_assignment_operator, err_pos,
                   class_type->source_corresp.name);
    }  /* if */
  } else if (ambiguous) {
    /* More than one applicable copy constructor. */
    pos_st_error(ec_ambiguous_assignment_operator, err_pos,
                 class_type->source_corresp.name);
  } else {
    /* Exactly one copy constructor is best. */
    /* Check that the constructor is accessible and mark it referenced. */
    reference_to_implicitly_invoked_function(opass_sym);
    opass_routine = opass_sym->variant.routine;
  }  /* if */
  db_exit();
  return opass_routine;
}  /* select_assignment_operator */


static void make_default_assignment_body(a_scope_ptr  scope)
/*
Create the body for a default assignment operator.  Typically it will
entail a series of member-wise and base-class-wise assignment operations:
based on the properties of the subobject, it will either call an assignment
operator routine or do bitwise assignment.
*/
{
  a_type_ptr                     class_type, tp, array_type;
  a_routine_type_supplement_ptr  rtsp;
  a_statement_ptr                sp;
  a_statement                    head_of_statement_list;
  a_variable_ptr                 source_var;
  an_expr_node_ptr               source_expr, dest_expr;
  a_base_class_ptr               bcp;
  a_field_ptr                    fp;
  a_routine_ptr                  rp;
  a_symbol_ptr                   sym;
  a_boolean                      pass_by_value, const_source_var;
  a_param_type_ptr               ptp;

  db_enter(4, "make_default_assignment_body");
  /* The source variable of the copy is the first parameter on the parameters
     list for the routine.  There must be exactly one parameter for an
     assignment function. */
  rtsp = (skip_typerefs(scope->variant.routine.ptr->type))->
                                                  variant.routine.extra_info;
  ptp = rtsp->param_type_list;
  source_var = make_parameter(ptp->type, (a_storage_class)sc_auto,
                              (a_symbol_ptr)NULL);
  source_var->assoc_param_type = ptp;
  class_type =
          type_pointed_to(scope->variant.routine.this_param_variable->type);
  /* "head_of_statement_list" is a local statement variable whose only
      interesting property is its "next" field, from which a linked list of
      allocated statement entries will be hung.  That list will eventually be
      transferred to the block statement that is created. */
  head_of_statement_list.next = NULL;
  sp = &head_of_statement_list;
  /* See if a bitwise copy is all that is called for. */
  if (symbol_supplement_for_class(class_type)->
                   assignment_by_bitwise_copy_allowed) {
    /* Yes.  (Then why are we defining a routine?  Probably because the
       address of the default assignment operator was taken, forcing the
       actual creation of the routine.) */
    /* Get the source and destination expressions to use as operands for an
       assignment statement. */
    source_expr = var_rvalue_expr(source_var);
    dest_expr = this_param_value_expr();
    sp = sp->next = make_assignment_statement(dest_expr, source_expr);
  } else {
    /* Memberwise copy is required.  That is, first do the appropriate
       operation on each direct base class (direct assignment or calling
       the base class's assignment function), and then do the appropriate
       copy of each member. */
    const_source_var = is_const_qualified_type(source_var->type);
    bcp = class_type->variant.class_struct_union.extra_info->base_classes;
    for (; bcp != NULL; bcp = bcp->next) {
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
        /* The source is the first parameter cast to the same base class. */
        source_expr = base_class_selection_expr(var_rvalue_expr(source_var),
                                                bcp);
        if (symbol_supplement_for_class(bcp->type)->
                         assignment_by_bitwise_copy_allowed) {
          /* A bitwise copy may be performed. */
#if 0
          /* Even though the assignment operator is not actually invoked,
             it must be accessible. */
          check_access_on_assignment_operator(bcp->type, const_source_var);
#endif /* if 0 */
          /* Dereference the pointer-to-base-class. */
          source_expr = add_indirection_to_node(source_expr);
          /* Create the assignment statement.  The appropriate operator
             will be selected by the function. */
          sp = sp->next = make_assignment_statement(dest_expr, source_expr);
        } else {
          /* A bitwise copy may not be done.  Find the default assignment
             operator and put out a call to it. */
          rp = select_assignment_operator(bcp->type, const_source_var,
                                          /*volatile_object_required=*/FALSE,
                                          &error_position, &pass_by_value);
          if (pass_by_value) {
            source_expr = add_indirection_to_node(source_expr);
          }  /* if */
          sp = sp->next = make_call_assignment_statement(rp, dest_expr,
                                                         source_expr);
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
        tp = fp->type;
#if 0
        /* Do error checking -- ARM 12.8. */
        if (is_reference_type(tp)) {
          error();
          continue;
        }  /* if */
        if (is_const_qualified_or_has_const_element_or_has_const_member) error;
#endif /* if 0 */
        /* If this is an array, we need the element type. */
        array_type = NULL;
        if (is_array_type(tp)) {
          array_type = tp;
          tp = underlying_array_element_type(tp);
        }  /* if */
        tp = skip_typerefs(tp);
        /* The destination is the appropriate field (lvalue) of the "this"
           parameter. */
        dest_expr = field_lvalue_selection_expr(this_param_value_expr(), fp);
        /* The source will be the appropriate field of the first argument,
           but we don't know yet whether it's an lvalue or an rvalue. */
        source_expr = var_rvalue_expr(source_var);
        if (is_class_struct_union_type(tp)) {
          /* It's a class type, so we may have to call an assignment operator
             function. */
          if (symbol_supplement_for_class(tp)->
                           assignment_by_bitwise_copy_allowed) {
            /* A bitwise copy may be performed. */
#if 0
            /* Even though the assignment operator is not actually invoked,
               it must be accessible. */
            check_access_on_assignment_operator(bcp->type, const_source_var);
#endif /* if 0 */
            /* Source is an rvalue field reference. */
            source_expr = field_rvalue_selection_expr(source_expr, fp);
            /* Create the assignment. */
            sp = sp->next = make_assignment_statement(dest_expr, source_expr);
          } else if (array_type == NULL) {
            /* A bitwise copy may not be done.  Find the default assignment
               operator and put out a call to it. */
            rp = select_assignment_operator(tp, const_source_var,
                                            /*volatile_object_required=*/FALSE,
                                            &error_position, &pass_by_value);
            source_expr = field_lvalue_selection_expr(source_expr, fp);
            if (pass_by_value) {
              source_expr = add_indirection_to_node(source_expr);
            }  /* if */
            sp = sp->next = make_call_assignment_statement(rp, dest_expr,
                                                           source_expr);
          } else {
#if 0
            add_statement(for, ...call...);
#else
#if CHECKING
            internal_error(
    "make_default_assignment_body: operator=() calls on array not implmented");
#endif /* CHECKING */
#endif /* if 0 */
          }  /* if */
        } else {
          /* Not a class type.  Just do a bitwise copy.  The appropriate IL
             operator will be selected by make_assignment_statement. */
          source_expr = field_rvalue_selection_expr(source_expr, fp);
          sp = sp->next = make_assignment_statement(dest_expr, source_expr);
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


void define_special_member_function(a_routine_ptr  rout_ptr,
                                    a_type_ptr     class_type)
/*
Define a compiler generated routine for a member function (constructor or
destructor).  This entails creating a new memory region, a scope, and an
empty statement block.
*/
{
  a_scope_ptr               scope;
  a_routine_type_supplement *rtsp = rout_ptr->type->variant.routine.extra_info;

  db_enter(4, "define_special_member_function");
#if CHECKING
  if (rout_ptr->special_kind != (a_special_function_kind)sfk_constructor &&
      rout_ptr->special_kind != (a_special_function_kind)sfk_destructor &&
      (rout_ptr->special_kind != (a_special_function_kind)sfk_operator ||
       rout_ptr->opname_kind != (an_opname_kind)onk_assign)) {
    internal_error(
                "define_special_member_function: expected ctor, dtor, or =");
  }  /* if */
#endif /* CHECKING */
  /* Push a class symbol reactivation scope, to make class member names
     visible for processing the function definition. */
  push_class_reactivation_scope(class_type);
  /* Push the scope for the new function itself. */
  scope = push_scope((a_scope_kind)sck_function, NO_SCOPE_NUMBER,
                     (a_type_ptr)NULL, rout_ptr);
  /* Associate the scope to the routine entry and the routine entry to its
     type entry. */
  rout_ptr->assoc_scope = curr_il_region_number;
  rtsp->assoc_routine = rout_ptr;
  scope->variant.routine.this_param_variable =
                make_param_variable(rtsp->implicit_this_param_type,
                                    (a_storage_class)sc_auto);
  /* Enter the constructor and destructor initializers, to record possible
     implicit initializers. */
  if (rout_ptr->special_kind == (a_special_function_kind)sfk_constructor) {
    scope->variant.routine.constructor_inits =
                          ctor_initializer(rout_ptr, /*user_defined=*/FALSE);
    (void)make_default_constructor_body(scope);
  } else if (rout_ptr->special_kind ==
                                (a_special_function_kind)sfk_destructor) {
    scope->variant.routine.constructor_inits = dtor_initializer(rout_ptr);
    (void)make_default_destructor_body(scope);
  } else {
    /* Assignment operator case. */
    (void)make_default_assignment_body(scope);
  }  /* if */
  /* End of statement block is unreachable because of the return statement. */
  scope->assoc_block->variant.block.extra_info->end_of_block_reachable = FALSE;
  /* Terminate the function scope. */
  pop_scope();
  /* Terminate the class reactivation scope. */
  pop_class_reactivation_scope();
  /* Mark the symbol for this routine "defined". */
  ((a_symbol_ptr)rout_ptr->source_corresp.assoc_info)->defined = TRUE;
  db_exit();
}  /* define_special_member_function */


void reference_to_implicitly_invoked_function(a_symbol_ptr sym)
/*
sym is points to a symbol for a special member function that is invoked
implicitly -- e.g., a copy constructor that is called when a class object
is passed by value or an assignment operator that is called when another
assignment operator function is being created.  Check that the special member
function is accessible and mark the routine entry referenced.  Also, if the
routine is compiler generated, it may still need to be defined, since the
definition may have been put off until an actual reference occurred (e.g.,
ARM 12.8).  This function deals with implicitly called constructors,
destructors, assignment operators, and conversion functions.
*/
{
  a_routine_ptr  rp = sym->variant.routine;
  an_error_code  err_code;

#if CHECKING
  if (rp->special_kind != (a_special_function_kind)sfk_constructor &&
      rp->special_kind != (a_special_function_kind)sfk_destructor &&
      rp->special_kind != (a_special_function_kind)sfk_conversion &&
      (rp->special_kind != (a_special_function_kind)sfk_operator ||
       rp->opname_kind != (an_opname_kind)onk_assign)) {
    internal_error(
               "reference_to_implicitly_invoked_function: unexpected sfkind");
  }  /* if */
#endif /* CHECKING */
  /* Check for accessibility. */
  if (!have_access_to_symbol(sym)) {
    if (rp->special_kind == (a_special_function_kind)sfk_conversion) {
      error(ec_inaccessible_conversion_function);
    } else {
      if (rp->special_kind == (a_special_function_kind)sfk_constructor) {
        err_code = ec_inaccessible_constructor;
      } else if (rp->special_kind == (a_special_function_kind)sfk_destructor) {
        err_code = ec_inaccessible_destructor;
      } else {
        err_code = ec_inaccessible_assignment_operator;
      }  /* if */
      str_error(err_code, name_of_symbol(sym));
    }  /* if */
  }  /* if */
  /* Mark the IL entry referenced. */
  rp->source_corresp.referenced = TRUE;
  /* If necessary, create the function body for a compiler generated
     routine. */
  if (rp->compiler_generated && rp->assoc_scope == NULL_region_number) {
    define_special_member_function(rp, sym->class_of_which_a_member);
  }  /* if */
}  /* reference_to_implicitly_invoked_function */


static void default_assignment_operator_check(a_type_ptr  class_type,
                                              a_boolean   *const_okay,
                                              a_boolean   *bitwise_copy_okay)
/*
We are about to create a compiler-generated default assignment operator.
Some of its characteristics are dependent on the assignment operators
defined for base classes and fields of the current class (class_type).
Specifically, we need to determine whether the default assignment operator
can copy a const object and whether bitwise copying is allowed.
*/
{
  a_base_class_ptr               bcp;
  a_type_ptr                     tp;
  a_class_symbol_supplement_ptr  cssp;
  a_symbol_ptr                   sym;
  a_field_ptr                    fp;
  a_boolean                      local_const_okay = TRUE;
  a_boolean                      local_bitwise_copy_okay = TRUE;

  db_enter(4, "default_assignment_operator_check");
  /* A bitwise copy to implement default assignment can be done if there are
     no virtual base classes and no virtual functions and if all subobjects
     can be assigned by bitwise copy.  The check for virtual base classes
     and virtual functions is easy. */
  if (class_type->variant.class_struct_union.any_virtual_base_classes ||
      class_type->variant.class_struct_union.extra_info->
                                                virtual_function_count > 0) {
    local_bitwise_copy_okay = FALSE;
  }  /* if */
  /* Now check for const.  Do the base classes first. */
  bcp = class_type->variant.class_struct_union.extra_info->base_classes;
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct || bcp->is_virtual) {
      cssp = symbol_supplement_for_class(bcp->type);
      if (local_const_okay) {
        (void)assignment_operator_for_copy_exists(cssp->assignment_operator,
                                                  &local_const_okay);
        if (!local_const_okay && !local_bitwise_copy_okay) goto done;
      }  /* if */
      if (local_bitwise_copy_okay &&
          !cssp->assignment_by_bitwise_copy_allowed) {
        local_bitwise_copy_okay = FALSE;
        if (!local_const_okay) goto done;
      }  /* if */
    }  /* if */
  }  /* for */
  /* Base classes are okay.  Now check the nonstatic data members. */
  sym = ((a_symbol_ptr)class_type->source_corresp.assoc_info)->
                         variant.class_struct_union.extra_info->symbols;
  for (; sym != NULL; sym = sym->next_in_scope) {
    if (sym->kind == (a_symbol_kind)sk_field) {
      fp = sym->variant.field.ptr;
      tp = fp->type;
      /* Get the element type if this is an array field. */
      if (is_array_type(tp)) tp = underlying_array_element_type(tp);
      if (is_class_struct_union_type(tp)) {
        cssp = symbol_supplement_for_class(tp);
        if (local_const_okay) {
          (void)assignment_operator_for_copy_exists(cssp->assignment_operator,
                                                    &local_const_okay);
          if (!local_const_okay && !local_bitwise_copy_okay) goto done;
        }  /* if */
        if (local_bitwise_copy_okay &&
            !cssp->assignment_by_bitwise_copy_allowed) {
          local_bitwise_copy_okay = FALSE;
          if (!local_const_okay) goto done;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
done:
  *const_okay = local_const_okay;
  *bitwise_copy_okay = local_bitwise_copy_okay;
  db_exit();
}  /* default_assignment_operator_check */


static void default_copy_constructor_check(a_type_ptr  class_type,
                                           a_boolean   *const_okay)
/*
When the compiler generates a default copy constructor, it should be declared
for copying a const object only if all the copy constructors it implicitly
invokes can also copy const objects (ARM 12.8).  Check all direct and virtual
base classes and all fields, and return TRUE if all class subobjects have copy
constructors that can copy const objects.
*/
{
  a_base_class_ptr               bcp;
  a_type_ptr                     tp;
  a_class_symbol_supplement_ptr  cssp;
  a_field_ptr                    fp;

  db_enter(4, "default_copy_constructor_check");
  *const_okay = TRUE;
  /* First check the base classes. */
  bcp = class_type->variant.class_struct_union.extra_info->base_classes;
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct || bcp->is_virtual) {
      cssp = symbol_supplement_for_class(bcp->type);
      if (cssp->has_copy_constructor &&
          !cssp->has_copy_constructor_for_const_object) {
        /* Class lacks a copy constructor that can copy a const object. */
        *const_okay = FALSE;
        goto done;
      }  /* if */
    }  /* if */
  }  /* for */
  /* Base classes are okay.  Now check the nonstatic data members. */
  fp = class_type->variant.class_struct_union.field_list;
  for (; fp != NULL; fp = fp->next) {
    tp = fp->type;
    /* Get the element type if this is an array field. */
    if (is_array_type(tp)) tp = underlying_array_element_type(tp);
    if (is_class_struct_union_type(tp)) {
      cssp = symbol_supplement_for_class(tp);
      if (cssp->has_copy_constructor &&
          !cssp->has_copy_constructor_for_const_object) {
        /* Class lacks a copy constructor that can copy a const object. */
        *const_okay = FALSE;
        goto done;
      }  /* if */
    }  /* if */
  }  /* for */
done:;
  db_exit();
}  /* default_copy_constructor_check */


static void check_special_member_functions(a_type_ptr  class_type)
/*
Check for the existence of constructors (including copy constructor) and
destructor among the user defined member functions for the class specified
by class_type.  If any compiler-generated functions are required, create
for each a routine entry and add it to the routines list for the class.
The routine body is not generated until it is known to be needed.
*/
{
  a_param_type_ptr              ptp;
  a_class_symbol_supplement_ptr cssp;
  a_boolean                     const_okay, bitwise_copy_okay;

  db_enter(3, "check_special_member_functions");
  cssp = symbol_supplement_for_class(class_type);
  if (cssp->constructor_required && cssp->constructor == NULL) {
    generate_special_function(class_type, (a_param_type_ptr)NULL,
                              (a_special_function_kind)sfk_constructor);
  }  /* if */
  if (cssp->constructor != NULL && !cssp->has_copy_constructor) {
    default_copy_constructor_check(class_type, &const_okay);
    /* Generate a copy constructor. */
    ptp = alloc_param_type(make_reference_type(
                             make_qualified_type(class_type,
                                                 /*is_const=*/const_okay,
                                                 /*is_volatile=*/FALSE)));
    generate_special_function(class_type, ptp,
                              (a_special_function_kind)sfk_constructor);
  }  /* if */
  if (cssp->destructor_required && cssp->destructor == NULL) {
    generate_special_function(class_type, (a_param_type_ptr)NULL,
                              (a_special_function_kind)sfk_destructor);
  }  /* if */
  /* Create a default assignment operator to copy an object of the current
     class if one doesn't already exist. */
  if (!assignment_operator_for_copy_exists(cssp->assignment_operator,
                                           &const_okay)) {
    default_assignment_operator_check(class_type, &const_okay,
                                      &bitwise_copy_okay);
    ptp = alloc_param_type(make_reference_type(
                             make_qualified_type(class_type,
                                                 /*is_const=*/const_okay,
                                                 /*is_volatile=*/FALSE)));
    generate_special_function(class_type, ptp,
                              (a_special_function_kind)sfk_operator);
    cssp->assignment_by_bitwise_copy_allowed = bitwise_copy_okay;
  }  /* if */
  db_exit();
}  /* check_special_member_functions */


static void project_base_class_conversion_functions(a_type_ptr class_type)
/*
Go through all the direct base classes of the current class class_type and
create projection symbols to represent inherited conversion functions.  Also
create a conversion_list_entry for each new projection symbol and link it to
the list for the current class.  Only create a new projection symbol if the
destination type is not yet on the current class's conversion list.
*/
{
  a_base_class_ptr               bcp;
  a_class_symbol_supplement_ptr  cssp;
  a_conversion_list_entry_ptr    clep, bcclep;
  a_symbol_locator               loc;

  bcp = class_type->variant.class_struct_union.extra_info->base_classes;
  cssp = symbol_supplement_for_class(class_type);
  /* Examine each direct base class. */
  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct) {
      /* Examine each conversion list entry in the base class. */
      bcclep = (symbol_supplement_for_class(bcp->type))->conversion_list;
      for (; bcclep != NULL; bcclep = bcclep->next) {
        /* Compare the conversion list entry from the base class with each
           conversion list entry for the current class.  They convert to the
           same type if they have the same header. */
        for (clep = cssp->conversion_list; clep != NULL; clep = clep->next) {
          if (clep->symbol->header == bcclep->symbol->header) break;
        }  /* for */
        /* If clep is not NULL a conversion list entry from the current class
           already represents a conversion to the type specified by the
           conversion defined in the base class.  Otherwise, go ahead and
           create a projection into the current class. */
        if (clep == NULL) {
          /* Allocate the new conversion list entry and link it in the the
             list for the current class. */
          clep = alloc_conversion_list_entry();
          clep->next = cssp->conversion_list;
          cssp->conversion_list = clep;
          /* Create the projection symbol and record it in the new conversion
             list entry. */
          make_locator_for_symbol(bcclep->symbol, &loc);
          loc.specific_symbol = NULL;
          (void)find_projected_symbol(class_type, &loc, /*must_be_tag=*/FALSE,
                                      /*must_be_type_name=*/FALSE,
                                      /*add_to_active_list=*/TRUE,
                                      (a_symbol_ptr)NULL, &clep->symbol);
#if CHECKING
          if (clep->symbol == NULL) {
            internal_error(
                     "project_base_class_conversion_functions: no projection");
          }  /* if */
#endif /* CHECKING */
        }  /* if */
        /* Get the next conversion list entry from the base class. */
      }  /* for */
    }  /* if */
    /* Get the next direct base class. */
  }  /* for */
}  /* project_base_class_conversion_functions */


static an_access_adjustment_ptr new_access_adjustment(
                                                 a_symbol_ptr         sym,
                                                 an_access_specifier  access)
/*
Allocate an access adjustment entry, set its fields based on sym, and return
a pointer to it.
*/
{
  an_access_adjustment_ptr   aap;
  an_access_adjustment_kind  aa_kind;

  /* Determine the access-adjustment-kind, based on the symbol kind. */
  switch (sym->kind) {
    case sk_static_data_member:
      aa_kind = (an_access_adjustment_kind)aak_variable;  break;
    case sk_constant:
      aa_kind = (an_access_adjustment_kind)aak_constant;  break;
    case sk_class_or_struct_tag:
    case sk_union_tag:
    case sk_enum_tag:
    case sk_type:
      aa_kind = (an_access_adjustment_kind)aak_type;      break;
    case sk_member_function:
      aa_kind = (an_access_adjustment_kind)aak_routine;   break;
    case sk_field:
      aa_kind = (an_access_adjustment_kind)aak_field;     break;
#if CHECKING
    default:
      internal_error("new_access_adjustment: unexpected symbol kind");
#endif /* CHECKING */
  }  /* switch */
  /* Allocate an access adjustment entry of the appropriate kind. */
  aap = alloc_access_adjustment(aa_kind);
  aap->access = access;
  /* Add a pointer to the correct IL entity. */
  switch (aa_kind) {
    case aak_variable:  aap->variant.variable = sym->variant.variable;  break;
    case aak_constant:  aap->variant.constant = sym->variant.constant;  break;
    case aak_type:      aap->variant.type = sym->variant.type;          break;
    case aak_routine:   aap->variant.routine = sym->variant.routine;    break;
    case aak_field:     aap->variant.field = sym->variant.field.ptr;    break;
  }  /* switch */

  return aap;
}  /* new_access_adjustment */


static void access_adjustment_decl(an_access_specifier  access,
                                   a_type_ptr           class_type)
/*
The current token is a qualified name and the next token is a semicolon.
Syntactically, this is an access adjustment declaration.  If the declaration
is semantically sound, update the data base appropriately.
*/
{
  an_access_specifier          progenitor_access;
  a_symbol_ptr                 projection_into_curr_class;
  a_symbol_ptr                 immediate_progenitor_sym;
  a_base_class_ptr             bcp;
  a_symbol_locator             locator;
  an_access_adjustment_ptr     aap;
  a_class_type_supplement_ptr  ctsp;

  db_enter(4, "access_adjustment_decl");
#if CHECKING
  if (curr_token != tok_identifier ||
      locator_for_curr_id.specific_symbol->class_of_which_a_member == NULL) {
    internal_error("access_adjustment_decl: expected qualified name");
  }  /* if */
#endif /* CHECKING */
  /* Be sure the class in the qualified name is one from which the current
     class is derived. */
  ctsp = class_type->variant.class_struct_union.extra_info;
  for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
    if (bcp->type == locator_for_curr_id.specific_symbol->
                                         class_of_which_a_member) break;
  }  /* for */
  if (bcp == NULL) {
    /* Qualified name must identify a member of a base class of the current
       class. */
    error(ec_bad_base_class);
    goto done;
  } else if (bcp->ambiguous) {
    error(ec_ambiguous_base_class);
    set_to_error_locator(locator_for_curr_id);
    goto done;
  }  /* if */
  /* Look up the name without class qualification.  This will show whether
     the name has already been declared and if not give us the projection
     of the name into the current class. */
  clear_locator(&locator, &locator_for_curr_id.source_position);
  locator.symbol_header = locator_for_curr_id.symbol_header;
  (void)normal_id_lookup(&locator, IDL_NO_OPTIONS);
  projection_into_curr_class = locator.specific_symbol;
  if (projection_into_curr_class->kind != (a_symbol_kind)sk_projection ||
      projection_into_curr_class->variant.projection.access_adjustment_made) {
    /* Name has already been redeclared in the current scope and cannot
       appear in an access adjustment. */
    pos_error(ec_id_already_declared, &locator_for_curr_id.source_position);
    goto done;
  }  /* if */
  /* Find the immediate progenitor of projection_into_curr_class.  If the
     symbol originally specified in the source is a member of an indirect
     base class, look for its projection in a direct base class. */
  if (bcp->direct) {
    immediate_progenitor_sym = locator_for_curr_id.specific_symbol;
  } else {
    /* Use the first base class on the derivation path. */
    bcp = bcp->derivation->base_class;
    clear_locator(&locator, &locator_for_curr_id.source_position);
    locator.symbol_header = locator_for_curr_id.symbol_header;
    (void)class_qualified_id_lookup(&locator, bcp->type, IDL_NO_OPTIONS);
    immediate_progenitor_sym = locator.specific_symbol;
  }  /* if */
  /* The projection in the current class and the progenitor from the immediate
     base class must refer to the same object.  This might not happen if
     the qualified name tried to "jump over" a redeclaration in the immediate
     base class. */
  if (fundamental_symbol_of(immediate_progenitor_sym) !=
                      fundamental_symbol_of(projection_into_curr_class)) {
    str_error(ec_not_equivalent_to_inherited_member,
              immediate_progenitor_sym->header->identifier);
    goto done;
  }  /* if */
  /* From here on out errors are treated differently -- in spite of the error
     the access_adjustment_made flag is set to TRUE. */
  projection_into_curr_class->variant.projection.access_adjustment_made = TRUE;
  if (access == (an_access_specifier)as_private) {
    /* Access adjustment may not be appear in the private part of a
       derived class declaration. */
    error(ec_access_adjustment_in_private_section);
  } else {
    progenitor_access = access_for_symbol(immediate_progenitor_sym);
    if (access != progenitor_access) {
      if (is_more_accessible(progenitor_access, access)) {
        /* Restricting access beyond what public derivation of its class
           would have produced is not allowed. */
        error(ec_restricting_access_not_allowed);
      } else {
        /* Enabling greater access than what public derivation of its class
           would have produced is not allowed. */
        error(ec_increasing_access_not_allowed);
      }  /* if */
    } else {
      /* This is a valid access adjustment. */
      projection_into_curr_class->variant.projection.access = access;
      /* Create an access-adjustment entry to represent this declaration in
         the IL. */
      aap = new_access_adjustment(
                   fundamental_symbol_of(projection_into_curr_class), access);
      /* Attach it the class type entry. */
      ctsp = class_type->variant.class_struct_union.extra_info;
      aap->next = ctsp->access_adjustments;
      ctsp->access_adjustments = aap;
    }  /* if */
  }  /* if */

done:
  db_exit();
}  /* access_adjustment_decl */


a_boolean class_specifier(a_boolean  vacuous_decl_allowed,
                          a_boolean  is_friend_decl,
                          a_type_ptr *type_ptr,
                          a_boolean  *declares_something,
			  a_boolean  *defines_something)
/*
Scan a class-specifier (3.5.2.1), which declares a struct or
union type.  The syntax is

9
        class-specifier:
                class-head { member-list    }
                                        opt

        class-head:
                class-key identifier    base-spec
                                    opt          opt
                class-key class-name base-spec
                                              opt

        class-key
                class
                struct
                union

9.2
        member-list
                member-declaration member-list
                                              opt
                access-specifier : member-list

        member-declaration:
                decl-specifiers    member-declarator-list    ;
                               opt                       opt
                function-definition ;
                                     opt
                qualified-name ;

        member-declarator-list:
                member-declarator
                member-declarator-list , member-declarator

        member-declarator
                declarator pure-specifier
                                         opt
                identifier    : constant-expression
                          opt

        pure-specifier
                = 0

The type is returned in *type_ptr. *declares_something is set to indicate
whether or not this specifier declares something, and *declares_something
to indicate whether the class/struct/union is actually defined.
*/
{
  an_access_specifier     access;
  a_symbol_kind           tag_kind;
  a_type_kind             type_kind;
  a_symbol_locator        locator;
  a_symbol_ptr            tag_sym;
  a_boolean               tag_id_present;
  a_type_ptr              class_type;
  a_storage_class         member_storage_class;
  a_type_ptr              member_type;
  a_decl_flag_set         dso_flags;
  a_type_ptr              local_type;
  a_type_ptr              bottom_derived_type;
  a_boolean               is_local_class = FALSE;
  a_boolean               unnamed_field;
  a_boolean               tag_resolution = FALSE;
  a_boolean               first_declarator;
  a_boolean               is_first_field;
  a_func_info_block       func_info;
  a_boolean               any_overflow = FALSE;
  a_boolean               err = FALSE;
  a_boolean               dangling_type_specifier;
  a_boolean               local_defines_something;
  a_boolean               local_declares_something;
  a_boolean               local_no_decl_specifiers;
  a_boolean               friend_specified;
  a_boolean               virtual_specified;
  a_boolean               inline_specified;
  a_boolean               type_explicitly_specified;
  a_source_position       decl_start_pos;
  a_scope_ptr             scope_ptr;
  a_decl_flag_set         dsi_flags;
  a_special_function_kind spec_kind;
  a_boolean               is_destructor, is_constructor;
  a_targ_size_t           byte_offset;
  int                     bit_offset;
  a_targ_alignment        alignment;
  a_field_ptr             end_of_field_list = NULL;
  a_symbol_ptr            rout_sym;
  a_memory_region_number  region_to_switch_back_to;
  a_class_symbol_supplement_ptr
                          cssp;
  a_scope_depth           effective_decl_level = decl_scope_level;
  a_boolean               is_anonymous_union;
  an_expr_node_ptr        dim_expr_ptr;

  db_enter(3, "class_specifier");
  *declares_something = FALSE;
  *defines_something = FALSE;
  /* Determine whether this is a local class (one being declared within a
     function scope). */
  is_local_class = (depth_of_containing_function_scope() != NO_SCOPE_DEPTH);
  if (!is_qualified_name_start()) {
    /* Skip over "class", "struct", or "union", remembering which appears. */
#if CHECKING
    if (curr_token != tok_class &&
        curr_token != tok_struct &&
        curr_token != tok_union) {
      internal_error("class_specifier: expected class, struct, or union");
    }  /* if */
#endif /* CHECKING */
    if (curr_token == tok_union) {
      tag_kind = (a_symbol_kind)sk_union_tag;
      type_kind = (a_type_kind)tk_union;
    } else {
      tag_kind = (a_symbol_kind)sk_class_or_struct_tag;
      type_kind = (a_type_kind)(curr_token == tok_struct ?
                                                    tk_struct : tk_class);
    }  /* if */
    /* If there is an identifier next, it is a tag.  It can be the declaration
       of a new tag or a reference to an existing tag.  Although it is an
       error, also be on the lookout for a qualified name. */
    (void)get_token();
    tag_id_present = is_qualified_name_start();
  } else {
#if CHECKING
    if (!is_friend_decl) {
      internal_error("class_specifier: identifier but not friend decl");
    }  /* if */
#endif /* CHECKING */
    tag_kind = (a_symbol_kind)sk_class_or_struct_tag;
    type_kind = (a_type_kind)tk_class;
    tag_id_present = TRUE;
  }  /* if */
  if (tag_id_present) {
    /* It seems that appearance of a tag name is a declaration of the
       tag, even if it just repeats a previous name.  At least, there's
       a Plum Hall test that implies that. */
    *declares_something = TRUE;
#if CHECKING
    if (vacuous_decl_allowed && is_friend_decl) {
      internal_error("class_specifier: vacuous decl not okay in friend decl");
    }  /* if */
#endif /* CHECKING */
    tag_sym = scan_tag_name(tag_kind, &locator, vacuous_decl_allowed,
                            &effective_decl_level, &tag_resolution);
  } else {
    /* No tag identifier present. */
    tag_sym = NULL;
    set_to_error_locator(locator);
    if (curr_token == tok_lbrace ||
        (C_dialect == C_dialect_cplusplus && curr_token == tok_colon)) {
      /* This is a tagless class definition. */
    } else {
      /* Neither the tag id nor the {...} is present.  This is an error. */
      add_stop_token(tok_lbrace);
      if (C_dialect == C_dialect_cplusplus) add_stop_token(tok_colon);
      syntax_error(ec_exp_definition_of_tag);
      err = TRUE;
      if (C_dialect == C_dialect_cplusplus) remove_stop_token(tok_colon);
      remove_stop_token(tok_lbrace);
    }  /* if */
  }  /* if */
  if (tag_sym == NULL) {
    /* Create a new class, struct, or union type.  All such types are
       allocated in the file scope memory region, though local types will be
       added to the function scope's types list. */
    class_type = alloc_type(type_kind);
    /* Wait to add the type to the types list; it should not be added
       until the closing brace of the full definition appears, to get the
       IL list in the right order. */
    /* Enter a new tag symbol, if a tag id was specified (a tag is not
       specified in something like "struct {int a; int b;}"). */
    if (tag_id_present) {
      tag_sym = enter_local_symbol(tag_kind, &locator, effective_decl_level,
                                   /*suppress_redecl_error=*/FALSE);
      set_source_corresp(&(class_type->source_corresp), tag_sym);
    } else {
      /* Tagless class, struct, or union.  Create a symbol to represent it;
         though not entered in the symbol table, it is needed to carry
         around some information about classes that is of interest to the
         front end only. */
      tag_sym = make_unnamed_class_symbol(tag_kind, &pos_curr_token);
      /* Although the symbol header has a name of sorts, it should not appear
         in the type, so NULL it out after the call to set_source_corresp. */
      set_source_corresp(&(class_type->source_corresp), tag_sym);
      class_type->source_corresp.name = NULL;
    }  /* if */
    tag_sym->variant.class_struct_union.type = class_type;
    if (C_dialect == C_dialect_cplusplus) {
      /* In C classes have no linkage.  In C++ most classes have either
         internal linkage or, for classes declared at file scope and with
         other characteristics (see ARM 3.3), C++ external linkage; local
         classes and classes nested within local classes have no linkage.
         For now give nonlocal classes internal linkage; it may be changed
         later (see check_class_linkage).  Note that even nameless classes
         may be marked as having linkage; this is useful for dealing with
         member functions.) */
      if (!is_local_class) {
        /* Nonlocal class. */
        class_type->source_corresp.name_linkage =
                                         (a_name_linkage_kind)nlk_internal;
      }  /* if */
    }  /* if */
  } else {
    /* Using an existing type.  Fetch the type pointer from it. */
    class_type = tag_sym->variant.class_struct_union.type;
    /* Record cross-reference information. */
    if (curr_token == tok_lbrace ||
        (C_dialect == C_dialect_cplusplus && curr_token == tok_colon)) {
      mark_declared(tag_sym, &locator.source_position,
                    /*save_as_decl_position=*/TRUE);
      /* Allow for alternating between class and struct, but stay with the
         one associated with the definition.  The difference only affects
         default member access. */
      class_type->kind = type_kind;
    } else {
      mark_referenced(tag_sym, &locator.source_position);
    }  /* if */
  }  /* if */
  if (C_dialect == C_dialect_cplusplus && curr_token == tok_colon) {
    /* Scan the list of base specifiers. */
    add_stop_token(tok_lbrace);
    scan_base_specifier_list(class_type);
    remove_stop_token(tok_lbrace);
    /* If there is a base specifier list and this is a class or struct
       declaration, it has to be definition, which means the next token
       should be a brace. */
    if (curr_token != tok_lbrace &&
        class_type->kind != (a_type_kind)tk_union) {
      syntax_error(ec_missing_class_definition);
      err = TRUE;
    }  /* if */
  }  /* if */
  if (curr_token == tok_lbrace) {
    /* Scan the structure or union itself.  Since (for reasons mentioned above)
       the class type is allocated in the file scope memory region, all its
       members are also allocated there (otherwise, an IL object in the file
       scope would point to a component that might not be available).  Switch
       to the file scope memory region here at the start of the definition and
       switch back when we reach the right brace. */
    switch_to_file_scope_region(&region_to_switch_back_to);
    /* This is a class, struct, or union definition -- not merely a
       declaration. */
    *defines_something = TRUE;
    tag_sym->defined = TRUE;
    /* If this is the definition of a nested class, set the parent class
       pointer in the tag symbol and set the access. */
    if (scope_stack[decl_scope_level].kind ==
                                (a_scope_kind)sck_class_struct_union) {
      class_type->source_corresp.class_of_which_a_member =
            tag_sym->class_of_which_a_member =
                              scope_stack[decl_scope_level].assoc_type;
      class_type->source_corresp.access =
                              scope_stack[decl_scope_level].current_access;
    }  /* if */
    /* Advance past the left brace. */
    (void)get_token();
    add_stop_token(tok_rbrace);
    /* Start a scope for the fields and other members. */
    scope_ptr = push_scope((a_scope_kind)sck_class_struct_union,
                           NO_SCOPE_NUMBER,
                           class_type, (a_routine_ptr)NULL);
    if (C_dialect == C_dialect_cplusplus) {
      /* In C++ every class, struct, and union type entry will have a non-NULL
         pointer to a class type supplement entry.  Put a pointer to the
         IL scope entry into it. */
      class_type->variant.class_struct_union.extra_info->assoc_scope =
                                                                 scope_ptr;
      /* Reserve space in the current class for its nonvirtual base classes,
         which are located at the start of the object.  (Virtual base classes
         appear at the end.) */
      set_offsets_for_nonvirtual_base_classes(class_type, &any_overflow);
    }  /* if */
    byte_offset = class_type->size;
    bit_offset = 0;
    alignment = class_type->alignment;
    if (C_dialect == C_dialect_cplusplus && curr_token == tok_rbrace) {
      /* A member list is optional in C++. */
    } else {
      if (class_type->kind == (a_type_kind)tk_class) {
        /* Members of a C++ class have private access by default. */
        access = (an_access_specifier)as_private;
      } else {
        /* Members of a C++ struct or union have public access by default,
           which is also the implicit access control for C struct and union
           fields. */
        access = (an_access_specifier)as_public;
      }  /* if */
      scope_stack[decl_scope_level].current_access = access;
      /* Set the flags to control the calls to decl_specifiers. */
      dsi_flags = DSI_TYPE_SPECIFIER_ALLOWED;
      if (C_dialect == C_dialect_cplusplus) {
	dsi_flags |= (DSI_STORAGE_CLASS_SPECIFIER_ALLOWED |
                      DSI_IS_MEMBER_DECLARATION | DSI_INLINE_ALLOWED |
                      DSI_EMPTY_DECL_SPECIFIERS_ALLOWED |
                      DSI_VACUOUS_TAG_DECL_ALLOWED);
      }  /* if */
      is_first_field = TRUE;
      do {
        if (C_dialect == C_dialect_cplusplus) {
          /* An access specification may appear anywhere amid the member
             declarations.  Check for it each time through the loop, and adjust
             the value of access accordingly.  The check is implemented as a
             loop because successive access specifications are permitted. */
          while (curr_token == tok_public || curr_token == tok_private ||
                 curr_token == tok_protected) {
            switch (curr_token) {
              case tok_public:
                access = (an_access_specifier)as_public;
                break;
              case tok_protected:
                access = (an_access_specifier)as_protected;
                break;
              case tok_private:
                access = (an_access_specifier)as_private;
                break;
            }  /* switch */
            scope_stack[decl_scope_level].current_access = access;
            /* Advance to the colon, which is required. */
            (void)get_token();
            if (curr_token == tok_colon) {
              /* Advance past it. */
              (void)get_token();
            } else {
              /* Calling is_member_decl_start involves calling
                 curr_type_symbol, which suppresses access and ambiguity
                 errors when looking up what may be a qualified name.  This
                 is correct in this case since we do not want to do the
                 access check until after excluding the possibility of an
                 access adjustment declaration. */
              if (curr_token == tok_identifier ||
                  is_member_decl_start()) {
                error(ec_exp_colon);
              } else {
                syntax_error(ec_exp_colon);
              }  /* if */
            }  /* if */
          }  /* while */
          /* This next check catches cases like "...public: }". */
          if (curr_token == tok_rbrace) break;
        }  /* if */
        /* Scan a member declaration. */
        add_stop_token(tok_semicolon);
        if (C_dialect == C_dialect_cplusplus) {
          /* Check for and discard declarations of the form "overload f;". */
          if (check_for_overload_anachronism()) goto next_declaration;
          if (curr_token == tok_identifier &&
              !simplify_curr_class_qualified_name() &&
              get_qualified_name(IDL_NO_OPTIONS) &&
              next_token() == tok_semicolon) {
            /* This looks syntactically like an access adjustment declaration.
               Be sure the semantics are correct. */
            access_adjustment_decl(access, class_type);
            /* Advance to the semicolon and past it. */
            (void)get_token();
            (void)get_token();
            goto next_declaration;
          }  /* if */
        }  /* if */
        copy_source_position(pos_curr_token, decl_start_pos);
        member_type = NULL;
        member_storage_class = (a_storage_class)sc_unspecified;
        is_anonymous_union = FALSE;
        /* First scan the declaration specifiers.  In C++ the specifiers may
           be omitted, e.g., for a function member with implicit type. */
        add_stop_token(tok_colon);
        (void)decl_specifiers(dsi_flags, &dso_flags, &member_storage_class,
                              &member_type);
        dangling_type_specifier = dso_flags & DSO_DANGLING_TYPE_SPECIFIER;
        local_defines_something = dso_flags & DSO_DEFINES_SOMETHING;
        local_declares_something = dso_flags & DSO_DECLARES_SOMETHING;
        if (local_defines_something) {
#if CHECKING
          if (C_dialect == C_dialect_cplusplus) {
            /* Should be a nested class, struct, union, or enum definition.
               Be sure the parent class and access were marked correctly. */
            a_symbol_ptr sym = (a_symbol_ptr)(skip_typerefs(member_type)->
                                                   source_corresp.assoc_info);
            if (sym != NULL &&
                sym->class_of_which_a_member != class_type) {
             internal_error("class_specifier: bad parent type on nested type");
            } else if (member_type->source_corresp.access != access) {
              internal_error("class_specifier: bad access on nested type");
            } /* if */
          }  /* if */
#endif /* CHECKING */
          if (is_class_struct_union_type(member_type)) {
            symbol_supplement_for_class(class_type)->any_nested_classes = TRUE;
          }  /* if */
          /* Mark the IL entry for the nested class or enum as referenced. */
#if 0
          /* This is premature, since it isn't really referenced at this
             point. */
#endif /* if 0 */
          member_type->source_corresp.referenced = TRUE;
        } /* if */
        local_no_decl_specifiers = dso_flags & DSO_NO_DECL_SPECIFIERS;
        type_explicitly_specified =
                               dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER;
        friend_specified = dso_flags & DSO_FRIEND;
        virtual_specified = (dso_flags & DSO_VIRTUAL) != 0;
        inline_specified = (dso_flags & DSO_INLINE) != 0;
        is_constructor = dso_flags & DSO_CONSTRUCTOR;
        is_destructor = dso_flags & DSO_DESTRUCTOR;
        remove_stop_token(tok_colon);
        if (dangling_type_specifier) {
          /* A malformed declaration was detected by decl_specifiers.  Issue
             errors indicating that an identifier (= a declarator) is missing,
             along with a semicolon.  Then branch to the bottom of the loop. */
          set_err_pos_to_curr_token();
          error(ec_exp_identifier);
          error(ec_exp_semicolon);
          goto next_declaration;
        }  /* if */
        if (curr_token == tok_semicolon && C_dialect == C_dialect_cplusplus) {
          /* There's no declarator following the declaration specifier.  This
             is okay sometimes.  When it is, skip over declarator processing
             to the next declaration. */
          if (friend_specified) {
            if ((dso_flags & DSO_ELABORATED_TYPE_SPECIFIER) &&
                !is_enum_type(member_type)) {
              /* This is a friend class declaration, of the form:
                         friend class A;
                 (which is the only form the ARM (see 11.4) allows. */
              (void)decl_friend_class(class_type, member_type);
            } else {
              /* Invalid friend declaration. */
              pos_error(ec_bad_friend_decl, &decl_start_pos);
            }  /* if */
            (void)get_token();
            goto next_declaration;
          } else if (local_declares_something) {
            /* This is a free standing declaration of a class, struct, union,
               or enum type entry.  It will already have been recorded on the
               types list for the current class.  No need to complain about a
               missing identifier.  Just bypass the semicolon. */
#if 0
  /* Check for "inline", "const", "static", etc. here. */
#endif /* if 0 */
            (void)get_token();
            goto next_declaration;
          } else if (local_defines_something &&
                     member_type->kind == (a_type_kind)tk_union &&
                     member_storage_class != (a_storage_class)sc_typedef &&
                     is_unnamed_class_symbol((a_symbol_ptr)member_type->
                                                 source_corresp.assoc_info)) {
            is_anonymous_union = TRUE;
          }  /* if */
        }  /* if */
        /* A declarator list should be present.  Scan it. */
        first_declarator = TRUE;
        do {
          add_stop_token(tok_comma);
          add_stop_token(tok_colon);
          unnamed_field = FALSE;
          /* The declarator can be omitted for an unnamed bit-field. */
          set_err_pos_to_curr_token();
          if (curr_token == tok_colon) {
            /* Unnamed bit-field. */
            unnamed_field = TRUE;
            local_type = member_type;
          } else if (curr_token == tok_semicolon && first_declarator &&
                     C_dialect == C_dialect_pcc) {
            /* In pcc mode, the entire declarator list can be omitted to
               indicate an unnamed field.  It's a non-bit-field that forces
               padding. */
            unnamed_field = TRUE;
            local_type = member_type;
          } else if (is_anonymous_union) {
            /* There is no declarator. */
            local_type = member_type;
          } else {
            /* Named member. */
            a_decl_flag_set  declarator_input_flags, declarator_output_flags;

            /* Set the various flags for declarator processing. */
            declarator_input_flags = DI_REAL_DECLARATOR_ALLOWED;
            if (dso_flags & DSO_DESTRUCTOR) {
              declarator_input_flags |= DI_DESTRUCTOR_SPECIFIERS;
            }  /* if */
            if (dso_flags & DSO_CONSTRUCTOR) {
              declarator_input_flags |= DI_IS_CONSTRUCTOR;
            }  /* if */
            if (member_storage_class != (a_storage_class)sc_static) {
              declarator_input_flags |= DI_NONSTATIC_MEMBER;
            }  /* if */
            if (friend_specified) {
              declarator_input_flags |= DI_QUALIFIED_NAME_ALLOWED;
            }  /* if */
            /* Pass the class's type pointer to declarator if this might
               be a nonstatic member function, in which case its presence
               will cause an implicit "this" parameter type to be created.
               (Static member functions do not have an implicit "this" pointer.
               The class pointer will be ignored for data members.) */
            declarator(declarator_input_flags, &declarator_output_flags,
                       member_type,
                       friend_specified ? (a_type_ptr)NULL : class_type,
                       &locator, &local_type, &bottom_derived_type,
                       &func_info, &dim_expr_ptr);
            if (C_dialect == C_dialect_cplusplus) {
              /* Abstract class objects are prohibited (ARM 10.3). */
              if (member_storage_class != (a_storage_class)sc_typedef &&
                  is_illegal_abstract_class_type(local_type)) {
                pos_error(ec_abstract_class_object_not_allowed,
                          &locator.source_position);
              }  /* if */
            }  /* if */
          }  /* if */
          remove_stop_token(tok_colon);
          if (is_function_type(local_type) &&
               member_storage_class != (a_storage_class)sc_typedef) {
            if (C_dialect != C_dialect_cplusplus) {
              error(ec_function_type_not_allowed);
              local_type = error_type();
            } else {
              /* Member function. */
              a_boolean suppress_pure_specifier_error = FALSE;

              if (friend_specified) {
                if (virtual_specified ||
                    member_storage_class != (a_storage_class)sc_unspecified) {
                  /* A storage class declaration along with "friend" is not
                     allowed.  The ARM doesn't disallow it, but that's how
                     Cfront 2.1 works.  "inline", by the way, is allowed. */
                  pos_error(ec_bad_friend_decl, &decl_start_pos);
                  set_to_error_locator(locator);
                  if (virtual_specified) {
                    virtual_specified = FALSE;
                    suppress_pure_specifier_error = TRUE;
                  }  /* if */
                }  /* if */
              } else if (is_union_type(class_type)) {
                if (virtual_specified) {
                  /* Unions may not have virtual member functions. */
                  pos_error(ec_virtual_function_in_union, &decl_start_pos);
                  virtual_specified = FALSE;
                  suppress_pure_specifier_error = TRUE;
                }  /* if */
              } else if (virtual_specified &&
                         member_storage_class == (a_storage_class)sc_static) {
                /* Only nonstatic member functions may be specified as
                   virtual.  This is a kind of specifiers conflict, so just
                   issue the message once. */
                pos_error(ec_bad_virtual_decl, &decl_start_pos);
                virtual_specified = FALSE;
                suppress_pure_specifier_error = TRUE;
              }  /* if */
              if (local_defines_something && first_declarator) {
                /* Type definition in function return type. */
                pos_error(ec_type_def_not_allowed_in_func_type_decl,
                          &decl_start_pos);
              } else if (!type_explicitly_specified) {
                /* No type specifier. */
                if (is_constructor || is_destructor ||
                    locator.is_conversion_name) {
                  /* Type specifier is not expected (nor permitted) on
                     constructors, destructors, and conversion functions. */
                } else {
                  /* Type specifier is missing.  The type defaults to int,
                     but issue a warning. */
                  if (first_declarator) {
                    pos_warning(ec_missing_type_specifier, &decl_start_pos);
                  }  /* if */
                }  /* if */
              }  /* if */
              spec_kind = (a_special_function_kind)sfk_none;
              if (friend_specified) {
                rout_sym = decl_friend_function(&locator, class_type,
                                                local_type, inline_specified);
              } else {
                if (is_destructor) {
                  spec_kind = (a_special_function_kind)sfk_destructor;
                } else if (is_constructor) {
                  spec_kind = (a_special_function_kind)sfk_constructor;
                }  /* if */
                /* Create a symbol for the member function. */
                rout_sym = decl_member_function(&locator, class_type,
                                                local_type, access,
                                                inline_specified,
                                                virtual_specified,
                                                spec_kind);
              }  /* if */
              if (curr_token == tok_lbrace ||
                  (spec_kind == (a_special_function_kind)sfk_constructor &&
                   curr_token == tok_colon)) {
                /* Next token indications start of a function definition. */
#if CHECKING
                if (rout_sym->defined) {
                  internal_error("class_specifier: rout already defined");
                }  /* if */
#endif /* CHECKING */
                rout_sym->defined = TRUE;
                rout_sym->variant.routine->is_inline = TRUE;
                remove_stop_token(tok_comma);
                /* Cache the tokens comprising the function definition
                   so that they can be rescanned once the entire class
                   definition has been processed. */
                if (prescan_function_definition(rout_sym->variant.routine,
                                                &func_info)) {
                  /* Advance past the terminating right brace. */
                  (void)get_token();
                }  /* if */
                if (curr_token == tok_semicolon) {
                  /* Advance past the optional semicolon. */
                  (void)get_token();
                }  /* if */
                /* A comma-list of function definitions is not allowed. */
                goto next_declaration;
              } else {
                /* Not a function definition. */
                if (!friend_specified && is_local_class) {
                  /* A member function declared in a local class definition
                     (which is the current case) must be defined within the
                     class definition (ARM 9.8). */
                  error(ec_local_class_function_def_missing);
                }  /* if */
                if (curr_token == tok_assign) {
                  /* Look for a pure specifier ("= 0"), which may appear on
                     virtual functions. */
                  scan_pure_specifier(rout_sym, class_type,
                                      suppress_pure_specifier_error);
                  /* A comma-list of function definitions is not allowed. */
                  remove_stop_token(tok_comma);
                  /* Break out of the declarator loop. */
                  break;
                }  /* if */
                if (curr_token == tok_comma &&
                         (is_destructor || is_constructor)) {
                  /* Even if this is not a function definition, we disallow
                     a comma-separated list of constructor (or destructor)
                     declarations.  This is consistent with Cfront 2.1,
                     though the ARM is silent. */
                  pos_error(ec_exp_semicolon, &pos_curr_token);
                  (void)get_token();
                  remove_stop_token(tok_comma);
                  goto next_declaration;
                }  /* if */
              }  /* if */
            }  /* if */
          } else if (friend_specified) {
            pos_error(ec_bad_friend_decl, &decl_start_pos);
            remove_stop_token(tok_comma);
            break;
          } else if (virtual_specified) {
            pos_error(ec_bad_virtual_decl, &decl_start_pos);
            remove_stop_token(tok_comma);
            break;
          } else if (inline_specified) {
            pos_error(ec_inline_and_nonfunction, &decl_start_pos);
            remove_stop_token(tok_comma);
            break;
          } else if (is_destructor) {
            /* Error has already been issued if it wasn't processed as a
               function. */
          } else if (local_no_decl_specifiers) {
            /* A declaration in which the declaration specifiers are
               entirely omitted can only be a function declaration (ARM 9.2,
               p. 171). */
            pos_error(ec_missing_decl_specifiers, &decl_start_pos);
            remove_stop_token(tok_comma);
            break;
          } else if (member_storage_class == (a_storage_class)sc_typedef) {
            a_symbol_ptr        typedef_sym_ptr;
#if CHECKING
            if (C_dialect != C_dialect_cplusplus) {
              internal_error("decl_class: typedef not expected");
            }  /* if */
#endif /* CHECKING */
            if (!type_explicitly_specified) {
              warning(ec_missing_type_specifier);
            }  /* if */
            /* Typedef declaration. */
            decl_typedef(&locator, local_type, &typedef_sym_ptr);
            typedef_sym_ptr->class_of_which_a_member = class_type;
            typedef_sym_ptr->variant.type->source_corresp.access = access;
            typedef_sym_ptr->variant.type->
                          source_corresp.class_of_which_a_member = class_type;
          } else if (curr_token == tok_assign &&
                     is_const_qualified_type(local_type) &&
                     !is_volatile_qualified_type(local_type) &&
                     member_storage_class == (a_storage_class)sc_unspecified &&
                     C_dialect == C_dialect_cplusplus) {
            /* Provide support for the nonstandard declaration of a member
               constant -- e.g., "const int I = 2;". */
            decl_member_constant(&locator, class_type, local_type, access);
          } else {
            if (C_dialect == C_dialect_cplusplus) {
              if (!type_explicitly_specified && first_declarator) {
                warning(ec_missing_type_specifier);
              }  /* if */
            }  /* if */
            if (member_storage_class == (a_storage_class)sc_static) {
              /* Static data member. */
              if (is_union_type(class_type)) {
                pos_error(ec_static_member_in_union, &decl_start_pos);
              }  /* if */
              if (is_local_class) {
                /* Static data members are not allowed in local classes. */
                pos_error(ec_static_member_in_local_class, &decl_start_pos);
                /* Set the type for this invalid static member to error_type.
                   This will assure "proper" (or unobtrusive) behavior later,
                   if a definition is encountered.  It also eliminates semi-
                   spurious error messages if there are references to it. */
                local_type = error_type();
              }  /* if */
              decl_static_data_member(&locator, class_type,
                                      local_type, access);
            } else {
              /* Non-static data member (= field). */
              /* The type specified must be complete. */
              if (is_incomplete_type(local_type)) {
                /* As a C extension (but not C++), allow an incomplete array
                   as the last member of a struct.  It can't be the first
                   member, though. */
                if (C_dialect != C_dialect_cplusplus &&
                    class_type->kind == (a_type_kind)tk_struct &&
                    is_array_type(local_type) && !is_first_field &&
                    (curr_token == tok_rbrace ||
                     (curr_token == tok_semicolon &&
                      next_token() == tok_rbrace))) {
                    /* Okay. */
                  if (strict_ansi_mode) {
                    warning(ec_incomplete_type_not_allowed);
                  }  /* if */
                } else {
                  error(ec_incomplete_type_not_allowed);
                  local_type = error_type();
                }  /* if */
              }  /* if */
              decl_nonstatic_data_member(&locator, class_type, local_type,
                                         access, unnamed_field,
                                         is_anonymous_union, &byte_offset,
                                         &bit_offset, &alignment,
                                         &end_of_field_list, &any_overflow);
              is_first_field = FALSE;
            }  /* if */
            if (C_dialect == C_dialect_cplusplus) {
              /* Issue an error if there appears to be an attempt to
                 initialize a data member within the class definition. */
              if (curr_token == tok_assign) {
                copy_source_position(pos_curr_token, error_position);
                /* Issue a syntax error to flush to the comma or semicolon. */
                syntax_error(ec_bad_data_member_initialization);
              }  /* if */
            }  /* if */
          }  /* if */
          remove_stop_token(tok_comma);
          first_declarator = FALSE;
          /* Loop for additional declarators. */
        } while (loop_token(tok_comma));
        /* Check for and ignore the semicolon following the member declaration.
           It's optional after the last declaration (that's an extension in
           ANSI mode). */
        if (curr_token == tok_rbrace) {
          /* The final semicolon is omitted. */
          if (C_dialect != C_dialect_pcc) warning(ec_exp_semicolon);
        } else {
          (void)required_token(tok_semicolon, ec_exp_semicolon);
        }  /* if */
next_declaration:
        remove_stop_token(tok_semicolon);
        /* Keep processing member declarations until the closing brace. */
      } while (curr_token != tok_rbrace && curr_token != tok_end_of_source);
    }  /* if */
    /* Wrap up field allocation. */
    finish_laying_out_class(class_type, byte_offset, bit_offset, alignment,
                            any_overflow);
    /* Adding the type to the current scope's types list is done after
       reaching the closing brace to get the IL types list in the right
       order. */
    if (C_dialect != C_dialect_cplusplus &&
        tag_sym->reentered_from_prototype_scope) {
      /* If the tag was declared in a prototype scope and is now being resolved
         within the function, as in
           int f(struct f p) {struct f{int a;};  ... }
         we may assume the type entry has already been entered on the types
         list. */
    } else {
      /* Add the class type to the list for the current scope.  Note that
         incomplete structs/unions are not added to the type list (this code
         is bypassed) because the actual definition has not yet appeared.  See
         pop_scope; they get added at the end of the scope. */
      add_to_types_list(class_type, effective_decl_level,
                        in_old_style_param_decl_list);
    }  /* if */
    /* Save a pointer to the list of member symbols in the tag symbol.  Note
       that there may be symbols even if there there were no declarations,
       since symbols may be inherited. */
    cssp = tag_sym->variant.class_struct_union.extra_info;
    cssp->symbols = scope_stack[depth_scope_stack].symbols;
    if (C_dialect == C_dialect_cplusplus) {
      /* Create compiler-generated default constructor, copy constructor,
         destructor, and assignment operator, if any is needed. */
      check_special_member_functions(class_type);
      /* Since check_special_member_functions can have added new symbols or
         modified the head of the old list, update the symbols list attached
         to the class. */
      cssp->symbols = scope_stack[depth_scope_stack].symbols;
      /* Check for inherited conversion functions.  This must be done before
         rescanning inline function definitions. */
      project_base_class_conversion_functions(class_type);
      /* Since project_base_class_conversion_functions can have added new
         symbols, update the symbols list attached to the class. */
      cssp->symbols = scope_stack[depth_scope_stack].symbols;
      /* Report errors in virtual function declarations that result from
         the failure to redeclare a virtual function originally declared in
         a virtual base class. */
      copy_source_position(pos_curr_token, error_position);
      report_virtual_function_ambiguities(class_type);
      /* If the current class is not already marked as "abstract", run
         through its base classes to determine whether it is abstract by
         inheritance and set the flag accordingly. */
      check_abstract_class(class_type);
      /* Classes with no constructors, no private or protected members, no
         base classes, and no virtual functions are used to declare
         "aggregate" objects (ARM 8.4.1). */
      if (cssp->constructor == NULL && !cssp->any_nonpublic_members &&
          class_type->
              variant.class_struct_union.extra_info->base_classes == NULL) {
        cssp->is_class_aggregate = TRUE;
      }  /* if */
    }  /* if */
    /* Pop the pseudo-scope created for the fields. */
    pop_scope();
    remove_stop_token(tok_rbrace);
    /* Check for and ignore the closing brace. */
    (void)required_token(tok_rbrace, ec_exp_rbrace);
    /* Rescan tokens that were cached (inline function definitions, default
       arguments). */
    if (C_dialect == C_dialect_cplusplus &&
        tag_sym->class_of_which_a_member == NULL) {
      /* For non-nested classes do delayed processing for default argument
         declarations and inline member function definitions. */
      delayed_scan_fixup_for_class(tag_sym);
    }  /* if */
    /* If this is the resolution of a previously incomplete tag, and there
       is a list of array types to be resolved, look to see if any of them
       are arrays whose element type is this struct/union type.  (This
       handles an infrequently-used extension.) */
    if (tag_resolution) check_fixup_list_for_array_types();
    /* Switch back from the file scope memory region to whatever region
       was current upon entry. */
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
  *type_ptr = class_type;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(tag_sym, "tag_sym: ", 4);
  }  /* if */
#endif
  db_exit();
  return !err;
}  /* class_specifier */


static void make_class_externally_linked(a_type_ptr type,
                                         int        *count)
/*
This routine changes the linkage of a type and its components from
internal to external.  The types it handles directly are class, struct,
and union types, for which it sets the name_linkage field, adjusts members
as needed, and searches for other classes that are entailed in its
definition and marks them external as well.  The routine also deals with
types that are built up from a class type (pointer to a class, array of
class, routine with parameter pointer to class, etc.); these are handled
by recursive calls.
*/
{
  a_field_ptr                  fp;
  a_class_type_supplement_ptr  ctsp;
  a_base_class_ptr             bcp;
  a_routine_ptr                rp;
  a_variable_ptr               vp;
  a_type_ptr                   tp;
  a_param_type_ptr             ptp;
  a_symbol_ptr                 sym;

  type = skip_typerefs(type);
  switch (type->kind) {
    case tk_class:
    case tk_struct:
    case tk_union:
      /* Class, struct, or union type.  These are handled directly. */
      if (type->source_corresp.name_linkage ==
                               (a_name_linkage_kind)nlk_internal) {
        /* Mark the class as externally linked immediately, to avoid infinite
           recursion if it is self referential. */
        type->source_corresp.name_linkage =
                                 (a_name_linkage_kind)nlk_cplusplus_external;
        /* Increment the count.  This lets the caller know how many classes
           were changed from internal to external linkage. */
        (*count)++;
#if DEBUG
        if (debug_level >= 3) {
          fputs("external linkage given to class \"", f_debug);
          db_name(&type->source_corresp);
          fputs("\"\n", f_debug);
        }  /* if */
#endif /* DEBUG */
        /* Be sure any class types involved in the definitions of subobjects
           of the class are marked external, too. */
        /* Nonstatic data members (fields) first. */
        fp = type->variant.class_struct_union.field_list;
        for (; fp != NULL; fp = fp->next) {
          make_class_externally_linked(fp->type, count);
        }  /* for */
        /* Base classes. */
        ctsp = type->variant.class_struct_union.extra_info;
        bcp = ctsp->base_classes;
        for (; bcp != NULL; bcp = bcp->next) {
          make_class_externally_linked(bcp->type, count);
        }  /* for */
        if (ctsp->assoc_scope != NULL) {
          /* A routine entry for a member function needs not only a check of
             return and parameter types; it may also need its own linkage and
             storage class set properly. */
          rp = ctsp->assoc_scope->routines;
          for (; rp != NULL; rp = rp->next) {
            if (!rp->is_inline) {
              /* An inline member function remains internally linked even
                 when it is a member of an externally linked class.  But a
                 noninline function is always externally linked.  Storage
                 class depends on whether it was defined in the current
                 translation unit. */
              rp->source_corresp.name_linkage =
                                   (a_name_linkage_kind)nlk_cplusplus_external;
              if (rp->assoc_scope == NULL) {
                /* No routine body. */
                rp->storage_class = (a_storage_class)sc_extern;
              } else {
                /* Routine is defined in this file.  Mark it referenced in
                   case it's referenced in another file. */
                rp->storage_class = (a_storage_class)sc_unspecified;
                rp->source_corresp.referenced = TRUE;
              }  /* if */
#if DEBUG
              if (debug_level >= 3) {
                fputs("external linkage given to member function \"", f_debug);
                db_name(&rp->source_corresp);
                fputs("\"\n", f_debug);
              }  /* if */
#endif /* DEBUG */
            }  /* if */
            make_class_externally_linked(rp->type, count);
          }  /* for */
          /* A variable entry for a static data member will need to have its
             storage class and linkage reset.  In addition, its type
             must be checked. */
          vp = ctsp->assoc_scope->variables;
          for (; vp != NULL; vp = vp->next) {
            vp->source_corresp.name_linkage =
                                   (a_name_linkage_kind)nlk_cplusplus_external;
            sym = (a_symbol_ptr)vp->source_corresp.assoc_info;
            if (sym->defined) {
              vp->storage_class = (a_storage_class)sc_unspecified;
            } else {
              vp->storage_class = (a_storage_class)sc_extern;
            }  /* if */
            make_class_externally_linked(vp->type, count);
          }  /* if */
          /* Classes nested in the class should also be treated as having
             external linkage. */
          tp = ctsp->assoc_scope->types;
          for (; tp != NULL; tp = tp->next) {
            make_class_externally_linked(tp, count);
          }  /* if */
        }  /* if */
      }  /* if */
      break;
    case tk_routine:
      /* For routine types check both the return type and the types of each
         of the parameters. */
      make_class_externally_linked(type->variant.routine.return_type, count);
      ptp = type->variant.routine.extra_info->param_type_list;
      for (; ptp != NULL; ptp = ptp->next) {
        make_class_externally_linked(ptp->type, count);
      }  /* for */
      break;
    case tk_pointer:
      /* For pointer and reference types check the type pointed to. */
      make_class_externally_linked(type_pointed_to(type), count);
      break;
    case tk_array:
      /* For arrays check the element type. */
      make_class_externally_linked(array_element_type(type), count);
      break;
    case tk_ptr_to_member:
      /* For pointer-to-member type check both the class type and the member
         type.  Ordinarily this would be excessive, since the member type
         should be handled recursively when the class type is processed.  The
         the member type is handled independently to allow for the case where
         no member of that type exists. */
      tp = pm_class_type(type);
      make_class_externally_linked(tp, count);
      make_class_externally_linked(pm_member_type(type), count);
    default:
      /* Cannot have a class subtype. */
      break;
  }  /* switch */
}  /* make_class_externally_linked */


void check_class_linkage(void)
/*
This routine makes a pass over all the classes defined in this translation
unit to determine which ones need to be changed from internal to external
linkage and to make the change when appropriate.

Classes are internally linked (i.e., local to a translation unit) by
default, but they become externally linked for one of two reasons: either
they have members that are external by default, or they are used in a way
that requires external linkage.  To be more specific, if a class has any
noninline member functions or any nonstatic data members it is externally
linked; or, it is is used in the declaration of any externally linked
class, variable, or routine it is externally linked (ARM 3.3).

Note that local classes have no linkage.  They are not changed to external
linkage.

This routine first examines each class for members that would require it
to be external.  When it finds one, it calls make_class_externally_linked,
which sets the name_linkage field in the class, adjusts members as needed,
and searches for other classes that are entailed in its definition and so
must themselves be marked external.

If, once this is done, any classes remain that are have internal linkage,
this routine goes on to determine whether they must be made external
because they were used in declaring an external function or variable.
*/
{
  int            num_internally_linked_classes = 0, count;
  a_scope_ptr    scope = il_header.primary_scope, class_scope;
  a_type_ptr     tp;
  a_routine_ptr  rp;
  a_variable_ptr vp;
  a_boolean      external;

  db_enter(3, "check_class_linkage");
  /* Search for classes by making a pass over all the types associated with
     the file scope. */
  scope = il_header.primary_scope;
  for (tp = scope->types; tp != NULL; tp = tp->next) {
    if (tp->kind != (a_type_kind)tk_typeref &&
        is_class_struct_union_type(tp)) {
      /* Found a class. */
      class_scope = tp->variant.class_struct_union.extra_info->assoc_scope;
      if (tp->source_corresp.name_linkage ==
                               (a_name_linkage_kind)nlk_internal &&
          class_scope != NULL) {
        /* Internally linked and defined.  Check for non-inlined functions
           and static data members. */
        external = FALSE;
        if (class_scope->variables != NULL) {
          /* At least one static data member: external linkage is required. */
          external = TRUE;
        } else {
          for (rp = class_scope->routines; rp != NULL; rp = rp->next) {
            if (!rp->is_inline && !rp->pure_virtual) {
              /* At least one noninline member function: external linkage is
                 required. */
              external = TRUE;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
        if (external) {
          /* Make the class externally linked and propagate this external
             linkage into its members and the classes referenced in
             declaring the members. */
          int dummy_count = 0;
          make_class_externally_linked(tp, &dummy_count);
        } else {
          /* Keep track of the fact that at least one class was encountered
             that did not require external linkage on the basis of its
             members. */
          num_internally_linked_classes = 1;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  if (num_internally_linked_classes > 0) {
    /* At least one class may have retained internal linkage after the
       previous scan.  Since it may have been subsequently referenced in
       declaring another class, it may have had its linkage changed to
       external after all; we need to make another pass over the class type
       entries to get an accurate count.  We keep track of the actual number
       of internally linked classes that remain so that we can stop looking
       at routines and variables as soon as possible. */
    for (tp = scope->types; tp != NULL; tp = tp->next) {
      if (tp->kind != (a_type_kind)tk_typeref &&
          is_class_struct_union_type(tp) &&
          tp->source_corresp.name_linkage ==
                             (a_name_linkage_kind)nlk_internal) {
        num_internally_linked_classes++;
      }  /* if */
    }  /* for */
  }  /* if */
  if (num_internally_linked_classes > 0) {
    /* There is at least one internally linked class.  Make a pass over all
       the variables defined at file scope to determine whether the
       declaration of an externally linked variable entails a reference to
       a class that is still marked as internally linked. */
    for (vp = scope->variables; vp != NULL; vp = vp->next) {
      if (vp->storage_class != (a_storage_class)sc_static) {
        /* This is an externally linked variable.  Check its type. */
        count = 0;
        make_class_externally_linked(vp->type, &count);
        /* "count" is returned as the number of internally linked classes
           that were changed to externally linked.  Adjust the number of
           internally linked classes remaining.  When it gets down to zero
           we can bail out. */
        num_internally_linked_classes -= count;
        if (num_internally_linked_classes < 1) break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (num_internally_linked_classes > 0) {
    /* There is still at least one internally linked class.  Make a pass over
       the file scope routine entries similar to the one made for variables. */
    for (rp = scope->routines; rp != NULL; rp = rp->next) {
      if (rp->storage_class != (a_storage_class)sc_static) {
        /* This is an externally linked routine.  Check its type. */
        count = 0;
        make_class_externally_linked(rp->type, &count);
        /* Again, we can bail out when the number of internally linked classes
           is reduced to zero. */
        num_internally_linked_classes -= count;
        if (num_internally_linked_classes < 1) break;
      }  /* if */
    }  /* for */
  }  /* if */
  db_exit();
}  /* check_class_linkage */


void class_decl_init(void)
/*
Initializations for class declaration processing.
*/
{
  /* Initialize the list of freed delayed-scan-fixup entries. */
  avail_delayed_scan_fixup = NULL;
  /* Initialize the list of freed derivation-step entries. */
  avail_derivation_steps = NULL;
  return;
}  /* class_decl_init */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
