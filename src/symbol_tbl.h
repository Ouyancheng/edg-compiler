/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
/*

symbol_tbl.h - Declarations related to symbol table processing.

*/

/* Avoid including these declararations more than once. */
#ifndef SYMBOL_TBL_H
#define SYMBOL_TBL_H 1

/* Declare pointer types up front to minimize mutual recursion problems. */
typedef struct a_symbol        *a_symbol_ptr;
typedef struct a_symbol_header *a_symbol_header_ptr;
typedef struct a_macro_param   *a_macro_param_ptr;
typedef struct a_macro_def     *a_macro_def_ptr;

/* Some other things declared up front to avoid mutual recursion problems. */

/* Type of a scope nesting depth.  This is the depth within the scope_stack. */
typedef int	a_scope_depth;

#define NO_SCOPE_DEPTH (-1)
#define DEPTH_OF_FILE_SCOPE 0

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
#define IDL_DO_NOT_MAKE_PROJECTION_IF_NOT_TYPE_NAME 0x8
				/* If the symbol found is a projection from
				   a base class, do not actually create the
				   symbol to represent that projection unless
				   is a type name -- a typedef or tag symbol
				   (class, struct, union, or enum) -- but
				   return NULL instead. */
#define IDL_NO_OPTIONS 0	/* No special lookup options. */


typedef struct a_symbol_locator {
  /* Data structure used to store information about an identifier token.
     Can be used to look up the identifier or enter it into the symbol
     table. */
  /* If you change this structure, be sure to also change clear_locator,
     which follows immediately. */
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
			   that begins with a unary "::" (e.g., "::y"). */
  a_symbol_ptr	specific_symbol;
			/* If is_qualified_name is TRUE, this points to the
			   specific symbol for the qualified name.  Otherwise,
			   if this pointer is non-NULL, it is the result of
			   the most recent lookup of this identifier (e.g.,
			   by normal_id_lookup). */
} a_symbol_locator;

/*
Clear a symbol locator.
*/
#define clear_locator(locator, position)                              \
{ (locator)->symbol_header = NULL;                                    \
  (locator)->source_position = *position;                             \
  (locator)->is_qualified_name = FALSE;                               \
  (locator)->is_global_qualified_name = FALSE;                        \
  (locator)->specific_symbol = NULL;                                  \
}  /* clear_locator */

/* Return TRUE if two locators indicate the same symbol. */
/* Only used in checking whether or not a declaration in an old-style
   parameter list has the same name as one of the old-style parameters.
   For that application, specific_symbol should not be checked. */
#define are_locators_for_same_symbol(loc1, loc2)                      \
  ((loc1).symbol_header == (loc2).symbol_header)

/* Set a symbol locator to a dummy value indicating an error. */
#define set_to_error_locator(loc) clear_locator(&(loc), &error_position)

/* Test a locator to see if it is an error locator. */
#define is_error_locator(loc) ((loc).symbol_header == NULL)

/* Retrieve a pointer to the symbol list from a locator. */
#define symbol_list_from_locator(loc) ((loc).symbol_header->symbol)

/* Retrieve a pointer to the inactive symbol list from a locator. */
#define inactive_symbol_list_from_locator(loc)                        \
  ((loc).symbol_header->inactive_symbols)


#ifndef LEXICAL_H
#include "lexical.h"
#endif /* ifndef LEXICAL_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */
#ifndef TRANS_LIMS_H
#include "trans_lims.h"
#endif /* ifndef trans_lims.h */
#ifndef MEM_TABLES_H
#include "mem_tables.h"
#endif /* ifndef MEM_TABLES_H */
#ifndef TYPES_H
#include "types.h"
#endif /* ifndef TYPES_H */
#ifndef CLASS_DECL_H
#include "class_decl.h"
#endif /* ifndef CLASS_DECL_H */


/*
Kinds of symbols in the symbol table.
(If this is changed, the definition for name_space_for_symbol_kind in
fe_init.c should also be changed.)
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
  sk_last
};
/* Define as "a_byte" to explicitly control storage size. */
typedef a_byte a_symbol_kind;


