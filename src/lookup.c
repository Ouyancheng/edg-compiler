/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

lookup.c - Name lookup routines.

*/

/* Header files common to all files. */
#include "fe_common.h"
/* Although symbol_tbl.c is not really a "declaration processing file",
   it turns out that most of the header files it needs are in decl_hdrs.h. */
#include "decl_hdrs.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */


static a_symbol_ptr find_nested_type_symbol(a_symbol_locator *locator)
/*
Find a "semivisible" symbol for a member type that is no longer in scope
(nested class, typedef name, or enumeration).  Such symbols are not found
by the normal lookup procedure but are visible according to the "nested
class anachronism" (ARM 18.3.5) which, it turns out, applies in cfront to
typedefs and enums as well.  They are visible as though they had been
entered in the file, function or block scope that is the containing
nonclass scope (i.e., the declaration scope of the parent class).  Look
for a qualifying symbol on the inactive list for the specified symbol
locator.  In the case of an ambiguity, return NULL.
*/
{
  a_symbol_ptr            sym, nested_type_sym = NULL;
  a_type_ptr              tp;
  a_scope_stack_entry_ptr ssep;
  a_scope_number          effective_scope, effective_scope_of_nested_type;

  db_enter(4, "find_nested_type_symbol");
  if (allow_anachronisms &&
      locator->symbol_header->any_nested_types_on_inactive_list) {
    sym = inactive_symbol_list_from_locator(*locator);
    effective_scope_of_nested_type = NO_SCOPE_NUMBER;
    for (; sym != NULL; sym = sym->next) {
      if (is_tag_symbol(sym) || sym->kind == (a_symbol_kind)sk_type) {
        /* Found a type symbol, but is it nested? */
        if (sym->is_class_member) {
          tp = sym->parent.class_type;
          /* It is a nested member type.  Pop out to the outermost parent
             class to get the nonclass scope number. */
          while (tp->source_corresp.is_class_member) {
            tp = tp->source_corresp.parent.class_type;
          }  /* if */
          /* Ignore classes that are namespace members. */
          if (tp->source_corresp.parent.namespace_ptr != NULL) continue;
          /* The effective scope is the innermost file, function, or block
             scope in which parent class is declared. */
          effective_scope =
                    ((a_symbol_ptr)tp->source_corresp.assoc_info)->decl_scope;
          if (effective_scope == effective_scope_of_nested_type) {
            /* We have an ambiguity.  We could issue a warning or error, but
               we choose to recognize the nested class anachronism only when
               it is "legally" used -- we don't want a message that says,
               "You're doing something nonstandard and what's more you aren't
               even doing it correctly."  Especially since the user may not
               have been intending to do any such thing.  However, this may
               introduce some differences with Cfront (2.1), which is wedded
               to the nested class anachronism is surprising ways. */
            nested_type_sym = NULL;
            break;
          }  /* if */
          /* If the effective scope is still active on the scope stack sym is
             a match.  Look through the scope stack for scope number. */
          ssep = &scope_stack[depth_scope_stack];
          for (;;) {
            if (nested_type_sym == NULL) {
              if (ssep->number == effective_scope) {
                /* sym's effective scope is still active, so sym is a match. */
                nested_type_sym = sym;
                effective_scope_of_nested_type = effective_scope;
                /* Break out of the inner loop, but keep looking at symbols
                   in case there's an ambiguity. */
                break;
              } else {
                /* Continue through the scope stack till the scope is found,
                   if it's still active. */
              }  /* if */
            } else {
              /* We already have a symbol but ambiguity has been ruled out.
                 If the current symbol also has an effective scope that's
                 that's still active, keep the symbol whose effective scope
                 is closer to the top of the scope stack. */
              if (ssep->number == effective_scope) {
                /* sym's effective scope is still active and is higher. */
                nested_type_sym = sym;
                effective_scope_of_nested_type = effective_scope;
                break;
              } else if (ssep->number == effective_scope_of_nested_type) {
                /* Other symbol's effective scope is higher.  Break out of
                   the inner loop but keep looking at symbols. */
                break;
              }  /* if */
            }  /* if */
            /* End the loop when we reach the bottom of the scope stack. */
            if (ssep == &scope_stack[DEPTH_OF_FILE_SCOPE]) break;
            ssep--;
          }  /* for */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  db_exit();
  return nested_type_sym;
}  /* find_nested_type_symbol */


static
a_symbol_ptr find_synthesized_projection_symbol(
                              a_symbol_locator          *locator,
                              an_id_lookup_options_set	options,
                              a_boolean			qualified_lookup,
			      a_namespace_ptr		qualifier_namespace)
/*
Look for a synthesized projection symbol from a previous lookup that
can be reused to capture the results of this lookup.  locator
represents the symbol to be found.

qualified_lookup is TRUE if the symbol being found is the result of
a namespace or file scope qualified lookup.  For qualified lookups
qualifier_namespace points to the namespace in which the lookup is
being done, or is NULL for a file scope lookup.  options specifies
the options being used for the lookup.
*/
{
  a_symbol_ptr		sym = NULL;
  a_symbol_header_ptr	sym_hdr = locator->symbol_header;

  if (is_reusable_using_directive_lookup(options)) {
    /* Only search if the lookup options represent a lookup whose results
       may be reused. */
    a_boolean		must_be_class_or_namespace
                             = (options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0;
    a_boolean		must_be_tag = (options & IDL_MUST_BE_TAG) != 0;
    a_boolean		must_be_class = (options & IDL_MUST_BE_CLASS) != 0;
    a_boolean		must_be_namespace
                                      = (options & IDL_MUST_BE_NAMESPACE) != 0;
    a_boolean		tentative_type_lookup
                                  = (options & IDL_TENTATIVE_TYPE_LOOKUP) != 0;
    a_scope_number	scope_number = scope_stack[depth_scope_stack].number;
    a_boolean		instantiation_context_lookup =
                                    (options & IDL_INSTANTIATION_CONTEXT) != 0;
    /* Loop through the list of other symbols.  Look for symbols marked
       as synthesized namespace projection symbols. */
    for (sym = sym_hdr->other_symbols; sym != NULL; sym = sym->next) {
      /* Look for symbols whose lookup characteristics match the current
         lookup. */
      if (sym->synthesized_namespace_projection &&
          (a_boolean)sym->qualified_lookup == qualified_lookup &&
          sym->parent.namespace_ptr == qualifier_namespace &&
          (a_boolean)sym->must_be_class_or_namespace_lookup ==
                                                 must_be_class_or_namespace &&
          (a_boolean)sym->instantiation_context_lookup ==
                                              instantiation_context_lookup &&
          (a_boolean)sym->tentative_type_lookup == tentative_type_lookup &&
          (a_boolean)sym->must_be_namespace_lookup == must_be_namespace &&
          (a_boolean)sym->must_be_class_lookup == must_be_class &&
          (a_boolean)sym->must_be_tag_lookup == must_be_tag) {
        /* If this is not a qualified lookup, the decl_scope of the symbol
           must match the current scope. */
        if (qualified_lookup) {
          break;
        } else {
          if (sym->decl_scope == scope_number) break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  if (sym != NULL) {
   if (sym->kind == (a_symbol_kind)sk_namespace_projection &&
        !is_function_or_template_symbol(fundamental_symbol_of(sym))) {
      /* A previous lookup created a synthesized namespace
         projection symbol.  If the fundamental symbol is not a
	 function symbol, clear the fundamental symbol pointed
         to.  This is needed in case something has changed that would
         cause a different symbol to be found. */
      sym->variant.namespace_projection.fundamental_symbol = NULL;
    }  /* if */
  }  /* if */
  return sym;
}  /* find_synthesized_projection_symbol */


a_symbol_ptr curr_scope_id_lookup(a_symbol_locator         *locator,
                                  an_id_lookup_options_set options)
/*
Lookup, in the current scope, the identifier indicated by *locator and
return a pointer to the symbol found, or NULL if the symbol is not found.
options contains bits indicating special restrictions, i.e., the symbol
must a tag.  Projection symbols are only considered in the lookup if
IDL_PROJ_SYMBOL_ALLOWED is specified in options.
*/
{
  a_symbol_ptr			sym;
  a_scope_number		scope_number;
  a_boolean			must_be_tag = (options & IDL_MUST_BE_TAG);
  a_boolean			projection_allowed =
                                           (options & IDL_PROJ_SYMBOL_ALLOWED);
  a_scope_stack_entry_ptr	ssep;
  a_name_space_kind		required_name_space_kind = nsk_other;

/* Local macro that tests whether or not a symbol is acceptable. */
#define is_acceptable_symbol(sym, fund_sym)                             \
   ((!must_be_tag || is_tag_symbol(fund_sym)) &&			\
    (name_space_for_symbol_kind[(int)sym->kind] ==			\
                                           required_name_space_kind) && \
    (projection_allowed || sym->kind != (a_symbol_kind)sk_projection))

  check_assertion_str2((options & ~(IDL_MUST_BE_TAG |
                                    IDL_PROJ_SYMBOL_ALLOWED |
                                    IDL_HIDDEN_NAME_LOOKUP)) == 0,
                       "curr_scope_id_lookup:", "invalid_option");
  /* In C mode, a "must be tag" lookup only considers symbols in the tag
     name space kind. */
  if (C_mode() && must_be_tag) required_name_space_kind = nsk_tag;
  sym = locator->specific_symbol;
  if (is_error_locator(*locator)) {
    /* The locator is an error locator, so return NULL (i.e., no symbol
       found). */
    sym = NULL;
  } else if (sym != NULL) {
    a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
    check_assertion(is_acceptable_symbol(sym, fund_sym));
    /* The locator is for a specific symbol, so return the symbol for it. */
  } else {
    ssep = &scope_stack[decl_scope_level];
    /* Look for a symbol in the current scope for which the kind matches that
       of the scope level specified by the caller. */
    scope_number = ssep->number;
    sym = symbol_list_from_locator(*locator);
    for (; sym != NULL; sym = sym->next) {
      a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
      if (sym->decl_scope == scope_number &&
          is_acceptable_symbol(sym, fund_sym)) {
         /* Found it. */
         break;
      }  /* if */
    }  /* for */
    if (sym == NULL && ssep->kind == (a_scope_kind)sck_namespace_extension) {
      /* If no symbol was found on the active list, and this is a namespace
         extension, then look on the inactive list too.  This doesn't
         have to be done for original namespace scopes because their symbols
         will still be on the active list. */
      a_symbol_ptr	tag_symbol = NULL;
      for (sym = inactive_symbol_list_from_locator(*locator);
           sym != NULL;
           sym = sym->next) {
        a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
        if (sym->decl_scope == scope_number &&
            is_acceptable_symbol(sym, fund_sym)) {
          /* Found an acceptable symbol. */
          /* If the symbol is a tag symbol, there's the possibility that
             there is a non-type symbol in the same scope later in the list
             (because the inactive list is not ordered in any way).  Save the
             tag symbol and keep looking.  If nothing else turns up,
             use the tag symbol. */
          if (!is_tag_symbol(fund_sym)) {
            tag_symbol = NULL;
            break;
          }  /* if */
          tag_symbol = sym;
        }  /* if */
      }  /* for */
      /* We reached the end of the list.  If there is a tag symbol saved
         within the loop, use it. */
      if (tag_symbol != NULL) sym = tag_symbol;
    }  /* if */
    locator->specific_symbol = sym;
  }  /* if */
  /* If the symbol is a projection symbol, reduce it to the fundamental
     symbol.  The specific_symbol in the locator stays pointing to the
     projection symbol. */
  if (sym != NULL) reduce_projection_symbol_to_fundamental_symbol(sym);
  return sym;
#undef is_acceptable_symbol
}  /* curr_scope_id_lookup */


#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG

static a_symbol_ptr check_for_cfront_name_lookup_bug
					(a_type_ptr                class_type,
					 a_symbol_ptr	           sym,
                                         a_symbol_locator          *locator,
					 an_id_lookup_options_set  options)
/*
Cfront 2.1 has a bug that causes a global identifier to be found when
a member of a class or one of its base classes should actually be found.
The following code illustrates an instance in which the bug occurs:

struct B   {
	void func(const char*);	// Needs to be here
};

struct D : public B {
public:
	D();
	void Init(const char* );
};

struct func {
	func( const char* msg);
};

D::D(){}

void D::Init(const char* t)
{
	new func(t);
}

For the bad lookup to occur:

1. A member in a base class must have the same name as an identifier
   at the global scope.  Any member kind is OK -- it can be a function,
   static data member, or nonstatic data member.  Member type names don't
   apply because a nested type will be promoted to the global scope by
   cfront which disallows a later declaration of a type with the same name
   at the global scope.

2. The declaration of the global scope name must occur between the declaration
   of the derived class and the declaration of either an out-of-line
   constructor or destructor.  The global scope name must be a type name.

3. No other member function definition -- even one for an unrelated class
   may appear between the destructor and the offending reference.
   This has the effect that the bad lookup applies to only one class at
   any given point in time.

The global variable last_ctor_or_dtor_sym is set by function_declaration
when the body of a constructor or destructor that is defined outside of
the class definition is processed.  This field is cleared when any other
member function is defined.
*/
{
  a_derivation_step_ptr	path = NULL;
  an_access_specifier   access;
  a_boolean             ambiguous, any_using_decl;
  a_symbol_ptr          new_sym = sym;

  /* Before this routine is called we will have already verified that the
     lookup terminated in the class reactivation scope for the same class
     as indicated by last_ctor_or_dtor_sym.  This means that we are
     in a member function definition (or static data member definition) of
     a class for which a constructor or destructor was just defined. */
  if (sym == NULL || sym->kind != (a_symbol_kind)sk_projection) {
    /* If sym is NULL it means that a symbol was found but it was not a
       type when a tentative type lookup was being done.  If the symbol
       is not a projection symbol, then the name was found in the
       derived class.  For the bug to occur, the name must be defined
       in the base class even if the name was redefined in the derived
       class.  Look for the name in a base class. */
    a_boolean  unambiguous_injected_template = FALSE;

    new_sym = find_progenitor_symbol(class_type, locator,
                                     IDL_NO_OPTIONS,
                                     /*qualified_lookup=*/FALSE, &path,
                                     &access,
                                     &ambiguous, &any_using_decl,
                                     &unambiguous_injected_template);
  }  /* if */
  if (new_sym != NULL) {
    a_symbol_ptr  fund_sym = fundamental_symbol_of(new_sym);
    if (!(is_type_symbol(fund_sym) &&
          type_symbol_type(fund_sym)->
                       use_cfront_transitional_nested_type_name_mangling)) {
      /* Names that are in essence promoted to file scope are not
         considered. */
      a_symbol_ptr file_scope_sym;
      /* new_sym must now be a projection symbol or progenitor symbol.  Look
         for a symbol with the same name at file scope. */
      check_assertion(class_type != fund_sym->parent.class_type);
      file_scope_sym = file_scope_id_lookup(locator, options);
      if (file_scope_sym != NULL && is_type_symbol(file_scope_sym)) {
        /* A file scope symbol was found.  For the incorrect lookup to be
           done the file scope symbol must have been declared after the
           derived class but before the most recent constructor or
           destructor body. */
        a_symbol_ptr	class_sym;
        class_sym = (a_symbol_ptr)class_type->source_corresp.assoc_info;
        if ((compare_source_positions(&file_scope_sym->decl_position,
                                      &class_sym->decl_position) > 0) &&
            (compare_source_positions(&file_scope_sym->decl_position,
                                      &last_ctor_or_dtor_sym->
                                                       decl_position) < 0)) {
          sym = file_scope_sym;
          pos_sy2_warning(ec_cfront_name_lookup_bug,
                          &locator->source_position,
                          sym, new_sym);
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return (sym);
}  /* check_for_cfront_name_lookup_bug */
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */


a_type_ptr proxy_class_for_template_param(a_type_ptr   templ_param_type)
/*
Return the proxy class associated with a template parameter.  If one does
not already exist, one is created.  Creation of the proxy class consists
of allocating and initializing the class and assigning a scope number.
The class type is created the first time that a template parameter is
used in a context in which a class type is required.  This includes
use in a qualified name lookup where the template parameter is used
as the class type, and use as a base class. 
*/
{
  a_type_ptr				type;
  a_template_param_type_supplement_ptr	tptsp;
  a_symbol_ptr				sym;
  a_symbol_ptr				templ_param_sym;
  a_class_symbol_supplement_ptr		cssp;

  tptsp = templ_param_type->variant.template_param.extra_info;
  /* If the template parameter does not yet have a proxy class, create one
     now. */
  if (tptsp->class_type == NULL) {
    /* Get the symbol pointer associated with the template parameter. */
    templ_param_sym =
                     (a_symbol_ptr)templ_param_type->source_corresp.assoc_info;
    if (templ_param_sym == NULL) {
      /* No template parameter symbol.  This is the case when getting the
         proxy class for type_of_unknown_templ_param_nontype. */
      sym = make_unnamed_tag_symbol((a_symbol_kind)sk_class_or_struct_tag,
                                    &null_source_position);
    } else {
      /* Create a symbol for the class.  The symbol will have the same name
         as the template parameter symbol.  mark_declared is not called
         because this symbol is not visible to the user. */
      sym = alloc_symbol((a_symbol_kind)sk_class_or_struct_tag,
                         templ_param_sym->header,
                         &templ_param_sym->decl_position);
    }  /* if */
    /* The class will be considered to be at file scope.  If this is changed
       to be some other scope then set_source_corresp_with_scope_depth may
       need to be called because set_source_corresp requires that the
       decl_scope of the symbol still be an active scope. */
    sym->decl_scope = file_scope_number;
    /* Create the type for the class. */
    type = alloc_type((a_type_kind)tk_class);
    /* Set the size and alignment so that the type will be considered to
       be complete. */
    type->size = 1;
    type->alignment = 1;
    set_source_corresp(&(type->source_corresp), sym);
    sym->variant.class_struct_union.type = type;
    if (templ_param_type->source_corresp.is_class_member) {
      set_class_membership(sym, &type->source_corresp,
                           templ_param_type->source_corresp.parent.class_type);
    }  /* if */
    tptsp->class_type = type;
    /* Set the scope number. */
    cssp = symbol_supplement_for_class(type);
    cssp->member_decl_scope = take_next_scope_number();
    cssp->template_param_for_proxy_class = templ_param_type;
    type->variant.class_struct_union.is_nonreal_class = TRUE;
    if (prototype_instantiations_in_il) {
      /* When prototype instantiations are included in the IL, add the proxy
         class to the IL. */
      add_to_types_list(type, DEPTH_OF_FILE_SCOPE);
    }  /* if */
  }  /* if */
  return tptsp->class_type;
}  /* proxy_class_for_template_param */


static a_symbol_ptr create_unknown_function_symbol(
				a_symbol_header_ptr	sym_hdr,
				a_type_ptr		parent_class,
				a_namespace_ptr		parent_namespace,
				a_boolean		is_qualified_name,
				a_type_ptr		conversion_type)
/*
Create a ck_constant entry of kind tpck_unknown_function, and an sk_constant
symbol that points to the constant.  These constants are used during
prototype instantiations to represent functions in dependent calls.
The symbol is created using the symbol header information from sym_hdr
and the parent information specified by parent_class and parent_namespace.
The is_qualified_name and conversion_type fields in the constant are set
as indicated by is_qualified_name and conversion_type.
*/
{
  /* Create a ck_template_param constant.  We don't know the type of the
     constant so we use a special template parameter type that represents
     the type of an unknown constant. */
  a_constant_ptr		constant;
  a_scope_depth			depth = NO_SCOPE_DEPTH;
  a_symbol_ptr			sym;
  a_source_correspondence	*scp;

#if RECORD_SCOPE_DEPTH_IN_IL
  {
    a_source_correspondence	*parent_scp = NULL;
    /* Get the source correspondence entry associated with the parent class
       or namespace, if any. */
    if (parent_class != NULL) {
      parent_scp = &parent_class->source_corresp;
    } else {
      if (parent_namespace != NULL) {
        parent_scp = &parent_namespace->source_corresp;
      }  /* if */
    }  /* if */
    if (parent_scp != NULL) {
      depth = parent_scp->scope_depth;
    }  /* if */
  }
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
  /* Create a symbol for the member.  mark_declared is not called
     because this symbol is not visible to the user. */
  sym = alloc_symbol((a_symbol_kind)sk_constant, sym_hdr,
                     &null_source_position);
  constant = fs_constant((a_constant_repr_kind)ck_template_param);
  set_template_param_constant_kind(
              constant, (a_template_param_constant_kind)tpck_unknown_function);
  sym->variant.constant = constant;
  constant->type = type_of_unknown_templ_param_nontype;
  constant->variant.template_param.is_qualified_name = is_qualified_name;
  constant->variant.template_param.variant.unknown_function.conversion_type =
                                                               conversion_type;
  scp = &constant->source_corresp;
  if (scp != NULL) set_source_corresp_with_scope_depth(scp, sym, depth);
  if (parent_class != NULL) {
    set_class_membership(sym, scp, parent_class);
  } else if (parent_namespace != NULL) {
    set_namespace_membership(sym, scp, parent_namespace);
  }  /* if */
  sym->is_unknown_function = TRUE;
  return sym;
}  /* create_unknown_function_symbol */


a_symbol_ptr find_unknown_function_symbol(
					a_symbol_ptr	orig_sym,
					a_boolean	is_qualified_name)
/*
Find an sk_constant symbol that points to a constant of kind
tpck_unknown_function.  The symbol name must match that of orig_sym
and must have the same parent scope.  The is_qualified_name flag in
the constant must match is_qualified_name.

If a matching symbol cannot be found, one is created.  The list of
previously created symbols is included in the "other symbols" list
of the symbol header.
*/
{
  a_symbol_ptr	sym;

  /* Look for a previously created unknown function symbol. */
  for (sym = orig_sym->header->other_symbols; sym != NULL; sym = sym->next) {
    if (sym->is_unknown_function &&
        sym->variant.constant->variant.template_param.is_qualified_name ==
                                                           is_qualified_name) {
      if (sym->is_class_member == orig_sym->is_class_member) {
        if (sym->is_class_member &&
            sym->parent.class_type == orig_sym->parent.class_type) {
          /* The symbols have the same parent class. */
          break;
        } else if (sym->parent.namespace_ptr ==
                                              orig_sym->parent.namespace_ptr) {
          /* The symbols have the same parent namespace (including the case
             where both have no namespace). */
          break;
        }  /* if */
      }  /* if */
    }  /* if */
  } /* for */
  if (sym == NULL) {
    /* No symbol was found -- create one now. */
    a_symbol_header_ptr		hdr;
    a_type_ptr			parent_class = NULL;
    a_namespace_ptr		parent_namespace = NULL;
    a_constant_ptr		con;
    if (orig_sym->is_class_member) {
      parent_class = orig_sym->parent.class_type;
    } else {
      parent_namespace = orig_sym->parent.namespace_ptr;
    }  /* if */
    sym = create_unknown_function_symbol(orig_sym->header, parent_class,
                                         parent_namespace, is_qualified_name,
                                         (a_type_ptr)NULL);
    check_assertion(sym->kind == (a_symbol_kind)sk_constant);
    con = sym->variant.constant;
    check_assertion(con->kind == (a_constant_repr_kind)ck_template_param &&
                    con->variant.template_param.kind ==
                        (a_template_param_constant_kind)tpck_unknown_function);
    /* Save the original symbol (probably an overload set) for use later
       in copy_type_with_substitution. */
    con->variant.template_param.variant.unknown_function.symbol = orig_sym;
    hdr = sym->header;
    /* Link this symbol onto the other symbols list. */
    sym->next = hdr->other_symbols;
    hdr->other_symbols = sym;
  }  /* if */
  return sym;
}  /* find_unknown_function_symbol */


/*
Macro that returns the symbol kind for a nonreal member created for the
current lookup options.  The symbol is created as a class template
if a "treat as template ID" lookup is done.  The symbol is created as a
type if the lookup is a "must be class or namespace", "must be tag" or
"typename lookup".  In addition, if "implicit typename" is enabled, we
also force the member to be a type when doing a "tentative type" lookup
(but not if the name being looked up is something that could not be
a typename, like a destructor).
  
Implicit typename mode is used to compile code that was not written
using "typename".  If the symbol is not considered to be a class
template or a type, then it is created as a constant.
*/
#define nonreal_member_symbol_kind(locator, options)		\
  ((a_symbol_kind)((options & IDL_TREAT_AS_TEMPLATE_ID)		\
    ? sk_class_template						\
    : 								\
      (options & IDL_MUST_BE_CLASS_OR_NAMESPACE ||		\
       options & IDL_MUST_BE_TAG ||				\
       options & IDL_MUST_BE_CLASS ||				\
       options & IDL_TYPENAME_LOOKUP) ||				\
      (implicit_typename_enabled && (options & IDL_TENTATIVE_TYPE_LOOKUP) && \
       !(locator)->is_destructor_name && !(locator)->is_conversion_name &&   \
       !(locator)->is_operator_name) \
        ? sk_type						\
        :							\
          sk_constant))


static a_symbol_ptr create_proxy_or_nonreal_class_member_of_kind(
				a_type_ptr		class_type,
				a_symbol_kind		kind,
				a_symbol_locator	*locator)
/*
Create a proxy or nonreal member with the specified symbol kind.
class_type is the nonreal class in which the member is to be created.
"locator" is used to get the name, source position, and conversion
result type.

The member that is created is not added to the inactive list by this
routine.
*/
{
  a_class_symbol_supplement_ptr cssp;
  a_scope_depth                 depth = NO_SCOPE_DEPTH;
  a_symbol_ptr                  sym;
  a_source_correspondence       *scp = NULL;

  db_enter(4, "create_proxy_or_nonreal_class_member_of_kind");
  /* Create a symbol for the member.  mark_declared is not called
     because this symbol is not visible to the user. */
  sym = alloc_symbol(kind, locator->symbol_header, &locator->source_position);
  /* Get the scope number from the symbol supplement.  The scope depth
     will be the scope depth of the class plus one. */
  cssp = symbol_supplement_for_class(class_type);
  sym->decl_scope = cssp->member_decl_scope;
  sym->is_nonreal_member = TRUE;
#if RECORD_SCOPE_DEPTH_IN_IL
  depth = class_type->source_corresp.scope_depth;
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
  /* Create the type or constant. */
  switch (kind) {
    case sk_type:
    {
      a_type_ptr	type = alloc_type((a_type_kind)tk_template_param);
      type->variant.template_param.kind =
                                    (a_template_param_type_kind)tptk_member;
      set_type_size(type);
      sym->variant.type.ptr = type;
      scp = &type->source_corresp;
      break;
    }
    case sk_constant:
    {
      /* Create a ck_template_param constant.  We don't know the type of the
         constant so we use a special template parameter type that represents
         the type of an unknown constant. */
      a_constant_ptr	constant;
      constant = fs_constant((a_constant_repr_kind)ck_template_param);
      if (locator->is_conversion_name) {
        /* For a conversion operator, create an "unknown function" entry that
           represents the conversion result type. */
        set_template_param_constant_kind(
              constant, (a_template_param_constant_kind)tpck_unknown_function);
        constant->variant.template_param.variant.unknown_function.
                     conversion_type = locator->variant.conversion_result_type;
      } else {
        /* For everything except a conversion function create a generic
           nontype member. */
        set_template_param_constant_kind(
                        constant, (a_template_param_constant_kind)tpck_member);
      }  /* if */
      sym->variant.constant = constant;
      constant->type = type_of_unknown_templ_param_nontype;
      scp = &constant->source_corresp;
      break;
    }
    case sk_static_data_member:
    {
      /* Create a static data member whose type is the unknown nontype
         type. */
      a_variable_ptr	var;
      var = make_variable(type_of_unknown_templ_param_nontype,
                          (a_storage_class)sc_extern, NO_SCOPE_DEPTH);
      sym->variant.static_data_member.variable = var;
      scp = &var->source_corresp;
      break;
    }
    case sk_class_template:
    {
      /* This is a template used in a context like T::A<int>.  A template
         symbol is created for T::A.  Indicate that this template is
         a nonreal class member. */
      a_template_symbol_supplement_ptr	tssp;
      a_template_ptr			templ_ptr;
      tssp = sym->variant.template_info;
      tssp->is_nonreal_member = TRUE;
      tssp->variant.class_template.type_kind = (a_type_kind)tk_class;
      tssp->variant.class_template.access = (an_access_specifier)as_public;
      templ_ptr = alloc_template();
      /* The templates associated with template parameters and nonreal classes
         have a template_info pointer that points back to the front end
         information. */
      templ_ptr->template_info = tssp;
      scp = &templ_ptr->source_corresp;
      templ_ptr->kind = (a_template_kind)templk_class;
      tssp->il_template_entry = templ_ptr;
      if (prototype_instantiations_in_il) {
        /* When prototype instantiations are included in the IL, add the
           template to the IL.  We arbitrarily use the file scope as the
           place to add it. */
        add_to_templates_list(templ_ptr, DEPTH_OF_FILE_SCOPE);
      }  /* if */
      break;
    }
    default:
      unexpected_condition();
  }  /* switch */
  if (scp != NULL) set_source_corresp_with_scope_depth(scp, sym, depth);
  set_class_membership(sym, scp, class_type);
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "Created: ");
    db_symbol(sym, "", 0);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return sym;
}  /* create_proxy_or_nonreal_class_member_of_kind */


a_symbol_ptr create_proxy_or_nonreal_class_member
					(a_type_ptr	          class_type,
					 an_id_lookup_options_set options,
					 a_symbol_locator         *locator)
/*
This routine is called by class_qualified_id_lookup when the name
being looked up is not found in the proxy class associated with a
template parameter type or in a class that is a nonreal instantiation.
We don't know anything about the name that is being looked up except
whether or not it is a type (inferred from the lookup options).  If
the name is a type, we create a member of class_type that is a
tk_template_param; otherwise, we create a member of class_type that is
a ck_template_param.

The member that is created is not added to the inactive list by this
routine.
*/
{
  a_symbol_kind                 kind;
  a_symbol_ptr                  sym;

  db_enter(4, "create_proxy_or_nonreal_class_member");
  /* Determine the symbol kind to be created.  The symbol can be a
     type, constant, or class template, depending on the kind of
     lookup being done. */
  kind = nonreal_member_symbol_kind(locator, options);
  sym = create_proxy_or_nonreal_class_member_of_kind(class_type, kind,
                                                     locator);
  db_exit();
  return sym;
}  /* create_proxy_or_nonreal_class_member */


static a_symbol_ptr add_member_to_proxy_or_nonreal_class
					(a_type_ptr	          class_type,
					 an_id_lookup_options_set options,
					 a_symbol_locator         *locator)
/*
Creates a proxy or nonreal class member of class_type.  Adds the newly
created symbol to the inactive list and returns it to the caller.
*/
{
  a_symbol_ptr	sym;

  sym = create_proxy_or_nonreal_class_member(class_type, options, locator);
  /* Add the symbol to the inactive list. */
  add_symbol_to_inactive_list(sym);
  return sym;
}  /* add_member_to_proxy_or_nonreal_class */


static
a_symbol_ptr enter_sym_for_out_of_scope_routine(a_symbol_ptr     extern_sym,
						a_symbol_locator *locator)
/*
Create a symbol entry in the current scope for the external routine
designated by extern_sym.  Return the symbol pointer to the caller.
This routine is only used in SVR4 C compatibility mode.  It is
called by find_out_of_scope_declaration when a normal lookup fails
and there is an external symbol (created by some other scope) that
should be used to satisfy the lookup.
*/
{
  an_id_linkage_kind     linkage;
  a_type_ptr             rout_type, old_type;
  a_symbol_ptr           ext_sym;
  a_func_info_block      func_info;
  a_symbol_ptr           sym;
  a_decl_modifiers_block decl_modifiers;

  /* This routine must only be called in ANSI C mode. */
  check_assertion(C_dialect == C_dialect_ANSI);
  clear_decl_modifiers_block(&decl_modifiers);
  /* Enter the symbol in the symbol table.  decl_routine expects
     this to be done by the caller for implicitly declared routines. */
  sym = enter_symbol((a_symbol_kind)sk_routine, locator, depth_scope_stack,
		     /*suppress_error=*/FALSE);
  /* Create a local declaration of the external routine.  Use the
     type from the sk_extern_routine symbol. */
  rout_type = extern_sym->variant.extern_symbol_descr->type;
  /* Declare the function identifier. */
  clear_func_info(&func_info);
  func_info.is_implicit_declaration = TRUE;
  if (exceptions_enabled) func_info.throw_position = locator->source_position;
  decl_routine(locator, (a_storage_class)sc_extern, rout_type,
                      &func_info, (a_source_sequence_entry_ptr)NULL,
                      (SRK_DECLARATION | SRK_IMPLICIT), &decl_modifiers,
                      &sym, &linkage, &old_type, &ext_sym,
                      (a_decl_pos_block_ptr)NULL);
  done_with_func_info(func_info);
  /* Set the referenced flag on the routine entry.  The implicit declaration
     is also an immediate reference. */
  sym->variant.routine.ptr->source_corresp.referenced = TRUE;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  return sym;
}  /* enter_sym_for_out_of_scope_routine */


static
a_symbol_ptr enter_sym_for_out_of_scope_variable(a_symbol_ptr     extern_sym,
						 a_symbol_locator *locator)
/*
Create a symbol entry in the current scope for the external variable
designated by extern_sym.  Return the symbol pointer to the caller.
This routine is only used in SVR4 C compatibility mode.  It is
called by find_out_of_scope_declaration when a normal lookup fails
and there is an external symbol (created by some other scope) that
should be used to satisfy the lookup.
*/
{
  an_id_linkage_kind     linkage;
  a_type_ptr             var_type, old_type;
  a_symbol_ptr           ext_sym;
  a_symbol_ptr           sym;
  a_decl_modifiers_block decl_modifiers;

  /* This routine must only be called in ANSI C mode. */
  check_assertion(C_dialect == C_dialect_ANSI);
  clear_decl_modifiers_block(&decl_modifiers);
  /* Create a local declaration of the external variable.  Use the type
     from the sk_extern_variable symbol. */
  var_type = extern_sym->variant.extern_symbol_descr->type;
  decl_variable(locator, (a_storage_class)sc_extern, var_type,
                (a_source_sequence_entry_ptr)NULL,
                (SRK_DECLARATION | SRK_IMPLICIT), &decl_modifiers, &sym,
                &linkage, &old_type, &ext_sym, (a_decl_pos_block_ptr)NULL);
  /* Set the referenced flag on the variable entry.  The implicit declaration
     is also an immediate reference. */
  sym->variant.variable.ptr->source_corresp.referenced = TRUE;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 4);
  }  /* if */
#endif /* DEBUG */
  return sym;
}  /* enter_sym_for_out_of_scope_variable */


static
a_symbol_ptr find_out_of_scope_declaration(a_symbol_locator         *locator,
                                           an_id_lookup_options_set options)
/*
This is an SVR4 compatibility feature that is now a default ANSI C
mode feature.  This routine is used to make external symbol declarations
from other scopes visible in the current scope.  For example

int f1(void)
{
  extern void f();
  extern int i;
}

int f2()
{
  int j;
  f();
  j = i;
}

In this example, symbols for f and i are entered in function f2.  They
refer to the external entities declared by the declarations in f1.

When a symbol lookup fails, this routine is called to see if an external
symbol exists with the name being looked up.  If so, a new symbol is
entered in the current scope that refers to the external entity.
"options" specifies the lookup options being used.  The external symbol
that is found must meet the criteria specified by the lookup options.
A warning is issued.  A pointer to the new symbol is returned.  If no
such pointer is found, NULL is returned.
*/
{
  a_symbol_locator  new_locator;
  a_symbol_ptr      sym = NULL;;

  sym = find_external_symbol(locator, (a_name_linkage_kind)nlk_external,
			     (a_type_ptr)NULL, &new_locator);
  if (sym != NULL && sym->kind == (a_symbol_kind)sk_extern_routine && 
      sym->variant.extern_symbol_descr->
                                  variant.routine.is_implicit_declaration) {
    /* Don't redeclare a previous symbol if it was implicitly
       declared. */
    sym = NULL;
  } else if (sym != NULL && !sym_matches_lookup_options(sym, options)) {
    /* The external symbol does not satisfy the lookup criteria. */
    sym = NULL;
  }  /* if */
  if (sym != NULL) {
    sym_warning(ec_using_out_of_scope_declaration, sym);
    if (sym->kind == (a_symbol_kind)sk_extern_routine) {
      sym = enter_sym_for_out_of_scope_routine(sym, locator);
    } else if (sym->kind == (a_symbol_kind)sk_extern_variable) {
      sym = enter_sym_for_out_of_scope_variable(sym, locator);
    } else {
      unexpected_condition();
    }  /* if */
  }  /* if */
  return sym;
}  /* find_out_of_scope_declaration */


a_boolean symbols_are_lookup_equivalent(a_symbol_ptr	sym1,
                                        a_symbol_ptr	sym2)
/*
Returns TRUE if sym1 is the same as sym2 or if sym1 and sym2 point
to the same IL entities.  The latter check is used, for example, to
determine whether two symbols from different namespaces point to
the same underlying extern "C" variable or function.  Two such symbols
that appear in the same using-directive lookup set are considered to
represent the same entity, so one of the two symbols is arbitrarily
selected.  sym1 and sym2 must have been reduced to their fundamental
symbols by the caller.
*/
{
  a_boolean	result = FALSE;
  if (sym1 == sym2) {
    /* The symbols are the same. */
    result = TRUE;
  } else if (sym1->kind == sym2->kind) {
    /* The symbols refer to the same kind of entity -- check further. */
    if (sym1->kind == (a_symbol_kind)sk_variable) {
      result = sym1->variant.variable.ptr == sym2->variant.variable.ptr;
    } else if (sym1->kind == (a_symbol_kind)sk_routine) {
      result = sym1->variant.routine.ptr == sym2->variant.routine.ptr;
    } else if (is_type_symbol(sym1)) {
      a_type_ptr  tp1 = type_symbol_type(sym1), tp2 = type_symbol_type(sym2);
      result = identical_types(tp1, tp2);
    }  /* if */
  }  /* if */
  return result;
}  /* symbols_are_lookup_equivalent */


a_boolean already_in_lookup_set(a_symbol_ptr curr_sym,
                                a_symbol_ptr new_sym)
/*
See if new_sym is already in the lookup set represented by curr_sym.
curr_sym could point to a single namespace projection symbol or
an sk_overloaded_function symbol that points to a set of namespace
projections symbols.  curr_sym is the fundamental symbol to be compared
with the fundamental symbols pointed to by the namespace projection
symbol(s) in curr_sym.
*/
{
  a_boolean	result = FALSE;

  new_sym = fundamental_symbol_of(new_sym);
  if (curr_sym == NULL) {
    /* No current list -- return FALSE. */
  } else if (curr_sym->kind == (a_symbol_kind)sk_namespace_projection) {
    /* See if the current symbol is a projection symbol that points to
       new_sym or a symbol equivalent to new_sym. */
    a_symbol_ptr	fund_curr_sym = fundamental_symbol_of(curr_sym);
    result = new_sym == fund_curr_sym ||
             symbols_are_lookup_equivalent(new_sym, fund_curr_sym);
  } else if (curr_sym->kind == (a_symbol_kind)sk_overloaded_function) {
    /* Look through the overload set for a fundamental symbol that matches
       new_sym. */
    a_symbol_ptr	sym;
    for (sym = curr_sym->variant.overloaded_function.symbols;
         sym != NULL; sym = sym->next) {
      a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
      if (new_sym == fund_sym ||
          symbols_are_lookup_equivalent(new_sym, fund_sym)) break;
    }  /* for */
    if (sym != NULL) result = TRUE;
  } else {
    /* See if the current symbol is a routine symbol that is the same as
       new symbol.  This case is used when the first symbol found is
       a function or template symbol. */
    check_assertion(is_function_or_template_symbol(curr_sym));
    result = curr_sym == new_sym ||
             symbols_are_lookup_equivalent(curr_sym, new_sym);
  }  /* if */
  return result;
}  /* already_in_lookup_set */


static
a_symbol_ptr merge_function_into_lookup_set(
                               a_symbol_ptr		curr_sym,
                               a_symbol_ptr		new_sym,
                               a_symbol_locator		*locator,
                               a_boolean		qualified_lookup,
                               a_namespace_ptr		qualifier_namespace,
                               an_id_lookup_options_set	options)
/*
curr_sym is a pointer to the current lookup set, and may be NULL,
a pointer to a single namespace projection symbol, or an
sk_overloaded_function symbol that points to a number of namespace
projection symbols.  Add the function(s) pointed to by new_sym
creating a new overload set.
qualified_lookup is TRUE if for a namespace or file scope qualified
lookup.  For qualified lookups qualifier_namespace points to the
namespace in which the lookup is being done, or is NULL for a file
scope lookup.  options specifies the options being used for the lookup.
*/
{
  /* The set is currently empty.  If the initial member is a single
     routine, create a namespace projection that points to it.  If
     it is an overload set, make a new overload set containing
     namespace projections that point to its members. */
  if (new_sym->kind != (a_symbol_kind)sk_overloaded_function) {
    /* The new symbol is a simple function or is a function template.
       Make a namespace projection that points to it. */
    if (curr_sym == NULL) {
      curr_sym = enter_synthesized_projection_symbol(new_sym, locator,
                                                     qualified_lookup,
                                                     qualifier_namespace,
                                                     options);
    } else if (curr_sym->kind == (a_symbol_kind)sk_namespace_projection &&
               fundamental_symbol_of(curr_sym) == NULL) {
      /* The current lookup set is a namespace projection symbol whose
         fundamental symbol pointer has been cleared.  Simply set this
         symbol to point to the new symbol. */
      set_namespace_projection_symbol(curr_sym, new_sym, depth_scope_stack);
    } else {
      /* If new_sym is not already in the lookup set, add it. */
      if (!already_in_lookup_set(curr_sym, new_sym)) {
        new_sym = make_namespace_projection_symbol(new_sym,
                                                   &locator->source_position,
                                                   depth_scope_stack);
        curr_sym = add_symbol_to_overload_list(new_sym, curr_sym,
                                               qualified_lookup,
                                               qualifier_namespace);
      }  /* if */
    }  /* if */
  } else {
    /* The new symbol is an overload set. */
    a_symbol_ptr	rout_sym;
    a_symbol_ptr	new_rout_sym;
    rout_sym = new_sym->variant.overloaded_function.symbols;
    if (curr_sym == NULL) {
      /* If the current symbol is NULL, take the first member of the
         overload set and create a projection symbol to it.  Later
         we will add the remaining members and create a new overload set. */
      curr_sym = enter_synthesized_projection_symbol(rout_sym, locator,
                                                     qualified_lookup,
                                                     qualifier_namespace,
                                                     options);
      rout_sym = rout_sym->next;
    } else if (curr_sym->kind == (a_symbol_kind)sk_namespace_projection &&
               fundamental_symbol_of(curr_sym) == NULL) {
      /* The current lookup set is a namespace projection symbol whose
         fundamental symbol pointer has been cleared.  Simply set this
         symbol to point to the first member of the overload set. */
      set_namespace_projection_symbol(curr_sym, rout_sym, depth_scope_stack);
      rout_sym = rout_sym->next;
    }  /* if */
    for (; rout_sym != NULL; rout_sym = rout_sym->next) {
      /* If rout_sym is not already in the lookup set, add it. */
      if (!already_in_lookup_set(curr_sym, rout_sym)) {
        new_rout_sym =
                   make_namespace_projection_symbol(rout_sym,
                                                    &locator->source_position,
                                                    depth_scope_stack);
        curr_sym = add_symbol_to_overload_list(new_rout_sym, curr_sym,
                                               qualified_lookup,
                                               qualifier_namespace);
      }  /* if */
    }  /* for */
  }  /* if */
  return curr_sym;
}  /* merge_function_into_lookup_set */


static a_boolean check_for_microsoft_qualifier_using_directive_bug(
				a_symbol_ptr			*curr_sym,
				a_symbol_ptr			fund_curr_sym,
				a_symbol_ptr			new_sym,
				an_id_lookup_options_set	options)
/*
In certain cases, the Microsoft compiler prefers a class name made visible
by a using-directive to a typedef-to-class that is visible in a scope
where using-directives apply.  For example:

  template <class T> struct basic_ios { };
  typedef basic_ios<char> ios;
  namespace std { struct ios { const static int i;}; };
  using namespace std;
  int i = ios::i;

The Microsoft compiler uses std::ios instead of reporting an ambiguity.

This routine checks for this case.  curr_sym could be the typedef symbol
or the class symbol.  Likewise for new_sym.  "options" is the set of
lookup options.  fund_curr_sym is the fundamental symbol associated with
curr_sym.  The special processing is only done when a "must be class or
namespace" lookup is done.
*/
{
  a_boolean	result = FALSE;

  if ((options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0) {
    a_boolean	curr_is_typedef;
    a_boolean	new_is_typedef;
    curr_is_typedef = fund_curr_sym->kind == (a_symbol_kind)sk_type;
    new_is_typedef = new_sym->kind == (a_symbol_kind)sk_type;
    if (!curr_is_typedef && new_is_typedef) {
      /* Use the current symbol. */
      result = TRUE;
    } else if (!new_is_typedef && curr_is_typedef) {
      /* Use the new symbol. */
     result = TRUE;
     set_namespace_projection_symbol(*curr_sym, new_sym,
                                     depth_scope_stack);
    }  /* if */
  }  /* if */
  return result;
}  /* check_for_microsoft_qualifier_using_directive_bug */
				


static a_boolean symbols_from_same_scope(a_symbol_ptr curr_sym,
                                         a_symbol_ptr new_sym)
/*
Returns TRUE if the two symbols are from the same scope.  curr_sym
represents a lookup set that has been constructed and new_sym is
a normal symbol (not a synthesized projection symbol) that is being
considered as an alternative to curr_sym because of the 1.5 namespace
rule for struct names.  When curr_sym points to a set of overloaded
functions, the overload set must be inspected to see if all of the
members of the set are from the same scope.
*/
{
  a_boolean		result = TRUE;
  a_scope_number	curr_scope;

  /* Get the scope associated with curr_sym. */
  if (curr_sym->kind != (a_symbol_kind)sk_overloaded_function) {
    curr_scope = fundamental_symbol_of(curr_sym)->decl_scope;
  } else {
    /* curr_sym is an overload set, see if all of the members of the set have
       the same scope. */
    a_symbol_ptr	overload_sym;
    overload_sym = curr_sym->variant.overloaded_function.symbols;
    curr_scope = fundamental_symbol_of(overload_sym)->decl_scope;
    overload_sym = overload_sym->next;
    for (; overload_sym != NULL; overload_sym = overload_sym->next) {
      if (fundamental_symbol_of(overload_sym)->decl_scope != curr_scope) {
        /* A scope mismatch -- stop the search. */
        result = FALSE;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  /* If we produced a single scope above, compare it with the new symbol. */
  if (result) {
    result = curr_scope == fundamental_symbol_of(new_sym)->decl_scope;
  }  /* if */
  return result;
}  /* symbols_from_same_scope */


/* Forward declaration. */
static a_symbol_ptr add_symbol_to_lookup_set(
                               a_symbol_ptr		curr_sym,
                               a_symbol_ptr		new_sym,
                               a_symbol_locator		*locator,
                               a_boolean		qualified_lookup,
                               a_namespace_ptr		qualifier_namespace,
                               an_id_lookup_options_set	options,
                               a_boolean		*any_errors);


static a_boolean check_for_tag_hiding(
			a_symbol_ptr			*curr_sym,
			a_symbol_ptr			fund_curr_sym,
			a_symbol_ptr			new_sym,
			a_symbol_locator		*locator,
			a_namespace_ptr			qualifier_namespace,
			a_boolean			qualified_lookup,
			an_id_lookup_options_set	options,
                        a_boolean			*any_errors)
/*
This routine is used by add_symbol_to_lookup_set to detect the case
in which a tag symbol is hidden by a nontype symbol from the same
scope.

curr_sym and new_sym are the two symbols, either one of which could be
a tag or nontag.

In Sun compatibility mode, the symbols need not be from the same scope.
*/
{
  a_boolean	result = FALSE;

  if (sun_mode || symbols_from_same_scope(fund_curr_sym, new_sym)) {
    a_boolean	new_is_tag = is_tag_symbol(new_sym);
    a_boolean	curr_is_tag = is_tag_symbol(fund_curr_sym);
    if (new_is_tag != curr_is_tag) {
      /* Two symbols from the same scope and only one is a nontag.
         This is okay. */
      result = TRUE;
      if (curr_is_tag) {
        /* The current symbol is a tag and the new one is not. 
           Prefer the nontag (i.e., the new symbol).  Update the
           namespace projection symbol to point to the new symbol. */
        check_assertion_str2((*curr_sym)->kind ==
                                   (a_symbol_kind)sk_namespace_projection,
                             "check_for_tag_hiding:",
                             "expected a namespace projection symbol");
        /* Reset the namespace projection to NULL, then recall this routine
           to add the new symbol.  This is done to handle cases where
           new_sym points to an overload set. */
        (*curr_sym)->variant.namespace_projection.fundamental_symbol = NULL;
        *curr_sym = add_symbol_to_lookup_set(
                             *curr_sym, new_sym, locator, qualified_lookup,
                             qualifier_namespace, options, any_errors);
      } else {
        /* The current symbol is a nontag and the new one is a tag.
           Simply ignore the new one. */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* check_for_tag_hiding */

static
a_symbol_ptr add_symbol_to_lookup_set(
                               a_symbol_ptr		curr_sym,
                               a_symbol_ptr		new_sym,
                               a_symbol_locator		*locator,
                               a_boolean		qualified_lookup,
                               a_namespace_ptr		qualifier_namespace,
                               an_id_lookup_options_set	options,
                               a_boolean		*any_errors)
/*
Reconcile the results of a lookup in which more than one symbol is found
in scopes that are considered equivalent.  This occurs when using
directives cause symbols from multiple namespaces (possibly including
the global namespace) to be found as the result of a lookup.

When the first symbol is found, create an sk_namespace_projection
symbol that points to it.  If a second symbol is found, and both
the old and new symbols are functions, create an sk_overloaded_function
symbol that points to two sk_namespace_projection symbols.  Continue
adding new sk_namespace_projections as long as all of the symbols
found are functions.  If, at any point, there are both functions and
nonfunctions, or more than one nonfunction, set the any_errors flag.
qualified_lookup is TRUE for a namespace or file scope qualified
lookup.  For qualified lookups qualifier_namespace points to the
namespace in which the lookup is being done, or is NULL for a file
scope lookup.  options specifies the options being used for the lookup.
*/
{
  a_boolean	err = FALSE;

  /* Make sure the lookup set points to the fundamental symbol. */
  new_sym = fundamental_symbol_of(new_sym);
  if (curr_sym == NULL) {
    if (is_function_or_template_symbol(new_sym)) {
      curr_sym = merge_function_into_lookup_set((a_symbol_ptr)NULL,
                                                new_sym, locator,
                                                qualified_lookup,
                                                qualifier_namespace, options);
    } else {
      curr_sym = enter_synthesized_projection_symbol(new_sym, locator,
                                                     qualified_lookup,
                                                     qualifier_namespace,
                                                     options);
    }  /* if */
  } else if (curr_sym->kind == (a_symbol_kind)sk_namespace_projection &&
             namespace_projection_fundamental_symbol(curr_sym) == NULL) {
    /* curr_sym is a namespace projection symbol that doesn't point to any
       other symbol.  This is the case when a synthesized namespace
       projection symbol from a previous lookup is being used, and the
       previous symbol does not point to a function.  In this case the
       fundamental symbol pointer is cleared because it may have been
       the result of a constrained lookup (e.g., a tag lookup) which
       may not be applicable now.  Set the existing symbol to point to
       the new symbol. */
    if (is_function_or_template_symbol(new_sym)) {
      curr_sym = merge_function_into_lookup_set(curr_sym, new_sym, locator,
                                                qualified_lookup,
                                                qualifier_namespace, options);
    } else {
      set_namespace_projection_symbol(curr_sym, new_sym, depth_scope_stack);
    }  /* if */
  } else if (already_in_lookup_set(curr_sym, new_sym)) {
    /* The symbol is already present -- nothing more to do. */
  } else {
    a_symbol_ptr	fund_curr_sym;
    fund_curr_sym = fundamental_symbol_of(curr_sym);
    if (!is_function_or_template_symbol(new_sym) ||
        !is_function_or_template_symbol(fund_curr_sym)) {
      /* There is more than one symbol, and they are not all functions.
         This is an error unless the two symbols are from the same scope
         and one is a tag and the other a nontag.  Set the error flag.
         It will be cleared later if we determine that this case is okay. */
      err = TRUE;
      if (check_for_tag_hiding(&curr_sym, fund_curr_sym, new_sym, locator,
                               qualifier_namespace, qualified_lookup,
                               options, any_errors)) {
        /* curr_sym is set to the appropriate symbol by
           check_for_tag_hiding. */
        err = FALSE;
      } else if (is_type_symbol(new_sym) && is_type_symbol(fund_curr_sym) &&
                 identical_types(type_symbol_type(new_sym),
                                 type_symbol_type(fund_curr_sym))) {
        /* The two symbols refer to the same type.  Ignore the new one. */
        err = FALSE;
      } else if ((options & IDL_TENTATIVE_TYPE_LOOKUP) != 0 &&
                 is_type_symbol(new_sym) && !is_type_symbol(fund_curr_sym)) {
        /* We are doing a tentative type lookup and the new symbol is
           a type, but the old one is not.  This is an ambiguous case,
           but when possible, we want the symbol returned to point to
           the type symbol.  This improves error recovery in declaration
           contexts. */
       set_namespace_projection_symbol(curr_sym, new_sym,
                                       depth_scope_stack);
      } else if (curr_sym->kind == (a_symbol_kind)sk_namespace_projection &&
                 fund_curr_sym->kind == (a_symbol_kind)sk_undefined) {
        /* The current symbol is an sk_undefined symbol.  Use a "real" symbol
           if one is available, for better error recovery. */
        curr_sym->variant.namespace_projection.fundamental_symbol = NULL;
        curr_sym = add_symbol_to_lookup_set(curr_sym, new_sym, locator,
                                            qualified_lookup,
                                            qualifier_namespace, options,
                                            &err);
      } else {
        /* An ambiguous symbol. */
        if (microsoft_bugs) {
          /* Check for cases where the Microsoft compiler prefers a given
             symbol. */
          if (check_for_microsoft_qualifier_using_directive_bug(
                                 &curr_sym, fund_curr_sym, new_sym, options)) {
            /* curr_sym is set by the call above. */
            err = FALSE;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* Both symbols are functions. */
      curr_sym = merge_function_into_lookup_set(curr_sym, new_sym, locator,
                                                qualified_lookup,
                                                qualifier_namespace, options);
    }  /* if */
  }  /* if */
  if (err) {
    *any_errors = TRUE;
    curr_sym->ambiguous = TRUE;
  }  /* if */
#if EXPENSIVE_CHECKING
  {
    a_symbol_ptr	fund_curr_sym;
    fund_curr_sym = fundamental_symbol_of(curr_sym);
    check_assertion_str2(fund_curr_sym != NULL,
                         "add_symbol_to_lookup_set:", "NULL fund_sym");
    if (fund_curr_sym->kind == (a_symbol_kind)sk_overloaded_function) {
      a_symbol_ptr	overload_sym;
      overload_sym = fund_curr_sym->variant.overloaded_function.symbols;
      for (; overload_sym != NULL; overload_sym = overload_sym->next) {
        check_assertion_str2(fundamental_symbol_of(overload_sym) != NULL,
                             "add_symbol_to_lookup_set:", "NULL fund_sym");
      }  /* for */
    }  /* if */
  }
#endif /* EXPENSIVE_CHECKING */
#if DEBUG
  if (debug_level >= 4 || db_flag_is_set("lookup_set")) {
    db_symbol(curr_sym, "add_symbol_to_lookup_set:", 0);
    if (curr_sym->kind == (a_symbol_kind)sk_overloaded_function) {
      a_symbol_ptr	overload_sym;
      overload_sym = curr_sym->variant.overloaded_function.symbols;
      for (; overload_sym != NULL; overload_sym = overload_sym->next) {
        db_symbol(fundamental_symbol_of(overload_sym), "", 4);
      }  /* for */
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  return curr_sym;
}  /* add_symbol_to_lookup_set */


/*
Structure used to pass name lookup state information between routines
used to implement name lookup.
*/
typedef struct a_lookup_state *a_lookup_state_ptr;
typedef struct a_lookup_state {
  a_boolean	must_be_class_or_namespace;
			/* TRUE if the IDL_MUST_BE_CLASS_OR_NAMESPACE
			   option was specified for this lookup. */
  a_boolean	must_be_tag;
			/* TRUE if the IDL_MUST_BE_TAG option
			   was specified for this lookup. */
  a_boolean	must_be_namespace;
			/* TRUE if the IDL_MUST_BE_NAMESPACE option
			   was specified for this lookup. */
  a_boolean	must_be_class;
			/* TRUE if the IDL_MUST_BE_CLASS option
			   was specified for this lookup. */
  a_boolean	tentative_type_lookup;
			/* TRUE if the IDL_TENTATIVE_TYPE_LOOKUP option
			   was specified for this lookup. */
  a_boolean	tentative_template_lookup;
			/* TRUE if the IDL_TENTATIVE_TEMPLATE_LOOKUP option
			   was specified for this lookup. */
  a_boolean	is_linkage_lookup;
			/* TRUE if the IDL_LINKAGE_LOOKUP option
			   was specified for this lookup. */
  a_boolean	is_friend_lookup;
			/* TRUE if the IDL_FRIEND_LOOKUP option
			   was specified for this lookup. */
  a_boolean	hidden_name_lookup;
			/* TRUE if the IDL_HIDDEN_NAME_LOOKUP option was
			   specified for this lookup. */
  a_boolean	do_not_create_proj_sym;
			/* TRUE if the IDL_DO_NOT_CREATE_PROJ_SYM option was
			   specified for this lookup. */
  a_boolean	skip_template_decl_scopes;
			/* TRUE if the IDL_SKIP_TEMPLATE_DECL_SCOPES option
			   was specified for this lookup. */
  a_boolean	terminate_lookup;
			/* TRUE if a condition occurred that should cause
			   the lookup to terminate even is a symbol was
			   not found. */
  a_boolean	skip_curr_scope;
			/* TRUE if the IDL_SKIP_CURR_SCOPE was specified for
			   this lookup. If the current scope is a class
			   scope, any base classes are still be searched
			   even though the derived class is not. */
  a_boolean	skip_class_scopes;
			/* TRUE if the IDL_SKIP_CLASS_SCOPES
			   was specified for this lookup. */
  a_boolean	skip_first_class_reactivation;
			/* TRUE in cfront mode if we have found a scope
			   for a friend function definition and that
			   we should skip the next class reactivation
			   encountered to duplicate a cfront bug. */
  a_boolean	check_for_nonreal_bases;
			/* TRUE if we are within an instantiation scope
			   and we should check whether any of the base
			   classes involved in the lookup are nonreal. */
  a_boolean	any_nonreal_bases;
			/* TRUE when check_for_nonreal_bases is TRUE and
			   a nonreal base class has been found. */
  a_boolean	look_for_projected_symbol;
			/* TRUE for class and class reactivation scopes if
			   the lookup should attempt to find a projection
			   symbol that meets the lookup criteria. */
  a_boolean	look_in_dependent_bases;
			/* TRUE if the lookup can consider dependent base
			   classes generated from class templates. */
  a_boolean	add_to_active_list;
			/* TRUE if look_for_projected_symbol is TRUE and
			   the resulting projection symbol (if any) should be
			   added to the active list. */
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
  a_boolean	projection_symbol_found;
			/* TRUE if a projection symbol was found, but did
                           not meet the requirements of the lookup. */
  a_scope_depth	last_scope_used;
			/* Last scope used to find the symbol. */
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
  a_type_ptr	class_with_nonreal_base;
			/* When any_nonreal_bases is TRUE, this points to
			   the class type of the nonreal base class that
			   was found. */
  a_symbol_ptr	insert_sym;
			/* If look_for_projection_symbol and add_to_active_list
			   are TRUE, this symbol points to the location at
			   which the projection symbol should be added to the
			   active list. */
  an_id_lookup_options_set
		options;
			/* The lookup options passed into the lookup
			   routine. */
  a_name_space_kind
		required_name_space_kind;
			/* Indicates whether we are looking in the tag
			   name_space or the "other" namespace.  The tag
			   name_space is used only in C. */  
  a_decl_sequence_number
		decl_seq;
			/* The declaration sequence number of the effective
			   lookup position.  Symbols declared after this
			   position are ignored.  Zero if the declaration
			   sequence number should not be checked. */
   a_boolean	check_decl_seq;
			/* Flag that is TRUE if the decl_seq field should be
			   compared with the corresponding value for each
			   candidate symbol. */
} a_lookup_state;


/*
A lookup state that has been cleared that can be used to initialize
new lookup state variables.
*/
static a_lookup_state
		cleared_lookup_state;


static void init_cleared_lookup_state(void)
/*
Routine that initializes cleared_lookup_state to the correct initial
value.
*/
{
  cleared_lookup_state.must_be_class_or_namespace    = FALSE;
  cleared_lookup_state.must_be_tag                   = FALSE;
  cleared_lookup_state.must_be_namespace             = FALSE;
  cleared_lookup_state.must_be_class                 = FALSE;
  cleared_lookup_state.tentative_type_lookup         = FALSE;
  cleared_lookup_state.tentative_template_lookup     = FALSE;
  cleared_lookup_state.is_linkage_lookup             = FALSE;
  cleared_lookup_state.is_friend_lookup              = FALSE;
  cleared_lookup_state.hidden_name_lookup            = FALSE;
  cleared_lookup_state.do_not_create_proj_sym        = FALSE;
  cleared_lookup_state.skip_template_decl_scopes     = FALSE;
  cleared_lookup_state.terminate_lookup              = FALSE;
  cleared_lookup_state.skip_curr_scope               = FALSE;
  cleared_lookup_state.skip_class_scopes             = FALSE;
  cleared_lookup_state.skip_first_class_reactivation = FALSE;
  cleared_lookup_state.check_for_nonreal_bases       = FALSE;
  cleared_lookup_state.any_nonreal_bases             = FALSE;
  cleared_lookup_state.look_for_projected_symbol     = FALSE;
  cleared_lookup_state.look_in_dependent_bases       = FALSE;
  cleared_lookup_state.add_to_active_list            = FALSE;
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
  cleared_lookup_state.projection_symbol_found       = FALSE;
  cleared_lookup_state.last_scope_used               = NO_SCOPE_DEPTH;
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
  cleared_lookup_state.class_with_nonreal_base       = NULL;
  cleared_lookup_state.insert_sym                    = NULL;
  cleared_lookup_state.options                       = IDL_NO_OPTIONS;
  cleared_lookup_state.required_name_space_kind      = nsk_other;
  cleared_lookup_state.decl_seq                      = 0;
  cleared_lookup_state.check_decl_seq                = 0;
}  /* init_cleared_lookup_state */

/*
Macro that initializes a lookup state variable.
*/
#define clear_lookup_state(state) (state) = cleared_lookup_state;


/* Macro used by normal_id_lookup and do_using_directive_lookup that tests
   whether or not a symbol is acceptable. */
/* symbol_may_precede_qualifier checks for a symbol that is a class,
   class template, namespace, or template type parameter. */
#define is_acceptable_symbol(sym, fund_sym, lookup_state)               \
  (((!(fund_sym->is_invisible) && (!sym->is_invisible)) ||		\
    (lookup_state).is_linkage_lookup ||					\
    (lookup_state).is_friend_lookup) &&					\
   (!(lookup_state).must_be_class_or_namespace ||			\
    symbol_may_precede_qualifier(fund_sym)) &&                          \
   (!(lookup_state).must_be_tag   ||				        \
    is_tag_or_tag_proxy_symbol(fund_sym)) &&				\
   (!(lookup_state).must_be_class ||					\
    is_class_or_class_proxy_symbol(fund_sym)) && 			\
   (!(lookup_state).must_be_namespace ||				\
    is_namespace_symbol(fund_sym)) &&					\
   (!(lookup_state).check_decl_seq ||					\
    ((lookup_state).decl_seq == NO_DECL_SEQUENCE_NUMBER ||		\
     (lookup_state).decl_seq >= (sym)->decl_seq)))


a_boolean sym_matches_lookup_options(a_symbol_ptr		sym,
				     an_id_lookup_options_set	options)
/*
Return TRUE if the symbol specified by "sym" is acceptable according to
the lookup options specified by "options".  This routine is similar to the
is_acceptable_symbol macro, but is intended to be called from routines that
do not have the appropriate local variables (e.g., lookup_state) set that
are needed to use is_acceptable_symbol.
*/
{
  a_boolean	result;
  a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
 
  result = ((!((options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0) ||
             symbol_may_precede_qualifier(fund_sym)) &&
            (!((options & IDL_MUST_BE_TAG) != 0) ||
             is_tag_or_tag_proxy_symbol(fund_sym)) &&
            (!((options & IDL_MUST_BE_NAMESPACE) != 0) ||
             is_namespace_symbol(fund_sym)) &&
            (!((options & IDL_MUST_BE_CLASS) != 0) ||
             is_class_or_class_proxy_symbol(fund_sym)));
  return result;
}  /* sym_matches_lookup_options */


static a_symbol_ptr do_using_directive_lookup
                              (a_scope_stack_entry_ptr	ssep,
                               a_symbol_ptr		sym_from_scope,
                               a_symbol_locator		*locator,
                               a_lookup_state_ptr	lookup_state)
/*
Look for a symbol, as described by locator, that is in a namespace whose
scope_depth_at_which_using_directive_applies matches the scope depth of ssep.

sym_from_scope points to a symbol found in ssep by the normal_id_lookup,
and may be NULL.

If no additional symbols are found then the value of sym_from_scope
is returned to the caller.  If any additional symbols are found,
a pointer to a namespace projection symbol that reflects the results
of the lookup is returned to the caller.
*/
{
  a_symbol_ptr		synth_sym = NULL;
  a_symbol_ptr		new_sym;
  a_symbol_ptr		sym = sym_from_scope;

  db_enter(4, "do_using_directive_lookup");
  /* Look through the inactive symbols for any symbols associated with
     one of the marked namespaces.  Note that we keep looking even if
     an ambiguity is detected.  The symbol pointed to by the ambiguous
     synthesized projection symbol may differ depending on the
     lookup options. */
  for (new_sym = locator->symbol_header->inactive_symbols;
       new_sym != NULL; new_sym = new_sym->next) {
    a_namespace_ptr		nsp;
    a_symbol_ptr		ns_sym;
    a_symbol_ptr		fund_sym;
    a_scope_depth		ns_depth;
    a_boolean			any_errors = FALSE;
    /* Ignore symbols that are not namespace members. */
    if (new_sym->is_class_member) continue;
    nsp = new_sym->parent.namespace_ptr;
    if (nsp == NULL) continue;
    /* Ignore symbols that do not match the lookup requirements. */
    fund_sym = fundamental_symbol_of(new_sym);
    if (!is_acceptable_symbol(new_sym, fund_sym, *lookup_state)) continue;
    /* The namespace symbol supplement contains the scope depth at which
       symbols from a given namespace should be visible.  See if the scope
       depth for this namespace matches the scope pointed to by ssep. */
    nsp = skip_namespace_aliases(nsp);
    ns_sym = (a_symbol_ptr)nsp->source_corresp.assoc_info;
    ns_depth = ns_sym->variant.namespace_info.extra_info->
                                 scope_depth_at_which_using_directive_applies;
    if (&scope_stack[ns_depth] == ssep) {
      if (synth_sym == NULL) {
        /* Look for a previous synthesized namespace projection symbol
           for this scope. */
        synth_sym = find_synthesized_projection_symbol
                                            (locator, lookup_state->options,
                                             /*qualified_lookup=*/FALSE,
                                             (a_namespace_ptr)NULL);
        if (sym != NULL) {
          /* The lookup from this scope did find a symbol.  Put it in
             the lookup set. */
          synth_sym = add_symbol_to_lookup_set(synth_sym, sym,
                                               locator,
                                               /*qualified_lookup=*/FALSE,
                                               (a_namespace_ptr)NULL,
                                               lookup_state->options,
                                               &any_errors);
        }  /* if */
        sym = synth_sym;
      }  /* if */
      /* Merge the information about this symbol, with that
         of any previous symbol that was found. */
      sym = add_symbol_to_lookup_set(sym, new_sym,
                                     locator,
                                     /*qualified_lookup=*/FALSE,
                                     (a_namespace_ptr)NULL,
                                     lookup_state->options,
                                     &any_errors);
      /* Set synth_sym in case it was not set earlier.  This
         suppresses subsequent attempts to look up synth_sym. */
      synth_sym = sym;
    }  /* if */
  }  /* for */
  db_exit();
  return sym;
}  /* do_using_directive_lookup */


static
a_symbol_ptr active_scope_lookup(a_scope_kind			kind,
                                 a_scope_stack_entry_ptr	ssep,
				 a_symbol_locator		*locator,
                                 a_lookup_state_ptr		lookup_state)
/*
This routine is called as part of normal_id_lookup processing to handle
scopes for which the symbols are on the active list.

Note that for namespace extension and reactivation scopes, kind will have
been changed to sck_namespace if the symbols for the namespace are
still on the active list.  As a result, this routine will also be called
for namespace extension and reactivation scopes in such cases.
Note also that this routine will also be called for class reactivations
of classes whose class scope is still on the scope stack (i.e., for classes
that are still in the process of being defined.  ssep points to the
scope being for which symbols are being considered.  locator is the
symbol locator for the name being looked up.  lookup_state is used to
pass state information between the various routines that do normal id
lookup processing.
*/
{
  a_symbol_ptr		sym = NULL;
  a_symbol_ptr		active_sym;
  a_symbol_ptr		prev_active_sym = NULL;

/* Local macro that tests whether or not a symbol on the active list
   is acceptable.  See if the symbol is in the proper name space. */
#define is_acceptable_active_symbol(sym, fund_sym)                           \
  (name_space_for_symbol_kind[(int)sym->kind] ==			     \
                                   lookup_state->required_name_space_kind && \
   is_acceptable_symbol(sym, fund_sym, *lookup_state))

  if (lookup_state->skip_curr_scope) {
    /* Skip this scope.  Note that we still look for a projected symbol. */
  } else {
    prev_active_sym = NULL;
    active_sym = symbol_list_from_locator(*locator);
    /* Find the first symbol on the active list for this scope. */
    while (active_sym != NULL && active_sym->decl_scope != ssep->number) {
      prev_active_sym = active_sym;
      active_sym = active_sym->next;
    }  /* while */
    for (; active_sym != NULL && active_sym->decl_scope == ssep->number;
         prev_active_sym = active_sym, active_sym = active_sym->next) {
      a_symbol_ptr	fund_sym = fundamental_symbol_of(active_sym);
      if (is_acceptable_active_symbol(active_sym, fund_sym)) {
        /* Found a symbol.  Record whether this symbol was found
           at file scope.  If it was, we will later need to also
           check for symbols visible as a result of using
           directives. */
        sym = active_sym;
        break;
      }  /* if */
    }  /* for */
    /* If this is a namespace scope or the file scope, also look for
       any symbols that are visible because of using directives. */
    if ((kind == (a_scope_kind)sck_file ||
        kind == (a_scope_kind)sck_namespace) &&
        ssep->using_directives_apply &&
        !lookup_state->is_linkage_lookup &&
        !lookup_state->is_friend_lookup) {
      sym = do_using_directive_lookup(ssep, sym, locator, lookup_state);
    }  /* if */
  }  /* if */
  if (sym == NULL &&
      (kind == (a_scope_kind)sck_class_struct_union ||
       kind == (a_scope_kind)sck_class_reactivation) && !C_mode()) {
    /* For class scopes, look for a symbol projected (inherited)
       into the class scope if a symbol was not found in the class
       itself.  Don't look in dependent base classes for classes that
       are generated from templates. */
    lookup_state->look_for_projected_symbol = TRUE;
    lookup_state->look_in_dependent_bases =
                            !is_unspecialized_template_class(ssep->assoc_type);
    lookup_state->add_to_active_list = TRUE;
    lookup_state->insert_sym = prev_active_sym;
  }  /* if */
  return sym;
#undef is_acceptable_active_symbol
}  /* active_scope_lookup */


static
a_symbol_ptr inactive_scope_lookup(a_scope_kind			kind,
                                   a_scope_stack_entry_ptr	ssep,
				   a_symbol_locator		*locator,
                                   a_lookup_state_ptr		lookup_state)
/*
This routine is called as part of normal_id_lookup processing to handle
scopes for which the symbols are now on the inactive list.  This
includes class reactivations, namespace reactivations (except when the
original namespace is still on the scope stack), and template instantiation
scopes.

Note that for namespace extension and reactivation scopes, kind will have
been changed to sck_namespace if the symbols for the namespace are
still on the active list (this routine will not be called for such
scopes).

ssep points to the scope being for which symbols are being considered.
locator is the symbol locator for the name being looked up.
lookup_state is used to pass state information between the various routines
that do normal id lookup processing.
*/
{
  a_boolean	skip_scope = FALSE;
  a_symbol_ptr	sym = NULL;

  if (cfront_2_1_mode &&
      kind == (a_scope_kind)sck_class_reactivation &&
      lookup_state->skip_first_class_reactivation) {
     /* This is used to skip class reactivation scopes when
        processing friend declarations in cfront compatibility
        mode.  Cfront ignores the innermost class reactivation
        scope when processing friend functions. */
    lookup_state->skip_first_class_reactivation = FALSE;
    skip_scope = TRUE;
  }  /* if */
  if (kind == (a_scope_kind)sck_class_reactivation &&
      lookup_state->skip_class_scopes) {
    /* This is a class scope and we are skipping class scopes. */
    skip_scope = TRUE;
  }  /* if */
  if (!skip_scope) {
    if (ssep->reactivated_class_being_defined) {
      /* If the class that is being reactivated is still in the process of
         being defined, look on the active list for the symbols instead of
         looking on the inactive list as is usually the case. */
      sym = active_scope_lookup(kind, ssep, locator, lookup_state);
    } else {
      if (lookup_state->skip_curr_scope) {
        /* Skip this scope.  Note that we still look for a projected symbol. */
      } else {
        /* Look on the inactive list for a symbol from this reactivated
           scope. */
        a_symbol_ptr	tag_symbol = NULL;
        a_symbol_ptr	inactive_sym;
        sym = NULL;
        for (inactive_sym = inactive_symbol_list_from_locator(*locator);
             inactive_sym != NULL;
             inactive_sym = inactive_sym->next) {
          if (inactive_sym->decl_scope == ssep->number) {
            a_symbol_ptr	fund_sym = fundamental_symbol_of(inactive_sym);
            if (is_acceptable_symbol(inactive_sym, fund_sym, *lookup_state)) {
              /* Found a symbol. */
              /* If this is a template parameter symbol that should not
                 be visible then continue looking for another symbol. */
              if (inactive_sym->template_param_not_visible) continue;
                /* If the symbol is a tag symbol and we're not required to find
                   a tag symbol, there's the possibility that there is a
                   non-type symbol in the same scope later in the list (because
                   the inactive list is not ordered in any way).  Save the
                   tag symbol and keep looking.  If nothing else turns up,
                   use the tag symbol. */
              if (is_tag_symbol(fund_sym) && !lookup_state->must_be_tag) {
                tag_symbol = inactive_sym;
              } else {
                /* Take the symbol. */
                sym = inactive_sym;
                break;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* for */
        /* We reached the end of the list.  If there is a tag symbol saved
           within the loop, use it. */
        if (sym == NULL && tag_symbol != NULL) {
          sym = tag_symbol;
        }  /* if */
        /* If this is a namespace scope, also look for any symbols that
           are visible because of using directives. */
        if ((kind == (a_scope_kind)sck_namespace_extension ||
             kind == (a_scope_kind)sck_namespace_reactivation ||
             kind == (a_scope_kind)sck_file) &&
            ssep->using_directives_apply &&
            !lookup_state->is_linkage_lookup &&
            !lookup_state->is_friend_lookup) {
          sym = do_using_directive_lookup(ssep, sym, locator, lookup_state);
        }  /* if */
      }  /* if */
      if (sym == NULL && kind == (a_scope_kind)sck_class_reactivation) {
        /* There is no inactive symbol that is in this class. */
        /* Look for a symbol projected (inherited) into this class.
           Don't look in dependent base classes if the associated scope is
           a generated template class. */
        lookup_state->look_for_projected_symbol = TRUE;
        lookup_state->look_in_dependent_bases =
                            !is_unspecialized_template_class(ssep->assoc_type);
        lookup_state->add_to_active_list = FALSE;
        lookup_state->insert_sym = NULL;
      }  /* if */
      /* The Microsoft compiler ignores inherited injected class names
         in most cases.  The principal case in which it is found is at
         the start of a qualified name. */
      if (sym != NULL && microsoft_bugs &&
          !lookup_state->must_be_class_or_namespace &&
          is_injected_class_symbol(fundamental_symbol_of(sym))) sym = NULL;
    }  /* if */
  }  /* if */
  return sym;
}  /* inactive_scope_lookup */


static
a_symbol_ptr look_for_projected_symbol(a_scope_stack_entry_ptr	ssep,
				       a_symbol_locator		*locator,
				       a_lookup_state_ptr	lookup_state)
/*
This routine is called as part of the normal_id_lookup processing when
a class or class reactivation scope is encountered that does not
satisfy the lookup.  In such cases we need to see if a projection symbol
to a base class symbol could satisfy the lookup.

If a projection symbol is found that does not satisfy the lookup (e.g.,
it is not a type and we are doing a tentative type lookup), a NULL
symbol is returned and terminate_lookup is set to TRUE.  ssep points to
the scope stack entry for the class or class reactivation scope
being processed.  locator is the symbol locator for the name being looked up.
lookup_state is used to pass state information between the various routines
that do normal id lookup processing.
*/
{
  a_symbol_ptr	sym = NULL;

  check_assertion(!C_mode());
  if (find_projected_symbol(ssep->assoc_type, locator,
                            lookup_state->options,
                            lookup_state->look_in_dependent_bases,
                            lookup_state->tentative_type_lookup,
                            lookup_state->tentative_template_lookup,
                            lookup_state->hidden_name_lookup ||
                            lookup_state->do_not_create_proj_sym,
                            lookup_state->add_to_active_list,
                            lookup_state->insert_sym, &sym,
                            /*can_create_nonreal=*/FALSE)) {
    if (sym == NULL) {
      /* A symbol was found in a base class, but it was not returned
         (presumably because must_be_type_name was not satisfied).
         Don't continue looking. */
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
      lookup_state->projection_symbol_found = TRUE;
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
      lookup_state->terminate_lookup = TRUE;
    } else {
      /* A projection symbol was created (or the fundamental symbol was
         returned, in the case of a hidden name lookup).  It must still
         satisfy the constraints for this lookup. */
      a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
      if (!is_acceptable_symbol(sym, fund_sym, *lookup_state)) {
        sym = NULL;
      } else if (microsoft_bugs) {
        /* The Microsoft compiler ignores inherited injected class names
           in most cases.  The principal case in which it is found is at
           the start of a qualified name. */
        if (!lookup_state->must_be_class_or_namespace &&
            is_injected_class_symbol(fund_sym)) sym = NULL;
      }  /* if */
    }  /* if */
  } else {
    if (lookup_state->check_for_nonreal_bases &&
        !lookup_state->any_nonreal_bases) {
      /* No projection symbol was found.  If the class has any
         nonreal base classes record this information for possible
         later use. */
      lookup_state->class_with_nonreal_base = ssep->assoc_type;
      lookup_state->any_nonreal_bases =
                              symbol_supplement_for_class(ssep->assoc_type)->
                                                      any_nonreal_base_classes;
    }  /* if */
  }  /* if */
  return sym;
}  /* look_for_projected_symbol */


static a_symbol_ptr find_conversion_template_instance(
			a_symbol_locator		*locator,
                        a_type_ptr			class_type,
			a_symbol_list_entry_ptr		conversion_templates)
/*
locator is a symbol locator for a conversion function.  conversion_templates
is a list of conversion templates for the class in which the lookup
is being done.  class_type is the type in which the lookup is being done.
Go through the conversion template list and find any templates that
can supply an appropriate conversion function.  Return the symbol for
the matching function.  If more than one match is found, create an
ambiguous symbol and return a pointer.  If no match is found, return NULL.
*/
{
  a_symbol_list_entry_ptr	slep;
  a_type_ptr			result_type =
                                       locator->variant.conversion_result_type;
  a_symbol_ptr			result_sym = NULL;
  a_partial_order_candidate_ptr	candidate_list = NULL;

  /* Loop though each of the templates.  Stop if we determine that the
     lookup is ambiguous. */
  for (slep = conversion_templates; slep != NULL; slep = slep->next) {
    a_symbol_ptr			sym;
    a_symbol_ptr			fund_sym;
    a_template_symbol_supplement_ptr	tssp;
    a_template_arg_ptr			templ_arg_list = NULL;
    a_routine_ptr			rout_ptr;
    a_type_ptr	       			rout_type;
    a_type_ptr				return_type;
    a_template_param_ptr		param_list;
    sym = slep->symbol;
    fund_sym = fundamental_symbol_of(sym);
    tssp = template_supplement_for_symbol(fund_sym);
    rout_ptr = fund_sym->variant.template_info->variant.function.routine;
    rout_type = skip_typerefs(rout_ptr->type);
    return_type = return_type_of(rout_type);
    param_list = tssp->variant.function.decl_cache.decl_info->parameters;
#if DEBUG
    if (db_flag_is_set("conversion_lookup")) {
      fprintf(f_debug, "Looking for conversion template match with:\n");
      db_symbol(sym, "", 2);
    }  /* if */
#endif /* DEBUG */
    /* See if the type specified matches the return type of the conversion
       function. */
    if (matches_template_type(result_type, return_type, &templ_arg_list,
                              param_list, MTT_NO_FLAGS)) {
      /* Do the wrapup processing to make sure that all of the parameters
         have been deduced. */
      if (wrapup_function_template_argument_deduction(
               templ_arg_list, fund_sym, (a_template_param_ptr)NULL) != NULL) {
        /* We have a match.  Add the matching template to a list of matching
           candidates.  Any poorer matches will be removed by this process.
           The template argument list is saved along with the symbol. */
        add_to_partial_order_candidates_list(&candidate_list, sym,
                                             templ_arg_list);
        /* Set the argument list pointer to NULL so it won't be freed
           below. */
        templ_arg_list = NULL;
      }  /* if */
    }  /* if */
    /* Free the template argument list.  The pointer will have been
       set to NULL above if we need to save this list. */
    if (templ_arg_list != NULL) free_template_arg_list(templ_arg_list);
  }  /* for */
  if (candidate_list != NULL) {
    a_boolean		ambiguous = FALSE;
    a_symbol_ptr	matching_sym = NULL;
    a_template_arg_ptr	matching_arg_list = NULL;
    /* Select the best matching candidate and its template argument list. */
    select_best_partial_order_candidate(candidate_list, (a_symbol_ptr)NULL,
                                        &matching_sym, &matching_arg_list,
                                        &ambiguous);
    /* Find or create the template instance that matches the type needed.
       Note that the template argument list is freed in the called function. */
    result_sym = find_template_function(matching_sym, &matching_arg_list,
					(a_boolean)locator->is_template_id,
                                        &locator->source_position);
    if (ambiguous || matching_sym->ambiguous) {
      /* Create a copy of the result_sym and mark that copy as
         ambiguous. */
      a_symbol_ptr	new_sym;
      new_sym = alloc_symbol((a_symbol_kind)sk_member_function,
                             result_sym->header,
                             &locator->source_position);
      set_class_membership(new_sym, (a_source_correspondence*)NULL,
                           class_type);
      new_sym->ambiguous = TRUE;
      new_sym->variant.routine.ptr = result_sym->variant.routine.ptr;
      new_sym->variant.routine.instance_ptr =
                                     result_sym->variant.routine.instance_ptr;
      result_sym = new_sym;
    }  /* if */
  }  /* if */
  return result_sym;
}  /* find_conversion_template_instance */


static a_symbol_ptr create_unknown_conversion_symbol(
					a_symbol_locator	*locator,
					a_type_ptr		class_type)
/*
Create an unknown function symbol that is a member of class_type.  Use the
symbol header information from the locator.  Return the symbol created.
*/
{
  a_symbol_ptr		sym;
  a_type_ptr		conv_result;

  /* Put the result type of the conversion function into the constant
     created. */
  conv_result = locator->variant.conversion_result_type;
  check_assertion(conv_result != NULL);
  sym = create_unknown_function_symbol(locator->symbol_header,
                                       class_type, (a_namespace_ptr)NULL,
                                       (a_boolean)locator->is_qualified_name,
                                       conv_result);
  return sym;
}  /* create_unknown_conversion_symbol */


static a_symbol_ptr look_up_conversion_template_instance(
			a_symbol_locator		*locator,
                        a_type_ptr			class_type)
/*
Find the conversion template instance that matches the type specified
by the conversion type in the locator.  The conversion_template_list
of class_type can be NULL, in which case this routine is still called to
(possibly) do the template dependent context processing.

If either the class_type or the conversion type is template dependent,
return an unknown function symbol that has the result type recorded in
the ck_template_param constant.
*/
{
  a_symbol_ptr			result_sym = NULL;
  a_type_ptr			conv_result =
				      locator->variant.conversion_result_type;
  a_class_symbol_supplement_ptr	cssp;

  cssp = symbol_supplement_for_class(class_type);
  if (class_type->variant.class_struct_union.is_nonreal_class ||
      is_or_contains_template_param(conv_result)) {
    /* A template context where either the source object type or the
       result type is dependent.  Create an unknown function symbol to
       represent the conversion function. */
    result_sym = create_unknown_conversion_symbol(locator, class_type);
  } else if (cssp->conversion_template_list != NULL) {
    /* A normal (nondependent) context.  Try to find a matching
       template conversion instance. */
    result_sym = find_conversion_template_instance(
                          locator, class_type, cssp->conversion_template_list);
  }  /* if */
  return result_sym;
}  /* look_up_conversion_template_instance */


a_symbol_ptr look_up_conversion_function(a_type_ptr		parent_class,
					 a_type_ptr		conv_type,
					 a_source_position	*source_pos)
/*
Look in parent_class for a conversion function that converts to conv_type.
source_pos is the position to be used in the locator used for the lookup.
*/
{
  a_symbol_locator	locator;
  a_symbol_ptr		new_sym;

  /* Create a symbol locator for the conversion type. */
  make_type_conversion_locator(conv_type, &locator, source_pos);
  /* First look for a nontemplate conversion operator. */
  new_sym = class_qualified_id_lookup(&locator, parent_class,
                                      IDL_DO_NOT_ADD_TO_NONREAL_CLASS);
  if (new_sym == NULL) {
    /* Look for a conversion function template, and possibly create an
       unknown conversion function. */
    new_sym = look_up_conversion_template_instance(&locator, parent_class);
  }  /* if */
  return new_sym;
}  /* look_up_conversion_function */


/* Forward declaration. */
static
a_symbol_ptr instantiation_context_lookup(
				a_scope_stack_entry_ptr		ssep,
	                        a_symbol_locator	    	*locator,
                                a_lookup_state_ptr	    	lookup_state);


static
a_symbol_ptr scope_stack_lookup(a_symbol_locator    *locator,
                                a_lookup_state_ptr  lookup_state,
				a_scope_depth	    start_depth,
				a_scope_depth       end_depth)
/*
This routine is used by normal_id_lookup to look though a specified
set of scopes and return the result of a normal_id_lookup when only
those scopes are considered.  start_depth is the depth of the innermost
scope to be considered, end_depth is scope at which the lookup should
terminate.  end_depth is not included in the lookup.

locator is the symbol locator for the name being looked up.
lookup_state is used to pass state information between the various routines
that do normal id lookup processing.
*/
{
  a_symbol_ptr			sym = NULL;
  a_scope_depth			curr_depth;
  a_scope_stack_entry_ptr	ssep = NULL;
  a_boolean			curr_scope_skipped = FALSE;

  /* Work out from the innermost scope on the stack, and look at each
     scope.  If the scope is a class reactivation or a template
     instantiation, look on the inactive list for a symbol from that
     scope.  Otherwise, search part of the active list to look for an
     active symbol. */
#if DEBUG
  if (debug_level >= 5 || db_flag_is_set("scope_stack_lookup")) {
    fprintf(f_debug, "Scope stack lookup of %s, initial lookup scope=%d\n",
            locator->symbol_header->identifier, depth_of_initial_lookup_scope);
  }  /* if */
#endif /* DEBUG */
  /* Loop through the scope stack until we reach the scope indicated by
     end_depth. */
  for (curr_depth = start_depth; curr_depth > end_depth;
       curr_depth = ssep->previous_scope) {
    a_scope_kind		kind;
    ssep = &scope_stack[curr_depth];
    kind = ssep->kind;
#if DEBUG
    if (debug_level >= 5 || db_flag_is_set("scope_stack_lookup")) {
      fprintf(f_debug, "Doing lookup in ");
      db_scope_stack_entry(ssep);
    }  /* if */
#endif /* DEBUG */
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
    /* Record the depth of the scope in which the symbol is being sought. */
    lookup_state->last_scope_used = curr_depth;
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
    /* If IDL_SKIP_TEMPLATE_DECL_SCOPES is used, skip any template declaration
       scopes on the stack. */
    if (ssep->kind == (a_scope_kind)sck_template_declaration &&
        lookup_state->skip_template_decl_scopes) continue;
    /* The "skip_curr_scope" flag is used to skip the initial lookup scope
       when a hidden name lookup is done.  Reset this flag after the first
       iteration of the loop. */
    if (curr_scope_skipped) {
      lookup_state->skip_curr_scope = FALSE;
    } else {
      curr_scope_skipped = TRUE;
    }  /* if */
    /* Clear the flag that indicates whether a projected symbol should be
       sought. */
    lookup_state->look_for_projected_symbol = FALSE;
    /* Lookups outside of a template instantiations scope check the declaration
       sequence number of the symbol found.  This check should be suppressed
       for class scopes. */
    lookup_state->check_decl_seq = !lookup_state->is_linkage_lookup &&
                         !lookup_state->is_friend_lookup &&
                         ssep->kind != (a_scope_kind)sck_class_reactivation &&
                         ssep->kind != (a_scope_kind)sck_class_struct_union;
    if (kind == (a_scope_kind)sck_namespace_extension ||
        kind == (a_scope_kind)sck_namespace_reactivation) {
      /* If a namespace extension or reactivation scope is pushed while the
         original namespace is still on the scope stack, the symbols will
         be on the active list and not on the inactive lists.  For
         purposes of name lookup, set the scope kind to sck_namespace if
         the symbols are still on the active list. */
      a_scope_pointers_block_ptr	spbp;
      spbp = assoc_pointers_block_of(ssep);
      if (!spbp->add_symbols_to_inactive_list) {
        kind = (a_scope_kind)sck_namespace;
      }  /* if */
    }  /* if */
    if (kind == (a_scope_kind)sck_class_reactivation ||
        kind == (a_scope_kind)sck_namespace_extension ||
        kind == (a_scope_kind)sck_namespace_reactivation ||
        kind == (a_scope_kind)sck_template_instantiation ||
        (kind == (a_scope_kind)sck_file && ssep->is_reactivation)) {
      if (kind == (a_scope_kind)sck_class_reactivation &&
          lookup_state->is_linkage_lookup) {
        /* Skip class reactivation scopes for linkage lookups. */
      } else {
        sym = inactive_scope_lookup(kind, ssep, locator, lookup_state);
        if (sym == NULL && kind == (a_scope_kind)sck_template_instantiation) {
          /* For template instantiation scopes, also look on the active list if
             the symbol was not found on the inactive list.  This is done to
             find symbols that are entered into the instantiation scope during
             a prototype instantiation, such as symbols for friend classes
             and friend functions. */
          sym = active_scope_lookup(kind, ssep, locator, lookup_state);
        }  /* if */
      }  /* if */
    } else if (kind == (a_scope_kind)sck_pragma) {
      /* We have found a pragma scope -- ignore symbols in this scope. */
    } else if (kind == (a_scope_kind)sck_class_struct_union &&
               lookup_state->skip_class_scopes) {
      /* This is a class scope and we are skipping class scopes. */
    } else {
      /* Not a class reactivation or a template instantiation,
         i.e., normal scope.  Search through any symbols on the front
         of the active list that are from the associated scope, and see
         if any one is the symbol desired. */
      sym = active_scope_lookup(kind, ssep, locator, lookup_state);
    }  /* if */
    /* For class and class reactivation scopes, when the symbol is not
       found, see if there is a projection of some symbol into the
       scope. */
    if (lookup_state->look_for_projected_symbol) {
      sym = look_for_projected_symbol(ssep, locator, lookup_state);
      if (lookup_state->terminate_lookup) break;
      /* If we are looking for a conversion function and we still haven't
         found a symbol, look for a conversion template that can match
         the specified type. */
      if (sym == NULL && locator->is_conversion_name) {
        a_type_ptr			class_type = ssep->assoc_type;
        check_assertion(class_type != NULL);
        sym = look_up_conversion_template_instance(locator, class_type);
      }  /*if */
    }  /* if */
    if (sym != NULL) break;
    if (lookup_state->is_linkage_lookup) {
      /* When doing a linkage lookup, stop when we encounter the first
         namespace scope. */
      if (kind == (a_scope_kind)sck_namespace ||
          kind == (a_scope_kind)sck_namespace_extension) break;
    } else if (lookup_state->is_friend_lookup) {
      /* When doing a friend lookup, stop when we encounter the first
         namespace scope (except for tag names in Microsoft and Sun modes)
         or (except in cfront mode) the first function/block scope. */
      if (((kind == (a_scope_kind)sck_namespace ||
            kind == (a_scope_kind)sck_namespace_extension) &&
           !(lookup_state->must_be_tag && (sun_mode || microsoft_mode))) ||
          (!any_cfront_mode() && (kind == (a_scope_kind)sck_function ||
                                  kind == (a_scope_kind)sck_block))) {
        break;
      }  /* if */
    }  /* if */
    if (cfront_2_1_mode && kind == (a_scope_kind)sck_function) {
      /* In cfront compatibility mode friend functions defined within
         a class ignore the innermost class reactivation scope.
         If this is a friend function, set a flag that will cause
         the innermost class reactivation scope to be ignored.  This
         is a cfront 2.1 problem that appears to have been fixed in
         cfront 3.0. */
      if (!ssep->assoc_routine->source_corresp.is_class_member) {
        /* Not a member function. */
        lookup_state->skip_first_class_reactivation = TRUE;
      }  /* if */
    }  /* if */
    if (kind == (a_scope_kind)sck_template_instantiation &&
        !ssep->nested_instantiation) {
      /* We have reached a template instantiation scope and have not
         yet found the symbol we are looking for.  Do the special
         template lookup that considers symbols from both the
         defining and referencing context. */
      sym = instantiation_context_lookup(ssep, locator, lookup_state);
      break;
    }  /* if */
  }  /* for */
#if DEBUG
  if (debug_level >= 5 || db_flag_is_set("scope_stack_lookup")) {
    fprintf(f_debug, "Scope stack lookup returning ");
    db_symbol(sym, "", 0);
  }  /* if */
#endif /* DEBUG */
  return sym;
}  /* scope_stack_lookup */


static
a_symbol_ptr merge_instantiation_lookup_symbols(
					a_symbol_ptr		ref_sym,
					a_symbol_ptr		def_sym,
					a_symbol_locator	*locator,
					a_lookup_state_ptr	lookup_state)
/*
Given the symbols that resulted from the two instantiation context lookups,
create a synthesized projection symbol that represents the merged
information from the two symbols.  ref_sym and def_sym are the
symbols from the referencing and defining contexts, respectively.
But if the name is found in the context that is common between them,
it is arbitrarily assigned to one of them.

locator is the symbol locator for the name being looked up.
lookup_state is used to pass state information between the various routines
that do normal id lookup processing.
*/
{
  a_symbol_ptr			sym = NULL;
  an_id_lookup_options_set	options;
  a_boolean			any_errors = FALSE;

  /* Add a bit to the options set that indicates that the synthesized
     namespace projection symbol being created is for a template
     context lookup. */
  options = lookup_state->options | IDL_INSTANTIATION_CONTEXT;
  /* Look for an existing synthesized namespace projection symbol
     from a previous lookup that can be reused. */
  sym = find_synthesized_projection_symbol(locator, options,
                                          /*qualified_lookup=*/FALSE,
                                          (a_namespace_ptr)NULL);
  /* Add the first symbol to an existing lookup set.  Note that
     sym may be NULL at this point. */
  sym = add_symbol_to_lookup_set(sym, ref_sym, locator,
                                 /*qualified_lookup=*/FALSE,
                                 (a_namespace_ptr)NULL, options,
                                 &any_errors);
  /* Add the second symbol to the set.  This is done even if an error was
     returned from the previous lookup. */
  sym = add_symbol_to_lookup_set(sym, def_sym, locator,
                                 /*qualified_lookup=*/FALSE,
                                 (a_namespace_ptr)NULL, options,
                                 &any_errors);
  return sym;
}  /* merge_instantiation_lookup_symbols */


static
a_symbol_ptr instantiation_context_lookup(
				a_scope_stack_entry_ptr		ssep,
	                        a_symbol_locator	    	*locator,
                                a_lookup_state_ptr	    	lookup_state)
/*
When a normal lookup reaches passes reaches an instantiation scope and,
after considering the template parameters, still has not found a symbol,
the context of the instantiation must be considered.  The context
has two components: the definition context (where the template was
declared or defined) and the referencing context (the namespace containing
the reference that caused the instantiation).

The symbol may be found in one or both of the contexts.  If the symbol is
found in both contexts, both instances must be functions.

The two lookups are done by calling scope_stack_lookup with the
appropriate set of starting and ending scopes.  The ending scopes
specified will result in all scopes having been considered up to
a "common" scope at which the two lookup paths come back together.
The two lookups could result in zero, one, or two symbols being found.
If no symbols are found, the search resumes from the common scope.
If one symbols is found, the search also begins with the common
scope but any symbol found from the common search is used as the
"second" symbol (i.e., the one for which no symbol was found
in the defining/referencing searches).  If two symbols are found,
either from the defining/referencing search or as a result of
finding one in the defining/referencing search and one in the
common search, the to symbols are merged and result in either
an overload set or an ambiguous symbol.

ssep is a pointer to the scope stack entry for the template instantiation
scope.  locator is the symbol locator for the name being looked up.
lookup_state is used to pass state information between the various routines
that do normal id lookup processing.
*/
{
  a_scope_depth		common_depth = ssep->instantiation_common_depth;
  a_scope_depth		def_start = ssep->previous_scope;
  a_scope_depth		ref_start = ssep->instantiation_context_depth;
  a_symbol_ptr		def_sym;
  a_symbol_ptr		ref_sym = NULL;
  a_symbol_ptr		sym = NULL;
  a_boolean		do_not_look_in_common_scopes = FALSE;

#if DEBUG
  if (debug_level >= 5 || db_flag_is_set("instantiation_lookup")) {
    fprintf(f_debug,
            "doing instantiation lookup: def_start=%d, ref_start=%d, ",
            def_start, ref_start);
    fprintf(f_debug, "common=%d\n", common_depth);
  }  /* if */
#endif /* DEBUG */
  if (do_dependent_name_processing) {
    /* Only consider names visible at the point at which the template was
       defined. */
    lookup_state->decl_seq = get_effective_decl_seq();
  }  /* if */
  def_sym = scope_stack_lookup(locator, lookup_state, def_start, common_depth);
  if (def_sym != NULL && def_sym->is_class_member) {
    /* Do not look for a name in the referencing context if the definition
       context search found a class member. */
    do_not_look_in_common_scopes = TRUE;
  } else {
    if (do_dependent_name_processing || microsoft_mode) {
      /* The referencing context should be not considered in dependent lookup
         mode and Microsoft mode.  This means the common lookup must be
         suppressed if we've already found a symbol. */
      if (def_sym != NULL) do_not_look_in_common_scopes = TRUE;
    } else {
      /* Only do the referencing context lookup when not doing the
         standard-conforming dependent name processing.  When doing
         dependent name processing only names visible to argument-dependent
         lookup are visible from the instantiation context. */
      if (ref_start > common_depth) {
        /* Only look in the referencing namespace when it is different from
           the common depth. */
        ref_sym = scope_stack_lookup(locator, lookup_state, ref_start,
                                     common_depth);
      }  /* if */
    }  /* if */
  }  /* if */
  /* If either of these lookups failed to find a symbol, continue the
     lookup starting from the common scope.  When doing a friend lookup,
     only do the common scope lookup if the innermost namespace scope
     is part of the common lookup. */
  if ((ref_sym == NULL || def_sym == NULL) &&
      (!lookup_state->is_friend_lookup ||
       depth_innermost_namespace_scope <= common_depth) &&
       !do_not_look_in_common_scopes) {
    /* One of the lookups did not find a symbol.  Do the lookup of
       the common scopes. */
    a_symbol_ptr	common_sym;
    common_sym = scope_stack_lookup(locator, lookup_state, common_depth,
                                    NO_SCOPE_DEPTH);
    /* Assign the common symbol to whichever of the previous lookups that
       did not produce a symbol.  If both were NULL, arbitrarily use the
       common symbol as the defining context symbol. */
    if (def_sym == NULL) {
      def_sym = common_sym;
    } else {
      ref_sym = common_sym;
    }  /* if */
  }  /* if */
  if (ref_sym != NULL) {
    a_symbol_ptr	fund_ref_sym;
    fund_ref_sym = fundamental_symbol_of(ref_sym);
    if (!is_function_or_template_symbol(fund_ref_sym)) {
      /* Only functions from the referencing context are used.  All other
         names can only come from the definition context.  The WP
         requires only "dependent" functions from the referencing context
         be considered.  The "dependent" lookup portion has not been
         implemented yet. */
      ref_sym = NULL;
    }  /* if */
  }  /* if */
  if (ref_sym != NULL && def_sym != NULL) {
    /* Both symbols are present.  Merge the results. */
    sym = merge_instantiation_lookup_symbols(ref_sym, def_sym, locator,
                                             lookup_state);
  } else if (ref_sym != NULL) {
    /* Only ref_sym is non-NULL. Return that value. */
    sym = ref_sym;
  } else if (def_sym != NULL) {
    /* Only def_sym is non-NULL. Return that value. */
    sym = def_sym;
  }  /* if */
#if DEBUG
  if (debug_level >= 5 || db_flag_is_set("instantiation_lookup")) {
    fprintf(f_debug, "instantiation lookup: ");
    if (sym == NULL) {
      fprintf(f_debug, "<NULL>\n");
    } else {
      db_symbol(sym, "", 4);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  return sym;
}  /* instantiation_context_lookup */


a_symbol_ptr normal_id_lookup(a_symbol_locator         *locator,
                              an_id_lookup_options_set options)
/*
Look up the identifier indicated by *locator and return a pointer to
the symbol found, or NULL if the symbol is not found.  options contains
bits indicating special restrictions, i.e., the symbol must be a class
name or tag.  The symbol is looked up in the normal name space (variables,
functions, classes, types, etc.).  If the symbol found is a projection
symbol, the projection symbol pointer is recorded in the locator and the
fundamental symbol pointer is returned.  This routine is used in both
C and C++.
*/
{
  a_symbol_ptr            sym, inactive_symbol_list;
  a_symbol_ptr            active_symbol_list;
  a_scope_stack_entry_ptr ssep;
  a_boolean		  use_slow_lookup = FALSE;

/* Local macro that tests whether or not a symbol on the active list
   is acceptable.  See if the symbol is in the proper name space. */
#define is_acceptable_active_symbol(sym, fund_sym)                           \
  (name_space_for_symbol_kind[(int)sym->kind] ==			     \
                                    lookup_state.required_name_space_kind && \
   is_acceptable_symbol(sym, fund_sym, lookup_state))

  db_enter(4, "normal_id_lookup");

  sym = locator->specific_symbol;
  if (sym != NULL) {
    /* The locator is for a specific symbol, so return the symbol for it. */
  } else if (is_error_locator(*locator)) {
    /* The locator is an error locator, so return NULL (i.e., no symbol
       found). */
    sym = NULL;
  } else {
    /* Initialize the lookup state information used to pass information
       about this lookup between the various routines used to do the
       lookup. */
    a_lookup_state	lookup_state;
    clear_lookup_state(lookup_state);
    lookup_state.must_be_class_or_namespace =
                               (options & IDL_MUST_BE_CLASS_OR_NAMESPACE) != 0;
    lookup_state.must_be_tag = (options & IDL_MUST_BE_TAG) != 0;
    lookup_state.must_be_namespace = (options & IDL_MUST_BE_NAMESPACE) != 0;
    lookup_state.must_be_class = (options & IDL_MUST_BE_CLASS) != 0;
    lookup_state.tentative_type_lookup =
                                    (options & IDL_TENTATIVE_TYPE_LOOKUP) != 0;
    lookup_state.tentative_template_lookup =
                                (options & IDL_TENTATIVE_TEMPLATE_LOOKUP) != 0;
    lookup_state.is_linkage_lookup = (options & IDL_LINKAGE_LOOKUP) != 0;
    lookup_state.is_friend_lookup = (options & IDL_FRIEND_LOOKUP) != 0;
    lookup_state.hidden_name_lookup = (options & IDL_HIDDEN_NAME_LOOKUP) != 0;
    lookup_state.do_not_create_proj_sym =
                                   (options & IDL_DO_NOT_CREATE_PROJ_SYM) != 0;
    lookup_state.skip_template_decl_scopes =
                                (options & IDL_SKIP_TEMPLATE_DECL_SCOPES) != 0;
    lookup_state.skip_curr_scope = (options & IDL_SKIP_CURR_SCOPE) != 0;
    lookup_state.skip_class_scopes = (options & IDL_SKIP_CLASS_SCOPES) != 0;
    /* If any instantiation scopes are active we will need to check for
       the presence of nonreal base classes.  This is done to "pretend"
       that certain names were found in a nonreal base class.  Don't
       do this special processing when doing standard dependent name
       lookup. */
    lookup_state.check_for_nonreal_bases =
                         !do_dependent_name_processing &&
                         !lookup_state.is_friend_lookup &&
			 !lookup_state.is_linkage_lookup &&
                         depth_innermost_instantiation_scope != NO_SCOPE_DEPTH;
    if (C_mode() && lookup_state.must_be_tag) {
      lookup_state.required_name_space_kind = nsk_tag;
    }  /* if */
    lookup_state.options = options;
    /* We must search for the symbol. */
    /* We have two search algorithms: the first is the C algorithm, which
       is fast; it just searches the active list.  The second is the C++
       algorithm, which is slower; it considers each scope on the scope
       stack in turn, and looks for a symbol in that scope. */
    active_symbol_list = symbol_list_from_locator(*locator);
    inactive_symbol_list = inactive_symbol_list_from_locator(*locator);
    /* The slow lookup mechanism is used:

	- in C mode if a pragma scope is active
	- if a template instantiation scope is active
	- if symbols from the inactive list may be visible, which is true
	  if a template instantiation, class reactivation, namespace
	  reactivation, namespace extension, or class scope for a class
	  with base classes is active.  It is also true for scopes containing
          using-directives and for the reactivation of the file scope.
    */
    ssep = &scope_stack[depth_scope_stack];
    if (ssep->slow_lookup_required) {
      /* Certain scopes (e.g., pragma and template instantiation) require
         slow lookups. */    
      use_slow_lookup = TRUE;
    } else if (!C_mode() &&
               (lookup_state.skip_curr_scope ||
                lookup_state.skip_class_scopes ||
                lookup_state.is_linkage_lookup ||
                lookup_state.is_friend_lookup)) {
      /* Certain lookup kinds require a slow lookup in C++ mode. */
      use_slow_lookup = TRUE;
    } else if ((ssep->inactive_symbols_may_be_visible &&
                inactive_symbol_list != NULL) ||
                locator->is_conversion_name) {
      /* A slow lookup is required if we need to search the inactive list
         or if we need to look up a conversion name. */
      use_slow_lookup = TRUE;
    }  /* if */
    if (!use_slow_lookup) {
      /* Fast algorithm: just search the active symbol list. */
#if DEBUG
      num_fast_id_lookups++;
#endif /* DEBUG */
      for (sym = active_symbol_list; sym != NULL; sym = sym->next) {
        /* See if the symbol is acceptable (e.g., it's a class if it
           must be one). */
        a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
        if (is_acceptable_active_symbol(sym, fund_sym)) break;
      }  /* for */
    } else {
      /* There are inactive symbols and they may be visible, so the more
         complicated search is required. */
#if DEBUG
      num_slow_id_lookups++;
#endif /* DEBUG */
      /* In C mode, slow lookups should only occur when the file scope has
         been reactivated. */
      check_assertion(!C_mode() ||
                      scope_stack[DEPTH_OF_FILE_SCOPE].is_reactivation);
      sym = scope_stack_lookup(locator, &lookup_state,
                               depth_of_initial_lookup_scope, NO_SCOPE_DEPTH);
    }  /* if */
    /* If this is a linkage lookup, don't do the nested class anachronism
       lookup, or SVR4 mode lookup. */
    if (!lookup_state.is_linkage_lookup && !lookup_state.terminate_lookup) {
      if (!C_mode()) {
        if (sym == NULL) {
          /* See if the nested class anachronism (ARM 18.3.5) yields a symbol.
             Note that if there is an ambiguity, NULL is returned.  Note
             also that we look for a semivisible nested class only if no
             other symbol is found.  This means a nested class that is
             semivisible at function scope will not hide a name at file
             scope; this is different from how cfront 2.1 works, but it
             means that programs that are legal by the ARM do not fail to
             compile or otherwise behave differently because the anachronism
             was invoked. */
          if (allow_anachronisms) sym = find_nested_type_symbol(locator);
          if (sym != NULL) {
            a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
            if (is_acceptable_symbol(sym, fund_sym, lookup_state)) {
              locator->is_semivisible_nested_type = TRUE;
            } else {
              sym = NULL;
            }  /* if */
          }  /* if */
        }  /* if */
        if (sym == NULL && lookup_state.any_nonreal_bases) {
          /* If no symbol was found and one of the classes searched has
             a nonreal base class then consider the symbol to be a member
             of the class with the nonreal base class.  This will occur when
             a base class depends on a template parameter (such as A<T>)
             or when the base class is a template parameter (such as T).
             In these cases it is impossible to know, at the time that
             prototype instantiation is done, which names will be in the
             classes used in the real instantiations.  Any name is accepted
             as a member of the class.  When class_qualified_id_lookup is
             used to lookup a name in a class with nonreal base classes,
             it will add a projection symbol to one of the nonreal bases
             if the name is not found.  In other words, this call is used
             to create the nonreal member.  any_nonreal_bases will only
             be set if check_for_nonreal_bases was set earlier. */
          sym = class_qualified_id_lookup(
                                 locator, lookup_state.class_with_nonreal_base,
                                 options | IDL_DO_NOT_CREATE_PROJ_SYM);
        }  /* if */
      }  /* if */
      if (sym == NULL && C_dialect == C_dialect_ANSI && !strict_ansi_mode &&
          (options & IDL_TENTATIVE_TYPE_LOOKUP) == 0) {
        /* This is a feature taken from SVR4 compatibility mode that has been
           expanded to be used in default ANSI C mode. A symbol declared as
           a block extern in a block that is no longer in scope may be
           referenced later.  Look for an external variable or routine that
           matches the name being looked up.  This is not done during
           tentative type lookups.  The "is_acceptable_symbol" test is done
           by find_out_of_scope_declaration. */
        sym = find_out_of_scope_declaration(locator, options);
      }  /* if */
    }  /* if */
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
    if (cfront_2_1_mode &&
        (sym != NULL || lookup_state.projection_symbol_found) &&
        lookup_state.last_scope_used != NO_SCOPE_DEPTH) {
      a_scope_stack_entry_ptr	proj_ssep;
      /* Special case to emulate a cfront 2.1 bug.  See the comments
         in check_for_cfront_name_lookup_bug for more information.  The
         special case code is only executed when the symbol found is from
	 a class reactivation scope for the same class of a constructor
	 or destructor that was just defined -- so this should not have
	 a significant performance impact.

         We have either found a symbol (sym != NULL) or a projection symbol
         was found that was not a type symbol in a tentative type lookup
         (projection_symbol_found == TRUE).  Call the special routine to
         see if a file scope name exists that satisfies the required
         criteria. */
      proj_ssep = &scope_stack[lookup_state.last_scope_used];
      if (proj_ssep->kind == (a_scope_kind)sck_class_reactivation) {
        a_type_ptr      class_type = proj_ssep->assoc_type;
	if (last_ctor_or_dtor_sym != NULL &&
            last_ctor_or_dtor_sym->parent.class_type == class_type) {
	  sym = check_for_cfront_name_lookup_bug(class_type, sym, locator,
						 options);
          /* This looks like we can find an alternate symbol when emulating
	     the cfront bug and then discard it because it is not the
	     correct kind of symbol.  In practice this will never happen
	     because the only symbols that can be rejected are types
	     (either classes or tags) and because of the transitional model
	     of nested type handling, will always be defined before a nested
	     class of the same name can be used. */
          if (sym != NULL) {
            a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
            if (!is_acceptable_symbol(sym, fund_sym, lookup_state)) {
              sym = NULL;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
    locator->specific_symbol = sym;
  }  /* if */
  /* If the symbol is a projection symbol, reduce it to the fundamental
     symbol.  The specific_symbol in the locator stays pointing to the
     projection symbol. */
  if (sym != NULL) reduce_projection_symbol_to_fundamental_symbol(sym);
#if DEBUG
  if (debug_level >= 4) {
    if (sym != NULL) {
      fprintf(f_debug, "normal_id_lookup: found %s\n",
                       sym->header->identifier);
      if (debug_level >= 5) db_symbol(sym, "", 2);
    } else {
      fprintf(f_debug, "normal_id_lookup: not found\n");
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return sym;
#undef is_acceptable_active_symbol
}  /* normal_id_lookup */

/* Undefine the macro used by normal_id_lookup and do_using_directive_lookup
   to determine whether a symbol is acceptable. */
#undef is_acceptable_symbol

a_symbol_ptr curr_tag_symbol(a_symbol_locator  *locator,
                             a_symbol_kind     tag_kind,
                             a_boolean         is_friend_decl)
/*
The current token is an identifier.  If it is a tag of the indicated kind
do ambiguity and access control checking and return a pointer to the tag
symbol.  Otherwise, return NULL.  is_friend_decl is TRUE when the tag appears
in a friend declaration.
*/
{
  a_symbol_ptr              assoc_symbol;
  an_id_lookup_options_set  options = IDL_MUST_BE_TAG;

  /* Look up the current token.  Note that a qualified name is not allowed. */
  if (is_friend_decl) options |= IDL_FRIEND_LOOKUP;
  assoc_symbol = normal_id_lookup(locator, options);
  if (assoc_symbol != NULL) {
    if (assoc_symbol->kind == (a_symbol_kind)sk_class_template &&
        depth_innermost_instantiation_scope != NO_SCOPE_DEPTH) {
      /* If the symbol found is a class template symbol and we are inside an
         instantiation of the class, use the template class symbol associated
         with the current instantiation. */
      (void)current_class_symbol_if_class_template(&assoc_symbol);
    } else if (is_friend_decl && locator->specific_symbol != NULL &&
               locator->specific_symbol->kind ==
                                     (a_symbol_kind)sk_namespace_projection) {
      /* Friend lookups should not find using declarations. */
      assoc_symbol = NULL;
      clear_specific_symbol(*locator);
    }  /* if */
  }  /* if */
  /* If the symbol still refers to the class template, ignore it. */
  if (assoc_symbol != NULL &&
      assoc_symbol->kind == (a_symbol_kind)sk_class_template) {
    assoc_symbol = NULL;
    clear_specific_symbol(*locator);
  }  /* if */
  if (assoc_symbol == NULL && !is_friend_decl) {
    /* If the symbol was not found using a normal lookup above, look again
       using a linkage lookup which will return an invisible symbol. */
    assoc_symbol = normal_id_lookup(locator, options | IDL_LINKAGE_LOOKUP);
    /* If the new symbol refers to the class template, ignore it. */
    if (assoc_symbol != NULL &&
        assoc_symbol->kind == (a_symbol_kind)sk_class_template) {
      assoc_symbol = NULL;
      clear_specific_symbol(*locator);
    }  /* if */
  }  /* if */
  if (assoc_symbol != NULL) {
    /* Make sure that the lookup was not ambiguous. */
    check_for_ambiguity(locator);
    if (assoc_symbol->is_template_param) {
      a_type_ptr   	tp;
      a_symbol_ptr	new_sym;
      an_error_severity	severity;
      /* We are within a template instantiation, so the name may map to a
         template parameter.  For example,
            class A { };
            template <class T> class B { class T x; };
            B<A> b;
         The symbol is for a type template parameter (nontypes will not be
         found by an IDL_MUST_BE_TAG lookup).  During prototype instantiation
         the template parameter symbol is simply returned to the caller.
         During a real instantiation the symbol associated with the type
         pointed to by the template parameter is returned. */
      check_assertion_str(assoc_symbol->kind == (a_symbol_kind)sk_type,
                          "curr_tag_symbol: bad symbol kind");
      tp = assoc_symbol->variant.type.ptr;
      /* If the type is a typedef, use the underlying type. */
      tp = skip_typedefs(tp);
      new_sym = (a_symbol_ptr)tp->source_corresp.assoc_info;
      /* Issue a diagnostic because this usage is no longer permitted by
         the Working Paper. */
      severity = strict_ansi_error_severity;
      pos_st_diagnostic(severity,
                        ec_template_param_in_elab_type, 
                        &error_position, assoc_symbol->header->identifier);
      if (severity == es_error) {
        /* If an error was issued above.  Return an error locator. */
        set_to_error_locator(*locator);
        assoc_symbol = NULL;
      } else if (tp->kind == (a_type_kind)tk_template_param) {
        /* A template parameter encountered during prototype instantiation.
           Simply return the original symbol. */
      } else if (new_sym != NULL && new_sym->kind == tag_kind) {
        /* Use the template argument to which the template parameter points. */
        assoc_symbol = new_sym;
      } else {
        /* The template argument is the wrong kind of tag. */
        pos_stty_error(ec_tag_kind_incompatible_with_template_parameter,
                       &error_position,
                       name_of_symbol_kind(tag_kind), tp);
        set_to_error_locator(*locator);
        assoc_symbol = NULL;
      }  /* if */
    }  /* if */
    if (assoc_symbol == NULL) {
      /* A NULL symbol resulted from an error above. */
    } else if (assoc_symbol->kind == (a_symbol_kind)sk_type) {
      /* This must be a symbol for a template parameter, and we must be in
         the midst of a prototype instantiation.  Return the symbol that
         was found. */
    } else if (assoc_symbol->kind != tag_kind &&
               assoc_symbol->decl_scope !=
                        scope_stack[decl_scope_level].number) {
      /* A tag, but it's from another scope and it's the wrong kind of tag
         (e.g., struct when union is required). */
      assoc_symbol = NULL;
    } else {
      if (locator->is_semivisible_nested_type) {
        /* The symbol in the locator is a nested class that is not visible
           according to the ARM lookup rules but is returned in support of the
           nested class anachronism (ARM 18.3.5).  Issue an anachronism
           diagnostic. */
        sym_diagnostic(anachronism_error_severity, ec_nested_class_anachronism,
                       locator->specific_symbol);
      }  /* if */
      /* Do access control checking on the member.  Ambiguity has already
         been checked above. */
      check_ambiguity_and_verify_access(locator);
    }  /* if */
  }  /* if */
  return assoc_symbol;
}  /* curr_tag_symbol */


static void determine_projected_symbol_insert_location(
                                          a_symbol_locator *locator,
                                          a_type_ptr       class_type,
                                          a_boolean        *add_to_active_list,
                                          a_symbol_ptr     *insert_sym)
/*
Determine the insert location (add_to_active_list and insert_sym) required
by find_projected_symbol to insert a projection symbol for the locator
*locator into the class indicated by class_type.
*/
{
  a_symbol_ptr            prev_active_sym, active_sym;
  a_scope_stack_entry_ptr ssep;

  /* Find out whether or not the class is active, and if so, where in
     the active list its entries begin. */
  *add_to_active_list = FALSE;
  *insert_sym = NULL;
  prev_active_sym = NULL;
  active_sym = symbol_list_from_locator(*locator);
  for (ssep = &scope_stack[depth_scope_stack];
       ssep != &scope_stack[DEPTH_OF_FILE_SCOPE];
       ssep--) {
    /* If we've found the class, exit the loop. */
    if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
        ssep->assoc_type == class_type) {
      *add_to_active_list = TRUE;
      *insert_sym = prev_active_sym;
      break;
    }  /* if */
    /* If the stack entry may have associated entries on the active list,
       move past them. */
    if (ssep->kind != (a_scope_kind)sck_class_reactivation) {
      for (;active_sym != NULL && active_sym->decl_scope == ssep->number;
           prev_active_sym = active_sym, active_sym = active_sym->next) {
      }  /* for */
    }  /* if */
  }  /* for */
}  /* determine_projected_symbol_insert_location */


a_symbol_ptr class_qualified_id_lookup(a_symbol_locator         *locator,
                                       a_type_ptr               class_type,
                                       an_id_lookup_options_set options)
/*
Look up the identifier indicated by *locator in the class indicated by
class_type, and return a pointer to the symbol found, or NULL if
the symbol is not found.  options indicates a set of special options,
as a bit set.  For example, if IDL_MUST_BE_CLASS_OR_NAMESPACE is TRUE,
the symbol found must be a class name (or typedef to a class name) or a
namespace.  If the symbol found is a projection symbol, the
projection symbol pointer is recorded in the locator and the fundamental
symbol pointer is returned.  This routine is used in both C and C++ mode.
*/
{
  a_symbol_ptr sym, tag_symbol, class_symbol;
  a_boolean    must_be_class_or_namespace
                                 = (options & IDL_MUST_BE_CLASS_OR_NAMESPACE);
  a_boolean    must_be_tag = (options & IDL_MUST_BE_TAG);
  a_boolean    must_be_class = (options & IDL_MUST_BE_CLASS);
  a_class_symbol_supplement_ptr
               cssp;
  a_symbol_ptr insert_sym;
  a_boolean    add_to_active_list;
  a_boolean    is_proxy_or_nonreal_class_lookup = FALSE;
  a_boolean    any_nonreal_base_classes = FALSE;
  a_boolean    direct_class_members_only =
                                (options & IDL_DIRECT_CLASS_MEMBERS_ONLY) != 0;
  a_boolean    dependent_conversion_operator = FALSE;

/* Local macro that tests whether or not a symbol is acceptable.  An
   injected class name symbol is only acceptable when the injected symbol
   does not points to the class in which the lookup is being done;
   otherwise, that symbol is rejected and (typically) the constructor
   symbol will be returned later. */
#define is_acceptable_symbol(sym, fund_sym)                           \
  ((sym)->is_class_member &&					      \
   (!is_injected_class_symbol(sym) ||				      \
    /* direct_class_members_only ||*/				      \
    class_type != (fund_sym)->variant.type.ptr) &&		      \
   (sym)->parent.class_type == class_type &&                          \
   (!must_be_class_or_namespace ||				      \
    symbol_may_precede_qualifier(fund_sym)) &&	     		      \
   (!must_be_class ||						      \
    is_class_or_class_proxy_symbol(fund_sym)) &&     		      \
   (!must_be_tag || is_tag_or_tag_proxy_symbol(fund_sym)))

  db_enter(4, "class_qualified_id_lookup");
  /* Remove any typedef on the class type. */
  class_type = skip_typerefs(class_type);
  if (class_type->kind == (a_type_kind)tk_template_param) {
    /* We are looking up a name in a template parameter that is being used
       as a class (e.g., T::X, where T is a template parameter).  Each
       template parameter that is used as a class has a "proxy class"
       created for it that contains a list of member names that have
       been looked up in the class.  Any name that is looked up in the
       proxy class will be found -- if it doesn't already exist, a symbol
       entry will be created for it. */
    /* Use the proxy class in place of the template parameter type. */
    class_type = proxy_class_for_template_param(class_type);
    is_proxy_or_nonreal_class_lookup = TRUE;
  } else {
    /* Determine whether we are looking up a name in a nonreal class
       that is not the prototype instantiation.  Nonreal lookups are
       handled like proxy class lookups; any name looked up is found.
       If the symbol does not exist one will be created.  The
       assoc_scope check is used to exclude the prototype
       instantiation from being considered nonreal for lookup
       purposes. */
    cssp = symbol_supplement_for_class(class_type);
    if (class_type->variant.class_struct_union.is_nonreal_class &&
        class_type->
                  variant.class_struct_union.extra_info->assoc_scope == NULL) {
      is_proxy_or_nonreal_class_lookup = TRUE;
    }  /* if */
    any_nonreal_base_classes = cssp->any_nonreal_base_classes;
  }  /* if */
  /* Determine whether the thing being looked up is a template dependent
     conversion operator name in an expression context.  When looking for
     such names, the ordinary lookup is suppressed causing an unknown
     function symbol to be created later. */
  if (locator->is_conversion_name && (options & IDL_IS_EXPR_CONTEXT) != 0) {
    dependent_conversion_operator =
        is_or_contains_template_param(locator->variant.conversion_result_type);
  }  /* if */
  sym = locator->specific_symbol;
  if (is_error_locator(*locator)) {
    /* The locator is an error locator, so return NULL (i.e., no symbol
       found). */
    sym = NULL;
  } else if (sym != NULL) {
    /* The locator is for a specific symbol, so return the symbol for it. */
    /* Make sure that the symbol we are returning matches the lookup
       criteria.  This check is suppressed if the do_not_clear_specific_symbol
       flag is set, which usually indicates that the symbol is a coalesced
       template reference. */
#if CHECKING
    a_symbol_ptr  fund_sym = fundamental_symbol_of(sym);
    check_assertion(is_acceptable_symbol(sym, fund_sym) ||
                    locator->do_not_clear_specific_symbol);
#endif /* CHECKING */
  } else {
    /* Search for a symbol in the right scope.  Suppress the normal lookup
       part of the search when looking for a dependent conversion operator in
       an expression context. */
    if (!dependent_conversion_operator) {
      /* First, search the list of inactive symbols.  These are class
         members for classes that are no longer active.  Or, in C,
         fields of structs/unions. */
      tag_symbol = NULL;
      for (sym = inactive_symbol_list_from_locator(*locator);
           sym != NULL;
           sym = sym->next) {
        a_symbol_ptr  fund_sym = fundamental_symbol_of(sym);
        if (is_acceptable_symbol(sym, fund_sym)) {
          /* Found an acceptable symbol. */
          if (is_proxy_or_nonreal_class_lookup &&
              sym->kind != nonreal_member_symbol_kind(locator, options)) {
            /* The nonreal class member found is a type when a nontype is
               expected or vice-versa.  Ignore this symbol. */
          } else if (any_nonreal_base_classes &&
                     !implicit_typename_enabled &&
                     sym->kind == (a_symbol_kind)sk_projection &&
                     sym->variant.projection.fund_sym_is_nonreal_member &&
                     fund_sym->kind != nonreal_member_symbol_kind(locator,
                                                                  options)) {
            /* The symbol is a projection symbol in derived class that points
               to a nonreal member of a base class.  Ignore this symbol
               when not using implicit-typename, if it is a type when a nontype
               is expected or vice-versa. */
          } else if (direct_class_members_only &&
                     sym->kind == (a_symbol_kind)sk_projection &&
                     !sym->variant.projection.is_using_decl) {
            /* This is a projection symbol not created by a using-declaration.
               This should be ignored for "direct class members only"
               lookups. */
          } else {
            /* If the symbol is a tag symbol, there's the possibility that
               there is a non-type symbol in the same scope later in the list
               (because the inactive list is not ordered in any way).  Save the
               tag symbol and keep looking.  If nothing else turns up,
               use the tag symbol. */
            if (!is_tag_symbol(fund_sym)) goto end_lookup;
            tag_symbol = sym;
          }  /* if */
        }  /* if */
      }  /* for */
      /* We reached the end of the list.  If there is a tag symbol saved
         within the loop, use it. */
      if (tag_symbol != NULL) {
        sym = tag_symbol;
        goto end_lookup;
      }  /* if */
    }  /* if */
    if (is_proxy_or_nonreal_class_lookup &&
        !(options & IDL_DO_NOT_ADD_TO_NONREAL_CLASS)) {
      /* When looking up a name in a proxy or nonreal class, the name is
         always found.  If we did not find the name in the search
         above then we must create a symbol now. */
      sym = add_member_to_proxy_or_nonreal_class(class_type, options, locator);
      goto end_lookup;
    }  /* if */
    if (C_dialect == C_dialect_cplusplus) {
      /* The name was not found on the inactive symbols list.  Try the
         active symbols list.  This would come up when a qualified name
         is used when the qualification is not really necessary, i.e.,
         when we're inside the class mentioned in the qualifier. */
      for (sym = symbol_list_from_locator(*locator);
           sym != NULL;
           sym = sym->next) {
        a_symbol_ptr  fund_sym = fundamental_symbol_of(sym);
        if (is_acceptable_symbol(sym, fund_sym)) {
          if (any_nonreal_base_classes &&
              !implicit_typename_enabled &&
              sym->kind == (a_symbol_kind)sk_projection &&
              sym->variant.projection.fund_sym_is_nonreal_member &&
              fund_sym->kind != nonreal_member_symbol_kind(locator, options)) {
          /* The symbol is a projection symbol in derived class that points
             to a nonreal member of a base class.  Ignore this symbol
             when not using implicit-typename, if it is a type when a nontype
             is expected or vice-versa. */
          } else if (direct_class_members_only &&
                     sym->kind == (a_symbol_kind)sk_projection &&
                     !sym->variant.projection.is_using_decl) {
            /* This is a projection symbol not created by a using-declaration.
               This should be ignored for "direct class members only"
               lookups. */
          } else {
            /* Found an acceptable symbol. */
            goto end_lookup;
          }  /* if */
        }  /* if */
      }  /* for */
      /* Look to see if the name is the name of a constructor or destructor
         for the class.  The symbols for those are not entered in the
         normal symbol table; they're pointed to from the class symbol
         supplement. */
      class_symbol = (a_symbol_ptr)(class_type->source_corresp.assoc_info);
      cssp = class_symbol->variant.class_struct_union.extra_info;
      if (locator->symbol_header == class_symbol->header) {
        /* Looking up the class name within itself.  Return the constructor if
           there is one. */
        sym = cssp->constructor;
        if (sym != NULL) {
          /* There is a constructor.  Change the locator symbol header to
             the header for the constructor rather than the header for the
             class.  They have the same name, but different headers. */
          locator->symbol_header = sym->header;
          goto end_lookup;
        }  /* if */
      } else if (cssp->destructor != NULL &&
                 locator->symbol_header == cssp->destructor->header) {
        /* This is the destructor. */
        sym = cssp->destructor;
        goto end_lookup;
      }  /* if */
      if (!direct_class_members_only) {
        /* The name was not found.  Try looking for a member symbol that can
           be projected into the class.  When doing a "direct class members
           only" lookup, projection symbols are not created and conversion
	   template instances are not found. */
        determine_projected_symbol_insert_location(locator,
                                                   class_type,
                                                   &add_to_active_list,
                                                   &insert_sym);
        (void)find_projected_symbol(
                                 class_type, locator, options,
                                 /*look_in_dependent_bases=*/TRUE,
                                 /*tentative_type_lookup=*/FALSE,
                                 /*tentative_template_lookup=*/FALSE,
                                 (options & IDL_HIDDEN_NAME_LOOKUP) != 0 ||
                                 (options & IDL_DO_NOT_CREATE_PROJ_SYM) != 0,
                                 add_to_active_list, insert_sym, &sym,
                                 !(options & IDL_DO_NOT_ADD_TO_NONREAL_CLASS));
        if (sym == NULL && locator->is_conversion_name &&
            (options & IDL_USING_DECLARATION) == 0) {
          /* We still haven't found a symbol, we are looking for a conversion
             function,  and this class has conversion function templates.
             See if any of the templates match the type desired.  This
             lookup is suppressed for member using-declarations because
             it should not be possible for a derived class to name a template
	     instance in a using-declaration. */
          sym = look_up_conversion_template_instance(locator, class_type);
        }  /* if */
      }  /* if */
    }  /* if */
end_lookup:
    locator->specific_symbol = sym;
  }  /* if */
  /* If the symbol is a projection symbol, reduce it to the fundamental
     symbol.  The specific_symbol in the locator stays pointing to the
     projection symbol. */
  if (sym != NULL) reduce_projection_symbol_to_fundamental_symbol(sym);
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "class_qualified_id_lookup: id = %s, %s\n",
                     locator->symbol_header->identifier,
                     (sym != NULL) ? "found" : "not found");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return sym;
#undef is_acceptable_symbol
}  /* class_qualified_id_lookup */


a_symbol_ptr enum_qualified_id_lookup(a_symbol_locator		*locator,
				      a_type_ptr		enum_type)
/*
Look up the identifier indicated by *locator as an enumerator associated
with the enumeration specified by enum_type.  This is used in Microsoft
mode to permit an enumerator to be used in a qualified name.  The enumerator
that is found, or NULL is no matching symbol is found.
*/
{
  a_symbol_ptr	sym;

/* Local macro that tests whether or not a symbol is acceptable. */
#define is_acceptable_symbol(sym)                                     \
  ((sym)->kind == (a_symbol_kind)sk_constant &&			      \
   (sym)->variant.constant->type == enum_type)

  db_enter(4, "enum_qualified_id_lookup");
  /* Remove any typedefs on the enum type. */
  enum_type = skip_typerefs(enum_type);
  sym = locator->specific_symbol;
  if (is_error_locator(*locator)) {
    /* The locator is an error locator, so return NULL (i.e., no symbol
       found). */
    sym = NULL;
  } else if (sym != NULL) {
    /* The locator is for a specific symbol, so return the symbol for it. */
    /* Make sure that the symbol we are returning matches the lookup
       criteria.  This check is suppressed if the do_not_clear_specific_symbol
       flag is set, which usually indicates that the symbol is a coalesced
       template reference. */
    check_assertion(is_acceptable_symbol(sym) ||
                    locator->do_not_clear_specific_symbol);
  } else {
    /* Try to find an acceptable symbol on the active list. */
    sym = symbol_list_from_locator(*locator);
    for (; sym != NULL; sym = sym->next) {
      if (is_acceptable_symbol(sym)) break;
    }  /* for */
    if (sym == NULL) {
      /* No symbol was found on the active list.  Look on the inactive list. */
      sym = inactive_symbol_list_from_locator(*locator);
      for (; sym != NULL; sym = sym->next) {
        if (is_acceptable_symbol(sym)) break;
      }  /* for */
    }  /* if */
    locator->specific_symbol = sym;
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "enum_qualified_id_lookup: id = %s, %s\n",
                     locator->symbol_header->identifier,
                     (sym != NULL) ? "found" : "not found");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return sym;
#undef is_acceptable_symbol
}  /* enum_qualified_id_lookup */


/* Forward declaration. */
static
a_symbol_ptr lookup_in_namespace(a_symbol_locator         *locator,
                                 a_namespace_ptr          ns_ptr,
                                 an_id_lookup_options_set options,
                                 a_namespace_ptr	  orig_ns_ptr,
				 a_symbol_ptr		  *synth_sym,
                                 a_boolean                *any_errors);


static
a_symbol_ptr qualified_using_directive_lookup(
                                 a_symbol_locator         *locator,
                                 a_namespace_ptr          ns_ptr,
                                 an_id_lookup_options_set options,
                                 a_namespace_ptr	  orig_ns_ptr,
				 a_symbol_ptr		  *synth_sym,
                                 a_boolean                *any_errors)
/*
Do a qualified lookup of namespaces nominated by using directives
in this scope.  This is used for qualified namespace and file scope
lookups.  For functions, the symbol returned may be an overload set
containing functions from several namespaces.  For nonfunctions,
an ambiguity may need to be diagnosed.

locator.extra_infoibes the symbol being looked up.  ns_ptr is the namespace
in which we should look for the symbol.  options are the lookup
options to be used.  orig_ns_ptr is the namespace specified in the
qualifier.  *synth_sym points to a synthesized projection symbol that
captures the results of the lookup.  *any_errors is set to TRUE if
an ambiguity is detected.

         D        E
          \      /
           B    C
            \  /
             A

  namespace E { int l; }  
  namespace D { int j, l; void g(double); }
  namespace C { int k;    void g(char); using namespace E; }
  namespace B { int j;    void g(int);  using namespace D; }
  namespace A { int i;    void f();     using namespace B; using namespace C;}

A qualified lookup begins with the namespace specified by the qualifier.
If the name is found there, the lookup stops.  If it is not found there,
the lookup looks in each of the namespaces used in using directives in
that namespace.  Functions from each namespace are merged together
into an overload set.  If more than one name is found, and they are not
all functions, the lookup is ambiguous.

Using the example above, the result of the following lookups are
as follows:

	Lookup		Result
 	------		------
	A::i		A::i
	A::j		B::j
	A::k		C::k
	A::l		ambiguous (in D and E)
	A::f()		A::f()
	A::g(1)		B::g(int)
	A::g('x')	C::g(char)
	A::g(2.0)	ambiguous (B::g or C::g.  D::g is hidden)

*/
{
  a_using_decl_ptr			udp;
  a_symbol_ptr				sym;
  a_namespace_symbol_supplement_ptr	nssp = NULL;

  /* For a file scope qualified lookup, get its list of using directives
     from the file scope entry.  For namespace scopes, get it from the
     scope associated with the namespace. */
  if (ns_ptr == NULL) {
    udp = il_header.primary_scope->using_decls;
  } else {
    udp = ns_ptr->variant.assoc_scope->using_decls;
    nssp = symbol_supplement_for_namespace(ns_ptr);
  }  /* if */
  /* Set a flag that indicates that this namespace is being processed so
     that in case of a recursive reference it is not visited again.
     Note that it is still possible for a namespace to be visited twice
     if it appears more than once on the lattice.  From a language
     point of view it should not be visited twice, but our implementation
     will disregard symbols that are already part of the lookup set. */
  if (nssp != NULL) nssp->visited_by_qualified_lookup = TRUE;
  /* Look through each of the using directives in this namespace.  We
     keep going even if an ambiguity is detected because the symbol
     returned may differ depending on the lookup options so we need
     to check each symbol to make sure we return the appropriate one. */
  for (; udp != NULL; udp = udp->next) {
    if (udp->is_using_directive) {
      a_namespace_symbol_supplement_ptr	next_nssp;
      a_namespace_ptr			assoc_namespace;

      assoc_namespace =
                  skip_namespace_aliases((a_namespace_ptr)udp->entity.ptr);
      next_nssp = symbol_supplement_for_namespace(assoc_namespace);
      /* Skip this namespace if we have already looked in it. */
      if (next_nssp->visited_by_qualified_lookup) continue;
      sym = lookup_in_namespace(locator, assoc_namespace, options,
                                orig_ns_ptr, synth_sym, any_errors);
      if (sym != NULL && !sym->synthesized_namespace_projection) {
        /* If this lookup found a symbol, add it to the lookup set.
           Don't do this if it is already a synthesized namespace
           projection -- such symbols are already represented in synth_sym. */
        if (*synth_sym == NULL) {
          /* Look for an existing synthesized namespace projection symbol
             from a previous lookup that can be reused. */
          *synth_sym = find_synthesized_projection_symbol
                                    (locator, options,
                                     /*qualified_lookup=*/TRUE, orig_ns_ptr);
        }  /* if */
        /* Add the new symbol to an existing lookup set.  Note that
           *synth_sym may be NULL at this point. */
        *synth_sym = add_symbol_to_lookup_set(*synth_sym, sym, locator,
                                              /*qualified_lookup=*/TRUE,
                                              orig_ns_ptr, options,
                                              any_errors);
      }  /* if */
    }  /* if */
  }  /* for */
  /* Clear the flag that indicates this namespace is being processed. */
  if (nssp != NULL) nssp->visited_by_qualified_lookup = FALSE;
  sym = *synth_sym;
  return sym;
}  /* qualified_using_directive_lookup */



static
a_symbol_ptr lookup_in_namespace(a_symbol_locator         *locator,
                                 a_namespace_ptr          ns_ptr,
                                 an_id_lookup_options_set options,
                                 a_namespace_ptr	  orig_ns_ptr,
				 a_symbol_ptr		  *synth_sym,
                                 a_boolean                *any_errors)
/*
Look up the identifier indicated by *locator in the namespace indicated by
ns_ptr, and return a pointer to the symbol found, or NULL if
the symbol is not found.  ns_ptr must refer to an actual namespace and not
a namespace alias.  options indicates a set of special options,
as a bit set.

This routine is called by namespace_qualified_id_lookup, and then calls
itself recursively to look in the namespaces of any using directives
present in the namespace.  A flag is set in the namespace symbol
supplement when each namespace is searched.  This flag is checked
to make sure that no namespace is searched more than once.

orig_ns_ptr is a pointer to the namespace specified in the call to
namespace_qualified_id_lookup.
*/
{
  a_symbol_ptr	sym;
  a_symbol_ptr	tag_symbol;
  a_boolean   	must_be_class_or_namespace
                                 = (options & IDL_MUST_BE_CLASS_OR_NAMESPACE);
  a_boolean    	must_be_tag = (options & IDL_MUST_BE_TAG);
  a_boolean    	must_be_class = (options & IDL_MUST_BE_CLASS);
  a_boolean	is_linkage_or_friend_lookup =
                         (options & (IDL_LINKAGE_LOOKUP | IDL_FRIEND_LOOKUP));
  a_boolean	direct_namespace_members_only = 
                         (options & IDL_DIRECT_NAMESPACE_MEMBERS_ONLY) != 0;
  a_boolean	check_decl_seq =
                               (options & IDL_SUPPRESS_DECL_SEQ_CHECK) == 0 &&
                               !is_linkage_or_friend_lookup;
  a_decl_sequence_number
		decl_seq_number = NO_DECL_SEQUENCE_NUMBER;

/* Local macro that tests whether or not a symbol is acceptable. */
#define is_acceptable_symbol(sym, fund_sym)                           \
  ((!(fund_sym->is_invisible) || is_linkage_or_friend_lookup) &&      \
   (!(sym)->is_class_member) &&                                       \
   (sym)->parent.namespace_ptr == ns_ptr &&                           \
   (!must_be_class_or_namespace ||				      \
    symbol_may_precede_qualifier(fund_sym)) &&     		      \
   (!must_be_class ||				     		      \
    is_class_or_class_proxy_symbol(fund_sym)) &&      		      \
   (!must_be_tag || is_tag_or_tag_proxy_symbol(fund_sym)) &&	      \
   (!check_decl_seq ||						      \
    (decl_seq_number == NO_DECL_SEQUENCE_NUMBER ||	              \
     decl_seq_number >= (sym)->decl_seq)))

  db_enter(4, "lookup_in_namespace");
  /* Get the declaration sequence number to be used for this lookup. */
  decl_seq_number = get_effective_decl_seq();
  /* Search for a symbol in the right scope. */
  /* First, search the list of inactive symbols.  Namespace symbols
     are moved to the inactive list after the initial definition of
     the namespace. */
  tag_symbol = NULL;
  for (sym = inactive_symbol_list_from_locator(*locator);
       sym != NULL;
       sym = sym->next) {
    a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
    if (is_acceptable_symbol(sym, fund_sym)) {
      /* Found an acceptable symbol. */
      /* If the symbol is a tag symbol, there's the possibility that
         there is a non-type symbol in the same scope later in the list
         (because the inactive list is not ordered in any way).  Save the
         tag symbol and keep looking.  If nothing else turns up,
         use the tag symbol. */
      if (!is_tag_symbol(fund_sym)) goto end_lookup;
      tag_symbol = sym;
    }  /* if */
  }  /* for */
  /* We reached the end of the list.  If there is a tag symbol saved
     within the loop, use it. */
  if (tag_symbol != NULL) {
    sym = tag_symbol;
    goto end_lookup;
  }  /* if */
  /* The name was not found on the inactive symbols list.  Try the
     active symbols list.  This would be used during the initial
     definition of the namespace. */
  for (sym = symbol_list_from_locator(*locator);
       sym != NULL;
       sym = sym->next) {
    a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
    if (is_acceptable_symbol(sym, fund_sym)) {
      /* Found an acceptable symbol. */
      goto end_lookup;
    }  /* if */
  }  /* for */
end_lookup:
  if (sym == NULL && !is_linkage_or_friend_lookup &&
      !direct_namespace_members_only) {
     /* If the symbol was not found in this namespace, look in namespaces
        visible because of using directives.  Skip this process for a
        linkage lookup.  A linkage or friend lookup should only find names
        that are actually defined in a scope. */
    sym = qualified_using_directive_lookup(locator, ns_ptr, options,
                                           orig_ns_ptr, synth_sym, any_errors);
  }  /* if */
  db_exit();
  return sym;
#undef is_acceptable_symbol
}  /* lookup_in_namespace */


a_symbol_ptr namespace_qualified_id_lookup(a_symbol_locator         *locator,
                                           a_namespace_ptr          ns_ptr,
                                           an_id_lookup_options_set options)
/*
Look up the identifier indicated by *locator in the namespace indicated by
ns_ptr, and return a pointer to the symbol found, or NULL if
the symbol is not found.  ns_ptr must refer to an actual namespace and not
a namespace alias.  options indicates a set of special options,
as a bit set.  For example, if IDL_MUST_BE_CLASS_OR_NAMESPACE is TRUE,
the symbol found must be a class name (or typedef to a class name) or a
namespace.  This routine is used only in C++ mode.
*/
{
  a_symbol_ptr	sym;
  a_symbol_ptr	synth_sym = NULL;
  a_boolean	any_errors = FALSE;

  db_enter(4, "namespace_qualified_id_lookup");
  if ((sym = locator->specific_symbol) != NULL) {
    /* There is an existing specific symbol. */
  } else if (ignore_std_namespace &&
             ns_ptr == symbol_for_namespace_std->variant.namespace_info.ptr) {
    /* When using the g++ compatibility feature that treats "std" as
       a synonym for the global namespace, do the lookup in the file scope. */
    sym = file_scope_id_lookup(locator, options);
  } else {
    /* Search for a symbol in the right scope. */
    sym = lookup_in_namespace(locator, ns_ptr, options, ns_ptr, &synth_sym,
                              &any_errors);
    locator->specific_symbol = sym;
  }  /* if */
  /* If the symbol is a projection symbol, reduce it to the fundamental
     symbol.  The specific_symbol in the locator stays pointing to the
     projection symbol. */
  if (sym != NULL) reduce_projection_symbol_to_fundamental_symbol(sym);
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "namespace_qualified_id_lookup: id = %s, %s\n",
                     locator->symbol_header->identifier,
                     (sym != NULL) ? "found" : "not found");
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return sym;
}  /* namespace_qualified_id_lookup */


a_symbol_ptr file_scope_id_lookup(a_symbol_locator         *locator,
                                  an_id_lookup_options_set options)
/*
Look up the identifier indicated by *locator in the file scope, and
return a pointer to the symbol found, or NULL if the symbol is not found.
options indicates a set of special options, as a bit set.  For example,
if IDL_MUST_BE_CLASS_OR_NAMESPACE is TRUE, the symbol found must be a class
name (or a typedef to a class name) or a namespace.  Only symbols in the
nsk_other name space are considered.  This routine is used for the unary
"::" qualifier and other cases in which it is necessary to determine
whether a given symbol exists in the file scope.  It is used in both
C and C++ (in C it is used for identifier linkage).  If the name is not
found in the file scope, and this is not a linkage lookup, the lookup
will also look in any namespaces used in using directives in the
file scope.
*/
{
  a_symbol_ptr  sym;
  a_boolean     must_be_class_or_namespace
                                  = (options & IDL_MUST_BE_CLASS_OR_NAMESPACE);
  a_boolean     must_be_tag = (options & IDL_MUST_BE_TAG);
  a_boolean     must_be_class = (options & IDL_MUST_BE_CLASS);
  a_symbol_ptr	synth_sym = NULL;
  a_boolean	any_errors = FALSE;
  a_boolean	is_linkage_or_friend_lookup =
                         (options & (IDL_LINKAGE_LOOKUP | IDL_FRIEND_LOOKUP));
  a_boolean	direct_namespace_members_only = 
                         (options & IDL_DIRECT_NAMESPACE_MEMBERS_ONLY) != 0;
  a_boolean	check_decl_seq =
                               (options & IDL_SUPPRESS_DECL_SEQ_CHECK) == 0 &&
                               !is_linkage_or_friend_lookup;
  a_decl_sequence_number
		decl_seq_number = NO_DECL_SEQUENCE_NUMBER;

/* Local macro that tests whether or not a symbol is acceptable. */
/* symbol_may_precede_qualifier checks for a symbol that is a class,
   class template, namespace, or template type parameter.  The name
   space test is needed when searching the file scope so that macro symbols
   are not found. */
#define is_acceptable_symbol(sym, fund_sym)                           \
  ((!(fund_sym->is_invisible) || is_linkage_or_friend_lookup) &&      \
   (sym)->decl_scope == file_scope_number &&                          \
   (name_space_for_symbol_kind[(int)sym->kind] == nsk_other) &&       \
   (!must_be_class_or_namespace ||				      \
    symbol_may_precede_qualifier(fund_sym)) && 			      \
   (!must_be_class ||				      		      \
    is_class_or_class_proxy_symbol(fund_sym)) &&      		      \
   (!must_be_tag || is_tag_symbol(fund_sym)) &&			      \
   (!check_decl_seq ||						      \
    (decl_seq_number == NO_DECL_SEQUENCE_NUMBER ||	              \
     decl_seq_number >= (sym)->decl_seq)))

  db_enter(4, "file_scope_id_lookup");
  /* Get the declaration sequence number to be used for this lookup. */
  decl_seq_number = get_effective_decl_seq();
  if ((sym = locator->specific_symbol) != NULL) {
    /* There is an existing specific symbol. */
  } else {
    /* Search for a symbol in the file scope. */
    for (sym = symbol_list_from_locator(*locator);
         sym != NULL;
         sym = sym->next) {
      a_symbol_ptr	fund_sym = fundamental_symbol_of(sym);
      if (is_acceptable_symbol(sym, fund_sym)) break;
    }  /* for */
    if (sym == NULL && !is_linkage_or_friend_lookup &&
        !direct_namespace_members_only) {
       /* If the symbol was not found in this namespace, look in namespaces
          visible because of using directives.  Skip this process for a
          linkage lookup.  A linkage or friend lookup should only find names
          that are actually defined in a scope. */
      sym = qualified_using_directive_lookup(locator, (a_namespace_ptr)NULL,
                                             options, (a_namespace_ptr)NULL,
                                             &synth_sym, &any_errors);
    }  /* if */
    locator->specific_symbol = sym;
  }  /* if */
#if DEBUG
  if (debug_level >= 4) {
    fprintf(f_debug, "file_scope_id_lookup: id = %s, %s\n",
                     locator->symbol_header->identifier,
                     (sym != NULL) ? "found" : "not found");
  }  /* if */
#endif /* DEBUG */
  /* If the symbol is a projection symbol, reduce it to the fundamental
     symbol.  The specific_symbol in the locator stays pointing to the
     projection symbol. */
  if (sym != NULL) reduce_projection_symbol_to_fundamental_symbol(sym);
  db_exit();
  return sym;
#undef is_acceptable_symbol
}  /* file_scope_id_lookup */


a_symbol_ptr opname_member_function_symbol(an_opname_kind kind,
                                           a_type_ptr     class_type)
/*
Return a pointer to the symbol entry for the operator function for the
operator identified by kind in class class_type, or NULL if there is no such
operator.  The symbol may be a projection symbol (that's desirable, because
a projection symbol is needed to check for ambiguity and access).
*/
{
  a_symbol_ptr        sym = NULL;
  a_symbol_header_ptr symhdr;
  a_symbol_locator    locator;

  /* See if there are any functions for this operator. */
  symhdr = opname_symbol_table[kind];
  if (symhdr != NULL) {
    /* Yes.  Look for one in the desired class. */
    make_opname_locator(kind, &locator, &pos_curr_token);
    if (class_qualified_id_lookup(&locator, class_type,
                                  (IDL_NO_OPTIONS |
                                   IDL_DO_NOT_ADD_TO_NONREAL_CLASS)) != NULL) {
      /* Get the projection symbol if any. */
      sym = locator.specific_symbol;
    }  /* if */
  }  /* if */
  return sym;
}  /* opname_member_function_symbol */


a_symbol_ptr opname_function_symbol(an_opname_kind kind)
/*
Return a pointer to the symbol entry for the operator function for the
operator identified by kind, or NULL if there is no such operator.
Only non-member functions will be found.  Function templates *will*
be found.
*/
{
  a_symbol_ptr        sym = NULL;
  a_symbol_header_ptr symhdr;

  /* See if there are any functions for this operator. */
  symhdr = opname_symbol_table[kind];
  if (symhdr != NULL) {
    /* Yes.  Look for one that's visible and a non-member function. */
    for (sym = symhdr->symbol; sym != NULL; sym = sym->next) {
      if (!sym->is_class_member &&
          (is_function_symbol(sym) ||
           sym->kind == (a_symbol_kind)sk_function_template)) {
        /* A non-member function or function template. */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return sym;
}  /* opname_function_symbol */


void add_to_arg_dependent_lookup_list(a_type_ptr		arg_type,
				      a_type_list_entry_ptr	*type_list)
/*
Add the type specified by arg_type to the list of types to be used for
argument-dependent lookup specified by type_list.  Return an updated
list pointer in type_list.  *type_list should be NULL on the first call.
*/
{
  a_type_ptr		orig_type = NULL;
  a_type_list_entry_ptr	tlep;

  /* Remove any pointer, references, arrays, or qualifiers on the type. */
  for (; orig_type != arg_type ;) {
    orig_type = arg_type;
    switch (arg_type->kind) {
      case tk_pointer:  /* Includes C++ reference too. */
        arg_type = type_pointed_to(arg_type);
        break;
      case tk_array:
        arg_type = array_element_type(arg_type);
        break;
      case tk_typeref:
        arg_type = arg_type->variant.typeref.type;
        break;
      default:
        break;
    }  /* switch */
    arg_type = skip_typerefs(arg_type);
  }  /* for */
  /* See if the type is already on the list. */
  for (tlep = *type_list; tlep != NULL; tlep = tlep->next) {
    if (tlep->type == arg_type) break;
  }  /* if */
  if (tlep == NULL) {
    /* The type is not on the list -- add it now. */
    tlep = alloc_type_list_entry();
    tlep->type = arg_type;
    /* Add this to the front of the list. */
    tlep->next = *type_list;
    *type_list = tlep;
    /* If the type is a pointer-to-member or function type, add the
       types of which the type is composed to the list. */
    switch (arg_type->kind) {
      case tk_class:
      case tk_struct:
      case tk_union:
        /* A class type.  Add the types of the template type arguments. */
        if (arg_type->variant.class_struct_union.is_template_class) {
          /* Include the types of any template type arguments. */
          a_template_arg_ptr	tap;
          tap = arg_type->variant.class_struct_union.extra_info->
                                                             template_arg_list;
          for (; tap != NULL; tap = tap->next) {
            if (is_type_templ_arg(tap)) {
              /* Note that nontype and template template arguments do not
                 influence argument dependent lookup. */
              add_to_arg_dependent_lookup_list(tap->variant.type, type_list);
            } /* if */
          }  /* for */
        }  /* if */
        break;
      case tk_ptr_to_member:
        /* A pointer to member type; add the type of the member, and the
           class type. */
        add_to_arg_dependent_lookup_list(pm_member_type(arg_type), type_list);
        add_to_arg_dependent_lookup_list(pm_class_type(arg_type), type_list);
        break;
      case tk_routine:
        /* A function type; add the types of each parameter, and the return
           type. */
        { a_routine_type_supplement_ptr	rtsp;
          a_param_type_ptr			ptp;
          rtsp = arg_type->variant.routine.extra_info;
          for (ptp = rtsp->param_type_list; ptp != NULL; ptp = ptp->next) {
           add_to_arg_dependent_lookup_list(ptp->type, type_list);
          }  /* for */
          add_to_arg_dependent_lookup_list(
                             arg_type->variant.routine.return_type, type_list);
        }
        break;
      default:
        break;
    }  /* switch */
  }  /* if */
#if DEBUG
  if (db_flag_is_set("add_to_arg_dependent_lookup_list")) {
    fprintf(f_debug, "add_to_arg_dependent_lookup_list:\n");
    for (tlep = *type_list; tlep != NULL; tlep = tlep->next) {
      fprintf(f_debug, "  ");
      db_type_name(tlep->type);
      fprintf(f_debug, "\n");
    }  /* for */
  }  /* if */
#endif /* DEBUG */
}  /* add_to_arg_dependent_lookup_list */


static void add_namespace_of_type_to_lookup_list(
			a_type_ptr			type,
			a_namespace_list_entry_ptr	*namespace_list)
/*
Add the namespace in which "type" is defined to the namespace_list.
*/
{
  a_namespace_list_entry_ptr	nlep;
  a_namespace_ptr		nsp;

  if (!type->source_corresp.is_local_to_function) {
    /* Local types have no associated namespace. */
    /* Determine the parent namespace. */
    while (type->source_corresp.is_class_member) {
      type = type->source_corresp.parent.class_type;
    }  /* while */
    /* Note that "nsp" will be NULL for global scope types. */
    nsp = type->source_corresp.parent.namespace_ptr;
    /* Look for its namespace on the namespace list. */
    for (nlep = *namespace_list; nlep != NULL; nlep = nlep->next) {
      if (nlep->ptr == nsp) break;
    }  /* for */
    if (nlep == NULL) {
      /* The namespace was not found.  Add it to the list now. */
      nlep = alloc_namespace_list_entry();
      nlep->ptr = nsp;
      nlep->next = *namespace_list;
      *namespace_list = nlep;
    }  /* if */
  }  /* if */
}  /* add_namespace_of_type_to_lookup_list */


static void add_class_to_lookup_lists(
			a_type_ptr			class_type,
			a_namespace_list_entry_ptr	*namespace_list,
			a_type_list_entry_ptr		*class_list)
/*
Add the specified class_type to the class_list and add the namespace in
which it is defined to the namespace_list.
*/
{
  a_type_list_entry_ptr	tlep;

  check_assertion(class_type->kind == (a_type_kind)tk_class ||
                  class_type->kind == (a_type_kind)tk_struct ||
                  class_type->kind == (a_type_kind)tk_union);
  /* Look for this type on the class list. */
  for (tlep = *class_list; tlep != NULL; tlep = tlep->next) {
    if (tlep->type == class_type) break;
  }  /* for */
  if (tlep == NULL) {
    /* It is not on the class list.  Add it to both lists now.  If it is on
       the class list, its namespace will already be on the namespace list
       too. */
    tlep = alloc_type_list_entry();
    tlep->type = class_type;
    /* Add this to the front of the list. */
    tlep->next = *class_list;
    *class_list = tlep;
    add_namespace_of_type_to_lookup_list(class_type, namespace_list);
  }  /* if */
}  /* add_class_to_lookup_lists */


static void determine_assoc_namespaces_and_classes_for_type(
			a_type_ptr			type,
			a_namespace_list_entry_ptr	*namespace_list,
			a_type_list_entry_ptr		*class_list)
/*
Determine the associated classes and namespaces of "type".  Add the
associated namespaces and classes to "namespace_list" and "class_list".
*/
{
  a_class_type_supplement_ptr	ctsp;
  a_base_class_ptr		bcp;
  a_boolean			add_parent = FALSE;

  switch (type->kind) {
    case tk_class:
    case tk_struct:
    case tk_union:
      /* The standard specifies different behavior for unions vs. classes.
         Specifically, the class of which a class is a member is not
         an associated class, and a union is not one of its own associated
         classes.  These seem to be errors in the standard, so this
         implementation treats unions and classes equivalently. */
      /* Add the class itself to the lookup list. */
      add_class_to_lookup_lists(type, namespace_list, class_list);
      /* If this is a template, make sure it is instantiated. */
      complete_class_type_is_needed(type);
      /* Add its base classes. */
      ctsp = type->variant.class_struct_union.extra_info;
      for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
        add_class_to_lookup_lists(bcp->type, namespace_list, class_list);
      }  /* for */
      /* The enclosing class (if any) and namespace should be included. */
      add_parent = TRUE;
      break;
    case tk_integer:
      /* Enums are represented using a tk_integer. */
      if (type->variant.integer.enum_type) {
        /* The enclosing class (if any) and namespace should be included. */
        add_parent = TRUE;
      }  /* if */
      break;
    default:
      break;
  }  /* switch */
  if (add_parent) {
    /* If this is a member of another class, add the class of which it is
       a members. */
    if (type->source_corresp.is_class_member) {
      add_class_to_lookup_lists(type->source_corresp.parent.class_type,
                                namespace_list, class_list);
    } else {
      /* Add the namespace in which the type was defined to the list. */
      add_namespace_of_type_to_lookup_list(type, namespace_list);
    }  /* if */
  }  /* if */
}  /* determine_assoc_namespaces_and_classes_for_type */


static void find_friend_functions_for_class(
					a_symbol_locator	*locator,
					a_type_ptr		class_type,
					a_symbol_list_entry_ptr	*symbol_list)
/*
Go through the friend functions list of class_type looking for a symbol
that matches the name specified by "locator".  If a match is found, add
the entry to to symbol_list.
*/
{
  a_symbol_ptr			sym;
  a_class_symbol_supplement_ptr	cssp;
  a_symbol_list_entry_ptr	slep;

  cssp = symbol_supplement_for_class(class_type);
  for (sym = cssp->friend_functions; sym != NULL; sym = sym->next) {
    if (sym->header == locator->symbol_header) break;
  }  /* for */
  if (sym != NULL) {
    /* A match was found.  Add this entry to the symbol list.  We don't check
       for an existing entry because it should not be possible for such an
       entry to exist. */
    slep = alloc_symbol_list_entry();
    slep->symbol = sym;
    slep->next = *symbol_list;
    *symbol_list = slep;
  }  /* if */
}  /* find_friend_functions_for_class */


static void find_functions_for_namespace(
					a_symbol_locator	*locator,
					a_namespace_ptr		nsp,
					a_symbol_list_entry_ptr	*symbol_list)
/*
Look for a function in the namespace specified by "nsp" whose name is specified
by "locator".  Note that "nsp" will be NULL for the global scope.
If a match is found, add the entry to to symbol_list.
*/
{
  a_symbol_ptr			sym;
  a_symbol_list_entry_ptr	slep;

  /* Clear the specific symbol so that it does not influence the lookup
     below. */
  clear_specific_symbol(*locator);
  /* Look up the symbol in the specified namespace or in the global scope.
     A "direct namespace members only" lookup is used to prevent other
     namespaces from being searched if the specified namespace includes
     using-directives. */
  if (nsp != NULL) {
    sym = namespace_qualified_id_lookup(locator, nsp,
                                        IDL_DIRECT_NAMESPACE_MEMBERS_ONLY |
                                        IDL_SUPPRESS_DECL_SEQ_CHECK);
  } else {
    sym = file_scope_id_lookup(locator, IDL_DIRECT_NAMESPACE_MEMBERS_ONLY |
                                        IDL_SUPPRESS_DECL_SEQ_CHECK);
  }  /* if */
  if (sym != NULL) {
    a_symbol_ptr	fund_sym;
    /* A symbol was found in the namespace.  Make sure it is a function
       symbol. */
    fund_sym = fundamental_symbol_of(sym);
    if (is_function_or_template_symbol(fund_sym)) {
      /* A match was found.  Add this entry to the symbol list.  We don't check
         for an existing entry because it should not be possible for such an
         entry to exist. */
      slep = alloc_symbol_list_entry();
      slep->symbol = sym;
      slep->next = *symbol_list;
      *symbol_list = slep;
    }  /* if */
  }  /* if */
}  /* find_functions_for_namespace */


a_symbol_list_entry_ptr argument_dependent_lookup(
					a_symbol_ptr		normal_sym,
					a_symbol_locator	*locator,
					a_type_list_entry_ptr	*type_list)
/*
Perform C++ argument-dependent lookup as specified in 3.4.2
[basic.lookup.koenig] of the C++ standard.  normal_sym is the result
of a normal lookup of the function name in the context of the call and
may be NULL.  type_list is a list of argument types to be used to
produce a list of associated classes and namespaces from which
candidate functions should be considered.  locator is the symbol
locator associated with the name that is being looked up.

normal_sym, if not NULL, is included in the symbol list that is returned
and must be the first entry on the list.

This routine builds a list of symbol list entries.  Each entry points to
a sk_routine, sk_overloaded_function, or sk_namespace_projection symbol.
The same function may be pointed to directly and/or indirectly by
any number of these symbols, so the caller is responsible for ignoring
duplicate entries.

The list pointed to by *type_list is freed by this routine, and *type_list
is set to NULL.
*/
{
  a_symbol_list_entry_ptr	slep;
  a_symbol_list_entry_ptr	symbol_list = NULL;
  a_type_list_entry_ptr		tlep;
  a_namespace_list_entry_ptr	nlep;
  a_namespace_list_entry_ptr	namespace_list = NULL;
  a_type_list_entry_ptr		class_list = NULL;

  /* Build a list of namespaces and classes to be included in the search. */
  for (tlep = *type_list; tlep != NULL; tlep = tlep->next) {
    determine_assoc_namespaces_and_classes_for_type(
                                     tlep->type, &namespace_list, &class_list);
  }  /* for */
  /* Go through the type list and create the symbol list entries for any
     matching friend declarations. */
  for (tlep = class_list; tlep != NULL; tlep = tlep->next) {
    find_friend_functions_for_class(locator, tlep->type, &symbol_list);
  }  /* for */
  /* Go through the namespace list and look for matching functions in each
     of the namespaces. */
  for (nlep = namespace_list; nlep != NULL; nlep = nlep->next) {
    find_functions_for_namespace(locator, nlep->ptr, &symbol_list);
  }  /* for */
  /* Add the specified normal symbol to the list of symbols found. */
  if (normal_sym != NULL) {
    slep = alloc_symbol_list_entry();
    slep->symbol = normal_sym;
    slep->next = symbol_list;
    symbol_list = slep;
  }  /* if */
#if DEBUG
  if (db_flag_is_set("argument_dependent_lookup")) {
    int	i;
    fprintf(f_debug, "argument_dependent_lookup:\n");
    for (i = 0, slep = symbol_list; slep != NULL; slep = slep->next, i++) {
      a_boolean		is_list;
      a_symbol_ptr	sym = slep->symbol;
      fprintf(f_debug, "  symbol list %d:", i);
      db_symbol(sym, "", 2);
      is_list = (sym->kind == (a_symbol_kind)sk_overloaded_function);
      if (is_list) sym = sym->variant.overloaded_function.symbols;
      for (; sym != NULL; sym = (is_list ? sym->next : NULL)) {
        a_symbol_ptr	fund_sym;
        if (is_list) db_symbol(sym, "", 4);
        fund_sym = fundamental_symbol_of(sym);
        if (sym != fund_sym) db_symbol(fund_sym, "  fund_sym:", 4);
        if (is_list) fprintf(f_debug, "\n");
      }  /* for */
    }  /* for */
    for (i = 0, tlep = class_list; tlep != NULL; tlep = tlep->next, i++) {
      fprintf(f_debug, "class list %d: ", i);
      db_type_name(tlep->type);
      fprintf(f_debug, "\n");
    }  /* for */
  }  /* if */
#endif /* DEBUG */
  /* Free the lists used to create the symbol list. */
  free_list_of_type_list_entries(*type_list);
  free_list_of_namespace_list_entries(namespace_list);
  free_list_of_type_list_entries(class_list);
  *type_list = NULL;
  /* Make sure that normal_sym is pointed to by the first entry on the
     returned list. */
  check_assertion(normal_sym == NULL || symbol_list->symbol == normal_sym);
  return symbol_list;
}  /* argument_dependent_lookup */


a_decl_sequence_number f_get_effective_decl_seq(void)
/*
Return the declaration sequence number to be used for lookups in the
current scope.
*/
{
  /* The macro that calls this function is responsible for making sure that
     we are inside a template instantiation scope. */
  /* Get the declaration sequence number that indicates which declarations
     should be visible at the point of definition of the template. */
  a_scope_stack_entry_ptr	ssep;
  a_decl_sequence_number	decl_seq_number;

  ssep = &scope_stack[depth_innermost_instantiation_scope];
  check_assertion(ssep->template_decl_info != NULL);
  decl_seq_number = ssep->template_decl_info->decl_seq;
  return decl_seq_number;
}  /* f_get_effective_decl_seq */


void lookup_one_time_init(void)
/*
Do one-time initialization of variables related to name lookup.
*/
{
  init_cleared_lookup_state();
}  /* lookup_one_time_init */


void lookup_init(void)
/*
Initialize static variables related to name lookup.  This is done as a
subroutine (rather than relying on static initialization) so that it
can be redone to compile more than one source file in a single invocation
of the front end.
*/
{
}  /* lookup_init */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

