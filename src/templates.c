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

templates.c -- Support for C++ templates.

*/

#include "basics.h"
#include "templates.h"
#include "cmd_line.h"
#include "decl_inits.h"
#include "decls.h"
#include "error.h"
#include "il.h"
#include "lexical.h"
#include "mem_manage.h"
#include "lower_name.h"
#include "statements.h"
#include "symbol_tbl.h"
#include "types.h"

#if AUTOMATIC_TEMPLATE_INSTANTIATION
typedef struct an_instance_lookup_entry *an_instance_lookup_entry_ptr;
typedef struct an_instance_lookup_entry {
  /* Structure used to represent entries in the hash table of template
     instantiation names.  This is used to match entries from the
     instantiation list file with entries on the compilers instantiation
     required list. */
  an_instance_lookup_entry_ptr
		next;
			/* Pointer to the next instance in a given hash
			   table bucket. */
  char		*name;
			/* Name of the instance.  This is the name read from
			   the instantiation list file.  This will typically
			   be the mangled name of the function or static
			   data member. */
} an_instance_lookup_entry;

#define INSTANCE_LOOKUP_TABLE_SIZE 127
			/* The number of buckets in the instance lookup table.
			   This number should be prime. */

static an_instance_lookup_entry_ptr
		instance_lookup_table[INSTANCE_LOOKUP_TABLE_SIZE];
			/* Each element of the array points to a list of
			   entries associated with instantiations that hashed
			   to a given group. */

#define HASH_FACTOR 73
			/* The multiplier used in the hash algorithm that
			   generates an index in the hash table from an
                           identifier name string.
			   Do not change without investigating the
			   hash table performance that results.  Prime
			   values are likely to work better than
			   non-prime values. */
#define INFO_FILE_LINE_INCREMENTAL_ALLOCATION 256
			/* The number of bytes added to the information file
			   input line each time it is reallocated; also the
			   initial allocation. */

static a_boolean
		any_instantiations_required;
			/* TRUE if there are any template instantiations
			   needed for this compilation.  This is TRUE
			   whether or not the instantiations are provided
			   by this file.  This is used to determine whether
			   to create an instantiation information file. */

static char	*instantiation_info_file_name;
                        /* The name of a file containing a list of names
			   of template functions and static data members to
			   be instantiated.  Intended to be used for linker
			   feedback mechanisms to provide automatic
			   instantiation. */
static FILE	*f_instantiation_info;
			/* File from which the instantiation list should be
			   read.  Only valid when do_auto_instantiation is
			   TRUE. */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

typedef struct a_can_instantiate_entry *a_can_instantiate_entry_ptr;
typedef struct a_can_instantiate_entry {
  /* Structure used to build a list of classes that have been used
     in can_instantiate pragmas.  This is used during instantiation
     wrapup to instantiate classes that have not been otherwise used.
     The instantiation needs to be delayed so that any template entities
     generated as a result of the instantiation of the class can be
     specially flagged. */
  a_can_instantiate_entry_ptr
		next;
			/* Pointer to the next instance in a given hash
			   table bucket. */
  a_type_ptr	class_type;
			/* Pointer to the template class type to
			   be instantiated later. */
} a_can_instantiate_entry;

static a_can_instantiate_entry_ptr can_instantiate_list;
	
static a_def_arg_expr_fixup_ptr	curr_default_args;
			/* Pointer to the default argument entries for
                           the function template being scanned. */

static a_template_instance_ptr instantiations_required;
			/* Points to the first entry on a list of template
			   instance entries for which either function
			   instantiations or compiler-generated static data
			   member definitions are required.  Entries are
			   added to the end of the list.  */

static a_template_instance_ptr instantiations_required_tail;
			/* Points to the end of the instantiations_required
			   list; needed because entries added to this list
			   must be added at the end. */

static a_boolean
		in_instantiation_wrapup;
			/* TRUE when instantiation wrapup has been called
			   to do end-of-compilation unit instantiations. */

static a_boolean
		entries_updated_during_instantiation_wrapup;
			/* TRUE when entries on the instantiation required
			   list have their instantiation required flag
			   set during instantiation wrapup.  This is used to
			   detect situations when an entry that may have
			   already been visited by instantiation wrapup
			   has its instantiation required flag updated
			   while processing an entry later on the list. */


static a_boolean instantiation_of_type_is_in_progress(a_type_ptr tp)
/*
Return TRUE if a class/struct/union scope for tp, which represents a template
class, is currently on the scope stack.  If it is, that means an instantiation
for tp is currently in progress.
*/
{
  a_scope_stack_entry_ptr ssep = &scope_stack[depth_scope_stack];
  a_boolean               found = FALSE;

  /* Loop through the scope stack. */
  for (; ssep != &scope_stack[0]; ssep--) {
    if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
        ssep->il_scope->variant.assoc_type == tp) {
      found = TRUE;
      break;
    }  /* if */
  }  /* for */
  return found;
}  /* instantiation_in_progress */


static void update_instantiation_required_for_template_class_members
						(a_type_ptr	class_type)
/*
Calls update_instantiation_required_flag for all member functions and
static data members declared in the class.  This needs to be called after
the class instantiation is complete so that the function or static data
member instantiation has access to the complete class.  This routine calls
itself recursively to process classes nested within this class.
*/
{
  a_class_type_supplement_ptr	ctsp;
  a_variable_ptr		var;
  a_symbol_ptr			sym;
  a_template_instance_ptr	tip;
  a_routine_ptr			rout;
  a_type_ptr			type;

  db_enter(4, "update_instantiation_required_for_template_class_members");  
  ctsp = class_type->variant.class_struct_union.extra_info;
  /* The assoc_scope pointer can be NULL if errors occurred during the
     instantiation of the class. */
  if (ctsp->assoc_scope != NULL) {
    /* Function instantiation entries are not marked for actual instantiation
       (that is, for generation of the function body) until there is
       an invocation of the function.  However, if the function is
       virtual, mark it for instantiation in all cases, since a
       virtual function table may have to be put out for it.  All
       instances are placed on the instantiation list.  In tim_all
       mode the instantiations will be generated even if the
       instantiation required flag is not set. */
    rout = ctsp->assoc_scope->routines;
    while (rout != NULL) {
      sym = (a_symbol_ptr)rout->source_corresp.assoc_info;
      tip = sym->variant.routine.instance_ptr;
      if (tip != NULL && !tip->instantiation_required) {
        /* Under certain conditions the instance pointer will be NULL.  This
           occurs for compiler generated routines and under some error
           conditions.  Simply skip this routine. */
        update_instantiation_required_flag
                      (tip, (a_boolean)(sym->variant.routine.ptr->is_virtual));
      }  /* if */
      rout = rout->next;
    }  /* while */
    
    /* Static data members are eligible for a compiler-generated definition
       only if a template definition appears in the source.  However, it
       still needs to appear on the instantiation-required list (because
       instantiation is required required somewhere in the program even if
       not in the current translation unit). */
    var = class_type->variant.class_struct_union.extra_info->
							assoc_scope->variables;
    while (var != NULL) {
      sym = (a_symbol_ptr)var->source_corresp.assoc_info;
      tip = sym->variant.static_data_member.instance_ptr;
#if 0
      /* Are there error cases when tip can be NULL?  It is probably safer
         to skip setting the instantiation required flag rather than
         generate a possibly spurious internal error. */
#endif /* 0 */
      if (tip != NULL && !tip->instantiation_required) {
        update_instantiation_required_flag(tip, /*value=*/TRUE);
      }  /* if */
      var = var->next;
    }  /* while */
    /* Process any classes nested within this class. */
    type = ctsp->assoc_scope->types;
    while (type != NULL) {
      a_type_kind	tk = type->kind;
      if (tk == (a_type_kind)tk_class ||
          tk == (a_type_kind)tk_struct || tk == (a_type_kind)tk_union) {
        update_instantiation_required_for_template_class_members(type);
      }  /* if */
      type = type->next;
    }  /* while */
  }  /* if */
  db_exit();
}  /* update_instantiation_required_for_template_class_members */


void f_check_for_uninstantiated_template_class(a_type_ptr  tp)
/*
tp is an incomplete type.  If it is a class in need of instantiation or an
array whose underlying element type is such a class, instantiate it.
Otherwise, do nothing.
*/
{
  if (is_array_type(tp)) tp = underlying_array_element_type(tp);
  if (tp != NULL && is_class_struct_union_type(tp)) {
    instantiate_template_class(tp);
  }  /* if */
}  /* f_check_for_uninstantiated_template_class */


