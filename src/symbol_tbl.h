/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

symbol_tbl.h - Declarations related to symbol table processing.

*/

/* Avoid including these declarations more than once. */
#ifndef SYMBOL_TBL_H
#define SYMBOL_TBL_H 1

/* Declare pointer types up front to minimize mutual recursion problems. */
typedef struct a_symbol        *a_symbol_ptr;
typedef struct a_symbol_header *a_symbol_header_ptr;
typedef struct a_macro_param   *a_macro_param_ptr;
typedef struct a_macro_def     *a_macro_def_ptr;

/* The pointer to a_routine_fixup is declared here even though the struct
   itself is defined in class_decl.c.  This allows the pointer to be made
   available to symbol_tbl.h without creating recursive reference problems. */
typedef struct a_routine_fixup *a_routine_fixup_ptr;

/* The pointer to a_def_arg_expr_fixup is declared here even though the struct
   itself is defined in def_arg.h.  This allows the pointer to be made
   available to symbol_tbl.h without creating recursive reference problems. */
typedef struct a_def_arg_expr_fixup *a_def_arg_expr_fixup_ptr;

/* The pointer to a_pending_pragma is declared here even though the struct
   itself is defined in pragma.h.  This allows the pointer to be made
   available to symbol_tbl.h without creating recursive reference problems. */
typedef struct a_pending_pragma *a_pending_pragma_ptr;

/* Some other things declared up front to avoid mutual recursion problems. */

/*
Options for normal_id_lookup, class_qualified_id_lookup, etc.,
represented as a bit set:
*/
typedef int an_id_lookup_options_set;
#define IDL_MUST_BE_CLASS 0x1	/* The symbol must be a class, struct, or
				   union name, or a typedef of one of those. */
#define IDL_MUST_BE_TAG 0x2	/* The symbol must be a class, struct, union,
				   or enum (not a typedef of one of those). */
#define IDL_CONSTRAINTS (IDL_MUST_BE_CLASS | IDL_MUST_BE_TAG)
				/* The set of all options that impose
				   constraints on the symbol to be found. */
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
#define IDL_NO_OPTIONS 0	/* No special lookup options. */

/*
A symbol-reference kind is a bit vector whose values are defined in
symbol_ref.h.  The typedef declaration is here to avoid mutual inclusion
problems.
*/
typedef int a_symbol_reference_kind;

/*
The definition of a_symbol_locator refers to declarations from il_def.h,
but lexical.h requires a_symbol_locator to be defined.  So the former is
included here, and the latter is included after a_symbol_locator is
declared.
*/
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef IL_TO_STR_H
#include "il_to_str.h"
#endif /* ifndef IL_TO_STR_H */


typedef struct a_symbol_locator {
  /* Data structure used to store information about an identifier token.
     Can be used to look up the identifier or enter it into the symbol
     table. */
  /* If you change this structure, be sure to also change the initialization
     of global variable cleared_locator in symbol_tbl_one_time_init. */
  a_symbol_header_ptr
		symbol_header;
			/* The symbol header for the list of symbols with the
			   identifier.  When this is NULL, this locator is
			   for an error symbol. */
  a_source_position
		source_position;
			/* The source position to be used when this symbol
			   is entered. */
  unsigned int	is_qualified_name:1;
			/* TRUE if the "identifier" is a C++ qualified-name
			   (e.g., "A::x" or "::y").  specific_symbol points
			   to the proper symbol. */
  unsigned int	is_global_qualified_name:1;
			/* TRUE if the "identifier" is a C++ qualified-name
			   that begins with a unary "::" (e.g., "::y" or
			   ::A::x). */
  unsigned int  is_file_scope_qualified_name:1;
			/* TRUE if the "identifier" is a C++ qualified-name
			   that refers to a file scope entity (e.g., ::y
			   but not ::A::x). */
  unsigned int  is_operator_name:1;
			/* TRUE if the "identifier" is a C++ overloaded
			   operator name, of the form "operator<token>",
			   e.g., "operator+".  Cannot be TRUE when
			   is_conversion_name is TRUE. */
  unsigned int  is_conversion_name:1;
			/* TRUE if the "identifier" is a C++ user-defined
			   conversion name, of the form "operator <type-name>",
			   e.g., "operator int".  Cannot be TRUE when
			   is_operator_name is TRUE. */
  unsigned int  is_destructor_name:1;
			/* TRUE if the "identifier" is a C++ destructor
			   name, of the form "~<name>". */
  unsigned int  is_semivisible_nested_type:1;
			/* TRUE if specific_symbol points to a nested type
			   that is not actually visible, except as a C++
			   anachronism (ARM 18.3.5). */
  unsigned int  access_control_error_reported:1;
			/* TRUE if an accessibility error has already been
			   issued on the associated symbol. */
  unsigned int  has_been_coalesced:1;
			/* TRUE if the identifier has already been processed
			   by is_generalized_identifier_start -- even if
			   no coalescing was actually performed.  This
			   indicates that no processing is needed should
			   is_generalized_identifier_start be called again. */
  unsigned int  is_vacuous_destructor_reference:1;
			/* TRUE if the identifier is a destructor name of
			   a type that has no destructor.  Used for
			   explicit destructor invocations of the form
			   p->int::~int.  The type can be a nonclass type
			   or a class type with no destructor. */
  unsigned int	is_nonclass_destructor:1;
			/* TRUE for vacuous destructor references for 
			   nonclass types such as int::~int or i::~i
			   where "i" is a typedef name. */
  unsigned int  is_error:1;
			/* TRUE if an error has been diagnosed on the use
			   of the associated identifier and no symbol should
			   be entered into the symbol table. */
  unsigned int	do_not_clear_specific_symbol:1;
			/* TRUE if the specific symbol field of the locator
		           should not be cleared when clear_specific_symbol
                           is called.  This is set when clearing the specific
			   symbol field would result in the loss of information
			   that cannot be recovered by repeating the lookup
			   process.  This is TRUE for template references
			   that have been coalesced and for specific symbol
			   error locators. */
  unsigned int	is_template_id:1;
			/* TRUE if the coalesced identifier is a template-id
			   (i.e., template-name < template-arg-list >). */
  a_symbol_ptr	specific_symbol;
			/* If is_qualified_name is TRUE, this points to the
			   specific symbol for the qualified name.  Otherwise,
			   if this pointer is non-NULL, it is the result of
			   the most recent lookup of this identifier (e.g.,
			   by normal_id_lookup). */
  a_type_ptr	qualifier_class_type;
			/* If is_qualified_name is TRUE, this points to the
			   type specified by the qualifier, if any.
			   If is_vacuous_destructor is TRUE this points
			   to the type of the qualifier, which may not
			   actually be a class type (e.g., for int::~int
			   this will point to the type "int"). */
  union {
    /* When both is_operator_name and is_conversion_name are FALSE, both
       variants are undefined. */
    /* When is_operator_name is TRUE: */
    an_opname_kind
		opname;
			/* The token that identifies the operator when an
			   operator name is scanned.  For () and [] operators
			   the identifying tokens are tok_lparen and
			   tok_lbracket, respectively. */
    /* When is_conversion_name is TRUE: */
    a_type_ptr  conversion_result_type;
			/* The return type when a user-defined conversion
			   name is scanned. */
  } variant;
} a_symbol_locator;


EXTERN a_symbol_locator
		cleared_locator;
			/* An empty locator used to initialize locators in
                           macro clear_locator. */

/*
Clear a symbol locator.
*/
#define clear_locator(locator, position)                              \
{  *(locator) = cleared_locator;                                      \
   (locator)->source_position = *position;                            \
}  /* clear_locator */

/* Mark a symbol locator to indicate an error and prevent its entry
   into the symbol table. */
#define set_to_error_locator(loc)                                     \
{  clear_locator(&(loc), &error_position); (loc).is_error = TRUE; }

/* Like set_to_error_locator, but preserving information about the
   identifier with which the locator is associated. */
#define set_to_named_error_locator(loc)                               \
{  (loc).is_error = TRUE; (loc).specific_symbol = NULL; }

/* Test a locator to see if it is an error locator. */
#define is_error_locator(loc) ((loc).is_error)

/* Retrieve a pointer to the symbol list from a locator. */
#define symbol_list_from_locator(loc) ((loc).symbol_header->symbol)

/* Retrieve a pointer to the inactive symbol list from a locator. */
#define inactive_symbol_list_from_locator(loc)                        \
  ((loc).symbol_header->inactive_symbols)

/* Clear the specific symbol field of the locator unless instructed not
   to by the do_not_clear_specific_symbol field of the locator. */
#define clear_specific_symbol(loc)					\
{  if (!((loc).do_not_clear_specific_symbol)) (loc).specific_symbol = NULL;}


#ifndef LEXICAL_H
#include "lexical.h"
#endif /* ifndef LEXICAL_H */
#ifndef MEM_TABLES_H
#include "mem_tables.h"
#endif /* ifndef MEM_TABLES_H */

