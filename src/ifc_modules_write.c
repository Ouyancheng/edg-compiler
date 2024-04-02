/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2024 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

ifc_modules_write.c -- IFC writing code.

*/

/* Header files common to all files. */
#include "fe_common.h"

/* Additional header files. */
#include "ifc_modules.h"
#include "ifc_map_functions.h"
#include "il_walk.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

using an_ifc_output_buffer = Dyn_array<char, General_allocator>;
                        /* The type used to hold the in-memory bytes. */

namespace {

/*
This structure represents an IFC partition via a resizable in memory buffer.
*/
struct an_ifc_output_partition {
  an_ifc_output_partition(size_t part_name_offset_val,
                          size_t element_size_val)
    : part_name_offset(part_name_offset_val), element_size(element_size_val)
    {}

  inline size_t new_element(char ***start, size_t *byte_offset);
  inline size_t new_elements(size_t num_nodes);
  inline void fetch_element(char ***start, size_t *byte_offset, size_t index);

  a_const_char* get_bytes() const
    { return this->contents.begin(); }
  size_t get_num_bytes() const
    { return this->contents.length(); }

  const size_t  part_name_offset;
                        /* The offset into the string table where this
                           partition's name can be retrieved. */
  const size_t  element_size;
                        /* The number of bytes for an element. */
private:
  an_ifc_output_buffer
                contents = {};
                        /* The bytes held by the partition. */
  char          *content_start = NULL;
                        /* A pointer to the start of the partition bytes. */
};  /* an_ifc_output_partition */


size_t an_ifc_output_partition::new_element(char   ***start,
                                            size_t *byte_offset)
/*
Construct a new IFC output node in the current partition.  *start is updated to
be a pointer to the pointer representing the start of the partition byte
buffer.  *byte_offset is updated to be the offset into the partition byte
buffer where the node starts.  Return the index into the partition.
*/
{
  size_t orig_size = this->contents.length();
  size_t capacity = this->contents.capacity();

  /* Check to see if this will push the array over capacity; if so, increase
     the capacity. */
  if (orig_size + this->element_size > capacity) {
    this->contents.reserve(capacity * 2);
  }  /* if */
  /* Insert the space for this new element. */
  this->contents.resize(orig_size + this->element_size, '\0');
  /* Update the content start pointer in case the buffer was reallocated. */
  this->content_start = this->contents.begin();
  *start = &this->content_start;
  *byte_offset = orig_size;
  return orig_size / this->element_size;
}  /* an_ifc_output_partition::new_element */


size_t an_ifc_output_partition::new_elements(size_t num_nodes)
/*
Construct the given number of new IFC output nodes in the current partition.
Return the index into the partition.
*/
{
  size_t orig_size = this->contents.length();
  size_t capacity = this->contents.capacity();
  size_t new_elements_size = num_nodes * this->element_size;


  /* Check to see if this will push the array over capacity; if so, increase
     the capacity. */
  if (orig_size + new_elements_size > capacity) {
    size_t reserve_amount = max_val(capacity * 2, new_elements_size);

    this->contents.reserve(reserve_amount);
  }  /* if */
  /* Insert the space for this new element. */
  this->contents.resize(orig_size + (this->element_size * num_nodes), '\0');
  /* Update the content start pointer in case the buffer was reallocated. */
  this->content_start = this->contents.begin();
  return orig_size / this->element_size;
}  /* an_ifc_output_partition::new_elements */


inline void an_ifc_output_partition::fetch_element(char   ***start,
                                                   size_t *byte_offset,
                                                   size_t index)
/*
Fetch the IFC output node in the current partition at the given index.  *start
is updated to be a pointer to the pointer representing the start of the
partition byte buffer.  *byte_offset is updated to be the offset into the
partition byte buffer where the node starts.
*/
{
  *start = &this->content_start;
  *byte_offset = index * this->element_size;
}  /* an_ifc_output_partition::fetch_element */


struct an_ifc_output_token_cache;


/*
This structure represents the IFC file we're building up in memory.
*/
struct an_ifc_output_state {
  an_ifc_output_state(an_ifc_module_file *output_file_val);
  ~an_ifc_output_state();

  inline an_ifc_module_file *get_file() const
    { return this->output_file; }

  inline size_t add_to_string_table(a_const_char *bytes, size_t num_bytes);
  inline size_t add_to_string_table(a_const_char *string_text);

  template<typename an_ifc_Node_type>
  inline size_t alloc_node(an_ifc_Node_type *result);
  template<typename an_ifc_Node_type>
  inline size_t alloc_node_block(size_t num_nodes);

  template<typename an_ifc_Node_type>
  inline an_ifc_chart_index alloc_chart(an_ifc_Node_type *result);
  template<typename an_ifc_Node_type>
  inline an_ifc_decl_index alloc_decl(an_ifc_Node_type *result);
  template<typename an_ifc_Node_type>
  inline an_ifc_type_index alloc_type(an_ifc_Node_type *result);
  template<typename an_ifc_Node_type>
  inline an_ifc_name_index alloc_name(an_ifc_Node_type *result);
  template<typename an_ifc_Node_type>
  inline an_ifc_edg_constant_index alloc_constant(an_ifc_Node_type *result);

  template<typename an_ifc_Node_type>
  inline an_ifc_edg_complex_token_index alloc_complex_token(
                                                     an_ifc_Node_type *result);
  inline an_ifc_expr_index alloc_token_cache(
                                const an_ifc_output_token_cache &output_cache);

  template<typename an_ifc_Node_type>
  inline void fetch_node(an_ifc_Node_type *result, size_t index);

  void set_global_scope(an_ifc_scope_offset scope_offset)
    { this->global_scope = scope_offset; }

  a_boolean write();
private:
  template<typename an_ifc_Node_type>
  an_ifc_output_partition *get_or_init_partition();
  an_ifc_output_partition *get_partition(an_ifc_partition_kind kind)
    { return this->partitions[kind - 1]; }
  void set_partition(an_ifc_partition_kind   kind,
                     an_ifc_output_partition *value)
    { this->partitions[kind - 1] = value; }
  an_ifc_module_file
                *output_file;
                        /* The target IFC module file. */
  an_ifc_scope_offset
                global_scope = {};
                        /* The global scope. */
  an_ifc_output_buffer
                string_table = {};
                        /* The string table's bytes. */
  size_t        source_file_name_offset = 0;
                        /* The offset into the string table representing
                           the source file name. */
  an_ifc_output_partition
                *partitions[IFC_PARTITION_COUNT] = {};
                        /* An array of IFC output partition pointers
                           that are initialized as needed. */
};  /* an_ifc_output_state */


an_ifc_output_state::an_ifc_output_state(an_ifc_module_file *output_file_val)
/*
Construct a new IFC output state writing to the given IFC module file.
*/
  : output_file(output_file_val)
{
  /* Add a null character to the start of the string table so that a 0 value
     TextOffset results in a null string. */
  (void)this->add_to_string_table("", /*num_bytes=*/1);

  /* Add the source file name to the string table. */
  an_ifc_module_file_write_state &write_state =
                                          this->output_file->get_write_state();
  if (write_state.source_file_name != NULL) {
    this->source_file_name_offset = this->add_to_string_table(
                                                 write_state.source_file_name);
  }  /* if */
}  /* an_ifc_output_state::an_ifc_output_state */


an_ifc_output_state::~an_ifc_output_state()
/*
Clean up the IFC output state.
*/
{
  for (size_t i = 0; i < IFC_PARTITION_COUNT; ++i) {
    delete_general(partitions[i]);
  }  /* for */
}  /* an_ifc_output_state::~an_ifc_output_state */


size_t an_ifc_output_state::add_to_string_table(a_const_char *bytes,
                                                size_t       num_bytes)
/*
Insert the given number of bytes into the string table.  Return the byte offset
into the string table where the string starts.
*/
{
  size_t start = this->string_table.length();

  this->string_table.insert(start, bytes, num_bytes);
  return start;
}  /* an_ifc_output_state::add_to_string_table */


size_t an_ifc_output_state::add_to_string_table(a_const_char *string_text)
/*
Insert the given null terminated string into the string table.  Return the byte
offset into the string table where the string starts.
*/
{
  size_t start = this->string_table.length();
  size_t num_bytes = strlen(string_text) + 1;

  this->string_table.insert(start, string_text, num_bytes);
  return start;
}  /* an_ifc_output_state::add_to_string_table */


template<typename an_ifc_Node_type>
size_t an_ifc_output_state::alloc_node(an_ifc_Node_type *result)
/*
Allocate a node in its corresponding output partition.  Set *result to the
allocated node.  Return the node's index into the partition.
*/
{
  an_ifc_output_partition
                *output_part = this->get_or_init_partition<an_ifc_Node_type>();
  char          **start;
  size_t        byte_offset;
  size_t        index = output_part->new_element(&start, &byte_offset);
  an_ifc_Node_type
                constructed_value(this->output_file, start, byte_offset);
  *result = constructed_value;
  return index;
}  /* an_ifc_output_state::alloc_node */


template<typename an_ifc_Node_type>
size_t an_ifc_output_state::alloc_node_block(size_t num_nodes)
/*
Allocate the given number of nodes in their corresponding output partition.
Return the first node in the block's index into the partition.
*/
{
  an_ifc_output_partition
                *output_part = this->get_or_init_partition<an_ifc_Node_type>();

  return output_part->new_elements(num_nodes);
}  /* an_ifc_output_state::alloc_node_block */


template<typename an_ifc_Node_type>
an_ifc_chart_index an_ifc_output_state::alloc_chart(an_ifc_Node_type *result)
/*
Allocate a chart node in its corresponding output partition.  Set *result to
the allocated node.  Return the node's chart index.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  size_t        part_offset = this->alloc_node(result);
  an_ifc_chart_sort
                chart_sort = to_chart_sort(part_kind);

  return an_ifc_chart_index(result->get_file(), chart_sort, part_offset);
}  /* an_ifc_output_state::alloc_chart */


template<typename an_ifc_Node_type>
an_ifc_decl_index an_ifc_output_state::alloc_decl(an_ifc_Node_type *result)
/*
Allocate a declaration node in its corresponding output partition.  Set *result
to the allocated node.  Return the node's declaration index.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  size_t        part_offset = this->alloc_node(result);
  an_ifc_decl_sort
                decl_sort = to_decl_sort(part_kind);

  return an_ifc_decl_index(result->get_file(), decl_sort, part_offset);
}  /* an_ifc_output_state::alloc_decl */


template<typename an_ifc_Node_type>
an_ifc_type_index an_ifc_output_state::alloc_type(an_ifc_Node_type *result)
/*
Allocate a type node in its corresponding output partition.  Set *result to the
allocated node.  Return the node's type index.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  size_t        part_offset = this->alloc_node(result);
  an_ifc_type_sort
                type_sort = to_type_sort(part_kind);

  return an_ifc_type_index(result->get_file(), type_sort, part_offset);
}  /* an_ifc_output_state::alloc_type */


template<typename an_ifc_Node_type>
an_ifc_name_index an_ifc_output_state::alloc_name(an_ifc_Node_type *result)
/*
Allocate a name node in its corresponding output partition.  Set *result to the
allocated node.  Return the node's name index.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  size_t        part_offset = this->alloc_node(result);
  an_ifc_name_sort
                name_sort = to_name_sort(part_kind);

  return an_ifc_name_index(result->get_file(), name_sort, part_offset);
}  /* an_ifc_output_state::alloc_name */


template<typename an_ifc_Node_type>
an_ifc_edg_constant_index an_ifc_output_state::alloc_constant(
                                                      an_ifc_Node_type *result)
/*
Allocate a constant node in its corresponding output partition.  Set *result to
the allocated node.  Return the node's constant index.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  size_t        part_offset = this->alloc_node(result);
  an_ifc_edg_constant_sort
                constant_sort = to_edg_constant_sort(part_kind);

  return an_ifc_edg_constant_index(result->get_file(), constant_sort,
                                   part_offset);
}  /* an_ifc_output_state::alloc_constant */


template<typename an_ifc_Node_type>
an_ifc_edg_complex_token_index an_ifc_output_state::alloc_complex_token(
                                                      an_ifc_Node_type *result)
/*
Allocate a complex token node in its corresponding output partition.  Set
*result to the allocated node.  Return the node's complex token index.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  size_t        part_offset = this->alloc_node(result);
  an_ifc_edg_complex_token_sort
                token_sort = to_edg_complex_token_sort(part_kind);

  return an_ifc_edg_complex_token_index(result->get_file(), token_sort,
                                        part_offset);
}  /* an_ifc_output_state::alloc_complex_token */


template<typename an_ifc_Node_type>
void an_ifc_output_state::fetch_node(an_ifc_Node_type *result,
                                     size_t           index)
/*
Fetch a node from its corresponding output partition at the given index.  Set
*result to the fetched node.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  an_ifc_output_partition
                *output_part = this->get_partition(part_kind);
  char          **start;
  size_t        byte_offset;

  output_part->fetch_element(&start, &byte_offset, index);

  an_ifc_Node_type constructed_value(this->output_file, start, byte_offset);
  *result = constructed_value;
}  /* an_ifc_output_state::fetch_node */


/*
This structure encapsulates various pieces of meta information that are
required for the IFC file format for an individual partition.
*/
struct an_ifc_output_partition_metadata {
  an_ifc_partition_kind
                kind;   /* The partition kind of this partition (this can be
                           used to retrieve the output partition from the array
                           of partitions -- i.e.,
                           an_ifc_output_state::partitions). */
  an_ifc_byte_offset_storage
                relative_offset;
                        /* The byte offset from the start of partition writing.
                           This is different from the byte offset from the
                           start of the file as it does not factor in the size
                           of preceding non-partition data (e.g., the table of
                           contents). */
};  /* an_ifc_output_partition_metadata */

}  /* namespace */
namespace detail {

template<>
struct Is_trivially_destructible_edg_impl<an_ifc_output_partition_metadata> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

}  /* namespace detail */
namespace {

/*
This structure encapsulates various pieces of meta information that are
required for the IFC file format.
*/
struct an_ifc_output_metadata {
  Dyn_array<an_ifc_output_partition_metadata, General_allocator>
                used_partitions = {};
                        /* The metadata for the partitions that will be written
                           to the IFC file. */
  an_ifc_byte_offset_storage
                num_partition_bytes = 0;
                        /* The total number of bytes used for the partitions
                           that will be written to the IFC file. */
  an_ifc_cardinality_storage
                num_string_table_bytes = 0;
                        /* The number of bytes used for the string table that
                           will be written to the IFC file. */
};  /* an_ifc_output_metadata */

}  /* namespace */