#if DEBUG
/*
Table of names corresponding to symbol kinds, for debug purposes.
*/
EXTERN char	*db_sym_names[(int)sk_last + 1]
#if VAR_INITIALIZERS
= {
   "keyword", "macro", "constant", "type", "class-or-struct", "union",
   "enum", "variable", "field", "static-data-member", "member-function",
   "routine", "label", "undefined", "extern-variable", "extern-routine",
   "projection", "overloaded-function",
   "last" /* used to check that initialization is right. */
}
#endif /* VAR_INITIALIZERS */
;
#endif /* DEBUG */


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
  nsk_extern		/* External names of variables and routines, perhaps
			   truncated.  Used to check that all uses of
			   a given external name are equivalent. */
} a_name_space_kind;

EXTERN a_name_space_kind
		name_space_for_symbol_kind[(int)sk_last+1];
			/* For each symbol kind, this array maps the kind
			   to the associated name space.  See sym_tbl_init for
			   initialization. */

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
			/* Raw string for argument.  Followed by 3 bytes
			   containing the argument number (first argument is
			   numbered 1). */
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
{ unsigned long temp = 0;                                             \
  temp  = *(a_byte *)rtp++;                                           \
  temp |= *(a_byte *)rtp++ << CHAR_BIT;                               \
  temp |= *(a_byte *)rtp++ << CHAR_BIT*2;                             \
  num = temp;                                                         \
}  /* put_macro_repl_text_number */


/*
Put a multi-byte number (num) into a macro-definition string. rtp points
to the first byte of the number; it is advanced past the number on return.
*/
#define PN_BYTE_MASK ((1 << CHAR_BIT) - 1);
#define put_macro_repl_text_number(num, rtp)                          \
{ unsigned long temp = num;                                           \
  *(a_byte *)rtp++ =  temp                & PN_BYTE_MASK;             \
  *(a_byte *)rtp++ = (temp >> CHAR_BIT)   & PN_BYTE_MASK;             \
  *(a_byte *)rtp++ = (temp >> CHAR_BIT*2) & PN_BYTE_MASK;             \
}  /* get_macro_repl_text_number */


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
  a_symbol_ptr	default_constructor;
			/* Pointer to an sk_member_function symbol that
			   identifies the default constructor for this class
			   (i.e., the constructor that can be called with no
			   arguments); NULL if there is none. */
  a_symbol_ptr	destructor;
			/* Pointer to an sk_member_function symbol that
			   identifies the destructor for this class; NULL if
			   there is none. */
  unsigned int	any_nonpublic_members:1;
			/* TRUE if the class contains any members declared
			   private or protected. */
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
  unsigned int  has_copy_constructor:1;
			/* TRUE if a copy constructor has either been declared
			   or generated for the class. */
} a_class_symbol_supplement;


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
    /* When symbol kind == sk_extern_routine: */
    a_routine_ptr
		routine;
  } variant;
} an_extern_symbol_descr;


typedef struct a_projection_descr *a_projection_descr_ptr;
typedef struct a_projection_descr {
  /* Description of the projection of a base class member symbol into
     a derived class.  Pointed to by an sk_projection symbol. */
  a_symbol_ptr  fundamental_symbol;
			/* The fundamental base class member to which this
			   projection symbol refers.  It will be different
			   from the progenitor_symbol when the latter is a
			   projection symbol.  Referring to the previous
			   example, B::i is the progenitor symbol for C::i,
			   but A::i is the fundamental symbol.  If ambiguous
			   is TRUE, this symbol is one of several possible. */
  a_base_class_ptr
		fundamental_base_class;
			/* This field is a pointer to the base class entry for
			   the entity represented by fundamental_symbol.  It
			   will be a base class entry on the current class's
			   base classes list, and its derivation field
			   specifies the path between the current class object
			   and the member specified by fundamental_symbol. */
} a_projection_descr;


