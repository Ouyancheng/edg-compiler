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

/* Type used for the options set for the name lookup routines.  This
   is declared here to prevent recursion problems. */
typedef int an_id_lookup_options_set;

/* Declare pointer types up front to minimize mutual recursion problems. */
typedef struct a_symbol        *a_symbol_ptr;
typedef struct a_symbol_header *a_symbol_header_ptr;
typedef struct a_macro_param   *a_macro_param_ptr;
typedef struct a_macro_def     *a_macro_def_ptr;
typedef struct a_vla_fixup     *a_vla_fixup_ptr;
typedef struct an_extern_type_fixup *an_extern_type_fixup_ptr;
typedef struct a_template_param *a_template_param_ptr;
typedef struct an_access_error_descr *an_access_error_descr_ptr;
typedef struct a_template_cache_segment *a_template_cache_segment_ptr;
typedef struct a_template_decl_info *a_template_decl_info_ptr;
typedef struct a_template_instance *a_template_instance_ptr;
typedef struct a_nondependent_call_info *a_nondependent_call_info_ptr;
typedef struct a_template_cache *a_template_cache_ptr;
typedef struct a_control_flow_descr a_control_flow_descr_dummy_typedef;
typedef struct a_tmpl_decl_state a_tmpl_decl_state_dummy_typedef;
typedef struct an_exception_spec_error_descr
                                          *an_exception_spec_error_descr_ptr;
typedef struct an_attribute    an_attribute_dummy_typedef;

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

/* The pointer to a_translation_unit is declared here even though the struct
   itself is defined in trans_unit.h.  This allows the pointer to be made
   available to symbol_tbl.h without creating recursive reference problems. */
typedef struct a_translation_unit *a_translation_unit_ptr;

/* Some other things declared up front to avoid mutual recursion problems. */
/*
A symbol-reference kind is a bit vector whose values are defined in
symbol_ref.h.  The typedef declaration is here to avoid mutual inclusion
problems.
*/
typedef int a_symbol_reference_kind;

/* Unique sequence number identifying a declaration in a given scope. */
typedef unsigned long a_decl_sequence_number;
  
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
#ifndef SCOPE_STK_H
#include "scope_stk.h"
#endif /* ifndef SCOPE_STK_H */

/*
The scope number for the file scope.  Equal to FILE_SCOPE_NUMBER (zero)
except in secondary translation units (i.e., when export template is used).
*/
EXTERN a_scope_number
		file_scope_number;

EXTERN a_translation_unit_ptr
		*trans_unit_for_scope;
			/* A dynamically allocated array of translation
			   unit pointers indexed by scope number. */


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
			   is entered.  When a qualified name is scanned,
			   this source position points to the final component
			   of the name, while pos_curr_token points to the
			   beginning of the entire qualified name. */
  a_bit_field	is_qualified_name:1;
			/* TRUE if the "identifier" is a C++ qualified-name
			   (e.g., "A::x" or "::y").  specific_symbol points
			   to the proper symbol. */
  a_bit_field	is_global_qualified_name:1;
			/* TRUE if the "identifier" is a C++ qualified-name
			   that begins with a unary "::" (e.g., "::y" or
			   ::A::x). */
  a_bit_field	is_file_scope_qualified_name:1;
			/* TRUE if the "identifier" is a C++ qualified-name
			   that refers to a file scope entity (e.g., ::y
			   but not ::A::x). */
  a_bit_field	is_operator_name:1;
			/* TRUE if the "identifier" is a C++ overloaded
			   operator name, of the form "operator<token>",
			   e.g., "operator+".  Cannot be TRUE when
			   is_conversion_name is TRUE. */
  a_bit_field	is_conversion_name:1;
			/* TRUE if the "identifier" is a C++ user-defined
			   conversion name, of the form "operator <type-name>",
			   e.g., "operator int".  Cannot be TRUE when
			   is_operator_name is TRUE. */
  a_bit_field	is_destructor_name:1;
			/* TRUE if the "identifier" is a C++ destructor
			   name, of the form "~<name>". */
  a_bit_field	is_semivisible_nested_type:1;
			/* TRUE if specific_symbol points to a nested type
			   that is not actually visible, except as a C++
			   anachronism (ARM 18.3.5). */
  a_bit_field	access_control_error_reported:1;
			/* TRUE if an accessibility error has already been
			   issued on the associated symbol. */
  a_bit_field	has_been_coalesced:1;
			/* TRUE if the identifier has already been processed
			   by is_generalized_identifier_start -- even if
			   no coalescing was actually performed.  This
			   indicates that no processing is needed should
			   is_generalized_identifier_start be called again. */
  a_bit_field	is_vacuous_destructor_reference:1;
			/* TRUE if the identifier is a destructor name of
			   a type that has no destructor.  Used for
			   explicit destructor invocations of the form
			   p->int::~int.  The type can be a nonclass type
			   or a class type with no destructor. */
  a_bit_field	is_nonclass_destructor:1;
			/* TRUE for vacuous destructor references for 
			   nonclass types such as int::~int or i::~i
			   where "i" is a typedef name. */
  a_bit_field	is_error:1;
			/* TRUE if an error has been diagnosed on the use
			   of the associated identifier and no symbol should
			   be entered into the symbol table. */
  a_bit_field	do_not_clear_specific_symbol:1;
			/* TRUE if the specific symbol field of the locator
		           should not be cleared when clear_specific_symbol
                           is called.  This is set when clearing the specific
			   symbol field would result in the loss of information
			   that cannot be recovered by repeating the lookup
			   process.  This is TRUE for template references
			   that have been coalesced and for specific symbol
			   error locators. */
  a_bit_field	is_template_id:1;
			/* TRUE if the coalesced identifier is a template-id
			   (i.e., template-name < template-arg-list >). */
  a_bit_field	is_class_member:1;
			/* TRUE if is_qualified_name is TRUE and the entity
			   pointed to by the parent field is a class
			   (not a namespace). */
  a_bit_field	is_unknown_template_reference:1;
			/* TRUE if this is a reference to a nonreal template
			   that was uncoalesced by ensure_correct_nonreal-
			   instance_kind. */
  a_symbol_ptr	specific_symbol;
			/* If is_qualified_name is TRUE, this points to the
			   specific symbol for the qualified name.  Otherwise,
			   if this pointer is non-NULL, it is the result of
			   the most recent lookup of this identifier (e.g.,
			   by normal_id_lookup). */
  a_parent_class_or_namespace
		parent;
			/* If is_qualified_name is TRUE, this points to
			   either the class or the namespace specified
			   by the qualifier (depending on the value of the
			   is_class_member flag).  If is_vacuous_destructor
			   is TRUE this points to the type of the qualifier,
			   which may not actually be a class type (e.g.,
			   for int::~int this will point to the type "int"). */
  a_template_arg_ptr
		template_arg_list;
			/* When a function template symbol, or an overloaded
			   function symbol is followed by a template argument
			   list, this points to the argument list that was
			   specified.  Typically, the reference cannot be
			   coalesced to a pointer to a template instance
			   until the function type is known. */
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

/*
If a locator refers to a class member, return a pointer to the parent class
type, otherwise return NULL.
*/
#define qualifier_class_type(locator)					\
  ((locator).is_class_member ? (locator).parent.class_type : (a_type_ptr)NULL)

/*
If a locator refers to a namespace member, return a pointer to the parent
namespace, otherwise return NULL.
*/
#define qualifier_namespace_ptr(locator)				\
  ((locator).is_class_member ? (a_namespace_ptr)NULL			\
                             : (locator).parent.namespace_ptr)


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

/* Retrieve a pointer to the symbol list on which file scope symbols are
   located.  This is usually the active list, but after the file scope
   has been popped, it is the inactive list. */
#define symbol_list_for_file_scope_symbols(symhdr)			\
  (file_scope_symbols_are_on_inactive_list ?				\
                       (symhdr)->inactive_symbols : (symhdr)->symbol)

/* Clear the specific symbol field of the locator unless instructed not
   to by the do_not_clear_specific_symbol field of the locator. */
#define clear_specific_symbol(loc)					\
{  if (!((loc).do_not_clear_specific_symbol)) {				\
     (loc).specific_symbol = NULL;					\
     (loc).is_semivisible_nested_type = FALSE;				\
   }									\
}


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
  sk_class_or_struct_tag,
			/* Tag of a struct, or C++ class type. */
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
  sk_overloaded_function,
			/* C++ overloaded function (member or non-member). */
  sk_parameter,         /* Parameter name in a function prototype. */
  sk_class_template,    /* Definition of a C++ class template. */
  sk_function_template, /* Definition of a C++ function template. */
  sk_namespace,         /* Definition of a C++ namespace. */
  sk_namespace_projection,
		        /* Projection of a member of a namespace into another
			   scope (either through a using-declaration or as a
			   by-product of a lookup). */
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
   "projection", "overloaded function", "parameter", "class template",
   "function template", "namespace", "namespace projection",
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
  a_bit_field	object_like:1;
			/* TRUE if this macro is object-like (i.e., has no
			   parameters). */
  a_bit_field	cannot_be_redefined:1;
			/* TRUE if this is a predefined macro that cannot
			   be redefined later.  This is TRUE for ANSI
			   predefined macros. */
  a_bit_field	ref_suppresses_pch_file:1;
			/* TRUE if referencing this macro within a header is
			   incompatible with creating a precompiled header
			   file; TRUE, e.g., for predefined macros __DATE__
			   and __TIME__. */
  a_bit_field	variadic:1;
			/* TRUE if the formal parameter list of this macro
			   ended with the ellipsis token (an extension). */
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
  rt_text,
			/* Raw text.  Followed by 3 bytes containing a
			   character count, and then that many characters
			   of raw text. */
  rt_paste,
			/* "##" token.  This is just a placeholder and not
			   actual replacement text. */
  rt_raw_argument,
			/* Raw string for argument.  Followed by 3 bytes
			   containing the argument number (first argument is
			   numbered 1).  "raw arguments" are used for arguments
			   adjacent to "##" and all arguments in pcc
			   mode. */
  rt_stringized_raw_argument,
			/* Same as rt_raw_argument, but argument raw string is
			   turned into a string literal (see standard,
			   3.8.3.2). */
  rt_charized_raw_argument,
			/* Same as rt_stringized_raw_argument, but argument
			   raw string is turned into a char literal instead
			   (Microsoft extension). */
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
  temp |= (sizeof_t)(*(a_byte *)rtp++) << (CHAR_BIT*2);          \
  num = temp;                                                         \
}  /* get_macro_repl_text_number */


