/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2016 Edison Design Group Inc.                   [_]          *
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
#define IDL_TENTATIVE_TYPE_LOOKUP 0x4
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
#define IDL_SKIP_CURR_SCOPE	0x8
				/* Causes normal_id_lookup to skip over the
				   innermost scope entry (after any template
				   declaration scopes have been skipped if
				   IDL_SKIP_TEMPLATE_DECL_SCOPES is used).
				   (This is used to look up the identifiers
				   used in constructor initializer lists;
				   names of parameters of the constructor must
				   not be visible during this lookup.  It is
				   also used during hidden-name processing to
				   find a name in the innermost scope
				   enclosing the current scope.)  If the
				   scope to be skipped is a class scope, only
				   the current class is skipped -- any base
				   classes are still searched. */
#define IDL_DO_NOT_ADD_TO_NONREAL_CLASS 0x10
				/* When a name is being looked up in
				   a proxy or nonreal class, this flag
				   suppresses the creation of a new
				   symbol if the name is not found in the
				   class. */
#define IDL_LINKAGE_LOOKUP 0x20
				/* A special lookup used for determining
				   identifier linkage.  This lookup stops
				   at the first namespace scope and suppresses
				   some of the special lookups (such as
				   the using directive lookup). */
#define IDL_PROJ_SYMBOL_ALLOWED 0x40
				/* Causes curr_scope_id_lookup to consider
				   projection symbols (but not synthesized
				   namespace projections). */
#define IDL_SKIP_CLASS_SCOPES 0x80
				/* Causes class and class reactivation scopes
				   to be ignored. */
#define IDL_INSTANTIATION_CONTEXT 0x100
				/* Used within normal_id_lookup to create
				   synthesized namespace projection symbols
				   for instantiation context lookups. */
#define IDL_MUST_BE_NAMESPACE 0x200
				/* The symbol must be a namespace. */
#define IDL_TYPENAME_LOOKUP   0x400
				/* When a name is being looked up in a
				   proxy or nonreal class, this flag forces
				   any symbol that may be created to be a
				   type symbol. */
#define IDL_MUST_BE_CLASS     0x800
				/* The symbol must be a class, struct, union,
                                   or a typedef of one of those. */
#define IDL_DIRECT_CLASS_MEMBERS_ONLY 0x1000
				/* For a class-qualified lookup, indicates
				   that only real members of the class, or
				   declarations made visible in the class by
				   using-declarations may be found.
				   This lookup will not create a projection
				   symbol.  This flag also suppresses
				   creation of a conversion operator function
				   based on a template that matches the
				   specified type. */
#define IDL_TREAT_AS_TEMPLATE_ID 0x2000
				/* For a class-qualified lookup, indicates
				   that if a proxy class member is created
				   for this lookup, the member created
				   should be a template, and that the
				   template argument list that follows
				   the identifier should be coalesced. */
#define IDL_FRIEND_LOOKUP 0x4000
				/* For lookup of an unqualified name in a
				   friend declaration, indicates (1) that the
				   lookup does not go beyond the innermost
				   enclosing namespace scope (or, for a local
				   class friend declaration, the innermost
				   enclosing function or block scope), and
				   (2) that using-directives are ignored. May
				   be combined with IDL_MUST_BE_TAG. */
#define IDL_TENTATIVE_TEMPLATE_LOOKUP 0x8000
				/* This is similar to a tentative type lookup
				   but for template names.  In other words,
				   a new symbol (i.e., projection symbol) will
				   not be created unless it is for a
				   template. */
#define IDL_USING_DECLARATION	0x10000
				/* Indicates that the name being looked up
				   is the name in a using-declaration.
				   For member using-declarations this
				   suppresses the special conversion template
				   lookup. */
#define IDL_HIDDEN_NAME_LOOKUP	0x20000
				/* Indicates that the name is being looked
				   up as part of the hidden name table
				   processing or to determine the validity of
				   the C++11 [[hiding]] attribute.  This
				   suppresses the creation of projection
				   symbols. */
#define IDL_SKIP_TEMPLATE_DECL_SCOPES 0x40000
				/* Skip any template declaration scopes before
				   doing other processing. */
#define IDL_DIRECT_NAMESPACE_MEMBERS_ONLY 0x80000
				/* For namespace-qualified and file scope
				   lookups, indicates that only members of the
				   namespace, and not members made visible by
				   using-directives, should be found. */
#define IDL_DO_NOT_CREATE_PROJ_SYM 0x100000
				/* If a name is found in a base class, return
			           that name but do not create a projection
				   symbol. */
