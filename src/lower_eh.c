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
#include "folding.h"


static unsigned long
		next_region_number;
			/* Next available destructible object region number
			   within the current function. */

static unsigned long
		max_region_number;
			/* NULL_EH_REGION_NUMBER (all 1 bits) truncated to fit
			   in a TARG_REGION_NUMBER_INT_KIND integer. */

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
type information.  Typerefs on the type are stripped off.
*/
{
  a_variable_ptr  typeinfo_var;
  char            *mangled_name;
  sizeof_t        mangled_name_length, alloc_length;
  a_storage_class storage_class;

  type = skip_typerefs(type);
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


static a_variable_ptr make_unnamed_local_static_array_var(a_type_ptr elem_type)
/*
Create an unnamed local static variable whose type is an array of elem_type,
and return a pointer to the variable.  The array size is begun as [0] and
will be adjusted as elements are added.  finish_unnamed_local_static_array_var
must be called sometime later to set the size on the type.
*/
{
  a_variable_ptr var;
  a_type_ptr     array_type;

  /* The current region is already the file scope memory region when
     this routine is called. */
  check_assertion(curr_il_region_number == FILE_SCOPE_REGION_NUMBER);
  /* Make a type that is an array of elem_type. */
  array_type = alloc_type((a_type_kind)tk_array);
  array_type->variant.array.variant.number_of_elements = 0; /* Initially. */
  array_type->variant.array.element_type = elem_type;
  /* set_type_size is not called yet. */
  /* Make the variable.  It is unnamed and static. */
  var = make_unnamed_local_static_variable(array_type);
  return var;
}  /* make_unnamed_local_static_array_var */


static a_variable_ptr make_init_unnamed_local_static_array_var(
                                                          a_type_ptr elem_type)
/*
Create an unnamed local static variable whose type is an array of elem_type,
and return a pointer to the variable.  The array size is begun as [0] and
will be adjusted as elements are added.  finish_unnamed_local_static_array_var
must be called sometime later to set the size on the type.  The variable
will be initialized; to start the process, an aggregate constant is attached
to the variable.  Initial values must be added under the aggregate.
*/
{
  a_variable_ptr var;
  a_constant_ptr aggr_con;

  /* The current region is already the file scope memory region when
     this routine is called. */
  /* Make the variable with an array type. */
  var = make_unnamed_local_static_array_var(elem_type);
  /* The initial value is an aggregate constant pointing to a list of
     aggregate constants. */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  /* Attach the aggregate constant as the initial value of the variable. */
  var->init_kind = (an_init_kind)initk_static;
  var->initializer.constant = aggr_con;
  return var;
}  /* make_init_unnamed_local_static_array_var */


static a_targ_size_t incr_nelems_or_array_var(a_variable_ptr var)
/*
Increment the number of elements of the array variable pointed to by var.
Return the pre-incremented value (which is right as a subscript).
*/
{
  /* Add one to the array size.  set_type_size is called later. */
  return var->type->variant.array.variant.number_of_elements++;
}  /* incr_nelems_or_array_var */


static a_targ_size_t add_elem_to_array_var(a_variable_ptr var,
                                           a_constant_ptr con)
/*
Add the indicated constant as an initializer of an element of the array
variable pointed to by var.  Increment the number of elements of the
array.  Return the pre-incremented size (which is right as a subscript).
*/
{
  a_constant_ptr aggr_con = var->initializer.constant;

  /* Add the constant to the aggregate initializer list. */
  if (aggr_con->variant.aggregate.first_constant == NULL) {
    aggr_con->variant.aggregate.first_constant = con;
  } else {
    aggr_con->variant.aggregate.last_constant->next = con;
  }  /* if */
  aggr_con->variant.aggregate.last_constant = con;
  /* Increment the number of elements in the array. */
  return incr_nelems_or_array_var(var);
}  /* add_elem_to_array_var */


static void finish_array_var(a_variable_ptr var)
/*
var is an array variable (for example, one created by
make_unnamed_local_static_array_var).  The building of the variable is now
completed, so finish it off.  In particular, the array size is now known,
so call set_type_size on the type.
*/
{
  /* Finish off the array type by setting its size. */
  set_type_size(var->type);
}  /* finish_array_var */


/*
Pointer to the base_class_spec struct type (used to represent a base class
for exception throw and catch specifications).  NULL until created.
*/
static a_type_ptr
		base_class_spec_type;

/*
Bit set values for the flags byte of base_class_spec.  These must
match the runtime's definition.
*/
#define BCS_VIRTUAL		0x01
			/* TRUE if the offset gives the position of a
			   pointer to the (virtual) base class rather than
			   the offset to the base class itself. */
#define BCS_AMBIGUOUS		0x02
			/* TRUE if the base class is ambiguous. */
#define BCS_LAST		0x04
			/* TRUE if this is the last base class specification
			   in the array. */


static a_type_ptr make_base_class_spec_type(void)
/*
Make the base_class_spec struct type (used to represent a base class
for exception throw and catch specifications) if it is not made already,
and return a pointer to it.  Its definition is

  struct base_class_spec {
    typeinfo      *tinfo;  // typeinfo for base class
    short         offset;  // Offset of base class in derived class
    unsigned char flags;   // Flags
  };

*/
{
  a_targ_size_t byte_offset;
  a_field_ptr   last_field;

  if (base_class_spec_type == NULL) {
    /* Make the struct type. */
    base_class_spec_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(base_class_spec_type);
    byte_offset = 0;
    last_field = NULL;
    /* field: typeinfo *tinfo */
    make_lowered_field("tinfo", make_pointer_type(make_typeinfo_type()),
                       &byte_offset, base_class_spec_type, &last_field);
    /* field: short offset */
    make_lowered_field("offset", integer_type(TARG_DELTA_INT_KIND),
                       &byte_offset, base_class_spec_type, &last_field);
    /* field: unsigned char flags */
    make_lowered_field("flags",
                       integer_type((an_integer_kind)ik_unsigned_char),
                       &byte_offset, base_class_spec_type, &last_field);
    finish_class_type(base_class_spec_type, &byte_offset);
  }  /* if */
  return base_class_spec_type;
}  /* make_base_class_spec_type */


static a_variable_ptr make_base_class_array_var(a_type_ptr type)
/*
type is a class type that has base classes.  Make a variable initialized
with an array of base_class_spec entries for the base classes of the type.
This is used as part of the typeinfo information.
*/
{
  a_type_ptr       array_type;
  a_base_class_ptr bcp;
  a_constant_ptr   aggr_con;
  a_variable_ptr   typeinfo_var, bc_var;
  a_boolean        ovflo;
  a_constant_ptr   typeinfo_con, offset_con, flags_con;
  unsigned long    flags_value;
  a_targ_size_t    offset;

  /* The current region is already the file scope memory region when
     this routine is called. */
  /* Make an initialized static variable that is an array of base_class_spec
     structures. */
  /* make_init_unnamed_local_static_array_var cannot be used because we
     want the variable always to be in the file scope. */
  /* Make the array type. */
  array_type = alloc_type((a_type_kind)tk_array);
  array_type->variant.array.variant.number_of_elements = 0; /* Initially. */
  array_type->variant.array.element_type = make_base_class_spec_type();
  /* set_type_size is not called yet. */
  /* Make the variable.  It is unnamed and static and in the file scope. */
  bc_var = make_file_scope_temporary(array_type);
  /* The initial value is an aggregate constant pointing to a list of
     aggregate constants. */
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  /* Attach the aggregate constant as the initial value of the variable. */
  bc_var->init_kind = (an_init_kind)initk_static;
  bc_var->initializer.constant = aggr_con;
#if CHECKING
  flags_con = NULL;
#endif /* CHECKING */
  /* The initial value is an aggregate constant pointing to a list of
     aggregate constants for base_class_spec structures. */
  for (bcp = type->variant.class_struct_union.extra_info->base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    /* Include information only on direct and virtual base classes. */
    if (bcp->direct || bcp->is_virtual) {
      /* The base class specification consists of three fields:
           1)  A pointer to the typeinfo variable for the base class.
           2)  The offset of the base class in the derived class.
           3)  A flags byte.
      */
      flags_value = 0;
      /* Make an address constant for a pointer to the base class typeinfo
         variable. */
      typeinfo_con = alloc_constant((a_constant_repr_kind)ck_address);
      typeinfo_var = bcp->type->typeinfo_var;
      check_assertion_str(typeinfo_var != NULL,
                          "make_base_class_array_var: NULL typeinfo var");
      set_variable_address_constant(typeinfo_var, typeinfo_con);
      /* Make the offset constant. */
      if (bcp->is_virtual) {
        /* Virtual base class.  The offset is to the pointer, and a flag in the
           flags byte indicates indirection. */
        offset = bcp->pointer_offset;
        flags_value |= BCS_VIRTUAL;
      } else {
        /* Non-virtual base class.  The offset is to the data. */
        offset = bcp->offset;
      }  /* if */
      offset_con = alloc_constant((a_constant_repr_kind)ck_integer);
      set_unsigned_integer_constant_with_overflow_check(offset_con,
                                                        (unsigned long)offset,
                                                        TARG_DELTA_INT_KIND);
      /* Make the flags constant. */
      /* If the base class is ambiguous, turn on the ambiguous bit. */
      if (bcp->ambiguous) flags_value |= BCS_AMBIGUOUS;
      flags_con = alloc_constant((a_constant_repr_kind)ck_integer);
      set_unsigned_integer_constant(flags_con, flags_value,
                                    (an_integer_kind)ik_unsigned_char);
      /* Link the constants together and make an aggregate constant. */
      typeinfo_con->next = offset_con;
      offset_con->next = flags_con;
      aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
      aggr_con->variant.aggregate.first_constant = typeinfo_con;
      aggr_con->variant.aggregate.last_constant = flags_con;
      /* Add the constant to the aggregate initializer list. */
      (void)add_elem_to_array_var(bc_var, aggr_con);
    }  /* if */
  }  /* for */
  /* Put the BCS_LAST bit on in the last entry. */
  check_assertion_str(flags_con != NULL,
                      "make_base_class_array_var: no base classes");
  flags_value = unsigned_value_of_integer_constant(flags_con, &ovflo);
  flags_value |= BCS_LAST;
  set_unsigned_integer_value(&flags_con->variant.integer_value, flags_value);
  /* Finish off the variable. */
  finish_array_var(bc_var);
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
    implicit_cast(dtor_con, curr_field_type);
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
    /* Make the type pointer-to-element instead of pointer-to-array. */
    implicit_cast(bc_con, curr_field_type);
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
Pointer to the exception_type_spec struct type (used to represent a type
for exception throw and catch specifications).  NULL until created.
*/
static a_type_ptr
		exception_type_spec_type;

/*
Bit set values for the flags byte of exception_type_spec.  These must
match the runtime's definition.
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


static a_type_ptr make_exception_type_spec_type(void)
/*
Make the exception_type_spec struct type (used to represent a type
for exception throw and catch specifications) if it is not made already,
and return a pointer to it.  Its definition is

  struct exception_type_spec {
    typeinfo      *tinfo;
    unsigned char flags;
  };

*/
{
  a_targ_size_t byte_offset;
  a_field_ptr   last_field;

  if (exception_type_spec_type == NULL) {
    /* Make the struct type. */
    exception_type_spec_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(exception_type_spec_type);
    byte_offset = 0;
    last_field = NULL;
    /* field: typeinfo *tinfo */
    make_lowered_field("tinfo", make_pointer_type(make_typeinfo_type()),
                       &byte_offset, exception_type_spec_type, &last_field);
    /* field: unsigned char flags */
    make_lowered_field("flags",
                       integer_type((an_integer_kind)ik_unsigned_char),
                       &byte_offset, exception_type_spec_type, &last_field);
    finish_class_type(exception_type_spec_type, &byte_offset);
  }  /* if */
  return exception_type_spec_type;
}  /* make_exception_type_spec_type */


static a_variable_ptr typeinfo_var_for_type(a_type_ptr    type,
                                            unsigned long *flags_value)
/*
Create the typeinfo variable for the indicated type, and return a pointer
to it.  Type qualifiers on the type are dropped.  For a pointer or reference
to a class type, make the typeinfo variable for the underlying class and
set *flags_value to indicate a pointer or reference.
*/
{
  a_variable_ptr typeinfo_var;
  a_type_ptr     typeinfo_type;

  typeinfo_type = type;
  *flags_value = 0;
  if (is_ptr_or_ref_type(type)) {
    /* For a pointer or reference to a class type, use the typeinfo for the
       class. */
    a_type_ptr base_type = type_pointed_to(type);
    if (is_class_struct_union_type(base_type)) {
      typeinfo_type = base_type;
      *flags_value = is_pointer_type(type) ? ETS_IS_POINTER : ETS_IS_REFERENCE;
    }  /* if */
  }  /* if */
  typeinfo_var = make_typeinfo_var(typeinfo_type);
  return typeinfo_var;
}  /* typeinfo_var_for_type */


/*
Pointer to the region_descr struct type (used to represent a cleanup
region for exception processing).  NULL until created.
*/
static a_type_ptr
		region_descr_type;


/*
Bit flags for the flags field of a region description.  These must match
the runtime's definition.
*/
#define RDF_INDIRECT		0x01
			/* TRUE if the address provided by the handle field
			   is a pointer to the object. */
#define RDF_CONDITIONAL_FLAG	0x02
			/* TRUE if the object has an associated flag that
			   indicates whether the construction has occurred.
			   The region entry following this one gives the
			   location of the flag. */
#define RDF_NEW_ALLOCATION	0x04
			/* TRUE if the object was allocated by new and
			   is to be freed in the event of a throw. */
#define RDF_ARRAY		0x08
			/* TRUE if the object is an array (or requires
			   information normally provided only for arrays). */
#define RDF_BASED_ON_THIS	0x10
			/* TRUE if the object is part of the object pointed
			   to by the "this" parameter. */


static a_type_ptr make_region_descr_type(void)
/*
Make the region_descr struct type (used to represent the cleanup required
in a particular region for exception processing) if it is not made already,
and return a pointer to it.  Its definition is

  struct region_descr {
    __vptp         dtor;    // Destructor or delete routine pointer
    unsigned short handle;  // Index of object in object address table
    unsigned short prev;    // Previous cleanup region
    unsigned char  flags;   // Bit flags
  };

*/
{
  a_targ_size_t byte_offset;
  a_field_ptr   last_field;

  if (region_descr_type == NULL) {
    /* Make the struct type. */
    region_descr_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(region_descr_type);
    byte_offset = 0;
    last_field = NULL;
    /* field: __vptp dtor */
    make_lowered_field("dtor", make_vptp_type(), &byte_offset,
                       region_descr_type, &last_field);
    /* field: unsigned short handle */
    make_lowered_field("handle",
                       integer_type(TARG_VAR_HANDLE_INT_KIND),
                       &byte_offset, region_descr_type, &last_field);
    /* field: unsigned short prev */
    make_lowered_field("prev",
                       integer_type(TARG_REGION_NUMBER_INT_KIND),
                       &byte_offset, region_descr_type, &last_field);
    /* field: unsigned char flags */
    make_lowered_field("flags",
                       integer_type((an_integer_kind)ik_unsigned_char),
                       &byte_offset, region_descr_type, &last_field);
    finish_class_type(region_descr_type, &byte_offset);
  }  /* if */
  return region_descr_type;
}  /* make_region_descr_type */


/*
Pointer to the array_descr struct type (used as a supplement to
the region description entry to represent an array object for exception
processing).  NULL until created.
*/
static a_type_ptr
		array_descr_type;

static a_type_ptr make_array_descr_type(void)
/*
Make the array_descr struct type (used as a supplement to the region
description entry to represent an array object for exception processing)
if it is not made already, and return a pointer to it.  Its definition is

  struct array_descr {
    unsigned short handle;     // Index of object in object address table
    size_t         elem_size;  // Array element size
    long           elem_count; // Element count
  };

*/
{
  a_targ_size_t byte_offset;
  a_field_ptr   last_field;

  if (array_descr_type == NULL) {
    /* Make the struct type. */
    array_descr_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(array_descr_type);
    byte_offset = 0;
    last_field = NULL;
    /* field: unsigned short handle */
    make_lowered_field("handle",
                       integer_type(TARG_VAR_HANDLE_INT_KIND),
                       &byte_offset, array_descr_type, &last_field);
    /* field: size_t elem_size */
    make_lowered_field("elem_size",
                       integer_type(TARG_SIZE_T_INT_KIND),
                       &byte_offset, array_descr_type, &last_field);
    /* field: long elem_count */
    make_lowered_field("elem_count",
                       integer_type((an_integer_kind)ik_long),
                       &byte_offset, array_descr_type, &last_field);
    finish_class_type(array_descr_type, &byte_offset);
  }  /* if */
  return array_descr_type;
}  /* make_array_descr_type */


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
		ehse_function_array_table_field,
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
        exception_type_spec *catch_entries;  // Catch list
      } try_block;
      struct {
        region_descr   *regions;            // Cleanup regions
        void           **obj_table;         // Object address table
        array_descr    *array_table;        // Array table
        unsigned short saved_region_number; // Saved __eh_curr_region
      } function;
      exception_type_spec *throw_spec; // Throw spec list
    } variant;
  };

