/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

lower_eh.c -- IL lowering for exception handling constructs.

*/

#include "basics.h"
#include "host_envir.h"

/* Only include this code if it is needed: */
#if DO_IL_LOWERING

#include "lower_eh.h"
#include "lower_il.h"
#include "lower_init.h"
#include "lower_name.h"
#include "debug.h"
#include "error.h"
#include "il.h"
#include "types.h"
#include "const_ints.h"
#include "cmd_line.h"


static unsigned long
		num_of_pending_class_typeinfo_vars;
			/* Count of typeinfo variables generated for classes
			   that have not been revisited to determine whether
			   they need definitions.  Used to cut short the
			   final pass that finds and defines the variables. */

/*
Pointer to the typeinfo struct type (used to represent runtime type
information).  NULL until created.
*/
static a_type_ptr
		typeinfo_type;

static a_type_ptr make_typeinfo_type(void)
/*
Make the typeinfo struct type (used to represent runtime type information)
if it is not made already, and return a pointer to it.  Its definition is

  struct typeinfo {
    char     *id;   // Id object pointer
    __vptp   dtor;  // Destructor
    typeinfo **bc;  // Pointer to base class array
  };

*/
{
  a_targ_size_t byte_offset;
  a_field_ptr   last_field;

  if (typeinfo_type == NULL) {
    /* Make the struct type. */
    typeinfo_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(typeinfo_type);
    byte_offset = 0;
    last_field = NULL;
    /* field: char *id */
    make_lowered_field("id",
                     make_pointer_type(integer_type((an_integer_kind)ik_char)),
                       &byte_offset, typeinfo_type, &last_field);
    /* field: __vptp dtor */
    make_lowered_field("dtor", make_vptp_type(),
                       &byte_offset, typeinfo_type, &last_field);
    /* field: typeinfo **bc */
    make_lowered_field("bc",
                       make_pointer_type(make_pointer_type(typeinfo_type)),
                       &byte_offset, typeinfo_type, &last_field);
    finish_class_type(typeinfo_type, &byte_offset);
  }  /* if */
  return typeinfo_type;
}  /* make_typeinfo_type */


static a_variable_ptr make_typeinfo_var(a_type_ptr type)
/*
Make a typeinfo variable for the indicated type (if it does not exist
already) and return a pointer to it.  The variable points to runtime
type information.
*/
{
  a_variable_ptr  typeinfo_var;
  char            *mangled_name;
  sizeof_t        mangled_name_length, alloc_length;
  a_storage_class storage_class;

  /* No need to create the variable if it exists already. */
  typeinfo_var = type->typeinfo_var;
  if (typeinfo_var == NULL) {
    /* Determine the length of the mangled name. */
    mangled_name_length = mangled_typeinfo_name(type, (char *)NULL);
    /* Allocate space for the mangled name, including the final null. */
    alloc_length = mangled_name_length + 1;
    mangled_name = alloc_lowered_name_string(alloc_length);
    /* Build the mangled name. */
    (void)mangled_typeinfo_name(type, mangled_name);
    mangled_name[mangled_name_length] = '\0';
    if (is_immediate_class_type(type)) {
      /* typeinfo variables for classes are sometimes external, sometimes
         static, but we don't know which yet.  Start with external, and
         change later if necessary. */
      storage_class = (a_storage_class)sc_extern;
      /* Keep a count of the number of class typeinfo variables so that the
         final pass to add definitions for these can be stopped when all
         of them have been found. */
      num_of_pending_class_typeinfo_vars++;
    } else {
      /* typeinfo variables for non-classes are always external tentative
         definitions (initialized to NULL/zero by default). */
      storage_class = (a_storage_class)sc_unspecified;
    }  /* if */
    typeinfo_var = make_lowered_variable(mangled_name,
                                         /*already_il_name=*/TRUE,
                                         make_typeinfo_type(),
                                         storage_class);
    typeinfo_var->source_corresp.name_has_been_mangled = TRUE;
    /* Remember the variable in the type. */
    type->typeinfo_var = typeinfo_var;
  }  /* if */
  return typeinfo_var;
}  /* make_typeinfo_var */


static a_variable_ptr make_id_object_var(a_type_ptr type)
/*
Make an id object variable for the given type, and return a pointer to it.
The id object variable is pointed to from the definition of the typeinfo for
the type.  When static typeinfo objects are put out in multiple files,
they will point to the same (external) id object, so one can tell that they
all denote the same type.  type must be an externally-linked class type.
*/
{
  a_variable_ptr  id_object_var;
  char            *mangled_name;
  sizeof_t        mangled_name_length, alloc_length;

  /* Determine the length of the mangled name. */
  mangled_name_length = mangled_id_object_name(type, (char *)NULL);
  /* Allocate space for the mangled name, including the final null. */
  alloc_length = mangled_name_length + 1;
  mangled_name = alloc_lowered_name_string(alloc_length);
  /* Build the mangled name. */
  (void)mangled_id_object_name(type, mangled_name);
  mangled_name[mangled_name_length] = '\0';
  /* Make the variable. */
  id_object_var = make_lowered_variable(mangled_name,
                                        /*already_il_name=*/TRUE,
                                        integer_type((an_integer_kind)ik_char),
                                        (a_storage_class)sc_unspecified);
  id_object_var->source_corresp.name_has_been_mangled = TRUE;
  return id_object_var;
}  /* make_id_object_var */