#define IDL_SUPPRESS_DECL_SEQ_CHECK 0x200000
				/* For normal, namespace qualified and
				   file-scope qualified lookups, suppress
				   the check of the declaration sequence number
				   during instantiation lookups.  For namespace
				   and file-scope qualified lookups, this is
				   used for argument-dependent lookups.  For
				   normal lookups, this is used for g++
				   lookup emulation. */
#define IDL_IS_EXPR_CONTEXT 0x400000
				/* Flag that indicates a lookup in an
				   expression context.  Controls the kind
				   of nonreal member created. */
#define IDL_MEMBER_OF_UNKNOWN_BASE 0x800000
				/* We are looking up a name that comes from
				   an unknown base class.  See the comment
				   on the tpck_member variant of a_constant
				   for more information. */
#define IDL_IS_FIELD_SELECTION_OPERAND 0x1000000
				/* Specifies that the name being scanned is the
				   operand following a "." or "->" operator. */
#if MICROSOFT_EXTENSIONS_ALLOWED
#define IDL_MEMBER_OF_UNKNOWN_SUPER 0x2000000
				/* We are looking up a name in a base class
				   using the Microsoft __super directive. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#define IDL_USE_PROTOTYPE_NOT_NONREAL 0x4000000
				/* Specifies that a reference such as
				   A<T>::X should be considered to refer to the
				   prototype instantiation X, not the nonreal
				   version of the same name. */
#define IDL_IS_DECLARATOR 0x8000000
				/* TRUE when looking up a qualified name in a
				   declarator. */
#if MICROSOFT_EXTENSIONS_ALLOWED
#define IDL_IS_STATIC_DECL 0x10000000
				/* Flag indicating that the keyword "static"
				   was seen prior to the declarator.  This
				   currently only affects the result of a
				   lookup when constructors are found in
				   C++/CLI mode: If this flag is selected,
				   ordinary (non-static) constructors are not
				   considered and static constructors (if any)
				   are found; otherwise, static constructors
				   are not considered. */
#define IDL_EXCLUDE_BASE_INTERFACE_MEMBERS 0x20000000
				/* TRUE if a lookup in a C++/CLI managed class
				   should ignore members from a base interface
				   class. */
#define IDL_IF_EXISTS_LOOKUP 0x40000000
				/* TRUE when looking up a name in a Microsoft
				   __if_exists or __if_not_exists directive. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#define IDL_IS_LOOKUP_TO_CHECK_FOR_NAME_HIDING 0x80000000
				/* TRUE if the lookup is being done to see if
				   the current declaration hides a name.
				   This differs from IDL_HIDDEN_NAME_LOOKUP,
				   which is used when building the hidden
				   name table. */
#define IDL_NO_OPTIONS 0	/* No special lookup options. */

/*
Returns TRUE if the specified set of lookup options represents a lookup
whose result can be saved as a synthesized projection symbol and
reused later.
*/
#define is_reusable_using_directive_lookup(option)			\
  ((options & ~(IDL_MUST_BE_TAG |					\
                IDL_MUST_BE_CLASS_OR_NAMESPACE |			\
                IDL_MUST_BE_NAMESPACE |					\
                IDL_MUST_BE_CLASS |					\
		IDL_INSTANTIATION_CONTEXT |				\
                IDL_TENTATIVE_TYPE_LOOKUP |				\
		IDL_IS_EXPR_CONTEXT |					\
		IDL_SKIP_CLASS_SCOPES |					\
                IDL_DO_NOT_ADD_TO_NONREAL_CLASS)) == 0)


#if MICROSOFT_EXTENSIONS_ALLOWED

/*
Returns TRUE if the specified set of lookup options represents a lookup
whose result can be saved as on the list of super lookup symbols and
reused later.
*/
#define is_reusable_super_lookup(options)				\
  ((options & ~(IDL_MUST_BE_TAG |					\
                IDL_MUST_BE_CLASS_OR_NAMESPACE |			\
                IDL_MUST_BE_CLASS |					\
                IDL_TENTATIVE_TYPE_LOOKUP |				\
		IDL_IS_EXPR_CONTEXT |					\
                IDL_DO_NOT_ADD_TO_NONREAL_CLASS)) == 0)

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/* Return the declaration sequence number to be used for lookups. */
#define get_effective_decl_seq()					\
  ((!is_nonspecialized_instantiation_context() ||		\
   !do_dependent_name_processing)					\
             ? NO_DECL_SEQUENCE_NUMBER					\
             : f_get_effective_decl_seq())

