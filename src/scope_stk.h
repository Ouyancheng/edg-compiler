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

scope_stk.h - Declarations related to management of the scope stack and
              related routines.

*/

/* Avoid including these declarations more than once. */
#ifndef SCOPE_STK_H
#define SCOPE_STK_H 1

/*
Forward declarations needed:
*/
typedef struct an_active_using_directive *an_active_using_directive_ptr;
typedef struct an_expr_stack_entry an_expr_stack_entry_dummy_typedef;
typedef struct a_class_def_state a_class_def_state_dummy_typedef;
typedef struct a_tmpl_decl_state a_tmpl_decl_state_dummy_typedef;

/*
Option flags passed to the push_scope routines.
*/
typedef int a_push_scope_options_set;
#define PS_NO_OPTIONS			0x00
#define PS_MICROSOFT_SPECIALIZATION	0x01
			/* The scope being pushed is a template instantiation
			   scope that is pushed around a class or class
			   reactivation scope in Microsoft mode to make the
			   template parameters visible. */
#define PS_PROTOTYPE_INSTANTIATION	0x02
			/* The scope being pushed is the template instantiation
			   scope for a prototype instantiation. */
#define PS_NONREAL_INSTANTIATION	0x04
			/* The scope being pushed in a template instantiation
			   in which the template arguments are template
			   dependent.  Only used for certain default template
			   argument cases. */
#define PS_IS_REACTIVATION		0x08
			/* TRUE to indicate that a file scope is being
			   reactivated. */

/*
Structure that is logically (and historically) part of a_scope_stack_entry,
but which must persist longer than a scope stack entry for namespace scopes
(since "extension-definitions" are allowed for them).  Therefore,
a_scope_pointers_block is also part of a_namespace_symbol_supplement.  When
an sck_namespace or sck_namespace_extension scope is pushed onto the stack, a
pointer in the scope stack entry is set to refer to the persistent scope
pointers block (the one in the symbol supplement) -- and the one in the scope
stack entry itself is unused.
*/
typedef struct a_scope_pointers_block *a_scope_pointers_block_ptr;
typedef struct a_scope_pointers_block {
  a_symbol_ptr	symbols;
			/* Pointer to the head of a linked list of all symbols
			   declared in this scope (linked by the field
			   next_in_scope); NULL if there are no such
			   declarations. */
  a_symbol_ptr	synth_namespace_projection_symbols;
			/* Pointer to the head of a list of synthesized
			   projection symbols created in this scope.
			   These are linked by the next_in_scope field in
                           the symbol. */
  a_symbol_ptr	last_symbol;
			/* End of the symbol list pointed to by symbols. */
  a_constant_ptr
		last_constant;
			/* End of list of named constants of this scope,
			   NULL if none. */
  a_type_ptr	last_type;
			/* End of list of local types of this scope, NULL if
			   none. */
  a_variable_ptr
		last_variable;
			/* End of list of local variables of this scope, NULL
			   if none. */
  a_routine_ptr	last_routine;
			/* End of list of local routines of this scope, NULL
			   if none.  Includes both routines with definitions
			   and those that are just declarations of interfaces
			   to external routines. */
  an_asm_entry_ptr
		last_asm_entry;
			/* End of list of asm entries of this scope, NULL if
			   none. */
  a_dynamic_init_ptr
		last_dynamic_init;
			/* End of list of dynamic initializations for this
			   scope, NULL if none.  Only the file scope has
			   a dynamic initializations list. */
  a_namespace_ptr
		last_namespace;
			/* End of list of namespace entries in this scope,
			   NULL if there are none. */
  a_using_decl_ptr
		last_using_decl;
			/* End of list of using-decl entries in this scope;
			   NULL if there are none. */
  a_pragma_ptr	last_pragma;
			/* End of list of IL pragma entries entered on the
			   pragma_list of il_scope, NULL if none. */
#if RECORD_HIDDEN_NAMES_IN_IL
  a_hidden_name_ptr
		last_hidden_name;
			/* End of the list of hidden-name entries entered on
			   the corresponding IL scope entry; NULL if none. */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
  a_template_ptr
		last_template;
			/* End of the list of template entries entered on
			   the corresponding IL scope entry; NULL if none. */
  a_symbol_ptr	unnamed_namespace_sym;
			/* For sck_file and sck_namespace scopes only, pointer
			   to the symbol representing the unnamed namespace
			   for the current scope; NULL if there is none. */
  a_bit_field	add_symbols_to_inactive_list:1;
			/* TRUE for sck_namespace_reactivation scopes if
			   symbols added to the scope should be added
			   directly to the inactive list, instead of being
			   added to the active list as is usually done. */
  bitfield_to_avoid_codecenter_warnings()
} a_scope_pointers_block;


/*
Entry identifying a symbol that is visible in a given scope but would not
be if old (cfront-compatible) for-init declaration scoping rules were used.
*/
typedef struct a_name_hidden_by_old_for_init
                                     *a_name_hidden_by_old_for_init_ptr;