static a_variable_ptr make_base_class_array_var(a_type_ptr type)
/*
type is a class type that has base classes.  Make a variable initialized
with an array of typeinfo pointers for the base classes of the type.
This is used as part of the typeinfo information.
*/
{
  a_base_class_ptr bcp;
  unsigned long    base_class_count;
  a_type_ptr       array_type;
  a_constant_ptr   aggr_con, con;
  a_variable_ptr   typeinfo_var, bc_var;

  /* The current region is already the file scope memory region when
     this routine is called. */
  /* The initial value is an aggregate constant pointing to a list of
     constants that are pointers to typeinfo variables. */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  base_class_count = 0;
  for (bcp = type->variant.class_struct_union.extra_info->base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    /* Include information only on direct base classes. */
    if (bcp->direct) {
      base_class_count++;
      /* Make an address constant for a pointer to the base class typeinfo
         variable. */
      con = alloc_constant((a_constant_repr_kind)ck_address);
      typeinfo_var = bcp->type->typeinfo_var;
      check_assertion_str(typeinfo_var != NULL,
                          "make_base_class_array_var: NULL typeinfo var");
      set_variable_address_constant(typeinfo_var, con);
      /* Add the constant to the aggregate list. */
      if (aggr_con->variant.aggregate.first_constant == NULL) {
        aggr_con->variant.aggregate.first_constant = con;
      } else {
        aggr_con->variant.aggregate.last_constant->next = con;
      }  /* if */
      aggr_con->variant.aggregate.last_constant = con;
    }  /* if */
  }  /* for */
  check_assertion_str(base_class_count != 0,
                      "make_base_class_array_var: no base classes");
  /* Add a zero entry to indicate the end of the list. */
  con = alloc_constant((a_constant_repr_kind)ck_address);
  make_zero_of_proper_type(make_pointer_type(typeinfo_type), con);
  aggr_con->variant.aggregate.last_constant->next = con;
  aggr_con->variant.aggregate.last_constant = con;
  /* Make a type that is an array of pointers to typeinfo entries.
     Leave room for the zero entry at the end. */
  array_type = alloc_type((a_type_kind)tk_array);
  array_type->variant.array.variant.number_of_elements = base_class_count+1;
  array_type->variant.array.element_type = make_pointer_type(typeinfo_type);
  set_type_size(array_type);
  /* Make the variable.  It is unnamed and static. */
  bc_var = make_unnamed_local_static_variable(array_type);
  /* Attach the aggregate constant as the initial value of the variable. */
  bc_var->init_kind = (an_init_kind)initk_static;
  bc_var->initializer.constant = aggr_con;
  return bc_var;
}  /* make_base_class_array_var */


static void define_typeinfo_var(a_type_ptr type,
                                a_boolean  force_static)
/*
Generate a definition for the typeinfo variable (used to provide runtime
type information) associated with type "type".  "type" must be a class type
(because the typeinfo variables for nonclass types are never defined --
tentative definition establishes the proper NULL values).  If force_static
is TRUE, change the typeinfo variable to static.
*/
{
  a_variable_ptr typeinfo_var = type->typeinfo_var;
  a_constant_ptr aggr_con, id_con, dtor_con, bc_con;
  a_field_ptr    curr_field;
  a_type_ptr     curr_field_type;
  a_symbol_ptr   dtor_sym;
  a_memory_region_number
                 region_to_switch_back_to;

  check_assertion(is_immediate_class_type(type));
  /* Switch to the file scope memory region so that initial values will
     be allocated there. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Set the linkage on the typeinfo variable. */
  if (force_static) {
    /* When forced to by the flag force_static, change the storage class to
       static and the linkage to internal. */
    typeinfo_var->storage_class = (a_storage_class)sc_static;
    typeinfo_var->source_corresp.name_linkage =
                                             (a_name_linkage_kind)nlk_internal;
  } else {
    /* For an externally-linked class, change the variable to an external
       definition. */
    typeinfo_var->storage_class = (a_storage_class)sc_unspecified;
    /* The variable can be referenced from another compilation unit.
       This is probably already set. */
    typeinfo_var->source_corresp.referenced = TRUE;
  }  /* if */
  /* The initial value of the typeinfo variable is an aggregate containing
     values as follows:
       1)  Id object: pointer to id object variable, or NULL if the class
           is internally linked.
       2)  Destructor: pointer to destructor, or NULL.
       3)  Base class array pointer: pointer to array containing pointers
           to typeinfo structures for base classes, or NULL if there are
           no base classes.
  */
  /* Id object pointer. */
  id_con = alloc_constant((a_constant_repr_kind)ck_address);
  curr_field = typeinfo_type->variant.class_struct_union.field_list;
  curr_field_type = curr_field->type;
  /* Note that we test for nlk_external and not nlk_cplusplus_external here
     because the linkage has already been rewritten. */
  if (type->source_corresp.name_linkage != (a_name_linkage_kind)nlk_external) {
    /* Internally linked class; id object pointer is NULL. */
    make_zero_of_proper_type(curr_field_type, id_con);
  } else {
    /* Externally linked class; make id object variable. */
    set_variable_address_constant(make_id_object_var(type), id_con);
  }  /* if */
  /* Destructor pointer. */
  curr_field = curr_field->next;
  curr_field_type = curr_field->type;
  dtor_con = alloc_constant((a_constant_repr_kind)ck_address);
  /* See if the class has a destructor. */
  dtor_sym = symbol_supplement_for_class(type)->destructor;
  if (dtor_sym == NULL) {
    /* The class has no destructor; use a NULL pointer. */
    make_zero_of_proper_type(curr_field_type, dtor_con);
  } else {
    /* The class has a destructor.  Make a pointer to the routine. */
    a_routine_ptr dtor_routine = dtor_sym->variant.routine.ptr;
    set_routine_address_constant(dtor_routine, dtor_con);
  }  /* if */
  /* Base class array pointer. */
  curr_field = curr_field->next;
  curr_field_type = curr_field->type;
  bc_con = alloc_constant((a_constant_repr_kind)ck_address);
  if (type->variant.class_struct_union.extra_info->base_classes == NULL) {
    /* The class has no base classes; use a NULL pointer. */
    make_zero_of_proper_type(curr_field_type, bc_con);
  } else {
    /* The class has base classes; make a variable whose initial value is
       an array of pointers to the typeinfo information for the base classes,
       and use its address here. */
    a_variable_ptr bc_var = make_base_class_array_var(type);
    set_variable_address_constant(bc_var, bc_con);
  }  /* if */
  /* Make the aggregate constant and attach it to the variable as its initial
     value. */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->variant.aggregate.first_constant = id_con;
  aggr_con->variant.aggregate.last_constant = bc_con;
  id_con->next = dtor_con;
  dtor_con->next = bc_con;
  typeinfo_var->init_kind = (an_init_kind)initk_static;
  typeinfo_var->initializer.constant = aggr_con;
  /* Return to the memory region that was current when this routine was
     entered. */
  switch_back_to_original_region(region_to_switch_back_to);
}  /* define_typeinfo_var */


