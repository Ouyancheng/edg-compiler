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

decls.h -- Declarations related to decls.c (having to do with scanning
           of declarations).

*/

/* Avoid including these declarations more than once: */
#ifndef DECLS_H
#define DECLS_H 1

/*
Kinds of linkage, meaning whether or not an identifier declared in
a certain way is linked to (the same as) some other like-named identifier
declared elsewhere.  See 3.1.2.2.
*/
typedef enum /*an_id_linkage_kind*/ {
  idl_none,		/* Identifier is different from others of the same
			   name. */
  idl_internal,		/* Identifier is the same as others in the same
			   compilation with this name. */
  idl_external		/* Identifier is the same as others in the same
			   compilation and in other compilations for
			   the same program */
} an_id_linkage_kind;


/* Return the symbol if the current token is a type name identifier. */
a_symbol_ptr curr_type_symbol(a_boolean is_new_type_name,
                              a_boolean in_prescan,
                              a_boolean in_type_check);

/*
Macro that is TRUE if the current token (which must be an identifier or
the "::" at the start of a qualified name) is a type name.
*/
#define curr_id_is_type_name()						\
  (curr_type_symbol(/*is_new_type_name=*/FALSE, /*in_prescan=*/FALSE,   \
                    /*in_type_check=*/TRUE) != NULL)

/* Test whether or not the current token is the start of a type. */
extern a_boolean is_type_start(a_boolean is_expr_context);

/*
Flags used to specify options to is_decl_start.
*/
typedef int an_is_decl_start_options_set;

#define IDS_NO_OPTIONS		0x0
			/*lint -esym(755,IDS_NO_OPTIONS)*/
#define IDS_EXPR_CONTEXT	0x1
			/* TRUE if we are in an expression context. */
#define IDS_REAL_DECLARATOR_ALLOWED \
				0x2
			/* TRUE if a real declarator is allowed. */
#define IDS_MS_ATTRIB_NOT_ALLOWED \
				0x4
			/* TRUE if a Microsoft attribute is not allowed in
			   this context. */

/* Test whether or not the current token is the start of a declaration. */
extern a_boolean is_decl_start(an_is_decl_start_options_set options);

void issue_no_exception_support_diag_on_throw_spec(
				a_func_info_block_ptr		func_info);

extern a_boolean check_member_function_typedef(a_type_ptr         tp,
                                               a_source_position  *pos);

extern a_boolean f_check_for_overload_anachronism(void);

/*
Return TRUE if the current token is "overload" and the declaration following
it is just an identifier (or a comma-list of identifiers).
*/
#define check_for_overload_anachronism()                               \
  (curr_token == tok_overload && f_check_for_overload_anachronism())

/*
Macro that returns TRUE if the current token is an identifier that is
a template-id.
*/
#define identifier_is_template_id()                                  \
  (is_generalized_identifier_start(GID_NO_OPTIONS) ?        \
         locator_for_curr_id.is_template_id : FALSE)

/*
Macro to be used in conjunction with is_type_keyword to check for
Microsoft extensions.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define or_is_microsoft_type_keyword(tok)                             \
  || (microsoft_mode &&                                               \
      ((tok) == tok_int8  || (tok) == tok_int16 ||                    \
       (tok) == tok_int32 || (tok) == tok_int64))
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define or_is_microsoft_type_keyword(tok)  /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Macro to be used in conjunction with is_type_keyword to check for complex
type extensions.
*/
#if C99_IL_EXTENSIONS_SUPPORTED
#define or_is_complex_type_keyword(tok)                                       \
  || ((tok) == tok_c99_complex || (tok) == tok_c99_imaginary)
#else /* !C99_IL_EXTENSIONS_SUPPORTED */
#define or_is_complex_type_keyword(tok)  /* Nothing */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */

/*
Macro to be used in conjunction with is_type_keyword to check for C99
extensions.
*/
#define or_is_c99_type_keyword(tok)                                       \
  || ((tok) == tok_c99_bool or_is_complex_type_keyword(tok))

/*
Macro to be used in conjunction with is_type_keyword to check for fixed-point
extensions.
*/
#if FIXED_POINT_ALLOWED
#define or_is_fixed_point_type_keyword(tok)                               \
  || ((tok) == tok_accum || (tok) == tok_fract || (tok) == tok_sat)
#else /* !FIXED_POINT_ALLOWED */
#define or_is_fixed_point_type_keyword(tok)  /* Nothing */
#endif /* FIXED_POINT_ALLOWED */

/*
Macro that can be redefined by users to include checking for user-defined
type keyword extensions.  If you change this, see also type_keyword.
*/
#ifndef or_is_extension_type_keyword
#define or_is_extension_type_keyword(tok)  /* Nothing */
#endif /* or_is_extension_type_keyword */

