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

Note that the processing here comes in three levels:

- With DO_FULL_PORTABLE_EH_LOWERING set, all EH constructs are lowered to
  C.  That means code is generated to maintain an EH stack, try/catch are
  fully lowered (using setjmp), throw is fully lowered, cleanup tables
  and typeinfo entries are generated, and __eh_curr_region is maintained.
  This mode is compatible with the C-generating back end and the minimal
  runtime supplied.  Setting DO_FULL_PORTABLE_EH_LOWERING forces the
  setting of GENERATE_EH_TABLES.
- With GENERATE_EH_TABLES set (and DO_FULL_PORTABLE_EH_LOWERING not set),
  the cleanup tables and typeinfo entries are generated, but throw/try/catch
  are retained.  Other low-level things are represented as
  enk_lowered_eh_construct expression nodes.  Also, the object address
  table is not maintained, and the handle numbers for entities in the
  cleanup tables use stack offsets represented as ck_stack_offset
  constants.  If you use this mode, you'll have to make modifications
  to the EDG-supplied runtime.
- With neither flag set, no tables are generated (but the object lifetime
  information is preserved), throw/try/catch are retained, and all other
  executable constructs are represented as enk_lowered_eh_construct nodes.
  If you use this mode, you'll have to write your own runtime routines.

The statements and expressions attached under exception handling
constructs (e.g., try/catch, throw) are lowered in all modes (as long as
IL lowering itself is done).
*/

#include "basic_hdrs.h"
#if DO_IL_LOWERING
/* Header files common to all files. */
#include "fe_common.h"
/* Header files used by files involved in IL lowering. */
#include "lower_hdrs.h"
#endif /* DO_IL_LOWERING */

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Only include this code if it is needed: */
#if DO_IL_LOWERING

#if GENERATE_EH_TABLES

static a_type_ptr array_of(a_type_ptr elem_type)
/*
Make an array type whose elements have type elem_type, and return a pointer
to it.  The array is given size [0]; it's assumed that will be changed.
It's also assumed that set_type_size will be called later (e.g., from
finish_array_var).
*/
{
  a_type_ptr array_type = alloc_type((a_type_kind)tk_array);

  array_type->variant.array.variant.number_of_elements = 0; /* Initially. */
  array_type->variant.array.element_type = elem_type;
  /* set_type_size is not called yet. */
  return array_type;
}  /* array_of */

#if DO_FULL_PORTABLE_EH_LOWERING

static a_variable_ptr make_unnamed_local_array_var(a_type_ptr elem_type)
/*
Create an unnamed local (auto) variable whose type is an array of elem_type,
and return a pointer to the variable.  The array size is begun as [0] and
will be adjusted as elements are added.  finish_array_var must be called
sometime later to set the size on the type.  The variable is put in the
current function scope even if the current context is a block inside that.
*/
{
  return make_temporary_in_scope(array_of(elem_type), innermost_function_scope,
                                 /*force_static=*/FALSE);
}  /* make_unnamed_local_array_var */

#endif /* DO_FULL_PORTABLE_EH_LOWERING */

static a_variable_ptr make_unnamed_local_static_array_var(
                                                  a_type_ptr elem_type,
                                                  a_boolean  in_function_scope)
/*
Create an unnamed local static variable whose type is an array of elem_type,
and return a pointer to the variable.  The array size is begun as [0] and
will be adjusted as elements are added.  finish_array_var must be called
sometime later to set the size on the type.  If in_function_scope is TRUE,
put the variable in the function scope instead of the current scope
(which might be a block scope).
*/
{
  a_variable_ptr var;
  a_type_ptr     array_type = array_of(elem_type);

  /* Make the variable.  It is unnamed and static. */
  var = make_unnamed_local_static_variable(array_type, in_function_scope);
  return var;
}  /* make_unnamed_local_static_array_var */


static a_variable_ptr make_init_unnamed_local_static_array_var(
                                              a_type_ptr     elem_type,
                                              a_boolean      in_function_scope,
                                              a_constant_ptr *aggr_con)
/*
Create an unnamed local static variable whose type is an array of elem_type,
and return a pointer to the variable.  The array size is begun as [0] and
will be adjusted as elements are added.  finish_array_var must be called
sometime later to set the size on the type.  The variable will be initialized;
to start the process, an aggregate constant is attached to the variable.
A pointer to this aggregate constant is returned in *aggr_con.
Initial values must be added under the aggregate.  If in_function_scope
is TRUE, put the variable in the function scope instead of the current
scope (which might be a block scope).
*/
{
  a_variable_ptr var;

  /* Make the variable with an array type. */
  var = make_unnamed_local_static_array_var(elem_type, in_function_scope);
  /* The initial value is an aggregate constant pointing to a list of
     aggregate constants. */
  *aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  /* Attach the aggregate constant as the initial value of the variable. */
  /* Use a local-static-variable-init entry to indicate the initialization. */
  (void)alloc_local_static_variable_init(var, 
                                         in_function_scope ?
                                              innermost_function_scope :
                                              curr_context->scope,
                                         (an_init_kind)initk_static,
                                         *aggr_con, (a_dynamic_init_ptr)NULL);
  return var;
}  /* make_init_unnamed_local_static_array_var */


static a_targ_size_t incr_nelems_of_array_var(a_variable_ptr var)
/*
Increment the number of elements of the array variable pointed to by var.
Return the pre-incremented value (which is right as a subscript).
*/
{
  /* Add one to the array size.  set_type_size is called later. */
  return var->type->variant.array.variant.number_of_elements++;
}  /* incr_nelems_of_array_var */


static a_targ_size_t add_elem_to_array_var(a_constant_ptr con,
                                           a_variable_ptr var,
                                           a_constant_ptr aggr_con)
/*
Add the indicated constant as an initializer of an element of the array
variable pointed to by var.  aggr_con is the top-level aggregate constant
that is the initial value of the variable.  Increment the number of
elements of the array.  Return the pre-incremented size (which is right
as a subscript).
*/
{
  /* Add the constant to the aggregate initializer list. */
  if (aggr_con->variant.aggregate.first_constant == NULL) {
    aggr_con->variant.aggregate.first_constant = con;
  } else {
    aggr_con->variant.aggregate.last_constant->next = con;
  }  /* if */
  aggr_con->variant.aggregate.last_constant = con;
  /* Increment the number of elements in the array. */
  return incr_nelems_of_array_var(var);
}  /* add_elem_to_array_var */


static void finish_array_var(a_variable_ptr var)
/*
var is an array variable (for example, one created by
make_init_unnamed_local_static_array_var).  The building of the variable
is now completed, so finish it off.  In particular, the array size is now
known, so call set_type_size on the type.
*/
{
  /* Finish off the array type by setting its size. */
  set_type_size(var->type);
}  /* finish_array_var */


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


static unsigned long
		num_of_pending_class_typeinfo_vars;
			/* Count of typeinfo variables generated for classes
			   that have not been revisited to determine whether
			   they need definitions.  Used to cut short the
			   final pass that finds and defines the variables. */


static a_variable_ptr make_typeinfo_var(a_type_ptr type)
/*
Make a typeinfo variable for the indicated type (if it does not exist
already) and return a pointer to it.  The variable points to runtime
type information.  It is always allocated in the file scope memory region.
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


/*
Bit set values for the flags byte of exception_type_spec.  These must
match the runtime's definition.
*/
#define ETS_IS_POINTER		0x01
			/* A pointer to an object of the type specified
			   by typeinfo. */
#define ETS_POINTER_TO_CONST	0x02
#define ETS_POINTER_TO_VOLATILE	0x04
			/* Indication of the type qualifiers on the type
			   pointed to, in the pointer case. */
#define ETS_IS_REFERENCE	0x08
			/* A reference to an object of the type specified
			   by typeinfo. */
#define ETS_IS_ELLIPSIS		0x10
			/* An ellipsis (for a catch clause). */
#define ETS_LAST		0x20
			/* TRUE if this is the last type specification in
			   the array. */


static a_variable_ptr typeinfo_var_for_type(a_type_ptr    type,
                                            unsigned long *flags_value)
/*
Create the typeinfo variable for the indicated type, and return a pointer
to it.  Type qualifiers on the type are dropped.  For a pointer or reference
to a type, make the typeinfo variable for the underlying type and set
*flags_value to indicate a pointer or reference.
*/
{
  a_variable_ptr        typeinfo_var;
  a_type_ptr            typeinfo_type;
  a_type_qualifier_set  qualifiers;

  typeinfo_type = type;
  *flags_value = 0;
  /* For a pointer or reference to a type, use the typeinfo for the
     underlying type and a flag to indicate the reference or pointer.
     Both flags are on for a reference to a pointer. */
  if (is_reference_type(typeinfo_type)) {
    typeinfo_type = type_pointed_to(typeinfo_type);
    *flags_value |= ETS_IS_REFERENCE;
  }  /* if */
  if (is_pointer_type(typeinfo_type)) {
    typeinfo_type = type_pointed_to(typeinfo_type);
    *flags_value |= ETS_IS_POINTER;
    /* Remember the type qualifiers on the type pointed to. */
    qualifiers = get_type_qualifiers(typeinfo_type);
    if (qualifiers & TQ_CONST) {
      *flags_value |= ETS_POINTER_TO_CONST;
    }  /* if */
    if (qualifiers & TQ_VOLATILE) {
      *flags_value |= ETS_POINTER_TO_VOLATILE;
    }  /* if */
  }  /* if */
  /* Strip typerefs but watch out for rewritten pointers-to-members. */
  typeinfo_type = underlying_type(typeinfo_type);
  /* Create the typeinfo variable. */
  typeinfo_var = make_typeinfo_var(typeinfo_type);
  return typeinfo_var;
}  /* typeinfo_var_for_type */