typedef struct a_name_hidden_by_old_for_init {
  a_name_hidden_by_old_for_init_ptr
		next;
			/* Next in a list of name_hidden_by_old_for_init
			   entries for a given scope; NULL for the last in
			   the list. */
  a_symbol_ptr	symbol;
			/* Pointer to a symbol (from an enclosing scope) for
			   which hidden_by_old_for_init is TRUE. */
  a_symbol_ptr	for_init_decl_sym;
			/* Pointer to a symbol declared a for-init declaration.
			   It has the same name as the other symbol, and under
			   the old rules would have hidden it for the rest of
			   the current scope. */
  a_byte_boolean
		already_hidden;
			/* Value to which hidden_by_old_for_init in symbol
			   should be restored when the current scope is
			   popped. */
} a_name_hidden_by_old_for_init;


/* A set of pointers to constants that might be generated in a scope. */
typedef struct a_generated_entity_block *a_generated_entity_block_ptr;
typedef struct a_generated_entity_block {
  a_variable_ptr
		function_name;
			/* Pointer to a constant string variable holding the
			   name of the function currently being defined.
			   Set only when the appropriate reserved identifier
			   (e.g., __func__) is used in Microsoft or C99 mode;
			   otherwise NULL. */
  a_variable_ptr
		decorated_function_name;
			/* Same as "function_name", but the string variable
			   entry holds the mangled name of the function
			   currently being defined (Microsoft mode only). */
} a_generated_entity_block;