template<typename an_ifc_Node_type>
static void append_node_to_file(const an_ifc_Node_type &node)
/*
Append the given IFC node contents to the file.
*/
{
  using a_storage_type = typename an_ifc_Node_type::storage_type;
  size_t storarge_len = get_ifc_buffer_size<a_storage_type>(node.get_file());

  (void)fwrite(node.get_storage(), storarge_len, /*num_to_write=*/1,
               node.get_file()->f_module);
}  /* append_node_to_file */


static void write_ifc_magic_bytes(an_ifc_module_file *file)
/*
Write the series of magic bytes identifying the IFC file.
*/
{
  /* If this assertion fails the front end is presumably trying to write a
     Microsoft compatible IFC file and that's not currently possible. */
  check_assertion(file->module_kind == mk_edg_ifc);
  /* FIXME: If the file header ever changes size, we'll need to change this. */
  (void)fwrite(edg_ifc_magic_numbers, sizeof(edg_ifc_magic_numbers),
               /*num_to_write=*/1, file->f_module);
}  /* write_ifc_magic_bytes */


static an_ifc_architecture_sort get_target_ifc_architecture()
/*
Return the IFC architecture sort value corresponding to the current target.
*/
{
  an_ifc_architecture_sort result;

  if (target_is_x86_based()) {
    if (target_is_64_bits()) {
      result = ifc_as_x64;
    } else {
      result = ifc_as_x86;
    }  /* if */
  } else {
    /* FIXME: Handle ARM. */
    unexpected_condition();
  }  /* if */
  return result;
}  /* get_target_ifc_architecture */


static an_ifc_byte_offset_storage get_partitions_start(
                                       an_ifc_module_file           *file,
                                       const an_ifc_output_metadata &metadata)
/*
Return the byte offset for the start of the partitions.
*/
{
  an_ifc_byte_offset_storage result = 0;

  /* Add the bytes for the magic numbers. */
  result += sizeof(edg_ifc_magic_numbers);
  /* Add the bytes for the file header. */
  result += get_ifc_buffer_size<an_ifc_file_header_storage>(file);
  return result;
}  /* get_partitions_start */


static an_ifc_byte_offset_storage get_string_table_start(
                                       an_ifc_module_file           *file,
                                       const an_ifc_output_metadata &metadata)
/*
Return the byte offset for the start of the string table given the associated
module file and output metadata.
*/
{
  an_ifc_byte_offset_storage result = 0;

  /* Add the bytes for the start of the partitions. */
  result += get_partitions_start(file, metadata);
  /* Add the bytes for the partitions themselves. */
  result += metadata.num_partition_bytes;
  return result;
}  /* get_string_table_start */


static an_ifc_byte_offset_storage get_toc_start(
                                       an_ifc_module_file           *file,
                                       const an_ifc_output_metadata &metadata)
/*
Return the byte offset for the start of the table of contents given the
associated module file and output metadata.
*/
{
  an_ifc_byte_offset_storage result = 0;

  /* Add the bytes for the start of the string table. */
  result += get_string_table_start(file, metadata);
  /* Add the bytes for the string table. */
  result += metadata.num_string_table_bytes;
  return result;
}  /* get_toc_start */


static void write_ifc_header(an_ifc_module_file           *file,
                             an_ifc_text_offset_storage   source_file_name,
                             an_ifc_scope_offset          global_scope,
                             const an_ifc_output_metadata &metadata)
/*
Create and write out the IFC file header for the given IFC module file using
the associated offset of the source file name into the string table, global
scope offset, and metadata.
*/
{
  /* Create an instance of an_ifc_file_header_storage on the stack and
     use it to set up, then write, the file header. */
  an_ifc_file_header file_header(file);
  an_ifc_version     ifc_major_version(file, file->version_major);
  an_ifc_version     ifc_minor_version(file, file->version_minor);

  /* Set the IFC version information. */
  set_ifc_major_version(&file_header, ifc_major_version);
  set_ifc_minor_version(&file_header, ifc_minor_version);
  /* Set the ABI version. */
  set_ifc_arch(&file_header, get_target_ifc_architecture());

  /* Set the string table information. */
  an_ifc_byte_offset ifc_string_table_start(file,
                                            get_string_table_start(file,
                                                                   metadata));
  an_ifc_cardinality ifc_string_table_length(file,
                                             metadata.num_string_table_bytes);
  set_ifc_string_table_bytes(&file_header, ifc_string_table_start);
  set_ifc_string_table_size(&file_header, ifc_string_table_length);

  /* Set the source path information. */
  an_ifc_text_offset ifc_source_path(file, source_file_name);
  set_ifc_src_path(&file_header, ifc_source_path);

  /* Set the primary scope information. */
  set_ifc_global_scope(&file_header, global_scope);

  /* Set the table of contents offset. */
  an_ifc_byte_offset ifc_toc_start(file, get_toc_start(file, metadata));
  set_ifc_toc(&file_header, ifc_toc_start);

  /* Set the number of partitions. */
  an_ifc_cardinality ifc_partition_count(file,
                                         metadata.used_partitions.length());
  set_ifc_partition_count(&file_header, ifc_partition_count);
  /* Do the append. */
  append_node_to_file(file_header);
}  /* write_ifc_header */


static void write_ifc_partition(an_ifc_module_file            *file,
                                const an_ifc_output_partition &partition)
/*
Append the contents of the given partition to the given module output file.
*/
{
  (void)fwrite(partition.get_bytes(), partition.get_num_bytes(),
               /*num_to_write=*/1, file->f_module);
}  /* write_ifc_partition */


static void write_string_table(an_ifc_module_file         *file,
                               const an_ifc_output_buffer &string_table_bytes)
/*
Append the contents of the given string table to the given module output file.
*/
{
  (void)fwrite(string_table_bytes.begin(), string_table_bytes.length(),
               /*num_to_write=*/1, file->f_module);
}  /* write_string_table */


static void write_table_of_contents_entry(
                   an_ifc_module_file                     *file,
                   an_ifc_byte_offset_storage             start_of_partitions,
                   const an_ifc_output_partition          &partition,
                   const an_ifc_output_partition_metadata &partition_metadata)
/*
Write the table of contents entry for the given partition into the given module
output file.

The start of partitions value should be the byte offset from the start of the
file to where the partition bytes have been written.  Partition metadata is the
associated partition metadata for the partition.
*/
{
  an_ifc_partition   toc_entry(file);
  an_ifc_text_offset ifc_name_offset(file, partition.part_name_offset);

  /* Set the name of the partition. */
  set_ifc_name(&toc_entry, ifc_name_offset);

  /* Set the offset where this partition starts in the file. */
  an_ifc_byte_offset ifc_part_offset(file,
                                     (start_of_partitions +
                                      partition_metadata.relative_offset));
  set_ifc_offset(&toc_entry, ifc_part_offset);

  /* Set the number of elements in this partition. */
  an_ifc_cardinality_storage num_elements = (partition.get_num_bytes() /
                                             partition.element_size);
  an_ifc_cardinality         ifc_num_elements(file, num_elements);
  set_ifc_cardinality(&toc_entry, ifc_num_elements);

  /* Set the entry size. */
  an_ifc_entity_size_storage entity_size = partition.element_size;
  an_ifc_entity_size         ifc_entity_size(file, entity_size);
  set_ifc_entry_size(&toc_entry, ifc_entity_size);

  append_node_to_file(toc_entry);
}  /* write_table_of_contents_entry */


static an_ifc_output_metadata make_output_metadata(
                                     an_ifc_output_partition    **partitions,
                                     const an_ifc_output_buffer &string_table)
/*
Given the array of partitions and string table for an IFC output state,
return the computed IFC output metadata.
*/
{
  an_ifc_output_metadata     result;
  an_ifc_byte_offset_storage relative_start = 0;

  for (size_t i = 0; i < IFC_PARTITION_COUNT; ++i) {
    const an_ifc_output_partition *partition = partitions[i];

    if (partition == NULL) {
      continue;
    }  /* if */

    an_ifc_output_partition_metadata part_metadata;
    /* Add the partition metadata for this partition. */
    part_metadata.kind = (an_ifc_partition_kind)(i + 1);
    part_metadata.relative_offset = relative_start;
    result.used_partitions.push_back(part_metadata);

    /* Update the relative_start for the next partition. */
    size_t bytes_in_partition = partition->get_num_bytes();
    relative_start += bytes_in_partition;
    /* Update the total number of bytes used by all partitions. */
    result.num_partition_bytes += bytes_in_partition;
  }  /* for */
  result.num_string_table_bytes = string_table.length();
  return result;
}  /* make_output_metadata */