*/
{
  a_targ_size_t byte_offset;
  a_field_ptr   last_field;
  a_type_ptr    try_block_struct_type, function_struct_type;
  a_type_ptr    variant_union_type, ptr_exception_type_spec;

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
    /* field: exception_type_spec *catch_entries */
    ptr_exception_type_spec =
                            make_pointer_type(make_exception_type_spec_type());
    make_lowered_field("catch_entries", ptr_exception_type_spec,
                       &byte_offset, try_block_struct_type, &last_field);
    ehse_try_catch_entries_field = last_field;
    finish_class_type(try_block_struct_type, &byte_offset);
    /* Make the function variant struct. */
    function_struct_type = alloc_type((a_type_kind)tk_struct);
    add_to_front_of_file_scope_types_list(function_struct_type);
    byte_offset = 0;
    last_field = NULL;
    /* field: region_descr *regions */
    make_lowered_field("regions",
                       make_pointer_type(make_region_descr_type()),
                       &byte_offset, function_struct_type, &last_field);
    ehse_function_regions_field = last_field;
    /* field: void **obj_table */
    make_lowered_field("obj_table",
                       make_pointer_type(void_star_type()),
                       &byte_offset, function_struct_type, &last_field);
    ehse_function_obj_table_field = last_field;
    /* field: array_descr *array_table */
    make_lowered_field("array_table",
                       make_pointer_type(make_array_descr_type()),
                       &byte_offset, function_struct_type, &last_field);
    ehse_function_array_table_field = last_field;
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
    /* field: exception_type_spec *throw_spec */
    make_lowered_field("throw_spec", ptr_exception_type_spec,
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
  long flags;

  /* We need a typeinfo variable for the underlying type.  Make it if it
     does not exist already. */
  (void)typeinfo_var_for_type(type, &flags);
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
  unsigned long      flags_value;                
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
                                          TARG_SIZE_T_INT_KIND);
    typeinfo_node->next = size_node;
    flags_node = node_for_integer_constant((long)flags_value,
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
Pointer to the variable entry for the object address array table of
a function.  NULL until allocated.
*/
static a_variable_ptr
		object_addr_table_var;

static a_targ_size_t object_addr_table_entry(
                                        an_init_pos_descr_ptr ipdp,
                                        an_insert_location    *insert_location)
/*
Add an entry to the object address table array (creating the table and its
associated variable if necessary) for the object identified by ipdp.
Return the index number into the object address table.  Also insert
(at *insert_location) an assignment statement to set the entry of the
object address array to the address of the object.
*/
{
  an_expr_node_ptr object_addr_table_node, subsc_node;
  a_targ_size_t    entry_number;

  /* Note that the current memory region must not have been forced to the
     file scope memory region at this point. */
  /* Make the variable if it has not yet been made. */
  if (object_addr_table_var == NULL) {
    /* Switch to the file scope memory region so the variable will
       be allocated there. */
    a_memory_region_number region_to_switch_back_to;
    switch_to_file_scope_region(&region_to_switch_back_to);
    /* The variable is an array whose elements have type "void *". */
    object_addr_table_var =
                         make_unnamed_local_static_array_var(void_star_type());
    /* Return to the memory region that was current when this routine was
       entered. */
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
  /* Add an element to the object address table array. */
  entry_number = incr_nelems_or_array_var(object_addr_table_var);
  /* Insert code to initialize the element of the table to the address of the
     object, i.e.,
       object_addr_table[n] = ipdp-address;
  */
  object_addr_table_node = array_var_lvalue_expr(object_addr_table_var);
  object_addr_table_node->next = node_for_integer_constant((long)entry_number,
                                                         TARG_SIZE_T_INT_KIND);
  subsc_node = make_operator_node((an_expr_operator_kind)eok_padd_subsc,
                                  object_addr_table_node->type,
                                  object_addr_table_node);
  (void)insert_assignment_statement(subsc_node,
                                    (an_expr_operator_kind)eok_passign,
                                    make_init_entity_node(ipdp),
                                    insert_location);
  return entry_number;
}  /* object_addr_table_entry */


/*
Pointer to the variable entry for the array table of a function.  NULL
until allocated.
*/
static a_variable_ptr
		array_table_var;

static a_targ_size_t array_table_entry(
                               a_required_destructor_call_ptr rdcp,
                               an_insert_location             *insert_location)
/*
Add an entry to the array table (creating the table and its associated
variable if necessary) for the object described in rdcp.  Return the
index number into the array table.  Also insert (at *insert_location)
initialization code for the proper entry in the object address table.
This routine can also be called for non-arrays in cases where an array
table entry is needed to provide information not include in the region
description entry (for example, for a new-allocation record in a case
where the delete routine requires a second parameter giving the size;
the size is not available in the region description entry).
*/
{
  a_targ_size_t    object_addr_index, entry_number;
  long             elem_count;
  a_memory_region_number
                   region_to_switch_back_to;
  a_constant_ptr   index_con, elem_size_con, size_con, aggr_con;
  a_type_ptr       elem_type;

  /* Note that the current memory region must not have been forced to the
     file scope memory region at this point. */
  /* Allocate the proper entry in the object address table. */
  object_addr_index = object_addr_table_entry(&rdcp->init_pos_descr,
                                              insert_location);
  /* Switch to the file scope memory region so the variable and initialization
     constants will be allocated there. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Make the variable if it has not yet been made. */
  if (array_table_var == NULL) {
    /* The variable is an array whose elements have type array_descr. */
    array_table_var =
             make_init_unnamed_local_static_array_var(make_array_descr_type());
  }  /* if */
  /* Make the aggregate constant for the entry in the array table.  It consists
     of the index in the object address table, the size of each element, and
     the number of elements. */
  index_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant_with_overflow_check(index_con,
                                                    object_addr_index,
                                                    TARG_VAR_HANDLE_INT_KIND);
  /* For the element size: note that the init_pos_descr has the type of an
     element, not of the whole array.  For non-arrays, the type is of
     course as expected. */
  elem_type = type_from_init_pos_descr(&rdcp->init_pos_descr);
  elem_type = skip_typerefs(elem_type);
  elem_size_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant(elem_size_con, (unsigned long)elem_type->size,
                                TARG_SIZE_T_INT_KIND);
  size_con = alloc_constant((a_constant_repr_kind)ck_integer);
  if (rdcp->init_pos_descr.whole_array) {
    /* The entity really is an array.  Get the element count.  -1 indicates
       that the runtime should look up the number of elements in the array. */
    elem_count = rdcp->init_pos_descr.array_element_count;
  } else {
    /* Not an array (see header comment above).  Use an element count of 0. */
    elem_count = 0;
  }  /* if */
  set_integer_constant(size_con, elem_count, (an_integer_kind)ik_long);
  /* Link the constants together and make an aggregate constant. */
  index_con->next = elem_size_con;
  elem_size_con->next = size_con;
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->variant.aggregate.first_constant = index_con;
  aggr_con->variant.aggregate.last_constant = size_con;
  /* Add the aggregate as an element of the object address table array. */
  entry_number = add_elem_to_array_var(array_table_var, aggr_con);
  /* Return to the memory region that was current when this routine was
     entered. */
  switch_back_to_original_region(region_to_switch_back_to);
  return entry_number;
}  /* array_table_entry */


/*
Variable entries for __eh_curr_region, __curr_eh_stack_entry, and
__catch_clause_number, global variables used for exception processing.
NULL until created.
*/
static a_variable_ptr
		eh_curr_region_var,
		curr_eh_stack_entry_var,
		catch_clause_number_var;

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


static a_variable_ptr make_catch_clause_number_var(void)
/*
Make __catch_clause_number, a global variable used for exception processing,
if it has not already been made.  Return a pointer to it.
*/
{
  if (catch_clause_number_var == NULL) {
    catch_clause_number_var =
               make_lowered_variable("__catch_clause_number",
                                     /*already_il_name=*/FALSE,
                                     integer_type((an_integer_kind)ik_int),
                                     (a_storage_class)sc_extern);
  }  /* if */
  return catch_clause_number_var;
}  /* make_catch_clause_number_var */


/*
Pointer to the variable entry for the region table of a function (which
contains information about destructible objects).  NULL until allocated.
*/
static a_variable_ptr
		region_table_var;

void make_region_table_entry(a_required_destructor_call_ptr rdcp,
                             an_insert_location             *insert_location)
/*
Add an entry to the region table (which describes destructible objects)
for the object described in rdcp.  Create the region table variable if
necessary.  Also insert (at *insert_location) initialization code for
the proper entry in the object address table and code to set
eh_curr_region to the region number for the region created.  rdcp must
already be linked on the list of required destructors so its "next"
pointer can be examined.
*/
{
  a_targ_size_t    handle_number;
  a_memory_region_number
                   region_to_switch_back_to;
  a_constant_ptr   dtor_con, handle_con, prev_con, flags_con, aggr_con;
  a_type_ptr       ptr_func_type;
  unsigned long    flags_value = 0, prev_region_number;
  a_routine_ptr    dtor_routine;
  a_required_destructor_call_ptr
                   next_rdcp;

  /* Note that the current memory region must not have been forced to the
     file scope memory region at this point. */
  if (rdcp->init_pos_descr.whole_array) {
    /* For arrays, we need an entry in the array table. */
    handle_number = array_table_entry(rdcp, insert_location);
    /* Set the flag that indicates this object is an array. */
    flags_value |= RDF_ARRAY;
  } else {
    /* Non-array. */
    /* Allocate the proper entry in the object address table. */
    handle_number = object_addr_table_entry(&rdcp->init_pos_descr,
                                            insert_location);
  }  /* if */
  /* Assign a region number to this entry. */
  rdcp->region_number = next_region_number++;
  /* Insert an assignment statement that sets the global variable
     eh_curr_region to the region number for this entry. */
  (void)insert_var_assignment_statement(
                                  make_eh_curr_region_var(),
                                  (an_expr_operator_kind)eok_iassign,
                                  node_for_integer_constant(
                                                  (long)rdcp->region_number,
                                                  TARG_REGION_NUMBER_INT_KIND),
                                  insert_location);
  /* Switch to the file scope memory region so the variable and initialization
     constants will be allocated there. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Make the variable if it has not yet been made. */
  if (region_table_var == NULL) {
    /* The variable is an array whose elements have type array_descr. */
    region_table_var =
            make_init_unnamed_local_static_array_var(make_region_descr_type());
  }  /* if */
  /* Make the aggregate constant for the entry in the region description
     table.  It has a structure as follows:
       struct region_descr {
         __vptp         dtor;    // Destructor or delete routine pointer
         unsigned short handle;  // Index of object in object address table
         unsigned short prev;    // Previous cleanup region
         unsigned char  flags;   // Bit flags
       };
  */
  /* Make the destructor pointer. */
#if 0
  /* This needs to deal with delete routines too. */
#endif
  dtor_con = alloc_constant((a_constant_repr_kind)ck_address);
  dtor_routine = rdcp->dynamic_init.destructor;
  /* Create the generic function pointer type if it does not exist already. */
  ptr_func_type = make_vptp_type();
  if (dtor_routine == NULL) {
    /* The object has no destructor; use a NULL pointer. */
    make_zero_of_proper_type(ptr_func_type, dtor_con);
  } else {
    /* The class has a destructor.  Make a pointer to the routine. */
    set_routine_address_constant(dtor_routine, dtor_con);
    implicit_cast(dtor_con, ptr_func_type);
  }  /* if */
  /* Make the handle. */
  handle_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant_with_overflow_check(handle_con,
                                                    handle_number,
                                                    TARG_VAR_HANDLE_INT_KIND);
  /* Make the previous region index number. */
  /* Find the previous region by going backwards on the required destructor
     call list. */
  next_rdcp = rdcp->next;
  while (next_rdcp != NULL &&
         next_rdcp->region_number == NULL_EH_REGION_NUMBER) {
    /* Ignore entries with no associated region number. */
    next_rdcp = next_rdcp->next;
  }  /* while */
  if (next_rdcp != NULL) {
    /* There is a previous entry. */
    prev_region_number = next_rdcp->region_number;
    if (prev_region_number >= max_region_number) {
      /* The region number is too big. */
      error(ec_integer_truncated);
      prev_region_number = 0;
    }  /* if */
  } else {
    /* There is no previous region.  Use a code (all 1 bits) that indicates
       that. */
    prev_region_number = max_region_number;
  }  /* if */
  prev_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant(prev_con, prev_region_number,
                                TARG_REGION_NUMBER_INT_KIND);
  /* Make the flags constant. */
  flags_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant(flags_con, flags_value,
                                (an_integer_kind)ik_unsigned_char);
  /* Link the constants together and make an aggregate constant. */
  dtor_con->next = handle_con;
  handle_con->next = prev_con;
  prev_con->next = flags_con;
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->variant.aggregate.first_constant = dtor_con;
  aggr_con->variant.aggregate.last_constant = flags_con;
  /* Add the aggregate as an element of the region table array. */
  (void)add_elem_to_array_var(region_table_var, aggr_con);
  /* Return to the memory region that was current when this routine was
     entered. */
  switch_back_to_original_region(region_to_switch_back_to);
}  /* region_table_entry */


void set_eh_curr_region(a_context_ptr      context,
                        an_insert_location *insert_location)
/*
Generate code at *insert_location to set the global variable eh_curr_region
to indicate the destruction region that applies to the last required
destructor call on the list attached to the indicated context.  If there
are no required destructor calls in that context, set eh_curr_region to
NULL_EH_REGION_NUMBER.
*/
{
  a_required_destructor_call_ptr rdcp;
  long                           region_number;

  /* See if there is a required destructor entry. */
  rdcp = context->required_destructor_calls;
  while (rdcp != NULL && rdcp->region_number == NULL_EH_REGION_NUMBER) {
    /* Ignore entries that are not regions. */
    rdcp = rdcp->next;
  }  /* while */
  /* Determine the region number to be used. */
  if (rdcp != NULL) {
    region_number = (long)rdcp->region_number;
  } else {
    /* Use the maximum region number (all 1 bits) to indicate no region. */
    region_number = (long)max_region_number;
  }  /* if */
  /* Generate an assignment statement to set curr_eh_region. */
  (void)insert_var_assignment_statement(
                                  make_eh_curr_region_var(),
                                  (an_expr_operator_kind)eok_iassign,
                                  node_for_integer_constant(
                                                  region_number,
                                                  TARG_REGION_NUMBER_INT_KIND),
                                  insert_location);
}  /* set_eh_curr_region */


static a_variable_ptr make_exception_type_spec_array_var(void)
/*
Create a variable whose initial value will be an array of exception type
specification entries, and return a pointer to the variable.
*/
{
  a_variable_ptr var;

  /* The current region is already the file scope memory region when
     this routine is called. */
  /* Make a variable that is an array of exception type specification
     entries. */
  var = make_init_unnamed_local_static_array_var(
                                              make_exception_type_spec_type());
  return var;
}  /* make_exception_type_spec_array_var */


static void add_exception_type_spec_array_entry(a_type_ptr     type,
                                                a_variable_ptr var)
/*
Add an entry that describes the type "type" to the array of exception type
specifications being built up as the initializer of the variable "var".
If type is NULL, add an ellipsis entry.
*/
{
  a_variable_ptr typeinfo_var;
  unsigned long  flags_value;
  a_constant_ptr typeinfo_con, flags_con, aggr_con;

  /* The current region is already the file scope memory region when
     this routine is called. */
  /* Each element of the array is an exception_type_spec struct
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
    flags_value = ETS_IS_ELLIPSIS;
  } else {
    /* Normal case. */
    typeinfo_var = typeinfo_var_for_type(type, &flags_value);
    set_variable_address_constant(typeinfo_var, typeinfo_con);
  }  /* if */
  flags_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant(flags_con, flags_value,
                                (an_integer_kind)ik_unsigned_char);
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->variant.aggregate.first_constant = typeinfo_con;
  typeinfo_con->next = flags_con;
  aggr_con->variant.aggregate.last_constant = flags_con;
  /* Add this aggregate constant to the list of constants under the aggregate
     constant for the array. */
  (void)add_elem_to_array_var(var, aggr_con);
}  /* add_exception_type_spec_array_entry */


static void finish_exception_type_spec_array(a_variable_ptr var)
/*
Finish the definition of a variable whose value is an array of exception
type specification entries.
*/
{
  a_constant_ptr array_aggr_con, aggr_con, flags_con;
  unsigned long  flags_value;
  a_boolean      ovflo;

  /* Put the ETS_LAST bit on in the last entry. */
  array_aggr_con = var->initializer.constant;
  aggr_con = array_aggr_con->variant.aggregate.last_constant;
  flags_con = aggr_con->variant.aggregate.last_constant;
  flags_value = unsigned_value_of_integer_constant(flags_con, &ovflo);
  flags_value |= ETS_LAST;
  set_unsigned_integer_value(&flags_con->variant.integer_value, flags_value);
  /* Finish off the variable. */
  finish_array_var(var);
}  /* finish_exception_type_spec_array */


static void push_eh_stack_frame(an_eh_stack_entry_kind kind,
                                a_variable_ptr         *stack_frame_var,
                                an_insert_location     *insert_location)
/*
Generate code to push an exception handling stack frame on the stack.
A local temporary variable is created to hold the stack frame; a pointer to
that variable is returned in *stack_frame_var.  The invariant fields of
the stack frame are set, and the kind is set to "kind".  The code is
inserted at *insert_location and *insert_location is updated to allow
the caller to do insertion after the code inserted.
*/
{
  a_variable_ptr   local_frame;
  an_expr_node_ptr local_frame_next, local_frame_kind;

  /* Create the local variable for the stack frame. */
  *stack_frame_var = local_frame =
                            make_lowered_temporary(make_eh_stack_entry_type());
  /* Add code as follows:
       local_frame.next = __curr_eh_stack_entry;
       __curr_eh_stack_entry = &local_frame;
       local_frame.kind = kind;
  */
  local_frame_next = field_lvalue_selection_expr(var_lvalue_expr(local_frame),
                                                 ehse_next_field);
  (void)insert_assignment_statement(
                              local_frame_next,
                              (an_expr_operator_kind)eok_passign,
                              var_rvalue_expr(make_curr_eh_stack_entry_var()),
                              insert_location);
  (void)insert_var_assignment_statement(curr_eh_stack_entry_var,
                                        (an_expr_operator_kind)eok_passign,
                                        var_lvalue_expr(local_frame),
                                        insert_location);
  local_frame_kind = field_lvalue_selection_expr(var_lvalue_expr(local_frame),
                                                 ehse_kind_field);
  (void)insert_assignment_statement(
                              local_frame_kind,
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
  local_frame_next = field_rvalue_selection_expr(
                                              var_lvalue_expr(stack_frame_var),
                                              ehse_next_field);
  (void)insert_var_assignment_statement(curr_eh_stack_entry_var,
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
    var = make_exception_type_spec_array_var();
    /* Fill the array with entries for the types that can be thrown. */
    for (;
         throw_spec_type != NULL;
         throw_spec_type = throw_spec_type->next) {
      add_exception_type_spec_array_entry(throw_spec_type->type, var);
    }  /* for */
    /* Finish off the array. */
    finish_exception_type_spec_array(var);
    /* Return to the memory region that was current when this routine was
       entered. */
    switch_back_to_original_region(region_to_switch_back_to);
  }  /* if */
  return var;
}  /* exception_type_spec_array_from_throw_spec */


void add_eh_function_prologue(a_scope_ptr scope)
/*
Add any prologue needed for exception handling to the function whose scope
is given by "scope".  Called only if exceptions are enabled.
*/
{
  a_routine_ptr             routine;
  a_type_ptr                routine_type, spec_array_ptr;
  a_throw_specification_ptr throw_spec;
  a_variable_ptr            throw_frame, func_frame, spec_array_var;
  an_expr_node_ptr          spec_array_node, throw_frame_throw_spec;
  an_expr_node_ptr          func_frame_function_regions;
  an_expr_node_ptr          func_frame_function_obj_table;
  an_expr_node_ptr          func_frame_function_array_table;
  an_expr_node_ptr          func_frame_function_saved_region_number;
  an_insert_location        insert_location;
  a_boolean                 need_throw_epilogue = FALSE;
  a_boolean                 need_function_epilogue = FALSE;
  a_return_memo_ptr         rmp;

  /* The insert location for the statements is the start of the top block of
     the routine. */
#if 0
  /* This needs to be adjusted (main, ctor, dtor). */
#endif
  set_block_start_insert_location(scope->assoc_block, &insert_location);
  /* See if the routine has a throw specification. */
  routine = scope->variant.routine.ptr;
  routine_type = routine->type;
  routine_type = skip_typerefs(routine_type);
  throw_spec = routine_type->variant.routine.extra_info->throw_specification;
  if (throw_spec != NULL) {
    /* The routine has a throw specification.  (A null pointer means
       the function can throw anything.) */
    /* Generate code to push an entry on the EH stack. */
    push_eh_stack_frame(ehsek_throw_spec, &throw_frame, &insert_location);
    need_throw_epilogue = TRUE;
    /* Build an array of the throw types. */
    spec_array_var = exception_type_spec_array_from_throw_spec(throw_spec);
    /* Generate code to set the throw_spec field of the stack entry to
       point to the array (or NULL if no types can be thrown). */
    spec_array_ptr = make_pointer_type(make_exception_type_spec_type());
    if (spec_array_var == NULL) {
      /* No types can be thrown, so use a NULL pointer. */
      a_constant null_constant;
      make_zero_of_proper_type(spec_array_ptr, &null_constant);
      spec_array_node = alloc_node_for_constant(&null_constant);
    } else {
      /* Use the address of the first element of the array. */
      spec_array_node = array_var_lvalue_expr(spec_array_var);
    }  /* if */
    /* Make an expression for throw_frame.variant.throw_spec */
    throw_frame_throw_spec = 
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(throw_frame),
                                                  ehse_variant_field),
                      ehse_throw_spec_field);
    /* Assign the array address to throw_frame.variant.throw_spec */
    (void)insert_assignment_statement(throw_frame_throw_spec,
                                      (an_expr_operator_kind)eok_passign,
                                      spec_array_node,
                                      &insert_location);
  }  /* if */
  if (region_table_var != NULL) {
    /* The function contains destructible objects, so we need to push a
       stack entry for the function itself. */
    /* Generate code to push an entry on the EH stack. */
    push_eh_stack_frame(ehsek_function, &func_frame, &insert_location);
    need_function_epilogue = TRUE;
    /* Finish off the various arrays and put pointers to them into the
       stack. */
    finish_array_var(region_table_var);
    /* Make an expression for throw_frame.variant.function.regions */
    func_frame_function_regions = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(func_frame),
                                                  ehse_variant_field),
                      ehse_function_field),
                    ehse_function_regions_field);
    /* Assign the region table address to
       func_frame.variant.function.regions */
    (void)insert_assignment_statement(func_frame_function_regions,
                                      (an_expr_operator_kind)eok_passign,
                                      array_var_lvalue_expr(region_table_var),
                                      &insert_location);
    if (object_addr_table_var != NULL) {
      finish_array_var(object_addr_table_var);
      /* Make an expression for throw_frame.variant.function.obj_table */
      func_frame_function_obj_table = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(func_frame),
                                                  ehse_variant_field),
                      ehse_function_field),
                    ehse_function_obj_table_field);
      /* Assign the object address table address to
         func_frame.variant.function.obj_table */
      (void)insert_assignment_statement(func_frame_function_obj_table,
                                        (an_expr_operator_kind)eok_passign,
                                        array_var_lvalue_expr(
                                                        object_addr_table_var),
                                        &insert_location);
    }  /* if */
    if (array_table_var != NULL) {
      finish_array_var(array_table_var);
      /* Make an expression for throw_frame.variant.function.array_table */
      func_frame_function_array_table = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(func_frame),
                                                  ehse_variant_field),
                      ehse_function_field),
                    ehse_function_array_table_field);
      /* Assign the object address table address to
         func_frame.variant.function.array_table */
      (void)insert_assignment_statement(func_frame_function_array_table,
                                        (an_expr_operator_kind)eok_passign,
                                        array_var_lvalue_expr(array_table_var),
                                        &insert_location);
    }  /* if */
    /* Generate an assignment to save eh_curr_region in the stack. */
    /* Make an expression for
       throw_frame.variant.function.saved_region_number */
    func_frame_function_saved_region_number = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(func_frame),
                                                  ehse_variant_field),
                      ehse_function_field),
                    ehse_function_saved_region_number_field);
    /* Copy the global variable eh_curr_region into
       func_frame.variant.function.saved_region_number */
    (void)insert_assignment_statement(func_frame_function_saved_region_number,
                                      (an_expr_operator_kind)eok_iassign,
                                      var_rvalue_expr(
                                                    make_eh_curr_region_var()),
                                      &insert_location);
    /* Reset eh_curr_region_var to max_region_number (all 1 bits). */
    (void)insert_var_assignment_statement(eh_curr_region_var,
                                          (an_expr_operator_kind)eok_iassign,
                                          node_for_integer_constant(
                                                 (long)max_region_number,
                                                 TARG_REGION_NUMBER_INT_KIND),
                                          &insert_location);
  }  /* if */
  if (need_throw_epilogue || need_function_epilogue) {
    /* Need to add epilogue code at each return in the routine. */
    for (rmp = return_memo_list; rmp != NULL; rmp = rmp->next) {
      /* Turn the return into a block. */
      turn_branch_into_block(rmp->stmt, &insert_location, &rmp->stmt);
      if (need_function_epilogue) {
        /* Insert code to pop the prologue pushed for the function. */
        pop_eh_stack_frame(func_frame, &insert_location);
      }  /* if */
      if (need_throw_epilogue) {
        /* Insert code to pop the prologue pushed for the throw
           specification. */
        pop_eh_stack_frame(throw_frame, &insert_location);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* add_eh_function_prologue */


static a_variable_ptr make_catch_array_var(a_handler_ptr handlers)
/*
Generate an exception type specification array to describe the types of the
catch clauses on the indicated list.  Return a pointer to the variable.
*/
{
  a_variable_ptr var;
  a_handler_ptr  handler;
  a_memory_region_number
                 region_to_switch_back_to;

  /* Switch to the file scope memory region so that initial values will
     be allocated there. */
  switch_to_file_scope_region(&region_to_switch_back_to);
  /* Make the variable. */
  var = make_exception_type_spec_array_var();
  /* Fill the array with entries for the catch clause types. */
  for (handler = handlers;
       handler != NULL;
       handler = handler->next) {
    a_type_ptr handler_type;
    if (handler->parameter == NULL) {
      /* NULL means an ellipsis catch, one that catches any type. */
      handler_type = NULL;
    } else {
      handler_type = handler->parameter->type;
    }  /* if */
    add_exception_type_spec_array_entry(handler_type, var);
  }  /* for */
  /* Finish off the array. */
  finish_exception_type_spec_array(var);
  /* Return to the memory region that was current when this routine was
     entered. */
  switch_back_to_original_region(region_to_switch_back_to);
  return var;
}  /* make_catch_array_var */


/*
Pointer to the routine entry for the runtime routine setjmp.  NULL until
created.
*/
static a_routine_ptr
		setjmp_routine;


void lower_try_block(a_statement_ptr statement)
/*
Do IL lowering for an stmk_try_block statement.
*/
{
  a_handler_ptr      handlers, handler;
  a_variable_ptr     try_frame, catch_array_var;
  an_insert_location insert_location;
  a_statement_ptr    stmt_to_try, copy_of_orig_stmt;
  an_expr_node_ptr   try_frame_catch_entries, try_frame_setjmp_buffer;
  an_expr_node_ptr   setjmp_call, compare_node, catch_clause_number_node;
  a_statement_ptr    prev_if_stmt, if_stmt;
  long               catch_clause_number;

  stmt_to_try = statement->variant.try_block.statement;
  handlers = statement->variant.try_block.handlers;
  /* Lower the dependent statement of the try. */
  lower_statement(stmt_to_try);
  /* Change the stmk_try_block statement into a block, and prepare to insert
     code at the start of the block. */
  turn_statement_into_block(statement);
  set_block_start_insert_location(statement, &insert_location);
  copy_of_orig_stmt = statement->variant.block.statements;
  /* Generate code to push a stack frame. */
  push_eh_stack_frame(ehsek_try_block, &try_frame, &insert_location);
  /* Generate a description of the catch clause types. */
  catch_array_var = make_catch_array_var(handlers);
  /* Put the address of the catch types description array into the stack
     frame. */
  try_frame_catch_entries = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(try_frame),
                                                  ehse_variant_field),
                      ehse_try_field),
                    ehse_try_catch_entries_field);
  (void)insert_assignment_statement(try_frame_catch_entries,
                                    (an_expr_operator_kind)eok_passign,
                                    array_var_lvalue_expr(catch_array_var),
                                    &insert_location);
  /* Change the original stmk_try_block statement into an if statement
     that looks like
       if (setjmp(try_frame.variant.try_block.setjmp_buffer) == 0) ...
  */
  /* Make try_frame.variant.try_block.setjmp_buffer.  Note the cast from
     pointer-to-array to pointer-to-element. */
  try_frame_setjmp_buffer = 
                 add_cast(
                   field_lvalue_selection_expr(
                     field_lvalue_selection_expr(
                       field_lvalue_selection_expr(var_lvalue_expr(try_frame),
                                                   ehse_variant_field),
                       ehse_try_field),
                     ehse_try_setjmp_buffer_field),
                   make_pointer_type(make_jmp_buf_type()));
  /* Make the setjmp call. */
#if 0
  /* We shouldn't assume setjmp is a routine. */
  /* What if the user has something called setjmp? */
#endif /* 0 */
  setjmp_call = make_runtime_rout_call("setjmp", &setjmp_routine,
                                       integer_type((an_integer_kind)ik_int),
                                       try_frame_setjmp_buffer);
  /* Generate the comparison against zero. */
  setjmp_call->next = node_for_integer_constant(0L, (an_integer_kind)ik_int);
  compare_node = make_operator_node((an_expr_operator_kind)eok_ieq,
                                    setjmp_call->type, setjmp_call);
  /* Rewrite the stmk_try_block as an "if". */
  set_statement_kind(copy_of_orig_stmt, (a_statement_kind)stmk_if);
  copy_of_orig_stmt->expr = compare_node;
  /* The dependent statement is the statement under the "try". */
  copy_of_orig_stmt->variant.if_stmt.then_statement = stmt_to_try;
  if_stmt = copy_of_orig_stmt;
  /* Pop the stack after the rewritten "if" statement. */
  set_insert_location(copy_of_orig_stmt, &insert_location);
  pop_eh_stack_frame(try_frame, &insert_location);
  /* Walk through the catch clauses and turn each one into an "if" in the
     "else" part of the previous "if". */
  catch_clause_number = 0;
  for (handler = handlers;
       handler != NULL;
       handler = handler->next) {
    a_statement_ptr dep_statement = handler->statement;
    catch_clause_number++;
    prev_if_stmt = if_stmt;
    /* lower the dependent statement of the catch clause. */
    lower_statement(dep_statement);
    if (handler->parameter == NULL) {
      /* This is an ellipsis entry.  No "if" is required, since it accepts
         any type.  Previous error checks have ensured that this is the
         last clause. */
      check_assertion_str(handler->next == NULL,
                          "lower_try_block: ellipsis clause not last");
      prev_if_stmt->variant.if_stmt.else_statement = dep_statement;
    } else {
      /* An entry other than an ellipsis.  Test the catch clause number
         returned by the runtime if an "if" statement:
           if (__catch_clause_number == n) ...
      */
      catch_clause_number_node =
                               var_rvalue_expr(make_catch_clause_number_var());
      catch_clause_number_node->next = 
                            node_for_integer_constant(catch_clause_number,
                                                      (an_integer_kind)ik_int);
      compare_node = make_operator_node((an_expr_operator_kind)eok_ieq,
                                        catch_clause_number_node->type,
                                        catch_clause_number_node);
      if_stmt = alloc_statement((a_statement_kind)stmk_if);
#if 0
      /* Position in a_handler? */
#endif /* 0 */
      if_stmt->position = dep_statement->position;
      if_stmt->expr = compare_node;
      if_stmt->variant.if_stmt.then_statement = dep_statement;
      prev_if_stmt->variant.if_stmt.else_statement = if_stmt;
    }  /* if */
  }  /* for */
}  /* lower_try_block */