/*
Macro that is TRUE if the indicated token is a type keyword, e.g., int.
If you change this, see also type_keyword.
*/
#define is_type_keyword(tok)                                          \
  ((tok) == tok_void     || (tok) == tok_char     ||                  \
   (tok) == tok_short    || (tok) == tok_int      ||                  \
   (tok) == tok_long     || (tok) == tok_float    ||                  \
   (tok) == tok_double   || (tok) == tok_signed   ||                  \
   (tok) == tok_unsigned || (tok) == tok_wchar_t  ||                  \
   (tok) == tok_bool                                                  \
   or_is_c99_type_keyword(tok)                                        \
   or_is_microsoft_type_keyword(tok)                                  \
   or_is_fixed_point_type_keyword(tok)                                \
   or_is_extension_type_keyword(tok)) 

/*
Macro that is TRUE if the given token is a top level visibility specifier for
C++/CLI.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_cli_assembly_visibility_specifier(tok)                        \
  (cppcli_enabled && ((tok) == tok_public || (tok) == tok_private))
#define or_is_cli_assembly_visibility_specifier(tok)                     \
  || is_cli_assembly_visibility_specifier(tok)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define or_is_cli_assembly_visibility_specifier(tok) /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Macro to be used in conjunction with is_class_type_keyword to check for
Microsoft __interface specifiers.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define or_is_interface_keyword(tok) || ((tok) == tok_interface)
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define or_is_interface_keyword(tok) /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Macro to be used in conjunction with is_class_type_keyword to check for
C++/CLI keywords that start a class type (these keywords are unusual because
they contain white space).
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define or_is_cli_class_type_keyword(tok)                                 \
  || (cppcli_enabled &&                                                   \
      ((tok) == tok_interface_class  || (tok) == tok_interface_struct ||  \
       (tok) == tok_ref_class        || (tok) == tok_ref_struct       ||  \
       (tok) == tok_value_class      || (tok) == tok_value_struct))
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define or_is_cli_class_type_keyword(tok) /* Nothing */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Macro that is TRUE if the given token can introduce an elaborated class type
specifier.
*/
#define is_class_type_keyword(tok)                                    \
  ((tok) == tok_struct || (tok) == tok_union ||                       \
   (tok) == tok_class  or_is_interface_keyword(tok)                   \
   or_is_cli_class_type_keyword(tok))

/*
Macro that is TRUE if the given token can introduce an elaborated enum type
specifier.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED
#define is_enum_type_keyword(tok)                                    \
  ((tok) == tok_enum ||                                              \
   (cppcli_enabled && ((tok) == tok_enum_class || (tok) == tok_enum_struct)))
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define is_enum_type_keyword(tok)  ((tok) == tok_enum)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
Macro that is TRUE if the current token is the start of a type
specifier (except for the typedef and friend cases).  (3.5.2)
*/
#define is_type_specifier()                                                \
 (is_type_keyword(curr_token)      || is_class_type_keyword(curr_token) || \
  is_enum_type_keyword(curr_token) || curr_token == tok_typename        || \
  curr_token == tok_typeof         || curr_token == tok_decltype        || \
  (auto_type_specifier_enabled && curr_token == tok_auto)                  \
  or_is_cli_assembly_visibility_specifier(curr_token))

#if NAMED_ADDRESS_SPACES_ALLOWED
extern a_boolean curr_id_is_named_address_space(void);
#define or_is_named_address_space_qualifier()                                \
  || (named_address_spaces_enabled && curr_token == tok_identifier &&        \
      curr_id_is_named_address_space())
#else /* !NAMED_ADDRESS_SPACES_ALLOWED */
#define or_is_named_address_space_qualifier()  /* Nothing */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */

/*
Macro that is TRUE if the current token is the start of a type qualifier
(3.5.3).
*/
#define is_type_qualifier()                                                  \
  (is_type_qualifier_token(curr_token) or_is_named_address_space_qualifier())

/*
Macro that is TRUE if the current token denotes a link scope specifier.
*/
#if SUN_EXTENSIONS_ALLOWED
#define is_sun_link_scope_specifier()                                 \
  (curr_token == tok_global_link_scope ||                             \
   curr_token == tok_symbolic_link_scope ||                           \
   curr_token == tok_hidden_link_scope)
#endif /* SUN_EXTENSIONS_ALLOWED */


/*
If type_ptr is float, change it to double.  Used in pcc mode to promote
function parameter and return types.
*/
#define promote_float_to_double(type_ptr)                             \
{ if (is_floating_type(type_ptr) &&                                   \
      skip_typerefs(type_ptr)->variant.float_kind ==                  \
                                            (a_float_kind)fk_float) { \
    type_ptr = float_type((a_float_kind)fk_double);                   \
  }  /* if */                                                         \
}  /* promote_float_to_double */


