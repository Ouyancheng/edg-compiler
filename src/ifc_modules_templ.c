/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2017-2022 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

ifc_modules_templ.c -- Microsoft-specific IFC module code template code that's
                       shared between ifc_modules.c, ifc_map_functions.c,
                       ifc_map_functions_acc.c, and ifc_map_functions_val.c.

*/

#include "basic_hdrs.h"
#include "fe_common.h"
#include "ifc_modules.h"
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ifc_map_functions.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED && !STANDALONE_UTILITY_PROGRAM

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE


template<typename an_ifc_Index_type>
an_ifc_partition_kind get_partition_kind(an_ifc_Index_type idx)
/*
Return the partition kind associated with the given index.
*/
{
  return to_partition_kind(idx.sort);
}  /* get_partition_kind */


template<>
an_ifc_partition_kind get_partition_kind(an_ifc_partition_kind_index idx)
/*
Return the partition kind associated with the given index.
*/
{
  return idx.partition_kind;
}  /* get_partition_kind */


/* FIXME: It probably shouldn't be necessary to have this specialization.  We
   probably need a role that represents an index into a particular
   partition. */
template<>
an_ifc_partition_kind get_partition_kind(an_ifc_form_spec_index idx)
/*
Return the partition kind associated with the given index.
*/
{
  return ifc_pk_form_spec;
}  /* get_partition_kind */


/* Macro used to explicitly instantiate get_partition_kind. */
#define INST_PARTITION_KIND(idx_type) \
  template \
  an_ifc_partition_kind get_partition_kind<idx_type>(idx_type idx);


/* Manually defined explicit instantiations of get_partition_kind. */
/* none */


template<typename an_ifc_Index_type>
an_ifc_partition_metadata *get_partition_metadata(an_ifc_Index_type idx)
/*
Return a pointer to the ifc partition metadata object associated with the given
index.
*/
{
  return &idx.mod->partitions[get_partition_kind(idx)];
}  /* get_partition_metadata */


/* Macro used to explicitly instantiate get_partition_metadata. */
#define INST_PARTITION_METADATA(idx_type) \
  template \
  an_ifc_partition_metadata *get_partition_metadata<idx_type>(idx_type idx);


/* Manually defined explicit instantiations of get_partition_metadata. */
INST_PARTITION_METADATA(an_ifc_partition_kind_index)


template<typename an_ifc_Index_type>
size_t get_partition_offset(an_ifc_Index_type idx)
/*
Convert a given IFC index type into a file offset into the partition.
*/
{
  an_ifc_partition_metadata *partition_metadata = get_partition_metadata(idx);
  size_t                    offset = partition_metadata->offset;
  size_t                    entry_size = partition_metadata->entry_size;

  return offset + (idx.value * entry_size);
}  /* get_partition_offset */


/* Macro used to explicitly instantiate get_partition_offset. */
#define INST_PARTITION_OFFSET(idx_type) \
  template \
  size_t get_partition_offset<idx_type>(idx_type idx);


/* Manually defined explicit instantiations of get_partition_offset. */
INST_PARTITION_OFFSET(an_ifc_partition_kind_index)


/* Macro used to explicitly instantiate get_partition_kind,
   get_partition_metdata, and get_partition_offset. */
#define INST_PARTITION_ALL(idx_type) \
  INST_PARTITION_KIND(idx_type) \
  INST_PARTITION_METADATA(idx_type) \
  INST_PARTITION_OFFSET(idx_type)


template<typename an_ifc_Index_type>
static void read_partition_element(an_ifc_Index_type idx)
/*
Given an IFC index, initialize the byte buffer to the start of the element at
the given index.
*/
{
#if EXPENSIVE_CHECKING
  /* If this fails one of two things has occurred:
     1. There's an issue with the generated code resulting in an unsafe index.
     2. This is a manually constructed index, and the caller didn't
        check its validity. */
  check_assertion(validate_element_exists(idx.mod, get_partition_kind(idx),
                                          idx.value, /*trace=*/NULL));
#endif /* EXPENSIVE_CHECKING */
  an_ifc_partition_metadata *partition_metadata = get_partition_metadata(idx);

#if DEBUG && EXPENSIVE_CHECKING
  debug_partition = partition_metadata;
#endif /* DEBUG && EXPENSIVE_CHECKING */
  init_byte_buffer(idx.mod, get_partition_offset(idx),
                   partition_metadata->size);
}  /* read_partition_element */