void eh_function_lower_init(void)
/*
Initialize static variables needed on a per-function basis for
IL lowering for exceptions.
*/
{
  object_addr_table_var = NULL;
  array_table_var = NULL;
  region_table_var = NULL;
  next_region_number = 0;
}  /* eh_function_lower_init */


void eh_lower_init(void)
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
  base_class_spec_type = NULL;
  exception_type_spec_type = NULL;
  region_descr_type = NULL;
  array_descr_type = NULL;
  eh_stack_entry_type = NULL;
  eh_curr_region_var = NULL;
  curr_eh_stack_entry_var = NULL;
  catch_clause_number_var = NULL;
  setjmp_routine = NULL;
  /* Make a constant for the maximum region number, also used for the
     null region number.  */
  { a_targ_size_t    size;
    a_targ_alignment align;
    /* Find out how big a field is used for region numbers. */
    get_integer_size_and_alignment(TARG_REGION_NUMBER_INT_KIND, &size, &align);
    size *= TARG_CHAR_BIT;
    /* Make a bit mask "size" bits long. */
    if (size >= sizeof(unsigned long)*CHAR_BIT) {
      max_region_number = ~0;
    } else {
      max_region_number = ((unsigned long)1 << size) - 1;
    }  /* if */
  }
}  /* eh_lower_init */

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