/* Scope stack, containing an entry for each currently-active scope. */
typedef struct a_scope_stack_entry *a_scope_stack_entry_ptr;
typedef struct a_scope_stack_entry {
  a_scope_number
		number;
			/* Scope number (unique identifier) for this scope. */
  a_scope_kind	kind;
			/* Kind of scope (file, function, block, function
			   prototype, etc.).  See the definition of
			   a_scope_kind in il_def.h. */
  a_bit_field /*an_access_specifier*/
		current_access:2;
			/* The access control specification that currently
			   prevails for declarations in the current scope;
			   as_public by default, but may be otherwise for
			   C++ class definitions.  (For instance, if an
                           enumeration is defined as a member type of a class,
			   the access to be applied to the enumeration
			   constants may be derived from the setting of this
			   field.) */
  a_bit_field	inactive_symbols_may_be_visible:1;
			/* TRUE if the scope stack to this depth contains any
			   class reactivation entries or class entries for
			   classes with base classes.  In either case,
			   symbols on a symbol header's inactive list may be
			   visible from the current scope. */
  a_bit_field	inside_local_class:1;
			/* TRUE if the current scope level is that of a local
			   class or is (logically) within the scope of a local
			   class.  Once this flag is set it is usually
			   propagated each time a new scope is pushed onto
			   the stack; the exception is when a template
			   instantiation scope is pushed, in which case the
			   flag is cleared. */
  a_bit_field	template_param_decl_scope:1;
			/* TRUE if this is the first scope that
			   affects the declarative level after a template
			   instantiation scope. */
  a_bit_field	is_loop_scope:1;
			/* TRUE if this scope is associated with the compound
			   statement of a for, do, or while loop. */
  a_bit_field	slow_lookup_required:1;
			/* TRUE if this is a scope for which a slow lookup
			   is required because the scope stack contains a
			   scope in which certain symbols on the active list
			   must not be visible. */
  a_bit_field	return_value_optimization_possible:1;
			/* TRUE if this scope is a function scope and return
			   value optimization is possible for the routine.
			   That is, the routine returns a class value via
			   a copy constructor, and all return statements
			   return a single local variable. */
  a_bit_field	in_prototype_instantiation:1;
			/* TRUE if kind is sck_template_instantiation and
			   what is being instantiated is the prototype for a
			   class template.  Also true for scopes nested within
			   a prototype instantiation. */
  a_bit_field	in_nonreal_instantiation:1;
			/* TRUE for instantiations based on template dependent
			   template arguments.  This is only true for certain
			   default template argument cases. */
  a_bit_field	defer_access_checks:1;
			/* TRUE while scanning the decl-specifiers and
			   declarator of a global or namespace-level
                           declaration.  Access checks for names
			   scanned while this is TRUE cannot be done
			   until the declarator has been scanned. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_bit_field	source_sequence_entries_disallowed:1;
			/* TRUE if the current scope establishes or belongs to
			   a context in which source sequence entries should
			   not be issued -- e.g. a template declaration, a
			   a template instantiation, or a pragma. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_bit_field	nested_instantiation:1;
                        /* TRUE for a template instantiation scope that
			   is expected to be nested inside of another
			   instantiation scope.  This occurs when a friend
			   template declaration from a class template is
			   being instantiated.  This flag lets name lookups
			   continue on past the nested instantiation scope so
			   that names from the outer instantiation scope can
			   be visible. */
  a_bit_field	is_try_block:1;
			/* TRUE if the scope is that of the compound statement
			   of a try block (sck_block only).  Note: not set
			   for the scope pushed for a catch clause. */
  a_bit_field	within_try_block:1;
			/* TRUE if is_try_block is TRUE or if this scope is
			   an sck_block scope nested within a scope for which
			   is_try_block is set. */
  a_bit_field	using_directives_apply:1;
			/* One or more using directives are present in this
			   scope or a scope nested within this scope for which
			   the symbols made visible by the using directive
			   are to be visible when the lookup reaches this
			   scope. */
  a_bit_field	within_unnamed_namespace:1;
			/* TRUE if the current entry on the scope stack is
			   itself an unnamed namespace or is a named
			   namespace contained within an unnamed namespace. */
  a_bit_field	reactivated_class_being_defined:1;
			/* TRUE for class reactivation scopes if the class
			   being reactivated is in the process of being
			   defined.  This causes the lookup to look on the
			   active list instead of the inactive list for
			   the class members. */
  a_bit_field	is_for_init_block:1;
			/* TRUE if the scope is pushed for a C++ for-init
			   declaration (sck_block only). */
  a_bit_field	namespace_pushed:1;
    		        /* TRUE for class reactivation scopes if the
                           parent namespace was pushed. */
  a_bit_field	exclude_from_context_output:1;
			/* TRUE for scopes that would normally result in
			   the creation of error context information
			   (such as template instantiation scopes),
			   but for which the context information should
			   be suppressed. */
  a_bit_field	instantiation_scope_pushed:1;
			/* TRUE if, when pushing a class and template
			   reactivation scope, a template instantiation
			   scope was pushed. */
  a_bit_field	microsoft_specialization_scope_pushed:1;
			/* TRUE if, when pushing a class and template
			   reactivation scope, a template instantiation
			   scope was pushed for a Microsoft specialization
			   scope. */
  a_bit_field	stop_token_stack_pushed:1;
			/* TRUE if, when pushing a template instantiation
			   scope, a new stop token stack entry was pushed.
			   This flag is set in the last scope pushed by
			   push_template_instantiation_scope, which is
			   not necessarily a template instantiation scope. */
  a_bit_field /* a_name_linkage_kind */
		default_name_linkage:NUM_BITS_FOR_NAME_LINKAGE;
			/* The default language linkage (e.g., extern "C++" or
			   extern "C") for declarations in the current scope
			   (used in C++ mode only).  In general, when a scope
			   is pushed, the setting is copied from the enclosing
			   scope; it may then be modified and later restored
			   when a linkage specification is seen.  However,
			   template instantiation scopes take the setting for
			   the template declaration. */
  a_bit_field	name_linkage_is_explicit:1;
			/* TRUE if the default name linkage was explicitly
			   specified in the source; FALSE for the default
			   setting for the translation unit as a whole. */
  a_bit_field	explicitly_declared_namespace_extension:1;
			/* TRUE for sck_namespace_extension scopes that
			   correspond to explicit declarations. */
  a_bit_field	microsoft_specialization_instantiation_scope:1;
			/* TRUE for an sck_template_instantiation scope pushed
			   for compatibility with the Microsoft compiler,
			   which permits the body of a class specialization to
			   reference template parameters of the template. */
#if USER_CONTROL_OF_STRUCT_PACKING
  a_bit_field	pragma_pack_is_local:1;
			/* TRUE for an sck_function scope of a routine in
			   which a "#pragma pack" directive is local in
			   effect -- i.e., does not affect the packing of
			   structs declared outside the function body. */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
  a_bit_field	is_reactivation:1;
			/* File scopes can be pushed, popped, and then
			   pushed again later.  This is TRUE when a file
			   scope has been re-pushed. */
  a_scope_pointers_block_ptr
		assoc_pointers_block;
			/* Pointer to a scope pointer block that should be
			   used (in place of the one that is embedded in
			   this scope stack entry); NULL when the embedded
			   scope-pointer-block should be used.  This pointer
			   will be non-NULL when kind is sck_namespace or
			   sck_namespace_extension; otherwise it is NULL. */
  a_scope_pointers_block
		pointers_block;
			/* A block of pointers associated with this scope,
			   including a pointer to the linked list of all
			   symbols declared in this scope and pointers to
			   the last entry in linked lists of IL entries
			   entered in the associated IL scope. */
  a_scope_ptr	il_scope;
			/* Pointer to the intermediate language scope
			   entry for this scope.  This can be a real
			   pointer rather than a memory region number because
			   the entry must always be in memory when the scope
			   is active.  NULL if the scope entry has not yet
			   been allocated, which happens in function 
			   declarators and blocks (almost always, a scope
			   entry is not needed, so we wait until something is
			   declared to allocate it).  The entry pointed
			   to can be the one attached to a_routine (usually)
			   or the one attached to a routine type entry
			   (rarely). */
  a_memory_region_number
		il_memory_region;
			/* The number of the IL memory region for this scope.
			   Set even if il_scope == NULL.  Note that this is
			   the "base" memory region; the "current" memory
			   region might switch between this "base" region and
			   the file scope region many times during the
			   processing of the scope. */
  a_memory_region_number
		prev_il_memory_region;
			/* The number of the IL memory region that was the
			   current region at the time this scope was entered.
			   This is restored by pop_scope. */
  a_type_ptr	assoc_type;
			/* When kind == sck_func_prototype, this points to
			   the function type whose prototype scope this is.
			   When kind == sck_class_struct_union or
			   kind == sck_class_reactivation, this points
			   to the class type. */
  a_routine_ptr	assoc_routine;
			/* When kind == sck_function or when kind == 
			   sck_template_instantiation for a function
			   instantiation, this points to the routine
			   whose scope this is. */
  a_namespace_ptr
		assoc_namespace;
			/* When kind == sck_namespace, sck_namespace_extension,
			   or sck_namespace_reactivation, this points to the
			   namespace. */
  a_vla_fixup_ptr
		vla_fixup_list;
			/* When kind == sck_func_prototype.  C mode only.
			   Temporary holding place for the vla_fixup_list.
			   When the scope_stack is popped the vla_fixup_list
			   is moved to a_func_info_block for the function. */
  an_extern_type_fixup_ptr
		extern_type_fixup_list;
			/* List of types of variables and routines to be
			   reset at the end of the scope.  Used when
			   inner- and outer-scope declarations of entities
			   with linkage have compatible but not identical
			   types, and the outer-scope type must be restored
			   at the end of the inner scope. */
  a_generated_entity_block_ptr
		generated_entities;
			/* Set of constants generated in the current scope.
			   (NULL if no such constants were generated.) */
  a_constant_ptr
		shareable_constants_list;
			/* List of shared constants for the current scope.
			   Only used if the scope is a function scope.
			   These are constants that refer to something local
			   to the scope, and therefore cannot be shared at 
			   the file scope.  The only meaningful case is
			   a constant indicating the address of a local
			   variable. */
  a_routine_fixup_ptr
		last_routine_fixup;
			/* Defined for sck_class_struct_union scopes only:
			   the tail of a list of entities used in the token
			   caching and delayed scanning scheme required for
			   C++ member functions (routine bodies and default
			   arguments). */
  /* The following pointers are the end pointers for the lists begun
     in the current IL scope entry.  They are needed only while the scope
     is active (to add entries to the ends of lists), and are therefore
     here instead of in the a_scope entry to save space. */
  a_variable_ptr
		last_parameter;
			/* End of list of parameters of the associated routine,
			   if assoc_routine != NULL.  In declaration order.
			   NULL if no parameters. */
  a_variable_ptr
		last_nonstatic_variable;
			/* End of list of nonstatic local variables of this
			   scope, NULL if none. */
  a_label_ptr	last_label;
			/* End of list of local labels of this scope, NULL
			   if none. */
  a_scope_ptr	first_scope,
		last_scope;
			/* Start and end of list of local scopes (those
			   associated with blocks containing declarations,
			   not with functions or prototypes), NULL if none.
			   A first_scope pointer is needed for those cases
			   where il_scope is NULL.  For ease of implementation,
			   the scopes list is always built using first_scope/
			   last_scope, then transferred to the il_scope entry
			   or into the parent scope when the current scope
			   is popped. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		source_sequence_avail_list;
			/* List of freed source sequence entries that are
			   available for reuse; NULL if none. */
  a_source_sequence_entry_ptr
		ss_list_instantiation_insert_point;
			/* If kind == sck_file, pointer to a source sequence
			   entry before which source sequence entries for a
			   template instantiation should be inserted, or NULL
			   if they should be added to the end of the list.
			   If kind == sck_template_instantiation, the current
			   pointer in the file scope entry when push_scope is
			   called and to which that pointer is restored by
			   pop_scope.  Not used for any other scope kinds. */
  a_source_sequence_entry_ptr
		source_sequence_list,
		end_of_source_sequence_list;
			/* Head and tail of a list of source sequence entries
			   generated while the current scope is active.  When
			   the scope is popped, the list is merged with a
			   list on a containing scope -- except when the scope
			   kind is sck_function and sck_file, in which case
			   the list is moved onto the associated IL scope. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_scope_depth depth_template_declaration_scope;
			/* Depth of the sck_template_declaration scope entry,
			   if any, that the current scope is enclosed by;
			   otherwise, NO_SCOPE_DEPTH. */
  a_scope_depth depth_innermost_instantiation_scope;
                        /* Depth of the nearest enclosing instantiation scope
			   of any kind.  This is a copy of the global
			   variable of the same name. */
  a_symbol_ptr  instance_sym;
                        /* When kind == sck_template_instantiation, contains
                           a pointer to the symbol for the class or function
			   being instantiated or the static data member being
			   defined. */
  a_symbol_ptr  template_sym;
                        /* When kind == sck_template_instantiation, contains
                           a pointer to the symbol for a symbol providing
			   information about the template on which the
			   instantiation is based.  When a template class is
			   being instantiated it points to an sk_class_template
			   symbol; for a nonmember function it points to an
			   sk_function_template symbol; for member functions
			   and static data members of an instance of a class
			   template, it points to an sk_member_function or
			   sk_static_data_member symbol that is a member of
			   a prototype instantiation of the template class. */
  a_template_arg_ptr
                template_arg_list;
                        /* When kind == sck_template_instantiation, contains
                           a pointer to a template argument list. */
  a_source_position
		source_position;
			/* The source position when the scope was pushed
			   onto the stack. */
  a_scope_depth depth_innermost_function_scope;
			/* The scope depth of the containing function scope,
			   or NO_SCOPE_DEPTH if there is no containing
			   function scope or if the scope of a local class or
			   template instantiation intervenes between the
			   current scope and the containing function scope. */
  a_template_decl_info_ptr
		template_decl_info;
                        /* When kind == sck_template_instantiation, contains
			   a pointer to the information about the template
			   declaration from which the instantiation is
			   being generated.
                           When kind == sck_template_declaration, contains
			   a pointer to the template declaration information
			   for the current template declaration nested
                           depth. */
  a_decl_sequence_number
		last_label_decl_seq;
			/* When kind == sck_function, the declaration sequence
			   number of the last label defined (so far) in the
			   current scope; 0 if this is not a function scope
			   or if there are no label definitions.  The value
			   is updated each time a label definition is seen. */
  a_pending_pragma_ptr
		pending_pragmas;
			/* A list of pragmas that have been cached by
			   the lexical routines but have not yet been
			   fully processed.  This list contains only
			   pbk_other pragmas. */
  a_pending_pragma_ptr
		curr_construct_pragmas;
			/* Points to the list of pbk_next_construct
			   pragmas for the construct that is currently
			   being scanned.  This is in the scope stack entry
			   so that it will automatically nest when
			   template instantiations are performed. */
  a_scope_depth	next_scope_that_affects_access_control;
			/* Depth of the first scope stack entry below this
			   one that has an effect on access control.
			   Indicates the next entry on a list headed by
			   depth_of_innermost_scope_that_affects_access_control
			   (a global variable).  Also set in scope stack
			   entries that are not part of the list because they
			   do not affect access control. */
  an_access_error_descr_ptr
		deferred_access_checks;
			/* When defer_access_checks is TRUE, this contains
			   a list of access checks that were done (and failed)
			   and must be repeated once the declarator has been
			   scanned. */
  an_access_error_descr_ptr
		last_deferred_access_check;
			/* When defer_access_checks is TRUE, this points
			   to the last element in a list of access checks. */
  a_scope_depth	saved_curr_deferred_access_scope;
			/* The value of curr_deferred_access_scope when
			   this scope was pushed.  Used to restore the value
			   when the scope is popped. */
  struct an_expr_stack_entry /* struct form used to avoid having to include
			        exprutil.h all over. */
		*saved_expr_stack;
			/* The value of expr_stack when this scope was pushed,
			   used to restore the value when the scope is
			   popped. */
  an_object_lifetime_ptr
		curr_scope_object_lifetime;
			/* A pointer to the object lifetime created for this
			   scope. */
  an_object_lifetime_ptr
		saved_curr_object_lifetime;
			/* The value of curr_object_lifetime when the scope
			   is pushed onto the stack, and the value to which
			   it will be restored when the scope is popped. */
  an_object_lifetime_ptr
		object_lifetime_avail_list;
			/* List of freed object lifetime entries that are
			   available for reuse.  Only used for file and
			   function scopes; the entries on the list belong to
			   the memory region associated with the scope. */
  a_symbol_ptr	templ_member_class_sym;
			/* For sck_template_declaration scopes, this points
			   to the symbol of the class of which the entity
			   currently being defined is a member (e.g., if
			   A<T>::f is being defined, this points to the
			   class type of A<T>.  Contains NULL if the
			   template is not a member. */
  a_scope_depth depth_innermost_namespace_scope;
                        /* Depth of the nearest enclosing namespace scope or,
			   by default, the depth of the file scope. This is
			   a copy of the global variable of the same name. */
  long		num_of_extra_times_pushed;
			/* Namespace scopes may be pushed more than once
			   under some circumstances (such as defining a
			   member of a nested namespace in the enclosing
			   namespace).  When this occurs, the scope is
			   not duplicated on the scope stack.  Instead,
			   this counter is incremented so that when the
			   scope is popped, it is possible to know when
			   the scope should actually be removed from
			   the stack. */
  an_active_using_directive_ptr
		active_using_directives;
			/* Linked list of entries representing the
			   using-directives currently active in the current
			   scope; NULL if none. */
  a_scope_depth	previous_scope;
			/* Scope depth of the scope that logically precedes
			   the current one.  This allows scopes on the stack
			   to be skipped over for name lookup and other
			   purposes.  This is primarily used to hide certain
			   scopes during template instantiation. */
  a_scope_depth	instantiation_context_depth;
			/* Present only for template instantiation scopes.
			   Contains the scope depth of the innermost
			   namespace scope at the point that the instantiation
			   was initiated. */
  a_scope_depth	instantiation_common_depth;
			/* Present only for template instantiation scopes.
			   Contains the scope depth of the scope that is
			   part of both the template definition context and
			   the context at the point of instantiation. */
  a_scope_depth	saved_depth_of_initial_lookup_scope;
			/* The previous value of the global variable
			   depth_of_initial_lookup_scope when a new scope
			   is pushed.  This value is restored when the
			   scope is popped. */
  a_scope_depth	orig_depth;
			/* For nonnested template instantiation scopes,
			   specifies the scope depth before the process
			   of pushing the instantiation context began.
			   This is used to determine how many scopes should
			   be popped when the instantiation scope is popped. */
  a_scope_depth	saved_innermost_scope_that_affects_access;
			/* This field is used in the last scope pushed when
			   a template instantiation scope is pushed.  It is
			   used to store the depth of the innermost scope
			   that affects access control when the template
			   instantiation scope was pushed.  This is needed
			   because the values in the scope stack that are
			   normally used to restore this value are altered
			   by the routines that create the instantiation
			   context. */
  a_template_cache_segment_ptr
		first_template_cache_segment;
			/* Pointer to the first template cache segment entry
			   for a member class or function of the current
			   prototype instantiation.  Present only for
			   template instantiation scopes associated with
			   prototype instantiations. */
  a_template_cache_segment_ptr
		last_template_cache_segment;
			/* Pointer to the last template cache segment entry
			   for a member class or function of the current
			   prototype instantiation.  Present only for
			   template instantiation scopes associated with
			   prototype instantiations. */
  struct a_class_def_state
		*class_def_state;
			/* For sck_class_struct_union scopes, pointer to an
			   entry that tracks general information about the
			   class/struct/union definition as it accumulates;
			   NULL otherwise. */
  a_name_hidden_by_old_for_init_ptr
		names_hidden_by_old_for_init;
			/* For sck_function and sck_block scopes, pointer to
			   a (possibly NULL) linked list of entries that
			   identify symbols from an enclosing scope for which
			   hidden_by_old_for_init is set to TRUE (because of
			   for-init declarations of for-statements in the
			   current scope).  Always NULL in C-mode or when
			   use_nonstandard_for_init_scope is TRUE. */
  struct a_tmpl_decl_state
		*tmpl_decl_state;
			/* For template declaration scopes, points to the
			   entry used to record information about the
			   current template declaration. */
  unsigned long
		pending_templ_arg_lists;
			/* The number of template argument lists that are
			   currently in the process of being scanned.
			   In other words, the number of opening "<" delimiters
			   that have been seen without matching closing ">"
			   delimiters. */
  a_nondependent_call_info_ptr
		next_nondependent_call;
			/* When doing dependent name processing, this
			   field is present for template instantiation scopes
			   and points to the next nondependent call entry
			   for the current instantiation.  During a real
			   instantiation this list is used to determine
			   whether a given call is dependent. */
} a_scope_stack_entry;

