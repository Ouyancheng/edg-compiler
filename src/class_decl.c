/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

class_decl.c -- Scanning of class declarations.

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
#include "expr.h"
#include "exprutil.h"
#include "layout.h"
#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ms_attrib.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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
  a_type_ptr	class_type;
			/* Pointer to the class that is current when the
			   declaration requiring a fixup was encountered.
			   Usually the parent class of "symbol". */
  a_symbol_ptr  symbol;
			/* Pointer to a symbol entry with which the fixup
			   is associated.  Usually, it is a member function
			   symbol, but it can be any member symbol with an
			   associated routine type for which default args
			   have been specified (a typedef or data member of
			   type ptr-to-routine). */
  a_func_info_block
		func_info;
			/* Information saved by declarator processing for
			   use in function definition processing. */
  a_def_arg_expr_fixup_ptr
		def_arg_expr_fixup_list;
			/* List of entries describing default argument
			   expression associated with parameters for the
			   current routine. */
  a_symbol_ptr	prototype_scope_symbols;
			/* Pointer to a list of prototype symbols to be
			   reactivated for scanning default arguments.
			   Used only when is_template is TRUE. */
  a_token_cache function_body_token_cache;
			/* A pointer to the token cache that describes the
			   function body. */
  a_byte_boolean
		is_specialization;
			/* TRUE if this entry is for a Microsoft mode
			   explicit specialization that appeared within
			   the class definition. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_byte_boolean
		is_partial_instantiation;
			/* TRUE if a secondary-decl source sequence entry,
			   created to represent a partial instantiation,
			   needs to be added to the source sequence list
			   during fixup. */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
  a_byte_boolean
		preserve_param_id_list;
			/* TRUE if the param_id_list in the func_info field
			   should not be deallocated with this fixup
			   (presumably because it is also pointed to by
			   another structure). */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_byte_boolean
		is_template;
			/* TRUE if this is a fixup entry for a function
			   template declaration. */
  a_byte_boolean
		is_definition;
			/* When is_template is TRUE, this is TRUE if the
			   declaration is a definition. */
} a_routine_fixup;


/*
Structure for keeping track of classes for which fixup processing
must still be done.  The fixups are deferred until the outermost
class definition is complete.
*/
typedef struct a_class_fixup {
  a_class_fixup_ptr
		next;
			/* Next in a linked list of class fixup blocks for
			   classes for which default argument fixup must be
			   done. Note that only the outermost class
			   is included on this list.  Nested classes
			   defined within another class definition are not
			   included on the list. */
  a_class_fixup_ptr
		next_in_inline_function_list;
			/* Next in a linked list of class fixup blocks for
			   classes for which inline function fixup must be
			   done. */
  a_type_ptr	class_type;
			/* Pointer to the class type to be fixed-up. */
  a_boolean	is_template_instantiation;
			/* TRUE if the class is a generated class template
                           instance. */
} a_class_fixup;

static a_boolean
		use_deferred_friend_fixup_list;
			/* TRUE if deferred friend fixups should be done at
			   the end of the translation unit instead of at the
			   point at which the friend function is first
			   referenced. */

static a_routine_fixup_ptr
		deferred_friend_fixup_list;
			/* A list of routine fixups for deferred friend
			   function fixups that should be performed at the
			   end of the translation unit.  Usually such fixups
			   are done when the routine is first referenced,
			   but in some modes they are done at the end of
			   the translation unit. */

static a_routine_fixup_ptr
		deferred_friend_fixup_list_tail;
			/* The end of the deferred_friend_fixup_list. */

/* The routine fixup entry for the current class member declaration. */
static a_routine_fixup_ptr curr_routine_fixup;

/* Previously allocated fixup entries available for reuse. */
static a_routine_fixup_ptr avail_routine_fixup;

/* Previously allocated fixup entries available for reuse. */
static a_class_fixup_ptr avail_class_fixup;

#if DEBUG
/*
Counters to track total use of memory.
*/
static unsigned long
		num_routine_fixups_allocated;

static unsigned long
		num_class_fixups_allocated;

unsigned long db_show_routine_fixups_used(unsigned long grand_total)
{
  unsigned long  num, size, total;

  db_space_used_lost("routine fixups", avail_routine_fixup,
                     num_routine_fixups_allocated, a_routine_fixup);
  return grand_total;
}  /* db_show_routine_fixups_used */


unsigned long db_show_class_fixups_used(unsigned long grand_total)
{
  unsigned long  num, size, total;

  db_space_used_lost("class fixups", avail_class_fixup,
                     num_class_fixups_allocated, a_class_fixup);
  return grand_total;
}  /* db_show_class_fixups_used */
#endif /* DEBUG */


static a_routine_fixup_ptr alloc_routine_fixup(a_type_ptr  class_type)
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
  rfp->symbol = NULL;
  rfp->class_type = class_type;
  rfp->def_arg_expr_fixup_list = NULL;
  rfp->prototype_scope_symbols = NULL;
  rfp->is_specialization = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  rfp->is_partial_instantiation = FALSE;
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
  rfp->preserve_param_id_list = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  rfp->is_template = FALSE;
  rfp->is_definition = FALSE;
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
  if (!rfp->is_template) {
    /* For templates, don't free the default argument entries because they
       are pointed to from elsewhere. */
    free_def_arg_expr_fixup(rfp->def_arg_expr_fixup_list);
  }  /* if */
  rfp->def_arg_expr_fixup_list = NULL;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (rfp->preserve_param_id_list) {
    /* Clear the param_id_list field of rfp->func_info so that the list won't
       be deallocated by done_with_func_info. */
    rfp->func_info.param_id_list = NULL;
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  done_with_func_info(rfp->func_info);
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

  check_assertion(rfp->symbol != NULL);
  while (ssep->kind == (a_scope_kind)sck_template_declaration) {
    /* Presumably a member template declaration. Move up to what should
       be the surrounding class scope. */
    --ssep;
  }  /* while */
  check_assertion(ssep->kind == (a_scope_kind)sck_class_struct_union);
  /* There's only one routine-fixup-list, and it's associated with the
     outermost enclosing class.  If this is a nested class, move up the
     scope stack to find the appropriate entry. */
  while ((ssep-1)->kind == (a_scope_kind)sck_class_struct_union) --ssep;
  /* Add the entry to the list. */
  if (ssep->last_routine_fixup == NULL) {
    (symbol_supplement_for_class(ssep->assoc_type))->routine_fixup_list = rfp;
  } else {
    ssep->last_routine_fixup->next = rfp;
  }  /* if */
  ssep->last_routine_fixup = rfp;
}  /* add_to_routine_fixup_list */


static a_boolean is_invalid_scope_for_class(void)
/*
Determine whether the current scope is valid for a class definition.  This
is used to suppress the fixup of routines declared in the class for classes
found in unexpected locations.  (It is also used for error recovery purposes.)
*/
{
  a_boolean            result = FALSE;
  a_scope_stack_entry  *ssep = &scope_stack[depth_scope_stack];

  while (ssep->kind == (a_scope_kind)sck_template_declaration) {
    /* Skip any template declaration scopes (for member template declarations)
       to find the enclosing class. */
    --ssep;
  }  /* while */
  check_assertion(ssep->kind == (a_scope_kind)sck_class_struct_union);
  while (ssep->kind == (a_scope_kind)sck_class_struct_union) {
    /* Skip all class scopes. */
    --ssep;
  }  /* while */
  /* Skip any block scopes. */
  while (ssep->kind == (a_scope_kind)sck_block) ssep--;
  /* If this is a lambda body, check whether the lambda was declared in
     an invalid scope.  If so, treat this as an invalid scope for a class. */
  if (ssep->kind == (a_scope_kind)sck_function &&
      ssep->assoc_routine->is_lambda_body) {
    a_class_symbol_supplement_ptr	cssp;
    a_type_ptr				lambda_type = (ssep-1)->assoc_type;
    cssp = symbol_supplement_for_class(lambda_type);
    result = cssp->lambda_in_invalid_scope;
  }  /* if */
  if (result) {
    /* We determined the scope is invalid above. */
  } else {
    /* So far it is valid.  Check the scope we found. */
    switch (ssep->kind) {
      case sck_template_declaration:
      case sck_enum:
        /* An invalid scope for a class definition. */
        result = TRUE;
        break;
      case sck_func_prototype:
        /* Classes normally aren't members of function prototype scopes, but
           in early GNU C++ modes they can be.  Allowing closure types (which
           early GCC versions don't support) in function prototype scopes
           causes other difficulties, so we don't emulate that GNU extension
           when lambdas are enabled. */
        result = !(gpp_mode && gnu_version < 30400 && !lambdas_enabled);
        break;
      default:
       break;
    }  /* switch */
  }  /* if */
  return result;
}  /* is_invalid_scope_for_class */


static void dispose_of_curr_routine_fixup(void)
/*
If the currently active routine fixup entry has been modified such that a
fixup pass over its tokens is required, add it to the routine fixup list for
the current class.  Otherwise free it for later use.
*/
{
  a_symbol_ptr  sym = curr_routine_fixup->symbol;
  a_boolean     needed = FALSE;

  if (is_invalid_scope_for_class()) {
    /* A class definition in an unexpected place.  Don't fixup the routines. */
  } else if (sym != NULL && !sym->is_error) {
    if (curr_routine_fixup->function_body_token_cache.first_token != NULL  ||
        curr_routine_fixup->def_arg_expr_fixup_list != NULL) {
      needed = TRUE;
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    /* If a source sequence entry to represent a partial instantiation needs
       to be added to the source sequence list, save the fixup entry for
       that, too. */
    if (sym->kind == (a_symbol_kind)sk_routine ||
        sym->kind == (a_symbol_kind)sk_member_function) {
      if (sym->variant.routine.instance_ptr != NULL &&
          sym->variant.routine.instance_ptr->partial_instantiation != NULL) {
        curr_routine_fixup->is_partial_instantiation = TRUE;
        needed = TRUE;
      }  /* if */
    }  /* if */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
  if (needed) {
    add_to_routine_fixup_list(curr_routine_fixup);
  } else {
    free_routine_fixup(curr_routine_fixup);
  }  /* if */
  curr_routine_fixup = NULL;
}  /* dispose_of_curr_routine_fixup */


void add_routine_fixup_for_specialization(a_type_ptr		class_type,
					  a_symbol_ptr		symbol,
					  a_func_info_block	*func_info,
					  a_token_cache_ptr	body_cache)
/*
Create a routine fixup entry for a specialization and add it to the routine
fixup list.  This is used for Microsoft mode specializations that can appear
in class contexts.
*/
{
  a_routine_fixup_ptr	rfp;

  rfp = alloc_routine_fixup(class_type);
  rfp->symbol = symbol;
  rfp->func_info = *func_info;
  rfp->function_body_token_cache = *body_cache;
  rfp->is_specialization = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
  /* If the specialization is in a class template, there is no syntax for an
     out-of-class definition.  E.g.:
       template<class T> struct S {
         template<class U> T f(U) {}
         template<> T f(int);  // Cannot be defined outside S.
       };
       template<class T> template<> T S::f(int) {}
          // Error: "template<>" cannot appear after "template<class T>".
  */
  rfp->func_info.is_movable_member_or_friend_def =
                     !class_type->variant.class_struct_union.is_nonreal_class;
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  add_to_routine_fixup_list(rfp);
}  /* add_routine_fixup_for_specialization */


void add_routine_fixup_for_template_decl(
		a_symbol_ptr			symbol,
		a_symbol_ptr			prototype_scope_symbols,
		a_type_ptr			class_type,
		a_boolean			is_definition,
		a_def_arg_expr_fixup_ptr	default_args)
				
/*
Create a routine fixup entry for a template function declaration that was
declared in a class scope.  symbol points to the function template symbol.
class_type is the class in which the declaration appeared.  default_args
is a list of default arguments to be fixed up.
*/
{
  a_routine_fixup_ptr	rfp;

  rfp = alloc_routine_fixup(class_type);
  rfp->symbol = symbol;
  rfp->is_template = TRUE;
  rfp->is_definition = is_definition;
  rfp->prototype_scope_symbols = prototype_scope_symbols;
  rfp->def_arg_expr_fixup_list = default_args;
  add_to_routine_fixup_list(rfp);
}  /* add_routine_fixup_for_template_decl */


static a_class_fixup_ptr alloc_class_fixup(void)
/*
Allocate (or take from the available-list) a class fixup entry and
initialize it.
*/
{
  a_class_fixup_ptr  cfp;
  
  if (avail_class_fixup != NULL) {
    /* Reuse a previously allocated entity. */
    cfp = avail_class_fixup;
    avail_class_fixup = cfp->next;
  } else {
    /* Allocate memory for a new entity. */
    cfp = (a_class_fixup_ptr)alloc_fe(sizeof(a_class_fixup));
#if DEBUG
    num_class_fixups_allocated++;
#endif /* DEBUG */
  }  /* if */
  /* Clear the entity. */
  cfp->next = NULL;
  cfp->next_in_inline_function_list = NULL;
  cfp->class_type = NULL;
  cfp->is_template_instantiation = FALSE;
  return cfp;
}  /* alloc_class_fixup */


static void free_class_fixup(a_class_fixup_ptr  cfp)
/*
Return a class fixup entry to the available list.
*/
{
  cfp->next = avail_class_fixup;
  avail_class_fixup = cfp;
}  /* free_class_fixup */


static
void add_to_class_fixup_list(a_type_ptr		class_type,
 			     a_boolean 		is_template_instantiation)
/*
Add a class fixup entry for class_type to the class fixup list.
*/
{
  a_class_fixup_ptr		cfp;
  a_class_fixup_header_ptr	cfhp;

  cfhp = curr_class_fixup_header(/*for_instantiation=*/FALSE);
  cfp = alloc_class_fixup();
  cfp->class_type = class_type;
  cfp->is_template_instantiation = is_template_instantiation;
  if (cfhp->def_arg_list == NULL) cfhp->def_arg_list = cfp;
  /* Add to the end of the default argument fixup list. */
  if (cfhp->def_arg_list_tail != NULL) {
    cfhp->def_arg_list_tail->next = cfp;
  }  /* if */
  cfhp->def_arg_list_tail = cfp;
  /* Add to the end of the inline function fixup list. */
  if (cfhp->inline_function_list == NULL) {
    cfhp->inline_function_list = cfp;
  }  /* if */
  if (cfhp->inline_function_list_tail != NULL) {
    cfhp->inline_function_list_tail->next_in_inline_function_list = cfp;
  }  /* if */
  cfhp->inline_function_list_tail = cfp;
}  /* add_to_class_fixup_list */


/*
Data structure in which to track partial overriding of an overload set of
virtual functions and to track override failures.
*/
typedef struct an_override_registry_entry *an_override_registry_entry_ptr;
typedef struct an_override_registry_entry {
  an_override_registry_entry_ptr
		next;
			/* Next in a linked list, or NULL when this is the
			   last entry on the list. */
  a_symbol_ptr	overridden_sym;
			/* Pointer to an sk_member_function or
			   sk_overloaded_function symbol a base class virtual
			   function that is a candidate to be overridden by a
			   member function declaration in the current class. */
  a_base_class_ptr
		base_class;
			/* Pointer to the base class entry in which the
			   overridden symbol appears.  This is significant only
			   when a base class occurs more than once in a
			   derivation. */
  a_symbol_list_entry_ptr
		override_failures;
			/* A linked list of entries each of which represents
			   a declaration in the derived class that has the
			   same name for not the right type, so it does
			   not succeed in overriding overridden_sym. */
  unsigned int	virtual_function_count;
			/* The number of virtual functions that may be
			   overridden -- >1 if overridden_sym is overloaded,
			   1 otherwise. */
  unsigned int	override_count;
			/* The number of declarations in the current class
			   that override virtual functions in the overload set.
			   When override_count > 0 and < virtual_function_count
			   then the overriding of the members of the overload
			   is partial. */
} an_override_registry_entry;


/* Available list of partial-override entries. */
static an_override_registry_entry_ptr avail_override_registry_entries;


static an_override_registry_entry_ptr alloc_override_registry_entry(void)
/*
Return a pointer to a new partial-override entry, after initializing its
fields.
*/
{
  an_override_registry_entry_ptr  orep;

  /* Reuse a entry from the available list; otherwise, allocate a new entry. */
  if (avail_override_registry_entries != NULL) {
    orep = avail_override_registry_entries;
    avail_override_registry_entries = avail_override_registry_entries->next;
  } else {
    orep = (an_override_registry_entry_ptr)alloc_fe(
                                           sizeof(an_override_registry_entry));
  }  /* if */
  /* Initialize its fields. */
  orep->next                   = NULL;
  orep->overridden_sym         = NULL;
  orep->base_class             = NULL;
  orep->override_failures      = NULL;
  orep->virtual_function_count = 0;
  orep->override_count         = 0;

  return orep;
}  /* alloc_override_registry_entry */


static void free_override_registry_entry(an_override_registry_entry_ptr  orep)
/*
Place a partial-override entry on the available list, so it can be reused.
*/
{
  orep->next = avail_override_registry_entries;
  avail_override_registry_entries = orep;
}  /* free_override_registry_entry */


#if IA64_ABI
/*
Data structure to record covariant overriding virtual functions.
*/
typedef struct a_covariant_override *a_covariant_override_ptr;
typedef struct a_covariant_override {
  a_covariant_override_ptr
		next;
			/* Next in a linked list, or NULL when this is the
			   last entry on the list. */
  a_base_class_ptr
		bcp;
			/* The base class entry of the member function being
			   overridden. */
  a_base_class_ptr
		adjustment_bcp;
			/* The base class entry from which the return type
			   adjustment is computed. */
  a_routine_ptr
		overridden;
			/* The base class member function being overridden. */
  a_routine_ptr
		overriding;
			/* The derived class member function that overrides
			   the base class member function. */
} a_covariant_override;

#endif /* IA64_ABI */

/*
A class-definition-state block, which tracks properties of the class as its
definition proceeds.
*/
typedef struct a_class_def_state* a_class_def_state_ptr;
typedef struct a_class_def_state {
  a_type_ptr	class_type;
			/* The class being defined. */
  a_bit_field	is_first_field:1;
			/* TRUE until the first nonstatic data member of the
			   class has been seen. */
  a_bit_field	class_aggregate_ruled_out:1;
			/* TRUE if a property of the class (e.g., the
			   declaration of a virtual function) disqualifies it
			   as an "aggregate". */
  a_bit_field   POD_ruled_out:1;
			/* TRUE if a property of the class disqualifies it as
			   a "POD". */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field   potentially_interface_like:1;
			/* TRUE if we haven't ruled out this type from being
			   an "interface-like" type (Microsoft mode only).
			   Not set until after the opening brace of the
			   definition is seen. */
  a_bit_field   current_decl_valid_in_property_or_event_def:1;
			/* TRUE if a member declaration that was just processed
			   is allowed within the braces of a C++/CLI property
			   or event.  (Currently, this is true only for member
			   function declarations, empty declarations, and some
			   error cases.  The latter only to inhibit additional
			   diagnostics.) */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	any_fields_other_than_unnamed_bitfields:1;
			/* TRUE if any fields other than unnamed bit-fields
			   are declared. */
  a_bit_field	any_friend_decls:1;
			/* TRUE if any friend declarations are encountered. */
  a_bit_field	any_const_or_ref_fields:1;
			/* TRUE if any nonstatic data members have reference
			   or const-qualified type. */
  a_bit_field	is_template_instantiation:1;
			/* TRUE if the class is a template instantiation
			   (real or nonreal), including classes nested within
			   template instances. */
  a_bit_field	is_nonreal_instantiation:1;
			/* TRUE if the class is a prototype instantiation of
			   a class template. */
  a_bit_field	is_local_class:1;
			/* TRUE if the class is local to a function. */
  a_bit_field	last_field_is_incomplete_array:1;
			/* TRUE if the most recently declared field in a
			   non-union is an incomplete array type. */
  a_bit_field	default_ctor_is_nontrivial:1;
			/* TRUE if the implied default constructor (if any)
			   must be nontrivial because the class has virtual
			   base classes, virtual functions, or base classes or
			   members with nontrivial default constructors. */
  a_bit_field	member_destruction_required:1;
			/* TRUE if the class has a direct member requiring
			   destruction. */
  a_bit_field	base_destruction_required:1;
			/* TRUE if the class has a base class requiring
			   destruction. */
  a_bit_field	ms_parenthesized_member:1;
			/* Some versions of the Microsoft compiler allow a
			   member declaration to start with a left parenthesis.
			   The matching right parenthesis can appear almost
			   anywhere in the declaration, or not at all.
			   For example:
			     struct S { (int a)[3]; };       */
  an_access_specifier
		access;
			/* The current access. */
  an_override_registry_entry_ptr
		override_registry;
			/* The registry of virtual function overrides for
			   the current class. */
  a_field_ptr	end_of_field_list;
			/* The last nonstatic data member declared for the
			   current class. */
  a_symbol_ptr	corresp_prototype_tag_sym;
			/* If the current class is a instance of a class
			   template, a pointer to the symbol for the
			   prototype instantiation of that template. */
#if IA64_ABI
  a_covariant_override_ptr
		covariant_overrides, last_covariant_override;
			/* A list keeping track of the virtual function
			   overrides that involve a covariant return type. */
#endif /* IA64_ABI */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_property_or_event_descr_ptr
		property_or_event_descr;
			/* While parsing C++/CLI property or event accessor
			   functions, this points to the associated IL
			   descriptor.  Otherwise, NULL. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} a_class_def_state;


static void initialize_class_def_state(a_type_ptr            class_type,
                                       a_class_def_state_ptr cdsp)
/*
Initialize fields of a class-definition-state block.  class_type is the
class being defined.
*/
{
  cdsp->class_type = class_type;
  cdsp->is_first_field = TRUE;
  cdsp->class_aggregate_ruled_out = FALSE;
  cdsp->POD_ruled_out = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  cdsp->potentially_interface_like = FALSE;
  cdsp->current_decl_valid_in_property_or_event_def = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  cdsp->any_fields_other_than_unnamed_bitfields = FALSE;
  cdsp->any_friend_decls = FALSE;
  cdsp->any_const_or_ref_fields = FALSE;
  cdsp->is_template_instantiation = FALSE;
  cdsp->is_nonreal_instantiation = FALSE;
  cdsp->is_local_class = FALSE;
  cdsp->last_field_is_incomplete_array = FALSE;
  cdsp->default_ctor_is_nontrivial = FALSE;
  cdsp->member_destruction_required = FALSE;
  cdsp->base_destruction_required = FALSE;
  cdsp->ms_parenthesized_member = FALSE;
  cdsp->access = (an_access_specifier)as_public;
  cdsp->override_registry = NULL;
  cdsp->end_of_field_list = NULL;
  cdsp->corresp_prototype_tag_sym = NULL;
#if IA64_ABI
  cdsp->covariant_overrides = NULL;
  cdsp->last_covariant_override = NULL;
#endif /* IA64_ABI */
#if MICROSOFT_EXTENSIONS_ALLOWED
  cdsp->property_or_event_descr = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* initialize_class_def_state */

/* Forward declaration. */
static void complete_class_definition(a_type_ptr         class_type,
                                      a_scope_depth      effective_decl_level,
                                      a_class_def_state  *class_state);


#if MICROSOFT_EXTENSIONS_ALLOWED
#define treat_declaration_as_okay_in_property_or_event(cdsp)                 \
  ((cdsp)->current_decl_valid_in_property_or_event_def = TRUE)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define treat_declaration_as_okay_in_property_or_event(cdsp)  /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if IA64_ABI

/* Available list of covariant override entries. */
static a_covariant_override_ptr  avail_covariant_overrides;

#if DEBUG

/*
Counter to track total use of memory.
*/
static unsigned long
		num_covariant_overrides_allocated;

unsigned long db_show_covariant_overrides_used(unsigned long grand_total)
{
  unsigned long  num, size, total;

  db_space_used_lost("covariant overrides", avail_covariant_overrides,
                     num_covariant_overrides_allocated, a_covariant_override);
  return grand_total;
}  /* db_show_covariant_overrides_used */

#endif /* DEBUG */

static void record_covariant_override(a_class_def_state_ptr  cdsp,
                                      a_base_class_ptr       bcp,
                                      a_base_class_ptr       adjustment_bcp,
                                      a_routine_ptr          overridden,
                                      a_routine_ptr          overriding)
/*
Allocate a covariant override record and append it to the end of the list
pointed to by cdsp.  Initialize the record with the given information.
*/
{
  a_covariant_override_ptr  cop;

  if (avail_covariant_overrides != NULL) {
    cop = avail_covariant_overrides;
    avail_covariant_overrides = avail_covariant_overrides->next;
  } else {
    cop = (a_covariant_override_ptr)alloc_fe(sizeof(a_covariant_override));
#if DEBUG
    ++num_covariant_overrides_allocated;
#endif /* DEBUG */
  }  /* if */
  cop->next = NULL;
  cop->bcp = bcp;
  cop->adjustment_bcp = adjustment_bcp;
  cop->overridden = overridden;
  cop->overriding = overriding;
  if (cdsp->covariant_overrides == NULL) {
    cdsp->covariant_overrides = cop;
  } else {
    cdsp->last_covariant_override->next = cop;
  }  /* if */
  cdsp->last_covariant_override = cop;
}  /* record_covariant_override */


static void free_covariant_overrides(a_class_def_state_ptr  cdsp)
/*
Make the covariant override records pointed to by cdsp available for future
use.
*/
{
  if (cdsp->covariant_overrides != NULL) {
    cdsp->last_covariant_override->next = avail_covariant_overrides;
    avail_covariant_overrides = cdsp->covariant_overrides;
    cdsp->covariant_overrides = NULL;
    cdsp->last_covariant_override = NULL;
  }  /* if */
}  /* free_covariant_overrides */

#endif /* IA64_ABI */

/*
A member-declaration-info block, for tracking information about a class member
declaration as it appears.
*/  
typedef struct a_member_decl_info *a_member_decl_info_ptr;
typedef struct a_member_decl_info {
  a_decl_parse_state
		decl_state;
			/* General declaration information. */
  a_decl_pos_block
		decl_pos_block;
			/* Additional source position information on the
			   declaration. */
  a_bit_field	is_first_in_declarator_list:1;
			/* TRUE for the first declarator in a declarator list,
			   FALSE thereafter. */
  a_bit_field	is_constructor:1;
			/* TRUE if the current declaration is a constructor.
			   In unusual cases this value may be different from
			   (dso_flags & DSO_CONSTRUCTOR). */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field   is_static_constructor:1;
                        /* TRUE if the current declaration is a C++/CLI static
                           constructor.  If this is TRUE, is_constructor must
                           be FALSE. */
  a_bit_field	multiple_overrides_diagnostic_issued:1;
			/* TRUE if the ec_multiple_overrides error has been
			   issued (used to avoid multiple diagnostics that
			   look identical). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	is_trivial_default_constructor:1;
			/* TRUE for an implicit declaration of a trivial
			   default constructor. */
  a_bit_field	is_destructor:1;
			/* TRUE if the current declaration is a destructor.
			   In unusual cases this value may be different from
			   (dso_flags & DSO_DESTRUCTOR). */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_bit_field	is_finalizer:1;
			/* TRUE if the current declaration is a finalizer.
			   In unusual cases this value may be different from
			   (dso_flags & DSO_FINALIZER). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_bit_field	invalid_virtual_specifier:1;
			/* TRUE when (dso_flags & DSO_VIRTUAL) is TRUE but
			   it is not a valid use of the specifier. */
  a_bit_field	is_unnamed_field:1;
			/* TRUE if the declaration is an unnamed field. */
  a_bit_field	is_anonymous_union:1;
			/* TRUE if the declaration is an anonymous union. */
  a_bit_field	is_nonstd_anonymous_union:1;
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
			/* TRUE if is_anonymous_union is TRUE but it is not
			   a standard-conforming construct. */
#else /* !ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
			/* Always FALSE. */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
  a_bit_field	return_type_def_err:1;
			/* TRUE if an error has issued on defining a class or
			   enum in a member function return type (used to
			   avoid issuing multiple errors when there is more
			   than one declarator). */
  a_bit_field	is_member_template:1;
			/* TRUE if the declaration is of a member template. */
  a_bit_field	is_bit_field:1;
			/* TRUE for a nonstatic data member that is a bit
			   field. */
  a_bit_field	is_captured_this:1;
			/* TRUE for a field that captures a this pointer. */
  a_bit_field	is_captured_pack_element:1;
			/* TRUE for a field that captures a pack element. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position
		bit_field_size_pos;
			/* Size of the start of the ":" of the bit field
			   size. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_constant	bit_field_size;
			/* Constant that represents the bit field size.  This
			   field must only be used when is_bit_field is
			   TRUE. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_symbol_list_entry_ptr
		named_overrides;
			/* A list of symbols representing named override
			   specifiers in C++/CLI mode. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} a_member_decl_info;


static void initialize_member_decl_info(a_member_decl_info_ptr mdip,
                                        a_source_position      *pos)
/*
Initialize a member-declaration-info block, used to track information about
a class member declaration as it appears.
*/
{
  init_decl_parse_state(&mdip->decl_state);
  mdip->decl_state.auto_type_allowed = auto_type_specifier_enabled;
  mdip->decl_state.trailing_return_type_allowed =
                                                trailing_return_types_enabled;
  mdip->decl_state.in_class_scope = TRUE;
  mdip->decl_state.start_pos = *pos;
  clear_decl_pos_block(&mdip->decl_pos_block);
  mdip->is_first_in_declarator_list = TRUE;
  mdip->is_constructor = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  mdip->is_static_constructor = FALSE;
  mdip->multiple_overrides_diagnostic_issued = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  mdip->is_trivial_default_constructor = FALSE;
  mdip->is_destructor = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  mdip->is_finalizer = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  mdip->invalid_virtual_specifier = FALSE;
  mdip->is_unnamed_field = FALSE;
  mdip->is_anonymous_union = FALSE;
  mdip->is_nonstd_anonymous_union = FALSE;
  mdip->return_type_def_err = FALSE;
  mdip->is_member_template = FALSE;
  mdip->is_bit_field = FALSE;
  mdip->is_captured_this = FALSE;
  mdip->is_captured_pack_element = FALSE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  mdip->bit_field_size_pos = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* bit_field_size is only set when is_bit_field is TRUE. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  mdip->named_overrides = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* initialize_member_decl_info */


/* Forward declaration. */
static a_field_ptr decl_nonstatic_data_member(
                                     a_symbol_locator        *locator,
                                     a_class_def_state_ptr   class_state,
                                     a_member_decl_info_ptr  decl_info,
                                     a_scope_depth           decl_scope_depth);


static a_lambda_capture_ptr find_lambda_capture(a_lambda_ptr   lambda,
                                                a_variable_ptr vp)
/*
If the indicated lambda already has a capture entry for the indicated
variable, return a pointer it.  Otherwise, return NULL.
*/
{
  a_lambda_capture_ptr  lcp;

  for (lcp = lambda->capture_list; lcp != NULL; lcp = lcp->next) {
    if (lcp->variable == vp) break;
  }  /* for */
  return lcp;
}  /* find_lambda_capture */


static a_field_ptr make_field_for_lambda_capture(
                                        a_lambda_ptr           lambda,
                                        a_variable_ptr         vp,
                                        a_boolean              by_reference,
                                        a_source_position_ptr  pos)
/*
Create the field of the closure class to store the capture of vp.  by_reference
is TRUE if the variable is being captured by reference.  Return the field
entry.  pos is the source position to be used as the decl_position of
the field.
*/
{
  a_field_ptr              fp;
  a_type_ptr               field_type;
  a_type_ptr               orig_field_type;
  a_boolean                is_this = FALSE, is_ref = FALSE;
  a_class_def_state_ptr    class_state;
  a_scope_stack_entry_ptr  ssep;
  a_symbol_locator         locator;
  a_member_decl_info       decl_info;
  a_scope_depth            closure_scope_depth;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean                saved_source_sequence_entries_disallowed =
                                           source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Don't issue source sequence entries for generated fields. */
  source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Find the scope stack entry for the lambda closure class. */
  for (ssep = scope_stack_entry_for(depth_scope_stack);
       !(ssep->kind == (a_scope_kind)sck_class_struct_union &&
         ssep->assoc_type == lambda->closure_class);
       ssep = previous_scope_of(ssep)) {
    check_assertion(ssep != NULL);
  }  /* for */
  class_state = ssep->class_def_state;
  /* Set up the context that is needed so that decl_nonstatic_data_member
     can be used to create the field. */
  initialize_member_decl_info(&decl_info, pos);
  closure_scope_depth = scope_depth_of(ssep);
  clear_locator(&locator, pos);
  /* "this" variables do not have associated symbols. */
  if (vp->is_this_parameter) {
    is_this = TRUE;
    decl_info.is_unnamed_field = TRUE;
  } else {
    a_symbol_ptr var_sym = symbol_for(vp);
    if (var_sym != NULL) {
      /* Create a symbol locator that can be used to declare the field. */
      locator.symbol_header = var_sym->header;
    } else {
      /* This is a rare error situation that can occur when capturing an
         element of a function parameter pack.  Use an error locator to
         proceed. */
      expect_error();
      set_to_error_locator(locator);
    }  /* if */
  }  /* if */
  orig_field_type = field_type = vp->type;
  /* If the variable is a reference, drop the reference. */
  if (is_any_reference_type(field_type)) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    check_assertion(!skip_typerefs(field_type)->variant.pointer.is_handle);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    is_ref = TRUE;
    field_type = type_pointed_to(field_type);
  }  /* if */
  if (is_this) {
    /* The field type is the type of the "this" parameter (already set
       above). */  
  } else if (by_reference) {
    /* The variable is being captured by reference.  Create a reference
       type based on the variable's type. */
    field_type = make_reference_type(field_type);
  } else if (is_ref && is_function_type(field_type)) {
    /* A variable with reference-to-function type is captured with its
       original type. */
    field_type = orig_field_type;
  } else {
    /* The variable is being captured by value.  The type is the type
       of the captured variable. */
  }  /* if */
  decl_info.decl_state.type = field_type;
  decl_info.is_captured_this = is_this;
  decl_info.is_captured_pack_element = vp->is_pack_element;
  /* The field must be private. */
  class_state->access = (an_access_specifier)as_private;
  fp = decl_nonstatic_data_member(&locator, class_state, &decl_info,
                                  closure_scope_depth);
  class_state->access = (an_access_specifier)as_public;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Restore the previous state wrt. generating source sequence entries. */
  source_sequence_entries_disallowed =
                                     saved_source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  return fp;
}  /* make_field_for_lambda_capture */


static a_lambda_capture_ptr r_add_lambda_capture(
                                       a_lambda_ptr           lambda,
                                       a_variable_ptr         vp,
                                       a_scope_depth          depth,
                                       a_boolean              is_implicit,
                                       a_boolean              by_reference,
                                       a_source_position_ptr  pos,
                                       a_boolean              *no_impl_capture)
/*
Helper routine for add_lambda_capture to handle the recursion.  Parameters
are the same, plus depth is the scope stack depth at which the capture is
being done.
*/
{
  a_lambda_capture_ptr   lcp;
  a_memory_region_number region_to_switch_back_to = curr_il_region_number;
  a_field_ptr            source_field = NULL;

  /* See if there is a lambda around the current one, which must
     capture the variable so that we can capture it at this level. */
  { a_lambda_ptr         enclosing_lambda;
    a_scope_depth        enclosing_depth;
    a_boolean            enclosing_is_implicit = TRUE;
    a_boolean            enclosing_by_reference;
    a_lambda_capture_ptr enclosing_lcp = NULL;
    enclosing_depth = scope_depth_for_local_variable_capture(vp,
                                                            depth,
                                                            &enclosing_lambda);
    if (enclosing_lambda != NULL) {
      /* There is an enclosing lambda, so generate a capture at that level
         first. */
      if ((enclosing_lcp = find_lambda_capture(enclosing_lambda, vp))
                                                                     != NULL) {
        /* There's already a capture for this variable at this level.  That
           also means the capture is taken care of in all enclosing lambdas,
           so we can stop the recursion. */
      } else if (!enclosing_lambda->has_capture_default) {
        /* No capture default, so implicit captures are not allowed.
           The caller will issue an error. */
        *no_impl_capture = TRUE;
      } else {
        /* The implicit capture is by value or by reference depending on the
           default capture setting of the enclosing lambda. */
        enclosing_by_reference = enclosing_lambda->default_is_by_reference;
        /* Make a recursive call to add the capture at the next level up. */
        enclosing_lcp = r_add_lambda_capture(enclosing_lambda, vp,
                                             enclosing_depth,
                                             enclosing_is_implicit,
                                             enclosing_by_reference,
                                             pos, no_impl_capture);
      }  /* if */
      if (enclosing_lcp != NULL) {
        /* The capture at this level copies from the closure field at the
           next level up. */
        source_field = enclosing_lcp->closure_field;
        check_assertion(source_field != NULL);
      }  /* if */
    }  /* if */
  }
  /* Switch to the memory region of the scope in which the capture will
     occur.  (For implicit captures, we're currently in the memory region of
     the point of the reference that necessitated the capture.) */
  switch_il_region(scope_stack[depth].il_memory_region);
  lcp = alloc_lambda_capture();
  /* Note that lcp->variable is set even when source_field is non-NULL.
     That's for the convenience of the front end.  The field will be
     cleared soon after it's been used to generate the capture copy code. */
  lcp->variable = vp;
  lcp->source_closure_field = source_field;
  if (is_implicit) {
    /* For implicit captures, create the capture field now.  For explicit
       captures this must wait until the closure class has been pushed. */
    lcp->closure_field = make_field_for_lambda_capture(lambda, vp,
                                                       by_reference, pos);
  }  /* if */
  lcp->capture_by_reference = by_reference;
  lcp->is_implicit = is_implicit;
  lcp->position = *pos;
  if (lambda->capture_list == NULL) {
    /* This is the first entry. */
    lambda->capture_list = lcp;
  } else {
    /* Find the last entry on the capture list so that we can add the new
       entry to the end of the list. */
    a_lambda_capture_ptr  last_lcp;
    for (last_lcp = lambda->capture_list; last_lcp->next != NULL;
         last_lcp = last_lcp->next) {}
    last_lcp->next = lcp;
  }  /* if */
  /* Restore the original memory region. */
  switch_back_to_original_region(region_to_switch_back_to);
  return lcp;
}  /* r_add_lambda_capture */


static a_lambda_capture_ptr add_lambda_capture(
                                       a_lambda_ptr           lambda,
                                       a_variable_ptr         vp,
                                       a_boolean              is_implicit,
                                       a_boolean              by_reference,
                                       a_source_position_ptr  pos,
                                       a_boolean              *no_impl_capture)
/*
Create a lambda capture entry for the lambda specified by "lambda" for the
variable vp.  is_implicit is TRUE if this is an implicit capture.
by_reference indicates if this is a by-reference or by-value capture.
Create the lambda capture entry and the associated field of the closure
class.  Add the lambda capture entry to the list of captures for "lambda"
and return a pointer to the capture entry.  pos is the source position
to be used for the capture.  If necessary, also record implicit
capture entries on any lambdas between the current one and the scope
where the variable appears.  *no_impl_capture will be returned TRUE
if an implicit capture like that can't be done because the intermediate
lambda does not allow implicit captures.
*/
{
  a_lambda_capture_ptr lcp;
  a_scope_depth        depth;

  *no_impl_capture = FALSE;
  /* Find the scope stack entry for the level at which the (innermost) capture
     will occur. */
  if (is_implicit) {
    a_lambda_ptr temp_lambda;
    depth = scope_depth_for_local_variable_capture(vp,
                                                   NO_SCOPE_DEPTH,
                                                   &temp_lambda);
    check_assertion(temp_lambda != NULL && temp_lambda == lambda);
  } else {
    /* For explicit captures (i.e., in the capture list), we're already in the
       scope of the capture. */
    depth = depth_scope_stack;
  }  /* if */
  lcp = r_add_lambda_capture(lambda, vp, depth, is_implicit, by_reference, pos,
                             no_impl_capture);
  return lcp;
}  /* add_lambda_capture */


a_lambda_capture_ptr lambda_capture_for_variable(a_variable_ptr         vp,
                                                 a_source_position_ptr  pos)
/*
vp is a local variable that is being used in a lambda.  Find or create a lambda
capture entry for it and return a pointer to it.  pos is the source position
of the variable reference.  If there is no existing capture entry, an
implicit capture will be created if the lambda allows it and if the variable
is appropriate to be captured.  If no capture can be found or created,
issue an error and return NULL.
*/
{
  a_lambda_ptr          lambda = get_current_lambda();
  a_lambda_capture_ptr  lcp;

  check_assertion(lambda != NULL);
  /* Find any existing lambda capture for this variable. */
  lcp = find_lambda_capture(lambda, vp);
  if (lcp == NULL) {
    /* No existing capture.  See if one can be created. */
    an_error_code err_code = ec_no_error;
    a_boolean     by_ref = lambda->default_is_by_reference;
    if (!check_var_for_lambda_capture(vp, /*implicit=*/TRUE, &err_code)) {
      /* The variable is not valid.  err_code explains why. */
    } else if (!lambda->has_capture_default) {
      /* No capture default, so implicit captures are not allowed. */
      err_code = ec_not_captured_local_var_in_lambda;
    } else {
      /* The variable is valid.  Add a new capture entry for it. */
      a_boolean no_impl_capture;
      lcp = add_lambda_capture(lambda, vp, /*is_implicit=*/TRUE,
                               by_ref, pos, &no_impl_capture);
      if (no_impl_capture) {
        err_code = ec_no_implicit_capture_on_enclosing_lambda;
      }  /* if */
    }  /* if */
    if (err_code != ec_no_error) {
      pos_error(err_code, pos);
    }  /* if */
  }  /* if */
  return lcp;
}  /* lambda_capture_for_variable */


static a_type_ptr make_closure_class(a_scope_depth      decl_level,
                                     a_source_position  *decl_position,
				     a_boolean		bad_scope)
/*
Create the class type that is used to represent a lambda closure.  Return
a pointer to the class type.  decl_level determines which scope the class
belongs to.  decl_position is the declaration position to be used for the
lambda.  bad_scope is TRUE if the lambda appeared in an invalid scope.

The class is created as an incomplete type.  It will be completed when its
various members have been added (call operator, constructors, destructor, and
the fields implied by the lambda's capture list).
*/
{
  a_type_ptr                     type;
  a_symbol_ptr                   sym;
  a_class_symbol_supplement_ptr  cssp;
  a_boolean                      is_prototype_instantiation = FALSE;

  /* Create an unnamed symbol for the lambda class. */
  sym = make_unnamed_tag_symbol((a_symbol_kind)sk_class_or_struct_tag,
                                decl_position);
  /* Create the type for the lambda class. */
  type = alloc_type((a_type_kind)tk_class);
  type->variant.class_struct_union.originally_unnamed = TRUE;
  class_type_supp(type)->is_lambda_closure_class = TRUE;
  set_source_corresp(&(type->source_corresp), sym);
  sym->variant.class_struct_union.type = type;
  if (is_template_dependent_context()) {
    /* If the lambda appears in a prototype instantiation context, mark it
       as a nonreal class.  Local classes in such contexts are not marked
       as prototype instantiations. */
    type->variant.class_struct_union.is_nonreal_class = TRUE;
    is_prototype_instantiation = TRUE;
  }  /* if */
  update_membership_of_class(sym, /*def_or_vacuous_decl=*/TRUE, decl_level,
                             decl_position);
  /* In some contexts the closure type is recorded with the entity associated
     with the expression containing the lambda (e.g., a closure type from a
     default argument is recorded in the associated a_param_type entry). */
  record_entity_defined_in_expression((char*)type, iek_type,
                                      /*in_file_scope=*/TRUE);
  if (!is_prototype_instantiation || prototype_instantiations_in_il) {
    add_lambda_closure_to_types_list(type, decl_level);
  } else {
    set_parent_scope_for_type(type, decl_level);
  }  /* if */
  cssp = sym->variant.class_struct_union.extra_info;
  /* Assume for now that bitwise copy is allowed for this class.  This will
     be cleared later if this is not the case. */
  cssp->construction_by_bitwise_copy_allowed = TRUE;
  cssp->lambda_in_invalid_scope = bad_scope;
  if (scope_stack[decl_level].depth_innermost_function_scope !=
                                                             NO_SCOPE_DEPTH) {
    /* If the lambda is local to an inline function or a function template
       instantiation, it is subject to the one-definition rule (ODR). */
    a_routine_ptr  prp;
    prp = scope_stack[scope_stack[decl_level].depth_innermost_function_scope]
                                                               .assoc_routine;
    check_assertion(prp != NULL);
    cssp->lambda_subject_to_trans_unit_corresp =
        prp->is_inline || (prp->is_template_function && !prp->is_specialized);
  }  /* if */
  return type;
}  /* make_closure_class */


static
a_boolean prescan_function_definition(a_token_sequence_number *first_tsn,
                                      a_token_sequence_number *last_tsn,
				      a_token_cache_ptr	      token_cache,
				      a_boolean		      is_constructor)
/*
Place the tokens for a function definition (including, perhaps, the
constructor initializer) into a token cache, to await actual processing
at a later point.  The current token is either a left brace or, when a
constructor initializer is present, a colon.  Return the token cache
pointer in token_cache and the starting and ending token sequence numbers
of the function definition in *first_tsn and *last_tsn.  is_constructor
is TRUE if the function being scanned is a constructor.
*/
{
  a_boolean          success = FALSE;

  db_enter(3, "prescan_function_definition");

  /* We don't know whether this cache will be reused or not.  Make it
     reusable here.  If it is rescanned as a nonreusable cache we
     will change it later. */ 
  clear_token_cache(token_cache, /*reusable=*/TRUE);
  /* Cache the function body, including any function try blocks and/or ctor
     initializers. */
  success = cache_function_body(token_cache, is_constructor, (a_boolean*)NULL,
                                first_tsn, last_tsn, (a_source_position*)NULL,
                                (a_source_position*)NULL);
  check_assertion_str2(curr_routine_fixup != NULL,
                       "prescan_function_definition:",
                       "curr_routine_fixup == NULL");
  curr_routine_fixup->function_body_token_cache = *token_cache;
  db_exit();
  return success;
}  /* prescan_function_definition */


void prescan_member_function_default_arg_expr(a_param_type_ptr  ptp,
					      a_boolean		is_friend,
					      unsigned long	param_number)
/*
Scan a default argument expression and link the default argument
entry onto a list in the current routine fixup entry.  "ptp" can be NULL if
the tokens should be scanned and discarded.  is_friend is TRUE if the
declaration being scanned is a friend function declaration.  param_number
specifies the position of the parameter in the parameter list.
*/
{
  a_def_arg_expr_fixup_ptr  *list;
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
  prescan_default_function_arg_expr(ptp, list, /*is_function_template=*/FALSE,
				    is_friend, param_number);
}  /* prescan_member_function_default_arg_expr */


static a_param_type_ptr corresponding_param_type(a_type_ptr        type,
                                                 a_param_type_ptr  ptp)
/*
"ptp" describes the n-th parameter of some unspecified routine type.  This
routine assumes "type" has a compatible routine type and returns its n-th
parameter type description.
*/
{
  a_routine_type_supplement_ptr  rtsp = type->variant.routine.extra_info;
  sizeof_t                       n_params = 0, n_params_remaining = 0,
                                 param_pos;

  /* Count the position of the given a_param_type entry (from the right). */
  for (; ptp != NULL; ptp = ptp->next) { ++n_params_remaining; }
  /* Count the total number of parameters. */
  ptp = rtsp->param_type_list;
  for (; ptp != NULL; ptp = ptp->next) { ++n_params; }
  /* Skip the right number. */
  ptp = rtsp->param_type_list;
  param_pos = n_params - n_params_remaining;
  for (; param_pos--;) { ptp = ptp->next; }
  return ptp;
}  /* corresponding_param_type */


static a_boolean fixup_is_for_friend(a_routine_fixup_ptr  rfp)
/*
Return TRUE if the given routine fixup is for a friend declaration.
*/
{
  a_symbol_ptr  sym = rfp->symbol;
  a_boolean     is_friend = FALSE;

  if (!sym->is_class_member) {
    is_friend = TRUE;
  } else {
    a_type_ptr  parent_class = sym_parent_class(sym);
    is_friend = !same_entities(parent_class, rfp->class_type);
  }  /* if */
  return is_friend;
}  /* fixup_is_for_friend */


void default_argument_fixup_for_class(a_type_ptr  class_type,
				      a_boolean   is_template_based,
				      a_boolean   template_second_pass)
/*
Process the default argument expressions for the indicated class.
is_template_based is TRUE if the class is the result of a template
instantiation.  When nonclass prototype instantiations are performed this
routine is called twice for each class.   The second pass (when
template_second_pass is TRUE) does prototype instantiations of default
arguments for member function templates of both normal and template classes,
and for member functions of template classes.
*/
{
  a_routine_fixup_ptr               rfp;
  a_def_arg_expr_fixup_ptr          daefp;
  a_type_ptr                        curr_scope_class_type = NULL;
  a_class_symbol_supplement_ptr     cssp;
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp;
  a_boolean                         is_real_template_instantiation = FALSE;
  a_boolean                         is_nonreal_template_instantiation = FALSE;
  a_boolean                         is_friend;
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_scope_depth                     scope_depth = NO_SCOPE_DEPTH;
  a_source_sequence_entry_ptr       orig_insert_point;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_enter(3, "default_argument_fixup_for_class");
  /* Go through all the routine fixup entries created for the class twice,
     once for the default arguments, then for the function bodies.  This
     is desirable to control dependencies, e.g.:
       class A {
         void f1() { f2(); }
         static void f2(int i=1) {}
       };
     Here we want to know about the default arguments for f2 before
     processing the call to it in the body of f1.  In between default arg
     processing and inline function body processing, recursive calls are
     made to handle nested classes.  This assures that all default arguments
     are processed, even those belonging to member functions of nested
     classes, before any inline member function bodies are scanned. */
  /* This does not, however, fix dependency problems in a case like this:
       class A {
         int f1(int i=f2()) { return i; }
         static int f2(int i=1) { return i; }
       };
     where the default argument for f2 must be dealt with before that of
     f1.  Currently, if the declaration for f1 precedes that of f2, we
     issue an error ("too few arguments in function call"), but vice versa
     is okay.  Is this order dependence appropriate?  Or should the
     language impose some restrictions on what can appear in a default
     argument expression?
  */
  /* First go though the routine fixup entries and scan the default
     argument expressions. */
  cssp = symbol_supplement_for_class(class_type);
  if ((rfp = cssp->routine_fixup_list) != NULL &&
      !(template_second_pass ? cssp->default_arg_fixup_pass_2_started
                             : cssp->default_arg_fixup_pass_1_started)) {
    if (template_second_pass) {
      cssp->default_arg_fixup_pass_2_started = TRUE;
    } else {
      cssp->default_arg_fixup_pass_1_started = TRUE;
    }  /* if */
#if DEBUG
    if (debug_level >= 3) {
      fputs("default-arg fixup for class \"", f_debug);
      db_type_name(class_type);
      fputs("\"\n", f_debug);
    }  /* if */
#endif /* DEBUG */
    if (class_type->variant.class_struct_union.is_template_class &&
        class_type->variant.class_struct_union.is_nonreal_class) {
      is_nonreal_template_instantiation = TRUE;
    } else if (is_template_based) {
      is_real_template_instantiation = TRUE;
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    /* If a template instantiation is triggered by a default argument,
       the point at which the instantiation should be represented in the
       source sequence list is problematic when the C++-generating back end
       is being called.  Since a explicit specialization is put out to
       represent the instantiation, there are potentially conflicting
       constraints: an explicit specialization may not appear in a class
       scope; it must appear before the default argument that references it;
       and it must appear after the declaration of types used in its template
       arguments.  When instantiations_permitted_in_class_src_seq_list is
       TRUE, the first constraint is ignored and the instantiation is
       entered immediately before the function declaration containing the
       default argument.  When it is FALSE, the third constraint is ignored
       and the instantiation is entered in the innermost namespace scope
       containing the class declaration. */
    if (!instantiations_permitted_in_class_src_seq_list &&
        !is_nonreal_template_instantiation &&
        !class_type->source_corresp.is_local_to_function &&
        !template_second_pass) {
      /* Set the instantiation insert point so that it precedes the class
         definition. */
      if (class_type->source_corresp.source_sequence_entry != NULL) {
#if DEBUG
        if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
          fputs("forcing ss-entries for instantiations triggered ", f_debug);
          fputs("by default args to precede\n  class \"", f_debug);
          db_type_name(class_type);
          fputs("\"\n", f_debug);
        }  /* if */
#endif /* DEBUG */
        /* Determine the scope into which instantiations will be inserted.
           It will be the innermost active namespace scope that is a parent
           of class_type. */
        scope_depth = scope_depth_for_class_ss_list(class_type);
        if (scope_depth != NO_SCOPE_DEPTH) {
          /* Save the instantiation insert point in that scope (so it can be
             restored later) and replace it with the source sequence entry for
             the class, so that specializations will be inserted before the
             class in the source sequence list. */
          orig_insert_point =
                 scope_stack[scope_depth].ss_list_instantiation_insert_point;
          scope_stack[scope_depth].ss_list_instantiation_insert_point =
                           class_type->source_corresp.source_sequence_entry;
        }  /* if */
        if (depth_innermost_namespace_scope != NO_SCOPE_DEPTH) {
          /* Reactivate the class (and the file-scope memory region). */
          push_class_and_template_reactivation_scope(
                     class_type, is_template_based, /*extend_namespace=*/TRUE);
          curr_scope_class_type = class_type;
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    for (; rfp != NULL; rfp = rfp->next) {
      a_boolean	fixup_class_is_real_template_instantiation =
                                                is_real_template_instantiation;
      a_boolean	fixup_class_is_nonreal_template_instantiation =
					     is_nonreal_template_instantiation;
      if (is_nonreal_template_instantiation &&
          rfp->class_type->variant.class_struct_union.is_specialized) {
        /* In Microsoft mode a class specialization may appear in a prototype
           instantiation.  Process such a class as a real instantiation. */
        fixup_class_is_real_template_instantiation = TRUE;
        fixup_class_is_nonreal_template_instantiation = FALSE;
      }  /* if */
      daefp = rfp->def_arg_expr_fixup_list;
      if (rfp->is_template) {
        /* A routine fixup for a template function declaration.  The default
           arguments have already been attached to the template.  Do the
           prototype instantiations of those default arguments.  This
           is not done for real template instantiations -- they get their
           default information from the information saved during the
           prototype instantiation. */
        if (!fixup_class_is_real_template_instantiation) {
          sym = rfp->symbol;
          if (daefp != NULL && nonclass_prototype_instantiations &&
              template_second_pass) {
            default_arg_prototype_instantiation(
                                     sym, daefp, rfp->prototype_scope_symbols,
                                     /*update_declared_type=*/TRUE);
          }  /* if */
        }  /* if */
      } else if (daefp != NULL) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
        a_boolean  do_declared_type_fixup = is_function_symbol(rfp->symbol);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        /* There is at least one default argument associated with this
           function type. */
        sym = rfp->symbol;
#if DEBUG
        if (debug_level >= 3) {
          db_symbol(sym, "scanning default args for ", 2);
        }  /* if */
#endif /* DEBUG */
        is_friend = is_function_symbol(sym) && fixup_is_for_friend(rfp);
        if (fixup_class_is_nonreal_template_instantiation) {
          /* Prototype instantiation. */
          if (sym->kind == (a_symbol_kind)sk_member_function && !is_friend) {
            if (!template_second_pass) {
              a_def_arg_expr_fixup_ptr  daefp_end;
              a_def_arg_expr_fixup_ptr  daefp_tmp = daefp;
              a_cached_token_ptr        first_token_ptr;
              /* Make sure that all of the default arguments are at the end of
                 the parameter list. */
              first_token_ptr = daefp->cache.tokens.first_token;
              check_assertion(first_token_ptr != NULL);
              check_default_args_for_param_type(
                                           daefp->param_type,
                                           &first_token_ptr->source_position);
              /* Update the template declaration information to refer to
                 the declaration information of the enclosing class
                 template. */
              tssp = symbol_supplement_for_class(rfp->class_type)->
                                                                 template_info;
              while (daefp_tmp != NULL) {
                check_assertion(tssp->cache.decl_info != NULL);
                daefp_tmp->cache.decl_info = tssp->cache.decl_info;
                daefp_tmp = daefp_tmp->next;
              }  /* while */
              /* Link the default argument list from the template supplement
                 onto the end of the list of current default arguments.  The
                 list in the supplement must be for arguments that follow the
                 new list (otherwise it would be an error).  Find the end
                 of the current list and link the existing list to the end. */
              daefp_end = daefp;
              if (daefp_end != NULL) {
                /* Find the end of the list of new default argument entries. */
                while (daefp_end->next != NULL) {
                  daefp_end = daefp_end->next;
                }  /* while */
                tssp = template_supplement_for_symbol(sym);
                daefp_end->next = tssp->variant.function.def_arg_expr_list;
                tssp->variant.function.def_arg_expr_list = daefp;
              }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
              /* The declared type fixup is suppressed on the first pass
                 for templates when nonclass_prototype_instantiations are
                 being performed. */
              do_declared_type_fixup = !nonclass_prototype_instantiations;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
            } else /* if (template_second_pass) */ {
              if (nonclass_prototype_instantiations) {
                /* Do the prototype instantiations of the default arguments. */
                default_arg_prototype_instantiation(
                           sym, daefp, rfp->func_info.prototype_scope_symbols,
                           /*update_declared_type=*/FALSE);
              }  /* if */
            }  /* if */
            if (template_second_pass || !nonclass_prototype_instantiations) {
              /* On the last pass clear the default argument fixup list to
                 prevent it from being freed. */
              rfp->def_arg_expr_fixup_list = NULL;
            }  /* if */
            /* Make sure no further processing will be done here. */
            daefp = NULL;
          } else {
            /* The default arg token cache is discarded for declarations
               that are not member functions of the current class (including
               friend declarations). */
            for (; daefp != NULL; daefp = daefp->next) {
              discard_token_cache(&daefp->cache.tokens);
            }  /* for */
          }  /* if */
          goto fixup_declared_type;
        }  /* if */
        /* The rest of this routine is not needed for the second pass. */
        if (template_second_pass) continue;
        if (!same_entities(curr_scope_class_type, rfp->class_type)) {
          if (curr_scope_class_type != NULL) {
            /* Pop the reactivated class scope from the scope stack. */
            pop_class_reactivation_scope();
          }  /* if  */
          /* Reactivate the class. */
          push_class_and_template_reactivation_scope(
                rfp->class_type, is_template_based, /*extend_namespace=*/TRUE);
          curr_scope_class_type = rfp->class_type;
        }  /* if */
        if (fixup_class_is_real_template_instantiation &&
            sym->kind == (a_symbol_kind)sk_member_function && !is_friend) {
          /* This is a real template instantiation and the default argument
             list is for a member function of the class being instantiated.
             Use the cache from the template symbol supplement instead of
             the one that has just been created. */
          for (; daefp != NULL; daefp = daefp->next) {
            discard_token_cache(&daefp->cache.tokens);
          }  /* for */
          /* Update the default argument information for this function based
	     on the information about the template from which it was
             generated.  Note that the default argument values are not
             actually scanned at this point.  They will be scanned later,
             only if their value(s) are needed. */
          if (sym->variant.routine.instance_ptr == NULL) {
            /* Some sort of error condition. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
            do_declared_type_fixup = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          }  /* if */
        } else {
          /* A friend (or other non-member-function) declaration in a real
             template instantiation or any declaration in an ordinary
             (nontemplate) class. */
          /* The function prototype scope should be reactivated and its symbols
             reentered because parameter names hide names from enclosing scopes
             and, moreover, may not be used in default argument expressions
             (ARM 8.2.6).  In Microsoft mode, member function parameters are
             not reactivated however (but friend function parameters are). */
          (void)push_scope((a_scope_kind)sck_func_prototype,
                           rfp->func_info.scope_number,
                           underlying_function_type(rfp->symbol),
                           (a_routine_ptr)NULL);
          if (rfp->func_info.prototype_scope_symbols != NULL &&
              !(microsoft_mode && !is_friend)) {
            reactivate_prototype_scope_symbols(
                                      rfp->func_info.prototype_scope_symbols);
          }  /* if */
          /* Loop through the list of default arg expression fixup entries. */
          for (; daefp != NULL; daefp = daefp->next) {
            /* It's a default arg expression that needs to be rescanned. */
            /* Let get_token know about the cache.  Default argument errors
               are not checked here for friend declarations because they
               will have been checked by decl_routine. */
            a_param_type_ptr  ptp = daefp->param_type;
            rescan_cached_tokens(&daefp->cache.tokens);
            if (is_friend) {
              /* Because a friend declaration can be a redeclaration, its
                 routine type may have changed during type reconciliation,
                 which occurs after the fixups are created.  That situation
                 happens for example in the case:
                    void f(int, int, int) {}
                    class C { friend void f(int, int = 0, int = 0); };
                 */
              ptp = corresponding_param_type(routine_symbol_type(sym), ptp);
            }  /* if */
            delayed_scan_of_default_arg_expr(ptp, sym,
                                            /*check_for_errors=*/!is_friend);
          }  /* for */
          /* Pop the reactivated function prototype scope off the stack. */
          pop_scope();
        }  /* if */
fixup_declared_type: ;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* The default arguments on the routine type have been fixed up.
           If the declared type refers to a different type entry, copy the
           default argument expressions to the declared type. */
        if (do_declared_type_fixup && rfp->func_info.declared_type != NULL) {
          copy_routine_type_default_args(routine_symbol_type(sym),
                                         rfp->func_info.declared_type);
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
    }  /* for */
    if (curr_scope_class_type != NULL) {
      /* Pop the reactivated class scope from the scope stack. */
      pop_class_reactivation_scope();
    }  /* if  */


#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    if (scope_depth != NO_SCOPE_DEPTH) {
      scope_stack[scope_depth].ss_list_instantiation_insert_point =
                                                        orig_insert_point;
    }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */


  }  /* if */
  db_exit();
}  /* default_argument_fixup_for_class */


static void defer_routine_fixup_until_use(a_routine_fixup_ptr	rfp)
/*
In some modes, friend functions defined in a class template are treated
much like a member function of such a class.  The body is only processed
if needed.  Save a pointer to the routine fixup entry in the routine
entry.  The fixup will be completed later, if needed. "rfp" is its
routine fixup entry for the definition to be deferred.
*/
{
  rfp->symbol->variant.routine.ptr->routine_fixup = rfp;
  rfp->next = NULL;
}  /* defer_routine_fixup_until_use */


static void deferred_friend_function_fixup(a_routine_fixup_ptr	rfp)
/*
Does the fixup on the friend function that is otherwise done when the
enclosing class is instantiated.  Normally, the fixup of such routines
is deferred until the first use of the routine.  But in some modes the
fixup is further postponed until the end of the translation unit.  This
routine is called by add_to_deferred_friend_fixup_list in the former case
and by process_deferred_friend_fixup list in the latter.  This routine is
also used for Microsoft in-class member function template specializations.
*/
{
  a_routine_ptr                rp = rfp->symbol->variant.routine.ptr;
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_scope_depth                scope_depth = NO_SCOPE_DEPTH;
  a_source_sequence_entry_ptr  insert_point;
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_enter(3, "deferred_friend_function_fixup");
  /* Reactivate the scope containing the function definition. */
  push_class_and_template_reactivation_scope(rfp->class_type,
					     /*is_template_based=*/TRUE,
					     /*extend_namespace=*/TRUE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  if (!scope_stack[DEPTH_OF_FILE_SCOPE].source_sequence_entries_disallowed) {
    a_type_ptr                   tp;
    a_source_sequence_entry_ptr  ssep;

    /* Turn on the generation of source sequence entries. */
    source_sequence_entries_disallowed = FALSE;
    /* Now put out the source sequence entry for the routine, after
       clearing the source-sequence pointer to be sure it will be
       reset. */
    rp->source_corresp.source_sequence_entry = NULL;
    add_to_source_sequence_list((char *)rp, (an_il_entry_kind)iek_routine);
    /* Set the declared type in the source sequence entry.  The default
       args are not copied because they are associated with the declared type
       of the secondary-decl entry that appeared in the class definition. */
    tp = copy_routine_type_with_param_types(rfp->func_info.declared_type,
                                            /*copy_default_args=*/FALSE);
    set_routine_declared_type(rp, tp);
    scope_depth = scope_depth_for_class_ss_list(rfp->class_type);
    if (scope_depth != NO_SCOPE_DEPTH) {
      ssep = rp->source_corresp.source_sequence_entry;
      /* Save the current instantiation insert point before resetting it.
         It will be restored later. */
      insert_point = scope_stack[scope_depth].
                             ss_list_instantiation_insert_point;
      /* Assure that instantiations triggered within the body of the
         relocated function are recorded in the source-sequence list
         immediately before it. */
      scope_stack[scope_depth].ss_list_instantiation_insert_point = ssep;
      if (insert_point != NULL || scope_depth != depth_scope_stack) {
        /* Move the source sequence entry that was just entered to an
           appropriate spot.  (Normally, the reference that triggers the
           instantiation of the function appears inside a function body,
           but that will never be the right place to which to anchor the
           definition.) */
        f_move_src_seq_list(ssep, ssep, depth_scope_stack,
                            insert_point, scope_depth);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Let get_token know about the cache. */
  rescan_cached_tokens(&rfp->function_body_token_cache);
  /* Scan the function body. */
  scan_function_body(rp, &rfp->func_info,
                     (SFB_NO_CLASS_REACTIVATION |
                      SFB_NEW_STRUCT_STMT_STACK_REQUIRED |
                      SFB_PRAGMA_PACK_IS_LOCAL));
  /* scan_function_body does not scan past the right brace. */
  if (curr_token == tok_rbrace) (void)get_token();
  /* In the normal case the current token should be end_of_source,
     which was inserted to mark the end of the cached token stream.
     If necessary, keep flushing until end-of-source is found. */
  flush_past_token_cache_terminator();
  /* Make sure various flags are set correctly in the routine entry.  This
     won't have been done before, since decl_routine was called for a
     declaration, not a definition.  Note that the defined flag must be
     set after scan_function_body has been called as in some cases it
     tests the defined flag to detect duplicate definitions. */
  rp->defined = TRUE;
  ((a_symbol_ptr)rp->source_corresp.assoc_info)->defined = TRUE;
  if (rp->is_in_class_specialization) {
    /* The fixup was for an in-class specialization, not for a friend
       declaration: Nothing needs to be done. */
  } else {
    rp->defined_in_friend_decl = TRUE;
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  if (scope_depth != NO_SCOPE_DEPTH) {
    scope_stack[scope_depth].ss_list_instantiation_insert_point = insert_point;
  }  /* if */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Pop the reactivated class scope from the scope stack. */
  pop_class_reactivation_scope();
  if (secondary_translation_unit_seen()) {
    /* The fact that the function now has a definition may make it the
       canonical entry when dealing with multiple translation units. */
    establish_function_instantiation_corresp(rp);
  }  /* if */
  db_exit();
}  /* deferred_friend_function_fixup */


void add_to_deferred_friend_function_fixup_list(a_routine_fixup_ptr	rfp)
/*
When deferring the fixup of friend functions, this routine is called when
a friend function defined in a class template is first used.  This either
does the fixup of the routine or adds it to a list of fixups to be done at
the end of the translation unit.  This routine is also used for Microsoft
in-class member function template specializations.
*/
{
  a_routine_ptr                rp = rfp->symbol->variant.routine.ptr;

  /* Reset the routine fixup pointer in the routine to prevent this
     process from being attempted again. */
  rp->routine_fixup = NULL;
  /* use_deferred_friend_fixup_list is TRUE in some modes when the deferred
     fixup of friend functions should be postponed until the end of the
     translation unit.  In such modes, the flag is cleared once the fixups
     have completed so that any additional fixups that are needed will be done
     when this routine is called. */
  if (use_deferred_friend_fixup_list) {
    if (deferred_friend_fixup_list == NULL) deferred_friend_fixup_list = rfp;
    if (deferred_friend_fixup_list_tail != NULL) {
      deferred_friend_fixup_list_tail->next = rfp;
    }  /* if */
    deferred_friend_fixup_list_tail = rfp;
  } else {
    deferred_friend_function_fixup(rfp);
  }  /* if */
}  /* add_to_deferred_friend_function_fixup_list */


void process_deferred_friend_fixup_list(void)
/*
Do the fixup for any entries on the deferred friend function fixup list.
*/
{
  a_routine_fixup_ptr	rfp;

  for (rfp = deferred_friend_fixup_list; rfp != NULL; rfp = rfp->next) {
    deferred_friend_function_fixup(rfp);
  }  /* for */
  if (deferred_friend_fixup_list != NULL) {
    /* The friend fixups could cause additional instantiations to be done.
       Notify the instantiation wrapup process that it should check for
       additional instantiations. */
    additional_instantiation_wrapup_processing_needed();
  }  /* if */
  /* Don't use the list for any additional friend fixups that may be
     needed. */
  use_deferred_friend_fixup_list = FALSE;
  /* Clear the list pointers so that this routine can harmlessly be called
     again. */
  deferred_friend_fixup_list = NULL;
  deferred_friend_fixup_list_tail = NULL;
}  /* process_deferred_friend_fixup_list */


static void inline_function_fixup_for_class(a_type_ptr  class_type,
                                            a_boolean   is_template_based)
/*
Process the inline function definitions for the indicated class.  If the
class contains nested classes, call this routine recursively for each
nested class.
*/
{
  a_routine_fixup_ptr               rfp, next_rfp;
  a_type_ptr                        curr_scope_class_type = NULL;
  a_class_symbol_supplement_ptr     cssp;
  a_symbol_ptr                      sym;
  a_template_symbol_supplement_ptr  tssp;
  a_boolean                         is_real_template_instantiation = FALSE;
  a_boolean                         is_nonreal_template_instantiation = FALSE;
  a_boolean                         is_friend;
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_scope_depth                     scope_depth;
  a_source_sequence_entry_ptr       orig_insert_point, insert_point;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_enter(3, "inline_function_fixup_for_class");
  /* First go though the routine fixup entries and scan the default
     argument expressions. */
  cssp = symbol_supplement_for_class(class_type);
  if (cssp->routine_fixup_list != NULL) {
#if DEBUG
    if (debug_level >= 3) {
      fputs("inline function fixup for class \"", f_debug);
      db_type_name(class_type);
      fputs("\"\n", f_debug);
    }  /* if */
#endif /* DEBUG */
    if (class_type->variant.class_struct_union.is_template_class &&
        class_type->variant.class_struct_union.is_nonreal_class) {
      is_nonreal_template_instantiation = TRUE;
    } else if (is_template_based) {
      is_real_template_instantiation = TRUE;
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    insert_point = orig_insert_point = NULL;
    scope_depth = scope_depth_for_class_ss_list(class_type);
    if (scope_depth != NO_SCOPE_DEPTH) {
      orig_insert_point = scope_stack[scope_depth].
                                        ss_list_instantiation_insert_point;
      scope_stack[scope_depth].ss_list_instantiation_insert_point = NULL;
    }  /* if */
    if (!class_type->source_corresp.is_local_to_function &&
        !is_nonreal_template_instantiation) {
      /* Temporarily remove source sequence entries, if any that have been
         entered after the end-of-construct entry for the class that was just
         defined.  Here's an example why:  Sometimes the definition of a
         member or friend functions is represented by a source sequence
         entry that is added after the end of the class body.  Moreover, in
         a case like this:
           class A {
             int friend f() { ... };
           } x = f();
         the definition of f must be moved to a position that precedes the
         declaration of x even while following the declaration of A. */
      a_source_sequence_entry_ptr     tail;
      a_src_seq_end_of_construct_ptr  sseocp;

      /* Check the end of the source sequence list. */
      if (scope_depth != NO_SCOPE_DEPTH &&
          class_type->source_corresp.source_sequence_entry != NULL) {
        /* Unless the last entry on the source sequence list is an
           end-of-construct entry that corresponds to the end of the
           definition of class_type, back up until it's found. */
        for (tail = scope_stack[scope_depth].end_of_source_sequence_list;;
             tail = tail->prev) {
          check_assertion(tail != NULL);
          if (ss_entry_kind(tail) ==
                  (an_il_entry_kind)iek_src_seq_end_of_construct) {
            sseocp = (a_src_seq_end_of_construct_ptr)tail->entity.ptr;
            if (sseocp->entity.ptr == (char *)class_type) {
#if DEBUG
              if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
                fputs("adding fixup entries following body of class \"",
                      f_debug);
                db_type_name(class_type);
                fputs("\"\n", f_debug);
              }  /* if */
#endif /* DEBUG */
              insert_point = tail->next;
              break;
            }  /* if */
          }  /* for */
        }  /* if */
      }  /* if */
    }  /* if */
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    for (rfp = cssp->routine_fixup_list; rfp != NULL; rfp = rfp->next) {
      if (rfp->is_partial_instantiation) {
        /* Add a secondary-decl source sequence entry to the source sequence
           list to represent a partial instantiation. */
        if (curr_scope_class_type != rfp->class_type) {
          if (curr_scope_class_type != NULL) {
            /* Pop the reactivated class scope from the scope stack. */
            pop_class_reactivation_scope();
          }  /* if  */
          /* Reactivate the class. */
          push_class_and_template_reactivation_scope(
                rfp->class_type, is_template_based, /*extend_namespace=*/TRUE);
          curr_scope_class_type = rfp->class_type;
        }  /* if */
        if (!source_sequence_entries_disallowed) {
          a_template_instance_ptr      tip;
          a_source_sequence_entry_ptr  ssep;

          tip = rfp->symbol->variant.routine.instance_ptr;
          check_assertion(tip != NULL);
          ssep = tip->partial_instantiation;
          if (ssep == NULL) {
            /* This can happen when the routine has been instantiated because
               of a friend declaration (see add_source_sequence_entry_for_-
               partial_instantiation). */
          } else {
            check_assertion(scope_depth != NO_SCOPE_DEPTH);
            tip->partial_instantiation = NULL;
            insert_src_seq_list(ssep, ssep, scope_depth, insert_point);
            rfp->symbol->variant.routine.ptr
                       ->source_corresp.source_sequence_entry = ssep;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Go through the routine fixup entries to scan inline function bodies. */
    /*lint --e{850} rfp modified in loop (LINTBUG) */
    for (rfp = cssp->routine_fixup_list; rfp != NULL; rfp = next_rfp) {
      a_boolean	in_class_specialization =
                   rfp->class_type->
                         variant.class_struct_union.is_in_class_specialization;
      next_rfp = rfp->next;
      if (rfp->function_body_token_cache.first_token != NULL ||
          (rfp->is_template && rfp->symbol->defined)) {
        sym = rfp->symbol;
#if DEBUG
        if (debug_level >= 3) {
          db_symbol(sym, "scanning function body for ", 2);
        }  /* if */
#endif /* DEBUG */
        is_friend = fixup_is_for_friend(rfp);
        if (!same_entities(curr_scope_class_type, rfp->class_type)) {
          if (curr_scope_class_type != NULL) {
            /* Pop the reactivated class scope from the scope stack. */
            pop_class_reactivation_scope();
          }  /* if  */
          /* Reactivate the class. */
          push_class_and_template_reactivation_scope(
                rfp->class_type, is_template_based, /*extend_namespace=*/TRUE);
          curr_scope_class_type = rfp->class_type;
        }  /* if */
        if ((is_real_template_instantiation &&
             !is_friend && !rfp->is_specialization &&
             !in_class_specialization)) {
          /* Discard the token cache for member functions of template
             classes -- instantiate_function_template does its thing based
             on the tokens saved during prototype instantiation. */
          discard_token_cache(&rfp->function_body_token_cache);
        } else if (!nonclass_prototype_instantiations &&
                   is_nonreal_template_instantiation &&
                   (is_friend || rfp->is_specialization)) {
          /* During class prototype instantiations when not doing function
             prototype instantiations, friend definitions and Microsoft
             mode specializations are just discarded. */
          discard_token_cache(&rfp->function_body_token_cache);
        } else if (defer_friend_instantiation &&
                   is_real_template_instantiation &&
                   is_function_symbol(sym) &&
                   (is_friend || rfp->is_specialization)) {
          /* In some modes friend functions defined in a class template
             are treated  much like a member function of such a class.
             The body is only processed if needed.  This special treatment
             is also extended to Microsoft mode specializations that are
             defined within the class.  Note that this processing is only
             needed for friends and specializations declared within class
             templates, not for declarations in normal classes. */
          defer_routine_fixup_until_use(rfp);
          /* Set rfp to NULL to prevent it from being freed below. */
          rfp = NULL;
        } else if (is_real_template_instantiation &&
                   rfp->is_template && is_friend) {
          /* A friend template in a real instantiation.   Ignore this.
             The friend from the prototype instantiation will be used. */
        } else if (rfp->is_template) {
          /* A function template declared in a class scope. */
          if (prototype_instantiation_should_be_done_for_function(sym) &&
              !defer_function_prototype_instantiations) {
            if (rfp->is_definition) {
              /* Do the prototype instantiation of the function body. */
              function_prototype_instantiation(sym);
            }  /* if */
            if (is_friend) {
              tssp = template_supplement_for_symbol(sym);
              if (rfp->is_definition) {
                tssp->variant.function.routine->defined_in_friend_decl = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
        } else if (is_nonreal_template_instantiation &&
                   !scope_stack[depth_scope_stack].inside_local_class &&
                   !is_friend && !rfp->is_specialization &&
                   !in_class_specialization) {
          /* Prototype instantiation -- copy the cache for member functions.
             (Note that member functions of local classes of a function
             prototype instantiation are nonreal, but they are not themselves
             prototype instantiations.)  In-class specializations are handled
             by the normal fixup process below. */
          tssp = template_supplement_for_symbol(sym);
          tssp->cache.tokens = rfp->function_body_token_cache;
          clear_token_cache(&rfp->function_body_token_cache,
                           /*reusable=*/TRUE);
          /* Also copy the func_info block.  Null out the param-id pointer
             in the fixup entry so that the list won't be freed when
             free_routine_fixup is called. */
          tssp->variant.function.func_info = rfp->func_info;
          rfp->func_info.param_id_list = NULL;
          if (prototype_instantiation_should_be_done_for_function(sym) &&
              !defer_function_prototype_instantiations) {
            /* Do the prototype instantiation of the member function body. */
            function_prototype_instantiation(sym);
          }  /* if */
        } else {
          /* Normal case. */
          a_routine_ptr  rp = rfp->symbol->variant.routine.ptr;

#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if !NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
          {
          a_boolean  saved_source_sequence_entries_disallowed =
                                         source_sequence_entries_disallowed;
          if (is_real_template_instantiation && is_friend) {
            /* Suppress the source sequence representation for the body of a
               friend definition within a class instantiation. */
            source_sequence_entries_disallowed = TRUE;
          }  /* if */
#endif /* !NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
          if (rfp->func_info.is_movable_member_or_friend_def &&
              !source_sequence_entries_disallowed) {
            /* Within the class definition a secondary-decl source sequence
               entry was put out for the member or friend function
               definition.  The primary source sequence entry was deferred
               till now, when the class definition is complete. */
            a_source_sequence_entry_ptr  ssep;
            if (!sym->is_error) {
              ssep = rp->source_corresp.source_sequence_entry;
              check_assertion(ss_entry_kind(ssep) ==
                                (an_il_entry_kind)iek_src_seq_secondary_decl);
              /* Since the definition is being moved out of the class, the
                 associated "name reference" is no longer "primary".
                 Instead, it should be associated with the secondary source
                 sequence entry. */
              if (rp->source_corresp.name_references != NULL) {
                a_name_reference_ptr          name_ref =
                                           rp->source_corresp.name_references;
                for (; name_ref != NULL; name_ref = name_ref->next) {
                  if (name_ref->used_in_primary_declarator) {
                    a_src_seq_secondary_decl_ptr  sec_decl =
                             ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
                    name_ref->used_in_primary_declarator = FALSE;
                    sec_decl->name_reference = name_ref;
                  }  /* if */
                }  /* for */
              }  /* if */
            }  /* if */
            /* Now put out the source sequence entry for the routine, after
               clearing the source-sequence pointer to be sure it will be
               reset. */
            rp->source_corresp.source_sequence_entry = NULL;
            add_to_source_sequence_list((char *)rp,
                                        (an_il_entry_kind)iek_routine);
            ssep = rp->source_corresp.source_sequence_entry;
            check_assertion(scope_depth != NO_SCOPE_DEPTH);
            if (insert_point != NULL || scope_depth != depth_scope_stack) {
              f_move_src_seq_list(ssep, ssep, depth_scope_stack,
                                  insert_point, scope_depth);
            }  /* if */
            /* Assure that instantiations triggered within the body of the
               relocated function are recorded in the source-sequence list
               immediately before it. */
            scope_stack[scope_depth].
                           ss_list_instantiation_insert_point = ssep;
            /* Since a source-sequence entry for the member function is being
               inserted immediately after the end-of-construct-entry for the
               class, mark this as an autonomous class definition (even if
               it wasn't); otherwise, invalid code may be put out by the
               C++-generating back end. */
            class_type->autonomous_primary_tag_decl = TRUE;
            /* If the routine was previously defined as part of a friend
               declaration, that is no longer true. */
            rp->defined_in_friend_decl = FALSE;
          }  /* if */
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          /* If the cache was scanned as reusable, rescan it as reusable and
             discard it below.  Otherwise, just rescan it as non-reusable. */
          if (rfp->function_body_token_cache.is_reusable) {
            rescan_reusable_cache(&rfp->function_body_token_cache);
          } else {
            rescan_cached_tokens(&rfp->function_body_token_cache);
          }  /* if */
          /* Scan the function body. */
          scan_function_body(rp, &rfp->func_info,
                             (SFB_NO_CLASS_REACTIVATION |
                              SFB_NEW_STRUCT_STMT_STACK_REQUIRED |
                              SFB_PRAGMA_PACK_IS_LOCAL));
          /* scan_function_body does not scan past the right brace. */
          if (curr_token == tok_rbrace) (void)get_token();
          if (rfp->function_body_token_cache.is_reusable) {
            discard_token_cache(&rfp->function_body_token_cache);
          }  /* if */
          /* In the normal case the current token should be end_of_source,
             which was inserted to mark the end of the cached token stream.
             If necessary, keep flushing until end-of-source is found. */
          flush_past_token_cache_terminator();
          if (!same_entities(curr_scope_class_type, rfp->class_type)) {
            if (curr_scope_class_type != NULL) {
              /* Pop the reactivated class scope from the scope stack. */
              pop_class_reactivation_scope();
            }  /* if  */
            /* Reactivate the class. */
            push_class_and_template_reactivation_scope(
                rfp->class_type, is_template_based, /*extend_namespace=*/TRUE);
            curr_scope_class_type = rfp->class_type;
          }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
          if (rfp->func_info.is_movable_member_or_friend_def &&
              !source_sequence_entries_disallowed) {
            if (curr_scope_class_type != NULL) {
              /* Pop the reactivated class scope from the scope stack. */
              pop_class_reactivation_scope();
              curr_scope_class_type = NULL;
            }  /* if  */
            if (scope_depth != NO_SCOPE_DEPTH) {
              scope_stack[scope_depth].
                           ss_list_instantiation_insert_point = insert_point;
            }  /* if */
          }  /* if */
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
#if !NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
          source_sequence_entries_disallowed =
                                    saved_source_sequence_entries_disallowed;
          }
#endif /* !NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        }  /* if */
      }  /* if */
      /* Free the current entry, returning it and any expr fixup entries
         attached to it to their respective available-lists.  "rfp" may
         be set to NULL earlier if it should not be freed. */
      if (rfp != NULL) free_routine_fixup(rfp);
    }  /* for */
    if (curr_scope_class_type != NULL) {
      /* Pop the reactivated class scope from the scope stack. */
      pop_class_reactivation_scope();
    }  /* if  */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    if (scope_depth != NO_SCOPE_DEPTH) {
      /* Restore the insert-point state. */
      scope_stack[scope_depth].
                  ss_list_instantiation_insert_point = orig_insert_point;
    }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* The delayed scan fixup entries have been freed, so clear the
       pointer in the class symbol supplement. */
    cssp->routine_fixup_list = NULL;
  }  /* if */
  db_exit();
}  /* inline_function_fixup_for_class */


static void check_trans_unit_for_fixup(
				a_class_fixup_ptr	cfp,
				a_boolean		*trans_unit_pushed)
/*
This routine is called when going through the class fixup lists to
ensure that the correct translation unit is on the top of the stack.
"trans_unit_pushed" is TRUE if a prior call of this routine pushed a
new translation unit.  It is set to TRUE if this call pushes a new
translation unit.
*/
{
  a_symbol_ptr			sym;
  a_translation_unit_ptr	tup_needed;

  sym = (a_symbol_ptr)cfp->class_type->source_corresp.assoc_info;
  tup_needed = trans_unit_for_symbol(sym);
  if (tup_needed != curr_translation_unit) {
    /* The current translation unit is not the right one.  If we previously
       pushed a translation unit, pop it now. */
    if (*trans_unit_pushed) {
      pop_translation_unit_stack();
      *trans_unit_pushed = FALSE;
    }  /* if */
    /* If the new top of stack is still not the right one, push a new entry
       for the translation unit needed. */
    if (tup_needed != curr_translation_unit) {
      push_translation_unit_stack(tup_needed);
      *trans_unit_pushed = TRUE;
    }  /* if */
  }  /* if */
}  /* check_trans_unit_for_fixup */


static void define_defaulted_special_member_functions(a_type_ptr  class_type)
/*
Generate the definitions of any special members defined with "= default" in
the definition of the given class type.
*/
{
  a_routine_ptr  rp = class_type_supp(class_type)->assoc_scope->routines;

  for (; rp != NULL; rp = rp->next) {
    if (rp->is_defaulted) {
      force_definition_of_compiler_generated_routine(rp);
    }  /* if */
  }  /* for */
}  /* define_defaulted_special_member_functions */


static void process_deferred_class_fixups(a_boolean	for_instantiation)
/*
Do the delayed scanning of default arguments and inline function bodies.
Note that this routine can be called recursively if, during the fixup
of a default argument or inline function, a local class is defined or
a template class is instantiated.  When this routine is invoked recursively,
only the default argument processing is done.  The inline function body
processing is only done by the outermost call.  This is done to permit an
inline function body of an instantiation or local class to make use of
a default argument of a class being fixed up at an outer level, for which
the fixups have not yet been done.

for_instantiation is TRUE if this routine is being called to do the fixup
after a class instantiation.
*/
{
  a_class_fixup_ptr		cfp;
  a_class_fixup_ptr		def_arg_list;
  a_class_fixup_ptr		next_cfp;
  a_boolean			trans_unit_pushed = FALSE;
  a_class_fixup_header_ptr	cfhp;

  db_enter(3, "process_deferred_class_fixups");
  cfhp = curr_class_fixup_header(for_instantiation);
  if (cfhp->def_arg_list != NULL ||
      cfhp->inline_function_list != NULL) {
    /* Clear the pointers to the start of the fixup lists so that classes
       created by the fixup process can be fixed up by a recursive call to
       this routine.  This could happen if a function body contains a
       nested class, for example. */
    def_arg_list = cfhp->def_arg_list;
    cfhp->def_arg_list = NULL;
    cfhp->def_arg_list_tail = NULL;
    cfhp->defer_inline_function_fixups++;
    defer_instantiations++;
    for (cfp = def_arg_list; cfp != NULL; cfp = cfp->next) {
      /* Make sure we are in the right translation unit. */
      check_trans_unit_for_fixup(cfp, &trans_unit_pushed);
      default_argument_fixup_for_class(cfp->class_type,
                                       cfp->is_template_instantiation,
                                       /*template_second_pass=*/FALSE);
    }  /* for */
    if (nonclass_prototype_instantiations) {
      /* Do the second pass of default argument fixup to do prototype
         instantiations of template default arguments. */
      for (cfp = def_arg_list; cfp != NULL; cfp = cfp->next) {
        /* Make sure we are in the right translation unit. */
        check_trans_unit_for_fixup(cfp, &trans_unit_pushed);
        default_argument_fixup_for_class(cfp->class_type,
                                         cfp->is_template_instantiation,
                                         /*template_second_pass=*/TRUE);
      }  /* for */
    }  /* if */
    /* cfhp points into the scope_stack, so refresh the pointer after
       the above processing. */
    cfhp = curr_class_fixup_header(for_instantiation);
    cfhp->defer_inline_function_fixups--;
    defer_instantiations--;
    if (cfhp->defer_inline_function_fixups == 0) {
      cfp = cfhp->inline_function_list;
      cfhp->inline_function_list = NULL;
      cfhp->inline_function_list_tail = NULL;
      for (; cfp != NULL; cfp = next_cfp) {
        /* Make sure we are in the right translation unit. */
        check_trans_unit_for_fixup(cfp, &trans_unit_pushed);
        /* Define any defaulted member functions. */
        define_defaulted_special_member_functions(cfp->class_type);
        inline_function_fixup_for_class(cfp->class_type,
                                        cfp->is_template_instantiation);
        next_cfp = cfp->next_in_inline_function_list;
        free_class_fixup(cfp);
      }  /* for */
    }  /* if */
    /* If we pushed a translation unit above, pop it now. */
    if (trans_unit_pushed) pop_translation_unit_stack();
  }  /* if */
  db_exit();
}  /* process_deferred_class_fixups */


void process_deferred_class_fixups_and_instantiations(
					a_boolean	for_instantiation)
/*
While one or more class definitions are pending, the fixup of member function
bodies and default arguments is deferred until all class definitions have
been complete.  Nonclass template definitions are also deferred.  When the
count of pending class definitions is zero, all class definitions have been
completed and any deferred class fixups and instantiations may now be done.

for_instantiation is TRUE if this routine is being called to do the fixup
after a class instantiation.
*/
{
  if (curr_class_fixup_header(for_instantiation)->pending_class_definitions
                                                                        == 0) {
    process_deferred_class_fixups(for_instantiation);
    if (defer_instantiations == 0) {
      process_deferred_instantiation_requests();
    }  /* if */
  }  /* if */
}  /* process_deferred_class_fixups_and_instantiations */


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
  if (ovfp->return_adjustment_base_class != NULL) {
    fputs("\n    return adjustment base class = ", f_debug);
    db_type_name(ovfp->return_adjustment_base_class->type);
  }  /* if */
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
                       class_type->variant.class_struct_union.is_nonreal_class;
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
        /* This is an error, unless we're dealing with a nonreal class.
           Also, many other compilers do not diagnose the problem if all but
           one of the conflicting overriders are in ambiguous base classes. */
        a_base_class_sequence_number  overriders_in_unambiguous_bases;
        overriders_in_unambiguous_bases = ovfp->base_class->ambiguous ? 0 : 1;
        /* Remove the next entry and any successors that also have the same
           virtual function number. */
        for (;;) {
          if (ovfp->next->base_class->ambiguous) {
            ovfp->next = ovfp->next->next;
          } else {
            ++overriders_in_unambiguous_bases;
            if (overriders_in_unambiguous_bases == 0) {
              /* Keep this overrider in case we do not issue an error. */
              *ovfp = *ovfp->next;
            } else {
              ovfp->next = ovfp->next->next;
            }  /* if */
          }  /* if */
          if (ovfp->next == NULL ||
              ovfp->next->primary_function != vfp) {
            /* No more duplicates on the list. */
            break;
          }  /* if */
        }  /* for */
        if (!is_nonreal_instantiation &&
            !(overriders_in_unambiguous_bases <= 1 &&
              (microsoft_bugs || sun_mode || any_cfront_mode()))) {
          a_symbol_ptr sym = (a_symbol_ptr)vfp->source_corresp.assoc_info;
          sym_diagnostic(es_discretionary_error,
                         ec_ambiguous_virtual_function_override, sym);
        }  /* if */
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

  db_enter(4, "check_abstract_class");
  if (class_type->variant.class_struct_union.abstract) {
    /* The class is already marked "abstract", presumably as a result of
       having one or more pure virtual member functions. */
  } else if (class_type->variant.class_struct_union.is_nonreal_class) {
    /* If the class is nonreal and not marked abstract, it could only be
       abstract because it doesn't override an inherited pure virtual.
       However, if that pure virtual member function comes from a dependent
       base we cannot make a good decision yet. */
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
           mark the current class as abstract. */
        bctsp = bcp->type->variant.class_struct_union.extra_info;
        rp = bctsp->assoc_scope->routines;
        for (; rp != NULL; rp = rp->next) {
          if (rp->pure_virtual) {
            /* Found a pure virtual function among the routines.  Advance
               through the overriding virtual function list, passing over
               entries in which the virtual function number of the primary
               function is lower than that of the current routine. */
            for (ovfp = bcp->overriding_virtual_functions;
                 ovfp != NULL;
                 ovfp = ovfp->next) {
              if (ovfp->primary_function->virtual_function_number >=
                                              rp->virtual_function_number) {
                break;
              }  /* if */
            }  /* for */
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


static void report_pure_virtual_functions(a_type_ptr        class_type,
                                          a_base_class_ptr  base_class,
                                          an_error_code     error_code,
                                          a_boolean         *found)
/*
Add to the list of non-overridden pure virtual functions put out with the
diagnostic about abstract class objects.  class_type is the abstract
most-derived-type.  base_class is on the base_classes list of class_type;
it may be NULL.  error_code indicates what message to put out.  *found is
updated to TRUE if one or more non-overridden pure virtual functions is
located.
*/
{
  a_type_ptr                          tp;
  a_base_class_ptr                    bcp;
  a_routine_ptr                       rp;
  an_overriding_virtual_function_ptr  ovfp;
  a_boolean                           overridden;

  tp = base_class == NULL ? class_type : base_class->type;
  if (tp->variant.class_struct_union.any_pure_virtual_functions) {
    /* tp is a class type with pure virtual functions.  List them only if
       they are not overridden in a more derived class. */
    /* Traverse the routines list. */
    rp = tp->variant.class_struct_union.extra_info->assoc_scope->routines;
    for (; rp != NULL; rp = rp->next) {
      if (rp->pure_virtual) {
        /* Found a virtual function declared pure.  But is it overridden? */
        overridden = FALSE;
        if (base_class != NULL) {
          /* There is a more derived class in which there may be an overriding
             function.  This check is required only when we're looking at
             pure virtual functions in a base class. */
          for (ovfp = base_class->overriding_virtual_functions;
               ovfp != NULL;
               ovfp = ovfp->next) {
            if (ovfp->primary_function == rp) {
              /* An overrider was found. */
              overridden = TRUE;
              break;
            } else if (ovfp->primary_function->virtual_function_number >
                                              rp->virtual_function_number) {
              /* Since the order on the routines list corresponds to the
                 virtual-function-number order, the search can terminate. */
              break;
            }  /* if */
          }  /* for */
        }  /* if */
        if (!overridden) {
          /* Put out the extra line of information. */
          a_symbol_ptr  sym = (a_symbol_ptr)rp->source_corresp.assoc_info;
          sym_add_diag_info(error_code, sym);
          *found = TRUE;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  /* The list must also include all non-overridden pure virtual functions in
     base classes. */
  for (bcp = base_classes_of(tp); bcp != NULL; bcp = bcp->next) {
    /* Only base classes that are themselves abstract need to be considered.
       Traverse the derivation hierarchy by visiting the direct base classes
       for the current level and, if this is the top-level base class list,
       the virtual base classes.  (We only visit virtual base classes when
       base_class is NULL to avoid hitting them more than once.) */
    if (bcp->type->variant.class_struct_union.abstract &&
        bcp->is_virtual ? base_class == NULL : bcp->direct) {
      /* Recursive call.  Note that a different error code is used for
         base class pure virtual functions. */
      report_pure_virtual_functions(class_type,
                                    base_class == NULL ?
                                      bcp :
                                      corresponding_base_class(bcp, class_type,
                                                               base_class),
                                    ec_no_overrider_for_pure_virtual_function,
                                    found);
    }  /* if */
  }  /* for */
}  /* report_pure_virtual_functions */


void abstract_class_diagnostic(an_error_severity  severity,
                               an_error_code      error_code,
                               a_type_ptr         class_type,
                               a_source_position  *diag_pos)
/*
Issue a diagnostic (using the message specified by error_code and with the
given severity) on an incorrect use of an object of abstract class type, as
indicated by class_type.  *diag_pos is the source position at which the
diagnostic should be issued.  Except for some Microsoft-specific cases, the
diagnostic includes a list of pure virtual functions, to assist the user in
correcting the class declarations that produced the problem.
*/
{
  a_boolean  found = FALSE;

  class_type = skip_typerefs(class_type);
  pos_ty_start_diagnostic(severity, error_code, diag_pos, class_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (class_type->variant.class_struct_union.is_interface) {
    ty_add_diag_info(ec_type_is_interface, class_type);
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    /* Put out the list of pure virtual functions. */
    report_pure_virtual_functions(class_type, (a_base_class_ptr)NULL,
                                  ec_pure_virtual_function, &found);
    if (!found) {
      /* If class_type is marked as abstract, at least one pure virtual
         function should have been found (except maybe in some Microsoft
         modes, where a class might be defined with the context-sensitive
         keyword "abstract"). */
      if (microsoft_mode && microsoft_version >= 1400) {
        sym_add_diag_info(ec_type_is_declared_abstract,
                          symbol_for(class_type));
#if MICROSOFT_EXTENSIONS_ALLOWED && BACK_END_IS_CP_GEN_BE
        check_assertion(class_type
           ->variant.class_struct_union.defined_with_abstract_class_modifier);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && BACK_END_IS_CP_GEN_BE */
      } else {
        unexpected_condition();
      }  /* if */
    }  /* if */
  }  /* if */
  /* Terminate the supplementary messages. */
  end_error();
}  /* abstract_class_diagnostic */


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


static void copy_virtual_function_override_list(
                                              a_base_class_ptr  old_bcp,
                                              a_base_class_ptr  new_bcp,
                                              a_base_class_ptr  new_direct_bcp)
/*
Copy the list of overriding virtual functions associated with old_bcp and add
each of the copies to the list belonging to new_bcp.  The entries are
being copied from a base class of new_direct_bcp->type to a base class of
new_direct_bcp->derived_class.
*/
{
  an_overriding_virtual_function_ptr  ovfp_to_copy, new_ovfp;
  an_overriding_virtual_function_ptr  ovfp_from_new_list;
  a_base_class_ptr                    new_ovfp_base_class;
  a_boolean                           check_new_list;

  db_enter(4, "copy_virtual_function_override_list");
  if (old_bcp->overriding_virtual_functions == NULL) {
    /* Nothing to copy. */
  } else {
    /* if new_bcp does not have a list of overriding virtual functions, no
       cross checking is required before doing the copies. */
    check_new_list = (new_bcp->overriding_virtual_functions != NULL);
    /* Make a pass over the list from old_bcp.  For each entry on it, see if
       a copy needs to be made.  If so, allocate an new entry and add it to
       the appropriate place in the list. */
    for (ovfp_to_copy = old_bcp->overriding_virtual_functions;
         ovfp_to_copy != NULL;
         ovfp_to_copy = ovfp_to_copy->next) {
      /* The base class of the overriding function must be translated into
         the new class. */
      if (ovfp_to_copy->base_class == NULL) {
        /* The overriding function was declared in new_direct_bcp->type
           (rather than in a base class thereof). */
        new_ovfp_base_class = new_direct_bcp;
      } else {
        /* Be sure to select the right base class (its type could appear
           multiple times in the object hierarchy). */
        new_ovfp_base_class = 
                 corresp_base_class(ovfp_to_copy->base_class, new_direct_bcp);
      }  /* if */
      if (check_new_list) {
        for (ovfp_from_new_list = new_bcp->overriding_virtual_functions;
             ovfp_from_new_list != NULL;
             ovfp_from_new_list = ovfp_from_new_list->next) {
          if (ovfp_from_new_list->primary_function ==
                                            ovfp_to_copy->primary_function) {
            if (ovfp_from_new_list->overriding_function ==
                                         ovfp_to_copy->overriding_function &&
                ovfp_from_new_list->base_class == ovfp_to_copy->base_class) {
              /* This override is already recorded.  No copy is needed. */
              goto next_entry_from_old_list;
            } else if (is_on_any_derivation_of(new_ovfp_base_class,
                                             ovfp_from_new_list->base_class)) {
              /* The override that is already recorded dominates the new
                 once, since the base class to which the new override belongs
                 is on a derivation of the other one.  Don't enter the
                 override from the dominated base class.  For example:
                        A virtual A::f()
                       / \
                     AA   B B::f()    -- B is new_ovfp_base_class
                       \ / \
                        BB  C C::f()  -- C is ovfp_from_new_list->base_class
                         \ /
                          D
                 In other words, we don't want to enter the override of
                 A::f by B::f in D if the override of B::f by C::f is already
                 on the list.  If that were done, a spurious ambiguity would
                 be diagnosed.  In this case, C::f dominates B::f since C-in-D
                 is on at least one derivation of B-in-D. */
              goto next_entry_from_old_list;
            } else if (is_on_any_derivation_of(ovfp_from_new_list->base_class,
                                               new_ovfp_base_class)) {
              an_overriding_virtual_function_ptr  ovfp, prev_ovfp;

              /* Similar to last case, but with the dominance relation
                 reversed.  Change the entry on the list to reflect the new
                 override. */
              ovfp_from_new_list->overriding_function =
                                           ovfp_to_copy->overriding_function;
              ovfp_from_new_list->base_class = new_ovfp_base_class;
              ovfp_from_new_list->return_adjustment_base_class =
                                ovfp_to_copy->return_adjustment_base_class;
              /* If there are any other entries on the list that are
                 similarly dominated by the new override, they should be
                 removed from the list.  Otherwise spurious ambiguity errors
                 would be issued. */
              prev_ovfp = ovfp_from_new_list;
              ovfp = ovfp_from_new_list->next;
              while (ovfp != NULL &&
                     ovfp->primary_function ==
                                         ovfp_to_copy->primary_function) {
                if (is_on_any_derivation_of(ovfp->base_class,
                                            new_ovfp_base_class)) {
                  /* Same dominance relation.  Remove ovfp from the list. */
                  prev_ovfp->next = ovfp->next;
                } else {
                  prev_ovfp = ovfp;
                }  /* if */
                /* Advance to the next entry, and continue looping. */
                ovfp = ovfp->next;
              }  /* while */
              goto next_entry_from_old_list;
            } else {
              /* We've found another entry on the list that records an
                 override of the very same primary function, but the
                 overriding function is distinct.  Keep looping. */
            }  /* if */
          } else if (ovfp_from_new_list->primary_function->
                                                   virtual_function_number >
                     ovfp_to_copy->primary_function->virtual_function_number) {
            /* Since the lists are ordered by virtual_function_number,
               no further checking is required. */
            break;
          }  /* if */
        }  /* for */
      }  /* if */
      /* Allocate a new entry and copy fields from the original. */
      new_ovfp = alloc_overriding_virtual_function();
      new_ovfp->primary_function = ovfp_to_copy->primary_function;
      new_ovfp->overriding_function = ovfp_to_copy->overriding_function;
      new_ovfp->base_class = new_ovfp_base_class;
      new_ovfp->return_adjustment_base_class =
                                ovfp_to_copy->return_adjustment_base_class;
#if DEBUG
      if (debug_level >= 4) {
        fputs("copy for base class ", f_debug);
        db_type_name(new_bcp->type);
        fputs(": ", f_debug);
        db_virtual_function_override(ovfp_to_copy);
      }  /* if */
#endif /* DEBUG */
      /* Add it to the new list. */
      insert_in_virtual_function_override_list(new_bcp, new_ovfp);
next_entry_from_old_list:;
    }  /* for */
  }  /* if */
  db_exit();
}  /* copy_virtual_function_override_list */


static void record_virtual_function_override(
                                a_member_decl_info_ptr  decl_info,
                                a_base_class_ptr        base_class,
                                a_routine_ptr           primary_func,
                                a_base_class_ptr        return_adjustment_bcp)
/*
Record the overriding of virtual function "primary_func", which was declared
in a base class ("base_class") of the current class, by the function described
by "decl_info", which was declared in the current class.  The override entry
appears on a linked list pointed to from base_class.
*/
{
  a_decl_parse_state_ptr  dps = &decl_info->decl_state;
  a_routine_ptr           overriding_func = dps->sym->variant.routine.ptr;
  an_overriding_virtual_function_ptr
                          ovfp;

  db_enter(4, "record_virtual_function_override");
  /* If there is already an override entry, created when the primary routine
     was overridden by a function in another base class (one on the path
     between the current class and the class of which the primary function
     is a member), we can simply reuse that entry. */
  ovfp = base_class->overriding_virtual_functions;
  for (; ovfp != NULL; ovfp = ovfp->next) {
    if (ovfp->primary_function == primary_func) {
      /* Usually, this is an entry copied when base classes are added (by
         copy_virtual_function_override_list) that can now be reused to
         represent the new overrider in the most-derived class.  However, in
         Microsoft mode, this may also occur because the base was selected
         multiple times for selective overriding in the same derived class.
         Check for the latter case first. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (parent_scope_of(ovfp->overriding_function) ==
                                           parent_scope_of(overriding_func) &&
          (ovfp->overriding_function->overridden_functions != NULL ||
           overriding_func->overridden_functions != NULL)) {
        a_boolean  replace_override = FALSE;
        check_assertion(microsoft_mode);
        if (cppcli_enabled && 
            cli_class_type_kind_is(base_class->type, cctk_interface)) {
          /* When overriding C++/CLI interface members, named overriding
             trumps ordinary (unnamed) overriding. */
          if (decl_info->named_overrides != NULL &&
              ovfp->overriding_function->overridden_functions == NULL) {
            /* The entry currently records an ordinary override and the new
               declaration is a named override; replace the record by the
               named override. */
            replace_override = TRUE;
          } else if (decl_info->named_overrides == NULL &&
                     ovfp->overriding_function->overridden_functions != NULL) {
            /* The entry already records a named override, and the current
               declaration is an ordinary override: Ignore the overriding
               implied by the current declaration for the given base class. */
            goto done;
          }  /* if */
        }  /* if */
        if (!replace_override) {
          if (!decl_info->multiple_overrides_diagnostic_issued &&
              !scope_stack_top().in_prototype_instantiation) {
            /* During prototype instantiations, no diagnostic is issued since
               the overridden base cannot always be identified reliably. */
            pos_sy2_error(ec_multiple_overrides, &dps->declarator_pos,
                          symbol_for(primary_func),
                          symbol_for(ovfp->overriding_function));
            decl_info->multiple_overrides_diagnostic_issued = TRUE;
          }  /* if */
          ovfp = NULL;
          break;
        }  /* if */
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DEBUG
      if (debug_level >= 4) {
        fputs("existing entry: ", f_debug);
        db_virtual_function_override(ovfp);
      }  /* if */
#endif /* DEBUG */
      ovfp->overriding_function = overriding_func;
      ovfp->base_class = NULL;
      ovfp->return_adjustment_base_class = return_adjustment_bcp;
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
    ovfp->return_adjustment_base_class = return_adjustment_bcp;
#if DEBUG
    if (debug_level >= 4) {
      fputs("newly created: ", f_debug);
      db_virtual_function_override(ovfp);
    }  /* if */
#endif /* DEBUG */
    insert_in_virtual_function_override_list(base_class, ovfp);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
done:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  db_exit();
}  /* record_virtual_function_override */


#if !ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
/*ARGSUSED*/ /* class_type is used only to support covariant return types. */
#endif /* !ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
static a_boolean return_types_are_override_compatible(
                                 a_type_ptr        type_of_overriding_routine,
                                 a_type_ptr        type_of_overridden_routine,
                                 a_base_class_ptr  *return_adjustment_bcp,
                                 a_symbol_ptr      overridden_sym,
                                 a_source_position *diag_pos)
/*
Given the routine types of overriding and overridden virtual functions,
return TRUE if the return types are identical or "covariant" (WP 10.3).
Covariance means both return types are references or pointers to class types
that are related by derivation, where the class associated with the overridden
function is a base class of the class associated with the overriding function.
When covariance is detected, return in *return_adjustment_bcp the base class
entry for the class associated with the overridden function.  Compatibility
problems are diagnosed at the given position for the given symbol describing
the overridden symbol.
*/
{
  a_type_ptr             tp1, tp2;
  a_boolean              compatible = FALSE;

  db_enter(4, "return_types_are_override_compatible");
  tp1 = type_of_overriding_routine->variant.routine.return_type;
  tp2 = type_of_overridden_routine->variant.routine.return_type;
  *return_adjustment_bcp = NULL;
  if (identical_types(tp1, tp2)) {
    /* The types are identical. */
    compatible = TRUE;
  } else if (is_or_contains_template_param(tp1) ||
             is_or_contains_template_param(tp2)) {
    /* We must be within a prototype instantiation.  The types may be
       compatible depending on the template argument in a real instantiation,
       so issue no error now. */
    compatible = TRUE;
  } else if (is_error_type(tp1) || is_error_type(tp2)) {
    /* Assume compatibility. */
    compatible = TRUE;
  } else {
    /* They're not "simply" compatible.  Do the other checking. */
    if ((types_are_references_of_the_same_kind(tp1, tp2) &&
         is_rvalue_reference_type(tp1) == is_rvalue_reference_type(tp2)) ||
        (types_are_both_pointers_or_both_handles(tp1, tp2) &&
         type_qualifiers_match(tp1, tp2))
#ifdef pointer_types_have_same_repr
        && pointer_types_have_same_repr(tp1, tp2)
#endif /* ifdef pointer_types_have_same_repr */
                                                 ) {
      /* Both types are references or both are pointers with identical type
         qualifiers on top of the pointer type.  Now check the types pointed
         to. */
      a_boolean  pointers_to_classes;
      tp1 = type_pointed_to(tp1);
      tp2 = type_pointed_to(tp2);
      pointers_to_classes = is_class_struct_union_type(tp1) &&
                            is_class_struct_union_type(tp2);
      if (pointers_to_classes || gpp_mode) {
        /* In most modes, the types referenced/pointed to must both be
           (related) classes.  GNU C++ accepts types that differ only in
           qualification.  GNU C++ also accepts overriding a function with a
           void* return type with a function that returns a different pointer
           type. */
        a_type_qualifier_set  tp1_quals = get_type_qualifiers(tp1);
        a_type_qualifier_set  tp2_quals = get_type_qualifiers(tp2);
        if (!any_qualifier_in_set_missing(tp2_quals, tp1_quals)) {
          /* The cv-qualification on the class of the overriding function's
             return type (tp1) is equal to or less than the cv-qualification
             on the class of the overridden function's return type (tp2). */
          tp1 = skip_typerefs(tp1);
          tp2 = skip_typerefs(tp2);
          /* Next see if the class associated with the overridden function
             is the same as or a base class of the class associated with the
             overriding function.  (In GNU mode, they might also be nonclass
             types.) */
          if (identical_types(tp1, tp2) || is_void_type(tp2)) {
            /* The underlying types are the same or the overridden function
               returns void* (ignoring cv-qualifiers). */
            compatible = TRUE;
            if (!pointers_to_classes) {
              check_assertion(gpp_mode);
              pos_syty_warning(
                        ec_different_return_type_on_virtual_function_override,
                        diag_pos, overridden_sym,
                        skip_typerefs(type_of_overridden_routine)
                                               ->variant.routine.return_type);
            }  /* if */
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
          } else if (pointers_to_classes) {
            a_base_class_ptr bcp;
            complete_type_is_needed(tp1);
            /* We don't need to test the completeness of the classes.  This
               is done by find_base_class_of. */
            bcp = find_base_class_of(tp1, tp2);
            if (bcp != NULL) {
              /* tp2 is a base class of tp1.  Be sure it's unambiguous and
                 accessible in tp1. */
              if (!bcp->ambiguous && is_accessible_base_class(bcp)) {
                compatible = TRUE;
                *return_adjustment_bcp = bcp;
              }  /* if */
            }  /* if */
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (!compatible &&
      !((gpp_mode || microsoft_mode) &&
        is_prototype_instantiation_context())) {
    /* Error -- return type must be identical to or covariant with that of the
       overridden function.  (GNU and Microsoft compilers appear not to check
       this during prototype instantiations.) */
    an_error_code  error_code =
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
                        ec_bad_return_type_on_virtual_function_override;
#else /* !ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
                        ec_different_return_type_on_virtual_function_override;
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
    pos_syty_error(error_code, diag_pos, overridden_sym,
                   skip_typerefs(type_of_overridden_routine)
                                               ->variant.routine.return_type);
  }  /* if */
  db_exit();
  return compatible;      
}  /* return_types_are_override_compatible */


a_base_class_ptr find_disambiguator(a_base_class_ptr  bcp1,
                                    a_base_class_ptr  bcp2)
/*
bcp1 is a base class and bcp2 is a base class of bcp1->type.  We need to
find a base class of bcp1->derived_class that can serve as disambiguator for
computing the base class of bcp1->derived_class to which bcp2 corresponds.
Usually bcp1 itself can serve as disambiguator, but if bcp2 is ambiguous,
more work needs to be done.
*/
{
  a_base_class_ptr       disambiguator;
  a_derivation_step_ptr  step;
  a_type_ptr             class_type = bcp1->derived_class;

  if (bcp2->is_virtual) {
    /* No disambiguator is required for virtual base classes. */
    disambiguator = NULL;
  } else {
    /* Try bcp1 itself to start with. */
    disambiguator = bcp1;
    if (bcp2->derivation->direct) {
      /* bcp2 is a direct base class of bcp1->type, so bcp1 may be used as a
         disambiguator to find the corresponding base class of class_type. */
    } else {
      /* Compute the disambiguator by traversing the derivation of bcp2. */
      step = bcp2->derivation->path;
      for (; step->base_class != bcp2; step = step->next) {
        disambiguator = corresponding_base_class(step->base_class,
                                                 class_type,
                                                 disambiguator);
      }  /* for */
    }  /* if */
  }  /* if */
  return disambiguator;
}  /* find_disambiguator */

#if IA64_ABI

a_base_class_ptr nominal_primary_base(a_base_class_ptr bcp)
/*
Return the base of bcp that would be the primary base if bcp were the complete
object.  Return NULL if bcp->type has no primary base.
*/
{
  a_base_class_ptr            primary;
  a_class_type_supplement_ptr ctsp;

  ctsp = bcp->type->variant.class_struct_union.extra_info;
  if (ctsp->primary_base_class != NULL) {
    primary = corresp_base_class(ctsp->primary_base_class, bcp);
  } else {
    primary = NULL;
  }  /* if */
  return primary;
}  /* nominal_primary_base */

#endif /* IA64_ABI */

static a_boolean shares_virtual_function_info(a_type_ptr        class_type,
                                              a_base_class_ptr  base_class)
/*
base_class points to a base class of class_type.  If class_type shares its
virtual function info with a base class and that base class is base_class or
the base class with which base_class shares its virtual function info, return
TRUE.
*/
{
  a_boolean         shares = FALSE;
#if IA64_ABI
  a_base_class_ptr  primary;

  for (primary = class_type->variant.class_struct_union.extra_info->
                                                            primary_base_class;
       primary != NULL;
       primary = nominal_primary_base(primary)) {
    if (primary == base_class) {
      shares = TRUE;
      break;
    }  /* if */
  }  /* for */
#else /* !IA64_ABI */
  a_base_class_ptr  virtual_function_info_base_class, bcp;

  virtual_function_info_base_class =
                          class_type->variant.class_struct_union.extra_info->
                                              virtual_function_info_base_class;
  if (virtual_function_info_base_class == base_class) {
    /* base_class is the base class with which class_type shares its virtual
       function info. */
    shares = TRUE;
  } else if (virtual_function_info_base_class != NULL) {
    bcp = base_class->type->variant.class_struct_union.extra_info->
                                              virtual_function_info_base_class;
    if (bcp != NULL) {
      /* bcp now points to a base class of base_class; change it to point
         to the corresponding base class of class_type. */
      bcp = corresp_base_class(bcp, base_class);
      if (virtual_function_info_base_class == bcp) {
        shares = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* !IA64_ABI */
  return shares;
}  /* shares_virtual_function_info */


static void update_override_registry(
                             an_override_registry_entry_ptr *registry_ptr,
                             a_symbol_ptr                   overridden_sym,
                             a_symbol_ptr                   nonoverriding_sym,
                             a_base_class_ptr               bcp)
/*
A declaration in the current derived class has been seen, and it has the
effect of overriding a virtual function from a base class.  The latter may be
a member of an overload set: overridden_sym represents that overload set or
a single overridden function if it is not part of an overload set.  Keep track
of the number of overrides by updating the linked list pointed to by
registry_ptr.  When all the virtual functions in the overload set have been
overridden, the corresponding entry is removed from the registry.
*/
{
  an_override_registry_entry_ptr  orep, prev_orep;

  /* Loop through the current entries in the registry to see if this symbol
     is already represented on the list. */
  prev_orep = NULL;
  for (orep = *registry_ptr; orep != NULL; orep = orep->next) {
    if (orep->overridden_sym == overridden_sym && orep->base_class == bcp) {
      /* It's a match. */
      break;
    }  /* if */
    prev_orep = orep;
  }  /* for */
  if (orep == NULL) {
    /* No matching entry was found in the registry.  Only if there is more
       than one virtual function in the overload set do we need a partial-
       override entry. */
    orep = alloc_override_registry_entry();
    orep->overridden_sym = overridden_sym;
    orep->base_class = bcp;
    if (overridden_sym->kind == (a_symbol_kind)sk_overloaded_function) {
      /* Count the number of functions in the overload set that are
         virtual.  There should be at least one if this routine is being
         called. */
      a_symbol_ptr  sym = overridden_sym->variant.overloaded_function.symbols;
      unsigned int  count = 0;
      for (; sym != NULL; sym = sym->next) {
        if (sym->kind == (a_symbol_kind)sk_member_function &&
            sym->variant.routine.ptr->is_virtual) {
          ++count;
        }  /* if */
      }  /* for */
      check_assertion(count > 0);
      orep->virtual_function_count = count;
    } else {
      check_assertion(overridden_sym->variant.routine.ptr->is_virtual);
      orep->virtual_function_count = 1;
    }  /* if */
    /* Add the new entry to the end of the registry. */
    if (prev_orep == NULL) {
      *registry_ptr = orep;
    } else {
      prev_orep->next = orep;
    }  /* if */
  }  /* if */
  /* If this routine is called with a non-NULL nonoverriding_sym, the
     declaration failed to override a base class virtual function; if so,
     add a new entry to the end of the list of "override failures".  But
     if nonoverriding_sym is NULL, then this is a successful override, in
     which case the override count should be bumped. */
  if (nonoverriding_sym != NULL) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cppcli_enabled &&
        nonoverriding_sym->variant.routine.ptr->overridden_functions != NULL) {
      /* The non-overriding derived class declaration selectively overrides
         specific functions.  It is therefore likely that it intentionally
         does not override its base-class homonym and no "failure" should be
         recorded. */
    } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    {
      a_symbol_list_entry_ptr  new_slep, slep;
      new_slep = alloc_symbol_list_entry();
      new_slep->symbol = nonoverriding_sym;
      if (orep->override_failures == NULL) {
        orep->override_failures = new_slep;
      } else {
        slep = orep->override_failures;
        while (slep->next != NULL) slep = slep->next;
        slep->next = new_slep;
      }  /* if */
    }  /* if */
  } else {
    /* Increment the override count. */
    orep->override_count += 1;
  }  /* if */
}  /* update_override_registry */


static void remove_name_from_override_registry(
                                         an_override_registry_entry_ptr  orep)
/*
Remove all override registry entries following the given one and pointing to
an overridden symbol with the same header as the given one (orep).
This filtering is used to avoid issuing many diagnostics on a single name.
*/
{
  a_symbol_header_ptr  header = orep->overridden_sym->header;
  an_override_registry_entry_ptr  next_orep = orep->next;

  while (next_orep != NULL) {
    if (next_orep->overridden_sym->header == header) {
      orep->next = next_orep->next;
      free_override_registry_entry(next_orep);
      next_orep = orep->next;
    } else {
      orep = next_orep;
      next_orep = next_orep->next;
    }  /* if */
  }  /* while */
}  /* remove_name_from_override_registry */


static a_boolean base_function_unhidden_by_projection(
                                         a_symbol_ptr                    tsym,
                                         an_override_registry_entry_ptr  orep)
/*
Look through the using declarations in the type associated with the given
symbol tsym for one that might unhide functions hidden by the incomplete
overriding of which orep is a part.
*/
{
  a_boolean            result = FALSE;
  a_type_ptr           type = tsym->variant.class_struct_union.type;
  a_class_type_supplement_ptr
                       ctsp = type->variant.class_struct_union.extra_info;
  a_using_decl_ptr     udecl = ctsp->assoc_scope->using_decls;
  a_symbol_header_ptr  header = orep->overridden_sym->header;

  for (; udecl != NULL; udecl = udecl->next) {
    if (udecl->entity.kind == (a_byte_il_entry_kind)iek_routine) {
      a_routine_ptr  routine = (a_routine_ptr)udecl->entity.ptr;
      if (symbol_for(routine)->header == header) {
        result = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* base_function_unhidden_by_projection */


static a_boolean class_member_name_marked_as_hiding(a_symbol_ptr         csym,
                                                    a_symbol_header_ptr  hdr)
/*
Return TRUE if a member of the class represented by csym with the name
represented by hdr was declared with the C++0x attribute "hiding".
*/
{
  a_boolean  result = FALSE;

  if (csym->variant.class_struct_union.extra_info->check_hiding_attr) {
    a_symbol_ptr  msym = csym->variant.class_struct_union.extra_info->symbols;
    for (; msym != NULL; msym = msym->next_in_scope) {
      if (msym->header == hdr) {
        a_boolean     ovl = symbol_is(msym, sk_overloaded_function);
        a_symbol_ptr  sym = ovl ? msym->variant.overloaded_function.symbols
                                : msym;
        for (; sym != NULL; sym = ovl ? sym->next : NULL) {
          a_source_correspondence  *scp = source_corresp_entry_for_symbol(sym);
          if (scp != NULL &&
              find_attribute(ak_hiding, scp->attributes) != NULL) {
            result = TRUE;
            goto done;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* for */
  }  /* if */
done:
  return result;
}  /* class_member_name_marked_as_hiding */


static void check_override_registry(a_class_def_state  *class_state)
/*
class_state describes a class definition that has just been completed.
Check for cases where a set of overloaded virtual functions in the base class 
was only partially overridden and issue a diagnostic if appropriate.
Similarly, check for virtual functions that are hidden rather than overridden.
(Whether a diagnostic is issued at all and the severity of any diagnostics is
dependent on the use of the C++0x attributes "base_check" and "hiding".)
*/
{
  a_symbol_ptr  tag_sym = symbol_for(class_state->class_type);
  a_boolean     strict_checking =
                   tag_sym->variant.class_struct_union.extra_info->base_check;
  an_override_registry_entry_ptr
                orep = class_state->override_registry, next_orep;

  /* Loop through the registry of overrides. */
  for (; orep != NULL; orep = next_orep) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (class_type_supp(class_state->class_type)->is_hide_by_sig) {
      /* In managed class types, lookup is based on "signature": The
         traditional hiding-instead-of-overriding problems are not an issue
         in that context. */
    } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    if (orep->override_count < orep->virtual_function_count) {
      if (orep->override_count > 0) {
        /* Issue a diagnostic on partial override of an overloaded
           virtual function. */
        if (base_function_unhidden_by_projection(tag_sym, orep)) {
          /* The partial overriding is mitigated by having the nonoverridden
             declarations projected through a using-declaration. */
          goto next;
        } else {
          /* Issue a diagnostic, unless the C++0x attribute "hiding" was
             specified on the function's name in the derived class. */
          if (!class_member_name_marked_as_hiding(
                                     tag_sym, orep->overridden_sym->header)) {
            pos_sy2_diagnostic(strict_checking ? es_error : es_warning,
                               ec_partial_override, &tag_sym->decl_position,
                               orep->overridden_sym, tag_sym);
          }  /* if */
          /* No need to issue any more diagnostics on this name. */
          remove_name_from_override_registry(orep);
        }  /* if */
      } else {
        /* Report on hidden virtual functions. */
        if (base_function_unhidden_by_projection(tag_sym, orep)) {
          /* The partial overriding is mitigated by having the nonoverridden
             declarations projected through a using-declaration. */
          goto next;
        } else {
          a_symbol_list_entry_ptr  slep = orep->override_failures;
          for (; slep != NULL; slep = slep->next) {
            pos_sy2_warning(ec_virtual_function_decl_hidden,
                            &slep->symbol->decl_position,
                            slep->symbol, orep->overridden_sym);
          }  /* for */
          if (orep->override_failures != NULL) {
            /* No need to issue any more diagnostics on this name. */
            remove_name_from_override_registry(orep);
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* Note that the count may be off in prototype instantiations because
         explicit overriders may have an unknown overridden base. */
      check_assertion(orep->override_count == orep->virtual_function_count ||
                      is_prototype_instantiation_symbol(tag_sym) ||
                      total_errors > 0);
      /* Okay -- no diagnostic, even if there were additional nonoverriding
         declarations of the same name. */
    }  /* if */
    /* Return the entry to the available list and advance. */
next:;
    next_orep = orep->next;
    free_override_registry_entry(orep);
  }  /* for */
}  /* check_override_registry */


static void update_virtual_function_number(
                                      a_routine_ptr             rp,
                                      a_virtual_function_number *number_ptr)
/*
If the given routine is virtual and has not yet been assigned a virtual
function number, assign one now.  number_ptr is the address of the field in
a_class_type_supplement that tracks the highest number assigned thus far.
*/
{
  if (!rp->is_virtual ||
      rp->virtual_function_number != VIRTUAL_FUNCTION_NUMBER_NONE) {
    /* The given routine is either nonvirtual or it already has a virtual
       function number assigned: Nothing to do. */
  } else {
    if (*number_ptr == VIRTUAL_FUNCTION_NUMBER_NONE) {
      /* No previous numbers, start at the first value. */
      *number_ptr = FIRST_VIRTUAL_FUNCTION_NUMBER;
    } else if (*number_ptr == MAX_VIRTUAL_FUNCTIONS_PER_CLASS) {
      a_type_ptr  parent_class = parent_class_of(rp);
      if (parent_class->variant.class_struct_union.is_nonreal_class) {
        /* Don't issue an error, since the number may not be maintained
           accurately for nonreal class instantiations. */
      } else {
        pos_error(ec_too_many_virtual_functions,
                  &rp->source_corresp.decl_position);
      }  /* if */
      /* Reset to the first number, to avoid more such messages. */
      *number_ptr = FIRST_VIRTUAL_FUNCTION_NUMBER;
    } else {
      /* Increment the number for the virtual functions declared so far in the
         current class and enter it in the routine entry.  It is used by the
         front end in managing virtual function override entries and can be
         used by the back end for indexing into a virtual function table. */
      ++(*number_ptr);
    }  /* if */
    rp->virtual_function_number = *number_ptr;
#if IA64_ABI
    if (rp->special_kind == (a_special_function_kind)sfk_destructor) {
      /* There are two entries for destructors: one for the complete object
         entry point and one for the deleting entry point. */
      ++(*number_ptr);
    }  /* if */
#endif /* IA64_ABI */
  }  /* if */
}  /* update_virtual_function_number */


static void report_override_exception_spec_mismatch(
                                               a_symbol_ptr       overrider,
                                               a_symbol_ptr       overridden,
                                               a_source_position  *source_pos)
/*
The member function represented by overrider overrides the virtual member
represented by overridden, but its exception specification is less
restrictive.  Issue an appropriate diagnostic at the given position.
*/
{
  if (overrider->variant.routine.ptr->compiler_generated) {
    /* In non-strict modes, issue a warning on a compiler-generated
       constructor, destructor, or assignment operator.  In strict
       modes, an error should be issued by default (discretionary). */
    pos_sy2_diagnostic(strict_ansi_mode ?
                              strict_ansi_discretionary_severity : es_warning,
                       ec_generated_exception_spec_override_incompat,
                       source_pos, overrider, overridden);
  } else {
    /* Microsoft compilers don't diagnose this (and in fact, they don't do
       much with exception specifications at all). */
    pos_sy2_diagnostic(microsoft_mode ? es_warning : es_discretionary_error,
                       ec_exception_spec_override_incompat,
                       source_pos, overrider, overridden);
  }  /* if */
}  /* report_override_exception_spec_mismatch */


static void check_deleted_function_overrides(
                                               a_symbol_ptr       overrider,
                                               a_symbol_ptr       overridden,
                                               a_source_position  *source_pos)
/*
The member function represented by overrider overrides the virtual member
represented by overridden. Check that if one function is a deleted member, the
other is also; issue an error at the given position otherwise.
*/
{
  if (overrider->variant.routine.ptr->is_deleted) {
    if (!overridden->variant.routine.ptr->is_deleted) {
      pos_sy_error(ec_deleted_function_overrides_nondeleted_function,
                   source_pos, overridden);
    }  /* if */
  } else if (overridden->variant.routine.ptr->is_deleted) {
    pos_sy_error(ec_nondeleted_function_overrides_deleted_function,
                 source_pos, overridden);
  }  /* if */
}  /* check_deleted_function_overrides */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean is_interface_like(a_type_ptr  class_type)
/*
Return TRUE if the given class type is a Microsoft interface class or an
interface-like class.
*/
{
  check_assertion(is_immediate_class_type(class_type));
  return class_type->variant.class_struct_union.is_interface ||
         class_type->variant.class_struct_union.is_interface_like;
}  /* is_interface_like */


static a_boolean is_selectively_overridden_by(a_symbol_ptr  overridden_sym,
                                              a_symbol_ptr  overriding_sym)
/*
Return TRUE if overridden_sym represents a member selectively overridden
by the member function overriding_sym.  Selective overriding is a Microsoft
extension.  For example:
  struct B1 { virtual int f() = 0; };
  struct B2 { virtual int f() = 0; };
  struct D: B1, B2 {
    int B1::f() { return 1; }  // Selectively overrides B1::f (not B2::f).
    int B2::f() { return 2; }  // Selectively overrides B2::f (not B1::f).
  };

Note: This function is only for use with non-C++/CLI-style selective
overriding.
*/
{
  a_boolean      result = FALSE;
  a_routine_ptr  overrider;

  check_assertion(overriding_sym->kind == (a_symbol_kind)sk_member_function);
  overrider = overriding_sym->variant.routine.ptr;
  if (overrider->overridden_functions == NULL) {
    result = (overridden_sym == NULL);
  } else if (overridden_sym == NULL) {
    result = (overrider->overridden_functions == NULL);
  } else if (overridden_sym->kind == (a_symbol_kind)sk_member_function) {
    /* overridden_sym represents a known base class member function. */
    result = (selectively_overridden_function(overrider) ==
                                         overridden_sym->variant.routine.ptr);
  } else if (is_nontype_template_param_symbol(overridden_sym) &&
             overridden_sym->variant.constant->variant.template_param.kind ==
                                (a_template_param_constant_kind)tpck_member) {
    /* overridden_sym represents a member of a dependent base class. */
    a_tagged_pointer  ep = overrider->overridden_functions->entity;
    if ((an_il_entry_kind)ep.kind == iek_constant) {
      result = eq_constants((a_constant_ptr)ep.ptr,
                            overridden_sym->variant.constant);
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  return result;
}  /* is_selectively_overridden_by */


static a_boolean selective_override_match(a_routine_ptr     overrider,
                                          a_routine_ptr     candidate,
                                          a_base_class_ptr  base_class)
/*
overrider is a function that selectively overrides a base class function (i.e.,
overrider->overridden_functions is non-NULL).  Return TRUE if it may override
candidate in the given base class.  Selective overriding is a Microsoft C++
extension.  For example:
  struct B1 { virtual int f() = 0; };
  struct B2 { virtual int f() = 0; };
  struct D: B1, B2 {
    int B1::f() { return 1; }  // Selectively overrides B1::f (not B2::f).
    int B2::f() { return 2; }  // Selectively overrides B2::f (not B1::f).
  };

Note: This function is only for use with non-C++/CLI-style selective
overriding.
*/
{
  a_boolean         result = FALSE;

  check_assertion(overrider->overridden_functions != NULL);
  if (is_interface_like(base_class->type)) {
    /* For __interface-like base classes (which does not include C++/CLI
       interface class/struct types), the overridden function is only the one
       in the indicated base subobject.
         __interface B {
           virtual void f() = 0;
         };
         __interface C1: B {};
         __interface C2: B {};
         struct D: C1, C2 {
           void C1::f() {}  // Overrides B::f (only) in the C1::B subobject.
           void C2::f() {}  // Overrides B::f (only) in the C2::B subobject.
         }; */
    a_routine_ptr  ofp = selectively_overridden_function(overrider);
    if (ofp == NULL) {
      /* overrider selectively overrides an unknown (i.e., template dependent)
         function.  So it "may" selectively override the given candidate. */
      result = TRUE;
    } else {
      a_base_class_ptr  of_bcp = find_base_class_of(base_class->derived_class,
                                                    parent_class_of(ofp));
      /* Check that the given base is a subobject of the explicitly designated
         base (of_bcp). */
      check_assertion(!of_bcp->ambiguous);
      if (is_on_any_derivation_of(base_class, of_bcp)) {
        if (ofp == candidate) {
          /* Direct overrider. */
          result = TRUE;
        } else {
          /* Check for indirect overriding (via an interface slot). */
          ofp = selectively_overridden_function(ofp);
          while (ofp != NULL) {
            if (ofp == candidate) {
              result = TRUE;
              break;
            }  /* if */
            ofp = selectively_overridden_function(ofp);
          }  /* while */
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    /* For non-__interface base classes, the overrider must directly indicate
       the overridden function, and all the base subobjects are overridden.
       For example:
         struct B { virtual void f() = 0; };
         struct C1: B {};
         struct C2: B {};
         struct D: C1, C2 {
           void C1::f(); // Overides f in both base subobjects.
           void C2::f(); // Error: Redeclaration.
         };
    */
    an_il_entity_list_entry_ptr  ofep = overrider->overridden_functions;
    a_tagged_pointer             ep;
    check_assertion(ofep != NULL && ofep->next == NULL);
    ep = ofep->entity;
    if ((an_il_entry_kind)ep.kind == iek_routine &&
        (a_routine_ptr)ep.ptr == candidate) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* selective_override_match */


static a_boolean matching_cli_accessors(a_routine_ptr  overrider,
                                        a_routine_ptr  candidate)
/*
Overrider is a function that might override virtual function "candidate".
If either function is a C++/CLI property or event accessor return FALSE if
the properties or events do not match for overriding purposes.  Otherwise,
return TRUE.
*/
{
  a_boolean                      mismatch = FALSE;
  a_property_or_event_descr_ptr  pdp1 = NULL, pdp2 = NULL;

  if (rout_is_cli_accessor(overrider)) {
    pdp1 = overrider->variant.property_or_event_descr;
  }  /* if */
  if (rout_is_cli_accessor(candidate)) {
    pdp2 = candidate->variant.property_or_event_descr;
  }  /* if */
  if (pdp1 == NULL && pdp2 == NULL) {
    /* No accessors involved.  Return TRUE. */
  } else if (pdp1 == NULL || pdp2 == NULL) {
    /* One is an accessor and the other not: Mismatch. */
    mismatch = TRUE;
  } else if (pdp1->is_static || pdp2->is_static) {
    /* If one property is static, it cannot participate in overriding. */
    mismatch = TRUE;
  } else {
    a_field_ptr  fp1 = pdp1->variant.field, fp2 = pdp2->variant.field;
    if (strcmp(fp1->source_corresp.name, fp2->source_corresp.name) != 0) {
      /* Properties with different names don't match. */
      mismatch = TRUE;
    }  /* if */
  }  /* if */
  return !mismatch;
}  /* matching_cli_accessors */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean base_is_final(a_base_class_ptr  bcp)
/*
Return TRUE if bcp is on a derivation path involving a "final" base class.
*/
{
  a_boolean                    result = FALSE;
  a_base_class_derivation_ptr  derivation = bcp->derivation;
  
  for (; derivation != NULL; derivation = derivation->next) {
    a_derivation_step_ptr   ds = derivation->path;
    for (; ds != NULL; ds = ds->next) {
      if (ds->base_class->type->variant.class_struct_union.final) {
        result = TRUE;
        goto done;
      }  /* if */
    }  /* for */
  }  /* for */
done:
  return result;
}  /* base_is_final */

static void check_virtual_function_override(
                                 a_class_def_state_ptr   class_state,
                                 a_member_decl_info_ptr  decl_info,
                                 a_symbol_ptr            overridden_sym,
                                 a_base_class_ptr        bcp,
                                 a_base_class_ptr        return_adjustment_bcp)
/*
A member function declaration (overrider_sym) was found to match a virtual
member function (overridden_sym) in a base class (bcp) of the class currently
being defined (described by class_state).  Check that the overriding is valid,
and if not issue diagnostics at the given source position.  If appropriate,
record that overriding in the IL.  If the override involves covariant return
types, return_adjustment_bcp is the base class entry that was determined by
return_types_are_override_compatible.
*/
{
  a_decl_parse_state_ptr  dps = &decl_info->decl_state;
  a_symbol_ptr            overrider_sym = dps->sym;
  a_source_position       *source_pos = &dps->declarator_pos;
  a_type_ptr              class_type = class_state->class_type;
  a_routine_ptr           rout = overrider_sym->variant.routine.ptr;
  a_routine_ptr           rp = overridden_sym->variant.routine.ptr;

  /* Compiler-generated members have no declarator.  Use the associated
     symbol's "decl_position" for diagnostics. */
  if (rout->compiler_generated) source_pos = &overrider_sym->decl_position;
  rout->is_virtual = TRUE;
  if (exception_spec_is_less_restrictive(rout->type, rp->type)) {
    /* The exception specification for the overriding virtual
       function is less restrictive that that of the overridden
       function. */
    report_override_exception_spec_mismatch(overrider_sym, overridden_sym,
                                            source_pos);
  }  /* if */
  check_deleted_function_overrides(overrider_sym, overridden_sym, source_pos);
  if (rp->final) {
    /* Sealed/final virtual functions cannot be overridden. */
    a_boolean  use_final_diag =
              find_attribute(ak_final, rp->source_corresp.attributes) != NULL;
    pos_sy_error(use_final_diag ? ec_override_of_final_function
                                : ec_override_of_sealed_function,
                 source_pos, overridden_sym);
  } else if (base_is_final(bcp)) {
    pos_sy_error(ec_override_of_final_function, source_pos, overridden_sym);
  } else {
    /* Record the virtual function override in the base class entry.
       It can be used later, e.g., for building a virtual function
       table. */
    record_virtual_function_override(decl_info, bcp, rp,
                                     return_adjustment_bcp);
    if (return_adjustment_bcp != NULL) {
      /* The overriding function has a covariant return type.
         Set a flag, since some extra processing may be needed
         later. */
      rout->covariant_return_virtual_override = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (cppcli_enabled && is_managed_class_type(class_type)) {
        pos_error(ec_covariant_override_in_managed_class, source_pos);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if IA64_ABI
      /* If the adjustment will always be trivial, we can reuse the
         virtual function slot from the base class.  However, we
         don't know that until the class layout algorithm has
         determined the base class offsets. */
      record_covariant_override(class_state, bcp,
                                return_adjustment_bcp, rp, rout);
#endif /* IA64_ABI */
    } else if (shares_virtual_function_info(class_type, bcp)) {
      /* The virtual function table is being shared and there
         is no base-class adjustment on the return type, so we
         can use the same virtual function number. */
      rout->virtual_function_number = rp->virtual_function_number;
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cppcli_enabled && is_immediate_managed_class_type(class_type) &&
        is_more_accessible(rp->source_corresp.access, class_state->access)) {
      /* For managed types, the accessibility of a member function cannot be
         reduced through overriding. */
      pos_sy_error(ec_overriding_reduces_accessibility_in_managed_type,
                   source_pos, overridden_sym);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
}  /* check_virtual_function_override */


#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/  /* func_info is not used in some configurations. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static a_boolean check_for_virtual_function(
                                     a_boolean               virtual_specified,
                                     a_member_decl_info_ptr  decl_info,
                                     a_class_def_state_ptr   class_state,
                                     a_func_info_block_ptr   func_info)
/*
A nonstatic member function, represented by decl_info, has been declared and,
depending on the value of virtual_specified, may have been explicitly declared
to be a virtual function.  Even if it has not, it will need to be marked as
virtual if it overrides a virtual function (i.e., if a function with the same
name and type signature was declared virtual in a base class of the current
class).  In addition, information about base class virtual functions that are
overridden by the current declaration is recorded to allow for appropriate
processing later (e.g., the construction of virtual function tables).  If the
current routine is a virtual function either from explicit specification or
from "inheriting" its virtualness, mark the routine entry and return TRUE;
otherwise return FALSE.  class_state describes the parent class of the member
function (which is being defined), and func_info points to some additional
information about the function declarator.
*/
{
  a_decl_parse_state_ptr          dps = &decl_info->decl_state;
  a_type_ptr                      class_type = class_state->class_type;
  a_boolean                       overloaded;
  a_base_class_ptr                bcp, return_adjustment_bcp;
  a_symbol_ptr                    rout_sym = dps->sym;
  a_symbol_ptr                    symbol_list, sym, sym_next;
  a_symbol_ptr                    sym_for_override_registry;
  a_symbol_header_ptr             sym_header_to_search;
  a_routine_ptr                   rout, rp;
  a_scope_ptr                     base_class_scope;
  a_boolean                       any_override_candidates = FALSE;
  a_boolean                       real_override = FALSE;
  an_override_registry_entry_ptr  *registry_ptr;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_symbol_list_entry_ptr         named_override = decl_info->named_overrides;
  a_symbol_ptr                    matching_interface_member = NULL;
  a_boolean                       new_okay = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_source_position               *source_pos = &dps->declarator_pos;

  db_enter(4, "check_for_virtual_function");
  check_assertion(rout_sym->kind == (a_symbol_kind)sk_member_function);
  sym_header_to_search = rout_sym->header;
  rout = rout_sym->variant.routine.ptr;
  rout->is_virtual = virtual_specified;
  /* Compiler-generated members have no declarator.  Use the associated
     symbol's "decl_position" for diagnostics. */
  if (rout->compiler_generated) source_pos = &rout_sym->decl_position;
  registry_ptr = &class_state->override_registry;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cppcli_enabled && decl_info->is_destructor &&
      is_immediate_managed_class_type(class_type)) {
    /* A destructor of a managed type is never virtual even when "virtual" is
       specified.  (A "virtual"-like behavior is instead achieved through the
       so-called "CLI dispose pattern".) */
    if (virtual_specified) {
      pos_remark(ec_virtual_has_no_effect, &dps->virtual_pos);
    }  /* if */
    goto done;
  }  /* if */
next_named_override:
  if (cppcli_enabled && named_override != NULL) {
    sym_header_to_search = named_override->symbol->header;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
          check_virtual_function_override(class_state, decl_info, sym, bcp,
                                          (a_base_class_ptr)NULL);
          dps->override_okay = real_override = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* Not a destructor, so do normal processing. */
      /* Pull out the unique scope identifier for this base class. */
      a_class_symbol_supplement_ptr  base_class_cssp;
      base_class_scope = class_type_supp(bcp->type)->assoc_scope;
      base_class_cssp = symbol_supplement_for_class(bcp->type);
      if (base_class_scope == NULL ||
          bcp->type->variant.class_struct_union.is_nonreal_class) {
        /* This is probably a nonreal base class in a prototype instantiation.
           Don't attempt a lookup in this case. */
        dps->override_okay = TRUE;
        goto next_base_class;
      }  /* if */
      /* Inner loop:  go through all the symbols for this name from the
         base class, looking for one which represents a member function
         (overloaded or simple) from the base class under examination. */
      /*lint --e{446,445} sym modified in loop (LINTBUG) */
      symbol_list = find_symbol_list_in_table(&base_class_cssp->pointers_block,
                                              sym_header_to_search);
      /*lint --e{850} sym modified in loop */
      for (sym = symbol_list; sym != NULL; sym = sym_next) {
        sym_next = sym->next_in_lookup_table;
        sym_for_override_registry = sym;
        if (sym->decl_scope == base_class_scope->number) {
          /* Symbol represents a member of bcp's class. */
          if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
            overloaded = TRUE;
            sym = sym->variant.overloaded_function.symbols;
          } else if (sym->kind == (a_symbol_kind)sk_member_function) {
            overloaded = FALSE;
          } else if (sym->kind == (a_symbol_kind)sk_field ||
                     sym->kind == (a_symbol_kind)sk_static_data_member) {
            /* It's not a symbol for a function, but it's in the same name
               space, so we've looked far enough for this base class. */
            goto next_base_class;
          } else {
            /* It's not a symbol for a function, but it's not in the same name
               space (it's a typedef name, say) so keep scanning. */
            continue;
          }  /* if */
          /* Innermost loop is run only once for simple functions but more
             for overloaded functions. */
          any_override_candidates = FALSE;
          for (; sym != NULL; sym = overloaded ? sym->next : NULL) {
            if (sym->kind != (a_symbol_kind)sk_member_function) {
              /* An overload set could include other symbol kinds, but only
                 sk_member_functions are checked. */
              continue;
            }  /* if */
            rp = sym->variant.routine.ptr;
            if (!rp->is_virtual) {
              /* sym does not represent a virtual function, so (if this is
                 an overload set) keep looking. */
              continue;
#if MICROSOFT_EXTENSIONS_ALLOWED
            } else if (cppcli_enabled &&
                       is_immediate_managed_class_type(class_type)) {
              if (!matching_cli_accessors(rout, rp)) {
                /* One or both routines is a property accessor and the other
                   one doesn't match (either because it is not an accessor, or
                   because it is an accessor for a non-matching property). */
                continue;
              } else if (rp->source_corresp.access ==
                                            (an_access_specifier)as_private &&
                         named_override == NULL) {
                /* Microsoft compilers appear to ignore private virtual
                   members (which should be sealed) in managed class types
                   when determining overriding. */
                continue;
              }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            }  /* if */
            any_override_candidates = TRUE;
            /* We are only interested in virtual functions with the same
               type signature.  Check first whether the parameter types are
               compatible. */
            if (!param_types_are_compatible(rout->type, rp->type,
                                            TCF_NO_FLAGS)) {
              /* Parameter type mismatch. Keep looking for another match in
                 the current overload set. */
              continue;
            }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (microsoft_mode) {
              if (microsoft_bugs && microsoft_version < 1500 &&
                  remove_qualifiers_from_param_types) {
                /* For earlier Microsoft compilers the functions may still not
                   match if the top-level type qualifiers on the parameters
                   (yes, the ones that have been stripped off) do not match. */
                if (!param_types_are_compatible(rout->type, rp->type,
                                     TCF_DONT_IGNORE_PARAM_TYPE_QUALIFIERS)) {
                  /* Keep looking for another match in the current overload
                     set. */
                  continue;
                }  /* if */
              }  /* if */
              if (cppcli_enabled &&
                  is_immediate_managed_class_type(class_type)) {
                /* Check for C++/CLI-style named overriding. */
                if (named_override != NULL) {
                  if (!identical_types(
                                     sym_parent_class(named_override->symbol),
                                     bcp->type)) {
                    /* named_override does not correspond to the current
                       base. */
                    goto next_base_class;
                  }  /* if */
                } else if (decl_info->named_overrides != NULL) {
                  /* A declaration with named overrides, but this pass is for
                     classic overriding of ref base classes: Ignore interface
                     base classes. */
                  if (cli_class_type_kind_is(bcp->type, cctk_interface)) {
                    goto next_base_class;
                  }  /* if */
                }  /* if */
              } else {
                /* Check for non-CLI-style selective overriding. */
                if (rout->overridden_functions != NULL &&
                    !selective_override_match(rout, rp, bcp)) {
                  /* rout is an explicit overrider that doesn't override rp. */
                  continue;
                }  /* if */
              }  /* if */
            }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            /* If rp is virtual, it must be non-static and therefore must
               have a this parameter. */
            check_assertion(skip_typerefs(rp->type)->variant.routine.
                                              extra_info->this_class != NULL);
            /* Be sure the new routine also has a this parameter.  If not,
               it must be static. */
            if (skip_typerefs(rout->type)->variant.routine.extra_info->
                                                         this_class == NULL) {
              /* A static member function "redeclares" a virtual nonstatic
                 member function from a base class.  Normally, that is not
                 allowed, but the Microsoft compilers don't mind (and treat
                 the function as "static" rather than "virtual"). Note that
                 a more derived class can still override the virtual function
                 in the original base. */
              if (!microsoft_bugs) {
                pos_error(ec_virtual_static_not_allowed, source_pos);
              }
              goto done;
            }  /* if */
            /* Check whether the implicit "this" param types are
               consistent -- they must be qualified identically). */
            if (!this_param_types_correspond(rout->type, rp->type,
                                             /*check_as_conversion=*/FALSE,
                                             /*check_as_operands=*/FALSE)) {
              /* Both rp and rout have this parameters, but their types do
                 not correspond.  Keep looking for a match. */
              continue;
            }  /* if */
            /* The parameter types correspond; now compare the return types. */
            if (!return_types_are_override_compatible(rout->type, rp->type,
                                                      &return_adjustment_bcp,
                                                      sym, source_pos)) {
              /* Since, except for the return types, there is a match, there
                 is no need to look any further in the current base class.
                 Advance to the next base class. */
              goto next_base_class;                                       
            }  /* if */
            /* Match */
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (cppcli_enabled) {
              /* Check that required modifiers are specified. */
              if (!func_info->override && !func_info->new_member &&
                  is_ref_class_type(bcp->type) && named_override == NULL) {
                /* If a match is found in a base ref class, the overriding 
                   function should have been declared with "new" or "override" 
                   unless it is a named override. */
                pos_sy_error(ec_new_or_override_required, source_pos, sym); 
              } else if (!virtual_specified && !func_info->new_member 
                         && (cli_class_type_kind_is(class_type, cctk_ref) ||
                             cli_class_type_kind_is(class_type, cctk_value))) {
                /* If a member function of a ref or value class matches a
                   virtual member function from a base class it should have
                   been declared with "new" or "virtual". */
                pos_sy_error(ec_new_or_virtual_required, source_pos, sym); 
              } else if ((func_info->override || func_info->new_member) &&
                         cli_class_type_kind_is(bcp->type, cctk_interface)) {
                /* For members of managed types, "override" and "new" are not
                   allowed when the base class is not a ref class.  We only
                   have to check that the type is a managed interface since
                   override specifiers can be omitted for native types, and
                   value types cannot be base classes. */
                matching_interface_member = sym;
              }  /* if */
              if (func_info->new_member &&
                  cli_class_type_kind_is(bcp->type, cctk_ref)) {
                /* The C++/CLI "new" modifier indicates that a member does not 
                   override a virtual base ref class member with the same
                   signature.  (It does not have an impact on matching
                   interface members, however.) */
                new_okay = TRUE;
                /* Don't establish overriding of a base ref class member if the
                   member function was declared "new".  The exception happens
                   when a named override specifier is also present (e.g.,
                   "virtual void f() new = X::g;"). */
                if (named_override == NULL) goto next_base_class;
              }  /* if */
            }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            check_virtual_function_override(class_state, decl_info, sym, bcp,
                                            return_adjustment_bcp);
            real_override = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (cppcli_enabled &&
                cli_class_type_kind_is(bcp->type, cctk_interface)) {
              /* The "override" modifier cannot be specified to indicate that
                 an interface member is overridden. */
            } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            /* Do not insert code here. */
            {
              dps->override_okay = TRUE;
            }  /* if */
            /* If this declaration amounts to an override of a member of an
               overload set, record some information about it in the
               partial-override-registry.  This allows for a diagnostic later
               if the rest of the members are not also overridden. */
            if (!rout->compiler_generated) {
              update_override_registry(
                                    registry_ptr, sym_for_override_registry,
                                    (a_symbol_ptr)NULL, bcp);
            }  /* if */
            goto next_base_class;                                       
          }  /* for */
          if (any_override_candidates && !rout->compiler_generated) {
            check_assertion(sym_for_override_registry != NULL);
            update_override_registry(registry_ptr, sym_for_override_registry,
                                     rout_sym, bcp);
          }  /* if */
          break;
        }  /* if */
      }  /* for */
    }  /* if */
next_base_class:;
  }  /* for */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (named_override != NULL) {
    if (named_override->next != NULL) {
      /* More than one named override was specified explicitly.  Process the
         next one. */
      named_override = named_override->next;
      goto next_named_override;
    } else {
      /* We've reached the last of the named override entries: The list can
         now be recycled. */
      free_list_of_symbol_list_entries(decl_info->named_overrides);
      named_override = NULL;
      if (!func_info->new_member) {
        /* If the declaration included named override specifiers but not the
           "new" modifier, the normal overriding should also be considered.
           (This is not clear in ECMA-372, but it corresponds to the behavior
           of Microsoft's compiler.) */
        sym_header_to_search = rout_sym->header;
        goto next_named_override;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
done:
  if (real_override) {
    rout->overrides_base_member = TRUE;
    /* rout was found to override at least one specific base class member. */
    if (!rout->compiler_generated &&
        rout->special_kind != (a_special_function_kind)sfk_destructor &&
        symbol_supplement_for_class(class_type)->base_check) {
      /* If a class has the "base_check" attribute, overriding virtual
         member functions must have the "override" attribute.  Destructors
         and compiler-generated functions are exempted from this
         requirement. */
      if (find_attribute(ak_override, dps->prefix_attributes) == NULL &&
          find_attribute(ak_override, dps->id_attributes) == NULL) {
        pos_error(ec_missing_override_attr_in_base_check_class,
                  &dps->declarator_pos);
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (func_info->override && !dps->override_okay) {
    if (cppcli_enabled && class_type_supp(class_type)->assembly_index != 0) {
      /* The class was loaded from an assembly file.  Because of limitations
         of the metadata, the code generated from such a file can contain
         extraneous "override" modifiers; these should just be silently
         ignored. */
    } else if (matching_interface_member != NULL) {
      /* "override" was used only to override one or more interface members;
         this is not normally valid, but Microsoft compilers only issue a
         warning on such harmless cases. */
      pos_sy_warning(ec_override_for_interface_member, source_pos,
                     matching_interface_member);
    } else {
      pos_diagnostic(is_immediate_managed_class_type(class_type) ? es_warning
                                                                 : es_error,
                     ec_override_member_does_not_override, source_pos);
    }  /* if */
  } else if (func_info->new_member && !new_okay) {
    if (cppcli_enabled && class_type_supp(class_type)->assembly_index != 0) {
      /* The class was loaded from an assembly file.  Because of limitations
         of the metadata, the code generated from such a file can contain
         extraneous "new" modifiers; these should just be silently ignored. */
    } else {
      /* The member function is marked as "new" but no matching
         member function was found in a base class. */
      pos_warning(ec_new_requires_matching_base_member, source_pos);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  if (rout->is_virtual) {
    /* Reflect the presence of a virtual function in the enclosing class. */
    class_type->variant.class_struct_union.any_virtual_functions = TRUE;
    class_type->variant.class_struct_union.
                 any_virtual_functions_including_in_base_classes = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cppcli_enabled) {
      if (virtual_specified) {
        if (class_state->access == (an_access_specifier)as_private &&
            is_immediate_managed_class_type(class_type) &&
            !func_info->sealed) {
          /* A private virtual member function of a managed type should
             be marked as sealed. */
          pos_warning(ec_private_virtual_member_function_not_sealed, 
                      source_pos);
        }  /* if */
      } else {
        /* An override modifier often requires that the function also be
           declared with an explicit "virtual" keyword. */
        if (func_info->override) {
          pos_error(ec_override_requires_virtual, source_pos);
        } else if (is_immediate_managed_class_type(class_type)) {
          if (func_info->abstract) {
            pos_error(ec_abstract_requires_virtual, source_pos);
          } else if (func_info->sealed) {
            pos_error(ec_sealed_requires_virtual, source_pos);
          } else if (named_override != NULL) {
            pos_error(ec_named_override_requires_virtual, source_pos);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (func_info->sealed || func_info->abstract) {
    pos_error(ec_function_modifier_requires_virtual_function, source_pos);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if (rout->final) {
    pos_error(ec_function_modifier_requires_virtual_function, source_pos);
    rout->final = FALSE;
  }  /* if */
  db_exit();
  return rout->is_virtual;
}  /* check_for_virtual_function */

#if ABI_COMPATIBILITY_VERSION >= 232 && !IA64_ABI

static void set_virtual_function_numbers_for_overload_set(
                                        a_symbol_ptr              sym,
                                        a_virtual_function_number *number_ptr)
/*
sym is a symbol in a member function overload set.  Process it and its
successors in the set, setting the virtual function number for each virtual
function for which it has not already been set.  number_ptr is the address
of the field in parent class's class-type-supplement that tracks the highest
number assigned thus far.
*/
{
  /* Skip over members that don't need a new virtual function number:
     Member templates and projected members. */
  for (; sym != NULL; sym = sym->next) {
    if (sym->kind == (a_symbol_kind)sk_member_function) break;
  }  /* for */
  if (sym != NULL) {
    if (sym->next != NULL) {
      /* If there are additional functions in the overload set, process
         them first.  This is because the symbols on the list are in
         the opposite order to that in which they were declared. */
      set_virtual_function_numbers_for_overload_set(sym->next, number_ptr);
    }  /* if */
    /* Update the routine entry with the next available virtual function
       number. */
    update_virtual_function_number(sym->variant.routine.ptr, number_ptr);
  }  /* if */
}  /* set_virtual_function_numbers_for_overload_set */

#endif /* ABI_COMPATIBILITY_VERSION >= 232 && !IA64_ABI */

static void set_virtual_function_numbers(a_class_def_state_ptr  cdsp)
/*
Update the virtual function numbers of the virtual function members of the
class associated with cdsp.
*/
{
  a_type_ptr                 class_type = cdsp->class_type;
  a_virtual_function_number  *number_ptr;

  if (class_type->variant.class_struct_union.any_virtual_functions) {
    /* We will pass in the address of the field that tracks the highest
       virtual function that has been assigned thus far.  (It may be nonzero
       at this point if the virtual function info for this class is shared
       with that of one of its base classes.) */
    number_ptr = &class_type->variant.class_struct_union.extra_info->
                                         highest_virtual_function_number;

#if ABI_COMPATIBILITY_VERSION >= 232 && !IA64_ABI
    /* Traverse the symbol list rather that the IL scope's function list.
       Both should reflect declaration order except in the handling of
       overloaded functions.  We do want to handle members of an overload set
       as a group. */
    { a_symbol_ptr  sym = symbol_supplement_for_class(class_type)->symbols;
      for (; sym != NULL; sym = sym->next_in_scope) {
        if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
          /* Examine each of the members of the overload set. */
          set_virtual_function_numbers_for_overload_set(
                         sym->variant.overloaded_function.symbols, number_ptr);
        } else if (sym->kind == (a_symbol_kind)sk_member_function) {
          update_virtual_function_number(sym->variant.routine.ptr, number_ptr);
        }  /* if */
      }  /* for */
    }
#else /* ABI_COMPATIBILITY_VERSION < 232 || IA64_ABI */
#if IA64_ABI
    /* In the IA-64 ABI, covariant overriders share a virtual slot with the
       overridden function when the covariant adjustment is zero. */
    { a_covariant_override_ptr  cop = cdsp->covariant_overrides;
      for (; cop != NULL; cop = cop->next) {
        if (shares_virtual_function_info(class_type, cop->bcp) &&
            cop->adjustment_bcp->offset == 0 &&
            !any_virtual_steps_in_derivation(cop->adjustment_bcp)) {
          cop->overriding->virtual_function_number =
                                     cop->overridden->virtual_function_number;
        }  /* if */
      }  /* for */
      free_covariant_overrides(cdsp);
    }
#endif /* IA64_ABI */
    /* Traverse the routines list (which is in declaration order). */
    { a_routine_ptr  rp = class_type->variant.class_struct_union.extra_info
                                    ->assoc_scope->routines;
      for (; rp != NULL; rp = rp->next) {
        update_virtual_function_number(rp, number_ptr);
      }  /* for */
    }
#endif /* ABI_COMPATIBILITY_VERSION >= 232 && !IA64_ABI */
  }  /* if */
}  /* set_virtual_function_numbers */


/* Previously allocated derivation-step entries available for reuse. */
static a_derivation_step_ptr avail_derivation_steps;

void free_derivation_step(a_derivation_step_ptr  step)
/*
Return a derivation step entry (or a list of them) to the free list for
reuse at another time.
*/
{
  a_derivation_step_ptr	step_tail;

  /* Find the last entry on the list. */
  for (step_tail = step; step_tail->next != NULL;
       step_tail = step_tail->next) { step_tail->base_class = NULL; }
  step_tail->next = avail_derivation_steps;
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
void db_path(a_derivation_step_ptr dsp,
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
        fprintf(f_debug, "@%lu", (unsigned long)dsp->base_class->offset);
#if !IA64_ABI
        if (dsp->base_class->is_virtual) {
          fprintf(f_debug, "(ptr @%lu)",
                  (unsigned long)dsp->base_class->pointer_offset);
        }  /* if */
#endif /* !IA64_ABI */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* db_path */


void db_abbreviated_base_class(a_base_class_ptr  bcp)
/*
Dump a base class entry, showing minimal information and not appending a
new line, for debug purposes.
*/
{
  (void)fputc('"', f_debug);
  db_type_name(bcp->type);
  if (bcp->derived_class != NULL) {
    fputs("\" in \"", f_debug);
    db_type_name(bcp->derived_class);
  }  /* if */
  fputc('"', f_debug);
}  /* db_abbreviated_base_class */


void db_base_class(a_base_class_ptr  bcp,
                   a_boolean         show_offset)
/*
Dump a base class entry, for debug purposes.
*/
{
  a_base_class_derivation_ptr  bcdp;
  a_boolean                    comma_needed = FALSE;

  if (bcp == NULL) {
    fprintf(f_debug, "<NULL>\n");
    goto done;
  }  /* if */
  (void)fputc('"', f_debug);
  db_type_name(bcp->type);
  if (bcp->derived_class != NULL) {
    fputc('"', f_debug);
    fprintf(f_debug, " (%lu/%d)",
            bcp->decl_position.seq, bcp->decl_position.column);
    fputs(", base class of \"", f_debug);
    db_type_name(bcp->derived_class);
  }  /* if */
  fputs("\": ", f_debug);
  if (show_offset) {
    fprintf(f_debug, "size = %lu, offset = %lu",
#if CFRONT_OBJECT_CODE_COMPATIBILITY
            bcp->complete_subobject ? (unsigned long)bcp->type->size :
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
              (unsigned long)bcp->type->variant.class_struct_union.extra_info->
                                             size_without_virtual_base_classes,
            (unsigned long)bcp->offset);
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
#if !IA64_ABI
    if (show_offset) {
      fprintf(f_debug, " (ptr offset = %lu",
              (unsigned long)bcp->pointer_offset);
      if (bcp->pointer_base_class != NULL) {
        fputs(", in ", f_debug);
        db_type_name(bcp->pointer_base_class->type);
      }  /* if */
      fputc(')', f_debug);
    }  /* if */
#endif /* !IA64_ABI */
    comma_needed = TRUE;
  }  /* if */
  if (bcp->shares_virtual_function_info) {
    if (comma_needed) fputs(", ", f_debug);
    fputs("shares vtbl", f_debug);
    comma_needed = TRUE;
  }  /* if */
  if (bcp->ambiguous) {
    if (comma_needed) fputs(", ", f_debug);
    fputs("ambig", f_debug);
    comma_needed = TRUE;
  }  /* if */
  bcdp = bcp->derivation;
  if (bcdp != NULL && comma_needed) fputc(',', f_debug);
  fputc('\n', f_debug);
  for (; bcdp != NULL; bcdp = bcdp->next) {
    fprintf(f_debug, "    %sderiv%s: ", (bcdp->direct ? "direct " : ""),
            ((bcp->is_virtual && bcdp->preferred) ? " (pref'd)" : ""));
    db_path(bcdp->path, show_offset);
    fputs(" (", f_debug);
    db_access_control(bcdp->access);
    fputs(")\n", f_debug);
  }  /* for */
done:;
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
  check_assertion((a_boolean)base_class->direct == (count == 1));
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
    if (debug_level >= 5) {
      if (ovfp != NULL) {
        fputs("base class = ", f_debug);
        db_base_class(base_class, /*show_offset=*/FALSE);
      }  /* if */
    }  /* if */
  for (; ovfp != NULL; ovfp = ovfp->next) {
    if (debug_level >= 5) {
      db_virtual_function_override(ovfp);
    }  /* if */
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
#endif /* DEBUG */

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
      if (!same_entities(dsp1->base_class->type, dsp2->base_class->type)) {
        break;
      }  /* if */
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

#if !IA64_ABI

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

#endif /* !IA64_ABI */

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
  class D : public B, public C,             //     \|/  
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
      if (base_class->data_section_base_class == NULL &&
          !dsp->base_class->direct) {
        /* dsp refers to a virtual base class which is not itself embedded.
           Only mark base_class as embedded within dsp->base_class (and then
           only provisionally) if this is a direct derivation. */
        if (dsp->next->base_class == base_class) {
          /* It is a direct derivation.  See if the base class that corresponds
             to base_class is embedded in the context of dsp->base_class->type.
             If so, defer the designation. */
          bcp = corresponding_base_class(base_class, dsp->base_class->type,
                                         (a_base_class_ptr)NULL);
          if (bcp->data_section_base_class == NULL) {
            base_class->data_section_base_class = dsp->base_class;
          }  /* if */
        }  /* if */
      }  /* if */
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
#if !IA64_ABI
    /* Set the pointer-base-class (the nonvirtual base class in which a
       pointer to this virtual base class may be found) if appropriate. */
    if (path != NULL) {
      if (base_class->pointer_base_class == NULL) {
        set_pointer_base_class(base_class, new_bcdp->path);
      }  /* if */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
      /* In cfront mode set the data section base class, which is where the
         data section of this virtual base class may be embedded. */
      if (base_class->data_section_base_class == NULL ||
          base_class->data_section_base_class->is_virtual) {
        set_data_section_base_class(base_class, new_bcdp->path);
      }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
    }  /* if */
#endif /* !IA64_ABI */
    /* Return a different path than the one stored in the base-class-
       derivation entry.  This is because each virtual base class is the
       start of a new segment. */
    path = step;
  }  /* if */
  return path;
}  /* update_base_class_derivation */


static void set_shares_virtual_function_info_flag(a_type_ptr       class_type,
                                                  a_base_class_ptr base_class)
/*
Set the shares_virtual_function_info flag for certain base classes of
class_type, if appropriate.  If base_class is NULL, then if class_type has
virtual functions and a non-NULL virtual_function_info_base_class pointer,
set the flag in the base class pointed to.  If base_class, which is a base
class of class_type, is non-NULL, similar processing applies to the type of
the base class.
*/
{
  a_type_ptr             tp = NULL;
  a_base_class_ptr       bcp;
#if !IA64_ABI
  a_derivation_step_ptr  step;
#else /* IA64_ABI */
  a_base_class_ptr       primary_bcp;
#endif /* !IA64_ABI */

  db_enter(4, "set_shares_virtual_function_info_flag");
  if (base_class == NULL) {
    /* Set the flag, if appropriate, based on the properties of the class. */
    tp = class_type;
  } else {
    /* Set the flag, if appropriate, based on the properties of the base
       class. */
    tp = base_class->type;
  }  /* if */
  if (needs_virtual_function_table(tp)) {
    /* The type (class type or base class type) does have virtual functions. */
    bcp = tp->variant.class_struct_union.extra_info->
                                              virtual_function_info_base_class;
    if (bcp != NULL) {
      /* A base class has been designated with which to share the virtual
         function info. */
      if (base_class == NULL) {
        /* bcp is already a base class of class_type. */
      } else {
        /* bcp now points to a base class of base_class; change it to point
           to the corresponding base class of class_type. */
        bcp = corresp_base_class(bcp, base_class);
      }  /* if */
      /* Set the flag. */
      bcp->shares_virtual_function_info = TRUE;
      /* It may be that bcp is not a direct base class of the type (class type
         or base class type), in which case it may be that flag has to be set
         on an intervening base class as well. */
#if !IA64_ABI
      if (!bcp->direct) {
        step = bcp->derivation->path;
        if (base_class != NULL) {
          /* Advance through the derivation path to the step immediately after
             the step that points to base class. */
          while (step->base_class != base_class) step = step->next;
          step = step->next;
        }  /* if */
        /* Continue through the derivation path looking for the first base
           class that has any virtual functions; stop at the first (if any),
           since the flags for any others would already have been set. */
        for (; step->base_class != bcp; step = step->next) {
          if (step->base_class->type->
                   variant.class_struct_union.any_virtual_functions) {
            step->base_class->shares_virtual_function_info = TRUE;
            break;
          }  /* if */
        }  /* while */
      }  /* if */
#else /* IA64_ABI */
      /* There may be virtual steps in the derivation so we must work down
         from the derived class to the base class.  */
      primary_bcp =
        tp->variant.class_struct_union.extra_info->primary_base_class; 
      /* Walk down the chain of primary bases, making sure that the
         shares_virtual_function_info is TRUE for each. */
      while (primary_bcp != NULL) {
        if (base_class != NULL) {
          primary_bcp = corresp_base_class(primary_bcp, base_class);
        }  /* if */
        if (primary_bcp == bcp) break;
        base_class = primary_bcp;
        primary_bcp = base_class->type->variant.class_struct_union.extra_info
                                      ->primary_base_class;
        if (primary_bcp != NULL) {
          base_class->shares_virtual_function_info = TRUE;
        }  /* if */
      }  /* while */
#endif /* !IA64_ABI */
    }  /* if */
  }  /* if */
  db_exit();
}  /* set_shares_virtual_function_info_flag */


static void set_target_of_conversion_function_flag(a_type_ptr  class_type)
/*
If it has not been set yet, set the target_of_conversion_function flag for
class_type.  Also set the flag in each of class_type's base classes.
*/
{
  a_class_symbol_supplement_ptr  cssp;
  a_base_class_ptr               bcp;

  cssp = symbol_supplement_for_class(class_type);
  if (cssp->target_of_conversion_function) {
    /* Already set.  In particular, this check saves the overhead of going
       through all the base classes more than necessary. */
  } else {
    /* The flag has not been set yet. */
    cssp->target_of_conversion_function = TRUE;
    /* Set the flag in each direct base class.  Since this is done
       recursively, all base class will be updated. */
    for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
      if (bcp->direct) set_target_of_conversion_function_flag(bcp->type);
    }  /* for */
  }  /* if */
}  /* set_target_of_conversion_function_flag */
    

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
  a_boolean            any_direct_virtual_base_class_fixup;

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
      if (bcp->is_virtual && same_entities(bcp->type,
                                           base_class_to_copy->type)) {
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
  /* base_class_to_copy->orig_type is not copied because an indirect base
     class could have multiple derivation paths with different names used
     to express the base class type. */
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
  }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  /* Check for ambiguity. */
  for (bcp = base_classes_of(new_class); bcp != NULL; bcp = bcp->next) {
    if (same_entities(bcp->type, new_bcp->type)) {
      check_assertion(!(bcp->is_virtual && new_bcp->is_virtual));
      /* Ambiguous base class. */
      bcp->ambiguous = TRUE;
      new_bcp->ambiguous = TRUE;
      break;
    }  /* if */
  }  /* for */
  /* Add the base classes of the current indirect base class to the base
     classes list of the most-derived-class. */
  any_direct_virtual_base_class_fixup = FALSE;
  for (bcp = base_classes_of(new_bcp->type); bcp != NULL; bcp = bcp->next) {
    if (!bcp->direct) {
      continue;
    } else if (bcp->is_virtual) {
      /* A virtual base class is marked as "direct" if any of its paths
         is direct.  However, for our purposes, the "first" path (first in
         a depth-first left-to-right traversal of the derivation graph)
         must be direct. */
      if (!bcp->derivation->direct) {
        any_direct_virtual_base_class_fixup = TRUE;
        continue;
      }  /* if */
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
  /* Set shares_virtual_function_info for a base class of new_bcp, if
     appropriate. */
  set_shares_virtual_function_info_flag(new_class, new_bcp);
  /* Do path fixup, if necessary. */
  if (any_direct_virtual_base_class_fixup) {
    a_base_class_ptr             fixup_bcp;
    a_base_class_derivation_ptr  bcdp;

    for (bcp = base_classes_of(new_bcp->type); bcp != NULL; bcp = bcp->next) {
      if (bcp->direct && bcp->is_virtual) {
        bcdp = bcp->derivation;
        if (!bcdp->direct) {
          /* Add path information about a direct virtual base class of
             new_direct_bcp that was not first in the depth-first
             left-to-right traversal of the latter's derivation graph. */
          /* Find the base class in new_class that corresponds to bcp. */
          fixup_bcp = corresp_base_class(bcp, new_bcp);
          /* Find the derivation, which has the appropriate access. */
          do {
            bcdp = bcdp->next;
          } while (!bcdp->direct);
          /* Update the path list. */
          (void)update_base_class_derivation(fixup_bcp, path, bcdp->access);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
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
position of its first appearance in the depth-first left-to-right traversal
of the base specifiers graph.  This position is independent of the which
appearance of the base class happens to have been marked preferred.
*/
{
  a_base_class_derivation_ptr
                       bcdp, preferred_bcdp = NULL;
  an_access_specifier  access,
                       preferred_access = (an_access_specifier)as_inaccessible;

  db_enter(4, "set_preferred_base_class_derivation");
  /* Has this set of base class derivations been checked yet?  This can be
     determined by seeing if any has the preferred flag set already. */
  for (bcdp = base_class->derivation; bcdp != NULL; bcdp = bcdp->next) {
    /* Preferred flag has already been set for this group of derivations. */
    if (bcdp->preferred) goto done;
  }  /* for */
  /* Traverse the linked list of base class derivations. */
  for (bcdp = base_class->derivation; bcdp != NULL; bcdp = bcdp->next) {
    if (bcdp->path->base_class->is_virtual &&
        bcdp->path->base_class != base_class) {
      /* If this virtual base class has a virtual base class in its
         derivation path, the intermediate step has to be processed first. */
      set_preferred_base_class_derivation(class_type, bcdp->path->base_class);
    }  /* if */
    /* Determine the accessibility of a public member of the virtual base
       class in the context of the most derived class. */
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
        check_assertion(preferred_bcdp != NULL);
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
  /* coverity[var_deref_op] */
  preferred_bcdp->preferred = TRUE;
done:;
  db_exit();
}  /* set_preferred_base_class_derivation */


static void mark_base_dependent_if_needed
                                   (a_base_class_ptr              bcp, 
                                    a_class_def_state_ptr         class_state,
                                    a_base_class_sequence_number  proto_num)
/*
bcp represents a direct base class generated during a real instantiation of a
class template (the instance is described by class_state) and proto_num is the
sequence number of the corresponding base in the prototype instantiation.
If appropriate, mark bcp as being dependent (and update related bookkeeping
information).
*/
{
  a_base_class_ptr  proto_bcp;
  a_type_ptr        proto_type;

  check_assertion(bcp->direct);
  if (class_state->is_nonreal_instantiation) {
    if (is_or_contains_template_param(bcp->type)) {
      bcp->ignore_during_dependent_lookup = TRUE;
      symbol_supplement_for_class(class_state->class_type)
                                          ->any_dependent_base_classes = TRUE;
    }  /* if */
  } else if (class_state->is_template_instantiation) {
    check_assertion(proto_num != 0);
    check_assertion_str2(class_state->corresp_prototype_tag_sym != NULL,
                         "mark_base_dependent_if_needed:",
                         "no corresp_prototype_tag_sym");
    proto_type = class_state->corresp_prototype_tag_sym
                            ->variant.class_struct_union.type;
    proto_bcp = base_classes_of(proto_type);
    /* Find the corresponding prototype base class.  Note that in error cases a
       given sequence number could be missing from the list. */
    while (proto_bcp != NULL && proto_bcp->direct_base_number != proto_num) {
      proto_bcp = proto_bcp->next;
    }  /* while */
    if (proto_bcp != NULL) {
      /* Normal case: We find the corresponding base of the prototype
         instantiation:  Set flags in the instantiated entities accordingly. */
      bcp->ignore_during_dependent_lookup =
                                    proto_bcp->ignore_during_dependent_lookup;
      /* Indicate that this class has a dependent base if this base class
         is dependent or any of its base classes are dependent. */
      if (bcp->ignore_during_dependent_lookup ||
          symbol_supplement_for_class(bcp->type)->any_dependent_base_classes) {
        symbol_supplement_for_class(class_state->class_type)
                                          ->any_dependent_base_classes = TRUE;
      }  /* if */
    } else {
      /* If we did not find a matching base class there must have been an
         earlier error. */
      check_assertion(total_errors != 0);
    }  /* if */
  }  /* if */
}  /* mark_base_dependent_if_needed */

#if IA64_ABI

static a_base_class_ptr *compute_preorder_base_classes(
                                                a_type_ptr        type_ptr,
                                                a_base_class_ptr  base,
                                                a_base_class_ptr  *end_of_list)
/*
Compute the preorder base class list for type_ptr.  base is the base of
type_ptr whose bases should be added to the list, or NULL if the direct bases
of type_ptr should be added.  *end_of_list points to the end of the preorder
list.  Returns a pointer to the new end of the list.
*/
{
  a_base_class_ptr first_base, bcp, new_base, old_base;
  a_base_class_sequence_number next_base;

  first_base = base_classes_of((base == NULL) ? type_ptr : base->type);
  /* Skip to the end if this type has no base classes.  */
  if (first_base == NULL) goto done;
  bcp = first_base;
  for (next_base = 1; ; next_base++) {
    /* Care must be taken to traverse the direct base classes in declaration
       order.  In particular, it is not sufficient to simply traverse the
       base class list and act on the direct bases.  Consider the following
       example:
         struct V {};
         struct B: virtual V {};
         struct D: virtual B, virtual V {};
       The base class list for D will first list the virtual base V because
       it is the "leftmost" base class of B, but it is also marked "direct".
       This for-loop therefore enumerates the direct base numbers which we
       then search for using an additional loop.  In most cases, this will
       only require a single traversal of the base class list, but in some
       unusual hierarchies the cost of the nested loops could be quadratic
       in the length of the base class list. */
    a_base_class_ptr start = bcp;
    /* Look for the base with the next sequence number. */
    while (bcp->direct_base_number != next_base) {
      bcp = bcp->next;
      if (bcp == NULL) {
        bcp = first_base;
      }  /* if */
      /* If we get back to the place where we started, then there is no next
         base.  */
      if (bcp == start) break;
    }  /* while */
    /* If there was no base with the next sequence number then we have reached
       the end of the list.  */
    if (bcp->direct_base_number != next_base) break;
    /* Find the base of type_ptr that corresponds to bcp. */
    if (base == NULL) {
      new_base = bcp;
    } else {
      new_base = corresp_base_class(bcp, base);
    } /* if */
    /* If the new_base is virtual, we may already have a copy on the
       list. */
    if (new_base->is_virtual) {
      for (old_base = preorder_base_classes_of(type_ptr); 
           old_base != NULL;
           old_base = old_base->next_preorder) {
        if (old_base == new_base) break;
      }  /* for */
      /* If the virtual base is already on the preorder list, skip this
         base. */
      if (old_base != NULL) continue;
    }  /* if */
    /* Add new_base to the list. */
    *end_of_list = new_base;
    end_of_list = &new_base->next_preorder;
    /* Recursively add the base classes of new_base. */
    end_of_list = compute_preorder_base_classes(type_ptr, new_base,
                                                end_of_list);
  }  /* for */
done:
  return end_of_list;
}  /* compute_preorder_base_classes */


static a_boolean is_nearly_empty_class(a_type_ptr type)
/*
Return TRUE if (and only if) the type (which must be a class, struct, or 
union type) is "nearly empty", i.e, has no data except a virtual pointer,
as defined in the IA64 ABI.
*/
{
  a_boolean                   nearly_empty = TRUE;
  a_field_ptr                 fp;
  a_base_class_ptr            bcp;
  a_class_type_supplement_ptr ctsp;

  check_assertion(is_immediate_class_type(type));
  ctsp = type->variant.class_struct_union.extra_info;
  /* If there is no virtual function table, then the class is not nearly 
     empty. */
  if (!needs_virtual_function_table(type) &&
      ctsp->virtual_function_info_base_class == NULL) {
    nearly_empty = FALSE;
  } else {
    /* There must be no non-static data members other than zero-width
       bitfields. */
    for (fp = type->variant.class_struct_union.field_list;
         fp != NULL;
         fp = fp->next) {
      if ((!fp->is_bit_field || fp->bit_size != 0) &&
          !(fp->compiler_generated && has_name(fp))) {
        /* Named compiler generated fields are virtual table pointers and
           prelowered bases, which may appear in nearly empty base classes
           (the base class case will be checked more closely below, however).
           Unnamed compiler generated fields correspond to anonymous union
           parent fields: They cannot appear in nearly empty base classes. */
        nearly_empty = FALSE;
        break;
      }  /* if */
    }  /* for */
    /* Check the base classes. */
    if (nearly_empty) {
      a_boolean has_non_virtual_nearly_empty_base = FALSE;
      for (bcp = base_classes_of(type); bcp != NULL; bcp = bcp->next) {
        /* Virtual (and morally virtual) bases don't matter. */
        if (any_virtual_steps_in_derivation(bcp)) continue;
        /* All direct bases must be empty or nearly empty. */
        if (bcp->direct &&
            !bcp->type->variant.class_struct_union.is_empty_class) {
          if (!is_nearly_empty_class(bcp->type)) {
            nearly_empty = FALSE;
            break;
          }  /* if */
          /* There can be at most one non-virtual, nearly empty direct base
             class. */
          if (has_non_virtual_nearly_empty_base) {
            nearly_empty = FALSE;
            break;
          }  /* if */
          has_non_virtual_nearly_empty_base = TRUE;
        }  /* if */
        /* Empty bases at non-zero offsets make a class not "nearly empty". */
        if (bcp->type->variant.class_struct_union.is_empty_class &&
            bcp->offset != 0) {
          nearly_empty = FALSE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  return nearly_empty;
}  /* is_nearly_empty_class */

#endif /* !IA64_ABI */

static void set_virtual_function_info_base_class(a_base_class_ptr bcp)
/*
Record the fact that bcp is the base with which bcp->derived_class
shares virtual function info.
*/
{
  a_type_ptr                  class_type, base_class_type;
  a_class_type_supplement_ptr ctsp, base_ctsp;
  a_base_class_ptr            base_bcp;

  class_type = bcp->derived_class;
  ctsp = class_type->variant.class_struct_union.extra_info;
#if IA64_ABI
  ctsp->primary_base_class = bcp;
#endif /* IA64_ABI */
  base_class_type = bcp->type;
  base_ctsp = base_class_type->variant.class_struct_union.extra_info;
  base_bcp = base_ctsp->virtual_function_info_base_class;
  if (base_bcp == NULL) {
    /* The base class does not share virtual function info with its
       own base classes. */
    ctsp->virtual_function_info_base_class = bcp;
  } else {
    /* Refer to the same virtual_function_info_base_class as the
       direct base class does.  */
    /* base_bcp is a base class of bcp->type; we need to find
       the corresponding base class of type_ptr. */
    ctsp->virtual_function_info_base_class = corresp_base_class(base_bcp, bcp);
  }  /* if */
  /* Advance the virtual function count so that any new virtual
     functions will be tacked on at the end of the shared virtual
     function info block.  (Redeclarations will use the slot
     already reserved for the function.) */
  ctsp->highest_virtual_function_number =
                                   base_ctsp->highest_virtual_function_number;
}  /* set_virtual_function_info_base_class */


#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/ /* type_ptr only used when Microsoft extensions are enabled. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static void scan_inheritance_kind(a_type_ptr           type_ptr,
                                  a_boolean            *is_virtual,
                                  an_access_specifier  *access,
                                  a_boolean            *explicit_access)
/*
Scan any of the keywords "virtual", "public", "private", and "protected"
that might precede a base class specifier in the definition of the class
represented by type_ptr.  Set *is_virtual to TRUE if "virtual" is seen,
and set *access to any explicitly mentioned access specifier (if such an
explicit specifier is seen, set *explicit_access to TRUE).  Issue any
diagnostics that can be emitted based on this information.
*/
{
  a_boolean  access_already_specified = FALSE;

  *is_virtual = FALSE;
  for (;;) {
    if (curr_token == tok_virtual) {
      if (*is_virtual) {
        pos_error(ec_dupl_decl_specifier, &pos_curr_token);
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (is_immediate_managed_class_type(type_ptr)) {
        pos_error(ec_virtual_base_for_managed_class, &pos_curr_token);
      } else if (type_ptr->variant.class_struct_union.is_interface) {
        pos_error(ec_interface_cannot_have_virtual_base, &pos_curr_token);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        *is_virtual = TRUE;
      }  /* if */
    } else if (curr_token == tok_public || curr_token == tok_protected ||
               curr_token == tok_private) {
      if (access_already_specified) {
        pos_error(ec_access_already_specified, &pos_curr_token);
      } else {
        if (curr_token == tok_public) {
          *access = (an_access_specifier)as_public;
        } else {
          if (curr_token == tok_protected) {
            *access = (an_access_specifier)as_protected;
          } else {
            *access = (an_access_specifier)as_private;
          }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (is_immediate_managed_class_type(type_ptr)) {
            pos_error(
                  ec_managed_class_type_cannot_have_private_or_protected_base,
                  &pos_curr_token);
            *access = (an_access_specifier)as_public;
          } else if (type_ptr->variant.class_struct_union.is_interface) {
            pos_error(ec_interface_cannot_have_private_or_protected,
                      &pos_curr_token);
            *access = (an_access_specifier)as_public;
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
        access_already_specified = TRUE;
      }  /* if */
    } else {
      /* Leave the loop and scan the class name. */
      break;
    }  /* if */
    (void)get_token();
  }  /* for */
  *explicit_access = access_already_specified;
}  /* scan_inheritance_kind */


#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/ /* type only used when Microsoft extensions are enabled. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static a_boolean check_base_class_type(a_type_ptr        type,
                                       a_type_ptr        base_type)
/*
Check whether a derived class type (type) can have a base class of type
base_type.  If so, return TRUE, and, for some template base classes in
some Microsoft modes, update the DLL interface of the base type.  Otherwise,
issue an error and return FALSE.
*/
{
  a_boolean   okay = TRUE;
  a_type_ptr  base_class_type = skip_typerefs(base_type);
  /* If it is the class now being defined or if it is a union or if it has
     been declared but not yet defined, issue an error and skip over this
     class: it is not a valid base class name. */
  /* In Microsoft mode the last field of a class may be a zero-length array;
     such a class may not be a base class. */
  if (base_class_type->kind == (a_type_kind)tk_union ||
      base_class_type->
                  variant.class_struct_union.contains_flexible_array_member) {
    error(ec_bad_base_class);
    okay = FALSE;
  } else {
    /* Force instantiation if the base class is a template class. */
    check_assertion(is_class_struct_union_type(base_class_type));
    complete_class_type_is_needed(base_class_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      if (is_immediate_managed_class_type(type)) {
        /* Managed class types have different constraints.  Value classes and
           interface classes can only derive from interface classes.  Ref
           classes can derive from at most one other ref class; other base
           classes must be interface classes. */
        if (!cli_class_type_kind_is(base_class_type, cctk_interface)) {
          switch (class_type_supp(type)->cli_class_type_kind) {
            case cctk_ref:
              if (cli_class_type_kind_is(base_class_type, cctk_ref)) {
                a_base_class_ptr  bcp = base_classes_of(type);
                for (; bcp != NULL; bcp = bcp->next) {
                  if (bcp->direct &&
                      cli_class_type_kind_is(bcp->type, cctk_ref)) {
                    pos_ty_error(ec_ref_class_has_multiple_ref_bases,
                                 &error_position, bcp->type);
                    break;
                  }  /* if */
                }  /* for */
              } else {
                pos_error(ec_invalid_ref_class_base, &error_position);
              }  /* if */
              break;
            case cctk_value:
              { a_type_ptr  system_value_type = cli_system_value_type();
                if (!identical_types(base_class_type, system_value_type)) {
                  pos_error(ec_invalid_value_class_base, &error_position);
                }  /* if */
              }
              break;
            case cctk_interface:
              pos_error(ec_invalid_interface_class_base, &error_position);
              break;
            default:
              unexpected_condition();
          }  /* switch */
        }  /* if */
      } else {
        if (cppcli_enabled &&
            is_immediate_managed_class_type(base_class_type)) {
          pos_error(ec_managed_base_for_standard_class, &error_position);
        }  /* if */
        if (microsoft_version >= 1300) {
          /* Recent Microsoft compilers apply the dllimport/dllexport
             attributes of a derived class to any base class type that is an
             implicit class template specialization (unless a DLL interface
             was already specified on that type). */
          a_decl_modifier  flags = class_type_supp(type)->decl_modifiers &
                                                                  DM_DLLFLAGS;
          if (flags != 0) {
            update_dll_info_for_class(base_class_type, flags,
                                      /*explicit_inst=*/FALSE,
                                      /*adjust_template_base=*/TRUE,
                                      &error_position);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (base_class_type->variant.class_struct_union.final) {
      /* Final/sealed classes cannot be derived from. */
      an_attribute_ptr  final_ap =
         find_attribute(ak_final, base_class_type->source_corresp.attributes);
      pos_error(final_ap != NULL ? ec_final_base_class : ec_sealed_base_class,
                &pos_curr_token);
      okay = FALSE;
    }  /* if */
    if (is_incomplete_type(base_class_type)) {
      if ((gpp_mode || microsoft_mode) &&
          class_type_supp(base_class_type)->assoc_scope != NULL &&
          is_template_param_or_nonreal_class_type(base_class_type)) {
        /* Microsoft and GNU compilers never check the completeness of a
           parameterized base class that has not been fully parsed yet.
           That causes the following example to be accepted:
             template<class T> struct S { struct N: S<T> {}; };
           In other modes, such cases result in an error. */
        warning(ec_unfinished_base_class);
      } else {
        error(ec_incomplete_type_not_allowed);
        okay = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  return okay;
}  /* check_base_class_type */


#if IA64_ABI
/*ARGSUSED*/  /* p_may_be_first_direct_nonvirtual_base is not used in some
                 configurations. */
#endif /* IA64_ABI */
static void add_new_direct_base(
                a_base_class_ptr       direct_bcp,
                a_class_def_state_ptr  class_state,
                an_access_specifier    access,
                a_base_class_ptr       *p_last_base,
                a_boolean              *p_may_be_first_direct_nonvirtual_base)
/*
Add direct_bcp as a base class to direct_bcp->derived_class, and recursively
add any base classes of direct_bcp->type.  class_state describes the class
definition in progress.  access is the (possibly implicit) access specified on
the base class.  *p_last_base points to the last base currently recorded for
the derived class and is updated by this function.
*p_may_be_first_direct_nonvirtual_base is TRUE if this may be the first direct
nonvirtual base of the direct base (in which case this function sets the flag
to FALSE before returning).
*/
{
  a_boolean                      is_virtual = direct_bcp->is_virtual;
  a_type_ptr                     bcp_type = direct_bcp->type;
  a_type_ptr                     class_type = direct_bcp->derived_class;
  a_class_type_supplement_ptr    ctsp = class_type_supp(class_type);
  a_class_symbol_supplement_ptr  bcp_cssp, cssp;
  a_derivation_step_ptr          path;
  a_base_class_ptr               bcp;
  a_boolean                      any_base_class_fixup_required = FALSE;
  a_boolean                      is_value_class = FALSE;

  cssp = symbol_supplement_for_class(class_type);
  bcp_cssp = symbol_supplement_for_class(bcp_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_class_type_kind_is(class_type, cctk_value)) {
    is_value_class = TRUE;
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    /* A class with base classes is neither an "aggregate" nor a POD.
       (C++/CLI value class types are the exception.) */
    class_state->class_aggregate_ruled_out = TRUE;
    class_state->POD_ruled_out = TRUE;
  }  /* if */
  /* The implied default constructor of the current class will be
     nontrivial if any of its base classes is virtual or has a nontrivial
     default constructor itself.  The current class requires a destructor
     if any of its base classes has a destructor.  Record such
     requirements, if any, at this time. */
  if ((is_virtual || !has_trivial_default_constructor(bcp_cssp)) &&
      !is_value_class) {
    class_state->default_ctor_is_nontrivial = TRUE;
  }  /* if */
  if (has_nontrivial_destructor(bcp_cssp)) {
    class_state->base_destruction_required = TRUE;
  }  /* if */
  /* Indicate whether an operator new or operate delete is inherited into
     the current derived class. */
  if (bcp_cssp->has_operator_new) cssp->has_operator_new = TRUE;
  if (bcp_cssp->has_operator_array_new) {
    cssp->has_operator_array_new = TRUE;
  }  /* if */
  if (bcp_cssp->has_operator_delete) cssp->has_operator_delete = TRUE;
  if (bcp_cssp->has_operator_array_delete) {
    cssp->has_operator_array_delete = TRUE;
  }  /* if */
  /* The current derived class cannot be copy-constructed or assigned by
     bitwise copying if the base class does not allow it or is a virtual
     base class.  (If the base class is nonreal, assume its type does not
     affect bitwise copyability.) */
  if (is_virtual) {
    cssp->construction_by_bitwise_copy_allowed = FALSE;
    cssp->assignment_by_bitwise_copy_allowed = FALSE;
  } else if (!bcp_type->variant.class_struct_union.is_nonreal_class &&
             !is_value_class) {
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
  /* Update the flag indicating whether there are any virtual base
     classes. */
  if (is_virtual ||
      bcp_type->variant.class_struct_union.any_virtual_base_classes) {
    class_type->variant.class_struct_union.any_virtual_base_classes = TRUE;
    cssp->standard_layout = FALSE;
  }  /* if */
  if (bcp_type->variant.class_struct_union
                           .any_virtual_functions_including_in_base_classes) {
    class_type->variant.class_struct_union
                      .any_virtual_functions_including_in_base_classes = TRUE;
  }  /* if */
  if (!bcp_cssp->standard_layout) {
    cssp->standard_layout = FALSE;
  }  /* if */
  if (bcp_type->variant.class_struct_union.any_volatile_member) {
    class_type->variant.class_struct_union.any_volatile_member = TRUE;
  }  /* if */
  if (bcp_type->variant.class_struct_union.any_mutable_member) {
    class_type->variant.class_struct_union.any_mutable_member = TRUE;
  }  /* if */
  if (bcp_type->variant.class_struct_union.has_operator_ampersand) {
    class_type->variant.class_struct_union.has_operator_ampersand = TRUE;
  }
  if (bcp_cssp->any_nonreal_base_classes ||
      (bcp_type->variant.class_struct_union.is_nonreal_class &&
       !(bcp_type->variant.class_struct_union.is_prototype_instantiation ||
         !bcp_type->variant.class_struct_union.is_template_class))) {
    /* Do not set the any_nonreal_base_classes field for a base that is a
       prototype instantiation, or a class defined as part of a prototype
       instantiation (e.g., a local class defined in the prototype
       instantiation of a function template). */
    cssp->any_nonreal_base_classes = TRUE;
  }  /* if */
  path = update_base_class_derivation(direct_bcp, (a_derivation_step_ptr)NULL,
                                      access);
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("base_specifiers")) {
    db_abbreviated_type(bcp_type);
    fputs(" is direct base class of ", f_debug);
    db_abbreviated_type(class_type);
    fputc('\n', f_debug);
  }  /* if */
#endif /* DEBUG */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
  /* When cfront lays out a class with base classes, the subobject for the
     first direct nonvirtual base class does not include the data sections for
     its own virtual base classes (if any).  However, the subobjects for the
     second and subsequent direct nonvirtual base classes and for virtual base
     classes do include the virtual base class data sections and are therefore
     marked as having a "complete subobject". */
  if (is_virtual || !*p_may_be_first_direct_nonvirtual_base) {
    direct_bcp->complete_subobject = TRUE;
  }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
  /* Add base classes derived from this base class to the current class' base
     class list.  They are marked as indirect. */
  for (bcp = base_classes_of(bcp_type); bcp != NULL; bcp = bcp->next) {
    if (bcp->overriding_virtual_functions != NULL) {
      any_base_class_fixup_required = TRUE;
    }  /* if */
    if (!bcp->direct) {
      continue;
    } else if (bcp->is_virtual) {
      /* A virtual base class is marked as "direct" if any of its paths is
         direct.  However, for our purposes, the "first" path (first in a
         depth-first left-to-right traversal of the derivation graph) must be
         direct.  On the other hand, we still need to record the direct path,
         but so set a flag so another pass will be done over the base class
         list. */
      if (!bcp->derivation->direct) {
        any_base_class_fixup_required = TRUE;
        continue;
      }  /* if */
    }  /* if */
      /* Add the direct base class and, recursively, the base classes thereof
         to the base class list for the derived class. */
    add_indirect_base_class(bcp, direct_bcp, path, p_last_base, class_type);
  }  /* for */
  /* Enter the base name on the base class list in the derived class's
     class-supplement entry. */
  if (*p_last_base == NULL) {
    ctsp->base_classes = direct_bcp;
  } else {
    (*p_last_base)->next = direct_bcp;
  }  /* if */
  *p_last_base = direct_bcp;
  /* Set shares_virtual_function_info for a base class of direct_bcp, if
     appropriate. */
  set_shares_virtual_function_info_flag(class_type, direct_bcp);
  if (any_base_class_fixup_required) {
    for (bcp = base_classes_of(bcp_type); bcp != NULL; bcp = bcp->next) {
      a_base_class_ptr  new_bcp;
      if (bcp->overriding_virtual_functions != NULL ||
          (bcp->direct && bcp->is_virtual && !bcp->derivation->direct)) {
        /* bcp is a base class of direct_bcp->type.  We need to find the
           corresponding base class of class_type.  Find a disambiguator in
           case what we are looking for is an ambiguous base class of
           class_type. */
        new_bcp = corresp_base_class(bcp, direct_bcp);
      } else {
        continue;
      }  /* if */
      if (bcp->direct && bcp->is_virtual && !bcp->derivation->direct) {
        /* Add path information about a direct virtual base class of direct_bcp
           that was not first in the depth-first left-to-right traversal of the
           latter's derivation graph.  Look for the matching derivation entry
           to get the right access. */
        a_base_class_derivation_ptr  bcdp = bcp->derivation->next;
        while (!bcdp->direct) bcdp = bcdp->next;
        (void)update_base_class_derivation(new_bcp, path, bcdp->access);
      }  /* if */
      if (bcp->overriding_virtual_functions != NULL) {
#if DEBUG
        if (debug_level >= 4) {
          fputs("copying virtual function override list from ", f_debug);
          db_base_class(bcp, FALSE);
          db_virtual_function_override_list(bcp);
        }  /* if */
#endif /* DEBUG */
        /* Copy the virtual function override entries from bcp (which is
           on the base classes list for bcp_type) to the
           corresponding copied base class new_bcp (which is on the base
           classes list for class_type). */
        copy_virtual_function_override_list(bcp, new_bcp, direct_bcp);
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
  if (ctsp->virtual_function_info_base_class == NULL &&
#if !IA64_ABI
      *p_may_be_first_direct_nonvirtual_base && 
#endif /* !IA64_ABI */
      !is_virtual) {
    /* For the first direct nonvirtual base class it is possible to
       share virtual function info (e.g., virtual function tables and
       their associated pointers) between the base class and the
       derived class. */
    bcp = class_type_supp(bcp_type)->virtual_function_info_base_class;
    /* Check to see whether or not the base has a virtual function table
       that could be shared. */
    if (needs_virtual_function_table(bcp_type) || bcp != NULL) {
      set_virtual_function_info_base_class(direct_bcp);
    }  /* if */
#if !IA64_ABI
    *p_may_be_first_direct_nonvirtual_base = FALSE;
#endif /* !IA64_ABI */
    /* If the derived class was already mentioned as the target of a
       conversion function, the base class should also have its
       target_of_conversion_function flag set.  Here's the kind of
       case where this is needed:
         class A;
         class B { ... };
         class X { operator A&(); };      // The flag is set for A
         class A : public B { ... };      // It must be set for B, too.
    */
    if (cssp->target_of_conversion_function) {
      set_target_of_conversion_function_flag(direct_bcp->type);
    }  /* if */
  }  /* if */
}  /* add_new_direct_base */


static void scan_base_specifier_list(a_type_ptr             type_ptr,
                                     a_class_def_state_ptr  class_state)
/*
Scan a list of base class specifiers, which may appear only on a class
or struct definition.  The syntax is


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
  a_base_class_ptr              bcp, end_of_base_classes_list = NULL;
  a_base_class_ptr              new_direct_bcp;
  an_access_specifier           access;
  a_boolean                     is_virtual;
  a_boolean                     explicit_access_specifier;
  char                          *default_access_str;
  a_symbol_ptr                  sym;
  a_type_ptr                    base_class_type;
  a_type_ptr                    orig_base_class_type;
  a_boolean                     ambiguous;
  a_class_symbol_supplement_ptr cssp;
  a_source_position             base_class_decl_pos;
  a_source_position             base_specifier_start_pos;
  a_boolean                     first_base_class = TRUE;
  a_base_class_sequence_number	direct_base_number = 0, proto_base_number = 0;
  a_boolean                     may_be_first_direct_nonvirtual_base = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_boolean                     interface_definition =
                                   microsoft_mode &&
                                   type_ptr->kind == (a_type_kind)tk_struct &&
                                   type_ptr->variant.class_struct_union
                                                                .is_interface;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_enter(3, "scan_base_specifier_list");
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("base_specifiers")) {
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
    a_pack_expansion_stack_entry_ptr	pesep;
    a_boolean				any_types;
    add_stop_token(tok_comma);
    /* A base-specifier is a potential variadic pack expansion context. */
    any_types = begin_potential_pack_expansion_context(&pesep);
    /* In the case of a template instantiation, variadic template parameters
       may cause the direct base number in the prototype instantiation to
       differ from that of a corresponding base in the real instantiation.
       We therefore keep track of the corresponding sequence number of the
       prototype instantiation. */
    if (class_state->is_template_instantiation) proto_base_number += 1;
    while (any_types) {
      a_pack_expansion_descr_ptr	pedep;
      an_attribute_ptr			attributes;
      attributes = scan_attributes(al_base_specifier);
      if (attributes != NULL) mark_primary_decl_attributes(attributes);
      /* Set the defaults. */
      if (type_ptr->kind == (a_type_kind)tk_class &&
          !is_immediate_managed_class_type(type_ptr)) {
        access = (an_access_specifier)as_private;
        default_access_str = "private";
      } else {
        access = (an_access_specifier)as_public;
        default_access_str = "public";
      }  /* if */
      base_specifier_start_pos = pos_curr_token;
      direct_base_number++;
      new_direct_bcp = NULL;
      /* Scan a single base specification, first looping through the specifying
         keywords virtual, public, private, and protected. */
      scan_inheritance_kind(type_ptr, &is_virtual, &access,
                            &explicit_access_specifier);
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      {
      a_source_sequence_entry_ptr  saved_insert_point = NULL;

      if (depth_innermost_function_scope == NO_SCOPE_DEPTH &&
          cssp->class_template == NULL) {
        /* Clear the instantiation insert point to assure that any
           instantiations triggered by the base specifier will appear right
           after the entry for the current class.  The order will be fixed up
           later. */
        saved_insert_point = scope_stack[depth_scope_stack].
                                           ss_list_instantiation_insert_point;
        reset_ss_list_instantiation_insert_point();
      }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* Test for identifier or "::" next. */
      if (!is_decl_qualified_name_start()) {
        syntax_error(ec_exp_identifier);
      } else {
        /* Scan the base class name. */
        a_boolean err = FALSE;
        base_class_decl_pos = pos_curr_token;
        base_class_type = NULL;
        if (!first_base_class || is_virtual) {
          /* Multiple inheritance and virtual inheritance are outside the
             "Embedded C++" subset. */
          feature_is_not_part_of_embedded_cplusplus_subset(
                                &base_specifier_start_pos,
                                ec_multiple_inheritance_in_embedded_cplusplus);
        }  /* if */
        /* Look up the identifier for the base class.  Only identifiers
           that could be classes (including typedefs to classes and template
           parameters) are considered in the lookup. */
        sym = coalesce_and_lookup_generalized_identifier(
                                   GID_IMPLICIT_TYPE_CONTEXT, ilm_class, &err);
        if (sym != NULL) {
          record_potential_pack_reference(
                                    sym, &locator_for_curr_id.source_position);
        }  /* if */
        /* Be sure a type symbol was found and that it identifies a class. */
        if (sym == NULL || !is_class_symbol(sym)) {
          /* Not a class symbol.  In most cases, issue an error and skip it.
             When a template param is involved, just skip it. */
          if (sym != NULL && sym->kind == (a_symbol_kind)sk_type) {
            a_type_ptr  tp = skip_typedefs(type_symbol_type(sym));
            if (tp->kind == (a_type_kind)tk_template_param) {
              if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
                /* No diagnostic on template parameters, which will only show
                   up during prototype instantiations.  Set the flag that
                   indicates that this prototype instantiation has a nonreal
                   base class.  ctsp will be NULL if an error was issued for
                   an attempt to put a base class on a union.  Don't set
                   any_nonreal_base_classes as the base class will not be
                   on the base class list. */
                cssp->any_nonreal_base_classes = ctsp != NULL;
                base_class_type = proxy_class_for_template_param(tp);
                orig_base_class_type = base_class_type;
              } else {
                /* Error case.  Ignore the specifier. */
                error(ec_bad_base_class);
                goto skip_base_class;
              }  /* if */
            }  /* if */
          } /* if */
          if (base_class_type == NULL) {
            error(ec_not_a_class_or_struct_name);
            reference_to_invalid_name(&locator_for_curr_id);
            goto skip_base_class;
          }  /* if */
        } else if (locator_for_curr_id.is_semivisible_nested_type) {
          /* The symbol in the locator is a nested class that is not visible
             according to the ARM lookup rules but is returned in support of
             the nested class anachronism (ARM 18.3.5). Issue an anachronism
             diagnostic. */
          sym_diagnostic(anachronism_error_severity,
                         ec_nested_class_anachronism,
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
        if (base_class_type == NULL) {
          /* Get the type entry for the base class name. */
          base_class_type = type_symbol_type(sym);
          base_class_type->source_corresp.referenced = TRUE;
          if (!check_base_class_type(type_ptr, base_class_type)) {
            /* The type of the base class is invalid (e.g., incomplete). */
            goto skip_base_class;
          }  /* if */
          orig_base_class_type = base_class_type;
          base_class_type = skip_typerefs(base_class_type);
#if BACK_END_IS_CP_GEN_BE && \
    CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
          if (scope_stack[depth_scope_stack].kind ==
                                   (a_scope_kind)sck_template_instantiation &&
              orig_base_class_type->source_corresp.is_class_member &&
              type_is_typedef(orig_base_class_type)) {
            /* The C++-generating back end will generate an explicit
               specialization for this class definition.  This can cause
               problems for examples like the following:
  
                 template <typename T> struct X: T { };
                 struct A { };
                 struct Y {
                   typedef A x;
                   X<x> xx;
                 };
  
               The explicit specialization of X<A> will appear before the
               definition of Y, so we must take care not to use the typedef
               Y::x as the base specifier for X<A>.  We do that by scanning
               the scope stack to see if the parent class of the typedef is
               on the stack; if it is, we use the underlying type instead of
               the typedef as the "original" type. */
            a_scope_depth depth;

            for (depth = depth_scope_stack - 1; depth != DEPTH_OF_FILE_SCOPE;
                 --depth) {
              a_scope_stack_entry_ptr ssep = scope_stack_entry_for(depth);
              if ((ssep->kind == (a_scope_kind)sck_class_struct_union ||
                   ssep->kind == (a_scope_kind)sck_class_reactivation) &&
                  same_entities(ssep->assoc_type,
                                parent_class_of(orig_base_class_type))) {
                /* Use the underlying type. */
                orig_base_class_type = base_class_type;
                break;
              }  /* if */
            }  /* for */
          }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE && ... */
          if (base_class_type
                       ->variant.class_struct_union.has_zero_init_component) {
            /* At least a part of this base class must be zero initialized when
               value-initializing object of the type being parsed. */
            type_ptr->
                     variant.class_struct_union.has_zero_init_component = TRUE;
          }  /* if */
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (interface_definition && !is_interface_like(base_class_type)) {
          error(ec_interface_must_derive_from_interface);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Issue a diagnostic if an explicit access specifier was not provided
           (as per the recommendation on p. 243 of the ARM). */
        if (!explicit_access_specifier) {
          pos_st_remark(ec_missing_access_specifier, &error_position,
                        default_access_str);
        }  /* if */
        check_assertion(ctsp != NULL);
        /* Before creating the base class entry and adding it to the list of
           base classes, go through the list looking for conflicts. */
        ambiguous = FALSE;
        for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
          if (same_entities(bcp->type, base_class_type)) {
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
              bcp->orig_type = orig_base_class_type;
              bcp->direct = TRUE;
              bcp->direct_base_number = direct_base_number;
              bcp->decl_position = base_class_decl_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
              bcp->base_specifier_range.start = base_specifier_start_pos;
              bcp->base_specifier_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
              if (attributes != NULL) {
                attach_attributes(attributes, (char*)bcp, iek_base_class);
              }  /* if */
              mark_base_dependent_if_needed(bcp, class_state,
                                            proto_base_number);
              goto skip_base_class;
            } else {
              /* At least one is non-virtual, so there is an ambiguity.  Mark
                 both as ambiguous.  */
              bcp->ambiguous = ambiguous = TRUE;
            }  /* if */
          }  /* if */
        }  /* for */
        /* Now create the new base class entry and add it to the end of the
           base classes list. */
        new_direct_bcp = alloc_base_class();
        new_direct_bcp->type = base_class_type;
        new_direct_bcp->orig_type = orig_base_class_type;
        new_direct_bcp->derived_class = type_ptr;
        new_direct_bcp->decl_position = base_class_decl_pos;
        new_direct_bcp->direct = TRUE;
        new_direct_bcp->ambiguous = ambiguous;
        new_direct_bcp->direct_base_number = direct_base_number;
        if (is_virtual) new_direct_bcp->is_virtual = TRUE;
        mark_base_dependent_if_needed(new_direct_bcp, class_state,
                                      proto_base_number);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        new_direct_bcp->base_specifier_range.start = base_specifier_start_pos;
        new_direct_bcp->base_specifier_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        if (attributes != NULL) {
          attach_attributes(attributes, (char*)new_direct_bcp, iek_base_class);
        }  /* if */
        add_new_direct_base(new_direct_bcp, class_state, access,
                            &end_of_base_classes_list,
                            &may_be_first_direct_nonvirtual_base);
skip_base_class:
        first_base_class = FALSE;
        /* Advance past the base class name to the comma or right brace. */
        (void)get_token();
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      /* Restore the instantiation insert point. */
      if (saved_insert_point != NULL) {
        scope_stack[depth_scope_stack].ss_list_instantiation_insert_point =
                                                          saved_insert_point;
      }  /* if */
      }
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      pedep = end_potential_pack_expansion_context(pesep,
                                                   /*is_declarator=*/FALSE);
      if (pedep != NULL && new_direct_bcp != NULL) {
        new_direct_bcp->is_pack_expansion = TRUE;
      }  /* if */
      any_types = advance_to_next_pack_element(pesep);
    }  /* while */
    /* Advance past the next comma, if any, and scan the next base class
       specifier. */
    remove_stop_token(tok_comma);
  } while (loop_token(tok_comma));
  db_exit();
}  /* scan_base_specifier_list */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void add_implicit_cli_bases(a_class_def_state_ptr  class_state)
/*
The given class is being defined in C++/CLI mode and its explicit base classes
have been scanned.  If the class type is a ref class type or a value class
type, add an implicit derivation from System::ObjectType or System::ValueType
(respectively) if appropriate.
*/
{
  a_type_ptr  class_type = class_state->class_type;

  if (cli_class_type_kind_is(class_type, cctk_ref) ||
      cli_class_type_kind_is(class_type, cctk_value)) {
    /* If a ref class or value class does not specify a ref class base, it
       derives implicitly from System::ObjectType or System::ValueType
       (respectively). */
    a_boolean         add_implicit_base = TRUE;
    a_base_class_ptr  bcp;
    for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
      if (is_ref_class_type(bcp->type)) {
        add_implicit_base = FALSE;
        break;
      }  /* if */
    }  /* for */
    if (add_implicit_base &&
        !f_identical_types(class_type, cli_system_object_type(),
                           ITF_NO_FLAGS)) {
      a_base_class_ptr              new_direct_bcp, last_bcp = NULL;
      a_boolean                     may_be_first_direct_nonvirtual_base = TRUE;
      a_base_class_sequence_number  direct_base_number = 0;
      /* Determine the last base class entry and the last direct base
         number. */
      bcp = base_classes_of(class_type);
      if (bcp != NULL) {
        for (; bcp != NULL; bcp = bcp->next) {
          if (bcp->next == NULL) last_bcp = bcp;
          if (bcp->direct_base_number > direct_base_number) {
            direct_base_number = bcp->direct_base_number;
          }  /* if */
        }  /* for */
        /* There are no virtual base classes for managed classes.  So if there
           is already a base class, there must also be a direct nonvirtual
           base class. */
        may_be_first_direct_nonvirtual_base = FALSE;
      }  /* if */
      new_direct_bcp = alloc_base_class();
      new_direct_bcp->type = cli_class_type_kind_is(class_type, cctk_ref) ?
                                                      cli_system_object_type()
                                                    : cli_system_value_type();
      complete_type_is_needed(new_direct_bcp->type);
      new_direct_bcp->orig_type = new_direct_bcp->type;
      new_direct_bcp->derived_class = class_type;
      new_direct_bcp->direct = TRUE;
      new_direct_bcp->is_implicit_direct_base = TRUE;
      new_direct_bcp->direct_base_number = direct_base_number+1;
      add_new_direct_base(new_direct_bcp, class_state,
                          (an_access_specifier)as_public, &last_bcp,
                          &may_be_first_direct_nonvirtual_base);
    }  /* if */
  }  /* if */
}  /* add_implicit_cli_bases */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void wrapup_base_classes(a_class_def_state_ptr  class_state)
/*
The base specifier list has been scanned and any implicit base classes have
been added.  Perform any needed post-processing for the resulting base class
list (such as computing the preorder list for the IA-64 ABI).
*/
{
  a_type_ptr                   type_ptr = class_state->class_type;
#if IA64_ABI || (DEBUG && CHECKING)
  a_class_type_supplement_ptr  ctsp = class_type_supp(type_ptr);
#endif /* IA64_ABI || (DEBUG && CHECKING) */
  a_base_class_ptr             bcp;
#if IA64_ABI
  a_base_class_ptr             first_indirect_primary_vbase = NULL;
#endif /* IA64_ABI */

#if IA64_ABI
  /* Compute the list of base classes in preorder, now that the postorder list
     is complete. */
  (void)compute_preorder_base_classes(type_ptr, (a_base_class_ptr)NULL, 
                                      &preorder_base_classes_of(type_ptr));
  /* See if there are any virtual base classes with which we could share 
     a virtual function table. */
  if (ctsp != NULL && ctsp->virtual_function_info_base_class == NULL) {
    for (bcp = preorder_base_classes_of(type_ptr); bcp != NULL; 
         bcp = bcp->next_preorder) {
      if (bcp->is_virtual && is_nearly_empty_class(bcp->type)) {
        if (!bcp->shares_virtual_function_info) {
          set_virtual_function_info_base_class(bcp);
          break;
        } else if (first_indirect_primary_vbase == NULL) {
          first_indirect_primary_vbase = bcp;
        }  /* if */
      }  /* if */
    }  /* for */
    /* If no satisfactory base has yet been found, use the first indirect
       primary virtual base. */
    if (ctsp->virtual_function_info_base_class == NULL &&
        first_indirect_primary_vbase != NULL) {
      set_virtual_function_info_base_class(first_indirect_primary_vbase);
    }  /* if */
  }  /* if */
#endif /* !IA64_ABI */
  if (type_ptr->variant.class_struct_union.any_virtual_base_classes) {
    /* Make a pass over the base class list to resolve duplicate virtual base
       classes (if any). */
    for (bcp = base_classes_of(type_ptr); bcp != NULL; bcp = bcp->next) {
      if (bcp->is_virtual) {      
        set_preferred_base_class_derivation(type_ptr, bcp);
      }  /* if */
    }  /* for */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  if (!source_sequence_entries_disallowed) {
    /* If a base specifier reference provokes an instantiation, the entries
       representing it will end up in the wrong place -- after the entry for
       the derived class instead of before it.  Move them to precede the
       entry for the class. */
    a_source_sequence_entry_ptr  ssep = type_ptr->source_corresp.
                                                   source_sequence_entry;
    if (ssep != NULL && ssep->next != NULL) {
#if DEBUG
      a_source_sequence_entry_ptr   prev = ssep->prev;
#endif /* DEBUG */

      /* Unlink the source sequence entry for the current class and
         replace it in the list with the new entry, if there is one. */
      move_src_seq_entry(ssep, (a_source_sequence_entry_ptr)NULL);
#if DEBUG
      if (debug_level >= 4 ||
          db_flag_is_set("dump_ss_full") ||
          db_flag_is_set("base_specifiers")) {
        fputs("moved base class ss entries for \"", f_debug);
        db_type_name(type_ptr);
        fputs("\":\n", f_debug);
        db_ss_list(prev != NULL ?
                      prev->next :
                      scope_stack[depth_scope_stack].source_sequence_list);
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("base_specifiers")) {
    db_base_class_list(type_ptr);
  }  /* if */
#if CHECKING
  if (db_active) {
    /* This check is somewhat expensive, so only do it when debugging is
       turned on. */
    if (type_ptr->kind != (a_type_kind)tk_union) {
      for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
        verify_path_consistency(type_ptr, bcp);
        verify_virt_func_override_list(type_ptr, bcp, /*null_allowed=*/FALSE);
      }  /* for */
    }  /* if */
  }  /* if */
#endif /* CHECKING */
#endif /* DEBUG */
}  /* wrapup_base_classes */


static a_boolean is_member_decl_start(void)
/*
Return TRUE if the current token looks like the start a member declaration
of a C++ class, struct, or union or a C struct or union.
*/
{
  a_boolean     is_start = FALSE;

  if (is_type_start(/*is_expr_context=*/FALSE)) {
    /* This is the only check required for C struct/union fields. */
    is_start = TRUE;
  } else if (C_dialect == C_dialect_cplusplus) {
    /* C++ class/struct/union members can also start with one of the following
       tokens. */
    is_start = (curr_token == tok_static || curr_token == tok_typedef ||
                curr_token == tok_private || curr_token == tok_protected ||
                curr_token == tok_public || curr_token == tok_compl
#if MICROSOFT_EXTENSIONS_ALLOWED
                || (cppcli_enabled && curr_token == tok_not)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                            );
  }  /* if */
  return is_start;
}  /* is_member_decl_start */


void decl_friend_class(a_type_ptr          class_type,
                       a_type_ptr          friend_class_type)
/*
Do processing for declaring an entire class (friend_class_type) friend of
the current class (class_type).
*/
{
  a_class_list_entry_ptr      clep;
  a_class_type_supplement_ptr ctsp;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_type_ptr  declared_type = friend_class_type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  if (is_error_type(friend_class_type)) {
    /* Ignore it. */
  } else if (!prototype_instantiations_in_il &&
             class_type->variant.class_struct_union.is_nonreal_class) {
    /* friend declarations are not processed during prototype instantiation
       -- they're meaningless until a real instantiation is done. */
  } else {
    /* In non-strict mode (e.g., for cfront compatibility) it is sometimes
       permitted to use a typedef name in the elaborated type specifier of
       a friend class declaration as long as it refers to a class.  In
       addition, declarations of the form "friend T", where T is a template
       parameter, are permitted in class templates. */
    friend_class_type = skip_typerefs(friend_class_type);
    if (friend_class_type->kind == (a_type_kind)tk_template_param) {
      friend_class_type = proxy_class_for_template_param(friend_class_type);
    }  /* if */
    check_assertion(is_immediate_class_type(friend_class_type));
    if (class_type == friend_class_type &&
        (scope_stack[depth_scope_stack].in_prototype_instantiation ||
         !is_template_class_type(friend_class_type))) {
      /* Diagnostic on excessive narcissism.  The diagnostic is not justified
         on certain template cases, however.  For example:
           template<class T> class C { friend class C<long>; ... };
           template class C<long>;
         The friend declaration might be needed if a member template of C
         is specialized. */
      warning(ec_self_friendship);
    } else {
      ctsp = friend_class_type->variant.class_struct_union.extra_info;
      /* Issue a remark if this is a duplicate friend declaration. */
      for (clep = ctsp->befriending_classes; clep != NULL; clep = clep->next) {
        if (clep->class_type == class_type) {
          remark(ec_duplicate_friend_decl);
          break;
        }  /* if */
      }  /* for */
      /* No duplication was detected. */
      clep = alloc_list_entry_for_class_full(
                                           &friend_class_type->source_corresp);
      clep->class_type = class_type;
      clep->next = ctsp->befriending_classes;
      ctsp->befriending_classes = clep;
      /* Now add the friend_class_type to the friends list for the current
         class. */
      ctsp = class_type->variant.class_struct_union.extra_info;
      clep = alloc_list_entry_for_class_full(&class_type->source_corresp);
      clep->class_type = friend_class_type;
      clep->next = ctsp->friend_classes;
      ctsp->friend_classes = clep;
#if DEBUG
      if (db_trace("friendship", class_type, iek_type) ||
          db_trace("friendship", friend_class_type, iek_type)) {
        db_abbreviated_type(friend_class_type);
        fprintf(f_debug, " designated a friend of ");
        db_abbreviated_type(class_type);
        fprintf(f_debug, "\n");
        if (db_flag_is_set("friendship")) {
          fprintf(f_debug, "befriending_classes list of ");
          db_abbreviated_type(friend_class_type);
          fprintf(f_debug, ":\n");
          db_class_list(friend_class_type->variant.class_struct_union.
                                              extra_info->befriending_classes);
          fprintf(f_debug, "friend_classes list of ");
          db_abbreviated_type(class_type);
          fprintf(f_debug, ":\n");
          db_class_list(class_type->variant.class_struct_union.
                                                   extra_info->friend_classes);
        }  /* if */
      }  /* if */
#endif /* DEBUG */
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    {
    a_source_sequence_entry_ptr   ssep;

    ssep = last_matching_source_sequence_entry((char *)declared_type);
    if (ssep != NULL && ss_entry_kind(ssep) == iek_src_seq_secondary_decl) {
      ((a_src_seq_secondary_decl_ptr)ssep->entity.ptr)->friend_decl = TRUE;
    }  /* if */
    }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
}  /* decl_friend_class */


static a_symbol_ptr member_function_redecl_sym_with_template_flag(
                                       a_symbol_ptr          sym,
                                       a_type_ptr            new_type,
                                       a_template_param_ptr  templ_param_list,
                                       a_boolean             templates_only,
                                       a_symbol_ptr          *other_match)
/*
sym is a member function symbol or overloaded function symbol from a
previous declaration.  new_type is the type from the current
declaration.  If the new declaration is a function template,
templ_param_list points to the template parameter list.  Check the
type for compatibility with sym or, if sym represents an overloaded
function, with any of the instances.  If a match is found, return a
pointer to the symbol.  If not, return NULL.  If other_match is non-NULL,
set *other_match to an additional matching function if there is one
(this can happen only with Microsoft-mode selective overriders) or to
NULL otherwise.

If the routine type from the current declaration or one from the original
declaration indicates that the function as a whole was qualified (e.g.,
int f(int) const), then the type compatibility check must take the
const qualification into account when seeking a match.  In other words,
if one of the functions was so qualified, both must be for them to have
compatible types.  The qualification is indicated by a separate field
"qualifiers" in the routine type supplement.

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
have a non-NULL this_class and the type match must be done without it.

When templates_only is TRUE, only function templates members are considered.
*/
{
  a_boolean                      is_overloaded_function, match;
  a_type_ptr                     orig_type, orig_this_class, new_this_class;
  a_routine_type_supplement_ptr  orig_rts, new_rts;
  a_boolean                      orig_function_is_qualified;
  a_boolean                      new_function_is_qualified;

  if (other_match != NULL) *other_match = NULL;
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
  new_rts = (skip_typerefs(new_type))->variant.routine.extra_info;
  new_this_class = new_rts->this_class;
  new_function_is_qualified = (new_rts->qualifiers != TQ_NONE);
  /* Go through the symbol list and look for an instance in which the
     types are compatible with the current type. */
  for (; sym != NULL; sym = is_overloaded_function ? sym->next : NULL) {
    a_template_param_ptr              other_templ_param_list;
    a_template_symbol_supplement_ptr  tssp;
    a_routine_ptr                     routine;
    a_symbol_ptr                      fund_sym = sym;
    if (sym->kind == (a_symbol_kind)sk_projection) {
      /* Ignore projection symbols, except those resulting from a using-
         declaration that project a member function or member function
         template. */
      if (is_class_member_using_decl_symbol(sym)) {
        /* A using declaration: This could be a match, in which case the caller
           may have to remove this symbol. */
        fund_sym = fundamental_symbol_of(sym);
      }  /* if */
      if (fund_sym->kind != (a_symbol_kind)sk_function_template &&
          fund_sym->kind != (a_symbol_kind)sk_member_function) {
        continue;
      }  /* if */
    }  /* if */
    check_assertion(fund_sym->kind == (a_symbol_kind)sk_function_template ||
                    fund_sym->kind == (a_symbol_kind)sk_member_function);
    /* If looking only for templates, ignore nontemplates.  When looking
       for nontemplates, ignore templates. */
    if ((fund_sym->kind == (a_symbol_kind)sk_function_template) !=
                                                             templates_only) {
      continue;
    }  /* if */
    /* Get the routine pointer associated with either the routine symbol
       or the function template symbol. */
    if (fund_sym->kind == (a_symbol_kind)sk_function_template) {
      routine = fund_sym->variant.template_info->variant.function.routine;
      orig_type = routine->type;
    } else {
      routine = fund_sym->variant.routine.ptr;
      orig_type = routine->type;
    }  /* if */
    orig_rts = (skip_typerefs(orig_type))->variant.routine.extra_info;
    orig_this_class = orig_rts->this_class;
    orig_function_is_qualified = (orig_rts->qualifiers != TQ_NONE);
    if (new_function_is_qualified != orig_function_is_qualified) {
      /* No match is possible.  Don't bother calling types_are_compatible. */
      continue;
    }  /* if */
    if (fund_sym->kind == (a_symbol_kind)sk_function_template &&
        templ_param_list == NULL) {
      /* The symbol we are checking is a template, but no template parameter
         list was supplied by the caller.  This is not a match. */
      continue;
    }  /* if */
    if (templ_param_list != NULL &&
        fund_sym->kind == (a_symbol_kind)sk_function_template) {
      /* If a template parameter list is present and the candidate symbol
         is for a function template, make sure the lists match.  A
         template parameter list could be present for a normal member function
         of a class template when the member function is being defined
         outside of the class. */
      tssp = template_supplement_for_symbol(fund_sym);
      other_templ_param_list =
                       tssp->variant.function.decl_cache.decl_info->parameters;
      if (!equiv_template_param_lists(other_templ_param_list,
                                      templ_param_list,
                                      /*issue_errors=*/FALSE,
                                      ETP_NO_OPTIONS,
                                      (a_source_position*)NULL, es_error)) {
        /* The template parameter lists do not match. */
        continue;
      }  /* if */
    }  /* if */
    if (new_function_is_qualified) {
      /* Both routines are qualified.  Use the "this" param type as part
         of the compatibility check. */
    } else {
      /* Neither routine is qualified.  Save away the "this" param types,
         do the compatibility check without them, and then restore them. */
      new_rts->this_class = NULL;
      orig_rts->this_class = NULL;
    }  /* if */
    match = routine_types_are_redecl_compatible(orig_type, new_type,
                                                TCF_NO_FLAGS);
    if (!new_function_is_qualified) {
      /* Restore the implicit "this" parameter types in orig_type and
         new_type. */
      new_rts->this_class = new_this_class;
      orig_rts->this_class = orig_this_class;
    }  /* if */
    if (match) {
      /* If a match was found by types_are_compatible, break out of the loop.
         An exception is made if the match we found is a selective overrider
         and other_match is non-NULL. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (other_match != NULL && routine->overridden_functions != NULL &&
          *other_match == NULL) {
        /* A selective overrider.  Record the current match but look for
           another one. */
        *other_match = sym;
      } else    
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Do not insert code here. */
      {
        break;
      }  /* if */
    }  /* if */
  }  /* for */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (sym == NULL && other_match != NULL && *other_match != NULL) {
    /* A selective overrider was found and recorded in *other_match, but no
       additional match was found.  Return the single match and clear
       *other_match. */
    sym = *other_match;
    *other_match = NULL;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return sym;
}  /* member_function_redecl_sym_with_template_flag */


a_symbol_ptr member_function_redecl_sym(
                                a_symbol_ptr          sym,
                                a_type_ptr            new_type,
                                a_template_param_ptr  templ_param_list,
                                a_symbol_ptr          *other_match)
/*
member_function_redecl_sym_with_template_flag does real processing for
this routine.  See the header comment there.

When looking for a matching declaration the nesting depths are not considered.
This allows us to produce the more helpful "nesting depths do not match"
error instead of just a "no matching declaration" error.  It does mean,
however, for the example below that a template declaration could match a
nontemplate member of a template class.

  template<int N> struct A {
    template <int M> int f();
    int f();
  };
  
  template<int N> template<int M> int A<N>::f(){ return M; }
  template<int N> int A<N>::f(){ return 1; }

To prevent this, member_function_redecl_sym_with_template_flag is called
twice; once to search for templates and again to search for nontemplates.
*/
{
  a_symbol_ptr  result;

  /* First look for a matching template. */
  result = member_function_redecl_sym_with_template_flag(
                     sym, new_type, templ_param_list, /*templates_only=*/TRUE,
                     other_match);
  if (result == NULL) {
    /* No template was found, look for a normal member function. */
    result = member_function_redecl_sym_with_template_flag(
                    sym, new_type, templ_param_list, /*templates_only=*/FALSE,
                    other_match);
  }  /* if */
  return result;
}  /* member_function_redecl_sym */


void update_friend_function_info(a_routine_ptr rout_ptr,
                                 a_type_ptr    class_type)
/*
Update the list of befriending classes associated with rout_ptr to reflect
that it is now a friend of class_type.  Also update class_type to indicate
that the routine indicated by rout_ptr is a friend.
*/
{
  a_class_list_entry_ptr clep;
  a_class_type_supplement_ptr ctsp;
  a_routine_list_entry_ptr    rlep;

  /* Issue a remark if this is a duplicate friend declaration. */
  for (clep = rout_ptr->befriending_classes; clep != NULL; clep = clep->next) {
    if (clep->class_type == class_type) {
      remark(ec_duplicate_friend_decl);
      break;
    } /* if */
  } /* for */
  /* Add a friend declaration to the befriending_classes list. */
  clep = alloc_list_entry_for_class_full(&rout_ptr->source_corresp);
  clep->class_type = class_type;
  clep->next = rout_ptr->befriending_classes;
  rout_ptr->befriending_classes = clep;
  /* Now add the routine to the friends list for the current class. */
  ctsp = class_type->variant.class_struct_union.extra_info;
  rlep = alloc_list_entry_for_routine();
  rlep->routine = rout_ptr;
  rlep->next = ctsp->friend_routines;
  ctsp->friend_routines = rlep;
#if DEBUG
  if (db_trace("friendship", rout_ptr, iek_routine) ||
      db_trace("friendship", class_type, iek_type)) {
    db_name_full(&rout_ptr->source_corresp, iek_routine);
    fprintf(f_debug, " designated a friend of ");
    db_abbreviated_type(class_type);
    fprintf(f_debug, "\n");
    if (db_flag_is_set("friendship")) {
      fprintf(f_debug, "befriending_classes list of ");
      db_name_full(&rout_ptr->source_corresp, iek_routine);
      fprintf(f_debug, ":\n");
      db_class_list(rout_ptr->befriending_classes);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
}  /* update_friend_function_info */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* pos_info is not used in some configurations. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
static a_symbol_ptr decl_dependent_friend_function(
                                         a_symbol_locator       *locator,
                                         a_type_ptr             function_type,
                                         a_func_info_block_ptr  func_info,
                                         a_decl_pos_block_ptr   pos_info)
/*
Create a routine and associated symbol for a template dependent friend
declaration of type function_type.  The locator for the friend declarator and
some extra declaration info are passed through locator, func_info, and
pos_info.
The routine symbol is returned (but not linked into the symbol table).
The routine entry itself is linked into the IL only if prototype
instantiations are recorded in the IL.
*/
{
  a_symbol_ptr                  sym = NULL;
  a_symbol_kind                 sym_kind;
  a_routine_ptr                 rp;
  a_memory_region_number        region_to_switch_back_to;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_src_seq_secondary_decl_ptr  sssdp;
  a_source_sequence_entry_ptr   ssep = func_info->declarator_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  switch_to_file_scope_region(&region_to_switch_back_to);
  /* First create a symbol for this function (though it will not be linked
     into the symbol table. */
  sym_kind = (a_symbol_kind)
               (locator->is_class_member ? sk_member_function : sk_routine);
  sym = alloc_symbol(sym_kind, locator->symbol_header,
                     &locator->source_position);
  /* Make a routine entry for this member: */
  rp = make_routine(function_type, (a_storage_class)sc_extern,
                    prototype_instantiations_in_il ?
                            depth_innermost_namespace_scope : NO_SCOPE_DEPTH);
  /* Treat this as a prototype instantiation so that it doesn't end up in
     the IL if prototype_instantiations_in_il is FALSE.  (Note that even
     though is_prototype_instantiation is TRUE, is_template_function is FALSE
     unless an explicit template argument list is specified). */
  rp->is_prototype_instantiation = TRUE;
  sym->variant.routine.ptr = rp;
  set_source_corresp(&rp->source_corresp, sym);
  if (locator->is_class_member) {
    a_type_ptr  parent_type = qualifier_class_type(*locator);
    if (is_template_param_type(parent_type)) {
      parent_type = skip_typerefs(parent_type);
      parent_type = proxy_class_for_template_param(parent_type);
    }  /* if */
    set_class_membership(sym, &rp->source_corresp, parent_type);
  } else {
    a_namespace_ptr  parent_nsp = qualifier_namespace_ptr(*locator);
    if (parent_nsp != NULL) {
      set_namespace_membership(sym, &rp->source_corresp, parent_nsp);
    }  /* if */
  }  /* if */
  if (locator->is_template_id) {
    /* A (possibly-empty) template argument list was used. */
    if (locator->template_arg_list != NULL) {
      /* The argument list was non-empty. */
      process_unattached_template_argument_list(locator->template_arg_list);
      rp->template_arg_list = locator->template_arg_list;
    }  /* if */
    rp->expl_template_arg_list_used = TRUE;
    rp->is_template_function = TRUE;
  }  /* if */
  if (func_info->is_definition) {
    /* set_inline_flag assumes the rp->defined flag reflects previous
       declarations.  So rp->defined shouldn't be updated until after
       set_inline_flag has been called. */
    set_inline_flag(rp, TRUE);
    rp->defined = sym->defined = TRUE;
    rp->defined_in_friend_decl = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    rp->declared_type = func_info->declared_type;
    if (ssep != NULL && prototype_instantiations_in_il) {
      ssep->entity.kind = (a_byte_il_entry_kind)iek_routine;
      ssep->entity.ptr  = (char *)rp;
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    rp->source_corresp.decl_pos_info =
                        make_decl_pos_supplement(in_file_scope(rp), pos_info);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  } else {
    /* A dependent friend declaration that is not a definition.  If this
       is neither a qualified name nor a template-id, issue a warning as
       it is probably not what was intended. */
    if (warning_on_non_template_friend && !guiding_decls_allowed &&
        !locator->is_qualified_name && !locator->is_template_id &&
        is_or_contains_template_param(function_type)) {
      pos_sy_warning(ec_probable_guiding_friend, &locator->source_position,
                     sym);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (ssep != NULL && prototype_instantiations_in_il) {
      /* Point to it from a secondary source sequence_entry: */
      sssdp = make_source_sequence_secondary_decl((char*)rp, iek_routine,
                                                  func_info->declared_type);
      sssdp->decl_position = sym->decl_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      sssdp->decl_pos_info = make_decl_pos_supplement(in_file_scope(sssdp),
                                                      pos_info);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      sssdp->friend_decl = TRUE;
      ssep->entity.kind = (a_byte_il_entry_kind)iek_src_seq_secondary_decl;
      ssep->entity.ptr  = (char *)sssdp;
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
  switch_back_to_original_region(region_to_switch_back_to);
  return sym;
}  /* decl_dependent_friend_function */


static a_symbol_ptr decl_friend_function(a_symbol_locator        *locator,
                                         a_class_def_state_ptr   class_state,
                                         a_func_info_block_ptr   func_info,
                                         a_member_decl_info_ptr  decl_info)
/*
Do processing for declaring a function (identified by *locator and described
by *func_info and *decl_info) friend of the current class (described through
class_state).  Getting the correct symbol of a previously declared function
means taking overloading into account.  For nonmember functions, this could
be the initial declaration of the function, and again overloading is a
possibility.
*/
{
  a_decl_parse_state           *state = &decl_info->decl_state;
  a_type_ptr                   class_type = class_state->class_type;
  a_type_ptr                   function_type = state->type;
  a_symbol_ptr                 sym, ext_sym;
  an_id_linkage_kind           linkage;
  a_type_ptr                   old_type;
  a_symbol_reference_kind      srk_flags;
  a_source_sequence_entry_ptr  declarator_ssep = NULL;

  db_enter(3, "decl_friend_function");
  if (is_template_dependent_context() &&
      !scope_stack[depth_scope_stack].in_prototype_instantiation) {
    /* Template dependent friend declarations should only be encountered in
       prototype instantiation scopes, but severe syntax errors can get us
       here nonetheless.  In that case we just skip the friend processing. */
    set_to_named_error_locator(*locator);
  }  /* if */
  if (!is_error_locator(*locator)) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      if (cppcli_enabled && is_immediate_managed_class_type(class_type)) {
        /* Friend declarations cannot appear in C++/CLI managed classes. */
        pos_error(ec_managed_class_cannot_have_friend, &state->start_pos);
      } else if (class_type->variant.class_struct_union.is_interface) {
        /* Friend declarations cannot appear in interface types. */
        pos_error(ec_interface_cannot_have_friend, &state->start_pos);
      }  /* if */
      /* Microsoft Visual C++ 7.0 and earlier do not seem to instantiate the
         body of a friend function definition as part of a class template
         instantiation.  (Visual C++ 7.1 fixed that.) */
      if (microsoft_version < 1310 && class_state->is_template_instantiation) {
        func_info->is_definition = FALSE;
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    sym = locator->specific_symbol;
    if (sym == NULL && locator->is_template_id) {
      /* If this is a template-id for which the symbol has not yet been
         found, look it up now. */
      an_id_lookup_options_set  idl_options = IDL_FRIEND_LOOKUP;
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (cppcli_enabled &&
          state->declared_storage_class == (a_storage_class)sc_static) {
        idl_options |= IDL_IS_STATIC_DECL;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      sym = normal_id_lookup(locator, idl_options);
    }  /* if */
    if (is_template_dependent_context()) {
      if (func_info->is_definition && locator->is_qualified_name) {
        /* A member function cannot be defined in a friend declaration. */
        pos_sy_error(ec_bad_scope_for_definition,
                     &locator->source_position, sym);
        sym = NULL;
        set_to_error_locator(*locator);
      } else {
        /* If the friend declaration appears in a template dependent context,
           create a dummy routine and associated symbol.  Return that instead
           of calling decl_routine. */
        sym = decl_dependent_friend_function(locator, function_type, func_info,
                                             &decl_info->decl_pos_block);
        state->sym = sym;
        state->first_decl = TRUE;
        goto decl_processed;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!is_error_locator(*locator)) {
    if (!no_access_check_on_friend_declarator_ids ||
        (sym != NULL && sym->ambiguous)) {
      /* Many compilers (Microsoft, GNU, ...) do not check access for the
         declarator-id of a friend declaration. */
      check_ambiguity_and_verify_access(locator);
    }  /* if */
    srk_flags = SRK_DECLARATION | SRK_FRIEND;
    if (func_info->is_definition) srk_flags |= SRK_DEFINITION;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    declarator_ssep = func_info->declarator_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (microsoft_mode && sym != NULL && sym->is_class_member &&
        sym->kind == (a_symbol_kind)sk_projection) {
      /* Microsoft compilers allow naming inherited members; e.g.:
           struct B { int f(); };
           struct D: B {};
           class X { friend int D::f(); };  // Okay in Microsoft mode.  */
      reduce_projection_symbol_to_fundamental_symbol(sym);
      if (is_member_function_symbol(sym) &&
          function_type->kind == (a_type_kind)tk_routine &&
          routine_type_is_nonstatic_member_function(function_type)) {
        /* A qualified function type will have this_class set.  Adjust
           this_class to the class inherited from. */
        function_type->variant.routine.extra_info->this_class =
                                                       sym->parent.class_type;
      }  /* if */
    }  /* if */
    if (sym != NULL && sym->is_class_member &&
        !is_member_function_symbol(sym)) {
      /* If sym represents a member of a class, but it is not a member
         function.  Issue an error. */
      if (sym->kind == (a_symbol_kind)sk_projection) {
        /* A member of a base class. */
        pos_error(ec_inherited_member_not_allowed, &locator->source_position);
      } else {
        pos_sy_error(ec_not_compatible_with_previous_decl,
                     &locator->source_position, sym);
      }  /* if */
      sym = NULL;
      set_to_error_locator(*locator);
    } else if (sym == NULL ||
              !(is_member_function_symbol(sym) ||
                is_proxy_member_symbol(sym))) {
      /* Not a member function.  Get the symbol -- the rest of what's returned
         from decl_routine is not relevant for processing in this context. */
      /* If the friend function is defined in this declaration or if it was
         specified as inline, that information should be passed on to
         decl_routine. */
      if (strcmp(locator->symbol_header->identifier, "main") == 0 &&
          (locator->is_qualified_name ?
            locator->is_file_scope_qualified_name :
            depth_innermost_namespace_scope == DEPTH_OF_FILE_SCOPE)) {
        /* Friendship is being given to the global main() function. */
        a_boolean  is_inline = func_info->is_inline;

        func_info->is_main_function = TRUE;
        check_main_function(func_info, function_type, &state->storage_class,
                            &is_inline, &locator->source_position);
        func_info->is_inline = is_inline;
      } else if (func_info->is_definition) {
        if (class_type->source_corresp.is_local_to_function) {
          /* It is an error to define a function in a friend declaration
             of a local class.  To avoid confusion down the road, clear
             is_inline. */
          func_info->is_inline = FALSE;
        } else if (qualifier_namespace_ptr(*locator) != NULL ||
                   locator->is_file_scope_qualified_name) {
          /* A function definition in a friend declaration involving a
             namespace-qualified name in the declarator is not allowed. */
          pos_error(ec_no_qualified_friend_definition,
                       &locator->source_position);
          sym = NULL;
          clear_qualifier_from_locator(locator);
          set_to_named_error_locator(*locator);
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
        } else if (class_type_can_be_named_in_namespace_scope(class_type)
#if MICROSOFT_EXTENSIONS_ALLOWED
                   && !(microsoft_mode &&
                        microsoft_routine_def_is_unmovable(
                                              /*explicit_overrider=*/FALSE))
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                                            ) {
          /* The primary source sequence entry will be deferred until the
             class definition has been completed; a secondary-decl entry
             will be put out here.  (That is not possible with unnamed
             classes.) */
          func_info->is_movable_member_or_friend_def = TRUE;
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        }  /* if */
      } else if (sym != NULL &&
                 sym->kind == (a_symbol_kind)sk_namespace_projection) {
        /* Look for ambiguity resulting from pulling in declarations from
           other namespaces. */
        if (sym->ambiguous) {
          /* Issue an error. */
          check_for_ambiguity(locator);
          sym = NULL;
          clear_qualifier_from_locator(locator);
          set_to_named_error_locator(*locator);
        }  /* if */          
      }  /* if */
      if (microsoft_mode &&
          state->storage_class != (a_storage_class)sc_unspecified) {
        /* In Microsoft mode "extern" and "static" are permitted on a
           nonmember friend declaration. */
        if (state->storage_class != (a_storage_class)sc_static &&
            state->storage_class != (a_storage_class)sc_extern) {
          /* The storage class of a function has to be extern or static. */
          pos_warning(ec_bad_function_storage_class, &state->start_pos);
          state->storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
      }  /* if */
      decl_routine(locator, state, func_info, srk_flags, &linkage,
                   &old_type, &ext_sym, &decl_info->decl_pos_block);
      sym = state->sym;
      /* WP 11.4 para 5 prohibits defining a nonmember function in a local
         class friend declaration. */
      if (func_info->is_definition &&
          !locator->is_error &&
          class_type->source_corresp.is_local_to_function) {
        pos_sy_error(ec_bad_scope_for_definition, &pos_curr_token, sym);
      }  /* if */
      /* If this symbol might not be found because it is invisible, add it
         to the friend list for the class. */
      { a_boolean	add_to_friend_list = FALSE;
        if (arg_dependent_lookup_enabled) {
          if (sym->is_invisible) add_to_friend_list = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (sym->is_microsoft_invisible_operator) add_to_friend_list = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
        if (add_to_friend_list) {
          add_friend_function_to_lookup_list_for_class(sym, class_type);
        }  /* if */
      }
    } else {
      /* The friend function is a class member. */
      if (sym_parent_class(sym) == class_type) {
        /* It's a member function of the very class that is according it
           friendship.  Issue a diagnostic. */
        diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
                   ec_self_friendship);
      }  /* if */
      /* It's a member function.  Find the right type signature for this
         member function name.  This could potentially be an instance of
         a member function template.  If none can be found, NULL is
         returned. */
      sym = find_matching_template_instance(
                               sym, function_type, locator->template_arg_list,
                               (a_boolean)locator->is_template_id,
                               /*in_class_specialization=*/FALSE,
                               es_error);
      if (sym == NULL) {
        /* This is a member function, but one with a type that doesn't
           match a previously declared member.  A diagnostic will have been
           issued by find_matching_template_instance. */
        set_to_error_locator(*locator);
      } else {
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode &&
            state->declared_storage_class != (a_storage_class)sc_unspecified) {
          /* Member function -- a storage class is not allowed, except for
             C++/CLI static constructors. */
          if (!is_static_constructor_symbol(sym)) {
            pos_warning(ec_storage_class_not_allowed, &state->start_pos);
          }  /* if */
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* "inline" may not be introduced by this declaration. */
        if (func_info->is_inline && !func_info->is_definition &&
            !sym->variant.routine.ptr->is_inline) {
          if (microsoft_mode) {
            /* Microsoft compilers ignore the inline specifier in this case. */
            func_info->is_inline = FALSE;
          } else {
            error(ec_inline_not_allowed);
          }  /* if */
        }  /* if */
      }  /* if */
      if (sym != NULL) {
        if (sym->defined && func_info->is_definition) {
          /* Trying to define a function that's already defined. */
          pos_sy_error(ec_function_redefinition,
                       &locator->source_position, sym);
          set_to_error_locator(*locator);
        } else {
          if (func_info->is_definition) {
            /* WP 11.4 para 5 prohibits defining a member function in a
               friend declaration. */
            pos_sy_error(ec_bad_scope_for_definition,
                         &locator->source_position, sym);
          }  /* if */
          record_symbol_declaration(srk_flags, sym, &locator->source_position,
                                    declarator_ssep);
          /* Do exception specification compatibility checking. */
          check_exception_specification(function_type, sym,
                                        &func_info->throw_position,
                                        /*is_redecl=*/TRUE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
          if (!func_info->is_definition) {
            a_routine_ptr         rp = sym->variant.routine.ptr;
            a_name_reference_ptr  name_ref = NULL;
            if (record_name_references_in_context()) {
              name_ref = qualifiable_name_reference(locator,
                                                    &rp->source_corresp);
            }  /* if */
            /* Since this is a non-defining entry, it is represented by a
               secondary-decl entry in the source sequence list.  Enter the
               current function type. */
            (void)update_src_seq_secondary_decl(
                               (char *)rp, func_info->declared_type, name_ref,
                               SSSD_FRIEND_DECL, &decl_info->decl_pos_block);
          }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        }  /* if */
      }  /* if */
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    wrapup_sse_for_simple_decl(state);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
  if (is_error_locator(*locator)) {
    /* Create a dummy symbol to return when there's been an error.  This is
       required for further processing, in case there's a definition of the
       the routine body. */
    a_routine_ptr  rp = make_routine(function_type, (a_storage_class)sc_static,
                                     NO_SCOPE_DEPTH);
    sym = enter_symbol((a_symbol_kind)sk_routine, locator, DEPTH_OF_FILE_SCOPE,
                       /*suppress_redecl_error=*/FALSE);
    sym->variant.routine.ptr = rp;
    state->sym = sym;
    state->first_decl = TRUE;
    state->prev_type = NULL;
    /* If this is a friend declaration in a prototype instantiation,
       mark it as a prototype instantiation too. */
    if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
      rp->is_prototype_instantiation = TRUE;
      rp->is_template_function = TRUE;
    }  /* if */
    /* Set the source correspondence. */
    set_source_corresp(&sym->variant.routine.ptr->source_corresp, sym);
  } else {
    if (!class_type->variant.class_struct_union.is_nonreal_class ||
        prototype_instantiations_in_il) {
      update_friend_function_info(sym->variant.routine.ptr, class_type);
    }  /* if */
    if (strict_ansi_mode && func_info->any_default_args) {
      if (sym->is_class_member) {
        pos_diagnostic(strict_ansi_error_severity,
                       ec_default_arg_on_member_friend, &state->start_pos);
      } else if (!func_info->is_definition) {
        pos_diagnostic(strict_ansi_error_severity,
                       ec_default_arg_requires_friend_to_be_definition,
                       &state->start_pos);
      }  /* if */
    }  /* if */
  }  /* if */
decl_processed:
  if (func_info->is_definition) {
    /* Since this is a definition, record the current lint argsused and
       varargs-count state in the routine type. That will suppress any
       warnings about unused parameters or variable arguments. */
    record_lint_argsused_and_varargs_state(sym);
    if (state->sym != NULL) {
      /* Check uses of "= default" (always an error) and "= delete". */
      check_defaulted_or_deleted_function(state, func_info, &pos_curr_token);
    }  /* if */
  }  /* if */
  /* Do processing required for any pragmas that are bound to the current
     declaration. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  db_exit();
  return sym;
}  /* decl_friend_function */


static a_symbol_ptr find_direct_member_function(a_symbol_locator  *locator,
                                                a_type_ptr        class_type)
/*
Find a member function (or member function template or overload set thereof)
that has the same symbol header as *locator and that is also either a direct
member of class_type or else a name brought in by a using-declaration.
Return NULL if none is found.
*/
{
  a_symbol_ptr                   sym = NULL, fund_sym;

  clear_specific_symbol(*locator);
  (void)class_qualified_id_lookup(locator, class_type,
                                  IDL_DIRECT_CLASS_MEMBERS_ONLY);
  sym = locator->specific_symbol;
  if (sym != NULL) {
    /* Ignore the symbol (i.e., return NULL) if it is not a member function
       or a projection symbol for a using-declaration that refers to a member
       function. */
    if (is_function_or_template_symbol(sym)) {
      /* Okay. */
    } else if (is_class_member_using_decl_symbol(sym)) {
      fund_sym = fundamental_symbol_of(sym);
      if (is_function_or_template_symbol(fund_sym)) {
        /* Okay. */
      } else if (is_nontype_template_param_symbol(fund_sym)) {
        /* Assume a nontype template parameter represents a function. */
      } else {
        sym = NULL;
      }  /* if */
    } else {
      sym = NULL;
    }  /* if */
  }  /* if */
  return sym;
}  /* find_direct_member_function */


static void remove_member_using_decl(a_symbol_ptr  *pu_sym,
                                     a_symbol_ptr  *ps_sym)
/*
*pu_sym is a symbol for a member using declaration.  If that entry is part of
an overload set, *ps_sym represents the associated sk_overloaded_function
symbol; otherwise, *ps_sym equals *pu_sym.  Remove *pu_sym from the symbol
table and if that empties the overload set, also remove the latter.  Set the
symbol pointers pointing to removed symbols to NULL.
*/
{
  check_assertion(is_class_member_using_decl_symbol(*pu_sym));
  if (*ps_sym == *pu_sym) {
    remove_symbol(*pu_sym);
    *ps_sym = NULL;
  } else {
    remove_symbol_from_overload_set(*pu_sym, *ps_sym);
    if ((*ps_sym)->variant.overloaded_function.symbols == NULL) {
      /* The last entry of the overload set was removed: Remove the set
         itself. */
      remove_symbol(*ps_sym);
      *ps_sym = NULL;
    }  /* if */
  }  /* if */
  *pu_sym = NULL;
}  /* remove_member_using_decl */


#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/ /* overridden_function only used when Microsoft extensions are
                enabled. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static a_symbol_ptr symbol_for_member_function(
                                   a_symbol_locator       *locator,
                                   a_type_ptr             class_type,
                                   a_symbol_ptr           overridden_function,
                                   a_member_decl_info_ptr decl_info,
                                   a_symbol_ptr           *overload_sym)
/*
Return a pointer to an sk_member_function symbol to represent a function
described by *locator and *decl_info.  If this is a redeclaration, the
existing symbol is returned.  If this declaration overloads a function name,
the symbol returned will be on the sk_overloaded_function symbol's list.  If
there is an error in attempting to overload the function name, a new symbol
is returned nonetheless, but it is not added to the list of overloaded
function symbols.  In Microsoft mode, it is possible to declare several
members of the same type, provided they explicitly override different
virtual functions: The function explicitly overridden by this declaration
is indicated by overridden_function (NULL if no explicit overriding syntax
was used).
*/
{
  a_symbol_ptr   sym, new_sym = NULL;
  an_error_code  error_code;
  a_boolean      suppress_redecl_error = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_boolean      multiple_selective_overriders = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_enter(4, "symbol_for_member_function");
  *overload_sym = NULL;
  if (is_error_locator(*locator)) {
    sym = NULL;
  } else {
    /* See if there's already a member function with this name. */
    sym = find_direct_member_function(locator, class_type);
    if (sym != NULL) {
      /* A member function by this name has already been entered into the
         symbol table.  This could be a redeclaration, which is illegal for
         class members.  Check for that first by looking for a type match. */
      new_sym = member_function_redecl_sym(sym, decl_info->decl_state.type,
                                           (a_template_param_ptr)NULL,
                                           (a_symbol_ptr*)NULL);
      if (new_sym == NULL) {
        /* The previously declared function with the same name (or, if it is
           already overloaded, any instance of it) does not have a matching
           type, so sym remains a candidate for overloading. */
      } else if (is_class_member_using_decl_symbol(new_sym)) {
        /* A using-declaration previously declared a matching function or
           template in this scope.  The new declaration hides the one brought
           in by the using-declaration: Remove new_sym. */
        remove_member_using_decl(&new_sym, &sym);
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (microsoft_mode &&
                 new_sym->kind == (a_symbol_kind)sk_member_function &&
                 !is_immediate_managed_class_type(class_type) &&
                 !is_selectively_overridden_by(overridden_function, new_sym)) {
        /* Although a declaration with a matching type was found, it overrides
           a different base member.  Treat the new declaration as a distinct
           member.  (Note: This applies only to the non-C++/CLI syntax for
           denoting selective overriding.) */
        new_sym = NULL;
        multiple_selective_overriders = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        /* This is a redeclaration.  Just return to old symbol entry, setting
           sym to NULL to avoid overload processing.  The caller will
           do some consistency checking and issue a warning. */
        sym = NULL;
      }  /* if */
    }  /* if */
    if (sym != NULL) {
      /* Not a redeclaration, so it is probably the overloading of a function
         name. */
      a_symbol_ptr  fund_sym = fundamental_symbol_of(sym);
      if (is_nontype_template_param_symbol(fund_sym)) {
        /* A non-type template parameter symbol may be a function, so don't
           issue a redeclaration error. */
        suppress_redecl_error = TRUE;
      } else {
        /* The routine overload_distinguishable returns TRUE if the
           routine types are candidates for overloading; if it returns FALSE
           it also returns the error code for a diagnostic explaining why.
           Note that overload_distinguishable only handles the function types
           without considering the Microsoft-specific case of multiple
           selective overriders with the same parameter types. */
        /* The templ_param_list is NULL in the following call because
           although member functions of class templates have template types
           in their parameters, they are not called using the template
           overload resolution mechanism. */
        if (
#if MICROSOFT_EXTENSIONS_ALLOWED
            !multiple_selective_overriders &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            !overload_distinguishable(sym, decl_info->decl_state.type,
                                      (a_template_param_ptr)NULL,
                                      &error_code)) {
          pos_error(error_code, &locator->source_position);
          suppress_redecl_error = TRUE;
          set_to_named_error_locator(*locator);
        } else {
          a_boolean  is_ctor = decl_info->is_constructor;

          /* Enter this symbol as an instance of overloading. */
          new_sym = enter_overloaded_symbol((a_symbol_kind)sk_member_function,
                                            locator, is_ctor, sym,
                                            overload_sym);
        }  /* if */
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


static void add_to_conversion_list(a_symbol_ptr                   orig_sym,
                                   a_class_symbol_supplement_ptr  cssp)
/*
Create a conversion list entry for orig_sym, which represents a conversion
operator, and add it to the conversion list in the class symbol supplement
pointed to by cssp.
*/
{
  a_symbol_list_entry_ptr  slep;
  a_symbol_ptr             sym;

  db_enter(4, "add_to_conversion_list");

  /* Check for the rather unusual case in which orig_sym represents an
     overloaded conversion operator derived from a base class. */
  sym = fundamental_symbol_of(orig_sym);
  if (orig_sym->kind == (a_symbol_kind)sk_projection &&
      sym->kind == (a_symbol_kind)sk_overloaded_function) {
    /* This is the projection of an overloaded conversion -- a case like this:
         class B {
           operator int();
           operator int() const;
         };
         class A : public B { };
       We want to add a conversion list entry for each member of the overload
       set rather than for the overload set as a whole.  This means that a
       projection symbol will be created for each. */
    a_type_ptr           class_type;
    a_base_class_ptr     base_class;
    an_access_specifier  base_class_access;
    a_symbol_ptr         new_sym;
    a_boolean            ambiguous;
    a_boolean            using_decl;

    class_type = sym_parent_class(orig_sym);
    check_assertion(symbol_supplement_for_class(class_type) == cssp);
    base_class = orig_sym->variant.projection.extra_info->
                                                 fundamental_base_class;
    base_class_access = preferred_derivation_of(base_class)->access;
    ambiguous = orig_sym->ambiguous;
    using_decl = (orig_sym->variant.projection.is_using_decl ||
                  orig_sym->variant.projection.any_intervening_using_decl);
    /* Go through the members of the overload set. */
    sym = sym->variant.overloaded_function.symbols;
    for (; sym != NULL; sym = sym->next) {
      /* Create a projection symbol.  Note that it is not entered into the
         the symbol table or put on the class scope entry's symbol list.
         Note also that the setting of the ambiguous flag in the overload
         symbol is copied into each new symbol. */
      new_sym = make_projection_symbol(sym, class_type, base_class,
                                       (a_derivation_step_ptr)NULL, ambiguous);
      new_sym->variant.projection.access =
                                      compute_access(access_for_symbol(sym),
                                                     base_class_access);
      new_sym->variant.projection.any_intervening_using_decl = using_decl;
      /* Add the new projection symbol to the conversion list. */
      add_to_conversion_list(new_sym, cssp);
    }  /* for */
  } else {
    /* This is the normal case.  Allocate the new entry and make it point to
       the symbol. */
    slep = alloc_symbol_list_entry();
    slep->symbol = orig_sym;
    /* Add it to the list associated with the parent class. */
    if (sym->kind == (a_symbol_kind)sk_function_template) {
      /* Add it to the list for templates. */
      slep->next = cssp->conversion_template_list;
      cssp->conversion_template_list = slep;
    } else {
      /* Add it to the list for ordinary conversion functions. */
      slep->next = cssp->conversion_list;
      cssp->conversion_list = slep;
    }  /* if */
  }  /* if */
  db_exit();
}  /* add_to_conversion_list */


void set_mixed_static_nonstatic_flag(a_symbol_ptr  overload_sym)
/*
overload_sym is an sk_overloaded_function symbol representing a set of
member functions.  If its mixed_static_nonstatic flag has not been set yet,
compare the first two entries in the list and set the flag if appropriate
(i.e., if one is a static member function and the other is a nonstatic
member function).  (Since this is called whenever a symbol is added to the
overload set, and since new entries are added to the front of the list, only
the first two need be checked.)
*/
{
  a_symbol_ptr                      sym1, sym2;
  a_type_ptr                        tp1, tp2;

  check_assertion_str2(overload_sym->kind ==
                               (a_symbol_kind)sk_overloaded_function,
                      "set_mixed_static_nonstatic_flag:",
                      "sk_overloaded_function expected");
  sym1 = overload_sym->variant.overloaded_function.symbols;
  sym2 = sym1->next;
  /* Set a flag in overload_sym if the instances of an overloaded function
     are a mixture of static and nonstatic member functions. */
  if (!overload_sym->variant.overloaded_function.mixed_static_nonstatic) {
    if (sym2 != NULL) {
      sym1 = fundamental_symbol_of(sym1);
      if (sym1->kind != (a_symbol_kind)sk_function_template) {
        tp1 = routine_symbol_type(sym1);
      } else {
        tp1 = sym1->variant.template_info->variant.function.routine->type;
      }  /* if */
      sym2 = fundamental_symbol_of(sym2);
      if (sym2->kind == (a_symbol_kind)sk_function_template) {
        tp2 = sym2->variant.template_info->variant.function.routine->type;
      } else if (sym2->kind == (a_symbol_kind)sk_routine ||
                 sym2->kind == (a_symbol_kind)sk_member_function) {
        tp2 = routine_symbol_type(sym2);
      } else {
        /* In a prototype instantiation, the overload set can contain
           symbols for nonreal base class members brought in by
           using-declarations. */
        check_assertion(is_prototype_instantiation_context());
        tp2 = NULL;
      }  /* if */
      if (tp2 != NULL &&
          routine_type_is_nonstatic_member_function(tp1) !=
                              routine_type_is_nonstatic_member_function(tp2)) {
        overload_sym->
           variant.overloaded_function.mixed_static_nonstatic = TRUE;
      }  /* if */
    }  /* if */
  } else if (sym2 == NULL) {
    overload_sym->variant.overloaded_function.mixed_static_nonstatic = FALSE;
  }  /* if */
}  /* set_mixed_static_nonstatic_flag */


static a_boolean types_of_decl_and_using_decl_conflict(a_symbol_ptr  decl_sym,
                                                       a_symbol_ptr  using_sym,
                                                       a_boolean     *err)
/*
decl_sym and using_sym represent routines in the same overload set, the
former by direct declaration, the latter by a using declaration; compare
their types and return TRUE if they conflict -- i.e., if they are too
compatible to coexist in the same overload set.  There is special handling
when using_sym refers to a virtual member function.  Return *err TRUE when
a diagnostic should be issued by the caller.
*/
{
  a_boolean   compat = FALSE;
  a_boolean   is_class_member = decl_sym->is_class_member;
  a_type_ptr  tp1 = routine_symbol_type(decl_sym);
  a_type_ptr  tp2 = routine_symbol_type(using_sym);

  *err = FALSE;
  /* First compare param types and, if appropriate, implicit-this-param
     types. */
  if (param_types_are_compatible(tp1, tp2, TCF_NO_FLAGS) &&
      (!is_class_member ||
       this_param_types_correspond(tp1, tp2, /*check_as_conversion=*/FALSE,
                                   /*check_as_operands=*/FALSE))) {
    /* They are compatible so far. */
    if (is_class_member) {
      /* No diagnostic for class members. */
      compat = TRUE;
    } else {
      /* Namespace-scope declarations cannot conflict in this way. */
      if (microsoft_mode && types_are_strictly_compatible(
                                         tp1->variant.routine.return_type,
                                         tp2->variant.routine.return_type)) {
        /* Microsoft compilers do not flag this case as an error
           (though an ambiguity error is issued at a point of use). */
      } else {
        *err = compat = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return compat;
}  /* types_of_decl_and_using_decl_conflict */


static void mark_class_member_using_decl_as_hidden(a_type_ptr    class_type,
                                                   a_symbol_ptr  sym)
/*
Set the "hidden" flag to TRUE in the using-decl entry belonging to the
scope of the derived class indicated by class_type and associated with the
base-class member indicated by sym.
*/
{
  a_using_decl_ptr  udp;
  a_routine_ptr     rp;

  check_assertion(sym->kind == (a_symbol_kind)sk_member_function);
  rp = sym->variant.routine.ptr;
  udp = class_type->variant.class_struct_union.extra_info->
                                                 assoc_scope->using_decls;
  for (; udp != NULL; udp = udp->next) {
    if (udp->entity.ptr == (char *)rp) {
      udp->hidden = TRUE;
      break;
    }  /* if */
  }  /* for */
}  /* mark_class_member_using_decl_as_hidden */


void check_for_conflicts_with_using_decls(a_symbol_ptr       overload_sym,
                                          a_source_position  *pos)
/*
A function (either a member or nonmember function) has been declared and
added to an overload list pointed to by overload_sym.  Check for conflicts
between the newly declared function (which will head the overload list) and
any other members of the overload set that have been introduced as a result
of a using declaration.  For each case in which there's a conflict (there
may be more than one), remove the using symbol from the overload list (so
it won't cause ambiguity errors later), and if appropriate, issue an error,
using *pos as the error position.
*/
{
  a_symbol_ptr   decl_sym, sym, prev_in_overload_set, using_sym;
  a_boolean      err;
  a_symbol_kind  using_sym_kind;

  db_enter(4, "check_for_conflicts_with_using_decls");
  /* The first symbol on the overload list is the current declaration. */
  decl_sym = overload_sym->variant.overloaded_function.symbols;
  if (decl_sym->kind == (a_symbol_kind)sk_function_template) {
    /* Ignore function templates. */
  } else {
    if (decl_sym->is_class_member) {
      /* It's a member function; we're looking for sk_projection symbols. */
      using_sym_kind = (a_symbol_kind)sk_projection;
    } else {
      /* It's a nonmember function; we're looking for sk_namespace_projection
         symbols. */
      using_sym_kind = (a_symbol_kind)sk_namespace_projection;
    }  /* if */
    /* Keep track of the previous symbol in the overload list, to enable
       removing a symbol from the list. */
    prev_in_overload_set = decl_sym;
    for (sym = decl_sym->next; sym != NULL; sym = prev_in_overload_set->next) {
      if (sym->kind == using_sym_kind) {
        /* Found a projection symbol.  Get the fundamental symbol so the types
           can be compared. */
        using_sym = fundamental_symbol_of(sym);
        /* Check for a conflict between the type of the newly declared function
           symbol (decl_sym) and the type of the symbol previously introduced
           by a using declaration (using_sym). */
        if (using_sym->kind == (a_symbol_kind)sk_function_template) {
          /* Ignore function template symbols in the overload set. */
        } else if (is_nontype_template_param_symbol(using_sym)) {
          /* Ignore symbols created to represent potential entities in
             dependent base classes. */
        } else if (types_of_decl_and_using_decl_conflict(decl_sym,
                                                         using_sym, &err)) {
          /* An error is issued, unless using_sym is a member function being
             hidden and/or overridden by decl_sym (err == FALSE), or both
             symbols refer to the same entity (because they are extern "C"
             declarations). */
          a_boolean  merge_gpp_c_routines = gpp_mode && gnu_version >= 30400;
          if (err && !symbols_are_lookup_equivalent(decl_sym, using_sym,
                                                    merge_gpp_c_routines)) {
            pos_sy2_error(ec_conflicts_with_using_decl, pos, decl_sym,
                          using_sym);
          }  /* if */
          /* Remove the symbol from the overload list by skipping around it. */
          prev_in_overload_set->next = sym->next;
          if (decl_sym->is_class_member) {
            /* Remove the class-member-using-decl entry associated with sym. */
            mark_class_member_using_decl_as_hidden(sym_parent_class(decl_sym),
                                                   using_sym);
          }  /* if */
          /* Continue through the overload list -- there may be more than
             one projection symbol with which decl_sym conflicts. */
          continue;
        }  /* if */
      }  /* if */
      prev_in_overload_set = sym;
    }  /* for */
  }  /* if */
  db_exit();
}  /* check_for_conflicts_with_using_decls */


static a_symbol_ptr special_function_symbol(
                                        a_type_ptr               class_type,
                                        a_special_function_kind  sfkind,
                                        a_param_type_ptr         first_param,
                                        a_source_position        *source_pos,
                                        a_boolean                *ambiguous)
/*
Find a member function (a constructor, destructor, or assignment operator,
as indicated by sfkind) whose parent class is class_type.  first_param, which
will be non-NULL for copy constructors and assignment operators, represents
the first parameter of the member function in a derived class to which the
the sought-for function corresponds.  source_pos is a source position,
used as the point of instantiation if a template ends up being
instantiated.  If the lookup is successful, return a pointer to the
symbol; otherwise, return NULL.  If there is more than one matching
function, set *ambiguous to TRUE.
*/
{
  a_symbol_ptr          sym;
  a_boolean             class_bitwise_copy, pass_by_value;
  a_type_qualifier_set  qualifiers = TQ_NONE;

  *ambiguous = FALSE;
  if (is_incomplete_type(class_type) ||
      !is_immediate_class_type(class_type)) {
    /* An error of some short. */
    sym = NULL;
  } else {
    if (first_param != NULL) {
      /* A copy constructor or an assignment operator.  If the parameter is
         of reference type, the qualifier underneath the reference is
         significant. */
      a_type_ptr  tp = first_param->type;
      if (is_any_reference_type(tp)) {
        /* Reference argument. */
        tp = type_pointed_to(tp);
        qualifiers = get_type_qualifiers(tp);
      }  /* if */
    }  /* if */
    switch (sfkind) {
      case sfk_constructor:
        if (first_param == NULL) {
          /* Default constructor. */
          sym = find_default_constructor(class_type, ambiguous,
                                         (a_boolean *)NULL);
        } else {
          /* Copy constructor. */
          sym = find_copy_constructor(class_type, qualifiers,
                                      /*source_is_rvalue=*/FALSE,
                                      source_pos, ambiguous,
                                      &class_bitwise_copy);
        }  /* if */
        break;
      case sfk_destructor:
        /* Destructor. */
        sym = (symbol_supplement_for_class(class_type))->destructor;
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case sfk_static_constructor:
        check_assertion(cppcli_enabled);
        sym = (symbol_supplement_for_class(class_type))->static_constructor;
        break;
      case sfk_finalizer:
        /* C++/CLI finalizer. */
        check_assertion(cppcli_enabled);
        sym = (symbol_supplement_for_class(class_type))->finalizer;
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case sfk_operator:
        /* Assignment operator. */
        check_assertion(first_param != NULL);
        sym = find_copy_assignment_operator(class_type, qualifiers,
                                            ambiguous, &pass_by_value);
        break;
      default:
        unexpected_condition_str2("special_function_symbol:",
                                  "bad special function kind");
    }  /* switch */
  }  /* if */
  return sym;
}  /* special_function_symbol */


static a_boolean merge_exception_specifications(a_symbol_ptr  sym,
                                                a_type_ptr    new_rout_type)
/*
Look up the exception specification associated with the member function
indicated by sym and record it in func_info, merging it with the exception
specification already there, if any.  If sym can throw any exception, return
TRUE.
*/
{
  a_boolean                            throw_any;
  an_exception_specification_ptr       old_esp, new_esp;
  an_exception_specification_type_ptr  old_estp, estp;
  a_routine_type_supplement_ptr        rtsp;

  check_assertion(sym->kind == (a_symbol_kind)sk_member_function);
  /* Fetch the exception specification associated with the member function
     indicated by sym. */
  old_esp = sym->variant.routine.ptr->type->
                  variant.routine.extra_info->exception_specification;
  if (old_esp == NULL) {
    /* The function can throw any exception. */
    throw_any = TRUE;
  } else {
    throw_any = FALSE;
    rtsp = new_rout_type->variant.routine.extra_info;
    new_esp = rtsp->exception_specification;
    if (new_esp == NULL) {
      /* No exception specification has been recorded in func_info yet, so
         allocate the entry. */
      new_esp = alloc_exception_specification();
#if EXTRA_SOURCE_POSITIONS_IN_IL
      new_esp->source_range.start = sym->decl_position;
      new_esp->source_range.end = sym->decl_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      rtsp->exception_specification = new_esp;
    }  /* if */
    /* Now traverse the types specified for the exception specification of the
       function indicated by sym.  Make a copy of any that does not already
       appear on the func_info list. */
    old_estp = old_esp->exception_specification_type_list;
    for (; old_estp != NULL; old_estp = old_estp->next) {
      if (old_estp->redundant) {
        /* Skip it. */
      } else {
        /* See if it's already on the list. */
        estp = new_esp->exception_specification_type_list;
        for (; estp != NULL; estp = estp->next) {
          if (identical_types(estp->type, old_estp->type)) {
            /* It's already on the list. */
            break;
          }  /* if */
        }  /* for */
        if (estp != NULL) {
          /* Skip it. */
        } else {
          /* It hasn't been added to the list yet.  Allocate a new
             exception-specification type entry and add it to the list.  The
             order is unimportant, so it can be placed on the front. */
          estp = alloc_exception_specification_type();
          estp->type = old_estp->type;
          estp->next = new_esp->exception_specification_type_list;
          new_esp->exception_specification_type_list = estp;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  return throw_any;
}  /* merge_exception_specifications */


static void form_exception_specification_for_generated_function(
                                                         a_routine_ptr  rp)
/*
Synthesize an exception specification for the implicitly declared (i.e.,
compiler-generated) member function rp -- a constructor, destructor, or
assignment operator.  The synthesized exception specification is the union of
all exception specifications for the routines that will be called when the
definition of the compiler-generated member function is finally put out.  For
instance, if a destructor is implicitly generated, it is assumed to throw all
exceptions that any destructor it calls (for a base class or nonstatic data
member) is able to throw.  This routine is only called in C++ mode and only
when exception support is enabled.
*/
{
  a_special_function_kind  sfkind;
  a_type_ptr               rout_type, class_type;
  a_base_class_ptr         bcp;
  a_field_ptr              fp;
  a_type_ptr               tp;
  a_symbol_ptr             sym;
  a_boolean                throw_any = FALSE;
  a_boolean                ambiguous;
  a_param_type_ptr         first_param;

  check_assertion(C_dialect == C_dialect_cplusplus && exceptions_enabled);
  sfkind = rp->special_kind;
  class_type = parent_class_of(rp);
  rout_type = rp->type;
  first_param = rout_type->variant.routine.extra_info->param_type_list;
  /* Go through the base classes looking for matching special functions, and
     merge the exception specifications. */
  bcp = base_classes_of(class_type);
  for (; bcp != NULL; bcp = bcp->next) {
    if (is_or_contains_template_param(bcp->type)) {
      /* We cannot tell what dependent bases might end up throwing. */
      throw_any = TRUE;
    } else if (bcp->direct) {
      sym = special_function_symbol(bcp->type, sfkind, first_param,
                                    &rp->source_corresp.decl_position,
                                    &ambiguous);
      if (ambiguous) {
        /* If there's an ambiguity, assume anything might be thrown. */
        throw_any = TRUE;
      } else if (sym != NULL) {
        /* Form the union of exception specifications. */
        throw_any = merge_exception_specifications(sym, rout_type);
      }  /* if */
    }  /* if */
    /* If any exception might be thrown (i.e., if the union is the universe),
       stop looking. */
    if (throw_any) break;
  }  /* for */
  if (!throw_any) {
    fp = class_type->variant.class_struct_union.field_list;
    for (; fp != NULL; fp = fp->next) {
      tp = fp->type;
      if (is_array_type(tp)) tp = underlying_array_element_type(tp);
      tp = skip_typedefs(tp);
      if (is_or_contains_template_param(tp)) {
        /* We cannot tell what dependent fields might end up throwing. */
        throw_any = TRUE;
      } else if (is_immediate_class_type(tp)) {
        sym = special_function_symbol(tp, sfkind, first_param,
                                      &rp->source_corresp.decl_position,
                                      &ambiguous);
        if (ambiguous) {
          /* If there's an ambiguity, assume anything might be thrown. */
          throw_any = TRUE;
        } else if (sym != NULL) {
          /* Form the union of exception specifications. */
          throw_any = merge_exception_specifications(sym, rout_type);
        }  /* if */
      }  /* if */
      /* If any exception might be thrown, stop looking. */
      if (throw_any) break;
    }  /* for */
  }  /* if */
  if (throw_any) {
    /* Clear the exception_specification pointer, in case it had been set. */
    rout_type->variant.routine.extra_info->exception_specification = NULL;
  }  /* if */
}  /* form_exception_specification_for_generated_function */


static a_boolean compatible_functions_with_c_linkage(a_symbol_ptr sym1,
                                                     a_symbol_ptr sym2)
/*
Return TRUE if the two given function symbols have C linkage and identical
types; otherwise, return FALSE.
*/
{
  a_boolean  result = FALSE;
  a_routine_ptr  rp1 = sym1->variant.routine.ptr,
                 rp2 = sym2->variant.routine.ptr;

  if (identical_types(rp1->type, rp2->type) &&
      rp1->type->variant.routine.extra_info->routine_name_linkage ==
                                          (a_name_linkage_kind)nlk_external &&
      rp2->type->variant.routine.extra_info->routine_name_linkage ==
                                          (a_name_linkage_kind)nlk_external) {
    result = TRUE;
  }  /* if */
  return result;
}  /* compatible_functions_with_c_linkage */


a_boolean conflicts_with_previous_function_decl(a_symbol_ptr       using_sym,
                                                a_symbol_ptr       sym,
                                                a_source_position  *pos)
/*
using_sym is a function symbol referred to by a using-declaration, either a
member of base class or a member of a namespace.  Unless it conflicts with a
function previously declared in the current scope, a projection symbol will
be created for it and it will be added to an overload list involving sym
(which may be an overload symbol).  Look for such a conflict, returning TRUE
if one if found.  In some cases a diagnostic should be issued; use *pos
as the error position.
*/
{
  a_boolean      conflicts = FALSE;
  a_boolean      is_list = FALSE;
  a_boolean      err;
  
  if (using_sym->kind == (a_symbol_kind)sk_function_template) {
    /* No need to do a check on function templates. */
  } else {
    if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
      /* We need to search an overload set. */
      is_list = TRUE;
      sym = sym->variant.overloaded_function.symbols;
    }  /* if */
    check_assertion(using_sym->is_class_member ?
                      using_sym->kind == (a_symbol_kind)sk_member_function :
                      using_sym->kind == (a_symbol_kind)sk_routine);
    /* Go through all function declarations in the current scope with the
       same name.  Ignore projection symbols. */
    for (; sym != NULL; sym = is_list ? sym->next : NULL) {
      if (sym->kind == using_sym->kind) {
        /* Check for a conflict between the type of the previously declared
           function (sym) and the type for which a projection symbol is about
           to be created (using_sym). */
        if ((microsoft_mode || gpp_mode) &&
            using_sym->kind == (a_symbol_kind)sk_routine &&
            compatible_functions_with_c_linkage(using_sym, sym)) {
          /* In Microsoft and GNU modes, extern "C" functions from different
             namespaces create different entities even if they have the same
             name and type.  However, two such entities do not conflict if
             they are brought in the same scope with a using-declaration. */
        } else if (types_of_decl_and_using_decl_conflict(
                                                      sym, using_sym, &err)) {
          /* Unless using_sym is a member function being hidden and/or
             overridden by the previous declaration, an error is issued.
             (In Microsoft mode, this case is not diagnosed.  The reverse
             case where a new declaration conflicts with a using-declaration
             is correctly diagnosed by Microsoft compilers.) */
          if (err && !microsoft_mode) {
            pos_sy2_error(ec_using_decl_conflicts_with_prev_decl, pos,
                          using_sym, sym);
          }  /* if */
          conflicts = TRUE;
          break;
        }  /* if */
      } else if (is_class_member_using_decl_symbol(sym)) {
        if (fundamental_symbol_of(sym) == using_sym) {
          /* A prior using declaration refers to the very same base class
             member.  Issue a diagnostic if appropriate and return TRUE to
             assure that the new one isn't added to the overload set, too. */
          if (sym->variant.projection.access !=
                      scope_stack[decl_scope_level].current_access) {
            /* Two using-declarations give different access to the same
               inherited member. */
            pos_sy_error(ec_cannot_change_access, pos,
                         fundamental_symbol_of(sym));
          }  /* if */
          conflicts = TRUE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  return conflicts;
}  /* conflicts_with_previous_function_decl */

#if MICROSOFT_EXTENSIONS_ALLOWED

void merge_dll_flags_from_parent_class(a_type_ptr          class_type,
                                       a_decl_parse_state  *dps)
/*
class_type is the type of the current class, in which the decl_modifiers
field may have been set to indicate modifiers for the class as a whole, and
a field in *dps represents the modifiers declared for the current member.
Check for compatibility regarding the DM_DLLIMPORT/DM_DLLEXPORT flags and
update *dps based on the two.
*/
{
  a_decl_modifier  decl_modifiers, class_decl_modifiers;

  class_decl_modifiers = class_type_supp(class_type)->decl_modifiers;
  /* Only dllimport and dllexport are applied to members, so strip off any
     others that may have been declared for the class as a whole (e.g.,
     novtable). */
  class_decl_modifiers &= DM_DLLFLAGS;
  if (class_decl_modifiers != DM_NONE) {
    decl_modifiers = dps->decl_modifiers.flags;
    if (decl_modifiers & DM_DLLFLAGS) {
      /* If there are dll modifiers on the class, they cannot appear on the
         member declaration, too. */
      pos_diagnostic(es_discretionary_error,
                     ec_class_and_member_have_dll_interface, &dps->start_pos);
      decl_modifiers &= ~(a_decl_modifier)DM_DLLFLAGS;
    }  /* if */
    if (dps->is_definition && (class_decl_modifiers & DM_DLLIMPORT)) {
      /* Put no dll attribute on an inline member function. */
    } else {
      /* Merge the sets of flags. */
      decl_modifiers |= class_decl_modifiers;
    }  /* if */
    dps->decl_modifiers.flags = decl_modifiers;
  }  /* if */
}  /* merge_dll_flags_from_parent_class */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void record_assignment_operator_in_class_symbol(
                                 a_class_symbol_supplement_ptr  cssp,
                                 a_symbol_ptr                   sym,
                                 a_symbol_ptr                   overload_sym)
/*
Update the assignment_operator field in the indicated class symbol
supplement.  sym is an assignment operator symbol.  overload_sym is the
overload set to which sym belongs; it may be NULL.
*/
{
  if (cssp->assignment_operator == NULL) {
    cssp->assignment_operator = overload_sym == NULL ? sym : overload_sym;
  } else if (cssp->assignment_operator->kind ==
                                    (a_symbol_kind)sk_overloaded_function) {
    /* The overloaded function symbol is already registered. */
  } else {
    /* The overloaded function symbol was just created. */
    cssp->assignment_operator = overload_sym;
  }  /* if */
}  /* record_assignment_operator_in_class_symbol */
  

void check_member_decl_is_copy_constructor(
				a_routine_ptr		rout_ptr,
				a_type_ptr		class_type,
				a_boolean		compiler_generated)
/*
Determine whether the routine pointed to by rout_ptr is a copy constructor.
Update the flags in the class symbol supplement accordingly.
*/
{
  a_class_symbol_supplement_ptr	cssp;
  a_type_qualifier_set          qualifiers;

  cssp = symbol_supplement_for_class(class_type);
  if (is_copy_constructor(rout_ptr, class_type, &qualifiers,
                          rvalue_ctor_is_copy_ctor,
                          /*is_declarative_context=*/TRUE)) {
    cssp->has_copy_constructor = TRUE;
    if (qualifiers & TQ_CONST) {
      cssp->has_copy_constructor_for_const_object = TRUE;
    }  /* if */
    if (!compiler_generated && !rout_ptr->is_defaulted) {
      /* Record the presence of a user-provided copy constructor.  Later, this
         will also imply that cssp->construction_by_bitwise_copy_allowed is
         FALSE because we cannot a priori assume that copy construction will
         involve a trivial copy constructor even though after overload
         resolution that may still be the case.  For example:
             struct S {
               S(S const&) = default;  // Trivial.
               S(S&);                  // Nontrivial.
             };
         For now, cssp->construction_by_bitwise_copy_allowed is left unchanged
         so that it reflects whether generated/defaulted copy constructors
         would be trivial (see also mark_trivial_copy_functions). */
      cssp->has_user_provided_copy_constructor = TRUE;
    }  /* if */
  }  /* if */
}  /* check_member_decl_is_copy_constructor */


static void make_virtual_function_pure(a_routine_ptr  routine,
                                       a_type_ptr     class_type)
/*
Update the given virtual routine to indicate that it is "pure virtual".
Also record the presence of a pure virtual function in the given class type.
*/
{
  routine->pure_virtual = TRUE;
  class_type->variant.class_struct_union.any_pure_virtual_functions = TRUE;
  class_type->variant.class_struct_union.abstract = TRUE;
}  /* make_virtual_function_pure */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean check_virtual_interface_member(a_class_def_state  *state,
                                                a_routine_ptr      rtn,
                                                a_symbol_locator   *locator)
/*
The given routine is being declared in a class type described by state.  Return
whether the member should be (pure) virtual because it appears in an interface
type.  If the current class is an interface type, issue an error for member
functions that cannot be (user-) declared in interface class types.  If the
current class is not an interface, state->potentially_interface_like may be
set to FALSE (and FALSE is always returned).
*/
{
  a_boolean   is_implicitly_pure_virtual = FALSE, in_interface;
  a_type_ptr  type = state->class_type;

  in_interface = (type->variant.class_struct_union.is_interface ||
                  cli_class_type_kind_is(type, cctk_interface));
  if (in_interface || state->potentially_interface_like) {
    switch (rtn->special_kind) {
      case sfk_none:
        is_implicitly_pure_virtual = in_interface;
        break;
      case sfk_constructor:
      case sfk_destructor:
        if (!rtn->compiler_generated) {
          if (in_interface) {
            pos_error(ec_interface_cannot_have_ctor_or_dtor,
                      &locator->source_position);
          } else {
            state->potentially_interface_like = FALSE;
          }  /* if */
        }  /* if */
        break;
      case sfk_conversion:
      case sfk_operator:
        if (!rtn->compiler_generated) {
          if (type->variant.class_struct_union.is_interface) {
            pos_error(ec_interface_cannot_have_operator,
                      &locator->source_position);
          } else {
            state->potentially_interface_like = FALSE;
          }  /* if */
        }  /* if */
        break;
      case sfk_static_constructor:
      case sfk_property_get:
      case sfk_property_set:
      case sfk_event_add:
      case sfk_event_remove:
      case sfk_event_raise:
        /* Static constructors, properties, and events are allowed on C++/CLI
           managed interface types (e.g. "interface class"), but not on
           non-CLI "__interface" types. */
        check_assertion(!type->variant.class_struct_union.is_interface);
        break;
      case sfk_finalizer:
        /* Finalizers are only allowed in C++/CLI ref class types. */
        check_assertion_or_expect_error(is_ref_class_type(type) &&
                                        !rtn->compiler_generated);
        break;
      default:
        unexpected_condition();
    }  /* switch */
  }  /* if */
  return is_implicitly_pure_virtual;
}  /* check_virtual_interface_member */


static a_symbol_ptr interface_slot_override(a_symbol_locator       *loc,
                                            a_symbol_ptr           base_sym)
/*
A member function was declared with the qualified declarator described by loc,
and the qualifier refers to a Microsoft __interface class.  The interface
inherits (i.e., does not itself declare) the corresponding member function
described by base_sym.
Return (and, if needed, create) a special member function (an "interface slot")
in the interface class that the function entry in the derived class can refer
to (with its overridden_functions field).
(Note: There is no counterpart for this in C++/CLI interface classes.)
*/
{
  a_symbol_ptr      result = NULL;
  a_routine_ptr     rp;
  a_routine_ptr     base_rp = base_sym->variant.routine.ptr;
  a_type_ptr        parent_class = qualifier_class_type(*loc), rp_type;
  a_scope_ptr       parent_scope = class_type_supp(parent_class)->assoc_scope;

  check_assertion(is_interface_like(sym_parent_class(base_sym)) &&
                  base_rp->pure_virtual);
  /* First look through the routines list of parent_class to see if we
     already created the required entry. */
  for (rp = parent_scope->routines; rp != NULL; rp = rp->next) {
    if (rp->virtual_function_number == base_rp->virtual_function_number) {
      check_assertion(symbol_for(rp)->header == loc->symbol_header &&
                      rp->pure_virtual);
      check_assertion(rp->interface_slot);
      result = symbol_for(rp);
      break;
    }  /* if */
  }  /* for */
  if (result == NULL) {
    /* Create a new interface entry slot (routine entry and associated symbol).
       Note that since parent_class is complete (i.e., its scope is not on the
       scope stack), we cannot call some of the more common routines to
       enter and initialize the symbol. */
    result = alloc_symbol((a_symbol_kind)sk_member_function,
                          loc->symbol_header, &loc->source_position);
    result->decl_scope = parent_scope->number;
    rp_type = copy_routine_type_with_param_types(base_rp->type,
                                                 /*copy_default_args=*/FALSE);
    rp_type->variant.routine.extra_info->this_class = parent_class;
    rp = make_routine(rp_type, (a_storage_class)sc_extern, NO_SCOPE_DEPTH);
    rp->next = parent_scope->routines;
    parent_scope->routines = rp;
    result->variant.routine.ptr = rp;
    rp->source_corresp.assoc_info = (char*)result;
    set_source_corresp_name(&rp->source_corresp, result->header);
    set_class_membership(result, &rp->source_corresp, parent_class);
    rp->source_corresp.is_local_to_function =
                            parent_class->source_corresp.is_local_to_function;
    rp->interface_slot = TRUE;
    rp->is_virtual = TRUE;
    /* Don't call make_virtual_function_pure, because "interface slots" should
       not cause the parent class' any_pure_virtual_functions flag to become
       TRUE. */
    rp->pure_virtual = TRUE;
    rp->virtual_function_number = base_rp->virtual_function_number;
    check_assertion(curr_il_region_number == file_scope_region_number);
    rp->overridden_functions = alloc_il_entity_list_entry();
    rp->overridden_functions->entity.kind = (a_byte_il_entry_kind)iek_routine;
    rp->overridden_functions->entity.ptr = (char*)base_rp;
    rp->compiler_generated = TRUE;
    /* The symbol must be entered in the symbol table so it can be encountered
       by check_for_virtual_function, but it shouldn't be found by name
       lookup. */
    result->is_invisible = TRUE;
    enter_symbol_into_completed_class(result);
  }  /* if */
  return result;
}  /* interface_slot_override */


static a_symbol_ptr find_explicitly_overridden_member(
                                           a_symbol_locator       *locator,
                                           a_class_def_state_ptr  class_state,
                                           a_type_ptr             member_type)
/*
We're declaring a member function with type member_type using a qualified
declarator described by locator.  class_state describes the class in which the
member is being declared. This is a microsoft extension to select a virtual
function from a particular base class type to be overridden.  Return that
function or NULL if none can be found.
*/
{
  a_symbol_ptr   result = NULL;
  a_type_ptr     class_type = class_state->class_type;
  a_type_ptr     parent_class = qualifier_class_type(*locator);

  if (!locator->is_class_member ||
      (!is_same_class_or_base_class_thereof(class_type, parent_class) &&
       !is_template_dependent_type(parent_class))) {
    /* The qualifier was not a base class: Issue an error. */
    pos_ty_error(ec_qualifier_must_be_base_class, &locator->source_position,
                 class_type);
  } else if (same_entities(parent_class, class_type)) {
    /* The qualifier was the class being defined.  This corresponds to a
       different Microsoft bug/extension.  Nothing needs to be done here. */
  } else if (locator->specific_symbol != NULL &&
             locator->specific_symbol->ambiguous) {
    pos_sy_error(ec_ambiguous_name, &locator->source_position,
                 locator->specific_symbol);
  } else {
    a_symbol_ptr  sym = class_qualified_id_lookup(locator, parent_class,
                                                  IDL_DO_NOT_CREATE_PROJ_SYM);
    if (sym != NULL) {
      if (is_member_function_symbol(sym)) {
        /* The qualified declarator identified a known member function. */
        sym = member_function_redecl_sym_with_template_flag(
                                                    sym, member_type,
                                                    (a_template_param_ptr)NULL,
                                                    /*templates_only=*/FALSE,
                                                    (a_symbol_ptr*)NULL);
        if (sym != NULL) {
          a_type_ptr  sym_parent = sym_parent_class(sym);
          if (is_interface_like(sym_parent) && sym_parent != parent_class) {
            /* Lookup might have found an inherited member.  That is okay if
               the member is inherited from an interface class, but the
               recorded base must be a proper member of the designated base
               class. */
            sym = interface_slot_override(locator, sym);
          }  /* if */
          if (!sym->variant.routine.ptr->is_virtual ||
              !sym->variant.routine.ptr->pure_virtual) {
            pos_error(ec_invalid_selective_overrider_declaration,
                      &locator->source_position);
          } else {
            result = sym;
          }  /* if */
        }  /* if */
      } else if (is_nontype_template_param_symbol(sym) &&
                 sym->variant.constant->variant.template_param.kind ==
                                (a_template_param_constant_kind)tpck_member) {
        /* A reference to a dependent base member. */
        result = sym;
      } else {
        sym = NULL;
      }  /* if */
    }  /* if */
    if (sym == NULL) {
      pos_error(ec_invalid_selective_overrider_declaration,
                &locator->source_position);
    }  /* if */
  }  /* if */
  return result;
}  /* find_explicitly_overridden_member */

#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS

a_boolean microsoft_routine_def_is_unmovable(a_boolean  explicit_overrider)
/*
Microsoft C++ allows some extended forms of member function definitions that
cannot be moved outside a class definition.  The first case are members
declared with a qualified name to indicate that overriding is limited to a
specific base class (there is no syntax to define such functions outside their
parent class definition).  The second are members defined in delayed nested
class definitions that appear in class scope.  For example:
    struct A {
      struct B {
        struct C;
      };
      struct B::C {
        void f() {};
      } x;
    };
The definition of A::B::C::f() cannot be placed immediately after the
definition of A::B::C, nor after the definition of A.
Return TRUE if the current declaration is such a member or friend that cannot
be moved.  explicit_overrider is TRUE if the current declaration is an
explicit overrider (which means this routine will return TRUE).
*/
{
  a_boolean   result = FALSE;
  a_type_ptr  class_type;

  check_assertion(scope_stack_top().kind ==
                                         (a_scope_kind)sck_class_struct_union);
  class_type = scope_stack_top().assoc_type;
  if (explicit_overrider && !is_immediate_managed_class_type(class_type)) {
    /* An explicitly overridden function cannot be defined outside its parent
       class (except in C++/CLI managed class types, which use a different
       explicit overriding syntax).  */
    result = TRUE;
  } else {
    if (class_type->source_corresp.is_class_member) {
      /* A member or friend of a nested class.  If it we are in a delayed
         nested class definition that appears in a class scope, we should not
         attempt to move the member definition outside the class, because it
         cannot appear there. */
      a_scope_depth  d = depth_scope_stack - 1;
      a_boolean      in_reactivated_class =
                 (scope_stack[d].kind == (a_scope_kind)sck_class_reactivation);
      if (in_reactivated_class) {
        /* A delayed nested class definition.  Skip the reactivations and see
           if they appeared in a class scope. */
        while (scope_stack[d].kind == (a_scope_kind)sck_class_reactivation ||
               scope_stack[d].kind ==
                                    (a_scope_kind)sck_namespace_reactivation) {
          d = scope_stack[d].previous_scope;
        }  /* while */
        result = (scope_stack[d].kind == (a_scope_kind)sck_class_struct_union);
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* microsoft_routine_def_is_unmovable */

#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean assignment_operator_for_copy_exists(
                                            a_symbol_ptr  sym,
                                            a_boolean     move_assign_okay,
                                            a_boolean     *p_is_user_provided,
                                            a_boolean     *p_const_okay)
/*
Return TRUE if sym is not NULL and qualifies as an assignment operator that
can copy a class object (if move_assign_okay is TRUE, also consider move
assignment operators).  If sym is an overloaded function, return TRUE if at
least one of the functions qualifies.  Set *p_const_okay TRUE if a const object
can be copied.  If p_is_user_provided is non-NULL, set *p_is_user_provided to
whether one of the operators is user-provided.
*/
{
  a_boolean             sym_is_overloaded, const_okay = FALSE;
  a_boolean             is_ref_arg;
  a_type_qualifier_set  qualifiers_accepted;
  a_boolean             found_assignment_operator_for_copy = FALSE;
  a_boolean             is_base_class_match;

  db_enter(4, "assignment_operator_for_copy_exists");
  if (p_is_user_provided != NULL) *p_is_user_provided = FALSE;
  if (sym != NULL) {
    sym_is_overloaded = (sym->kind == (a_symbol_kind)sk_overloaded_function);
    if (sym_is_overloaded) sym = sym->variant.overloaded_function.symbols;
    /* Loop through the one or more symbols looking for one with the right
       argument type. */
    for (; sym != NULL; sym = sym_is_overloaded ? sym->next : NULL) {
      a_symbol_ptr  viable_sym = NULL;
      qualifiers_accepted = TQ_NONE;
      if (sym->kind == (a_symbol_kind)sk_member_function &&
          is_assignment_operator_for_copy(sym, move_assign_okay, &is_ref_arg,
                                          &qualifiers_accepted,
                                          &is_base_class_match)) {
        viable_sym = sym;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (microsoft_bugs &&
                 sym->kind == (a_symbol_kind)sk_function_template) {
        viable_sym = copy_assignment_specialization(sym, &is_ref_arg,
                                                    &qualifiers_accepted,
                                                    &is_base_class_match);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
      if (viable_sym != NULL) {
        /* Found an assignment operator that can serve to make a copy of
           the current class. */
        found_assignment_operator_for_copy = TRUE;
        if (!viable_sym->variant.routine.ptr->compiler_generated &&
            !viable_sym->variant.routine.ptr->is_defaulted) {
          if (p_is_user_provided != NULL) *p_is_user_provided = TRUE;
        }  /* if */
        /* If it takes the object to be copied by value, a const object
           may be copied; if it takes it by reference, a const qualifier
           must be present on the parameter declaration. */
        if (!is_ref_arg || (qualifiers_accepted & TQ_CONST) != 0) {
          /* A copy assignment operator has been located, and it accepts
             a const object. */
          const_okay = TRUE;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  /* Set const_okay if the copy assignment operator is implicit, or if the
     implicit one allows the copying of const objects. */
  *p_const_okay = !found_assignment_operator_for_copy || const_okay;
  db_exit();
  return found_assignment_operator_for_copy;
}  /* assignment_operator_for_copy_exists */


static a_boolean default_assignment_of_const_object_okay(a_type_ptr class_type)
/*
We are about to declare a compiler-generated or an explicitly-defaulted copy
assignment operator for the given class type.  Return whether it can copy a
const object.  This is dependent on the assignment operators defined for base
classes and fields of the given class.
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
      if (assignment_operator_for_copy_exists(cssp->assignment_operator,
                                              /*move_assign_okay=*/FALSE,
                                              (a_boolean*)NULL, &const_okay) &&
          !const_okay) {
        /* There is a default assignment operator for this base class type,
           but it does not accept a const object.  No need to look any
           further. */
        goto done;
      }  /* if */
    }  /* if */
  }  /* for */
  /* Base classes are okay.  Now check the nonstatic data members. */
  sym = symbol_for(class_type)->variant.class_struct_union.extra_info->symbols;
  for (; sym != NULL; sym = sym->next_in_scope) {
    if (sym->kind == (a_symbol_kind)sk_field) {
      fp = sym->variant.field.ptr;
      tp = fp->type;
      /* Get the element type if this is an array field. */
      if (is_array_type(tp)) tp = underlying_array_element_type(tp);
      if (is_class_struct_union_type(tp)) {
        cssp = symbol_supplement_for_class(tp);
        if (assignment_operator_for_copy_exists(cssp->assignment_operator,
                                                /*move_assign_okay=*/FALSE,
                                                (a_boolean*)NULL,
                                                &const_okay) &&
            !const_okay) {
          /* There is a default assignment operator for this nonstatic data
             member's class type, but it does not accept a const object.
             No need to look any further. */
          goto done;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
done:;
  db_exit();
  /* Return TRUE unless a subobject type has a default assignment operator
     that cannot accept a const object. */
  return const_okay;
}  /* default_assignment_of_const_object_okay */


static a_boolean constructor_can_be_defaulted(a_symbol_ptr  sym,
                                              a_boolean     *is_default_ctor,
                                              a_boolean     *has_default_arg)
/*
sym is a constructor.  Return whether it can be "defaulted".  I.e., if its
parent class is X, it must have one of the following signatures and not include
a default argument:
	X()
	X(X&)
	X(X const&)
If the signature is the first in the list above, set *is_default_ctor to TRUE;
otherwise set it to FALSE.  If the signature is one of the latter two and the
parameter has an associated default argument set *has_default_arg to TRUE (and
return FALSE); otherwise, set *has_default_arg to FALSE.
*/
{
  a_boolean         result = FALSE;
  a_type_ptr        class_type = sym_parent_class(sym), rout_type;
  a_param_type_ptr  params;

  *is_default_ctor = FALSE;
  *has_default_arg = FALSE;
  check_assertion(sym->kind == (a_symbol_kind)sk_member_function ||
                  (sym->is_error && sym->kind == (a_symbol_kind)sk_routine));
  rout_type = skip_typerefs(sym->variant.routine.ptr->type);
  check_assertion(rout_type->kind == (a_type_kind)tk_routine);
  params = rout_type->variant.routine.extra_info->param_type_list;
  if (rout_type->variant.routine.extra_info->has_ellipsis) {
    /* An ellipsis is not allowed in the signature. */
  } else if (params == NULL) {
    /* No parameters: A default constructor. */
    result = TRUE;
    *is_default_ctor = TRUE;
  } else if (params->next == NULL) {
    /* One parameter: Check the signature. */
    /* The parameter type must be X& or X const& (although the latter requires
       that bases and members allow for such copying).  Try X& first. */
    a_type_ptr  param_type = make_reference_type(class_type);
    if (identical_types(param_type, params->type)) {
      result = TRUE;
    } else {
      param_type = make_reference_type(
                     make_qualified_type(class_type, TQ_CONST));
      if (identical_types(param_type, params->type)) {
        result = TRUE;
      }  /* if */
    }  /* if */
    if (result && params->has_default_arg) {
      /* Don't allow a copy constructor with a default argument to be
         defaulted. */
      result = FALSE;
      *has_default_arg = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* constructor_can_be_defaulted */


static a_boolean assignment_operator_can_be_defaulted(a_symbol_ptr  sym)
/*
sym is an assignment operator.  Check if it can be "defaulted".  I.e., if its
parent class is X, it must have one of the following signatures:
	X& operator=(X&)
	X& operator=(X const&)
*/
{
  a_boolean         result = FALSE;
  a_type_ptr        class_type = sym_parent_class(sym);
  a_type_ptr        rout_type, return_type;
  a_param_type_ptr  params;

  check_assertion(sym->kind == (a_symbol_kind)sk_member_function ||
                  (sym->is_error && sym->kind == (a_symbol_kind)sk_routine));
  rout_type = skip_typerefs(sym->variant.routine.ptr->type);
  check_assertion(rout_type->kind == (a_type_kind)tk_routine);
  params = rout_type->variant.routine.extra_info->param_type_list;
  /* Assignment operators can have only one parameter. */
  check_assertion(params->next == NULL);
  /* The operator cannot be a const or volatile member, and the return type
     must be X& (where X is the parent type). */
  return_type = make_reference_type(class_type);
  if (rout_type->variant.routine.extra_info->qualifiers == TQ_NONE &&
      identical_types(return_type, rout_type->variant.routine.return_type)) {
    /* The parameter type must be X& or X const& (although the latter requires
       that bases and members allow for such an assignment).  Try X& first. */
    a_type_ptr  param_type = return_type;
    if (identical_types(param_type, params->type)) {
      result = TRUE;
    } else {
      param_type = make_reference_type(
                     make_qualified_type(class_type, TQ_CONST));
      if (identical_types(param_type, params->type)) {
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* assignment_operator_can_be_defaulted */


void check_defaulted_or_deleted_function(a_decl_parse_state  *dps,
                                         a_func_info_block   *func_info,
                                         a_source_position   *def_pos)
/*
*dps and *func_info describe a function declaration.  If func_info indicates
that the function is being defined with "= default;" or "= delete;", check that
it is appropriate (and issue an error if it is not), and update the routine's
IL entry accordingly.  def_pos is the position of the "= default;" or
"= delete;" construct.
*/
{
  an_error_code      err_code = ec_no_error;
  a_symbol_ptr       sym = dps->sym;
  a_routine_ptr      rp;
  a_source_position  *diag_pos = def_pos;

  /* Retrieve the appropriate IL routine entry. */
  if (is_simple_function_symbol(sym)) {
    rp = sym->variant.routine.ptr;
  } else {
    a_template_symbol_supplement_ptr  tssp;
    check_assertion(sym->kind == (a_symbol_kind)sk_function_template);
    tssp = sym->variant.template_info;
    /* Make sure the func_info flags are also recorded in the template
       symbol supplement. */
    tssp->variant.function.func_info.is_deleted = func_info->is_deleted;
    tssp->variant.function.func_info.is_defaulted = func_info->is_defaulted;
    rp = tssp->variant.function.routine;
  }  /* if */
  if (func_info->is_deleted) {
    if (!dps->first_decl) {
      err_code = ec_deleted_function_definition_must_be_first_declaration;
    } else if (dps->first_decl_of_predeclared_entity) {
      err_code = ec_predeclared_function_cannot_be_deleted;
    } else {
      /* A deleted definition is implicitly "inline". */
      rp->is_deleted = TRUE;
      rp->is_inline = TRUE;
      rp->defined = TRUE;
    }  /* if */
  } else if (func_info->is_defaulted) {
    /* Verify that sym represents a special member function for which a
       definition can be generated. */
    if ((dps->dso_flags & DSO_FRIEND) != 0) {
      /* A special member cannot be defined in a friend declaration. */
      err_code = ec_function_defaulted_in_friend_decl;
    } else if (sym->kind == (a_symbol_kind)sk_function_template) {
      /* Templates (and member templates) cannot be defaulted. */
      err_code = ec_function_template_cannot_be_defaulted;
    } else if (rp->special_kind == (a_special_function_kind)sfk_constructor) {
      a_boolean  is_default_ctor, has_default_arg;
      if (constructor_can_be_defaulted(sym, &is_default_ctor,
                                       &has_default_arg)) {
        rp->is_defaulted = TRUE;
        if (is_default_ctor && dps->in_class_scope) {
          /* The "= default" declaration appeared on the in-class declaration
             of the canonical default constructor.  Assume the default
             constructor is trivial for now and revisit the flag later. */
          rp->is_trivial_default_constructor = TRUE;
        }  /* if */
      } else {
        err_code = has_default_arg ?
                             ec_copy_ctor_with_default_arg_cannot_be_defaulted
                           : ec_invalid_constructor_to_be_defaulted;
        diag_pos = &dps->declarator_pos;
      }  /* if */
    } else if (rp->special_kind == (a_special_function_kind)sfk_destructor) {
      rp->is_defaulted = TRUE;
    } else if (rp->special_kind == (a_special_function_kind)sfk_operator &&
               rp->variant.opname_kind == (an_opname_kind)onk_assign) {
      if (assignment_operator_can_be_defaulted(sym)) {
        rp->is_defaulted = TRUE;
      } else {
        err_code = ec_invalid_assignment_operator_to_be_defaulted;
        diag_pos = &dps->declarator_pos;
      }  /* if */
    } else {
      /* Not a constructor, destructor, or assignment operator. */
      err_code = ec_invalid_function_to_be_defaulted;
    }  /* if */
    if (!microsoft_mode && dps->first_decl &&
        (rp->source_corresp.access != (an_access_specifier)as_public ||
         (dps->dso_flags & DSO_EXPLICIT) != 0)) {
      /* If a member is non-public or explicit, it cannot be defaulted in the
         class definition.  (Microsoft compilers do not currently implement
         this rule -- it came late in the standardization process.) */
      pos_error(ec_nonpublic_or_explicit_member_defaulted_in_class, def_pos);
    }  /* if */
  }  /* if */
  if (err_code != ec_no_error) {
    pos_error(err_code, diag_pos);
    /* Discard the definition (and make the routine non-inline). */
    func_info->is_definition = FALSE;
    func_info->is_inline = FALSE;
    sym->defined = FALSE;
    rp->defined = FALSE;
    rp->defined_in_friend_decl = FALSE;
    rp->is_inline = FALSE;
    rp->storage_class = (a_storage_class)sc_extern;
  }  /* if */
}  /* check_defaulted_or_deleted_function */

#if GNU_EXTENSIONS_ALLOWED

#if !GNU_VISIBILITY_ATTRIBUTE_ALLOWED
/*ARGSUSED*/
#endif /* !GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
static void copy_gnu_class_properties_to_routine(a_type_ptr     class_type,
                                                 a_routine_ptr  routine)
/*
routine is a member function of class_type.  Copy any properties of class_type
(specified by GNU attributes) that should be propagated to its member
functions.
*/
{
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  if (routine->ELF_visibility == (an_ELF_visibility_kind)evk_unspecified) {
    routine->ELF_visibility = class_type_supp(class_type)->ELF_visibility;
  }  /* if */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
}  /* copy_class_gnu_properties_to_routine */

#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean in_cli_property_or_event_definition(void)
/*
Return TRUE if we are currently parsing the brace-enclosed definition of a
non-trivial C++/CLI property or event.
*/
{
  a_class_def_state  *cdsp = scope_stack_top().class_def_state;

  return (cdsp != NULL && cdsp->property_or_event_descr != NULL);
}  /* in_cli_property_or_event_definition */


a_boolean in_static_cli_property_or_event_definition(void)
/*
Return TRUE if we are currently parsing the brace-enclosed definition of a
non-trivial static C++/CLI property or event.
*/
{
  a_class_def_state  *cdsp = scope_stack_top().class_def_state;

  return (cdsp != NULL && cdsp->property_or_event_descr != NULL &&
          cdsp->property_or_event_descr->is_static);
}  /* in_static_cli_property_or_event_definition */


static void check_property_accessor_type(a_routine_ptr       rp,
                                         a_decl_parse_state  *dps)
/*
rp points to the entry of a property accessor function.  Check that its type
is compatible with the declaration of the C++/CLI property it is associated
with and issue diagnostics as needed.
*/
{
  a_type_ptr            rtp = skip_typerefs(rp->type), prop_type;
  a_routine_type_supplement_ptr
                        rtsp = rtp->variant.routine.extra_info;
  a_param_type_ptr      ptp = function_type_params(rtp);
  a_property_or_event_descr_ptr
                        pdp = rp->variant.property_or_event_descr;
  a_boolean             is_setter, err = FALSE;

  if (pdp->is_static) {
    prop_type = pdp->variant.variable->type;
  } else {
    prop_type = pdp->variant.field->type;
  }  /* if */
  /* First check the return type. */
  if (rp->special_kind == (a_special_function_kind)sfk_property_get) {
    if (!types_are_compatible(rtp->variant.routine.return_type, prop_type)) {
      pos_error(ec_bad_property_get_return, &dps->start_pos);
      err = TRUE;
    }  /* if */
    is_setter = FALSE;
  } else {
    check_assertion(rp->special_kind ==
                                   (a_special_function_kind)sfk_property_set);
    if (!is_void_type(rtp->variant.routine.return_type) ||
        is_qualified_type(rtp->variant.routine.return_type)) {
      /* The return type of a property "set" accessor must be void; "void
         const" is not acceptable. */
      pos_error(ec_bad_property_set_return, &dps->start_pos);
      err = TRUE;
    }  /* if */
    is_setter = TRUE;
  }  /* if */
  if (!err && pdp->indices != NULL) {
    /* Check that the accessor parameters match the index types.  ECMA-372
       requires identical types, but MSVC++ 10 doesn't appear to compare the
       types at all: So type mismatches are diagnosed as warnings only. */
    a_property_index_type_ptr  pitp = pdp->indices;
    while (!err && ptp != NULL && pitp != NULL) {
      if (is_error_type(pitp->type)) {
        /* Some error occurred while parsing the index types: Additional
           diagnostics are unlikely helpful. */
        err = TRUE;
        break;
      } else if (!types_are_compatible(pitp->type, ptp->type)) {
        pos_warning(is_setter ? ec_property_set_index_type_mismatch
                              : ec_property_get_index_type_mismatch,
                    &pitp->position);
      }  /* if */
      ptp = ptp->next;
      pitp = pitp->next;
    }  /* while */
    if (!err && pitp != NULL) {
      pos_error(is_setter ? ec_property_set_index_type_missing
                          : ec_property_get_index_type_missing,
                &pitp->position);
      err = TRUE;
    }  /* if */
  }  /* if */
  if (!err) {
    if (is_setter) {
      /* Check that the setter has exactly one more parameter that
         corresponds to the property type. */
      if (ptp == NULL) {
        pos_error(ec_property_set_missing_value_parameter,
                  &dps->declarator_pos);
        err = TRUE;
      } else if (ptp->next != NULL) {
        pos_error(ec_extra_property_accessor_parameters, &dps->declarator_pos);
        err = TRUE;
      } else if (!types_are_compatible(ptp->type, prop_type)) {
        pos_error(ec_property_set_value_parameter_mismatch,
                  &dps->declarator_pos);
        err = TRUE;
      }  /* if */
    } else if (ptp != NULL) {
      /* The setter has extra parameters: Issue an error. */
      pos_error(pdp->indices == NULL ? ec_property_get_cannot_have_parameter
                                     : ec_extra_property_accessor_parameters,
                &dps->declarator_pos);
      err = TRUE;
    }  /* if */
  }  /* if */
  if (!err) {
    if (rtsp->qualifiers != TQ_NONE) {
      pos_error(ec_qualified_cli_accessor, &dps->declarator_pos);
      err = TRUE;
    } else if (rtsp->has_ellipsis) {
      pos_error(ec_ellipsis_cli_accessor, &dps->declarator_pos);
      err = TRUE;
    }  /* if */
  }  /* if */
}  /* check_property_accessor_type */


static void check_property_accessor(a_symbol_ptr            sym,
                                    a_member_decl_info_ptr  decl_info,
                                    a_class_def_state_ptr   class_state)
/*
sym represents a member function declared in a property definition.  Check
that it is a valid "get" or "set" accessor for the property and issue
diagnostics as appropriate.  Update the associated IL entries to reflect that
the member function is an accessor for the property (if the accessor is valid).
*/
{
  a_routine_ptr         rp = sym->variant.routine.ptr;
  a_property_or_event_descr_ptr
                        pdp = class_state->property_or_event_descr;
  a_decl_parse_state    *dps = &decl_info->decl_state;

  check_assertion(pdp->kind == (a_property_or_event_kind)pek_cli_property);
  if (rp->special_kind != (a_special_function_kind)sfk_none) {
    /* A special member (like a constructor) declared in a property definition.
       Issue an error. */
    pos_error(ec_invalid_property_accessor_decl, &dps->declarator_pos);
  } else if (strcmp(rp->source_corresp.name, "get") == 0) {
    if (pdp->get_routine.ptr != NULL) {
      pos2_diagnostic(es_error, ec_property_get_already_declared,
                      &dps->declarator_pos,
                      &pdp->get_routine.ptr->source_corresp.decl_position);
    } else {
      rp->special_kind = (a_special_function_kind)sfk_property_get;
    }  /* if */
  } else if (strcmp(rp->source_corresp.name, "set") == 0) {
    if (pdp->set_routine.ptr != NULL) {
      pos2_diagnostic(es_error, ec_property_set_already_declared,
                      &dps->declarator_pos,
                      &pdp->set_routine.ptr->source_corresp.decl_position);
    } else {
      rp->special_kind = (a_special_function_kind)sfk_property_set;
    }  /* if */
  } else {
    /* Neither "get" nor "set": Issue an error. */
    pos_error(ec_invalid_property_accessor_decl, &dps->declarator_pos);
  }  /* if */
  if (rout_is_cli_accessor(rp)) {
    rp->variant.property_or_event_descr = pdp;
    if (rp->special_kind == (a_special_function_kind)sfk_property_get) {
      pdp->get_routine.ptr = rp;
    } else {
      pdp->set_routine.ptr = rp;
    }  /* if */
    check_property_accessor_type(rp, dps);
    if ((pdp->is_virtual &&
         dps->declared_storage_class == (a_storage_class)sc_static) ||
        (pdp->is_static && (dps->dso_flags & DSO_VIRTUAL))) {
      pos_error(ec_virtual_static_property_accessor, &dps->specifiers_pos);
      decl_info->invalid_virtual_specifier = TRUE;
    }  /* if */
  }  /* if */
}  /* check_property_accessor */


static void check_event_accessor_type(a_routine_ptr       rp,
                                      a_decl_parse_state  *dps)
/*
rp points to the entry of an event accessor function.  Check that its type
is compatible with the declaration of the C++/CLI event it is associated
with and issue diagnostics as needed.
*/
{
  a_type_ptr            rtp = skip_typerefs(rp->type), prop_type;
  a_routine_type_supplement_ptr
                        rtsp = rtp->variant.routine.extra_info;
  a_param_type_ptr      ptp = function_type_params(rtp);
  a_property_or_event_descr_ptr
                        pdp = rp->variant.property_or_event_descr;
  a_boolean             err;

  if (pdp->is_static) {
    prop_type = pdp->variant.variable->type;
  } else {
    prop_type = pdp->variant.field->type;
  }  /* if */
  err = is_error_type(prop_type);
  if (err) {
    /* Further error checks are unlikely to be helpful. */
    expect_error();
  } else if (rp->special_kind == (a_special_function_kind)sfk_event_add ||
             rp->special_kind == (a_special_function_kind)sfk_event_remove) {
    /* First check the return type. */
    if (!is_void_type(rtp->variant.routine.return_type) ||
        is_qualified_type(rtp->variant.routine.return_type)) {
      /* The return type of an event "add" or "remove" accessor must be void;
         "void const" is not acceptable. */
      if (!is_error_type(rtp->variant.routine.return_type)) {
        pos_error(ec_bad_event_add_or_remove_return, &dps->start_pos);
      }  /* if */
      err = TRUE;
    } else {
      /* Check that the "add" or "remove" accessor has exactly one parameter
         that corresponds to the property type. */
      if (ptp == NULL) {
        pos_error(ec_event_accessor_missing_value_parameter,
                  &dps->declarator_pos);
        err = TRUE;
      } else if (ptp->next != NULL) {
        pos_error(ec_extra_event_accessor_parameters, &dps->declarator_pos);
        err = TRUE;
      } else if (!types_are_compatible(ptp->type, prop_type)) {
        pos_ty2_diagnostic(es_error,
                           ec_event_accessor_value_parameter_mismatch,
                           &dps->declarator_pos, ptp->type, prop_type);
        err = TRUE;
      }  /* if */
    }  /* if */
  } else {
    a_type_ptr  invocation_type;
    check_assertion(rp->special_kind ==
                                   (a_special_function_kind)sfk_event_raise);
    invocation_type = delegate_invocation_type(type_pointed_to(prop_type));
    if (!f_types_are_compatible(rtp, invocation_type,
                                TCF_IGNORE_THIS_CLASS_TYPE |
                                TCF_ERROR_TYPE_COMPATIBLE_WITH_ANYTHING)) {
      pos_error(ec_event_raise_type_mismatch, &dps->start_pos);
      err = TRUE;
    }  /* if */
  }  /* if */
  if (!err) {
    if (rtsp->qualifiers != TQ_NONE) {
      pos_error(ec_qualified_cli_accessor, &dps->declarator_pos);
      err = TRUE;
    } else if (rtsp->has_ellipsis) {
      pos_error(ec_ellipsis_cli_accessor, &dps->declarator_pos);
      err = TRUE;
    }  /* if */
  }  /* if */
}  /* check_event_accessor_type */


static void check_event_accessor(a_symbol_ptr            sym,
                                 a_member_decl_info_ptr  decl_info,
                                 a_class_def_state_ptr   class_state)
/*
sym represents a member function declared in an event definition.  Check that
it is a valid "add", "remove", or "raise" accessor for the event and issue
diagnostics as appropriate.  Update the associated IL entries to reflect that
the member function is an accessor for the event (if the accessor is valid).
*/
{
  a_routine_ptr         rp = sym->variant.routine.ptr;
  a_property_or_event_descr_ptr
                        pdp = class_state->property_or_event_descr;
  a_decl_parse_state    *dps = &decl_info->decl_state;

  check_assertion(pdp->kind == (a_property_or_event_kind)pek_cli_event);
  if (rp->special_kind != (a_special_function_kind)sfk_none) {
    /* A special member (like a constructor) declared in a property definition.
       Issue an error. */
    pos_error(ec_invalid_event_accessor_decl, &dps->declarator_pos);
  } else if (strcmp(rp->source_corresp.name, "add") == 0) {
    if (pdp->get_routine.ptr != NULL) {
      pos2_diagnostic(es_error, ec_event_add_already_declared,
                      &dps->declarator_pos,
                      &pdp->add_routine->source_corresp.decl_position);
    } else {
      rp->special_kind = (a_special_function_kind)sfk_event_add;
    }  /* if */
  } else if (strcmp(rp->source_corresp.name, "remove") == 0) {
    if (pdp->set_routine.ptr != NULL) {
      pos2_diagnostic(es_error, ec_event_remove_already_declared,
                      &dps->declarator_pos,
                      &pdp->remove_routine->source_corresp.decl_position);
    } else {
      rp->special_kind = (a_special_function_kind)sfk_event_remove;
    }  /* if */
  } else if (strcmp(rp->source_corresp.name, "raise") == 0) {
    if (pdp->set_routine.ptr != NULL) {
      pos2_diagnostic(es_error, ec_event_raise_already_declared,
                      &dps->declarator_pos,
                      &pdp->raise_routine->source_corresp.decl_position);
    } else {
      rp->special_kind = (a_special_function_kind)sfk_event_raise;
    }  /* if */
  } else {
    /* Not "add", "remove", or "raise": Issue an error. */
    pos_error(ec_invalid_event_accessor_decl, &dps->declarator_pos);
  }  /* if */
  if (rout_is_cli_accessor(rp)) {
    rp->variant.property_or_event_descr = pdp;
    if (rp->special_kind == (a_special_function_kind)sfk_event_add) {
      pdp->add_routine = rp;
    } else if (rp->special_kind == (a_special_function_kind)sfk_event_remove) {
      pdp->remove_routine = rp;
    } else {
      pdp->raise_routine = rp;
    }  /* if */
    check_event_accessor_type(rp, dps);
    if ((pdp->is_virtual &&
         dps->declared_storage_class == (a_storage_class)sc_static) ||
        (pdp->is_static && (dps->dso_flags & DSO_VIRTUAL))) {
      pos_error(ec_virtual_static_event_accessor, &dps->specifiers_pos);
      decl_info->invalid_virtual_specifier = TRUE;
    }  /* if */
  }  /* if */
}  /* check_event_accessor */


static an_il_entity_list_entry_ptr make_overridden_functions_entry(
                                                            a_symbol_ptr  sym)
/*
Create an entry corresponding to the given symbol for the overridden_functions
list of an IL entry of type a_routine.
*/
{
  an_il_entity_list_entry_ptr  entry;

  check_assertion(curr_il_region_number == file_scope_region_number);
  entry = alloc_il_entity_list_entry();
  if (sym->kind == (a_symbol_kind)sk_member_function) {
    entry->entity.kind = (a_byte_il_entry_kind)iek_routine;
    entry->entity.ptr = (char*)sym->variant.routine.ptr;
  } else {
    check_assertion(is_nontype_template_param_symbol(sym) &&
                    sym->variant.constant->variant.template_param.kind ==
                                 (a_template_param_constant_kind)tpck_member);
    entry->entity.kind = (a_byte_il_entry_kind)iek_constant;
    entry->entity.ptr = (char*)sym->variant.constant;
  }  /* if */
  return entry;
}  /* make_overridden_functions_entry */


static void record_selective_overriding(a_member_decl_info_ptr  decl_info,
                                        a_symbol_ptr            overridden_sym)
/*
decl_info describes a (presumably virtual) member function declaration.  If
that declaration selectively overrides specific base class members, record
that in the derived-class routine entry.  If the overrides were specified
using C++/CLI syntax like "virtual int f() = I::f, K::g;", overridden_sym is
NULL, and decl_info->named_overrides will describe the overridden members.
If a specific override was specified using a qualified member declarator
(e.g., "virtual int B::f();"), overridden_sym describes the overridden member
(there cannot be more than one in that case).
*/
{
  a_routine_ptr  rp = decl_info->decl_state.sym->variant.routine.ptr;

  if (overridden_sym != NULL) {
    check_assertion(decl_info->named_overrides == NULL);
    rp->is_virtual = TRUE;
    rp->overridden_functions = make_overridden_functions_entry(overridden_sym);
  } else if (decl_info->named_overrides != NULL) {
    an_il_entity_list_entry_ptr  *p_entry = &rp->overridden_functions;
    a_symbol_list_entry_ptr      sym_entry = decl_info->named_overrides;
    check_assertion(cppcli_enabled && *p_entry == NULL);
    for (; sym_entry != NULL; sym_entry = sym_entry->next) {
      *p_entry = make_overridden_functions_entry(sym_entry->symbol);
      p_entry = &(*p_entry)->next;
    }  /* for */
  }  /* if */
}  /* record_selective_overriding */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void update_class_for_special_member(a_class_def_state   *class_state,
                                            a_member_decl_info  *decl_info,
                                            a_symbol_ptr        overload_sym)
/*
Update the class structures associated with class_state to reflect the fact
that a special member function (e.g., a constructor) described by decl_info
is being declared in the associated class.  If the special member is part of
an overload set, overload_sym points to the symbol representing that set;
otherwise, it is NULL.
*/
{
  a_type_ptr                 class_type = class_state->class_type;
  a_class_symbol_supplement  *cssp = symbol_supplement_for_class(class_type);
  a_decl_parse_state         *dps = &decl_info->decl_state;
  a_symbol_ptr               sym = dps->sym;
  a_routine_ptr              rtn = sym->variant.routine.ptr;

  switch (rtn->special_kind) {
    case sfk_constructor:
      /* Set the pointer to the constructor symbol in the class symbol
         supplement. */
      if (decl_info->is_trivial_default_constructor) {
        /* An implicitly-declared trivial default constructor is never
           actually called or declared, so it is not added to the constructor
           set (which should be empty). */
        check_assertion(cssp->constructor == NULL);
        cssp->trivial_default_constructor = sym;
        rtn->is_trivial_default_constructor = TRUE;
      } else {
        if (cssp->constructor == NULL) {
          cssp->constructor = sym;
        } else if (cssp->constructor->kind ==
                                    (a_symbol_kind)sk_overloaded_function) {
          /* The overloaded function symbol is already registered. */
        } else if (overload_sym != NULL) {
          /* The overloaded function symbol was just created.  (Unless an
             error occurred, in which case overload_sym is NULL.) */
          cssp->constructor = overload_sym;
        }  /* if */
        /* Determine if this is a default constructor. */
        if (is_default_constructor(rtn, /*is_declarative_context=*/TRUE)) {
          if (rtn->is_trivial_default_constructor) {
            /* A defaulted default constructor.  It is assumed trivial until
               the class is completed, at which point we can make a final
               determination as to whether it is really trivial. */
            cssp->trivial_default_constructor = sym;
          } else {
            cssp->has_nontrivial_default_constructor = TRUE;
          }  /* if */
          if (!rtn->compiler_generated) {
            cssp->has_user_declared_default_constructor = TRUE;
            if (!rtn->is_defaulted) {
              cssp->has_user_provided_default_constructor = TRUE;
              class_state->POD_ruled_out = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
        /* Determine if this is a copy constructor.  If so, set the class
           symbol supplement flags appropriately. */
        check_member_decl_is_copy_constructor(rtn, class_type,
                                              rtn->compiler_generated);
      }  /* if */
      break;
    case sfk_destructor:
      /* Set the pointer to the destructor symbol in the class symbol
         supplement. */
      cssp->destructor = sym;
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case sfk_static_constructor:
      /* If multiple static constructors are declared in the same class, an
         error should have been issued. */
      check_assertion_or_expect_error(cssp->static_constructor == NULL);
      cssp->static_constructor = sym;
      break;
    case sfk_finalizer:
      /* Set the pointer to the finalizer symbol in the class symbol
         supplement. */
      cssp->finalizer = sym;
      break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default:
      /* Nothing to do. */
      break;
  }  /* switch */
}  /* update_class_for_special_member */


a_boolean is_implicitly_callable_conversion_function(a_type_ptr rout_type)
/*
Return TRUE if a conversion function with the indicated routine type is
one that can be implicitly called.  As described in [class.conv.fct],
a conversion function will not be used implicitly to convert T to T,
T to T&, or a number of other conversions that are doable as standard
conversions.
*/
{
  a_boolean  is_implicitly_callable = TRUE;
  a_type_ptr class_type;
  a_type_ptr ret_type;

  rout_type = skip_typerefs(rout_type);
  check_assertion(rout_type->kind == (a_type_kind)tk_routine);
  class_type = rout_type->variant.routine.extra_info->this_class;
  /* Note that return_type_of removes references, which is desired. */
  ret_type = f_skip_typerefs(return_type_of(rout_type));
  if (class_type == NULL) {
    /* This can happen with severe syntax errors. */
    expect_error();
  } else if (same_entities(ret_type, class_type)) {
    /* Converting to same type (possibly qualified) is not allowed. */
    is_implicitly_callable = FALSE;
  } else if (is_immediate_class_type(ret_type)) {
    if (!cfront_2_1_mode && find_base_class_of(class_type, ret_type) != NULL) {
      /* An operator that converts from a derived class to a base class
         is not allowed, except by cfront 2.1. */
      is_implicitly_callable = FALSE;
    }  /* if */
  } else if (is_void_type(ret_type)) {
    /* Conversion to (possibly qualified) void type is not allowed. */
    is_implicitly_callable = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cppcli_enabled &&
             boxing_conversion_possible(class_type, ret_type,
                                        (a_std_conv_descr *)NULL)) {
    /* A conversion function that does a C++/CLI boxing conversion
       is not allowed. */
    is_implicitly_callable = FALSE;
#endif /*MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  return is_implicitly_callable;
}  /* is_implicitly_callable_conversion_function */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void exclude_special_members_from_value_class_type(
                                                a_routine_ptr      rtn,
                                                a_type_ptr         class_type,
                                                a_source_position  *diag_pos)
/*
Check that the given member function of the given C++/CLI value class type is
not a special member that is prohibited in value class types.  If it is, issue
an error at the given position.
*/
{
  an_error_code         err_code = ec_no_error;
  a_type_qualifier_set  tqs;

  switch (rtn->special_kind) {
    case sfk_constructor:
      if (is_copy_constructor(rtn, class_type, &tqs,
                              /*include_move_ctors=*/TRUE,
                              /*is_declarative_context=*/TRUE)) {
        err_code = ec_copy_constructor_in_value_class_type;
      } else if (is_default_constructor(rtn,
                                        /*is_declarative_context=*/TRUE)) {
        err_code = ec_default_constructor_in_value_class_type;
      }  /* if */
      break;
    case sfk_destructor:
      err_code = ec_destructor_in_value_class_type;
      break;
    case sfk_finalizer:
      /* A more general error for finalizers outside ref class types is
         issued elsewhere. */
      expect_error();
      break;
    case sfk_operator:
      if (rtn->variant.opname_kind == (an_opname_kind)onk_assign) {
        err_code = ec_assignment_in_value_class_type;
      }  /* if */
      break;
    default:
      /* Nothing to do. */
      break;
  }  /* switch */
  if (err_code != ec_no_error) {
    pos_error(err_code, diag_pos);
  }  /* if */
}  /* exclude_special_members_from_value_class_type */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void decl_member_function(a_symbol_locator        *locator,
                                 a_func_info_block_ptr   func_info,
                                 a_class_def_state_ptr   class_state,
                                 a_member_decl_info_ptr  decl_info,
                                 a_boolean               compiler_generated)
/*
For a member function declaration: create a symbol entry and a routine entry
for the member function, add the symbol to the symbol table, and append the
routine entry to the routines list for the current class.  *locator gives the
source locator of the declaration.  *func_info contains function-specific
information about the function declaration.  *class_state and *decl_info track
general information about the class definition and specific information about
the member declaration, respectively.  compiler_generated is TRUE for
implicitly declared member functions.
*/
{
  a_decl_parse_state            *decl_state = &decl_info->decl_state;
  a_type_ptr                    class_type = class_state->class_type;
  a_type_ptr                    member_type = decl_state->type;
  a_symbol_ptr                  sym, overload_sym = NULL;
  a_routine_ptr                 rtn;
  a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(class_type);
  a_type_ptr                    tp;
  a_source_sequence_entry_ptr   declarator_ssep = NULL;
  a_name_linkage_kind           def_name_linkage;
  a_routine_type_supplement_ptr rtsp;
  a_symbol_ptr                  overridden_function = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_property_or_event_descr_ptr pdp = class_state->property_or_event_descr;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_enter(3, "decl_member_function");
  decl_state->is_definition = func_info->is_definition;
  rtsp = skip_typerefs(member_type)->variant.routine.extra_info;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cppcli_enabled) {
    if (decl_info->is_static_constructor) {
      check_assertion(decl_state->storage_class == (a_storage_class)sc_static);
    } else if (decl_info->is_finalizer) {
      /* Finalizers can only appear in ref class types. */
      if (!cli_class_type_kind_is(class_type, cctk_ref)) {
        pos_error(ec_finalizer_requires_reference_type,
                  &decl_state->declarator_pos);
      }  /* if */
    } else if (pdp != NULL && pdp->is_static) {
      /* The member function declaration appears as part of a static property
         declaration. */
      decl_state->storage_class = (a_storage_class)sc_static;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (decl_state->storage_class == (a_storage_class)sc_static) {
    /* A static member function. */
    /* Check if we are attempting to declare a static member function through a
       qualified function type typedef. E.g.,
         typedef void f() const; struct S { static F f(); }           */
    if (member_type->kind == (a_type_kind)tk_typeref &&
        typeref_is_typedef(member_type) &&
        (rtsp->qualifiers | rtsp->this_qualifiers) != TQ_NONE) {
      pos_error(ec_bad_qualified_function_type, &locator->source_position);
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      if (class_type->variant.class_struct_union.is_interface) {
        /* Static member functions cannot appear in interface types. */
        pos_error(ec_interface_cannot_have_static_members,
                  &decl_state->start_pos);
      }  /* if */
      if (skip_typerefs(member_type)->variant.routine.extra_info
                                    ->calling_convention ==
                                           (a_calling_convention)cc_thiscall) {
        /* Static member functions cannot have the __thiscall calling
           convention. */
        pos_error(ec_thiscall_requires_nonstatic_member,
                  &decl_state->start_pos);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  /* If this is a user-defined conversion or an overloaded operator,
     check for errors in the argument list.  Note that this is done before
     creating the symbol, since an invalid conversion or operator should not
     be added to the overload list. */
  check_operator_function_params(member_type, class_type, locator);
  /* Look for a prior declaration or function overloading. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (pdp != NULL) {
    /* Do not check for redeclarations or overloading here since any errors
       would likely be spurious.  Instead, check_property_accessor or
       check_event_accessor will report duplicates. */
    sym = enter_cli_accessor(locator, decl_scope_level, pdp);
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode && locator->is_qualified_name &&
        !is_error_locator(*locator)) {
      /* In non-managed class types, a qualified member function declarator
         indicates selective overriding in Microsoft mode, but in managed
         classes the construct is not allowed. */
      if (!is_immediate_managed_class_type(class_type)) {
        overridden_function = find_explicitly_overridden_member(
                                           locator, class_state, member_type);
      } else {
        pos_error(ec_qualified_name_not_allowed, &locator->source_position);
        set_to_error_locator(*locator);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    sym = symbol_for_member_function(locator, class_type, overridden_function,
                                     decl_info, &overload_sym);
    if (sym->variant.routine.ptr != NULL) {
      /* symbol_for_member_function has returned a symbol that has already been
         declared.  Issue an error on trying to redeclare a member function. */
      pos_sy_error(ec_member_function_redeclaration, &locator->source_position,
                   sym);
      set_to_named_error_locator(*locator);
      sym = enter_local_symbol((a_symbol_kind)sk_member_function, locator,
                               decl_scope_level,
                               /*suppress_redecl_error=*/TRUE);
    }  /* if */
  }  /* if */
  decl_info->decl_state.sym = sym;
  /* Create the routine entry for the member function. */
  /* The routine is allocated in the current memory region, as indicated
     by curr_il_region_number -- i.e., in the memory region of the scope in
     which its class is declared. */
  /* Member functions are static by default. */
  /* Pass NO_SCOPE_DEPTH for trivial default constructor so that routine entry
     will not actually be added to the IL. */
  rtn = make_routine(member_type, (a_storage_class)sc_static,
                     decl_info->is_trivial_default_constructor ?
                                  NO_SCOPE_DEPTH : decl_scope_level);
  sym->variant.routine.ptr = rtn;
  /* Set the source correspondence, including the access specifier. */
  set_source_corresp(&rtn->source_corresp, sym);
  set_class_membership(sym, &rtn->source_corresp, class_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cppcli_enabled && decl_info->is_finalizer) {
    /* ECMA-372 says that the access-specifier of a finalizer is ignored and
       that the finalizer can only be called from other members of its parent
       class. */
    rtn->source_corresp.access = (an_access_specifier)as_private;
  } else if (cppcli_enabled && decl_info->is_destructor &&
             cli_class_type_kind_is(class_type, cctk_ref)) {
    /* Similarly, ECMA-372 says that the access-specifier of a destructor for
       a ref class type is ignored.  Microsoft compilers appear to treat them
       as public members in that case. */
    rtn->source_corresp.access = (an_access_specifier)as_public;
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    rtn->source_corresp.access = class_state->access;
  }  /* if */
  if (locator->is_operator_name) {
    /* Overloaded operator function. */
    set_routine_special_kind(rtn, (a_special_function_kind)sfk_operator);
    rtn->variant.opname_kind = locator->variant.opname;
    if (locator->variant.opname == (an_opname_kind)onk_ampersand) {
      class_type->variant.class_struct_union.has_operator_ampersand = TRUE;
    }  /* if */
  } else if (locator->is_conversion_name) {
    /* User-defined conversion function. */
    set_routine_special_kind(rtn, (a_special_function_kind)sfk_conversion);
  } else if (decl_info->is_constructor) {
    set_routine_special_kind(rtn, (a_special_function_kind)sfk_constructor);
  } else if (decl_info->is_destructor) {
    set_routine_special_kind(rtn, (a_special_function_kind)sfk_destructor);
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cppcli_enabled) {
    if (decl_info->is_static_constructor) {
      /* A C++/CLI static constructor declaration. */
      set_routine_special_kind(
                        rtn, (a_special_function_kind)sfk_static_constructor);
    } else if (decl_info->is_finalizer) {
      /* A C++/CLI finalizer declaration. */
      set_routine_special_kind(rtn, (a_special_function_kind)sfk_finalizer);
    } else if (pdp != NULL) {
      /* A C++/CLI property accessor. */
      if (pdp->kind == (a_property_or_event_kind)pek_cli_property) {
        check_property_accessor(sym, decl_info, class_state);
      } else if (pdp->kind == (a_property_or_event_kind)pek_cli_event) {
        check_event_accessor(sym, decl_info, class_state);
      } else {
        unexpected_condition();
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  check_defaulted_or_deleted_function(&decl_info->decl_state, func_info,
                                      &locator->source_position);
  if (func_info->is_inline) {
    /* Inline member function (either because "inline" was specified or
       a function definition is present). */
    set_inline_flag(rtn, TRUE);
  }  /* if */
  if (compiler_generated) {
    rtn->compiler_generated = TRUE;
#if SUN_EXTENSIONS_ALLOWED
    if (sun_linker_scope_allowed) {
      /* A linker scope specifier on a class type is applied to all the
         implicit members of that class type. */
      a_class_type_supplement_ptr  ctsp = class_type_supp(class_type);
      rtn->decl_modifiers |= (ctsp->decl_modifiers & DM_ANY_SUN_LINK_SCOPE);
    }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cppcli_enabled) {
    if (cli_class_type_kind_is(class_type, cctk_value)) {
      /* Issue an error for a default or copy constructor, a destructor, or an
         assignment operator. */
      exclude_special_members_from_value_class_type(rtn, class_type,
                                                    &locator->source_position);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  /* The routine name linkage on the function type is also required to be
     C++ no matter what the name linkage of the routine turns out to be. */
  rtsp->routine_name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
  /* Member functions should have the same name linkage as the class of
     which they are members.  (In cfront mode that may mean internal
     linkage -- if and when its linkage is promoted to C++, the linkage of
     the member functions will also be changed. */
  def_name_linkage = class_type->source_corresp.name_linkage;
  if (def_name_linkage == (a_name_linkage_kind)nlk_none ||
      def_name_linkage == (a_name_linkage_kind)nlk_internal) {
    /* Either this is a local class (nlk_none) or a cfront-compatible
       declaration (nlk_internal). */
    rtn->source_corresp.name_linkage = def_name_linkage;
    /* storage_class is already set to sc_static. */
  } else if (func_info->is_inline && !extern_inline_allowed) {
    rtn->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
    /* storage_class is already set to sc_static. */
  } else {
    /* Except for special cases, class member functions have C++ linkage
       whatever the default name linkage may be.  That is, member functions
       of a class have C++ name linkage even if the class definition is
       wrapped in (for example) an extern "C" declaration. */
    rtn->source_corresp.name_linkage =
                               (a_name_linkage_kind)nlk_cplusplus_external;
    /* The storage class will be changed to sc_unspecified if a definition is
       seen. */
    rtn->storage_class = (a_storage_class)sc_extern;
    /* Check whether any types without linkage are used in the declaration. */
    check_constituent_types_have_linkage(sym, &locator->source_position,
                                         /*is_declaration=*/TRUE);
  }  /* if */
#if BACK_END_IS_CP_GEN_BE
  if (func_info->is_definition &&
      scope_stack[decl_scope_level].default_name_linkage ==
                                           (a_name_linkage_kind)nlk_external) {
    /* If this member function is moved outside its class, it must be
       enclosed in an extern "C" block. */
    rtn->definition_C_name_linkage_specified = TRUE;
  }  /* if */
#endif /* BACK_END_IS_CP_GEN_BE */
  if (!is_error_locator(*locator)) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      /* If this function declaration specifies selective overrides, record
         that fact.  (Managed and non-managed classes use different syntax to
         select the overridden base class member.) */
      record_selective_overriding(decl_info, overridden_function);
      if (decl_state->ms_attributes != NULL) {
        apply_microsoft_attributes(&decl_state->ms_attributes, (char*)rtn,
                                   (an_il_entry_kind)iek_routine, MSAT_METHOD);
      }  /* if */
      if (microsoft_version >= 1400 || cppcli_enabled) {
        /* Record any function modifiers (they can only appear in the class-
           scope declaration). */
        rtn->final = func_info->sealed;
#if BACK_END_IS_CP_GEN_BE
        rtn->abstract = func_info->abstract;
        rtn->override = func_info->override;
        rtn->new_member = func_info->new_member;
#endif /* BACK_END_IS_CP_GEN_BE */
      }  /* if */
    }  /* if */
  } else if (decl_state->ms_attributes != NULL) {
    /* We indicate that the attributes have been consumed by clearing the
       caller's attribute pointer. */
    decl_state->ms_attributes = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (gpp_mode) {
    /* Propagate any class attributes that also apply to its member
       functions. */
    copy_gnu_class_properties_to_routine(class_type, rtn);
    /* Record the assembly name. */
    if (decl_state->asm_name != NULL) {
      rtn->asm_name = decl_state->asm_name;
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (!compiler_generated) {
    a_symbol_reference_kind  srk_flags = SRK_DECLARATION;
    if (func_info->is_definition) srk_flags |= SRK_DEFINITION;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    declarator_ssep = func_info->declarator_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    record_symbol_declaration(srk_flags, sym, &locator->source_position,
                              declarator_ssep);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    update_decl_pos_info(&rtn->source_corresp, &decl_info->decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (func_info->is_definition) {
      if (record_name_references_in_context()) {
        /* A definition is always the primary declaration.  Record the form of
           the associated declarator. */
        a_name_reference_ptr
          name_ref = qualifiable_name_reference(locator, &rtn->source_corresp);
        if (name_ref != NULL) {
          name_ref->used_in_primary_declarator = TRUE;
        }  /* if */
      }  /* if */
      /* For a definition enter the function type as the "declared_type" in
         the routine entry itself. Avoid adding a redundant type to the IL
         if possible. */
      set_routine_declared_type(rtn, func_info->declared_type);
      /* For default arg processing later on, save the type that's used as
         the declared type. */
      func_info->declared_type = rtn->declared_type;
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
      /* In most cases, this inline member function definition will be
         represented in the source-sequence list as a non-defining
         declaration, and the source-sequence entry for its definition
         will be put out after the class definition is terminated.
         This is to solve a problem in generated C++ when function
         template instantiations are represented as explicit specializations
         and where, at the point of instantiation, the class is required to
         be complete.  In some cases (such as for members of local or
         unnamed classes), the transformation cannot be performed because
         no valid (or equivalent) out-of-class syntax is available. */
      if (!class_type->source_corresp.is_local_to_function &&
          !rtn->is_defaulted && !rtn->is_deleted &&
#if MICROSOFT_EXTENSIONS_ALLOWED
          !(microsoft_mode &&
            microsoft_routine_def_is_unmovable(overridden_function != NULL)) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          class_type_can_be_named_in_namespace_scope(class_type)) {
        func_info->is_movable_member_or_friend_def = TRUE;
      }  /* if */
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    }  /* if */
    if (!func_info->is_definition
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
        || func_info->is_movable_member_or_friend_def
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
                                                     ) {
      /* A non-defining entry is represented by a secondary-decl entry in the
         source sequence list. */
      an_sssd_flag_set      sssd_flags;
      a_name_reference_ptr  name_ref = NULL;
      if (record_name_references_in_context()) {
        name_ref = qualifiable_name_reference(locator, &rtn->source_corresp);
      }  /* if */
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
      if (func_info->is_movable_member_or_friend_def) {
        /* Set the flag that indicates the definition appears outside the
           class body -- it's used by the code that eliminates unneeded
           function declarations, if this routine turns out to be
           unreferenced. */
        rtn->defined_outside_of_parent = TRUE;
        /* A given default argument expression cannot appear both on a
           secondary source-sequence entry and on the primary declaration. */
        /* Remove default arguments, if any, from the type recorded as the
           declared type on the primary declaration.  This means, in the
           source-sequence representation the default arguments will be on
           the declaration that appears inside the class definition and not
           on the definition that appears outside the class definition.  For
           example, it has the effect of making the following transformation
           in the output of the C++-generating back end:
             class A {
               void f(int = 0) { }
             };
           becomes
             class A {
               void f(int = 0);
             };
             void A::f(int) { }
        */
        if (rtn->declared_type != rtn->type) {
          /* There must be default arguments.  Make a new type entry. */
          tp = copy_routine_type_with_param_types(rtn->declared_type,
                                                  /*copy_default_args=*/FALSE);
          /* For the default arg fixup later on, reset the pointer to the
             declared type that needs to be updated.  (Note that it will be
             associated with the source-sequence secondary decl entry, not
             the routine.) */
          func_info->declared_type = tp;
        } else {
          /* No need to create a new type entry. */
          tp = rtn->declared_type;
        }  /* if */
        /* Note: The aforementioned transformation is often better than
           associating the default argument with the out-of-class definition
           (e.g., when the current function is a default constructor or when
           it is called in the body of a member function already defined
           within the current class).  However, if the transformation should
           (sometimes or always) produce this:
             class A {
               void f(int);
             };
             void A::f(int = 0) { }
           then func_info->declared_type should be left pointing at
           rtn->declared_type; that way default-arg fixup will not affect the
           declared-type on the source sequence secondary-decl entry. */
      } else
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
      /* Do not insert code here. */
      {
        /* Normal declaration.  If necessary, update the declared type,
           which was saved during function declarator processing, to make
           it consistent with the routine type. */
        func_info->declared_type = update_routine_declared_type(
                                       member_type, func_info->declared_type);
        tp = func_info->declared_type;
      }  /* if */          
      /* Update the secondary-declaration entry.  A member function
         declaration within a class definition is always the initial
         declaration. */
      sssd_flags = SSSD_FIRST_DECLARATION;
#if GNU_EXTENSIONS_ALLOWED
      if (decl_state->marked_as_gnu_extension) {
        sssd_flags |= SSSD_MARKED_AS_GNU_EXTENSION;
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
      if (!update_src_seq_secondary_decl((char *)rtn, tp, name_ref, sssd_flags,
                                         &decl_info->decl_pos_block)) {
        /* No source-sequence secondary declaration entity was found, which
           means the declared type will not be needed.  Clear the pointer
           to suppress copying the default arg expression to it later on. */
        func_info->declared_type = NULL;
      }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    } else {
      /* An in-class definition (that will not be moved out-of-class). */
      /* Record whether the declaration was preceded by __extension__. */
      rtn->source_corresp.marked_as_gnu_extension =
                                          decl_state->marked_as_gnu_extension;
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
    wrapup_sse_for_simple_decl(decl_state);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (func_info->is_definition) {
      /* Since this is a definition, record the current lint argsused and
         varargs-count state in the routine type. That will suppress any
         warnings about unused parameters or variable arguments. */
      record_lint_argsused_and_varargs_state(sym);
    }  /* if */
    /* Do processing required for any pragmas that are bound to the current
       declaration. */
    process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  }  /* if */
  if (overload_sym != NULL) {
    check_for_conflicts_with_using_decls(overload_sym,
                                         &locator->source_position);
    set_mixed_static_nonstatic_flag(overload_sym);
  }  /* if */
  if (class_type->variant.class_struct_union.is_nonreal_class) {
    /* This symbol represents a member function of a prototype instantiation
       of a class template.  As such it is a quasi function template itself.
       Set it up to look like that.  Microsoft/Sun in-class specializations
       that appear in class templates are prototype instantiations, but their
       members should not be considered templates. */
    if (class_type->variant.class_struct_union.is_in_class_specialization) {
      rtn->is_prototype_instantiation = TRUE;
    } else {
      a_template_instance_ptr           tip;
      a_template_symbol_supplement_ptr  tssp;

      sym->variant.routine.instance_ptr = tip = alloc_template_instance();
      tip->instance_sym = tip->template_sym = sym;
      tip->template_info = tssp = alloc_template_symbol_supplement(sym->kind);
      tssp->variant.function.routine = rtn;
      tssp->variant.function.func_info = *func_info;
      tssp->is_variadic = scope_stack_top().in_variadic_template;
      rtn->is_prototype_instantiation = TRUE;
      rtn->is_template_function = TRUE;
      tip->prototype_scope_symbols = func_info->prototype_scope_symbols;
      if (!decl_info->is_trivial_default_constructor) {
      /* Although it is not a template, it is an instantiatable function
         and hence we create a placeholder a_template entry for it.  (Trivial
         default constructors are not linked in the IL and hence do no need
         that information.) */
        a_template_ptr  templ = alloc_template();
        templ->kind = (a_template_kind)templk_member_function;
        set_source_corresp(&templ->source_corresp, sym);
        set_class_membership((a_symbol_ptr)NULL, &templ->source_corresp,
                              class_type);
        templ->source_corresp.name_linkage =
                                   (a_name_linkage_kind)nlk_cplusplus_external;
        /* Update the IL template pointer in the template symbol supplement. */
        tssp->il_template_entry = templ;
        templ->source_corresp.access = class_state->access;
        /* A member function of a class template is exported if the enclosing
           class is declared as exported and the function is not inline. */
        templ->is_exported = class_is_exported(class_type) &&
                             !func_info->is_inline;
        add_to_templates_list(templ, decl_scope_level);
        if (prototype_instantiations_in_il) {
          templ->prototype_instantiation.routine = rtn;
        }  /* if */
        templ->canonical_template = templ;
        if (func_info->is_definition) {
          templ->definition_template = templ;
        }  /* if */
        rtn->assoc_template = templ;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!is_error_locator(*locator)) {
    /* Do processing for special member functions, including assignment
       operators, constructors and destructors. */
    if (locator->is_operator_name) {
      /* If this is an assignment operator, record a pointer to it in the
         symbol -- to facilitate generating default assignment operators. */
      if (rtn->variant.opname_kind == (an_opname_kind)onk_assign) {
        record_assignment_operator_in_class_symbol(cssp, sym, overload_sym);
      } else if (rtn->variant.opname_kind == (an_opname_kind)onk_new) {
        cssp->has_operator_new = TRUE;
      } else if (rtn->variant.opname_kind == (an_opname_kind)onk_array_new) {
        cssp->has_operator_array_new = TRUE;
      } else if (rtn->variant.opname_kind == (an_opname_kind)onk_delete) {
        cssp->has_operator_delete = TRUE;
      } else if (rtn->variant.opname_kind == 
                                            (an_opname_kind)onk_array_delete) {
        cssp->has_operator_array_delete = TRUE;
      }  /* if */
    } else if (locator->is_conversion_name) {
      /* User-defined conversion function. */
      if (!is_implicitly_callable_conversion_function(rtn->type)) {
        /* Conversion to void or to the same type or a reference to the same
           type or to a base class or a reference to a base class "is never
           used" (WP 12.3.2; that is, it is not used in implicit or explicit
           conversions but only in an explicit invocations of the function). */
        pos_sy_warning(ec_conversion_function_not_usable,
                       &locator->source_position, sym);
      } else {
        /* Create a conversion list entry.  This list provides an alternative
           to traversing the entire symbols list for a class to find its
           conversion functions. */
        add_to_conversion_list(sym, cssp);
        tp = f_skip_typerefs(return_type_of(rtn->type));
        if (is_immediate_class_type(tp)) {
          /* The target type of the conversion is a class or ref-to-class
             type: set a flag to mark it as target of a conversion. */
          set_target_of_conversion_function_flag(tp);
        }  /* if */
      }  /* if */
    }  /* if */
    if (exceptions_enabled && compiler_generated &&
#if MICROSOFT_EXTENSIONS_ALLOWED
        !is_immediate_managed_class_type(class_type) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        !class_type->variant.class_struct_union.is_nonreal_class) {
      /* A compiler generated constructor, destructor, or assignment
         operator is assumed to throw any exception that can be thrown by
         any base-class function it will call. */
      form_exception_specification_for_generated_function(rtn);
    }  /* if */
    if (!sym->is_error) {
      /* If "virtual" was specified in the declaration, mark the routine as
         virtual.  Even if it wasn't, its virtualness can be inherited.  In
         either case record the relationship between the current routine and
         its appearance in the base classes of the current class. */
      a_boolean  is_virtual = ((decl_state->dso_flags & DSO_VIRTUAL) &&
                               !decl_info->invalid_virtual_specifier);
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode) {
        if (decl_state->storage_class == (a_storage_class)sc_static) {
          /* An interface member explicitly declared static. */
          is_virtual = FALSE;
        } else if (check_virtual_interface_member(class_state, rtn, locator)) {
          /* This member is implicitly pure virtual by virtue of being
             declared in an interface class type.  Interfaces with virtual
             members cannot be PODs (in particular, they need generated
             copy-constructors to set virtual function table pointers.) */
          make_virtual_function_pure(rtn, class_type);
          is_virtual = TRUE;
          class_state->POD_ruled_out = TRUE;
        }  /* if */
        if (pdp != NULL && pdp->is_virtual) is_virtual = TRUE;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (rtn->compiler_generated && rtn->is_prototype_instantiation) {
        /* Compiler-generated members of prototype instantiation cannot
           always be matched to potentially overridden member functions
           because of insufficient type information.  To avoid spurious
           errors, we do not call check_for_virtual_function in such
           cases. */
      } else if (check_for_virtual_function(is_virtual, decl_info,
                                            class_state, func_info)) {
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (cli_class_type_kind_is(class_type, cctk_value)) {
          /* C++/CLI value classes are an exception to the rules implemented
             below: They're trivially constructible/copyable even when they
             have virtual member functions. */
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          /* Classes with virtual functions require nontrivial default
             constructors. */
          class_state->default_ctor_is_nontrivial = TRUE;
          /* Classes with virtual functions cannot be constructed or assigned
             by bitwise copying. */
          cssp->construction_by_bitwise_copy_allowed = FALSE;
          cssp->assignment_by_bitwise_copy_allowed = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (!special_kind_is(rtn, sfk_none)) {
      update_class_for_special_member(class_state, decl_info, overload_sym);
    }  /* if */
#if BACK_END_IS_CP_GEN_BE
    /* Set the "name linkage environment" for this routine. */
    rtn->surrounding_name_linkage_state =
                          scope_stack[depth_scope_stack].default_name_linkage;
#endif /* BACK_END_IS_CP_GEN_BE */
    attach_decl_attributes(decl_state,
                           /*is_primary_decl=*/func_info->is_definition);
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* If decl-modifiers were declared for the class and/or for the member,
       check for consistency and use the union of the two. */
    if (microsoft_mode) {
      if (decl_state->prefix_attributes != NULL) {
        add_flags_from_dll_attributes(&decl_state->decl_modifiers.flags,
                                      decl_state->prefix_attributes);
      }  /* if */
      merge_dll_flags_from_parent_class(class_type, decl_state);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    update_routine_decl_modifiers(rtn, &decl_state->decl_modifiers,
                                  &locator->source_position,
                                  /*is_redecl=*/FALSE,
                                  (a_boolean)func_info->is_definition,
                                  (a_boolean)func_info->is_inline);
    if (!compiler_generated) {
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
      if (!rtn->source_corresp.is_deprecated) {
        /* Check if a deprecated type was involved in this declaration. */
        warn_about_use_of_deprecated_type(member_type,
                                          &locator->source_position);
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) db_symbol(sym, "", 4);
#endif /* DEBUG */
  treat_declaration_as_okay_in_property_or_event(class_state);
  db_exit();
}  /* decl_member_function */


static void decl_call_operator_for_lambda(a_lambda_ptr        lambda,
                                          a_class_def_state   *class_state,
                                          a_member_decl_info  *decl_info,
                                          a_func_info_block   *func_info)
/*
Create operator()(...) for the given lambda (except in some error cases).
*decl_info and *func_info describe various properties about the construct that
was parsed.  *class_state describes the synthesized "closure class" associated
with the lambda.
The heavy lifting for this routine is performed by decl_member_function.
*/
{
  a_decl_parse_state  *dps = &decl_info->decl_state;

  if (!is_error_type(dps->type)) {
    a_symbol_locator    loc;
    a_routine_ptr       rp;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    a_boolean           prev_source_sequence_entries_disallowed
                                         = source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    check_assertion(is_function_type(dps->type));
    func_info->is_inline = TRUE;
    func_info->is_definition = TRUE;
    make_opname_locator((an_opname_kind)onk_function_call, &loc,
                        &decl_info->decl_state.declarator_pos);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    decl_member_function(&loc, func_info, class_state, decl_info,
                         /*compiler_generated=*/FALSE);
    /* Ordinarily, the "symbols" field of a class symbol supplement isn't
       updated until the class definition is completed.  However, the mangling
       rules for lambdas are such that this is sometimes needed earlier for
       closure types.  So we set it now (since it's the symbol for the
       implied call operator that is needed). */
    symbol_supplement_for_class(class_state->class_type)->symbols =
          assoc_pointers_block_of(&scope_stack[depth_scope_stack])->symbols;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    source_sequence_entries_disallowed =
                                      prev_source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    rp = decl_info->decl_state.sym->variant.routine.ptr;
    lambda->lambda_routine = rp;
    rp->is_lambda_body = TRUE;
    rp->type->variant.routine.extra_info->assoc_routine = rp;
    rp->is_prototype_instantiation =
                     scope_stack[depth_scope_stack].in_prototype_instantiation;
  }  /* if */
}  /* decl_call_operator_for_lambda */


static void decl_member_function_template(
				a_symbol_locator        *locator,
				a_template_param_ptr	templ_param_list,
                                a_func_info_block       *func_info,
                                a_class_def_state_ptr   class_state,
                                a_member_decl_info_ptr  decl_info)
/*
Process the declaration of a member function template.  *locator is the
symbol locator of the template.  templ_param_list is the template parameter
list of the function template.  *func_info contains information gathered in
processing the declarator.  *class_state and *decl_info track general
information about the class definition and specific information about the
member declaration, respectively.  (This function is similar to
decl_function_template, which handles non-member function templates and
out-of-class template declarations of functions that are members of template
classes, and to decl_member_function, which handles in-class member function
declarations.)
*/
{
  a_decl_parse_state                *dps = &decl_info->decl_state;
  a_type_ptr                        class_type = class_state->class_type;
  a_type_ptr                        member_type = dps->type;
  a_template_symbol_supplement_ptr  tssp;
  a_routine_ptr                     rtn;
  a_symbol_ptr                      sym = NULL;
  a_symbol_ptr                      prototype_sym;
  a_symbol_ptr                      other_sym, overload_sym = NULL;
  a_class_symbol_supplement_ptr     cssp;
  a_scope_depth                     effective_decl_level;

  db_enter(3, "decl_member_function_template");
  dps->is_definition = func_info->is_definition;
  if (!is_error_locator(*locator)) {
    if (is_single_param_operator_new_or_delete(locator, member_type)) {
      /* Overloading should not be allowed on the single-argument version
         of operator new(size_t) or delete(void *). */
      pos_error(is_new_operator(locator->variant.opname) ?
                    ec_template_operator_new : ec_template_operator_delete,
                &locator->source_position);
      set_to_named_error_locator(*locator);
    }  /* if */
  }  /* if */
  check_operator_function_params(member_type, class_type, locator);
  if (!is_error_locator(*locator)) {
    sym = find_direct_member_function(locator, class_type);
    if (sym != NULL) {
      /* Be sure the declaration does not conflict with a previous member
         function template declaration in the current class. */
      a_boolean  is_list =
                      (sym->kind == (a_symbol_kind)sk_overloaded_function);
      other_sym = is_list ? sym->variant.overloaded_function.symbols : sym;
      for (; other_sym != NULL; other_sym = is_list ? other_sym->next : NULL) {
        a_symbol_ptr  fund_sym = other_sym;
        /* Ignore projections not resulting from a using-declaration. */
        if (fund_sym->kind == (a_symbol_kind)sk_projection) {
          if (is_class_member_using_decl_symbol(fund_sym)) {
            reduce_projection_symbol_to_fundamental_symbol(fund_sym);
          } else {
            continue;
          }  /* if */
        }  /* if */
        if (fund_sym->kind == (a_symbol_kind)sk_function_template) {
          /* Issue an error if the other member function template declaration
             has a type compatible with this one -- compare the routine
             types. */
          a_template_param_ptr			other_templ_param_list;
          a_template_symbol_supplement_ptr	other_tssp;
          a_type_ptr				tp;
          other_tssp = template_supplement_for_symbol(fund_sym);
          tp = other_tssp->variant.function.routine->type;
          other_templ_param_list =
                 other_tssp->variant.function.decl_cache.decl_info->parameters;
          if (equiv_template_param_lists(other_templ_param_list,
                                         templ_param_list,
                                         /*issue_errors=*/FALSE,
                                         ETP_NO_OPTIONS,
                                         (a_source_position*)NULL, es_error) &&
              param_types_are_compatible(tp, member_type, TCF_NO_FLAGS)) {
            an_error_code  error_code = ec_no_error;
            if (other_sym != fund_sym) {
              /* We found a matching using-declaration: Remove it from the
                 symbol table. */
              remove_member_using_decl(&other_sym, &sym);
              fund_sym = NULL;
            } else if (routine_type_is_nonstatic_member_function(tp) !=
                  routine_type_is_nonstatic_member_function(member_type)) {
              error_code = ec_static_nonstatic_with_same_param_types;
              pos_error(error_code, &locator->source_position);
            } else if (routine_types_are_redecl_compatible(tp, member_type,
                                                           TCF_NO_FLAGS)) {
              error_code = ec_member_function_redeclaration;
              pos_sy_error(error_code, &locator->source_position, other_sym);
            }  /* if */
            if (error_code != ec_no_error) {
              set_to_named_error_locator(*locator);
              sym = NULL;
            }  /* if */
            break;
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  /* Set a flag in each param type entry whose associated type is or
     contains a template parameter. */
  set_parameter_list_template_param_flags(member_type);
  /* Create the new symbol and enter it into the symbol table. */
  effective_decl_level = class_type->variant.class_struct_union.extra_info->
                                           assoc_scope->depth_in_scope_stack;
  check_assertion(effective_decl_level != NO_SCOPE_DEPTH);
  if (sym == NULL) {
    sym = enter_local_symbol((a_symbol_kind)sk_function_template, locator,
                             effective_decl_level,
                             /*suppress_redecl_error=*/FALSE);
  } else {
    a_boolean  is_ctor = decl_info->is_constructor;

    /* Enter this symbol as an instance of overloading. */
    sym = enter_overloaded_symbol((a_symbol_kind)sk_function_template,
                                  locator, is_ctor, sym, &overload_sym);
  }  /* if */
  dps->sym = sym;
  rtn = make_routine(member_type, (a_storage_class)sc_unspecified,
                     prototype_instantiations_in_il && !sym->is_error
                                     ? effective_decl_level : NO_SCOPE_DEPTH);
  tssp = template_supplement_for_symbol(sym);
  tssp->variant.function.routine = rtn;
  /* Copy the func_info block and then null out its param-id pointer so that
     it won't be freed. */
  tssp->variant.function.func_info = *func_info;
  func_info->param_id_list = NULL;
  /* Allocate the symbol for the prototype instantiation of the
     function template. */
  prototype_sym = make_function_template_prototype_symbol(
                                                   sym, rtn, templ_param_list);
  /* Set the source correspondence, including the access specifier. */
  set_source_corresp(&rtn->source_corresp, prototype_sym);
  set_class_membership(sym, &rtn->source_corresp, class_type);
  set_class_membership(prototype_sym, (a_source_correspondence*)NULL,
                       class_type);
  rtn->source_corresp.access = class_state->access;
  if (func_info->is_inline) {
    /* Inline member function (either because "inline" was specified or
       a function definition is present). */
    set_inline_flag(rtn, TRUE);
  }  /* if */
  if (func_info->is_inline && !extern_inline_allowed) {
    rtn->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
    rtn->storage_class = (a_storage_class)sc_static;
  } else {
    /* Member functions should have the same name linkage as the class of
       which they are members. */
    rtn->source_corresp.name_linkage = class_type->source_corresp.name_linkage;
    rtn->storage_class = (a_storage_class)sc_extern;
  }  /* if */
  if (prototype_instantiations_in_il && !sym->is_error) {
    add_to_routines_list(rtn, NO_SCOPE_DEPTH);
  }  /* if */
  if (!is_error_locator(*locator)) {
#if EXTRA_SOURCE_POSITIONS_IN_IL
    update_decl_pos_info(&rtn->source_corresp, &decl_info->decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
  if (locator->is_operator_name) {
    /* Overloaded operator function. */
    set_routine_special_kind(rtn, (a_special_function_kind)sfk_operator);
    rtn->variant.opname_kind = locator->variant.opname;
  } else if (locator->is_conversion_name) {
    /* User-defined conversion function. */
    set_routine_special_kind(rtn, (a_special_function_kind)sfk_conversion);
  }  /* if */
  check_defaulted_or_deleted_function(dps, func_info, &pos_curr_token);
  if (overload_sym != NULL) {
    set_mixed_static_nonstatic_flag(overload_sym);
  }  /* if */
  cssp = symbol_supplement_for_class(class_type);
  if (!is_error_locator(*locator)) {
    /* Do processing for special member functions, including assignment
       operators, constructors and destructors. */
    if (locator->is_operator_name) {
      /* If this is an assignment operator, record a pointer to it in the
         symbol -- to facilitate generating default assignment operators. */
      switch (rtn->variant.opname_kind) {
        case onk_assign:
          record_assignment_operator_in_class_symbol(cssp, sym, overload_sym);
          break;
        case onk_new:
          cssp->has_operator_new = TRUE;
          break;
        case onk_array_new:
          cssp->has_operator_array_new = TRUE;
          break;
        case onk_delete:
          cssp->has_operator_delete = TRUE;
          break;
        case onk_array_delete:
          cssp->has_operator_array_delete = TRUE;
          break;
        default:;
      }  /* switch */
    } else if (locator->is_conversion_name) {
      /* Create a conversion list entry.  This list provides an alternative
         to traversing the entire symbols list for a class to find its
         conversion functions. */
      add_to_conversion_list(sym, cssp);
    }  /* if */
    if (decl_info->is_constructor) {
      set_routine_special_kind(rtn, (a_special_function_kind)sfk_constructor);
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
    }  /* if */
    attach_decl_attributes(dps, (a_boolean)func_info->is_definition);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      if (dps->prefix_attributes != NULL) {
        add_flags_from_dll_attributes(&dps->decl_modifiers.flags,
                                      dps->prefix_attributes);
      }  /* if */
    }  /* if */
    if (cppcli_enabled && decl_info->is_static_constructor) {
      /* A static constructor member template is invalid. */
      pos_error(ec_static_constructor_member_template,
                &locator->source_position);
      /* For error recovery purposes, treat the prototype instance as a static
         constructor entry. */
      set_routine_special_kind(
                        rtn, (a_special_function_kind)sfk_static_constructor);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    update_routine_decl_modifiers(rtn, &dps->decl_modifiers,
                                  &locator->source_position,
                                  /*is_redecl=*/FALSE,
                                  (a_boolean)func_info->is_definition,
                                  (a_boolean)func_info->is_inline);
#if GNU_EXTENSIONS_ALLOWED
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    if (rtn->ELF_visibility == (an_ELF_visibility_kind)evk_unspecified) {
      /* If no ELF visibility attribute was specified on the member template
         itself, adopt any visibility that might have been specified for the
         enclosing class. */
      rtn->ELF_visibility = class_type_supp(class_type)->ELF_visibility;
    }  /* if */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  dps->sym = sym;
  db_exit();
}  /* decl_member_function_template */


static void scan_pure_specifier(a_symbol_ptr            rout_sym,
                                a_type_ptr              class_type,
                                a_member_decl_info_ptr  decl_info,
                                a_func_info_block_ptr   func_info)
/*
The current token is an "=", encountered just after the scanning of a
member or friend function declarator.  A pure specifier is defined as "= 0",
and it is legal for virtual member functions only.  rout_sym represents the
function being declared (in the definition of class_type).  decl_info and
func_info describe the current member function declaration.
*/
{
  a_routine_ptr  rout;
  a_boolean      pure_specifier_allowed, pure_specifier_ignored = FALSE;

  db_enter(4, "scan_pure_specifier");
  /* A pure specifier is allowed for virtual functions only.  (Check the
     parent class to exclude friend declarations.) */
  if (!rout_sym->is_class_member ||
      sym_parent_class(rout_sym) != class_type) {
    rout = NULL;
    pure_specifier_allowed = FALSE;
  } else {
    rout = (rout_sym->kind == (a_symbol_kind)sk_function_template) ?
                   rout_sym->variant.template_info->variant.function.routine :
                   rout_sym->variant.routine.ptr;
    pure_specifier_allowed = rout->is_virtual;
    if (!pure_specifier_allowed &&
        class_type->variant.class_struct_union.is_prototype_instantiation) {
      /* If class_type has a template-dependent base, the routine might be an
         overrider of a virtual function in that base, which means the routine
         would be virtual too.  In such cases we must also allow the pure
         specifier.  GNU, Microsoft, and Sun always allow the pure specifier
         in class templates. */
      if (symbol_supplement_for_class(class_type)
                                               ->any_dependent_base_classes) {
        pure_specifier_allowed = TRUE;
      } else if (gpp_mode || microsoft_mode || sun_mode) {
        pure_specifier_allowed = TRUE;
        pure_specifier_ignored = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!pure_specifier_allowed && !decl_info->invalid_virtual_specifier) {
    pos_error(ec_pure_specifier_on_nonvirtual_function, &pos_curr_token);
  } else if (pure_specifier_allowed &&
             (rout->final || class_type->variant.class_struct_union.final)) {
    /* Making a pure virtual member sealed/final is useless, but while
       Microsoft makes the "sealed" case an error, the C++0x standard does not
       prohibit the "[[final]]" case. */
    an_attribute_ptr  final_ap =
                    find_attribute(ak_final, rout->source_corresp.attributes);
    if (!rout->final) {
      check_assertion(class_type->variant.class_struct_union.final);
      pos_warning(ec_pure_final_virtual, &pos_curr_token);
    } else if (final_ap == NULL) {
      /* The routine was declared with "sealed". */
      check_assertion(microsoft_mode);
      pos_error(ec_pure_specifier_on_sealed_member, &pos_curr_token);
      pure_specifier_allowed = FALSE;
    } else {
      /* The routine was declared with "final". */
      pos_warning(ec_pure_final_virtual, &final_ap->position);
    }  /* if */
  }  /* if */
  /* Advance past the "=". */
  (void)get_token();
  if ((curr_token == tok_int_constant &&
       (const_for_curr_token.is_simple_zero ||
        ((microsoft_mode || gpp_mode) &&
         is_zero_constant(&const_for_curr_token)))) ||
      (gpp_mode && gnu_version < 30400 && curr_token == tok_null)) {
    /* Token following "=" should be "0".  Note that we normally don't test
       for an integer value of zero but rather for the literal "0", since
       "= 00" should elicit an error.  In Microsoft and early GNU modes,
       however, other forms of "zero" are accepted (including "__null" in GNU
       mode). */
    if (pure_specifier_allowed && !pure_specifier_ignored) {
      /* Update the routine and class type entities. */
      make_virtual_function_pure(rout_sym->variant.routine.ptr, class_type);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      {
      /* Include the pure-specifier in the declarator range.  Note that
         the routine entry is updated directly, since decl_member_function
         has already been called at this point. */
      a_decl_position_supplement_ptr  dpsp = rout_sym->variant.routine.ptr->
                                                 source_corresp.decl_pos_info;
      if (dpsp != NULL && dpsp->variant.declarator_range.start.seq != 0) {
        dpsp->variant.declarator_range.end = pos_curr_token;
      }  /* if */
      }
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    }  /* if */
    /* Advance past the "0". */
    (void)get_token();
    /* The C++ standard's grammar does not allow a function body to follow a
       pure-specifier.  Microsoft compilers, however, accept such
       constructs. */
    if (func_info->is_definition) {
      pos_diagnostic(microsoft_mode ? es_warning : es_discretionary_error,
                     ec_pure_virtual_definition, &pos_curr_token);
    } else {
      check_assertion_or_expect_error(curr_token == tok_semicolon ||
                                      curr_token == tok_comma);
    }  /* if */
  } else {
    set_err_pos_to_curr_token();
    /* Invalid pure specifier:  something other than "0" follows the "=". */
    syntax_error(ec_bad_pure_specifier);
  }  /* if */
  db_exit();
}  /* scan_pure_specifier */


static void decl_nonstd_member_constant(a_symbol_locator        *locator,
                                        a_class_def_state_ptr   class_state,
                                        a_member_decl_info_ptr  decl_info)
/*
Do processing for a nonstandard member constant, including scanning the
initializer constant and entering the name in the symbol table.  The member
type is guaranteed to be a const-qualified scalar type.  This construct is an
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
    static const int l = 10;       // standard form of member constant,
                                   //   not handled here
  };

*class_state and *decl_info track general information about the class
definition and specific information about the member declaration,
respectively.
*/
{
  a_type_ptr       class_type = class_state->class_type;
  a_type_ptr       member_type = decl_info->decl_state.type;
  a_symbol_ptr     sym;
  a_constant_ptr   cp;

  db_enter(3, "decl_nonstd_member_constant");
  /* The current token is the "=".  Pointing to it issue a diagnostic that this
     is a nonstandard construct.  This is a strict ANSI diagnostic in
     strict ANSI mode, otherwise it is a warning. */
  pos_diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
                 ec_nonstd_const_member, &pos_curr_token);
  /* Advance past the "=". */
  (void)get_token();
  /* Scan the constant expression. */
  cp = alloc_constant((a_constant_repr_kind)ck_error);
  scan_constant_initializer_expression(member_type,
                                       &decl_info->decl_state,
                                       cp);
  /* Enter the constant name in the symbol table.  Do this after scanning
     the expression to avoid problems with a recursive reference, though
     it may mean the order in which errors are issued is a little strange. */
  sym = enter_local_symbol((a_symbol_kind)sk_constant, locator,
                           decl_scope_level, /*suppress_redecl_error=*/FALSE);
  /* Update the symbol and the constant entry. */
  sym->variant.constant = cp;
  set_source_corresp(&(cp->source_corresp), sym);
  set_class_membership(sym, &cp->source_corresp, class_type);
  decl_info->decl_state.sym = sym;
  cp->source_corresp.access = class_state->access;
  record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, sym,
                            &locator->source_position,
                            decl_info->decl_state.source_sequence_entry);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  update_decl_pos_info(&cp->source_corresp, &decl_info->decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Do processing required for any pragmas that are bound to the current
     declaration. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  add_to_constants_list(cp, /*at_file_scope=*/FALSE);
  db_exit();
}  /* decl_nonstd_member_constant */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void decl_literal_field(a_symbol_locator        *locator,
                               a_class_def_state_ptr   class_state,
                               a_member_decl_info_ptr  decl_info)
/*
Do processing for a C++/CLI literal field, including scanning the initializer
constant and entering the name in the symbol table.
*/
{
  a_type_ptr          class_type = class_state->class_type;
  a_decl_parse_state  *dps = &decl_info->decl_state;
  a_type_ptr          member_type = dps->type;

  db_enter(3, "decl_literal_field");
  if (cli_class_type_kind_is(class_type, cctk_standard)) {
    pos_error(ec_literal_requires_managed_class, &dps->declarator_pos);
  }  /* if */
  if (curr_token != tok_assign) {
    syntax_error(ec_literal_without_initializer);
  } else {
    a_constant_ptr     constant;
    a_source_position  init_pos = pos_curr_token;
    dps->has_initializer = TRUE;
    /* Record the starting position of the initialization */
    decl_info->decl_pos_block.var_init_range.start = pos_curr_token;
    /* Advance past the "=". */
    (void)get_token();
    if (dps->auto_type_specifier_seen && !is_error_type(member_type)) {
      prescan_initializer_for_auto_type_deduction(dps,
                                                 /*parenthesized_init=*/FALSE);
      member_type = dps->type;
    }  /* if */
    if (is_scalar_type(member_type) || is_template_param_type(member_type)) {
      if (is_const_qualified_type(member_type)) {
        /* "const" is useless on a C++/CLI literal field declaration. */
        a_source_position  *diag_pos = &dps->qualifiers_pos;
        if (!(dps->qualifiers & TQ_CONST)) diag_pos = &dps->start_pos;
        pos_warning(ec_literal_const_has_no_effect, diag_pos);
      }  /* if */
      /* Scan the constant expression. */
      constant = alloc_constant((a_constant_repr_kind)ck_error);
      scan_member_constant_initializer_expression(dps, constant);
      constant->is_literal_field = TRUE;
      /* Enter the constant name in the symbol table.  Do this after scanning
         the expression to avoid problems with a recursive reference. */
      dps->sym = enter_local_symbol((a_symbol_kind)sk_constant, locator,
                                    decl_scope_level,
                                    /*suppress_redecl_error=*/FALSE);
      /* Update the symbol and the constant entry. */
      dps->sym->variant.constant = constant;
      set_source_corresp(&(constant->source_corresp), dps->sym);
      set_class_membership(dps->sym, &constant->source_corresp, class_type);
      constant->source_corresp.access = class_state->access;
      record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, dps->sym,
                                &locator->source_position,
                                dps->source_sequence_entry);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      decl_info->decl_pos_block.var_init_range.end =
                                                  curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Do processing required for any pragmas that are bound to the current
      declaration. */
      process_curr_construct_pragmas(dps->sym, (a_statement_ptr)NULL);
      add_to_constants_list(constant, /*at_file_scope=*/FALSE);
    } else {
      /* Issue a diagnostic for an invalid literal type. */
      if (!is_error_type(member_type)) {
        pos_ty_error(ec_invalid_literal_type, &init_pos, member_type);
      }  /* if */
      scan_and_discard_initializer_expression(dps);
    }  /* if */
  }  /* if */
  db_exit();
}  /* decl_literal_field */


static a_property_or_event_descr_ptr property_or_event_descr_for_sym(
                                                            a_symbol_ptr  sym)
/*
The given symbol must be for a field or a static data member.  Return the
associated property/event description entry or NULL if there is none.
*/
{
  a_property_or_event_descr_ptr  pedp;

  if (sym->kind == (a_symbol_kind)sk_field) {
    pedp = sym->variant.field.ptr->property_or_event_descr;
  } else {
    check_assertion(sym->kind == (a_symbol_kind)sk_static_data_member);
    pedp = sym->variant.static_data_member.variable->property_or_event_descr;
  }  /* if */
  return pedp;
}  /* property_or_event_descr_for_sym */


static a_boolean distinguishable_property_indices(
                                             a_property_index_type_ptr  pitp1,
                                             a_property_index_type_ptr  pitp2)
/*
Return TRUE if and only if the given non-empty sequences of property index
types are distinguishable when overloading C++/CLI properties.
*/
{
  a_boolean  result = FALSE;

  do {
    if (pitp1 == NULL || pitp2 == NULL) {
      /* One sequence (but not both) has no more elements. */
      result = TRUE;
      break;
    } else if (!types_are_compatible(pitp1->type, pitp2->type)) {
      /* Incompatible corresponding index types are distinguishable. */
      result = TRUE;
      break;
    } else {
      pitp1 = pitp1->next;
      pitp2 = pitp2->next;
    }  /* if */
  } while (pitp1 != NULL || pitp2 != NULL);
  return result;
}  /* distinguishable_property_indices */


static void check_for_overloaded_property_conflict(a_symbol_ptr  property_set,
                                                   a_symbol_ptr  property_sym)
/*
property_sym represents a C++/CLI property that has just been declared, and it
is a member of the given property set (possibly the only such member).  If the
set contains any previously declared properties, diagnose any conflict created
by the new property.
*/
{
  a_symbol_ptr  sym = property_set->variant.property_info->properties;
  a_property_or_event_descr_ptr
                new_pedp, prev_pedp;

  check_assertion(cppcli_enabled);
  new_pedp = property_or_event_descr_for_sym(property_sym);
  for (; sym != property_sym; sym = sym->next) {
    a_boolean  conflict = FALSE;
    check_assertion(sym != NULL);
    prev_pedp = property_or_event_descr_for_sym(sym);
    if ((prev_pedp->indices == NULL) != (new_pedp->indices == NULL)) {
      /* One property is indexed, and the other isn't: No conflict. */
    } else if (new_pedp->indices == NULL) {
      /* Neither property is indexed: They conflict. */
      conflict = TRUE; 
    } else {
      conflict = !distinguishable_property_indices(prev_pedp->indices,
                                                   new_pedp->indices);
    }  /* if */
    if (conflict) {
      pos_sy_error(ec_conflicting_properties, &property_sym->decl_position,
                   sym);
      break;
    }  /* if */
  }  /* for */
}  /* check_for_overloaded_property_conflict */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean is_or_is_nested_within_unnamed_class(a_type_ptr  tp)
/*
Return TRUE if class type tp is an unnamed class or is nested within an
unnamed class.
*/
{
  a_boolean  unnamed = FALSE;

  check_assertion(is_immediate_class_type(tp));
  if (tp->variant.class_struct_union.originally_unnamed) {
    unnamed = TRUE;
  } else if (tp->source_corresp.is_class_member) {
    tp = parent_class_of(tp);
    unnamed = is_or_is_nested_within_unnamed_class(tp);
  }  /* if */
  return unnamed;
}  /* is_or_is_nested_within_unnamed_class */
    
#if GNU_EXTENSIONS_ALLOWED

#if !GNU_VISIBILITY_ATTRIBUTE_ALLOWED
/*ARGSUSED*/
#endif /* !GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
static void copy_gnu_class_properties_to_variable(a_type_ptr      class_type,
                                                  a_variable_ptr  var)
/*
var is a static data member of class_type.  Copy any properties of class_type
(from attributes applied to the class) that should be propagated to its static
data members.
*/
{
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
  if (var->ELF_visibility == (an_ELF_visibility_kind)evk_unspecified) {
    a_class_type_supplement_ptr
                     ctsp = class_type->variant.class_struct_union.extra_info;
    var->ELF_visibility = ctsp->ELF_visibility;
  }  /* if */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
}  /* copy_gnu_class_properties_to_variable */

#endif /* GNU_EXTENSIONS_ALLOWED */

static void decl_static_data_member(a_symbol_locator        *locator,
                                    a_class_def_state_ptr   class_state,
                                    a_member_decl_info_ptr  decl_info)
/*
Do processing for a static data member, including entering it in the symbol
table.  *locator is the symbol-locator for the current declaration, and
*p_member_type is the type with which the member was declared.  *class_state
and *decl_info track general information about the class definition and
specific information about the member declaration, respectively.
*/
{
  a_symbol_ptr          sym, prototype_tag_sym;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_symbol_ptr          property_set = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_variable_ptr        var;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_name_reference_ptr  name_ref = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_decl_parse_state    *decl_state = &decl_info->decl_state;
  a_type_ptr            class_type = class_state->class_type;
  a_type_ptr            member_type = decl_state->type;
  a_source_position     *start_pos = &decl_state->start_pos;

  db_enter(3, "decl_static_data_member");
  if (is_void_type(member_type)) {
    error(ec_incomplete_type_not_allowed);
    member_type = error_type();
  } else if (is_abstract_class_type(member_type)) {
    /* Abstract class objects are prohibited (ARM 10.3). */
    abstract_class_diagnostic(es_error, ec_abstract_class_object_not_allowed,
                              member_type, &locator->source_position);
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cppcli_enabled && (is_interior_ptr_type(member_type) || 
                                is_pin_ptr_type(member_type))) {
    /* In C++/CLI, an interior_ptr or pin_ptr cannot be a class member. */
    type_error(ec_type_cannot_be_class_member, member_type);
    member_type = error_type();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  /* The Microsoft compiler instantiates a template class used as the type
     of a static data member. */
  if (microsoft_bugs) complete_type_is_needed(member_type);
  if (class_state->is_local_class) {
    /* Static data members are not allowed in local classes. */
    pos_error(ec_static_data_member_not_allowed, start_pos);
    /* Set the type for this invalid static member to error type. This will
       assure "proper" (or unobtrusive) behavior later, if a definition is
       encountered.  It also eliminates semi-spurious error messages if there
       are references to it. */
    member_type = error_type();
  } else if (is_union_type(class_type)) {
    /* Unions are not allowed to have static data members. */
    pos_error(ec_static_data_member_not_allowed, start_pos);
  } else if (!any_cfront_mode() && !microsoft_mode && !gpp_mode &&
             is_or_is_nested_within_unnamed_class(class_type)) {
    /* Except for cfront, Microsoft, and GNU compatibility, static data members
       may not be declared in an unnamed class or a class contained within an
       unnamed class (9.4.2 [class.static.data]). However, permit this with a
       warning if anachronisms are enabled. */
    pos_diagnostic(anachronism_error_severity,
                   ec_static_data_member_not_allowed, start_pos);
  }  /* if */
  decl_state->type = member_type;
  if (decl_info->is_member_template) set_to_named_error_locator(*locator);
  /* Create the variable entry for the static data member. */
  /* All static data member variables are allocated in the file scope memory
     region and put on the variables list for the current class.  The storage
     class will usually be set to extern (except sometimes in cfront mode). */
  var = make_variable(member_type, (a_storage_class)sc_static, NO_SCOPE_DEPTH);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cppcli_enabled) {
    if (decl_state->has_cli_initonly_keyword) {
      var->is_initonly = TRUE;
    } else if (decl_state->has_cli_property_keyword ||
               decl_state->has_cli_event_keyword) {
      /* A static property or event is represented via a static data member. */
      var->property_or_event_descr = class_state->property_or_event_descr;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* If this is a member template declaration, don't add it to the variables
     list (in part to avoid problems caused by an invalid scope). */
  if (!decl_info->is_member_template || prototype_instantiations_in_il) {
    add_to_variables_list(var, decl_scope_level);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (var_is_property_or_event(var) &&
      property_or_event_kind_is(var, pek_cli_property)) {
    /* C++/CLI properties are associated with an sk_property_set symbol.
       Multiple properties (static and/or nonstatic) of the same name can be
       recorded under the same symbol (but each property also has its own
       sk_field or sk_static_data_member symbol). */
    sym = enter_property_set_member(locator, decl_scope_level,
                                    var->property_or_event_descr,
                                    &property_set);
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    sym = enter_symbol((a_symbol_kind)sk_static_data_member, locator,
                       decl_scope_level, /*suppress_redecl_error=*/FALSE);
  }  /* if */
  decl_state->sym = sym;
  /* Set the source correspondence fields of the variable. */
  set_source_corresp(&var->source_corresp, sym);
  sym->variant.static_data_member.variable = var;
  set_class_membership(sym, &var->source_corresp, class_type);
  if (decl_info->is_member_template && locator->symbol_header != NULL) {
    pos_sy_error(ec_bad_member_template_sym, &locator->source_position, sym);
  }  /* if */
  /* Static data members will have the same name linkage as the class of
     which they are members.  (In cfront mode that may mean internal linkage
     -- if and when its linkage is promoted to C++, the linkage of the static
     data members will also be changed.) */
  var->source_corresp.name_linkage = class_type->source_corresp.name_linkage;
  if (class_type->source_corresp.name_linkage ==
                        (a_name_linkage_kind)nlk_cplusplus_external) {
    /* Ordinarily a static data member gets sc_extern storage class, which
       is promoted to sc_unspecified if a definition is seen.  In cfront mode,
       the storage is sc_static (already set), which is changed to sc_extern
       or sc_unspecified during a final fixup pass. */
    var->storage_class = (a_storage_class)sc_extern;
    /* Check whether any types without linkage are used in the declaration. */
    check_constituent_types_have_linkage(sym, &locator->source_position,
                                         /*is_declaration=*/TRUE);
  }  /* if */
  var->source_corresp.access = class_state->access;
  attach_decl_attributes(decl_state, /*primary_decl=*/FALSE);
  update_variable_decl_modifiers(decl_state);
  if (curr_token == tok_assign && is_expr_start_token(next_token())) {
    a_constant         constant;
    a_source_position  init_pos;
    a_boolean          restore_member_visibility = FALSE;
    init_pos = pos_curr_token;
    /* Advance past the "=". */
    (void)get_token();
    decl_state->has_initializer = TRUE;
    if (decl_state->auto_type_specifier_seen && !is_error_type(member_type)) {
      prescan_initializer_for_auto_type_deduction(decl_state,
                                                 /*parenthesized_init=*/FALSE);
      member_type = decl_state->type;
    }  /* if */
    if ((microsoft_bugs || gpp_mode) && decl_state->sym != NULL) {
      /* In Microsoft bugs and GNU mode, the static data member being
         initialized is not visible while parsing the initializer. */
      decl_state->sym->is_invisible = TRUE;
      restore_member_visibility = TRUE;
    }  /* if */
    if ((is_const_qualified_type(member_type) &&
         (is_integral_or_enum_type(member_type) ||
          (gpp_mode &&
           (is_floating_type(member_type) ||
            (gnu_version < 30300 && is_pointer_type(member_type)))))) ||
#if MICROSOFT_EXTENSIONS_ALLOWED
        var->is_initonly ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        (class_state->is_nonreal_instantiation &&
         is_template_param_type(member_type))) {
      /* A const integral or const enumeration type may be initialized inside
         the class definition (9.5.2).   This makes the static data member
         usable as a member constant.  Note that the variable entry will have
         an initializer but it is not yet considered defined.  GNU compilers
         allow floating-point in-class initializers, and some versions even
         allow pointers to be initialized in this way.  C++/CLI also allows
         in-class initializers for initonly static data members. */
      decl_info->decl_pos_block.var_init_range.start = init_pos;
      /* Scan the constant expression. */
      scan_member_constant_initializer_expression(decl_state, &constant);
#if EXTRA_SOURCE_POSITIONS_IN_IL
      decl_info->decl_pos_block.var_init_range.end =
                                            curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      var->init_kind = (an_init_kind)initk_static;
      var->initializer.constant = alloc_unshared_constant(&constant);
      /* Set the flag indicating to the back end that, even though there is
         an initializer for this variable entry, it has not necessarily been
         defined. */
      var->is_member_constant = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      add_src_seq_end_of_variable_if_needed(decl_state);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else {
      /* Issue a diagnostic for an invalid member constant type. */
      if (!is_error_type(member_type)) {
        if (!is_const_qualified_type(member_type)) {
          pos_error(ec_member_constant_not_const, &init_pos);
        } else {
          pos_ty_error(ec_invalid_member_constant_type, &init_pos,
                       member_type);
        }  /* if */
      }  /* if */
      scan_and_discard_initializer_expression(decl_state);
    }  /* if */
    if (restore_member_visibility) {
      /* Restore the member's visibility. */
      decl_state->sym->is_invisible = FALSE;
    }  /* if */
  }  /* if */
  /* This is entered as a declaration rather than a definition, since the
     definition must appear outside the class definition. */
  record_symbol_declaration(SRK_DECLARATION, sym, &locator->source_position,
                            decl_state->source_sequence_entry);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  update_decl_pos_info(&var->source_corresp, &decl_info->decl_pos_block);
  if (var->is_member_constant) {
    var->initializer_range = decl_info->decl_pos_block.var_init_range;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (record_name_references_in_context()) {
    name_ref = qualifiable_name_reference(locator, &var->source_corresp);
  }  /* if */
  { an_sssd_flag_set  flags = SSSD_NO_FLAGS;
#if GNU_EXTENSIONS_ALLOWED
    if (decl_state->marked_as_gnu_extension) {
      flags |= SSSD_MARKED_AS_GNU_EXTENSION;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    (void)update_src_seq_secondary_decl((char *)var, member_type, name_ref,
                                        flags, &decl_info->decl_pos_block);
    wrapup_sse_for_simple_decl(decl_state);
  }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Do processing required for any pragmas that are bound to the current
     declaration. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  /* Special processing for static data members of template classes. */
  prototype_tag_sym = class_state->corresp_prototype_tag_sym;
  if (prototype_tag_sym != NULL || class_state->is_nonreal_instantiation) {
    /* A nonnull instance_ptr marks this static data member as a member of
       a (real or nonreal) instantiation of a class template. */
    if (!is_error_locator(*locator)) {
      if (class_state->is_nonreal_instantiation) {
        /* A member of a prototype instantiation. */
        a_template_ptr		 templ;
        a_template_instance_ptr  tip = alloc_template_instance();
        sym->variant.static_data_member.instance_ptr = tip;
        tip->instance_sym = sym;
        tip->template_sym = sym;
        tip->template_info = alloc_template_symbol_supplement(
                                       (a_symbol_kind)sk_static_data_member);
        tip->template_info->token_sequence_number = curr_token_sequence_number;
        /* Although this is not a template, it is an instantiatable variable
           and hence we create a placeholder a_template entry for it. */
        var->assoc_template = templ = alloc_template();
        templ->kind = (a_template_kind)templk_static_data_member;
        set_source_corresp(&templ->source_corresp, sym);
        templ->source_corresp.name_linkage =
                                   (a_name_linkage_kind)nlk_cplusplus_external;
        set_class_membership((a_symbol_ptr)NULL, &templ->source_corresp,
                              class_type);
        templ->source_corresp.access = var->source_corresp.access;
        /* Update the IL template pointer in the template symbol supplement. */
        tip->template_info->il_template_entry = templ;
        /* It is exported if the enclosing class template is exported. */
        templ->is_exported = class_is_exported(class_type);
        add_to_templates_list(templ, decl_scope_level);
        if (prototype_instantiations_in_il) {
          templ->prototype_instantiation.variable = var;
        }  /* if */
        templ->canonical_template = templ;
      } else {
        /* We must be in the midst of a template class instantiation.  We need
           to bind this static data member to the static data member template
           that was created for it in the prototype instantiation.  This will
           enable the compiler to generate a definition if a defining template
           is declared. */
        find_static_data_member_template(sym, prototype_tag_sym);
      }  /* if */
    }  /* if */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* Disallow data members in interface types. */
    if (class_type->variant.class_struct_union.is_interface) {
      pos_error(ec_interface_cannot_have_data_member,
                &locator->source_position);
    } else {
      class_state->potentially_interface_like = FALSE;
    }  /* if */
    if (decl_state->ms_attributes != NULL) {
      apply_microsoft_attributes(&decl_state->ms_attributes, (char*)var,
                                 iek_variable, MSAT_DATA_MEMBER);
    }  /* if */
    if (property_set != NULL) {
      check_for_overloaded_property_conflict(property_set, sym);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  if (gpp_mode) {
    /* Propagate any class properties that also apply to its static data
       members. */
    copy_gnu_class_properties_to_variable(class_type, var);
    /* If applicable, record the asm-name. */
    if (decl_state->asm_name != NULL) {
      var->asm_name_or_reg.name = decl_state->asm_name;
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if USER_CONTROL_OF_STRUCT_PACKING
  record_std_alignment_attr(decl_state);
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
  if (!var->source_corresp.is_deprecated) {
    /* Check if a deprecated type was involved in this declaration. */
    warn_about_use_of_deprecated_type(member_type, &locator->source_position);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED */
  /* Check for the case in which the type is or contains a routine type for
     which default arguments have been specified. */
  if (curr_routine_fixup != NULL &&
      curr_routine_fixup->def_arg_expr_fixup_list != NULL) {
    /* Update the symbol pointer in the fixup entry -- it's needed when the
       default args are scanned (once the entire class has been scanned). */
    curr_routine_fixup->symbol = sym;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) db_symbol(sym, "", 4);
#endif /* DEBUG */
  db_exit();
}  /* decl_static_data_member */


a_boolean is_assignment_operator_for_copy(
                                   a_symbol_ptr          sym,
                                   a_boolean             move_assign_okay,
                                   a_boolean             *is_ref_arg,
                                   a_type_qualifier_set  *qualifiers,
                                   a_boolean             *is_base_class_match)
/*
Return TRUE if sym, an sk_member_function symbol for an operator= function,
qualifies as a "copy assignment operator" that can copy a class object.
It qualifies if its first parameter has a type of "A", "A&", or "A const&",
where "A" is the class of which it is a member.  If move_assign_okay is TRUE,
the parameter can also have type "A&&" or "A const&&".  (In cfront
compatibility mode, sym also qualifies if the first parameter involves type B
where B is a base class of A.)  Set *is_ref_arg to TRUE if the first
parameter is a reference type.  Set *qualifiers based on how the first
parameter is qualified.  Return *is_base_class_match set to TRUE for the
cfront compatibility case.
*/
{
  a_boolean         found = FALSE;
  a_param_type_ptr  ptp;
  a_type_ptr        tp, routine_type;
  a_routine_type_supplement_ptr
                    rtsp;

  check_assertion(sym->kind == (a_symbol_kind)sk_member_function);
  routine_type = routine_symbol_type(sym);
  rtsp = routine_type->variant.routine.extra_info;
  ptp = rtsp->param_type_list;
  check_assertion(ptp != NULL);
  tp = skip_typerefs(ptp->type);
  if (move_assign_okay ? is_reference_type(tp)
                       : is_lvalue_reference_type(tp)) {
    /* Reference argument. */
    tp = type_pointed_to(tp);
    /* Don't do a skip_typerefs on what's returned from type_pointed_to,
       since we need to distinguish between "A&" and "const A&". */
    *is_ref_arg = TRUE;
  } else {
    /* Not a reference argument. */
    *is_ref_arg = FALSE;
  }  /* if */
  *is_base_class_match = FALSE;
  if (is_class_struct_union_type(tp)) {
    /* The type of the first parameter is a class type. */
    if (f_same_entities(skip_typerefs(tp), sym_parent_class(sym))) {
      /* The parameter's type matches the class of which the assignment
         operator is a member. */
      found = TRUE;
    } else if (allow_copy_assignment_op_with_base_class_param) {
      if (find_base_class_of(sym_parent_class(sym), tp) != NULL) {
        /* The parameter's type matches a base class of the class of which the
           assignment operator is a member. */
        found = TRUE;
        *is_base_class_match = TRUE;
      }  /* if */
    }  /* if */
    if (found) {
      /* Check the qualifiers.  (Call get_top_level_type_qualifiers instead
         of get_type_qualifiers because we know tp cannot be an array.) */
      *qualifiers = get_top_level_type_qualifiers(tp);
    }  /* if */
  }  /* if */
  if (rtsp->qualifiers != TQ_NONE && !microsoft_mode && !gnu_mode &&
      !sun_mode) {
    /* cv-qualifiers on the function disqualify it as a "copy assignment
       operator".  As of April 2006 this is not in 12.8p9 of the standard,
       but it makes sense.  (MSVC++, g++, and Sun CC all accept cv-qualified
       operator= functions as copy assignment operators.) */
    found = FALSE;
  }  /* if */
  return found;
}  /* is_assignment_operator_for_copy */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_symbol_ptr copy_assignment_specialization(
                                    a_symbol_ptr          templ_sym,
                                    a_boolean             *is_ref_arg,
                                    a_type_qualifier_set  *qualifiers,
                                    a_boolean             *is_base_class_match)
/*
If the given template symbol has an explicit specialization that looks like
a copy assignment operator, return the symbol for that specialization.
Otherwise, return NULL.  is_ref_arg, qualifiers and is_base_class_match
have the same meaning as the corresponding parameters of
is_assignment_operator_for_copy.
*/
{
  a_template_instance_ptr  inst = templ_sym->variant.template_info
                                           ->variant.function.instantiations;
  a_symbol_ptr             result = NULL;

  for (; inst != NULL; inst = inst->next) {
    if (inst->instance_sym->variant.routine.ptr->is_specialized &&
        is_assignment_operator_for_copy(
                               inst->instance_sym, /*move_assign_okay=*/FALSE,
                               is_ref_arg, qualifiers, is_base_class_match)) {
      result = inst->instance_sym;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* copy_assignment_specialization */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean is_valid_union_field(a_type_ptr        field_type,
                                      a_boolean         is_nonstd,
                                      a_source_position *pos)
/*
Nonstatic data members of a union may not be objects with a constructor,
a destructor, or a user-defined assignment operator.  If any such member
functions are present, then:  in cfront mode issue a warning if there's only
a user-defined assignment operator; otherwise, issue an error and return
FALSE .
*/
{
  a_type_ptr                     tp = skip_typerefs(field_type);
  a_class_symbol_supplement_ptr  cssp;
  an_error_severity              severity = es_none;

  db_enter(4, "is_valid_union_field");
  if (is_array_type(tp)) tp=f_skip_typerefs(underlying_array_element_type(tp));
  if (is_class_struct_union_type(tp)) {
    cssp = symbol_supplement_for_class(tp);
    if (tp->variant.class_struct_union.is_nonreal_class) {
      /* Suppress these checks for union members that are nonreal.  The test
         will be repeated when a real instantiation of the enclosing
         union is performed. */
    } else if (cssp->constructor != NULL || has_nontrivial_destructor(cssp)) {
      /* A union member's (underlying) type cannot be a class with a
         nontrivial constructor or destructor (WP 9.6).  If the constructor
         or destructor pointer is non-NULL, then even if one was generated by
         the compiler, it will be nontrivial. */
      severity = es_error;
    } else if (!cssp->assignment_by_bitwise_copy_allowed) {
      /* When this flag is false, memberwise assignment of the union would
         require calling an assignment operator, but that involves knowing
         which variant in the union is active.  This means, even if there
         is no user-defined copy assignment operator the compiler generated
         one is not trivial.  (This goes beyond what is literally required
         in WP 9.6 at this time.) */
      /* There is no error with cfront 2.1, but it is fixed in cfront 3.0. */
      severity = cfront_2_1_mode ? es_warning : es_error;
    }  /* if */
    if (severity != es_none) {
      an_error_code  err_code;
      if (is_nonstd) {
        err_code = ec_bad_nonstd_anonymous_union_field;
        if (severity == es_error) {
          /* The constraints for union fields are not really needed for
             nonstandard anonymous unions (GNU C++ imposes the constraints,
             but Microsoft C++ doesn't).  So at most a discretionary error
             should be issued. */
          severity = es_discretionary_error;
        }  /* if */
      } else {
        err_code = ec_bad_union_field;
      }  /* if */
      pos_ty_diagnostic(severity, err_code, pos, tp);
    }  /* if */
  }  /* if */

  db_exit();
  return (severity != es_error);
}  /* is_valid_union_field */

#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS

static a_symbol_ptr find_anonymous_parent_object_symbol_clone(
                                               a_symbol_ptr  apo_sym,
                                               a_symbol_ptr  *new_apo_sym_list,
                                               a_symbol_ptr  assoc_object_sym)
/*
apo_sym is an anonymous parent object symbol whose "clone" needs either to
be found or created.  Finding means looking on the list pointed to by
new_apo_sym_list; if a new one is created, it will be added to the list.
assoc_object_sym is the top-level anonymous parent object; it will always
be the last in the anonymous-union-parent chain.
*/
{
  a_symbol_ptr  new_apo_sym;

  db_enter(4, "find_anonymous_parent_object_symbol_clone");
  /* Loop through the list looking for a symbol that points at the same
     field as apo_sym. */
  for (new_apo_sym = *new_apo_sym_list;
       new_apo_sym != NULL;
       new_apo_sym = new_apo_sym->next) {
    if (new_apo_sym->variant.field.ptr == apo_sym->variant.field.ptr) {
      /* Found a match. */
      break;
    }  /* if */
  }  /* for */
  if (new_apo_sym == NULL) {
#if DEBUG
    if (debug_level >= 4) {
      db_symbol(apo_sym, "cloning: ", 2);
    }  /* if */
#endif /* DEBUG */
    /* None was found, so create a new one. */
    new_apo_sym = make_anonymous_parent_object_symbol((a_symbol_kind)sk_field,
                                                      &apo_sym->decl_position,
                                                      apo_sym->decl_scope);
    set_class_membership(new_apo_sym, (a_source_correspondence *)NULL,
                         sym_parent_class(apo_sym));
    /* Set it to point to the same field. */
    new_apo_sym->variant.field.ptr = apo_sym->variant.field.ptr;
    /* If apo_sym does is not itself nested in an anonymous parent object,
       then set the new symbol to point to assoc_object_sym.  Otherwise,
       find (or clone) the parent symbol. */
    if (apo_sym->variant.field.anonymous_parent_object == NULL) {
      new_apo_sym->variant.field.anonymous_parent_object = assoc_object_sym;
    } else {
      new_apo_sym->variant.field.anonymous_parent_object =
                 find_anonymous_parent_object_symbol_clone(
                                apo_sym->variant.field.anonymous_parent_object,
                                new_apo_sym_list, assoc_object_sym);
    }  /* if */
    /* Add the new symbol to the list.  Note that the next pointer is used.
       This is okay, since the symbol was not added to the symbol table. */
    new_apo_sym->next = *new_apo_sym_list;
    *new_apo_sym_list = new_apo_sym;
  }  /* if */
  db_exit();
  return new_apo_sym;
}  /* find_anonymous_parent_object_symbol_clone */

#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */

#if !ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
/*ARGSUSED*/ /* new_apo_syms is not used in some configurations. */
#endif /* !ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
static void promote_anonymous_union_field_symbol(
                                         a_symbol_ptr         sym,
                                         a_type_ptr           class_type,
                                         a_symbol_ptr         *new_apo_syms,
                                         a_symbol_ptr         assoc_object_sym,
                                         an_access_specifier  new_access,
                                         a_boolean            reuse_symbol,
                                         a_boolean            is_nonstd)
/*
sym represents a field in an anonymous union.  This procedure promotes it to
the surrounding (class or namespace) scope.  class_type is the class into which
the field is being promoted (or NULL if the promotion is into a namespace
scope).  *new_apo_syms is a list of newly created anonymous union parent
symbols that may need to be fixed up later on.  assoc_object_sym represents the
anonymous union object (field or variable) and new_access is the access that
must be given to the promoted field symbol.  If reuse_symbol is TRUE, sym can
just be moved; otherwise, it needs to be copied.  is_nonstd is TRUE if the
promotion is for a nonstandard anonymous union.
*/
{
  a_symbol_ptr  apo_sym = sym->variant.field.anonymous_parent_object;
  a_field_ptr   field = sym->variant.field.ptr;
 
  if (is_nonstd && gpp_mode &&
      !is_valid_union_field(field->type, /*is_nonstd=*/TRUE,
                            &field->source_corresp.decl_position)) {
    /* GNU C++ compilers apply the same constraints to nonstandard anonymous
       unions (which aren't really unions) as to ordinary unions.  There is
       nothing to do because is_valid_union_field already issued the
       diagnostic.  This test was already done for standard anonymous (and
       named) unions, but for nonstandard anonymous unions it had to wait
       until the lack of a declarator determined that this is in fact a
       nonstandard anonymous union. */
  }  /* if */
  if (reuse_symbol) {
    /* Allow conflicts with existing fields in GNU C and Microsoft C++ bugs
       modes. */
    a_boolean  suppress_error = gcc_mode || (microsoft_bugs && !C_mode());
    /* Unlink the symbol from the inactive list and link it back into
       the symbol table in the current scope. */
    remove_anonymous_union_member_from_inactive_symbols_list(sym);
      /* Enter the symbol back into the current scope. */
    reenter_symbol(sym, depth_scope_stack, suppress_error);
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
  } else {
    /* The symbol has to be kept bound to the type, since the latter
       may be used again.  Therefore, we have to clone the symbol,
       making a copy of it in the new class scope.  Note that there may
       turn out to be a many-to-one mapping between member symbols and
       field-of-assoc-object-type.  Note that the new symbol should
       normally not conflict with existing fields, but in GNU C mode
       such conflicts are ignored and only the first declaration is
       visible. */
    a_symbol_locator loc;
    make_locator_for_symbol(sym, &loc);
    loc.source_position = field->source_corresp.decl_position;
    sym = enter_local_symbol(sym->kind, &loc, depth_scope_stack,
                             /*suppress_error=*/gcc_mode);
    sym->variant.field.ptr = field;
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
  }  /* if */
  /* Set parent information in the symbol but not in the IL entry.  The
     symbol is promoted, but the type remains nested. */
  if (class_type != NULL) {
    /* The members of an anonymous union within a class take on the access
       specifier of the anonymous union itself (except in some GNU C++ mode
       cases); the members of a variable anonymous union should be (i.e.,
       should remain) public. */
    if (gpp_mode &&
        !same_entities(parent_class_of(parent_class_of(field)), class_type)) {
      /* GNU compilers only adjust the accessibility of a promoted field the
         first time it is promoted.  I.e., if the field appears within
         multiple levels of anonymous unions, it may not eventually acquire
         the accessibility of the outermost anonymous union.  For example:
             class C { union { struct { int i; }; }; };
         Here, field "i" is adjusted as it is promoted from the nonstandard
         anonymous to the standard anonymous union (which leaves the
         accessibility as public), but it is not adjusted during the second
         promotion from the union to class C (the end result is that C::i is
         publically accessible). */
      /* The field should normally already have public access, but to avoid
         repeated errors on private members of nested anonymous unions, we
         force access to be public here. */
      field->source_corresp.access = (an_access_specifier)as_public;
    } else {
      field->source_corresp.access = new_access;
    }  /* if */
    set_class_membership(sym, (a_source_correspondence *)NULL, class_type);
  } else {
    set_namespace_membership(sym, (a_source_correspondence *)NULL,
                             (a_namespace_ptr)NULL);
  }  /* if */
  if (apo_sym == NULL) {
    sym->variant.field.anonymous_parent_object = assoc_object_sym;
  } else if (reuse_symbol) {
    /* Only update the anonymous-parent-object pointer for a given
       symbol on the first promotion. */
    /* Walk up the chain of anonymous_parent_objects, which represent
       nested anonymous unions.  Stop if the current assoc_object_sym
       is found -- it will have been recorded, presumably, for a
       previously promoted field). */
    while (apo_sym != assoc_object_sym) {
      if (apo_sym->variant.field.anonymous_parent_object == NULL) {
        /* The end of the list: add assoc_object_sym and stop. */
        apo_sym->variant.field.anonymous_parent_object = assoc_object_sym;
        break;
      }  /* if */
      /* Advance up the chain. */
      apo_sym = apo_sym->variant.field.anonymous_parent_object;
    }  /* while */
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
  } else {
    /* Just as the original anonymous union member symbol had to be
       cloned, so too must its parent chain be cloned.  Go through the
       list of anonymous-union-parent symbols that have already been
       cloned and look for a match.  If none is found, make a new one. */
    sym->variant.field.anonymous_parent_object =
                 find_anonymous_parent_object_symbol_clone(apo_sym,
                                                           new_apo_syms,
                                                           assoc_object_sym);
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
  }  /* if */
}  /* promote_anonymous_union_field_symbol */


void check_anonymous_union_symbols(a_symbol_ptr  assoc_object_sym,
                                   a_type_ptr    class_type,
                                   a_boolean     is_nonstd)
/*
assoc_object_sym is a symbol for an unnamed field or variable that is the
object associated with an anonymous union.  The type of the field or
variable is an anonymous union type.  Process the member symbols of the
anonymous union: make a pass over all its members, perform some error
checking, and promote each field from the anonymous union to its containing
scope.  The scope to which the symbols are promoted is decl_scope_level.

If ALLOW_NONSTANDARD_ANONYMOUS_UNIONS, then, when assoc_object_sym refers
to a field, its type may also be an unnamed struct or class, or a typedef
referring to an unnamed class, struct, or union.  If the type is a typedef,
or a named struct or class, the symbols are not promoted, but rather new
ones are allocated in the scope specified by decl_scope_level.  For such
nonstandard anonymous unions is_nonstd is TRUE.
*/
{
  a_symbol_ptr                   sym, next_sym, mf_sym;
  a_class_symbol_supplement_ptr  cssp;
  a_class_type_supplement_ptr    ctsp;
  an_access_specifier            access, assoc_object_access;
  a_boolean                      access_error_already_issued = FALSE;
  a_boolean                      member_function_error_already_issued = FALSE;
  a_boolean                      is_overloaded;
  a_type_ptr                     assoc_object_type, tp;
  a_boolean                      reuse_symbol = TRUE;
  a_field_ptr                    au_field;
  a_symbol_ptr                   new_apo_sym_list = NULL;

  db_enter(4, "check_anonymous_union_symbols");
  switch (assoc_object_sym->kind) {
    case sk_variable:
      assoc_object_type = assoc_object_sym->variant.variable.ptr->type;
      assoc_object_type = skip_typerefs(assoc_object_type);
      check_assertion(assoc_object_type->kind == (a_type_kind)tk_union);
      assoc_object_access = (an_access_specifier)as_public;
      break;
    case sk_field:
      au_field = assoc_object_sym->variant.field.ptr;
      assoc_object_type = au_field->type;
      assoc_object_access = au_field->source_corresp.access;
      if (au_field->is_mutable) {
        /* No storage class is allowed at all, but the others are diagnosed
           elsewhere already. */
        pos_error(ec_no_mutable_allowed_on_anonymous_union,
                  &assoc_object_sym->decl_position);
      }  /* if */
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
      check_assertion(is_class_struct_union_type(assoc_object_type));
      if (assoc_object_type->kind == (a_type_kind)tk_typeref ||
          has_name(assoc_object_type)) {
        reuse_symbol = FALSE;
      }  /* if */
#else /* !ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
      check_assertion(assoc_object_type->kind == (a_type_kind)tk_union);
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
      break;
#if CHECKING
    default:
      internal_error("check_anonymous_union_symbols: bad symbol kind");
#endif /* CHECKING */
  }  /* switch */
#if DEBUG
  if (debug_level >= 4) {
    fputs("adding symbols to ", f_debug);
    if (class_type != NULL) {
      db_abbreviated_type(class_type);
    } else {
      fputs("file scope", f_debug);
    }  /* if */
    fputs(" from ", f_debug);
    db_abbreviated_type(assoc_object_type);
    db_symbol(assoc_object_sym, ":\n  ", 4);
  }  /* if */
#endif /* DEBUG */
  if (reuse_symbol && !C_mode() && !is_nonstd) {
    ctsp = assoc_object_type->variant.class_struct_union.extra_info;
    if (assoc_object_sym->kind == (a_symbol_kind)sk_field) {
      ctsp->anonymous_union_kind = (an_anonymous_union_kind)auk_field;
      ctsp->anonymous_union_field = assoc_object_sym->variant.field.ptr;
      ctsp->anonymous_union_field->compiler_generated = TRUE;
    } else {
      /* Save the storage class, which is used by the C++ generating back
         end.  The variable pointer cannot be stored in the class type
         supplement because it need not be in the file-scope memory region,
         but the type and its supplement always are. */
      ctsp->anonymous_union_kind = (an_anonymous_union_kind)auk_variable;
    }  /* if */
#if NEED_NAME_MANGLING
    /* A discriminator was assigned to the unnamed type, but the IA-64 ABI
       does not consider anonymous union types to be unnamed for mangling
       purposes (and we follow the IA-64 ABI conventions in the Cfront ABI
       here). */
#if IA64_ABI
    if (emulate_gnu_abi_bugs) {
      /* GNU counts anonymous unions as unnamed types (which affects any
         subsequent discriminators in this scope), so don't cancel the
         discriminator for this anonymous union type. */
    } else
#endif /* IA64_ABI */
    /* Do not insert code here. */
    {
      /* Reclaim the discriminator value. */
      cancel_name_collision_discriminator(symbol_for(assoc_object_type),
                                          decl_scope_level);
    }  /* if */
#endif /* NEED_NAME_MANGLING */
  }  /* if */
  /* Get the list of symbols that are to be either promoted (i.e., reused
     in the new scope) or cloned. */
  cssp = symbol_supplement_for_class(assoc_object_type);
  sym = cssp->symbols;
  if (reuse_symbol) {
    /* The symbols list for the anonymous union will be eliminated.  Its
       field symbols are promoted to the scope of the containing class. */
    cssp->symbols = NULL;
    /* Clear special symbol pointers for routines that will be discarded. */
    cssp->constructor = NULL;
    cssp->destructor = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
    check_assertion(cssp->finalizer == NULL &&
                    cssp->static_constructor == NULL);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    cssp->trivial_default_constructor = NULL;
    cssp->assignment_operator = NULL;
    /* Also reset some flags to values that make sense after the union
       is transformed. */
    cssp->has_nontrivial_default_constructor = FALSE;
    cssp->has_user_declared_default_constructor = FALSE;
    cssp->has_user_provided_default_constructor = FALSE;
    cssp->has_copy_constructor = FALSE;
    cssp->has_copy_constructor_for_const_object = FALSE;
    cssp->has_user_provided_copy_constructor = FALSE;
    cssp->has_trivial_destructor = FALSE;
    cssp->assignment_by_bitwise_copy_allowed = TRUE;
    cssp->construction_by_bitwise_copy_allowed = TRUE;
  }  /* if */
  /* Go through each of the symbols on the list. */
  check_assertion(decl_scope_level == depth_scope_stack || C_mode());
  for (; sym != NULL; sym = next_sym) {
    next_sym = sym->next_in_scope;
    if (reuse_symbol) {
#if DEBUG
      if (debug_level >= 4) {
        db_symbol(sym, (char *)(is_function_symbol(sym) ? "discarding: "
                                                        : "promoting: "), 2);
      }  /* if */
#endif /* DEBUG */
      /* Disjoin the symbol from the list.  It will be added to another
         list when it is reentered in the symbol table. */
      sym->next_in_scope = NULL;
      sym->prev_in_scope = NULL;
      remove_symbol_from_lookup_table(sym, &cssp->pointers_block);
      if (!is_template_symbol(sym)) {
        /* It is no longer treated as a member of the anonymous union but
           rather it will be a member of the class_type.  (Templates are not
           permitted and their symbols will be removed without being promoted.
           To avoid inconsistencies between the template symbol and the
           prototype instantiation, we don't clear the parent state.) */
        sym->is_class_member = FALSE;
        sym->parent.class_type = NULL;
      }  /* if */
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
    } else {
#if DEBUG
      if (debug_level >= 4) {
        db_symbol(sym, "cloning: ", 2);
      }  /* if */
#endif /* DEBUG */
      /* Creation of a new symbol is only implemented for fields, because it
         can only happen in C mode or with C++ classes that have no C++
         features.  (A compiler-generated assignment operator is fine.  If
         "near" and "far" qualifiers are enabled, that operator may be an
         overload set; see check_special_member_functions.) */
      check_assertion_str(
        sym->kind == (a_symbol_kind)sk_field ||
        (sym->kind == (a_symbol_kind)sk_member_function &&
         sym->variant.routine.ptr->compiler_generated) ||
        (near_and_far_enabled() && sym->is_class_member &&
         sym->kind == (a_symbol_kind)sk_overloaded_function),
        "check_anonymous_union_symbols: unexpected symbol kind");
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
    }  /* if */
    /* Private and protected members are not allowed in an anonymous union
       (ARM 9.5). */
    access = access_for_symbol(sym);
    if (access == (an_access_specifier)as_private ||
        access == (an_access_specifier)as_protected) {
      if (!access_error_already_issued) {
        pos_error(ec_anon_union_member_access,
                  &assoc_object_type->source_corresp.decl_position);
        access_error_already_issued = TRUE;
      }  /* if */
    }  /* if */
    switch (sym->kind) {
      case sk_field:
        promote_anonymous_union_field_symbol(
                         sym, class_type, &new_apo_sym_list, assoc_object_sym,
                         assoc_object_access, reuse_symbol, is_nonstd);
        break;
      case sk_member_function:
      case sk_overloaded_function:
      case sk_function_template:
        if (!is_nonstd) {
          /* Remove the symbol and don't reenter it.  (Don't attempt to remove
             the symbol of a nonstandard anonymous union since that would
             invalidate a type that may need to be used for other purposes.) */
          remove_anonymous_union_member_from_inactive_symbols_list(sym);
        }  /* if */
        /* This may be a compiler generated default assignment operator, which
           is okay.  Any user-defined member function is illegal. */
        if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
          mf_sym = sym->variant.overloaded_function.symbols;
          is_overloaded = TRUE;
        } else {
          mf_sym = sym;
          is_overloaded = FALSE;
        }  /* if */
        for (; mf_sym != NULL;
               mf_sym = is_overloaded ? mf_sym->next : NULL) {
          if (!member_function_error_already_issued &&
              (mf_sym->kind == (a_symbol_kind)sk_function_template ||
               !mf_sym->variant.routine.ptr->compiler_generated)) {
            pos_error(ec_anon_union_member_function,
                      &assoc_object_type->source_corresp.decl_position);
            member_function_error_already_issued = TRUE;
          }  /* if */
        }  /* for */
        break;
      case sk_type:
      case sk_class_or_struct_tag:
      case sk_union_tag:
      case sk_enum_tag:
        /* Unlink the symbol from the inactive list and link it back into
           the symbol table in the current scope. */
        tp = type_symbol_type(sym);
        /* Set parent information in the symbol but not in the IL entry.  The
           symbol is promoted, but the type remains nested. */
        if (class_type != NULL) {
          set_class_membership(sym, (a_source_correspondence *)NULL,
                               class_type);
        } else {
          set_namespace_membership(sym, (a_source_correspondence *)NULL,
                                   (a_namespace_ptr)NULL);
        }  /* if */
        /* The members of an anonymous union within a class take on the
           access specifier of the anonymous union itself; the members
           of a variable anonymous union should be (i.e., should remain)
           public. */
        tp->source_corresp.access = assoc_object_access;
        remove_anonymous_union_member_from_inactive_symbols_list(sym);
        reenter_symbol(sym, decl_scope_level, /*suppress_error=*/FALSE);
        break;
      case sk_constant:
        /* An enum constant. */
        /* Set parent information in the symbol but not in the IL entry.  The
           symbol is promoted, but the type remains nested. */
        if (class_type != NULL) {
          set_class_membership(sym, (a_source_correspondence *)NULL,
                               class_type);
        } else {
          set_namespace_membership(sym, (a_source_correspondence *)NULL,
                                   (a_namespace_ptr)NULL);
        }  /* if */
        sym->variant.constant->source_corresp.access = assoc_object_access;
        remove_anonymous_union_member_from_inactive_symbols_list(sym);
        reenter_symbol(sym, decl_scope_level, /*suppress_error=*/FALSE);
        break;
      case sk_class_template:
        /* Member class template -- issue an error.  (This is not explicitly
           required by anything in the WP at this time.) */
        pos_error(ec_anon_union_class_member_template,
                  &assoc_object_type->source_corresp.decl_position);
        /* Remove the symbol and don't reenter it. */
        remove_anonymous_union_member_from_inactive_symbols_list(sym);
        break;
      case sk_static_data_member:
        /* Must be an error, since unions cannot have static data members,
           and the nonstandard case is only allowed to have fields.  Ignore
           this symbol. */
        break;
      case sk_undefined:
        /* Error. */
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case sk_property_set:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      default:
        unexpected_condition_str(
                        "check_anonymous_union_symbols: unexpected sym kind");
    }  /* switch */
#if DEBUG
    if (debug_level >= 4) {
      if (is_function_symbol(sym)) {
        /* Nothing. */
      } else if (reuse_symbol) {
        db_symbol(sym, "after promotion: ", 2);
      } else {
        db_symbol(sym, "new symbol: ", 2);
      }  /* if */
    }  /* if */
#endif /* DEBUG */
  }  /* for */
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
  /* Just to be safe, clear the next pointers on any anonymous union parent
     symbols created in this routine. */
  for (sym = new_apo_sym_list; sym != NULL; sym = next_sym) {
    next_sym = sym->next;
    sym->next = NULL;
  }  /* for */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
  if (!(C_mode() || microsoft_mode || sun_mode || any_cfront_mode())) {
    /* Types should normally not be declared inside an anonymous union. */
    a_scope_ptr  scope = skip_typerefs(assoc_object_type)
                                        ->variant.class_struct_union.extra_info
                                        ->assoc_scope;
    if (scope != NULL) {
      a_type_ptr  nested_type = scope->types;
      for (; nested_type != NULL; nested_type = nested_type->next) {
        if (allow_anon_types_in_anon_unions && !has_name(nested_type)) {
          /* Some test suites commonly declare anonymous types in anonymous
             unions.  Since these tests must run in strict mode, a flag is
             provided to inhibit this particular diagnostic. */
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
        } else if (allow_nonstandard_anonymous_unions &&
                   !has_name(nested_type)) {
          /* Similarly, nonstandard anonymous unions can contain nested
             anonymous types. */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
        } else {
          pos_diagnostic(strict_ansi_mode ?
                         strict_ansi_discretionary_severity : es_warning,
                         ec_type_decl_in_anon_union,
                         &nested_type->source_corresp.decl_position);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
  db_exit();
}  /* check_anonymous_union_symbols */

#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS

static a_boolean is_compiler_generated_member_function(a_symbol_ptr  sym)
/*
Returns TRUE if and only if sym refers to either a compiler-generated member
function or an overload set of compiler generated member functions.
*/
{
  a_boolean result;

  if (sym->kind == (a_symbol_kind)sk_member_function &&
      sym->variant.routine.ptr->compiler_generated) {
    result = TRUE;
  } else if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
    a_symbol_ptr  overloaded_sym = sym->variant.overloaded_function.symbols;
    /* Assume all the members of the overload set are compiler-generated.
       Set result to FALSE as soon as one is not. */
    result = TRUE;
    for (; overloaded_sym != NULL; overloaded_sym = overloaded_sym->next) {
      if (!is_compiler_generated_member_function(overloaded_sym)) {
        result = FALSE;
        break;
      }  /* if */
    }  /* for */
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* is_compiler_generated_member_function */

#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */

static a_boolean is_anonymous_union_decl(a_member_decl_info_ptr  decl_info)
/*
A declaration has appeared in which there is no declarator.  Return TRUE if
it is an anonymous union declaration.  If ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
is TRUE and this is not a standard C++ anonymous union, return TRUE and
also set the is_nonstd_anonymous_union flag in the member-decl-info block.
*/
{
  a_type_ptr       member_type = decl_info->decl_state.type;
  a_decl_flag_set  dso_flags = decl_info->decl_state.dso_flags;

  if (!C_mode() && member_type->kind == (a_type_kind)tk_union) {
    if (member_type->source_corresp.name != NULL ||
        !(dso_flags & DSO_DEFINES_SOMETHING)) {
      /* This union was named and/or is a reference to a previously defined
         type -- in any case, it's not an anonymous union. */
    } else if (dso_flags & (DSO_DECLARES_SOMETHING | DSO_FRIEND)) {
      /* This cannot be a standard or a nonstandard anonymous union in C++. */
    } else {
      decl_info->is_anonymous_union = TRUE;
    }  /* if */
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
  } else if (!allow_nonstandard_anonymous_unions) {
    /* This compilation is not configured to support this extension -- e.g.,
       this is not a Microsoft or GNU mode. */
  } else if (!is_class_struct_union_type(member_type)) {
    /* Not a pseudo-anonymous-union -- it's not a class, struct,
       or union type. */
  } else if ((!C_mode() || (gcc_mode && gnu_version >= 30300)) &&
             ((dso_flags & (DSO_DECLARES_SOMETHING | DSO_FRIEND)) ||
              !(skip_typerefs(member_type))->
                            variant.class_struct_union.originally_unnamed)) {
    /* Not a pseudo-anonymous-union -- either a tag appeared on the current
       declaration, or it's a typedef name and a tag was declared originally,
       or else it's a C++ friend declaration. */
  } else {
    /* This may in fact be an anonymous-union-like construct. */
    /* Skip the typedefs but not cv qualifiers. */
    a_type_ptr  tp = member_type;
    if (tp->kind == (a_type_kind)tk_typeref) {
      a_boolean  typedef_used = skip_typerefs_not_typedefs(tp)->kind ==
                                                    (a_type_kind)tk_typeref;
      if (typedef_used && !C_mode()) {
        /* The anonymous-union-like construct was expressed through a typedef.
           E.g.:  typedef union { int i; } U;
                  struct S { U; };
           That form is not allowed in C++ modes (GNU and Microsoft compilers
           don't accept it; disallowing this in C++ also simplifies lowering
           later on). */
      } else if ((microsoft_mode || gnu_mode)) {
        /* In Microsoft and GNU modes, cv-qualifiers are allowed are allowed on
           anonymous-union-like constructs not expressed through a typedef.  In
           Microsoft C mode, cv-qualifiers are also allowed on anonymous-union-
           like constructs expressed through a typedef (the C++-mode case was
           already handled above). */
        if (microsoft_mode) {
          tp = skip_typerefs(tp);
        } else if (!typedef_used) {
          /* GNU mode and member_type is a qualified immediate class type
             (i.e., there is no typedef involved). */
          tp = skip_typerefs(tp);
        } else {
          /* GNU C mode nonstandard anonymous union expressed through a
             typedef: Ignore the typedef, but not any cv-qualifiers.  */
          check_assertion(C_mode());
          tp = skip_typedefs(tp);
        }  /* if */
      } else {
        tp = skip_typedefs(tp);
      }  /* if */
    }  /* if */
    if (tp->kind == (a_type_kind)tk_typeref) {
      /* This must be a cv qualifier on top of what we already know to be a
         class, struct, or union type, or a typedef in C++ mode.  Either way,
         it is disqualified from being treated as an anonymous-union-like
         construct (except sometimes in Microsoft and GNU C++ modes; in those
         cases tp will have been adjusted above). */
    } else {
      if (C_mode()) {
        /* In C mode that's all we need to know. */
        decl_info->is_anonymous_union = TRUE;
      } else {
        /* Some C++ features cannot appear in nonstandard anonymous unions
           (e.g., member functions, static data members). */
        a_class_symbol_supplement_ptr  cssp;
        a_symbol_ptr                   sym;

        cssp = symbol_supplement_for_class(tp);
        if (cssp->is_class_aggregate ||
            ((gpp_mode || microsoft_mode) && base_classes_of(tp) == NULL)) {
          if ((sym = cssp->symbols) != NULL) {
            /* Assume. */
            decl_info->is_anonymous_union = TRUE;
            for (; sym != NULL; sym = sym->next_in_scope) {
              if (sym->kind == (a_symbol_kind)sk_field) {
                /* Okay. */
              } else if (sym == cssp->trivial_default_constructor) {
                /* Okay. */
              } else if (is_type_symbol(sym) &&
                         same_entities(tp, member_type)) {
                /* Okay if the nonstandard anonymous union is not of the
                   variety introduced with a typedef. */
              } else if (is_compiler_generated_member_function(sym)) {
                /* A compiler generated function -- most likely a default
                   assignment operator.  This is okay. */
              } else {
                decl_info->is_anonymous_union = FALSE;
                break;
              }  /* if */
            }  /* for */
          }  /* if */
        }  /* if */
      }  /* if */
      if (decl_info->is_anonymous_union) {
        /* Set the nonstandard flag. */
        decl_info->is_nonstd_anonymous_union = TRUE;
        if (strict_ansi_mode) {
          /* Issue a diagnostic that this is an extension. */
          pos_diagnostic(strict_ansi_error_severity, 
                         C_mode() ? ec_nonstd_unnamed_field :
                                    ec_nonstd_unnamed_member,
                         &pos_curr_token);
        } else if (gnu_mode) {
          report_gnu_extension_if_needed(&pos_curr_token,
                                         C_mode() ? ec_nonstd_unnamed_field :
                                                    ec_nonstd_unnamed_member);
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
  }  /* if */
  return decl_info->is_anonymous_union;
}  /* is_anonymous_union_decl */


static void add_error_field(a_type_ptr  class_type,
                            a_field_ptr *end_of_list)
/*
Add a field of error type to the field list for the specified class type.
*/
{
  a_symbol_locator  locator;
  a_symbol_ptr      sym;
  a_field_ptr       fp = alloc_field();

  set_to_error_locator(locator);
  fp->type = error_type();
  /* Create the field symbol. */
  sym = enter_local_symbol((a_symbol_kind)sk_field, &locator,
                           depth_scope_stack, /*suppress_redecl_error=*/TRUE);
  sym->variant.field.ptr = fp;
  set_source_corresp(&fp->source_corresp, sym);
  set_class_membership(sym, &fp->source_corresp, class_type);
  /* Add the field to the temporary list for this class/struct/union. */
  if (*end_of_list == NULL) {
    class_type->variant.class_struct_union.field_list = fp;
  } else {
    (*end_of_list)->next = fp;
  }  /* if */
  *end_of_list = fp;
}  /* add_error_field */


static void check_enum_type_for_bit_field(a_type_ptr    bit_field_type,
                                          unsigned long bit_field_size,
                                          a_boolean     *need_signed_type)
/*
Check to see that the values of the enumerated type bit_field_type will all
fit in a bit field of size bit_field_size.  If not, give a warning.  Return
*need_signed_type TRUE if the bit field type must be signed, FALSE if it
must be unsigned.
*/
{
  a_boolean      use_signed = FALSE, smallest_is_negative;
  a_constant     smallest, largest;
  unsigned long  bits_needed, bits_needed_largest, bits_needed_smallest;
  a_constant_ptr enum_con;

  enum_con = enum_constants(bit_field_type);
  if (enum_con == NULL) {
    /* There are no enumeration constants, so no bits are needed to
       represent all of them; by definition, they fit in the bit field. */
  } else {
    /* Check the constants on the list.  Start by finding the largest and
       smallest constants.  We are assuming most enum type lists
       won't be too long, and there won't be too many bit fields with enum
       type, so a linear search should be acceptable.  Furthermore,
       the usual case is that the bit field is big enough, so we're probably
       going to scan the whole constant list; therefore it's okay to always
       scan the whole list even though some errors could be detected during
       the scan. */
    smallest = *enum_con;
    largest = *enum_con;
    for (;;) {
      enum_con = enum_con->next;
      if (enum_con == NULL) break;
      if (cmp_integer_constants(enum_con, &smallest) < 0) smallest = *enum_con;
      if (cmp_integer_constants(enum_con, &largest)  > 0) largest  = *enum_con;
    }  /* for */
    /* Determine the number of bits needed to represent largest value. */
    bits_needed_largest =
                        bits_required_to_represent_integer_constant(&largest);
    /* See if the smallest value is negative. */
    smallest_is_negative = (sign_of_integer_constant(&smallest) < 0);
    if (targ_enum_bit_fields_are_always_unsigned) {
      /* Enum bit fields are always unsigned (many ABIs require this). */
      use_signed = FALSE;
      bits_needed = bits_needed_largest;
    } else {
      /* Determine the proper signedness for the bit field.  One can't
         simply use the signedness of the enum type, since that was chosen
         for efficiency reasons: if the enum values just fit in the bit
         field size, an unsigned field might be necessary even though a 
         signed type was a good choice for the enum type. */
      if (smallest_is_negative) {
        /* Some enum values are negative, so a signed type is required.
           The enum type must already be signed. */
        use_signed = TRUE;
      } else if (bits_needed_largest >= bit_field_size) {
        /* The largest value is nonnegative (because the smallest is
           nonnegative), and it's big enough that it wouldn't fit in a
           signed field.  Therefore, an unsigned type is required. */
        use_signed = FALSE;
      } else {
        /* The signedness is not forced by the enum values, so use the 
           target preference. */
        /* Note that because bits_needed_largest is at least 1, we can
           never get here for a bit field of length one. */
        use_signed = !targ_nonnegative_enum_bit_field_is_unsigned;
      }  /* if */
      if (use_signed && sign_of_integer_constant(&largest) > 0) {
        /* Using a signed bit field and the largest is positive, so the
           largest value really requires one more bit for a zero sign. */
        bits_needed_largest++;
      }  /* if */
      /* Determine the number of bits needed. */
      bits_needed_smallest =
                        bits_required_to_represent_integer_constant(&smallest);
      if (bits_needed_largest > bits_needed_smallest) {
        bits_needed = bits_needed_largest;
      } else {
        bits_needed = bits_needed_smallest;
      }  /* if */
    }  /* if */
    /* Check that the enum values will fit in the bit field. */
    if (bits_needed > bit_field_size) {
      warning(ec_enum_bit_field_too_small);
    }  /* if */
    if (targ_enum_bit_fields_are_always_unsigned && smallest_is_negative) {
      type_warning(ec_unsigned_enum_bit_field_with_signed_enumerator,
                   bit_field_type);
    }  /* if */
  }  /* if */
  *need_signed_type = use_signed;
}  /* check_enum_type_for_bit_field */


static void apply_bit_field_size(a_field_ptr       field,
                                 a_constant_ptr    size_constant,
                                 a_boolean         *unnamed_bit_field,
                                 a_type_ptr        *p_base_type,
                                 a_symbol_locator  *locator)
/*
The given field is declared as a bit field with the given size constant:

    unsigned int j: 5 ;
                    ^---- this size.

If *unnamed_bit_field is TRUE, the bit-field is unnamed.  *p_base_type gives
the base type of the declaration (unsigned int in the above example); it may
be updated on return.
*/
{
  unsigned long    bit_field_size, max_size_allowed;
  unsigned long    declared_bit_field_size;
  a_type_ptr       base_type = *p_base_type;
  a_boolean        err = FALSE, is_signed = FALSE, templated_type = FALSE;
  a_type_ptr       bit_field_type;
  an_integer_kind  int_kind;

  db_enter(3, "apply_bit_field_size");
  /* ANSI C says the type of a bit-field must be int, unsigned int,
     or signed int, but we also allow enums and integral types (see A.6.5.8
     in the Common Extensions appendix).  pcc and C++ (ARM 9.6) allow any
     integral or enum type. */
  bit_field_type = skip_typerefs(base_type);
  if (is_template_param_type(bit_field_type)) {
    templated_type = TRUE;
  } else if (!is_integral_or_enum_type(bit_field_type)) {
    /* Diagnostic has already been issued. */
    bit_field_type = integer_type((an_integer_kind)ik_int);
  }  /* if */
  /* Note that if the base type was not integral it has been replaced by
     "int" by this point. */
  field->bit_size_constant = alloc_shareable_constant(size_constant);
  if (is_error_constant(size_constant)) {
    /* Use small value to avoid more errors, but not 1 which is special. */
    declared_bit_field_size = bit_field_size = targ_char_bit;
    err = TRUE;
  } else if (size_constant->kind == (a_constant_repr_kind)ck_template_param) {
    /* A template parameter during the prototype instantiation.  The value
       is not known.  Use a small value that is not 1. */
    declared_bit_field_size = bit_field_size = targ_char_bit;
  } else {
    a_boolean  ovflo = FALSE;
#if CHECKING
    if (size_constant->kind != (a_constant_repr_kind)ck_integer) {
      internal_error("apply_bit_field_size: size not int");
    }  /* if */
#endif /* CHECKING */
    /* The size of the bit field must be non-negative and must not exceed
       the size of the underlying type. */
    if (templated_type || is_error_type(*p_base_type)) {
      max_size_allowed =
                  (unsigned long)(TARG_SIZEOF_LARGEST_INTEGER*targ_char_bit);
    } else {
      max_size_allowed = (unsigned long)(bit_field_type->size*targ_char_bit);
    }  /* if */
    bit_field_size = (unsigned long)
                     unsigned_value_of_integer_constant(size_constant, &ovflo);
    declared_bit_field_size = bit_field_size;
    /* Note that one reason for ovflo to be TRUE is if the constant is
       less than zero. */
    if (ovflo || bit_field_size > max_size_allowed) {
      if (ovflo || (C_mode() && !(gcc_mode && gnu_version < 30400))) {
        /* Force the declared size to something reasonable. */
        error(ec_bad_bit_field_size);
        declared_bit_field_size = max_size_allowed;
        err = TRUE;
      } else if (bit_field_size > max_size_allowed) {
        /* A warning in C++ and GNU C modes (prior to GNU version 3.4). */
        char  buffer[8];
        sprintf(buffer, "%lu", max_size_allowed);
        pos_st_warning(ec_extra_bits_ignored, &error_position, buffer);
        if (gcc_mode) {
          /* In GNU C mode (but not in GNU C++ mode), oversized bitfields are
             turned into ordinary fields. */
          field->is_bit_field = FALSE;
          goto done;
        }  /* if */
      }  /* if */
      bit_field_size = max_size_allowed;
    } else if (bit_field_size == 0) {
      /* The bit-field size is zero, so the field must be unnamed. */
      if (*unnamed_bit_field) {
        /* Okay. */
      } else if (any_cfront_mode()) {
        /* Cfront compatibility -- permit named bit fields to have zero
           size, but change the value of *unnamed_bit_field so that they
           will not be entered into the symbol table.  Note that it would
           be possible for the name to be used again (though this would not
           be acceptable to cfront), but it also means the field will not
           be subject to initialization (cfront allows such fields to be
           initialized and may generate invalid C as a result) and it means
           the field cannot be referenced (again, cfront allows it and
           generates invalid C). */
        pos_warning(ec_zero_length_bit_field_must_be_unnamed,
                    &locator->source_position);
        *unnamed_bit_field = TRUE;
      } else {
        /* Error. */
        pos_error(ec_zero_length_bit_field_must_be_unnamed,
                  &locator->source_position);
        bit_field_size = 1;
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Determine the signedness of the bit field. */
  if (templated_type) {
    /* Signedness constraints cannot be checked. */
  } else if (bit_field_type->variant.integer.enum_type) {
    /* The integral type is an enum type.  Give a warning if any of the
       enumeration's constants will not fit in the bit field, and determine
       whether the bit field should be signed or unsigned. */
    if (size_constant->kind != (a_constant_repr_kind)ck_template_param) {
      check_enum_type_for_bit_field(bit_field_type, bit_field_size,
                                    &is_signed);
    }  /* if */
  } else {
    int_kind = bit_field_type->variant.integer.int_kind;
    if (bit_field_type->variant.integer.explicitly_signed ||
        (C_dialect != C_dialect_pcc &&
         int_kind == (an_integer_kind)ik_signed_char)) {
      /* The integral type was explicitly signed in the source, e.g.,
         "signed int" instead of just "int".  (This information comes from
         the type entry itself.)  That forces the bit field to be signed.
         The integral type already has the right kind and signedness.  Note
         that this won't happen in pcc mode because "signed" is not part of
         the pcc language. */
      is_signed = TRUE;
    } else if (!int_kind_is_signed[int_kind]) {
      /* The integral type must have been explicitly declared "unsigned", or
          else it's a plain "char" that is treated as unsigned. */
      is_signed = FALSE;
    } else {
      /* The integral type is "plain" (i.e., plain "int", "char", "short",
         "long", or "long long") -- it's not explicitly signed or unsigned and
         it's not an enum type. */
      if (any_cfront_mode()) {
        /* cfront treats all bit fields as unsigned. */
        is_signed = FALSE;
      } else if (targ_plain_int_bit_field_is_unsigned) {
        /* The default for plain integral types in bit fields is unsigned. */
        is_signed = FALSE;
        if (C_dialect == C_dialect_pcc) {
          /* In pcc mode when the environment expects plain-int bit fields to
             be unsigned, change the underlying type to reflect that -- this
             produces more accurate IL for expressions in which integral
             promotion is not done. */
          int_kind = unsigned_int_kind_of[int_kind];
          bit_field_type = integer_type(int_kind);
        }  /* if */
      } else if (bit_field_size == 1 &&
                 (targ_force_one_bit_bit_field_to_be_unsigned ||
                  is_bool_type(bit_field_type))) {
        /* Force a one-bit bit field to be unsigned, because a bit field
           consisting of only a sign is not very useful.  Also force one-bit
           bool bit fields to have an unsigned underlying type (the bool type
           itself is neither signed nor unsigned, but its underlying type is
           often signed). */
        is_signed = FALSE;
      } else {
        /* The default for plain integral types in bit fields is signed. */
        is_signed = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Give a warning for a signed one-bit field; ANSI C allows it, but it's
     strange.  If the bit field type is an enumeration type, warnings were
     already issued if the enumerator constant value cannot be represented
     by the field; so we don't issue another warning here. */
  if (!err && !*unnamed_bit_field && is_signed && bit_field_size == 1 &&
      !bit_field_type->variant.integer.enum_type) {
    pos_warning(ec_signed_one_bit_field, &locator->source_position);
  }  /* if */
  /* Set base_type to bit_field_type with the proper type qualifiers. */
  if (bit_field_type == skip_typerefs(base_type)) {
    /* The original type, base_type, has turned out to be correct after all.
       Use it directly to avoid wasting the type qualifiers, if any. */
  } else {
    /* Build a type with the right qualifiers.  Note that bit_field_type
       should not have any qualifiers at this point; the qualifiers from the
       base type, if any, are added. */
    base_type = make_identically_qualified_type(bit_field_type, base_type);
  }  /* if */
  *p_base_type = base_type;
  field->bit_size = (a_byte)bit_field_size;
  field->declared_bit_size = declared_bit_field_size;
  field->bit_field_is_signed = is_signed;
done:;
  db_exit();
}  /* apply_bit_field_size */


static a_boolean last_field_is_flexible(a_type_ptr  class_type)
/*
Return TRUE if the last field (if any) of the given class type (possibly a
typedef) has an incomplete array type, or has a class type that has the flag
contains_flexible_array_member set to TRUE.
*/
{
  a_type_ptr  tp = skip_typerefs(class_type);
  a_field_ptr fp = tp->variant.class_struct_union.field_list;
  a_boolean   result = FALSE;

  if (fp != NULL) {
    a_type_ptr  field_type;
    while (fp->next != NULL) fp = fp->next;
    field_type = skip_typerefs(fp->type);
    result = is_incomplete_array_type(field_type) ||
             (is_immediate_class_type(field_type) &&
              field_type
                 ->variant.class_struct_union.contains_flexible_array_member);
  }  /* if */
  return result;
}  /* last_field_is_flexible */


static void check_field_type(a_symbol_locator        *locator,
                             a_class_def_state_ptr   class_state,
                             a_member_decl_info_ptr  decl_info,
                             a_boolean               is_bit_field)
/*
Check that the type of a nonstatic data member is valid, and report incomplete
types and incorrect types on bit-field declarations.  *locator is the symbol
locator for the field being declared.  *class_state and *decl_info track
general information about the class definition and specific information about
the member declaration, respectively.  is_bit_field is TRUE for bit field
declarations.
*/
{
  a_decl_parse_state  *decl_state = &decl_info->decl_state;
  a_type_ptr          field_type = decl_state->type;
  a_type_ptr          class_type = class_state->class_type;

  /* First check whether there was a preceding field of incomplete array type
     for which an error should now be issued. */
  if (class_state->last_field_is_incomplete_array) {
    a_field_ptr  prev_field = class_state->end_of_field_list;

    check_assertion(prev_field != NULL &&
                    !is_union_type(class_state->class_type));
    pos_error(ec_incomplete_type_not_allowed,
              &prev_field->source_corresp.decl_position);
    prev_field->type = error_type();
    class_state->last_field_is_incomplete_array = FALSE;
  } else if (microsoft_mode || (c99_mode && !gcc_mode)) {
    /* In Microsoft mode a class or struct may include a member whose type
       contains a final field that is an unknown-size array (in nonstrict C99
       mode, we accept this as an extension).  Such a member must be the last
       field.  If the previous field was of such a type, no error was issued,
       in case it was the last field; issue the error now.  (In GNU mode,
       such members are also allowed, but they are not constrained to be the
       last field.) */
    if (!is_union_type(class_type) &&
        class_type->variant.class_struct_union.
                              contains_flexible_array_member) {
      a_field_ptr  prev_field = class_state->end_of_field_list;
      check_assertion(prev_field != NULL &&
                      is_class_struct_union_type(prev_field->type) &&
                      skip_typerefs(prev_field->type)->
                                        variant.class_struct_union.
                                        contains_flexible_array_member);
      if (microsoft_bugs && is_union_type(prev_field->type) &&
          !last_field_is_flexible(prev_field->type)) {
        /* Microsoft compilers allow flexible array members anywhere in unions,
           and if the last field of a union does not contain a flexible array,
           a field of that union type may be followed by another field.
           For example:
             union X { float f[]; int i; };  // f is not the last field.
             struct Y { X x; int y; };  // Accepted in Microsoft bugs mode.
           We emulate this in Microsoft bugs mode. */
      } else {
        pos_error(ec_flexible_array_member_not_allowed,
                  &prev_field->source_corresp.decl_position);
        prev_field->type = error_type();
      }  /* if */
      class_type->variant.class_struct_union.contains_flexible_array_member =
                                                                        FALSE;
    }  /* if */
  }  /* if */
  /* The type specified must be complete. */
  complete_type_is_needed(field_type);
  if (C_mode() && is_function_type(field_type) &&
      decl_state->storage_class != (a_storage_class)sc_typedef) {
    pos_error(ec_function_type_not_allowed, &locator->source_position);
    field_type = error_type();
  } else if (vla_enabled && is_variably_modified_type(field_type)) {
    pos_error(ec_field_cannot_involve_vla_type, &locator->source_position);
    field_type = error_type();
  } else if (is_incomplete_type(field_type)) {
    /* The member type is incomplete.  This is not necessarily an error:
       an array of unknown size is sometimes allowed as the last member. */
    a_boolean   incomplete_okay = FALSE;
    /* The last member may be an incomplete array in C99 mode, as an
       extension otherwise in C mode, and in Microsoft and GNU C++ modes as 
       long as the class has no virtual base classes. */
    if (C_mode() ||
        ((microsoft_mode || gpp_mode) &&
         !class_type->variant.class_struct_union.any_virtual_base_classes)) {
      /* The member must be an incomplete array, but not one whose
         underlying element type is incomplete. */
      if (is_array_type(field_type) &&
          !is_incomplete_type(underlying_array_element_type(field_type))) {
        if (is_union_type(class_type)) {
          /* Incomplete member in a union; not usually allowed. */
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (microsoft_mode) {
            /* In Microsoft mode, any member of a union can have such an
               array type.  The problem of a zero-sized union is dealt with
               in the layout code. */
            incomplete_okay = TRUE;
            class_type
                ->variant.class_struct_union.contains_flexible_array_member =
                                                                          TRUE;

          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else {
          /* The field has an incomplete array type, and it's a member of a
             struct or class.  This can sometimes be okay -- in Microsoft
             mode (both C and C++), and, as long as it's not the first named
             field, in C99 and GNU C modes.  As an extension, this is
             supported in other C modes (except in strict C89 mode). */
          if ((!class_state->is_first_field &&
               class_state->any_fields_other_than_unnamed_bitfields) ||
              microsoft_mode) {
            /* A further restriction is that the incomplete array has to be
               the last field in the struct or class.  This can't always be
               determined simply by looking at the next token, so set a flag
               now and issue the error later if it turns out that another
               field follows it.  Note that fields marked as "properties"
               (Microsoft mode) never need to be complete either: such
               fields behave more like member functions. */
            incomplete_okay = TRUE;
            class_state->last_field_is_incomplete_array = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (decl_state->is_property_or_event_field) {
              /* This is a property or event field: no need to guard against
                 additionally appended fields. */
              class_state->last_field_is_incomplete_array = FALSE;
            }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (incomplete_okay) {
      /* Incomplete type is okay for now: it will be determined later if
         an error should be issued. */
    } else if (is_template_param_type(field_type)) {
      check_assertion(skip_typerefs(field_type)->variant.template_param.kind ==
                                   (a_template_param_type_kind)tptk_member);
      /* Okay. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (decl_state->is_property_or_event_field) {
      /* A property or event field doesn't need to have a complete type. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      a_type_ptr  el_type = skip_array_types(field_type);
      el_type = skip_typerefs(el_type);
      if (class_state->is_nonreal_instantiation &&
          is_immediate_class_type(el_type) &&
          (microsoft_mode ||
           (gpp_mode &&
            (gnu_version < 30400 ||
             is_template_param_or_nonreal_class_type(el_type))))) {
        /* In Microsoft and early g++ modes, a field type can be incomplete in
           a prototype instantiation.  In later g++ modes incomplete class
           types are permitted if they are nonreal types. */
      } else {
        if (!C_mode() && is_error_locator(*locator) &&
            !decl_info->is_unnamed_field) {
          /* Don't issue an error since we can't be sure this was intended to
             be a field -- it could be an ill-formed function declaration with
             a void return type, such as
               void operator?:();
             in which the param list is not processed. */
        } else {
          pos_error(incomplete_type_err_code(field_type),
                    &locator->source_position);
        }  /* if */
        field_type = error_type();
      }  /* if */
    }  /* if */
  } else if (flexible_array_members_allowed &&
             is_class_struct_union_type(field_type) &&
             skip_typerefs(field_type)->
               variant.class_struct_union.contains_flexible_array_member) {
    /* The member is a struct whose final member is an incomplete array or
       else the member is a union that contains such a struct. */
    if (class_type->kind == (a_type_kind)tk_union) {
      /* The containing type is a union.  The member is allowed, but be sure
         the containing class is marked, since there are restrictions on its
         use in C99 mode. */
      class_type->variant.class_struct_union.
                                  contains_flexible_array_member = TRUE;
    } else if (microsoft_mode || gnu_mode || (c99_mode && !strict_ansi_mode)) {
      /* In Microsoft and GNU modes the error is issued only if the struct
         containing a flexible array member is not the last member.  Just
         set the flag for now and do the check later.  (This is also supported
         as an extension in default C99 mode.) */
      class_type->variant.class_struct_union.
                                  contains_flexible_array_member = TRUE;
    } else {
      /* The containing type is a struct, and so the member type is not
         allowed (i.e., the member may not be a struct with an incomplete
         array type as its last field nor be a union with such a member --
         see C99 standard, 6.7.2.1 para 2). */
      pos_error(ec_flexible_array_member_not_allowed,
                &locator->source_position);
      field_type = error_type();
    }  /* if */
  }  /* if */
  if (!is_error_type(field_type)) {
    a_boolean  is_ref = is_any_reference_type(field_type);
    if (is_abstract_class_type(field_type)) {
      /* Abstract class objects are prohibited (ARM 10.3). */
      abstract_class_diagnostic(es_error, ec_abstract_class_object_not_allowed,
                                field_type, &locator->source_position);
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (cppcli_enabled && is_tracking_reference_type(field_type)) {
      pos_error(ec_field_cannot_be_tracking_reference,
                &decl_state->declarator_pos);
      field_type = error_type();
    } else if (cppcli_enabled &&
               is_immediate_managed_class_type(class_type) ?
                     is_array_type(field_type) : is_handle_type(field_type)) {
      /* Array types are disallowed in managed class types and handles are
         disallowed in non-managed (i.e., standard) class types. */
      
      pos_error(is_immediate_managed_class_type(class_type) ?
                  ec_standard_array_field_in_managed_class :
                  ec_handle_field_in_standard_class,
                &decl_state->declarator_pos);
      field_type = error_type();
    } else if (cppcli_enabled && (is_interior_ptr_type(field_type) || 
                                  is_pin_ptr_type(field_type))) {
      /* In C++/CLI, an interior_ptr or pin_ptr cannot be a class member. */
      pos_ty_error(ec_type_cannot_be_class_member, 
                   &locator->source_position, field_type);
      field_type = error_type();
    } else if (cppcli_enabled && is_value_class_type(class_type) &&
               is_class_struct_union_type(field_type) &&
               !is_value_class_type(field_type)) {
      /* Non-value class types cannot be used for value class members. */
      pos_ty_error(ec_nonvalue_class_type_cannot_be_value_class_member, 
                   &locator->source_position, field_type);
      field_type = error_type();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (strict_ansi_mode && is_union_type(class_type) && is_ref) {
      /* Unions are not allowed to have members of reference type. */
      pos_diagnostic(strict_ansi_error_severity, ec_ref_not_allowed_in_union,
                     &decl_state->start_pos);
      if ((int)strict_ansi_error_severity > (int)es_warning) {
        field_type = error_type();
      } /* if */
#if NAMED_ADDRESS_SPACES_ALLOWED
    } else if (type_qualified_with_named_address_space(field_type)) {
      pos_error(ec_field_type_cannot_be_qualified_with_named_address_space,
                &locator->source_position);
      field_type = type_without_named_address_space_qualifiers(field_type);
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
    }  /* if */
    if (is_ref && (decl_state->dso_flags & DSO_MUTABLE)) {
      pos_diagnostic(
          strict_ansi_mode ?  strict_ansi_discretionary_severity : es_warning,
          ec_reference_declared_mutable, &decl_state->start_pos);
    }  /* if */
  }  /* if */
  if (is_bit_field) {
    /* Bit-field declaration -- be sure the type is okay. */
    a_type_ptr  unqual_type = skip_typerefs(field_type);
    if (!is_integral_or_enum_type(unqual_type)) {
      /* Error, not an integral or enum type. */
      if (is_error_type(unqual_type)) {
        /* An error has already been issued. */
      } else if (is_template_param_type(unqual_type)) {
        /* We're in a prototype instantiation -- don't issue an error. */
      } else {
        /* Invalid type. */
        pos_error(ec_bad_bit_field_type, &decl_state->start_pos);
        field_type = error_type();
      }  /* if */
    } else {
      /* Integral or enum base type.  In strict ANSI C mode, give a
         diagnostic about a nonstandard base type (anything other than int,
         unsigned int, and signed int). */
      if (C_mode() && strict_ansi_mode) {
        if (c99_mode && is_bool_type(unqual_type)) {
          /* C99 allows _Bool. */
        } else if (unqual_type->variant.integer.enum_type ||
                   (unqual_type->variant.integer.int_kind !=
                                        (an_integer_kind)ik_int &&
                    unqual_type->variant.integer.int_kind !=
                                        (an_integer_kind)ik_unsigned_int)) {
          pos_diagnostic(strict_ansi_error_severity, ec_nonstd_bit_field_type,
                         &decl_state->start_pos);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  decl_state->type = field_type;
}  /* check_field_type */

#if DECL_MODIFIERS_IN_USE

#if !MICROSOFT_EXTENSIONS_ALLOWED
/*ARGSUSED*/ /* class_type and member_type only used when Microsoft extensions
                are enabled. */
#endif /* !MICROSOFT_EXTENSIONS_ALLOWED */
static void check_declspec_for_field(a_member_decl_info_ptr  decl_info,
                                     a_symbol_locator        *locator,
                                     a_type_ptr              class_type,
                                     a_type_ptr              member_type)
/*
A field of type member_type is being declared in the given class.  Issue a
diagnostic for __declspec specifiers that are not valid in this context.  Also
warn about potentially unintended situations.  The diagnostic is emitted for
the position indicated by the given locator.
*/
{
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode && !C_mode() && is_class_struct_union_type(member_type)) {
    /* Microsoft compilers warn when fields of certain non-DLL class types are
       used as members of classes with a DLL interface.  Specifically, a
       warning is issued if the member type has a virtual function or a
       constructor. */
    a_class_type_supplement_ptr  c_ctsp, m_ctsp;
    c_ctsp = skip_typerefs(class_type)->variant.class_struct_union.extra_info;
    m_ctsp = skip_typerefs(member_type)->variant.class_struct_union.extra_info;
    if ((c_ctsp->decl_modifiers & DM_DLLFLAGS) != 0 &&
        (m_ctsp->decl_modifiers & DM_DLLFLAGS) == 0 &&
        (symbol_supplement_for_class(member_type)->constructor != NULL ||
         skip_typerefs(member_type)->variant.class_struct_union
                           .any_virtual_functions_including_in_base_classes)) {
      pos_warning(ec_field_without_dll_interface, &locator->source_position);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  if (decl_info->decl_state.decl_modifiers.flags & DM_THREAD) {
    pos_error(ec_cannot_use_thread_local_storage, &locator->source_position);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || THREAD_LOCAL_STORAGE_SPECIFIER_... */
}  /* check_declspec_for_field */

#endif /* DECL_MODIFIERS_IN_USE */

static void attach_field_attributes(a_decl_parse_state  *dps,
                                    a_field_ptr         field)
/*
Attach the attributes recorded in *dps to the given field.
*/
{
  if (dps->id_attributes != NULL || dps->prefix_attributes != NULL) {
    if (dps->secondary_declarator) {
      dps->prefix_attributes = copy_of_attributes_list(dps->prefix_attributes);
    }  /* if */
    attach_parse_state_to_attributes(dps);
    mark_primary_decl_attributes(dps->id_attributes);
    attach_attributes(dps->id_attributes, (char*)field, iek_field);
    mark_primary_decl_attributes(dps->prefix_attributes);
    attach_attributes(dps->prefix_attributes, (char*)field, iek_field);
    detach_parse_state_from_attributes(dps);
  }  /* if */
}  /* attach_field_attributes */


static a_field_ptr decl_nonstatic_data_member(
                                      a_symbol_locator        *locator,
                                      a_class_def_state_ptr   class_state,
                                      a_member_decl_info_ptr  decl_info,
                                      a_scope_depth           decl_scope_depth)
/*
Create the IL for a nonstatic data member of a class, struct, or union.
Create an entry in the symbol table for it if it has a name.  *locator is
the symbol locator for the declaration.  *class_state and *decl_info track
general information about the class definition and specific information
about the member declaration, respectively.  Return the field entry that was
created.  decl_scope_depth is the depth at which the member symbol should
be entered.
*/
{
  a_decl_parse_state             *decl_state = &decl_info->decl_state;
  a_type_ptr                     class_type = class_state->class_type;
  a_type_ptr                     member_type, member_element_type;
  a_field_ptr                    field;
  a_symbol_ptr                   member_sym = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_symbol_ptr                   property_set = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_class_symbol_supplement_ptr  cssp;
  a_boolean                      unnamed_field = decl_info->is_unnamed_field;

  db_enter(3, "decl_nonstatic_data_member");
  /* Create the field entry. */
  field = alloc_field();
  field->is_bit_field = decl_info->is_bit_field;
  field->is_captured_this = decl_info->is_captured_this;
  field->is_captured_pack_element = decl_info->is_captured_pack_element;
  cssp = symbol_supplement_for_class(class_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cppcli_enabled) {
    if (decl_state->has_cli_initonly_keyword) {
      /* A C++/CLI initonly field: The enclosing class cannot be bitwise
         copied. */
      field->is_initonly = TRUE;
      cssp->assignment_by_bitwise_copy_allowed = FALSE;
    } else if (decl_state->has_cli_property_keyword ||
               decl_state->has_cli_event_keyword) {
      /* A nonstatic property or event is represented via a nonstatic data
         member. */
      field->property_or_event_descr = class_state->property_or_event_descr;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (decl_info->is_member_template) {
    /* Error -- suppress incomplete-type errors, etc.. */
    set_to_named_error_locator(*locator);
    member_type = error_type();
  } else {
    /* Do error checking on the type. */
    check_field_type(locator, class_state, decl_info, field->is_bit_field);
    member_type = decl_state->type;
  }  /* if */
  /* Set the flag to record that at least one field that is not an unnamed
     bit-field was encountered. */
  if (!decl_info->is_unnamed_field || !decl_info->is_bit_field) {
    class_state->any_fields_other_than_unnamed_bitfields = TRUE;
  }  /* if */
  if (!C_mode() && class_type->kind == (a_type_kind)tk_union &&
      !decl_info->is_anonymous_union) {
    /* An object of a class with a constructor, a destructor, or a user-
       defined assignment operator cannot be a member of a union. */
    if (!is_valid_union_field(member_type, /*is_nonstd=*/FALSE,
                              &locator->source_position)) {
      member_type = error_type();
    }  /* if */
  }  /* if */
  if (field->is_bit_field) {
    /* Scan the bit-field size and determine the bit-field type. */
    apply_bit_field_size(field, &decl_info->bit_field_size,
                         &unnamed_field, &member_type, locator);
  }  /* if */
  /* Copy the type (which may have been changed by apply_bit_field_size) into
     the field entry. */
  field->type = member_type;
  /* For an unnamed bit field, do not create the field symbol. */
  if (unnamed_field && decl_info->is_bit_field) {
    /* All field entries for an unnamed bit field share the same symbol.  It is
       used for easy identification. */
    field->source_corresp.assoc_info = (char *)unnamed_field_symbol();
    /* Update the source correspondence information manually -- there's no
       symbol. */
    field->source_corresp.decl_position = locator->source_position;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    field->source_corresp.decl_pos_info = alloc_decl_position_supplement(
                                                      /*at_file_scope=*/TRUE);
    decl_info->decl_pos_block.declarator_range.start =
                                                 decl_info->bit_field_size_pos;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Ordinarily we create source sequence entries only for named
       entities (see sym_update_source_sequence_list, called for fields
       from record_symbol_declaration).  An exception is made for unnamed
       fields; call the subroutine directly. */
    update_source_sequence_list((char *)field, (an_il_entry_kind)iek_field,
                                decl_state->source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (decl_info->is_member_template) {
      pos_error(ec_bad_member_template_decl, &decl_state->start_pos);
    }  /* if */
  } else {
    /* Create the field symbol. */
    if (decl_info->is_anonymous_union) {
      member_sym = make_anonymous_parent_object_symbol(
                              (a_symbol_kind)sk_field, &decl_state->start_pos,
                              scope_stack[decl_scope_depth].number);
      /* Don't call set_source_corresp since we don't want to record a name
         in the IL entry. */
      field->is_anonymous_parent_object = TRUE;
      field->source_corresp.assoc_info = (char*)member_sym;
      field->source_corresp.decl_position = decl_state->start_pos;
    } else if (unnamed_field) {
      /* An unnamed field (but not a bit-field).  Such fields are used
         to represent the captured "this" parameter in lambdas. */
      member_sym = make_unnamed_symbol((a_symbol_kind)sk_field,
                                       &locator->source_position);
      /* Adjust the decl_scope given by make_unnamed_symbol. */
      member_sym->decl_scope = decl_scope_depth;
    } else {
      /* A named field.  C++/CLI property fields are treated specially since
         they can be "overloaded". */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (field_is_property_or_event(field) &&
          property_or_event_kind_is(field, pek_cli_property)) {
        member_sym = enter_property_set_member(locator, decl_scope_depth,
                                               field->property_or_event_descr,
                                               &property_set);
        if (field->property_or_event_descr->is_default_indexed) {
          cssp->default_indexed_properties = property_set;
        }  /* if */
      } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Do not insert code here. */
      {
        member_sym = enter_symbol((a_symbol_kind)sk_field, locator,
                                  decl_scope_depth,
                                  /*suppress_redecl_error=*/
                                             field->is_captured_pack_element);
      }  /* if */
      set_source_corresp(&(field->source_corresp), member_sym);
    }  /* if */
    member_sym->variant.field.ptr = field;
    decl_info->decl_state.sym = member_sym;
  }  /* if */
  /* Set the parent class in the field and (unless member_sym is NULL) in the
     symbol. */
  set_class_membership(member_sym, &field->source_corresp, class_type);
  if (decl_info->is_member_template && locator->symbol_header != NULL) {
    pos_sy_error(ec_bad_member_template_sym, &locator->source_position,
                 member_sym);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (decl_state->is_property_or_event_field) {
    /* The field for a property or event doesn't really exist, so pragmas
       cannot be bound to it. */
    cannot_bind_to_curr_construct();
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (member_sym != NULL && !decl_info->is_anonymous_union) {
    record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, member_sym,
                              &locator->source_position,
                              decl_state->source_sequence_entry);
    /* Do processing required for any pragmas that are bound to the current
       declaration. */
    process_curr_construct_pragmas(member_sym, (a_statement_ptr)NULL);
  } else {
    /* Issue diagnostics on pragmas that are trying to bind to an unnamed
       field. */
    cannot_bind_to_curr_construct();
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  /* The extended position information must be updated after the call to
     record_symbol_declaration (since the latter clears that information). */
  update_decl_pos_info(&field->source_corresp, &decl_info->decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  attach_field_attributes(decl_state, field);
  /* An error during attribute application may modify the field type. */
  member_type = field->type;
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode) {
    /* Check if a deprecated type was involved in this declaration.
       Unlike other similar cases, the warning is issued even when the field
       itself is marked as deprecated. */
    warn_about_use_of_deprecated_type(member_type, &locator->source_position);
    /* An asm name is not allowed on a field. */
    if (decl_state->asm_name != NULL) {
      pos_error(ec_field_with_asm_name_not_allowed, &decl_state->asm_name_pos);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Record whether the declaration was preceded by __extension__. */
    field->source_corresp.marked_as_gnu_extension =
                                          decl_state->marked_as_gnu_extension;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (decl_state->ms_attributes != NULL) {
    apply_microsoft_attributes(&decl_state->ms_attributes, (char*)field,
                               iek_field, MSAT_DATA_MEMBER);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Add the field to the temporary list for this class/struct/union. */
  if (class_state->end_of_field_list == NULL) {
    class_type->variant.class_struct_union.field_list = field;
  } else {
    class_state->end_of_field_list->next = field;
  }  /* if */
  class_state->end_of_field_list = field;
  if (C_dialect == C_dialect_cplusplus) {
    field->source_corresp.access = class_state->access;
    if (decl_state->dso_flags & DSO_MUTABLE) {
      /* The member is declared "mutable". */
      field->is_mutable = TRUE;
      class_type->variant.class_struct_union.any_mutable_member = TRUE;
    }  /* if */
    /* In C++ we need to keep track of whether any members have reference
       type. */
    if (is_any_reference_type(member_type)) {
      cssp->any_ref_member = TRUE;
      /* Assignment by bitwise copy is not allowed when a class has reference
         type members. */
      cssp->assignment_by_bitwise_copy_allowed = FALSE;
    }  /* if */
    /* Record that there is at least one nonstatic data member in the class. */
    cssp->any_nonstatic_data_members = TRUE;
    if (is_or_contains_template_param(member_type)) {
      /* The field is template parameter dependent and hence the POD/non-POD
         character of the containing class may not be certain. */
      cssp->any_template_dependent_fields = TRUE;
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      if (!decl_state->is_property_or_event_field) {
        /* Disallow real data members in interface types (declspec property
           fields are fine; C++/CLI properties and events will already have
           triggered an error). */
        if (class_type->variant.class_struct_union.is_interface) {
          pos_error(ec_interface_cannot_have_data_member,
                    &locator->source_position);
        } else {
          class_state->potentially_interface_like = FALSE;
        }  /* if */
      } else if (property_set != NULL) {
        check_for_overloaded_property_conflict(property_set, member_sym);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
#if DECL_MODIFIERS_IN_USE
  /* Check validity of __declspec. */
  check_declspec_for_field(decl_info, locator, class_type, member_type);
#endif /* DECL_MODIFIERS_IN_USE */
  member_element_type = is_array_type(member_type) ?
                     underlying_array_element_type(member_type) : member_type;
  /* Remember if any member of the class, struct, or union is const-
     qualified, including recursively the members of any contained
     classes, structs, or unions.  This is useful for determination of
     modifiable lvalues (see 3.2.2.1).  Note that C89 is subtly different
     from C99 and C++ in this regard: C89 does not consider the qualification
     of array element types (though it does consider the qualification of
     members of those element types). */
  { a_type_ptr  type_to_check = (!C_mode() || (c99_mode && strict_ansi_mode)) ?
                                   member_element_type : member_type;
    if (is_const_qualified_type(type_to_check) ||
        (is_class_struct_union_type(member_element_type) &&
         skip_typerefs(member_element_type)->
                               variant.class_struct_union.any_const_member)) {
      class_type->variant.class_struct_union.any_const_member = TRUE;
      if (C_dialect == C_dialect_cplusplus) {
        /* Assignment by bitwise copy is not allowed when a class has const
           qualified members. */
        cssp->assignment_by_bitwise_copy_allowed = FALSE;
      }  /* if */
    }  /* if */
  }
  if (cssp->standard_layout &&
      is_class_struct_union_type(member_element_type)) {
    cssp->standard_layout =
            symbol_supplement_for_class(member_element_type)->standard_layout;
  }  /* if */
  /* Note if any member (or member of a member, recursively) has a
     volatile-qualified type, to handle side effects and warnings
     correctly. */
  if (is_or_has_volatile_qualified_type(member_type)) {
    class_type->variant.class_struct_union.any_volatile_member = TRUE;
  }  /* if */
  if (decl_info->is_anonymous_union) {
    /* Do checking, promote symbols to the current class. */
    check_anonymous_union_symbols(member_sym, class_type,
                                  (a_boolean)decl_info->
                                               is_nonstd_anonymous_union);
  }  /* if */
  if (is_aggregate_or_union_type(member_type)) {
    /* If the member's type is class, struct, or union -- or array of class,
       struct, or union -- there is additional checking to be done. */
    a_type_ptr  tp = skip_typerefs(member_element_type);
    if (is_class_struct_union_type(tp)) {
      /* Propagate the flag indicating that zero-initialization may be needed
         as part of value-initialization. */
      if (tp->variant.class_struct_union.has_zero_init_component) {
        class_type->variant.class_struct_union.has_zero_init_component = TRUE;
      }  /* if */
      if (C_dialect == C_dialect_cplusplus) {
        a_class_symbol_supplement_ptr  member_cssp =
                                              symbol_supplement_for_class(tp);
        /* If the member type has any members of ref type, propagate the
           flag to the parent type. */
        if (member_cssp->any_ref_member) cssp->any_ref_member = TRUE;
        /* If a nonstatic data member of a class is itself a class object (or
           an array whose elements are class objects) and the subobject has a
           nontrivial default constructor and/or destructor, the containing
           class is also required to have a nontrivial default constructor
           and/or destructor.  Do the check at this time, and record the
           requirement, if any. */
        if (!has_trivial_default_constructor(member_cssp)) {
          class_state->default_ctor_is_nontrivial = TRUE;
        }  /* if */
        if (has_nontrivial_destructor(member_cssp)) {
          class_state->member_destruction_required = TRUE;
        }  /* if */
        /* The parent class cannot be copy-constructed or assigned by bitwise
           copying if the member class does not allow it.  (If the member
           class is nonreal, assume it doesn't affect this.) */
        if (!tp->variant.class_struct_union.is_nonreal_class) {
          if (!member_cssp->construction_by_bitwise_copy_allowed) {
            cssp->construction_by_bitwise_copy_allowed = FALSE;
          }  /* if */
          if (!member_cssp->assignment_by_bitwise_copy_allowed) {
            cssp->assignment_by_bitwise_copy_allowed = FALSE;
          }  /* if */
        }  /* if */
        /* If the member type has mutable members, set the flag in the parent
           type. */
        if (tp->variant.class_struct_union.any_mutable_member) {
          class_type->variant.class_struct_union.any_mutable_member = TRUE;
        }  /* if */
        /* A POD may not have a field with a type that is a non-POD class
           (or array thereof). */
        if (!member_cssp->is_POD) class_state->POD_ruled_out = TRUE;
      }  /* if */
    } else {
      /* The field's type is an array of nonclass elements. */
      class_type->variant.class_struct_union.has_zero_init_component = TRUE;
    }  /* if */
  } else {
    if (!(unnamed_field && field->is_bit_field)) {
      /* Unnamed bit fields do not need to be initialized.  Other fields that
         do not have a class (or array of class) type may need to be zero-
         initialized. */
      class_type->variant.class_struct_union.has_zero_init_component = TRUE;
    }  /* if */
    if (C_dialect == C_dialect_cplusplus && !class_state->POD_ruled_out) {
      if (is_any_reference_type(member_type)) {
        /* A POD may not have a field with a reference type. */
        class_state->POD_ruled_out = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Check for the case in which the type is or contains a routine type for
     which default arguments have been specified. */
  if (curr_routine_fixup != NULL &&
      curr_routine_fixup->def_arg_expr_fixup_list != NULL) {
    /* Update the symbol pointer in the fixup entry -- it's needed when the
       default args are scanned (once the entire class has been scanned). */
    curr_routine_fixup->symbol = member_sym;
  }  /* if */
  if (!class_state->class_aggregate_ruled_out) {
    if (class_state->access != (an_access_specifier)as_public) {
      if (decl_info->is_unnamed_field && decl_info->is_bit_field) {
        /* Unnamed bit fields are not subject to initialization (and
           are not even members, according to WP 9.6) so a nonpublic
           one (whatever that means) has no effect on aggregate
           state. */
      } else {
        /* No class with private or protected nonstatic data members
           is an aggregate (WP 8.5.1). */
        class_state->class_aggregate_ruled_out = TRUE;
        class_state->POD_ruled_out = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!class_state->any_const_or_ref_fields) {
    /* Record whether a const or ref field has been encountered. */
    if (decl_info->is_anonymous_union) {
      /* Note whether there was a const member of the anonymous union -- it
         will have been promoted into the current class. */
      if (skip_typerefs(member_type)->
                            variant.class_struct_union.any_const_member) {
        class_state->any_const_or_ref_fields = TRUE;
      }  /* if */
    } else if (decl_info->is_unnamed_field && decl_info->is_bit_field) {
      /* Ignore unnamed bit-fields. */
    } else if (is_any_reference_type(member_type) ||
               is_const_qualified_type(member_type)) {
      class_state->any_const_or_ref_fields = TRUE;
    }  /* if */
  }  /* if */
  class_state->is_first_field = FALSE;
#if DEBUG
  if (debug_level >= 3) {
    if (member_sym != NULL) {
      db_symbol(member_sym, "", 2);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return field;
}  /* decl_nonstatic_data_member */


static void scan_nonstatic_data_member(a_symbol_locator        *locator,
                                       a_class_def_state_ptr   class_state,
                                       a_member_decl_info_ptr  decl_info)
/*
Scan a nonstatic data member of a class, struct, or union.  Call
decl_nonstatic_data_member to create the field entry to represent it, etc.
*locator is the symbol locator for the declaration.  *class_state and
*decl_info track general information about the class definition and specific
information about the member declaration, respectively.
*/
{
  decl_info->is_bit_field = FALSE;
  /* A colon next indicates a bit-field. */
  if (curr_token == tok_colon) {
    decl_info->is_bit_field = TRUE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    decl_info->bit_field_size_pos = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Advance past the colon. */
    (void)get_token();
    /* Scan the integral size in bits of the bit-field. */
    scan_fs_integral_constant_expression(&decl_info->bit_field_size);
#if GNU_EXTENSIONS_ALLOWED
    if (gnu_attributes_enabled) {
      scan_gnu_declarator_attributes(&decl_info->decl_state);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    /* Update the end position of the declarator to include the bit field
       size construct. */
    decl_info->decl_pos_block.declarator_range.end =
                                            curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
  /* Create the IL for the field, enter the symbol (if needed), etc. */
  (void)decl_nonstatic_data_member(locator, class_state, decl_info,
                                   depth_scope_stack);
}  /* scan_nonstatic_data_member */


static void generate_special_function(a_class_def_state_ptr   class_state,
                                      a_member_decl_info_ptr  decl_info,
                                      a_param_type_ptr        ptp)
/*
Create a routine entry for a compiler generated constructor, destructor, or
assignment operator.  The created routine is a member function of the class
described by class_state.  If it has any parameter besides the implicit "this"
parameter (i.e., for a copy constructor or assignment operator), a non-NULL
param type pointer is passed in as ptp.  *decl_info tracks information about
the declaration, including whether a constructor, destructor, or assignment
operator should be created.  No routine body is generated at this time.
*/
{
  a_type_ptr                rout_type, class_type = class_state->class_type;
  a_routine_type_supplement *extra_info;
  a_symbol_locator          locator;
  a_func_info_block         func_info;
  a_source_position         *class_decl_pos;
  a_routine_ptr             routine;

  db_enter(3, "generate_special_function");
  /* Allocate and initialize the routine type entry for the function. */
  rout_type = alloc_type((a_type_kind)tk_routine);
  extra_info = rout_type->variant.routine.extra_info;
  if (decl_info->is_constructor) {
    /* Constructors are given a return type of void. */
    rout_type->variant.routine.return_type = void_type();
    extra_info->assoc_routine_is_ctor = TRUE;
  } else if (decl_info->is_destructor) {
    /* Destructors are given a return type of void. */
    rout_type->variant.routine.return_type = void_type();
    extra_info->assoc_routine_is_dtor = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cppcli_enabled && decl_info->is_static_constructor) {
    rout_type->variant.routine.return_type = void_type();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    /* Default assignment operators are given a return type of reference
       to class-type. */
    rout_type->variant.routine.return_type = make_reference_type(class_type);
  }  /* if */
  extra_info->param_type_list = ptp;
  extra_info->this_class = class_type;
  extra_info->prototyped = TRUE;
  if (ptp != NULL) {
    /* Set a flag in the param type entry if its associated type is or contains
       a template parameter. */
    set_parameter_list_template_param_flags(rout_type);
  }  /* if */
  /* Check whether the routine needs special support for returning a class
     object by value.  This call should be superfluous; it is included just
     to be safe, in case the rules change on when the flag needs to be set. */
  set_routine_calling_method_flag(rout_type, &null_source_position);
  decl_info->decl_state.type = rout_type;
  /* Create a locator for the symbol that will be created. */
  class_decl_pos = &class_type->source_corresp.decl_position;
  if (decl_info->is_constructor || decl_info->is_destructor) {
    a_symbol_ptr tag_sym = symbol_for(class_type);
    make_locator_for_symbol(tag_sym, &locator);
    if (decl_info->is_constructor) {
      change_class_locator_into_constructor_locator(&locator, class_decl_pos,
                                                    /*is_static_ctor=*/FALSE);
    } else {
      tildize_locator(&locator);
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cppcli_enabled && decl_info->is_static_constructor) {
    a_symbol_ptr tag_sym = symbol_for(class_type);
    make_locator_for_symbol(tag_sym, &locator);
    change_class_locator_into_constructor_locator(&locator, class_decl_pos,
                                                  /*is_static_ctor=*/TRUE);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    /* Must be an assignment operator. */
    make_opname_locator((an_opname_kind)onk_assign, &locator, class_decl_pos);
  }  /* if */
  clear_func_info(&func_info);
  func_info.is_inline = TRUE;
  if (exceptions_enabled) func_info.throw_position = *class_decl_pos;
  /* Create a symbol and enter it in the symbol table, and create a routine
     entry and add it to the routines list for the current scope. */
  decl_member_function(&locator, &func_info, class_state, decl_info,
                       /*compiler_generated=*/TRUE);
  done_with_func_info(func_info);
  /* It can be that the head of symbols list for the scope has been
     modified (it may have been changed to an sk_overloaded_function, or
     it may have been empty), so update the class symbol supplement, just to
     be safe. */
  (symbol_supplement_for_class(class_type))->symbols =
            assoc_pointers_block_of(&scope_stack[depth_scope_stack])->symbols;
  check_assertion(decl_info->decl_state.sym != NULL);
  routine = decl_info->decl_state.sym->variant.routine.ptr;
  if (instantiate_extern_inline && !routine->is_prototype_instantiation) {
    /* When inline functions are instantiated like templates, add the function
       to the list of inline functions if it is inline.  (Members of prototype
       instantiations don't need to be treated that way, of course.) */
    add_to_inline_function_list(
                              decl_info->decl_state.sym->variant.routine.ptr);
  }  /* if */
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
  a_boolean                   ambiguous;

  check_assertion(is_immediate_class_type(class_type));
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp->assoc_operator_new_routine == NULL) {
    /* Use the class "new" if there is one, and otherwise the global operator
       new. */
    sym = opname_member_function_symbol((an_opname_kind)onk_new, class_type);
    if (sym != NULL) {
      if (sym->ambiguous) {
        /* The inclusion of the operator new routine in the class type
           supplement is an optimization.  Don't use it if it's ambiguous. */
        sym = NULL;
      } else {
        /* There is a class-specific operator new() (or several).  See if
           there is a default (one-argument) version. */
        sym = find_default_operator_new_sym(sym, &ambiguous);
      }  /* if */
    } else {
      /* Look for a global operator new(). */
      sym = opname_function_symbol((an_opname_kind)onk_new);
      /* "new" can be overloaded; find the default (one-argument) version
         of the routine if so. */
      sym = find_default_operator_new_sym(sym, &ambiguous);
    }  /* if */
    if (sym != NULL) {
      a_routine_ptr     rp = sym->variant.routine.ptr;
      a_param_type_ptr  ptp = rp->type->
                                variant.routine.extra_info->param_type_list;
      if (ptp->next != NULL) {
        /* This operator new declaration must have a default argument.  It's
           more trouble than it's worth to deal with (setting this pointer
           is just an optimization, after all), so ignore this case. */
        check_assertion(ptp->next->has_default_arg);
      } else {
        ctsp->assoc_operator_new_routine = rp;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* set_class_assoc_operator_new_routine */

#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */

a_symbol_ptr find_class_assoc_operator_delete_routine(a_type_ptr class_type,
                                                      a_boolean  *ambiguous)
/*
Determine the default operator delete() function to be used for the
indicated class and return a pointer to its symbol.  Return NULL
if there is no visible default operator delete().  If the symbol
found is ambiguous, return it and also return *ambiguous TRUE.
*/
{
  a_symbol_ptr sym, other_sym;

  check_assertion(is_immediate_class_type(class_type));
  *ambiguous = FALSE;
  /* Use the class "delete" if there is one, and otherwise the global
     operator delete. */
  sym = opname_member_function_symbol((an_opname_kind)onk_delete,
                                      class_type);
  if (sym != NULL) {
    /* A member delete. */
    if (sym->ambiguous) {
      *ambiguous = TRUE;
    }  /* if */
  } else {
    sym = opname_function_symbol((an_opname_kind)onk_delete);
  }  /* if */
  check_assertion(sym != NULL);
  if (!*ambiguous) {
    /* Since delete might be overloaded, find the default version. */
    other_sym = find_default_operator_delete_sym(sym, ambiguous);
    if (!*ambiguous) sym = other_sym;
  }  /* if */
  return sym;
}  /* find_class_assoc_operator_delete_routine */

#if DELETE_CAN_BE_FOLDED_INTO_DTOR

void set_class_assoc_operator_delete_routine(a_type_ptr class_type)
/*
Determine the operator delete() function to be used for the indicated class
and record it in the class's assoc_operator_delete_routine field.
*/
{
  a_symbol_ptr                sym;
  a_class_type_supplement_ptr ctsp;
  a_boolean                   ambiguous;

  check_assertion(is_immediate_class_type(class_type));
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp->assoc_operator_delete_routine == NULL) {
    sym = find_class_assoc_operator_delete_routine(class_type, &ambiguous);
    if (sym != NULL && !ambiguous) {
      sym = fundamental_symbol_of(sym);
      ctsp->assoc_operator_delete_routine = sym->variant.routine.ptr;
    }  /* if */
  }  /* if */
}  /* set_class_assoc_operator_delete_routine */

#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */

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


static void check_base_or_mbr_class_type_for_suppression(
                                   a_type_ptr           class_type,
                                   a_type_ptr           base_or_mbr_type,
                                   a_type_qualifier_set asgn_qualifiers,
                                   a_type_qualifier_set ctor_qualifiers,
                                   a_boolean            check_all,
                                   a_boolean            *asgn_warning_needed,
                                   a_boolean            *ctor_warning_needed,
                                   a_boolean            *dtor_warning_needed,
                                   a_boolean            *suppress_copy_asgn_op,
                                   a_boolean            *suppress_copy_ctor,
                                   a_boolean            *suppress_dtor)
/*
This is a helper routine for check_microsoft_suppressed_special_functions.
It checks a base class or the class type of a member of class_type to see
if any errors exist that would prevent the successful generation of the
implicit definition of a copy assignment operator, copy constructor, or
destructor for the specified class_type.  The asgn_qualifiers and
ctor_qualifiers are the qualifiers on the parameter of the function, used
in looking up the corresponding function in the base or member class type;
the three "suppress" booleans are set to reflect whether an error would
occur in the definition or not.  If the corresponding "warning_needed" flag
is TRUE, a warning will be issued for each suppressed special function and
the flag updated to prevent repeated warnings.  If check_all is TRUE, all
checks will be run; otherwise, checking will be limited to the cases that
affect the behavior of the MSVC++ version indicated by microsoft_version.
*/
{
  a_class_symbol_supplement_ptr cssp;
  a_symbol_ptr                  rout_sym;
  a_boolean                     ambiguous;
  a_boolean                     pass_by_value;
  a_boolean                     bitwise_copy;

  if (base_or_mbr_type->variant.class_struct_union.
                                             copy_assignment_decl_suppressed) {
    /* A base or member with a suppressed copy assignment operator suppresses
       this one, too. */
    *suppress_copy_asgn_op = TRUE;
    if (*asgn_warning_needed) {
      *asgn_warning_needed = FALSE;
      pos_ty2_diagnostic(es_remark, ec_subobj_copy_asgn_decl_suppressed,
                         &class_type->source_corresp.decl_position,
                         class_type, base_or_mbr_type);
    }  /* if */
  } else if (microsoft_version < 1400 || check_all) {
    /* MSVC++ 8.0 issues an error for an inaccessible base or member copy
       assignment operator, while earlier versions suppress the containing
       class's copy assignment operator. */
    rout_sym = find_copy_assignment_operator(base_or_mbr_type, asgn_qualifiers,
                                             &ambiguous, &pass_by_value);
    if (ambiguous ||
        (rout_sym != NULL && !have_access_to_symbol(rout_sym))) {
      /* A base or member with an ambiguous or inaccessible copy assignment
         operator prevents this copy assignment operator from being
         generated. */
      *suppress_copy_asgn_op = TRUE;
      if (*asgn_warning_needed) {
        *asgn_warning_needed = FALSE;
        pos_ty2_diagnostic(es_remark, ambiguous ?
                           ec_ambig_suppresses_copy_asgn_decl :
                           ec_access_suppresses_copy_asgn_decl,
                           &class_type->source_corresp.decl_position,
                           class_type, base_or_mbr_type);
      }  /* if */
    }  /* if */
  }  /* if */
  if (microsoft_version < 1400 || check_all) {
    /* Microsoft versions before 8.0 had similar processing for copy
       constructors. */
    if (base_or_mbr_type->variant.class_struct_union.
                                                   copy_ctor_decl_suppressed) {
      /* A base or member with a suppressed copy constructor suppresses
         this one, too. */
      *suppress_copy_ctor = TRUE;
      if (*ctor_warning_needed) {
        *ctor_warning_needed = FALSE;
        pos_ty2_diagnostic(es_remark, ec_subobj_copy_ctor_decl_suppressed,
                           &class_type->source_corresp.decl_position,
                           class_type, base_or_mbr_type);
      }  /* if */
    } else {
      rout_sym = find_copy_constructor(
                               base_or_mbr_type, ctor_qualifiers,
                               /*source_is_rvalue=*/FALSE,
                               &base_or_mbr_type->source_corresp.decl_position,
                               &ambiguous, &bitwise_copy);
      if (ambiguous ||
          (rout_sym != NULL && !have_access_to_symbol(rout_sym))) {
        /* A base or member with an ambiguous or inaccessible copy
           constructor prevents this one from being generated. */
        *suppress_copy_ctor = TRUE;
        if (*ctor_warning_needed) {
          *ctor_warning_needed = FALSE;
          pos_ty2_diagnostic(es_remark, ambiguous ?
                             ec_ambig_suppresses_copy_ctor_decl :
                             ec_access_suppresses_copy_ctor_decl,
                             &class_type->source_corresp.decl_position,
                             class_type, base_or_mbr_type);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* Now check for the destructor's accessibility. */
  cssp = symbol_supplement_for_class(base_or_mbr_type);
  if (cssp->destructor != NULL &&
      !have_access_to_symbol(cssp->destructor)) {
    /* An inaccessible base or member destructor prevents this one from
       being generated. */
    *suppress_dtor = TRUE;
    if (*dtor_warning_needed) {
      *dtor_warning_needed = FALSE;
      /* Note that this diagnostic is a warning, not a remark like the
         other diagnostics for suppressed functions.  Because MSVC++ issues
         a warning only for this case, issuing warnings for the other cases
         would likely result in a lot of unwanted diagnostic output for
         code that compiles silently under MSVC++. */
      pos_ty2_diagnostic(es_warning, ec_access_prevents_dtor_generation,
                         &class_type->source_corresp.decl_position,
                         class_type, base_or_mbr_type);
    }  /* if */
  }  /* if */
}  /* check_base_or_mbr_class_type_for_suppression */


static void check_microsoft_suppressed_special_functions(
                                   a_type_ptr           class_type,
                                   a_type_qualifier_set asgn_qualifiers,
                                   a_type_qualifier_set ctor_qualifiers,
                                   a_boolean            check_all,
                                   a_boolean            asgn_warning_needed,
                                   a_boolean            ctor_warning_needed,
                                   a_boolean            dtor_warning_needed,
                                   a_boolean            *suppress_copy_asgn_op,
                                   a_boolean            *suppress_copy_ctor,
                                   a_boolean            *suppress_dtor)
/*
Check to see if any errors exist that would prevent the successful
generation of the implicit definition of a copy assignment operator, copy
constructor, or destructor for the specified class_type.  This is called in
Microsoft mode to emulate the behavior of the Microsoft compiler, which
suppresses the declaration of these member functions in case of an error,
allowing overload resolution to select a non-copy constructor or assignment
operator for copy operations and preventing errors resulting from an unused
destructor.  The asgn_qualifiers and ctor_qualifiers are the qualifiers on
the parameter of the function, used in looking up the corresponding
function in the members and bases; the three "suppress" booleans are set to
reflect whether an error would occur in the definition or not.  A warning
will be issued for each suppressed special function if the corresponding
"warning_needed" flag is TRUE.  If check_all is TRUE, all checks will be
run; otherwise, checking will be limited to the cases that affect the
behavior of the MSVC++ version indicated by microsoft_version.
*/
{
  a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(class_type);
  a_symbol_ptr                  sym;
  a_base_class_ptr              bcp;
  a_type_ptr                    tp;

  *suppress_copy_asgn_op = FALSE;
  *suppress_copy_ctor = FALSE;
  *suppress_dtor = FALSE;
  /* First, scan through all the nonstatic data members, using the symbol
     list rather than the field list to be sure that only user-defined
     fields are checked and to be sure that anonymous union fields are
     picked up. */
  for (sym = cssp->symbols; sym != NULL; sym = sym->next_in_scope) {
    if (sym->kind == (a_symbol_kind)sk_field &&
        /* Property fields and events do not affect the special member
           functions. */
        !field_is_property_or_event(sym->variant.field.ptr)) {
      tp = sym->variant.field.ptr->type;
      if (is_array_type(tp)) {
        tp = underlying_array_element_type(tp);
      }  /* if */
      if (is_const_qualified_type(tp)) {
        /* A nonstatic data member with const-qualified type prevents the
           copy assignment operator from being generated. */
        *suppress_copy_asgn_op = TRUE;
        if (asgn_warning_needed) {
          asgn_warning_needed = FALSE;
          pos_syty_diagnostic(es_remark,
                              ec_const_mbr_suppresses_copy_asgn_decl,
                              &class_type->source_corresp.decl_position,
                              sym, class_type);
        }  /* if */
      } else if (is_any_reference_type(tp)) {
        /* A nonstatic data member with reference type prevents the copy
           assignment operator from being generated. */
        *suppress_copy_asgn_op = TRUE;
        if (asgn_warning_needed) {
          asgn_warning_needed = FALSE;
          pos_syty_diagnostic(es_remark,
                              ec_ref_mbr_suppresses_copy_asgn_decl,
                              &class_type->source_corresp.decl_position,
                              sym, class_type);
        }  /* if */
      }  /* if */
      if (is_class_struct_union_type(tp)) {
        /* Check to see if the special member functions of the member's
           class type would prevent the corresponding functions from being
           generated. */
        check_base_or_mbr_class_type_for_suppression(class_type,
                                                     skip_typerefs(tp),
                                                     asgn_qualifiers,
                                                     ctor_qualifiers,
                                                     check_all,
                                                     &asgn_warning_needed,
                                                     &ctor_warning_needed,
                                                     &dtor_warning_needed,
                                                     suppress_copy_asgn_op,
                                                     suppress_copy_ctor,
                                                     suppress_dtor);
      }  /* if */
    }  /* if */
  }  /* for */
  /* Now scan through all the direct base classes of this class. */
  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
    if (bcp->direct  &&
        !(bcp->is_virtual &&
          virtual_base_class_is_indirect(bcp, class_type))) {
      /* This is a direct base class; check to see if its special member
         functions would prevent the corresponding functions from being
         generated. */
      check_base_or_mbr_class_type_for_suppression(class_type, bcp->type,
                                                   asgn_qualifiers,
                                                   ctor_qualifiers,
                                                   check_all,
                                                   &asgn_warning_needed,
                                                   &ctor_warning_needed,
                                                   &dtor_warning_needed,
                                                   suppress_copy_asgn_op,
                                                   suppress_copy_ctor,
                                                   suppress_dtor);
    }  /* if */
  }  /* for */
}  /* check_microsoft_suppressed_special_functions */


static void generate_default_constructor(a_class_def_state_ptr  class_state,
                                         a_boolean              is_deleted)
/*
Add a declaration for a default constructor to the class definition described
by class_state.  If is_deleted is TRUE, make that constructor "deleted".
*/
{
  a_type_ptr          class_type = class_state->class_type;
  a_member_decl_info  decl_info;

  initialize_member_decl_info(&decl_info,
                              &class_type->source_corresp.decl_position);
  decl_info.is_constructor = TRUE;
  if (!class_state->default_ctor_is_nontrivial && !is_deleted) {
    /* We are generating a declaration of a trivial default constructor.
       Since it will never actually be called it gets special handling. */
    decl_info.is_trivial_default_constructor = TRUE;
  }  /* if */
  generate_special_function(class_state, &decl_info, (a_param_type*)NULL);
  if (is_deleted) {
    a_symbol_ptr  sym = decl_info.decl_state.sym;
    sym->defined = TRUE;
    sym->variant.routine.ptr->is_deleted = TRUE;
    sym->variant.routine.ptr->defined = TRUE;
  }  /* if */
}  /* generate_default_constructor */


static void add_default_ctor_if_needed(a_class_def_state_ptr  class_state)
/*
If appropriate, add an implicitly declared default constructor to the class
definition described by class_state.
*/
{
  a_type_ptr                 class_type = class_state->class_type;
  a_class_symbol_supplement  *cssp = symbol_supplement_for_class(class_type);

  if (cssp->trivial_default_constructor != NULL) {
    /* A defaulted default constructor, which was initially assumed to be
       trivial.  Check if it is indeed trivial now that the whole class has
       been processed.  If not, make the necessary adjustments. */
    a_symbol_ptr  default_ctor = cssp->trivial_default_constructor;
    check_assertion(default_ctor->variant.routine.ptr->is_defaulted);
    if (class_state->default_ctor_is_nontrivial) {
      cssp->trivial_default_constructor = NULL;
      cssp->has_nontrivial_default_constructor = TRUE;
      default_ctor->variant.routine.ptr
                  ->is_trivial_default_constructor = FALSE;
    }  /* if */
  }  /* if */
  if (cssp->constructor == NULL) {
    /* See if a default constructor declaration is needed. */
    if (!class_state->POD_ruled_out) {
      /* This is a POD class.  Its implicitly-declared default constructor
         need not actually be generated. */
    } else if (class_type_supp(class_type)->is_lambda_closure_class) {
      /* A deleted constructor was already declared (but not recorded in
         cssp->constructor if it was trivial). */
    } else {
      /* A default constructor needs to be generated. */
      generate_default_constructor(class_state, /*is_deleted=*/FALSE);
    }  /* if */
  } else if (!cssp->has_user_declared_default_constructor) {
    /* This class has a user-declared or nontrivial constructor (since
       cssp->constructor != NULL), but no user-declared default constructor
       (and hence no explicitly-defaulted default constructor).  So it cannot
       be a "trivial class" and therefore it cannot be POD. */ 
    class_state->POD_ruled_out = TRUE;
  }  /* if */
}  /* add_default_ctor_if_needed */


static void generate_assignment_operator(a_class_def_state_ptr  class_state,
                                         a_boolean              is_deleted,
                                         a_type_qualifier_set   qualifiers)
/*
Add a declaration for a copy assignment operator to the class definition
described by class_state.  If is_deleted is TRUE, make that operator "deleted".
The parameter of the assignment operator is of type X& (where X is the possibly
qualified parent class type) and qualifiers describes the qualifiers in X.
(In some modes, a second operator is declared to handle "far" objects.)
*/
{
  a_type_ptr          class_type = class_state->class_type;
  a_source_position   *pos = &class_type->source_corresp.decl_position;
  a_member_decl_info  decl_info;
  a_param_type_ptr    ptp;
  a_type_ptr          ptype;

  initialize_member_decl_info(&decl_info, pos);
  ptype = make_qualified_type(class_type, qualifiers);
  ptp = alloc_param_type(make_reference_type(ptype));
  generate_special_function(class_state, &decl_info, ptp);
  if (is_deleted) {
    a_symbol_ptr  sym = decl_info.decl_state.sym;
    sym->defined = TRUE;
    sym->variant.routine.ptr->is_deleted = TRUE;
    sym->variant.routine.ptr->defined = TRUE;
  }  /* if */
#if NEAR_AND_FAR_ALLOWED
  if (near_and_far_enabled()) {
    /* Generate also an operator= that can copy a "far" object. */
    a_type_ptr       far_ptype = make_qualified_type(class_type,
                                                     TQ_CONST|TQ_FAR);
    /* Don't create the "far" operator= if the default one is "far" (e.g.,
       because the class is declared "far"). */
    if (!identical_types(far_ptype, ptype)) {
      ptp = alloc_param_type(make_reference_type(far_ptype));
      initialize_member_decl_info(&decl_info, pos);
      generate_special_function(class_state, &decl_info, ptp);
    }  /* if */
    if (is_deleted) {
      a_symbol_ptr  sym = decl_info.decl_state.sym;
      sym->defined = TRUE;
      sym->variant.routine.ptr->is_deleted = TRUE;
      sym->variant.routine.ptr->defined = TRUE;
    }  /* if */
  }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
}  /* generate_assignment_operator */


static void mark_trivial_copy_functions(a_class_def_state_ptr  class_state)
/*
Set the is_trivial_copy_function flag to TRUE for every trivial copy
constructor routine or trivial copy assignment routine of the class described
by class_state.  (These are compiler-generated or explicitly-defaulted
member functions.)
*/
{
  a_type_ptr  class_type = class_state->class_type;
  a_class_symbol_supplement_ptr
              cssp = symbol_supplement_for_class(class_type);

  /* At this point, cssp->assignment_by_bitwise_copy_allowed and
     cssp->construction_by_bitwise_copy_allowed only reflect whether the
     class' subcomponents are bitwise copyable (that includes the fact that
     e.g. virtual function table pointers are not bitwise copyable).  The
     flags do not yet reflect the presence of user-provided copy constructors
     or user-provided copy assignment operators. */
  if (cssp->assignment_by_bitwise_copy_allowed ||
      cssp->construction_by_bitwise_copy_allowed) {
    /* Trivial copying is possible: Traverse the member to find defaulted or
       compiler-generated copy constructors and copy assignment operators. */
    a_routine_ptr  rp = class_type_supp(class_type)->assoc_scope->routines;
    for (; rp != NULL; rp = rp->next) {
      if (rp->compiler_generated || rp->is_defaulted) {
        a_type_qualifier_set  tqs;
        if (rp->special_kind == (a_special_function_kind)sfk_constructor &&
            is_copy_constructor(rp, (a_type*)NULL, &tqs,
                                /*include_move_ctors=*/TRUE,
                                /*is_declarative_context=*/TRUE)) {
          /* The call to is_copy_constructor could set the include_move_ctors
             parameter TRUE or FALSE in this case.  We choose TRUE to allow an
             additional consistency check: A move constructor is never
             defaulted or compiler-generated. */
          check_assertion(!copy_ctor_is_move_ctor(rp));
          rp->is_trivial_copy_function =
                                   cssp->construction_by_bitwise_copy_allowed;
        } else if (rp->special_kind == (a_special_function_kind)sfk_operator &&
                   rp->variant.opname_kind == (an_opname_kind)onk_assign) {
          rp->is_trivial_copy_function =
                                     cssp->assignment_by_bitwise_copy_allowed;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* mark_trivial_copy_functions */


static void check_special_member_functions(a_type_ptr            class_type,
                                           a_class_def_state_ptr class_state)

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
  a_class_type_supplement_ptr   ctsp;
  a_boolean                     const_okay, dummy_flag;
  a_type_qualifier_set          ctor_qualifiers;
  a_type_qualifier_set          asgn_qualifiers;
  a_member_decl_info            decl_info;
  a_source_position             *pos;
  a_boolean                     user_declared_copy_assignment_op;
  a_boolean                     user_provided_copy_assignment_op;
  a_boolean                     suppress_copy_asgn_op = FALSE;
  a_boolean                     suppress_copy_ctor = FALSE;
  a_boolean                     suppress_dtor = FALSE;
  a_boolean                     declare_copy_asgn_op;
  a_boolean                     declare_copy_ctor;
  a_boolean                     declare_dtor;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_boolean                     declare_static_ctor;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_enter(3, "check_special_member_functions");
  cssp = symbol_supplement_for_class(class_type);
  ctsp = class_type_supp(class_type);
  pos = &class_type->source_corresp.decl_position;
  /* Check for a user-declared copy assignment or move assignment operator. */
  user_declared_copy_assignment_op = assignment_operator_for_copy_exists(
                                          cssp->assignment_operator,
                                          rvalue_ctor_is_copy_ctor,
                                          &user_provided_copy_assignment_op,
                                          &dummy_flag);
  if (user_provided_copy_assignment_op) {
    /* A POD cannot have a user-provided copy assignment operator.  This must
       be determined before calling add_default_ctor_if_needed, because it
       may affects whether a trivial default constructor is actually
       generated (it wouldn't be generated for a POD). */
    class_state->POD_ruled_out = TRUE;
  }  /* if */
  add_default_ctor_if_needed(class_state);
  const_okay = default_assignment_of_const_object_okay(class_type);
  asgn_qualifiers = const_okay ? TQ_CONST : TQ_NONE;
  default_copy_constructor_check(class_type, &const_okay);
  ctor_qualifiers = const_okay ? TQ_CONST : TQ_NONE;
  declare_copy_asgn_op = !user_declared_copy_assignment_op &&
                     (!any_cfront_mode() || cssp->assignment_operator == NULL);
  /* If no copy constructor has been declared, we generally declare one
     implicitly.  An exception occurs for classes that are trivially copyable,
     provided there are no other constructors (in which case the trivial copy
     constructor must be represented so it can compete in overload resolution)
     and the class is not a closure type. */
  declare_copy_ctor = !cssp->has_copy_constructor &&
                      (ctsp->is_lambda_closure_class ||
                       cssp->constructor != NULL ||
                       !cssp->construction_by_bitwise_copy_allowed);
  declare_dtor = (class_state->member_destruction_required ||
                  class_state->base_destruction_required) &&
                 cssp->destructor == NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  declare_static_ctor = cppcli_enabled &&
                        !cli_class_type_kind_is(class_type, cctk_standard) &&
                        !cli_class_type_kind_is(class_type, cctk_interface) &&
                        cssp->static_constructor == NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (microsoft_mode && !is_prototype_instantiation_context() &&
      (declare_copy_asgn_op || declare_copy_ctor || declare_dtor)) {
    /* The Microsoft compiler does not implicitly declare some special
       member functions if their definition would have errors.  Check to
       see if these member function declarations should be suppressed. */
    check_microsoft_suppressed_special_functions(class_type, asgn_qualifiers,
                                                 ctor_qualifiers,
                                                 /*check_all=*/FALSE,
                                                 declare_copy_asgn_op,
                                                 declare_copy_ctor,
                                                 declare_dtor,
                                                 &suppress_copy_asgn_op,
                                                 &suppress_copy_ctor,
                                                 &suppress_dtor);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (declare_static_ctor) {
    /* In C++/CLI reference and value classes, generate an implicit static
       constructor if none was declared explicitly. */
    initialize_member_decl_info(&decl_info, pos);
    decl_info.decl_state.storage_class = (a_storage_class)sc_static;
    decl_info.is_static_constructor = TRUE;
    generate_special_function(class_state, &decl_info, (a_param_type_ptr)NULL);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (declare_copy_ctor) {
    if (suppress_copy_ctor) {
      /* Mark this class as having a suppressed copy constructor and do not
         add its declaration. */
      class_type->variant.class_struct_union.copy_ctor_decl_suppressed = TRUE;
    } else {
      /* Generate a copy constructor. */
      ptp = alloc_param_type(make_reference_type(
                            make_qualified_type(class_type, ctor_qualifiers)));
      initialize_member_decl_info(&decl_info, pos);
      decl_info.is_constructor = TRUE;
      generate_special_function(class_state, &decl_info, ptp);
    }  /* if */
  }  /* if */
  if (declare_dtor) {
    if (suppress_dtor) {
      /* Mark the class as having a suppressed destructor and do not add
         the declaration. */
      class_type->variant.class_struct_union.dtor_decl_suppressed = TRUE;
    } else {
      /* Add the declaration of the destructor. */
      initialize_member_decl_info(&decl_info, pos);
      decl_info.is_destructor = TRUE;
      generate_special_function(class_state, &decl_info,
                                (a_param_type_ptr)NULL);
    }  /* if */
  }  /* if */
  /* Record whether the destructor is "trivial".  Usually, this means that
     cssp->destructor is NULL, but it could also be a defaulted destructor
     with no effect. */
  if (cssp->destructor == NULL ||
      (cssp->destructor->variant.routine.ptr->is_defaulted &&
       !cssp->destructor->variant.routine.ptr->is_virtual &&
       !class_state->member_destruction_required &&
       !class_state->base_destruction_required)) {
    cssp->has_trivial_destructor = TRUE;
  }  /* if */
  /* Create a default assignment operator to copy an object of the current
     class if one doesn't already exist.  Note that in cfront mode, the
     presence of any assignment operator suppresses the creation of
     a default assignment operator. */
  if (declare_copy_asgn_op) {
    /* An implicit assignment operator is generated if the class does not
       contain a user-declared copy assignment operator. */
    if (suppress_copy_asgn_op) {
      /* Mark this class as having a suppressed copy assignment operator and
         do not add its declaration. */
      class_type->variant.class_struct_union.copy_assignment_decl_suppressed
                                                                        = TRUE;
    } else {
      /* Add the implicit declaration of the copy assignment operator. */
      generate_assignment_operator(class_state, /*is_deleted=*/FALSE,
                                   asgn_qualifiers);
    }  /* if */
  }  /* if */
  mark_trivial_copy_functions(class_state);
  /* If there were user-provided copy constructors and/or user-provided copy
     assignment operators, set construction_by_bitwise_copy_allowed and/or
     assignment_by_bitwise_copy_allowed to FALSE.  This must happen after
     the call to mark_trivial_copy_functions. */
  if (cssp->has_user_provided_copy_constructor) {
    cssp->construction_by_bitwise_copy_allowed = FALSE;
  }  /* if */
  if (user_provided_copy_assignment_op) {
    cssp->assignment_by_bitwise_copy_allowed = FALSE;
  }  /* if */
  db_exit();
}  /* check_special_member_functions */


static void check_base_class_destructors(a_class_def_state_ptr  class_state)
/*
Issue a remark in some situations where class_type is a class derived from a
base class with no virtual destructor.  Only truly suspect cases are diagnosed
(e.g., no diagnostic is issued if none of the subobjects of class_type have a
nontrivial destructor).
*/
{
  a_type_ptr        class_type = class_state->class_type;
  a_base_class_ptr  bcp;

  if ((int)error_threshold > (int)es_remark) {
    /* Only a remark would have been issued.  Don't bother checking for a
       diagnosable condition. */
  } else if ((bcp = base_classes_of(class_type)) != NULL) {
    /* The derived class has base classes.  We issue a remark only if a base
       does not have a virtual destructor and at least one of the following
       is true:
           - the derived class has a user-provided destructor
           - a member added by the derivation requires destruction
           - another direct nondependent base has a nontrivial destructor
       This may require two passes through the list of base classes.  */
    a_symbol_ptr  dtor_sym;
    a_class_symbol_supplement_ptr
                  cssp = symbol_supplement_for_class(class_type);
    a_boolean     issue_remarks = (cssp->destructor != NULL ||
                                   class_state->member_destruction_required);
    if (!issue_remarks && class_state->base_destruction_required) {
      /* The derivation itself does not require a destructor, but at least
         one base does.  Compare the number of direct bases with the number
         of virtual destructors in those bases to decide whether remarks are
         warranted. */
      unsigned long  base_count = 0, virtual_dtor_count = 0;
      for (; bcp != NULL; bcp = bcp->next) {
        if (bcp->direct &&
            (!bcp->ignore_during_dependent_lookup ||
             !is_template_dependent_type(bcp->type))) {            
          dtor_sym = symbol_supplement_for_class(bcp->type)->destructor;
          if (dtor_sym != NULL && dtor_sym->variant.routine.ptr->is_virtual) {
            ++virtual_dtor_count;
          }  /* if */
          ++base_count;
        }  /* if */
      }  /* for */
      issue_remarks = base_count > 1 && base_count > virtual_dtor_count;
    }  /* if */
    if (issue_remarks) {
      /* We know there are suspect direct base classes with no virtual
         destructors: Issue a remark on each one. */
      for (; bcp != NULL; bcp = bcp->next) {
        if (bcp->direct &&
            (!bcp->ignore_during_dependent_lookup ||
             !is_template_dependent_type(bcp->type))) {            
          dtor_sym = symbol_supplement_for_class(bcp->type)->destructor;
          if (dtor_sym == NULL ||
              !dtor_sym->variant.routine.ptr->is_virtual) {
            pos_sy_remark(ec_base_class_with_nonvirtual_dtor,
                          &bcp->decl_position, symbol_for(bcp->type));
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* check_base_class_destructors */


static a_boolean is_template_conversion_to_same_type(
						a_symbol_ptr	sym1,
						a_symbol_ptr	sym2)
/*
Return TRUE if sym1 and sym2 are conversion templates that convert to the
same type.
*/
{
  a_type_ptr	tp1, tp2;
  a_boolean	result;
  sym1 = fundamental_symbol_of(sym1);
  check_assertion(sym1->kind == (a_symbol_kind)sk_function_template);
  tp1 = sym1->variant.template_info->variant.function.
                                    routine->type->variant.routine.return_type;
  sym2 = fundamental_symbol_of(sym2);
  check_assertion(sym2->kind == (a_symbol_kind)sk_function_template);
  tp2 = sym2->variant.template_info->variant.function.
                                    routine->type->variant.routine.return_type;
  /* Nesting depths are ignored for this comparison because "operator T()"
     and "operator X()" should be considered identical even if one is
     more deeply nested than the other. */
  result = f_identical_types(tp1, tp2, ITF_IGNORE_NESTING_DEPTH);
  return result;
}  /* is_template_conversion_to_same_type */


static void check_base_class_conversion_list(a_type_ptr       class_type,
                                             a_base_class_ptr base_class,
                                             a_boolean        is_template_list,
                                             a_boolean        *updated)
/*
For each entry on the conversion list of the indicated base class (or, if
is_template_list is TRUE, on the conversion template list), look for an
overriding conversion function on the corresponding list of class_type.  If
none is found, create a projection symbol for it and add it to the list of
class_type.  Set *updated if a projection symbol is created.
*/
{
  a_class_symbol_supplement_ptr cssp, bcssp;
  a_symbol_list_entry_ptr       slep, bcslep;
  a_symbol_ptr                  sym;

  bcssp = symbol_supplement_for_class(base_class->type);
  bcslep = is_template_list ? bcssp->conversion_template_list :
                              bcssp->conversion_list;
  if (bcslep != NULL) {
    cssp = symbol_supplement_for_class(class_type);
    for (; bcslep != NULL; bcslep = bcslep->next) {
      /* Compare the conversion list entry from the base class with each
         conversion list entry for the current class.  They convert to the
         same type if they have the same header. */
      slep = is_template_list ? cssp->conversion_template_list :
                                cssp->conversion_list;
      for (; slep != NULL; slep = slep->next) {
        if (slep->symbol->header == bcslep->symbol->header ||
            (is_template_list &&
             is_template_conversion_to_same_type(slep->symbol,
                                                 bcslep->symbol))) {
          /* A conversion to the same type.  If this is from the current class
             (i.e., it is not a projection symbol) we should ignore the one
             from the base class.  If the entry on the current class list
             is a projection to the same routine as the symbol from
             the base class, it can also be ignored. */
          if (slep->symbol->kind != (a_symbol_kind)sk_projection) {
            /* The symbol is from the current class.  Ignore the base
               symbol. */
            break;
          } else {
            /* A projection symbol.  Ignore this entry if it refers to the
	       same function or template as one already on the list. */
            a_symbol_ptr	fund_curr_sym =
                                           fundamental_symbol_of(slep->symbol);
            a_symbol_ptr	fund_base_sym =
                                         fundamental_symbol_of(bcslep->symbol);
            if (fund_curr_sym->kind == (a_symbol_kind)sk_function_template) {
              if (same_entities(fund_curr_sym->variant.template_info->
                                         il_template_entry->canonical_template,
                                fund_base_sym->variant.template_info->
                                     il_template_entry->canonical_template)) {
                break;
              }  /* if */
            } else {
              if (same_entities(fund_curr_sym->variant.routine.ptr,
                                fund_base_sym->variant.routine.ptr)) {
                break;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
      if (slep == NULL) {
        /* A new destination type for conversion.  Create a symbol to
           represent its projection into the current class and record it in
           a new conversion list entry.  Note that we do not mark the symbol
           ambiguous even if similar conversions are projected from different
           base classes because conversion functions are not looked up by
           name (and sym->ambiguous is meant to denote name lookup
           ambiguity). */
        a_symbol_ptr      fund_sym = fundamental_symbol_of(bcslep->symbol);
        a_type_ptr        fund_base_type = sym_parent_class(fund_sym);
        a_base_class_ptr  fund_base = find_base_with_type(fund_base_type,
                                                          class_type,
                                                          base_class);
        sym = make_projection_symbol(bcslep->symbol, class_type, fund_base,
                                     /*path=*/(a_derivation_step*)NULL,
                                     /*ambiguous=*/FALSE);
        sym->variant.projection.access =
                            compute_access(access_for_symbol(bcslep->symbol),
                                           base_class->derivation->access);
        sym->variant.projection.any_intervening_using_decl =
               bcslep->symbol->variant.projection.any_intervening_using_decl;
        /* Allocate the new conversion list entry and link it in the
           list for the current class. */
        add_to_conversion_list(sym, cssp);
        *updated = TRUE;
      }  /* if */
    }  /* for */
  }  /* if */
}  /* check_base_class_conversion_list */


static void project_base_class_conversion_functions(a_type_ptr class_type)
/*
Go through all the direct base classes of the current class class_type and
create projection symbols to represent inherited conversion functions.  Also
create a symbol_list_entry for each new projection symbol and link it to
the list for the current class.  Only create a new projection symbol if the
destination type is not yet on the current class's conversion list.
*/
{
  a_base_class_ptr  bcp;
  a_boolean         updated = FALSE;

  db_enter(4, "project_base_class_conversion_functions");
  /* Examine each direct base class. */
  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
    if (bcp->direct) {
      /* Examine each conversion list entry in the base class. */
      check_base_class_conversion_list(class_type, bcp,
                                       /*is_template_list=*/FALSE, &updated);
      check_base_class_conversion_list(class_type, bcp,
                                       /*is_template_list=*/TRUE, &updated);
    }  /* if */
  }  /* for */
  if (updated) {
    /* Since the scope symbol list may have been empty before and since
       at least one new symbol has been added, update the symbols list
       attached to the class. */
    symbol_supplement_for_class(class_type)->symbols =
          assoc_pointers_block_of(&scope_stack[depth_scope_stack])->symbols;
  }  /* if */
#if DEBUG
  if (debug_level >= 3) {
    a_class_symbol_supplement_ptr cssp = 
                                   symbol_supplement_for_class(class_type);
    a_symbol_list_entry_ptr       slep = cssp->conversion_list;

    fputs("conversion list for ", f_debug);
    db_type_name(class_type);
    fprintf(f_debug, ": %s\n", slep == NULL ? "NULL" : "");
    for (; slep != NULL; slep = slep->next) {
      db_symbol(slep->symbol, "  ", 4);
    }  /* for */
    slep = cssp->conversion_template_list;
    fputs("conversion template list for ", f_debug);
    db_type_name(class_type);
    fprintf(f_debug, ": %s\n", slep == NULL ? "NULL" : "");
    for (; slep != NULL; slep = slep->next) {
      db_symbol(slep->symbol, "  ", 4);
    }  /* for */
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* project_base_class_conversion_functions */


static a_boolean is_duplicate_member_using_decl(a_symbol_ptr       sym,
                                                a_source_position  *err_pos)
/*
Check for a duplicate class member using declaration.  sym represents the
base-class member resulting from the current using declaration.  If this
declaration does duplicate a previous using declaration, issue a diagnostic
(using err_pos as the position at which to report the problem) and return
TRUE.
*/
{
  a_using_decl_ptr         udp;
  a_scope_ptr              sp = scope_stack[depth_scope_stack].il_scope;
  a_boolean                is_duplicate = FALSE;
  a_source_correspondence  *scp;

  check_assertion(sp != NULL &&
                  sp->kind == (a_scope_kind)sck_class_struct_union);
  /* Using declarations are recorded in the scope for the class. */
  udp = sp->using_decls;
  /* Traverse the list looking for a name and qualifier match. */
  for (; udp != NULL; udp = udp->next) {
    if (same_entities(udp->qualifier.class_type, sym_parent_class(sym))) {
      scp = source_corresp_for_il_entry(udp->entity.ptr,
                                        (an_il_entry_kind)udp->entity.kind);
      check_assertion(scp != NULL);
      if (((a_symbol_ptr)scp->assoc_info)->header == sym->header) {
        /* This must be a duplicate.  In strict mode issue an error (see
           7.3.3 para 8); otherwise issue a lesser diagnostic. */
        pos_sy_diagnostic(strict_ansi_mode ?
                            strict_ansi_discretionary_severity : es_warning,
                          ec_duplicate_using_decl, err_pos, sym);
        is_duplicate = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return is_duplicate;
}  /* is_duplicate_member_using_decl */


static a_boolean has_dependent_base_class(a_type_ptr  class_type)
/*
Return TRUE if the given class has a dependent base class.
*/
{
  a_boolean  result = FALSE;

  class_type = skip_typerefs(class_type);
  if (class_type->variant.class_struct_union.is_prototype_instantiation) {
    result = symbol_supplement_for_class(class_type)
                                                 ->any_dependent_base_classes;
  }  /* if */
  return result;
}  /* has_dependent_base_class */


static void create_member_using_declaration(
                                          a_symbol_ptr         sym,
                                          a_symbol_ptr         declared_sym,
                                          a_symbol_ptr         *other_sym,
                                          a_base_class_ptr     bcp,
                                          a_boolean            dummy_base,
                                          a_type_ptr           class_type,
                                          a_using_decl_ptr     *prev_udp,
                                          an_access_specifier  access)
/*
Check that a valid explicit projection (a class-scope using-declaration) can
be created for the symbol "sym", and if so create it.  "declared_sym" is
either equal to "sym", or it points to the overload set symbol of which "sym"
is an element.  "*other_sym" points to a declaration of the same name in the
scope of the derived class "class_type" (NULL if none exists).  "bcp" is the
base class from which the symbol is being projected (or, when "dummy_base" is
TRUE, a dummy entry created only to represent the using-declaration in a
prototype instantiation).  "*prev_udp" is the previous a_using_decl structure
created for the using-declaration that is currently being processed.  "access"
is the access specifier applicable to the new declaration.
*/
{
  a_symbol_ptr       fund_sym = fundamental_symbol_of(sym);
  a_source_position  decl_pos;

  decl_pos = locator_for_curr_id.source_position;
  if (!have_access_to_symbol(sym)) {
    /* The specified symbol (either the explicitly declared symbol or
       a member of the overload set the symbol refers to) is inaccessible.
       Issue an error instead of creating the projection symbol. */
    pos_sy_error(ec_no_access_to_name, &decl_pos, sym);
  } else if (sym->kind == (a_symbol_kind)sk_member_function &&
             sym->variant.routine.ptr->compiler_generated &&
             !(sym->variant.routine.ptr->special_kind ==
                                      (a_special_function_kind)sfk_operator &&
               sym->variant.routine.ptr->variant.opname_kind ==
                                                (an_opname_kind)onk_assign)) {
    /* Ignore compiler-generated member functions silently (except assignment
       operators). */
  } else if (*other_sym != NULL &&
             conflicts_with_previous_function_decl(fund_sym, *other_sym,
                                                   &decl_pos)) {
    /* Error (if one was required) was issued by subroutine.  Don't
       enter a projection symbol. */
  } else {
    a_using_decl_ptr  udp;
    /* Find the base class of class_type to which fund_sym belongs.  bcp
       points to the base class to which declared_sym belongs. */
    a_symbol_ptr      new_sym;
    a_base_class_ptr  fund_base_class;
    a_routine_ptr     rp = NULL;

    if (dummy_base) {
      /* Don't attempt to find an actual base class that is referred to; just
         use bcp (which is a dummy entry created only to represent this
         using-declaration). */
      fund_base_class = bcp;
    } else if (fund_sym == declared_sym ||
               same_entities(sym_parent_class(fund_sym),
                             sym_parent_class(declared_sym))) {
      /* Common case: the fundamental symbol is the same as the declared
         symbol, or a member of the overload set it represents. */
      fund_base_class = bcp;
    } else {
      /* Special case:  Find the base class associated with the
         fundamental symbol. */
      fund_base_class = base_classes_of(class_type);
      for (;;) {
        if (same_entities(fund_base_class->type,
                          sym_parent_class(fund_sym)) &&
            is_on_any_derivation_of(fund_base_class, bcp)) break;
        fund_base_class = fund_base_class->next;
        check_assertion(fund_base_class != NULL);
      }  /* for */
    }  /* if */
    /* Create the projection symbol. */
    new_sym = make_projection_symbol(sym, class_type, fund_base_class,
                                     (a_derivation_step_ptr)NULL,
                                     /*ambiguous=*/FALSE);
    new_sym->variant.projection.is_using_decl = TRUE;
    new_sym->variant.projection.access = access;
    /* Note that projection symbols for using-declarations have the
       source position of the using-declaration itself, whereas
       other projection symbols take on the source position of the
       fundamental symbol. */
    new_sym->decl_position = decl_pos;
    if (*other_sym == NULL) {
      a_symbol_ptr  front_sym = new_sym->header->symbol;
      /* If this is a using-declaration referring to a dependent base member
         (which is represented through a template parameter symbol), we want
         to ensure that any member function of the same name already in this
         class scope is found before the new using-declaration.  This is
         achieved by arranging for the existing member symbol to remain at
         the head of the list of symbols. */
      if (front_sym != NULL && new_sym->decl_scope == front_sym->decl_scope &&
          is_nontype_template_param_symbol(fund_sym)) {
        remove_symbol(front_sym);
      } else {
        front_sym = NULL;
      }  /* if */
      /* Just enter it, since no overloading is involved. */
      reenter_symbol(new_sym, depth_scope_stack, /*suppress_error=*/TRUE);
      /* Save new_sym as *other_sym, in case is_overloaded is TRUE. */
      if (!new_sym->is_error) *other_sym = new_sym;
      if (front_sym != NULL) {
        /* Reenter front_sym so it remains at the head of the symbol list. */
        reenter_symbol(front_sym, depth_scope_stack, /*suppress_error=*/TRUE);
      }  /* if */
    } else {
      *other_sym = add_symbol_to_overload_list(new_sym, *other_sym,
                                              /*use_namespace=*/FALSE,
                                              (a_namespace_ptr)NULL);
      set_mixed_static_nonstatic_flag(*other_sym);
    }  /* if */
    if (fund_sym->kind == (a_symbol_kind)sk_member_function) {
      rp = fund_sym->variant.routine.ptr;
    } else if (fund_sym->kind == (a_symbol_kind)sk_function_template) {
      rp = fund_sym->variant.template_info->variant.function.routine;
    }  /* if */
    if (rp != NULL) {
      if (rp->special_kind == (a_special_function_kind)sfk_conversion) {
        /* Allocate the new conversion list entry and link it in the
           list for the current class. */
        add_to_conversion_list(new_sym, 
                               symbol_supplement_for_class(class_type));
      } else if (rp->special_kind ==
                               (a_special_function_kind)sfk_operator &&
                 rp->variant.opname_kind == (an_opname_kind)onk_assign) {
        /* Record the assignment operator in the symbol. */
        record_assignment_operator_in_class_symbol(
                                 symbol_supplement_for_class(class_type),
                                 new_sym, *other_sym);
      }  /* if */
    }  /* if */
    /* Create a class member using decl entry to represent this
       declaration in the IL. */
    udp = make_using_decl(fund_sym, &decl_pos, depth_scope_stack);
    /* Record the class that was actually specified in the qualified
       name in the source. */
    udp->qualifier.class_type = sym_parent_class(declared_sym);
    udp->access = access;
    udp->is_class_member = TRUE;
    /* Update cross-reference and source-sequence info, if required. */
    record_using_decl(fund_sym, &decl_pos, udp, *prev_udp);
    *prev_udp = udp;
  }  /* if */
}  /* create_member_using_declaration */


static a_boolean in_overload_set(a_symbol_ptr  member,
                                 a_symbol_ptr  set)
/*
Return TRUE if and only if the routine represented by member is listed in
the overload set represented by set.  This routine also searches nested
sets.
*/
{
  a_boolean         result = FALSE;
  char              *member_entry, *set_entry;
  an_il_entry_kind  member_kind, set_kind;

  member = fundamental_symbol_of(member);
  member_entry = il_entry_for_symbol(member, &member_kind);
  set = fundamental_symbol_of(set);
  check_assertion(set->kind == (a_symbol_kind)sk_overloaded_function);
  set = set->variant.overloaded_function.symbols;
  for (; set != NULL; set = set->next) {
    a_symbol_ptr  fund_sym = fundamental_symbol_of(set);
    set_entry = il_entry_for_symbol(fund_sym, &set_kind);
    if (set_kind == member_kind &&
        corresponding_entries(set_entry, member_entry, member_kind)) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* in_overload_set */


static void check_member_using_visibility(a_type_ptr    class_type,
                                          a_symbol_ptr  fund_sym,
                                          a_boolean     *err)
/*
Members (represented by fund_sym) designated by a member using-declaration must
be visible in a direct base class of the class (represented by class_type) in
which the using-declaration appears.  If that is not the case, *err is set to
TRUE and an error is issued (*err should be passed in as FALSE).
*/
{
  a_base_class_ptr  direct_bcp = base_classes_of(class_type);
  a_symbol_locator  locator;

  check_assertion(!*err);
  if (fund_sym->kind == (a_symbol_kind)sk_overloaded_function) {
    /* Treat each member of the overload set separately. */
    fund_sym = fund_sym->variant.overloaded_function.symbols;
    for (; fund_sym != NULL && !*err; fund_sym = fund_sym->next) {
      check_member_using_visibility(class_type,
                                    fundamental_symbol_of(fund_sym), err);
    }  /* for */
  } else {
    for (; direct_bcp != NULL; direct_bcp = direct_bcp->next) {
      if (direct_bcp->direct) {
        a_symbol_ptr  visible_sym;
        clear_locator(&locator, &locator_for_curr_id.source_position);
        locator.symbol_header = locator_for_curr_id.symbol_header;
        visible_sym = class_qualified_id_lookup(&locator, direct_bcp->type,
                                                IDL_NO_OPTIONS);
        if (visible_sym == NULL) {
          /* Nothing to be done. */
        } else if (visible_sym == fund_sym) {
          goto search_done;
        } else if (visible_sym->kind ==
                                      (a_symbol_kind)sk_overloaded_function &&
                   in_overload_set(fund_sym, visible_sym)) {
          goto search_done;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* for */
search_done:
  if (direct_bcp == NULL) {
    error(ec_member_using_must_be_visible_in_direct_base);
    *err = TRUE;
  }  /* if */
}  /* check_member_using_visibility */


static void member_using_or_alias_declaration(a_type_ptr           class_type,
                                              an_access_specifier  access)
/*
Scan what is either a using-declaration, an alias declaration, or (if
tok_using is not the current token) a deprecated access-adjustment
declaration.  The semantics and representation of using-declarations and
access-adjustment declarations are identical.  class_type is the class in
which the declaration appears, and access is the current access (explicitly
specified or implicit) controlling the declaration.  (The alias declaration
case is almost entirely handled by a call to alias_declaration.  The latter
call is made in this routine because the tok_using token must be consumed to
distinguish an alias declaration from a using-declaration.)
*/
{
  a_symbol_ptr       sym, declared_sym;
  a_symbol_ptr       other_sym, fund_sym;
  a_base_class_ptr   bcp;
  a_boolean          err = FALSE, bcp_is_dummy = FALSE;
  a_boolean          is_overloaded;
  a_symbol_locator   locator;
  a_using_decl_ptr   prev_udp = NULL;
  a_source_position  decl_pos, using_pos, end_of_using_pos;

  db_enter(3, "member_using_or_alias_declaration");
  add_stop_token(tok_semicolon);
  using_pos = pos_curr_token;
  if (curr_token == tok_using) {
    a_token_kind  next_tok;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    end_of_using_pos = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    (void)get_token();
    if (alias_declarations_enabled &&
        is_generalized_identifier_start(GID_NO_OPTIONS) &&
        ((next_tok = next_token()) == tok_assign ||
         (std_attributes_enabled && next_tok == tok_lbracket))) {
      /* An identifier followed by a "=" or a bracket (presumably the start of
         C++0x-style attributes): This looks like an alias declaration. */
      a_decl_parse_state  dps;
      init_decl_parse_state(&dps);
      dps.in_class_scope = TRUE;
      dps.start_pos = using_pos;
      alias_declaration(&dps, &end_of_using_pos);
      goto done;
    }  /* if */
    /* A using-declaration is outside the "Embedded C++" subset. */
    feature_is_not_part_of_embedded_cplusplus_subset(
                             &using_pos, ec_using_decl_in_embedded_cplusplus);
    /* This is a using declaration.  Bypass "using" and scan the
       identifier. */
    if (!is_decl_qualified_name_start() && curr_token != tok_typename) {
      syntax_error(ec_exp_identifier);
      discard_curr_construct_pragmas();
      goto done;
    }  /* if */
  } else {
    /* This is an old-style access adjustment declaration (described in the
       ARM but now deprecated with the addition of using-declarations to the
       language). */
  }  /* if */
  /* Coalesce the identifier, which should be a qualified name with a class
     qualifier where the class is a base class of the current class (as
     indicated by class_type). */
  if (curr_token == tok_typename) {
    /* If typename appears in the using declaration, the lookup is a bit
       different, and there are some additional error checks.  If an error
       type is returned, an error was reported in the subroutine. */
    a_type_ptr   tp;
    a_symbol_ptr type_sym;

    typename_specifier(&tp, &type_sym, /*within_using_decl=*/TRUE,
                       (a_decl_pos_block_ptr)NULL);
    if (is_error_type(tp)) {
      err = TRUE;
#if CHECKING
    } else {
      sym = locator_for_curr_id.specific_symbol;
      if (sym != NULL && sym->is_class_member &&
          is_or_contains_template_param(sym_parent_class(sym))) {
        /* The using-declaration was for a member of a dependent class type.
           Normally this should be a base class type, but we may also end
           up here with the following invalid code:
             template<typename T> struct B { typedef int I; };
             template<typename T> struct D: B<T> {
               using D::I;  // Error (issued later on).
             };
           Because of the latter possibility, we must ensure we must consider
           the possibility of sym being a projection symbol. */
        sym = fundamental_symbol_of(sym);
        check_assertion(is_type_template_param_symbol(sym) ||
                        is_nonreal_instance_class_symbol(sym));
      }  /* if */
#endif /* CHECKING */
    }  /* if */
  } else {
    /* Not "using typename ...". */
    (void)coalesce_and_lookup_generalized_identifier(
                              GID_DTOR_RECOGNIZED | GID_TEMPLATE_ARGS_OPTIONAL,
                              ilm_using_declaration, &err);
  }  /* if */
  if (!err && is_union_type(class_type)) {
    pos_error(ec_no_access_or_using_decl_in_union, &using_pos);
    err = TRUE;
  }  /* if */
  if (!err) {
    decl_pos = locator_for_curr_id.source_position;
    /* The identifier should be a qualified name, with the qualifier a base
       class of the current class. */
    declared_sym = locator_for_curr_id.specific_symbol;
    fund_sym = (declared_sym == NULL) ? NULL
                                      : fundamental_symbol_of(declared_sym);
    if (!locator_for_curr_id.is_class_member) {
      error(ec_class_qualified_name_required);
      err = TRUE;
#if CHECKING
    } else if (declared_sym == NULL) {
      internal_error("member_using_decl: NULL symbol ptr");
#endif /* CHECKING */
    } else if (is_constructor_symbol(declared_sym) ||
               is_destructor_symbol(declared_sym)) {
      /* A using-declaration may not specify a constructor or destructor.
         (C++/CLI static constructors cannot get here.) */
      pos_diagnostic(microsoft_mode ? es_warning :
                     strict_ansi_mode ? es_error : es_discretionary_error,
                     ec_no_ctor_or_dtor_using_declaration, &decl_pos);
      err = TRUE;
    } else if (cppcli_enabled && is_finalizer_symbol(declared_sym)) {
      /* A using-declaration may not specify a finalizer. */
      pos_error(ec_no_finalizer_using_declaration, &decl_pos);
      err = TRUE;
    } else if (declared_sym->ambiguous) {
      /* declared_sym must be a projection symbol -- and it is ambiguous. */
      sym_error(ec_ambiguous_name, declared_sym);
      err = TRUE;
    } else if (locator_for_curr_id.is_template_id) {
      /* A template-id (that is, template-name<template-args>) is not allowed
         here. */
      error(ec_template_id_not_allowed);
      err = TRUE;
    } else {
      a_type_ptr  parent_class = qualifier_class_type(locator_for_curr_id);
      if ((could_be_dependent_class_type(parent_class) ||
           has_dependent_base_class(class_type)) &&
          !same_entities(class_type, parent_class)) {
        /* The qualifier is a dependent class or the enclosing class has a
           dependent base class.  Either way, we cannot in general determine
           which base class the using-declaration refers to.  Suppress the
           base class check and create a dummy base class. */
        bcp_is_dummy = TRUE;
        bcp = alloc_base_class();
        bcp->type = sym_parent_class(declared_sym);
        bcp->derived_class = class_type;
      } else {
        bcp = find_base_class_of(class_type, parent_class);
        if (bcp == NULL) {
          error(ec_bad_base_class);
          err = TRUE;
        } else if (bcp->ambiguous) {
          /* The base class is ambiguous, but only issue an error if the member
             itself is ambiguous -- that is, the member must be either a field
             or a nonstatic member function or an overload set containing at
             least one nonstatic member function. */
          sym = fund_sym;
          if (sym->kind == (a_symbol_kind)sk_field) {
            /* A field in an ambiguous base class is ambiguous. */
            err = TRUE;
          } else {
            if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
              if (sym->variant.overloaded_function.mixed_static_nonstatic) {
                /* There must be at least one nonstatic member function in this
                   overload set. */
                err = TRUE;
              } else {
                /* Either all are static or all are nonstatic.  Decide which
                   by looking at the first in the list. */
                sym = sym->variant.overloaded_function.symbols;
                sym = fundamental_symbol_of(sym);
              }  /* if */
            }  /* if */
            if (sym->kind == (a_symbol_kind)sk_member_function &&
                routine_type_is_nonstatic_member_function(
                                                  routine_symbol_type(sym))) {
              /* A nonstatic member function in an ambiguous base classes is
                 ambiguous. */
              err = TRUE;
            }  /* if */
          }  /* if */
          if (err) sym_error(ec_ambiguous_name, declared_sym);
        } else if (!(bcp->direct || any_cfront_mode() || gpp_mode ||
                     (microsoft_mode && microsoft_version > 1200))) {
          /* Base class members designated in a using-declaration must be
             visible in the scope of at least one direct base class. */
          check_member_using_visibility(class_type, fund_sym, &err);
        }  /* if */
      }  /* if */
    }  /* if */
    if (!err) {
      a_symbol_ptr  existing_sym;
      /* Look up the name in the scope of the current class. */
      clear_locator(&locator, &decl_pos);
      locator.symbol_header = locator_for_curr_id.symbol_header;
      (void)curr_scope_id_lookup(&locator, IDL_PROJ_SYMBOL_ALLOWED);
      existing_sym = locator.specific_symbol;
      if (existing_sym != NULL) {
        reduce_projection_symbol_to_fundamental_symbol(existing_sym);
      }  /* if */
      if (existing_sym != NULL && existing_sym != fund_sym &&
          (existing_sym->kind != (a_symbol_kind)sk_type ||
           !existing_sym->variant.type.is_injected_class_name)) {
        /* Except to introduce function names into an overload set, a
           using declaration cannot usually coexist with another declaration
           with the same name. */
        if (is_function_or_template_symbol(fund_sym)) {
          /* Okay. */
        } else if (is_nontype_template_param_symbol(fund_sym)) {
          /* Might be a function symbol, so it's okay. */
        } else {
          err = TRUE;
        }  /* if */
        if (!err) {
          if (is_function_or_template_symbol(existing_sym)) {
            /* Okay. */
          } else if (is_nontype_template_param_symbol(existing_sym)) {
            /* Might be a function symbol, so it's okay. */
          } else {
            err = TRUE;
          }  /* if */
        }  /* if */
        if (err) {
          /* Name has already been declared. */
          pos_st_error(ec_id_already_declared, &decl_pos,
                       locator_for_curr_id.symbol_header->identifier);
        }  /* if */
      }  /* if */
      if (!err) {
        /* Issue an error if a using-declaration introduces a name that is
           the same as the current class name. */
        a_symbol_ptr  class_sym = (a_symbol_ptr)class_type->
                                                  source_corresp.assoc_info;
        if (locator.symbol_header == class_sym->header) {
          pos_error(ec_class_and_member_name_conflict, &decl_pos);
          err = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (!err) {
    /* Issue diagnostics on pragmas that are trying to bind to a using
       declaration or an access declaration. */
    cannot_bind_to_curr_construct();
  } else {
    discard_curr_construct_pragmas();
  }  /* if */
  if (!err && !is_duplicate_member_using_decl(declared_sym, &using_pos)) {
    /* No error so far, so enter the using-declaration symbol. */
    other_sym = NULL;
    sym = declared_sym;
    fund_sym = fundamental_symbol_of(sym);
    is_overloaded = FALSE;
    /* See if the using declaration refers to a function or overload set. */
    if (is_function_or_template_symbol(fund_sym)) {
      /* Member function or member function template. */
      /* If other_sym is non-NULL, there is already a function declaration by
         this name in the current class: we will add the declared symbol or
         symbols to an overload set of the current class. */
      other_sym = locator.specific_symbol;
      if (other_sym != NULL && is_nontype_template_param_symbol(other_sym)) {
        /* We're treating the template param symbol as if it were a function
           but we don't want it to be in the overload set. */
        other_sym = NULL;
      }  /* if */
      if (fund_sym->kind == (a_symbol_kind)sk_overloaded_function) {
        /* The using-declaration specifies a base-class member function
           overload set. */
        is_overloaded = TRUE;
        sym = fund_sym->variant.overloaded_function.symbols;
      }  /* if */
    }  /* if */
    /* This is a loop in case the using-declaration specifies an overload
       set -- each member of the overload set is projected independently. */
    if (!(scope_stack[depth_scope_stack].in_prototype_instantiation ||
          is_tag_symbol(sym))) {
      /* Check if we missed a tag symbol; it should be imported too.
         A dummy overload_sym is used, because tag names are not overloaded. */
      a_symbol_ptr      tag_sym, overload_sym = NULL;
      locator = locator_for_curr_id;
      clear_specific_symbol(locator);
      tag_sym = class_qualified_id_lookup(&locator, bcp->type,
                                          IDL_MUST_BE_TAG |
                                            IDL_DIRECT_CLASS_MEMBERS_ONLY);
      if (tag_sym != NULL && !is_class_template_symbol(tag_sym) &&
          tag_sym->kind != (a_symbol_kind)sk_type) {
        /* In some modes, "must be tag" lookups can find typedefs.  Ignore
           such symbols. */
        create_member_using_declaration(tag_sym, tag_sym, &overload_sym, bcp,
                                        bcp_is_dummy, class_type, &prev_udp,
                                        access);
      }  /* if */
    }  /* if */
    for (;;) {
      create_member_using_declaration(sym, declared_sym, &other_sym, bcp,
                                      bcp_is_dummy, class_type, &prev_udp,
                                      access);
      if (!is_overloaded) break;
      if ((sym = sym->next) == NULL) break;
    }  /* for */
  }  /* if */
  /* Bypass the identifier. */
  (void)get_token();
done:;
  remove_stop_token(tok_semicolon);
  (void)required_token(tok_semicolon, ec_exp_semicolon);
  db_exit();
}  /* member_using_or_alias_declaration */


a_symbol_ptr find_corresp_prototype_tag_sym(a_symbol_ptr  curr_sym)
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
*/
{
  a_symbol_ptr                   corresp_prototype_tag_sym = NULL;
  a_symbol_ptr                   sym, templ_sym;
  a_class_symbol_supplement_ptr  cssp;
  a_class_symbol_supplement_ptr  proto_cssp;
  a_type_ptr                     tp;
  a_symbol_ptr	                 parent_sym;

  db_enter(3, "find_corresp_prototype_tag_sym");
  if (is_nonreal_instance_class_symbol(curr_sym)) {
    /* Return NULL. */
  } else if (curr_sym->is_class_member) {
    /* curr_sym represents a nested class.  Get the corresponding prototype
       tag symbol of its parent class; then find the corresponding nested
       class within it.  The prototype tag symbol of the parent class is
       stored in the latter's class symbol supplement. */
    parent_sym = symbol_supplement_for_class(sym_parent_class(curr_sym))->
                                                       corresp_prototype_sym;
    if (parent_sym != NULL) {
      /* sym is the corresponding prototype tag symbol of the parent class.
         It represents a prototype instantiation of a class template or a
         class nested within a prototype instantiation. One of its own nested
         classes will be the nested class that corresponds to curr_sym: find
         a symbol for that nested class. */
      tp = parent_sym->variant.class_struct_union.type;
      if (is_unnamed_tag_symbol(curr_sym) || curr_sym->is_error) {
        /* Unusual case of an unnamed class -- e.g., an anonymous union.
           Look through the types list associated with the parent class. */
        tp = tp->variant.class_struct_union.extra_info->assoc_scope->types;
        for (; tp != NULL; tp = tp->next) {
          sym = (a_symbol_ptr)tp->source_corresp.assoc_info;
          if (sym != NULL && sym->kind == curr_sym->kind) {
            cssp = sym->variant.class_struct_union.extra_info;
            if (cssp->prototype_token_sequence_number ==
                                                curr_token_sequence_number) {
              corresp_prototype_tag_sym = sym;
              break;
            }  /* if */
          }  /* if */
        }  /* for */
      } else {
        proto_cssp = parent_sym->variant.class_struct_union.extra_info;
        for (sym = find_symbol_list_in_table(&proto_cssp->pointers_block,
                                             curr_sym->header);
             sym != NULL;
             sym = sym->next_in_lookup_table) {
          if (sym->kind == curr_sym->kind) {
            cssp = sym->variant.class_struct_union.extra_info;
            /* Note that a translation unit test is not needed because
               token sequence numbers uniquely identify a translation unit. */
            if (cssp->prototype_token_sequence_number ==
                                                curr_token_sequence_number) {
              corresp_prototype_tag_sym = sym;
              break;
            }  /* if */
          }  /* if */
        }  /* for */
      }  /* if */
      if (corresp_prototype_tag_sym == NULL) {
        /* In some cases involving anonymous unions the symbol may not
           be found in the lookup tables (these cases are nonstandard).
           Go through the active or inactive symbols to look for a match. */
        if (is_incomplete_type(parent_sym->variant.class_struct_union.type)) {
          /* We must still be in the midst of the prototype instantiation, so
             the symbol is still on the active list. */
          sym = curr_sym->header->symbol;
        } else {
          /* Look through the symbols on the inactive list. */
          sym = curr_sym->header->inactive_symbols;
        }  /* if */
        for (; sym != NULL; sym = sym->next) {
          if (sym->kind == curr_sym->kind) {
            cssp = sym->variant.class_struct_union.extra_info;
            /* Note that a translation unit test is not needed because
               token sequence numbers uniquely identify a translation unit. */
            if (cssp->prototype_token_sequence_number ==
                                                curr_token_sequence_number) {
              corresp_prototype_tag_sym = sym;
              break;
            }  /* if */
          }  /* if */
        }  /* for */
      }  /* if */
      check_assertion(corresp_prototype_tag_sym != NULL || total_errors != 0);
    }  /* if */
  } else {
    /* curr_sym is not a nested class.  If it has a template symbol it may be
       an instantiation of a class template. */
    cssp = curr_sym->variant.class_struct_union.extra_info;
    templ_sym = cssp->class_template;
    if (templ_sym == NULL ||
        curr_sym->variant.class_struct_union.type->
                       variant.class_struct_union.is_specialized) {
      /* The current symbol is not an instantiation (because it is
         not associated with a template) or else is a specific definition
         (i.e., provided by the user rather than generated by the compiler
         based on the template). */
    } else {
      /* The current symbol is an instantiation.  Get the prototype
         instantiation symbol from the template symbol supplement. */
      corresp_prototype_tag_sym = templ_sym->variant.template_info->
                               variant.class_template.prototype_instantiation;
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


static void check_nonreal_nested_class(a_symbol_ptr                  tag_sym,
                                       a_template_symbol_supplement_ptr tssp)
/*
This is a prototype instantiation of a nested class specified by tag_sym.
If the nested class is defined inside a template definition, set fields of
*tssp, its template-symbol-supplement, based on the template-symbol-supplement
of its parent class.
*/
{
  a_scope_stack_entry_ptr           instantiation_ssep;
  a_template_symbol_supplement_ptr  parent_tssp;

  instantiation_ssep = &scope_stack[depth_innermost_instantiation_scope];
  /* If this is a prototype instantiation, allocate a template symbol
     supplement if one has not already been created.  This is only done at
     this point for nested classes defined within the template. Note that a
     nested class defined outside of the template may itself have classes
     defined within its body. */
  if (tag_sym->variant.class_struct_union.type !=
                                      instantiation_ssep->assoc_type) {
    parent_tssp = symbol_supplement_for_class(sym_parent_class(tag_sym))
                                                             ->template_info;
    tssp->variant.class_template.prototype_instantiation = tag_sym;
    /* A member class of a template class whose body is supplied in the class
       shares the template declaration information with the enclosing class. */
    set_template_cache_info(&tssp->cache, (a_token_cache_ptr)NULL,
                            parent_tssp->cache.decl_info);
    tssp->variant.class_template.name_linkage =
                             parent_tssp->variant.class_template.name_linkage;
    /* The cache segment information is used later to remove nested class
       definitions from the token cache of the enclosing class. */
    tssp->cache_segment = alloc_template_cache_segment(tag_sym, tssp);
    tssp->cache_segment->first_token_number = curr_token_sequence_number;
    if (is_unnamed_tag_symbol(tag_sym)) {
      /* The definition of this nested class cannot be moved outside of the
         enclosing class because it has no name.  Record this information in
         the template symbol supplement. */
      tssp->variant.class_template.not_standalone_nested_class = TRUE;
    }  /* if */
  }  /* if */
}  /* check_for_nonreal_nested_class */


static a_boolean scan_access_specification(a_class_def_state  *state)
/*
Check for an access specifier in a class definition (described by state).  If
one is found return TRUE and update state->access accordingly.
*/
{
  a_boolean  found = FALSE;

  /* The check is implemented as a loop because successive access
     specifications are permitted. */
  while (curr_token == tok_public || curr_token == tok_private ||
         curr_token == tok_protected) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cppcli_enabled && curr_token != tok_protected) {
      /* A C++/CLI top-level visibility specifier is not handled here. */
      a_token_kind  next_tok = next_token();
      if (is_class_type_keyword(next_tok) || next_tok == tok_enum) {
        break;
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    found = TRUE;
    if (curr_token == tok_public) {
      state->access = (an_access_specifier)as_public;
    } else if (curr_token == tok_protected) {
      state->access = (an_access_specifier)as_protected;
    } else {
      state->access = (an_access_specifier)as_private;
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode) {
      a_type_ptr  class_type = state->class_type;
      if (curr_token == tok_protected || curr_token == tok_private) {
        if (class_type->variant.class_struct_union.is_interface ||
            (cppcli_enabled &&
             cli_class_type_kind_is(class_type, cctk_interface))) {
          error(ec_interface_cannot_have_private_or_protected);
          state->access = (an_access_specifier)as_public;
        } else {
          state->potentially_interface_like = FALSE;
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    scope_stack[decl_scope_level].current_access = state->access;
    /* Advance to the colon, which is required. */
    (void)get_token();
    if (curr_token == tok_colon) {
      /* Advance past it. */
      (void)get_token();
    } else {
      /* Calling is_member_decl_start involves calling curr_type_symbol,
         which suppresses access and ambiguity errors when looking up what
         may be a qualified name.  This is correct in this case since we do
         not want to do the access check until after excluding the possibility
         of an access adjustment declaration. */
      if (curr_token == tok_identifier || is_member_decl_start()) {
        error(ec_exp_colon);
      } else {
        syntax_error(ec_exp_colon);
      }  /* if */
    }  /* if */
    /* Any next-construct-pragmas that appear after the access specifier
       should be added to those that appear before.  This means the access
       specifier is ignored as a "construct" -- the binding skips over it. */
    (void)select_curr_construct_pragmas(/*add_to_list=*/TRUE);
  }  /* while */
  return found;
}  /* scan_access_specification */


static void check_friend_class_decl(a_type_ptr              class_type,
                                    a_member_decl_info_ptr  decl_info)
/*
The current construct seems to make member_type a friend of class_type.
Check that this is a valid type and if so make member_type a friend.
*decl_info contains some additional information about the declaration.
*/
{
  a_decl_parse_state  *state = &decl_info->decl_state;
  a_type_ptr          member_type = state->specifiers_type;

  if (is_error_type(member_type)) {
    /* An error was already issued. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cppcli_enabled && is_immediate_managed_class_type(class_type)) {
    pos_error(ec_managed_class_cannot_have_friend, &state->start_pos);
  } else if (microsoft_mode &&
             class_type->variant.class_struct_union.is_interface) {
    pos_error(ec_interface_cannot_have_friend, &state->start_pos);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else if ((state->dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) == 0) {
    /* No type was specified at all; e.g. "friend;". */
    pos_error(ec_bad_friend_decl, &state->start_pos);
  } else if ((((is_class_struct_union_type(member_type) ||
                is_template_param_type(member_type)) &&
               !is_top_level_qualified_type(member_type)) ||
              (extended_friends_enabled && state->qualifiers == TQ_NONE)) &&
             depth_template_declaration_scope == NO_SCOPE_DEPTH) {
    /* This is a friend class declaration.  Normally, only the form
           friend class A;  // or "struct" or "union" instead of "class"
       is allowed, but many compilers also accept friend declarations
       without a class-key.  For example:
            friend A;
       We also accept the latter form as an extension (except in strict
       mode).  The working paper for the next standard allows many other
       forms (e.g., "friend int;", but not "friend int const;").  We
       implement those rules when extended_friends_enabled is TRUE (e.g.,
       in C++0x mode). */
    a_boolean  normal_friend_type =
                               (!extended_friends_enabled ||  /* For speed. */
                                is_class_struct_union_type(member_type) ||
                                is_template_param_type(member_type));
    if ((state->dso_flags & DSO_TYPENAME) && !extended_friends_enabled) {
      /* "friend typename ..." is not allowed in many modes. */
      pos_error(ec_no_typename_in_friend_class_decl, &state->start_pos);
    } else {
#if GENERATE_SOURCE_SEQUENCE_LISTS
      a_boolean  generated_real_instance =
                 (class_type->variant.class_struct_union.is_template_class &&
                  !class_type
                     ->variant.class_struct_union.is_prototype_instantiation &&
                  !class_type->variant.class_struct_union.is_specialized);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      if (state->dso_flags & (DSO_INLINE | DSO_VIRTUAL)) {
        /* A friend class declaration cannot contain inline or virtual
           specifiers. */
        pos_error(ec_bad_friend_decl, &state->start_pos);
      } else if (!normal_friend_type) {
        /* This is a friend declaration that names neither a class type nor a
           template-dependent type.  For example, "friend int;".  Such friend
           declarations have no effect, but we may need to record them in the
           list of source sequence entries.  Since these do not have any
           effect, we do not record them in the source sequence list if they
           result from a template instantiation (even when instances are
           otherwise recorded in the source sequence entry list).  Among other
           things, this avoids potential problems with the code produced by
           the C++-generating back end not being consumable by older C++
           compilers. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (!generated_real_instance &&
            !source_sequence_entries_disallowed) {
          a_source_sequence_entry_ptr   ssep;
          a_src_seq_secondary_decl_ptr  sssdp;
          ssep = add_empty_source_sequence_entry();
          sssdp = make_source_sequence_secondary_decl(
                               (char*)member_type, (an_il_entry_kind)iek_type,
                               (a_type_ptr)NULL);
          sssdp->friend_decl = TRUE;
          sssdp->decl_position = state->start_pos;
          ssep->entity.kind = (a_byte_il_entry_kind)iek_src_seq_secondary_decl;
          ssep->entity.ptr  = (char *)sssdp;
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      } else if (!(state->dso_flags & DSO_ELABORATED_TYPE_SPECIFIER)) {
        member_type = skip_typerefs(member_type);
        if (!extended_friends_enabled) {
          char  *class_key_string;
          switch (member_type->kind) {
            case tk_class:   class_key_string = "class";   break;
            case tk_struct:  class_key_string = "struct";  break;
            case tk_union:   class_key_string = "union";   break;
            case tk_template_param:
                             class_key_string = "class";   break;
            default:
              unexpected_condition();
          }  /* switch */
          /* Strict ANSI diagnostic in strict ANSI mode, remark
             otherwise. */
          pos_st_diagnostic(strict_ansi_mode ?
                              strict_ansi_error_severity : es_remark,
                            ec_nonstd_friend_decl,
                            &locator_for_curr_id.source_position,
                            class_key_string);
        }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if !CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
        if (generated_real_instance) {
          /* We're parsing a normal instantiation and it is not to be recorded
             in the source sequence list in this configuration. */
          goto done_with_sse_for_nonstandard_friend;
        }  /* if */
#endif /* !CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
        if (member_type->kind == (a_type_kind)tk_template_param) {
          if (!prototype_instantiations_in_il) {
            /* We are in a prototype instantiation, but we do not record them
               in the IL: Nothing should be done in terms of source sequence
               entries. */
            goto done_with_sse_for_nonstandard_friend;
          }  /* if */
          member_type = proxy_class_for_template_param(
                                                  skip_typerefs(member_type));
        }  /* if */
        if (!source_sequence_entries_disallowed) {
          /* Since this type name did not involve an elaborated type name,
             we do not yet have a source sequence entry for it. */
          a_source_sequence_entry_ptr   ssep;
          a_src_seq_secondary_decl_ptr  sssdp;
          record_symbol_declaration(
                         SRK_DECLARATION | SRK_FRIEND,
                         (a_symbol_ptr)member_type->source_corresp.assoc_info,
                         &locator_for_curr_id.source_position,
                         (a_source_sequence_entry_ptr)NULL);
          ssep = last_matching_source_sequence_entry((char *)member_type);
          check_assertion(ssep != NULL &&
                          ss_entry_kind(ssep) == iek_src_seq_secondary_decl);
          sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
          sssdp->autonomous_tag_decl = TRUE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
          /* Record extended position information. */
          check_assertion(sssdp->decl_pos_info == NULL);
          sssdp->decl_pos_info = make_decl_pos_supplement(
                                                  in_file_scope(sssdp),
                                                  &decl_info->decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        }
done_with_sse_for_nonstandard_friend:;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
      if (normal_friend_type) {
        /* decl_friend_class is only called if member_type is a class or a
           template-dependent type.  Constructs like "friend int;" do not
           have a semantic effect and therefore do not need a call to
           decl_friend_class. */
        decl_friend_class(class_type, member_type);
      }  /* if */
    }  /* if */
  } else {
    /* Invalid friend declaration. */
    pos_error(ec_bad_friend_decl, &state->start_pos);
  }  /* if */
}  /* check_friend_class_decl */


static void check_missing_declarator_in_member_declaration(
                                           a_type_ptr              class_type,
                                           a_member_decl_info_ptr  decl_info)
/*
This routine is called while a member declaration is being scanned when a
semicolon is encountered immediately after the declaration-specifiers.  In
other words, there is no declarator in the member declaration.  Issue an error
if appropriate.  class_type is the class whose definition is being scanned.
*decl_info contains information about the declaration as it has been scanned
thus far; moreover, several fields of *decl_info may be updated by this
routine.
*/
{
  a_decl_parse_state  *decl_state = &decl_info->decl_state;
  a_type_ptr           member_type = decl_state->specifiers_type;
  a_source_position    *err_pos = &decl_state->start_pos;
  a_decl_flag_set      dso_flags = decl_state->dso_flags;
  a_storage_class      storage_class = decl_state->storage_class;

  /* Check first whether this is an anonymous union declaration. */
  if (storage_class == (a_storage_class)sc_unspecified &&
      !is_incomplete_type(member_type) &&
      is_anonymous_union_decl(decl_info)) {
    /* A C++ anonymous union -- "union { int i, j; };".
       decl_info->is_anonymous_union will have been set to TRUE by the call
       to is_anonymous_union_decl. */
    a_type_ptr  au_type = skip_typerefs_not_typedefs(member_type);
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
    /* It might also be an anonymous-union-like construct in C or C++, namely
       an unnamed class/struct/union type, possibly represented by a typedef
       name, whose subfields are to be visible as though they were fields of
       the current class.  For a case like
         typedef struct { int i; int j; } A;
         struct B {
           A;
         };
       put out the declaration entry for the anonymous struct.
    */
    if (decl_info->is_nonstd_anonymous_union) {
      a_symbol_ptr  sym = symbol_for(au_type);
      if (sym != NULL && has_name(au_type)) {
        record_symbol_declaration(SRK_DECLARATION, sym, err_pos,
                                  (a_source_sequence_entry_ptr)NULL);
      }  /* if */
      if (!has_name(au_type)) {
        /* Only the types of anonymous unions whose type itself (as opposed
           to the associated member object) is anonymous are marked as being
           nonstandard anonymous union types. */
        check_assertion(is_immediate_class_type(au_type));
        au_type
           ->variant.class_struct_union.is_nonstd_anonymous_union_type = TRUE;
      }  /* if */
    }  /* if */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
    /* Set the IL referenced flag for the anonymous union type. */
    au_type->source_corresp.referenced = TRUE;
  } else if (!C_mode()) {
    /* C++ mode. */
    if (dso_flags & DSO_MUTABLE) {
      /* "mutable" is only allowed on nonstatic data member decls. */
      pos_error(ec_mutable_not_allowed, err_pos);
    }  /* if */
    if (dso_flags & DSO_FRIEND) {
      check_friend_class_decl(class_type, decl_info);
    } else if (decl_info->is_member_template) {
      if (decl_info->decl_state.sym != NULL) {
        /* Diagnostic will be issued later. */
      } else {
        /* Function declarator is missing on a member template declaration. */
        pos_error(ec_bad_member_template_decl, err_pos);
      }  /* if */
    } else if (dso_flags & DSO_DECLARES_SOMETHING) {
      /* This is a free standing declaration of a class, struct, union, or
         enum type entry.  It will already have been recorded on the types
         list for the current class.  No need to complain about a missing
         identifier.  Just check for some errors (except in Microsoft mode). */
      if (!microsoft_mode) {
        if (storage_class == (a_storage_class)sc_typedef) {
          /* A case like "typedef struct S { int i; };" */
          pos_diagnostic(strict_ansi_mode ?
                             strict_ansi_error_severity : es_warning,
                         ec_missing_typedef_name, &pos_curr_token);
        } else if (storage_class != (a_storage_class)sc_unspecified) {
          pos_diagnostic(any_cfront_mode() ? es_warning : es_error,
                         ec_storage_class_not_allowed, err_pos);
        }  /* if */
        if (dso_flags & DSO_VIRTUAL) {
          pos_error(ec_virtual_not_allowed, err_pos);
        }  /* if */
        if (dso_flags & DSO_INLINE) {
          pos_error(ec_inline_not_allowed, err_pos);
        }  /* if */
        if (dso_flags & DSO_EXPLICIT) {
          pos_error(ec_explicit_not_allowed, err_pos);
        }  /* if */
        if (is_qualified_type(member_type)) {
          pos_diagnostic(strict_ansi_mode ? strict_ansi_error_severity :
                                            es_warning,
                       ec_useless_type_qualifiers, err_pos);
        }  /* if */
      }  /* if */
    } else if (storage_class == (a_storage_class)sc_typedef) {
      /* A case like "typedef int;" or "typedef struct { int i; };" */
      pos_diagnostic(strict_ansi_mode ? strict_ansi_error_severity :
                                        es_warning,
                     ec_missing_typedef_name, &pos_curr_token);
    } else if (dso_flags & DSO_DEFINES_SOMETHING) {
      /* A declaration with no declarator that defines a type but doesn't
         declare a name (since DSO_DECLARES_SOMETHING flag is FALSE) -- e.g.,
         "struct { int i; };" or "enum {};".
         The standard does not allow such constructs, but we accept them
         with a warning in default mode. */
      pos_diagnostic(strict_ansi_mode ? strict_ansi_error_severity :
                                        es_warning,
                     ec_useless_decl, err_pos);
      if (is_qualified_type(member_type)) {
        pos_diagnostic(strict_ansi_mode ? strict_ansi_error_severity :
                                          es_warning,
                       ec_useless_type_qualifiers, err_pos);
      }  /* if */
    } else {
      /* A case like "int;" or "enum ::E;": Issue an error. */
      an_error_severity  sev = es_error;
      if (gpp_mode && (dso_flags & DSO_ELABORATED_TYPE_SPECIFIER) != 0) {
        /* g++ accepts the useless member declaration in
               enum E {}; struct X { enum ::E; }; */
        sev = es_warning;
      }  /* if */
      pos_diagnostic(sev, ec_useless_decl, err_pos);
    }  /* if */
  } else {
    /* C mode. */
    if (C_dialect == C_dialect_pcc) {
      /* Silently ignore the unnamed field.  Note that no trace of it appears
         in the IL. */
    } else if (dso_flags & DSO_DEFINES_SOMETHING) {
      /* A struct or enum declaration, but no identifier.  Issue a warning
         (or error in -A mode). */
      pos_diagnostic(strict_ansi_mode ? strict_ansi_error_severity :
                                        es_warning,
                     ec_exp_identifier, &pos_curr_token);
    } else {
      /* Issue a warning (or error in -A mode) on the useless declaration. */
      pos_diagnostic(strict_ansi_mode ?
                       strict_ansi_error_severity : es_warning,
                     ec_useless_decl, err_pos);
    }  /* if */
  }  /* if */
  if ((dso_flags & (DSO_DECLARES_SOMETHING | DSO_DEFINES_SOMETHING)) ||
      (decl_info->is_anonymous_union &&
       member_type->kind != (a_type_kind)tk_typeref)) {
    /* This is a free-standing declaration of a class, struct, union, or
       enum. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    a_type_ptr  tp = skip_typerefs(member_type);
    if (dso_flags & DSO_DEFINES_SOMETHING) {
      tp->autonomous_primary_tag_decl = TRUE;
    } else if (!source_sequence_entries_disallowed) {
      a_source_sequence_entry_ptr  ssep =
                              last_matching_source_sequence_entry((char *)tp);
      if (ssep == NULL) {
        /* There is no source sequence entry to update. */
      } else if (ss_entry_kind(ssep) == iek_src_seq_secondary_decl) {
        /* This is the normal case, but errors while processing declaration
           specifiers may have caused us to not create a secondary source
           sequence entry. */
        a_src_seq_secondary_decl_ptr  sssdp =
                               (a_src_seq_secondary_decl_ptr)ssep->entity.ptr;
        sssdp->autonomous_tag_decl = TRUE;
      } else {
        check_assertion(total_errors > 0);
      }  /* if */
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Prefix attributes require a declarator. */
    check_prefix_attributes_without_a_declarator(decl_state);
  }  /* if */
}  /* check_missing_declarator_in_member_declaration */


static void check_completed_member_type(a_symbol_locator        *locator,
                                        a_class_def_state_ptr   class_state,
                                        a_member_decl_info_ptr  decl_info)
/*
This routine is called after declarator to perform some checks on *type,
which is the type produced by the combined processing of decl_specifiers
and declarator. locator points to the symbol locator for the member.
*class_state tracks general information about the class, and *decl_info
tracks information about the current declaration.
*/
{
  a_decl_parse_state  *decl_state = &decl_info->decl_state;
  a_type_ptr          type = decl_state->type;

  if (decl_state->storage_class != (a_storage_class)sc_typedef) {
    if (any_cfront_mode() &&
        check_member_function_typedef(type, &locator->source_position)) {
      /* This is declaration using a member function typedef.  A typedef has
         been previously been declared like this:
              typedef void A::t(int);  // Nonstandard
         meaning "t" names a routine type taking an int argument, returning
         void, and having an implicit this-param type of const-ptr-to-A.
         (This "member function typedef" is not part of the standard language
         nor of the ARM; it's allowed for cfront compatibility only.)  The
         only supported use is to declare a pointer-to-member type, e.g.,
              t *pm;                   // Okay
         Whereas it is apparently being used here to declare a function, e.g.,
              t f;                     // Error
         The diagnostic has already been issued by the subroutine, but change
         the type to an error type. */
      decl_state->type = error_type();
    }  /* if */
  }  /* if */
  if ((decl_state->dso_flags & DSO_DEFINES_SOMETHING) && !microsoft_mode) {
    /* A class or enum definition was scanned as part of this declaration.
       However, it is explicitly prohibited to define a type in a function
       return type.  This is taken to apply to pointer-to-function type
       declarations as well to the function declarations.  Microsoft
       compilers accept this however. */
    if (decl_info->return_type_def_err) {
      /* Error has already been issued. */
    } else {
      a_type_ptr  tp = type;
      for (;;) {
        if (is_function_type(tp)) {
          /* Function type in which the return type involves a
             definition. */
          pos_error(ec_type_def_not_allowed_in_func_type_decl,
                    &decl_state->start_pos);
          decl_info->return_type_def_err = TRUE;
          break;
        } else if (is_any_ptr_or_ref_type(tp)) {
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
  if (class_state->is_nonreal_instantiation && is_function_type(type)) {
    /* The class is a non-real template instantiation.  Go through the
       parameters for this function type, and if any of the associated types
       involves a template parameter, mark the param type entry; this is
       useful for function arg matching. */
    set_parameter_list_template_param_flags(type);
  }  /* if */
}  /* check_completed_member_type */


static void check_typedef_function_type(a_type_ptr         *member_type,
                                        a_source_position  *err_pos,
                                        a_boolean          is_definition,
                                        a_type_ptr         class_type,
                                        a_boolean          is_nonstatic_member)
/*
*member_type points to a typedef type with which a member or friend function
has been declared.  Since typedef types are shared, update *member_type with
a copy of the underlying routine type if this is a definition or if the
implicit-this-param pointer needs to be supplied.  *err_pos is the source
position at which to issue a diagnostic, if required.  is_definition is TRUE
if this declaration is a definition; class_type indicates the class in which
the member or friend function appears, and is_nonstatic_member is TRUE when
the function is a nonstatic member of class_type.
*/
{
  a_type_ptr                     rout_type;
  a_routine_type_supplement_ptr  rtsp;

  if (is_definition) {
    /* Not legal to define a function with a typedef type. */
    pos_error(ec_function_type_must_come_from_declarator, err_pos);
  }  /* if */
  rout_type = skip_typerefs(*member_type);
  rtsp = rout_type->variant.routine.extra_info;
  if (is_definition || is_nonstatic_member ||
      rtsp->routine_name_linkage !=
                      (a_name_linkage_kind)nlk_cplusplus_external) {
    /* Build a copy of the routine type so as to have a
       non-shared routine type entry. */
    rout_type = copy_routine_type_with_param_types(rout_type,
                                                   /*copy_default_args=*/TRUE);
    if (is_nonstatic_member) {
      /* This is a nonstatic member function declared through a typedef.
         Be sure the implicit this-param type is filled in, since that's
         the only way a nonstatic member function is distinguished from a
         static member function. */
      rout_type->variant.routine.extra_info->this_class = class_type;
    } else if (any_cfront_mode()) {
      /* Just in case this is a copy of the weird cfront-compatibility
         typedef, clear out the implicit this-param pointer in the copied
         type entry. */
      rout_type->variant.routine.extra_info->this_class = NULL;
      rout_type->variant.routine.extra_info->qualifiers = TQ_NONE;
    }  /* if */
    *member_type = rout_type;
  }  /* if */
}  /* check_typedef_function_type */


static void check_for_invalid_use_of_virtual(a_symbol_locator       *locator,
                                             a_type_ptr             class_type,
                                             a_member_decl_info_ptr decl_info)
/*
Issue an error and return TRUE if the virtual specifier is invalid for the
current function declaration.  *locator identifies the function declared, and
class_type is the class in which the declared function appears.  *decl_info
tracks information about the current declaration and is updated if an error
is found.
*/
{
  an_error_code       error_code = ec_no_error;
  an_error_severity   severity = es_error;
  a_decl_parse_state  *decl_state = &decl_info->decl_state;

  if (decl_info->invalid_virtual_specifier) {
    /* An error has already been issued on a previous declarator. */
  } else if (decl_state->dso_flags & DSO_FRIEND) {
    /* A friend function may not be declared virtual. */
    error_code = ec_virtual_not_allowed;
    if (microsoft_mode) {
      severity = es_warning;
    }  /* if */
  } else if (decl_info->is_constructor || is_union_type(class_type)
#if MICROSOFT_EXTENSIONS_ALLOWED
             || (cppcli_enabled && decl_info->is_finalizer)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                           ) {
    /* Constructors may not be virtual functions and unions may not have them.
       C++/CLI finalizers may not be virtual functions. */
    error_code = ec_virtual_not_allowed;
  } else if (decl_state->storage_class == (a_storage_class)sc_static ||
             (locator->is_operator_name &&
              (is_new_operator(locator->variant.opname) ||
               is_delete_operator(locator->variant.opname)))) {
    /* Only nonstatic member functions may be specified as virtual.  This
       applies to operators new and delete since they are always static. */
    error_code = ec_virtual_static_not_allowed;
  }  /* if */
  if (error_code != ec_no_error) {
    pos_diagnostic(severity, error_code,
                   decl_info->is_first_in_declarator_list ?
                                                  &decl_state->start_pos :
                                                  &locator->source_position);
    decl_info->invalid_virtual_specifier = TRUE;
  }  /* if */
}  /* check_for_invalid_use_of_virtual */


static void report_missing_constructor(a_symbol_ptr  tag_sym)
/*
tag_sym is a class/struct/union symbol for which no constructor was
explicitly declared and which has at least one const or ref nonstatic data
member.  Determine whether a diagnostic is actually required and put it out.
*/
{
  a_class_symbol_supplement_ptr  cssp;
  a_symbol_ptr                   sym;
  a_boolean                      any_diagnostics_issued;
  a_class_type_supplement_ptr    ctsp;
  a_type_ptr                     class_type;

  class_type = tag_sym->variant.class_struct_union.type;
  ctsp = class_type_supp(class_type);
  if (tag_sym->kind == (a_symbol_kind)sk_union_tag) {
    /* Note that we do not do this check for unions.  This is partly because
       a union may have a mixture of const and non-const declarations, and
       it's not clear that the const members really need to be initialized. */
  } else if (ctsp->is_lambda_closure_class) {
    /* This is the class generated to represent a lambda.  Suppress the
       constructor check on this class. */
  } else {
    any_diagnostics_issued = FALSE;
    cssp = tag_sym->variant.class_struct_union.extra_info;
    /* If a diagnostic is required, each of the uninitialized const or ref
       members will be listed, so loop through the symbols looking for
       candidates. */
    for (sym = cssp->symbols; sym != NULL; sym = sym->next_in_scope) {
      if (sym->kind == (a_symbol_kind)sk_field) {
        a_field_ptr    field = sym->variant.field.ptr;
        a_type_ptr     tp = field->type;
        an_error_code  error_code;

#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode && field_is_property_or_event(field)) {
          /* A property or event field in Microsoft C++ mode.  This is not a
             real field and therefore the check does not apply. */
          continue;
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        if (is_any_reference_type(tp)) {
          /* Member of reference type must be explicitly initialized. */
          error_code = ec_reference_member;
        } else if (is_const_qualified_type(tp)) {
          /* Usually, a member of const type must be explicitly initialized. */
          if (type_has_user_provided_default_constructor(tp)) {
            /* A const data member that has its own default constructor will
               be initialized when the default constructor for the current
               class is generated.  So skip this one and keep looking. */
            continue;
          }  /* if */
          error_code = ec_const_member;
        } else {
          /* Not a const or ref member.  Keep looking. */
          continue;
        }  /* if */
        if (!any_diagnostics_issued) {
          /* This is the first field for which a diagnostic should be issued.
             Put out the "head" of the message first. */
          pos_sy_start_warning(ec_no_ctor_but_const_or_ref_member,
                               &tag_sym->decl_position, tag_sym);
          /* Remember that a diagnostic has already been issued. */
          any_diagnostics_issued = TRUE;
        }  /* if */
        sym_add_diag_info(error_code, sym);
      }  /* if */
    }  /* for */
    /* If a diagnostic was issue, end the diag-info list. */
    if (any_diagnostics_issued) end_error();
  }  /* if */
}  /* report_missing_constructor */


static void check_if_function_defined_in_class(a_func_info_block   *func_info,
                                               a_member_decl_info  *decl_info)
/*
We've just parsed a member function declarator in a class definition (the
declaration is described by *func_info and *decl_info).  Record in *func_info
if this is part of a function definition.  If that is the case, record some
additional properties related to the definition and issue an error if the
current declarator was preceded by another one sharing the same specifiers
(e.g., "int f(), g() {};" is an error).
*/
{
  if (curr_token == tok_semicolon) {
    /* Probably the most common case: Not a definition. */
  } else if (curr_token == tok_lbrace || curr_token == tok_try) {
    func_info->is_definition = TRUE;
  } else if (decl_info->is_constructor && curr_token == tok_colon) {
    func_info->is_definition = TRUE;
  } else if (curr_token == tok_assign) {
    /* This could be "= default" or "= delete" (which are treated as
       definitions), or a pure-virtual specifier (which can only be a
       definition in Microsoft mode). */
    a_token_cache  cache;
    clear_token_cache(&cache, /*reusable=*/FALSE);
    /* Put the current token (tok_assign) in the cache. */
    cache_curr_token(&cache);
    (void)get_token();
    if (deleted_functions_enabled && curr_token == tok_delete) {
      func_info->is_deleted = TRUE;
      func_info->is_definition = TRUE;
    } else if (defaulted_special_members_enabled &&
               curr_token == tok_default) {
      func_info->is_defaulted = TRUE;
      func_info->is_definition = TRUE;
    } else if (curr_token == tok_int_constant) {
      /* A pure virtual specifier (presumably).  If a definition follows,
         that will usually be diagnosed as an error (elsewhere), but in
         Microsoft compatibility mode it is valid.  Either way, record
         whether a definition does in fact follow. */
      cache_curr_token(&cache);
      /* Advance past the constant and see if the next token is a left
         brace. */
      if (get_token() == tok_lbrace) func_info->is_definition = TRUE;
    }  /* if */
    /* Restore the lexical state. */
    rescan_cached_tokens(&cache);
  }  /* if */
  func_info->is_inline = func_info->is_definition ||
                         (decl_info->decl_state.dso_flags & DSO_INLINE);
  if (func_info->is_definition) {
    if (!decl_info->is_first_in_declarator_list) {
      pos_error(ec_exp_semicolon, &pos_curr_token);
    }  /* if */
#if USER_CONTROL_OF_STRUCT_PACKING
    /* Record the current setting of the maximum alignment for local class
       members (an adjustment may be required for packing). */
    func_info->max_member_alignment =
                                    current_max_alignment_for_class_members();
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  }  /* if */
}  /* check_if_function_defined_in_class */


static void cache_in_class_function_definition(
                                             a_func_info_block   *func_info,
                                             a_member_decl_info  *decl_info,
                                             a_class_def_state   *class_state)
/*
A function definition appears in a class definition described by class_state.
*func_info and *decl_info describe the function declaration.  Skip past the
function definition and cache its tokens if appropriate.
*/
{
  a_symbol_ptr  rout_sym = decl_info->decl_state.sym;
  a_boolean     is_friend = (decl_info->decl_state.dso_flags & DSO_FRIEND);

#if CHECKING
  if (is_friend) {
    /* The inline flag is set for friend functions in decl_friend_function,
       which also handles cases in which it should be left unset despite the
       presence of a function body. */
  } else if (rout_sym->variant.routine.ptr->is_inline) {
    /* The usual case: In-class member function definitions are
       inline. */
#if GNU_EXTENSIONS_ALLOWED
  } else if (rout_sym->variant.routine.ptr->never_inline) {
    /* An in-class definition may have been declared with the "noinline"
       attribute.  (Note: The Microsoft __declspec(noinline) attribute does
       not make a function non-inline.) */
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else if (func_info->is_deleted || func_info->is_defaulted) {
    /* "= delete;" or "= default;" was encountered, but the routine entry is
       not marked as inline.  This can happen in error cases (the "= delete;"
       or "= default;" is essentially discarded). */
    expect_error();
  } else {
    unexpected_condition();
  }  /* if */
#endif /* CHECKING */
  if (func_info->is_deleted || func_info->is_defaulted) {
    /* Consume three tokens: "= delete ;" or "= default ;". */
    check_assertion(curr_token == tok_assign);
    (void)get_token();
    check_assertion(curr_token == tok_delete || curr_token == tok_default);
    (void)get_token();
    (void)required_token(tok_semicolon, ec_exp_semicolon);
  } else {
    /* Cache the tokens comprising the function definition so that they can be
       rescanned once the entire class definition has been processed. */
    a_token_sequence_number  first_token_number, last_token_number;
    a_token_cache            body_cache;
    a_type_ptr               class_type = class_state->class_type;
    if (prescan_function_definition(&first_token_number, &last_token_number,
                                    &body_cache,
                                    (a_boolean)decl_info->is_constructor)) {
      /* Advance past the terminating right brace. */
      (void)get_token();
    }  /* if */
    if (curr_token == tok_semicolon) {
      /* Advance past the optional semicolon. */
      (void)get_token();
    }  /* if */
    if (class_state->is_nonreal_instantiation &&
        !class_type->variant.class_struct_union.is_specialized &&
        !class_type->source_corresp.is_local_to_function) {
      /* The test of is_specialized is done to exclude Microsoft mode
         specializations in a class template scope.  Similarly, a member
         function of a local class of a prototype instantiation is nonreal
         but not a template of itself. */
      if (is_friend) {
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
        /* A template cache segment entry is created for a friend function
           that is defined in a class template.  This information is used
           when creating template strings to eliminate the friend function
           body from the template string. */
        a_template_cache_segment_ptr	tcsp;
        tcsp = alloc_template_cache_segment(
                         rout_sym, (a_template_symbol_supplement_ptr)NULL);
        tcsp->first_token_number = first_token_number;
        tcsp->last_token_number = last_token_number;
        tcsp->is_friend = TRUE;
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
      } else if (rout_sym->variant.routine.instance_ptr == NULL) {
        /* Normally, rout_sym->variant.routine.instance_ptr should be non-
           NULL since we're apparently dealing with a member function of a
           nonreal class.  However, severely ill-formed cases may get here
           nonetheless. */
        expect_error();
      } else {
        /* A member function of a nonreal class serves as a template, and
           since this is the definition the template_info associated with
           this member function must be updated, based on the template_info
           of the prototype instantiation.  Note that the current class may
           be nested within the prototype instantiation. */
        a_template_symbol_supplement_ptr  tssp, class_tssp;
        /* A member function of a template class whose body is supplied in
           the class shares the template declaration information with the
           enclosing class. */
        tssp = rout_sym->variant.routine.instance_ptr->template_info;
        class_tssp = symbol_supplement_for_class(class_type)->template_info;
        /* The body cache is saved here, but will be updated later during
           routine fixup.  This is needed for the generation of template
           strings to be done properly. */
        set_template_cache_info(&tssp->cache, &body_cache,
                                class_tssp->cache.decl_info);
        tssp->cache_segment = alloc_template_cache_segment(rout_sym, tssp);
        tssp->cache_segment->first_token_number = first_token_number;
        tssp->cache_segment->last_token_number = last_token_number;
        /* Save a checksum of this template to be used for cross
           translation unit comparisons. */
        record_cache_checksum(tssp, &body_cache);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* cache_in_class_function_definition */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void scan_microsoft_member_decl_prefix(
                                          a_class_def_state    *class_state,
                                          an_ms_attribute_ptr  *ms_attributes,
                                          a_boolean            *complete_decl)
/*
Scan Microsoft-specific leading components of a member declaration.  Normally,
these are the Microsoft attributes enclosed in square brackets: *ms_attributes
is updated to point to any such attributes.  If attributes are scanned and if
they are followed by a semicolon, *complete_decl is set to TRUE; otherwise,
*complete_decl is to FALSE.  Some Microsoft compilers also have a bug that
allows for a member declaration to start with a left parenthesis: We scan the
parenthesis here (with a warning) and record its presence in *class_state.
The "matching" right parenthesis (which Microsoft compilers accept almost
anywhere in the member declaration) should be consumed using the macro
consume_any_stray_microsoft_rparen.
*/
{
  *complete_decl = FALSE;
  if (curr_token == tok_lbracket) {
    /* A Microsoft attribute of the form "[ ... ]". */
    *ms_attributes = scan_microsoft_attributes(/*is_parameter=*/FALSE);
    if (curr_token == tok_semicolon) {
      /* This is a standalone attribute block.  Make sure all of the specified
         attributes are standalone attributes.  This also sets ms_attributes
         to NULL. */
      if (!is_template_context()) {
        verify_standalone_attributes(ms_attributes);
      } else {
        dispose_of_unapplied_attributes(ms_attributes, ec_ms_attr_not_allowed);
      }  /* if */
      cannot_bind_to_curr_construct();
      (void)get_token();
      *complete_decl = TRUE;
    }  /* if */
  }  /* if */
  class_state->ms_parenthesized_member = FALSE;
  if (microsoft_bugs && microsoft_version >= 1310 && !C_mode() &&
      curr_token == tok_lparen) {
    /* In some Microsoft versions a member declaration can start with a left
       parenthesis that can be closed just about anywhere in the declaration
       (or not at all). */
    warning(ec_microsoft_parenthesized_member);
    (void)get_token();
    if (curr_token != tok_rparen) {
      class_state->ms_parenthesized_member = TRUE;
    } else {
      /* The member declaration started with "()", which is accepted and
         ignored by the Microsoft compiler. */
      (void)get_token();
    }  /* if */
  }  /* if */
}  /* scan_microsoft_member_decl_prefix */


void f_consume_any_stray_microsoft_rparen(void)
/*
Some Microsoft compilers accept a member declaration that starts with a left
parenthesis.  The "matching" right parenthesis can appear almost anywhere in
the declaration (or not at all).  This function consumes the right parenthesis
token and clears the flag that was set when the left parenthesis was scanned.
Call this function through the macro consume_any_stray_microsoft_rparen.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];

  if (ssep->kind == (a_scope_kind)sck_class_struct_union) {
    /* The Microsoft bug only occurs in class scope. */
    a_class_def_state  *class_state = ssep->class_def_state;
    if (class_state->ms_parenthesized_member && curr_token == tok_rparen) {
      /* We're in a member declaration that started with a left parenthesis. */
      (void)get_token();
      class_state->ms_parenthesized_member = FALSE;
    }  /* if */
  }  /* if */
}  /* f_consume_any_stray_microsoft_rparen */


static a_boolean type_specifiers_next(a_token_cache  *cache)
/*
Look ahead in the token stream (saving tokens in the given cache) to see if a
sequence of type specifiers followed by a declarator comes next.  If so,
return TRUE; otherwise, return FALSE.
This is used in C++/CLI mode to determine if the identifiers "delegate",
"event", "initonly", "literal", and "property" should be treated as context-
sensitive keywords.
*/
{
  a_boolean  result = FALSE;

  for (;;) {
    if (is_type_qualifier()) {
      /* A type qualifier is always part of the sequence. */
    } else if (is_type_specifier() || curr_token == tok_colon_colon) {
      /* A keyword introducing a type specifier always denotes a type, which
         implies that the previously-encountered identifier was indeed a
         context-sensitive keyword.  Similarly, since a "::" cannot start a
         member declarator or a delegate declarator, it must introduce a type
         name. */
      result = TRUE;
      goto done;
    } else if (curr_token == tok_identifier) {
      /* An identifier naming a type, on the other hand, could be
         a declarator-id. */
      if (curr_type_symbol(/*is_new_type_name=*/FALSE,
                           /*in_prescan=*/TRUE,
                           /*in_type_check=*/FALSE) == NULL) {
        /* The current identifier is not a type.  So it must be a declarator-id
           and the potential context-sensitive keyword must be a type name. */
        break;
      } else {
        /* The current identifier can be resolved as a type.  It is not a type
           but a declarator-id if it is followed by: a semicolon (end of
           declaration), a left bracket (array field or indexed property), a
           left brace (property definition), an equal sign (in-class
           initializer), or a colon (bit field).  It may also not be a type
           name if it is followed by a left parenthesis (it could be a
           function or delegate declarator).  */
        cache_curr_token(cache);
        (void)get_token();
        if (curr_token == tok_semicolon || curr_token == tok_lbracket ||
            curr_token == tok_lbrace || curr_token == tok_assign ||
            curr_token == tok_colon) {
          break;
        } else if (curr_token == tok_lparen) {
          cache_curr_token(cache);
          (void)get_token();
          if (curr_token == tok_rparen ||
              is_decl_start(IDS_REAL_DECLARATOR_ALLOWED |
                            IDS_MS_ATTRIB_NOT_ALLOWED) ||
              curr_token == tok_ellipsis) {
            /* Function declarator rather than a nested declarator. */
            break;
          }  /* if */
        }  /* if */
        result = TRUE;
        goto done;
      }  /* if */
    } else if (is_declarator_start()) {
      /* The start of a declarator.  Since we haven't seen a type name yet,
         the potential context-sensitive keyword is presumably a type. */
      break;
    } else if (curr_token == tok_end_of_source ||
               curr_token == tok_semicolon ||
               curr_token == tok_lbrace || curr_token == tok_rbrace) {
      /* We've scanned too far: The code contains a syntax error.  Assume a
         context-sensitive keyword since collisions with user-declared
         identifiers are presumably rare. */
      expect_error();
      result = TRUE;
      goto done;
    } else if ((!is_file_or_namespace_scope(&scope_stack_top()) &&
                (curr_token == tok_typedef || curr_token == tok_extern)) ||
               curr_token == tok_friend || curr_token == tok_asm ||
               curr_token == tok_explicit) {
      /* These specifiers cannot appear in a declaration involving a
         context-sensitive specifier and they are unlikely to accidentally
         appear in a malformed declaration involving such a specifier: Don't
         attempt to parse this assuming the identifier is a keyword. */
      break;
    } else if (curr_token == tok_declspec) {
      /* A __declspec attribute: Cache the tokens until (but not including)
         the closing right parenthesis. */
      cache_curr_token(cache);
      (void)get_token();
      if (curr_token != tok_lparen ||
          cache_token_stream_until_matching_token(cache,
                                                  /*coalesce_ids=*/FALSE)) {
        /* A syntax error.  Assume a context-sensitive error. */
        expect_error();
        result = TRUE;
        goto done;
      } else {
        /* The closing parenthesis will be cached below. */
        check_assertion(curr_token == tok_rparen);
      }  /* if */
    } else {
      /* Presumably another specifier token. */
    }  /* if */
    cache_curr_token(cache);
    (void)get_token();
  }  /* for */
done:
  return result;
}  /* type_specifiers_next */


static a_boolean identifier_starts_name_qualifier_or_template_id(void)
/*
The current token is an identifier that might be a C++/CLI context-sensitive
declaration specifier (e.g., "delegate" or "property").  Return TRUE if it
starts a name qualifier (i.e., it is a class, enum, or namespace name followed
by a "::") or if it is followed by a "<" (which presumably means that it
starts a template-id), but is not the special case for "generic <...".
*/
{
  a_boolean     result = FALSE;
  a_token_kind  next_tok = next_token();

  if (next_tok == tok_colon_colon) {
    /* "<identifier>::" Determine if this starts a qualified name.  We cannot
       call is_generalized_identifier_start here because that would trigger
       errors if the identifier doesn't name a tag or namespace.  Copy
       locator_for_curr_id before performing the lookup to avoid biasing
       future lookups. */
    a_symbol_locator  loc;
    loc = locator_for_curr_id;
    result = normal_id_lookup(&loc, IDL_MUST_BE_CLASS_OR_NAMESPACE) != NULL;
  } else if (is_start_of_generic_decl()) {
    /* We have "generic < class" or "generic < typename".  This is the
       start of a generic declaration. */
  } else if (next_tok == tok_lt) {
    result = TRUE;
  }  /* if */
  return result;
}  /* identifier_starts_name_qualifier_or_template_id */


a_boolean check_for_cli_delegate_definition(void)
/*
We're in a scope that allows a C++/CLI delegate definition and any prefix
Microsoft attributes have been scanned.  Check if what follows looks like a
delegate definition.  If it is, return TRUE; otherwise, return FALSE.
*/
{
  a_boolean      result = FALSE;
  a_token_cache  cache;

  clear_token_cache(&cache, /*reusable=*/FALSE);
  while (curr_token == tok_public || curr_token == tok_private) {
    cache_curr_token(&cache);
    (void)get_token();
  }  /* if */
  if (curr_token_is_identifier_string("delegate")) {
    if (identifier_starts_name_qualifier_or_template_id()) {
      /* "delegate" is part of qualified name or template-id and therefore not
          a keyword. */
    } else {
      /* "delegate" is a simple unqualified name: Treat it as a keyword if a
         type specifier follows. */
      cache_curr_token(&cache);
      (void)get_token();
      result = type_specifiers_next(&cache);
    }  /* if */
  }  /* if */
  rescan_cached_tokens(&cache);
  return result;
}  /* check_for_cli_delegate_definition */


static void add_cli_system_base_class(a_class_def_state_ptr  class_state,
                                      a_symbol_ptr           base_type_symbol)
/*
Add to the given C++/CLI class a base class of the type indicated by
base_type_symbol (which is a System::... type).  The given class should have
no other base classes.
*/
{
  a_type_ptr        class_type = class_state->class_type;
  a_base_class_ptr  bcp = alloc_base_class(), last_base = NULL;
  a_boolean         may_be_first_direct_nonvirtual_base = TRUE;

  check_assertion(base_classes_of(class_type) == NULL);
  check_assertion(base_type_symbol != NULL);
  bcp->type = type_symbol_type(base_type_symbol);
  complete_type_is_needed(bcp->type);
  check_assertion(bcp->type != NULL && is_class_struct_union_type(bcp->type));
  bcp->orig_type = bcp->type;
  bcp->derived_class = class_type;
  bcp->direct = TRUE;
  bcp->direct_base_number = 1;
  add_new_direct_base(bcp, class_state, (an_access_specifier)as_public,
                      &last_base, &may_be_first_direct_nonvirtual_base);
}  /* add_cli_system_base_class */


void scan_cli_delegate_definition(a_decl_parse_state  *dps)
/*
The caller has determined that the upcoming tokens look like a C++/CLI
delegate definition (by calling check_for_cli_delegate_definition).  Scan the
definition and record it in the IL (as a special-purpose class type).
*/
{
  an_assembly_visibility       visibility;
  a_source_position            visibility_pos;
  a_decl_pos_block             decl_pos_block;
  a_decl_flag_set              dsi_flags = DSI_TYPE_SPECIFIER_ALLOWED |
                                           DSI_NO_TAG_DEFINITION |
                                           DSI_VACUOUS_TAG_DECL_ALLOWED;
  a_decl_flag_set              di_flags = DI_REAL_DECLARATOR_ALLOWED;
  a_symbol_locator             loc, member_loc;
  a_symbol_ptr                 prev_decl = NULL;
  a_func_info_block            func_info;
  a_type_ptr                   parent_type = NULL, class_type, htype;
  a_class_type_supplement_ptr  ctsp;
  a_scope_depth                decl_level = depth_scope_stack;
  a_class_def_state            class_state;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean                    saved_source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_member_decl_info           member_info;
  a_decl_parse_state           *mdps = &member_info.decl_state;

  clear_decl_pos_block(&decl_pos_block);
  visibility = scan_cli_visibility_specifier_if_any(&visibility_pos);
  check_assertion(curr_token_is_identifier_string("delegate"));
  if (scope_stack_top().kind == (a_scope_kind)sck_class_struct_union) {
    parent_type = scope_stack_top().assoc_type;
    if (!is_immediate_managed_class_type(parent_type)) {
      pos_error(ec_delegate_requires_managed_class, &pos_curr_token);
    }  /* if */
  }  /* if */
  (void)get_token();
  if (scope_stack_top().kind == (a_scope_kind)sck_class_struct_union) {
    dsi_flags |= DSI_IS_MEMBER_DECLARATION;
  }  /* if */
  decl_specifiers(dsi_flags, dps, &decl_pos_block);
  clear_func_info(&func_info);
  declarator(di_flags, dps, /*member_parent_type=*/(a_type_ptr)NULL, &loc,
             &func_info, &decl_pos_block);
  if (is_template_context()) {
    /* Type checks are unreliable: Delay them until a real instantiation.
       E.g. "template<class T> ref struct S { delegate T D; };". */
  } else if (!is_function_type(dps->type)) {
    if (is_error_type(dps->type)) {
      expect_error();
    } else {
      pos_ty_error(ec_invalid_delegate_type, &dps->declarator_pos, dps->type);
      dps->type = error_type();
    }  /* if */
  }  /* if */
  /* If a delegate is generated from an assembly (metadata) file, it was
     previously loaded as an incomplete ref class.  Only in this case is a
     "redeclaration" allowed. */
  prev_decl = curr_scope_id_lookup(&loc, IDL_MUST_BE_TAG);
  if (prev_decl != NULL) {
    class_type = type_symbol_type(prev_decl);
    if (class_type_supp(class_type)->assembly_index != 0) {
      /* The delegate was loaded from an assembly file. */
      a_boolean      is_local = FALSE;
      decl_level = scope_depth_of_symbol(prev_decl, &is_local);
      check_assertion(!is_local);
      dps->sym = prev_decl;
    } else {
      /* A redeclaration of a user-declared delegate: Ignore the previous
         declaration, which will trigger an error later on. */
      prev_decl = NULL;
      class_type = NULL;
      expect_error();
    }  /* if */
  }  /* if */
  if (prev_decl == NULL) {
    /* Create the delegate class type (a sealed ref class). */
    class_type = alloc_type((a_type_kind)tk_struct);
  }  /* if */
  ctsp = class_type_supp(class_type);
  ctsp->cli_class_type_kind = (a_cli_class_type_kind)cctk_ref;
  ctsp->is_hide_by_sig = TRUE;
  class_type->variant.class_struct_union.is_delegate_class = TRUE;
  class_type->variant.class_struct_union.final = TRUE;
  if (prev_decl == NULL) {
    /* Associate a symbol with the new type created above and record
       membership information. */
    dps->sym = enter_local_symbol((a_symbol_kind)sk_class_or_struct_tag, &loc,
                                  decl_level, /*suppress_redecl_error=*/FALSE);
    dps->sym->variant.class_struct_union.type = class_type;
    set_source_corresp(&(class_type->source_corresp), dps->sym);
    update_membership_of_class(dps->sym, /*def_or_vacuous_decl=*/TRUE,
                               decl_level, &dps->start_pos);
    add_to_types_list(class_type, decl_level);
  }  /* if */
  record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, dps->sym,
                            &loc.source_position, dps->source_sequence_entry);
  if (cppcli_enabled) {
    set_cli_visibility(class_type, visibility, &visibility_pos,
                       /*is_definition=*/TRUE);
  }  /* if */
  /* Start the class definition (and associated class scope). */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  class_type->autonomous_primary_tag_decl = TRUE;
  /* Don't issue source sequence entries for generated entities. */
  saved_source_sequence_entries_disallowed =
                                            source_sequence_entries_disallowed;
  scope_stack_top().source_sequence_entries_disallowed = TRUE;
  source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  initialize_class_def_state(class_type, &class_state);
  /* Add System::MulticastDelegate as a base class. */
  add_cli_system_base_class(
            &class_state, cli_symbol_from_kind(csk_system_multicast_delegate));
  class_state.access = (an_access_specifier)as_public;
  ctsp->assoc_scope =
             push_scope((a_scope_kind)sck_class_struct_union, NO_SCOPE_NUMBER,
                        class_type, (a_routine_ptr)NULL);
  scope_stack_top().class_def_state = &class_state;
  /* Add the Invoke member (declaration only). */
  clear_locator(&member_loc, &dps->declarator_pos);
  (void)find_symbol("Invoke", sizeof("Invoke")-1, &member_loc);
  initialize_member_decl_info(&member_info, &dps->specifiers_pos);
  mdps->declared_type = dps->declared_type;
  mdps->type = dps->type;
  decl_member_function(&member_loc, &func_info, &class_state, &member_info,
                       /*compiler_generated=*/TRUE);
  /* Add the one-argument constructor (declaration only). */
  member_loc = loc;
  change_class_locator_into_constructor_locator(&member_loc,
                                                &dps->declarator_pos,
                                                /*is_static_ctor=*/FALSE);
  initialize_member_decl_info(&member_info, &dps->specifiers_pos);
  member_info.is_constructor = TRUE;
  mdps->declared_type = mdps->type =
                  make_routine_type(void_type(), make_pointer_type(dps->type),
                                    /*param2_type=*/(a_type_ptr)NULL,
                                    /*param3_type=*/(a_type_ptr)NULL,
                                    /*param4_type=*/(a_type_ptr)NULL);
  decl_member_function(&member_loc, &func_info, &class_state, &member_info,
                       /*compiler_generated=*/TRUE);
  /* Add static operators "+" and "-" (again, declarations only). */
  make_opname_locator((an_opname_kind)onk_plus, &member_loc,
                      &dps->declarator_pos);
  initialize_member_decl_info(&member_info, &dps->specifiers_pos);
  mdps->storage_class = mdps->declared_storage_class =
                                                   (a_storage_class)sc_static;
  htype = make_handle_type(class_type);
  mdps->declared_type = mdps->type =
                          make_routine_type(htype, htype, htype,
                                            /*param3_type=*/(a_type_ptr)NULL,
                                            /*param4_type=*/(a_type_ptr)NULL);
  decl_member_function(&member_loc, &func_info, &class_state, &member_info,
                       /*compiler_generated=*/TRUE);
  make_opname_locator((an_opname_kind)onk_minus, &member_loc,
                      &dps->declarator_pos);
  initialize_member_decl_info(&member_info, &dps->specifiers_pos);
  mdps->storage_class = mdps->declared_storage_class =
                                                   (a_storage_class)sc_static;
  htype = make_handle_type(class_type);
  mdps->declared_type = mdps->type =
                          make_routine_type(htype, htype, htype,
                                            /*param3_type=*/(a_type_ptr)NULL,
                                            /*param4_type=*/(a_type_ptr)NULL);
  decl_member_function(&member_loc, &func_info, &class_state, &member_info,
                       /*compiler_generated=*/TRUE);
  /* Wrap up the definition. */
  complete_class_definition(class_type, decl_level, &class_state);
  pop_scope();
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Restore the previous state wrt. generating source sequence entries. */
  source_sequence_entries_disallowed =
                                     saved_source_sequence_entries_disallowed;
  scope_stack_top().source_sequence_entries_disallowed 
                                    = saved_source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* scan_cli_delegate_definition */


void scan_cli_delegate_definition_from_assembly_import(void)
/*
Scan a delegate definition generated from assembly metadata.  (Delegates from
assemblies are first loaded as incomplete ref class declarations that can
later be completed via a call to this function.  The caller has already
ensured that the token stream contains a delegate definition corresponding to
the assembly file.)
*/
{
  a_decl_parse_state  dps;

  init_decl_parse_state(&dps);
  scan_cli_delegate_definition(&dps);
}  /* scan_cli_delegate_definition_from_assembly_import */


static a_boolean check_for_cli_field_modifier(a_decl_parse_state  *dps)
/*
This function must be called at the beginning of a class member declaration
in C++/CLI mode and returns TRUE if the following tokens appear to form a
C++/CLI property or event declaration, an initonly field declaration, or a
literal field declaration.  Otherwise, FALSE is returned.  If TRUE is
returned, flags in *dps are set to reflect which kind of declaration was
encountered (i.e., which kind of context-sensitive keyword appeared:
"property", "event", "initonly", or "literal").
*/
{
  a_boolean      result = FALSE, property_or_event_only = FALSE;
  a_token_cache  cache;

  clear_token_cache(&cache, /*reusable=*/FALSE);
  while (curr_token == tok_static || curr_token == tok_virtual) {
    /* Only "property" and "event" (i.e., not "initonly" or "literal") may be
       preceded by "static" or "virtual". */
    property_or_event_only = TRUE;
    cache_curr_token(&cache);
    (void)get_token();
  }  /* if */
  if (curr_token != tok_identifier) {
    /* Not an identifier and hence not a context-sensitive keyword. */
    goto done;
  } else {
    a_symbol_header_ptr  sym_hdr = locator_for_curr_id.symbol_header;
    if (symbol_header_is_for_identifier_string(sym_hdr, "property")) {
      dps->has_cli_property_keyword = TRUE;
    } else if (symbol_header_is_for_identifier_string(sym_hdr, "event")) {
      dps->has_cli_event_keyword = TRUE;
    } else if (property_or_event_only) {
      /* We already ruled out identifiers not spelled "property" or "event". */
      goto done;
    } else if (symbol_header_is_for_identifier_string(sym_hdr, "initonly")) {
      dps->has_cli_initonly_keyword = TRUE;
    } else if (symbol_header_is_for_identifier_string(sym_hdr, "literal")) {
      dps->has_cli_literal_keyword = TRUE;
    } else {
      /* Not one of the identifiers used for context-sensitive keywords. */
      goto done;
    }  /* if */
    if (identifier_starts_name_qualifier_or_template_id()) {
      /* The identifier is part of qualified name or template-id and can
         therefore not be a keyword. */
      goto done;
    }  /* if */
  }  /* if */
  /* Cache the identifier (which now appears likely to be a context-sensitive
     specifier keyword). */
  cache_curr_token(&cache);
  (void)get_token();
  /* For this to be a field-like declaration preceded by a context-sensitive
     keyword, a sequence of decl-specifiers including a type must follow. */
  if (type_specifiers_next(&cache)) {
    result = TRUE;
    dps->has_cli_context_sensitive_keyword = TRUE;
  } else {
    dps->has_cli_property_keyword = FALSE;
    dps->has_cli_initonly_keyword = FALSE;
    dps->has_cli_literal_keyword = FALSE;
  }  /* if */
done:
  rescan_cached_tokens(&cache);
  return result;
}  /* check_for_cli_field_modifier */


static void record_trivial_property_accessors(a_class_def_state  *class_state)
/*
Generate "get" and "set" accessor declarations (but not definitions) for the
trivial property described by class_state->property_or_event_descr.
*/
{
  a_property_or_event_descr_ptr
                      pdp = class_state->property_or_event_descr;
  a_type_ptr          type;
  a_symbol_locator    member_loc;
  a_func_info_block   func_info;
  a_member_decl_info  member_info;
  a_decl_parse_state  *mdps = &member_info.decl_state;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean           saved_source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  int                 k;

  check_assertion(pdp != NULL);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Don't issue source sequence entries for generated entities. */
  saved_source_sequence_entries_disallowed =
                                            source_sequence_entries_disallowed;
  scope_stack_top().source_sequence_entries_disallowed = TRUE;
  source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  for (k = 0; k<2; ++k) {
    /* k == 0: "get", k == 1: "set". */
    a_boolean  is_get = k == 0;
    clear_locator(&member_loc, &pos_curr_token);
    (void)find_symbol(is_get ? (char*)"get" : (char*)"set",
                      (is_get ? sizeof("get") : sizeof("set"))-1,
                      &member_loc);
    member_loc.is_property_or_event_accessor = TRUE;
    initialize_member_decl_info(&member_info, &pos_curr_token);;
    clear_func_info(&func_info);
    if (pdp->is_static) {
      /* The accessor is a static member function. */
      mdps->storage_class = mdps->declared_storage_class =
                                                   (a_storage_class)sc_static;
      member_loc.property_or_event_parent = symbol_for(pdp->variant.variable);
      type = pdp->variant.variable->type;
    } else {
      /* The accessor is a nonstatic, possibly virtual, member function. */
      if (pdp->is_virtual) mdps->dso_flags |= DSO_VIRTUAL;
      member_loc.property_or_event_parent = symbol_for(pdp->variant.field);
      type = pdp->variant.field->type;
    }  /* if */
    /* For a property of type X the "get" signature is "X get()" and the "set"
       signature is "void set(X)". */
    mdps->declared_type = make_routine_type(is_get ? type : void_type(), 
                                            is_get ? (a_type_ptr)NULL : type,
                                            /*param2_type=*/(a_type_ptr)NULL,
                                            /*param3_type=*/(a_type_ptr)NULL,
                                            /*param4_type=*/(a_type_ptr)NULL);
    if (!pdp->is_static) {
      /* The accessor must have a "this" parameter. */
      mdps->declared_type->variant.routine.extra_info->this_class =
                                                      class_state->class_type;
    }  /* if */
    mdps->type = mdps->declared_type;
    decl_member_function(&member_loc, &func_info, class_state, &member_info,
                         /*compiler_generated=*/TRUE);
  }  /* for */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Restore the previous state wrt. generating source sequence entries. */
  source_sequence_entries_disallowed =
                                     saved_source_sequence_entries_disallowed;
  scope_stack_top().source_sequence_entries_disallowed 
                                    = saved_source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* record_trivial_property_accessors */


static void scan_cli_property_indices(a_property_or_event_descr_ptr  pdp)
/*
Scan a list of C++/CLI property index types and record them in the pdp->indices
list.
*/
{
  /* Skip over the left bracket. */
  check_assertion(curr_token == tok_lbracket);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  pdp->indices_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  (void)get_token();
  if (curr_token == tok_rbracket) {
    pos_error(ec_empty_property_indices, &pos_curr_token);
  } else {
    /* Scan the list of types. */
    a_property_index_type_ptr  *p_pitp = &pdp->indices;
    add_stop_token(tok_rbracket);
    do {
      add_stop_token(tok_comma);
      *p_pitp = alloc_property_index_type();
      (*p_pitp)->position = pos_curr_token;
      type_name(&(*p_pitp)->type);
      if (is_void_type((*p_pitp)->type)) {
        pos_error(ec_void_property_index_type, &(*p_pitp)->position);
        (*p_pitp)->type = error_type();
      }  /* if */
      p_pitp = &(*p_pitp)->next;
      remove_stop_token(tok_comma);
    } while (loop_token(tok_comma));
    remove_stop_token(tok_rbracket);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  pdp->indices_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  (void)required_token(tok_rbracket, ec_exp_rbracket);
}  /* scan_cli_property_indices */


static void scan_cli_property_or_event_head(
                                          a_class_def_state   *class_state,
                                          a_member_decl_info  *decl_info,
                                          a_decl_pos_block    *decl_pos_block)
/*
A C++/CLI property or event declaration is next.  Scan its "head" and update
the IL and symbol table accordingly.  The "head" of a property declaration has
the following syntax:
   property-or-event-modifier(opt) property type-specifier-seq declarator
     property-indices(opt) {-or-;
where the optional property-or-event-modifier is either the "static" or
"virtual" keyword, and the final token is a left brace (introducing accessor
member declarations) or a semicolon (in the case of a trivial scalar property).
The "head" of an event declaration is similar:
   property-or-event-modifier(opt) event type-specifier-seq ^(opt)
     identifier {-or-;
*class_state holds information of the enclosing class (whose definition is
being parsed), *decl_info describes the current member declaration, and
*decl_pos_block tracks extended position information.
*/
{
  a_decl_parse_state    *dps = &decl_info->decl_state;
  a_symbol_locator      loc;
  a_type_ptr            class_type = class_state->class_type;
  a_class_type_supplement_ptr
                        ctsp = class_type_supp(class_type);
  a_decl_flag_set       dsi_flags = DSI_TYPE_SPECIFIER_ALLOWED |
                                    DSI_NO_TAG_DEFINITION |
                                    DSI_VACUOUS_TAG_DECL_ALLOWED |
                                    DSI_IS_MEMBER_DECLARATION;
  a_property_or_event_descr_ptr
                        pdp = alloc_property_or_event_descr();
  a_boolean             ptr_to_member_scanned;
  a_source_position     decl_pos, type_pos;
  a_boolean             is_property = dps->has_cli_property_keyword;

  add_stop_token(tok_semicolon);
  add_stop_token(tok_lbrace);
  decl_pos = pos_curr_token;
  if (is_property) {
    pdp->kind = (a_property_or_event_kind)pek_cli_property;
  } else {
    check_assertion(dps->has_cli_event_keyword);
    pdp->kind = (a_property_or_event_kind)pek_cli_event;
  }  /* if */
  class_state->class_aggregate_ruled_out = TRUE;
  class_state->POD_ruled_out = TRUE;
  /* First scan leading static/virtual keywords, and skip over the "property"
     or "event" token. */
  while (curr_token == tok_static || curr_token == tok_virtual) {
    if (curr_token == tok_static) {
      if (pdp->is_static) pos_error(ec_dupl_decl_specifier, &pos_curr_token);
      pdp->is_static = TRUE;
    } else {
      /* Microsoft compilers only warn about duplicate "virtual" keywords. */
      if (pdp->is_virtual) pos_warning(ec_dupl_decl_specifier,
                                       &pos_curr_token);
      pdp->is_virtual = TRUE;
    }  /* if */
    (void)get_token();
  }  /* while */
  if (pdp->is_static && pdp->is_virtual) {
    pos_error(is_property ? ec_virtual_static_property
                          : ec_virtual_static_event,
              &decl_pos);
    pdp->is_static = FALSE;
  }  /* if */
  check_assertion(is_property ? curr_token_is_identifier_string("property")
                              : curr_token_is_identifier_string("event"));
#if EXTRA_SOURCE_POSITIONS_IN_IL
  pdp->property_or_event_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (!is_immediate_managed_class_type(class_type)) {
    pos_error(is_property ? ec_property_requires_managed_class
                          : ec_event_requires_managed_class,
              &pos_curr_token);
  }  /* if */
  (void)get_token();
  type_pos = pos_curr_token;
  decl_specifiers(dsi_flags, dps, decl_pos_block);
  /* Most nontype specifiers will have been diagnosed by decl_specifiers (or
     caused check_for_cli_field_modifier not to treat "property" or "event" as
     a keyword): */
  if (dps->dso_flags & DSO_VIRTUAL) {
    pos_error(ec_virtual_not_allowed, &dps->virtual_pos);
    if (!pdp->is_static) pdp->is_virtual = TRUE;
  }  /* if */            
  dps->type = pointer_declarator(dps->type, dps, /*reference_allowed=*/TRUE,
                                 (a_call_conv_descr_ptr)NULL,
                                 (a_call_conv_descr_ptr)NULL,
                                 (a_type_qualifier_set *)NULL,
                                 (a_type_qualifier_set *)NULL,
                                 &ptr_to_member_scanned, decl_pos_block);
  /* The type constraints for properties and events differ. */
  if (is_property) {
    if (is_array_type(dps->type) || is_function_type(dps->type)) {
      pos_error(is_array_type(dps->type) ? ec_array_type_not_allowed
                                         : ec_function_type_not_allowed,
                &type_pos);
      dps->type = error_type();
    }  /* if */
  } else {
    /* An event's type must be handle-to-delegate. */
    if (is_handle_type(dps->type)) {
      a_type_ptr  underlying_tp = type_pointed_to(dps->type);
      if (is_template_param_type(underlying_tp) ||
          is_error_type(underlying_tp)) {
        /* More specific checking is not needed or possible. */
      } else {
        complete_type_is_needed(underlying_tp);
        if (!is_delegate_type(underlying_tp)) {
          pos_error(ec_invalid_event_type, &type_pos);
          dps->type = error_type();
        }  /* if */
      }  /* if */
    } else if (!is_template_param_type(dps->type) &&
               !is_error_type(dps->type)) {
      pos_error(ec_invalid_event_type, &type_pos);
      dps->type = error_type();
    }  /* if */
  }  /* if */
  /* An identifier should be next. */
  if (!required_token_no_advance(tok_identifier, ec_exp_identifier)) {
    discard_curr_construct_pragmas();
    goto done;
  } else if (!is_generalized_identifier_start(GID_ERROR_FLAGS)) {
    /* Since we already checked that the next token is an identifier, this
       can only be a pointer-to-member. */
    check_assertion(curr_token == tok_ptr_to_member);
    syntax_error(ec_exp_identifier);
  } else if (is_property && curr_token_is_identifier_string("default")) {
    /* "default" (which is an identifier and not a keyword in Microsoft
       modes) has a special meaning for properties but not for events. */
    if (pdp->is_static) {
      pos_error(ec_static_default_indexed_property, &pos_curr_token);
    } else {
      pdp->is_default_indexed = TRUE;
    }  /* if */
  }  /* if */
  class_state->property_or_event_descr = pdp;
  dps->is_property_or_event_field = TRUE;
  loc = locator_for_curr_id;
  (void)get_token();
  if (is_property) {
    /* Check for index types. */
    if (curr_token == tok_lbracket) {
      scan_cli_property_indices(pdp);
    } else if (pdp->is_default_indexed) {
      pos_error(ec_exp_lbracket, &pos_curr_token);
    }  /* if */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  pdp->definition_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (pdp->is_static) {
    decl_static_data_member(&loc, class_state, decl_info);
    check_assertion(dps->sym != NULL &&
                    dps->sym->kind == (a_symbol_kind)sk_static_data_member);
    pdp->variant.variable = dps->sym->variant.static_data_member.variable;
  } else {
    pdp->variant.field = decl_nonstatic_data_member(&loc, class_state,
                                                    decl_info,
                                                    depth_scope_stack);
  }  /* if */
  ctsp->has_direct_property_or_event = TRUE;
  if (curr_token == tok_semicolon) {
    /* A trivial scalar property or event. */
    pdp->is_trivial = TRUE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    pdp->definition_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (is_property) {
      if (pdp->indices != NULL) {
        /* A trivial property cannot be an indexed property. */
        pos_error(ec_trivial_indexed_property, &pos_curr_token);
        pdp->indices = NULL;
      } else if (is_any_reference_type(dps->type)) {
        /* A trivial property cannot have a reference type. */
        pos_error(ec_trivial_reference_property, &type_pos);
      } else if (get_type_qualifiers(dps->type) & (TQ_CONST | TQ_VOLATILE)) {
        /* A trivial property cannot have a const or volatile type. */
        pos_error(ec_trivial_const_or_volatile_property, &type_pos);
      }  /* if */
      record_trivial_property_accessors(class_state);
    }  /* if */
    (void)get_token();
    class_state->property_or_event_descr = NULL;
  } else {
    /* A nontrivial property or event. */
    (void)required_token(tok_lbrace, ec_exp_lbrace);
    class_state->property_or_event_descr = pdp;
    treat_declaration_as_okay_in_property_or_event(class_state);
  }  /* if */
done:
  remove_stop_token(tok_lbrace);
  remove_stop_token(tok_semicolon);
}  /* scan_cli_property_or_event_head */


static void check_cli_accessor_decl(a_class_def_state   *class_state,
                                    a_source_position   *diag_pos)
/*
A member declaration was just completed within the braces of a C++/CLI
property or event definition.  Check that the declaration is valid and issue
a diagnostic at the given position if it is not so.  If the closing brace of
the property or event declaration is next, scan it, diagnose any missing
accessor functions, and update the IL accordingly.  class_state represents the
innermost function being defined.
*/
{
  a_property_or_event_descr_ptr  pedp = class_state->property_or_event_descr;

  if (!class_state->current_decl_valid_in_property_or_event_def) {
    an_error_code  ec;
    if (pedp->kind == (a_property_or_event_kind)pek_cli_property) {
      ec = ec_invalid_property_accessor_decl;
    } else {
      check_assertion(pedp->kind == (a_property_or_event_kind)pek_cli_event);
      ec = ec_invalid_event_accessor_decl;
    }  /* if */
    pos_error(ec, diag_pos);
  } else {
    /* Reset the flag for a possible subsequent declaration. */
    class_state->current_decl_valid_in_property_or_event_def = FALSE;
  }  /* if */
  if (curr_token == tok_rbrace) {
    /* The closing brace of a nontrivial property or event definition.  Skip
       over the token and update class_state to indicate we're no longer in a
       property or event definition.  Also check that any required accessor
       functions have been declared. */
    a_source_position_ptr  decl_pos;
    if (pedp->is_static) {
      decl_pos = &pedp->variant.variable->source_corresp.decl_position;
    } else {
      decl_pos = &pedp->variant.field->source_corresp.decl_position;
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    pedp->definition_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    if (pedp->kind == (a_property_or_event_kind)pek_cli_property) {
      /* Check that at least one of the "get" and "set" accessors have been
         declared. */
      if (pedp->get_routine.ptr == NULL && pedp->set_routine.ptr == NULL) {
        pos_error(ec_missing_get_and_set_accessors, decl_pos);
      }  /* if */
    } else {
      /* Check that both "add" and "remove" have been declared. */
      if (pedp->add_routine == NULL || pedp->remove_routine == NULL) {
        pos_error(ec_missing_add_or_remove_accessor, decl_pos);
      }  /* if */
    }  /* if */
    (void)get_token();
    class_state->property_or_event_descr = NULL;
  }  /* if */
}  /* check_cli_accessor_decl */


static void scan_named_overrides_if_any(a_member_decl_info_ptr  decl_info)
/*
decl_info describes a member function declaration in a class definition.  The
top-level function declarator has just been scanned.  It may be followed by
"= X, Y, Z, ..." (with X, Y, Z, ... qualified or unqualified names), to name
specific virtual base class members that are overridden by the newly declared
member.  If so, scan and validate the "= X, Y, Z, ..." construct and record
the overridden base class members in decl_info->named_overrides.
*/
{
  if (curr_token == tok_assign) {
    a_token_cache            cache;
    a_symbol_list_entry_ptr  *p_list_entry;
    clear_token_cache(&cache, /*reusable=*/FALSE);
    cache_curr_token(&cache);
    (void)get_token();
    if (!is_generalized_identifier_start(GID_NO_OPTIONS)) {
      rescan_cached_tokens(&cache);
      goto done;
    }  /* if */
    discard_token_cache(&cache);
    p_list_entry = &decl_info->named_overrides;
    do {
      a_symbol_ptr  sym;
      a_boolean     err;
      if (!is_generalized_identifier_start(GID_NO_OPTIONS)) goto done;
      add_stop_token(tok_comma);
      sym = coalesce_and_lookup_generalized_identifier(
                                            GID_NO_OPTIONS, ilm_normal, &err);
      remove_stop_token(tok_comma);
      if (err) {
        expect_error();
        sym = NULL;
      } else if (sym == NULL) {
        /* The symbol was not found: Issue an error. */
        pos_error(ec_override_name_must_be_a_base_class_member_function,
                  &pos_curr_token);
      } else if (is_nontype_template_param_symbol(sym)) {
        /* A template-dependent symbol (possibly due to the presence of a
           dependent base class).  Further checks are not possible at this
           time. */
      } else if (sym->ambiguous) {
        pos_sy_error(ec_ambiguous_name, &pos_curr_token, sym);
        sym = NULL;
      } else if (!is_member_function_symbol(sym)) {
        pos_error(ec_override_name_must_be_a_base_class_member_function,
                  &pos_curr_token);
        sym = NULL;
      } else {
        sym = member_function_redecl_sym_with_template_flag(
                                              sym, decl_info->decl_state.type,
                                              (a_template_param_ptr)NULL,
                                              /*templates_only=*/FALSE,
                                              (a_symbol_ptr*)NULL);
        if (sym != NULL) {
          a_routine_ptr  rp;
          check_assertion(sym->kind == (a_symbol_kind)sk_member_function);
          rp = sym->variant.routine.ptr;
          if (!rp->is_virtual) {
            pos_sy_error(ec_override_name_nonvirtual, &pos_curr_token, sym);
            sym = NULL;
          }  /* if */
        }  /* if */
      }  /* if */
      if (sym != NULL) {
        *p_list_entry = alloc_symbol_list_entry();
        (*p_list_entry)->symbol = sym;
        p_list_entry = &(*p_list_entry)->next;
      }  /* if */
      (void)get_token();
    } while (loop_token(tok_comma));
done:;
  }  /* if */
}  /* scan_named_overrides_if_any */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean member_declarator(a_class_def_state_ptr    class_state,
                                   a_member_decl_info_ptr   decl_info,
                                   a_symbol_locator         *locator,
                                   a_func_info_block        *func_info,
                                   a_boolean                *is_function,
                                   a_boolean                *is_typedef)
/*
Parse a declarator for an in-class declaration (which might be a friend
declarator, and therefore not actually the declaration of a class member).
class_state describes the class in which the declarator appears and decl_info
describes the current member declaration as a whole.  Return through *locator
a description of the declarator-id and through *is_function if this declarator
is for a function (in which case *func_info will contain a description of the
top-level function declarator).  The caller should set *is_typedef to TRUE if
the declaration contained a typedef specifier, but this routine may clear that
flag if error recovery should be performed as if the specifier didn't occur.
*/
{
  a_boolean           okay = TRUE;
  a_decl_parse_state  *dps = &decl_info->decl_state;
  a_type_ptr          class_type = class_state->class_type;
  a_boolean           no_decl_specifiers, is_member_template_rescan;
  a_boolean           friend_specified;

  add_stop_token(tok_colon);
  add_stop_token(tok_try);
  clear_func_info(func_info);
  no_decl_specifiers = (dps->dso_flags & DSO_NO_DECL_SPECIFIERS) != 0;
  friend_specified = (dps->dso_flags & DSO_FRIEND) != 0;
  is_member_template_rescan = (scope_stack[depth_scope_stack].kind ==
                                 (a_scope_kind)sck_template_instantiation);
  /* Initialize certain decl_info fields for each declarator. */
  decl_info->is_unnamed_field = FALSE;
  dps->sym = NULL;
  if (!decl_info->is_first_in_declarator_list) {
    /* Check if a secondary declarator declares a constructor or destructor.
       (In C++/CLI mode, also consider static constructors and finalizers.) */
    decl_info->is_destructor = decl_info->is_constructor = FALSE;
    dps->dso_flags &= ~(DSO_CONSTRUCTOR | DSO_DESTRUCTOR);
#if MICROSOFT_EXTENSIONS_ALLOWED
    decl_info->is_static_constructor = FALSE;
    decl_info->is_finalizer = FALSE;
    dps->dso_flags &= ~(DSO_STATIC_CONSTRUCTOR | DSO_FINALIZER);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (curr_token == tok_compl ||
        (is_generalized_identifier_start(GID_NO_OPTIONS) &&
         locator_for_curr_id.is_destructor_name)) {
      decl_info->is_destructor = TRUE;
      dps->type = dps->declared_type = unknown_type();
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (cppcli_enabled &&
               (curr_token == tok_not ||
                (is_generalized_identifier_start(GID_NO_OPTIONS) &&
                 locator_for_curr_id.is_finalizer_name))) {
      decl_info->is_finalizer = TRUE;
      dps->type = dps->declared_type = unknown_type();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (curr_token == tok_identifier &&
               is_constructor_decl(class_type, dps)) {
      if (dps->declared_storage_class != (a_storage_class)sc_static) {
        decl_info->is_constructor = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else {
        check_assertion(cppcli_enabled);
        decl_info->is_static_constructor = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
      dps->type = dps->declared_type = unknown_type();
    } else if (no_decl_specifiers) {
      dps->start_pos = pos_curr_token;
      dps->type = dps->declared_type = integer_type((an_integer_kind)ik_int);
    }  /* if */
  }  /* if */
  if (curr_token == tok_colon && !no_decl_specifiers) {
    decl_info->is_unnamed_field = TRUE;
  }  /* if */
  /* The declarator can be omitted in some cases. */
  set_err_pos_to_curr_token();
  if (decl_info->is_unnamed_field || decl_info->is_anonymous_union) {
    set_to_error_locator(*locator);
    check_pending_qualifiers_used(dps);
  } else if (no_decl_specifiers && !decl_info->is_constructor &&
             !decl_info->is_destructor &&
#if MICROSOFT_EXTENSIONS_ALLOWED
             !decl_info->is_finalizer &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
             !is_declarator_start()) {
    /* No specifiers and no declarator: Issue a syntax error. */
    remove_stop_token(tok_comma);
    remove_stop_token(tok_colon);
    remove_stop_token(tok_try);
    syntax_error(ec_exp_declaration);
    if (curr_token == tok_semicolon) {
      /* Advance past the semicolon. */
      (void)get_token();
    }  /* if */
    discard_curr_construct_pragmas();
    okay = FALSE;
    goto done;
  } else {
    /* Named member -- we need to call declarator. */
    a_decl_flag_set  di_flags = DI_REAL_DECLARATOR_ALLOWED;
    if (C_mode()) {
      /* Must be a field (= nonstatic data member) in C mode. */
      di_flags |= DI_NONSTATIC_MEMBER;
    } else {
      /* C++ mode */
      if (curr_routine_fixup != NULL) {
        /* We must be in a declarator list and this must be at least the
           second item in the list. */
        /* This should not be a cached function body. */
        check_assertion(curr_routine_fixup->
                        function_body_token_cache.first_token == NULL);
        if (curr_routine_fixup->def_arg_expr_fixup_list != NULL) {
           /* The previous one must have been a routine declaration with
              default arguments, so we have to save the routine fixup entry
              onto the fixup list. */
          add_to_routine_fixup_list(curr_routine_fixup);
          /* Make a new one fixup entry for the current declarator. */
          curr_routine_fixup = alloc_routine_fixup(class_type);
        } else {
          /* The other one can be reused. */
          check_assertion(same_entities(curr_routine_fixup->class_type,
                                        class_type));
        }  /* if */
      } else if (!is_member_template_rescan) {
        /* Normal case.  Allocate a new routine fixup entry. */
        curr_routine_fixup = alloc_routine_fixup(class_type);
      }  /* if */
      add_stop_token(tok_lbrace);
      /* Set the various flags for declarator processing (C++ only). */
      di_flags |= DI_OPERATOR_NAME_ALLOWED;
      if ((dps->dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) == 0 &&
          dps->qualifiers == TQ_NONE) {
        di_flags |= DI_NO_TYPE_SPECIFIERS;
      }  /* if */
      if (decl_info->is_constructor) di_flags |= DI_IS_CONSTRUCTOR;
      if (friend_specified) {
        di_flags |= DI_QUALIFIED_NAME_ALLOWED;
      }  /* if */
      if (decl_info->is_member_template) {
        di_flags |= DI_IS_TEMPLATE_DECLARATION;
      }  /* if */
    }  /* if */
    /* Microsoft compilers allow redundant qualifiers when declaring
       members.  In g++ mode, allow additional qualifiers when rescanning
       a member template declaration to generate a partial instantiation.
       The initial declaration is accepted in g++ mode as a result of
       processing in simplify_curr_class_qualified_name. */
    if (microsoft_mode ||
        (gpp_mode && scope_stack[depth_scope_stack].kind ==
                                 (a_scope_kind)sck_template_instantiation)) {
      di_flags |= DI_QUALIFIED_NAME_ALLOWED;
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if (gnu_attributes_enabled) {
      /* Scan prefix declarator attributes.  Note that those can only
         appear after a comma separating two declarators.  Any attributes
         prefixing a leading declarator will have been parsed as part of
         the specifier attributes.  The GNU documentation says that such
         attributes apply to the entity associated with the subsequent
         declarator only.  However, in reality, the GNU compiler appears
         to ignore these prefix declarator attributes altogether when they
         appear on class members.  We implement the documented behavior. */
      scan_gnu_declarator_attributes(dps);
    }  /* if */
    if (gnu_mode) {
      if (gcc_mode && depth_innermost_function_scope != NO_SCOPE_DEPTH) {
        /* GNU C allows VLA fields in local classes.  We will scan such
           fields in GNU C mode, but issue a warning that the field will
           be treated as an array of length zero. */
        di_flags |= DI_VLA_ALLOWED;
      }  /* if */
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    /* Pass the class's type pointer to declarator if this might be a
       nonstatic member function, in which case its presence will cause an
       implicit "this" parameter type to be created. (Static member
       functions do not have an implicit "this" pointer. The class pointer
       will be ignored for data members.) */
    declarator(di_flags, dps, class_type, locator, func_info,
               &decl_info->decl_pos_block);
    if (!C_mode()) {
      remove_stop_token(tok_lbrace);
      check_completed_member_type(locator, class_state, decl_info);
      if (is_member_template_rescan) {
        if (dps->storage_class != (a_storage_class)sc_unspecified &&
            dps->storage_class != (a_storage_class)sc_static) {
          /* An error will already have been issued on, e.g.,
               struct A { template <class T> typedef A (T) { } };
          */
          dps->storage_class = (a_storage_class)sc_unspecified;
          *is_typedef = FALSE;
        }  /* if */
      }  /* if */
      decl_info->is_constructor = (dps->do_flags & DO_IS_CONSTRUCTOR) != 0;
      decl_info->is_destructor = locator->is_destructor_name ||
                                 (dps->do_flags & DO_IS_DESTRUCTOR) != 0;
    }  /* if */
    *is_function = (!*is_typedef && is_function_type(dps->type));
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cppcli_enabled && *is_function) {
      if ((dps->do_flags & DO_IS_STATIC_CONSTRUCTOR) != 0) {
        decl_info->is_static_constructor = TRUE;
        check_assertion(dps->type->kind == (a_type_kind)tk_routine);
        if (function_type_params(dps->type) != NULL) {
          /* C++/CLI static constructors cannot have parameters. */
          pos_error(ec_static_constructor_with_params, &dps->start_pos);
        }  /* if */
      } else if (locator->is_finalizer_name ||
                 (dps->do_flags & DO_IS_FINALIZER) != 0) {
        decl_info->is_finalizer = TRUE;
      }  /* if */
      scan_named_overrides_if_any(decl_info);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
    scan_gnu_asm_name(dps);
    scan_gnu_declarator_attributes(dps);
#endif /* GNU_EXTENSIONS_ALLOWED */
  }  /* if */
  /* In-class member declarations are never "redeclarations".  For friend
     declarations, the setting of the flag depends on the context; if
     appropriate, it will be set to TRUE later. */
  dps->first_decl = !friend_specified;
  remove_stop_token(tok_colon);
  remove_stop_token(tok_try);
done:
  return okay;
}  /* member_declarator */

#if !GENERATE_SOURCE_SEQUENCE_LISTS
/*ARGSUSED*/ /* instance and template_decl is not used unless source
                sequence lists are generated. */
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS */
static a_symbol_ptr class_member_declaration(
                      a_type_ptr               class_type,
                      a_class_def_state_ptr    class_state,
                      an_ms_attribute_ptr      ms_attributes,
                      a_boolean                is_member_template,
                      a_template_param_ptr     templ_param_list,
                      a_boolean                *skip_semicolon_check,
                      a_type_ptr               *member_template_instance_type,
                      a_template_instance_ptr  instance,
                      a_template_ptr           il_template_entry,
                      a_decl_pos_block_ptr     decl_pos_block_ptr)
/*
Scan a member declaration appearing inside a class definition.  class_type
is the type of the class.  class_state points to a block of information
tracking general information about the class.  ms_attributes points to a
list of Microsoft attributes that have already been parsed for this
declaration (if any). *skip_semicolon_check is returned TRUE if the caller
should suppress the check for a semicolon following the member declaration.
templ_param_list is non-NULL for function template declarations.
decl_pos_block_ptr is non-NULL when then extra source position information
collected during this declaration needs to be returned to the caller.
If prototype instantiations are recorded in the IL, the template header is
passed via template_decl.  
*/
{
  a_boolean            missing_declarator = FALSE;
  a_decl_flag_set      dsi_flags;
  a_decl_flag_set      dso_flags = DSO_NO_OUTPUT_FLAGS;
  a_type_ptr           specifiers_type;
  a_boolean            is_typedef;
  a_boolean            no_decl_specifiers;
  a_boolean            friend_specified;
  a_boolean            type_explicitly_specified;
  a_boolean            mutable_specified;
  a_symbol_ptr         rout_sym;
  a_member_decl_info   decl_info;
  a_decl_parse_state   *decl_state = &decl_info.decl_state;
  a_boolean            is_member_template_rescan;
  a_type_qualifier_set saved_qualifiers;
  a_source_position    saved_qualifiers_pos;

  db_enter(3, "class_member_declaration");
  *skip_semicolon_check = FALSE;
  initialize_member_decl_info(&decl_info, &pos_curr_token);
  is_member_template_rescan = (scope_stack[depth_scope_stack].kind ==
                                 (a_scope_kind)sck_template_instantiation);
  /* Scan prefix attributes. */
  decl_state->prefix_attributes = scan_attributes(al_prefix);
  /* Set the flags to control the calls to decl_specifiers. */
  dsi_flags = DSI_TYPE_SPECIFIER_ALLOWED |
              DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER |
              DSI_IS_MEMBER_DECLARATION;
#if ASM_FUNCTION_ALLOWED
  dsi_flags |= DSI_ASM_ALLOWED;
#endif /* ASM_FUNCTION_ALLOWED */
  if (C_dialect == C_dialect_cplusplus) {
    dsi_flags |= (DSI_STORAGE_CLASS_SPECIFIER_ALLOWED | DSI_INLINE_ALLOWED |
                  DSI_EMPTY_DECL_SPECIFIERS_ALLOWED |
                  DSI_VACUOUS_TAG_DECL_ALLOWED);
    if (is_member_template) {
      dsi_flags |= DSI_IS_TEMPLATE_DECLARATION;
      decl_info.is_member_template = TRUE;
    }  /* if */
  }  /* if */
  /* Allow specifier attributes. */
  if (std_attributes_enabled)  dsi_flags |= DSI_STD_ATTRIBUTES_ALLOWED;
  if (gnu_attributes_enabled) dsi_flags |= DSI_GNU_ATTRIBUTES_ALLOWED;
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode) {
    dsi_flags |= DSI_EMPTY_DECL_SPECIFIERS_ALLOWED;
    if (curr_token == tok_extension) {
      dsi_flags |= DSI_MARKED_AS_GNU_EXTENSION;
      decl_state->marked_as_gnu_extension = TRUE;
      (void)get_token();
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* Record any Microsoft attributes in *decl_state before calling
       decl_specifiers, because that call may append additional attributes. */
    decl_state->ms_attributes = ms_attributes;
    if (cppcli_enabled) {
      /* Look ahead to see if the current declaration is for a field or
         property using a C++/CLI context-sensitive keyword "property",
         "event", "initonly", or "literal". */
      if (check_for_cli_field_modifier(decl_state)) {
        if (decl_state->has_cli_property_keyword ||
            decl_state->has_cli_event_keyword) {
          scan_cli_property_or_event_head(class_state, &decl_info,
                                          &decl_info.decl_pos_block);
          *skip_semicolon_check = TRUE;
          goto next_declaration;
        } else {
          /* Just skip the "literal" or "initonly" token that is next. */
          (void)get_token();
        }  /* if */
      } else if (check_for_cli_delegate_definition()) {
        scan_cli_delegate_definition(decl_state);
        cannot_bind_to_curr_construct();
        goto next_declaration;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* First scan the declaration specifiers.  In C++ the specifiers may be
     omitted, e.g., for a function member with implicit type. */
  add_stop_token(tok_colon);
  decl_specifiers(dsi_flags, decl_state, &decl_info.decl_pos_block);
  dso_flags = decl_state->dso_flags;
  no_decl_specifiers = (dso_flags & DSO_NO_DECL_SPECIFIERS) != 0;
  type_explicitly_specified =
                           (dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) != 0;
  friend_specified = dso_flags & DSO_FRIEND;
  if (friend_specified) {
    class_state->any_friend_decls = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    class_state->potentially_interface_like = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  decl_info.is_constructor = (dso_flags & DSO_CONSTRUCTOR) != 0;
  decl_info.is_destructor = (dso_flags & DSO_DESTRUCTOR) != 0;
#if MICROSOFT_EXTENSIONS_ALLOWED
  decl_info.is_static_constructor = (dso_flags & DSO_STATIC_CONSTRUCTOR) != 0;
  decl_info.is_finalizer = (dso_flags & DSO_FINALIZER) != 0;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  mutable_specified = (dso_flags & DSO_MUTABLE) != 0;
  is_typedef =
            decl_state->declared_storage_class == (a_storage_class)sc_typedef;
  remove_stop_token(tok_colon);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    consume_any_stray_microsoft_rparen();
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED
  if (microsoft_mode || sun_mode) {
    if (decl_state->specifiers_type == NULL && type_explicitly_specified) {
      /* A friend declaration of the form "friend class X;" where "X" is a
         class template. */
      check_assertion(friend_specified && curr_token == tok_semicolon);
      goto next_declaration;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode && decl_state->ms_attributes != NULL) {
    if (is_member_template || is_member_template_rescan ||
        is_template_context() ||
        class_type->source_corresp.is_local_to_function) {
      /* Microsoft attributes cannot be specified on templates, nor on members
         of local class types.  When rescanning member templates, there is no
         need to repeat the diagnostic.*/
      an_error_code  ec = is_member_template_rescan ? ec_no_error
                                                    : ec_ms_attr_not_allowed;
      dispose_of_unapplied_attributes(&decl_state->ms_attributes, ec);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (!C_mode() && (dso_flags & DSO_DEFINES_SOMETHING) &&
      !is_error_type(decl_state->type)) {
    /* Should be a class, struct, union, or enum definition. */
    a_type_ptr    tp = skip_typerefs(decl_state->type);
    a_symbol_ptr  sym = (a_symbol_ptr)(tp->source_corresp.assoc_info);

    if (is_member_template) {
      if (curr_token == tok_semicolon && !is_typedef) {
        /* Issue an error later, based on the symbol. */
        decl_info.decl_state.sym = sym;
      }  /* if */
#if CHECKING
    } else if (!sym->is_error) {
      /* A nested class, struct, union, or enum definition.  Be sure the
         parent class was marked correctly.  In GNU or Microsoft modes, this
         could also be a delayed nested class definition appearing in a class
         scope. */
      check_assertion_str2(sym->is_class_member &&
                           (sym_parent_class(sym) == class_type ||
                            microsoft_mode || gpp_mode),
                           "class_member_declaration:",
                           "bad parent type on nested type");
#endif /* CHECKING */
    }  /* if */
  } /* if */
  if (dso_flags & DSO_DANGLING_TYPE_SPECIFIER) {
    /* A malformed declaration was detected by decl_specifiers.  Issue
       errors indicating that an identifier (= a declarator) is missing,
       along with a semicolon.  Then branch to the bottom of the loop. */
    set_err_pos_to_curr_token();
    if (!(dso_flags & DSO_DECLARES_SOMETHING)) error(ec_exp_identifier);
    error(ec_exp_semicolon);
    discard_curr_construct_pragmas();
    *skip_semicolon_check = TRUE;
    goto next_declaration;
  }  /* if */
  if (curr_token == tok_semicolon) {
    /* There's no declarator following the declaration specifier.  This may
       be okay, but sometimes a diagnostic should be issued. */
    check_missing_declarator_in_member_declaration(class_type, &decl_info);
    missing_declarator = TRUE;
    if (decl_info.is_anonymous_union) {
      /* decl_nonstatic_data_member needs to be called. */
      /* Ignore any top-level cv-qualifiers in Microsoft mode and in some
         GNU modes.  (In GNU modes prior to 3.4, the qualifiers are accepted
         and they apply to the implied field.) */
      if ((microsoft_mode || gnu_mode) &&
          decl_state->type->kind == (a_type_kind)tk_typeref &&
          !typeref_is_typedef(decl_state->type)) {
        if (gnu_mode && gnu_version < 30400) {
          pos_warning(ec_nonstandard_anonymous_union_qualifier,
                      &decl_state->start_pos);
        } else {
          decl_state->type = skip_typerefs(decl_state->type);
          decl_state->specifiers_type = decl_state->type;
          pos_warning(ec_anonymous_union_qualifier_ignored,
                      &decl_state->start_pos);
        }  /* if */
      }  /* if */
    } else {
      cannot_bind_to_curr_construct();
      /* Bypass the semicolon and skip to the next declaration. */
      if (!is_member_template) {
        (void)get_token();
        *skip_semicolon_check = TRUE;
      }  /* if */
      goto next_declaration;
    }  /* if */
  }  /* if */
  /* Save some state that must be restored for each declarator. */
  saved_qualifiers = decl_state->qualifiers;
  saved_qualifiers_pos = decl_state->qualifiers_pos;
  /* Save the effective specifiers type (which may be different from
     decl_state->specifiers_type; e.g., for constructors). */
  specifiers_type = decl_state->type;
  /* A declarator list should be present.  Scan it. */
  do {
    a_symbol_locator                  locator;
    a_func_info_block                 func_info;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    a_boolean                         preserve_param_id_list = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    a_template_symbol_supplement_ptr  tssp;
    a_source_position                 declarator_start_pos;
    a_boolean                         is_function = FALSE;
    declarator_start_pos = pos_curr_token;
    add_stop_token(tok_comma);
    if (!member_declarator(class_state, &decl_info, &locator, &func_info,
                           &is_function, &is_typedef)) {
      /* A syntax error occurred: Proceed with the next declaration. */
      expect_error();
      *skip_semicolon_check = TRUE;
      goto next_declaration;
    }
    if (!C_mode() && is_function) {
      /* Member or friend function. */
      a_boolean  function_def_present;
      check_if_function_defined_in_class(&func_info, &decl_info);
      /* At this point func_info.is_definition reflects the presence of a
         definition, but that may be set to FALSE later on if the definition
         should be ignored (this happens with certain friend declaration in
         some Microsoft modes). */
      function_def_present = func_info.is_definition;
      if (mutable_specified) {
        /* "mutable" is only allowed on nonstatic data member decls. */
        pos_error(ec_mutable_not_allowed, &decl_state->start_pos);
      }  /* if */
      if (!type_explicitly_specified) {
        /* No type specifier. */
        if (decl_info.is_constructor || decl_info.is_destructor ||
#if MICROSOFT_EXTENSIONS_ALLOWED
            decl_info.is_static_constructor || decl_info.is_finalizer ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            locator.is_conversion_name ||
            (locator.is_error && looks_like_ctor_or_dtor(&locator))) {
          /* Type specifier is not expected (nor permitted) on constructors,
             destructors, and conversion functions.  Similarly, they are not
             permitted on C++/CLI static constructors and finalizers. */
        } else {
          /* Type specifier is missing.  The type defaults to int, but issue
             a diagnostic. */
          report_missing_type_specifier(&declarator_start_pos,
                                        decl_state->type,
                                        /*is_function=*/TRUE,
                                        func_info.is_definition,
                                        /*is_main_function=*/FALSE,
                                        !no_decl_specifiers);
        }  /* if */
      }  /* if */
      /* Issue diagnostic on an incomplete-type in an exception
         specification.  (It wasn't done when the exception specification
         was scanned because definitions and declarations are treated
         differently.) */
      report_exception_spec_errors(&func_info);
      if (same_entities(decl_state->type, specifiers_type)) {
        /* When scanning the declarator does not change the type, we know
           this member is a function based on the specifier type alone.
           This is only possible with a typedef name that represents a
           function type. */
        func_info.function_type_from_typedef = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        func_info.declarator_ssep = decl_state->source_sequence_entry;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        /* Such typedef function types are shared and so are unsuited to be
           the type of a defined function. */
        check_typedef_function_type(&decl_state->type,
                                    &locator.source_position,
                                    func_info.is_definition, class_type,
                                    (!friend_specified &&
                                     decl_state->storage_class !=
                                          (a_storage_class)sc_static));
      }  /* if */
      if (dso_flags & DSO_VIRTUAL && !locator.is_error) {
        check_for_invalid_use_of_virtual(&locator, class_type, &decl_info);
      }  /* if */
      if (!friend_specified) {
        if ((decl_info.is_constructor ||
#if MICROSOFT_EXTENSIONS_ALLOWED
             decl_info.is_finalizer ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
             decl_info.is_destructor) &&
            decl_state->storage_class == (a_storage_class)sc_static) {
          /* Constructors, destructors, and finalizers may not be declared
             "static" (except in C++/CLI mode, but "static constructors" do
             not have the is_constructor flag set to TRUE).  C++/CLI
             finalizers cannot be static either. */
          pos_error(ec_static_not_allowed, &decl_state->start_pos);
          decl_state->storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
        if ((decl_info.is_constructor && !func_info.is_defaulted) ||
            (dso_flags & DSO_VIRTUAL)) {
          /* A class with a user-provided constructor or a virtual function
             cannot be an "aggregate" (8.5.1).  (A defaulted constructor is
             not considered "user-provided".) */
          class_state->class_aggregate_ruled_out = TRUE;
          class_state->POD_ruled_out = TRUE;
        } else if (decl_info.is_destructor && !func_info.is_defaulted) {
        /* A POD may not have a user-provided destructor, either. */
          class_state->POD_ruled_out = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (decl_info.is_static_constructor) {
          /* A user-defined static constructor precludes a class from being an
             aggregate (at least, that is how Microsoft compilers behave). */
          class_state->POD_ruled_out = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
      }  /* if */
      if (friend_specified) {
        /* Process a friend function declaration. */
        rout_sym = decl_friend_function(&locator, class_state, &func_info,
                                        &decl_info);
      } else if (is_member_template_rescan) {
        *member_template_instance_type = decl_state->type;
        remove_stop_token(tok_comma);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* Set the declared type immediately, before the func_info block is
           discarded. */
        instance->declared_type = func_info.declared_type;
        /* Also save the parameter-id list to later reconstruct the declared
           types of parameters for the associated parameter variables. */
        instance->param_id_list = func_info.param_id_list;
        /* Clear the func_info field to prevent deallocation: */
        func_info.param_id_list = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        goto next_declaration;
      } else if (is_member_template) {
        /* An "= 0" is not valid for a member template, but in some modes
           such a spurious pure specifier is ignored while parsing the
           template (but not when the template is instantiated). */
        if ((microsoft_mode || (gpp_mode && gnu_version < 40200)) &&
            curr_token == tok_assign && next_token() == tok_int_constant) {
          (void)get_token();
          (void)get_token();
        }  /* if */
        /* Process the member function template. */
        decl_member_function_template(&locator, templ_param_list, &func_info,
                                      class_state, &decl_info);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        remove_declarator_sse(decl_state, depth_scope_stack);
        if (!func_info.is_definition && !source_sequence_entries_disallowed) {
          /* Turn the source sequence entry for the a_template entry into a
             secondary source sequence entry. */
          a_src_seq_secondary_decl_ptr sssdp =
                            secondary_src_seq_for_template(il_template_entry);
          sssdp->declared_type = func_info.declared_type;
        } else if (func_info.is_definition) {
          template_supplement_for_symbol(decl_info.decl_state.sym)->
                              variant.function.routine->declared_type =
                                                      func_info.declared_type;
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        rout_sym = decl_info.decl_state.sym;
        if (dso_flags & DSO_EXPLICIT) {
          if (decl_info.is_constructor) {
            tssp = rout_sym->variant.template_info;
            tssp->variant.function.routine->is_explicit_constructor = TRUE;
          } else if (locator.is_conversion_name && cppcli_enabled) {
            tssp = rout_sym->variant.template_info;
            tssp->variant.function.routine
                ->is_explicit_conversion_function = TRUE;
          }  /* if */
        }  /* if */
        remove_stop_token(tok_comma);
        goto next_declaration;
      } else {
        /* Must be a member function declaration. */
        /* Create a symbol for the member function. */
        decl_member_function(&locator, &func_info, class_state, &decl_info,
                             /*compiler_generated=*/FALSE);
        rout_sym = decl_info.decl_state.sym;
        if (class_state->is_nonreal_instantiation &&
            !class_type->
                       variant.class_struct_union.is_in_class_specialization) {
          /* During the prototype instantiation, save the token sequence
             number associated with this declaration so that it can be used
             for matching purposes during real instantiations. */
          tssp = rout_sym->variant.routine.instance_ptr->template_info;
          check_assertion(tssp != NULL);
          if (tssp->token_sequence_number == NO_TOKEN_SEQUENCE_NUMBER) {
            /* Only set this if not already set (which could occur in error
               cases). */
            tssp->token_sequence_number = curr_token_sequence_number;
          }  /* if */
        } else if (class_state->corresp_prototype_tag_sym != NULL) {
          /* The class must be the instantiation of a class template (or a
             class nested within such an instantiation). Bind the current
             member function symbol to the function template symbol
             established during prototype instantiation. */
          a_symbol_ptr  prototype_sym = class_state->corresp_prototype_tag_sym;
          a_type_ptr    tp = prototype_sym->variant.class_struct_union.type;

          if (tp->kind == (a_type_kind)tk_union &&
              tp->variant.class_struct_union.
                    extra_info->anonymous_union_kind !=
                                   (an_anonymous_union_kind)auk_none) {
            /* A member function of an anonymous union is an error (to be
               issued later, in check_anonymous_union_symbols).
               find_member_function_template should not be called, since it
               can't handle this sort of thing. */
          } else if (!is_error_locator(locator)) {
            find_member_function_template(rout_sym, prototype_sym);
#if GENERATE_SOURCE_SEQUENCE_LISTS
            /* Set the declared_type in the instance entry.  Try to use
               the declared_type already entered in the func_info block.
               This is not just to save bytes -- the pointer in the func_info
               block is the same as the pointer in a secondary-decl source
               sequence entry; keep the same correspondence in the instance
               entry, since default arg fixup depends on it. */
            if (rout_sym->variant.routine.instance_ptr != NULL) {
              a_type_ptr  declared_type = func_info.declared_type;
              a_template_instance_ptr
                          tip = rout_sym->variant.routine.instance_ptr;

              tip->declared_type_for_default_arg_fixup = declared_type;
              if (declared_type == NULL) {
                declared_type = form_declared_type(decl_state->type,
                                                   &func_info);
              }  /* if */
              tip->declared_type = declared_type;
              /* Save the param_id_list so we can accurately represent the
                 actual declared type of the parameters later on. */
              tip->param_id_list = func_info.param_id_list;
              /* Do no let the param_id_list be deallocated later on: */
              preserve_param_id_list = TRUE;
            }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          }  /* if */
        }  /* if */
        if (dso_flags & DSO_EXPLICIT) {
          if (decl_info.is_constructor) {
            rout_sym->variant.routine.ptr->is_explicit_constructor = TRUE;
          } else if (locator.is_conversion_name && cppcli_enabled) {
            rout_sym->variant.routine.ptr
                    ->is_explicit_conversion_function = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (!func_info.is_definition ||
          (prototype_instantiations_in_il &&
           !nonclass_prototype_instantiations &&
           class_type
                   ->variant.class_struct_union.is_prototype_instantiation)) {
        /* Update xref info on param ids.  Note that if we are in a prototype
           instantiation and nonclass templates are not parsed in their generic
           form, the function should be considered undefined (since it won't
           be parsed until the parameters are substituted). */
        record_param_id_list_declarations(&func_info);
      }  /* if */
      if (curr_routine_fixup != NULL) {
        /* Update the symbol pointer in the fixup entry -- it's needed when
           the default args are scanned (once the entire class has been
           scanned). */
        curr_routine_fixup->symbol = rout_sym;
        curr_routine_fixup->func_info = func_info;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        curr_routine_fixup->preserve_param_id_list = preserve_param_id_list;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      } else {
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (preserve_param_id_list) { func_info.param_id_list = NULL; }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        done_with_func_info(func_info);
      }  /* if */
      if (curr_token == tok_assign) {
        /* Look for a pure specifier ("= 0"), which may appear on virtual
           functions, but don't attempt to scan past a C++0x "= default" or
           "= delete" construct. */
        a_token_kind  next_tok = next_token();
        if (next_tok != tok_delete && next_tok != tok_default) {
          scan_pure_specifier(rout_sym, class_type, &decl_info, &func_info);
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (func_info.abstract) {
        /* The function modifier "abstract" means the same thing as "= 0" (but
           it always requires an explicit "virtual" specifier, which was
           checked earlier). */ 
        make_virtual_function_pure(rout_sym->variant.routine.ptr, class_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
      if (function_def_present) {
        remove_stop_token(tok_comma);
        cache_in_class_function_definition(&func_info, &decl_info,
                                           class_state);
        /* A comma-list of function definitions is not allowed. */
        *skip_semicolon_check = TRUE;
        goto next_declaration;
      } else {
        /* Not a function definition. */
        if (!friend_specified) {
          if (rout_sym->variant.routine.ptr->is_virtual &&
              !rout_sym->variant.routine.ptr->pure_virtual) {
            /* Virtual member function. */
            if (class_state->is_local_class) {
              /* A member function declared in a local class definition
                 (which is the current case) must be defined within the class
                 definition if it is used -- virtual functions are assumed to
                 be used (e.g., because an address is needed for a vtbl).
                 (For a non-virtual function we issue the error when it is
                 referenced.) */
              sym_error(ec_local_class_function_def_missing, rout_sym);
            } else if (class_state->corresp_prototype_tag_sym == NULL) {
              /* An undefined virtual member function in an unnamed class (or
                 in a named class that is nested in an unnamed class) cannot
                 be defined later (there's no way to name it), so issue an
                 error.  Note that if the enclosing class is a real template
                 instantiation, the function body may actually exist but it
                 was replaced by a semicolon in extract_member_bodies.  In
                 those cases no error should be issued (but if indeed the
                 body is missing, an error will have been issued on the
                 prototype instantiation). */
              a_type_ptr  tp = class_type;
              for (;;) {
                if (tp->variant.class_struct_union.originally_unnamed) {
                  sym_error(ec_unnamed_class_virtual_function_def_missing,
                            rout_sym);
                  break;
                }  /* if */
                if (!tp->source_corresp.is_class_member) break;
                tp = parent_class_of(tp);
              }  /* for */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (is_member_template) {
      /* Invalid declaration of a member template. */
      pos_error(ec_bad_member_template_decl, &decl_state->start_pos);
      remove_stop_token(tok_comma);
      discard_curr_construct_pragmas();
      break;
    } else if (dso_flags & (DSO_FRIEND | DSO_VIRTUAL | DSO_INLINE)) {
      if (dso_flags & DSO_FRIEND) {
        pos_error(ec_bad_friend_decl, &decl_state->start_pos);
      }  /* if */            
      if (dso_flags & DSO_VIRTUAL) {
        pos_error(ec_virtual_not_allowed, &decl_state->start_pos);
      }  /* if */            
      if (dso_flags & DSO_INLINE) {
        pos_error(ec_inline_and_nonfunction, &decl_state->start_pos);
      }  /* if */            
      remove_stop_token(tok_comma);
      discard_curr_construct_pragmas();
      break;
    } else if (decl_info.is_destructor
#if MICROSOFT_EXTENSIONS_ALLOWED
               || decl_info.is_finalizer
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                        ) {
      /* Error has already been issued if it wasn't processed as a
         function. */
      discard_curr_construct_pragmas();
    } else if (is_typedef) {
      check_assertion(C_dialect == C_dialect_cplusplus);
      if (decl_state->do_flags & DO_CFRONT_MEMBER_FUNCTION_TYPEDEF) {
        /* This looked like a cfront-style member function typedef.  Be sure
           the type was a function type. */
        if (is_function_type(decl_state->type)) {
          /* Issue a warning on the extension. */
          pos_warning(ec_ptr_to_member_typedef, &locator.source_position);
        } else {
          /* No function type, so what looked like a qualified name really
             was -- but they aren't allowed. */
          pos_error(ec_qualified_name_not_allowed, &locator.source_position);
          set_to_error_locator(locator);
        }  /* if */
      }  /* if */
      if (!type_explicitly_specified) {
        /* Omitted type specifier. */
        report_missing_type_specifier(&declarator_start_pos,
                                      decl_state->type,
                                      /*is_function=*/FALSE,
                                      /*is_function_def=*/FALSE,
                                      /*is_main_function=*/FALSE,
                                      !no_decl_specifiers);
      }  /* if */
      /* Typedef declaration. */
      decl_typedef(&locator, decl_state, class_type, 
                   &decl_info.decl_pos_block);
      /* Note: access will have been set in decl_typedef. */
      if (curr_routine_fixup != NULL &&
          curr_routine_fixup->def_arg_expr_fixup_list != NULL) {
        /* Update the symbol pointer in the fixup entry -- it's needed when
           the default args are scanned (once the entire class has been
           scanned). */
        curr_routine_fixup->symbol = decl_info.decl_state.sym;
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode) {
        class_state->potentially_interface_like = FALSE;
        if (microsoft_bugs) {
          /* In Microsoft bugs mode, the typedef is processed before member
             function bodies etc. are rescanned.  This makes e.g. the following
             legal:
                typedef struct {
                  enum { e };
                  void f() { S::e; }
                } S;
          */
          process_deferred_class_fixups_and_instantiations(
                                                  /*for_instantiation=*/FALSE);
        }  /* if */
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (curr_token == tok_assign && !C_mode() &&
               ((is_scalar_type(decl_state->type) && !mutable_specified &&
                 (get_type_qualifiers(decl_state->type) == TQ_CONST)) ||
                is_template_param_type(decl_state->type)) &&
               decl_state->storage_class == (a_storage_class)sc_unspecified &&
               !decl_state->has_cli_literal_keyword) {
      /* Provide support for the nonstandard declaration of a member constant
         of scalar type -- e.g., "const int I = 2;". */
      if (in_expression_context()) {
        syntax_error(ec_nonstd_const_member_decl_not_allowed);
      } else {
        decl_nonstd_member_constant(&locator, class_state, &decl_info);
      }  /* if */
    } else {
      /* A static or nonstatic data member. */
      if (mutable_specified && is_const_qualified_type(decl_state->type)) {
        /* "mutable" and top-level "const" are not allowed together. */
        pos_error(ec_mutable_not_allowed, &decl_state->start_pos);
      }  /* if */
      if (!type_explicitly_specified) {
        report_missing_type_specifier(&declarator_start_pos,
                                      decl_state->type,
                                      /*is_function=*/FALSE,
                                      /*is_function_def=*/FALSE,
                                      /*is_main_function=*/FALSE,
                                      !no_decl_specifiers);
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (decl_state->has_cli_initonly_keyword) {
        if (cli_class_type_kind_is(class_type, cctk_standard)) {
          pos_error(ec_initonly_requires_managed_class,
                    &decl_state->declarator_pos);
        } else if (is_const_qualified_type(decl_state->type)) {
          /* "const" is useless on a C++/CLI initonly declaration. */
          a_source_position  *diag_pos = &decl_state->qualifiers_pos;
          if (!(decl_state->qualifiers & TQ_CONST)) {
            diag_pos = &decl_state->start_pos;
          }  /* if */
          pos_warning(ec_initonly_const_has_no_effect, diag_pos);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
      if (!(missing_declarator || decl_info.is_unnamed_field) &&
          !(decl_state->do_flags & DO_REAL_DECLARATOR_SCANNED) &&
          is_error_locator(locator)) {
        /* Some problem occurred while parsing the declarator.  To avoid
           strange error recovery problems, we do not add a member to the
           class type.  (If there was no declarator or if the declarator
           consisted solely of an unnamed bit field length, the locator is
           set to an error locator even though there could not possibly be
           a declarator-parsing error.) */
        check_assertion(total_errors != 0);
      } else if (decl_state->storage_class == (a_storage_class)sc_static) {
        /* Static data member. */
        decl_static_data_member(&locator, class_state, &decl_info);
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (decl_state->has_cli_literal_keyword) {
        /* C++/CLI literal field */
        decl_literal_field(&locator, class_state, &decl_info);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        /* Non-static data member (= field). */
        scan_nonstatic_data_member(&locator, class_state, &decl_info);
      }  /* if */
      if (C_dialect == C_dialect_cplusplus) {
        /* Issue an error if there appears to be an attempt to initialize a
           data member within the class definition. */
        if (curr_token == tok_assign) {
          set_err_pos_to_curr_token();
          /* Issue a syntax error to flush to the comma or semicolon. */
          syntax_error(ec_bad_data_member_initialization);
        }  /* if */
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (decl_state->ms_attributes != NULL) {
      dispose_of_unapplied_attributes(&decl_state->ms_attributes,
                                      ec_ms_attr_not_allowed);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (!decl_info.is_first_in_declarator_list) {
      mark_decl_after_first_in_comma_list(decl_state);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    remove_stop_token(tok_comma);
    decl_info.is_first_in_declarator_list = FALSE;
    if (curr_token == tok_comma) {
      /* Another declarator is presumably coming next. */
      check_use_of_auto_type(decl_state);
      /* Before parsing the next declaration, run any end-of-parse actions
         needed for the previous declarator. */
      run_end_of_parse_actions(decl_state);
      /* Reset certain decl_state fields. */
      start_secondary_declarator(decl_state);
      decl_state->qualifiers = saved_qualifiers;
      decl_state->qualifiers_pos = saved_qualifiers_pos;
    }  /* if */
    /* Loop for additional declarators. */
  } while (loop_token(tok_comma));
next_declaration:;
  if (dso_flags & DSO_EXPLICIT) {
    /* The keyword "explicit" is allowed only on a constructor declaration and
       on a C++/CLI conversion function declaration.  Microsoft compilers also
       allow it on free-standing class/enum declarations.  This check must
       occur after any declarator processing because we cannot know for sure
       whether the declaration was a constructor until then. */
    if (microsoft_mode && missing_declarator) {
      /* Microsoft compilers appear to ignore "explicit" in this case. */
    } else if (!(dso_flags & DSO_FRIEND) &&
               (decl_info.is_constructor ||
                (cppcli_enabled &&
                  is_conversion_function_symbol(decl_state->sym)))) {
      /* Okay. */
    } else {
      pos_error(ec_explicit_not_allowed, &decl_state->start_pos);
    }  /* if */
  }  /* if */
  check_use_of_auto_type(decl_state);
  run_end_of_parse_actions(decl_state);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    /* Restore the default name linkage if a linkage specification appeared
       among the decl-specifiers. */
    if (dso_flags & DSO_LINKAGE_SPEC_DECL) pop_name_linkage();
    if (decl_state->ms_attributes != NULL) {
      dispose_of_unapplied_attributes(&decl_state->ms_attributes,
                                      ec_ms_attr_not_allowed);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (decl_pos_block_ptr != NULL) {
    /* Return to the caller the extra source position information collected
       for this declaration. */
    *decl_pos_block_ptr = decl_info.decl_pos_block;
  }  /* if */
  db_exit();
  return decl_info.decl_state.sym;
}  /* class_member_declaration */


a_symbol_ptr class_member_template_declaration(
                                     a_type_ptr            class_type,
                                     a_template_param_ptr  templ_param_list,
                                     a_template_ptr        il_template_entry,
                                     a_decl_pos_block_ptr  decl_pos_block_ptr)
/*
Scan a template function declaration that appears inside a class (or class
template) definition.  class_type is the parent type, which may be a nonreal
class (prototype instantiation of a class template).  templ_param_list
is the template parameter list for the function template.
*/
{
  a_class_def_state  *class_state_ptr;
  a_boolean          skip_semicolon_check;
  a_scope_depth      scope_level;
  a_symbol_ptr       sym;
  a_type_ptr         dummy_type;

  db_enter(3, "class_member_template_declaration");
  /* Get the class definition state, which is pointed to from the scope-stack
     entry. */
  scope_level = class_type->variant.class_struct_union.extra_info->
                                       assoc_scope->depth_in_scope_stack;
  check_assertion(scope_level != NO_SCOPE_DEPTH);
  class_state_ptr = scope_stack[scope_level].class_def_state;
  sym = class_member_declaration(class_type, class_state_ptr,
                                 (an_ms_attribute_ptr)NULL,
                                 /*is_member_template=*/TRUE,
                                 templ_param_list, &skip_semicolon_check,
                                 &dummy_type, (a_template_instance_ptr)NULL,
                                 il_template_entry,
                                 decl_pos_block_ptr);
  if (curr_routine_fixup != NULL) dispose_of_curr_routine_fixup();
  if (sym == NULL) {
    /* An error has already been issued. */
  } else if (sym->is_error) {
    /* An error has already been issued -- return null. */
    sym = NULL;
  } else if (sym->kind != (a_symbol_kind)sk_function_template) {
    /* Issue the error and return NULL. */
    pos_sy_error(ec_bad_member_template_sym, &sym->decl_position, sym);
    sym = NULL;
  }  /* if */
  db_exit();
  return sym;
}  /* class_member_template_declaration */


a_type_ptr rescan_member_template_declaration(
                                          a_type_ptr               class_type,
                                          a_template_instance_ptr  instance)
/*
The current token is the start of a member template function declaration
which is being rescanned as part of its instantiation.  class_type is the
parent type.  A pointer to the member type (the result of calling
decl_specifiers and declarator) is returned.  instance is the template
instance record associated with this instantiation.
*/
{
  a_type_ptr           member_template_instance_type = NULL;
  a_class_def_state    class_state;
  a_boolean            skip_semicolon_check;
  a_routine_fixup_ptr  saved_routine_fixup;

  db_enter(3, "rescan_member_template_declaration");
  /* Initialize class_state to default values.  It should have no decisive
     effect on the limited processing that is to be done. */
  initialize_class_def_state(class_type, &class_state);
  saved_routine_fixup = curr_routine_fixup;
  curr_routine_fixup = NULL;
  (void)class_member_declaration(class_type, &class_state,
                                 (an_ms_attribute_ptr)NULL,
                                 /*is_member_template=*/FALSE,
                                 (a_template_param_ptr)NULL,
                                 &skip_semicolon_check,
                                 &member_template_instance_type, instance,
                                 (a_template_ptr)NULL,
                                 (a_decl_pos_block *)NULL);
  curr_routine_fixup = saved_routine_fixup;
  db_exit();
  return member_template_instance_type;
}  /* rescan_member_template_declaration */


a_boolean is_two_argument_delete(a_routine_ptr delete_routine)
/*
Return TRUE if the indicated delete routine is of the two-argument form.
*/
{
  a_boolean                     is_two_arg;
  a_routine_type_supplement_ptr delete_routine_rtsp =
                                        f_skip_typerefs(delete_routine->type)->
                                                    variant.routine.extra_info;
  a_param_type_ptr              param1 = delete_routine_rtsp->param_type_list;

  check_assertion(param1 != NULL);
  is_two_arg = (param1->next != NULL);
  return is_two_arg;
}  /* is_two_argument_delete */


static void check_operator_new_and_delete(a_symbol_ptr  tag_sym)
/*
Check that the new and delete operators are declared in consistent pairs
in the class designated by tag_sym.
*/
{
  a_class_symbol_supplement_ptr  cssp;
  a_type_ptr                     class_type;
  a_symbol_ptr                   new_sym, del_sym;
  a_boolean                      array_pass, ambiguous;
  an_opname_kind                 new_kind;
  an_opname_kind                 del_kind;

  cssp = tag_sym->variant.class_struct_union.extra_info;
  class_type = tag_sym->variant.class_struct_union.type;
  /* Do the checking once for the non-array new and delete declarations and
     then for the array new and delete declarations.  This is done as loop
     that iterates twice, first with array_pass set to FALSE and then with
     array_pass set to TRUE. */
  if (cssp->has_operator_new || cssp->has_operator_delete) {
    /* There are non-array operator new and/or delete declarations in the
       current class. */
    array_pass = FALSE;
    new_kind = (an_opname_kind)onk_new;
    del_kind = (an_opname_kind)onk_delete;
  } else {
    /* Skip the first set of checks and move straight to the array new and
       delete checking. */
    array_pass = TRUE;
  }  /* if */
  for (;;) {
    if (array_pass) {
      if (cssp->has_operator_array_new || cssp->has_operator_array_delete) {
        /* There are array operator new and/or delete declarations. */
        new_kind = (an_opname_kind)onk_array_new;
        del_kind = (an_opname_kind)onk_array_delete;
      } else {
        /* Skip the second set of checks. */
        break;
      }  /* if */
    }  /* if */
    /* Get a pointer to the new and delete symbols (which may be overload
       symbols). */
    new_sym = opname_member_function_symbol(new_kind, class_type);
    if (new_sym != NULL) {
      if (new_sym->kind == (a_symbol_kind)sk_projection &&
          !new_sym->variant.projection.is_using_decl) {
        /* Ignore operator new if it is simply inherited. */
        new_sym = NULL;
      }  /* if */
    }  /* if */
    del_sym = opname_member_function_symbol(del_kind, class_type);
    ambiguous = FALSE;
    if (del_sym != NULL) {
      a_symbol_ptr default_del_sym;
      /* Pick the default operator delete (if any) out of the overload set. */
      default_del_sym = find_default_operator_delete_sym(del_sym, &ambiguous);
      if (array_pass && !ambiguous && default_del_sym != NULL) {
        /* Note whether the class operator delete[] is of the two-argument
           form. */
        a_routine_ptr delete_routine;
        a_symbol_ptr  fund_operator_delete =
                                        fundamental_symbol_of(default_del_sym);
        check_assertion(fund_operator_delete->kind ==
                                            (a_symbol_kind)sk_member_function);
        delete_routine = fund_operator_delete->variant.routine.ptr;
        if (is_two_argument_delete(delete_routine)) {
          cssp->has_two_argument_operator_array_delete = TRUE;
        }  /* if */
      }  /* if */
      if (del_sym->kind == (a_symbol_kind)sk_projection &&
          !del_sym->variant.projection.is_using_decl) {
        /* Ignore operator delete if it is simply inherited. */
        del_sym = NULL;
        ambiguous = FALSE;
      } else {
        del_sym = default_del_sym;
      }  /* if */
    }  /* if */
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
    if (exceptions_enabled) {
      /* When exceptions are enabled, be sure each placement operator new
         has a corresponding operator delete. */
      a_boolean  is_overloaded;

      if (new_sym != NULL) {
        a_symbol_ptr  sym = new_sym, ovl_sym, fund_sym;
        if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
          is_overloaded = TRUE;
          sym = sym->variant.overloaded_function.symbols;
        } else {
          is_overloaded = FALSE;
        }  /* if */
        /* Loop through the entire overload set of operator new symbols. */
        for (; sym != NULL; sym = is_overloaded ? sym->next : NULL) {
          fund_sym = fundamental_symbol_of(sym);
          /* Ignore function templates. */
          if (fund_sym->kind == (a_symbol_kind)sk_function_template) continue;
          del_sym = find_corresponding_operator_delete_sym(
                                                     fund_sym, class_type,
                                                     /*template_okay=*/TRUE,
                                                     &ambiguous, &ovl_sym);
          if ((del_sym == NULL || !del_sym->is_class_member) && !ambiguous) {
            /* There is no member operator delete that "corresponds" to this
               operator new (i.e., whose parameter types after the first
               match).  It is possible that a non-member operator delete
               would match at the point of call, but relying on such is
               widely considered to be poor coding practice. */
            pos_stsy_remark(ec_no_corresponding_member_delete,
                            &sym->decl_position,
                            (char *)(array_pass ? "[]" : ""), sym);
          }  /* if */
        }  /* for */
      }  /* if */
    } else {
      /* Exceptions are not enabled. */
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
      /* Just issue a remark if the class has an operator new() but no
         default operator delete() or vice versa. */
      /* If del_sym is non-NULL it now points to a default operator delete. */
      if (new_sym != NULL) {
        if (del_sym == NULL && !ambiguous) {
          /* No default operator delete. */
          /* coverity[dead_error_line] */ /* Coverity bug */
          pos_stsy_remark(ec_class_with_op_new_but_no_op_delete,
                          &error_position, (char *)(array_pass ? "[]" : ""),
                          tag_sym);
        }  /* if */
      } else {
        /* No operator new was declared.  If a default operator delete was
           declared, issue a diagnostic. */
        if (del_sym != NULL) {
          /* coverity[dead_error_line] */ /* Coverity bug */
          pos_stsy_remark(ec_class_with_op_delete_but_no_op_new,
                          &error_position, (char *)(array_pass ? "[]" : ""),
                          tag_sym);
        }  /* if */
      }  /* if */
#if ABI_CHANGES_FOR_PLACEMENT_DELETE
    }  /* if */
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
    if (array_pass) break;
    array_pass = TRUE;
  }  /* for */
}  /* check_operator_new_and_delete */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void check_if_potentially_interface_like(a_class_def_state  *state)
/*
state describes a class type in the process of being defined, and its base
classes (if any) have been scanned.  Set state->potentially_interface_like
based on the information recorded so far.  Additional elements of the
definition may cause the flag to be set to FALSE later on.  (The flag
remains set for a class type with properties similar to that of a Microsoft
__interface type.  This is significant because __interface types may derive
from such interface-like types.)
*/
{
  a_type_ptr  type = state->class_type;

  if (!type->variant.class_struct_union.is_interface) {
    char  *uuid_str = class_type_supp(type)->uuid_string;
    if (type->kind == (a_type_kind)tk_struct &&
        uuid_str != NULL &&
        type->source_corresp.name != NULL &&
        type->source_corresp.name[0] == 'I' &&
        ((strcmp(type->source_corresp.name, "IUnknown") == 0 &&
          strcmp(uuid_str, "00000000-0000-0000-c000-000000000046") == 0) ||
         (strcmp(type->source_corresp.name, "IDispatch") == 0 &&
          strcmp(uuid_str, "00020400-0000-0000-c000-000000000046") == 0))) {
      state->potentially_interface_like = TRUE;
    } else {
      /* Check that all the base classes are nonvirtual and public, with
         interface or interface-like class types.  There must be at least one
         interface-like base class. */
      a_base_class_ptr  bcp = base_classes_of(type);
      a_boolean         interface_like = TRUE, has_interface_like_base = FALSE;
      for (; bcp != NULL; bcp = bcp->next) {
        if (!bcp->direct) continue;
        if (bcp->is_virtual ||
            bcp->derivation->access == (an_access_specifier)as_private ||
            bcp->derivation->access == (an_access_specifier)as_protected) {
          interface_like = FALSE;
          break;
        } else if (bcp->type->variant.class_struct_union.is_interface_like) {
          has_interface_like_base = TRUE;
        } else if (!bcp->type->variant.class_struct_union.is_interface) {
          interface_like = FALSE;
          break;
        }  /* if */
      }  /* for */
      if (has_interface_like_base && interface_like) {
        state->potentially_interface_like = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* check_if_potentially_interface_like */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_using_decl_ptr find_unhiding_using_decl(a_type_ptr    dtype,
                                                 a_symbol_ptr  bsym)
/*
If the given derived type (dtype) contains a using-declaration that refers to
the name of the given base class member (bsym), return an a_using_decl entry
associated with that using declaration.  Otherwise, return NULL.
*/
{
  a_using_decl_ptr     udp = class_type_supp(dtype)->assoc_scope->using_decls;
  a_symbol_header_ptr  hdr = bsym->header;

  check_assertion(bsym->is_class_member);
  for (; udp != NULL; udp = udp->next) {
    a_source_correspondence  *scp;
    check_assertion(udp->is_class_member);
    scp = source_corresp_for_il_entry(udp->entity.ptr,
                                      (an_il_entry_kind)udp->entity.kind);
    if (((a_symbol_ptr)scp->assoc_info)->header == hdr) break;
  }  /* for */
  return udp;
}  /* find_unhiding_using_decl */


static a_boolean sym_hides_base_member(a_symbol_ptr  sym,
                                       a_symbol_ptr  *p_bsym)
/*
Return TRUE if the given class member symbol (sym) hides a base class member,
and if so return the symbol for a hidden member in *p_bsym.  Otherwise,
return TRUE and set *p_bsym to NULL.
*/
{
  a_boolean         result;
  a_symbol_locator  loc;
  a_symbol_ptr      bsym;

  make_locator_for_symbol(sym, &loc);
  clear_specific_symbol(loc);
  loc.parent.class_type = NULL;
  loc.is_class_member = FALSE;
  bsym = normal_id_lookup(&loc, IDL_SKIP_CURR_SCOPE | IDL_HIDDEN_NAME_LOOKUP);
  if (bsym == NULL || !bsym->is_class_member ||
      find_base_class_of(sym_parent_class(sym),
                         sym_parent_class(bsym)) == NULL) {
    /* The declaration does not hide a base member. */
    result = FALSE;
    *p_bsym = NULL;
  } else {
    result = TRUE;
    *p_bsym = bsym;
  }  /* if */
  return result;
}  /* sym_hides_base_member */


static void check_base_member_hiding(a_class_def_state  *class_state)
/*
class_state describes a class definition that was just completed.  Diagnose
unintentional hiding of base class members as appropriate.  This may include
issuing errors if the class was defined with the C++0x attribute "base_check". 
Also diagnose cases where a member declared with the "hiding" attribute does
not actually hide a base class member.
*/
{
  a_type_ptr    dtype = class_state->class_type;
  a_class_symbol_supplement_ptr
                cssp = symbol_supplement_for_class(dtype);
  a_symbol_ptr  msym = cssp->symbols;

  /* Don't check for hiding if a base is dependent.  Otherwise, only check
     if the class is a "base_check" class or if the "hiding" attribute
     appeared on a member declaration. */
  if ((cssp->base_check || cssp->check_hiding_attr) &&
      !has_dependent_base_class(dtype)) {
    for (; msym != NULL; msym = msym->next_in_scope) {
      a_boolean     ovl = symbol_is(msym, sk_overloaded_function);
      a_symbol_ptr  sym = ovl ? msym->variant.overloaded_function.symbols
                              : msym;
      for (; sym != NULL; sym = ovl ? sym->next : NULL) {
        a_source_correspondence  *scp = source_corresp_entry_for_symbol(sym);
        an_attribute_ptr         ap;
        a_symbol_ptr             bsym;
        if (!sym->is_class_member) {
          /* This can happen in some error cases. */
          expect_error();
        } else if (symbol_is(sym, sk_member_function) &&
                   sym->variant.routine.ptr->compiler_generated) {
          /* Don't check compiler-generated member functions. */
        } else if (symbol_is(sym, sk_type) &&
                   sym->variant.type.is_injected_class_name) {
          /* Don't check the injected class name. */
        } else if (symbol_is(sym, sk_projection)) {
          /* Don't check using-declarations or implicitly generated
             projections. */
        } else if (scp != NULL &&
                   (ap = find_attribute(ak_hiding, scp->attributes)) != NULL) {
          /* The declaration was marked as [[hiding]]: Check that it does
             indeed hide base class member. */
          if (!sym_hides_base_member(sym, &bsym)) {
            /* The declaration does not hide a base member. */
            pos_error(ec_hiding_attr_on_nonhiding_member, &ap->position);
          } else {
            /* Check if the hidden base class member is "unhidden" by a
               using-declaration in the derived class. */
            a_using_decl_ptr  udp = find_unhiding_using_decl(dtype, bsym);
            if (udp != NULL) {
              pos2_diagnostic(es_error, ec_hiding_attr_on_unhidden_member,
                              &ap->position, &udp->position);
            }  /* if */
          }  /* if */
        } else if (cssp->base_check) {
          /* The declaration was not marked as [[hiding]] but the enclosing
             class has the "base_check" attribute: Check that [[hiding]] is
             not required. */
          if (symbol_is(sym, sk_member_function) &&
              sym->variant.routine.ptr->overrides_base_member) {
            /* Don't check for hiding if a member function overrides a base
               class (technically, an overriding virtual function hides the
               members it overrides, but that is normal). */
          } else if (scp != NULL &&
                     find_attribute(ak_override, scp->attributes) != NULL) {
            /* If the declaration is marked with the "override" attribute but
               does not actually override a base class member, an error will
               already have been issued, and adding another one reporting
               hiding is not likely to be helpful. */
            expect_error();
          } else if (sym_hides_base_member(sym, &bsym)) {
            /* Check if the hidden base class member is "unhidden" by a
               using-declaration in the derived class. */
            a_using_decl_ptr  udp = find_unhiding_using_decl(dtype, bsym);
            if (udp == NULL) {
              if (symbol_is(bsym, sk_overloaded_function)) {
                bsym = bsym->variant.overloaded_function.symbols;
              }  /* if */
              pos_sy_error(ec_hiding_attr_required, &sym->decl_position, bsym);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* if */
  if (class_state->override_registry != NULL) {
    /* Check for incomplete overriding of virtual functions, and issue
       diagnostics where appropriate. */
    check_override_registry(class_state);
    /* All entries on the list have been freed, so clear the pointer. */
    class_state->override_registry = NULL;
  }  /* if */
}  /* check_base_member_hiding */

#if MICROSOFT_EXTENSIONS_ALLOWED

static a_boolean check_conflict_with_direct_property_or_event(
                                              a_symbol_locator  *ploc,
                                              a_type_ptr        class_type,
                                              a_symbol_ptr      diag_sym,
                                              a_boolean         property_case)
/*
diag_sym represents a member declared in class_type or a type derived from
class_type.  If property_case is TRUE, its name is of the form set_X or get_X;
otherwise, it's of the form add_X, remove_X, or raise_X.  ploc is a locator
for X.  Issue a diagnostic if class_type contains a conflicting direct (i.e.,
not inherited) property or event named X.
*/
{
  a_boolean     result = FALSE;
  a_symbol_ptr  sym;

  sym = class_qualified_id_lookup(ploc, class_type,
                                  IDL_DIRECT_CLASS_MEMBERS_ONLY);
  if (sym != NULL) {
    a_property_or_event_descr_ptr  pdp = NULL;
    a_boolean                      true_conflict;
    if (symbol_is(sym, sk_property_set)) {
      /* A potentially overloaded property: The check can be performed against
         any member of the set. */
      sym = sym->variant.property_info->properties;
    }  /* if */
    if (symbol_is(sym, sk_field)) {
      pdp = sym->variant.field.ptr->property_or_event_descr;
    } else if (symbol_is(sym, sk_static_data_member)) {
      pdp = sym->variant.static_data_member.variable->property_or_event_descr;
    }  /* if */
    if (pdp != NULL) {
      switch (pdp->kind) {
        case pek_declspec_property:
          /* declspec properties don't have associated reserved names. */
          true_conflict = FALSE;
          break;
        case pek_cli_property:
          true_conflict = property_case;
          break;
        case pek_cli_event:
          true_conflict = !property_case;
          break;
        default:
          unexpected_condition();
      }  /* switch */
      if (true_conflict) {
        pos_stsy_error(ec_member_name_reserved_by_property,
                       &diag_sym->decl_position, diag_sym->header->identifier,
                       sym);
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* check_conflict_with_direct_property_or_event */

                                    
static void check_names_reserved_by_cli_properties_and_events(
                                                       a_type_ptr  class_type)
/*
Check every direct member of class_type to see if it is of the form get_XYZ,
set_XYZ, add_XYZ, remove_XYZ, or raise_XYZ.  If it is, issue an error if
class_type also contains a (possibly inherited) conflicting property or event
named XYZ.
*/
{
  a_symbol_ptr  sym = symbol_supplement_for_class(class_type)->symbols;

  for (; sym != NULL; sym = sym->next_in_scope) {
    char       *mem_id = sym->header->identifier, *pname = NULL;
    a_boolean  property_case;
    if (symbol_is(sym, sk_type) && sym->variant.type.is_injected_class_name) {
      /* The injected class name is not considered. */
      continue;
    } else if (sym->decl_position.seq == 0) {
      /* Ignore compiler-generated declarations. */
      continue;
    }  /* if */
    if ((mem_id[0] == 'g' || mem_id[0] == 's') &&
        mem_id[1] == 'e' && mem_id[2] == 't' && mem_id[3] == '_' &&
        mem_id[4] != '\0') {
      /* "get_..." or "set_...". */
      pname = mem_id+4;
      property_case = TRUE;
    } else if (mem_id[0] == 'a' && mem_id[1] == 'd' && mem_id[2] == 'd' &&
               mem_id[3] == '_' && mem_id[4] != '\0') {
      /* "add_...". */
      pname = mem_id+4;
      property_case = FALSE;
    } else if (mem_id[0] == 'r' && mem_id[1] == 'e' && mem_id[2] == 'm' &&
               mem_id[3] == 'o' && mem_id[4] == 'v' && mem_id[5] == 'e' &&
               mem_id[6] == '_' && mem_id[7] != '\0') {
      /* "remove_...". */
      pname = mem_id+7;
      property_case = FALSE;
    } else if (mem_id[0] == 'r' && mem_id[1] == 'a' && mem_id[2] == 'i' &&
               mem_id[3] == 's' && mem_id[4] == 'e' && mem_id[5] == '_' &&
               mem_id[6] != '\0') {
      /* "raise_...". */
      pname = mem_id+6;
      property_case = FALSE;
    }  /* if */
    if (pname != NULL) {
      a_symbol_locator  ploc;
      a_base_class_ptr  bcp;
      clear_locator(&ploc, &null_source_position);
      (void)find_symbol(pname, strlen(pname), &ploc);
      if (class_type_supp(class_type)->has_direct_property_or_event &&
          check_conflict_with_direct_property_or_event(&ploc, class_type, sym,
                                                       property_case)) {
        /* A diagnostic has been issued: Additional ones for this symbol
           would not be helpful. */
        goto next_derived_class_symbol;
      }  /* if */
      for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
        if (class_type_supp(bcp->type)->has_direct_property_or_event &&
            check_conflict_with_direct_property_or_event(&ploc, bcp->type, sym,
                                                         property_case)) {
          /* A diagnostic has been issued: Additional ones for this symbol
             would not be helpful. */
          goto next_derived_class_symbol;
        }  /* if */
      }  /* for */
    }  /* if */
next_derived_class_symbol:;
  }  /* for */
}  /* check_names_reserved_by_cli_properties_and_events */


static void check_for_subscript_mechanism_conflict(a_type_ptr  class_type)
/*
The given class type is a C++/CLI managed class type whose complete definition
has just been parsed.  Issue an error if it contains both a member operator[]
and a default-indexed property.
*/
{
  a_symbol_locator  loc;
  a_symbol_ptr      sym;

  check_assertion(is_immediate_managed_class_type(class_type));
  /* Check if this class contains an operator[]. */
  make_opname_locator((an_opname_kind)onk_subscript, &loc,
                      &null_source_position);
  /* Members of interface bases are not considered because they aren't really
     inherited members for the purpose of this test. */
  sym = class_qualified_id_lookup(&loc, class_type,
                                  IDL_EXCLUDE_BASE_INTERFACE_MEMBERS |
                                  IDL_DO_NOT_CREATE_PROJ_SYM);
  if (sym != NULL) {
    /* The class contains an operator[]: A conflict is possible. */
    a_symbol_ptr  default_indexed_properties =
                                    symbol_supplement_for_class(class_type)
                                                 ->default_indexed_properties;
    if (default_indexed_properties == NULL &&
        same_entities(sym_parent_class(sym), class_type)) {
      /* Look for a conflict with a default-indexed property in a base class.
         (If operator[] were in a base class, this is not needed because a
         diagnostic would already have been issued for a conflict in a
         base.) */
      a_base_class_ptr  bcp = base_classes_of(class_type);
      for (; bcp != NULL; bcp = bcp->next) {
        if (!cli_class_type_kind_is(bcp->type, cctk_interface)) {
          default_indexed_properties = symbol_supplement_for_class(bcp->type)
                                                 ->default_indexed_properties;
          if (default_indexed_properties != NULL) break;
        }  /* if */
      }  /* for */
    }  /* if */
    if (default_indexed_properties != NULL) {
      /* A conflict: Issue an error. */
      a_source_position_ptr  decl_pos = &default_indexed_properties
                                                  ->variant.property_info
                                                  ->properties->decl_position;
      sym = fundamental_symbol_of(sym);
      pos_sy_error(ec_subscript_mechanism_conflict, decl_pos, sym);
    }  /* for */
  }  /* if */
}  /* check_for_subscript_mechanism_conflict */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void wrapup_standard_layout_flag(a_type_ptr  class_type)
/*
Determine the final value of the "standard_layout" flag in the symbol
supplement for the given class type.  This routine can only change the flag
from TRUE to FALSE.
*/
{
  a_symbol_ptr      class_sym = symbol_for(class_type);
  a_class_symbol_supplement_ptr
                    cssp = class_sym->variant.class_struct_union.extra_info;

  if (class_type->variant.class_struct_union
                           .any_virtual_functions_including_in_base_classes) {
    cssp->standard_layout = FALSE;
  }  /* if */
  if (cssp->standard_layout &&
      !class_type->variant.class_struct_union.is_prototype_instantiation) {
    a_base_class_ptr  bcp = base_classes_of(class_type), bcp_with_data = NULL;
    a_field_ptr       first_field = 
                            class_type->variant.class_struct_union.field_list;
    for (; bcp != NULL; bcp = bcp->next) {
      a_class_symbol_supplement_ptr  bcssp = symbol_for(bcp->type)
                                      ->variant.class_struct_union.extra_info;
      if (bcssp->any_nonstatic_data_members) {
        if (first_field != NULL || bcp_with_data != NULL) {
          /* If the derivation includes nonstatic data members, a base class
             cannot.  Otherwise, at most one base class can do so. */
          cssp->standard_layout = FALSE;
          break;
        } else {
          bcp_with_data = bcp;
        }  /* if */
      }  /* if */
      if (first_field != NULL) {
        /* The first field of a standard layout type cannot have the same type
           as a base class. */
        a_type_ptr  etype = skip_array_types(first_field->type);
        if (identical_types(bcp->type, etype)) {
          cssp->standard_layout = FALSE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
    if (first_field != NULL) {
      /* All fields of a standard layout type must be declared with the same
         access. */
      an_access_specifier  access = first_field->source_corresp.access;
      a_field_ptr          fp = first_field->next;
      for (; fp != NULL; fp = fp->next) {
        if (fp->source_corresp.access != access) {
          cssp->standard_layout = FALSE;
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* wrapup_standard_layout_flag */


static void complete_class_definition(a_type_ptr         class_type,
                                      a_scope_depth      effective_decl_level,
                                      a_class_def_state  *class_state)
/*
We have seen the complete definition of class_type belonging to scope level
effective_decl_level.  Perform various postprocessing steps such as computing
the layout and synthesizing special members (C++).  *class_state holds some
bits of information that were acquired while parsing.
*/
{
  a_symbol_ptr  tag_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  a_class_symbol_supplement_ptr  cssp
                        = tag_sym->variant.class_struct_union.extra_info;
  a_source_position saved_error_position;

  if (class_state->last_field_is_incomplete_array) {
    /* The last field that was recorded was an incomplete array.  This is
       permitted in C mode (as an extension), in C99 mode, and in GNU (most
       versions) and Microsoft mode (both C and C++).  If this is an early
       GNU mode or a strict-ANSI-C89 mode, issue a diagnostic.  Otherwise,
       mark class_type as containing an incomplete array member, since there
       are constraints on how it can be used.  (E.g., it can't be the element
       type of an array, and in Microsoft and GNU C++ modes it can't be used
       as a base class.) */
    check_assertion((C_mode() || microsoft_mode || gpp_mode) &&
                    !is_union_type(class_state->class_type));
    class_type->variant.class_struct_union.
                                    contains_flexible_array_member = TRUE;
    if ((strict_ansi_mode || (gnu_mode && gnu_version < 30000)) && !c99_mode) {
      a_field_ptr  fp = class_state->end_of_field_list;
      pos_diagnostic(strict_ansi_error_severity,
                     ec_incomplete_type_not_allowed,
                     &fp->source_corresp.decl_position);
      if (strict_ansi_error_severity == es_error) {
        fp->type = error_type();
        class_type->variant.class_struct_union.
                                    contains_flexible_array_member = FALSE;
      }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    } else if (gnu_mode && !c99_mode) {
      a_field_ptr  fp = class_state->end_of_field_list;
      report_gnu_extension_if_needed(&fp->source_corresp.decl_position,
                                     ec_flexible_array_is_nonstandard);
#endif /* GNU_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */    
  if (!class_state->is_nonreal_instantiation) {
    if (may_be_added_to_types_list(class_type, effective_decl_level)) {
      /* The type will already have been added to the current scope's types
         list.  However, it should be moved to the end of the list (unless
         it's already there), since its location in the types list should
         record where it was defined, not where it was initially declared. */
      move_to_end_of_types_list(class_type, effective_decl_level);
#if DEBUG
    } else {
      if (db_flag_is_set("dump_type_lists")) {
        fprintf(f_debug, "Not moving to end of type list: ");
        db_abbreviated_type(class_type);
        fprintf(f_debug, "\n");
      }  /* if */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
  /* Save a pointer to the list of member symbols in the tag symbol.  Note
     that there may be symbols even if there there were no declarations,
     since symbols may be inherited. */
  cssp->symbols =
          assoc_pointers_block_of(&scope_stack[depth_scope_stack])->symbols;
  /* A number of the checks done as a part the "wrapup" phase of scanning a
     class definition produce diagnostics.  Set error_position to assure
     that these diagnostics will be associated with tag_sym instead of
     with the current token, which is the closing brace. */
  saved_error_position = error_position;
  error_position = tag_sym->decl_position;
  if (C_dialect == C_dialect_cplusplus) {
    /* Reset the access to "public" for compiler-generated functions, if
       any. */
    class_state->access = (an_access_specifier)as_public;
    if (!class_state->class_aggregate_ruled_out) {
      /* Classes with no constructors, no private or protected nonstatic
         data members, no base classes, and no virtual functions are used to
         declare "aggregate" objects (WP 8.5.1). */
      cssp->is_class_aggregate = TRUE;
    }  /* if */
    /* Issue a diagnostic on a class with no user-defined constructor and
       with one or more nonstatic data members with reference or const type.
       This check must be done before compiler-generated constructors, if
       any, are entered.  (No diagnostic is issued on a const member that
       has a default constructor, since it will be initialized properly
       when the default constructor for the current class is generated. */
    if (class_state->any_const_or_ref_fields && cssp->constructor == NULL) {
      /* The current class has no user-defined constructor and at least
         one const or ref nonstatic data member.  A diagnostic may be
         required. */
      report_missing_constructor(tag_sym);
    }  /* if */
    /* Check to see if a remark should be issued on direct base classes
       with nonvirtual destructors. */
    check_base_class_destructors(class_state);
    /* Create compiler-generated default constructor, copy constructor,
       destructor, and assignment operator, if any is needed. */
    check_special_member_functions(class_type, class_state);
    if (cssp->is_class_aggregate && !class_state->POD_ruled_out) {
      /* It was intentional to wait until check_special_member_functions
         was called to set the is_POD flag -- the check for copy
         assignment operator was needed first. */
      cssp->is_POD = TRUE;
    }  /* if */
    /* Set shares_virtual_function_info for a base class of class_type, if
       appropriate. */
    set_shares_virtual_function_info_flag(class_type,
                                          (a_base_class_ptr)NULL);
  }  /* if */
  /* Do subobject allocation and compute the size and alignment of the
     class. */
  do_class_layout(class_type);
  if (C_dialect == C_dialect_cplusplus) {
    /* Go through all the functions declared for this class and set the
       virtual function numbers of virtual functions (some may already have
       a number assigned). */
    set_virtual_function_numbers(class_state);
    if (!class_state->is_nonreal_instantiation) {
      /* Check for inherited conversion functions.  This must be done before
         rescanning inline function definitions. */
      project_base_class_conversion_functions(class_type);
    }  /* if */
    /* Report errors in virtual function declarations that result from
       the failure to redeclare a virtual function originally declared in
       a virtual base class. */
    if (class_type->variant.class_struct_union.any_virtual_base_classes) {
      report_virtual_function_ambiguities(class_type);
    }  /* if */
    /* If the current class is not already marked as "abstract", run
       through its base classes to determine whether it is abstract by
       inheritance and set the flag accordingly. */
    check_abstract_class(class_type);
    /* Issue warnings/remarks if the class has an operator new but no
       operator delete, etc. */
    check_operator_new_and_delete(tag_sym);
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* Record whether this class is "interface-like" (i.e., a non-__interface
       type that is a valid base for an __interface type). */
    class_type->variant.class_struct_union.is_interface_like =
                                      class_state->potentially_interface_like;
    if (cppcli_enabled && is_immediate_managed_class_type(class_type)) {
      check_names_reserved_by_cli_properties_and_events(class_type);
      check_for_subscript_mechanism_conflict(class_type);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Check for missing or erroneous uses of the "hiding" attribute and
       for incomplete overriding of virtual functions. */
    check_base_member_hiding(class_state);
    /* Add final checks for the "standard_layout" flag. */
    wrapup_standard_layout_flag(class_type);
  }  /* if */
  error_position = saved_error_position;
}  /* complete_class_definition */


void check_for_file_with_unterminated_type_definition(
                                                  a_source_position  *end_pos)
/*
end_pos is the position of the last token of a class type or enum type
definition (usually the position of the closing brace, although GNU attributes
can make it the position of a parenthesis closing the attribute).  If the
current token (which must be the subsequent token; usually a declarator or a
semicolon) is in a different file, issue a warning for what is likely going to
be a syntax error showing up in the next file.  I.e., something like:

	struct S {}  // Warn on the suspect end-of-file condition here.
	# 1 "defs.c"
	S* f();  // Missing semicolon error issued here.
*/
{
  if (end_pos->seq != pos_curr_token.seq &&
      depth_innermost_instantiation_scope == NO_SCOPE_DEPTH) {
    /* The last token of the type definition and the token after that are on
       different lines.  (This can also happen with template instantiations;
       hence the condition on depth_innermost_instantiation_scope.)  Now check
       whether these two position correspond to different files (taking into
       account any #line directives). */
    a_line_number      line1, line2;
    a_boolean          eos1, eos2;
    a_source_file_ptr  src1, src2;
    src1 = source_file_for_seq(end_pos->seq, &line1, &eos1,
                               /*physical_line=*/FALSE);
    src2 = source_file_for_seq(pos_curr_token.seq, &line2, &eos2,
                               /*physical_line=*/FALSE);
    /* We cannot just compare src1 and src2 for equality because #line
       directives create new a_source_file entries.  E.g.:
             struct S {}
             #line 100
             x;  // Should not trigger a diagnostic, and yet src1 != src2.
       Instead, we perform a file name comparison. */
    if (src1 != src2 && src1 != NULL && src2 != NULL &&
        src1->file_name != NULL && src2->file_name != NULL &&
        compare_file_names(src1->file_name, src2->file_name) != 0) {
      /* Issue a warning in the file containing the definition to clarify the
         error that is likely to follow. */
      pos_warning(ec_file_ends_with_unterminated_type_definition, end_pos);
    }  /* if */
  }  /* if */
}  /* check_for_file_with_unterminated_type_definition */


#if !EXTRA_SOURCE_POSITIONS_IN_IL || !GENERATE_SOURCE_SEQUENCE_LISTS
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL.
                il_template_entry is not used unless source sequence entries
                are being generated. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL || !GENERATE_SOURCE_SEQUENCE_LISTS */
a_boolean scan_class_definition(a_type_ptr       class_type,
                                a_scope_depth    effective_decl_level,
                                a_boolean        is_local_class,
                                a_boolean        delayed_nested_class_def,
                                a_boolean        is_template_instantiation,
                                a_boolean        is_template_specialization,
                                a_template_ptr   il_template_entry,
                                a_decl_pos_block *decl_pos_block)
/*
Scan the body of a class definition, including the base classes list.
class_type points to the type entry of the class, struct, or union whose
definition is to be scanned.  effective_decl_level indicates the name scope
to which the class declaration belongs.  is_local_class is TRUE if the class
definition appears inside a function body.  delayed_nested_class_def is TRUE
if the class is a nested class whose parent class definition has already been
completed (C++ only).  is_template_instantiation is TRUE when a template
is being instantiated either for the purpose of producing the prototype
instantiation or for generating a real instantiation.  It is also TRUE for
nested classes when their definition appears outside of the class template.
If a prototype instantiation is produced, il_template_entry is set to the
template entry for the class template definition; otherwise it is NULL.
is_template_specialization is TRUE for explicit specializations of template
classes.
*/
{
  a_boolean                        err = FALSE;
  a_symbol_ptr                     tag_sym;
  a_scope_ptr                      scope_ptr;
  a_class_symbol_supplement_ptr    cssp;
  a_class_type_supplement_ptr      ctsp = class_type_supp(class_type);
  a_routine_fixup_ptr              saved_routine_fixup;
  a_template_symbol_supplement_ptr class_tssp;
  a_token_sequence_number          last_token_number_of_definition;
  a_class_def_state                class_state;
  a_boolean                        skip_semicolon_check;
  a_type_ptr                       dummy_type;
  a_boolean			   instantiation_scope_pushed = FALSE;
  a_boolean			   is_in_class_specialization;
#if USER_CONTROL_OF_STRUCT_PACKING
  a_pack_alignment_state           saved_pack_alignment_state;
  a_boolean			   need_restore_pack_alignment_statate = FALSE;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  a_boolean                        class_is_in_valid_scope;
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_scope_depth                   class_scope_depth;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if DO_IL_LOWERING && IA64_ABI
  a_routine_ptr                   rout;
#endif /* DO_IL_LOWERING && IA64_ABI */
  a_source_position               end_pos;

  db_enter(3, "scan_class_definition");
  initialize_class_def_state(class_type, &class_state);
  class_state.is_local_class = is_local_class;
  /* Increment the counter of class definitions currently in progress. */
  curr_class_fixup_header(/*for_instantiation=*/FALSE)->
                                                   pending_class_definitions++;
  tag_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
  cssp = tag_sym->variant.class_struct_union.extra_info;
  class_tssp = cssp->template_info;
  /* A copy constructor need not be generated if construction by bitwise
     copy is equivalent.  When a class is being defined, set the flag to
     TRUE initially, and change it if a base class or member is declared
     that precludes construction by bitwise copy. */
  cssp->construction_by_bitwise_copy_allowed = TRUE;
  /* Similarly, assignment by bitwise copy is allowed unless there are
     virtual base classes, virtual functions, or base classes or fields
     for which bitwise copy is not allowed. */
  cssp->assignment_by_bitwise_copy_allowed = TRUE;
#if USER_CONTROL_OF_STRUCT_PACKING
  if (class_type->variant.class_struct_union.max_member_alignment == 0) {
    /* Determine the alignment adjustment required for packing (unless it was
       already set). */
    class_type->variant.class_struct_union.max_member_alignment =
                                  current_max_alignment_for_class_members();
  }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  /* Set the instantiation insert point to assure that instantiations are
     inserted immediately before the source sequence entry for the class
     definition itself.  Save the depth so the insert point can be restored. */
  scope_stack[depth_scope_stack].ss_list_instantiation_insert_point =
                           class_type->source_corresp.source_sequence_entry;
  class_scope_depth = depth_scope_stack;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  is_in_class_specialization = tag_sym->is_class_member &&
                               is_template_specialization &&
                               !delayed_nested_class_def;
  class_type->variant.class_struct_union.is_in_class_specialization =
                                                    is_in_class_specialization;
  if (C_dialect == C_dialect_cplusplus) {
#if BACK_END_IS_CP_GEN_BE
    /* Set the "name linkage environment" for this class type.  This is used
       by the C++-generating back end to decide when to emit extern "C"; this
       may be necessary because extern "C" cannot be emitted in the class.
       E.g.,    extern "C" {
                  struct A {
                    void f() { void g(); g(); } -- ::g has extern "C" linkage
                  };
                }                                                           */
    ctsp->surrounding_name_linkage_state =
                          scope_stack[depth_scope_stack].default_name_linkage;
#endif /* BACK_END_IS_CP_GEN_BE */
    if (class_type->variant.class_struct_union.is_prototype_instantiation ||
        (scope_stack[depth_scope_stack].in_prototype_instantiation &&
         (class_type->source_corresp.is_local_to_function ||
          scope_stack[depth_scope_stack].in_class_specialization))) {
      /* This is a prototype instantiation or an instantiation of a local
         class type, so the resulting class is "nonreal" (i.e., based on
         template arguments that include the dummy types and constants of
         template parameters rather than real types and constants).  The
         in_class_specialization test detects classes nested within a
         Microsoft/Sun in-class specialization.   Note that for nested classes
         the flag is set later. */
      class_state.is_nonreal_instantiation = TRUE;
      class_type->variant.class_struct_union.is_nonreal_class = TRUE;
      if (tag_sym->is_class_member &&
          class_tssp != NULL &&
          (curr_token == tok_lbrace || curr_token == tok_colon)) {
        /* This is a definition of a nested class.  See if the enclosing class
           is a prototype and/or nonreal class.  If so, copy the information
           to the current class. */
        check_nonreal_nested_class(tag_sym, class_tssp);
      }  /* if */
    } else if (class_type->variant.class_struct_union.is_nonreal_class) {
      /* A nonreal class, but not a prototype instantiation.  This can
         occur when an in-class specialization occurs in a prototype
         instantiation.  Such specializations are only allowed in Microsoft
         mode, but may still occur (with an error) in other modes. */
      class_state.is_nonreal_instantiation = TRUE;
      if (allow_in_class_specializations &&
          class_type->variant.class_struct_union.is_specialized) {
        class_state.is_nonreal_instantiation = TRUE;
      } else {
        /* This can only occur in strange error situations, such as:
             template<template <class X> class T> struct S struct T<int> {};
           Check that an error has been or will be issued. */
        expect_error();
      }  /* if */
    } else if (is_template_instantiation && tag_sym->is_class_member) {
      /* An instance of a member template.  Mark it as nonreal if the
         instantiation is being triggered inside a prototype instantiation. */
      if (sym_parent_class(tag_sym)
                              ->variant.class_struct_union.is_nonreal_class) {
        class_state.is_nonreal_instantiation = TRUE;
        class_type->variant.class_struct_union.is_nonreal_class = TRUE;
      }  /* if */
    }  /* if */
    /* If this class is nested in an in-class specialization, consider it
       an in-class specialization too.  An exception is made for a class
       template declared within an in-class specialization. */
    if (tag_sym->is_class_member &&
        sym_parent_class(tag_sym)
                    ->variant.class_struct_union.is_in_class_specialization &&
        (!class_type->variant.class_struct_union.is_prototype_instantiation ||
         ctsp->template_arg_list == NULL)) {
      class_type->variant.class_struct_union.is_in_class_specialization = TRUE;
    }  /* if */
    class_state.is_template_instantiation = is_template_instantiation;
    /* Find the prototype instantiation symbol associated with this
       real instantiation. */
    class_state.corresp_prototype_tag_sym =
                            corresp_prototype_for_class_symbol(tag_sym);
#if USER_CONTROL_OF_STRUCT_PACKING
    if (class_state.corresp_prototype_tag_sym != NULL) {
      /* The class is an instantiation of a class template (or a class nested
         within such an instantiation).  Overwrite the alignment entered for
         this class (which was based on the instantiation context) with the
         alignment in force at the point of the template definition, as
         recorded in the type of the prototype instantiation. */
      a_type_ptr  tp = class_state.corresp_prototype_tag_sym->
                                         variant.class_struct_union.type;
      class_type->variant.class_struct_union.max_member_alignment =
                         tp->variant.class_struct_union.max_member_alignment;
    }  /* if */
    if (is_template_instantiation &&
        !class_type->variant.class_struct_union.is_prototype_instantiation) {
      /* Since a template instantiation may appear out of sequence relative
         to the textual sequence of the source program, reset the alignment
         state (saving the current state to restore it later).  Note that
         prototype instantiations are not handled this way -- #pragma pack
         directives that appear in them are processed in the normal way. */
      reset_pack_alignment_state(class_type->variant.class_struct_union.
                                                      max_member_alignment,
                                 &saved_pack_alignment_state);
      need_restore_pack_alignment_statate = TRUE;
    }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
    if (use_microsoft_specialization_scope && !is_in_class_specialization &&
        is_real_template_instance_specific_def_symbol(tag_sym)) {
      /* The Microsoft compiler permits a class specialization to reference
         template parameters of the template.  Push an instantiation scope
         if this is a specialization definition.  (This is not allowed for
         in-class specializations.) */
      a_scope_depth  depth;

      push_instantiation_scope_for_class(
                      class_type, /*is_microsoft_specialization_scope=*/TRUE);
      instantiation_scope_pushed = TRUE;
      depth = depth_scope_stack;
      scope_stack[depth].microsoft_specialization_instantiation_scope = TRUE;
    } else if (delayed_nested_class_def && !is_template_instantiation) {
      /* This is a definition of a C++ nested class that appears outside the
         scope of the parent class definition itself.  Reactivate the
         lexical context.  Note that this is done before the base specifiers
         are scanned so that symbols from the enclosing class are visible.
         For template instantiations, this is done when the template
         instantiation scope is pushed.  Note that this is not done when
         a template instantiation scope is pushed for a specialization
         in Microsoft mode (above) because that process reactivates the
         enclosing class. */
      push_class_reactivation_scope(sym_parent_class(tag_sym),
                                    /*extend_namespace=*/TRUE);
    }  /* if */
    if (curr_token == tok_colon) {
      /* Scan the list of base specifiers. */
      add_stop_token(tok_lbrace);
      scan_base_specifier_list(class_type, &class_state);
      remove_stop_token(tok_lbrace);
      /* If there is a base specifier list and this is a class or struct
         declaration, it has to be definition, which means the next token
         should be a brace. */
      if (curr_token != tok_lbrace) {
        if (class_type->kind == (a_type_kind)tk_union) {
          /* An error has already been issued. */
        } else {
          syntax_error(ec_missing_class_definition);
        }  /* if */
        err = TRUE;
        /* Clear the base-classes field to avoid problems down the line. */
        ctsp->base_classes = NULL;
        if (!instantiation_scope_pushed &&
            delayed_nested_class_def && !is_template_instantiation) {
          /* Restore the scope stack to its original state. For template
             instantiations, this is done when the template instantiation
             scope is popped. */
          pop_class_reactivation_scope();
        }  /* if */
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cppcli_enabled) add_implicit_cli_bases(&class_state);
    check_if_potentially_interface_like(&class_state);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (ctsp->base_classes != NULL) wrapup_base_classes(&class_state);
  }  /* if */
  if (curr_token == tok_lbrace) {
    /* Scan the structure or union definition. */
    /* Start a scope for the fields and other members.  Since the class type
       is allocated in the file scope memory region, all its members must also
       allocated there -- push_scope will switch to the file scope memory
       region; pop_scope will switch back. */
    scope_ptr = push_scope((a_scope_kind)sck_class_struct_union,
                           NO_SCOPE_NUMBER, class_type, (a_routine_ptr)NULL);
    scope_stack_top().class_def_state = &class_state;
    class_is_in_valid_scope = !is_invalid_scope_for_class();
    if (!class_is_in_valid_scope && is_template_dependent_context()) {
      /* We've got a class in an invalid context (e.g., a lambda in a function
         prototype scope), and template parameters will be visible in the
         body of the class, so mark the class as nonreal.  This tells
         overload resolution that there might be member functions that have
         template parameters in their signatures and therefore for which
         overload resolution should be suppressed. */
      expect_error();
      class_type->variant.class_struct_union.is_nonreal_class = TRUE;
    }  /* if */
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
    if (!C_mode()) {
      scope_stack_top().ELF_visibility = ctsp->ELF_visibility;
    }  /* if */
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
    /* Begin a new stop token state. */
    push_stop_token_stack();
    /* Record the associated scope in the class type supplement. */
    ctsp->assoc_scope = scope_ptr;
    /* Advance past the left brace. */
    (void)get_token();
    add_stop_token(tok_rbrace);
    if (!C_mode()) {
      saved_routine_fixup = curr_routine_fixup;
      curr_routine_fixup = NULL;
      if (class_name_injection_enabled) {
        /* In C++ the name of the class is entered into the scope of the
           class; enter an sk_type symbol. */
        if (microsoft_bugs && microsoft_version < 1400 &&
            !class_type->variant.class_struct_union.is_specialized &&
            ctsp->template_arg_list != NULL) {
          /* In Microsoft bugs mode for Microsoft versions prior to 8.0,
             template class names are not injected.  (NB: code in
             check_hiding_by_inherited_names duplicates this test to simulate
             an injected class name for the hidden name table.  If this
             condition changes, so should that one.) */
        } else {
          enter_injected_class_name_symbol(tag_sym);
        }  /* if */
      }  /* if */
    }  /* if */
    if (curr_token == tok_rbrace) {
      /* A member list is optional in C++.  In C mode issue an error and add
         a dummy field to reduce error recovery problems down the line. */
      if (gcc_mode) {
        /* In GNU C mode, empty classes are allowed and have size zero.
           Set the empty class bit early to distinguish this from an
           incomplete type. */
        class_type->variant.class_struct_union.is_empty_class = TRUE;
      } else if (C_mode()) {
        error(ec_exp_declaration);
        add_error_field(class_type, &class_state.end_of_field_list);
      }  /* if */
    } else {
      if (class_type->kind == (a_type_kind)tk_class
#if MICROSOFT_EXTENSIONS_ALLOWED
          && !cli_class_type_kind_is(class_type, cctk_interface)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                                ) {
        /* Members of a C++ class have private access by default. */
        class_state.access = (an_access_specifier)as_private;
      } else {
        /* Members of a C++ struct or union have public access by default,
           which is also the implicit access control for C struct and union
           fields. */
        class_state.access = (an_access_specifier)as_public;
      }  /* if */
      scope_stack[decl_scope_level].current_access = class_state.access;
      do {
        an_ms_attribute_ptr  ms_attributes = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
        a_source_position    decl_start_pos;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        add_stop_token(tok_semicolon);
        /* This is a valid location for an __if_exists pragma to appear when
           creating source sequence entries for __if_exists. */
        check_for_if_exists_pragmas();
        /* Move cached #pragma declarations (if any) to the current scope
           stack entry so they can be examined and acted upon in subsequent
           processing. */
        (void)select_curr_construct_pragmas(/*add_to_list=*/FALSE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
        /* Reset the insertion point for instantiations to NULL. */
        reset_ss_list_instantiation_insert_point();
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        if (C_dialect == C_dialect_cplusplus) {
          /* An access specification may appear anywhere amid the member
             declarations.  Check for it each time through the loop, and adjust
             the value of class_state.access accordingly. */
          if (scan_access_specification(&class_state)) {
            /* An access specifier was found.  This next check catches cases
               like "...public: }". */
            if (curr_token == tok_rbrace) {
              /* Issue diagnostics on pragmas that are trying to bind to a
                 nonexistent declaration. */
              cannot_bind_to_curr_construct();
              /* Exit the loop. */
              remove_stop_token(tok_semicolon);
              break;
            }  /* if */
          }  /* if */
        }  /* if */
        /* Scan a member declaration. */
        if (curr_token == tok_semicolon && 
            (C_dialect == C_dialect_cplusplus ||
             !(class_state.is_first_field && next_token() == tok_rbrace))) {
          /* No declaration -- just a semicolon.  Issue a warning (or error in
             strict ANSI mode).  Note: in C mode we bypass the "extra ':'"
             diagnostic when there are no fields in the struct -- i.e.,
             "struct S { ; };" is treated just like "struct S { };". */
          pos_diagnostic(strict_ansi_mode ?
                           strict_ansi_discretionary_severity : es_warning,
                         ec_extra_semicolon, &pos_curr_token);
          cannot_bind_to_curr_construct();
          /* Bypass the superfluous semicolon and continue looping. */
          (void)get_token();
          treat_declaration_as_okay_in_property_or_event(&class_state);
          goto next_declaration;
        }  /* if */
#if !ASM_FUNCTION_ALLOWED
        /* Check for an (illegal) asm declaration. */
        if (curr_token == tok_asm || curr_token == tok_microsoft_asm) {
          /* An asm declaration is not allowed in a class definition, but
             scan it anyway (after issuing the error). */
          an_attribute_ptr  attributes;
          (void)asm_declaration(/*asm_decl_allowed=*/FALSE,
                                /*is_asm_statement=*/FALSE,
                                &attributes);
          /* The semicolon will have been consumed by the subroutine.
             Continue looping through the members. */
          treat_declaration_as_okay_in_property_or_event(&class_state);
          goto next_declaration;
        }  /* if */
#endif /* !ASM_FUNCTION_ALLOWED */
        if (C_dialect == C_dialect_cplusplus) {
          a_boolean	is_generic = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (microsoft_mode) {
            /* Scan any Microsoft attributes, and perhaps a leading
               parenthesis. */
            a_boolean  complete_decl;
            decl_start_pos = pos_curr_token;
            scan_microsoft_member_decl_prefix(&class_state, &ms_attributes,
                                              &complete_decl);
            if (complete_decl) goto next_declaration;
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          /* Check for and discard declarations of the form "overload f;". */
          if (check_for_overload_anachronism()) {
            /* Issue diagnostics on pragmas that are trying to bind to an
               overload declaration. */
            cannot_bind_to_curr_construct();
            (void)required_token(tok_semicolon, ec_exp_semicolon);
            goto next_declaration;
          }  /* if */
          /* Check for a using declaration, alias declaration, or
             static_assert declaration. */
          if (curr_token == tok_using) {
            member_using_or_alias_declaration(class_type, class_state.access);
            goto next_declaration;
          } else if (curr_token == tok_static_assert) {
            static_assert_declaration(/*leave_semicolon=*/FALSE);
            goto next_declaration;
          }  /* if */
          /* Check for an access adjustment declaration. */
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (cppcli_enabled && curr_token == tok_identifier &&
              !identifier_starts_name_qualifier_or_template_id()) {
            /* In C++/CLI mode, a separate test is needed first to avoid
               calling is_decl_qualified_name_start() on a valid context-
               sensitive keyword that is followed by a "::".  E.g.:
                 typedef int I;
                 ref class C { property ::I p; };
               In this example, calling is_decl_qualified_name() would complain
               that "property" is not a class or namespace name. */
          } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          /* Do not insert code here. */
          if (is_decl_qualified_name_start() &&
              !f_same_entities(qualifier_class_type(locator_for_curr_id),
                               class_type) &&
              locator_for_curr_id.is_qualified_name &&
              next_token() == tok_semicolon) {
            /* This looks syntactically like an access adjustment declaration.
               Be sure the semantics are correct.  Its semantics are the same
               as a using-declaration. */
            member_using_or_alias_declaration(class_type, class_state.access);
            goto next_declaration;
          }  /* if */
          /* Check for template declaration. */
          if (curr_token == tok_template || curr_token == tok_export ||
              (extern_template_allowed && curr_token == tok_extern &&
               next_token() == tok_template) ||
               (cppcli_enabled &&
                (is_generic = is_start_of_generic_decl()/*lint --e(820)*/))) {
            /* A template declaration in a class may be a member template
               declaration or a friend declaration.  Explicit instantiations
               are not permitted in a class context.  The error for an
               explicit instantiation in a class will be issued by
               template_directive_or_declaration. */
            a_token_kind                 final_token = tok_semicolon;
            a_template_decl_options_set  td_flags = TDO_NO_OPTIONS;
            a_source_position	          directive_start_pos = pos_curr_token;

#if MICROSOFT_EXTENSIONS_ALLOWED
            if (ms_attributes != NULL) {
              dispose_of_unapplied_attributes(&ms_attributes,
                                              ec_ms_attr_not_allowed);
            }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            if (curr_token == tok_extern) {
              /* In some modes "extern template ..." is permitted. */
              (void)get_token();
              td_flags = TDO_EXTERN;
            } else if (is_generic) {
              /* A C++/CLI generic declaration. */
              td_flags |= TDO_GENERIC;
            }  /* if */
            template_directive_or_declaration(&final_token, td_flags,
                                              &directive_start_pos);
            /* The terminating token will be either a semicolon or a right
               brace.  The latter has already been checked for, but the former
               has not. */
            if (curr_token == tok_end_of_source) {
              /* In some variadic rescan cases, the terminating
                 tok_end_of_source can end up being the current token. */
              (void)get_token();
            }  /* if */
            if (final_token == tok_semicolon) {
              (void)required_token_no_advance(tok_semicolon, ec_exp_semicolon);
            }  /* if */
            /* Advance past the terminating token. */
            if (curr_token == final_token) (void)get_token();
            goto next_declaration;
          }  /* if */
        }  /* if */
        (void)class_member_declaration(class_type, &class_state, ms_attributes,
                                       /*is_template_member=*/FALSE,
                                       (a_template_param_ptr)NULL,
                                       &skip_semicolon_check,
                                       &dummy_type,
                                       (a_template_instance_ptr)NULL,
                                       (a_template_ptr)NULL,
                                       (a_decl_pos_block *)NULL);
        if (!skip_semicolon_check) {
          /* Check for and ignore the semicolon following the member
             declaration.  It's optional after the last declaration in
             pcc mode; as an extension, it is also accepted with a warning
             in nonstrict ANSI C modes. */
          if (curr_token == tok_rbrace) {
            /* The final semicolon is omitted. */
            if (C_dialect != C_dialect_pcc) {
              diagnostic(strict_ansi_mode ?
                                          strict_ansi_discretionary_severity :
                         C_mode() ? es_warning : es_discretionary_error,
                         ec_exp_semicolon);
            }  /* if */ 
          } else {
            (void)required_token(tok_semicolon, ec_exp_semicolon);
          }  /* if */
        }  /* if */
next_declaration:
        if (curr_routine_fixup != NULL) dispose_of_curr_routine_fixup();
        remove_stop_token(tok_semicolon);
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (class_state.property_or_event_descr != NULL) {
          check_cli_accessor_decl(&class_state, &decl_start_pos);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Keep processing member declarations until the closing brace or
           the end-of-source marker is reached. */
      } while (curr_token != tok_rbrace && curr_token != tok_end_of_source);
      /* Check that a non-empty struct/union in C mode has at least one
         named field. */
      if (C_mode() && !class_state.any_fields_other_than_unnamed_bitfields) {
        /* Something like "struct S { int:1; };", which has undefined behavior
           according to the C standard.  Issue a diagnostic. */
        diagnostic(strict_ansi_mode ? strict_ansi_discretionary_severity
                                    : es_warning,
                   ec_no_named_fields);
      }  /* if */
    }  /* if */
    if (is_template_instantiation && delayed_nested_class_def) {
      /* Force the functions to compute the scope depth, if any. */
      effective_decl_level = NO_SCOPE_DEPTH;
    }  /* if */
    /* This is a valid location for an __if_exists pragma to appear when
       creating source sequence entries for __if_exists. */
    check_for_if_exists_pragmas();
    /* Process pragmas associated with the closing brace before the current
       scope is popped and before add_end_of_construct_source_sequence_entry
       is called. */
    process_curr_token_pragmas();
    /* Check for and ignore the closing brace. */
    last_token_number_of_definition = curr_token_sequence_number;
    /* Since a brace is a single-character token, pos_curr_token is also the
       end position of that token.  (I.e., end_pos will be a correct end
       position.) */
    end_pos = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    /* Update the end-of-decl-specifiers source position. */
    if (decl_pos_block != NULL) {
      decl_pos_block->specifiers_range.end = pos_curr_token;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    (void)required_token(tok_rbrace, ec_exp_rbrace);
    if (gnu_mode && curr_token == tok_attribute) {
      an_attribute_ptr  attributes =
                            scan_gnu_attribute_groups(al_post_tag_definition);
      if (attributes != NULL) {
        last_token_number_of_definition = last_token_number_of_attributes;
        end_pos = end_position_of_attributes;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        if (decl_pos_block != NULL) {
          /* Update the recorded end position to the end of the attributes
             specifier. */
          decl_pos_block->specifiers_range.end = curr_construct_end_position;
        }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        mark_primary_decl_attributes(attributes);
        attach_attributes(attributes, (char*)class_type, iek_type);
      }  /* if */
    }  /* if */
    /* Issue a warning if the current token is in a file different from the
       last token of the class definition. */
    check_for_file_with_unterminated_type_definition(&end_pos);
    if (effective_decl_level != NO_SCOPE_DEPTH &&
        scope_stack[effective_decl_level].kind ==
                                     (a_scope_kind)sck_template_declaration) {
      /* Something went wrong if we are in a template declaration scope;
         we ought to be in class_template_declaration instead.  An error
         has been or will be issued elsewhere. */
      expect_error();
    } else {
      complete_class_definition(class_type, effective_decl_level,
                                &class_state);
    }  /* if */
#if USER_CONTROL_OF_STRUCT_PACKING
    if (need_restore_pack_alignment_statate) {
      /* Now that the class instantiation has been scanned, restore the
         original pack alignment state.  Note that this must occur after the
         pragmas associated with the closing brace have been processed. */
      restore_pack_alignment_state(&saved_pack_alignment_state);
    }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Add a source sequence entry marking the end of the class definition. */
    if (il_template_entry != NULL) {
      /* This is a prototype instantiation of a class template. */
      add_end_of_construct_source_sequence_entry(
                                        (char *)il_template_entry,
                                        (a_byte_il_entry_kind)iek_template);
    } else
    /* Do not insert code here. */
    {
      add_end_of_construct_source_sequence_entry(
                         (char *)class_type, (a_byte_il_entry_kind)iek_type);
    }  /* if */
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  /* Clear the modified the ss-list instantiation insert point for the scope
     to which the class being defined belongs.  This has to be done before
     the call to pop_template_instantiation_scope -- otherwise, the
     insert point is wrong for the class body.  Note also that, at this
     point, the depth of the innermost namespace scope will not be on the top
     of the stack if an extra instantiation scope was pushed. */
    scope_stack[class_scope_depth].ss_list_instantiation_insert_point = NULL;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Pop the scope created for the class definition. */
    pop_scope();
    if (delayed_nested_class_def) {
      /* A nested class defined outside the parent class definition. */
      class_type->variant.class_struct_union
                               .nested_class_defined_outside_of_parent = TRUE;
      if (is_template_instantiation || instantiation_scope_pushed) {
        /* The class reactivation scope is popped along with the template
           instantiation scope. */
      } else {
        /* Restore the scope stack to its original state. */
        pop_class_reactivation_scope();
      }  /* if */
    }  /* if */
    remove_stop_token(tok_rbrace);
    /* Restore the stop token state. */
    pop_stop_token_stack();
    /* If entities dependent on this class were declared before the class
       was defined, they will have been recorded on a fixup list.  Now
       go through the fixup list and complete the declarations.  (Note that
       this must be done before inline function bodies are scanned, since
       return code may be affected by how the routine calling method flag
       is set.)  If we're in a template declaration scope, something went
       wrong earlier (diagnostic elsewhere) and this processing would make
       no sense. */
    if (depth_template_declaration_scope == NO_SCOPE_DEPTH) {
      check_dependent_type_fixup_list(tag_sym);
    }  /* if */
    /* Build a list of the namespaces in which this class and its bases
       classes are defined.  This is needed to look up operators that
       operate on this class type. */
    if (!C_mode()) determine_operator_lookup_namespaces(class_type);
    if (C_dialect == C_dialect_cplusplus) {
      /* Rescan tokens that were cached (inline function definitions, default
         arguments). */
      if ((!tag_sym->is_class_member || delayed_nested_class_def ||
           is_in_class_specialization) && class_is_in_valid_scope) {
        /* For non-nested classes add the class to the list of classes for
           which delayed processing for default argument declarations and
           inline member function definitions must be done.  The actual
           processing will be done when all pending class definitions have
           been completed.  Don't add the fixup entries if this class appeared
           in an invalid location. */
        add_to_class_fixup_list(class_type, is_template_instantiation);
      }  /* if */
      curr_routine_fixup = saved_routine_fixup;
      if (class_type->variant.class_struct_union.is_prototype_instantiation &&
          !class_type->variant.class_struct_union.is_specialized) {
        a_template_symbol_supplement_ptr  tssp = class_tssp;
        check_assertion(tssp != NULL);
        tssp->variant.class_template.prototype_instantiation = tag_sym;
        tssp->variant.class_template.prototype_instantiation_complete = TRUE;
        if (tag_sym->is_class_member && tssp->cache_segment != NULL) {
          /* For a nested class, save the ending token number of the
             definition.  This is only done when the nested class is
             defined within the enclosing class.  When the class is
             defined outside of the enclosing class, cache_segment will be
             NULL. */
          tssp->cache_segment->last_token_number =
                                              last_token_number_of_definition;
        }  /* if */
        if (curr_token != tok_semicolon) {
          /* If the token following the closing brace of the class is not 
             a semicolon, then the class (if it is a nested class) is not
             "standalone", meaning that the body cannot be extracted from
             the enclosing template.  Nested classes that are not
             standalone cannot be specialized. */
          tssp->variant.class_template.not_standalone_nested_class = TRUE;
        }  /* if */
      }  /* if */
      if (is_template_specialization && microsoft_mode &&
          !class_state.is_nonreal_instantiation) {
        /* The Microsoft compiler allows a template friend declaration to
           also affect members of explicit specializations. */
        update_friend_info_for_specialization(class_type);
      }  /* if */
    }  /* if */
#if DO_IL_LOWERING && IA64_ABI
    /* Keep track of which routines are marked inline at this point.  The IA64
       ABI requires this information when deciding whether or not to emit a
       virtual function table. */
    if (C_dialect == C_dialect_cplusplus) {
      for (rout = scope_ptr->routines; rout != NULL; rout = rout->next) {
        if (rout->is_inline) rout->inline_in_class_definition = TRUE;
      }  /* for */
    }  /* if */
#endif /* DO_IL_LOWERING && IA64_ABI */
  }  /* if */
  /* Decrement the counter of class definitions currently in progress. */
  curr_class_fixup_header(/*for_instantiation=*/FALSE)->
                                                   pending_class_definitions--;
  if (instantiation_scope_pushed) {
    /* If an instantiation scope was pushed earlier to support the Microsoft
       bug that permits a specialization to reference a template parameter,
       pop that scope now. */
    pop_template_instantiation_scope();
  }  /* if */

  db_exit();
  return !err;
}  /* scan_class_definition */


static void scan_lambda_capture_list(a_lambda_ptr  lambda)
/*
Scan the capture list for a lambda construct associated with lambda.  The
caller has already moved past the '[', and this routine leaves the trailing
']' to be consumed by the caller.  The grammar to be parsed is thus:

    lambda-capture(opt)

    lambda-capture:
        capture-default | capture-list | capture-default ',' capture-list

    capture-default:
        '&' | '='

    capture-list:
        capture | capture-list ',' capture

    capture:
        identifier | '&' identifier | 'this'
*/
{
  a_token_kind       tok_after_ref = tok_error;
  a_source_position  capture_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position  capture_end_pos;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  add_stop_token(tok_comma);
  if (curr_token == tok_ampersand) tok_after_ref = next_token();
  /* Look for a capture-default token. */
  if (curr_token == tok_assign ||
      (curr_token == tok_ampersand &&
       (tok_after_ref == tok_comma || tok_after_ref == tok_rbracket))) {
    lambda->has_capture_default = TRUE;
    lambda->default_is_by_reference = (curr_token == tok_ampersand);
    /* Skip the & or =, and if the next token is a comma, skip that too. */
    (void)get_token();
    if (curr_token == tok_comma) {
      a_source_position  pos_first_comma;
      pos_first_comma = pos_curr_token;
      (void)get_token();
      if (curr_token == tok_rbracket) {
        /* Something like "[ = , ](){}".  Issue an error. */
        pos_diagnostic(es_discretionary_error, ec_nonstd_extra_comma,
                       &pos_first_comma);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Now scan the explicit captures. */
  if (curr_token != tok_rbracket) {
    do {
      a_pack_expansion_stack_entry_ptr pesep;
      a_boolean                        any_more;
      any_more = begin_potential_pack_expansion_context(&pesep);
      /* This inner loop repeats if there is a variadic template pack
         expansion. */
      while (any_more) {
        a_pack_expansion_descr_ptr pedep;
        a_lambda_capture_ptr       lcp = NULL;
        a_source_position          pos_capture;
        a_variable_ptr             var = NULL;
        a_boolean                  by_ref = FALSE;
        pos_capture = pos_curr_token;
        if (curr_token == tok_ampersand) {
          by_ref = TRUE;
          (void)get_token();
        }  /* if */
        capture_pos = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        capture_end_pos = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        if (curr_token == tok_this) {
          /* Capture of "this" from an enclosing class.  (This is not the
             "this" of a closure class member.) */
          if (!variable_this_exists(&var)) {
            /* We should be in a nonstatic member function. */
            error(ec_this_used_incorrectly);
          } else if (by_ref) {
            /* "&this" is not allowed in a capture list. */
            pos_error(ec_cannot_capture_this_by_reference, &pos_capture);
            var = NULL;
          }  /* if */
          (void)get_token();
        } else if (curr_token == tok_identifier) {
          /* Explicit capture of what should be a local automatic variable.
             Look up the identifier. */
          a_symbol_ptr  sym = normal_id_lookup(&locator_for_curr_id,
                                               IDL_DO_NOT_CREATE_PROJ_SYM);
          if (sym == NULL) {
            str_error(ec_undefined_identifier,
                      locator_for_curr_id.symbol_header->identifier);
          } else {
            record_potential_pack_reference(sym, &pos_curr_token);
            if (sym->kind != (a_symbol_kind)sk_variable) {
              sym_error(ec_not_a_variable, sym);
            } else {
              an_error_code  diag = ec_no_error;
              var = sym->variant.variable.ptr;
              if (!check_var_for_lambda_capture(var, /*implicit=*/FALSE,
                                                &diag)) {
                error(diag);
                var = NULL;
              }  /* if */
            }  /* if */
          }  /* if */
          (void)get_token();
        } else {
          syntax_error(ec_exp_identifier);
        }  /* if */
        if (lambda->has_capture_default &&
            lambda->default_is_by_reference == by_ref && var != NULL) {
          /* An explicit capture cannot match the default capture mode. */
          pos_diagnostic(es_discretionary_error,
                         ec_capture_mode_matches_default, &pos_capture);
        }  /* if */
        if (var != NULL) {
          /* See if there is already a capture entry for this variable. */
          if (find_lambda_capture(lambda, var) != NULL) {
            /* A name cannot appear more than once in the capture list. */
            pos_diagnostic(es_discretionary_error,
                           ec_more_than_one_capture, &capture_pos);
          } else {
            /* Create the lambda capture entry for this variable. */
            a_boolean             no_impl_capture;
            lcp = add_lambda_capture(lambda, var, /*is_implicit=*/FALSE,
                                     by_ref, &capture_pos, &no_impl_capture);
            check_assertion(lcp != NULL);
#if EXTRA_SOURCE_POSITIONS_IN_IL
            lcp->end_position = capture_end_pos;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
            if (no_impl_capture) {
              pos_error(ec_no_implicit_capture_on_enclosing_lambda,
                        &capture_pos);
            }  /* if */
          }  /* if */
        }  /* if */
        pedep = end_potential_pack_expansion_context(pesep,
                                                     /*is_declarator=*/FALSE);
        if (pedep != NULL && lcp != NULL) {
          /* This capture is a variadic template pack expansion, i.e.,
             it's followed by "...".  Furthermore, we're in the prototype
             instantiation, so mark the capture as a pack expansion. */
          lcp->is_pack_expansion = TRUE;
        }  /* if */
        any_more = advance_to_next_pack_element(pesep);
      }  /* while */
    } while (loop_token(tok_comma));
  }  /* while */
  remove_stop_token(tok_comma);
}  /* scan_lambda_capture_list */


static void decl_lambda_capture_fields(a_lambda_ptr  lambda)
/*
Declare the fields corresponding to the "captures" of the given lambda in its
associated closure type.
*/
{
  a_lambda_capture_ptr  lcp;

  for (lcp = lambda->capture_list; lcp != NULL; lcp = lcp->next) {
    lcp->closure_field = make_field_for_lambda_capture(
                                    lambda, lcp->variable,
                                    lcp->capture_by_reference, &lcp->position);
  }  /* for */
}  /* decl_lambda_capture_fields */


static void push_closure_class(a_lambda_ptr           lambda,
                               a_class_def_state_ptr  class_state)
/*
Push the scope stack entry for the closure class for lambda and
initialize class_def_state.
*/
{
  a_type_ptr  closure_class = lambda->closure_class;

  initialize_class_def_state(closure_class, class_state);
  if (innermost_function_scope != NULL || inside_local_class) {
    class_state->is_local_class = TRUE;
  }  /* if */
  class_state->access = (an_access_specifier)as_public;
  /* Lambdas are forced to be non-POD so that any default initialization will
     be done by attempting to call the default constructor (which will
     fail). */
  class_state->POD_ruled_out = TRUE;
  /* Don't allow aggregate initialization of a closure object. */
  class_state->class_aggregate_ruled_out = TRUE;
  class_state->is_nonreal_instantiation =
                    closure_class->variant.class_struct_union.is_nonreal_class;
  class_type_supp(closure_class)->assoc_scope =
             push_scope((a_scope_kind)sck_class_struct_union, NO_SCOPE_NUMBER,
                        closure_class, (a_routine_ptr)NULL);
  scope_stack_top().class_def_state = class_state;
}  /* push_closure_class */


static void scan_optional_lambda_declarator(a_lambda_ptr        lambda,
                                            a_func_info_block   *func_info,
                                            a_member_decl_info  *decl_info)
/*
For the given lambda, parse the (optional) declarator-like construct, which
consists of a parameter list and, optionally, a mutable specifier, an exception
specification, and/or a return type specification.  Return properties of the
implied call operator in *func_info and *decl_info (both are initialized here).
*/
{
  a_decl_parse_state  *dps = &decl_info->decl_state;
  a_decl_pos_block    *decl_pos_block = &decl_info->decl_pos_block;

  clear_func_info(func_info);
  func_info->lambda = lambda;
  initialize_member_decl_info(decl_info, &pos_curr_token);
  decl_info->is_first_in_declarator_list = TRUE;
  dps->type = dps->specifiers_type = void_type();
  dps->start_pos = dps->specifiers_pos = pos_curr_token;
  dps->in_class_scope = TRUE;
  dps->declarator_start_pos = dps->declarator_pos = pos_curr_token;
  if (curr_token == tok_lparen) {
    /* A parameter list presumably follows. */
    add_stop_token(tok_lbrace);
    scan_lambda_declarator(lambda, dps, func_info, decl_pos_block);
    lambda->has_parameter_decl = TRUE;
    remove_stop_token(tok_lbrace);
  } else {
    /* The parameter list was omitted: Treat this as if the declarator-like
       construct was just an empty parameter list.  This also means that the
       return type is unknown at this point. */
    a_routine_type_supplement_ptr  rtsp;
    dps->type = make_routine_type(unknown_type(), /*param1_type=*/NULL,
                                  /*param2_type=*/NULL, /*param3_type=*/NULL,
                                  /*param4_type=*/NULL);
    rtsp = dps->type->variant.routine.extra_info;
    rtsp->this_class = lambda->closure_class;
    rtsp->qualifiers = TQ_CONST;
    dps->declared_type = dps->type;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    func_info->declared_type = dps->type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
}  /* scan_optional_lambda_declarator */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean is_lambda(void)
/*
The current token is a "[" that could be the start of a lambda, or in Microsoft
mode could be the start of a Microsoft attribute.  Look ahead to determine
whether this is a lambda.  Return TRUE if it is.
*/
{
  a_token_cache  cache;
  a_boolean      result = TRUE;

  /* Skip the processing if lambdas are not enabled. */
  if (!lambdas_enabled) {
    result = FALSE;
    goto done;
  }  /* if */
  check_assertion(curr_token == tok_lbracket);
  clear_token_cache(&cache, /*reusable=*/FALSE);
  cache_curr_token(&cache);
  /* Get the token after the "[". */
  (void)get_token();
  if (curr_token == tok_assign || curr_token == tok_ampersand ||
      curr_token == tok_rbracket) {
    /* Something like "[=...", "[&..." or "[]". Treat this as a lambda. */
  } else if (curr_token != tok_identifier) {
    /* After the cases above have been excluded, both lambdas and Microsoft
       attributes should have an identifier next.  If the next token is
       not an identifier treat this as a lambda for error recovery purposes. */
  } else {
    /* The token is an identifier. */
    /* Cache the identifier. */
    cache_curr_token(&cache);
    (void)get_token();
    /* Skip past a comma-separated list of identifiers. */
    while (curr_token == tok_comma) {
      cache_curr_token(&cache);
      (void)get_token();
      if (curr_token != tok_identifier) break;
      cache_curr_token(&cache);
      (void)get_token();
    }  /* while */
    /* Note that next_token() is not called until we've looked at the current
       token.  This is done to avoid caching an unquoted uuid. */
    if ((curr_token == tok_assign || curr_token == tok_ampersand) &&
        next_token() == tok_identifier) {
      /* We encountered "=x" or "&x".  Treat this is a lambda. */
    } else if (curr_token == tok_rbracket) {
      /* "[x]...": If the token after the right bracket is a "{" or "(",
         assume this is a lambda. */
      a_token_kind  next_tok;
      next_tok = next_token();
      if (next_tok != tok_lbrace && next_tok != tok_lparen) result = FALSE;
    } else if (curr_token == tok_colon_colon) {
      /* "[x::...", which cannot be a lambda but could be an attribute. */
      result = FALSE;
    } else {
      /* Something else not handled above.  Assume this to be an attribute. */
      result = FALSE;
    }  /* if */
  }  /* if */
  /* Rescan the tokens cached above. */
  rescan_cached_tokens(&cache);
done:
  return result;
}  /* is_lambda */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_scope_depth decl_level_for_lambda_closure_class(a_boolean  *bad_scope)
/*
A lambda appears in the current scope context.  Return the scope depth at
which the associated closure class should be declared.  Return in *bad_scope
a flag that indicates whether or not the current scope is valid for a
lambda.
*/
{
  a_scope_depth           previous_scope;
  a_scope_stack_entry_ptr ssep;
  a_boolean               scope_error = FALSE;

  for (ssep = scope_stack_entry_for(depth_scope_stack);;
       ssep = scope_stack_entry_for(previous_scope)) {
    previous_scope = ssep->previous_scope;
    switch (ssep->kind) {
      case sck_file:
      case sck_block:
      case sck_namespace:
      case sck_namespace_extension:
      case sck_class_struct_union:
      case sck_condition:
      case sck_function:
      case sck_class_reactivation:
      case sck_namespace_reactivation:
        goto done;
      case sck_instantiation_context:
        /* We might see this while looking for a suitable scope after error
           recovery. */
        check_assertion(scope_error);
        break;
      case sck_template_instantiation:
        /* For a template instantiation scope, keep following the
           previous_scope links in the scope stack.   This will result in
           the closure going in the namespace of the template definition.  This
           case only comes up for lambdas in the initializer of template
           static data members and default arguments of namespace scope
           function templates. */
        break;
      case sck_func_prototype:
        /* If a lambda appears in a function prototype scopes, its closure
           type is treated as if it were defined in the nearest enclosing
           non-function-prototype scope. */
        break;
      case sck_template_declaration:
      case sck_enum:
      default:
        /* We currently don't accept lambdas in template parameter lists nor
           in scoped enum definitions.  Other scopes not covered above are
           unexpected, but it is safe to treat them as errors. */
        scope_error = TRUE;
        break;
    }  /* switch */
  }  /* for */
done:
  if (scope_error) {
    /* An error should have already been issued that a lambda is not allowed
       in a constant expression (which must be the case for the invalid
       scopes. */
    expect_error();
  }  /* if */
  *bad_scope = scope_error;
  return scope_depth_of(ssep);
}  /* decl_level_for_lambda_closure_class */


static void finish_lambda_routine_processing(a_lambda_ptr  *p_lambda)
/*
The given lambda has been completely parsed, and its closure type has been
completed.  Perform any final processing for the closure type's operator()
(notably, IL lowering).
In severe error cases, *p_lambda or *p_lambda->lambda_routine can be NULL:
Set *p_lambda to NULL in such cases.
*/
{
  a_lambda_ptr  lambda = *p_lambda;

  if (lambda != NULL && lambda->lambda_routine != NULL) {
    if (lambda->lambda_routine->assoc_scope != NULL_region_number) {
#if DO_IL_LOWERING
      if (is_primary_translation_unit && 
          should_delay_lowering_on_function(lambda->lambda_routine,
                                            /*at_initial_scope_pop=*/FALSE)) {
        /* Delay lowering of lambdas in some cases (e.g., a lambda could
           be referenced by a template and therefore might have to be
           externalized and it may be too early to create a module id). */
      } else
#endif /* DO_IL_LOWERING */
      /* Do not insert code here. */
      {
        /* Lowering of the lambda body function is deferred because the closure
           class was not complete when the function was scanned.  Now that the
           closure class is complete, do the lowering of the lambda body (if
           needed).  In some cases involving prototype instantiations the
           lambda body may have already been discarded. */
        finish_function_processing_for_memory_region(
                   lambda->lambda_routine->assoc_scope, /*only_inline=*/FALSE);
      }  /* if */
    }  /* if */
  } else {
    /* Severe errors prevented the creation of a call operator.  Don't return
       a lambda. */
    check_assertion(total_errors != 0);
    *p_lambda = NULL;
  }  /* if */
}  /* finish_lambda_routine_processing */


static void scan_lambda_body(a_lambda_ptr       lambda,
                             a_func_info_block  *func_info)
/*
Scan the body of the given lambda (except in some error cases).  If no body is
found, set lambda->lambda_routine to NULL.  *func_info describes some
properties of the call operator with which the lambda body is associated.
The heavy lifting for this routine is performed by scan_function_body.
*/
{
  if (lambda->lambda_routine != NULL) {
    /* Parse the body of the lambda.  A class reactivation is not pushed for
       the lambda closure class because it is still on the scope stack. */
    error_position = pos_curr_token;
    add_stop_token(tok_rbrace);
    if (curr_token != tok_lbrace) {
      /* If a lambda body is missing, set lambda to NULL since the parsed
         construct may not have been meant as a lambda at all. */
      error(ec_missing_lambda_body);
      /* The return type of the routine might be the unknown type.  Set it to
         an error type to avoid surprises (e.g., some IL traversal routines
         expect that no unknown types remain in the IL). */
      lambda->lambda_routine->type->variant.routine.return_type = error_type();
      lambda->lambda_routine = NULL;
    } else {
      a_routine_ptr      rp = lambda->lambda_routine;
      a_decl_flag_set    sfb_flags = SFB_NEW_STRUCT_STMT_STACK_REQUIRED |
                                     SFB_NO_CLASS_REACTIVATION;
      scan_function_body(rp, func_info, sfb_flags);
    }  /* if */
    if (curr_token == tok_rbrace) {
      /* Don't use required_token, because if we aren't at a brace, an error
         has already been issued, and we are at the token to restart parsing
         with. */
      (void)get_token();
    }  /* if */
    remove_stop_token(tok_rbrace);
  }  /* if */
}  /* scan_lambda_body */


a_lambda_ptr scan_lambda(void)
/*
Scan a C++0x lambda construct and return a pointer to an a_lambda entry
describing it.  If errors do not permit the construction of a consistent
entry, return NULL.

The grammar for a lambda expression is as follows:
  
    '[' lambda-capture(opt) ']' lambda-declarator(opt) compound-statement

    lambda-declarator:
        '(' parameter-declaration-clause ')' 'mutable'(opt)
          exception-specification(opt) tailing-return-type(opt)

    trailing-return-type:
        '->' type-id

(parameter-declaration-clause and exception-specification have their counter-
parts in function declarators.  See scan_lambda_capture_list for the grammar
for lambda-capture.)
For example:
    [=, &array](int i)->float { return array[i+k]; }
*/
{
  a_lambda_ptr         lambda = alloc_lambda();
  a_type_ptr           closure_class;
  a_scope_depth        decl_level, saved_decl_scope_level = decl_scope_level;
  a_class_def_state    class_state;
  a_func_info_block    func_info;
  a_member_decl_info   decl_info;
  a_boolean            bad_scope;

  /* Start a new stop token context. */
  push_stop_token_stack();
  check_assertion(curr_token == tok_lbracket);
  lambda->start_position = pos_curr_token;
  /* Initialize the closure class and set up a context in which members
     can be added. */
  decl_level = decl_level_for_lambda_closure_class(&bad_scope);
  decl_scope_level = decl_level;
  lambda->closure_class = closure_class =
                 make_closure_class(decl_level, &lambda->start_position,
                                    bad_scope);
  record_start_of_lambda_header(lambda);
  /* Scan the lambda capture list. */
  (void)get_token();
  add_stop_token(tok_rbracket);
  scan_lambda_capture_list(lambda);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  lambda->capture_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  (void)required_token(tok_rbracket, ec_exp_rbracket);
  remove_stop_token(tok_rbracket);
  /* Parse the "declarator" part of the lambda (the parameter list, etc.). */
  scan_optional_lambda_declarator(lambda, &func_info, &decl_info);
  record_end_of_lambda_header(lambda);
  /* Now that the lambda declarator has been scanned, push the scope stack
     entry for the closure class. */
  push_closure_class(lambda, &class_state);
  /* Declare the call operator for the closure class. */
  decl_call_operator_for_lambda(lambda, &class_state, &decl_info, &func_info);
#if NEED_NAME_MANGLING
  /* When multiple closure types appear in the same scope or context, their
     mangled names are distinguished using a unique number ("discriminator").
     Compute that number now if appropriate (in some contexts, such as
     default arguments, the number will be determined elsewhere).  The notion
     of "discriminator" here is a generalization of the one defined in the
     IA-64 ABI. */
  compute_name_collision_discriminator(symbol_for(closure_class), decl_level);
#endif /* NEED_NAME_MANGLING */
  /* Fill in the capture fields information for the explicit captures. */
  decl_lambda_capture_fields(lambda);
  scan_lambda_body(lambda, &func_info);
  generate_default_constructor(&class_state, /*is_deleted=*/TRUE);
  generate_assignment_operator(&class_state, /*is_deleted=*/TRUE, TQ_CONST);
  /* Record the capture list and complete the closure class. */
  complete_class_definition(closure_class, decl_level, &class_state);
  pop_scope();
  finish_lambda_routine_processing(&lambda);
  /* Restore the previous default declaration scope. */
  decl_scope_level = saved_decl_scope_level;
  /* Restore the previous stop token context. */
  pop_stop_token_stack();
  return lambda;
}  /* scan_lambda */

#if USE_X86_64

static void add_field_to_generated_type(char        *name,
                                        a_type_ptr  type)
/*
A sck_class_struct_union scope is currently on top of the scope stack.  It
is associated with a compiler-generated class type.  Declare a field with the
given name and type in that class.
*/
{
  a_class_def_state_ptr  class_state = scope_stack_top().class_def_state;
  a_symbol_locator       loc;
  a_member_decl_info     decl_info;

  check_assertion(class_state != NULL);
  /* Create a locator. */
  clear_locator(&loc, &null_source_position);
  (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
  /* Declare/create the field by calling decl_nonstatic_data_member. */
  initialize_member_decl_info(&decl_info, &null_source_position);
  decl_info.decl_state.type = type;
  (void)decl_nonstatic_data_member(&loc, class_state, &decl_info,
                                   depth_scope_stack);
}  /* add_field_to_generated_type */


a_type_ptr make_va_list_tag_type(void)
/*
Create and return the __va_list_tag struct type that is predefined by certain
64-bit GCC implementations.  The class is defined as follows:

       struct __va_list_tag {
         unsigned int  gp_offset;
         unsigned int  fp_offset;
         void          *overflow_arg_area;
         void          *reg_save_area;
       };

*/
{
  a_class_def_state              class_state;
  a_symbol_ptr                   sym;
  a_type_ptr                     type, uint_type, voidptr_type;
  a_class_symbol_supplement_ptr  cssp;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean                      saved_source_sequence_entries_disallowed =
                                           source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Don't issue source sequence entries for generated entities. */
  source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Create a struct with name __va_list_tag. */
  type = init_predeclared_class((a_type_kind)tk_struct, "__va_list_tag");
  enter_predeclared_class(type, DEPTH_OF_FILE_SCOPE, &null_source_position);
  sym = symbol_for(type);
  cssp = sym->variant.class_struct_union.extra_info;
  cssp->construction_by_bitwise_copy_allowed = TRUE;
  /* Start the class definition (and associated class scope). */
  initialize_class_def_state(type, &class_state);
  class_state.access = (an_access_specifier)as_public;
  class_type_supp(type)->assoc_scope =
             push_scope((a_scope_kind)sck_class_struct_union, NO_SCOPE_NUMBER,
                        type, (a_routine_ptr)NULL);
  scope_stack_top().class_def_state = &class_state;
  /* Add the fields. */
  uint_type = integer_type((an_integer_kind)ik_unsigned_int);
  add_field_to_generated_type("gp_offset", uint_type);
  add_field_to_generated_type("fp_offset", uint_type);
  voidptr_type = make_pointer_type(void_type());
  add_field_to_generated_type("overflow_arg_area", voidptr_type);
  add_field_to_generated_type("reg_save_area", voidptr_type);
  /* Wrap up the definition. */
  complete_class_definition(type, DEPTH_OF_FILE_SCOPE, &class_state);
  pop_scope();
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Restore the previous state wrt. generating source sequence entries. */
  source_sequence_entries_disallowed =
                                     saved_source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  return type;
}  /* make_va_list_tag_type */

#endif /* USE_X86_64 */

/* Forward declaration for recursive call. */
static void check_type_for_linkage_change(a_type_ptr type,
                                          int        *count);


static void make_routine_externally_linked(a_routine_ptr rp,
                                           int           *count)
/*
Change the linkage of the indicated routine to external.  Count is
a counter of types processed, used in deciding when all types
have been processed.
*/
{
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
      /* Not defined. */
      rp->storage_class = (a_storage_class)sc_extern;
    } else {
      /* Routine is defined in this file.  Mark it referenced in
         case it's referenced in another file. */
      rp->storage_class = (a_storage_class)sc_unspecified;
      rp->source_corresp.referenced = TRUE;
#if MAINTAIN_NEEDED_FLAGS
      mark_as_needed((char *)rp, (an_il_entry_kind)iek_routine);
#endif /* MAINTAIN_NEEDED_FLAGS */
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
#if IA64_ABI && DO_IL_LOWERING
  /* If this is a constructor or destructor with alternate entry points,
     change the linkage on those as well. */
  if (rp->special_kind == (a_special_function_kind)sfk_constructor ||
      rp->special_kind == (a_special_function_kind)sfk_destructor) {
    a_routine_list_entry_ptr rlep;
    for (rlep = rp->variant.ctor_dtor.alternate_entry_points;
         rlep != NULL;
         rlep = rlep->next) {
      make_routine_externally_linked(rlep->routine, count);
    }  /* for */
  }  /* if */
  /* Likewise for any thunks for the routine. */
  { a_routine_ptr rout;
    for (rout = rp->next;
         rout != NULL && rout->overriding_function_for_wrapper == rp;
         rout = rout->next) {
      make_routine_externally_linked(rout, count);
    }  /* for */
  }
#endif /* IA64_ABI && DO_IL_LOWERING */
}  /* make_routine_externally_linked */


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
  if (!type->source_corresp.is_class_member) {
    /* Increment the count.  This lets the caller know how many types (enum
       types and classes) were changed from internal to external linkage and
       permits an early termination of this processing.  Note that the count
       does not include nested types. */
    (*count)++;
  }  /* if */
}  /* make_enum_type_externally_linked */


static void make_class_components_externally_linked(a_type_ptr type,
                                                    int        *count)
/*
This routine changes the linkage of a type's components from internal to
external.  The types it handles directly are class, struct, and union types,
for which it adjusts members as needed, and searches for other classes that
are entailed in its definition and marks them external as well.
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

  if (!type->source_corresp.is_class_member) {
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
      make_routine_externally_linked(rp, count);
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
    begin_template_arg_list_traversal_simple(ctsp->template_arg_list,
                                             &tap);
    for (; tap != NULL; advance_to_next_template_arg_simple(&tap)) {
      switch (tap->kind) {
        case tak_type: tp = tap->variant.type; break;
        case tak_nontype: tp = tap->variant.constant->type; break;
        /* A template template parameter has no type. */
        case tak_template: tp = NULL; break;
        default: unexpected_condition(); break;
      }  /* switch */
      if (tp != NULL) check_type_for_linkage_change(tp, count);
    }  /* for */
  }  /* if */
}  /* make_class_components_externally_linked */


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
  db_enter(4, "make_class_externally_linked");
  /* Mark the class as externally linked immediately, to avoid infinite
     recursion if it is self referential. */
  type->source_corresp.name_linkage =
                                 (a_name_linkage_kind)nlk_cplusplus_external;
  make_class_components_externally_linked(type, count);
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
  check_assertion(is_tag_type(tp));
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
        tp->variant.class_struct_union.is_specialized) {
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
      } else if (class_type_supp(type)->anonymous_union_field != NULL) {
        /* An anonymous union type: It cannot meaningfully have external name
           linkage, but types involved in its definition may need to have their
           name linkage updated. */
        /* Avoid infinite recursion by temporarily giving the anonymous union
           type linkage. */
        a_name_linkage_kind  saved_nlk = type->source_corresp.name_linkage;
        if (saved_nlk != (a_name_linkage_kind)nlk_cplusplus_external) {
          type->source_corresp.name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
          make_class_components_externally_linked(type, count);
          type->source_corresp.name_linkage = saved_nlk;
        }  /* if */
      }  /* if */
      if (type->source_corresp.is_class_member) {
        /* Nested class -- be sure parent class is also externally linked. */
        check_type_for_linkage_change(parent_class_of(type), count);
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
      check_type_for_linkage_change(pm_class_type(type), count);
      check_type_for_linkage_change(pm_member_type(type), count);
      break;
    case tk_integer:
      /* Check for an enum type.  If it's a member of a class, its class
         should be made externally linked, too. */
      if (type->variant.integer.enum_type) {
        if (type->source_corresp.is_class_member) {
          /* Nested enum -- changing the parent's linkage causes the linkage
             of all its nested types to be changed. */
          check_type_for_linkage_change(parent_class_of(type), count);
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
  } else if (extern_inline_allowed && scope->routines != NULL) {
    /* Ordinarily this path is not taken in cfront mode, but if extern inline
       was explicitly specified on the command line, then any member function
       has external linkage. */
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
linkage and to make the change when appropriate.  This processing is
required by the C++ language as described in the ARM, but it is no longer
part of current C++ specification; consequently, this routine is called in
cfront-compatibility mode only.

In cfront mode classes are internally linked (i.e., local to a translation
unit) by default, but they become externally linked for one of two reasons:
either they have members that are external by default, or they are used in a
way that requires external linkage.  To be more specific, if a class has any
noninline member functions or any nonstatic data members it is externally
linked; or, it is used in the declaration of any externally linked class,
variable, or routine it is externally linked (ARM 3.3).

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
  check_assertion(any_cfront_mode());
  /* Search for classes by making a pass over all the types associated with
     the file scope. */
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
      if (!tp->source_corresp.is_local_to_function && is_tag_type(tp)) {
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
        if (vp->source_corresp.name_linkage != (a_name_linkage_kind)nlk_none &&
            vp->source_corresp.name_linkage !=
                                           (a_name_linkage_kind)nlk_internal) {
          /* This is an externally linked variable.  Check its type. */
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
        if (rp->source_corresp.name_linkage != (a_name_linkage_kind)nlk_none &&
            rp->source_corresp.name_linkage !=
                                           (a_name_linkage_kind)nlk_internal) {
          /* This is an externally linked routine.  Check its type. */
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
  db_exit();
}  /* check_class_linkage */


/* ARGSUSED */ /* Neither sym nor stmt is used. */
void define_type_info_pragma(a_pending_pragma_ptr    ppp,
                             a_symbol_ptr            sym,
                             a_statement_ptr         stmt)
/*
Called when a define_type_info pragma is encountered.  Since this pragma
is supposed to be handled in scan_tag_name, any automatic call of this
routine is an error.
*/
{
  pos_error(ec_pragma_may_not_be_used_here, &ppp->id_position);
}  /* define_type_info_pragma */


void class_decl_one_time_init(void)
/*
One-time initialization for class_decl.c static variables.
*/
{
  /* Save variables that are needed for precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_routine_fixup),
      pch_saved_var_array_elem(avail_class_fixup),
      pch_saved_var_array_elem(avail_derivation_steps),
      pch_saved_var_array_elem(avail_override_registry_entries),
      pch_saved_var_array_elem(deferred_friend_fixup_list),
      pch_saved_var_array_elem(deferred_friend_fixup_list_tail),
      pch_saved_var_array_elem(use_deferred_friend_fixup_list),
#if DEBUG
      pch_saved_var_array_elem(num_routine_fixups_allocated),
      pch_saved_var_array_elem(num_class_fixups_allocated),
#endif /* if DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Static variables in class_decl.c. */
  register_trans_unit_variable(avail_derivation_steps);
  register_trans_unit_variable(deferred_friend_fixup_list);
  register_trans_unit_variable(deferred_friend_fixup_list_tail);
  register_trans_unit_variable(use_deferred_friend_fixup_list);
}  /* class_decl_one_time_init */


void class_decl_trans_unit_init(void)
/*
Initializations for class declaration processing that must be done for each
translation unit.
*/
{
  /* Static variables in class_decl.c. */
  curr_routine_fixup = NULL;
  avail_derivation_steps = NULL;
  /* g++ (prior to 3.4) and the Microsoft compiler do not evaluate friend
     functions of template classes until the end of the translation unit, and
     then only if they are referenced. */
  use_deferred_friend_fixup_list = (gpp_mode && gnu_version < 30400) ||
                                    microsoft_mode;
  deferred_friend_fixup_list = NULL;
  deferred_friend_fixup_list_tail = NULL;
}  /* class_decl_trans_unit_init */


void class_decl_init(void)
/*
Initializations for class declaration processing.
*/
{
  /* Static variables in class_decl.c. */
  avail_routine_fixup = NULL;
  avail_class_fixup = NULL;
  avail_override_registry_entries = NULL;
#if IA64_ABI
  avail_covariant_overrides = NULL;
#if DEBUG
  num_covariant_overrides_allocated = 0;
#endif /* DEBUG */
#endif /* IA64_ABI */
#if DEBUG
  num_routine_fixups_allocated = 0;
  num_class_fixups_allocated = 0;
#endif /* DEBUG */
  return;
}  /* class_decl_init */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