void type_is_used_in_exception(a_type_ptr type)
/*
The indicated type is used in an exception context.  Put out any necessary
information on it.
*/
{
  unsigned long flags_value;

  /* We need a typeinfo variable for the underlying type.  Make it if it
     does not exist already. */
  (void)typeinfo_var_for_type(type, &flags_value);
}  /* type_is_used_in_exception */


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
#define BCS_LAST		0x02
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
This is used as part of the typeinfo information.  The variable is always
allocated in the file scope memory region.
*/
{
  a_type_ptr       array_type;
  a_base_class_ptr bcp;
  a_constant_ptr   aggr_con, sub_aggr_con;
  a_variable_ptr   typeinfo_var, bc_var;
  a_boolean        ovflo;
  a_constant_ptr   typeinfo_con, offset_con, flags_con;
  unsigned long    flags_value;
  a_targ_size_t    offset;

  /* The current region is already the file scope memory region when
     this routine is called. */
  /* Make an initialized static variable that is an array of base_class_spec
     structures. */
  /* Make the array type. */
  array_type = array_of(make_base_class_spec_type());
  /* Make the variable.  It is unnamed and static and in the file scope. */
  /* make_init_unnamed_local_static_array_var cannot be used because we
     want the variable always to be in the file scope. */
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
    /* Include information only on direct or virtual base classes. */
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
      set_variable_address_constant(typeinfo_var, typeinfo_con,
                                    /*set_address_taken_flag=*/TRUE);
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
      set_integer_constant_with_overflow_check(offset_con,
                                               (long)offset,
                                               TARG_DELTA_INT_KIND);
      /* Make the flags constant. */
      flags_con = alloc_constant((a_constant_repr_kind)ck_integer);
      set_unsigned_integer_constant(flags_con, flags_value,
                                    (an_integer_kind)ik_unsigned_char);
      /* Link the constants together and make an aggregate constant. */
      typeinfo_con->next = offset_con;
      offset_con->next = flags_con;
      sub_aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
      sub_aggr_con->variant.aggregate.first_constant = typeinfo_con;
      sub_aggr_con->variant.aggregate.last_constant = flags_con;
      /* Add the constant to the aggregate initializer list. */
      (void)add_elem_to_array_var(sub_aggr_con, bc_var, aggr_con);
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
  a_routine_ptr  dtor_routine;
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
    set_variable_address_constant(make_id_object_var(type), id_con,
                                  /*set_address_taken_flag=*/TRUE);
  }  /* if */
  /* Destructor pointer. */
  curr_field = curr_field->next;
  curr_field_type = curr_field->type;
  dtor_con = alloc_constant((a_constant_repr_kind)ck_address);
  /* See if the class has a destructor. */
  dtor_sym = symbol_supplement_for_class(type)->destructor;
  dtor_routine = NULL;
  if (dtor_sym != NULL) {
    dtor_routine = dtor_sym->variant.routine.ptr;
    if (dtor_routine->assoc_scope == NULL_region_number) {
      /* The destructor is declared but not defined.  Use a null pointer. */
      dtor_routine = NULL;
    }  /* if */
  }  /* if */
  if (dtor_routine == NULL) {
    /* The class has no destructor; use a NULL pointer. */
    make_zero_of_proper_type(curr_field_type, dtor_con);
  } else {
    /* The class has a destructor.  Make a pointer to the routine. */
    dtor_routine->source_corresp.referenced = TRUE;
    set_routine_address_constant(dtor_routine, dtor_con,
                                 /*set_address_taken_flag=*/TRUE);
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
    set_variable_address_constant(bc_var, bc_con,
                                  /*set_address_taken_flag=*/TRUE);
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
          /* The typeinfo variable is static if the virtual function table
             is static. */
          force_static =
                       (vtbl_var->storage_class == (a_storage_class)sc_static);
          definition_needed =
                (vtbl_var->storage_class == (a_storage_class)sc_unspecified) ||
                force_static;
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


#if DO_FULL_PORTABLE_EH_LOWERING
/*
Pointer to the variable entry for the object address array table of
a function.  NULL until allocated.
*/
static a_variable_ptr
		object_addr_table_var;


a_handle_number object_addr_table_index(void)
/*
Add an entry to the object address table array (creating the table and its
associated variable if necessary) and return its index number.  The entry
is not filled in (that's done by init_object_addr_table_entry); this routine
just increases the size of the array by one.
*/
{
  a_handle_number entry_number;

  /* Make the variable if it has not yet been made. */
  if (object_addr_table_var == NULL) {
    /* The variable is an array whose elements have type "void *". */
    a_type_ptr elem_type = void_star_type();
    object_addr_table_var = make_unnamed_local_array_var(elem_type);
  }  /* if */
  /* Add an element to the object address table array. */
  entry_number = incr_nelems_of_array_var(object_addr_table_var);
  return entry_number;
}  /* object_addr_table_index */


void init_object_addr_table_entry(an_init_pos_descr_ptr ipdp,
                                  a_handle_number       entry_number,
                                  an_insert_location    *insert_location)
/*
Initialize entry entry_number of the object address table so that it
points to the entity identified by ipdp.  Any code required is inserted
at *insert_location and *insert_location is updated.
*/
{
  an_expr_node_ptr object_addr_table_node, subsc_node, object_addr_node;

  /* Insert code to initialize the element of the table to the address of the
     object, i.e.,
       object_addr_table[n] = (void *)ipdp-address;
  */
  object_addr_table_node = array_var_lvalue_expr(object_addr_table_var);
  object_addr_table_node->next = node_for_integer_constant((long)entry_number,
                                                         targ_size_t_int_kind);
  subsc_node = make_operator_node((an_expr_operator_kind)eok_padd_subsc,
                                  object_addr_table_node->type,
                                  object_addr_table_node);
  object_addr_node = add_cast_if_necessary(make_init_entity_node(ipdp,
                                                    /*using_as_address=*/TRUE,
                                                    /*using_as_dest=*/FALSE),
                                           void_star_type());
  (void)insert_assignment_statement(subsc_node,
                                    (an_expr_operator_kind)eok_passign,
                                    object_addr_node, insert_location);
}  /* init_object_addr_table_entry */

#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#if !DO_FULL_PORTABLE_EH_LOWERING

static a_targ_size_t offset_for_init_modifiers(
                                            an_init_pos_modifier_ptr modifiers)
/*
Return the byte offset that results from the indicated list of modifiers
of an init position description.
*/
{
  a_targ_size_t offset = 0;

  /* If there are no modifiers, return 0. */
  if (modifiers != NULL) {
    /* Process the modifiers preceding the final modifier, then add the final
       qualifier (recall that the modifiers are in order from the innermost
       to the outermost). */
    offset = offset_for_init_modifiers(modifiers->next);
    /* Add the final modifier. */
    if (modifiers->curr_field != NULL) {
      /* Add a field selection. */
      a_field_ptr field = modifiers->curr_field;
      offset += field->offset;
    } else if (modifiers->curr_base != NULL) {
      /* Add a base class selection.  Note that we assume a complete object
         here, which is okay for the intended use of this routine. */
      offset += modifiers->curr_base->offset;
    } else {
      /* Add an array element selection. */
      a_type_ptr elem_type = f_skip_typerefs(modifiers->type);
      offset += elem_type->size * modifiers->curr_elem;
    }  /* if */
  }  /* if */
  return offset;
}  /* offset_for_init_modifiers */

#endif /* !DO_FULL_PORTABLE_EH_LOWERING */

/* Type used to carry information about the location of an entity in the
   form used in the region table. */
#if DO_FULL_PORTABLE_EH_LOWERING
/* In the portable scheme, all that's needed is an index into the object
   address table. */
typedef a_handle_number a_handle;
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
typedef struct a_handle {
  /* Representation for the non-portable scheme, which can result in a
     ck_stack_offset constant. */
  a_variable_ptr
		variable;
			/* A variable whose offset in the stack is the base
			   for the handle value, or NULL if there is no such
			   variable.  If this is non-NULL, a ck_stack_offset
			   constant will be required. */
  a_targ_size_t	offset;	/* Offset relative to the variable if there is one, or
			   constant value if there is no variable. */
  unsigned long	flags;	/* Extra flag bits needed, e.g., RDF_INDIRECT. */
} a_handle;

static void clear_handle(a_handle *handle)
/*
Clear the fields of a handle to default values.  A handle is used to
represent the address of an entity in the region table.
*/
{
  handle->variable = NULL;
  handle->offset = 0;
  handle->flags = 0;
}  /* clear_handle */

#endif /* DO_FULL_PORTABLE_EH_LOWERING */

/*
Bit flags for the flags field of a region description.  These must match
the runtime's definition.
*/
#define RDF_INDIRECT		0x01
			/* TRUE if the address provided by the handle field
			   is a pointer to the object.  Not used in the
			   portable scheme. */
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
#define RDF_THIS_PARAM_OFFSET	0x10
			/* TRUE if the object is a base class of an
			   object being constructed or destructed.  Not
			   used in the portable scheme. */


#if !DO_FULL_PORTABLE_EH_LOWERING
/*ARGSUSED*/ /* <-- insert_location is not used. */
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
static void make_handle_for_entity(an_init_pos_descr_ptr ipdp,
                                   a_handle              *handle,
                                   an_insert_location    *insert_location)
/*
Return the handle (identifier to be used in the region table) for the
entity indicated by ipdp.  The handle is placed in *handle.  If any code
needs to be generated to establish that handle, insert the code at
*insert_Location.
*/
{
#if DO_FULL_PORTABLE_EH_LOWERING
  /* Portable scheme: allocate the proper entry in the object address table. */
  a_handle_number handle_number = object_addr_table_index();
  *handle = handle_number;
  /* Put the entity address in the object address table. */
  init_object_addr_table_entry(ipdp, handle_number, insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* Non-portable scheme.  Use a stack offset, with variations. */
  clear_handle(handle);
  if (ipdp->indirect_through_variable) {
    if (ipdp->variable->is_this_parameter) {
      /* This entity is relative to the "this" parameter, e.g., it's a
         member or base class being initialized in a constructor. */
      handle->flags |= RDF_THIS_PARAM_OFFSET;
      handle->offset = offset_for_init_modifiers(ipdp->modifiers);
      /* variable stays NULL. */
    } else {
      /* Simple indirection through a variable. */
      check_assertion_str(ipdp->modifiers == NULL,
                          "make_handle_for_entity: non-simple indirection");
      handle->flags |= RDF_INDIRECT;
      handle->variable = ipdp->variable;
      /* offset stays zero. */
    }  /* if */
  } else {
    /* Not indirect. */
    handle->variable = ipdp->variable;
    handle->offset = offset_for_init_modifiers(ipdp->modifiers);
  }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
}  /* make_handle_for_entity */


static a_constant_ptr make_handle_constant(a_handle *handle)
/*
Make a constant for the indicated handle (description of the location of
a variable) and return a pointer to the constant.
*/
{
  a_constant_ptr handle_con;

#if DO_FULL_PORTABLE_EH_LOWERING
  /* Portable scheme: the number is the index in the object address table
     or the array table. */
  handle_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant_with_overflow_check(handle_con,
                                                    (unsigned long)*handle,
                                                    targ_var_handle_int_kind);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* Non-portable scheme -- can use a ck_stack_offset for the offset of
     a variable. */
  if (handle->variable != NULL) {
    handle_con = alloc_constant((a_constant_repr_kind)ck_stack_offset);
    handle_con->type = integer_type(targ_var_handle_int_kind);
    handle_con->variant.stack_offset.variable = handle->variable;
    handle_con->variant.stack_offset.offset = handle->offset;
  } else {
    /* No variable, so this is a simple constant (e.g., an index into the
       array table). */
    handle_con = alloc_constant((a_constant_repr_kind)ck_integer);
    set_unsigned_integer_constant_with_overflow_check(handle_con,
                                                 (unsigned long)handle->offset,
                                                 targ_var_handle_int_kind);
  }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  return handle_con;
}  /* make_handle_constant */


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
                               // (or stack offset in non-portable scheme)
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
    /* field: unsigned short (or whatever) handle */
    make_lowered_field("handle",
                       integer_type(targ_var_handle_int_kind),
                       &byte_offset, array_descr_type, &last_field);
    /* field: size_t elem_size */
    make_lowered_field("elem_size",
                       integer_type(targ_size_t_int_kind),
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
Pointer to the variable entry for the array table of a function, and to
the top-level aggregate constant that is its initial value.  NULL
until allocated.
*/
static a_variable_ptr
		array_table_var;
static a_constant_ptr
		array_table_aggr_con;


static void make_array_table_entry(an_init_pos_descr_ptr ipdp,
                                   a_handle              *handle)
/*
Add an entry to the array table (creating the table and its associated
variable if necessary) for the object whose position is given by ipdp.
The handle (identifying information for the region table) for
the object is provided in *handle; it is modified appropriately
on return.  This routine can also be called for non-arrays in cases
where an array table entry is needed to provide information not
included in the region description entry (for example, for a
new-allocation record in a case where the delete routine requires a
second parameter giving the size; the size is not available in the
region description entry).
*/
{
  a_handle_number  entry_number;
  a_targ_ptrdiff_t elem_count;
  a_constant_ptr   handle_con, elem_size_con, size_con, aggr_con;
  a_type_ptr       elem_type;

  /* Make the variable for the array table if it has not yet been made. */
  if (array_table_var == NULL) {
    /* The variable is an array whose elements have type array_descr. */
    array_table_var =
          make_init_unnamed_local_static_array_var(make_array_descr_type(),
                                                   /*in_function_scope=*/TRUE,
                                                   &array_table_aggr_con);
  }  /* if */
  /* Make the aggregate constant for the entry in the array table.  It consists
     of the handle offset, the size of each element, and the number of
     elements. */
  /* Make the handle constant. */
  handle_con = make_handle_constant(handle);
  /* For the element size: note that the init_pos_descr has the type of an
     element, not of the whole array.  For non-arrays, the type is of
     course as expected. */
  elem_type = type_from_init_pos_descr(ipdp);
  elem_type = skip_typerefs(elem_type);
  elem_size_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant(elem_size_con, (unsigned long)elem_type->size,
                                targ_size_t_int_kind);
  size_con = alloc_constant((a_constant_repr_kind)ck_integer);
  if (ipdp->whole_array) {
    /* The entity really is an array.  Get the element count.  -1 indicates
       that the runtime should look up the number of elements in the array. */
    elem_count = ipdp->array_element_count;
  } else {
    /* Not an array (see header comment above).  Use an element count of 0. */
    elem_count = 0;
  }  /* if */
  set_integer_constant(size_con, (long)elem_count, (an_integer_kind)ik_long);
  /* Link the constants together and make an aggregate constant. */
  handle_con->next = elem_size_con;
  elem_size_con->next = size_con;
  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->variant.aggregate.first_constant = handle_con;
  aggr_con->variant.aggregate.last_constant = size_con;
  /* Add the aggregate as an element of the object address table array. */
  entry_number = add_elem_to_array_var(aggr_con, array_table_var,
                                       array_table_aggr_con);
  /* Adjust the handle to refer to the index into the array table in place
     of the original object. */
#if DO_FULL_PORTABLE_EH_LOWERING
  *handle = entry_number;
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  handle->variable = NULL;
  handle->offset = entry_number;
  /* Set the flag that indicates this object is an array. */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
}  /* make_array_table_entry */


/*
Pointer to the region_descr struct type (used to represent a cleanup
region for exception processing).  NULL until created.
*/
static a_type_ptr
		region_descr_type;


static a_type_ptr make_region_descr_type(void)
/*
Make the region_descr struct type (used to represent the cleanup required
in a particular region for exception processing) if it is not made already,
and return a pointer to it.  Its definition is

  struct region_descr {
    __vptp         dtor;    // Destructor or delete routine pointer
    unsigned short handle;  // Index of object in object address table
                            // (or stack offset in non-portable scheme)
    unsigned short next;    // Next cleanup region
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
    /* field: unsigned short (or whatever) handle */
    make_lowered_field("handle",
                       integer_type(targ_var_handle_int_kind),
                       &byte_offset, region_descr_type, &last_field);
    /* field: unsigned short next */
    make_lowered_field("next",
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


a_cleanup_region_number cleanup_region_number(a_dynamic_init_ptr dip)
/*
Return the cleanup region number for the indicated initialization.
If dip is NULL, return null_eh_region_number.
*/
{
  a_cleanup_region_number region_number;

  /* Get the region number. */
  if (dip != NULL) {
    a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
    check_assertion(dedp != NULL);
    region_number = dedp->region_number;
  } else {
    /* There is no region.  Use a code that indicates that. */
    region_number = null_eh_region_number;
  }  /* if */
  return region_number;
}  /* cleanup_region_number */


static a_constant_ptr next_region_number_constant(a_dynamic_init_ptr dip)
/*
Return a pointer to the next-region-number constant of the region table
entry for the indicated destruction.
*/
{
  a_destructible_entity_descr_ptr
                 dedp = dip->destructible_entity_descr;
  a_constant_ptr aggr_con = dedp->region_table_entry;
  /* The constant for the next region number is the third one on the
     list of constants in the region table entry.
     See add_region_table_entry. */
  a_constant_ptr con = aggr_con->variant.aggregate.first_constant->next->next;
  return con;
}  /* next_region_number_constant */


static void set_next_region_number(a_dynamic_init_ptr      dip,
                                   a_cleanup_region_number next_region_number)
/*
Set the next region number of the indicated destruction to next_region_number.
This routine is used to relink entries after they've been created.
*/
{
  a_constant_ptr con = next_region_number_constant(dip);
  a_constant_ptr con_next = con->next;

  set_unsigned_integer_constant(con, next_region_number,
                                TARG_REGION_NUMBER_INT_KIND);
  con->next = con_next;
}  /* set_next_region_number */

#if DO_UNORDERED_EH_PROCESSING

static a_cleanup_region_number get_next_region_number(a_dynamic_init_ptr dip)
/*
Fetch the next region number of the indicated destruction.
*/
{
  a_constant_ptr          con = next_region_number_constant(dip);
  a_boolean               ovflo;
  a_cleanup_region_number next_region_number =
                               unsigned_value_of_integer_constant(con, &ovflo);
  check_assertion(!ovflo);
  return next_region_number;
}  /* get_next_region_number */

#endif /* DO_UNORDERED_EH_PROCESSING */

static a_cleanup_region_number
		next_avail_region_number;
			/* Next available destructible object region number
			   within the current function. */

/*
Pointer to the variable entry for the region table of a function (which
contains information about destructible objects), and to the top-level
aggregate constant that is its initial value.  NULL until allocated.
*/
static a_variable_ptr
		region_table_var;
static a_constant_ptr
		region_table_aggr_con;


static a_constant_ptr add_raw_region_table_entry(a_constant_ptr con_list,
                                                 a_constant_ptr end_con_list)
/*
Create a region table entry from the constants on the list delimited by
con_list and end_con_list and add it to the region table array.  Return
a pointer to the aggregate constant created.
*/
{
  a_constant_ptr aggr_con;

  aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  aggr_con->variant.aggregate.first_constant = con_list;
  aggr_con->variant.aggregate.last_constant = end_con_list;
  /* Add the aggregate as an element of the region table array. */
  (void)add_elem_to_array_var(aggr_con, region_table_var,
                              region_table_aggr_con);
  /* Increment the count of entries in the array. */
  next_avail_region_number++;
  if (next_avail_region_number >= null_eh_region_number) {
    /* Too many regions. */
    catastrophe(ec_program_too_large);
  }  /* if */
  return aggr_con;
}  /* add_raw_region_table_entry */


static a_constant_ptr add_region_table_entry(
                                         a_routine_ptr           dtor_routine,
                                         a_handle                *handle,
                                         a_cleanup_region_number next_region,
                                         unsigned long           flags_value)
/*
Create an entry in the exception cleanup region table.  dtor_routine,
handle, next_region, and flags_value give the values for the various
fields.   Create an aggregate constant for the entry and add
it to the initial value of region_table_var.  Return the address of
the aggregate constant.
*/
{
  a_constant_ptr dtor_con, handle_con, next_con, flags_con, aggr_con;
  a_type_ptr     ptr_func_type;

  /* Make the aggregate constant for the entry in the region description
     table.  It has a structure as follows:
       struct region_descr {
         __vptp         dtor;    // Destructor or delete routine pointer
         unsigned short handle;  // Index of object in object address table
                                 // (or stack offset in non-portable scheme)
         unsigned short next;    // Next cleanup region
         unsigned char  flags;   // Bit flags
       };
  */
  /* Make the destructor pointer. */
  dtor_con = alloc_constant((a_constant_repr_kind)ck_address);
  /* Create the generic function pointer type if it does not exist already. */
  ptr_func_type = make_vptp_type();
  if (dtor_routine == NULL) {
    /* The object has no destructor; use a NULL pointer. */
    make_zero_of_proper_type(ptr_func_type, dtor_con);
  } else {
    /* The class has a destructor.  Make a pointer to the routine. */
    dtor_routine->source_corresp.referenced = TRUE;
    set_routine_address_constant(dtor_routine, dtor_con,
                                 /*set_address_taken_flag=*/TRUE);
    implicit_cast(dtor_con, ptr_func_type);
  }  /* if */
  /* Make the handle constant. */
  handle_con = make_handle_constant(handle);
  /* Make the next region index number.  NOTE that next_region_number_constant
     expects the constant to be the third one on the list. */
  next_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant(next_con, next_region,
                                TARG_REGION_NUMBER_INT_KIND);
  /* Make the flags constant. */
  flags_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant(flags_con, flags_value,
                                (an_integer_kind)ik_unsigned_char);
  /* Link the constants together to make an aggregate constant. */
  dtor_con->next = handle_con;
  handle_con->next = next_con;
  next_con->next = flags_con;
  /* Make the aggregate and add it as an element of the region table array. */
  aggr_con = add_raw_region_table_entry(dtor_con, flags_con);
  return aggr_con;
}  /* add_region_table_entry */


static a_constant_ptr make_region_table_entry(
                              an_init_pos_descr_ptr   ipdp,
                              a_routine_ptr           routine,
                              a_boolean               is_delete,
                              a_variable_ptr          conditional_flag_var,
                              a_handle                *conditional_flag_handle,
                              a_cleanup_region_number next_region_number,
                              a_cleanup_region_number *region_number,
                              an_insert_location      *insert_location)
/*
Add an entry to the region table (which describes destructible objects)
related to the object whose position is given by ipdp.  routine is
a destructor (is_delete == FALSE) or a delete routine (is_delete ==
TRUE) to be called to do cleanup on the object.  conditional_flag_var,
if non-NULL, points to a conditional flag variable that is non-zero to
indicate that the destruction or deletion should be done.  In that case,
conditional_flag_handle gives the handle for the address for the
conditional flag.  next_region_number is used as the
next-region-table-entry number for the new entry.  The region table
entry number for the new entry is returned in *region_number.  Any
initialization code required will be inserted at *insert_location.
The region table variable is created if necessary.  Return a pointer
to the aggregate constant for the region table entry.
*/
{
  a_handle        handle;
  unsigned long   flags_value = 0;
  a_constant_ptr  region_table_entry;
  a_boolean       need_array_info = FALSE;

  /* Make the handle for the entity. */
  make_handle_for_entity(ipdp, &handle, insert_location);
#if !DO_FULL_PORTABLE_EH_LOWERING
  flags_value |= handle.flags;
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* See if we need array information on the entity. */
  if (ipdp->whole_array) {
    need_array_info = TRUE;
  } else if (is_delete) {
    /* Check for the 2-argument version of delete; we need array information
       for that because we need the size of the entity. */
    a_param_type_ptr param1 = unlowered_param_type_list(routine->type);
    check_assertion(param1 != NULL);
    if (param1->next != NULL) {
      /* Two-argument form.  Need array information. */
      need_array_info = TRUE;
    }  /* if */
  }  /* if */
  if (need_array_info) {
    /* We need an entry in the array table. */
    make_array_table_entry(ipdp, &handle);
    /* Set the flag that indicates this object is an array. */
    flags_value |= RDF_ARRAY;
  }  /* if */
  if (conditional_flag_var != NULL) {
    /* This entry needs a conditional flag.  More on this below. */
    /* The object address table entry must be initialized when the conditional
       flag is initialized; here is too late because the runtime needs to
       be able to test the conditional flag even if the associated entity
       was not constructed.  See init_conditional_flag_var. */
    flags_value |= RDF_CONDITIONAL_FLAG;
  }  /* if */
  if (is_delete) {
    /* Indicate the delete case. */
    flags_value |= RDF_NEW_ALLOCATION;
  }  /* if */
  /* Make the variable for the region table if it has not yet been made. */
  if (region_table_var == NULL) {
    /* The variable is an array whose elements have type array_descr. */
    region_table_var =
          make_init_unnamed_local_static_array_var(make_region_descr_type(),
                                                   /*in_function_scope=*/TRUE,
                                                   &region_table_aggr_con);
  }  /* if */
  /* Assign a region number to this entry. */
  *region_number = next_avail_region_number;
  /* Make the region table entry. */
  region_table_entry = add_region_table_entry(routine,
                                              &handle,
                                              next_region_number,
                                              flags_value);
  if (conditional_flag_var != NULL) {
    /* Make a second region table entry for the conditional flag. */
    (void)add_region_table_entry((a_routine_ptr)NULL,
                                 conditional_flag_handle,
                                 null_eh_region_number,
                                 (unsigned long)0);
  }  /* if */
  return region_table_entry;
}  /* make_region_table_entry */

#if DO_UNORDERED_EH_PROCESSING

static void maintain_unordered_destructions_set(a_dynamic_init_ptr dip)
/*
dip points to a destruction with the "unordered" flag TRUE, for which
the region table entry was just created.  Do some extra processing
required for unordered destructions.
*/
{
  a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
  a_dynamic_init_ptr              next_dip = dedp->next_in_region_table;

  check_assertion(dip->unordered);
  /* If this entry is part of an unordered set, all the region table entries
     for the entities in the unordered set are treated as a block.  That is,
     the entire block goes into the region table cleanup chain as soon as
     any member of the set is initialized, and the entire block stays in
     the region table cleanup chain until all of the entities have been
     destroyed.  Each entity is given a conditional flag so that we can
     tell at runtime which entities have been constructed and not yet
     destroyed.  Since we do not want to go back and fix code, the
     first unordered entry encountered establishes the beginning region
     number for the block.  Subsequent contiguous unordered entries
     are linked into the region table on a list following the region
     table for the initial entry.  The final entry (so far) points to
     the first region number past the ordered set, i.e., the original
     next region number from the initial entry.  The region_number
     and cleanup_state_to_set_when_starting_destruction fields of the
     entries after the first are set to the region number of the first
     entry so that the entire block will be put into the cleanup chain
     and kept there until the destruction for the initial entry is
     generated (it gets generated after the destruction for the others). */
  if (next_dip != NULL && next_dip->unordered) {
    a_destructible_entity_descr_ptr
                              next_dedp = next_dip->destructible_entity_descr;
    /* Relink the previous last entry to this new entry, and this new
       entry to point to the first region number beyond the ordered set. */
    a_cleanup_region_number next_region_number_past_ordered_set =
                                              get_next_region_number(next_dip);
    set_next_region_number(next_dip, dedp->region_number);
    set_next_region_number(dip, next_region_number_past_ordered_set);
    dedp->region_number = next_dedp->region_number;
    dedp->cleanup_state_to_set_when_starting_destruction = next_dip;
  }  /* if */
}  /* maintain_unordered_destructions_set */

#endif /* DO_UNORDERED_EH_PROCESSING */

void make_dyn_init_region_table_entry(a_dynamic_init_ptr dip,
                                      a_dynamic_init_ptr next_dip,
                                      an_insert_location *insert_location)
/*
Add an entry to the region table (which describes destructible objects)
for the initialization described by dip.  The entry will point to
next_dip as its next region.  The initialization must have an attached
destructible entity description, and the conditional_flag_var field of
that entry must be filled in if appropriate (if a conditional flag
variable is indicated, a region table entry will be created for it
as well); cleanup_state_to_set_when_starting_destruction should
also be set (it differs from next_dip in that the latter does not
cross over lifetime boundaries).  Also insert (at *insert_location)
initialization code for the proper entry in the object address table.
The region table variable is created if necessary.
*/
{
  a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
  a_handle                        conditional_flag_handle;
  a_cleanup_region_number         next_region_number;

  check_assertion(dedp != NULL);
  /* Make a handle that describes the address of the conditional flag if
     any. */
  if (dedp->conditional_flag_var != NULL) {
#if DO_FULL_PORTABLE_EH_LOWERING
    conditional_flag_handle = dedp->conditional_flag_handle;
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
    an_init_pos_descr ipd;
    set_var_init_pos_descr(dedp->conditional_flag_var, &ipd);
    make_handle_for_entity(&ipd, &conditional_flag_handle, insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  }  /* if */
  dedp->next_in_region_table = next_dip;
  next_region_number = cleanup_region_number(
                         dedp->cleanup_state_to_set_when_starting_destruction);
  dedp->region_table_entry =
             make_region_table_entry(&dedp->init_pos_descr,
                                     dip->destructor,
                                     (a_boolean)dip->
                                            is_freeing_of_storage_on_exception,
                                     dedp->conditional_flag_var,
                                     &conditional_flag_handle,
                                     next_region_number,
                                     &dedp->region_number,
                                     insert_location);
#if DO_UNORDERED_EH_PROCESSING
  if (dip->unordered) {
    /* The destruction is unordered with respect to some surrounding
       destructions, so do some extra processing. */
    maintain_unordered_destructions_set(dip);
  }  /* if */
#endif /* DO_UNORDERED_EH_PROCESSING */
}  /* make_dyn_init_region_table_entry */


static a_constant_ptr clone_raw_region_table_entry(
                                        a_constant_ptr          aggr_con,
                                        a_cleanup_region_number *region_number)
/*
Clone the region table entry defined by the aggregate constant.  Return
a pointer to the clone, and set *region_number to the region number for
the clone.
*/
{
  a_constant_ptr con_list, end_con_list, source_con, copy_con, clone_aggr_con;
  
  /* Copy the list of constants. */
  con_list = end_con_list = NULL;
  for (source_con = aggr_con->variant.aggregate.first_constant;
       source_con != NULL;
       source_con = source_con->next) {
    /* Make a copy of the constant. */
    copy_con = alloc_unshared_constant(source_con);
    /* Add the constant to the list of the aggregate copy. */
    if (con_list == NULL) {
      con_list = copy_con;
    } else {
      end_con_list->next = copy_con;
    }  /* if */
    end_con_list = copy_con;
  }  /* for */
  /* Add the cloned region table entry. */
  *region_number = next_avail_region_number;
  clone_aggr_con = add_raw_region_table_entry(con_list, end_con_list);
  return clone_aggr_con;
}  /* clone_raw_region_table_entry */


void clone_region_table_entry_list(a_dynamic_init_ptr dip,
                                   a_dynamic_init_ptr stop_before)
/*
Clone the region table entry associated with the initialization pointed to
by dip, and all preceding initializations (following the
next_in_region_table pointer), stopping before the entry stop_before.
*/
{
  a_destructible_entity_descr_ptr dedp = dip->destructible_entity_descr;
  a_cleanup_region_number         next_region_number, region_number;
  a_constant_ptr                  orig_region_table_entry;
  a_dynamic_init_ptr              next_dip = dedp->next_in_region_table;

  if (next_dip != stop_before) {
    /* This is not the last entry on the list, so do a recursive call to
       clone the rest of the list. */
    clone_region_table_entry_list(next_dip, stop_before);
  }  /* if */
  /* Clone the entry and update the information in dedp (that is, the clone
     becomes the official entry from now on). */
  orig_region_table_entry = dedp->region_table_entry;
  dedp->region_table_entry = clone_raw_region_table_entry(
                                                       orig_region_table_entry,
                                                       &dedp->region_number);
  if (dedp->conditional_flag_var != NULL) {
    /* The entry has a conditional flag, so clone the region table entry for
       the conditional flag too. */
    (void)clone_raw_region_table_entry(orig_region_table_entry->next,
                                       &region_number);
  }  /* if */
  /* Link the clone to the proper next entry. */
  next_region_number = cleanup_region_number(next_dip);
  set_next_region_number(dip, next_region_number);
  dedp->cleanup_state_to_set_when_starting_destruction = next_dip;
#if DO_UNORDERED_EH_PROCESSING
  if (dip->unordered) {
    /* The destruction is unordered with respect to some surrounding
       destructions, so do some extra processing. */
    maintain_unordered_destructions_set(dip);
  }  /* if */
#endif /* DO_UNORDERED_EH_PROCESSING */
}  /* clone_region_table_entry_list */

#endif /* GENERATE_EH_TABLES */
#if DO_FULL_PORTABLE_EH_LOWERING

/*
Variable entries for the global variables used for exception processing.
NULL until created.
*/
static a_variable_ptr
		eh_curr_region_var,
		curr_eh_stack_entry_var,
		catch_clause_number_var,
		caught_object_address_var;


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


static void assign_to_eh_curr_region(an_expr_node_ptr   node,
                                     an_insert_location *insert_location)
/*
Insert an assignment to set __eh_curr_region to the given expression node.
The assignment is inserted at *insert_location and *insert_location is
updated.
*/
{
  (void)insert_var_assignment_statement(make_eh_curr_region_var(),
                                        (an_expr_operator_kind)eok_iassign,
                                        node, insert_location);
}  /* assign_to_eh_curr_region */


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


static a_variable_ptr make_caught_object_address_var(void)
/*
Make an expression node representing the address of the object 
Make __caught_object_address, a global variable used for exception processing,
if it has not already been made.  Return a pointer to it.
*/
{
  if (caught_object_address_var == NULL) {
    caught_object_address_var =
                             make_lowered_variable("__caught_object_address",
                                                   /*already_il_name=*/FALSE,
                                                   void_star_type(),
                                                   (a_storage_class)sc_extern);
  }  /* if */
  return caught_object_address_var;
}  /* make_caught_object_address_var */


/*
Pointer to the exception_type_spec struct type (used to represent a type
for exception throw and catch specifications).  NULL until created.
*/
static a_type_ptr
		exception_type_spec_type;


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


static a_variable_ptr make_exception_type_spec_array_var(
                                                      a_constant_ptr *aggr_con)
/*
Create a variable whose initial value will be an array of exception type
specification entries, and return a pointer to the variable.  An empty
aggregate constant is attached to the variable as its initial value, and
a pointer to the aggregate constant is returned in *aggr_con.
*/
{
  a_variable_ptr var;

  /* Make a variable that is an array of exception type specification
     entries. */
  var = make_init_unnamed_local_static_array_var(
                                              make_exception_type_spec_type(),
                                              /*in_function_scope=*/FALSE,
                                              aggr_con);
  return var;
}  /* make_exception_type_spec_array_var */


static void add_exception_type_spec_array_entry(a_type_ptr     type,
                                                a_variable_ptr var,
                                                a_constant_ptr aggr_con)
/*
Add an entry that describes the type "type" to the array of exception type
specifications being built up as the initializer of the variable var.
aggr_con points to the top-level aggregate constant that is the initial
value of the variable.  If type is NULL, add an ellipsis entry.
*/
{
  a_variable_ptr typeinfo_var;
  unsigned long  flags_value;
  a_constant_ptr typeinfo_con, flags_con, sub_aggr_con;

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
    set_variable_address_constant(typeinfo_var, typeinfo_con,
                                  /*set_address_taken_flag=*/TRUE);
  }  /* if */
  flags_con = alloc_constant((a_constant_repr_kind)ck_integer);
  set_unsigned_integer_constant(flags_con, flags_value,
                                (an_integer_kind)ik_unsigned_char);
  sub_aggr_con = alloc_constant((a_constant_repr_kind)ck_aggregate);
  sub_aggr_con->variant.aggregate.first_constant = typeinfo_con;
  typeinfo_con->next = flags_con;
  sub_aggr_con->variant.aggregate.last_constant = flags_con;
  /* Add this aggregate constant to the list of constants under the aggregate
     constant for the array. */
  (void)add_elem_to_array_var(sub_aggr_con, var, aggr_con);
}  /* add_exception_type_spec_array_entry */


static void finish_exception_type_spec_array(a_variable_ptr var,
                                             a_constant_ptr aggr_con)
/*
Finish the definition of a variable whose value is an array of exception
type specification entries.  var is the variable, and aggr_con is
the aggregate constant that is its initial value.
*/
{
  a_constant_ptr sub_aggr_con, flags_con;
  unsigned long  flags_value;
  a_boolean      ovflo;

  /* Put the ETS_LAST bit on in the last entry. */
  sub_aggr_con = aggr_con->variant.aggregate.last_constant;
  flags_con = sub_aggr_con->variant.aggregate.last_constant;
  flags_value = unsigned_value_of_integer_constant(flags_con, &ovflo);
  flags_value |= ETS_LAST;
  set_unsigned_integer_value(&flags_con->variant.integer_value, flags_value);
  /* Finish off the variable. */
  finish_array_var(var);
}  /* finish_exception_type_spec_array */


static a_variable_ptr exception_type_spec_array_from_throw_spec(
                                     an_exception_specification_ptr throw_spec)
/*
Make an array that describes the throw specification indicated by throw_spec,
and return a pointer to the variable for the array.  Return NULL if the
throw specification indicates that no types may be thrown.
*/
{
  a_variable_ptr                       var;
  an_exception_specification_type_ptr  espt;
  a_constant_ptr                       aggr_con;

  espt = throw_spec->exception_specification_type_list;
  /* If the routine can throw nothing, return NULL. */
  if (espt == NULL) {
    var = NULL;
  } else {
    /* There are some types on the throw list, so an array of those will
       have to be built. */
    /* Make the variable. */
    var = make_exception_type_spec_array_var(&aggr_con);
    /* Fill the array with entries for the types that can be thrown. */
    for (;
         espt != NULL;
         espt = espt->next) {
      add_exception_type_spec_array_entry(espt->type, var, aggr_con);
    }  /* for */
    /* Finish off the array. */
    finish_exception_type_spec_array(var, aggr_con);
  }  /* if */
  return var;
}  /* exception_type_spec_array_from_throw_spec */


static a_variable_ptr make_catch_array_var(a_handler_ptr handlers)
/*
Generate an exception type specification array to describe the types of the
catch clauses on the indicated list.  Return a pointer to the variable.
*/
{
  a_variable_ptr var;
  a_handler_ptr  handler;
  a_constant_ptr aggr_con;

  /* Make the variable. */
  var = make_exception_type_spec_array_var(&aggr_con);
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
    add_exception_type_spec_array_entry(handler_type, var, aggr_con);
  }  /* for */
  /* Finish off the array. */
  finish_exception_type_spec_array(var, aggr_con);
  return var;
}  /* make_catch_array_var */


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
       an array of elements of some integral or floating type; that's not
       standard, but it's common.  For other cases, one can pick an
       integral kind and number of elements to give the right size and
       alignment. */
    jmp_buf_type = alloc_type((a_type_kind)tk_array);
    jmp_buf_type->variant.array.element_type =
                             targ_jmp_buf_elements_are_float ?
                                  float_type(targ_jmp_buf_element_float_kind) :
                                  integer_type(targ_jmp_buf_element_int_kind);
    jmp_buf_type->variant.array.variant.number_of_elements =
                                                     targ_jmp_buf_num_elements;
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
		ehse_try_rtinfo_field,
		ehse_try_region_number_field,
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
        jmp_buf        setjmp_buffer;       // Buffer for setjmp
        exception_type_spec *catch_entries; // Catch list
        void           *rtinfo;             // Runtime info
        unsigned short region_number;       // Region number at entry
      } try_block;
      struct {
        region_descr   *regions;            // Cleanup regions
        void           **obj_table;         // Object address table
        array_descr    *array_table;        // Array table
        unsigned short saved_region_number; // Saved __eh_curr_region
      } function;
      exception_type_spec *throw_spec;      // Throw spec list
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
    /* field: void *rtinfo */
    make_lowered_field("rtinfo", void_star_type(),
                       &byte_offset, try_block_struct_type, &last_field);
    ehse_try_rtinfo_field = last_field;
    /* field: unsigned short region_number */
    make_lowered_field("region_number",
                       integer_type(TARG_REGION_NUMBER_INT_KIND),
                       &byte_offset, try_block_struct_type, &last_field);
    ehse_try_region_number_field = last_field;
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


static void pop_eh_stack_frame(an_eh_stack_entry_kind kind,
                               a_variable_ptr         stack_frame,
                               an_insert_location     *insert_location)
/*
Generate code to pop an exception handling stack frame off the stack.
kind indicates the kind of stack frame.  *stack_frame points to a
local temporary variable that holds the stack frame.  The code is
inserted at *insert_location.
*/
{
  an_expr_node_ptr local_frame_next, stack_frame_function_saved_region_number;

  /* If this is a function stack frame, restore __eh_curr_region from
     the stack frame. */
  if (kind == ehsek_function) {
    stack_frame_function_saved_region_number = 
                  field_rvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(stack_frame),
                                                  ehse_variant_field),
                      ehse_function_field),
                    ehse_function_saved_region_number_field);
    /* Copy stack_frame.variant.function.saved_region_number into
       the global variable __eh_curr_region. */
    assign_to_eh_curr_region(stack_frame_function_saved_region_number,
                             insert_location);
  }  /* if */
  /* Add __curr_eh_stack_entry = local_frame.next; */
  local_frame_next = field_rvalue_selection_expr(var_lvalue_expr(stack_frame),
                                                 ehse_next_field);
  (void)insert_var_assignment_statement(curr_eh_stack_entry_var,
                                        (an_expr_operator_kind)eok_passign,
                                        local_frame_next,
                                        insert_location);
}  /* pop_eh_stack_frame */

