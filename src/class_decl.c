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
#include "debug.h"
#include "def_arg.h"
#include "decls.h"
#include "il.h"
#include "layout.h"
#include "symbol_tbl.h"
#include "statements.h"
#include "lexical.h"
#include "lower_il.h"
#include "error.h"
#include "expr.h"
#include "exprutil.h"
#include "lang_feat.h"
#include "cmd_line.h"
#include "types.h"
#include "mem_tables.h"
#include "mem_manage.h"
#include "expr.h"
#include "target.h"
#include "templates.h"
#include "decl_inits.h"
#include "preproc.h"
#include "const_ints.h"
#if ASM_FUNCTION_ALLOWED
#include "asm_func.h"
#endif /* ASM_FUNCTION_ALLOWED */


/*
Structure for keeping track of fixup information for a particular member
function, including both the cached tokens comprising default argument
expressions of its parameters and the cached tokens comprising the function
body, if it is defined inline.
*/
/* a_routine_fixup_ptr is already defined in symbol_tbl.h. */
typedef struct a_routine_fixup {
  a_routine_fixup_ptr
		next;
			/* Next in a linked list of routine fixup blocks,
			   each of which is associated with a particular
			   member function of a given class. */
  a_routine_ptr routine;
			/* Pointer to the routine entry to which the fixup
			   applies. */
  a_func_info_block
		func_info;
			/* Information saved by declarator processing for
			   use in function definition processing. */
  a_def_arg_expr_fixup_ptr
		def_arg_expr_fixup_list;
			/* List of entries describing default argument
			   expression associated with parameters for the
			   current routine. */
  a_token_cache function_body_token_cache;
			/* A pointer to the token cache that describes the
			   function body. */
} a_routine_fixup;

/* The routine fixup entry for the current class member declaration. */
static a_routine_fixup_ptr curr_routine_fixup;

/* Previously allocated fixup entries available for reuse. */
static a_routine_fixup_ptr avail_routine_fixup;


#if DEBUG
/*
Counter to track total use of memory.
*/
static unsigned long
		num_routine_fixups_allocated;

unsigned long db_show_routine_fixups_used(unsigned long grand_total)
{
  unsigned long  num, size, total;

  db_space_used_lost("routine fixups", avail_routine_fixup,
                     num_routine_fixups_allocated, a_routine_fixup);
  return grand_total;
}  /* db_show_routine_fixups_used */
#endif /* DEBUG */


static a_routine_fixup_ptr alloc_routine_fixup(void)
/*
Allocate (or take from the available-list) a routine fixup entry and
initialize it.
*/
{
  a_routine_fixup_ptr  rfp;
  
  if (avail_routine_fixup != NULL) {
    /* Reuse a previously allocated entity. */
    rfp = avail_routine_fixup;
    avail_routine_fixup = rfp->next;
  } else {
    /* Allocate memory for a new entity. */
    rfp = (a_routine_fixup_ptr)alloc_fe(sizeof(a_routine_fixup));
#if DEBUG
    num_routine_fixups_allocated++;
#endif /* DEBUG */
  }  /* if */
  /* Clear the entity. */
  rfp->next = NULL;
  rfp->routine = NULL;
  rfp->def_arg_expr_fixup_list = NULL;
  clear_func_info(&rfp->func_info);
  /* We don't know whether this cache will be reused or not.  Make it
     reusable here.  If it is rescanned as a nonreusable cache we
     will change it later. */
  clear_token_cache(&rfp->function_body_token_cache, /*reusable=*/TRUE);

  return rfp;
}  /* alloc_routine_fixup */


static void free_routine_fixup(a_routine_fixup_ptr  rfp)
/*
Return a routine fixup entry, along with any default arg expr fixup entries
associated with it, to their respective available-lists.
*/
{
  free_def_arg_expr_fixup(rfp->def_arg_expr_fixup_list);
  rfp->def_arg_expr_fixup_list = NULL;
  rfp->next = avail_routine_fixup;
  avail_routine_fixup = rfp;
}  /* free_routine_fixup */


static void add_to_routine_fixup_list(a_routine_fixup_ptr      rfp)
/*
Add a routine fixup entry to the end of the list for the class associated
with the indicated scope stack entry.
*/
{
  a_scope_stack_entry  *ssep = &scope_stack[depth_scope_stack];

  check_assertion(ssep->il_scope->kind ==
                                   (a_scope_kind)sck_class_struct_union);
  if (ssep->last_routine_fixup == NULL) {
    (symbol_supplement_for_class(ssep->assoc_type))->routine_fixup_list = rfp;
  } else {
    ssep->last_routine_fixup->next = rfp;
  }  /* if */
  ssep->last_routine_fixup = rfp;
}  /* add_to_routine_fixup_list */


static a_boolean prescan_function_definition(void)
/*
Place the tokens for a function definition (including, perhaps, the
constructor initializer) into a token cache, to await actual processing
at a later point.  The current token is either a left brace or, when a
constructor initializer is present, a colon.
*/
{
  a_token_cache             token_cache;
  a_stop_token_array        save_stop_token_array;
  a_boolean                 success = FALSE;

  db_enter(3, "prescan_function_definition");

  /* We don't know whether this cache will be reused or not.  Make it
     reusable here.  If it is rescanned as a nonreusable cache we
     will change it later. */ 
 clear_token_cache(&token_cache, /*reusable=*/TRUE);
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
    cache_token_stream(&token_cache);
    remove_stop_token(tok_lbrace);
    remove_stop_token(tok_semicolon);
  }  /* if */
  if (curr_token == tok_lbrace) {
    /* The left brace marks the start of the function body.  Cache all the
       tokens up to the right brace. */
    cache_curr_token(&token_cache);
    (void)get_token();
    cache_token_stream(&token_cache);
  }  /* if */
  remove_stop_token(tok_rbrace);
  if (curr_token == tok_rbrace) {
    cache_curr_token(&token_cache);
    success = TRUE;
  }  /* if */
  /* Add an end-of-source token to the end of the token cache.  This assures
     that we won't scan past the end of the cache in the actual scan. */
  terminate_token_cache(&token_cache);
  /* Restore the original stop token state. */
  copy_stop_tokens(save_stop_token_array, stop_token_array);
  if (curr_routine_fixup == NULL) {
    /* We must be within a prototype instantiation for a class template.  Just
       throw away the cached tokens.  (We do not scan the bodies of inline
       member functions during prototype instantiation.). */
    discard_token_cache(&token_cache);
  } else {
    /* Record the token cache info in the routine-fixup entry for the current
       member function. */
    curr_routine_fixup->function_body_token_cache = token_cache;
  }  /* if */
  db_exit();
  return success;
}  /* prescan_function_definition */


void prescan_member_function_default_arg_expr(a_param_type_ptr  ptp)
/*
Scan a default argument expression and link the default argument
entry onto a list in the current routine fixup entry.
*/
{
  a_def_arg_expr_fixup_ptr	*list;
  if (curr_routine_fixup == NULL) {
    /* We must be within a prototype instantiation for a class template.
       Pass a NULL list pointer to indicate that we should throw away the
       cached tokens.  (We do not scan the default argument expression
       when it appears in a prototype instantiation; it is only scanned
       during real instantiations.) */
    list = NULL;
  } else {
    list = &curr_routine_fixup->def_arg_expr_fixup_list;
  }  /* if */
  prescan_default_function_arg_expr(ptp, list);
}  /* prescan_member_function_default_arg_expr */


static void delayed_scan_fixup_for_class(a_symbol_ptr  class_sym,
                                         a_boolean     is_template_based)
/*
Process the default argument expressions and inline function definitions
for the indicated class.  If the class contains nested classes, call this
routine recursively for each nested class.
*/
{
  a_routine_fixup_ptr               rfp, next_rfp;
  a_def_arg_expr_fixup_ptr          daefp;
  a_type_ptr                        class_type;
  a_class_symbol_supplement_ptr     cssp;
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp = NULL;
  a_boolean                         is_real_template_instantiation = FALSE;
  a_boolean                         is_nonreal_template_instantiation = FALSE;
  a_boolean                         is_friend;

  db_enter(3, "delayed_scan_fixup_for_class");
  cssp = class_sym->variant.class_struct_union.extra_info;
  if (cssp->is_nonreal_class) {
    is_nonreal_template_instantiation = TRUE;
  } else if (is_template_based) {
    is_real_template_instantiation = TRUE;
  }  /* if */
  /* Process nested classes first. */
  if (cssp->any_nested_classes) {
    for (sym = cssp->symbols; sym != NULL; sym = sym->next_in_scope) {
      if (sym->kind == (a_symbol_kind)sk_class_or_struct_tag ||
          sym->kind == (a_symbol_kind)sk_union_tag) {
        delayed_scan_fixup_for_class(sym, is_template_based);
      }  /* if */
    }  /* for */
  }  /* if */
  /* Do processing for the current class only if there are tokens cached for
     delayed scanning ("rewriting"). */
  rfp = cssp->routine_fixup_list;
  if (rfp != NULL) {
    /* There is at least one fixup entry. */
    class_type = skip_typerefs(class_sym->variant.class_struct_union.type);
#if DEBUG
    if (debug_level >= 3) {
      fputs("delayed scan fixup for ", f_debug);
      db_type_name(class_type);
      fputc('\n', f_debug);
    }  /* if */
#endif /* DEBUG */
    /* Reactivate the class. */ 
    push_class_reactivation_scope(class_type);
    /* Go through all the routine fixup entries created for the class twice,
       once for the default arguments, then for the function bodies.  This
       is desirable to control dependencies, e.g.:
         class A {
           void f1() { f2(); }
           void f2(int i=1) {}
         };
       Here we want to know about the default arguments for f2 before
       processing the call to it in the body of f1. */
#if 0
    /* This does not, however, fix dependency problems in a case like this:
         class A {
           int f1(int i=f2()) { return i; }
           static int f2(int i=1) { return i; }
         };
    */
#endif /* if 0 */
    /* First go though the routine fixup entries and scan the default
       argument expressions. */
    for (; rfp != NULL; rfp = rfp->next) {
      daefp = rfp->def_arg_expr_fixup_list;
      if (daefp != NULL) {
        /* There is at least one default argument associated with this
           function. */
        sym = (a_symbol_ptr)rfp->routine->source_corresp.assoc_info;
        is_friend = (sym->class_of_which_a_member != class_type);
        if ((is_real_template_instantiation && !is_friend) ||
            (is_nonreal_template_instantiation && is_friend)) {
          /* The token cache should be discarded for friend declarations in
             prototype instantiations and for everything but friends in
             real template instantiations. */
          for (; daefp != NULL; daefp = daefp->next) {
            discard_token_cache(&daefp->token_cache);
          }  /* for */
          if (is_real_template_instantiation) {
            /* Scan the default arguments associated with the template for this
               function. */
            /* Get the template symbol from the instance pointer. */
            sym = sym->variant.routine.instance_ptr->template_sym;
            tssp = sym->variant.routine.instance_ptr->template_info;
            delayed_scan_for_function_template_default_args
                        (tssp->variant.function.routine, rfp->routine, tssp);
          }  /* if */
        } else if (is_nonreal_template_instantiation) {
          a_def_arg_expr_fixup_ptr    daefp_end;

          /* Link the default argument list from the template supplement
             onto the end of the list of current default arguments.  The
             list in the supplement must be for arguments that follow the
             new list (otherwise it would be an error).  Find the end
             of the current list and link the existing list to the end. */
          daefp_end = daefp;
          if (daefp_end != NULL) {
            while (daefp_end->next != NULL) daefp_end = daefp_end->next;
            tssp = sym->variant.routine.instance_ptr->template_info;
            daefp_end->next = tssp->variant.function.def_arg_expr_list;
            tssp->variant.function.def_arg_expr_list = daefp;
          }  /* if */
          /* Make sure no further processing will be done here and
             make sure that the list isn't freed. */
          daefp = NULL;
          rfp->def_arg_expr_fixup_list = NULL;
        } else {
          /* A friend function declaration in a real template instantiation
             or just a normal (nontemplate) class. */
          /* The function prototype scope should be reactivated and its symbols
             reentered because parameter names hide names from enclosing scopes
             and, moreover, may not be used in default argument expressions
             (ARM 8.2.6). */
          (void)push_scope((a_scope_kind)sck_func_prototype,
                           rfp->func_info.scope_number, (a_type_ptr)NULL,
                           (a_routine_ptr)NULL, (a_symbol_ptr)NULL,
                           (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL);
          if (rfp->func_info.prototype_scope_symbols != NULL) {
            reactivate_prototype_scope_symbols(
                                      rfp->func_info.prototype_scope_symbols);
          }  /* if */
          /* Loop through the list of default arg expression fixup entries. */
          for (; daefp != NULL; daefp = daefp->next) {
            /* It's a default arg expression that needs to be rescanned. */
            /* Let get_token know about the cache. */
            rescan_cached_tokens(&daefp->token_cache);
            delayed_scan_of_default_arg_expr(daefp->param_type);
          }  /* for */
          /* Restore the prototype scope symbols pointer in the func info
             block. It shouldn't have changed, but we do it to be safe. */
          rfp->func_info.prototype_scope_symbols =
                                       scope_stack[depth_scope_stack].symbols;
          /* Pop the reactivated function prototype scope off the stack. */
          pop_scope();
        }  /* if */
      }  /* if */
    }  /* for */
    /* Now go through the routine fixup entries a second time to scan inline
       function bodies. */
    for (rfp = cssp->routine_fixup_list; rfp != NULL; rfp = next_rfp) {
      if (rfp->function_body_token_cache.first_token != NULL) {
        sym = (a_symbol_ptr)rfp->routine->source_corresp.assoc_info;
        is_friend = (sym->class_of_which_a_member != class_type);
        if ((is_real_template_instantiation && !is_friend) ||
            (is_nonreal_template_instantiation && is_friend)) {
          /* Discard the token cache for member functions of template
             classes -- instantiate_function_template does its thing based
             on the tokens saved during prototype instantiation.  Also,
             there is no reason to preserve the tokens for friend functions
             during prototype instantiation. */
          discard_token_cache(&rfp->function_body_token_cache);
        } else if (is_nonreal_template_instantiation) {
          /* Prototype instantiation -- copy the cache for member functions. */
          tssp = sym->variant.routine.instance_ptr->template_info;
          tssp->token_cache = rfp->function_body_token_cache;
          clear_token_cache(&rfp->function_body_token_cache,
                           /*reusable=*/TRUE);
          /* Also copy the func_info block. */
          tssp->variant.function.func_info = rfp->func_info;
        } else {
          /* Normal case. */
          /* Let get_token know about the cache. */
          rescan_cached_tokens(&rfp->function_body_token_cache);
          inline_function_definition(rfp->routine, &rfp->func_info);
          /* In the normal case the current token should be end_of_source,
             which was inserted to mark the end of the cached token stream.
             If necessary, keep flushing until end-of-source is found. */
          while (curr_token != tok_end_of_source) (void)get_token();
          /* Advance past the end-of-source token, which was added in
             the prescan routine. */
          (void)get_token();
        }  /* if */
      }  /* if */
      /* Advance to the next routine fixup entry before freeing the current
         one (returning it and any expr fixup entries attached to it to their
         respective available-lists). */
      next_rfp = rfp->next;
      free_routine_fixup(rfp);
    }  /* for */
    /* Pop the reactivated class scope from the scope stack. */
    pop_class_reactivation_scope();
    /* The delayed scan fixup entries have been freed, so clear the
       pointer in the class symbol supplement. */
    cssp->routine_fixup_list = NULL;
  }  /* if */
  db_exit();
}  /* delayed_scan_fixup_for_class */


#if DEBUG
static void db_virtual_function_override(
                                       an_overriding_virtual_function_ptr ovfp)
/*
Dump a virtual function override entry, for debug purposes.
*/
{
  fputs("  virtual function ", f_debug);
  db_name(&ovfp->primary_function->source_corresp);
  fputs(" overridden by ", f_debug);
  db_name(&ovfp->overriding_function->source_corresp);
  fputs(", type =\n    ", f_debug);
  db_type(ovfp->overriding_function->type);
  (void)fputc('\n', f_debug);
}  /* db_virtual_function_override */


static void db_virtual_function_override_list(a_base_class_ptr  bcp)
/*
Dump a base class's list of overriding virtual functions, for debug purposes.
*/
{
  an_overriding_virtual_function_ptr ovfp = bcp->overriding_virtual_functions;
  for (; ovfp != NULL; ovfp = ovfp->next) {
    db_virtual_function_override(ovfp);
  }  /* for */
}  /* db_virtual_function_override_list */


void db_all_virtual_function_override_lists(a_type_ptr  class_type)
/*
Dump the virtual function override lists for a class, by base class.
*/
{
  a_base_class_ptr  bcp;

  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
    if (bcp->overriding_virtual_functions != NULL) {
      fputs("virtual function override list for base class \"", f_debug);
      db_type_name(bcp->type);
      fputs("\" in class \"", f_debug);
      db_type_name(class_type);
      fputs("\":\n", f_debug);
      db_virtual_function_override_list(bcp);
    }  /* if */
  }  /* for */
}  /* db_all_virtual_function_override_lists */


static void db_virtual_function_number_sequence(a_base_class_ptr  bcp)
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


