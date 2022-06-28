/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2022 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

ifc_map.h -- IFC types for Microsoft modules.

** NOTICE: This file is produced by an external script. **

While EDG staff should update the generation script rather than manually
editing this file, customers are welcome to modify this file and create patches
as they see fit.

Please contact EDG Support if you would be interested in using, or learning
more about, the tool that generated this file.
*/

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

struct an_ifc_module;

/*
An encapsulated representation of an IFC byte buffer.  This encapsulation
abstracts away the exact location of the underlying byte buffer storage.
*/
template<typename an_ifc_Storage_type>
struct an_ifc_Byte_buffer {
  an_ifc_Byte_buffer()
    : storing_value(FALSE), mod(NULL), storage_ptr(NULL)
    {}

  an_ifc_Byte_buffer(an_ifc_module             *mod_val,
                     const an_ifc_Storage_type &storage_ref)
    : storing_value(TRUE), mod(mod_val), storage{}
    { memcpy(&storage, &storage_ref, sizeof(an_ifc_Storage_type)); }

  an_ifc_Byte_buffer(an_ifc_module             *mod_val,
                     const an_ifc_Storage_type *storage_ptr_val)
    : storing_value(FALSE), mod(mod_val), storage_ptr(storage_ptr_val)
    {}

#if CHECKING
  /* A default state is provided for forward declaration.  This struct
     shouldn't remain "uninitialized." */
  ~an_ifc_Byte_buffer()
    { check_assertion(storing_value || storage_ptr != NULL); }
#endif /* CHECKING */

  inline an_ifc_module *get_module() const
    { return mod; }
  inline const an_ifc_Storage_type *get_storage() const;
private:
  a_boolean storing_value;
                        /* TRUE when the underlying IFC byte buffer is stored
                           as a data member and should be obtained from the
                           "storage" data member.  FALSE when the underlying
                           IFC byte buffer is stored at a different address
                           and should be obtained from the "storage_ptr"
                           data member. */
  an_ifc_module *mod;
                        /* The module associated with the underlying IFC byte
                           buffer. */
  union {
    an_ifc_Storage_type
                 storage;
                        /* When "storing_value" is TRUE this data member holds
                           the underlying IFC byte buffer. */
    const an_ifc_Storage_type
                 *storage_ptr;
                        /* When "storing_value" is FALSE this is the pointer
                           used to obtain the underlying IFC byte buffer. */
  };
};  /* an_ifc_Byte_buffer */


template<typename an_ifc_Storage_type>
inline const an_ifc_Storage_type*
an_ifc_Byte_buffer<an_ifc_Storage_type>::get_storage() const
/*
This function provides access to the underlying IFC node storage by returning
a pointer to the associated IFC byte buffer (regardless of whether the byte
buffer is stored as part of this object or a pointer to a memory mapping).
*/
{
  const an_ifc_Storage_type *result;

  if (storing_value) {
    /* The value is stored as part of this object, "storage" is the correct
       resolution. */
    result = &storage;
  } else {
    /* Check that this isn't an "uninitialized" forward declaration that's
       being accessed. */
    check_assertion(storage_ptr != NULL);
    /* The value is not stored as part of this object, "storage_ptr" is the
       correct resolution. */
    result = storage_ptr;
  }  /* if */
  return result;
}  /* get_storage */


/*
  |----------------|
  | Version | Size |
  |---------|------|
  | 0.33    | 8    |
  |---------|------|
*/
enum an_ifc_ieeele_float_part : uint8_t {};
using an_ifc_ieeele_float_storage = an_ifc_ieeele_float_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_ieeele_float_bytes = const an_ifc_ieeele_float_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_ieeele_float_bytes = an_ifc_ieeele_float_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_ieeele_float = an_ifc_Byte_buffer<an_ifc_ieeele_float_storage>;


/*
  |----------------|
  | Version | Size |
  |---------|------|
  | 0.33    | 32   |
  |---------|------|
*/
enum an_ifc_sha256_part : uint8_t {};
using an_ifc_sha256_storage = an_ifc_sha256_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_sha256_bytes = const an_ifc_sha256_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_sha256_bytes = an_ifc_sha256_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_sha256 = an_ifc_Byte_buffer<an_ifc_sha256_storage>;


/*
  |----------------|
  | Version | Size |
  |---------|------|
  | 0.33    | 4    |
  |---------|------|
*/
enum an_ifc_storage_class_part : uint8_t {};
using an_ifc_storage_class_storage = an_ifc_storage_class_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_storage_class_bytes = const an_ifc_storage_class_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_storage_class_bytes = an_ifc_storage_class_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_storage_class = an_ifc_Byte_buffer<an_ifc_storage_class_storage>;


/*
  |----------------|
  | Version | Size |
  |---------|------|
  | 0.33    | 2    |
  |---------|------|
*/
enum an_ifc_uuid_part : uint8_t {};
using an_ifc_uuid_storage = an_ifc_uuid_part[2];
#if USE_MMAP_FOR_MODULES
using an_ifc_uuid_bytes = const an_ifc_uuid_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_uuid_bytes = an_ifc_uuid_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_uuid = an_ifc_Byte_buffer<an_ifc_uuid_storage>;


/*
  |----------------|
  | Version | Size |
  |---------|------|
  | 0.33    | 4    |
  |---------|------|
*/
enum an_ifc_variadic_arity_part : uint8_t {};
using an_ifc_variadic_arity_storage = an_ifc_variadic_arity_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_variadic_arity_bytes = const an_ifc_variadic_arity_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_variadic_arity_bytes = an_ifc_variadic_arity_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_variadic_arity =
                             an_ifc_Byte_buffer<an_ifc_variadic_arity_storage>;


enum an_ifc_abi_0_33 : uint8_t;
using an_ifc_abi_storage = uint8_t;


/*
The universal representation for an IFC Abi.
*/
struct an_ifc_abi {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_abi_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_abi_storage() const
    { return this->value; }
};  /* an_ifc_abi */


enum an_ifc_active_member_0_33 : uint32_t;
using an_ifc_active_member_storage = uint32_t;


/*
The universal representation for an IFC ActiveMember.
*/
struct an_ifc_active_member {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_active_member_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_active_member_storage() const
    { return this->value; }
};  /* an_ifc_active_member */


enum an_ifc_associativity_0_33 : uint8_t;
using an_ifc_associativity_storage = uint8_t;


/*
The universal representation for an IFC Associativity.
*/
struct an_ifc_associativity {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_associativity_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_associativity_storage() const
    { return this->value; }
};  /* an_ifc_associativity */


enum an_ifc_byte_offset_0_33 : uint32_t;
using an_ifc_byte_offset_storage = uint32_t;


/*
The universal representation for an IFC ByteOffset.
*/
struct an_ifc_byte_offset {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_byte_offset_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_byte_offset_storage() const
    { return this->value; }
};  /* an_ifc_byte_offset */


enum an_ifc_cardinality_0_33 : uint32_t;
using an_ifc_cardinality_storage = uint32_t;


/*
The universal representation for an IFC Cardinality.
*/
struct an_ifc_cardinality {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_cardinality_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_cardinality_storage() const
    { return this->value; }
};  /* an_ifc_cardinality */


enum an_ifc_column_0_33 : uint32_t;
using an_ifc_column_storage = uint32_t;


/*
The universal representation for an IFC Column.
*/
struct an_ifc_column {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_column_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_column_storage() const
    { return this->value; }
};  /* an_ifc_column */


enum an_ifc_destructor_sort_0_33 : uint8_t;
using an_ifc_destructor_sort_storage = uint8_t;


/*
The universal representation for an IFC DestructorSort.
*/
struct an_ifc_destructor_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_destructor_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_destructor_sort_storage() const
    { return this->value; }
};  /* an_ifc_destructor_sort */


enum an_ifc_eh_flags_0_33 : uint16_t;
using an_ifc_eh_flags_storage = uint16_t;


/*
The universal representation for an IFC EHFlags.
*/
struct an_ifc_eh_flags {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_eh_flags_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_eh_flags_storage() const
    { return this->value; }
};  /* an_ifc_eh_flags */


enum an_ifc_encoded_access_sort_0_33 : uint8_t;
using an_ifc_encoded_access_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedAccessSort.
*/
struct an_ifc_encoded_access_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_access_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_access_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_access_sort */


enum an_ifc_encoded_architecture_sort_0_33 : uint8_t;
using an_ifc_encoded_architecture_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedArchitectureSort.
*/
struct an_ifc_encoded_architecture_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_architecture_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_architecture_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_architecture_sort */


enum an_ifc_encoded_attr_index_0_33 : uint32_t;
using an_ifc_encoded_attr_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedAttrIndex.
*/
struct an_ifc_encoded_attr_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_attr_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_attr_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_attr_index */


enum an_ifc_encoded_attr_sort_0_33 : uint32_t;
using an_ifc_encoded_attr_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedAttrSort.
*/
struct an_ifc_encoded_attr_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_attr_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_attr_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_attr_sort */


enum an_ifc_encoded_calling_convention_sort_0_33 : uint8_t;
using an_ifc_encoded_calling_convention_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedCallingConventionSort.
*/
struct an_ifc_encoded_calling_convention_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_calling_convention_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_calling_convention_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_calling_convention_sort */


enum an_ifc_encoded_chart_index_0_33 : uint32_t;
using an_ifc_encoded_chart_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedChartIndex.
*/
struct an_ifc_encoded_chart_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_chart_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_chart_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_chart_index */


enum an_ifc_encoded_chart_sort_0_33 : uint32_t;
using an_ifc_encoded_chart_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedChartSort.
*/
struct an_ifc_encoded_chart_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_chart_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_chart_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_chart_sort */


enum an_ifc_encoded_decl_index_0_33 : uint32_t;
using an_ifc_encoded_decl_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedDeclIndex.
*/
struct an_ifc_encoded_decl_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_decl_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_decl_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_decl_index */


enum an_ifc_encoded_decl_sort_0_33 : uint32_t;
using an_ifc_encoded_decl_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedDeclSort.
*/
struct an_ifc_encoded_decl_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_decl_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_decl_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_decl_sort */


enum an_ifc_encoded_delimiter_sort_0_33 : uint8_t;
using an_ifc_encoded_delimiter_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedDelimiterSort.
*/
struct an_ifc_encoded_delimiter_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_delimiter_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_delimiter_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_delimiter_sort */


enum an_ifc_encoded_dyadic_operator_sort_0_33 : uint16_t;
using an_ifc_encoded_dyadic_operator_sort_storage = uint16_t;


/*
The universal representation for an IFC EncodedDyadicOperatorSort.
*/
struct an_ifc_encoded_dyadic_operator_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_dyadic_operator_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_dyadic_operator_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_dyadic_operator_sort */


enum an_ifc_encoded_expansion_mode_sort_0_33 : uint8_t;
using an_ifc_encoded_expansion_mode_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedExpansionModeSort.
*/
struct an_ifc_encoded_expansion_mode_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_expansion_mode_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_expansion_mode_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_expansion_mode_sort */


enum an_ifc_encoded_expr_index_0_33 : uint32_t;
using an_ifc_encoded_expr_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedExprIndex.
*/
struct an_ifc_encoded_expr_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_expr_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_expr_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_expr_index */


enum an_ifc_encoded_expr_sort_0_33 : uint32_t;
using an_ifc_encoded_expr_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedExprSort.
*/
struct an_ifc_encoded_expr_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_expr_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_expr_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_expr_sort */


enum an_ifc_encoded_fold_direction_sort_0_33 : uint32_t;
using an_ifc_encoded_fold_direction_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedFoldDirectionSort.
*/
struct an_ifc_encoded_fold_direction_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_fold_direction_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_fold_direction_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_fold_direction_sort */


enum an_ifc_encoded_form_index_0_33 : uint32_t;
using an_ifc_encoded_form_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedFormIndex.
*/
struct an_ifc_encoded_form_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_form_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_form_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_form_index */


enum an_ifc_encoded_form_sort_0_33 : uint32_t;
using an_ifc_encoded_form_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedFormSort.
*/
struct an_ifc_encoded_form_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_form_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_form_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_form_sort */


enum an_ifc_encoded_initializer_sort_0_33 : uint8_t;
using an_ifc_encoded_initializer_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedInitializerSort.
*/
struct an_ifc_encoded_initializer_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_initializer_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_initializer_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_initializer_sort */


enum an_ifc_encoded_keyword_sort_0_33 : uint32_t;
using an_ifc_encoded_keyword_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedKeywordSort.
*/
struct an_ifc_encoded_keyword_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_keyword_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_keyword_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_keyword_sort */


enum an_ifc_encoded_label_sort_0_33 : uint32_t;
using an_ifc_encoded_label_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedLabelSort.
*/
struct an_ifc_encoded_label_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_label_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_label_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_label_sort */


enum an_ifc_encoded_lit_index_0_33 : uint32_t;
using an_ifc_encoded_lit_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedLitIndex.
*/
struct an_ifc_encoded_lit_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_lit_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_lit_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_lit_index */


enum an_ifc_encoded_lit_sort_0_33 : uint32_t;
using an_ifc_encoded_lit_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedLitSort.
*/
struct an_ifc_encoded_lit_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_lit_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_lit_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_lit_sort */


enum an_ifc_encoded_macro_index_0_33 : uint32_t;
using an_ifc_encoded_macro_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedMacroIndex.
*/
struct an_ifc_encoded_macro_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_macro_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_macro_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_macro_index */


enum an_ifc_encoded_macro_sort_0_33 : uint32_t;
using an_ifc_encoded_macro_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedMacroSort.
*/
struct an_ifc_encoded_macro_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_macro_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_macro_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_macro_sort */


enum an_ifc_encoded_monadic_operator_sort_0_33 : uint16_t;
using an_ifc_encoded_monadic_operator_sort_storage = uint16_t;


/*
The universal representation for an IFC EncodedMonadicOperatorSort.
*/
struct an_ifc_encoded_monadic_operator_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_monadic_operator_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_monadic_operator_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_monadic_operator_sort */


enum an_ifc_encoded_name_index_0_33 : uint32_t;
using an_ifc_encoded_name_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedNameIndex.
*/
struct an_ifc_encoded_name_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_name_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_name_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_name_index */


enum an_ifc_encoded_name_sort_0_33 : uint32_t;
using an_ifc_encoded_name_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedNameSort.
*/
struct an_ifc_encoded_name_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_name_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_name_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_name_sort */


enum an_ifc_encoded_niladic_operator_sort_0_33 : uint16_t;
using an_ifc_encoded_niladic_operator_sort_storage = uint16_t;


/*
The universal representation for an IFC EncodedNiladicOperatorSort.
*/
struct an_ifc_encoded_niladic_operator_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_niladic_operator_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_niladic_operator_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_niladic_operator_sort */


enum an_ifc_encoded_noexcept_sort_0_33 : uint8_t;
using an_ifc_encoded_noexcept_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedNoexceptSort.
*/
struct an_ifc_encoded_noexcept_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_noexcept_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_noexcept_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_noexcept_sort */


enum an_ifc_encoded_operator_sort_0_33 : uint16_t;
using an_ifc_encoded_operator_sort_storage = uint16_t;


/*
The universal representation for an IFC EncodedOperatorSort.
*/
struct an_ifc_encoded_operator_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_operator_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_operator_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_operator_sort */


enum an_ifc_encoded_parameter_sort_0_33 : uint8_t;
using an_ifc_encoded_parameter_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedParameterSort.
*/
struct an_ifc_encoded_parameter_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_parameter_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_parameter_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_parameter_sort */


enum an_ifc_encoded_pointer_declarator_sort_0_33 : uint8_t;
using an_ifc_encoded_pointer_declarator_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedPointerDeclaratorSort.
*/
struct an_ifc_encoded_pointer_declarator_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_pointer_declarator_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_pointer_declarator_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_pointer_declarator_sort */


enum an_ifc_encoded_pragma_index_0_33 : uint32_t;
using an_ifc_encoded_pragma_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedPragmaIndex.
*/
struct an_ifc_encoded_pragma_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_pragma_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_pragma_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_pragma_index */


enum an_ifc_encoded_pragma_sort_0_33 : uint32_t;
using an_ifc_encoded_pragma_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedPragmaSort.
*/
struct an_ifc_encoded_pragma_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_pragma_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_pragma_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_pragma_sort */


enum an_ifc_encoded_read_conversion_sort_0_33 : uint8_t;
using an_ifc_encoded_read_conversion_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedReadConversionSort.
*/
struct an_ifc_encoded_read_conversion_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_read_conversion_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_read_conversion_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_read_conversion_sort */


enum an_ifc_encoded_return_sort_0_33 : uint8_t;
using an_ifc_encoded_return_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedReturnSort.
*/
struct an_ifc_encoded_return_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_return_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_return_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_return_sort */


enum an_ifc_encoded_source_directive_sort_0_33 : uint16_t;
using an_ifc_encoded_source_directive_sort_storage = uint16_t;


/*
The universal representation for an IFC EncodedSourceDirectiveSort.
*/
struct an_ifc_encoded_source_directive_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_source_directive_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_source_directive_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_source_directive_sort */


enum an_ifc_encoded_source_identifier_sort_0_33 : uint16_t;
using an_ifc_encoded_source_identifier_sort_storage = uint16_t;


/*
The universal representation for an IFC EncodedSourceIdentifierSort.
*/
struct an_ifc_encoded_source_identifier_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_source_identifier_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_source_identifier_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_source_identifier_sort */


enum an_ifc_encoded_source_keyword_sort_0_33 : uint16_t;
using an_ifc_encoded_source_keyword_sort_storage = uint16_t;


/*
The universal representation for an IFC EncodedSourceKeywordSort.
*/
struct an_ifc_encoded_source_keyword_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_source_keyword_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_source_keyword_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_source_keyword_sort */


enum an_ifc_encoded_source_literal_sort_0_33 : uint16_t;
using an_ifc_encoded_source_literal_sort_storage = uint16_t;


/*
The universal representation for an IFC EncodedSourceLiteralSort.
*/
struct an_ifc_encoded_source_literal_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_source_literal_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_source_literal_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_source_literal_sort */


enum an_ifc_encoded_source_operator_sort_0_33 : uint16_t;
using an_ifc_encoded_source_operator_sort_storage = uint16_t;


/*
The universal representation for an IFC EncodedSourceOperatorSort.
*/
struct an_ifc_encoded_source_operator_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_source_operator_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_source_operator_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_source_operator_sort */


enum an_ifc_encoded_source_punctuator_sort_0_33 : uint16_t;
using an_ifc_encoded_source_punctuator_sort_storage = uint16_t;


/*
The universal representation for an IFC EncodedSourcePunctuatorSort.
*/
struct an_ifc_encoded_source_punctuator_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_source_punctuator_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_source_punctuator_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_source_punctuator_sort */


enum an_ifc_encoded_specialization_sort_0_33 : uint8_t;
using an_ifc_encoded_specialization_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedSpecializationSort.
*/
struct an_ifc_encoded_specialization_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_specialization_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_specialization_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_specialization_sort */


enum an_ifc_encoded_stmt_index_0_33 : uint32_t;
using an_ifc_encoded_stmt_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedStmtIndex.
*/
struct an_ifc_encoded_stmt_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_stmt_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_stmt_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_stmt_index */


enum an_ifc_encoded_stmt_sort_0_33 : uint32_t;
using an_ifc_encoded_stmt_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedStmtSort.
*/
struct an_ifc_encoded_stmt_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_stmt_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_stmt_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_stmt_sort */


enum an_ifc_encoded_storage_instruction_operator_sort_0_33 : uint16_t;
using an_ifc_encoded_storage_instruction_operator_sort_storage = uint16_t;


/*
The universal representation for an IFC EncodedStorageInstructionOperatorSort.
*/
struct an_ifc_encoded_storage_instruction_operator_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_storage_instruction_operator_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator
  an_ifc_encoded_storage_instruction_operator_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_storage_instruction_operator_sort */


enum an_ifc_encoded_string_index_0_33 : uint32_t;
using an_ifc_encoded_string_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedStringIndex.
*/
struct an_ifc_encoded_string_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_string_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_string_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_string_index */


enum an_ifc_encoded_string_sort_0_33 : uint32_t;
using an_ifc_encoded_string_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedStringSort.
*/
struct an_ifc_encoded_string_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_string_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_string_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_string_sort */


enum an_ifc_encoded_syntax_index_0_33 : uint32_t;
using an_ifc_encoded_syntax_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedSyntaxIndex.
*/
struct an_ifc_encoded_syntax_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_syntax_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_syntax_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_syntax_index */


enum an_ifc_encoded_syntax_sort_0_33 : uint32_t;
using an_ifc_encoded_syntax_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedSyntaxSort.
*/
struct an_ifc_encoded_syntax_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_syntax_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_syntax_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_syntax_sort */


enum an_ifc_encoded_triadic_operator_sort_0_33 : uint16_t;
using an_ifc_encoded_triadic_operator_sort_storage = uint16_t;


/*
The universal representation for an IFC EncodedTriadicOperatorSort.
*/
struct an_ifc_encoded_triadic_operator_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_triadic_operator_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_triadic_operator_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_triadic_operator_sort */


enum an_ifc_encoded_type_basis_sort_0_33 : uint8_t;
using an_ifc_encoded_type_basis_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedTypeBasisSort.
*/
struct an_ifc_encoded_type_basis_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_type_basis_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_type_basis_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_type_basis_sort */


enum an_ifc_encoded_type_index_0_33 : uint32_t;
using an_ifc_encoded_type_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedTypeIndex.
*/
struct an_ifc_encoded_type_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_type_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_type_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_type_index */


enum an_ifc_encoded_type_precision_sort_0_33 : uint8_t;
using an_ifc_encoded_type_precision_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedTypePrecisionSort.
*/
struct an_ifc_encoded_type_precision_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_type_precision_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_type_precision_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_type_precision_sort */


enum an_ifc_encoded_type_sign_sort_0_33 : uint8_t;
using an_ifc_encoded_type_sign_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedTypeSignSort.
*/
struct an_ifc_encoded_type_sign_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_type_sign_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_type_sign_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_type_sign_sort */


enum an_ifc_encoded_type_sort_0_33 : uint32_t;
using an_ifc_encoded_type_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedTypeSort.
*/
struct an_ifc_encoded_type_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_type_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_type_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_type_sort */


enum an_ifc_encoded_unit_index_0_33 : uint32_t;
using an_ifc_encoded_unit_index_storage = uint32_t;


/*
The universal representation for an IFC EncodedUnitIndex.
*/
struct an_ifc_encoded_unit_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_unit_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_unit_index_storage() const
    { return this->value; }
};  /* an_ifc_encoded_unit_index */


enum an_ifc_encoded_unit_sort_0_33 : uint32_t;
using an_ifc_encoded_unit_sort_storage = uint32_t;


/*
The universal representation for an IFC EncodedUnitSort.
*/
struct an_ifc_encoded_unit_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_unit_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_unit_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_unit_sort */


enum an_ifc_encoded_variadic_operator_sort_0_33 : uint16_t;
using an_ifc_encoded_variadic_operator_sort_storage = uint16_t;


/*
The universal representation for an IFC EncodedVariadicOperatorSort.
*/
struct an_ifc_encoded_variadic_operator_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_variadic_operator_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_variadic_operator_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_variadic_operator_sort */


enum an_ifc_encoded_word_sort_0_33 : uint8_t;
using an_ifc_encoded_word_sort_storage = uint8_t;


/*
The universal representation for an IFC EncodedWordSort.
*/
struct an_ifc_encoded_word_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_encoded_word_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_encoded_word_sort_storage() const
    { return this->value; }
};  /* an_ifc_encoded_word_sort */


enum an_ifc_entity_size_0_33 : uint32_t;
using an_ifc_entity_size_storage = uint32_t;


/*
The universal representation for an IFC EntitySize.
*/
struct an_ifc_entity_size {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_entity_size_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_entity_size_storage() const
    { return this->value; }
};  /* an_ifc_entity_size */


enum an_ifc_form_operator_sort_0_33 : uint16_t;
using an_ifc_form_operator_sort_storage = uint16_t;


/*
The universal representation for an IFC FormOperatorSort.
*/
struct an_ifc_form_operator_sort {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_form_operator_sort_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_form_operator_sort_storage() const
    { return this->value; }
};  /* an_ifc_form_operator_sort */


enum an_ifc_form_spec_index_0_33 : uint32_t;
using an_ifc_form_spec_index_storage = uint32_t;


/*
The universal representation for an IFC FormSpecIndex.
*/
struct an_ifc_form_spec_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_form_spec_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_form_spec_index_storage() const
    { return this->value; }
};  /* an_ifc_form_spec_index */


enum an_ifc_guide_traits_bitfield_0_33 : uint8_t;
using an_ifc_guide_traits_bitfield_storage = uint8_t;


/*
The universal representation for an IFC GuideTraitsBitfield.
*/
struct an_ifc_guide_traits_bitfield {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_guide_traits_bitfield_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_guide_traits_bitfield_storage() const
    { return this->value; }
};  /* an_ifc_guide_traits_bitfield */


enum an_ifc_index_0_33 : uint32_t;
using an_ifc_index_storage = uint32_t;


/*
The universal representation for an IFC Index.
*/
struct an_ifc_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_index_storage() const
    { return this->value; }
};  /* an_ifc_index */


enum an_ifc_language_version_0_33 : uint32_t;
using an_ifc_language_version_storage = uint32_t;


/*
The universal representation for an IFC LanguageVersion.
*/
struct an_ifc_language_version {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_language_version_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_language_version_storage() const
    { return this->value; }
};  /* an_ifc_language_version */


enum an_ifc_line_index_0_33 : uint32_t;
using an_ifc_line_index_storage = uint32_t;


/*
The universal representation for an IFC LineIndex.
*/
struct an_ifc_line_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_line_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_line_index_storage() const
    { return this->value; }
};  /* an_ifc_line_index */


enum an_ifc_line_number_0_33 : uint32_t;
using an_ifc_line_number_storage = uint32_t;


/*
The universal representation for an IFC LineNumber.
*/
struct an_ifc_line_number {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_line_number_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_line_number_storage() const
    { return this->value; }
};  /* an_ifc_line_number */


enum an_ifc_pack_size_0_33 : uint16_t;
using an_ifc_pack_size_storage = uint16_t;


/*
The universal representation for an IFC PackSize.
*/
struct an_ifc_pack_size {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_pack_size_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_pack_size_storage() const
    { return this->value; }
};  /* an_ifc_pack_size */


enum an_ifc_parameter_level_0_33 : uint32_t;
using an_ifc_parameter_level_storage = uint32_t;


/*
The universal representation for an IFC ParameterLevel.
*/
struct an_ifc_parameter_level {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_parameter_level_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_parameter_level_storage() const
    { return this->value; }
};  /* an_ifc_parameter_level */


enum an_ifc_parameter_position_0_33 : uint32_t;
using an_ifc_parameter_position_storage = uint32_t;


/*
The universal representation for an IFC ParameterPosition.
*/
struct an_ifc_parameter_position {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_parameter_position_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_parameter_position_storage() const
    { return this->value; }
};  /* an_ifc_parameter_position */


enum an_ifc_scope_index_0_33 : uint32_t;
using an_ifc_scope_index_storage = uint32_t;


/*
The universal representation for an IFC ScopeIndex.
*/
struct an_ifc_scope_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_scope_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_scope_index_storage() const
    { return this->value; }
};  /* an_ifc_scope_index */


enum an_ifc_segment_traits_0_33 : uint32_t;
using an_ifc_segment_traits_storage = uint32_t;


/*
The universal representation for an IFC SegmentTraits.
*/
struct an_ifc_segment_traits {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_segment_traits_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_segment_traits_storage() const
    { return this->value; }
};  /* an_ifc_segment_traits */


enum an_ifc_segment_type_0_33 : uint32_t;
using an_ifc_segment_type_storage = uint32_t;


/*
The universal representation for an IFC SegmentType.
*/
struct an_ifc_segment_type {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_segment_type_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_segment_type_storage() const
    { return this->value; }
};  /* an_ifc_segment_type */


enum an_ifc_sentence_index_0_33 : uint32_t;
using an_ifc_sentence_index_storage = uint32_t;


/*
The universal representation for an IFC SentenceIndex.
*/
struct an_ifc_sentence_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_sentence_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_sentence_index_storage() const
    { return this->value; }
};  /* an_ifc_sentence_index */


enum an_ifc_source_unknown_identifier_0_33 : uint32_t;
using an_ifc_source_unknown_identifier_storage = uint32_t;


/*
The universal representation for an IFC SourceUnknownIdentifier.
*/
struct an_ifc_source_unknown_identifier {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_source_unknown_identifier_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_source_unknown_identifier_storage() const
    { return this->value; }
};  /* an_ifc_source_unknown_identifier */


enum an_ifc_source_unknown_literal_0_33 : uint32_t;
using an_ifc_source_unknown_literal_storage = uint32_t;


/*
The universal representation for an IFC SourceUnknownLiteral.
*/
struct an_ifc_source_unknown_literal {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_source_unknown_literal_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_source_unknown_literal_storage() const
    { return this->value; }
};  /* an_ifc_source_unknown_literal */


enum an_ifc_source_unknown_word_0_33 : uint16_t;
using an_ifc_source_unknown_word_storage = uint16_t;


/*
The universal representation for an IFC SourceUnknownWord.
*/
struct an_ifc_source_unknown_word {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_source_unknown_word_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_source_unknown_word_storage() const
    { return this->value; }
};  /* an_ifc_source_unknown_word */


enum an_ifc_text_offset_0_33 : uint32_t;
using an_ifc_text_offset_storage = uint32_t;


/*
The universal representation for an IFC TextOffset.
*/
struct an_ifc_text_offset {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_text_offset_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_text_offset_storage() const
    { return this->value; }
};  /* an_ifc_text_offset */


enum an_ifc_unique_id_0_33 : uint32_t;
using an_ifc_unique_id_storage = uint32_t;


/*
The universal representation for an IFC UniqueID.
*/
struct an_ifc_unique_id {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_unique_id_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_unique_id_storage() const
    { return this->value; }
};  /* an_ifc_unique_id */


enum an_ifc_version_0_33 : uint8_t;
using an_ifc_version_storage = uint8_t;


/*
The universal representation for an IFC Version.
*/
struct an_ifc_version {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_version_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_version_storage() const
    { return this->value; }
};  /* an_ifc_version */


enum an_ifc_bool_0_33 : uint8_t;
using an_ifc_bool_storage = uint8_t;


/*
The universal representation for an IFC bool.
*/
struct an_ifc_bool {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_bool_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_bool_storage() const
    { return this->value; }
};  /* an_ifc_bool */


enum an_ifc_u16_0_33 : uint16_t;
using an_ifc_u16_storage = uint16_t;


/*
The universal representation for an IFC u16.
*/
struct an_ifc_u16 {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_u16_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_u16_storage() const
    { return this->value; }
};  /* an_ifc_u16 */


enum an_ifc_u64_0_33 : uint64_t;
using an_ifc_u64_storage = uint64_t;


/*
The universal representation for an IFC u64.
*/
struct an_ifc_u64 {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_u64_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */

  inline operator an_ifc_u64_storage() const
    { return this->value; }
};  /* an_ifc_u64 */


enum an_ifc_access_sort_0_33 : uint8_t {
  ifc_0_33_as_none      = 0,
  ifc_0_33_as_private   = 1,
  ifc_0_33_as_protected = 2,
  ifc_0_33_as_public    = 3
};  /* an_ifc_access_sort_0_33 */


enum an_ifc_access_sort {
  ifc_as_none,
  ifc_as_private,
  ifc_as_protected,
  ifc_as_public
};  /* an_ifc_access_sort */


enum an_ifc_architecture_sort_0_33 : uint8_t {
  ifc_0_33_as_unknown          = 0,
  ifc_0_33_as_x86              = 1,
  ifc_0_33_as_x64              = 2,
  ifc_0_33_as_arm32            = 3,
  ifc_0_33_as_arm64            = 4,
  ifc_0_33_as_hybrid_x86_arm64 = 5
};  /* an_ifc_architecture_sort_0_33 */


enum an_ifc_architecture_sort {
  ifc_as_arm32,
  ifc_as_arm64,
  ifc_as_hybrid_x86_arm64,
  ifc_as_unknown,
  ifc_as_x64,
  ifc_as_x86
};  /* an_ifc_architecture_sort */


enum an_ifc_attr_sort_0_33 : uint32_t {
  ifc_0_33_as_attr_nothing    = 0,
  ifc_0_33_as_attr_basic      = 1,
  ifc_0_33_as_attr_scoped     = 2,
  ifc_0_33_as_attr_labeled    = 3,
  ifc_0_33_as_attr_called     = 4,
  ifc_0_33_as_attr_expanded   = 5,
  ifc_0_33_as_attr_factored   = 6,
  ifc_0_33_as_attr_elaborated = 7,
  ifc_0_33_as_attr_tuple      = 8
};  /* an_ifc_attr_sort_0_33 */


enum an_ifc_attr_sort {
  ifc_as_attr_basic,
  ifc_as_attr_called,
  ifc_as_attr_elaborated,
  ifc_as_attr_expanded,
  ifc_as_attr_factored,
  ifc_as_attr_labeled,
  ifc_as_attr_nothing,
  ifc_as_attr_scoped,
  ifc_as_attr_tuple
};  /* an_ifc_attr_sort */


enum an_ifc_calling_convention_sort_0_33 : uint8_t {
  ifc_0_33_ccs_cdecl  = 0,
  ifc_0_33_ccs_fast   = 1,
  ifc_0_33_ccs_std    = 2,
  ifc_0_33_ccs_this   = 3,
  ifc_0_33_ccs_clr    = 4,
  ifc_0_33_ccs_vector = 5,
  ifc_0_33_ccs_eabi   = 6
};  /* an_ifc_calling_convention_sort_0_33 */


enum an_ifc_calling_convention_sort {
  ifc_ccs_cdecl,
  ifc_ccs_clr,
  ifc_ccs_eabi,
  ifc_ccs_fast,
  ifc_ccs_std,
  ifc_ccs_this,
  ifc_ccs_vector
};  /* an_ifc_calling_convention_sort */


enum an_ifc_chart_sort_0_33 : uint32_t {
  ifc_0_33_cs_chart_none       = 0,
  ifc_0_33_cs_chart_unilevel   = 1,
  ifc_0_33_cs_chart_multilevel = 2
};  /* an_ifc_chart_sort_0_33 */


enum an_ifc_chart_sort {
  ifc_cs_chart_multilevel,
  ifc_cs_chart_none,
  ifc_cs_chart_unilevel
};  /* an_ifc_chart_sort */


enum an_ifc_decl_sort_0_33 : uint32_t {
  ifc_0_33_ds_decl_vendor_extension        = 0,
  ifc_0_33_ds_decl_enumerator              = 1,
  ifc_0_33_ds_decl_variable                = 2,
  ifc_0_33_ds_decl_parameter               = 3,
  ifc_0_33_ds_decl_field                   = 4,
  ifc_0_33_ds_decl_bitfield                = 5,
  ifc_0_33_ds_decl_scope                   = 6,
  ifc_0_33_ds_decl_enumeration             = 7,
  ifc_0_33_ds_decl_alias                   = 8,
  ifc_0_33_ds_decl_temploid                = 9,
  ifc_0_33_ds_decl_template                = 10,
  ifc_0_33_ds_decl_partial_specialization  = 11,
  ifc_0_33_ds_decl_explicit_specialization = 12,
  ifc_0_33_ds_decl_explicit_instantiation  = 13,
  ifc_0_33_ds_decl_concept                 = 14,
  ifc_0_33_ds_decl_function                = 15,
  ifc_0_33_ds_decl_method                  = 16,
  ifc_0_33_ds_decl_constructor             = 17,
  ifc_0_33_ds_decl_inherited_constructor   = 18,
  ifc_0_33_ds_decl_destructor              = 19,
  ifc_0_33_ds_decl_reference               = 20,
  ifc_0_33_ds_decl_using_declaration       = 21,
  ifc_0_33_ds_decl_using_directive         = 22,
  ifc_0_33_ds_decl_friend                  = 23,
  ifc_0_33_ds_decl_expansion               = 24,
  ifc_0_33_ds_decl_deduction_guide         = 25,
  ifc_0_33_ds_decl_barren                  = 26,
  ifc_0_33_ds_decl_tuple                   = 27,
  ifc_0_33_ds_decl_syntax_tree             = 28,
  ifc_0_33_ds_decl_intrinsic               = 29,
  ifc_0_33_ds_decl_property                = 30,
  ifc_0_33_ds_decl_output_segment          = 31
};  /* an_ifc_decl_sort_0_33 */


enum an_ifc_decl_sort_0_41 : uint32_t {
  ifc_0_41_ds_decl_vendor_extension       = 0,
  ifc_0_41_ds_decl_enumerator             = 1,
  ifc_0_41_ds_decl_variable               = 2,
  ifc_0_41_ds_decl_parameter              = 3,
  ifc_0_41_ds_decl_field                  = 4,
  ifc_0_41_ds_decl_bitfield               = 5,
  ifc_0_41_ds_decl_scope                  = 6,
  ifc_0_41_ds_decl_enumeration            = 7,
  ifc_0_41_ds_decl_alias                  = 8,
  ifc_0_41_ds_decl_temploid               = 9,
  ifc_0_41_ds_decl_template               = 10,
  ifc_0_41_ds_decl_partial_specialization = 11,
  ifc_0_41_ds_decl_specialization         = 12,
  ifc_0_41_ds_decl_concept                = 14,
  ifc_0_41_ds_decl_function               = 15,
  ifc_0_41_ds_decl_method                 = 16,
  ifc_0_41_ds_decl_constructor            = 17,
  ifc_0_41_ds_decl_inherited_constructor  = 18,
  ifc_0_41_ds_decl_destructor             = 19,
  ifc_0_41_ds_decl_reference              = 20,
  ifc_0_41_ds_decl_using_declaration      = 21,
  ifc_0_41_ds_decl_using_directive        = 22,
  ifc_0_41_ds_decl_friend                 = 23,
  ifc_0_41_ds_decl_expansion              = 24,
  ifc_0_41_ds_decl_deduction_guide        = 25,
  ifc_0_41_ds_decl_barren                 = 26,
  ifc_0_41_ds_decl_tuple                  = 27,
  ifc_0_41_ds_decl_syntax_tree            = 28,
  ifc_0_41_ds_decl_intrinsic              = 29,
  ifc_0_41_ds_decl_property               = 30,
  ifc_0_41_ds_decl_output_segment         = 31
};  /* an_ifc_decl_sort_0_41 */


enum an_ifc_decl_sort {
  ifc_ds_decl_alias,
  ifc_ds_decl_barren,
  ifc_ds_decl_bitfield,
  ifc_ds_decl_concept,
  ifc_ds_decl_constructor,
  ifc_ds_decl_deduction_guide,
  ifc_ds_decl_destructor,
  ifc_ds_decl_enumeration,
  ifc_ds_decl_enumerator,
  ifc_ds_decl_expansion,
  ifc_ds_decl_explicit_instantiation,
  ifc_ds_decl_explicit_specialization,
  ifc_ds_decl_field,
  ifc_ds_decl_friend,
  ifc_ds_decl_function,
  ifc_ds_decl_inherited_constructor,
  ifc_ds_decl_intrinsic,
  ifc_ds_decl_method,
  ifc_ds_decl_output_segment,
  ifc_ds_decl_parameter,
  ifc_ds_decl_partial_specialization,
  ifc_ds_decl_property,
  ifc_ds_decl_reference,
  ifc_ds_decl_scope,
  ifc_ds_decl_specialization,
  ifc_ds_decl_syntax_tree,
  ifc_ds_decl_template,
  ifc_ds_decl_temploid,
  ifc_ds_decl_tuple,
  ifc_ds_decl_using_declaration,
  ifc_ds_decl_using_directive,
  ifc_ds_decl_variable,
  ifc_ds_decl_vendor_extension
};  /* an_ifc_decl_sort */


enum an_ifc_delimiter_sort_0_33 : uint8_t {
  ifc_0_33_ds_unknown     = 0,
  ifc_0_33_ds_brace       = 1,
  ifc_0_33_ds_parenthesis = 2
};  /* an_ifc_delimiter_sort_0_33 */


enum an_ifc_delimiter_sort {
  ifc_ds_brace,
  ifc_ds_parenthesis,
  ifc_ds_unknown
};  /* an_ifc_delimiter_sort */


enum an_ifc_dyadic_operator_sort_0_33 : uint16_t {
  ifc_0_33_dos_unknown                                             = 0,
  ifc_0_33_dos_plus                                                = 1,
  ifc_0_33_dos_minus                                               = 2,
  ifc_0_33_dos_mult                                                = 3,
  ifc_0_33_dos_slash                                               = 4,
  ifc_0_33_dos_modulo                                              = 5,
  ifc_0_33_dos_remainder                                           = 6,
  ifc_0_33_dos_bitand                                              = 7,
  ifc_0_33_dos_bitor                                               = 8,
  ifc_0_33_dos_bitxor                                              = 9,
  ifc_0_33_dos_lshift                                              = 10,
  ifc_0_33_dos_rshift                                              = 11,
  ifc_0_33_dos_equal                                               = 12,
  ifc_0_33_dos_not_equal                                           = 13,
  ifc_0_33_dos_less                                                = 14,
  ifc_0_33_dos_less_equal                                          = 15,
  ifc_0_33_dos_greater                                             = 16,
  ifc_0_33_dos_greater_equal                                       = 17,
  ifc_0_33_dos_compare                                             = 18,
  ifc_0_33_dos_logic_and                                           = 19,
  ifc_0_33_dos_logic_or                                            = 20,
  ifc_0_33_dos_assign                                              = 21,
  ifc_0_33_dos_plus_assign                                         = 22,
  ifc_0_33_dos_minus_assign                                        = 23,
  ifc_0_33_dos_mult_assign                                         = 24,
  ifc_0_33_dos_slash_assign                                        = 25,
  ifc_0_33_dos_modulo_assign                                       = 26,
  ifc_0_33_dos_bitand_assign                                       = 27,
  ifc_0_33_dos_bitor_assign                                        = 28,
  ifc_0_33_dos_bitxor_assign                                       = 29,
  ifc_0_33_dos_lshift_assign                                       = 30,
  ifc_0_33_dos_rshift_assign                                       = 31,
  ifc_0_33_dos_comma                                               = 32,
  ifc_0_33_dos_dot                                                 = 33,
  ifc_0_33_dos_arrow                                               = 34,
  ifc_0_33_dos_dot_star                                            = 35,
  ifc_0_33_dos_arrow_star                                          = 36,
  ifc_0_33_dos_curry                                               = 37,
  ifc_0_33_dos_apply                                               = 38,
  ifc_0_33_dos_index                                               = 39,
  ifc_0_33_dos_default_at                                          = 40,
  ifc_0_33_dos_new                                                 = 41,
  ifc_0_33_dos_new_array                                           = 42,
  ifc_0_33_dos_destruct                                            = 43,
  ifc_0_33_dos_destruct_at                                         = 44,
  ifc_0_33_dos_cleanup                                             = 45,
  ifc_0_33_dos_qualification                                       = 46,
  ifc_0_33_dos_promote                                             = 47,
  ifc_0_33_dos_demote                                              = 48,
  ifc_0_33_dos_coerce                                              = 49,
  ifc_0_33_dos_rewrite                                             = 50,
  ifc_0_33_dos_bless                                               = 51,
  ifc_0_33_dos_cast                                                = 52,
  ifc_0_33_dos_explicit_conversion                                 = 53,
  ifc_0_33_dos_reinterpret_cast                                    = 54,
  ifc_0_33_dos_static_cast                                         = 55,
  ifc_0_33_dos_const_cast                                          = 56,
  ifc_0_33_dos_dynamic_cast                                        = 57,
  ifc_0_33_dos_narrow                                              = 58,
  ifc_0_33_dos_widen                                               = 59,
  ifc_0_33_dos_pretend                                             = 60,
  ifc_0_33_dos_closure                                             = 61,
  ifc_0_33_dos_zero_initialize                                     = 62,
  ifc_0_33_dos_clear_storage                                       = 63,
  ifc_0_33_dos_select                                              = 64,
  ifc_0_33_dos_msvc                                                = 1024,
  ifc_0_33_dos_msvc_try_cast                                       = 1025,
  ifc_0_33_dos_msvc_curry                                          = 1026,
  ifc_0_33_dos_msvc_virtual_curry                                  = 1027,
  ifc_0_33_dos_msvc_align                                          = 1028,
  ifc_0_33_dos_msvc_bit_span                                       = 1029,
  ifc_0_33_dos_msvc_bitfield_access                                = 1030,
  ifc_0_33_dos_msvc_obscure_bitfield_access                        = 1031,
  ifc_0_33_dos_msvc_initialize                                     = 1032,
  ifc_0_33_dos_msvc_builtin_offset_of                              = 1033,
  ifc_0_33_dos_msvc_is_base_of                                     = 1034,
  ifc_0_33_dos_msvc_is_convertible_to                              = 1035,
  ifc_0_33_dos_msvc_is_trivially_assignable                        = 1036,
  ifc_0_33_dos_msvc_is_nothrow_assignable                          = 1037,
  ifc_0_33_dos_msvc_is_assignable                                  = 1038,
  ifc_0_33_dos_msvc_is_assignable_nocheck                          = 1039,
  ifc_0_33_dos_msvc_builtin_bit_cast                               = 1040,
  ifc_0_33_dos_msvc_builtin_is_layout_compatible                   = 1041,
  ifc_0_33_dos_msvc_builtin_is_pointer_interconvertible_base_of    = 1042,
  ifc_0_33_dos_msvc_builtin_is_pointer_interconvertible_with_class = 1043,
  ifc_0_33_dos_msvc_builtin_is_corresponding_member                = 1044,
  ifc_0_33_dos_msvc_intrinsic                                      = 1045,
  ifc_0_33_dos_msvc_saturated_arithmetic                           = 1046
};  /* an_ifc_dyadic_operator_sort_0_33 */


enum an_ifc_dyadic_operator_sort {
  ifc_dos_apply,
  ifc_dos_arrow,
  ifc_dos_arrow_star,
  ifc_dos_assign,
  ifc_dos_bitand,
  ifc_dos_bitand_assign,
  ifc_dos_bitor,
  ifc_dos_bitor_assign,
  ifc_dos_bitxor,
  ifc_dos_bitxor_assign,
  ifc_dos_bless,
  ifc_dos_cast,
  ifc_dos_cleanup,
  ifc_dos_clear_storage,
  ifc_dos_closure,
  ifc_dos_coerce,
  ifc_dos_comma,
  ifc_dos_compare,
  ifc_dos_const_cast,
  ifc_dos_curry,
  ifc_dos_default_at,
  ifc_dos_demote,
  ifc_dos_destruct,
  ifc_dos_destruct_at,
  ifc_dos_dot,
  ifc_dos_dot_star,
  ifc_dos_dynamic_cast,
  ifc_dos_equal,
  ifc_dos_explicit_conversion,
  ifc_dos_greater,
  ifc_dos_greater_equal,
  ifc_dos_index,
  ifc_dos_less,
  ifc_dos_less_equal,
  ifc_dos_logic_and,
  ifc_dos_logic_or,
  ifc_dos_lshift,
  ifc_dos_lshift_assign,
  ifc_dos_minus,
  ifc_dos_minus_assign,
  ifc_dos_modulo,
  ifc_dos_modulo_assign,
  ifc_dos_msvc,
  ifc_dos_msvc_align,
  ifc_dos_msvc_bit_span,
  ifc_dos_msvc_bitfield_access,
  ifc_dos_msvc_builtin_bit_cast,
  ifc_dos_msvc_builtin_is_corresponding_member,
  ifc_dos_msvc_builtin_is_layout_compatible,
  ifc_dos_msvc_builtin_is_pointer_interconvertible_base_of,
  ifc_dos_msvc_builtin_is_pointer_interconvertible_with_class,
  ifc_dos_msvc_builtin_offset_of,
  ifc_dos_msvc_curry,
  ifc_dos_msvc_initialize,
  ifc_dos_msvc_intrinsic,
  ifc_dos_msvc_is_assignable,
  ifc_dos_msvc_is_assignable_nocheck,
  ifc_dos_msvc_is_base_of,
  ifc_dos_msvc_is_convertible_to,
  ifc_dos_msvc_is_nothrow_assignable,
  ifc_dos_msvc_is_trivially_assignable,
  ifc_dos_msvc_obscure_bitfield_access,
  ifc_dos_msvc_saturated_arithmetic,
  ifc_dos_msvc_try_cast,
  ifc_dos_msvc_virtual_curry,
  ifc_dos_mult,
  ifc_dos_mult_assign,
  ifc_dos_narrow,
  ifc_dos_new,
  ifc_dos_new_array,
  ifc_dos_not_equal,
  ifc_dos_plus,
  ifc_dos_plus_assign,
  ifc_dos_pretend,
  ifc_dos_promote,
  ifc_dos_qualification,
  ifc_dos_reinterpret_cast,
  ifc_dos_remainder,
  ifc_dos_rewrite,
  ifc_dos_rshift,
  ifc_dos_rshift_assign,
  ifc_dos_select,
  ifc_dos_slash,
  ifc_dos_slash_assign,
  ifc_dos_static_cast,
  ifc_dos_unknown,
  ifc_dos_widen,
  ifc_dos_zero_initialize
};  /* an_ifc_dyadic_operator_sort */


enum an_ifc_expansion_mode_sort_0_33 : uint8_t {
  ifc_0_33_ems_full    = 0,
  ifc_0_33_ems_partial = 1
};  /* an_ifc_expansion_mode_sort_0_33 */


enum an_ifc_expansion_mode_sort {
  ifc_ems_full,
  ifc_ems_partial
};  /* an_ifc_expansion_mode_sort */


enum an_ifc_expr_sort_0_33 : uint32_t {
  ifc_0_33_es_expr_vendor_extension            = 0,
  ifc_0_33_es_expr_empty                       = 1,
  ifc_0_33_es_expr_literal                     = 2,
  ifc_0_33_es_expr_lambda                      = 3,
  ifc_0_33_es_expr_type                        = 4,
  ifc_0_33_es_expr_named_decl                  = 5,
  ifc_0_33_es_expr_unresolved_id               = 6,
  ifc_0_33_es_expr_template_id                 = 7,
  ifc_0_33_es_expr_unqualified_id              = 8,
  ifc_0_33_es_expr_simple_identifier           = 9,
  ifc_0_33_es_expr_pointer                     = 10,
  ifc_0_33_es_expr_qualified_name              = 11,
  ifc_0_33_es_expr_path                        = 12,
  ifc_0_33_es_expr_read                        = 13,
  ifc_0_33_es_expr_monad                       = 14,
  ifc_0_33_es_expr_dyad                        = 15,
  ifc_0_33_es_expr_triad                       = 16,
  ifc_0_33_es_expr_string                      = 17,
  ifc_0_33_es_expr_temporary                   = 18,
  ifc_0_33_es_expr_call                        = 19,
  ifc_0_33_es_expr_member_initializer          = 20,
  ifc_0_33_es_expr_member_access               = 21,
  ifc_0_33_es_expr_inheritance_path            = 22,
  ifc_0_33_es_expr_initializer_list            = 23,
  ifc_0_33_es_expr_cast                        = 24,
  ifc_0_33_es_expr_condition                   = 25,
  ifc_0_33_es_expr_expression_list             = 26,
  ifc_0_33_es_expr_sizeof_type                 = 27,
  ifc_0_33_es_expr_alignof                     = 28,
  ifc_0_33_es_expr_new                         = 29,
  ifc_0_33_es_expr_delete                      = 30,
  ifc_0_33_es_expr_typeid                      = 31,
  ifc_0_33_es_expr_destructor_call             = 32,
  ifc_0_33_es_expr_syntax_tree                 = 33,
  ifc_0_33_es_expr_function_string             = 34,
  ifc_0_33_es_expr_compound_string             = 35,
  ifc_0_33_es_expr_string_sequence             = 36,
  ifc_0_33_es_expr_initializer                 = 37,
  ifc_0_33_es_expr_requires                    = 38,
  ifc_0_33_es_expr_unary_fold                  = 39,
  ifc_0_33_es_expr_binary_fold                 = 40,
  ifc_0_33_es_expr_hierarchy_conversion        = 41,
  ifc_0_33_es_expr_product_type_value          = 42,
  ifc_0_33_es_expr_sum_type_value              = 43,
  ifc_0_33_es_expr_subobject_value             = 44,
  ifc_0_33_es_expr_array_value                 = 45,
  ifc_0_33_es_expr_dynamic_dispatch            = 46,
  ifc_0_33_es_expr_virtual_function_conversion = 47,
  ifc_0_33_es_expr_placeholder                 = 48,
  ifc_0_33_es_expr_expansion                   = 49,
  ifc_0_33_es_expr_generic                     = 50,
  ifc_0_33_es_expr_tuple                       = 51,
  ifc_0_33_es_expr_nullptr                     = 52,
  ifc_0_33_es_expr_this                        = 53,
  ifc_0_33_es_expr_template_reference          = 54,
  ifc_0_33_es_expr_push_state                  = 55,
  ifc_0_33_es_expr_type_trait_intrinsic        = 56,
  ifc_0_33_es_expr_designated_initializer      = 57,
  ifc_0_33_es_expr_packed_template_arguments   = 58,
  ifc_0_33_es_expr_tokens                      = 59,
  ifc_0_33_es_expr_assign_initializer          = 60
};  /* an_ifc_expr_sort_0_33 */


enum an_ifc_expr_sort {
  ifc_es_expr_alignof,
  ifc_es_expr_array_value,
  ifc_es_expr_assign_initializer,
  ifc_es_expr_binary_fold,
  ifc_es_expr_call,
  ifc_es_expr_cast,
  ifc_es_expr_compound_string,
  ifc_es_expr_condition,
  ifc_es_expr_delete,
  ifc_es_expr_designated_initializer,
  ifc_es_expr_destructor_call,
  ifc_es_expr_dyad,
  ifc_es_expr_dynamic_dispatch,
  ifc_es_expr_empty,
  ifc_es_expr_expansion,
  ifc_es_expr_expression_list,
  ifc_es_expr_function_string,
  ifc_es_expr_generic,
  ifc_es_expr_hierarchy_conversion,
  ifc_es_expr_inheritance_path,
  ifc_es_expr_initializer,
  ifc_es_expr_initializer_list,
  ifc_es_expr_lambda,
  ifc_es_expr_literal,
  ifc_es_expr_member_access,
  ifc_es_expr_member_initializer,
  ifc_es_expr_monad,
  ifc_es_expr_named_decl,
  ifc_es_expr_new,
  ifc_es_expr_nullptr,
  ifc_es_expr_packed_template_arguments,
  ifc_es_expr_path,
  ifc_es_expr_placeholder,
  ifc_es_expr_pointer,
  ifc_es_expr_product_type_value,
  ifc_es_expr_push_state,
  ifc_es_expr_qualified_name,
  ifc_es_expr_read,
  ifc_es_expr_requires,
  ifc_es_expr_simple_identifier,
  ifc_es_expr_sizeof_type,
  ifc_es_expr_string,
  ifc_es_expr_string_sequence,
  ifc_es_expr_subobject_value,
  ifc_es_expr_sum_type_value,
  ifc_es_expr_syntax_tree,
  ifc_es_expr_template_id,
  ifc_es_expr_template_reference,
  ifc_es_expr_temporary,
  ifc_es_expr_this,
  ifc_es_expr_tokens,
  ifc_es_expr_triad,
  ifc_es_expr_tuple,
  ifc_es_expr_type,
  ifc_es_expr_type_trait_intrinsic,
  ifc_es_expr_typeid,
  ifc_es_expr_unary_fold,
  ifc_es_expr_unqualified_id,
  ifc_es_expr_unresolved_id,
  ifc_es_expr_vendor_extension,
  ifc_es_expr_virtual_function_conversion
};  /* an_ifc_expr_sort */


enum an_ifc_fold_direction_sort_0_33 : uint32_t {
  ifc_0_33_fds_unknown = 0,
  ifc_0_33_fds_left    = 1,
  ifc_0_33_fds_right   = 2
};  /* an_ifc_fold_direction_sort_0_33 */


enum an_ifc_fold_direction_sort {
  ifc_fds_left,
  ifc_fds_right,
  ifc_fds_unknown
};  /* an_ifc_fold_direction_sort */


enum an_ifc_form_sort_0_33 : uint32_t {
  ifc_0_33_fs_form_identifier    = 0,
  ifc_0_33_fs_form_number        = 1,
  ifc_0_33_fs_form_character     = 2,
  ifc_0_33_fs_form_string        = 3,
  ifc_0_33_fs_form_operator      = 4,
  ifc_0_33_fs_form_keyword       = 5,
  ifc_0_33_fs_form_whitespace    = 6,
  ifc_0_33_fs_form_parameter     = 7,
  ifc_0_33_fs_form_stringize     = 8,
  ifc_0_33_fs_form_catenate      = 9,
  ifc_0_33_fs_form_pragma        = 10,
  ifc_0_33_fs_form_header        = 11,
  ifc_0_33_fs_form_parenthesized = 12,
  ifc_0_33_fs_form_tuple         = 13,
  ifc_0_33_fs_form_junk          = 14
};  /* an_ifc_form_sort_0_33 */


enum an_ifc_form_sort {
  ifc_fs_form_catenate,
  ifc_fs_form_character,
  ifc_fs_form_header,
  ifc_fs_form_identifier,
  ifc_fs_form_junk,
  ifc_fs_form_keyword,
  ifc_fs_form_number,
  ifc_fs_form_operator,
  ifc_fs_form_parameter,
  ifc_fs_form_parenthesized,
  ifc_fs_form_pragma,
  ifc_fs_form_string,
  ifc_fs_form_stringize,
  ifc_fs_form_tuple,
  ifc_fs_form_whitespace
};  /* an_ifc_form_sort */


enum an_ifc_initializer_sort_0_33 : uint8_t {
  ifc_0_33_is_unknown = 0,
  ifc_0_33_is_direct  = 1,
  ifc_0_33_is_copy    = 2
};  /* an_ifc_initializer_sort_0_33 */


enum an_ifc_initializer_sort {
  ifc_is_copy,
  ifc_is_direct,
  ifc_is_unknown
};  /* an_ifc_initializer_sort */


enum an_ifc_keyword_sort_0_33 : uint32_t {
  ifc_0_33_ks_nothing   = 0,
  ifc_0_33_ks_class     = 1,
  ifc_0_33_ks_struct    = 2,
  ifc_0_33_ks_union     = 3,
  ifc_0_33_ks_public    = 4,
  ifc_0_33_ks_protected = 5,
  ifc_0_33_ks_private   = 6,
  ifc_0_33_ks_default   = 7,
  ifc_0_33_ks_delete    = 8,
  ifc_0_33_ks_mutable   = 9,
  ifc_0_33_ks_constexpr = 10,
  ifc_0_33_ks_consteval = 11,
  ifc_0_33_ks_typename  = 12
};  /* an_ifc_keyword_sort_0_33 */


enum an_ifc_keyword_sort {
  ifc_ks_class,
  ifc_ks_consteval,
  ifc_ks_constexpr,
  ifc_ks_default,
  ifc_ks_delete,
  ifc_ks_mutable,
  ifc_ks_nothing,
  ifc_ks_private,
  ifc_ks_protected,
  ifc_ks_public,
  ifc_ks_struct,
  ifc_ks_typename,
  ifc_ks_union
};  /* an_ifc_keyword_sort */


enum an_ifc_label_sort_0_33 : uint32_t {
  ifc_0_33_ls_unknown = 0,
  ifc_0_33_ls_case    = 1,
  ifc_0_33_ls_default = 2,
  ifc_0_33_ls_label   = 3
};  /* an_ifc_label_sort_0_33 */


enum an_ifc_label_sort {
  ifc_ls_case,
  ifc_ls_default,
  ifc_ls_label,
  ifc_ls_unknown
};  /* an_ifc_label_sort */


enum an_ifc_lit_sort_0_33 : uint32_t {
  ifc_0_33_ls_immediate      = 0,
  ifc_0_33_ls_integer        = 1,
  ifc_0_33_ls_floating_point = 2
};  /* an_ifc_lit_sort_0_33 */


enum an_ifc_lit_sort {
  ifc_ls_floating_point,
  ifc_ls_immediate,
  ifc_ls_integer
};  /* an_ifc_lit_sort */


enum an_ifc_macro_sort_0_33 : uint32_t {
  ifc_0_33_ms_macro_object_like   = 0,
  ifc_0_33_ms_macro_function_like = 1
};  /* an_ifc_macro_sort_0_33 */


enum an_ifc_macro_sort {
  ifc_ms_macro_function_like,
  ifc_ms_macro_object_like
};  /* an_ifc_macro_sort */


enum an_ifc_monadic_operator_sort_0_33 : uint16_t {
  ifc_0_33_mos_unknown                                = 0,
  ifc_0_33_mos_plus                                   = 1,
  ifc_0_33_mos_negate                                 = 2,
  ifc_0_33_mos_deref                                  = 3,
  ifc_0_33_mos_address                                = 4,
  ifc_0_33_mos_complement                             = 5,
  ifc_0_33_mos_not                                    = 6,
  ifc_0_33_mos_pre_increment                          = 7,
  ifc_0_33_mos_pre_decrement                          = 8,
  ifc_0_33_mos_post_increment                         = 9,
  ifc_0_33_mos_post_decrement                         = 10,
  ifc_0_33_mos_truncate                               = 11,
  ifc_0_33_mos_ceil                                   = 12,
  ifc_0_33_mos_floor                                  = 13,
  ifc_0_33_mos_paren                                  = 14,
  ifc_0_33_mos_brace                                  = 15,
  ifc_0_33_mos_alignas                                = 16,
  ifc_0_33_mos_alignof                                = 17,
  ifc_0_33_mos_sizeof                                 = 18,
  ifc_0_33_mos_cardinality                            = 19,
  ifc_0_33_mos_typeid                                 = 20,
  ifc_0_33_mos_noexcept                               = 21,
  ifc_0_33_mos_requires                               = 22,
  ifc_0_33_mos_co_return                              = 23,
  ifc_0_33_mos_await                                  = 24,
  ifc_0_33_mos_yield                                  = 25,
  ifc_0_33_mos_throw                                  = 26,
  ifc_0_33_mos_new                                    = 27,
  ifc_0_33_mos_delete                                 = 28,
  ifc_0_33_mos_delete_array                           = 29,
  ifc_0_33_mos_expand                                 = 30,
  ifc_0_33_mos_read                                   = 31,
  ifc_0_33_mos_materialize                            = 32,
  ifc_0_33_mos_pseudo_dtor_call                       = 33,
  ifc_0_33_mos_lookup_globally                        = 34,
  ifc_0_33_mos_msvc                                   = 1024,
  ifc_0_33_mos_msvc_assume                            = 1025,
  ifc_0_33_mos_msvc_alignof                           = 1026,
  ifc_0_33_mos_msvc_uuidof                            = 1027,
  ifc_0_33_mos_msvc_is_class                          = 1028,
  ifc_0_33_mos_msvc_is_union                          = 1029,
  ifc_0_33_mos_msvc_is_enum                           = 1030,
  ifc_0_33_mos_msvc_is_polymorphic                    = 1031,
  ifc_0_33_mos_msvc_is_empty                          = 1032,
  ifc_0_33_mos_msvc_is_trivially_copy_constructible   = 1033,
  ifc_0_33_mos_msvc_is_trivially_copy_assignable      = 1034,
  ifc_0_33_mos_msvc_is_trivially_destructible         = 1035,
  ifc_0_33_mos_msvc_has_virtual_destructor            = 1036,
  ifc_0_33_mos_msvc_is_nothrow_copy_constructible     = 1037,
  ifc_0_33_mos_msvc_is_nothrow_copy_assignable        = 1038,
  ifc_0_33_mos_msvc_is_pod                            = 1039,
  ifc_0_33_mos_msvc_is_abstract                       = 1040,
  ifc_0_33_mos_msvc_is_trivial                        = 1041,
  ifc_0_33_mos_msvc_is_trivially_copyable             = 1042,
  ifc_0_33_mos_msvc_is_standard_layout                = 1043,
  ifc_0_33_mos_msvc_is_literal_type                   = 1044,
  ifc_0_33_mos_msvc_is_trivially_move_constructible   = 1045,
  ifc_0_33_mos_msvc_has_trivial_move_assign           = 1046,
  ifc_0_33_mos_msvc_is_trivially_move_assignable      = 1047,
  ifc_0_33_mos_msvc_is_nothrow_move_assignable        = 1048,
  ifc_0_33_mos_msvc_underlying_type                   = 1049,
  ifc_0_33_mos_msvc_is_destructible                   = 1050,
  ifc_0_33_mos_msvc_is_nothrow_destructible           = 1051,
  ifc_0_33_mos_msvc_has_unique_object_representations = 1052,
  ifc_0_33_mos_msvc_is_aggregate                      = 1053,
  ifc_0_33_mos_msvc_builtin_address_of                = 1054,
  ifc_0_33_mos_msvc_is_ref_class                      = 1055,
  ifc_0_33_mos_msvc_is_value_class                    = 1056,
  ifc_0_33_mos_msvc_is_simple_value_class             = 1057,
  ifc_0_33_mos_msvc_is_interface_class                = 1058,
  ifc_0_33_mos_msvc_is_delegate                       = 1059,
  ifc_0_33_mos_msvc_is_final                          = 1060,
  ifc_0_33_mos_msvc_is_sealed                         = 1061,
  ifc_0_33_mos_msvc_has_finalizer                     = 1062,
  ifc_0_33_mos_msvc_has_copy                          = 1063,
  ifc_0_33_mos_msvc_has_assign                        = 1064,
  ifc_0_33_mos_msvc_has_user_destructor               = 1065,
  ifc_0_33_mos_msvc_confusion                         = 4064,
  ifc_0_33_mos_msvc_confused_expand                   = 4065,
  ifc_0_33_mos_msvc_confused_dependent_sizeof         = 4066
};  /* an_ifc_monadic_operator_sort_0_33 */


enum an_ifc_monadic_operator_sort {
  ifc_mos_address,
  ifc_mos_alignas,
  ifc_mos_alignof,
  ifc_mos_await,
  ifc_mos_brace,
  ifc_mos_cardinality,
  ifc_mos_ceil,
  ifc_mos_co_return,
  ifc_mos_complement,
  ifc_mos_delete,
  ifc_mos_delete_array,
  ifc_mos_deref,
  ifc_mos_expand,
  ifc_mos_floor,
  ifc_mos_lookup_globally,
  ifc_mos_materialize,
  ifc_mos_msvc,
  ifc_mos_msvc_alignof,
  ifc_mos_msvc_assume,
  ifc_mos_msvc_builtin_address_of,
  ifc_mos_msvc_confused_dependent_sizeof,
  ifc_mos_msvc_confused_expand,
  ifc_mos_msvc_confusion,
  ifc_mos_msvc_has_assign,
  ifc_mos_msvc_has_copy,
  ifc_mos_msvc_has_finalizer,
  ifc_mos_msvc_has_trivial_move_assign,
  ifc_mos_msvc_has_unique_object_representations,
  ifc_mos_msvc_has_user_destructor,
  ifc_mos_msvc_has_virtual_destructor,
  ifc_mos_msvc_is_abstract,
  ifc_mos_msvc_is_aggregate,
  ifc_mos_msvc_is_class,
  ifc_mos_msvc_is_delegate,
  ifc_mos_msvc_is_destructible,
  ifc_mos_msvc_is_empty,
  ifc_mos_msvc_is_enum,
  ifc_mos_msvc_is_final,
  ifc_mos_msvc_is_interface_class,
  ifc_mos_msvc_is_literal_type,
  ifc_mos_msvc_is_nothrow_copy_assignable,
  ifc_mos_msvc_is_nothrow_copy_constructible,
  ifc_mos_msvc_is_nothrow_destructible,
  ifc_mos_msvc_is_nothrow_move_assignable,
  ifc_mos_msvc_is_pod,
  ifc_mos_msvc_is_polymorphic,
  ifc_mos_msvc_is_ref_class,
  ifc_mos_msvc_is_sealed,
  ifc_mos_msvc_is_simple_value_class,
  ifc_mos_msvc_is_standard_layout,
  ifc_mos_msvc_is_trivial,
  ifc_mos_msvc_is_trivially_copy_assignable,
  ifc_mos_msvc_is_trivially_copy_constructible,
  ifc_mos_msvc_is_trivially_copyable,
  ifc_mos_msvc_is_trivially_destructible,
  ifc_mos_msvc_is_trivially_move_assignable,
  ifc_mos_msvc_is_trivially_move_constructible,
  ifc_mos_msvc_is_union,
  ifc_mos_msvc_is_value_class,
  ifc_mos_msvc_underlying_type,
  ifc_mos_msvc_uuidof,
  ifc_mos_negate,
  ifc_mos_new,
  ifc_mos_noexcept,
  ifc_mos_not,
  ifc_mos_paren,
  ifc_mos_plus,
  ifc_mos_post_decrement,
  ifc_mos_post_increment,
  ifc_mos_pre_decrement,
  ifc_mos_pre_increment,
  ifc_mos_pseudo_dtor_call,
  ifc_mos_read,
  ifc_mos_requires,
  ifc_mos_sizeof,
  ifc_mos_throw,
  ifc_mos_truncate,
  ifc_mos_typeid,
  ifc_mos_unknown,
  ifc_mos_yield
};  /* an_ifc_monadic_operator_sort */


enum an_ifc_name_sort_0_33 : uint32_t {
  ifc_0_33_ns_text_offset         = 0,
  ifc_0_33_ns_name_operator       = 1,
  ifc_0_33_ns_name_conversion     = 2,
  ifc_0_33_ns_name_literal        = 3,
  ifc_0_33_ns_name_template       = 4,
  ifc_0_33_ns_name_specialization = 5,
  ifc_0_33_ns_name_source_file    = 6,
  ifc_0_33_ns_name_guide          = 7
};  /* an_ifc_name_sort_0_33 */


enum an_ifc_name_sort {
  ifc_ns_name_conversion,
  ifc_ns_name_guide,
  ifc_ns_name_literal,
  ifc_ns_name_operator,
  ifc_ns_name_source_file,
  ifc_ns_name_specialization,
  ifc_ns_name_template,
  ifc_ns_text_offset
};  /* an_ifc_name_sort */


enum an_ifc_niladic_operator_sort_0_33 : uint16_t {
  ifc_0_33_nos_unknown              = 0,
  ifc_0_33_nos_phantom              = 1,
  ifc_0_33_nos_constant             = 2,
  ifc_0_33_nos_nil                  = 3,
  ifc_0_33_nos_msvc                 = 1024,
  ifc_0_33_nos_msvc_constant_object = 1025,
  ifc_0_33_nos_msvc_lambda          = 1026
};  /* an_ifc_niladic_operator_sort_0_33 */


enum an_ifc_niladic_operator_sort {
  ifc_nos_constant,
  ifc_nos_msvc,
  ifc_nos_msvc_constant_object,
  ifc_nos_msvc_lambda,
  ifc_nos_nil,
  ifc_nos_phantom,
  ifc_nos_unknown
};  /* an_ifc_niladic_operator_sort */


enum an_ifc_noexcept_sort_0_33 : uint8_t {
  ifc_0_33_ns_none       = 0,
  ifc_0_33_ns_false      = 1,
  ifc_0_33_ns_true       = 2,
  ifc_0_33_ns_expression = 3,
  ifc_0_33_ns_inferred   = 4,
  ifc_0_33_ns_unenforced = 5
};  /* an_ifc_noexcept_sort_0_33 */


enum an_ifc_noexcept_sort {
  ifc_ns_expression,
  ifc_ns_false,
  ifc_ns_inferred,
  ifc_ns_none,
  ifc_ns_true,
  ifc_ns_unenforced
};  /* an_ifc_noexcept_sort */


enum an_ifc_operator_sort_0_33 : uint16_t {
  ifc_0_33_os_niladic_operator             = 0,
  ifc_0_33_os_monadic_operator             = 1,
  ifc_0_33_os_dyadic_operator              = 2,
  ifc_0_33_os_triadic_operator             = 3,
  ifc_0_33_os_storage_instruction_operator = 14,
  ifc_0_33_os_variadic_operator            = 15
};  /* an_ifc_operator_sort_0_33 */


enum an_ifc_operator_sort {
  ifc_os_dyadic_operator,
  ifc_os_monadic_operator,
  ifc_os_niladic_operator,
  ifc_os_storage_instruction_operator,
  ifc_os_triadic_operator,
  ifc_os_variadic_operator
};  /* an_ifc_operator_sort */


enum an_ifc_parameter_sort_0_33 : uint8_t {
  ifc_0_33_ps_object   = 0,
  ifc_0_33_ps_type     = 1,
  ifc_0_33_ps_non_type = 2,
  ifc_0_33_ps_template = 3
};  /* an_ifc_parameter_sort_0_33 */


enum an_ifc_parameter_sort {
  ifc_ps_non_type,
  ifc_ps_object,
  ifc_ps_template,
  ifc_ps_type
};  /* an_ifc_parameter_sort */


enum an_ifc_pointer_declarator_sort_0_33 : uint8_t {
  ifc_0_33_pds_none              = 0,
  ifc_0_33_pds_pointer           = 1,
  ifc_0_33_pds_lvalue_reference  = 2,
  ifc_0_33_pds_rvalue_reference  = 3,
  ifc_0_33_pds_pointer_to_member = 4
};  /* an_ifc_pointer_declarator_sort_0_33 */


enum an_ifc_pointer_declarator_sort {
  ifc_pds_lvalue_reference,
  ifc_pds_none,
  ifc_pds_pointer,
  ifc_pds_pointer_to_member,
  ifc_pds_rvalue_reference
};  /* an_ifc_pointer_declarator_sort */


enum an_ifc_pragma_sort_0_33 : uint32_t {
  ifc_0_33_ps_vendor_extension = 0
};  /* an_ifc_pragma_sort_0_33 */


enum an_ifc_pragma_sort {
  ifc_ps_vendor_extension
};  /* an_ifc_pragma_sort */


enum an_ifc_read_conversion_sort_0_33 : uint8_t {
  ifc_0_33_rcs_identity            = 0,
  ifc_0_33_rcs_indirection         = 1,
  ifc_0_33_rcs_dereference         = 2,
  ifc_0_33_rcs_lvalue_to_rvalue    = 3,
  ifc_0_33_rcs_integral_conversion = 4
};  /* an_ifc_read_conversion_sort_0_33 */


enum an_ifc_read_conversion_sort {
  ifc_rcs_dereference,
  ifc_rcs_identity,
  ifc_rcs_indirection,
  ifc_rcs_integral_conversion,
  ifc_rcs_lvalue_to_rvalue
};  /* an_ifc_read_conversion_sort */


enum an_ifc_return_sort_0_33 : uint8_t {
  ifc_0_33_rs_return    = 0,
  ifc_0_33_rs_co_return = 1
};  /* an_ifc_return_sort_0_33 */


enum an_ifc_return_sort {
  ifc_rs_co_return,
  ifc_rs_return
};  /* an_ifc_return_sort */


enum an_ifc_source_directive_sort_0_33 : uint16_t {
  ifc_0_33_sds_msvc                                = 8191,
  ifc_0_33_sds_msvc_pragma_push                    = 8192,
  ifc_0_33_sds_msvc_pragma_pop                     = 8193,
  ifc_0_33_sds_msvc_directive_start                = 8194,
  ifc_0_33_sds_msvc_directive_end                  = 8195,
  ifc_0_33_sds_msvc_pragma_alloc_text              = 8196,
  ifc_0_33_sds_msvc_pragma_auto_inline             = 8197,
  ifc_0_33_sds_msvc_pragma_bss_seg                 = 8198,
  ifc_0_33_sds_msvc_pragma_check_stack             = 8199,
  ifc_0_33_sds_msvc_pragma_code_seg                = 8200,
  ifc_0_33_sds_msvc_pragma_comment                 = 8201,
  ifc_0_33_sds_msvc_pragma_component               = 8202,
  ifc_0_33_sds_msvc_pragma_conform                 = 8203,
  ifc_0_33_sds_msvc_pragma_const_seg               = 8204,
  ifc_0_33_sds_msvc_pragma_data_seg                = 8205,
  ifc_0_33_sds_msvc_pragma_deprecated              = 8206,
  ifc_0_33_sds_msvc_pragma_detect_mismatch         = 8207,
  ifc_0_33_sds_msvc_pragma_endregion               = 8208,
  ifc_0_33_sds_msvc_pragma_execution_character_set = 8209,
  ifc_0_33_sds_msvc_pragma_fenv_access             = 8210,
  ifc_0_33_sds_msvc_pragma_file_hash               = 8211,
  ifc_0_33_sds_msvc_pragma_float_control           = 8212,
  ifc_0_33_sds_msvc_pragma_fp_contract             = 8213,
  ifc_0_33_sds_msvc_pragma_function                = 8214,
  ifc_0_33_sds_msvc_pragma_bgi                     = 8215,
  ifc_0_33_sds_msvc_pragma_ident                   = 8216,
  ifc_0_33_sds_msvc_pragma_implementation_key      = 8217,
  ifc_0_33_sds_msvc_pragma_include_alias           = 8218,
  ifc_0_33_sds_msvc_pragma_init_seq                = 8219,
  ifc_0_33_sds_msvc_pragma_inline_depth            = 8220,
  ifc_0_33_sds_msvc_pragma_inline_recursion        = 8221,
  ifc_0_33_sds_msvc_pragma_intrinsic               = 8222,
  ifc_0_33_sds_msvc_pragma_loop                    = 8223,
  ifc_0_33_sds_msvc_pragma_make_public             = 8224,
  ifc_0_33_sds_msvc_pragma_managed                 = 8225,
  ifc_0_33_sds_msvc_pragma_message                 = 8226,
  ifc_0_33_sds_msvc_pragma_omp                     = 8227,
  ifc_0_33_sds_msvc_pragma_optimize                = 8228,
  ifc_0_33_sds_msvc_pragma_pack                    = 8229,
  ifc_0_33_sds_msvc_pragma_pointer_to_members      = 8230,
  ifc_0_33_sds_msvc_pragma_pop_macro               = 8231,
  ifc_0_33_sds_msvc_pragma_prefast                 = 8232,
  ifc_0_33_sds_msvc_pragma_push_macro              = 8233,
  ifc_0_33_sds_msvc_pragma_region                  = 8234,
  ifc_0_33_sds_msvc_pragma_runtime_checks          = 8235,
  ifc_0_33_sds_msvc_pragma_same_seg                = 8236,
  ifc_0_33_sds_msvc_pragma_section                 = 8237,
  ifc_0_33_sds_msvc_pragma_segment                 = 8238,
  ifc_0_33_sds_msvc_pragma_setlocale               = 8239,
  ifc_0_33_sds_msvc_pragma_start_map_region        = 8240,
  ifc_0_33_sds_msvc_pragma_stop_map_region         = 8241,
  ifc_0_33_sds_msvc_pragma_strict_gs_check         = 8242,
  ifc_0_33_sds_msvc_pragma_system_header           = 8243,
  ifc_0_33_sds_msvc_pragma_unmanaged               = 8244,
  ifc_0_33_sds_msvc_pragma_vtordisp                = 8245,
  ifc_0_33_sds_msvc_pragma_warning                 = 8246,
  ifc_0_33_sds_msvc_pragma_p0include               = 8247,
  ifc_0_33_sds_msvc_pragma_p0line                  = 8248
};  /* an_ifc_source_directive_sort_0_33 */


enum an_ifc_source_directive_sort {
  ifc_sds_msvc,
  ifc_sds_msvc_directive_end,
  ifc_sds_msvc_directive_start,
  ifc_sds_msvc_pragma_alloc_text,
  ifc_sds_msvc_pragma_auto_inline,
  ifc_sds_msvc_pragma_bgi,
  ifc_sds_msvc_pragma_bss_seg,
  ifc_sds_msvc_pragma_check_stack,
  ifc_sds_msvc_pragma_code_seg,
  ifc_sds_msvc_pragma_comment,
  ifc_sds_msvc_pragma_component,
  ifc_sds_msvc_pragma_conform,
  ifc_sds_msvc_pragma_const_seg,
  ifc_sds_msvc_pragma_data_seg,
  ifc_sds_msvc_pragma_deprecated,
  ifc_sds_msvc_pragma_detect_mismatch,
  ifc_sds_msvc_pragma_endregion,
  ifc_sds_msvc_pragma_execution_character_set,
  ifc_sds_msvc_pragma_fenv_access,
  ifc_sds_msvc_pragma_file_hash,
  ifc_sds_msvc_pragma_float_control,
  ifc_sds_msvc_pragma_fp_contract,
  ifc_sds_msvc_pragma_function,
  ifc_sds_msvc_pragma_ident,
  ifc_sds_msvc_pragma_implementation_key,
  ifc_sds_msvc_pragma_include_alias,
  ifc_sds_msvc_pragma_init_seq,
  ifc_sds_msvc_pragma_inline_depth,
  ifc_sds_msvc_pragma_inline_recursion,
  ifc_sds_msvc_pragma_intrinsic,
  ifc_sds_msvc_pragma_loop,
  ifc_sds_msvc_pragma_make_public,
  ifc_sds_msvc_pragma_managed,
  ifc_sds_msvc_pragma_message,
  ifc_sds_msvc_pragma_omp,
  ifc_sds_msvc_pragma_optimize,
  ifc_sds_msvc_pragma_p0include,
  ifc_sds_msvc_pragma_p0line,
  ifc_sds_msvc_pragma_pack,
  ifc_sds_msvc_pragma_pointer_to_members,
  ifc_sds_msvc_pragma_pop,
  ifc_sds_msvc_pragma_pop_macro,
  ifc_sds_msvc_pragma_prefast,
  ifc_sds_msvc_pragma_push,
  ifc_sds_msvc_pragma_push_macro,
  ifc_sds_msvc_pragma_region,
  ifc_sds_msvc_pragma_runtime_checks,
  ifc_sds_msvc_pragma_same_seg,
  ifc_sds_msvc_pragma_section,
  ifc_sds_msvc_pragma_segment,
  ifc_sds_msvc_pragma_setlocale,
  ifc_sds_msvc_pragma_start_map_region,
  ifc_sds_msvc_pragma_stop_map_region,
  ifc_sds_msvc_pragma_strict_gs_check,
  ifc_sds_msvc_pragma_system_header,
  ifc_sds_msvc_pragma_unmanaged,
  ifc_sds_msvc_pragma_vtordisp,
  ifc_sds_msvc_pragma_warning
};  /* an_ifc_source_directive_sort */


enum an_ifc_source_identifier_sort_0_33 : uint16_t {
  ifc_0_33_sis_plain                  = 0,
  ifc_0_33_sis_msvc                   = 8191,
  ifc_0_33_sis_msvc_builtin_huge_val  = 8192,
  ifc_0_33_sis_msvc_builtin_huge_valf = 8193,
  ifc_0_33_sis_msvc_builtin_nan       = 8194,
  ifc_0_33_sis_msvc_builtin_nanf      = 8195,
  ifc_0_33_sis_msvc_builtin_nans      = 8196,
  ifc_0_33_sis_msvc_builtin_nansf     = 8197
};  /* an_ifc_source_identifier_sort_0_33 */


enum an_ifc_source_identifier_sort {
  ifc_sis_msvc,
  ifc_sis_msvc_builtin_huge_val,
  ifc_sis_msvc_builtin_huge_valf,
  ifc_sis_msvc_builtin_nan,
  ifc_sis_msvc_builtin_nanf,
  ifc_sis_msvc_builtin_nans,
  ifc_sis_msvc_builtin_nansf,
  ifc_sis_plain
};  /* an_ifc_source_identifier_sort */


enum an_ifc_source_keyword_sort_0_33 : uint16_t {
  ifc_0_33_sks_unknown                                             = 0,
  ifc_0_33_sks_alignas                                             = 1,
  ifc_0_33_sks_alignof                                             = 2,
  ifc_0_33_sks_asm                                                 = 3,
  ifc_0_33_sks_auto                                                = 4,
  ifc_0_33_sks_bool                                                = 5,
  ifc_0_33_sks_break                                               = 6,
  ifc_0_33_sks_case                                                = 7,
  ifc_0_33_sks_catch                                               = 8,
  ifc_0_33_sks_char                                                = 9,
  ifc_0_33_sks_char8_t                                             = 10,
  ifc_0_33_sks_char16_t                                            = 11,
  ifc_0_33_sks_char32_t                                            = 12,
  ifc_0_33_sks_class                                               = 13,
  ifc_0_33_sks_concept                                             = 14,
  ifc_0_33_sks_const                                               = 15,
  ifc_0_33_sks_consteval                                           = 16,
  ifc_0_33_sks_constexpr                                           = 17,
  ifc_0_33_sks_constinit                                           = 18,
  ifc_0_33_sks_const_cast                                          = 19,
  ifc_0_33_sks_continue                                            = 20,
  ifc_0_33_sks_co_await                                            = 21,
  ifc_0_33_sks_co_return                                           = 22,
  ifc_0_33_sks_co_yield                                            = 23,
  ifc_0_33_sks_decltype                                            = 24,
  ifc_0_33_sks_default                                             = 25,
  ifc_0_33_sks_delete                                              = 26,
  ifc_0_33_sks_do                                                  = 27,
  ifc_0_33_sks_double                                              = 28,
  ifc_0_33_sks_dynamic_cast                                        = 29,
  ifc_0_33_sks_else                                                = 30,
  ifc_0_33_sks_enum                                                = 31,
  ifc_0_33_sks_explicit                                            = 32,
  ifc_0_33_sks_export                                              = 33,
  ifc_0_33_sks_extern                                              = 34,
  ifc_0_33_sks_false                                               = 35,
  ifc_0_33_sks_float                                               = 36,
  ifc_0_33_sks_for                                                 = 37,
  ifc_0_33_sks_friend                                              = 38,
  ifc_0_33_sks_generic                                             = 39,
  ifc_0_33_sks_goto                                                = 40,
  ifc_0_33_sks_if                                                  = 41,
  ifc_0_33_sks_inline                                              = 42,
  ifc_0_33_sks_int                                                 = 43,
  ifc_0_33_sks_long                                                = 44,
  ifc_0_33_sks_mutable                                             = 45,
  ifc_0_33_sks_namespace                                           = 46,
  ifc_0_33_sks_new                                                 = 47,
  ifc_0_33_sks_noexcept                                            = 48,
  ifc_0_33_sks_nullptr                                             = 49,
  ifc_0_33_sks_operator                                            = 50,
  ifc_0_33_sks_pragma                                              = 51,
  ifc_0_33_sks_private                                             = 52,
  ifc_0_33_sks_protected                                           = 53,
  ifc_0_33_sks_public                                              = 54,
  ifc_0_33_sks_register                                            = 55,
  ifc_0_33_sks_reinterpret_cast                                    = 56,
  ifc_0_33_sks_requires                                            = 57,
  ifc_0_33_sks_restrict                                            = 58,
  ifc_0_33_sks_return                                              = 59,
  ifc_0_33_sks_short                                               = 60,
  ifc_0_33_sks_signed                                              = 61,
  ifc_0_33_sks_sizeof                                              = 62,
  ifc_0_33_sks_static                                              = 63,
  ifc_0_33_sks_static_assert                                       = 64,
  ifc_0_33_sks_static_cast                                         = 65,
  ifc_0_33_sks_struct                                              = 66,
  ifc_0_33_sks_switch                                              = 67,
  ifc_0_33_sks_template                                            = 68,
  ifc_0_33_sks_this                                                = 69,
  ifc_0_33_sks_thread_local                                        = 70,
  ifc_0_33_sks_throw                                               = 71,
  ifc_0_33_sks_true                                                = 72,
  ifc_0_33_sks_try                                                 = 73,
  ifc_0_33_sks_typedef                                             = 74,
  ifc_0_33_sks_typeid                                              = 75,
  ifc_0_33_sks_typename                                            = 76,
  ifc_0_33_sks_union                                               = 77,
  ifc_0_33_sks_unsigned                                            = 78,
  ifc_0_33_sks_using                                               = 79,
  ifc_0_33_sks_virtual                                             = 80,
  ifc_0_33_sks_void                                                = 81,
  ifc_0_33_sks_volatile                                            = 82,
  ifc_0_33_sks_wchar_t                                             = 83,
  ifc_0_33_sks_while                                               = 84,
  ifc_0_33_sks_msvc                                                = 8191,
  ifc_0_33_sks_msvc_asm                                            = 8192,
  ifc_0_33_sks_msvc_assume                                         = 8193,
  ifc_0_33_sks_msvc_alignof                                        = 8194,
  ifc_0_33_sks_msvc_based                                          = 8195,
  ifc_0_33_sks_msvc_cdecl                                          = 8196,
  ifc_0_33_sks_msvc_clrcall                                        = 8197,
  ifc_0_33_sks_msvc_declspec                                       = 8198,
  ifc_0_33_sks_msvc_eabi                                           = 8199,
  ifc_0_33_sks_msvc_event                                          = 8200,
  ifc_0_33_sks_msvc_seh_except                                     = 8201,
  ifc_0_33_sks_msvc_fastcall                                       = 8202,
  ifc_0_33_sks_msvc_seh_finally                                    = 8203,
  ifc_0_33_sks_msvc_forceinline                                    = 8204,
  ifc_0_33_sks_msvc_hook                                           = 8205,
  ifc_0_33_sks_msvc_identifier                                     = 8206,
  ifc_0_33_sks_msvc_if_exists                                      = 8207,
  ifc_0_33_sks_msvc_if_not_exists                                  = 8208,
  ifc_0_33_sks_msvc_int8                                           = 8209,
  ifc_0_33_sks_msvc_int16                                          = 8210,
  ifc_0_33_sks_msvc_int32                                          = 8211,
  ifc_0_33_sks_msvc_int64                                          = 8212,
  ifc_0_33_sks_msvc_int128                                         = 8213,
  ifc_0_33_sks_msvc_interface                                      = 8214,
  ifc_0_33_sks_msvc_leave                                          = 8215,
  ifc_0_33_sks_msvc_multiple_inheritance                           = 8216,
  ifc_0_33_sks_msvc_nullptr                                        = 8217,
  ifc_0_33_sks_msvc_novtordisp                                     = 8218,
  ifc_0_33_sks_msvc_pragma                                         = 8219,
  ifc_0_33_sks_msvc_ptr32                                          = 8220,
  ifc_0_33_sks_msvc_ptr64                                          = 8221,
  ifc_0_33_sks_msvc_restrict                                       = 8222,
  ifc_0_33_sks_msvc_single_inheritance                             = 8223,
  ifc_0_33_sks_msvc_sptr                                           = 8224,
  ifc_0_33_sks_msvc_stdcall                                        = 8225,
  ifc_0_33_sks_msvc_super                                          = 8226,
  ifc_0_33_sks_msvc_thiscall                                       = 8227,
  ifc_0_33_sks_msvc_seh_try                                        = 8228,
  ifc_0_33_sks_msvc_uptr                                           = 8229,
  ifc_0_33_sks_msvc_uuidof                                         = 8230,
  ifc_0_33_sks_msvc_unaligned                                      = 8231,
  ifc_0_33_sks_msvc_unhook                                         = 8232,
  ifc_0_33_sks_msvc_vectorcall                                     = 8233,
  ifc_0_33_sks_msvc_virtual_inheritance                            = 8234,
  ifc_0_33_sks_msvc_w64                                            = 8235,
  ifc_0_33_sks_msvc_is_class                                       = 8236,
  ifc_0_33_sks_msvc_is_union                                       = 8237,
  ifc_0_33_sks_msvc_is_enum                                        = 8238,
  ifc_0_33_sks_msvc_is_polymorphic                                 = 8239,
  ifc_0_33_sks_msvc_is_empty                                       = 8240,
  ifc_0_33_sks_msvc_has_trivial_constructor                        = 8241,
  ifc_0_33_sks_msvc_is_trivially_constructible                     = 8242,
  ifc_0_33_sks_msvc_is_trivially_copy_constructible                = 8243,
  ifc_0_33_sks_msvc_is_trivially_copy_assignable                   = 8244,
  ifc_0_33_sks_msvc_is_trivially_destructible                      = 8245,
  ifc_0_33_sks_msvc_has_virtual_destructor                         = 8246,
  ifc_0_33_sks_msvc_is_nothrow_constructible                       = 8247,
  ifc_0_33_sks_msvc_is_nothrow_copy_constructible                  = 8248,
  ifc_0_33_sks_msvc_is_nothrow_copy_assignable                     = 8249,
  ifc_0_33_sks_msvc_is_pod                                         = 8250,
  ifc_0_33_sks_msvc_is_abstract                                    = 8251,
  ifc_0_33_sks_msvc_is_base_of                                     = 8252,
  ifc_0_33_sks_msvc_is_convertibleto                               = 8253,
  ifc_0_33_sks_msvc_is_trivial                                     = 8254,
  ifc_0_33_sks_msvc_is_trivially_copyable                          = 8255,
  ifc_0_33_sks_msvc_is_standard_layout                             = 8256,
  ifc_0_33_sks_msvc_is_literal_type                                = 8257,
  ifc_0_33_sks_msvc_is_trivially_move_constructible                = 8258,
  ifc_0_33_sks_msvc_has_trivial_move_assign                        = 8259,
  ifc_0_33_sks_msvc_is_trivially_move_assignable                   = 8260,
  ifc_0_33_sks_msvc_is_nothrow_move_assignable                     = 8261,
  ifc_0_33_sks_msvc_is_constructible                               = 8262,
  ifc_0_33_sks_msvc_underlying_type                                = 8263,
  ifc_0_33_sks_msvc_is_trivially_assignable                        = 8264,
  ifc_0_33_sks_msvc_is_nothrow_assignable                          = 8265,
  ifc_0_33_sks_msvc_is_destructible                                = 8266,
  ifc_0_33_sks_msvc_is_nothrow_destructible                        = 8267,
  ifc_0_33_sks_msvc_is_assignable                                  = 8268,
  ifc_0_33_sks_msvc_is_assignable_no_check                         = 8269,
  ifc_0_33_sks_msvc_has_unique_object_representations              = 8270,
  ifc_0_33_sks_msvc_is_aggregate                                   = 8271,
  ifc_0_33_sks_msvc_builtin_address_of                             = 8272,
  ifc_0_33_sks_msvc_builtin_offset_of                              = 8273,
  ifc_0_33_sks_msvc_builtin_bit_cast                               = 8274,
  ifc_0_33_sks_msvc_builtin_is_layout_compatible                   = 8275,
  ifc_0_33_sks_msvc_builtin_is_pointer_interconvertible_base_of    = 8276,
  ifc_0_33_sks_msvc_builtin_is_pointer_interconvertible_with_class = 8277,
  ifc_0_33_sks_msvc_builtin_is_corresponding_member                = 8278,
  ifc_0_33_sks_msvc_is_ref_class                                   = 8279,
  ifc_0_33_sks_msvc_is_value_class                                 = 8280,
  ifc_0_33_sks_msvc_is_simple_value_class                          = 8281,
  ifc_0_33_sks_msvc_is_interface_class                             = 8282,
  ifc_0_33_sks_msvc_is_delegate                                    = 8283,
  ifc_0_33_sks_msvc_is_final                                       = 8284,
  ifc_0_33_sks_msvc_is_sealed                                      = 8285,
  ifc_0_33_sks_msvc_has_finalizer                                  = 8286,
  ifc_0_33_sks_msvc_has_copy                                       = 8287,
  ifc_0_33_sks_msvc_has_assign                                     = 8288,
  ifc_0_33_sks_msvc_has_user_destructor                            = 8289,
  ifc_0_33_sks_msvc_pack_cardinality                               = 8290,
  ifc_0_33_sks_msvc_confused_sizeof                                = 8291,
  ifc_0_33_sks_msvc_confused_alignas                               = 8292
};  /* an_ifc_source_keyword_sort_0_33 */


enum an_ifc_source_keyword_sort {
  ifc_sks_alignas,
  ifc_sks_alignof,
  ifc_sks_asm,
  ifc_sks_auto,
  ifc_sks_bool,
  ifc_sks_break,
  ifc_sks_case,
  ifc_sks_catch,
  ifc_sks_char,
  ifc_sks_char16_t,
  ifc_sks_char32_t,
  ifc_sks_char8_t,
  ifc_sks_class,
  ifc_sks_co_await,
  ifc_sks_co_return,
  ifc_sks_co_yield,
  ifc_sks_concept,
  ifc_sks_const,
  ifc_sks_const_cast,
  ifc_sks_consteval,
  ifc_sks_constexpr,
  ifc_sks_constinit,
  ifc_sks_continue,
  ifc_sks_decltype,
  ifc_sks_default,
  ifc_sks_delete,
  ifc_sks_do,
  ifc_sks_double,
  ifc_sks_dynamic_cast,
  ifc_sks_else,
  ifc_sks_enum,
  ifc_sks_explicit,
  ifc_sks_export,
  ifc_sks_extern,
  ifc_sks_false,
  ifc_sks_float,
  ifc_sks_for,
  ifc_sks_friend,
  ifc_sks_generic,
  ifc_sks_goto,
  ifc_sks_if,
  ifc_sks_inline,
  ifc_sks_int,
  ifc_sks_long,
  ifc_sks_msvc,
  ifc_sks_msvc_alignof,
  ifc_sks_msvc_asm,
  ifc_sks_msvc_assume,
  ifc_sks_msvc_based,
  ifc_sks_msvc_builtin_address_of,
  ifc_sks_msvc_builtin_bit_cast,
  ifc_sks_msvc_builtin_is_corresponding_member,
  ifc_sks_msvc_builtin_is_layout_compatible,
  ifc_sks_msvc_builtin_is_pointer_interconvertible_base_of,
  ifc_sks_msvc_builtin_is_pointer_interconvertible_with_class,
  ifc_sks_msvc_builtin_offset_of,
  ifc_sks_msvc_cdecl,
  ifc_sks_msvc_clrcall,
  ifc_sks_msvc_confused_alignas,
  ifc_sks_msvc_confused_sizeof,
  ifc_sks_msvc_declspec,
  ifc_sks_msvc_eabi,
  ifc_sks_msvc_event,
  ifc_sks_msvc_fastcall,
  ifc_sks_msvc_forceinline,
  ifc_sks_msvc_has_assign,
  ifc_sks_msvc_has_copy,
  ifc_sks_msvc_has_finalizer,
  ifc_sks_msvc_has_trivial_constructor,
  ifc_sks_msvc_has_trivial_move_assign,
  ifc_sks_msvc_has_unique_object_representations,
  ifc_sks_msvc_has_user_destructor,
  ifc_sks_msvc_has_virtual_destructor,
  ifc_sks_msvc_hook,
  ifc_sks_msvc_identifier,
  ifc_sks_msvc_if_exists,
  ifc_sks_msvc_if_not_exists,
  ifc_sks_msvc_int128,
  ifc_sks_msvc_int16,
  ifc_sks_msvc_int32,
  ifc_sks_msvc_int64,
  ifc_sks_msvc_int8,
  ifc_sks_msvc_interface,
  ifc_sks_msvc_is_abstract,
  ifc_sks_msvc_is_aggregate,
  ifc_sks_msvc_is_assignable,
  ifc_sks_msvc_is_assignable_no_check,
  ifc_sks_msvc_is_base_of,
  ifc_sks_msvc_is_class,
  ifc_sks_msvc_is_constructible,
  ifc_sks_msvc_is_convertibleto,
  ifc_sks_msvc_is_delegate,
  ifc_sks_msvc_is_destructible,
  ifc_sks_msvc_is_empty,
  ifc_sks_msvc_is_enum,
  ifc_sks_msvc_is_final,
  ifc_sks_msvc_is_interface_class,
  ifc_sks_msvc_is_literal_type,
  ifc_sks_msvc_is_nothrow_assignable,
  ifc_sks_msvc_is_nothrow_constructible,
  ifc_sks_msvc_is_nothrow_copy_assignable,
  ifc_sks_msvc_is_nothrow_copy_constructible,
  ifc_sks_msvc_is_nothrow_destructible,
  ifc_sks_msvc_is_nothrow_move_assignable,
  ifc_sks_msvc_is_pod,
  ifc_sks_msvc_is_polymorphic,
  ifc_sks_msvc_is_ref_class,
  ifc_sks_msvc_is_sealed,
  ifc_sks_msvc_is_simple_value_class,
  ifc_sks_msvc_is_standard_layout,
  ifc_sks_msvc_is_trivial,
  ifc_sks_msvc_is_trivially_assignable,
  ifc_sks_msvc_is_trivially_constructible,
  ifc_sks_msvc_is_trivially_copy_assignable,
  ifc_sks_msvc_is_trivially_copy_constructible,
  ifc_sks_msvc_is_trivially_copyable,
  ifc_sks_msvc_is_trivially_destructible,
  ifc_sks_msvc_is_trivially_move_assignable,
  ifc_sks_msvc_is_trivially_move_constructible,
  ifc_sks_msvc_is_union,
  ifc_sks_msvc_is_value_class,
  ifc_sks_msvc_leave,
  ifc_sks_msvc_multiple_inheritance,
  ifc_sks_msvc_novtordisp,
  ifc_sks_msvc_nullptr,
  ifc_sks_msvc_pack_cardinality,
  ifc_sks_msvc_pragma,
  ifc_sks_msvc_ptr32,
  ifc_sks_msvc_ptr64,
  ifc_sks_msvc_restrict,
  ifc_sks_msvc_seh_except,
  ifc_sks_msvc_seh_finally,
  ifc_sks_msvc_seh_try,
  ifc_sks_msvc_single_inheritance,
  ifc_sks_msvc_sptr,
  ifc_sks_msvc_stdcall,
  ifc_sks_msvc_super,
  ifc_sks_msvc_thiscall,
  ifc_sks_msvc_unaligned,
  ifc_sks_msvc_underlying_type,
  ifc_sks_msvc_unhook,
  ifc_sks_msvc_uptr,
  ifc_sks_msvc_uuidof,
  ifc_sks_msvc_vectorcall,
  ifc_sks_msvc_virtual_inheritance,
  ifc_sks_msvc_w64,
  ifc_sks_mutable,
  ifc_sks_namespace,
  ifc_sks_new,
  ifc_sks_noexcept,
  ifc_sks_nullptr,
  ifc_sks_operator,
  ifc_sks_pragma,
  ifc_sks_private,
  ifc_sks_protected,
  ifc_sks_public,
  ifc_sks_register,
  ifc_sks_reinterpret_cast,
  ifc_sks_requires,
  ifc_sks_restrict,
  ifc_sks_return,
  ifc_sks_short,
  ifc_sks_signed,
  ifc_sks_sizeof,
  ifc_sks_static,
  ifc_sks_static_assert,
  ifc_sks_static_cast,
  ifc_sks_struct,
  ifc_sks_switch,
  ifc_sks_template,
  ifc_sks_this,
  ifc_sks_thread_local,
  ifc_sks_throw,
  ifc_sks_true,
  ifc_sks_try,
  ifc_sks_typedef,
  ifc_sks_typeid,
  ifc_sks_typename,
  ifc_sks_union,
  ifc_sks_unknown,
  ifc_sks_unsigned,
  ifc_sks_using,
  ifc_sks_virtual,
  ifc_sks_void,
  ifc_sks_volatile,
  ifc_sks_wchar_t,
  ifc_sks_while
};  /* an_ifc_source_keyword_sort */


enum an_ifc_source_literal_sort_0_33 : uint16_t {
  ifc_0_33_sls_unknown                  = 0,
  ifc_0_33_sls_scalar                   = 1,
  ifc_0_33_sls_string                   = 2,
  ifc_0_33_sls_defined_string           = 3,
  ifc_0_33_sls_msvc                     = 8191,
  ifc_0_33_sls_msvc_function_name_macro = 8192,
  ifc_0_33_sls_msvc_string_prefix_macro = 8193,
  ifc_0_33_sls_msvc_binding             = 8194,
  ifc_0_33_sls_msvc_resolved_type       = 8195,
  ifc_0_33_sls_msvc_defined_constant    = 8196,
  ifc_0_33_sls_msvc_cast_target_type    = 8197
};  /* an_ifc_source_literal_sort_0_33 */


enum an_ifc_source_literal_sort {
  ifc_sls_defined_string,
  ifc_sls_msvc,
  ifc_sls_msvc_binding,
  ifc_sls_msvc_cast_target_type,
  ifc_sls_msvc_defined_constant,
  ifc_sls_msvc_function_name_macro,
  ifc_sls_msvc_resolved_type,
  ifc_sls_msvc_string_prefix_macro,
  ifc_sls_scalar,
  ifc_sls_string,
  ifc_sls_unknown
};  /* an_ifc_source_literal_sort */


enum an_ifc_source_operator_sort_0_33 : uint16_t {
  ifc_0_33_sos_unknown             = 0,
  ifc_0_33_sos_equal               = 1,
  ifc_0_33_sos_comma               = 2,
  ifc_0_33_sos_exclaim             = 3,
  ifc_0_33_sos_plus                = 4,
  ifc_0_33_sos_dash                = 5,
  ifc_0_33_sos_star                = 6,
  ifc_0_33_sos_slash               = 7,
  ifc_0_33_sos_percent             = 8,
  ifc_0_33_sos_left_chevron        = 9,
  ifc_0_33_sos_right_chevron       = 10,
  ifc_0_33_sos_tilde               = 11,
  ifc_0_33_sos_caret               = 12,
  ifc_0_33_sos_bar                 = 13,
  ifc_0_33_sos_ampersand           = 14,
  ifc_0_33_sos_plus_plus           = 15,
  ifc_0_33_sos_dash_dash           = 16,
  ifc_0_33_sos_less                = 17,
  ifc_0_33_sos_less_equal          = 18,
  ifc_0_33_sos_greater             = 19,
  ifc_0_33_sos_greater_equal       = 20,
  ifc_0_33_sos_equal_equal         = 21,
  ifc_0_33_sos_exclaim_equal       = 22,
  ifc_0_33_sos_diamond             = 23,
  ifc_0_33_sos_plus_equal          = 24,
  ifc_0_33_sos_dash_equal          = 25,
  ifc_0_33_sos_star_equal          = 26,
  ifc_0_33_sos_slash_equal         = 27,
  ifc_0_33_sos_percent_equal       = 28,
  ifc_0_33_sos_ampersand_equal     = 29,
  ifc_0_33_sos_bar_equal           = 30,
  ifc_0_33_sos_caret_equal         = 31,
  ifc_0_33_sos_left_chevron_equal  = 32,
  ifc_0_33_sos_right_chevron_equal = 33,
  ifc_0_33_sos_ampersand_ampersand = 34,
  ifc_0_33_sos_bar_bar             = 35,
  ifc_0_33_sos_ellipsis            = 36,
  ifc_0_33_sos_dot                 = 37,
  ifc_0_33_sos_arrow               = 38,
  ifc_0_33_sos_dot_star            = 39,
  ifc_0_33_sos_arrow_star          = 40
};  /* an_ifc_source_operator_sort_0_33 */


enum an_ifc_source_operator_sort {
  ifc_sos_ampersand,
  ifc_sos_ampersand_ampersand,
  ifc_sos_ampersand_equal,
  ifc_sos_arrow,
  ifc_sos_arrow_star,
  ifc_sos_bar,
  ifc_sos_bar_bar,
  ifc_sos_bar_equal,
  ifc_sos_caret,
  ifc_sos_caret_equal,
  ifc_sos_comma,
  ifc_sos_dash,
  ifc_sos_dash_dash,
  ifc_sos_dash_equal,
  ifc_sos_diamond,
  ifc_sos_dot,
  ifc_sos_dot_star,
  ifc_sos_ellipsis,
  ifc_sos_equal,
  ifc_sos_equal_equal,
  ifc_sos_exclaim,
  ifc_sos_exclaim_equal,
  ifc_sos_greater,
  ifc_sos_greater_equal,
  ifc_sos_left_chevron,
  ifc_sos_left_chevron_equal,
  ifc_sos_less,
  ifc_sos_less_equal,
  ifc_sos_percent,
  ifc_sos_percent_equal,
  ifc_sos_plus,
  ifc_sos_plus_equal,
  ifc_sos_plus_plus,
  ifc_sos_right_chevron,
  ifc_sos_right_chevron_equal,
  ifc_sos_slash,
  ifc_sos_slash_equal,
  ifc_sos_star,
  ifc_sos_star_equal,
  ifc_sos_tilde,
  ifc_sos_unknown
};  /* an_ifc_source_operator_sort */


enum an_ifc_source_punctuator_sort_0_33 : uint16_t {
  ifc_0_33_sps_unknown                     = 0,
  ifc_0_33_sps_left_parenthesis            = 1,
  ifc_0_33_sps_right_parenthesis           = 2,
  ifc_0_33_sps_left_bracket                = 3,
  ifc_0_33_sps_right_bracket               = 4,
  ifc_0_33_sps_left_brace                  = 5,
  ifc_0_33_sps_right_brace                 = 6,
  ifc_0_33_sps_colon                       = 7,
  ifc_0_33_sps_question                    = 8,
  ifc_0_33_sps_semicolon                   = 9,
  ifc_0_33_sps_colon_colon                 = 10,
  ifc_0_33_sps_msvc                        = 8191,
  ifc_0_33_sps_msvc_zero_width_space       = 8192,
  ifc_0_33_sps_msvc_end_of_phrase          = 8193,
  ifc_0_33_sps_msvc_full_stop              = 8194,
  ifc_0_33_sps_msvc_nested_template_start  = 8195,
  ifc_0_33_sps_msvc_default_argument_start = 8196,
  ifc_0_33_sps_msvc_alignas_edict_start    = 8197,
  ifc_0_33_sps_msvc_default_init_start     = 8198
};  /* an_ifc_source_punctuator_sort_0_33 */


enum an_ifc_source_punctuator_sort {
  ifc_sps_colon,
  ifc_sps_colon_colon,
  ifc_sps_left_brace,
  ifc_sps_left_bracket,
  ifc_sps_left_parenthesis,
  ifc_sps_msvc,
  ifc_sps_msvc_alignas_edict_start,
  ifc_sps_msvc_default_argument_start,
  ifc_sps_msvc_default_init_start,
  ifc_sps_msvc_end_of_phrase,
  ifc_sps_msvc_full_stop,
  ifc_sps_msvc_nested_template_start,
  ifc_sps_msvc_zero_width_space,
  ifc_sps_question,
  ifc_sps_right_brace,
  ifc_sps_right_bracket,
  ifc_sps_right_parenthesis,
  ifc_sps_semicolon,
  ifc_sps_unknown
};  /* an_ifc_source_punctuator_sort */


enum an_ifc_specialization_sort_0_33 : uint8_t {
  ifc_0_33_ss_implicit      = 0,
  ifc_0_33_ss_explicit      = 1,
  ifc_0_33_ss_instantiation = 2
};  /* an_ifc_specialization_sort_0_33 */


enum an_ifc_specialization_sort {
  ifc_ss_explicit,
  ifc_ss_implicit,
  ifc_ss_instantiation
};  /* an_ifc_specialization_sort */


enum an_ifc_stmt_sort_0_33 : uint32_t {
  ifc_0_33_ss_stmt_vendor_extension = 0,
  ifc_0_33_ss_stmt_empty            = 1,
  ifc_0_33_ss_stmt_if               = 2,
  ifc_0_33_ss_stmt_for              = 3,
  ifc_0_33_ss_stmt_case             = 4,
  ifc_0_33_ss_stmt_while            = 5,
  ifc_0_33_ss_stmt_block            = 6,
  ifc_0_33_ss_stmt_break            = 7,
  ifc_0_33_ss_stmt_switch           = 8,
  ifc_0_33_ss_stmt_do_while         = 9,
  ifc_0_33_ss_stmt_default          = 10,
  ifc_0_33_ss_stmt_continue         = 11,
  ifc_0_33_ss_stmt_expression       = 12,
  ifc_0_33_ss_stmt_return           = 13,
  ifc_0_33_ss_stmt_variable_decl    = 14,
  ifc_0_33_ss_stmt_expansion        = 15,
  ifc_0_33_ss_stmt_syntax_tree      = 16
};  /* an_ifc_stmt_sort_0_33 */


enum an_ifc_stmt_sort {
  ifc_ss_stmt_block,
  ifc_ss_stmt_break,
  ifc_ss_stmt_case,
  ifc_ss_stmt_continue,
  ifc_ss_stmt_default,
  ifc_ss_stmt_do_while,
  ifc_ss_stmt_empty,
  ifc_ss_stmt_expansion,
  ifc_ss_stmt_expression,
  ifc_ss_stmt_for,
  ifc_ss_stmt_if,
  ifc_ss_stmt_return,
  ifc_ss_stmt_switch,
  ifc_ss_stmt_syntax_tree,
  ifc_ss_stmt_variable_decl,
  ifc_ss_stmt_vendor_extension,
  ifc_ss_stmt_while
};  /* an_ifc_stmt_sort */


enum an_ifc_storage_instruction_operator_sort_0_33 : uint16_t {
  ifc_0_33_sios_unknown           = 0,
  ifc_0_33_sios_allocate_single   = 1,
  ifc_0_33_sios_allocate_array    = 2,
  ifc_0_33_sios_deallocate_single = 3,
  ifc_0_33_sios_deallocate_array  = 4,
  ifc_0_33_sios_msvc              = 2014
};  /* an_ifc_storage_instruction_operator_sort_0_33 */


enum an_ifc_storage_instruction_operator_sort {
  ifc_sios_allocate_array,
  ifc_sios_allocate_single,
  ifc_sios_deallocate_array,
  ifc_sios_deallocate_single,
  ifc_sios_msvc,
  ifc_sios_unknown
};  /* an_ifc_storage_instruction_operator_sort */


enum an_ifc_string_sort_0_33 : uint32_t {
  ifc_0_33_ss_ordinary = 0,
  ifc_0_33_ss_utf8     = 1,
  ifc_0_33_ss_char16   = 2,
  ifc_0_33_ss_char32   = 3,
  ifc_0_33_ss_wide     = 4
};  /* an_ifc_string_sort_0_33 */


enum an_ifc_string_sort {
  ifc_ss_char16,
  ifc_ss_char32,
  ifc_ss_ordinary,
  ifc_ss_utf8,
  ifc_ss_wide
};  /* an_ifc_string_sort */


enum an_ifc_syntax_sort_0_33 : uint32_t {
  ifc_0_33_ss_syntax_vendor_extension               = 0,
  ifc_0_33_ss_syntax_simple_type_specifier          = 1,
  ifc_0_33_ss_syntax_decltype_specifier             = 2,
  ifc_0_33_ss_syntax_placeholder_type_specifier     = 3,
  ifc_0_33_ss_syntax_type_specifier_seq             = 4,
  ifc_0_33_ss_syntax_decl_specifier_seq             = 5,
  ifc_0_33_ss_syntax_virtual_specifier_seq          = 6,
  ifc_0_33_ss_syntax_noexcept_specification         = 7,
  ifc_0_33_ss_syntax_explicit_specifier             = 8,
  ifc_0_33_ss_syntax_enum_specifier                 = 9,
  ifc_0_33_ss_syntax_enumerator_definition          = 10,
  ifc_0_33_ss_syntax_class_specifier                = 11,
  ifc_0_33_ss_syntax_member_specification           = 12,
  ifc_0_33_ss_syntax_member_declaration             = 13,
  ifc_0_33_ss_syntax_member_declarator              = 14,
  ifc_0_33_ss_syntax_access_specifier               = 15,
  ifc_0_33_ss_syntax_base_specifier_list            = 16,
  ifc_0_33_ss_syntax_base_specifier                 = 17,
  ifc_0_33_ss_syntax_type_id                        = 18,
  ifc_0_33_ss_syntax_trailing_return_type           = 19,
  ifc_0_33_ss_syntax_declarator                     = 20,
  ifc_0_33_ss_syntax_pointer_declarator             = 21,
  ifc_0_33_ss_syntax_array_declarator               = 22,
  ifc_0_33_ss_syntax_function_declarator            = 23,
  ifc_0_33_ss_syntax_array_or_function_declarator   = 24,
  ifc_0_33_ss_syntax_parameter_declarator           = 25,
  ifc_0_33_ss_syntax_init_declarator                = 26,
  ifc_0_33_ss_syntax_new_declarator                 = 27,
  ifc_0_33_ss_syntax_simple_declaration             = 28,
  ifc_0_33_ss_syntax_exception_declaration          = 29,
  ifc_0_33_ss_syntax_condition_declaration          = 30,
  ifc_0_33_ss_syntax_static_assert_declaration      = 31,
  ifc_0_33_ss_syntax_alias_declaration              = 32,
  ifc_0_33_ss_syntax_concept_definition             = 33,
  ifc_0_33_ss_syntax_compound_statement             = 34,
  ifc_0_33_ss_syntax_return_statement               = 35,
  ifc_0_33_ss_syntax_if_statement                   = 36,
  ifc_0_33_ss_syntax_while_statement                = 37,
  ifc_0_33_ss_syntax_do_while_statement             = 38,
  ifc_0_33_ss_syntax_for_statement                  = 39,
  ifc_0_33_ss_syntax_init_statement                 = 40,
  ifc_0_33_ss_syntax_range_based_for_statement      = 41,
  ifc_0_33_ss_syntax_for_range_declaration          = 42,
  ifc_0_33_ss_syntax_labeled_statement              = 43,
  ifc_0_33_ss_syntax_break_statement                = 44,
  ifc_0_33_ss_syntax_continue_statement             = 45,
  ifc_0_33_ss_syntax_switch_statement               = 46,
  ifc_0_33_ss_syntax_goto_statement                 = 47,
  ifc_0_33_ss_syntax_declaration_statement          = 48,
  ifc_0_33_ss_syntax_expression_statement           = 49,
  ifc_0_33_ss_syntax_try_block                      = 50,
  ifc_0_33_ss_syntax_handler                        = 51,
  ifc_0_33_ss_syntax_handler_seq                    = 52,
  ifc_0_33_ss_syntax_function_try_block             = 53,
  ifc_0_33_ss_syntax_type_id_list_element           = 54,
  ifc_0_33_ss_syntax_dynamic_exception_spec         = 55,
  ifc_0_33_ss_syntax_statement_seq                  = 56,
  ifc_0_33_ss_syntax_function_body                  = 57,
  ifc_0_33_ss_syntax_expression                     = 58,
  ifc_0_33_ss_syntax_function_definition            = 59,
  ifc_0_33_ss_syntax_member_function_declaration    = 60,
  ifc_0_33_ss_syntax_template_declaration           = 61,
  ifc_0_33_ss_syntax_requires_clause                = 62,
  ifc_0_33_ss_syntax_simple_requirement             = 63,
  ifc_0_33_ss_syntax_type_requirement               = 64,
  ifc_0_33_ss_syntax_compound_requirement           = 65,
  ifc_0_33_ss_syntax_nested_requirement             = 66,
  ifc_0_33_ss_syntax_requirement_body               = 67,
  ifc_0_33_ss_syntax_type_template_parameter        = 68,
  ifc_0_33_ss_syntax_template_template_parameter    = 69,
  ifc_0_33_ss_syntax_type_template_argument         = 70,
  ifc_0_33_ss_syntax_non_type_template_argument     = 71,
  ifc_0_33_ss_syntax_template_parameter_list        = 72,
  ifc_0_33_ss_syntax_template_argument_list         = 73,
  ifc_0_33_ss_syntax_template_id                    = 74,
  ifc_0_33_ss_syntax_mem_initializer                = 75,
  ifc_0_33_ss_syntax_ctor_initializer               = 76,
  ifc_0_33_ss_syntax_lambda_introducer              = 77,
  ifc_0_33_ss_syntax_lambda_declarator              = 78,
  ifc_0_33_ss_syntax_capture_default                = 79,
  ifc_0_33_ss_syntax_simple_capture                 = 80,
  ifc_0_33_ss_syntax_init_capture                   = 81,
  ifc_0_33_ss_syntax_this_capture                   = 82,
  ifc_0_33_ss_syntax_attributed_statement           = 83,
  ifc_0_33_ss_syntax_attributed_declaration         = 84,
  ifc_0_33_ss_syntax_attribute_specifier_seq        = 85,
  ifc_0_33_ss_syntax_attribute_specifier            = 86,
  ifc_0_33_ss_syntax_attribute_using_prefix         = 87,
  ifc_0_33_ss_syntax_attribute                      = 88,
  ifc_0_33_ss_syntax_attribute_argument_clause      = 89,
  ifc_0_33_ss_syntax_alignas                        = 90,
  ifc_0_33_ss_syntax_using_declaration              = 91,
  ifc_0_33_ss_syntax_using_declarator               = 92,
  ifc_0_33_ss_syntax_using_directive                = 93,
  ifc_0_33_ss_syntax_array_index                    = 94,
  ifc_0_33_ss_syntax_seh_try                        = 95,
  ifc_0_33_ss_syntax_seh_except                     = 96,
  ifc_0_33_ss_syntax_seh_finally                    = 97,
  ifc_0_33_ss_syntax_seh_leave                      = 98,
  ifc_0_33_ss_syntax_type_trait_intrinsic           = 99,
  ifc_0_33_ss_syntax_tuple                          = 100,
  ifc_0_33_ss_syntax_asm_statement                  = 101,
  ifc_0_33_ss_syntax_namespace_alias_definition     = 102,
  ifc_0_33_ss_syntax_super                          = 103,
  ifc_0_33_ss_syntax_unary_fold_expression          = 104,
  ifc_0_33_ss_syntax_binary_fold_expression         = 105,
  ifc_0_33_ss_syntax_empty_statement                = 106,
  ifc_0_33_ss_syntax_structured_binding_declaration = 107,
  ifc_0_33_ss_syntax_structured_binding_identifier  = 108,
  ifc_0_33_ss_syntax_using_enum_declaration         = 109
};  /* an_ifc_syntax_sort_0_33 */


enum an_ifc_syntax_sort {
  ifc_ss_syntax_access_specifier,
  ifc_ss_syntax_alias_declaration,
  ifc_ss_syntax_alignas,
  ifc_ss_syntax_array_declarator,
  ifc_ss_syntax_array_index,
  ifc_ss_syntax_array_or_function_declarator,
  ifc_ss_syntax_asm_statement,
  ifc_ss_syntax_attribute,
  ifc_ss_syntax_attribute_argument_clause,
  ifc_ss_syntax_attribute_specifier,
  ifc_ss_syntax_attribute_specifier_seq,
  ifc_ss_syntax_attribute_using_prefix,
  ifc_ss_syntax_attributed_declaration,
  ifc_ss_syntax_attributed_statement,
  ifc_ss_syntax_base_specifier,
  ifc_ss_syntax_base_specifier_list,
  ifc_ss_syntax_binary_fold_expression,
  ifc_ss_syntax_break_statement,
  ifc_ss_syntax_capture_default,
  ifc_ss_syntax_class_specifier,
  ifc_ss_syntax_compound_requirement,
  ifc_ss_syntax_compound_statement,
  ifc_ss_syntax_concept_definition,
  ifc_ss_syntax_condition_declaration,
  ifc_ss_syntax_continue_statement,
  ifc_ss_syntax_ctor_initializer,
  ifc_ss_syntax_decl_specifier_seq,
  ifc_ss_syntax_declaration_statement,
  ifc_ss_syntax_declarator,
  ifc_ss_syntax_decltype_specifier,
  ifc_ss_syntax_do_while_statement,
  ifc_ss_syntax_dynamic_exception_spec,
  ifc_ss_syntax_empty_statement,
  ifc_ss_syntax_enum_specifier,
  ifc_ss_syntax_enumerator_definition,
  ifc_ss_syntax_exception_declaration,
  ifc_ss_syntax_explicit_specifier,
  ifc_ss_syntax_expression,
  ifc_ss_syntax_expression_statement,
  ifc_ss_syntax_for_range_declaration,
  ifc_ss_syntax_for_statement,
  ifc_ss_syntax_function_body,
  ifc_ss_syntax_function_declarator,
  ifc_ss_syntax_function_definition,
  ifc_ss_syntax_function_try_block,
  ifc_ss_syntax_goto_statement,
  ifc_ss_syntax_handler,
  ifc_ss_syntax_handler_seq,
  ifc_ss_syntax_if_statement,
  ifc_ss_syntax_init_capture,
  ifc_ss_syntax_init_declarator,
  ifc_ss_syntax_init_statement,
  ifc_ss_syntax_labeled_statement,
  ifc_ss_syntax_lambda_declarator,
  ifc_ss_syntax_lambda_introducer,
  ifc_ss_syntax_mem_initializer,
  ifc_ss_syntax_member_declaration,
  ifc_ss_syntax_member_declarator,
  ifc_ss_syntax_member_function_declaration,
  ifc_ss_syntax_member_specification,
  ifc_ss_syntax_namespace_alias_definition,
  ifc_ss_syntax_nested_requirement,
  ifc_ss_syntax_new_declarator,
  ifc_ss_syntax_noexcept_specification,
  ifc_ss_syntax_non_type_template_argument,
  ifc_ss_syntax_parameter_declarator,
  ifc_ss_syntax_placeholder_type_specifier,
  ifc_ss_syntax_pointer_declarator,
  ifc_ss_syntax_range_based_for_statement,
  ifc_ss_syntax_requirement_body,
  ifc_ss_syntax_requires_clause,
  ifc_ss_syntax_return_statement,
  ifc_ss_syntax_seh_except,
  ifc_ss_syntax_seh_finally,
  ifc_ss_syntax_seh_leave,
  ifc_ss_syntax_seh_try,
  ifc_ss_syntax_simple_capture,
  ifc_ss_syntax_simple_declaration,
  ifc_ss_syntax_simple_requirement,
  ifc_ss_syntax_simple_type_specifier,
  ifc_ss_syntax_statement_seq,
  ifc_ss_syntax_static_assert_declaration,
  ifc_ss_syntax_structured_binding_declaration,
  ifc_ss_syntax_structured_binding_identifier,
  ifc_ss_syntax_super,
  ifc_ss_syntax_switch_statement,
  ifc_ss_syntax_template_argument_list,
  ifc_ss_syntax_template_declaration,
  ifc_ss_syntax_template_id,
  ifc_ss_syntax_template_parameter_list,
  ifc_ss_syntax_template_template_parameter,
  ifc_ss_syntax_this_capture,
  ifc_ss_syntax_trailing_return_type,
  ifc_ss_syntax_try_block,
  ifc_ss_syntax_tuple,
  ifc_ss_syntax_type_id,
  ifc_ss_syntax_type_id_list_element,
  ifc_ss_syntax_type_requirement,
  ifc_ss_syntax_type_specifier_seq,
  ifc_ss_syntax_type_template_argument,
  ifc_ss_syntax_type_template_parameter,
  ifc_ss_syntax_type_trait_intrinsic,
  ifc_ss_syntax_unary_fold_expression,
  ifc_ss_syntax_using_declaration,
  ifc_ss_syntax_using_declarator,
  ifc_ss_syntax_using_directive,
  ifc_ss_syntax_using_enum_declaration,
  ifc_ss_syntax_vendor_extension,
  ifc_ss_syntax_virtual_specifier_seq,
  ifc_ss_syntax_while_statement
};  /* an_ifc_syntax_sort */


enum an_ifc_triadic_operator_sort_0_33 : uint16_t {
  ifc_0_33_tos_unknown      = 0,
  ifc_0_33_tos_choice       = 1,
  ifc_0_33_tos_construct_at = 2,
  ifc_0_33_tos_initialize   = 3,
  ifc_0_33_tos_msvc         = 1024
};  /* an_ifc_triadic_operator_sort_0_33 */


enum an_ifc_triadic_operator_sort {
  ifc_tos_choice,
  ifc_tos_construct_at,
  ifc_tos_initialize,
  ifc_tos_msvc,
  ifc_tos_unknown
};  /* an_ifc_triadic_operator_sort */


enum an_ifc_type_basis_sort_0_33 : uint8_t {
  ifc_0_33_tbs_void              = 0,
  ifc_0_33_tbs_bool              = 1,
  ifc_0_33_tbs_char              = 2,
  ifc_0_33_tbs_wchar_t           = 3,
  ifc_0_33_tbs_int               = 4,
  ifc_0_33_tbs_float             = 5,
  ifc_0_33_tbs_double            = 6,
  ifc_0_33_tbs_nullptr           = 7,
  ifc_0_33_tbs_ellipsis          = 8,
  ifc_0_33_tbs_segment_type      = 9,
  ifc_0_33_tbs_class             = 10,
  ifc_0_33_tbs_struct            = 11,
  ifc_0_33_tbs_union             = 12,
  ifc_0_33_tbs_enum              = 13,
  ifc_0_33_tbs_typename          = 14,
  ifc_0_33_tbs_namespace         = 15,
  ifc_0_33_tbs_interface         = 16,
  ifc_0_33_tbs_function          = 17,
  ifc_0_33_tbs_empty             = 18,
  ifc_0_33_tbs_variable_template = 19,
  ifc_0_33_tbs_concept           = 20,
  ifc_0_33_tbs_auto              = 21,
  ifc_0_33_tbs_decltype_auto     = 22,
  ifc_0_33_tbs_overload          = 23
};  /* an_ifc_type_basis_sort_0_33 */


enum an_ifc_type_basis_sort {
  ifc_tbs_auto,
  ifc_tbs_bool,
  ifc_tbs_char,
  ifc_tbs_class,
  ifc_tbs_concept,
  ifc_tbs_decltype_auto,
  ifc_tbs_double,
  ifc_tbs_ellipsis,
  ifc_tbs_empty,
  ifc_tbs_enum,
  ifc_tbs_float,
  ifc_tbs_function,
  ifc_tbs_int,
  ifc_tbs_interface,
  ifc_tbs_namespace,
  ifc_tbs_nullptr,
  ifc_tbs_overload,
  ifc_tbs_segment_type,
  ifc_tbs_struct,
  ifc_tbs_typename,
  ifc_tbs_union,
  ifc_tbs_variable_template,
  ifc_tbs_void,
  ifc_tbs_wchar_t
};  /* an_ifc_type_basis_sort */


enum an_ifc_type_precision_sort_0_33 : uint8_t {
  ifc_0_33_tps_default = 0,
  ifc_0_33_tps_short   = 1,
  ifc_0_33_tps_long    = 2,
  ifc_0_33_tps_bit8    = 3,
  ifc_0_33_tps_bit16   = 4,
  ifc_0_33_tps_bit32   = 5,
  ifc_0_33_tps_bit64   = 6,
  ifc_0_33_tps_bit128  = 7
};  /* an_ifc_type_precision_sort_0_33 */


enum an_ifc_type_precision_sort {
  ifc_tps_bit128,
  ifc_tps_bit16,
  ifc_tps_bit32,
  ifc_tps_bit64,
  ifc_tps_bit8,
  ifc_tps_default,
  ifc_tps_long,
  ifc_tps_short
};  /* an_ifc_type_precision_sort */


enum an_ifc_type_sign_sort_0_33 : uint8_t {
  ifc_0_33_tss_plain    = 0,
  ifc_0_33_tss_signed   = 1,
  ifc_0_33_tss_unsigned = 2
};  /* an_ifc_type_sign_sort_0_33 */


enum an_ifc_type_sign_sort {
  ifc_tss_plain,
  ifc_tss_signed,
  ifc_tss_unsigned
};  /* an_ifc_type_sign_sort */


enum an_ifc_type_sort_0_33 : uint32_t {
  ifc_0_33_ts_type_vendor_extension  = 0,
  ifc_0_33_ts_type_fundamental       = 1,
  ifc_0_33_ts_type_designated        = 2,
  ifc_0_33_ts_type_tor               = 3,
  ifc_0_33_ts_type_syntactic         = 4,
  ifc_0_33_ts_type_expansion         = 5,
  ifc_0_33_ts_type_pointer           = 6,
  ifc_0_33_ts_type_pointer_to_member = 7,
  ifc_0_33_ts_type_lvalue_reference  = 8,
  ifc_0_33_ts_type_rvalue_reference  = 9,
  ifc_0_33_ts_type_function          = 10,
  ifc_0_33_ts_type_method            = 11,
  ifc_0_33_ts_type_array             = 12,
  ifc_0_33_ts_type_typename          = 13,
  ifc_0_33_ts_type_qualified         = 14,
  ifc_0_33_ts_type_base              = 15,
  ifc_0_33_ts_type_decltype          = 16,
  ifc_0_33_ts_type_placeholder       = 17,
  ifc_0_33_ts_type_tuple             = 18,
  ifc_0_33_ts_type_forall            = 19,
  ifc_0_33_ts_type_unaligned         = 20,
  ifc_0_33_ts_type_syntax_tree       = 21
};  /* an_ifc_type_sort_0_33 */


enum an_ifc_type_sort {
  ifc_ts_type_array,
  ifc_ts_type_base,
  ifc_ts_type_decltype,
  ifc_ts_type_designated,
  ifc_ts_type_expansion,
  ifc_ts_type_forall,
  ifc_ts_type_function,
  ifc_ts_type_fundamental,
  ifc_ts_type_lvalue_reference,
  ifc_ts_type_method,
  ifc_ts_type_placeholder,
  ifc_ts_type_pointer,
  ifc_ts_type_pointer_to_member,
  ifc_ts_type_qualified,
  ifc_ts_type_rvalue_reference,
  ifc_ts_type_syntactic,
  ifc_ts_type_syntax_tree,
  ifc_ts_type_tor,
  ifc_ts_type_tuple,
  ifc_ts_type_typename,
  ifc_ts_type_unaligned,
  ifc_ts_type_vendor_extension
};  /* an_ifc_type_sort */


enum an_ifc_unit_sort_0_33 : uint32_t {
  ifc_0_33_us_source      = 0,
  ifc_0_33_us_primary     = 1,
  ifc_0_33_us_partition   = 2,
  ifc_0_33_us_header      = 3,
  ifc_0_33_us_exported_tu = 4
};  /* an_ifc_unit_sort_0_33 */


enum an_ifc_unit_sort {
  ifc_us_exported_tu,
  ifc_us_header,
  ifc_us_partition,
  ifc_us_primary,
  ifc_us_source
};  /* an_ifc_unit_sort */


enum an_ifc_variadic_operator_sort_0_33 : uint16_t {
  ifc_0_33_vos_unknown                         = 0,
  ifc_0_33_vos_collection                      = 1,
  ifc_0_33_vos_sequence                        = 2,
  ifc_0_33_vos_msvc                            = 1024,
  ifc_0_33_vos_msvc_has_trivial_constructor    = 1025,
  ifc_0_33_vos_msvc_is_constructible           = 1026,
  ifc_0_33_vos_msvc_is_nothrow_constructible   = 1027,
  ifc_0_33_vos_msvc_is_trivially_constructible = 1028
};  /* an_ifc_variadic_operator_sort_0_33 */


enum an_ifc_variadic_operator_sort {
  ifc_vos_collection,
  ifc_vos_msvc,
  ifc_vos_msvc_has_trivial_constructor,
  ifc_vos_msvc_is_constructible,
  ifc_vos_msvc_is_nothrow_constructible,
  ifc_vos_msvc_is_trivially_constructible,
  ifc_vos_sequence,
  ifc_vos_unknown
};  /* an_ifc_variadic_operator_sort */


enum an_ifc_word_sort_0_33 : uint8_t {
  ifc_0_33_ws_unknown           = 0,
  ifc_0_33_ws_source_directive  = 1,
  ifc_0_33_ws_source_punctuator = 2,
  ifc_0_33_ws_source_literal    = 3,
  ifc_0_33_ws_source_operator   = 4,
  ifc_0_33_ws_source_keyword    = 5,
  ifc_0_33_ws_source_identifier = 6
};  /* an_ifc_word_sort_0_33 */


enum an_ifc_word_sort {
  ifc_ws_source_directive,
  ifc_ws_source_identifier,
  ifc_ws_source_keyword,
  ifc_ws_source_literal,
  ifc_ws_source_operator,
  ifc_ws_source_punctuator,
  ifc_ws_unknown
};  /* an_ifc_word_sort */


enum an_ifc_attr_index_0_33 : uint32_t {};


/*
The universal representation for an IFC AttrIndex.
*/
struct an_ifc_attr_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_attr_sort
                sort;
                        /* The associated AttrSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_attr_index */


inline a_boolean operator==(const an_ifc_attr_index &lhs,
                            const an_ifc_attr_index &rhs)
/*
Compare two instances of this AttrIndex (lhs and rhs).  If the two instances
are equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_attr_index &lhs,
                            const an_ifc_attr_index &rhs)
/*
Compare two instances of this AttrIndex (lhs and rhs).  If the two instances
are equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


enum an_ifc_chart_index_0_33 : uint32_t {};


/*
The universal representation for an IFC ChartIndex.
*/
struct an_ifc_chart_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_chart_sort
                sort;
                        /* The associated ChartSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_chart_index */


inline a_boolean operator==(const an_ifc_chart_index &lhs,
                            const an_ifc_chart_index &rhs)
/*
Compare two instances of this ChartIndex (lhs and rhs).  If the two instances
are equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_chart_index &lhs,
                            const an_ifc_chart_index &rhs)
/*
Compare two instances of this ChartIndex (lhs and rhs).  If the two instances
are equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


enum an_ifc_decl_index_0_33 : uint32_t {};
enum an_ifc_decl_index_0_41 : uint32_t {};


/*
The universal representation for an IFC DeclIndex.
*/
struct an_ifc_decl_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_decl_sort
                sort;
                        /* The associated DeclSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_decl_index */


inline a_boolean operator==(const an_ifc_decl_index &lhs,
                            const an_ifc_decl_index &rhs)
/*
Compare two instances of this DeclIndex (lhs and rhs).  If the two instances
are equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_decl_index &lhs,
                            const an_ifc_decl_index &rhs)
/*
Compare two instances of this DeclIndex (lhs and rhs).  If the two instances
are equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


enum an_ifc_expr_index_0_33 : uint32_t {};


/*
The universal representation for an IFC ExprIndex.
*/
struct an_ifc_expr_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_expr_sort
                sort;
                        /* The associated ExprSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_expr_index */


inline a_boolean operator==(const an_ifc_expr_index &lhs,
                            const an_ifc_expr_index &rhs)
/*
Compare two instances of this ExprIndex (lhs and rhs).  If the two instances
are equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_expr_index &lhs,
                            const an_ifc_expr_index &rhs)
/*
Compare two instances of this ExprIndex (lhs and rhs).  If the two instances
are equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


enum an_ifc_form_index_0_33 : uint32_t {};


/*
The universal representation for an IFC FormIndex.
*/
struct an_ifc_form_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_form_sort
                sort;
                        /* The associated FormSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_form_index */


inline a_boolean operator==(const an_ifc_form_index &lhs,
                            const an_ifc_form_index &rhs)
/*
Compare two instances of this FormIndex (lhs and rhs).  If the two instances
are equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_form_index &lhs,
                            const an_ifc_form_index &rhs)
/*
Compare two instances of this FormIndex (lhs and rhs).  If the two instances
are equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


enum an_ifc_lit_index_0_33 : uint32_t {};


/*
The universal representation for an IFC LitIndex.
*/
struct an_ifc_lit_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_lit_sort
                sort;
                        /* The associated LitSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_lit_index */


inline a_boolean operator==(const an_ifc_lit_index &lhs,
                            const an_ifc_lit_index &rhs)
/*
Compare two instances of this LitIndex (lhs and rhs).  If the two instances are
equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_lit_index &lhs,
                            const an_ifc_lit_index &rhs)
/*
Compare two instances of this LitIndex (lhs and rhs).  If the two instances are
equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


enum an_ifc_macro_index_0_33 : uint32_t {};


/*
The universal representation for an IFC MacroIndex.
*/
struct an_ifc_macro_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_macro_sort
                sort;
                        /* The associated MacroSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_macro_index */


inline a_boolean operator==(const an_ifc_macro_index &lhs,
                            const an_ifc_macro_index &rhs)
/*
Compare two instances of this MacroIndex (lhs and rhs).  If the two instances
are equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_macro_index &lhs,
                            const an_ifc_macro_index &rhs)
/*
Compare two instances of this MacroIndex (lhs and rhs).  If the two instances
are equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


enum an_ifc_name_index_0_33 : uint32_t {};


/*
The universal representation for an IFC NameIndex.
*/
struct an_ifc_name_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_name_sort
                sort;
                        /* The associated NameSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_name_index */


inline a_boolean operator==(const an_ifc_name_index &lhs,
                            const an_ifc_name_index &rhs)
/*
Compare two instances of this NameIndex (lhs and rhs).  If the two instances
are equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_name_index &lhs,
                            const an_ifc_name_index &rhs)
/*
Compare two instances of this NameIndex (lhs and rhs).  If the two instances
are equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


enum an_ifc_pragma_index_0_33 : uint32_t {};


/*
The universal representation for an IFC PragmaIndex.
*/
struct an_ifc_pragma_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_pragma_sort
                sort;
                        /* The associated PragmaSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_pragma_index */


inline a_boolean operator==(const an_ifc_pragma_index &lhs,
                            const an_ifc_pragma_index &rhs)
/*
Compare two instances of this PragmaIndex (lhs and rhs).  If the two instances
are equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_pragma_index &lhs,
                            const an_ifc_pragma_index &rhs)
/*
Compare two instances of this PragmaIndex (lhs and rhs).  If the two instances
are equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


enum an_ifc_stmt_index_0_33 : uint32_t {};


/*
The universal representation for an IFC StmtIndex.
*/
struct an_ifc_stmt_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_stmt_sort
                sort;
                        /* The associated StmtSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_stmt_index */


inline a_boolean operator==(const an_ifc_stmt_index &lhs,
                            const an_ifc_stmt_index &rhs)
/*
Compare two instances of this StmtIndex (lhs and rhs).  If the two instances
are equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_stmt_index &lhs,
                            const an_ifc_stmt_index &rhs)
/*
Compare two instances of this StmtIndex (lhs and rhs).  If the two instances
are equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


enum an_ifc_string_index_0_33 : uint32_t {};


/*
The universal representation for an IFC StringIndex.
*/
struct an_ifc_string_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_string_sort
                sort;
                        /* The associated StringSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_string_index */


inline a_boolean operator==(const an_ifc_string_index &lhs,
                            const an_ifc_string_index &rhs)
/*
Compare two instances of this StringIndex (lhs and rhs).  If the two instances
are equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_string_index &lhs,
                            const an_ifc_string_index &rhs)
/*
Compare two instances of this StringIndex (lhs and rhs).  If the two instances
are equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


enum an_ifc_syntax_index_0_33 : uint32_t {};


/*
The universal representation for an IFC SyntaxIndex.
*/
struct an_ifc_syntax_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_syntax_sort
                sort;
                        /* The associated SyntaxSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_syntax_index */


inline a_boolean operator==(const an_ifc_syntax_index &lhs,
                            const an_ifc_syntax_index &rhs)
/*
Compare two instances of this SyntaxIndex (lhs and rhs).  If the two instances
are equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_syntax_index &lhs,
                            const an_ifc_syntax_index &rhs)
/*
Compare two instances of this SyntaxIndex (lhs and rhs).  If the two instances
are equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


enum an_ifc_type_index_0_33 : uint32_t {};


/*
The universal representation for an IFC TypeIndex.
*/
struct an_ifc_type_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_type_sort
                sort;
                        /* The associated TypeSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_type_index */


inline a_boolean operator==(const an_ifc_type_index &lhs,
                            const an_ifc_type_index &rhs)
/*
Compare two instances of this TypeIndex (lhs and rhs).  If the two instances
are equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_type_index &lhs,
                            const an_ifc_type_index &rhs)
/*
Compare two instances of this TypeIndex (lhs and rhs).  If the two instances
are equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


enum an_ifc_unit_index_0_33 : uint32_t {};


/*
The universal representation for an IFC UnitIndex.
*/
struct an_ifc_unit_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_unit_sort
                sort;
                        /* The associated UnitSort value for this index. */
  uint32_t      value;
                        /* The index value into the associated partition of
                           "sort" for this index.  Represented as the largest
                           common underlying type. */
};  /* an_ifc_unit_index */


inline a_boolean operator==(const an_ifc_unit_index &lhs,
                            const an_ifc_unit_index &rhs)
/*
Compare two instances of this UnitIndex (lhs and rhs).  If the two instances
are equivalent, return TRUE; otherwise return FALSE.
*/
{
  a_boolean result = TRUE;

  /* Check members that are more likely to be unique first.  This allows the
     logic to short circuit in common negative cases. */
  if (lhs.value != rhs.value) {
    result = FALSE;
  } else if (lhs.sort != rhs.sort) {
    result = FALSE;
  } else if (lhs.mod != rhs.mod) {
    result = FALSE;
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const an_ifc_unit_index &lhs,
                            const an_ifc_unit_index &rhs)
/*
Compare two instances of this UnitIndex (lhs and rhs).  If the two instances
are equivalent, return FALSE; otherwise return TRUE.
*/
{
  return !(lhs == rhs);
}  /* operator!= */


/*
A type representing the largest possible index size for any index.
*/
using an_ifc_index_type = uint32_t;

enum an_ifc_decl_foreign_index_0_33 : uint32_t;
using an_ifc_decl_foreign_index_storage = uint32_t;


/*
The universal representation for an IFC DeclForeignIndex.
*/
struct an_ifc_decl_foreign_index {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_decl_foreign_index_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */
};  /* an_ifc_decl_foreign_index */


enum an_ifc_basic_specifiers_bitfield_0_33 : uint8_t {
  ifc_0_33_bsb_cxx                        = 0,
  ifc_0_33_bsb_c                          = 1 << 0,
  ifc_0_33_bsb_internal                   = 1 << 1,
  ifc_0_33_bsb_vague                      = 1 << 2,
  ifc_0_33_bsb_external                   = 1 << 3,
  ifc_0_33_bsb_deprecated                 = 1 << 4,
  ifc_0_33_bsb_initialized_in_class       = 1 << 5,
  ifc_0_33_bsb_non_exported               = 1 << 6,
  ifc_0_33_bsb_is_member_of_global_module = 1 << 7
};  /* an_ifc_basic_specifiers_bitfield_0_33 */


using an_ifc_basic_specifiers_bitfield_storage = uint8_t;


/*
The universal representation for an IFC BasicSpecifiersBitfield.
*/
struct an_ifc_basic_specifiers_bitfield {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_basic_specifiers_bitfield_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */
};  /* an_ifc_basic_specifiers_bitfield */


enum an_ifc_basic_specifiers_bitfield_query {
  ifc_bsb_c                          = 1 << 0,
  ifc_bsb_cxx                        = 1 << 1,
  ifc_bsb_deprecated                 = 1 << 2,
  ifc_bsb_external                   = 1 << 3,
  ifc_bsb_initialized_in_class       = 1 << 4,
  ifc_bsb_internal                   = 1 << 5,
  ifc_bsb_is_member_of_global_module = 1 << 6,
  ifc_bsb_non_exported               = 1 << 7,
  ifc_bsb_vague                      = 1 << 8
};  /* an_ifc_basic_specifiers_bitfield_query */


enum an_ifc_function_traits_bitfield_0_33 : uint16_t {
  ifc_0_33_ftb_none          = 0,
  ifc_0_33_ftb_inline        = 1 << 0,
  ifc_0_33_ftb_constexpr     = 1 << 1,
  ifc_0_33_ftb_explicit      = 1 << 2,
  ifc_0_33_ftb_virtual       = 1 << 3,
  ifc_0_33_ftb_no_return     = 1 << 4,
  ifc_0_33_ftb_pure_virtual  = 1 << 5,
  ifc_0_33_ftb_hidden_friend = 1 << 6,
  ifc_0_33_ftb_defaulted     = 1 << 7,
  ifc_0_33_ftb_deleted       = 1 << 8,
  ifc_0_33_ftb_constrained   = 1 << 9,
  ifc_0_33_ftb_immediate     = 1 << 10
};  /* an_ifc_function_traits_bitfield_0_33 */


using an_ifc_function_traits_bitfield_storage = uint16_t;


/*
The universal representation for an IFC FunctionTraitsBitfield.
*/
struct an_ifc_function_traits_bitfield {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_function_traits_bitfield_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */
};  /* an_ifc_function_traits_bitfield */


enum an_ifc_function_traits_bitfield_query {
  ifc_ftb_constexpr     = 1 << 0,
  ifc_ftb_constrained   = 1 << 1,
  ifc_ftb_defaulted     = 1 << 2,
  ifc_ftb_deleted       = 1 << 3,
  ifc_ftb_explicit      = 1 << 4,
  ifc_ftb_hidden_friend = 1 << 5,
  ifc_ftb_immediate     = 1 << 6,
  ifc_ftb_inline        = 1 << 7,
  ifc_ftb_no_return     = 1 << 8,
  ifc_ftb_none          = 1 << 9,
  ifc_ftb_pure_virtual  = 1 << 10,
  ifc_ftb_virtual       = 1 << 11
};  /* an_ifc_function_traits_bitfield_query */


enum an_ifc_function_type_traits_bitfield_0_33 : uint8_t {
  ifc_0_33_fttb_none     = 0,
  ifc_0_33_fttb_const    = 1 << 0,
  ifc_0_33_fttb_volatile = 1 << 1,
  ifc_0_33_fttb_lvalue   = 1 << 2,
  ifc_0_33_fttb_rvalue   = 1 << 3
};  /* an_ifc_function_type_traits_bitfield_0_33 */


using an_ifc_function_type_traits_bitfield_storage = uint8_t;


/*
The universal representation for an IFC FunctionTypeTraitsBitfield.
*/
struct an_ifc_function_type_traits_bitfield {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_function_type_traits_bitfield_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */
};  /* an_ifc_function_type_traits_bitfield */


enum an_ifc_function_type_traits_bitfield_query {
  ifc_fttb_const    = 1 << 0,
  ifc_fttb_lvalue   = 1 << 1,
  ifc_fttb_none     = 1 << 2,
  ifc_fttb_rvalue   = 1 << 3,
  ifc_fttb_volatile = 1 << 4
};  /* an_ifc_function_type_traits_bitfield_query */


enum an_ifc_msvc_traits_bitfield_0_33 : uint32_t {
  ifc_0_33_mtb_none           = 0,
  ifc_0_33_mtb_force_inline   = 1 << 0,
  ifc_0_33_mtb_naked          = 1 << 1,
  ifc_0_33_mtb_no_alias       = 1 << 2,
  ifc_0_33_mtb_no_inline      = 1 << 3,
  ifc_0_33_mtb_restrict       = 1 << 4,
  ifc_0_33_mtb_safe_buffers   = 1 << 5,
  ifc_0_33_mtb_dll_export     = 1 << 6,
  ifc_0_33_mtb_dll_import     = 1 << 7,
  ifc_0_33_mtb_code_segment   = 1 << 8,
  ifc_0_33_mtb_novtable       = 1 << 9,
  ifc_0_33_mtb_intrinsic_type = 1 << 10,
  ifc_0_33_mtb_empty_bases    = 1 << 11,
  ifc_0_33_mtb_process        = 1 << 12,
  ifc_0_33_mtb_allocate       = 1 << 13,
  ifc_0_33_mtb_select_any     = 1 << 14,
  ifc_0_33_mtb_comdat         = 1 << 15,
  ifc_0_33_mtb_uuid           = 1 << 16
};  /* an_ifc_msvc_traits_bitfield_0_33 */


using an_ifc_msvc_traits_bitfield_storage = uint32_t;


/*
The universal representation for an IFC MsvcTraitsBitfield.
*/
struct an_ifc_msvc_traits_bitfield {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_msvc_traits_bitfield_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */
};  /* an_ifc_msvc_traits_bitfield */


enum an_ifc_msvc_traits_bitfield_query {
  ifc_mtb_allocate       = 1 << 0,
  ifc_mtb_code_segment   = 1 << 1,
  ifc_mtb_comdat         = 1 << 2,
  ifc_mtb_dll_export     = 1 << 3,
  ifc_mtb_dll_import     = 1 << 4,
  ifc_mtb_empty_bases    = 1 << 5,
  ifc_mtb_force_inline   = 1 << 6,
  ifc_mtb_intrinsic_type = 1 << 7,
  ifc_mtb_naked          = 1 << 8,
  ifc_mtb_no_alias       = 1 << 9,
  ifc_mtb_no_inline      = 1 << 10,
  ifc_mtb_none           = 1 << 11,
  ifc_mtb_novtable       = 1 << 12,
  ifc_mtb_process        = 1 << 13,
  ifc_mtb_restrict       = 1 << 14,
  ifc_mtb_safe_buffers   = 1 << 15,
  ifc_mtb_select_any     = 1 << 16,
  ifc_mtb_uuid           = 1 << 17
};  /* an_ifc_msvc_traits_bitfield_query */


enum an_ifc_object_traits_bitfield_0_33 : uint8_t {
  ifc_0_33_otb_none                 = 0,
  ifc_0_33_otb_constexpr            = 1 << 0,
  ifc_0_33_otb_mutable              = 1 << 1,
  ifc_0_33_otb_thread_local         = 1 << 2,
  ifc_0_33_otb_inline               = 1 << 3,
  ifc_0_33_otb_initializer_exported = 1 << 4,
  ifc_0_33_otb_vendor               = 1 << 7
};  /* an_ifc_object_traits_bitfield_0_33 */


using an_ifc_object_traits_bitfield_storage = uint8_t;


/*
The universal representation for an IFC ObjectTraitsBitfield.
*/
struct an_ifc_object_traits_bitfield {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_object_traits_bitfield_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */
};  /* an_ifc_object_traits_bitfield */


enum an_ifc_object_traits_bitfield_query {
  ifc_otb_constexpr            = 1 << 0,
  ifc_otb_initializer_exported = 1 << 1,
  ifc_otb_inline               = 1 << 2,
  ifc_otb_mutable              = 1 << 3,
  ifc_otb_none                 = 1 << 4,
  ifc_otb_thread_local         = 1 << 5,
  ifc_otb_vendor               = 1 << 6
};  /* an_ifc_object_traits_bitfield_query */


enum an_ifc_qualifier_bitfield_0_33 : uint8_t {
  ifc_0_33_qb_none     = 0,
  ifc_0_33_qb_const    = 1 << 0,
  ifc_0_33_qb_volatile = 1 << 1,
  ifc_0_33_qb_restrict = 1 << 2
};  /* an_ifc_qualifier_bitfield_0_33 */


using an_ifc_qualifier_bitfield_storage = uint8_t;


/*
The universal representation for an IFC QualifierBitfield.
*/
struct an_ifc_qualifier_bitfield {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_qualifier_bitfield_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */
};  /* an_ifc_qualifier_bitfield */


enum an_ifc_qualifier_bitfield_query {
  ifc_qb_const    = 1 << 0,
  ifc_qb_none     = 1 << 1,
  ifc_qb_restrict = 1 << 2,
  ifc_qb_volatile = 1 << 3
};  /* an_ifc_qualifier_bitfield_query */


enum an_ifc_reachable_properties_bitfield_0_33 : uint8_t {
  ifc_0_33_rpb_all               = 0xff,
  ifc_0_33_rpb_none              = 0,
  ifc_0_33_rpb_initializer       = 1 << 0,
  ifc_0_33_rpb_default_arguments = 1 << 1,
  ifc_0_33_rpb_attributes        = 1 << 2
};  /* an_ifc_reachable_properties_bitfield_0_33 */


using an_ifc_reachable_properties_bitfield_storage = uint8_t;


/*
The universal representation for an IFC ReachablePropertiesBitfield.
*/
struct an_ifc_reachable_properties_bitfield {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_reachable_properties_bitfield_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */
};  /* an_ifc_reachable_properties_bitfield */


enum an_ifc_reachable_properties_bitfield_query {
  ifc_rpb_all               = 1 << 0,
  ifc_rpb_attributes        = 1 << 1,
  ifc_rpb_default_arguments = 1 << 2,
  ifc_rpb_initializer       = 1 << 3,
  ifc_rpb_none              = 1 << 4
};  /* an_ifc_reachable_properties_bitfield_query */


enum an_ifc_scope_traits_bitfield_0_33 : uint8_t {
  ifc_0_33_stb_none                 = 0,
  ifc_0_33_stb_unnamed              = 1 << 0,
  ifc_0_33_stb_inline               = 1 << 1,
  ifc_0_33_stb_initializer_exported = 1 << 2,
  ifc_0_33_stb_closure_type         = 1 << 3,
  ifc_0_33_stb_final                = 1 << 4,
  ifc_0_33_stb_vendor               = 1 << 7
};  /* an_ifc_scope_traits_bitfield_0_33 */


using an_ifc_scope_traits_bitfield_storage = uint8_t;


/*
The universal representation for an IFC ScopeTraitsBitfield.
*/
struct an_ifc_scope_traits_bitfield {
  an_ifc_module*
                mod;
                        /* The associated module. */
  an_ifc_scope_traits_bitfield_storage
                value;
                        /* The raw bit value obtained from the module.
                           Represented as the largest common underlying
                           type. */
};  /* an_ifc_scope_traits_bitfield */


enum an_ifc_scope_traits_bitfield_query {
  ifc_stb_closure_type         = 1 << 0,
  ifc_stb_final                = 1 << 1,
  ifc_stb_initializer_exported = 1 << 2,
  ifc_stb_inline               = 1 << 3,
  ifc_stb_none                 = 1 << 4,
  ifc_stb_unnamed              = 1 << 5,
  ifc_stb_vendor               = 1 << 6
};  /* an_ifc_scope_traits_bitfield_query */

enum an_ifc_operator_category_0_33 : uint16_t {};


/*
The universal representation for an IFC OperatorCategory.
*/
struct an_ifc_operator_category {
  an_ifc_operator_sort
                sort;
                        /* The associated OperatorSort value for this index,
                           determining which union field is active. */
  union {
    /* When sort == ifc_os_dyadic_operator: */
    an_ifc_dyadic_operator_sort
                dyadic_operator;
                        /* The represented universal value when this category
                           sort represents a DyadicOperatorSort value. */
    /* When sort == ifc_os_monadic_operator: */
    an_ifc_monadic_operator_sort
                monadic_operator;
                        /* The represented universal value when this category
                           sort represents a MonadicOperatorSort value. */
    /* When sort == ifc_os_niladic_operator: */
    an_ifc_niladic_operator_sort
                niladic_operator;
                        /* The represented universal value when this category
                           sort represents a NiladicOperatorSort value. */
    /* When sort == ifc_os_storage_instruction_operator: */
    an_ifc_storage_instruction_operator_sort
                storage_instruction_operator;
                        /* The represented universal value when this category
                           sort represents a StorageInstructionOperatorSort
                           value. */
    /* When sort == ifc_os_triadic_operator: */
    an_ifc_triadic_operator_sort
                triadic_operator;
                        /* The represented universal value when this category
                           sort represents a TriadicOperatorSort value. */
    /* When sort == ifc_os_variadic_operator: */
    an_ifc_variadic_operator_sort
                variadic_operator;
                        /* The represented universal value when this category
                           sort represents a VariadicOperatorSort value. */
  } variant;
};  /* an_ifc_operator_category */

enum an_ifc_source_identifier_category_0_33 : uint64_t {};


/*
The universal representation for an IFC SourceIdentifierCategory.
*/
struct an_ifc_source_identifier_category {
  an_ifc_source_identifier_sort
                sort;
                        /* The associated SourceIdentifierSort value for this
                           index, determining which union field is active. */
  union {
    /* When sort == ifc_sis_msvc: */
    an_ifc_source_unknown_identifier
                msvc;
                        /* The represented universal value when this category
                           sort represents a SourceUnknownIdentifier value. */
    /* When sort == ifc_sis_msvc_builtin_huge_val: */
    an_ifc_source_unknown_identifier
                msvc_builtin_huge_val;
                        /* The represented universal value when this category
                           sort represents a SourceUnknownIdentifier value. */
    /* When sort == ifc_sis_msvc_builtin_huge_valf: */
    an_ifc_source_unknown_identifier
                msvc_builtin_huge_valf;
                        /* The represented universal value when this category
                           sort represents a SourceUnknownIdentifier value. */
    /* When sort == ifc_sis_msvc_builtin_nan: */
    an_ifc_source_unknown_identifier
                msvc_builtin_nan;
                        /* The represented universal value when this category
                           sort represents a SourceUnknownIdentifier value. */
    /* When sort == ifc_sis_msvc_builtin_nanf: */
    an_ifc_source_unknown_identifier
                msvc_builtin_nanf;
                        /* The represented universal value when this category
                           sort represents a SourceUnknownIdentifier value. */
    /* When sort == ifc_sis_msvc_builtin_nans: */
    an_ifc_source_unknown_identifier
                msvc_builtin_nans;
                        /* The represented universal value when this category
                           sort represents a SourceUnknownIdentifier value. */
    /* When sort == ifc_sis_msvc_builtin_nansf: */
    an_ifc_source_unknown_identifier
                msvc_builtin_nansf;
                        /* The represented universal value when this category
                           sort represents a SourceUnknownIdentifier value. */
    /* When sort == ifc_sis_plain: */
    an_ifc_text_offset
                plain;
                        /* The represented universal value when this category
                           sort represents a TextOffset value. */
  } variant;
};  /* an_ifc_source_identifier_category */

enum an_ifc_source_literal_category_0_33 : uint64_t {};


/*
The universal representation for an IFC SourceLiteralCategory.
*/
struct an_ifc_source_literal_category {
  an_ifc_source_literal_sort
                sort;
                        /* The associated SourceLiteralSort value for this
                           index, determining which union field is active. */
  union {
    /* When sort == ifc_sls_defined_string: */
    an_ifc_string_index
                defined_string;
                        /* The represented universal value when this category
                           sort represents a StringIndex value. */
    /* When sort == ifc_sls_msvc: */
    an_ifc_source_unknown_literal
                msvc;
                        /* The represented universal value when this category
                           sort represents a SourceUnknownLiteral value. */
    /* When sort == ifc_sls_msvc_binding: */
    an_ifc_expr_index
                msvc_binding;
                        /* The represented universal value when this category
                           sort represents a ExprIndex value. */
    /* When sort == ifc_sls_msvc_cast_target_type: */
    an_ifc_source_unknown_literal
                msvc_cast_target_type;
                        /* The represented universal value when this category
                           sort represents a SourceUnknownLiteral value. */
    /* When sort == ifc_sls_msvc_defined_constant: */
    an_ifc_source_unknown_literal
                msvc_defined_constant;
                        /* The represented universal value when this category
                           sort represents a SourceUnknownLiteral value. */
    /* When sort == ifc_sls_msvc_function_name_macro: */
    an_ifc_text_offset
                msvc_function_name_macro;
                        /* The represented universal value when this category
                           sort represents a TextOffset value. */
    /* When sort == ifc_sls_msvc_resolved_type: */
    an_ifc_source_unknown_literal
                msvc_resolved_type;
                        /* The represented universal value when this category
                           sort represents a SourceUnknownLiteral value. */
    /* When sort == ifc_sls_msvc_string_prefix_macro: */
    an_ifc_text_offset
                msvc_string_prefix_macro;
                        /* The represented universal value when this category
                           sort represents a TextOffset value. */
    /* When sort == ifc_sls_scalar: */
    an_ifc_expr_index
                scalar;
                        /* The represented universal value when this category
                           sort represents a ExprIndex value. */
    /* When sort == ifc_sls_string: */
    an_ifc_string_index
                string;
                        /* The represented universal value when this category
                           sort represents a StringIndex value. */
    /* When sort == ifc_sls_unknown: */
    an_ifc_source_unknown_literal
                unknown;
                        /* The represented universal value when this category
                           sort represents a SourceUnknownLiteral value. */
  } variant;
};  /* an_ifc_source_literal_category */

enum an_ifc_word_category_0_33 : uint64_t {};


/*
The universal representation for an IFC WordCategory.
*/
struct an_ifc_word_category {
  an_ifc_word_sort
                sort;
                        /* The associated WordSort value for this index,
                           determining which union field is active. */
  union {
    /* When sort == ifc_ws_source_directive: */
    an_ifc_source_directive_sort
                source_directive;
                        /* The represented universal value when this category
                           sort represents a SourceDirectiveSort value. */
    /* When sort == ifc_ws_source_identifier: */
    an_ifc_source_identifier_category
                source_identifier;
                        /* The represented universal value when this category
                           sort represents a SourceIdentifierCategory value. */
    /* When sort == ifc_ws_source_keyword: */
    an_ifc_source_keyword_sort
                source_keyword;
                        /* The represented universal value when this category
                           sort represents a SourceKeywordSort value. */
    /* When sort == ifc_ws_source_literal: */
    an_ifc_source_literal_category
                source_literal;
                        /* The represented universal value when this category
                           sort represents a SourceLiteralCategory value. */
    /* When sort == ifc_ws_source_operator: */
    an_ifc_source_operator_sort
                source_operator;
                        /* The represented universal value when this category
                           sort represents a SourceOperatorSort value. */
    /* When sort == ifc_ws_source_punctuator: */
    an_ifc_source_punctuator_sort
                source_punctuator;
                        /* The represented universal value when this category
                           sort represents a SourcePunctuatorSort value. */
    /* When sort == ifc_ws_unknown: */
    an_ifc_source_unknown_word
                unknown;
                        /* The represented universal value when this category
                           sort represents a SourceUnknownWord value. */
  } variant;
};  /* an_ifc_word_category */


/*
  |-----------------------------------------|
  |     KeywordSyntax - 0.33 (12 bytes)     |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | value | KeywordSort    | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_keyword_syntax_part : uint8_t {};
using an_ifc_keyword_syntax_storage = an_ifc_keyword_syntax_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_keyword_syntax_bytes = const an_ifc_keyword_syntax_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_keyword_syntax_bytes = an_ifc_keyword_syntax_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_keyword_syntax =
                             an_ifc_Byte_buffer<an_ifc_keyword_syntax_storage>;


/*
  |-----------------------------------------|
  |    ModuleReference - 0.33 (8 bytes)     |
  |-----------|------------|---------|------|
  | Name      | Type       | Version | Size |
  |-----------|------------|---------|------|
  | owner     | TextOffset | 0.33    | 4    |
  | partition | TextOffset | 0.33    | 4    |
  |-----------|------------|---------|------|
*/
enum an_ifc_module_reference_part : uint8_t {};
using an_ifc_module_reference_storage = an_ifc_module_reference_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_module_reference_bytes = const an_ifc_module_reference_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_module_reference_bytes = an_ifc_module_reference_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_module_reference =
                           an_ifc_Byte_buffer<an_ifc_module_reference_storage>;


/*
  |-----------------------------------------------|
  |        NestableWord - 0.33 (16 bytes)         |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | locus       | SourceLocation | 0.33    | 8    |
  | index       | Index          | 0.33    | 4    |
  | value       | u16            | 0.33    | 2    |
  | sort        | WordSort       | 0.33    | 1    |
  | __padding__ | uint8_t[1]     |         | 1    |
  |-------------|----------------|---------|------|
  | category    | WordCategory   | 0.33    | RF   |
  |-------------|----------------|---------|------|
*/
enum an_ifc_nestable_word_part : uint8_t {};
using an_ifc_nestable_word_storage = an_ifc_nestable_word_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_nestable_word_bytes = const an_ifc_nestable_word_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_nestable_word_bytes = an_ifc_nestable_word_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_nestable_word = an_ifc_Byte_buffer<an_ifc_nestable_word_storage>;


/*
  |----------------------------------------------|
  |    NoexceptSpecification - 0.33 (8 bytes)    |
  |-------------|---------------|---------|------|
  | Name        | Type          | Version | Size |
  |-------------|---------------|---------|------|
  | words       | SentenceIndex | 0.33    | 4    |
  | sort        | NoexceptSort  | 0.33    | 1    |
  | __padding__ | uint8_t[3]    |         | 3    |
  |-------------|---------------|---------|------|
*/
enum an_ifc_noexcept_specification_part : uint8_t {};
using an_ifc_noexcept_specification_storage =
                                         an_ifc_noexcept_specification_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_noexcept_specification_bytes =
                                  const an_ifc_noexcept_specification_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_noexcept_specification_bytes =
                                         an_ifc_noexcept_specification_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_noexcept_specification =
                     an_ifc_Byte_buffer<an_ifc_noexcept_specification_storage>;


/*
  |---------------------------------------------|
  |    ParameterizedEntity - 0.33 (16 bytes)    |
  |------------|---------------|---------|------|
  | Name       | Type          | Version | Size |
  |------------|---------------|---------|------|
  | decl       | DeclIndex     | 0.33    | 4    |
  | head       | SentenceIndex | 0.33    | 4    |
  | body       | SentenceIndex | 0.33    | 4    |
  | attributes | SentenceIndex | 0.33    | 4    |
  |------------|---------------|---------|------|

  |---------------------------------------------|
  |    ParameterizedEntity - 0.41 (16 bytes)    |
  |------------|---------------|---------|------|
  | Name       | Type          | Version | Size |
  |------------|---------------|---------|------|
  | decl       | DeclIndex     | 0.41    | 4    |
  | head       | SentenceIndex | 0.33    | 4    |
  | body       | SentenceIndex | 0.33    | 4    |
  | attributes | SentenceIndex | 0.33    | 4    |
  |------------|---------------|---------|------|
*/
enum an_ifc_parameterized_entity_part : uint8_t {};
using an_ifc_parameterized_entity_storage =
                                          an_ifc_parameterized_entity_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_parameterized_entity_bytes =
                                    const an_ifc_parameterized_entity_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_parameterized_entity_bytes = an_ifc_parameterized_entity_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_parameterized_entity =
                       an_ifc_Byte_buffer<an_ifc_parameterized_entity_storage>;


/*
  |--------------------------------------------|
  |         Sequence - 0.33 (8 bytes)          |
  |-------------|-------------|---------|------|
  | Name        | Type        | Version | Size |
  |-------------|-------------|---------|------|
  | start       | Index       | 0.33    | 4    |
  | cardinality | Cardinality | 0.33    | 4    |
  |-------------|-------------|---------|------|
*/
enum an_ifc_sequence_part : uint8_t {};
using an_ifc_sequence_storage = an_ifc_sequence_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_sequence_bytes = const an_ifc_sequence_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_sequence_bytes = an_ifc_sequence_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_sequence = an_ifc_Byte_buffer<an_ifc_sequence_storage>;


/*
  |-------------------------------------|
  |   SourceLocation - 0.33 (8 bytes)   |
  |--------|-----------|---------|------|
  | Name   | Type      | Version | Size |
  |--------|-----------|---------|------|
  | line   | LineIndex | 0.33    | 4    |
  | column | Column    | 0.33    | 4    |
  |--------|-----------|---------|------|
*/
enum an_ifc_source_location_part : uint8_t {};
using an_ifc_source_location_storage = an_ifc_source_location_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_source_location_bytes = const an_ifc_source_location_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_source_location_bytes = an_ifc_source_location_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_source_location =
                            an_ifc_Byte_buffer<an_ifc_source_location_storage>;


/*
  |--------------------------------------------------------|
  |              FileHeader - 0.33 (69 bytes)              |
  |--------------------|------------------|---------|------|
  | Name               | Type             | Version | Size |
  |--------------------|------------------|---------|------|
  | checksum           | SHA256           | 0.33    | 32   |
  | major_version      | Version          | 0.33    | 1    |
  | minor_version      | Version          | 0.33    | 1    |
  | abi                | Abi              | 0.33    | 1    |
  | arch               | ArchitectureSort | 0.33    | 1    |
  | dialect            | LanguageVersion  | 0.33    | 4    |
  | string_table_bytes | ByteOffset       | 0.33    | 4    |
  | string_table_size  | Cardinality      | 0.33    | 4    |
  | unit               | UnitIndex        | 0.33    | 4    |
  | src_path           | TextOffset       | 0.33    | 4    |
  | global_scope       | ScopeIndex       | 0.33    | 4    |
  | toc                | ByteOffset       | 0.33    | 4    |
  | partition_count    | Cardinality      | 0.33    | 4    |
  | internal           | bool             | 0.33    | 1    |
  |--------------------|------------------|---------|------|
*/
enum an_ifc_file_header_part : uint8_t {};
using an_ifc_file_header_storage = an_ifc_file_header_part[69];
#if USE_MMAP_FOR_MODULES
using an_ifc_file_header_bytes = const an_ifc_file_header_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_file_header_bytes = an_ifc_file_header_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_file_header = an_ifc_Byte_buffer<an_ifc_file_header_storage>;


/*
  |--------------------------------------------|
  |        Partition - 0.33 (16 bytes)         |
  |-------------|-------------|---------|------|
  | Name        | Type        | Version | Size |
  |-------------|-------------|---------|------|
  | name        | TextOffset  | 0.33    | 4    |
  | offset      | ByteOffset  | 0.33    | 4    |
  | cardinality | Cardinality | 0.33    | 4    |
  | entry_size  | EntitySize  | 0.33    | 4    |
  |-------------|-------------|---------|------|
*/
enum an_ifc_partition_part : uint8_t {};
using an_ifc_partition_storage = an_ifc_partition_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_partition_bytes = const an_ifc_partition_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_partition_bytes = an_ifc_partition_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_partition = an_ifc_Byte_buffer<an_ifc_partition_storage>;


/*
  |--------------------------------------|
  |     AttrBasic - 0.33 (16 bytes)      |
  |------|--------------|---------|------|
  | Name | Type         | Version | Size |
  |------|--------------|---------|------|
  | word | NestableWord | 0.33    | 16   |
  |------|--------------|---------|------|
*/
enum an_ifc_attr_basic_part : uint8_t {};
using an_ifc_attr_basic_storage = an_ifc_attr_basic_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_attr_basic_bytes = const an_ifc_attr_basic_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_attr_basic_bytes = an_ifc_attr_basic_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_attr_basic = an_ifc_Byte_buffer<an_ifc_attr_basic_storage>;


/*
  |----------------------------------------|
  |      AttrCalled - 0.33 (8 bytes)       |
  |-----------|-----------|---------|------|
  | Name      | Type      | Version | Size |
  |-----------|-----------|---------|------|
  | function  | AttrIndex | 0.33    | 4    |
  | arguments | AttrIndex | 0.33    | 4    |
  |-----------|-----------|---------|------|
*/
enum an_ifc_attr_called_part : uint8_t {};
using an_ifc_attr_called_storage = an_ifc_attr_called_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_attr_called_bytes = const an_ifc_attr_called_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_attr_called_bytes = an_ifc_attr_called_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_attr_called = an_ifc_Byte_buffer<an_ifc_attr_called_storage>;


/*
  |-----------------------------------------|
  |     AttrElaborated - 0.33 (4 bytes)     |
  |------------|-----------|---------|------|
  | Name       | Type      | Version | Size |
  |------------|-----------|---------|------|
  | expression | ExprIndex | 0.33    | 4    |
  |------------|-----------|---------|------|
*/
enum an_ifc_attr_elaborated_part : uint8_t {};
using an_ifc_attr_elaborated_storage = an_ifc_attr_elaborated_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_attr_elaborated_bytes = const an_ifc_attr_elaborated_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_attr_elaborated_bytes = an_ifc_attr_elaborated_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_attr_elaborated =
                            an_ifc_Byte_buffer<an_ifc_attr_elaborated_storage>;


/*
  |--------------------------------------|
  |    AttrExpanded - 0.33 (4 bytes)     |
  |---------|-----------|---------|------|
  | Name    | Type      | Version | Size |
  |---------|-----------|---------|------|
  | operand | AttrIndex | 0.33    | 4    |
  |---------|-----------|---------|------|
*/
enum an_ifc_attr_expanded_part : uint8_t {};
using an_ifc_attr_expanded_storage = an_ifc_attr_expanded_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_attr_expanded_bytes = const an_ifc_attr_expanded_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_attr_expanded_bytes = an_ifc_attr_expanded_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_attr_expanded = an_ifc_Byte_buffer<an_ifc_attr_expanded_storage>;


/*
  |----------------------------------------|
  |     AttrFactored - 0.33 (20 bytes)     |
  |--------|--------------|---------|------|
  | Name   | Type         | Version | Size |
  |--------|--------------|---------|------|
  | factor | NestableWord | 0.33    | 16   |
  | terms  | AttrIndex    | 0.33    | 4    |
  |--------|--------------|---------|------|
*/
enum an_ifc_attr_factored_part : uint8_t {};
using an_ifc_attr_factored_storage = an_ifc_attr_factored_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_attr_factored_bytes = const an_ifc_attr_factored_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_attr_factored_bytes = an_ifc_attr_factored_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_attr_factored = an_ifc_Byte_buffer<an_ifc_attr_factored_storage>;


/*
  |-------------------------------------------|
  |       AttrLabeled - 0.33 (20 bytes)       |
  |-----------|--------------|---------|------|
  | Name      | Type         | Version | Size |
  |-----------|--------------|---------|------|
  | label     | NestableWord | 0.33    | 16   |
  | attribute | AttrIndex    | 0.33    | 4    |
  |-----------|--------------|---------|------|
*/
enum an_ifc_attr_labeled_part : uint8_t {};
using an_ifc_attr_labeled_storage = an_ifc_attr_labeled_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_attr_labeled_bytes = const an_ifc_attr_labeled_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_attr_labeled_bytes = an_ifc_attr_labeled_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_attr_labeled = an_ifc_Byte_buffer<an_ifc_attr_labeled_storage>;


/*
  |----------------------------------------|
  |      AttrScoped - 0.33 (32 bytes)      |
  |--------|--------------|---------|------|
  | Name   | Type         | Version | Size |
  |--------|--------------|---------|------|
  | scope  | NestableWord | 0.33    | 16   |
  | member | NestableWord | 0.33    | 16   |
  |--------|--------------|---------|------|
*/
enum an_ifc_attr_scoped_part : uint8_t {};
using an_ifc_attr_scoped_storage = an_ifc_attr_scoped_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_attr_scoped_bytes = const an_ifc_attr_scoped_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_attr_scoped_bytes = an_ifc_attr_scoped_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_attr_scoped = an_ifc_Byte_buffer<an_ifc_attr_scoped_storage>;


/*
  |--------------------------------------------|
  |         AttrTuple - 0.33 (8 bytes)         |
  |-------------|-------------|---------|------|
  | Name        | Type        | Version | Size |
  |-------------|-------------|---------|------|
  | start       | Index       | 0.33    | 4    |
  | cardinality | Cardinality | 0.33    | 4    |
  |-------------|-------------|---------|------|
*/
enum an_ifc_attr_tuple_part : uint8_t {};
using an_ifc_attr_tuple_storage = an_ifc_attr_tuple_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_attr_tuple_bytes = const an_ifc_attr_tuple_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_attr_tuple_bytes = an_ifc_attr_tuple_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_attr_tuple = an_ifc_Byte_buffer<an_ifc_attr_tuple_storage>;


/*
  |--------------------------------------------|
  |      ChartMultilevel - 0.33 (8 bytes)      |
  |-------------|-------------|---------|------|
  | Name        | Type        | Version | Size |
  |-------------|-------------|---------|------|
  | start       | Index       | 0.33    | 4    |
  | cardinality | Cardinality | 0.33    | 4    |
  |-------------|-------------|---------|------|
*/
enum an_ifc_chart_multilevel_part : uint8_t {};
using an_ifc_chart_multilevel_storage = an_ifc_chart_multilevel_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_chart_multilevel_bytes = const an_ifc_chart_multilevel_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_chart_multilevel_bytes = an_ifc_chart_multilevel_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_chart_multilevel =
                           an_ifc_Byte_buffer<an_ifc_chart_multilevel_storage>;


/*
  |--------------------------------------------|
  |      ChartUnilevel - 0.33 (12 bytes)       |
  |-------------|-------------|---------|------|
  | Name        | Type        | Version | Size |
  |-------------|-------------|---------|------|
  | start       | Index       | 0.33    | 4    |
  | cardinality | Cardinality | 0.33    | 4    |
  | constraint  | ExprIndex   | 0.33    | 4    |
  |-------------|-------------|---------|------|
*/
enum an_ifc_chart_unilevel_part : uint8_t {};
using an_ifc_chart_unilevel_storage = an_ifc_chart_unilevel_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_chart_unilevel_bytes = const an_ifc_chart_unilevel_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_chart_unilevel_bytes = an_ifc_chart_unilevel_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_chart_unilevel =
                             an_ifc_Byte_buffer<an_ifc_chart_unilevel_storage>;


/*
  |--------------------------------------------|
  |         ConstF64 - 0.33 (12 bytes)         |
  |-------------|-------------|---------|------|
  | Name        | Type        | Version | Size |
  |-------------|-------------|---------|------|
  | value       | IEEELEFloat | 0.33    | 8    |
  | __padding__ | uint8_t[4]  |         | 4    |
  |-------------|-------------|---------|------|
*/
enum an_ifc_const_f64_part : uint8_t {};
using an_ifc_const_f64_storage = an_ifc_const_f64_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_const_f64_bytes = const an_ifc_const_f64_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_const_f64_bytes = an_ifc_const_f64_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_const_f64 = an_ifc_Byte_buffer<an_ifc_const_f64_storage>;


/*
  |-------------------------------|
  |   ConstI64 - 0.33 (8 bytes)   |
  |-------|------|---------|------|
  | Name  | Type | Version | Size |
  |-------|------|---------|------|
  | value | u64  | 0.33    | 8    |
  |-------|------|---------|------|
*/
enum an_ifc_const_i64_part : uint8_t {};
using an_ifc_const_i64_storage = an_ifc_const_i64_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_const_i64_bytes = const an_ifc_const_i64_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_const_i64_bytes = an_ifc_const_i64_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_const_i64 = an_ifc_Byte_buffer<an_ifc_const_i64_storage>;


/*
  |---------------------------------------|
  |      ConstStr - 0.33 (12 bytes)       |
  |--------|-------------|---------|------|
  | Name   | Type        | Version | Size |
  |--------|-------------|---------|------|
  | start  | TextOffset  | 0.33    | 4    |
  | length | Cardinality | 0.33    | 4    |
  | suffix | TextOffset  | 0.33    | 4    |
  |--------|-------------|---------|------|
*/
enum an_ifc_const_str_part : uint8_t {};
using an_ifc_const_str_storage = an_ifc_const_str_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_const_str_bytes = const an_ifc_const_str_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_const_str_bytes = an_ifc_const_str_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_const_str = an_ifc_Byte_buffer<an_ifc_const_str_storage>;


/*
  |-------------------------------------------------------|
  |              DeclAlias - 0.33 (26 bytes)              |
  |------------|-------------------------|---------|------|
  | Name       | Type                    | Version | Size |
  |------------|-------------------------|---------|------|
  | name       | TextOffset              | 0.33    | 4    |
  | locus      | SourceLocation          | 0.33    | 8    |
  | type       | TypeIndex               | 0.33    | 4    |
  | home_scope | DeclIndex               | 0.33    | 4    |
  | aliasee    | TypeIndex               | 0.33    | 4    |
  | specifiers | BasicSpecifiersBitfield | 0.33    | 1    |
  | access     | AccessSort              | 0.33    | 1    |
  |------------|-------------------------|---------|------|

  |-------------------------------------------------------|
  |              DeclAlias - 0.41 (26 bytes)              |
  |------------|-------------------------|---------|------|
  | Name       | Type                    | Version | Size |
  |------------|-------------------------|---------|------|
  | name       | TextOffset              | 0.33    | 4    |
  | locus      | SourceLocation          | 0.33    | 8    |
  | type       | TypeIndex               | 0.33    | 4    |
  | home_scope | DeclIndex               | 0.41    | 4    |
  | aliasee    | TypeIndex               | 0.33    | 4    |
  | specifiers | BasicSpecifiersBitfield | 0.33    | 1    |
  | access     | AccessSort              | 0.33    | 1    |
  |------------|-------------------------|---------|------|
  | home_scope | DeclIndex               | 0.41    | RF   |
  |------------|-------------------------|---------|------|
*/
enum an_ifc_decl_alias_part : uint8_t {};
using an_ifc_decl_alias_storage = an_ifc_decl_alias_part[26];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_alias_bytes = const an_ifc_decl_alias_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_alias_bytes = an_ifc_decl_alias_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_alias = an_ifc_Byte_buffer<an_ifc_decl_alias_storage>;


/*
  |------------------------------------------------------------|
  |               DeclBitfield - 0.33 (32 bytes)               |
  |-------------|-----------------------------|---------|------|
  | Name        | Type                        | Version | Size |
  |-------------|-----------------------------|---------|------|
  | name        | TextOffset                  | 0.33    | 4    |
  | locus       | SourceLocation              | 0.33    | 8    |
  | type        | TypeIndex                   | 0.33    | 4    |
  | home_scope  | DeclIndex                   | 0.33    | 4    |
  | width       | ExprIndex                   | 0.33    | 4    |
  | initializer | ExprIndex                   | 0.33    | 4    |
  | traits      | ObjectTraitsBitfield        | 0.33    | 1    |
  | specifiers  | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access      | AccessSort                  | 0.33    | 1    |
  | properties  | ReachablePropertiesBitfield | 0.33    | 1    |
  |-------------|-----------------------------|---------|------|

  |------------------------------------------------------------|
  |               DeclBitfield - 0.41 (32 bytes)               |
  |-------------|-----------------------------|---------|------|
  | Name        | Type                        | Version | Size |
  |-------------|-----------------------------|---------|------|
  | name        | TextOffset                  | 0.33    | 4    |
  | locus       | SourceLocation              | 0.33    | 8    |
  | type        | TypeIndex                   | 0.33    | 4    |
  | home_scope  | DeclIndex                   | 0.41    | 4    |
  | width       | ExprIndex                   | 0.33    | 4    |
  | initializer | ExprIndex                   | 0.33    | 4    |
  | traits      | ObjectTraitsBitfield        | 0.33    | 1    |
  | specifiers  | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access      | AccessSort                  | 0.33    | 1    |
  | properties  | ReachablePropertiesBitfield | 0.33    | 1    |
  |-------------|-----------------------------|---------|------|
  | home_scope  | DeclIndex                   | 0.41    | RF   |
  |-------------|-----------------------------|---------|------|
*/
enum an_ifc_decl_bitfield_part : uint8_t {};
using an_ifc_decl_bitfield_storage = an_ifc_decl_bitfield_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_bitfield_bytes = const an_ifc_decl_bitfield_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_bitfield_bytes = an_ifc_decl_bitfield_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_bitfield = an_ifc_Byte_buffer<an_ifc_decl_bitfield_storage>;


/*
  |-------------------------------------------------------|
  |             DeclConcept - 0.33 (40 bytes)             |
  |------------|-------------------------|---------|------|
  | Name       | Type                    | Version | Size |
  |------------|-------------------------|---------|------|
  | name       | TextOffset              | 0.33    | 4    |
  | locus      | SourceLocation          | 0.33    | 8    |
  | home_scope | DeclIndex               | 0.33    | 4    |
  | type       | TypeIndex               | 0.33    | 4    |
  | chart      | ChartIndex              | 0.33    | 4    |
  | constraint | ExprIndex               | 0.33    | 4    |
  | specifiers | BasicSpecifiersBitfield | 0.33    | 1    |
  | access     | AccessSort              | 0.33    | 1    |
  | unknown    | u16                     | 0.33    | 2    |
  | head       | SentenceIndex           | 0.33    | 4    |
  | body       | SentenceIndex           | 0.33    | 4    |
  |------------|-------------------------|---------|------|

  |-------------------------------------------------------|
  |             DeclConcept - 0.41 (40 bytes)             |
  |------------|-------------------------|---------|------|
  | Name       | Type                    | Version | Size |
  |------------|-------------------------|---------|------|
  | name       | TextOffset              | 0.33    | 4    |
  | locus      | SourceLocation          | 0.33    | 8    |
  | home_scope | DeclIndex               | 0.41    | 4    |
  | type       | TypeIndex               | 0.33    | 4    |
  | chart      | ChartIndex              | 0.33    | 4    |
  | constraint | ExprIndex               | 0.33    | 4    |
  | specifiers | BasicSpecifiersBitfield | 0.33    | 1    |
  | access     | AccessSort              | 0.33    | 1    |
  | unknown    | u16                     | 0.33    | 2    |
  | head       | SentenceIndex           | 0.33    | 4    |
  | body       | SentenceIndex           | 0.33    | 4    |
  |------------|-------------------------|---------|------|
  | home_scope | DeclIndex               | 0.41    | RF   |
  |------------|-------------------------|---------|------|
*/
enum an_ifc_decl_concept_part : uint8_t {};
using an_ifc_decl_concept_storage = an_ifc_decl_concept_part[40];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_concept_bytes = const an_ifc_decl_concept_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_concept_bytes = an_ifc_decl_concept_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_concept = an_ifc_Byte_buffer<an_ifc_decl_concept_storage>;


/*
  |-----------------------------------------------------------|
  |             DeclConstructor - 0.33 (29 bytes)             |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | name       | TextOffset                  | 0.33    | 4    |
  | locus      | SourceLocation              | 0.33    | 8    |
  | type       | TypeIndex                   | 0.33    | 4    |
  | home_scope | DeclIndex                   | 0.33    | 4    |
  | chart      | ChartIndex                  | 0.33    | 4    |
  | traits     | FunctionTraitsBitfield      | 0.33    | 2    |
  | specifiers | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access     | AccessSort                  | 0.33    | 1    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|
  | name       | NameIndex                   | ANY     | RF   |
  |------------|-----------------------------|---------|------|

  |-----------------------------------------------------------|
  |             DeclConstructor - 0.41 (29 bytes)             |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | name       | TextOffset                  | 0.33    | 4    |
  | locus      | SourceLocation              | 0.33    | 8    |
  | type       | TypeIndex                   | 0.33    | 4    |
  | home_scope | DeclIndex                   | 0.41    | 4    |
  | chart      | ChartIndex                  | 0.33    | 4    |
  | traits     | FunctionTraitsBitfield      | 0.33    | 2    |
  | specifiers | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access     | AccessSort                  | 0.33    | 1    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|
  | home_scope | DeclIndex                   | 0.41    | RF   |
  | name       | NameIndex                   | ANY     | RF   |
  |------------|-----------------------------|---------|------|
*/
enum an_ifc_decl_constructor_part : uint8_t {};
using an_ifc_decl_constructor_storage = an_ifc_decl_constructor_part[29];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_constructor_bytes = const an_ifc_decl_constructor_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_constructor_bytes = an_ifc_decl_constructor_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_constructor =
                           an_ifc_Byte_buffer<an_ifc_decl_constructor_storage>;


/*
  |-------------------------------------------------------|
  |         DeclDeductionGuide - 0.33 (26 bytes)          |
  |------------|-------------------------|---------|------|
  | Name       | Type                    | Version | Size |
  |------------|-------------------------|---------|------|
  | name       | TextOffset              | 0.33    | 4    |
  | locus      | SourceLocation          | 0.33    | 8    |
  | home_scope | DeclIndex               | 0.33    | 4    |
  | source     | ChartIndex              | 0.33    | 4    |
  | target     | ExprIndex               | 0.33    | 4    |
  | traits     | GuideTraitsBitfield     | 0.33    | 1    |
  | specifiers | BasicSpecifiersBitfield | 0.33    | 1    |
  |------------|-------------------------|---------|------|

  |-------------------------------------------------------|
  |         DeclDeductionGuide - 0.41 (26 bytes)          |
  |------------|-------------------------|---------|------|
  | Name       | Type                    | Version | Size |
  |------------|-------------------------|---------|------|
  | name       | TextOffset              | 0.33    | 4    |
  | locus      | SourceLocation          | 0.33    | 8    |
  | home_scope | DeclIndex               | 0.41    | 4    |
  | source     | ChartIndex              | 0.33    | 4    |
  | target     | ExprIndex               | 0.33    | 4    |
  | traits     | GuideTraitsBitfield     | 0.33    | 1    |
  | specifiers | BasicSpecifiersBitfield | 0.33    | 1    |
  |------------|-------------------------|---------|------|
  | home_scope | DeclIndex               | 0.41    | RF   |
  |------------|-------------------------|---------|------|
*/
enum an_ifc_decl_deduction_guide_part : uint8_t {};
using an_ifc_decl_deduction_guide_storage =
                                          an_ifc_decl_deduction_guide_part[26];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_deduction_guide_bytes =
                                    const an_ifc_decl_deduction_guide_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_deduction_guide_bytes = an_ifc_decl_deduction_guide_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_deduction_guide =
                       an_ifc_Byte_buffer<an_ifc_decl_deduction_guide_storage>;


/*
  |-----------------------------------------------------------|
  |             DeclDestructor - 0.33 (30 bytes)              |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | name       | TextOffset                  | 0.33    | 4    |
  | locus      | SourceLocation              | 0.33    | 8    |
  | home_scope | DeclIndex                   | 0.33    | 4    |
  | eh_spec    | NoexceptSpecification       | 0.33    | 8    |
  | traits     | FunctionTraitsBitfield      | 0.33    | 2    |
  | specifiers | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access     | AccessSort                  | 0.33    | 1    |
  | convention | CallingConventionSort       | 0.33    | 1    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|
  | name       | NameIndex                   | ANY     | RF   |
  |------------|-----------------------------|---------|------|

  |-----------------------------------------------------------|
  |             DeclDestructor - 0.41 (30 bytes)              |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | name       | TextOffset                  | 0.33    | 4    |
  | locus      | SourceLocation              | 0.33    | 8    |
  | home_scope | DeclIndex                   | 0.41    | 4    |
  | eh_spec    | NoexceptSpecification       | 0.33    | 8    |
  | traits     | FunctionTraitsBitfield      | 0.33    | 2    |
  | specifiers | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access     | AccessSort                  | 0.33    | 1    |
  | convention | CallingConventionSort       | 0.33    | 1    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|
  | home_scope | DeclIndex                   | 0.41    | RF   |
  | name       | NameIndex                   | ANY     | RF   |
  |------------|-----------------------------|---------|------|
*/
enum an_ifc_decl_destructor_part : uint8_t {};
using an_ifc_decl_destructor_storage = an_ifc_decl_destructor_part[30];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_destructor_bytes = const an_ifc_decl_destructor_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_destructor_bytes = an_ifc_decl_destructor_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_destructor =
                            an_ifc_Byte_buffer<an_ifc_decl_destructor_storage>;


/*
  |------------------------------------------------------------|
  |             DeclEnumeration - 0.33 (39 bytes)              |
  |-------------|-----------------------------|---------|------|
  | Name        | Type                        | Version | Size |
  |-------------|-----------------------------|---------|------|
  | name        | TextOffset                  | 0.33    | 4    |
  | locus       | SourceLocation              | 0.33    | 8    |
  | type        | TypeIndex                   | 0.33    | 4    |
  | base        | TypeIndex                   | 0.33    | 4    |
  | initializer | Sequence                    | 0.33    | 8    |
  | home_scope  | DeclIndex                   | 0.33    | 4    |
  | alignment   | ExprIndex                   | 0.33    | 4    |
  | specifiers  | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access      | AccessSort                  | 0.33    | 1    |
  | properties  | ReachablePropertiesBitfield | 0.33    | 1    |
  |-------------|-----------------------------|---------|------|

  |------------------------------------------------------------|
  |             DeclEnumeration - 0.41 (39 bytes)              |
  |-------------|-----------------------------|---------|------|
  | Name        | Type                        | Version | Size |
  |-------------|-----------------------------|---------|------|
  | name        | TextOffset                  | 0.33    | 4    |
  | locus       | SourceLocation              | 0.33    | 8    |
  | type        | TypeIndex                   | 0.33    | 4    |
  | base        | TypeIndex                   | 0.33    | 4    |
  | initializer | Sequence                    | 0.33    | 8    |
  | home_scope  | DeclIndex                   | 0.41    | 4    |
  | alignment   | ExprIndex                   | 0.33    | 4    |
  | specifiers  | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access      | AccessSort                  | 0.33    | 1    |
  | properties  | ReachablePropertiesBitfield | 0.33    | 1    |
  |-------------|-----------------------------|---------|------|
  | home_scope  | DeclIndex                   | 0.41    | RF   |
  |-------------|-----------------------------|---------|------|
*/
enum an_ifc_decl_enumeration_part : uint8_t {};
using an_ifc_decl_enumeration_storage = an_ifc_decl_enumeration_part[39];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_enumeration_bytes = const an_ifc_decl_enumeration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_enumeration_bytes = an_ifc_decl_enumeration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_enumeration =
                           an_ifc_Byte_buffer<an_ifc_decl_enumeration_storage>;


/*
  |--------------------------------------------------------|
  |            DeclEnumerator - 0.33 (22 bytes)            |
  |-------------|-------------------------|---------|------|
  | Name        | Type                    | Version | Size |
  |-------------|-------------------------|---------|------|
  | name        | TextOffset              | 0.33    | 4    |
  | locus       | SourceLocation          | 0.33    | 8    |
  | type        | TypeIndex               | 0.33    | 4    |
  | initializer | ExprIndex               | 0.33    | 4    |
  | specifiers  | BasicSpecifiersBitfield | 0.33    | 1    |
  | access      | AccessSort              | 0.33    | 1    |
  |-------------|-------------------------|---------|------|
  | home_scope  | DeclIndex               | 0.33    | RF   |
  |-------------|-------------------------|---------|------|
*/
enum an_ifc_decl_enumerator_part : uint8_t {};
using an_ifc_decl_enumerator_storage = an_ifc_decl_enumerator_part[22];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_enumerator_bytes = const an_ifc_decl_enumerator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_enumerator_bytes = an_ifc_decl_enumerator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_enumerator =
                            an_ifc_Byte_buffer<an_ifc_decl_enumerator_storage>;


/*
  |-------------------------------------------|
  |      DeclExpansion - 0.33 (12 bytes)      |
  |---------|----------------|---------|------|
  | Name    | Type           | Version | Size |
  |---------|----------------|---------|------|
  | operand | DeclIndex      | 0.33    | 4    |
  | locus   | SourceLocation | 0.33    | 8    |
  |---------|----------------|---------|------|

  |-------------------------------------------|
  |      DeclExpansion - 0.41 (12 bytes)      |
  |---------|----------------|---------|------|
  | Name    | Type           | Version | Size |
  |---------|----------------|---------|------|
  | operand | DeclIndex      | 0.41    | 4    |
  | locus   | SourceLocation | 0.33    | 8    |
  |---------|----------------|---------|------|
*/
enum an_ifc_decl_expansion_part : uint8_t {};
using an_ifc_decl_expansion_storage = an_ifc_decl_expansion_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_expansion_bytes = const an_ifc_decl_expansion_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_expansion_bytes = an_ifc_decl_expansion_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_expansion =
                             an_ifc_Byte_buffer<an_ifc_decl_expansion_storage>;


/*
  |--------------------------------------------|
  | DeclExplicitInstantiation - 0.33 (8 bytes) |
  |--------|----------------|----------|-------|
  | Name   | Type           | Version  | Size  |
  |--------|----------------|----------|-------|
  | form   | FormSpecIndex  | 0.33     | 4     |
  | decl   | DeclIndex      | 0.33     | 4     |
  |--------|----------------|----------|-------|
*/
enum an_ifc_decl_explicit_instantiation_part : uint8_t {};
using an_ifc_decl_explicit_instantiation_storage =
                                    an_ifc_decl_explicit_instantiation_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_explicit_instantiation_bytes =
                             const an_ifc_decl_explicit_instantiation_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_explicit_instantiation_bytes =
                                    an_ifc_decl_explicit_instantiation_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_explicit_instantiation =
                an_ifc_Byte_buffer<an_ifc_decl_explicit_instantiation_storage>;


/*
  |---------------------------------------------|
  | DeclExplicitSpecialization - 0.33 (8 bytes) |
  |---------|----------------|----------|-------|
  | Name    | Type           | Version  | Size  |
  |---------|----------------|----------|-------|
  | form    | FormSpecIndex  | 0.33     | 4     |
  | decl    | DeclIndex      | 0.33     | 4     |
  |---------|----------------|----------|-------|
*/
enum an_ifc_decl_explicit_specialization_part : uint8_t {};
using an_ifc_decl_explicit_specialization_storage =
                                   an_ifc_decl_explicit_specialization_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_explicit_specialization_bytes =
                            const an_ifc_decl_explicit_specialization_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_explicit_specialization_bytes =
                                   an_ifc_decl_explicit_specialization_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_explicit_specialization =
               an_ifc_Byte_buffer<an_ifc_decl_explicit_specialization_storage>;


/*
  |------------------------------------------------------------|
  |                DeclField - 0.33 (32 bytes)                 |
  |-------------|-----------------------------|---------|------|
  | Name        | Type                        | Version | Size |
  |-------------|-----------------------------|---------|------|
  | name        | TextOffset                  | 0.33    | 4    |
  | locus       | SourceLocation              | 0.33    | 8    |
  | type        | TypeIndex                   | 0.33    | 4    |
  | home_scope  | DeclIndex                   | 0.33    | 4    |
  | initializer | ExprIndex                   | 0.33    | 4    |
  | alignment   | ExprIndex                   | 0.33    | 4    |
  | traits      | ObjectTraitsBitfield        | 0.33    | 1    |
  | specifiers  | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access      | AccessSort                  | 0.33    | 1    |
  | properties  | ReachablePropertiesBitfield | 0.33    | 1    |
  |-------------|-----------------------------|---------|------|

  |------------------------------------------------------------|
  |                DeclField - 0.41 (32 bytes)                 |
  |-------------|-----------------------------|---------|------|
  | Name        | Type                        | Version | Size |
  |-------------|-----------------------------|---------|------|
  | name        | TextOffset                  | 0.33    | 4    |
  | locus       | SourceLocation              | 0.33    | 8    |
  | type        | TypeIndex                   | 0.33    | 4    |
  | home_scope  | DeclIndex                   | 0.41    | 4    |
  | initializer | ExprIndex                   | 0.33    | 4    |
  | alignment   | ExprIndex                   | 0.33    | 4    |
  | traits      | ObjectTraitsBitfield        | 0.33    | 1    |
  | specifiers  | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access      | AccessSort                  | 0.33    | 1    |
  | properties  | ReachablePropertiesBitfield | 0.33    | 1    |
  |-------------|-----------------------------|---------|------|
  | home_scope  | DeclIndex                   | 0.41    | RF   |
  |-------------|-----------------------------|---------|------|
*/
enum an_ifc_decl_field_part : uint8_t {};
using an_ifc_decl_field_storage = an_ifc_decl_field_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_field_bytes = const an_ifc_decl_field_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_field_bytes = an_ifc_decl_field_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_field = an_ifc_Byte_buffer<an_ifc_decl_field_storage>;


/*
  |-------------------------------------|
  |     DeclFriend - 0.33 (4 bytes)     |
  |--------|-----------|---------|------|
  | Name   | Type      | Version | Size |
  |--------|-----------|---------|------|
  | entity | ExprIndex | 0.33    | 4    |
  |--------|-----------|---------|------|
*/
enum an_ifc_decl_friend_part : uint8_t {};
using an_ifc_decl_friend_storage = an_ifc_decl_friend_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_friend_bytes = const an_ifc_decl_friend_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_friend_bytes = an_ifc_decl_friend_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_friend = an_ifc_Byte_buffer<an_ifc_decl_friend_storage>;


/*
  |-----------------------------------------------------------|
  |              DeclFunction - 0.33 (29 bytes)               |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | name       | NameIndex                   | 0.33    | 4    |
  | locus      | SourceLocation              | 0.33    | 8    |
  | type       | TypeIndex                   | 0.33    | 4    |
  | home_scope | DeclIndex                   | 0.33    | 4    |
  | chart      | ChartIndex                  | 0.33    | 4    |
  | traits     | FunctionTraitsBitfield      | 0.33    | 2    |
  | specifiers | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access     | AccessSort                  | 0.33    | 1    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|

  |-----------------------------------------------------------|
  |              DeclFunction - 0.41 (29 bytes)               |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | name       | NameIndex                   | 0.33    | 4    |
  | locus      | SourceLocation              | 0.33    | 8    |
  | type       | TypeIndex                   | 0.33    | 4    |
  | home_scope | DeclIndex                   | 0.41    | 4    |
  | chart      | ChartIndex                  | 0.33    | 4    |
  | traits     | FunctionTraitsBitfield      | 0.33    | 2    |
  | specifiers | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access     | AccessSort                  | 0.33    | 1    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|
  | home_scope | DeclIndex                   | 0.41    | RF   |
  |------------|-----------------------------|---------|------|
*/
enum an_ifc_decl_function_part : uint8_t {};
using an_ifc_decl_function_storage = an_ifc_decl_function_part[29];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_function_bytes = const an_ifc_decl_function_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_function_bytes = an_ifc_decl_function_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_function = an_ifc_Byte_buffer<an_ifc_decl_function_storage>;


/*
  |-------------------------------------------------------|
  |      DeclInheritedConstructor - 0.33 (32 bytes)       |
  |------------|-------------------------|---------|------|
  | Name       | Type                    | Version | Size |
  |------------|-------------------------|---------|------|
  | name       | TextOffset              | 0.33    | 4    |
  | locus      | SourceLocation          | 0.33    | 8    |
  | type       | TypeIndex               | 0.33    | 4    |
  | home_scope | DeclIndex               | 0.33    | 4    |
  | chart      | ChartIndex              | 0.33    | 4    |
  | traits     | FunctionTraitsBitfield  | 0.33    | 2    |
  | specifiers | BasicSpecifiersBitfield | 0.33    | 1    |
  | access     | AccessSort              | 0.33    | 1    |
  | base_ctor  | DeclIndex               | 0.33    | 4    |
  |------------|-------------------------|---------|------|

  |-------------------------------------------------------|
  |      DeclInheritedConstructor - 0.41 (32 bytes)       |
  |------------|-------------------------|---------|------|
  | Name       | Type                    | Version | Size |
  |------------|-------------------------|---------|------|
  | name       | TextOffset              | 0.33    | 4    |
  | locus      | SourceLocation          | 0.33    | 8    |
  | type       | TypeIndex               | 0.33    | 4    |
  | home_scope | DeclIndex               | 0.41    | 4    |
  | chart      | ChartIndex              | 0.33    | 4    |
  | traits     | FunctionTraitsBitfield  | 0.33    | 2    |
  | specifiers | BasicSpecifiersBitfield | 0.33    | 1    |
  | access     | AccessSort              | 0.33    | 1    |
  | base_ctor  | DeclIndex               | 0.41    | 4    |
  |------------|-------------------------|---------|------|
  | home_scope | DeclIndex               | 0.41    | RF   |
  |------------|-------------------------|---------|------|
*/
enum an_ifc_decl_inherited_constructor_part : uint8_t {};
using an_ifc_decl_inherited_constructor_storage =
                                    an_ifc_decl_inherited_constructor_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_inherited_constructor_bytes =
                              const an_ifc_decl_inherited_constructor_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_inherited_constructor_bytes =
                                     an_ifc_decl_inherited_constructor_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_inherited_constructor =
                 an_ifc_Byte_buffer<an_ifc_decl_inherited_constructor_storage>;


/*
  |-------------------------------------------------------|
  |            DeclIntrinsic - 0.33 (22 bytes)            |
  |------------|-------------------------|---------|------|
  | Name       | Type                    | Version | Size |
  |------------|-------------------------|---------|------|
  | name       | TextOffset              | 0.33    | 4    |
  | locus      | SourceLocation          | 0.33    | 8    |
  | type       | TypeIndex               | 0.33    | 4    |
  | home_scope | DeclIndex               | 0.33    | 4    |
  | specifiers | BasicSpecifiersBitfield | 0.33    | 1    |
  | access     | AccessSort              | 0.33    | 1    |
  |------------|-------------------------|---------|------|

  |-------------------------------------------------------|
  |            DeclIntrinsic - 0.41 (22 bytes)            |
  |------------|-------------------------|---------|------|
  | Name       | Type                    | Version | Size |
  |------------|-------------------------|---------|------|
  | name       | TextOffset              | 0.33    | 4    |
  | locus      | SourceLocation          | 0.33    | 8    |
  | type       | TypeIndex               | 0.33    | 4    |
  | home_scope | DeclIndex               | 0.41    | 4    |
  | specifiers | BasicSpecifiersBitfield | 0.33    | 1    |
  | access     | AccessSort              | 0.33    | 1    |
  |------------|-------------------------|---------|------|
  | home_scope | DeclIndex               | 0.41    | RF   |
  |------------|-------------------------|---------|------|
*/
enum an_ifc_decl_intrinsic_part : uint8_t {};
using an_ifc_decl_intrinsic_storage = an_ifc_decl_intrinsic_part[22];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_intrinsic_bytes = const an_ifc_decl_intrinsic_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_intrinsic_bytes = an_ifc_decl_intrinsic_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_intrinsic =
                             an_ifc_Byte_buffer<an_ifc_decl_intrinsic_storage>;


/*
  |-----------------------------------------------------------|
  |               DeclMethod - 0.33 (29 bytes)                |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | name       | NameIndex                   | 0.33    | 4    |
  | locus      | SourceLocation              | 0.33    | 8    |
  | type       | TypeIndex                   | 0.33    | 4    |
  | home_scope | DeclIndex                   | 0.33    | 4    |
  | chart      | ChartIndex                  | 0.33    | 4    |
  | traits     | FunctionTraitsBitfield      | 0.33    | 2    |
  | specifiers | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access     | AccessSort                  | 0.33    | 1    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|

  |-----------------------------------------------------------|
  |               DeclMethod - 0.41 (29 bytes)                |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | name       | NameIndex                   | 0.33    | 4    |
  | locus      | SourceLocation              | 0.33    | 8    |
  | type       | TypeIndex                   | 0.33    | 4    |
  | home_scope | DeclIndex                   | 0.41    | 4    |
  | chart      | ChartIndex                  | 0.33    | 4    |
  | traits     | FunctionTraitsBitfield      | 0.33    | 2    |
  | specifiers | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access     | AccessSort                  | 0.33    | 1    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|
  | home_scope | DeclIndex                   | 0.41    | RF   |
  |------------|-----------------------------|---------|------|
*/
enum an_ifc_decl_method_part : uint8_t {};
using an_ifc_decl_method_storage = an_ifc_decl_method_part[29];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_method_bytes = const an_ifc_decl_method_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_method_bytes = an_ifc_decl_method_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_method = an_ifc_Byte_buffer<an_ifc_decl_method_storage>;


/*
  |-----------------------------------------|
  |   DeclOutputSegment - 0.33 (16 bytes)   |
  |--------|---------------|---------|------|
  | Name   | Type          | Version | Size |
  |--------|---------------|---------|------|
  | name   | TextOffset    | 0.33    | 4    |
  | ID     | TextOffset    | 0.33    | 4    |
  | traits | SegmentTraits | 0.33    | 4    |
  | type   | SegmentType   | 0.33    | 4    |
  |--------|---------------|---------|------|
*/
enum an_ifc_decl_output_segment_part : uint8_t {};
using an_ifc_decl_output_segment_storage = an_ifc_decl_output_segment_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_output_segment_bytes =
                                     const an_ifc_decl_output_segment_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_output_segment_bytes = an_ifc_decl_output_segment_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_output_segment =
                        an_ifc_Byte_buffer<an_ifc_decl_output_segment_storage>;


/*
  |------------------------------------------------------------|
  |              DeclParameter - 0.33 (35 bytes)               |
  |-------------|-----------------------------|---------|------|
  | Name        | Type                        | Version | Size |
  |-------------|-----------------------------|---------|------|
  | name        | TextOffset                  | 0.33    | 4    |
  | locus       | SourceLocation              | 0.33    | 8    |
  | type        | TypeIndex                   | 0.33    | 4    |
  | constraint  | ExprIndex                   | 0.33    | 4    |
  | initializer | ExprIndex                   | 0.33    | 4    |
  | level       | ParameterLevel              | 0.33    | 4    |
  | position    | ParameterPosition           | 0.33    | 4    |
  | sort        | ParameterSort               | 0.33    | 1    |
  | properties  | ReachablePropertiesBitfield | 0.33    | 1    |
  | pack        | bool                        | 0.33    | 1    |
  |-------------|-----------------------------|---------|------|
*/
enum an_ifc_decl_parameter_part : uint8_t {};
using an_ifc_decl_parameter_storage = an_ifc_decl_parameter_part[35];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_parameter_bytes = const an_ifc_decl_parameter_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_parameter_bytes = an_ifc_decl_parameter_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_parameter =
                             an_ifc_Byte_buffer<an_ifc_decl_parameter_storage>;


/*
  |-----------------------------------------------------------|
  |        DeclPartialSpecialization - 0.33 (43 bytes)        |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | name       | NameIndex                   | 0.33    | 4    |
  | locus      | SourceLocation              | 0.33    | 8    |
  | home_scope | DeclIndex                   | 0.33    | 4    |
  | chart      | ChartIndex                  | 0.33    | 4    |
  | entity     | ParameterizedEntity         | 0.33    | 16   |
  | form       | FormSpecIndex               | 0.33    | 4    |
  | specifiers | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access     | AccessSort                  | 0.33    | 1    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|
  | home_scope | DeclIndex                   | ANY     | RF   |
  | name       | NameIndex                   | ANY     | RF   |
  |------------|-----------------------------|---------|------|

  |-----------------------------------------------------------|
  |        DeclPartialSpecialization - 0.41 (43 bytes)        |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | name       | NameIndex                   | 0.33    | 4    |
  | locus      | SourceLocation              | 0.33    | 8    |
  | home_scope | DeclIndex                   | 0.41    | 4    |
  | chart      | ChartIndex                  | 0.33    | 4    |
  | entity     | ParameterizedEntity         | 0.41    | 16   |
  | form       | FormSpecIndex               | 0.33    | 4    |
  | specifiers | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access     | AccessSort                  | 0.33    | 1    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|
  | home_scope | DeclIndex                   | ANY     | RF   |
  | name       | NameIndex                   | ANY     | RF   |
  |------------|-----------------------------|---------|------|
*/
enum an_ifc_decl_partial_specialization_part : uint8_t {};
using an_ifc_decl_partial_specialization_storage =
                                   an_ifc_decl_partial_specialization_part[43];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_partial_specialization_bytes =
                             const an_ifc_decl_partial_specialization_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_partial_specialization_bytes =
                                    an_ifc_decl_partial_specialization_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_partial_specialization =
                an_ifc_Byte_buffer<an_ifc_decl_partial_specialization_storage>;


/*
  |--------------------------------------|
  |    DeclProperty - 0.33 (12 bytes)    |
  |--------|------------|---------|------|
  | Name   | Type       | Version | Size |
  |--------|------------|---------|------|
  | member | DeclIndex  | 0.33    | 4    |
  | getter | TextOffset | 0.33    | 4    |
  | setter | TextOffset | 0.33    | 4    |
  |--------|------------|---------|------|

  |--------------------------------------|
  |    DeclProperty - 0.41 (12 bytes)    |
  |--------|------------|---------|------|
  | Name   | Type       | Version | Size |
  |--------|------------|---------|------|
  | member | DeclIndex  | 0.41    | 4    |
  | getter | TextOffset | 0.33    | 4    |
  | setter | TextOffset | 0.33    | 4    |
  |--------|------------|---------|------|
*/
enum an_ifc_decl_property_part : uint8_t {};
using an_ifc_decl_property_storage = an_ifc_decl_property_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_property_bytes = const an_ifc_decl_property_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_property_bytes = an_ifc_decl_property_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_property = an_ifc_Byte_buffer<an_ifc_decl_property_storage>;


/*
  |-------------------------------------------------|
  |         DeclReference - 0.33 (12 bytes)         |
  |-------------|------------------|---------|------|
  | Name        | Type             | Version | Size |
  |-------------|------------------|---------|------|
  | unit        | ModuleReference  | 0.33    | 8    |
  | local_index | DeclForeignIndex | 0.33    | 4    |
  |-------------|------------------|---------|------|
  | index       | DeclIndex        | ANY     | RF   |
  |-------------|------------------|---------|------|
*/
enum an_ifc_decl_reference_part : uint8_t {};
using an_ifc_decl_reference_storage = an_ifc_decl_reference_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_reference_bytes = const an_ifc_decl_reference_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_reference_bytes = an_ifc_decl_reference_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_reference =
                             an_ifc_Byte_buffer<an_ifc_decl_reference_storage>;


/*
  |------------------------------------------------------------|
  |                DeclScope - 0.33 (38 bytes)                 |
  |-------------|-----------------------------|---------|------|
  | Name        | Type                        | Version | Size |
  |-------------|-----------------------------|---------|------|
  | name        | NameIndex                   | 0.33    | 4    |
  | locus       | SourceLocation              | 0.33    | 8    |
  | type        | TypeIndex                   | 0.33    | 4    |
  | base        | TypeIndex                   | 0.33    | 4    |
  | initializer | ScopeIndex                  | 0.33    | 4    |
  | home_scope  | DeclIndex                   | 0.33    | 4    |
  | alignment   | ExprIndex                   | 0.33    | 4    |
  | pack_size   | PackSize                    | 0.33    | 2    |
  | specifiers  | BasicSpecifiersBitfield     | 0.33    | 1    |
  | traits      | ScopeTraitsBitfield         | 0.33    | 1    |
  | access      | AccessSort                  | 0.33    | 1    |
  | properties  | ReachablePropertiesBitfield | 0.33    | 1    |
  |-------------|-----------------------------|---------|------|

  |------------------------------------------------------------|
  |                DeclScope - 0.41 (38 bytes)                 |
  |-------------|-----------------------------|---------|------|
  | Name        | Type                        | Version | Size |
  |-------------|-----------------------------|---------|------|
  | name        | NameIndex                   | 0.33    | 4    |
  | locus       | SourceLocation              | 0.33    | 8    |
  | type        | TypeIndex                   | 0.33    | 4    |
  | base        | TypeIndex                   | 0.33    | 4    |
  | initializer | ScopeIndex                  | 0.33    | 4    |
  | home_scope  | DeclIndex                   | 0.41    | 4    |
  | alignment   | ExprIndex                   | 0.33    | 4    |
  | pack_size   | PackSize                    | 0.33    | 2    |
  | specifiers  | BasicSpecifiersBitfield     | 0.33    | 1    |
  | traits      | ScopeTraitsBitfield         | 0.33    | 1    |
  | access      | AccessSort                  | 0.33    | 1    |
  | properties  | ReachablePropertiesBitfield | 0.33    | 1    |
  |-------------|-----------------------------|---------|------|
  | home_scope  | DeclIndex                   | 0.41    | RF   |
  |-------------|-----------------------------|---------|------|
*/
enum an_ifc_decl_scope_part : uint8_t {};
using an_ifc_decl_scope_storage = an_ifc_decl_scope_part[38];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_scope_bytes = const an_ifc_decl_scope_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_scope_bytes = an_ifc_decl_scope_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_scope = an_ifc_Byte_buffer<an_ifc_decl_scope_storage>;


/*
  |--------------------------------------------------|
  |       DeclSpecialization - 0.41 (9 bytes)        |
  |------------|--------------------|---------|------|
  | Name       | Type               | Version | Size |
  |------------|--------------------|---------|------|
  | form       | FormSpecIndex      | 0.33    | 4    |
  | decl       | DeclIndex          | 0.41    | 4    |
  | sort       | SpecializationSort | 0.33    | 1    |
  |------------|--------------------|---------|------|
  | home_scope | DeclIndex          | ANY     | RF   |
  | locus      | SourceLocation     | ANY     | RF   |
  | name       | NameIndex          | ANY     | RF   |
  |------------|--------------------|---------|------|
*/
enum an_ifc_decl_specialization_part : uint8_t {};
using an_ifc_decl_specialization_storage = an_ifc_decl_specialization_part[9];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_specialization_bytes =
                                     const an_ifc_decl_specialization_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_specialization_bytes = an_ifc_decl_specialization_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_specialization =
                        an_ifc_Byte_buffer<an_ifc_decl_specialization_storage>;


/*
  |-----------------------------------------------------------|
  |              DeclTemplate - 0.33 (43 bytes)               |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | name       | NameIndex                   | 0.33    | 4    |
  | locus      | SourceLocation              | 0.33    | 8    |
  | home_scope | DeclIndex                   | 0.33    | 4    |
  | chart      | ChartIndex                  | 0.33    | 4    |
  | entity     | ParameterizedEntity         | 0.33    | 16   |
  | type       | TypeIndex                   | 0.33    | 4    |
  | specifiers | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access     | AccessSort                  | 0.33    | 1    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|
  | name       | NameIndex                   | ANY     | RF   |
  |------------|-----------------------------|---------|------|

  |-----------------------------------------------------------|
  |              DeclTemplate - 0.41 (43 bytes)               |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | name       | NameIndex                   | 0.33    | 4    |
  | locus      | SourceLocation              | 0.33    | 8    |
  | home_scope | DeclIndex                   | 0.41    | 4    |
  | chart      | ChartIndex                  | 0.33    | 4    |
  | entity     | ParameterizedEntity         | 0.41    | 16   |
  | type       | TypeIndex                   | 0.33    | 4    |
  | specifiers | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access     | AccessSort                  | 0.33    | 1    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|
  | home_scope | DeclIndex                   | 0.41    | RF   |
  | name       | NameIndex                   | ANY     | RF   |
  |------------|-----------------------------|---------|------|
*/
enum an_ifc_decl_template_part : uint8_t {};
using an_ifc_decl_template_storage = an_ifc_decl_template_part[43];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_template_bytes = const an_ifc_decl_template_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_template_bytes = an_ifc_decl_template_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_template = an_ifc_Byte_buffer<an_ifc_decl_template_storage>;


/*
  |-----------------------------------------------------------|
  |              DeclTemploid - 0.33 (21 bytes)               |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | entity     | ParameterizedEntity         | 0.33    | 16   |
  | chart      | ChartIndex                  | 0.33    | 4    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|

  |-----------------------------------------------------------|
  |              DeclTemploid - 0.41 (21 bytes)               |
  |------------|-----------------------------|---------|------|
  | Name       | Type                        | Version | Size |
  |------------|-----------------------------|---------|------|
  | entity     | ParameterizedEntity         | 0.41    | 16   |
  | chart      | ChartIndex                  | 0.33    | 4    |
  | properties | ReachablePropertiesBitfield | 0.33    | 1    |
  |------------|-----------------------------|---------|------|
*/
enum an_ifc_decl_temploid_part : uint8_t {};
using an_ifc_decl_temploid_storage = an_ifc_decl_temploid_part[21];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_temploid_bytes = const an_ifc_decl_temploid_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_temploid_bytes = an_ifc_decl_temploid_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_temploid = an_ifc_Byte_buffer<an_ifc_decl_temploid_storage>;


/*
  |--------------------------------------------|
  |         DeclTuple - 0.33 (8 bytes)         |
  |-------------|-------------|---------|------|
  | Name        | Type        | Version | Size |
  |-------------|-------------|---------|------|
  | start       | Index       | 0.33    | 4    |
  | cardinality | Cardinality | 0.33    | 4    |
  |-------------|-------------|---------|------|
*/
enum an_ifc_decl_tuple_part : uint8_t {};
using an_ifc_decl_tuple_storage = an_ifc_decl_tuple_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_tuple_bytes = const an_ifc_decl_tuple_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_tuple_bytes = an_ifc_decl_tuple_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_tuple = an_ifc_Byte_buffer<an_ifc_decl_tuple_storage>;


/*
  |-------------------------------------------------------|
  |        DeclUsingDeclaration - 0.33 (31 bytes)         |
  |------------|-------------------------|---------|------|
  | Name       | Type                    | Version | Size |
  |------------|-------------------------|---------|------|
  | name       | TextOffset              | 0.33    | 4    |
  | locus      | SourceLocation          | 0.33    | 8    |
  | home_scope | DeclIndex               | 0.33    | 4    |
  | resolution | DeclIndex               | 0.33    | 4    |
  | parent     | ExprIndex               | 0.33    | 4    |
  | name2      | TextOffset              | 0.33    | 4    |
  | specifiers | BasicSpecifiersBitfield | 0.33    | 1    |
  | access     | AccessSort              | 0.33    | 1    |
  | hidden     | bool                    | 0.33    | 1    |
  |------------|-------------------------|---------|------|

  |-------------------------------------------------------|
  |        DeclUsingDeclaration - 0.41 (31 bytes)         |
  |------------|-------------------------|---------|------|
  | Name       | Type                    | Version | Size |
  |------------|-------------------------|---------|------|
  | name       | TextOffset              | 0.33    | 4    |
  | locus      | SourceLocation          | 0.33    | 8    |
  | home_scope | DeclIndex               | 0.41    | 4    |
  | resolution | DeclIndex               | 0.41    | 4    |
  | parent     | ExprIndex               | 0.33    | 4    |
  | name2      | TextOffset              | 0.33    | 4    |
  | specifiers | BasicSpecifiersBitfield | 0.33    | 1    |
  | access     | AccessSort              | 0.33    | 1    |
  | hidden     | bool                    | 0.33    | 1    |
  |------------|-------------------------|---------|------|
  | home_scope | DeclIndex               | 0.41    | RF   |
  |------------|-------------------------|---------|------|
*/
enum an_ifc_decl_using_declaration_part : uint8_t {};
using an_ifc_decl_using_declaration_storage =
                                        an_ifc_decl_using_declaration_part[31];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_using_declaration_bytes =
                                  const an_ifc_decl_using_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_using_declaration_bytes =
                                         an_ifc_decl_using_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_using_declaration =
                     an_ifc_Byte_buffer<an_ifc_decl_using_declaration_storage>;


/*
  |------------------------------------------------------------|
  |               DeclVariable - 0.33 (32 bytes)               |
  |-------------|-----------------------------|---------|------|
  | Name        | Type                        | Version | Size |
  |-------------|-----------------------------|---------|------|
  | name        | NameIndex                   | 0.33    | 4    |
  | locus       | SourceLocation              | 0.33    | 8    |
  | type        | TypeIndex                   | 0.33    | 4    |
  | home_scope  | DeclIndex                   | 0.33    | 4    |
  | initializer | ExprIndex                   | 0.33    | 4    |
  | alignment   | ExprIndex                   | 0.33    | 4    |
  | traits      | ObjectTraitsBitfield        | 0.33    | 1    |
  | specifiers  | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access      | AccessSort                  | 0.33    | 1    |
  | properties  | ReachablePropertiesBitfield | 0.33    | 1    |
  |-------------|-----------------------------|---------|------|

  |------------------------------------------------------------|
  |               DeclVariable - 0.41 (32 bytes)               |
  |-------------|-----------------------------|---------|------|
  | Name        | Type                        | Version | Size |
  |-------------|-----------------------------|---------|------|
  | name        | NameIndex                   | 0.33    | 4    |
  | locus       | SourceLocation              | 0.33    | 8    |
  | type        | TypeIndex                   | 0.33    | 4    |
  | home_scope  | DeclIndex                   | 0.41    | 4    |
  | initializer | ExprIndex                   | 0.33    | 4    |
  | alignment   | ExprIndex                   | 0.33    | 4    |
  | traits      | ObjectTraitsBitfield        | 0.33    | 1    |
  | specifiers  | BasicSpecifiersBitfield     | 0.33    | 1    |
  | access      | AccessSort                  | 0.33    | 1    |
  | properties  | ReachablePropertiesBitfield | 0.33    | 1    |
  |-------------|-----------------------------|---------|------|
  | home_scope  | DeclIndex                   | 0.41    | RF   |
  |-------------|-----------------------------|---------|------|
*/
enum an_ifc_decl_variable_part : uint8_t {};
using an_ifc_decl_variable_storage = an_ifc_decl_variable_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_decl_variable_bytes = const an_ifc_decl_variable_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_decl_variable_bytes = an_ifc_decl_variable_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_decl_variable = an_ifc_Byte_buffer<an_ifc_decl_variable_storage>;


/*
  |-------------------------------------------|
  |       ExprAlignof - 0.33 (16 bytes)       |
  |---------|----------------|---------|------|
  | Name    | Type           | Version | Size |
  |---------|----------------|---------|------|
  | locus   | SourceLocation | 0.33    | 8    |
  | type    | TypeIndex      | 0.33    | 4    |
  | operand | SyntaxIndex    | 0.33    | 4    |
  |---------|----------------|---------|------|
*/
enum an_ifc_expr_alignof_part : uint8_t {};
using an_ifc_expr_alignof_storage = an_ifc_expr_alignof_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_alignof_bytes = const an_ifc_expr_alignof_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_alignof_bytes = an_ifc_expr_alignof_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_alignof = an_ifc_Byte_buffer<an_ifc_expr_alignof_storage>;


/*
  |------------------------------------------------|
  |        ExprArrayValue - 0.33 (20 bytes)        |
  |--------------|----------------|---------|------|
  | Name         | Type           | Version | Size |
  |--------------|----------------|---------|------|
  | locus        | SourceLocation | 0.33    | 8    |
  | type         | TypeIndex      | 0.33    | 4    |
  | elements     | ExprIndex      | 0.33    | 4    |
  | element_type | TypeIndex      | 0.33    | 4    |
  |--------------|----------------|---------|------|
*/
enum an_ifc_expr_array_value_part : uint8_t {};
using an_ifc_expr_array_value_storage = an_ifc_expr_array_value_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_array_value_bytes = const an_ifc_expr_array_value_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_array_value_bytes = an_ifc_expr_array_value_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_array_value =
                           an_ifc_Byte_buffer<an_ifc_expr_array_value_storage>;


/*
  |-----------------------------------------------|
  |    ExprAssignInitializer - 0.33 (12 bytes)    |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | equal       | SourceLocation | 0.33    | 8    |
  | initializer | ExprIndex      | 0.33    | 4    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_expr_assign_initializer_part : uint8_t {};
using an_ifc_expr_assign_initializer_storage =
                                       an_ifc_expr_assign_initializer_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_assign_initializer_bytes =
                                 const an_ifc_expr_assign_initializer_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_assign_initializer_bytes =
                                        an_ifc_expr_assign_initializer_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_assign_initializer =
                    an_ifc_Byte_buffer<an_ifc_expr_assign_initializer_storage>;


/*
  |-----------------------------------------------------|
  |          ExprBinaryFold - 0.33 (23 bytes)           |
  |---------------|--------------------|---------|------|
  | Name          | Type               | Version | Size |
  |---------------|--------------------|---------|------|
  | locus         | SourceLocation     | 0.33    | 8    |
  | type          | TypeIndex          | 0.33    | 4    |
  | left          | ExprIndex          | 0.33    | 4    |
  | right         | ExprIndex          | 0.33    | 4    |
  | operation     | DyadicOperatorSort | 0.33    | 2    |
  | associativity | Associativity      | 0.33    | 1    |
  |---------------|--------------------|---------|------|
*/
enum an_ifc_expr_binary_fold_part : uint8_t {};
using an_ifc_expr_binary_fold_storage = an_ifc_expr_binary_fold_part[23];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_binary_fold_bytes = const an_ifc_expr_binary_fold_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_binary_fold_bytes = an_ifc_expr_binary_fold_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_binary_fold =
                           an_ifc_Byte_buffer<an_ifc_expr_binary_fold_storage>;


/*
  |---------------------------------------------|
  |         ExprCall - 0.33 (20 bytes)          |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | locus     | SourceLocation | 0.33    | 8    |
  | type      | TypeIndex      | 0.33    | 4    |
  | operation | ExprIndex      | 0.33    | 4    |
  | arguments | ExprIndex      | 0.33    | 4    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_expr_call_part : uint8_t {};
using an_ifc_expr_call_storage = an_ifc_expr_call_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_call_bytes = const an_ifc_expr_call_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_call_bytes = an_ifc_expr_call_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_call = an_ifc_Byte_buffer<an_ifc_expr_call_storage>;


/*
  |----------------------------------------------|
  |          ExprCast - 0.33 (22 bytes)          |
  |--------|--------------------|---------|------|
  | Name   | Type               | Version | Size |
  |--------|--------------------|---------|------|
  | locus  | SourceLocation     | 0.33    | 8    |
  | type   | TypeIndex          | 0.33    | 4    |
  | source | ExprIndex          | 0.33    | 4    |
  | target | TypeIndex          | 0.33    | 4    |
  | op     | DyadicOperatorSort | 0.33    | 2    |
  |--------|--------------------|---------|------|
*/
enum an_ifc_expr_cast_part : uint8_t {};
using an_ifc_expr_cast_storage = an_ifc_expr_cast_part[22];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_cast_bytes = const an_ifc_expr_cast_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_cast_bytes = an_ifc_expr_cast_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_cast = an_ifc_Byte_buffer<an_ifc_expr_cast_storage>;


/*
  |------------------------------------------|
  |   ExprCompoundString - 0.33 (20 bytes)   |
  |--------|----------------|---------|------|
  | Name   | Type           | Version | Size |
  |--------|----------------|---------|------|
  | locus  | SourceLocation | 0.33    | 8    |
  | type   | TypeIndex      | 0.33    | 4    |
  | prefix | TextOffset     | 0.33    | 4    |
  | string | ExprIndex      | 0.33    | 4    |
  |--------|----------------|---------|------|
*/
enum an_ifc_expr_compound_string_part : uint8_t {};
using an_ifc_expr_compound_string_storage =
                                          an_ifc_expr_compound_string_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_compound_string_bytes =
                                    const an_ifc_expr_compound_string_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_compound_string_bytes = an_ifc_expr_compound_string_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_compound_string =
                       an_ifc_Byte_buffer<an_ifc_expr_compound_string_storage>;


/*
  |-----------------------------------------|
  |     ExprCondition - 0.33 (16 bytes)     |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | type  | TypeIndex      | 0.33    | 4    |
  | expr  | ExprIndex      | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_condition_part : uint8_t {};
using an_ifc_expr_condition_storage = an_ifc_expr_condition_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_condition_bytes = const an_ifc_expr_condition_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_condition_bytes = an_ifc_expr_condition_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_condition =
                             an_ifc_Byte_buffer<an_ifc_expr_condition_storage>;


/*
  |-----------------------------------------------|
  |  ExprDesignatedInitializer - 0.33 (20 bytes)  |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | locus       | SourceLocation | 0.33    | 8    |
  | type        | TypeIndex      | 0.33    | 4    |
  | member      | TextOffset     | 0.33    | 4    |
  | initializer | ExprIndex      | 0.33    | 4    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_expr_designated_initializer_part : uint8_t {};
using an_ifc_expr_designated_initializer_storage =
                                   an_ifc_expr_designated_initializer_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_designated_initializer_bytes =
                             const an_ifc_expr_designated_initializer_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_designated_initializer_bytes =
                                    an_ifc_expr_designated_initializer_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_designated_initializer =
                an_ifc_Byte_buffer<an_ifc_expr_designated_initializer_storage>;


/*
  |------------------------------------------------------|
  |         ExprDestructorCall - 0.33 (21 bytes)         |
  |--------------------|----------------|---------|------|
  | Name               | Type           | Version | Size |
  |--------------------|----------------|---------|------|
  | locus              | SourceLocation | 0.33    | 8    |
  | type               | TypeIndex      | 0.33    | 4    |
  | name               | ExprIndex      | 0.33    | 4    |
  | decltype_specifier | SyntaxIndex    | 0.33    | 4    |
  | cleanup            | DestructorSort | 0.33    | 1    |
  |--------------------|----------------|---------|------|
*/
enum an_ifc_expr_destructor_call_part : uint8_t {};
using an_ifc_expr_destructor_call_storage =
                                          an_ifc_expr_destructor_call_part[21];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_destructor_call_bytes =
                                    const an_ifc_expr_destructor_call_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_destructor_call_bytes = an_ifc_expr_destructor_call_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_destructor_call =
                       an_ifc_Byte_buffer<an_ifc_expr_destructor_call_storage>;


/*
  |--------------------------------------------------|
  |            ExprDyad - 0.33 (26 bytes)            |
  |------------|--------------------|---------|------|
  | Name       | Type               | Version | Size |
  |------------|--------------------|---------|------|
  | locus      | SourceLocation     | 0.33    | 8    |
  | type       | TypeIndex          | 0.33    | 4    |
  | impl       | DeclIndex          | 0.33    | 4    |
  | argument_0 | ExprIndex          | 0.33    | 4    |
  | argument_1 | ExprIndex          | 0.33    | 4    |
  | assoc      | DyadicOperatorSort | 0.33    | 2    |
  |------------|--------------------|---------|------|

  |--------------------------------------------------|
  |            ExprDyad - 0.41 (26 bytes)            |
  |------------|--------------------|---------|------|
  | Name       | Type               | Version | Size |
  |------------|--------------------|---------|------|
  | locus      | SourceLocation     | 0.33    | 8    |
  | type       | TypeIndex          | 0.33    | 4    |
  | impl       | DeclIndex          | 0.41    | 4    |
  | argument_0 | ExprIndex          | 0.33    | 4    |
  | argument_1 | ExprIndex          | 0.33    | 4    |
  | assoc      | DyadicOperatorSort | 0.33    | 2    |
  |------------|--------------------|---------|------|
*/
enum an_ifc_expr_dyad_part : uint8_t {};
using an_ifc_expr_dyad_storage = an_ifc_expr_dyad_part[26];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_dyad_bytes = const an_ifc_expr_dyad_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_dyad_bytes = an_ifc_expr_dyad_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_dyad = an_ifc_Byte_buffer<an_ifc_expr_dyad_storage>;


/*
  |-----------------------------------------|
  |  ExprDynamicDispatch - 0.33 (16 bytes)  |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | type  | TypeIndex      | 0.33    | 4    |
  | pivot | ExprIndex      | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_dynamic_dispatch_part : uint8_t {};
using an_ifc_expr_dynamic_dispatch_storage =
                                         an_ifc_expr_dynamic_dispatch_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_dynamic_dispatch_bytes =
                                   const an_ifc_expr_dynamic_dispatch_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_dynamic_dispatch_bytes =
                                          an_ifc_expr_dynamic_dispatch_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_dynamic_dispatch =
                      an_ifc_Byte_buffer<an_ifc_expr_dynamic_dispatch_storage>;


/*
  |-----------------------------------------|
  |       ExprEmpty - 0.33 (12 bytes)       |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | type  | TypeIndex      | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_empty_part : uint8_t {};
using an_ifc_expr_empty_storage = an_ifc_expr_empty_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_empty_bytes = const an_ifc_expr_empty_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_empty_bytes = an_ifc_expr_empty_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_empty = an_ifc_Byte_buffer<an_ifc_expr_empty_storage>;


/*
  |-------------------------------------------|
  |      ExprExpansion - 0.33 (16 bytes)      |
  |---------|----------------|---------|------|
  | Name    | Type           | Version | Size |
  |---------|----------------|---------|------|
  | locus   | SourceLocation | 0.33    | 8    |
  | type    | TypeIndex      | 0.33    | 4    |
  | operand | ExprIndex      | 0.33    | 4    |
  |---------|----------------|---------|------|
*/
enum an_ifc_expr_expansion_part : uint8_t {};
using an_ifc_expr_expansion_storage = an_ifc_expr_expansion_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_expansion_bytes = const an_ifc_expr_expansion_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_expansion_bytes = an_ifc_expr_expansion_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_expansion =
                             an_ifc_Byte_buffer<an_ifc_expr_expansion_storage>;


/*
  |---------------------------------------------|
  |    ExprExpressionList - 0.33 (21 bytes)     |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | left      | SourceLocation | 0.33    | 8    |
  | right     | SourceLocation | 0.33    | 8    |
  | contents  | ExprIndex      | 0.33    | 4    |
  | delimiter | DelimiterSort  | 0.33    | 1    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_expr_expression_list_part : uint8_t {};
using an_ifc_expr_expression_list_storage =
                                          an_ifc_expr_expression_list_part[21];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_expression_list_bytes =
                                    const an_ifc_expr_expression_list_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_expression_list_bytes = an_ifc_expr_expression_list_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_expression_list =
                       an_ifc_Byte_buffer<an_ifc_expr_expression_list_storage>;


/*
  |-----------------------------------------|
  |  ExprFunctionString - 0.33 (16 bytes)   |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | type  | TypeIndex      | 0.33    | 4    |
  | macro | TextOffset     | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_function_string_part : uint8_t {};
using an_ifc_expr_function_string_storage =
                                          an_ifc_expr_function_string_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_function_string_bytes =
                                    const an_ifc_expr_function_string_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_function_string_bytes = an_ifc_expr_function_string_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_function_string =
                       an_ifc_Byte_buffer<an_ifc_expr_function_string_storage>;


/*
  |---------------------------------------------------|
  |     ExprHierarchyConversion - 0.33 (30 bytes)     |
  |-------------|--------------------|---------|------|
  | Name        | Type               | Version | Size |
  |-------------|--------------------|---------|------|
  | locus       | SourceLocation     | 0.33    | 8    |
  | type        | TypeIndex          | 0.33    | 4    |
  | source      | ExprIndex          | 0.33    | 4    |
  | target      | TypeIndex          | 0.33    | 4    |
  | inheritance | ExprIndex          | 0.33    | 4    |
  | override    | ExprIndex          | 0.33    | 4    |
  | op          | DyadicOperatorSort | 0.33    | 2    |
  |-------------|--------------------|---------|------|
*/
enum an_ifc_expr_hierarchy_conversion_part : uint8_t {};
using an_ifc_expr_hierarchy_conversion_storage =
                                     an_ifc_expr_hierarchy_conversion_part[30];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_hierarchy_conversion_bytes =
                               const an_ifc_expr_hierarchy_conversion_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_hierarchy_conversion_bytes =
                                      an_ifc_expr_hierarchy_conversion_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_hierarchy_conversion =
                  an_ifc_Byte_buffer<an_ifc_expr_hierarchy_conversion_storage>;


/*
  |-----------------------------------------|
  |  ExprInheritancePath - 0.33 (16 bytes)  |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | type  | TypeIndex      | 0.33    | 4    |
  | path  | ExprIndex      | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_inheritance_path_part : uint8_t {};
using an_ifc_expr_inheritance_path_storage =
                                         an_ifc_expr_inheritance_path_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_inheritance_path_bytes =
                                   const an_ifc_expr_inheritance_path_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_inheritance_path_bytes =
                                          an_ifc_expr_inheritance_path_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_inheritance_path =
                      an_ifc_Byte_buffer<an_ifc_expr_inheritance_path_storage>;


/*
  |------------------------------------------|
  |    ExprInitializer - 0.33 (17 bytes)     |
  |-------|-----------------|---------|------|
  | Name  | Type            | Version | Size |
  |-------|-----------------|---------|------|
  | locus | SourceLocation  | 0.33    | 8    |
  | type  | TypeIndex       | 0.33    | 4    |
  | expr  | ExprIndex       | 0.33    | 4    |
  | sort  | InitializerSort | 0.33    | 1    |
  |-------|-----------------|---------|------|
*/
enum an_ifc_expr_initializer_part : uint8_t {};
using an_ifc_expr_initializer_storage = an_ifc_expr_initializer_part[17];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_initializer_bytes = const an_ifc_expr_initializer_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_initializer_bytes = an_ifc_expr_initializer_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_initializer =
                           an_ifc_Byte_buffer<an_ifc_expr_initializer_storage>;


/*
  |--------------------------------------------|
  |   ExprInitializerList - 0.33 (16 bytes)    |
  |----------|----------------|---------|------|
  | Name     | Type           | Version | Size |
  |----------|----------------|---------|------|
  | locus    | SourceLocation | 0.33    | 8    |
  | type     | TypeIndex      | 0.33    | 4    |
  | elements | ExprIndex      | 0.33    | 4    |
  |----------|----------------|---------|------|
*/
enum an_ifc_expr_initializer_list_part : uint8_t {};
using an_ifc_expr_initializer_list_storage =
                                         an_ifc_expr_initializer_list_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_initializer_list_bytes =
                                   const an_ifc_expr_initializer_list_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_initializer_list_bytes =
                                          an_ifc_expr_initializer_list_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_initializer_list =
                      an_ifc_Byte_buffer<an_ifc_expr_initializer_list_storage>;


/*
  |----------------------------------------------------|
  |            ExprLambda - 0.33 (20 bytes)            |
  |---------------------|-------------|---------|------|
  | Name                | Type        | Version | Size |
  |---------------------|-------------|---------|------|
  | introducer          | SyntaxIndex | 0.33    | 4    |
  | template_parameters | SyntaxIndex | 0.33    | 4    |
  | declarator          | SyntaxIndex | 0.33    | 4    |
  | constraint          | SyntaxIndex | 0.33    | 4    |
  | body                | SyntaxIndex | 0.33    | 4    |
  |---------------------|-------------|---------|------|
*/
enum an_ifc_expr_lambda_part : uint8_t {};
using an_ifc_expr_lambda_storage = an_ifc_expr_lambda_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_lambda_bytes = const an_ifc_expr_lambda_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_lambda_bytes = an_ifc_expr_lambda_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_lambda = an_ifc_Byte_buffer<an_ifc_expr_lambda_storage>;


/*
  |-----------------------------------------|
  |      ExprLiteral - 0.33 (16 bytes)      |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | type  | TypeIndex      | 0.33    | 4    |
  | value | LitIndex       | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_literal_part : uint8_t {};
using an_ifc_expr_literal_storage = an_ifc_expr_literal_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_literal_bytes = const an_ifc_expr_literal_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_literal_bytes = an_ifc_expr_literal_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_literal = an_ifc_Byte_buffer<an_ifc_expr_literal_storage>;


/*
  |---------------------------------------------|
  |     ExprMemberAccess - 0.33 (24 bytes)      |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | locus     | SourceLocation | 0.33    | 8    |
  | type      | TypeIndex      | 0.33    | 4    |
  | offset    | ExprIndex      | 0.33    | 4    |
  | enclosing | TypeIndex      | 0.33    | 4    |
  | name      | TextOffset     | 0.33    | 4    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_expr_member_access_part : uint8_t {};
using an_ifc_expr_member_access_storage = an_ifc_expr_member_access_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_member_access_bytes =
                                      const an_ifc_expr_member_access_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_member_access_bytes = an_ifc_expr_member_access_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_member_access =
                         an_ifc_Byte_buffer<an_ifc_expr_member_access_storage>;


/*
  |-----------------------------------------------|
  |    ExprMemberInitializer - 0.33 (24 bytes)    |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | locus       | SourceLocation | 0.33    | 8    |
  | type        | TypeIndex      | 0.33    | 4    |
  | member      | DeclIndex      | 0.33    | 4    |
  | base        | TypeIndex      | 0.33    | 4    |
  | initializer | ExprIndex      | 0.33    | 4    |
  |-------------|----------------|---------|------|

  |-----------------------------------------------|
  |    ExprMemberInitializer - 0.41 (24 bytes)    |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | locus       | SourceLocation | 0.33    | 8    |
  | type        | TypeIndex      | 0.33    | 4    |
  | member      | DeclIndex      | 0.41    | 4    |
  | base        | TypeIndex      | 0.33    | 4    |
  | initializer | ExprIndex      | 0.33    | 4    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_expr_member_initializer_part : uint8_t {};
using an_ifc_expr_member_initializer_storage =
                                       an_ifc_expr_member_initializer_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_member_initializer_bytes =
                                 const an_ifc_expr_member_initializer_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_member_initializer_bytes =
                                        an_ifc_expr_member_initializer_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_member_initializer =
                    an_ifc_Byte_buffer<an_ifc_expr_member_initializer_storage>;


/*
  |-------------------------------------------------|
  |           ExprMonad - 0.33 (22 bytes)           |
  |----------|---------------------|---------|------|
  | Name     | Type                | Version | Size |
  |----------|---------------------|---------|------|
  | locus    | SourceLocation      | 0.33    | 8    |
  | type     | TypeIndex           | 0.33    | 4    |
  | impl     | DeclIndex           | 0.33    | 4    |
  | argument | ExprIndex           | 0.33    | 4    |
  | assoc    | MonadicOperatorSort | 0.33    | 2    |
  |----------|---------------------|---------|------|

  |-------------------------------------------------|
  |           ExprMonad - 0.41 (22 bytes)           |
  |----------|---------------------|---------|------|
  | Name     | Type                | Version | Size |
  |----------|---------------------|---------|------|
  | locus    | SourceLocation      | 0.33    | 8    |
  | type     | TypeIndex           | 0.33    | 4    |
  | impl     | DeclIndex           | 0.41    | 4    |
  | argument | ExprIndex           | 0.33    | 4    |
  | assoc    | MonadicOperatorSort | 0.33    | 2    |
  |----------|---------------------|---------|------|
*/
enum an_ifc_expr_monad_part : uint8_t {};
using an_ifc_expr_monad_storage = an_ifc_expr_monad_part[22];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_monad_bytes = const an_ifc_expr_monad_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_monad_bytes = an_ifc_expr_monad_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_monad = an_ifc_Byte_buffer<an_ifc_expr_monad_storage>;


/*
  |----------------------------------------------|
  |       ExprNamedDecl - 0.33 (16 bytes)        |
  |------------|----------------|---------|------|
  | Name       | Type           | Version | Size |
  |------------|----------------|---------|------|
  | locus      | SourceLocation | 0.33    | 8    |
  | type       | TypeIndex      | 0.33    | 4    |
  | resolution | DeclIndex      | 0.33    | 4    |
  |------------|----------------|---------|------|

  |----------------------------------------------|
  |       ExprNamedDecl - 0.41 (16 bytes)        |
  |------------|----------------|---------|------|
  | Name       | Type           | Version | Size |
  |------------|----------------|---------|------|
  | locus      | SourceLocation | 0.33    | 8    |
  | type       | TypeIndex      | 0.33    | 4    |
  | resolution | DeclIndex      | 0.41    | 4    |
  |------------|----------------|---------|------|
*/
enum an_ifc_expr_named_decl_part : uint8_t {};
using an_ifc_expr_named_decl_storage = an_ifc_expr_named_decl_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_named_decl_bytes = const an_ifc_expr_named_decl_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_named_decl_bytes = an_ifc_expr_named_decl_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_named_decl =
                            an_ifc_Byte_buffer<an_ifc_expr_named_decl_storage>;


/*
  |-----------------------------------------|
  |      ExprNullptr - 0.33 (12 bytes)      |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | type  | TypeIndex      | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_nullptr_part : uint8_t {};
using an_ifc_expr_nullptr_storage = an_ifc_expr_nullptr_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_nullptr_bytes = const an_ifc_expr_nullptr_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_nullptr_bytes = an_ifc_expr_nullptr_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_nullptr = an_ifc_Byte_buffer<an_ifc_expr_nullptr_storage>;


/*
  |-----------------------------------------------|
  | ExprPackedTemplateArguments - 0.33 (16 bytes) |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | locus       | SourceLocation | 0.33    | 8    |
  | type        | TypeIndex      | 0.33    | 4    |
  | arguments   | ExprIndex      | 0.33    | 4    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_expr_packed_template_arguments_part : uint8_t {};
using an_ifc_expr_packed_template_arguments_storage =
                                an_ifc_expr_packed_template_arguments_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_packed_template_arguments_bytes =
                          const an_ifc_expr_packed_template_arguments_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_packed_template_arguments_bytes =
                                 an_ifc_expr_packed_template_arguments_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_packed_template_arguments =
             an_ifc_Byte_buffer<an_ifc_expr_packed_template_arguments_storage>;


/*
  |------------------------------------------|
  |        ExprPath - 0.33 (20 bytes)        |
  |--------|----------------|---------|------|
  | Name   | Type           | Version | Size |
  |--------|----------------|---------|------|
  | locus  | SourceLocation | 0.33    | 8    |
  | type   | TypeIndex      | 0.33    | 4    |
  | scope  | ExprIndex      | 0.33    | 4    |
  | member | ExprIndex      | 0.33    | 4    |
  |--------|----------------|---------|------|
*/
enum an_ifc_expr_path_part : uint8_t {};
using an_ifc_expr_path_storage = an_ifc_expr_path_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_path_bytes = const an_ifc_expr_path_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_path_bytes = an_ifc_expr_path_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_path = an_ifc_Byte_buffer<an_ifc_expr_path_storage>;


/*
  |-----------------------------------------|
  |    ExprPlaceholder - 0.33 (12 bytes)    |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | type  | TypeIndex      | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_placeholder_part : uint8_t {};
using an_ifc_expr_placeholder_storage = an_ifc_expr_placeholder_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_placeholder_bytes = const an_ifc_expr_placeholder_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_placeholder_bytes = an_ifc_expr_placeholder_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_placeholder =
                           an_ifc_Byte_buffer<an_ifc_expr_placeholder_storage>;


/*
  |-----------------------------------------|
  |      ExprPointer - 0.33 (8 bytes)       |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_pointer_part : uint8_t {};
using an_ifc_expr_pointer_storage = an_ifc_expr_pointer_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_pointer_bytes = const an_ifc_expr_pointer_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_pointer_bytes = an_ifc_expr_pointer_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_pointer = an_ifc_Byte_buffer<an_ifc_expr_pointer_storage>;


/*
  |---------------------------------------------------|
  |      ExprProductTypeValue - 0.33 (24 bytes)       |
  |-----------------|----------------|---------|------|
  | Name            | Type           | Version | Size |
  |-----------------|----------------|---------|------|
  | locus           | SourceLocation | 0.33    | 8    |
  | type            | TypeIndex      | 0.33    | 4    |
  | class_decl      | TypeIndex      | 0.33    | 4    |
  | members         | ExprIndex      | 0.33    | 4    |
  | base_subobjects | ExprIndex      | 0.33    | 4    |
  |-----------------|----------------|---------|------|
*/
enum an_ifc_expr_product_type_value_part : uint8_t {};
using an_ifc_expr_product_type_value_storage =
                                       an_ifc_expr_product_type_value_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_product_type_value_bytes =
                                 const an_ifc_expr_product_type_value_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_product_type_value_bytes =
                                        an_ifc_expr_product_type_value_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_product_type_value =
                    an_ifc_Byte_buffer<an_ifc_expr_product_type_value_storage>;


/*
  |---------------------------------------------|
  |       ExprPushState - 0.33 (22 bytes)       |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | locus     | SourceLocation | 0.33    | 8    |
  | type      | TypeIndex      | 0.33    | 4    |
  | ctor_call | ExprIndex      | 0.33    | 4    |
  | dtor_call | ExprIndex      | 0.33    | 4    |
  | flags     | EHFlags        | 0.33    | 2    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_expr_push_state_part : uint8_t {};
using an_ifc_expr_push_state_storage = an_ifc_expr_push_state_part[22];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_push_state_bytes = const an_ifc_expr_push_state_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_push_state_bytes = an_ifc_expr_push_state_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_push_state =
                            an_ifc_Byte_buffer<an_ifc_expr_push_state_storage>;


/*
  |----------------------------------------------------|
  |        ExprQualifiedName - 0.33 (24 bytes)         |
  |------------------|----------------|---------|------|
  | Name             | Type           | Version | Size |
  |------------------|----------------|---------|------|
  | locus            | SourceLocation | 0.33    | 8    |
  | type             | TypeIndex      | 0.33    | 4    |
  | elements         | ExprIndex      | 0.33    | 4    |
  | typename_keyword | SourceLocation | 0.33    | 8    |
  |------------------|----------------|---------|------|
*/
enum an_ifc_expr_qualified_name_part : uint8_t {};
using an_ifc_expr_qualified_name_storage = an_ifc_expr_qualified_name_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_qualified_name_bytes =
                                     const an_ifc_expr_qualified_name_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_qualified_name_bytes = an_ifc_expr_qualified_name_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_qualified_name =
                        an_ifc_Byte_buffer<an_ifc_expr_qualified_name_storage>;


/*
  |-----------------------------------------------|
  |          ExprRead - 0.33 (17 bytes)           |
  |---------|--------------------|---------|------|
  | Name    | Type               | Version | Size |
  |---------|--------------------|---------|------|
  | locus   | SourceLocation     | 0.33    | 8    |
  | type    | TypeIndex          | 0.33    | 4    |
  | address | ExprIndex          | 0.33    | 4    |
  | sort    | ReadConversionSort | 0.33    | 1    |
  |---------|--------------------|---------|------|
*/
enum an_ifc_expr_read_part : uint8_t {};
using an_ifc_expr_read_storage = an_ifc_expr_read_part[17];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_read_bytes = const an_ifc_expr_read_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_read_bytes = an_ifc_expr_read_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_read = an_ifc_Byte_buffer<an_ifc_expr_read_storage>;


/*
  |----------------------------------------------|
  |        ExprRequires - 0.33 (20 bytes)        |
  |------------|----------------|---------|------|
  | Name       | Type           | Version | Size |
  |------------|----------------|---------|------|
  | locus      | SourceLocation | 0.33    | 8    |
  | type       | TypeIndex      | 0.33    | 4    |
  | parameters | SyntaxIndex    | 0.33    | 4    |
  | body       | SyntaxIndex    | 0.33    | 4    |
  |------------|----------------|---------|------|
*/
enum an_ifc_expr_requires_part : uint8_t {};
using an_ifc_expr_requires_storage = an_ifc_expr_requires_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_requires_bytes = const an_ifc_expr_requires_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_requires_bytes = an_ifc_expr_requires_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_requires = an_ifc_Byte_buffer<an_ifc_expr_requires_storage>;


/*
  |-----------------------------------------|
  | ExprSimpleIdentifier - 0.33 (16 bytes)  |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | type  | TypeIndex      | 0.33    | 4    |
  | name  | NameIndex      | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_simple_identifier_part : uint8_t {};
using an_ifc_expr_simple_identifier_storage =
                                        an_ifc_expr_simple_identifier_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_simple_identifier_bytes =
                                  const an_ifc_expr_simple_identifier_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_simple_identifier_bytes =
                                         an_ifc_expr_simple_identifier_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_simple_identifier =
                     an_ifc_Byte_buffer<an_ifc_expr_simple_identifier_storage>;


/*
  |-------------------------------------------|
  |     ExprSizeofType - 0.33 (16 bytes)      |
  |---------|----------------|---------|------|
  | Name    | Type           | Version | Size |
  |---------|----------------|---------|------|
  | locus   | SourceLocation | 0.33    | 8    |
  | type    | TypeIndex      | 0.33    | 4    |
  | operand | TypeIndex      | 0.33    | 4    |
  |---------|----------------|---------|------|
*/
enum an_ifc_expr_sizeof_type_part : uint8_t {};
using an_ifc_expr_sizeof_type_storage = an_ifc_expr_sizeof_type_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_sizeof_type_bytes = const an_ifc_expr_sizeof_type_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_sizeof_type_bytes = an_ifc_expr_sizeof_type_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_sizeof_type =
                           an_ifc_Byte_buffer<an_ifc_expr_sizeof_type_storage>;


/*
  |------------------------------------------------|
  |          ExprString - 0.33 (16 bytes)          |
  |--------------|----------------|---------|------|
  | Name         | Type           | Version | Size |
  |--------------|----------------|---------|------|
  | locus        | SourceLocation | 0.33    | 8    |
  | type         | TypeIndex      | 0.33    | 4    |
  | string_index | StringIndex    | 0.33    | 4    |
  |--------------|----------------|---------|------|
*/
enum an_ifc_expr_string_part : uint8_t {};
using an_ifc_expr_string_storage = an_ifc_expr_string_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_string_bytes = const an_ifc_expr_string_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_string_bytes = an_ifc_expr_string_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_string = an_ifc_Byte_buffer<an_ifc_expr_string_storage>;


/*
  |-------------------------------------------|
  |   ExprStringSequence - 0.33 (16 bytes)    |
  |---------|----------------|---------|------|
  | Name    | Type           | Version | Size |
  |---------|----------------|---------|------|
  | locus   | SourceLocation | 0.33    | 8    |
  | type    | TypeIndex      | 0.33    | 4    |
  | strings | ExprIndex      | 0.33    | 4    |
  |---------|----------------|---------|------|
*/
enum an_ifc_expr_string_sequence_part : uint8_t {};
using an_ifc_expr_string_sequence_storage =
                                          an_ifc_expr_string_sequence_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_string_sequence_bytes =
                                    const an_ifc_expr_string_sequence_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_string_sequence_bytes = an_ifc_expr_string_sequence_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_string_sequence =
                       an_ifc_Byte_buffer<an_ifc_expr_string_sequence_storage>;


/*
  |-------------------------------------|
  | ExprSubobjectValue - 0.33 (4 bytes) |
  |--------|-----------|---------|------|
  | Name   | Type      | Version | Size |
  |--------|-----------|---------|------|
  | value  | ExprIndex | 0.33    | 4    |
  |--------|-----------|---------|------|
*/
enum an_ifc_expr_subobject_value_part : uint8_t {};
using an_ifc_expr_subobject_value_storage =
                                           an_ifc_expr_subobject_value_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_subobject_value_bytes =
                                    const an_ifc_expr_subobject_value_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_subobject_value_bytes = an_ifc_expr_subobject_value_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_subobject_value =
                       an_ifc_Byte_buffer<an_ifc_expr_subobject_value_storage>;


/*
  |------------------------------------------------|
  |       ExprSumTypeValue - 0.33 (24 bytes)       |
  |--------------|----------------|---------|------|
  | Name         | Type           | Version | Size |
  |--------------|----------------|---------|------|
  | locus        | SourceLocation | 0.33    | 8    |
  | type         | TypeIndex      | 0.33    | 4    |
  | variant      | DeclIndex      | 0.33    | 4    |
  | discriminant | ActiveMember   | 0.33    | 4    |
  | value        | ExprIndex      | 0.33    | 4    |
  |--------------|----------------|---------|------|

  |------------------------------------------------|
  |       ExprSumTypeValue - 0.41 (24 bytes)       |
  |--------------|----------------|---------|------|
  | Name         | Type           | Version | Size |
  |--------------|----------------|---------|------|
  | locus        | SourceLocation | 0.33    | 8    |
  | type         | TypeIndex      | 0.33    | 4    |
  | variant      | DeclIndex      | 0.41    | 4    |
  | discriminant | ActiveMember   | 0.33    | 4    |
  | value        | ExprIndex      | 0.33    | 4    |
  |--------------|----------------|---------|------|
*/
enum an_ifc_expr_sum_type_value_part : uint8_t {};
using an_ifc_expr_sum_type_value_storage = an_ifc_expr_sum_type_value_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_sum_type_value_bytes =
                                     const an_ifc_expr_sum_type_value_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_sum_type_value_bytes = an_ifc_expr_sum_type_value_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_sum_type_value =
                        an_ifc_Byte_buffer<an_ifc_expr_sum_type_value_storage>;


/*
  |---------------------------------------|
  |    ExprSyntaxTree - 0.33 (4 bytes)    |
  |--------|-------------|---------|------|
  | Name   | Type        | Version | Size |
  |--------|-------------|---------|------|
  | syntax | SyntaxIndex | 0.33    | 4    |
  |--------|-------------|---------|------|
*/
enum an_ifc_expr_syntax_tree_part : uint8_t {};
using an_ifc_expr_syntax_tree_storage = an_ifc_expr_syntax_tree_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_syntax_tree_bytes = const an_ifc_expr_syntax_tree_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_syntax_tree_bytes = an_ifc_expr_syntax_tree_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_syntax_tree =
                           an_ifc_Byte_buffer<an_ifc_expr_syntax_tree_storage>;


/*
  |---------------------------------------------|
  |      ExprTemplateId - 0.33 (20 bytes)       |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | locus     | SourceLocation | 0.33    | 8    |
  | type      | TypeIndex      | 0.33    | 4    |
  | primary   | ExprIndex      | 0.33    | 4    |
  | arguments | ExprIndex      | 0.33    | 4    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_expr_template_id_part : uint8_t {};
using an_ifc_expr_template_id_storage = an_ifc_expr_template_id_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_template_id_bytes = const an_ifc_expr_template_id_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_template_id_bytes = an_ifc_expr_template_id_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_template_id =
                           an_ifc_Byte_buffer<an_ifc_expr_template_id_storage>;


/*
  |------------------------------------------------|
  |    ExprTemplateReference - 0.33 (32 bytes)     |
  |--------------|----------------|---------|------|
  | Name         | Type           | Version | Size |
  |--------------|----------------|---------|------|
  | locus        | SourceLocation | 0.33    | 8    |
  | type         | TypeIndex      | 0.33    | 4    |
  | member_name  | NameIndex      | 0.33    | 4    |
  | member_locus | SourceLocation | 0.33    | 8    |
  | scope        | TypeIndex      | 0.33    | 4    |
  | arguments    | ExprIndex      | 0.33    | 4    |
  |--------------|----------------|---------|------|
*/
enum an_ifc_expr_template_reference_part : uint8_t {};
using an_ifc_expr_template_reference_storage =
                                       an_ifc_expr_template_reference_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_template_reference_bytes =
                                 const an_ifc_expr_template_reference_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_template_reference_bytes =
                                        an_ifc_expr_template_reference_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_template_reference =
                    an_ifc_Byte_buffer<an_ifc_expr_template_reference_storage>;


/*
  |-----------------------------------------|
  |     ExprTemporary - 0.33 (16 bytes)     |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | type  | TypeIndex      | 0.33    | 4    |
  | id    | UniqueID       | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_temporary_part : uint8_t {};
using an_ifc_expr_temporary_storage = an_ifc_expr_temporary_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_temporary_bytes = const an_ifc_expr_temporary_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_temporary_bytes = an_ifc_expr_temporary_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_temporary =
                             an_ifc_Byte_buffer<an_ifc_expr_temporary_storage>;


/*
  |-----------------------------------------|
  |       ExprThis - 0.33 (12 bytes)        |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | type  | TypeIndex      | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_this_part : uint8_t {};
using an_ifc_expr_this_storage = an_ifc_expr_this_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_this_bytes = const an_ifc_expr_this_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_this_bytes = an_ifc_expr_this_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_this = an_ifc_Byte_buffer<an_ifc_expr_this_storage>;


/*
  |-----------------------------------------|
  |      ExprTokens - 0.33 (16 bytes)       |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | type  | TypeIndex      | 0.33    | 4    |
  | words | SentenceIndex  | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_tokens_part : uint8_t {};
using an_ifc_expr_tokens_storage = an_ifc_expr_tokens_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_tokens_bytes = const an_ifc_expr_tokens_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_tokens_bytes = an_ifc_expr_tokens_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_tokens = an_ifc_Byte_buffer<an_ifc_expr_tokens_storage>;


/*
  |---------------------------------------------------|
  |            ExprTriad - 0.33 (30 bytes)            |
  |------------|---------------------|---------|------|
  | Name       | Type                | Version | Size |
  |------------|---------------------|---------|------|
  | locus      | SourceLocation      | 0.33    | 8    |
  | type       | TypeIndex           | 0.33    | 4    |
  | impl       | DeclIndex           | 0.33    | 4    |
  | argument_0 | ExprIndex           | 0.33    | 4    |
  | argument_1 | ExprIndex           | 0.33    | 4    |
  | argument_2 | ExprIndex           | 0.33    | 4    |
  | assoc      | TriadicOperatorSort | 0.33    | 2    |
  |------------|---------------------|---------|------|

  |---------------------------------------------------|
  |            ExprTriad - 0.41 (30 bytes)            |
  |------------|---------------------|---------|------|
  | Name       | Type                | Version | Size |
  |------------|---------------------|---------|------|
  | locus      | SourceLocation      | 0.33    | 8    |
  | type       | TypeIndex           | 0.33    | 4    |
  | impl       | DeclIndex           | 0.41    | 4    |
  | argument_0 | ExprIndex           | 0.33    | 4    |
  | argument_1 | ExprIndex           | 0.33    | 4    |
  | argument_2 | ExprIndex           | 0.33    | 4    |
  | assoc      | TriadicOperatorSort | 0.33    | 2    |
  |------------|---------------------|---------|------|
*/
enum an_ifc_expr_triad_part : uint8_t {};
using an_ifc_expr_triad_storage = an_ifc_expr_triad_part[30];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_triad_bytes = const an_ifc_expr_triad_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_triad_bytes = an_ifc_expr_triad_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_triad = an_ifc_Byte_buffer<an_ifc_expr_triad_storage>;


/*
  |-----------------------------------------------|
  |          ExprTuple - 0.33 (20 bytes)          |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | locus       | SourceLocation | 0.33    | 8    |
  | type        | TypeIndex      | 0.33    | 4    |
  | start       | Index          | 0.33    | 4    |
  | cardinality | Cardinality    | 0.33    | 4    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_expr_tuple_part : uint8_t {};
using an_ifc_expr_tuple_storage = an_ifc_expr_tuple_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_tuple_bytes = const an_ifc_expr_tuple_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_tuple_bytes = an_ifc_expr_tuple_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_tuple = an_ifc_Byte_buffer<an_ifc_expr_tuple_storage>;


/*
  |----------------------------------------------|
  |          ExprType - 0.33 (16 bytes)          |
  |------------|----------------|---------|------|
  | Name       | Type           | Version | Size |
  |------------|----------------|---------|------|
  | locus      | SourceLocation | 0.33    | 8    |
  | type       | TypeIndex      | 0.33    | 4    |
  | denotation | TypeIndex      | 0.33    | 4    |
  |------------|----------------|---------|------|
*/
enum an_ifc_expr_type_part : uint8_t {};
using an_ifc_expr_type_storage = an_ifc_expr_type_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_type_bytes = const an_ifc_expr_type_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_type_bytes = an_ifc_expr_type_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_type = an_ifc_Byte_buffer<an_ifc_expr_type_storage>;


/*
  |-----------------------------------------------|
  |   ExprTypeTraitIntrinsic - 0.33 (18 bytes)    |
  |-----------|------------------|---------|------|
  | Name      | Type             | Version | Size |
  |-----------|------------------|---------|------|
  | locus     | SourceLocation   | 0.33    | 8    |
  | type      | TypeIndex        | 0.33    | 4    |
  | arguments | TypeIndex        | 0.33    | 4    |
  | intrinsic | OperatorCategory | 0.33    | 2    |
  |-----------|------------------|---------|------|
*/
enum an_ifc_expr_type_trait_intrinsic_part : uint8_t {};
using an_ifc_expr_type_trait_intrinsic_storage =
                                     an_ifc_expr_type_trait_intrinsic_part[18];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_type_trait_intrinsic_bytes =
                               const an_ifc_expr_type_trait_intrinsic_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_type_trait_intrinsic_bytes =
                                      an_ifc_expr_type_trait_intrinsic_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_type_trait_intrinsic =
                  an_ifc_Byte_buffer<an_ifc_expr_type_trait_intrinsic_storage>;


/*
  |-------------------------------------------|
  |       ExprTypeid - 0.33 (16 bytes)        |
  |---------|----------------|---------|------|
  | Name    | Type           | Version | Size |
  |---------|----------------|---------|------|
  | locus   | SourceLocation | 0.33    | 8    |
  | type    | TypeIndex      | 0.33    | 4    |
  | operand | TypeIndex      | 0.33    | 4    |
  |---------|----------------|---------|------|
*/
enum an_ifc_expr_typeid_part : uint8_t {};
using an_ifc_expr_typeid_storage = an_ifc_expr_typeid_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_typeid_bytes = const an_ifc_expr_typeid_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_typeid_bytes = an_ifc_expr_typeid_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_typeid = an_ifc_Byte_buffer<an_ifc_expr_typeid_storage>;


/*
  |-----------------------------------------------------|
  |           ExprUnaryFold - 0.33 (19 bytes)           |
  |---------------|--------------------|---------|------|
  | Name          | Type               | Version | Size |
  |---------------|--------------------|---------|------|
  | locus         | SourceLocation     | 0.33    | 8    |
  | type          | TypeIndex          | 0.33    | 4    |
  | expr          | ExprIndex          | 0.33    | 4    |
  | operation     | DyadicOperatorSort | 0.33    | 2    |
  | associativity | Associativity      | 0.33    | 1    |
  |---------------|--------------------|---------|------|
*/
enum an_ifc_expr_unary_fold_part : uint8_t {};
using an_ifc_expr_unary_fold_storage = an_ifc_expr_unary_fold_part[19];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_unary_fold_bytes = const an_ifc_expr_unary_fold_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_unary_fold_bytes = an_ifc_expr_unary_fold_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_unary_fold =
                            an_ifc_Byte_buffer<an_ifc_expr_unary_fold_storage>;


/*
  |----------------------------------------------------|
  |        ExprUnqualifiedId - 0.33 (28 bytes)         |
  |------------------|----------------|---------|------|
  | Name             | Type           | Version | Size |
  |------------------|----------------|---------|------|
  | locus            | SourceLocation | 0.33    | 8    |
  | type             | TypeIndex      | 0.33    | 4    |
  | name             | NameIndex      | 0.33    | 4    |
  | resolution       | ExprIndex      | 0.33    | 4    |
  | template_keyword | SourceLocation | 0.33    | 8    |
  |------------------|----------------|---------|------|
*/
enum an_ifc_expr_unqualified_id_part : uint8_t {};
using an_ifc_expr_unqualified_id_storage = an_ifc_expr_unqualified_id_part[28];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_unqualified_id_bytes =
                                     const an_ifc_expr_unqualified_id_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_unqualified_id_bytes = an_ifc_expr_unqualified_id_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_unqualified_id =
                        an_ifc_Byte_buffer<an_ifc_expr_unqualified_id_storage>;


/*
  |-----------------------------------------|
  |   ExprUnresolvedId - 0.33 (16 bytes)    |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | type  | TypeIndex      | 0.33    | 4    |
  | name  | NameIndex      | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_expr_unresolved_id_part : uint8_t {};
using an_ifc_expr_unresolved_id_storage = an_ifc_expr_unresolved_id_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_unresolved_id_bytes =
                                      const an_ifc_expr_unresolved_id_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_unresolved_id_bytes = an_ifc_expr_unresolved_id_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_unresolved_id =
                         an_ifc_Byte_buffer<an_ifc_expr_unresolved_id_storage>;


/*
  |-------------------------------------------------|
  | ExprVirtualFunctionConversion - 0.33 (16 bytes) |
  |------------|-----------------|----------|-------|
  | Name       | Type            | Version  | Size  |
  |------------|-----------------|----------|-------|
  | locus      | SourceLocation  | 0.33     | 8     |
  | type       | TypeIndex       | 0.33     | 4     |
  | function   | DeclIndex       | 0.33     | 4     |
  |------------|-----------------|----------|-------|

  |-------------------------------------------------|
  | ExprVirtualFunctionConversion - 0.41 (16 bytes) |
  |------------|-----------------|----------|-------|
  | Name       | Type            | Version  | Size  |
  |------------|-----------------|----------|-------|
  | locus      | SourceLocation  | 0.33     | 8     |
  | type       | TypeIndex       | 0.33     | 4     |
  | function   | DeclIndex       | 0.41     | 4     |
  |------------|-----------------|----------|-------|
*/
enum an_ifc_expr_virtual_function_conversion_part : uint8_t {};
using an_ifc_expr_virtual_function_conversion_storage =
                              an_ifc_expr_virtual_function_conversion_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_expr_virtual_function_conversion_bytes =
                        const an_ifc_expr_virtual_function_conversion_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_expr_virtual_function_conversion_bytes =
                               an_ifc_expr_virtual_function_conversion_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_expr_virtual_function_conversion =
           an_ifc_Byte_buffer<an_ifc_expr_virtual_function_conversion_storage>;


/*
  |------------------------------------------|
  |      FormCatenate - 0.33 (16 bytes)      |
  |--------|----------------|---------|------|
  | Name   | Type           | Version | Size |
  |--------|----------------|---------|------|
  | locus  | SourceLocation | 0.33    | 8    |
  | first  | FormIndex      | 0.33    | 4    |
  | second | FormIndex      | 0.33    | 4    |
  |--------|----------------|---------|------|
*/
enum an_ifc_form_catenate_part : uint8_t {};
using an_ifc_form_catenate_storage = an_ifc_form_catenate_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_catenate_bytes = const an_ifc_form_catenate_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_catenate_bytes = an_ifc_form_catenate_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_catenate = an_ifc_Byte_buffer<an_ifc_form_catenate_storage>;


/*
  |--------------------------------------------|
  |      FormCharacter - 0.33 (12 bytes)       |
  |----------|----------------|---------|------|
  | Name     | Type           | Version | Size |
  |----------|----------------|---------|------|
  | locus    | SourceLocation | 0.33    | 8    |
  | spelling | TextOffset     | 0.33    | 4    |
  |----------|----------------|---------|------|
*/
enum an_ifc_form_character_part : uint8_t {};
using an_ifc_form_character_storage = an_ifc_form_character_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_character_bytes = const an_ifc_form_character_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_character_bytes = an_ifc_form_character_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_character =
                             an_ifc_Byte_buffer<an_ifc_form_character_storage>;


/*
  |--------------------------------------------|
  |        FormHeader - 0.33 (12 bytes)        |
  |----------|----------------|---------|------|
  | Name     | Type           | Version | Size |
  |----------|----------------|---------|------|
  | locus    | SourceLocation | 0.33    | 8    |
  | spelling | TextOffset     | 0.33    | 4    |
  |----------|----------------|---------|------|
*/
enum an_ifc_form_header_part : uint8_t {};
using an_ifc_form_header_storage = an_ifc_form_header_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_header_bytes = const an_ifc_form_header_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_header_bytes = an_ifc_form_header_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_header = an_ifc_Byte_buffer<an_ifc_form_header_storage>;


/*
  |--------------------------------------------|
  |      FormIdentifier - 0.33 (12 bytes)      |
  |----------|----------------|---------|------|
  | Name     | Type           | Version | Size |
  |----------|----------------|---------|------|
  | locus    | SourceLocation | 0.33    | 8    |
  | spelling | TextOffset     | 0.33    | 4    |
  |----------|----------------|---------|------|
*/
enum an_ifc_form_identifier_part : uint8_t {};
using an_ifc_form_identifier_storage = an_ifc_form_identifier_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_identifier_bytes = const an_ifc_form_identifier_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_identifier_bytes = an_ifc_form_identifier_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_identifier =
                            an_ifc_Byte_buffer<an_ifc_form_identifier_storage>;


/*
  |--------------------------------------------|
  |         FormJunk - 0.33 (12 bytes)         |
  |----------|----------------|---------|------|
  | Name     | Type           | Version | Size |
  |----------|----------------|---------|------|
  | locus    | SourceLocation | 0.33    | 8    |
  | spelling | TextOffset     | 0.33    | 4    |
  |----------|----------------|---------|------|
*/
enum an_ifc_form_junk_part : uint8_t {};
using an_ifc_form_junk_storage = an_ifc_form_junk_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_junk_bytes = const an_ifc_form_junk_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_junk_bytes = an_ifc_form_junk_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_junk = an_ifc_Byte_buffer<an_ifc_form_junk_storage>;


/*
  |--------------------------------------------|
  |       FormKeyword - 0.33 (12 bytes)        |
  |----------|----------------|---------|------|
  | Name     | Type           | Version | Size |
  |----------|----------------|---------|------|
  | locus    | SourceLocation | 0.33    | 8    |
  | spelling | TextOffset     | 0.33    | 4    |
  |----------|----------------|---------|------|
*/
enum an_ifc_form_keyword_part : uint8_t {};
using an_ifc_form_keyword_storage = an_ifc_form_keyword_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_keyword_bytes = const an_ifc_form_keyword_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_keyword_bytes = an_ifc_form_keyword_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_keyword = an_ifc_Byte_buffer<an_ifc_form_keyword_storage>;


/*
  |--------------------------------------------|
  |        FormNumber - 0.33 (12 bytes)        |
  |----------|----------------|---------|------|
  | Name     | Type           | Version | Size |
  |----------|----------------|---------|------|
  | locus    | SourceLocation | 0.33    | 8    |
  | spelling | TextOffset     | 0.33    | 4    |
  |----------|----------------|---------|------|
*/
enum an_ifc_form_number_part : uint8_t {};
using an_ifc_form_number_storage = an_ifc_form_number_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_number_bytes = const an_ifc_form_number_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_number_bytes = an_ifc_form_number_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_number = an_ifc_Byte_buffer<an_ifc_form_number_storage>;


/*
  |----------------------------------------------|
  |        FormOperator - 0.33 (14 bytes)        |
  |----------|------------------|---------|------|
  | Name     | Type             | Version | Size |
  |----------|------------------|---------|------|
  | locus    | SourceLocation   | 0.33    | 8    |
  | spelling | TextOffset       | 0.33    | 4    |
  | op       | FormOperatorSort | 0.33    | 2    |
  |----------|------------------|---------|------|
*/
enum an_ifc_form_operator_part : uint8_t {};
using an_ifc_form_operator_storage = an_ifc_form_operator_part[14];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_operator_bytes = const an_ifc_form_operator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_operator_bytes = an_ifc_form_operator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_operator = an_ifc_Byte_buffer<an_ifc_form_operator_storage>;


/*
  |--------------------------------------------|
  |      FormParameter - 0.33 (12 bytes)       |
  |----------|----------------|---------|------|
  | Name     | Type           | Version | Size |
  |----------|----------------|---------|------|
  | locus    | SourceLocation | 0.33    | 8    |
  | spelling | TextOffset     | 0.33    | 4    |
  |----------|----------------|---------|------|
*/
enum an_ifc_form_parameter_part : uint8_t {};
using an_ifc_form_parameter_storage = an_ifc_form_parameter_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_parameter_bytes = const an_ifc_form_parameter_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_parameter_bytes = an_ifc_form_parameter_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_parameter =
                             an_ifc_Byte_buffer<an_ifc_form_parameter_storage>;


/*
  |-------------------------------------------|
  |    FormParenthesized - 0.33 (12 bytes)    |
  |---------|----------------|---------|------|
  | Name    | Type           | Version | Size |
  |---------|----------------|---------|------|
  | locus   | SourceLocation | 0.33    | 8    |
  | operand | FormIndex      | 0.33    | 4    |
  |---------|----------------|---------|------|
*/
enum an_ifc_form_parenthesized_part : uint8_t {};
using an_ifc_form_parenthesized_storage = an_ifc_form_parenthesized_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_parenthesized_bytes =
                                      const an_ifc_form_parenthesized_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_parenthesized_bytes = an_ifc_form_parenthesized_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_parenthesized =
                         an_ifc_Byte_buffer<an_ifc_form_parenthesized_storage>;


/*
  |-------------------------------------------|
  |       FormPragma - 0.33 (12 bytes)        |
  |---------|----------------|---------|------|
  | Name    | Type           | Version | Size |
  |---------|----------------|---------|------|
  | locus   | SourceLocation | 0.33    | 8    |
  | operand | FormIndex      | 0.33    | 4    |
  |---------|----------------|---------|------|
*/
enum an_ifc_form_pragma_part : uint8_t {};
using an_ifc_form_pragma_storage = an_ifc_form_pragma_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_pragma_bytes = const an_ifc_form_pragma_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_pragma_bytes = an_ifc_form_pragma_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_pragma = an_ifc_Byte_buffer<an_ifc_form_pragma_storage>;


/*
  |-----------------------------------------------|
  |           FormSpec - 0.33 (8 bytes)           |
  |------------------|-----------|---------|------|
  | Name             | Type      | Version | Size |
  |------------------|-----------|---------|------|
  | primary_template | DeclIndex | 0.33    | 4    |
  | arguments        | ExprIndex | 0.33    | 4    |
  |------------------|-----------|---------|------|

  |-----------------------------------------------|
  |           FormSpec - 0.41 (8 bytes)           |
  |------------------|-----------|---------|------|
  | Name             | Type      | Version | Size |
  |------------------|-----------|---------|------|
  | primary_template | DeclIndex | 0.41    | 4    |
  | arguments        | ExprIndex | 0.33    | 4    |
  |------------------|-----------|---------|------|
*/
enum an_ifc_form_spec_part : uint8_t {};
using an_ifc_form_spec_storage = an_ifc_form_spec_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_spec_bytes = const an_ifc_form_spec_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_spec_bytes = an_ifc_form_spec_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_spec = an_ifc_Byte_buffer<an_ifc_form_spec_storage>;


/*
  |--------------------------------------------|
  |        FormString - 0.33 (12 bytes)        |
  |----------|----------------|---------|------|
  | Name     | Type           | Version | Size |
  |----------|----------------|---------|------|
  | locus    | SourceLocation | 0.33    | 8    |
  | spelling | TextOffset     | 0.33    | 4    |
  |----------|----------------|---------|------|
*/
enum an_ifc_form_string_part : uint8_t {};
using an_ifc_form_string_storage = an_ifc_form_string_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_string_bytes = const an_ifc_form_string_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_string_bytes = an_ifc_form_string_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_string = an_ifc_Byte_buffer<an_ifc_form_string_storage>;


/*
  |-------------------------------------------|
  |      FormStringize - 0.33 (12 bytes)      |
  |---------|----------------|---------|------|
  | Name    | Type           | Version | Size |
  |---------|----------------|---------|------|
  | locus   | SourceLocation | 0.33    | 8    |
  | operand | FormIndex      | 0.33    | 4    |
  |---------|----------------|---------|------|
*/
enum an_ifc_form_stringize_part : uint8_t {};
using an_ifc_form_stringize_storage = an_ifc_form_stringize_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_stringize_bytes = const an_ifc_form_stringize_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_stringize_bytes = an_ifc_form_stringize_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_stringize =
                             an_ifc_Byte_buffer<an_ifc_form_stringize_storage>;


/*
  |--------------------------------------------|
  |         FormTuple - 0.33 (8 bytes)         |
  |-------------|-------------|---------|------|
  | Name        | Type        | Version | Size |
  |-------------|-------------|---------|------|
  | start       | Index       | 0.33    | 4    |
  | cardinality | Cardinality | 0.33    | 4    |
  |-------------|-------------|---------|------|
*/
enum an_ifc_form_tuple_part : uint8_t {};
using an_ifc_form_tuple_storage = an_ifc_form_tuple_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_tuple_bytes = const an_ifc_form_tuple_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_tuple_bytes = an_ifc_form_tuple_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_tuple = an_ifc_Byte_buffer<an_ifc_form_tuple_storage>;


/*
  |-----------------------------------------|
  |     FormWhitespace - 0.33 (8 bytes)     |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  |-------|----------------|---------|------|
*/
enum an_ifc_form_whitespace_part : uint8_t {};
using an_ifc_form_whitespace_storage = an_ifc_form_whitespace_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_form_whitespace_bytes = const an_ifc_form_whitespace_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_form_whitespace_bytes = an_ifc_form_whitespace_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_form_whitespace =
                            an_ifc_Byte_buffer<an_ifc_form_whitespace_storage>;


/*
  |------------------------------------|
  |     HeapAttr - 0.33 (4 bytes)      |
  |-------|-----------|---------|------|
  | Name  | Type      | Version | Size |
  |-------|-----------|---------|------|
  | value | AttrIndex | 0.33    | 4    |
  |-------|-----------|---------|------|
*/
enum an_ifc_heap_attr_part : uint8_t {};
using an_ifc_heap_attr_storage = an_ifc_heap_attr_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_heap_attr_bytes = const an_ifc_heap_attr_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_heap_attr_bytes = an_ifc_heap_attr_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_heap_attr = an_ifc_Byte_buffer<an_ifc_heap_attr_storage>;


/*
  |-------------------------------------|
  |     HeapChart - 0.33 (4 bytes)      |
  |-------|------------|---------|------|
  | Name  | Type       | Version | Size |
  |-------|------------|---------|------|
  | value | ChartIndex | 0.33    | 4    |
  |-------|------------|---------|------|
*/
enum an_ifc_heap_chart_part : uint8_t {};
using an_ifc_heap_chart_storage = an_ifc_heap_chart_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_heap_chart_bytes = const an_ifc_heap_chart_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_heap_chart_bytes = an_ifc_heap_chart_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_heap_chart = an_ifc_Byte_buffer<an_ifc_heap_chart_storage>;


/*
  |------------------------------------|
  |     HeapDecl - 0.33 (4 bytes)      |
  |-------|-----------|---------|------|
  | Name  | Type      | Version | Size |
  |-------|-----------|---------|------|
  | value | DeclIndex | 0.33    | 4    |
  |-------|-----------|---------|------|

  |------------------------------------|
  |     HeapDecl - 0.41 (4 bytes)      |
  |-------|-----------|---------|------|
  | Name  | Type      | Version | Size |
  |-------|-----------|---------|------|
  | value | DeclIndex | 0.41    | 4    |
  |-------|-----------|---------|------|
*/
enum an_ifc_heap_decl_part : uint8_t {};
using an_ifc_heap_decl_storage = an_ifc_heap_decl_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_heap_decl_bytes = const an_ifc_heap_decl_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_heap_decl_bytes = an_ifc_heap_decl_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_heap_decl = an_ifc_Byte_buffer<an_ifc_heap_decl_storage>;


/*
  |------------------------------------|
  |     HeapExpr - 0.33 (4 bytes)      |
  |-------|-----------|---------|------|
  | Name  | Type      | Version | Size |
  |-------|-----------|---------|------|
  | value | ExprIndex | 0.33    | 4    |
  |-------|-----------|---------|------|
*/
enum an_ifc_heap_expr_part : uint8_t {};
using an_ifc_heap_expr_storage = an_ifc_heap_expr_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_heap_expr_bytes = const an_ifc_heap_expr_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_heap_expr_bytes = an_ifc_heap_expr_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_heap_expr = an_ifc_Byte_buffer<an_ifc_heap_expr_storage>;


/*
  |------------------------------------|
  |     HeapForm - 0.33 (4 bytes)      |
  |-------|-----------|---------|------|
  | Name  | Type      | Version | Size |
  |-------|-----------|---------|------|
  | value | FormIndex | 0.33    | 4    |
  |-------|-----------|---------|------|
*/
enum an_ifc_heap_form_part : uint8_t {};
using an_ifc_heap_form_storage = an_ifc_heap_form_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_heap_form_bytes = const an_ifc_heap_form_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_heap_form_bytes = an_ifc_heap_form_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_heap_form = an_ifc_Byte_buffer<an_ifc_heap_form_storage>;


/*
  |------------------------------------|
  |    HeapPPForm - 0.33 (4 bytes)     |
  |-------|-----------|---------|------|
  | Name  | Type      | Version | Size |
  |-------|-----------|---------|------|
  | value | FormIndex | 0.33    | 4    |
  |-------|-----------|---------|------|
*/
enum an_ifc_heap_pp_form_part : uint8_t {};
using an_ifc_heap_pp_form_storage = an_ifc_heap_pp_form_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_heap_pp_form_bytes = const an_ifc_heap_pp_form_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_heap_pp_form_bytes = an_ifc_heap_pp_form_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_heap_pp_form = an_ifc_Byte_buffer<an_ifc_heap_pp_form_storage>;


/*
  |------------------------------------|
  |     HeapStmt - 0.33 (4 bytes)      |
  |-------|-----------|---------|------|
  | Name  | Type      | Version | Size |
  |-------|-----------|---------|------|
  | value | StmtIndex | 0.33    | 4    |
  |-------|-----------|---------|------|
*/
enum an_ifc_heap_stmt_part : uint8_t {};
using an_ifc_heap_stmt_storage = an_ifc_heap_stmt_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_heap_stmt_bytes = const an_ifc_heap_stmt_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_heap_stmt_bytes = an_ifc_heap_stmt_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_heap_stmt = an_ifc_Byte_buffer<an_ifc_heap_stmt_storage>;


/*
  |--------------------------------------|
  |     HeapSyntax - 0.33 (4 bytes)      |
  |-------|-------------|---------|------|
  | Name  | Type        | Version | Size |
  |-------|-------------|---------|------|
  | value | SyntaxIndex | 0.33    | 4    |
  |-------|-------------|---------|------|
*/
enum an_ifc_heap_syntax_part : uint8_t {};
using an_ifc_heap_syntax_storage = an_ifc_heap_syntax_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_heap_syntax_bytes = const an_ifc_heap_syntax_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_heap_syntax_bytes = an_ifc_heap_syntax_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_heap_syntax = an_ifc_Byte_buffer<an_ifc_heap_syntax_storage>;


/*
  |------------------------------------|
  |     HeapType - 0.33 (4 bytes)      |
  |-------|-----------|---------|------|
  | Name  | Type      | Version | Size |
  |-------|-----------|---------|------|
  | value | TypeIndex | 0.33    | 4    |
  |-------|-----------|---------|------|
*/
enum an_ifc_heap_type_part : uint8_t {};
using an_ifc_heap_type_storage = an_ifc_heap_type_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_heap_type_bytes = const an_ifc_heap_type_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_heap_type_bytes = an_ifc_heap_type_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_heap_type = an_ifc_Byte_buffer<an_ifc_heap_type_storage>;


/*
  |--------------------------------------------------|
  |       MacroFunctionLike - 0.33 (24 bytes)        |
  |----------------|----------------|---------|------|
  | Name           | Type           | Version | Size |
  |----------------|----------------|---------|------|
  | locus          | SourceLocation | 0.33    | 8    |
  | name           | TextOffset     | 0.33    | 4    |
  | parameters     | FormIndex      | 0.33    | 4    |
  | body           | FormIndex      | 0.33    | 4    |
  | arity_variadic | VariadicArity  | 0.33    | 4    |
  |----------------|----------------|---------|------|
*/
enum an_ifc_macro_function_like_part : uint8_t {};
using an_ifc_macro_function_like_storage = an_ifc_macro_function_like_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_macro_function_like_bytes =
                                     const an_ifc_macro_function_like_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_macro_function_like_bytes = an_ifc_macro_function_like_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_macro_function_like =
                        an_ifc_Byte_buffer<an_ifc_macro_function_like_storage>;


/*
  |-----------------------------------------|
  |    MacroObjectLike - 0.33 (16 bytes)    |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  | name  | TextOffset     | 0.33    | 4    |
  | body  | FormIndex      | 0.33    | 4    |
  |-------|----------------|---------|------|
*/
enum an_ifc_macro_object_like_part : uint8_t {};
using an_ifc_macro_object_like_storage = an_ifc_macro_object_like_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_macro_object_like_bytes = const an_ifc_macro_object_like_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_macro_object_like_bytes = an_ifc_macro_object_like_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_macro_object_like =
                          an_ifc_Byte_buffer<an_ifc_macro_object_like_storage>;


/*
  |----------------------------------------------|
  |    ModuleExportReference - 0.33 (8 bytes)    |
  |-----------|-----------------|---------|------|
  | Name      | Type            | Version | Size |
  |-----------|-----------------|---------|------|
  | reference | ModuleReference | 0.33    | 8    |
  |-----------|-----------------|---------|------|
*/
enum an_ifc_module_export_reference_part : uint8_t {};
using an_ifc_module_export_reference_storage =
                                        an_ifc_module_export_reference_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_module_export_reference_bytes =
                                 const an_ifc_module_export_reference_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_module_export_reference_bytes =
                                        an_ifc_module_export_reference_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_module_export_reference =
                    an_ifc_Byte_buffer<an_ifc_module_export_reference_storage>;


/*
  |----------------------------------------------|
  |    ModuleImportReference - 0.33 (8 bytes)    |
  |-----------|-----------------|---------|------|
  | Name      | Type            | Version | Size |
  |-----------|-----------------|---------|------|
  | reference | ModuleReference | 0.33    | 8    |
  |-----------|-----------------|---------|------|
*/
enum an_ifc_module_import_reference_part : uint8_t {};
using an_ifc_module_import_reference_storage =
                                        an_ifc_module_import_reference_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_module_import_reference_bytes =
                                 const an_ifc_module_import_reference_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_module_import_reference_bytes =
                                        an_ifc_module_import_reference_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_module_import_reference =
                    an_ifc_Byte_buffer<an_ifc_module_import_reference_storage>;


/*
  |---------------------------------------|
  |    NameConversion - 0.33 (8 bytes)    |
  |---------|------------|---------|------|
  | Name    | Type       | Version | Size |
  |---------|------------|---------|------|
  | target  | TypeIndex  | 0.33    | 4    |
  | encoded | TextOffset | 0.33    | 4    |
  |---------|------------|---------|------|
*/
enum an_ifc_name_conversion_part : uint8_t {};
using an_ifc_name_conversion_storage = an_ifc_name_conversion_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_name_conversion_bytes = const an_ifc_name_conversion_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_name_conversion_bytes = an_ifc_name_conversion_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_name_conversion =
                            an_ifc_Byte_buffer<an_ifc_name_conversion_storage>;


/*
  |-----------------------------------------------|
  |          NameGuide - 0.33 (4 bytes)           |
  |------------------|-----------|---------|------|
  | Name             | Type      | Version | Size |
  |------------------|-----------|---------|------|
  | primary_template | DeclIndex | 0.33    | 4    |
  |------------------|-----------|---------|------|

  |-----------------------------------------------|
  |          NameGuide - 0.41 (4 bytes)           |
  |------------------|-----------|---------|------|
  | Name             | Type      | Version | Size |
  |------------------|-----------|---------|------|
  | primary_template | DeclIndex | 0.41    | 4    |
  |------------------|-----------|---------|------|
*/
enum an_ifc_name_guide_part : uint8_t {};
using an_ifc_name_guide_storage = an_ifc_name_guide_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_name_guide_bytes = const an_ifc_name_guide_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_name_guide_bytes = an_ifc_name_guide_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_name_guide = an_ifc_Byte_buffer<an_ifc_name_guide_storage>;


/*
  |---------------------------------------|
  |     NameLiteral - 0.33 (4 bytes)      |
  |---------|------------|---------|------|
  | Name    | Type       | Version | Size |
  |---------|------------|---------|------|
  | encoded | TextOffset | 0.33    | 4    |
  |---------|------------|---------|------|
*/
enum an_ifc_name_literal_part : uint8_t {};
using an_ifc_name_literal_storage = an_ifc_name_literal_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_name_literal_bytes = const an_ifc_name_literal_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_name_literal_bytes = an_ifc_name_literal_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_name_literal = an_ifc_Byte_buffer<an_ifc_name_literal_storage>;


/*
  |----------------------------------------------|
  |        NameOperator - 0.33 (6 bytes)         |
  |----------|------------------|---------|------|
  | Name     | Type             | Version | Size |
  |----------|------------------|---------|------|
  | encoded  | TextOffset       | 0.33    | 4    |
  | operator | OperatorCategory | 0.33    | 2    |
  |----------|------------------|---------|------|
*/
enum an_ifc_name_operator_part : uint8_t {};
using an_ifc_name_operator_storage = an_ifc_name_operator_part[6];
#if USE_MMAP_FOR_MODULES
using an_ifc_name_operator_bytes = const an_ifc_name_operator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_name_operator_bytes = an_ifc_name_operator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_name_operator = an_ifc_Byte_buffer<an_ifc_name_operator_storage>;


/*
  |-------------------------------------|
  |   NameSourceFile - 0.33 (8 bytes)   |
  |-------|------------|---------|------|
  | Name  | Type       | Version | Size |
  |-------|------------|---------|------|
  | path  | TextOffset | 0.33    | 4    |
  | guard | TextOffset | 0.33    | 4    |
  |-------|------------|---------|------|
*/
enum an_ifc_name_source_file_part : uint8_t {};
using an_ifc_name_source_file_storage = an_ifc_name_source_file_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_name_source_file_bytes = const an_ifc_name_source_file_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_name_source_file_bytes = an_ifc_name_source_file_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_name_source_file =
                           an_ifc_Byte_buffer<an_ifc_name_source_file_storage>;


/*
  |----------------------------------------|
  |  NameSpecialization - 0.33 (8 bytes)   |
  |-----------|-----------|---------|------|
  | Name      | Type      | Version | Size |
  |-----------|-----------|---------|------|
  | primary   | NameIndex | 0.33    | 4    |
  | arguments | ExprIndex | 0.33    | 4    |
  |-----------|-----------|---------|------|
*/
enum an_ifc_name_specialization_part : uint8_t {};
using an_ifc_name_specialization_storage = an_ifc_name_specialization_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_name_specialization_bytes =
                                     const an_ifc_name_specialization_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_name_specialization_bytes = an_ifc_name_specialization_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_name_specialization =
                        an_ifc_Byte_buffer<an_ifc_name_specialization_storage>;


/*
  |-----------------------------------|
  |   NameTemplate - 0.33 (4 bytes)   |
  |------|-----------|---------|------|
  | Name | Type      | Version | Size |
  |------|-----------|---------|------|
  | name | NameIndex | 0.33    | 4    |
  |------|-----------|---------|------|
*/
enum an_ifc_name_template_part : uint8_t {};
using an_ifc_name_template_storage = an_ifc_name_template_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_name_template_bytes = const an_ifc_name_template_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_name_template_bytes = an_ifc_name_template_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_name_template = an_ifc_Byte_buffer<an_ifc_name_template_storage>;


/*
  |--------------------------------------------|
  |      ScopeDescriptor - 0.33 (8 bytes)      |
  |-------------|-------------|---------|------|
  | Name        | Type        | Version | Size |
  |-------------|-------------|---------|------|
  | start       | Index       | 0.33    | 4    |
  | cardinality | Cardinality | 0.33    | 4    |
  |-------------|-------------|---------|------|
*/
enum an_ifc_scope_descriptor_part : uint8_t {};
using an_ifc_scope_descriptor_storage = an_ifc_scope_descriptor_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_scope_descriptor_bytes = const an_ifc_scope_descriptor_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_scope_descriptor_bytes = an_ifc_scope_descriptor_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_scope_descriptor =
                           an_ifc_Byte_buffer<an_ifc_scope_descriptor_storage>;


/*
  |------------------------------------|
  |    ScopeMember - 0.33 (4 bytes)    |
  |-------|-----------|---------|------|
  | Name  | Type      | Version | Size |
  |-------|-----------|---------|------|
  | index | DeclIndex | 0.33    | 4    |
  |-------|-----------|---------|------|

  |------------------------------------|
  |    ScopeMember - 0.41 (4 bytes)    |
  |-------|-----------|---------|------|
  | Name  | Type      | Version | Size |
  |-------|-----------|---------|------|
  | index | DeclIndex | 0.41    | 4    |
  |-------|-----------|---------|------|
*/
enum an_ifc_scope_member_part : uint8_t {};
using an_ifc_scope_member_storage = an_ifc_scope_member_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_scope_member_bytes = const an_ifc_scope_member_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_scope_member_bytes = an_ifc_scope_member_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_scope_member = an_ifc_Byte_buffer<an_ifc_scope_member_storage>;


/*
  |------------------------------------|
  |    SourceLine - 0.33 (8 bytes)     |
  |------|------------|---------|------|
  | Name | Type       | Version | Size |
  |------|------------|---------|------|
  | file | NameIndex  | 0.33    | 4    |
  | line | LineNumber | 0.33    | 4    |
  |------|------------|---------|------|
*/
enum an_ifc_source_line_part : uint8_t {};
using an_ifc_source_line_storage = an_ifc_source_line_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_source_line_bytes = const an_ifc_source_line_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_source_line_bytes = an_ifc_source_line_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_source_line = an_ifc_Byte_buffer<an_ifc_source_line_storage>;


/*
  |-----------------------------------------------|
  |       SourceSentence - 0.33 (16 bytes)        |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | start       | Index          | 0.33    | 4    |
  | cardinality | Cardinality    | 0.33    | 4    |
  | locus       | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_source_sentence_part : uint8_t {};
using an_ifc_source_sentence_storage = an_ifc_source_sentence_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_source_sentence_bytes = const an_ifc_source_sentence_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_source_sentence_bytes = an_ifc_source_sentence_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_source_sentence =
                            an_ifc_Byte_buffer<an_ifc_source_sentence_storage>;


/*
  |-----------------------------------------------|
  |         SourceWord - 0.33 (23 bytes)          |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | locus       | SourceLocation | 0.33    | 8    |
  | index       | Index          | 0.33    | 4    |
  | value       | u16            | 0.33    | 2    |
  | sort        | WordSort       | 0.33    | 1    |
  | __padding__ | uint8_t[8]     |         | 8    |
  |-------------|----------------|---------|------|
  | category    | WordCategory   | 0.33    | RF   |
  |-------------|----------------|---------|------|
*/
enum an_ifc_source_word_part : uint8_t {};
using an_ifc_source_word_storage = an_ifc_source_word_part[23];
#if USE_MMAP_FOR_MODULES
using an_ifc_source_word_bytes = const an_ifc_source_word_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_source_word_bytes = an_ifc_source_word_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_source_word = an_ifc_Byte_buffer<an_ifc_source_word_storage>;


/*
  |--------------------------------------------|
  |         StmtBlock - 0.33 (8 bytes)         |
  |-------------|-------------|---------|------|
  | Name        | Type        | Version | Size |
  |-------------|-------------|---------|------|
  | start       | Index       | 0.33    | 4    |
  | cardinality | Cardinality | 0.33    | 4    |
  |-------------|-------------|---------|------|
*/
enum an_ifc_stmt_block_part : uint8_t {};
using an_ifc_stmt_block_storage = an_ifc_stmt_block_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_block_bytes = const an_ifc_stmt_block_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_block_bytes = an_ifc_stmt_block_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_block = an_ifc_Byte_buffer<an_ifc_stmt_block_storage>;


/*
  |-----------------------------------------|
  |       StmtBreak - 0.33 (8 bytes)        |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  |-------|----------------|---------|------|
*/
enum an_ifc_stmt_break_part : uint8_t {};
using an_ifc_stmt_break_storage = an_ifc_stmt_break_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_break_bytes = const an_ifc_stmt_break_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_break_bytes = an_ifc_stmt_break_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_break = an_ifc_Byte_buffer<an_ifc_stmt_break_storage>;


/*
  |-----------------------------------------|
  |       StmtCase - 0.33 (12 bytes)        |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | expr  | ExprIndex      | 0.33    | 4    |
  | locus | SourceLocation | 0.33    | 8    |
  |-------|----------------|---------|------|
*/
enum an_ifc_stmt_case_part : uint8_t {};
using an_ifc_stmt_case_storage = an_ifc_stmt_case_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_case_bytes = const an_ifc_stmt_case_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_case_bytes = an_ifc_stmt_case_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_case = an_ifc_Byte_buffer<an_ifc_stmt_case_storage>;


/*
  |-----------------------------------------|
  |      StmtContinue - 0.33 (8 bytes)      |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  |-------|----------------|---------|------|
*/
enum an_ifc_stmt_continue_part : uint8_t {};
using an_ifc_stmt_continue_storage = an_ifc_stmt_continue_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_continue_bytes = const an_ifc_stmt_continue_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_continue_bytes = an_ifc_stmt_continue_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_continue = an_ifc_Byte_buffer<an_ifc_stmt_continue_storage>;


/*
  |-----------------------------------------|
  |      StmtDefault - 0.33 (8 bytes)       |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  |-------|----------------|---------|------|
*/
enum an_ifc_stmt_default_part : uint8_t {};
using an_ifc_stmt_default_storage = an_ifc_stmt_default_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_default_bytes = const an_ifc_stmt_default_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_default_bytes = an_ifc_stmt_default_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_default = an_ifc_Byte_buffer<an_ifc_stmt_default_storage>;


/*
  |---------------------------------------------|
  |        StmtDoWhile - 0.33 (16 bytes)        |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | condition | StmtIndex      | 0.33    | 4    |
  | body      | StmtIndex      | 0.33    | 4    |
  | locus     | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_stmt_do_while_part : uint8_t {};
using an_ifc_stmt_do_while_storage = an_ifc_stmt_do_while_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_do_while_bytes = const an_ifc_stmt_do_while_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_do_while_bytes = an_ifc_stmt_do_while_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_do_while = an_ifc_Byte_buffer<an_ifc_stmt_do_while_storage>;


/*
  |-----------------------------------------|
  |       StmtEmpty - 0.33 (8 bytes)        |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  |-------|----------------|---------|------|
*/
enum an_ifc_stmt_empty_part : uint8_t {};
using an_ifc_stmt_empty_storage = an_ifc_stmt_empty_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_empty_bytes = const an_ifc_stmt_empty_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_empty_bytes = an_ifc_stmt_empty_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_empty = an_ifc_Byte_buffer<an_ifc_stmt_empty_storage>;


/*
  |--------------------------------------|
  |    StmtExpansion - 0.33 (4 bytes)    |
  |---------|-----------|---------|------|
  | Name    | Type      | Version | Size |
  |---------|-----------|---------|------|
  | operand | StmtIndex | 0.33    | 4    |
  |---------|-----------|---------|------|
*/
enum an_ifc_stmt_expansion_part : uint8_t {};
using an_ifc_stmt_expansion_storage = an_ifc_stmt_expansion_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_expansion_bytes = const an_ifc_stmt_expansion_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_expansion_bytes = an_ifc_stmt_expansion_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_expansion =
                             an_ifc_Byte_buffer<an_ifc_stmt_expansion_storage>;


/*
  |-----------------------------------------|
  |    StmtExpression - 0.33 (12 bytes)     |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | expr  | ExprIndex      | 0.33    | 4    |
  | locus | SourceLocation | 0.33    | 8    |
  |-------|----------------|---------|------|
*/
enum an_ifc_stmt_expression_part : uint8_t {};
using an_ifc_stmt_expression_storage = an_ifc_stmt_expression_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_expression_bytes = const an_ifc_stmt_expression_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_expression_bytes = an_ifc_stmt_expression_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_expression =
                            an_ifc_Byte_buffer<an_ifc_stmt_expression_storage>;


/*
  |--------------------------------------------------|
  |            StmtFor - 0.33 (24 bytes)             |
  |----------------|----------------|---------|------|
  | Name           | Type           | Version | Size |
  |----------------|----------------|---------|------|
  | initialization | StmtIndex      | 0.33    | 4    |
  | condition      | StmtIndex      | 0.33    | 4    |
  | continuation   | StmtIndex      | 0.33    | 4    |
  | body           | StmtIndex      | 0.33    | 4    |
  | locus          | SourceLocation | 0.33    | 8    |
  |----------------|----------------|---------|------|
*/
enum an_ifc_stmt_for_part : uint8_t {};
using an_ifc_stmt_for_storage = an_ifc_stmt_for_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_for_bytes = const an_ifc_stmt_for_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_for_bytes = an_ifc_stmt_for_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_for = an_ifc_Byte_buffer<an_ifc_stmt_for_storage>;


/*
  |--------------------------------------------------|
  |             StmtIf - 0.33 (24 bytes)             |
  |----------------|----------------|---------|------|
  | Name           | Type           | Version | Size |
  |----------------|----------------|---------|------|
  | initialization | StmtIndex      | 0.33    | 4    |
  | condition      | StmtIndex      | 0.33    | 4    |
  | consequence    | StmtIndex      | 0.33    | 4    |
  | alternative    | StmtIndex      | 0.33    | 4    |
  | locus          | SourceLocation | 0.33    | 8    |
  |----------------|----------------|---------|------|
*/
enum an_ifc_stmt_if_part : uint8_t {};
using an_ifc_stmt_if_storage = an_ifc_stmt_if_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_if_bytes = const an_ifc_stmt_if_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_if_bytes = an_ifc_stmt_if_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_if = an_ifc_Byte_buffer<an_ifc_stmt_if_storage>;


/*
  |---------------------------------------------------|
  |           StmtReturn - 0.33 (20 bytes)            |
  |-----------------|----------------|---------|------|
  | Name            | Type           | Version | Size |
  |-----------------|----------------|---------|------|
  | expr            | ExprIndex      | 0.33    | 4    |
  | function_type   | TypeIndex      | 0.33    | 4    |
  | expression_type | TypeIndex      | 0.33    | 4    |
  | locus           | SourceLocation | 0.33    | 8    |
  |-----------------|----------------|---------|------|
*/
enum an_ifc_stmt_return_part : uint8_t {};
using an_ifc_stmt_return_storage = an_ifc_stmt_return_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_return_bytes = const an_ifc_stmt_return_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_return_bytes = an_ifc_stmt_return_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_return = an_ifc_Byte_buffer<an_ifc_stmt_return_storage>;


/*
  |--------------------------------------------------|
  |           StmtSwitch - 0.33 (20 bytes)           |
  |----------------|----------------|---------|------|
  | Name           | Type           | Version | Size |
  |----------------|----------------|---------|------|
  | initialization | StmtIndex      | 0.33    | 4    |
  | condition      | ExprIndex      | 0.33    | 4    |
  | body           | StmtIndex      | 0.33    | 4    |
  | locus          | SourceLocation | 0.33    | 8    |
  |----------------|----------------|---------|------|
*/
enum an_ifc_stmt_switch_part : uint8_t {};
using an_ifc_stmt_switch_storage = an_ifc_stmt_switch_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_switch_bytes = const an_ifc_stmt_switch_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_switch_bytes = an_ifc_stmt_switch_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_switch = an_ifc_Byte_buffer<an_ifc_stmt_switch_storage>;


/*
  |-----------------------------------------|
  |   StmtVariableDecl - 0.33 (12 bytes)    |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | decl  | DeclIndex      | 0.33    | 4    |
  | locus | SourceLocation | 0.33    | 8    |
  |-------|----------------|---------|------|

  |-----------------------------------------|
  |   StmtVariableDecl - 0.41 (12 bytes)    |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | decl  | DeclIndex      | 0.41    | 4    |
  | locus | SourceLocation | 0.33    | 8    |
  |-------|----------------|---------|------|
*/
enum an_ifc_stmt_variable_decl_part : uint8_t {};
using an_ifc_stmt_variable_decl_storage = an_ifc_stmt_variable_decl_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_variable_decl_bytes =
                                      const an_ifc_stmt_variable_decl_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_variable_decl_bytes = an_ifc_stmt_variable_decl_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_variable_decl =
                         an_ifc_Byte_buffer<an_ifc_stmt_variable_decl_storage>;


/*
  |---------------------------------------------|
  |         StmtWhile - 0.33 (16 bytes)         |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | condition | StmtIndex      | 0.33    | 4    |
  | body      | StmtIndex      | 0.33    | 4    |
  | locus     | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_stmt_while_part : uint8_t {};
using an_ifc_stmt_while_storage = an_ifc_stmt_while_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_stmt_while_bytes = const an_ifc_stmt_while_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_stmt_while_bytes = an_ifc_stmt_while_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_stmt_while = an_ifc_Byte_buffer<an_ifc_stmt_while_storage>;


/*
  |-----------------------------------------------|
  |    SyntaxAccessSpecifier - 0.33 (48 bytes)    |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | designator  | ExprIndex      | 0.33    | 4    |
  | access      | KeywordSyntax  | 0.33    | 12   |
  | virtual_kw  | SourceLocation | 0.33    | 8    |
  | locus       | SourceLocation | 0.33    | 8    |
  | virtual_kw2 | SourceLocation | 0.33    | 8    |
  | comma       | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_access_specifier_part : uint8_t {};
using an_ifc_syntax_access_specifier_storage =
                                       an_ifc_syntax_access_specifier_part[48];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_access_specifier_bytes =
                                 const an_ifc_syntax_access_specifier_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_access_specifier_bytes =
                                        an_ifc_syntax_access_specifier_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_access_specifier =
                    an_ifc_Byte_buffer<an_ifc_syntax_access_specifier_storage>;


/*
  |---------------------------------------------|
  |  SyntaxAliasDeclaration - 0.33 (32 bytes)   |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | name      | ExprIndex      | 0.33    | 4    |
  | aliasee   | SyntaxIndex    | 0.33    | 4    |
  | locus     | SourceLocation | 0.33    | 8    |
  | equal     | SourceLocation | 0.33    | 8    |
  | semicolon | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_alias_declaration_part : uint8_t {};
using an_ifc_syntax_alias_declaration_storage =
                                      an_ifc_syntax_alias_declaration_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_alias_declaration_bytes =
                                const an_ifc_syntax_alias_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_alias_declaration_bytes =
                                       an_ifc_syntax_alias_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_alias_declaration =
                   an_ifc_Byte_buffer<an_ifc_syntax_alias_declaration_storage>;


/*
  |-----------------------------------------------|
  |        SyntaxAlignas - 0.33 (28 bytes)        |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | operand     | SyntaxIndex    | 0.33    | 4    |
  | locus       | SourceLocation | 0.33    | 8    |
  | left_paren  | SourceLocation | 0.33    | 8    |
  | right_paren | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_alignas_part : uint8_t {};
using an_ifc_syntax_alignas_storage = an_ifc_syntax_alignas_part[28];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_alignas_bytes = const an_ifc_syntax_alignas_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_alignas_bytes = an_ifc_syntax_alignas_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_alignas =
                             an_ifc_Byte_buffer<an_ifc_syntax_alignas_storage>;


/*
  |-------------------------------------------------|
  |     SyntaxArrayDeclarator - 0.33 (20 bytes)     |
  |---------------|----------------|---------|------|
  | Name          | Type           | Version | Size |
  |---------------|----------------|---------|------|
  | bound         | ExprIndex      | 0.33    | 4    |
  | left_bracket  | SourceLocation | 0.33    | 8    |
  | right_bracket | SourceLocation | 0.33    | 8    |
  |---------------|----------------|---------|------|
*/
enum an_ifc_syntax_array_declarator_part : uint8_t {};
using an_ifc_syntax_array_declarator_storage =
                                       an_ifc_syntax_array_declarator_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_array_declarator_bytes =
                                 const an_ifc_syntax_array_declarator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_array_declarator_bytes =
                                        an_ifc_syntax_array_declarator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_array_declarator =
                    an_ifc_Byte_buffer<an_ifc_syntax_array_declarator_storage>;


/*
  |-------------------------------------------------|
  |       SyntaxArrayIndex - 0.33 (24 bytes)        |
  |---------------|----------------|---------|------|
  | Name          | Type           | Version | Size |
  |---------------|----------------|---------|------|
  | array         | ExprIndex      | 0.33    | 4    |
  | index         | ExprIndex      | 0.33    | 4    |
  | left_bracket  | SourceLocation | 0.33    | 8    |
  | right_bracket | SourceLocation | 0.33    | 8    |
  |---------------|----------------|---------|------|
*/
enum an_ifc_syntax_array_index_part : uint8_t {};
using an_ifc_syntax_array_index_storage = an_ifc_syntax_array_index_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_array_index_bytes =
                                      const an_ifc_syntax_array_index_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_array_index_bytes = an_ifc_syntax_array_index_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_array_index =
                         an_ifc_Byte_buffer<an_ifc_syntax_array_index_storage>;


/*
  |--------------------------------------------------|
  | SyntaxArrayOrFunctionDeclarator - 0.33 (8 bytes) |
  |----------------|--------------|----------|-------|
  | Name           | Type         | Version  | Size  |
  |----------------|--------------|----------|-------|
  | declarator     | SyntaxIndex  | 0.33     | 4     |
  | next           | SyntaxIndex  | 0.33     | 4     |
  |----------------|--------------|----------|-------|
*/
enum an_ifc_syntax_array_or_function_declarator_part : uint8_t {};
using an_ifc_syntax_array_or_function_declarator_storage =
                            an_ifc_syntax_array_or_function_declarator_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_array_or_function_declarator_bytes =
                     const an_ifc_syntax_array_or_function_declarator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_array_or_function_declarator_bytes =
                            an_ifc_syntax_array_or_function_declarator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_array_or_function_declarator =
        an_ifc_Byte_buffer<an_ifc_syntax_array_or_function_declarator_storage>;


/*
  |------------------------------------------|
  |   SyntaxAsmStatement - 0.33 (12 bytes)   |
  |--------|----------------|---------|------|
  | Name   | Type           | Version | Size |
  |--------|----------------|---------|------|
  | tokens | SentenceIndex  | 0.33    | 4    |
  | locus  | SourceLocation | 0.33    | 8    |
  |--------|----------------|---------|------|
*/
enum an_ifc_syntax_asm_statement_part : uint8_t {};
using an_ifc_syntax_asm_statement_storage =
                                          an_ifc_syntax_asm_statement_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_asm_statement_bytes =
                                    const an_ifc_syntax_asm_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_asm_statement_bytes = an_ifc_syntax_asm_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_asm_statement =
                       an_ifc_Byte_buffer<an_ifc_syntax_asm_statement_storage>;


/*
  |---------------------------------------------------|
  |         SyntaxAttribute - 0.33 (36 bytes)         |
  |-----------------|----------------|---------|------|
  | Name            | Type           | Version | Size |
  |-----------------|----------------|---------|------|
  | name            | ExprIndex      | 0.33    | 4    |
  | scope           | ExprIndex      | 0.33    | 4    |
  | argument_clause | SyntaxIndex    | 0.33    | 4    |
  | colons          | SourceLocation | 0.33    | 8    |
  | expander        | SourceLocation | 0.33    | 8    |
  | comma           | SourceLocation | 0.33    | 8    |
  |-----------------|----------------|---------|------|
*/
enum an_ifc_syntax_attribute_part : uint8_t {};
using an_ifc_syntax_attribute_storage = an_ifc_syntax_attribute_part[36];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_attribute_bytes = const an_ifc_syntax_attribute_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attribute_bytes = an_ifc_syntax_attribute_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attribute =
                           an_ifc_Byte_buffer<an_ifc_syntax_attribute_storage>;


/*
  |-------------------------------------------------|
  | SyntaxAttributeArgumentClause - 0.33 (20 bytes) |
  |---------------|----------------|---------|------|
  | Name          | Type           | Version | Size |
  |---------------|----------------|---------|------|
  | tokens        | SentenceIndex  | 0.33    | 4    |
  | left_paren    | SourceLocation | 0.33    | 8    |
  | right_paren   | SourceLocation | 0.33    | 8    |
  |---------------|----------------|---------|------|
*/
enum an_ifc_syntax_attribute_argument_clause_part : uint8_t {};
using an_ifc_syntax_attribute_argument_clause_storage =
                              an_ifc_syntax_attribute_argument_clause_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_attribute_argument_clause_bytes =
                        const an_ifc_syntax_attribute_argument_clause_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attribute_argument_clause_bytes =
                               an_ifc_syntax_attribute_argument_clause_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attribute_argument_clause =
           an_ifc_Byte_buffer<an_ifc_syntax_attribute_argument_clause_storage>;


/*
  |-------------------------------------------------|
  |   SyntaxAttributeSpecifier - 0.33 (40 bytes)    |
  |---------------|----------------|---------|------|
  | Name          | Type           | Version | Size |
  |---------------|----------------|---------|------|
  | prefix        | SyntaxIndex    | 0.33    | 4    |
  | attributes    | SyntaxIndex    | 0.33    | 4    |
  | left_paren_1  | SourceLocation | 0.33    | 8    |
  | left_paren_2  | SourceLocation | 0.33    | 8    |
  | right_paren_1 | SourceLocation | 0.33    | 8    |
  | right_paren_2 | SourceLocation | 0.33    | 8    |
  |---------------|----------------|---------|------|
*/
enum an_ifc_syntax_attribute_specifier_part : uint8_t {};
using an_ifc_syntax_attribute_specifier_storage =
                                    an_ifc_syntax_attribute_specifier_part[40];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_attribute_specifier_bytes =
                              const an_ifc_syntax_attribute_specifier_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attribute_specifier_bytes =
                                     an_ifc_syntax_attribute_specifier_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attribute_specifier =
                 an_ifc_Byte_buffer<an_ifc_syntax_attribute_specifier_storage>;


/*
  |----------------------------------------------|
  | SyntaxAttributeSpecifierSeq - 0.33 (4 bytes) |
  |---------------|-------------|---------|------|
  | Name          | Type        | Version | Size |
  |---------------|-------------|---------|------|
  | attributes    | SyntaxIndex | 0.33    | 4    |
  |---------------|-------------|---------|------|
*/
enum an_ifc_syntax_attribute_specifier_seq_part : uint8_t {};
using an_ifc_syntax_attribute_specifier_seq_storage =
                                 an_ifc_syntax_attribute_specifier_seq_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_attribute_specifier_seq_bytes =
                          const an_ifc_syntax_attribute_specifier_seq_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attribute_specifier_seq_bytes =
                                 an_ifc_syntax_attribute_specifier_seq_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attribute_specifier_seq =
             an_ifc_Byte_buffer<an_ifc_syntax_attribute_specifier_seq_storage>;


/*
  |----------------------------------------------|
  | SyntaxAttributeUsingPrefix - 0.33 (16 bytes) |
  |---------|-----------------|----------|-------|
  | Name    | Type            | Version  | Size  |
  |---------|-----------------|----------|-------|
  | scope   | SourceLocation  | 0.33     | 8     |
  | locus   | SourceLocation  | 0.33     | 8     |
  |---------|-----------------|----------|-------|
*/
enum an_ifc_syntax_attribute_using_prefix_part : uint8_t {};
using an_ifc_syntax_attribute_using_prefix_storage =
                                 an_ifc_syntax_attribute_using_prefix_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_attribute_using_prefix_bytes =
                           const an_ifc_syntax_attribute_using_prefix_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attribute_using_prefix_bytes =
                                  an_ifc_syntax_attribute_using_prefix_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attribute_using_prefix =
              an_ifc_Byte_buffer<an_ifc_syntax_attribute_using_prefix_storage>;


/*
  |-----------------------------------------------|
  | SyntaxAttributedDeclaration - 0.33 (16 bytes) |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | locus       | SourceLocation | 0.33    | 8    |
  | decl        | SyntaxIndex    | 0.33    | 4    |
  | attributes  | SyntaxIndex    | 0.33    | 4    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_attributed_declaration_part : uint8_t {};
using an_ifc_syntax_attributed_declaration_storage =
                                 an_ifc_syntax_attributed_declaration_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_attributed_declaration_bytes =
                           const an_ifc_syntax_attributed_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attributed_declaration_bytes =
                                  an_ifc_syntax_attributed_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attributed_declaration =
              an_ifc_Byte_buffer<an_ifc_syntax_attributed_declaration_storage>;


/*
  |---------------------------------------------|
  | SyntaxAttributedStatement - 0.33 (12 bytes) |
  |------------|---------------|---------|------|
  | Name       | Type          | Version | Size |
  |------------|---------------|---------|------|
  | pragma     | SentenceIndex | 0.33    | 4    |
  | stmt       | SyntaxIndex   | 0.33    | 4    |
  | attributes | SyntaxIndex   | 0.33    | 4    |
  |------------|---------------|---------|------|
*/
enum an_ifc_syntax_attributed_statement_part : uint8_t {};
using an_ifc_syntax_attributed_statement_storage =
                                   an_ifc_syntax_attributed_statement_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_attributed_statement_bytes =
                             const an_ifc_syntax_attributed_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attributed_statement_bytes =
                                    an_ifc_syntax_attributed_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_attributed_statement =
                an_ifc_Byte_buffer<an_ifc_syntax_attributed_statement_storage>;


/*
  |------------------------------------------|
  |  SyntaxBaseSpecifier - 0.33 (20 bytes)   |
  |--------|----------------|---------|------|
  | Name   | Type           | Version | Size |
  |--------|----------------|---------|------|
  | access | KeywordSyntax  | 0.33    | 12   |
  | colon  | SourceLocation | 0.33    | 8    |
  |--------|----------------|---------|------|
*/
enum an_ifc_syntax_base_specifier_part : uint8_t {};
using an_ifc_syntax_base_specifier_storage =
                                         an_ifc_syntax_base_specifier_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_base_specifier_bytes =
                                   const an_ifc_syntax_base_specifier_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_base_specifier_bytes =
                                          an_ifc_syntax_base_specifier_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_base_specifier =
                      an_ifc_Byte_buffer<an_ifc_syntax_base_specifier_storage>;


/*
  |---------------------------------------------------|
  |     SyntaxBaseSpecifierList - 0.33 (12 bytes)     |
  |-----------------|----------------|---------|------|
  | Name            | Type           | Version | Size |
  |-----------------|----------------|---------|------|
  | base_specifiers | SyntaxIndex    | 0.33    | 4    |
  | colon           | SourceLocation | 0.33    | 8    |
  |-----------------|----------------|---------|------|
*/
enum an_ifc_syntax_base_specifier_list_part : uint8_t {};
using an_ifc_syntax_base_specifier_list_storage =
                                    an_ifc_syntax_base_specifier_list_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_base_specifier_list_bytes =
                              const an_ifc_syntax_base_specifier_list_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_base_specifier_list_bytes =
                                     an_ifc_syntax_base_specifier_list_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_base_specifier_list =
                 an_ifc_Byte_buffer<an_ifc_syntax_base_specifier_list_storage>;


/*
  |----------------------------------------------------|
  |    SyntaxBinaryFoldExpression - 0.33 (54 bytes)    |
  |--------------|--------------------|---------|------|
  | Name         | Type               | Version | Size |
  |--------------|--------------------|---------|------|
  | direction    | FoldDirectionSort  | 0.33    | 4    |
  | operand_1    | ExprIndex          | 0.33    | 4    |
  | operand_2    | ExprIndex          | 0.33    | 4    |
  | dyad         | DyadicOperatorSort | 0.33    | 2    |
  | locus        | SourceLocation     | 0.33    | 8    |
  | ellipsis     | SourceLocation     | 0.33    | 8    |
  | glyph_loci_1 | SourceLocation     | 0.33    | 8    |
  | glyph_loci_2 | SourceLocation     | 0.33    | 8    |
  | right_paren  | SourceLocation     | 0.33    | 8    |
  |--------------|--------------------|---------|------|
*/
enum an_ifc_syntax_binary_fold_expression_part : uint8_t {};
using an_ifc_syntax_binary_fold_expression_storage =
                                 an_ifc_syntax_binary_fold_expression_part[54];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_binary_fold_expression_bytes =
                           const an_ifc_syntax_binary_fold_expression_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_binary_fold_expression_bytes =
                                  an_ifc_syntax_binary_fold_expression_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_binary_fold_expression =
              an_ifc_Byte_buffer<an_ifc_syntax_binary_fold_expression_storage>;


/*
  |---------------------------------------------|
  |   SyntaxBreakStatement - 0.33 (16 bytes)    |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | break     | SourceLocation | 0.33    | 8    |
  | semicolon | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_break_statement_part : uint8_t {};
using an_ifc_syntax_break_statement_storage =
                                        an_ifc_syntax_break_statement_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_break_statement_bytes =
                                  const an_ifc_syntax_break_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_break_statement_bytes =
                                         an_ifc_syntax_break_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_break_statement =
                     an_ifc_Byte_buffer<an_ifc_syntax_break_statement_storage>;


/*
  |------------------------------------------|
  |  SyntaxCaptureDefault - 0.33 (17 bytes)  |
  |--------|----------------|---------|------|
  | Name   | Type           | Version | Size |
  |--------|----------------|---------|------|
  | locus  | SourceLocation | 0.33    | 8    |
  | comma  | SourceLocation | 0.33    | 8    |
  | by_ref | bool           | 0.33    | 1    |
  |--------|----------------|---------|------|
*/
enum an_ifc_syntax_capture_default_part : uint8_t {};
using an_ifc_syntax_capture_default_storage =
                                        an_ifc_syntax_capture_default_part[17];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_capture_default_bytes =
                                  const an_ifc_syntax_capture_default_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_capture_default_bytes =
                                         an_ifc_syntax_capture_default_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_capture_default =
                     an_ifc_Byte_buffer<an_ifc_syntax_capture_default_storage>;


/*
  |----------------------------------------------|
  |    SyntaxClassSpecifier - 0.33 (32 bytes)    |
  |-------------|---------------|---------|------|
  | Name        | Type          | Version | Size |
  |-------------|---------------|---------|------|
  | name        | ExprIndex     | 0.33    | 4    |
  | class_key   | KeywordSyntax | 0.33    | 12   |
  | bases       | SyntaxIndex   | 0.33    | 4    |
  | members     | SyntaxIndex   | 0.33    | 4    |
  | left_paren  | SyntaxIndex   | 0.33    | 4    |
  | right_paren | SyntaxIndex   | 0.33    | 4    |
  |-------------|---------------|---------|------|
*/
enum an_ifc_syntax_class_specifier_part : uint8_t {};
using an_ifc_syntax_class_specifier_storage =
                                        an_ifc_syntax_class_specifier_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_class_specifier_bytes =
                                  const an_ifc_syntax_class_specifier_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_class_specifier_bytes =
                                         an_ifc_syntax_class_specifier_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_class_specifier =
                     an_ifc_Byte_buffer<an_ifc_syntax_class_specifier_storage>;


/*
  |------------------------------------------------|
  |  SyntaxCompoundRequirement - 0.33 (32 bytes)   |
  |--------------|----------------|---------|------|
  | Name         | Type           | Version | Size |
  |--------------|----------------|---------|------|
  | condition    | ExprIndex      | 0.33    | 4    |
  | constraint   | ExprIndex      | 0.33    | 4    |
  | locus        | SourceLocation | 0.33    | 8    |
  | right_curly  | SourceLocation | 0.33    | 8    |
  | noexcept_loc | SourceLocation | 0.33    | 8    |
  |--------------|----------------|---------|------|
*/
enum an_ifc_syntax_compound_requirement_part : uint8_t {};
using an_ifc_syntax_compound_requirement_storage =
                                   an_ifc_syntax_compound_requirement_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_compound_requirement_bytes =
                             const an_ifc_syntax_compound_requirement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_compound_requirement_bytes =
                                    an_ifc_syntax_compound_requirement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_compound_requirement =
                an_ifc_Byte_buffer<an_ifc_syntax_compound_requirement_storage>;


/*
  |-----------------------------------------------|
  |   SyntaxCompoundStatement - 0.33 (24 bytes)   |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | pragam      | SentenceIndex  | 0.33    | 4    |
  | stmts       | SyntaxIndex    | 0.33    | 4    |
  | left_curly  | SourceLocation | 0.33    | 8    |
  | right_curly | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_compound_statement_part : uint8_t {};
using an_ifc_syntax_compound_statement_storage =
                                     an_ifc_syntax_compound_statement_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_compound_statement_bytes =
                               const an_ifc_syntax_compound_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_compound_statement_bytes =
                                      an_ifc_syntax_compound_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_compound_statement =
                  an_ifc_Byte_buffer<an_ifc_syntax_compound_statement_storage>;


/*
  |---------------------------------------------------|
  |     SyntaxConceptDefinition - 0.33 (44 bytes)     |
  |-----------------|----------------|---------|------|
  | Name            | Type           | Version | Size |
  |-----------------|----------------|---------|------|
  | parameters      | SyntaxIndex    | 0.33    | 4    |
  | locus           | SourceLocation | 0.33    | 8    |
  | name            | TextOffset     | 0.33    | 4    |
  | initializer     | ExprIndex      | 0.33    | 4    |
  | concept_keyword | SourceLocation | 0.33    | 8    |
  | equal           | SourceLocation | 0.33    | 8    |
  | semicolon       | SourceLocation | 0.33    | 8    |
  |-----------------|----------------|---------|------|
*/
enum an_ifc_syntax_concept_definition_part : uint8_t {};
using an_ifc_syntax_concept_definition_storage =
                                     an_ifc_syntax_concept_definition_part[44];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_concept_definition_bytes =
                               const an_ifc_syntax_concept_definition_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_concept_definition_bytes =
                                      an_ifc_syntax_concept_definition_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_concept_definition =
                  an_ifc_Byte_buffer<an_ifc_syntax_concept_definition_storage>;


/*
  |---------------------------------------------------|
  |   SyntaxConditionDeclaration - 0.33 (16 bytes)    |
  |-----------------|----------------|---------|------|
  | Name            | Type           | Version | Size |
  |-----------------|----------------|---------|------|
  | decl_specifier  | SyntaxIndex    | 0.33    | 4    |
  | initializaerion | SyntaxIndex    | 0.33    | 4    |
  | locus           | SourceLocation | 0.33    | 8    |
  |-----------------|----------------|---------|------|
*/
enum an_ifc_syntax_condition_declaration_part : uint8_t {};
using an_ifc_syntax_condition_declaration_storage =
                                  an_ifc_syntax_condition_declaration_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_condition_declaration_bytes =
                            const an_ifc_syntax_condition_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_condition_declaration_bytes =
                                   an_ifc_syntax_condition_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_condition_declaration =
               an_ifc_Byte_buffer<an_ifc_syntax_condition_declaration_storage>;


/*
  |---------------------------------------------|
  |  SyntaxContinueStatement - 0.33 (16 bytes)  |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | continue  | SourceLocation | 0.33    | 8    |
  | semicolon | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_continue_statement_part : uint8_t {};
using an_ifc_syntax_continue_statement_storage =
                                     an_ifc_syntax_continue_statement_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_continue_statement_bytes =
                               const an_ifc_syntax_continue_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_continue_statement_bytes =
                                      an_ifc_syntax_continue_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_continue_statement =
                  an_ifc_Byte_buffer<an_ifc_syntax_continue_statement_storage>;


/*
  |------------------------------------------------|
  |    SyntaxCtorInitializer - 0.33 (12 bytes)     |
  |--------------|----------------|---------|------|
  | Name         | Type           | Version | Size |
  |--------------|----------------|---------|------|
  | initializers | SyntaxIndex    | 0.33    | 4    |
  | colon        | SourceLocation | 0.33    | 8    |
  |--------------|----------------|---------|------|
*/
enum an_ifc_syntax_ctor_initializer_part : uint8_t {};
using an_ifc_syntax_ctor_initializer_storage =
                                       an_ifc_syntax_ctor_initializer_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_ctor_initializer_bytes =
                                 const an_ifc_syntax_ctor_initializer_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_ctor_initializer_bytes =
                                        an_ifc_syntax_ctor_initializer_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_ctor_initializer =
                    an_ifc_Byte_buffer<an_ifc_syntax_ctor_initializer_storage>;


/*
  |----------------------------------------------------|
  |      SyntaxDeclSpecifierSeq - 0.33 (29 bytes)      |
  |---------------|-------------------|---------|------|
  | Name          | Type              | Version | Size |
  |---------------|-------------------|---------|------|
  | type          | TypeIndex         | 0.33    | 4    |
  | type_name     | SyntaxIndex       | 0.33    | 4    |
  | locus         | SourceLocation    | 0.33    | 8    |
  | storage_class | StorageClass      | 0.33    | 4    |
  | declspec      | SentenceIndex     | 0.33    | 4    |
  | explicit_kw   | SyntaxIndex       | 0.33    | 4    |
  | qualifiers    | QualifierBitfield | 0.33    | 1    |
  |---------------|-------------------|---------|------|
*/
enum an_ifc_syntax_decl_specifier_seq_part : uint8_t {};
using an_ifc_syntax_decl_specifier_seq_storage =
                                     an_ifc_syntax_decl_specifier_seq_part[29];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_decl_specifier_seq_bytes =
                               const an_ifc_syntax_decl_specifier_seq_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_decl_specifier_seq_bytes =
                                      an_ifc_syntax_decl_specifier_seq_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_decl_specifier_seq =
                  an_ifc_Byte_buffer<an_ifc_syntax_decl_specifier_seq_storage>;


/*
  |---------------------------------------------|
  | SyntaxDeclarationStatement - 0.33 (8 bytes) |
  |---------|----------------|----------|-------|
  | Name    | Type           | Version  | Size  |
  |---------|----------------|----------|-------|
  | pragma  | SentenceIndex  | 0.33     | 4     |
  | decl    | SyntaxIndex    | 0.33     | 4     |
  |---------|----------------|----------|-------|
*/
enum an_ifc_syntax_declaration_statement_part : uint8_t {};
using an_ifc_syntax_declaration_statement_storage =
                                   an_ifc_syntax_declaration_statement_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_declaration_statement_bytes =
                            const an_ifc_syntax_declaration_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_declaration_statement_bytes =
                                   an_ifc_syntax_declaration_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_declaration_statement =
               an_ifc_Byte_buffer<an_ifc_syntax_declaration_statement_storage>;


/*
  |-------------------------------------------------------------|
  |             SyntaxDeclarator - 0.33 (43 bytes)              |
  |--------------------|-----------------------|---------|------|
  | Name               | Type                  | Version | Size |
  |--------------------|-----------------------|---------|------|
  | pointer            | SyntaxIndex           | 0.33    | 4    |
  | parenthesized      | SyntaxIndex           | 0.33    | 4    |
  | array_or_function  | SyntaxIndex           | 0.33    | 4    |
  | trailing_target    | SyntaxIndex           | 0.33    | 4    |
  | virtual_specifiers | SyntaxIndex           | 0.33    | 4    |
  | name               | ExprIndex             | 0.33    | 4    |
  | ellipsis           | SourceLocation        | 0.33    | 8    |
  | locus              | SourceLocation        | 0.33    | 8    |
  | qualifiers         | QualifierBitfield     | 0.33    | 1    |
  | convention         | CallingConventionSort | 0.33    | 1    |
  | callable           | bool                  | 0.33    | 1    |
  |--------------------|-----------------------|---------|------|
*/
enum an_ifc_syntax_declarator_part : uint8_t {};
using an_ifc_syntax_declarator_storage = an_ifc_syntax_declarator_part[43];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_declarator_bytes = const an_ifc_syntax_declarator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_declarator_bytes = an_ifc_syntax_declarator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_declarator =
                          an_ifc_Byte_buffer<an_ifc_syntax_declarator_storage>;


/*
  |----------------------------------------------------|
  |     SyntaxDecltypeSpecifier - 0.33 (28 bytes)      |
  |------------------|----------------|---------|------|
  | Name             | Type           | Version | Size |
  |------------------|----------------|---------|------|
  | expr             | ExprIndex      | 0.33    | 4    |
  | decltype_keyword | SourceLocation | 0.33    | 8    |
  | left_paren       | SourceLocation | 0.33    | 8    |
  | right_paren      | SourceLocation | 0.33    | 8    |
  |------------------|----------------|---------|------|
*/
enum an_ifc_syntax_decltype_specifier_part : uint8_t {};
using an_ifc_syntax_decltype_specifier_storage =
                                     an_ifc_syntax_decltype_specifier_part[28];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_decltype_specifier_bytes =
                               const an_ifc_syntax_decltype_specifier_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_decltype_specifier_bytes =
                                      an_ifc_syntax_decltype_specifier_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_decltype_specifier =
                  an_ifc_Byte_buffer<an_ifc_syntax_decltype_specifier_storage>;


/*
  |---------------------------------------------|
  |  SyntaxDoWhileStatement - 0.33 (36 bytes)   |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | pragma    | SentenceIndex  | 0.33    | 4    |
  | condition | ExprIndex      | 0.33    | 4    |
  | body      | SyntaxIndex    | 0.33    | 4    |
  | do        | SourceLocation | 0.33    | 8    |
  | while     | SourceLocation | 0.33    | 8    |
  | semicolon | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_do_while_statement_part : uint8_t {};
using an_ifc_syntax_do_while_statement_storage =
                                     an_ifc_syntax_do_while_statement_part[36];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_do_while_statement_bytes =
                               const an_ifc_syntax_do_while_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_do_while_statement_bytes =
                                      an_ifc_syntax_do_while_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_do_while_statement =
                  an_ifc_Byte_buffer<an_ifc_syntax_do_while_statement_storage>;


/*
  |-----------------------------------------------|
  | SyntaxDynamicExceptionSpec - 0.33 (36 bytes)  |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | type_list   | SyntaxIndex    | 0.33    | 4    |
  | throw       | SourceLocation | 0.33    | 8    |
  | left_paren  | SourceLocation | 0.33    | 8    |
  | expander    | SourceLocation | 0.33    | 8    |
  | right_paren | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_dynamic_exception_spec_part : uint8_t {};
using an_ifc_syntax_dynamic_exception_spec_storage =
                                 an_ifc_syntax_dynamic_exception_spec_part[36];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_dynamic_exception_spec_bytes =
                           const an_ifc_syntax_dynamic_exception_spec_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_dynamic_exception_spec_bytes =
                                  an_ifc_syntax_dynamic_exception_spec_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_dynamic_exception_spec =
              an_ifc_Byte_buffer<an_ifc_syntax_dynamic_exception_spec_storage>;


/*
  |-----------------------------------------|
  |  SyntaxEmptyStatement - 0.33 (8 bytes)  |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  |-------|----------------|---------|------|
*/
enum an_ifc_syntax_empty_statement_part : uint8_t {};
using an_ifc_syntax_empty_statement_storage =
                                         an_ifc_syntax_empty_statement_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_empty_statement_bytes =
                                  const an_ifc_syntax_empty_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_empty_statement_bytes =
                                         an_ifc_syntax_empty_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_empty_statement =
                     an_ifc_Byte_buffer<an_ifc_syntax_empty_statement_storage>;


/*
  |-----------------------------------------------|
  |     SyntaxEnumSpecifier - 0.33 (56 bytes)     |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | name        | ExprIndex      | 0.33    | 4    |
  | class_key   | KeywordSyntax  | 0.33    | 12   |
  | enumerators | SyntaxIndex    | 0.33    | 4    |
  | base        | SyntaxIndex    | 0.33    | 4    |
  | locus       | SourceLocation | 0.33    | 8    |
  | colon       | SourceLocation | 0.33    | 8    |
  | left_brace  | SourceLocation | 0.33    | 8    |
  | right_brace | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_enum_specifier_part : uint8_t {};
using an_ifc_syntax_enum_specifier_storage =
                                         an_ifc_syntax_enum_specifier_part[56];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_enum_specifier_bytes =
                                   const an_ifc_syntax_enum_specifier_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_enum_specifier_bytes =
                                          an_ifc_syntax_enum_specifier_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_enum_specifier =
                      an_ifc_Byte_buffer<an_ifc_syntax_enum_specifier_storage>;


/*
  |-----------------------------------------------|
  | SyntaxEnumeratorDefinition - 0.33 (32 bytes)  |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | name        | TextOffset     | 0.33    | 4    |
  | initializer | ExprIndex      | 0.33    | 4    |
  | locus       | SourceLocation | 0.33    | 8    |
  | equal       | SourceLocation | 0.33    | 8    |
  | comma       | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_enumerator_definition_part : uint8_t {};
using an_ifc_syntax_enumerator_definition_storage =
                                  an_ifc_syntax_enumerator_definition_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_enumerator_definition_bytes =
                            const an_ifc_syntax_enumerator_definition_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_enumerator_definition_bytes =
                                   an_ifc_syntax_enumerator_definition_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_enumerator_definition =
               an_ifc_Byte_buffer<an_ifc_syntax_enumerator_definition_storage>;


/*
  |---------------------------------------------------|
  |   SyntaxExceptionDeclaration - 0.33 (24 bytes)    |
  |-----------------|----------------|---------|------|
  | Name            | Type           | Version | Size |
  |-----------------|----------------|---------|------|
  | type_specifiers | SyntaxIndex    | 0.33    | 4    |
  | declarator      | SyntaxIndex    | 0.33    | 4    |
  | locus           | SourceLocation | 0.33    | 8    |
  | ellipsis        | SourceLocation | 0.33    | 8    |
  |-----------------|----------------|---------|------|
*/
enum an_ifc_syntax_exception_declaration_part : uint8_t {};
using an_ifc_syntax_exception_declaration_storage =
                                  an_ifc_syntax_exception_declaration_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_exception_declaration_bytes =
                            const an_ifc_syntax_exception_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_exception_declaration_bytes =
                                   an_ifc_syntax_exception_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_exception_declaration =
               an_ifc_Byte_buffer<an_ifc_syntax_exception_declaration_storage>;


/*
  |-----------------------------------------------|
  |   SyntaxExplicitSpecifier - 0.33 (28 bytes)   |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | condition   | ExprIndex      | 0.33    | 4    |
  | locus       | SourceLocation | 0.33    | 8    |
  | left_paren  | SourceLocation | 0.33    | 8    |
  | right_paren | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_explicit_specifier_part : uint8_t {};
using an_ifc_syntax_explicit_specifier_storage =
                                     an_ifc_syntax_explicit_specifier_part[28];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_explicit_specifier_bytes =
                               const an_ifc_syntax_explicit_specifier_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_explicit_specifier_bytes =
                                      an_ifc_syntax_explicit_specifier_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_explicit_specifier =
                  an_ifc_Byte_buffer<an_ifc_syntax_explicit_specifier_storage>;


/*
  |-----------------------------------------|
  |    SyntaxExpression - 0.33 (4 bytes)    |
  |------------|-----------|---------|------|
  | Name       | Type      | Version | Size |
  |------------|-----------|---------|------|
  | expression | ExprIndex | 0.33    | 4    |
  |------------|-----------|---------|------|
*/
enum an_ifc_syntax_expression_part : uint8_t {};
using an_ifc_syntax_expression_storage = an_ifc_syntax_expression_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_expression_bytes = const an_ifc_syntax_expression_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_expression_bytes = an_ifc_syntax_expression_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_expression =
                          an_ifc_Byte_buffer<an_ifc_syntax_expression_storage>;


/*
  |---------------------------------------------|
  | SyntaxExpressionStatement - 0.33 (16 bytes) |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | pragma    | SentenceIndex  | 0.33    | 4    |
  | expr      | ExprIndex      | 0.33    | 4    |
  | semicolon | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_expression_statement_part : uint8_t {};
using an_ifc_syntax_expression_statement_storage =
                                   an_ifc_syntax_expression_statement_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_expression_statement_bytes =
                             const an_ifc_syntax_expression_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_expression_statement_bytes =
                                    an_ifc_syntax_expression_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_expression_statement =
                an_ifc_Byte_buffer<an_ifc_syntax_expression_statement_storage>;


/*
  |--------------------------------------------|
  | SyntaxForRangeDeclaration - 0.33 (8 bytes) |
  |-------------|-------------|---------|------|
  | Name        | Type        | Version | Size |
  |-------------|-------------|---------|------|
  | specifiers  | SyntaxIndex | 0.33    | 4    |
  | declarator  | SyntaxIndex | 0.33    | 4    |
  |-------------|-------------|---------|------|
*/
enum an_ifc_syntax_for_range_declaration_part : uint8_t {};
using an_ifc_syntax_for_range_declaration_storage =
                                   an_ifc_syntax_for_range_declaration_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_for_range_declaration_bytes =
                            const an_ifc_syntax_for_range_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_for_range_declaration_bytes =
                                   an_ifc_syntax_for_range_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_for_range_declaration =
               an_ifc_Byte_buffer<an_ifc_syntax_for_range_declaration_storage>;


/*
  |--------------------------------------------------|
  |       SyntaxForStatement - 0.33 (52 bytes)       |
  |----------------|----------------|---------|------|
  | Name           | Type           | Version | Size |
  |----------------|----------------|---------|------|
  | pragma         | SentenceIndex  | 0.33    | 4    |
  | initialization | SyntaxIndex    | 0.33    | 4    |
  | condition      | ExprIndex      | 0.33    | 4    |
  | continuation   | ExprIndex      | 0.33    | 4    |
  | body           | SyntaxIndex    | 0.33    | 4    |
  | for            | SourceLocation | 0.33    | 8    |
  | left_paren     | SourceLocation | 0.33    | 8    |
  | right_paren    | SourceLocation | 0.33    | 8    |
  | semicolon      | SourceLocation | 0.33    | 8    |
  |----------------|----------------|---------|------|
*/
enum an_ifc_syntax_for_statement_part : uint8_t {};
using an_ifc_syntax_for_statement_storage =
                                          an_ifc_syntax_for_statement_part[52];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_for_statement_bytes =
                                    const an_ifc_syntax_for_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_for_statement_bytes = an_ifc_syntax_for_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_for_statement =
                       an_ifc_Byte_buffer<an_ifc_syntax_for_statement_storage>;


/*
  |------------------------------------------------|
  |      SyntaxFunctionBody - 0.33 (40 bytes)      |
  |--------------|----------------|---------|------|
  | Name         | Type           | Version | Size |
  |--------------|----------------|---------|------|
  | stmts        | SyntaxIndex    | 0.33    | 4    |
  | try_block    | SyntaxIndex    | 0.33    | 4    |
  | initializers | SyntaxIndex    | 0.33    | 4    |
  | generate     | KeywordSyntax  | 0.33    | 12   |
  | assign       | SourceLocation | 0.33    | 8    |
  | semicolon    | SourceLocation | 0.33    | 8    |
  |--------------|----------------|---------|------|
*/
enum an_ifc_syntax_function_body_part : uint8_t {};
using an_ifc_syntax_function_body_storage =
                                          an_ifc_syntax_function_body_part[40];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_function_body_bytes =
                                    const an_ifc_syntax_function_body_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_function_body_bytes = an_ifc_syntax_function_body_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_function_body =
                       an_ifc_Byte_buffer<an_ifc_syntax_function_body_storage>;


/*
  |-----------------------------------------------|
  |  SyntaxFunctionDeclarator - 0.33 (44 bytes)   |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | parameters  | SyntaxIndex    | 0.33    | 4    |
  | eh_spec     | SyntaxIndex    | 0.33    | 4    |
  | left_paren  | SourceLocation | 0.33    | 8    |
  | right_paren | SourceLocation | 0.33    | 8    |
  | __padding__ | uint8_t[20]    |         | 20   |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_function_declarator_part : uint8_t {};
using an_ifc_syntax_function_declarator_storage =
                                    an_ifc_syntax_function_declarator_part[44];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_function_declarator_bytes =
                              const an_ifc_syntax_function_declarator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_function_declarator_bytes =
                                     an_ifc_syntax_function_declarator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_function_declarator =
                 an_ifc_Byte_buffer<an_ifc_syntax_function_declarator_storage>;


/*
  |------------------------------------------------|
  |   SyntaxFunctionDefinition - 0.33 (40 bytes)   |
  |--------------|----------------|---------|------|
  | Name         | Type           | Version | Size |
  |--------------|----------------|---------|------|
  | stmts        | SyntaxIndex    | 0.33    | 4    |
  | try_block    | SyntaxIndex    | 0.33    | 4    |
  | initializers | SyntaxIndex    | 0.33    | 4    |
  | synthesis    | KeywordSyntax  | 0.33    | 12   |
  | assign       | SourceLocation | 0.33    | 8    |
  | semicolon    | SourceLocation | 0.33    | 8    |
  |--------------|----------------|---------|------|
*/
enum an_ifc_syntax_function_definition_part : uint8_t {};
using an_ifc_syntax_function_definition_storage =
                                    an_ifc_syntax_function_definition_part[40];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_function_definition_bytes =
                              const an_ifc_syntax_function_definition_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_function_definition_bytes =
                                     an_ifc_syntax_function_definition_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_function_definition =
                 an_ifc_Byte_buffer<an_ifc_syntax_function_definition_storage>;


/*
  |---------------------------------------------|
  |  SyntaxFunctionTryBlock - 0.33 (12 bytes)   |
  |--------------|-------------|---------|------|
  | Name         | Type        | Version | Size |
  |--------------|-------------|---------|------|
  | body         | SyntaxIndex | 0.33    | 4    |
  | handlers     | SyntaxIndex | 0.33    | 4    |
  | initializers | SyntaxIndex | 0.33    | 4    |
  |--------------|-------------|---------|------|
*/
enum an_ifc_syntax_function_try_block_part : uint8_t {};
using an_ifc_syntax_function_try_block_storage =
                                     an_ifc_syntax_function_try_block_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_function_try_block_bytes =
                               const an_ifc_syntax_function_try_block_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_function_try_block_bytes =
                                      an_ifc_syntax_function_try_block_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_function_try_block =
                  an_ifc_Byte_buffer<an_ifc_syntax_function_try_block_storage>;


/*
  |---------------------------------------------|
  |    SyntaxGotoStatement - 0.33 (32 bytes)    |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | pragma    | SentenceIndex  | 0.33    | 4    |
  | target    | TextOffset     | 0.33    | 4    |
  | locus     | SourceLocation | 0.33    | 8    |
  | label     | SourceLocation | 0.33    | 8    |
  | semicolon | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_goto_statement_part : uint8_t {};
using an_ifc_syntax_goto_statement_storage =
                                         an_ifc_syntax_goto_statement_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_goto_statement_bytes =
                                   const an_ifc_syntax_goto_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_goto_statement_bytes =
                                          an_ifc_syntax_goto_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_goto_statement =
                      an_ifc_Byte_buffer<an_ifc_syntax_goto_statement_storage>;


/*
  |-----------------------------------------------|
  |        SyntaxHandler - 0.33 (36 bytes)        |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | pragma      | SentenceIndex  | 0.33    | 4    |
  | exception   | SyntaxIndex    | 0.33    | 4    |
  | body        | SyntaxIndex    | 0.33    | 4    |
  | catch       | SourceLocation | 0.33    | 8    |
  | left_paren  | SourceLocation | 0.33    | 8    |
  | right_paren | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_handler_part : uint8_t {};
using an_ifc_syntax_handler_storage = an_ifc_syntax_handler_part[36];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_handler_bytes = const an_ifc_syntax_handler_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_handler_bytes = an_ifc_syntax_handler_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_handler =
                             an_ifc_Byte_buffer<an_ifc_syntax_handler_storage>;


/*
  |-----------------------------------------|
  |    SyntaxHandlerSeq - 0.33 (4 bytes)    |
  |----------|-------------|---------|------|
  | Name     | Type        | Version | Size |
  |----------|-------------|---------|------|
  | handlers | SyntaxIndex | 0.33    | 4    |
  |----------|-------------|---------|------|
*/
enum an_ifc_syntax_handler_seq_part : uint8_t {};
using an_ifc_syntax_handler_seq_storage = an_ifc_syntax_handler_seq_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_handler_seq_bytes =
                                      const an_ifc_syntax_handler_seq_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_handler_seq_bytes = an_ifc_syntax_handler_seq_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_handler_seq =
                         an_ifc_Byte_buffer<an_ifc_syntax_handler_seq_storage>;


/*
  |-------------------------------------------------------|
  |          SyntaxIfStatement - 0.33 (48 bytes)          |
  |---------------------|----------------|---------|------|
  | Name                | Type           | Version | Size |
  |---------------------|----------------|---------|------|
  | pragma              | SentenceIndex  | 0.33    | 4    |
  | initialization      | SyntaxIndex    | 0.33    | 4    |
  | condition_as_syntax | SyntaxIndex    | 0.33    | 4    |
  | condition_as_expr   | ExprIndex      | 0.33    | 4    |
  | consequence         | SyntaxIndex    | 0.33    | 4    |
  | alternative         | SyntaxIndex    | 0.33    | 4    |
  | if                  | SourceLocation | 0.33    | 8    |
  | constexpr           | SourceLocation | 0.33    | 8    |
  | else                | SourceLocation | 0.33    | 8    |
  |---------------------|----------------|---------|------|
*/
enum an_ifc_syntax_if_statement_part : uint8_t {};
using an_ifc_syntax_if_statement_storage = an_ifc_syntax_if_statement_part[48];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_if_statement_bytes =
                                     const an_ifc_syntax_if_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_if_statement_bytes = an_ifc_syntax_if_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_if_statement =
                        an_ifc_Byte_buffer<an_ifc_syntax_if_statement_storage>;


/*
  |-----------------------------------------------|
  |      SyntaxInitCapture - 0.33 (32 bytes)      |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | name        | ExprIndex      | 0.33    | 4    |
  | initializer | ExprIndex      | 0.33    | 4    |
  | expander    | SourceLocation | 0.33    | 8    |
  | ampersand   | SourceLocation | 0.33    | 8    |
  | comma       | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_init_capture_part : uint8_t {};
using an_ifc_syntax_init_capture_storage = an_ifc_syntax_init_capture_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_init_capture_bytes =
                                     const an_ifc_syntax_init_capture_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_init_capture_bytes = an_ifc_syntax_init_capture_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_init_capture =
                        an_ifc_Byte_buffer<an_ifc_syntax_init_capture_storage>;


/*
  |-----------------------------------------------|
  |    SyntaxInitDeclarator - 0.33 (20 bytes)     |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | declarator  | SyntaxIndex    | 0.33    | 4    |
  | constraint  | SyntaxIndex    | 0.33    | 4    |
  | initializer | ExprIndex      | 0.33    | 4    |
  | comma       | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_init_declarator_part : uint8_t {};
using an_ifc_syntax_init_declarator_storage =
                                        an_ifc_syntax_init_declarator_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_init_declarator_bytes =
                                  const an_ifc_syntax_init_declarator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_init_declarator_bytes =
                                         an_ifc_syntax_init_declarator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_init_declarator =
                     an_ifc_Byte_buffer<an_ifc_syntax_init_declarator_storage>;


/*
  |-----------------------------------------|
  |  SyntaxInitStatement - 0.33 (8 bytes)   |
  |--------|---------------|---------|------|
  | Name   | Type          | Version | Size |
  |--------|---------------|---------|------|
  | pragma | SentenceIndex | 0.33    | 4    |
  | init   | SyntaxIndex   | 0.33    | 4    |
  |--------|---------------|---------|------|
*/
enum an_ifc_syntax_init_statement_part : uint8_t {};
using an_ifc_syntax_init_statement_storage =
                                          an_ifc_syntax_init_statement_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_init_statement_bytes =
                                   const an_ifc_syntax_init_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_init_statement_bytes =
                                          an_ifc_syntax_init_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_init_statement =
                      an_ifc_Byte_buffer<an_ifc_syntax_init_statement_storage>;


/*
  |------------------------------------------|
  | SyntaxLabeledStatement - 0.33 (20 bytes) |
  |---------|---------------|---------|------|
  | Name    | Type          | Version | Size |
  |---------|---------------|---------|------|
  | pragma  | SentenceIndex | 0.33    | 4    |
  | label   | ExprIndex     | 0.33    | 4    |
  | stmt    | SyntaxIndex   | 0.33    | 4    |
  | locus   | KeywordSort   | 0.33    | 4    |
  | sort    | LabelSort     | 0.33    | 4    |
  |---------|---------------|---------|------|
*/
enum an_ifc_syntax_labeled_statement_part : uint8_t {};
using an_ifc_syntax_labeled_statement_storage =
                                      an_ifc_syntax_labeled_statement_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_labeled_statement_bytes =
                                const an_ifc_syntax_labeled_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_labeled_statement_bytes =
                                       an_ifc_syntax_labeled_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_labeled_statement =
                   an_ifc_Byte_buffer<an_ifc_syntax_labeled_statement_storage>;


/*
  |---------------------------------------------------|
  |     SyntaxLambdaDeclarator - 0.33 (40 bytes)      |
  |-----------------|----------------|---------|------|
  | Name            | Type           | Version | Size |
  |-----------------|----------------|---------|------|
  | parameters      | SyntaxIndex    | 0.33    | 4    |
  | eh_spec         | SyntaxIndex    | 0.33    | 4    |
  | trailing_target | SyntaxIndex    | 0.33    | 4    |
  | modifier        | KeywordSort    | 0.33    | 4    |
  | left_paren      | SourceLocation | 0.33    | 8    |
  | right_paren     | SourceLocation | 0.33    | 8    |
  | expander        | SourceLocation | 0.33    | 8    |
  |-----------------|----------------|---------|------|
*/
enum an_ifc_syntax_lambda_declarator_part : uint8_t {};
using an_ifc_syntax_lambda_declarator_storage =
                                      an_ifc_syntax_lambda_declarator_part[40];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_lambda_declarator_bytes =
                                const an_ifc_syntax_lambda_declarator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_lambda_declarator_bytes =
                                       an_ifc_syntax_lambda_declarator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_lambda_declarator =
                   an_ifc_Byte_buffer<an_ifc_syntax_lambda_declarator_storage>;


/*
  |-------------------------------------------------|
  |    SyntaxLambdaIntroducer - 0.33 (20 bytes)     |
  |---------------|----------------|---------|------|
  | Name          | Type           | Version | Size |
  |---------------|----------------|---------|------|
  | captures      | SyntaxIndex    | 0.33    | 4    |
  | left_bracket  | SourceLocation | 0.33    | 8    |
  | right_bracket | SourceLocation | 0.33    | 8    |
  |---------------|----------------|---------|------|
*/
enum an_ifc_syntax_lambda_introducer_part : uint8_t {};
using an_ifc_syntax_lambda_introducer_storage =
                                      an_ifc_syntax_lambda_introducer_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_lambda_introducer_bytes =
                                const an_ifc_syntax_lambda_introducer_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_lambda_introducer_bytes =
                                       an_ifc_syntax_lambda_introducer_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_lambda_introducer =
                   an_ifc_Byte_buffer<an_ifc_syntax_lambda_introducer_storage>;


/*
  |-----------------------------------------------|
  |    SyntaxMemInitializer - 0.33 (24 bytes)     |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | member      | ExprIndex      | 0.33    | 4    |
  | initializer | ExprIndex      | 0.33    | 4    |
  | expander    | SourceLocation | 0.33    | 8    |
  | comma       | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_mem_initializer_part : uint8_t {};
using an_ifc_syntax_mem_initializer_storage =
                                        an_ifc_syntax_mem_initializer_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_mem_initializer_bytes =
                                  const an_ifc_syntax_mem_initializer_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_mem_initializer_bytes =
                                         an_ifc_syntax_mem_initializer_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_mem_initializer =
                     an_ifc_Byte_buffer<an_ifc_syntax_mem_initializer_storage>;


/*
  |---------------------------------------------------|
  |     SyntaxMemberDeclaration - 0.33 (16 bytes)     |
  |-----------------|----------------|---------|------|
  | Name            | Type           | Version | Size |
  |-----------------|----------------|---------|------|
  | decl_specifiers | SyntaxIndex    | 0.33    | 4    |
  | declarations    | SyntaxIndex    | 0.33    | 4    |
  | semicolon       | SourceLocation | 0.33    | 8    |
  |-----------------|----------------|---------|------|
*/
enum an_ifc_syntax_member_declaration_part : uint8_t {};
using an_ifc_syntax_member_declaration_storage =
                                     an_ifc_syntax_member_declaration_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_member_declaration_bytes =
                               const an_ifc_syntax_member_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_member_declaration_bytes =
                                      an_ifc_syntax_member_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_member_declaration =
                  an_ifc_Byte_buffer<an_ifc_syntax_member_declaration_storage>;


/*
  |-----------------------------------------------|
  |   SyntaxMemberDeclarator - 0.33 (40 bytes)    |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | declarator  | SyntaxIndex    | 0.33    | 4    |
  | constraint  | SyntaxIndex    | 0.33    | 4    |
  | bitwidth    | ExprIndex      | 0.33    | 4    |
  | initializer | ExprIndex      | 0.33    | 4    |
  | locus       | SourceLocation | 0.33    | 8    |
  | colon       | SourceLocation | 0.33    | 8    |
  | comma       | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_member_declarator_part : uint8_t {};
using an_ifc_syntax_member_declarator_storage =
                                      an_ifc_syntax_member_declarator_part[40];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_member_declarator_bytes =
                                const an_ifc_syntax_member_declarator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_member_declarator_bytes =
                                       an_ifc_syntax_member_declarator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_member_declarator =
                   an_ifc_Byte_buffer<an_ifc_syntax_member_declarator_storage>;


/*
  |--------------------------------------------------|
  | SyntaxMemberFunctionDeclaration - 0.33 (4 bytes) |
  |----------------|--------------|----------|-------|
  | Name           | Type         | Version  | Size  |
  |----------------|--------------|----------|-------|
  | definition     | SyntaxIndex  | 0.33     | 4     |
  |----------------|--------------|----------|-------|
*/
enum an_ifc_syntax_member_function_declaration_part : uint8_t {};
using an_ifc_syntax_member_function_declaration_storage =
                             an_ifc_syntax_member_function_declaration_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_member_function_declaration_bytes =
                      const an_ifc_syntax_member_function_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_member_function_declaration_bytes =
                             an_ifc_syntax_member_function_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_member_function_declaration =
         an_ifc_Byte_buffer<an_ifc_syntax_member_function_declaration_storage>;


/*
  |----------------------------------------------------|
  |     SyntaxMemberSpecification - 0.33 (4 bytes)     |
  |---------------------|-------------|---------|------|
  | Name                | Type        | Version | Size |
  |---------------------|-------------|---------|------|
  | member_declarations | SyntaxIndex | 0.33    | 4    |
  |---------------------|-------------|---------|------|
*/
enum an_ifc_syntax_member_specification_part : uint8_t {};
using an_ifc_syntax_member_specification_storage =
                                    an_ifc_syntax_member_specification_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_member_specification_bytes =
                             const an_ifc_syntax_member_specification_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_member_specification_bytes =
                                    an_ifc_syntax_member_specification_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_member_specification =
                an_ifc_Byte_buffer<an_ifc_syntax_member_specification_storage>;


/*
  |--------------------------------------------------|
  | SyntaxNamespaceAliasDefinition - 0.33 (32 bytes) |
  |----------------|----------------|---------|------|
  | Name           | Type           | Version | Size |
  |----------------|----------------|---------|------|
  | name           | ExprIndex      | 0.33    | 4    |
  | target         | ExprIndex      | 0.33    | 4    |
  | namespace_kw   | SourceLocation | 0.33    | 8    |
  | assign         | SourceLocation | 0.33    | 8    |
  | semicolon      | SourceLocation | 0.33    | 8    |
  |----------------|----------------|---------|------|
*/
enum an_ifc_syntax_namespace_alias_definition_part : uint8_t {};
using an_ifc_syntax_namespace_alias_definition_storage =
                             an_ifc_syntax_namespace_alias_definition_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_namespace_alias_definition_bytes =
                       const an_ifc_syntax_namespace_alias_definition_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_namespace_alias_definition_bytes =
                              an_ifc_syntax_namespace_alias_definition_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_namespace_alias_definition =
          an_ifc_Byte_buffer<an_ifc_syntax_namespace_alias_definition_storage>;


/*
  |---------------------------------------------|
  |  SyntaxNestedRequirement - 0.33 (12 bytes)  |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | condition | ExprIndex      | 0.33    | 4    |
  | locus     | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_nested_requirement_part : uint8_t {};
using an_ifc_syntax_nested_requirement_storage =
                                     an_ifc_syntax_nested_requirement_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_nested_requirement_bytes =
                               const an_ifc_syntax_nested_requirement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_nested_requirement_bytes =
                                      an_ifc_syntax_nested_requirement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_nested_requirement =
                  an_ifc_Byte_buffer<an_ifc_syntax_nested_requirement_storage>;


/*
  |-------------------------------------------|
  |   SyntaxNewDeclarator - 0.33 (4 bytes)    |
  |------------|-------------|---------|------|
  | Name       | Type        | Version | Size |
  |------------|-------------|---------|------|
  | declarator | SyntaxIndex | 0.33    | 4    |
  |------------|-------------|---------|------|
*/
enum an_ifc_syntax_new_declarator_part : uint8_t {};
using an_ifc_syntax_new_declarator_storage =
                                          an_ifc_syntax_new_declarator_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_new_declarator_bytes =
                                   const an_ifc_syntax_new_declarator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_new_declarator_bytes =
                                          an_ifc_syntax_new_declarator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_new_declarator =
                      an_ifc_Byte_buffer<an_ifc_syntax_new_declarator_storage>;


/*
  |-----------------------------------------------|
  | SyntaxNoexceptSpecification - 0.33 (28 bytes) |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | expr        | SyntaxIndex    | 0.33    | 4    |
  | locus       | SourceLocation | 0.33    | 8    |
  | left_paren  | SourceLocation | 0.33    | 8    |
  | right_paren | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_noexcept_specification_part : uint8_t {};
using an_ifc_syntax_noexcept_specification_storage =
                                 an_ifc_syntax_noexcept_specification_part[28];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_noexcept_specification_bytes =
                           const an_ifc_syntax_noexcept_specification_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_noexcept_specification_bytes =
                                  an_ifc_syntax_noexcept_specification_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_noexcept_specification =
              an_ifc_Byte_buffer<an_ifc_syntax_noexcept_specification_storage>;


/*
  |-------------------------------------------------|
  | SyntaxNonTypeTemplateArgument - 0.33 (20 bytes) |
  |------------|-----------------|----------|-------|
  | Name       | Type            | Version  | Size  |
  |------------|-----------------|----------|-------|
  | argument   | ExprIndex       | 0.33     | 4     |
  | ellipsis   | SourceLocation  | 0.33     | 8     |
  | comma      | SourceLocation  | 0.33     | 8     |
  |------------|-----------------|----------|-------|
*/
enum an_ifc_syntax_non_type_template_argument_part : uint8_t {};
using an_ifc_syntax_non_type_template_argument_storage =
                             an_ifc_syntax_non_type_template_argument_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_non_type_template_argument_bytes =
                       const an_ifc_syntax_non_type_template_argument_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_non_type_template_argument_bytes =
                              an_ifc_syntax_non_type_template_argument_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_non_type_template_argument =
          an_ifc_Byte_buffer<an_ifc_syntax_non_type_template_argument_storage>;


/*
  |---------------------------------------------------|
  |    SyntaxParameterDeclarator - 0.33 (21 bytes)    |
  |-----------------|----------------|---------|------|
  | Name            | Type           | Version | Size |
  |-----------------|----------------|---------|------|
  | decl_specifiers | SyntaxIndex    | 0.33    | 4    |
  | declarator      | SyntaxIndex    | 0.33    | 4    |
  | default_expr    | ExprIndex      | 0.33    | 4    |
  | locus           | SourceLocation | 0.33    | 8    |
  | sort            | ParameterSort  | 0.33    | 1    |
  |-----------------|----------------|---------|------|
*/
enum an_ifc_syntax_parameter_declarator_part : uint8_t {};
using an_ifc_syntax_parameter_declarator_storage =
                                   an_ifc_syntax_parameter_declarator_part[21];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_parameter_declarator_bytes =
                             const an_ifc_syntax_parameter_declarator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_parameter_declarator_bytes =
                                    an_ifc_syntax_parameter_declarator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_parameter_declarator =
                an_ifc_Byte_buffer<an_ifc_syntax_parameter_declarator_storage>;


/*
  |--------------------------------------------------|
  | SyntaxPlaceholderTypeSpecifier - 0.33 (21 bytes) |
  |-------------|-----------------|----------|-------|
  | Name        | Type            | Version  | Size  |
  |-------------|-----------------|----------|-------|
  | constraint  | ExprIndex       | 0.33     | 4     |
  | basis       | TypeBasisSort   | 0.33     | 1     |
  | keyword     | SourceLocation  | 0.33     | 8     |
  | locus       | SourceLocation  | 0.33     | 8     |
  |-------------|-----------------|----------|-------|
*/
enum an_ifc_syntax_placeholder_type_specifier_part : uint8_t {};
using an_ifc_syntax_placeholder_type_specifier_storage =
                             an_ifc_syntax_placeholder_type_specifier_part[21];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_placeholder_type_specifier_bytes =
                       const an_ifc_syntax_placeholder_type_specifier_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_placeholder_type_specifier_bytes =
                              an_ifc_syntax_placeholder_type_specifier_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_placeholder_type_specifier =
          an_ifc_Byte_buffer<an_ifc_syntax_placeholder_type_specifier_storage>;


/*
  |-----------------------------------------------------|
  |      SyntaxPointerDeclarator - 0.33 (20 bytes)      |
  |------------|-----------------------|---------|------|
  | Name       | Type                  | Version | Size |
  |------------|-----------------------|---------|------|
  | whole      | SyntaxIndex           | 0.33    | 4    |
  | next       | SyntaxIndex           | 0.33    | 4    |
  | locus      | SourceLocation        | 0.33    | 8    |
  | sort       | PointerDeclaratorSort | 0.33    | 1    |
  | qualifiers | QualifierBitfield     | 0.33    | 1    |
  | convention | CallingConventionSort | 0.33    | 1    |
  | callable   | bool                  | 0.33    | 1    |
  |------------|-----------------------|---------|------|
*/
enum an_ifc_syntax_pointer_declarator_part : uint8_t {};
using an_ifc_syntax_pointer_declarator_storage =
                                     an_ifc_syntax_pointer_declarator_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_pointer_declarator_bytes =
                               const an_ifc_syntax_pointer_declarator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_pointer_declarator_bytes =
                                      an_ifc_syntax_pointer_declarator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_pointer_declarator =
                  an_ifc_Byte_buffer<an_ifc_syntax_pointer_declarator_storage>;


/*
  |------------------------------------------------|
  | SyntaxRangeBasedForStatement - 0.33 (52 bytes) |
  |--------------|----------------|---------|------|
  | Name         | Type           | Version | Size |
  |--------------|----------------|---------|------|
  | pragma       | SentenceIndex  | 0.33    | 4    |
  | init         | SyntaxIndex    | 0.33    | 4    |
  | decl         | SyntaxIndex    | 0.33    | 4    |
  | initializer  | SyntaxIndex    | 0.33    | 4    |
  | body         | SyntaxIndex    | 0.33    | 4    |
  | for          | SourceLocation | 0.33    | 8    |
  | left_paren   | SourceLocation | 0.33    | 8    |
  | right_paren  | SourceLocation | 0.33    | 8    |
  | colon        | SourceLocation | 0.33    | 8    |
  |--------------|----------------|---------|------|
*/
enum an_ifc_syntax_range_based_for_statement_part : uint8_t {};
using an_ifc_syntax_range_based_for_statement_storage =
                              an_ifc_syntax_range_based_for_statement_part[52];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_range_based_for_statement_bytes =
                        const an_ifc_syntax_range_based_for_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_range_based_for_statement_bytes =
                               an_ifc_syntax_range_based_for_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_range_based_for_statement =
           an_ifc_Byte_buffer<an_ifc_syntax_range_based_for_statement_storage>;


/*
  |------------------------------------------------|
  |    SyntaxRequirementBody - 0.33 (20 bytes)     |
  |--------------|----------------|---------|------|
  | Name         | Type           | Version | Size |
  |--------------|----------------|---------|------|
  | requirements | SyntaxIndex    | 0.33    | 4    |
  | locus        | SourceLocation | 0.33    | 8    |
  | right_curly  | SourceLocation | 0.33    | 8    |
  |--------------|----------------|---------|------|
*/
enum an_ifc_syntax_requirement_body_part : uint8_t {};
using an_ifc_syntax_requirement_body_storage =
                                       an_ifc_syntax_requirement_body_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_requirement_body_bytes =
                                 const an_ifc_syntax_requirement_body_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_requirement_body_bytes =
                                        an_ifc_syntax_requirement_body_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_requirement_body =
                    an_ifc_Byte_buffer<an_ifc_syntax_requirement_body_storage>;


/*
  |---------------------------------------------|
  |   SyntaxRequiresClause - 0.33 (12 bytes)    |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | condition | ExprIndex      | 0.33    | 4    |
  | locus     | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_requires_clause_part : uint8_t {};
using an_ifc_syntax_requires_clause_storage =
                                        an_ifc_syntax_requires_clause_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_requires_clause_bytes =
                                  const an_ifc_syntax_requires_clause_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_requires_clause_bytes =
                                         an_ifc_syntax_requires_clause_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_requires_clause =
                     an_ifc_Byte_buffer<an_ifc_syntax_requires_clause_storage>;


/*
  |---------------------------------------------|
  |   SyntaxReturnStatement - 0.33 (25 bytes)   |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | pragma    | SentenceIndex  | 0.33    | 4    |
  | expr      | ExprIndex      | 0.33    | 4    |
  | sort      | ReturnSort     | 0.33    | 1    |
  | return    | SourceLocation | 0.33    | 8    |
  | semicolon | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_return_statement_part : uint8_t {};
using an_ifc_syntax_return_statement_storage =
                                       an_ifc_syntax_return_statement_part[25];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_return_statement_bytes =
                                 const an_ifc_syntax_return_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_return_statement_bytes =
                                        an_ifc_syntax_return_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_return_statement =
                    an_ifc_Byte_buffer<an_ifc_syntax_return_statement_storage>;


/*
  |-----------------------------------------------|
  |       SyntaxSEHExcept - 0.33 (32 bytes)       |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | condition   | ExprIndex      | 0.33    | 4    |
  | body        | SyntaxIndex    | 0.33    | 4    |
  | except_kw   | SourceLocation | 0.33    | 8    |
  | left_paren  | SourceLocation | 0.33    | 8    |
  | right_paren | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_seh_except_part : uint8_t {};
using an_ifc_syntax_seh_except_storage = an_ifc_syntax_seh_except_part[32];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_seh_except_bytes = const an_ifc_syntax_seh_except_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_seh_except_bytes = an_ifc_syntax_seh_except_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_seh_except =
                          an_ifc_Byte_buffer<an_ifc_syntax_seh_except_storage>;


/*
  |----------------------------------------------|
  |      SyntaxSEHFinally - 0.33 (12 bytes)      |
  |------------|----------------|---------|------|
  | Name       | Type           | Version | Size |
  |------------|----------------|---------|------|
  | body       | SyntaxIndex    | 0.33    | 4    |
  | finally_kw | SourceLocation | 0.33    | 8    |
  |------------|----------------|---------|------|
*/
enum an_ifc_syntax_seh_finally_part : uint8_t {};
using an_ifc_syntax_seh_finally_storage = an_ifc_syntax_seh_finally_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_seh_finally_bytes =
                                      const an_ifc_syntax_seh_finally_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_seh_finally_bytes = an_ifc_syntax_seh_finally_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_seh_finally =
                         an_ifc_Byte_buffer<an_ifc_syntax_seh_finally_storage>;


/*
  |---------------------------------------------|
  |      SyntaxSEHLeave - 0.33 (16 bytes)       |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | leave_kw  | SourceLocation | 0.33    | 8    |
  | semicolon | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_seh_leave_part : uint8_t {};
using an_ifc_syntax_seh_leave_storage = an_ifc_syntax_seh_leave_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_seh_leave_bytes = const an_ifc_syntax_seh_leave_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_seh_leave_bytes = an_ifc_syntax_seh_leave_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_seh_leave =
                           an_ifc_Byte_buffer<an_ifc_syntax_seh_leave_storage>;


/*
  |-------------------------------------------|
  |      SyntaxSEHTry - 0.33 (16 bytes)       |
  |---------|----------------|---------|------|
  | Name    | Type           | Version | Size |
  |---------|----------------|---------|------|
  | body    | SyntaxIndex    | 0.33    | 4    |
  | handler | SyntaxIndex    | 0.33    | 4    |
  | try_kw  | SourceLocation | 0.33    | 8    |
  |---------|----------------|---------|------|
*/
enum an_ifc_syntax_seh_try_part : uint8_t {};
using an_ifc_syntax_seh_try_storage = an_ifc_syntax_seh_try_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_seh_try_bytes = const an_ifc_syntax_seh_try_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_seh_try_bytes = an_ifc_syntax_seh_try_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_seh_try =
                             an_ifc_Byte_buffer<an_ifc_syntax_seh_try_storage>;


/*
  |---------------------------------------------|
  |    SyntaxSimpleCapture - 0.33 (28 bytes)    |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | name      | ExprIndex      | 0.33    | 4    |
  | ampersand | SourceLocation | 0.33    | 8    |
  | expander  | SourceLocation | 0.33    | 8    |
  | comma     | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_simple_capture_part : uint8_t {};
using an_ifc_syntax_simple_capture_storage =
                                         an_ifc_syntax_simple_capture_part[28];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_simple_capture_bytes =
                                   const an_ifc_syntax_simple_capture_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_simple_capture_bytes =
                                          an_ifc_syntax_simple_capture_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_simple_capture =
                      an_ifc_Byte_buffer<an_ifc_syntax_simple_capture_storage>;


/*
  |---------------------------------------------------|
  |     SyntaxSimpleDeclaration - 0.33 (24 bytes)     |
  |-----------------|----------------|---------|------|
  | Name            | Type           | Version | Size |
  |-----------------|----------------|---------|------|
  | decl_specifiers | SyntaxIndex    | 0.33    | 4    |
  | declarators     | SyntaxIndex    | 0.33    | 4    |
  | locus           | SourceLocation | 0.33    | 8    |
  | semicolon       | SourceLocation | 0.33    | 8    |
  |-----------------|----------------|---------|------|
*/
enum an_ifc_syntax_simple_declaration_part : uint8_t {};
using an_ifc_syntax_simple_declaration_storage =
                                     an_ifc_syntax_simple_declaration_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_simple_declaration_bytes =
                               const an_ifc_syntax_simple_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_simple_declaration_bytes =
                                      an_ifc_syntax_simple_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_simple_declaration =
                  an_ifc_Byte_buffer<an_ifc_syntax_simple_declaration_storage>;


/*
  |---------------------------------------------|
  |  SyntaxSimpleRequirement - 0.33 (12 bytes)  |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | condition | ExprIndex      | 0.33    | 4    |
  | locus     | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_simple_requirement_part : uint8_t {};
using an_ifc_syntax_simple_requirement_storage =
                                     an_ifc_syntax_simple_requirement_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_simple_requirement_bytes =
                               const an_ifc_syntax_simple_requirement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_simple_requirement_bytes =
                                      an_ifc_syntax_simple_requirement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_simple_requirement =
                  an_ifc_Byte_buffer<an_ifc_syntax_simple_requirement_storage>;


/*
  |---------------------------------------------|
  | SyntaxSimpleTypeSpecifier - 0.33 (16 bytes) |
  |--------|-----------------|----------|-------|
  | Name   | Type            | Version  | Size  |
  |--------|-----------------|----------|-------|
  | type   | TypeIndex       | 0.33     | 4     |
  | expr   | ExprIndex       | 0.33     | 4     |
  | locus  | SourceLocation  | 0.33     | 8     |
  |--------|-----------------|----------|-------|
*/
enum an_ifc_syntax_simple_type_specifier_part : uint8_t {};
using an_ifc_syntax_simple_type_specifier_storage =
                                  an_ifc_syntax_simple_type_specifier_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_simple_type_specifier_bytes =
                            const an_ifc_syntax_simple_type_specifier_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_simple_type_specifier_bytes =
                                   an_ifc_syntax_simple_type_specifier_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_simple_type_specifier =
               an_ifc_Byte_buffer<an_ifc_syntax_simple_type_specifier_storage>;


/*
  |--------------------------------------|
  | SyntaxStatementSeq - 0.33 (4 bytes)  |
  |-------|-------------|---------|------|
  | Name  | Type        | Version | Size |
  |-------|-------------|---------|------|
  | stmts | SyntaxIndex | 0.33    | 4    |
  |-------|-------------|---------|------|
*/
enum an_ifc_syntax_statement_seq_part : uint8_t {};
using an_ifc_syntax_statement_seq_storage =
                                           an_ifc_syntax_statement_seq_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_statement_seq_bytes =
                                    const an_ifc_syntax_statement_seq_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_statement_seq_bytes = an_ifc_syntax_statement_seq_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_statement_seq =
                       an_ifc_Byte_buffer<an_ifc_syntax_statement_seq_storage>;


/*
  |-------------------------------------------------|
  | SyntaxStaticAssertDeclaration - 0.33 (48 bytes) |
  |---------------|----------------|---------|------|
  | Name          | Type           | Version | Size |
  |---------------|----------------|---------|------|
  | condition     | ExprIndex      | 0.33    | 4    |
  | message       | ExprIndex      | 0.33    | 4    |
  | locus         | SourceLocation | 0.33    | 8    |
  | left_paren    | SourceLocation | 0.33    | 8    |
  | right_paren   | SourceLocation | 0.33    | 8    |
  | semicolon     | SourceLocation | 0.33    | 8    |
  | comma         | SourceLocation | 0.33    | 8    |
  |---------------|----------------|---------|------|
*/
enum an_ifc_syntax_static_assert_declaration_part : uint8_t {};
using an_ifc_syntax_static_assert_declaration_storage =
                              an_ifc_syntax_static_assert_declaration_part[48];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_static_assert_declaration_bytes =
                        const an_ifc_syntax_static_assert_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_static_assert_declaration_bytes =
                               an_ifc_syntax_static_assert_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_static_assert_declaration =
           an_ifc_Byte_buffer<an_ifc_syntax_static_assert_declaration_storage>;


/*
  |------------------------------------------------------|
  | SyntaxStructuredBindingDeclaration - 0.33 (28 bytes) |
  |-----------------|-----------------|----------|-------|
  | Name            | Type            | Version  | Size  |
  |-----------------|-----------------|----------|-------|
  | locus           | SourceLocation  | 0.33     | 8     |
  | ref             | SourceLocation  | 0.33     | 8     |
  | specifiers      | SyntaxIndex     | 0.33     | 4     |
  | names           | SyntaxIndex     | 0.33     | 4     |
  | initializer     | ExprIndex       | 0.33     | 4     |
  |-----------------|-----------------|----------|-------|
*/
enum an_ifc_syntax_structured_binding_declaration_part : uint8_t {};
using an_ifc_syntax_structured_binding_declaration_storage =
                         an_ifc_syntax_structured_binding_declaration_part[28];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_structured_binding_declaration_bytes =
                   const an_ifc_syntax_structured_binding_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_structured_binding_declaration_bytes =
                          an_ifc_syntax_structured_binding_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_structured_binding_declaration =
      an_ifc_Byte_buffer<an_ifc_syntax_structured_binding_declaration_storage>;


/*
  |-----------------------------------------------------|
  | SyntaxStructuredBindingIdentifier - 0.33 (12 bytes) |
  |----------|-------------------|------------|---------|
  | Name     | Type              | Version    | Size    |
  |----------|-------------------|------------|---------|
  | name     | ExprIndex         | 0.33       | 4       |
  | comma    | SourceLocation    | 0.33       | 8       |
  |----------|-------------------|------------|---------|
*/
enum an_ifc_syntax_structured_binding_identifier_part : uint8_t {};
using an_ifc_syntax_structured_binding_identifier_storage =
                          an_ifc_syntax_structured_binding_identifier_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_structured_binding_identifier_bytes =
                    const an_ifc_syntax_structured_binding_identifier_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_structured_binding_identifier_bytes =
                           an_ifc_syntax_structured_binding_identifier_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_structured_binding_identifier =
       an_ifc_Byte_buffer<an_ifc_syntax_structured_binding_identifier_storage>;


/*
  |-----------------------------------------|
  |      SyntaxSuper - 0.33 (8 bytes)       |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | locus | SourceLocation | 0.33    | 8    |
  |-------|----------------|---------|------|
*/
enum an_ifc_syntax_super_part : uint8_t {};
using an_ifc_syntax_super_storage = an_ifc_syntax_super_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_super_bytes = const an_ifc_syntax_super_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_super_bytes = an_ifc_syntax_super_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_super = an_ifc_Byte_buffer<an_ifc_syntax_super_storage>;


/*
  |---------------------------------------------|
  |   SyntaxSwitchStatement - 0.33 (24 bytes)   |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | pragma    | SentenceIndex  | 0.33    | 4    |
  | init      | SyntaxIndex    | 0.33    | 4    |
  | condition | SyntaxIndex    | 0.33    | 4    |
  | body      | SyntaxIndex    | 0.33    | 4    |
  | switch    | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_switch_statement_part : uint8_t {};
using an_ifc_syntax_switch_statement_storage =
                                       an_ifc_syntax_switch_statement_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_switch_statement_bytes =
                                 const an_ifc_syntax_switch_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_switch_statement_bytes =
                                        an_ifc_syntax_switch_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_switch_statement =
                    an_ifc_Byte_buffer<an_ifc_syntax_switch_statement_storage>;


/*
  |-----------------------------------------------|
  | SyntaxTemplateArgumentList - 0.33 (20 bytes)  |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | arguments   | SyntaxIndex    | 0.33    | 4    |
  | left_angle  | SourceLocation | 0.33    | 8    |
  | right_angle | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_template_argument_list_part : uint8_t {};
using an_ifc_syntax_template_argument_list_storage =
                                 an_ifc_syntax_template_argument_list_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_template_argument_list_bytes =
                           const an_ifc_syntax_template_argument_list_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_template_argument_list_bytes =
                                  an_ifc_syntax_template_argument_list_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_template_argument_list =
              an_ifc_Byte_buffer<an_ifc_syntax_template_argument_list_storage>;


/*
  |----------------------------------------------|
  | SyntaxTemplateDeclaration - 0.33 (16 bytes)  |
  |------------|----------------|---------|------|
  | Name       | Type           | Version | Size |
  |------------|----------------|---------|------|
  | parameters | SyntaxIndex    | 0.33    | 4    |
  | subject    | SyntaxIndex    | 0.33    | 4    |
  | locus      | SourceLocation | 0.33    | 8    |
  |------------|----------------|---------|------|
*/
enum an_ifc_syntax_template_declaration_part : uint8_t {};
using an_ifc_syntax_template_declaration_storage =
                                   an_ifc_syntax_template_declaration_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_template_declaration_bytes =
                             const an_ifc_syntax_template_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_template_declaration_bytes =
                                    an_ifc_syntax_template_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_template_declaration =
                an_ifc_Byte_buffer<an_ifc_syntax_template_declaration_storage>;


/*
  |-----------------------------------------------|
  |      SyntaxTemplateId - 0.33 (28 bytes)       |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | name        | SyntaxIndex    | 0.33    | 4    |
  | symbol      | ExprIndex      | 0.33    | 4    |
  | arguments   | SyntaxIndex    | 0.33    | 4    |
  | locus       | SourceLocation | 0.33    | 8    |
  | template_kw | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_template_id_part : uint8_t {};
using an_ifc_syntax_template_id_storage = an_ifc_syntax_template_id_part[28];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_template_id_bytes =
                                      const an_ifc_syntax_template_id_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_template_id_bytes = an_ifc_syntax_template_id_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_template_id =
                         an_ifc_Byte_buffer<an_ifc_syntax_template_id_storage>;


/*
  |-----------------------------------------------|
  | SyntaxTemplateParameterList - 0.33 (24 bytes) |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | parameters  | SyntaxIndex    | 0.33    | 4    |
  | clause      | SyntaxIndex    | 0.33    | 4    |
  | left_angle  | SourceLocation | 0.33    | 8    |
  | right_angle | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_template_parameter_list_part : uint8_t {};
using an_ifc_syntax_template_parameter_list_storage =
                                an_ifc_syntax_template_parameter_list_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_template_parameter_list_bytes =
                          const an_ifc_syntax_template_parameter_list_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_template_parameter_list_bytes =
                                 an_ifc_syntax_template_parameter_list_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_template_parameter_list =
             an_ifc_Byte_buffer<an_ifc_syntax_template_parameter_list_storage>;


/*
  |---------------------------------------------------|
  | SyntaxTemplateTemplateParameter - 0.33 (48 bytes) |
  |--------------|-----------------|----------|-------|
  | Name         | Type            | Version  | Size  |
  |--------------|-----------------|----------|-------|
  | name         | TextOffset      | 0.33     | 4     |
  | argument     | SyntaxIndex     | 0.33     | 4     |
  | parameters   | SyntaxIndex     | 0.33     | 4     |
  | locus        | SourceLocation  | 0.33     | 8     |
  | ellipsis     | SourceLocation  | 0.33     | 8     |
  | comma        | SourceLocation  | 0.33     | 8     |
  | key          | KeywordSyntax   | 0.33     | 12    |
  |--------------|-----------------|----------|-------|
*/
enum an_ifc_syntax_template_template_parameter_part : uint8_t {};
using an_ifc_syntax_template_template_parameter_storage =
                            an_ifc_syntax_template_template_parameter_part[48];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_template_template_parameter_bytes =
                      const an_ifc_syntax_template_template_parameter_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_template_template_parameter_bytes =
                             an_ifc_syntax_template_template_parameter_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_template_template_parameter =
         an_ifc_Byte_buffer<an_ifc_syntax_template_template_parameter_storage>;


/*
  |--------------------------------------------|
  |    SyntaxThisCapture - 0.33 (24 bytes)     |
  |----------|----------------|---------|------|
  | Name     | Type           | Version | Size |
  |----------|----------------|---------|------|
  | locus    | SourceLocation | 0.33    | 8    |
  | asterisk | SourceLocation | 0.33    | 8    |
  | comma    | SourceLocation | 0.33    | 8    |
  |----------|----------------|---------|------|
*/
enum an_ifc_syntax_this_capture_part : uint8_t {};
using an_ifc_syntax_this_capture_storage = an_ifc_syntax_this_capture_part[24];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_this_capture_bytes =
                                     const an_ifc_syntax_this_capture_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_this_capture_bytes = an_ifc_syntax_this_capture_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_this_capture =
                        an_ifc_Byte_buffer<an_ifc_syntax_this_capture_storage>;


/*
  |--------------------------------------------|
  | SyntaxTrailingReturnType - 0.33 (12 bytes) |
  |----------|----------------|---------|------|
  | Name     | Type           | Version | Size |
  |----------|----------------|---------|------|
  | target   | SyntaxIndex    | 0.33    | 4    |
  | arrow    | SourceLocation | 0.33    | 8    |
  |----------|----------------|---------|------|
*/
enum an_ifc_syntax_trailing_return_type_part : uint8_t {};
using an_ifc_syntax_trailing_return_type_storage =
                                   an_ifc_syntax_trailing_return_type_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_trailing_return_type_bytes =
                             const an_ifc_syntax_trailing_return_type_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_trailing_return_type_bytes =
                                    an_ifc_syntax_trailing_return_type_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_trailing_return_type =
                an_ifc_Byte_buffer<an_ifc_syntax_trailing_return_type_storage>;


/*
  |--------------------------------------------|
  |      SyntaxTryBlock - 0.33 (20 bytes)      |
  |----------|----------------|---------|------|
  | Name     | Type           | Version | Size |
  |----------|----------------|---------|------|
  | pragma   | SentenceIndex  | 0.33    | 4    |
  | body     | SyntaxIndex    | 0.33    | 4    |
  | handlers | SyntaxIndex    | 0.33    | 4    |
  | try      | SourceLocation | 0.33    | 8    |
  |----------|----------------|---------|------|
*/
enum an_ifc_syntax_try_block_part : uint8_t {};
using an_ifc_syntax_try_block_storage = an_ifc_syntax_try_block_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_try_block_bytes = const an_ifc_syntax_try_block_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_try_block_bytes = an_ifc_syntax_try_block_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_try_block =
                           an_ifc_Byte_buffer<an_ifc_syntax_try_block_storage>;


/*
  |--------------------------------------------|
  |        SyntaxTuple - 0.33 (8 bytes)        |
  |-------------|-------------|---------|------|
  | Name        | Type        | Version | Size |
  |-------------|-------------|---------|------|
  | start       | Index       | 0.33    | 4    |
  | cardinality | Cardinality | 0.33    | 4    |
  |-------------|-------------|---------|------|
*/
enum an_ifc_syntax_tuple_part : uint8_t {};
using an_ifc_syntax_tuple_storage = an_ifc_syntax_tuple_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_tuple_bytes = const an_ifc_syntax_tuple_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_tuple_bytes = an_ifc_syntax_tuple_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_tuple = an_ifc_Byte_buffer<an_ifc_syntax_tuple_storage>;


/*
  |-------------------------------------------------------|
  |            SyntaxTypeId - 0.33 (16 bytes)             |
  |---------------------|----------------|---------|------|
  | Name                | Type           | Version | Size |
  |---------------------|----------------|---------|------|
  | type_specifier      | SyntaxIndex    | 0.33    | 4    |
  | abstract_declarator | SyntaxIndex    | 0.33    | 4    |
  | locus               | SourceLocation | 0.33    | 8    |
  |---------------------|----------------|---------|------|
*/
enum an_ifc_syntax_type_id_part : uint8_t {};
using an_ifc_syntax_type_id_storage = an_ifc_syntax_type_id_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_type_id_bytes = const an_ifc_syntax_type_id_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_id_bytes = an_ifc_syntax_type_id_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_id =
                             an_ifc_Byte_buffer<an_ifc_syntax_type_id_storage>;


/*
  |--------------------------------------------|
  | SyntaxTypeIdListElement - 0.33 (12 bytes)  |
  |----------|----------------|---------|------|
  | Name     | Type           | Version | Size |
  |----------|----------------|---------|------|
  | type_id  | SyntaxIndex    | 0.33    | 4    |
  | ellipsis | SourceLocation | 0.33    | 8    |
  |----------|----------------|---------|------|
*/
enum an_ifc_syntax_type_id_list_element_part : uint8_t {};
using an_ifc_syntax_type_id_list_element_storage =
                                   an_ifc_syntax_type_id_list_element_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_type_id_list_element_bytes =
                             const an_ifc_syntax_type_id_list_element_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_id_list_element_bytes =
                                    an_ifc_syntax_type_id_list_element_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_id_list_element =
                an_ifc_Byte_buffer<an_ifc_syntax_type_id_list_element_storage>;


/*
  |-----------------------------------------|
  | SyntaxTypeRequirement - 0.33 (12 bytes) |
  |-------|----------------|---------|------|
  | Name  | Type           | Version | Size |
  |-------|----------------|---------|------|
  | type  | ExprIndex      | 0.33    | 4    |
  | locus | SourceLocation | 0.33    | 8    |
  |-------|----------------|---------|------|
*/
enum an_ifc_syntax_type_requirement_part : uint8_t {};
using an_ifc_syntax_type_requirement_storage =
                                       an_ifc_syntax_type_requirement_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_type_requirement_bytes =
                                 const an_ifc_syntax_type_requirement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_requirement_bytes =
                                        an_ifc_syntax_type_requirement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_requirement =
                    an_ifc_Byte_buffer<an_ifc_syntax_type_requirement_storage>;


/*
  |-------------------------------------------------|
  |    SyntaxTypeSpecifierSeq - 0.33 (18 bytes)     |
  |------------|-------------------|---------|------|
  | Name       | Type              | Version | Size |
  |------------|-------------------|---------|------|
  | type_name  | SyntaxIndex       | 0.33    | 4    |
  | type       | TypeIndex         | 0.33    | 4    |
  | locus      | SourceLocation    | 0.33    | 8    |
  | qualifiers | QualifierBitfield | 0.33    | 1    |
  | unhashed   | bool              | 0.33    | 1    |
  |------------|-------------------|---------|------|
*/
enum an_ifc_syntax_type_specifier_seq_part : uint8_t {};
using an_ifc_syntax_type_specifier_seq_storage =
                                     an_ifc_syntax_type_specifier_seq_part[18];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_type_specifier_seq_bytes =
                               const an_ifc_syntax_type_specifier_seq_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_specifier_seq_bytes =
                                      an_ifc_syntax_type_specifier_seq_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_specifier_seq =
                  an_ifc_Byte_buffer<an_ifc_syntax_type_specifier_seq_storage>;


/*
  |----------------------------------------------|
  | SyntaxTypeTemplateArgument - 0.33 (20 bytes) |
  |------------|----------------|---------|------|
  | Name       | Type           | Version | Size |
  |------------|----------------|---------|------|
  | argument   | SyntaxIndex    | 0.33    | 4    |
  | ellipsis   | SourceLocation | 0.33    | 8    |
  | comma      | SourceLocation | 0.33    | 8    |
  |------------|----------------|---------|------|
*/
enum an_ifc_syntax_type_template_argument_part : uint8_t {};
using an_ifc_syntax_type_template_argument_storage =
                                 an_ifc_syntax_type_template_argument_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_type_template_argument_bytes =
                           const an_ifc_syntax_type_template_argument_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_template_argument_bytes =
                                  an_ifc_syntax_type_template_argument_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_template_argument =
              an_ifc_Byte_buffer<an_ifc_syntax_type_template_argument_storage>;


/*
  |-----------------------------------------------|
  | SyntaxTypeTemplateParameter - 0.33 (28 bytes) |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | name        | TextOffset     | 0.33    | 4    |
  | constraint  | SyntaxIndex    | 0.33    | 4    |
  | argument    | SyntaxIndex    | 0.33    | 4    |
  | locus       | SourceLocation | 0.33    | 8    |
  | ellipsis    | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_type_template_parameter_part : uint8_t {};
using an_ifc_syntax_type_template_parameter_storage =
                                an_ifc_syntax_type_template_parameter_part[28];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_type_template_parameter_bytes =
                          const an_ifc_syntax_type_template_parameter_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_template_parameter_bytes =
                                 an_ifc_syntax_type_template_parameter_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_template_parameter =
             an_ifc_Byte_buffer<an_ifc_syntax_type_template_parameter_storage>;


/*
  |-----------------------------------------------|
  |  SyntaxTypeTraitIntrinsic - 0.33 (14 bytes)   |
  |-----------|------------------|---------|------|
  | Name      | Type             | Version | Size |
  |-----------|------------------|---------|------|
  | arguments | SyntaxIndex      | 0.33    | 4    |
  | locus     | SourceLocation   | 0.33    | 8    |
  | intrinsic | OperatorCategory | 0.33    | 2    |
  |-----------|------------------|---------|------|
*/
enum an_ifc_syntax_type_trait_intrinsic_part : uint8_t {};
using an_ifc_syntax_type_trait_intrinsic_storage =
                                   an_ifc_syntax_type_trait_intrinsic_part[14];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_type_trait_intrinsic_bytes =
                             const an_ifc_syntax_type_trait_intrinsic_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_trait_intrinsic_bytes =
                                    an_ifc_syntax_type_trait_intrinsic_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_type_trait_intrinsic =
                an_ifc_Byte_buffer<an_ifc_syntax_type_trait_intrinsic_storage>;


/*
  |---------------------------------------------------|
  |    SyntaxUnaryFoldExpression - 0.33 (42 bytes)    |
  |-------------|--------------------|---------|------|
  | Name        | Type               | Version | Size |
  |-------------|--------------------|---------|------|
  | direction   | FoldDirectionSort  | 0.33    | 4    |
  | operand     | ExprIndex          | 0.33    | 4    |
  | dyad        | DyadicOperatorSort | 0.33    | 2    |
  | locus       | SourceLocation     | 0.33    | 8    |
  | ellipsis    | SourceLocation     | 0.33    | 8    |
  | glyph_locus | SourceLocation     | 0.33    | 8    |
  | right_paren | SourceLocation     | 0.33    | 8    |
  |-------------|--------------------|---------|------|
*/
enum an_ifc_syntax_unary_fold_expression_part : uint8_t {};
using an_ifc_syntax_unary_fold_expression_storage =
                                  an_ifc_syntax_unary_fold_expression_part[42];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_unary_fold_expression_bytes =
                            const an_ifc_syntax_unary_fold_expression_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_unary_fold_expression_bytes =
                                   an_ifc_syntax_unary_fold_expression_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_unary_fold_expression =
               an_ifc_Byte_buffer<an_ifc_syntax_unary_fold_expression_storage>;


/*
  |-----------------------------------------------|
  |   SyntaxUsingDeclaration - 0.33 (20 bytes)    |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | declarators | SyntaxIndex    | 0.33    | 4    |
  | keyword     | SourceLocation | 0.33    | 8    |
  | semicolon   | SourceLocation | 0.33    | 8    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_using_declaration_part : uint8_t {};
using an_ifc_syntax_using_declaration_storage =
                                      an_ifc_syntax_using_declaration_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_using_declaration_bytes =
                                const an_ifc_syntax_using_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_using_declaration_bytes =
                                       an_ifc_syntax_using_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_using_declaration =
                   an_ifc_Byte_buffer<an_ifc_syntax_using_declaration_storage>;


/*
  |--------------------------------------------------|
  |     SyntaxUsingDeclarator - 0.33 (28 bytes)      |
  |----------------|----------------|---------|------|
  | Name           | Type           | Version | Size |
  |----------------|----------------|---------|------|
  | qualified_name | ExprIndex      | 0.33    | 4    |
  | typename_kw    | SourceLocation | 0.33    | 8    |
  | expander       | SourceLocation | 0.33    | 8    |
  | comma          | SourceLocation | 0.33    | 8    |
  |----------------|----------------|---------|------|
*/
enum an_ifc_syntax_using_declarator_part : uint8_t {};
using an_ifc_syntax_using_declarator_storage =
                                       an_ifc_syntax_using_declarator_part[28];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_using_declarator_bytes =
                                 const an_ifc_syntax_using_declarator_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_using_declarator_bytes =
                                        an_ifc_syntax_using_declarator_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_using_declarator =
                    an_ifc_Byte_buffer<an_ifc_syntax_using_declarator_storage>;


/*
  |--------------------------------------------------|
  |      SyntaxUsingDirective - 0.33 (28 bytes)      |
  |----------------|----------------|---------|------|
  | Name           | Type           | Version | Size |
  |----------------|----------------|---------|------|
  | qualified_name | ExprIndex      | 0.33    | 4    |
  | using_kw       | SourceLocation | 0.33    | 8    |
  | namespace_kw   | SourceLocation | 0.33    | 8    |
  | semicolon      | SourceLocation | 0.33    | 8    |
  |----------------|----------------|---------|------|
*/
enum an_ifc_syntax_using_directive_part : uint8_t {};
using an_ifc_syntax_using_directive_storage =
                                        an_ifc_syntax_using_directive_part[28];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_using_directive_bytes =
                                  const an_ifc_syntax_using_directive_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_using_directive_bytes =
                                         an_ifc_syntax_using_directive_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_using_directive =
                     an_ifc_Byte_buffer<an_ifc_syntax_using_directive_storage>;


/*
  |----------------------------------------------|
  | SyntaxUsingEnumDeclaration - 0.33 (28 bytes) |
  |------------|----------------|---------|------|
  | Name       | Type           | Version | Size |
  |------------|----------------|---------|------|
  | name       | ExprIndex      | 0.33    | 4    |
  | using_kw   | SourceLocation | 0.33    | 8    |
  | enum_kw    | SourceLocation | 0.33    | 8    |
  | semicolon  | SourceLocation | 0.33    | 8    |
  |------------|----------------|---------|------|
*/
enum an_ifc_syntax_using_enum_declaration_part : uint8_t {};
using an_ifc_syntax_using_enum_declaration_storage =
                                 an_ifc_syntax_using_enum_declaration_part[28];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_using_enum_declaration_bytes =
                           const an_ifc_syntax_using_enum_declaration_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_using_enum_declaration_bytes =
                                  an_ifc_syntax_using_enum_declaration_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_using_enum_declaration =
              an_ifc_Byte_buffer<an_ifc_syntax_using_enum_declaration_storage>;


/*
  |-----------------------------------------------|
  |  SyntaxVirtualSpecifierSeq - 0.33 (25 bytes)  |
  |-------------|----------------|---------|------|
  | Name        | Type           | Version | Size |
  |-------------|----------------|---------|------|
  | locus       | SourceLocation | 0.33    | 8    |
  | final_kw    | SourceLocation | 0.33    | 8    |
  | override_kw | SourceLocation | 0.33    | 8    |
  | pure        | bool           | 0.33    | 1    |
  |-------------|----------------|---------|------|
*/
enum an_ifc_syntax_virtual_specifier_seq_part : uint8_t {};
using an_ifc_syntax_virtual_specifier_seq_storage =
                                  an_ifc_syntax_virtual_specifier_seq_part[25];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_virtual_specifier_seq_bytes =
                            const an_ifc_syntax_virtual_specifier_seq_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_virtual_specifier_seq_bytes =
                                   an_ifc_syntax_virtual_specifier_seq_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_virtual_specifier_seq =
               an_ifc_Byte_buffer<an_ifc_syntax_virtual_specifier_seq_storage>;


/*
  |---------------------------------------------|
  |   SyntaxWhileStatement - 0.33 (20 bytes)    |
  |-----------|----------------|---------|------|
  | Name      | Type           | Version | Size |
  |-----------|----------------|---------|------|
  | pragma    | SentenceIndex  | 0.33    | 4    |
  | condition | ExprIndex      | 0.33    | 4    |
  | body      | SyntaxIndex    | 0.33    | 4    |
  | while     | SourceLocation | 0.33    | 8    |
  |-----------|----------------|---------|------|
*/
enum an_ifc_syntax_while_statement_part : uint8_t {};
using an_ifc_syntax_while_statement_storage =
                                        an_ifc_syntax_while_statement_part[20];
#if USE_MMAP_FOR_MODULES
using an_ifc_syntax_while_statement_bytes =
                                  const an_ifc_syntax_while_statement_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_syntax_while_statement_bytes =
                                         an_ifc_syntax_while_statement_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_syntax_while_statement =
                     an_ifc_Byte_buffer<an_ifc_syntax_while_statement_storage>;


/*
  |--------------------------------------------------|
  |       TraitAliasTemplate - 0.33 (8 bytes)        |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.33    | 4    |
  | trait        | SyntaxIndex      | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|

  |--------------------------------------------------|
  |       TraitAliasTemplate - 0.41 (8 bytes)        |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.41    | 4    |
  | trait        | SyntaxIndex      | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|
*/
enum an_ifc_trait_alias_template_part : uint8_t {};
using an_ifc_trait_alias_template_storage =
                                           an_ifc_trait_alias_template_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_trait_alias_template_bytes =
                                    const an_ifc_trait_alias_template_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_trait_alias_template_bytes = an_ifc_trait_alias_template_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_trait_alias_template =
                       an_ifc_Byte_buffer<an_ifc_trait_alias_template_storage>;


/*
  |--------------------------------------------------|
  |         TraitAttribute - 0.33 (8 bytes)          |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.33    | 4    |
  | trait        | AttrIndex        | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|

  |--------------------------------------------------|
  |         TraitAttribute - 0.41 (8 bytes)          |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.41    | 4    |
  | trait        | AttrIndex        | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|
*/
enum an_ifc_trait_attribute_part : uint8_t {};
using an_ifc_trait_attribute_storage = an_ifc_trait_attribute_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_trait_attribute_bytes = const an_ifc_trait_attribute_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_trait_attribute_bytes = an_ifc_trait_attribute_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_trait_attribute =
                            an_ifc_Byte_buffer<an_ifc_trait_attribute_storage>;


/*
  |--------------------------------------------------|
  |       TraitDeductionGuide - 0.33 (8 bytes)       |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.33    | 4    |
  | trait        | DeclIndex        | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|

  |--------------------------------------------------|
  |       TraitDeductionGuide - 0.41 (8 bytes)       |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.41    | 4    |
  | trait        | DeclIndex        | 0.41    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|
*/
enum an_ifc_trait_deduction_guide_part : uint8_t {};
using an_ifc_trait_deduction_guide_storage =
                                          an_ifc_trait_deduction_guide_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_trait_deduction_guide_bytes =
                                   const an_ifc_trait_deduction_guide_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_trait_deduction_guide_bytes =
                                          an_ifc_trait_deduction_guide_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_trait_deduction_guide =
                      an_ifc_Byte_buffer<an_ifc_trait_deduction_guide_storage>;


/*
  |--------------------------------------------------|
  |         TraitDeprecated - 0.33 (8 bytes)         |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.33    | 4    |
  | trait        | TextOffset       | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|

  |--------------------------------------------------|
  |         TraitDeprecated - 0.41 (8 bytes)         |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.41    | 4    |
  | trait        | TextOffset       | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|
*/
enum an_ifc_trait_deprecated_part : uint8_t {};
using an_ifc_trait_deprecated_storage = an_ifc_trait_deprecated_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_trait_deprecated_bytes = const an_ifc_trait_deprecated_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_trait_deprecated_bytes = an_ifc_trait_deprecated_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_trait_deprecated =
                           an_ifc_Byte_buffer<an_ifc_trait_deprecated_storage>;


/*
  |--------------------------------------------------|
  |          TraitFriend - 0.33 (12 bytes)           |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.33    | 4    |
  | trait        | Sequence         | 0.33    | 8    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|

  |--------------------------------------------------|
  |          TraitFriend - 0.41 (12 bytes)           |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.41    | 4    |
  | trait        | Sequence         | 0.33    | 8    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|
*/
enum an_ifc_trait_friend_part : uint8_t {};
using an_ifc_trait_friend_storage = an_ifc_trait_friend_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_trait_friend_bytes = const an_ifc_trait_friend_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_trait_friend_bytes = an_ifc_trait_friend_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_trait_friend = an_ifc_Byte_buffer<an_ifc_trait_friend_storage>;


/*
  |--------------------------------------------------|
  |    TraitFunctionDefinition - 0.33 (16 bytes)     |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.33    | 4    |
  | parameters   | ChartIndex       | 0.33    | 4    |
  | initializers | ExprIndex        | 0.33    | 4    |
  | body         | StmtIndex        | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|

  |--------------------------------------------------|
  |    TraitFunctionDefinition - 0.41 (16 bytes)     |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.41    | 4    |
  | parameters   | ChartIndex       | 0.33    | 4    |
  | initializers | ExprIndex        | 0.33    | 4    |
  | body         | StmtIndex        | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|
*/
enum an_ifc_trait_function_definition_part : uint8_t {};
using an_ifc_trait_function_definition_storage =
                                     an_ifc_trait_function_definition_part[16];
#if USE_MMAP_FOR_MODULES
using an_ifc_trait_function_definition_bytes =
                               const an_ifc_trait_function_definition_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_trait_function_definition_bytes =
                                      an_ifc_trait_function_definition_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_trait_function_definition =
                  an_ifc_Byte_buffer<an_ifc_trait_function_definition_storage>;


/*
  |--------------------------------------------------|
  |       TraitMsvcDeclAttrs - 0.33 (8 bytes)        |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.33    | 4    |
  | trait        | AttrIndex        | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|

  |--------------------------------------------------|
  |       TraitMsvcDeclAttrs - 0.41 (8 bytes)        |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.41    | 4    |
  | trait        | AttrIndex        | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|
*/
enum an_ifc_trait_msvc_decl_attrs_part : uint8_t {};
using an_ifc_trait_msvc_decl_attrs_storage =
                                          an_ifc_trait_msvc_decl_attrs_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_trait_msvc_decl_attrs_bytes =
                                   const an_ifc_trait_msvc_decl_attrs_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_trait_msvc_decl_attrs_bytes =
                                          an_ifc_trait_msvc_decl_attrs_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_trait_msvc_decl_attrs =
                      an_ifc_Byte_buffer<an_ifc_trait_msvc_decl_attrs_storage>;


/*
  |--------------------------------------------------|
  |       TraitMsvcFuncParams - 0.33 (8 bytes)       |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.33    | 4    |
  | params       | ChartIndex       | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|

  |--------------------------------------------------|
  |       TraitMsvcFuncParams - 0.41 (8 bytes)       |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.41    | 4    |
  | params       | ChartIndex       | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|
*/
enum an_ifc_trait_msvc_func_params_part : uint8_t {};
using an_ifc_trait_msvc_func_params_storage =
                                         an_ifc_trait_msvc_func_params_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_trait_msvc_func_params_bytes =
                                  const an_ifc_trait_msvc_func_params_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_trait_msvc_func_params_bytes =
                                         an_ifc_trait_msvc_func_params_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_trait_msvc_func_params =
                     an_ifc_Byte_buffer<an_ifc_trait_msvc_func_params_storage>;


/*
  |--------------------------------------------------|
  |          TraitMsvcUuid - 0.33 (6 bytes)          |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.33    | 4    |
  | uuid         | Uuid             | 0.33    | 2    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|

  |--------------------------------------------------|
  |          TraitMsvcUuid - 0.41 (6 bytes)          |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.41    | 4    |
  | uuid         | Uuid             | 0.33    | 2    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|
*/
enum an_ifc_trait_msvc_uuid_part : uint8_t {};
using an_ifc_trait_msvc_uuid_storage = an_ifc_trait_msvc_uuid_part[6];
#if USE_MMAP_FOR_MODULES
using an_ifc_trait_msvc_uuid_bytes = const an_ifc_trait_msvc_uuid_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_trait_msvc_uuid_bytes = an_ifc_trait_msvc_uuid_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_trait_msvc_uuid =
                            an_ifc_Byte_buffer<an_ifc_trait_msvc_uuid_storage>;


/*
  |----------------------------------------------------|
  |       TraitMsvcVendorTrait - 0.33 (8 bytes)        |
  |--------------|--------------------|---------|------|
  | Name         | Type               | Version | Size |
  |--------------|--------------------|---------|------|
  | decl         | DeclIndex          | 0.33    | 4    |
  | trait        | MsvcTraitsBitfield | 0.33    | 4    |
  |--------------|--------------------|---------|------|
  | encoded_decl | EncodedDeclIndex   | 0.33    | RF   |
  |--------------|--------------------|---------|------|

  |----------------------------------------------------|
  |       TraitMsvcVendorTrait - 0.41 (8 bytes)        |
  |--------------|--------------------|---------|------|
  | Name         | Type               | Version | Size |
  |--------------|--------------------|---------|------|
  | decl         | DeclIndex          | 0.41    | 4    |
  | trait        | MsvcTraitsBitfield | 0.33    | 4    |
  |--------------|--------------------|---------|------|
  | encoded_decl | EncodedDeclIndex   | 0.33    | RF   |
  |--------------|--------------------|---------|------|
*/
enum an_ifc_trait_msvc_vendor_trait_part : uint8_t {};
using an_ifc_trait_msvc_vendor_trait_storage =
                                        an_ifc_trait_msvc_vendor_trait_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_trait_msvc_vendor_trait_bytes =
                                 const an_ifc_trait_msvc_vendor_trait_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_trait_msvc_vendor_trait_bytes =
                                        an_ifc_trait_msvc_vendor_trait_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_trait_msvc_vendor_trait =
                    an_ifc_Byte_buffer<an_ifc_trait_msvc_vendor_trait_storage>;


/*
  |--------------------------------------------------|
  |          TraitRequires - 0.33 (8 bytes)          |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.33    | 4    |
  | trait        | SyntaxIndex      | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|

  |--------------------------------------------------|
  |          TraitRequires - 0.41 (8 bytes)          |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.41    | 4    |
  | trait        | SyntaxIndex      | 0.33    | 4    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|
*/
enum an_ifc_trait_requires_part : uint8_t {};
using an_ifc_trait_requires_storage = an_ifc_trait_requires_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_trait_requires_bytes = const an_ifc_trait_requires_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_trait_requires_bytes = an_ifc_trait_requires_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_trait_requires =
                             an_ifc_Byte_buffer<an_ifc_trait_requires_storage>;


/*
  |--------------------------------------------------|
  |      TraitSpecialization - 0.33 (12 bytes)       |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.33    | 4    |
  | trait        | Sequence         | 0.33    | 8    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|

  |--------------------------------------------------|
  |      TraitSpecialization - 0.41 (12 bytes)       |
  |--------------|------------------|---------|------|
  | Name         | Type             | Version | Size |
  |--------------|------------------|---------|------|
  | decl         | DeclIndex        | 0.41    | 4    |
  | trait        | Sequence         | 0.33    | 8    |
  |--------------|------------------|---------|------|
  | encoded_decl | EncodedDeclIndex | 0.33    | RF   |
  |--------------|------------------|---------|------|
*/
enum an_ifc_trait_specialization_part : uint8_t {};
using an_ifc_trait_specialization_storage =
                                          an_ifc_trait_specialization_part[12];
#if USE_MMAP_FOR_MODULES
using an_ifc_trait_specialization_bytes =
                                    const an_ifc_trait_specialization_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_trait_specialization_bytes = an_ifc_trait_specialization_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_trait_specialization =
                       an_ifc_Byte_buffer<an_ifc_trait_specialization_storage>;


/*
  |--------------------------------------|
  |      TypeArray - 0.33 (8 bytes)      |
  |---------|-----------|---------|------|
  | Name    | Type      | Version | Size |
  |---------|-----------|---------|------|
  | element | TypeIndex | 0.33    | 4    |
  | extent  | ExprIndex | 0.33    | 4    |
  |---------|-----------|---------|------|
*/
enum an_ifc_type_array_part : uint8_t {};
using an_ifc_type_array_storage = an_ifc_type_array_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_array_bytes = const an_ifc_type_array_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_array_bytes = an_ifc_type_array_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_array = an_ifc_Byte_buffer<an_ifc_type_array_storage>;


/*
  |---------------------------------------------|
  |          TypeBase - 0.33 (7 bytes)          |
  |---------------|------------|---------|------|
  | Name          | Type       | Version | Size |
  |---------------|------------|---------|------|
  | type          | TypeIndex  | 0.33    | 4    |
  | access        | AccessSort | 0.33    | 1    |
  | shared        | bool       | 0.33    | 1    |
  | pack_expanded | bool       | 0.33    | 1    |
  |---------------|------------|---------|------|
*/
enum an_ifc_type_base_part : uint8_t {};
using an_ifc_type_base_storage = an_ifc_type_base_part[7];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_base_bytes = const an_ifc_type_base_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_base_bytes = an_ifc_type_base_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_base = an_ifc_Byte_buffer<an_ifc_type_base_storage>;


/*
  |-------------------------------------|
  |    TypeDecltype - 0.33 (4 bytes)    |
  |------|-------------|---------|------|
  | Name | Type        | Version | Size |
  |------|-------------|---------|------|
  | expr | SyntaxIndex | 0.33    | 4    |
  |------|-------------|---------|------|
*/
enum an_ifc_type_decltype_part : uint8_t {};
using an_ifc_type_decltype_storage = an_ifc_type_decltype_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_decltype_bytes = const an_ifc_type_decltype_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_decltype_bytes = an_ifc_type_decltype_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_decltype = an_ifc_Byte_buffer<an_ifc_type_decltype_storage>;


/*
  |-----------------------------------|
  |  TypeDesignated - 0.33 (4 bytes)  |
  |------|-----------|---------|------|
  | Name | Type      | Version | Size |
  |------|-----------|---------|------|
  | decl | DeclIndex | 0.33    | 4    |
  |------|-----------|---------|------|

  |-----------------------------------|
  |  TypeDesignated - 0.41 (4 bytes)  |
  |------|-----------|---------|------|
  | Name | Type      | Version | Size |
  |------|-----------|---------|------|
  | decl | DeclIndex | 0.41    | 4    |
  |------|-----------|---------|------|
*/
enum an_ifc_type_designated_part : uint8_t {};
using an_ifc_type_designated_storage = an_ifc_type_designated_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_designated_bytes = const an_ifc_type_designated_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_designated_bytes = an_ifc_type_designated_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_designated =
                            an_ifc_Byte_buffer<an_ifc_type_designated_storage>;


/*
  |-------------------------------------------|
  |      TypeExpansion - 0.33 (5 bytes)       |
  |------|-------------------|---------|------|
  | Name | Type              | Version | Size |
  |------|-------------------|---------|------|
  | pack | TypeIndex         | 0.33    | 4    |
  | mode | ExpansionModeSort | 0.33    | 1    |
  |------|-------------------|---------|------|
*/
enum an_ifc_type_expansion_part : uint8_t {};
using an_ifc_type_expansion_storage = an_ifc_type_expansion_part[5];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_expansion_bytes = const an_ifc_type_expansion_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_expansion_bytes = an_ifc_type_expansion_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_expansion =
                             an_ifc_Byte_buffer<an_ifc_type_expansion_storage>;


/*
  |---------------------------------------|
  |      TypeForall - 0.33 (8 bytes)      |
  |---------|------------|---------|------|
  | Name    | Type       | Version | Size |
  |---------|------------|---------|------|
  | chart   | ChartIndex | 0.33    | 4    |
  | subject | TypeIndex  | 0.33    | 4    |
  |---------|------------|---------|------|
*/
enum an_ifc_type_forall_part : uint8_t {};
using an_ifc_type_forall_storage = an_ifc_type_forall_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_forall_bytes = const an_ifc_type_forall_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_forall_bytes = an_ifc_type_forall_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_forall = an_ifc_Byte_buffer<an_ifc_type_forall_storage>;


/*
  |----------------------------------------------------------|
  |              TypeFunction - 0.33 (18 bytes)              |
  |------------|----------------------------|---------|------|
  | Name       | Type                       | Version | Size |
  |------------|----------------------------|---------|------|
  | target     | TypeIndex                  | 0.33    | 4    |
  | source     | TypeIndex                  | 0.33    | 4    |
  | eh_spec    | NoexceptSpecification      | 0.33    | 8    |
  | convention | CallingConventionSort      | 0.33    | 1    |
  | traits     | FunctionTypeTraitsBitfield | 0.33    | 1    |
  |------------|----------------------------|---------|------|
*/
enum an_ifc_type_function_part : uint8_t {};
using an_ifc_type_function_storage = an_ifc_type_function_part[18];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_function_bytes = const an_ifc_type_function_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_function_bytes = an_ifc_type_function_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_function = an_ifc_Byte_buffer<an_ifc_type_function_storage>;


/*
  |--------------------------------------------------|
  |         TypeFundamental - 0.33 (4 bytes)         |
  |-------------|-------------------|---------|------|
  | Name        | Type              | Version | Size |
  |-------------|-------------------|---------|------|
  | basis       | TypeBasisSort     | 0.33    | 1    |
  | precision   | TypePrecisionSort | 0.33    | 1    |
  | sign        | TypeSignSort      | 0.33    | 1    |
  | __padding__ | uint8_t[1]        |         | 1    |
  |-------------|-------------------|---------|------|
*/
enum an_ifc_type_fundamental_part : uint8_t {};
using an_ifc_type_fundamental_storage = an_ifc_type_fundamental_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_fundamental_bytes = const an_ifc_type_fundamental_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_fundamental_bytes = an_ifc_type_fundamental_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_fundamental =
                           an_ifc_Byte_buffer<an_ifc_type_fundamental_storage>;


/*
  |--------------------------------------|
  | TypeLvalueReference - 0.33 (4 bytes) |
  |---------|-----------|---------|------|
  | Name    | Type      | Version | Size |
  |---------|-----------|---------|------|
  | referee | TypeIndex | 0.33    | 4    |
  |---------|-----------|---------|------|
*/
enum an_ifc_type_lvalue_reference_part : uint8_t {};
using an_ifc_type_lvalue_reference_storage =
                                          an_ifc_type_lvalue_reference_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_lvalue_reference_bytes =
                                   const an_ifc_type_lvalue_reference_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_lvalue_reference_bytes =
                                          an_ifc_type_lvalue_reference_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_lvalue_reference =
                      an_ifc_Byte_buffer<an_ifc_type_lvalue_reference_storage>;


/*
  |----------------------------------------------------------|
  |               TypeMethod - 0.33 (22 bytes)               |
  |------------|----------------------------|---------|------|
  | Name       | Type                       | Version | Size |
  |------------|----------------------------|---------|------|
  | target     | TypeIndex                  | 0.33    | 4    |
  | source     | TypeIndex                  | 0.33    | 4    |
  | scope      | TypeIndex                  | 0.33    | 4    |
  | eh_spec    | NoexceptSpecification      | 0.33    | 8    |
  | convention | CallingConventionSort      | 0.33    | 1    |
  | traits     | FunctionTypeTraitsBitfield | 0.33    | 1    |
  |------------|----------------------------|---------|------|
*/
enum an_ifc_type_method_part : uint8_t {};
using an_ifc_type_method_storage = an_ifc_type_method_part[22];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_method_bytes = const an_ifc_type_method_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_method_bytes = an_ifc_type_method_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_method = an_ifc_Byte_buffer<an_ifc_type_method_storage>;


/*
  |----------------------------------------------|
  |       TypePlaceholder - 0.33 (9 bytes)       |
  |-------------|---------------|---------|------|
  | Name        | Type          | Version | Size |
  |-------------|---------------|---------|------|
  | constraint  | ExprIndex     | 0.33    | 4    |
  | basis       | TypeBasisSort | 0.33    | 1    |
  | elaboration | TypeIndex     | 0.33    | 4    |
  |-------------|---------------|---------|------|
*/
enum an_ifc_type_placeholder_part : uint8_t {};
using an_ifc_type_placeholder_storage = an_ifc_type_placeholder_part[9];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_placeholder_bytes = const an_ifc_type_placeholder_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_placeholder_bytes = an_ifc_type_placeholder_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_placeholder =
                           an_ifc_Byte_buffer<an_ifc_type_placeholder_storage>;


/*
  |--------------------------------------|
  |     TypePointer - 0.33 (4 bytes)     |
  |---------|-----------|---------|------|
  | Name    | Type      | Version | Size |
  |---------|-----------|---------|------|
  | pointee | TypeIndex | 0.33    | 4    |
  |---------|-----------|---------|------|
*/
enum an_ifc_type_pointer_part : uint8_t {};
using an_ifc_type_pointer_storage = an_ifc_type_pointer_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_pointer_bytes = const an_ifc_type_pointer_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_pointer_bytes = an_ifc_type_pointer_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_pointer = an_ifc_Byte_buffer<an_ifc_type_pointer_storage>;


/*
  |--------------------------------------|
  | TypePointerToMember - 0.33 (8 bytes) |
  |---------|-----------|---------|------|
  | Name    | Type      | Version | Size |
  |---------|-----------|---------|------|
  | scope   | TypeIndex | 0.33    | 4    |
  | member  | TypeIndex | 0.33    | 4    |
  |---------|-----------|---------|------|
*/
enum an_ifc_type_pointer_to_member_part : uint8_t {};
using an_ifc_type_pointer_to_member_storage =
                                         an_ifc_type_pointer_to_member_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_pointer_to_member_bytes =
                                  const an_ifc_type_pointer_to_member_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_pointer_to_member_bytes =
                                         an_ifc_type_pointer_to_member_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_pointer_to_member =
                     an_ifc_Byte_buffer<an_ifc_type_pointer_to_member_storage>;


/*
  |--------------------------------------------------|
  |          TypeQualified - 0.33 (5 bytes)          |
  |-------------|-------------------|---------|------|
  | Name        | Type              | Version | Size |
  |-------------|-------------------|---------|------|
  | unqualified | TypeIndex         | 0.33    | 4    |
  | qualifiers  | QualifierBitfield | 0.33    | 1    |
  |-------------|-------------------|---------|------|
*/
enum an_ifc_type_qualified_part : uint8_t {};
using an_ifc_type_qualified_storage = an_ifc_type_qualified_part[5];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_qualified_bytes = const an_ifc_type_qualified_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_qualified_bytes = an_ifc_type_qualified_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_qualified =
                             an_ifc_Byte_buffer<an_ifc_type_qualified_storage>;


/*
  |--------------------------------------|
  | TypeRvalueReference - 0.33 (4 bytes) |
  |---------|-----------|---------|------|
  | Name    | Type      | Version | Size |
  |---------|-----------|---------|------|
  | referee | TypeIndex | 0.33    | 4    |
  |---------|-----------|---------|------|
*/
enum an_ifc_type_rvalue_reference_part : uint8_t {};
using an_ifc_type_rvalue_reference_storage =
                                          an_ifc_type_rvalue_reference_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_rvalue_reference_bytes =
                                   const an_ifc_type_rvalue_reference_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_rvalue_reference_bytes =
                                          an_ifc_type_rvalue_reference_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_rvalue_reference =
                      an_ifc_Byte_buffer<an_ifc_type_rvalue_reference_storage>;


/*
  |-----------------------------------|
  |  TypeSyntactic - 0.33 (4 bytes)   |
  |------|-----------|---------|------|
  | Name | Type      | Version | Size |
  |------|-----------|---------|------|
  | expr | ExprIndex | 0.33    | 4    |
  |------|-----------|---------|------|
*/
enum an_ifc_type_syntactic_part : uint8_t {};
using an_ifc_type_syntactic_storage = an_ifc_type_syntactic_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_syntactic_bytes = const an_ifc_type_syntactic_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_syntactic_bytes = an_ifc_type_syntactic_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_syntactic =
                             an_ifc_Byte_buffer<an_ifc_type_syntactic_storage>;


/*
  |---------------------------------------|
  |    TypeSyntaxTree - 0.33 (4 bytes)    |
  |--------|-------------|---------|------|
  | Name   | Type        | Version | Size |
  |--------|-------------|---------|------|
  | syntax | SyntaxIndex | 0.33    | 4    |
  |--------|-------------|---------|------|
*/
enum an_ifc_type_syntax_tree_part : uint8_t {};
using an_ifc_type_syntax_tree_storage = an_ifc_type_syntax_tree_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_syntax_tree_bytes = const an_ifc_type_syntax_tree_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_syntax_tree_bytes = an_ifc_type_syntax_tree_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_syntax_tree =
                           an_ifc_Byte_buffer<an_ifc_type_syntax_tree_storage>;


/*
  |-----------------------------------------------------|
  |              TypeTor - 0.33 (13 bytes)              |
  |------------|-----------------------|---------|------|
  | Name       | Type                  | Version | Size |
  |------------|-----------------------|---------|------|
  | source     | TypeIndex             | 0.33    | 4    |
  | eh_spec    | NoexceptSpecification | 0.33    | 8    |
  | convention | CallingConventionSort | 0.33    | 1    |
  |------------|-----------------------|---------|------|
*/
enum an_ifc_type_tor_part : uint8_t {};
using an_ifc_type_tor_storage = an_ifc_type_tor_part[13];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_tor_bytes = const an_ifc_type_tor_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_tor_bytes = an_ifc_type_tor_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_tor = an_ifc_Byte_buffer<an_ifc_type_tor_storage>;


/*
  |--------------------------------------------|
  |         TypeTuple - 0.33 (8 bytes)         |
  |-------------|-------------|---------|------|
  | Name        | Type        | Version | Size |
  |-------------|-------------|---------|------|
  | start       | Index       | 0.33    | 4    |
  | cardinality | Cardinality | 0.33    | 4    |
  |-------------|-------------|---------|------|
*/
enum an_ifc_type_tuple_part : uint8_t {};
using an_ifc_type_tuple_storage = an_ifc_type_tuple_part[8];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_tuple_bytes = const an_ifc_type_tuple_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_tuple_bytes = an_ifc_type_tuple_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_tuple = an_ifc_Byte_buffer<an_ifc_type_tuple_storage>;


/*
  |-----------------------------------|
  |   TypeTypename - 0.33 (4 bytes)   |
  |------|-----------|---------|------|
  | Name | Type      | Version | Size |
  |------|-----------|---------|------|
  | path | ExprIndex | 0.33    | 4    |
  |------|-----------|---------|------|
*/
enum an_ifc_type_typename_part : uint8_t {};
using an_ifc_type_typename_storage = an_ifc_type_typename_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_typename_bytes = const an_ifc_type_typename_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_typename_bytes = an_ifc_type_typename_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_typename = an_ifc_Byte_buffer<an_ifc_type_typename_storage>;


/*
  |-----------------------------------|
  |  TypeUnaligned - 0.33 (4 bytes)   |
  |------|-----------|---------|------|
  | Name | Type      | Version | Size |
  |------|-----------|---------|------|
  | type | TypeIndex | 0.33    | 4    |
  |------|-----------|---------|------|
*/
enum an_ifc_type_unaligned_part : uint8_t {};
using an_ifc_type_unaligned_storage = an_ifc_type_unaligned_part[4];
#if USE_MMAP_FOR_MODULES
using an_ifc_type_unaligned_bytes = const an_ifc_type_unaligned_storage*;
#else /* !USE_MMAP_FOR_MODULES */
using an_ifc_type_unaligned_bytes = an_ifc_type_unaligned_storage;
#endif /* USE_MMAP_FOR_MODULES */
using an_ifc_type_unaligned =
                             an_ifc_Byte_buffer<an_ifc_type_unaligned_storage>;


enum an_ifc_partition_kind : uint32_t {
  ifc_pk_none,
  ifc_pk_msvc_trait_decl_attrs,
  ifc_pk_msvc_trait_named_function_parameters,
  ifc_pk_msvc_trait_uuid,
  ifc_pk_msvc_trait_vendor_traits,
  ifc_pk_attr_basic,
  ifc_pk_attr_called,
  ifc_pk_attr_elaborated,
  ifc_pk_attr_expanded,
  ifc_pk_attr_factored,
  ifc_pk_attr_labeled,
  ifc_pk_attr_scoped,
  ifc_pk_attr_tuple,
  ifc_pk_chart_multilevel,
  ifc_pk_chart_unilevel,
  ifc_pk_const_f64,
  ifc_pk_const_i64,
  ifc_pk_const_str,
  ifc_pk_decl_alias,
  ifc_pk_decl_bitfield,
  ifc_pk_decl_concept,
  ifc_pk_decl_constructor,
  ifc_pk_decl_deduction_guide,
  ifc_pk_decl_destructor,
  ifc_pk_decl_enum,
  ifc_pk_decl_enumerator,
  ifc_pk_decl_expansion,
  ifc_pk_decl_explicit_instantiation,
  ifc_pk_decl_explicit_specialization,
  ifc_pk_decl_field,
  ifc_pk_decl_friend,
  ifc_pk_decl_function,
  ifc_pk_decl_inherited_constructor,
  ifc_pk_decl_intrinsic,
  ifc_pk_decl_method,
  ifc_pk_decl_parameter,
  ifc_pk_decl_partial_specialization,
  ifc_pk_decl_property,
  ifc_pk_decl_reference,
  ifc_pk_decl_scope,
  ifc_pk_decl_segment,
  ifc_pk_decl_specialization,
  ifc_pk_decl_template,
  ifc_pk_decl_temploid,
  ifc_pk_decl_tuple,
  ifc_pk_decl_using_declaration,
  ifc_pk_decl_variable,
  ifc_pk_expr_alignof_type_id,
  ifc_pk_expr_array_value,
  ifc_pk_expr_assign_initializer,
  ifc_pk_expr_binary_fold,
  ifc_pk_expr_call,
  ifc_pk_expr_cast,
  ifc_pk_expr_class_subobject_value,
  ifc_pk_expr_compound_string,
  ifc_pk_expr_condition,
  ifc_pk_expr_decl,
  ifc_pk_expr_designated_init,
  ifc_pk_expr_destructor_call,
  ifc_pk_expr_dyad,
  ifc_pk_expr_dynamic_dispatch,
  ifc_pk_expr_empty,
  ifc_pk_expr_expansion,
  ifc_pk_expr_expression_list,
  ifc_pk_expr_function_string,
  ifc_pk_expr_hierarchy_conversion,
  ifc_pk_expr_inheritance_path,
  ifc_pk_expr_initializer,
  ifc_pk_expr_initializer_list,
  ifc_pk_expr_lambda,
  ifc_pk_expr_literal,
  ifc_pk_expr_member_access,
  ifc_pk_expr_member_initializer,
  ifc_pk_expr_monad,
  ifc_pk_expr_nullptr,
  ifc_pk_expr_packed_template_arguments,
  ifc_pk_expr_path,
  ifc_pk_expr_placeholder,
  ifc_pk_expr_pointer,
  ifc_pk_expr_product_type_value,
  ifc_pk_expr_push_state,
  ifc_pk_expr_qualified_name,
  ifc_pk_expr_read,
  ifc_pk_expr_requires,
  ifc_pk_expr_simple_identifier,
  ifc_pk_expr_sizeof_type,
  ifc_pk_expr_string_sequence,
  ifc_pk_expr_strings,
  ifc_pk_expr_sum_type_value,
  ifc_pk_expr_syntax_tree,
  ifc_pk_expr_template_id,
  ifc_pk_expr_template_reference,
  ifc_pk_expr_temporary,
  ifc_pk_expr_this,
  ifc_pk_expr_tokens,
  ifc_pk_expr_triad,
  ifc_pk_expr_tuple,
  ifc_pk_expr_type,
  ifc_pk_expr_type_trait,
  ifc_pk_expr_typeid,
  ifc_pk_expr_unary_fold,
  ifc_pk_expr_unqualified_id,
  ifc_pk_expr_unresolved,
  ifc_pk_expr_virtual_function_conversion,
  ifc_pk_form_spec,
  ifc_pk_heap_attr,
  ifc_pk_heap_chart,
  ifc_pk_heap_decl,
  ifc_pk_heap_expr,
  ifc_pk_heap_form,
  ifc_pk_heap_pp,
  ifc_pk_heap_stmt,
  ifc_pk_heap_syn,
  ifc_pk_heap_type,
  ifc_pk_macro_function_like,
  ifc_pk_macro_object_like,
  ifc_pk_module_exported,
  ifc_pk_module_imported,
  ifc_pk_name_conversion,
  ifc_pk_name_guide,
  ifc_pk_name_literal,
  ifc_pk_name_operator,
  ifc_pk_name_source_file,
  ifc_pk_name_specialization,
  ifc_pk_name_template,
  ifc_pk_pp_catenate,
  ifc_pk_pp_char,
  ifc_pk_pp_header,
  ifc_pk_pp_ident,
  ifc_pk_pp_junk,
  ifc_pk_pp_key,
  ifc_pk_pp_num,
  ifc_pk_pp_op,
  ifc_pk_pp_param,
  ifc_pk_pp_paren,
  ifc_pk_pp_pragma,
  ifc_pk_pp_space,
  ifc_pk_pp_string,
  ifc_pk_pp_to_string,
  ifc_pk_pp_tuple,
  ifc_pk_scope_desc,
  ifc_pk_scope_member,
  ifc_pk_src_line,
  ifc_pk_src_sentence,
  ifc_pk_src_word,
  ifc_pk_stmt_block,
  ifc_pk_stmt_break,
  ifc_pk_stmt_case,
  ifc_pk_stmt_continue,
  ifc_pk_stmt_default,
  ifc_pk_stmt_do_while,
  ifc_pk_stmt_empty,
  ifc_pk_stmt_expansion,
  ifc_pk_stmt_expression,
  ifc_pk_stmt_for,
  ifc_pk_stmt_if,
  ifc_pk_stmt_return,
  ifc_pk_stmt_switch,
  ifc_pk_stmt_variable,
  ifc_pk_stmt_while,
  ifc_pk_syntax_access_specifier,
  ifc_pk_syntax_alias_declaration,
  ifc_pk_syntax_alignas,
  ifc_pk_syntax_array_declarator,
  ifc_pk_syntax_array_index,
  ifc_pk_syntax_array_or_function_declarator,
  ifc_pk_syntax_asm_statement,
  ifc_pk_syntax_attribute,
  ifc_pk_syntax_attribute_argument_clause,
  ifc_pk_syntax_attribute_specifier,
  ifc_pk_syntax_attribute_specifier_seq,
  ifc_pk_syntax_attribute_using_prefix,
  ifc_pk_syntax_attributed_declaration,
  ifc_pk_syntax_attributed_statement,
  ifc_pk_syntax_base_specifier,
  ifc_pk_syntax_base_specifier_list,
  ifc_pk_syntax_binary_fold_expression,
  ifc_pk_syntax_break_statement,
  ifc_pk_syntax_capture_default,
  ifc_pk_syntax_class_specifier,
  ifc_pk_syntax_compound_requirement,
  ifc_pk_syntax_compound_statement,
  ifc_pk_syntax_concept_definition,
  ifc_pk_syntax_condition_declaration,
  ifc_pk_syntax_continue_statement,
  ifc_pk_syntax_ctor_initializer,
  ifc_pk_syntax_decl_specifier_seq,
  ifc_pk_syntax_declaration_statement,
  ifc_pk_syntax_declarator,
  ifc_pk_syntax_decltype_specifier,
  ifc_pk_syntax_do_statement,
  ifc_pk_syntax_dynamic_exception_spec,
  ifc_pk_syntax_empty_statement,
  ifc_pk_syntax_enum_specifier,
  ifc_pk_syntax_enumerator_definition,
  ifc_pk_syntax_exception_declaration,
  ifc_pk_syntax_explicit_specifier,
  ifc_pk_syntax_expression,
  ifc_pk_syntax_expression_statement,
  ifc_pk_syntax_for_range_declaration,
  ifc_pk_syntax_for_statement,
  ifc_pk_syntax_function_body,
  ifc_pk_syntax_function_declarator,
  ifc_pk_syntax_function_definition,
  ifc_pk_syntax_function_try_block,
  ifc_pk_syntax_goto_statement,
  ifc_pk_syntax_handler,
  ifc_pk_syntax_handler_seq,
  ifc_pk_syntax_if_statement,
  ifc_pk_syntax_init_capture,
  ifc_pk_syntax_init_declarator,
  ifc_pk_syntax_init_statement,
  ifc_pk_syntax_labeled_statement,
  ifc_pk_syntax_lambda_declarator,
  ifc_pk_syntax_lambda_introducer,
  ifc_pk_syntax_mem_initializer,
  ifc_pk_syntax_member_declaration,
  ifc_pk_syntax_member_declarator,
  ifc_pk_syntax_member_function_declaration,
  ifc_pk_syntax_member_specification,
  ifc_pk_syntax_namespace_alias_definition,
  ifc_pk_syntax_nested_requirement,
  ifc_pk_syntax_new_declarator,
  ifc_pk_syntax_noexcept_specification,
  ifc_pk_syntax_non_type_template_argument,
  ifc_pk_syntax_parameter_declarator,
  ifc_pk_syntax_placeholder_type_specifier,
  ifc_pk_syntax_pointer_declarator,
  ifc_pk_syntax_range_based_for_statement,
  ifc_pk_syntax_requirement_body,
  ifc_pk_syntax_requires_clause,
  ifc_pk_syntax_return_statement,
  ifc_pk_syntax_seh_except,
  ifc_pk_syntax_seh_finally,
  ifc_pk_syntax_seh_leave,
  ifc_pk_syntax_seh_try,
  ifc_pk_syntax_simple_capture,
  ifc_pk_syntax_simple_declaration,
  ifc_pk_syntax_simple_requirement,
  ifc_pk_syntax_simple_type_specifier,
  ifc_pk_syntax_statement_seq,
  ifc_pk_syntax_static_assert_declaration,
  ifc_pk_syntax_structured_binding_declaration,
  ifc_pk_syntax_structured_binding_identifier,
  ifc_pk_syntax_super,
  ifc_pk_syntax_switch_statement,
  ifc_pk_syntax_template_argument_list,
  ifc_pk_syntax_template_declaration,
  ifc_pk_syntax_template_id,
  ifc_pk_syntax_template_parameter_list,
  ifc_pk_syntax_template_template_parameter,
  ifc_pk_syntax_this_capture,
  ifc_pk_syntax_trailing_return_type,
  ifc_pk_syntax_try_block,
  ifc_pk_syntax_tuple,
  ifc_pk_syntax_type_id,
  ifc_pk_syntax_type_id_list_element,
  ifc_pk_syntax_type_requirement,
  ifc_pk_syntax_type_specifier_seq,
  ifc_pk_syntax_type_template_argument,
  ifc_pk_syntax_type_template_parameter,
  ifc_pk_syntax_type_trait_intrinsic,
  ifc_pk_syntax_unary_fold_expression,
  ifc_pk_syntax_using_declaration,
  ifc_pk_syntax_using_declarator,
  ifc_pk_syntax_using_directive,
  ifc_pk_syntax_using_enum_declaration,
  ifc_pk_syntax_virtual_specifier_seq,
  ifc_pk_syntax_while_statement,
  ifc_pk_trait_alias_template,
  ifc_pk_trait_attribute,
  ifc_pk_trait_deduction_guides,
  ifc_pk_trait_deprecated,
  ifc_pk_trait_friend,
  ifc_pk_trait_mapping_expr,
  ifc_pk_trait_requires,
  ifc_pk_trait_specialization,
  ifc_pk_type_array,
  ifc_pk_type_base,
  ifc_pk_type_decltype,
  ifc_pk_type_designated,
  ifc_pk_type_expansion,
  ifc_pk_type_forall,
  ifc_pk_type_function,
  ifc_pk_type_fundamental,
  ifc_pk_type_lvalue_reference,
  ifc_pk_type_nonstatic_member_function,
  ifc_pk_type_placeholder,
  ifc_pk_type_pointer,
  ifc_pk_type_pointer_to_member,
  ifc_pk_type_qualified,
  ifc_pk_type_rvalue_reference,
  ifc_pk_type_syntactic,
  ifc_pk_type_syntax_tree,
  ifc_pk_type_tor,
  ifc_pk_type_tuple,
  ifc_pk_type_typename,
  ifc_pk_type_unaligned
};  /* an_ifc_partition_kind */


#define IFC_PARTITION_COUNT 297

/*
A method for mapping partition names to an_ifc_partition_kind values.  Used
when reading an IFC file to identify which partitions are which.  The list is
guaranteed to be sorted by the partition name (for binary search).
*/
struct an_ifc_partition_map {
  a_const_char* name;
                        /* The textual identifier for the IFC partition. */
  an_ifc_partition_kind
                kind;
                        /* The respective partition kind enumerator. */
};  /* an_ifc_partition_map */

EXTERN an_ifc_partition_map ifc_partition_map[IFC_PARTITION_COUNT]
#if VAR_INITIALIZERS
= {
  { ".msvc.trait.decl-attrs", ifc_pk_msvc_trait_decl_attrs },
  { ".msvc.trait.named-function-parameters",
                                 ifc_pk_msvc_trait_named_function_parameters },
  { ".msvc.trait.uuid", ifc_pk_msvc_trait_uuid },
  { ".msvc.trait.vendor-traits", ifc_pk_msvc_trait_vendor_traits },
  { "attr.basic", ifc_pk_attr_basic },
  { "attr.called", ifc_pk_attr_called },
  { "attr.elaborated", ifc_pk_attr_elaborated },
  { "attr.expanded", ifc_pk_attr_expanded },
  { "attr.factored", ifc_pk_attr_factored },
  { "attr.labeled", ifc_pk_attr_labeled },
  { "attr.scoped", ifc_pk_attr_scoped },
  { "attr.tuple", ifc_pk_attr_tuple },
  { "chart.multilevel", ifc_pk_chart_multilevel },
  { "chart.unilevel", ifc_pk_chart_unilevel },
  { "const.f64", ifc_pk_const_f64 },
  { "const.i64", ifc_pk_const_i64 },
  { "const.str", ifc_pk_const_str },
  { "decl.alias", ifc_pk_decl_alias },
  { "decl.bitfield", ifc_pk_decl_bitfield },
  { "decl.concept", ifc_pk_decl_concept },
  { "decl.constructor", ifc_pk_decl_constructor },
  { "decl.deduction-guide", ifc_pk_decl_deduction_guide },
  { "decl.destructor", ifc_pk_decl_destructor },
  { "decl.enum", ifc_pk_decl_enum },
  { "decl.enumerator", ifc_pk_decl_enumerator },
  { "decl.expansion", ifc_pk_decl_expansion },
  { "decl.explicit-instantiation", ifc_pk_decl_explicit_instantiation },
  { "decl.explicit-specialization", ifc_pk_decl_explicit_specialization },
  { "decl.field", ifc_pk_decl_field },
  { "decl.friend", ifc_pk_decl_friend },
  { "decl.function", ifc_pk_decl_function },
  { "decl.inherited-constructor", ifc_pk_decl_inherited_constructor },
  { "decl.intrinsic", ifc_pk_decl_intrinsic },
  { "decl.method", ifc_pk_decl_method },
  { "decl.parameter", ifc_pk_decl_parameter },
  { "decl.partial-specialization", ifc_pk_decl_partial_specialization },
  { "decl.property", ifc_pk_decl_property },
  { "decl.reference", ifc_pk_decl_reference },
  { "decl.scope", ifc_pk_decl_scope },
  { "decl.segment", ifc_pk_decl_segment },
  { "decl.specialization", ifc_pk_decl_specialization },
  { "decl.template", ifc_pk_decl_template },
  { "decl.temploid", ifc_pk_decl_temploid },
  { "decl.tuple", ifc_pk_decl_tuple },
  { "decl.using-declaration", ifc_pk_decl_using_declaration },
  { "decl.variable", ifc_pk_decl_variable },
  { "expr.alignof-type-id", ifc_pk_expr_alignof_type_id },
  { "expr.array-value", ifc_pk_expr_array_value },
  { "expr.assign-initializer", ifc_pk_expr_assign_initializer },
  { "expr.binary-fold", ifc_pk_expr_binary_fold },
  { "expr.call", ifc_pk_expr_call },
  { "expr.cast", ifc_pk_expr_cast },
  { "expr.class-subobject-value", ifc_pk_expr_class_subobject_value },
  { "expr.compound-string", ifc_pk_expr_compound_string },
  { "expr.condition", ifc_pk_expr_condition },
  { "expr.decl", ifc_pk_expr_decl },
  { "expr.designated-init", ifc_pk_expr_designated_init },
  { "expr.destructor-call", ifc_pk_expr_destructor_call },
  { "expr.dyad", ifc_pk_expr_dyad },
  { "expr.dynamic-dispatch", ifc_pk_expr_dynamic_dispatch },
  { "expr.empty", ifc_pk_expr_empty },
  { "expr.expansion", ifc_pk_expr_expansion },
  { "expr.expression-list", ifc_pk_expr_expression_list },
  { "expr.function-string", ifc_pk_expr_function_string },
  { "expr.hierarchy-conversion", ifc_pk_expr_hierarchy_conversion },
  { "expr.inheritance-path", ifc_pk_expr_inheritance_path },
  { "expr.initializer", ifc_pk_expr_initializer },
  { "expr.initializer-list", ifc_pk_expr_initializer_list },
  { "expr.lambda", ifc_pk_expr_lambda },
  { "expr.literal", ifc_pk_expr_literal },
  { "expr.member-access", ifc_pk_expr_member_access },
  { "expr.member-initializer", ifc_pk_expr_member_initializer },
  { "expr.monad", ifc_pk_expr_monad },
  { "expr.nullptr", ifc_pk_expr_nullptr },
  { "expr.packed-template-arguments", ifc_pk_expr_packed_template_arguments },
  { "expr.path", ifc_pk_expr_path },
  { "expr.placeholder", ifc_pk_expr_placeholder },
  { "expr.pointer", ifc_pk_expr_pointer },
  { "expr.product-type-value", ifc_pk_expr_product_type_value },
  { "expr.push-state", ifc_pk_expr_push_state },
  { "expr.qualified-name", ifc_pk_expr_qualified_name },
  { "expr.read", ifc_pk_expr_read },
  { "expr.requires", ifc_pk_expr_requires },
  { "expr.simple-identifier", ifc_pk_expr_simple_identifier },
  { "expr.sizeof-type", ifc_pk_expr_sizeof_type },
  { "expr.string-sequence", ifc_pk_expr_string_sequence },
  { "expr.strings", ifc_pk_expr_strings },
  { "expr.sum-type-value", ifc_pk_expr_sum_type_value },
  { "expr.syntax-tree", ifc_pk_expr_syntax_tree },
  { "expr.template-id", ifc_pk_expr_template_id },
  { "expr.template-reference", ifc_pk_expr_template_reference },
  { "expr.temporary", ifc_pk_expr_temporary },
  { "expr.this", ifc_pk_expr_this },
  { "expr.tokens", ifc_pk_expr_tokens },
  { "expr.triad", ifc_pk_expr_triad },
  { "expr.tuple", ifc_pk_expr_tuple },
  { "expr.type", ifc_pk_expr_type },
  { "expr.type-trait", ifc_pk_expr_type_trait },
  { "expr.typeid", ifc_pk_expr_typeid },
  { "expr.unary-fold", ifc_pk_expr_unary_fold },
  { "expr.unqualified-id", ifc_pk_expr_unqualified_id },
  { "expr.unresolved", ifc_pk_expr_unresolved },
  { "expr.virtual-function-conversion",
                                     ifc_pk_expr_virtual_function_conversion },
  { "form.spec", ifc_pk_form_spec },
  { "heap.attr", ifc_pk_heap_attr },
  { "heap.chart", ifc_pk_heap_chart },
  { "heap.decl", ifc_pk_heap_decl },
  { "heap.expr", ifc_pk_heap_expr },
  { "heap.form", ifc_pk_heap_form },
  { "heap.pp", ifc_pk_heap_pp },
  { "heap.stmt", ifc_pk_heap_stmt },
  { "heap.syn", ifc_pk_heap_syn },
  { "heap.type", ifc_pk_heap_type },
  { "macro.function-like", ifc_pk_macro_function_like },
  { "macro.object-like", ifc_pk_macro_object_like },
  { "module.exported", ifc_pk_module_exported },
  { "module.imported", ifc_pk_module_imported },
  { "name.conversion", ifc_pk_name_conversion },
  { "name.guide", ifc_pk_name_guide },
  { "name.literal", ifc_pk_name_literal },
  { "name.operator", ifc_pk_name_operator },
  { "name.source-file", ifc_pk_name_source_file },
  { "name.specialization", ifc_pk_name_specialization },
  { "name.template", ifc_pk_name_template },
  { "pp.catenate", ifc_pk_pp_catenate },
  { "pp.char", ifc_pk_pp_char },
  { "pp.header", ifc_pk_pp_header },
  { "pp.ident", ifc_pk_pp_ident },
  { "pp.junk", ifc_pk_pp_junk },
  { "pp.key", ifc_pk_pp_key },
  { "pp.num", ifc_pk_pp_num },
  { "pp.op", ifc_pk_pp_op },
  { "pp.param", ifc_pk_pp_param },
  { "pp.paren", ifc_pk_pp_paren },
  { "pp.pragma", ifc_pk_pp_pragma },
  { "pp.space", ifc_pk_pp_space },
  { "pp.string", ifc_pk_pp_string },
  { "pp.to-string", ifc_pk_pp_to_string },
  { "pp.tuple", ifc_pk_pp_tuple },
  { "scope.desc", ifc_pk_scope_desc },
  { "scope.member", ifc_pk_scope_member },
  { "src.line", ifc_pk_src_line },
  { "src.sentence", ifc_pk_src_sentence },
  { "src.word", ifc_pk_src_word },
  { "stmt.block", ifc_pk_stmt_block },
  { "stmt.break", ifc_pk_stmt_break },
  { "stmt.case", ifc_pk_stmt_case },
  { "stmt.continue", ifc_pk_stmt_continue },
  { "stmt.default", ifc_pk_stmt_default },
  { "stmt.do-while", ifc_pk_stmt_do_while },
  { "stmt.empty", ifc_pk_stmt_empty },
  { "stmt.expansion", ifc_pk_stmt_expansion },
  { "stmt.expression", ifc_pk_stmt_expression },
  { "stmt.for", ifc_pk_stmt_for },
  { "stmt.if", ifc_pk_stmt_if },
  { "stmt.return", ifc_pk_stmt_return },
  { "stmt.switch", ifc_pk_stmt_switch },
  { "stmt.variable", ifc_pk_stmt_variable },
  { "stmt.while", ifc_pk_stmt_while },
  { "syntax.access-specifier", ifc_pk_syntax_access_specifier },
  { "syntax.alias-declaration", ifc_pk_syntax_alias_declaration },
  { "syntax.alignas", ifc_pk_syntax_alignas },
  { "syntax.array-declarator", ifc_pk_syntax_array_declarator },
  { "syntax.array-index", ifc_pk_syntax_array_index },
  { "syntax.array-or-function-declarator",
                                  ifc_pk_syntax_array_or_function_declarator },
  { "syntax.asm-statement", ifc_pk_syntax_asm_statement },
  { "syntax.attribute", ifc_pk_syntax_attribute },
  { "syntax.attribute-argument-clause",
                                     ifc_pk_syntax_attribute_argument_clause },
  { "syntax.attribute-specifier", ifc_pk_syntax_attribute_specifier },
  { "syntax.attribute-specifier-seq", ifc_pk_syntax_attribute_specifier_seq },
  { "syntax.attribute-using-prefix", ifc_pk_syntax_attribute_using_prefix },
  { "syntax.attributed-declaration", ifc_pk_syntax_attributed_declaration },
  { "syntax.attributed-statement", ifc_pk_syntax_attributed_statement },
  { "syntax.base-specifier", ifc_pk_syntax_base_specifier },
  { "syntax.base-specifier-list", ifc_pk_syntax_base_specifier_list },
  { "syntax.binary-fold-expression", ifc_pk_syntax_binary_fold_expression },
  { "syntax.break-statement", ifc_pk_syntax_break_statement },
  { "syntax.capture-default", ifc_pk_syntax_capture_default },
  { "syntax.class-specifier", ifc_pk_syntax_class_specifier },
  { "syntax.compound-requirement", ifc_pk_syntax_compound_requirement },
  { "syntax.compound-statement", ifc_pk_syntax_compound_statement },
  { "syntax.concept-definition", ifc_pk_syntax_concept_definition },
  { "syntax.condition-declaration", ifc_pk_syntax_condition_declaration },
  { "syntax.continue-statement", ifc_pk_syntax_continue_statement },
  { "syntax.ctor-initializer", ifc_pk_syntax_ctor_initializer },
  { "syntax.decl-specifier-seq", ifc_pk_syntax_decl_specifier_seq },
  { "syntax.declaration-statement", ifc_pk_syntax_declaration_statement },
  { "syntax.declarator", ifc_pk_syntax_declarator },
  { "syntax.decltype-specifier", ifc_pk_syntax_decltype_specifier },
  { "syntax.do-statement", ifc_pk_syntax_do_statement },
  { "syntax.dynamic-exception-spec", ifc_pk_syntax_dynamic_exception_spec },
  { "syntax.empty-statement", ifc_pk_syntax_empty_statement },
  { "syntax.enum-specifier", ifc_pk_syntax_enum_specifier },
  { "syntax.enumerator-definition", ifc_pk_syntax_enumerator_definition },
  { "syntax.exception-declaration", ifc_pk_syntax_exception_declaration },
  { "syntax.explicit-specifier", ifc_pk_syntax_explicit_specifier },
  { "syntax.expression", ifc_pk_syntax_expression },
  { "syntax.expression-statement", ifc_pk_syntax_expression_statement },
  { "syntax.for-range-declaration", ifc_pk_syntax_for_range_declaration },
  { "syntax.for-statement", ifc_pk_syntax_for_statement },
  { "syntax.function-body", ifc_pk_syntax_function_body },
  { "syntax.function-declarator", ifc_pk_syntax_function_declarator },
  { "syntax.function-definition", ifc_pk_syntax_function_definition },
  { "syntax.function-try-block", ifc_pk_syntax_function_try_block },
  { "syntax.goto-statement", ifc_pk_syntax_goto_statement },
  { "syntax.handler", ifc_pk_syntax_handler },
  { "syntax.handler-seq", ifc_pk_syntax_handler_seq },
  { "syntax.if-statement", ifc_pk_syntax_if_statement },
  { "syntax.init-capture", ifc_pk_syntax_init_capture },
  { "syntax.init-declarator", ifc_pk_syntax_init_declarator },
  { "syntax.init-statement", ifc_pk_syntax_init_statement },
  { "syntax.labeled-statement", ifc_pk_syntax_labeled_statement },
  { "syntax.lambda-declarator", ifc_pk_syntax_lambda_declarator },
  { "syntax.lambda-introducer", ifc_pk_syntax_lambda_introducer },
  { "syntax.mem-initializer", ifc_pk_syntax_mem_initializer },
  { "syntax.member-declaration", ifc_pk_syntax_member_declaration },
  { "syntax.member-declarator", ifc_pk_syntax_member_declarator },
  { "syntax.member-function-declaration",
                                   ifc_pk_syntax_member_function_declaration },
  { "syntax.member-specification", ifc_pk_syntax_member_specification },
  { "syntax.namespace-alias-definition",
                                    ifc_pk_syntax_namespace_alias_definition },
  { "syntax.nested-requirement", ifc_pk_syntax_nested_requirement },
  { "syntax.new-declarator", ifc_pk_syntax_new_declarator },
  { "syntax.noexcept-specification", ifc_pk_syntax_noexcept_specification },
  { "syntax.non-type-template-argument",
                                    ifc_pk_syntax_non_type_template_argument },
  { "syntax.parameter-declarator", ifc_pk_syntax_parameter_declarator },
  { "syntax.placeholder-type-specifier",
                                    ifc_pk_syntax_placeholder_type_specifier },
  { "syntax.pointer-declarator", ifc_pk_syntax_pointer_declarator },
  { "syntax.range-based-for-statement",
                                     ifc_pk_syntax_range_based_for_statement },
  { "syntax.requirement-body", ifc_pk_syntax_requirement_body },
  { "syntax.requires-clause", ifc_pk_syntax_requires_clause },
  { "syntax.return-statement", ifc_pk_syntax_return_statement },
  { "syntax.seh-except", ifc_pk_syntax_seh_except },
  { "syntax.seh-finally", ifc_pk_syntax_seh_finally },
  { "syntax.seh-leave", ifc_pk_syntax_seh_leave },
  { "syntax.seh-try", ifc_pk_syntax_seh_try },
  { "syntax.simple-capture", ifc_pk_syntax_simple_capture },
  { "syntax.simple-declaration", ifc_pk_syntax_simple_declaration },
  { "syntax.simple-requirement", ifc_pk_syntax_simple_requirement },
  { "syntax.simple-type-specifier", ifc_pk_syntax_simple_type_specifier },
  { "syntax.statement-seq", ifc_pk_syntax_statement_seq },
  { "syntax.static-assert-declaration",
                                     ifc_pk_syntax_static_assert_declaration },
  { "syntax.structured-binding-declaration",
                                ifc_pk_syntax_structured_binding_declaration },
  { "syntax.structured-binding-identifier",
                                 ifc_pk_syntax_structured_binding_identifier },
  { "syntax.super", ifc_pk_syntax_super },
  { "syntax.switch-statement", ifc_pk_syntax_switch_statement },
  { "syntax.template-argument-list", ifc_pk_syntax_template_argument_list },
  { "syntax.template-declaration", ifc_pk_syntax_template_declaration },
  { "syntax.template-id", ifc_pk_syntax_template_id },
  { "syntax.template-parameter-list", ifc_pk_syntax_template_parameter_list },
  { "syntax.template-template-parameter",
                                   ifc_pk_syntax_template_template_parameter },
  { "syntax.this-capture", ifc_pk_syntax_this_capture },
  { "syntax.trailing-return-type", ifc_pk_syntax_trailing_return_type },
  { "syntax.try-block", ifc_pk_syntax_try_block },
  { "syntax.tuple", ifc_pk_syntax_tuple },
  { "syntax.type-id", ifc_pk_syntax_type_id },
  { "syntax.type-id-list-element", ifc_pk_syntax_type_id_list_element },
  { "syntax.type-requirement", ifc_pk_syntax_type_requirement },
  { "syntax.type-specifier-seq", ifc_pk_syntax_type_specifier_seq },
  { "syntax.type-template-argument", ifc_pk_syntax_type_template_argument },
  { "syntax.type-template-parameter", ifc_pk_syntax_type_template_parameter },
  { "syntax.type-trait-intrinsic", ifc_pk_syntax_type_trait_intrinsic },
  { "syntax.unary-fold-expression", ifc_pk_syntax_unary_fold_expression },
  { "syntax.using-declaration", ifc_pk_syntax_using_declaration },
  { "syntax.using-declarator", ifc_pk_syntax_using_declarator },
  { "syntax.using-directive", ifc_pk_syntax_using_directive },
  { "syntax.using-enum-declaration", ifc_pk_syntax_using_enum_declaration },
  { "syntax.virtual-specifier-seq", ifc_pk_syntax_virtual_specifier_seq },
  { "syntax.while-statement", ifc_pk_syntax_while_statement },
  { "trait.alias-template", ifc_pk_trait_alias_template },
  { "trait.attribute", ifc_pk_trait_attribute },
  { "trait.deduction-guides", ifc_pk_trait_deduction_guides },
  { "trait.deprecated", ifc_pk_trait_deprecated },
  { "trait.friend", ifc_pk_trait_friend },
  { "trait.mapping-expr", ifc_pk_trait_mapping_expr },
  { "trait.requires", ifc_pk_trait_requires },
  { "trait.specialization", ifc_pk_trait_specialization },
  { "type.array", ifc_pk_type_array },
  { "type.base", ifc_pk_type_base },
  { "type.decltype", ifc_pk_type_decltype },
  { "type.designated", ifc_pk_type_designated },
  { "type.expansion", ifc_pk_type_expansion },
  { "type.forall", ifc_pk_type_forall },
  { "type.function", ifc_pk_type_function },
  { "type.fundamental", ifc_pk_type_fundamental },
  { "type.lvalue-reference", ifc_pk_type_lvalue_reference },
  { "type.nonstatic-member-function", ifc_pk_type_nonstatic_member_function },
  { "type.placeholder", ifc_pk_type_placeholder },
  { "type.pointer", ifc_pk_type_pointer },
  { "type.pointer-to-member", ifc_pk_type_pointer_to_member },
  { "type.qualified", ifc_pk_type_qualified },
  { "type.rvalue-reference", ifc_pk_type_rvalue_reference },
  { "type.syntactic", ifc_pk_type_syntactic },
  { "type.syntax-tree", ifc_pk_type_syntax_tree },
  { "type.tor", ifc_pk_type_tor },
  { "type.tuple", ifc_pk_type_tuple },
  { "type.typename", ifc_pk_type_typename },
  { "type.unaligned", ifc_pk_type_unaligned }
}  /* ifc_partition_map */
#endif /* VAR_INITIALIZERS */
;


/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2022 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