void f_instantiate_template_class(a_type_ptr  class_type)
/*
class_type is an incomplete class type.  If it is an instance of a class
template, perform a full instantiation of it.  This entails rescanning the
tokens that were cached when the template definition was originally
encountered; the cached tokens include the base specifiers list, if any,
and the class body (from opening left brace through closing right brace).
The template arguments (the real values which the template parameters take
on) have been recorded in class_type and will be substituted for the
template parameters when the instantiation scope is pushed.

This routine should be called from macro instantiatiate_template_class,
which determines that class_type is an incomplete type.  If it also turns
out to be a template type, this routine attempts to instantiate it; it
might not be able to if the template itself has not yet been defined.
*/
{
  a_symbol_ptr                      instance_sym, template_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_token_cache                     *p_token_cache;
  a_class_symbol_supplement_ptr     cssp;
  a_template_arg_ptr                template_arg_list;

  db_enter(3, "f_instantiate_template_class");
#if CHECKING
  if (!is_class_struct_union_type(class_type)) {
    internal_error("f_instantiate_template_class: not a class");
  }  /* if */
#endif /* CHECKING */
  class_type = skip_typerefs(class_type);
  instance_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  cssp = instance_sym->variant.class_struct_union.extra_info;
  template_sym = cssp->class_template;
  if (template_sym == NULL) {
    /* Not a class based on a class template. */
  } else if (cssp->is_nonreal_class) {
    /* Don't try to instantiate a template class without real template
       arguments. */
  } else if (cssp->is_specific_template_def) {
    /* This is an attempt to instantiate an incomplete type that is
       a specific definition.  This can occur in error cases while scanning
       the class definition.  Simply ignore the instantiation request. */
  } else {
    /* There is a class template from which to generate this class and it is
       a real instantiation. */
    tssp = template_sym->variant.template_info;
    p_token_cache = &tssp->token_cache;
    if (p_token_cache->first_token == NULL) {
      /* The template itself has not yet been defined.  The caller will
         issue an incomplete-type error. */
    } else if (!tssp->variant.class_template.
					prototype_instantiation_complete) {
      /* A real instantiation is being requested while the prototype
         instantiation is still be processed.  We simply ignore the
         instantiation request which will typically result in an incomplete
         type not allowed error to be issued by the caller. */
    } else if (instantiation_of_type_is_in_progress(class_type)) {
      /* This particular template class (not just some other one based on
         the same template) is currently being instantiated. */
    } else if (tssp->pending_instantiations >= MAX_PENDING_INSTANTIATIONS) {
      /* This class instantiation occurs within the context of other
         instantiations of the same class template.  When the number of
         such instantiations-in-progress exceeds a configuration
         constant value, we assume this to be runaway recursion -- for
         for instance (to give a rather unlikely example):
            template <class T, int I> class X {
              X<T,I+1> x;
            };
      */                
      sym_error(ec_runaway_recursive_instantiation, instance_sym);
      /* Give class_type a size of 1 so it won't be treated as incomplete in
         subsequent processing. */
      class_type->size = 1;
    } else {
      /* We proceed with the instantiation. */
      /* Increment the count of instantiations-in-progress for the current
         class template.  It will be decremented when the instantiation is
         complete. */
      ++(tssp->pending_instantiations);
#if DEBUG
      if (debug_level >= 3) {
        fprintf(f_debug, "instantiating: ");
        db_type(class_type);
        db_symbol(template_sym, "\nbased on: ", 2);
      }  /* if */
#endif /* DEBUG */
      /* Push a template instantiation scope.  The real values of the
         the template arguments will be associated with the template
         parameter names. */
      template_arg_list = class_type->variant.class_struct_union.extra_info->
                                                             template_arg_list;
      (void)push_scope((a_scope_kind)sck_template_instantiation,
                       tssp->declaration_scope, class_type,
                       (a_routine_ptr)NULL, instance_sym, template_sym,
                       template_arg_list);
      /* The tokens of the template definition have been cached away.
         Activate the cache so that they can be rescanned in light of
         the new values associated with the template parameters. */
      rescan_reusable_cache(p_token_cache);
#if CHECKING
      if (curr_token != tok_lbrace && curr_token != tok_colon) {
        internal_error("f_instantiate_template_class: bad 1st token in cache");
      }  /* if */
#endif /* CHECKING */
      mark_defined(instance_sym, &instance_sym->decl_position);
      /* Scan the base specifiers list, if any, and the body of the class. */
      (void)scan_class_definition(class_type, DEPTH_OF_FILE_SCOPE,
                                  /*is_local_class=*/FALSE,
                                  /*is_prototype_instantiation=*/FALSE);
      update_instantiation_required_for_template_class_members(class_type);
      pop_scope();
      /* In the normal case the current token should be end_of_source,
         which was inserted to mark the end of the cached token stream.
         If necessary, keep flushing until end-of-source is found. */
      flush_past_token_cache_terminator();
      /* Decrement the count of instantiations-in-progress for the current
         class template. */
      --(tssp->pending_instantiations);
      /* If this instantiation occurred in the midst of a class definition,
         the instantiation may be dependent upon nested types from the class.
         The instantiation was put out on the file scope types list, but
         the types upon which it is possibly dependent have been recorded on
         the class scope types list.  To enable il-lowering to get the
         ordering right when it promotes the nested types to file scope,
         enter a placeholder type in the class scope to mark the declaration
         position of the instantiation.  This is not an issue when the class
         is a local class, since a template cannot legally be defined in terms
         of local classes or types that are local class members. */
      if (scope_stack[decl_scope_level].kind ==
                                 (a_scope_kind)sck_class_struct_union &&
          depth_innermost_function_scope == NO_SCOPE_DEPTH) {
        a_type_ptr  tp;

        /* Allocate the placeholder type, set its fields, and add it to the
           types list of the class.  Note that this typeref has no name
           or symbol associated with it. */
        tp = alloc_type((a_type_kind)tk_typeref);
        tp->variant.typeref.type = class_type;
        tp->variant.typeref.is_placeholder_for_file_scope_type = TRUE;
        class_type->variant.class_struct_union.
                             referenced_by_placeholder_typeref = TRUE;
        add_to_types_list(tp, decl_scope_level);
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* f_instantiate_template_class */


static void instantiate_class_template(a_symbol_ptr  template_sym,
                                       a_type_ptr    prototype_type)
/*
This routine is called to do a "prototype instantiation" of a class template,
namely, to scan the template definition even though the template parameters
have not yet been given "real" values; because declaration/expression
disambiguation often cannot be done based on dummy types, only declarative
information is scanned; inline function bodies and default argument
expressions are ignored.  A side effect of this scan is to detect gross
syntax errors.  The main benefit is to record the names and types of member
functions and static data members, for which template definitions may be
encountered.
*/
{
  a_template_symbol_supplement_ptr  tssp;
  a_token_cache                     *p_token_cache;
  a_symbol_ptr                      instance_sym;
  a_template_arg_ptr                template_arg_list;

  db_enter(3, "instantiate_class_template");
  tssp = template_sym->variant.template_info;
  p_token_cache = &tssp->token_cache;
#if CHECKING
  if (p_token_cache->first_token == NULL) {
    /* The template itself has not yet been defined. */
    internal_error("instantiate_class_template: bad cache");
  } else if (instantiation_of_type_is_in_progress(prototype_type)) {
    /* The template is currently being instantiated. */
    internal_error("instantiate_class_template: already being instantiated");
  };
#endif /* CHECKING */
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(template_sym, "prototype instantiation of: ", 2);
  }  /* if */
#endif /* DEBUG */
  instance_sym = (a_symbol_ptr)prototype_type->source_corresp.assoc_info;
  /* Save a pointer to the prototype instantiation. */
  tssp->variant.class_template.prototype_instantiation = instance_sym;
  template_arg_list = prototype_type->variant.class_struct_union.extra_info->
                                                             template_arg_list;
  (void)push_scope((a_scope_kind)sck_template_instantiation,
                   tssp->declaration_scope, prototype_type,
                   (a_routine_ptr)NULL, instance_sym, template_sym,
                   template_arg_list);
  rescan_reusable_cache(p_token_cache);
#if CHECKING
  if (curr_token != tok_lbrace && curr_token != tok_colon) {
    internal_error("instantiate_class_template: bad 1st token in cache");
  }  /* if */
#endif /* CHECKING */
  mark_defined(template_sym, &template_sym->decl_position);
  /* Scan the base specifiers list, if any, and the body of the class. */
  (void)scan_class_definition(prototype_type, DEPTH_OF_FILE_SCOPE,
                              /*is_local_class=*/FALSE,
                              /*is_prototype_instantiation=*/TRUE);
  pop_scope();
  /* In the normal case the current token should be end_of_source,
     which was inserted to mark the end of the cached token stream.
     If necessary, keep flushing until end-of-source is found. */
  flush_past_token_cache_terminator();
  /* Set the flag that indicates that the prototype instantiation has
     been completed. */
  tssp->variant.class_template.prototype_instantiation_complete = TRUE;
  /* Save a pointer to the prototype instantiation. */
  tssp->variant.class_template.prototype_instantiation = instance_sym;
  db_exit();
}  /* instantiate_class_template */


void instantiate_template_function(a_template_instance_ptr  tip)
/*
Instantiate the body of the template function associated with tip.
*/
{
  a_symbol_ptr                      rout_sym;
  a_routine_ptr                     rout_ptr;
  a_template_symbol_supplement_ptr  tssp;

  db_enter(3, "instantiate_template_function");
  rout_sym = tip->instance_sym;
  rout_ptr = rout_sym->variant.routine.ptr;
  if (rout_ptr->assoc_scope != NULL_region_number) {
    /* Already instantiated. */
    goto done;
  }  /* if */
  if (rout_sym->kind == (a_symbol_kind)sk_member_function) {
    tssp = tip->template_sym->variant.routine.instance_ptr->template_info;
  } else {
    tssp = tip->template_sym->variant.template_info;
  }  /* if */
  if (tssp->pending_instantiations >= MAX_PENDING_INSTANTIATIONS) {
    /* This function instantiation occurs within the context of other
       instantiations of the same function template.  When the number of
       such instantiations-in-progress exceeds a configuration
       constant value, we assume this to be runaway recursion -- for
       for instance (to give a rather unlikely example):

       template <int i> class A {
         void f() {
           A<i+1> a;
           a.f();
         }
       };
       void main() {
         A<1> a;
         a.f();
       }

       Note that this can only catch recursive instantiations of inline
       functions.  Runaway instantiations of out-of-line functions can
       not be detected this way because they are instantiated serially
       not recursively.
    */                
    sym_error(ec_runaway_recursive_instantiation, rout_sym);
    goto done;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    fprintf(f_debug, "instantiating: ");
    db_symbol(rout_sym, "", 0);
    db_symbol(tip->template_sym, "\nbased on: ", 2);
  }  /* if */
#endif /* DEBUG */
  rout_ptr->is_inline = tssp->variant.function.routine->is_inline;
  rout_sym->defined = TRUE;
  if (rout_ptr->type->kind == (a_type_kind)tk_typeref) {
    /* The function was declared using a typedef.  Now that it is being
       defined (given a body by the instantiation), create an unshared type
       with the typedef stripped off. */
    a_type_ptr  new_tp = alloc_type((a_type_kind)tk_routine);
    copy_routine_type_with_param_types(skip_typerefs(rout_ptr->type), new_tp);
    rout_ptr->type = new_tp;
  }  /* if */
  /* Set the linkage and storage class. */
  if (rout_sym->class_of_which_a_member != NULL) {
    /* Member functions are handled in check_class_linkage. */
#if 0
    /* Should the reference flag be set in scan_function body? */
#endif /* 0 */
    rout_sym->class_of_which_a_member->source_corresp.referenced = TRUE;
  } else {
    if (instantiation_mode == tim_local) {
      /* Put out template function as internally linked. */
      rout_ptr->storage_class = (a_storage_class)sc_static;
      rout_ptr->source_corresp.name_linkage =
                                  (a_name_linkage_kind)nlk_internal;
    } else if (!rout_ptr->is_inline) {
      /* Set the linkage for the definition of an externally linked routine. */
      rout_ptr->storage_class = (a_storage_class)sc_unspecified;
      rout_ptr->source_corresp.name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
    }  /* if */
  }  /* if */
  ++(tssp->pending_instantiations);
  /* Push the template instantiation scope. */
  (void)push_scope((a_scope_kind)sck_template_instantiation,
                   tssp->declaration_scope, (a_type_ptr)NULL, rout_ptr,
                   rout_sym, tip->template_sym, tip->arg_list);
  /* Reactivate the tokens comprising the function body and scan them. */
  rescan_reusable_cache(&tssp->token_cache);
  scan_function_body(rout_ptr, &tssp->variant.function.func_info,
                     (SFB_NEW_STRUCT_STMT_STACK_REQUIRED |
                      SFB_IS_INSTANTIATION));
  /* Pop the template instantiation scope. */
  pop_scope();
  --(tssp->pending_instantiations);
  /* In the normal case the current token should be end_of_source, which was
     inserted to mark the end of the cached token stream. If necessary, keep
     flushing until end-of-source is found. */
  flush_past_token_cache_terminator();
  /* Usually template functions are instantiated "on demand" and the
     referenced flag will already have been set.  But if the
     instantiation mode says to instantiate whether or not there is
     a reference, we should set the referenced flag anyway, so that
     the back-end will be sure to generate the function. */ 
  tip->instance_sym->variant.routine.ptr->source_corresp.referenced = TRUE;

done:;
  /* The already instantiated flag is set even if certain error conditions
     (such as runaway instantiation) to prevent the compiler from attempting
     to instantiate this function again. */
  tip->already_instantiated = TRUE;
  db_exit();
}  /* instantiate_template_function */


void define_template_static_data_member(a_template_instance_ptr  tip)
/*
Generate a definition of a static data member of a template class.  The
definition may be based on a template definition of the static data
member or may be a default initialization.  Checking for runaway
instantiation is not necessary for static data members because 
static data members are instantiated as result of class instantiations --
and the class instantiation will detect the runaway case.
*/
{
  a_symbol_ptr                      static_data_member_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_variable_ptr		    var_ptr;

  db_enter(3, "define_template_static_data_member");
  var_ptr = tip->instance_sym->variant.static_data_member.variable;
  tssp = tip->template_sym->
                 variant.static_data_member.instance_ptr->template_info;
  static_data_member_sym = tip->instance_sym;
#if CHECKING
  if (!tip->template_sym->defined || tssp->parameters == NULL) {
    internal_error("define_template_static_data_member: undef'd template");
  } else if (static_data_member_sym->defined) {
    internal_error("define_template_static_data_member: sym already def'd");
  }  /* if */
#endif /* CHECKING */
  static_data_member_sym->defined = TRUE;
  if (tssp->token_cache.first_token != NULL) {
    a_boolean  incomplete_type_error_reported;
    a_boolean  has_parenthesized_initializer;
    a_type_ptr tp = tip->template_sym->class_of_which_a_member;
    while (tp->source_corresp.class_of_which_a_member != NULL) {
      tp = tp->source_corresp.class_of_which_a_member;
    }  /* while */
    /* Push a template instantiation scope.  The real values of the
       the template arguments will be associated with the template
       parameter names. */
#if 0
    /* But note that a template parameter T will be hidden by a member T --
       is this correct? */
#endif /* if 0 */
    (void)push_scope((a_scope_kind)sck_template_instantiation,
                     tssp->declaration_scope, (a_type_ptr)NULL,
                     (a_routine_ptr)NULL, static_data_member_sym,
                     tip->template_sym, tip->arg_list);
    push_class_reactivation_scope(static_data_member_sym->
                                                  class_of_which_a_member);
    

    rescan_reusable_cache(&tssp->token_cache);
    /* If the first token is an equals sign then this is not a parenthesized
       initializer.   Initializers that begin with an invalid token will
       have already been discarded. */
    if (curr_token == tok_assign) {
      /* Discard the equals sign. */
      has_parenthesized_initializer = FALSE;
      (void)get_token();
    } else {
      has_parenthesized_initializer = TRUE;
    }  /* if */
    initializer(static_data_member_sym, &static_data_member_sym->decl_position,
                idl_internal, has_parenthesized_initializer,
                /*is_old_style_param_decl=*/FALSE,
                &incomplete_type_error_reported);
    if (curr_token != tok_end_of_source) {
      pos_error(ec_exp_semicolon, &pos_curr_token);
      while (curr_token != tok_end_of_source) (void)get_token();
    }  /* if */
    /* By pass end-of-source token, which is probably the terminator token
       in the cache. */
    (void)get_token();
    pop_class_reactivation_scope();
    pop_scope();

  } else {
    (void)def_initializer(static_data_member_sym,
                          &static_data_member_sym->decl_position);
  }  /* if */
  /* Usually template functions are instantiated "on demand" and the
     referenced flag will already have been set.  But if the
     instantiation mode says to instantiate whether or not there is
     a reference, we should set the referenced flag anyway, so that
     the back-end will be sure to generate the function. */ 
  var_ptr->source_corresp.referenced = TRUE;
  var_ptr->is_template_static_data_member = TRUE;
  tip->already_instantiated = TRUE;
  db_exit();
}  /* define_template_static_data_member */


a_boolean equiv_template_arg_lists(a_template_arg_ptr  list1,
                                   a_template_arg_ptr  list2,
                                   a_boolean           is_func_template)
/*
Return TRUE if the two linked lists of template arguments for a given template
class or template function are equivalent -- that is, if corresponding type
arguments refer to the same type and corresponding constant arguments refer to
the same constant.  If is_func_template is TRUE, the lists will contain
only type arguments.
*/
{
  a_boolean           equiv;
  a_template_arg_ptr  arg1 = list1, arg2 = list2;

  db_enter(4, "equiv_template_arg_lists");
#if CHECKING
  /* There is no way to produce a NULL template argument list, so the real
     code doesn't need to check for that. */
  if (arg1 == NULL || arg2 == NULL) {
    internal_error("equiv_template_arg_lists: NULL arg list");
  }  /* if */
#endif /* CHECKING */
  /* Assume they are equivalent, until we find evidence to the contrary. */
  equiv = TRUE;
  /* Loop through both lists in step, comparing arguments. */
  do {
#if CHECKING
    if (is_func_template) {
      /* Nontype arguments are not allowed in function template arg lists. */
      if (!arg1->is_type || !arg2->is_type) {
        internal_error("equiv_template_arg_lists: nontype arg");
      }  /* if */
    } else {
    /* For a given class, argument lists should always have the same sequence
       of type and constant arguments. */
      if (arg1->is_type != arg2->is_type) {
        internal_error("equiv_template_arg_lists: arg inconsistency");
      }  /* if */
    }  /* if */
#endif /* CHECKING */
    if (!is_func_template && !arg1->is_type) {
      /* Both are constant arguments.  If they are not identical, this is a
         mismatch. */
      if (!eq_constants(arg1->variant.constant, arg2->variant.constant)) {
        equiv = FALSE;
        break;
      }  /* if */
    } else {
      /* Both are type arguments.  If they are not identical, this is a
         mismatch. */
      if (!identical_types(arg1->variant.type, arg2->variant.type)) {
        equiv = FALSE;
        break;
      }  /* if */
    }  /* if */
    /* Advance to the next arguments in step. */
    arg1 = arg1->next;
    arg2 = arg2->next;
#if CHECKING
    /* For a given function argument lists should always be exactly the same
       length. */
    if ((arg1 == NULL) != (arg2 == NULL)) {
      internal_error("equiv_template_arg_lists: unequal arg list lengths");
    }  /* if */
#endif /* CHECKING */
  } while (arg1 != NULL);

  db_exit();
  return equiv;
}  /* equiv_template_arg_lists */


/* Forward declaration for recursive reference. */
static a_boolean template_arg_involves_template_param(a_template_arg_ptr tap);

static a_boolean type_involves_template_param(a_type_ptr tp)
/*
Return TRUE if the type pointed to by tp is a template parameter type or
has a template parameter (type or constant) in its type tree.
*/
{
  a_boolean           found;

  tp = skip_typerefs(tp);
  switch (tp->kind) {
    case tk_template_param:
      found = TRUE;
      break;
    case tk_pointer:
      found = type_involves_template_param(type_pointed_to(tp));
      break;
    case tk_array:
      found = type_involves_template_param(underlying_array_element_type(tp));
      break;
    case tk_routine:
      /* Check the return type and each parameter type. */
      if (type_involves_template_param(tp->variant.routine.return_type)) {
        found = TRUE;
      } else {
        a_param_type_ptr ptp = tp->variant.routine.extra_info->param_type_list;
        found = FALSE;
        for (; ptp != NULL; ptp = ptp->next) {
          if (type_involves_template_param(ptp->type)) {
            found = TRUE;
            break;
          }  /* if */
        }  /* if */
      }  /* if */
      break;
    case tk_ptr_to_member:
      found = type_involves_template_param(pm_member_type(tp)) ||
              type_involves_template_param(pm_class_type(tp));
      break;
    case tk_class:
    case tk_struct:
    case tk_union:
      /* If the class is a nonreal class it may be assumed that it is based
         on template parameters. */
      found = (symbol_supplement_for_class(tp))->is_nonreal_class;
#if CHECKING
      if (!found) {
        a_template_arg_ptr  tap;
        tap = tp->variant.class_struct_union.extra_info->template_arg_list;
        for (; tap != NULL; tap = tap->next) {
          if (template_arg_involves_template_param(tap)) {
            internal_error(
                    "type_involves_template_param: bad is_nonreal_class flag");
          }  /* if */
        }  /* for */
      }  /* if */
#endif /* CHECKING */
      break;
    case tk_error:
#if 0
/* Is this correct?? */
#endif /* if 0 */
      found = FALSE;
      break;
    default:
      found = FALSE;
  }  /* switch */
  return found;
}  /* type_involves_template_param */


static a_boolean template_arg_involves_template_param(a_template_arg_ptr tap)
/*
Return TRUE if the template argument entry pointed to by tap contains
a template parameter (type or constant).
*/
{
  a_boolean  template_param_found;

  if (tap->is_type) {
    template_param_found = type_involves_template_param(tap->variant.type);
  } else {
    template_param_found = (tap->variant.constant->kind ==
                                 (a_constant_repr_kind)ck_template_param);
  }  /* if */
  return template_param_found;
}  /* template_arg_involves_template_param */


a_symbol_ptr find_template_class(a_symbol_ptr        class_template_sym,
                                 a_template_arg_ptr  *new_list,
                                 a_source_position   *source_pos,
				 a_boolean	     prototype_allowed)
/*
Given a symbol for a class template and a template argument list (that is,
a list of actual arguments), look for an existing class that is the
corresponding instantiation of the template.  If none is found, create
such an instantiation (i.e., allocate the type entry and create the
symbol, adding the latter to the instantiation list for the template).
Return the symbol that is found or newly created.

Note that this function does not fully instantiate a class template;
rather, when it creates a class type entry, it is for an incomplete type.
The full instantiation is done later, when it is clearly needed.  This
allows this kind of code to be handled correctly:

  template <class T> class X;  // class template X is not yet defined.
  X<int> *pxi;                 // declares a pointer to an instantiation of X
                               //   which is incomplete at this point.

The class template X may or may not be defined subsequently, and even if it
is the full instantiation of X<int> may not be needed.  This is consistent
with the handling of pointers to incomplete non-template classes:

  class Y;                     // Y is not yet defined.
  Y *py;                       // pointer to incomplete class is okay.

Note, moreover, that even if class template X were defined there would be
no need to actually instantiate X<int> in the example above.

If prototype_allowed is TRUE then the prototype instantiation is checked
before any of the other instantiations and is returned if the argument
lists match.  If it is FALSE the prototype instantiation will not be
included in the search.
*/
{
  a_symbol_ptr                      sym, prev_sym;
  a_symbol_ptr 			    prototype_sym;
  a_template_arg_ptr                old_list;
  a_type_ptr                        class_type;
  a_template_symbol_supplement_ptr  tssp;
  a_template_arg_ptr                tap;

  db_enter(3, "find_template_class");
  tssp = class_template_sym->variant.template_info ;
  sym = NULL;
  prototype_sym = tssp->variant.class_template.prototype_instantiation;
  if (prototype_allowed && prototype_sym != NULL) {
    /* Old list is the template argument list from a template class that has
       already been created.  See if the list passed in matches it. */
    old_list = prototype_sym->variant.type->
                     variant.class_struct_union.extra_info->template_arg_list;
    if (equiv_template_arg_lists(old_list, *new_list,
                                 /*is_func_template=*/FALSE)) {
      /* A match.  Set sym which will suppress any further search. */
      sym = prototype_sym;
    }  /* if */
  }  /* if */
  if (sym == NULL) {
    /* Make a pass over the symbols representing instantiations of the class
       template. */
    sym = tssp->variant.class_template.instantiations;
    prev_sym = NULL;
    for (; sym != NULL; prev_sym = sym, sym = sym->next) {
      /* The prototype instantiation should not be checked.  If
         prototype_allowed is TRUE then we would have already found it
         in the test above. */
      if (sym == prototype_sym) continue;
      /* Old list is the template argument list from a template class that has
         already been created.  See if the list passed in matches it. */
      old_list = sym->variant.type->
                     variant.class_struct_union.extra_info->template_arg_list;
      if (equiv_template_arg_lists(old_list, *new_list,
                                   /*is_func_template=*/FALSE)) {
        /* We've found a match.  Remove the found symbol from its current
           position in the instantiation list and add it to the front. */
        if (prev_sym != NULL) {
          prev_sym->next = sym->next;
          sym->next = tssp->variant.class_template.instantiations;
          tssp->variant.class_template.instantiations = sym;
        }  /* if */
#if DEBUG
        if (debug_level >= 3) db_symbol(sym, "found: ", 2);
#endif /* DEBUG */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  if (sym == NULL) {
    /* No match was found on the list, so do a partial instantiation of the
       template class based on the template arguments.  First create a symbol
       (but do not enter it into the symbol table, since class templates
       are always looked up through the template. */
    sym = make_template_class_symbol(class_template_sym, source_pos);
    /* Add the new symbol to the head of the instantiation list. */
    sym->next = tssp->variant.class_template.instantiations;
    tssp->variant.class_template.instantiations = sym;
    /* Now create a new type entry. */
    class_type = alloc_type(tssp->variant.class_template.type_kind);
    sym->variant.class_struct_union.type = class_type;
    /* If this is a "real instantiation" leave the type incomplete; it will
       become complete when it is instantiated.  However, if it is based on
       template parameters and is therefore a "nonreal" instantiation, give it
       a size and alignment to permit it to pass through subsequent processing
       without causing spurious errors. */
    for (tap = *new_list; tap != NULL; tap = tap->next) {
      if (!sym->variant.class_struct_union.extra_info->is_nonreal_class) {
        if (template_arg_involves_template_param(tap)) {
          sym->variant.class_struct_union.extra_info->is_nonreal_class = TRUE;
        }  /* if */
      }  /* if */
      if (depth_scope_stack != DEPTH_OF_FILE_SCOPE) {
        /* Local typedef names (legal if they refer to nonlocal types) should
           not be part of the type signature of the template class itself,
           which is nonlocal.  Strip them off, if there are any. */
        if (tap->is_type) {
          tap->variant.type = strip_local_typedefs(tap->variant.type);
        }  /* if */
      }  /* if */
    }  /* for */
    /* Record the argument list in the type.  It should be available in the
       IL at least for name generation and possibly for debuggers, too.  Note,
       however, that the type itself is not added to the scope types list
       until a full instantiation takes place -- or, if there is none, in
       pop_scope, as with ordinary classes. */
    class_type->variant.class_struct_union.extra_info->
                                            template_arg_list = *new_list;
    set_source_corresp(&(class_type->source_corresp), sym);
    /* All template instantiations have C++ external linkage, but mark it as
       internally linked for now.  The name linkage will be fixed up later,
       along with nontemplate classes.  This assures uniform processing of
       members. */
    class_type->source_corresp.name_linkage =
                                        (a_name_linkage_kind)nlk_internal;
    if (sym->variant.class_struct_union.extra_info->is_nonreal_class) {
      class_type->size = 1;
      class_type->alignment = 1;
    }  /* if */
#if DEBUG
    if (debug_level >= 3) {
      db_symbol(sym, "created: ", 2);
      db_symbol(class_template_sym, "template: ", 2);
    }  /* if */
#endif /* DEBUG */
  } else {
    /* We are reusing a class type that already exists, so *new_list will not
       be used.  Return the entries to the available list for reuse. */
    free_template_arg_list(*new_list);
  }  /* if */
  /* The list is cleared in all cases.  The caller cannot use the list
     after we return because it may have been freed. */
  *new_list = NULL;
  db_exit();
  return sym;
}  /* find_template_class */


static a_boolean matches_template_type_for_class_type
                                   (a_type_ptr         type,
                                    a_type_ptr         templ_type,
                                    a_template_arg_ptr *templ_arg_list)
/*
Called by matches_template_type to determine whether a given class type
matches a class type from the parameter list of a template function.
*/
{
  a_boolean			match = FALSE;
  a_class_symbol_supplement_ptr templ_cssp;
  /* Non-identical class types match if one represents a template
     and the other is an instantiation of that template.  For
     instance:
        template <class T> class A {  ...  };
        template <class TT> void f(A<TT>) {  ...  };
        f(A<int>);
     We reach this code when examining the argument in the call to f.
     "type" would refer to A<int> and "templ_type" would refer to
     A<TT>.  First we determine that A<int> and A<TT> refer to the same
     class template, and that the latter is a nonreal instantiation.
     Then we call matches_template_type on the template arg types. */
  templ_cssp = symbol_supplement_for_class(templ_type);
  if (templ_cssp->class_template != NULL &&
      symbol_supplement_for_class(type)->class_template ==
                                     templ_cssp->class_template &&
      templ_cssp->is_nonreal_class) {
    /* The two classes refer to the same template, but templ_type
       is a nonreal instantiation -- i.e., one based on template
       parameter types instead of real types. */
    a_template_arg_ptr  tap, templ_tap;
    tap = type->variant.class_struct_union.extra_info->
                                                   template_arg_list;
    templ_tap = templ_type->variant.class_struct_union.
                                       extra_info->template_arg_list;
    do {
      if (tap->is_type) {
        match = matches_template_type(tap->variant.type,
                                      templ_tap->variant.type,
                                      templ_arg_list,
                                      /*allow_conversion=*/FALSE,
                                      (a_base_class_ptr*)NULL);
      } else if (templ_tap->variant.constant->kind ==
                                  (a_constant_repr_kind)ck_template_param) {
        match = matches_template_type(
                                  tap->variant.constant->type,
                                  templ_tap->variant.constant->type,
                                  templ_arg_list,
                                  /*allow_conversion=*/FALSE,
                                  (a_base_class_ptr*)NULL);
      } else {
        match = eq_constants(tap->variant.constant,
                             templ_tap->variant.constant);
      }  /* if */
              tap = tap->next;
      templ_tap = templ_tap->next;
    } while (match && tap != NULL);
  }  /* if */
  return match;
}  /* matches_class_type_for_class_type */


a_boolean matches_template_type(a_type_ptr         type,
                                a_type_ptr         templ_type,
                                a_template_arg_ptr *templ_arg_list,
				a_boolean          allow_conversion,
                                a_base_class_ptr   *base_class_conv_needed)
/*
Compare type and templ_type.  The latter is from a parameter list of a
function template (function params, not template params).  If the types are
identical, return TRUE.  If they are identical but for a template parameter,
return TRUE if the type is consistent with other uses of that template
parameter, as represented in the template argument list.  Otherwise, return
FALSE.  When for the nth template parameter, the nth template arg has not
yet been created, extend the template argument list to include n entries.
allow_conversion specifies that a conversion from Derived<T> to Base<T>
may be done if needed.  The value pointed to by base_class_conv_needed
is set to point to the base class description if such a conversion
is required; otherwise it is set to NULL.  base_class_conv_needed may
be NULL if the caller does not need to know whether a conversion was performed.
*/
{
  a_boolean                      match = FALSE;
  a_type_ptr                     tp, ttp;
  a_param_type_ptr               ptp, tptp;
  unsigned long                  i;
  a_template_arg_ptr             tap, prev_tap;

  db_enter(5, "matches_template_type");
  if (base_class_conv_needed != NULL) *base_class_conv_needed = NULL;
  if (is_template_param_type(templ_type)) {
    if (is_qualified_type(templ_type)) {
      /* If the template parameter has any type qualifiers, the argument type
         will have to have a set of type qualifiers that includes any on the
         template parameter.  Remove any that are shared in common and then
         do a check. */
      skip_common_type_qualifiers(&type, &templ_type);
    }  /* if */
    if (is_qualified_type(templ_type)) {
      /* The qualifier on templ_type did not also appear on type, so there is
         no match. */
    } else {
      if (templ_type->variant.template_param.kind ==
                             (a_template_param_type_kind)tptk_param) {
        /* This is a template parameter from the original source program
           and not a synthesized template parameter. */
        /* A real type "matches" a template parameter type if it is identical
           to the real type, if any, that was previously associated with that
           template type. */
        /* For the nth template parameter find the nth template argument.  If
           the nth template argument hasn't been created yet, create it along
           with all missing template args that should precede it in the linked
           list. */
        prev_tap = NULL;
        for (i = templ_type->variant.template_param.list_position; i > 0; --i) {
          if (prev_tap == NULL) {
            /* This must be the first time through the loop. */
            tap = *templ_arg_list;
          } else {
            /* Not the first iteration. */
            tap = prev_tap->next;
          }  /* if */
          /* If the template arg doesn't exist yet, create it and add it to the
             list.  Note that some of the template args on the list will have
             NULL type pointers. */
          if (tap == NULL) {
            tap = alloc_template_arg(/*is_arg_type=*/TRUE);
            if (prev_tap == NULL) {
              /* First iteration -- the start of the list. */
              *templ_arg_list = tap;
            } else {
              /* Add to the end of the list. */
              prev_tap->next = tap;
            }  /* if */
          }  /* if */
          /* Remember the current entry so that next time though (if there is a
             next time) we can find its successor or, if necessary, append a
             new entry to it. */
          prev_tap = tap;
        }  /* for */
        /* Now we have the nth template argument, which should correspond to
           the nth template parameter, whose type is templ_type. */
        if (tap->variant.type == NULL) {
          /* No type has been bound to this template argument yet, so just use
             "type".  This counts as a match. */
          tap->variant.type = type;
          match = TRUE;
        } else {
          /* A type was already bound to this template argument.  We have a match
             if and only if the new type is the same as the one already there. */
          if (identical_types(type, tap->variant.type)) {
            /* Okay. */
            match = TRUE;
          } else {
            /* Not a match.  Return FALSE. */
          }  /* if */
        }  /* if */
      } else {
        /* This is a template parameter associated with a member of a
           proxy class (e.g., X in a type like T::X).  The members must have
           the same name (e.g., T::X matches A::X) and the parent classes
           must match. */
        /* Skip typedefs on the real type. */
        type = skip_typedefs(type);
        if (type->source_corresp.class_of_which_a_member != NULL) {
          tp = type->source_corresp.class_of_which_a_member;
          ttp = templ_type->source_corresp.class_of_which_a_member;
          if (ttp == NULL) {
            /* No parent class -- no match. */
          } else {
            a_symbol_ptr  sym, templ_sym;

            sym = (a_symbol_ptr)type->source_corresp.assoc_info;
            templ_sym = (a_symbol_ptr)templ_type->source_corresp.assoc_info;
            if (sym->header != templ_sym->header) {
              /* Members have different names -- no match. */
            } else {
              /* Convert the proxy class into its associated template
                 parameter and call matches_template_type on the parent
                 type. */
              a_class_symbol_supplement_ptr  cssp;
              cssp = symbol_supplement_for_class(ttp);
              ttp = cssp->template_param_for_proxy_class;
              if (matches_template_type(tp, ttp, templ_arg_list,
                                        /*allow_conversion=*/FALSE,
                                        (a_base_class_ptr*)NULL)) {
                /* Members have the same names and the parent classes
                   "match". */
                match = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
        } /* if */
      }  /* if */
    }  /* if */
  } else {
    /* The type from the template is not a template parameter type.  Before
       checking further, remove typedefs -- but keep the type qualifiers
       in place. */
    type = skip_typedefs(type);
    templ_type = skip_typedefs(templ_type);
    if (templ_type == type) {
      /* Identical type entries, so it's a match. */
      match = TRUE;
    } else if (templ_type->kind != type->kind) {
      /* No match. */
    } else if (type->source_corresp.class_of_which_a_member != NULL) {
      /* The argument type is a class member -- a nested class or enum.  Be
         sure the parent classes match and that the members correspond (i.e.,
         have the same name). */
      tp = type->source_corresp.class_of_which_a_member;
      ttp = templ_type->source_corresp.class_of_which_a_member;
      if (ttp == NULL) {
        /* No match. */
      } else {
        a_symbol_ptr  sym, templ_sym;

        sym = (a_symbol_ptr)type->source_corresp.assoc_info;
        templ_sym = (a_symbol_ptr)templ_type->source_corresp.assoc_info;
        if (sym->header != templ_sym->header) {
          /* Members have different names -- no match. */
        } else if (matches_template_type(tp, ttp, templ_arg_list,
                                         /*allow_conversion=*/FALSE,
                                         (a_base_class_ptr*)NULL)) {
          /* Members have the same names and the parent classes "match". */
          match = TRUE;
        }  /* if */
      }  /* if */
    } else {
      switch (type->kind) {
        case tk_class:
        case tk_struct:
        case tk_union:
          match = matches_template_type_for_class_type(type, templ_type,
                                                       templ_arg_list);
          if (!match && allow_conversion) {
            a_base_class_ptr	bcp;
            /* See if the type matches a base class type of actual argument
               type.  This is allows a Derived<T> to be passed to a function
               expecting a Base<T> as an argument. */
            bcp = type->variant.class_struct_union.extra_info->base_classes;
            while (bcp != NULL) {
              match = matches_template_type_for_class_type(bcp->type,
                                                           templ_type,
                                                           templ_arg_list);
              if (match) {
                if (base_class_conv_needed != NULL) {
                  *base_class_conv_needed = bcp;
                }  /* if */
                break;
              }  /* if */
              bcp = bcp->next;
            }  /* while */
          }  /* if */
          break;
        case tk_typeref:
          if (!type_qualifiers_match(type, templ_type)) {
            /* Not a match. */
          } else {
            /* Qualifiers match.  See if the underlying types do, too. */
            tp = type->variant.typeref.type;
            ttp = templ_type->variant.typeref.type;
            match = matches_template_type(tp, ttp, templ_arg_list,
                                          /*allow_conversion=*/FALSE,
                                          (a_base_class_ptr*)NULL);
          }  /* if */
          break;
        case tk_array:
          /* Array types match if their element types match and the number of
             elements is the same. */
          check_assertion(!type->variant.array.is_variable_size_array);
          check_assertion(!templ_type->variant.array.is_variable_size_array);
          if (type->variant.array.variant.number_of_elements !=
                        templ_type->variant.array.variant.number_of_elements) {
            /* Not a match. */
          } else {
            tp = type->variant.array.element_type;
            ttp = templ_type->variant.array.element_type;
            match = matches_template_type(tp, ttp, templ_arg_list,
                                          /*allow_conversion=*/FALSE,
                                          (a_base_class_ptr*)NULL);
          }  /* if */
          break;
        case tk_pointer:
          /* Pointer matches pointer and reference matches reference, but they
             can't be mixed. */
          if (type->variant.pointer.is_reference !=
                         templ_type->variant.pointer.is_reference) {
            /* Not a match. */
          } else {
            tp = type->variant.pointer.type;
            ttp = templ_type->variant.pointer.type;
            match = matches_template_type(tp, ttp, templ_arg_list,
                                          /*allow_conversion=*/FALSE,
                                          (a_base_class_ptr*)NULL);
          }  /* if */
          break;
        case tk_ptr_to_member:
          /* For ptr-to-member types, there needs to be a match on both the
             member types and the class-of-which-a-member. */
          tp = type->variant.ptr_to_member.type;
          ttp = templ_type->variant.ptr_to_member.type;
          if (matches_template_type(tp, ttp, templ_arg_list,
                                    /*allow_conversion=*/FALSE,
                                    (a_base_class_ptr*)NULL)) {
            tp = type->variant.ptr_to_member.class_of_which_a_member;
            ttp = templ_type->variant.ptr_to_member.class_of_which_a_member;
            match = (matches_template_type(tp, ttp, templ_arg_list,
                                           /*allow_conversion=*/FALSE,
                                           (a_base_class_ptr*)NULL));
          }  /* if */
          break;
        case tk_routine:
          /* For routine types there has to be a match both on the return types
             and on all the parameter types.  In addition, the has-ellipsis
             flags should be set the same. */
          tp = type->variant.routine.return_type;
          ttp = templ_type->variant.routine.return_type;
          if (matches_template_type(tp, ttp, templ_arg_list,
                                    /*allow_conversion=*/FALSE,
                                    (a_base_class_ptr*)NULL) &&
              (type->variant.routine.extra_info->has_ellipsis ==
                  templ_type->variant.routine.extra_info->has_ellipsis)) {
            /* Return type and ellipsis are okay.  Check the param types. */
            ptp = type->variant.routine.extra_info->param_type_list;
            tptp = templ_type->variant.routine.extra_info->param_type_list;
            for (;;) {
              if (ptp == NULL || tptp == NULL) {
                /* One or both of the param type lists is exhausted.  It's a
                   match only if they're both done. */
                match = (ptp == tptp);
                break;
              }  /* if */
              tp = ptp->type;
              ttp = tptp->type;
              if (!matches_template_type(tp, ttp, templ_arg_list,
                                         /*allow_conversion=*/FALSE,
                                         (a_base_class_ptr*)NULL)) {
                /* The first param type for which there is a mismatch causes
                   a mismatch for the entire type.  No need to keep looping. */
                break;
              }  /* if */
              ptp = ptp->next;
              tptp = tptp->next;
            }  /* for */
          }  /* if */
          break;
        default:
          /* They are simple types -- these are leaf nodes in a type tree.
             Check for identity. */
          match = identical_types(templ_type, type);
      }  /* switch */
    }  /* if */
  }  /* if */
  db_exit();
  return match;
}  /* matches_template_type */


#if CHECKING
static void check_function_template_arg_list(
                                    a_template_arg_ptr  templ_arg_list,
                                    a_symbol_ptr        templ_sym)
/*
Do some simple consistency checking on a function template argument list.
*/
{
  a_template_param_ptr  tpp;
  a_template_arg_ptr    tap;

  if (templ_sym->kind == (a_symbol_kind)sk_member_function) {
    tpp = templ_sym->variant.routine.instance_ptr->template_info->parameters;
  } else {
    tpp = templ_sym->variant.template_info->parameters;
  }  /* if */
  for (tap = templ_arg_list; tap != NULL; tap = tap->next) {
    if (!tap->is_type) {
      internal_error("check_template_arg_list: not a type arg");
    } else if (tap->variant.type == NULL) {
      internal_error("check_template_arg_list: missing type ptr");
    }  /* if */
    if (tpp == NULL) {
      internal_error("check_template_arg_list: too many template args");
    }  /* if */
    tpp = tpp->next;
  }  /* for */
  if (tpp != NULL) {
    internal_error("check_template_arg_list: too few template args");
  }  /* if */
}  /* check_template_arg_list */
#endif /* CHECKING */


void delayed_scan_for_function_template_default_args
			 (a_routine_ptr			    templ_rout,
			  a_routine_ptr			    rout_ptr,
			  a_template_symbol_supplement_ptr  tssp)
/*
Rescan the default arguments of a function template.
*/
{
  a_def_arg_expr_fixup_ptr	daefp;
  a_param_type_ptr		templ_ptp;
  a_param_type_ptr		ptp;
  a_type_ptr			templ_rout_type = templ_rout->type;
  a_type_ptr			rout_type = rout_ptr->type;

  daefp = tssp->variant.function.def_arg_expr_list;
  if (daefp != NULL) {
    templ_ptp = templ_rout_type->variant.routine.extra_info->param_type_list;
    ptp = rout_type->variant.routine.extra_info->param_type_list;
    /* The function prototype scope should be reactivated and its symbols
       reentered because parameter names hide names from enclosing scopes
       and, moreover, may not be used in default argument expressions
       (ARM 8.2.6). */
    (void)push_scope((a_scope_kind)sck_func_prototype,
                     tssp->declaration_scope, (a_type_ptr)NULL,
                     (a_routine_ptr)NULL, (a_symbol_ptr)NULL,
                     (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL);
    if (tssp->variant.function.func_info.prototype_scope_symbols != NULL) {
      reactivate_prototype_scope_symbols(
                    tssp->variant.function.func_info.prototype_scope_symbols);
    }  /* if */
    /* Loop through the two linked lists of param_type entries and the
       default argument expression fixup entries, and update the default
       arg expressions in the corresponding the param_type entries. */
    for (; ptp != NULL; ptp = ptp->next, templ_ptp = templ_ptp->next) {
      if (templ_ptp->has_default_arg) {
	check_assertion(daefp != NULL);
        /* Update the default argument expression entry to point to the
           current param type entry. */
	daefp->param_type = ptp;
        ptp->has_default_arg = TRUE;
        /* It's a default arg expression that needs to be rescanned. */
        /* Let get_token know about the cache. */
        rescan_reusable_cache(&daefp->token_cache);
        delayed_scan_of_default_arg_expr(daefp->param_type);
        daefp = daefp->next;
      }  /* if */
    }  /* for */
    check_assertion(daefp == NULL);
    /* Restore the prototype scope symbols pointer in the func_info
       block. It shouldn't have changed, but we do it to be safe. */
    tssp->variant.function.func_info.prototype_scope_symbols =
                                 scope_stack[depth_scope_stack].symbols;
    /* Pop the reactivated function prototype scope off the stack. */
    pop_scope();
  }  /* if */
}  /* delayed_scan_for_function_template_default_args */


void scan_template_declaration(a_boolean	 is_initial_decl,
			       a_decl_flag_set   *dso_flags,
			       a_decl_flag_set   *do_flags,
                               a_symbol_locator  *locator,
                               a_type_ptr        *type,
                               a_func_info_block *func_info,
			       a_storage_class   *storage_class)
/*
Calls decl_specifiers and declarator to scan a template declaration of
a function or static data member.  is_initial_decl is TRUE if this
is being called to scan the original declaration and is FALSE when
rescanning the tokens to generate a type for a specific instance
of a function template.
*/
{
  a_decl_flag_set	dsi_flags;
  a_decl_flag_set	di_flags;
  a_type_ptr            bottom_derived_type = NULL;
  dsi_flags = DSI_IS_TEMPLATE_DECLARATION |
              DSI_INLINE_ALLOWED |
              DSI_TYPE_SPECIFIER_ALLOWED |
              DSI_EMPTY_DECL_SPECIFIERS_ALLOWED |
              DSI_STORAGE_CLASS_SPECIFIER_ALLOWED;
  di_flags = DI_REAL_DECLARATOR_ALLOWED |
             DI_QUALIFIED_NAME_ALLOWED |
             DI_PARENTHESIZED_INITIALIZER_ALLOWED |
             DI_OPERATOR_NAME_ALLOWED;
  if (is_initial_decl) {
    dsi_flags |= DSI_IS_TEMPLATE_DECLARATION;
    di_flags |= DI_IS_TEMPLATE_DECLARATION;
    /* An end-of-source marker is not present when the initial declaration
       is scanned. */
    add_stop_token(tok_lbrace);
    add_stop_token(tok_colon);
    add_stop_token(tok_semicolon);
  } else {
    add_stop_token(tok_end_of_source);
  }  /* if */
  (void)decl_specifiers(dsi_flags, dso_flags, storage_class, type);
  if (is_error_type(*type) && !is_declarator_start()) {
    /* Error of some sort. */
    set_to_error_locator(*locator);
    *do_flags = 0;
  } else {
    declarator(di_flags, do_flags, *type, (a_type_ptr)NULL, locator, type,
               &bottom_derived_type, func_info);
    func_info->is_inline = ((*dso_flags & DSO_INLINE) != 0);
  }  /* if */
  if (is_initial_decl) {
    /* An end-of-source marker is not present when the initial declaration
       is scanned. */
    remove_stop_token(tok_lbrace);
    remove_stop_token(tok_colon);
    remove_stop_token(tok_semicolon);
  } else {
    remove_stop_token(tok_end_of_source);
    /* In the normal case the current token should be end_of_source,
       which was inserted to mark the end of the cached token stream.
       If necessary, keep flushing until end-of-source is found. */
    flush_past_token_cache_terminator();
  }  /* if */
}  /* scan_template_declaration */


a_symbol_ptr make_template_function(a_symbol_ptr        templ_sym,
                                    a_type_ptr          rout_type,
                                    a_template_arg_ptr  templ_arg_list,
                                    a_source_position   *source_pos)
/*
Allocate the symbol and routine entry for a template function, based on
the function template (represented by templ_sym) and the function type
(rout_type), and allocate and enter the associated function instantiation
entry, where the template arg list for the instantiation (templ_arg_list)
is also recorded.  If rout_type is NULL, create a routine type based on
the template argument list and the template parameter list (reached through
templ_sym).
*/
{
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp;
  a_memory_region_number            region_to_switch_back_to;
  a_template_instance_ptr           tip;
  a_routine_ptr                     templ_rout, rp;
  a_boolean			    is_new_rout_type = FALSE;

  db_enter(4, "make_template_function");
#if CHECKING
  check_function_template_arg_list(templ_arg_list, templ_sym);
#endif /* CHECKING */
  /* Allocate the template function symbol.  Note that it is not entered
     into the symbol table -- it will appear on a function instantiation
     list under the function template symbol and, optionally, in the overload
     list if it is also explicitly declared by the user. */
  sym = make_template_function_symbol(templ_sym, source_pos);
  if (templ_sym->kind == (a_symbol_kind)sk_member_function) {
#if 0
    tssp = templ_sym->variant.routine.instance_ptr->template_info;
#else /* 0 */
    unexpected_condition();
#endif /* if 0 */
  } else {
    tssp = templ_sym->variant.template_info;
  }  /* if */
  templ_rout = tssp->variant.function.routine;
  /* All IL routines must be at the file scope level, so switch to that
     memory region if necessary to allocate the routine entry. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  sym->variant.routine.ptr = rp = alloc_routine();
  if (rout_type == NULL) {
    /* If the routine type does not already exist, create one by
       rescanning the original declaration with the template parameters
       updated to refer to the appropriate template arguments. */
    a_decl_flag_set	do_flags;
    a_decl_flag_set	dso_flags;
    a_symbol_locator    locator;
    a_func_info_block	func_info;
    a_storage_class     storage_class;
    a_source_position   saved_pos_curr_token;
    a_source_position   saved_error_position;
    /* Push the template instantiation scope.  Note that the instance symbol
       passed to push_scope is NULL.  This is done because the type
       associated with the symbol is not yet complete (it has no routine
       type).  Using a partially constructed symbol could cause problems
       if errors occur while rescanning the declaration. */
    (void)push_scope((a_scope_kind)sck_template_instantiation,
                     tssp->declaration_scope, (a_type_ptr)NULL,
                     (a_routine_ptr)NULL, (a_symbol_ptr)NULL, templ_sym,
                     templ_arg_list);
    /* Rescan the tokens of the function declaration. */
    saved_pos_curr_token = pos_curr_token;
    saved_error_position = error_position;
    rescan_reusable_cache(&tssp->variant.function.decl_token_cache);
    scan_template_declaration(/*is_initial_decl=*/FALSE, &dso_flags, &do_flags,
                              &locator, &rout_type, &func_info,
                              &storage_class);
    error_position = saved_error_position;
    pos_curr_token = saved_pos_curr_token;
    /* Pop the template instantiation scope. */
    pop_scope();
    is_new_rout_type = TRUE;
  }  /* if */
  switch_back_to_original_region(region_to_switch_back_to);
  /* Give the routine entry the type passed in, and set other fields in
     accord with the settings in the template. */
  rp->type = rout_type;
  rp->storage_class = templ_rout->storage_class;
  rp->special_kind = templ_rout->special_kind;
  rp->opname_kind = templ_rout->opname_kind;
  rp->is_inline = templ_rout->is_inline;
  rp->is_template_function = TRUE;
  set_source_corresp(&rp->source_corresp, sym);
  rp->source_corresp.name_linkage = templ_rout->source_corresp.name_linkage;
  /* Add it to the file scope routines list. */
  add_to_routines_list(rp, /*at_file_scope=*/TRUE);
  /* Create the associated function instantiation entry and link it
     onto the front of the instantiation list for the template. */
  tip = alloc_template_instance();
  tip->template_sym = templ_sym;
  tip->arg_list = templ_arg_list;
  tip->next = tssp->variant.function.instantiations;
  tssp->variant.function.instantiations = tip;
  /* Make the function instantiation entry and its associated symbol
     point at each other. */
  tip->instance_sym = sym;
  sym->variant.routine.instance_ptr = tip;
  if (is_new_rout_type) {
    a_symbol_locator	locator;
    /* If there are default arguments whose types depend on template
       parameters, scan the default argument expressions. */
    if (tssp->variant.function.def_arg_expr_list != NULL) {
      /* Push the template instantiation scope. */
      (void)push_scope((a_scope_kind)sck_template_instantiation,
                       tssp->declaration_scope, (a_type_ptr)NULL, rp,
                       sym, tip->template_sym, tip->arg_list);
      delayed_scan_for_function_template_default_args
			(templ_rout, rp, tssp);
      /* Pop the template instantiation scope. */
      pop_scope();
    }  /* if */
    /* If this is a user-defined conversion or an overloaded operator,
       check for errors in the argument list.  The routine we are
       calling requires a locator.  Make a locator and fill in the
       information needed. */
    make_locator_for_symbol(sym, &locator);
    if (rp->special_kind == (a_special_function_kind)sfk_operator) {
      locator.is_operator_name = TRUE;
      locator.variant.opname = rp->opname_kind;
    } else if (rp->special_kind == (a_special_function_kind)sfk_conversion) {
      locator.is_conversion_name = TRUE;
      locator.variant.conversion_result_type = NULL;
    }  /* if */
    check_operator_function_params(rout_type, /*class_type=*/(a_type_ptr)NULL,
                                   &locator);
  }  /* if */
  /* Function instantiation entries are not marked for actual instantiation
     (that is, for generation of the function body) until there is an
     invocation of the function.  In tim_all mode the instantiations
     will be generated even if the instantiation required flag is not
     set. */
  if (!tip->instantiation_required) {
    /* If the flag is set then the entry is already on the list and the flag
       should not be reset. */
    update_instantiation_required_flag(tip, /*value=*/FALSE);
  }  /* if */
  db_exit();
  return sym;
}  /* make_template_function */


a_boolean is_match_for_function_template(a_symbol_ptr       templ_sym,
                                         a_type_ptr         curr_type,
                                         a_template_arg_ptr *templ_arg_list,
                                         a_symbol_ptr       *instance_sym)
/*
Search for a template function based on the function template represented
by templ_sym and the type pointed to by curr_type.  If such a template
function exists, return its symbol.  Otherwise, try to generate a template
arg list to serve as the basis for creating one.  If either a symbol can
be found or a template arg list can be created, return TRUE; otherwise,
return FALSE.
*/
{
  a_boolean                         match = FALSE;
  a_symbol_ptr                      sym = NULL;
  a_type_ptr                        rout_type, templ_rout_type;
  a_template_symbol_supplement_ptr  tssp;
  a_template_instance_ptr           tip;
  a_param_type_ptr                  ptp, other_ptp;

  db_enter(3, "is_match_for_function_template");
#if CHECKING
  if (!is_function_type(curr_type)) {
    internal_error("is_match_for_template_function: expected routine type");
  }  /* if */
#endif /* CHECKING */
  *templ_arg_list = NULL;
  *instance_sym = NULL;
  /* sym is the symbol for a template function to be returned.  Returning NULL
     means no template function could be found or created. */
  if (templ_sym->kind == (a_symbol_kind)sk_member_function) {
    tssp = templ_sym->variant.routine.instance_ptr->template_info;
  } else {
    tssp = templ_sym->variant.template_info;
  }  /* if */
  tssp = templ_sym->variant.template_info;
  templ_rout_type = tssp->variant.function.routine->type;
  /* First be sure the number of parameters in the template function is
     equal to the number in param_type_list. */
  ptp = curr_type->variant.routine.extra_info->param_type_list;
  other_ptp = templ_rout_type->variant.routine.extra_info->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    if (other_ptp == NULL) {
      /* Too many params to match this template. */
      goto done;
    }  /* if */
    other_ptp = other_ptp->next;
  }  /* if */
  if (other_ptp != NULL) {
    /* Too many args in function template (and therefore in each of its
       instantiations) to justify looking any further. */
    goto done;
  }  /* if */
  if (curr_type->variant.routine.extra_info->has_ellipsis != 
      templ_rout_type->variant.routine.extra_info->has_ellipsis) {
    /* One routine has an ellipsis argument and the other does not.
       This cannot be a match. */
    goto done;
  }  /* if */
  /* Make a pass over the entries representing instantiations of the function
     template to see if any of them match the current type signature. */
  for (tip = tssp->variant.function.instantiations;
       tip != NULL;
       tip = tip->next) {
    /* We used to skip entries that represent specific declarations.
       This is no longer done because these entries must be examined this
       routine is called during instantiation pragma processing. */
    sym = tip->instance_sym;
    rout_type = sym->variant.routine.ptr->type;
    /* Return type must match exactly. */
    if (!identical_types(curr_type->variant.routine.return_type,
                         rout_type->variant.routine.return_type)) {
      /* No match.  Advance to the next template function symbol. */
      goto get_next_sym;
    }  /* if */
    /* Each parameter type must match exactly. */
    ptp = curr_type->variant.routine.extra_info->param_type_list;
    other_ptp = rout_type->variant.routine.extra_info->param_type_list;
    for (; ptp != NULL; ptp = ptp->next) {
      if (!identical_types(ptp->type, other_ptp->type)) {
        /* No match.  Advance to the next template function symbol. */
        goto get_next_sym;
      }  /* if */
      other_ptp = other_ptp->next;      
    }  /* for */
    /* Falling through to here means curr_type exactly matches the function
       type for sym.  Skip over the remaining processing and return sym to
       the caller. */
    match = TRUE;
    *instance_sym = sym;
    goto done;
get_next_sym:;
    /* No match so far.  Continue looping through the function instantiation
       entries. */
  }  /* for */
  /* Falling through to here means the type signature passed in does not
     match any existing template function based on the function template in
     question, but that it is not disqualified on other grounds.  Try to match
     the type signature to the template's type signature.  If successful, a
     template arg list is returned; otherwise, NULL is returned. */
  if (!matches_template_type(curr_type->variant.routine.return_type,
                             templ_rout_type->variant.routine.return_type,
                             templ_arg_list, /*allow_conversion=*/FALSE,
                             (a_base_class_ptr*)NULL)) {
    goto done;
  } else {
    /* The routine type for curr_type can be accommodated to the template
       return type.  Now check each of the parameters. */
    ptp = curr_type->variant.routine.extra_info->param_type_list;
    other_ptp = templ_rout_type->variant.routine.extra_info->param_type_list;
    for (; other_ptp != NULL; other_ptp = other_ptp->next) {
      if (!matches_template_type(ptp->type, other_ptp->type,
                                 templ_arg_list, /*allow_conversion=*/FALSE,
                                 (a_base_class_ptr*)NULL)) {
        goto done;
      }  /* if */
      ptp = ptp->next;
    }  /* for */
    match = TRUE;
  }  /* if */
done:
  if (!match) {
    if (*templ_arg_list != NULL) {
      free_template_arg_list(*templ_arg_list);
      *templ_arg_list = NULL;
    }  /* if */
#if CHECKING
  } else if ((*instance_sym == NULL) == (*templ_arg_list == NULL)) {
    internal_error(
              "is_match_for_function_template: bad sym or templ arg list");
#endif /* CHECKING */
  }  /* if */
  db_exit();
  return match;
}  /* is_match_for_function_template */


a_symbol_ptr matching_template_function(a_symbol_ptr        templ_sym,
                                        a_type_ptr          curr_type,
                                        a_source_position   *source_pos)
/*
Search for a template function based on the function template represented
by templ_sym and the type pointed to by curr_type.  If no such template
function exists, try to create one.  If the search/creation is successful
return a pointer to the symbol; otherwise, return NULL.
*/
{
  a_symbol_ptr          sym;
  a_template_arg_ptr    templ_arg_list;

  db_enter(3, "matching_template_function");
#if CHECKING
  if (!is_function_type(curr_type)) {
    internal_error("matching_template_function: expected routine type");
  }  /* if */
#endif /* CHECKING */
  curr_type = skip_typerefs(curr_type);
  if (is_match_for_function_template(templ_sym, curr_type,
                                     &templ_arg_list, &sym)) {
    if (sym != NULL) {
      /* A match has been found -- just return a pointer to it. */
    } else {
      /* Use the template arg list to create a new symbol. */
      sym = make_template_function(templ_sym, curr_type, templ_arg_list,
                                   source_pos);
    }  /* if */
  }  /* if */
  db_exit();
  return sym;
}  /* matching_template_function */


void record_predeclared_template_function(a_symbol_ptr  templ_sym,
                                          a_symbol_ptr  rout_sym)
/*
rout_sym represents a routine that has already been declared, and templ_sym
represents a function template of the same name.  It may be that rout_sym
is a "predeclared" instance of templ_sym, as in the following example:
  void f(int i) { ... }
  template <class T> void f(T t) { ... }
If so, we want to treat the first f as an instance of the template f.  This
means including a reference to it on the list of function instantiation
entries bound to the template f as well as devising a template argument list
for it.  Check for such a case, and when it occurs create and initialize
the function instantiation entry and set all the pointers.
*/
{
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp = NULL;
  a_template_instance_ptr           tip;
  a_type_ptr                        tp;
  a_template_arg_ptr                templ_arg_list;

  db_enter(3, "record_predeclared_template_function");
  tip = rout_sym->variant.routine.instance_ptr;
  if (tip != NULL) {
    /* Symbol is already marked as an instantiation. */
    if (tip->template_sym != templ_sym) {
      /* But it's an instance of some other template -- ignore it. */
    } else {
      tssp = templ_sym->variant.template_info;
    }  /* if */
  } else {
    tp = skip_typerefs(rout_sym->variant.routine.ptr->type);
    if (is_match_for_function_template(templ_sym, tp, &templ_arg_list, &sym)) {
      /* A match has been found. */
#if CHECKING
#if 0
      /* This situation might come up in an error case.  We'll figure out what
         to do about it if it ever happens. */
#endif /* if 0 */
      if (sym != NULL) {
        internal_error("record_predeclared_template_function: sym found");
      }  /* if */
      if (templ_sym->kind != (a_symbol_kind)sk_function_template) {
        internal_error("record_predeclared_template_function: bad sym kind");
      }  /* if */
#endif /* CHECKING */
      /* Create the associated function instantiation entry and link it
         onto the front of the instantiation list for the template. */
      tip = alloc_template_instance();
      tip->template_sym = templ_sym;
      tip->arg_list = templ_arg_list;
      /* Mark this function as a "specialization". */
      tip->specific_decl = TRUE;
      tssp = templ_sym->variant.template_info;
      tip->next = tssp->variant.function.instantiations;
      tssp->variant.function.instantiations = tip;
      /* Make the function instantiation entry and its associated symbol
         point at each other. */
      tip->instance_sym = rout_sym;
      rout_sym->variant.routine.instance_ptr = tip;
      rout_sym->variant.routine.ptr->is_template_function = TRUE;
    }  /* if */
  }  /* if */
  if (tssp != NULL) {
    if (rout_sym->defined) {
      /* User-defined, so no instantiation is required. */
      tip->specific_def = TRUE;
      rout_sym->variant.routine.ptr->specific_def = TRUE;
    } else {
      /* Not defined by the user, so still a candidate for instantiation
         based on the template. */
      a_routine_ptr  rp = rout_sym->variant.routine.ptr;
      a_routine_ptr  templ_rp = tssp->variant.function.routine;
      if (templ_rp->storage_class == (a_storage_class)sc_static) {
        if (rp->storage_class != (a_storage_class)sc_static) {
          sym_warning(ec_template_and_instance_linkage_conflict, rout_sym);
          rp->storage_class = (a_storage_class)sc_static;
          rp->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
        }  /* if */
        if (templ_rp->is_inline) {
          if (rp->called) {
            sym_error(ec_called_function_redeclared_inline, rout_sym);
          }  /* if */
          rp->is_inline = TRUE;
        }  /* if */
      } else {
        if (rp->storage_class == (a_storage_class)sc_static) {
          sym_warning(ec_template_and_instance_linkage_conflict, rout_sym);
          rp->storage_class = (a_storage_class)sc_unspecified;
          rp->source_corresp.name_linkage =
                                (a_name_linkage_kind)nlk_cplusplus_external;
          rp->is_inline = FALSE;
        }  /* if */
      }  /* if */
      /* Function instantiation entries are not marked for actual instantiation
         (that is, for generation of the function body) until there is an
         invocation of the function.  In tim_all mode the instantiations
         will be generated even if the instantiation required flag is not
         set. */
      if (!tip->instantiation_required) {
        /* If the flag is set then the entry is already on the list and
           the flag should not be reset. */
        update_instantiation_required_flag(tip, /*value=*/FALSE);
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* record_predeclared_template_function */


void find_member_function_template(a_symbol_ptr  rout_sym,
                                   a_symbol_ptr  corresp_prototype_tag_sym)
/*
rout_sym is a member function of a template class.  corresp_prototype_tag_sym
is a symbol representing the corresponding prototype instantiation.  (For
instance, if rout_sym is a member of A<int>, the corresponding prototype
instantiation is A<T>; if rout_sym is a member of A<int>::B, the corresponding
prototype instantiation is A<T>::B.)  Find the function template symbol that
is a member of the corresponding prototype instantiation and that corresponds
to rout_sym (if the name is overloaded, use the source position to decide),
and create a function instantiation entry to bind the two symbols together.
*/
{
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp;
  a_template_instance_ptr           tip;
  a_type_ptr                        tp;
  a_scope_number                    corresp_prototype_decl_scope;

  db_enter(3, "find_member_function_template");
  /* Find a function symbol on the inactive list that is in the scope of the
     prototype instantiation.  It should either be a function template or
     overloaded function symbol. */
  if (is_constructor_symbol(rout_sym)) {
    sym = corresp_prototype_tag_sym->
                         variant.class_struct_union.extra_info->constructor;
  } else if (rout_sym->variant.routine.ptr->special_kind ==
                                    (a_special_function_kind)sfk_conversion) {
    /* Look through the conversion routines of the prototype instantiation. */
    a_conversion_list_entry_ptr   clep;

    sym = NULL;
    for (clep = corresp_prototype_tag_sym->
                      variant.class_struct_union.extra_info->conversion_list;
         clep != NULL;
         clep = clep->next) {
      if (clep->symbol->decl_position.seq == rout_sym->decl_position.seq &&
          clep->symbol->decl_position.column ==
                                             rout_sym->decl_position.column) {
        /* clep->symbol is the template function symbol for rout_sym. */
#if 0
        /* Eventually we need a more reliable technique than relying on
           declaration position. */
#endif /* if 0 */
        sym = clep->symbol;
        break;
      }  /* if */
    }  /* for */
  } else {
    /* Get the scope in which the members of the class represented by
       corresp_prototype_tag_sym were declared. */
    tp = type_symbol_type(corresp_prototype_tag_sym);
    corresp_prototype_decl_scope =
               tp->variant.class_struct_union.extra_info->assoc_scope->number;
    for (sym = rout_sym->header->inactive_symbols;
         sym != NULL;
         sym = sym->next) {
      if (sym->decl_scope == corresp_prototype_decl_scope &&
          (sym->kind == (a_symbol_kind)sk_member_function ||
           sym->kind == (a_symbol_kind)sk_overloaded_function)) {
        break;
      }  /* if */
    }  /* for */
  }  /* if */
#if CHECKING
  if (sym != NULL)
#endif /* CHECKING */
  if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    /* An overloaded function was found.  Go through the symbols on its list
       and find the function template symbol that corresponds to rout_sym.
       The easiest way is just to compare source positions. */
    for (sym = sym->variant.overloaded_function.symbols;
         sym != NULL;
         sym = sym->next) {
      if (sym->kind == (a_symbol_kind)sk_member_function &&
          sym->decl_position.seq == rout_sym->decl_position.seq &&
          sym->decl_position.column == rout_sym->decl_position.column) {
        /* sym is the template function symbol for rout_sym. */
#if 0
        /* Eventually we need a more reliable technique than relying on
           declaration position. */
#endif /* if 0 */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
#if CHECKING
  if (sym == NULL || sym->kind != (a_symbol_kind)sk_member_function ||
      sym->decl_position.seq != rout_sym->decl_position.seq ||
      sym->decl_position.column != rout_sym->decl_position.column) {
    internal_error("find_member_function_template: no corresponding template");
  }  /* if */
#endif /* CHECKING */
  /* sym is the template symbol for which member function rout_sym is an
     instantiation.  Create the function instantiation entry and set the
     pointers to bind them together. */
  tip = alloc_template_instance();
  tip->template_sym = sym;
  /* Get the template arg list for the class and use it.  Note that if
     this is a nested class we have to climb the parent chain to find the
     template class in which the template arg list is recorded. */
  tp = rout_sym->class_of_which_a_member;
  while (tp->source_corresp.class_of_which_a_member != NULL) {
    tp = tp->source_corresp.class_of_which_a_member;
  }  /* if */
  tip->arg_list =
             tp->variant.class_struct_union.extra_info->template_arg_list;
  tssp = sym->variant.routine.instance_ptr->template_info;
  /* Link the new entry to the start of the instantiation list of the
     function template. */
  tip->next = tssp->variant.function.instantiations;
  tssp->variant.function.instantiations = tip;
  /* Make the function instantiation entry and its associated symbol
     point at each other. */
  tip->instance_sym = rout_sym;
  rout_sym->variant.routine.instance_ptr = tip;
  /* Mark the routine entry as an instance of a member function template. */
  rout_sym->variant.routine.ptr->is_template_function = TRUE;
  /* Be sure the is_line flag is transferred to the new routine entry. */
  if (sym->variant.routine.ptr->is_inline) {
    rout_sym->variant.routine.ptr->is_inline = TRUE;
  }  /* if */

  db_exit();
}  /* find_member_function_template */


void find_static_data_member_template(a_symbol_ptr  static_data_member_sym,
                                      a_symbol_ptr  corresp_prototype_tag_sym)
/*
static_data_member_sym is a symbol representing a static data member of a
real instantiation of a class template.  corresp_prototype_tag_sym identifies
the nonreal prototype instantiation of the same class template.  Find the
sk_static_data_member symbol from the prototype instantiation (it serves as
the template for the real static data member), and record it in the
template instance entry already associated with static_data_member_sym.
Also, add the instance to the definitions list for the template.
*/
{
  a_template_instance_ptr           tip;
  a_scope_number                    corresp_prototype_decl_scope;
  a_type_ptr                        tp, member_type;
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp;
  a_variable_ptr                    vp;

  db_enter(3, "find_static_data_member_template");
  /* Find a static data member symbol belonging to the prototype instantiation
     and corresponding to static_data_member_sym. */
  tp = type_symbol_type(corresp_prototype_tag_sym);
  member_type = static_data_member_sym->
                        variant.static_data_member.variable->type;
  if (member_type->kind == (a_type_kind)tk_union &&
      is_unnamed_class_symbol(
                  (a_symbol_ptr)member_type->source_corresp.assoc_info)) {
    /* Error case -- the static data member is an anonymous union.  Look
       through the variables list of the prototype instantiation type. */
    vp = tp->variant.class_struct_union.extra_info->assoc_scope->variables;
    sym = NULL;
    for (; vp != NULL; vp = vp->next) {
      sym = (a_symbol_ptr)vp->source_corresp.assoc_info;
      if (sym != NULL) {
        tp = sym->variant.static_data_member.variable->type;
        if (tp->kind == (a_type_kind)tk_union &&
            is_unnamed_class_symbol(
                        (a_symbol_ptr)tp->source_corresp.assoc_info) &&
            sym->decl_position.column ==
                       static_data_member_sym->decl_position.column &&
            sym->decl_position.seq ==
                       static_data_member_sym->decl_position.seq) {
#if 0
          /* Eventually we need a more reliable technique than relying on
             declaration position. */
#endif /* if 0 */
          break;
        } else {
          sym = NULL;
        }  /* if */
      }  /* if */
    }  /* for */
  } else {
    /* Normal case -- do the lookup by name. */
    /* Get the scope in which the members of the class represented by
       corresp_prototype_tag_sym were declared. */
    corresp_prototype_decl_scope =
               tp->variant.class_struct_union.extra_info->assoc_scope->number;
    for (sym = static_data_member_sym->header->inactive_symbols;
         sym != NULL;
         sym = sym->next) {
      if (sym->decl_scope == corresp_prototype_decl_scope &&
          sym->kind == (a_symbol_kind)sk_static_data_member &&
          sym->variant.static_data_member.instance_ptr != NULL) {
        break;
      }  /* if */
    }  /* for */
  }  /* if */
#if CHECKING
  if (sym == NULL) {
    internal_error(
               "find_static_data_member_template: no corresponding template");
  }  /* if */
#endif /* CHECKING */
  /* sym is the template symbol with which static_data_member_sym is
     associated.  Create a static data member def entry and set the pointers
     to bind them all together. */
  tip = static_data_member_sym->variant.static_data_member.instance_ptr;
  tip->template_sym = sym;
  /* Get the template arg list for the class and use it.  Note that if
     this is a nested class we have to climb the parent chain to find the
     template class in which the template arg list is recorded. */
  tp = static_data_member_sym->class_of_which_a_member;
  while (tp->source_corresp.class_of_which_a_member != NULL) {
    tp = tp->source_corresp.class_of_which_a_member;
  }  /* if */
  tip->arg_list =
             tp->variant.class_struct_union.extra_info->template_arg_list;
  /* Link the new entry to the start of the definition list of the static
     data member template. */
  tssp = sym->variant.static_data_member.instance_ptr->template_info;
  tip->next = tssp->variant.static_data_member.definitions;
  tssp->variant.static_data_member.definitions = tip;
  /* Mark the variable entry as an instance of a static data member
     template. */
  static_data_member_sym->variant.static_data_member.variable->
                                      is_template_static_data_member = TRUE;

  db_exit();
}  /* find_static_data_member_template */


a_symbol_ptr find_template_function(a_symbol_ptr        templ_sym,
                                    a_template_arg_ptr  *new_list,
                                    a_source_position   *source_pos)
/*
templ_sym is a pointer to a symbol representing a function template and
*new_list is a pointer to a linked list of template arg entries.  If
*new_list is equivalent to the template arg list of a previous instantiation,
return the symbol representing the latter and put the entries on *new_list
back onto the available list.  Otherwise, create a new function instantiation
entry, symbol, routine entry, etc., and return the new symbol; in that
case *new_list is not disposed of but rather used in the resulting data
structure.
*/
{
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp;
  a_template_instance_ptr           tip, prev_tip;
  a_template_arg_ptr		    tap = *new_list;

  db_enter(3, "find_template_function");
  /* Make a pass over the entries representing instantiations of the function
     template. */
  if (templ_sym->kind == (a_symbol_kind)sk_member_function) {
    tssp = templ_sym->variant.routine.instance_ptr->template_info;
  } else {
    tssp = templ_sym->variant.template_info;
  }  /* if */
  /* Check for invalid type arguments.  Local types may not be used as
     arguments nor may unnamed types.  Issue an error if any are found. */
  while (tap != NULL) {
    a_type_ptr	type = tap->variant.type;
    a_boolean	is_unnamed;
    a_boolean	is_local;
    if (is_or_contains_unnamed_or_local_type(type, &is_unnamed, &is_local)) {
      if (is_local) {
        pos_error(ec_local_type_in_template_arg, source_pos);
      } else if (is_unnamed) {
        pos_error(ec_unnamed_type_in_template_arg, source_pos);
      }  /* if */
    }  /* if */
    tap = tap->next;
  }  /* while */
  tip = tssp->variant.function.instantiations;
  prev_tip = NULL;
  for (; tip != NULL; tip = tip->next) {
    if (equiv_template_arg_lists(tip->arg_list, *new_list,
                                 /*is_func_template=*/TRUE)) {
      /* We've found a match.  Remove the found function instantiation entry
         from its current position in the instantiation list and add it to
         the front. */
      if (prev_tip != NULL) {
        prev_tip->next = tip->next;
        tip->next = tssp->variant.function.instantiations;
        tssp->variant.function.instantiations = tip;
      }
      sym = tip->instance_sym;
#if DEBUG
      if (debug_level >= 3) db_symbol(sym, "found: ", 2);
#endif /* DEBUG */
      break;
    }  /* if */
    prev_tip = tip;
  }  /* for */
  if (tip == NULL) {
    /* No match was found, so create a new template function.  That means
       create a symbol entry, a routine entry, a routine type entry, and a
       function instantiation entry, and linking all these appropriately.
       Note that the symbol will not be added to the symbol table, since it
       is accessed through the list of function instantiation entries. */
    sym = make_template_function(templ_sym, (a_type_ptr)NULL, *new_list,
                                 source_pos);
#if DEBUG
    if (debug_level >= 3) {
      db_symbol(sym, "created: ", 2);
      db_symbol(templ_sym, "template: ", 2);
    }  /* if */
#endif /* DEBUG */
  } else {
    /* We are reusing a template function that already exists, so *new_list
       will not be used.  Return it to the available list for reuse. */
    free_template_arg_list(*new_list);
  }  /* if */
  /* The list is cleared in all cases.  The caller cannot use the list
     after we return because it may have been freed. */
  *new_list = NULL;
  db_exit();
  return sym;
}  /* find_template_function */


static a_boolean reconcile_template_param_lists
					(a_template_param_ptr param_list,
                                         a_symbol_ptr         class_sym,
					 a_source_position    *error_pos)
/*
Compare the template parameter list of the template declaration currently
being scanned with the template parameter list of a previous declaration
of the same class.  Make sure that the parameter lists match and
merge the default argument information from the two lists.  The default
argument information is updated into both lists because we don't know
which version will be used as the "primary" argument list.  This routine
is called for each redeclaration of a template argument list for a class.
For example, this routine will be called for all of these declarations
except for the first one:

	template <class T, int I> class A;
	template <class T, int I> class A { ... };
	template <class T, int I> void A<T,I>::f() { ... };
	template <class T, int I> int A<T,I>::i =  ... ;

Return TRUE if the parameter lists are compatible.  Otherwise, return FALSE.
*/
{
  a_template_param_ptr	new_tpp;
  a_template_param_ptr	old_tpp;
  a_template_param_ptr	prev_new_tpp = NULL;
  a_boolean		any_errors = FALSE;

  new_tpp = param_list;
  old_tpp = class_sym->variant.template_info->parameters;
  while (new_tpp != NULL && old_tpp != NULL) {
    a_symbol_ptr	old_sym = old_tpp->param_symbol;
    a_symbol_ptr	new_sym = new_tpp->param_symbol;
    a_boolean		err = FALSE;
    if (old_sym->kind != new_sym->kind) {
      /* One argument is a type and the other is a constant -- this is an
         error. */
      err = TRUE;
    } else if (old_sym->kind == (a_symbol_kind)sk_type) {
      /* Both are types.  Make sure the types match. */
      a_template_param_type_descr_ptr tptdp;
      a_type_ptr        old_type = old_tpp->variant.param_type;
      a_type_ptr        new_type = new_tpp->variant.param_type;
      err = !identical_types(old_type, new_type);
      /* If one does not already exist, create a template parameter
         type description record that is pointed to by both types.  A
         existing description may be associated with either the old or new
         type. */
      tptdp = old_type->variant.template_param.descr;
      if (tptdp == NULL) {
        tptdp = new_type->variant.template_param.descr;
        if (tptdp == NULL) {
          tptdp = alloc_template_param_type_descr();
        }  /* if */
      }  /* if */
      /* Update both type entries to point to the same description entry. */
      old_type->variant.template_param.descr = tptdp;
      new_type->variant.template_param.descr = tptdp;
    } else {
      /* Both are constants.  Make sure the values are the same. */
      check_assertion(old_sym->kind == (a_symbol_kind)sk_constant);
      err = !eq_constants(old_tpp->variant.param_constant.ptr,
                          new_tpp->variant.param_constant.ptr);
    }  /* if */
    if (err) {
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &new_sym->decl_position, old_sym);
      any_errors = TRUE;
    }  /* if */
    old_tpp = old_tpp->next;
    prev_new_tpp = new_tpp;
    new_tpp = new_tpp->next;
  }  /* while */
  if (old_tpp != NULL || new_tpp != NULL) {
    /* The number of template parameters does not match the previous
       declaration. */
    an_error_code	error_code;
    a_source_position	*pos;
    if (old_tpp == NULL) {
      /* Too many parameters.  Use the position of the first extra
         parameter as the error position. */
      error_code = ec_too_many_template_params;
      pos = &new_tpp->param_symbol->decl_position;
    } else {
      /* Too few parameters.  Use the position of the last parameter present
         to report the error.  If there were no parameters specified,
         use the position supplied by the caller, which will point to
         the thing being declared. */
      error_code = ec_too_few_template_params;
      pos = prev_new_tpp == NULL ? error_pos :
                                   &prev_new_tpp->param_symbol->decl_position;
    }  /* if */
    pos_error(error_code, pos);
    any_errors = TRUE;
  }  /* if */
  /* Merge the default argument information from the two parameter lists.
     This is only done if there were no errors in the previous tests so
     we know that the parameter lists match. */
  if (!any_errors) {
    new_tpp = param_list;
    old_tpp = class_sym->variant.template_info->parameters;
    while (new_tpp != NULL && old_tpp != NULL) {
      if (new_tpp->param_symbol->kind == (a_symbol_kind)sk_constant) {
        /* Only constant parameters have default arguments. */
        a_boolean type_involves_template_param;
        a_boolean old_has_default;
        a_boolean new_has_default;
        old_has_default = old_tpp->variant.param_constant.has_default_arg;
        new_has_default = new_tpp->variant.param_constant.has_default_arg;
        if (old_has_default && new_has_default) {
          /* This parameter already has a default argument. */
          pos_error(ec_default_arg_already_defined, &
                    new_tpp->param_symbol->decl_position);
        } else if (old_has_default || new_has_default) {
          a_template_param_ptr	from_tpp;
          a_template_param_ptr	to_tpp;
          /* Copy the default information into the other parameter.  We
             end up with two argument lists with complete parameter
             information. This is done because we don't know which parameter
             list is going to end up being the one actually used. */
          if (old_has_default) {
            from_tpp = old_tpp;
            to_tpp = new_tpp;
          } else {
            from_tpp = new_tpp;
            to_tpp = old_tpp;
          }  /* if */
          to_tpp->variant.param_constant.has_default_arg = TRUE;
          type_involves_template_param =
                from_tpp->variant.param_constant.type_involves_template_param;
          to_tpp->variant.param_constant.type_involves_template_param =
                                                 type_involves_template_param;
          if (type_involves_template_param) {
            to_tpp->variant.param_constant.default_arg.token_cache =
                     from_tpp->variant.param_constant.default_arg.token_cache;
          } else {
            to_tpp->variant.param_constant.default_arg.constant = 
                        from_tpp->variant.param_constant.default_arg.constant;
          }  /* if */
        }  /* if */
      }  /* if */
      old_tpp = old_tpp->next;
      new_tpp = new_tpp->next;
    }  /* while */
  }  /* if */
  return !any_errors;
}  /* reconcile_template_param_lists */


static a_boolean member_template_param_list_matches_class
					(a_template_param_ptr param_list,
                                         a_symbol_ptr         member_sym,
					 a_source_position    *error_pos)
/*
This routine is called for template declarations of member functions
static data members of class templates.  It calls
reconcile_template_param_lists to compare the template parameters of this
declaration with the parameter list of the class declaration.
Return TRUE if the parameter lists are compatible.  Otherwise, return FALSE.
*/
{
  a_symbol_ptr	class_sym;
  a_boolean	result;
  a_type_ptr    type;
  a_type_ptr	cowam_type;

  /* Find the type of the class.  If this class is nested in another class
     find the type of the outermost class. */
  type = member_sym->class_of_which_a_member;
  while ((cowam_type = type->source_corresp.class_of_which_a_member) != NULL) {
    type = cowam_type;
  }  /* while */
  /* Get the symbol associated with the type.  This symbol is the
     template class symbol. */
  class_sym = (a_symbol_ptr)type->source_corresp.assoc_info;
  /* Get a pointer to the symbol for the class template. */
  class_sym = class_sym->variant.class_struct_union.extra_info->class_template;
  result = reconcile_template_param_lists(param_list, class_sym, error_pos);
  return result;
}  /* member_template_param_list_matches_class */


static void check_template_param_default_args(a_template_param_ptr param_list)
/*
Make sure that any default arguments are at the end of the parameter list.
*/
{
  a_template_param_ptr	tpp;
  a_boolean		any_defaults = FALSE;

  tpp = param_list;
  while (tpp != NULL) {
    a_boolean has_default = FALSE;
    /* Does this parameter have a default argument?  Only constant parameters
       may have default arguments. */
    if (tpp->param_symbol->kind == (a_symbol_kind)sk_constant) {
       has_default = tpp->variant.param_constant.has_default_arg;
       any_defaults |= has_default;
    }  /* if */
    /* If there have been parameters with default and this one doesn't have
       a default then issue an error and exit the loop. */
    if (any_defaults && !has_default) {
      pos_error(ec_default_arg_not_at_end, &tpp->param_symbol->decl_position);
    }  /* if */
    tpp = tpp->next;
  }  /* while */
 }  /* check_template_param_default_args */


static a_boolean class_template_declaration(
                                    a_template_param_ptr templ_params,
                                    a_symbol_ptr         *p_sym_ptr,
                                    a_boolean            *resolution,
                                    a_type_ptr           *new_type,
                                    a_boolean            *defines_something)
/*
If this turns out to be a class template declaration, scan it and return
TRUE, setting *p_sym_ptr to the class template symbol.  If it is not a class
declaration, return FALSE.  If a class template had been declared previously
but not defined, and this is a defining declaration, return *resolution
TRUE.  In addition, if this is a defining declaration, cache all the tokens
that make up the declaration and do a prototype instantiation.
*/
{
  a_boolean                         is_class_template_decl = FALSE;
  a_boolean                         suppress_redecl_error = FALSE;
  a_boolean                         is_definition, is_redecl;
  a_symbol_locator                  locator;
  a_symbol_ptr                      sym = NULL, prototype_sym, param_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_token_cache                     local_token_cache;
  a_type_kind                       type_kind;
  a_type_ptr                        prototype_type = NULL;
  a_template_arg_ptr                tap, *append_addr;
  a_template_param_ptr              tpp;
  a_boolean			    err;
  a_stop_token_array                save_stop_token_array;

  db_enter(3, "class_template_declaration");
  if (curr_token == tok_typedef || curr_token == tok_auto ||
      curr_token == tok_register) {
    error(ec_bad_storage_class_on_template_decl);
    (void)get_token();
  }  /* if */
  if (curr_token == tok_class || curr_token == tok_struct ||
      curr_token == tok_union) {
    switch (curr_token) {
      case tok_class:  type_kind = (a_type_kind)tk_class;  break;
      case tok_struct: type_kind = (a_type_kind)tk_struct; break;
      case tok_union:  type_kind = (a_type_kind)tk_union;  break;
      default:;  /* Avoid gcc warnings. */
    }  /* switch */
    /* This appears to be a class template declaration -- though it could
       be a function template declaration with a return type using one of
       these keywords.  We'll proceed on the assumption that it is indeed
       a class template until we see evidence to the contrary. */
    is_class_template_decl = TRUE;
    /* Bypass "class", "struct", or "union".  It has to be cached in case it
       has to be rescanned as part of a function template declaration. */
    clear_token_cache(&local_token_cache, /*reusable=*/FALSE);
    cache_curr_token(&local_token_cache);
    (void)get_token();
    /* Next should be the class name. */
    if (!is_qualified_name_start()) {  /* Identifier or "::". */
      /* Not an identifier. */
      error(ec_exp_identifier);
      set_to_error_locator(locator);
    } else {
      /* Look up the identifier.  If it's a qualified name there will be an
         error down the line. */
      sym = coalesce_and_lookup_generalized_identifier
                               (GID_TEMPLATE_ARGS_OPTIONAL, ilm_normal, &err);
      /* Cache the identifier and advance past it so we can discriminate
         between a class template and a function template. */
      cache_curr_token(&local_token_cache);
      locator = locator_for_curr_id;
      (void)get_token();
      if (is_declarator_start()) {
        /* Since the current token appears to be the start of a declarator
           this looks like a function template declaration after all.  Return
           to the caller, but first rewind to the start of the return type
           declaration. */
        is_class_template_decl = FALSE;
        rescan_cached_tokens(&local_token_cache);
        sym = NULL;
        goto done;
      }  /* if */
      /* Now check for a qualified name.  If it is, set the locator to an
         error locator -- we don't have to worry about the locator that's
         already in the cache because this template will never be
         instantiated. */
      if (locator.is_qualified_name) {
        error(ec_qualified_name_not_allowed);
        set_to_error_locator(locator);
        sym = NULL;
      }  /* if */
    }  /* if */
    /* We needed local_token_cache only in case this was not a class
       template declaration.  But now we can assume it is. */
    discard_token_cache(&local_token_cache);
    is_definition = (curr_token == tok_colon || curr_token == tok_lbrace);
    /* If get_normal_id_or_qualified_name returned something, we may have a
       name conflict or a redefinition. */
    if (sym != NULL) {
      if (sym->kind == (a_symbol_kind)sk_class_template) {
        tssp = sym->variant.template_info;
        is_redecl = TRUE;
        if ((type_kind == (a_type_kind)tk_union) !=
              (tssp->variant.class_template.type_kind ==
                                                    (a_type_kind)tk_union)) {
          /* Cannot mix union and nonunion declarations. */
          pos_sy_error(ec_not_compatible_with_previous_decl,
                       &locator.source_position, sym);
          suppress_redecl_error = TRUE;
          sym = NULL;
        } else if (!sym->defined) {
          /* Not previously defined. */
          *resolution = is_definition;
        } else if (is_definition) {
          /* Attempting to redefine a class template. */
          pos_sy_error(ec_already_defined, &locator.source_position, sym);
          suppress_redecl_error = TRUE;
          sym = NULL;
        }  /* if */
        if ((is_definition || is_redecl) && sym != NULL) {
          /* Either a definition or a redeclaration.  Make sure the template
             parameters are compatible with the previous declaration. */
          (void)reconcile_template_param_lists(templ_params, sym,
                                               &locator.source_position);
        }  /* if */
      } else if (sym->is_template_param) {
        /* The class being declared has the same name as one of its
           template parameters. */
        if (is_definition) {
          /* This is only an error on a class template definition, not
             on a simple declaration in which the names are
             never used again. */
          pos_error(ec_class_template_same_name_as_templ_param,
                    &locator.source_position);
        }  /* if */
        suppress_redecl_error = TRUE;
        sym = NULL;
      } else {
        /* Force the call to enter symbol, which will report the name clash. */
        sym = NULL;
      }  /* if */
    }  /* if */
    /* Make sure that the default arguments for the template parameters
       are valid (i.e., that they are at the end of the parameter list).
       This is done now because we have to wait until the parameter lists
       have been merged to do the test. */
    check_template_param_default_args(templ_params);
    if (sym == NULL) {
      /* Enter the symbol at file scope. */
      sym = enter_symbol((a_symbol_kind)sk_class_template, &locator,
                         DEPTH_OF_FILE_SCOPE, suppress_redecl_error);
      tssp = sym->variant.template_info;
      is_redecl = FALSE;
    }  /* if */
    if (is_definition || !is_redecl) {
      /* Either this is the first declaration of the template class or a
         defining redeclaration. */

      /* Save the type kind (corresponding to the class/struct/union token)
         in the class template symbol's supplement -- it will be needed when
         type entries for instantiations are created. */
      tssp->variant.class_template.type_kind = type_kind;

      tssp->parameters = templ_params;
      tssp->declaration_scope = scope_stack[decl_scope_level].number;
    }  /* if */
    if (is_definition) {
      *defines_something = TRUE;
      prototype_sym = make_template_class_symbol(sym, &sym->decl_position);
      /* Add the new symbol to the head of the instantiation list. */
      prototype_sym->next = tssp->variant.class_template.instantiations;
      tssp->variant.class_template.instantiations = prototype_sym;
      /* Now create a new type entry. */
      prototype_type = alloc_type(tssp->variant.class_template.type_kind);
      prototype_sym->variant.class_struct_union.type = prototype_type;
      set_source_corresp(&(prototype_type->source_corresp), prototype_sym);
      prototype_type->source_corresp.name_linkage =
                                           (a_name_linkage_kind)nlk_internal;
#if 0
      mark_defined(prototype_sym, &prototype_sym->decl_position);
#else /* 0 */
      prototype_sym->defined = TRUE;
#endif /* if 0 */
      /* Build the template argument list for the prototype instantiation
         of this template.  Loop through the template parameters and
         create a corresponding template argument for each. */
      append_addr = &prototype_type->
                     variant.class_struct_union.extra_info->template_arg_list;
      for (tpp = templ_params; tpp != NULL; tpp = tpp->next) {
        param_sym = tpp->param_symbol;
        if (param_sym->kind == (a_symbol_kind)sk_type) {
          tap = alloc_template_arg(/*is_arg_type=*/TRUE);
          tap->variant.type = param_sym->variant.type;
        } else {
          tap = alloc_template_arg(/*is_arg_type=*/FALSE);
          tap->variant.constant = param_sym->variant.constant;
        }  /* if */
        *append_addr = tap;
        append_addr = &tap->next;
      }  /* for */
      /* Save the current stop token state, and reinitialize it. */
      copy_stop_tokens(stop_token_array, save_stop_token_array);
      clear_stop_tokens();
      /* This is a class template definition, so scan all the tokens that
         comprise it and cache them away. */
      add_stop_token(tok_semicolon);
      if (curr_token == tok_colon) {
        /* Scan the tokens in the base class declarations, stopping when
           the "{" is reached. */
        add_stop_token(tok_lbrace);
        cache_token_stream(&tssp->token_cache);
        remove_stop_token(tok_lbrace);
      }  /* if */
      remove_stop_token(tok_semicolon);
      /* Scan the class body.  If the body is missing the error will be
         found during prototype instantiation. */
      if (curr_token == tok_lbrace) {
        /* Swallow the "{" and then cache everything through to the "}". */
        cache_curr_token(&tssp->token_cache);
        (void)get_token();
        add_stop_token(tok_rbrace);
        cache_token_stream(&tssp->token_cache);
        remove_stop_token(tok_rbrace);
        /* Now cache the "}" (unless we didn't find one). */
        if (curr_token == tok_rbrace) {
          cache_curr_token(&tssp->token_cache);
          (void)get_token();
        }  /* if */
      }  /* if */
      /* Add an end-of-source token to the end of the token cache to assure
         that we don't scan past the end of the cache in the actual scan. */
      terminate_token_cache(&tssp->token_cache);
      /* Restore the stop token state. */
      copy_stop_tokens(save_stop_token_array, stop_token_array);
      /* Note that the semicolon is not cached. */
    } else {
      /* This is not a class template definition, so we have no need to
         cache the tokens. */
    }  /* if */
  }  /* if */
done:;
  *p_sym_ptr = sym;
  *new_type = prototype_type;

  db_exit();
  return is_class_template_decl;
}  /* class_template_declaration */


static void cache_function_template_body(a_token_cache  *p_token_cache,
                                         a_boolean      is_constructor,
                                         a_boolean      *defines_something)
/*
Scan a function body and cache the tokens so that they can be rescanned
for the instantiation.
*/
{
  a_stop_token_array  save_stop_token_array;

  db_enter(3, "cache_function_template_body");
  if (curr_token == tok_lbrace ||
      (curr_token == tok_colon && is_constructor)) {
    *defines_something = TRUE;
    /* Save the current stop token state, and reinitialize it. */
    copy_stop_tokens(stop_token_array, save_stop_token_array);
    clear_stop_tokens();
    if (curr_token == tok_colon) {
      add_stop_token(tok_lbrace);
      add_stop_token(tok_semicolon);
      cache_token_stream(p_token_cache);
      remove_stop_token(tok_lbrace);
      remove_stop_token(tok_semicolon);
    }  /* if */
    if (curr_token == tok_lbrace) {
      /* Cache the "{" and advance past it. */
      cache_curr_token(p_token_cache);
      (void)get_token();
      /* Cache all tokens up to the "}" (or end-of-source). */
      add_stop_token(tok_rbrace);
      cache_token_stream(p_token_cache);
      remove_stop_token(tok_rbrace);
      /* Cache the "}" and append an end-of-source token. */
      if (curr_token == tok_rbrace) {
        cache_curr_token(p_token_cache);
        /* Advance to the next token. */
        (void)get_token();
      }  /* if */
      /* Add an end-of-source token to the end of the token cache to
         assure that we don't scan past the end of the cache in the actual
         scan. */
      terminate_token_cache(p_token_cache);
      /* Restore the stop token state. */
      copy_stop_tokens(save_stop_token_array, stop_token_array);
    }  /* if */
  } else {
    /* No body to cache. */
  }  /* if */
  db_exit();
}  /* cache_function_template_body */


static void cache_template_declaration(a_token_cache  *p_token_cache)
/*
Scan a declaration and cache the tokens so that they can be rescanned
for the instantiation.  The declarations for functions must be saved
so that they may be rescanned with the appropriate values substituted
for the template parameters.  At this point, however, we don't know
whether the thing being scanned is a function.  So this routine must
be capable of scanning an arbitrary template declaration.  In practice,
this will never be a class declaration.
*/
{
  a_stop_token_array  save_stop_token_array;

  db_enter(3, "cache_template_declaration");
  clear_token_cache(p_token_cache, /*reusable=*/TRUE);
  /* Save the current stop token state, and reinitialize it. */
  copy_stop_tokens(stop_token_array, save_stop_token_array);
  clear_stop_tokens();
  add_stop_token(tok_lbrace);
  add_stop_token(tok_colon);
  add_stop_token(tok_semicolon);
  /* Cache the current token and advance past it. */
  cache_curr_token(p_token_cache);
  (void)get_token();
  /* Cache all tokens up to the ";" that follows a declaration, the
     "{" that begins a definition, or a ":" that begins a ctor
     initializer list. */
  cache_token_stream(p_token_cache);
  remove_stop_token(tok_lbrace);
  remove_stop_token(tok_colon);
  remove_stop_token(tok_semicolon);
  /* Restore the stop token state. */
  copy_stop_tokens(save_stop_token_array, stop_token_array);
  /* Add an end-of-source token to the end of the token cache to
     assure that we don't scan past the end of the cache in the actual
     scan. */
  terminate_token_cache(p_token_cache);
  /* Rescan a copy of the cached tokens from this cache.  This is done so that
     when the original template declaration is scanned the last token of
     the cache is followed by the token that followed it in the original
     source program with no intervening tok_end_of_source.  This also
     allows the reusable token cache to be discarded if it turns out that
     this is not a function declaration. */
  rescan_copy_of_cache(p_token_cache);
  db_exit();
}  /* cache_template_declaration */


void prescan_function_template_default_arg_expr(a_param_type_ptr  ptp)
/*
Scan a default argument expression and add it to the list of arguments
pointed to by the template symbol supplement.
*/
{
  a_def_arg_expr_fixup_ptr	*list;
  list = &curr_default_args;
  prescan_default_function_arg_expr(ptp, list);
}  /* prescan_function_template_default_arg_expr */


void prescan_template_param_decl(a_token_cache	*token_cache)
/*
Place the tokens for a template parameter into a token cache.
*/
{
  a_stop_token_array        save_stop_token_array;

  db_enter(3, "prescan_template_param_decl");
  clear_token_cache(token_cache, /*reusable=*/TRUE);
  /* Save the current stop token state, and reinitialize it. */
  copy_stop_tokens(stop_token_array, save_stop_token_array);
  clear_stop_tokens();
  /* In the normal case we will scan an expression and encounter a comma
     or right parenthesis.  If both of these are omitted, terminate the token
     stream when some likely delimiter is reached. */
  add_stop_token(tok_comma);
  add_stop_token(tok_gt);
  add_stop_token(tok_semicolon);
  cache_token_stream(token_cache);
  /* Note that the terminating token (comma, etc.) is not added to
     the cache. */
  /* Add an end-of-source token to the end of the token cache.  This assures
     that we won't scan past the end of the cache in the actual scan. */
  terminate_token_cache(token_cache);
  /* Restore the original stop token state. */
  copy_stop_tokens(save_stop_token_array, stop_token_array);
  /* Rescan a copy of the tokens that were just cached.  Rescanning a copy
     ensures that processing of the remainder of the original line will
     not be affected by the tok_end_of_source that terminates the cache. */
  rescan_copy_of_cache(token_cache);
  db_exit();
}  /* prescan_template_param_decl */


void scan_a_template_parameter_declaration(a_symbol_locator *param_locator,
					   a_type_ptr       *param_type_ptr)
/*
Scan the declaration of a single template nontype parameter.
*/
{
  a_decl_flag_set			do_flags;
  a_decl_flag_set			dso_flags;
  a_storage_class    			param_storage_class;
  a_type_ptr 			        bottom_derived_type;
  a_source_position			param_pos;

  /* Scan the declaration specifiers. */
  param_pos = pos_curr_token;
  (void)decl_specifiers((DSI_TYPE_SPECIFIER_ALLOWED |
                         DSI_IS_TEMPLATE_PARAMETER),
                         &dso_flags, &param_storage_class,
                         param_type_ptr);
  if (dso_flags & DSO_DEFINES_SOMETHING) {
    pos_error(ec_type_definition_not_allowed, &param_pos);
    *param_type_ptr = error_type();
  }  /* if */
  if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
    /* Missing type specifier. */
    warning(ec_missing_type_specifier);
  }  /* if */
  /* Scan the declarator. */
  declarator(DI_REAL_DECLARATOR_ALLOWED, &do_flags,
             *param_type_ptr, /*member_parent_type=*/(a_type_ptr)NULL,
             param_locator, param_type_ptr, &bottom_derived_type,
             (a_func_info_block_ptr)NULL);
  /* Adjust the type if necessary (for example, "array of x"
     becomes "pointer to x"). */
  adjust_parameter_type(param_type_ptr);
}  /* scan_a_template_parameter_declaration */


static a_template_param_ptr scan_template_param_list(void)
/*
Scan a comma-separated list of template parameters.  The opening "<" will
already have been scanned, and an empty list will have already been
checked for.  The current token, consequently, is the first token of the
first parameter.  Return a pointer to the linked list that is created
to represent the template parameters.
*/
{
  a_symbol_ptr         sym;
  a_template_param_ptr template_param;
  a_template_param_ptr template_param_list = NULL;
  a_template_param_ptr end_of_template_param_list = NULL;
  a_type_ptr           template_param_type;
  int                  template_param_list_pos = 0;
  a_token_cache        param_cache;
  a_boolean	       parameter_cache_used = FALSE;

  db_enter(3, "scan_template_param_list");
  /* Check for an bypass the "<". */
  if (curr_token != tok_lt) {
    error(ec_exp_lt);
  } else {
    (void)get_token();
  }  /* if */
  add_stop_token(tok_semicolon);
  add_stop_token(tok_lbrace);
  add_stop_token(tok_gt);
  /* Loop through the comma-separated list of template parameter
     declarations. */
  do {
    a_boolean      has_default_arg = FALSE;
    a_boolean	   const_type_involves_template_param = FALSE;
    a_token_cache  def_arg_cache;
    a_constant_ptr default_arg_constant;

    ++template_param_list_pos;
    /* Cache the tokens that comprise the template parameter declaration.
       If the parameter depends on other template parameters this cache
       will be saved and rescanned to scan template argument lists. */
    prescan_template_param_decl(&param_cache);
    add_stop_token(tok_comma);
    /* Determine whether this is a "type-argument" (a parameter that
       represents a type) or a "arg-declaration" (a parameter that represents
       a constant). */
    if (curr_token == tok_class && next_token() == tok_identifier) {
      /* A type-argument. Note that there is a possible ambiguity here:
         template <class T> vs. template <class T X>, where in the second
         case T is already declared.  One could argue that the second is an
         "arg-declaration" rather than a "type-argument", but the working
         paper (14.1 para 2) appears to resolve the ambiguity in favor of
         always interpreting <class T ... as a type-argument.  Moreover, a
         class object cannot be a constant. */
      /* Bypass "class". */
      (void)get_token();
      /* Enter a type symbol in the symbol table.  It is made (for now) to
         point to an error type, to make everything work smoothly during
         preliminary scanning of the body of the class. */
      sym = enter_symbol((a_symbol_kind)sk_type, &locator_for_curr_id,
                         decl_scope_level, /*suppress_redecl_error=*/FALSE);
      /* Allocate a template-param type.  This type is for front-end use
         only and will not appear in the IL passed on to the back end.  It
         is therefore not added to any scope types list. */
      template_param_type = alloc_type((a_type_kind)tk_template_param);
      template_param_type->variant.template_param.list_position =
                                                     template_param_list_pos;
      set_type_size(template_param_type);
      set_source_corresp(&template_param_type->source_corresp, sym);
      /* The type symbol for the template parameter points for now to the
         template-param type -- "for now", since it will be replaced with
         an actual type during instantiation of the class or function. */
      sym->variant.type = template_param_type;
      /* Bypass the identifier. */
      (void)get_token();
      if (curr_token == tok_assign) {
        /* A default value is not allowed for a type parameter. */
        error(ec_default_arg_expr_not_allowed);
	flush_tokens();
      }  /* if */
    } else if (curr_token != tok_template) {
      a_type_ptr           param_type_ptr;
      a_symbol_locator     param_locator;
      /* Not a type-argument, so treat it as an arg-declaration.  If this
         template declaration happens to be of a function rather than a class,
         arg-declarations are not allowed.  That will be detected later. */
      scan_a_template_parameter_declaration(&param_locator, &param_type_ptr);
#if 0
      /* Check here for types for which constants cannot be created?  E.g.,
         the program would not be able to declare a constant class object or
         a constant array.  Likewise, should reference types be permitted?
         Should a constant with an error type be created for such cases? */
#endif /* if 0 */
      /* Enter a symbol and bind a template param constant to it. At each
         point of instantiation an actual constant will be substituted. */
      sym = enter_symbol((a_symbol_kind)sk_constant, &param_locator,
                         decl_scope_level, /*suppress_redecl_error=*/FALSE);
      sym->variant.constant =
                         fs_constant((a_constant_repr_kind)ck_template_param);
      sym->variant.constant->type = param_type_ptr;
      /* Note that the variant field template_param.kind was initialized to
         tpck_param when the constant was allocated. */
      sym->variant.constant->variant.template_param.variant.list_position =
                                                      template_param_list_pos;
      set_source_corresp(&sym->variant.constant->source_corresp, sym);
      const_type_involves_template_param = 
				is_or_contains_template_param(param_type_ptr);

      if (curr_token == tok_assign) {
        /* Scan the default value. */
	has_default_arg = TRUE;
	/* Skip past the equals sign. */
        (void)get_token();
        if (const_type_involves_template_param) {
	  /* The type of the constant parameter involve a template parameter
	     type so we can't scan the expression now.  Cache the tokens
	     that comprise the default argument. */
	  prescan_default_arg_expr(&def_arg_cache, /*is_template_param=*/TRUE);
        } else {
	  /* The type doesn't involve a template parameter type.  Scan the
	     default argument expression. */
          default_arg_constant = fs_constant((a_constant_repr_kind)ck_error);
          scan_template_argument_constant_expression(param_type_ptr,
						     default_arg_constant);
        }  /* if */
      }  /* if */
    } else {
      /* Error case, but scan it as a template declaration anyway. */
      a_boolean  defines_something;

      sym = template_declaration(&defines_something);
      set_to_error_locator(locator_for_curr_id);
      if (sym != NULL &&
          sym->kind == (a_symbol_kind)sk_class_template) {
        /* It's a class declaration in the template param list.  Just to be
           complete, be sure there's a full declaration. */
        if (!defines_something) error(ec_exp_declaration);
        /* Enter a dummy param type. */
        sym = enter_symbol((a_symbol_kind)sk_type, &locator_for_curr_id,
                           decl_scope_level, /*suppress_redecl_error=*/FALSE);
        sym->variant.type = error_type();
      } else {
        /* It's not a class declaration, so (whatever it might be) treat it
           as a constant. */
        sym = enter_symbol((a_symbol_kind)sk_constant, &locator_for_curr_id,
                           decl_scope_level, /*suppress_redecl_error=*/FALSE);
        sym->variant.constant =
                         fs_constant((a_constant_repr_kind)ck_template_param);
        sym->variant.constant->type = error_type();
        sym->variant.constant->variant.template_param.variant.list_position =
                                                      template_param_list_pos;
      }  /* if */
    }  /* if */
    sym->is_template_param = TRUE;
    /* Allocate a template parameter and set its fields based on sym. */
    template_param = alloc_template_param(sym);
    if (const_type_involves_template_param) {
      template_param->
	    variant.param_constant.type_involves_template_param = TRUE;
      template_param->token_cache = param_cache;
      parameter_cache_used = TRUE;
    }  /* if */
    if (has_default_arg) {
      /* Update the default argument information in the template parameter. */
      template_param->variant.param_constant.has_default_arg = TRUE;
      if (const_type_involves_template_param) {
        template_param->
	    variant.param_constant.default_arg.token_cache = def_arg_cache;
      } else {
        template_param->
            variant.param_constant.default_arg.constant = default_arg_constant;
      }  /* if */
    }  /* if */
    /* Add the template param to the end of the list. */
    if (template_param_list == NULL) {
      template_param_list = template_param;
    } else {
      end_of_template_param_list->next = template_param;
    }  /* if */
    /* Discard the parameter token cache if it is not needed for later use. */
    if (!parameter_cache_used) discard_token_cache(&param_cache);
    end_of_template_param_list = template_param;
    remove_stop_token(tok_comma);
    /* Keep looping on a comma. */
  } while (loop_token(tok_comma));
  if (template_param_list == NULL) {
    error(ec_missing_template_param);
  }  /* if */
  remove_stop_token(tok_gt);
  remove_stop_token(tok_lbrace);
  remove_stop_token(tok_semicolon);
  /* Check for an bypass the ">". */
  if (curr_token != tok_gt) {
    error(ec_exp_gt);
  } else {
    (void)get_token();
  }  /* if */
  db_exit();
  return template_param_list;
}  /* scan_template_param_list */


static a_boolean template_param_appears_in_param_list
				(a_type_ptr  tparam_type,
                                 a_type_ptr  rout_type,
			         a_boolean   *only_used_in_default_args)
/*
tparam_type is a tk_template_parameter type entry used in a template
declaration, and rout_type is a routine type.  Search each of the routine's
parameter types to see if tparam_type appears in it.  If
only_used_in_default_args is not NULL then also determine whether the
template parameter is only used in function parameters with default
arguments.
*/
{
  a_boolean         found = FALSE;
  a_boolean	    only_in_default_args;
  a_param_type_ptr  ptp;

  /* Only do this check if the pointer passed by the caller is non-NULL. */
  only_in_default_args = (only_used_in_default_args != NULL);
  ptp = rout_type->variant.routine.extra_info->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) {
    if (ptp->type_involves_template_param) {
      if (is_or_contains_specific_template_param(ptp->type, tparam_type)) {
        found = TRUE;
        if (!ptp->has_default_arg) only_in_default_args = FALSE;
      }  /* if */
      /* If we've found all the information we are looking for then stop. */
      if (found && !only_in_default_args) break;
    }  /* if */
  }  /* for */
  if (only_used_in_default_args != NULL) {
    *only_used_in_default_args = only_in_default_args;
  }  /* if */
  return found;
}  /* template_param_appears_in_param_list */


a_symbol_ptr template_declaration(a_boolean  *defines_something)
/*
Scan a C++ template declaration.  Syntax:

  template-declaration:

    template < template-argument-list > declaration

  template-argument:

    type-argument
    argument-declaration

  type-argument:

    class identifier

Template declarations will declare either a class template or a function
template; in the latter case the template argument list may include only
type-arguments. During the scan of the template declaration a special scope
entry is pushed on the scope stack.
*/
{
  a_template_param_ptr              tpp, template_param_list = NULL;
  a_symbol_ptr                      sym, param_sym;
  a_template_symbol_supplement_ptr  tssp;
  a_boolean                         tag_resolution = FALSE;
  a_type_ptr                        prototype_type = NULL;
  a_def_arg_expr_fixup_ptr	    saved_curr_default_args;
  a_token_cache 		    decl_token_cache;
  a_boolean		            decl_token_cache_used = FALSE;

  db_enter(3, "template_declaration");
#if CHECKING
  if (curr_token != tok_template) {
    internal_error("template_declaration: expected tok_template");
  }  /* if */
#endif /* CHECKING */
  saved_curr_default_args = curr_default_args;
  curr_default_args = NULL;
  *defines_something = FALSE;
  if (decl_scope_level != DEPTH_OF_FILE_SCOPE) {
    /* template declarations may appear at file scope only (ARM 14.1). */
    error(ec_nonglobal_template_declaration);
  }  /* if */
  (void)push_scope((a_scope_kind)sck_template_declaration, NO_SCOPE_NUMBER,
                   (a_type_ptr)NULL, (a_routine_ptr)NULL, (a_symbol_ptr)NULL,
                   (a_symbol_ptr)NULL, (a_template_arg_ptr)NULL);
  /* Bypass "template".  The next token should be "<". */
  (void)get_token();
  /* The the template parameters. */
  template_param_list = scan_template_param_list();
  /* Cache the tokens for this declaration.  If this turns out to be
     a function the cache will be saved to generates new routine types
     for this function.  If it is not a function the cache will be
     discarded.  The tokens are cached and a temporary copy of the
     cache is made.  The tokens are scanned in a nonreusable manner
     from the temporary cache.  The last cached token is followed
     immediately by the token that followed it in the original source
     program (i.e., the temporary cache does not contain a terminating
     tok_end_of_source). */
  cache_template_declaration(&decl_token_cache);
  /* See if it is a class template declaration.  If it is, scan the tokens
     of the definition (if any) and cache them away of later reference. */
  if (class_template_declaration(template_param_list, &sym, &tag_resolution,
                                 &prototype_type, defines_something)) {
    /* The declaration was successfully scanned as a class template
       declaration. */
  } else if (is_decl_start(/*expr_context=*/FALSE,
                           /*real_declarator_allowed=*/TRUE) ||
             is_declarator_start()) {
    /* Not a class template declaration.  Check for a function template
       declaration or a static data member template definition. */
    a_type_ptr         type;
    a_symbol_locator   locator;
    a_decl_flag_set    do_flags;
    a_decl_flag_set    dso_flags;
    a_boolean          has_parenthesized_initializer = FALSE;
    a_func_info_block  func_info;
    a_storage_class    storage_class;

    /* Scan the decl. specifiers and the declaration. */
    scan_template_declaration(/*is_initial_decl=*/TRUE, &dso_flags, &do_flags,
                              &locator, &type, &func_info, &storage_class);
    has_parenthesized_initializer = 
                             (do_flags & DO_PARENTHESIZED_INITIALIZER) != 0;
    if (!is_function_type(type) && 
        (locator.specific_symbol != NULL ||
         (is_error_locator(locator) && curr_token == tok_assign))) {
      /* Name is a member of a class template (or a class nested within a class
         template).  It is not a function, so (in a legal program) it must be
         a static data member. */
      /* Special processing for static data member template declarations. */
      a_boolean      err = FALSE;
      a_token_cache  local_token_cache, *p_token_cache;

      sym = locator.specific_symbol;
      if (is_error_locator(locator)) {
        /* An error occurred while scanning the declarator of what we assume
	   is a static data member.  We make this assumption because the
           declarator is not a function and is followed by an equals sign. */
        err = TRUE;
      } else if (sym->kind != (a_symbol_kind)sk_static_data_member) {
        /* Not a static data member. */
        if (sym->kind == (a_symbol_kind)sk_field) {
          pos_error(ec_nonstatic_member_def_not_allowed,
                    &locator.source_position);
        } else if (sym->kind == (a_symbol_kind)sk_projection) {
          /* A member of a base class. */
          pos_error(ec_inherited_member_not_allowed, &locator.source_position);
        } else {
          pos_sy_error(ec_not_compatible_with_previous_decl,
                       &locator.source_position, sym);
        }  /* if */
        err = TRUE;
      } else if (sym->defined) {
        /* Prior definition. */
        pos_sy_error(ec_already_defined, &locator.source_position, sym);
        err = TRUE;
      } else if (!types_are_compatible(type,
                                       sym->variant.static_data_member.
                                                          variable->type)) {
        /* The type of the static data member definition does not match
           the declaration in the class. */
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &locator.source_position, sym);
        err = TRUE;
      } else {
        /* This is a template definition of a static data member of a
           class template. */
#if CHECKING
        if (sym->variant.static_data_member.instance_ptr->
                                                  template_sym != sym) {
          internal_error("template_declaration: bad instance for static mem");
        }  /* if */
#endif /* CHECKING */
        sym->defined = TRUE;
        tssp = sym->variant.static_data_member.instance_ptr->template_info;
        /* Update the param list ptr, which should be non-null when the
           symbol is defined. */
        tssp->parameters = template_param_list;
        tssp->declaration_scope = scope_stack[decl_scope_level].number;
        /* Make sure the parameter list matches the class declaration. */
        if (!member_template_param_list_matches_class
                      (template_param_list, sym, &error_position)) {
          err = TRUE;
        }  /* if */
      }  /* if */
      /* Scan the initializer expression, if any, and cache its tokens.
         The initializer may be of the form "= ...;" or "(...);".
         Anything else will not get cached and an error will be generated
         on this declaration. */
      if (curr_token == tok_assign || has_parenthesized_initializer) {
        add_stop_token(tok_semicolon);
        p_token_cache = err ? &local_token_cache : &tssp->token_cache;
        clear_token_cache(p_token_cache, /*reusable=*/TRUE);
        cache_token_stream(p_token_cache);
        remove_stop_token(tok_semicolon);
        if (err) {
          discard_token_cache(p_token_cache);
        } else if (curr_token == tok_semicolon) {
          terminate_token_cache(p_token_cache);
        }  /* if */
      }  /* if */
    } else if (is_function_type(type)) {
      a_boolean  err = FALSE;

      /* Process a function template declaration. */
      decl_function_template(&locator, type, &func_info, &sym, storage_class);
      if (is_error_locator(locator)) {
        err = TRUE;
      } else if (curr_token == tok_lbrace ||
                 (curr_token == tok_colon && is_constructor_symbol(sym))) {
        if (sym->defined) {
          pos_sy_error(ec_already_defined, &locator.source_position, sym);
          err = TRUE;
        }  /* if */
        sym->defined = TRUE;
      } else {
        if (sym->kind == (a_symbol_kind)sk_member_function) {
          /* A non-defining declaration of a member function is not
             allowed. */
          pos_error(ec_member_function_redecl_outside_class,
                    &locator.source_position);
        }  /* if */
      }  /* if */
      if (sym->kind == (a_symbol_kind)sk_member_function) {
        tssp = sym->variant.routine.instance_ptr->template_info;
      } else {
        tssp = sym->variant.template_info;
      }  /* if */
      /* Make sure that the template parameter list is compatible with
         any previous declaration (i.e., the declaration of the class
         if this is a member function. */
      if (sym->class_of_which_a_member != NULL) {
        if (!member_template_param_list_matches_class
                      (template_param_list, sym, &error_position)) {
          err = TRUE;
        }  /* if */
      }  /* if */
      /* Copy the token cache header into the template symbol supplement. */
      tssp->variant.function.decl_token_cache = decl_token_cache;
      decl_token_cache_used = TRUE;
      if (err) {
        a_token_cache  local_token_cache;
        clear_token_cache(&local_token_cache, /*reusable=*/FALSE);
        cache_function_template_body(&local_token_cache, /*is_ctor=*/TRUE,
                                     defines_something);
        discard_token_cache(&local_token_cache);
      } else {
	a_def_arg_expr_fixup_ptr  daefp;
        tssp->variant.function.func_info = func_info;
	/* Link the default argument list from the template supplement
	   onto the end of the list of current default arguments.  The
	   list in the supplement must be for arguments that follow the
	   new list (otherwise it would be an error).  Find the end
	   of the current list and link the existing list to the end. */
	daefp = curr_default_args;
	if (daefp != NULL) {
	  while (daefp->next != NULL) daefp = daefp->next;
	  daefp->next = tssp->variant.function.def_arg_expr_list;
          tssp->variant.function.def_arg_expr_list = curr_default_args;
	}  /* if */
        tssp->parameters = template_param_list;
        tssp->declaration_scope = scope_stack[decl_scope_level].number;
        cache_function_template_body(&tssp->token_cache,
                                     is_constructor_symbol(sym),
                                     defines_something);
      }  /* if */
      if (sym->class_of_which_a_member != NULL) {
        /* Out-of-line definition of a member function of a class template.
           Don't impose requirements on the use of template parameters in the
           parameters. */
      } else if (err) {
        /* Avoid spurious errors -- skip the check for template params, since
           this might have been intended to be a member function. */
      } else {
        /* Go back through the template params and be sure there are only
           type args.  The other kind is allowed only for class templates. */
        for (tpp = template_param_list; tpp != NULL; tpp = tpp->next) {
          param_sym = tpp->param_symbol;
          if (param_sym->kind != (a_symbol_kind)sk_type) {
            pos_error(ec_not_a_type_arg, &param_sym->decl_position);
          } else {
	    /* Make sure that all template parameters are used by
	       function parameter types and not just by parameters
	       with default arguments.  If an error occurs set the
	       cannot_be_called flag to prevent an instantiation from
	       being attempted with an incomplete set of template arguments. */
	    a_boolean	only_in_default_args;
	    a_boolean	param_used;
	    param_used = template_param_appears_in_param_list
                      (param_sym->variant.type, type, &only_in_default_args);
	    if (!param_used) {
              pos_sy2_error(ec_not_used_in_template_function_params,
                            &param_sym->decl_position, param_sym, sym);
	      tssp->variant.function.cannot_be_called = TRUE;
	    } else if (only_in_default_args) {
              pos_sy2_error(ec_template_param_only_used_in_default_args,
                            &param_sym->decl_position, param_sym, sym);
	      tssp->variant.function.cannot_be_called = TRUE;
            }  /* if */
          }  /* if */
        }  /* for */
      }  /* if */
    } else {
      /* Error -- not a class template, a function template, nor a static
         data member template. */
      if (!is_error_locator(locator)) {
        pos_st_error(ec_bad_template_declaration, &locator.source_position,
                     locator.symbol_header->identifier);
      }  /* if */
    }  /* if */
  } else {
    /* Template parameters are declared, but the declaration is missing. */
    pos_error(ec_exp_declaration, &pos_curr_token);
  }  /* if */
  /* Note that the template declaration scope must be popped before doing the
     prototype instantiation. */
  pop_scope();
  if (prototype_type != NULL) {
#if CHECKING
    if (sym == NULL || sym->kind != (a_symbol_kind)sk_class_template ||
        (tssp = sym->variant.template_info) == NULL ||
        tssp->variant.class_template.instantiations == NULL ||
        tssp->variant.class_template.instantiations->
                         variant.class_struct_union.type != prototype_type) {
      internal_error("template_declaration: sym & prototype_type out of sync");
    }  /* if */
#endif /* CHECKING */
    /* Do a "prototype instantiation" of the class template -- i.e., parse
       the declarative information looking for gross syntax errors. */
    instantiate_class_template(sym, prototype_type);
  }  /* if */
  if (tag_resolution) {
    /* This is the resolution of a previously incomplete template declaration;
       if there are array types to be resolved, look to see if any of them are
       arrays whose element type is an instantiation of this template.  (This
       is by analogy with normal classes, for which an array of incomplete
       class objects is allowed, pending completion.) */
    check_fixup_list_for_array_types();
  }  /* if */
  /* If the declaration token cache is not needed, discard it. */
  if (!decl_token_cache_used) {
    discard_token_cache(&decl_token_cache);
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    if (sym != NULL) db_symbol(sym, "template symbol: ", 2);
  }  /* if */
#endif /* DEBUG */
  curr_default_args = saved_curr_default_args;
  db_exit();
  return sym;
}  /* template_declaration */


static a_can_instantiate_entry_ptr alloc_can_instantiate_entry(void)
/*
Allocate an entry of a can_instantiate list, initialize it, and return
a pointer to it.
*/
{
  a_can_instantiate_entry_ptr	ciep;

  ciep = (a_can_instantiate_entry_ptr)
                                alloc_fe(sizeof(a_can_instantiate_entry));
  ciep->next = NULL;
  ciep->class_type = NULL;
  return ciep;
}  /* alloc_can_instantiate_entry */


void add_to_can_instantiate_list(a_type_ptr class_type)
/*
Add an entry to the can_instantiate list.
*/
{
  a_can_instantiate_entry_ptr	ciep;

  ciep = alloc_can_instantiate_entry();
  ciep->class_type = class_type;
  ciep->next = can_instantiate_list;
  can_instantiate_list = ciep;
}  /* add_to_can_instantiate_list */


#if INSTANTIATION_BY_IMPLICIT_INCLUSION
static void do_implicit_include_if_needed(a_template_instance_ptr tip)
/*
Gets the name of the file in which the template was declared and attempts
to include a source file (e.g., .c file) that corresponds to the header
of the definition.  If an implicit include was already attempted then
we return without doing anything.  If there is no corresponding source
file we simply return.
*/
{
  a_source_position	*decl_position;
  a_line_number		line_number;
  a_boolean		at_end_of_source;
  unsigned long		nesting_depth;
  a_source_file_ptr	sfp;
  char			*full_file_name, *display_name;
  FILE			*f_source;
  a_boolean		is_system_include;

  db_enter(3, "do_implicit_include_if_needed");
  /* Translate the sequence number into a file name and line number. */
#if DEBUG
  if (debug_level >= 3) {
    fprintf(f_debug, "Attempting implicit include to define:\n");
    db_symbol(tip->instance_sym, "", 2);
  }  /* if */
#endif /* DEBUG */
  decl_position = &tip->template_sym->decl_position;
  sfp = source_file_for_seq(decl_position->seq, &line_number,
                            &at_end_of_source, &nesting_depth,
                            /*physical_line=*/FALSE);
  if (sfp != NULL && sfp != il_header.primary_source_file) {
    /* A source file was found and it does not refer to the primary source
       file. */
    if (!sfp->related_file_implicit_include_done) {
      /* If we haven't already included the corresponding source file then
         do so now. */
#if DEBUG
      if (debug_level >= 3) {
        fprintf(f_debug, "  Looking for source file related to '%s'\n",
                sfp->file_name);
      }  /* if */
#endif /* DEBUG */
      sfp->related_file_implicit_include_done = TRUE;
      is_system_include = sfp->included_by_system_include;
      /* Call a routine to search for a file with an appropriate suffix. */
      f_source = open_file_for_input(sfp->name_as_written, 
                                     is_system_include ? sys_incl_search_path :
                                                         incl_search_path,
				     /*replace_suffix=*/TRUE,
				     &full_file_name, &display_name);
      if (f_source != NULL) {
        /* A related source file was found.  Make sure that the name of the
           file found is not the same as the file we started with.  This
           could occur if the user included a .c file that contains a
           template declaration. */
        if (strcmp(full_file_name, sfp->full_name) != 0) {
#if DEBUG
          if (debug_level >= 3) {
            fprintf(f_debug, "  Including text from '%s'\n", full_file_name);
          }  /* if */
#endif /* DEBUG */
          /* Push the new file onto the input stack and scan it.  There is
             no "name as written" so a NULL pointer is passed in. */
          push_input_stack(f_source, (char *)NULL, display_name,
                           full_file_name, is_system_include);
          scan_implicitly_included_template_definition_file();
        } else {
          /* The file name returned by open_file_for_input is the same as
             the file in which the template was declared.  Just close
             the file. */
          (void)fclose(f_source);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  db_exit();
}  /* do_implicit_include_if_needed */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */


static void add_to_instantiations_required_list(a_template_instance_ptr  tip)
/*
Add a template instance entry to the end of the instantiatiations_required
list.
*/
{
  db_enter(5, "add_to_instantiations_required_list");
  if (tip->next_in_instantiation_list != NULL ||
      tip == instantiations_required_tail) {
    /* Already on the list -- don't try to add it again. */
    if (in_instantiation_wrapup && tip->instantiation_required) {
      /* The instantiation required flag has been set for an entry already
         on the list.  This means that instantiation_wrapup must make another
         pass over the instantiations list. */
      entries_updated_during_instantiation_wrapup = TRUE;
    }  /* if */
  } else {
    /* The entry must be added to the end of the list.  This is because new
       entries may be placed on the list even after processing on the list
       begins (see instantiation_wrapup). */
    if (instantiations_required == NULL) {
      instantiations_required = tip;
    } else {
      instantiations_required_tail->next_in_instantiation_list = tip;
    }  /* if */
    instantiations_required_tail = tip;
  }  /* if */
  db_exit();
}  /* add_to_instantiations_required_list */


static a_boolean is_inline_template_function(a_template_instance_ptr tip)
/*
Determines whether a template instance pointer refers to a function that
is inline.
*/
{
  a_boolean	result = FALSE;
  if (is_function_symbol(tip->instance_sym)) {
    /* If the is_inline flag is set in the routine entry then the routine
       must be inline whether or not an instantiation has been done or
       whether a template definition has been supplied.  If the routine
       is_inline flag is FALSE then we need to look at the is_inline flags
       associated with the template.  For member functions the is_inline
       flag in the template's routine entry reflects the declaration in
       the class template, whereas the flag in the func_info block reflects
       the function template definition, if any. */
    a_routine_ptr	rout = tip->instance_sym->variant.routine.ptr;
    result =  rout->is_inline;
    if (!result) {
      if (rout->assoc_scope == NULL_region_number) {
        a_template_symbol_supplement_ptr	tssp;
        tssp = template_supplement_for_symbol(tip->template_sym);
        result = tssp->variant.function.routine->is_inline ||
                 tssp->variant.function.func_info.is_inline;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_inline_template_function */


static a_boolean is_static_or_inline_template_function
					(a_template_instance_ptr tip)
/*
Determines whether a template instance pointer refers to a function that
is static or inline (i.e., is not an external function).
*/
{
  a_boolean     result = FALSE;

  if (!is_function_symbol(tip->instance_sym)) {
    /* Must be a static data member. */
  } else if (is_inline_template_function(tip)) {
    result = TRUE;
  } else if (tip->instance_sym->kind != (a_symbol_kind)sk_member_function) {
    /* Only check the storage class of nonmember functions.  The linkage
       of member functions has not been determined yet -- and member
       functions are inline or noninline.  There is no such thing as
       a noninline member function with static storage class.  This
       is only important in tim_none mode.  In all other modes any
       function with the instantiation required flag set will be
       instantiated. */
    a_routine_ptr	rout = tip->instance_sym->variant.routine.ptr;
    result = (rout->storage_class == (a_storage_class)sc_static);
  }  /* if */
  return result;
}  /* is_static_or_inline_template_function */

#if !INSTANTIATION_BY_IMPLICIT_INCLUSION
/*ARGSUSED*/ /* <-- implicit_inclusion_ok is not used if no implicit
                 inclusion. */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
static a_boolean should_be_instantiated
				(a_template_instance_ptr tip,
				 a_boolean	         implicit_inclusion_ok)
/*
Determines whether this template instance needs an instantiation and
generates any errors caused by conflicting instantiation information
such as instantiating a template for which no body was supplied.
implicit_inclusion_ok is TRUE if the compiler should attempt to include
a template definition file to provide definitions for externally linked
template entities.
*/
{
  a_boolean	result = TRUE;
  a_boolean	specific_def;
  a_boolean	template_def;
  if (tip->explicit_instantiation ||
      ((tip->instantiation_required || instantiation_mode == tim_all) &&
        (instantiation_mode != tim_none ||
         is_static_or_inline_template_function(tip)))) {
    /* For error checking purposes, find out if a specific definition
       exists and whether a body exists for the template definition. */
    if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
      specific_def = tip->instance_sym->defined;
      template_def = tip->template_sym->defined;
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
      if (!template_def && implicit_inclusion_ok &&
          implicit_template_inclusion_mode) {
        /* If a template definition is not present, attempt to include a
           source file that will provide the definition.  Then check
           again to see if a template definition is present. */
        do_implicit_include_if_needed(tip);
        template_def = tip->template_sym->defined;
      }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
    } else {
      a_template_symbol_supplement_ptr  tssp;
      specific_def = tip->specific_def;
      if (tip->instance_sym->class_of_which_a_member == NULL) {
        /* This is an instance of a nonmember function -- template_sym
           points to an sk_function_template symbol. */
        tssp = tip->template_sym->variant.template_info;
      } else {
        /* It is an instance of a member function -- template_sym points to
           an sk_member_function from the prototype instantiation, and the
           template supplement pointer is to be found in the latter's
           instance entry. */
        tssp = tip->template_sym->variant.routine.instance_ptr->template_info;
      }  /* if */
      template_def = tssp->token_cache.first_token != NULL;
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
      if (!template_def && implicit_inclusion_ok &&
          implicit_template_inclusion_mode) {
        /* If a template definition is not present, attempt to include a
           source file that will provide the definition.  Then check
           again to see if a template definition is present. */
        do_implicit_include_if_needed(tip);
        template_def = tssp->token_cache.first_token != NULL;
      }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
    }  /* if */
    if (!template_def && !specific_def) {
      /* A template can be declared and referenced without ever being defined.
         If, however, an instantiation was explicitly requested an error is
         issued.  In any case, the instantiation cannot be done without
         a template definition. */
      result = FALSE;
      if (tip->explicit_instantiation) {
        pos_sy_error(ec_instantiation_requested_no_definition_supplied,
  	           &tip->explicit_instantiation_pos,
  		    tip->instance_sym);
      }  /* if */
    } else {
      /* There is a body or a specific definition. */
      if (specific_def) {
        /* A specific definition was supplied.  Simply skip the instantiation
           unless an instantiation was explicitly requested. */
        result = FALSE;
        if (tip->explicit_instantiation) {
          pos_sy_error(ec_instantiation_requested_and_specific_definition,
  	             &tip->explicit_instantiation_pos, tip->instance_sym);
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    /* No instantiation needed. */
    result = FALSE;
  }  /* if */
  return result;
}  /* should_be_instantiated */


void update_instantiation_required_flag(a_template_instance_ptr tip,
                                        a_boolean               value)
/*
Updates the instantiation required flag in a template instance entry.  If the
flag is set to TRUE the instance entry is added to a list of entries for which
instantiation is required.  If the flag is set to FALSE the entry is simply
updated but not removed from the list.
*/
{
  a_symbol_ptr   sym;

  db_enter(5, "update_instantiation_required_flag");
#if DEBUG
  if (debug_level >= 5) {
    a_symbol_ptr sym = tip->instance_sym;
    fprintf(f_debug, "Setting instantiation_required flag to %s for ",
            value ? "TRUE" : "FALSE");
    db_symbol(tip->instance_sym, "", 0);
    fprintf(f_debug, "is_function_symbol=%d\n", is_function_symbol(sym));
    fprintf(f_debug, "defined=%d\n", sym->defined);
    if (is_function_symbol(sym)) {
      fprintf(f_debug, "inline=%d\n", sym->variant.routine.ptr->is_inline);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  if (instantiation_mode == tim_can_instantiate) {
    /* Leave the instantiation_required flag unchanged in this mode. */
  } else if (value) {
    a_template_symbol_supplement_ptr tssp;
    sym = tip->instance_sym;
    tssp = template_supplement_for_symbol(tip->template_sym);
    if (sym == tip->template_sym) {
      /* Somehow a member function of a nonreal class (e.g., a prototype
         instantiation of a class template) has been referenced.  (This
         can occur in a sizeof operation applied to the address of a
         static member function -- anywhere else?).  Do not instantiate
         the function. */
    } else if (is_function_symbol(sym) &&
               tssp->token_cache.first_token != NULL &&
               is_inline_template_function(tip)) {
      /* Inline (member or nonmember) functions are instantiated at the
         point of first use, in case the back end requires the function
         body immediately to perform inlining. */
      if (!tip->already_instantiated) {
        instantiate_template_function(tip);
      }  /* if */
      tip->instantiation_required = TRUE;
    } else if (value == (a_boolean)tip->instantiation_required) {
      /* The instantiation required flag is already set to the desired
         value.  This test is used to ensure that an entry that is already
         on the instantiation required list won't be instantiated until
         reached on the list.  This prevents things on the list from being
         instantiated during the instantiation of other functions of entries
         earlier on the list. */
    } else {
      /* If we are in instantiation wrapup then instantiate the function now
	 instead of just adding it to the end of the list.  This makes
	 it possible to detect runaway recursive instantiations that
	 are very difficult to detect when the instantiations are done
	 serially. */
      tip->instantiation_required = TRUE;
      if (in_instantiation_wrapup) {
        if (!tip->already_instantiated &&
            should_be_instantiated(tip, /*implicit_inclusion_ok=*/FALSE)) {
          /* Implicit inclusion is not done for "on the fly" instantiations
             because the includes cannot be processed in the middle of
	     the instantiation of another function.  The entry will be put
	     on the instantiation required list and instantiated later in
             instantiation_wrapup. */
          if (tip->instance_sym->kind ==
                                        (a_symbol_kind)sk_static_data_member) {
            define_template_static_data_member(tip);
          } else {
            instantiate_template_function(tip);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    tip->instantiation_required = FALSE;
  }  /* if */
  /* The entry is always added to the instantiations list because certain
     entries for which instantiation is not required need to be processed
     for automatic instantiation processing. */
  add_to_instantiations_required_list(tip);
  db_exit();
}  /* update_instantiation_required_flag */


#if AUTOMATIC_TEMPLATE_INSTANTIATION
static a_boolean open_instantiation_info_file(void)
/*
Open the instantiation information file associated with the primary source
file.  Return TRUE if the file was successfully opened.
*/
{
  f_instantiation_info = NULL;
  if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) != 0) {
    /* Only open the file if the input is coming from a file. */
    instantiation_info_file_name =
            derived_name(primary_source_file_name, INSTANTIATION_FILE_SUFFIX);
    f_instantiation_info = fopen(instantiation_info_file_name, "r");
  }  /* if */
  return f_instantiation_info != NULL;
}  /* open_instantiation_info_file */


void create_or_remove_instantiation_information_file(void)
{
  FILE		*f_ii_file;

  if (strcmp(primary_source_file_name, FILE_NAME_FOR_STDIN) != 0) {
    /* Only create the file if the input is coming from a file.  Note
       that the file will have been closed after all input was read so
       it must be reopened now. */
    f_ii_file = fopen(instantiation_info_file_name, "r");
    if (f_ii_file != NULL) (void)fclose(f_ii_file);
    if (any_instantiations_required) {
      /* If the file does not exist, create it. */
      if (f_ii_file == NULL) {
        f_ii_file = fopen(instantiation_info_file_name, "a");
        if (f_ii_file == NULL) {
          str_catastrophe(ec_cannot_create_instantiation_information_file,
                          instantiation_info_file_name);
        }  /* if */
      }  /* if */
    } else {
      /* No instantiation information needed.  Delete the file if it
         already exits. */
      if (f_ii_file != NULL) {
        delete_file(instantiation_info_file_name);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* create_or_remove_instantiation_information_file */


static an_instance_lookup_entry_ptr alloc_instance_lookup_entry(void)
/*
Allocate an instance lookup entry, initialize it, and return a pointer
to it.
*/
{
  an_instance_lookup_entry_ptr	ilp;

  ilp = (an_instance_lookup_entry_ptr)
                                alloc_fe(sizeof(an_instance_lookup_entry));
  ilp->next = NULL;
  ilp->name = NULL;
  return ilp;
}  /* alloc_instance_lookup_entry */


static an_instance_lookup_entry_ptr find_instance(char		*name,
				                  a_boolean	add)
/*
Find a symbol entry with the specified name.  Add the name to the
list if an entry does not already exist.
*/
{
  register unsigned            hash_value = 0;
  register char                *ptr;
  an_instance_lookup_entry_ptr ilp    = NULL;
  int                          bucket_number;
  int			       length;

  length = strlen(name);
  /* Hash the symbol's name.  This involves taking the name's
     first, last, and middle 3 characters.  Of course, if the name has
     fewer than 5 characters, take the entire name. */
  if (length > 5) {
    ptr = name + (length >> 1) - 1;
    hash_value = (int)*name;
    hash_value = (hash_value * HASH_FACTOR) +
                                             (int)*(name + length - 1);
    hash_value = (hash_value * HASH_FACTOR) + (int)*ptr++;
    hash_value = (hash_value * HASH_FACTOR) + (int)*ptr++;
    hash_value = (hash_value * HASH_FACTOR) + (int)*ptr;
  } else {
    register int i;
    ptr = name;
    for (i = 0; i < length; i++) {
      hash_value = (hash_value * HASH_FACTOR) + (int)*ptr++;
    }  /* for */
  }  /* if */

  /* Look in the symbol bucket saving the position in case this symbol needs
     to be added. */
  bucket_number = hash_value % INSTANCE_LOOKUP_TABLE_SIZE;
  if ((ilp = instance_lookup_table[bucket_number]) != NULL) {
    do {
      if (strcmp(name, ilp->name) == 0) {
        /* We have a match. */
        goto symbol_found;
      }  /* if */
    } while ((ilp = ilp->next) != NULL);
  }  /* if */

  /* Exiting this loop indicates that the symbol does not exist in the table;
     allocate a symbol header for it. */
  if (add) {
    ilp = alloc_instance_lookup_entry();
    /* Link the new header onto the front of the appropriate bucket of the
       symbol table. */
    ilp->next = instance_lookup_table[bucket_number];
    instance_lookup_table[bucket_number] = ilp;
    /* Allocate space for the name (including a null terminator) and make a
       copy of the name. */
    ilp->name = (char *)alloc_fe((sizeof_t)length + 1);
    strcpy(ilp->name, name);
  }  /* if */

symbol_found:
  return ilp;
}  /* find_instance */


static char *read_info_file(void)
/*
Reads a line of input from the instantiation list file.  Returns TRUE if
a line of input is being returned.  Returns FALSE at end-of-file.
*/
{
  register char*    buffer_pos;
  register sizeof_t size = 0;
  register int      ch;
  char              *result;
  static char	    *input_line;
  static sizeof_t   info_file_line_size = 0;

  /* Allocate space into which the input line can be read.  Only done
     the first time this routine is called. */
  if (info_file_line_size == 0) {
    input_line =
            (char *)alloc_general(INFO_FILE_LINE_INCREMENTAL_ALLOCATION);
    info_file_line_size = INFO_FILE_LINE_INCREMENTAL_ALLOCATION;
  }  /* if */
  buffer_pos = input_line;

  while (ch = getc(f_instantiation_info), ch != EOF && ch != '\n') {
    if (++size == info_file_line_size) {
      /* The input line needs to be expanded.  This occurs one character
         before the actual end of the buffer to ensure that there will be
         enough room for the null terminator at the end of the string. */
      sizeof_t  curr_offset;
      sizeof_t	new_size;
      new_size = info_file_line_size + INFO_FILE_LINE_INCREMENTAL_ALLOCATION;
      curr_offset = buffer_pos - input_line;
      input_line = realloc_general(input_line, info_file_line_size, new_size);
      info_file_line_size = new_size;
      buffer_pos = input_line + curr_offset;
    }  /* if */
    *buffer_pos++ = ch;
  }  /* while */
  
  /* Terminate string with a null character. */
  *buffer_pos++ = '\0';

  /* Determine whether to return end-of-file (NULL). */
  result = input_line;
  if (ch == EOF && size == 0) result = NULL;

  return (result);
}  /* read_info_file */


static a_boolean read_instantiation_info_file(void)
/*
Read the list of names from the instantiation information file and
enter the names into a hash table.  Returns TRUE if any entries
were entered in the hash table; otherwise returns FALSE.
*/
{
  char				*line;
  a_boolean			result = FALSE;
  int				i;

  if (open_instantiation_info_file()) {
    /* If the file does not exist, the open routine will return FALSE. */
    /* Skip over initial lines of the instantiation information file
       that don't contain instantiation entries. */
    for (i = 1; i <= INSTANTIATION_INFO_LINES_RESERVED; ++i) {
      /* Read and discard the line. */
      (void)read_info_file();
    }  /* if */
    /* The variable do_auto_instantiation indicates that an instantiation
       list file is present. */
    while ((line = read_info_file()) != NULL) {
      (void)find_instance(line, /*add=*/TRUE);
      result = TRUE;
    }  /* while */
    (void)fclose(f_instantiation_info);
  }  /* if */
  return result;
}  /* read_instantiation_info_file */


static a_boolean can_be_instantiated(a_template_instance_ptr tip)
/*
Determines whether this compilation is capable of generating an
instantiation of a given template instance.
*/
{
  a_boolean	result = TRUE;
  a_boolean	specific_def;
  a_boolean	template_def;
  /* For error checking purposes, find out if a specific definition
     exists and whether a body exists for the template definition. */
  if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
    specific_def = tip->instance_sym->defined;
    template_def = tip->template_sym->defined;
  } else {
    a_symbol_ptr		      template_sym;
    a_template_symbol_supplement_ptr  tssp;
    template_sym = tip->template_sym;
    tssp = template_supplement_for_symbol(template_sym);
    specific_def = tip->specific_def;
    template_def = tssp->token_cache.first_token != NULL;
#if INSTANTIATION_BY_IMPLICIT_INCLUSION
    if (!template_def && implicit_template_inclusion_mode) {
      /* If a template definition is not present, attempt to include a
         source file that will provide the definition.  Then check
         again to see if a template definition is present. */
      do_implicit_include_if_needed(tip);
      template_def = tssp->token_cache.first_token != NULL;
    }  /* if */
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */
  }  /* if */
  result = template_def && !specific_def && !tip->already_instantiated;
  return result;
}  /* can_be_instantiated */


static void automatic_instantiation(void)
/*
This is the main routine that handles automatic instantiation processing.
The list of instantiations that are to be performed by this compilation is
read from the instantiation information file.  We then go through the
instantiations required list and look for names that match the
instantiations to be done.  When a match is found the instantiation is
performed.
*/
{
  a_boolean			instantiations_needed;
  a_template_instance_ptr	tip;

  db_enter(3, "automatic_instantiation");
  /* Set the instantiation mode to tim_none.  This is done to ensure that
     only the instantiations explicitly requested in the list file are
     performed.  We don't want a mode like "used" or "all" to cause
     other instantiations to happen as a consequence of the requested
     instantiations that are performed. */
  instantiation_mode = tim_none;
  /* We always need to go through the full instantiation list to set the
     flags to be passed to the back-end.  We don't, however, need to
     compare mangled names unless there are actually instantiations that
     we need to do.   The read routine returns a flag that indicates whether
     any information was present in the instantiation file. */
  instantiations_needed = read_instantiation_info_file();
  tip = instantiations_required;
  for (; tip != NULL; tip = tip->next_in_instantiation_list) {
    char	*name;
    a_symbol_ptr			instance_sym = tip->instance_sym;
    a_routine_ptr			routine;
    a_variable_ptr			variable;
    a_boolean				is_static_data_member;
    a_boolean				can_instantiate;
    an_instance_lookup_entry_ptr	ilp = NULL;

    /* Skip non-external function. */
    if (is_static_or_inline_template_function(tip)) continue;
    /* Get a pointer to the IL entry to be processed. */
    if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
      is_static_data_member = TRUE;
      variable = instance_sym->variant.static_data_member.variable;
    } else {
      is_static_data_member = FALSE;
      routine = instance_sym->variant.routine.ptr;
    }  /* if */
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, "Automatic instantiation processing for:\n");
      db_symbol(instance_sym, "", 0);
    }  /* if */
#endif /* DEBUG */
    any_instantiations_required = TRUE;
    can_instantiate = can_be_instantiated(tip);
    if (instantiations_needed && can_instantiate) {
      /* If an instantiation list is present and if this template could
         be instantiated then check whether it was present in the
         instantiation list file. */
      if (is_static_data_member) {
        name = get_mangled_static_data_member_name(variable);
      } else {
        name = get_mangled_function_name(routine);
      }  /* if */
      ilp = find_instance(name, /*add=*/FALSE);
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, " instantiation_required=%d\n",
              tip->instantiation_required);
      fprintf(f_debug, " explicit_can_instantiate=%d\n",
              tip->explicit_can_instantiate);
    }  /* if */
#endif /* DEBUG */
      if (ilp != NULL) {
        /* The name was in the instantiation list.  Generate an
           instantiation. */
        if (is_static_data_member) {
          define_template_static_data_member(tip);
        } else {
          instantiate_template_function(tip);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
  db_exit();
}  /* automatic_instantiation */


static void update_auto_instantiation_flags(void)
/*
This is the main routine that handles automatic instantiation processing.
The list of instantiations that are to be performed by this compilation is
read from the instantiation information file.  We then go through the
instantiations required list and look for names that match the
instantiations to be done.  When a match is found the instantiation is
performed.  The routine and variable IL entries contain flags which are used
to pass information to a link-time instantiation processor.  This routine
is responsible for setting the appropriate flags.
*/
{
  a_template_instance_ptr	tip;

  db_enter(3, "update_auto_instantiation_flags");
  /* Make a second pass through all of the instantiations to set the
     flags to be passed to the link time instantiation mechanism.
     This needs to be done after all instantiations have been done
     so that the flags are in their final state. */
  tip = instantiations_required;
  for (; tip != NULL; tip = tip->next_in_instantiation_list) {
    a_symbol_ptr			instance_sym = tip->instance_sym;
    a_routine_ptr			routine;
    a_variable_ptr			variable;
    a_boolean				can_instantiate;
    a_boolean				is_static_data_member;

    /* Skip non-external function. */
    if (is_static_or_inline_template_function(tip)) continue;
    /* Get a pointer to the IL entry to be processed. */
    if (tip->instance_sym->kind == (a_symbol_kind)sk_static_data_member) {
      is_static_data_member = TRUE;
      variable = instance_sym->variant.static_data_member.variable;
    } else {
      is_static_data_member = FALSE;
      routine = instance_sym->variant.routine.ptr;
    }  /* if */
    can_instantiate = can_be_instantiated(tip);
#if DEBUG
    if (debug_level >= 4) {
      fprintf(f_debug, " already_instantiated=%d\n",
              tip->already_instantiated);
      fprintf(f_debug, " instantiation_required=%d\n",
              tip->instantiation_required);
      fprintf(f_debug, " can_instantiate=%d\n", can_instantiate);
      fprintf(f_debug, " specific_def=%d\n", tip->specific_def);
    }  /* if */
#endif /* DEBUG */
    if (is_static_data_member) {
      variable->can_be_instantiated = can_instantiate ||
                                      tip->already_instantiated;
      variable->instance_required = tip->instantiation_required;
      variable->do_not_instantiate = tip->explicit_do_not_instantiate;
    } else {
      routine->can_be_instantiated = can_instantiate ||
				     tip->already_instantiated;
      routine->instance_required = tip->instantiation_required;
      routine->do_not_instantiate = tip->explicit_do_not_instantiate;
    }  /* if */
  }  /* for */
  db_exit();
}  /* update_auto_instantiation_flags */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */


static void delayed_processing_of_can_instantiate_class_pragmas(void)
/*
The can_instantiate pragma is used to let the instantiation routines
know that certain functions or static data members can be instantiated
by a given compilation even if no other references to the entities
are seen.  This is designed to be used in cases such as a library
that internally references certain template classes which are not
used in the external interface of the library (and for which the
library does not provide instantiations).  Without the can_instantiate
pragma the instantiator would have no way of knowing how to generate
the instantiations needed to resolve the references from within the
library.
*/
#if 0
/*
There is currently a problem caused by creating a class in
tim_can_instantiate mode and then referencing the class later.

The current workaround to this problem is to instantiate the
class in tim_none mode.  This has the undesired effect that
any static data members or virtual functions will be flagged
as requiring instantiations.
*/
#endif /* 0 */
{
  a_can_instantiate_entry_ptr	ciep;

  db_enter(4, "delayed_processing_of_can_instantiate_class_pragmas");
#if 0
  /* Temporarily disabled until we solve the problem of classes that
     are referenced after the instantiation is done. */
  instantiation_mode = tim_can_instantiate;
#endif /* 0 */
  ciep = can_instantiate_list;
  while (ciep != NULL) {
    a_type_ptr	class_type = ciep->class_type;
    check_for_uninstantiated_template_class(class_type);
    ciep = ciep->next;
  }  /* while */
  db_exit();
}  /* delayed_processing_of_can_instantiate_class_pragmas */


void instantiation_wrapup(void)
/*
Performs end-of-compilation processing for template instantiation.  An
instantiation will be done if an explicit instantiation has been
requested (i.e., via a pragma) or if an instantiation is required because
the function has been referenced and we are not in "instantiate none"
mode.  Note that the pragma overrides the command line option.  Something
can appear on the list with the instantiation required flag FALSE if, for
instance, a reference that forced instantiation was followed by a
specific definition that made it unnecessary.
*/
{
  a_template_instance_ptr           tip;

  db_enter(3, "instantiation_wrapup");
  /* Now that all input has been processed including any instantiations that
     may be done, process the classes that have been put on the can
     instantiate list. */
  delayed_processing_of_can_instantiate_class_pragmas();

  /* The in_instantiation_wrapup flag indicates that we are generating
     instantiations that were requested earlier in the compilation.  When
     this flag is TRUE new instantiations are generated on the fly instead
     of being added to the end of the list.  This makes it possible to
     detect certain types of recursive instantiations that would otherwise
     be difficult to detect. */
  in_instantiation_wrapup = TRUE;
  do {
    entries_updated_during_instantiation_wrapup = FALSE;
    for (tip = instantiations_required;
         tip != NULL;
         tip = tip->next_in_instantiation_list) {
      if ((instantiation_mode == tim_all || tip->instantiation_required) &&
          !tip->already_instantiated) {
        if (should_be_instantiated(tip, /*implicit_inclusion_ok=*/TRUE)) {
          if (tip->instance_sym->kind ==
                                        (a_symbol_kind)sk_static_data_member) {
            /* Static data member definition. */
            define_template_static_data_member(tip);
          } else {
            /* Function instantiation. */
            instantiate_template_function(tip);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  } while (entries_updated_during_instantiation_wrapup);

#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (automatic_instantiation_mode) {
    /* Reset the flag that indicates that we are doing instantiation wrapup
       processing.  During automatic instantiation processing we once again
       want entries added to the instantiation required list. */
    in_instantiation_wrapup = FALSE;
    /* Do processing related to automatic instantiation processing. */
    automatic_instantiation();
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

#if AUTOMATIC_TEMPLATE_INSTANTIATION
  if (automatic_instantiation_mode) {
    update_auto_instantiation_flags();
  }  /* if */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

  db_exit();
}  /* instantiation_wrapup */


void templates_init(void)
/*
Initializations for template.
*/
{
  curr_default_args = NULL;
  instantiations_required = NULL;
  instantiations_required_tail = NULL;
  in_instantiation_wrapup = FALSE;
  entries_updated_during_instantiation_wrapup = FALSE;
  can_instantiate_list = NULL;
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  any_instantiations_required = FALSE;
  instantiation_info_file_name = NULL;
  f_instantiation_info = NULL;
  memzero((char *)instance_lookup_table, sizeof(instance_lookup_table));
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
}  /* templates_init */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