typedef struct a_symbol {
  /* A symbol as used by the front end. */
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
  union {
    /* When kind == sk_undefined, no variant fields. */
    /* When kind == sk_keyword: */
    a_token_kind
		keyword_token;
			/* For keywords, the token identifying the keyword. */
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
    /* When kind == sk_variable or sk_static_data_member: */
    a_variable_ptr
		variable;
			/* The variable. */
    /* When kind == sk_field: */
    a_field_ptr	field;
			/* The field. */
    /* When kind == sk_routine or sk_member_function: */
    a_routine_ptr
		routine;
			/* The routine. */
    /* When kind == sk_label: */
    a_label_ptr	label;
			/* The label. */
    /* When kind == sk_extern_variable or sk_extern_routine: */
    an_extern_symbol_descr_ptr
		extern_symbol_descr;
			/* Information on the external symbol. */
    /* When kind = sk_projection: */
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
			/* TRUE if progenitor_symbol's name is ambiguous in
			   the current scope, i.e., another symbol with the
			   same name is visible, and there is no reason to
			   prefer one over the other. */
      unsigned int
		access_adjustment_made:1;
			/* If TRUE an access declaration has been made for
			   the projection symbol, in which case it cannot
			   be overridden by a local symbol of the same name. */
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
      unsigned int
		mixed_access:1;
			/* TRUE if the access specifications on the functions
			   are not all the same; applies to
			   sk_member_function overloading only. */
      unsigned int
		any_virtual_functions:1;
			/* TRUE if any of the overloaded function names
			   represents a virtual function. */
    } overloaded_function;
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
} a_symbol_header;


#define SYMBOL_TABLE_SIZE 599
	  		/* The number of buckets in the symbol table.  This
			    number should be prime. */
EXTERN a_symbol_header_ptr
		symbol_table[SYMBOL_TABLE_SIZE];
			/* The actual symbol table.  Each bucket of the array
			   contains a pointer to a list of symbol headers whose
			   identifiers hash to that bucket. */

/*
Table of pointers to symbol headers for C++ operator name symbols, for
names like "operator+".  Indexed by token value.
*/
EXTERN a_symbol_header_ptr
		opname_symbol_table[(int)tok_last];

/*
Symbol information related to the current token:
*/
EXTERN a_symbol_locator
		locator_for_curr_id;
			/* If curr_token == tok_identifier, this is information
			   fully specifying the identifier. */

/*
Entries on a list of array types whose sizes must be fixed up.  These
are used for arrays of incomplete structs/unions (an extension).
*/
typedef struct an_array_type_fixup *an_array_type_fixup_ptr;
typedef struct an_array_type_fixup {
  an_array_type_fixup_ptr
		next;
			/* Next fixup on the list, or NULL if this is the
			   last. */
  a_type_ptr	array_type;
			/* Pointer to an array type whose element type
			   was an incomplete struct or union type when
			   declared. */
} an_array_type_fixup;


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
  an_array_type_fixup_ptr
		array_type_fixup_list;
			/* List of array types to be fixed up at the end of
			   the scope.  Used with arrays of incomplete
			   struct/union types (an extension). */
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
  a_delayed_scan_fixup_ptr
		delayed_scan_fixup_list,
		last_delayed_scan_fixup;
			/* Defined for sck_class_struct_union scopes only:
			   the head and tail of a list of entities used in
			   the token caching and delayed scanning scheme
			   required for C++ member functions (routine bodies
			   and default arguments). */
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
  a_label_ptr	last_label;
			/* End of list of local labels of this scope, NULL
			   if none. */
  a_routine_ptr	last_routine;
			/* End of list of local routines of this scope, NULL
			   if none.  Includes both routines with definitions
			   and those that are just declarations of interfaces
			   to external routines. */
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
EXTERN a_scope_number
		next_scope_number;
			/* Next scope number to be assigned.  These are
			   unique identifiers for each scope, not just
			   the scope nesting depth.  Also used for the
			   pseudo-scopes associated with the members of
			   structs and unions in C (not C++). */


/*
Enumeration indicating a kind of reference to a symbol, used in
generating cross-reference information.
*/
typedef enum /*a_symbol_reference_kind*/ {
  srk_declaration,	/* Declaration or definition. */
  srk_modification,	/* Reference that changes the value of a variable. */
  srk_address_taken,	/* Address of a variable or function taken. */
  srk_reference		/* All other kinds of references. */
} a_symbol_reference_kind;