/*
Put a multi-byte number (num) into a macro-definition string. rtp points
to the first byte of the number; it is advanced past the number on return.
*/
#define PN_BYTE_MASK ((1 << CHAR_BIT) - 1)
#define put_macro_repl_text_number(num, rtp)                          \
{ sizeof_t temp = num;                                           \
  *(a_byte *)rtp++ = (a_byte)(temp                   & PN_BYTE_MASK); \
  *(a_byte *)rtp++ = (a_byte)((temp >> CHAR_BIT)     & PN_BYTE_MASK); \
  *(a_byte *)rtp++ = (a_byte)((temp >> (CHAR_BIT*2)) & PN_BYTE_MASK); \
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
#if GNU_EXTENSIONS_ALLOWED
  dtfk_copy_definition,
			/* Copy the definition of the newly defined
			   type. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  dtfk_array_type_size	/* Set the size of an array type. */
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


typedef struct a_type_list_entry *a_type_list_entry_ptr;
typedef struct a_type_list_entry {
  /* Entry created to produce a list of types for some special purpose.
     For example, such a list is used when building a list of associated
     types for namespace/class directed lookup. */
  a_type_list_entry_ptr
		next;
			/* Next in a linked list of type list entries; NULL
			   for the last on the list. */
  a_type_ptr	type;
			/* Pointer to a type entry. */
} a_type_list_entry;


typedef struct a_substituted_type_list_entry
			*a_substituted_type_list_entry_ptr;
typedef struct a_substituted_type_list_entry {
  /* Entry that points to a template argument list and a routine type
     that resulted from substituting the template parameters of the
     associated template with the template arguments. */
  a_substituted_type_list_entry_ptr
		next;
			/* Next in a linked list of type list entries; NULL
			   for the last on the list. */
  a_template_arg_ptr
		templ_arg_list;
			/* Pointer to a template argument list that was used
			   to create type.  This argument list may contain
			   unspecified template arguments (i.e., template
			   arguments with NULL type or constant pointers). */
  a_type_ptr	type;
			/* Pointer to a type entry. */
} a_substituted_type_list_entry;


typedef struct a_namespace_list_entry *a_namespace_list_entry_ptr;
typedef struct a_namespace_list_entry {
  /* Entry created to produce a list of namespaces for some special purpose.
     For example, the list of namespaces in which operators may be found
     for an argument of a given class type. */
  a_namespace_list_entry_ptr
		next;
			/* Next in a linked list of namespace list
			   entries; NULL for the last on the list. */
  a_namespace_ptr  ptr;
			/* Pointer to a namespace entry. */
} a_namespace_list_entry;

/* Forward definition. */
typedef struct a_template_symbol_supplement *a_template_symbol_supplement_ptr;

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
			   more than one); NULL if there is none.  The set
			   may comprise user-declared constructors, an
			   implicitly-declared (trivial or nontrivial) copy
			   constructor, or an implicitly-declared nontrivial
			   default constructor.  The set is empty if the
			   class has only a trivial default constructor (12.1
			   [class.ctor]) and trivial copy constructor (12.8
			   [class.copy]). */
  a_symbol_ptr	trivial_default_constructor;
			/* When constructor is NULL and is_POD is FALSE,
			   pointer to an sk_member_function symbol for the
			   trivial default constructor; it is never actually
			   called (that's why it's not in the constructor set
			   for this class), and the associated routine entry
			   is not added to the IL, but its definition may
			   nevertheless require diagnostics:
			     class X { const int i; };
			     X x;
			   X is not a POD (private member) and the implicit
			   definition of X::X() is ill-formed.  Note: when
			   an implicitly declared default constructor is
			   nontrivial, it appears on the constructor list for
			   the class. */
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
  a_symbol_list_entry_ptr
		conversion_template_list;
			/* Pointer to a linked list of entries identifying
			   conversion operator templates declared for this
			   class. Symbols pointed to are (or are projection
			   symbols referring to) sk_function_template symbols.
			   Instances of the conversion operator templates
			   are not added to conversion_list but are listed
			   under the template on which they are based. */
  a_routine_fixup_ptr
		routine_fixup_list;
			/* Pointer to a list of entities used in the token
			   caching and delayed scanning scheme required for
			   C++ member functions (routine bodies and default
			   arguments). */
  a_symbol_ptr  class_template;
                        /* Pointer to a class template symbol.  Present
                           only when this class is an instantiation of
                           a class template, NULL otherwise.  Note that
			   this is NULL for a class nested within a
			   class template (except for member templates)
			   even if the nested class was defined outside
			   of the class template. */
  a_template_symbol_supplement_ptr
		template_info;
			/* Pointer to associated template information when
			   the associated class is a prototype instantiation
			   of a class template or a nested class of a class
			   template.  NULL for other classes including 
			   other instantiations of the template. */
  a_symbol_ptr	next_in_instantiations_list;
			/* When the class is an instance of a class template
			   or a nested class of a class template, this field
			   points to the next instance in the instantiations
			   list pointed to by the template symbol supplement
			   of the template with which this instance is
			   associated. */
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
  a_namespace_ptr
		referencing_namespace;
			/* For template classes this contains a pointer
			   to the namespace in which the use that
			   first required the instantiation of the template
			   was encountered.  NULL if the first reference
			   was in the global namespace.  This field is
			   set when the instantiation_required flag is
			   set. */
  a_dependent_type_fixup_ptr
		dependent_type_fixup_list;
			/* If the current class is not yet defined, a pointer
			   to a list of entries identifying arrays, function
			   types, and parameters that are dependent on it and
			   require fixup when it is completed.  Once the class
			   is defined, the pointer is cleared. */
  a_namespace_list_entry_ptr
		operator_lookup_namespaces;
			/* Pointer to a list of namespaces that are the
			   parent namespaces of this class or one of its
			   base classes.  This is the list of namespaces
			   that must be searched for an operand of this
			   class type.  The entire list or some portion
			   of the end of the list may be shared between
			   classes in a given namespace. */
  a_symbol_ptr	friend_functions;
			/* Pointer to a list of non-class-member functions
			   declared as friends of the current class.  The
			   symbols on the list will be sk_namespace_projection
			   symbols that point to an sk_routine symbol that
			   is a friend (or sk_overloaded_function symbols that
			   point to sk_namespace_projection symbols).  This
			   list is used to assist with namespace and class
			   directed lookup. */
  a_bit_field	has_nontrivial_default_constructor:1;
			/* TRUE if a default constructor has been explicitly
			   declared or a nontrivial default constructor has
			   implicitly declared for this class. */
  a_bit_field	has_user_declared_default_constructor:1;
			/* TRUE if a default constructor has been explicitly
			   declared for this class. */
  a_bit_field	has_copy_constructor:1;
			/* TRUE if a copy constructor has either been declared
			   or generated for the class. */
  a_bit_field	has_copy_constructor_for_const_object:1;
			/* TRUE if there is a copy constructor for the class
			   and it can be used to copy a const object. */
  a_bit_field	assignment_by_bitwise_copy_allowed:1;
			/* TRUE if assignment can be performed by a bitwise
			   copy rather than by calling an assignment operator
			   function (i.e., when the assignment operator is
			   not user-defined and when the current class has no
			   virtual base classes and no subobjects for which
			   bitwise copy is not allowed). */
  a_bit_field	construction_by_bitwise_copy_allowed:1;
			/* TRUE if copy construction can be performed by a
			   bitwise copy rather than by calling a copy
			   constructor function. */
  a_bit_field	target_of_conversion_function:1;
			/* TRUE if this class is the target of a user-defined
			   conversion function (for conversion from another
			   class to this class). */
  a_bit_field	any_ref_member:1;
			/* TRUE if this class has any fields of reference
			   type. */
  a_bit_field	is_class_aggregate:1;
			/* TRUE if the class has no constructors, no base
			   classes, no private or protected members, and
			   no virtual functions (ARM 8.4.1). */
  a_bit_field	is_POD:1;
			/* TRUE if the class is a "POD" -- an aggregate with
			   further restrictions that make it look like a
			   C struct or union (WP 9 [class]). */
  a_bit_field	has_operator_new:1;
			/* TRUE if a member operator new() has been declared
			   for this class or a class from which it derived. */
  a_bit_field	has_operator_array_new:1;
			/* TRUE if a member operator new[]() has been
			   declared for this class or a class from which it
			   is derived. */
  a_bit_field	has_operator_delete:1;
			/* TRUE if a member operator delete() has been
			   declared for this class or a class from which it
			   is derived. */
  a_bit_field	has_operator_array_delete:1;
			/* TRUE if a member operator delete[]() has been
			   declared for this class or a class from which it
			   is derived. */
  a_bit_field	any_nonstatic_data_members:1;
			/* TRUE if the class or any of its base classes has
			   one or more nonstatic data members. */
  a_bit_field	any_nonreal_base_classes:1;
			/* For a prototype instantiation this is TRUE
			   if any of its base classes are nonreal classes. */
  a_bit_field	any_template_dependent_fields;
			/*  For a prototype instantiation this is TRUE if any
			    field is dependent on a template parameter. */
  a_bit_field	instantiation_in_progress:1;
			/* For an real instantiation, this is TRUE if the
			   full instantiation is in the process of being
			   generated. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_bit_field	definition_is_first_decl:1;
			/* TRUE when the first declaration of this class in
			   the translation unit is the definition. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  bitfield_to_avoid_codecenter_warnings()
} a_class_symbol_supplement;


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
  a_type_ptr	declared_type;
			/* The type as actually declared by the program --
			   before array-to-pointer adjustment and before
			   type-qualifiers are stripped off. */
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
			/* TRUE for an old-style parameter for which
			   an explicit declaration is omitted. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		source_sequence_entry;
			/* Source-sequence information saved during declarator
			   processing. */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if GNU_EXTENSIONS_ALLOWED
  struct an_attribute
  		*attributes;
			/* The attributes associated with this parameter. */
#endif /* GNU_EXTENSIONS_ALLOWED */
  a_variable_ptr
		dummy_vla_variable;
			/* A dummy variable created for scanning a VLA
			   expression that refers to the parameter before
			   its "real" variable entry is allocated. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_range
		specifiers_range,
		declarator_range,
		identifier_range;
			/* Source position information recorded at the point
			   of declaration, to be transferred to the associated
			   parameter variable entry if one is created. */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  a_source_position
		old_style_id_pos;
			/* For an old-style parameter, the source position of
			   the initial reference (i.e., of the declaration
			   within the parenthesized comma-list of parameter
			   names).  null_source_position for a new-style
			   parameter. */
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
  an_exception_spec_error_descr_ptr
		exception_spec_errors;
			/* Pointer to a linked list of diagnostics that were
			   detected during scanning of exception
			   specifications but that are to be issued later;
			   may be NULL.  C++ only. */
  a_scope_number
		scope_number;
			/* The scope number used for the function prototype
			   scope for the parameters, to be reused for the
			   function scope if a body is found. */
  a_vla_fixup_ptr
                vla_fixup_list;
			/* A list of entries representing fixups that are
			   required resulting from a VLA declaration in a
			   function prototype scope.  Originally the list
			   appears in the sck_function_prototype scope stack
			   entry; it is moved when the scope stack is popped,
			   and the fixups are done if the function prototype
			   is associated with a function definition. */
  a_bit_field	any_prototype_names_omitted:1;
			/* TRUE if the parameter list is a prototype list,
			   and it includes at least one parameter with
			   just a type and no name. */
  a_bit_field	is_inline:1;
			/* TRUE if inline was specified (C++ only). */
  a_bit_field	is_definition:1;
			/* TRUE if the current declaration is a definition. */
  a_bit_field	is_main_function:1;
			/* TRUE if the function "main". */
  a_bit_field	is_implicit_declaration:1;
			/* TRUE if this is an implicit declaration. */
  a_bit_field	function_type_from_typedef:1;
			/* TRUE if the function type came from a typedef
			   rather than from the declarator.  When it is TRUE,
			   an error will be issued on a function definition
			   and param_id_list and prototype_scope_symbols will
			   be NULL. */
  a_bit_field	any_default_args:1;
			/* TRUE if the function type declaration included
			   the declarations of default arguments. */
#if ASM_FUNCTION_ALLOWED
  a_bit_field	is_asm_function:1;
			/* TRUE if the function type declaration included the
			   asm specifier. */
#endif /* ASM_FUNCTION_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS
  a_bit_field	is_movable_member_or_friend_def:1;
#endif /* FRIEND_AND_MEMBER_DEFINITIONS_MAY_BE_MOVED_OUT_OF_CLASS */
			/* TRUE if the function is defined inside a class
			   definition but the source-sequence entry for its
			   definition should make it appear to have been
			   defined outside the class.  Only set when
			   NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_
			   SEQUENCE_LISTS is configured to TRUE. */
  a_source_sequence_entry_ptr
		declarator_ssep;
			/* Source sequence entry for the function
			   declarator. */
  a_type_ptr	declared_type;
			/* The routine type as it actually appears in the
			   current declaration. */
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
  a_source_sequence_entry_ptr
		prototype_scope_ss_list;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if USER_CONTROL_OF_STRUCT_PACKING
      a_targ_alignment
		max_member_alignment;
			/* If nonzero, the default maximum alignment of any
			   nonstatic data member of any class, struct, or
			   union defined in the body of the function.  The
			   value may be overridden by #pragma pack directives
			   within the function body. */
#endif /* USER_CONTROL_OF_STRUCT_PACKING */
} a_func_info_block;


/*
Structure that contains the information about a template declaration that
is needed to recreate the context in which tokens from the declaration
should be rescanned when creating an instantiation.
*/
typedef struct a_template_decl_info {
  a_template_param_ptr
		parameters;
			/* The formal template parameters that must be
			   visible when then tokens are rescanned. */
  a_scope_number
		declaration_scope;
			/* The scope number assigned when the template
			   declaration containing these tokens was scanned.
			   This scope needs to be used when the tokens are
			   scanned for the parameter symbols to be visible. */
  a_scope_ptr	enclosing_scope;
			/* The scope containing the template declaration of
			   which these tokens are a part. */
  a_template_decl_info_ptr
		enclosing_template_decl;
			/* If the template declaration appeared as part of a
			   nested template declaration (i.e., a single
			   declaration that includes more than one
			   "template <...>" clause), this points to the
			   template declaration information of the enclosing
			   template declaration information structure (i.e.,
			   the "template <...>" to the left of the current one
			   in the declaration).  Contains NULL for the leftmost
			   "template <...>" clause in a declaration. */
  a_name_linkage_kind
		name_linkage;
			/* The default name linkage at the point of the
			   declaration.  This is "reactivated" as the default
			   when a template is instantiated. */
  a_decl_sequence_number
		decl_seq;
			/* The declaration sequence number at the point of
			   the template declaration.  Used during lookup
			   to exclude names not visible at the point of
			   template definition. */
  a_nondependent_call_info_ptr
		nondependent_calls;
			/* A list of entries that describe the nondependent
			   calls within this template.  NULL if no such list
			   exists.  The list is maintained in token sequence
			   number order.  For class templates, this includes
			   the nondependent calls for default argument
			   expressions and bodies of nontemplate member
			   functions. */
  a_nondependent_call_info_ptr
		last_entry_added;
			/* Pointer to the entry most recently added to the
			   list. */
} a_template_decl_info;


/*
Structure used to record information about nondependent calls.
An entry is created during the prototype instantiation of a call.
The list is traversed during a real instantiation, and a matching
entry is returned if a given call is nondependent.  Entries are
matched using a token sequence number.  The list is maintained in
token sequence number order.

For class templates, the list includes nondependent call entries for
default arguments and bodies of nontemplate member functions.
*/
typedef struct a_nondependent_call_info {
  a_nondependent_call_info_ptr
		previous;
			/* The previous entry in the list.  NULL for the first
			   entry. */
  a_nondependent_call_info_ptr
		next;
			/* The next entry in the list.  NULL for the last
			   entry. */
  a_token_sequence_number
		token_sequence_number;
			/* Token sequence number that identifies the location
			   of the call.  For normal calls, this is the
			   number associated with the "(" of the argument
			   list.  For calls made via operators, this is the
			   position of the operator. */
  a_symbol_ptr	symbol;
			/* Pointer to the symbol of the function to be
			   called.  NULL for nondependent calls for which
			   overload resolution must be deferred to the
			   real instantiation. */
} a_nondependent_call_info;