void define_scope_class_typeinfo_vars(a_scope_ptr scope)
/*
Visit all the class types of the indicated scope and look for typeinfo
variables (generated earlier).  For each typeinfo variable, generate
the appropriate definition if one is needed.  This must be done late
in the lowering process, so that all necessary typeinfo variables have
been generated already.
*/
{
  a_type_ptr  type;
  a_scope_ptr class_scope, block_scope;
  a_boolean   definition_needed, force_static;

  /* Visit all types to find all class types. */
  /* Note that when processing a function or block scope we will be crossing
     into the file scope here, but these class types are truly local types
     and are not used in the file scope, so it's okay to define their
     typeinfo variables now. */
  /* Once all of the typeinfo variables have been found, quit. */
  for (type = scope->types;
       type != NULL && num_of_pending_class_typeinfo_vars != 0;
       type = type->next) {
    if (is_immediate_class_type(type)) {
      /* Found a class type. */
      /* If the type has an associated typeinfo variable, define it now. */
      if (type->typeinfo_var != NULL) {
        /* If the class has a virtual function table, the typeinfo variable
           is defined if and only if the virtual function table is defined. */
        a_variable_ptr vtbl_var = type->variant.class_struct_union.extra_info->
                                                    virtual_function_table_var;
        if (vtbl_var != NULL) {
          definition_needed =
                           (vtbl_var->init_kind == (an_init_kind)initk_static);
          /* The typeinfo variable is static if the virtual function table
             is static. */
          force_static =
                       (vtbl_var->storage_class == (a_storage_class)sc_static);
        } else {
          /* The class has no virtual function table (i.e., it's not
             polymorphic), so its typeinfo variable must be defined and must
             be static. */
          definition_needed = force_static = TRUE;
        }  /* if */
        if (definition_needed) define_typeinfo_var(type, force_static);
        num_of_pending_class_typeinfo_vars--;
      }  /* if */
      /* If the class has a definition, visit its members. */
      class_scope = type->variant.class_struct_union.extra_info->assoc_scope;
      if (class_scope != NULL) define_scope_class_typeinfo_vars(class_scope);
    }  /* if */
  }  /* for */
  /* Visit all block scopes. */
  /* Once all of the typeinfo variables have been found, quit. */
  for (block_scope = scope->scopes;
       block_scope != NULL && num_of_pending_class_typeinfo_vars != 0;
       block_scope = block_scope->next) {
    define_scope_class_typeinfo_vars(block_scope);
  }  /* for */
}  /* define_scope_class_typeinfo_vars */


/*
Pointer to the exception_type_specification struct type (used to represent
a type for exception throw and catch specifications).  NULL until created.
*/
static a_type_ptr
		exception_type_specification_type;

/*
Bit set values for the flags byte of exception_type_specification.
These must match the runtime's definition of these values.
*/
#define ETS_IS_POINTER		0x01
			/* A pointer to an object of the type specified
			   by typeinfo. */
#define ETS_IS_REFERENCE	0x02
			/* A reference to an object of the type specified
			   by typeinfo. */
#define ETS_IS_ELLIPSIS		0x04
			/* An ellipsis (for a catch clause). */
#define ETS_LAST		0x08
			/* TRUE if this is the last type specification in
			   the array. */


static a_type_ptr make_exception_type_specification_type(void)
/*
Make the exception_type_specification struct type (used to represent
a type for exception throw and catch specifications) if it is not made
already, and return a pointer to it.  Its definition is

  struct exception_type_specification {
    typeinfo      *tinfo;
    unsigned char flags;
  };

*/
{
  a_targ_size_t byte_offset;
  a_field_ptr   last_field;

  if (exception_type_specification_type == NULL) {
    /* Make the struct type. */
    exception_type_specification_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(exception_type_specification_type);
    byte_offset = 0;
    last_field = NULL;
    /* field: typeinfo *tinfo */
    make_lowered_field("tinfo", make_pointer_type(make_typeinfo_type()),
                       &byte_offset, exception_type_specification_type,
                       &last_field);
    /* field: unsigned char flags */
    make_lowered_field("flags",
                       integer_type((an_integer_kind)ik_unsigned_char),
                       &byte_offset, exception_type_specification_type,
                       &last_field);
    finish_class_type(exception_type_specification_type, &byte_offset);
  }  /* if */
  return exception_type_specification_type;
}  /* make_exception_type_specification_type */


static a_variable_ptr typeinfo_var_for_type(a_type_ptr type,
                                            long       *flags_value)