namespace {

a_boolean an_ifc_output_state::write()
/*
Write the IFC output state to the file associated with the output state's
output file (i.e., an_ifc_output_state::output_file).
*/
{
  an_ifc_output_metadata
                metadata = make_output_metadata(this->partitions,
                                                this->string_table);

  write_ifc_magic_bytes(this->output_file);
  write_ifc_header(this->output_file, this->source_file_name_offset,
                   this->global_scope, metadata);
  for (const an_ifc_output_partition_metadata &part_metadata :
                                                   metadata.used_partitions) {
    an_ifc_output_partition
                *partition = this->get_partition(part_metadata.kind);

    write_ifc_partition(this->output_file, *partition);
  }  /* for */
  write_string_table(this->output_file, this->string_table);

  an_ifc_byte_offset_storage
                start_of_partitions = get_partitions_start(this->output_file,
                                                           metadata);
  for (const an_ifc_output_partition_metadata &part_metadata :
                                                   metadata.used_partitions) {
    an_ifc_output_partition
                *partition = this->get_partition(part_metadata.kind);

    write_table_of_contents_entry(this->output_file, start_of_partitions,
                                  *partition, part_metadata);
  }  /* for */
  /* FIXME: Assume all is fine for now. */
  return TRUE;
}  /* an_ifc_output_state::write */


template<typename an_ifc_Node_type>
an_ifc_output_partition *an_ifc_output_state::get_or_init_partition()
/*
Return the corresponding output partition for the given node type.  If the
output partition is not already initialized, initialize it now.
*/
{
  an_ifc_partition_kind
                part_kind = get_ifc_partition_kind<an_ifc_Node_type>();
  an_ifc_output_partition
                *output_part = this->get_partition(part_kind);

  if (output_part == NULL) {
    /* The output partition has not yet been created; create it now. */
    size_t       part_size = get_ifc_partition_element_size(this->output_file,
                                                            part_kind);
    /* The partition will need a name for the table of contents write out.
       To simplify things later, create the name now. */
    a_const_char *part_name = get_partition_name_from_kind(part_kind);
    size_t       part_name_len = strlen(part_name) + 1;
    size_t       part_name_offset = this->add_to_string_table(part_name,
                                                              part_name_len);

    output_part = new_general<an_ifc_output_partition>(part_name_offset,
                                                       part_size);
    this->set_partition(part_kind, output_part);
  }  /* if */
  return output_part;
}  /* an_ifc_output_state::get_or_init_partition */


NORETURN static void header_unit_catastrophe(
                     an_error_code reason = ec_unsupported_header_unit_feature)
/*
Issue a catastrophic diagnostic that the header unit could not be created for
the given reason and terminate the compilation.  This routine does not return.
*/
{
  a_diagnostic_ptr diag = start_diagnostic(es_catastrophe,
                                           ec_header_unit_creation_failure);

  add_diag_info(diag, reason);
  end_diagnostic(diag);
  /* Avoid spurious warning.  The function above does not return. */
  exit_compilation(es_internal_error);
}  /* header_unit_catastrophe */


using a_seq_number_key = uint32_t;
                        /* This type is used to key entries of a_seq_number in
                           the an_ifc_il_map::seq_number_map. */

using a_scope_member_array = Dyn_array<an_ifc_decl_index, General_allocator>;
                        /* This type is used to represent an array of scope
                           members for an_ifc_il_map::scope_members. */

/*
A structure used to represent a token cache to be added to the IFC.  This
structure does not itself create any IFC nodes; instead it represents the token
cache as it is constructed.  To form the token cache represented by its current
state it should be passed to an_ifc_output_state::alloc_token_cache.
*/
struct an_ifc_output_token_cache {
#if 0
  /* FIXME: See the definition below. */
  void add_basic(an_ifc_edg_basic_token_sort basic_token);
#endif /* 0 */
  void add_complex(an_ifc_edg_complex_token_index complex_token);

  size_t get_num_basic_tokens() const
    { return this->basic_tokens.length(); }
  size_t get_num_complex_tokens() const
    { return this->complex_tokens.length(); }

  an_ifc_edg_basic_token_sort get_basic_token(size_t i) const
    { return this->basic_tokens[i]; }
  an_ifc_edg_complex_token_index get_complex_token(size_t i) const
    { return this->complex_tokens[i]; }
private:
  Dyn_array<an_ifc_edg_basic_token_sort, General_allocator>
                basic_tokens = {};
                        /* This is the array of EDG IFC basic tokens that will
                           be converted to a block of EDG IFC TokenBasic
                           nodes. */
  Dyn_array<an_ifc_edg_complex_token_index, General_allocator>
                complex_tokens = {};
                        /* This is the array of EDG IFC complex token indexes
                           that will be converted to a block of EDG IFC
                           TokenComplex nodes. */
};  /* an_ifc_output_token_cache */


an_ifc_expr_index an_ifc_output_state::alloc_token_cache(
                                 const an_ifc_output_token_cache &output_cache)
/*
Allocate and populate the token cache with the information tracked by the given
IFC output token cache.  Return the IFC expression index of the created token
cache.
*/
{
  /* Allocate the token cache. */
  an_ifc_edg_token_cache
                token_cache;
  size_t        token_cache_offset = this->alloc_node(&token_cache);
  an_ifc_expr_index
                result(token_cache.get_file(), ifc_es_expr_vendor_extension,
                       token_cache_offset + 1);
  /* Allocate the basic and complex token blocks. */
  size_t        num_basic_tokens = output_cache.get_num_basic_tokens();
  size_t        basic_tokens_start = this->
                                      alloc_node_block<an_ifc_edg_token_basic>(
                                                             num_basic_tokens);
  size_t        num_complex_tokens = output_cache.get_num_complex_tokens();
  size_t        complex_tokens_start = this->
                               alloc_node_block<an_ifc_edg_heap_complex_token>(
                                                           num_complex_tokens);
  /* Associate the basic tokens with the token cache. */
  an_ifc_edg_token_basic_offset
                ifc_basic_tokens_start(token_cache.get_file(),
                                       basic_tokens_start);
  an_ifc_cardinality
                ifc_num_basic_tokens(token_cache.get_file(), num_basic_tokens);
  set_ifc_tokens(&token_cache, ifc_basic_tokens_start);
  set_ifc_num_tokens(&token_cache, ifc_num_basic_tokens);

  /* Associate the complex tokens with the token cache. */
  an_ifc_edg_heap_complex_token_offset
                ifc_complex_tokens_start(token_cache.get_file(),
                                         complex_tokens_start);
  an_ifc_cardinality
                ifc_num_complex_tokens(token_cache.get_file(),
                                       num_complex_tokens);
  set_ifc_complex_tokens(&token_cache, ifc_complex_tokens_start);
  set_ifc_num_complex_tokens(&token_cache, ifc_num_complex_tokens);

  /* Complete the basic tokens. */
  for (size_t i = 0; i < num_basic_tokens; ++i) {
    an_ifc_edg_token_basic basic_token;

    this->fetch_node(&basic_token, basic_tokens_start + i);
    set_ifc_kind(&basic_token, output_cache.get_basic_token(i));
  }  /* for */
  /* Complete the complex tokens. */
  for (size_t i = 0; i < num_complex_tokens; ++i) {
    an_ifc_edg_heap_complex_token complex_token;

    this->fetch_node(&complex_token, complex_tokens_start + i);
    set_ifc_index(&complex_token, output_cache.get_complex_token(i));
  }  /* for */
  return result;
}  /* an_ifc_output_state::alloc_token_cache */


/*
FIXME: This is disabled to suppress build warnings about an unused function
with internal linkage.  Re-enable this function when we're ready to use it.
*/
#if 0

void an_ifc_output_token_cache::add_basic(
                                       an_ifc_edg_basic_token_sort basic_token)
/*
Add the given basic token to the end of the token cache.
*/
{
  /* Complex tokens should be added via add_complex not add_basic to ensure
     the complex token and basic token arrays are properly managed. */
  check_assertion(basic_token != ifc_ebts_complex);
  this->basic_tokens.push_back(basic_token);
}  /* an_ifc_output_token_cache::add_basic */

#endif /* 0 */

void an_ifc_output_token_cache::add_complex(
                                      an_ifc_edg_complex_token_index token_idx)
/*
Add the given complex token to the end of the token cache.
*/
{
  this->basic_tokens.push_back(ifc_ebts_complex);
  this->complex_tokens.push_back(token_idx);
}  /* an_ifc_output_token_cache::add_complex */


/*
This structure maps IL entries to their IFC entries.
*/
struct an_ifc_il_map {
  an_ifc_il_map(an_ifc_output_state *output_state_val)
    : output_state(output_state_val)
    {}
  an_ifc_name_index find_or_enter_src_file(a_source_file_ptr file);
  an_ifc_name_index enter_src_file(a_source_file_ptr file);
  an_ifc_source_location find_or_enter_null_pos();
  an_ifc_source_location find_or_enter_pos(const a_source_position &pos);
  an_ifc_source_location enter_pos(const a_source_position &pos);
  an_ifc_type_index find_or_enter_type(a_type_ptr type);
  an_ifc_type_index enter_type(a_type_ptr type);
  an_ifc_scope_offset find_or_enter_scope(a_scope_ptr scope);
  an_ifc_scope_offset enter_scope(a_scope_ptr scope);
  an_ifc_decl_index find_or_enter_home_scope(a_source_correspondence_ptr scp);
  an_ifc_decl_index enter_home_scope(a_scope_ptr scope);
  an_ifc_decl_index find_or_enter_enum(a_type_ptr type);
  an_ifc_decl_index enter_enum(a_type_ptr type);
  an_ifc_decl_index find_or_enter_routine(a_routine_ptr rp);
  an_ifc_decl_index enter_routine(a_routine_ptr rp);

  size_t get_number_of_scopes() const
    { return this->scope_members.length(); }
  const a_scope_member_array& get_scope_members(size_t scope_idx) const
    { return this->scope_members[scope_idx]; }
private:
  an_ifc_module_file* get_default_file() const
    { return this->output_state->get_file(); }

  template<typename an_ifc_Node_type>
  inline an_ifc_type_index map_new_type(a_type_ptr       type,
                                        an_ifc_Node_type *node);
  template<typename an_ifc_Node_type>
  inline an_ifc_decl_index map_new_home_scope(a_scope_ptr      scope,
                                              an_ifc_Node_type *node);

  an_ifc_type_index enter_enum_type(a_type_ptr type);
  an_ifc_type_index enter_float_type(a_type_ptr type);
  an_ifc_type_index enter_integer_type(a_type_ptr type);
  an_ifc_type_index enter_nullptr_type(a_type_ptr type);
  an_ifc_type_index enter_pointer_type(a_type_ptr type);
  an_ifc_type_index enter_routine_params_type(a_type_ptr type);
  an_ifc_type_index enter_routine_type(a_type_ptr type);
  an_ifc_type_index enter_typedef_type(a_type_ptr type);
  an_ifc_type_index enter_void_type(a_type_ptr type);