extern a_decl_sequence_number f_get_effective_decl_seq(void);

extern a_scope_number scope_depth_for_synth_namespace_symbol(void);

extern
a_boolean sym_matches_lookup_options(a_symbol_ptr		sym,
				     an_id_lookup_options_set	options);

extern a_boolean symbols_are_lookup_equivalent(
			a_symbol_ptr			sym1,
			a_symbol_ptr			sym2,
			a_boolean			merge_c_funcs,
			an_id_lookup_options_set	options);

extern
a_boolean already_in_lookup_set(a_symbol_ptr			curr_sym,
                                a_symbol_ptr			new_sym,
				a_boolean			merge_c_funcs,
				an_id_lookup_options_set	options);

extern
a_symbol_ptr f_nonreal_type_if_nested_prototype_type(a_symbol_ptr	sym);

extern a_symbol_ptr curr_scope_id_lookup(a_symbol_locator         *locator,
                                         an_id_lookup_options_set options);

extern a_symbol_ptr normal_id_lookup(a_symbol_locator         *locator,
                                     an_id_lookup_options_set options);

extern a_symbol_ptr curr_tag_symbol(a_symbol_locator  *locator,
                                    a_symbol_kind     tag_kind,
                                    a_boolean         allow_typedef,
                                    a_boolean         is_friend_decl);

extern a_symbol_ptr class_qualified_id_lookup(
                                         a_symbol_locator         *locator,
                                         a_type_ptr               class_type,
                                         an_id_lookup_options_set options);

extern a_type_ptr get_super_class_type(void);

extern a_symbol_ptr super_qualified_id_lookup(
				a_symbol_locator		*locator,
				an_id_lookup_options_set	options);

extern
a_symbol_ptr namespace_qualified_id_lookup(a_symbol_locator         *locator,
                                           a_namespace_ptr          ns_ptr,
                                           an_id_lookup_options_set options);

extern
a_symbol_ptr enum_qualified_id_lookup(a_symbol_locator		*locator,
				      a_type_ptr		enum_type);

extern a_symbol_ptr file_scope_id_lookup(
			a_scope_ptr			file_scope_to_use,
			a_symbol_locator		*locator,
			an_id_lookup_options_set	options);

extern a_symbol_ptr opname_member_function_symbol(an_opname_kind kind,
                                                  a_type_ptr     class_type);

extern a_symbol_ptr opname_function_symbol(an_opname_kind kind);

extern a_boolean any_opname_function_symbol(an_opname_kind kind);

extern
void add_to_arg_dependent_lookup_list(a_type_ptr		arg_type,
				      a_type_list_entry_ptr	*type_list);

extern a_symbol_list_entry_ptr argument_dependent_lookup(
				a_symbol_ptr		normal_sym,
				a_symbol_locator	*locator,
				a_type_list_entry_ptr	*type_list,
				a_boolean		include_std_namespace);

extern
a_type_ptr proxy_class_for_template_param(a_type_ptr   templ_param_type);

extern a_symbol_ptr find_unknown_function_symbol(
					a_symbol_ptr	orig_sym,
					a_boolean	is_qualified_name);

extern void create_nonreal_version_of_nested_type(a_symbol_ptr	orig_sym);

extern a_symbol_ptr create_proxy_or_nonreal_class_member_of_kind(
				a_type_ptr			class_type,
				a_symbol_kind			kind,
				an_id_lookup_options_set	options,
				a_symbol_locator		*locator);

extern a_type_ptr f_orig_nested_type_if_nonreal_nested_type(a_type_ptr	tp);

/*
Macro that calls f_orig_nested_type_if_nonreal_nested_type only if tp
is a template parameter type.
*/
#define orig_nested_type_if_nonreal_nested_type(tp)			\
  ((tp)->kind == (a_type_kind)tk_template_param			        \
    ? f_orig_nested_type_if_nonreal_nested_type(tp)			\
    : (tp))

extern a_symbol_ptr find_conversion_template_instance(
                                a_symbol_locator         *locator,
                                a_symbol_list_entry_ptr  conversion_templates,
                                a_boolean                match_fn_qualifiers,
                                a_type_qualifier_set     fn_qualifiers);

extern
a_symbol_ptr look_up_conversion_function(a_type_ptr		parent_class,
					 a_type_ptr		conv_type,
					 a_source_position	*source_pos);

extern void lookup_one_time_init(void);

extern void lookup_init(void);

#endif /* ifndef LOOKUP_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2016 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