/*
Structure that contains supplementary declarative information (much of it
nonstandard, e.g., as used in Microsoft-compatibility mode).  This block is
passed around during declaration processing; its contents may be copied into
IL entries after the appropriate checking is done.
*/
typedef struct a_decl_modifiers_block *a_decl_modifiers_block_ptr;
typedef struct a_decl_modifiers_block {
  a_decl_modifier
		flags;
			/* A bit-vector of flags representing additional
			   declarative information (e.g.,  via the __declspec
			   mechanism in Microsoft compatibility mode).  This
			   may eventually be copied into the IL. */
  a_bit_field	direct_linkage_specifier:1;
			/* TRUE if an only if a linkage specifier was added
			   directly to the declaration (e.g., extern "C" A x;
			   but not extern "C" { A x; }).  Not copied into
			   the IL. */
#if GNU_EXTENSIONS_ALLOWED
  a_bit_field  marked_as_gnu_extension:1;
			/* TRUE if the declaration was preceded by the GNU
			   keyword __extension__. */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  char		*uuid_string;
			/* Pointer to a string representing the argument of
			   a uuid decl-modifier (in Microsoft-compatibility
			   mode). */
  char		*get_property_name,
		*put_property_name;
			/* When __declspec(property(get=gname,put=pname))
			   (a Microsoft extension in C++ mode) is specified
			   for a field, these fields point to the get and put
			   routine names, null-terminated.  NULL otherwise. */
  char		*allocate_segname;
			/* Pointer to a string representing the argument of an
			   allocate decl-modifier (in Microsoft-compatibility
			   mode). */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} a_decl_modifiers_block;


/*
Structure that contains a token cache that represents a template or
part of a template, and the information needed to recreate the context
in which the tokens should be rescanned.
*/
typedef struct a_template_cache {
  a_token_cache	tokens;
			/* The token cache containing the tokens. */
  a_template_decl_info_ptr
		decl_info;
			/* Pointer to the template declaration information
			   associated with the template declaration that
			   contained the tokens in the token cache above. */
} a_template_cache;


typedef struct a_template_param {
  /* Information describing a template formal parameter.  Pointed to by the
     template symbol supplement. */
  a_template_param_ptr
                next;
                        /* Pointer to the next template parameter. */
  a_symbol_ptr	param_symbol;
			/* Symbol entry for a formal parameter of the
                           template. */
  a_template_cache
		cache;
			/* Contains the cached tokens that comprise the
			   template parameter declaration.  Used to
			   create the parameter types for instances of
			   the class template when the parameter type
			   depends on other template parameters. */
  a_bit_field	has_default_arg:1;
			/* TRUE if a default argument has been declared for
			   this parameter. */
  a_bit_field	def_arg_involves_template_param:1;
			/* TRUE if the default argument involves a template
			   parameter.  For nontype parameters, this means
			   that the constant involves a template parameter.
			   It will also be set TRUE if the type of the
			   constant involves a template parameter. */
  bitfield_to_avoid_codecenter_warnings()
  union {
    /* When param_symbol->kind = sk_type. */
    a_type_ptr
		type;
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
      a_bit_field
		type_involves_template_param:1;
			/* TRUE if the type entry associated with the
			   parameter constant involves (anywhere in its
			   type tree) a tk_template_param type entry. */
      bitfield_to_avoid_codecenter_warnings()
    } constant;
    /* When param_symbol->kind = sk_class_template. */
    a_template_symbol_supplement_ptr
		templ;
			/* Template entry for a formal parameter.  A unique
			   template entry is created for each template
			   template parameter. */
  } variant;
  union {
    /* Note that when def_arg_involves_template_param is FALSE, these
       fields contain the actual default argument to be used.  When
       def_arg_involves_template_param is TRUE, they contain the
       "prototype" default argument (i.e., the template-dependent one
       that was scanned when the template declaration is scanned).
       In some modes, the default is not scanned when its type is
       dependent.  In such cases, the prototype value is NULL. */
    /* When param_symbol->kind = sk_constant. */
    a_constant_ptr
		constant;
			/* Constant containing the default value
			   to be used as the actual argument of an
		           instantiation when the actual argument
			   corresponding to this parameter is omitted. */
    /* When param_symbol->kind = sk_type. */
    a_type_ptr
		type;
			/* Type containing the default value to be used
			   as the actual argument of an instantiation when
			   the actual argument corresponding to this parameter
			   is omitted. */
    /* When param_symbol->kind = sk_class_template. */
    a_template_ptr
		templ;
			/* Template that is the default value to be used
			   as the actual argument of an instantiation when
			   the actual argument corresponding to this parameter
			   is omitted. */
  } default_arg;
  /* When def_arg_involves_template_param is TRUE. */
  a_template_cache
		default_arg_cache;
			/* The template cache that contains the
			   tokens of the default argument expression.
			   Only used when def_arg_involves_template_param is
			   TRUE. */
} a_template_param;


typedef struct a_def_undef_string *a_def_undef_string_ptr;
typedef struct a_def_undef_string {
  /* Used to save -D (define symbol) and -U (undefined symbol) command-line
     arguments.  There are separate lists for def and undef, so the
     entry itself need not identify the function involved. */
  a_def_undef_string_ptr
		next;
			/* Next entry on this list, or NULL if this is the
			   last entry. */
  char		*text;
			/* The text of the argument (i.e., "x=1" for the
			   option "-Dx=1", "x" for "-Ux"). */
} a_def_undef_string;


/*
Entry used to record information about a file containing exported
template definitions.
*/
typedef struct an_exported_template_file *an_exported_template_file_ptr;
typedef struct an_exported_template_file {
  char		*directory_name;
			/* Directory containing the file. */
  char		*source_file_name;
			/* Name of the source file. */
  a_translation_unit_ptr
		translation_unit;
			/* If the translation unit for this file has been
			   loaded, this point to the translation unit entry.
			   NULL if the translation unit has not been loaded. */
  char		*module_id;
			/* The module ID read from the exported template
			   file.  When instantiating exported templates, the
			   original module ID must be used when referring to
			   things like static entities that were promoted to
			   be external so that they could be referenced from
			   instantiations. */
  a_directory_name_entry_ptr
		incl_search_path;
			/* The include search path to be used when loading this
			   file. */
  a_directory_name_entry_ptr
		sys_incl_search_path;
			/* The system include search path to be used when
			   loading this file. */
  a_def_undef_string_ptr
		define_list;
			/* A list of command-line macro definitions to be used
			   when loading this file. */
  a_def_undef_string_ptr
		undefine_list;
			/* A list of command-line macro undefines to be used
			   when loading this file. */
} an_exported_template_file;


typedef int an_instance_required_count;
			/* Type used to record the number of translation units
			   for which an instance of a given template is
			   required. */

/*
Entry that describes the use of a template instance across the entire
set of translation units that are being processed.  A template instance
for an external entity may be referenced by any number of translation
units, but only one instantiation of the instance is required.  This
entry is used to track the instance-related information that is shared
among translation units.
*/
typedef struct a_master_instance *a_master_instance_ptr;
typedef struct a_master_instance {
  a_master_instance_ptr
		next;	/* Pointer to the next entry in the list of master
			   instance entries, or NULL for the last entry. */
  a_template_instance_ptr
		instance;
			/* Pointer to one of the instance entries associated
			   with one of the translation units.  This points to
			   an arbitrary instance, and not necessarily the
			   canonical one. */
  char		*name;
			/* The mangled name of the entity. */
  an_instance_required_count
		instance_required_count;
			/* The number of translation units for which the
			   instantiation_required flag is set for this
			   instance. */
  a_bit_field	already_instantiated:1;
			/* TRUE if instantiation has already been performed
			   (for instance, for inline functions, which are
			   instantiated at the point of first reference). */
  a_bit_field	automatically_instantiated:1;
			/* TRUE if the instance was listed in the instantiation
			   request file as an instantiation assigned to
			   this compilation.  This field is only used when
			   automatic template instantiation is configured. */
  a_bit_field	add_to_request_file:1;
			/* TRUE if the instance was "adopted" by this
			   translation unit because it was known not to be
			   defined elsewhere.  A list of these entities is
			   returned to the prelinker to be appended to
			   the instantiation request file. */
  a_bit_field	is_static_or_inline:1;
			/* TRUE if the routine has previously been determined
			   to be static or inline (or should be treated as
			   such for instantiation purposes).  This is the saved
			   result of is_static_or_inline_template_entity.
			   That routine should always be used instead of
			   checking this flag directly (because it may not
			   have been set yet). */
} a_master_instance;


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
  a_master_instance_ptr
		master_instance;
			/* Pointer to the master instance that contains
			   information about this instance that is shared
			   among translation units. */
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
  a_namespace_ptr
		referencing_namespace;
			/* Pointer to the namespace in which the use that
			   first required the instantiation of the template
			   was encountered.  NULL if the first reference
			   was in the global namespace.  This field is
			   set when the instantiation_required flag is
			   set. */
  a_template_symbol_supplement_ptr
		template_info;
			/* Pointer to associated template information when
			   instance_sym points to a member of a prototype
			   instantiation of a class template (in which case
			   instance_sym == template_sym and the instance is
			   not a "real" instance but a kind of template for a
			   member function or a static data member).  Otherwise
			   (i.e., usually) NULL. */
  a_symbol_ptr	prototype_scope_symbols;
			/* For member and nonmember functions, a list of
			   symbols in the prototype scope, linked on the
			   next_in_scope field.  NULL if none. */
  an_exported_template_file_ptr
		exported_template_file;
			/* If this is an instance of an exported template,
			   this points to an entry that describes the file in
			   which the template definition was found.  This is
			   set when determining whether it is possible to
			   generate the instance. */
  a_bit_field	instantiation_required:1;
			/* TRUE if a routine body or static data member
			   definition needs to be generated for this instance.
			   This flag is FALSE if an explicit definition has
			   been provided by the user (i.e., if specific_def
			   is set). */
  a_bit_field	suppress_instantiation:1;
			/* TRUE if the instantiation of this entity should be
			   suppressed because of previous errors that occurred
			   during the partial instantiation of the entity. */
  a_bit_field	is_guiding_decl:1;
			/* For instances of nonmember function templates,
			   TRUE if this instance is a guiding declaration
			   (i.e., if it has been explicitly declared as
			   though it were a normal function -- in which case
			   instance_sym has been added to the overload list
			   for this name).  Undefined for member functions
			   and static data members of template classes. */
  a_bit_field	explicit_instantiation:1;
			/* TRUE if an instantiation has been explicitly
			   requested using a pragma directive. */
  a_bit_field	class_explicitly_instantiated:1;
			/* TRUE if the instantiation request specified the
			   class (meaning that all its members should be
			   instantiated).  When the class is specified for
			   instantiation, no error is issued if template
			   definitions are not available for some of the
			   members. */
  a_bit_field	explicit_do_not_instantiate:1;
			/* TRUE if instantiation has been explicitly 
			   suppressed by a do_not_instantiate pragma. */
  a_bit_field	explicit_can_instantiate:1;
			/* TRUE if instantiation has been explicitly declared
                           as being possible by a can_instantiate pragma. */
  a_bit_field	can_be_instantiated:1;
			/* TRUE if this entity can be instantiated.  This
			   means that a template definition is available
			   if one is needed.  Note that if this flag is FALSE
			   it does not necessarily mean that the entity cannot
			   be instantiated, because a definition may have
			   been supplied since the last time the check was
			   done.  The can_be_instantiated routine should
		           be used instead of this field. */
  a_bit_field	on_instantiations_list:1;
			/* TRUE if this entry is already on the instantiations
			   required list. */
  a_source_position
		explicit_instantiation_pos;
			/* The position of the instantiation request pragma
			   when explicit_instantiation is TRUE. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_type_ptr	declared_type;
			/* When instance_sym points to an sk_routine or
			   sk_member_function, pointer to the routine's type
			   as it actually appears in the source program (i.e.,
			   before parameter type adjustments). */
  a_type_ptr	declared_type_for_default_arg_fixup;
			/* Same as declared_type, but only when the declared
			   type is a candidate for default argument fixup.
			   (It will not be, for instance, when no source
			   sequence entry is generated to record the declared
			   type.)  NULL when default arg fixup is not
			   appropriate. */
  a_param_id_ptr
		param_id_list;
			/* List of entries describing parameter symbols.
			   NULL if there were none. */
#if NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
  a_source_sequence_entry_ptr
		partial_instantiation;
			/* An iek_src_seq_secondary_decl source sequence
			   entry representing the partial instantiation of
			   a function template that is dependent on a class
			   that is currently being defined.  As long as this
			   pointer is non-NULL, the source sequence list has
			   not yet been updated. */
#endif /* NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
} a_template_instance;


/*
Structure used to keep track of the segments of a template token cache
that are used to record the definition of member classes and member
functions of a class template definition.  This information is used
to extract the member bodies from the enclosing token cache.
*/
typedef struct a_template_cache_segment {
  a_template_cache_segment_ptr
		next;
			/* Pointer to the next entry in a list of template
			   cache segments. */
  a_symbol_ptr	symbol;
			/* Pointer to the symbol entry for the member
			   associated with this entry. */
  a_template_symbol_supplement_ptr
		template_info;
			/* Pointer to the template supplement for
			   template_sym. */
  a_token_sequence_number
		first_token_number;
			/* Token sequence number of the first token in
			   the definition of the template. */
  a_token_sequence_number
		last_token_number;
			/* Token sequence number of the last token in
			   the definition of the template. */
  a_cached_token_ptr
		before_first_token;
			/* Pointer to the token before the first token of
			   the cache.  This is initially set to NULL.
			   Then, a pass is made through the enclosing
			   token cache and the first and last token
			   pointers in all of the associated template
			   cache segment entries are updated. */
  a_cached_token_ptr
		last_token;
			/* Pointer to the last token of the cache.  See
			   first_token above. */
  a_byte_boolean
		is_friend;
			/* TRUE if this entry represents a friend function. */
  a_byte_boolean
		is_default_arg;
			/* TRUE if this entry represents a default argument
			   expression. */
  a_byte_boolean
		default_arg_missing;
			/* TRUE if the default argument expression is empty.
			   (e.g., "void f(int=)"). */
} a_template_cache_segment;