/*
Given a scope depth, return a pointer to the scope stack entry or
a NULL pointer if the scope depth is NO_SCOPE_DEPTH.
*/
#define scope_stack_entry_for(depth)					\
  ((depth) == NO_SCOPE_DEPTH ? NULL : &scope_stack[(depth)])


/*
Given a pointer a scope stack entry, return the address of the associated
scope-pointers-block -- it may either be part of the entry itself or part of
another data structure elsewhere (as indicated by the value of the
assoc_pointers_block field in the scope stack entry).
*/
#define assoc_pointers_block_of(ssep)                                    \
  ((ssep)->assoc_pointers_block == NULL ?                                \
     &((ssep)->pointers_block) : (ssep)->assoc_pointers_block)

/*
Given a pointer to a scope stack entry, return the address of the previous
scope stack entry according to the previous_scope field.  Return NULL if there
is no previous scope.
*/
#define previous_scope_of(ssep)						\
  ((ssep)->previous_scope == NO_SCOPE_DEPTH				\
                                ? NULL : &scope_stack[(ssep)->previous_scope])


/*
Given a pointer to a scope stack entry, return the scope depth.  If the
pointer is NULL, return NO_SCOPE_DEPTH.
*/
#define scope_depth_of(ssep)						\
  ((ssep) == NULL ? NO_SCOPE_DEPTH : (ssep - &scope_stack[0]))