/*
Kinds of symbols in the symbol table.
If this is changed, the definitions for name_space_for_symbol_kind
and symbol_kind_names should also be changed.
*/
enum a_symbol_kind_tag {
  sk_keyword,    	/* Language keyword. */
  sk_macro,       	/* Preprocessor macro. */
  sk_constant,		/* Constant (enumerator). */
  sk_type,		/* Typedef'd type. */
  sk_class_or_struct_tag, /* Tag of a struct, or C++ class type. */
  sk_union_tag,		/* Tag of a union, or C++ union type. */
  sk_enum_tag,		/* Tag of an enumeration, or C++ enum type. */
  sk_variable,		/* Variable or parameter. */
  sk_field,		/* Field (member) of a struct or union.  In C++,
			   a non-static data member. */
  sk_static_data_member,/* Static data member of a class. */
  sk_member_function,   /* Member function of a class. */
  sk_routine,		/* Function. */
  sk_label,		/* Label in a function. */
  sk_undefined,		/* Undefined identifier. */
  sk_extern_variable,	/* Definition of a variable with external or internal
			   linkage, used to check that all definitions of a
			   given external/internal name are equivalent. */
  sk_extern_routine,	/* Definition of a routine with external or internal
			   linkage, ditto. */
  sk_projection,	/* Projection of a member symbol from a base class
			   into a derived class. */
  sk_overloaded_function, /* C++ overloaded function (member or non-member). */
  sk_parameter,         /* Parameter name in a function prototype. */
  sk_class_template,    /* Definition of a class template. */
  sk_function_template, /* Definition of a function template. */
  sk_last
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte a_symbol_kind;


/*
Table of names corresponding to symbol kinds.
*/
EXTERN char	*symbol_kind_names[(int)sk_last + 1]
#if VAR_INITIALIZERS
= {
   "keyword", "macro", "constant", "type", "class or struct", "union",
   "enum", "variable", "field", "static data member", "member function",
   "routine", "label", "undefined", "extern variable", "extern routine",
   "projection", "overloaded function", "parameter",
   "class template", "function template",
   "last" /* used to check that initialization is right. */
}
#endif /* VAR_INITIALIZERS */
;

/* Macro to return the name of a symbol kind. */
#define name_of_symbol_kind(kind) symbol_kind_names[(int)(kind)]


/*
There are different "name spaces" in each scope (see standard, 3.1.2.3).
The following define the name spaces and a mapping from symbol kind to
the associated name space.
*/
typedef enum /*a_name_space_kind*/ {
  nsk_label,		/* Code labels. */
  nsk_tag,		/* Struct, union, and enum tags, in C.  (In C++,
			   those have kind nsk_other, and this kind is
			   not used.) */
  nsk_other,		/* The primary case: types, constants, variables,
			   functions. */
  nsk_macro,		/* Macros. */
  nsk_keyword,		/* Keywords. */
  nsk_extern,		/* External names of variables and routines, perhaps
			   truncated.  Used to check that all uses of
			   a given external name are equivalent. */
  nsk_member		/* Members (fields) of structs and unions.  Used in
			   C mode only. */
} a_name_space_kind;

EXTERN a_name_space_kind
		name_space_for_symbol_kind[(int)sk_last+1];
			/* For each symbol kind, this array maps the kind to
			   the associated name space.  See
			   symbol_tbl_one_time_init for initialization. */

typedef struct a_macro_param {
  /* A parameter of a function-like preprocessor macro.  The names of
     parameters need to be kept around for checking of benign redefinitions
     of macros. */
  char		*name;
			/* Parameter name, null-terminated. */
  a_macro_param_ptr
		next;
			/* Pointer to the next parameter for the same macro,
			   or NULL if this is the last parameter. */
} a_macro_param;

typedef struct a_macro_def {
  /* For macros defined to the preprocessor: */
  unsigned int	object_like:1;
			/* TRUE if this macro is object-like (i.e., has no
			   parameters). */
  unsigned int	try_to_scan_and_save_constant_value:1;
			/* TRUE if the macro is object-like and its definition
			   appears to be a single literal constant, so
			   the constant value should be scanned and saved
			   when the macro is expanded.  At that point, this
			   flag will be set to FALSE, is_manifest_constant
			   will be set to TRUE, and constant_value will
			   point to the constant value. */
  unsigned int	is_manifest_constant:1;
			/* TRUE if the expansion of this (object-like) macro
			   is a literal constant, and its value is given
			   by constant_value. */
  unsigned int	cannot_be_redefined:1;
			/* TRUE if this is a predefined macro that cannot
			   be redefined later.  This is TRUE for ANSI
			   predefined macros. */
  unsigned int	ref_suppresses_pch_file:1;
			/* TRUE if referencing this macro within a header is
			   incompatible with creating a precompiled header
			   file; TRUE, e.g., for predefined macros __DATE__
			   and __TIME__. */
  a_macro_param_ptr
		param_list;
			/* Pointer to a list of entries describing the
			   formal parameters of this macro.  NULL if
			   the macro is object-like or has no parameters. */
  char		*repl_text;
			/* Replacement body for the macro.  Contains cues
			   on where to insert argument values, do pasting,
			   etc.  See below.  NULL for special macros that
			   require code expansion (e.g., __LINE__). */
  a_token_kind	constant_token_kind;
			/* If is_manifest_constant is TRUE, this indicates
			   the kind of token that the macro expands to
			   (e.g., tok_int_constant).  When try_to_scan_...
			   is TRUE, this indicates the same thing, but in
			   pp-token terms (e.g., tok_pp_number). */
  a_constant_ptr
	       	constant_value;
			/* If is_manifest_constant is TRUE, this points to
			   the value of the literal constant. */
#if RECORD_MACROS_IN_IL
  a_macro_ptr	macro;	/* The IL macro entry.  NULL for predefined macros
			   and those defined on the command line. */
#endif /* RECORD_MACROS_IN_IL */
} a_macro_def;

/*
The repl_text string for a macro definition is a sequence of sections,
each of which defines some part of the replacement text of the macro.
A null follows the last sequence and ends the repl_text string.  The
sequences are:
*/
typedef enum /*a_repl_text_seq_kind*/ {
  rt_null,      	/* Equivalent to '\0' (null), marks end of string. */
  rt_text,		/* Raw text.  Followed by 3 bytes containing a
			   character count, and then that many characters
			   of raw text. */
  rt_raw_argument,
  rt_right_raw_argument,
			/* Raw string for argument.  Followed by 3 bytes
			   containing the argument number (first argument is
			   numbered 1).  "right" argument is one to the right
			   of "##"; the normal form is used for an argument
			   to the left of "##" and all arguments in pcc
			   mode. */
  rt_stringized_raw_argument,
			/* Same as rt_raw_argument, but argument raw string is
			   turned into a string literal (see standard,
			   3.8.3.2). */
  rt_argument		/* Macro-expanded string for argument.  Followed by 3
			   bytes containing the argument number, as for 
			   rt_raw_argument. */
} a_repl_text_seq_kind;


/*
Fetch a multi-byte number from a macro-definition string.  Return it in
num.  rtp points to the first byte of the number; it is advanced
past the number on return.
*/
#define NUM_BYTES_IN_MULTI_BYTE_REPL_TEXT_NUMBER 3
#define get_macro_repl_text_number(num, rtp)                          \
{ sizeof_t temp = 0;                                             \
  temp  = (sizeof_t)*(a_byte *)rtp++;                            \
  temp |= (sizeof_t)(*(a_byte *)rtp++) << CHAR_BIT;              \
  temp |= (sizeof_t)(*(a_byte *)rtp++) << CHAR_BIT*2;            \
  num = temp;                                                         \
}  /* get_macro_repl_text_number */


/*
Put a multi-byte number (num) into a macro-definition string. rtp points
to the first byte of the number; it is advanced past the number on return.
*/
#define PN_BYTE_MASK ((1 << CHAR_BIT) - 1)
#define put_macro_repl_text_number(num, rtp)                          \
{ sizeof_t temp = num;                                           \
  *(a_byte *)rtp++ = (a_byte)(temp                 & PN_BYTE_MASK);   \
  *(a_byte *)rtp++ = (a_byte)((temp >> CHAR_BIT)   & PN_BYTE_MASK);   \
  *(a_byte *)rtp++ = (a_byte)((temp >> CHAR_BIT*2) & PN_BYTE_MASK);   \
}  /* put_macro_repl_text_number */


/*
Kinds of fixup to be performed on entries in the dependent type fixup list.
*/
enum a_dependent_type_fixup_kind_tag {
  dtfk_arg_transfer_method,	
			/* Set the arg transfer method flag in a param type. */
  dtfk_routine_calling_method,
			/* Set the routine calling method flag in a routine
			   type. */
  dtfk_array_type_size,	/* Set the size of an array type. */
  dtfk_check_op_arrow_return_type
			/* Check the return type of an operator-> function. */
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte a_dependent_type_fixup_kind;

/*
Entries identifying array types, routine types, and parameters that are
dependent on an incomplete class type and that must be fixed up when the
class is completed.  For example, the array types must have their sizes
computed (note: arrays of incomplete struct are an extension).
*/
typedef struct a_dependent_type_fixup *a_dependent_type_fixup_ptr;
typedef struct a_dependent_type_fixup {
  a_dependent_type_fixup_ptr
		next;
			/* Next fixup on the list, or NULL if this is the
			   last. */
  a_source_position
		decl_position;
			/* Source position at which a diagnostic is to be
			   issued, if required. */
  a_dependent_type_fixup_kind
		fixup_kind;
			/* The kind of fixup to be applied to the entity. */
  a_tagged_pointer
		entity;
			/* A pointer to a type or to a param type.  When the
			   entity is a type, it may be an array whose
			   underlying element type is an incomplete class type
			   or it may be a routine type whose return type is an
			   incomplete class type; when the entity is a param
			   type, the type is an incomplete class type.  The
			   fixup takes place when the incomplete class type
			   is completed. */
} a_dependent_type_fixup;


typedef struct a_symbol_list_entry *a_symbol_list_entry_ptr;
typedef struct a_symbol_list_entry {
  /* Entry created to produce a list of symbols for some special purpose.
     (For example, such a list is created to track the user-defined conversion
     functions for a given class type.  The symbol pointed to will be an
     sk_member_function or sk_projection symbol identifying a function to
     convert a class object to another type.) */
  a_symbol_list_entry_ptr
		next;
			/* Next in a linked list of symbol list entries; NULL
			   for the last on the list. */
  a_symbol_ptr  symbol;
			/* Pointer to a symbol entry. */
} a_symbol_list_entry;


typedef struct a_class_symbol_supplement *a_class_symbol_supplement_ptr;
typedef struct a_class_symbol_supplement {
  /* Additional information about a C++ class, struct, or union, supplementing
     the information residing in the class's symbol entry. */
  a_symbol_ptr	symbols;
			/* Symbol entries for members of the class. */
  a_symbol_ptr	constructor;
			/* Pointer to either an sk_member_function symbol (when
			   there is only one constructor defined for the class)
			   or an sk_overloaded_function symbol (when there are
			   more than one); NULL if there is none. */
  a_symbol_ptr	destructor;
			/* Pointer to an sk_member_function symbol that
			   identifies the destructor for this class; NULL if
			   there is none. */
  a_symbol_ptr  assignment_operator;
			/* Pointer to a symbol (sk_member_function or
			   sk_overloaded_function) symbol that identifies
			   the assignment operator for this class; NULL if
			   there is none. */
  a_symbol_list_entry_ptr
		conversion_list;
			/* Pointer to a linked list of entries providing
			   quick access to user-defined conversion functions
			   declared for this class.  Once the class has been
			   defined this list is exhaustive, including one
			   entry for each target type for which a conversion
			   is defined.  Inherited conversion functions are
			   represented by projection symbols. */
  a_routine_fixup_ptr
		routine_fixup_list;
			/* Pointer to a list of entities used in the token
			   caching and delayed scanning scheme required for
			   C++ member functions (routine bodies and default
			   arguments). */
  a_symbol_ptr  class_template;
                        /* Pointer to a class template symbol.  Present
                           only when this class is an instantiation of
                           a class template, NULL otherwise. */
  a_scope_number
		member_decl_scope;
			/* Scope number of members of the class.  For
			   normal classes this is set by push_scope.  For
			   proxy and nonreal classes this is assigned when
			   a lookup is done. */
  a_type_ptr    template_param_for_proxy_class;
			/* If the class is a proxy class associated with
			   a template parameter type this field points
			   back to the template parameter; otherwise
			   it is NULL. */
  a_symbol_ptr	corresp_prototype_sym;
			/* If the class is a template class instance, or a
			   class nested within a template class, this points
			   to the corresponding prototype instantiation
			   class.  Otherwise, it is NULL. */
  a_token_sequence_number
		prototype_token_sequence_number;
			/* The token sequence number of a token that
			   represents this class.  Present for the prototype
			   instantiation of a class template and the
			   prototype instantiation of any nested classes
			   within the class template. */
  a_dependent_type_fixup_ptr
		dependent_type_fixup_list;
			/* If the current class is not yet defined, a pointer
			   to a list of entries identifying arrays, function
			   types, and parameters that are dependent on it and
			   require fixup when it is completed.  Once the class
			   is defined, the pointer is cleared. */
  unsigned int	constructor_required:1;
			/* TRUE if the class must have a constructor (either
			   declared by the user or generated by the compiler)
			   because it has virtual base classes, virtual
			   functions, or base classes or members with
			   constructors. */
  unsigned int	destructor_required:1;
			/* TRUE if the class must have a destructor (either
			   declared by the user or generated by the compiler)
			   because it has base classes or members with
			   destructors. */
  unsigned int	has_default_constructor:1;
			/* TRUE if a default constructor has either been
			   declared or generated for the class. */
  unsigned int  has_copy_constructor:1;
			/* TRUE if a copy constructor has either been declared
			   or generated for the class. */
  unsigned int  has_copy_constructor_for_const_object:1;
			/* TRUE if there is a copy constructor for the class
			   and it can be used to copy a const object. */
  unsigned int  assignment_by_bitwise_copy_allowed:1;
			/* TRUE if assignment can be performed by a bitwise
			   copy rather than by calling an assignment operator
			   function (i.e., when the assignment operator is
			   not user-defined and when the current class has no
			   virtual base classes and no subobjects for which
			   bitwise copy is not allowed). */
  unsigned int	construction_by_bitwise_copy_allowed:1;
			/* TRUE if copy construction can be performed by a
			   bitwise copy rather than by calling a copy
			   constructor function. */
  unsigned int  target_of_conversion_function:1;
			/* TRUE if this class is the target of a user-defined
			   conversion function (for conversion from another
			   class to this class). */
  unsigned int  any_ref_member:1;
			/* TRUE if this class has any fields of reference
			   type. */
  unsigned int  any_nested_classes:1;
			/* TRUE if this class has any nested classes. */
  unsigned int  is_class_aggregate:1;
			/* TRUE if the class has no constructors, no base
			   classes, no private or protected members, and
			   no virtual functions (ARM 8.4.1). */
  unsigned int  has_operator_new:1;
			/* TRUE if a member operator new() has been declared
			   for this class or a class from which it derived. */
  unsigned int  has_operator_delete:1;
			/* TRUE if a member operator delete() has been declared
			   for this class or a class from which it derived. */
  unsigned int  is_nonreal_class:1;
			/* TRUE if the class is an instantiation of a class
			   template based on template arguments that include
			   one or more template parameters.  For instance,
			   for the class template declared by
			      template <class T, int I> class vec;
			   the prototype instantiation vec<T,I> is a "nonreal"
			   class, but so is vec<T,3>, where T represents a
			   template parameter, e.g., in the declaration:
			      template <class T> void f(vec<T,3> *vp) { ... }
                           In addition, classes that are nested within
			   nonreal classes are marked as nonreal. */
  unsigned int  is_prototype_instantiation:1;
			/* TRUE when this class is a nonreal class that
		 	   is the prototype instantiation.  Also TRUE for
			   classes nested within the prototype
			   instantiation. */
  unsigned int	is_specific_template_def:1;
			/* TRUE if the class is a specific definition of
			   a template class instance.  FALSE if the
			   instance was generated from the class template. */
  unsigned int  any_nonstatic_data_members:1;
			/* TRUE if the class or any of its base classes has
			   one or more nonstatic data members. */
  unsigned int	any_nonreal_base_classes:1;
			/* For a prototype instantiation this is TRUE
			   if any of its base classes are nonreal classes. */
} a_class_symbol_supplement;


/* Unique sequence number identifying a declaration in a given scope. */
typedef unsigned long a_decl_sequence_number;
  
/*
Data structure used to pass information about function declarations back
from the scanning of the function declarator.
*/
typedef struct a_param_id *a_param_id_ptr;
typedef struct a_param_id {
  /* Entry giving the name of one parameter in a function declarator.
     The type of the parameter does not appear here; it is in an
     entry of type a_param_type attached to the type entry for the
     function.  The present structure is used both for old-style
     identifier lists and for the names of parameters in prototypes. */
  a_param_id_ptr
		next;
			/* Next parameter id on the list, or NULL if this
			   is the last parameter id. */
  a_symbol_ptr	symbol;
			/* Points to an sk_parameter symbol to represent a
			   parameter name.  It is NULL when a name is omitted
			   in a function prototype.  The symbol pointed to,
			   when present, is transformed into an sk_variable
			   symbol as part part of function definition
			   processing. */
  a_type_ptr	type;
			/* For a new- or old-style function parameter, this
			   is its type.  This is usually the same as the
			   information in the function type parameter list,
			   but is kept here also so we can be sure of
			   associating the proper identifier and type
			   in error cases. */
  a_source_position
		type_pos;
			/* Source position of the start of the type
			   specification of the parameter declaration. */
  a_storage_class
		storage_class;
			/* For a new- or old-style function parameter, this is
			   the storage class to be associated with it when it
			   is declared. */
  a_byte_boolean
		implicitly_declared;
			/* TRUE for an old-style parameter that for which
			   an explicit declaration is omitted. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		source_sequence_entry;
			/* Source-sequence information saved during declarator
			   processing. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
} a_param_id;


typedef struct a_func_info_block *a_func_info_block_ptr;
typedef struct a_func_info_block {
  /* Information about the parameter list in a function declarator. */
  a_symbol_ptr	prototype_scope_symbols;
			/* List of symbols in the prototype scope, linked
			   on the next_in_scope field.  NULL if none.
			   Usually NULL.  Only named types (structs/unions/
			   enums) declared within the prototype scope
			   appear on this list. */
  a_param_id_ptr
		param_id_list;
			/* List of entries giving parameter names, NULL if
			   there were none.  Used for both old-style and
			   new-style parameter names. */
  an_exception_specification_ptr
		exception_specification;
			/* An entry (or list of entries) representing an
			   exception specification (C++ only). */
  a_source_position
		throw_position;
			/* Source position of the exception specification (C++
			   only). */
  a_scope_number
		scope_number;
			/* The scope number used for the function prototype
			   scope for the parameters, to be reused for the
			   function scope if a body is found. */
  unsigned int  any_prototype_names_omitted:1;
			/* TRUE if the parameter list is a prototype list,
			   and it includes at least one parameter with
			   just a type and no name. */
  unsigned int  is_inline:1;
			/* TRUE if inline was specified (C++ only). */
  unsigned int  is_definition:1;
			/* TRUE if the current declaration is a definition. */
  unsigned int  is_main_function:1;
			/* TRUE if the function "main". */
  unsigned int  is_implicit_declaration:1;
			/* TRUE if this is an implicit declaration. */
  unsigned int	function_type_from_typedef:1;
			/* TRUE if the function type came from a typedef
			   rather than from the declarator.  When it is TRUE,
			   an error will be issued on a function definition
			   and param_id_list and prototype_scope_symbols will
			   be NULL. */
  unsigned int	any_default_args:1;
			/* TRUE if the function type declaration included
			   the declarations of default arguments. */
#if ASM_FUNCTION_ALLOWED
  unsigned int	is_asm_function:1;
			/* TRUE if the function type declaration included the
			   asm specifier. */
#endif /* ASM_FUNCTION_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		declarator_ssep;
			/* Source sequence entry for the function
			   declarator. */
  a_source_sequence_entry_ptr
		prototype_scope_ss_entry_start;
			/* Pointer to a file-scope source sequence entry that
			   immediately precedes the first entry generated for
			   declarations in the function prototype scope; NULL
			   indicates that the first function prototype entry
			   is also the first on the file-scope list.  (Also
			   NULL if param_id_list is NULL.) */
  a_source_sequence_entry_ptr
		prototype_scope_ss_entry_end;
			/* Pointer to a file-scope source sequence entry that
			   is the last entry generated for declarations in the
                           function prototype scope; NULL if there no entries
                           and prototype_scope_ss_entry_end is also NULL. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
} a_func_info_block;


typedef struct a_template_param *a_template_param_ptr;
typedef struct a_template_param {
  /* Information describing a template formal parameter.  Pointed to by the
     template symbol supplement. */
  a_template_param_ptr
                next;
                        /* Pointer to the next template parameter. */
  a_symbol_ptr	param_symbol;
			/* Symbol entry for a formal parameters of the
                           template. */
  a_token_cache	token_cache;
			/* Contains the cached tokens that comprise the
			   template parameter declaration.  Used to
			   create the parameter types for instances of
			   the class template when the parameter type
			   depends on other template parameters. */
  union {
    /* When param_symbol->kind = sk_type. */
    a_type_ptr  param_type;
                        /* Type entry for a formal parameter.  A unique type
                           entry is created for each template type
                           parameter. */
    /* When param_symbol->kind = sk_constant. */
    struct {
      a_constant_ptr
		ptr;
			/* Constant entry for a formal parameter.  A unique
			   constant entry is created for each template constant
			   parameter. */
      unsigned int
		has_default_arg:1;
			/* TRUE if a default argument has been declared for
			   this parameter. */
      unsigned int
		type_involves_template_param:1;
			/* TRUE if the type entry associated with the
			   parameter constant involves (anywhere in its
			   type tree) a tk_template_param type entry. */
      unsigned int
		constant_involves_template_param:1;
			/* TRUE if the default argument expression contains
			   a ck_template_param. */
#if CHECKING
      unsigned int
		dummy:2;
			/* Extra field that can be initialized to prevent
			   spurious reference to uninitialized data warnings
			   from CodeCenter. */
#endif /* CHECKING */
      union {
        /* When type_involves_template_param and
           constant_involves_template_param are FALSE. */
        a_constant_ptr
		constant;
			/* Constant containing the default value
			   to be used as the actual argument of an
		           instantiation when the actual argument
			   corresponding to this parameter is omitted. */
        /* When type_involves_template_param or
           constant_involves_template_param is TRUE. */
        a_token_cache
		token_cache;
			/* Header of the token cache that contains the
			   tokens of the default argument expression. */
      } default_arg;
    } param_constant;
  } variant;
} a_template_param;


typedef struct a_template_symbol_supplement *a_template_symbol_supplement_ptr;

typedef struct a_template_instance *a_template_instance_ptr;
typedef struct a_template_instance {
  /* Information describing an instance of a function template or an
     instance of a member function or static data member of a template class.
     The kind of instance may be derived from instance_sym->kind. */
  a_template_instance_ptr
                next;
                        /* Pointer to the next instance of a given template. */
  a_template_instance_ptr
                next_in_instantiation_list;
                        /* Pointer to the next instance in a list of
			   entries for which full instantiation is required. */
  a_symbol_ptr  instance_sym;
                        /* Pointer to the sk_routine, sk_member_function, or
			   sk_static_data_member symbol entry that describes
			   this template instance.  Note that the instance
			   may not be a "real" instance when it is a member
			   of a prototype instantiation of a class template. */
  a_symbol_ptr  template_sym;
			/* For nonmember functions, a pointer to the
			   sk_function_template of which it is an instance.
			   For member functions, a pointer to the
			   sk_member_function entry of the class template's
			   prototype instantiation.  For static data members,
			   a pointer to the sk_static_data_member symbol of
			   the prototype instantiation.  (Note that
                           template_sym == instance_sym when instance_sym is
			   a member of prototype instantiation; when this is
			   the case template_info is non-NULL.) */
  a_template_arg_ptr
                arg_list;
                        /* Pointer to the template argument list -- the
                           arguments that correspond to the template
                           parameter list (e.g., template <class T>). */
  a_template_symbol_supplement_ptr
		template_info;
			/* Pointer to associated template information when
			   instance_sym points to a member of a prototype
			   instantiation of a class template (in which case
			   instance_sym == template_sym and the instance is
			   not a "real" instance but a kind of template for a
			   member function or a static data member).  Otherwise
			   (i.e., usually) NULL. */
  unsigned int  instantiation_required:1;
			/* TRUE if a routine body or static data member
			   definition needs to be generated for this instance.
			   This flag is FALSE if an explicit definition has
			   been provided by the user (i.e., if specific_def
			   is set). */
  unsigned int  specific_decl:1;
			/* For instances of nonmember function templates,
			   TRUE if this instance has been explicitly declared
			   (in which case, instance_sym has been added to the
			   overload list for this name).  Always TRUE (and
			   therefore meaningless) for member functions and
			   static data members of template classes. */
  unsigned int  specific_def:1;
			/* For instances of nonmember function templates and
			   member functions of template classes, TRUE if this
			   instance has been explicitly defined (in which case
			   no implicit instantiation will be done). The
			   specific_decl flag will always be TRUE when this
			   flag is set.  For static data members its value is
			   identical to the defined flag in instance_sym.
                           specific_def is also set TRUE for entities
			   whose instantiations have been suppressed using
			   a do_not_instantiate pragma. */
  unsigned int	explicit_instantiation:1;
			/* TRUE if an instantiation has been explicitly
			   requested using a pragma directive. */
  unsigned int  already_instantiated:1;
			/* TRUE if instantiation has already been performed
			   (for instance, for inline functions, which are
			   instantiated at the point of first reference). */
  unsigned int	explicit_do_not_instantiate:1;
			/* TRUE if instantiation has been explicitly 
			   suppressed by a do_not_instantiate pragma. */
  unsigned int	explicit_can_instantiate:1;
			/* TRUE if instantiation has been explicitly declared
                           as being possible by a can_instantiate pragma. */
#if AUTOMATIC_TEMPLATE_INSTANTIATION
  unsigned int	in_info_file:1;
			/* TRUE if the instance was listed in the instantiation
			   information file as an instantiation assigned to
			   this compilation. */
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */
  a_source_position
		explicit_instantiation_pos;
			/* The position of the instantiation request pragma
			   when explicit_instantiation is TRUE. */
} a_template_instance;


/* Used to track the number of pending instantiations of a given class. */
typedef short a_pending_instantiation_count;

/* Used to track the number of instantiations performed in tim_all mode that
   were not actually required. */
typedef short an_unused_instantiation_count;


typedef struct a_template_symbol_supplement {
  /* Additional information about a C++ class or function template
     supplementing the information residing in the class's symbol entry. */
  a_template_param_ptr
                parameters;
			/* Symbol entries for formal parameters of the
                           template. */
  a_token_cache token_cache;
                        /* The tokens comprising the template are cached
                           in order to be rescanned later during
                           instantiation.  Typically begins with the left
			   brace that begins the class or function body and
                           extends to the right brace; for constructors it
			   may begin at a colon.  For templates for static
			   data members it embraces the initializer
			   expression, if any. */
  a_scope_number
                declaration_scope;
                        /* The scope number assigned when the template
                           declaration is processed.  This scope needs
                           to be used at instantiation for symbol lookup
                           to work properly. */
  a_pending_instantiation_count
		pending_instantiations;
			/* The number of instantiations of this template
			   that are in the process of being instantiated.
			   Used to detect runaway recursive instantiations. */
  a_pending_pragma_ptr
		pragmas_bound_to_template;
			/* A list of pbk_next_construct pragmas to be bound
			   to each instance generated from this template. */
  a_token_sequence_number
		token_sequence_number;
			/* This is used for member functions and static
			   data members to match the declarations of
			   the prototype instantiation (to which the
			   template symbol supplement is attached) to
			   declarations found inside real instantiations.
			   This field contains the token sequence number
			   of a certain token within the declaration. */
  a_class_list_entry_ptr
                befriending_classes;
                        /* A linked list of entries identifying classes
			   that have declared the current class a friend
			   (i.e., classes that have befriended the this
			   template).  If the template friend declaration
			   appears as part of a class template definition,
			   a new entry will be added to this list for
			   each class instantiated from the class
			   template. */
  union {
    /* When symbol kind = sk_class_template: */
    struct {
      a_symbol_ptr
                instantiations;
                        /* Pointer to a list of symbols describing template
                           classes that have been instantiated from this
                           class template. */
      a_type_kind
		type_kind;
			/* The kind (tk_class, tk_struct, or tk_union) which
			   the instantiated types will have. */
      a_symbol_ptr
		member_function_templates;
			/* Pointer to a linked list of sk_function_template
			   symbols representing member functions whose bodies
			   are defined outside the class template declaration.
			   The are linked by next pointers and thus are
			   not actually in the symbol table. */
      a_symbol_ptr
		prototype_instantiation;
			/* Points to the symbol representing the prototype
			   instantiation.  The prototype instantiation is
			   also on the instantiations list above. */
      unsigned int
		prototype_instantiation_complete:1;
			/* TRUE when the prototype instantiation of the
			   class template has been completed.  Used to
			   prevent a real instantiation from occurring while
			   the prototype instantiation is in progress. */
#if CHECKING
      unsigned int
		dummy:2;
			/* Extra field that can be initialized to prevent
			   spurious reference to uninitialized data warnings
			   from CodeCenter. */
#endif /* CHECKING */
    } class_template;
    /* When symbol kind = sk_function_template: */
    struct {
      a_template_instance_ptr
                instantiations;
                        /* Pointer to a list of entries describing template
                           functions that have been instantiated from this
                           function template. */
      a_routine_ptr
                routine;
                        /* Points to a routine entry for the function
                           template.  This is needed for function
                           matching. */
      a_func_info_block
		func_info;
			/* Information about the prototype parameters
			   in a function template declaration (the function
			   parameters not the template parameters). */
      a_def_arg_expr_fixup_ptr
		def_arg_expr_list;
			/* List of entries describing default argument
			   expressions associated with parameters for
			   this template declaration. */
      a_token_cache
		decl_token_cache;
			/* A cache of the tokens that comprise the function
			   declaration.  These are rescanned later to create
			   routine types for instances of the function
			   template.  The cache begins with the first token
			   of the function declaration (the token after the
			   closing ">" of the template parameter list) and
			   ends with the last token of the function
			   declarator. */
      unsigned int
		cannot_be_called:1;
			/* TRUE if this function cannot be called because
			   not all of the template parameters were used
			   in function parameter types or were used only
			   in function parameters that have default values. */
#if CHECKING
      unsigned int
		dummy:2;
			/* Extra field that can be initialized to prevent
			   spurious reference to uninitialized data warnings
			   from CodeCenter. */
#endif /* CHECKING */
      an_unused_instantiation_count
		unused_instantiations;
			/* When a function is added to the instantiations
			   required list in tim_all mode but is not actually
			   required, it is not instantiated until instantiation
			   wrapup is done, even if it is an inline function.
			   This is done because these functions may be put
			   on the list before they can actually be
			   instantiated.  Consequently, the runaway recursive
			   instantiation check will not detect a loop in which
			   new "unused" entries get added while instantiating
			   earlier "unused" entries.  To prevent such loops
			   we set an arbitrary limit to the number of unused
			   instantiations that can be generated for a given
			   function.  This field records the number of unused
			   instantiations that have been performed so far. */
      a_type_ptr
	        class_declared_in;
                        /* This field is used for template friend declarations
			   that appear inside class definitions.  It points
			   to the class in which the template declaration was
			   found.  When instantiating the function the
			   class scope must be reactivated.  If the class is
			   a template instance, the template parameters must
			   also be reactivated. */
    } function;
    /* When symbol kind = sk_static_data_member: */
    struct {
      a_template_instance_ptr
		definitions;
			/* Pointer to a list of entries specifying definitions
			   for static data members of instantiated template
			   classes. */
    } static_data_member;
  } variant;
} a_template_symbol_supplement;


typedef struct an_extern_symbol_descr *an_extern_symbol_descr_ptr;
typedef struct an_extern_symbol_descr {
  /* Information on an sk_extern_variable or sk_extern_routine entry, i.e.,
     an external symbol.  Such an entry is created for each name with
     linkage (external or internal) as a place to hold the pointer to the
     unique IL entry for that variable or function. */
  a_type_ptr	type;
			/* The full type for the entry.  May differ from the
			   type in the variable or routine for the symbol
			   in that the type here is the full composite type
			   of all declarations seen so far, while the type
			   in the variable or routine is compatible with
			   the full composite type but may have only a subset
			   of the information.  For example, the type here
			   could be "int [5]" while the type in the variable
			   entry is "int []". */
  union {
    /* When symbol kind == sk_extern_variable: */
    a_variable_ptr
		variable;
    struct {
      /* When symbol kind == sk_extern_routine: */
      a_routine_ptr
		ptr;
      a_byte_boolean
                is_implicit_declaration;
                        /* TRUE if this external routine has only been
			   declared implicitly. */
    } routine;
  } variant;
} an_extern_symbol_descr;


typedef struct a_projection_descr *a_projection_descr_ptr;
typedef struct a_projection_descr {
  /* Description of the projection of a base class member symbol into
     a derived class.  Pointed to by an sk_projection symbol. */
  a_symbol_ptr  fundamental_symbol;
			/* The fundamental base class member to which this
			   projection symbol refers, i.e., the symbol for
			   the definition of the entity rather than any
			   inherited instance of it. */
  a_base_class_ptr
		fundamental_base_class;
			/* This field is a pointer to the base class entry for
			   the entity represented by fundamental_symbol.  It
			   will be a base class entry on the current class's
			   base classes list, and its derivation
			   specifies the path between the current class object
			   and the member specified by fundamental_symbol. */
} a_projection_descr;


typedef struct a_symbol {
  /* A symbol as used by the front end. */
  /* If you change this structure, be sure to also change the initialization
     of global variable cleared_symbol in symbol_tbl_one_time_init. */
  a_symbol_header_ptr
		header;
			/* Pointer back to the header which contains a list of
			   symbols of which this is one.  This helps
			   when this symbol needs to be removed from the symbol
			   table.  The header also gives the symbol identifier
			   string. */
  a_symbol_ptr  next;
    			/* The next symbol in this list with the same
			   identifier.  This field is NULL if this is the
			   last symbol with this identifier. */
  a_symbol_ptr	next_in_scope;
			/* When the symbol is in the symbol table, this
			   points to the next symbol in the same scope. */
  a_scope_number
		decl_scope;
			/* Scope number of the scope in which this symbol
			   was declared. */
  a_decl_sequence_number
		decl_seq;
			/* A number (> 0) that, within the declaration scope,
			   uniquely identifies the declaration associated with
			   this symbol.  The numbers are assigned sequentially,
			   so that a symbol with a higher decl_seq value was
			   declared after one with a lower number. */
  a_source_position
		decl_position;
			/* Source position of the declaration of this
			   symbol. */
  a_type_ptr	class_of_which_a_member;
			/* For a symbol that is a class member, this points
			   to the class type (this includes structs/unions
			   when compiling C); NULL otherwise. */
  a_symbol_kind kind;
			/* The kind of symbol. */
  unsigned int	referenced:1;
			/* TRUE if the symbol is actually referenced, not just
			   declared. */
  unsigned int	defined:1;
			/* TRUE if the symbol is actually defined, not just
			   declared. */
  unsigned int  explicit_linkage_specifier:1;
			/* TRUE for variables and routines for which an
			   explicit external linkage was specified (e.g.,
			   ``extern "C"'' -- C++ only). */
  unsigned int  reentered_from_prototype_scope:1;
			/* TRUE if symbol was originally declared in a
			   function prototype scope and was subsequently
			   reentered in the function scope. */
  unsigned int  is_error:1;
			/* TRUE if the symbol represents an identifier for
			   which an error has been diagnosed and which should
			   not be entered into the symbol table. */
  unsigned int	is_template_param:1;
			/* TRUE if the symbol represent a template
			   parameter. */
  unsigned int  template_param_not_visible:1;
			/* TRUE if this is a template parameter that should
			   not be visible for name lookup purposes at this
			   point in time. */
  unsigned int	force_external_linkage:1;
			/* TRUE if this is a class or enum type that has been
			   used in a way that would force external linkage (if
			   it has linkage at all). */
  union {
    /* When kind == sk_undefined, no variant fields. */
    /* When kind == sk_keyword: */
    struct {
      a_token_kind
		token;
			/* For keywords, the token identifying the keyword. */
      an_error_code
		diagnostic_issued_if_used;
			/* The error code of a diagnostic to be issued
			   the first time that this keyword is used.
			   The error code is replaced with ec_no_error
			   after the diagnostic has been issued. */
    } keyword;
    /* When kind == sk_macro: */
    a_macro_def_ptr
		macro_def;
			/* A structure defining the macro. */
    /* When kind == sk_constant: */
    a_constant_ptr
		constant;
			/* The value of the constant. */
    /* When kind == sk_type or sk_enum_tag: */
    a_type_ptr	type;
			/* The type. */
    /* When kind == sk_class_or_struct_tag or sk_union_tag: */
    struct {
      a_type_ptr
		type;
			/* The type. */
      a_class_symbol_supplement_ptr
		extra_info;
			/* Pointer to an entry providing additional info about
			   a C++ class. */
    } class_struct_union;
    /* When kind == sk_variable: */
    struct {
      a_variable_ptr
		ptr;
			/* Pointer to the variable entry. */
      unsigned int
		value_has_been_set:1;
			/* TRUE if the variable was initialized (explicitly or
			   implicitly), has been assigned to, or has had its
			   address taken.  Also TRUE if it is of aggregate
			   type and at least one of its fields or elements has
			   been assigned to or has had its address taken.
			   Also TRUE if its storage class is extern, since its
			   value will be set where in the definition. */
      unsigned int
		used:1;
			/* TRUE if the variable was directly used or had
			   its address taken. */
    } variable;
    /* When kind == sk_static_data_member: */
    struct {
      a_variable_ptr
		variable;
			/* Pointer to the variable entry. */
      a_template_instance_ptr
                instance_ptr;
			/* For a symbol that represents a static data member
			   of a (real or prototype) instantiation of a class
			   template, a pointer to an entry providing
			   additional information about whether and how to
			   define the static data member.  NULL otherwise. */
    } static_data_member;
    /* When kind == sk_field: */
    struct {
      a_field_ptr
		ptr;
			/* The field. */
      a_symbol_ptr
		anonymous_parent_object;
			/* If this field is a member of an anonymous union,
			   a pointer to the symbol for the (unnamed) variable
			   or field it is associated with; otherwise NULL.
			   Note that the symbol for an anonymous union member
			   is "promoted" into the scope of its parent entity,
			   so that this is a way to get at the intervening
			   anonymous structure(s) it belongs to. */
    } field;
    /* When kind == sk_routine or sk_member_function: */
    struct {
      a_routine_ptr 
                ptr;
			/* The routine. */
      a_template_instance_ptr
                instance_ptr;
                        /* Present for template functions and member functions
                           of template classes.  Points to information about
                           the particular instance of the function. */
    } routine;
    /* When kind == sk_label: */
    struct {
      a_label_ptr
		ptr;
			/* The label. */
      struct a_control_flow_descr
		*assoc_control_flow_descr;
			/* When the label has been referenced in one or more
			   goto statements but has not yet been defined,
			   pointer to a list of entries identifying the
			   references; and when the label has been defined,
			   a pointer to an entry representing the label
			   statement itself. */
    } label;
    /* When kind == sk_extern_variable or sk_extern_routine: */
    an_extern_symbol_descr_ptr
		extern_symbol_descr;
			/* Information on the external symbol. */
    /* When kind == sk_projection: */
    struct {
      a_projection_descr_ptr
		extra_info;
			/* Additional information about the projection. */
      unsigned int /*an_access_specifier*/
		access:2;
			/* Access to this symbol in the scope of the derived
			   class.  This may differ from the access with
			   which it was originally declared in its base
			   class. */
      unsigned int
		ambiguous:1;
			/* TRUE if the symbol name is ambiguous in
			   the current scope, i.e., another symbol with the
			   same name is visible, and there is no reason to
			   prefer one over the other. */
      unsigned int
		access_adjustment_made:1;
			/* If TRUE an access declaration has been made for
			   the projection symbol, in which case it cannot
			   be overridden by a local symbol of the same name. */
      unsigned int
		intervening_access_adjustment:1;
			/* TRUE if the access of the inherited name was
			   modified by an access declaration anywhere on the
			   derivation path between the fundamental symbol and
			   the current projection. */
    } projection;
    /* When kind = sk_overloaded_function: */
    struct {
      a_symbol_ptr
		symbols;
			/* Pointer to two or more sk_member_function or
			   sk_routine symbol entries that represent instances
			   of an overloaded function name. */
      unsigned int
		mixed_static_nonstatic:1;
			/* TRUE if some but not all the functions have been
			   declared "static"; applies to sk_member_function
			   overloading only. */
    } overloaded_function;
    /* When kind == sk_parameter: */
    a_param_id_ptr
		param_id;
			/* Pointer to the param_id entry with which this
			   symbol is associated. */
    /* When kind = sk_class_template or sk_function_template: */
    a_template_symbol_supplement_ptr
                template_info;
			/* Pointer to an entry providing additional info about
			   a C++ class template or function template. */
  } variant;
} a_symbol;

typedef struct a_symbol_header {
  /* This is the container for information that the symbol table
     management routines use in manipulating a list of symbols that have
     the same name. */
  a_symbol_header_ptr
		next;
			/* This is the pointer to the next symbol header in the
			   same bucket of the symbol table.  This field is NULL
			   if this is the last symbol header in this bucket. */
  char		*identifier;
			/* A pointer to a null-terminated string containing the
			   name of the symbol. */
  sizeof_t	identifier_length;
			/* The length of the identifier, not counting the
			   final null. */
  a_symbol_ptr	symbol;
			/* This is the pointer to a symbol table entry.  This
			   is actually a list of all symbols with the same
			   identifier. */
  a_symbol_ptr	inactive_symbols;
			/* A list of symbols that are currently inactive
			   but can be reached with some sort of qualification,
			   i.e., members of structs/unions/classes. */
  unsigned int  any_nested_types_on_inactive_list:1;
			/* TRUE if a symbol for a nested type has been
                           transferred to the inactive list.  This field is
                           used to speed up processing to support the
                           nested class anachronism (ARM 18.3.5) and is
                           only set when anachronisms are allowed. */
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  unsigned int  has_cfront_transitional_nested_type_mangled_name:1;
                        /* TRUE if a nested type has been flagged for
                           special handling during name mangling.  The first
                           nested type with a given name will have this flag
                           set indicating that its name should be mangled as
                           if it were not a nested type. */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
} a_symbol_header;


#define SYMBOL_TABLE_SIZE 599
	  		/* The number of buckets in the symbol table.  This
			    number should be prime. */

/*
Top level structure for the hash-table portion of the symbol table.  Each
bucket of the array contains a pointer to a list of symbol headers whose
identifiers hash to that bucket.  (Other portions of the symbol table,
defined for C++ only, are the opname_symbol_table, for accessing operator
functions by operator, and the conversion_header_list, for accessing
conversion functions by destination type.)  
*/
EXTERN a_symbol_header_ptr
		symbol_table[SYMBOL_TABLE_SIZE];

/*
Table of pointers to symbol headers for C++ operator name symbols, for
names like "operator+".  Indexed by opname kind.
*/
EXTERN a_symbol_header_ptr
		opname_symbol_table[(int)onk_last];

typedef struct a_conversion_header *a_conversion_header_ptr;
typedef struct a_conversion_header {
  /* Top level lookup mechanism for symbols that identify user defined
     conversion functions.  Since such symbols are looked up by return
     type, they do not appear in the symbol proper.  Each conversion header
     entry points to a symbol header that points to symbols for all the
     user-defined conversion functions that return objects of a given type. */
  a_conversion_header_ptr
		next;
			/* Next in a linked list of conversion header
			   entries; NULL for the last entry on the list. */
  a_symbol_header_ptr
		symbol_header;
			/* Pointer to the symbol header pointing to conversion
			   functions whose destination type is "type". */
  a_type_ptr	type;
			/* Pointer to the type entry by which the symbol
			   header is looked up. */
} a_conversion_header;

/*
List of conversion header entries that serve as a lookup list for conversion
function symbols.
*/
EXTERN a_conversion_header_ptr
		conversion_header_list;

/*
Symbol information related to the current token:
*/
EXTERN a_symbol_locator
		locator_for_curr_id;
			/* If curr_token == tok_identifier, this is information
			   fully specifying the identifier. */

typedef struct an_extern_type_fixup *an_extern_type_fixup_ptr;
typedef struct an_extern_type_fixup {
  /* Entry on a list indicating variables and routines whose types must
     be reset to an earlier state at the end of a scope.  This is
     needed because there is only one IL entry for a variable or routine
     even though there may be several symbols at different scope levels
     with varying visibility of the overall type of the IL entry.
     For example,
       int a[];
       main () {
         extern int a[5];
         ... Type of "a" is now "int [5]".
       }
       ... Type of "a" must be restored to "int []" at the end of "main".
  */
  an_extern_type_fixup_ptr
		next;	/* Pointer to the next fixup entry on the list for
			   the same scope. */
  a_type_ptr	type;	/* The type to be restored. */
  a_boolean	is_routine;
			/* TRUE for routine, FALSE for variable. */
  union {
    /* When is_routine == FALSE: */
    a_variable_ptr
		variable;
			/* The variable whose type is to be changed. */
    /* When is_routine == TRUE: */
    a_routine_ptr
		routine;
			/* The routine whose type is to be changed. */
  } variant;
} an_extern_type_fixup;


/* Contains a description of an access error that has been detected
   for which an error may need to be issued later. */
typedef struct an_access_error_descr *an_access_error_descr_ptr;
typedef struct an_access_error_descr {
  an_access_error_descr_ptr
		next;	/* Pointer to the next error description record. */
  struct a_symbol	
		*sym;
			/* Symbol that the program was trying to access
			   that should be included in the error message. */
  a_source_position
		position;
			/* Position to be used when the error is issued. */
  a_token_sequence_number
		token_sequence_number;
			/* Token sequence number of the current token when the
			   access error was first detected. */
} an_access_error_descr;


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
  unsigned int /*an_access_specifier*/
		current_access:2;
			/* The access control specification that currently
			   prevails for declarations in the current scope;
			   as_public by default, but may be otherwise for
			   C++ class definitions.  (For instance, if an
                           enumeration is defined as a member type of a class,
			   the access to be applied to the enumeration
			   constants may be derived from the setting of this
			   field.) */
  unsigned int	inactive_symbols_may_be_visible:1;
			/* TRUE if the scope stack to this depth contains any
			   class reactivation entries or class entries for
			   classes with base classes.  In either case,
			   symbols on a symbol header's inactive list may be
			   visible from the current scope. */
  unsigned int  inside_local_class:1;
			/* TRUE if the current scope level is that of a local
			   class or is (logically) within the scope of a local
			   class.  Once this flag is set it is usually
			   propagated each time a new scope is pushed onto
			   the stack; the exception is when a template
			   instantiation scope is pushed, in which case the
			   flag is cleared. */
  unsigned int	template_param_decl_scope:1;
			/* TRUE if this is the first scope that
			   affects the declarative level after a template
			   instantiation scope. */
  unsigned int	is_loop_scope:1;
			/* TRUE if this scope is associated with the compound
			   statement of a for, do, or while loop. */
  unsigned int	slow_lookup_required:1;
			/* TRUE if this is a scope for which a slow lookup
			   is required because the scope stack contains a
			   scope in which certain symbols on the active list
			   must not be visible. */
  unsigned int	return_value_optimization_possible:1;
			/* TRUE if this scope is a function scope and return
			   value optimization is possible for the routine.
			   That is, the routine returns a class value via
			   a copy constructor, and all return statements
			   return a single local variable. */
  unsigned int	in_prototype_instantiation:1;
			/* TRUE if kind is sck_template_instantiation and
			   what is being instantiated is the prototype for a
			   class template. */
  unsigned int	defer_access_checks:1;
			/* TRUE while scanning the decl-specifiers and
			   declarator of a global or namespace-level
                           declaration.  Access checks for names
			   scanned while this is TRUE cannot be done
			   until the declarator has been scanned. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  unsigned int	source_sequence_entries_disallowed:1;
			/* TRUE if the current scope establishes or belongs to
			   a context in which source sequence entries should
			   not be issued -- e.g. a template declaration, a
			   a template instantiation, or a pragma. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  unsigned int  nested_instantiation:1;
                        /* TRUE for a template instantiation scope that
			   is expected to be nested inside of another
			   instantiation scope.  This occurs when a friend
			   template declaration from a class template is
			   being instantiated.  This flag lets name lookups
			   continue on past the nested instantiation scope so
			   that names from the outer instantiation scope can
			   be visible. */
  unsigned int	is_try_block:1;
			/* TRUE if the scope is that of the compound statement
			   of a try block (sck_block only).  Note: not set
			   for the scope pushed for a catch clause. */
  unsigned int	within_try_block:1;
			/* TRUE if is_try_block is TRUE or if this scope is
			   an sck_block scope nested within a scope for which
			   is_try_block is set. */
  a_symbol_ptr	symbols,
		last_symbol;
			/* First/last pointers to the list of all symbols
			   declared in this scope, linked by the field
			   next_in_scope. */
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
  an_extern_type_fixup_ptr
		extern_type_fixup_list;
			/* List of types of variables and routines to be
			   reset at the end of the scope.  Used when
			   inner- and outer-scope declarations of entities
			   with linkage have compatible but not identical
			   types, and the outer-scope type must be restored
			   at the end of the inner scope. */
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
  a_variable_ptr
		last_nonstatic_variable;
			/* End of list of nonstatic local variables of this
			   scope, NULL if none. */
  a_label_ptr	last_label;
			/* End of list of local labels of this scope, NULL
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
  a_dynamic_init_ptr
		last_dynamic_init;
			/* End of list of local dynamic initializations, NULL
			   if none. */
  a_pragma_ptr	last_pragma;
			/* End of list of IL pragma entries entered on the
			   pragma_list of il_scope, NULL if none. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		last_source_sequence_entry;
			/* For file and function scopes, the last in the
			   linked list of source sequence entries that are
			   pointed to by il_scope; NULL if none. */
  a_source_sequence_entry_ptr
		source_sequence_avail_list;
			/* List of freed source sequence entries that are
			   available for reuse; NULL if none. */
  a_src_seq_sublist_ptr
		last_src_seq_sublist;
			/* For function scopes, the last in the linked list
			   of source sequence sublist entries that are pointed
			   to by il_scope; NULL if none. */
  a_scope_depth depth_innermost_ss_list_scope;
			/* Depth of the innermost scope on the scope stack
			   with a source sequence list (= DEPTH_OF_FILE_SCOPE
			   or depth_innermost_function_scope). */
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
		saved_last_ss_entry;
			/* If kind == sck_template_instantiation, the current
			   value of last_source_sequence_entry in the file
			   scope when push_scope is called and to which that
			   pointer is restored by pop_scope.  Not used for
			   any other scope kinds. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if RECORD_HIDDEN_NAMES_IN_IL
  a_hidden_name_ptr
		last_hidden_name;
			/* End of the list of hidden-name entries entered on
			   the corresponding IL scope entry; NULL if none. */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
#if RECORD_TEMPLATES_IN_IL
  a_template_ptr
		last_template;
			/* End of the list of template entries entered on
			   the corresponding IL scope entry; NULL if none. */
#endif /* RECORD_TEMPLATES_IN_IL */
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
  a_template_param_ptr
		template_param_list;
                        /* When kind == sck_template_instantiation, contains
			   a pointer to the template parameter list. */
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
} a_scope_stack_entry;


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
EXTERN a_scope_depth
		depth_innermost_ss_list_scope;
			/* Depth of the innermost scope on the scope stack
			   with a source sequence list (= DEPTH_OF_FILE_SCOPE
			   or depth_innermost_function_scope). */
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
EXTERN a_scope_number
		next_scope_number;
			/* Next scope number to be assigned.  These are
			   unique identifiers for each scope, not just
			   the scope nesting depth.  Also used for the
			   pseudo-scopes associated with the members of
			   structs and unions in C (not C++). */

#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
EXTERN a_symbol_ptr
		last_ctor_or_dtor_sym;
			/* The last constructor or destructor
			   with a definition outside of a class
			   declaration.  Used only in cfront
			   compatibility mode to emulate a cfront
			   name lookup bug. */
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */


extern a_symbol_ptr find_symbol(char             *identifier,
			        sizeof_t         identifier_length,
				a_symbol_locator *location);

extern a_boolean find_projected_symbol(a_type_ptr        class_ptr,
                                       a_symbol_locator  *locator,
                                       a_boolean         must_be_tag,
                                       a_boolean         must_be_type_name,
                                       a_boolean         add_to_active_list,
                                       a_symbol_ptr      insert_sym,
                                       a_symbol_ptr      *projected_symbol);

extern void make_locator_for_symbol(a_symbol_ptr     sym_ptr,
                                    a_symbol_locator *location);

extern void make_specific_symbol_error_locator(a_symbol_locator *locator);

extern a_template_symbol_supplement_ptr alloc_template_symbol_supplement(
                                                         a_symbol_kind  kind);

extern a_symbol_ptr enter_symbol(a_symbol_kind    sym_kind,
				 a_symbol_locator *location,
                                 a_scope_depth    scope_depth,
                                 a_boolean        suppress_error);

extern void reenter_symbol(a_symbol_ptr     symbol_to_reenter,
                           a_scope_depth    scope_depth,
                           a_boolean        suppress_error);

extern void reactivate_prototype_scope_symbols(
                                        a_symbol_ptr  prototype_scope_symbols);

extern void relink_unnamed_class_symbol(a_symbol_ptr      sym,
                                        a_symbol_locator  *locator);

extern a_symbol_ptr add_symbol_to_overload_list(a_symbol_ptr  new_sym,
                                                a_symbol_ptr  other_sym);

extern a_symbol_ptr enter_overloaded_symbol(a_symbol_kind    sym_kind,
                                            a_symbol_locator *location,
                                            a_symbol_ptr     old_sym_ptr,
                                            a_symbol_ptr     *overload_sym);

extern a_symbol_ptr make_projection_symbol(a_symbol_ptr      progenitor_sym,
                                           a_type_ptr        class_ptr,
                                           a_base_class_ptr  fundamental_bcp,
                                           a_derivation_step *path,
                                           a_boolean         ambiguous);

extern a_symbol_ptr make_template_class_symbol(a_symbol_ptr       ct_symbol,
                                               a_source_position *pos);

extern a_symbol_ptr make_template_function_symbol(a_symbol_ptr       templ_sym,
                                                  a_source_position  *pos);

extern a_symbol_ptr get_member_function_template_symbol(a_symbol_ptr rout_sym);

extern a_symbol_ptr make_unnamed_class_symbol(a_symbol_kind      sym_kind,
                                              a_source_position  *pos);

extern a_boolean is_unnamed_class_symbol(a_symbol_ptr  sym);

extern a_symbol_ptr unnamed_field_symbol(void);

extern a_symbol_ptr make_anonymous_parent_object_symbol(
                                                a_symbol_kind      kind,
                                                a_source_position  *pos,
                                                a_scope_number     decl_scope);

extern a_symbol_ptr full_enter_symbol(char          *identifier,
				      sizeof_t      identifier_length,
				      a_symbol_kind sym_kind,
				      a_scope_depth scope_depth);

extern void set_symbol_kind(a_symbol_ptr  sym_ptr,
			    a_symbol_kind sym_kind);

extern void remove_symbol(a_symbol_ptr sym_ptr);

extern void remove_anonymous_union_member_from_inactive_symbols_list
                                                       (a_symbol_ptr sym_ptr);

#if RECORD_HIDDEN_NAMES_IN_IL
extern void record_defeatable_name_hiding(a_symbol_ptr  hidden_sym,
                                          a_boolean     tag_hidden_by_nontag,
                                          a_scope_ptr   sp);
#endif /* RECORD_HIDDEN_NAMES_IN_IL */

extern a_boolean symbols_may_coexist_in_curr_scope
					(a_symbol_ptr  old_sym,
                                         a_symbol_ptr  new_sym,
                                         a_symbol_ptr  *insert_sym,
					 a_boolean     suppress_error);

extern a_symbol_ptr find_external_symbol(a_symbol_locator     *location,
                                         a_name_linkage_kind  linkage,
                                         a_type_ptr           type,
                                         a_symbol_locator     *ext_location);

extern void tildize_locator(a_symbol_locator *locator);

extern a_boolean destructor_name_matches_class_name(a_symbol_ptr class_sym);

extern void change_class_locator_into_constructor_locator(
                                                  a_symbol_locator   *locator,
                                                  a_source_position  *pos);

extern void make_opname_locator(an_opname_kind    opname,
                                a_symbol_locator  *locator,
                                a_source_position *pos);

extern void make_type_conversion_locator(a_type_ptr         type,
                                         a_symbol_locator   *locator,
                                         a_source_position  *pos);

extern a_symbol_ptr extract_default_operator_new_sym(a_symbol_ptr sym);

extern void make_global_operator_new_or_delete_symbol(an_opname_kind  opname);

extern a_routine_ptr select_default_constructor
					(a_type_ptr        class_type,
                                         a_source_position *err_pos,
					 a_type_ptr	   object_class_type,
                                         a_boolean         evaluated);

extern a_routine_ptr select_destructor(a_type_ptr       class_type,
				       a_type_ptr       object_class_type,
                                       a_source_position *position,
                                       a_boolean        honor_virtual,
                                       a_boolean        evaluated,
                                       a_boolean        suppress_access_check);

extern a_symbol_ptr find_copy_constructor(
                                   a_type_ptr            class_type,
                                   a_type_qualifier_set  required_qualifiers,
                                   a_boolean             *ambiguous,
                                   a_boolean             *class_bitwise_copy);

extern a_routine_ptr select_copy_constructor(
                                  a_type_ptr            class_type,
                                  a_type_qualifier_set  required_qualifiers,
                                  a_source_position     *err_pos,
                                  a_type_ptr            object_class_type,
                                  a_boolean             *class_bitwise_copy,
                                  a_boolean             evaluated,
                                  a_boolean             suppress_access_check);

extern char *il_entry_for_symbol(a_symbol_ptr      sym,
                                 an_il_entry_kind  *kind);

extern a_source_correspondence *source_corresp_entry_for_symbol(
                                                         a_symbol_ptr sym_ptr);

extern an_access_specifier compute_access(an_access_specifier access,
                                          an_access_specifier class_access);

extern an_access_specifier access_to_end_of_path
                                      (an_access_specifier         sym_access,
                                       a_derivation_step_ptr       path,
                                       a_base_class_derivation_ptr bcdp);

extern an_access_specifier access_for_symbol(a_symbol_ptr sym_ptr);

extern a_boolean have_member_access_privilege(a_type_ptr class_type);

extern a_boolean have_protected_member_access_privilege(a_type_ptr class_type);

extern a_boolean have_access_to_symbol(a_symbol_ptr symbol);

extern void member_check_ambiguity_and_verify_access
			(a_symbol_locator		*loc);

extern void perform_deferred_access_checks(void);

extern void perform_deferred_access_checks_for_function(a_routine_ptr rp);

extern void f_discard_deferred_access_checks(void);

extern void discard_declarator_access_errors(void);

extern void overload_check_ambiguity_and_verify_access(
                                           a_symbol_locator *locator,
                                           a_symbol_ptr     overloaded_symbol);


/*
Check to see if a symbol found is ambiguous or inaccessible.  Ambiguity
checking precedes access control (ARM, 10.1.1).  Only class members
can be ambiguous (in fact, only symbols projected into a derived class
by inheritance can be ambiguous), and only class members are subject
to access control.  Therefore, return immediately for non-class-members,
and call a subroutine for class members.
*/
#define check_ambiguity_and_verify_access(locator)                    \
{ if ((locator)->specific_symbol->class_of_which_a_member != NULL &&  \
      C_dialect == C_dialect_cplusplus) {                             \
    member_check_ambiguity_and_verify_access(locator);                \
  }  /* if */                                                         \
}  /* check_ambiguity_and_verify_access */


/*
Return TRUE if access1 represents greater accessibility than access2.
*/
#define is_more_accessible(access1, access2)    \
    ((int)(access1) < (int)(access2))

/*
Macro that returns TRUE if there are any deferred access checks to be
processed.
*/
#define any_deferred_access_checks()					\
  (curr_deferred_access_scope != NO_SCOPE_DEPTH &&			\
   scope_stack[curr_deferred_access_scope].deferred_access_checks != NULL)

/*
Set the flag that specifies that access errors should be deferred and
rechecked later.
*/
#define begin_deferral_of_access_checks()				\
{									\
  if (C_dialect == C_dialect_cplusplus) {				\
    check_assertion(curr_deferred_access_scope != NO_SCOPE_DEPTH);	\
    scope_stack[curr_deferred_access_scope].defer_access_checks = TRUE; \
  }  /* if */								\
}

/*
Clear the flag that specifies that access errors should be deferred.
*/
#define end_deferral_of_access_checks()					\
{									\
  if (C_dialect == C_dialect_cplusplus) {				\
    check_assertion(curr_deferred_access_scope != NO_SCOPE_DEPTH);	\
    scope_stack[curr_deferred_access_scope].defer_access_checks = FALSE;  \
    if (scope_stack[curr_deferred_access_scope].			\
                                           deferred_access_checks != NULL) { \
      /* Only make this call if there are entries on the list. */	\
      perform_deferred_access_checks();					\
    }  /* if */								\
  }  /* if */								\
}

/*
Throw away any deferred access entries.
*/
#define discard_deferred_access_checks()				\
{									\
  check_assertion(curr_deferred_access_scope != NO_SCOPE_DEPTH);	\
  if (scope_stack[curr_deferred_access_scope].			\
                                           deferred_access_checks != NULL) { \
    /* Only make this call if there are entries on the list. */	       	\
    f_discard_deferred_access_checks();				\
  }  /* if */								\
}

extern a_boolean uniform_access_of_overloaded_function(a_symbol_ptr  sym);

extern void f_check_protected_member_access(a_symbol_ptr      sym,
				            a_source_position *err_pos,
                                            a_type_ptr        access_class);
/*
If the symbol specified by locator is a protected member, do the access
check of ARM 11.5.  The symbol is being accessed through an object or
pointer of class class_type.
*/
#define check_protected_member_access(sym, err_pos, class_type)       \
{ if (access_for_symbol(fundamental_symbol_of(sym)) ==                \
                                (an_access_specifier)as_protected) {  \
    f_check_protected_member_access(sym, err_pos, class_type);        \
  }  /* if */                                                         \
}  /* check_protected_member_access */


/*
If symbol is a projection symbol, change it to the fundamental symbol pointed
to by the projection.
*/
#define reduce_projection_symbol_to_fundamental_symbol(symbol)        \
{ if ((symbol)->kind == (a_symbol_kind)sk_projection) {               \
    (symbol) = (symbol)->variant.projection.extra_info->fundamental_symbol;\
  }  /* if */                                                         \
}  /* reduce_projection_symbol_to_fundamental_symbol */

/*
Return the fundamental symbol for a given symbol.
*/
#define fundamental_symbol_of(symbol)                                 \
  (((symbol)->kind == (a_symbol_kind)sk_projection) ?                 \
        (symbol)->variant.projection.extra_info->fundamental_symbol : \
        (symbol))

/*
Return TRUE if the base class indicated by the base class entry bcp
is an accessible base class of viewpoint_class.  bcp must be a direct or
virtual base class of viewpoint_class, but bcp->derived_class might
be something other than viewpoint_class.  A base class is
accessible if its public members are accessible, which means
  (a) if the derivation is public, the base class is accessible;
  (b) if the derivation is private, the base class is accessible if we
      have member access to the derived class;
  (c) if the derivation is protected, the base class is accessible if we
      have member access to the derived class or to one of its derived
      classes.
is_accessible_imm_base_class can be used for direct or virtual base classes;
If the derivation step is a non-simple virtual step, it calls
is_accessible_virtual_base_class.
is_accessible_direct_base_class_derivation can be used for specific
derivations of direct or simple virtual base classes.
The function is_accessible_base_class should be used when it is
not known that the base class is an immediate base class.
*/
#define is_accessible_direct_base_class_derivation(bcdp, viewpoint_class) \
  ((bcdp)->access == (an_access_specifier)as_public ||                \
   have_member_access_privilege(viewpoint_class) ||                   \
   ((bcdp)->access == (an_access_specifier)as_protected &&            \
    have_protected_member_access_privilege(viewpoint_class)))
#define is_virtual_but_not_simple_direct_base_class(bcp)              \
  ((bcp)->is_virtual && (!(bcp)->direct || (bcp)->derivation->next != NULL))
#define is_accessible_imm_base_class(bcp, viewpoint_class)            \
  (is_virtual_but_not_simple_direct_base_class(bcp) ?                 \
    is_accessible_virtual_base_class(bcp, viewpoint_class) :          \
    is_accessible_direct_base_class_derivation(bcp->derivation,       \
                                               viewpoint_class))

/*
Given a symbol kind that describes a tag kind, this macro returns the
associated type kind.  If tag_kind does is not the symbol kind of a
class/struct, union, or enum, then tk_error is returned.
*/
#define type_kind_for_tag_kind(tag_kind)				\
  (a_type_kind)								\
  (tag_kind == (a_symbol_kind)sk_class_or_struct_tag ? tk_struct :	\
    (tag_kind == (a_symbol_kind)sk_union_tag ? tk_union :		\
      (tag_kind == (a_symbol_kind)sk_enum_tag ? tk_enum : tk_error)))


/*
Compare the tag kinds associated with two template_param_type_descrs.
They match if they are the same, or if one of them is unknown.
*/
#define matching_template_tag_kinds(tptdp1, tptdp2)			\
  (tptdp1 == NULL || tptdp2 == NULL ||					\
   tptdp1->tag_kind == tptdp2->tag_kind ||				\
   (tptdp1->tag_kind == (a_type_kind)tk_unknown ||			\
    tptdp2->tag_kind == (a_type_kind)tk_unknown))

extern a_boolean is_accessible_base_class(a_base_class_ptr bcp);

extern a_boolean is_accessible_virtual_base_class(
                                             a_base_class_ptr bcp,
                                             a_type_ptr       viewpoint_class);

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

extern a_symbol_ptr file_scope_id_lookup(a_symbol_locator         *locator,
                                         an_id_lookup_options_set options);

extern a_symbol_ptr opname_member_function_symbol(an_opname_kind kind,
                                                  a_type_ptr     class_type);

extern a_symbol_ptr opname_function_symbol(an_opname_kind kind);

/* Begin a name scope. */
extern a_scope_ptr push_scope(a_scope_kind       kind,
       	                      a_scope_number     scope_number_to_reuse,
                              a_type_ptr         assoc_type,
                              a_routine_ptr      assoc_routine);

extern a_scope_ptr push_template_instantiation_scope
                           (a_scope_number       scope_number_to_reuse,
			    a_type_ptr           assoc_type,
			    a_routine_ptr        assoc_routine,
			    a_symbol_ptr         instance_sym,
			    a_symbol_ptr         template_sym,
			    a_template_arg_ptr   template_arg_list,
			    a_boolean            nested_instantiation);
/* End a name scope. */
extern void pop_scope(void);
extern void push_class_reactivation_scope(a_type_ptr class_type);
extern void pop_class_reactivation_scope(void);

extern a_scope_depth scope_depth_of(a_symbol_ptr  sym,
                                    a_boolean     *is_local_to_function);
extern void set_source_corresp(a_source_correspondence *sc,
                               a_symbol_ptr            sp);
extern a_boolean current_class_symbol_if_class_template(a_symbol_ptr *sym);

/* Allocation */
extern an_extern_type_fixup_ptr alloc_etype_fixup(void);
extern a_symbol_list_entry_ptr alloc_symbol_list_entry(void);
extern a_template_param_ptr alloc_template_param(a_symbol_ptr sym);
extern a_template_instance_ptr alloc_template_instance(void);
extern void free_param_id(a_param_id_ptr *ppip);
extern void free_param_id_list(a_param_id_ptr *pidlist);
extern void clear_func_info(a_func_info_block *func_info);

#define done_with_func_info(func_info)                                 \
  free_param_id_list(&(func_info.param_id_list))


extern void add_to_param_id_list(a_symbol_locator            *locator,
                                 a_type_ptr                  type_ptr,
                                 a_source_position           *type_pos,
                                 a_storage_class             storage_class,
                                 a_func_info_block_ptr       func_info,
                                 a_source_sequence_entry_ptr param_ssep,
                                 a_param_id_ptr              *last_param_id);
extern a_param_id_ptr param_id_on_list(a_symbol_locator *locator,
                                       a_param_id_ptr    param_id_list);

extern void add_to_dependent_type_fixup_list(
                                      a_type_ptr                   class_type,
                                      a_dependent_type_fixup_kind  fixup_kind,
                                      char                         *ptr,
                                      a_byte_il_entry_kind         entity_kind,
                                      a_source_position            *pos);

extern void check_dependent_type_fixup_list(a_type_ptr  class_type);

/* Examine the list of symbols with a given name, looking for an instance
   with a particular kind. */
#define get_symbol_of_kind(des_kind, ptr)			      \
  while (((ptr) != NULL) && ((ptr)->kind != (des_kind))) (ptr) = (ptr)->next;

/* Return TRUE if a symbol is a class symbol.   A class symbol is
   one defined as a class, struct, or union, or a typedef of one of
   those.  This macro should only be used in C++ mode. */
#define is_class_symbol(sym)                                          \
  ((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||            \
   (sym)->kind == (a_symbol_kind)sk_union_tag ||                      \
   ((sym)->kind == (a_symbol_kind)sk_type &&                          \
                   is_class_struct_union_type((sym)->variant.type)))

/* Return TRUE if a symbol is a class symbol, class template symbol,
   or a type template parameter.  This macro should only be used in
   C++ mode. */
#define is_class_or_class_proxy_symbol(sym)                           \
  ((sym)->kind == (a_symbol_kind)sk_class_template ||		      \
   is_class_symbol(sym) ||                                            \
   ((sym)->kind == (a_symbol_kind)sk_type &&                          \
    (sym)->variant.type->kind == (a_type_kind)tk_template_param))

/* Return TRUE if the symbol is a template class symbol. */
#define is_template_class_symbol(sym)				      \
  (((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||	      \
    (sym)->kind == (a_symbol_kind)sk_union_tag) &&		      \
   ((sym)->variant.class_struct_union.extra_info->class_template != NULL))

/* Return TRUE if the symbol is a template class symbol for a class
   generated from the template (i.e., not a specific definition). */
#define is_template_class_and_not_specific_def_symbol(sym)		\
  (is_template_class_symbol((sym)) &&					\
   !(sym)->variant.class_struct_union.extra_info->is_specific_template_def)

/* Return TRUE if the symbol is a class template symbol. */
#define is_class_template_symbol(sym)					\
  ((sym)->kind == (a_symbol_kind)sk_class_template)

/* Return TRUE if the symbol is a class symbol for either a normal
   (non-template) or a "real" instantiation of a template class.  */
#define is_real_class_symbol(sym)				      \
  (((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||           \
    (sym)->kind == (a_symbol_kind)sk_union_tag) &&		      \
    !(sym)->variant.class_struct_union.extra_info->is_nonreal_class)

/* Return TRUE if a symbol is a tag symbol.   A tag symbol is
   one defined as a class, struct, union, or enum (but not as a typedef
   of one of those). */
#define is_tag_symbol(sym)                                            \
  ((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||            \
   (sym)->kind == (a_symbol_kind)sk_union_tag ||                      \
   (sym)->kind == (a_symbol_kind)sk_enum_tag)

/* Return TRUE if a symbol is a tag symbol, a class template symbol,
   or a type template parameter. */
#define is_tag_or_tag_proxy_symbol(sym)                               \
  ((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||            \
   (sym)->kind == (a_symbol_kind)sk_union_tag ||                      \
   (sym)->kind == (a_symbol_kind)sk_enum_tag ||			      \
   (sym)->kind == (a_symbol_kind)sk_class_template ||		      \
   ((sym)->kind == (a_symbol_kind)sk_type && (sym)->is_template_param))

/* Return TRUE if a symbol is a type symbol.   A type symbol is
   one defined as a typedef, or, in C++, as a class, struct, union,
   or enum. */
#define is_type_symbol(sym)                                           \
  ((sym)->kind == (a_symbol_kind)sk_type ||                           \
   (C_dialect == C_dialect_cplusplus && is_tag_symbol(sym)))

/* Return TRUE if a symbol is a function symbol. */
#define is_function_symbol(sym)                                       \
  ((sym)->kind == (a_symbol_kind)sk_routine ||                        \
   (sym)->kind == (a_symbol_kind)sk_member_function ||                \
   (sym)->kind == (a_symbol_kind)sk_overloaded_function)

/* Return TRUE if a symbol is a member function symbol. */
#define is_member_function_symbol(sym)                                \
  ((sym)->kind == (a_symbol_kind)sk_member_function ||                \
   ((sym)->kind == (a_symbol_kind)sk_overloaded_function &&           \
    (sym)->variant.overloaded_function.symbols->kind ==               \
                                (a_symbol_kind)sk_member_function))

extern a_boolean is_special_function_symbol(a_symbol_ptr             sym,
                                            a_special_function_kind  kind);

/* Return TRUE if a symbol is a constructor symbol. */
#define is_constructor_symbol(sym)                                    \
  is_special_function_symbol(sym,                                     \
                             (a_special_function_kind)sfk_constructor)

/* Return TRUE if a symbol is a destructor symbol. */
#define is_destructor_symbol(sym)                                     \
  is_special_function_symbol(sym,                                     \
                             (a_special_function_kind)sfk_destructor)

/* Return TRUE if a symbol is a projection symbol created for an
   access declaration. */
#define is_access_adjustment_symbol(sym)                              \
  ((sym)->kind == (a_symbol_kind)sk_projection &&                     \
   (sym)->variant.projection.access_adjustment_made)

/*
Extract the type from a type symbol (one for which is_type_symbol is TRUE).
*/
#define type_symbol_type(sym)                                         \
  (((sym)->kind == (a_symbol_kind)sk_type ||                          \
    (sym)->kind == (a_symbol_kind)sk_enum_tag) ?                      \
                               (sym)->variant.type :                  \
                               (sym)->variant.class_struct_union.type)

/*
Extract the routine type from the routine associated with an sk_routine
or sk_member_function symbol.
*/
#define routine_symbol_type(sym)                                      \
  (skip_typerefs((sym)->variant.routine.ptr->type))

/*
Extract a pointer to the class symbol supplement for a given type for
which is_class_struct_union_type is TRUE.
*/
#define symbol_supplement_for_class(tp)                              \
  (((a_symbol_ptr)(skip_typerefs(tp))->source_corresp.assoc_info)->  \
                            variant.class_struct_union.extra_info)

/* Return a pointer to the current routine entry (only usable when within
   a routine definition). */
#define current_routine_entry() (innermost_function_scope->variant.routine.ptr)

/* Return a pointer to the template symbol supplement for a given
   symbol.  Return NULL for symbols of the wrong kind. */
#define template_supplement_for_symbol(sym)				\
  /* if */ ((sym)->kind == (a_symbol_kind)sk_class_template ||		\
            (sym)->kind == (a_symbol_kind)sk_function_template) ? /* { */ \
    (sym)->variant.template_info :					\
  /* } else if */ (sym)->kind == (a_symbol_kind)sk_member_function ? /* { */ \
    (sym)->variant.routine.instance_ptr->template_info :		\
  /* } else if */ (sym)->kind ==					\
			 (a_symbol_kind)sk_static_data_member ? /* { */	\
    (sym)->variant.static_data_member.instance_ptr->template_info :	\
  /* } else { */							\
    NULL								\
  /* } */

/* Return TRUE if the symbol represents the prototype instantiation of a
   class template. */
#define is_prototype_instantiation_symbol(sym)				\
  (is_template_class_symbol((sym)) &&					\
   (sym)->variant.class_struct_union.extra_info->is_prototype_instantiation)

extern void form_symbol_name(a_symbol_ptr                          sym,
                             an_il_to_str_output_control_block_ptr octl);

#if DEBUG
/* Show and return the amount of memory used by symbol table entries. */
extern unsigned long show_symbol_space_used(void);
/* Display a symbol table entry. */
extern void db_symbol(a_symbol_ptr	sym,
                      char		*string,
                      int		indentation);

extern int db_scope_kind(a_scope_kind sck);
extern void db_scope_stack(void);
#endif /* DEBUG */

extern void symbol_tbl_one_time_init(void);

extern void symbol_tbl_init(void);

#endif /* ifndef SYMBOL_TBL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