/*
Structure that contains information associated with a friend template
declared in a class template.
*/
typedef struct a_templ_friend_info *a_templ_friend_info_ptr;
typedef struct a_templ_friend_info {
  a_templ_friend_info_ptr
		next;
			/* Pointer to the next entry on the list, or NULL
			   for the last entry. */
  a_symbol_ptr	symbol;
			/* Pointer to the symbol entry for the friend
			   declaration. */
  a_token_sequence_number
		token_number;
			/* Then token sequence number of end of the friend
			   function declaration.  This is used to associate
			   a declaration in a real instantiation with the
			   corresponding declaration in the prototype
			   instantiation. */
} a_templ_friend_info;


/*
Entry used to record information about partial specializations of members
of class templates that are declared outside of the parent class template.
When an instance of the enclosing class template is instantiated, each
of the entries on this list is processed to create a declaration of the
partial specialization for that instance of the enclosing class.
*/
typedef struct an_out_of_class_partial_spec *an_out_of_class_partial_spec_ptr;
typedef struct an_out_of_class_partial_spec {
  an_out_of_class_partial_spec_ptr
		next;	/* Pointer to the next entry on the list or NULL for
			   the last entry. */
  a_symbol_ptr	symbol;
			/* The class template symbol of the partial
			   specialization. */
  a_template_cache
		cache;	/* The cache containing the declaration of the
			   partial specialization. */
  struct a_tmpl_decl_state
		*tmpl_decl_state;
			/* Pointer to a copy of the template declaration
			   state entry used when the partial specialization
			   was first scanned. */
} an_out_of_class_partial_spec;


/* Used to track the number of pending instantiations of a given class. */
typedef unsigned long a_pending_instantiation_count;

/* Used to track the number of instantiations performed in tim_all mode that
   were not actually required. */
typedef short an_unused_instantiation_count;