  an_ifc_sequence enter_enumerators(a_type_ptr type);
  an_ifc_chart_index enter_routine_params(a_routine_ptr rp);

  an_ifc_edg_constant_index enter_constant(a_constant_ptr cp);

  an_ifc_edg_complex_token_index enter_constant_token(
                                  an_ifc_edg_constant_token_sort token_kind,
                                  an_ifc_edg_constant_index      constant_idx);

  an_ifc_type_index find_or_enter_namespace_scope_type();
  an_ifc_type_index find_or_enter_alias_typedef_type();
  an_ifc_type_index find_or_enter_scoped_enum_type();
  an_ifc_type_index find_or_enter_unscoped_enum_type();

  an_ifc_decl_index enter_namespace(a_scope_ptr scope);
  an_ifc_decl_index enter_typedef(a_type_ptr type);

  void map_scope_member(a_scope_ptr scope, an_ifc_decl_index decl);
  void map_scope_member(a_source_correspondence_ptr scp,
                        an_ifc_decl_index           decl);

  an_ifc_output_state
                *output_state;
                        /* The associated IFC output state. */
  Ptr_map<a_source_file_ptr, an_ifc_name_index, General_allocator>
                src_file_map = {/*mask_width=*/10};
                        /* A map of IL source files to IFC source file
                           names. */
  Ptr_map<a_seq_number_key, an_ifc_line_offset, General_allocator>
                seq_number_map = {/*mask_width=*/10};
                        /* A map of IL sequence numbers to IFC source line
                           offsets. */
  Ptr_map<a_type_ptr, an_ifc_type_index, General_allocator>
                type_map = {/*mask_width=*/10};
                        /* A map of IL types to IFC type indexes. */
  Ptr_map<a_scope_ptr, an_ifc_scope_offset, General_allocator>
                scope_map = {/*mask_width=*/10};
                        /* A map of IL scopes to IFC decl indexes. */
  Ptr_map<a_scope_ptr, an_ifc_decl_index, General_allocator>
                home_scope_map = {/*mask_width=*/10};
                        /* A map of IL scopes to IFC decl indexes. */
  an_ifc_type_index
                fund_namespace_type;
                        /* The fundamental type used to represent a namespace
                           DeclSort::Scope. */
  an_ifc_type_index
                fund_alias_typedef_type;
                        /* The fundamental type used to represent a typedef
                           DeclSort::Alias. */
  an_ifc_type_index
                fund_scoped_enum_type;
                        /* The fundamental type used to represent a scoped
                           DeclSort::Enumeration. */
  an_ifc_type_index
                fund_unscoped_enum_type;
                        /* The fundamental type used to represent an unscoped
                           DeclSort::Enumeration. */
  Dyn_array<a_scope_member_array, General_allocator>
                scope_members;
                        /* Each scope offset maps to an array in scope members
                           containing the members of that scope.  This data is
                           then used to generate the scope membership IFC
                           information after declarations are mapped. */
  Ptr_map<a_type_ptr, an_ifc_decl_index, General_allocator>
                enum_map = {/*mask_width=*/10};
                        /* A map of IL enum types to IFC decl indexes. */
  Ptr_map<a_routine_ptr, an_ifc_decl_index, General_allocator>
                routine_map = {/*mask_width=*/10};
                        /* A map of IL routines to IFC decl indexes. */
};  /* an_ifc_il_map */

}  /* namespace */

