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

#if AUTOMATIC_TEMPLATE_INSTANTIATION
extern char *get_mangled_function_name(a_routine_ptr routine);

extern char *get_mangled_static_data_member_name(a_variable_ptr variable);
#endif /* AUTOMATIC_TEMPLATE_INSTANTIATION */

extern sizeof_t mangled_class_name(a_type_ptr type,
                                   char       *store_at);

extern sizeof_t mangled_vtbl_name(a_type_ptr       class_type,
                                  a_base_class_ptr bcp,
                                  char             *store_at);

#if GENERATE_EH_TABLES
extern sizeof_t mangled_typeinfo_name(a_type_ptr type,
                                      char       *store_at);

extern sizeof_t mangled_id_object_name(a_type_ptr type,
                                       char       *store_at);
#endif /* GENERATE_EH_TABLES */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
extern void mangle_promoted_entity_name(a_source_correspondence *scp,
                                        a_routine_ptr           routine,
                                        a_scope_ptr             scope);
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */

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