/* Bit vector used to pass flags into declarator and into and out of
   declaration routines.  Each bit represents a flag.  (Note that several of
   the bit sets described with this type have more than 16 flags; "unsigned
   long" is used to assure that there is no overflow problem in environments
   where "unsigned int" would be too small.) */
typedef unsigned long a_decl_flag_set;


/*
Structure used to hold a list of pre-scanned expressions so they
can be retrieved and scanned later.  Used, for example, for the expression
in an "auto" declaration, so it can be scanned, examined for its type, and
then put in a cache so it will be picked up again later as if scanned from
source at that point.
*/
typedef struct an_arg_operand an_arg_operand_dummy_typedef;
typedef struct an_expression_cache {
  an_arg_operand_ptr
		first_expression,
		last_expression;
			/* First and last expressions on a list of
			   expressions in the cache. */
} an_expression_cache;

/* Macro to test whether an expression cache contains something. */
#define anything_cached(pc) ((pc)->first_expression != NULL)


/*
Forward declaration of a structure used to pass around information about a
template being declared.
*/
typedef struct a_tmpl_decl_state *a_tmpl_decl_state_ptr;

/*
Forward declaration of a structure to track information while parsing a
declaration (definition is below).
*/
typedef struct a_decl_parse_state *a_decl_parse_state_ptr;

/*
Type of callback functions to call at end of declaration processing.
*/
typedef void a_decl_parse_callback_function(a_decl_parse_state_ptr);

/*
A structure describing an action to be taken during declaration parsing.
Actions can be chained (currently, the actions happen at the end of
declaration processing).
*/
typedef struct a_decl_parse_callback *a_decl_parse_callback_ptr;
typedef struct a_decl_parse_callback {
  a_decl_parse_callback_ptr
		next;
			/* Next callback in a list. */
  a_decl_parse_callback_function
		*callback_fn;
			/* Pointer to function to call. */
} a_decl_parse_callback;