#endif /* DO_FULL_PORTABLE_EH_LOWERING */

void add_eh_function_prologue(a_scope_ptr scope)
/*
Add any prologue needed for exception handling to the function whose scope
is given by "scope".  Called only if exceptions are enabled.  This is
done late so that it gets inserted before any code inserted at the
beginning of the function.  Epilogue code is also inserted at each return
statement if necessary.
*/
{
  a_routine_ptr             routine;
  an_insert_location        insert_location;
  a_boolean                 need_function_epilogue = FALSE;
  a_return_memo_ptr         rmp;
#if DO_FULL_PORTABLE_EH_LOWERING
  a_type_ptr                routine_type, spec_array_ptr;
  an_exception_specification_ptr
                            tsp;
  a_variable_ptr            throw_frame, func_frame, spec_array_var;
  an_expr_node_ptr          spec_array_node, throw_frame_throw_spec;
  an_expr_node_ptr          func_frame_function_regions;
  an_expr_node_ptr          func_frame_function_obj_table;
  an_expr_node_ptr          func_frame_function_array_table;
  an_expr_node_ptr          func_frame_function_saved_region_number;
  a_boolean                 need_throw_epilogue = FALSE;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

  /* The insert location for the statements is the start of the top block of
     the routine. */
  set_block_start_insert_location(scope->assoc_block, &insert_location);
  routine = scope->variant.routine.ptr;
#if DO_FULL_PORTABLE_EH_LOWERING
  routine_type = routine->type;
  routine_type = skip_typerefs(routine_type);
  /* See if the routine has a throw specification. */
  tsp = routine_type->variant.routine.extra_info->exception_specification;
  if (tsp != NULL) {
    /* The routine has a throw specification.  (A null pointer means
       the function can throw anything.) */
    /* Generate code to push an entry on the EH stack. */
    push_eh_stack_frame(ehsek_throw_spec, &throw_frame, &insert_location);
    need_throw_epilogue = TRUE;
    /* Build an array of the throw types. */
    spec_array_var = exception_type_spec_array_from_throw_spec(tsp);
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
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  if (scope->lifetime != NULL || routine->contains_try_block) {
    /* The function contains destructible objects, or it contains try
       blocks, so it needs a prologue and epilogue. */
    need_function_epilogue = TRUE;
#if DO_FULL_PORTABLE_EH_LOWERING
    /* Generate code to push an entry on the EH stack. */
    push_eh_stack_frame(ehsek_function, &func_frame, &insert_location);
    /* Finish off the various arrays and put pointers to them into the
       stack. */
    if (region_table_var != NULL) {
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
    }  /* if */
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
    /* Generate an assignment to save __eh_curr_region in the stack. */
    /* Make an expression for
       throw_frame.variant.function.saved_region_number */
    func_frame_function_saved_region_number = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(func_frame),
                                                  ehse_variant_field),
                      ehse_function_field),
                    ehse_function_saved_region_number_field);
    /* Copy the global variable __eh_curr_region into
       func_frame.variant.function.saved_region_number */
    (void)insert_assignment_statement(func_frame_function_saved_region_number,
                                      (an_expr_operator_kind)eok_iassign,
                                      var_rvalue_expr(
                                                    make_eh_curr_region_var()),
                                      &insert_location);
    /* Reset __eh_curr_region to null_eh_region_number. */
    (void)insert_var_assignment_statement(eh_curr_region_var,
                                          (an_expr_operator_kind)eok_iassign,
                                          node_for_integer_constant(
                                                 (long)null_eh_region_number,
                                                 TARG_REGION_NUMBER_INT_KIND),
                                          &insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
    /* Non-portable schemes: generate an enk_lowered_eh_construct/
       leck_routine_prologue expression node. */
  { an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                          (a_lowered_eh_construct_kind)leck_function_prologue);
    an_eh_prologue_supplement_ptr psp =
                                node->variant.lowered_eh.variant.prologue_info;
    psp->routine = routine;
#if GENERATE_EH_TABLES
    psp->region_table = region_table_var;
    psp->array_table = array_table_var;
#endif /* GENERATE_EH_TABLES */
    (void)insert_expr_statement(node, &insert_location);
  }
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  }  /* if */
  if (need_function_epilogue
#if DO_FULL_PORTABLE_EH_LOWERING
      || need_throw_epilogue
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
                            ) {
    /* Need to add epilogue code at each return in the routine. */
    for (rmp = return_memo_list; rmp != NULL; rmp = rmp->next) {
      /* Turn the return into a block. */
      turn_branch_into_block(rmp->stmt, &insert_location, &rmp->stmt);
      if (need_function_epilogue) {
#if DO_FULL_PORTABLE_EH_LOWERING
        /* Insert code to pop the prologue pushed for the function. */
        pop_eh_stack_frame(ehsek_function, func_frame, &insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
        /* Insert an enk_lowered_eh_construct node to represent the
           function epilogue. */
        an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                          (a_lowered_eh_construct_kind)leck_function_epilogue);
        node->variant.lowered_eh.variant.epilogue_routine = routine;
        (void)insert_expr_statement(node, &insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
      }  /* if */
#if DO_FULL_PORTABLE_EH_LOWERING
      if (need_throw_epilogue) {
        /* Insert code to pop the prologue pushed for the throw
           specification. */
        pop_eh_stack_frame(ehsek_throw_spec, throw_frame, &insert_location);
      }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
    }  /* for */
  }  /* if */
}  /* add_eh_function_prologue */