namespace {

an_ifc_name_index an_ifc_il_map::find_or_enter_src_file(
                                                       a_source_file_ptr file)
/*
For the given source file find or enter the file into the IFC output state.
Return the name index for the source file.
*/
{
  an_ifc_name_index result = this->src_file_map.get(file);

  if (is_null_index(result)) {
    result = this->enter_src_file(file);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_src_file */


an_ifc_name_index an_ifc_il_map::enter_src_file(a_source_file_ptr file)
/*
For the given source file enter the file into the IFC output state.  Return the
name index for the source file.
*/
{
  an_ifc_name_source_file
                src_file;
  an_ifc_name_index
                result = this->output_state->alloc_name(&src_file);
  size_t        path_offset = this->output_state->add_to_string_table(
                                                             file->file_name);
  an_ifc_text_offset
                ifc_path_offset(src_file.get_file(), path_offset);

  set_ifc_path(&src_file, ifc_path_offset);
  this->src_file_map.map(file, result);
  return result;
}  /* an_ifc_il_map::enter_src_file */

}  /* namespace */

static inline a_seq_number_key
make_seq_num_map_key(const a_source_position &pos)
/*
Given a source position, return the corresponding hash map key.
*/
{
  return a_seq_number_key(pos.seq + 1);
}  /* make_seq_num_map_key */

namespace {

an_ifc_source_location an_ifc_il_map::find_or_enter_null_pos()
/*
Find or enter the NULL source position into the IFC output state.  Return the
corresponding IFC source location.
*/
{
  return this->find_or_enter_pos(null_source_position);
}  /* an_ifc_il_map::find_or_enter_null_pos */


an_ifc_source_location an_ifc_il_map::find_or_enter_pos(
                                                 const a_source_position &pos)
/*
For the given source position find or enter the source position into the IFC
output state.  Return the corresponding IFC source location.
*/
{
  an_ifc_source_location result;
  an_ifc_line_offset     ifc_line_offset = this->seq_number_map.get(
                                                   make_seq_num_map_key(pos));

  if (is_null_index(ifc_line_offset)) {
    result = this->enter_pos(pos);
  } else {
    an_ifc_column ifc_column(ifc_line_offset.get_file(), pos.column);

    result = {ifc_line_offset.get_file()};
    set_ifc_line(&result, ifc_line_offset);
    set_ifc_column(&result, ifc_column);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_pos */


an_ifc_source_location an_ifc_il_map::enter_pos(const a_source_position &pos)
/*
For the given source position enter the source position into the IFC output
state.  Return the corresponding IFC source location.
*/
{
  a_seq_number_key   seq_key = make_seq_num_map_key(pos);

  check_assertion(is_null_index(this->seq_number_map.get(seq_key)));

  an_ifc_line_offset ifc_line_offset;
  if (pos.seq == 0) {
    /* A unknown source line is being referenced. */
    /* Allocate a source line node. */
    an_ifc_source_line
                line;
    size_t      line_offset = this->output_state->alloc_node(&line);
    /* Allocate a source file for the empty source file. */
    an_ifc_name_source_file
                src_file;
    an_ifc_name_index
                src_file_idx = this->output_state->alloc_name(&src_file);

    /* Associate the source file. */
    set_ifc_file(&line, src_file_idx);

    /* Associate an absent line number. */
    an_ifc_line_number
                ifc_line_number(this->get_default_file(), 0);
    set_ifc_line(&line, ifc_line_number);
    /* Use the created line offset. */
    ifc_line_offset = an_ifc_line_offset(line.get_file(), line_offset + 1);
  } else {
    /* A known source line is required. */
    an_ifc_source_line
                line;
    size_t      line_offset = this->output_state->alloc_node(&line);
      /* Gather the information for the source sequence. */
    a_line_number
                line_number;
    a_boolean   at_end_of_source;
    /* physical_line == FALSE means consider information from #line directives
       as well as true file information. */
    a_source_file_ptr
                file = source_file_for_seq(pos.seq, &line_number,
                                           &at_end_of_source,
                                           /*physical_line=*/FALSE);
    an_ifc_name_index
                file_name_idx = this->find_or_enter_src_file(file);
    an_ifc_line_number
                ifc_line_number(line.get_file(), line_number);

    set_ifc_file(&line, file_name_idx);
    set_ifc_line(&line, ifc_line_number);
    /* Use the created line offset. */
    ifc_line_offset = an_ifc_line_offset(line.get_file(), line_offset + 1);
  }  /* if */
  this->seq_number_map.map(seq_key, ifc_line_offset);

  /* Form a result IFC SourceLocation. */
  an_ifc_source_location result(ifc_line_offset.get_file());
  an_ifc_column          ifc_column(result.get_file(), pos.column);
  set_ifc_line(&result, ifc_line_offset);
  set_ifc_column(&result, ifc_column);
  return result;
}  /* an_ifc_il_map::enter_pos */


an_ifc_type_index an_ifc_il_map::find_or_enter_type(a_type_ptr type)
/*
For the given type find or enter the type into the IFC output state.  Return
the type index for the type file.
*/
{
  an_ifc_type_index result = this->type_map.get(type);

  if (is_null_index(result)) {
    result = this->enter_type(type);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_type */


an_ifc_type_index an_ifc_il_map::enter_type(a_type_ptr type)
/*
For the given type enter the type into the IFC output state.  Return the type
index for the type file.
*/
{
  check_assertion(is_null_index(this->type_map.get(type)));
  an_ifc_type_index result;

  switch (type->kind) {
    case tk_float:
      result = this->enter_float_type(type);
      break;
    case tk_integer:
      if (is_enum_type(type)) {
        result = this->enter_enum_type(type);
      } else {
        result = this->enter_integer_type(type);
      }  /* if */
      break;
    case tk_nullptr:
      result = this->enter_nullptr_type(type);
      break;
    case tk_pointer:
      result = this->enter_pointer_type(type);
      break;
    case tk_routine:
      result = this->enter_routine_type(type);
      break;
    case tk_typeref:
      if (typeref_is_typedef(type)) {
        result = this->enter_typedef_type(type);
      } else if (is_typeref_kind(type, trk_is_decltype)) {
        /* FIXME: This is almost definitely incomplete (e.g., dependent cases).
           The IFC has a decltype type node for representing decltype
           expressions (presumably we only need this in dependent cases?). */
        a_type_ptr underlying_type = type->variant.typeref.type;

        result = this->enter_type(underlying_type);
      } else {
        /* FIXME: Handle other kinds of typerefs. */
        header_unit_catastrophe();
      }  /* if */
      break;
    case tk_void:
      result = this->enter_void_type(type);
      break;
    case tk_struct:
    case tk_union:
    case tk_error:
#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
#endif /* FIXED_POINT_ALLOWED */
#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_imaginary:
    case tk_complex:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case tk_array:
    case tk_class:
    case tk_ptr_to_member:
    case tk_template_param:
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
    case tk_scalable_vector:
    case tk_scalable_vector_count:
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    case tk_reflection:
    case tk_unknown:
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* an_ifc_il_map::enter_type */


an_ifc_scope_offset an_ifc_il_map::find_or_enter_scope(a_scope_ptr scope)
/*
For the given scope enter the scope into the IFC output state.  Return the
scope offset for the type file.
*/
{
  an_ifc_scope_offset result = this->scope_map.get(scope);

  if (is_null_index(result)) {
    result = this->enter_scope(scope);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_scope */


an_ifc_scope_offset an_ifc_il_map::enter_scope(a_scope_ptr scope)
/*
For the given scope enter the scope into the IFC output state.  Return the
scope offset for the type file.
*/
{
  check_assertion(is_null_index(this->scope_map.get(scope)));
  an_ifc_scope_descriptor
                descriptor;
  size_t        part_offset = this->output_state->alloc_node(&descriptor);
  an_ifc_scope_offset
                result(descriptor.get_file(), part_offset + 1);

  this->scope_map.map(scope, result);
  this->scope_members.push_back(a_scope_member_array());
  /* If this assertion fails a scope member array is missing for one or more
     IFC scope descriptors. */
  check_assertion((size_t)this->scope_members.length() == (part_offset + 1));
  return result;
}  /* an_ifc_il_map::enter_scope */


an_ifc_decl_index an_ifc_il_map::find_or_enter_home_scope(
                                              a_source_correspondence_ptr scp)
/*
For the given source correspondence find or enter the associated parent scope
declaration into the IFC output state.  Return the declaration index for the
scope.
*/
{
  a_scope_ptr       scope = scp->parent_scope;
  (void)this->find_or_enter_scope(scope);
  an_ifc_decl_index result = this->home_scope_map.get(scope);

  if (is_null_index(result)) {
    result = this->enter_home_scope(scope);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_home_scope */


an_ifc_decl_index an_ifc_il_map::enter_home_scope(a_scope_ptr scope)
/*
For the given scope enter the associated parent scope declaration into the IFC
output state.  Return the declaration index for the scope.
*/
{
  check_assertion(is_null_index(this->home_scope_map.get(scope)));
  an_ifc_decl_index result;

  switch (scope->kind) {
    case sck_file:
      /* Use a null index to indicate the primary scope. */
      /* Additionally ensure this scope is entered in the scope table. */
      (void)this->find_or_enter_scope(scope);
      break;
    case sck_namespace:
    case sck_namespace_extension:
    case sck_namespace_reactivation:
      result = this->enter_namespace(scope);
      break;
    case sck_func_prototype:
    case sck_block:
    case sck_class_struct_union:
    case sck_class_reactivation:
    case sck_template_declaration:
    case sck_template_instantiation:
    case sck_instantiation_context:
    case sck_module_decl_import:
    case sck_module_isolated:
    case sck_pragma:
    case sck_function_access:
    case sck_condition:
    case sck_enum:
    case sck_function:
    case sck_none:
      break;
    default_is_unexpected();
  }  /* switch */
  return result;
}  /* an_ifc_il_map::enter_home_scope */


an_ifc_decl_index an_ifc_il_map::find_or_enter_enum(a_type_ptr type)
/*
For the given enumeration type find or enter the enumeration into the IFC
output state.  Return the declaration index for the enumeration.
*/
{
  an_ifc_decl_index result = this->enum_map.get(type);

  if (is_null_index(result)) {
    result = this->enter_enum(type);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_enum */

}  /* namespace */

static size_t add_name_to_string_table(an_ifc_output_state     *output_state,
                                       a_source_correspondence *scp)
/*
Add the name for the given source correspondence entry to the string table of
the given output state.  Return the offset into the string table where the name
has been placed.
*/
{
  size_t result = 0;

  if (scp->name != NULL) {
    result = output_state->add_to_string_table(scp->name);
  }  /* if */
  return result;
}  /* add_name_to_string_table */


template<typename a_Type>
static size_t add_name_to_string_table(an_ifc_output_state *output_state,
                                       a_Type              *il_entity)
/*
Add the name for the given IL entity to the string table of the given output
state.  Return the offset into the string table where the name has been placed.
*/
{
  return add_name_to_string_table(output_state, &il_entity->source_corresp);
}  /* add_name_to_string_table */

namespace {

an_ifc_decl_index an_ifc_il_map::enter_enum(a_type_ptr type)
/*
For the given enumeration type enter the enumeration into the IFC output state.
Return the declaration index for the enumeration.
*/
{
  check_assertion(is_null_index(this->enum_map.get(type)) &&
                  type->kind == tk_enum && type->variant.integer.enum_type);
  an_ifc_decl_enumeration
                enum_decl;
  an_ifc_decl_index
                result = this->output_state->alloc_decl(&enum_decl);
  /* Set the name information. */
  size_t        name_offset = add_name_to_string_table(this->output_state,
                                                       type);
  an_ifc_text_offset
                ifc_name_offset(enum_decl.get_file(), name_offset);

  set_ifc_name(&enum_decl, ifc_name_offset);

  /* Set the source location information. */
  a_source_position
                src_pos = type->source_corresp.decl_position;
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_pos(src_pos);
  set_ifc_locus(&enum_decl, ifc_src_pos);

  /* Set the type information. */
  an_ifc_type_index
                type_idx;
  if (type->variant.integer.is_scoped_enum) {
    type_idx = this->find_or_enter_scoped_enum_type();
  } else {
    type_idx = this->find_or_enter_unscoped_enum_type();
  }  /* if */
  set_ifc_type(&enum_decl, type_idx);

  /* Set the base type. */
  an_integer_kind
                base_int_kind = type->variant.integer.int_kind;
  a_type_ptr    base_type = integer_type(base_int_kind);
  an_ifc_type_index
                base_type_idx = this->find_or_enter_type(base_type);
  set_ifc_base(&enum_decl, base_type_idx);

  /* Construct and set the initializer to provide the enumerators. */
  an_ifc_sequence seq = this->enter_enumerators(type);
  set_ifc_initializer(&enum_decl, seq);

  /* Set the scope information. */
  a_source_correspondence_ptr
                scp = &type->source_corresp;
  an_ifc_decl_index
                scope_decl_idx = this->find_or_enter_home_scope(scp);
  this->map_scope_member(scp, result);
  set_ifc_home_scope(&enum_decl, scope_decl_idx);
  /* FIXME: Set alignment. */
  /* FIXME: Set specifiers. */
  /* FIXME: Set access. */
  /* FIXME: Set properties. */
  return result;
}  /* an_ifc_il_map::enter_enum */


an_ifc_decl_index an_ifc_il_map::find_or_enter_routine(a_routine_ptr rp)
/*
For the given routine find or enter the function declaration into the IFC
output state.  Return the declaration index for the routine.
*/
{
  an_ifc_decl_index result = this->routine_map.get(rp);

  if (is_null_index(result)) {
    result = this->enter_routine(rp);
  }  /* if */
  return result;
}  /* an_ifc_il_map::find_or_enter_routine */


an_ifc_decl_index an_ifc_il_map::enter_routine(a_routine_ptr rp)
/*
For the given routine enter the function declaration into the IFC output state.
Return the declaration index for the routine.
*/
{
  check_assertion(is_null_index(this->routine_map.get(rp)));
  an_ifc_decl_function
                func_decl;
  an_ifc_decl_index
                result = this->output_state->alloc_decl(&func_decl);

  this->routine_map.map(rp, result);

  /* Set the name information. */
  size_t        name_offset = add_name_to_string_table(this->output_state, rp);
  an_ifc_name_index
                ifc_name_offset(func_decl.get_file(), ifc_ns_text_offset,
                                name_offset);
  set_ifc_name(&func_decl, ifc_name_offset);

  /* Set the source location information. */
  a_source_position
                src_pos = rp->source_corresp.decl_position;
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_pos(src_pos);
  set_ifc_locus(&func_decl, ifc_src_pos);

  /* Set the type information. */
  a_type_ptr    type = rp->type;
  an_ifc_type_index
                type_idx = this->find_or_enter_type(type);
  set_ifc_type(&func_decl, type_idx);

  /* Set the scope information. */
  a_source_correspondence_ptr
                scp = &rp->source_corresp;
  an_ifc_decl_index
                scope_decl_idx = this->find_or_enter_home_scope(scp);
  this->map_scope_member(scp, result);
  set_ifc_home_scope(&func_decl, scope_decl_idx);

  /* Set the parameter information. */
  an_ifc_chart_index
                param_chart_idx = this->enter_routine_params(rp);
  set_ifc_chart(&func_decl, param_chart_idx);
  return result;
}  /* an_ifc_il_map::enter_routine */


template<typename an_ifc_Node_type>
an_ifc_type_index an_ifc_il_map::map_new_type(a_type_ptr       type,
                                              an_ifc_Node_type *node)
/*
For the given IL type, construct a corresponding IFC type node in the IFC
output state.  Return the type index for the constructed type node.
*/
{
  an_ifc_type_index result = this->output_state->alloc_type(node);

  this->type_map.map(type, result);
  return result;
}  /* an_ifc_il_map::map_new_type */


template<typename an_ifc_Node_type>
an_ifc_decl_index an_ifc_il_map::map_new_home_scope(a_scope_ptr      scope,
                                                    an_ifc_Node_type *node)
/*
For the given IL scope, construct a corresponding IFC declaration node in the
IFC output state.  Return the declaration index for the constructed declaration
node.
*/
{
  an_ifc_decl_index result = this->output_state->alloc_decl(node);

  this->home_scope_map.map(scope, result);
  return result;
}  /* an_ifc_il_map::map_new_home_scope */


an_ifc_type_index an_ifc_il_map::enter_enum_type(a_type_ptr type)
/*
Enter the given enum type into the IFC output state.  Return the type index for
the enum type.
*/
{
  check_assertion(type->kind == tk_integer);
  an_ifc_type_designated
                designated_type;
  an_ifc_type_index
                result = this->map_new_type(type, &designated_type);
  an_ifc_decl_index
                decl_idx = this->find_or_enter_enum(type);

  set_ifc_decl(&designated_type, decl_idx);
  return result;
}  /* an_ifc_il_map::enter_enum_type */


an_ifc_type_index an_ifc_il_map::enter_float_type(a_type_ptr type)
/*
Enter the given float type into the IFC output state.  Return the type index
for the float type.
*/
{
  check_assertion(type->kind == tk_float);
  an_ifc_type_fundamental
                fund_type;
  an_ifc_type_index
                result = this->map_new_type(type, &fund_type);

  switch (type->variant.float_kind) {
    case fk_float:
      set_ifc_basis(&fund_type, ifc_tbs_float);
      set_ifc_precision(&fund_type, ifc_tps_default);
      break;
    case fk_double:
      set_ifc_basis(&fund_type, ifc_tbs_double);
      set_ifc_precision(&fund_type, ifc_tps_default);
      break;
    case fk_long_double:
      set_ifc_basis(&fund_type, ifc_tbs_double);
      set_ifc_precision(&fund_type, ifc_tps_long);
      break;
    case fk_float128:
      set_ifc_basis(&fund_type, ifc_tbs_float);
      set_ifc_precision(&fund_type, ifc_tps_bit128);
      break;
    case fk_float16:
    case fk_fp16:
    case fk_float32x:
    case fk_float64x:
    case fk_float80:
    case fk_std_bfloat16:
    case fk_std_float16:
    case fk_std_float32:
    case fk_std_float64:
    case fk_std_float128:
    case fk_last:
      header_unit_catastrophe();
      break;
    default_is_unexpected();
  }  /* switch */
  set_ifc_sign(&fund_type, ifc_tss_signed);
  return result;
}  /* an_ifc_il_map::enter_float_type */

}  /* namespace */

static an_ifc_type_precision_sort type_size_to_precision(a_type_ptr type)
/*
Given a type, return the corresponding precision.
*/
{
  an_ifc_type_precision_sort result;

  switch (size_of_type(type)) {
    case 1:
      result = ifc_tps_bit8;
      break;
    case 2:
      result = ifc_tps_bit16;
      break;
    case 4:
      result = ifc_tps_bit32;
      break;
    case 8:
      result = ifc_tps_bit64;
      break;
    case 16:
      result = ifc_tps_bit128;
      break;
    default:
      /* The conversion from type size to type precision is unknown. */
      unexpected_condition();
  }  /* switch */
  return result;
}  /* type_size_to_precision */

namespace {

an_ifc_type_index an_ifc_il_map::enter_integer_type(a_type_ptr type)
/*
Enter the given integer type into the IFC output state.  Return the type index
for the integer type.
*/
{
  check_assertion(type->kind == tk_integer);
  an_ifc_type_fundamental
                fund_type;
  an_ifc_type_index
                result = this->map_new_type(type, &fund_type);

  /* Set the basis, precision, and sign. */
  if (type->variant.integer.bool_type) {
    /* Handle the bool type. */
    set_ifc_basis(&fund_type, ifc_tbs_bool);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  } else if (type->variant.integer.wchar_t_type) {
    /* Handle the wchar_t type. */
    set_ifc_basis(&fund_type, ifc_tbs_wchar_t);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  } else if (type->variant.integer.char8_t_type) {
    /* Handle the char8_t type. */
    set_ifc_basis(&fund_type, ifc_tbs_char);
    set_ifc_precision(&fund_type, ifc_tps_bit8);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  } else if (type->variant.integer.char16_t_type) {
    /* Handle the char16_t type. */
    set_ifc_basis(&fund_type, ifc_tbs_char);
    set_ifc_precision(&fund_type, ifc_tps_bit16);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  } else if (type->variant.integer.char32_t_type) {
    /* Handle the char32_t type. */
    set_ifc_basis(&fund_type, ifc_tbs_char);
    set_ifc_precision(&fund_type, ifc_tps_bit32);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  } else {
    switch (type->variant.integer.int_kind) {
      case ik_char:
        /* Handle the char type. */
        set_ifc_basis(&fund_type, ifc_tbs_char);
        set_ifc_precision(&fund_type, ifc_tps_default);
        set_ifc_sign(&fund_type, ifc_tss_plain);
        break;
      case ik_signed_char:
        /* Handle the signed char type. */
        set_ifc_basis(&fund_type, ifc_tbs_char);
        set_ifc_precision(&fund_type, ifc_tps_default);
        set_ifc_sign(&fund_type, ifc_tss_signed);
        break;
      case ik_unsigned_char:
        /* Handle the unsigned char type. */
        set_ifc_basis(&fund_type, ifc_tbs_char);
        set_ifc_precision(&fund_type, ifc_tps_default);
        set_ifc_sign(&fund_type, ifc_tss_unsigned);
        break;
      case ik_short:
        /* Handle the (signed) short type. */
        set_ifc_basis(&fund_type, ifc_tbs_int);
        set_ifc_precision(&fund_type, ifc_tps_short);
        set_ifc_sign(&fund_type, ifc_tss_signed);
        break;
      case ik_unsigned_short:
        /* Handle the unsigned short type. */
        set_ifc_basis(&fund_type, ifc_tbs_int);
        set_ifc_precision(&fund_type, ifc_tps_short);
        set_ifc_sign(&fund_type, ifc_tss_unsigned);
        break;
      case ik_int:
        /* Handle the (signed) int type. */
        set_ifc_basis(&fund_type, ifc_tbs_int);
        set_ifc_precision(&fund_type, ifc_tps_default);
        set_ifc_sign(&fund_type, ifc_tss_signed);
        break;
      case ik_unsigned_int:
        /* Handle the unsigned int type. */
        set_ifc_basis(&fund_type, ifc_tbs_int);
        set_ifc_precision(&fund_type, ifc_tps_default);
        set_ifc_sign(&fund_type, ifc_tss_unsigned);
        break;
      case ik_long:
        /* Handle the (signed) long type. */
        set_ifc_basis(&fund_type, ifc_tbs_int);
        set_ifc_precision(&fund_type, ifc_tps_long);
        set_ifc_sign(&fund_type, ifc_tss_signed);
        break;
      case ik_unsigned_long:
        /* Handle the unsigned long type. */
        set_ifc_basis(&fund_type, ifc_tbs_int);
        set_ifc_precision(&fund_type, ifc_tps_long);
        set_ifc_sign(&fund_type, ifc_tss_unsigned);
        break;
      default:
        /* Handle miscellaneous fixed size integer types (e.g., uint32_t,
           int32_t, etc). */
        { an_ifc_type_precision_sort
                  precision_sort = type_size_to_precision(type);
          an_ifc_type_sign_sort
                  sign_sort = is_signed_integral_type(type) ? ifc_tss_signed
                                                            : ifc_tss_unsigned;
          set_ifc_basis(&fund_type, ifc_tbs_int);
          set_ifc_precision(&fund_type, precision_sort);
          set_ifc_sign(&fund_type, sign_sort);
        }
        break;
    }  /* switch */
  }  /* if */
  return result;
}  /* an_ifc_il_map::enter_integer_type */


an_ifc_type_index an_ifc_il_map::enter_nullptr_type(a_type_ptr type)
/*
Enter the given nullptr type into the IFC output state.  Return the type index
for the nullptr type.
*/
{
  check_assertion(type->kind == tk_nullptr);
  an_ifc_type_fundamental
                fund_type;
  an_ifc_type_index
                result = this->map_new_type(type, &fund_type);

  set_ifc_basis(&fund_type, ifc_tbs_nullptr);
  set_ifc_precision(&fund_type, ifc_tps_default);
  set_ifc_sign(&fund_type, ifc_tss_plain);
  return result;
}  /* an_ifc_il_map::enter_nullptr_type */


an_ifc_type_index an_ifc_il_map::enter_pointer_type(a_type_ptr type)
/*
Enter the given pointer type into the IFC output state.  Return the type index
for the pointer type.
*/
{
  check_assertion(type->kind == tk_pointer);
  an_ifc_type_pointer
                ptr_type;
  an_ifc_type_index
                result = this->map_new_type(type, &ptr_type);
  a_type_ptr    pointee_type = type->variant.pointer.type;
  an_ifc_type_index
                ifc_pointee_type = this->find_or_enter_type(pointee_type);

  set_ifc_pointee(&ptr_type, ifc_pointee_type);
  return result;
}  /* an_ifc_il_map::enter_pointer_type */


an_ifc_type_index an_ifc_il_map::enter_routine_params_type(a_type_ptr type)
/*
Given a routine type, find or enter the types for the routine's parameters.
Return the IFC type index of the parameter types.
*/
{
  check_assertion(type->kind == tk_routine);
  an_ifc_type_index
                result;
  Small_dyn_array<an_ifc_type_index, 10, General_allocator>
                param_types;
  a_param_type_ptr
                curr_param_type = function_type_params(type);

  /* Convert each parameter type and collect the converted types. */
  while (curr_param_type != NULL) {
    an_ifc_type_index ifc_param_type =
                              this->find_or_enter_type(curr_param_type->type);

    param_types.push_back(ifc_param_type);
    curr_param_type = curr_param_type->next;
  }  /* while */

  if (param_types.length() == 0) {
    /* Do nothing, the result type is a null index as there are no
       parameters. */
  } else if (param_types.length() == 1) {
    /* There is only one parameter type, use it directly. */
    result = param_types[0];
  } else {
    /* Add the parameter types to the type heap and track the type heap offset
       of the first type added to the heap. */
    a_boolean first = TRUE;
    size_t    heap_start_offset = 0;

    for (const an_ifc_type_index &param_type : param_types) {
      an_ifc_heap_type
                heap_type;
      size_t    heap_part_offset = this->output_state->alloc_node(&heap_type);

      if (first) {
        /* Update the heap start offset. */
        first = FALSE;
        heap_start_offset = heap_part_offset;
      }  /* if */
      set_ifc_value(&heap_type, param_type);
    }  /* for */

    /* Create and return a type tuple type pointing to all the parameters for
       this type. */
    an_ifc_type_tuple
                param_tuple_type;
    result = this->output_state->alloc_type(&param_tuple_type);

    an_ifc_index
                ifc_type_heap_start(param_tuple_type.get_file(),
                                    heap_start_offset);
    set_ifc_start(&param_tuple_type, ifc_type_heap_start);

    an_ifc_cardinality
                ifc_param_type_count(param_tuple_type.get_file(),
                                     param_types.length());
    set_ifc_cardinality(&param_tuple_type, ifc_param_type_count);
  }  /* if */
  return result;
}  /* an_ifc_il_map::enter_routine_params_type */


an_ifc_type_index an_ifc_il_map::enter_routine_type(a_type_ptr type)
/*
Enter the given routine type into the IFC output state.  Return the type index
for the function type.
*/
{
  check_assertion(type->kind == tk_routine);
  an_ifc_type_function
                func_type;
  an_ifc_type_index
                result = this->map_new_type(type, &func_type);
  a_type_ptr    return_type = type->variant.routine.return_type;
  an_ifc_type_index
                ifc_return_type = this->find_or_enter_type(return_type);

  set_ifc_target(&func_type, ifc_return_type);

  an_ifc_type_index
                ifc_param_types = this->enter_routine_params_type(type);
  set_ifc_source(&func_type, ifc_param_types);
  return result;
}  /* an_ifc_il_map::enter_routine_type */


an_ifc_type_index an_ifc_il_map::enter_typedef_type(a_type_ptr type)
/*
Enter the given typedef type into the IFC output state.  Return the type index
for the typedef type.
*/
{
  check_assertion(type->kind == tk_typeref && typeref_is_typedef(type));
  an_ifc_type_designated
                designated_type;
  an_ifc_type_index
                result = this->map_new_type(type, &designated_type);
  an_ifc_decl_index
                typedef_decl = this->enter_typedef(type);

  set_ifc_decl(&designated_type, typedef_decl);
  return result;
}  /* an_ifc_il_map::enter_typedef_type */


an_ifc_type_index an_ifc_il_map::enter_void_type(a_type_ptr type)
/*
Enter the given void type into the IFC output state.  Return the type index
for the void type.
*/
{
  an_ifc_type_fundamental
                fund_type;
  an_ifc_type_index
                result = this->map_new_type(type, &fund_type);

  set_ifc_basis(&fund_type, ifc_tbs_void);
  return result;
}  /* an_ifc_il_map::enter_void_type */


static a_constant_ptr enumerator_constants_for_type(a_type_ptr type)
/*
Return the list of constants representing the enumerators of the given
enumeration type.
*/
{
  check_assertion(type->kind == tk_enum && type->variant.integer.enum_type);
  a_constant_ptr result = NULL;

  if (type->variant.integer.is_scoped_enum) {
    /* Scoped enumerators use a scope with a constant list rather than
       storing the constants directly on the integer type.  Check for
       said scope and then read the constants from it if it exists. */
    a_scope_ptr scope = type->variant.integer.enum_info.assoc_scope;

    if (scope != NULL) {
      result = scope->constants;
    }  /* if */
  } else {
    result = type->variant.integer.enum_info.constant_list;
  }  /* if */
  return result;
}  /* enumerator_constants_for_type */


an_ifc_sequence an_ifc_il_map::enter_enumerators(a_type_ptr type)
/*
Given a enumeration type, enter the type's enumerators.  Return the IFC
sequence representing the IFC DeclSort::Enumerators.
*/
{
  check_assertion(type->kind == tk_enum && type->variant.integer.enum_type);
  an_ifc_sequence result(this->get_default_file());
  a_constant_ptr  constants = enumerator_constants_for_type(type);
  size_t          num_constants = count_list_elements(constants);

  if (num_constants > 0) {
    /* Preallocate all the enumerators to ensure we get one contiguous
       block. */
    size_t      start = this->output_state->
                       alloc_node_block<an_ifc_decl_enumerator>(num_constants);
    /* Associate the preallocated enumerator nodes with the sequence. */
    an_ifc_index
                ifc_start(this->get_default_file(), start);
    an_ifc_cardinality
                ifc_cardinality(this->get_default_file(), num_constants);

    set_ifc_start(&result, ifc_start);
    set_ifc_cardinality(&result, ifc_cardinality);

    /* Find or enter the enumeration type. */
    an_ifc_type_index
                enumeration_type = this->find_or_enter_type(type);
    /* Complete the enumerator declarations. */
    a_constant_ptr curr_constant = constants;
    for (size_t i = 0; i < num_constants; ++i) {
      size_t                 curr_enumerator_offset = start + i;
      an_ifc_decl_enumerator curr_enumerator;

      this->output_state->fetch_node(&curr_enumerator,
                                     curr_enumerator_offset);

      /* Set the enumerator name. */
      size_t    name_offset = add_name_to_string_table(this->output_state,
                                                     curr_constant);
      an_ifc_text_offset
                ifc_name_offset(curr_enumerator.get_file(), name_offset);
      set_ifc_name(&curr_enumerator, ifc_name_offset);

      /* Set the source location information. */
      a_source_position
                src_pos = curr_constant->source_corresp.decl_position;
      an_ifc_source_location
                ifc_src_pos = this->find_or_enter_pos(src_pos);
      set_ifc_locus(&curr_enumerator, ifc_src_pos);
      /* Associate the enumeration type with the enumerator. */
      set_ifc_type(&curr_enumerator, enumeration_type);

      /* Create a token cache representation of the initializing constant. */
      an_ifc_output_token_cache
                init_token_cache;
      /* FIXME: We probably eventually want to have a find_or_enter_constant
         to hash and deduplicate constants. */
      an_ifc_edg_constant_index
                ifc_constant = this->enter_constant(curr_constant);
      an_ifc_edg_complex_token_index
                ifc_constant_token = this->enter_constant_token(
                                                         ifc_ects_int_constant,
                                                         ifc_constant);
      init_token_cache.add_complex(ifc_constant_token);

      /* Set the initializing expression. */
      an_ifc_expr_index init_idx = this->output_state->alloc_token_cache(
                                                             init_token_cache);
      set_ifc_initializer(&curr_enumerator, init_idx);
      /* FIXME: Set specifier. */
      /* FIXME: Set access. */
      /* Advance to the next parameter. */
      curr_constant = curr_constant->next;
    }  /* for */
  }  /* if */
  return result;
}  /* an_ifc_il_map::enter_enumerators */


an_ifc_chart_index an_ifc_il_map::enter_routine_params(a_routine_ptr rp)
/*
Given a routine, enter the routine's parameters.  Return the IFC chart index of
the parameters.
*/
{
  an_ifc_chart_index
                result;
  a_type_ptr    type = rp->type;
  a_param_type_ptr
                param_type_list = function_type_params(type);
  unsigned      param_count = count_list_elements(param_type_list);

  if (param_count > 0) {
    an_ifc_chart_unilevel
                param_chart;

    result = this->output_state->alloc_chart(&param_chart);

    /* Preallocate all the parameters to ensure we get one contiguous block. */
    size_t      start = this->output_state->
                          alloc_node_block<an_ifc_decl_parameter>(param_count);
    /* Associate the preallocated parameter nodes with the parameter chart. */
    an_ifc_module_file
                *file = this->get_default_file();
    an_ifc_index
                ifc_param_start(file, start);
    an_ifc_cardinality
                ifc_param_count(file, param_count);
    set_ifc_start(&param_chart, ifc_param_start);
    set_ifc_cardinality(&param_chart, ifc_param_count);

    /* Complete the parameter declarations. */
    a_param_type_ptr curr_param_type = param_type_list;
    for (size_t i = 0; i < param_count; ++i) {
      size_t                curr_param_offset = start + i;
      an_ifc_decl_parameter curr_param;

      this->output_state->fetch_node(&curr_param, curr_param_offset);

      /* Set the parameter name. */
      a_const_char
                *name = curr_param_type->name;
      if (name != NULL) {
        size_t  name_offset = this->output_state->add_to_string_table(name);
        an_ifc_text_offset
                ifc_name_offset(curr_param.get_file(), name_offset);

        set_ifc_name(&curr_param, ifc_name_offset);
      } else {
        an_ifc_text_offset
                ifc_name_offset;

        set_ifc_name(&curr_param, ifc_name_offset);
      }  /* if */

      /* Set the source location information. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      a_decl_position_supplement_ptr
                decl_pos_sup = curr_param_type->decl_pos_info;
      if (decl_pos_sup != NULL) {
        a_source_position
                src_pos = decl_pos_sup->identifier_range.start;
        an_ifc_source_location
                ifc_src_pos = this->find_or_enter_pos(src_pos);

        set_ifc_locus(&curr_param, ifc_src_pos);
      } else
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      {
        an_ifc_source_location
                ifc_src_pos = this->find_or_enter_null_pos();

        set_ifc_locus(&curr_param, ifc_src_pos);
      }  /* if */

      /* Set the type information. */
      an_ifc_type_index
                ifc_type_idx = this->find_or_enter_type(curr_param_type->type);
      set_ifc_type(&curr_param, ifc_type_idx);
      /* FIXME: Set the constraint. */
      /* FIXME: Set the initializer. */
      /* FIXME: Set the level. */
      /* FIXME: Set the position. */
      set_ifc_sort(&curr_param, ifc_ps_object);
      /* FIXME: Set the reachable properties. */
      /* Advance to the next parameter. */
      curr_param_type = curr_param_type->next;
    }  /* for */
  }  /* if */
  return result;
}  /* an_ifc_il_map::enter_routine_params */


an_ifc_edg_constant_index an_ifc_il_map::enter_constant(a_constant_ptr cp)
/*
For the given constant enter the constant into the IFC output state.  Return
the expr index for the constant.
*/
{
  an_ifc_edg_constant_index
                result;

  switch (cp->kind) {
    case ck_error:
    case ck_last:
      header_unit_catastrophe();
      break;
    case ck_integer:
      { const an_integer_value
                &int_val = cp->variant.integer_value;
        Integer_translator<uint32_t, 2>
                output_int;

        /* Figure out and create the necessary number of EDG IFC integer
           constant words. */
#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
        output_int.add_part(int_val);
#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
        for (size_t i = 0; i < INT_VALUE_PARTS_PER_INTEGER_VALUE; ++i) {
          output_int.add_part(int_val.part[i]);
        }  /* for */
#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

        /* Allocate the integer constant word block and populate it with
           the computed values. */
        size_t  num_parts_required = output_int.parts.length();
        size_t  start = this->output_state->
                            alloc_node_block<an_ifc_edg_constant_integer_word>(
                                                           num_parts_required);
        for (size_t i = 0; i < num_parts_required; ++i) {
          an_ifc_edg_constant_integer_word int_word;

          this->output_state->fetch_node(&int_word, start + i);

          an_ifc_edg_constant_word_storage word_bytes;
          memcpy(word_bytes, (void*)(&output_int.parts[i]), /*num_bytes=*/4);

          an_ifc_edg_constant_word word_contents(int_word.get_file(),
                                                 word_bytes);
          set_ifc_bytes(&int_word, word_contents);
        }  /* for */

        /* Create the integer constant itself. */
        an_ifc_edg_constant_integer
                int_constant;
        result = this->output_state->alloc_constant(&int_constant);

        /* Associate the constant's type. */
        a_type_ptr
                constant_type = cp->type;
        if (is_enum_type(constant_type)) {
          /* The IFC reading code expects an integer constant with a
             fundamental integral type.  The constant's enumeration type will
             be restored automatically as part of the reconstruction
             process. */
          an_integer_kind
                underlying_int_kind = constant_type->variant.integer.int_kind;

          constant_type = integer_type(underlying_int_kind);
        }  /* if */

        an_ifc_type_index
                ifc_constant_type = this->find_or_enter_type(constant_type);
        set_ifc_type(&int_constant, ifc_constant_type);

        /* Reference the created words. */
        an_ifc_edg_constant_integer_word_offset
                ifc_start(int_constant.get_file(), start);
        an_ifc_cardinality
                ifc_cardinality(int_constant.get_file(), num_parts_required);
        set_ifc_start(&int_constant, ifc_start);
        set_ifc_cardinality(&int_constant, ifc_cardinality);
      }
      break;
#if FIXED_POINT_ALLOWED
    case ck_fixed_point:
#endif /* FIXED_POINT_ALLOWED */
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:
#endif /* DO_IL_LOWERING && ... */
    case ck_string:
    case ck_float:
#if C99_IL_EXTENSIONS_SUPPORTED
    case ck_complex:
    case ck_imaginary:
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    case ck_address:
    case ck_ptr_to_member:
#if GNU_EXTENSIONS_ALLOWED
    case ck_label_difference:
#endif /* GNU_EXTENSIONS_ALLOWED */
    case ck_dynamic_init:
    case ck_aggregate:
    case ck_init_repeat:
    case ck_template_param:
    case ck_designator:
#if UPC_EXTENSIONS_ALLOWED
    case ck_upc_threads:
    case ck_upc_mythread:
#endif /* UPC_EXTENSIONS_ALLOWED */
    case ck_void:
    case ck_reflection:
      header_unit_catastrophe();
      break;
    default_is_unexpected();
  } /* switch */
  return result;
}  /* an_ifc_il_map::enter_constant */


an_ifc_edg_complex_token_index an_ifc_il_map::enter_constant_token(
                                   an_ifc_edg_constant_token_sort token_kind,
                                   an_ifc_edg_constant_index      constant_idx)

/*
For the given constant and constant token kind, enter a complex constant token
into the IFC output state.  Return the index of the token.
*/
{
  an_ifc_edg_token_constant
                constant_token;
  an_ifc_edg_complex_token_index
                result = this->output_state->alloc_complex_token(
                                                              &constant_token);

  set_ifc_kind(&constant_token, token_kind);
  set_ifc_constant(&constant_token, constant_idx);
  return result;
}  /* an_ifc_il_map::enter_constant_token */


an_ifc_type_index an_ifc_il_map::find_or_enter_namespace_scope_type()
/*
Find or enter the fundamental type used by the IFC to indicate a given IFC
DeclSort::Scope is a namespace.  Return the index for the fundamental type.
*/
{
  if (is_null_index(this->fund_namespace_type)) {
    an_ifc_type_fundamental fund_type;

    this->fund_namespace_type = this->output_state->alloc_type(&fund_type);
    set_ifc_basis(&fund_type, ifc_tbs_namespace);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  }  /* if */
  return this->fund_namespace_type;
}  /* an_ifc_il_map::find_or_enter_namespace_scope_type */


an_ifc_type_index an_ifc_il_map::find_or_enter_alias_typedef_type()
/*
Find or enter the fundamental type used by the IFC to indicate a given IFC
DeclSort::Alias is a typedef.  Return the index for the fundamental type.
*/
{
  if (is_null_index(this->fund_alias_typedef_type)) {
    an_ifc_type_fundamental fund_type;

    this->fund_alias_typedef_type = this->output_state->alloc_type(&fund_type);
    set_ifc_basis(&fund_type, ifc_tbs_typename);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  }  /* if */
  return this->fund_alias_typedef_type;
}  /* an_ifc_il_map::find_or_enter_alias_typedef_type */


an_ifc_type_index an_ifc_il_map::find_or_enter_scoped_enum_type()
/*
Find or enter the fundamental type used by the IFC to indicate a given IFC
DeclSort::Enumeration is a scoped enum type.  Return the index for the
fundamental type.
*/
{
  if (is_null_index(this->fund_scoped_enum_type)) {
    an_ifc_type_fundamental fund_type;

    this->fund_scoped_enum_type = this->output_state->alloc_type(&fund_type);
    set_ifc_basis(&fund_type, ifc_tbs_class);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  }  /* if */
  return this->fund_scoped_enum_type;
}  /* an_ifc_il_map::find_or_enter_scoped_enum_type */


an_ifc_type_index an_ifc_il_map::find_or_enter_unscoped_enum_type()
/*
Find or enter the fundamental type used by the IFC to indicate a given IFC
DeclSort::Enumeration is an unscoped enum type.  Return the index for the
fundamental type.
*/
{
  if (is_null_index(this->fund_unscoped_enum_type)) {
    an_ifc_type_fundamental fund_type;

    this->fund_unscoped_enum_type = this->output_state->alloc_type(&fund_type);
    set_ifc_basis(&fund_type, ifc_tbs_enum);
    set_ifc_precision(&fund_type, ifc_tps_default);
    set_ifc_sign(&fund_type, ifc_tss_plain);
  }  /* if */
  return this->fund_unscoped_enum_type;
}  /* an_ifc_il_map::find_or_enter_unscoped_enum_type */


an_ifc_decl_index an_ifc_il_map::enter_namespace(a_scope_ptr scope)
/*
Enter the namespace corresponding to the given scope into the IFC output state.
Return the declaration index of the scope declaration.
*/
{
  check_assertion(scope->kind == sck_namespace ||
                  scope->kind == sck_namespace_extension ||
                  scope->kind == sck_namespace_reactivation);
  an_ifc_decl_scope
                scope_decl;
  an_ifc_decl_index
                result = this->map_new_home_scope(scope, &scope_decl);
  a_namespace_ptr
                nsp = scope->variant.assoc_namespace;
  /* Set the name information. */
  size_t        name_offset = add_name_to_string_table(this->output_state,
                                                       nsp);
  an_ifc_name_index
                ifc_name_index(scope_decl.get_file(), ifc_ns_text_offset,
                               name_offset);

  set_ifc_name(&scope_decl, ifc_name_index);

  /* Set the source location information. */
  a_source_position
                src_pos = nsp->source_corresp.decl_position;
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_pos(src_pos);
  set_ifc_locus(&scope_decl, ifc_src_pos);

  /* Set the IFC fundamental type to indicate this is a namespace scope. */
  an_ifc_type_index
                type = this->find_or_enter_namespace_scope_type();
  set_ifc_type(&scope_decl, type);

  /* Namespaces do not have a base type, so use a null type. */
  an_ifc_type_index
                base_type;
  set_ifc_base(&scope_decl, base_type);

  /* Associate the namespace scope decl with its contents. */
  an_ifc_scope_offset
                initializer = this->find_or_enter_scope(scope);
  set_ifc_initializer(&scope_decl, initializer);

  /* Set the scope information. */
  a_source_correspondence_ptr
                scp = &nsp->source_corresp;
  an_ifc_decl_index
                scope_decl_idx = this->find_or_enter_home_scope(scp);
  this->map_scope_member(scp, result);
  set_ifc_home_scope(&scope_decl, scope_decl_idx);
  return result;
}  /* an_ifc_il_map::enter_namespace */


an_ifc_decl_index an_ifc_il_map::enter_typedef(a_type_ptr type)
/*
Enter the typedef corresponding to the given type into the IFC output state.
Return the declaration index of the IFC DeclSort::Alias (i.e., the IFC
representation of the typedef).
*/
{
  check_assertion(type->kind == tk_typeref && typeref_is_typedef(type));
  an_ifc_decl_alias
                alias_decl;
  an_ifc_decl_index
                result = this->output_state->alloc_decl(&alias_decl);
  /* Set the name information. */
  size_t        name_offset = add_name_to_string_table(this->output_state,
                                                       type);
  an_ifc_text_offset
                ifc_name_offset(alias_decl.get_file(), name_offset);

  set_ifc_name(&alias_decl, ifc_name_offset);

  /* Set the source location information. */
  a_source_position
                src_pos = type->source_corresp.decl_position;
  an_ifc_source_location
                ifc_src_pos = this->find_or_enter_pos(src_pos);
  set_ifc_locus(&alias_decl, ifc_src_pos);

  /* Set the IFC fundamental type to indicate this is a typedef
     DeclSort::Alias. */
  an_ifc_type_index
                type_kind_type = this->find_or_enter_alias_typedef_type();
  set_ifc_type(&alias_decl, type_kind_type);

  /* Set the scope information. */
  a_source_correspondence_ptr
                scp = &type->source_corresp;
  an_ifc_decl_index
                scope_decl_idx = this->find_or_enter_home_scope(scp);
  this->map_scope_member(scp, result);
  set_ifc_home_scope(&alias_decl, scope_decl_idx);

  /* Set the aliasee type. */
  an_ifc_type_index
                aliasee = this->find_or_enter_type(type->variant.typeref.type);
  set_ifc_aliasee(&alias_decl, aliasee);
  /* FIXME: Set specifiers. */
  /* FIXME: Set access. */
  return result;
}  /* an_ifc_il_map::enter_typedef */


void an_ifc_il_map::map_scope_member(a_scope_ptr       scope,
                                     an_ifc_decl_index decl)
/*
Map the given declaration index to the given scope's associated array of scope
members.
*/
{
  an_ifc_scope_offset offset = this->find_or_enter_scope(scope);

  this->scope_members[offset.value - 1].push_back(decl);
}  /* an_ifc_il_map::map_scope_member */


void an_ifc_il_map::map_scope_member(a_source_correspondence_ptr scp,
                                     an_ifc_decl_index           decl)
/*
Map the given declaration index to the associated array of scope members for
the given source correspondent.
*/
{
  a_scope_ptr scope = scp->parent_scope;

  this->map_scope_member(scope, decl);
}  /* an_ifc_il_map::map_scope_member */

}  /* namespace */

static void dump_scope_recursively(an_ifc_il_map *il_map,
                                   a_scope_ptr   scope);


static void dump_scope_types(an_ifc_il_map *il_map,
                             a_scope_ptr   scope)
/*
Add all the types in the given scope to the given IL -> IFC mapping.
*/
{
  for (a_type_ptr type = scope->types; type != NULL; type = type->next) {
    (void)il_map->find_or_enter_type(type);
  }  /* for */
}  /* dump_scope_types */


static void dump_scope_routines(an_ifc_il_map *il_map,
                                a_scope_ptr   scope)
/*
Add all the routines in the given scope to the given IL -> IFC mapping.
*/
{
  for (a_routine_ptr rp = scope->routines; rp != NULL; rp = rp->next) {
    (void)il_map->find_or_enter_routine(rp);
  }  /* for */
}  /* dump_scope_routines */


static void dump_scope_namespaces(an_ifc_il_map *il_map,
                                  a_scope_ptr   scope)
/*
Add all the namespaces in the given scope to the given IL -> IFC mapping.
*/
{
  for (a_namespace_ptr np = scope->namespaces; np != NULL; np = np->next) {
    if (np->is_namespace_alias) {
      header_unit_catastrophe();
    } else {
      a_scope_ptr assoc_scope = np->variant.assoc_scope;

      dump_scope_recursively(il_map, assoc_scope);
    }  /* if */
  }  /* for */
}  /* dump_scope_namespaces */


static void dump_scope_recursively(an_ifc_il_map *il_map,
                                   a_scope_ptr   scope)
/*
Add the contents of the given scope and all its child scopes to the given IL ->
IFC mapping.
*/
{
  dump_scope_types(il_map, scope);
  dump_scope_routines(il_map, scope);
  dump_scope_namespaces(il_map, scope);
}  /* dump_scope_recursively */


static void complete_scope_info(an_ifc_output_state *output_state,
                                an_ifc_il_map       *il_map)
/*
The IFC scope membership information is built up during the IL -> IFC mapping
process.  This function is called once the IL has been completely mapped to the
output state to create the scope membership information as a sort of
"post-processing" step.

This is done as IFC scopes are required to be contiguous blocks of declaration
indexes; thus, by collecting the scope members and deferring construction no
special traversal logic is required to account for all scope members.
*/
{
  size_t num_of_scopes = il_map->get_number_of_scopes();
  size_t scope_start = 0;

  for (size_t i = 0; i < num_of_scopes; ++i) {
    an_ifc_scope_descriptor scope_descr;

    output_state->fetch_node(&scope_descr, i);

    const a_scope_member_array &members = il_map->get_scope_members(i);
    for (an_ifc_decl_index decl_idx : members) {
      an_ifc_scope_member scope_mem;

      (void)output_state->alloc_node(&scope_mem);
      set_ifc_index(&scope_mem, decl_idx);
    }  /* for */

    an_ifc_module_file *file = scope_descr.get_file();
    an_ifc_index       ifc_scope_start(file, scope_start);
    an_ifc_cardinality ifc_scope_count(file, members.length());
    set_ifc_start(&scope_descr, ifc_scope_start);
    set_ifc_cardinality(&scope_descr, ifc_scope_count);
    scope_start += members.length();
  }  /* for */
}  /* complete_scope_info */


static Opt<an_ifc_module_file> create_output_file(a_const_char  *file_path,
                                                  an_error_code file_kind)
/*
Create and return an instance of an IFC module file (with an open file handle
in binary write mode) intended for writing with the given file path and file
kind.

If creation of the file fails for any reason an empty optional is instead
returned.
*/
{
  FILE *f_handle = open_output_file_with_error_handling(
                                                 file_path,
                                                 /*binary_file=*/TRUE,
                                                 /*update_mode=*/FALSE,
                                                 /*open_flags=*/OFF_NO_OPTIONS,
                                                 file_kind);

  if (f_handle == NULL) {
    return {};
  }  /* if */

  an_ifc_module_file result(mk_edg_ifc, /*for_read=*/FALSE);
  result.version_major = 0;
  result.version_minor = 43;
  result.f_module = f_handle;

  an_ifc_module_file_write_state &write_state = result.get_write_state();
  write_state.file_kind = file_kind;
  write_state.source_file_name = primary_source_file_name;
  return {move_from(&result)};
}  /* create_output_file */


void ifc_modules_write_out()
/*
Write out the module files for the current translation unit in the EDG flavor
of the IFC format.
*/
{
  a_const_char  *output_file_name = module_header_unit_output_file_name;
  Opt<an_ifc_module_file>
                opt_module_file = create_output_file(output_file_name,
                                                     ec_edg_ifc_header_unit);

  if (opt_module_file.has_value()) {
    an_ifc_module_file  module_file = move_from(&(*opt_module_file));
    an_ifc_output_state output_state(&module_file);
    an_ifc_il_map       il_map(&output_state);
    a_scope_ptr         scope = il_header.primary_scope;
    an_ifc_scope_offset ifc_global_scope = il_map.enter_scope(scope);

    output_state.set_global_scope(ifc_global_scope);
    dump_scope_recursively(&il_map, scope);
    complete_scope_info(&output_state, &il_map);
    if (!output_state.write()) {
      /* FIXME: Add error? */
    }  /* if */
  }  /* if */
}  /* ifc_modules_write_out */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2024 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