/*
Given a pointer to a scope stack entry, return TRUE if and only if the
associated scope is a file or namespace scope.
*/
#define is_file_or_namespace_scope(ssep)                     \
  ((ssep)->kind == (a_scope_kind)sck_file ||                \
   (ssep)->kind == (a_scope_kind)sck_namespace ||            \
   (ssep)->kind == (a_scope_kind)sck_namespace_extension)

/*
TRUE if we are in a context in which template dependent types need to
be handled in contexts such as expressions.  Typically, this is in
a prototype instantiation, but can also occur in template declaration
scopes.  It is also TRUE when is_nonreal_instantiation is TRUE.
*/
#define is_template_dependent_context()					\
  (depth_template_declaration_scope != NO_SCOPE_DEPTH ||		\
   scope_stack[depth_scope_stack].in_prototype_instantiation ||		\
   scope_stack[depth_scope_stack].in_nonreal_instantiation)


/*
TRUE if we are in a template prototype instantiation context, which
includes template declaration scopes.  This is similar to
is_template_dependent_context, but excludes nonreal instantiations.
*/
#define is_prototype_instantiation_context()				\
  (depth_template_declaration_scope != NO_SCOPE_DEPTH ||		\
   scope_stack[depth_scope_stack].in_prototype_instantiation)