#if !DO_FULL_PORTABLE_EH_LOWERING

static a_handler_ptr current_catch_handler(void)
/*
Find and return the address of the handler for the current catch clause.
*/
{
  a_context_ptr context;
  a_handler_ptr handler = NULL;

  for (context = curr_context; context != NULL; context = context->parent) {
    a_scope_ptr scope = context->scope;
    if (scope->kind == (a_scope_kind)sck_block) {
      handler = scope->variant.assoc_handler;
      if (handler != NULL) break;
    }  /* if */
  }  /* for */
  check_assertion(handler != NULL);
  return handler;
}  /* current_catch_handler */

#endif /* !DO_FULL_PORTABLE_EH_LOWERING */

#if !DO_FULL_PORTABLE_EH_LOWERING
/*ARGSUSED*/ /* <-- param_type is not used. */
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
an_expr_node_ptr make_caught_object_address_node(a_type_ptr param_type)
/*
Make an expression node for the address of the object caught at the
currently active catch clause, and return a pointer to the node.
param_type is the type of the catch parameter.
*/
{
  an_expr_node_ptr source_node;
#if DO_FULL_PORTABLE_EH_LOWERING
  /* In the portable scheme, use the global variable
     __caught_object_address. */
  a_variable_ptr   caught_object_addr = make_caught_object_address_var();

  if (is_reference_type(param_type)) {
    /* Initializing a reference parameter, so copy the pointer into
       the parameter, instead of copying the object pointed to. */
    source_node = var_lvalue_expr(caught_object_addr);
   } else {
    /* Normal case (not a reference). */
    source_node = var_rvalue_expr(caught_object_addr);
  }  /* if */
#else /* DO_FULL_PORTABLE_EH_LOWERING */
  /* In other schemes, use an enk_lowered_eh_construct/
     leck_caught_object_address expression node. */
  source_node = alloc_lowered_eh_construct_node(
                      (a_lowered_eh_construct_kind)leck_caught_object_address);
  source_node->type = void_star_type();
  source_node->variant.lowered_eh.variant.caught_object_handler =
                                                       current_catch_handler();
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  return source_node;
}  /* make_caught_object_address_node */


