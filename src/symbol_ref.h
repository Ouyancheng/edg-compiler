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

symbol_ref.h - Declarations related to symbol reference processing.

*/

/* Avoid including these declarations more than once. */
#ifndef SYMBOL_REF_H
#define SYMBOL_REF_H 1

#ifndef SYMBOL_TBL_H
#include "symbol_tbl.h"
#endif /* ifndef SYMBOL_TBL_H */
#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

EXTERN a_decl_sequence_number
		decl_seq_counter;
			/* Counter, initialized to 0 with each compilation
			   unit, for maintaining the declaration sequence
			   numbers for symbols. */

/*
Set the declaration sequence number of the symbol pointed to by sym.
*/
#define set_decl_sequence_number(sym) (sym)->decl_seq = ++decl_seq_counter

/*
A symbol-reference-set is a bit vector designed to describe the declarations
and uses of symbols.  The bit positions are specified by the SRK_ values
defined below.  The bit vector is used in generating cross-reference
information and in tracking use-def status of variables.  Note that "use" and
"modification" apply only to objects -- i.e., to variables and static and
nonstatic data members -- and that "address taken" applies only to objects
and functions.  A given reference may be described by the union of several
bits.  For example, every declaration has the SRK_DECLARATION bit set; those
which are also definitions have the SRK_DEFINITION bit set, too.  Similarly,
every reference has SRK_REFERENCE set, but most references will have one or
more additional bits set as well; for example, an increment expression like
"++x" would have SRK_USE and SRK_MODIFICATION set (as well as SRK_REFERENCE).

It is intended that implementations that need to track reference information
in more detail would be able to define (and maintain) additional bits in the
bit vector.  For example, bits could be defined to describe the specific ways
in which an address can be taken (e.g., to discriminate between taking the
address of a const and taking the address of a nonconst object).
*/
#define SRK_NONE 0x0
#define SRK_DECLARATION 0x1
			/* Any declaration. */
#define SRK_DEFINITION 0x2
			/* A declaration that is also definition. */
#define SRK_REFERENCE 0x4
			/* Any kind of reference.  Most commonly a reference
			   will be a use, a modification, an "address-taken",
			   or a reference in an error context (see following
			   bit positions).  If it is none of those, the
			   reference bit may still be set -- e.g., for a
			   reference to a class or typedef name in a
			   declaration, to a label in a goto statement, to a
			   routine name in a call, to a variable in a sizeof
			   operation, etc.). */
#define SRK_USE 0x8
			/* A use of the value of an object.  Both the use and
			   modification bits may be set for a given reference
			   (e.g., an increment). */
#define SRK_MODIFICATION 0x10
			/* A reference that changes the value of an object.
			   Both the use and modification bits may be set for a
			   given reference (e.g., an increment). */
#define SRK_ADDRESS_TAKEN 0x20
			/* A reference in which the address of an object or
			   function is taken. */
#define SRK_ERROR 0x40
			/* A reference of some sort, but because of an error
			   in the source the kind of reference is uncertain;
			   such a reference is treated both as a use and as a
			   modification, in order to suppress use/def
			   diagnostics. */
#define SRK_IMPLICIT 0x80
			/* A reference or declaration is implicit or a
			   definition involves an implicit initialization. */
#define SRK_FRIEND 0x100
			/* Or'ed with SRK_DECLARATION, a friend declaration. */
#define SRK_TENTATIVE_DEF 0x200
			/* Or'ed with SRK_DEFINITION, a variable declaration
			   is a tentative definition (C only). */
#define SRK_IMPLICIT_TEMPLATE_ARG 0x400
			/* Within a template instantiation, an implicit
			   reference to a name involved in a template argument
			   by means of an explicit reference to a template
			   parameter. */
#define SRK_INITIALIZATION 0x800
			/* Or'ed with SRK_DEFINITION to indicate a variable
			   initialization.  When SRK_IMPLICIT is set, a
			   constructor is called implicitly; otherwise, an
			   explicit initializer appeared in the definition. */
#define SRK_ALL_REFERENCES \
  (SRK_USE | SRK_MODIFICATION | SRK_ADDRESS_TAKEN | SRK_ERROR)
			/* All types of references.  Used to mask off those
			   bits. */


/* Record use information (for cross-reference, etc.). */
extern void record_symbol_declaration(
                            a_symbol_reference_kind      srk_flags,
                            a_symbol_ptr                 sym_ptr,
                            a_source_position            *source_position,
                            a_source_sequence_entry_ptr  ssep);

extern void record_symbol_reference(a_symbol_reference_kind  kind,
                                    a_symbol_ptr             sym_ptr,
                                    a_source_position        *source_position,
                                    a_boolean                update_il_entry);

#define mark_defined(sym, pos)                                          \
  record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, (sym),    \
                            (pos), (a_source_sequence_entry_ptr)NULL)
#define mark_declared(sym, pos)                                         \
  record_symbol_declaration(SRK_DECLARATION, (sym), (pos),              \
                            (a_source_sequence_entry_ptr)NULL)

#define mark_referenced(sym, err_pos)                                   \
  record_symbol_reference(SRK_REFERENCE, (sym), (err_pos),              \
                          /*update_il_entry=*/TRUE)

void reference_to_invalid_name(a_symbol_locator *locator);

extern void record_access_adjustment(an_access_adjustment_ptr  aap,
                                     a_symbol_ptr              sym,
                                     a_source_position         *pos);

extern void mark_variable_value_set(a_symbol_ptr  sym);

extern void reference_to_implicitly_invoked_function
                                    (a_symbol_ptr       sym,
                                     a_source_position  *pos,
                                     a_type_ptr         class_of_object,
                                     a_boolean          honor_virtual,
                                     a_boolean          evaluated,
                                     a_boolean          suppress_access_check);

#endif /* ifndef SYMBOL_REF_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1994 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