/*
Create the typeinfo variable for the indicated type, and return a pointer
to it.  Type qualifiers on the type are dropped.  For a pointer or reference
to a class type, make the typeinfo variable for the underlying class and
set *flags_value to indicate a pointer or reference.
*/
{
  a_variable_ptr typeinfo_var;
  a_type_ptr     typeinfo_type;

  type = skip_typerefs(type);
  typeinfo_type = type;
  *flags_value = 0;
  if (is_ptr_or_ref_type(type)) {
    /* For a pointer or reference to a class type, use the typeinfo for the
       class. */
    a_type_ptr base_type = type_pointed_to(type);
    if (is_class_struct_union_type(base_type)) {
      typeinfo_type = f_skip_typerefs(base_type);
      *flags_value = is_pointer_type(type) ? ETS_IS_POINTER : ETS_IS_REFERENCE;
    }  /* if */
  }  /* if */
  typeinfo_var = make_typeinfo_var(typeinfo_type);
  return typeinfo_var;
}  /* typeinfo_var_for_type */


/*
Pointer to the eh_region_descr struct type (used to represent a cleanup
region for exception processing).  NULL until created.
*/
static a_type_ptr
		eh_region_descr_type;

static a_type_ptr make_eh_region_descr_type(void)
/*
Make the eh_region_descr struct type (used to represent the cleanup required
in a particular region for exception processing) if it is not made already,
and return a pointer to it.  Its definition is

  struct eh_region_descr {
    __vptp         dtor;    // Destructor or delete routine pointer
    unsigned short handle;  // Index of object in object address table
    unsigned short prev;    // Previous cleanup region
    unsigned char  flags;   // Bit flags
  };

*/
{
  a_targ_size_t byte_offset;
  a_field_ptr   last_field;

  if (eh_region_descr_type == NULL) {
    /* Make the struct type. */
    eh_region_descr_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(eh_region_descr_type);
    byte_offset = 0;
    last_field = NULL;
    /* field: __vptp dtor */
    make_lowered_field("dtor", make_vptp_type(), &byte_offset,
                       eh_region_descr_type, &last_field);
    /* field: unsigned short handle */
    make_lowered_field("handle",
                       integer_type(TARG_VAR_HANDLE_INT_KIND),
                       &byte_offset, eh_region_descr_type, &last_field);
    /* field: unsigned short prev */
    make_lowered_field("prev",
                       integer_type(TARG_REGION_NUMBER_INT_KIND),
                       &byte_offset, eh_region_descr_type, &last_field);
    /* field: unsigned char flags */
    make_lowered_field("flags",
                       integer_type((an_integer_kind)ik_unsigned_char),
                       &byte_offset, eh_region_descr_type, &last_field);
    finish_class_type(eh_region_descr_type, &byte_offset);
  }  /* if */
  return eh_region_descr_type;
}  /* make_eh_region_descr_type */


/*
Pointer to the jmp_buf type (used for setjmp/longjmp as part of exception
try/throw).  NULL until created.
*/
static a_type_ptr
		jmp_buf_type;

static a_type_ptr make_jmp_buf_type(void)
/*
Make the jmp_buf type (used for setjmp/longjmp as part of exception
try/throw) if it is not made already, and return a pointer to it.
*/
{
  if (jmp_buf_type == NULL) {
    /* jmp_buf is an array; that's part of the standard.  We assume it's
       an array of elements of some integral type; that's not standard,
       but it's common.  For other cases, one can pick an integral kind
       and number of elements to give the right size and alignment. */
    jmp_buf_type = alloc_type((a_type_kind)tk_array);
    jmp_buf_type->variant.array.element_type =
                                   integer_type(TARG_JMP_BUF_ELEMENT_INT_KIND);
    jmp_buf_type->variant.array.variant.number_of_elements =
                                                     TARG_JMP_BUF_NUM_ELEMENTS;
    set_type_size(jmp_buf_type);
  }  /* if */
  return jmp_buf_type;
}  /* make_jmp_buf_type */


/*
Pointer to the eh_stack_entry struct type (used to represent a stack frame
for exception processing).  NULL until created.  Also fields within that
type.
*/
static a_type_ptr
		eh_stack_entry_type;
static a_field_ptr
		ehse_next_field,
		ehse_kind_field,
		ehse_variant_field,
		ehse_try_field,
		ehse_try_setjmp_buffer_field,
		ehse_try_catch_entries_field,
		ehse_function_field,
		ehse_function_regions_field,
		ehse_function_obj_table_field,
		ehse_function_saved_region_number_field,
		ehse_throw_spec_field;

/*
Values for the "kind" field of eh_stack_entry.  This must match the runtime's
definition of these values.
*/
typedef enum {
  ehsek_try_block,
  ehsek_function,
  ehsek_throw_spec
} an_eh_stack_entry_kind;