void begin_catch_clause(a_handler_ptr handler)
/*
Generate code for the start of a catch clause.  The current context is
for the scope of the handler.
*/
{
  an_init_pos_descr  ipd;
  a_boolean          keep_dynamic_init;
  an_insert_location insert_location;

  if (handler->parameter != NULL) {
    /* Insert code to initialize the catch clause parameter from the
       runtime copy of the thrown object. */
    set_block_start_insert_location(handler->statement, &insert_location);
    set_var_init_pos_descr(handler->parameter, &ipd);
    lower_dynamic_init(handler->dynamic_init, &ipd,
                       (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                       (a_constructor_init_ptr)NULL,
                       &insert_location, &keep_dynamic_init);
    check_assertion(keep_dynamic_init == FALSE);
    /* Mark the parameter as referenced. */
    handler->parameter->source_corresp.referenced = TRUE;
  }  /* if */
}  /* begin_catch_clause */


/*ARGSUSED*/ /* <-- try_block is not used in the portable mode. */
             /*     context_ptr is not used in the other modes. */
void cleanup_on_exit_from_try_block(a_context_ptr        context_ptr,
                                    a_try_supplement_ptr try_block,
                                    an_insert_location   *insert_location)
/*
Generate any cleanup required on exit from a try block.  context_ptr points to
the context for the try block.  Any code generated is inserted at
*insert_location.
*/
{
#if DO_FULL_PORTABLE_EH_LOWERING
  /* In the portable scheme, pop the stack frame for the try block. */
  pop_eh_stack_frame(ehsek_try_block, context_ptr->try_frame, insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* In other schemes, insert an enk_lowered_eh_construct/leck_try_epilogue
     expression node. */
  an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                               (a_lowered_eh_construct_kind)leck_try_epilogue);
  node->variant.lowered_eh.variant.epilogue_try_block = try_block;
  (void)insert_expr_statement(node, insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
}  /* cleanup_on_exit_from_try_block */


#if DO_FULL_PORTABLE_EH_LOWERING
/*
Pointer to the routine entry for the runtime routine __free_thrown_object.
NULL until created.
*/
static a_routine_ptr
		free_thrown_object_routine;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */


#if DO_FULL_PORTABLE_EH_LOWERING
/*ARGSUSED*/ /* <-- handler is not used in the portable mode. */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
void cleanup_on_exit_from_catch(a_handler_ptr      handler,
                                an_insert_location *insert_location)
/*
Generate any cleanup required on exit from a catch clause.  Any code
generated is inserted at insert_location.
*/
{
#if DO_FULL_PORTABLE_EH_LOWERING
  /* Portable scheme: */
  /* Make a call of the runtime routine __free_thrown_object.  This tells
     the runtime it can now destroy the caught object and free the space
     for it. */
  make_call_statement(make_runtime_routine("__free_thrown_object",
                                           &free_thrown_object_routine,
                                           void_type()),
                      (an_expr_node_ptr)NULL,
                      insert_location);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* In other schemes, insert an enk_lowered_eh_construct/leck_catch_epilogue
     expression node. */
  an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                             (a_lowered_eh_construct_kind)leck_catch_epilogue);
  node->variant.lowered_eh.variant.epilogue_handler = handler;
  (void)insert_expr_statement(node, insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
}  /* cleanup_on_exit_from_catch */

#if DO_FULL_PORTABLE_EH_LOWERING
#if FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS

static void add_var_addr_to_list_if_modified_in_try_block(
                                                    a_variable_ptr   var,
                                                    an_expr_node_ptr *arg_list)
/*
If the indicated variable is modified in a try block, add an expression node
for the address of the variable at the front of the indicated expression list.
This is used in a call that convinces optimizers that these variables
must be stored out when modified.
*/
{
  an_expr_node_ptr arg;
  a_constant       constant;

  if (var->modified_within_try_block) {
    set_variable_address_constant(var, &constant,
                                  /*set_address_taken_flag=*/TRUE);
    arg = alloc_node_for_constant(&constant);
    arg->next = *arg_list;
    *arg_list = arg;
  }  /* if */
}  /* add_var_addr_to_list_if_modified_in_try_block */

#endif /* FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#if DO_FULL_PORTABLE_EH_LOWERING
/*
Pointers to the routine entries for the runtime routines setjmp and 
__suppress_optim_on_vars_in_try.  NULL until created.
*/
static a_routine_ptr
		setjmp_routine,
		suppress_optim_on_vars_in_try_routine;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */


void lower_try_block(a_statement_ptr statement)
/*
Do IL lowering for an stmk_try_block statement.
*/
{
  a_try_supplement_ptr
                     tsp = statement->variant.try_block;
  a_statement_ptr    stmt_to_try = tsp->statement;
  a_statement_ptr    orig_stmt, orig_stmt_to_try;
  a_handler_ptr      handlers = tsp->handlers, handler;
  an_insert_location insert_location;
  a_context          context;
  an_object_lifetime_ptr
                     lifetime;
#if DO_FULL_PORTABLE_EH_LOWERING
  a_variable_ptr     try_frame, catch_array_var;
  an_expr_node_ptr   try_frame_catch_entries, try_frame_setjmp_buffer;
  an_expr_node_ptr   try_frame_rtinfo, try_frame_region_number;
  an_expr_node_ptr   setjmp_call, compare_node, catch_clause_number_node;
  a_statement_ptr    prev_if_stmt, if_stmt;
  long               catch_clause_number;
  a_constant         null_constant;
#if FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS
  a_label_ptr        label;
#endif /* FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

#if DO_FULL_PORTABLE_EH_LOWERING
  /* Change the stmk_try_block statement into a block, and prepare to insert
     code at the start of the block. */
  turn_statement_into_block(statement, &insert_location, &orig_stmt);
  /* Generate code to push a stack frame. */
  push_eh_stack_frame(ehsek_try_block, &try_frame, &insert_location);
  orig_stmt_to_try = stmt_to_try;
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
  /* In the non-portable schemes, we keep the original "try" statement. */
  orig_stmt = statement;
  turn_statement_into_block(stmt_to_try, &insert_location, &orig_stmt_to_try);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  /* Push a context around the try and catch.  This is needed to ensure that
     the "try" stack frame is popped on a goto out of the try or catch. */
  lifetime = tsp->lifetime;
  push_context(&context, (a_scope_ptr)NULL, lifetime);
#if DO_FULL_PORTABLE_EH_LOWERING
  curr_context->try_frame = try_frame;
  if (keep_object_lifetime_info_in_lowered_il) {
    /* To keep the object lifetime when the try block is eliminated,
       attach the object lifetime to the block generated above. */
    unbind_object_lifetime(lifetime);
    bind_object_lifetime(lifetime, iek_block,
                         (char *)statement->variant.block.extra_info);
  }  /* if */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  begin_object_lifetime(lifetime, &insert_location);
#if DO_FULL_PORTABLE_EH_LOWERING
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
  /* Set the rtinfo field (which points to runtime information) to NULL. */
  try_frame_rtinfo = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(try_frame),
                                                  ehse_variant_field),
                      ehse_try_field),
                    ehse_try_rtinfo_field);
  make_zero_of_proper_type(void_star_type(), &null_constant);
  (void)insert_assignment_statement(try_frame_rtinfo,
                                    (an_expr_operator_kind)eok_passign,
                                    alloc_node_for_constant(&null_constant),
                                    &insert_location);
  /* Set the region_number field to the region number at entry to the try
     block.  This tells the runtime where to stop the cleanup process to
     end the "try" but not things in the surrounding function. */
  try_frame_region_number = 
                  field_lvalue_selection_expr(
                    field_lvalue_selection_expr(
                      field_lvalue_selection_expr(var_lvalue_expr(try_frame),
                                                  ehse_variant_field),
                      ehse_try_field),
                    ehse_try_region_number_field);
  (void)insert_assignment_statement(try_frame_region_number,
                                    (an_expr_operator_kind)eok_iassign,
                                    node_for_integer_constant(
                                                  (long)cleanup_region_number(
                                                           curr_cleanup_state),
                                                  TARG_REGION_NUMBER_INT_KIND),
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
                   make_pointer_type(array_element_type(make_jmp_buf_type())));
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
  set_statement_kind(orig_stmt, (a_statement_kind)stmk_if);
  orig_stmt->expr = compare_node;
  /* The dependent statement is the statement under the "try". */
  orig_stmt->variant.if_stmt.then_statement = orig_stmt_to_try;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  /* Lower the dependent statement of the try. */
  lower_statement(orig_stmt_to_try);