EXTERN a_scope_stack_entry_ptr
		scope_stack /* = NULL */;
			/* Stack of entries describing active scopes.
			   scope_stack[0] is the entry for the file scope,
			   scope_stack[1] is an entry for a function scope,
			   etc.  Dynamically allocated; can be expanded
			   if necessary.  size_scope_stack gives the
			   number of elements currently allocated.
			   Allocation is not per-file. */
/* Note that the following variables, which give positions in the scope stack,
   are defined as indexes into the array, not as pointers.  Pointers into
   the scope stack are dangerous because the scope stack can be reallocated
   and moved on a push_scope. */
EXTERN a_scope_depth
		depth_scope_stack;
			/* Current depth of the scope stack.  NO_SCOPE_DEPTH
			   (i.e., -1) indicates that the stack is empty. */
EXTERN a_scope_depth
		depth_of_initial_lookup_scope;
			/* Scope depth of the scope at which name lookup
			   operations should begin.  This is usually the
			   same as depth_scope_stack but is different
			   under certain conditions (for example, when
			   a namespace reactivation is pushed on top of
			   a template declaration scope). */
EXTERN a_scope_depth
		decl_scope_level;
			/* Level in the scope stack that contains the
			   current declaration level.  In C, differs from
			   depth_scope_stack when the innermost "scopes"
			   are for struct/union fields; decl_scope_level
			   then contains the real scope level rather than
			   the struct/union pseudo-scope level.  In C++,
			   differs from depth_scope_stack when the innermost
			   "scope" is a class reactivation. */
