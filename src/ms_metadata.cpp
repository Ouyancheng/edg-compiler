/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2010 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/

/*

ms_metadata.cpp -- reading of C++/CLI metadata from assemblies.

This is a C++ file that relies on Microsoft Windows
APIs.  As a result, it can only be compiled on a Windows platform.
In addition, the C++ code makes use of C++0x features, so it must be
compiled by at least the Microsoft VC10 compiler or version 4.2 of the
EDG front end.

If you have DEFAULT_CPPCLI_ENABLED set to FALSE (the default), you
don't need this file.  You don't need to compile it, and you don't
need to link it in.  You can stick with the traditional C-only build
process.

Note that it uses the alink.h include file.  This file is not
currently part of the Windows SDK, although it is expected to be
at some point in the future.  The file can be downloaded at
http://code.msdn.microsoft.com/alink

*/

#include <windows.h>
#include <metahost.h>
#include <cor.h>
#include <alink.h>

#include <string>
#include <map>
#include <vector>
#include <stack>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <memory>

#include "basics.h"

EXTERN_C_BLOCK_IN_CPP_FILE
#include "error.h"
#include "ms_metadata.h"
#include "host_envir.h"
#include "mem_manage.h"
#if WRITE_CPPCLI_PORTABLE_ASSEMBLIES
#include "symbol_tbl.h"
#include "cmd_line.h"
#endif /* WRITE_CPPCLI_PORTABLE_ASSEMBLIES */
END_EXTERN_C_BLOCK_IN_CPP_FILE


using namespace std;