#if DO_FULL_PORTABLE_EH_LOWERING
#if FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS
  /* Add a label inside the dependent statement to defeat optimization.
     See comment below. */
  { a_statement_ptr label_stmt = alloc_statement((a_statement_kind)stmk_label);
    label = alloc_label();
    /* Add the label to the front of the function scope list. */
    label->next = innermost_function_scope->labels;
    innermost_function_scope->labels = label;
    label_stmt->variant.label.ptr = label;
    label->variant.exec_stmt = label_stmt;
    /* Add the label statement at the start of the try compound statement. */
    check_assertion(orig_stmt_to_try->kind == (a_statement_kind)stmk_block);
    label_stmt->next = orig_stmt_to_try->variant.block.statements;
    orig_stmt_to_try->variant.block.statements = label_stmt;
    mark_stmk_inits_as_following_exec_statement(label_stmt->next);
  }
#endif /* FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
  catch_clause_number = 0;
  prev_if_stmt = orig_stmt;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  /* Walk through the catch clauses. */
  for (handler = handlers;
       handler != NULL;
       handler = handler->next) {
    a_statement_ptr dep_statement = handler->statement;
#if DO_FULL_PORTABLE_EH_LOWERING
    catch_clause_number++;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
    /* Lower the dependent statement of the catch clause. */
    /* Note that the code to initialize the parameter (if there is one)
       is generated during the lowering of the dependent statement. */
    lower_statement(dep_statement);
#if DO_FULL_PORTABLE_EH_LOWERING
#if !FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS
    /* The special processing for the ellipsis entry is not done if we want
       to add an "else" at the end to suppress optimization. */
    if (handler->parameter == NULL) {
      /* This is an ellipsis entry.  No "if" is required, since it accepts
         any type.  Previous error checks have ensured that this is the
         last clause. */
      check_assertion_str(handler->next == NULL,
                          "lower_try_block: ellipsis clause not last");
      prev_if_stmt->variant.if_stmt.else_statement = dep_statement;
    } else {
#endif /* !FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
      /* Test the catch clause number returned by the runtime in an "if"
         statement:
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
      if_stmt->position = handler->catch_position;
      if_stmt->expr = compare_node;
      if_stmt->variant.if_stmt.then_statement = dep_statement;
      prev_if_stmt->variant.if_stmt.else_statement = if_stmt;
      prev_if_stmt = if_stmt;
#if !FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS
    }  /* if */
#endif /* !FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
    /* Clear the assoc_handler pointer in the handler scope because it's not
       a C field. */
    handler->statement->variant.block.extra_info->assoc_scope->
                                                  variant.assoc_handler = NULL;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  }  /* for */
#if DO_FULL_PORTABLE_EH_LOWERING
#if FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS
  /* Add an unreachable "else" at the end of the "if".  It contains a call
     followed by a goto that look like
       suppress_optim_on_vars_modified_in_try(&x, &y, &z);
       goto start_of_try;
     This convinces C compilers that the indicated variables must be stored
     out when modified in the try block. */
  { a_statement_ptr  block_stmt, call_stmt, goto_stmt;
    an_expr_node_ptr arg_list = NULL, call_node;
    a_context_ptr    context;
    a_variable_ptr   var;
    /* Work up through the context stack from the current location (the
       try block) out to the function scope.  At each scope, look for local
       variables that are modified within the try and add them to the
       argument list for the call. */
    context = curr_context;
    do {
      context = context->parent;
      for (var = context->scope->variables;
           var != NULL;
           var = var->next) {
        add_var_addr_to_list_if_modified_in_try_block(var, &arg_list);
      }  /* for */
      for (var = context->scope->nonstatic_variables;
           var != NULL;
           var = var->next) {
        add_var_addr_to_list_if_modified_in_try_block(var, &arg_list);
      }  /* for */
    } while (context->scope != innermost_function_scope);
    /* The extra code is needed only if there are such modified variables. */
    if (arg_list != NULL) {
      call_node = make_runtime_rout_call("__suppress_optim_on_vars_in_try",
                                        &suppress_optim_on_vars_in_try_routine,
                                         void_type(),
                                         arg_list);
      call_stmt = alloc_expr_statement(call_node);
      /* Add a block statement as the "else" of the last "if" for a catch
         handler. */
      block_stmt = alloc_statement((a_statement_kind)stmk_block);
      prev_if_stmt->variant.if_stmt.else_statement = block_stmt;
      /* Put the call into the block. */
      block_stmt->variant.block.statements = call_stmt;
      /* Put the goto following the call. */
      goto_stmt = alloc_statement((a_statement_kind)stmk_goto);
      goto_stmt->variant.label.ptr = label;
      call_stmt->next = goto_stmt;
    }  /* if */
  }
#endif /* FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  /* Generate code to pop the "try" frame off the stack after the rewritten
     "if" statement. */
  set_insert_location(orig_stmt, &insert_location);
  gen_cleanup_actions(lifetime, &insert_location);
  /* Pop the context pushed around the try block. */
  pop_context();
}  /* lower_try_block */