static a_type_ptr make_eh_stack_entry_type(void)
/*
Make the eh_stack_entry struct type (used to represent a stack frame for
exception processing) if it is not made already, and return a pointer to it.
Its definition is

  struct eh_stack_entry {
    eh_stack_entry *next;  // Next stack frame
    unsigned char  kind    // try, function, or throw
    union {
      struct {
        jmp_buf  setjmp_buffer; // Buffer for setjmp
        exception_type_specification *catch_entries;  // Catch list
      } try_block;
      struct {
        eh_region_descr *regions;  // Cleanup regions
        void     **obj_table;      // Object address table
        unsigned short saved_region_number; // Saved __eh_curr_region
      } function;
      exception_type_specification *throw_spec; // Throw spec list
    } variant;
  };

*/
{
  a_targ_size_t byte_offset;
  a_field_ptr   last_field;
  a_type_ptr    try_block_struct_type, function_struct_type;
  a_type_ptr    variant_union_type, ptr_exception_type_specification;

  if (eh_stack_entry_type == NULL) {
    /* Make the class types (without defining them) to get them on the
       types list in the right order (they get added to the front so they
       end up in reverse order of insertion). */
    /* Make the eh_stack_entry struct type. */
    eh_stack_entry_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(eh_stack_entry_type);
    /* Make the variant union type. */
    variant_union_type = alloc_type((a_type_kind)tk_union);
    add_to_front_of_file_scope_types_list(variant_union_type);
    /* Make the try_block variant struct. */
    try_block_struct_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(try_block_struct_type);
    byte_offset = 0;
    last_field = NULL;
    /* field: jmp_buf setjmp_buffer */
    make_lowered_field("setjmp_buffer", make_jmp_buf_type(),
                       &byte_offset, try_block_struct_type, &last_field);
    ehse_try_setjmp_buffer_field = last_field;
    /* field: exception_type_specification *catch_entries */
    ptr_exception_type_specification =
                   make_pointer_type(make_exception_type_specification_type());
    make_lowered_field("catch_entries", ptr_exception_type_specification,
                       &byte_offset, try_block_struct_type, &last_field);
    ehse_try_catch_entries_field = last_field;
    finish_class_type(try_block_struct_type, &byte_offset);
    /* Make the function variant struct. */
    function_struct_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(function_struct_type);
    byte_offset = 0;
    last_field = NULL;
    /* field: eh_region_descr *regions */
    make_lowered_field("regions",
                       make_pointer_type(make_eh_region_descr_type()),
                       &byte_offset, function_struct_type, &last_field);
    ehse_function_regions_field = last_field;
    /* field: void **obj_table */
    make_lowered_field("obj_table",
                       make_pointer_type(void_star_type()),
                       &byte_offset, function_struct_type, &last_field);
    ehse_function_obj_table_field = last_field;
    /* field: unsigned short saved_region_number */
    make_lowered_field("saved_region_number",
                       integer_type(TARG_REGION_NUMBER_INT_KIND),
                       &byte_offset, function_struct_type, &last_field);
    ehse_function_saved_region_number_field = last_field;
    finish_class_type(function_struct_type, &byte_offset);
    /* Define the variant union type. */
    byte_offset = 0;
    last_field = NULL;
    /* field: struct {...} try_block */
    make_lowered_field("try_block", try_block_struct_type,
                       &byte_offset, variant_union_type, &last_field);
    ehse_try_field = last_field;
    /* field: struct {...} function */
    make_lowered_field("function", function_struct_type,
                       &byte_offset, variant_union_type, &last_field);
    ehse_function_field = last_field;
    /* field: exception_type_specification *throw_spec */
    make_lowered_field("throw_spec", ptr_exception_type_specification,
                       &byte_offset, variant_union_type, &last_field);
    ehse_throw_spec_field = last_field;
    finish_class_type(variant_union_type, &byte_offset);
    /* Define the eh_stack_entry struct type. */
    byte_offset = 0;
    last_field = NULL;
    /* field: eh_stack_entry *next */
    make_lowered_field("next", make_pointer_type(eh_stack_entry_type),
                       &byte_offset, eh_stack_entry_type, &last_field);
    ehse_next_field = last_field;
    /* field: unsigned char kind */
    make_lowered_field("kind",
                       integer_type((an_integer_kind)ik_unsigned_char),
                       &byte_offset, eh_stack_entry_type, &last_field);
    ehse_kind_field = last_field;
    /* field: union {...} variant */
    make_lowered_field("variant", variant_union_type,
                       &byte_offset, eh_stack_entry_type, &last_field);
    ehse_variant_field = last_field;
    finish_class_type(eh_stack_entry_type, &byte_offset);
  }  /* if */
  return eh_stack_entry_type;
}  /* make_eh_stack_entry_type */