extern char *name_of_symbol(a_symbol_ptr  sym);

extern a_symbol_ptr find_symbol(char             *identifier,
			        sizeof_t         identifier_length,
				a_symbol_locator *location);

extern void make_locator_for_symbol(a_symbol_ptr     sym_ptr,
                                    a_symbol_locator *location);

extern void make_specific_symbol_error_locator(a_symbol_locator *locator);

extern a_symbol_ptr enter_symbol(a_symbol_kind    sym_kind,
				 a_symbol_locator *location,
                                 a_scope_depth    scope_depth,
                                 a_boolean        suppress_error);

extern void reenter_symbol(a_symbol_ptr     symbol_to_reenter,
                           a_scope_depth    scope_depth,
                           a_boolean        suppress_error);

extern a_symbol_ptr enter_overloaded_symbol(a_symbol_kind    sym_kind,
                                            a_symbol_locator *location,
                                            a_symbol_ptr     old_sym_ptr,
                                            a_symbol_ptr     *overload_sym);

extern a_symbol_ptr make_unnamed_class_symbol(a_symbol_kind      sym_kind,
                                              a_type_ptr         class_type,
                                              a_source_position  *pos);

extern a_boolean is_unnamed_class_symbol(a_symbol_ptr  sym);

extern a_symbol_ptr full_enter_symbol(char          *identifier,
				      sizeof_t      identifier_length,
				      a_symbol_kind sym_kind,
				      a_scope_depth scope_depth);

extern void set_symbol_kind(a_symbol_ptr  sym_ptr,
			    a_symbol_kind sym_kind);

extern void remove_symbol(a_symbol_ptr sym_ptr);

extern a_symbol_ptr find_external_symbol(a_symbol_locator     *location,
                                         a_name_linkage_kind  linkage,
                                         a_type_ptr           type,
                                         a_symbol_locator     *ext_location);

extern void tildize_locator(a_symbol_locator *locator);

extern void change_class_locator_into_constructor_locator(
                                                    a_symbol_locator *locator);

extern void make_opname_locator(a_token_kind      token,
                                a_symbol_locator  *locator,
                                a_source_position *pos);

extern an_access_specifier access_for_symbol(a_symbol_ptr sym_ptr);

extern a_boolean have_member_access_privilege(a_type_ptr class_type);

extern a_boolean have_access_to_symbol(a_symbol_ptr symbol);

extern void member_check_ambiguity_and_verify_access(a_symbol_locator *loc);

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


extern a_symbol_ptr normal_id_lookup(a_symbol_locator         *locator,
                                     an_id_lookup_options_set options);

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
is an accessible base class of derived_class.  bcp indicates a
direct base class of derived_class.  A base class is accessible if it's
public or if we have member access to the derived class.
*/
#define is_accessible_base_class(bcp, derived_class)                  \
  ((bcp)->access == (an_access_specifier)as_public ||                 \
   have_member_access_privilege(derived_class))


extern a_symbol_ptr class_qualified_id_lookup(
                                         a_symbol_locator         *locator,
                                         a_type_ptr               class_type,
                                         an_id_lookup_options_set options);

extern a_symbol_ptr file_scope_id_lookup(a_symbol_locator         *locator,
                                         an_id_lookup_options_set options);
/* Begin a name scope. */
extern a_scope_ptr push_scope(a_scope_kind   kind,
			      a_scope_number scope_number_for_function,
                              a_type_ptr     assoc_type,
                              a_routine_ptr  assoc_routine);
/* End a name scope. */
extern void pop_scope(void);
extern void push_class_reactivation_scope(a_type_ptr class_type);
extern void pop_class_reactivation_scope(void);
/* Record use information (for cross-reference, etc.). */
extern void mark_declared(a_symbol_ptr      sym_ptr,
                          a_source_position *source_position,
                          a_boolean         save_as_decl_position);
extern void mark_referenced(a_symbol_ptr      sym_ptr,
                            a_source_position *source_position);
