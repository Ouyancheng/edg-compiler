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
#include "layout.h"
#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */


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
} a_routine_fixup;


/*
Structure for keeping track of classes for which fixup processing
must still be done.  The fixups are deferred until the outermost
class definition is complete.
*/
typedef struct a_class_fixup *a_class_fixup_ptr;
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

static a_class_fixup_ptr
		def_arg_class_fixup_list;
			/* Pointer to a list of class fixup entries for
			   class definitions for which default argument
			   fixup must be done. */

static a_class_fixup_ptr
		def_arg_class_fixup_list_tail;
			/* End of the def_arg_class_fixup_list. */

static a_class_fixup_ptr
		inline_function_class_fixup_list;
			/* Pointer to a list of class fixup entries for
			   class definitions for which default argument
			   fixup must be done. */

static a_class_fixup_ptr
		inline_function_class_fixup_list_tail;
			/* End of the inline_function_class_fixup_list. */

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


static void dispose_of_curr_routine_fixup(void)
/*
If the currently active routine fixup entry has been modified such that a
fixup pass over its tokens is required, add it to the routine fixup list for
the current class.  Otherwise free it for later use.
*/
{
  a_symbol_ptr  sym = curr_routine_fixup->symbol;
  a_boolean     needed = FALSE;

  if (sym != NULL && !sym->is_error) {
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
  rfp->func_info.is_movable_member_or_friend_def = TRUE;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  add_to_routine_fixup_list(rfp);
}  /* add_routine_fixup_for_specialization */


void add_routine_fixup_for_template_decl(
		a_symbol_ptr			symbol,
		a_symbol_ptr			prototype_scope_symbols,
		a_type_ptr			class_type,
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
  a_class_fixup_ptr	cfp;

  cfp = alloc_class_fixup();
  cfp->class_type = class_type;
  cfp->is_template_instantiation = is_template_instantiation;
  if (def_arg_class_fixup_list == NULL) def_arg_class_fixup_list = cfp;
  /* Add to the end of the default argument fixup list. */
  if (def_arg_class_fixup_list_tail != NULL) {
    def_arg_class_fixup_list_tail->next = cfp;
  }  /* if */
  def_arg_class_fixup_list_tail = cfp;
  /* Add to the end of the inline function fixup list. */
  if (inline_function_class_fixup_list == NULL) {
    inline_function_class_fixup_list = cfp;
  }  /* if */
  if (inline_function_class_fixup_list_tail != NULL) {
    inline_function_class_fixup_list_tail->next_in_inline_function_list = cfp;
  }  /* if */
  inline_function_class_fixup_list_tail = cfp;
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
  a_bit_field	any_named_fields:1;
			/* TRUE if any named fields are declared. */
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
  a_bit_field	constructor_required:1;
			/* TRUE if the class must have a constructor (either
			   declared by the user or generated by the compiler)
			   because it has virtual base classes, virtual
			   functions, or base classes or members with
			   constructors. */
  a_bit_field	destructor_required:1;
			/* TRUE if the class must have a destructor (either
			   declared by the user or generated by the compiler)
			   because it has base classes or members with
			   destructors. */
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
  cdsp->any_named_fields = FALSE;
  cdsp->any_friend_decls = FALSE;
  cdsp->any_const_or_ref_fields = FALSE;
  cdsp->is_template_instantiation = FALSE;
  cdsp->is_nonreal_instantiation = FALSE;
  cdsp->is_local_class = FALSE;
  cdsp->last_field_is_incomplete_array = FALSE;
  cdsp->constructor_required = FALSE;
  cdsp->destructor_required = FALSE;
  cdsp->access = (an_access_specifier)as_public;
  cdsp->override_registry = NULL;
  cdsp->end_of_field_list = NULL;
  cdsp->corresp_prototype_tag_sym = NULL;
}  /* initialize_class_def_state */


/*
A member-declaration-info block, for tracking information about a class member
declaration as it appears.
*/  
typedef struct a_member_decl_info *a_member_decl_info_ptr;
typedef struct a_member_decl_info {
  a_source_position
		decl_start_pos;
			/* Source position of the first token of the
			   declaration. */
  a_decl_flag_set
		dso_flags;
			/* Bit vector comprising flags returned from
			   decl_specifiers. */
  a_decl_flag_set
		do_flags;
			/* Bit vector comprising flags returned from
			   declarator. */
  a_type_qualifier_set
		qualifiers;
			/* Type qualifiers returned from decl_specifiers. */
  a_decl_modifiers_block
		decl_modifiers;
			/* Decl-modifiers returned from decl_specifiers
			   (Microsoft compatibility mode only). */
  a_decl_pos_block
		decl_pos_block;
			/* Additional source position information on the
			   declaration. */
  a_storage_class
		storage_class;
			/* Storage class returned from decl_specifiers. */
  a_bit_field	is_first_in_declarator_list:1;
			/* TRUE for the first declarator in a declarator list,
			   FALSE thereafter. */
  a_bit_field	is_constructor:1;
			/* TRUE if the current declaration is a constructor.
			   In unusual cases this value may be different from
			   (dso_flags & DSO_CONSTRUCTOR). */
  a_bit_field	is_trivial_default_constructor:1;
			/* TRUE for an implicit declaration of a trivial
			   default constructor. */
  a_bit_field	is_destructor:1;
			/* TRUE if the current declaration is a destructor.
			   In unusual cases this value may be different from
			   (dso_flags & DSO_DESTRUCTOR). */
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
  a_source_sequence_entry_ptr
		declarator_ssep;
#if GENERATE_SOURCE_SEQUENCE_LISTS
			/* Pointer to the source-sequence entry for the
			   declarator. */
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
			/* Always NULL. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_symbol_ptr	member_sym;
			/* Pointer to the symbol entry created to represent
			   the current member; may be NULL. */
} a_member_decl_info;


static void initialize_member_decl_info(a_member_decl_info_ptr mdip,
                                        a_source_position      *pos)
/*
Initialize a member-declaration-info block, used to track information about
a class member declaration as it appears.
*/
{
  mdip->decl_start_pos = *pos;
  mdip->dso_flags = DSO_NO_OUTPUT_FLAGS;
  mdip->do_flags = DO_NO_OUTPUT_FLAGS;
  mdip->qualifiers = TQ_NONE;
  clear_decl_modifiers_block(&mdip->decl_modifiers);
  clear_decl_pos_block(&mdip->decl_pos_block);
  mdip->storage_class = (a_storage_class)sc_unspecified;
  mdip->is_first_in_declarator_list = TRUE;
  mdip->is_constructor = FALSE;
  mdip->is_trivial_default_constructor = FALSE;
  mdip->is_destructor = FALSE;
  mdip->invalid_virtual_specifier = FALSE;
  mdip->is_unnamed_field = FALSE;
  mdip->is_anonymous_union = FALSE;
  mdip->is_nonstd_anonymous_union = FALSE;
  mdip->return_type_def_err = FALSE;
  mdip->is_member_template = FALSE;
  mdip->declarator_ssep = NULL;
  mdip->member_sym = NULL;
}  /* initialize_member_decl_info */


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
					      a_token_cache_ptr decl_cache)
/*
Scan a default argument expression and link the default argument
entry onto a list in the current routine fixup entry.  "ptp" can be NULL if
the tokens should be scanned and discarded.  is_friend is TRUE if the
declaration being scanned is a friend function declaration.
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
  if (decl_cache->first_token == NULL) {
    /* When prescanning a default argument we need to make sure that the
       remainder of the declaration is in a token cache.  If the declaration
       has not been cached yet, cache it now.  This cache will be discarded
       at the end of processing this function declarator. */
    cache_rest_of_declaration(decl_cache, /*stop_on_colon=*/FALSE,
                              /*stop_on_lbrace=*/TRUE);
  }  /* if */
  prescan_default_function_arg_expr(ptp, list, decl_cache,
                                    /*is_function_template=*/FALSE,
				    is_friend);
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


static void default_argument_fixup_for_class(a_type_ptr  class_type,
                                             a_boolean   is_template_based)
