/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

lower_name.h -- Declarations related to lower_name.c (name mangling for
                IL lowering).

*/

/* Avoid including these declarations more than once: */
#ifndef LOWER_NAME_H
#define LOWER_NAME_H 1

/* Only include this code if it is needed: */
/* NEED_NAME_MANGLING is always TRUE if DO_IL_LOWERING is TRUE. */
#if NEED_NAME_MANGLING

#ifndef IL_H
#include "il.h"
#endif /* ifndef IL_H */

/*
Entry used to record the position of a compressible string in a mangled name.
Used in compressing the mangled name.
*/
typedef struct a_compressible_string_pos *a_compressible_string_pos_ptr;
typedef struct a_compressible_string_pos {
  a_compressible_string_pos_ptr
		next;
			/* Next entry in the same bucket of the hash table. */
  sizeof_t	str_pos;
			/* The index of the compressible string in the
			   original mangled name. */
} a_compressible_string_pos;

EXTERN a_compressible_string_pos_ptr
		avail_compressible_string_pos;
			/* List of compressible string position entries freed
			   and available for reuse. */

#if DEBUG
/*
Count of entries allocated, for debugging purposes.
*/
EXTERN unsigned long
		num_compressible_string_pos_allocated;
#endif /* DEBUG */

#if TEMPLATE_LOOKUP_NEEDED || MICROSOFT_EXTENSIONS_ALLOWED
extern char *get_mangled_function_name(a_routine_ptr routine);
#endif /* TEMPLATE_LOOKUP_NEEDED || MICROSOFT_EXTENSIONS_ALLOWED */

#if TEMPLATE_LOOKUP_NEEDED
extern char *get_mangled_static_data_member_name(a_variable_ptr variable);
#endif /* TEMPLATE_LOOKUP_NEEDED */

extern char *mangled_vtbl_name(a_type_ptr       class_type,
                               a_base_class_ptr bcp,
                               a_base_class_ptr ctor_bcp);

extern char *mangled_class_name(a_type_ptr type);

extern char *mangled_typeinfo_name(a_type_ptr type);

extern char *mangled_id_object_name(a_type_ptr type);

#if DO_IL_LOWERING
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE || LOWER_EXTERN_INLINE
extern void mangle_promoted_entity_name(a_source_correspondence *scp,
                                        a_boolean               is_type,
                                        a_routine_ptr           routine,
                                        a_scope_ptr             scope);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE || LOWER_EXTERN_INLINE */
#endif /* DO_IL_LOWERING */

#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
extern void mangle_covariant_return_type_entry_name(
                                             a_routine_ptr entry_routine,
                                             a_routine_ptr prim_routine,
                                             a_type_ptr    vtbl_class);
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */

extern void do_all_name_mangling(void);

extern void name_lower_one_time_init(void);

extern void name_lower_init(void);

#endif /* NEED_NAME_MANGLING */
#endif /* ifndef LOWER_NAME_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1993 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