#if DO_FULL_PORTABLE_EH_LOWERING

/* Routines to build an access string for a throw, using the temp_text
   buffer. */

static sizeof_t	curr_size_access_string_buffer;
			/* Number of characters currently used in
			   temp_text_buffer. */


static void add_char_to_access_string_buffer(char ch)
/*
Add the indicated character to the temp_text_buffer.
*/
{
  ensure_temp_text_buffer_space(curr_size_access_string_buffer+1);
  temp_text_buffer[curr_size_access_string_buffer++] = ch;
}  /* add_char_to_access_string_buffer */


static void add_to_throw_access_string(a_type_ptr                   type,
                                       an_accessible_base_class_ptr abcp,
                                       a_base_class_ptr             parent_bcp)
/*
Generate part of the access control string for a throw.  type is a
class type to be processed.  abcp is the list of accessible base classes
from the throw expression.  parent_bcp is the base class pointer for the
immediate parent of this base class as we descend recursively, or NULL
if there is no parent.
*/
{
  a_base_class_ptr bcp, throw_bcp;
  a_type_ptr       throw_class = abcp->base_class->derived_class;
  char             ch;
  an_accessible_base_class_ptr
                   temp_abcp;

  /* Go through the direct and virtual base classes (the same ones listed
     in the typeinfo base class list).  Do the top level first before
     looking at base classes of base classes because we want to find
     virtual base classes at the top. */
  for (bcp = type->variant.class_struct_union.extra_info->base_classes;
       bcp != NULL;
       bcp = bcp->next) {
   if (bcp->direct || bcp->is_virtual) {
      /* Add "Y" if this base class is on the list of accessible base classes,
         "N" otherwise.  Ambiguous classes are considered inaccessible. */
      ch = 'N';
      if (!bcp->ambiguous) {
        /* Find the base class entry on the throw type that corresponds to the
           base class we are examining here. */
        throw_bcp = corresponding_base_class(bcp, throw_class, parent_bcp);
        /* See if the base class is on the list of accessible base classes. */
        for (temp_abcp = abcp;
             temp_abcp != NULL;
             temp_abcp = temp_abcp->next) {
          if (temp_abcp->base_class == throw_bcp) {
            /* Yes, the base class is accessible. */
            ch = 'Y';
            break;
          }  /* if */
        }  /* if */
      }  /* if */
      add_char_to_access_string_buffer(ch);
    }  /* if */
  }  /* for */
  /* Go through the base classes of the direct and virtual base classes. */
  for (bcp = type->variant.class_struct_union.extra_info->base_classes;
       bcp != NULL;
       bcp = bcp->next) {
    if (bcp->direct || bcp->is_virtual) {
      /* Find the base class entry on the throw type that corresponds to the
         base class we are examining here. */
      throw_bcp = corresponding_base_class(bcp, throw_class, parent_bcp);
      add_to_throw_access_string(bcp->type, abcp, throw_bcp);
    }  /* if */
  }  /* for */
}  /* add_to_throw_access_string */


static a_constant_ptr make_throw_access_string(an_expr_node_ptr expr)
/*
expr is a throw expression node.  If necessary, generate a string
constant to describe the accessible base classes of the thrown type and
return a pointer to it.  The constant is shareable.  If no constant
is needed, return NULL.
*/
{
  a_type_ptr                   type;
  an_accessible_base_class_ptr abcp;
  a_constant_ptr               string_con = NULL;
  a_constant                   constant;
  char                         *pstr;

  /* No constant is needed if there are no accessible base classes
     (that includes the case where the type involved is not a class type). */
  abcp = expr->variant.throw_info->accessible_base_classes;
  if (abcp != NULL) {
    /* A string is needed.  Get the class involved in the throw. */
    type = abcp->base_class->derived_class;
    /* Start with an empty string. */
    curr_size_access_string_buffer = 0;
    /* Build the string. */
    add_to_throw_access_string(type, abcp, (a_base_class_ptr)NULL);
    /* Put a terminating null on the string. */
    add_char_to_access_string_buffer('\0');
    /* Build the string constant. */
    pstr = alloc_text_of_string_literal(curr_size_access_string_buffer);
    (void)strcpy(pstr, temp_text_buffer);
    clear_constant(&constant, (a_constant_repr_kind)ck_string);
    constant.type = string_type((a_targ_size_t)curr_size_access_string_buffer);
    constant.variant.string.length = curr_size_access_string_buffer;
    constant.variant.string.value  = pstr;
    string_con = alloc_shareable_constant(&constant);
  }  /* if */
  return string_con;
}  /* make_throw_access_string */


/*
Pointers to routine entries for the runtime routines __throw_alloc,
__throw, and __rethrow, used in throwing exceptions.  NULL until allocated.
*/
static a_routine_ptr
		throw_alloc_routine,
		throw_routine,
		rethrow_routine;

#endif /* DO_FULL_PORTABLE_EH_LOWERING */

#if !DO_FULL_PORTABLE_EH_LOWERING

an_expr_node_ptr make_thrown_object_address_node(void)
/*
Make an expression node that represents the address in the runtime to which
a thrown object should be copied, and return a pointer to the node.
*/
{
  an_expr_node_ptr node = alloc_lowered_eh_construct_node(
                      (a_lowered_eh_construct_kind)leck_thrown_object_address);
  node->type = void_star_type();
  return node;
}  /* make_thrown_object_address_node */