/*
Process the default argument expressions for the indicated class.
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
  if ((rfp = cssp->routine_fixup_list) != NULL) {
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
        !class_type->source_corresp.is_local_to_function) {
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
      daefp = rfp->def_arg_expr_fixup_list;
      if (rfp->is_template) {
        /* A routine fixup for a template function declaration.  The default
           arguments have already been attached to the template.  Do the
           prototype instantiations of those default arguments.  This
           is not done for real template instantiations -- they get their
           default information from the information saved during the
           prototype instantiation. */
        if (!is_real_template_instantiation) {
          sym = rfp->symbol;
          if (daefp != NULL && nonclass_prototype_instantiations) {
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
        is_friend = (is_function_symbol(sym) &&
                     (!sym->is_class_member ||
                      sym->parent.class_type != rfp->class_type));
        if (is_nonreal_template_instantiation) {
          /* Prototype instantiation. */
          if (sym->kind == (a_symbol_kind)sk_member_function && !is_friend) {
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
            tssp = symbol_supplement_for_class(rfp->class_type)->template_info;
            while (daefp_tmp != NULL) {
              check_assertion(tssp->cache.decl_info != NULL);
              daefp_tmp->cache.decl_info = tssp->cache.decl_info;
              daefp_tmp = daefp_tmp->next;
            }  /* while */
            if (nonclass_prototype_instantiations) {
              /* Do the prototype instantiations of the default arguments. */
              default_arg_prototype_instantiation(
                           sym, daefp, rfp->func_info.prototype_scope_symbols,
                           /*update_declared_type=*/FALSE);
            }  /* if */
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
            /* Make sure no further processing will be done here and
               make sure that the list isn't freed. */
            daefp = NULL;
            rfp->def_arg_expr_fixup_list = NULL;
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
        if (is_real_template_instantiation &&
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
          } else {
            /* Get the template symbol from the instance pointer. */
            tssp = sym->variant.routine.instance_ptr->template_sym->
                               variant.routine.instance_ptr->template_info;
            check_for_function_template_default_args(
                                           tssp->variant.function.routine,
                                           sym->variant.routine.ptr,
                                           tssp);
          }  /* if */
        } else {
          /* A friend (or other non-member-function) declaration in a real
             template instantiation or any declaration in an ordinary
             (nontemplate) class. */
          /* The function prototype scope should be reactivated and its symbols
             reentered because parameter names hide names from enclosing scopes
             and, moreover, may not be used in default argument expressions
             (ARM 8.2.6). */
          (void)push_scope((a_scope_kind)sck_func_prototype,
                           rfp->func_info.scope_number,
                           underlying_function_type(rfp->symbol),
                           (a_routine_ptr)NULL);
          if (rfp->func_info.prototype_scope_symbols != NULL) {
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
            delayed_scan_of_default_arg_expr(ptp,
                                            /*check_for_errors=*/!is_friend);
          }  /* for */
          /* Restore the prototype scope symbols pointer in the func info
             block. It shouldn't have changed, but we do it to be safe. */
          rfp->func_info.prototype_scope_symbols =
             assoc_pointers_block_of(&scope_stack[depth_scope_stack])->symbols;
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


#if MICROSOFT_EXTENSIONS_ALLOWED

static void defer_routine_fixup_until_use(a_routine_fixup_ptr	rfp)
/*
The Microsoft compiler treats friend functions defined in a class
template much like a member function of such a class.  The body is
only processed if needed.  Save a pointer to the routine fixup entry
in the routine entry.  The fixup will be completed later, if needed.
"rfp" is its routine fixup entry for the definition to be deferred.
*/
{
  rfp->symbol->variant.routine.ptr->routine_fixup = rfp;
}  /* defer_routine_fixup_until_use */


void microsoft_friend_function_fixup(a_routine_fixup_ptr	rfp)
/*
Called in Microsoft mode when a friend function defined in a class template
is first used.  Does the fixup on the friend function that is normally done
when the enclosing class is instantiated.
*/
{
  a_routine_ptr                rp = rfp->symbol->variant.routine.ptr;
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_scope_depth                scope_depth = NO_SCOPE_DEPTH;
  a_source_sequence_entry_ptr  insert_point;
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_enter(3, "microsoft_friend_function_fixup");
  /* Reset the routine fixup pointer in the routine to prevent this
     process from being attempted again. */
  rp->routine_fixup = NULL;
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
  /* Make sure various flags are set correctly in the routine entry.  This
     won't have been done before, since decl_routine was called for a
     declaration, not a definition. */
  rp->defined = TRUE;
  ((a_symbol_ptr)rp->source_corresp.assoc_info)->defined = TRUE;
  if (rp->storage_class == (a_storage_class)sc_extern) {
    rp->storage_class = (a_storage_class)sc_unspecified;
  }  /* if */
  rp->defined_in_friend_decl = TRUE;
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
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  if (scope_depth != NO_SCOPE_DEPTH) {
    scope_stack[scope_depth].ss_list_instantiation_insert_point = insert_point;
  }  /* if */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Pop the reactivated class scope from the scope stack. */
  pop_class_reactivation_scope();
  db_exit();
}  /* microsoft_friend_function_fixup */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

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
          check_assertion(tip != NULL && tip->partial_instantiation != NULL);
          ssep = tip->partial_instantiation;
          check_assertion(scope_depth != NO_SCOPE_DEPTH);
          tip->partial_instantiation = NULL;
          insert_src_seq_list(ssep, ssep, scope_depth, insert_point);
          rfp->symbol->variant.routine.ptr->
                         source_corresp.source_sequence_entry = ssep;
        }  /* if */
      }  /* if */
    }  /* for */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Go through the routine fixup entries to scan inline function bodies. */
    for (rfp = cssp->routine_fixup_list; rfp != NULL; rfp = next_rfp) {
      next_rfp = rfp->next;
      if (rfp->function_body_token_cache.first_token != NULL ||
          (rfp->is_template && rfp->symbol->defined)) {
        sym = rfp->symbol;
#if DEBUG
        if (debug_level >= 3) {
          db_symbol(sym, "scanning function body for ", 2);
        }  /* if */
#endif /* DEBUG */
        is_friend = (!sym->is_class_member ||
                     sym->parent.class_type != rfp->class_type);
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
        if ((is_real_template_instantiation &&
             !is_friend && !rfp->is_specialization)) {
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
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (microsoft_mode &&
                   is_real_template_instantiation &&
                   (is_friend || rfp->is_specialization)) {
          /* The Microsoft compiler treats friend functions defined in a
             class template much like a member function of such a class.
             The body is only processed if needed.  This special treatment
             is also extended to Microsoft mode specializations that are
             defined within the class.  Note that this processing is only
             needed for friends and specializations declared within class
             templates, not for declarations in normal classes. */
          defer_routine_fixup_until_use(rfp);
          /* Set rfp to NULL to prevent it from being freed below. */
          rfp = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else if (rfp->is_template) {
          /* A function template declared in a class scope. */
          if (nonclass_prototype_instantiations) {
            /* Do the prototype instantiation of the function body. */
            function_prototype_instantiation(sym);
            if (is_friend) {
              tssp = template_supplement_for_symbol(sym);
              tssp->variant.function.routine->defined_in_friend_decl = TRUE;
            }  /* if */
          }  /* if */
        } else if (is_nonreal_template_instantiation &&
                   !scope_stack[depth_scope_stack].inside_local_class &&
                   !is_friend && !rfp->is_specialization) {
          /* Prototype instantiation -- copy the cache for member functions.
             (Note that member functions of local classes of a function
             prototype instantiation are nonreal, but they are not themselves
             prototype instantiations.) */
          tssp = template_supplement_for_symbol(sym);
          tssp->cache.tokens = rfp->function_body_token_cache;
          clear_token_cache(&rfp->function_body_token_cache,
                           /*reusable=*/TRUE);
          /* Also copy the func_info block.  Null out the param-id pointer
             in the fixup entry so that the list won't be freed when
             free_routine_fixup is called. */
          tssp->variant.function.func_info = rfp->func_info;
          rfp->func_info.param_id_list = NULL;
          if (nonclass_prototype_instantiations) {
            /* Do the prototype instantiation of the member function body. */
            function_prototype_instantiation(sym);
          }  /* if */
        } else {
          /* Normal case. */
          a_routine_ptr  rp = rfp->symbol->variant.routine.ptr;

          if (rp->storage_class == (a_storage_class)sc_extern) {
            rp->storage_class = (a_storage_class)sc_unspecified;
          }  /* if */
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
          if (rfp->func_info.is_movable_member_or_friend_def &&
              !source_sequence_entries_disallowed) {
            /* Within the class definition a secondary-decl source sequence
               entry was put out for the member or friend function
               definition.  The primary source sequence entry was deferred
               till now, when the class definition is complete. */
            a_source_sequence_entry_ptr  ssep;
#if CHECKING
            if (!sym->is_error) {
              ssep = rp->source_corresp.source_sequence_entry;
              check_assertion(ss_entry_kind(ssep) ==
                                (an_il_entry_kind)iek_src_seq_secondary_decl);
            }  /* if */
#endif /* CHECKING */
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
          }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
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
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
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


static void process_deferred_class_fixups(void)
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
*/
{
  a_class_fixup_ptr  cfp;
  a_class_fixup_ptr  next_cfp;

  db_enter(3, "process_deferred_class_fixups");
  if (def_arg_class_fixup_list != NULL ||
      inline_function_class_fixup_list != NULL) {
    /* Clear the pointers to the start of the fixup lists so that classes
       created by the fixup process can be fixed up by a recursive call to
       this routine.  This could happen if a function body contains a
       nested class, for example. */
    cfp = def_arg_class_fixup_list;
    def_arg_class_fixup_list = NULL;
    def_arg_class_fixup_list_tail = NULL;
    defer_inline_function_fixup_and_instantiations++;
    for (; cfp != NULL; cfp = cfp->next) {
      default_argument_fixup_for_class(cfp->class_type,
                                       cfp->is_template_instantiation);
    }  /* for */
    defer_inline_function_fixup_and_instantiations--;
    if (defer_inline_function_fixup_and_instantiations == 0) {
      cfp = inline_function_class_fixup_list;
      inline_function_class_fixup_list = NULL;
      inline_function_class_fixup_list_tail = NULL;
      for (; cfp != NULL; cfp = next_cfp) {
        inline_function_fixup_for_class(cfp->class_type,
                                        cfp->is_template_instantiation);
        next_cfp = cfp->next_in_inline_function_list;
        free_class_fixup(cfp);
      }  /* for */
    }  /* if */
  }  /* if */
  db_exit();
}  /* process_deferred_class_fixups */


void process_deferred_class_fixups_and_instantiations(void)
/*
While one or more class definitions are pending, the fixup of member function
bodies and default arguments is deferred until all class definitions have
been complete.  Nonclass template definitions are also deferred.  When the
count of pending class definitions is zero, all class definitions have been
completed and any deferred class fixups and instantiations may now be done.
*/
{
  if (pending_class_definitions == 0) {
    process_deferred_class_fixups();
    if (defer_inline_function_fixup_and_instantiations == 0) {
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


void report_abstract_class_error(an_error_code      error_code,
                                 a_type_ptr         class_type,
                                 a_source_position  *error_pos)
/*
Issue an error (using the message specified by error_code) on an incorrect
use of an object of abstract class type, as indicated by class_type.
*error_pos is the source position at which the error should be issued.
The diagnostic includes a list of pure virtual functions, to assist the
user in correcting the class declarations that produced the problem.
*/
{
  a_boolean  found = FALSE;

  class_type = skip_typerefs(class_type);
  pos_ty_start_error(error_code, error_pos, class_type);
  /* Put out the list of pure virtual functions. */
  report_pure_virtual_functions(class_type, (a_base_class_ptr)NULL,
                                ec_pure_virtual_function, &found);
#if CHECKING
  /* If class_type is marked as abstract, at least one pure virtual function
     should have been found. */
  check_assertion(found);
#endif /* if */
  /* Terminate the supplementary messages. */
  end_error();
}  /* report_abstract_class_error */


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
        new_ovfp_base_class = find_direct_base_class_of(new_class,
                                                        old_class);
      } else {
        /* Be sure to select the right base class (its type could appear
           multiple times in the object hierarchy). */
        a_base_class_ptr  disambiguator = NULL;
        if (!new_bcp->is_virtual) {
          /* If the base class is virtual, no disambiguator is needed.
             Otherwise, there is only one derivation path and we can search
             for a disambiguator along that path. */
          disambiguator = find_disambiguator(
                                        new_bcp->derivation->path->base_class,
                                        ovfp_to_copy->base_class);
        }  /* if */
        new_ovfp_base_class = 
                 corresponding_base_class(ovfp_to_copy->base_class, new_class,
                                          disambiguator);
      }  /* if */
      if (check_new_list) {
        for (ovfp_from_new_list = new_bcp->overriding_virtual_functions;
             ovfp_from_new_list != NULL;
             ovfp_from_new_list = ovfp_from_new_list->next) {
          if (ovfp_from_new_list->primary_function ==
                                            ovfp_to_copy->primary_function) {
            if (ovfp_from_new_list->overriding_function ==
                                         ovfp_to_copy->overriding_function) {
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
                                      a_base_class_ptr  base_class,
                                      a_routine_ptr     primary_func,
                                      a_routine_ptr     overriding_func,
                                      a_base_class_ptr  return_adjustment_bcp)
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
  db_exit();
}  /* record_virtual_function_override */


#if !ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
/*ARGSUSED*/ /* class_type is used only to support covariant return types. */
#endif /* !ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
static a_boolean return_types_are_override_compatible(
                                 a_type_ptr       type_of_overriding_routine,
                                 a_type_ptr       type_of_overridden_routine,
                                 a_base_class_ptr *return_adjustment_bcp)
/*
Given the routine types of overriding and overridden virtual functions,
return TRUE if the return types are identical or "covariant" (WP 10.3).
Covariance means both return types are references or pointers to class types
that are related by derivation, where the class associated with the overridden
function is a base class of the class associated with the overriding function.
When covariance is detected, return in *return_adjustment_bcp the base class
entry for the class associated with the overridden function.
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
    if ((is_reference_type(tp1) && is_reference_type(tp2)) ||
        (is_pointer_type(tp1) && is_pointer_type(tp2) &&
         type_qualifiers_match(tp1, tp2))
#ifdef pointer_types_have_same_repr
        && pointer_types_have_same_repr(tp1, tp2)
#endif /* ifdef pointer_types_have_same_repr */
                                                 ) {
      /* Both types are references or both are pointers with identical type
         qualifiers on top of the pointer type.  Now check the types pointed
         to. */
      tp1 = type_pointed_to(tp1);
      tp2 = type_pointed_to(tp2);
      if (is_class_struct_union_type(tp1) && is_class_struct_union_type(tp2)) {
        /* The types referenced/pointed to are both classes. */
        if (!any_qualifier_in_set_missing(get_type_qualifiers(tp2),
                                          get_type_qualifiers(tp1))) {
          /* The cv-qualification on the class of the overriding function's
             return type (tp1) is equal to or less than the cv-qualification
             on the class of the overridden function's return type (tp2). */
          tp1 = skip_typerefs(tp1);
          tp2 = skip_typerefs(tp2);
          /* Next see if the class associated with the overridden function
             is the same as or a base class of the class associated with the
             overriding function. */
          if (identical_types(tp1, tp2)) {
            /* The class types are the same. */
            compatible = TRUE;
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
          } else {
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
  a_base_class_ptr  virtual_function_info_base_class, bcp, disambiguator;

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
      /* bcp is the base class of base_class->type with which the latter
         shares its virtual function info.  Find out what the corresponding
         base class of class_type is. */
      disambiguator = find_disambiguator(base_class, bcp);
      /* bcp now points to a base class of base_class; change it to point
         to the corresponding base class of class_type. */
      bcp = corresponding_base_class(bcp, class_type, disambiguator);
      if (virtual_function_info_base_class == bcp) {
        shares = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return shares;
}  /* shares_virtual_function_info */


static void update_override_registry(
                             an_override_registry_entry_ptr *registry_ptr,
                             a_symbol_ptr                   overridden_sym,
                             a_symbol_ptr                   nonoverriding_sym,
                             a_base_class_ptr               bcp)
/*
A declaration in the current derived class has been seen, and it has the
effect of overriding a virtual function from a base class.  But the latter is
a member of an overload set (represented by overload_sym).  Keep track of the
number of overrides by updating the linked list pointed to by registry_ptr.
When all the virtual functions in the overload set have been overridden, the
corresponding entry is removed from the registry.
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

      if (((a_symbol_ptr)routine->source_corresp.assoc_info)->header ==
                                                                     header &&
          orep->base_class->type == udecl->qualifier.class_type) {
        result = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* base_function_unhidden_by_projection */


static void check_override_registry(an_override_registry_entry_ptr  first_orep,
                                    a_symbol_ptr                    tag_sym)
/*
orep is the first entry in the "override-registry" for the class associated
with tag_sym.  Traverse the linked list, checking each entry for conditions
that would warrant a warning.  There are two cases: when the overridden
function is an overload set in a base class and one or more virtual functions
was overridden by a declaration in the current class and one or more was not
overridden; though allowed, this could produce subtle inconsistencies in a
user program, so issue a warning.  The other case is where a declaration in
the derived class might have been intended to override a base class virtual
function, but didn't.  Again, it's perfectly legal, but it *might* have been
a mistake.  Both these warnings should perhaps be remarks.
*/
{
  an_override_registry_entry_ptr  orep = first_orep, next_orep;

  /* Loop through the registry of overrides. */
  for (; orep != NULL; orep = next_orep) {
    if (orep->override_count < orep->virtual_function_count) {
      if (orep->virtual_function_count > 1 && orep->override_count > 0) {
        /* Issue a diagnostic on partial override of an overloaded
           virtual function. */
        if (base_function_unhidden_by_projection(tag_sym, orep)) {
          /* The partial overriding is mitigated by having the nonoverridden
             declarations projected through a using-declaration. */
          goto next;
        } else {
          pos_sy2_warning(ec_partial_override, &tag_sym->decl_position,
                          orep->overridden_sym, tag_sym);
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
          if (orep->override_failures) {
            /* No need to issue any more diagnostics on this name. */
            remove_name_from_override_registry(orep);
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      check_assertion(orep->override_count == orep->virtual_function_count ||
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
Update the virtual function number of the virtual member function pointed to
by rp.  number_ptr is the address of the field in a_class_type_supplement
that tracks the highest number assigned thus far.
*/
{
  if (*number_ptr == MAX_VIRTUAL_FUNCTIONS_PER_CLASS) {
    a_type_ptr  parent_class = rp->source_corresp.parent.class_type;
    if (parent_class->variant.class_struct_union.is_nonreal_class) {
      /* Don't issue an error, since the number may not be maintained
         accurately for nonreal class instantiations. */
    } else {
      pos_error(ec_too_many_virtual_functions,
                &rp->source_corresp.decl_position);
    }  /* if */
    /* Reset to zero, to avoid more such messages. */
    *number_ptr = 0;
  }  /* if */
  /* Increment the number for the virtual functions declared so far in the
     current class and enter it in the routine entry.  It is used by the
     front end in managing virtual function override entries and can be used
     by the back end for indexing into a virtual function table. */
  rp->virtual_function_number = ++(*number_ptr);
}  /* update_virtual_function_number */


static a_boolean check_for_virtual_function(
                                     a_boolean             virtual_specified,
                                     a_symbol_ptr          rout_sym,
                                     a_type_ptr            class_type,
                                     a_class_def_state_ptr class_state,
                                     a_source_position     *source_pos)
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
  a_boolean                       is_virtual = virtual_specified;
  a_boolean                       overloaded;
  a_base_class_ptr                bcp;
  a_symbol_ptr                    symbol_list, sym, sym_next;
  a_symbol_ptr                    sym_for_override_registry;
  a_routine_ptr                   rout, rp;
  a_scope_ptr                     base_class_scope;
  a_virtual_function_number       virtual_function_number = 0;
  a_boolean                       any_override_candidates = FALSE;
  an_override_registry_entry_ptr  *registry_ptr;
  a_base_class_ptr                return_adjustment_bcp;

  db_enter(4, "check_for_virtual_function");
  check_assertion(rout_sym->kind == (a_symbol_kind)sk_member_function);
  rout = rout_sym->variant.routine.ptr;
  registry_ptr = &class_state->override_registry;
  /* We scan symbols on the inactive list, since we are only interested in
     functions declared in base classes. */
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
          if (exception_spec_is_less_restrictive(rout->type,
                                                 rp->type)) {
            /* The exception specification for the overriding virtual function
               is less restrictive that that of the overridden function. */
            if (rout->compiler_generated) {
              /* Issue a warning on a compiler-generated destructor. */
              pos_sy2_warning(ec_generated_exception_spec_override_incompat,
                              source_pos, rout_sym, sym);
            } else {
              pos_sy2_diagnostic(es_discretionary_error,
                                 ec_exception_spec_override_incompat,
                                 source_pos, rout_sym, sym);
            }  /* if */
          }  /* if */
          record_virtual_function_override(bcp, rp, rout,
                                           (a_base_class_ptr)NULL);
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
             for overloaded functions.  This is a do-while loop instead of a
             for loop because we can be sure of the initial conditions on the
             first iteration. */
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
            if (microsoft_bugs && remove_qualifiers_from_param_types) {
              /* For the Microsoft compiler the functions may still not
                 match if the top-level type qualifiers on the parameters
                 (yes, the ones that have been stripped off) do not match. */
              if (!param_types_are_compatible(rout->type, rp->type,
                                     TCF_DONT_IGNORE_PARAM_TYPE_QUALIFIERS)) {
                /* Keep looking for another match in the current overload
                   set. */
                continue;
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
                                                     &return_adjustment_bcp)) {
              /* Error -- return type must be identical to or covariant
                 with that of the overridden function. */
              an_error_code  error_code =
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
                        ec_bad_return_type_on_virtual_function_override;
#else /* !ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
                        ec_different_return_type_on_virtual_function_override;
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
              pos_syty_error(error_code, source_pos, sym,
                             skip_typerefs(rp->type)->
                                             variant.routine.return_type);
              /* Since, except for the return types, there is a match, there
                 is no need to look any further in the current base class.
                 Advance to the next base class. */
              goto next_base_class;                                       
            }  /* if */
            /* Match */
            is_virtual = TRUE;
            if (exception_spec_is_less_restrictive(rout->type, rp->type)) {
              /* The exception specification for the overriding virtual
                 function is less restrictive that that of the overridden
                 function. */
              if (rout->compiler_generated) {
                /* Issue a warning on a compiler-generated constructor
                   or assignment operator. */
                pos_sy2_warning(ec_generated_exception_spec_override_incompat,
                                source_pos, rout_sym, sym);
              } else {
                pos_sy2_diagnostic(es_discretionary_error,
                                   ec_exception_spec_override_incompat,
                                   source_pos, rout_sym, sym);
              }  /* if */
            }  /* if */
            /* Record the virtual function override in the base class entry.
               It can be used later, e.g., for building a virtual function
               table. */
            record_virtual_function_override(bcp, rp, rout,
                                             return_adjustment_bcp);
            if (return_adjustment_bcp != NULL) {
              /* The overriding function has a covariant return type.
                 Set a flag, since some extra processing may be needed
                 later. */
              rout->covariant_return_virtual_override = TRUE;
            } else if (shares_virtual_function_info(class_type, bcp)) {
              /* The virtual function table is being shared and there
                 is no base-class adjustment on the return type, so we
                 can use the same virtual function number. */
              virtual_function_number = rp->virtual_function_number;
            }  /* if */
            /* If this declaration amounts to an override of a member of an
               overload set, record some information about it in the
               partial-override-registry.  This allows for a diagnostic later
               if the rest of the members are not also overridden. */
            if (!rout->compiler_generated) {
              update_override_registry(registry_ptr, sym_for_override_registry,
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
      rout->virtual_function_number = virtual_function_number;
    } else {
#if ABI_COMPATIBILITY_VERSION >= 232
      /* For more current ABIs the virtual function numbers are updated after
         all routine declarations for the current class have been processed.
         This way all the functions in an overload set can be grouped
         together, providing better cfront object layout compatibility. */
#else /* ABI_COMPATIBILITY_VERSION <  232 */
      /* Don't try to do overload-set grouping -- use the declaration order
         instead.  Update the routine entry with the next available virtual
         function number. */
      a_virtual_function_number  *number_ptr;
      number_ptr = &class_type->variant.class_struct_union.extra_info->
                                          highest_virtual_function_number;
      update_virtual_function_number(rout, number_ptr);
#endif /* ABI_COMPATIBILITY_VERSION >= 232 */
    }  /* if */
  }  /* if */
  db_exit();
  return is_virtual;
}  /* check_for_virtual_function */

#if ABI_COMPATIBILITY_VERSION >= 232

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
  a_routine_ptr  rp;

  for (; sym != NULL; sym = sym->next) {
    if (sym->kind == (a_symbol_kind)sk_member_function) {
      rp = sym->variant.routine.ptr;
      if (rp->is_virtual) {
        if (rp->virtual_function_number != 0) {
          /* The routine already has a virtual function number assigned.
             This occurs when the virtual base class is being shared between
             the current class and one of its base classes. */
        } else {
          /* If there are additional functions in the overload set, process
             them first.  This is because the symbols on the list are in
             the opposite order to that in which they were declared. */
          set_virtual_function_numbers_for_overload_set(sym->next, number_ptr);
          /* Update the routine entry with the next available virtual
             function number. */
          update_virtual_function_number(rp, number_ptr);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* set_virtual_function_numbers_for_overload_set */


static void set_virtual_function_numbers(a_type_ptr  class_type)
/*
Traverse the scope-list of symbols for class_type, updating the virtual
function numbers of the virtual functions.
*/
{
  a_virtual_function_number  *number_ptr;
  a_symbol_ptr               sym;
  a_routine_ptr              rp;

  if (class_type->variant.class_struct_union.any_virtual_functions) {
    /* We will pass in the address of the field that tracks the highest
       virtual function that has been assigned thus far.  (It may be nonzero
       at this point if the virtual function info for this class is shared
       with that of one of its base classes.) */
    number_ptr = &class_type->variant.class_struct_union.extra_info->
                                         highest_virtual_function_number;
    /* Traverse the symbol list rather that the IL scope's function list.
       Both should reflect declaration order except in the handling of
       overloaded functions.  We do want to handle members of an overload set
       as a group. */
    for (sym = symbol_supplement_for_class(class_type)->symbols;
         sym != NULL;
         sym = sym->next_in_scope) {
      if (sym->kind == (a_symbol_kind)sk_overloaded_function) {
        /* Examine each of the members of the overload set. */
        set_virtual_function_numbers_for_overload_set(
                                     sym->variant.overloaded_function.symbols, 
                                     number_ptr);
      } else if (sym->kind == (a_symbol_kind)sk_member_function) {
        rp = sym->variant.routine.ptr;
        if (rp->is_virtual) {
          /* The member function is virtual. */
          if (rp->virtual_function_number != 0) {
            /* The routine already has a virtual-function number assigned.
               This occurs when the virtual base class is being shared between
               the current class and one of its base classes. */
          } else {
            /* Update the routine entry with the next available virtual
               function number. */
            update_virtual_function_number(rp, number_ptr);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* set_virtual_function_numbers */

#endif /* ABI_COMPATIBILITY_VERSION >= 232 */

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
        if (dsp->base_class->is_virtual) {
          fprintf(f_debug, "(ptr @%lu)",
                  (unsigned long)dsp->base_class->pointer_offset);
        }  /* if */
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
    if (show_offset) {
      fprintf(f_debug, " (ptr offset = %lu",
              (unsigned long)bcp->pointer_offset);
      if (bcp->pointer_base_class != NULL) {
        fputs(", in ", f_debug);
        db_type_name(bcp->pointer_base_class->type);
      }  /* if */
      fputc(')', f_debug);
    }  /* if */
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
  a_base_class_ptr       bcp, disambiguator;
  a_derivation_step_ptr  step;

  db_enter(4, "set_shares_virtual_function_info_flag");
  if (base_class == NULL) {
    /* Set the flag, if appropriate, based on the properties of the class. */
    tp = class_type;
  } else {
    /* Set the flag, if appropriate, based on the properties of the base
       class. */
    tp = base_class->type;
  }  /* if */
  if (tp->variant.class_struct_union.any_virtual_functions) {
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
        disambiguator = find_disambiguator(base_class, bcp);
        bcp = corresponding_base_class(bcp, class_type, disambiguator);
      }  /* if */
      /* Set the flag. */
      bcp->shares_virtual_function_info = TRUE;
      /* It may be that bcp is not a direct base class of the type (class type
         or base class type), in which case it may be that flag has to be set
         on an intervening base class as well. */
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
    }  /* if */
  }  /* if */
  db_exit();
}  /* set_shares_virtual_function_info_flags */


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
  }  /* if */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
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
    a_base_class_ptr             disambiguator, fixup_bcp;
    a_base_class_derivation_ptr  bcdp;

    for (bcp = base_classes_of(new_bcp->type); bcp != NULL; bcp = bcp->next) {
      if (bcp->direct && bcp->is_virtual) {
        bcdp = bcp->derivation;
        if (!bcdp->direct) {
          /* Add path information about a direct virtual base class of
             new_direct_bcp that was not first in the depth-first
             left-to-right traversal of the latter's derivation graph. */
          /* Find the base class in new_class that corresponds to bcp. */
          disambiguator = find_disambiguator(new_bcp, bcp);
          fixup_bcp = corresponding_base_class(bcp, new_class, disambiguator);
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
  a_base_class_derivation_ptr  bcdp, preferred_bcdp;
  an_access_specifier          access, preferred_access;

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


static void mark_dependent_base_classes(a_type_ptr		class_type,
					a_class_def_state_ptr	class_state)
/*
Determine which of the base classes of class_type are template-dependent,
and so should not be visible for certain lookups.

For a prototype instantiation, we go through the base classes of the
prototype type and determine whether the base class depends on a
template parameter.  It is marked accordingly.  For real classes,
we check the flag previously set for the corresponding base class of
the prototype instantiation.

This routine is only called for generated instantiations, not for normal
classes or explicitly specialized classes.
*/
{
  a_class_type_supplement_ptr	ctsp;
  a_base_class_ptr		bcp;

  ctsp = class_type->variant.class_struct_union.extra_info;
  if (class_state->is_nonreal_instantiation) {
    /* The class is a prototype instantiation.  If a direct base class
       depends on a template parameter it should be ignored for unqualified
       lookups. */
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      if (bcp->direct) {
        bcp->ignore_during_dependent_lookup =
                                      is_or_contains_template_param(bcp->type);
      }  /* if */
    }  /* for */
  } else {
    a_base_class_ptr		proto_bcp;
    a_type_ptr			proto_type;
    a_class_type_supplement_ptr	proto_ctsp;
    a_symbol_ptr		proto_sym;

    proto_sym = class_state->corresp_prototype_tag_sym;
    check_assertion_str2(proto_sym != NULL,
                         "mark_dependent_base_classes:",
                         "no corresp_prototype_tag_sym");
    proto_type = proto_sym->variant.class_struct_union.type;
    proto_ctsp = proto_type->variant.class_struct_union.extra_info;
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      /* Only process direct base classes. */
      if (bcp->direct) {
        /* Find the corresponding prototype base class.  Note that in
           error cases a given sequence number could be missing from
           either list.  Also, when dealing with virtual bases, the
           order in which the base classes appear can differ from the
           prototype instantiation to the real instantiation (e.g.,
           a template-dependent base class (whose base classes are
           unknown) could have a virtual base that is the same as a
           direct virtual base of the current class).  Consequently,
           we search from the beginning of the list for each base
           class. */
        proto_bcp = proto_ctsp->base_classes;
        while (proto_bcp != NULL &&
               (!proto_bcp->direct ||
                proto_bcp->direct_base_number != bcp->direct_base_number)) {
          proto_bcp = proto_bcp->next;
        }  /* while */
        if (proto_bcp != NULL &&
            proto_bcp->direct_base_number == bcp->direct_base_number) {
          /* Skip a base class if we did not find a correspondence.  In an
             error case, this could result in names from a base class being
             visible when they really shouldn't be. */
          bcp->ignore_during_dependent_lookup =
                                     proto_bcp->ignore_during_dependent_lookup;
        } else {
          /* If we did not find a matching base class there must have been
             an earlier error. */
          check_assertion(total_errors != 0);
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* mark_dependent_base_classes */


static void scan_base_specifier_list(a_type_ptr             type_ptr,
                                     a_class_def_state_ptr  class_state)
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
  a_base_class_ptr              new_direct_bcp, disambiguator;
  an_access_specifier           access;
  a_boolean                     is_virtual;
  a_boolean                     access_already_specified;
  char                          *default_access_str;
  a_symbol_ptr                  sym;
  a_type_ptr                    base_class_type;
  a_boolean                     ambiguous;
  a_class_symbol_supplement_ptr cssp, bcp_cssp;
  a_boolean                     any_base_class_fixup_required;
  a_boolean                     first_direct_nonvirtual_base_class = TRUE;
  a_source_position             base_class_decl_pos;
  a_source_position             base_specifier_start_pos;
  a_derivation_step_ptr         path;
  a_boolean                     first_base_class = TRUE;
  a_base_class_sequence_number	direct_base_number = 0;

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
    base_specifier_start_pos = pos_curr_token;
    direct_base_number++;
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
          if (curr_token == tok_public) {
            access = (an_access_specifier)as_public;
          } else if (curr_token == tok_protected) {
            access = (an_access_specifier)as_protected;
          } else {
            access = (an_access_specifier)as_private;
          }  /* if */
          access_already_specified = TRUE;
        }  /* if */
      } else {
        /* Leave the loop and scan the class name. */
        break;
      }  /* if */
      (void)get_token();
    }  /* for */
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
      /* Be sure a type symbol was found and that it identifies a class. */
      if (sym == NULL || !is_class_symbol(sym)) {
        /* Not a class symbol.  In most cases, issue and error and skip it.
           When a template param is involved, just skip it. */
        if (sym != NULL && sym->kind == (a_symbol_kind)sk_type) {
          a_type_ptr  tp = skip_typedefs(type_symbol_type(sym));
          if (tp->kind == (a_type_kind)tk_template_param) {
            if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
              /* No diagnostic on template parameters, which will only show
                 up during prototype instantiations.  Set the flag that
                 indicates that this prototype instantiation has a nonreal
                 base class. */
              cssp->any_nonreal_base_classes = TRUE;
              base_class_type = proxy_class_for_template_param(tp);
              bcp_cssp = symbol_supplement_for_class(base_class_type);
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
      if (base_class_type == NULL) {
        /* Get the type entry for the base class name. */
        base_class_type = type_symbol_type(sym);
        bcp_cssp = symbol_supplement_for_class(base_class_type);
        base_class_type->source_corresp.referenced = TRUE;
        /* If it is a const or volatile qualified type name (where in the
           ARM is this required?) or if it is the class now being defined or
           if it is a union or if it has been declared but not yet defined
           (ARM 10, p. 196), issue an error and skip over this class: it is
           not a valid base class name. */
        /* In Microsoft mode the last field of a class may be a zero-length
           array; such a class may not be a base class. */
        if (is_qualified_type(base_class_type) ||
            (base_class_type = skip_typerefs(base_class_type)) == type_ptr ||
            base_class_type->kind == (a_type_kind)tk_union ||
            base_class_type->
                 variant.class_struct_union.contains_flexible_array_member) {
          error(ec_bad_base_class);
          goto skip_base_class;
        } else {
          /* Force instantiation if the base class is a template class. */
          check_assertion(is_class_struct_union_type(base_class_type));
          complete_class_type_is_needed(base_class_type);
          if (is_incomplete_type(base_class_type)) {
            error(ec_incomplete_type_not_allowed);
            goto skip_base_class;
          }  /* if */
        }  /* if */
      }  /* if */
      /* Issue a diagnostic if an explicit access specifier was not provided
         (as per the recommendation on p. 243 of the ARM). */
      if (!access_already_specified) {
        pos_st_remark(ec_missing_access_specifier, &error_position,
                      default_access_str);
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
            bcp->direct_base_number = direct_base_number;
            bcp->decl_position = base_class_decl_pos;
#if EXTRA_SOURCE_POSITIONS_IN_IL
            bcp->base_specifier_range.start = base_specifier_start_pos;
            bcp->base_specifier_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
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
      if (is_virtual || bcp_cssp->constructor != NULL) {
        class_state->constructor_required = TRUE;
      }  /* if */
      if (bcp_cssp->destructor != NULL) {
        class_state->destructor_required = TRUE;
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
      if (bcp_cssp->any_nonreal_base_classes ||
          base_class_type->variant.class_struct_union.is_nonreal_class) {
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
      if (base_class_type->variant.class_struct_union.any_mutable_member) {
        type_ptr->variant.class_struct_union.any_mutable_member = TRUE;
      }  /* if */
      /* Now create the new base class entry and add it to the end of the
         base classes list. */
      new_direct_bcp = alloc_base_class();
      new_direct_bcp->type = base_class_type;
      new_direct_bcp->derived_class = type_ptr;
      new_direct_bcp->decl_position = base_class_decl_pos;
      new_direct_bcp->direct = TRUE;
      new_direct_bcp->ambiguous = ambiguous;
      new_direct_bcp->direct_base_number = direct_base_number;
      if (is_virtual) new_direct_bcp->is_virtual = TRUE;
      path = update_base_class_derivation(new_direct_bcp,
                                          (a_derivation_step_ptr)NULL, access);
#if DEBUG
      if (debug_level >= 3 || db_flag_is_set("base_specifiers")) {
        db_abbreviated_type(base_class_type);
        fputs(" is direct base class of ", f_debug);
        db_abbreviated_type(type_ptr);
        fputc('\n', f_debug);
      }  /* if */
#endif /* DEBUG */
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
#if EXTRA_SOURCE_POSITIONS_IN_IL
      new_direct_bcp->base_specifier_range.start = base_specifier_start_pos;
      new_direct_bcp->base_specifier_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Offset is updated in merge_field_lists. */
      new_direct_bcp->offset = 0;
      /* Add base classes derived from this base class to the current class's
         base class list.  They are marked as indirect. */
      any_base_class_fixup_required = FALSE;
      for (bcp = base_classes_of(new_direct_bcp->type);
           bcp != NULL;
           bcp = bcp->next) {
        if (bcp->overriding_virtual_functions != NULL) {
          any_base_class_fixup_required = TRUE;
        }  /* if */
        if (!bcp->direct) {
          continue;
        } else if (bcp->is_virtual) {
          /* A virtual base class is marked as "direct" if any of its paths
             is direct.  However, for our purposes, the "first" path (first in
             a depth-first left-to-right traversal of the derivation graph)
             must be direct.  On the other hand, we still need to record the
             direct path, but so set a flag so another pass will be done over
             the base class list. */
          if (!bcp->derivation->direct) {
            any_base_class_fixup_required = TRUE;
            continue;
          }  /* if */
        }  /* if */
          /* Add the direct base class and all *its* base classes to the
           base class list for the derived class. */
        add_indirect_base_class(bcp, new_direct_bcp, path,
                                &end_of_base_classes_list, type_ptr);
      }  /* for */
      /* Enter the base name on the base class list in the derived class's
         class-supplement entry. */
      if (end_of_base_classes_list == NULL) {
        ctsp->base_classes = new_direct_bcp;
      } else {
        end_of_base_classes_list->next = new_direct_bcp;
      }  /* if */
      end_of_base_classes_list = new_direct_bcp;
      /* Set shares_virtual_function_info for a base class of new_direct_bcp,
         if appropriate. */
      set_shares_virtual_function_info_flag(type_ptr, new_direct_bcp);
      if (any_base_class_fixup_required) {
        for (bcp = base_classes_of(new_direct_bcp->type);
             bcp != NULL;
             bcp = bcp->next) {
          if (bcp->overriding_virtual_functions != NULL ||
              (bcp->direct && bcp->is_virtual && !bcp->derivation->direct)) {
            /* bcp is a base class of new_direct_bcp->type.  We need to find
               the corresponding base class of type_ptr.  Find a disambiguator
               in case what we are looking for is an ambiguous base class of
               type_ptr. */
            disambiguator = find_disambiguator(new_direct_bcp, bcp);
            new_bcp = corresponding_base_class(bcp, type_ptr, disambiguator);
          } else {
            continue;
          }  /* if */
          if (bcp->direct && bcp->is_virtual && !bcp->derivation->direct) {
            /* Add path information about a direct virtual base class of
               new_direct_bcp that was not first in the depth-first
               left-to-right traversal of the latter's derivation graph.
               Look for the matching derivation entry to get the right
               access. */
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
            /* bcp is a base class of new_direct_bcp->type; we need to find
               the corresponding base class of type_ptr.  Find a disambiguator
               in case what we are looking for is an ambiguous base class of
               type_ptr. */
            disambiguator = find_disambiguator(new_direct_bcp, bcp);
            ctsp->virtual_function_info_base_class =
                        corresponding_base_class(bcp, type_ptr, disambiguator);
          }  /* if */
          /* Advance the virtual function count so that any new virtual
             functions will be tacked on at the end of the shared virtual
             function info block.  (Redeclarations will use the slot
             already reserved for the function.) */
          ctsp->highest_virtual_function_number =
                                   base_ctsp->highest_virtual_function_number;
        }  /* if */
        first_direct_nonvirtual_base_class = FALSE;
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
          set_target_of_conversion_function_flag(new_direct_bcp->type);
        }  /* if */
      }  /* if */
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
  /* Determine which, if any, of the base classes was specified with a
     template-dependent name and so should not be visible for certain
     lookups. */
  if (type_ptr->variant.class_struct_union.is_template_class &&
      !type_ptr->variant.class_struct_union.is_specialized) {
    mark_dependent_base_classes(type_ptr, class_state);
  }  /* if */
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
  db_exit();
}  /* scan_base_specifier_list */


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
       keywords. */
    is_start = (curr_token == tok_static || curr_token == tok_typedef ||
                curr_token == tok_private || curr_token == tok_protected ||
                curr_token == tok_public || curr_token == tok_compl);

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
       a friend class declaration as long as it refers to a class. */
    friend_class_type = skip_typerefs(friend_class_type);
    check_assertion(is_immediate_class_type(friend_class_type));
    if (class_type == friend_class_type) {
      /* Diagnostic on excessive narcissism. */
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
				a_symbol_ptr		sym,
				a_type_ptr		new_type,
				a_template_param_ptr	templ_param_list,
				a_boolean		templates_only)
/*
sym is a member function symbol or overloaded function symbol from a
previous declaration.  new_type is the type from the current
declaration.  If the new declaration is a function template,
templ_param_list points to the template parameter list.  Check the
type for compatibility with sym or, if sym represents an overloaded
function, with any of the instances.  If a match is found, return a
pointer to the symbol.  If not, return NULL.

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
  a_boolean			 is_overloaded_function, match;
  a_type_ptr                     orig_type, orig_this_class, new_this_class;
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
  new_rts = (skip_typerefs(new_type))->variant.routine.extra_info;
  new_this_class = new_rts->this_class;
  new_function_is_qualified = (new_rts->qualifiers != TQ_NONE);
  /* Go through the symbol list and look for an instance in which the
     types are compatible with the current type. */
  for (; sym != NULL; sym = is_overloaded_function ? sym->next : NULL) {
    a_template_param_ptr		other_templ_param_list;
    a_template_symbol_supplement_ptr	tssp;
    /* Ignore projection symbols. */
    if (sym->kind == (a_symbol_kind)sk_projection) continue;
    check_assertion(sym->kind == (a_symbol_kind)sk_function_template ||
                    sym->kind == (a_symbol_kind)sk_member_function);
    /* If looking only for templates, ignore nontemplates.  When looking
       for nontemplates, ignore templates. */
    if (sym->kind == (a_symbol_kind)sk_function_template != templates_only) {
      continue;
    }  /* if */
    /* Get the routine pointer associated with either the routine symbol
       or the function template symbol. */
    if (sym->kind == (a_symbol_kind)sk_function_template) {
      orig_type = sym->variant.template_info->variant.function.routine->type;
    } else {
      orig_type = sym->variant.routine.ptr->type;
    }  /* if */
    orig_rts = (skip_typerefs(orig_type))->variant.routine.extra_info;
    orig_this_class = orig_rts->this_class;
    orig_function_is_qualified = (orig_rts->qualifiers != TQ_NONE);
    if (new_function_is_qualified != orig_function_is_qualified) {
      /* No match is possible.  Don't bother calling types_are_compatible. */
      continue;
    }  /* if */
    if (sym->kind == (a_symbol_kind)sk_function_template &&
        templ_param_list == NULL) {
      /* The symbol we are checking is a template, but no template parameter
         list was supplied by the caller.  This is not a match. */
      continue;
    }  /* if */
    if (templ_param_list != NULL &&
        sym->kind == (a_symbol_kind)sk_function_template) {
      /* If a template parameter list is present and the candidate symbol
         is for a function template, make sure the lists match.  A
         template parameter list could be present for a normal member function
         of a class template when the member function is being defined
         outside of the class. */
      tssp = template_supplement_for_symbol(sym);
      other_templ_param_list =
                       tssp->variant.function.decl_cache.decl_info->parameters;
      if (!equiv_template_param_lists(other_templ_param_list,
                                     templ_param_list,
                                     /*issue_errors=*/FALSE,
                                     (a_source_position*)NULL)) {
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
    match = routine_types_are_compatible(orig_type, new_type, TCF_NO_FLAGS);
    if (!new_function_is_qualified) {
      /* Restore the implicit "this" parameter types in orig_type and
         new_type. */
      new_rts->this_class = new_this_class;
      orig_rts->this_class = orig_this_class;
    }  /* if */
    /* If a match was found by types_are_compatible, break out of the
       loop. */
    if (match) break;
  }  /* for */
  return sym;
}  /* member_function_redecl_sym_with_template_flag */


a_symbol_ptr member_function_redecl_sym(
				a_symbol_ptr		sym,
				a_type_ptr		new_type,
				a_template_param_ptr	templ_param_list)
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
  a_symbol_ptr	result;

  /* First look for a matching template. */
  result = member_function_redecl_sym_with_template_flag(
                     sym, new_type, templ_param_list, /*templates_only=*/TRUE);
  if (result == NULL) {
    /* No template was found, look for a normal member function. */
    result = member_function_redecl_sym_with_template_flag(
                    sym, new_type, templ_param_list, /*templates_only=*/FALSE);
  }  /* if */
  return result;
}  /* member_function_redecl_sym */


void update_friend_function_info(a_routine_ptr   rout_ptr,
                                 a_type_ptr      class_type,
                                 a_boolean       is_definition,
                                 a_boolean       move_to_front)
/*
Update the list of befriending classes associated with rout_ptr to reflect
that it is now a friend of class_type.  Also update class_type to indicate
that the routine indicated by rout_ptr is a friend.  is_definition is TRUE
when the function is defined in the friend declaration.  move_to_front
is TRUE when an entry already on the list is being moved to the beginning
of the list because we only now know that the function was defined in
a friend declaration.  This is used for template instantiations.
*/
{
  a_class_list_entry_ptr clep;
  a_class_list_entry_ptr prev_clep = NULL;

  clep = rout_ptr->befriending_classes;
  /* Issue a warning if this is a duplicate friend declaration. */
  for (; clep != NULL; prev_clep = clep, clep = clep->next) {
    if (clep->class_type == class_type) {
      /* Suppress the diagnostic when we are moving an existing entry to
         the front of the list. */
      if (!move_to_front) remark(ec_duplicate_friend_decl);
      break;
    } /* if */
  } /* for */
  if (clep == NULL) {
    a_class_type_supplement_ptr ctsp;
    a_routine_list_entry_ptr    rlep;
    check_assertion(!move_to_front);
    /* No duplication was detected. */
    clep = alloc_list_entry_for_class();
    clep->class_type = class_type;
    /* Add a friend declaration that is a definition to the front of the
       befriending_classes list and a nondefining declaration to the end. */
    if (is_definition || rout_ptr->befriending_classes == NULL) {
      clep->next = rout_ptr->befriending_classes;
      rout_ptr->befriending_classes = clep;
    } else {
      a_class_list_entry_ptr  end_of_list = rout_ptr->befriending_classes;
      while (end_of_list->next != NULL) end_of_list = end_of_list->next;
      end_of_list->next = clep;
    }  /* if */
    /* Now add the routine to the friends list for the current class. */
    ctsp = class_type->variant.class_struct_union.extra_info;
    rlep = alloc_list_entry_for_routine();
    rlep->routine = rout_ptr;
    rlep->next = ctsp->friend_routines;
    ctsp->friend_routines = rlep;
  } else if (move_to_front) {
    if (prev_clep == NULL) {
      /* The entry is already on the front of the list. */
    } else {
      /* Move the entry to the front of the list. */
      prev_clep->next = clep->next;
      clep->next = rout_ptr->befriending_classes;
      rout_ptr->befriending_classes = clep;
    }  /* if */
  } /* if */
}  /* update_friend_function_info */


static a_symbol_ptr decl_dependent_friend_function(
                                         a_symbol_locator       *locator,
                                         a_type_ptr             function_type,
                                         a_func_info_block_ptr  func_info)
/*
Create a routine and associated symbol for a template dependent friend
declaration of type function_type.  The locator for the friend declarator
and some extra declaration info are passed through locator and func_info.
The routine symbol is returned (but not linked into the symbol table).
The routine entry itself is linked into the IL only if prototype
instantiations are recorded in the IL.
*/
{
  a_symbol_ptr  sym = NULL;
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
     the IL if prototype_instantiations_in_il is FALSE. */
  rp->is_prototype_instantiation = TRUE;
  rp->is_template_function = TRUE;
  sym->variant.routine.ptr = rp;
  set_source_corresp(&rp->source_corresp, sym);
  if (locator->is_class_member) {
    a_type_ptr  parent_type = locator->parent.class_type;
    if (parent_type->kind == (a_type_kind)tk_template_param) {
      parent_type = proxy_class_for_template_param(parent_type);
    }  /* if */
    set_class_membership(sym, &rp->source_corresp, parent_type);
  } else if (locator->parent.namespace_ptr != NULL) {
    set_namespace_membership(sym, &rp->source_corresp,
                             locator->parent.namespace_ptr);
  }  /* if */
  if (locator->template_arg_list != NULL) {
    process_unattached_template_argument_list(locator->template_arg_list);
    rp->template_arg_list = locator->template_arg_list;
    rp->expl_template_arg_list_used = TRUE;
  }  /* if */
  if (func_info->is_definition) {
    rp->defined = sym->defined = TRUE;
    rp->defined_in_friend_decl = TRUE;
    rp->is_inline = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    rp->declared_type = func_info->declared_type;
    if (ssep != NULL && prototype_instantiations_in_il) {
      ssep->entity.kind = (a_byte_il_entry_kind)iek_routine;
      ssep->entity.ptr  = (char *)rp;
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  } else {
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (ssep != NULL && prototype_instantiations_in_il) {
      /* Point to it from a secondary source sequence_entry: */
      sssdp = make_source_sequence_secondary_decl((char*)rp, iek_routine,
                                                  func_info->declared_type);
      sssdp->decl_position = sym->decl_position;
      sssdp->friend_decl = TRUE;
      ssep->entity.kind = (a_byte_il_entry_kind)iek_src_seq_secondary_decl;
      ssep->entity.ptr  = (char *)sssdp;
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
  switch_back_to_original_region(region_to_switch_back_to);
  return sym;
}  /* decl_dependent_friend_function */


static a_symbol_ptr decl_friend_function(a_symbol_locator       *locator,
                                         a_type_ptr             class_type,
                                         a_type_ptr             function_type,
                                         a_func_info_block_ptr  func_info,
                                         a_member_decl_info_ptr decl_info)
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
  a_storage_class              storage_class;
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
    sym = locator->specific_symbol;
    if (sym == NULL && locator->is_template_id) {
      /* If this is a template-id for which the symbol has not yet been
         found, look it up now. */
      sym = normal_id_lookup(locator, IDL_FRIEND_LOOKUP);
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
        sym = decl_dependent_friend_function(locator, function_type,
                                             func_info);
        goto done;
      }  /* if */
    }  /* if */
  }  /* if */    
  if (!is_error_locator(*locator)) {
    if (!(microsoft_mode || any_cfront_mode()) ||
        (sym != NULL && sym->ambiguous)) {
      check_ambiguity_and_verify_access(locator);
    }  /* if */
    srk_flags = SRK_DECLARATION | SRK_FRIEND;
    if (func_info->is_definition) srk_flags |= SRK_DEFINITION;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    declarator_ssep = func_info->declarator_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (microsoft_mode && sym != NULL && sym->is_class_member &&
        sym->kind == (a_symbol_kind)sk_projection) {
      reduce_projection_symbol_to_fundamental_symbol(sym);
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
        check_main_function(func_info, function_type,
                            &decl_info->storage_class, &is_inline,
                            &locator->source_position);
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
        } else {
          /* The primary source sequence entry will be deferred until the
             class definition has been completed; a secondary-decl entry
             will be put out here. */
          func_info->is_movable_member_or_friend_def = TRUE;
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
      storage_class = decl_info->storage_class;
      if (microsoft_mode &&
          storage_class != (a_storage_class)sc_unspecified) {
        /* In Microsoft mode "extern" and "static" are permitted on a
           nonmember friend declaration. */
        if (storage_class != (a_storage_class)sc_static &&
            storage_class != (a_storage_class)sc_extern) {
          /* The storage class of a function has to be extern or static. */
          pos_warning(ec_bad_function_storage_class,
                      &decl_info->decl_start_pos);
          storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
      }  /* if */
      decl_routine(locator, storage_class, function_type, func_info,
                   declarator_ssep, srk_flags, &decl_info->decl_modifiers,
                   &sym, &linkage, &old_type, &ext_sym,
                   &decl_info->decl_pos_block);
      /* WP 11.4 para 5 prohibits defining a nonmember function in a local
         class friend declaration. */
      if (func_info->is_definition &&
          !locator->is_error &&
          class_type->source_corresp.is_local_to_function) {
        pos_sy_error(ec_bad_scope_for_definition, &pos_curr_token, sym);
      }  /* if */
      if (arg_dependent_lookup_enabled && sym->is_invisible) {
        add_friend_function_to_lookup_list_for_class(sym, class_type);
      }  /* if */
    } else {
      if (sym->parent.class_type == class_type) {
        /* It's a member function of the very class that is according it
           friendship.  Issue a diagnostic. */
        diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
                   ec_self_friendship);
      }  /* if */
      if (microsoft_mode &&
          decl_info->storage_class != (a_storage_class)sc_unspecified) {
        /* Member function -- storage class is not allowed. */
        pos_warning(ec_storage_class_not_allowed, &decl_info->decl_start_pos);
      }  /* if */
      /* It's a member function.  Find the right type signature for this
         member function name.  This could potentially be an instance of
         a member function template.  If none can be found, NULL is
         returned. */
      sym = find_matching_template_instance(
                               sym, function_type, locator->template_arg_list,
                               (a_boolean)locator->is_template_id,
                               es_error);
      if (sym == NULL) {
        /* This is a member function, but one with a type that doesn't
           match a previously declared member.  A diagnostic will have been
           issued by find_matching_template_instance. */
        set_to_error_locator(*locator);
      } else {
        /* "inline" may not be introduced by this declaration. */
        if (func_info->is_inline && !func_info->is_definition &&
            !sym->variant.routine.ptr->is_inline) {
          error(ec_inline_not_allowed);
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
            a_routine_ptr  rp = sym->variant.routine.ptr;

            /* Since this is a non-defining entry, it is represented by a
               secondary-decl entry in the source sequence list.  Enter the
               current function type. */
            (void)update_src_seq_secondary_decl((char *)rp,
                                                func_info->declared_type,
                                                SSSD_FRIEND_DECL,
                                                &decl_info->decl_pos_block);
          }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (is_error_locator(*locator)) {
    /* Create a dummy symbol to return when there's been an error.  This is
       required for further processing, in case there's a definition of the
       the routine body. */
    a_routine_ptr	rp;
    sym = enter_symbol((a_symbol_kind)sk_routine, locator, DEPTH_OF_FILE_SCOPE,
                       /*suppress_redecl_error=*/FALSE);
    rp = make_routine(function_type, (a_storage_class)sc_static,
                      NO_SCOPE_DEPTH);
    sym->variant.routine.ptr = rp;
    /* If this is a friend declaration in a prototype instantiation,
       mark it as a prototype instantiation too. */
    if (scope_stack[depth_scope_stack].in_prototype_instantiation) {
      rp->is_prototype_instantiation = TRUE;
      rp->is_template_function = TRUE;
    }  /* if */
    /* Set the source correspondence. */
    set_source_corresp(&sym->variant.routine.ptr->source_corresp, sym);
  } else if (!class_type->variant.class_struct_union.is_nonreal_class ||
             prototype_instantiations_in_il) {
    update_friend_function_info(sym->variant.routine.ptr, class_type,
                                (a_boolean)func_info->is_definition,
                                /*move_to_front=*/FALSE);
  }  /* if */
done:
  if (func_info->is_definition) {
    /* Since this is a definition, record the current lint argsused and
       varargs-count state in the routine type. That will suppress any
       warnings about unused parameters or variable arguments. */
    record_lint_argsused_and_varargs_state(sym);
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


static a_symbol_ptr symbol_for_member_function(
                                         a_symbol_locator       *locator,
                                         a_type_ptr             type,
                                         a_type_ptr             class_type,
                                         a_member_decl_info_ptr decl_info,
                                         a_symbol_ptr           *overload_sym)
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
    sym = find_direct_member_function(locator, class_type);
    if (sym != NULL) {
      /* A member function by this name has already been entered into the
         symbol table.  This could be a redeclaration, which is illegal for
         class members.  Check for that first by looking for a type match. */
      new_sym = member_function_redecl_sym(sym, type,
                                           (a_template_param_ptr)NULL);
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
         name. */
      a_symbol_ptr  fund_sym = fundamental_symbol_of(sym);

      if (is_nontype_template_param_symbol(fund_sym)) {
        /* A non-type template parameter symbol may be a function, so don't
           issue a redeclaration error. */
        suppress_redecl_error = TRUE;
      } else {
        /* The routine overload_distinguishable returns TRUE if the
           routine types are candidates for overloading; if it returns FALSE
           it also returns the error code for a diagnostic explaining why. */
        /* The templ_param_list is NULL in the following call because
           although member functions of class templates have template types
           in their parameters, they are not called using the template
           overload resolution mechanism. */
        if (!overload_distinguishable(sym, type,
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

    class_type = orig_sym->parent.class_type;
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


static void set_mixed_static_nonstatic_flag(a_symbol_ptr  overload_sym)
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
      if (sym2->kind != (a_symbol_kind)sk_function_template) {
        tp2 = routine_symbol_type(sym2);
      } else {
        tp2 = sym2->variant.template_info->variant.function.routine->type;
      }  /* if */
      if (routine_type_is_nonstatic_member_function(tp1) !=
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
#if 0
      /* This is based on an interpretation that WP 7.3.3 para 13 applies to
         all member functions (as the example suggests), not only to virtual
         functions (as the text currently indicates). */
#endif /* if 0 */
      compat = TRUE;
    } else if (types_are_strictly_compatible(
                                         tp1->variant.routine.return_type,
                                         tp2->variant.routine.return_type)) {
      /* The return types are also compatible.  Set *err so that an error
         will be issued by the caller. */
      *err = compat = TRUE;
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
        } else if (types_of_decl_and_using_decl_conflict(decl_sym,
                                                         using_sym, &err)) {
          /* An error is issued, unless using_sym is a member function being
             hidden and/or overridden by decl_sym (err == FALSE), or both
             symbols refer to the same entity (because they are extern "C"
             declarations). */
          if (err && !symbols_are_lookup_equivalent(decl_sym, using_sym)) {
            pos_sy2_error(ec_conflicts_with_using_decl, pos, decl_sym,
                          using_sym);
          }  /* if */
          /* Remove the symbol from the overload list by skipping around it. */
          prev_in_overload_set->next = sym->next;
          if (decl_sym->is_class_member) {
            /* Remove the class-member-using-decl entry associated with sym. */
            mark_class_member_using_decl_as_hidden(decl_sym->parent.class_type,
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
      if (is_reference_type(tp)) {
        /* Reference argument. */
        tp = type_pointed_to(tp);
        qualifiers = get_type_qualifiers(tp);
      }  /* if */
    }  /* if */
    switch (sfkind) {
      case sfk_constructor:
        if (first_param == NULL) {
          /* Default constructor. */
          sym = find_default_constructor(class_type, ambiguous);
        } else {
          /* Copy constructor. */
          sym = find_copy_constructor(class_type, qualifiers,
                                      source_pos, ambiguous,
                                      &class_bitwise_copy);
        }  /* if */
        break;
      case sfk_destructor:
        /* Destructor. */
        sym = (symbol_supplement_for_class(class_type))->destructor;
        break;
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
  class_type = rp->source_corresp.parent.class_type;
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
        if (microsoft_mode && using_sym->kind == (a_symbol_kind)sk_routine &&
            compatible_functions_with_c_linkage(using_sym, sym)) {
          /* In Microsoft mode, extern "C" functions from different namespaces
             create different entities even if they have the same name and
             type.  However, two such entities do not conflict if they are
             brought in the same scope with a using-declaration. */
        } else if (types_of_decl_and_using_decl_conflict(
                                                      sym, using_sym, &err)) {
          /* Unless using_sym is a member function being hidden and/or
             overridden by the previous declaration, an error is issued. */
          if (err) {
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

static void merge_decl_modifiers(a_type_ptr              class_type,
                                 a_member_decl_info_ptr  decl_info,
                                 a_boolean               is_definition)
/*
class_type is the type of the current class, in which the decl_modifiers
field may have been set to indicate modifiers for the class as a whole, and
a field in *decl_info represents the modifiers declared for the current member.
Check for compatibility and update *decl_info based on the two.  is_definition
is TRUE when this is called for a member function definition.
*/
{
  a_decl_modifier  decl_modifiers, class_decl_modifiers;

  class_decl_modifiers =
          class_type->variant.class_struct_union.extra_info->decl_modifiers;
  /* Only dllimport and dllexport are applied to members, so strip off any
     others that may have been declared for the class as a whole (e.g.,
     novtable). */
  class_decl_modifiers &= (DM_DLLIMPORT | DM_DLLEXPORT);
  if (class_decl_modifiers != DM_NONE) {
    decl_modifiers = decl_info->decl_modifiers.flags;
    if (decl_modifiers & (DM_DLLIMPORT | DM_DLLEXPORT)) {
      /* If there are dll modifiers on the class, they cannot appear on the
         member declaration, too. */
      pos_st_warning(ec_decl_modifiers_invalid_for_this_decl,
                     &decl_info->decl_start_pos,
                     decl_modifier_names[(decl_modifiers & DM_DLLIMPORT ?
                                           (int)dmt_dllimport :
                                           (int)dmt_dllexport)]);
      decl_modifiers &= ~(DM_DLLIMPORT | DM_DLLEXPORT);
    }  /* if */
    if (is_definition && (class_decl_modifiers & DM_DLLIMPORT)) {
      /* Put no dll attribute on an inline member function. */
    } else {
      /* Merge the sets of flags. */
      decl_modifiers |= class_decl_modifiers;
    }  /* if */
    decl_info->decl_modifiers.flags = decl_modifiers;
  }  /* if */
}  /* merge_decl_modifiers */

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
    cssp->assignment_operator = sym;
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
                          /*is_declarative_context=*/TRUE)) {
    cssp->has_copy_constructor = TRUE;
    if (qualifiers & TQ_CONST) {
      cssp->has_copy_constructor_for_const_object = TRUE;
    }  /* if */
    if (!compiler_generated) {
      /* If a user-defined copy constructor is declared for the class,
         construction by bitwise copying is not allowed.  (On the other
         hand, this flag *may* be TRUE even when the compiler generates
         a copy constructor.) */
      cssp->construction_by_bitwise_copy_allowed = FALSE;
    }  /* if */
  }  /* if */
}  /* check_member_decl_is_copy_constructor */


static void decl_member_function(a_symbol_locator        *locator,
                                 a_type_ptr              class_type,
                                 a_type_ptr              member_type,
                                 a_func_info_block_ptr   func_info,
                                 a_class_def_state_ptr   class_state,
                                 a_member_decl_info_ptr  decl_info,
                                 a_boolean               compiler_generated)
/*
For a member function declaration: create a symbol entry and a routine entry
for the member function, add the symbol to the symbol table, and append the
routine entry to the routines list for the current class.  *locator gives the
source locator of the declaration.  class_type points to the type entry of the
class of which the function is a member, and member_type points to the type
entry of the function itself.  *class_state and *decl_info track general
information about the class definition and specific information about the
member declaration, respectively.  compiler_generated is TRUE for implicitly
declared member functions.
*/
{
  a_symbol_ptr                  sym, overload_sym;
  a_routine_ptr                 rtn;
  a_class_symbol_supplement_ptr cssp = symbol_supplement_for_class(class_type);
  a_type_ptr                    tp;
  a_source_sequence_entry_ptr   declarator_ssep = NULL;
  a_name_linkage_kind           def_name_linkage;
  a_routine_type_supplement_ptr rtsp;

  db_enter(3, "decl_member_function");
  rtsp = skip_typerefs(member_type)->variant.routine.extra_info;
  /* Check if we are attempting to declare a static member function through a
     qualified function type typedef. E.g.,
       typedef void f() const; struct S { static F f(); }           */
  if (decl_info->storage_class == (a_storage_class)sc_static) {
    if (member_type->kind == (a_type_kind)tk_typeref &&
        typeref_is_typedef(member_type) &&
        rtsp->qualifiers != TQ_NONE) {
      pos_error(ec_bad_qualified_function_type, &locator->source_position);
    }  /* if */
  }  /* if */

  /* If this is a user-defined conversion or an overloaded operator,
     check for errors in the argument list.  Note that this is done before
     creating the symbol, since an invalid conversion or operator should not
     be added to the overload list. */
  check_operator_function_params(member_type, class_type, locator);
  /* Look for a prior declaration or function overloading. */
  sym = symbol_for_member_function(locator, member_type, class_type,
                                   decl_info, &overload_sym);
  if (sym->variant.routine.ptr != NULL) {
    /* symbol_for_member_function has returned a symbol that has already been
       declared.  Issue an error to redeclare a member function. */
    /* Issue an error on trying to redeclare the function. */
    pos_sy_error(ec_member_function_redeclaration, &locator->source_position,
                 sym);
    set_to_named_error_locator(*locator);
    sym = enter_local_symbol((a_symbol_kind)sk_member_function, locator,
                             decl_scope_level, /*suppress_redecl_error=*/TRUE);
  }  /* if */
  decl_info->member_sym = sym;
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
  rtn->source_corresp.access = class_state->access;
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
  }  /* if */
  if (func_info->is_inline) {
    /* Inline member function (either because "inline" was specified or
       a function definition is present). */
    rtn->is_inline = TRUE;
  }  /* if */
  if (locator->is_operator_name) {
    /* Overloaded operator function. */
    rtn->special_kind = (a_special_function_kind)sfk_operator;
    rtn->opname_kind = locator->variant.opname;
  } else if (locator->is_conversion_name) {
    /* User-defined conversion function. */
    rtn->special_kind = (a_special_function_kind)sfk_conversion;
  } else if (decl_info->is_constructor) {
    rtn->special_kind = (a_special_function_kind)sfk_constructor;
  } else if (decl_info->is_destructor) {
    rtn->special_kind = (a_special_function_kind)sfk_destructor;
  }  /* if */
  if (compiler_generated) {
    rtn->compiler_generated = TRUE;
  } else {
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
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (!is_error_locator(*locator)) {
    /* If decl-modifiers were declared for the class and/or for the
       member, check for consistency and use the union of the two. */
    merge_decl_modifiers(class_type, decl_info,
                         (a_boolean)func_info->is_definition);
    update_routine_decl_modifiers(rtn, &decl_info->decl_modifiers,
                                  &locator->source_position,
                                  /*is_redecl=*/FALSE,
                                  (a_boolean)func_info->is_definition,
                                  (a_boolean)func_info->is_inline);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (!compiler_generated) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (func_info->is_definition) {
      /* For a definition enter the function type as the "declared_type" in
         the routine entry itself. Avoid adding a redundant type to the IL
         if possible. */
      set_routine_declared_type(rtn, func_info->declared_type);
      /* For default arg processing later on, save the type that's used as
         the declared type. */
      func_info->declared_type = rtn->declared_type;
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      /* Unless this is a member of a local class, this inline member
         function definition will be represented in the source-sequence
         list as a non-defining declaration, and the source-sequence entry
         for its definition will be put out after the class definition
         is terminated.  This is to solve a problem in generated C++ when
         function template instantiations are represented as explicit
         specializations and where, at the point of instantiation, the
         class is required to be complete. */
      if (!class_type->source_corresp.is_local_to_function) {
        func_info->is_movable_member_or_friend_def = TRUE;
      }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    }  /* if */
    if (!func_info->is_definition ||
        func_info->is_movable_member_or_friend_def) {
      /* A non-defining entry is represented by a
         secondary-decl entry in the source sequence list. */
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
      } else {
        /* Normal declaration.  If necessary, update the declared type,
           which was saved during function declarator processing, to make
           it consistent with the routine type. */
        a_routine_type_supplement_ptr  rtsp1, rtsp2;

        tp = func_info->declared_type;
        rtsp1 = skip_typerefs(member_type)->variant.routine.extra_info;
        rtsp2 = skip_typerefs(tp)->variant.routine.extra_info;
        if (rtsp1->this_class != rtsp2->this_class ||
            rtsp1->qualifiers != rtsp2->qualifiers ||
            rtsp1->routine_name_linkage != rtsp2->routine_name_linkage) {
          /* The implicit-this-param-type and/or name-linkage may need to be
             set in the declared type. */
          if (tp->kind == (a_type_kind)tk_typeref) {
            /* The typedef is potentially shared, so don't modify the
               type it points to without copying it first. */
            check_assertion(!is_qualified_type(tp));
            tp = copy_routine_type_with_param_types(tp,
                                                   /*copy_default_args=*/TRUE);
            rtsp2 = tp->variant.routine.extra_info;
            /* For default arg processing later on, save the type that will
               be used as the declared type in the secondary declaration
               entry. */
            func_info->declared_type = tp;
          }  /* if */
          rtsp2->this_class = rtsp1->this_class;
          rtsp2->qualifiers = rtsp1->qualifiers;
          rtsp2->routine_name_linkage = rtsp1->routine_name_linkage;
        }  /* if */
      }  /* if */          
      /* Update the secondary-declaration entry.  A member function
         declaration within a class definition is always the initial
         declaration. */
      if (!update_src_seq_secondary_decl((char *)rtn, tp,
                                         SSSD_FIRST_DECLARATION,
                                         &decl_info->decl_pos_block)) {
        /* No source-sequence secondary declaration entity was found, which
           means the declared type will not be needed.  Clear the pointer
           to suppress copying the default arg expression to it later on. */
        func_info->declared_type = NULL;
      }  /* if */
    }  /* if */
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
       Set it up to look like that. */
    a_template_instance_ptr           tip;
    a_template_symbol_supplement_ptr  tssp;

    sym->variant.routine.instance_ptr = tip = alloc_template_instance();
    tip->instance_sym = tip->template_sym = sym;
    tip->template_info = tssp = alloc_template_symbol_supplement(sym->kind);
    tssp->variant.function.routine = rtn;
    tssp->variant.function.func_info = *func_info;
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
      set_class_membership_for_template((a_symbol_ptr)NULL, templ,
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
  if (!is_error_locator(*locator)) {
    /* Do processing for special member functions, including assignment
       operators, constructors and destructors. */
    if (locator->is_operator_name) {
      /* If this is an assignment operator, record a pointer to it in the
         symbol -- to facilitate generating default assignment operators. */
      if (rtn->opname_kind == (an_opname_kind)onk_assign) {
        record_assignment_operator_in_class_symbol(cssp, sym, overload_sym);
      } else if (rtn->opname_kind == (an_opname_kind)onk_new) {
        cssp->has_operator_new = TRUE;
      } else if (rtn->opname_kind == (an_opname_kind)onk_array_new) {
        cssp->has_operator_array_new = TRUE;
      } else if (rtn->opname_kind == (an_opname_kind)onk_delete) {
        cssp->has_operator_delete = TRUE;
      } else if (rtn->opname_kind == (an_opname_kind)onk_array_delete) {
        cssp->has_operator_array_delete = TRUE;
      }  /* if */
    } else if (locator->is_conversion_name) {
      /* User-defined conversion function. */
      a_boolean  is_usable = TRUE;

      /* Check the target type of the conversion -- which is the return type
         of rout_type. */
      tp = f_skip_typerefs(return_type_of(rtn->type));
      if (tp == class_type) {
        /* Converting to same type (possibly qualified) is not done. */
        is_usable = FALSE;
      } else if (is_immediate_class_type(tp)) {
        if (!cfront_2_1_mode && find_base_class_of(class_type, tp) != NULL) {
          /* An operator that converts from a derived class to a base class
             is allowed by cfront 2.1, but not by cfront 3.0. */
          is_usable = FALSE;
        } else {
          /* The target type of the conversion is a class or ref-to-class
             type: set a flag to mark it as target of a conversion. */
          set_target_of_conversion_function_flag(tp);
        }  /* if */
      } else if (is_void_type(tp)) {
        /* Conversion to (possibly qualified) void type. */
        is_usable = FALSE;
      }  /* if */
      if (is_usable) {
        /* Create a conversion list entry.  This list provides an alternative
           to traversing the entire symbols list for a class to find its
           conversion functions. */
        add_to_conversion_list(sym, cssp);
      } else {
        /* Conversion to void or to the same type or a reference to the same
           type or to a base class or a reference to a base class "is never
           used" (WP 12.3.2; that is, it is not used in implicit or explicit
           conversions but only in an explicit invocations of the function). */
        pos_sy_warning(ec_conversion_function_not_usable,
                       &locator->source_position, sym);
      }  /* if */
    }  /* if */
    if (exceptions_enabled && compiler_generated) {
      /* A compiler generated constructor, destructor, or assignment
         operator is assumed to through any exception that can be thrown
         a base-class function it will call. */
      form_exception_specification_for_generated_function(rtn);
    }  /* if */
    if (!sym->is_error) {
      /* If "virtual" was specified in the declaration, mark the routine as
         virtual.  Even if it wasn't, its virtualness can be inherited.  In
         either case record the relationship between the current routine and
         its appearance in the base classes of the current class. */
      a_boolean  is_virtual = ((decl_info->dso_flags & DSO_VIRTUAL) &&
                               !decl_info->invalid_virtual_specifier);
      if (check_for_virtual_function(is_virtual, sym, class_type, class_state,
                                     &locator->source_position)) {
        /* Classes with virtual functions require constructors. */
        class_state->constructor_required = TRUE;
        /* Classes with virtual functions cannot be constructed or assigned
           by bitwise copying. */
        cssp->construction_by_bitwise_copy_allowed = FALSE;
        cssp->assignment_by_bitwise_copy_allowed = FALSE;
      }  /* if */
    }  /* if */
    if (rtn->special_kind == (a_special_function_kind)sfk_constructor) {
      /* Set the pointer to the constructor symbol in the class symbol
         supplement. */
      if (decl_info->is_trivial_default_constructor) {
        /* A trivial default constructor is never actually called, so it is
           not added to the constructor set (which should be empty). */
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
          cssp->has_nontrivial_default_constructor = TRUE;
          if (!compiler_generated) {
            cssp->has_user_declared_default_constructor = TRUE;
          }  /* if */
        }  /* if */
        /* Determine if this is a copy constructor.  If so, set the class
           symbol supplement flags appropriately. */
        check_member_decl_is_copy_constructor(rtn, class_type,
                                              compiler_generated);
      }  /* if */
    } else if (rtn->special_kind == (a_special_function_kind)sfk_destructor) {
      /* Set the pointer to the destructor symbol in the class symbol
         supplement. */
      cssp->destructor = sym;
    }  /* if */
#if BACK_END_IS_CP_GEN_BE
    /* Set the "name linkage environment" for this routine. */
    rtn->surrounding_name_linkage_state =
                          scope_stack[depth_scope_stack].default_name_linkage;
#endif /* BACK_END_IS_CP_GEN_BE */
  }  /* if */
#if DEBUG
  if (debug_level >= 3) db_symbol(sym, "", 4);
#endif /* DEBUG */

  db_exit();
}  /* decl_member_function */


static void decl_member_function_template(
				a_symbol_locator        *locator,
                                a_type_ptr              class_type,
                                a_type_ptr              member_type,
				a_template_param_ptr	templ_param_list,
                                a_func_info_block       *func_info,
                                a_class_def_state_ptr   class_state,
                                a_member_decl_info_ptr  decl_info)
/*
Process the declaration of a member function template.  *locator is the
symbol locator of the template.  class_type identifies the class in which it
was declared, and member_type is its function type.  templ_param_list
is the template parameter list of the function template.  *func_info contains
information gathered in processing the declarator.  *class_state and
*decl_info track general information about the class definition and specific
information about the member declaration, respectively.  (This function is
similar to decl_function_template, which handles non-member function
templates and out-of-class template declarations of functions that are
members of template classes, and to decl_member_function, which handles
in-class member function declarations.)
*/
{
  a_template_symbol_supplement_ptr   tssp;
  a_routine_ptr                      rtn;
  a_symbol_ptr                       sym = NULL;
  a_symbol_ptr                       prototype_sym;
  a_symbol_ptr                       other_sym, overload_sym = NULL;
  a_class_symbol_supplement_ptr      cssp;
  a_scope_depth                      effective_decl_level;

  db_enter(3, "decl_member_function_template");
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
        if (other_sym->kind == (a_symbol_kind)sk_function_template) {
          /* Issue an error if the other member function template declaration
             has a type compatible with this one -- compare the routine
             types. */
          a_template_param_ptr			other_templ_param_list;
          a_template_symbol_supplement_ptr	other_tssp;
          a_type_ptr				tp;
          other_tssp = template_supplement_for_symbol(other_sym);
          tp = other_tssp->variant.function.routine->type;
          other_templ_param_list =
                 other_tssp->variant.function.decl_cache.decl_info->parameters;
          if (equiv_template_param_lists(other_templ_param_list,
                                         templ_param_list,
                                         /*issue_errors=*/FALSE,
                                         (a_source_position*)NULL) &&
              param_types_are_compatible(tp, member_type, TCF_NO_FLAGS)) {
            an_error_code  error_code = ec_no_error;
            if (routine_type_is_nonstatic_member_function(tp) !=
                  routine_type_is_nonstatic_member_function(member_type)) {
              error_code = ec_static_nonstatic_with_same_param_types;
              pos_error(error_code, &locator->source_position);
            } else if (routine_types_are_compatible(tp, member_type,
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
  set_type_involves_deduced_template_param(member_type);
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
    rtn->is_inline = TRUE;
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
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Prevent the generation of a source sequence entry for the a_template
       entry: we already did so elsewhere. */
    a_boolean saved_sses_disallowed = source_sequence_entries_disallowed;
    source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Update cross-reference information, etc. */
    if (func_info->is_definition) {
      mark_defined(sym, &locator->source_position);
    } else {
      mark_declared(sym, &locator->source_position);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Restore the previous state wrt. the generation of source sequence
       entries. */
    source_sequence_entries_disallowed = saved_sses_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    update_decl_pos_info(&rtn->source_corresp, &decl_info->decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
  if (locator->is_operator_name) {
    /* Overloaded operator function. */
    rtn->special_kind = (a_special_function_kind)sfk_operator;
    rtn->opname_kind = locator->variant.opname;
  } else if (locator->is_conversion_name) {
    /* User-defined conversion function. */
    rtn->special_kind = (a_special_function_kind)sfk_conversion;
  }  /* if */
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
      switch(rtn->opname_kind) {
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
      rtn->special_kind = (a_special_function_kind)sfk_constructor;
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
#if MICROSOFT_EXTENSIONS_ALLOWED
#if 0
    /* If decl-modifiers were declared for the class and/or for the
       member, check for consistency and use the union of the two. */
    merge_decl_modifiers(class_type, decl_info,
                         (a_boolean)func_info->is_definition);
#endif /* if 0 */
    update_routine_decl_modifiers(rtn, &decl_info->decl_modifiers,
                                  &locator->source_position,
                                  /*is_redecl=*/FALSE,
                                  (a_boolean)func_info->is_definition,
                                  (a_boolean)func_info->is_inline);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  decl_info->member_sym = sym;
  db_exit();
}  /* decl_member_function_template */


static void scan_pure_specifier(a_symbol_ptr            rout_sym,
                                a_type_ptr              class_type,
                                a_member_decl_info_ptr  decl_info)
/*
The current token is an "=", encountered just after the scanning of a
member or friend function declarator.  A pure specifier is defined as "= 0",
and it is legal for virtual member functions only.
*/
{
  a_boolean          pure_specifier_allowed;

  db_enter(4, "scan_pure_specifier");
  /* A pure specifier is allowed for virtual functions only.  (Check the
     parent class to exclude friend declarations.) */
  if (!rout_sym->is_class_member ||
      rout_sym->parent.class_type != class_type) {
    pure_specifier_allowed = FALSE;
  } else {
    pure_specifier_allowed =
             (rout_sym->kind == (a_symbol_kind)sk_function_template) ?
                  rout_sym->variant.template_info->
                                  variant.function.routine->is_virtual :
                  rout_sym->variant.routine.ptr->is_virtual;
  }  /* if */
  if (!pure_specifier_allowed && !decl_info->invalid_virtual_specifier) {
    pos_error(ec_pure_specifier_on_nonvirtual_function, &pos_curr_token);
  }  /* if */
  /* Advance past the "=". */
  (void)get_token();
  if (curr_token == tok_int_constant && const_for_curr_token.is_simple_zero) {
    /* Token following "=" is "0".  Note that we don't test for an integer
       value of zero but rather for the literal "0", since "= 00" should
       elicit an error. */
    if (pure_specifier_allowed) {
      /* Update the routine and class type entities. */
      rout_sym->variant.routine.ptr->pure_virtual = TRUE;
      class_type->variant.class_struct_union.any_pure_virtual_functions = TRUE;
      class_type->variant.class_struct_union.abstract = TRUE;
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
  } else {
    set_err_pos_to_curr_token();
    /* Invalid pure specifier:  something other than "0" follows the "=". */
    syntax_error(ec_bad_pure_specifier);
  }  /* if */
  db_exit();
}  /* scan_pure_specifier */


static void decl_nonstd_member_constant(a_symbol_locator        *locator,
                                        a_type_ptr              class_type,
                                        a_type_ptr              member_type,
                                        a_class_def_state_ptr   class_state,
                                        a_member_decl_info_ptr  decl_info)
/*
Do processing for a nonstandard member constant, including scanning the
initializer constant and entering the name in the symbol table.  member_type
is guaranteed to be a const-qualified scalar type.  This construct is an
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
  a_symbol_ptr     sym;
  a_constant_ptr   cp;

  db_enter(3, "decl_nonstd_member_constant");
  /* The current token is the "=".  Pointing to it issue a diagnostic that this
     is a nonstandard construct.  This is a strict ANSI diagnostic in
     strict ANSI mode, otherwise it is a warning. */
  diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
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
  set_class_membership(sym, &cp->source_corresp, class_type);
  decl_info->member_sym = sym;
  cp->source_corresp.access = class_state->access;
  record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, sym,
                            &locator->source_position,
                            decl_info->declarator_ssep);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  update_decl_pos_info(&cp->source_corresp, &decl_info->decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Do processing required for any pragmas that are bound to the current
     declaration. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  add_to_constants_list(cp, /*at_file_scope=*/FALSE);
  db_exit();
}  /* decl_nonstd_member_constant */


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
    tp = tp->source_corresp.parent.class_type;
    unnamed = is_or_is_nested_within_unnamed_class(tp);
  }  /* if */
  return unnamed;
}  /* is_or_is_nested_within_unnamed_class */
    

static void decl_static_data_member(a_symbol_locator        *locator,
                                    a_type_ptr              class_type,
                                    a_type_ptr              member_type,
                                    a_class_def_state_ptr   class_state,
                                    a_member_decl_info_ptr  decl_info)
/*
Do processing for a static data member, including entering it in the symbol
table.  *locator is the symbol-locator for the current declaration, class_type
is the class of which it is a member, and *p_member_type is the type with
which the member was declared.  *class_state and *decl_info track general
information about the class definition and specific information about the
member declaration, respectively.
*/
{
  a_symbol_ptr    sym, prototype_tag_sym;
  a_variable_ptr  var;

  db_enter(3, "decl_static_data_member");
  if (is_void_type(member_type)) {
    error(ec_incomplete_type_not_allowed);
    member_type = error_type();
  } else if (is_abstract_class_type(member_type)) {
    /* Abstract class objects are prohibited (ARM 10.3). */
    report_abstract_class_error(ec_abstract_class_object_not_allowed,
                                member_type, &locator->source_position);
  }  /* if */
  if (class_state->is_local_class) {
    /* Static data members are not allowed in local classes. */
    pos_error(ec_static_data_member_not_allowed, &decl_info->decl_start_pos);
    /* Set the type for this invalid static member to error type. This will
       assure "proper" (or unobtrusive) behavior later, if a definition is
       encountered.  It also eliminates semi-spurious error messages if there
       are references to it. */
    member_type = error_type();
  } else if (is_union_type(class_type)) {
    /* Unions are not allowed to have static data members. */
    pos_error(ec_static_data_member_not_allowed, &decl_info->decl_start_pos);
  } else if (!any_cfront_mode() && !microsoft_mode &&
             is_or_is_nested_within_unnamed_class(class_type)) {
    /* Except for cfront or Microsoft compatibility, static data members may
       not be declared in an unnamed class or a class contained within an
       unnamed class (9.4.2 [class.static.data]). However, permit this with
       a warning if anachronisms are enabled. */
    pos_diagnostic(anachronism_error_severity,
                   ec_static_data_member_not_allowed,
                   &decl_info->decl_start_pos);
  }  /* if */
  if (decl_info->is_member_template) set_to_named_error_locator(*locator);
  /* Create the variable entry for the static data member. */
  /* The storage class of static data members is sc_static until they are
     promoted to external linkage, at which time the storage class will
     become sc_extern or sc_unspecified (depending on whether or not a
     definition is provided).  All static data member variables are allocated
     in the file scope memory region and put on the variables list for the
     current class. */
  var = make_variable(member_type, (a_storage_class)sc_static, NO_SCOPE_DEPTH);
  /* If this is a member template declaration, don't add it to the variables
     list (in part to avoid problems caused by an invalid scope). */
  if (!decl_info->is_member_template || prototype_instantiations_in_il) {
    add_to_variables_list(var, decl_scope_level);
  }  /* if */
  sym = enter_local_symbol((a_symbol_kind)sk_static_data_member, locator,
                           decl_scope_level, /*suppress_redecl_error=*/FALSE);
  /* Set the source correspondence fields of the variable. */
  set_source_corresp(&var->source_corresp, sym);
  sym->variant.static_data_member.variable = var;
  set_class_membership(sym, &var->source_corresp, class_type);
  decl_info->member_sym = sym;
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
       or sc_unspecified when during a final fixup pass. */
    var->storage_class = (a_storage_class)sc_extern;
  }  /* if */
  var->source_corresp.access = class_state->access;
  if (curr_token == tok_assign) {
    if ((is_const_qualified_type(member_type) &&
         is_integral_or_enum_type(member_type)) ||
        (class_state->is_nonreal_instantiation &&
         is_template_param_type(member_type))) {
      /* A const integral or const enumeration type may be initialized inside
         the class definition (9.5.2).   This makes the static data member
         usable as a member constant.  Note that the variable entry will
         have an initializer but it is not yet considered defined. */
      a_constant constant;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      decl_info->decl_pos_block.var_init_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Advance past the "=". */
      (void)get_token();
      /* Scan the constant expression. */
      scan_member_constant_initializer_expression(member_type, &constant);
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
    }  /* if */
  }  /* if */
  /* This is entered as a declaration rather than a definition, since the
     definition must appear outside the class definition. */
  record_symbol_declaration(SRK_DECLARATION, sym, &locator->source_position,
                            decl_info->declarator_ssep);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  update_decl_pos_info(&var->source_corresp, &decl_info->decl_pos_block);
  if (var->is_member_constant) {
    var->initializer_range = decl_info->decl_pos_block.var_init_range;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  (void)update_src_seq_secondary_decl((char *)var, member_type, SSSD_NO_FLAGS,
                                      &decl_info->decl_pos_block);
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
        set_class_membership_for_template((a_symbol_ptr)NULL,
                                          templ,
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
  /* If decl-modifiers were declared for the class and/or for the member,
     check for consistency and use the union of the two. */
  merge_decl_modifiers(class_type, decl_info, /*is_definition=*/FALSE);
  update_variable_decl_modifiers(var, &decl_info->decl_modifiers,
                                 &locator->source_position,
                                 /*is_redecl=*/FALSE);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
                                   a_boolean             *is_ref_arg,
                                   a_type_qualifier_set  *qualifiers,
                                   a_boolean             *is_base_class_match)
/*
Return TRUE if sym, an sk_member_function symbol for an operator= function,
qualifies as a "copy assignment operator" (WP 12.8) that can copy a class
object.  It qualifies if its first parameter has a type of "A", "A&", or
"const A&", where "A" is the class of which it is a member.  (In cfront
compatibility mode, sym also qualifies if the first parameter involves type
B where B is a base class of A.)  Set *is_ref_arg to TRUE if the first
parameter is a reference type.  Set *qualifiers based on how the first
parameter is qualified.  Return *is_base_class_match set to TRUE for the
cfront compatibility case.
*/
{
  a_boolean         found = FALSE;
  a_param_type_ptr  ptp;
  a_type_ptr        tp;

  check_assertion(sym->kind == (a_symbol_kind)sk_member_function);
  ptp = routine_symbol_type(sym)->variant.routine.extra_info->param_type_list;
  check_assertion(ptp != NULL);
  tp = skip_typerefs(ptp->type);
  if (is_reference_type(tp)) {
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
    if (skip_typerefs(tp) == sym->parent.class_type) {
      /* The parameter's type matches the class of which the assignment
         operator is a member. */
      found = TRUE;
    } else if (allow_copy_assignment_op_with_base_class_param) {
      if (find_base_class_of(sym->parent.class_type, tp) != NULL) {
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
  return found;
}  /* is_assignment_operator_for_copy */


static a_boolean is_copy_assignment_operator_sym(a_symbol_ptr  sym)
/*
Return TRUE if sym represents a copy assignment operator.
*/
{
  a_boolean             is_copy_assignment_op = FALSE;
  a_routine_ptr         rp;
  a_boolean             is_ref_arg;
  a_type_qualifier_set  qualifiers_accepted;
  a_boolean             is_base_class_match;

  if (sym->kind == (a_symbol_kind)sk_member_function) {
    rp = sym->variant.routine.ptr;
    if (rp->special_kind == (a_special_function_kind)sfk_operator &&
        rp->opname_kind == (an_opname_kind)onk_assign &&
        is_assignment_operator_for_copy(sym, &is_ref_arg,
                                        &qualifiers_accepted,
                                        &is_base_class_match)) {
      is_copy_assignment_op = TRUE;
    }  /* if */
  }  /* if */
  return is_copy_assignment_op;
}  /* is_copy_assignment_operator_sym */


static a_boolean assignment_operator_for_copy_exists(a_symbol_ptr  sym,
                                                     a_boolean     *const_okay)
/*
Return TRUE if sym is not NULL and qualifies as an assignment operator that
can copy a class object (ARM 12.8).  If sym is an overloaded function,
return TRUE if at least one of the functions qualifies.  Set *const_okay
TRUE if a const object can be copied.
*/
{
  a_boolean             sym_is_overloaded;
  a_boolean             is_ref_arg;
  a_type_qualifier_set  qualifiers_accepted;
  a_boolean             found_assignment_operator_for_copy = FALSE;
  a_boolean             is_base_class_match;

  db_enter(4, "assignment_operator_for_copy_exists");
  /* Set *const_okay to TRUE unless this subobject's type has a default
     assignment operator that cannot accept a const object. */
  *const_okay = TRUE;
  if (sym != NULL) {
    sym_is_overloaded = (sym->kind == (a_symbol_kind)sk_overloaded_function);
    if (sym_is_overloaded) sym = sym->variant.overloaded_function.symbols;
    /* Loop through the one or more symbols looking for one with the right
       argument type. */
    for (; sym != NULL; sym = sym_is_overloaded ? sym->next : NULL) {
      if (sym->kind == (a_symbol_kind)sk_member_function) {
        qualifiers_accepted = TQ_NONE;
        if (is_assignment_operator_for_copy(sym, &is_ref_arg,
                                            &qualifiers_accepted,
                                            &is_base_class_match)) {
          /* Found an assignment operator that can serve to make a copy of
             the current class. */
          found_assignment_operator_for_copy = TRUE;
          /* If it takes the object to be copied by value, a const object
             may be copied; if it takes it by reference, a const qualifier
             must be present on the parameter declaration. */
          if (!is_ref_arg || (qualifiers_accepted & TQ_CONST) != 0) {
            /* An copy assignment operator has been located, and it accepts
               a const object. */
            *const_okay = TRUE;
            break;
          } else {
            /* This one does not accept a const object, so set *const_okay
               to FALSE.  However, another in the overload list might accept
               const, so keep looping. */
            *const_okay = FALSE;
          }  /* if */
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
    } else if (cssp->constructor != NULL || cssp->destructor != NULL) {
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
      pos_ty_diagnostic(severity, ec_bad_union_field, pos, tp);
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
                         apo_sym->parent.class_type);
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
  a_symbol_ptr                   sym, next_sym, mf_sym, apo_sym;
  a_class_symbol_supplement_ptr  cssp;
  a_class_type_supplement_ptr    ctsp;
  an_access_specifier            access, assoc_object_access;
  a_boolean                      access_error_already_issued = FALSE;
  a_boolean                      member_function_error_already_issued = FALSE;
  a_boolean                      is_overloaded;
  a_type_ptr                     assoc_object_type, tp;
  a_boolean                      reuse_symbol = TRUE;
  a_boolean                      suppress_reenter_symbol_call;
  a_field_ptr                    au_field;
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
  a_symbol_ptr                   new_apo_sym_list = NULL;
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */

  db_enter(4, "check_anonymous_union_symbols");
  switch (assoc_object_sym->kind) {
    case sk_variable:
      assoc_object_type = assoc_object_sym->variant.variable.ptr->type;
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
    } else {
      /* Save the storage class, which is used by the C++ generating back
         end.  The variable pointer cannot be stored in the class type
         supplement because it need not be in the file-scope memory region,
         but the type and its supplement always are. */
      ctsp->anonymous_union_kind = (an_anonymous_union_kind)auk_variable;
    }  /* if */
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
    cssp->trivial_default_constructor = NULL;
    cssp->assignment_operator = NULL;
    /* Also reset some flags to values that make sense after the union
       is transformed. */
    cssp->has_nontrivial_default_constructor = FALSE;
    cssp->has_user_declared_default_constructor = FALSE;
    cssp->has_copy_constructor = FALSE;
    cssp->has_copy_constructor_for_const_object = FALSE;
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
         features.  (A compiler-generated assignment operator is fine.) */
      check_assertion(sym->kind == (a_symbol_kind)sk_field ||
                      (sym->kind == (a_symbol_kind)sk_member_function &&
                       sym->variant.routine.ptr->compiler_generated));
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
        apo_sym = sym->variant.field.anonymous_parent_object;
        suppress_reenter_symbol_call = FALSE;
        if (reuse_symbol) {
          /* Unlink the symbol from the inactive list and link it back into
             the symbol table in the current scope. */
          remove_anonymous_union_member_from_inactive_symbols_list(sym);
          if (microsoft_bugs && class_type != NULL) {
            /* The Microsoft compiler does not diagnose promoting an
               anonymous union member into a scope in which its name has
               already been declared.  Emulate the behavior by suppressing
               the reenter_symbol call. */
            a_symbol_locator  locator;
            a_symbol_ptr      other_sym;

            clear_locator(&locator, &sym->decl_position);
            locator.symbol_header = sym->header;
            other_sym = class_qualified_id_lookup(&locator, class_type,
                                               IDL_DIRECT_CLASS_MEMBERS_ONLY);
            if (other_sym != NULL && !is_tag_symbol(other_sym)) {
              pos_st_warning(ec_id_already_declared, &(sym->decl_position),
                             sym->header->identifier);
              suppress_reenter_symbol_call = TRUE;
            }  /* if */
          }  /* if */
          if (!suppress_reenter_symbol_call) {
            /* Enter the symbol back into the current scope. */
            reenter_symbol(sym, depth_scope_stack, /*suppress_error=*/FALSE);
          }  /* if */
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
        } else {
          /* The symbol has to be kept bound to the type, since the latter
             may be used again.  Therefore, we have to clone the symbol,
             making a copy of it in the new class scope.  Note that there may
             turn out to be a many-to-one mapping between member symbols and
             field-of-assoc-object-type. */
          a_field_ptr      fp = sym->variant.field.ptr;
          a_symbol_locator loc;

          make_locator_for_symbol(sym, &loc);
          loc.source_position = assoc_object_sym->decl_position;
          sym = enter_local_symbol(sym->kind, &loc, depth_scope_stack,
                                   /*suppress_error=*/FALSE);
          sym->variant.field.ptr = fp;
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
        }  /* if */
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
        sym->variant.field.ptr->source_corresp.access = assoc_object_access;
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
              apo_sym->variant.field.
                         anonymous_parent_object = assoc_object_sym;
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
                                                           &new_apo_sym_list,
                                                           assoc_object_sym);
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
        }  /* if */
        break;
      case sk_member_function:
      case sk_overloaded_function:
      case sk_function_template:
        /* Remove the symbol and don't reenter it. */
        remove_anonymous_union_member_from_inactive_symbols_list(sym);
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
        if (!(microsoft_mode || sun_mode || any_cfront_mode())) {
          pos_diagnostic(strict_ansi_mode ?
                           strict_ansi_discretionary_severity : es_warning,
                         ec_type_decl_in_anon_union, &sym->decl_position);
        }  /* if */
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
#if CHECKING
      default:
        internal_error("check_anonymous_union_symbols: unexpected sym kind");
#endif /* CHECKING */
    }  /* switch */
#if RECORD_HIDDEN_NAMES_IN_IL
    if (class_type == NULL &&
        (sym->decl_scope == file_scope_number ||
         sym->parent.namespace_ptr != NULL)) {
      /* Set a flag in the symbol header to indicate that at least one
         declaration with this name appeared in the file scope or a
         namespace scope.  The information is used in building the hidden
         name table. */
      sym->header->any_decl_in_file_or_namespace_scope = TRUE;
    }  /* if */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
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

static a_boolean is_anonymous_union_decl(a_type_ptr              member_type,
                                         a_member_decl_info_ptr  decl_info)
/*
A declaration has appeared in which there is no declarator.  Return TRUE if
it is an anonymous union declaration.  If ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
is TRUE and this is not a standard C++ anonymous union, return TRUE and
also set the is_nonstd_anonymous_union flag in the member-decl-info block.
*/
{
  if (!C_mode() &&
      member_type->kind == (a_type_kind)tk_union) {
    if (member_type->source_corresp.name != NULL ||
        !(decl_info->dso_flags & DSO_DEFINES_SOMETHING)) {
      /* This union was named and/or is a reference to a previously defined
         type -- in any case, it's not an anonymous union. */
    } else if (decl_info->dso_flags & (DSO_DECLARES_SOMETHING | DSO_FRIEND)) {
      /* This cannot be a standard or a nonstandard anonymous union in C++. */
    } else {
      decl_info->is_anonymous_union = TRUE;
    }  /* if */
#if ALLOW_NONSTANDARD_ANONYMOUS_UNIONS
  } else if (!allow_nonstandard_anonymous_unions) {
    /* This compilation is not configured to support this extension -- e.g.,
       this is not Microsoft mode. */
  } else if (!is_class_struct_union_type(member_type)) {
    /* Not a pseudo-anonymous-union -- it's not a class, struct,
       or union type. */
  } else if (!C_mode() &&
             ((decl_info->dso_flags & (DSO_DECLARES_SOMETHING | DSO_FRIEND)) ||
              !(skip_typerefs(member_type))->
                            variant.class_struct_union.originally_unnamed)) {
    /* Not a pseudo-anonymous-union -- either a tag appeared on the current
       declaration, or it's a typedef name and a tag was declared originally,
       or else it's a C++ friend declaration. */
  } else {
    /* This may in fact be an anonymous-union-like construct. */
    /* Skip the typedefs but not cv qualifiers. */
    a_type_ptr  tp = skip_typedefs(member_type);

    if (tp->kind == (a_type_kind)tk_typeref && !microsoft_mode) {
      /* This must be a cv qualifier on top of what we already know to be a
         class, struct, or union type.  The qualifier disqualifies it from
         being treated as an anonymous-union-like construct. */
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
        if (cssp->is_class_aggregate) {
          if ((sym = cssp->symbols) != NULL) {
            /* Assume. */
            decl_info->is_anonymous_union = TRUE;
            for (; sym != NULL; sym = sym->next_in_scope) {
              if (sym->kind == (a_symbol_kind)sk_field) {
                /* Okay. */
              } else if (sym == cssp->trivial_default_constructor) {
                /* Okay. */
              } else if (is_type_symbol(sym) && tp == member_type) {
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

  enum_con = bit_field_type->variant.integer.enum_info.constant_list;
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
           target preference.  Make a one-bit field always unsigned. */
        if (bit_field_size == 1) {
          use_signed = FALSE;
        } else {
          use_signed = !targ_plain_int_bit_field_is_unsigned;
        }  /* if */
      }  /* if */
      if (use_signed && sign_of_integer_constant(&largest) >= 0) {
        /* Using a signed bit field and the largest is nonnegative, so the
           largest really requires one more bit for a zero sign. */
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


static void scan_bit_field_size(a_field_ptr       field,
                                a_boolean         *unnamed_bit_field,
                                a_type_ptr        *p_base_type,
                                a_symbol_locator  *locator)
/*
Scan the size in a bit-field declaration:

    unsigned int j: 5 ;
                    ^---- this size.

The current token is the colon preceding the size.  If *unnamed_bit_field
is TRUE, the bit-field is unnamed.  *p_base_type gives the base type
of the declaration (unsigned int in the above example); it may be updated
on return.  *p_bit_field_size is set to the bit field size in bits.
*p_is_signed is set to indicate whether or not the bit field is signed.
*/
{
  unsigned long    bit_field_size, max_size_allowed;
  a_type_ptr       base_type = *p_base_type;
  a_boolean        err = FALSE, is_signed = FALSE;
  a_constant       constant;
  a_type_ptr       bit_field_type;
  an_integer_kind  int_kind;

  db_enter(3, "scan_bit_field_size");
  /* ANSI C says the type of a bit-field must be int, unsigned int,
     or signed int, but we also allow enums and integral types (see A.6.5.8
     in the Common Extensions appendix).  pcc and C++ (ARM 9.6) allow any
     integral or enum type. */
  bit_field_type = skip_typerefs(base_type);
  if (!is_integral_or_enum_type(bit_field_type)) {
    /* Diagnostic has already been issued. */
    bit_field_type = integer_type((an_integer_kind)ik_int);
  }  /* if */
  /* Note that if the base type was not integral it has been replaced by
     "int" by this point. */
  /* Advance past the colon. */
  (void)get_token();
  /* Scan the integral size in bits of the bit-field. */
  scan_fs_integral_constant_expression(&constant);
#if RECORD_CONSTANT_EXPRESSIONS_IN_IL
  field->bit_size_constant = alloc_shareable_constant(&constant);
#endif /* RECORD_CONSTANT_EXPRESSIONS_IN_IL */
  if (is_error_constant(&constant)) {
    /* Use small value to avoid more errors, but not 1 which is special. */
    bit_field_size = targ_char_bit;
    err = TRUE;
  } else if (constant.kind == (a_constant_repr_kind)ck_template_param) {
    /* A template parameter during the prototype instantiation.  The value
       is not known.  Use a small value that is not 1. */
    bit_field_size = targ_char_bit;
  } else {
#if CHECKING
    if (constant.kind != (a_constant_repr_kind)ck_integer) {
      internal_error("scan_bit_field_size: size not int");
    }  /* if */
#endif /* CHECKING */
    /* The size of the bit field must be non-negative and must not exceed
       the size of the underlying type (except for enums, whose type was
       picked by the front end). */
    if (bit_field_type->variant.integer.enum_type) {
      /* An enum type.  We don't use the underlying integer type because that
         is not directly specified by the user and reporting a conflict
         between the specified bit-size and the capacity of the underlying
         type could produce surprising diagnostics.  So, simply use the
         largest number of bits any enum could have. */
      if (!enum_types_can_be_larger_than_int) {
        max_size_allowed = (unsigned long)(targ_sizeof_int*targ_char_bit);
      } else {
#if LONG_LONG_ALLOWED
        max_size_allowed = (unsigned long)
                                         (targ_sizeof_long_long*targ_char_bit);
#else /* !LONG_LONG_ALLOWED */
        max_size_allowed = (unsigned long)(targ_sizeof_long*targ_char_bit);
#endif /* LONG_LONG_ALLOWED */
      }  /* if */
    } else {
      /* Normal case.  Number of bits cannot exceed the capacity of the
         bit field type. */
      max_size_allowed = (unsigned long)(bit_field_type->size*targ_char_bit);
    }  /* if */
    bit_field_size = (unsigned long)
                           unsigned_value_of_integer_constant(&constant, &err);
    /* Note that one reason for err to be TRUE is if the constant is
       less than zero. */
    if (err || bit_field_size > max_size_allowed) {
      if (err || C_mode()) {
        error(ec_bad_bit_field_size);
      } else if (bit_field_size > max_size_allowed) {
        /* A warning in C++. */
        char  buffer[8];
        sprintf(buffer, "%ld", max_size_allowed);
        pos_st_warning(ec_extra_bits_ignored, &error_position, buffer);
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
  if (bit_field_type->variant.integer.enum_type) {
    /* The integral type is an enum type.  Give a warning if any of the
       enumeration's constants will not fit in the bit field, and determine
       whether the bit field should be signed or unsigned. */
    check_enum_type_for_bit_field(bit_field_type, bit_field_size, &is_signed);
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
      if (C_dialect == C_dialect_pcc &&
          targ_plain_int_bit_field_is_unsigned) {
        /* In pcc mode when the environment expects plain-int bit fields to
           be unsigned, change the underlying type to reflect that -- this
           produces more accurate IL for expressions in which integral
           promotion is not done. */
        int_kind = unsigned_int_kind_of[int_kind];
        bit_field_type = integer_type(int_kind);
        is_signed = FALSE;
      } else if (bit_field_size > 1 &&
                 !targ_plain_int_bit_field_is_unsigned &&
                 !any_cfront_mode()) {
        /* Keep the default signedness of the plain integral type.  Note that
           cfront treats all bit fields as unsigned. */
        is_signed = TRUE;
      } else {
        /* The default for plain integral types in bit fields is unsigned --
           or else this is a one-bit bit field, for which anything but
           unsigned may not make much sense.  However, we do not change the
           type to an unsigned version of the same integral type, since that
           would affect C++ overload resolution adversely; the code to do
           integral promotion has special handling if the width of the bit
           field is the same as the width of an integer. */
        is_signed = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* Give a warning for a signed one-bit field; ANSI C allows it, but it's
     strange. */
  if (!err && !*unnamed_bit_field && is_signed && bit_field_size == 1) {
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
  field->bit_field_is_signed = is_signed;

  db_exit();
}  /* scan_bit_field_size */


static void check_field_type(a_symbol_locator        *locator,
                             a_type_ptr              *member_type,
                             a_class_def_state_ptr   class_state,
                             a_member_decl_info_ptr  decl_info)

/*
Check that the type of a nonstatic data member is valid, and report incomplete
types and incorrect types on bit-field declarations.  *locator is the symbol
locator for the field being declared, and *member_type is its type.
*class_state and *decl_info track general information about the class
definition and specific information about the member declaration,
respectively.
*/
{
  a_type_ptr  field_type = *member_type;
  a_type_ptr  class_type = class_state->class_type;

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
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (microsoft_mode) {
    /* In Microsoft mode a class or struct may include a member whose type
       contains a final field that is an unknown-size array, but only if the
       member with such a type is the last field.  If the previous field was
       of such a type, no error was issued, in case it was the last field;
       issue the error now. */
    if (!is_union_type(class_type) &&
        class_type->variant.class_struct_union.
                              contains_flexible_array_member) {
      a_field_ptr  prev_field = class_state->end_of_field_list;

      check_assertion(prev_field != NULL &&
                      is_class_struct_union_type(prev_field->type) &&
                      skip_typerefs(prev_field->type)->
                                        variant.class_struct_union.
                                        contains_flexible_array_member);
      pos_error(ec_flexible_array_member_not_allowed,
                &prev_field->source_corresp.decl_position);
      prev_field->type = error_type();
      class_type->variant.class_struct_union.
                                  contains_flexible_array_member = FALSE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  /* The type specified must be complete. */
  complete_type_is_needed(field_type);
  if (C_mode() && is_function_type(field_type) &&
      decl_info->storage_class != (a_storage_class)sc_typedef) {
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
       extension otherwise in C mode, and in Microsoft C++ mode as long as
       the class has no virtual base classes. */
    if (C_mode() ||
        (microsoft_mode &&
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
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else {
          /* The field has an incomplete array type, and it's a member of a
             struct or class.  This can sometimes be okay -- in Microsoft
             mode (both C and C++) and, as long as it's not the first named
             field, in C99 mode.  As an extension, this is supported in
             other C modes (except in strict C89 mode). */
          if ((!class_state->is_first_field &&
               class_state->any_named_fields) ||
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
            if (microsoft_mode &&
                (decl_info->decl_modifiers.get_property_name != NULL ||
                 decl_info->decl_modifiers.put_property_name != NULL)) {
              /* This is a property field: no need to guard against
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
    } else {
      if (!C_mode() && is_error_locator(*locator) &&
          !decl_info->is_unnamed_field) {
        /* Don't issue an error since we can't be sure this was intended to
           be a field -- it could be an ill-formed function declaration with
           a void return type, such as
             void operator?:();
           in which the param list is not processed. */
      } else {
        pos_error(ec_incomplete_type_not_allowed, &locator->source_position);
      }  /* if */
      field_type = error_type();
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
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (microsoft_mode) {
      /* In Microsoft mode the error is issued only if the struct containing
         a flexible array member is not the last member.  Just set the flag
         for now and do the check later. */
      class_type->variant.class_struct_union.
                                  contains_flexible_array_member = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
    if (is_abstract_class_type(field_type)) {
      /* Abstract class objects are prohibited (ARM 10.3). */
      report_abstract_class_error(ec_abstract_class_object_not_allowed,
                                  field_type, &locator->source_position);
    } else if (strict_ansi_mode && is_union_type(class_type) &&
               is_reference_type(field_type)) {
      /* Unions are not allowed to have members of reference type. */
      pos_diagnostic(strict_ansi_error_severity, ec_ref_not_allowed_in_union,
                     &decl_info->decl_start_pos);
      if ((int)strict_ansi_error_severity > (int)es_warning) {
        field_type = error_type();
      } /* if */
    }  /* if */
  }  /* if */
  if (curr_token == tok_colon) {
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
        pos_error(ec_bad_bit_field_type, &decl_info->decl_start_pos);
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
                         &decl_info->decl_start_pos);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  *member_type = field_type;
}  /* check_field_type */


static void decl_nonstatic_data_member(a_symbol_locator        *locator,
                                       a_type_ptr              class_type,
                                       a_type_ptr              member_type,
                                       a_class_def_state_ptr   class_state,
                                       a_member_decl_info_ptr  decl_info)
/*
Scan a nonstatic data member of a class, struct, or union, create a field
entry to represent it in the IL, and create an entry in the symbol table
for it if it has a name.  class_type is a pointer to the tk_class,
tk_struct, or tk_union type entry for the entity of which the member is a
member.  *locator is the symbol locator for the declaration.  *class_state
and *decl_info track general information about the class definition and
specific information about the member declaration, respectively.
*/
{
  a_field_ptr                    field;
  a_symbol_ptr                   member_sym = NULL;
  a_class_symbol_supplement_ptr  cssp;
  a_boolean                      unnamed_field = decl_info->is_unnamed_field;

  db_enter(3, "decl_nonstatic_data_member");
  if (decl_info->is_member_template) {
    /* Error -- suppress incomplete-type errors, etc.. */
    member_type = error_type();
    set_to_named_error_locator(*locator);
  } else {
    /* Do error checking on the type. */
    check_field_type(locator, &member_type, class_state, decl_info);
  }  /* if */
  /* Set the flag to record that at least one named field was encountered. */
  if (!decl_info->is_unnamed_field) class_state->any_named_fields = TRUE;
  if (!C_mode() && class_type->kind == (a_type_kind)tk_union &&
      !decl_info->is_anonymous_union) {
    /* An object of a class with a constructor, a destructor, or a user-
       defined assignment operator cannot be a member of a union. */
    if (!is_valid_union_field(member_type, &locator->source_position)) {
      member_type = error_type();
    }  /* if */
  }  /* if */
  /* Create the field entry. */
  field = alloc_field();
  /* A colon next indicates a bit-field. */
  if (curr_token == tok_colon) {
    /* Scan the bit-field size and determine the bit-field type. */
    scan_bit_field_size(field, &unnamed_field, &member_type, locator);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    decl_info->decl_pos_block.declarator_range.end =
                                            curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    field->is_bit_field = TRUE;
  }  /* if */
  /* Copy the type (which may have been changed by scan_bit_field_size) into
     the field entry. */
  field->type = member_type;
  /* For an unnamed field, do not create the field symbol. */
  if (unnamed_field) {
    /* All field entries for an unnamed fields share the same symbol.  It is
       used for easy identification. */
    field->source_corresp.assoc_info = (char *)unnamed_field_symbol();
    /* Update the source correspondence information manually -- there's no
       symbol. */
    field->source_corresp.decl_position = locator->source_position;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Ordinarily we create source sequence entries only for named
       entities (see sym_update_source_sequence_list, called for fields
       from record_symbol_declaration).  An exception is made for unnamed
       fields; call the subroutine directly. */
    update_source_sequence_list((char *)field, (an_il_entry_kind)iek_field,
                                decl_info->declarator_ssep);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (decl_info->is_member_template) {
      pos_error(ec_bad_member_template_decl, &decl_info->decl_start_pos);
    }  /* if */
  } else {
    /* Create the field symbol. */
    if (decl_info->is_anonymous_union) {
      member_sym = make_anonymous_parent_object_symbol(
                                       (a_symbol_kind)sk_field,
                                       &locator->source_position,
                                       scope_stack[depth_scope_stack].number);
      field->is_anonymous_parent_object = TRUE;
    } else {
      member_sym = enter_local_symbol((a_symbol_kind)sk_field, locator,
                                      depth_scope_stack,
                                      /*suppress_redecl_error=*/FALSE);
      set_source_corresp(&(field->source_corresp), member_sym);
    }  /* if */
    member_sym->variant.field.ptr = field;
    decl_info->member_sym = member_sym;
  }  /* if */
  /* Set the parent class in the field and (unless member_sym is NULL) in the
     symbol. */
  set_class_membership(member_sym, &field->source_corresp, class_type);
  if (decl_info->is_member_template && locator->symbol_header != NULL) {
    pos_sy_error(ec_bad_member_template_sym, &locator->source_position,
                 member_sym);
  }  /* if */
  if (C_dialect == C_dialect_cplusplus) {
    field->source_corresp.access = class_state->access;
    if (decl_info->dso_flags & DSO_MUTABLE) {
      /* The member is declared "mutable". */
      field->is_mutable = TRUE;
      class_type->variant.class_struct_union.any_mutable_member = TRUE;
    }  /* if */
  }  /* if */
  if (member_sym != NULL && !decl_info->is_anonymous_union) {
    record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, member_sym,
                              &locator->source_position,
                              decl_info->declarator_ssep);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    update_decl_pos_info(&field->source_corresp, &decl_info->decl_pos_block);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Do processing required for any pragmas that are bound to the current
       declaration. */
    process_curr_construct_pragmas(member_sym, (a_statement_ptr)NULL);
  } else {
    /* Issue diagnostics on pragmas that are trying to bind to an unnamed
       field. */
    cannot_bind_to_curr_construct();
  }  /* if */
  /* Add the field to the temporary list for this class/struct/union. */
  if (class_state->end_of_field_list == NULL) {
    class_type->variant.class_struct_union.field_list = field;
  } else {
    class_state->end_of_field_list->next = field;
  }  /* if */
  class_state->end_of_field_list = field;
  if (C_dialect == C_dialect_cplusplus) {
    /* In C++ we need to keep track of whether any members have reference
       type. */
    cssp = symbol_supplement_for_class(class_type);
    if (is_reference_type(member_type)) {
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
    if (decl_info->decl_modifiers.get_property_name != NULL ||
        decl_info->decl_modifiers.put_property_name != NULL) {
      /* This declaration includes __declspec(property(...)).  This is
         valid only on nonstatic data members that are not bit fields. */
      if (field->is_bit_field) {
        pos_diagnostic(es_discretionary_error,
                       ec_declspec_property_not_allowed,
                       &locator->source_position);
      } else {
        field->get_property_name = decl_info->decl_modifiers.get_property_name;
        field->put_property_name = decl_info->decl_modifiers.put_property_name;
      }  /* if */
    }  /* if */
    if (decl_info->decl_modifiers.allocate_segname != NULL) {
      /* Only allowed for variables with static storage duration. */
      pos_error(ec_declspec_allocate_not_allowed, &locator->source_position);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  /* Remember if any member of the class, struct, or union is const-
     qualified, including recursively the members of any contained
     classes, structs, or unions.  This is useful for determination of
     modifiable lvalues (see 3.2.2.1). */
  if (is_const_qualified_type(member_type) ||
      (is_class_struct_union_type(member_type) &&
       skip_typerefs(member_type)->
                            variant.class_struct_union.any_const_member)) {
    class_type->variant.class_struct_union.any_const_member = TRUE;
    if (C_dialect == C_dialect_cplusplus) {
      /* Assignment by bitwise copy is not allowed when a class has const
         qualified members. */
      cssp->assignment_by_bitwise_copy_allowed = FALSE;
    }  /* if */
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
    a_type_ptr  tp = skip_typerefs(member_type);
    if (is_array_type(tp)) {
      tp = f_skip_typerefs(underlying_array_element_type(tp));
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
        /* If a nonstatic data member of a class is itself a class object
           (or an array whose elements are class objects) and the subobject
           has a constructor and/or destructor, the containing class is
           also required to have a constructor and/or destructor.  Do the
           check at this time, and record the requirement, if any. */
        if (member_cssp->constructor != NULL) {
          class_state->constructor_required = TRUE;
        }  /* if */
        if (member_cssp->destructor != NULL) {
          class_state->destructor_required = TRUE;
        }  /* if */
        /* The parent class cannot be copy-constructed or assigned by bitwise
           copying if the member class does not allow it. */
        if (!member_cssp->construction_by_bitwise_copy_allowed) {
          cssp->construction_by_bitwise_copy_allowed = FALSE;
        }  /* if */
        if (!member_cssp->assignment_by_bitwise_copy_allowed) {
          cssp->assignment_by_bitwise_copy_allowed = FALSE;
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
    }  /* if */
  } else if (C_dialect == C_dialect_cplusplus && !class_state->POD_ruled_out) {
    if (is_reference_type(member_type)) {
      /* A POD may not have a field with a reference type. */
      class_state->POD_ruled_out = TRUE;
    } else {
      /* A POD may not have a field with a type that is a pointer-to-member
         (or array thereof). */
      a_type_ptr  tp = member_type;
      if (is_array_type(tp)) tp = underlying_array_element_type(tp);
      if (is_ptr_to_member_type(tp)) class_state->POD_ruled_out = TRUE;
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
      if (decl_info->is_unnamed_field) {
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
    } else if (decl_info->is_unnamed_field) {
      /* Ignore unnamed fields. */
    } else if (is_reference_type(member_type) ||
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
}  /* decl_nonstatic_data_member */


static void generate_special_function(a_type_ptr               class_type,
                                      a_class_def_state_ptr    class_state,
                                      a_member_decl_info_ptr   decl_info,
                                      a_param_type_ptr         ptp)
/*
Create a routine entry for a compiler generated constructor, destructor, or
assignment operator.  The created routine is a member function of the class
specified by class_type.  If it has any parameter besides the implicit "this"
parameter (i.e., for a copy constructor or assignment operator), a non-NULL
param type pointer is passed in as ptp.  *decl_info tracks information about
the declaration, including whether a constructor, destructor, or assignment
operator should be created.  No routine body is generated at this time.
*/
{
  a_type_ptr                rout_type;
  a_routine_type_supplement *extra_info;
  a_symbol_locator          locator;
  a_func_info_block         func_info;
  a_source_position         *class_decl_pos;
  a_routine_ptr             routine;

  db_enter(3, "generate_special_function");
  /* Allocate and initialize the routine type entry for the function. */
  rout_type = alloc_type((a_type_kind)tk_routine);
  extra_info = rout_type->variant.routine.extra_info;
  if (decl_info->is_destructor) {
    /* Destructors are given a return type of void. */
    rout_type->variant.routine.return_type = void_type();
    extra_info->assoc_routine_is_dtor = TRUE;
  } else {
    /* Constructors and default assignment operators are given a return type
       of reference to class-type. */
    rout_type->variant.routine.return_type = make_reference_type(class_type);
    if (decl_info->is_constructor) {
      extra_info->assoc_routine_is_ctor = TRUE;
    }  /* if */
  }  /* if */
  if (ptp != NULL) {
    /* Set a flag in the param type entry if its associated type is or contains
       a template parameter. */
    if (is_or_contains_template_param(class_type)) {
      ptp->type_involves_deduced_template_param = TRUE;
    }  /* if */
  }  /* if */
  extra_info->param_type_list = ptp;
  extra_info->this_class = class_type;
  extra_info->prototyped = TRUE;
  /* Check whether the routine needs special support for returning a class
     object by value.  This call should be superfluous; it is included just
     to be safe, in case the rules change on when the flag needs to be set. */
  set_routine_calling_method_flag(rout_type, &null_source_position);
  /* Create a locator for the symbol that will be created. */
  class_decl_pos = &class_type->source_corresp.decl_position;
  if (decl_info->is_constructor || decl_info->is_destructor) {
    a_symbol_ptr tag_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;

    make_locator_for_symbol(tag_sym, &locator);
    if (decl_info->is_constructor) {
      change_class_locator_into_constructor_locator(&locator, class_decl_pos);
    } else {
      tildize_locator(&locator);
    }  /* if */
  } else {
    /* Must be an assignment operator. */
    make_opname_locator((an_opname_kind)onk_assign, &locator, class_decl_pos);
  }  /* if */
  clear_func_info(&func_info);
  func_info.is_inline = TRUE;
  if (exceptions_enabled) func_info.throw_position = *class_decl_pos;
  /* Create a symbol and enter it in the symbol table, and create a routine
     entry and add it to the routines list for the current scope. */
  decl_member_function(&locator, class_type, rout_type, &func_info,
                       class_state, decl_info, /*compiler_generated=*/TRUE);
  done_with_func_info(func_info);
  /* It can be that the head of symbols list for the scope has been
     modified (it may have been changed to an sk_overloaded_function, or
     it may have been empty), so update the class symbol supplement, just to
     be safe. */
  (symbol_supplement_for_class(class_type))->symbols =
            assoc_pointers_block_of(&scope_stack[depth_scope_stack])->symbols;
  check_assertion(decl_info->member_sym != NULL);
  routine = decl_info->member_sym->variant.routine.ptr;
  if (instantiate_extern_inline && !routine->is_prototype_instantiation) {
    /* When inline functions are instantiated like templates, add the function
       to the list of inline functions if it is inline.  (Members of prototype
       instantiations don't need to be treated that way, of course.) */
    add_to_inline_function_list(decl_info->member_sym->variant.routine.ptr);
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
#if DELETE_CAN_BE_FOLDED_INTO_DTOR

void set_class_assoc_operator_delete_routine(a_type_ptr     class_type,
                                             a_routine_ptr  dtor_rout)
/*
Determine the operator delete() function to be used for the indicated class
and record it in the class's assoc_operator_delete_routine field.  If
dtor_rout is non-NULL, it indicates a destructor for which the delete
function is potentially part of the wrapper code.
*/
{
  a_symbol_ptr                sym;
  a_class_type_supplement_ptr ctsp;
  a_boolean                   ambiguous;

  check_assertion(is_immediate_class_type(class_type));
  ctsp = class_type->variant.class_struct_union.extra_info;
  if (ctsp->assoc_operator_delete_routine == NULL) {
    /* Use the class "delete" if there is one, and otherwise the global
       operator delete. */
    sym = opname_member_function_symbol((an_opname_kind)onk_delete,
                                        class_type);
    if (sym != NULL) {
      /* A member delete. */
      if (sym->ambiguous) {
        if (dtor_rout != NULL && dtor_rout->is_virtual) {
          /* Only issue the diagnostic if the destructor is virtual.  For
             nonvirtual destructors (or for implicit deallocation when an
             exception occurs in the midst of construction) the diagnostic is
             issued when the delete (or new) expression is processed. */
          check_assertion(dtor_rout->special_kind ==
                                    (a_special_function_kind)sfk_destructor);
          pos_sy2_error(ec_implicit_call_of_ambiguous_name,
                        &error_position, sym,
                        (a_symbol_ptr)dtor_rout->source_corresp.assoc_info);
        }  /* if */
        /* Don't return an ambiguous function. */
        sym = NULL;
      }  /* if */
    } else {
      sym = opname_function_symbol((an_opname_kind)onk_delete);
      check_assertion(sym != NULL);
    }  /* if */
    if (sym != NULL) {
      /* Since delete might be overloaded, find the default version. */
      sym = find_default_operator_delete_sym(sym, &ambiguous);
      if (sym != NULL) {
        sym = fundamental_symbol_of(sym);
        ctsp->assoc_operator_delete_routine = sym->variant.routine.ptr;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* set_class_assoc_operator_delete_routine */

#endif /* DELETE_CAN_BE_FOLDED_INTO_DTOR */

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
      if (assignment_operator_for_copy_exists(cssp->assignment_operator,
                                              &const_okay) &&
          !const_okay) {
        /* There is a default assignment operator for this base class type,
           but it does not accept a const object.  No need to look any
           further. */
        goto done;
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
        if (assignment_operator_for_copy_exists(cssp->assignment_operator,
                                                &const_okay) &&
            !const_okay) {
          /* There is a default assignment operator for this static data
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
  a_boolean                     const_okay, dummy_flag;
  a_type_qualifier_set          qualifiers;
  a_member_decl_info            decl_info;
  a_source_position             *pos;
  a_boolean                     user_declared_copy_assignment_op = FALSE;

  db_enter(3, "check_special_member_functions");
  cssp = symbol_supplement_for_class(class_type);
  pos = &class_type->source_corresp.decl_position;
  /* Check for a user-declared copy assignment operator. */
  if (assignment_operator_for_copy_exists(cssp->assignment_operator,
                                          &dummy_flag)) {
    /* If the user has already defined an assignment operator, neither
       is bitwise copying allowed nor must the compiler generate one. */
    cssp->assignment_by_bitwise_copy_allowed = FALSE;
    user_declared_copy_assignment_op = TRUE;
    /* A POD cannot have a user-defined copy assignment operator. */
    class_state->POD_ruled_out = TRUE;
  }  /* if */
  if (cssp->constructor == NULL) {
    if (!class_state->POD_ruled_out) {
      /* This is a POD class.  Its implicitly-declared default constructor
         need not actually be generated. */
    } else {
      /* A default constructor needs to be generated. */
      initialize_member_decl_info(&decl_info, pos);
      decl_info.is_constructor = TRUE;
      if (!class_state->constructor_required) {
        /* We are generating a declaration of a trivial default constructor.
           Since it will never actually be called it gets special handling. */
        decl_info.is_trivial_default_constructor = TRUE;
      }  /* if */
      generate_special_function(class_type, class_state, &decl_info,
                                (a_param_type_ptr)NULL);
    }  /* if */
  }  /* if */
  if (cssp->constructor != NULL && !cssp->has_copy_constructor) {
    default_copy_constructor_check(class_type, &const_okay);
    /* Generate a copy constructor. */
    qualifiers = const_okay ? TQ_CONST : TQ_NONE;
    ptp = alloc_param_type(make_reference_type(
                               make_qualified_type(class_type, qualifiers)));
    /* Set a flag in the param type entry if its associated type is or contains
       a template parameter. */
    ptp->type_involves_deduced_template_param =
                                  is_or_contains_template_param(class_type);
    initialize_member_decl_info(&decl_info, pos);
    decl_info.is_constructor = TRUE;
    generate_special_function(class_type, class_state, &decl_info, ptp);
  }  /* if */
  if (class_state->destructor_required && cssp->destructor == NULL) {
    initialize_member_decl_info(&decl_info, pos);
    decl_info.is_destructor = TRUE;
    generate_special_function(class_type, class_state, &decl_info,
                              (a_param_type_ptr)NULL);
  }  /* if */
  /* Create a default assignment operator to copy an object of the current
     class if one doesn't already exist.  Note that in cfront mode, the
     presence of any assignment operator suppresses the creation of
     a default assignment operator. */
  if (!user_declared_copy_assignment_op &&
      (!any_cfront_mode() || cssp->assignment_operator == NULL)) {
    /* An implicit assignment operator is generated if the class does not
       contain a user-declared copy assignment operator. */
    a_type_ptr this_type;
    const_okay = default_assignment_of_const_object_okay(class_type);
    qualifiers = const_okay ? TQ_CONST : TQ_NONE;
    this_type = make_qualified_type(class_type, qualifiers);
    ptp = alloc_param_type(make_reference_type(this_type));
    /* Set a flag in the param type entry if its associated type is or
       contains a template parameter. */
    ptp->type_involves_deduced_template_param =
                                is_or_contains_template_param(class_type);
    initialize_member_decl_info(&decl_info, pos);
    generate_special_function(class_type, class_state, &decl_info, ptp);
#if NEAR_AND_FAR_ALLOWED
    if (near_and_far_enabled()) {
      /* Generate also an operator= that can copy a "far" object. */
      a_param_type_ptr ptp_far;
      a_type_ptr       this_type_far;
      this_type_far = make_qualified_type(class_type, TQ_CONST|TQ_FAR);
      /* Don't create the "far" operator= if the default one is "far"
         (e.g., because the class is declared "far"). */
      if (!identical_types(this_type_far, this_type)) {
        ptp_far = alloc_param_type(make_reference_type(this_type_far));
        /* Set a flag in the param type entry if its associated type is or
           contains a template parameter. */
        ptp_far->type_involves_deduced_template_param =
                                     ptp->type_involves_deduced_template_param;
        initialize_member_decl_info(&decl_info, pos);
        generate_special_function(class_type, class_state, &decl_info,
                                  ptp_far);
      }  /* if */
    }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
  }  /* if */
  db_exit();
}  /* check_special_member_functions */


static a_boolean has_more_than_one_direct_base_class(a_type_ptr  class_type)
/*
Return TRUE if the base class list for class_type includes more than one
entry marked "direct".
*/
{
  a_base_class_ptr  bcp = base_classes_of(class_type);
  int               count = 0;

  for (; bcp != NULL; bcp = bcp->next) {
    if (bcp->direct) {
      if (++count > 1) break;
    }  /* if */
  }  /* for */
  return (count > 1);
}  /* has_more_than_one_direct_base_class */


static void check_base_class_destructors(a_type_ptr  class_type)
/*
Issue a diagnostic on any direct base class of class_type that has a
nonvirtual destructor.  The point is to warn the user in case the program
should attempt to delete an object of type class_type through a pointer to
one of its direct base classes.
*/
{
  a_base_class_ptr               bcp;
  a_class_symbol_supplement_ptr  cssp;
  a_symbol_ptr                   dtor_sym;

  if ((int)error_threshold > (int)es_remark) {
    /* Only a remark would have been issued.  Don't bother checking for a
       diagnosable condition. */
  } else if ((bcp = base_classes_of(class_type)) != NULL) {
    /* class_type does have base classes.  Be sure class_type is not an
       empty wrapper. */
    cssp = symbol_supplement_for_class(class_type);
    if (!cssp->any_nonstatic_data_members && cssp->destructor == NULL &&
        !class_type->variant.class_struct_union.any_virtual_functions &&
        !class_type->variant.class_struct_union.any_virtual_base_classes &&
        !has_more_than_one_direct_base_class(class_type)) {
      /* class_type is just a wrapper around a single direct base class.
         It introduces no new fields and has no implicit pointers (so its
         size will be identical to that of its direct base class), and it has
         no user-defined destructor.  Therefore calling the base class
         destructor will have the same effect as calling the derived class
         destructor, and so the diagnostic would be pointless. */
    } else {
      /* The derived class is not a mere wrapper. */
      for (; bcp != NULL; bcp = bcp->next) {
        if (bcp->direct) {            
          dtor_sym = symbol_supplement_for_class(bcp->type)->destructor;
          if (dtor_sym != NULL &&
              !dtor_sym->variant.routine.ptr->is_virtual) {
            /* The base class has a nonvirtual destructor, which is not
               recommended (see commentary in ARM 12.4). */
            pos_ty_remark(ec_base_class_with_nonvirtual_dtor,
                          &bcp->decl_position, bcp->type);
          }  /* if */
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* check_base_class_destructors */


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
        if (slep->symbol->kind == (a_symbol_kind)sk_projection &&
            !slep->symbol->variant.projection.is_using_decl) {
          /* This is another inherited conversion function.  If it converts
             to another type, it certainly should not mask the conversion
             function that we are now processing.  If it converts to the same
             type, we have a potential ambiguity: by also projecting the
             one we are now processing, we will detect such an ambiguity
             downstream. */
        } else if (slep->symbol->header == bcslep->symbol->header) {
          /* A conversion list entry from the current class already represents
             a conversion to the type specified by the conversion defined in
             the base class.  Ignore it. */
          break;
#if CHECKING
        } else if (is_template_list) {
          a_type_ptr  tp1, tp2;
          sym = fundamental_symbol_of(slep->symbol);
          check_assertion(sym->kind == (a_symbol_kind)sk_function_template);
          tp1 = sym->variant.template_info->variant.function.
                               routine->type->variant.routine.return_type;
          sym = fundamental_symbol_of(bcslep->symbol);
          check_assertion(sym->kind == (a_symbol_kind)sk_function_template);
          tp2 = sym->variant.template_info->variant.function.
                               routine->type->variant.routine.return_type;
          check_assertion_str2(!identical_types(tp1, tp2),
                               "check_base_class_conversion_list: types are",
                               "identical but symbol headers do not match");
#endif /* if CHECKING */
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
        a_type_ptr        fund_base_type = fund_sym->parent.class_type;
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
    if (udp->qualifier.class_type == sym->parent.class_type) {
      scp = source_corresp_for_il_entry(udp->entity.ptr,
                                        (an_il_entry_kind)udp->entity.kind);
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


static void create_member_using_declaration(
                                          a_symbol_ptr      sym,
                                          a_symbol_ptr      declared_sym,
                                          a_symbol_ptr      *other_sym,
                                          a_base_class_ptr  bcp,
                                          a_type_ptr        class_type,
                                          a_using_decl_ptr  *prev_udp,
                                          an_access_specifier  access)
/*
Check that a valid explicit projection (a class-scope using-declaration) can
be created for the symbol "sym", and if so create it.  "declared_sym" is
either equal to "sym", or it points to the overload set symbol of which "sym"
is an element.  "*other_sym" points to a declaration of the same name in the
scope of the derived class "class_type" (NULL if none exists).  "bcp" is the
base class from which the symbol is being projected.  "*prev_udp" is the
previous a_using_decl structure created for the using-declaration that is
currently being processed.  "access" is the access specifier applicable to
the new declaration.
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
             sym->variant.routine.ptr->compiler_generated) {
    /* Ignore compiler-generated member functions silently. */
  } else if (is_copy_assignment_operator_sym(sym)) {
    /* Using-declaration cannot apply to a copy-assignment operator,
       since they are not inheritable. */
    pos_sy_warning(ec_using_declaration_ignored, &decl_pos, sym);
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

    if (fund_sym == declared_sym ||
        fund_sym->parent.class_type == declared_sym->parent.class_type) {
      /* Common case: the fundamental symbol is the same as the declared
         symbol, or a member of the overload set it represents. */
      fund_base_class = bcp;
    } else {
      /* Special case:  Find the base class associated with the
         fundamental symbol. */
      fund_base_class = base_classes_of(class_type);
      for (;;) {
        if (fund_base_class->type == fund_sym->parent.class_type &&
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
      /* Just enter it, since no overloading is involved. */
      reenter_symbol(new_sym, depth_scope_stack,
                     /*suppress_error=*/TRUE);
      /* Save new_sym as *other_sym, in case is_overloaded is TRUE. */
      if (!new_sym->is_error) *other_sym = new_sym;
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
      if (rp ->special_kind == (a_special_function_kind)sfk_conversion) {
        /* Allocate the new conversion list entry and link it in the
           list for the current class. */
        add_to_conversion_list(new_sym, 
                               symbol_supplement_for_class(class_type));
      } else if (rp->special_kind ==
                               (a_special_function_kind)sfk_operator &&
                 rp->opname_kind == (an_opname_kind)onk_assign) {
        /* Record the assignment operator in the symbol. */
        record_assignment_operator_in_class_symbol(
                                 symbol_supplement_for_class(class_type),
                                 new_sym, *other_sym);
      }  /* if */
    }  /* if */
    /* Create a class member using decl entry to represent this
       declaration in the IL. */
    udp = make_using_decl(fund_sym, &decl_pos);
    /* Record the class that was actually specified in the qualified
       name in the source. */
    udp->qualifier.class_type = declared_sym->parent.class_type;
    udp->access = access;
    udp->is_class_member = TRUE;
    /* Update cross-reference and source-sequence info, if required. */
    record_using_decl(fund_sym, &decl_pos, udp, *prev_udp);
    *prev_udp = udp;
  }  /* if */
}  /* create_member_using_declaration */


static void member_using_declaration(a_type_ptr           class_type,
                                     an_access_specifier  access)
/*
Scan what is either a using-declaration (if tok_using is the current token)
or a deprecated access-adjustment declaration.  The semantics and
representation are identical.  class_type is the class in which the
declaration appears, and access is the current access (explicitly specified
or implicit) controlling the declaration.
*/
{
  a_symbol_ptr       sym, declared_sym;
  a_symbol_ptr       other_sym, fund_sym;
  a_base_class_ptr   bcp;
  a_boolean          err = FALSE;
  a_boolean          is_overloaded;
  a_symbol_locator   locator;
  a_using_decl_ptr   prev_udp = NULL;
  a_source_position  decl_pos, using_pos;

  db_enter(3, "member_using_declaration");
  add_stop_token(tok_semicolon);
  using_pos = pos_curr_token;
  if (curr_token == tok_using) {
    /* A using-declaration is outside the "Embedded C++" subset. */
    feature_is_not_part_of_embedded_cplusplus_subset(
                                        &pos_curr_token,
                                        ec_using_decl_in_embedded_cplusplus);
    /* This is a using declaration.  Bypass "using" and scan the
       identifier. */
    (void)get_token();
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
    a_type_ptr  tp;

    typename_specifier(&tp, /*within_using_decl=*/TRUE,
                       (a_decl_pos_block_ptr)NULL);
    if (is_error_type(tp)) {
      err = TRUE;
#if CHECKING
    } else {
      sym = locator_for_curr_id.specific_symbol;
      if (sym != NULL && sym->is_class_member &&
          is_or_contains_template_param(sym->parent.class_type)) {
        check_assertion(is_type_template_param_symbol(sym) ||
                        is_nonreal_instance_class_symbol(sym));
      }  /* if */
#endif /* CHECKING */
    }  /* if */
  } else {
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
      /* A using-declaration may not specify a constructor or destructor. */
      pos_diagnostic(microsoft_mode ? es_warning :
                     strict_ansi_mode ? es_error : es_discretionary_error,
                     ec_no_ctor_or_dtor_using_declaration, &decl_pos);
      err = TRUE;
    } else if (is_copy_assignment_operator_sym(declared_sym)) {
      /* Using-declaration cannot apply to a copy-assignment operator,
         since they are not inheritable. */
      pos_sy_warning(ec_using_declaration_ignored, &decl_pos, declared_sym);
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
    } else if (locator_for_curr_id.parent.class_type->kind ==
                                            (a_type_kind)tk_template_param) {
      /* Suppress the base class check and create a dummy base class. */
      bcp = alloc_base_class();
      bcp->type = declared_sym->parent.class_type;
      bcp->derived_class = class_type;
    } else {
      for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
        if (bcp->type == locator_for_curr_id.parent.class_type) {
          /* The qualifier is a base class of the current class. */
          break;
        }  /* if */
      }  /* for */
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
      } else if (!(bcp->direct || any_cfront_mode())) {
        /* Base class members designated in a using-declaration must be
           visible in the scope of at least one direct base class. */
        a_base_class_ptr  direct_bcp = base_classes_of(class_type);
        for (; direct_bcp != NULL; direct_bcp = direct_bcp->next) {
          if (direct_bcp->direct) {
            a_symbol_ptr  visible_sym;
            clear_locator(&locator, &decl_pos);
            locator.symbol_header = locator_for_curr_id.symbol_header;
            visible_sym = class_qualified_id_lookup(&locator, direct_bcp->type,
                                                    IDL_NO_OPTIONS);
            if (visible_sym == fund_sym) { break; }
          }  /* if */
        }  /* for */
        if (direct_bcp == NULL) {
          error(ec_member_using_must_be_visible_in_direct_base);
          err = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
    if (!err) {
      /* Look up the name in the scope of the current class. */
      clear_locator(&locator, &decl_pos);
      locator.symbol_header = locator_for_curr_id.symbol_header;
      (void)curr_scope_id_lookup(&locator, IDL_PROJ_SYMBOL_ALLOWED);
      if (locator.specific_symbol != NULL &&
          (locator.specific_symbol->kind != (a_symbol_kind)sk_type ||
           !locator.specific_symbol->variant.type.is_injected_class_name)) {
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
          fund_sym = fundamental_symbol_of(locator.specific_symbol);
          if (is_function_or_template_symbol(fund_sym)) {
            /* Okay. */
          } else if (is_nontype_template_param_symbol(fund_sym)) {
            /* Might be a function symbol, so it's okay. */
          } else {
            err = TRUE;
          }  /* if */
        }  /* if */
        if (err) {
          /* Name has already been declared. */
          pos_st_error(ec_id_already_declared, &decl_pos,
                       locator_for_curr_id.symbol_header->identifier);
          err = TRUE;
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
      if (tag_sym != NULL && !is_class_template_symbol(tag_sym)) {
        create_member_using_declaration(tag_sym, tag_sym,
                                        &overload_sym, bcp, class_type,
                                        &prev_udp, access);
      }  /* if */
    }  /* if */
    for (;;) {
      create_member_using_declaration(sym, declared_sym, &other_sym,
                                      bcp, class_type, &prev_udp, access);
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
}  /* member_using_declaration */


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
  a_type_ptr                     tp;

  db_enter(3, "find_corresp_prototype_tag_sym");
  if (is_nonreal_instance_class_symbol(curr_sym)) {
    /* Return NULL. */
  } else if (curr_sym->is_class_member) {
    /* curr_sym represents a nested class.  Get the corresponding prototype
       tag symbol of its parent class; then find the corresponding nested
       class within it.  The prototype tag symbol of the parent class is
       stored in the latter's class symbol supplement. */
    sym = symbol_supplement_for_class(curr_sym->parent.class_type)->
                                                       corresp_prototype_sym;
    if (sym != NULL) {
      /* sym is the corresponding prototype tag symbol of the parent class.
         It represents a prototype instantiation of a class template or a
         class nested within a prototype instantiation. One of its own nested
         classes will be the nested class that corresponds to curr_sym: find
         a symbol for that nested class. */
      tp = sym->variant.class_struct_union.type;
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
        if (is_incomplete_type(sym->variant.class_struct_union.type)) {
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
    parent_tssp = symbol_supplement_for_class(tag_sym->parent.class_type)->
                                                               template_info;
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


static a_boolean scan_access_specification(an_access_specifier  *access)
/*
Check for an access specifier in the source.  If one is found, return TRUE
and update *access accordingly.
*/
{
  a_boolean  found = FALSE;

  /* The check is implemented as a loop because successive access
     specifications are permitted. */
  while (curr_token == tok_public || curr_token == tok_private ||
         curr_token == tok_protected) {
    found = TRUE;
    if (curr_token == tok_public) {
      *access = (an_access_specifier)as_public;
    } else if (curr_token == tok_protected) {
      *access = (an_access_specifier)as_protected;
    } else {
      *access = (an_access_specifier)as_private;
    }  /* if */
    scope_stack[decl_scope_level].current_access = *access;
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


static void check_missing_declarator_in_member_declaration(
                                           a_type_ptr              class_type,
                                           a_type_ptr              member_type,
                                           a_member_decl_info_ptr  decl_info)
/*
This routine is called while a member declaration is being scanned when a
semicolon is encountered immediately after the declaration-specifiers.  In
other words, there is no declarator in the member declaration.  Issue an error
if appropriate.  class_type is the class whose definition is being scanned.
member_type is the type returned from decl_specifiers. *decl_info contains
other information about the declaration as it has been scanned thus far;
moreover, several fields of *decl_info may be updated by this routine.
*/
{
  a_source_position  *err_pos = &decl_info->decl_start_pos;
  a_decl_flag_set    dso_flags = decl_info->dso_flags;
  a_storage_class    storage_class = decl_info->storage_class;

  /* Check first whether this is an anonymous union declaration. */
  if (storage_class == (a_storage_class)sc_unspecified &&
      !is_incomplete_type(member_type) &&
      is_anonymous_union_decl(member_type, decl_info)) {
    /* A C++ anonymous union -- "union { int i, j; };" */
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
      a_symbol_ptr  sym;

      sym = (a_symbol_ptr)(member_type)->source_corresp.assoc_info;
      if (sym != NULL && has_name(member_type)) {
        record_symbol_declaration(SRK_DECLARATION, sym, err_pos,
                                  (a_source_sequence_entry_ptr)NULL);
      }  /* if */
      if (!has_name(member_type)) {
        /* Only the types of anonymous unions whose type itself (as opposed
           to the associated member object) is anonymous are marked as being
           nonstandard anonymous union types. */
        member_type
           ->variant.class_struct_union.is_nonstd_anonymous_union_type = TRUE;
      }  /* if */
    }  /* if */
#endif /* ALLOW_NONSTANDARD_ANONYMOUS_UNIONS */
    decl_info->is_anonymous_union = TRUE;
    /* Set the IL referenced flag for the anonymous union type. */
#if 0
    /* It would probably be better to set it when an anonymous union member
       is actually referenced. */
#endif /* if 0 */
    member_type->source_corresp.referenced = TRUE;
  } else if (!C_mode()) {
    /* C++ mode. */
    if (dso_flags & DSO_MUTABLE) {
      /* "mutable" is only allowed on nonstatic data member decls. */
      pos_error(ec_mutable_not_allowed, err_pos);
    }  /* if */
    if (dso_flags & DSO_FRIEND) {
      if ((dso_flags & DSO_ELABORATED_TYPE_SPECIFIER) &&
          !is_enum_type(member_type) &&
          depth_template_declaration_scope == NO_SCOPE_DEPTH) {
        /* This is a friend class declaration, of the form:
                   friend class A;
           which is the only form the ARM (see 11.4) allows. */
        if (dso_flags & DSO_TYPENAME) {
          /* "friend typename ..." is not allowed. */
          pos_error(ec_no_typename_in_friend_class_decl, err_pos);
        } else {
          decl_friend_class(class_type, member_type);
        }  /* if */
      } else if (!is_error_type(member_type)) {
        /* Invalid friend declaration. */
        pos_error(ec_bad_friend_decl, err_pos);
      }  /* if */
    } else if (decl_info->is_member_template) {
      if (decl_info->member_sym != NULL) {
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
         "struct { int i; };" or "enum {};". */
      /* Does the Working Paper rule out such useless constructs?  The first
         sentence of Chapter 7 says, "A declaration introduces one or more
         names into a program", and if DSO_DECLARES_SOMETHING is not set no
         name was introduced.  On the other hand, 9.2 para 6 allows the
         omission of declarators with enum and class specifiers.  However,
         we take this to include only enum and class specifiers that at
         least declare *something*. */
      pos_diagnostic(strict_ansi_mode ? strict_ansi_error_severity :
                                        es_warning,
                     ec_useless_decl, err_pos);
      if (is_qualified_type(member_type)) {
        pos_diagnostic(strict_ansi_mode ? strict_ansi_error_severity :
                                          es_warning,
                       ec_useless_type_qualifiers, err_pos);
      }  /* if */
    } else {
      /* A case like "int;" is explicitly disallowed by language in ARM 9.2. */
      pos_error(ec_useless_decl, err_pos);
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
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if ((dso_flags & (DSO_DECLARES_SOMETHING | DSO_DEFINES_SOMETHING)) ||
      (decl_info->is_anonymous_union &&
       member_type->kind != (a_type_kind)tk_typeref)) {
    /* This is a free-standing declaration of a class, struct, union, or
       enum. */
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
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
}  /* check_missing_declarator_in_member_declaration */


static void check_completed_member_type(a_type_ptr              *type,
                                        a_symbol_locator        *locator,
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
  if (decl_info->storage_class != (a_storage_class)sc_typedef) {
    if (any_cfront_mode() &&
        check_member_function_typedef(*type, &locator->source_position)) {
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
      *type = error_type();
    }  /* if */
  }  /* if */
  if (decl_info->dso_flags & DSO_DEFINES_SOMETHING) {
    /* A class or enum definition was scanned as part of this declaration.
       However, it is explicitly prohibited to define a type in a function
       return type.  This is taken to apply to pointer-to-function type
       declarations as well to the function declarations. */
    if (decl_info->return_type_def_err) {
      /* Error has already been issued. */
    } else {
      a_type_ptr  tp = *type;
      for (;;) {
        if (is_function_type(tp)) {
          /* Function type in which the return type involves a
             definition. */
          pos_error(ec_type_def_not_allowed_in_func_type_decl,
                    &decl_info->decl_start_pos);
          decl_info->return_type_def_err = TRUE;
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
  if (class_state->is_nonreal_instantiation && is_function_type(*type)) {
    /* The class is a non-real template instantiation.  Go through the
       parameters for this function type, and if any of the associated types
       involves a template parameter, mark the param type entry; this is
       useful for function arg matching. */
    set_type_involves_deduced_template_param(*type);
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
  an_error_code  error_code = ec_no_error;

  if (decl_info->invalid_virtual_specifier) {
    /* An error has already been issued on a previous declarator. */
  } else if (decl_info->dso_flags & DSO_FRIEND) {
    /* A friend function may not be declared virtual. */
    error_code = ec_bad_friend_decl;
  } else if (decl_info->is_constructor || is_union_type(class_type)) {
    /* Constructors may not be virtual functions (WP 12.1 [class.ctor]) and
       unions may not have them (WP 9.5 [class.union]). */
    error_code = ec_virtual_not_allowed;
  } else if (decl_info->storage_class == (a_storage_class)sc_static ||
             (locator->is_operator_name &&
              (is_new_operator(locator->variant.opname) ||
               is_delete_operator(locator->variant.opname)))) {
    /* Only nonstatic member functions may be specified as virtual.  This
       applies to operators new and delete since they are always static. */
    error_code = ec_virtual_static_not_allowed;
  }  /* if */
  if (error_code != ec_no_error) {
    pos_error(error_code, decl_info->is_first_in_declarator_list ?
                            &decl_info->decl_start_pos :
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

  if (tag_sym->kind == (a_symbol_kind)sk_union_tag) {
    /* Note that we do not do this check for unions.  This is partly because
       a union may have a mixture of const and non-const declarations, and
       it's not clear that the const members really need to be initialized. */
  } else {
    any_diagnostics_issued = FALSE;
    cssp = tag_sym->variant.class_struct_union.extra_info;
    /* If a diagnostic is required, each of the uninitialized const or ref
       members will be listed, so loop through the symbols looking for
       candidates. */
    for (sym = cssp->symbols; sym != NULL; sym = sym->next_in_scope) {
      if (sym->kind == (a_symbol_kind)sk_field) {
        a_type_ptr     tp = sym->variant.field.ptr->type;
        an_error_code  error_code;

        if (is_reference_type(tp)) {
          /* Member of reference type must be explicitly initialized. */
          error_code = ec_reference_member;
        } else if (is_const_qualified_type(tp)) {
          /* Usually, a member of const type must be explicitly
             initialized. */
          if (type_has_user_declared_default_constructor(tp)) {
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


#if !GENERATE_SOURCE_SEQUENCE_LISTS
/*ARGSUSED*/ /* instance and template_decl is not used unless source
                sequence lists are generated. */
#endif /* !GENERATE_SOURCE_SEQUENCE_LISTS */
static a_symbol_ptr class_member_declaration(
                      a_type_ptr               class_type,
                      a_class_def_state_ptr    class_state,
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
tracking general information about the class.  *skip_semicolon_check is
returned TRUE if the caller should suppress the check for a semicolon
following the member declaration.  templ_param_list is non-NULL for
function template declarations.  decl_pos_block_ptr is non-NULL when then
extra source position information collected during this declaration needs
to be returned to the caller.  If prototype instantiations are recorded in
the IL, the template header is passed via template_decl.  
*/
{
  a_source_position    decl_start_pos;
  a_decl_flag_set      dsi_flags;
  a_decl_flag_set      dso_flags;
  a_type_qualifier_set qualifiers;
  a_type_ptr           member_type;
  a_boolean            no_decl_specifiers;
  a_boolean            friend_specified;
  a_boolean            type_explicitly_specified, inline_specified;
  a_boolean            mutable_specified;
  a_symbol_ptr         rout_sym;
  a_member_decl_info   decl_info;
  a_boolean            is_member_template_rescan;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_boolean            any_decl_other_than_nonstatic_data_member = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

  db_enter(3, "class_member_declaration");
  *skip_semicolon_check = FALSE;
  decl_start_pos = pos_curr_token;
  initialize_member_decl_info(&decl_info, &decl_start_pos);
  is_member_template_rescan = (scope_stack[depth_scope_stack].kind ==
                                 (a_scope_kind)sck_template_instantiation);
  /* Set the flags to control the calls to decl_specifiers. */
  dsi_flags = DSI_TYPE_SPECIFIER_ALLOWED |
              DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER;
  if (C_dialect == C_dialect_cplusplus) {
    dsi_flags |= (DSI_STORAGE_CLASS_SPECIFIER_ALLOWED |
                  DSI_IS_MEMBER_DECLARATION | DSI_INLINE_ALLOWED |
                  DSI_EMPTY_DECL_SPECIFIERS_ALLOWED |
                  DSI_VACUOUS_TAG_DECL_ALLOWED);
    if (is_member_template) {
      dsi_flags |= DSI_IS_TEMPLATE_DECLARATION;
      decl_info.is_member_template = TRUE;
    }  /* if */
  }  /* if */
  /* First scan the declaration specifiers.  In C++ the specifiers may be
     omitted, e.g., for a function member with implicit type. */
  add_stop_token(tok_colon);
  (void)decl_specifiers(dsi_flags, &dso_flags, &decl_info.storage_class,
                        &member_type, &qualifiers, &decl_info.decl_modifiers,
                        &decl_info.decl_pos_block);
  decl_info.dso_flags = dso_flags;
  if (C_dialect == C_dialect_cplusplus &&
      (dso_flags & DSO_DEFINES_SOMETHING) && !is_error_type(member_type)) {
    /* Should be a class, struct, union, or enum definition. */
    a_type_ptr    tp = skip_typerefs(member_type);
    a_symbol_ptr  sym = (a_symbol_ptr)(tp->source_corresp.assoc_info);

    if (is_member_template) {
      if (curr_token == tok_semicolon &&
          decl_info.storage_class != (a_storage_class)sc_typedef) {
        /* Issue an error later, based on the symbol. */
        decl_info.member_sym = sym;
      }  /* if */
#if CHECKING
    } else if (!sym->is_error) {
      /* A nested class, struct, union, or enum definition.  Be sure the
         parent class was marked correctly. */
      check_assertion_str2(sym->is_class_member &&
                           sym->parent.class_type == class_type,
                           "class_member_declaration:",
                           "bad parent type on nested type");
#endif /* CHECKING */
    }  /* if */
  } /* if */
  no_decl_specifiers = (dso_flags & DSO_NO_DECL_SPECIFIERS) != 0;
  type_explicitly_specified =
                       (dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) != 0;
  friend_specified = dso_flags & DSO_FRIEND;
  if (friend_specified) class_state->any_friend_decls = TRUE;
  inline_specified = (dso_flags & DSO_INLINE) != 0;
  decl_info.is_constructor = (dso_flags & DSO_CONSTRUCTOR) != 0;
  decl_info.is_destructor = (dso_flags & DSO_DESTRUCTOR) != 0;
  mutable_specified = (dso_flags & DSO_MUTABLE) != 0;
  remove_stop_token(tok_colon);
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
  if ((dso_flags & DSO_EXPLICIT) && !(dso_flags & DSO_CONSTRUCTOR) &&
      !(microsoft_mode && curr_token == tok_semicolon)) {
    /* The keyword "explicit" is allowed only on a constructor declaration,
       and in Microsoft mode on free standing class/enum declarations. */
    pos_error(ec_explicit_not_allowed, &decl_start_pos);
  }  /* if */
  if (curr_token == tok_semicolon) {
    /* There's no declarator following the declaration specifier.  This may
       be okay, but sometimes a diagnostic should be issued. */
    check_missing_declarator_in_member_declaration(class_type, member_type,
                                                   &decl_info);
    if (decl_info.is_anonymous_union) {
      /* decl_nonstatic_data_member needs to be called. */
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
#if MICROSOFT_EXTENSIONS_ALLOWED
  any_decl_other_than_nonstatic_data_member = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* A declarator list should be present.  Scan it. */
  do {
    a_symbol_locator                  locator;
    a_type_ptr                        local_type;
    a_func_info_block                 func_info;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    a_boolean                         preserve_param_id_list = FALSE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    a_template_symbol_supplement_ptr  tssp;
    a_source_position                 declarator_start_pos;
#if MICROSOFT_EXTENSIONS_ALLOWED
    a_boolean                         is_nonstatic_data_member = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

    declarator_start_pos = pos_curr_token;
    add_stop_token(tok_comma);
    add_stop_token(tok_colon);
    add_stop_token(tok_try);
    clear_func_info(&func_info);
    /* Clear certain decl_info fields each time through the loop. */
    decl_info.do_flags = DO_NO_OUTPUT_FLAGS;
    decl_info.is_unnamed_field = FALSE;
    decl_info.declarator_ssep = NULL;
    decl_info.member_sym = NULL;
    if (!decl_info.is_first_in_declarator_list &&
        (dso_flags & (DSO_CONSTRUCTOR | DSO_DESTRUCTOR))) {
      /* This section of code is entered when there is a comma-list of
         constructors and/or destructors. */
      decl_info.is_destructor = decl_info.is_constructor = FALSE;
      if (curr_token == tok_compl ||
          (is_generalized_identifier_start(GID_NO_OPTIONS) &&
           locator_for_curr_id.is_destructor_name)) {
        decl_info.is_destructor = TRUE;
        member_type = unknown_type();
      } else if (curr_token == tok_identifier &&
                 is_constructor_decl(class_type)) {
        decl_info.is_constructor = TRUE;
        member_type = unknown_type();
      } else {
        decl_start_pos = pos_curr_token;
        member_type = integer_type((an_integer_kind)ik_int);
      }  /* if */
    }  /* if */
    /* The declarator can be omitted for an unnamed bit-field. */
    set_err_pos_to_curr_token();
    if (curr_token == tok_colon && !no_decl_specifiers) {
      /* Unnamed bit-field. */
      decl_info.is_unnamed_field = TRUE;
      local_type = member_type;
      set_to_error_locator(locator);
    } else if (decl_info.is_anonymous_union) {
      /* There is no declarator. */
      local_type = member_type;
      set_to_error_locator(locator);
    } else if (no_decl_specifiers && !decl_info.is_constructor &&
               !decl_info.is_destructor && !is_declarator_start()) {
      remove_stop_token(tok_comma);
      remove_stop_token(tok_colon);
      remove_stop_token(tok_try);
      syntax_error(ec_exp_declaration);
      if (curr_token == tok_semicolon) {
        /* Advance past the semicolon. */
        (void)get_token();
      }  /* if */
      discard_curr_construct_pragmas();
      *skip_semicolon_check = TRUE;
      goto next_declaration;
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
            check_assertion(curr_routine_fixup->class_type == class_type);
          }  /* if */
        } else if (!is_member_template_rescan) {
          /* Normal case.  Allocate a new routine fixup entry. */
          curr_routine_fixup = alloc_routine_fixup(class_type);
        }  /* if */
        add_stop_token(tok_lbrace);
        /* Set the various flags for declarator processing (C++ only). */
        di_flags |= DI_OPERATOR_NAME_ALLOWED;
        if (!type_explicitly_specified && qualifiers == TQ_NONE) {
          di_flags |= DI_NO_TYPE_SPECIFIERS;
        }  /* if */
        if (decl_info.is_constructor) di_flags |= DI_IS_CONSTRUCTOR;
        if (decl_info.storage_class == (a_storage_class)sc_typedef) {
          di_flags |= DI_IS_TYPEDEF_DECLARATION;
        } else if (decl_info.storage_class != (a_storage_class)sc_static) {
          /* The storage class "static" was not specified and it is not a
             typedef declaration.   Therefore, if this turns out to be a member
             function declaration, it will be a nonstatic member function.
             This is important because when the routine type is created,
             function_declarator needs to know whether to add an implicit
             this-param pointer to the type. */
          di_flags |= DI_NONSTATIC_MEMBER;
        }  /* if */
        if (friend_specified) {
          di_flags |= (DI_IS_FRIEND_DECL | DI_QUALIFIED_NAME_ALLOWED);
        }  /* if */
        if (is_member_template) {
          di_flags |= DI_IS_TEMPLATE_DECLARATION;
        }  /* if */
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* While scanning the declarator it will be useful to know whether we're
         dealing with the declaration of a Microsoft property field. */
      if (decl_info.decl_modifiers.get_property_name != NULL ||
          decl_info.decl_modifiers.put_property_name != NULL) {
        di_flags |= DI_IS_MICROSOFT_PROPERTY;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Pass the class's type pointer to declarator if this might be a
         nonstatic member function, in which case its presence will cause an
         implicit "this" parameter type to be created. (Static member
         functions do not have an implicit "this" pointer. The class pointer
         will be ignored for data members.) */
      declarator(di_flags, &decl_info.do_flags, (a_type_qualifier_set *)NULL,
                 member_type,
                 friend_specified ? (a_type_ptr)NULL : class_type,
                 &locator, &local_type, &decl_info.declarator_ssep,
                 &func_info, &decl_info.decl_pos_block);
      if (!C_mode()) {
        remove_stop_token(tok_lbrace);
        check_completed_member_type(&local_type, &locator, class_state,
                                    &decl_info);
        if (is_member_template_rescan) {
          if (decl_info.storage_class != (a_storage_class)sc_unspecified &&
              decl_info.storage_class != (a_storage_class)sc_static) {
            /* An error will already have been issued on, e.g.,
                 struct A { template <class T> typedef A (T) { } };
            */
            decl_info.storage_class = (a_storage_class)sc_unspecified;
          }  /* if */
        }  /* if */
        decl_info.is_constructor = 
                                 (decl_info.do_flags & DO_IS_CONSTRUCTOR) != 0;
        decl_info.is_destructor = locator.is_destructor_name ||
                                  (decl_info.do_flags & DO_IS_DESTRUCTOR) != 0;
      }  /* if */
    }  /* if */
    remove_stop_token(tok_colon);
    remove_stop_token(tok_try);
    if (!C_mode() && is_function_type(local_type) &&
        decl_info.storage_class != (a_storage_class)sc_typedef) {
      /* Member or friend function. */
      a_boolean  function_def_present = FALSE;

      if (mutable_specified) {
        /* "mutable" is only allowed on nonstatic data member decls. */
        pos_error(ec_mutable_not_allowed, &decl_start_pos);
      }  /* if */
      if (curr_token == tok_lbrace || curr_token == tok_try ||
          (decl_info.is_constructor && (curr_token == tok_colon))) {
        function_def_present = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (microsoft_mode && curr_token == tok_assign) {
        /* In Microsoft compatibility mode the pure specifier is permitted
           on a definition; it's usually a syntax error. */
        a_token_cache  cache;

        clear_token_cache(&cache, /*reusable=*/FALSE);
        /* Put the current token in the cache. */
        cache_curr_token(&cache);
        /* Advance to what may be "0". */
        if (get_token() == tok_int_constant) {
          cache_curr_token(&cache);
          /* Advance past it and see if the next token is a left brace. */
          if (get_token() == tok_lbrace) function_def_present = TRUE;
        }  /* if */
        /* Restore the lexical state. */
        rescan_cached_tokens(&cache);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
      if (function_def_present && !decl_info.is_first_in_declarator_list) {
        pos_error(ec_exp_semicolon, &pos_curr_token);
      }  /* if */
      func_info.is_definition = function_def_present;
      func_info.is_inline = inline_specified || function_def_present;
#if USER_CONTROL_OF_STRUCT_PACKING
      if (function_def_present) {
        /* Record the current setting of the maximum alignment for local
           class members (an adjustment may be required for packing). */
        func_info.max_member_alignment =
                             current_max_alignment_for_class_members();
      }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
      if (!type_explicitly_specified) {
        /* No type specifier. */
        if (decl_info.is_constructor || decl_info.is_destructor ||
            locator.is_conversion_name) {
          /* Type specifier is not expected (nor permitted) on constructors,
             destructors, and conversion functions. */
        } else {
          /* Type specifier is missing.  The type defaults to int, but issue
             a diagnostic. */
          report_missing_type_specifier(&declarator_start_pos,
                                        /*is_function=*/TRUE,
                                        function_def_present,
                                        /*is_main_function=*/FALSE,
                                        !no_decl_specifiers);
          /* Under most circumstances the implicit-int substitution will be
             done in decl_specifiers.  An exception is a comma list that
             includes a conversion operator declaration followed by another
             declaration -- e.g.,
               struct S { operator X(), i; };
             for which the type returned by decl_specifiers an
             unknown_type(). */
          if (!decl_info.is_first_in_declarator_list) {
            a_type_ptr  tp = integer_type((an_integer_kind)ik_int);

            local_type->variant.routine.return_type = tp;
#if GENERATE_SOURCE_SEQUENCE_LISTS
            skip_typerefs(func_info.declared_type)->
                                     variant.routine.return_type = tp;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
          }  /* if */
        }  /* if */
      }  /* if */
      /* Issue diagnostic on an incomplete-type in an exception
         specification.  (It wasn't done when the exception specification
         was scanned because definitions and declarations are treated
         differently.) */
      report_exception_spec_errors(&func_info);
      if (local_type == member_type) {
        /* When scanning the declarator does not change the type, we know
           this member is a function based on the specifier type alone.
           This is only possible with a typedef name that represents a
           function type. */
        func_info.function_type_from_typedef = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        func_info.declarator_ssep = decl_info.declarator_ssep;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        /* Such typedef function types are shared and so are unsuited to be
           the type of a defined function. */
        check_typedef_function_type(&local_type, &locator.source_position,
                                    function_def_present, class_type,
                                    (!friend_specified &&
                                     decl_info.storage_class !=
                                          (a_storage_class)sc_static));
      }  /* if */
      if (decl_info.dso_flags & DSO_VIRTUAL) {
        check_for_invalid_use_of_virtual(&locator, class_type, &decl_info);
      }  /* if */
      if (!friend_specified) {
        if ((decl_info.is_constructor || decl_info.is_destructor) &&
            decl_info.storage_class == (a_storage_class)sc_static) {
          /* Constructors and destructors may not be declared "static"
             (ARM 12.1, 12.4). */
          pos_error(ec_static_not_allowed, &decl_start_pos);
          decl_info.storage_class = (a_storage_class)sc_unspecified;
        }  /* if */
        if (decl_info.is_constructor || decl_info.dso_flags & DSO_VIRTUAL) {
          /* A class with a user-defined constructor or a virtual function
             cannot be an "aggregate" (8.5.1). */
          class_state->class_aggregate_ruled_out = TRUE;
          class_state->POD_ruled_out = TRUE;
        } else if (decl_info.is_destructor) {
        /* A POD may not have a user-defined destructor, either. */
          class_state->POD_ruled_out = TRUE;
        }  /* if */
      }  /* if */
      if (friend_specified) {
        /* Process a friend function declaration. */
        if (function_def_present && microsoft_mode &&
            class_state->is_template_instantiation) {
          func_info.is_definition = FALSE;
        }  /* if */
        rout_sym = decl_friend_function(&locator, class_type, local_type,
                                        &func_info, &decl_info);
      } else if (is_member_template_rescan) {
        *member_template_instance_type = local_type;
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
        /* Process the member function template. */
        decl_member_function_template(&locator, class_type, local_type,
                                      templ_param_list, &func_info,
                                      class_state, &decl_info);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (decl_info.declarator_ssep != NULL) {
          remove_from_src_seq_list(decl_info.declarator_ssep);
          decl_info.declarator_ssep = NULL;
        }  /* if */
        if (!func_info.is_definition && !source_sequence_entries_disallowed) {
          /* Turn the source sequence entry for the a_template entry into a
             secondary source sequence entry. */
          a_src_seq_secondary_decl_ptr sssdp =
                            secondary_src_seq_for_template(il_template_entry);
          sssdp->declared_type = func_info.declared_type;
        } else if (func_info.is_definition) {
          template_supplement_for_symbol(decl_info.member_sym)->
                              variant.function.routine->declared_type =
                                                      func_info.declared_type;
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        rout_sym = decl_info.member_sym;
        if (decl_info.is_constructor && (dso_flags & DSO_EXPLICIT)) {
          tssp = rout_sym->variant.template_info;
          tssp->variant.function.routine->is_explicit_constructor = TRUE;
        }  /* if */
        remove_stop_token(tok_comma);
        goto next_declaration;
      } else {
        /* Must be a member function declaration. */
        /* Create a symbol for the member function. */
        decl_member_function(&locator, class_type, local_type, &func_info,
                             class_state, &decl_info,
                             /*compiler_generated=*/FALSE);
        rout_sym = decl_info.member_sym;
        if (class_state->is_nonreal_instantiation) {
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
                declared_type = form_declared_type(local_type, &func_info);
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
        if (decl_info.is_constructor && (dso_flags & DSO_EXPLICIT)) {
          rout_sym->variant.routine.ptr->is_explicit_constructor = TRUE;
        }  /* if */
      }  /* if */
      if (!function_def_present) {
        /* Update xref info on param ids. */
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
           functions. */
        scan_pure_specifier(rout_sym, class_type, &decl_info);
      }  /* if */
      if (function_def_present) {
        a_token_sequence_number  first_token_number;
        a_token_sequence_number  last_token_number;
        a_token_cache		 body_cache;

        /* The inline flag is set for friend functions in
           decl_friend_function, which also handles cases in which it should
           be left unset despite the presence of a function body. */
        check_assertion(friend_specified ||
                        rout_sym->variant.routine.ptr->is_inline);
        remove_stop_token(tok_comma);
        /* Cache the tokens comprising the function definition so that they
           can be rescanned once the entire class definition has been
           processed. */
        if (prescan_function_definition(&first_token_number,
                                        &last_token_number,
                                        &body_cache,
                                        (a_boolean)decl_info.is_constructor)) {
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
          if (friend_specified) {
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
          } else {
            /* A member function of a nonreal class serves as a template, and
               since this is the definition the template_info associated with
               this member function must be updated, based on the template_info
               of the prototype instantiation.  Note that the current class may
               be nested within the prototype instantiation. */
            a_template_symbol_supplement_ptr  class_tssp;
            /* A member function of a template class whose body is supplied in
               the class shares the template declaration information with the
               enclosing class. */
            tssp = rout_sym->variant.routine.instance_ptr->template_info;
            class_tssp = symbol_supplement_for_class(class_type)->
                                                                 template_info;
            /* The body cache is saved here, but will be updated later during
               routine fixup.  This is needed for the generation of template
	       strings to be done properly. */
            set_template_cache_info(&tssp->cache, &body_cache,
                                    class_tssp->cache.decl_info);
            tssp->cache_segment = alloc_template_cache_segment(rout_sym, tssp);
            tssp->cache_segment->first_token_number = first_token_number;
            tssp->cache_segment->last_token_number = last_token_number;
          }  /* if */
        }  /* if */
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
            } else {
              /* An undefined virtual member function in an unnamed class (or
                 in a named class that is nested in an unnamed class) cannot
                 be defined later (there's no way to name it), so issue an
                 error. */
              a_type_ptr  tp = class_type;
              for (;;) {
                if (tp->variant.class_struct_union.originally_unnamed) {
                  sym_error(ec_unnamed_class_virtual_function_def_missing,
                            rout_sym);
                  break;
                }  /* if */
                if (!tp->source_corresp.is_class_member) break;
                tp = tp->source_corresp.parent.class_type;
              }  /* for */
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (is_member_template) {
      /* Invalid declaration of a member template. */
      pos_error(ec_bad_member_template_decl, &decl_start_pos);
      remove_stop_token(tok_comma);
      discard_curr_construct_pragmas();
      break;
    } else if (decl_info.dso_flags & (DSO_FRIEND | DSO_VIRTUAL | DSO_INLINE)) {
      if (decl_info.dso_flags & DSO_FRIEND) {
        pos_error(ec_bad_friend_decl, &decl_start_pos);
      }  /* if */            
      if (decl_info.dso_flags & DSO_VIRTUAL) {
        pos_error(ec_virtual_not_allowed, &decl_start_pos);
      }  /* if */            
      if (decl_info.dso_flags & DSO_INLINE) {
        pos_error(ec_inline_and_nonfunction, &decl_start_pos);
      }  /* if */            
      remove_stop_token(tok_comma);
      discard_curr_construct_pragmas();
      break;
    } else if (decl_info.is_destructor) {
      /* Error has already been issued if it wasn't processed as a
         function. */
      discard_curr_construct_pragmas();
    } else if (decl_info.storage_class == (a_storage_class)sc_typedef) {
      check_assertion(C_dialect == C_dialect_cplusplus);
      if (decl_info.do_flags & DO_CFRONT_MEMBER_FUNCTION_TYPEDEF) {
        /* This looked like a cfront-style member function typedef.  Be sure
           the type was a function type. */
        if (is_function_type(local_type)) {
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
                                      /*is_function=*/FALSE,
                                      /*is_function_def=*/FALSE,
                                      /*is_main_function=*/FALSE,
                                      !no_decl_specifiers);
      }  /* if */
      /* Typedef declaration. */
      decl_typedef(&locator, local_type, class_type, &decl_info.member_sym,
                   decl_info.declarator_ssep, &decl_info.decl_pos_block);
      /* Note: access will have been set in decl_typedef. */
      if (curr_routine_fixup != NULL &&
          curr_routine_fixup->def_arg_expr_fixup_list != NULL) {
        /* Update the symbol pointer in the fixup entry -- it's needed when
           the default args are scanned (once the entire class has been
           scanned). */
        curr_routine_fixup->symbol = decl_info.member_sym;
      }  /* if */
      if (microsoft_bugs) {
        /* In Microsoft bugs mode, the typedef is processed before member
           function bodies etc. are rescanned.  This makes e.g. the following
           legal:
              typedef struct {
                enum { e };
                void f() { S::e; }
              } S;
        */
        process_deferred_class_fixups_and_instantiations();
      }  /* if */
    } else if (curr_token == tok_assign && !C_mode() &&
               ((is_scalar_type(local_type) && !mutable_specified &&
                 (get_type_qualifiers(local_type) == TQ_CONST)) ||
                is_or_contains_template_param(local_type)) &&
               decl_info.storage_class == (a_storage_class)sc_unspecified) {
      /* Provide support for the nonstandard declaration of a member constant
         of scalar type -- e.g., "const int I = 2;". */
      if (in_expression_context()) {
        syntax_error(ec_nonstd_const_member_decl_not_allowed);
      } else {
        decl_nonstd_member_constant(&locator, class_type, local_type,
                                    class_state, &decl_info);
      }  /* if */
    } else {
      if (mutable_specified &&
          is_const_qualified_type(local_type)) {
        /* "mutable" and top-level "const" are not allowed together. */
        pos_error(ec_mutable_not_allowed, &decl_start_pos);
      }  /* if */
      if (!type_explicitly_specified) {
        report_missing_type_specifier(&declarator_start_pos,
                                      /*is_function=*/FALSE,
                                      /*is_function_def=*/FALSE,
                                      /*is_main_function=*/FALSE,
                                      !no_decl_specifiers);
        /* Under most circumstances the implicit-int substitution will be
           done in decl_specifiers.  An exception is a comma list that
           includes a conversion operator declaration followed by another
           declaration -- e.g.,
             struct S { operator X(), i; };
           for which the type returned by decl_specifiers an unknown_type(). */
        if (!decl_info.is_first_in_declarator_list) {
          local_type = integer_type((an_integer_kind)ik_int);
        }  /* if */
      }  /* if */
      if (decl_info.storage_class == (a_storage_class)sc_static) {
        /* Static data member. */
        decl_static_data_member(&locator, class_type, local_type,
                                class_state, &decl_info);
      } else {
        /* Non-static data member (= field). */
        decl_nonstatic_data_member(&locator, class_type, local_type,
                                   class_state, &decl_info);
#if MICROSOFT_EXTENSIONS_ALLOWED
        is_nonstatic_data_member = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
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
    if (!is_nonstatic_data_member) {
      any_decl_other_than_nonstatic_data_member = TRUE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    remove_stop_token(tok_comma);
    decl_info.is_first_in_declarator_list = FALSE;
    /* Loop for additional declarators. */
  } while (loop_token(tok_comma));
next_declaration:;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode) {
    if (any_decl_other_than_nonstatic_data_member &&
        (decl_info.decl_modifiers.get_property_name != NULL ||
         decl_info.decl_modifiers.put_property_name != NULL)) {
        /* __declspec(property(...)) is allowed only on nonstatic data
           members. */
      pos_diagnostic(es_discretionary_error, ec_declspec_property_not_allowed,
                     &decl_start_pos);
    }  /* if */
    /* Restore the default name linkage if a linkage specification appeared
       among the decl-specifiers. */
    if (dso_flags & DSO_LINKAGE_SPEC_DECL) pop_name_linkage();
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (decl_pos_block_ptr != NULL) {
    /* Return to the caller the extra source position information collected
       for this declaration. */
    *decl_pos_block_ptr = decl_info.decl_pos_block;
  }  /* if */
  db_exit();
  return decl_info.member_sym;
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
          if (del_sym == NULL && !ambiguous) {
            /* There is no operator delete that "corresponds" to this
               operator new (i.e., whose parameter types after the first
               match). */
            pos_stsy_warning(ec_no_corresponding_delete, &sym->decl_position,
                             (char *)(array_pass ? "[]" : ""), sym);
          }  /* if */
        }  /* for */
      }  /* if */
    } else {
      /* Exceptions are not enabled. */
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE */
      /* Just issue a remark if the class has an operator new() but no
         default operator delete() or vice versa. */
      del_sym = opname_member_function_symbol(del_kind, class_type);
      ambiguous = FALSE;
      if (del_sym != NULL) {
        if (del_sym->kind == (a_symbol_kind)sk_projection &&
            !del_sym->variant.projection.is_using_decl) {
          /* Ignore operator delete if it is simply inherited. */
          del_sym = NULL;
        } else {
          del_sym = find_default_operator_delete_sym(del_sym, &ambiguous);
        }  /* if */
      }  /* if */
      /* If del_sym is non-NULL it now points to a default operator delete. */
      if (new_sym != NULL) {
        if (del_sym == NULL && !ambiguous) {
          /* No default operator delete. */
          pos_stsy_remark(ec_class_with_op_new_but_no_op_delete,
                          &error_position, (char *)(array_pass ? "[]" : ""),
                          tag_sym);
        }  /* if */
      } else {
        /* No operator new was declared.  If a default operator delete was
           declared, issue a diagnostic. */
        if (del_sym != NULL) {
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

  if (class_state->last_field_is_incomplete_array) {
    /* The last field that was recorded was an incomplete array.  This is
       permitted in C mode (as an extension), in C99 mode, and in Microsoft
       mode (both C and C++).  If this is strict-ANSI-C89 mode, issue a
       diagnostic.  Otherwise, mark class_type as containing an incomplete
       array member, since there are constraints on how it can be used.
       (E.g., it can't be the element type of an array, and in Microsoft C++
       mode it can't be used as a base class.) */
    check_assertion((C_mode() || microsoft_mode) &&
                    !is_union_type(class_state->class_type));
    class_type->variant.class_struct_union.
                                    contains_flexible_array_member = TRUE;
    if (strict_ansi_mode && !c99_mode) {
      a_field_ptr  fp = class_state->end_of_field_list;
      pos_diagnostic(strict_ansi_error_severity,
                     ec_incomplete_type_not_allowed,
                     &fp->source_corresp.decl_position);
      if (strict_ansi_error_severity == es_error) {
        fp->type = error_type();
        class_type->variant.class_struct_union.
                                    contains_flexible_array_member = FALSE;
      }  /* if */
    }  /* if */
  }  /* if */    
  if (!class_state->is_nonreal_instantiation) {
    if (may_be_added_to_types_list(class_type, effective_decl_level)) {
      /* The type will already have been added to the current scope's types
         list.  However, it should be moved to the end of the list (unless
         it's already there), since its location in the types list should
         record where it was defined, not where it was initially declared.
         move_to_end_of_types_list also takes care of the placeholder
         typerefs associated with this class. */
      move_to_end_of_types_list(class_type, effective_decl_level,
                                /*delete_placeholder=*/FALSE);
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
    check_base_class_destructors(class_type);
    /* Create compiler-generated default constructor, copy constructor,
       destructor, and assignment operator, if any is needed. */
    check_special_member_functions(class_type, class_state);
    if (cssp->is_class_aggregate && !class_state->POD_ruled_out) {
      /* It was intentional to wait until check_special_member_functions
         was called to set the is_POD flag -- the check for copy
         assignment operator was needed first. */
      cssp->is_POD = TRUE;
    }  /* if */
#if ABI_COMPATIBILITY_VERSION >= 232
    /* Go though all the functions declared for this class and set the
       virtual function number of virtual functions.  (Note: with less
       current ABIs the numbers are updated on the fly as the member
       function declaration is processed.) */
    set_virtual_function_numbers(class_type);
#endif /* ABI_COMPATIBILITY_VERSION >= 232 */
    /* Set shares_virtual_function_info for a base class of class_type, if
       appropriate. */
    set_shares_virtual_function_info_flag(class_type,
                                          (a_base_class_ptr)NULL);
  }  /* if */
  /* Do subobject allocation and compute the size and alignment of the
     class. */
  do_class_layout(class_type);
  if (C_dialect == C_dialect_cplusplus) {
    if (!class_state->is_nonreal_instantiation) {
      /* Check for inherited conversion functions.  This must be done before
         rescanning inline function definitions. */
      project_base_class_conversion_functions(class_type);
    }  /* if */
    /* Report errors in virtual function declarations that result from
       the failure to redeclare a virtual function originally declared in
       a virtual base class. */
    set_err_pos_to_curr_token();
    report_virtual_function_ambiguities(class_type);
    /* If the current class is not already marked as "abstract", run
       through its base classes to determine whether it is abstract by
       inheritance and set the flag accordingly. */
    check_abstract_class(class_type);
    /* Issue warnings/remarks if the class has an operator new but no
       operator delete, etc. */
    check_operator_new_and_delete(tag_sym);
    if (class_state->override_registry != NULL) {
      /* Check for incomplete overriding of virtual functions, and issue
         diagnostics where appropriate. */
      check_override_registry(class_state->override_registry, tag_sym);
      /* All entries on the list have been freed, so clear the pointer. */
      class_state->override_registry = NULL;
    }  /* if */
  }  /* if */
}  /* complete_class_definition */


#if !EXTRA_SOURCE_POSITIONS_IN_IL
/*ARGSUSED*/ /* decl_pos_block is not used unless extra source-position
                information is being recorded in the IL. */
#endif /* !EXTRA_SOURCE_POSITIONS_IN_IL */
a_boolean scan_class_definition(a_type_ptr       class_type,
                                a_scope_depth    effective_decl_level,
                                a_scope_depth    orig_decl_level,
                                a_boolean        is_local_class,
                                a_boolean        delayed_nested_class_def,
                                a_boolean        is_template_instantiation,
				a_boolean	 is_template_specialization,
                                a_template_ptr   il_template_entry,
                                a_decl_pos_block *decl_pos_block)
/*
Scan the body of a class definition, including the base classes list.
class_type points to the type entry of the class, struct, or union whose
definition is to be scanned.  effective_decl_level indicates the name scope
to which the class declaration belongs.  orig_decl_level is usually the same
as effective_decl_level, but when the class was specified with a
namespace-qualified name, it is instead the scope depth before the namespace
extension scope was pushed.  is_local_class is TRUE if the class definition
appears inside a function body.  delayed_nested_class_def is TRUE if the
class is a nested class whose parent class definition has already been
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
  a_routine_fixup_ptr              saved_routine_fixup;
  a_template_symbol_supplement_ptr class_tssp;
  a_token_sequence_number          token_number_of_closing_brace;
  a_class_def_state                class_state;
  a_boolean                        skip_semicolon_check;
  a_type_ptr                       dummy_type;
  a_boolean			   instantiation_scope_pushed = FALSE;
  a_boolean			   is_in_class_specialization;
#if USER_CONTROL_OF_STRUCT_PACKING
  a_pack_alignment_state           saved_pack_alignment_state;
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_scope_depth                   class_scope_depth;
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_enter(3, "scan_class_definition");
  initialize_class_def_state(class_type, &class_state);
  class_state.is_local_class = is_local_class;
  /* Increment the counter of class definitions currently in progress. */
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
  /* Determine the alignment adjustment required for packing. */
  class_type->variant.class_struct_union.max_member_alignment =
                                  current_max_alignment_for_class_members();
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
  is_in_class_specialization = is_template_specialization &&
                               !delayed_nested_class_def;
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
    class_type->
      variant.class_struct_union.extra_info->surrounding_name_linkage_state =
                          scope_stack[depth_scope_stack].default_name_linkage;
#endif /* BACK_END_IS_CP_GEN_BE */
    if (class_type->variant.class_struct_union.is_prototype_instantiation ||
        (scope_stack[depth_scope_stack].in_prototype_instantiation &&
         class_type->source_corresp.is_local_to_function)) {
      /* This is a prototype instantiation or an instantiation of a local
         class type, so the resulting class is "nonreal" (i.e., based on
         template arguments that include the dummy types and constants of
         template parameters rather than real types and constants). Note
         that for nested classes the flag is set later. */
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
    } else if (is_template_instantiation && tag_sym->is_class_member) {
      /* An instance of a member template.  Mark it as nonreal if the
         instantiation is being triggered inside a prototype instantiation. */
      if (tag_sym->parent.class_type->
                                 variant.class_struct_union.is_nonreal_class) {
        class_state.is_nonreal_instantiation = TRUE;
        class_type->variant.class_struct_union.is_nonreal_class = TRUE;
      }  /* if */
    } else if (class_type->variant.class_struct_union.is_nonreal_class) {
      /* A definition of a nonreal class that is not a template instantiation.
         This should only occur when defining a specialization of a class
         in a class scope. */
      if (microsoft_mode &&
          class_type->variant.class_struct_union.is_specialized) {
        class_state.is_nonreal_instantiation = TRUE;
      } else {
        /* This can only occur in strange error situations, such as:
             template<template <class X> class T> struct S struct T<int> {};
           Check that an error has been or will be issued. */
        expect_error();
      }  /* if */
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
    }  /* if */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
    if (microsoft_mode && is_template_instance_specific_def_symbol(tag_sym)) {
      /* The Microsoft compiler permits a class specialization to reference
         template parameters of the template.  Push an instantiation scope
         if this is a specialization definition. */
      a_scope_depth  depth;

      push_instantiation_scope_for_class(
                      class_type, /*is_microsoft_specialization_scope=*/FALSE);
      instantiation_scope_pushed = TRUE;
      depth = depth_scope_stack;
      scope_stack[depth].microsoft_specialization_instantiation_scope = TRUE;
      while (scope_stack[depth].nested_instantiation) {
        /* This specialization is nested.  Walk up the scope stack to find
           the specialization it's nested inside of and set the flag there,
           too. */
        do {
          depth--;
          check_assertion(depth > DEPTH_OF_FILE_SCOPE);
        } while (scope_stack[depth].kind !=
                              (a_scope_kind)sck_template_instantiation);
        scope_stack[depth].microsoft_specialization_instantiation_scope = TRUE;
      }  /* while */
    } else if (delayed_nested_class_def && !is_template_instantiation) {
      /* This is a definition of a C++ nested class that appears outside the
         scope of the parent class definition itself.  Reactivate the
         lexical context.  Note that this is done before  the base specifiers
         are scanned so that symbols from the enclosing class are visible.
         For template instantiations, this is done when the template
         instantiation scope is pushed.  Note that this is not done when
         a template instantiation scope is pushed for a specialization
         in Microsoft mode (above) because that process reactivates the
         enclosing class. */
      push_class_reactivation_scope(tag_sym->parent.class_type,
                                    /*extend_namespace=*/TRUE);
    }  /* if */
    if (curr_token == tok_colon) {
      /* Scan the list of base specifiers. */
      add_stop_token(tok_lbrace);
      scan_base_specifier_list(class_type, &class_state);
      remove_stop_token(tok_lbrace);
      /* A class with base classes is not an "aggregate" (ARM 8.4.1). */
      class_state.class_aggregate_ruled_out = TRUE;
      class_state.POD_ruled_out = TRUE;
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
        class_type->variant.class_struct_union.extra_info->base_classes = NULL;
        if (!instantiation_scope_pushed &&
            delayed_nested_class_def && !is_template_instantiation) {
          /* Restore the scope stack to its original state. For template
             instantiations, this is done when the template instantiation
             scope is popped. */
          pop_class_reactivation_scope();
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (curr_token == tok_lbrace) {
    /* Scan the structure or union definition. */
    /* Begin a new stop token state. */
    push_stop_token_stack();
    /* Advance past the left brace. */
    (void)get_token();
    add_stop_token(tok_rbrace);
    /* Start a scope for the fields and other members.  Since the class type
       is allocated in the file scope memory region, all its members must also
       allocated there -- push_scope will switch to the file scope memory
       region; pop_scope will switch back. */
    scope_ptr = push_scope((a_scope_kind)sck_class_struct_union,
                           NO_SCOPE_NUMBER, class_type, (a_routine_ptr)NULL);
    scope_stack[depth_scope_stack].class_def_state = &class_state;
    if (C_dialect == C_dialect_cplusplus) {
      /* In C++ every class, struct, and union type entry will have a non-NULL
         pointer to a class type supplement entry.  Put a pointer to the
         IL scope entry into it. */
      class_type->variant.class_struct_union.extra_info->assoc_scope =
                                                                 scope_ptr;
      saved_routine_fixup = curr_routine_fixup;
      curr_routine_fixup = NULL;
      if (class_name_injection_enabled) {
        /* In C++ the name of the class is entered into the scope of the
           class; enter an sk_type symbol. */
        if (microsoft_bugs &&
            class_type->variant.class_struct_union.is_template_class) {
          /* In Microsoft bugs mode, template class names are not injected. */
        } else {
          enter_injected_class_name_symbol(tag_sym);
        }  /* if */
      }  /* if */
    }  /* if */
    if (curr_token == tok_rbrace) {
      /* A member list is optional in C++.  In C mode issue an error and add
         a dummy field to reduce error recovery problems down the line. */
      if (C_mode()) {
        error(ec_exp_declaration);
        add_error_field(class_type, &class_state.end_of_field_list);
      }  /* if */
    } else {
      if (class_type->kind == (a_type_kind)tk_class) {
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
        add_stop_token(tok_semicolon);
        /* Move cached #pragma declarations (if any) to the current scope
           stack entry so they can be examined and acted upon in subsequent
           processing. */
        (void)select_curr_construct_pragmas(/*add_to_list=*/FALSE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* Reset the insertion point for instantiations to NULL. */
        reset_ss_list_instantiation_insert_point();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        if (C_dialect == C_dialect_cplusplus) {
          /* An access specification may appear anywhere amid the member
             declarations.  Check for it each time through the loop, and adjust
             the value of access accordingly. */
          if (scan_access_specification(&class_state.access)) {
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
          goto next_declaration;
        }  /* if */
        /* Check for an (illegal) asm declaration. */
        if (curr_token == tok_asm || curr_token == tok_microsoft_asm) {
          /* An asm declaration is not allowed in a class definition, but
             scan it anyway (after issuing the error). */
          (void)asm_declaration(/*asm_decl_allowed=*/FALSE,
                                /*is_asm_statement=*/FALSE);
          /* The semicolon will have been consumed by the subroutine.
             Continue looping through the members. */
          goto next_declaration;
        }  /* if */
        if (C_dialect == C_dialect_cplusplus) {
          /* Check for and discard declarations of the form "overload f;". */
          if (check_for_overload_anachronism()) {
            /* Issue diagnostics on pragmas that are trying to bind to an
               overload declaration. */
            cannot_bind_to_curr_construct();
            (void)required_token(tok_semicolon, ec_exp_semicolon);
            goto next_declaration;
          }  /* if */
          /* Check for a using declaration. */
          if (curr_token == tok_using) {
            member_using_declaration(class_type, class_state.access);
            goto next_declaration;
          }  /* if */
          /* Check for an access adjustment declaration. */
          if (is_decl_qualified_name_start() &&
              qualifier_class_type(locator_for_curr_id) != class_type &&
              locator_for_curr_id.is_qualified_name &&
              next_token() == tok_semicolon) {
            /* This looks syntactically like an access adjustment declaration.
               Be sure the semantics are correct.  Its semantics are the same
               as a using-declaration. */
            member_using_declaration(class_type, class_state.access);
            goto next_declaration;
          }  /* if */
          /* Check for template declaration. */
          if (curr_token == tok_template || curr_token == tok_export) {
            /* A template declaration in a class may be a member template
               declaration or a friend declaration.  Explicit instantiations
               are not permitted in a class context.  The error for an
               explicit instantiation in a class will be issued by
               template_directive_or_declaration. */
            a_token_kind  final_token = tok_semicolon;

            template_directive_or_declaration(&final_token, TDO_NO_OPTIONS);
            /* The terminating token will be either a semicolon or a right
               brace.  The latter has already been checked for, but the former
               has not. */
            if (final_token == tok_semicolon) {
              (void)required_token_no_advance(tok_semicolon, ec_exp_semicolon);
            }  /* if */
            /* Advance past the terminating token. */
            if (curr_token == final_token) (void)get_token();
            goto next_declaration;
          }  /* if */
        }  /* if */
        (void)class_member_declaration(class_type, &class_state,
                                       /*is_template_member=*/FALSE,
                                       (a_template_param_ptr)NULL,
                                       &skip_semicolon_check,
                                       &dummy_type,
                                       (a_template_instance_ptr)NULL,
                                       (a_template_ptr)NULL,
                                       (a_decl_pos_block *)NULL);
        if (!skip_semicolon_check) {
          /* Check for and ignore the semicolon following the member
             declaration.  It's optional after the last declaration (that's
             an extension in ANSI mode). */
          if (curr_token == tok_rbrace) {
            /* The final semicolon is omitted. */
            if (C_dialect != C_dialect_pcc) {
              diagnostic(strict_ansi_mode ? strict_ansi_error_severity :
                                            es_warning,
                         ec_exp_semicolon);
            }  /* if */ 
          } else {
            (void)required_token(tok_semicolon, ec_exp_semicolon);
          }  /* if */
        }  /* if */
next_declaration:
        if (curr_routine_fixup != NULL) dispose_of_curr_routine_fixup();
        remove_stop_token(tok_semicolon);
        /* Keep processing member declarations until the closing brace. */
      } while (curr_token != tok_rbrace && curr_token != tok_end_of_source);
      /* Check that a non-empty struct/union in C mode has at least one
         named field. */
      if (C_mode() && strict_ansi_mode && !class_state.any_named_fields) {
        /* Something like "struct S { int:1; };", which has undefined behavior
           according to the C standard.  Issue a warning. */
        warning(ec_no_named_fields);
      }  /* if */
    }  /* if */
    if (is_template_instantiation && delayed_nested_class_def) {
      /* Force the functions to compute the scope depth, if any. */
      effective_decl_level = NO_SCOPE_DEPTH;
    }  /* if */
    if (depth_template_declaration_scope == NO_SCOPE_DEPTH) {
      /* Something went wrong if we are in a template declaration scope;
         we ought to be in class_template_declaration instead.  An error
         has been or will be issued elsewhere. */
      complete_class_definition(class_type, effective_decl_level,
                                &class_state);
    }  /* if */
    /* Process pragmas associated with the closing brace before the current
       scope is popped and before add_end_of_construct_source_sequence_entry
       is called. */
    process_curr_token_pragmas();
#if USER_CONTROL_OF_STRUCT_PACKING
    if (is_template_instantiation &&
        !class_type->variant.class_struct_union.is_prototype_instantiation) {
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
    if (is_template_instantiation && !class_state.is_nonreal_instantiation) {
      /* A class template instantiation (or a nontemplate class nested
         in a template class): if it appears inside a class definition,
         a placeholder typeref must often be added to the types list for
         the class; if one had already been entered, it may have to be
         removed. */
      add_placeholder_for_class_instantiation(class_type);
    }  /* if */
    if (delayed_nested_class_def) {
      /* A nested class defined outside the parent class definition. */
      if (is_template_instantiation || instantiation_scope_pushed) {
        /* The class reactivation scope is popped along with the template
           instantiation scope. */
      } else {
        /* Restore the scope stack to its original state. */
        pop_class_reactivation_scope();
      }  /* if */
      /* Put out a nested-class-definition placeholder, if necessary. */
      if (class_state.is_nonreal_instantiation) {
        /* Ignore prototype (and other non-real) instantiations. */
      } else if (class_type->variant.class_struct_union.
                    referenced_by_class_instantiation_placeholder_typeref) {
        /* If a class-instantiation placeholder has been put out, a
           nested-class-definition placeholder is not needed.  (Note that
           this applies both to member templates and to nontemplate classes
           that are nested within template class instantiations.) */
      } else if (is_template_instantiation &&
                 class_type->source_corresp.parent.class_type->
                     variant.class_struct_union.extra_info->
                     assoc_scope->depth_in_scope_stack != NO_SCOPE_DEPTH) {
        /* Don't put out the nested-class-definition placeholder for a delayed
           definition if the parent class is still on the stack. This may be
           needed both for a member template -- e.g.,
             struct S {
               template <class T> class X { ... };
               X<int> x;
             };
           -- and for a nontemplate class nested in a template class (since
           in that case the nested class is not instantiated immediately when
           it is encountered) -- e.g., 
             template <class T> class A {
               class B { ... };
               B b;
             };
             A<int> a;
           When A<int> is instantiated, the instantiation of A<int>::B is
           delayed but then triggered by the declaration of A<int>::b. */
      } else {
        a_type_ptr  placeholder;

        /* Enter a typedef entry that points at the nested class just
           defined.  It will serve to indicate just where (in the sequence
           of type declarations) the delayed nested type definition
           appeared. */
        placeholder = alloc_type((a_type_kind)tk_typeref);
        placeholder->variant.typeref.type = class_type;
        placeholder->variant.typeref.
                               is_placeholder_for_nested_class_def = TRUE;
        class_type->variant.class_struct_union.
                               nested_class_defined_outside_of_parent = TRUE;
        /* Note that we add the placeholder type to the types list of the
           scope active when the original declaration was seen -- before any
           namespace extension scopes were pushed if the nested class was
           specified with a namespace-qualified name -- for instance:
             namespace N { class A { class B; }; }
             class N::A::B { };
           Here the namespace-extension scope for N is still on the scope
           stack, but we want the placeholder typeref to be added to the file
           scope, which is what orig_decl_level should specify. */
        if (scope_stack[orig_decl_level].il_scope->kind ==
                                            (a_scope_kind)sck_namespace) {
          /* The original declaration scope is a namespace scope instead of
             the file scope.  Make the placeholder a member of the
             namespace. */
          a_namespace_ptr nsp = scope_stack[orig_decl_level].il_scope->
                                                    variant.assoc_namespace;
          set_namespace_membership((a_symbol_ptr)NULL,
                                   &placeholder->source_corresp, nsp);
        }  /* if */
        add_to_types_list(placeholder, orig_decl_level);
      }  /* if */
    }  /* if */
    remove_stop_token(tok_rbrace);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    /* Record the end-of-decl-specifiers source position. */
    if (decl_pos_block != NULL) {
      decl_pos_block->specifiers_range.end = pos_curr_token;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Check for and ignore the closing brace. */
    token_number_of_closing_brace = curr_token_sequence_number;  
    (void)required_token(tok_rbrace, ec_exp_rbrace);
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
      if (!tag_sym->is_class_member || delayed_nested_class_def ||
          is_in_class_specialization) {
        /* For non-nested classes add the class to the list of classes for
           which delayed processing for default argument declarations and
           inline member function definitions must be done.  The actual
           processing will be done when all pending class definitions have
           been completed. */
        add_to_class_fixup_list(class_type, is_template_instantiation);
      }  /* if */
      curr_routine_fixup = saved_routine_fixup;
      if (class_type->variant.class_struct_union.is_prototype_instantiation) {
        a_template_symbol_supplement_ptr      tssp = class_tssp;
        tssp->variant.class_template.prototype_instantiation = tag_sym;
        tssp->variant.class_template.prototype_instantiation_complete = TRUE;
        if (tag_sym->is_class_member && tssp->cache_segment != NULL) {
          /* For a nested class, save the ending token number of the
             definition.  This is only done when the nested class is
             defined within the enclosing class.  When the class is
             defined outside of the enclosing class, cache_segment will be
             NULL. */
          tssp->cache_segment->last_token_number =
                                                 token_number_of_closing_brace;
        }  /* if */
        if (curr_token != tok_semicolon) {
          /* If the token following the closing brace of the class is not 
             a semicolon, then the class (if it is a nested class) is not
             "standalone", meaning that the body cannot be extract from
             the enclosing template.  Nested classes that are not
             standalone cannot be specialized. */
          tssp->variant.class_template.not_standalone_nested_class = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  /* Decrement the counter of class definitions currently in progress. */
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
  if (!type->source_corresp.is_class_member) {
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
      }  /* if */
      if (type->source_corresp.is_class_member) {
        /* Nested class -- be sure parent class is also externally linked. */
        check_type_for_linkage_change(type->source_corresp.parent.class_type,
                                      count);
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
          check_type_for_linkage_change(type->source_corresp.parent.class_type,
                                        count);
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
#if DEBUG
      pch_saved_var_array_elem(num_routine_fixups_allocated),
      pch_saved_var_array_elem(num_class_fixups_allocated),
#endif /* if DEBUG */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Global variables in class_decl.h. */
  register_trans_unit_variable(pending_class_definitions);
  /* Static variables in class_decl.c. */
  register_trans_unit_variable(avail_derivation_steps);
}  /* class_decl_one_time_init */


void class_decl_trans_unit_init(void)
/*
Initializations for class declaration processing that must be done for each
translation unit.
*/
{
  /* Global variables in class_decl.h. */
  pending_class_definitions = 0;
  /* Static variables in class_decl.c. */
  curr_routine_fixup = NULL;
  avail_derivation_steps = NULL;
  def_arg_class_fixup_list = NULL;
  def_arg_class_fixup_list_tail = NULL;
  inline_function_class_fixup_list = NULL;
  inline_function_class_fixup_list_tail = NULL;
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
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