static void report_virtual_function_ambiguities(a_type_ptr class_type)
/* 
Report errors in virtual function declarations that result from the failure
to redeclare a virtual function originally declared in a virtual base class.

The situation we are looking for (discussed in ARM 10.10c) is of this sort:
	class A { virtual int f(); };
        class B : virtual A { int f(); };
        class C : virtual A { int f(); };
        class D : B, C { };
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
functions would remain.  When this routine finds two or more overriding
virtual function entries that override the same function, it reports the
ambiguity.
*/
{
  a_base_class_ptr                    bcp;
  an_overriding_virtual_function_ptr  ovfp;
  a_routine_ptr                       vfp;
  a_boolean                           is_nonreal_instantiation;

  db_enter(4, "report_virtual_function_ambiguities");
  is_nonreal_instantiation =
                    symbol_supplement_for_class(class_type)->is_nonreal_class;
  /* Make a pass over all the base classes (direct and indirect both) of
     the class indicated by class_type. */
  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
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
         function entry to that of its successor. */
      if (ovfp->next == NULL) break;
      vfp = ovfp->primary_function;
      if (ovfp->next->primary_function == vfp) {
        /* The virtual functions (member functions of the base class to which
           bcp refers) are the same -- i.e., both ovfp and ovfp->next
           represent an override of the same function. */
        if (!is_nonreal_instantiation) {
          a_symbol_ptr sym = (a_symbol_ptr)vfp->source_corresp.assoc_info;
          sym_error(ec_ambiguous_virtual_function_override, sym);
        }  /* if */
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
  a_class_type_supplement_ptr         bctsp;
  a_routine_ptr                       rp;
  an_overriding_virtual_function_ptr  ovfp;
  a_boolean                           override_order_matches_routine_order;

  db_enter(4, "check_abstract_class");
  if (class_type->variant.class_struct_union.abstract) {
    /* The class is already marked "abstract", presumably as a result of
       having one or more pure virtual member functions. */
  } else {
    /* The class was not already marked "abstract".  Go through its base
       classes to look for a pure virtual function that is inherited without
       an intervening declaration that overrides it. */
    for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
      if (bcp->type->variant.class_struct_union.any_pure_virtual_functions) {
        /* This base class *is* abstract, with at least one pure virtual
           function.  For each of the base class's pure virtual functions,
           inspect the appropriate virtual function function override list.
           If the pure virtual function is not overridden (i.e., if no entry
           on the override list points to it as the primary function) then
           mark the current class as abstract.  We can optimize the search by
           taking advantage of the fact that, unless bcp shares its virtual
           function info with one of its own base classes, the override entries
           and the routine entries will be in virtual-function-number order. */
        bctsp = bcp->type->variant.class_struct_union.extra_info;
        rp = bctsp->assoc_scope->routines;
        override_order_matches_routine_order =
                            (bctsp->virtual_function_info_base_class == NULL);
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
            if (!override_order_matches_routine_order) {
              /* Restart the search through the overriding virtual functions
                 list at the head of the list. */
              ovfp = bcp->overriding_virtual_functions;
            }  /* if */
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
    db_type_name(base_class->type);
    fputs(": ", f_debug);
    db_virtual_function_number_sequence(base_class);
    (void)fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* insert_in_virtual_function_override_list */


static a_boolean overriding_virtual_function_lists_correspond(
                                    an_overriding_virtual_function_ptr  list1,
                                    an_overriding_virtual_function_ptr  list2)
/*
Return TRUE if the lists of overriding virtual functions, list1 and list2,
correspond item for item (that is, if entries in the same position of the
lists refer to the same virtual function override).
*/
{
  a_boolean  lists_correspond = TRUE;

  while (list1 != NULL || list2 != NULL) {
    if (list1 == NULL || list2 == NULL ||
        list1->primary_function != list2->primary_function ||
        list1->overriding_function != list2->overriding_function) {
      lists_correspond = FALSE;
      break;
    }  /* if */
    list1 = list1->next;
    list2 = list2->next;
  }  /* while */
  return lists_correspond;
}  /* overriding_virtual_function_lists_correspond */


static a_boolean already_on_overriding_virtual_function_list(
                                     a_base_class_ptr                    bcp,
                                     an_overriding_virtual_function_ptr  ovfp)
{
  a_boolean                           already_on_list = FALSE;
  an_overriding_virtual_function_ptr  ovfp_from_list;

  for (ovfp_from_list = bcp->overriding_virtual_functions;
       ovfp_from_list != NULL;
       ovfp_from_list = ovfp_from_list->next) {
    if (ovfp_from_list->primary_function == ovfp->primary_function &&
        ovfp_from_list->overriding_function == ovfp->overriding_function) {
      already_on_list = TRUE;
    }  /* if */
  }  /* for */
  return already_on_list;
}  /* already_on_overriding_virtual_function_list */


static void copy_virtual_function_override_list(a_base_class_ptr  old_bcp,
                                                a_base_class_ptr  new_bcp,
                                                a_type_ptr        old_class,
                                                a_type_ptr        new_class)
/*
Copy the list of overriding virtual functions associated with old_bcp and add
each of the copies to the list belonging to new_bcp.  The entries are
being copied from a base class of old_class to a base class of new_class.
new_bcp is the base class being created in new_class.
*/
{
  an_overriding_virtual_function_ptr  ovfp, new_ovfp, old_list;
  a_boolean                           check_new_list;

  db_enter(4, "copy_virtual_function_override_list");
  old_list = old_bcp->overriding_virtual_functions;
  if (old_list != NULL) {
    if (new_bcp->overriding_virtual_functions == NULL) {
      check_new_list = FALSE;
    } else if (overriding_virtual_function_lists_correspond(
                       old_list, new_bcp->overriding_virtual_functions)) {
      goto done;
    } else {
      check_new_list = TRUE;
    }  /* if */
    /* Make a pass over the existing list. */
    for (ovfp = old_list; ovfp != NULL; ovfp = ovfp->next) {
      if (!check_new_list ||
          !already_on_overriding_virtual_function_list(new_bcp, ovfp)) {
        /* Allocate a new entry and copy fields from the original. */
        new_ovfp = alloc_overriding_virtual_function();
        new_ovfp->primary_function = ovfp->primary_function;
        new_ovfp->overriding_function = ovfp->overriding_function;
        /* The base class of the overriding function must be translated into
           the new class. */
        if (ovfp->base_class == NULL) {
          new_ovfp->base_class = find_direct_base_class_of(new_class,
                                                           old_class);
        } else {
          new_ovfp->base_class = 
                         corresponding_base_class(ovfp->base_class, new_class,
                                                  (a_base_class_ptr)NULL);
        }  /* if */
#if DEBUG
        if (debug_level >= 4) {
          fputs("copy for base class ", f_debug);
          db_type_name(new_bcp->type);
          fputs(": ", f_debug);
          db_virtual_function_override(ovfp);
        }  /* if */
#endif /* DEBUG */
        /* Add it to the new list. */
        insert_in_virtual_function_override_list(new_bcp, new_ovfp);
      }  /* for */
    }  /* if */
  }  /* if */
done:;
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


static a_boolean return_types_are_override_compatible(
                                        a_type_ptr  type_of_overriding_routine,
                                        a_type_ptr  type_of_overridden_routine)
/*
It is an error for the return type of an overriding virtual function to
differ from the return type of the function that is overridden -- unless
they are both pointers or both references to class types where the class
associated with the overriding function is publicly derived from the
other class (WP 10.2, but not in the ARM).  This routine does the checking
required.
*/
{
  a_type_ptr             tp1, tp2;
  a_boolean              compatible = FALSE;

  db_enter(4, "return_types_are_override_compatible");
  tp1 = type_of_overriding_routine->variant.routine.return_type;
  tp2 = type_of_overridden_routine->variant.routine.return_type;
  if (types_are_compatible(tp1, tp2)) {
    /* The types are "simply" compatible.  No further checking is required. */
    compatible = TRUE;
  } else if (is_or_contains_template_param(tp1) ||
             is_or_contains_template_param(tp2)) {
    /* We must be within a prototype instantiation.  The types may be
       compatible depending on the template argument in a real instantiation,
       so issue no error now. */
    compatible = TRUE;
#if 0
/* IL lowering is not ready for the rest of this routine yet. */
  } else {
    /* They're not "simply" compatible.  Do the other checking. */
    a_base_class_ptr       bcp;
    a_derivation_step_ptr  dsp;

    if ((is_reference_type(tp1) && is_reference_type(tp2)) ||
        (is_pointer_type(tp1) && is_pointer_type(tp2))) {
      /* Both types are references or both are pointers. */
      tp1 = type_pointed_to(tp1);
      tp2 = type_pointed_to(tp2);
      if (is_class_struct_union_type(tp1) && is_class_struct_union_type(tp2)) {
        /* The types referenced/pointed to are both classes.  See if the
           class associated with the overridden function is a base class
           of the class associated with the overriding function. */
        bcp = find_base_class_of(tp1, tp2);
        if (bcp != NULL) {
          /* One is a base class of the other.  Just be sure it's a publicly
             accessible base class by checking the access on the base class
             at each step of the derivation. */
          for (dsp = bcp->derivation; dsp != NULL; dsp = dsp->next) {
            if (dsp->base_class->access != (an_access_specifier)as_public) {
              /* At least on step in the derivation is inaccessible, so the
                 conditions for "override compatibility" are not satisfied. */
              goto done;
            }  /* if */
          }  /* for */
          compatible = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
done:;
#endif /* if 0 */
  }  /* if */
  db_exit();
  return compatible;      
}  /* return_types_are_override_compatible */


static a_boolean shares_virtual_function_info(a_type_ptr        class_type,
                                              a_base_class_ptr  bcp)
/*
bcp points to a base class of class_type.  If class_type shares its virtual
function info with a base class and that base class is bcp or the base class
with which bcp shares its virtual function info, return TRUE.
*/
{
  a_boolean         shares = FALSE;
  a_base_class_ptr  virtual_function_info_base_class;

  virtual_function_info_base_class =
                          class_type->variant.class_struct_union.extra_info->
                                              virtual_function_info_base_class;
  if (virtual_function_info_base_class == bcp) {
    shares = TRUE;
  } else if (virtual_function_info_base_class != NULL) {
    bcp = bcp->type->variant.class_struct_union.extra_info->
                                              virtual_function_info_base_class;
    if (bcp != NULL) {
      bcp = corresponding_base_class(bcp, class_type, (a_base_class_ptr)NULL);
      if (virtual_function_info_base_class == bcp) {
        shares = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return shares;
}  /* shares_virtual_function_info */


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
  a_boolean                    is_nonreal_instantiation;
  a_base_class_ptr             bcp;
  a_symbol_ptr                 symbol_list, sym, sym_next;
  a_routine_ptr                rout, rp;
  a_scope_ptr                  base_class_scope;
  a_class_type_supplement_ptr  ctsp;
  a_virtual_function_number    virtual_function_number = 0;

  db_enter(4, "check_for_virtual_function");
  is_virtual = virtual_specified;
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (rout_sym->kind == (a_symbol_kind)sk_function_template) {
    rout = rout_sym->variant.template_info->variant.function.routine;
    is_nonreal_instantiation = TRUE;
    /* If a member function of a template class is marked "virtual" that's
       all we need to know, since no virtual function override information
       is maintained for template classes. */
    if (is_virtual) goto done;
  } else {
    rout = rout_sym->variant.routine.ptr;
    is_nonreal_instantiation =
                    symbol_supplement_for_class(class_type)->is_nonreal_class;
  }  /* if */
  /* We scan symbols on the inactive list, since we are only interested in
     base classes symbols. */
  symbol_list = rout_sym->header->inactive_symbols;
  /* Outer loop:  go through the base classes of the current class. */
  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
    if (rout->special_kind == (a_special_function_kind)sfk_destructor) {
      /* Special processing is required for destructors, since a virtual
         destructor in a base class is not overridden in the derived class
         by a function of the same name. */
      sym = (symbol_supplement_for_class(bcp->type))->destructor;
      if (sym != NULL) {
        /* Base class does have a destructor. */
        rp = sym->variant.routine.ptr;
        if (rp->is_virtual) {
          /* Base class destructor is virtual. */
          is_virtual = TRUE;
          record_virtual_function_override(bcp, rp, rout);
          if (shares_virtual_function_info(class_type, bcp)) {
            /* The virtual function table is being shared, so we must use the
               identical number. */
            virtual_function_number = rp->virtual_function_number;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* Not a destructor, so do normal processing. */
      /* Pull out the unique scope identifier for this base class. */
      base_class_scope = bcp->type->
                          variant.class_struct_union.extra_info->assoc_scope;
      if (base_class_scope == NULL) {
        /* This is probably a nonreal base class in a prototype instantiation.
           Don't attempt a lookup in this case. */
        goto next_base_class;
      }  /* if */
      /* Inner loop:  go thorough all the symbols for this name, looking for
         one which represents a member function (overloaded or simple) from
         the base class under examination. */
      for (sym = symbol_list; sym != NULL; sym = sym_next) {
        sym_next = sym->next;
        if (sym->decl_scope == base_class_scope->number) {
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
               it's a typedef name, say, we can keep scanning. */
            if (sym->kind != (a_symbol_kind)sk_field &&
                sym->kind != (a_symbol_kind)sk_static_data_member) continue;
            goto next_base_class;
          }  /* if */
          /* Innermost loop is run only once for simple functions but more
             for overloaded functions.  This is a do-while loop instead of a
             for loop because we can be sure of the initial conditions on the
             first iteration. */
          do {
            rp = sym->variant.routine.ptr;
            /* We are only interested in virtual functions with the same
               type signature.  See first whether the parameter types are
               compatible and whether the implicit "this" param types are
               consistent (either both must be absent or both must be present
               and qualified identically). */
            if (rp->is_virtual &&
                param_types_are_compatible(
                                  rout->type, rp->type,
                                  TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING) &&
                this_param_types_correspond(rout->type, rp->type,
                                            /*check_as_conversion=*/FALSE,
                                            /*check_as_operands=*/FALSE)) {
              /* Now compare the return types. */
              if (return_types_are_override_compatible(rout->type, rp->type)) {
                /* Match */
                is_virtual = TRUE;
                /* Record the virtual function override in the base class
                   entry.  It can be used later, e.g., for building a virtual
                   function table. */
                record_virtual_function_override(bcp, rp, rout);
                if (shares_virtual_function_info(class_type, bcp)) {
                  /* The virtual function table is being shared, so we must
                     use the identical number. */
                  virtual_function_number = rp->virtual_function_number;
                }  /* if */
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
      }  /* if */
    }  /* for */
next_base_class:;
  }  /* for */
done:
  if (is_virtual) {
    /* Mark the routine entry. */
    rout->is_virtual = TRUE;
    class_type->variant.class_struct_union.any_virtual_functions = TRUE;
    class_type->variant.class_struct_union.
                 any_virtual_functions_including_in_base_classes = TRUE;
    if (virtual_function_number != 0) {
      /* The virtual base class is being shared between the current class
         and one of its base classes.  We reuse the existing number instead
         of reserving a new slot in the table. */
    } else {
      /* The number of virtual functions declared so far in this routine has
         is recorded in the class type supplement.  Increment that number and
         enter it in the routine entry.  It is used by the front end in
         managing virtual function override entries and can be used by the
         back end for indexing into a virtual function table. */
      if (ctsp->highest_virtual_function_number >=
                                         MAX_VIRTUAL_FUNCTIONS_PER_CLASS) {
        if (is_nonreal_instantiation) {
          /* Don't issue an error, since the number is not accurately
             maintained for class templates. */
        } else {
          pos_error(ec_too_many_virtual_functions, source_pos);
        }  /* if */
        /* Reset to zero, to avoid more such messages. */
        ctsp->highest_virtual_function_number = 0;
      }  /* if */
      virtual_function_number = ++(ctsp->highest_virtual_function_number);
    }  /* if */
    rout->virtual_function_number = virtual_function_number;
  }  /* if */
  db_exit();
  return is_virtual;
}  /* check_for_virtual_function */


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
      db_type_name(dsp->base_class->type);
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
  a_base_class_derivation_ptr  bcdp;
  a_boolean                    comma_needed = FALSE;

  (void)fputc('"', f_debug);
  db_type_name(bcp->type);
  if (bcp->derived_class != NULL) {
    fputc('"', f_debug);
    fprintf(f_debug, " (%lu/%u)",
            bcp->decl_position.seq, bcp->decl_position.column);
    fputs(", base class of \"", f_debug);
    db_type_name(bcp->derived_class);
  }  /* if */
  fputs("\": ", f_debug);
  if (show_offset) {
    fprintf(f_debug, "size = %ld, offset = %ld",
#if CFRONT_OBJECT_CODE_COMPATIBILITY
                     bcp->complete_subobject ?
                       bcp->type->size :
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
                       bcp->type->variant.class_struct_union.extra_info->
                                          size_without_virtual_base_classes,
                     bcp->offset);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
    if (bcp->data_section_base_class != NULL) {
      fputs(", in ", f_debug);
      db_type_name(bcp->data_section_base_class->type);
    }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
    comma_needed = TRUE;
  }  /* if */
  if (bcp->is_virtual) {
    if (comma_needed) fputs(", ", f_debug);
    fputs("virtual", f_debug);
    if (show_offset) {
      fprintf(f_debug, " (ptr offset = %ld", bcp->pointer_offset);
      if (bcp->pointer_base_class != NULL) {
        fputs(", in ", f_debug);
        db_type_name(bcp->pointer_base_class->type);
      }  /* if */
      fputc(')', f_debug);
    }  /* if */
    comma_needed = TRUE;
  }  /* if */
  if (bcp->ambiguous) {
    if (comma_needed) fputs(", ", f_debug);
    fputs("ambig", f_debug);
    comma_needed = TRUE;
  }  /* if */
  bcdp = bcp->derivation;
  if (comma_needed) fputs(",\n", f_debug);
  for (; bcdp != NULL; bcdp = bcdp->next) {
    fprintf(f_debug, "    %sderiv%s: ", (bcdp->direct ? "direct " : ""),
            ((bcp->is_virtual && bcdp->preferred) ? " (pref'd)" : ""));
    db_path(bcdp->path, show_offset);
    fputs(" (", f_debug);
    db_access_control(bcdp->access);
    fputs(")\n", f_debug);
  }  /* if */
}  /* db_base_class */


void db_base_class_list(a_type_ptr tp)
/*
Dump a linked list of base class entries, for debug purposes.
*/
{
  a_base_class_ptr       bcp;

  if (is_class_struct_union_type(tp)) {
    fputs("base classes for ", f_debug);
    db_type_name(tp);
    bcp = base_classes_of(tp);
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
  a_base_class_ptr             bcp;
  a_base_class_derivation_ptr  bcdp;
  int                          count;

  /* Only a virtual base class may have more than one derivation. */
  check_assertion(base_class->is_virtual ||
                  base_class->derivation->next == NULL);
  /* Count the number of direct derivations.  There should be exactly one
     if base_class is marked as direct, none otherwise. */
  count = 0;
  for (bcdp = base_class->derivation; bcdp != NULL; bcdp = bcdp->next) {
    if (bcdp->direct) ++count;
  }  /* for */
  check_assertion(base_class->direct == (count == 1));
  /* There should be exactly one preferred derivation. */
  count = 0;
  for (bcdp = base_class->derivation; bcdp != NULL; bcdp = bcdp->next) {
    if (bcdp->preferred) ++count;
  }  /* for */
  check_assertion(count == 1);
  /* Examine each derivation's path. */
  for (bcdp = base_class->derivation; bcdp != NULL; bcdp = bcdp->next) {
    /* The path should not be NULL. */
    check_assertion(bcdp->path != NULL);
    /* The first step should be either a direct base class or a virtual base
       class. */
    check_assertion(bcdp->path->base_class->derivation->direct ||
                    bcdp->path->base_class->is_virtual);
    /* Go through each step of the path. */
    for (dsp = bcdp->path; dsp != NULL; dsp = dsp->next) {
      /* Be sure the base class entry pointed to from the step entry is
         actually on the class type's list of base classes. */
      for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
        if (bcp == dsp->base_class) break;
        check_assertion(bcp->next != NULL);
      }  /* if */
      /* The last step (and it alone) should refer to base_class. */
      check_assertion((dsp->next == NULL) == (dsp->base_class == base_class));
      /* Only the first step and the last step may be virtual. */
      check_assertion(!dsp->base_class->is_virtual ||
                      (dsp->next == NULL || dsp == bcdp->path));
    }  /* for */
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
  a_base_class_ptr                    bcp;
  an_overriding_virtual_function_ptr  ovfp;

  db_enter(5, "verify_virt_func_override_list");
  ovfp = base_class->overriding_virtual_functions;
#if DEBUG
    if (debug_level >= 5) {
      if (ovfp != NULL) {
        fputs("base class = ", f_debug);
        db_base_class(base_class, /*show_offset=*/FALSE);
      }  /* if */
    }  /* if */
#endif /*if */
  for (; ovfp != NULL; ovfp = ovfp->next) {
#if DEBUG
    if (debug_level >= 5) {
      db_virtual_function_override(ovfp);
    }  /* if */
#endif /*if */
    if (ovfp->base_class == NULL) {
      check_assertion(null_allowed);
    } else if (ovfp->base_class != base_class) {
      for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
        if (bcp == ovfp->base_class) break;
      }  /* for */
      check_assertion(bcp != NULL);
    }  /* if */
  }  /* for */
  db_exit();
}  /* verify_virt_func_override_list */
#endif /* CHECKING */


a_boolean congruent_paths(a_derivation_step_ptr  dsp1,
                          a_derivation_step_ptr  dsp2)
/*
Return TRUE if the class sequence signatures of the paths headed by dsp1 and
dsp2 are identical.
*/
{
  a_boolean              congruent;
  a_derivation_step_ptr  dsp1_next, dsp2_next;

  db_enter(4, "congruent_paths");
#if DEBUG
  if (debug_level >= 4) {
    fputs("comparing ", f_debug);
    db_path(dsp1, /*show_offset=*/FALSE);
    fputs(" and ", f_debug);
    db_path(dsp2, /*show_offset=*/FALSE);
  }  /* if */
#endif /* DEBUG */
  congruent = FALSE;
  /* Only the start of a derivation and the end of a derivation can be
     virtual.  Check the start. */
  if (dsp1->base_class->is_virtual == dsp2->base_class->is_virtual) {
    for (;;) {
      /* Check the types of the corresponding steps. */
      if (dsp1->base_class->type != dsp2->base_class->type) break;
      /* Advance to the next step. */
      dsp1_next = dsp1->next;
      dsp2_next = dsp2->next;
      if (dsp1_next == NULL) {
        /* At the end of path1.  Terminate the loop after one more check. */
        if (dsp2_next == NULL) {
          /* At the end of path2 also.  Again, check the end of the derivation
             for matching is_virtual flags. */
          if (dsp1->base_class->is_virtual == dsp2->base_class->is_virtual) {
            congruent = TRUE;
          }  /* if */
        }  /* if */
        break;
      } else if (dsp2_next == NULL) {
        /* At the end of path2.  Terminate the loop. */
        break;
      }  /* if */
      /* Both paths have additional steps, so keep checking. */
      dsp1 = dsp1_next;
      dsp2 = dsp2_next;
    }  /* for */
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, " : %scongruent\n", congruent ? "" : "not ");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return congruent;
}  /* congruent_paths */


static a_derivation_step_ptr copy_and_extend_path(a_derivation_step_ptr path,
                                                  a_derivation_step_ptr step)
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


static void set_pointer_base_class(a_base_class_ptr       base_class,
                                   a_derivation_step_ptr  path)
/*
Base class is a virtual base class for which the pointer_base_class field has
not yet been set.  Set it to point to the nonvirtual base class in its
derivation path that is furthest along the derivation path without having any
virtual base classes of its own.  For instance, given this derivation:
    ==>D==>C==>V1==>B==>A==>V2
(where base classes V1 and V2 are virtual and the others are nonvirtual)
the pointer_base_class for both V1 and V2 is C.
*/
{
  a_derivation_step_ptr     dsp;
  a_base_class_ptr          bcp, pointer_base_class;
  a_base_class_derivation_ptr  bcdp;

  /* Find the segment of the derivation path that is headed by a direct base
     class.  In the example above, the path for V2 is ==>V1==>B==>A==>V2,
     but V1 is not a direct base class; therefore, we look at V1's path,
     namely, ==>D==>C==>V1, and find that D is a direct base class. */
  dsp = path;
  if (path->base_class->is_virtual) {
    pointer_base_class = path->base_class->pointer_base_class;
  } else {
    /* If the head of the derivation is non-virtual, we may have a candidate
       for a pointer base class. */
    check_assertion(dsp->next != NULL);
    /* Look for the second-to-last base class in this path segment.  (The
       last path entry should be either base_class itself. */
    while (dsp->next->next != NULL) dsp = dsp->next;
    pointer_base_class = dsp->base_class;
  }  /* if */
  if (pointer_base_class != NULL) {
    bcp = corresponding_base_class(base_class, pointer_base_class->type,
                                   (a_base_class_ptr)NULL);
    if (bcp->pointer_base_class != NULL) {
      /* Its pointer is embedded in some other base class, so we don't want it
         after all.  In such a case we'll encounter that base class later in
         processing and use it then. */
    } else {
      base_class->pointer_base_class = pointer_base_class;
      if (base_class->type->
            variant.class_struct_union.any_virtual_base_classes) {
        for (bcp = base_classes_of(base_class->derived_class);
             bcp != NULL;
             bcp = bcp->next) {
          if (bcp->is_virtual && bcp->pointer_base_class == NULL) {
            for (bcdp = bcp->derivation; bcdp != NULL; bcdp = bcdp->next) {
              if (bcdp->path->base_class == base_class) {
                set_pointer_base_class(bcp, bcdp->path);
                break;
              }  /* if */
            }  /* for */
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* set_pointer_base_class */

#if CFRONT_OBJECT_CODE_COMPATIBILITY

static void set_data_section_base_class(a_base_class_ptr       base_class,
                                        a_derivation_step_ptr  path)
/*
In cfront-compatibility mode the data section for a virtual base class
may be embedded in the data section of some other base class.  When this
is the case, the data_section_base_class field in the entry for the virtual
base class will contain a pointer to the base class that contains its data
section.  (This is not an issue in normal layout mode since virtual base
class data sections are never embedded.)  Here are some examples that
reveal how cfront decides when a virtual base class is embedded and when
it is not:

(1) Virtual base class data sections are not embedded in incomplete
    subobjects -- i.e., in the first direct nonvirtual base class:

  class A { int a; };                       //     A
  class B : public virtual A { int b; };    //     |
  class C : public B { int c; };            //     B
                                            //     |
Cfront's layout:                            //     C

  struct A {
    int a__1A ;
  };
  struct B {
    int b__1B ;
    struct A *PA;
    struct A OA;
  };
  struct C {
    int b__1B ;
    struct A *PA;
    int c__1C ;
    struct A OA;
  };

Here, the data section for virtual base class A is allocated independently
in C and not embedded in B, since be is not a complete subobject of C.

(2) Virtual base class data sections are (usually) embedded in complete
    subobjects.

  class A { };                             //  A
  class B : public virtual A { };          //   \
  class C { };                             //    B   C
  class D : public C, public B { };        //     \ /
                                           //      D
Cfront's layout:

  struct A {
    int a__1A ;
  };
  struct B {
    int b__1B ;
    struct A *PA;
    struct A OA;
  };
  struct C {
    int c__1C ;
  };
  struct D {
    int c__1C ;
    struct B OB;
    int d__1D ;
  };

In this case the data section for virtual base class A is not allocated
independently in D.  Rather, the A embedded in B is used, since B is a
complete subobject of D.

(3) When a virtual base class appears more than once in a class, the
    data section is (usually) that of the first complete subobject it
    belongs to.

  class A { int a; };                       //      A
  class B : public virtual A { int b; };    //     /|\
  class C : public virtual A { int c; };    //    B | C
  class D : public C, public B,             //     \|/
            public virtual A { int d; };    //      D   

Cfront's layout:

  struct A {
    int a__1A ;
  };
  struct B {
    int b__1B ;
    struct A *PA;
    struct A OA;
  };
  struct C {
    int c__1C ;
    struct A *PA;
    struct A OA;
  };
  struct D {
    int b__1B ;
    struct A *PA;
    struct C OC;
    int d__1D ;
  };

Here the data section for virtual base A in D is embedded in C, because
B is not complete and even though A is also a direct base class of D.

(4) An "interesting" anomaly (or bug?) appears when another level of
    inheritance is added to example (3):

  class A { int a; };                       //      A   
  class B : public virtual A { int b; };    //     /|\  
  class C : public virtual A { int c; };    //    B | C 
  class D : public B, public C,		    //     \|/  
            public virtual A { int d; };    //      D   
  class E : public D { int e; };            //      |
                                            //      E
Cfront's layout:

  struct A {
    int a__1A ;
  };
  struct B {
    int b__1B ;
    struct A *PA;
    struct A OA;
  };
  struct C {
    int c__1C ;
    struct A *PA;
    struct A OA;
  };
  struct D {
    int b__1B ;
    struct A *PA;
    struct C OC;
    int d__1D ;
  };
  struct E {
    int b__1B ;
    struct A *PA;
    struct C OC;
    int d__1D ;
    int e__1E ;
    struct A OA;
  };

What's anomalous about this case is that, even though there is a copy of
A's data section in C, and even though C is a complete subobject of E,
another copy of A's data section is allocated in E.

The rule here seems to be that a virtual base class (e.g., A) that is an
indirect base class of an incomplete subobject base class (e.g., D) is
treated as though it were not embedded in an intermediate complete
subobject (e.g., C).
*/
{
  a_derivation_step_ptr  dsp = path;
  a_base_class_ptr       bcp;

  db_enter(4, "set_data_section_base_class");
  /* If the first entry on the derivation path is a complete subobject,
     it may have virtual base classes embedded within it. */
  if (dsp->base_class->complete_subobject) {
    if (dsp->base_class->is_virtual &&
        dsp->base_class->data_section_base_class == NULL) {
      /* If dsp refers to a virtual base class which is not itself embedded,
         then don't (yet) mark the current base class as embedded in it. */
    } else {
      /* Traverse the path. */
      for (;; dsp = dsp->next) {
        if (dsp->next->base_class == base_class ||
            !dsp->next->base_class->complete_subobject) {
          /* dsp represents an intermediate base class.  If the next entry
             on the path is NULL (i.e., if dsp is the last entry before the
             base_class) or is an incomplete subobject (meaning it cannot
             have data sections for virtual base classes embedded within it),
             then this is where base_class may be embedded. */
          bcp = corresponding_base_class(base_class, dsp->base_class->type,
                                         (a_base_class_ptr)NULL);
          if (bcp->data_section_base_class == NULL) {
            base_class->data_section_base_class = dsp->base_class;
          }  /* if */
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  db_exit();
}  /* set_data_section_base_class */

#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */


static a_derivation_step_ptr update_base_class_derivation(
                                         a_base_class_ptr       base_class,
                                         a_derivation_step_ptr  path,
                                         an_access_specifier    access)
/*
base_class is a direct base class declared explicitly (in which case path is
NULL) or an indirect base class implied by the declaration of a direct base
class (in which case path is non-NULL).  access is the access to be applied
for this derivation, which for indirect base classes corresponds to the access
of the last step on the path.  This routine allocates a base-class-derivation
entry for the derivation and supplies it with the appropriate derivation
path and access.
*/
{
  a_base_class_derivation_ptr  bcdp, new_bcdp;
  a_derivation_step_ptr        step;

  /* Allocate the entry and make a step entry. */
  new_bcdp = alloc_base_class_derivation();
  step = make_derivation_step(base_class, (a_derivation_step_ptr)NULL);
  if (path == NULL) {
    /* A direct base class. */
    new_bcdp->direct = TRUE;
    new_bcdp->path = step;
  } else {
    /* An indirect base class. */
    new_bcdp->path = copy_and_extend_path(path, step);
  }  /* if */
  new_bcdp->access = access;
  if (!base_class->is_virtual) {
    /* In the case of a nonvirtual base class, there is only one derivation,
       so it is marked as "preferred" by default. */
    new_bcdp->preferred = TRUE;
    base_class->derivation = new_bcdp;
    path = new_bcdp->path;
  } else {
    /* For virtual base classes, this may be one derivation among several.
       Add it to the end of the linked list of base-class-derivation
       entries. */
    bcdp = base_class->derivation;
    if (bcdp == NULL) {
      base_class->derivation = new_bcdp;
    } else {
      while (bcdp->next != NULL) bcdp = bcdp->next;
      bcdp->next = new_bcdp;
    }  /* if */
    /* Note that the preferred flag is set later. */
    /* Set the pointer-base-class (the nonvirtual base class in which a
       pointer to this virtual base class may be found) if appropriate. */
    if (path != NULL) {
      if (base_class->pointer_base_class == NULL) {
        set_pointer_base_class(base_class, new_bcdp->path);
      }  /* if */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
      /* In cfront mode set the data section base class, which is where the
         data section of this virtual base class may be embedded. */
      if (base_class->data_section_base_class == NULL) {
        set_data_section_base_class(base_class, new_bcdp->path);
      }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
    }  /* if */
    /* Return a different path than the one stored in the base-class-
       derivation entry.  This is because each virtual base class is the
       start of a new segment. */
    path = step;
  }  /* if */
  return path;
}  /* update_base_class_derivation */


static void add_indirect_base_class(a_base_class_ptr      base_class_to_copy,
                                    a_base_class_ptr      directly_derived_bcp,
                                    a_derivation_step_ptr path,
                                    a_base_class_ptr      *p_end_of_add_list,
                                    a_type_ptr            new_class)
/*

Create a new indirect base class based on base_class_to_copy and,
typically, add it to the end of the base classes list for new_class.
directly_derived_bcp points to a recently created (or copied) base class,
a direct or indirect base class of new_class, of which the new base class
will be a direct base class. In addition, check for ambiguity and
duplicate paths.  The copy will be a base class of new_class.

*/
{
  a_base_class_ptr     new_bcp = NULL, bcp;
  an_access_specifier  access;

  db_enter(3, "add_indirect_base_class");
  /* Record the derivation path from the most derived class to the class that
     is directly derived from the new base class; it will be copied and
     extended to produce the new base class's derivation. */
  /* We will mark the derivation's access by copying the access associated
     with the first derivation of base_class_to_copy, which should also be a
     direct derivation. */
  check_assertion(base_class_to_copy->derivation->direct);
  access = base_class_to_copy->derivation->access;
  if (base_class_to_copy->is_virtual) {
    for (bcp = base_classes_of(new_class); bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual && bcp->type == base_class_to_copy->type) {
        /* The base class to be copied is a virtual base class and a virtual
           base class referring to the same class type is already on the base
           classes list for the new class.  No new base class entry needs to
           be created. */
#if DEBUG
        if (debug_level >= 3) {
          fputs("  reencountering virtual base class \"", f_debug);
          db_type_name(bcp->type);
          fputs("\" for ", f_debug);
          db_abbreviated_type(new_class);
          fputc('\n', f_debug);
        }  /* if */
#endif /* DEBUG */
        (void)update_base_class_derivation(bcp, path, access);
        goto done;
      }  /* if */
    }  /* for */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    fputs("  creating indirect base class \"", f_debug);
    db_type_name(base_class_to_copy->type);
    fputs("\" for ", f_debug);
    db_abbreviated_type(new_class);
    fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
  /* Create a new base class entry. */
  new_bcp = alloc_base_class();
  new_bcp->type = base_class_to_copy->type;
  new_bcp->derived_class = new_class;
  new_bcp->decl_position = directly_derived_bcp->decl_position;
  new_bcp->direct = FALSE;
  if (base_class_to_copy->is_virtual) new_bcp->is_virtual = TRUE;
  path = update_base_class_derivation(new_bcp, path, access);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  if (new_bcp->is_virtual) {
    /* According to cfront all virtual base classes are complete
       subobjects. */
    new_bcp->complete_subobject = TRUE;
  } else {
    new_bcp->complete_subobject = base_class_to_copy->complete_subobject;
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  }  /* if */
  /* Check for ambiguity. */
  for (bcp = base_classes_of(new_class); bcp != NULL; bcp = bcp->next) {
    if (bcp->type == new_bcp->type) {
      check_assertion(!(bcp->is_virtual && new_bcp->is_virtual));
      /* Ambiguous base class. */
      bcp->ambiguous = TRUE;
      new_bcp->ambiguous = TRUE;
      break;
    }  /* if */
  }  /* for */
  /* Add the base classes of the current indirect base class to the base
     classes list of the most-derived-class. */
  for (bcp = base_classes_of(new_bcp->type); bcp != NULL; bcp = bcp->next) {
    if (!bcp->direct) {
      continue;
    } else if (bcp->is_virtual) {
      /* A virtual base class is marked as "direct" if any of its paths
         is direct.  However, for our purposes, the "first" path (first in
         a depth-first left-to-right traversal of the derivation graph)
         must be direct. */
      if (!bcp->derivation->direct) continue;
    }  /* if */
    add_indirect_base_class(bcp, new_bcp, path, p_end_of_add_list, new_class);
  }  /* for */
  /* Add this to the end of add_list. */
  if (*p_end_of_add_list == NULL) {
    new_class->variant.class_struct_union.extra_info->base_classes = new_bcp;
  } else {
    (*p_end_of_add_list)->next = new_bcp;
  }  /* if */
  *p_end_of_add_list = new_bcp;
  /* Branch here for duplicate virtual base class. */
done:;
  db_exit();
}  /* add_indirect_base_class */


static void set_preferred_base_class_derivation(a_type_ptr        class_type,
                                             a_base_class_ptr  base_class)
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
The derivations for X and Y in C are represented as ==>V==>X and ==>V==>Y,
respectively, but V in C has two derivations, ==>A==>V and ==>B==>V.  Both
paths are represented in the IL -- the base class entry for V points to a
linked list of virtual-derivation entries, each of which points to a
derivation of V.

This routine is called to identify the "preferred" path -- the sequence of
casts that affords the greatest "normal" accessibility (i.e., without
special treatment for casts in the context of member or friend functions).
When there are two entries with equally good access, a direct base class is
preferred over in indirect, and an indirect base class with no virtual base
classes in its derivation is preferred over one that has virtual base
classes in its derivation.

Note that the virtual base class appears on the base class list in the
position of its first appearance in the depth-first left-to-right tranversal
of the base specifiers graph.  This position is independent of the which
appearance of the base class happens to have been marked preferred.
*/
{
  a_base_class_derivation_ptr  bcdp, preferred_bcdp;
  an_access_specifier          access, preferred_access;

  db_enter(4, "set_preferred_base_class_derivation");
  /* Has this set of virtual derivations been checked yet?  This can be
     determined by seeing if any has the preferred flag set already. */
  for (bcdp = base_class->derivation; bcdp != NULL; bcdp = bcdp->next) {
    /* Preferred flag has already been set for this group of derivations. */
    if (bcdp->preferred) goto done;
  }  /* for */
  /* Traverse the linked list of virtual derivations. */
  for (bcdp = base_class->derivation; bcdp != NULL; bcdp = bcdp->next) {
    if (bcdp->path->base_class->is_virtual &&
        bcdp->path->base_class != base_class) {
      /* If this virtual base class has a virtual base class in its
         derivation path, the intermediate step has to be processed first. */
      set_preferred_base_class_derivation(class_type, bcdp->path->base_class);
    }  /* if */
    /* Determine the accessibility of a public member of the virtual base
       class in the context of the the most derived class. */
    access = access_to_end_of_path((an_access_specifier)as_public,
                                   bcdp->path, bcdp);
    if (bcdp == base_class->derivation) {
      /* Prefer the first unless another turns out to have better access. */
      preferred_bcdp = bcdp;
      preferred_access = access;
    } else {
      /* Compare the two paths. */
      if (is_more_accessible(access, preferred_access)) {
        /* The new one is more accessible.  Use it. */
        preferred_bcdp = bcdp;
        preferred_access = access;
      } else if (access == preferred_access) {
        /* No preference based on accessibility.  Look for other criteria. */
        if (!preferred_bcdp->direct) {
          if (bcdp->direct) {
            /* Choose a direct base class over an indirect. */
            preferred_bcdp = bcdp;
          } else if (!bcdp->path->base_class->is_virtual &&
                     preferred_bcdp->path->base_class->is_virtual) {
            /* Both are indirect, but we choose a path with no virtual steps
               over one that a path that has virtual steps. */
            preferred_bcdp = bcdp;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  preferred_bcdp->preferred = TRUE;
done:;
  db_exit();
}  /* set_preferred_base_class_derivation */


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
  a_base_class_ptr              new_direct_bcp;
  an_access_specifier           access;
  a_boolean                     is_virtual;
  a_boolean                     access_already_specified;
  char                          *default_access_str;
  a_symbol_ptr                  sym;
  a_type_ptr                    base_class_type;
  a_boolean                     ambiguous;
  a_class_symbol_supplement_ptr cssp, bcp_cssp;
  a_boolean                     any_base_class_with_override_list;
  a_boolean                     first_direct_nonvirtual_base_class = TRUE;
  a_source_position             base_class_decl_pos;
  a_derivation_step_ptr         path;

  db_enter(3, "scan_base_specifier_list");
#if DEBUG
  if (debug_level >= 3) {
    fputs("scanning base classes for ", f_debug);
    db_abbreviated_type(type_ptr);
    fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
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
  cssp = symbol_supplement_for_class(type_ptr);
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
            default:;  /* Avoid gcc warnings. */
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
      a_boolean gid_err;

      base_class_decl_pos = pos_curr_token;
      sym = coalesce_and_lookup_generalized_identifier
            	(GID_NO_OPTIONS, ilm_normal, &gid_err);
      if (sym == NULL || !is_class_symbol(sym)) {
        if (sym != NULL && sym->kind == (a_symbol_kind)sk_type &&
            sym->variant.type->kind == (a_type_kind)tk_template_param) {
          /* No diagnostic on template parameters, which will only show
             up during prototype instantiations.  Set the flag that
             indicates that this prototype instantiation has a nonreal
             base class. */
          cssp->any_nonreal_base_classes = TRUE;
        } else {
          error(ec_not_a_class_or_struct_name);
        }  /* if */
        goto skip_base_class;
      } else if (locator_for_curr_id.is_semivisible_nested_type) {
        /* The symbol in the locator is a nested class that is not visible
           according to the ARM lookup rules but is returned in support of
           the nested class anachronism (ARM 18.3.5). Issue an anachronism
           diagnostic. */
        sym_diagnostic(anachronism_error_severity, ec_nested_class_anachronism,
                       locator_for_curr_id.specific_symbol);
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
      /* Get the type entry for the base class name. */
      base_class_type = type_symbol_type(sym);
      base_class_type->source_corresp.referenced = TRUE;
      /* If it is a const or volatile qualified type name (where in the ARM is
         this required???) or if it is the class now being defined or if
         it is a union or if it has been declared but not yet defined (ARM
         10, p. 196), issue an error and skip over this class: it is not a
         valid base class name. */
      check_for_uninstantiated_template_class(base_class_type);
      if (is_qualified_type(base_class_type) ||
          (base_class_type = skip_typerefs(base_class_type)) == type_ptr ||
          base_class_type->kind == (a_type_kind)tk_union ||
          !is_complete_class_struct_union_type(base_class_type)) {
        error(ec_bad_base_class);
        goto skip_base_class;
      }  /* if */
      /* Issue a warning if an explicit access specifier was not provided
         (as per the recommendation on p. 243 of the ARM). */
      if (!access_already_specified) {
        str_warning(ec_missing_access_specifier, default_access_str);
      }  /* if */
      check_assertion(ctsp != NULL);
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
            /* This virtual base class is already on the list.  Record this
               derivation as an alternate path; it may turn out to be the
               preferred path. */
            (void)update_base_class_derivation(bcp,
                                               (a_derivation_step_ptr)NULL,
                                               access);
            bcp->direct = TRUE;
            bcp->decl_position = base_class_decl_pos;
            goto skip_base_class;
          } else {
            /* At least one is non-virtual, so there is an ambiguity.  Mark
               both as ambiguous.  */
            ambiguous = bcp->ambiguous = TRUE;
          }  /* if */
        }  /* if */
      }  /* for */
      /* The current class will have to have a constructor if any of its base
         classes is virtual or itself has a constructor; it requires a
         destructor if any of its base classes has a destructor.  Record such
         requirements, if any, at this time. */
      bcp_cssp = symbol_supplement_for_class(base_class_type);
      if (is_virtual || bcp_cssp->constructor != NULL) {
        cssp->constructor_required = TRUE;
      }  /* if */
      if (bcp_cssp->destructor != NULL) {
        cssp->destructor_required = TRUE;
        if (!bcp_cssp->destructor->variant.routine.ptr->is_virtual) {
          /* The base class has a nonvirtual destructor, which is not
             recommended (see commentary in ARM 12.4). */
          type_remark(ec_base_class_with_nonvirtual_dtor, base_class_type);
        }  /* if */
      }  /* if */
      /* Indicate whether an operator new or operate delete is inherited into
         the current derived class. */
      if (bcp_cssp->has_operator_new) cssp->has_operator_new = TRUE;
      if (bcp_cssp->has_operator_delete) cssp->has_operator_delete = TRUE;
      /* The current derived class cannot be copy-constructed or assigned by
         bitwise copying if the base class does not allow it or is a virtual
         base class. */
      if (is_virtual) {
        cssp->construction_by_bitwise_copy_allowed = FALSE;
        cssp->assignment_by_bitwise_copy_allowed = FALSE;
      } else {
        if (!bcp_cssp->construction_by_bitwise_copy_allowed) {
          cssp->construction_by_bitwise_copy_allowed = FALSE;
        }  /* if */
        if (!bcp_cssp->assignment_by_bitwise_copy_allowed) {
          cssp->assignment_by_bitwise_copy_allowed = FALSE;
        }  /* if */
      }  /* if */
      if (bcp_cssp->any_nonstatic_data_members) {
        cssp->any_nonstatic_data_members = TRUE;
      }  /* if */
      if (bcp_cssp->any_nonreal_base_classes || bcp_cssp->is_nonreal_class) {
        cssp->any_nonreal_base_classes = TRUE;
      }  /* if */
      /* Update the flag indicating whether there are any virtual base
         classes. */
      if (is_virtual || base_class_type->
                       variant.class_struct_union.any_virtual_base_classes) {
        type_ptr->variant.class_struct_union.any_virtual_base_classes = TRUE;
      }  /* if */
      if (base_class_type->variant.class_struct_union.
                       any_virtual_functions_including_in_base_classes) {
        type_ptr->variant.class_struct_union.
                       any_virtual_functions_including_in_base_classes = TRUE;
      }  /* if */
      /* Now create the new base class entry and add it to the end of the
         base classes list. */
      new_direct_bcp = alloc_base_class();
      new_direct_bcp->type = base_class_type;
      new_direct_bcp->derived_class = type_ptr;
      new_direct_bcp->decl_position = base_class_decl_pos;
      new_direct_bcp->direct = TRUE;
      new_direct_bcp->ambiguous = ambiguous;
      if (is_virtual) new_direct_bcp->is_virtual = TRUE;
      path = update_base_class_derivation(new_direct_bcp,
                                          (a_derivation_step_ptr)NULL, access);
#if CFRONT_OBJECT_CODE_COMPATIBILITY
      /* When cfront lays out a class with base classes, the subobject for the
         first direct nonvirtual base class does not include the data sections
         for its own virtual base classes (if any).  However, the subobjects
         for the second and subsequent direct nonvirtual base classes and for
         virtual base classes do include the virtual base class data sections
         and are therefore marked as having a "complete subobject". */
      if (is_virtual || !first_direct_nonvirtual_base_class) {
        new_direct_bcp->complete_subobject = TRUE;
      }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
      /* Offset is updated in merge_field_lists. */
      new_direct_bcp->offset = 0;
      /* Add base classes derived from this base class to the current class's
         base class list.  They are marked as indirect. */
      any_base_class_with_override_list = FALSE;
      for (bcp = base_classes_of(new_direct_bcp->type);
           bcp != NULL;
           bcp = bcp->next) {
        if (bcp->overriding_virtual_functions != NULL) {
          any_base_class_with_override_list = TRUE;
        }  /* if */
        if (!bcp->direct) {
          continue;
        } else if (bcp->is_virtual) {
          /* A virtual base class is marked as "direct" if any of its paths
             is direct.  However, for our purposes, the "first" path (first in
             a depth-first left-to-right traversal of the derivation graph)
             must be direct. */
          if (!bcp->derivation->direct) continue;
        }  /* if */
          /* Add the direct base class and all *its* base classes to the
           base class list for the derived class. */
        add_indirect_base_class(bcp, new_direct_bcp, path,
                                &end_of_base_classes_list, type_ptr);
      }  /* for */
#if 0
      if (!new_direct_bcp->is_virtual &&
          new_direct_bcp->
              type->variant.class_struct_union.any_virtual_base_classes) {
        for (bcp = base_classes_of(type_ptr); bcp != NULL; bcp = bcp->next) {
          if (bcp->is_virtual && bcp->pointer_base_class == NULL) {
            /* bcp is a virtual base class of the new class and its
               pointer_base_class field has not been set yet. */
            a_base_class_ptr other_bcp =
                                 base_classes_of(new_direct_bcp->type);
            a_base_class_ptr pointer_base_class = NULL;
            for (; other_bcp != NULL; other_bcp = other_bcp->next) {
              if (other_bcp->is_virtual && other_bcp->type == bcp->type) {
                pointer_base_class = other_bcp->pointer_base_class;
                break;
              }  /* if */
            }  /* for */
            if (pointer_base_class != NULL) {
              pointer_base_class =
                       corresponding_base_class(pointer_base_class, type_ptr,
                                                (a_base_class_ptr)NULL);
                check_assertion(!pointer_base_class->
                                              any_virtual_steps_in_derivation);
              bcp->pointer_base_class = pointer_base_class;
            }  /* if */
          }  /* if */
        }  /* for */
      }  /* if */
#endif /* if 0 */
      /* Enter the base name on the base class list in the derived class's
         class-supplement entry. */
      if (end_of_base_classes_list == NULL) {
        ctsp->base_classes = new_direct_bcp;
      } else {
        end_of_base_classes_list->next = new_direct_bcp;
      }  /* if */
      end_of_base_classes_list = new_direct_bcp;
      if (any_base_class_with_override_list) {
        for (bcp = base_classes_of(new_direct_bcp->type);
             bcp != NULL;
             bcp = bcp->next) {
          if (bcp->overriding_virtual_functions != NULL) {
#if DEBUG
            if (debug_level >= 4) {
              fputs("copying virtual function override list from ", f_debug);
              db_base_class(bcp, FALSE);
              db_virtual_function_override_list(bcp);
            }  /* if */
#endif /* DEBUG */
            new_bcp = corresponding_base_class(bcp, type_ptr,
                                               (a_base_class_ptr)NULL);
            /* Copy the virtual function override entries from bcp (which is
               on the base classes list for base_class_type) to the
               corresponding copied base class new_bcp (which is on the base
               bases list for type_ptr). */
            copy_virtual_function_override_list(bcp, new_bcp,
                                                base_class_type, type_ptr);
#if DEBUG
            if (debug_level >= 4) {
              fputs("new base class ", f_debug);
              db_base_class(bcp, FALSE);
              db_virtual_function_override_list(new_bcp);
            }  /* if */
#endif /* DEBUG */
          }  /* if */
        }  /* for */
      }  /* if */
      if (first_direct_nonvirtual_base_class && !is_virtual) {
        /* For the first direct nonvirtual base class it is possible to
           share virtual function info (e.g., virtual function tables and
           their associated pointers) between the base class and the
           derived class. */
        a_class_type_supplement_ptr  base_ctsp;

        base_ctsp = base_class_type->variant.class_struct_union.extra_info;
        /* Check highest_virtual_function_number instead of the
           any_virtual_functions flag, since the latter will be TRUE only if
           the base class actually declared its own virtual functions, but
           the highest number is "inherited" when it itself was eligible to
           share with a base class of its own. For example:
                  class A { virtual void f() };  // flag is TRUE, highest is 1
                  class B : public A {};         // flag is FALSE, highest is 1
                  class C : public B { ...
           The virtual function table for C and the one for A-in-C can be
           shared, even though B doesn't have a virtual function table.  B's
           virtual_function_info_base_class will, however, still refer to A. */
        if (base_ctsp->highest_virtual_function_number > 0) {
          bcp = base_ctsp->virtual_function_info_base_class;
          if (bcp == NULL) {
            /* The base class does not share virtual function info with its
               own base classes. */
            ctsp->virtual_function_info_base_class = new_direct_bcp;
          } else {
            /* Refer to the same virtual_function_info_base_class as the
               direct base class does.  (In the above example, set the field
               to point to A.) */
            ctsp->virtual_function_info_base_class =
                            corresponding_base_class(bcp, type_ptr,
                                                     (a_base_class_ptr)NULL);
          }  /* if */
          /* Advance the virtual function count so that any new virtual
             functions will be tacked on at the end of the shared virtual
             function info block.  (Redeclarations will use the slot
             already reserved for the function.) */
          ctsp->highest_virtual_function_number =
                                   base_ctsp->highest_virtual_function_number;
        }  /* if */
        first_direct_nonvirtual_base_class = FALSE;
      }  /* if */
skip_base_class:
      /* Advance past the base class name to the comma or right brace. */
      (void)get_token();
    }  /* if */
    /* Advance past the next comma, if any, and scan the next base class
       specifier. */
    remove_stop_token(tok_comma);
  } while (loop_token(tok_comma));
  if (type_ptr->variant.class_struct_union.any_virtual_base_classes) {
    /* Make a pass over the base class list to resolve duplicate virtual base
       classes (if any). */
    for (bcp = base_classes_of(type_ptr); bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual) {      
        set_preferred_base_class_derivation(type_ptr, bcp);
      }  /* if */
    }  /* for */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    db_base_class_list(type_ptr);
  }  /* if */
#endif /* DEBUG */
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

  if (class_type == friend_class_type) {
    /* Diagnostic on excessively narcissism. */
    diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
               ec_self_friendship);
  } else {
    ctsp = friend_class_type->variant.class_struct_union.extra_info;
    /* Issue a remark if this is a duplicate friend declaration. */
    for (clep = ctsp->befriending_classes; clep != NULL; clep = clep->next) {
      if (clep->class_type == class_type) {
        remark(ec_duplicate_friend_decl);
        break;
      }  /* if */
    }  /* for */
    if (clep == NULL) {
      /* No duplication was detected. */
      clep = alloc_list_entry_for_class();
      clep->class_type = class_type;
      clep->next = ctsp->befriending_classes;
      ctsp->befriending_classes = clep;
      /* Now add the friend_class_type to the friends list for the current
         class. */
      ctsp = class_type->variant.class_struct_union.extra_info;
      clep = alloc_list_entry_for_class();
      clep->class_type = friend_class_type;
      clep->next = ctsp->friend_classes;
      ctsp->friend_classes = clep;
    }  /* if */
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
    orig_type = sym->variant.routine.ptr->type;
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
      match = types_are_strictly_compatible(orig_type, new_type);
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


static a_symbol_ptr decl_friend_function(a_symbol_locator      *locator,
                                         a_type_ptr            class_type,
                                         a_type_ptr            function_type,
                                         a_func_info_block_ptr func_info)
/*
Do processing for declaring a function (identified by *locator and with
a type of function_type) friend of the current class (class_type).  Getting
the correct symbol of a previously declared function means taking overloading
into account.  For nonmember functions, this could be the initial declaration
of the function, and again overloading is a possibility.
*/
{
  a_symbol_ptr                 sym, ext_sym;
  an_id_linkage_kind           linkage;
  a_type_ptr                   old_type;
  a_class_list_entry_ptr       clep;
  a_boolean                    is_overloaded_function;
  a_storage_class              storage_class;
  a_class_type_supplement_ptr  ctsp;
  a_routine_list_entry_ptr     rlep;

  db_enter(3, "decl_friend_function");
  if (!is_error_locator(*locator)) {
    if (symbol_supplement_for_class(class_type)->is_nonreal_class) {
      /* Scan past friend functions during prototype instantiation. */
      set_to_error_locator(*locator);
    }  /* if */
  }  /* if */
  if (!is_error_locator(*locator)) {
    sym = locator->specific_symbol;
    if (sym != NULL && sym->class_of_which_a_member != NULL &&
        !is_member_function_symbol(sym)) {
      /* sym represents a member of a class, but it is not a member function.
         Issue an error. */
      if (sym->kind == (a_symbol_kind)sk_projection) {
        /* A member of a base class. */
        pos_error(ec_inherited_member_not_allowed, &locator->source_position);
      } else {
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &locator->source_position, sym);
      }  /* if */
      sym = NULL;
      set_to_error_locator(*locator);
    } else if (sym == NULL || !is_member_function_symbol(sym)) {
      /* Not a member function.  Get the symbol -- the rest of what's
         returned from decl_var_or_routine is not relevant for processing
         in this context. */
      /* If the friend function is defined in this declaration or if it was
         specified as inline, that information should be passed on to
         decl_var_or_routine. */
      if (strcmp(locator->symbol_header->identifier, "main") == 0) {
        /* Friendship is being given to the main() function. */
        func_info->is_main_function = TRUE;
        if (func_info->is_inline) {
          /* But it can't be declared "inline" or defined inline. */
          pos_error(ec_inline_main, &locator->source_position);
          func_info->is_inline = FALSE;
        }  /* if */
      } else if (func_info->is_definition &&
                 class_type->source_corresp.is_local_to_function) {
        /* It is an error to define a function in a friend declaration of a
           local class.  To avoid confusion down the road, clear is_inline. */
        func_info->is_inline = FALSE;
      }  /* if */
      if (func_info->is_inline) {
        storage_class = (a_storage_class)sc_static;
      } else {
        storage_class = (a_storage_class)sc_extern;
      }  /* if */
      decl_var_or_routine(locator, storage_class, function_type, func_info,
                          /*is_variable_def=*/FALSE, &sym, &linkage,
                          &old_type, &ext_sym);
      /* WP 11.4 para 5 prohibits defining a nonmember function in a local
         class friend declaration. */
      if (func_info->is_definition &&
          class_type->source_corresp.is_local_to_function) {
        pos_sy_error(ec_bad_scope_for_definition, &pos_curr_token, sym);
      }  /* if */
    } else {
      if (sym->class_of_which_a_member == class_type) {
        /* It's a member function of the very class that is according it
           friendship.  Issue a diagnostic. */
        diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
                   ec_self_friendship);
      } else {
        /* It's a member function.  Find the right type signature for this
           member function name.  If none can be found, NULL is returned. */
        is_overloaded_function =
                          sym->kind == (a_symbol_kind)sk_overloaded_function;
        sym = member_function_redecl_sym(sym, function_type);
        if (sym == NULL) {
          sym_error(is_overloaded_function ?
                          ec_overloaded_function_incompatible_type :
                          ec_not_compatible_with_previous_decl,
                    locator->specific_symbol);
          set_to_error_locator(*locator);
        } else {
          if (func_info->is_inline && !func_info->is_definition &&
              !sym->variant.routine.ptr->is_inline) {
            error(ec_inline_not_allowed);
          }  /* if */
        }  /* if */
      }  /* if */
      if (sym != NULL) {
        if (sym->defined && func_info->is_definition) {
          /* Trying to defined a function that's already defined. */
          pos_sy_error(ec_function_redefinition,
                       &locator->source_position, sym);
          set_to_error_locator(*locator);
        } else {
          if (func_info->is_definition) {
            mark_defined(sym, &locator->source_position);
          } else {
            mark_declared(sym, &locator->source_position);
          }  /* if */
          /* Do throw specification compatibility checking. */
          check_throw_specification(func_info, sym->variant.routine.ptr);
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
    sym->variant.routine.ptr = make_routine(function_type,
                                            (a_storage_class)sc_static,
                                            /*at_file_scope=*/TRUE,
                                            /*add_to_list=*/FALSE);
    /* Set the source correspondence. */
    set_source_corresp(&sym->variant.routine.ptr->source_corresp, sym);
  } else {
    clep = sym->variant.routine.ptr->befriending_classes;
    /* Issue a warning if this is a duplicate friend declaration. */
    for (; clep != NULL; clep = clep->next) {
      if (clep->class_type == class_type) {
        remark(ec_duplicate_friend_decl);
        break;
      }  /* if */
    }  /* for */
    if (clep == NULL) {
      /* No duplication was detected. */
      clep = alloc_list_entry_for_class();
      clep->class_type = class_type;
      clep->next = sym->variant.routine.ptr->befriending_classes;
      sym->variant.routine.ptr->befriending_classes = clep;
      /* Now add the routine to the friends list for the current class. */
      ctsp = class_type->variant.class_struct_union.extra_info;
      rlep = alloc_list_entry_for_routine();
      rlep->routine = sym->variant.routine.ptr;
      rlep->next = ctsp->friend_routines;
      ctsp->friend_routines = rlep;
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

  check_assertion(ctor_rout->special_kind ==
                                  (a_special_function_kind)sfk_constructor);
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

  check_assertion(ctor_rout->special_kind ==
                                  (a_special_function_kind)sfk_constructor);
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
    if (sym->kind == (a_symbol_kind)sk_function_template) {
      rp = sym->variant.template_info->variant.function.routine;
    } else {
      rp = sym->variant.routine.ptr;
    }  /* if */
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
      new_sym = member_function_redecl_sym(sym, type);
      if (new_sym == NULL) {
        /* The previously declared function with the same name (or, if it is
           already overloaded, any instance of it) does not have a matching
           type, so sym remains a candidate for overloading. */
      } else {
        /* This is a redeclaration.  Just return to old symbol entry, setting
           sym to NULL to avoid overload processing.  The caller will
           do some consistency checking and issue a warning. */
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
      /* template_case is FALSE in the following because although member
         functions of class templates have template types in their parameters,
         they are not called using the template overload resolution
         mechanism. */
      } else if (!overload_distinguishable(sym, type,
                                           /*template_case=*/FALSE, /* sic! */
                                           &error_code)) {
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
    new_sym = enter_local_symbol((a_symbol_kind)sk_member_function, locator,
                                 decl_scope_level, suppress_redecl_error);
  }  /* if */
  db_exit()
  return new_sym;
}  /* symbol_for_member_function */


static void redecl_member_function(a_symbol_ptr         sym,
                                   a_type_ptr           member_type,
                                   an_access_specifier  access,
                                   a_boolean            is_inline,
                                   a_boolean            is_virtual,
                                   a_source_position    *err_pos)
/*
The current member function redeclares the function to which sym refers.
Parameters member_type, access, is_inline, and is_virtual indicate
specifications of the current declaration.  The ARM (9.2) disallows the
redeclaration of member functions, so issue an error, but if access,
static-ness, and virtual-ness are unchanged, merge the two declarations.
(This routine was originally written to support an extension to allow
member function redeclarations, as long as the second declaration was not
too different from the first.  Changing it back to support such behavior
is just a matter of changing where and how diagnostics are issued.)
*/
{
  a_routine_ptr             rp = sym->variant.routine.ptr;
  a_param_type_ptr          ptp1, ptp2;
  a_def_arg_expr_fixup_ptr  daefp;

  db_enter(3, "redecl_member_function");
  /* Issue the error.  Then, if the declarations are close enough, proceed
     as if function redeclaration were permitted. */
  pos_sy_error(ec_member_function_redeclaration, err_pos, sym);
  if (access != rp->source_corresp.access ||
      (is_virtual && !rp->is_virtual) ||
      (routine_type_is_nonstatic_member_function(rp->type) !=
         routine_type_is_nonstatic_member_function(member_type))) {
    /* The two declarations differ with respect to access specifier, virtual
       vs. nonvirtual, and/or static vs. nonstatic.  Rather than trying to
       resolve such differences, just throw the second declaration away. */
  } else {
    /* In the interests of better error recovery, merge the declarations. */
    /* If the new declaration specifies "inline", keep it, even if the
       previous declaration did not. */
    if (is_inline) rp->is_inline = TRUE;
    /* Reconcile the types. */
    reconcile_routine_types(rp, member_type, /*preserve_rout_type=*/TRUE,
                            /*preserve_type_ptr=*/FALSE);
    /* If any default arguments were encountered in the second declaration,
       the tokens were cached in an entry that makes reference to a now
       obsolete param type entry.  Find such references and change them to
       refer to the corresponding param type entry in the old param types
       list (the one that's being preserved). */
    ptp1 = rp->type->variant.routine.extra_info->param_type_list;
    ptp2 = member_type->variant.routine.extra_info->param_type_list;
    for (; ptp1 != NULL; ptp1 = ptp1->next, ptp2 = ptp2->next) {
      if (ptp2->has_default_arg) {
        /* A default arg appears in the current declaration.  Find the
           delayed scan fixup entry that points to this param type entry. */
        daefp = curr_routine_fixup->def_arg_expr_fixup_list;
        for (; daefp != NULL; daefp = daefp->next) {
          /* Stop when we find the entry that refers to the current
             param type entry. */
          if (daefp->param_type == ptp2) {
            daefp->param_type = ptp1;
            /* We intentionally do not set the has_default_arg flag to TRUE
               in ptp1.  This enables us to detect errors in the default arg
               list of the first declaration that would otherwise be missed.
               For instance,
                   class A { void f(int=1,int); void f(int,int=0); };
               According to ARM 8.2.6 (commentary on p. 141) an error should
               be issued on the first declaration of f(). has_default_arg is
               FALSE on the second param type entry when the cached default arg
               expression is scanned (see delayed_scan_of_default_arg_expr),
               and so the error can be detected. */
            /* Leave the inner loop but continue the outer loop. */
            break;
          } else {
            check_assertion(daefp->next != NULL);
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* for */
  }  /* if */
  db_exit();
}  /* redecl_member_function */


static void add_to_conversion_list(a_symbol_ptr                   sym,
                                   a_class_symbol_supplement_ptr  cssp)
/*
Create a conversion list entry for sym, which represents a conversion
operator, and add it to the conversion list in the class symbol supplement
pointed to by cssp.
*/
{
  a_conversion_list_entry_ptr    clep;

  db_enter(4, "add_to_conversion_list");
  /* Allocate the new entry and make it point to the symbol. */
  clep = alloc_conversion_list_entry();
  clep->symbol = sym;
  /* Add it to the list associated with the parent class. */
  clep->next = cssp->conversion_list;
  cssp->conversion_list = clep;
  db_exit();
}  /* add_to_conversion_list */


static a_symbol_ptr decl_member_function(
                                   a_symbol_locator        *locator,
                                   a_type_ptr              class_type,
                                   a_type_ptr              member_type,
                                   a_func_info_block_ptr   func_info,
                                   an_access_specifier     access,
                                   a_boolean               is_virtual,
                                   a_boolean               compiler_generated,
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
  a_symbol_ptr                  sym, overload_sym;
  a_routine_ptr                 rtn;
  a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(class_type);
  a_boolean                     const_object_okay, dummy_flag;
  a_type_ptr                    tp;

  db_enter(3, "decl_member_function");
  /* If this is a user-defined conversion or an overloaded operator,
     check for errors in the argument list.  Note that this is done before
     creating the symbol, since an invalid conversion or operator should not
     be added to the overload list. */
  check_operator_function_params(member_type, class_type, locator);
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
  rtn = sym->variant.routine.ptr;
  if (rtn != NULL) {
    /* symbol_for_member_function has returned a symbol that has already been
       declared.  It is an error to redeclare a member function, but we try
       merge the declarations anyway. */
    redecl_member_function(sym, member_type, access,
                           (a_boolean)func_info->is_inline, is_virtual,
                           &locator->source_position);
  } else {
    sym->class_of_which_a_member = class_type;
    /* Create the routine entry for the member function. */
    /* The routine is allocated in the current memory region, as indicated
       by curr_il_region_number -- i.e., in the memory region of the scope in
       which its class is declared. */
    /* Member functions are static by default. */
    rtn = make_routine(member_type, (a_storage_class)sc_static,
                       /*at_file_scope=*/FALSE, /*add_to_list=*/TRUE);
    sym->variant.routine.ptr = rtn;
    /* Set the source correspondence, including the access specifier. */
    set_source_corresp(&rtn->source_corresp, sym);
    rtn->source_corresp.class_of_which_a_member = class_type;
    /* Member functions should have the same name linkage as the class of
       which they are members.  For now, the class will have internal or no
       linkage.  If and when its linkage is promoted to C++, the linkage of
       the member functions will also be changed. */
    rtn->source_corresp.name_linkage = class_type->source_corresp.name_linkage;
    rtn->source_corresp.access = access;
    rtn->is_inline = func_info->is_inline;
    if (compiler_generated) {
      rtn->compiler_generated = TRUE;
    } else {
      if (func_info->is_definition) {
        mark_defined(sym, &locator->source_position);
      } else {
        mark_declared(sym, &locator->source_position);
      }  /* if */
    }  /* if */
    add_throw_specification(func_info, rtn);
    if (cssp->is_nonreal_class) {
      /* This symbol represents a member function of a prototype instantiation
         of a class template.  As such it is a quasi function template itself.
         Set it up to look like that. */
      a_template_instance_ptr           tip;
      a_template_symbol_supplement_ptr  tssp;

      sym->variant.routine.instance_ptr = tip = alloc_template_instance();
      tip->instance_sym = tip->template_sym = sym;
      tip->template_info = tssp = alloc_template_symbol_supplement(sym->kind);
      tssp->variant.function.routine = rtn;
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
      } else if (rtn->opname_kind == (an_opname_kind)onk_new) {
        cssp->has_operator_new = TRUE;
      } else if (rtn->opname_kind == (an_opname_kind)onk_delete) {
        cssp->has_operator_delete = TRUE;
      }  /* if */
    } else if (locator->is_conversion_name) {
      /* User-defined conversion function. */
      a_boolean  is_usable = TRUE;

      rtn->special_kind = (a_special_function_kind)sfk_conversion;
      /* Check the target type of the conversion -- which is the return type
         of rout_type. */
      tp = skip_typerefs(rtn->type->variant.routine.return_type);
      if (is_reference_type(tp)) {
        tp = skip_typerefs(type_pointed_to(tp));
      }  /* if */
      if (tp == class_type) {
        is_usable = FALSE;
      } else if (is_class_struct_union_type(tp)) {
        if (find_base_class_of(class_type, tp) != NULL) {
          is_usable = FALSE;
        } else {
          /* The target type of the conversion is a class or ref-to-class
             type: set a flag to mark it as target of a conversion. */
          (symbol_supplement_for_class(tp))->
                                   target_of_conversion_function = TRUE;
        }  /* if */
      } else if (is_void_type(tp)) {
        /* Except in cfront-compatibility mode, conversion to void type will
           already have been checked for. */
        check_assertion(cfront_compatibility_mode);
        is_usable = FALSE;
      }  /* if */
      if (is_usable) {
        /* Create a conversion list entry.  This list provides an alternative
           to traversing the entire symbols list for a class to find its
           conversion functions. */
        add_to_conversion_list(sym, cssp);
      } else {
        /* Conversion to the same type or a reference to the same type or to
           a base class or a reference to a base class "is never used" (WP
           12.3.2; that is, it is not used in implicit or explicit conversions
           but only in an explicit invocations of the function). */
        pos_sy_diagnostic(cfront_compatibility_mode ? es_remark : es_warning,
                          ec_conversion_function_not_usable,
                          &locator->source_position, sym);
      }  /* if */
    } else {
      rtn->special_kind = spec_kind;
    }  /* if */
    /* If "virtual" was specified in the declaration, mark the routine as
       virtual.  Even if it wasn't, its virtualness can be inherited.  In
       either case record the relationship between the current routine and
       its appearance in the base classes of the current class. */
    if (check_for_virtual_function(is_virtual, sym, class_type,
                                   &locator->source_position)) {
      /* Classes with virtual functions require constructors. */
      cssp->constructor_required = TRUE;
      /* Classes with virtual functions cannot be constructed or assigned
         by bitwise copying. */
      cssp->construction_by_bitwise_copy_allowed = FALSE;
      cssp->assignment_by_bitwise_copy_allowed = FALSE;
    }  /* if */
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
      if (is_copy_constructor(rtn, class_type, &const_object_okay,
          &dummy_flag)) {
        cssp->has_copy_constructor = TRUE;
        cssp->has_copy_constructor_for_const_object |= const_object_okay;
        if (!compiler_generated) {
          /* If a user-defined copy constructor is declared for the class,
             construction by bitwise copying is not allowed.  (On the other
             hand, this flag *may* be TRUE even when the compiler generates a
             a copy constructor.) */
          cssp->construction_by_bitwise_copy_allowed = FALSE;
        }  /* if */
      }  /* if */
    } else if (spec_kind == (a_special_function_kind)sfk_destructor) {
      /* Set the pointer to the destructor symbol in the class symbol
         supplement. */
      cssp->destructor = sym;
    }  /* if */
    /* Do checking associated with function overloading. */
    if (overload_sym != NULL && !cssp->is_nonreal_class) {
      a_symbol_ptr  other_sym = sym->next;

      check_assertion(sym ==
                        overload_sym->variant.overloaded_function.symbols);
      check_assertion(other_sym != NULL &&
                      other_sym->kind == (a_symbol_kind)sk_member_function);
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
  if (rout_sym->class_of_which_a_member != class_type) {
    pure_specifier_allowed = FALSE;
  } else {
    pure_specifier_allowed =
             (rout_sym->kind == (a_symbol_kind)sk_function_template) ?
                  rout_sym->variant.template_info->
                                  variant.function.routine->is_virtual :
                  rout_sym->variant.routine.ptr->is_virtual;
  }  /* if */
  if (!pure_specifier_allowed && !suppress_error) {
    pos_error(ec_pure_specifier_on_nonvirtual_function, &pos_curr_token);
  }  /* if */
  /* Advance past the "=". */
  (void)get_token();
  if (curr_token == tok_int_constant && const_for_curr_token.is_simple_zero) {
    /* Token following "=" is "0".  Note that we don't test for an integer
       value of zero but rather for the literal "0", since "= 00" should
       elicit an error. */
    if (pure_specifier_allowed) {
      /* Update the routine and class type enties. */
      rout_sym->variant.routine.ptr->pure_virtual = TRUE;
      class_type->variant.class_struct_union.abstract = TRUE;
      class_type->variant.class_struct_union.any_pure_virtual_functions = TRUE;
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
constant and entering the name in the symbol table.  member_type is
guaranteed to be a const-qualified scalar type.  This construct is an
extension.  Such a declaration is of the form:

  decl-specifiers declarator = constant-expression ;

where the decl-specifiers have no explicit storage class and "const" but
no other qualifier, and where the resulting type is a scalar type -- e.g.,

  class A {
    const int i = 10;              // member constant
    const float f = 1.0;           // member constant
    char * const s = "abc";        // member constant
    A * const pa = (A *)0;         // member constant
    // Added for comparison:
    const int j;                   // nonstatic data member
    static const int k;            // static data member
    static const int l = 10;       // static data member, syntax error
  };
*/
{
  a_symbol_ptr     sym;
  a_constant_ptr   cp;

  db_enter(3, "decl_member_constant");
  /* The current token is the "=".  Pointing to it issue a diagnostic that this
     is a nonstandard construct.  This is a strict ANSI diagnostic in
     strict ANSI mode, otherwise it is a remark. */
  diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_remark,
             ec_nonstd_const_member);
  /* Advance past the "=". */
  (void)get_token();
  /* Scan the constant expression. */
  cp = alloc_constant((a_constant_repr_kind)ck_error);
  scan_constant_initializer_expression(member_type, cp);
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
  mark_defined(sym, &locator->source_position);
  add_to_constants_list(cp, /*at_file_scope=*/FALSE);
  db_exit();
}  /* decl_member_constant */


static void decl_static_data_member(a_symbol_locator *locator,
                                    a_type_ptr       class_type,
                                    a_type_ptr       member_type,
                                    an_access_specifier access,
                                    a_boolean        is_anonymous_union,
                                    a_boolean        is_nonreal_class,
                                    a_symbol_ptr     corresp_prototype_tag_sym)
/*
Do processing for a static data member, including entering it in the symbol
table.
*/
{
  a_symbol          *sym;
  a_variable        *var;

  db_enter(3, "decl_static_data_member");
  /* Enter a new symbol in the symbol table. */
  sym = enter_local_symbol((a_symbol_kind)sk_static_data_member, locator,
                           decl_scope_level, /*suppress_redecl_error=*/FALSE);
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
  sym->variant.static_data_member.variable = var;
  var->source_corresp.class_of_which_a_member = class_type;
  /* Static data members will have the same name linkage as the class of
     which they are members.  For now, the class will have internal linkage.
     If and when its linkage is promoted to C++, the linkage of the static
     data members will also be changed. */
  var->source_corresp.name_linkage = class_type->source_corresp.name_linkage;
  var->source_corresp.access = access;
  /* Set the source correspondence fields of the variable. */
  set_source_corresp(&var->source_corresp, sym);
  /* This is entered as a declaration rather than a definition, since the
     definition must appear outside the class definition. */
  mark_declared(sym, &locator->source_position);
  if (is_anonymous_union) {
    /* A static data members is not allowed to be an anonymous union.  An error
       will have been issued already, but promote the fields anyway. */
    check_anonymous_union_symbols(class_type, (a_field_ptr)NULL, var);
  }  /* if */
  /* Special processing for static data members of template classes. */
  if (corresp_prototype_tag_sym != NULL || is_nonreal_class) {
    /* A nonnull instance_ptr marks this static data member as a member of
       a (real or nonreal) instantiation of a class template. */
    if (!is_error_locator(*locator)) {
      a_template_instance_ptr  tip = alloc_template_instance();
      sym->variant.static_data_member.instance_ptr = tip;
      tip->instance_sym = sym;
      if (is_nonreal_class) {
        /* A member of a prototype instantiation. */
        tip->template_sym = sym;
        tip->template_info = alloc_template_symbol_supplement(
                                       (a_symbol_kind)sk_static_data_member);
      } else {
        /* We must be in the midst of a template class instantiation.  We need
           to bind this static data member to the static data member template
           that was created for it in the prototype instantiation.  This will
           enable the compiler to generate a definition if a defining template
           is declared. */
        find_static_data_member_template(sym, corresp_prototype_tag_sym);
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) db_symbol(sym, "", 4);
#endif /* DEBUG */
  db_exit();
}  /* decl_static_data_member */


static a_boolean is_assignment_operator_for_copy(
                                               a_symbol_ptr  sym,
                                               a_boolean     *is_ref_arg,
                                               a_boolean     *accepts_const,
                                               a_boolean     *accepts_volatile)
/*
Return TRUE if sym qualifies as an assignment operator that can copy a
class object (ARM 12.8).  It qualifies if its first parameter has a type of
"A", "A&", or "const A&", where "A" is the class of which it is a member.
(The logic also supports a more relaxed (and dubious) reading of the ARM
whereby a first parameter involving type B also qualifies if B is a base
class of A; this interpretation is supported to provide compatibility with
other C++ compilers, which work this way.)  Set *is_ref_arg to TRUE if the
first parameter is a reference type.  Set *accepts_const and
*accepts_volatile based on how the first parameter is qualified.
*/
{
  a_boolean         found = FALSE;
  a_param_type_ptr  ptp;
  a_type_ptr        tp;

  ptp = routine_symbol_type(sym)->variant.routine.extra_info->param_type_list;
  check_assertion(ptp != NULL);
  tp = skip_typerefs(ptp->type);
  if (is_reference_type(tp)) {
    /* Reference argument. */
    tp = type_pointed_to(tp);
    *is_ref_arg = TRUE;
  } else {
    /* Not a reference argument. */
    *is_ref_arg = FALSE;
  }  /* if */
  if (is_class_struct_union_type(tp) &&
      is_same_class_or_base_class_thereof(sym->class_of_which_a_member, tp)) {
    /* Found it. */
    found = TRUE;
    /* Check the qualifiers. */
    *accepts_const = is_const_qualified_type(tp);
    *accepts_volatile = is_volatile_qualified_type(tp);
  }  /* if */
  return found;
}  /* is_assignment_operator_for_copy */


static a_boolean assignment_operator_for_copy_exists(a_symbol_ptr  sym,
                                                     a_boolean     *const_okay)
/*
Return TRUE if sym is not NULL and qualifies as an assignment operator that
can copy a class object (ARM 12.8).  If sym is an overloaded function,
return TRUE if at least one of the functions qualifies.  Set *const_okay
TRUE if a const object can be copied.
*/
{
  a_boolean         sym_is_overloaded;
  a_boolean         is_ref_arg, accepts_const, accepts_volatile;
  a_boolean         found_assignment_operator_for_copy = FALSE;

  db_enter(4, "assignment_operator_for_copy_exists");
  if (sym == NULL) {
    *const_okay = TRUE;
  } else {
    *const_okay = FALSE;
    sym_is_overloaded = (sym->kind == (a_symbol_kind)sk_overloaded_function);
    if (sym_is_overloaded) sym = sym->variant.overloaded_function.symbols;
    /* Loop through the one or more symbols looking for one with the right
       argument type. */
    for (; sym != NULL; sym = sym_is_overloaded ? sym->next : NULL) {
      if (is_assignment_operator_for_copy(sym, &is_ref_arg, &accepts_const,
                                          &accepts_volatile)) {
        /* Found an assignment operator that can serve to make a copy of the
           current class. */
        found_assignment_operator_for_copy = TRUE;
        /* If it takes the object to be copied by value, a const object
           may be copied; if it takes it by reference, a const qualifier must
           be present on the parameter declaration. */
        if (!is_ref_arg || accepts_const) {
          *const_okay = TRUE;
          break;
        } else {
          /* This one does not accept a const object, but another in the
             overload list might, so keep looping. */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
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

  db_enter(4, "is_valid_union_field");
  if (is_array_type(tp)) tp = skip_typerefs(underlying_array_element_type(tp));
  if (is_class_struct_union_type(tp)) {
    cssp = symbol_supplement_for_class(tp);
    if (cssp->constructor != NULL || cssp->destructor != NULL) {
      is_valid = FALSE;
    } else if (cssp->assignment_operator != NULL) {
      /* Check for existence of a user defined assignment operator function.
         There may be a compiler-generated assignment operator -- that's okay.
         But if it's overloaded, there must be a user-defined operator. */
      sym = cssp->assignment_operator;
      if (sym->kind == (a_symbol_kind)sk_overloaded_function ||
          !sym->variant.routine.ptr->compiler_generated) {
        is_valid = FALSE;
      }  /* if */
    }  /* if */
    if (!is_valid) {
      pos_ty_error(ec_bad_union_field, pos, tp);
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
  an_access_specifier            access, assoc_object_access;
  a_boolean                      access_error_already_issued = FALSE;
  a_boolean                      member_function_error_already_issued = FALSE;
  a_boolean                      is_overloaded;
  a_type_ptr                     object_type, tp;

  db_enter(4, "check_anonymous_union_symbols");
  if (assoc_var_object != NULL) {
    object_type = assoc_var_object->type;
    ctsp = object_type->variant.class_struct_union.extra_info;
    ctsp->anonymous_union_kind = (an_anonymous_union_kind)auk_variable;
    assoc_object_access = assoc_var_object->source_corresp.access;
  } else {
    object_type = assoc_field_object->type;
    ctsp = object_type->variant.class_struct_union.extra_info;
    ctsp->anonymous_union_kind = (an_anonymous_union_kind)auk_field;
    ctsp->anonymous_union_field = assoc_field_object;
    assoc_object_access = assoc_field_object->source_corresp.access;
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
    switch (sym->kind) {
      case sk_field:
        /* Unlink the symbol from the inactive list and link it back into the
           symbol table in the current scope. */
        sym->class_of_which_a_member = class_type;
        /* The fields of an anonymous union within a class take on the access
           specifier of the anonymous union itself; the fields of a variable
           anonymous union should be (i.e., should remain) public. */
        sym->variant.field.ptr->source_corresp.access = assoc_object_access;
        remove_from_inactive_symbols_list(sym);
        reenter_symbol(sym, decl_scope_level, /*suppress_error=*/FALSE);
        if (assoc_var_object != NULL) {
          /* Update the IL. */
          sym->variant.field.anonymous_union_variable = assoc_var_object;
        }  /* if */
        break;
      case sk_member_function:
      case sk_overloaded_function:
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
          for (; mf_sym != NULL;
                 mf_sym = is_overloaded ? mf_sym->next : NULL) {
            if (!mf_sym->variant.routine.ptr->compiler_generated) {
              error(ec_anon_union_member_function);
              member_function_error_already_issued = TRUE;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
        break;
      case sk_type:
      case sk_class_or_struct_tag:
      case sk_union_tag:
      case sk_enum_tag:
        /* Unlink the symbol from the inactive list and link it back into the
           symbol table in the current scope. */
        tp = type_symbol_type(sym);
        tp->source_corresp.class_of_which_a_member = class_type;
        sym->class_of_which_a_member = class_type;
        /* The members of an anonymous union within a class take on the access
           specifier of the anonymous union itself; the members of a variable
           anonymous union should be (i.e., should remain) public. */
        tp->source_corresp.access = assoc_object_access;
        remove_from_inactive_symbols_list(sym);
        reenter_symbol(sym, decl_scope_level, /*suppress_error=*/FALSE);
        break;
      case sk_constant:
        /* An enum constant. */
        sym->variant.constant->source_corresp.class_of_which_a_member =
                                  sym->class_of_which_a_member = class_type;
        sym->variant.constant->source_corresp.access = assoc_object_access;
        remove_from_inactive_symbols_list(sym);
        reenter_symbol(sym, decl_scope_level, /*suppress_error=*/FALSE);
        break;
#if CHECKING
      default:
        internal_error("check_anonymous_union_symbols: unexpected sym kind");
#endif /* CHECKING */
    }  /* switch */
  }  /* for */
  db_exit();
}  /* check_anonymous_union_symbols */


static void decl_nonstatic_data_member(a_symbol_locator    *locator,
                                       a_layout_block_ptr  lob,
                                       a_type_ptr          *member_type,
                                       an_access_specifier access,
				       a_boolean	   unnamed_field,
				       a_boolean	   is_anonymous_union,
                                       a_field_ptr         *end_of_list)
/*
Scan a nonstatic data member of a class, struct, or union, create a field
entry to represent it in the IL, and create an entry in the symbol table
for it if it has a name.  class_type is a pointer to the tk_class,
tk_struct, or tk_union type entry for the entity of which the member is a
member.  *locator and *member_type describe what is so far known about the
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
  a_type_ptr                     class_type = lob->class_type;
  long                           bit_field_size = 0;
  a_field_ptr		         field;
  a_symbol_ptr		         member_sym = NULL;
  a_class_symbol_supplement_ptr  cssp;
  a_boolean                      bit_field_is_signed = FALSE;

  db_enter(3, "decl_nonstatic_data_member");
  if (class_type->kind == (a_type_kind)tk_union) {
    /* All fields in a union have offset zero. */
    local_byte_offset = 0;
    local_bit_offset = 0;
    if (C_dialect == C_dialect_cplusplus) {
      /* An object of a class with a constructor, a destructor, or a user-
         defined assignment operator cannot be a member of a union. */
      if (!is_valid_union_field(*member_type, &locator->source_position)) {
        *member_type = error_type();
      }  /* if */
    }  /* if */
  } else {
    local_byte_offset = lob->byte_offset;
    local_bit_offset = lob->bit_offset;
  }  /* if */
  /* A colon next indicates a bit-field. */
  if (curr_token == tok_colon) {
    /* Scan the bit-field size and determine the bit-field type. */
    scan_bit_field_size(&unnamed_field, member_type, &bit_field_size,
                        &bit_field_is_signed, locator);
  }  /* if */
  /* Create the field entry.  For unnamed fields it will not actually become
     part of the IL. */
  field = alloc_field();
  field->type = *member_type;
  field->bit_size = (a_byte)bit_field_size;
  field->bit_field_is_signed = bit_field_is_signed;
  /* For an unnamed field, do not create the field symbol. */
  if (unnamed_field) {
    /* All field entries for an unnamed fields share the same symbol.  It is
       used for easy identification.  These field entries are for front-end
       use only and are thrown away. */
    field->source_corresp.assoc_info = (char *)unnamed_field_symbol();
  } else {
    if (!is_anonymous_union) {
      /* Create the field symbol. */
      member_sym = enter_local_symbol((a_symbol_kind)sk_field, locator,
                                      depth_scope_stack,
                                      /*suppress_redecl_error=*/FALSE);
      member_sym->class_of_which_a_member = class_type;
      member_sym->variant.field.ptr = field;
      set_source_corresp(&(field->source_corresp), member_sym);
      mark_defined(member_sym, &locator->source_position);
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
#else /* if !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE */
  /* Fields are allocated in groups based on access.  Only public fields are
     allocated at this time.  The rest are handled after all the fields have
     been seen. */
  if (access == (an_access_specifier)as_public) {
#endif /* !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE */
    if (!set_field_size_and_offset(field, &local_byte_offset,
                                   &local_bit_offset, &lob->alignment)) {
      /* FALSE was returned, which means an overflow error was encountered in
	 computing the new size of the struct -- i.e., this field will not
	 fit.  Remember it, so that only one such error is put out. */
      if (!lob->any_overflow) {
        error(ec_struct_too_large);
	lob->any_overflow = TRUE;
      }  /* if */
    } else {
      /* Offset values were modified.  Save highest offset for unions, last
         offset for structs and classes, for use in establishing the size of
         the overall aggregate. */
      if (class_type->kind != (a_type_kind)tk_union ||
          local_byte_offset > lob->byte_offset ||
          (local_byte_offset == lob->byte_offset &&
           local_bit_offset > lob->bit_offset)) {
        lob->byte_offset = local_byte_offset;
        lob->bit_offset = local_bit_offset;
      }  /* if */
    }  /* if */
#if !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE
  } else {
    /* No access other than public should be possible in non-C++ modes. */
    check_assertion(C_dialect == C_dialect_cplusplus);
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
  if (C_dialect == C_dialect_cplusplus) {
    /* In C++ we need to keep track of whether any members have reference
       type. */
    cssp = symbol_supplement_for_class(class_type);
    if (is_reference_type(*member_type)) {
      cssp->any_ref_member = TRUE;
      /* Assignment by bitwise copy is not allowed when a class has reference
         type members. */
      cssp->assignment_by_bitwise_copy_allowed = FALSE;
    }  /* if */
    /* Record that there is at least one nonstatic data member in the class. */
    cssp->any_nonstatic_data_members = TRUE;
  }  /* if */
  /* Remember if any member of the class, struct, or union is const-
     qualified, including recursively the members of any contained
     classes, structs, or unions.  This is useful for determination of
     modifiable lvalues (see 3.2.2.1). */
  if (type_or_element_type_is_const_qualified(*member_type) ||
      (is_class_struct_union_type(*member_type) &&
       skip_typerefs(*member_type)->
                            variant.class_struct_union.any_const_member)) {
    class_type->variant.class_struct_union.any_const_member = TRUE;
    if (C_dialect == C_dialect_cplusplus) {
      /* Assignment by bitwise copy is not allowed when a class has const
         qualified members. */
      cssp->assignment_by_bitwise_copy_allowed = FALSE;
    }  /* if */
  }  /* if */
  if (is_aggregate_or_union_type(*member_type)) {
    /* If the member's type is class, struct, or union -- or array of class,
       struct, or union -- there is additional checking to be done. */
    a_type_ptr  tp = skip_typerefs(*member_type);
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
        /* The parent class cannot be copy-constructed or assigned by bitwise
           copying if the member class does not allow it. */
        if (!member_cssp->construction_by_bitwise_copy_allowed) {
          cssp->construction_by_bitwise_copy_allowed = FALSE;
        }  /* if */
        if (!member_cssp->assignment_by_bitwise_copy_allowed) {
          cssp->assignment_by_bitwise_copy_allowed = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    if (member_sym != NULL) {
      db_symbol(member_sym, "", 2);
    }  /* if */
    fprintf(f_debug, "final byte offset = %lu", lob->byte_offset);
    if (lob->bit_offset > 0) {
      fprintf(f_debug, ", final bit offset = %d", lob->bit_offset);
    }  /* if */
    fprintf(f_debug, ", max alignment = %d\n", (int)lob->alignment);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* decl_nonstatic_data_member */


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
  a_routine_type_supplement *extra_info;
  a_symbol_locator          locator;
  a_source_position         pos;
  a_func_info_block         func_info;

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
  /* Create a locator for the symbol that will be created. */
  pos = class_type->source_corresp.decl_position;
  if (sfkind == (a_special_function_kind)sfk_operator) {
    make_opname_locator((an_opname_kind)onk_assign, &locator, &pos);
  } else {
    make_locator_for_symbol((a_symbol_ptr)class_type->
                                                  source_corresp.assoc_info,
                            &locator);
    if (sfkind == (a_special_function_kind)sfk_constructor) {
      change_class_locator_into_constructor_locator(&locator, &pos);
    } else {
      tildize_locator(&locator);
    }  /* if */
  }  /* if */
  clear_func_info(&func_info);
  if (exceptions_enabled) func_info.throw_position = pos_curr_token;
  func_info.is_inline = TRUE;
  /* Create a symbol and enter it in the symbol table, and create a routine
     entry and add it to the routines list for the current scope. */
  (void)decl_member_function(&locator, class_type, rout_type, &func_info,
                             (an_access_specifier)as_public,
                             /*is_virtual=*/FALSE,
                             /*compiler_generated=*/TRUE, sfkind);
  /* It can be that the head of symbols list for the scope has been
     modified (it may have been changed to an sk_overloaded_function, or
     it may have been empty), so update the class symbol supplement, just to
     be safe. */
  (symbol_supplement_for_class(class_type))->symbols =
                                      scope_stack[depth_scope_stack].symbols;
  db_exit();
}  /* generate_special_function */

#if NEW_CAN_BE_FOLDED_INTO_CTOR

void set_class_assoc_operator_new_routine(a_type_ptr class_type)
/*
Determine the default operator new() function to be used for the indicated
class and record it in the class's assoc_operator_new_routine field.
*/
{
  a_symbol_ptr                sym;
  a_class_type_supplement_ptr ctsp;

  check_assertion(is_immediate_class_type(class_type));
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp->assoc_operator_new_routine == NULL) {
    /* Use the class "new" if there is one, and otherwise the global operator
       new. */
    sym = opname_member_function_symbol((an_opname_kind)onk_new, class_type);
    if (sym != NULL) {
      /* There is a class-specific operator new() (or several).  See if
         there is a default (one-argument) version. */
      sym = extract_default_operator_new_sym(sym);
      /* Note that no access or ambiguity checking is done.  If it's
         appropriate, it's done at the point of call. */
    } else {
      /* Look for a global operator new(). */
      sym = opname_function_symbol((an_opname_kind)onk_new);
      /* "new" can be overloaded; find the default (one-argument) version
         of the routine if so.  Since the default version always exists,
         we must find something here. */
      sym = extract_default_operator_new_sym(sym);
      check_assertion(sym != NULL);
    }  /* if */
    if (sym != NULL) {
      ctsp->assoc_operator_new_routine = sym->variant.routine.ptr;
    }  /* if */
  }  /* if */
}  /* set_class_assoc_operator_new_routine */

#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */

static void make_default_constructor_body(a_scope_ptr  scope)
/*
Create the body for a default constructor or a default copy constructor.  It
will return a pointer to the constructed object.
*/
{
  a_routine_ptr                  rp;
  a_statement_ptr                sp;
  a_routine_type_supplement_ptr  rtsp;
  a_variable_ptr                 vp;
  a_param_type_ptr               ptp;

  db_enter(4, "make_default_constructor_body");
  rp = scope->variant.routine.ptr;
  /* Create the parameter variable -- needed for copy constructors only. */
  rtsp = (skip_typerefs(rp->type))->variant.routine.extra_info;
  ptp = rtsp->param_type_list;
  if (ptp != NULL) {
    vp = make_parameter(ptp->type, (a_storage_class)sc_auto,
                        (a_symbol_ptr)NULL);
    vp->assoc_param_type = ptp;
  }  /* if */    
  /* Create entries describing constructions to be done in the wrapper code. */
  scope->variant.routine.constructor_inits =
                                  ctor_initializer(rp, /*user_defined=*/FALSE);
  /* Create a statement block that is empty except for the return statement. */
  scope->assoc_block = alloc_statement((a_statement_kind)stmk_block);
  scope->assoc_block->variant.block.statements = sp =
          alloc_statement((a_statement_kind)stmk_return);
  sp->expr = this_param_value_expr();
  db_exit();
}  /* make_default_constructor_body */

#if DELETE_CAN_BE_FOLDED_INTO_DTOR

void set_class_assoc_operator_delete_routine(a_type_ptr class_type)
/*
Determine the operator delete() function to be used for the indicated class
and record it in the class's assoc_operator_delete_routine field.
*/
{
  a_symbol_ptr                sym;
  a_class_type_supplement_ptr ctsp;

  check_assertion(is_immediate_class_type(class_type));
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp->assoc_operator_delete_routine == NULL) {
    /* Use the class "delete" if there is one, and otherwise the global
       operator delete. */
    sym = opname_member_function_symbol((an_opname_kind)onk_delete,
                                        class_type);
    if (sym != NULL) {
      /* A member delete.  If it was inherited get the fundamental symbol. */
      if (sym->kind == (a_symbol_kind)sk_projection) {
        
        reduce_projection_symbol_to_fundamental_symbol(sym);
      }  /* if */
      /* Note that no access or ambiguity checking is done.  If it's
         appropriate, it's done at the point of call. */
    } else {
      sym = opname_function_symbol((an_opname_kind)onk_delete);
    }  /* if */
    /* Since delete cannot be overloaded, the symbol should not be overloaded
       and should not be a function template. */
    check_assertion(sym != NULL && is_function_symbol(sym));
    ctsp->assoc_operator_delete_routine = sym->variant.routine.ptr;
  }  /* if */
}  /* set_class_assoc_operator_delete_routine */

#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */

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
  db_exit();
}  /* make_default_destructor_body */


static a_boolean is_virtual_base_class_of(a_type_ptr  base_class_type,
                                          a_type_ptr  derived_type)
/*
Return TRUE if base_class_type is a virtual base class of derived_type.
*/
{
  a_base_class_ptr  bcp;

  /* Loop through the base classes. */
  for (bcp = base_classes_of(derived_type); bcp != NULL; bcp = bcp->next) {
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

  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
    if (is_virtual_base_class_of(vbcp->type, bcp->type)) {
      is_indirect = TRUE;
      break;
    }  /* if */
  }  /* for */
  return is_indirect;
}  /* virtual_base_class_is_indirect */


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
  a_boolean       ambiguous = FALSE;
  a_boolean       opass_sym_matches_exactly = FALSE;
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
    a_boolean  sym_matches_exactly;
    a_boolean  is_ref_arg;
    a_boolean  const_object_okay, volatile_object_okay;

    if (is_assignment_operator_for_copy(sym, &is_ref_arg, &const_object_okay,
                                        &volatile_object_okay)) {
      /* Found an assignment operator that can copy the current class. */
      if (!is_ref_arg) {
        /* Not a reference type, so qualifiers are ignored. */
        sym_matches_exactly = TRUE;
      } else {
        /* Reference type. */
        if ((const_object_required && !const_object_okay) ||
            (volatile_object_required && !volatile_object_okay)) {
          /* No match -- keep looking. */
          continue;
        } else if (const_object_okay == const_object_required &&
                   volatile_object_okay == volatile_object_required) {
          /* It's an exact match. */
          sym_matches_exactly = TRUE;
        } else {
          /* It's not quite an exact match. */
          sym_matches_exactly = FALSE;
        }  /* if */
      }  /* if */
      if (opass_sym != NULL) {
        /* We have a match on this symbol, but we've already had one before
           as well.  If one but not the other is an exact match, take the
           one that matches.  Otherwise it's an ambiguity.  */
        ambiguous = (sym_matches_exactly == opass_sym_matches_exactly);
        if (!sym_matches_exactly) continue;
      }  /* if */
      opass_sym = sym;
      opass_sym_matches_exactly = sym_matches_exactly;
      *pass_by_value = !is_ref_arg;
    }  /* if */
  }  /* for */
  opass_routine = NULL;
  if (opass_sym == NULL) {
    /* No applicable assignment operator function. */
    if (const_object_required && !volatile_object_required) {
      /* The common case:  missing const assignment operator function. */
      pos_ty_error(ec_missing_const_assignment_operator, err_pos, class_type);
    } else {
      /* Unusual case: volatile or const-volatile expected. */
      pos_ty_error(ec_no_suitable_assignment_operator, err_pos, class_type);
    }  /* if */
  } else {
    if (ambiguous) {
      /* More than one applicable assignment operator function. */
      pos_ty_error(ec_ambiguous_assignment_operator, err_pos, class_type);
    } else {
      /* Exactly one assignment operator function is best. */
      /* Check that the function is accessible and mark it referenced. */
      reference_to_implicitly_invoked_function(
                                  opass_sym, err_pos, (a_type_ptr)NULL,
                                  /*honor_virtual=*/FALSE, /*evaluated=*/TRUE,
                                  /*suppress_access_check=*/FALSE);
    }  /* if */
    opass_routine = opass_sym->variant.routine.ptr;
  }  /* if */
  db_exit();
  return opass_routine;
}  /* select_assignment_operator */


static void make_default_assignment_body(a_scope_ptr        scope,
                                         a_source_position  *err_pos)
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
  a_boolean                      bitwise_assign;

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
    const_source_var =
                 is_const_qualified_type(type_pointed_to(source_var->type));
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
        /* The source is the first parameter cast to the same base class. */
        source_expr = base_class_selection_expr(var_rvalue_expr(source_var),
                                                bcp);
        if (symbol_supplement_for_class(bcp->type)->
                         assignment_by_bitwise_copy_allowed) {
          /* A bitwise copy may be performed. */
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
                                          &bcp->decl_position, &pass_by_value);
          if (rp == NULL) {
            /* Error has already been issued in the subroutine. */
            continue;
          }  /* if */
          if (pass_by_value) {
            source_expr = add_indirection_to_node(source_expr);
            /* Make sure a copy constructor call is added if one is needed. */
            ptp = skip_typerefs(rp->type)->variant.routine.extra_info->
                                                               param_type_list;
            source_expr = prep_rvalue_arg_expr(source_expr, ptp, err_pos);
          }  /* if */
          sp = sp->next = make_call_assignment_statement(rp, dest_expr,
                                                         source_expr, err_pos);
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
        /* The check for const and ref members has already been done. */
        /* If this is an array, we need the element type. */
        if (is_array_type(tp)) {
          array_type = tp;
          tp = skip_typerefs(underlying_array_element_type(tp));
        } else {
          array_type = NULL;
        }  /* if */
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
            bitwise_assign = TRUE;
          } else {
            a_statement_ptr call_stmt;
            /* A bitwise copy may not be done.  Find the default assignment
               operator and put out a call to it. */
            bitwise_assign = FALSE;
            rp = select_assignment_operator(tp, const_source_var,
                                            /*volatile_object_required=*/FALSE,
                                            &fp->source_corresp.decl_position,
                                            &pass_by_value);
            if (rp == NULL) {
              /* Error has already been issued in the subroutine. */
              continue;
            }  /* if */
            source_expr = field_lvalue_selection_expr(source_expr, fp);
            if (array_type != NULL) {
              /* Copying an array of classes.  Generate a loop around the
                 call of the assignment routine, like
                   tmp = 0;
                   do {
                     assignfunc(&dest[tmp], &src[tmp]);
                   } while (++tmp < num_elements);
              */
              a_variable_ptr   temp_var;
              an_expr_node_ptr temp_node, temp_incr_node, compare_node;
              a_type_ptr       size_t_type =
                           integer_type((an_integer_kind)TARG_SIZE_T_INT_KIND);
              a_targ_size_t    num_elems;
              temp_var = alloc_temporary_variable(size_t_type);
              /* Make "tmp = 0;" */
              temp_node = var_lvalue_expr(temp_var);
              sp = sp->next = make_assignment_statement(temp_node,
                                node_for_integer_constant(0L,
                                       (an_integer_kind)TARG_SIZE_T_INT_KIND));
              /* Make "++tmp < num_elements". */
              temp_node = var_lvalue_expr(temp_var);
              temp_incr_node = make_operator_node(
                                          (an_expr_operator_kind)eok_ipre_incr,
                                          size_t_type, temp_node);
              num_elems = skip_typerefs(array_type)->size / tp->size;
              temp_incr_node->next = 
                                node_for_integer_constant((long)num_elems,
                                        (an_integer_kind)TARG_SIZE_T_INT_KIND);
              compare_node =
                      make_operator_node((an_expr_operator_kind)eok_ilt,
                                         integer_type((an_integer_kind)ik_int),
                                         temp_incr_node);
              /* Make the do-while statement. */
              sp = sp->next = alloc_statement(
                                        (a_statement_kind)stmk_end_test_while);
              sp->expr = compare_node;
              /* Convert the source and destination expressions from
                 pointer-to-array to pointer-to-array-element. */
              cast_node(&source_expr, make_pointer_type(tp),
                        /*is_implicit_cast=*/TRUE, err_pos);
              cast_node(&dest_expr, make_pointer_type(tp),
                        /*is_implicit_cast=*/TRUE, err_pos);
              /* Add the subscript to the source_expr. */
              source_expr->next = var_rvalue_expr(temp_var);
              source_expr =
                      make_operator_node((an_expr_operator_kind)eok_padd_subsc,
                                         source_expr->type, source_expr);
              /* Add the subscript to the dest_expr. */
              dest_expr->next = var_rvalue_expr(temp_var);
              dest_expr =
                      make_operator_node((an_expr_operator_kind)eok_padd_subsc,
                                         dest_expr->type, dest_expr);
            }  /* if */
            if (pass_by_value) {
              /* The assignment operator function takes its source by value. */
              source_expr = add_indirection_to_node(source_expr);
              /* Make sure a copy constructor call is added if one is
                 needed. */
              ptp = skip_typerefs(rp->type)->variant.routine.extra_info->
                                                               param_type_list;
              source_expr = prep_rvalue_arg_expr(source_expr, ptp, err_pos);
            }  /* if */
            /* Make the call of the assignment operator function. */
            call_stmt = make_call_assignment_statement(rp, dest_expr,
                                                       source_expr, err_pos);
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
               address). */
            source_expr = field_lvalue_selection_expr(source_expr, fp);
            sp = sp->next =
                       make_array_assignment_statement(dest_expr, source_expr);
        
          } else {
            /* Not an array.  The appropriate IL operator will be selected
               by make_assignment_statement. */
            source_expr = field_rvalue_selection_expr(source_expr, fp);
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


static void check_default_assignment_operator(a_type_ptr         class_type,
                                              a_source_position  *err_pos)
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
        if (is_reference_type(tp)) {
          /* An assignment operator should not be generated if a member has a
             ref type. */
          is_ref = TRUE;
        } else {
          if (is_array_type(tp)) tp = underlying_array_element_type(tp);
          /* An assignment operator should not be generated if a member has a
             const type. */
          if (is_const_qualified_type(tp)) is_const = TRUE;
        }  /* if */
        if (is_ref || is_const) {
          if (!err) {
            /* Multi-line diagnostic has not been started yet. */
            pos_start_error(ec_bad_default_assignment, err_pos);
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
  a_source_position              *err_pos;

  db_enter(4, "define_special_member_function");
  class_type = rout_ptr->source_corresp.class_of_which_a_member;
  if (symbol_supplement_for_class(class_type)->is_nonreal_class) {
    /* Don't bother generating the definition for a member of an unreal
       instantiation of a template class. */
  } else {
    /* Push a class symbol reactivation scope, to make class member names
       visible for processing the function definition. */
    push_class_reactivation_scope(class_type);
    /* Push the scope for the new function itself. */
    scope = push_scope((a_scope_kind)sck_function, NO_SCOPE_NUMBER,
                       (a_type_ptr)NULL, rout_ptr, (a_symbol_ptr)NULL,
                       (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL);
    /* Associate the scope to the routine entry and the routine entry to its
       type entry. */
    rout_ptr->assoc_scope = curr_il_region_number;
    rtsp = rout_ptr->type->variant.routine.extra_info;
    rtsp->assoc_routine = rout_ptr;
    scope->variant.routine.this_param_variable =
                  make_param_variable(rtsp->implicit_this_param_type,
                                      (a_storage_class)sc_auto);
    /* Enter the constructor and destructor initializers, to record possible
       implicit initializers. */
    if (rout_ptr->special_kind == (a_special_function_kind)sfk_constructor) {
      make_default_constructor_body(scope);
    } else if (rout_ptr->special_kind ==
                                  (a_special_function_kind)sfk_destructor) {
      make_default_destructor_body(scope);
    } else {
      /* Assignment operator case. */
      check_assertion(rout_ptr->special_kind ==
                                   (a_special_function_kind)sfk_operator &&
                      rout_ptr->opname_kind == (an_opname_kind)onk_assign);
      err_pos = &class_type->source_corresp.decl_position;
      check_default_assignment_operator(class_type, err_pos);
      make_default_assignment_body(scope, err_pos);
    }  /* if */
    /* End of statement block is unreachable because of the return
       statement. */
    scope->assoc_block->
                   variant.block.extra_info->end_of_block_reachable = FALSE;
    /* Terminate the function scope. */
    pop_scope();
    /* Terminate the class reactivation scope. */
    pop_class_reactivation_scope();
    /* Mark the symbol for this routine "defined". */
    ((a_symbol_ptr)rout_ptr->source_corresp.assoc_info)->defined = TRUE;
  }  /* if */
  db_exit();
}  /* define_special_member_function */


static a_boolean is_cfront_base_class_destructor_access_bug
				(a_symbol_ptr	sym,
				 a_routine_ptr	rp,
				 a_type_ptr	class_of_object)
/*
Cfront has a bug in which a private destructor in a base class can be
called when the derived class really should not have access to it.
This function, which should only be called in cfront mode, detects
the condition in which the access error should be suppressed.
*/
{
  a_boolean	result = FALSE;
  if (rp->special_kind == (a_special_function_kind)sfk_destructor &&
      sym->class_of_which_a_member != class_of_object &&
      class_of_object != NULL) {
    result = TRUE;
  }  /* if */
  return result;
}  /* is_cfront_base_class_destructor_access_bug */


void reference_to_implicitly_invoked_function
				(a_symbol_ptr       sym,
                                 a_source_position  *err_pos,
				 a_type_ptr         class_of_object,
                                 a_boolean          honor_virtual,
                                 a_boolean          evaluated,
                                 a_boolean          suppress_access_check)
/*
sym is points to a symbol for a special member function that is invoked
implicitly -- e.g., a copy constructor that is called when a class object
is passed by value or an assignment operator that is called when another
assignment operator function is being created.  Check that the special member
function is accessible and mark the routine entry referenced.  class_of_object
points to the type of the object for which the function is being called,
which is not always the same as the class of which a the function is
a member.  This is used to check protected member access which only applies
to objects of a derived class.  class_of_object may be NULL if protected member
access checking is not needed. Also, if the routine is compiler generated,
it may still need to be defined, since the definition may have been put off
until an actual reference occurred (e.g., ARM 12.8).  This function deals
with implicitly called constructors, destructors, assignment operators,
and conversion functions.  If honor_virtual is TRUE, and the function is
virtual, the reference is considered to be a virtual call; that means
the access control checking is done, but the IL entry is not marked as
referenced.  If evaluated is FALSE, the reference is within an unevaluated
expression; again, access control checking is done, but the IL entry is not
marked as referenced.  If suppress_access_check is TRUE, no access control
checking is done.
*/
{
  a_routine_ptr rp = sym->variant.routine.ptr;

  check_assertion(rp->special_kind ==
                               (a_special_function_kind)sfk_constructor ||
                  rp->special_kind ==
                               (a_special_function_kind)sfk_destructor ||
                  rp->special_kind ==
                               (a_special_function_kind)sfk_conversion ||
                  (rp->special_kind == (a_special_function_kind)sfk_operator &&
                   rp->opname_kind == (an_opname_kind)onk_assign));
  if (!suppress_access_check) {
    /* Check for accessibility. */
    if (!have_access_to_symbol(sym)) {
      an_error_severity	severity = es_error;
      /* Normally an error, but in cfront mode there is a special case
         involving a private base class destructor where we issue a warning. */
      if (cfront_compatibility_mode &&
          is_cfront_base_class_destructor_access_bug(sym, rp,
                                                     class_of_object)) {
        severity = es_warning;
      }  /* if */
      pos_sy_diagnostic(severity, ec_inaccessible_special_function,
                        err_pos, sym);
    } else if (class_of_object != NULL) {
      /* Protected members of a base class can only be accessed through an
         object of a derived class. */
      check_protected_member_access(sym, err_pos, class_of_object);
    }  /* if */
  }  /* if */
  if (!evaluated) {
    /* Unevaluated expression.  Do not set referenced (etc.). */
  } else if (rp->is_virtual && honor_virtual) {
    /* Virtual function call.  Do not set referenced (etc.) because the
       call might actually be of an overriding function. */
  } else {
    /* Non-virtual call. */
    mark_routine_referenced(rp);
  }  /* if */
}  /* reference_to_implicitly_invoked_function */


void force_definition_of_compiler_generated_routine(a_routine_ptr  rp)
/*
If rp points to a compiler-generated routine that is being referenced and
whose definition has not yet been generated, force the definition now.
*/
{
  a_special_function_kind  skind = rp->special_kind;

  if (rp->compiler_generated && rp->assoc_scope == NULL_region_number) {
    /* Only force a definition for constructors, destructors, and
       operator= functions.  In particular, do not try to define operator new
       and delete functions. */
    if (skind == (a_special_function_kind)sfk_constructor ||
        skind == (a_special_function_kind)sfk_destructor  ||
        (skind == (a_special_function_kind)sfk_operator &&
         rp->opname_kind == (an_opname_kind)onk_assign)) {
      define_special_member_function(rp);
    }  /* if */
  }  /* if */
}  /* force_definition_of_compiler_generated_routine */


static a_boolean default_assignment_of_const_object_okay(a_type_ptr class_type)
/*
We are about to create a compiler-generated default assignment operator.
Whether it can copy a const object is dependent on the assignment operators
defined for base classes and fields of the current class (class_type).
*/
{
  a_base_class_ptr               bcp;
  a_type_ptr                     tp;
  a_class_symbol_supplement_ptr  cssp;
  a_symbol_ptr                   sym;
  a_field_ptr                    fp;
  a_boolean                      const_okay = TRUE;

  db_enter(4, "default_assignment_of_const_object_okay");
  /* Check for const.  Do the base classes first. */
  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
    if (bcp->direct || bcp->is_virtual) {
      cssp = symbol_supplement_for_class(bcp->type);
      (void)assignment_operator_for_copy_exists(cssp->assignment_operator,
                                                &const_okay);
      if (!const_okay) goto done;
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
        (void)assignment_operator_for_copy_exists(cssp->assignment_operator,
                                                  &const_okay);
        if (!const_okay) goto done;
      }  /* if */
    }  /* if */
  }  /* for */
done:;
  db_exit();
  return const_okay;
}  /* default_assignment_of_const_object_okay */


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
  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
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
  a_boolean                     const_okay, dummy_flag;

  db_enter(3, "check_special_member_functions");
  cssp = symbol_supplement_for_class(class_type);
  if (cssp->constructor_required && cssp->constructor == NULL) {
    /* A default constructor needs to be generated. */
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
  if (assignment_operator_for_copy_exists(cssp->assignment_operator,
                                          &dummy_flag)) {
    /* If the user has already defined an assignment operator, neither
       is bitwise copying allowed nor must the compiler generate one. */
    cssp->assignment_by_bitwise_copy_allowed = FALSE;
  } else if (!cssp->assignment_by_bitwise_copy_allowed) {
    /* Only try to generate a default assignment operator if bitwise
       copying is not allowed. */
    const_okay = default_assignment_of_const_object_okay(class_type);
    ptp = alloc_param_type(make_reference_type(
                               make_qualified_type(class_type,
                                                   /*is_const=*/const_okay,
                                                   /*is_volatile=*/FALSE)));
    generate_special_function(class_type, ptp,
                              (a_special_function_kind)sfk_operator);
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
  a_base_class_ptr              bcp;
  a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(class_type);
  a_conversion_list_entry_ptr   clep, bcclep;
  a_symbol_locator              loc;
  a_boolean                     update = FALSE;
  a_symbol_ptr                  sym;

  /* Examine each direct base class. */
  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
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
          /* Create the projection symbol and record it in the new conversion
             list entry. */
          make_locator_for_symbol(bcclep->symbol, &loc);
          loc.specific_symbol = NULL;
          (void)find_projected_symbol(class_type, &loc, /*must_be_tag=*/FALSE,
                                      /*must_be_type_name=*/FALSE,
                                      /*add_to_active_list=*/TRUE,
                                      (a_symbol_ptr)NULL, &sym);
          check_assertion(sym != NULL);
          /* Allocate the new conversion list entry and link it in the
             list for the current class. */
          add_to_conversion_list(sym, cssp);
          update = TRUE;
        }  /* if */
        /* Get the next conversion list entry from the base class. */
      }  /* for */
    }  /* if */
    /* Get the next direct base class. */
  }  /* for */
  if (update) {
    /* Since the scope symbol list may have been empty before and since
       at least one new symbol has been added, update the symbols list
       attached to the class. */
    cssp->symbols = scope_stack[depth_scope_stack].symbols;
  }  /* if */
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
    case aak_variable:  aap->variant.variable =
                               sym->variant.static_data_member.variable; break;
    case aak_constant:  aap->variant.constant = sym->variant.constant;   break;
    case aak_type:      aap->variant.type = sym->variant.type;           break;
    case aak_routine:   aap->variant.routine = sym->variant.routine.ptr; break;
    case aak_field:     aap->variant.field = sym->variant.field.ptr;     break;
  }  /* switch */

  return aap;
}  /* new_access_adjustment */


static void access_adjustment_decl(an_access_specifier   access,
                                   a_type_ptr            class_type)
/*
The current token is a qualified name and the next token is a semicolon.
Syntactically, this is an access adjustment declaration.  If the declaration
is semantically sound, update the data base appropriately.  "access" is the
the access (explicitly specified or implicit) controlling the declaration,
and "class_type" indicates the class in which the declaration occurs.
*/
{
  an_access_specifier          progenitor_access;
  a_symbol_ptr                 projection_into_curr_class;
  a_symbol_ptr                 immediate_progenitor_sym;
  a_base_class_ptr             bcp;
  a_symbol_locator             locator;
  an_access_adjustment_ptr     aap;
  a_class_type_supplement_ptr  ctsp;
  a_boolean                    is_overloaded_function;
  a_symbol_ptr                 sym;
  an_access_specifier          function_access;
  a_type_ptr		       local_class_of_which_a_member;

  db_enter(4, "access_adjustment_decl");
  if (symbol_supplement_for_class(class_type)->any_nonreal_base_classes) {
    /* We must be in the midst of a prototype instantiation.  The entity
       specified for access adjustment may be a member of a nonreal base
       class (or of a base class of a nonreal base class).  We can't be sure
       about specializations of nonreal base classes at this point, nor is
       there any point in recording the access adjustment, so just bail out. */
    goto done;
  }  /* if */
  /* Get the class of which a member.  Normally the pointer from the
     specific symbol in the locator is used, but in the case of an
     undefined symbol from an error locator, we use the qualifier class type
     value from the locator (if not NULL). */
  if (locator_for_curr_id.specific_symbol->kind ==
                                              (a_symbol_kind)sk_undefined) {
    local_class_of_which_a_member = locator_for_curr_id.qualifier_class_type;
  } else {
    local_class_of_which_a_member = locator_for_curr_id.
                                    specific_symbol->class_of_which_a_member;
    /* In processing a qualified name the specific_symbol field of the locator
       will have been filled in. */
    check_assertion(curr_token == tok_identifier &&
                    locator_for_curr_id.specific_symbol->
                                          class_of_which_a_member != NULL);
  }  /* if */
  /* Be sure the class in the qualified name is one from which the current
     class is derived. */
  ctsp = class_type->variant.class_struct_union.extra_info;
  for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
    if (bcp->type == local_class_of_which_a_member) break;
  }  /* for */
  if (bcp == NULL) {
    /* Qualified name must identify a member of a base class of the current
       class.  Don't issue this error if we don't know the base class.  This
       can only occur if we have an error locator in which the
       qualifier_class_type field is NULL. */
    if (local_class_of_which_a_member != NULL) error(ec_bad_base_class);
    goto done;
  } else if (bcp->ambiguous) {
    type_error(ec_ambiguous_base_class, bcp->type);
    set_to_error_locator(locator_for_curr_id);
    goto done;
  }  /* if */
  if (locator_for_curr_id.specific_symbol->kind ==
                                              (a_symbol_kind)sk_undefined) {
    /* Not a valid member of what may or may not be a valid base class. No
       further processing can be done. */
    goto done;
  }  /* if */
  /* Look up the name without class qualification.  This will show whether
     the name has already been declared and if not give us the projection
     of the name into the current class. */
  clear_locator(&locator, &locator_for_curr_id.source_position);
  locator.symbol_header = locator_for_curr_id.symbol_header;
  (void)normal_id_lookup(&locator, IDL_NO_OPTIONS);
  if (locator.specific_symbol->kind == (a_symbol_kind)sk_projection) {
    projection_into_curr_class = locator.specific_symbol;
    if (projection_into_curr_class->
                           variant.projection.access_adjustment_made) {
      /* Name has already been declared in an access declaration. */
      pos_st_error(ec_id_already_declared,
                   &locator_for_curr_id.source_position,
                   locator_for_curr_id.symbol_header->identifier);
      goto done;
    }  /* if */
  } else {
    /* Name has already been declared in this scope.  Since tag names and
       nontag names can coexist in the same scope, check for this condition. */
    a_symbol_ptr  insert_sym = NULL;
    if (symbols_may_coexist_in_curr_scope(locator.specific_symbol,
                                          locator_for_curr_id.specific_symbol,
                                          &insert_sym,
					 /*supress_error=*/FALSE)) {
      clear_locator(&locator, &locator_for_curr_id.source_position);
      locator.symbol_header = locator_for_curr_id.symbol_header;
      (void)find_projected_symbol(class_type, &locator, /*must_be_tag=*/FALSE,
                                  /*must_be_type_name=*/FALSE,
                                  /*add_to_active_list=*/TRUE, insert_sym,
                                  &projection_into_curr_class);
    } else {
      /* Name has already been declared in the current scope and cannot
         appear in an access adjustment. */
      pos_st_error(ec_id_already_declared,
                   &locator_for_curr_id.source_position,
                   locator_for_curr_id.symbol_header->identifier);
      goto done;
    }  /* if */
  }  /* if */
  /* Find the immediate progenitor of projection_into_curr_class.  If the
     symbol originally specified in the source is a member of an indirect
     base class, look for its projection in a direct base class. */
  if (bcp->direct) {
    immediate_progenitor_sym = locator_for_curr_id.specific_symbol;
  } else {
    /* Use the first base class on the derivation path. */
#if 0
    /* Is the following loop correct?  Not likely. */
#endif /* if 0 */
    do {
      bcp = preferred_derivation_of(bcp)->path->base_class;
    } while (!bcp->direct);
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
    /* Access adjustment may not appear in the private part of a derived
       class declaration. */
    error(ec_access_adjustment_in_private_section);
  } else {
    sym = fundamental_symbol_of(projection_into_curr_class);
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      /* Check for uniform access on overloaded functions (requirement
         inferred from ARM 11.3, bottom of p. 246). */
      if (!max_access_of_overloaded_function(sym, &function_access)) {
        /* Functions overloading this name were not all declared with the
           same access. */
        sym_error(ec_bad_access_adjustment_with_overloading, sym);
        goto done;
      }  /* if */
      if (immediate_progenitor_sym->kind == (a_symbol_kind)sk_projection) {
        /* Indirectly inherited, so use the access from the projection
           symbol. */
        progenitor_access =
                     immediate_progenitor_sym->variant.projection.access;
      } else {
        /* Directly inherited name so we just want the access of the
           functions -- each of which has an access of function_access. */
        progenitor_access = function_access;
      }  /* if */
      is_overloaded_function = TRUE;
    } else {
      progenitor_access = access_for_symbol(immediate_progenitor_sym);
      is_overloaded_function = FALSE;
    }  /* if */
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
      goto done;
    }  /* if */
    /* This is a valid access adjustment. */
    projection_into_curr_class->variant.projection.access = access;
    if (is_overloaded_function) {
      sym = sym->variant.overloaded_function.symbols;
    } else if (sym->kind == (a_symbol_kind)sk_member_function) {
      /* If the projection symbol represents a conversion operator, be sure
         it is entered into the conversion list. */
      if (sym->variant.routine.ptr->special_kind ==
                                   (a_special_function_kind)sfk_conversion) {
        /* Allocate the new conversion list entry and link it in the
           list for the current class. */
        add_to_conversion_list(projection_into_curr_class,
                               symbol_supplement_for_class(class_type));
      }  /* if */
    }  /* if */
    for (; sym != NULL; sym = (is_overloaded_function ? sym->next : NULL)) {
      /* Create an access-adjustment entry to represent this declaration in
         the IL. */
      aap = new_access_adjustment(sym, access);
      /* Attach it the class type entry. */
      aap->next = ctsp->access_adjustments;
      ctsp->access_adjustments = aap;
    }  /* for */
  }  /* if */

done:
  db_exit();
}  /* access_adjustment_decl */


static a_symbol_ptr find_corresp_prototype_tag_sym(a_symbol_ptr  curr_sym)
/*
If a given tag symbol (curr_sym) represents an instantiation of a class
template, return the corresponding tag symbol for the prototype instantiation
of the same template.  If curr_sym is not an instantiation or if it is
itself a prototype instantiation, return NULL.

For example, given

  template <class T> class A {
    class B {
      class C { };
    };
  };
  A<int> a;

a prototype instantiation for A<T> will have been done, and in the process
symbols for the nested classes A<T>::B and A<T>::B::C will have been created.
With the real instantiation A<int> the "corresponding prototype instantiations"
are:   A<T> for A<int>, A<T>::B for A<int>::B, and A<T>::B::C for A<int>::B::C.

Note that the code for finding a nonnested prototype instantiation is quite
different from that for finding the corresponding nested class.  For the
latter case a recursive algorithm is used, since we have to navigate the
parent chain (e.g., climb up from A<int>::B to A<int>, find A<T>, then climb
back down to find A<T>::B).
*/
{
  a_symbol_ptr                   corresp_prototype_tag_sym = NULL;
  a_symbol_ptr                   sym, templ_sym;
  a_class_symbol_supplement_ptr  cssp;


  db_enter(3, "find_corresp_prototype_tag_sym");
  if (curr_sym->variant.class_struct_union.extra_info->is_nonreal_class) {
    /* Return NULL. */
  } else if (curr_sym->class_of_which_a_member != NULL) {
    /* curr_sym represents a nested class.  Find the corresponding prototype
       tag symbol of its parent class; then find the corresponding nested
       class within it. */
    a_type_ptr      tp = curr_sym->class_of_which_a_member;
    a_scope_number  decl_scope;

    sym = find_corresp_prototype_tag_sym(
                                 (a_symbol_ptr)tp->source_corresp.assoc_info);
    if (sym != NULL) {
      /* sym is the corresponding prototype tag symbol of the parent class.
         It represents a prototype instantiation of a class template or a
         class nested within a prototype instantiation. One of its own nested
         classes will be the nested class that corresponds to curr_sym: find
         a symbol for that nested class. */
      tp = sym->variant.class_struct_union.type;
      if (is_unnamed_class_symbol(curr_sym)) {
        /* Unusual case of an unnamed class -- e.g., an anonymous union.
           Look through the types list associated with the parent class. */
        tp = tp->variant.class_struct_union.extra_info->assoc_scope->types;
        for (; tp != NULL; tp = tp->next) {
          sym =(a_symbol_ptr)tp->source_corresp.assoc_info;
          if (sym != NULL && sym->kind == curr_sym->kind) {
            if (sym->decl_position.column == curr_sym->decl_position.column &&
                sym->decl_position.seq == curr_sym->decl_position.seq) {
              corresp_prototype_tag_sym = sym;
              break;
            }  /* if */
          }  /* if */
        }  /* for */
      } else {
        /* Normal case -- find the symbol with the same header as curr_sym
           and belonging to the scope that sym established. */
        decl_scope = 
               tp->variant.class_struct_union.extra_info->assoc_scope->number;
        if (is_incomplete_type(sym->variant.class_struct_union.type)) {
          /* We must still be in the midst of the prototype instantiation, so
             the symbol is still on the active list. */
          sym = curr_sym->header->symbol;
        } else {
          /* Look through the symbols on the inactive list. */
          sym = curr_sym->header->inactive_symbols;
        }  /* if */
        for (; sym != NULL; sym = sym->next) {
          if (sym->decl_scope == decl_scope && sym->kind == curr_sym->kind) {
            corresp_prototype_tag_sym = sym;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
      check_assertion(corresp_prototype_tag_sym != NULL);
    }  /* if */
  } else {
    /* curr_sym is not a nested class.  If it has a template symbol it may be
       an instantiation of a class template. */
    cssp = curr_sym->variant.class_struct_union.extra_info;
    templ_sym = cssp->class_template;
    if (templ_sym == NULL || cssp->is_specific_template_def) {
      /* The current symbol is not an instantiation (because it is
         not associated with a template) or else is a specific definition
         (i.e., provided by the user rather than generated by the compiler
         based on the template). */
    } else {
      /* Go through the instantiations of the associated template looking for
         the prototype instantiation (namely, a "nonreal" instantiation for
         which there is also a definition). */
      sym = templ_sym->
              variant.template_info->variant.class_template.instantiations;
      for (; sym != NULL; sym = sym->next) {
        if (sym->defined &&
            sym->variant.class_struct_union.extra_info->is_nonreal_class) {
          /* Found it. */
          corresp_prototype_tag_sym = sym;
          break;
        }  /* if */
      }  /* for */
      check_assertion(corresp_prototype_tag_sym != NULL);
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    if (corresp_prototype_tag_sym != NULL) {
      fputs("returning symbol for ", f_debug);
      db_type_name(type_symbol_type(corresp_prototype_tag_sym));
      fputc('\n', f_debug);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return corresp_prototype_tag_sym;
}  /* find_corresp_prototype_tag_sym */


a_boolean scan_class_definition(a_type_ptr    class_type,
                                a_scope_depth effective_decl_level,
                                a_boolean     is_local_class,
                                a_boolean     is_prototype_instantiation)

/*
Scan the body of a class definition, including the base classes list.
*/
{
  a_boolean               err = FALSE;
  an_access_specifier     access;
  a_symbol_ptr            tag_sym, corresp_prototype_tag_sym = NULL;
  a_boolean               first_declarator;
  a_decl_flag_set         dsi_flags;
  a_boolean               is_first_field;
  a_source_position       decl_start_pos;
  a_scope_ptr             scope_ptr;
  a_field_ptr             end_of_field_list = NULL;
  a_symbol_ptr            rout_sym;
  a_memory_region_number  region_to_switch_back_to;
  a_class_symbol_supplement_ptr
                          cssp;
  a_boolean               class_aggregate_ruled_out = FALSE;
  a_boolean               any_friend_decls = FALSE;
  a_routine_fixup_ptr     saved_routine_fixup;
  a_boolean               any_const_or_ref_fields = FALSE;
  a_layout_block          layout_block;
  a_boolean               is_template_instantiation;
  a_boolean               is_nonreal_instantiation = FALSE;
  a_boolean               error_on_def_in_return_type_already_issued;

  db_enter(3, "scan_class_definition");
  /* Set a flag to indicate whether we scanning a class template declaration
     for the sake of producing a "prototype instantiation" of the template.
     This amounts to scanning the declarative sections (i.e., no function
     bodies or default arg expressions) and issuing such syntax errors as can
     be detected. */
  is_template_instantiation = is_prototype_instantiation ||
                              (scope_stack[depth_scope_stack].kind ==
                                     (a_scope_kind)sck_template_instantiation);
  tag_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  cssp = tag_sym->variant.class_struct_union.extra_info;
  if (is_prototype_instantiation) {
    /* This is a prototype instantiation, so the resulting class is "nonreal"
       (i.e., based on template arguments that include the dummy types and
       constants of template parameters rather than real types and constants).
       Note that for nested classes the flag is set later. */
    is_nonreal_instantiation = cssp->is_nonreal_class = TRUE;
    cssp->is_prototype_instantiation = TRUE;
  }  /* if */
  /* A copy constructor need not be generated if construction by bitwise
     copy is equivalent.  When a class is being defined, set the flag to
     TRUE initially, and change it if a base class or member is declared
     that precludes construction by bitwise copy. */
  cssp->construction_by_bitwise_copy_allowed = TRUE;
  /* Similarly, assignment by bitwise copy is allowed unless there are
     virtual base classes, virtual functions, or base classes or fields
     for which bitwise copy is not allowed. */
  cssp->assignment_by_bitwise_copy_allowed = TRUE;
  if (curr_token == tok_colon && C_dialect == C_dialect_cplusplus) {
    /* Scan the list of base specifiers. */
    add_stop_token(tok_lbrace);
    scan_base_specifier_list(class_type);
    remove_stop_token(tok_lbrace);
    /* A class with base classes is not an "aggregate" (ARM 8.4.1). */
    class_aggregate_ruled_out = TRUE;
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
    /* If this is the definition of a nested class, set the parent class
       pointer in the tag symbol and set the access. */
    if (!is_template_instantiation &&
        scope_stack[decl_scope_level].kind ==
                                (a_scope_kind)sck_class_struct_union) {
      a_class_symbol_supplement_ptr tag_cssp;
      class_type->source_corresp.class_of_which_a_member =
            tag_sym->class_of_which_a_member =
                              scope_stack[decl_scope_level].assoc_type;
      class_type->source_corresp.access =
                              scope_stack[decl_scope_level].current_access;
      tag_cssp = symbol_supplement_for_class(tag_sym->class_of_which_a_member);
      /* A class nested within a nonreal class is itself nonreal and a
         class nested within a prototype instantiation is itself a prototype
         instantiation. */
      is_nonreal_instantiation = tag_cssp->is_nonreal_class;
      cssp->is_nonreal_class = is_nonreal_instantiation;
      cssp->is_prototype_instantiation = tag_cssp->is_prototype_instantiation;
    }  /* if */
    /* Advance past the left brace. */
    (void)get_token();
    add_stop_token(tok_rbrace);
    /* Start a scope for the fields and other members. */
    scope_ptr = push_scope((a_scope_kind)sck_class_struct_union,
                           NO_SCOPE_NUMBER, class_type, (a_routine_ptr)NULL,
                           (a_symbol_ptr)NULL, (a_symbol_ptr)NULL,
                           (a_template_arg_ptr)NULL);
    clear_layout_block(&layout_block, class_type);
    if (C_dialect == C_dialect_cplusplus) {
      /* In C++ every class, struct, and union type entry will have a non-NULL
         pointer to a class type supplement entry.  Put a pointer to the
         IL scope entry into it. */
      class_type->variant.class_struct_union.extra_info->assoc_scope =
                                                                 scope_ptr;
      /* Reserve space in the current class for its nonvirtual base classes,
         which are located at the start of the object.  (Virtual base classes
         appear at the end.) */
      set_offsets_for_nonvirtual_base_classes(&layout_block);
      saved_routine_fixup = curr_routine_fixup;
      curr_routine_fixup = NULL;
      /* Record the scope number used for the corresponding prototype
         instantiation, if any. */
      if (!is_nonreal_instantiation) {
        corresp_prototype_tag_sym = find_corresp_prototype_tag_sym(tag_sym);
      }  /* if */
    }  /* if */
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
        a_decl_flag_set   dso_flags;
        a_storage_class   member_storage_class;
        a_type_ptr        member_type;
        a_boolean         dangling_type_specifier;
        a_boolean         local_defines_something, local_declares_something;
        a_boolean         local_no_decl_specifiers;
        a_boolean         friend_specified, virtual_specified;
        a_boolean         type_explicitly_specified, inline_specified;
        a_boolean         is_destructor, is_constructor;
        a_boolean         is_anonymous_union;

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
              default:;  /* Avoid gcc warnings. */
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
        if (curr_token == tok_semicolon && 
            (C_dialect == C_dialect_cplusplus ||
             !(is_first_field && next_token() == tok_rbrace))) {
          /* No declaration -- just a semicolon.  Issue a warning (or error in
             strict ANSI mode).  Note: in C mode we bypass the "extra ':'"
             diagnostic when there are no fields in the struct -- i.e.,
             "struct S { ; };" is treated just like "struct S { };". */
          pos_diagnostic(strict_ansi_mode ?
                           strict_ansi_error_severity : es_warning,
                         ec_extra_semicolon, &pos_curr_token);
          /* Bypass the superfluous semicolon and continue looping. */
          (void)get_token();
          goto next_declaration;
        } else if (curr_token == tok_asm) {
          /* An asm declaration is not allowed in a class definition, but
             scan it anyway (after issuing the error). */
          (void)asm_declaration(/*asm_decl_allowed=*/FALSE);
        }  /* if */
        if (C_dialect == C_dialect_cplusplus) {
          /* Check for and discard declarations of the form "overload f;". */
          a_boolean		err;
          if (check_for_overload_anachronism()) goto next_declaration;
          if (is_qualified_name_start() &&
	      locator_for_curr_id.qualifier_class_type != class_type &&
	      !locator_for_curr_id.is_global_qualified_name &&
              locator_for_curr_id.is_qualified_name &&
              next_token() == tok_semicolon) {
            /* This looks syntactically like an access adjustment declaration.
               Be sure the semantics are correct. */
            (void)coalesce_and_lookup_qualified_name(GID_DTOR_RECOGNIZED,
                                                     ilm_normal, &err);
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
        if (local_defines_something && !is_error_type(member_type)) {
          a_type_ptr  tp = skip_typerefs(member_type);

#if CHECKING
          if (C_dialect == C_dialect_cplusplus) {
            /* Should be a nested class, struct, union, or enum definition.
               Be sure the parent class and access were marked correctly. */
            a_symbol_ptr sym = (a_symbol_ptr)(tp->source_corresp.assoc_info);
            if (sym != NULL &&
                sym->class_of_which_a_member != class_type) {
             internal_error(
                      "scan_class_definition: bad parent type on nested type");
            } else if (tp->source_corresp.access != access) {
              internal_error(
                      "scan_class_definition: bad access on nested type");
            } /* if */
          }  /* if */
#endif /* CHECKING */
          if (is_class_struct_union_type(tp)) {
            symbol_supplement_for_class(class_type)->any_nested_classes = TRUE;
          }  /* if */
          /* Mark the IL entry for the nested class or enum as referenced. */
#if 0
          /* This is premature, since it isn't really referenced at this
             point. */
#endif /* if 0 */
          tp->source_corresp.referenced = TRUE;
        } /* if */
        local_no_decl_specifiers = dso_flags & DSO_NO_DECL_SPECIFIERS;
        type_explicitly_specified =
                               dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER;
        friend_specified = dso_flags & DSO_FRIEND;
        if (friend_specified) any_friend_decls = TRUE;
        virtual_specified = (dso_flags & DSO_VIRTUAL) != 0;
        inline_specified = (dso_flags & DSO_INLINE) != 0;
        is_constructor = dso_flags & DSO_CONSTRUCTOR;
        is_destructor = dso_flags & DSO_DESTRUCTOR;
        remove_stop_token(tok_colon);
        if (dangling_type_specifier) {
          /* A malformed declaration was detected by decl_specifiers.  Issue
             errors indicating that an identifier (= a declarator) is missing,
             along with a semicolon.  Then branch to the bottom of the loop. */
          a_token_kind  next_tok;
          if (curr_token == tok_identifier &&
              ((next_tok = next_token()) == tok_semicolon ||
               next_tok == tok_comma || next_tok == tok_assign ||
               next_tok == tok_lbracket || next_tok == tok_lparen)) {
            /* Even though the current token is a type name, it looks more
               like a declarator with a following ";" or "," or "=". */
          } else {
            set_err_pos_to_curr_token();
            if (!local_declares_something) error(ec_exp_identifier);
            error(ec_exp_semicolon);
            goto next_declaration;
          }  /* if */
        }  /* if */
        if (curr_token == tok_semicolon && C_dialect == C_dialect_cplusplus) {
          /* There's no declarator following the declaration specifier.  This
             is okay sometimes.  When it is, skip over declarator processing
             to the next declaration. */
          if (local_defines_something && !local_declares_something &&
              member_type->kind == (a_type_kind)tk_union &&
              is_unnamed_class_symbol((a_symbol_ptr)member_type->
                                                source_corresp.assoc_info) &&
              !friend_specified &&
              member_storage_class != (a_storage_class)sc_typedef) {
            /* An anonymous union -- "union { int i, j; };" */
            is_anonymous_union = TRUE;
            /* Note that in this case we don't just skip on to the next
               declaration -- decl_nonstatic_data_member needs to be called. */
          } else {
            if (friend_specified) {
              if ((dso_flags & DSO_ELABORATED_TYPE_SPECIFIER) &&
                  !is_enum_type(member_type)) {
                /* This is a friend class declaration, of the form:
                           friend class A;
                   (which is the only form the ARM (see 11.4) allows. */
                if (is_nonreal_instantiation) {
                  /* The friend declaration is not processed during prototype
                     instantiation -- it's meaningless until a real
                     instantiation is done. */
                } else {
                  (void)decl_friend_class(class_type, member_type);
                }  /* if */
              } else if (!is_error_type(member_type)) {
                /* Invalid friend declaration. */
                pos_error(ec_bad_friend_decl, &decl_start_pos);
              }  /* if */
            } else if (local_declares_something) {
              /* This is a free standing declaration of a class, struct,
                 union, or enum type entry.  It will already have been
                 recorded on the types list for the current class.  No need
                 to complain about a missing identifier.  Just bypass the
                 semicolon, after checking for some errors. */
              if (member_storage_class != (a_storage_class)sc_unspecified) {
                if (member_storage_class == (a_storage_class)sc_typedef) {
                  /* A case like "typedef struct S { int i; };" */
                  pos_diagnostic(strict_ansi_mode ?
                                   strict_ansi_error_severity : es_warning,
                                 ec_missing_typedef_name, &pos_curr_token);
                } else {
                  pos_error(ec_storage_class_not_allowed, &decl_start_pos);
                }  /* if */
              }  /* if */
              if (inline_specified) {
                pos_error(ec_inline_not_allowed, &decl_start_pos);
              }  /* if */
              if (is_qualified_type(member_type)) {
                pos_error(ec_useless_type_qualifiers, &decl_start_pos);
              }  /* if */
            } else if (member_storage_class == (a_storage_class)sc_typedef) {
              /* A case like "typedef int;" or "typedef struct { int i; };" */
              pos_diagnostic(strict_ansi_mode ?
                               strict_ansi_error_severity : es_warning,
                             ec_missing_typedef_name, &pos_curr_token);
            } else if (local_defines_something) {
              /* A declaration with no declarator that defines a type but does
                 not declare a name -- something like "struct { int i; };" or
                 "enum {};".  */
              /* Does the ARM rule out such useless constructs?  The
                 introduction to Chapter 7 says, "A declaration introduces
                 one or more names into a program", and when declares_something
                 is FALSE no name was introduced.  On the other hand, 9.2 para
                 4 allows the omission of declarators with enum and class
                 specifiers.  However, we take this to include only enum and
                 class specifiers that at least declare *something*. */
              pos_diagnostic(strict_ansi_mode ?
                               strict_ansi_error_severity : es_warning,
                             ec_useless_decl, &decl_start_pos);
            } else {
              /* A case like "int;" is explicitly disallowed by language in
                 ARM 9.2. */
              pos_error(ec_useless_decl, &decl_start_pos);
            }  /* if */
            /* Bypass the semicolon and skip to the next declaration. */
            (void)get_token();
            goto next_declaration;
          }  /* if */
        }  /* if */
        /* A declarator list should be present.  Scan it. */
        first_declarator = TRUE;
        error_on_def_in_return_type_already_issued = FALSE;
        do {
          a_symbol_locator   locator;
          a_type_ptr         local_type;
          a_boolean          unnamed_field = FALSE;
          a_func_info_block  func_info;

          add_stop_token(tok_comma);
          add_stop_token(tok_colon);
          unnamed_field = FALSE;
          /* The declarator can be omitted for an unnamed bit-field. */
          set_err_pos_to_curr_token();
          if (curr_token == tok_colon) {
            /* Unnamed bit-field. */
            unnamed_field = TRUE;
            local_type = member_type;
            set_to_error_locator(locator);
          } else if (curr_token == tok_semicolon && first_declarator &&
                     C_dialect == C_dialect_pcc) {
            /* In pcc mode, the entire declarator list can be omitted to
               indicate an unnamed field.  It's a non-bit-field that forces
               padding. */
            unnamed_field = TRUE;
            local_type = member_type;
            set_to_error_locator(locator);
          } else if (is_anonymous_union) {
            /* There is no declarator. */
            local_type = member_type;
            set_to_error_locator(locator);
          } else {
            /* Named member -- we need to call declarator. */
            a_decl_flag_set    declarator_input_flags, declarator_output_flags;
            a_type_ptr         bottom_derived_type;

            if (C_dialect == C_dialect_cplusplus) {
              if (curr_routine_fixup != NULL) {
                /* We must be in a declarator list and this must be at least
                   the second item in the list. */
                /* This should not be cached function body. */
                check_assertion(curr_routine_fixup->
                                function_body_token_cache.first_token == NULL);
                if (curr_routine_fixup->def_arg_expr_fixup_list != NULL) {
                   /* The previous one must have been a routine declaration
                      with default arguments, so we have to save the routine
                      fixup entry onto the fixup list. */
                  add_to_routine_fixup_list(curr_routine_fixup);
                  /* Make a new one fixup entry for the current declarator. */
                  curr_routine_fixup = alloc_routine_fixup();
                } else {
                  /* The other one can be reused. */
                }  /* if */
              } else {
                /* Normal case.  Allocate a new routine fixup entry. */
                curr_routine_fixup = alloc_routine_fixup();
              }  /* if */
            }  /* if */
            /* Set the various flags for declarator processing. */
            declarator_input_flags = DI_REAL_DECLARATOR_ALLOWED;
            if (dso_flags & DSO_DESTRUCTOR) {
              if (!type_explicitly_specified && !friend_specified) {
                declarator_input_flags |= DI_DESTRUCTOR_SPECIFIERS;
              }  /* if */
            }  /* if */
            if (dso_flags & DSO_CONSTRUCTOR) {
              declarator_input_flags |= DI_IS_CONSTRUCTOR;
            }  /* if */
            if (member_storage_class == (a_storage_class)sc_typedef) {
              declarator_input_flags |= DI_IS_TYPEDEF_DECLARATION;
            } else if (member_storage_class != (a_storage_class)sc_static) {
              /* The storage class "static" was not specified and it is not
                 a typedef declaration.   Therefore, if this turns out to be
                 a member function declaration, it will be a nonstatic member
                 function.  This is important because when the routine type
                 is created, function_declarator needs to know whether to
                 add an implicit this-param pointer to the type. */
              declarator_input_flags |= DI_NONSTATIC_MEMBER;
            }  /* if */
            if (friend_specified) {
              declarator_input_flags |= DI_IS_FRIEND_DECL |
                                        DI_QUALIFIED_NAME_ALLOWED;
            }  /* if */
            declarator_input_flags |= DI_OPERATOR_NAME_ALLOWED;
            /* Pass the class's type pointer to declarator if this might
               be a nonstatic member function, in which case its presence
               will cause an implicit "this" parameter type to be created.
               (Static member functions do not have an implicit "this" pointer.
               The class pointer will be ignored for data members.) */
            declarator(declarator_input_flags, &declarator_output_flags,
                       member_type,
                       friend_specified ? (a_type_ptr)NULL : class_type,
                       &locator, &local_type, &bottom_derived_type,
                       &func_info);
            if (C_dialect == C_dialect_cplusplus) {
              /* Abstract class objects are prohibited (ARM 10.3). */
              if (member_storage_class != (a_storage_class)sc_typedef &&
                  is_illegal_abstract_class_type(local_type)) {
                pos_error(ec_abstract_class_object_not_allowed,
                          &locator.source_position);
              }  /* if */
              if (local_defines_something &&
                  !error_on_def_in_return_type_already_issued) {
                /* The ARM (8.2.5) explicitly prohibits defining a type in
                   a function return type.  This is taken to apply to
                   pointer-to-function type declarations as well to the
                   function declarations. */
                a_type_ptr  tp = local_type;
                for (;;) {
                  if (is_function_type(tp)) {
                    /* Function type in which the return type involves a
                       definition. */
                    pos_error(ec_type_def_not_allowed_in_func_type_decl,
                              &decl_start_pos);
                    error_on_def_in_return_type_already_issued = TRUE;
                    break;
                  } else if (is_ptr_or_ref_type(tp)) {
                    /* Get type pointed to and continue. */
                    tp = type_pointed_to(tp);
                  } else if (is_ptr_to_member_type(tp)) {
                    /* Get member type and continue. */
                    tp = pm_member_type(tp);
                  } else {
                    /* No function type can be involved.  Stop looping. */
                    break;
                  }  /* if */
                }  /* for */
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
              /* Member or friend function. */
              a_boolean      suppress_pure_specifier_error = FALSE;
              a_boolean      function_def_present;
              a_special_function_kind
                             spec_kind = (a_special_function_kind)sfk_none;

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
                  member_storage_class = (a_storage_class)sc_unspecified;
                }  /* if */
              } else {
                if ((is_constructor || is_destructor) &&
                    member_storage_class == (a_storage_class)sc_static) {
                  /* Constructors and destructors may not be declared
                     "static" (ARM 12.1, 12.4). */
                  pos_error(ec_static_not_allowed, &decl_start_pos);
                  member_storage_class = (a_storage_class)sc_unspecified;
                }  /* if */
                if (virtual_specified) {
                  if (is_constructor || is_union_type(class_type)) {
                    /* Constructors may not be virtual functions (ARM 12.1)
                       and unions may not have them (ARM 9.5). */
                    pos_error(ec_virtual_not_allowed, &decl_start_pos);
                    virtual_specified = FALSE;
                    suppress_pure_specifier_error = TRUE;
                  } else if (member_storage_class ==
                                           (a_storage_class)sc_static ||
                             (locator.is_operator_name &&
                              (locator.variant.opname ==
                                               (an_opname_kind)onk_new ||
                               locator.variant.opname ==
                                               (an_opname_kind)onk_delete))) {
                    /* Only nonstatic member functions may be specified as
                       virtual.  This applies to operators new and delete
                       since they are always static (ARM 12.5). */
                    pos_error(ec_virtual_static_not_allowed, &decl_start_pos);
                    virtual_specified = FALSE;
                    suppress_pure_specifier_error = TRUE;
                  }  /* if */
                }  /* if */
              }  /* if */
              if (!type_explicitly_specified) {
                /* No type specifier. */
                if (is_constructor || is_destructor ||
                    locator.is_conversion_name) {
                  /* Type specifier is not expected (nor permitted) on
                     constructors, destructors, and conversion functions. */
                } else {
                  /* Type specifier is missing.  The type defaults to int,
                     but issue a diagnostic. */
                  if (first_declarator) {
                    pos_remark(ec_missing_type_specifier, &decl_start_pos);
                  }  /* if */
                }  /* if */
              }  /* if */
              spec_kind = (a_special_function_kind)sfk_none;
              function_def_present = (curr_token == tok_lbrace);
              if (local_type == member_type) {
                /* When scanning the declarator does not change the type,
                   we know this member is a function based on the specifier
                   type alone.  This is only possible with a typedef name that
                   represents a function type.  Such typedef types do not
                   (usually) have implicit this-param types.  Moreover, since
                   they are shared, they are unsuited to be the type of
                   a defined function. */
                a_type_ptr  rout_type = skip_typerefs(local_type);
                a_boolean   copy_needed = TRUE;

                func_info.function_type_from_typedef = TRUE;
                if (cfront_compatibility_mode &&
                    rout_type->variant.routine.extra_info->
                                            implicit_this_param_type != NULL) {
                  /* We have a situation in which a typedef has been declared
                     like this:
                            typedef void A::t(int);  // Nonstandard
                     meaning "t" names a routine type taking an int argument
                     and returning void and having an implicit this-param type
                     of const-ptr-to-A.  (This "member function typedef" is
                     not part of the language of the ARM  and is allowed for
                     cfront compatibility only.)  The only supported use is
                     to declare a pointer-to-member type, e.g.,
                            t *pm;                   // Okay
                     Whereas it is apparently being used here to declare a
                     function, e.g.,
                            t f;                     // Error
                     Issue the error. */
                  pos_sy_error(ec_bad_use_of_ptr_to_member_typedef,
                               &decl_start_pos,
                               (a_symbol_ptr)local_type->
                                        source_corresp.assoc_info);
                } else if (function_def_present) {
                  /* Not legal to define a function with a typedef type. */
                  pos_error(ec_function_type_must_come_from_declarator,
                            &locator.source_position);
                } else if (friend_specified ||
                           member_storage_class ==
                                           (a_storage_class)sc_static) {
                  /* No copy is needed. */
                  copy_needed = FALSE;
                }  /* if */
                if (copy_needed) {
                  /* Build a copy of the routine type so as to have a
                     non-shared routine type entry. */
                  local_type = alloc_type((a_type_kind)tk_routine);
                  copy_routine_type_with_param_types(rout_type, local_type);
                  if (!friend_specified &&
                      member_storage_class != (a_storage_class)sc_static) {
                    /* This is a nonstatic member function declared through
                       a typedef.  Be sure the implicit this-param type is
                       filled in, since that's the only way a nonstatic
                       member function is distinguished from a static member
                       function. */
                    a_type_ptr tp;

                    tp = make_pointer_type(class_type);
                    tp = make_qualified_type(tp, /*is_const=*/TRUE,
                                             /*is_volatile=*/FALSE);
                    local_type->variant.routine.extra_info->
                                      implicit_this_param_type = tp;
                  } else if (cfront_compatibility_mode) {
                    /* Just in case this is a copy of the weird
                       cfront-compatibility typedef, clear out the implicit
                       this-param pointer in the copied type entry. */
                    local_type->variant.routine.extra_info->
                                            implicit_this_param_type = NULL;
                  }  /* if */
                }  /* if */
              }  /* if */
              func_info.is_definition = function_def_present;
              func_info.is_inline = inline_specified || function_def_present;
              if (friend_specified) {
                rout_sym = decl_friend_function(&locator, class_type,
                                                local_type, &func_info);
              } else {
                if (is_destructor) {
                  spec_kind = (a_special_function_kind)sfk_destructor;
                } else if (is_constructor) {
                  spec_kind = (a_special_function_kind)sfk_constructor;
                  if (curr_token == tok_colon) {
                    func_info.is_definition = function_def_present = TRUE;
                    func_info.is_inline = TRUE;
                  }  /* if */
                }  /* if */
                /* Create a symbol for the member function. */
                rout_sym = decl_member_function(
                                   &locator, class_type, local_type,
                                   &func_info, access, virtual_specified,
                                   /*compiler_generated=*/FALSE, spec_kind);
                if (corresp_prototype_tag_sym != NULL) {
                  /* The class must be the instantiation of a class template
                     (or a class nested within such an instantiation). Bind
                     the current member function symbol to the function
                     template symbol established during prototype
                     instantiation. */
                  if (!is_error_locator(locator)) {
                    find_member_function_template(rout_sym,
                                                  corresp_prototype_tag_sym);
                  }  /* if */
                }  /* if */
              }  /* if */
              if (!function_def_present) {
                if (func_info.param_id_list != NULL) {
                  /* Free the list of parameter identifiers -- they're not
                     needed if there's no definition. */
                  free_param_id_list(&(func_info.param_id_list));
                }  /* if */
              }  /* if */
              if (curr_routine_fixup != NULL) {
                curr_routine_fixup->routine = rout_sym->variant.routine.ptr;
                curr_routine_fixup->func_info = func_info;
              }  /* if */
              if (function_def_present) {
                if (!friend_specified) {
                  /* The inline flag is set for friend functions in
                     decl_friend_function, which also handles cases in which
                     it should be left unset despite the presence of a
                     function body. */
                  check_assertion(rout_sym->variant.routine.ptr->is_inline);
                }  /* if */
                remove_stop_token(tok_comma);
                /* Cache the tokens comprising the function definition
                   so that they can be rescanned once the entire class
                   definition has been processed. */
                if (prescan_function_definition()) {
                  /* Advance past the terminating right brace. */
                  (void)get_token();
                }  /* if */
                if (curr_token == tok_semicolon) {
                  /* Advance past the optional semicolon. */
                  (void)get_token();
                }  /* if */
                if (!friend_specified && is_nonreal_instantiation) {
                  /* A member function of a nonreal class serves as a
                     template, and since this is the definition the
                     template-info associated with this member function must
                     be updated, based on the template-info of the prototype
                     instantiation.  Note that the current class may be
                     nested within the prototype instantiation. */
                  a_template_symbol_supplement_ptr  tssp, class_tssp;

                  tssp = rout_sym->variant.routine.instance_ptr->template_info;
                  class_tssp =
                       scope_stack[depth_innermost_instantiation_scope].
                                           template_sym->variant.template_info;
                  tssp->parameters = class_tssp->parameters;
                  tssp->declaration_scope = class_tssp->declaration_scope;
                }  /* if */
                /* A comma-list of function definitions is not allowed. */
                goto next_declaration;
              } else {
                /* Not a function definition. */
                if (curr_token == tok_assign) {
                  /* Look for a pure specifier ("= 0"), which may appear on
                     virtual functions. */
                  scan_pure_specifier(rout_sym, class_type,
                                      suppress_pure_specifier_error);
                } else if (!friend_specified && is_local_class) {
                  /* A member function declared in a local class definition
                     (which is the current case) must be defined within the
                     class definition (ARM 9.8). */
                  error(ec_local_class_function_def_missing);
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
            pos_error(ec_virtual_not_allowed, &decl_start_pos);
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

            check_assertion(C_dialect == C_dialect_cplusplus);
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
                     is_scalar_type(local_type) &&
                     is_const_qualified_type(local_type) &&
                     !is_volatile_qualified_type(local_type) &&
                     member_storage_class == (a_storage_class)sc_unspecified &&
                     C_dialect == C_dialect_cplusplus) {
            /* Provide support for the nonstandard declaration of a member
               constant of integral type -- e.g., "const int I = 2;". */
            decl_member_constant(&locator, class_type, local_type, access);
          } else {
            if (C_dialect == C_dialect_cplusplus) {
              if (!type_explicitly_specified && first_declarator) {
                warning(ec_missing_type_specifier);
              }  /* if */
            }  /* if */
            if (member_storage_class == (a_storage_class)sc_static) {
              /* Static data member. */
              if (is_void_type(local_type)) {
                error(ec_incomplete_type_not_allowed);
                local_type = error_type();
              }  /* if */
              if (is_anonymous_union) {
                /* This static data member is an anonymous union.  Because
                   of the impossibility of an explicitly defining such things
                   (and despite the ARM's silence on the issue), they are
                   disallowed.  Our approach does not involve promoting the
                   names to the current scope, so some error recovery problems
                   are bound to show up. */
                pos_error(ec_static_data_member_anon_union, &decl_start_pos);
              } else if (is_union_type(class_type)) {
                /* Unions are not allowed to have static data members. */
                pos_error(ec_static_not_allowed, &decl_start_pos);
              } else if (is_local_class) {
                /* Static data members are not allowed in local classes. */
                pos_error(ec_static_not_allowed, &decl_start_pos);
                /* Set the type for this invalid static member to error_type.
                   This will assure "proper" (or unobtrusive) behavior later,
                   if a definition is encountered.  It also eliminates semi-
                   spurious error messages if there are references to it. */
                local_type = error_type();
              }  /* if */
              decl_static_data_member(&locator, class_type, local_type,
                                      access, is_anonymous_union,
                                      is_nonreal_instantiation,
                                      corresp_prototype_tag_sym);
            } else {
              /* Non-static data member (= field). */
              /* The type specified must be complete. */
              check_for_uninstantiated_template_class(local_type);
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
                    diagnostic(strict_ansi_error_severity,
                               ec_incomplete_type_not_allowed);
                  }  /* if */
                } else if (is_template_param_type(local_type)) {
                  check_assertion(local_type->variant.template_param.kind ==
                                      (a_template_param_type_kind)tptk_member);
                  /* Okay. */
#if 0
/* The following code is removed on the assumption (based on Stroustrup et al.,
   document X3J16/92-133) that any "free-symbol" referenced in a prototype
   instantiation must be completely defined at that point. */
                } else if (is_nonreal_instantiation) {
                  /* Issue no error on incomplete types if the class currently
                     being defined is a prototype instantiation or a class
                     nested within a prototype instantiation. */
                  /* Is it better to set the type to error-type or just to
                     skip the call to decl_nonstatic_data_member?  Or is there
                     another approach? */
                  local_type = error_type();
#endif /* if 0 */
                } else {
                  error(ec_incomplete_type_not_allowed);
                  local_type = error_type();
                }  /* if */
              }  /* if */
              if (curr_token == tok_colon) {
                /* Bit-field declaration -- be sure the type is okay.  Do it
                   here rather than in the subroutine because here we have
                   the right error position. */
                a_type_ptr  bit_field_type = skip_typerefs(local_type);
                if (!is_integral_type(bit_field_type)) {
                  /* Error, not an integral type. */
                  if (is_error_type(bit_field_type)) {
                    /* An error has already been issued. */
                  } else if (is_template_param_type(bit_field_type)) {
                    /* We're in a prototype instantiation -- don't issue an
                       error. */
                  } else {
                    /* Invalid type. */
                    pos_error(ec_bad_bit_field_type, &decl_start_pos);
                  }  /* if */
                } else {
                  /* Integral base type.  In strict ANSI C mode, give a
                     diagnostic about a nonstandard base type (anything other
                     than int, unsigned int, and signed int). */
                  if (C_dialect != C_dialect_cplusplus && strict_ansi_mode) {
                    if (bit_field_type->variant.integer.enum_type ||
                        (bit_field_type->variant.integer.int_kind !=
                                           (an_integer_kind)ik_int &&
                         bit_field_type->variant.integer.int_kind !=
                                           (an_integer_kind)ik_unsigned_int)) {
                      pos_diagnostic(strict_ansi_error_severity,
                                     ec_nonstd_bit_field_type,
                                     &decl_start_pos);
                    }  /* if */
                  }  /* if */
                }  /* if */
              }  /* if */
              decl_nonstatic_data_member(&locator, &layout_block, &local_type,
                                         access, unnamed_field,
                                         is_anonymous_union,
                                         &end_of_field_list);
              if (!class_aggregate_ruled_out) {
                /* The ARM says that classes with private or protected members
                   are not treated as "aggregates" (8.4.1).  We interpret this
                   to refer to nonstatic data members only (given the context).
                   Furthermore, we suppose that a class with a field whose type
                   is a nonaggregate class (or an array thereof) cannot be
                   treated as an aggregate either; this seems in accord with
                   the intent of 8.4.1 if not the letter. */
                if (access != (an_access_specifier)as_public) {
                  if (unnamed_field) {
                    /* Unnamed bit fields are not subject to initialization
                       (and are not even members, according to WP 9.6) so a
                       nonpublic one (whatever that means) has no effect on
                       aggregate status. */
                  } else {
                    class_aggregate_ruled_out = TRUE;
                  }  /* if */
                } else {
                  a_type_ptr  tp = local_type;
                  if (is_array_type(tp)) {
                    tp = underlying_array_element_type(tp);
                  }  /* if */
                  if (is_class_struct_union_type(tp) &&
                     !symbol_supplement_for_class(tp)->is_class_aggregate) {
                    class_aggregate_ruled_out = TRUE;
                  }  /* if */
                }  /* if */
              }  /* if */
              if (!any_const_or_ref_fields &&
                  !unnamed_field && !is_anonymous_union &&
                  (is_reference_type(local_type) ||
                   type_or_element_type_is_const_qualified(local_type))) {
                any_const_or_ref_fields = TRUE;
              }  /* if */
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
          if (C_dialect != C_dialect_pcc) {
            diagnostic(strict_ansi_mode ?
                         strict_ansi_error_severity : es_warning,
                       ec_exp_semicolon);
          }  /* if */ 
        } else {
          (void)required_token(tok_semicolon, ec_exp_semicolon);
        }  /* if */
next_declaration:
        if (curr_routine_fixup != NULL) {
          /* If the currently active routine fixup entry has been modified
             such that a fixup pass over its tokens is required, add it to
             the routine fixup list for the current class.  Otherwise free
             it for later use. */
          if (curr_routine_fixup->
                        function_body_token_cache.first_token != NULL  ||
              curr_routine_fixup->def_arg_expr_fixup_list != NULL) {
            add_to_routine_fixup_list(curr_routine_fixup);
          } else {
            free_routine_fixup(curr_routine_fixup);
          }  /* if */
          curr_routine_fixup = NULL;
        }  /* if */
        remove_stop_token(tok_semicolon);
        /* Keep processing member declarations until the closing brace. */
      } while (curr_token != tok_rbrace && curr_token != tok_end_of_source);
    }  /* if */
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
    } else if (is_prototype_instantiation) {
      /* The type entries created for a class template are not added to the
         types list. */
    } else if (scope_stack[effective_decl_level].kind ==
                                   (a_scope_kind)sck_template_declaration) {
      /* This is an error case -- a class definition within a template
         parameter declaration.  Don't try to enter the class in the IL. */
    } else {
      /* Add the class type to the list for the current scope.  Note that
         incomplete structs/unions are not added to the type list (this code
         is bypassed) because the actual definition has not yet appeared.  See
         pop_scope; they get added at the end of the scope. */
      add_to_types_list(class_type, effective_decl_level);
    }  /* if */
    /* Save a pointer to the list of member symbols in the tag symbol.  Note
       that there may be symbols even if there there were no declarations,
       since symbols may be inherited. */
    cssp->symbols = scope_stack[depth_scope_stack].symbols;
    /* A number of the checks done as a part the "wrapup" phase of scanning a
       class definition produce diagnostics.  Set error_position to assure
       that these diagnostics will be associated with tag_sym instead of
       with the current token, which is the closing brace. */
    error_position = tag_sym->decl_position;
    if (C_dialect == C_dialect_cplusplus) {
      /* Issue a diagnostic on a class with no user-defined constructor and
         with one or more members with reference or const type.  Note that
         this check is done before compiler-generated constructors, if any,
         are entered. */
      if (any_const_or_ref_fields && cssp->constructor == NULL) {
        a_symbol_ptr  sym;

        if (class_aggregate_ruled_out) {
          /* Issue an error for a non-aggregate class, since there's no other
             way to initialize an object of the class. */
          pos_sy_start_error(ec_no_ctor_but_const_or_ref_member,
                             &error_position, tag_sym);
        } else {
          /* Issue a warning for an aggregate class.  If an attempt is made
             to declare an object without appropriate initialization, an error
             will be issued.  For example:
               class A { const int i; };     // Just a warning
               A x = { 0 };                  // Okay -- ARM 8.4.1
               A y = x;                      // Probably okay -- ARM 8.4.1
               A z;                          // Error will be issued       */
          pos_sy_start_warning(ec_no_ctor_but_const_or_ref_member,
                               &error_position, tag_sym);
        }  /* if */
        /* List each of the uninitialized const or ref member. */
        for (sym = cssp->symbols; sym != NULL; sym = sym->next_in_scope) {
          if (sym->kind == (a_symbol_kind)sk_field) {
            a_type_ptr  tp = sym->variant.field.ptr->type;
            if (is_reference_type(tp)) {
              sym_add_diag_info(ec_reference_member, sym);
            } else if (type_or_element_type_is_const_qualified(tp)) {
              sym_add_diag_info(ec_const_member, sym);
            }  /* if */
          }  /* if */
        }  /* for */
        end_error();
      }  /* if */
      if (!is_nonreal_instantiation) {
        /* Create compiler-generated default constructor, copy constructor,
           destructor, and assignment operator, if any is needed. */
        check_special_member_functions(class_type);
        /* Classes with no constructors, no private or protected members, no
           base classes, and no virtual functions are used to declare
           "aggregate" objects (ARM 8.4.1). */
        if (!class_aggregate_ruled_out && cssp->constructor == NULL) {
          cssp->is_class_aggregate = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    /* Wrap up field allocation. */
    finish_laying_out_class(&layout_block);
    if (C_dialect == C_dialect_cplusplus) {
      if (!is_nonreal_instantiation) {
        /* Check for inherited conversion functions.  This must be done before
           rescanning inline function definitions. */
        project_base_class_conversion_functions(class_type);
      }  /* if */
      /* Report errors in virtual function declarations that result from
         the failure to redeclare a virtual function originally declared in
         a virtual base class. */
      copy_source_position(pos_curr_token, error_position);
      report_virtual_function_ambiguities(class_type);
      /* If the current class is not already marked as "abstract", run
         through its base classes to determine whether it is abstract by
         inheritance and set the flag accordingly. */
      check_abstract_class(class_type);
      /* Issue a warning on a class with an operator new() but no operator
         delete() or vice versa. */
      if (cssp->has_operator_new != cssp->has_operator_delete) {
        sym_remark(cssp->has_operator_new ?
                     ec_class_with_op_new_but_no_op_delete :
                     ec_class_with_op_delete_but_no_op_new,
                   tag_sym);
      }  /* if */
      /* Issue a warning on a class with all private constructors and no
         friend functions. */
      if (!any_friend_decls) {
        a_symbol_ptr  ctor_sym = cssp->constructor;
        a_boolean     is_overloaded = FALSE;

        if (ctor_sym != NULL) {
          if (ctor_sym->kind == (a_symbol_kind)sk_overloaded_function) {
            is_overloaded = TRUE;
            ctor_sym = ctor_sym->variant.overloaded_function.symbols;
          }  /* if */
          /* See if the class has at least one constructor with nonprivate
             access control. */
          for (; ctor_sym != NULL;
               ctor_sym = is_overloaded ? ctor_sym->next : NULL) {
            if (ctor_sym->variant.routine.ptr->source_corresp.access !=
                                            (an_access_specifier)as_private) {
              /* Break out of the loop with non-null ctor_sym. */
              break;
            }  /* if */
          }  /* for */
          if (ctor_sym == NULL) {
            /* All constructors are private. */
            pos_sy_warning(ec_no_access_to_constructors,
                           &tag_sym->decl_position, tag_sym);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    /* Pop the pseudo-scope created for the fields. */
    pop_scope();
    remove_stop_token(tok_rbrace);
    /* Check for and ignore the closing brace. */
    (void)required_token(tok_rbrace, ec_exp_rbrace);
    if (C_dialect == C_dialect_cplusplus) {
      /* Rescan tokens that were cached (inline function definitions, default
         arguments). */
      if (tag_sym->class_of_which_a_member == NULL) {
        /* For non-nested classes do delayed processing for default argument
           declarations and inline member function definitions. */
        delayed_scan_fixup_for_class(tag_sym, is_template_instantiation);
      }  /* if */
      curr_routine_fixup = saved_routine_fixup;
    }  /* if */
    /* Switch back from the file scope memory region to whatever region
       was current upon entry. */
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */

  db_exit();
  return !err;
}  /* scan_class_definition */


a_boolean class_specifier(a_boolean  vacuous_decl_allowed,
                          a_boolean  is_friend_decl,
                          a_boolean  is_ref_within_new_expr,
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
  a_symbol_kind           tag_kind;
  a_type_kind             type_kind;
  a_symbol_locator        locator;
  a_symbol_ptr            tag_sym, error_tag_sym = NULL;
  a_boolean               tag_id_present;
  a_type_ptr              class_type;
  a_boolean               is_local_class = FALSE;
  a_boolean               is_template_class_instantiation = FALSE;
  a_boolean               tag_resolution = FALSE;
  a_boolean               err = FALSE;
  a_scope_depth           effective_decl_level = decl_scope_level;
  a_boolean               is_class_definition;
  a_source_position       decl_start_pos;
  a_scope_stack_entry_ptr ssep;
  a_source_position       tag_position;

  db_enter(3, "class_specifier");
  *declares_something = FALSE;
  *defines_something = FALSE;
  decl_start_pos = pos_curr_token;
  /* Determine whether this is a template class instantiation or a local
     class (one being declared within a function scope). */
  ssep = &scope_stack[depth_scope_stack];
  if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
    is_template_class_instantiation = TRUE;
    class_type = scope_stack[depth_scope_stack].assoc_type;
    tag_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
    if (curr_token == tok_struct) {
      class_type->kind = (a_type_kind)tk_struct;
    }  /* if */
    type_kind = class_type->kind;
    (void)get_token();
    (void)get_token();
    goto skip_tag_scan;
  } else if (depth_innermost_function_scope != NO_SCOPE_NUMBER ||
             inside_local_class) {
    /* This declaration appears within a function or block scope, or else it
       is a nested class declaration within a local class.  In either case,
       it is a local class. */
    is_local_class = TRUE;
  }  /* if */
  if (!is_qualified_name_start()) {
    /* Skip over "class", "struct", or "union", remembering which appears. */
    check_assertion(curr_token == tok_class || curr_token == tok_struct ||
                    curr_token == tok_union);
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
    /* class_specifier is called with is_friend_decl TRUE only when the name
       has not yet been declared; this happens in cfront compatibility mode
       only.  Default kind is "class" when a class is introduced by a friend
       declaration.   (In fact, there is a slight incompatibility here, since
       in cfront 2.1 this can also be turned into a union declaration.) */
    check_assertion(is_friend_decl);
    tag_id_present = TRUE;
    tag_kind = (a_symbol_kind)sk_class_or_struct_tag;
    type_kind = (a_type_kind)tk_class;
  }  /* if */
  if (tag_id_present) {
    /* It seems that appearance of a tag name is a declaration of the
       tag, even if it just repeats a previous name.  At least, there's
       a Plum Hall test that implies that. */
    check_assertion(!vacuous_decl_allowed || !is_friend_decl);
    tag_position = pos_curr_token;
    *declares_something = TRUE;
    tag_sym = scan_tag_name(tag_kind, &locator, vacuous_decl_allowed,
                            is_ref_within_new_expr, &effective_decl_level,
                            &tag_resolution);
    if (tag_sym != NULL) {
      /* Check for tag mismatch.  This can only happen when an instance of a
         class template is being referenced in an elaborated type specifier. */
      if (tag_sym->kind == (a_symbol_kind)sk_type) {
        check_assertion(tag_sym->variant.type->kind ==
                                             (a_type_kind)tk_template_param);
        /* Template param used in with a class-key -- for instance:
             template <class T> class A {
               class T x;
             };
           During prototype instantiation we have to assume that T can be a
           valid class name.  Therefore "class T x" is treated as synonymous
           with "T x".  In addition, "friend class T" is also supported. */
      } else if (tag_sym->kind != tag_kind) {
        check_assertion(is_template_class_symbol(tag_sym));
        /* Error -- tag-kind mismatch in a specialization. */
        pos_sy_error(ec_union_nonunion_mismatch, &decl_start_pos,
                     tag_sym->variant.class_struct_union.extra_info->
                                                            class_template);
        set_to_named_error_locator(locator);
        error_tag_sym = tag_sym;
        tag_sym = NULL;
      }  /* if */
    }  /* if */    
    if (is_error_locator(locator)) err = TRUE;
  } else {
    /* No tag identifier present. */
    tag_sym = NULL;
    set_to_error_locator(locator);
    if (is_ref_within_new_expr) {
      /* We are within a new expression and no class name is given following
         the keyword -- e.g., "class A *pa = new class;" -- report the missing
         identifier as a syntax error. */
      syntax_error(ec_exp_identifier);
      err = TRUE;
    } else if (curr_token == tok_lbrace ||
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
skip_tag_scan:
  /* If the next token is a "{" or, in C++, a ":" (introducing a list of
     base classes) we should expect to scan a class definition.  The exception
     to this is when an elaborated class name (e.g., "struct S" instead of
     simply "S") appears within the context of a new expression.  The
     issue is the colon: since a colon could be part of the expression
     context (e.g., "struct S *ps = flag ? new struct S : 0;") it should
     not be interpreted as introducing a base classes list. */
  is_class_definition = curr_token == tok_lbrace ||
                        (C_dialect == C_dialect_cplusplus &&
                         curr_token == tok_colon && !is_ref_within_new_expr);
  if (tag_sym != NULL && C_dialect == C_dialect_cplusplus) {
    if (tag_sym->kind == (a_symbol_kind)sk_type) {
      if (is_class_definition) {
        /* Attempting to redefine a template parameter name.  Let enter_symbol
           issue an error. */
        tag_sym = NULL;
      }  /* if */
    } else {
      a_class_symbol_supplement_ptr  cssp;

      cssp = tag_sym->variant.class_struct_union.extra_info;
      if (cssp->class_template != NULL) {
        if (is_class_definition && tag_sym->defined) {
          /* This template class has already been instantiated. */
          pos_sy_error(ec_already_defined, &tag_position, tag_sym);
          error_tag_sym = tag_sym;
          tag_sym = NULL;
          set_to_named_error_locator(locator);
          err = TRUE;
        } else if (is_class_definition ||
                   (curr_token == tok_semicolon && !is_friend_decl)) {
          /* We have a specific declaration of a template class. */
          if (decl_scope_level != DEPTH_OF_FILE_SCOPE) {
            /* Specific definitions of template classes may only occur at
               file scope. */
            pos_error(ec_specific_def_must_be_global, &tag_position);
            error_tag_sym = tag_sym;
            tag_sym = NULL;
            set_to_named_error_locator(locator);
            err = TRUE;
          } else {
            cssp->is_specific_template_def = TRUE;
          }  /* if */
        }  /* if */
      } else if (is_class_definition) {
        if (tag_sym->class_of_which_a_member != NULL &&
            (ssep->kind != (a_scope_kind)sck_class_struct_union ||
             tag_sym->class_of_which_a_member != ssep->assoc_type)) {
          /* A definition of a nested class that appears in the scope other
             than that of its parent class. */
          pos_sy_error(ec_bad_scope_for_definition, &tag_position, tag_sym);
          tag_sym = NULL;
          set_to_error_locator(locator);
#if 0
        } else if (tag_sym->defined) {
          pos_sy_error(ec_already_defined, &locator.source_position, tag_sym);
          tag_sym = NULL;
          set_to_error_locator(locator);
#endif /* if 0 */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (tag_sym == NULL) {
    /* Create a new class, struct, or union type.  All such types are
       allocated in the file scope memory region, though local types will be
       added to the function scope's types list. */
    class_type = alloc_type(type_kind);
    if (C_dialect == C_dialect_cplusplus && error_tag_sym != NULL) {
      class_type->variant.class_struct_union.extra_info->template_arg_list =
             error_tag_sym->variant.class_struct_union.type->
                      variant.class_struct_union.extra_info->template_arg_list;
    }  /* if */
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
    if (is_class_definition) {
      mark_defined(tag_sym, &locator.source_position);
    } else {
      mark_declared(tag_sym, &locator.source_position);
    }  /* if */
  } else if (tag_sym->kind == (a_symbol_kind)sk_type) {
    /* Use of template parameter name as a proxy tag name during a
       prototype instantiation. */
  } else {
    /* Using an existing type.  Fetch the type pointer from it. */
    class_type = tag_sym->variant.class_struct_union.type;
    /* Record cross-reference information. */
    if (is_class_definition) {
      if (is_template_class_instantiation) {
        mark_declared(tag_sym, &locator.source_position);
      } else {
        mark_defined(tag_sym, &locator.source_position);
      }  /* if */
      /* Allow for alternating between class and struct, but stay with the
         one associated with the definition.  The difference only affects
         default member access. */
      class_type->kind = type_kind;
    } else {
      mark_referenced(tag_sym, &locator.source_position);
    }  /* if */
  }  /* if */
  if (is_class_definition) {
    if (scan_class_definition(class_type, effective_decl_level,
                              is_local_class,
                              /*is_prototype_instantiation=*/FALSE)) {
      *defines_something = TRUE;
      /* If this is the resolution of a previously incomplete tag, and there
         is a list of array types to be resolved, look to see if any of them
         are arrays whose element type is this struct/union type.  (This
         handles an infrequently-used extension.) */
      if (tag_resolution) check_fixup_list_for_array_types();
    } else {
      err = TRUE;
    }  /* if */
  }  /* if */
  if (err) {
    *type_ptr = error_type();
  } else if (tag_sym->kind == (a_symbol_kind)sk_type) {
    *type_ptr = tag_sym->variant.type;
  } else {
    *type_ptr = class_type;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(tag_sym, "tag_sym: ", 4);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return !err;
}  /* class_specifier */

/* Forward declaration for recursive call. */
static void check_type_for_linkage_change(a_type_ptr type,
                                          int        *count);


static void make_enum_type_externally_linked(a_type_ptr  type,
                                             int         *count)
/*
This routine changes the linkage of type, an enum type, from internal to
external.
*/
{
  check_assertion(is_immediate_enum_type(type));
  /* Mark the enum type externally linked. */
  type->source_corresp.name_linkage =
                                 (a_name_linkage_kind)nlk_cplusplus_external;
  if (type->source_corresp.class_of_which_a_member == NULL) {
    /* Increment the count.  This lets the caller know how many types (enum
       types and classes) were changed from internal to external linkage and
       permits an early termination of this processing.  Note that the count
       does not include nested types. */
    (*count)++;
  }  /* if */
}  /* make_enum_type_externally_linked */


static void make_class_externally_linked(a_type_ptr type,
                                         int        *count)
/*
This routine changes the linkage of a type and its components from
internal to external.  The types it handles directly are class, struct,
and union types, for which it sets the name_linkage field, adjusts members
as needed, and searches for other classes that are entailed in its
definition and marks them external as well.
*/
{
  a_field_ptr                  fp;
  a_class_type_supplement_ptr  ctsp;
  a_base_class_ptr             bcp;
  a_routine_ptr                rp;
  a_variable_ptr               vp;
  a_type_ptr                   tp;
  a_symbol_ptr                 sym;
  a_template_arg_ptr           tap;

  db_enter(4, "make_class_externally_linked");
  /* Mark the class as externally linked immediately, to avoid infinite
     recursion if it is self referential. */
  type->source_corresp.name_linkage =
                                 (a_name_linkage_kind)nlk_cplusplus_external;
  if (type->source_corresp.class_of_which_a_member == NULL) {
    /* Increment the count.  This lets the caller know how many classes
       were changed from internal to external linkage and permits an early
       termination of this processing.  Note that a count is not made of
       nested classes, since the optimization relies on classes at file scope
       only. */
    (*count)++;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    fputs("external linkage given to class \"", f_debug);
    db_type_name(type);
    fputs("\"\n", f_debug);
  }  /* if */
#endif /* DEBUG */
  /* Be sure any class types involved in the definitions of subobjects
     of the class are marked external, too. */
  /* Nonstatic data members (fields) first. */
  fp = type->variant.class_struct_union.field_list;
  for (; fp != NULL; fp = fp->next) {
    check_type_for_linkage_change(fp->type, count);
  }  /* for */
  /* Base classes. */
  ctsp = type->variant.class_struct_union.extra_info;
  bcp = ctsp->base_classes;
  for (; bcp != NULL; bcp = bcp->next) {
    check_type_for_linkage_change(bcp->type, count);
  }  /* for */
  if (ctsp->assoc_scope != NULL) {
    /* A routine entry for a member function needs not only a check of
       return and parameter types; it may also need its own linkage and
       storage class set properly. */
    rp = ctsp->assoc_scope->routines;
    for (; rp != NULL; rp = rp->next) {
      if (rp->is_inline) {
        /* An inline member function remains internally linked even
           when it is a member of an externally linked class. */
      } else {
        /* All other functions must be externally linked.  The storage
           class (extern or unspecified) depends on whether the function
           was defined in the current translation unit. */
        rp->source_corresp.name_linkage =
                 (a_name_linkage_kind)nlk_cplusplus_external;
        if (rp->assoc_scope == NULL_region_number) {
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
      check_type_for_linkage_change(rp->type, count);
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
#if DEBUG
      if (debug_level >= 3) {
        fputs("external linkage given to static data member \"", f_debug);
        db_name(&vp->source_corresp);
        fputs("\"\n", f_debug);
      }  /* if */
#endif /* DEBUG */
      check_type_for_linkage_change(vp->type, count);
    }  /* if */
    /* Classes nested in the class should also be treated as having external
       linkage. */
    tp = ctsp->assoc_scope->types;
    for (; tp != NULL; tp = tp->next) {
      check_type_for_linkage_change(tp, count);
    }  /* if */
    /* Any class used directly or indirectly in specifying a template class
       should be externally linked.  This is not explicitly specified by the
       ARM, but may be inferred. */
    tap = ctsp->template_arg_list;
    for (; tap != NULL; tap = tap->next) {
      if (tap->is_type) {
        tp = tap->variant.type;
      } else {
        tp = tap->variant.constant->type;
      }  /* if */
      check_type_for_linkage_change(tp, count);
    }  /* for */
  }  /* if */
  db_exit();
}  /* make_class_externally_linked */


static a_boolean is_candidate_for_linkage_change(a_type_ptr  tp)
/*
Return TRUE if the current linkage of this class is internal and there
is nothing that prevents it from being changed to having external linkage.
*/
{
  a_boolean  is_external_linkage_candidate = FALSE;

  db_enter(5, "is_candidate_for_linkage_change");
  check_assertion(is_immediate_class_type(tp) || is_immediate_enum_type(tp));
  if (tp->source_corresp.name_linkage !=
                               (a_name_linkage_kind)nlk_internal) {
    /* Already marked as having external linkage or else no linkage.  Only
       classes and enum types with internal linkage may be changed. */
  } else if (is_immediate_enum_type(tp)) {
    /* A non-local enum type may become externally linked. */
    is_external_linkage_candidate = TRUE;
  } else if (tp->variant.class_struct_union.extra_info->
                                                template_arg_list == NULL) {
    /* Not a template class -- it may become externally linked. */
    is_external_linkage_candidate = TRUE;
  } else {
    /* Template classes are usually externally linked.  The exception is
       template-generated class when the instantiation mode is tim_local
       (i.e., when all template classes and template functions referenced
       in the translation unit are instantiated but are left with internal
       linkage to avoid errors from the linker). */
    if (instantiation_mode != tim_local ||
        symbol_supplement_for_class(tp)->is_specific_template_def) {
      is_external_linkage_candidate = TRUE;
    }  /* if */
  }  /* if */
  db_exit();
  return is_external_linkage_candidate;
}  /* is_candidate_for_linkage_change */


static void check_type_for_linkage_change(a_type_ptr type,
                                          int        *count)
/*
If type is a class type that is eligible for change from internal to
external linkage, make that change.  If it contains such a type, make
the change on the contained type.
*/
{
  a_type_ptr                   tp;
  a_param_type_ptr             ptp;

  db_enter(4, "check_type_for_linkage_change");
  type = skip_typerefs(type);
  switch (type->kind) {
    case tk_class:
    case tk_struct:
    case tk_union:
      if (is_candidate_for_linkage_change(type)) {
        /* Class, struct, or union type.  Change it (and its members, where
           required) to have external linkage. */
        make_class_externally_linked(type, count);
      }  /* if */
      tp = type->source_corresp.class_of_which_a_member; 
      if (tp != NULL) {
        /* Nested class -- be sure parent class is also externally linked. */
        check_type_for_linkage_change(tp, count);
      }  /* if */
      break;
    case tk_routine:
      /* For routine types check both the return type and the types of each
         of the parameters. */
      check_type_for_linkage_change(type->variant.routine.return_type, count);
      ptp = type->variant.routine.extra_info->param_type_list;
      for (; ptp != NULL; ptp = ptp->next) {
        check_type_for_linkage_change(ptp->type, count);
      }  /* for */
      break;
    case tk_pointer:
      /* For pointer and reference types check the type pointed to. */
      check_type_for_linkage_change(type_pointed_to(type), count);
      break;
    case tk_array:
      /* For arrays check the element type. */
      check_type_for_linkage_change(array_element_type(type), count);
      break;
    case tk_ptr_to_member:
      /* For pointer-to-member type check both the class type and the member
         type.  Ordinarily this would be excessive, since the member type
         should be handled recursively when the class type is processed.  The
         the member type is handled independently to allow for the case where
         no member of that type exists. */
      tp = pm_class_type(type);
      check_type_for_linkage_change(tp, count);
      check_type_for_linkage_change(pm_member_type(type), count);
      break;
    case tk_integer:
      /* Check for an enum type.  If it's a member of a class, its class
         should be made externally linked, too. */
      if (type->variant.integer.enum_type) {
        tp = type->source_corresp.class_of_which_a_member;
        if (tp != NULL) {
          /* Nested enum -- changing the parent's linkage causes the linkage
             of all its nested types to be changed. */
          check_type_for_linkage_change(tp, count);
        } else if (is_candidate_for_linkage_change(type)) {
          make_enum_type_externally_linked(type, count);
        }  /* if */
      }  /* if */
      break;
    default:
      /* Cannot have a class subtype. */
      break;
  }  /* switch */
  db_exit();
}  /* check_type_for_linkage_change */


static a_boolean class_members_force_external_linkage(a_type_ptr  class_type)
/*
If class_type contains a static data member or a member function that is not
defined inline, or if has a nested class with such members, return TRUE.
*/
{
  a_scope_ptr    scope;
  a_boolean      external = FALSE;
  a_routine_ptr  rp;
  a_type_ptr     tp;

  scope = class_type->variant.class_struct_union.extra_info->assoc_scope;
  if (scope == NULL) {
    /* Class has not been defined. */
  } else if (scope->variables != NULL) {
    /* At least one static data member: external linkage is required. */
    external = TRUE;
  } else {
    /* Look for noninline member functions. */
    for (rp = scope->routines; rp != NULL; rp = rp->next) {
      if (!rp->is_inline) {
        /* At least one noninline member function: external linkage is
           required. */
        external = TRUE;
        break;
      }  /* if */
    }  /* for */
    if (!external) {
      /* Look for nested classes whose properties force external linkage not
         only on the nested class itself but on the parent class as well. */
      for (tp = scope->types; tp != NULL; tp = tp->next) {
        if (is_immediate_class_type(tp)) {
          /* Found a nested class.  Make a recursive call to check it. */
          if (class_members_force_external_linkage(tp)) {
            /* Nested class contains static data member or noninline
               function. */
            external = TRUE;
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return external;
}  /* class_members_force_external_linkage */


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
  int             num_internally_linked_types, count;
  a_scope_ptr     scope = il_header.primary_scope;
  a_type_ptr      tp;
  a_routine_ptr   rp;
  a_variable_ptr  vp;
  a_boolean       external;
  a_boolean       any_candidates_for_linkage_change = FALSE;
  a_symbol_ptr    sym;             

  db_enter(3, "check_class_linkage");
  /* Search for classes by making a pass over all the types associated with
     the file scope. */
  scope = il_header.primary_scope;
  for (tp = scope->types; tp != NULL; tp = tp->next) {
    if (tp->source_corresp.is_local_to_function) {
      /* Local type, possibly promoted to file scope during IL lowering of a
         routine.  Ignore it. */
      continue;
    }  /* if */
#if DEBUG
    if (debug_level >= 3) {
      fputs("file scope type: ", f_debug);
      db_abbreviated_type(tp);
      fputc('\n', f_debug);
    }  /* if */
#endif /* DEBUG */
    if (is_immediate_class_type(tp)) {
      /* Found a class. */
      if (is_candidate_for_linkage_change(tp)) {
        /* Class has internal linkage but nothing prevents it from changing
           to external linkage. */
        if (tp->variant.class_struct_union.extra_info->
                                              template_arg_list != NULL) {
          /* This is a template class, so it should have external linkage.
             (Template classes that should not have external linkage have been
             screened out by is_candidate_for_linkage_change. */
          external = TRUE;
        } else {
          sym = (a_symbol_ptr)tp->source_corresp.assoc_info;
          if (sym->force_external_linkage) {
            /* Type has been used in a context that requires it to have
               external linkage. */
            external = TRUE;
          } else {
            external = class_members_force_external_linkage(tp);
          }  /* if */
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
          any_candidates_for_linkage_change = TRUE;
        }  /* if */
      }  /* if */
    } else if (is_immediate_enum_type(tp)) {
      if (is_candidate_for_linkage_change(tp)) {
        sym = (a_symbol_ptr)tp->source_corresp.assoc_info;
        if (sym != NULL && sym->force_external_linkage) {
          int dummy_count = 0;
          make_enum_type_externally_linked(tp, &dummy_count);
        } else {
          any_candidates_for_linkage_change = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  if (any_candidates_for_linkage_change) {
    /* At least one class may have retained internal linkage after the
       previous scan.  Since it may have been subsequently referenced in
       declaring another class, it may have had its linkage changed to
       external after all; we need to make another pass over the class type
       entries to get an accurate count.  We keep track of the actual number
       of internally linked classes that remain so that we can stop looking
       at routines and variables as soon as possible. */
    num_internally_linked_types = 0;
    for (tp = scope->types; tp != NULL; tp = tp->next) {
      if (!tp->source_corresp.is_local_to_function &&
          (is_immediate_class_type(tp) || is_immediate_enum_type(tp))) {
        if (is_candidate_for_linkage_change(tp)) {
          num_internally_linked_types++;
        }  /* if */
      }  /* if */
    }  /* for */
    if (num_internally_linked_types > 0) {
      /* There is at least one internally linked class or enum type.  Make a
         pass over all the variables defined at file scope to determine
         whether the declaration of an externally linked variable entails a
         reference to a class or enum type that is still marked as internally
         linked. */
      for (vp = scope->variables; vp != NULL; vp = vp->next) {
        if (vp->source_corresp.name_linkage ==
                               (a_name_linkage_kind)nlk_cplusplus_external) {
          /* This is an externally linked variable.  Check its type. */
          check_assertion(vp->source_corresp.class_of_which_a_member == NULL);
          count = 0;
          check_type_for_linkage_change(vp->type, &count);
          /* "count" is returned as the number of internally linked classes
             that were changed to externally linked.  Adjust the number of
             internally linked classes remaining.  When it gets down to zero
             we can bail out. */
          num_internally_linked_types -= count;
          if (num_internally_linked_types < 1) break;
        }  /* if */
      }  /* for */
    }  /* if */
    if (num_internally_linked_types > 0) {
      /* There is still at least one internally linked class or enum type.
         Make a pass over the file scope routine entries similar to the one
         made for variables. */
      for (rp = scope->routines; rp != NULL; rp = rp->next) {
        if (rp->source_corresp.name_linkage ==
                               (a_name_linkage_kind)nlk_cplusplus_external) {
          /* This is an externally linked routine.  Check its type. */
          check_assertion(rp->source_corresp.class_of_which_a_member == NULL);
          count = 0;
          check_type_for_linkage_change(rp->type, &count);
          /* Again, we can bail out when the number of internally linked
             classes is reduced to zero. */
          num_internally_linked_types -= count;
          if (num_internally_linked_types < 1) break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  /* Go through the classes again, now that linkage decisions have been
     made, and generate bodies for virtual destructors, as required. */
  for (tp = scope->types; tp != NULL; tp = tp->next) {
    if (is_immediate_class_type(tp) && tp->source_corresp.assoc_info != NULL) {
      a_class_symbol_supplement_ptr  cssp = symbol_supplement_for_class(tp);
      if (cssp->destructor != NULL) {
        rp = cssp->destructor->variant.routine.ptr;
        if (rp->is_virtual && rp->compiler_generated &&
            rp->assoc_scope == NULL_region_number) {
          /* The destructor for the current class is virtual and was generated
             automatically but does not yet have a body. */
          if (virtual_dtor_should_be_generated_for_class(tp)) {
            /* But the body for it should be generated, e.g., because the
               virtual function table in which its address will appear is
               being generated.  (Since no errors are issued on the definitions
               of destructors, the error position used has no effect.) */
            define_special_member_function(rp);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  db_exit();
}  /* check_class_linkage */


void class_decl_init(void)
/*
Initializations for class declaration processing.
*/
{
  /* Initialize the list of freed delayed-scan-fixup entries. */
  avail_routine_fixup = NULL;
  curr_routine_fixup = NULL;
  /* Initialize the list of freed derivation-step entries. */
  avail_derivation_steps = NULL;
#if DEBUG
  num_routine_fixups_allocated = 0;
#endif /* DEBUG */
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