void type_is_used_in_exception(a_type_ptr type)
/*
The indicated type is used in an exception context.  Put out any necessary
information on it.
*/
{
  if (is_ptr_or_ref_type(type)) {
    /* For a pointer or reference to a class, remove the pointer or reference
       level of the type. */
    a_type_ptr base_type = type_pointed_to(type);
    if (is_class_struct_union_type(base_type)) type = base_type;
  }  /* if */
  /* Typedefs and qualifiers should be ignored. */
  type = f_skip_typerefs(type);
  /* We need a typeinfo variable for the underlying type.  Make it if it
     does not exist already. */
  if (type->typeinfo_var == NULL) {
    (void)make_typeinfo_var(type);
    /* If the type is a class, we also need typeinfo variables for its
       base classes. */
    if (is_immediate_class_type(type)) {
      a_base_class_ptr bcp;
      for (bcp = type->variant.class_struct_union.extra_info->base_classes;
           bcp != NULL;
           bcp = bcp->next) {
        (void)make_typeinfo_var(bcp->type);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* type_is_used_in_exception */


/*
Pointers to routine entries for the runtime routines __throw_alloc,
__throw, and __rethrow, used in throwing exceptions.  NULL until allocated.
*/
static a_routine_ptr
		throw_alloc_routine,
		throw_routine,
		rethrow_routine;


void lower_throw(an_expr_node_ptr expr)
/*
Lower an enk_throw expression node.
*/
{
  a_type_ptr         throw_type, ptr_throw_type;
  a_variable_ptr     temp_var, typeinfo_var;
  an_expr_node_ptr   call_node, typeinfo_node, size_node, flags_node;
  an_expr_node_ptr   temp_node, assign_node;
  a_dynamic_init_ptr dip;
  long               flags_value;                
  an_init_pos_descr  ipd;
  an_insert_location insert_location;
  a_boolean          keep_dynamic_init;

  /* Check for a throw with no operand, i.e., a rethrow. */
  if (expr->variant.throw_info == NULL) {
    /* This is a rethrow.  Replace the enk_throw node with a call of
       __rethrow. */
    call_node = make_runtime_rout_call("__rethrow", &rethrow_routine,
                                       void_type(), (an_expr_node_ptr)NULL);
    overwrite_node(expr, call_node);
  } else {
    /* Throw of an object. */
    throw_type = expr->variant.throw_info->type;
    lower_os_type(throw_type);
    throw_type = f_skip_typerefs(throw_type);  /* Probably unnecessary. */
    dip = expr->variant.throw_info->dynamic_init;
    /* Make the assignment
         temp = __throw_alloc(&typeinfo, size, flags)
       This allocates the space into which the thrown object is copied and
       sets temp to point to that space.  typeinfo is the typeinfo variable
       for the base type of the type thrown; size is the size in bytes of
       the type thrown; and flags has the ETS_IS_POINTER bit set to indicate
       that a pointer to the typeinfo type is being thrown. */
    ptr_throw_type = make_pointer_type(throw_type);
    temp_var = make_lowered_temporary(ptr_throw_type);
    /* Make the typeinfo variable for the throw type. */
    typeinfo_var = typeinfo_var_for_type(throw_type, &flags_value);
    /* Make the arguments for the __throw_alloc call. */
    typeinfo_node = var_lvalue_expr(typeinfo_var);
    size_node = node_for_integer_constant((long)throw_type->size,
                                        (an_integer_kind)TARG_SIZE_T_INT_KIND);
    typeinfo_node->next = size_node;
    flags_node = node_for_integer_constant(flags_value,
                                           (an_integer_kind)ik_int);
    size_node->next = flags_node;
    /* Make the __throw_alloc call. */
    call_node = make_runtime_rout_call("__throw_alloc", &throw_alloc_routine,
                                       void_star_type(), typeinfo_node);
    /* Cast the pointer to the right type. */
    call_node = add_cast_if_necessary(call_node, ptr_throw_type);
    /* Make the node to assign the pointer to the temporary. */
    temp_node = var_lvalue_expr(temp_var);
    temp_node->next = call_node;
    assign_node = make_operator_node((an_expr_operator_kind)eok_passign,
                                     ptr_throw_type, temp_node);
    /* Make the call to the __throw routine, which actually does the
       throw.  It has no arguments. */
    call_node = make_runtime_rout_call("__throw", &throw_routine,
                                       void_type(), (an_expr_node_ptr)NULL);
    /* Overwrite the original node with a comma expression joining the
       __throw_alloc and __throw expressions:
         ((temp = __throw_alloc(...)), __throw())
    */
    assign_node->next = call_node;
    set_expr_node_kind(expr, (an_expr_node_kind)enk_operation);
    set_node_operator(expr, (an_expr_operator_kind)eok_comma,
                      call_node->type, assign_node);
    /* Now generate the initialization code for the dynamic initialization
       and insert it preceding the call of __throw.  That gets it between
       the allocation and the throw:
         ((temp = __throw_alloc(...)), (initialization, __throw()))
       The address to be initialized is pointed to by the temporary. */
    set_var_indirect_init_pos_descr(temp_var, &ipd);
    set_expr_insert_location(call_node, &insert_location);
    /* Erase the destructor call if there is one.  The runtime takes care
       of the destruction. */
    dip->destructor = NULL;
    lower_dynamic_init(dip, &ipd,
                       /*first_time_test_var=*/(a_variable_ptr)NULL,
                       /*is_expr_temporary=*/FALSE,
                       (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                       (a_constructor_init_ptr)NULL,
                       &insert_location, &keep_dynamic_init);
    check_assertion(!keep_dynamic_init);
  }  /* if */
}  /* lower_throw */


/*
Variable entries for __eh_curr_region and __curr_eh_stack_entry,
global variables used for exception processing.  NULL until created.
*/
static a_variable_ptr
		eh_curr_region_var,
		curr_eh_stack_entry_var;

static a_variable_ptr make_eh_curr_region_var(void)
/*
Make __eh_curr_region, a global variable used for exception processing,
if it has not already been made.  Return a pointer to it.
*/
{
  if (eh_curr_region_var == NULL) {
    eh_curr_region_var =
               make_lowered_variable("__eh_curr_region",
                                     /*already_il_name=*/FALSE,
                                     integer_type(TARG_REGION_NUMBER_INT_KIND),
                                     (a_storage_class)sc_extern);
  }  /* if */
  return eh_curr_region_var;
}  /* make_eh_curr_region_var */


static a_variable_ptr make_curr_eh_stack_entry_var(void)
/*
Make __curr_eh_stack_entry, a global variable used for exception processing,
if it has not already been made.  Return a pointer to it.
*/
{
  if (curr_eh_stack_entry_var == NULL) {
    curr_eh_stack_entry_var =
           make_lowered_variable("__curr_eh_stack_entry",
                                 /*already_il_name=*/FALSE,
                                 make_pointer_type(make_eh_stack_entry_type()),
                                 (a_storage_class)sc_extern);
  }  /* if */
  return curr_eh_stack_entry_var;
}  /* make_curr_eh_stack_entry_var */


static a_variable_ptr make_exception_type_specification_array_var(void)
/*
Create a variable whose initial value will be an array of exception type
specification entries, and return a pointer to the variable.
*/
{
  a_variable_ptr var;
  a_type_ptr     array_type;
  a_constant_ptr aggr_con;

  /* The current region is already the file scope memory region when
     this routine is called. */
  /* The initial value is an aggregate constant pointing to a list of
     aggregate constants. */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  /* Make a type that is an array of exception_type_specification entries. */
  array_type = alloc_type((a_type_kind)tk_array);
  array_type->variant.array.variant.number_of_elements = 0; /* Initially. */
  array_type->variant.array.element_type =
                                      make_exception_type_specification_type();
  /* set_type_size is not called yet. */
  /* Make the variable.  It is unnamed and static. */
  var = make_unnamed_local_static_variable(array_type);
  /* Attach the aggregate constant as the initial value of the variable. */
  var->init_kind = (an_init_kind)initk_static;
  var->initializer.constant = aggr_con;
  return var;
}  /* make_exception_type_specification_array_var */


static void add_exception_type_specification_array_entry(a_type_ptr     type,
                                                         a_variable_ptr var)
/*
Add an entry that describes the type "type" to the array of exception type
specifications being built up as the initializer of the variable "var".
If type is NULL, add an ellipsis entry.
*/
{
  a_variable_ptr typeinfo_var;
  long           flags_value;                
  a_constant_ptr typeinfo_con, flags_con, aggr_con, array_aggr_con;

  /* The current region is already the file scope memory region when
     this routine is called. */
  /* Each element of the array is an exception_type_specification struct
     containing a pointer to the typeinfo information and a flags byte.
     The flags byte indicates the cases where the type indicated is a
     reference or pointer to the typeinfo type. */
  /* Create an aggregate constant with two constants (a pointer to the
     typeinfo variable and the flags value) under it. */
  typeinfo_con = alloc_constant((a_constant_repr_kind)ck_address);
  if (type == NULL) {
    /* This entry is for an ellipsis, so the typeinfo pointer is NULL. */
    make_zero_of_proper_type(make_pointer_type(make_typeinfo_type()),
                             typeinfo_con);
  } else {
    /* Normal case. */
    typeinfo_var = typeinfo_var_for_type(type, &flags_value);
    set_variable_address_constant(typeinfo_var, typeinfo_con);
  }  /* if */
  flags_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_integer_constant(flags_con, flags_value,
                       (an_integer_kind)ik_unsigned_char);
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->variant.aggregate.first_constant = typeinfo_con;
  typeinfo_con->next = flags_con;
  aggr_con->variant.aggregate.last_constant = flags_con;
  /* Add this aggregate constant to the list of constants under the aggregate
     constant for the array. */
  array_aggr_con = var->initializer.constant;
  if (array_aggr_con->variant.aggregate.first_constant == NULL) {
    /* This is the first element of the array. */
    array_aggr_con->variant.aggregate.first_constant = aggr_con;
  } else {
    array_aggr_con->variant.aggregate.last_constant->next = aggr_con;
  }  /* if */
  array_aggr_con->variant.aggregate.last_constant = aggr_con;
  /* Add one to the array size.  set_type_size is called later. */
  var->type->variant.array.variant.number_of_elements++;
}  /* add_exception_type_specification_array_entry */


static void finish_exception_type_specification_array(a_variable_ptr var)
/*
Finish the definition of a variable whose value is an array of exception
type specification entries.
*/
{
  a_constant_ptr array_aggr_con, aggr_con, flags_con;
  long           flags_value;
  a_boolean      ovflo;

  /* Finish off the array type by setting its size. */
  set_type_size(var->type);
  /* Put the ETS_LAST bit on in the last entry. */
  array_aggr_con = var->initializer.constant;
  aggr_con = array_aggr_con->variant.aggregate.last_constant;
  flags_con = aggr_con->variant.aggregate.last_constant;
  flags_value = value_of_integer_constant(flags_con, &ovflo);
  flags_value |= ETS_LAST;
  set_integer_value(&flags_con->variant.integer_value, flags_value);
}  /* finish_exception_type_specification_array */


static void push_eh_stack_frame(an_eh_stack_entry_kind kind,
                                a_variable_ptr         *stack_frame_var,
                                an_insert_location     *insert_location)
/*
Generate code to push an exception handling stack frame on the stack.
A local temporary variable is created to hold the stack frame; a pointer to
that variable is returned in *stack_frame_var.  The invariant fields of
the stack frame are set, and the kind is set to "kind".  The code is
inserted at the beginning of the current routine and *insert_location is
set to allow the caller to do insertion after the code inserted.  The
current context must be the function context.
*/
{
  a_variable_ptr   local_frame;
  a_statement_ptr  block;
  an_expr_node_ptr local_frame_next, local_frame_kind;

  /* Create the local variable for the stack frame. */
  *stack_frame_var = local_frame =
                            make_lowered_temporary(make_eh_stack_entry_type());
  /* Find the top block of the routine. */
  block = curr_context->scope->assoc_block;
  check_assertion(block != NULL);
  /* The insert location for the statements is the start of the top block of
     the routine. */
  set_block_start_insert_location(block, insert_location);
  /* Add code as follows:
       local_frame.next = __curr_eh_stack_entry;
       __curr_eh_stack_entry = &local_frame;
       local_frame.kind = kind;
  */
  local_frame_next = field_lvalue_selection_expr(var_lvalue_expr(local_frame),
                                                 ehse_next_field);
  insert_assignment_statement(local_frame_next,
                              (an_expr_operator_kind)eok_passign,
                              var_rvalue_expr(make_curr_eh_stack_entry_var()),
                              insert_location);
  insert_var_assignment_statement(curr_eh_stack_entry_var,
                                  (an_expr_operator_kind)eok_passign,
                                  var_lvalue_expr(local_frame),
                                  insert_location);
  local_frame_kind = field_lvalue_selection_expr(var_lvalue_expr(local_frame),
                                                 ehse_kind_field);
  insert_assignment_statement(local_frame_kind,
                              (an_expr_operator_kind)eok_iassign,
                              node_for_integer_constant((long)kind,
                                            (an_integer_kind)ik_unsigned_char),
                              insert_location);
}  /* push_eh_stack_frame */


static void pop_eh_stack_frame(a_variable_ptr     stack_frame_var,
                               an_insert_location *insert_location)
/*
Generate code to pop an exception handling stack frame on the stack.
*stack_frame_var points to a local temporary variable that holds the
stack frame.  The code is inserted at *insert_location.
*/
{
  an_expr_node_ptr local_frame_next;

  /* Add code as follows:
       __curr_eh_stack_entry = local_frame.next;
  */
  local_frame_next = field_lvalue_selection_expr(
                                              var_lvalue_expr(stack_frame_var),
                                              ehse_next_field);
  insert_var_assignment_statement(curr_eh_stack_entry_var,
                                  (an_expr_operator_kind)eok_passign,
                                  local_frame_next,
                                  insert_location);
}  /* pop_eh_stack_frame */


static a_variable_ptr exception_type_spec_array_from_throw_spec(
                                          a_throw_specification_ptr throw_spec)
/*
Make an array that describes the throw specification indicated by throw_spec,
and return a pointer to the variable for the array.  Return NULL if the
throw specification indicates that no types may be thrown.
*/
{
  a_variable_ptr         var;
  a_memory_region_number region_to_switch_back_to;
  a_throw_spec_type_ptr  throw_spec_type;

  throw_spec_type = throw_spec->throw_spec_type_list;
  /* If the routine can throw nothing, return NULL. */
  if (throw_spec_type == NULL) {
    var = NULL;
  } else {
    /* There are some types on the throw list, so an array of those will
       have to be built. */
    /* Switch to the file scope memory region so that initial values will
       be allocated there. */
    switch_to_file_scope_region(&region_to_switch_back_to);
    /* Make the variable. */
    var = make_exception_type_specification_array_var();
    /* Fill the array with entries for the types that can be thrown. */
    for (;
         throw_spec_type != NULL;
         throw_spec_type = throw_spec_type->next) {
      add_exception_type_specification_array_entry(throw_spec_type->type, var);
    }  /* for */
    /* Finish off the array. */
    finish_exception_type_specification_array(var);
    /* Return to the memory region that was current when this routine was
       entered. */
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
  return var;
}  /* exception_type_spec_array_from_throw_spec */


void add_eh_function_prologue(a_scope_ptr scope)
/*
Add any prologue needed for exception handling to the function whose scope
is given by "scope".
*/
{
  a_routine_ptr             routine;
  a_type_ptr                routine_type, spec_array_ptr;
  a_throw_specification_ptr throw_spec;
  a_variable_ptr            local_frame, spec_array_var;
  an_expr_node_ptr          spec_array_node, local_frame_variant_throw_spec;
  an_insert_location        insert_location;
  a_boolean                 need_throw_epilogue = FALSE;
  a_return_memo_ptr         rmp;

  /* Only add the code if exceptions are enabled. */
  if (exceptions_enabled) {
    /* See if the routine has a throw specification. */
    routine = scope->variant.routine.ptr;
    routine_type = routine->type;
    routine_type = skip_typerefs(routine_type);
    throw_spec = routine_type->variant.routine.extra_info->throw_specification;
    if (throw_spec != NULL) {
      /* The routine has a throw specification.  (A null pointer means
         the function can throw anything.) */
      /* Generate code to push an entry on the EH stack. */
      push_eh_stack_frame(ehsek_throw_spec, &local_frame, &insert_location);
      need_throw_epilogue = TRUE;
      /* Build an array of the throw types. */
      spec_array_var = exception_type_spec_array_from_throw_spec(throw_spec);
      /* Generate code to set the throw_spec field of the stack entry to
         point to the array (or NULL if no types can be thrown). */
      spec_array_ptr = make_pointer_type(
                                     make_exception_type_specification_type());
      if (spec_array_var == NULL) {
        /* No types can be thrown, so use a NULL pointer. */
        a_constant null_constant;
        make_zero_of_proper_type(spec_array_ptr,
                                 &null_constant);
        spec_array_node = alloc_node_for_constant(&null_constant);
      } else {
        /* Use the address of the first element of the array. */
        spec_array_node = var_lvalue_expr(spec_array_var);
        spec_array_node = add_cast(spec_array_node, spec_array_ptr);
      }  /* if */
      /* Make an expression for local_frame.variant.throw_spec */
      local_frame_variant_throw_spec = 
                   field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(local_frame),
                                                  ehse_variant_field),
                      ehse_throw_spec_field);
      /* Assign the array address to local_frame.variant.throw_spec */
      insert_assignment_statement(local_frame_variant_throw_spec,
                                  (an_expr_operator_kind)eok_passign,
                                  spec_array_node,
                                  &insert_location);
    }  /* if */
    if (need_throw_epilogue) {
      /* Need to add epilogue code at each return in the routine. */
      for (rmp = return_memo_list; rmp != NULL; rmp = rmp->next) {
        /* Turn the return into a block. */
        turn_branch_into_block(rmp->stmt, &insert_location, &rmp->stmt);
        if (need_throw_epilogue) {
          /* Insert code to pop the prologue pushed for the throw
             specification. */
          pop_eh_stack_frame(local_frame, &insert_location);
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* add_eh_function_prologue */


void il_eh_lower_init(void)
/*
Initialize static variables related to IL lowering of exceptions.
This is done as a subroutine (rather than relying on static initialization)
so that it can be redone to compile more than one source file in a single
invocation of the front end.
*/
{
  /* Static variables in lower_eh.c: */
  throw_alloc_routine = NULL;
  throw_routine = NULL;
  rethrow_routine = NULL;
  typeinfo_type = NULL;
  num_of_pending_class_typeinfo_vars = 0;
  jmp_buf_type = NULL;
  exception_type_specification_type = NULL;
  eh_region_descr_type = NULL;
  eh_stack_entry_type = NULL;
  eh_curr_region_var = NULL;
  curr_eh_stack_entry_var = NULL;
}  /* il_lower_init */

#endif /* DO_IL_LOWERING */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-1992 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