/*
A structure to carry state information through the declaration parsing process.
*/
typedef struct a_decl_parse_state {
  a_symbol_ptr
		sym;
			/* A symbol representing the entity being declared. */
  a_decl_flag_set
		dso_flags;
			/* The flags returned by the call to
			   decl_specifiers. */
  a_decl_flag_set
		do_flags;
			/* The flags returned by the call to declarator. */
  a_source_position
		start_pos;
			/* The first position of the current declaration. */
  a_source_position
		specifiers_pos;
			/* The position of the current token when
			   decl_specifiers is called. */
  a_source_position
		declarator_start_pos;
			/* The position of the current token when "declarator"
			   is called, or the null position if there is no
			   declarator.  (For lambdas, if there is no
                           "declarator", this is the position of the start of
			   the lambda body.) */
  a_source_position
		declarator_pos;
			/* The position of the declarator-id if there is one.
			   Otherwise, same as declarator_start_pos. */
  a_source_position
		return_type_pos;
			/* When the declarator is a function declarator, this
			   is the position of the return type (otherwise, it is
			   the null position).  When has_trailing_return_type
			   is TRUE, it is the position of the trailing return
			   type; otherwise, it is the same as
			   specifiers_pos. */
  a_type_qualifier_set
		qualifiers;
			/* Top-level type qualifiers (but not function type
                           qualifiers). */
  a_source_position
		qualifiers_pos;
			/* The position of the type qualifiers (except for
                           the "restrict" qualifiers). */
  a_source_position
		restrict_pos;
			/* The position of the "restrict" qualifier (if
			   any). */
  a_source_position
		inline_pos;
			/* The position of the "inline" specifier (if any). */
  a_source_position
		virtual_pos;
			/* The position of the "virtual" specifier (if any). */
  a_source_position
		auto_pos;
			/* The position of the "auto" specifier (if any). */
  a_bit_field
		in_class_scope:1;
			/* TRUE if the current declaration appears in class
			   scope. */
  a_bit_field
		secondary_declarator:1;
			/* TRUE if the current declaration corresponds to a
			   secondary declarator (e.g., "y" in "int x, y;"). */
  a_bit_field
		is_definition:1;
			/* TRUE if the current declaration defines a variable
			   or function. */
  a_bit_field
		in_nested_declarator:1;
			/* TRUE while parsing a nested declarator. */
  a_bit_field
		is_trailing_return_type:1;
			/* TRUE if this information block describes the parsing
			   of a trailing return type (this is set before
			   parsing that type with a call to type_name_full). */
  a_bit_field
		is_type_name:1;
			/* TRUE if this information block is one created for a
			   call to type_name_full. */
  a_bit_field
		is_template_type_argument:1;
			/* TRUE if this information block is one created for a
			   call to scan_template_type_argument (which in turn
			   calls type_name_full; so is_type_name will also be
			   TRUE in that case). */
  a_bit_field
		trailing_return_type_allowed:1;
			/* TRUE if the current context allows a trailing
			   return type. */
  a_bit_field
		has_trailing_return_type:1;
			/* TRUE if the current declaration has a trailing
			   return type (this is set after the trailing return
			   type has been parsed). */
  a_bit_field
		pack_ellipsis_allowed:1;
			/* TRUE if the current declaration can declare a
			   parameter pack. */
  a_bit_field
		has_pack_ellipsis:1;
			/* TRUE if the current declaration includes an
			   ellipsis indicating that a parameter pack is
			   being declared. */
  a_bit_field	is_pack_element:1;
			/* TRUE during a real instantiation if the current
			   declaration is a parameter that is a pack
			   element. */
  a_bit_field
		is_new_expr_type:1;
			/* TRUE if this information block describes the parsing
			   of a type for a "new-expression". */
  a_bit_field
		is_evaluated_sizeof_type_arg:1;
			/* TRUE if this information block describes the parsing
			   of a type that is the argument to a sizeof operator
			   in an evaluated expression context (e.g., in
			   "__alignof(sizeof(int[n]))" the flag is FALSE while
			   parsing "int[n]" because the sizeof operator is not
			   itself evaluated). */
  a_bit_field
		disallow_variably_modified_type:1;
			/* TRUE if this information block describes the parsing
			   of a type in which variably modified types are not
			   allowed. */
  a_bit_field
		nested_ptr_or_ref_seen:1;
			/* TRUE during declarator processing if we've seen a
			   nested "*", "&", or "&&" declarator operator.  This
			   means in particular that a subsequent array
			   declarator will not be the top-level array
			   component of a variable-length array (although it
			   could contribute to creating a "variably modified
			   type"). */
  a_bit_field
		unused_qualifiers:1;
			/* TRUE if there are pending qualifiers that have
			   not had an effect on the current declaration.
			   ("An effect" may be a diagnostic.) */
  a_bit_field
		auto_type_allowed:1;
			/* TRUE in a declarative context in which "auto" may
			   appear as a type specifier. */
  a_bit_field
		auto_type_specifier_seen:1;
			/* TRUE if "auto" appeared as a type specifier. */
  a_bit_field
		is_asm_function:1;
			/* TRUE if the current declaration is for an asm
			   function.  (An extension available only when
			   ASM_FUNCTION_ALLOWED is TRUE.) */
  a_bit_field
		function_definition_allowed:1;
			/* TRUE if the declaration context allows a function
			   definition. */
  a_bit_field
		is_old_style_param_decl:1;
			/* TRUE if the current declaration is an old-style C
			   parameter declaration. */
  a_bit_field
		is_top_level_declaration:1;
			/* TRUE if the current declaration appears at file
			   scope and is not part of any other declarative
			   structure.  (This is significant for precompiled-
			   header processing.) */
  a_bit_field
		is_linkage_spec_decl:1;
			/* TRUE if the current declaration has a linkage
			   specifier (like extern "C") attached directly to
			   it (as opposed to just being inside a linkage
			   block). */
  a_bit_field
		marked_as_gnu_extension:1;
			/* TRUE if this declaration started with the GNU
			   keyword __extension__. */
  a_bit_field
		decl_specifiers_omitted:1;
			/* TRUE if the current declaration omits both linkage
			   specifiers and decl-specifiers. */
  a_bit_field
		decl_specifiers_error:1;
			/* TRUE if an error occurred during the call to
			   "decl_specifiers". */
  a_bit_field
		need_semicolon_remove_stop_token:1;
			/* tok_semicolon is a stop token that still needs to be
			   removed from the stop token set. */
  a_bit_field
		need_comma_remove_stop_token:1;
			/* tok_comma is a stop token that still needs to be
			   removed from the stop token set. */
  a_bit_field
		need_assign_remove_stop_token:1;
			/* tok_assign is a stop token that still needs to be
			   removed from the stop token set. */
  a_bit_field
		need_lbrace_remove_stop_token:1;
			/* tok_lbrace is a stop token that still needs to be
			   removed from the stop token set. */
  a_bit_field
		restore_name_linkage:1;
			/* TRUE if pop_name_linkage must be called at the end
			   of the processing for this declaration. */
  a_bit_field	has_initializer:1;
			/* TRUE if this is a variable or data member
			   declaration that includes an initializer. */
  a_bit_field	first_decl:1;
			/* TRUE if this is the first declaration of a variable
			   or function. */
  a_bit_field	first_decl_of_predeclared_entity:1;
			/* TRUE if this is the first declaration of a variable
			   or function that was predeclared by the front
			   end. */
  a_bit_field	is_property_or_event_field:1;
			/* TRUE if this is a field declaration with the
			   Microsoft __declspec(property(...)) specifier or
			   a non-static C++/CLI property or event
			   declaration. */
  a_bit_field	is_declspec_property_field:1;
			/* TRUE if this is a field declaration with the
			   Microsoft __declspec(property(...)) specifier. */
  a_bit_field	has_cli_context_sensitive_keyword:1;
			/* TRUE if a C++/CLI context-sensitive keyword was
			   seen among the specifiers. */
  a_bit_field	has_cli_property_keyword:1;
			/* TRUE if a C++/CLI context-sensitive keyword
			   "property" was seen in a member declaration. */
  a_bit_field	has_cli_event_keyword:1;
			/* TRUE if a C++/CLI context-sensitive keyword "event"
			   was seen in a member declaration. */
  a_bit_field	has_cli_initonly_keyword:1;
			/* TRUE if a C++/CLI context-sensitive keyword
			   "initonly" was seen in a member declaration. */
  a_bit_field	has_cli_literal_keyword:1;
			/* TRUE if a C++/CLI context-sensitive keyword
			   "literal" was seen in a member declaration. */
  a_bit_field	override_okay:1;
			/* TRUE if this is a member function on which the
			   "override" attribute (C++0x) attribute or modifier
			   (Microsoft) can be specified. */
  a_bit_field	initializer_is_expr_list:1;
			/* TRUE if the part of the entity's initializer being
			   scanned now is an element on an expression list.
			   If so, variadic template pack expansions are
			   allowed. */
  a_bit_field	initializer_is_single_expr:1;
			/* Used in conjunction with initializer_is_expr_list
			   TRUE to indicate a context where, though the
			   initializer is syntactically an expression list,
			   in actuality there must be a single expression. */
  a_bit_field	no_special_cli_class_type_check:1;
			/* Some special C++/CLI class types (like cli::array
			   instances) can ordinarily not be used as top-level
			   types; only as types "pointed to" by a handle.  If
			   this flag is TRUE, the generic diagnostic for the
			   top-level uses of such special types is inhibited
			   (e.g., because higher-level code will issue a more
			   specific diagnostic). */
  an_attribute_ptr
		prefix_attributes;
			/* A list of non-type-transforming attributes scanned
			   at the start of the declaration. */
  an_attribute_ptr
		id_attributes;
			/* A list of attributes scanned right after the
			   declarator-id. */
  an_attribute_ptr
		specifier_attributes;
			/* A list of attributes scanned as part of the
			   declaration specifiers. */
  an_attribute_ptr
		tag_attributes;
			/* If the specifiers contain an elaborated tag name
			   (e.g., "enum [[align(4)]] X"), the list of
			   attributes specified in that tag reference. */
  a_decl_modifiers_block
		decl_modifiers;
			/* Extended declaration information (most of it
			  related to Microsoft extensions). */
  an_ms_attribute_ptr 
		ms_attributes;
			/* A list of Microsoft attributes scanned for the
			   current declaration. */
  char
		*asm_name;
			/* The string specified by a GNU asm name construct. */
  a_source_position
		asm_name_pos;
			/* The position of the string literal specified by a
			   GNU asm name construct (if any). */
  a_named_register_id
		register_id;
			/* An integer representing a named register storage
			   class (zero if no named register was specified). */
  a_source_position
		storage_class_pos;
			/* The position of any explicitly specified storage
			   class. */
  a_storage_class
		declared_storage_class;
			/* The storage class as it appears in the source. */
  a_storage_class
		storage_class;
			/* The storage class as adjusted for the nature of the
			   declaration (including possible error recovery
			   decisions). */
  a_type_ptr
		specifiers_type;
			/* The type returned by the call to decl_specifiers. */
  a_type_ptr
		declared_type;
			/* The type as it appears in the source (updated by
			   calls to decl_specifier and declarator). */
  a_type_ptr
		type;
			/* The type of the entity being declared. */
  a_type_ptr
		prev_type;
			/* If the current declaration is a redeclaration, the
			   type previously recorded for the declared entity. */
  a_type_ptr
		auto_type;
			/* The tk_template_param type used to represent the
			   "auto" type specifier (if any). */
  a_type_ptr
		deduced_auto_type;
			/* The type that "auto" was deduced to after scanning
			   the initializer. */
  an_expression_cache
		prescanned_initializer_cache;
			/* A cache containing an expression scanned early
			   to deduce the type of the "auto" type specifier,
			   or expressions generated by a variadic template
			   pack expansion, cached for later consumption
			   before scanning more from source. */
  unsigned long
		prescanned_initializer_levels_down;
			/* When initializing an aggregate, we sometimes
			   determine that the expression up next initializes
			   the first member of the current aggregate, or the
			   first member of its first member, etc.  If non-zero,
			   this indicates that the first expression in
			   prescanned_initializer_cache initializes the member
			   that many levels down. */
  a_source_sequence_entry_ptr
		source_sequence_entry;
			/* The source sequence entry created for the declarator
			   (if any). */
  a_param_id_ptr
		param_id;
			/* A description of the parameter declared using an
			   old-style C parameter definition.  May be NULL even
			   when is_old_style_param_decl is TRUE in error
			   cases. */
  a_targ_alignment
		alignment;
			/* The explicit alignment specified for the declared
			   entity in this declaration, or zero if no alignment
			   has been specified explicitly. */
  a_upc_block_size
		upc_block_size;
			/* The UPC block size associated with any UPC shared
			   qualifier (or UPC_BLOCK_SIZE_NONE if there is no
			   such qualifier). */
  an_il_entity_list_entry_ptr
		*p_postfix_entities;
			/* While parsing a declaration statement (stmk_decl),
			   this pointer keeps track of associated entities
			   that appeared after a declarator-id for which no
			   associated entry has been created yet. */
  a_decl_parse_state_ptr
		assoc_func_decl_state;
			/* For a non-old-style parameter declaration, this
			   points to the parse state of the function declarator
			   for which the parameter is being parsed.  Otherwise,
			   NULL. */
  a_decl_parse_callback_ptr
		end_of_parse_actions;
			/* A list of functions to call at the end of
			   declaration processing.  Adding items to this list
			   is handy when a feature is encountered with a
			   constraint that cannot be checked until the whole
			   declaration has been processed. */
} a_decl_parse_state;


