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

extern void record_access_adjustment(an_access_adjustment_ptr  aap,
                                     a_symbol_ptr              sym,
                                     a_source_position         *pos);

extern void mark_variable_value_set(a_symbol_ptr  sym);

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