EXTERN a_scope_depth
		depth_innermost_function_scope;
			/* Level in the scope stack that contains the innermost
			   function scope, or NO_SCOPE_DEPTH if there isn't
			   one. */
EXTERN a_scope_ptr
		innermost_function_scope;
			/* The innermost function scope, or NULL if there isn't
			   one.  Usually matches
			   depth_innermost_function_scope, but can be
			   different in situations where a function is being
			   processed where no scope stack entry exists
			   (e.g., in IL lowering, when routines are
			   generated). */
EXTERN a_scope_depth
		depth_innermost_instantiation_scope;
			/* If there are template instantiation scopes on the
                           scope stack, this is the depth of the innermost
                           one.  Otherwise, NO_SCOPE_DEPTH. */
EXTERN a_scope_depth
		depth_template_declaration_scope;
			/* Depth of the sck_template_declaration scope entry,
			   if any, that the current scope is enclosed by;
			   otherwise, NO_SCOPE_DEPTH. */
EXTERN a_scope_depth
		curr_deferred_access_scope;
			/* Depth of the scope entry to be used to determine
			   whether access checking should be deferred, and if
			   so, the entry to which the deferred access checks
			   should be attached.  Set to NO_SCOPE_DEPTH if
			   access checking cannot be deferred in this scope. */

#if GENERATE_SOURCE_SEQUENCE_LISTS
EXTERN a_boolean
		source_sequence_entries_disallowed;
			/* TRUE if the current scope establishes or belongs to
			   a context in which source sequence entries should
			   not be issued -- e.g. a template declaration, a
			   a template instantiation, or a pragma. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

EXTERN a_boolean
		inside_local_class;
			/* TRUE if we are currently inside a local class,
			   i.e., a class defined within a function. */
EXTERN a_scope_depth
		depth_innermost_namespace_scope;
			/* If there are any namespace scopes on the scope
			   stack, this is the depth of the innermost one.
			   Otherwise, it is the depth of the file scope.
			   It is defined in both C and C++. */
EXTERN a_scope_number
		next_scope_number;
			/* Next scope number to be assigned.  These are
			   unique identifiers for each scope, not just
			   the scope nesting depth.  Also used for the
			   pseudo-scopes associated with the members of
			   structs and unions in C (not C++). */