EXTERN a_decl_parse_state
		null_decl_parse_state;
			/* Null "parse state".  Used for initialization by the
			   macro init_decl_parse_state macro. */

/*
Macro to initialize the "declaration parsing state" pointed to by the
argument.
*/
#define init_decl_parse_state(ps) {                                          \
  *(ps) = null_decl_parse_state;                                             \
  (ps)->start_pos = pos_curr_token;                                          \
}

/*
Macro to record in a parsing state that a type error was encountered.
*/
#define invalidate_type(ps)                                                  \
  ((ps)->type = (ps)->declared_type = (ps)->specifiers_type = error_type())

extern void add_end_of_parse_action(a_decl_parse_callback_function  *fn,
                                    a_decl_parse_state              *dps);

extern void run_end_of_parse_actions(a_decl_parse_state  *dps);

extern void attach_parse_state_to_attributes(a_decl_parse_state  *dps);

extern void detach_parse_state_from_attributes(a_decl_parse_state  *dps);

extern void attach_decl_attributes(a_decl_parse_state  *dps,
                                   a_boolean           primary_decl);

extern void attach_param_attributes(a_decl_parse_state  *dps,
                                    a_param_type_ptr    ptp);

extern void check_prefix_attributes_without_a_declarator(
                                                    a_decl_parse_state  *dps);