template<typename an_ifc_Node_type>
an_ifc_Node_type construct_node_from_module(an_ifc_module *mod)
/*
Using the previously initialized and validated module source buffer, initialize
and return a new IFC node of the given type.

Direct use of this function is discouraged, prefer one of the other construct_
functions that build upon this call.
*/
{
  using an_ifc_Storage_type = typename an_ifc_Node_type::storage_type;
  an_ifc_Node_type    result;
  an_ifc_Storage_type nts, *ntsp;

  ntsp = get<an_ifc_Storage_type>(mod, &nts, /*fill_storage=*/FALSE);
  /* When memory mapping is enabled and the endianness of the IFC and the host
     match, the front end can directly refer to portions of the IFC.
     Otherwise, the front end must fall back to copying the bytes locally with
     the correct endianness.

     If the front end is not using memory mapping, the front end must always
     create a copy of the bytes locally. */
#if USE_MMAP_FOR_MEMORY_REGIONS
  if (ntsp != &nts) {
    /* The storage wasn't used, use the pointer. */
    result = an_ifc_Node_type(mod, ntsp);
  } else {
#else /* !USE_MMAP_FOR_MEMORY_REGIONS */
  {
    check_assertion(ntsp == &nts);
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
    /* The storage was used, copy it. */
    result = an_ifc_Node_type(mod, nts);
  }
  return result;
}  /* construct_node_from_module */


/* Macro used to explicitly instantiate construct_node_from_module. */
#define INST_CONSTRUCT_NODE_FM(node_type) \
  template \
  node_type construct_node_from_module<node_type>(an_ifc_module *mod);


/* Manually defined explicit instantiations of construct_node. */
INST_CONSTRUCT_NODE_FM(an_ifc_file_header)
INST_CONSTRUCT_NODE_FM(an_ifc_partition)


template<typename an_ifc_Index_type>
static a_boolean has_been_validated(an_ifc_Index_type idx)
/*
Return TRUE if the element at the given index has already been validated.
*/
{
  /* Setup a bit mask that can be used to check the validated status of a
     position's element.  Operationally, this creates a 32 bit mask working on
     the lower 16 bits:

       0000 0000 0000 0000 - 0000 0000 0000 0001

     Shifts the bit into the correct bit positions:

       0000 0000 0000 0000 - 0000 0000 0000 0100

     This then represents the true state for this position's validated bit.

     Then a bit-and operation is used, checking if said validated bit was
     set. */
  uint32_t index = idx.value;
  size_t   block = index / 16;
  size_t   bit_index = index % 16;
  unsigned bit_mask = 0x1 << bit_index;

  return get_partition_metadata(idx)->format_validated[block] & bit_mask;
}  /* has_been_validated */


template<typename an_ifc_Index_type>
static a_boolean is_marked_invalid(an_ifc_Index_type idx)
/*
Return TRUE if the element at the given index was invalid when previously
validated.  This is only a valid operation if has_been_validated returns TRUE.
*/
{
  check_assertion(has_been_validated(idx));
  /* Setup a bit mask that can be used to check the invalid status of a
     position's element.  Operationally, this creates a 32 bit mask working
     on the higher 16 bits:

       0000 0000 0000 0001 - 0000 0000 0000 0000



       0000 0000 0000 0100 - 0000 0000 0000 0000

     This then represents the true state for this position's invalid bit.

     Then a bit-and operation is used, checking if said invalid bit was
     set. */
  uint32_t index = idx.value;
  size_t   block = index / 16;
  size_t   bit_index = index % 16;
  unsigned bit_mask = (0x1 << 16) << bit_index;

  return get_partition_metadata(idx)->format_validated[block] & bit_mask;
}  /* is_marked_invalid */


template<typename an_ifc_Index_type>
static void mark_validated(an_ifc_Index_type idx)
/*
Mark the element at the given index as having been validated.
*/
{
  /* Setup a bit mask that can be used to mark a position's element as
     validated.  Operationally, this creates a 32 bit mask working on the lower
     16 bits:

       0000 0000 0000 0000 - 0000 0000 0000 0001

     Shifts the bit into the correct bit positions:

       0000 0000 0000 0000 - 0000 0000 0000 0100

     This then represents the true state for this position's validated bit.

     Then a bit-or assignment operation is used, setting said validated bit
     while leaving the others untouched. */
  uint32_t index = idx.value;
  size_t   block = index / 16;
  size_t   bit = index % 16;
  unsigned bit_mask = 0x1 << bit;

  get_partition_metadata(idx)->format_validated[block] |= bit_mask;
}  /* mark_validated */


template<typename an_ifc_Index_type>
static void mark_invalid(an_ifc_Index_type idx)
/*
Mark the element at the given index as invalid.  This is only a valid operation
if has_been_validated returns TRUE.
*/
{
  check_assertion(has_been_validated(idx));
  /* Setup a bit mask that can be used to mark a position's element as
     invalid.  Operationally, this creates a 32 bit mask working on the
     higher 16 bits:

       0000 0000 0000 0001 - 0000 0000 0000 0000

     Shifts the bit into the correct bit positions:

       0000 0000 0000 0100 - 0000 0000 0000 0000

     This then represents the true state for this position's invalid bit.

     Then a bit-or assignment operation is used, setting said invalid bit
     while leaving the others untouched. */
  uint32_t index = idx.value;
  size_t   block = index / 16;
  size_t   bit = index % 16;
  unsigned bit_mask = (0x1 << 16) << bit;

  get_partition_metadata(idx)->format_validated[block] |= bit_mask;
}  /* mark_invalid */


template<typename an_ifc_Node_type, typename an_ifc_Index_type>
void construct_node(Opt<an_ifc_Node_type> *result,
                    an_ifc_Index_type     idx)
/*
Construct the node at the given index using the byte buffer pointed to by
result.  The given index is assumed to point to a valid partition element of
some kind, however, it will be checked for a mismatch between the associated
partition kind and the node type.  Additionally, the constructed node will be
checked for validity, and if invalid result will not be updated.  Any validity
issues with the node will be diagnosed (unless previously diagnosed by a prior
call).
*/
{
  an_ifc_partition_kind node_part_kind =
                                    get_ifc_partition_kind<an_ifc_Node_type>();
  an_ifc_partition_kind idx_part_kind = get_partition_kind(idx);

  if (node_part_kind == idx_part_kind) {
    an_ifc_Node_type read_value;

    read_partition_element(idx);
    read_value = construct_node_from_module<an_ifc_Node_type>(idx.mod);
    /* First, check to see if this node has already been validated.  If the
       node hasn't been validated, validate it, and cache the result
       appropriately; otherwise, skip re-validation and use the cached result.
       */
    if (!has_been_validated(idx)) {
      an_ifc_validation_trace trace{idx.mod, idx_part_kind, idx.value, NULL};
      a_boolean               is_valid = validate(read_value, &trace);

      mark_validated(idx);
      if (!is_valid) {
        mark_invalid(idx);
      } /* if */
    }  /* if */
    /* Then, checking the result, return the read value. */
    if (!is_marked_invalid(idx)) {
      *result = read_value;
    }  /* if */
  } else {
    a_const_char *idx_part_name = get_partition_name_from_kind(idx_part_kind);
    a_const_char *node_part_name =
                                  get_partition_name_from_kind(node_part_kind);

    /* FIXME: Use a better source position. */
    pos_st2_error(ec_ifc_partition_mismatch, &null_source_position,
                  node_part_name, idx_part_name);
  }  /* if */
}  /* construct_node */


/* Macro used to explicitly instantiate construct_node. */
#define INST_CONSTRUCT_NODE(node_type, idx_type) \
  template \
  void construct_node<node_type, idx_type>(Opt<node_type> *result, \
                                           idx_type       idx);


/* Manually defined explicit instantiations of construct_node. */
INST_CONSTRUCT_NODE(an_ifc_const_f64, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_const_i64, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_const_str, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_decl_parameter, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_decl_template, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_form_spec, an_ifc_form_spec_index)
INST_CONSTRUCT_NODE(an_ifc_heap_attr, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_heap_decl, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_heap_expr, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_heap_pp_form, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_heap_stmt, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_heap_syntax, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_heap_type, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_module_export_reference,
                    an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_scope_descriptor, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_scope_member, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_source_line, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_source_sentence, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_source_word, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_trait_function_definition,
                    an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_trait_deduction_guide, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_trait_msvc_decl_attrs, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_trait_msvc_func_params, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_trait_msvc_vendor_trait,
                    an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_trait_friend, an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE(an_ifc_trait_specialization, an_ifc_partition_kind_index)


template<typename an_ifc_Node_type, typename an_ifc_Index_type>
void construct_node_prechecked(an_ifc_Node_type  *result,
                               an_ifc_Index_type idx)
/*
Construct the node at the given index using the byte buffer pointed to by
result.  The given index is assumed to point to a valid partition element,
without any mismatches between the index's sort and the node type.  The
constructed node must have previously been checked for validity.
*/
{
  check_assertion(has_been_validated(idx) && !is_marked_invalid(idx));
  check_assertion(get_ifc_partition_kind<an_ifc_Node_type>() ==
                                                      get_partition_kind(idx));
  read_partition_element(idx);
  *result = construct_node_from_module<an_ifc_Node_type>(idx.mod);
}  /* construct_node_prechecked */


/* Macro used to explicitly instantiate construct_node_prechecked. */
#define INST_CONSTRUCT_NODE_PRE(node_type, idx_type) \
  template \
  void construct_node_prechecked<node_type, idx_type>(node_type *result, \
                                                      idx_type  idx);


/* Manually defined explicit instantiations of construct_node_prechecked. */
/* none */


template<typename an_ifc_Node_type, typename an_ifc_Index_type>
void construct_node_unchecked(an_ifc_Node_type  *result,
                              an_ifc_Index_type idx)
/*
Construct the node at the given index using the byte buffer pointed to by
result.  The given index is assumed to point to a valid partition element,
without any mismatches between the index's sort and the node type.  The
constructed node will not be checked for validity.
*/
{
  check_assertion(get_ifc_partition_kind<an_ifc_Node_type>() ==
                                                      get_partition_kind(idx));
  read_partition_element(idx);
  *result = construct_node_from_module<an_ifc_Node_type>(idx.mod);
}  /* construct_node_unchecked */


/* Macro used to explicitly instantiate construct_node_unchecked. */
#define INST_CONSTRUCT_NODE_UN(node_type, idx_type) \
  template \
  void construct_node_unchecked<node_type, idx_type>(node_type *result, \
                                                     idx_type  idx);

/* Manually defined explicit instantiations of construct_node_unchecked. */
/* FIXME: This should be automatically handled by the codegen script,
   but it isn't. */
INST_CONSTRUCT_NODE_UN(an_ifc_form_spec, an_ifc_form_spec_index)
INST_CONSTRUCT_NODE_UN(an_ifc_trait_function_definition,
                       an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE_UN(an_ifc_trait_deduction_guide,
                       an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE_UN(an_ifc_trait_msvc_decl_attrs,
                       an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE_UN(an_ifc_trait_msvc_func_params,
                       an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE_UN(an_ifc_trait_msvc_vendor_trait,
                       an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE_UN(an_ifc_trait_friend,
                       an_ifc_partition_kind_index)
INST_CONSTRUCT_NODE_UN(an_ifc_trait_specialization,
                       an_ifc_partition_kind_index)


/* Macro used to explicitly instantiate all versions of construct_node. */
#define INST_CONSTRUCT_NODE_ALL(node_type, idx_type) \
  INST_CONSTRUCT_NODE(node_type, idx_type) \
  INST_CONSTRUCT_NODE_PRE(node_type, idx_type) \
  INST_CONSTRUCT_NODE_UN(node_type, idx_type) \


/* Manually defined explicit instantiations of construct_node,
   construct_node_prechecked, and construct_node_unchecked, all in one. */
/* none */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#include "ifc_modules_inst.h"

#endif /* MICROSOFT_EXTENSIONS_ALLOWED && !STANDALONE_UTILITY_PROGRAM */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2022 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