extern void mark_symbol_referenced(a_symbol_reference_kind kind,
                                   a_symbol_ptr            sym_ptr,
                                   a_source_position       *source_position);
extern void reference_to_symbol(a_symbol_reference_kind kind,
                                a_symbol_ptr            sym_ptr,
                                a_source_position       *source_position);
extern void set_source_corresp(a_source_correspondence *sc,
                               a_symbol_ptr            sp);

extern an_extern_type_fixup_ptr alloc_etype_fixup(void);

/* Examine the list of symbols with a given name, looking for an
   instance with a particular kind. */
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

/* Return TRUE if a symbol is a tag symbol.   A tag symbol is
   one defined as a class, struct, union, or enum (but not as a typedef
   of one of those). */
#define is_tag_symbol(sym)                                            \
  ((sym)->kind == (a_symbol_kind)sk_class_or_struct_tag ||            \
   (sym)->kind == (a_symbol_kind)sk_union_tag ||                      \
   (sym)->kind == (a_symbol_kind)sk_enum_tag)

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

/* Return TRUE if an sk_routine or sk_member_function symbol "sym" is marked
   as being of special function kind "kind". */
#define is_special_kind_function_symbol(sym, kind)                    \
  ((sym)->variant.routine->special_kind == (a_special_function_kind)kind)

/* Return TRUE if a symbol is a constructor symbol. */
#define is_constructor_symbol(sym)                                    \
  (((sym)->kind == (a_symbol_kind)sk_member_function &&               \
    is_special_kind_function_symbol((sym), sfk_constructor)) ||       \
   ((sym)->kind == (a_symbol_kind)sk_overloaded_function &&           \
    is_special_kind_function_symbol(                                  \
       (sym)->variant.overloaded_function.symbols, sfk_constructor)))

/* Return TRUE if a symbol is a destructor symbol. */
#define is_destructor_symbol(sym)                                     \
  (((sym)->kind == (a_symbol_kind)sk_member_function &&               \
    is_special_kind_function_symbol((sym), sfk_destructor)) ||        \
   ((sym)->kind == (a_symbol_kind)sk_overloaded_function &&           \
    is_special_kind_function_symbol(                                  \
       (sym)->variant.overloaded_function.symbols, sfk_destructor)))

/* Return TRUE if a symbol is a copy constructor symbol. */
#define is_copy_constructor_symbol(sym, ptr_to_is_const_flag)         \
  ((sym)->kind == (a_symbol_kind)sk_member_function &&                \
   is_special_kind_function_symbol((sym), sfk_constructor) &&         \
   is_copy_constructor((sym)->variant.routine,                        \
                       (sym)->class_of_which_a_member,                \
                       ptr_to_is_const_flag))
/*
Extract the type from a type symbol (one for which is_type_symbol is TRUE).
*/
#define type_symbol_type(sym)                                         \
  (((sym)->kind == (a_symbol_kind)sk_type ||                          \
    (sym)->kind == (a_symbol_kind)sk_enum_tag) ?                      \
                               (sym)->variant.type :                  \
                               (sym)->variant.class_struct_union.type)

/*
Extract a pointer to the class symbol supplement for a given type for
which is_class_struct_union_type is TRUE.
*/
#define symbol_supplement_for_class(tp)                              \
  (((a_symbol_ptr)(skip_typerefs(tp))->source_corresp.assoc_info)->  \
                            variant.class_struct_union.extra_info)

/* Return a pointer to the current routine entry (only usable when within
   a routine definition). */
#define current_routine_entry()                                       \
  (scope_stack[depth_innermost_function_scope].il_scope->variant.routine.ptr)


#if DEBUG
/* Show and return the amount of memory used by symbol table entries. */
extern unsigned long show_symbol_space_used(void);
/* Display a symbol table entry. */
extern void db_symbol(a_symbol_ptr	sym,
                      char		*string,
                      int		indentation);

#endif /* DEBUG */

extern void sym_tbl_init(void);

#endif /* ifndef SYMBOL_TBL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C Front End                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright (C) 1988, 1989 Edison Design Group Inc.              [_]          *
*                                                                             *
******************************************************************************/
