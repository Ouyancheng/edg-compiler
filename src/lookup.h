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

lookup.h - Declarations related to name lookup.

*/

/* Avoid including these declarations more than once. */
#ifndef LOOKUP_H
#define LOOKUP_H 1

/*
Options for normal_id_lookup, class_qualified_id_lookup, etc.,
represented as a bit set:
*/
#define IDL_MUST_BE_CLASS_OR_NAMESPACE  0x1
				/* The symbol must be a class, struct, or
				   union name, a typedef of one of those, or
                                   a namespace.  In other words, one of the
                                   things that, in C++, may precede a ::. */
#define IDL_MUST_BE_TAG 0x2	/* The symbol must be a class, struct, union,
				   or enum (not a typedef of one of those). */
#define IDL_SUPPRESS_QUALIFIED_NAME_NOT_FOUND_ERROR 0x4
				/* Suppress the error on a qualified name
				   not being found on lookup. */
#define IDL_TENTATIVE_TYPE_LOOKUP 0x8
                                /* We are looking up a symbol to see if it is
				   a type name.  This mode suppresses the
				   introduction of new symbols as a consequence
				   of the lookup.  The primary example of this
				   is when the symbol found is a projection
				   from a base class.  In that case we do not
				   actually create the symbol to represent
				   that projection unless is a type name -- a
				   typedef or tag symbol (class, struct,
				   union, or enum) -- but return NULL instead.
				   This flag also suppresses the out of
				   scope declaration lookup in SVR4 C
				   compatibility mode. */
#define IDL_SKIP_CURR_FUNCTION_SCOPE 0x10
				/* Causes normal_id_lookup to skip over
				   the innermost scope entry which
				   must be a function scope.  This is
				   used to look up the identifiers used
				   in constructor initializer lists.  Names
				   of parameters of the constructor must not
				   be visible during this lookup. */
#define IDL_DO_NOT_ADD_TO_NONREAL_CLASS 0x20
				/* When a name is being looked up in
				   a proxy or nonreal class, this flag
				   suppresses the creation of a new
				   symbol if the name is not found in the
				   class. */
#define IDL_LINKAGE_LOOKUP 0x40
				/* A special lookup used for determining
				   identifier linkage.  This lookup stops
				   at the first namespace scope and suppresses
				   some of the special lookups (such as
				   the using directive lookup). */
#define IDL_PROJ_SYMBOL_ALLOWED 0x80
				/* Causes curr_scope_id_lookup to consider
				   projection symbols (but not synthesized
				   namespace projections). */
#define IDL_SKIP_CLASS_SCOPES 0x100
				/* Causes class and class reactivation scopes
				   to be ignored. */
#define IDL_INSTANTIATION_CONTEXT 0x200
				/* Used within normal_id_lookup to create
				   synthesized namespace projection symbols
				   for instantiation context lookups. */
#define IDL_MUST_BE_NAMESPACE 0x400
				/* The symbol must be a namespace. */
#define IDL_NO_OPTIONS 0	/* No special lookup options. */

/*
Returns TRUE if the specified set of lookup options represents a lookup
whose result can be saved as a synthesized projection symbol and
reused later.
*/
#define is_reusable_using_directive_lookup(option)			\
  ((options & ~(IDL_MUST_BE_TAG |					\
                IDL_MUST_BE_CLASS_OR_NAMESPACE |			\
		IDL_INSTANTIATION_CONTEXT |				\
                IDL_TENTATIVE_TYPE_LOOKUP |				\
                IDL_DO_NOT_ADD_TO_NONREAL_CLASS)) == 0)


extern
a_boolean sym_matches_lookup_options(a_symbol_ptr		sym,
				     an_id_lookup_options_set	options);

extern a_boolean already_in_lookup_set(a_symbol_ptr curr_sym,
                                       a_symbol_ptr new_sym);

extern a_symbol_ptr curr_scope_id_lookup(a_symbol_locator         *locator,
                                         an_id_lookup_options_set options);

extern a_symbol_ptr normal_id_lookup(a_symbol_locator         *locator,
                                     an_id_lookup_options_set options);

extern a_symbol_ptr curr_tag_symbol(a_symbol_locator  *locator,
                                    a_symbol_kind     tag_kind);

extern a_symbol_ptr class_qualified_id_lookup(
                                         a_symbol_locator         *locator,
                                         a_type_ptr               class_type,
                                         an_id_lookup_options_set options);

extern
a_symbol_ptr namespace_qualified_id_lookup(a_symbol_locator         *locator,
                                           a_namespace_ptr          ns_ptr,
                                           an_id_lookup_options_set options);

extern a_symbol_ptr file_scope_id_lookup(a_symbol_locator         *locator,
                                         an_id_lookup_options_set options);

extern a_symbol_ptr opname_member_function_symbol(an_opname_kind kind,
                                                  a_type_ptr     class_type);

extern a_symbol_ptr opname_function_symbol(an_opname_kind kind);

extern a_symbol_list_entry_ptr nonmember_operator_function_lookup(
                                 an_opname_kind kind,
                                 a_type_ptr	type_1,
                                 a_type_ptr     type_2);

extern void lookup_one_time_init(void);

extern void lookup_init(void);

#endif /* ifndef LOOKUP_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