typedef struct a_template_symbol_supplement {
  /* Additional information about a C++ class or function template
     supplementing the information residing in the class's symbol entry. */
  a_template_cache
		cache;
                        /* The tokens comprising the template are cached
                           in order to be rescanned later during
                           instantiation.  Typically begins with the left
			   brace that begins the class or function body and
                           extends to the right brace; for constructors it
			   may begin at a colon.  For templates for static
			   data members it embraces the initializer
			   expression, if any. */
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
			/* This is used for member functions, member
			   function templates, and static data members
 			   to match the declarations of the prototype
 			   instantiation (to which the template symbol
 			   supplement is attached) to declarations
 			   found inside real instantiations.  This
 			   field contains the token sequence number of
 			   a certain token within the declaration. */
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
  a_template_cache_segment_ptr
		cache_segment;
			/* Pointer to a structure that describes the
			   range of tokens from the template cache
			   of the enclosing template that contain the
			   definition of this template.  This field  is
			   used to extract the definitions of member
			   functions and nested classes from the bodies
			   of class template definitions. */
  a_symbol_ptr	prototype_template;
			/* If this is a member template of a class template
			   instance, this points to the template symbol for
			   the original member template declaration in the
			   prototype instantiation.  This pointer will be
			   set even if the member template is specialized in
			   one of the instances of the enclosing class
			   template.  In other words, the
			   is_specific_definition flag must be used to
			   determine whether the cache information from
			   the prototype template or the cache information
			   from this template should be used. */
  a_symbol_list_entry_ptr
		subordinate_templates;
			/* If this is a member template of a prototype
			   instantiation, this points to a list of template
			   symbols for the templates generated from this
			   template. */
  a_template_ptr
		il_template_entry;
			/* When  the symbol kind is sk_class_template or
			   sk_function_template, the IL entry created to
			   represent this template.  Points to the entry
			   associated with the first declaration. */
  a_symbol_list_entry_ptr
		all_instantiations;
			/* When secondary translation units are processed,
			   this points to a list of all the instantiations of
			   this template (across all translation units).
                           Only set for the canonical entry. */
  char		*name;
			/* The mangled name of the entity.  Used for exported
			   templates. */
  a_bit_field
		is_specific_definition:1;
			/* TRUE if the template is a specific definition of
			   a member template. */
  a_bit_field	is_nonreal_member:1;
			/* TRUE if the template was created as a member of
			   a proxy or nonreal class and does not represent
			   an actual template declaration. */
  a_bit_field	is_error:1;
			/* TRUE if this is an error class template created
			   for error recovery purposes. */
  bitfield_to_avoid_codecenter_warnings()
  union {
    /* When symbol kind = sk_class_template: */
    struct {
      a_symbol_ptr
                instantiations;
                        /* Pointer to a list of symbols describing template
                           classes that have been instantiated from this
                           class template.  Nonreal classes are included
			   in this list, but prototype instantiations are
			   not. */
      a_type_kind
		type_kind;
			/* The kind (tk_class, tk_struct, or tk_union) which
			   the instantiated types will have. */
      a_symbol_ptr
		prototype_instantiation;
			/* Points to the symbol representing the prototype
			   instantiation. */
      a_symbol_ptr
		partial_specializations;
			/* A list of class template symbols for partial
			   specializations of the current class template.
			   This is present only for class templates that are
			   "primary" templates (i.e., those that are not
			   already partial specializations).  NULL for
			   templates with no partial specializations or for
			   templates that are already partial
			   specializations. */
      a_symbol_ptr
		primary_template_sym;
			/* For partial specialization, points back to the
			   primary template of which this is a partial
			   specialization. */
      an_out_of_class_partial_spec_ptr
		out_of_class_partial_specs;
			/* When a partial specialization of a class template
			   that is a member of another class template is
			   declared outside of the enclosing class, the
			   partial specialization must be evaluated for
			   each instantiation of the enclosing class.  This
			   happens automatically for partial specializations
			   that appear inside the enclosing class (because
			   those tokens are rescanned during the instantiation
			   of the enclosing class).  For partial
			   specializations that appear outside of the class
			   this is done by rescanning the declarations
			   associated with the entries on this list. */
      a_templ_friend_info_ptr
		friend_info;
			/* Information about default arguments of friend
			   templates declared in this class template. */
      a_symbol_ptr
		argument_template;
			/* For a class template associated with a template
			   template parameter, points to the symbol of the
			   actual template template argument for the
			   current instantiation. */
      a_bit_field
		prototype_instantiation_complete:1;
			/* TRUE when the prototype instantiation of the
			   class template has been completed.  Used to
			   prevent a real instantiation from occurring while
			   the prototype instantiation is in progress. */
      a_bit_field /* a_name_linkage_kind */
		name_linkage:NUM_BITS_FOR_NAME_LINKAGE;
			/* The name linkage associated with this class
			   template -- typically C++ linkage, but internal
			   linkage if the template is declared inside an
			   unnamed namespace. */
      a_bit_field
		not_standalone_nested_class:1;
			/* TRUE for nested classes of class templates in
			   which the definition of the nested class cannot
			   be extracted from the token cache for the
			   enclosing template because it is part of the
			   declaration of some other entity in the enclosing
			   class.  For example, "struct { ... } a;". */
      a_bit_field /* an_access_specifier */
		access:2;
			/* If the template is a member of a class, this
                           specifies the access for the member. */
      a_bit_field
		template_template_param:1;
			/* TRUE if this is a class template symbol associated
			   with a template template parameter. */
      a_bit_field
		any_full_instantiations:1;
			/* TRUE if any full instantiations have been done of
			   this template or any of its partial
			   specializations. */
      bitfield_to_avoid_codecenter_warnings()
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
      a_source_sequence_entry_ptr
		source_sequence_list;
			/* List of source-sequence entries collected during
			   prototype instantiation of the class template;
			   May be NULL. */
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
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
			   this template declaration.  For a subordinate
			   template this points to the default argument
			   list of the prototype template. */
      a_template_cache
		decl_cache;
			/* A cache of the tokens that comprise the function
			   declaration.  These are rescanned later to create
			   routine types for instances of the function
			   template.  The cache begins with the first token
			   of the function declaration (the token after the
			   closing ">" of the template parameter list) and
			   ends with the last token of the function
			   declarator. */
      a_substituted_type_list_entry_ptr
		substituted_types;
			/* A list of template argument lists and the type
			   that results from substituting the template
			   parameters in the template routine types with
			   specified template arguments.  This is used by
			   substitute_template_arguments to determine whether
			   a type has already been produced for a given
			   template argument list. */
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
      a_pending_instantiation_count
		pending_partial_instantiations;
			/* The number of partial instantiations of this
                           template that are in the process of being
			   instantiated.  Used to detect runaway recursive
			   instantiations. */
      a_symbol_ptr
		prototype_friend_symbol;
			/* If this template was declared as a friend of a
			   class template this field is used for friend
			   declarations of real instantiations of the
			   class template and points to the corresponding
			   friend symbol from the prototype instantiation
			   of the class template. */
      a_bit_field
		template_param_not_in_function_type:1;
			/* TRUE if the function template has template
			   parameters that are not used in the function
			   type. */
      bitfield_to_avoid_codecenter_warnings()
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


typedef struct a_namespace_symbol_supplement
                                        *a_namespace_symbol_supplement_ptr;
typedef struct a_namespace_symbol_supplement {
  a_scope_pointers_block
		pointers_block;
			/* A block of pointers that are logically part of the
			   scope stack entry for the associated namespace
			   -- including a pointer to a linked list of all
			   symbols declared in the namespace and pointers to
			   the last entries in linked lists of IL entries
			   entered in the associated IL scope. */
  a_scope_depth
		scope_depth_at_which_using_directive_applies;
			/* Contains the scope depth of the scope at which
                           symbols from this namespace should be visible.
                           This flag is set when a using directive is
			   added to the active using list of a scope stack
                           entry.  Contains NO_SCOPE_DEPTH if symbols from
                           this namespace are not visible.  If a namespace
                           is visible at more than one point, this contains
                           the depth of the innermost scope at which it
                           is visible. */
  a_scope_depth
		depth_innermost_active_using_directive;
			/* The scope depth at which the innermost using
			   directive for this namespace appeared.  Contains
			   NO_SCOPE_DEPTH if there are no active using
			   directives for this scope.  Used to optimize
			   certain tests of whether or not a namespace is on a
			   scopes active using list. */
  a_namespace_list_entry_ptr
		namespace_list_entry;
			/* A namespace list entry that points to the associated
			   namespace.  This is used so that the
			   operator_lookup_namespaces pointer in the
			   class symbol supplement can point to a common
			   entry for all of the leaf classes (i.e., most
			   base classes) in a given namespace. */
  a_bit_field	visited_by_qualified_lookup:1;
			/* Used by the qualified lookup routines to indicate
			   that this namespace has already been visited. */
  a_bit_field	within_unnamed_namespace:1;
			/* TRUE when the namespace is itself an unnamed
			   namespace or is enclosed by an unnamed namespace. */
} a_namespace_symbol_supplement;


/*
An entry corresponding to an IL entry of kind a_using_directive and containing
front-end-only information.  (Note: a using-directive is a declaration of the
form "using namespace N"; it should not be confused with "using N::x" or
"using ::x", which are referred to as "using-declarations".)
*/
typedef struct an_active_using_directive {
  an_active_using_directive_ptr
		next;
			/* Next in the linked list of active using-directives
			   associated with the current scope or namespace. */
  a_using_decl_ptr
		entry;
			/* The IL entry to which this front-end only entry
			   corresponds; there is a one-to-one correspondence
			   between the two sorts of entries, though a pointer
			   is required in one direction only. */
  a_namespace_symbol_supplement_ptr
		namespace_supplement;
			/* The namespace symbol supplement associated with
			   the namespace referenced in the using directive.
			   If the using directive refers to a namespace
			   alias, this field points to the namespace
			   supplement associated with the underlying
			   namespace. */
  a_scope_depth
		scope_depth_at_which_using_directive_applies;
			/* Contains the scope depth of the scope at which
                           symbols from this namespace should be visible.
                           This value is copied to the namespace symbol
                           supplement when the active using list is
			   processed. */
  a_decl_sequence_number
		effective_decl_seq;
			/* The declaration sequence number of the point at
			   which this using-directive comes into effect.
			   This is usually the declaration sequence number
			   of the using-directive, but for a namespace made
			   visible as a result of the transitivity of
			   using-directives, this will be the declaration
			   sequence number of the outermost using-directive. */
} an_active_using_directive;


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
  a_parent_class_or_namespace
		parent;
			/* When is_class_member is TRUE, parent.class_type
			   points to the class of which the current symbol is
			   a member; it may be assumed to be non-NULL.  When
			   is_class_member is FALSE and the current entity
			   was declared to be a namespace member (C++ only),
			   parent.namespace_ptr points to the namespace;
			   otherwise it is NULL. */
  a_symbol_kind kind;
			/* The kind of symbol. */
  a_bit_field	referenced:1;
			/* TRUE if the symbol is actually referenced, not just
			   declared. */
  a_bit_field	defined:1;
			/* TRUE if the symbol is actually defined, not just
			   declared. */
  a_bit_field	explicit_linkage_specifier:1;
			/* TRUE for variables and routines for which an
			   explicit external linkage was specified (e.g.,
			   ``extern "C"'' -- C++ only). */
  a_bit_field	reentered_from_prototype_scope:1;
			/* TRUE if symbol was originally declared in a
			   function prototype scope and was subsequently
			   reentered in the function scope. */
  a_bit_field	is_class_member:1;
			/* TRUE if symbol represents a C++ class member; also
			   TRUE for fields in C.  (Note: it is not set for
			   anonymous union members whose names are promoted to
			   a non-class scope.)  */
  a_bit_field	is_error:1;
			/* TRUE if the symbol represents an identifier for
			   which an error has been diagnosed and which should
			   not be entered into the symbol table. */
  a_bit_field	is_template_param:1;
			/* TRUE if the symbol represent a template
			   parameter. */
  a_bit_field	template_param_not_visible:1;
			/* TRUE if this is a template parameter that should
			   not be visible for name lookup purposes at this
			   point in time. */
  a_bit_field	force_external_linkage:1;
			/* TRUE if this is a class or enum type that has been
			   used in a way that would force external linkage (if
			   it has linkage at all).  Maintained only in
			   cfront mode (in other modes, the linkage of a class
			   or enum is not affected by the ways it is used). */
  a_bit_field	ambiguous:1;
			/* TRUE if the symbol name is ambiguous in
			   the current scope, i.e., another symbol with the
			   same name is visible, and there is no reason to
			   prefer one over the other.  This is used for
			   sk_projection, sk_namespace_projection, and
			   sk_overloaded_function symbols that are
			   synthesized namespace projection symbols. */
  a_bit_field	synthesized_namespace_projection:1;
			/* TRUE for sk_namespace_projection and
			   sk_overloaded_function symbols that were created
			   as a result of a lookup that found one or more
			   symbols that are visible as a result of
			   using directives.  Also TRUE for
                           sk_overloaded_function symbols created by
			   template instantiation lookups. */
  a_bit_field	qualified_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that were generated as a result of a namespace
			   qualified lookup. */
  a_bit_field	must_be_class_or_namespace_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that were generated as a result of an
			   IDL_MUST_BE_CLASS_OR_NAMESPACE lookup. */
  a_bit_field	must_be_tag_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that were generated as a result of an
			   IDL_MUST_BE_TAG lookup. */
  a_bit_field	tentative_type_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that were generated as a result of an
			   IDL_TENTATIVE_TYPE_LOOKUP lookup. */
  a_bit_field	do_not_reuse:1;
			/* TRUE for synthesized namespace symbols generated
			   as a result of a special lookup that cannot be
			   reused by a subsequent lookup. */
  a_bit_field	instantiation_context_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that are generated as a result of an instantiation
			   context lookup. */
  a_bit_field	must_be_class_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that were generated as a result of an
			   IDL_MUST_BE_CLASS lookup. */
  a_bit_field	must_be_namespace_lookup:1;
			/* TRUE for synthesized namespace projection symbols
			   that were generated as a result of an
			   IDL_MUST_BE_NAMESPACE lookup. */
  a_bit_field	hidden_by_old_for_init:1;
			/* TRUE for a symbol that is visible with the new
			   for-init scoping rules but would be hidden if old
			   (cfront-compatible) scoping were used.  For example,
			       int i;
			       void f() {
			         for (int i = 0; i < 10; i++) { ... }
			         return i;
			       }
			   Under the old scoping rules the local i is returned
			   and ::i is hidden, but by the new rules the local
			   i goes out of scope and ::i is returned.  Unless
			   global flag use_nonstandard_for_init_scope is TRUE,
			   hidden_by_for_init will be set for ::i in the
			   function scope (following the termination of the
			   for statement) and will be cleared again once the
			   function scope is terminated.  A symbol for which
			   this flag is set is pointed to by an entry of type
			   a_name_hidden_by_old_for_init entry, accessed from
			   the scope stack.  (Used in C++ only.) */
  a_bit_field	overload_set_member:1;
			/* TRUE for a symbol that is on the symbols list of
			   an sk_overloaded_function symbol. */
  a_bit_field	is_invisible:1;
			/* TRUE for a symbol that is "invisible" (i.e., to be
			   ignored during normal lookup).  This occurs when
			   the initial declaration of a function or class is
			   a friend declaration; the entity becomes visible
			   only when it is subsequently declared in the
			   scope to which it belongs.  This is also used for
			   projection symbols to names found in base classes
			   that are ignored during normal lookup (when doing
			   dependent name processing. */
  a_bit_field	is_unknown_function:1;
			/* TRUE if this symbol was created to represent an
			   unknown function. */
  a_bit_field	is_nonreal_member:1;
			/* TRUE if this symbol represents a member of a
			   nonreal class. */
  /* bitfield_to_avoid_codecenter_warnings() -- at byte boundary right now. */
  union {
    /* When kind == sk_undefined, no variant fields. */
    /* When kind == sk_keyword: */
    struct {
      a_byte_token_kind
		token;
			/* For keywords, the token identifying the keyword. */
      a_bit_field
		is_preprocessing_op_or_punc:1;
			/* TRUE for symbols corresponding to keywords that
			   are also preprocessing tokens (e.g., "and"). */
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
    /* When kind == sk_type: */
    struct {
      a_type_ptr
		ptr;
			/* The type. */
      a_byte_boolean
		is_injected_class_name;
			/* TRUE if the symbol represents an injected class
			   name generated by the compiler (C++ only). */
    } type;
    /* When kind == sk_enum_tag: */
    struct {
      a_type_ptr
		type;
			/* The type that represents the enumeration. */
      a_dependent_type_fixup_ptr
		dependent_type_fixup_list;
			/* If the enum type is not yet defined (possible as
			   an extension in both C and C++ modes), a pointer
			   to a list of entries identifying arrays that are
			   dependent on it and require fixup when it is
			   completed.  Once the enum is defined, the pointer
			   is cleared. */
    } enumeration;
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
      a_bit_field
		value_has_been_set:1;
			/* TRUE if the variable was initialized (explicitly or
			   implicitly), has been assigned to, or has had its
			   address taken.  Also TRUE if it is of aggregate
			   type and at least one of its fields or elements has
			   been assigned to or has had its address taken.
			   Also TRUE if its storage class is extern, since its
			   value will be set where in the definition. */
      a_bit_field
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
      a_bit_field /*an_access_specifier*/
		access:2;
			/* Access to the base class member in the scope of the
			   derived class.  This may differ from the access
			   with which it was originally declared in its own
			   class: when the projection symbol represents a
			   using declaration or access adjustment (i.e., when
			   is_using_decl is TRUE), the access is the declared
			   access in the derived class; otherwise, the access
			   is computed based on both the access of the member
			   within the base class and the access of the base
			   class itself (along the "preferred derivation" --
			   the path affording greatest access -- when there
			   are multiple paths) within the derived class. */
      a_bit_field
		is_using_decl:1;
			/* If TRUE this projection symbol represents a
			   using-declaration. */
      a_bit_field
		any_intervening_using_decl:1;
			/* TRUE if the access of the inherited name was
			   modified by an using-declaration anywhere on the
			   derivation path between the fundamental symbol and
			   the current projection. */
      a_bit_field
		fund_sym_is_nonreal_member:1;
			/* TRUE if the fundamental symbol is a member of
			   a nonreal or proxy class.  Such members are
			   created as a result of a class-qualified
			   lookup of a member in the prototype instantiation
			   of a derived class, when the lookup fails to find
			   a member in the derived class or any of the real
		 	   base classes. */
      a_bit_field
		injected_class_template_name_is_unambiguous:1;
			/* If ambiguous is TRUE, this flag is TRUE if the
			   fundamental symbols represent injected class names
			   for instances of the same class template; if
			   ambiguous is FALSE, this flag is undefined.
			   The flag is TRUE in cases like this:
			     class B : public A<int>, public A<float> { ... };
			   where within B the projections of the injected
			   class names of the base classes are ambiguous
			   insofar as one is interested in the base class type
			   and unambiguous insofar as one is interested in
			   the template. */
    } projection;
    /* When kind = sk_overloaded_function: */
    struct {
      a_symbol_ptr
		symbols;
			/* Linked list of two or more symbols comprising a
			   function overload set, where each symbol in the
			   list has the same name as the current symbol.  When
			   the latter is a class member, each symbol in the
			   list is an sk_member_function or
			   sk_function_template symbol or an sk_projection
			   symbol that points to an sk_member_function or
			   sk_function_template fundamental symbol.  When the
			   current symbol is not a class member and
			   synthesized_namespace_projection is FALSE, each
			   symbol is an sk_routine or sk_function_template
			   symbol or an sk_namespace_projection symbol that
			   points to an sk_routine or sk_function_template
			   symbol.  When synthesized_namespace_projection is
			   TRUE, the symbols in the list can be a combination
			   of sk_routine, sk_member_function, sk_projection,
			   sk_namespace_projection, and sk_function_template
			   symbols (and the sk_function_template symbols can
			   be members and/or nonmembers). */
      a_byte_boolean
		mixed_static_nonstatic;
			/* TRUE when the current symbol is a class member and
			   some but not all the members of the overload set
			   are static member functions. */
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
    /* When kind == sk_namespace: */
    struct {
      a_namespace_ptr
		ptr;
			/* The IL entry for the namespace. */
      a_namespace_symbol_supplement_ptr
		extra_info;
			/* Pointer to an entry providing additional info
			   about a C++ namespace definition; NULL when the
			   symbol represents a namespace alias. */
    } namespace_info;
    /* When kind == sk_namespace_projection: */
    struct {
      a_symbol_ptr
		fundamental_symbol;
			/* Pointer to the symbol representing the fundamental
			   namespace member to which this projection refers.
			   For instance:
			     namespace A { int i; }
			     namespace B { using A::i; }
			     namespace C { using B::i; }
			   The sk_namespace_projection symbols in the scopes
			   of B and C both point to A::i as fundamental
			   symbol: the fundamental_symbol is never itself an
			   an sk_namespace_projection symbol.  Nor will the
			   fundamental symbol be an sk_overloaded_function
			   symbol; rather, separate projection symbols will be
			   created for members of the fundamental namespace's
			   overload set. */
    } namespace_projection;
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
  a_symbol_ptr	other_symbols;
			/* sk_extern_variable, sk_extern_routine and
                           synthesized namespace projection symbols
			   associated with this name. */
  a_bit_field	any_nested_types_on_inactive_list:1;
			/* TRUE if a symbol for a nested type has been
                           transferred to the inactive list.  This field is
                           used to speed up processing to support the
                           nested class anachronism (ARM 18.3.5) and is
                           only set when anachronisms are allowed. */
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
  a_bit_field	has_cfront_transitional_nested_type_mangled_name:1;
                        /* TRUE if a nested type has been flagged for
                           special handling during name mangling.  The first
                           nested type with a given name will have this flag
                           set indicating that its name should be mangled as
                           if it were not a nested type. */
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#if RECORD_HIDDEN_NAMES_IN_IL
  a_bit_field	any_tag_decl:1;
			/* TRUE if any symbol represents a tag declaration. */
  a_bit_field	any_decl_in_file_or_namespace_scope:1;
			/* TRUE if any symbol represents a declaration in
			   the file scope or in a namespace scope (i.e., a
			   non-class-member declaration that can be referred
			   to with a qualified name). */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
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

EXTERN an_active_using_directive_ptr
		avail_active_using_directives;
			/* List of active using directive entries freed and
			   available for reuse. */

EXTERN sizeof_t	size_scope_stack /* = 0*/;
			/* Allocated size of scope_stack in elements.
			   Not per-file. */


/*
Entry describing a fixup that is required for a VLA that appears in a
function prototype paramenter declaration.  There are two sorts of fixup that
happen once the function scope and its associated memory region are created:
(1) Parameter variable fixup -- If the dimension expression refers to a
    parameter name, the expression will have been scanned before the param
    variable was actually created (since it cannot be created until the
    function's IL scope exists).  Instead, the expression will refer to a
    dummy variable, and the fixup involves replacing the dummy variable with
    the "real" param variable.
(2) Dimension expression fixup -- Every VLA dimension expression is
    eventually represented by a VLA-dimension entry, which points to the
    expression node.  Both the VLA-dimension entry and the expression node
    have to be in the scope of the function, but when the expression is
    originally scanned, the function's IL scope does not yet exist, so the
    fixup involves copying the expression node into the function scope memory
    region and allocating the VLA-dimension entry to point to it.
When no function definition is associated with the function prototype
declaration, the fixup entries are discarded.
*/
typedef struct a_vla_fixup {
  a_vla_fixup_ptr
		next;
			/* Pointer to the next fixup entry on the list. */
  a_type_ptr	array_type;
			/* Pointer to a type entry for a variable length
			   array.  If it is non-NULL, dimension expression
			   fixup is required; otherwise, parameter variable
			   fixup is required. */
  an_expr_node_ptr
                expr;   /* If array_type is NULL, a pointer to an enk_variable
			   or enk_variable_address expression node which needs
			   to be patched with the correct variable for the
			   function parameter.  If array_type is non-NULL, a
			   pointer to an expression representing a variable
			   dimension, and dimension expression fixup will be
			   done. */
  a_symbol_ptr	param_sym;
			/* If array_type is NULL, a pointer to the parameter
			   symbol associated with the param variable fixup.
			   NULL if array_type is non-NULL. */
  a_source_position
		position;
			/* Source position of the VLA expression. */
} a_vla_fixup;


extern void add_vla_fixup_entry(a_type_ptr        array_type,
                                an_expr_node_ptr  expr_node,
                                a_symbol_ptr      param_sym,
                                a_source_position *position);

extern void free_vla_fixup_list(a_vla_fixup_ptr vfp);


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


/* Contains a description of an exception specification error that has been
   detected and for which an error may need to be issued later. */
typedef struct an_exception_spec_error_descr {
  an_exception_spec_error_descr_ptr
		next;
			/* Pointer to the next error description record. */
  a_source_position
		position;
			/* Position to be used when the diagnostic is
			   issued. */
  an_error_code
		error_code;
			/* The code indicating the diagnostic message to be
			   issued. */
} an_exception_spec_error_descr;

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

extern
a_boolean find_projected_symbol(
			a_type_ptr               class_ptr,
                        a_symbol_locator         *locator,
                        an_id_lookup_options_set options,
			a_boolean		 look_in_dependent_bases,
                        a_boolean                tentative_type_lookup,
                        a_boolean                tentative_template_lookup,
			a_boolean		 do_not_create_proj_sym,
                        a_boolean                add_to_active_list,
                        a_symbol_ptr             insert_sym,
                        a_symbol_ptr             *projected_symbol,
                        a_boolean		 can_create_nonreal);

extern a_boolean looks_like_ctor_or_dtor(a_symbol_locator  *loc);

extern void make_locator_for_symbol(a_symbol_ptr     sym_ptr,
                                    a_symbol_locator *location);

extern void make_specific_symbol_error_locator(a_symbol_locator *locator);

extern void clear_qualifier_from_locator(a_symbol_locator  *locator);

extern a_symbol_ptr corresp_prototype_for_class_symbol(a_symbol_ptr sym);

extern a_symbol_ptr template_symbol_for_class_symbol(a_symbol_ptr class_sym);

extern
a_template_cache_segment_ptr alloc_template_cache_segment(
                                a_symbol_ptr				sym,
                                a_template_symbol_supplement_ptr	tssp);

extern void free_template_cache_segment(a_template_cache_segment_ptr tcsp);

extern an_out_of_class_partial_spec_ptr alloc_out_of_class_partial_spec(void);

extern a_template_decl_info_ptr alloc_template_decl_info(void);

extern a_nondependent_call_info_ptr get_nondependent_call_info(
				a_token_sequence_number		tsn);

extern void record_nondependent_call(a_symbol_ptr		symbol,
				     a_token_sequence_number	tsn);

extern a_templ_friend_info_ptr alloc_templ_friend_info(void);

extern
void clear_template_cache(a_template_cache_ptr	tcp,
                          a_boolean		is_reusable);

extern
void set_template_cache_info(a_template_cache_ptr	tcp,
			     a_token_cache_ptr		tokens,
			     a_template_decl_info_ptr	tdip);

extern a_template_symbol_supplement_ptr alloc_template_symbol_supplement(
                                                         a_symbol_kind  kind);

extern a_symbol_ptr enter_symbol(a_symbol_kind    sym_kind,
				 a_symbol_locator *location,
                                 a_scope_depth    scope_depth,
                                 a_boolean        suppress_error);

extern void reenter_symbol(a_symbol_ptr     symbol_to_reenter,
                           a_scope_depth    scope_depth,
                           a_boolean        suppress_error);

extern void enter_copy_of_symbol(a_symbol_ptr     orig_sym,
				 a_scope_depth    scope_depth,
		                 a_boolean        suppress_error);

extern a_symbol_ptr enter_extern_symbol(a_symbol_kind    sym_kind,
                                        a_symbol_locator *locator);

extern void reactivate_prototype_scope_symbols(
                                        a_symbol_ptr  prototype_scope_symbols);

extern void relink_unnamed_tag_symbol(a_symbol_ptr      sym,
                                      a_symbol_locator  *locator);

extern void enter_undefined_symbol(a_symbol_ptr sym);

extern a_symbol_ptr enter_undefined_member_symbol(a_symbol_locator *locator);

extern a_symbol_ptr make_namespace_projection_symbol(
                                              a_symbol_ptr       fund_sym,
                                              a_source_position  *pos,
                                              a_scope_depth      scope_depth);

extern void set_namespace_projection_symbol(a_symbol_ptr     proj_sym,
                                            a_symbol_ptr     fund_sym,
                                            a_scope_depth    scope_depth);

extern a_symbol_ptr enter_namespace_projection_symbol(
                                            a_symbol_ptr    fund_sym,
                                            a_symbol_locator *location,
                                            a_scope_depth   scope_depth,
                                            a_boolean       suppress_error);

extern void add_friend_function_to_lookup_list_for_class(
                                                  a_symbol_ptr  rout_sym,
                                                  a_type_ptr    class_type);

extern a_symbol_ptr enter_synthesized_projection_symbol(
                               a_symbol_ptr		fund_sym,
                               a_symbol_locator		*location,
                               a_boolean		qualified_lookup,
                               a_namespace_ptr		qualifier_namespace,
                               an_id_lookup_options_set	options);

extern a_symbol_ptr add_symbol_to_overload_list(a_symbol_ptr    new_sym,
                                                a_symbol_ptr    other_sym,
                                                a_boolean	use_namespace,
                                                a_namespace_ptr ns_ptr);

extern a_symbol_ptr enter_overloaded_symbol(a_symbol_kind    sym_kind,
                                            a_symbol_locator *location,
                                            a_boolean        is_constructor,
                                            a_symbol_ptr     old_sym_ptr,
                                            a_symbol_ptr     *overload_sym);

extern a_base_class_ptr find_base_with_type(a_type_ptr        base_type,
                                            a_type_ptr        class_type,
                                            a_base_class_ptr  ref_bcp);

extern a_symbol_ptr make_projection_symbol(a_symbol_ptr      progenitor_sym,
                                           a_type_ptr        class_ptr,
                                           a_base_class_ptr  fundamental_bcp,
                                           a_derivation_step *path,
                                           a_boolean         ambiguous);

extern a_symbol_ptr make_function_template_prototype_symbol(
				a_symbol_ptr		template_sym,
				a_routine_ptr		rout_ptr,
				a_template_param_ptr	templ_param_list);

extern a_symbol_ptr make_template_class_symbol(a_symbol_ptr  ct_symbol);

extern
a_symbol_ptr make_template_function_symbol(a_symbol_ptr       templ_sym,
                                           a_source_position  *pos,
                                           a_type_ptr         rout_type);

extern a_symbol_ptr error_class_template(void);

extern a_template_symbol_supplement_ptr template_supplement_for_template(
						a_template_ptr	templ_ptr);

extern a_symbol_ptr get_member_function_template_symbol(a_symbol_ptr rout_sym);

extern a_symbol_ptr make_unnamed_tag_symbol(a_symbol_kind      sym_kind,
                                            a_source_position  *pos);

extern a_boolean is_unnamed_tag_symbol(a_symbol_ptr  sym);

extern a_symbol_ptr make_unnamed_namespace_symbol(a_source_position  *pos);

extern
a_symbol_ptr make_unnamed_template_param_symbol(a_symbol_kind		kind,
						a_source_position	*pos);

extern a_symbol_ptr unnamed_field_symbol(void);

extern a_symbol_ptr make_anonymous_parent_object_symbol(
                                                a_symbol_kind      kind,
                                                a_source_position  *pos,
                                                a_scope_number     decl_scope);

extern a_symbol_ptr full_enter_symbol(char          *identifier,
				      sizeof_t      identifier_length,
				      a_symbol_kind sym_kind,
				      a_scope_depth scope_depth);

extern void enter_keyword(a_token_kind token,
                          char         *keyword);

extern void make_symbol_for_predeclared_type(a_type_ptr  predeclared_type,
                                             char        *name);

extern void enter_injected_class_name_symbol(a_symbol_ptr  tag_sym);

extern a_symbol_ptr enter_typedef_symbol(a_type_ptr       type_ptr,
                                         a_symbol_locator *locator,
                                         a_scope_depth    scope_level,
                                         a_boolean        suppress_error);

EXTERN a_symbol_ptr
		symbol_for_namespace_std;
			/* Symbol for namespace "std", which is predeclared
			   by the front end (but not entered into the symbol
			   table until a user declaration is encountered).
			   C++ only. */

extern void make_symbol_for_namespace_std(void);

extern void enter_symbol_for_namespace_std(a_symbol_locator  *locator);

EXTERN a_type_ptr
		builtin_va_list_type;
			/* When the <stdarg.h> header is handled as a builtin,
			   this points to the va_list type once it has been
			   defined.  NULL until then. */

EXTERN a_symbol_ptr
		symbols_with_no_scope;
			/* A list of symbols that were entered into the
			   symbol table, but are not associated with any
			   scope (and so, are not on a scope list).  This
			   includes things such as predefined macros and
			   keywords.  The next_in_scope field is used to
			   link these symbols together. */

EXTERN a_boolean
		file_scope_symbols_are_on_inactive_list;
			/* TRUE once the file scope has been popped for the
			   first time, any any file scope symbols have been
			   moved to the inactive list. */

void declare_builtin_va_list_type(void);

extern void set_symbol_kind(a_symbol_ptr  sym_ptr,
			    a_symbol_kind sym_kind);

extern void unlink_symbol_from_symbol_table(a_symbol_ptr sym_ptr);

extern a_symbol_ptr alloc_symbol(a_symbol_kind       kind,
                                 a_symbol_header_ptr hdr_ptr,
                                 a_source_position   *position);

extern void remove_symbol(a_symbol_ptr sym_ptr);

extern void remove_anonymous_union_member_from_inactive_symbols_list
                                                       (a_symbol_ptr sym_ptr);

extern void add_symbol_to_inactive_list(a_symbol_ptr sym_ptr);

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

extern a_symbol_ptr find_default_operator_new_sym(a_symbol_ptr sym,
                                                  a_boolean    *ambiguous);

extern a_boolean is_default_operator_delete(a_routine_ptr routine);

extern a_symbol_ptr find_default_operator_delete_sym(a_symbol_ptr sym,
                                                     a_boolean    *ambiguous);

extern a_symbol_ptr find_corresponding_operator_delete_sym(
                                                  a_symbol_ptr op_new_sym,
                                                  a_type_ptr   class_type,
                                                  a_boolean    template_okay,
                                                  a_boolean    *ambiguous,
                                                  a_symbol_ptr *overload_sym);

extern a_symbol_ptr make_predeclared_function_symbol(
                                              a_symbol_locator  *locator,
                                              a_type_ptr        return_type,
                                              a_type_ptr        param1_type,
                                              a_type_ptr        param2_type,
                                              a_type_ptr        param3_type,
					      a_type_ptr        param4_type);

extern void make_global_operator_new_or_delete_symbol(an_opname_kind  opname);

#if MICROSOFT_EXTENSIONS_ALLOWED

extern void make_predeclared_alloca_symbol(void);

EXTERN a_symbol_ptr
		predeclared_size_t_symbol;
			/* Symbol for predeclared "size_t", in microsoft
			   mode.*/

extern void make_predeclared_size_t_symbol(void);

extern void make_predeclared_bool_symbol(void);

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

extern a_symbol_ptr find_default_constructor(a_type_ptr  class_type,
                                             a_boolean   *ambiguous);

extern a_routine_ptr select_default_constructor
					(a_type_ptr        class_type,
                                         a_source_position *err_pos,
					 a_type_ptr	   object_class_type,
                                         a_boolean         evaluated);

extern a_routine_ptr select_destructor(a_type_ptr       class_type,
				       a_type_ptr       object_class_type,
                                       a_source_position *position,
                                       a_boolean        honor_virtual,
                                       a_boolean        evaluated);

extern a_routine_ptr select_copy_constructor(
                                  a_type_ptr            class_type,
                                  a_type_qualifier_set  required_qualifiers,
                                  a_source_position     *err_pos,
                                  a_type_ptr            object_class_type,
                                  a_boolean             *class_bitwise_copy,
                                  a_boolean             evaluated);

extern a_symbol_ptr find_copy_assignment_operator(
                                    a_type_ptr            class_type,
                                    a_type_qualifier_set  required_qualifiers,
                                    a_boolean             *pass_by_value,
                                    a_boolean             *ambiguous);

extern a_routine_ptr select_copy_assignment_operator(
                                    a_type_ptr            class_type,
                                    a_type_qualifier_set  required_qualifiers,
                                    a_source_position     *err_pos,
                                    a_boolean             *pass_by_value);

extern char *il_entry_for_symbol_null_okay(a_symbol_ptr      sym,
                                           an_il_entry_kind  *kind);

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

extern void f_check_ambiguity_and_verify_access
				(a_symbol_locator	*loc,
				 a_boolean		is_templ_context);

extern void perform_deferred_access_checks(void);

extern void perform_deferred_access_checks_for_function(a_routine_ptr rp);

extern void f_discard_deferred_access_checks(void);

extern void discard_declarator_access_errors(void);

extern void overload_check_ambiguity_and_verify_access(
                                           a_symbol_locator *locator,
                                           a_symbol_ptr     overloaded_symbol);

/*
Check to see if a symbol found is ambiguous or inaccessible.  There
are two kinds of ambiguity: ambiguity caused by inheritance and
ambiguity caused by using directives.  Inheritance ambiguity
checking precedes access control (ARM, 10.1.1).  Call a subroutine
to do further checking if the ambiguous flag is set, or for class
members in C++ (so that access checking can be done).  This macro
does nothing when called in C mode.
*/
#define check_ambiguity_and_verify_access(locator)                    \
{ if (C_dialect == C_dialect_cplusplus &&                             \
      (locator)->specific_symbol != NULL &&                           \
      ((locator)->specific_symbol->is_class_member ||                 \
       (locator)->specific_symbol->ambiguous)) {                      \
    f_check_ambiguity_and_verify_access(locator,		      \
                                        /*is_template_context=*/FALSE); \
  }  /* if */                                                         \
}  /* check_ambiguity_and_verify_access */


/*
Similar to check_ambiguity_and_verify_access, except that the templ_context
flag is passed to the routine called.
*/
#define check_ambiguity_and_access_with_template_flag(locator, templ_context) \
{ if (C_dialect == C_dialect_cplusplus &&                             \
      (locator)->specific_symbol != NULL &&                           \
      ((locator)->specific_symbol->is_class_member ||                 \
       (locator)->specific_symbol->ambiguous)) {                      \
    f_check_ambiguity_and_verify_access(locator, templ_context);      \
  }  /* if */                                                         \
}  /* check_ambiguity_and_access_with_template_flag */


/*
Check to see if a symbol found is ambiguous.  If so, call a routine to
report the error.  Note that although the routine called also does
access checking, the access checks will not be done because the
the access checks are suppressed when an ambiguity error is detected.
The locator is set to an error locator by f_check_ambiguity_and_verify_access.
*/
#define check_for_ambiguity(locator)					\
{ if ((locator)->specific_symbol != NULL &&				\
      (locator)->specific_symbol->ambiguous) {				\
    f_check_ambiguity_and_verify_access(locator, /*is_template_id=*/FALSE); \
  }  /* if */                                                         	\
}  /* check_for_ambiguity */

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
  } else if ((symbol)->kind == (a_symbol_kind)sk_namespace_projection) {   \
    (symbol) = (symbol)->variant.namespace_projection.fundamental_symbol;\
  }  /* if */                                                         \
}  /* reduce_projection_symbol_to_fundamental_symbol */

/*
Return the fundamental symbol for a given symbol.
*/
#define fundamental_symbol_of(symbol)                                 \
  (									   \
  /* if */ ((symbol)->kind == (a_symbol_kind)sk_projection) /* { */ ?      \
    (symbol)->variant.projection.extra_info->fundamental_symbol            \
  /* } else { */  : 							   \
    /* if */ ((symbol)->kind ==					   \
                          (a_symbol_kind)sk_namespace_projection) /* { */ ? \
      (symbol)->variant.namespace_projection.fundamental_symbol		   \
    /* } else { */ :  							   \
      (symbol)								   \
    /* } */								   \
  /* } */								   \
  )


/*
Given a namespace projection symbol, return the fundamental symbol.
*/
#define namespace_projection_fundamental_symbol(sym)			\
  ((sym)->variant.namespace_projection.fundamental_symbol)

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
#if MICROSOFT_EXTENSIONS_ALLOWED
/* The Microsoft version of this macro is different in that it considers
   a base class accessible if we have member access to the base class.
   This is probably based on the wording in 11.2 that says "A base class
   is said to be accessible if an invented public member of the class
   is accessible." */
#define is_accessible_direct_base_class_derivation(bcp, bcdp, viewpoint_class)\
  ((bcdp)->access == (an_access_specifier)as_public ||                \
   have_member_access_privilege(viewpoint_class) ||                   \
   ((bcdp)->access == (an_access_specifier)as_protected &&            \
    have_protected_member_access_privilege(viewpoint_class)) ||       \
   (microsoft_mode && have_member_access_privilege(bcp->type)))
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
#define is_accessible_direct_base_class_derivation(bcp, bcdp, viewpoint_class)\
  ((bcdp)->access == (an_access_specifier)as_public ||                \
   have_member_access_privilege(viewpoint_class) ||                   \
   ((bcdp)->access == (an_access_specifier)as_protected &&            \
    have_protected_member_access_privilege(viewpoint_class)))
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#define is_virtual_but_not_simple_direct_base_class(bcp)              \
  ((bcp)->is_virtual && (!(bcp)->direct || (bcp)->derivation->next != NULL))
#define is_accessible_imm_base_class(bcp, viewpoint_class)            \
  (is_virtual_but_not_simple_direct_base_class(bcp) ?                 \
    is_accessible_virtual_base_class(bcp, viewpoint_class) :          \
    is_accessible_direct_base_class_derivation(bcp, bcp->derivation,  \
                                               viewpoint_class))

extern a_boolean is_accessible_base_class(a_base_class_ptr bcp);

extern a_boolean is_accessible_virtual_base_class(
                                             a_base_class_ptr bcp,
                                             a_type_ptr       viewpoint_class);

extern a_symbol_ptr find_progenitor_symbol(
                      a_type_ptr               class_ptr,
                      a_symbol_locator         *locator,
                      an_id_lookup_options_set options,
		      a_boolean		       look_in_dependent_bases,
                      a_derivation_step_ptr    *path,
                      an_access_specifier      *access,
                      a_boolean                *ambiguous,
                      a_boolean                *any_using_decl,
                      a_boolean                *unambiguous_injected_template);

extern void set_source_corresp(a_source_correspondence *sc,
                               a_symbol_ptr            sp);

extern
void set_source_corresp_with_scope_depth(a_source_correspondence *sc,
                                         a_symbol_ptr            sp,
			                 a_scope_depth		depth);

extern void set_class_membership(a_symbol_ptr             sym,
                                 a_source_correspondence  *scp,
                                 a_type_ptr               class_type);

extern void set_class_membership_for_template(a_symbol_ptr	sym,
					      a_template_ptr	templ,
					      a_type_ptr	class_type);

extern void set_namespace_membership(a_symbol_ptr             sym,
                                     a_source_correspondence  *scp,
                                     a_namespace_ptr          nsp);

extern void set_membership_in_source_corresp(a_source_correspondence  *scp,
                                             a_symbol_ptr             sym);

/* Allocation */
extern an_extern_type_fixup_ptr alloc_etype_fixup(void);
extern
a_substituted_type_list_entry_ptr alloc_substituted_type_list_entry(void);
extern void free_list_of_substituted_type_list_entries(
				a_substituted_type_list_entry_ptr stlep);
extern a_symbol_list_entry_ptr alloc_symbol_list_entry(void);
extern void free_list_of_symbol_list_entries(a_symbol_list_entry_ptr slep);
extern a_type_list_entry_ptr alloc_type_list_entry(void);
extern void free_list_of_type_list_entries(a_type_list_entry_ptr slep);
extern a_namespace_list_entry_ptr alloc_namespace_list_entry(void);
extern
void free_list_of_namespace_list_entries(a_namespace_list_entry_ptr nlep);

extern a_template_param_ptr alloc_template_param(a_symbol_ptr sym);

extern a_template_instance_ptr alloc_template_instance(void);
extern a_master_instance_ptr alloc_master_instance(void);
extern void free_param_id_list(a_param_id_ptr *pidlist);
extern void clear_func_info(a_func_info_block *func_info);

#define done_with_func_info(func_info)                                 \
  free_param_id_list(&(func_info.param_id_list))

extern void clear_decl_modifiers_block(a_decl_modifiers_block *decl_modifiers);

extern void add_to_param_id_list(a_symbol_locator            *locator,
                                 a_type_ptr                  type_ptr,
                                 a_source_position           *type_pos,
                                 a_storage_class             storage_class,
                                 struct an_attribute         *attributes,
                                 a_func_info_block_ptr       func_info,
                                 a_source_sequence_entry_ptr param_ssep,
                                 a_param_id_ptr              *last_param_id);
extern a_param_id_ptr param_id_on_list(a_symbol_locator *locator,
                                       a_param_id_ptr    param_id_list);

extern void add_to_dependent_type_fixup_list(
                                      a_type_ptr                   type_ptr,
                                      a_dependent_type_fixup_kind  fixup_kind,
                                      char                         *entity_ptr,
                                      a_byte_il_entry_kind         entity_kind,
                                      a_source_position            *pos);

extern void defer_exception_spec_error(a_func_info_block  *func_info,
                                       an_error_code      error_code,
                                       a_source_position  *pos);

extern void report_exception_spec_errors(a_func_info_block  *func_info);

extern void check_dependent_type_fixup_list(a_symbol_ptr  sym);

extern a_namespace_ptr parent_namespace_for_symbol(a_symbol_ptr sym);

extern a_boolean is_local_symbol(a_symbol_ptr sym);

extern a_boolean is_block_extern_symbol(a_symbol_ptr sym);

extern void determine_operator_lookup_namespaces(a_type_ptr	class_type);

extern a_symbol_ptr find_label_symbol(a_symbol_header_ptr	sym_hdr,
				      a_scope_number		scope_number);

extern a_symbol_ptr find_macro_symbol(a_symbol_header_ptr	sym_hdr);

extern a_symbol_ptr find_macro_symbol_by_name(char             *identifier,
					      sizeof_t         length,
					      a_symbol_locator	*locator);

extern a_symbol_header_ptr find_symbol_header(char             *identifier,
					      sizeof_t         length,
					      a_symbol_locator	*locator);

/*
Return the master instance pointer of a template instance.
*/
#define master_instance_of(tip)						\
  ((check_assertion((tip)->master_instance != NULL), (tip)->master_instance))

/* Return TRUE if a symbol is a class symbol.   A class symbol is
   one defined as a class, struct, or union, or a typedef of one of
   those.  This macro should only be used in C++ mode. */
#define is_class_symbol(sym)                                          \
  ((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||            \
   (sym)->kind == (a_symbol_kind)sk_union_tag ||                      \
   ((sym)->kind == (a_symbol_kind)sk_type &&                          \
                   is_class_struct_union_type((sym)->variant.type.ptr)))

/* Return TRUE if a symbol is of kind sk_class_or_struct_tag or
   sk_union_tag. */
#define is_class_struct_union_symbol(sym)                             \
  ((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||            \
   (sym)->kind == (a_symbol_kind)sk_union_tag)

/* Return TRUE if a symbol is a namespace symbol. */
#define is_namespace_symbol(sym)                                          \
  ((sym)->kind == (a_symbol_kind)sk_namespace)

/* Return TRUE if a symbol is an enum symbol or a typedef to an enum. */
#define is_enum_symbol(sym)					      \
  ((sym)->kind == (a_symbol_kind)sk_enum_tag ||			      \
   ((sym)->kind == (a_symbol_kind)sk_type &&			      \
    is_enum_type((sym)->variant.type.ptr)))

extern a_boolean is_member_enum_symbol(a_symbol_ptr sym);

extern a_boolean overload_set_contains_template(a_symbol_ptr sym);

/* Return TRUE if a symbol is an sk_type symbol that points to a
   tk_template_param type, or a typeref to such a type.
*/
#define is_template_param_type_symbol(sym)				\
  ((sym)->kind == (a_symbol_kind)sk_type &&				\
   skip_typerefs((sym)->variant.type.ptr)->kind ==			\
                                              (a_type_kind)tk_template_param)

/* Return TRUE if a symbol is one that may be used as part of the
   qualifier in a qualified name.  This includes class symbols,
   typedefs to class symbols, type template parameters, class template
   symbols, and namespace symbols.  In Microsoft bugs mode, enum tags are also
   considered to be eligible for use in the qualifier portion of a name.
   This macro should only be used in C++ mode. */
#define symbol_may_precede_qualifier(sym)                           \
  ((sym)->kind == (a_symbol_kind)sk_class_template ||		      \
   is_class_symbol(sym) ||                                            \
   (sym)->kind == (a_symbol_kind)sk_namespace ||		      \
   ((sym)->kind == (a_symbol_kind)sk_type &&                          \
    is_template_param_type((sym)->variant.type.ptr)) ||		      \
   (microsoft_bugs && is_member_enum_symbol(sym)))

/* Return TRUE if a symbol is a class symbol, a class template symbol,
   a template parameter symbol, or a typedef to a template parameter.
   Note that a template parameter symbol is considered even if the type
   referred to is not a class type. */
#define is_class_or_class_proxy_symbol(sym)                               \
  (is_class_symbol(sym) ||					      \
   (sym)->kind == (a_symbol_kind)sk_class_template ||		      \
   ((sym)->kind == (a_symbol_kind)sk_type &&			      \
    (is_template_param_type((sym)->variant.type.ptr) ||		      \
    (sym)->is_template_param)))

/* Return TRUE if the symbol is a template class symbol. */
#define is_template_class_symbol(sym)				      \
  (((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||	      \
    (sym)->kind == (a_symbol_kind)sk_union_tag) &&		      \
   ((sym)->variant.class_struct_union.extra_info->class_template != NULL))

/* Return TRUE if the symbol is a template class symbol for a class
   generated from the template (i.e., not a specific definition). */
#define is_template_class_and_not_specific_def_symbol(sym)		\
  (is_template_class_symbol((sym)) &&					\
   !(sym)->variant.class_struct_union.type->                            \
                        variant.class_struct_union.is_specialized)

/* Return TRUE if the symbol is a class template symbol. */
#define is_class_template_symbol(sym)					\
  ((sym)->kind == (a_symbol_kind)sk_class_template)


/* Return TRUE if the symbol is an sk_type symbol that represents an
   injected class name in a template class.  In a template class, the
   template name can be used either to refer to the template or to refer
   to the current instance. */
#define is_injected_template_symbol(sym)				\
  ((sym)->kind == (a_symbol_kind)sk_type &&				\
   (sym)->variant.type.is_injected_class_name &&			\
   (sym)->variant.type.ptr->variant.class_struct_union.is_template_class && \
   (sym)->variant.type.ptr->						\
	    variant.class_struct_union.extra_info->template_arg_list != NULL)

/* Return TRUE if the symbol is an sk_type symbol that represents an
   injected class name. */
#define is_injected_class_symbol(sym)					\
  ((sym)->kind == (a_symbol_kind)sk_type &&				\
   (sym)->variant.type.is_injected_class_name)

/* Return TRUE if the symbol is a class template symbol or an sk_type
   symbol that represents an injected class name in a template class. */
#define is_class_template_or_injected_template_symbol(sym)		\
  (is_class_template_symbol(sym) ||					\
   is_injected_template_symbol(sym))

/* Return TRUE if the symbol is a class symbol for either a normal
   (non-template) or a "real" instantiation of a template class.  */
#define is_real_class_symbol(sym)				      \
  (((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||           \
    (sym)->kind == (a_symbol_kind)sk_union_tag) &&		      \
    !((sym)->variant.class_struct_union.type->			      \
                   variant.class_struct_union.is_nonreal_class))

/* Return TRUE if the symbol is a template class symbol for a class template
   instance or a class nested within a class template. */
#define is_template_instance_class_symbol(sym)				\
  (is_real_class_symbol(sym) &&						\
   (sym)->variant.class_struct_union.type->				\
                   variant.class_struct_union.is_template_class)

/* Return TRUE if the symbol is a template class symbol for a real or
   nonreal class template instance or a class nested within a class
   template. */
#define is_any_template_instance_class_symbol(sym)		\
  (((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||           \
    (sym)->kind == (a_symbol_kind)sk_union_tag) &&		      \
   (sym)->variant.class_struct_union.type->			      \
                   variant.class_struct_union.is_template_class)

/* Return TRUE if the symbol is a template class symbol for a nonreal
   class template instance or a class nested within a nonreal class
   template.  This will include prototype instantiations.  Note that this
   is not TRUE for other nonreal types such as proxy classes for template
   parameters. */
#define is_nonreal_instance_class_symbol(sym)				\
  (((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||           \
    (sym)->kind == (a_symbol_kind)sk_union_tag) &&		      \
   (sym)->variant.class_struct_union.type->			      \
                   variant.class_struct_union.is_template_class &&    \
   (sym)->variant.class_struct_union.type->			      \
                   variant.class_struct_union.is_nonreal_class)

extern a_boolean is_proxy_member_symbol(a_symbol_ptr  sym);

/* Return TRUE if the symbol is a specific definition of a class template
   instance or a class nested within a class template. */
#define is_template_instance_specific_def_symbol(sym)			\
  (is_real_class_symbol(sym) &&						\
   (sym)->variant.class_struct_union.type->                             \
                        variant.class_struct_union.is_specialized)

/* Return TRUE if a symbol is a tag symbol.   A tag symbol is
   one defined as a class, struct, union, or enum (but not as a typedef
   of one of those).  An injected class name, although represented as
   an sk_type symbol, is considered a tag for lookup purposes. */
#define is_tag_symbol(sym)                                            \
  ((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||            \
   (sym)->kind == (a_symbol_kind)sk_union_tag ||                      \
   (sym)->kind == (a_symbol_kind)sk_enum_tag ||			      \
   is_injected_class_symbol(sym))

/* Return TRUE if a symbol is a tag symbol, a class template symbol,
   or a type template parameter. */
#define is_tag_or_tag_proxy_symbol(sym)                               \
  (is_tag_symbol(sym) ||					      \
   (sym)->kind == (a_symbol_kind)sk_class_template ||		      \
   ((sym)->kind == (a_symbol_kind)sk_type && (sym)->is_template_param))

/* Return TRUE if a symbol is a type symbol.   A type symbol is
   one defined as a typedef, or, in C++, as a class, struct, union,
   or enum. */
#define is_type_symbol(sym)                                           \
  ((sym)->kind == (a_symbol_kind)sk_type ||                           \
   (C_dialect == C_dialect_cplusplus && is_tag_symbol(sym)))

/* Return TRUE if a symbol is a class or function template symbol. */
#define is_template_symbol(sym)                                           \
  ((sym)->kind == (a_symbol_kind)sk_class_template ||                  \
   (sym)->kind == (a_symbol_kind)sk_function_template)

/* Return TRUE if a symbol is a class or function template symbol, or
   an injected template symbol. */
#define is_template_or_injected_template_symbol(sym)			\
  ((sym)->kind == (a_symbol_kind)sk_class_template ||                  \
   (sym)->kind == (a_symbol_kind)sk_function_template ||		\
   is_injected_template_symbol(sym))

/* Return TRUE if a symbol is a class or function template symbol or an
   overload set containing a function template symbol */
#define symbol_is_or_contains_template(sym)				\
  (is_class_template_or_injected_template_symbol(sym) ||		\
   (sym)->kind == (a_symbol_kind)sk_function_template ||		\
   ((sym)->kind == (a_symbol_kind)sk_overloaded_function &&		\
    overload_set_contains_template(sym)))

/* Return TRUE if a symbol is a function symbol. */
#define is_function_symbol(sym)                                       \
  ((sym)->kind == (a_symbol_kind)sk_routine ||                        \
   (sym)->kind == (a_symbol_kind)sk_member_function ||                \
   (sym)->kind == (a_symbol_kind)sk_overloaded_function)

/* Return TRUE if a symbol is a function or function template symbol. */
#define is_function_or_template_symbol(sym)				\
  (is_function_symbol((sym)) ||						\
   (sym)->kind == (a_symbol_kind)sk_function_template)

/* Return TRUE if a symbol is a member function symbol. */
#define is_member_function_symbol(sym)                                \
  ((sym)->is_class_member &&                                          \
   ((sym)->kind == (a_symbol_kind)sk_member_function ||               \
    (sym)->kind == (a_symbol_kind)sk_overloaded_function ||           \
    (sym)->kind == (a_symbol_kind)sk_function_template))

extern
a_special_function_kind special_function_kind_for_symbol(a_symbol_ptr	sym);

/*
If sym is a routine symbol of some sort, return TRUE if the special function
kind recorded in its routine entry is "kind" and FALSE if it is not.  If sym
is not a routine symbol, return FALSE.
*/
#define is_special_function_symbol(sym, kind)				\
  (special_function_kind_for_symbol(sym) == (a_special_function_kind)(kind))

extern a_type_ptr underlying_function_type(a_symbol_ptr  sym);

/* Return TRUE if a symbol is a constructor symbol. */
#define is_constructor_symbol(sym)                                    \
  is_special_function_symbol(sym,                                     \
                             (a_special_function_kind)sfk_constructor)

/* Return TRUE if a symbol is a destructor symbol. */
#define is_destructor_symbol(sym)                                     \
  is_special_function_symbol(sym,                                     \
                             (a_special_function_kind)sfk_destructor)

/* Return TRUE if a symbol is a conversion operator symbol. */
#define is_conversion_function_symbol(sym)                            \
  is_special_function_symbol(sym,                                     \
                             (a_special_function_kind)sfk_conversion)

/* Return TRUE if a symbol is a projection symbol created for a class
   member using declaration. */
#define is_class_member_using_decl_symbol(sym)                        \
  ((sym)->kind == (a_symbol_kind)sk_projection &&                     \
   (sym)->variant.projection.is_using_decl)

/* Return TRUE if a symbol is a class template symbol that represents
   a nonreal template (such as X in T::X<int>). */
#define is_nonreal_template_symbol(sym)					\
  ((sym)->kind == (a_symbol_kind)sk_class_template &&			\
   (sym)->variant.template_info->is_nonreal_member)

/* Return TRUE if a symbol is a class template symbol that represents
   a template template parameter. */
#define is_template_template_param_symbol(sym)				\
  ((sym)->kind == (a_symbol_kind)sk_class_template &&			\
   (sym)->variant.template_info->					\
                          variant.class_template.template_template_param)

/* Return TRUE if a symbol represents a type template param. */
#define is_type_template_param_symbol(sym)                              \
  ((sym)->kind == (a_symbol_kind)sk_type &&                             \
   (sym)->variant.type.ptr->kind == (a_type_kind)tk_template_param)

/* Return TRUE if a symbol represents a non-type template param. */
#define is_nontype_template_param_symbol(sym)                           \
  ((sym)->kind == (a_symbol_kind)sk_constant &&                         \
   (sym)->variant.constant != NULL &&					\
   (sym)->variant.constant->kind == (a_constant_repr_kind)ck_template_param)

/*
Extract the type from a type symbol (one for which is_type_symbol is TRUE).
*/
#define type_symbol_type(sym)                                         \
  ((sym)->kind == (a_symbol_kind)sk_type ?                            \
    (sym)->variant.type.ptr :                                             \
    (((sym)->kind == (a_symbol_kind)sk_enum_tag) ?                    \
      (sym)->variant.enumeration.type :                               \
      (sym)->variant.class_struct_union.type))

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

/*
Given a namespace pointer, return a pointer to the namespace symbol
supplement.
*/
#define symbol_supplement_for_namespace(nsp)	                     \
  (((a_symbol_ptr)(skip_namespace_aliases(nsp))->source_corresp.assoc_info)-> \
                                          variant.namespace_info.extra_info)


/* Return a pointer to the current routine entry (only usable when within
   a routine definition). */
#define current_routine_entry() (innermost_function_scope->variant.routine.ptr)

/* Return a pointer to the template symbol supplement for a given
   symbol.  Return NULL for symbols of the wrong kind. */
#define template_supplement_for_symbol(sym)				\
  (/* if */ ((sym)->kind == (a_symbol_kind)sk_class_template ||		\
             (sym)->kind == (a_symbol_kind)sk_function_template) ? /* { */ \
    (sym)->variant.template_info :					\
  /* } else if */ (sym)->kind == (a_symbol_kind)sk_member_function ? /* { */ \
    (sym)->variant.routine.instance_ptr->template_info :		\
  /* } else if */ ((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||  \
                  (sym)->kind == (a_symbol_kind)sk_union_tag) ? /* { */ \
    (sym)->variant.class_struct_union.extra_info->template_info :       \
  /* } else if */ (sym)->kind ==					\
			 (a_symbol_kind)sk_static_data_member ? /* { */	\
    (sym)->variant.static_data_member.instance_ptr->template_info :	\
  /* } else { */							\
    NULL								\
  /* } */)

/* Return the template symbol for a given IL template entry. */
#define symbol_for_template(templ)					\
  ((a_symbol_ptr)((templ)->source_corresp.assoc_info))

/* If sym is a template template parameter, return the symbol for the template
   argument, otherwise return the original symbol. */
#define template_argument_if_template_template_param(sym)		\
  (((sym)->kind == (a_symbol_kind)sk_class_template &&			\
    (sym)->variant.template_info->					\
                     variant.class_template.template_template_param)	\
     ? (sym)->variant.template_info->variant.class_template.argument_template \
     : sym)

/* Return TRUE if the symbol represents the prototype instantiation of a
   class template. */
#define is_prototype_instantiation_symbol(sym)				\
  (is_class_struct_union_symbol(sym) &&					\
   (sym)->variant.class_struct_union.type->				\
                   variant.class_struct_union.is_prototype_instantiation)

/* If a symbol represents a subordinate template, return a pointer to the
   prototype template; otherwise return the symbol provided. */
#define prototype_template_of(sym)					\
  ((sym)->variant.template_info->prototype_template != NULL &&		\
   !(sym)->variant.template_info->is_specific_definition ?		\
      (sym)->variant.template_info->prototype_template : (sym))

/*
If "sym" is a template symbol (class or function) return the prototype template
symbol; otherwise return the original symbol.
*/
#define prototype_template_if_template_symbol(sym)			\
  (is_template_symbol(sym) ? prototype_template_of(sym) : (sym))

/*
If "sym" is a class template symbol return the primary template
symbol; otherwise return the original symbol.
*/
#define primary_template_if_template_symbol(sym)			\
  (is_class_template_symbol(sym) ? primary_template_of(sym) : (sym))

/* Return a pointer to the namespace associated with a namespace symbol.
   Remove any namespace aliases that may be present.  The symbol provided
   must be a namespace symbol. */
#define namespace_symbol_namespace(sym)					\
  (skip_namespace_aliases((sym)->variant.namespace_info.ptr))

/* Return a pointer to the next instance symbol in a list of class
   instantiations. */
#define next_instance_sym(sym)						\
  ((sym)->variant.class_struct_union.extra_info->next_in_instantiations_list)

/*
Given a symbol kind (associated with a template parameter) return the
template argument kind to be used.
*/
#define templ_arg_kind_for_symbol_kind(sym_kind)			\
  ((a_templ_arg_kind)((sym_kind) == (a_symbol_kind)sk_type ? tak_type :	\
   ((sym_kind) == (a_symbol_kind)sk_constant ? tak_nontype : tak_template)))

void form_optionally_qualified_symbol_name(
		a_symbol_ptr				sym,
		an_il_to_str_output_control_block_ptr	octl,
		a_boolean				suppress_qualifier);

extern void form_symbol_name(a_symbol_ptr                          sym,
                             an_il_to_str_output_control_block_ptr octl);


#if DEBUG
/* Show and return the amount of memory used by symbol table entries. */
extern unsigned long show_symbol_space_used(void);
/* Display a symbol table entry. */
extern void db_symbol(a_symbol_ptr	sym,
                      char		*string,
                      int		indentation);

/* Short-hand version of db_symbol. */
extern void db_sym(a_symbol_ptr  sym);

extern void db_symbol_name(a_symbol_ptr  sym);

extern char *db_symbol_trans_unit(a_symbol_ptr sym);

extern void db_symbol_name_trans_unit(a_symbol_ptr sym);

/*
Information used to gather performance statistics related to symbol
table processing that needs to be externally visible.
*/
EXTERN unsigned long
		num_fast_id_lookups,
		num_slow_id_lookups,
		num_active_using_directives_allocated,
		num_generated_entity_blocks_allocated;
#endif /* DEBUG */

extern a_symbol_ptr f_class_template_for_type(a_type_ptr	type);

/*
Interface to f_class_template_for_type that handles all of the
cases that are not template classes.
*/
#define class_template_for_type(type)					\
  ((is_immediate_class_type(type) &&					\
   (type)->variant.class_struct_union.is_template_class) ?		\
   f_class_template_for_type(type) : (a_symbol_ptr)NULL)

extern a_symbol_ptr class_template_for_injected_template_symbol(
							a_symbol_ptr sym);

extern a_scope_number take_next_scope_number(void);

extern a_boolean symbol_is_from_trans_unit(a_symbol_ptr			sym,
					   a_translation_unit_ptr	tup);

extern a_translation_unit_ptr trans_unit_for_symbol(a_symbol_ptr	sym);

extern void symbol_tbl_one_time_init(void);

extern void symbol_tbl_trans_unit_init(void);

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