#endif /* !DO_FULL_PORTABLE_EH_LOWERING */

void lower_throw(an_expr_node_ptr expr)
/*
Lower an enk_throw expression node.
*/
{
  a_type_ptr         throw_type;
  a_dynamic_init_ptr dip;
  an_init_pos_descr  ipd;
  an_insert_location insert_location;
  a_boolean          keep_dynamic_init;
  a_throw_supplement_ptr
                     tsp = expr->variant.throw_info;
#if DO_FULL_PORTABLE_EH_LOWERING
  an_expr_node_ptr   temp_node;
  a_type_ptr         ptr_throw_type;
  a_variable_ptr     temp_var, typeinfo_var;
  an_expr_node_ptr   call_node, typeinfo_node, size_node, flags_node;
  an_expr_node_ptr   access_node, assign_node;
  unsigned long      flags_value;
  a_constant         access_con;
  a_constant_ptr     string_con;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

  /* Check for a throw with no operand, i.e., a rethrow. */
  if (tsp == NULL) {
#if DO_FULL_PORTABLE_EH_LOWERING
    /* This is a rethrow.  Replace the enk_throw node with a call of
       __rethrow. */
    call_node = make_runtime_rout_call("__rethrow", &rethrow_routine,
                                       void_type(), (an_expr_node_ptr)NULL);
    overwrite_node(expr, call_node);
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
    /* No lowering required in the non-portable schemes. */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  } else {
    /* Throw of an object. */
    throw_type = tsp->type;
    lower_os_type(throw_type);
    throw_type = f_skip_typerefs(throw_type);  /* Probably unnecessary. */
    dip = tsp->dynamic_init;
    /* There should be no destructor indicated, because the runtime handles
       the destruction. */
    check_assertion(dip->destructor == NULL);
#if DO_FULL_PORTABLE_EH_LOWERING
    /* Make the assignment
         temp = __throw_alloc(&typeinfo, size, flags, access)
       This allocates the space into which the thrown object is copied and
       sets temp to point to that space.  typeinfo is the typeinfo variable
       for the base type of the type thrown; size is the size in bytes of
       the type thrown; flags has the ETS_IS_POINTER bit set to indicate
       that a pointer to the typeinfo type is being thrown; and access
       is non-NULL when throwing a class -- it is a character string
       indicating which of the base classes are accessible. */
    ptr_throw_type = make_pointer_type(throw_type);
    temp_var = make_lowered_temporary(ptr_throw_type);
    /* Make the typeinfo variable for the throw type. */
    typeinfo_var = typeinfo_var_for_type(throw_type, &flags_value);
    /* Make the arguments for the __throw_alloc call. */
    typeinfo_node = var_lvalue_expr(typeinfo_var);
    size_node = node_for_integer_constant((long)throw_type->size,
                                          targ_size_t_int_kind);
    typeinfo_node->next = size_node;
    flags_node = node_for_integer_constant((long)flags_value,
                                           (an_integer_kind)ik_int);
    size_node->next = flags_node;
    /* Make a string to describe the accessible base classes, and pass
       its address to the runtime routine. */
    string_con = make_throw_access_string(expr);
    if (string_con == NULL) {
      /* No access string.  Use a NULL pointer. */
      make_zero_of_proper_type(char_star_type(), &access_con);
    } else {
      set_constant_address_constant(string_con, &access_con);
      implicit_cast(&access_con, char_star_type());
    }  /* if */
    access_node = alloc_node_for_constant(&access_con);
    flags_node->next = access_node;
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
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
    /* For the non-portable schemes, we want to lower the dynamic
       initialization, with any references to the destination address
       represented by an enk_lowered_eh_construct/leck_thrown_object_address
       expression node. */
    set_thrown_object_init_pos_descr(throw_type, &ipd);
    set_expr_creation_insert_location(&insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
    /* Generate code to copy the thrown expression to the runtime. */
    lower_dynamic_init(dip, &ipd,
                       (an_expr_node_ptr)NULL, (an_expr_node_ptr)NULL,
                       (a_constructor_init_ptr)NULL,
                       &insert_location, &keep_dynamic_init);
    check_assertion(!keep_dynamic_init);
#if !DO_FULL_PORTABLE_EH_LOWERING
    /* Put the lowered node pointer into the throw supplement. */
    tsp->expr = insert_location.variant.expr;
    tsp->dynamic_init = NULL;
#endif /* !DO_FULL_PORTABLE_EH_LOWERING */
  }  /* if */
}  /* lower_throw */


void set_curr_cleanup_state(a_dynamic_init_ptr cleanup_state,
                            an_insert_location *insert_location)
/*
Set curr_cleanup_state (the cleanup state that applies at the current
location in the program) to cleanup_state, and generate code at
*insert_location to record that information.
*/
{
  an_expr_node_ptr node;

  curr_cleanup_state = cleanup_state;
#if DO_FULL_PORTABLE_EH_LOWERING
  /* In the portable scheme, assign the region number to __eh_curr_region. */
  node = node_for_integer_constant((long)cleanup_region_number(cleanup_state),
                                   TARG_REGION_NUMBER_INT_KIND);
  assign_to_eh_curr_region(node, insert_location);
#else /* DO_FULL_PORTABLE_EH_LOWERING */
  /* In the other schemes, generate an enk_lower_eh_construct/
     leck_cleanup_state expression node. */
  node = alloc_lowered_eh_construct_node(
                              (a_lowered_eh_construct_kind)leck_cleanup_state);
#if GENERATE_EH_TABLES
  /* With partial lowering, the region number is put into the node. */
  node->variant.lowered_eh.variant.cleanup_region_number =
                                          cleanup_region_number(cleanup_state);
#else /* GENERATE_EH_TABLES */
  /* With no lowering, a pointer to the dynamic initialization entry is put
     into the node. */
  node->variant.lowered_eh.variant.cleanup_ptr = cleanup_state;
#endif /* GENERATE_EH_TABLES */
  (void)insert_expr_statement(node, insert_location);
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
}  /* set_curr_cleanup_state */


void eh_function_lower_init(void)
/*
Initialize static variables needed on a per-function basis for
IL lowering for exceptions.
*/
{
#if DO_FULL_PORTABLE_EH_LOWERING
  object_addr_table_var = NULL;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#if GENERATE_EH_TABLES
  array_table_var = NULL;
  array_table_aggr_con = NULL;
  region_table_var = NULL;
  region_table_aggr_con = NULL;
  next_avail_region_number = 0;
#endif /* GENERATE_EH_TABLES */
  curr_cleanup_state = NULL;
}  /* eh_function_lower_init */


void eh_lower_one_time_init(void)
/*
Do one-time initialization of variables related to lowering of structures
involved in exception handling.  (Variables that need to be reinitialized
with each new translation unit are handled in eh_lower_init.)
*/
{
  /* Save variables from lower_eh.h and lower_eh.c that are needed for
     precompiled headers */
  if (exceptions_enabled && precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
#if GENERATE_EH_TABLES
      pch_saved_var_array_elem(typeinfo_type),
      pch_saved_var_array_elem(num_of_pending_class_typeinfo_vars),
      pch_saved_var_array_elem(base_class_spec_type),
      pch_saved_var_array_elem(region_descr_type),
      pch_saved_var_array_elem(array_descr_type),
#endif /* GENERATE_EH_TABLES */
#if DO_FULL_PORTABLE_EH_LOWERING
      pch_saved_var_array_elem(throw_alloc_routine),
      pch_saved_var_array_elem(throw_routine),
      pch_saved_var_array_elem(rethrow_routine),
      pch_saved_var_array_elem(jmp_buf_type),
      pch_saved_var_array_elem(exception_type_spec_type),
      pch_saved_var_array_elem(eh_stack_entry_type),
      pch_saved_var_array_elem(ehse_function_array_table_field),
      pch_saved_var_array_elem(ehse_function_field),
      pch_saved_var_array_elem(ehse_function_obj_table_field),
      pch_saved_var_array_elem(ehse_function_regions_field),
      pch_saved_var_array_elem(ehse_function_saved_region_number_field),
      pch_saved_var_array_elem(ehse_kind_field),
      pch_saved_var_array_elem(ehse_next_field),
      pch_saved_var_array_elem(ehse_throw_spec_field),
      pch_saved_var_array_elem(ehse_try_catch_entries_field),
      pch_saved_var_array_elem(ehse_try_field),
      pch_saved_var_array_elem(ehse_try_region_number_field),
      pch_saved_var_array_elem(ehse_try_rtinfo_field),
      pch_saved_var_array_elem(ehse_try_setjmp_buffer_field),
      pch_saved_var_array_elem(ehse_variant_field),
      pch_saved_var_array_elem(eh_curr_region_var),
      pch_saved_var_array_elem(curr_eh_stack_entry_var),
      pch_saved_var_array_elem(catch_clause_number_var),
      pch_saved_var_array_elem(caught_object_address_var),
      pch_saved_var_array_elem(setjmp_routine),
      pch_saved_var_array_elem(suppress_optim_on_vars_in_try_routine),
      pch_saved_var_array_elem(free_thrown_object_routine),
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* eh_lower_one_time_init */


void eh_lower_init(void)
/*
Initialize static variables related to IL lowering of exceptions.
This is done as a subroutine (rather than relying on static initialization)
so that it can be redone to compile more than one source file in a single
invocation of the front end.
*/
{
  /* Static variables in lower_eh.c: */
#if GENERATE_EH_TABLES
  typeinfo_type = NULL;
  num_of_pending_class_typeinfo_vars = 0;
  base_class_spec_type = NULL;
  region_descr_type = NULL;
  array_descr_type = NULL;
#endif /* GENERATE_EH_TABLES */
#if DO_FULL_PORTABLE_EH_LOWERING
  throw_alloc_routine = NULL;
  throw_routine = NULL;
  rethrow_routine = NULL;
  jmp_buf_type = NULL;
  exception_type_spec_type = NULL;
  eh_stack_entry_type = NULL;
  eh_curr_region_var = NULL;
  curr_eh_stack_entry_var = NULL;
  catch_clause_number_var = NULL;
  caught_object_address_var = NULL;
  setjmp_routine = NULL;
  suppress_optim_on_vars_in_try_routine = NULL;
  free_thrown_object_routine = NULL;
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
  /* Variables in lower_eh.h: */
#if GENERATE_EH_TABLES
  /* Make a constant for the maximum region number, also used for the
     null region number.  */
  { a_targ_size_t    size;
    a_targ_alignment align;
    /* Find out how big a field is used for region numbers. */
    get_integer_size_and_alignment(TARG_REGION_NUMBER_INT_KIND, &size, &align);
    size = size * targ_char_bit; /* Not "*=" to avoid CodeCenter bug. */
    /* Make a bit mask "size" bits long. */
    if (size >= sizeof(unsigned long)*CHAR_BIT) {
      null_eh_region_number = ~(unsigned long)0;
    } else {
      null_eh_region_number = ((unsigned long)1 << size) - 1;
    }  /* if */
  }
#endif /* GENERATE_EH_TABLES */
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