/*
If hr indicates an error occurred, issue an internal error indicating
that the routine specified by name failed.
*/
#define CHECK_API_RESULT(hr, name)                               \
  if (FAILED(hr)) {                                              \
    unexpected_condition_str("call to API routine " #name " failed"); \
  }


static wstring char_string_to_wstring(char* str)
/* 
Convert a char* to a std::wstring. 
*/
{
  return wstring(conv_utf8_to_wchar(str));
}  /* char_string_to_wstring */


template<typename T>
void release_and_zero_out_helper(T*& ptr)
/*
Helper function to call Release and then zero out a COM interface pointer.
*/
{
  if (ptr != nullptr) {
    ptr->Release();
    ptr = nullptr;
  }  /* if */
}  /* release_and_zero_out_helper */


ostream& operator<<(ostream& buffer, const wstring& text)
/*
Operator to convert text from wide characters to UTF-8 and output the result.
*/
{
  return buffer << conv_wide_to_utf8(const_cast<wchar_t*>(text.c_str()));
}  /* operator<< */


class an_import_scope;

/*
The representation of a single generic parameter.  Note: at the moment we only
really need the name but if we want to support constraints we may need the
other information.
*/
class a_generic_parameter {
public:
  a_generic_parameter(ULONG index, wstring&& name, DWORD flags)
    : index_(index),
      name_(move(name)),
      flags_(flags)
  {
  }  /* constructor */

  a_generic_parameter(a_generic_parameter&& other)
    : index_(move(other.index_)),
      name_(move(other.name_)),
      flags_(move(other.flags_))
  {
  }  /* constructor */

  wstring name() const
  {
    return name_;
  }  /* name */

private:
  ULONG         index_; 
                        /* The index of this generic parameter. */
  wstring       name_;  
                        /* The name of this generic parameter. */
  DWORD         flags_; 
                        /* Any flags associated with this generic parameter. */
}; /* a_generic_parameter */


/* A list of generic parameters. */
typedef vector<a_generic_parameter> a_generic_parameter_list;
/* Empty lists of generic type parameters and generic method parameters. */
a_generic_parameter_list no_generic_type_parameters;
a_generic_parameter_list no_generic_method_parameters;

/* 
A class to decode a CLR signature.  It returns the result as a std::wstring.
*/
class a_signature_decoder {
public:
  a_signature_decoder(
                     const an_import_scope&         import_scope,
                     PCCOR_SIGNATURE                signature, 
                     ULONG                          bytes_in_signature,
                     const a_generic_parameter_list &generic_type_parameters,
                     const a_generic_parameter_list &generic_method_parameters)
    : signature_(signature),
      bytes_in_signature_(bytes_in_signature),
      index_(0),
      import_scope_(import_scope),
      generic_type_parameters_(generic_type_parameters),
      generic_method_parameters_(generic_method_parameters)
  {
  }  /* constructor */

  wstring decode_type();
  wstring decode_method_signature(const wstring  &name, 
                                  bool           is_for_constructor,
                                  bool           is_for_property);

  wstring decode_field_signature()
  {
    check_assertion(generic_method_parameters_.empty());
    skip_calling_convention(IMAGE_CEE_CS_CALLCONV_FIELD);
    return decode_type();
  }  /* decode_field_signature */

private:
  BYTE read_one_byte()
  {
    check_assertion(index_ < bytes_in_signature_);
    return signature_[index_++];
  }  /* read_one_byte */


  ULONG read_four_bytes()
  {
    ULONG value;

    check_assertion(index_ < bytes_in_signature_);
    index_ += CorSigUncompressData(&signature_[index_], &value);
    return value;
  }  /* read_four_bytes */


  mdToken read_token()
  {
    mdToken value;

    check_assertion(index_ < bytes_in_signature_);
    index_ += CorSigUncompressToken(&signature_[index_], &value);
    return value;
  }  /* read_token */


  CorElementType get_element_type()
  {
    check_assertion(index_ < bytes_in_signature_);
    return static_cast<CorElementType>(signature_[index_++]);
  }  /* get_element_type */


  void skip_calling_convention(CorCallingConvention expected_value)
  {
    check_assertion(index_ < bytes_in_signature_);
    check_assertion((signature_[index_] & IMAGE_CEE_CS_CALLCONV_MASK) ==
                                                               expected_value);
    ++index_;
  }  /* skip_calling_convention */


  void decode_generic_arguments(wostringstream& buffer);

private:
  const PCCOR_SIGNATURE
                signature_;
                        /* The signature we want to decode. */
  const ULONG   bytes_in_signature_;
                        /* The number of bytes in the signature. */
  ULONG         index_;
                        /* The current index into the signature. */
  const an_import_scope
                &import_scope_;
                        /* The import scope associated with this signature. */
  const a_generic_parameter_list
                &generic_type_parameters_;
                        /* Any generic parameters associated with the type with
                           which this signature is associated. */
  const a_generic_parameter_list
                &generic_method_parameters_;
                        /* Any generic parameters associated with this method
                           signature. */
}; /* a_signature_decoder */


wstring a_signature_decoder::decode_method_signature(const wstring& name,
                                            bool           is_for_constructor,
                                            bool           is_for_property)
/*
Decode a function signature and then combine the various elements along with
the name of the function to create a declaration which we return as a
std::wstring.  Note: this function also handles decoding a property signature
which is almost the same as a method signature.
*/
{
  BYTE           first_byte;
  BYTE           calling_convention;
  BYTE           count_of_generic_parameters;
  wostringstream declaration;
  ULONG          number_of_parameters;
  wstring        return_type;

  /* A method cannot be both a constructor and a property method. */
  check_assertion(!is_for_constructor || !is_for_property);
  first_byte = read_one_byte();
  calling_convention = first_byte & IMAGE_CEE_CS_CALLCONV_MASK;
  check_assertion(!is_for_property ||
                  (calling_convention == IMAGE_CEE_CS_CALLCONV_PROPERTY));
  /* If this is a generic method we need to get the count of generic 
     parameters. */
  if ((first_byte & IMAGE_CEE_CS_CALLCONV_GENERIC) != 0) {
    count_of_generic_parameters = read_one_byte();
  } else {
    count_of_generic_parameters = 0;
  }  /* if */
  /* These two better agree. */
  check_assertion(generic_method_parameters_.size() ==
                                                  count_of_generic_parameters);
  /* If this is a generic method then we need to emit the generic header. */
  if (count_of_generic_parameters > 0) {
    declaration << L"generic<";
    for (BYTE i = 0; i < count_of_generic_parameters; ++i) {
      if (i > 0) {
        declaration << L", ";
      }  /* if */
      declaration << L"typename " << generic_method_parameters_[i].name();
    }  /* for */
    declaration << L"> ";
  }  /* if */
  number_of_parameters = read_four_bytes();
  return_type = decode_type();
  /* Constructors don't have a (visible) return type. */
  if (is_for_constructor) {
    check_assertion(return_type == L"void");
  } else {
    declaration << return_type << L' ';
  }  /* if */
  declaration << name;
  /* We only emit parameters for methods and parameterized properties (and we
     use "[]" instead of "()" for parameterized properties).  Simple properties
     do not have any parameters. */
  if (is_for_property) {
    if (number_of_parameters > 0) {
      declaration << L'[';
    }  /* if */
  } else {
    declaration << L'(';
  }  /* if */
  for (ULONG i = 0; i < number_of_parameters; ++i) {
    if (i > 0) {
      declaration << L", ";
    }  /* if */
    declaration << decode_type();
  }  /* for */
  switch (calling_convention) {
    case IMAGE_CEE_CS_CALLCONV_DEFAULT:
    case IMAGE_CEE_CS_CALLCONV_PROPERTY:
      break;
    case IMAGE_CEE_CS_CALLCONV_VARARG:
      if (number_of_parameters > 0) {
        declaration << L", ";
      }  /* if */
      declaration << L"...";
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  /* Close the parameter list: see above for the special rules for
     properties. */
  if (is_for_property) {
    if (number_of_parameters > 0) {
      declaration << L']';
    }  /* if */
  } else {
    declaration << L')';
  }  /* if */
  return declaration.str();
}  /* a_signature_decoder::decode_method_signature */


/* 
A class to decode a CLR constant: it returns the result as a std::string.
*/
class a_constant_decoder {
public:
  a_constant_decoder(DWORD         constant_type, 
                     UVCP_CONSTANT constant_value,
                     ULONG         characters_in_constant)
    : constant_type_(constant_type),
      constant_value_(constant_value),
      characters_in_constant_(characters_in_constant)
  {
  }  /* constructor */

  wstring decode();

private:
  template<typename T>
  static T convert_to(UVCP_CONSTANT value)
  /* 
  Cast the constant blob to the correct type. 
  */
  {
    return *reinterpret_cast<UNALIGNED const T*>(value);
  }  /* convert_to */

private:
  DWORD         constant_type_;
                        /* The type of the constant. */
  UVCP_CONSTANT constant_value_;
                        /* The value of the constant. */
  ULONG         characters_in_constant_;
                        /* The number of characters in the constant (usually
                           zero except for strings). */
}; /* a_constant_decoder. */


wstring a_constant_decoder::decode()
/*
Decode the constant.  Emit the constant as a hexadecimal constant and then
cast to the appropriate type.
*/
{
  wostringstream buffer;

  switch (constant_type_) {
    case ELEMENT_TYPE_BOOLEAN: 
      {
        bool value = convert_to<bool>(constant_value_);

        buffer << (value ? "true" : "false");
        break;
      }  /* case ELEMENT_TYPE_BOOLEAN */
    case ELEMENT_TYPE_CHAR: 
      { /* This type requires special handling to ensure that we get the
           decimal value not the character. */
        int value = convert_to<unsigned short>(constant_value_);

        buffer << L"0x" << hex << value;
        break;
      }  /* case ELEMENT_TYPE_CHAR */
    case ELEMENT_TYPE_I1: 
      { /* This type requires special handling to ensure that we get the
           decimal value not the character. */
        int value = convert_to<signed char>(constant_value_);

        buffer << L"0x" << hex << value;
        break;
      }  /* case ELEMENT_TYPE_I1 */
    case ELEMENT_TYPE_U1: 
      { /* This type requires special handling to ensure that we get the
           decimal value not the character. */
        unsigned int value = convert_to<unsigned char>(constant_value_);
        buffer << L"0x" << hex << value;
        break;
      }  /* case ELEMENT_TYPE_U1 */
    case ELEMENT_TYPE_I2:
      buffer << L"0x" << hex << convert_to<short>(constant_value_);
      break;
    case ELEMENT_TYPE_U2:
      buffer << L"0x" << hex << convert_to<unsigned short>(constant_value_);
      break;
    case ELEMENT_TYPE_I4:
      buffer << L"0x" << hex << convert_to<int>(constant_value_);
      break;
    case ELEMENT_TYPE_U4:
      buffer << L"0x" << hex << convert_to<unsigned int>(constant_value_);
      break;
    case ELEMENT_TYPE_I8:
      buffer << L"0x" << hex << convert_to<long long>(constant_value_);
      break;
    case ELEMENT_TYPE_U8:
      buffer << L"0x" << hex <<
                               convert_to<unsigned long long>(constant_value_);
      break;
    case ELEMENT_TYPE_R4:
      /* FIXME: This needs more work - we need to be able to handle INF and 
         NaN. */
      buffer << convert_to<float>(constant_value_);
      break;
    case ELEMENT_TYPE_R8:
      /* FIXME: This needs more work - we need to be able to handle INF and 
         NaN. */
      buffer << convert_to<double>(constant_value_);
      break;
    case ELEMENT_TYPE_STRING:
      { wchar_t *ch = (wchar_t*)(constant_value_);

        buffer << L"L\"";
        for (ULONG i = 0; i < characters_in_constant_; ++i) {
          buffer << ch[i];
        }  /* for */
        buffer << L'\"';
        break;
      } /* case ELEMENT_TYPE_STRING */
    default:
      unexpected_condition();
      break;
  }  /* switch */
  return buffer.str();
}  /* a_constant_decoder::decode */

/*
Classes for importing an assembly and a scope with an assembly.
*/

/*
The representation of a single assembly.  Note: an assembly may contain
multiple import scopes, but only one import scope contains metadata.
*/
class an_assembly {
public:
  an_assembly(const wstring                &full_assembly_name, 
              IALink                       *alink_interface,
              mdFile                       file_token,
              a_cpp_cli_feature_set        supported_features)
    : full_assembly_name_(full_assembly_name),
      assembly_index_(-1),
      alink_interface_(alink_interface),
      alink_token_(mdTokenNil),
      supported_features_(supported_features),
      md_assembly_import_interface_(nullptr),
      resolution_scope_(mdTokenNil),
      file_token_(file_token),
      count_of_scopes_(0)
  {
  }  /* constructor */


  an_assembly(an_assembly&& other)
    : full_assembly_name_(move(other.full_assembly_name_)),
      assembly_index_(move(other.assembly_index_)),
      alink_interface_(move(other.alink_interface_)),
      alink_token_(move(other.alink_token_)),
      supported_features_(move(other.supported_features_)),
      md_assembly_import_interface_(move(other.md_assembly_import_interface_)),
      resolution_scope_(move(other.resolution_scope_)),
      file_token_(move(other.file_token_)),
      count_of_scopes_(move(other.count_of_scopes_)),
      imported_scopes_(move(other.imported_scopes_))
  {
  }  /* constructor */


  an_assembly& operator=(an_assembly&& other)
  {
    full_assembly_name_ = move(other.full_assembly_name_);
    assembly_index_ = move(other.assembly_index_);
    alink_interface_ = move(other.alink_interface_);
    alink_token_ = move(other.alink_token_);
    supported_features_ = move(other.supported_features_);
    md_assembly_import_interface_ = move(other.md_assembly_import_interface_);
    resolution_scope_ = move(other.resolution_scope_);
    file_token_ = move(other.file_token_);
    count_of_scopes_ = move(other.count_of_scopes_);
    imported_scopes_ = move(other.imported_scopes_);
    return *this;
  }  /* operator= */
  
  bool process();
  void cleanup();

  a_cpp_cli_feature_set supported_features() const
  {
    return supported_features_;
  }  /* supported_features */


  an_assembly_index assembly_index() const
  {
    return assembly_index_;
  }  /* assembly_index */

  
  static void reset_index() 
  {
    index = 0;
  }  /* assembly_index */


  an_import_scope& find_scope()
  {
    check_assertion(!imported_scopes_.empty());
    check_assertion(imported_scopes_.size() == 1);
    return imported_scopes_.front();
  }  /* find_scope */

private:
  bool get_assembly_info();
  bool import_all_scopes();

private:
  static an_assembly_index    
                index;
                        /* Counter used to generate unique assembly index.
                           This is incremented when a new assembly is 
                           created. */

private:
  wstring       full_assembly_name_;
                        /* The full name of the assembly. */
  an_assembly_index
                assembly_index_;
                        /* The index of this assembly.  An index of greater 
                           than zero indicates the assembly was imported 
                           successfully.  An index of -1 (a temporary state)
                           indicates the assembly wasn't imported yet.  An 
                           index of 0 indicates the assembly failed to be 
                           imported. */
  IALink        *alink_interface_;
                        /* The interface to the functionality provided by
                           alink.dll. */
  mdToken       alink_token_;
                        /* The token to be used when calling any IALink API. */
  a_cpp_cli_feature_set 
                supported_features_;
                        /* The currently supported C++/CLI features. */
  IMetaDataAssemblyImport
                *md_assembly_import_interface_;
                        /* The IMetaDataAssemblyImport interface. */
  mdToken       resolution_scope_;
                        /* The resolution scope token for the assembly. */
  mdFile        file_token_;
                        /* The token for the current translation unit. */
  DWORD         count_of_scopes_;
                        /* The number of scopes in this assembly. */
  vector<an_import_scope>
                imported_scopes_;
                        /* The scopes that we imported from this assembly. */
};  /* an_assembly */

/* 
The possible top level types we can find in an assembly. 
*/
enum a_top_level_kind {
  tlk_unknown_kind,
  tlk_ref_class,
  tlk_value_type,
  tlk_interface,
  tlk_enumeration,
  tlk_delegate,
};  /* a_top_level_kind */

/*
The representation of a single import scope.  Each assembly can contain one
or more import scope - though only one import scope has the metadata for types.
*/
class an_import_scope {
public:
  an_import_scope(IMetaDataImport2* md_import2_interface,
                  an_assembly&      containing_assembly);

  an_import_scope(an_import_scope&& other)
    : scope_name_(move(other.scope_name_)),
      containing_assembly_(move(other.containing_assembly_)),
      md_import2_interface_(move(other.md_import2_interface_)),
      namespace_stack_(move(other.namespace_stack_))
  {
  }  /* constructor */

  an_import_scope &operator=(an_import_scope&& other);

  IMetaDataImport2 *import_interface() const
  {
    return md_import2_interface_;
  }  /* import_interface */


  const an_assembly &containing_assembly() const
  {
    return containing_assembly_;
  }  /* containing_assembly */


  void import_all_types(ostringstream &buffer);
  void import_one_type(ostringstream &buffer, 
                       mdTypeDef     typedef_token,
                       bool          at_top_level,
                       bool          want_definition,
                       bool          class_body_only);
  wstring resolve_type_token(
                       mdToken                        token,
                       const a_generic_parameter_list &generic_type_parameters,
                       bool                           replaces_dots) const;

  a_generic_parameter_list get_generic_parameters(mdToken token) const;

  void cleanup();

private:
  void import_enum_definition(ostringstream &buffer, 
                             mdTypeDef      typedef_token,
                             const wstring  &enumeration_name);
  void import_delegate_definition(ostringstream &buffer,
                                  mdTypeDef     typedef_token,
                                  const wstring &delegate_name);

  a_top_level_kind classify_type(
                       DWORD                          attributes,
                       const a_generic_parameter_list &generic_type_parameters,
                       mdToken                        extends_token);
  wstring top_level_kind_as_wstring(a_top_level_kind kind);

  void open_namespace_scopes(ostringstream &buffer,
                             const wstring &namespace_name);
  void open_single_namespace_scope(ostringstream &buffer,
                                   const wstring &namespace_name);
  void open_multiple_namespace_scopes(ostringstream         &buffer,
                                      const vector<wstring> &namespaces);
  void open_namespace(ostringstream &buffer, const wstring &namespace_name);

  void close_all_namespace_scopes(ostringstream &buffer);
  void close_namespace(ostringstream &buffer, const wstring &namespace_name);

private:
  wstring       scope_name_;
                        /* The name of this scope. */
  an_assembly   &containing_assembly_;
                        /* The assembly that contains this scope. */
  IMetaDataImport2
                *md_import2_interface_;
                        /* The IMetaDataImport2 interface. */
  vector<wstring>
                namespace_stack_;
                        /* The stack of active namespaces. */
  map<mdTypeDef, wstring>
                map_of_tokens_to_names_;
                        /* A mapping from a mdTypeDef to the name of the type:
                           useful when we want the name of a type we have
                           already imported. */
}; /* an_import_scope */


an_import_scope::an_import_scope(IMetaDataImport2 *md_import2_interface,
                                 an_assembly      &containing_assembly)
/*
Create a representation of an import scope and get the information about the
scope that we will need later.  Currently this is just the name of the scope.
*/
  : md_import2_interface_(md_import2_interface),
    containing_assembly_(containing_assembly)
{
  ULONG               characters_in_name;
  HRESULT             hr;
  unique_ptr<WCHAR[]> name_buffer;

  hr = md_import2_interface_->GetScopeProps(/*szName=*/nullptr, 
                                            /*cchName=*/0, 
                                            &characters_in_name,
                                            /*pmvid=*/nullptr);
  CHECK_API_RESULT(hr, GetScopeProps);
  name_buffer.reset(new WCHAR[characters_in_name]);
  hr = md_import2_interface_->GetScopeProps(name_buffer.get(),
                                            characters_in_name,
                                            &characters_in_name,
                                            /*pmvid=*/nullptr);
  CHECK_API_RESULT(hr, GetScopeProps);
  scope_name_ = name_buffer.get();
}  /* an_import_scope::an_import_scope */


void an_import_scope::cleanup()
/*
Cleanup an import scope. 
*/
{
  release_and_zero_out_helper(md_import2_interface_);
}  /* an_import_scope::cleanup */


void an_import_scope::import_all_types(ostringstream& buffer)
/*
Import all the types from an import scope.
*/
{
  HCORENUM  enum_typedefs = 0;
  mdTypeDef typedefs[64];
  ULONG     count_of_typedefs;

  do {
    HRESULT hr = md_import2_interface_->EnumTypeDefs(&enum_typedefs, typedefs,
                                                     _countof(typedefs),
                                                     &count_of_typedefs);

    CHECK_API_RESULT(hr, EnumTypeDefs);
    for (ULONG i = 0; i < count_of_typedefs; ++i) {
      import_one_type(buffer, typedefs[i], /*at_top_level=*/true, 
                      /*want_definition=*/false, /*class_body_only=*/false);
    }  /* for */
  } while (count_of_typedefs > 0);
  md_import2_interface_->CloseEnum(enum_typedefs);
  close_all_namespace_scopes(buffer);
}  /* an_import_scope::import_all_types */


void an_import_scope::open_namespace(ostringstream& buffer,
                                     const wstring& namespace_name)
/*
Emit the text to open a namespace scope.
*/
{
  buffer << "namespace ";
  if (namespace_name.find(L'<') != wstring::npos) {
    /* Use the __identifier keyword. */
    buffer << "__identifier(\"" << namespace_name << "\")";
  } else {
    buffer << namespace_name;
  }  /* if */
  buffer << " { ";
#if DEBUG
  buffer << endl;
#endif /* DEBUG */
}  /* an_import_scope::open_namespace */


void an_import_scope::close_namespace(ostringstream& buffer,
                                                 const wstring& namespace_name)
/*
Emit the text to close a namespace scope.
*/
{
  buffer << "}" ;
#if DEBUG
  buffer << "  /* namespace " << namespace_name << " */ " << endl;
#endif /* DEBUG */
}  /* an_import_scope::close_namespace */


void an_import_scope::open_multiple_namespace_scopes(
                                             ostringstream         &buffer,
                                             const vector<wstring> &namespaces)
/*
Open multiple namespaces.  Basically we want to keep open those namespaces
that we need and which are already open, close those we don't need and then
open any new namespaces.
*/
{
  auto iter1 = namespace_stack_.begin();
  auto end1  = namespace_stack_.end();
  auto iter2 = namespaces.begin();
  auto end2  = namespaces.end();

  /* Walk up each of the namespace stacks. */
  while ((iter1 != end1) && (iter2 != end2)) {
    /* If the namespaces match then move on to the next elements: assuming
       there are any. */
    if (*iter1 == *iter2) {
      ++iter1;
      ++iter2;
    } else {
      break;
    }  /* if */
  }  /* while */
  /* If the current namespace stack still has namespaces beyond this point then
     we need to close them in the correct order. */
  if (iter1 != end1) {
    stack<wstring> namespaces_to_close;

    for_each(iter1, end1, [&](const wstring& namespace_name) {
                            namespaces_to_close.push(namespace_name);
                          });
    namespace_stack_.erase(iter1, end1);
    while (!namespaces_to_close.empty()) {
      close_namespace(buffer, namespaces_to_close.top());
      namespaces_to_close.pop();
    }  /* while */
  }  /* if */
  /* If the new list of namespaces still has namespaces then we need to open
     them. */
  if (iter2 != end2) {
    for_each(iter2, end2, [&](const wstring& namespace_name) {
                            open_namespace(buffer, namespace_name);
                            namespace_stack_.push_back(namespace_name);
                          });
  }  /* if */
}  /* an_import_scope::open_multiple_namespace_scopes */


void an_import_scope::open_single_namespace_scope(
                                                 ostringstream &buffer,
                                                 const wstring &namespace_name)
/*
Open a single namespace.
*/
{
  if (namespace_stack_.empty()) {
    /* The namespace stack is empty so just add the new namespace. */
    open_namespace(buffer, namespace_name);
    namespace_stack_.push_back(namespace_name);
  } else {
    check_assertion(namespace_stack_.size() >= 1);
    /* If there is more than one namespace on the namespace stack then we need
       to remove all but the last (outermost) namespace. */
    while (namespace_stack_.size() != 1) {
      close_namespace(buffer, namespace_stack_.back());
      namespace_stack_.pop_back();
    }  /* while */
    /* If the outermost namespace doesn't match the new namespace then we need
       to replace it with the new namespace. */
    if (namespace_stack_.back() != namespace_name) {
      close_namespace(buffer, namespace_stack_.back());
      open_namespace(buffer, namespace_name);
      namespace_stack_.back() = namespace_name;
     }  /* if */
  }  /* if */
}  /* an_import_scope::open_single_namespace_scope */


void an_import_scope::open_namespace_scopes(ostringstream &buffer,
                                            const wstring &namespace_name)
/*
Open one or more namespace scopes.
*/
{
  /* Determine whether this is a single namespace or multiple namespaces. */
  if (namespace_name.find(L'.') == wstring::npos) {
    open_single_namespace_scope(buffer, namespace_name);
  } else {
    /* Multiple namespaces - split the name into its individual elements. */
    vector<wstring> namespaces;
    string::size_type start = 0;

    for (;;) {
      auto end = namespace_name.find(L'.', start);

      if (end != wstring::npos) {
        namespaces.push_back(wstring(namespace_name, start, end - start));
        start = end + 1;
      } else {
        namespaces.push_back(wstring(namespace_name, start));
        break;
      }  /* if */
    }  /* for */
    /* Now open the multiple namespaces. */
    open_multiple_namespace_scopes(buffer, namespaces);
  }  /* if */
}  /* an_import_scope::open_namespace_scopes */


void an_import_scope::close_all_namespace_scopes(ostringstream &buffer)
/* 
Close all the open namespace scopes.
*/
{
  while (!namespace_stack_.empty()) {
    close_namespace(buffer, namespace_stack_.back());
    namespace_stack_.pop_back();
  }  /* while */
}  /* an_import_scope::close_all_namespace_scopes */


a_top_level_kind an_import_scope::classify_type(
                       DWORD                          attributes,
                       const a_generic_parameter_list &generic_type_parameters,
                       mdToken                        extends_token)
/* 
Classify a type into one of the top-level kinds - this depends on the 
attributes associated with the type and/or the type that this type extends.
*/
{
  a_top_level_kind kind = tlk_unknown_kind;

  if (IsTdInterface(attributes)) {
    kind = tlk_interface;
  } else if (!IsNilToken(extends_token)) {
    wstring extends_class_name = resolve_type_token(extends_token,
                                                    generic_type_parameters,
                                                    /*replaces_dots=*/false);

    if (extends_class_name == L"System.ValueType") {
      kind = tlk_value_type;
    } else if (extends_class_name == L"System.Enum") {
      kind = tlk_enumeration;
    } else if (extends_class_name == L"System.MulticastDelegate") {
      kind = tlk_delegate;
    } else {
      /* Not one of the above so this must be a ref-class. */
      kind = tlk_ref_class;
    }  /* if */
  } else {
    /* Not an interface and it doesn't extend anything so this must be
       System.Object which is a ref-class. */
    kind = tlk_ref_class;
  }  /* if */
  return kind;
}  /* an_import_scope::classify_type */


an_assembly_index an_assembly::index = 0;

void an_assembly::cleanup()
/*
Cleanup up an assembly once we have finished processing. 
*/
{
  for_each(imported_scopes_.begin(), imported_scopes_.end(),
           [](an_import_scope& scope) {
             scope.cleanup();
           });
  release_and_zero_out_helper(md_assembly_import_interface_);
}  /* an_assembly::cleanup */


bool an_assembly::get_assembly_info()
/*
Get all the pertinent information associated with this assembly.
*/
{
  HRESULT hr;

  hr = alink_interface_->ImportFile(
                             full_assembly_name_.c_str(), 
                             /*pszTargetName=*/nullptr, /*fSmartImport=*/FALSE,
                             &alink_token_, &md_assembly_import_interface_,
                             &count_of_scopes_);
  if (SUCCEEDED(hr)) {
    hr = alink_interface_->GetResolutionScope(AssemblyIsUBM, file_token_,
                                              alink_token_,
                                              &resolution_scope_);
  }  /* if */
  return SUCCEEDED(hr);
}  /* an_assembly::get_assembly_info */


bool an_assembly::import_all_scopes()
/*
Import all the import scopes associated with this assembly.  Note, only one 
import scope will contain any interesting metadata.
*/
{
  bool processed_an_interesting_scope = false;
  bool result = true;
  
  for (DWORD scope_index = 0; scope_index < count_of_scopes_; ++scope_index) {
    IMetaDataImport  *md_import_inferface  = nullptr;
    IMetaDataImport2 *md_import2_inferface = nullptr;
    HRESULT          hr;

    hr = alink_interface_->GetScope(AssemblyIsUBM, alink_token_, scope_index,
                                    &md_import_inferface);
    if (FAILED(hr)) {
      result = false;
      break;
    } else if ((hr == S_FALSE) || (md_import_inferface == nullptr)) {
      /* There are no types in this scope.  Skip it. */
      continue;
    }  /* if */
    check_assertion(!processed_an_interesting_scope);
    /* Query interface to the new, improved interface. */
    hr = md_import_inferface->QueryInterface(
                              IID_IMetaDataImport2,
                              reinterpret_cast<void**>(&md_import2_inferface));
    if (FAILED(hr)) {
      result = false;
      break;
    }  /* if */
    /* Release old interface. */
    md_import_inferface->Release();
    /* Create an import scope and import all the types. */
    imported_scopes_.push_back(an_import_scope(md_import2_inferface, *this));
    processed_an_interesting_scope = true;
  }  /* for */
  return result;
}  /* an_assembly::import_all_scopes */


bool an_assembly::process()
/*
Process a single assembly.  This includes getting information about the
assembly and importing its scopes.  If succeeds, assign a non-zero assembly 
index.  If fails, the assembly index will be 0.
*/
{
  bool result = false;

  check_assertion(assembly_index_ == -1);
  if (get_assembly_info()) {
    check_assertion(count_of_scopes_ > 0);
    if (import_all_scopes()) {
      result = true;
    }  /* if */
  }  /* if */
  /* Assign the assembly index. */
  assembly_index_ = result ? ++index : 0;
  return result;
}  /* an_assembly::process */


void an_import_scope::import_enum_definition(ostringstream &buffer,
                                             mdTypeDef     typedef_token,
                                             const wstring &enumeration_name)
/*
Import the definition of an enumeration and emit the code for the definition.
*/
{
  /* Each enumerator is represented by a name-value pair. */
  typedef pair<wstring, wstring> an_enumerator; 
  HCORENUM              enum_members = 0;
  mdTypeDef             members[64];
  ULONG                 count_of_members;
  HRESULT               hr;
  wstring               underlying_type;
  vector<an_enumerator> enumerators;

  do {
    hr = md_import2_interface_->EnumMembers(&enum_members, typedef_token,
                                            members, _countof(members),
                                            &count_of_members);
    CHECK_API_RESULT(hr, EnumMembers);
    /* Reserve the necessary space. */
    enumerators.reserve(enumerators.size() + count_of_members);
    for (ULONG i = 0; i < count_of_members; ++i) {
      ULONG               characters_in_name, characters_in_constant;
      unique_ptr<WCHAR[]> name_buffer;
      DWORD               attributes, constant_type;
      PCCOR_SIGNATURE     signature;
      ULONG               bytes_in_signature;
      UVCP_CONSTANT       constant_value;
      
      /* Get the number of characters in the name. */
      hr = md_import2_interface_->GetMemberProps(
                                    members[i], /*pClass=*/nullptr, 
                                    /*szMember=*/nullptr, /*cchMember=*/0, 
                                    &characters_in_name, /*pdwAttr=*/nullptr, 
                                    /*ppvSigBlob=*/nullptr, 
                                    /*pcbSigBlob=*/nullptr, 
                                    /*pulCodeRVA=*/nullptr, 
                                    /*pdwImplFlags=*/nullptr, 
                                    /*pdwCPlusTypeFlag=*/nullptr, 
                                    /*ppValue=*/nullptr,
                                    /*pcchValue=*/nullptr);
      CHECK_API_RESULT(hr, GetMemberProps);
      name_buffer.reset(new WCHAR[characters_in_name]);
      /* Get the name, the type and the constant associated with this
         enumerator. */
      hr = md_import2_interface_->GetMemberProps(
                                          members[i], /*pClass=*/nullptr,
                                          name_buffer.get(),
                                          characters_in_name,
                                          &characters_in_name, &attributes,
                                          &signature, &bytes_in_signature,
                                          /*pulCodeRVA=*/nullptr, 
                                          /*pdwImplFlags=*/nullptr, 
                                          &constant_type, &constant_value,
                                          &characters_in_constant);
      CHECK_API_RESULT(hr, GetMemberProps);
      if (wcscmp(name_buffer.get(), L"value__") == 0) {
        /* This is the special member: its type is the underlying type of the
           enumeration. */
        a_signature_decoder decoder(*this, signature, bytes_in_signature,
                                    no_generic_type_parameters,
                                    no_generic_method_parameters);
        
        check_assertion(IsFdRTSpecialName(attributes) != 0);
        check_assertion(constant_type == ELEMENT_TYPE_VOID);
        check_assertion(underlying_type.empty());
        underlying_type = decoder.decode_field_signature();
      } else {
        a_constant_decoder decoder(constant_type, constant_value,
                                   characters_in_constant);

        /* Get the constant value associated with it. */
        enumerators.push_back(an_enumerator(wstring(name_buffer.get()),
                                            decoder.decode()));
      }  /* if */
    }  /* for */
  } while (count_of_members > 0);
  md_import2_interface_->CloseEnum(enum_members);
  /* Now we have all the information we need, we can emit the definition of
     the enumeration.  Note, we emit the value of an enumerator as a
     hexadecimal constant cast to the underlying type of the enumeration. */
  check_assertion(!underlying_type.empty());
  buffer << enumeration_name << " : " << underlying_type << " { ";
#if DEBUG
  buffer << endl;
#endif /* DEBUG */
  for_each(enumerators.begin(), enumerators.end(),
          [&](const an_enumerator& enumerator) {
            buffer << enumerator.first;
            buffer << " = static_cast<";
            buffer << underlying_type << ">(";
            buffer << enumerator.second;
            buffer << "), ";
#if DEBUG
            buffer << endl;
#endif /* DEBUG */
          });
  buffer << "}; ";
#if DEBUG
  buffer << endl;
#endif /* DEBUG */
}  /* an_import_scope::import_enum_definition */


void an_import_scope::import_delegate_definition(ostringstream &buffer,
                                                 mdTypeDef     typedef_token,
                                                 const wstring &delegate_name)
/*
Import the definition of a delegate.  This is essentially the signature of
the Invoke method - which every delegate must have.
*/
{
  HCORENUM                 enum_methods = 0;
  mdMethodDef              methods[4];
  ULONG                    count_of_methods;
  HRESULT                  hr;
  a_generic_parameter_list generic_type_parameters =
                                      get_generic_parameters(typedef_token);
  a_cpp_cli_feature_set    supported_features = 
                                     containing_assembly_.supported_features();

  /* If this is a generic delegate and we don't support and/or want generic
     types then we should just return. */
  if (generic_type_parameters.empty() || 
      (supported_features & cpp_cli_generic_types) != 0) {
    hr = md_import2_interface_->EnumMethodsWithName(&enum_methods, 
                                                    typedef_token,
                                                    L"Invoke", methods,
                                                    _countof(methods),
                                                    &count_of_methods);
    CHECK_API_RESULT(hr, EnumMethodsWithName);
    /* There should be only one Invoke method.  Get the signature and 
       decode it. */
    if (count_of_methods == 1) {
      PCCOR_SIGNATURE signature;
      ULONG           bytes_in_signature;

      hr = md_import2_interface_->GetMethodProps(
                                       methods[0], /*pClass=*/nullptr, 
                                       /*szMethod=*/nullptr, /*cchMethod=*/0,
                                       /*pchMethod=*/nullptr,
                                       /*pdwAttr=*/nullptr, 
                                       &signature, &bytes_in_signature, 
                                       /*pulCodeRVA=*/nullptr, 
                                       /*pdwImplFlags=*/nullptr);
      CHECK_API_RESULT(hr, GetMethodProps);
      a_signature_decoder decoder(*this, signature, bytes_in_signature,
                                  generic_type_parameters,
                                  no_generic_method_parameters);
      buffer << decoder.decode_method_signature(delegate_name, 
                                                /*is_for_constructor=*/false, 
                                                /*is_for_property=*/false);
      buffer << "; ";
#if DEBUG
      buffer << endl;
#endif /* DEBUG */
    } else {
      unexpected_condition();
    }  /* if */
  md_import2_interface_->CloseEnum(enum_methods);
  }  /* if */
}  /* an_import_scope::import_delegate_definition */


a_generic_parameter_list an_import_scope::get_generic_parameters(
                                                           mdToken token) const
/* 
Return the generic parameters associated with this type or method.  Return an 
empty list if there are no generic parameters.
*/
{
  a_generic_parameter_list generic_parameter_list;
  HCORENUM                 enum_generic_parameters = nullptr;
  mdGenericParam           generic_parameters[4];
  ULONG                    count_of_generic_parameters;
  HRESULT                  hr;

  do {
    hr = md_import2_interface_->EnumGenericParams(
                   &enum_generic_parameters, token, generic_parameters,
                   _countof(generic_parameters), &count_of_generic_parameters);
    CHECK_API_RESULT(hr, EnumGenericParams);
    for (ULONG i = 0; i < count_of_generic_parameters; ++i) {
      ULONG               characters_in_name;
      ULONG               index;
      DWORD               flags;
      unique_ptr<WCHAR[]> name_buffer;

      hr = md_import2_interface_->GetGenericParamProps(
                               generic_parameters[i], /*pulParamSeq=*/nullptr, 
                               /*pdwParamFlags=*/nullptr, /*ptOwner=*/nullptr, 
                               /*reserved=*/nullptr, /*wzName=*/nullptr, 
                               /*cchName=*/0, &characters_in_name);
      CHECK_API_RESULT(hr, GetGenericParamProps);
      name_buffer.reset(new WCHAR[characters_in_name]);
      hr = md_import2_interface_->GetGenericParamProps(generic_parameters[i],
                                                       &index, &flags,
                                                       /*ptOwner=*/nullptr, 
                                                       /*reserved=*/nullptr,
                                                       name_buffer.get(),
                                                       characters_in_name,
                                                       &characters_in_name);
      CHECK_API_RESULT(hr, GetGenericParamProps);
      generic_parameter_list.push_back(a_generic_parameter(
                                                    index,
                                                    wstring(name_buffer.get()),
                                                    flags));
    }  /* for */
  } while (count_of_generic_parameters > 0);
  md_import2_interface_->CloseEnum(enum_generic_parameters);
  return generic_parameter_list;
}  /* an_import_scope::get_generic_parameters */


wstring an_import_scope::resolve_type_token(
                       mdToken                        token,
                       const a_generic_parameter_list &generic_type_parameters,
                       bool                           replace_dots) const
/*
Resolve the type given by the token.  Get the name of the type and replace
any "." in the name with the C++ scope operator, "::".
*/
{
  wstring type_name;
  /* First check if this token is a typedef token for a type we have already
     imported within the current scope. */
  auto    iter = map_of_tokens_to_names_.find(token);

  if (iter != map_of_tokens_to_names_.end()) {
    type_name = iter->second;
  } else {
    unique_ptr<WCHAR[]> name_buffer;
    ULONG               characters_in_name;
    HRESULT             hr;
    
    switch (TypeFromToken(token)) {
      case mdtTypeDef:
        hr = md_import2_interface_->GetTypeDefProps(
                                         token, /*szTypeDef=*/nullptr,
                                         /*cchTypeDef=*/0, &characters_in_name,
                                         /*pdwTypeDefFlags=*/nullptr, 
                                         /*ptkExtends=*/nullptr);
        CHECK_API_RESULT(hr, GetTypeDefProps);
        name_buffer.reset(new WCHAR[characters_in_name]);
        hr = md_import2_interface_->GetTypeDefProps(
                                                   token, name_buffer.get(),
                                                   characters_in_name,
                                                   /*pchTypeDef=*/nullptr,
                                                   /*pdwTypeDefFlags=*/nullptr,
                                                   /*ptkExtends=*/nullptr);
        CHECK_API_RESULT(hr, GetTypeDefProps);
        break;
      case mdtTypeRef:
        hr = md_import2_interface_->GetTypeRefProps(
                                         token, /*ptkResolutionScope=*/nullptr,
                                         /*szName=*/nullptr, /*cchName=*/0,
                                         /*pchName=*/&characters_in_name);
        CHECK_API_RESULT(hr, GetTypeRefProps);
        name_buffer.reset(new WCHAR[characters_in_name]);
        hr = md_import2_interface_->GetTypeRefProps(
                                         token, /*ptkResolutionScope=*/nullptr,
                                         name_buffer.get(), characters_in_name,
                                         /*pchName=*/nullptr);
        CHECK_API_RESULT(hr, GetTypeRefProps);
        break;
      case mdtInterfaceImpl:
        /* If this is an interface-impl token then get the token for the 
           interface definition and then attempt to resolve that token. */
        hr = md_import2_interface_->GetInterfaceImplProps(
                                         token, /*mdTypeDef=*/nullptr, &token);
        CHECK_API_RESULT(hr, GetInterfaceImplProps);
        type_name = resolve_type_token(token, generic_type_parameters, 
                                       replace_dots);
        break;
      case mdtTypeSpec: {
        PCCOR_SIGNATURE signature;
        ULONG           bytes_in_signature;

        hr = md_import2_interface_->GetTypeSpecFromToken(token, &signature,
                                                         &bytes_in_signature);
        CHECK_API_RESULT(hr, GetTypeSpecFromToken);
        a_signature_decoder decoder(*this, signature, bytes_in_signature,
                                    generic_type_parameters,
                                    no_generic_method_parameters);
        type_name = decoder.decode_type();
        break;
      }  /* case mdtTypeSpec */
      default:
        unexpected_condition();
        break;
    }  /* switch */
    if (name_buffer) {
      check_assertion(type_name.empty());
      type_name = name_buffer.get();
    }  /* if */
  }  /* if */
  /* If necessary replace any "." in the type-name with the C++ token "::". */
  if (replace_dots) {
    wstring::size_type start = 0;
    auto               dot   = type_name.find(L'.', start);

    while (dot != wstring::npos) {
      type_name.replace(dot, 1, L"::");
      start = dot + 2;
      dot = type_name.find(L'.', start);
    }  /* while */
  }  /* if */
  return type_name;
}  /* an_import_scope::resolve_type_token */


an_import_scope& an_import_scope::operator=(an_import_scope&& other)
{
  scope_name_ = move(other.scope_name_);
  containing_assembly_ = move(other.containing_assembly_);
  md_import2_interface_ = move(other.md_import2_interface_);
  namespace_stack_ = move(other.namespace_stack_);
  return *this;
}  /* an_import_scope::operator= */


/* 
The representation of a single type definition.  This could be either a
ref-class, a value-type or an interface.
*/
class a_type_definition {
private:
  typedef unsigned int an_accessibility;
  
private:
  /* 
  The representation of a single property method - either a get method or a
  set method.
  */
  class a_property_method {
  public:
    a_property_method(mdMethodDef method_token)
      : method_token_(method_token)
    {
    }  /* constructor */


    a_property_method(mdMethodDef method_token, wstring&& method_name,
                      DWORD attributes)
      : method_token_(method_token),
        method_name_(move(method_name)),
        attributes_(attributes)
    {
    }  /* constructor */


    a_property_method(a_property_method&& other)
      : method_token_(move(other.method_token_)),
        method_name_(move(other.method_name_)),
        attributes_(move(other.attributes_))
    {
    }  /* constructor */


    a_property_method& operator=(a_property_method&& other)
    {
      method_token_ = move(other.method_token_);
      method_name_ = move(other.method_name_);
      attributes_ = move(other.attributes_);
      return *this;
    }  /* operator= */


    mdMethodDef token() const
    {
      return method_token_;
    }  /* token */


    an_accessibility accessibility() const
    {
      return exists() ? get_accessibility(attributes_) : mdPrivateScope;
    }  /* accessibility */


    bool exists() const
    {
      return !IsNilToken(method_token_);
    }  /* exists */

  private:
    mdMethodDef method_token_;
                        /* The token associated with this method. */
    wstring     method_name_;
                        /* The name of the method. */
    DWORD       attributes_;
                        /* The attributes for this method. */
  };  /* a_property_method */

  /* 
  The representation of a single event method - either an add method, a remove
  method, or a raise method.
  */
  class an_event_method {
  public:
    an_event_method(mdMethodDef method_token)
      : method_token_(method_token)
    {
    }  /* constructor */


    an_event_method(mdMethodDef method_token, 
                    wstring&&   method_name,
                    DWORD       attributes)
      : method_token_(method_token),
        method_name_(move(method_name)),
        attributes_(attributes)
    {
    }  /* constructor */


    an_event_method(an_event_method&& other)
      : method_token_(move(other.method_token_)),
        method_name_(move(other.method_name_)),
        attributes_(move(other.attributes_))
    {
    }  /* constructor */


    an_event_method& operator=(an_event_method&& other)
    {
      method_token_ = move(other.method_token_);
      method_name_ = move(other.method_name_);
      attributes_ = move(other.attributes_);
      return *this;
    }  /* operator= */


    mdMethodDef token() const
    {
      return method_token_;
    }  /* token */


    an_accessibility accessibility() const
    {
      return exists() ? get_accessibility(attributes_) : mdPrivateScope;
    }  /* accessibility */


    bool exists() const
    {
      return !IsNilToken(method_token_);
    }  /* exists */

  private:
    mdMethodDef method_token_;
                        /* The token associated with this method. */
    wstring     method_name_;
                        /* The name of the method. */
    DWORD       attributes_;
                        /* The attributes for this method. */
  };  /* an_event_method */

public:
  a_type_definition(mdTypeDef                      typedef_token, 
                    const wstring                  &type_name,
                    mdToken                        extends_token,
                    const a_generic_parameter_list &generic_type_parameters,
                    an_import_scope                &import_scope,
                    a_top_level_kind               kind)
    : typedef_token_(typedef_token),
      type_name_(type_name),
      extends_token_(extends_token),
      generic_type_parameters_(generic_type_parameters),
      import_scope_(import_scope),
      md_import2_interface_(import_scope_.import_interface()),
      kind_(kind),
      is_first_base_class_processed(false)
  {
  }  /* constructor */

  void import_definition(ostringstream& buffer);

private:
  void process_extends(ostringstream& buffer);
  void process_interfaces(ostringstream& buffer);
  void import_all_members(ostringstream& buffer);
  void import_nested_classes(ostringstream& buffer);
  void import_one_member(ostringstream &buffer, 
                         mdTypeDef     member_token,
                         bool          accept_property_method,
                         bool          accept_event_method);
  void import_properties(ostringstream& buffer);
  void import_one_property(ostringstream& buffer, mdProperty property_token);
  a_property_method import_property_method(mdMethodDef method_token);

  void import_events(ostringstream& buffer);
  void import_one_event(ostringstream& buffer, mdEvent event_token);
  an_event_method import_event_method(mdMethodDef method_token);

private:
  bool is_accessible(an_accessibility accessibility);
  bool is_nested_type_accessible(DWORD nested_type_attributes);

  wstring accessibility_as_wstring(an_accessibility accessibility);
  wstring nested_type_accessibility_as_wstring(DWORD nested_type_attributes);

  static an_accessibility get_accessibility(DWORD member_attributes)
  {
    return member_attributes & mdMemberAccessMask;
  }  /* get_accessibility */


private:
  mdTypeDef     typedef_token_;
                        /* The mdTypeDef token for this type. */
  const wstring
                &type_name_;
                        /* The name of the type. */
  mdToken       extends_token_;
                        /* The token for the type that this type extends. */
  const a_generic_parameter_list&
                generic_type_parameters_;
                        /* The generic parameters associated with this type. */
  an_import_scope
                &import_scope_;
                        /* The import scope in which this type is defined. */
  IMetaDataImport2
                *md_import2_interface_;
                        /* The appropriate IMetaDataImport2 interface. */
  a_top_level_kind
                kind_;  /* The top-level kind of this type. */
  bool          is_first_base_class_processed;
                        /* True if the first case class has been processed. */
};  /* a_type_definition */


void a_type_definition::process_extends(ostringstream& buffer)
/*
Decode the extends token and emit the appropriate text.
*/
{
  wstring extends_name = import_scope_.resolve_type_token(
                                                      extends_token_,
                                                      generic_type_parameters_,
                                                      /*replaces_dots=*/true);
  if (kind_ == tlk_value_type) {
    /* By definition all value types extend System.ValueType so there is no
       need to explicitly add it as a base-class. */
    check_assertion(extends_name == L"System::ValueType");
  } else if (!extends_name.empty()) {
    /* Similarly with ref classes: by definition they all extend (directly or
       indirectly) System.Object. */
    if ((kind_ != tlk_ref_class) || (extends_name != L"System::Object")) {
      buffer << " : " << extends_name;
      is_first_base_class_processed = true;
    }  /* if */
  }  /* if */
}  /* a_type_definition::process_extends */


void a_type_definition::process_interfaces(ostringstream &buffer)
/*
Decode the interface tokens (if there are any) and emit the appropriate text.
*/
{
  HCORENUM enum_interfaces = 0;
  mdToken  interfaces[8];
  ULONG    count_of_interfaces;
  HRESULT  hr;

  do {
    hr = md_import2_interface_->EnumInterfaceImpls(&enum_interfaces,
                                                   typedef_token_, interfaces,
                                                   _countof(interfaces),
                                                   &count_of_interfaces);
    CHECK_API_RESULT(hr, EnumInterfaceImpls);
    for (ULONG i = 0; i < count_of_interfaces; ++i) {
      wstring               interface_name = import_scope_.resolve_type_token(
                                                      interfaces[i],
                                                      generic_type_parameters_,
                                                      /*replaces_dots=*/true);
      auto                  back_tick = interface_name.find(L'`');
      a_cpp_cli_feature_set supported_features = 
                     import_scope_.containing_assembly().supported_features();

      if (back_tick != wstring::npos && 
          (supported_features & cpp_cli_generic_types) == 0) {
        /* Generics are not supported yet. */
        continue;
      }  /* if */
      if (is_first_base_class_processed) {
        buffer << ", ";
      } else {
        buffer << " : ";
        is_first_base_class_processed = true;
      }  /* if */
      buffer << interface_name;
    }  /* for */
  } while (count_of_interfaces > 0);
  md_import2_interface_->CloseEnum(enum_interfaces);
}  /* a_type_definition::process_interfaces */


bool a_type_definition::is_accessible(an_accessibility accessibility)
/*
Returns true if this is accessible via metadata import.
*/
{
  bool                  result = false;
  a_cpp_cli_feature_set supported_features = 
                      import_scope_.containing_assembly().supported_features();

  switch (accessibility) {
    case mdPrivateScope:
      result = false;
      break;
    case mdPrivate:
    case mdFamANDAssem:
    case mdAssem:
      if ((supported_features & cpp_cli_as_friend_assembly) != 0) {
        result = true;
      } else {
        result = false;
      }  /* if */
      break;
    case mdFamily:
    case mdFamORAssem:
    case mdPublic:
      result = true;
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  return result;
}  /* a_type_definition::is_accessible */


bool a_type_definition::is_nested_type_accessible(DWORD nested_type_attributes)
/*
Returns true if this nested class is accessible via metadata import.
*/
{
  bool                  result = false;
  a_cpp_cli_feature_set supported_features = 
                      import_scope_.containing_assembly().supported_features();

  check_assertion(IsTdNested(nested_type_attributes));
  switch (nested_type_attributes & tdVisibilityMask) {
    case tdNestedPrivate:
    case tdNestedFamANDAssem:
    case tdNestedAssembly:
      if ((supported_features & cpp_cli_as_friend_assembly) != 0) {
        result = true;
      } else {
        result = false;
      }  /* if */
      break;
    case tdNestedFamily:
    case tdNestedFamORAssem:
    case tdNestedPublic:
      result = true;
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  return result;
}  /* a_type_definition::is_nested_type_accessible */


wstring a_type_definition::accessibility_as_wstring(
                                                an_accessibility accessibility)
/*
Return the appropriate string for the specified accessibility.
*/
{
  wstring               result;
  a_cpp_cli_feature_set supported_features =
                      import_scope_.containing_assembly().supported_features();

  switch (accessibility) {
    case mdPrivateScope:
      /* We should not be emitting this accessibility. */
      unexpected_condition();
      break;
    case mdPrivate:
      if ((supported_features & cpp_cli_as_friend_assembly) != 0) {
        result = L"private";
      } else {
        unexpected_condition();
      }  /* if */
      break;
    case mdFamANDAssem:
      if ((supported_features & cpp_cli_as_friend_assembly) != 0) {
        result = L"protected";
      } else {
        unexpected_condition();
      }  /* if */
      break;
    case mdAssem:
      if ((supported_features & cpp_cli_as_friend_assembly) != 0) {
        result = L"public";
      } else {
        unexpected_condition();
      }  /* if */
      break;
    case mdFamORAssem:
      if ((supported_features & cpp_cli_as_friend_assembly) != 0) {
        result = L"public";
      } else {
        result = L"protected";
      }  /* if */
      break;
    case mdFamily:
      result = L"protected";
      break;
    case mdPublic:
      result = L"public";
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  check_assertion(!result.empty());
  return result;
}  /* a_type_definition::accessibility_as_wstring */


wstring a_type_definition::nested_type_accessibility_as_wstring(
                                                 DWORD nested_type_attributes)
/*
Return the appropriate string for the specified accessibility.
*/
{
  wstring                result;
  a_cpp_cli_feature_set  supported_features;

  supported_features =
                      import_scope_.containing_assembly().supported_features();
  check_assertion(IsTdNested(nested_type_attributes));
  switch (nested_type_attributes & tdVisibilityMask) {
    case tdNestedPrivate:
      if ((supported_features & cpp_cli_as_friend_assembly) != 0) {
        result = L"private";
      } else {
        unexpected_condition();
      }  /* if */
      break;
    case tdNestedFamANDAssem:
      if ((supported_features & cpp_cli_as_friend_assembly) != 0) {
        result = L"protected";
      } else {
        unexpected_condition();
      }  /* if */
      break;
    case tdNestedAssembly:
      if ((supported_features & cpp_cli_as_friend_assembly) != 0) {
        result = L"public";
      } else {
        unexpected_condition();
      }  /* if */
      break;
    case tdNestedFamORAssem:
      if ((supported_features & cpp_cli_as_friend_assembly) != 0) {
        result = L"public";
      } else {
        result = L"protected";
      }  /* if */
      break;
    case tdNestedFamily:
      result = L"protected";
      break;
    case tdNestedPublic:
      result = L"public";
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  check_assertion(!result.empty());
  return result;
}  /* a_type_definition::accessibility_as_wstring */


void a_type_definition::import_one_member(ostringstream &buffer,
                                          mdTypeDef     member_token,
                                          bool          accept_property_method,
                                          bool          accept_event_method)
/* 
Import a single member of a type.
*/
{
  bool                  is_constructor = false, emit_declaration = true;
  bool                  is_method, skip_member = false;
  HRESULT               hr;
  ULONG                 characters_in_name, bytes_in_signature;
  ULONG                 characters_in_constant;
  DWORD                 member_attributes, constant_type;
  PCCOR_SIGNATURE       signature;
  UVCP_CONSTANT         constant_value;
  wstring               member_name;
  wostringstream        declaration;
  unique_ptr<WCHAR[]>   name_buffer;
  an_accessibility      accessibility;
  a_cpp_cli_feature_set supported_features = 
                      import_scope_.containing_assembly().supported_features();

  hr = md_import2_interface_->GetMemberProps(
                             member_token, /*pClass=*/nullptr, 
                             /*szMember=*/nullptr, /*cchMember=*/0,
                             &characters_in_name, /*pdwAttr=*/nullptr, 
                             /*ppvSigBlob=*/nullptr, /*pcbSigBlob=*/nullptr, 
                             /*pulCodeRVA=*/nullptr, /*pdwImplFlags=*/nullptr,
                             /*pdwCPlusTypeFlag=*/nullptr, /*ppValue=*/nullptr,
                             /*pcchValue=*/nullptr);
  CHECK_API_RESULT(hr, GetMemberProps);
  name_buffer.reset(new WCHAR[characters_in_name]);
  hr = md_import2_interface_->GetMemberProps(member_token, /*pClass=*/nullptr,
                                             name_buffer.get(),
                                             characters_in_name,
                                             &characters_in_name, 
                                             &member_attributes, &signature,
                                             &bytes_in_signature,
                                             /*pdwImplFlags=*/nullptr, 
                                             /*pdwCPlusTypeFlag=*/nullptr, 
                                             &constant_type, &constant_value,
                                             &characters_in_constant);
  CHECK_API_RESULT(hr, GetMemberProps);
  /* Get the accessibility.  Skip those methods that are not accessible. */
  accessibility = get_accessibility(member_attributes);
  skip_member = !is_accessible(accessibility);
  member_name = name_buffer.get();
  /* Handle special members methods such as constructors (both instance and 
     class) and property accessor methods. */
  if (IsMdInstanceInitializerW(member_attributes, member_name.c_str()) ||
      IsMdClassConstructorW(member_attributes, member_name.c_str())) {
    member_name = type_name_;
    is_constructor = true;
  } else if (IsMdSpecialName(member_attributes) && !skip_member) {
    skip_member = true;
    
    /* Property accessor methods are processed as part of the property itself.
       Skip the property accessor methods unless accept_property_method is 
       true.  The same applies for event methods. */
    if (accept_property_method) {
      if (member_name.find(L"set_") == 0) {
        skip_member = false;
        member_name = L"set";
      } else if (member_name.find(L"get_") == 0) {
        skip_member = false;
        member_name = L"get";
      }  /* if */
    } else if (accept_event_method) {
      if (member_name.find(L"add_") == 0) {
        skip_member = false;
        member_name = L"add";
      } else if (member_name.find(L"remove_") == 0) {
        skip_member = false;
        member_name = L"remove";
      }  /* if */
    }  /* if */
  }  /* if  */
  if (!skip_member) {
    /* Decode the signature and create the appropriate declaration.  This 
       can either be a method or a field. */
    a_generic_parameter_list generic_method_parameters;
    a_signature_decoder      decoder(import_scope_, signature, 
                                     bytes_in_signature,
                                     generic_type_parameters_,
                                     generic_method_parameters);

    declaration << accessibility_as_wstring(accessibility) << L": ";
    if ((supported_features & cpp_cli_declspec_member_info) != 0) {
      declaration << L"__declspec(member_info(";
      declaration << L"0x" << setw(8) << setfill(L'0') << hex << member_token;
      declaration << L")) ";
    }  /* if */
    is_method = TypeFromToken(member_token) == mdtMethodDef;
    if (is_method) {
      /* If this is a generic method and we don't want and/or support generic
         methods then we should not emit this declaration. */
      generic_method_parameters =
                            import_scope_.get_generic_parameters(member_token);
      if (!generic_method_parameters.empty() && 
          (supported_features & cpp_cli_generic_methods) == 0) {
        emit_declaration = false;
      }  /* if */
      /* Emit the storage class. */
      if (IsMdStatic(member_attributes)) {
        declaration << L"static ";
      } else if (IsMdVirtual(member_attributes)) {
        declaration << L"virtual ";
      }  /* if */
      declaration << decoder.decode_method_signature(
                       member_name, is_constructor, /*is_for_property=*/false);
      if (IsMdFinal(member_attributes)) {
        declaration << L" sealed";
      }  /* if */
      if (IsMdNewSlot(member_attributes)) {
        declaration << L" new";
      } else if (IsMdVirtual(member_attributes)) {
        declaration << L" override";
      }  /* if */
    } else {
      /* A field. */
      wstring field_type;

      /* Emit the storage class.  "literal" implies "static". */
      if (IsFdStatic(member_attributes) && !IsFdLiteral(member_attributes)) {
        declaration << L"static ";
      }  /* if */
      if (IsFdInitOnly(member_attributes)) {
        declaration << L"initonly ";
        emit_declaration = false;
      } else if (IsFdLiteral(member_attributes)) {
        declaration << L"literal ";
        emit_declaration = false;
      }  /* if */
      field_type = decoder.decode_field_signature();
      declaration << field_type << L' ' << member_name;
      if (IsFdLiteral(member_attributes)) {
        /* Note: we emit the value as a hexadecimal constant cast to the
           appropriate type.  This seems to work best for some corner cases. */
        a_constant_decoder decoder(constant_type, constant_value,
                                   characters_in_constant);

        declaration << L" = static_cast<" << field_type << L">(";
        declaration << decoder.decode() << L')';
      }
    }  /* if */
    /* Check for any generic types in the declaration. */
    auto back_tick = declaration.str().find(L'`');
    if (back_tick != wstring::npos &&
        (supported_features & cpp_cli_generic_types) == 0) {
      /* Generics are not supported yet. */
      emit_declaration = false;
    }  /* if */
    if (emit_declaration) {
      buffer << declaration.str() << "; ";
#if DEBUG
      buffer << endl;
#endif /* DEBUG */
    }  /* if */
  }  /* if */
}  /* a_type_definition::import_one_member */


void a_type_definition::import_all_members(ostringstream &buffer)
/*
Import all the members associated with a single type.
*/
{
  HCORENUM  enum_members = 0;
  mdTypeDef members[64];
  ULONG     count_of_members;
  HRESULT   hr;

  do {
    hr = md_import2_interface_->EnumMembers(&enum_members, typedef_token_,
                                            members, _countof(members),
                                            &count_of_members);
    CHECK_API_RESULT(hr, EnumMembers);
    for (ULONG i = 0; i < count_of_members; ++i) {
      import_one_member(buffer, members[i], /*accept_property_method=*/false, 
                        /*accept_event_method=*/false);
    }  /* for */
  } while (count_of_members > 0);
  md_import2_interface_->CloseEnum(enum_members);
}  /* a_type_definition::import_all_members */


a_type_definition::a_property_method a_type_definition::import_property_method(
                                                      mdMethodDef method_token)
/*
Import the definition of a property accessor method.  
*/
{
  ULONG               characters_in_name;
  HRESULT             hr;
  unique_ptr<WCHAR[]> name_buffer;
  DWORD               attributes;

  hr = md_import2_interface_->GetMethodProps(
                             method_token, /*pClass=*/nullptr, 
                             /*szMethod=*/nullptr, /*cchMethod=*/0,
                             &characters_in_name, /*pdwAttr=*/nullptr, 
                             /*ppvSigBlob=*/nullptr, /*pcbSigBlob=*/nullptr, 
                             /*pulCodeRVA=*/nullptr, /*pdwImplFlags=*/nullptr);
  CHECK_API_RESULT(hr, GetMethodProps);
  name_buffer.reset(new WCHAR[characters_in_name]);
  hr = md_import2_interface_->GetMethodProps(
                             method_token, /*pClass=*/nullptr,
                             name_buffer.get(), characters_in_name,
                             &characters_in_name, &attributes, 
                             /*ppvSigBlob=*/nullptr, /*pcbSigBlob=*/nullptr, 
                             /*pulCodeRVA=*/nullptr, /*pdwImplFlags=*/nullptr);
  CHECK_API_RESULT(hr, GetMethodProps);
  return a_property_method(method_token, wstring(name_buffer.get()),
                           attributes);
}  /* a_type_definition::import_property_method */


void a_type_definition::import_one_property(ostringstream &buffer,
                                            mdProperty    property_token)
/*
Import the definition of single property - this includes both the property
itself and its associated accessor methods.
*/
{
  HRESULT             hr;
  ULONG               characters_in_name, bytes_in_signature;
  DWORD               property_attributes;
  PCCOR_SIGNATURE     signature;
  mdMethodDef         set_method_token;
  mdMethodDef         get_method_token;
  unique_ptr<WCHAR[]> name_buffer;
  an_accessibility    set_method_accessibility;
  an_accessibility    get_method_accessibility;
  an_accessibility    property_accessibility;
  a_property_method   set_method(mdMethodDefNil);
  a_property_method   get_method(mdMethodDefNil);

  hr = md_import2_interface_->GetPropertyProps(
                                property_token, /*pClass=*/nullptr, 
                                /*szProperty=*/nullptr, /*cchProperty=*/0, 
                                &characters_in_name, /*pdwPropFlags=*/nullptr, 
                                /*ppvSig=*/nullptr, /*pbSig=*/nullptr, 
                                /*pdwCPlusTypeFlag=*/nullptr, 
                                /*ppDefaultValue=*/nullptr, 
                                /*pcchDefaultValue=*/nullptr, 
                                /*pmdSetter=*/nullptr, 
                                /*pmdGetter=*/nullptr, 
                                /*rmdOtherMethod=*/nullptr, 
                                /*cMax=*/0, /*pcOtherMethod=*/nullptr);
  CHECK_API_RESULT(hr, EnumProperties);
  name_buffer.reset(new WCHAR[characters_in_name]);
  hr = md_import2_interface_->GetPropertyProps(
                                       property_token, /*pClass=*/nullptr,
                                       name_buffer.get(), 
                                       characters_in_name,
                                       &characters_in_name,
                                       &property_attributes,
                                       &signature, &bytes_in_signature, 
                                       /*pdwCPlusTypeFlag=*/nullptr, 
                                       /*ppDefaultValue=*/nullptr, 
                                       /*pcchDefaultValue=*/nullptr, 
                                       &set_method_token, &get_method_token,
                                       /*rmdOtherMethod=*/nullptr, 
                                       /*cMax=*/0, /*pcOtherMethod =*/nullptr);
  CHECK_API_RESULT(hr, EnumProperties);
  /* If there is a set method and/or a get method then import the necesssary
     information about the method. */
  if (!IsNilToken(set_method_token)) {
    set_method = import_property_method(set_method_token);
  }  /* if */
  if (!IsNilToken(get_method_token)) {
    get_method = import_property_method(get_method_token);
  }  /* if */
  /* Set the accessibility of the property itself.  The accessibility of the 
     property itself is the greater accessibility of its associated accessor 
     methods. */
  set_method_accessibility = set_method.accessibility();
  get_method_accessibility = get_method.accessibility();
  property_accessibility   = mdPrivateScope;
  if (get_method_accessibility != mdPrivateScope) {
    if (set_method_accessibility != mdPrivateScope) {
      if (get_method_accessibility == set_method_accessibility) {
        property_accessibility = get_method_accessibility;
      } else if (get_method_accessibility > set_method_accessibility) {
        property_accessibility = get_method_accessibility;
      } else {
        property_accessibility = set_method_accessibility;
      }  /* if */
    } else {
      property_accessibility = get_method_accessibility;
    }  /* if */
  } else if (set_method_accessibility != mdPrivateScope) {
    property_accessibility = set_method_accessibility;
  } else {
    unexpected_condition();
  }  /* if */
  if (is_accessible(property_accessibility)) {
    /* Now that we have everything we need emit the definition of the
       property. */
    ostringstream       declaration;
    a_signature_decoder decoder(import_scope_, signature, bytes_in_signature,
                                generic_type_parameters_,
                                no_generic_method_parameters);

    declaration << accessibility_as_wstring(property_accessibility) << ": ";
    if (IsMdStatic(property_attributes)) {
      declaration << "static ";
    }  /* if */
    declaration << "property ";
    declaration << decoder.decode_method_signature(
                                                 wstring(name_buffer.get()),
                                                 /*is_for_constructor=*/false, 
                                                 /*is_for_property=*/true);
    declaration << " { ";
#if DEBUG
    declaration << endl;
#endif /* DEBUG */
    /* Import the get and/or set method. */
    if (set_method.exists()) {
      import_one_member(declaration, set_method.token(), 
                        /*accept_property_method=*/true, 
                        /*accept_event_method=*/false);
    }  /* if */
    if (get_method.exists()) {
      import_one_member(declaration, get_method.token(), 
                        /*accept_property_method=*/true, 
                        /*accept_event_method=*/false);
    }  /* if */
    declaration << "} ";
#if DEBUG
    declaration << endl;
#endif /* DEBUG */
    buffer << declaration.str();
  }  /* if */
}  /* a_type_definition::import_one_property */


void a_type_definition::import_properties(ostringstream &buffer)
/*
Import all the properties associated with this type.
*/
{
  HCORENUM   enum_properties = 0;
  mdProperty properties[16];
  ULONG      count_of_properties;
  HRESULT    hr;

  do {
    hr = md_import2_interface_->EnumProperties(&enum_properties,
                                               typedef_token_,
                                               properties,
                                               _countof(properties),
                                               &count_of_properties);
    CHECK_API_RESULT(hr, EnumProperties);
    for (ULONG i = 0; i < count_of_properties; ++i) {
      import_one_property(buffer, properties[i]);
    }  /* for */
  } while (count_of_properties > 0);
  md_import2_interface_->CloseEnum(enum_properties);
}  /* a_type_definition::import_properties */


a_type_definition::an_event_method a_type_definition::import_event_method(
                                                      mdMethodDef method_token)
/*
Import the definition of an event method. 
*/
{
  ULONG               characters_in_name;
  HRESULT             hr;
  unique_ptr<WCHAR[]> name_buffer;
  DWORD               attributes;
  
  hr = md_import2_interface_->GetMethodProps(
                               method_token, /*pClass=*/nullptr, 
                               /*szMethod=*/nullptr, /*cchMethod=*/0,
                               &characters_in_name, /*pdwAttr=*/nullptr, 
                               /*ppvSigBlob=*/nullptr, /*pcbSigBlob=*/nullptr, 
                               /*pulCodeRVA=*/nullptr, 
                               /*pdwImplFlags=*/nullptr);
  CHECK_API_RESULT(hr, GetMethodProps);
  name_buffer.reset(new WCHAR[characters_in_name]);
  hr = md_import2_interface_->GetMethodProps(
                               method_token, /*pClass=*/nullptr,
                               name_buffer.get(), characters_in_name,
                               &characters_in_name, &attributes,
                               /*ppvSigBlob=*/nullptr, /*pcbSigBlob=*/nullptr, 
                               /*pulCodeRVA=*/nullptr, 
                               /*pdwImplFlags=*/nullptr);
  CHECK_API_RESULT(hr, GetMethodProps);
  return an_event_method(method_token, wstring(name_buffer.get()), attributes);
}  /* a_type_definition::import_event_method */


void a_type_definition::import_one_event(ostringstream  &buffer, 
                                         mdEvent        event_token)
/*
Import the definition of single event - this includes both the event
itself and its associated methods. 
*/
{
  ULONG               characters_in_name;
  HRESULT             hr;
  unique_ptr<WCHAR[]> name_buffer;
  DWORD               event_attributes;
  mdToken             event_type_token;
  mdMethodDef         add_method_token, remove_method_token;
  mdMethodDef         raise_method_token;
  an_event_method     add_method(mdMethodDefNil);
  an_event_method     remove_method(mdMethodDefNil);
  an_event_method     raise_method(mdMethodDefNil);
  an_accessibility    add_method_accessibility;
  an_accessibility    remove_method_accessibility;
  an_accessibility    event_accessibility;

  hr = md_import2_interface_->GetEventProps(
                               event_token, /*pClass=*/nullptr, 
                               /*szEvent=*/nullptr, /*cchEvent=*/0,
                               &characters_in_name, /*pdwEventFlags=*/nullptr, 
                               /*ptkEventType=*/nullptr, /*pmdAddOn=*/nullptr, 
                               /*pmdRemoveOn=*/nullptr, /*pmdFire=*/nullptr,
                               /*rmdOtherMethod=*/nullptr, /*cMax=*/0, 
                               /*pcOtherMethod=*/nullptr);
  CHECK_API_RESULT(hr, GetEventProps);
  name_buffer.reset(new WCHAR[characters_in_name]);
  hr = md_import2_interface_->GetEventProps(
                                     event_token, /*pClass=*/nullptr,
                                     name_buffer.get(), characters_in_name,
                                     &characters_in_name, &event_attributes,
                                     &event_type_token, &add_method_token,
                                     &remove_method_token, &raise_method_token,
                                     /*rmdOtherMethod=*/nullptr, /*cMax=*/0, 
                                     /*pcOtherMethod=*/nullptr);
  CHECK_API_RESULT(hr, GetEventProps);
  /* If there is an add/remove/raise method then import the necesssary
     information about the method. */
  if (!IsNilToken(add_method_token)) {
    add_method = import_event_method(add_method_token);
  }  /* if */
  if (!IsNilToken(remove_method_token)) {
    remove_method = import_event_method(remove_method_token);
  }  /* if */
  if (!IsNilToken(raise_method_token)) {
    raise_method = import_event_method(raise_method_token);
  }  /* if */
  /* The accessibility of the event itself is the greater accessibility of
     its associated methods. */
  add_method_accessibility = add_method.accessibility();
  remove_method_accessibility = remove_method.accessibility();
  event_accessibility = mdPrivateScope;
  if (add_method_accessibility != mdPrivateScope) {
    if (remove_method_accessibility != mdPrivateScope) {
      if (add_method_accessibility == remove_method_accessibility) {
        event_accessibility = add_method_accessibility;
      } else {
        unexpected_condition();
      }  /* if */
    } else {
      event_accessibility = add_method_accessibility;
    }  /* if */
  } else if (remove_method_accessibility != mdPrivateScope) {
    event_accessibility = remove_method_accessibility;
  } else {
    unexpected_condition();
  }  /* if */
  if (is_accessible(event_accessibility)) {
    /* Now that we have everything we need emit the definition of the 
       event. */
    ostringstream declaration;
    wstring event_type;

    declaration << accessibility_as_wstring(event_accessibility) << ": ";
    if (IsMdStatic(event_attributes)) {
      declaration << "static ";
    }  /* if */
    declaration << "event ";
    event_type = import_scope_.resolve_type_token(event_type_token,
                                                  generic_type_parameters_,
                                                  /*replaces_dots=*/true);
    declaration << event_type << "^ ";
    declaration << wstring(name_buffer.get()) << " { ";
#if DEBUG
    declaration << endl;
#endif /* DEBUG */
    if (add_method.exists()) {
      import_one_member(declaration, add_method.token(), 
                        /*accept_property_method=*/false, 
                        /*accept_event_method=*/true);
    }  /* if */
    if (remove_method.exists()) {
      import_one_member(declaration, remove_method.token(), 
                        /*accept_property_method=*/false, 
                        /*accept_event_method=*/true);
    }  /* if */
    if (raise_method.exists()) {
      import_one_member(declaration, raise_method.token(), 
                        /*accept_property_method=*/false, 
                        /*accept_event_method=*/true);
    }  /* if */
    declaration << "} ";
#if DEBUG
    declaration << endl;
#endif /* DEBUG */
    buffer << declaration.str();
  }  /* if */
}  /* a_type_definition::import_one_event */


void a_type_definition::import_events(ostringstream &buffer)
/*
Import all the events associated with this type.
*/
{
  HCORENUM enum_events = 0;
  mdEvent  events[8];
  ULONG    count_of_events;
  HRESULT  hr;

  do {
    hr = md_import2_interface_->EnumEvents(&enum_events, typedef_token_,
                                           events, _countof(events),
                                           &count_of_events);
    CHECK_API_RESULT(hr, EnumEvents);
    for (ULONG i = 0; i < count_of_events; ++i) {
      import_one_event(buffer, events[i]);
    }  /* for */
  } while (count_of_events > 0);
  md_import2_interface_->CloseEnum(enum_events);
}  /* a_type_definition::import_events */


void a_type_definition::import_nested_classes(ostringstream &buffer)
/*
Import all the nested classes enclosed by this type.
*/
{
  HCORENUM  enum_typedefs = 0;
  mdTypeDef typedefs[64];
  ULONG     count_of_typedefs;

  do {
    HRESULT hr = md_import2_interface_->EnumTypeDefs(&enum_typedefs, typedefs,
                                                     _countof(typedefs),
                                                     &count_of_typedefs);

    CHECK_API_RESULT(hr, EnumTypeDefs);
    for (ULONG i = 0; i < count_of_typedefs; ++i) {
      DWORD attributes;

      hr = md_import2_interface_->GetTypeDefProps(
                                     typedefs[i], /*szTypeDef=*/nullptr, 
                                     /*cchTypeDef=*/0, /*pchTypeDef=*/nullptr, 
                                     &attributes, /*ptkExtends=*/nullptr);
      if (IsTdNested(attributes)) {
        mdTypeDef enclosing_typedef;

        hr = md_import2_interface_->GetNestedClassProps(typedefs[i], 
                                                        &enclosing_typedef);
        if (enclosing_typedef == typedef_token_ && 
            is_nested_type_accessible(attributes)) {
          buffer << nested_type_accessibility_as_wstring(attributes) << ": ";
          import_scope_.import_one_type(buffer, typedefs[i], 
                                        /*at_top_level=*/false,
                                        /*want_definition=*/true, 
                                        /*class_body_only=*/false);
        }  /* if */
      }  /* if */
    }  /* for */
  } while (count_of_typedefs > 0);
  md_import2_interface_->CloseEnum(enum_typedefs);
}  /* a_type_definition::import_nested_classes */


void a_type_definition::import_definition(ostringstream &buffer)
/* 
Create the definition for the current type. 
*/
{
  a_cpp_cli_feature_set supported_features = 
                      import_scope_.containing_assembly().supported_features();

  process_extends(buffer);
  process_interfaces(buffer);
  buffer << " { ";
#if DEBUG
  buffer << endl;
#endif /* DEBUG */
  if ((supported_features & cpp_cli_nested_types) != 0) {
    import_nested_classes(buffer);
  }  /* if */
  import_all_members(buffer);
  if ((supported_features & cpp_cli_properties) != 0) {
    import_properties(buffer);
  }  /* if */
  if ((supported_features & cpp_cli_events) != 0) {
    import_events(buffer);
  }  /* if */
  buffer << "}; ";
#if DEBUG
  buffer << endl;
#endif /* DEBUG */
}  /* a_type_definition::import_definition */


wstring an_import_scope::top_level_kind_as_wstring(a_top_level_kind kind)
/*
Return the appropriate string for the specified top level kind.
*/
{
  wstring result;

  switch (kind) {
    case tlk_ref_class:
      result = L"ref class";
      break;
    case tlk_value_type:
      result = L"value class";
      break;
    case tlk_interface:
      result = L"interface class";
      break;
    case tlk_enumeration:
      result = L"enum class";
      break;
    case tlk_delegate:
      result = L"delegate";
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  return result;
}  /* a_type_definition::top_level_kind_as_wstring */


void an_import_scope::import_one_type(ostringstream& buffer,
                                      mdTypeDef typedef_token,
                                      bool at_top_level,
                                      bool want_definition,
                                      bool class_body_only)
/*
Import a single type from an import scope and create either a declaration or
a definition for the type depending on want_definition.  Note, in some cases
(like enumerations) we always need to create a definition.  at_top_level
denotes  whether this declaration/definition is at file scope.  If so,
namespace scopes and top-level-visibility will be emitted and nested classes
will be suppressed. 

If class_body_only is true, the class head and the namespace scopes will be 
omitted.
*/
{
  ULONG                    characters_in_name;
  HRESULT                  hr;
  DWORD                    attributes;
  mdToken                  extends_token;
  unique_ptr<WCHAR[]>      name_buffer;
  WCHAR                    *full_type_name;
  a_generic_parameter_list generic_type_parameters;
  a_top_level_kind         kind;
  bool                     skip_type = false;
  a_cpp_cli_feature_set    supported_features = 
                                     containing_assembly_.supported_features();

  hr = md_import2_interface_->GetTypeDefProps(
                                        typedef_token, /*szTypeDef=*/nullptr, 
                                        /*cchTypeDef=*/0, &characters_in_name, 
                                        /*pdwTypeDefFlags=*/nullptr, 
                                        /*ptkExtends=*/nullptr);
  CHECK_API_RESULT(hr, GetTypeDefProps);
  name_buffer.reset(new WCHAR[characters_in_name]);
  hr = md_import2_interface_->GetTypeDefProps(typedef_token, name_buffer.get(),
                                              characters_in_name,
                                              &characters_in_name, &attributes,
                                              &extends_token);
  CHECK_API_RESULT(hr, GetTypeDefProps);
  generic_type_parameters = get_generic_parameters(typedef_token);
  full_type_name = name_buffer.get();
  /* Check whether this type should be emitted based on the supported 
     features. */
  if (IsTdNested(attributes)) {
    if (at_top_level) {
      /* Do not emit nested types at top level scopes.  Nested types are 
         emitted in their enclosing type. */
      skip_type = true;
    }  /* if */
  } else if (!IsTdPublic(attributes) &&
             (supported_features & cpp_cli_as_friend_assembly) == 0) {
    /* Skip this type if private types are not accessible. */
    skip_type = true;
  } else if (!generic_type_parameters.empty() &&
             (supported_features & cpp_cli_generic_types) == 0) {
    /* Skip this type if generic types are not supported. */
    skip_type = true;
  } else if (full_type_name[0] == L'<' &&
      (supported_features & cpp_cli_implementation_details) == 0) {
    /* Skip this type if private implemention details are not supported. */
    check_assertion(
        wcsncmp(full_type_name, L"<CrtImplementationDetails>",
                _countof(L"<CrtImplementationDetails>") - 1) == 0 ||
        wcsncmp(full_type_name, L"<CppImplementationDetails>",
                _countof(L"<CppImplementationDetails>") - 1) == 0 ||
        wcsncmp(full_type_name, L"<PrivateImplementationDetails>",
                _countof(L"<PrivateImplementationDetails>") - 1) == 0);
    skip_type = true;
  } else if (wcscmp(full_type_name, L"_GUID") == 0) {
    /* _GUID is a built-in type in Microsoft mode.  Skip it. */
    skip_type = true;
  }  /* if */
  /* Classify the type - ref class, value class, interface etc. */
  kind = classify_type(attributes, generic_type_parameters, extends_token);
  /* Check whether the top level kind is supported. */
  switch (kind) {
    case tlk_ref_class:
      if ((supported_features & cpp_cli_ref_classes) == 0) {
        skip_type = true;
      }  /* if */
      break;
    case tlk_value_type:
      if ((supported_features & cpp_cli_value_types) == 0) {
        skip_type = true;
      }  /* if */
      break;
    case tlk_interface:
      if ((supported_features & cpp_cli_interfaces) == 0) {
        skip_type = true;
      }  /* if */
      break;
    case tlk_enumeration:
      if ((supported_features & cpp_cli_enumerations) == 0) {
        skip_type = true;
      }  /* if */
      break;
    case tlk_delegate:
      if ((supported_features & cpp_cli_delegates) == 0) {
        skip_type = true;
      } else if (!want_definition) {
        /* If no definition is required, treat the delegate as a ref class
           since "delegate ..." is always a definition.  Doing so avoids
           declaration ordering problems. */
        kind = tlk_ref_class;
      }  /* if */
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  if (!skip_type) {
    const WCHAR *last_dot;
    wstring     type_name;
    wstring     namespace_name;

    /* If this is a generic type then we need remove the trailing number of
       generic parameters which is encoded at the end of the name. */
    if (WCHAR* back_tick = wcsrchr(full_type_name, L'`')) {
      *back_tick = L'\0';
      last_dot = back_tick - 1;
    } else {
      last_dot = &full_type_name[characters_in_name - 2];
    }  /* if */
    /* Split the full type name into a namespace and a type-name. */
    for ( ; last_dot != full_type_name; --last_dot) {
      if (*last_dot == L'.') {
        break;
      }  /* if */
    }  /* for */
    if (last_dot == full_type_name) {
      type_name = full_type_name;
    } else {
      namespace_name = wstring(full_type_name, last_dot);
      type_name = wstring(last_dot + 1);
    }  /* if */
    if (type_name.find(L'<') != wstring::npos) {
      /* The type name contains angle brackets.  This is a template 
         specialization.  For example, "Foo<int>."  Use __identifier to emit
         the type name. */
      type_name = wstring(L"__identifier(\"") + type_name + wstring(L"\")");
    }  /* if */
    /* Emit the namespace scopes and class head if required. */
    if (!class_body_only) {
      if (at_top_level) {
        /* Make sure that the correct namespace scopes are opened. */
        if (!namespace_name.empty()) {
          open_namespace_scopes(buffer, namespace_name);
        } else {
          close_all_namespace_scopes(buffer);
        }  /* if */
      }  /* if */
      /* First if this is a generic class we need to emit the generic
         header. */
      if (!generic_type_parameters.empty()) {
        buffer << "generic<";
        for (ULONG i = 0; i < generic_type_parameters.size(); ++i) {
          if (i > 0) {
            buffer << ", ";
          }  /* if */
          buffer << "typename " << generic_type_parameters[i].name();
        }  /* for */
        buffer << "> ";
      }  /* if */
      /* Emit the assembly level visibility - either public or private. */
      if (at_top_level && want_definition) {
        if (IsTdPublic(attributes)) {
          buffer << "public ";
        } else {
          buffer << "private ";
        }  /* if */
      }  /* if */
      /* Emit the tokens that represent the kind. */
      buffer << top_level_kind_as_wstring(kind) << ' ';
      /* Emit the assembly_info declspec. */
      if ((supported_features & cpp_cli_declspec_assemby_info) != 0) {
        buffer << "__declspec(assembly_info(";
        buffer << containing_assembly_.assembly_index();
        buffer << ", 0x" << setw(8) << setfill('0') << hex << typedef_token;
        buffer << ")) ";
      }  /* if */
    } else if (kind == tlk_delegate) {
      /* Even when class_body_only is TRUE, the context-sensitive keyword
         "delegate" is needed so that a delegate class definition can be
         easily distinguished from a more traditional (managed) class
         definition. */
      buffer << "delegate ";
    }  /* if  */
    if (kind == tlk_delegate) {
      import_delegate_definition(buffer, typedef_token, type_name);
    } else if (kind == tlk_enumeration) {
      /* At the moment we can't forward declare a C++/CLI enumeration so we
         need to import (and emit) the full definition. */
      import_enum_definition(buffer, typedef_token, type_name);
    } else {
      bool define_all_types = 
                          (supported_features & cpp_cli_define_all_types) != 0;

      /* Emit the name of the type. */
      if (!class_body_only) buffer << type_name;
      if (want_definition || define_all_types) {
        a_type_definition type_definition(typedef_token, type_name, 
                                          extends_token,
                                          generic_type_parameters, 
                                          *this, kind);

        type_definition.import_definition(buffer);
      } else {
        buffer << "; "; 
#if DEBUG
        buffer << endl;
#endif /* DEBUG */
      }  /* if */
      if (at_top_level && want_definition && !class_body_only) {
        close_all_namespace_scopes(buffer);
      }  /* if */
    }  /* if */
    if (!want_definition && !IsTdNested(attributes)) {
      /* If this is for a declaration we need to remember the mapping from 
         the def-token to the name as this will make it easier to find any 
         future references to this token. */
      map_of_tokens_to_names_.insert(make_pair(typedef_token,
                                               wstring(full_type_name)));
    }  /* if */
  }  /* if */
}  /* an_import_scope::import_one_type */


void a_signature_decoder::decode_generic_arguments(wostringstream &buffer)
/*
Decode the generic arguments associated with a type.  Note, it is the caller's
responsiblity to ensure that there is at least one generic argument.
*/
{
  BYTE count_of_generic_arguments = read_one_byte();

  check_assertion(count_of_generic_arguments > 0);
  buffer << L'<';
  for (BYTE i = 0; i < count_of_generic_arguments; ++i) {
    if (i > 0) {
      buffer << L", ";
    }  /* if */
    buffer << decode_type();
  }  /* for */
  buffer << L'>';
}  /* a_signature_decoder::decode_generic_arguments */


wstring a_signature_decoder::decode_type()
/*
Decode a type signature and return it as a std::wstring.
*/
{
  wostringstream buffer;
  bool           is_generic;
  CorElementType element_type = get_element_type();

  if (element_type == ELEMENT_TYPE_GENERICINST) {
    is_generic = true;
    element_type = get_element_type();
  } else {
    is_generic = false;
  }  /* if  */
  switch (element_type) {
    case ELEMENT_TYPE_VOID:
      buffer << L"void";
      break;
    case ELEMENT_TYPE_BOOLEAN:
      buffer << L"bool";
      break;
    case ELEMENT_TYPE_CHAR:
      buffer << L"wchar_t";
      break;
    case ELEMENT_TYPE_I1:
      buffer << L"signed char";
      break;
    case ELEMENT_TYPE_U1:
      buffer << L"unsigned char";
      break;
    case ELEMENT_TYPE_I2:
      buffer << L"short";
      break;
    case ELEMENT_TYPE_U2:
      buffer << L"unsigned short";
      break;
    case ELEMENT_TYPE_I4:
      buffer << L"int";
      break;
    case ELEMENT_TYPE_U4:
      buffer << L"unsigned int";
      break;
    case ELEMENT_TYPE_I8:
      buffer << L"long long";
      break;
    case ELEMENT_TYPE_U8:
      buffer << L"unsigned long long";
      break;
    case ELEMENT_TYPE_R4:
      buffer << L"float";
      break;
    case ELEMENT_TYPE_R8:
      buffer << L"double";
      break;
    case ELEMENT_TYPE_STRING:
      buffer << L"System::String^";
      break;
    case ELEMENT_TYPE_PTR:
      /* Decode the pointee type and append a '*'. */
      buffer << decode_type() << L'*';
      break;
    case ELEMENT_TYPE_BYREF:
      /* Decode the referenced type and append a "%". */
      buffer << decode_type() << L'%';
      break;
    case ELEMENT_TYPE_VALUETYPE:
    case ELEMENT_TYPE_CLASS:
      buffer << import_scope_.resolve_type_token(read_token(),
                                                 generic_type_parameters_, 
                                                 /*replaces_dots=*/true);
      break;
    case ELEMENT_TYPE_VAR:
      buffer << generic_type_parameters_[read_one_byte()].name();
      break;
    case ELEMENT_TYPE_TYPEDBYREF:
      buffer << L"System::TypedReference";
      break;
    case ELEMENT_TYPE_I:
      buffer << L"System::IntPtr";
      break;
    case ELEMENT_TYPE_U:
      buffer << L"System::UIntPtr";
      break;
    case ELEMENT_TYPE_OBJECT:
      buffer << L"System::Object^";
      break;
    case ELEMENT_TYPE_SZARRAY:
      /* Decode the element type and wrap it in our array syntax. */
      buffer << L"cli::array<" << decode_type() << L">^";
      break;
    case ELEMENT_TYPE_ARRAY:
      {
        ULONG rank, num_of_sizes, num_of_lower_bounds;

        buffer << L"cli::array<" << decode_type() << ", ";
        /* Get the Rank of the array. */
        rank = read_four_bytes();
        buffer << rank;
        num_of_sizes = read_four_bytes();
        for (ULONG i = 0; i < num_of_sizes; ++i) {
          /* We don't need the sizes.  They don't affect the typename.
             However, we need to consume these bytes in the signature blob. */
          (void)read_four_bytes();
        }  /* if */
        num_of_lower_bounds = read_four_bytes();
        for (ULONG i = 0; i < num_of_lower_bounds; ++i) {
          /* We don't need the lower bounds.  They don't affect the typename.  
             However, we need to consumer these bytes in the signature blob. */
          (void)read_four_bytes();
        }  /* if */
        buffer << L">^";
        break;
      }  /* ELEMENT_TYPE_ARRAY */
    case ELEMENT_TYPE_MVAR:
      buffer << generic_method_parameters_[read_one_byte()].name();
      break;
    case ELEMENT_TYPE_CMOD_REQD:
      buffer << "/* CMOD_REQD ";
      buffer << import_scope_.resolve_type_token(read_token(), 
                                                 generic_type_parameters_, 
                                                 /*replaces_dots=*/true);
      buffer << " */ ";
      buffer << decode_type();
      break;
    case ELEMENT_TYPE_CMOD_OPT:
      buffer << "/* CMOD_OPT ";
      buffer << import_scope_.resolve_type_token(read_token(), 
                                                 generic_type_parameters_, 
                                                 /*replaces_dots=*/true);
      buffer << " */ ";
      buffer << decode_type();
      break;
    case ELEMENT_TYPE_INTERNAL:
#if DEBUG
      buffer << "/* TYPE_INTERNAL " << " */ ";
#endif /* DEBUG */
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  if (is_generic) {
    decode_generic_arguments(buffer);
  }  /* if */
  if (element_type == ELEMENT_TYPE_CLASS) {
    buffer << L'^';
  }  /* if */
  return buffer.str();
}  /* a_signature_decoder::decode_type */


/*
The class that handles the reading of metadata.
*/
class a_metadata_reader {
private:
  typedef map<wstring, an_assembly> an_assembly_set;

public:
  a_metadata_reader()
    : alink_handle_(NULL), alink_interface_(NULL), clr_metahost_policy_(NULL),
      clr_runtime_info_(NULL), md_dispenser_interface_(NULL), 
      md_emit2_interface_(NULL), md_import2_interface_(NULL)
  {
    initialized_ = initialize();
  }  /* constructor */


  bool is_initialized() const
  {
    return initialized_;
  }  /* is_initialized */


  an_assembly_index import_assembly(char                  *assembly_full_name,
                                    a_cpp_cli_feature_set supported_features,
                                    bool                  *is_duplicated);
  void import_all_types(ostringstream     &buffer,
                        an_assembly_index assembly_index);
  void import_class_definition(ostringstream     &buffer,
                               an_assembly_index assembly_index,
                               a_cpp_cli_token   typedef_token,
                               bool              class_body_only);
  bool initialize();
  void cleanup();
  bool trans_unit_init(char *trans_unit_file_name);
  void trans_unit_wrapup();

#if WRITE_CPPCLI_PORTABLE_ASSEMBLIES
  IMetaDataImport2 *import_interface(an_assembly_index assembly_index)
  /*
  The code that writes portable assemblies needs access to the assembly's
  import_interface in order to enumerate the typedefs in the assembly.
  */
  {
    auto iter = assemblies_.begin();
    auto end = assemblies_.end();

    for ( ; iter != end; ++iter) {
      if (iter->second.assembly_index() == assembly_index) {
        break;
      }  /* if */
    }  /* for */
    return iter->second.find_scope().import_interface();
  }  /* a_metadata_reader::import_interface */
#endif /* WRITE_CPPCLI_PORTABLE_ASSEMBLIES */

private:
  bool init_clr_host();
  bool init_clr_runtime_info();
  bool init_alink();
  bool init_metadata_interfaces();

private:
  an_assembly_set
                assemblies_;
                        /* The assemblies that we have imported. */
  HMODULE       alink_handle_;
                        /* The handle for alink.dll. */
  IALink        *alink_interface_;
                        /* The interface to the functionality provided by
                           alink.dll. */
  ICLRMetaHostPolicy
                *clr_metahost_policy_;
                        /* The ICLRMetaHostPolicy interface. */
  ICLRRuntimeInfo
                *clr_runtime_info_;
                        /* The ICLRRuntimeInfo interface. */
  IMetaDataDispenserEx
                *md_dispenser_interface_;
                        /* The IMetaDataDispenserEx interface. */
  IMetaDataEmit2
                *md_emit2_interface_;
                        /* The IMetaDataEmit2 interface. */
  IMetaDataImport2
                *md_import2_interface_;
                        /* The IMetaDataImport2 interface. */
  mdFile        file_token_;
                        /* The token for current translation unit. */
  bool          initialized_;
                        /* True if the metadata reader was initialized 
                           properly. */
};  /* a_metadata_reader */

static a_metadata_reader *metadata_reader;

bool a_metadata_reader::init_clr_host()
/*
Initialize the clr host.
*/
{
  HRESULT   hr;

  hr = CLRCreateInstance(CLSID_CLRMetaHostPolicy, IID_ICLRMetaHostPolicy, 
                         reinterpret_cast<LPVOID*>(&clr_metahost_policy_));
  CHECK_API_RESULT(hr, CLRCreateInstance);
  return SUCCEEDED(hr);
}  /* a_metadata_reader::init_clr_host */


bool a_metadata_reader::init_clr_runtime_info()
/*
Initialize the clr runtime info.
*/
{
  HRESULT   hr = E_FAIL;
  DWORD     dwConfigFlags = 0;

  check_assertion(clr_metahost_policy_);
  hr = clr_metahost_policy_->GetRequestedRuntime(
                        METAHOST_POLICY_USE_PROCESS_IMAGE_PATH,
                        /*pwzBinary=*/NULL, /*pCfgStream=*/NULL, 
                        /*pwzVersion=*/NULL, /*pcchVersion=*/NULL, 
                        /*pwzImageVersion=*/NULL, /*pcchImageVersion=*/NULL,
                        /*pdwConfigFlags=*/NULL, IID_ICLRRuntimeInfo, 
                        reinterpret_cast<LPVOID*>(&clr_runtime_info_));
  if (FAILED(hr) || clr_runtime_info_ == NULL) {
    const int SIZE = 128;
    WCHAR     version_buffer[SIZE];
    DWORD     version_size = SIZE;

    wcscpy_s(version_buffer, version_size, CLR_FALLBACK_VERSION);
    hr = clr_metahost_policy_->GetRequestedRuntime(
                          static_cast<METAHOST_POLICY_FLAGS>(
                            METAHOST_POLICY_USE_PROCESS_IMAGE_PATH | 
                            METAHOST_POLICY_APPLY_UPGRADE_POLICY),
                          /*pwzBinary=*/NULL, /*pCfgStream=*/NULL, 
                          version_buffer, &version_size, 
                          /*pwzImageVersion=*/NULL, /*pcchImageVersion=*/NULL,
                          /*pdwConfigFlags=*/NULL, IID_ICLRRuntimeInfo, 
                          reinterpret_cast<LPVOID*>(&clr_runtime_info_));
    CHECK_API_RESULT(hr, GetRequestedRuntime);
  }  /* if */
  return SUCCEEDED(hr);
}  /* a_metadata_reader::init_clr_runtime_info */


bool a_metadata_reader::init_alink()
/*
Initialize the alink interface handle. 
*/
{
  HRESULT            hr;

  check_assertion(clr_runtime_info_);
  hr = clr_runtime_info_->LoadLibrary(L"alink.dll", &alink_handle_);
  CHECK_API_RESULT(hr, LoadLibrary);
  if (alink_handle_) {
    typedef HRESULT (WINAPI* a_create_alink_ptr)(REFIID, IUnknown**);
    a_create_alink_ptr create_alink_ptr;

    create_alink_ptr = reinterpret_cast<a_create_alink_ptr>(GetProcAddress(
                                                               alink_handle_, 
                                                               "CreateALink"));
    check_assertion(create_alink_ptr);
    hr = (*create_alink_ptr)(IID_IALink,
                             reinterpret_cast<IUnknown**>(&alink_interface_));
    CHECK_API_RESULT(hr, CreateALink);
  } else {
    unexpected_condition_str("ERROR: failed to find/load alink.dll");
  }  /* if */
  return (alink_interface_ != nullptr);
}  /* a_metadata_reader::init_alink */


bool a_metadata_reader::init_metadata_interfaces()
/*
Initialize various metadata interfaces.
*/
{
  HRESULT hr;

  check_assertion(clr_runtime_info_);
  check_assertion(alink_interface_);
  hr = clr_runtime_info_->GetInterface(CLSID_CorMetaDataDispenser, 
                                       IID_IMetaDataDispenserEx, 
                                       reinterpret_cast<LPVOID*>(
                                                    &md_dispenser_interface_));
  CHECK_API_RESULT(hr, GetInterface);
  if (SUCCEEDED(hr)) {
    hr = md_dispenser_interface_->DefineScope(CLSID_CLR_v2_MetaData, 0,
                                              IID_IMetaDataEmit2,
                                              reinterpret_cast<IUnknown **>(
                                                        &md_emit2_interface_));
    CHECK_API_RESULT(hr, DefineScope);
    if (SUCCEEDED(hr)) {
      hr = md_emit2_interface_->QueryInterface(IID_IMetaDataImport2,
                                               reinterpret_cast<void**>(
                                                      &md_import2_interface_));
      CHECK_API_RESULT(hr, QueryInterface);
      if (SUCCEEDED(hr)) {
        hr = alink_interface_->Init(md_dispenser_interface_, nullptr);
        CHECK_API_RESULT(hr, Init);
      }  /* if */
    }  /* if */
  }  /* if */
  return SUCCEEDED(hr);
}  /* a_metadata_reader::init_metadata_interfaces */


bool a_metadata_reader::initialize()
/*
Initialize the metadata reader - this means initializing clr host/runtime, 
loading and initializing alink.dll, and creating the various metadata reading 
interfaces.
*/
{
  bool result;

  if (init_clr_host() && init_clr_runtime_info() && init_alink() && 
      init_metadata_interfaces()) {
    result = true;
  } else {
    result = false;
    cleanup();
  }  /* if */
  return result;
}  /* a_metadata_reader::initialize */


bool a_metadata_reader::trans_unit_init(char *trans_unit_file_name)
/*
Do per-translation unit initialization.  This resets the assembly
index, sets the name of the translation unit, and retrieves a file token for
the translation unit.
*/
{
  HRESULT hr;
  wstring input_file(char_string_to_wstring(trans_unit_file_name));

  check_assertion(is_initialized());
  an_assembly::reset_index();
  hr = alink_interface_->AddFile(AssemblyIsUBM, input_file.c_str(),
                                 ffContainsMetaData, md_emit2_interface_,
                                 &file_token_);
  CHECK_API_RESULT(hr, AddFile);
  return SUCCEEDED(hr);
}  /* a_metadata_reader::trans_unit_init */


void a_metadata_reader::trans_unit_wrapup()
/*
Perform clean up for the translation unit.  This involves freeing
the all the assemblies that were imported and resetting the assembly
index.
*/
{
  if (alink_interface_ != nullptr) {
    HRESULT hr = alink_interface_->CloseAssembly(AssemblyIsUBM);
    CHECK_API_RESULT(hr, CloseAssembly);
  }  /* if */
  for_each(assemblies_.begin(), assemblies_.end(),
           [](an_assembly_set::value_type& element) {
             element.second.cleanup();
           });
  assemblies_.clear();
  an_assembly::reset_index();
}  /* a_metadata_reader::trans_unit_wrapup */


void a_metadata_reader::cleanup()
/*
Clean up and tear down the interface that were created.
*/
{
  release_and_zero_out_helper(md_dispenser_interface_);
  release_and_zero_out_helper(md_emit2_interface_);
  release_and_zero_out_helper(md_import2_interface_);
  release_and_zero_out_helper(alink_interface_);
  release_and_zero_out_helper(clr_runtime_info_);
  release_and_zero_out_helper(clr_metahost_policy_);
}  /* a_metadata_reader::cleanup */


an_assembly_index a_metadata_reader::import_assembly(
                                     char                  *assembly_full_name,
                                     a_cpp_cli_feature_set supported_features,
                                     bool                  *is_duplicated)
/*
Import a single assembly.  assembly_full_name contains the full path to the 
assembly to be processed.  If this assembly has been processed, 
*is_duplicated is set to TRUE and the previous assembly index is returned.
*/
{
  an_assembly_index result;
  wstring           full_assembly_name(char_string_to_wstring(
                                                          assembly_full_name));
  /* Determine whether this assembly has been imported.  If so, don't process
     it again. */
  auto              iter = assemblies_.find(full_assembly_name);

  check_assertion(is_duplicated);
  *is_duplicated = false;
  if (iter != assemblies_.end()) {
    result = iter->second.assembly_index();
    *is_duplicated = true;
  } else {
    auto assembly = assemblies_.insert(make_pair(
                                             full_assembly_name,
                                             an_assembly(full_assembly_name,
                                                         alink_interface_,
                                                         file_token_,
                                                         supported_features)));
    assembly.first->second.process();
    result = assembly.first->second.assembly_index();
  }  /* if */
  return result;
}  /* a_metadata_reader::import_assembly */


void a_metadata_reader::import_all_types(ostringstream     &buffer,
                                         an_assembly_index assembly_index)
/*
Import all the types defined in the specified assembly. 
*/
{
  auto iter = assemblies_.begin();
  auto end = assemblies_.end();

  for ( ; iter != end; ++iter) {
    if (iter->second.assembly_index() == assembly_index) {
      break;
    }  /* if */
  }  /* for */
  iter->second.find_scope().import_all_types(buffer);
}  /* a_metadata_reader::import_all_types */


void a_metadata_reader::import_class_definition(
                                             ostringstream     &buffer,
                                             an_assembly_index assembly_index,
                                             a_cpp_cli_token   typedef_token,
                                             bool              class_body_only)
/*
Import the definition of the class specified by the provided assembly index
and typedef token. 
*/
{
  auto iter = assemblies_.begin();
  auto end = assemblies_.end();

  for ( ; iter != end; ++iter) {
    if (iter->second.assembly_index() == assembly_index) {
      break;
    }  /* if */
  }  /* for */
  check_assertion(iter != end);
  iter->second.find_scope().import_one_type(buffer, typedef_token, 
                                            /*at_top_level=*/true,
                                            /*want_definition=*/true, 
                                            class_body_only);
}  /* a_metadata_reader::import_class_definition */

/*
Interface functions to the front end proper.
*/

static a_boolean ms_metadata_init_if_needed()
/* 
Helper function to initialize the metadata reader.
*/
{
  a_boolean result = TRUE;

  /* Reset the index counter. */
  if (metadata_reader == nullptr) {
    metadata_reader = new a_metadata_reader();
    if (!metadata_reader->is_initialized()) {
      metadata_reader->cleanup();
      delete metadata_reader;
      metadata_reader = nullptr;
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* ms_metadata_init_if_needed */

#if WRITE_CPPCLI_PORTABLE_ASSEMBLIES

static a_portable_assembly_table_entry
                *pa_table = nullptr;
                        /* A dynamically-allocated list of entries used to
                           refer to metadata tokens and their associated
                           strings. */

static uint32_t pa_table_entries = 0;
                        /* The number of allocated entries in pa_table. */

static a_text_buffer_ptr
		metadata_string_buffer = nullptr;
			/* Text buffer used by create_portable_assembly. */

static void create_portable_assembly(char               *assembly_name,
                                     an_assembly_index  index)
/*
Create a "portable assembly" for the assembly named by assembly_name, and
referred to by index.  A portable assembly contains all of the metadata
information contained by the assembly, but in string format so that it can
be used on non-Windows systems (for testing purposes).
*/
{
  a_portable_assembly_header      header;
  a_portable_assembly_table_entry *table;
  IMetaDataImport2                *import_interface;
  string                          pa_name(start_of_file_name(assembly_name));
  FILE                            *f_pa;
  uint32_t                        cur_entry_no = 0;
  HCORENUM                        enum_typedefs = 0;
  mdTypeDef                       typedefs[64];
  ULONG                           count_of_typedefs;
  a_text_buffer_ptr               buffer;
  sizeof_t                        size;
  uint32_t                        i;

  /* The portable assembly is written in the current directory (without
     regard to an existing file).  This is usually okay because the assemblies
     we're concerned with are typically in system directories. */
  f_pa = fopen_with_error((char *)pa_name.c_str(), "wb", OFF_NO_OPTIONS,
                          ec_portable_assembly);
  if (f_pa != nullptr) {
    check_assertion(metadata_reader != nullptr);
    check_assertion(metadata_reader->is_initialized());
    check_assertion(index != 0);
    import_interface = metadata_reader->import_interface(index);
    /* Write a dummy header to the file initially (will be overwritten at
       the end with the proper information). */
    clear_portable_assembly_header(&header);
    /* Write the header in ASCII so it's more portable.  Use a format string
       that will result in the same number of bytes being used when the
       header is later re-written (so the offsets will stay the same). */
    (void)fprintf(f_pa, PORTABLE_ASSEMBLY_HEADER_FORMAT,
                  header.magic, header.num_entries, header.table_offset);
    /* Get the string returned by import_all_types. */
    if (metadata_string_buffer == nullptr) {
      /* Allocate the buffer that will temporarily house the metadata string
         information. */
      metadata_string_buffer =
                alloc_text_buffer(METADATA_IMPORT_BUFFER_ALLOCATION_INCREMENT);
      expand_text_buffer(metadata_string_buffer, METADATA_IMPORT_BUFFER_SIZE);
    }  /* if */
    reset_text_buffer(metadata_string_buffer);
    buffer = metadata_string_buffer;
    size = buffer->allocated_size;
    import_all_types(index, buffer->buffer, &size);
    if (size <= buffer->allocated_size) {
      /* The buffer fits.  Mark the size that has been written. */
      buffer->size = size;
    } else {
      /* Expand the buffer */
      reset_text_buffer(buffer);
      expand_text_buffer(buffer, size);
      import_all_types(index, buffer->buffer, &size);
      check_assertion(size <= buffer->allocated_size);
      buffer->size = size;
    }  /* if */
    if (pa_table == nullptr) {
      /* 2780 entries are needed for mscorlib, so allocate enough so to handle
         that (most likely) case. */
      pa_table_entries = 3000;
      pa_table = (a_portable_assembly_table_entry *)alloc_resizable_buffer(
                          (sizeof_t)(pa_table_entries *
                                     sizeof(a_portable_assembly_table_entry)));
    }  /* if */
    /* The import_all_types string is pointed to by the first entry in the
       table.  Keep the offset and size information, then write the string
       to the portable assembly file. */
    pa_table[0].token = 0;
    pa_table[0].offset = ftell(f_pa);
    pa_table[0].size = buffer->size;
    (void)fwrite((a_stdio_arg)buffer->buffer, 1, buffer->size, f_pa);
    cur_entry_no++;
    /* For each typedef in the assembly, get its definition (in case we ever
       need it) and write it to the portable assembly. */
    do {
      HRESULT hr = import_interface->EnumTypeDefs(&enum_typedefs,
                                                  typedefs,
                                                  _countof(typedefs),
                                                  &count_of_typedefs);
      CHECK_API_RESULT(hr, EnumTypeDefs);
      check_assertion(count_of_typedefs <= _countof(typedefs));
      for (ULONG i = 0; i < count_of_typedefs; ++i) {
        check_assertion(typedefs[i] != 0);
        reset_text_buffer(metadata_string_buffer);
        buffer = metadata_string_buffer;
        size = buffer->allocated_size;
        import_class_definition(index, typedefs[i], buffer->buffer, &size);
        if (size <= buffer->allocated_size) {
          /* The buffer fits.  Mark the size that has been written. */
          buffer->size = size;
        } else {
          /* Expand the buffer */
          reset_text_buffer(buffer);
          expand_text_buffer(buffer, size);
          import_class_definition(index, typedefs[i], buffer->buffer, &size);
          check_assertion(size <= buffer->allocated_size);
          buffer->size = size;
        }  /* if */
        if (++cur_entry_no > pa_table_entries-1) {
          /* Double the size of the table if we've run out of space. */
          sizeof_t old_size = pa_table_entries *
                              sizeof(a_portable_assembly_table_entry);
          pa_table_entries *= 2;
          pa_table = (a_portable_assembly_table_entry *)realloc_buffer(
                                       (char *)pa_table, old_size, old_size*2);
        }  /* if */
        table = &pa_table[cur_entry_no];
        table->token = typedefs[i];
        table->offset = ftell(f_pa);
        table->size = buffer->size;
        (void)fwrite((a_stdio_arg)buffer->buffer, 1, buffer->size, f_pa);
      }  /* for */
    } while (count_of_typedefs > 0);
    import_interface->CloseEnum(enum_typedefs);
    /* Initialize the header now that we know the proper information. */
    header.magic = PORTABLE_ASSEMBLY_MAGIC_NUMBER;
    header.num_entries = cur_entry_no;
    header.table_offset = ftell(f_pa);
    /* Add a newline before the table contents. */
    (void)fputc('\n', f_pa);
    /* Write out the table contents. */
    for (i = 0; i < cur_entry_no; i++) {
      (void)fprintf(f_pa, PORTABLE_ASSEMBLY_HEADER_FORMAT,
                    pa_table[i].token, pa_table[i].offset, pa_table[i].size);
    }  /* for */
    /* Re-write the header with the proper information now that it is known. */
    (void)fseek(f_pa, 0L, SEEK_SET);
    (void)fprintf(f_pa, PORTABLE_ASSEMBLY_TABLE_FORMAT,
                  header.magic, header.num_entries, header.table_offset);
    (void)fclose(f_pa);
  }  /* if */
}  /* create_portable_assembly */

#endif /* WRITE_CPPCLI_PORTABLE_ASSEMBLIES */

EXTERN_C_IN_CPP_FILE
an_assembly_index import_metadata_file(
                                    char                  *assembly_full_name,
                                    a_cpp_cli_feature_set supported_features,
                                    a_boolean             *is_duplicated)
/*
Import the assembly and return an unique assembly index.  *is_duplicated is set
to true if this assembly have been imported before.  If so, the previous
assembly will be returned.
*/
{
  an_assembly_index result;
  bool              is_dup= false;

  check_assertion(is_duplicated);
  check_assertion(metadata_reader->is_initialized());
  *is_duplicated = FALSE;
  /* Attempt to import this assembly. */
  result = metadata_reader->import_assembly(assembly_full_name, 
                                            supported_features, &is_dup);
  *is_duplicated = is_dup ? TRUE : FALSE;
#if WRITE_CPPCLI_PORTABLE_ASSEMBLIES
  if (generate_portable_assemblies && result != 0) {
    /* Create a portable assembly. */
    create_portable_assembly(assembly_full_name, result);
  }  /* if */
#endif /* WRITE_CPPCLI_PORTABLE_ASSEMBLIES */
  return result;
}  /* import_metadata_file */


EXTERN_C_IN_CPP_FILE
void import_all_types(an_assembly_index assembly_index,
                      char              *buffer,
                      size_t            *buffer_size)
/*
Import all the types defined in the specified assembly.  *buffer_size describes
the allocated size of *buffer.  If there is enough space, the generated code
is returned in *buffer and the amount of buffer used is returned in 
*buffer_size.  Otherwise, *buffer is null terminated and *buffer_size contains
the required size.
*/
{
  string        str;
  ostringstream os;
  a_boolean     result = FALSE;

  check_assertion(metadata_reader != nullptr);
  check_assertion(metadata_reader->is_initialized());
  metadata_reader->import_all_types(os, assembly_index);
  str = os.str();
  /* '+1' for the NULL terminator. */
  if (str.size() + 1 <= *buffer_size) {
    /* The buffer fits.  Copy it. */
    strcpy_s(buffer, *buffer_size, str.c_str());
#if DEBUG && !defined(WANT_LINE_BREAKS)
    /* Replace newlines ('\n') with spaces.  The newlines are for debugging 
       purposes only -- it's easier to view the generated code in the debugger
       with newlines. */
    for (size_t i = 0; i < str.size(); ++i) {
      if (buffer[i] == '\n') {
        buffer[i] = ' ';
      }  /* if */
    }  /* for */
#endif /* DEBUG */
  } else if (*buffer_size > 0) {
    /* Not enough space.  Terminate the buffer. */
    *buffer = '\0';
  }  /* if */
  /* '+1' for the NULL terminator. */
  *buffer_size = str.size() + 1;
}  /* import_all_types */
                                     

EXTERN_C_IN_CPP_FILE
void import_class_definition(an_assembly_index assembly_index,
                             a_cpp_cli_token   typedef_token,
                             char              *buffer,
                             size_t            *buffer_size)
/*
Import the definition of the type specified by typedef_token.  The generated
code only contains the body of the class definiton, including the base classes 
list.  The namespace scopes and class head are omitted.  
*/
{
  string str;
  ostringstream os;

  check_assertion(metadata_reader != nullptr);
  check_assertion(metadata_reader->is_initialized());
  metadata_reader->import_class_definition(os, assembly_index, typedef_token, 
                                           /*class_body_only=*/true);
  str = os.str();
  /* '+1' for the NULL terminator. */
  if (str.size() + 1 <= *buffer_size) {
    strcpy_s(buffer, *buffer_size, str.c_str());
#if DEBUG && !defined(WANT_LINE_BREAKS)
    /* Replace newlines ('\n') with spaces.  The newlines are for debugging 
       purposes only -- it's easier to view the generated code in the debugger 
       with newlines. */
    for (size_t i = 0; i < str.size(); ++i) {
      if (buffer[i] == '\n') {
        buffer[i] = ' ';
      }  /* if */
    }  /* for */
#endif /* DEBUG */
  } else if (*buffer_size > 0) {
    *buffer = '\0';
  }  /* if */
  /* '+1' for the NULL terminator. */
  *buffer_size = str.size() + 1;
}  /* import_class_definition */


EXTERN_C_IN_CPP_FILE
void ms_metadata_trans_unit_init(char *trans_unit_file_name)
/*
Reset the metadata reader for reading metadata for the next translation unit.
*/
{
  if (ms_metadata_init_if_needed()) {
    (void)metadata_reader->trans_unit_init(trans_unit_file_name);
  }  /* if */
}  /* ms_metadata_trans_unit_init */


EXTERN_C_IN_CPP_FILE
void ms_metadata_trans_unit_wrapup()
/*
Reset the metadata reader for the next translation unit.  This clears all 
imported assemblies.
*/
{
  if (metadata_reader != nullptr) {
    metadata_reader->trans_unit_wrapup();
  }  /* if */
}  /* ms_metadata_trans_unit_wrapup */


EXTERN_C_IN_CPP_FILE void ms_metadata_cleanup()
/*
Cleanup.  Free all memory and release the interfaces.
*/
{
  if (metadata_reader != nullptr) {
    metadata_reader->cleanup();
    delete metadata_reader;
    metadata_reader = nullptr;
  }  /* if */
}  /* ms_metadata_cleanup */


EXTERN_C_IN_CPP_FILE
a_cpp_cli_feature_set 
                edg_supported_features = cpp_cli_ref_classes |
                                         cpp_cli_value_types |
                                         cpp_cli_interfaces |
                                         cpp_cli_enumerations |
                                         cpp_cli_properties |
                                         cpp_cli_events |
//                                       cpp_cli_generic_types |
//                                       cpp_cli_generic_methods |
                                         cpp_cli_delegates |
//                                       cpp_cli_define_all_types |
                                         cpp_cli_declspec_assemby_info;

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2010 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