EXTERN a_scope_depth
		depth_of_innermost_scope_that_affects_access_control;
			/* If there are scopes on the scope stack that
			   affect C++ access control, this is the depth of
			   the innermost one.  Otherwise, NO_SCOPE_DEPTH.
			   Heads a list linked by
			   next_scope_that_affects_access_control. */

EXTERN a_scope_depth
		num_classes_on_scope_stack;
			/* Current count of sck_class_struct_union and
			   sck_class_reactivation entries in scope_stack.
			   When non-zero, we are inside a class or
			   reactivation of the scope of a class, and name
			   lookup is more complicated. */

/* Begin a name scope. */
extern a_scope_ptr push_scope(a_scope_kind       kind,
       	                      a_scope_number     scope_number_to_reuse,
                              a_type_ptr         assoc_type,
                              a_routine_ptr      assoc_routine);

extern void push_file_scope(a_boolean	is_reactivation);

extern
void push_template_declaration_scope(a_template_decl_info_ptr decl_info);


extern a_scope_ptr push_for_init_scope(void);

extern a_scope_ptr push_namespace_scope(a_scope_kind    kind,
                                        a_namespace_ptr assoc_namespace);

extern void pop_namespace_scope(void);

extern void push_template_instantiation_scope(
                            a_template_decl_info_ptr	decl_info,
                            a_type_ptr			assoc_type,
                            a_routine_ptr		assoc_routine,
                            a_symbol_ptr		instance_sym,
                            a_symbol_ptr		template_sym,
                            a_template_arg_ptr		template_arg_list,
			    a_boolean			push_stop_tokens,
			    a_push_scope_options_set	options);

extern void pop_template_instantiation_scope(void);

extern void finish_function_body_processing(a_scope_ptr scope,
                                            a_boolean   discard_function_body);
/* End a name scope. */
extern void pop_scope(void);
extern void push_namespace_extension_scope(a_namespace_ptr nsp);
extern void pop_namespace_extension_scope(void);
extern void f_push_namespace_reactivation_scope(
				a_namespace_ptr		nsp,
				a_boolean		force_new_entry);

/* Macro that calls f_push_namespace_reactivation_scope and supplies a default
   value for the force_new_entry parameter. */
#define push_namespace_reactivation_scope(nsp)				\
  f_push_namespace_reactivation_scope(nsp, /*force_new_entry=*/FALSE)

extern void pop_namespace_reactivation_scope(void);
extern void push_class_reactivation_scope(a_type_ptr   class_type,
                                          a_boolean    extend_namespace);
extern void pop_class_reactivation_scope(void);
extern void push_instantiation_scope_for_class(
			a_type_ptr	class_type,
			a_boolean	is_microsoft_specialization_scope);
extern void push_class_and_template_reactivation_scope(
                                 a_type_ptr	class_type,
                                 a_boolean      reactivate_template_params,
                                 a_boolean	extend_namespace);

extern
a_scope_depth scope_depth_of_symbol(a_symbol_ptr  sym,
                                    a_boolean     *is_local_to_function);
extern a_boolean namespace_is_enclosed_by_scope(a_symbol_ptr             sym,
                                                a_scope_stack_entry_ptr  ssep);
/*
Call namespace_is_enclosed_by_scope for the current scope.
*/
#define namespace_is_enclosed_by_curr_scope(sym)                     \
  (namespace_is_enclosed_by_scope((sym), &scope_stack[depth_scope_stack]))

extern a_boolean current_class_symbol_if_class_template(a_symbol_ptr *sym);

extern void add_active_using_directive(a_using_decl_ptr udp,
				       a_scope_depth    depth);

extern void report_for_init_difference(a_symbol_ptr       sym,
                                       a_source_position  *pos);

extern void push_name_linkage(a_name_linkage_kind  kind);

extern void pop_name_linkage(void);

extern void set_needed_flags_at_end_of_file_scope(a_scope_ptr scope);

a_boolean keep_function_body_for_possible_inlining(a_routine_ptr routine);

extern void clear_scope_pointers_block(a_scope_pointers_block_ptr  spbp);

extern void wrapup_namespace_scopes(a_scope_ptr scope_ptr);

extern
void wrapup_scope(a_scope_ptr			scope_ptr,
                  a_scope_kind			kind,
                  a_scope_pointers_block_ptr	pointers_block,
                  a_boolean 	                is_namespace_wrapup);

extern void scope_stk_one_time_init(void);

extern void scope_stk_init(void);

#if DEBUG
extern int db_scope_kind(a_scope_kind sck);
extern void db_scope_stack_entry_at_depth(a_scope_depth  depth);
extern void db_scope_stack_entry(a_scope_stack_entry_ptr ssep);
extern void db_scope_stack(void);
#if EXTRA_SOURCE_POSITIONS_IN_IL
extern void db_decl_pos_info(a_symbol_ptr sym);
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#endif /* DEBUG */

#endif /* ifndef SCOPE_STK_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1995 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