extern void start_secondary_declarator(a_decl_parse_state  *ps);

extern void check_deduced_auto_type(a_decl_parse_state  *dps);

extern void check_use_of_auto_type(a_decl_parse_state  *dps);

/*
Macro to discard the source sequence entry associated with the declarator.
(The first argument is a pointer to the current a_decl_parse_state block and
the second argument determines the scope stack entry associated with the
source sequence entry.)
*/
#if GENERATE_SOURCE_SEQUENCE_LISTS

#define remove_declarator_sse(dps, scope_level)                              \
  if ((dps)->source_sequence_entry != NULL) {                                \
    f_remove_from_src_seq_list((dps)->source_sequence_entry, scope_level);   \
    (dps)->source_sequence_entry = NULL;                                     \
  }  /* if */

extern void mark_decl_after_first_in_comma_list(a_decl_parse_state*);

extern void wrapup_sse_for_simple_decl(a_decl_parse_state  *dps);

#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */

#define remove_declarator_sse(dps, scope_level)  /* Nothing */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern void f_check_pending_qualifiers_used(a_decl_parse_state  *ps);

#define check_pending_qualifiers_used(ps)                                    \
  if ((ps)->unused_qualifiers) {                                             \
    f_check_pending_qualifiers_used((ps));                                   \
  }  /* if */


extern a_boolean reconcile_static_data_member_types(
					a_symbol_ptr		sym,
					a_type_ptr		type_ptr,
					a_source_position_ptr	err_pos);

extern a_boolean simplify_curr_class_qualified_name(void);

extern void report_missing_type_specifier(
                                     a_source_position  *err_pos,
                                     a_type_ptr         type,
                                     a_boolean          is_function,
                                     a_boolean          is_function_def,
                                     a_boolean          is_main_function,
                                     a_boolean          any_decl_specifiers);

/*
Report use of "implicit int" in a declaration.  This macro is not called for
function declarations (call report_missing_type_specifier directly) or
when all decl-specifiers are missing.
*/
#define report_implicit_int(pos, type)                                \
  report_missing_type_specifier((pos), (type),                        \
                                /*is_function=*/FALSE,                \
                                /*is_function_def=*/FALSE,            \
                                /*is_main_function=*/FALSE,           \
                                /*any_decl_specifiers=*/TRUE)


extern void type_name_full(a_decl_parse_state  *dps);

extern void check_type_definition_in_type_name(a_decl_parse_state  *dps);

extern void type_name(a_type_ptr  *type);

extern a_type_ptr scan_type_for_cast(a_boolean  const_expr_context,
                                     a_boolean  *explicit_cv_qualifiers,
                                     a_boolean  *type_definition);

extern a_type_ptr scan_type_for_sizeof(a_boolean  evaluated_context);

extern a_type_ptr scan_template_type_argument(void);

extern void new_type_name(a_decl_parse_state  *state,
                          a_boolean           is_parenthesized);

#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
extern a_type_ptr simple_type_specifier_sequence(void);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */

extern
a_boolean scan_conversion_operator(
			a_source_position		*id_pos,
                        a_boolean			is_class_member,
                        a_parent_class_or_namespace_ptr	parent,
			a_type_ptr			field_sel_type);

extern a_type_ptr type_keyword(void);

extern void adjust_parameter_type(a_type_ptr           *type_ptr);

extern a_boolean is_single_param_operator_new_or_delete(
                                                   a_symbol_locator *locator,
                                                   a_type_ptr       type);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern a_boolean valid_static_conversion_class_type(a_type_ptr  tp,
                                                    a_type_ptr  class_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void check_operator_function_params(a_type_ptr        rout_type,
                                           a_type_ptr        class_type,
                                           a_symbol_locator  *locator);

extern void check_and_adjust_parameter_type(a_type_ptr           *type_ptr,
                                            a_source_position    *error_pos);

void check_old_specialization_allowed(a_symbol_ptr       sym,
                                      a_source_position  *pos);

extern void decl_default_function(a_symbol_ptr symbol_ptr);

extern a_label_ptr scan_label(a_boolean  is_definition,
                              a_boolean  is_declaration);

extern void check_main_function(a_func_info_block_ptr  func_info,
                                a_type_ptr             type,
                                a_storage_class        *declared_storage_class,
                                a_boolean              *is_inline,
                                a_source_position_ptr  pos);

#if GNU_EXTENSIONS_ALLOWED
extern void scan_gnu_asm_name(a_decl_parse_state  *dps);

extern void scan_gnu_declarator_attributes(a_decl_parse_state  *dps);

extern
void gnu_attributes_after_parenthesized_initializer(a_variable_ptr      var,
                                                    a_decl_parse_state  *dps);

extern void report_gnu_postfix_attributes_on_function_definition(
                                                    a_decl_parse_state  *dps);
#endif /* GNU_EXTENSIONS_ALLOWED */

#if GENERATE_SOURCE_SEQUENCE_LISTS
extern void add_src_seq_end_of_variable_if_needed(a_decl_parse_state  *dps);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern void declaration(a_boolean       function_definition_allowed,
                        a_boolean       is_old_style_param_decl,
                        a_boolean       is_top_level_declaration,
                        a_boolean       marked_as_gnu_extension,
                        a_param_id_ptr  param_id_list,
                        a_source_range  *linkage_spec_range_ptr);

extern void local_declaration(a_boolean  marked_as_gnu_extension);

extern void translation_unit(void);

#if INSTANTIATION_BY_IMPLICIT_INCLUSION
extern void scan_implicitly_included_template_definition_file(void);
#endif /* INSTANTIATION_BY_IMPLICIT_INCLUSION */

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void scan_top_level_metadata_declarations(char* buffer);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_boolean reconcile_external_symbol_types(
                            a_symbol_ptr          ext_sym,
                            a_source_position_ptr position,
                            a_type_ptr            type_ptr,
                            an_error_severity     incompatible_severity);

extern a_routine_ptr make_routine(a_type_ptr      type_ptr,
                                  a_storage_class storage_class,
                                  a_scope_depth   scope_depth);

extern a_using_decl_ptr make_using_decl(a_symbol_ptr      sym,
                                        a_source_position *pos,
					a_scope_depth	  scope_depth);

extern a_symbol_ptr enter_local_symbol(a_symbol_kind    kind,
                                       a_symbol_locator *locator,
                                       a_scope_depth    scope_level,
                                       a_boolean        suppress_redecl_error);

extern void decl_typedef(a_symbol_locator             *locator,
                         a_decl_parse_state           *state,
                         a_type_ptr                   class_type,
                         a_decl_pos_block_ptr         decl_pos_block);

extern void alias_declaration(a_decl_parse_state  *dps,
                              a_source_position   *p_end_of_using_pos);

extern void record_lint_argsused_and_varargs_state(a_symbol_ptr  rout_sym);

extern void record_arg_pragma(a_pending_pragma_ptr  ppp,
                              a_symbol_ptr          sym,
                              a_statement_ptr       sp);

extern void reconcile_routine_types(a_routine_ptr       routine_ptr,
                                    a_type_ptr          type_ptr,
                                    a_boolean           preserve_rout_type,
                                    a_boolean           preserve_type_ptr,
                                    a_decl_parse_state  *dps);

extern
void check_constituent_types_have_linkage(a_symbol_ptr      sym,
                                          a_source_position *error_pos,
                                          a_boolean         is_declaration);

extern void check_exception_specification(a_type_ptr         new_rout_type,
                                          a_symbol_ptr       prev_decl,
                                          a_source_position  *throw_pos,
                                          a_boolean          is_redecl);

#if GENERATE_SOURCE_SEQUENCE_LISTS

extern a_type_ptr update_routine_declared_type(a_type_ptr  rout_type,
                                               a_type_ptr  declared_type);

extern void set_routine_declared_type(a_routine_ptr  routine_ptr,
                                      a_type_ptr     declared_type);

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

extern void decl_routine(a_symbol_locator         *locator,
                         a_decl_parse_state       *dps,
                         a_func_info_block_ptr    func_info,
                         a_symbol_reference_kind  srk_flags,
                         an_id_linkage_kind       *linkage_ptr,
                         a_type_ptr               *old_type,
                         a_symbol_ptr             *ext_sym,
                         a_decl_pos_block_ptr     decl_pos_block);

void decl_variable(a_symbol_locator             *locator,
                   a_decl_parse_state           *dps,
                   a_symbol_reference_kind      srk_flags,
                   an_id_linkage_kind           *linkage_ptr,
                   a_symbol_ptr                 *ext_sym,
                   a_decl_pos_block_ptr         decl_pos_block);

extern void decl_function_template(a_symbol_locator            *locator,
                                   a_func_info_block           *func_info,
                                   a_symbol_ptr                *symbol_ptr,
                                   a_tmpl_decl_state_ptr       decl_state);

extern void handler_declaration(a_statement_ptr     sp,
                                a_source_position*  catch_pos,
				a_boolean	    is_function_try_block);

extern an_asm_entry_ptr asm_declaration(a_boolean         asm_decl_allowed,
                                        a_boolean         is_asm_statement,
                                        an_attribute_ptr  *p_attributes);

extern a_variable_ptr condition_declaration(void);

#if MICROSOFT_EXTENSIONS_ALLOWED
extern void for_each_iterator_declaration(a_statement_ptr sp);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern void static_assert_declaration(a_boolean  leave_semicolon);

extern void make_using_directive(a_namespace_ptr    nsp,
				 a_scope_depth	    depth,
                                 a_source_position  *pos,
		   	         a_boolean	    compiler_generated,
				 an_attribute_ptr   attributes); 

#if DECL_MODIFIERS_IN_USE
#if MICROSOFT_EXTENSIONS_ALLOWED
extern void update_dll_info_for_routine(a_routine_ptr         routine,
                                        a_decl_modifier       flags,
                                        a_boolean             is_inline,
                                        a_boolean             is_redecl,
                                        a_boolean             is_definition,
                                        a_source_position     *err_pos);

extern void update_dll_info_for_variable(a_variable_ptr       var,
                                         a_decl_modifier       flags,
                                         a_boolean             is_redecl,
                                         a_boolean             is_definition,
                                         a_source_position     *err_pos);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern
void update_routine_decl_modifiers(a_routine_ptr               routine,
                                   a_decl_modifiers_block_ptr  new_modifiers,
                                   a_source_position           *position,
                                   a_boolean                   is_redecl,
                                   a_boolean                   is_definition,
                                   a_boolean                   is_inline);

extern
void update_variable_decl_modifiers(a_decl_parse_state  *dps);
#else /* !DECL_MODIFIERS_IN_USE */
/* Define these as macros that expand to nothing. */
#define update_routine_decl_modifiers(a,b,c,d,e,f) /* nothing */
#define update_variable_decl_modifiers(a) /* nothing */
#endif /* !DECL_MODIFIERS_IN_USE */

extern void check_default_args_for_param_type(a_param_type_ptr  ptp,
                                              a_source_position *pos);

extern void check_nonfunction_declaration_errors(a_decl_parse_state  *state,
                                                 a_symbol_locator    *locator);

#if USER_CONTROL_OF_STRUCT_PACKING
extern void record_std_alignment_attr(a_decl_parse_state_ptr  dps);
#endif /* USER_CONTROL_OF_STRUCT_PACKING */

extern void decls_one_time_init(void);

extern void decls_trans_unit_init(void);

extern void decls_init(void);

#if DEBUG
unsigned long show_decl_space_used(void);
#endif /* DEBUG */

#endif /* DECLS_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
