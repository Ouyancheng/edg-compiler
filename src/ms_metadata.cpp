/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2010-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/

/*

ms_metadata.cpp -- reading of C++/CLI metadata from assemblies.

This is a C++ file that relies on Microsoft Windows
APIs.  As a result, it can only be compiled on a Windows platform.
In addition, the C++ code makes use of C++0x features, so it must be
compiled by at least the Microsoft VC10 compiler or version 4.2 of the
EDG front end.

If you have CPPCLI_ENABLING_POSSIBLE set to FALSE (the default), you
don't need this file.  You don't need to compile it, and you don't
need to link it in.  You can stick with the traditional C-only build
process.

Note that it uses the alink.h include file.  This file is not
currently part of the Windows SDK, although it is expected to be
at some point in the future.  The file can be downloaded at
http://code.msdn.microsoft.com/alink

*/

#include "basics.h"

#if CPPCLI_ENABLING_POSSIBLE

#include <windows.h>
#include <metahost.h>
#include <cor.h>
#include "alink.h"

#include <string>
#include <map>
#include <vector>
#include <stack>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <memory>

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

/* The carriage return character is used because the new line character,
   ATTENTION_MARKER, has special meaning. */
#define END_OF_LINE '\r'
#define L_END_OF_LINE L'\r'

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


/*
A helper for the an_import_interface class that implements the wrapping of
calls to functions that require a string buffer of unknown length.  An
attempt is first made to call the wrapped function with a stack-based
buffer of reasonable length.  If that buffer was not large enough, a
heap-based buffer of the required size is allocated and the wrapped
function is called a second time.
*/
#define GET_NAME_WRAPPER(WRAPPED_FUNCTION) \
{ \
  HRESULT             hr; \
  WCHAR               name_buffer_stack[128]; \
  unique_ptr<WCHAR[]> name_buffer_heap; \
  WCHAR               *name_buffer = name_buffer_stack; \
  ULONG               characters_in_name = _countof(name_buffer_stack); \
  ULONG               characters_required = 0; \
  hr = WRAPPED_FUNCTION; \
  if (hr == CLDB_S_TRUNCATION) { \
    check_assertion(characters_required > characters_in_name); \
    name_buffer_heap.reset(new WCHAR[characters_required]); \
    name_buffer = name_buffer_heap.get(); \
    characters_in_name = characters_required; \
    hr = WRAPPED_FUNCTION; \
    check_assertion(hr != CLDB_S_TRUNCATION); \
  }  /* if */ \
  check_assertion(characters_required != 0); \
  name.assign(name_buffer, characters_required - 1); \
  return hr; \
}


class an_import_interface : public IMetaDataImport2
/*
A wrapper for the IMetaDataImport2 interface that facilitates invoking the
functions that require a string buffer of unknown length.
*/
{
private:
  an_import_interface();
public:
  using IMetaDataImport2::GetScopeProps;
  HRESULT GetScopeProps(wstring &name,
                        GUID    *pmvid) {
    GET_NAME_WRAPPER(GetScopeProps(
      name_buffer, characters_in_name, &characters_required, pmvid));
  }  /* GetScopeProps */

  using IMetaDataImport2::GetTypeDefProps;
  HRESULT GetTypeDefProps(mdTypeDef td,
                          wstring   &name,
                          DWORD     *pdwTypeDefFlags,
                          mdToken   *ptkExtends) {
    GET_NAME_WRAPPER(GetTypeDefProps(
      td, name_buffer, characters_in_name, &characters_required,
      pdwTypeDefFlags, ptkExtends));
  }  /* GetTypeDefProps */

  using IMetaDataImport2::GetTypeRefProps;
  HRESULT GetTypeRefProps(mdTypeRef tr,
                          mdToken   *ptkResolutionScope,
                          wstring   &name) {
    GET_NAME_WRAPPER(GetTypeRefProps(
      tr, ptkResolutionScope, name_buffer, characters_in_name,
      &characters_required));
  }  /* GetTypeRefProps */

  HRESULT GetTypeRefProps(mdTypeRef tr,
                          mdToken   *ptkResolutionScope) {
    return GetTypeRefProps(tr, ptkResolutionScope, /*szName=*/nullptr,
                           /*cchName=*/0, /*pchName=*/0);
  }  /* GetTypeRefProps */

  using IMetaDataImport2::GetMethodProps;
  HRESULT GetMethodProps(mdMethodDef     mb,
                         mdTypeDef       *pClass,
                         wstring         &name,
                         DWORD           *pdwAttr,
                         PCCOR_SIGNATURE *ppvSigBlob,
                         ULONG           *pcbSigBlob,
                         ULONG           *pulCodeRVA,
                         DWORD           *pdwImplFlags) {
    GET_NAME_WRAPPER(GetMethodProps(
      mb, pClass, name_buffer, characters_in_name, &characters_required,
      pdwAttr, ppvSigBlob, pcbSigBlob, pulCodeRVA, pdwImplFlags));
  }  /* GetMethodProps */

  using IMetaDataImport2::GetMemberRefProps;
  HRESULT GetMemberRefProps(mdMemberRef     mr,
                            mdToken         *ptk,
                            wstring         &name,
                            PCCOR_SIGNATURE *ppvSigBlob,
                            ULONG           *pbSig) {
    GET_NAME_WRAPPER(GetMemberRefProps(
      mr, ptk, name_buffer, characters_in_name, &characters_required,
      ppvSigBlob, pbSig));
  }  /* GetMemberRefProps */

  using IMetaDataImport2::GetEventProps;
  HRESULT GetEventProps(mdEvent     ev,
                        mdTypeDef   *pClass,
                        wstring     &name,
                        DWORD       *pdwEventFlags,
                        mdToken     *ptkEventType,
                        mdMethodDef *pmdAddOn,
                        mdMethodDef *pmdRemoveOn,
                        mdMethodDef *pmdFire,
                        mdMethodDef rmdOtherMethod[],
                        ULONG       cMax,
                        ULONG       *pcOtherMethod) {
    GET_NAME_WRAPPER(GetEventProps(
      ev, pClass, name_buffer, characters_in_name, &characters_required,
      pdwEventFlags, ptkEventType, pmdAddOn, pmdRemoveOn, pmdFire,
      rmdOtherMethod, cMax, pcOtherMethod));
  }  /* GetEventProps */

  using IMetaDataImport2::GetModuleRefProps;
  HRESULT GetModuleRefProps(mdModuleRef mur,
                            wstring     &name) {
    GET_NAME_WRAPPER(GetModuleRefProps(
      mur, name_buffer, characters_in_name, &characters_required));
  }  /* GetModuleRefProps */

  using IMetaDataImport2::GetUserString;
  HRESULT GetUserString(mdString stk,
                        wstring  &name) {
    GET_NAME_WRAPPER(GetUserString(
      stk, name_buffer, characters_in_name, &characters_required));
  }  /* GetUserString */

  using IMetaDataImport2::GetPinvokeMap;
  HRESULT GetPinvokeMap(mdToken     tk,
                        DWORD       *pdwMappingFlags,
                        wstring     &name,
                        mdModuleRef *pmrImportDLL) {
    GET_NAME_WRAPPER(GetPinvokeMap(
      tk, pdwMappingFlags, name_buffer, characters_in_name,
      &characters_required, pmrImportDLL));
  }  /* GetPinvokeMap */

  using IMetaDataImport2::GetMemberProps;
  HRESULT GetMemberProps(mdToken         mb,
                         mdTypeDef       *pClass,
                         wstring         &name,
                         DWORD           *pdwAttr,
                         PCCOR_SIGNATURE *ppvSigBlob,
                         ULONG           *pcbSigBlob,
                         ULONG           *pulCodeRVA,
                         DWORD           *pdwImplFlags,
                         DWORD           *pdwCPlusTypeFlag,
                         UVCP_CONSTANT   *ppValue,
                         ULONG           *pcchValue) {
    GET_NAME_WRAPPER(GetMemberProps(
      mb, pClass, name_buffer, characters_in_name, &characters_required,
      pdwAttr, ppvSigBlob, pcbSigBlob, pulCodeRVA, pdwImplFlags,
      pdwCPlusTypeFlag, ppValue, pcchValue));
  }  /* GetMemberProps */

  using IMetaDataImport2::GetFieldProps;
  HRESULT GetFieldProps(mdFieldDef      mb,
                        mdTypeDef       *pClass,
                        wstring         &name,
                        DWORD           *pdwAttr,
                        PCCOR_SIGNATURE *ppvSigBlob,
                        ULONG           *pcbSigBlob,
                        DWORD           *pdwCPlusTypeFlag,
                        UVCP_CONSTANT   *ppValue,
                        ULONG           *pcchValue) {
    GET_NAME_WRAPPER(GetFieldProps(
      mb, pClass, name_buffer, characters_in_name, &characters_required,
      pdwAttr, ppvSigBlob, pcbSigBlob, pdwCPlusTypeFlag, ppValue,
      pcchValue));
  }  /* GetFieldProps */

  using IMetaDataImport2::GetPropertyProps;
  HRESULT GetPropertyProps(mdProperty      prop,
                           mdTypeDef       *pClass,
                           wstring         &name,
                           DWORD           *pdwPropFlags,
                           PCCOR_SIGNATURE *ppvSig,
                           ULONG           *pbSig,
                           DWORD           *pdwCPlusTypeFlag,
                           UVCP_CONSTANT   *ppDefaultValue,
                           ULONG           *pcchDefaultValue,
                           mdMethodDef     *pmdSetter,
                           mdMethodDef     *pmdGetter,
                           mdMethodDef     rmdOtherMethod[],
                           ULONG           cMax,
                           ULONG           *pcOtherMethod) {
    GET_NAME_WRAPPER(GetPropertyProps(
      prop, pClass, name_buffer, characters_in_name, &characters_required,
      pdwPropFlags, ppvSig, pbSig, pdwCPlusTypeFlag, ppDefaultValue,
      pcchDefaultValue, pmdSetter, pmdGetter, rmdOtherMethod, cMax,
      pcOtherMethod));
  }  /* GetPropertyProps */

  using IMetaDataImport2::GetParamProps;
  HRESULT GetParamProps(mdParamDef    tk,
                        mdMethodDef   *pmd,
                        ULONG         *pulSequence,
                        wstring       &name,
                        DWORD         *pdwAttr,
                        DWORD         *pdwCPlusTypeFlag,
                        UVCP_CONSTANT *ppValue,
                        ULONG         *pcchValue) {
    GET_NAME_WRAPPER(GetParamProps(
      tk, pmd, pulSequence, name_buffer, characters_in_name,
      &characters_required, pdwAttr, pdwCPlusTypeFlag, ppValue, pcchValue));
  }  /* GetParamProps */

  using IMetaDataImport2::GetGenericParamProps;
  HRESULT GetGenericParamProps(mdGenericParam gp,
                               ULONG          *pulParamSeq,
                               DWORD          *pdwParamFlags,
                               mdToken        *ptOwner,
                               DWORD          *reserved,
                               wstring        &name) {
    GET_NAME_WRAPPER(GetGenericParamProps(
      gp, pulParamSeq, pdwParamFlags, ptOwner, reserved, name_buffer,
      characters_in_name, &characters_required));
  }  /* GetGenericParamProps */
};  /* an_import_interface */
static_assert(sizeof(an_import_interface) == sizeof(IMetaDataImport2),
             "an_import_interface must be the same size as IMetaDataImport2");

class an_import_scope;

/*
The representation of a single generic parameter.
*/
class a_generic_parameter_info {
public:
  a_generic_parameter_info(mdGenericParam token, DWORD flags)
    : token_(token),
      flags_(flags)
  {
  }  /* constructor */

  a_generic_parameter_info(a_generic_parameter_info&& other)
    : token_(move(other.token_)),
      flags_(move(other.flags_))
  {
  }  /* constructor */

  mdGenericParam token() const { return token_; }
  DWORD          flags() const { return flags_; }

private:
  mdGenericParam token_;
                        /* The token for this generic parameter. */
  DWORD          flags_;
                        /* Any flags associated with this generic
                           parameter. */
}; /* a_generic_parameter_data */

/* A list of generic parameter data. */
typedef vector<const a_generic_parameter_info> a_generic_parameter_info_list;
/* A list of generic parameter names or arguments. */
typedef vector<const wstring> a_generic_param_or_arg_list;
typedef a_generic_param_or_arg_list::const_iterator
                                              a_generic_param_or_arg_iterator;
/* A list of generic parameter names. */
typedef a_generic_param_or_arg_list a_generic_parameter_list;
/* An empty list of generic type parameters. */
const a_generic_parameter_list no_generic_type_parameters;
/* An empty list of generic method parameters. */
const a_generic_parameter_list no_generic_method_parameters;
/* A list of generic arguments. */
typedef a_generic_param_or_arg_list a_generic_argument_list;
/* A list of constraint clauses. */
typedef vector<const wstring> a_constraint_clause_list;
/* An empty list of generic constraints. */
const a_constraint_clause_list no_generic_constraints;


BYTE strip_generic_arity(wstring &type_name)
/*
Strip the generic arity encoded after the last backtick in the type name.
*/
{
  BYTE generic_arity = 0;
  wstring::size_type back_tick_index = type_name.rfind(L'`');
  check_assertion(back_tick_index != 0 &&
                  back_tick_index != type_name.size());
  if (back_tick_index != wstring::npos) {
    ULONG val = _wtoi(type_name.c_str() + back_tick_index + 1);
    check_assertion(val > 0 && val <= (numeric_limits<BYTE>::max)());
    generic_arity = static_cast<BYTE>(val);
    type_name.resize(back_tick_index);
  }  /* if */
  return generic_arity;
}  /* strip_generic_arity */


static void expand_generic_type_name(
                wstring                           &type_name,
                const a_generic_param_or_arg_list &generic_params_or_args,
                a_generic_param_or_arg_iterator   &generic_params_or_args_end)
/*
Strip the generic arity encoded after the last backtick in type_name and
replace it with the generic parameters or arguments from the list provided in
generic_params_or_args.  Upon return, generic_params_or_args_end refers to the
last generic parameter or argument consumed by the expansion.
*/
{
  BYTE generic_arity = strip_generic_arity(type_name);
  if (generic_arity != 0) {
    /* The generic parameter names or arguments associated with this
       generic type or instantiation are at the end of the list. */
    check_assertion(generic_arity <=
                              distance(generic_params_or_args.begin(),
                                        generic_params_or_args_end));
    type_name += L'<';
    for (auto iter = generic_params_or_args_end - generic_arity;
         iter != generic_params_or_args_end;
         ++iter) {
      const wstring &param_or_arg_name = *iter;
      type_name += param_or_arg_name;
      if (iter + 1 != generic_params_or_args_end)
        type_name += L", ";
    }  /* for */
    type_name += L'>';
    generic_params_or_args_end = generic_params_or_args_end - generic_arity;
  }  /* if */
}  /* expand_generic_type_name */


/*
A class to decode a CLR signature.  It returns the result as a std::wstring.
*/
class a_signature_decoder {
public:
  a_signature_decoder(
               const an_import_scope&         import_scope,
               mdToken                        token,
               PCCOR_SIGNATURE                signature,
               ULONG                          bytes_in_signature,
               const a_generic_parameter_list &generic_type_parameters,
               const a_generic_parameter_list &generic_method_parameters,
               a_boolean                      is_system_string_member)
    : import_scope_(import_scope),
      token_(token),
      signature_(signature),
      bytes_in_signature_(bytes_in_signature),
      generic_type_parameters_(generic_type_parameters),
      generic_method_parameters_(generic_method_parameters),
      is_system_string_member_(is_system_string_member),
      index_(0)
  {
  }  /* constructor */

  wstring decode_type(bool add_handle_to_class = true);

  wstring decode_method_signature(const wstring &name,
                                  DWORD         method_attributes,
                                  bool          omit_return_type,
                                  bool          is_for_property);

  wstring decode_field_signature()
  {
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


  CorElementType peek_element_type()
  {
    check_assertion(index_ < bytes_in_signature_);
    return static_cast<CorElementType>(signature_[index_]);
  }  /* peek_element_type */


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


  a_generic_argument_list decode_generic_arguments();

private:
  const an_import_scope
                &import_scope_;
                        /* The import scope associated with this signature. */
  mdToken       token_;
                        /* The token for the method. */
  const PCCOR_SIGNATURE
                signature_;
                        /* The signature we want to decode. */
  const ULONG   bytes_in_signature_;
                        /* The number of bytes in the signature. */
  const a_generic_parameter_list
                &generic_type_parameters_;
                        /* Any generic type parameters associated with this
                           method signature. */
  a_generic_parameter_list
                generic_method_parameters_;
                        /* Any generic method parameters associated with this
                           method signature. */
  a_boolean     is_system_string_member_;
                        /* TRUE if this signature is for a member of
                           System::String. */
  ULONG         index_;
                        /* The current index into the signature. */
}; /* a_signature_decoder */


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
      { /* This type requires special handling to ensure that we get the
           decimal value not the character. */
        unsigned char value = convert_to<unsigned char>(constant_value_);

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
      /* The cast to unsigned int is used to prevent the value from being
         interpreted as a Unicode character when wchar_t is not defined. */
      buffer << L"0x" << hex << static_cast<unsigned int>(
                                 convert_to<unsigned short>(constant_value_));
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
      buffer << L"0x" << hex
             << convert_to<unsigned long long>(constant_value_);
      break;
    case ELEMENT_TYPE_R4:
      { UINT      f = convert_to<unsigned int>(constant_value_);
        bool      sign =         (f & 0x80000000U) != 0;
        int       exponent = int((f & 0x7F800000U) >> 23) - 127;
        ULONGLONG fraction =      f & 0x007FFFFFU;
        if (sign) buffer << L'-';
        buffer << L"0x." << hex << fraction << L"p";
        buffer << dec << exponent << L"f";
        break;
      } /* case ELEMENT_TYPE_R4 */
    case ELEMENT_TYPE_R8:
      { ULONGLONG d = convert_to<unsigned long long>(constant_value_);
        bool      sign =         (d & 0x8000000000000000ULL) != 0;
        int       exponent = int((d & 0x7FF0000000000000ULL) >> 52) - 1023;
        ULONGLONG fraction =      d & 0x000FFFFFFFFFFFFFULL;
        if (sign) buffer << L'-';
        buffer << L"0x." << hex << fraction << L"p";
        buffer << dec << exponent;
        break;
      } /* case ELEMENT_TYPE_R8 */
    case ELEMENT_TYPE_STRING:
      { wchar_t *ch = (wchar_t*)(constant_value_);
        buffer << L"L\"" << hex;
        for (ULONG i = 0; i < characters_in_constant_; ++i) {
          /* Output every character as a hexadecimal escape sequence to avoid
             having to special-case characters such as quotes, backslashes,
             tabs, carriage returns, and nulls. */
          buffer << L"\\x" << static_cast<unsigned int>(ch[i]);
        }  /* for */
        buffer << L'\"' << dec;
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
  an_assembly(const wstring             &full_assembly_name,
              IALink                    *alink_interface,
              mdFile                    file_token,
              a_cpp_cli_import_flag_set import_flags)
    : full_assembly_name_(full_assembly_name),
      assembly_index_(static_cast<an_assembly_index>(-1)),
      alink_interface_(alink_interface),
      alink_token_(mdTokenNil),
      import_flags_(import_flags),
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
      import_flags_(move(other.import_flags_)),
      md_assembly_import_interface_(
                                   move(other.md_assembly_import_interface_)),
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
    import_flags_ = move(other.import_flags_);
    md_assembly_import_interface_ = move(other.md_assembly_import_interface_);
    resolution_scope_ = move(other.resolution_scope_);
    file_token_ = move(other.file_token_);
    count_of_scopes_ = move(other.count_of_scopes_);
    imported_scopes_ = move(other.imported_scopes_);
    return *this;
  }  /* operator= */

  bool process();
  void cleanup();

  a_cpp_cli_import_flag_set import_flags() const
  {
    return import_flags_;
  }  /* import_flags */


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
                        /* The token to be used when calling any IALink
                           API. */
  a_cpp_cli_import_flag_set
                import_flags_;
                        /* Flags which control the import behavior. */
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
or more import scope - though only one import scope has the metadata for
types.
*/
class an_import_scope {
public:
  an_import_scope(an_import_interface* import_interface,
                  an_assembly&      containing_assembly);

  an_import_scope(an_import_scope&& other)
    : scope_name_(move(other.scope_name_)),
      containing_assembly_(other.containing_assembly_),
      import_interface_(move(other.import_interface_)),
      namespace_stack_(move(other.namespace_stack_))
  {
  }  /* constructor */

  an_import_scope &operator=(an_import_scope&& other);

  an_import_interface *import_interface() const
  {
    return import_interface_;
  }  /* import_interface */


  const an_assembly &containing_assembly() const
  {
    return containing_assembly_;
  }  /* containing_assembly */


  void import_all_types(ostringstream &buffer);
  void import_one_type(
                ostringstream                  &buffer,
                mdTypeDef                      typedef_token,
                bool                           at_top_level,
                const a_generic_parameter_list &enclosing_type_generic_params,
                bool                           want_definition,
                bool                           class_body_only);
  wstring resolve_type_token(
             mdToken                           token,
             const a_generic_param_or_arg_list &generic_type_params_or_args,
             const a_generic_param_or_arg_list &generic_method_params_or_args,
             bool                              replaces_dots) const;
  wstring resolve_type_token(
            mdToken                           token,
            const a_generic_param_or_arg_list &generic_type_params_or_args,
            a_generic_param_or_arg_iterator   generic_type_params_or_args_end,
            const a_generic_param_or_arg_list &generic_method_params_or_args,
            bool                              replaces_dots) const;
  void get_generic_parameters_and_constraints(
           mdToken                        token,
           const a_generic_parameter_list &generic_type_parameters_for_method,
           BYTE                           generic_arity,
           a_generic_parameter_list       &generic_parameters,
           a_constraint_clause_list       &generic_constraints) const;
  void get_generic_constraints(
      mdToken                             token,
      const a_generic_parameter_list      &generic_type_parameters_for_method,
      BYTE                                generic_arity,
      const a_generic_parameter_info_list &generic_parameters_info,
      const a_generic_parameter_list      &generic_parameters,
      a_constraint_clause_list            &generic_constraints) const;
  wstring form_generic_type_header(
                   mdTypeDef                         typedef_token,
                   DWORD                             type_attributes,
                   BYTE                              generic_arity,
                   const a_generic_param_or_arg_list &generic_parameters,
                   a_generic_param_or_arg_iterator   generic_parameters_end,
                   const a_constraint_clause_list    &generic_constraints,
                   a_boolean                         out_of_class_definition);
  void cleanup();

private:
  void import_enum_definition(
                     ostringstream                  &buffer,
                     mdTypeDef                      typedef_token,
                     const wstring                  &enumeration_name,
                     const a_generic_parameter_list &generic_type_parameters);
  void import_delegate_definition(
                     ostringstream                  &buffer,
                     mdTypeDef                      typedef_token,
                     const wstring                  &delegate_name,
                     const a_generic_parameter_list &generic_type_parameters);

  a_top_level_kind classify_type(
                      const wstring                  &full_type_name,
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
  an_import_interface
                *import_interface_;
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


an_import_scope::an_import_scope(an_import_interface *import_interface,
                                 an_assembly      &containing_assembly)
/*
Create a representation of an import scope and get the information about the
scope that we will need later.  Currently this is just the name of the scope.
*/
  : import_interface_(import_interface),
    containing_assembly_(containing_assembly)
{
  HRESULT hr;

  hr = import_interface_->GetScopeProps(scope_name_, /*pmvid=*/nullptr);
  CHECK_API_RESULT(hr, GetScopeProps);
}  /* an_import_scope::an_import_scope */


void an_import_scope::cleanup()
/*
Cleanup an import scope.
*/
{
  release_and_zero_out_helper(import_interface_);
}  /* an_import_scope::cleanup */


void an_import_scope::import_all_types(ostringstream& buffer)
/*
Import all the types from an import scope.
*/
{
  HCORENUM  enum_typedefs = nullptr;
  mdTypeDef typedefs[64];
  ULONG     count_of_typedefs;

  do {
    HRESULT hr = import_interface_->EnumTypeDefs(&enum_typedefs, typedefs,
                                                 _countof(typedefs),
                                                 &count_of_typedefs);

    CHECK_API_RESULT(hr, EnumTypeDefs);
    for (ULONG i = 0; i < count_of_typedefs; ++i) {
      import_one_type(buffer, typedefs[i], /*at_top_level=*/true,
                      no_generic_type_parameters, /*want_definition=*/false,
                      /*class_body_only=*/false);
    }  /* for */
  } while (count_of_typedefs > 0);
  import_interface_->CloseEnum(enum_typedefs);
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
  buffer << " {" << END_OF_LINE;
}  /* an_import_scope::open_namespace */


void an_import_scope::close_namespace(ostringstream& buffer,
                                      const wstring& namespace_name)
/*
Emit the text to close a namespace scope.
*/
{
  buffer << '}';
#if DEBUG
  buffer << "  /* namespace " << namespace_name << " */";
#endif /* DEBUG */
  buffer << END_OF_LINE;
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
  /* If the current namespace stack still has namespaces beyond this point
     then we need to close them in the correct order. */
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
                      const wstring                  &full_type_name,
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
  } else if (full_type_name == L"System.Enum" ||
             full_type_name == L"System.MulticastDelegate") {
    kind = tlk_ref_class;
  } else if (!IsNilToken(extends_token)) {
    wstring extends_class_name = resolve_type_token(
                                                 extends_token,
                                                 generic_type_parameters,
                                                 no_generic_method_parameters,
                                                 /*replaces_dots=*/false);
    if (extends_class_name == L"System.ValueType") {
      kind = tlk_value_type;
    } else if (extends_class_name == L"System.Enum") {
      kind = tlk_enumeration;
    } else if (extends_class_name == L"System.Delegate" ||
               extends_class_name == L"System.MulticastDelegate") {
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
    hr = md_import_inferface->QueryInterface(IID_IMetaDataImport2,
                                             reinterpret_cast<void**>(
                                                      &md_import2_inferface));
    if (FAILED(hr)) {
      result = false;
      break;
    }  /* if */
    /* Release old interface. */
    md_import_inferface->Release();
    /* Create an import scope and import all the types. */
    imported_scopes_.push_back(an_import_scope(
             static_cast<an_import_interface*>(md_import2_inferface), *this));
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


void an_import_scope::import_enum_definition(
                      ostringstream                  &buffer,
                      mdTypeDef                      typedef_token,
                      const wstring                  &enumeration_name,
                      const a_generic_parameter_list &generic_type_parameters)
/*
Import the definition of an enumeration and emit the code for the definition.
*/
{
  /* Each enumerator is represented by a name-value pair. */
  typedef pair<wstring, wstring> an_enumerator;
  HCORENUM              enum_members = nullptr;
  mdTypeDef             members[64];
  ULONG                 count_of_members;
  HRESULT               hr;
  wstring               underlying_type;
  vector<an_enumerator> enumerators;

  do {
    hr = import_interface_->EnumMembers(&enum_members, typedef_token,
                                        members, _countof(members),
                                        &count_of_members);
    CHECK_API_RESULT(hr, EnumMembers);
    /* Reserve the necessary space. */
    enumerators.reserve(enumerators.size() + count_of_members);
    for (ULONG i = 0; i < count_of_members; ++i) {
      wstring         member_name;
      ULONG           characters_in_constant;
      DWORD           attributes, constant_type;
      PCCOR_SIGNATURE signature;
      ULONG           bytes_in_signature;
      UVCP_CONSTANT   constant_value;

      /* Get the name, the type and the constant associated with this
         enumerator. */
      hr = import_interface_->GetMemberProps(members[i], /*pClass=*/nullptr,
                                             member_name, &attributes,
                                             &signature, &bytes_in_signature,
                                             /*pulCodeRVA=*/nullptr,
                                             /*pdwImplFlags=*/nullptr,
                                             &constant_type, &constant_value,
                                             &characters_in_constant);
      CHECK_API_RESULT(hr, GetMemberProps);
      if (member_name == L"value__") {
        /* This is the special member: its type is the underlying type of the
           enumeration. */
        a_signature_decoder decoder(*this, members[i], signature,
                                    bytes_in_signature,
                                    generic_type_parameters,
                                    no_generic_method_parameters,
                                    /*is_system_string_member=*/FALSE);
        check_assertion(IsFdRTSpecialName(attributes) != 0);
        check_assertion(constant_type == ELEMENT_TYPE_VOID);
        check_assertion(underlying_type.empty());
        underlying_type = decoder.decode_field_signature();
      } else {
        a_constant_decoder decoder(constant_type, constant_value,
                                   characters_in_constant);

        /* Get the constant value associated with it. */
        enumerators.push_back(an_enumerator(member_name, decoder.decode()));
      }  /* if */
    }  /* for */
  } while (count_of_members > 0);
  import_interface_->CloseEnum(enum_members);
  /* Now we have all the information we need, we can emit the definition of
     the enumeration.  Note, we emit the value of an enumerator as a
     hexadecimal constant cast to the underlying type of the enumeration. */
  check_assertion(!underlying_type.empty());
  buffer << enumeration_name << " : " << underlying_type << " {"
         << END_OF_LINE;
  for (auto enum_iter = enumerators.begin();
       enum_iter != enumerators.end();
       ++enum_iter) {
    const an_enumerator &enumerator = *enum_iter;
    buffer << enumerator.first << " = static_cast<" << underlying_type << ">("
           << enumerator.second << ')';
    if (enum_iter + 1 != enumerators.end()) {
      buffer << ",";
    }  /* if */
    buffer << END_OF_LINE;
  }  /* for */
  buffer << "};";
#if DEBUG
  buffer << "  /* enum " << enumeration_name << " */";
#endif /* DEBUG */
  buffer << END_OF_LINE;
}  /* an_import_scope::import_enum_definition */


void an_import_scope::import_delegate_definition(
                      ostringstream                  &buffer,
                      mdTypeDef                      typedef_token,
                      const wstring                  &delegate_name,
                      const a_generic_parameter_list &generic_type_parameters)
/*
Import the definition of a delegate.  This is essentially the signature of
the Invoke method - which every delegate must have.
*/
{
  HCORENUM                  enum_methods = nullptr;
  mdMethodDef               methods[2];
  ULONG                     count_of_methods;
  HRESULT                   hr;

  hr = import_interface_->EnumMethodsWithName(&enum_methods,
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
    wstring         method_name;
    DWORD           method_attributes;
    hr = import_interface_->GetMethodProps(methods[0], /*pClass=*/nullptr,
                                           method_name, &method_attributes,
                                           &signature, &bytes_in_signature,
                                           /*pulCodeRVA=*/nullptr,
                                           /*pdwImplFlags=*/nullptr);
    CHECK_API_RESULT(hr, GetMethodProps);
    check_assertion(method_name == L"Invoke");
    a_signature_decoder decoder(*this, methods[0], signature,
                                bytes_in_signature, generic_type_parameters,
                                no_generic_method_parameters,
                                /*is_system_string_member=*/FALSE);
    buffer << decoder.decode_method_signature(delegate_name,
                                              method_attributes &
                                                         ~(mdStatic|mdVirtual),
                                              /*omit_return_type=*/false,
                                              /*is_for_property=*/false);
    buffer << ';' << END_OF_LINE;
  } else {
    unexpected_condition();
  }  /* if */
  import_interface_->CloseEnum(enum_methods);
}  /* an_import_scope::import_delegate_definition */


void an_import_scope::get_generic_parameters_and_constraints(
           mdToken                        token,
           const a_generic_parameter_list &generic_type_parameters_for_method,
           BYTE                           generic_arity,
           a_generic_parameter_list       &generic_parameters,
           a_constraint_clause_list       &generic_constraints) const
/*
Fill-in the generic parameters and constraints associated with this type or
method.
*/
{
  a_generic_parameter_info_list generic_parameters_info;

  if (TypeFromToken(token) == mdtTypeDef ||
      TypeFromToken(token) == mdtMethodDef) {
    HRESULT        hr;
    HCORENUM       enum_parameters = nullptr;
    mdGenericParam parameters[8];
    ULONG          count_of_parameters;
    do {
      hr = import_interface_->EnumGenericParams(&enum_parameters,
                                                token, parameters,
                                                _countof(parameters),
                                                &count_of_parameters);
      CHECK_API_RESULT(hr, EnumGenericParams);
      generic_parameters_info.reserve(generic_parameters_info.size() +
                                      count_of_parameters);
      generic_parameters.reserve(generic_parameters.size() +
                                 count_of_parameters);
      for (ULONG parameter_index = 0;
           parameter_index < count_of_parameters;
           ++parameter_index) {
        mdGenericParam           parameter = parameters[parameter_index];
        ULONG                    param_index;
        DWORD                    param_flags;
        wstring                  param_name;
        hr = import_interface_->GetGenericParamProps(parameter,
                                                     &param_index,
                                                     &param_flags,
                                                     /*param_owner=*/nullptr,
                                                     /*reserved=*/nullptr,
                                                     param_name);
        CHECK_API_RESULT(hr, GetGenericParamProps);
        check_assertion(generic_parameters.size() == param_index);
        generic_parameters_info.push_back(
                            a_generic_parameter_info(parameter, param_flags));
        generic_parameters.push_back(move(param_name));
      }  /* for */
    } while (count_of_parameters > 0);
    import_interface_->CloseEnum(enum_parameters);
  }  /* if */
  if (TypeFromToken(token) == mdtTypeDef) {
    /* When a generic class is nested within a generic class, parameter names
       associated with the enclosing generic class can be the same as
       parameter names associated with the nested generic class.  For example:
         generic <typename T>
         public ref struct GenericClass {
           typedef T OUTER_T;
           generic <typename T>
           ref struct NestedGenericClass {
             T t;
             OUTER_T ot;
           };
         };
       However, the generated definition would not be valid if duplicate names
       exist.  For example, the following would be generated as the definition
       of NestedGenericClass:
         generic <typename T>
         generic <typename T>
         ref struct GenericClass<T>::NestedGenericClass {
           T t;
           T ot;
         };
       To handle this case, rename any duplicate generic parameter names
       associated with any enclosing generic classes. */
    check_assertion(generic_arity <= generic_parameters.size());
    for (auto param_iter = generic_parameters.begin();
         param_iter != generic_parameters.end() - generic_arity;
         ++param_iter) {
      wstring &param_name = *param_iter;
      auto    duplicate_count = count(param_iter + 1,
                                      generic_parameters.end(),
                                      param_name);
      if (duplicate_count > 0) {
        param_name += L"__hidden";
        param_name += to_wstring((LONGLONG)duplicate_count);
      }  /* if */
    }  /* for */
  } else if (TypeFromToken(token) == mdtMethodDef) {
    /* When a generic method is nested within a generic class, parameter names
       associated with the enclosing generic class can be the same as
       parameter names associated with the nested generic method.  For
       example:
         generic <typename T>
         public ref struct GenericClass {
           typedef T OUTER_T;
           generic <typename T>
           void NestedGenericMethod(T t, OUTER_T ot) {}
         };
       However, the generated definition would not be valid if duplicate names
       exist.  For example, the following would be generated as the definition
       of GenericClass:
         generic <typename T>
         generic <typename T>
         ref struct GenericClass<T>::NestedGenericClass {
           generic <typename T>
           void NestedGenericMethod(T t, T ot) {}
         };
       To handle this case, rename any duplicate generic parameter names
       associated with the generic method. */
    check_assertion(generic_arity == generic_parameters.size());
    for (auto param_iter = generic_parameters.begin();
         param_iter != generic_parameters.end();
         ++param_iter) {
      wstring &param_name = *param_iter;
      if (find(generic_type_parameters_for_method.begin(),
               generic_type_parameters_for_method.end(),
               param_name) != generic_type_parameters_for_method.end()) {
        param_name += L"__method_param";
      }  /* if */
    }  /* for */
  }  /* if */
  if (generic_arity != 0) {
    get_generic_constraints(token, generic_type_parameters_for_method,
                            generic_arity, generic_parameters_info,
                            generic_parameters, generic_constraints);
  }
}  /* an_import_scope::get_generic_parameters_and_constraints */


void an_import_scope::get_generic_constraints(
      mdToken                             token,
      const a_generic_parameter_list      &generic_type_parameters_for_method,
      BYTE                                generic_arity,
      const a_generic_parameter_info_list &generic_parameters_info,
      const a_generic_parameter_list      &generic_parameters,
      a_constraint_clause_list            &generic_constraints) const
/*
Return the constraint clauses associated with this generic type or method.
*/
{
  const a_generic_parameter_list &generic_type_parameters =
                                          TypeFromToken(token) == mdtMethodDef
                                          ? generic_type_parameters_for_method
                                          : generic_parameters;
  const a_generic_parameter_list &generic_method_parameters =
                                          TypeFromToken(token) == mdtMethodDef
                                          ? generic_parameters
                                          : no_generic_method_parameters;

  check_assertion(generic_arity != 0 &&
                  generic_arity <= generic_parameters_info.size() &&
                  generic_parameters_info.size()==generic_parameters.size());
  generic_constraints.reserve(generic_arity);
  /* The generic parameters associated with this generic type are at the end
     of the parameter list. */
  auto info_iter = generic_parameters_info.end() - generic_arity;
  auto param_iter = generic_parameters.end() - generic_arity;
  for (; param_iter != generic_parameters.end(); ++param_iter, ++info_iter) {
    HRESULT                        hr;
    const a_generic_parameter_info &param_info = *info_iter;
    const wstring                  &param_name = *param_iter;
    DWORD                          param_flags = param_info.flags();
    HCORENUM                       enum_constraints = nullptr;
    mdGenericParamConstraint       constraints[8];
    ULONG                          count_of_constraints;
    wstring                        constraint_items;
    if ((param_flags & gpSpecialConstraintMask) != gpNoSpecialConstraint) {
      if (param_flags & gpDefaultConstructorConstraint) {
          constraint_items += L"gcnew(), ";
      }  /* if */
      if (param_flags & gpReferenceTypeConstraint) {
        check_assertion((param_flags &
                         gpNotNullableValueTypeConstraint) == 0);
        constraint_items += L"ref class, ";
      }  /* if */
      if (param_flags & gpNotNullableValueTypeConstraint) {
        check_assertion((param_flags & gpReferenceTypeConstraint) == 0);
        constraint_items += L"value class, ";
      }  /* if */
    }  /* switch */
    do {
      hr = import_interface_->EnumGenericParamConstraints(
                                                       &enum_constraints,
                                                       param_info.token(),
                                                       constraints,
                                                       _countof(constraints),
                                                       &count_of_constraints);
      CHECK_API_RESULT(hr, EnumGenericParamConstraints);
      for (ULONG constraint_index = 0;
           constraint_index < count_of_constraints;
           ++constraint_index) {
        mdGenericParamConstraint constraint = constraints[constraint_index];
        mdToken                  constraint_param;
        mdToken                  constraint_item;
        hr = import_interface_->GetGenericParamConstraintProps(
                                                            constraint,
                                                            &constraint_param,
                                                            &constraint_item);
        CHECK_API_RESULT(hr, GetGenericParamConstraintProps);
        check_assertion(constraint_param == param_info.token());
        constraint_items += resolve_type_token(constraint_item,
                                               generic_type_parameters,
                                               generic_method_parameters,
                                               /*replace_dots=*/true);
        constraint_items += L", ";
      }  /* for */
    } while (count_of_constraints > 0);
    import_interface_->CloseEnum(enum_constraints);
    if (!constraint_items.empty()) {
      wstring constraint_clause = L"where " + param_name + L" : ";
      constraint_clause.append(constraint_items, 0,
                               constraint_items.size() - (_countof(L", ")-1));
      generic_constraints.push_back(move(constraint_clause));
    }  /* if */
  }  /* for */
  generic_constraints.shrink_to_fit();
}  /* an_import_scope::get_generic_constraints */


wstring an_import_scope::resolve_type_token(
             mdToken                           token,
             const a_generic_param_or_arg_list &generic_type_params_or_args,
             const a_generic_param_or_arg_list &generic_method_params_or_args,
             bool                              replace_dots) const
/*
Resolve the type given by the token.  Get the name of the type and replace
any "." in the name with the C++ scope operator, "::".
*/
{
  return resolve_type_token(token, generic_type_params_or_args,
                            generic_type_params_or_args.end(),
                            generic_method_params_or_args, replace_dots);
}  /* an_import_scope::resolve_type_token */


wstring an_import_scope::resolve_type_token(
            mdToken                           token,
            const a_generic_param_or_arg_list &generic_type_params_or_args,
            a_generic_param_or_arg_iterator   generic_type_params_or_args_end,
            const a_generic_param_or_arg_list &generic_method_params_or_args,
            bool                              replace_dots) const
/*
Resolve the type given by the token.  Get the name of the type and replace
any "." in the name with the C++ scope operator, "::".
*/
{
  wstring type_name;
  /* First check if this token is a typedef token for a type we have already
     imported within the current scope. */
  auto iter = map_of_tokens_to_names_.find(token);
  if (iter != map_of_tokens_to_names_.end()) {
    type_name = iter->second;
  } else {
    HRESULT hr;
    switch (TypeFromToken(token)) {
      case mdtTypeDef:
        { DWORD type_flags;
          hr = import_interface_->GetTypeDefProps(token, type_name,
                                                  &type_flags,
                                                  /*ptkExtends=*/nullptr);
          CHECK_API_RESULT(hr, GetTypeDefProps);
          expand_generic_type_name(type_name, generic_type_params_or_args,
                                   generic_type_params_or_args_end);
          if (IsTdNested(type_flags)) {
            mdTypeDef enclosing_typedef;
            hr = import_interface_->GetNestedClassProps(token,
                                                        &enclosing_typedef);
            CHECK_API_RESULT(hr, GetNestedClassProps);
            wstring enclosing_type_name = resolve_type_token(
                                              enclosing_typedef,
                                              generic_type_params_or_args,
                                              generic_type_params_or_args_end,
                                              generic_method_params_or_args,
                                              replace_dots);
            type_name = enclosing_type_name + L"::" + type_name;
          }  /* if */
          break;
        }  /* case mdtTypeDef */
      case mdtTypeRef:
        { mdToken resolution_scope;
          hr = import_interface_->GetTypeRefProps(token,
                                                  &resolution_scope,
                                                  type_name);
          CHECK_API_RESULT(hr, GetTypeRefProps);
          expand_generic_type_name(type_name, generic_type_params_or_args,
                                   generic_type_params_or_args_end);
          /* FIXME: TypeRefs to nested classes are not yet supported. */
          break;
        }  /* case mdtTypeRef */
      case mdtInterfaceImpl:
        /* If this is an interface-impl token then get the token for the
           interface definition and then attempt to resolve that token. */
        hr = import_interface_->GetInterfaceImplProps(token,
                                                      /*mdTypeDef=*/nullptr,
                                                      &token);
        CHECK_API_RESULT(hr, GetInterfaceImplProps);
        type_name = resolve_type_token(token,
                                       generic_type_params_or_args,
                                       generic_method_params_or_args,
                                       replace_dots);
        break;
      case mdtTypeSpec:
        { PCCOR_SIGNATURE signature;
          ULONG           bytes_in_signature;

          hr = import_interface_->GetTypeSpecFromToken(token, &signature,
                                                       &bytes_in_signature);
          CHECK_API_RESULT(hr, GetTypeSpecFromToken);
          a_signature_decoder decoder(*this, token, signature,
                                      bytes_in_signature,
                                      generic_type_params_or_args,
                                      generic_method_params_or_args,
                                      /*is_system_string_member=*/FALSE);
          type_name = decoder.decode_type(/*add_handle_to_class=*/false);
          break;
        }  /* case mdtTypeSpec */
      default:
        unexpected_condition();
        break;
    }  /* switch */
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
  import_interface_ = move(other.import_interface_);
  namespace_stack_ = move(other.namespace_stack_);
  return *this;
}  /* an_import_scope::operator= */


class an_accessibility
{
public:
  an_accessibility() : access_(access_none) {}
  an_accessibility(mdToken token, DWORD attributes)
  {
    check_assertion(!IsNilToken(token));
    switch (TypeFromToken(token)) {
      case mdtTypeDef:
        switch (attributes & tdVisibilityMask) {
          case tdNotPublic:
            access_ = access_private;
            break;
          case tdPublic:
            access_ = access_public;
            break;
          case tdNestedPublic:
            access_ = access_public;
            break;
          case tdNestedPrivate:
            access_ = access_private;
            break;
          case tdNestedFamily:
            access_ = access_family;
            break;
          case tdNestedAssembly:
            access_ = access_assembly;
            break;
          case tdNestedFamANDAssem:
            access_ = access_family_and_assembly;
            break;
          case tdNestedFamORAssem:
            access_ = access_family_or_assembly;
            break;
          default:
            unexpected_condition();
            break;
        }  /* switch */
        break;
      case mdtFieldDef:
        switch (attributes & fdFieldAccessMask) {
          case fdPrivateScope:
            access_ = access_none;
            break;
          case fdPrivate:
            access_ = access_private;
            break;
          case fdFamANDAssem:
            access_ = access_family_and_assembly;
            break;
          case fdAssembly:
            access_ = access_assembly;
            break;
          case fdFamily:
            access_ = access_family;
            break;
          case fdFamORAssem:
            access_ = access_family_or_assembly;
            break;
          case fdPublic:
            access_ = access_public;
            break;
          default:
            unexpected_condition();
            break;
        }  /* switch */
        break;
      case mdtMethodDef:
        switch (attributes & mdMemberAccessMask) {
          case mdPrivateScope:
            access_ = access_none;
            break;
          case mdPrivate:
            access_ = access_private;
            break;
          case mdFamANDAssem:
            access_ = access_family_and_assembly;
            break;
          case mdAssem:
            access_ = access_assembly;
            break;
          case mdFamily:
            access_ = access_family;
            break;
          case mdFamORAssem:
            access_ = access_family_or_assembly;
            break;
          case mdPublic:
            access_ = access_public;
            break;
          default:
            unexpected_condition();
            break;
        }  /* switch */
        break;
      default:
        unexpected_condition();
        break;
    }  /* switch */
  }  /* constructor */

  wstring get_string(bool as_friend) const
  {
    wstring result;
    switch (access_) {
      case access_none:
        /* We should not be emitting this accessibility. */
        unexpected_condition();
        break;
      case access_private:
        /* Accessible only by the parent type */
        result = L"private";
        break;
      case access_family_and_assembly:
        /* Accessible by subtypes only in the assembly. */
        if (as_friend) {
          result = L"protected";
        } else {
          result = L"private protected";
        }  /* if */
        break;
      case access_family:
        /* Accessible only by type and subtypes. */
        result = L"protected";
        break;
      case access_assembly:
        /* Accessibly by anyone in the assembly. */
        if (as_friend) {
          result = L"public";
        } else {
          result = L"internal";
        }  /* if */
        break;
      case access_family_or_assembly:
        /* Accessible by derived classes and by other types in the
            assembly. */
        if (as_friend) {
          result = L"public";
        } else {
          result = L"protected public";
        }  /* if */
        break;
      case access_public:
        /* Accessible by all types with access to the scope. */
        result = L"public";
        break;
      default:
        unexpected_condition();
        break;
    }  /* switch */
    return result;
  }  /* get_string */

  bool is_accessible() const { return access_ != access_none; }

  bool operator==(const an_accessibility &access) const
  {
    return access_ == access.access_;
  }  /* operator== */

  static an_accessibility wider_accessibility(
                                            const an_accessibility &access1,
                                            const an_accessibility &access2)
  /*
    "access1" has wider access than "access2 if "access1" permits more
    access than "access2" both within the assembly and outside the assembly.
  */
  {
    return access1.access_ > access2.access_ ? access1 : access2;
  }

private:
  enum access_kind {
                                /* within assembly  outside assembly */
    access_none,                /* none             none             */
    access_private,             /* private          private          */
    access_family_and_assembly, /* protected        private          */
    access_family,              /* protected        protected        */
    access_assembly,            /* public           private          */
    access_family_or_assembly,  /* public           protected        */
    access_public               /* public           public           */
  } access_;

  an_accessibility(access_kind access) : access_(access) {}
};  /* an_accessibility */

/*
The representation of a single type definition.  This could be either a
ref-class, a value-type or an interface.
*/
class a_type_definition {
private:
  /*
  A method definition.
  */
  class a_method_def {
  public:
    a_method_def() : token_(mdMethodDefNil) {}

    a_method_def(mdMethodDef token, wstring &&name, DWORD attributes)
      : token_(token),
        name_(move(name)),
        attributes_(attributes)
    {
      check_assertion(!IsNilToken(token) &&
                      TypeFromToken(token) == mdtMethodDef);
    }  /* constructor */

    a_method_def(a_method_def&& other)
      : token_(move(other.token_)),
        name_(move(other.name_)),
        attributes_(move(other.attributes_))
    {
    }  /* constructor */

    a_method_def& operator=(a_method_def &&other)
    {
      token_ = move(other.token_);
      name_ = move(other.name_);
      attributes_ = move(other.attributes_);
      return *this;
    }  /* operator= */

    mdMethodDef token() const
    {
      return token_;
    }  /* token */

    an_accessibility accessibility() const
    {
      return exists() ? an_accessibility(token_, attributes_)
                      : an_accessibility();
    }  /* accessibility */

    DWORD attributes() const
    {
      return exists() ? attributes_ : 0;
    }

    bool exists() const
    {
      return !IsNilToken(token_);
    }  /* exists */

  private:
    mdMethodDef token_;
                        /* The token associated with this method. */
    wstring     name_;
                        /* The name of the method. */
    DWORD       attributes_;
                        /* The attributes for this method. */
  };  /* a_method_def */

public:
  a_type_definition(mdTypeDef                      typedef_token,
                    const wstring                  &full_type_name,
                    const wstring                  &type_name,
                    DWORD                          attributes,
                    mdToken                        extends_token,
                    const a_generic_parameter_list &generic_parameters,
                    an_import_scope                &import_scope,
                    a_top_level_kind               kind)
    : typedef_token_(typedef_token),
      type_name_(type_name),
      attributes_(attributes),
      extends_token_(extends_token),
      generic_parameters_(generic_parameters),
      import_scope_(import_scope),
      import_interface_(import_scope_.import_interface()),
      kind_(kind),
      is_system_string_type_(full_type_name == L"System.String"),
      is_first_base_class_processed(false)
  {
    a_cpp_cli_import_flag_set import_flags = import_scope_.
                                         containing_assembly().import_flags();

    import_as_friend_ = (import_flags & cpp_cli_as_friend_assembly) != 0;
  }  /* constructor */

  void import_definition(ostringstream& buffer);

private:
  void process_extends(ostringstream& buffer);
  void process_interfaces(ostringstream& buffer);
  void get_method_impls();
  void import_nested_classes(ostringstream& buffer);
  void import_all_methods(ostringstream &buffer);
  void import_one_method(ostringstream &buffer,
                         mdMethodDef   method_token,
                         DWORD         method_semantics);
  void import_all_fields(ostringstream &buffer);
  void import_one_field(ostringstream &buffer,
                        mdFieldDef    field_token);
  void import_properties(ostringstream& buffer);
  void import_one_property(ostringstream& buffer, mdProperty property_token);
  a_method_def import_property_method(mdMethodDef method_token);

  void import_events(ostringstream& buffer);
  void import_one_event(ostringstream& buffer, mdEvent event_token);
  a_method_def import_event_method(mdMethodDef method_token);

private:
  wstring get_overridden_name(mdToken member_token);

private:
  mdTypeDef     typedef_token_;
                        /* The mdTypeDef token for this type. */
  const wstring
                &type_name_;
                        /* The name of the type. */
  DWORD         attributes_;
                        /* The attributes for this type. */
  mdToken       extends_token_;
                        /* The token for the type that this type extends. */
  const a_generic_parameter_list&
                generic_parameters_;
                        /* The generic parameters associated with this
                           type. */
  an_import_scope
                &import_scope_;
                        /* The import scope in which this type is defined. */
  an_import_interface
                *import_interface_;
                        /* The appropriate IMetaDataImport2 interface. */
  a_top_level_kind
                kind_;  /* The top-level kind of this type. */
  a_boolean     is_system_string_type_;
                        /* TRUE if this type is System::String. */
  bool          is_first_base_class_processed;
                        /* True if the first case class has been processed. */
  multimap<mdToken, mdToken>
                method_impls_;
                        /* If this type definition is a class, this maps
                           a method body token (mdMethodDef or mdMemberRef)
                           for a virtual member function in this class to a
                           method declaration token (mdMethodDef or
                           mdMemberRef) for a virtual member function in a
                           base class.  It is used to indicate that the named
                           override syntax should be used in the
                           declaration of that method. */
  typedef multimap<mdToken, mdToken>::value_type a_method_impl_entry;
  bool          import_as_friend_;
                        /* True if this type should be imported as a
                           friend. */
};  /* a_type_definition */


void a_type_definition::process_extends(ostringstream& buffer)
/*
Decode the extends token and emit the appropriate text.
*/
{
  wstring extends_name = import_scope_.resolve_type_token(
                                                 extends_token_,
                                                 generic_parameters_,
                                                 no_generic_method_parameters,
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
  HCORENUM enum_interfaces = nullptr;
  mdToken  interfaces[8];
  ULONG    count_of_interfaces;
  HRESULT  hr;

  do {
    hr = import_interface_->EnumInterfaceImpls(&enum_interfaces,
                                               typedef_token_, interfaces,
                                               _countof(interfaces),
                                               &count_of_interfaces);
    CHECK_API_RESULT(hr, EnumInterfaceImpls);
    for (ULONG i = 0; i < count_of_interfaces; ++i) {
      wstring interface_name = import_scope_.resolve_type_token(
                                                 interfaces[i],
                                                 generic_parameters_,
                                                 no_generic_method_parameters,
                                                 /*replaces_dots=*/true);
      auto    back_tick = interface_name.find(L'`');
      if (is_first_base_class_processed) {
        buffer << ", ";
      } else {
        buffer << " : ";
        is_first_base_class_processed = true;
      }  /* if */
      buffer << interface_name;
    }  /* for */
  } while (count_of_interfaces > 0);
  import_interface_->CloseEnum(enum_interfaces);
}  /* a_type_definition::process_interfaces */


wstring name_from_method_semantics(DWORD method_semantics)
{
  wstring name;
  switch (method_semantics) {
    case msSetter:
      name = L"set";
      break;
    case msGetter:
      name = L"get";
      break;
    case msAddOn:
      name = L"add";
      break;
    case msRemoveOn:
      name = L"remove";
      break;
    case msFire:
      name = L"raise";
      break;
    case msOther:
    default:
      unexpected_condition();
      break;
  }  /* switch */
  return name;
}  /* name_from_method_semantics */


wstring a_type_definition::get_overridden_name(mdToken method_token)
/*
Get the name for the specified member for use in the overridden name list of
an override specifier.
*/
{
  HRESULT hr;
  mdToken parent_token;
  wstring method_name;
  DWORD   method_attributes;
  switch (TypeFromToken(method_token)) {
    case mdtMethodDef:
      { hr = import_interface_->GetMethodProps(method_token,
                                               &parent_token,
                                               method_name,
                                               &method_attributes,
                                               /*ppvSigBlob=*/nullptr,
                                               /*pcbSigBlob=*/nullptr,
                                               /*pulCodeRVA=*/nullptr,
                                               /*pdwImplFlags=*/nullptr);
        CHECK_API_RESULT(hr, GetMethodProps);
        if (IsMdSpecialName(method_attributes)) {
          /* Determine if this member is an event or property method. */
          HCORENUM           enum_method_semantics = NULL;
          static const ULONG max_tokens = 2;
          mdToken            tokens[max_tokens];
          ULONG              count_tokens = 2;
          hr = import_interface_->EnumMethodSemantics(&enum_method_semantics,
                                                      method_token,
                                                      tokens,
                                                      max_tokens,
                                                      &count_tokens);
          CHECK_API_RESULT(hr, EnumMethodSemantics);
          if (count_tokens == 1) {
            /* Get the name of the event or property. */
            wstring event_or_property_name;
            DWORD token_type = TypeFromToken(tokens[0]);
            switch (token_type) {
              case mdtEvent:
                hr = import_interface_->GetEventProps(tokens[0],
                                        /*pClass=*/nullptr,
                                        event_or_property_name,
                                        /*dwEventFlags=*/nullptr,
                                        /*tkEventType=*/nullptr,
                                        /*mdAddOn=*/nullptr,
                                        /*mdRemoveOn=*/nullptr,
                                        /*mdFire=*/nullptr,
                                        /*rmdOtherMethod=*/nullptr,
                                        /*cMax=*/0,
                                        /*pcOtherMethod=*/nullptr);
                CHECK_API_RESULT(hr, GetEventProps);
                break;
              case mdtProperty:
                 hr = import_interface_->GetPropertyProps(tokens[0],
                                           /*pClass=*/nullptr,
                                           event_or_property_name,
                                           /*pdwPropFlags=*/nullptr,
                                           /*ppvSig=*/nullptr,
                                           /*pbSig=*/nullptr,
                                           /*pdwCPlusTypeFlag=*/nullptr,
                                           /*ppDefaultValue=*/nullptr,
                                           /*pcchDefaultValue=*/nullptr,
                                           /*pmdSetter=*/nullptr,
                                           /*pmdGetter*/nullptr,
                                           /*rmdOtherMethod=*/nullptr,
                                           /*cMax=*/0,
                                           /*pcOtherMethod =*/nullptr);
                CHECK_API_RESULT(hr, GetPropertyProps);
                break;
              default:
                unexpected_condition();
                break;
            }  /* switch */
            check_assertion(!event_or_property_name.empty());
            /* Determine which kind of event or property method it is. */
            DWORD method_semantics;
            hr = import_interface_->GetMethodSemantics(method_token,
                                                       tokens[0],
                                                       &method_semantics);
            CHECK_API_RESULT(hr, GetMethodSemantics);
            method_name = event_or_property_name + L"::" +
                                 name_from_method_semantics(method_semantics);
          } else if (count_tokens == 0) {
            /* This member has a special name, but it is neither an event nor
               a property. */
            unexpected_condition();
          } else {
            /* A specific member should only map to a single property or
               event. */
            unexpected_condition();
          }  /* if */
        }  /* if */
      }  /* case mdtMethodDef */
      break;
    case mdtMemberRef:
      { hr = import_interface_->GetMemberRefProps(method_token, &parent_token,
                                                  method_name,
                                                  /*ppvSigBlob=*/nullptr,
                                                  /*pbSig=*/nullptr);
        CHECK_API_RESULT(hr, GetMemberRefProps);
        /* FIXME: There needs to be an extra level of indirection here to match
           a member function of an instantiated generic type back to the
           corresponding method in the generic type so that we can determine
           if it is the get/set/add/remove/raise method of a property/event.
           The following is a workaround for the lack of this functionality. */
        wstring::size_type method_name_index = 0;
        wstring::size_type last_dot_index = method_name.rfind(L'.');
        if (last_dot_index != wstring::npos) {
          method_name_index = last_dot_index + 1;
        }  /* if */
        if (method_name.compare(method_name_index,
                                _countof(L"get_")-1, L"get_") == 0) {
          method_name = method_name.substr(method_name_index +
                                              _countof(L"get_")-1) + L"::get";
        } else if (method_name.compare(method_name_index,
                                        _countof(L"set_")-1, L"set_") == 0) {
          method_name = method_name.substr(method_name_index +
                                              _countof(L"set_")-1) + L"::set";
        } else if (method_name.compare(method_name_index,
                                        _countof(L"add_")-1, L"add_") == 0) {
          method_name = method_name.substr(method_name_index +
                                              _countof(L"add_")-1) + L"::add";
        } else if (method_name.compare(method_name_index,
                                   _countof(L"remove_")-1, L"remove_") == 0) {
          method_name = method_name.substr(method_name_index +
                                        _countof(L"remove_")-1) + L"::remove";
        } else if (method_name.compare(method_name_index,
                                     _countof(L"raise_")-1, L"raise_") == 0) {
          method_name = method_name.substr(method_name_index +
                                          _countof(L"raise_")-1) + L"::raise";
        }  /* if */
        break;
      }  /* mdtMemberRef */
    default:
      unexpected_condition();
      break;
  }  /* switch */
  wstring type = import_scope_.resolve_type_token(
                                            parent_token, generic_parameters_,
                                            no_generic_method_parameters,
                                            /*replace_dots=*/true);
  return type + L"::" + method_name;
}  /* get_method_def_or_member_ref_name */


void a_type_definition::import_one_method(ostringstream &buffer,
                                          mdTypeDef     method_token,
                                          DWORD         method_semantics)
/*
Import a single member of a type.
*/
{
  bool                  omit_return_type = false;
  bool                  skip_member = false;
  HRESULT               hr;
  ULONG                 bytes_in_signature;
  DWORD                 method_attributes;
  PCCOR_SIGNATURE       signature;
  wstring               method_name;
  wostringstream        declaration;
  an_accessibility      accessibility;

  hr = import_interface_->GetMethodProps(method_token, /*pClass=*/nullptr,
                                         method_name, &method_attributes,
                                         &signature, &bytes_in_signature,
                                         /*pulCodeRVA=*/nullptr,
                                         /*pdwImplFlags=*/nullptr);
  CHECK_API_RESULT(hr, GetMethodProps);
  /* Get the accessibility.  Skip those methods that are not accessible. */
  accessibility = an_accessibility(method_token, method_attributes);
  skip_member = !accessibility.is_accessible();
  /* Handle special members methods such as constructors (both instance and
     class) and property accessor methods. */
  if (IsMdInstanceInitializerW(method_attributes, method_name.c_str()) ||
      IsMdClassConstructorW(method_attributes, method_name.c_str())) {
    method_name = type_name_;
    omit_return_type = true;
  } else if (method_name == (L"~" + type_name_) ||
             method_name == (L"!" + type_name_)) {
    /* A return type should not be emitted for destructors or finalizers. */
    omit_return_type = true;
  } else if (method_semantics != 0) {
    /* Any event or property method should be marked as having a special
       name. */
    check_assertion(IsMdSpecialName(method_attributes));
    method_name = name_from_method_semantics(method_semantics);
  } else if (!skip_member && IsMdSpecialName(method_attributes)) {
    skip_member = true;
  }  /* if  */
  if (!skip_member) {
    declaration << accessibility.get_string(import_as_friend_) << L": ";
    /* Decode the signature and create the appropriate declaration.  This
       can either be a method or a field. */
    a_signature_decoder decoder(import_scope_, method_token, signature,
                                bytes_in_signature, generic_parameters_,
                                no_generic_method_parameters,
                                is_system_string_type_);
    declaration << decoder.decode_method_signature(method_name,
                                                   method_attributes,
                                                   omit_return_type,
                                                   /*is_for_property=*/false);
    if (IsMdFinal(method_attributes)) {
      declaration << L" sealed";
    }  /* if */
    if (IsMdNewSlot(method_attributes)) {
      if (kind_ != tlk_interface) {
        declaration << L" new";
      }  /* if */
    } else if (IsMdVirtual(method_attributes)) {
      declaration << L" override";
    }  /* if */
    /* Emit any named overrides. */
    if (IsMdVirtual(method_attributes)) {
      auto method_impls_range = method_impls_.equal_range(method_token);
      for (auto method_impls_iterator = method_impls_range.first;
            method_impls_iterator != method_impls_range.second;
            ++method_impls_iterator) {
        if (method_impls_iterator == method_impls_range.first) {
            declaration << L" = ";
        } else {
            declaration << L", ";
        }  /* if */
        declaration << get_overridden_name(method_impls_iterator->second);
      }  /* for */
    }  /* if */
    buffer << declaration.str() << ';' << END_OF_LINE;
  }  /* if */
}  /* a_type_definition::import_one_member */


void a_type_definition::get_method_impls()
/*
Initialize method_impls_, which is used to determine any virtual member
functions that should use the named override syntax in its declaration to
indicate that it implements a base class virtual member function.
*/
{
  HRESULT            hr;
  HCORENUM           enum_method_impls = NULL;
  static const ULONG max_method_impls = 64;
  mdToken            method_bodies[max_method_impls];
  mdToken            method_decls[max_method_impls];
  ULONG              count_method_impls = 64;
  switch (kind_) {
    case tlk_ref_class:
    case tlk_value_type:
    case tlk_interface:
      do {
        hr = import_interface_->EnumMethodImpls(&enum_method_impls,
                                                typedef_token_,
                                                method_bodies,
                                                method_decls,
                                                max_method_impls,
                                                &count_method_impls);
        CHECK_API_RESULT(hr, EnumMethodImpls);
        for (ULONG i = 0; i < count_method_impls; ++i) {
          method_impls_.insert(a_method_impl_entry(method_bodies[i],
                                                   method_decls[i]));
        }  /* for */
      } while (count_method_impls > 0);
      import_interface_->CloseEnum(enum_method_impls);
      break;
    default:
      break;
  }  /* switch */
}  /* a_type_definition::get_method_impls */

void a_type_definition::import_all_methods(ostringstream &buffer)
/*
Import all the members associated with a single type.
*/
{
  HCORENUM    enum_methods = NULL;
  mdMethodDef methods[64];
  ULONG       count_of_methods;
  HRESULT     hr;

  get_method_impls();
  do {
    hr = import_interface_->EnumMethods(&enum_methods, typedef_token_,
                                        methods, _countof(methods),
                                        &count_of_methods);
    CHECK_API_RESULT(hr, EnumMethods);
    for (ULONG i = 0; i < count_of_methods; ++i) {
      import_one_method(buffer, methods[i], /*method_semantics=*/0);
    }  /* for */
  } while (count_of_methods > 0);
  import_interface_->CloseEnum(enum_methods);
}  /* a_type_definition::import_all_methods */


void a_type_definition::import_one_field(ostringstream &buffer,
                                         mdFieldDef    field_token)
/*
Import a single field of a type.
*/
{
  bool                      skip_member = false;
  HRESULT                   hr;
  ULONG                     bytes_in_signature;
  ULONG                     characters_in_constant;
  DWORD                     field_attributes, constant_type;
  PCCOR_SIGNATURE           signature;
  UVCP_CONSTANT             constant_value;
  wstring                   field_name;
  wostringstream            declaration;
  an_accessibility          accessibility;
  a_cpp_cli_import_flag_set import_flags;

  import_flags = import_scope_.containing_assembly().import_flags();
  hr = import_interface_->GetFieldProps(field_token, /*pClass=*/nullptr,
                                        field_name, &field_attributes,
                                        &signature, &bytes_in_signature,
                                        &constant_type, &constant_value,
                                        &characters_in_constant);
  CHECK_API_RESULT(hr, GetMemberProps);
  /* Get the accessibility.  Skip those fields that are not accessible. */
  accessibility = an_accessibility(field_token, field_attributes);
  skip_member = !accessibility.is_accessible();
  if (!skip_member && IsFdSpecialName(field_attributes)) {
    skip_member = true;
  }  /* if  */
  if (!skip_member) {
    declaration << accessibility.get_string(import_as_friend_) << L": ";
    if ((import_flags & cpp_cli_declspec_member_info) != 0) {
      declaration << L"__declspec(member_info(";
      declaration << L"0x" << setw(8) << setfill(L'0') << hex << field_token;
      declaration << L")) ";
    }  /* if */
    /* Decode the signature and create the appropriate declaration.  This
       can either be a method or a field. */
    a_signature_decoder decoder(import_scope_, field_token, signature,
                                bytes_in_signature, generic_parameters_,
                                no_generic_method_parameters,
                                is_system_string_type_);
    wstring field_type;

    if (IsFdInitOnly(field_attributes)) {
      declaration << L"initonly ";
    } else if (IsFdLiteral(field_attributes)) {
      declaration << L"literal ";
    }  /* if */
    field_type = decoder.decode_field_signature();
    /* Emit the storage class.  "literal" implies "static". */
    if (IsFdStatic(field_attributes) && !IsFdLiteral(field_attributes)) {
      declaration << L"static ";
    }  /* if */
    declaration << field_type << L' ';
    if (field_name.find(L'<') != wstring::npos) {
      /* Use the __identifier keyword. */
      declaration << L"__identifier(\"" << field_name << L"\")";
    } else {
      declaration << field_name;
    }  /* if */
    if (IsFdHasDefault(field_attributes)) {
      /* Note: we emit the value as a hexadecimal constant cast to the
         appropriate type.  This seems to work best for some corner
         cases. */
      a_constant_decoder decoder(constant_type, constant_value,
                                 characters_in_constant);
      if (constant_type == ELEMENT_TYPE_STRING) {
        /* FIXME: String literal conversions are not yet supported. */
        declaration << L" = nullptr /*" << decoder.decode() << L"*/";
      } else {
        declaration << L" = static_cast<" << field_type << L">("
                    << decoder.decode() << L')';
      }  /* if */
    }
    buffer << declaration.str() << ';' << END_OF_LINE;
  }  /* if */
}  /* a_type_definition::import_one_field */


void a_type_definition::import_all_fields(ostringstream &buffer)
/*
Import all the fields associated with a single type.
*/
{
  HCORENUM    enum_fields = NULL;
  mdFieldDef  fields[64];
  ULONG       count_of_fields;
  HRESULT     hr;

  do {
    hr = import_interface_->EnumFields(&enum_fields, typedef_token_,
                                        fields, _countof(fields),
                                        &count_of_fields);
    CHECK_API_RESULT(hr, EnumFields);
    for (ULONG i = 0; i < count_of_fields; ++i) {
      import_one_field(buffer, fields[i]);
    }  /* for */
  } while (count_of_fields > 0);
  import_interface_->CloseEnum(enum_fields);
}  /* a_type_definition::import_all_fields */


a_type_definition::a_method_def a_type_definition::import_property_method(
                                                     mdMethodDef method_token)
/*
Import the definition of a property accessor method.
*/
{
  HRESULT hr;
  wstring method_name;
  DWORD   attributes;

  hr = import_interface_->GetMethodProps(method_token, /*pClass=*/nullptr,
                                         method_name, &attributes,
                                         /*ppvSigBlob=*/nullptr,
                                         /*pcbSigBlob=*/nullptr,
                                         /*pulCodeRVA=*/nullptr,
                                         /*pdwImplFlags=*/nullptr);
  CHECK_API_RESULT(hr, GetMethodProps);
  return a_method_def(method_token, move(method_name), attributes);
}  /* a_type_definition::import_property_method */


void a_type_definition::import_one_property(ostringstream &buffer,
                                            mdProperty    property_token)
/*
Import the definition of single property - this includes both the property
itself and its associated accessor methods.
*/
{
  HRESULT             hr;
  ULONG               bytes_in_signature;
  DWORD               property_attributes;
  PCCOR_SIGNATURE     signature;
  mdMethodDef         set_method_token;
  mdMethodDef         get_method_token;
  wstring             property_name;
  a_method_def        set_method;
  a_method_def        get_method;
  an_accessibility    property_accessibility;

  hr = import_interface_->GetPropertyProps(property_token, /*pClass=*/nullptr,
                                           property_name,
                                           &property_attributes,
                                           &signature, &bytes_in_signature,
                                           /*pdwCPlusTypeFlag=*/nullptr,
                                           /*ppDefaultValue=*/nullptr,
                                           /*pcchDefaultValue=*/nullptr,
                                           &set_method_token,
                                           &get_method_token,
                                           /*rmdOtherMethod=*/nullptr,
                                           /*cMax=*/0,
                                           /*pcOtherMethod =*/nullptr);
  CHECK_API_RESULT(hr, GetPropertyProps);
  /* If there is a set method and/or a get method then import the necesssary
     information about the method. */
  if (!IsNilToken(set_method_token)) {
    set_method = import_property_method(set_method_token);
  }  /* if */
  if (!IsNilToken(get_method_token)) {
    get_method = import_property_method(get_method_token);
  }  /* if */
  /* Set the accessibility of the property itself.  The accessibility of the
     property itself is the wider accessibility of its associated accessor
     methods. */
  property_accessibility = an_accessibility::wider_accessibility(
                                                  get_method.accessibility(),
                                                  set_method.accessibility());
  if (property_accessibility.is_accessible()) {
    /* Now that we have everything we need emit the definition of the
       property. */
    ostringstream       declaration;
    a_signature_decoder decoder(import_scope_, property_token, signature,
                                bytes_in_signature, generic_parameters_,
                                no_generic_method_parameters,
                                is_system_string_type_);
    DWORD               method_attributes_mask = mdStatic;
    DWORD               method_attributes = 0;
    if (get_method.exists()) {
      method_attributes = get_method.attributes() & method_attributes_mask;
      if (set_method.exists()) {
        check_assertion(method_attributes ==
                        (set_method.attributes() & method_attributes_mask));
      }  /* if */
    } else if (set_method.exists()) {
      method_attributes = set_method.attributes() & method_attributes_mask;
    }  /* if */
    declaration << property_accessibility.get_string(
                                                   import_as_friend_) << ": ";
    declaration << decoder.decode_method_signature(
                                                 property_name,
                                                 method_attributes,
                                                 /*omit_return_type=*/false,
                                                 /*is_for_property=*/true);
    declaration << " {" << END_OF_LINE;
    /* Import the get and/or set method. */
    if (get_method.exists()) {
      import_one_method(declaration, get_method.token(), msGetter);
    }  /* if */
    if (set_method.exists()) {
      import_one_method(declaration, set_method.token(), msSetter);
    }  /* if */
    declaration << '}' << END_OF_LINE;
    buffer << declaration.str();
  }  /* if */
}  /* a_type_definition::import_one_property */


void a_type_definition::import_properties(ostringstream &buffer)
/*
Import all the properties associated with this type.
*/
{
  HCORENUM   enum_properties = nullptr;
  mdProperty properties[16];
  ULONG      count_of_properties;
  HRESULT    hr;

  do {
    hr = import_interface_->EnumProperties(&enum_properties,
                                           typedef_token_,
                                           properties,
                                           _countof(properties),
                                           &count_of_properties);
    CHECK_API_RESULT(hr, EnumProperties);
    for (ULONG i = 0; i < count_of_properties; ++i) {
      import_one_property(buffer, properties[i]);
    }  /* for */
  } while (count_of_properties > 0);
  import_interface_->CloseEnum(enum_properties);
}  /* a_type_definition::import_properties */


a_type_definition::a_method_def a_type_definition::import_event_method(
                                                     mdMethodDef method_token)
/*
Import the definition of an event method.
*/
{
  HRESULT hr;
  wstring method_name;
  DWORD   attributes;

  hr = import_interface_->GetMethodProps(method_token, /*pClass=*/nullptr,
                                         method_name, &attributes,
                                         /*ppvSigBlob=*/nullptr,
                                         /*pcbSigBlob=*/nullptr,
                                         /*pulCodeRVA=*/nullptr,
                                         /*pdwImplFlags=*/nullptr);
  CHECK_API_RESULT(hr, GetMethodProps);
  return a_method_def(method_token, move(method_name), attributes);
}  /* a_type_definition::import_event_method */


void a_type_definition::import_one_event(ostringstream  &buffer,
                                         mdEvent        event_token)
/*
Import the definition of single event - this includes both the event
itself and its associated methods.
*/
{
  HRESULT          hr;
  wstring          event_name;
  DWORD            event_attributes;
  mdToken          event_type_token;
  mdMethodDef      add_method_token, remove_method_token, raise_method_token;
  a_method_def     add_method, remove_method, raise_method;
  an_accessibility accessibility;
  DWORD            method_attributes;

  hr = import_interface_->GetEventProps(event_token, /*pClass=*/nullptr,
                                        event_name, &event_attributes,
                                        &event_type_token, &add_method_token,
                                        &remove_method_token,
                                        &raise_method_token,
                                        /*rmdOtherMethod=*/nullptr,
                                        /*cMax=*/0,
                                        /*pcOtherMethod=*/nullptr);
  CHECK_API_RESULT(hr, GetEventProps);
  check_assertion(!IsNilToken(add_method_token) &&
                  !IsNilToken(remove_method_token));
  /* If there is an add/remove/raise method then import the necesssary
     information about the method. */
  add_method = import_event_method(add_method_token);
  remove_method = import_event_method(remove_method_token);
  if (!IsNilToken(raise_method_token)) {
    raise_method = import_event_method(raise_method_token);
  }  /* if */
  check_assertion(add_method.accessibility() ==
                                              remove_method.accessibility() &&
                  (IsNilToken(raise_method_token) ||
                   add_method.accessibility() ==
                                               raise_method.accessibility()));
  accessibility = add_method.accessibility();
  DWORD method_attributes_mask = mdStatic | mdVirtual;
  check_assertion((add_method.attributes() & method_attributes_mask) ==
                      (remove_method.attributes() & method_attributes_mask) &&
                  (IsNilToken(raise_method_token) ||
                   (add_method.attributes() & method_attributes_mask) ==
                       (raise_method.attributes() & method_attributes_mask)));
  method_attributes = add_method.attributes();
  if (accessibility.is_accessible()) {
    /* Now that we have everything we need emit the definition of the
       event. */
    ostringstream declaration;
    wstring event_type;

    declaration << accessibility.get_string(import_as_friend_) << ": ";
    if (IsMdStatic(method_attributes)) {
      check_assertion(!IsMdVirtual(method_attributes));
      declaration << "static ";
    } else if (IsMdVirtual(method_attributes)) {
      declaration << "virtual ";
    }  /* if */
    declaration << "event ";
    event_type = import_scope_.resolve_type_token(
                                                 event_type_token,
                                                 generic_parameters_,
                                                 no_generic_method_parameters,
                                                 /*replaces_dots=*/true);
    declaration << event_type << "^ ";
    if (event_name.find(L'.') != wstring::npos) {
      /* The event name contains a dot; use __identifier to emit it. */
      declaration << "__identifier(\"" << event_name << "\")";
    } else {
      declaration << event_name;
    }  /* if */
    declaration << " {" << END_OF_LINE;
    import_one_method(declaration, add_method.token(), msAddOn);
    import_one_method(declaration, remove_method.token(), msRemoveOn);
    if (raise_method.exists()) {
      import_one_method(declaration, raise_method.token(), msFire);
    }  /* if */
    declaration << '}' << END_OF_LINE;
    buffer << declaration.str();
  }  /* if */
}  /* a_type_definition::import_one_event */


void a_type_definition::import_events(ostringstream &buffer)
/*
Import all the events associated with this type.
*/
{
  HCORENUM enum_events = nullptr;
  mdEvent  events[8];
  ULONG    count_of_events;
  HRESULT  hr;

  do {
    hr = import_interface_->EnumEvents(&enum_events, typedef_token_,
                                       events, _countof(events),
                                       &count_of_events);
    CHECK_API_RESULT(hr, EnumEvents);
    for (ULONG i = 0; i < count_of_events; ++i) {
      import_one_event(buffer, events[i]);
    }  /* for */
  } while (count_of_events > 0);
  import_interface_->CloseEnum(enum_events);
}  /* a_type_definition::import_events */


void a_type_definition::import_nested_classes(ostringstream &buffer)
/*
Import all the nested classes enclosed by this type.
*/
{
  HCORENUM  enum_typedefs = nullptr;
  mdTypeDef typedefs[64];
  ULONG     count_of_typedefs;

  do {
    HRESULT hr = import_interface_->EnumTypeDefs(&enum_typedefs, typedefs,
                                                 _countof(typedefs),
                                                 &count_of_typedefs);
    CHECK_API_RESULT(hr, EnumTypeDefs);
    for (ULONG i = 0; i < count_of_typedefs; ++i) {
      DWORD attributes;
      hr = import_interface_->GetTypeDefProps(
                                     typedefs[i], /*szTypeDef=*/nullptr,
                                     /*cchTypeDef=*/0, /*pchTypeDef=*/nullptr,
                                     &attributes, /*ptkExtends=*/nullptr);
      if (IsTdNested(attributes)) {
        an_accessibility accessibility(typedefs[i], attributes);
        mdTypeDef enclosing_typedef;
        hr = import_interface_->GetNestedClassProps(typedefs[i],
                                                    &enclosing_typedef);
        if (enclosing_typedef == typedef_token_ &&
            accessibility.is_accessible()) {
          buffer << accessibility.get_string(import_as_friend_) << ": ";
          import_scope_.import_one_type(buffer,
                                        typedefs[i],
                                        /*at_top_level=*/false,
                                        generic_parameters_,
                                        /*want_definition=*/false,
                                        /*class_body_only=*/false);
        }  /* if */
      }  /* if */
    }  /* for */
  } while (count_of_typedefs > 0);
  import_interface_->CloseEnum(enum_typedefs);
}  /* a_type_definition::import_nested_classes */


void a_type_definition::import_definition(ostringstream &buffer)
/*
Create the definition for the current type.
*/
{
  if (IsTdAbstract(attributes_)) {
    buffer << " abstract";
  }  /* if */
  if (IsTdSealed(attributes_)) {
    buffer << " sealed";
  }  /* if */
  process_extends(buffer);
  process_interfaces(buffer);
  buffer << " {" << END_OF_LINE;
  import_nested_classes(buffer);
  import_all_methods(buffer);
  import_all_fields(buffer);
  import_properties(buffer);
  import_events(buffer);
  buffer << "};";
#if DEBUG
  buffer << "  /* " << type_name_ << " */";
#endif /* DEBUG */
  buffer << END_OF_LINE;
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


wstring form_generic_parameter_list(
                       BYTE                            generic_arity,
                       a_generic_param_or_arg_iterator generic_parameters_end)
/*
Return a generic parameter list for a generic type or method.
*/
{
  wstring parameter_list;

  check_assertion(generic_arity > 0);
  parameter_list = L"generic<";
  for (auto iter = generic_parameters_end - generic_arity;
       iter != generic_parameters_end;
       ++iter) {
    const wstring &param_name = *iter;
    parameter_list += L"typename " + param_name;
    if (iter + 1 != generic_parameters_end)
      parameter_list += L", ";
  }  /* for */
  parameter_list += L'>';
  parameter_list += L_END_OF_LINE;
  return parameter_list;
}  /* form_generic_parameter_list */


wstring form_generic_constraint_clause_list(
                          const a_constraint_clause_list &generic_constraints)
/*
Return the constraint clause list for a generic type or method.
*/
{
  wstring constraint_clause_list;

  /* Append the generic constraints. */
  for (auto iter = generic_constraints.begin();
        iter != generic_constraints.end();
        ++iter) {
    const wstring &constraint_clause = *iter;
    constraint_clause_list += constraint_clause + L_END_OF_LINE;
  }  /* for */
  return constraint_clause_list;
}  /* form_generic_constraint_clause_list */


wstring form_generic_method_header(
                     BYTE                           generic_arity,
                     const a_generic_parameter_list &generic_parameters,
                     const a_constraint_clause_list &generic_constraints)
/*
Return the generic header associated with this generic method.
*/
{
  wstring generic_header;

  generic_header = form_generic_parameter_list(generic_arity,
                                               generic_parameters.end());
  if (!generic_constraints.empty()) {
    generic_header += form_generic_constraint_clause_list(
                                                         generic_constraints);
  }  /* if */
  return generic_header;
}  /* form_generic_method_header */


wstring an_import_scope::form_generic_type_header(
                    mdTypeDef                         typedef_token,
                    DWORD                             type_attributes,
                    BYTE                              generic_arity,
                    const a_generic_param_or_arg_list &generic_parameters,
                    a_generic_param_or_arg_iterator   generic_parameters_end,
                    const a_constraint_clause_list    &generic_constraints,
                    a_boolean                         out_of_class_definition)
/*
Return the generic header associated with this generic type.
*/
{
  wstring generic_header;

  /* When obtaining the definition of a type that is directly or indirectly a
     generic type, the generated code should be in the form of an
     out-of-class definition.  If this is a nested type and all of the generic
     parameters are not directly associated with it, then recursively generate
     the generic header for the enclosing generic type(s). */
  if (out_of_class_definition && IsTdNested(type_attributes) &&
      generic_arity < distance(generic_parameters.begin(),
                               generic_parameters_end)) {
    HRESULT   hr;
    mdTypeDef enclosing_typedef_token;
    wstring   enclosing_type_name;
    DWORD     enclosing_type_attributes;
    BYTE      enclosing_type_generic_arity;
    hr = import_interface_->GetNestedClassProps(typedef_token,
                                                &enclosing_typedef_token);
    CHECK_API_RESULT(hr, GetNestedClassProps);
    hr = import_interface_->GetTypeDefProps(enclosing_typedef_token,
                                            enclosing_type_name,
                                            &enclosing_type_attributes,
                                            /*ptkExtends=*/nullptr);
    CHECK_API_RESULT(hr, GetTypeDefProps);
    enclosing_type_generic_arity = strip_generic_arity(enclosing_type_name);
    generic_header = form_generic_type_header(
                                       enclosing_typedef_token,
                                       enclosing_type_attributes,
                                       enclosing_type_generic_arity,
                                       generic_parameters,
                                       generic_parameters_end - generic_arity,
                                       no_generic_constraints,
                                       out_of_class_definition);
  }  /* if */
  if (generic_arity > 0) {
    check_assertion(generic_arity <= distance(generic_parameters.begin(),
                                              generic_parameters_end));
    generic_header += form_generic_parameter_list(generic_arity,
                                                  generic_parameters_end);
    if (!generic_constraints.empty()) {
      generic_header += form_generic_constraint_clause_list(
                                                         generic_constraints);
    }  /* if */
  }  /* if */
  return generic_header;
}  /* an_import_scope::form_generic_type_header */


void an_import_scope::import_one_type(
                ostringstream                  &buffer,
                mdTypeDef                      typedef_token,
                bool                           at_top_level,
                const a_generic_parameter_list &enclosing_type_generic_params,
                bool                           want_definition,
                bool                           class_body_only)
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
  HRESULT                   hr;
  DWORD                     attributes;
  mdToken                   extends_token;
  wstring                   full_type_name;
  a_generic_parameter_list  this_type_generic_parameters;
  a_constraint_clause_list  generic_constraints;
  a_top_level_kind          kind;
  bool                      skip_type = false;
  a_cpp_cli_import_flag_set import_flags =
                                          containing_assembly_.import_flags();
  bool                      define_all_types =
                               (import_flags & cpp_cli_define_all_types) != 0;

  hr = import_interface_->GetTypeDefProps(typedef_token, full_type_name,
                                          &attributes, &extends_token);
  CHECK_API_RESULT(hr, GetTypeDefProps);
  BYTE generic_arity = strip_generic_arity(full_type_name);
  /* Get the generic parameters and constraints for this type if it is a
     generic type.  Also do this when obtaining the body of nested classes
     as they may be nested within a generic type. */
  if (generic_arity != 0 || (IsTdNested(attributes) && at_top_level &&
                             class_body_only)) {
    get_generic_parameters_and_constraints(typedef_token,
                                           no_generic_type_parameters,
                                           generic_arity,
                                           this_type_generic_parameters,
                                           generic_constraints);
  }  /* if */
  const a_generic_parameter_list &generic_type_parameters =
                                          this_type_generic_parameters.empty()
                                               ? enclosing_type_generic_params
                                               : this_type_generic_parameters;
  /* Check whether this type should be emitted based on the supported
     features. */
  if (IsTdNested(attributes)) {
    if (at_top_level && !class_body_only) {
      /* Do not emit nested types at top level scopes.  Nested types are
         emitted in their enclosing type. */
      skip_type = true;
    }  /* if */
  } else if (full_type_name[0] == L'<') {
    /* Skip types such as "<CrtImplementationDetails>" and
       "<CppImplementationDetails>". */
    skip_type = true;
  } else if (full_type_name == L"_GUID") {
    /* _GUID is a built-in type in Microsoft mode.  Skip it. */
    skip_type = true;
  }  /* if */
  if (!skip_type) {
    /* Classify the type - ref class, value class, interface etc. */
    kind = classify_type(full_type_name, attributes, generic_type_parameters,
                         extends_token);
    if (kind == tlk_delegate && !want_definition && !define_all_types) {
      /* If no definition is required, treat the delegate as a ref class
         since "delegate ..." is always a definition.  Doing so avoids
         declaration ordering problems. */
      kind = tlk_ref_class;
    }  /* if */
  }  /* if */
  if (!skip_type) {
    wstring::size_type last_dot_index;
    wstring            namespace_name;
    wstring            type_name;
    /* Split the full type name into a namespace and a type-name. */
    last_dot_index = full_type_name.rfind(L'.');
    check_assertion(last_dot_index != 0 &&
                    last_dot_index != full_type_name.length());
    if (last_dot_index != wstring::npos) {
      namespace_name = full_type_name.substr(0, last_dot_index);
    }  /* if */
    type_name = full_type_name.substr(last_dot_index + 1);
    if (type_name.find(L'<') != wstring::npos) {
      /* The type name contains angle brackets.  This is a template
         specialization.  For example, "Foo<int>."  Use __identifier to emit
         the type name. */
      type_name = wstring(L"__identifier(\"") + type_name + wstring(L"\")");
    }  /* if */
    /* Emit the namespace scopes and class head if required.  These are
       present on the original declaration and also on the definitions of
       generics. */
    if (!class_body_only || !generic_type_parameters.empty()) {
      if (at_top_level && !class_body_only) {
        /* Make sure that the correct namespace scopes are opened. */
        if (!namespace_name.empty()) {
          open_namespace_scopes(buffer, namespace_name);
        } else {
          close_all_namespace_scopes(buffer);
        }  /* if */
      }  /* if */
      /* Emit the generic header if this is directly or indirectly a generic
         type.  When obtaining the body of such a type, the generated code
         should be in the form of an out-of-class definition. */
      if (!generic_type_parameters.empty()) {
        a_boolean out_of_class_definition = at_top_level && class_body_only;
        buffer << form_generic_type_header(typedef_token, attributes,
                                           generic_arity,
                                           generic_type_parameters,
                                           generic_type_parameters.end(),
                                           generic_constraints,
                                           out_of_class_definition);
      }  /* if */
      /* Emit the assembly level visibility - either public or private. */
      if (at_top_level && want_definition) {
        bool import_as_friend = (import_flags &
                                             cpp_cli_as_friend_assembly) != 0;
        buffer << an_accessibility(typedef_token, attributes).
                                          get_string(import_as_friend) << ' ';
      }  /* if */
      /* Emit the tokens that represent the kind. */
      buffer << top_level_kind_as_wstring(kind) << ' ';
      /* Emit the assembly_info declspec. */
      if (!class_body_only &&
          (import_flags & cpp_cli_declspec_assemby_info) != 0) {
        buffer << "__declspec(assembly_info(0x";
        buffer << setw(8) << setfill('0') << hex <<
                                        containing_assembly_.assembly_index();
        buffer << ", 0x" << setw(8) << setfill('0') << hex << typedef_token;
        buffer << ")) ";
      }  /* if */
    }  /* if */
    if (class_body_only && kind == tlk_delegate &&
        generic_type_parameters.empty()) {
      /* Even when class_body_only is TRUE, the context-sensitive keyword
         "delegate" is needed so that a delegate class definition can be
         easily distinguished from a more traditional (managed) class
         definition.  (This is not needed in the case of a generic delegate
         since it will have */
      buffer << "delegate ";
    }  /* if  */
    if (kind == tlk_delegate) {
      import_delegate_definition(buffer, typedef_token, type_name,
                                 generic_type_parameters);
    } else if (kind == tlk_enumeration) {
      /* At the moment we can't forward declare a C++/CLI enumeration so we
         need to import (and emit) the full definition. */
      import_enum_definition(buffer, typedef_token, type_name,
                             generic_type_parameters);
    } else {
      /* Emit the name of the type. */
      if (!class_body_only || !generic_type_parameters.empty()) {
        /* Emit the enclosing type name qualifiers for definitions of nested
           generic types. */
        if (class_body_only && at_top_level && IsTdNested(attributes)) {
          mdTypeDef enclosing_typedef;
          hr = import_interface_->GetNestedClassProps(typedef_token,
                                                      &enclosing_typedef);
          CHECK_API_RESULT(hr, GetNestedClassProps);
          wstring enclosing_type_name = resolve_type_token(
                                enclosing_typedef,
                                generic_type_parameters,
                                generic_type_parameters.end() - generic_arity,
                                no_generic_method_parameters,
                                /*replace_dots=*/TRUE);
          buffer << enclosing_type_name << "::";
        }  /* if */
        buffer << type_name;
      }  /* if */
      if (want_definition || define_all_types) {
        a_type_definition type_definition(typedef_token, full_type_name,
                                          type_name, attributes,
                                          extends_token,
                                          generic_type_parameters,
                                          *this, kind);
        type_definition.import_definition(buffer);
      } else {
        buffer << ';' << END_OF_LINE;
      }  /* if */
      if (at_top_level && want_definition && !class_body_only) {
        close_all_namespace_scopes(buffer);
      }  /* if */
    }  /* if */
    if (!want_definition && !IsTdNested(attributes) && generic_arity == 0) {
      /* If this is for a declaration we need to remember the mapping from
         the def-token to the name as this will make it easier to find any
         future references to this token.  If this declaration is for a nested
         type, we can't cache the name because full_type_name does not include
         the enclosing class scopes.  If this declaration is for a generic
         type, we can't cache the name because the generic parameters may
         differ in different contexts. */
      map_of_tokens_to_names_.insert(make_pair(typedef_token,
                                               move(full_type_name)));
    }  /* if */
  }  /* if */
}  /* an_import_scope::import_one_type */


a_generic_argument_list a_signature_decoder::decode_generic_arguments()
/*
Decode the generic arguments associated with a type.  Note, it is the caller's
responsiblity to ensure that there is at least one generic argument.
*/
{
  a_generic_argument_list generic_arguments;
  BYTE count_of_generic_arguments = read_one_byte();

  check_assertion(count_of_generic_arguments > 0);
  generic_arguments.reserve(count_of_generic_arguments);
  for (BYTE i = 0; i < count_of_generic_arguments; ++i) {
    generic_arguments.push_back(decode_type());
  }  /* for */
  return generic_arguments;
}  /* a_signature_decoder::decode_generic_arguments */


wstring a_signature_decoder::decode_type(bool add_handle_to_class/*=true*/)
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
      if ((import_scope_.containing_assembly().import_flags()
                                         & cpp_cli_wchar_t_is_keyword) != 0) {
        buffer << L"wchar_t";
      } else {
        buffer << L"unsigned short";
      }  /* if */
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
      if (is_system_string_member_) {
        /* If we are decoding a type signature associated with a member of
           System::String, then we should perform the following conversions:
           ELEMENT_TYPE_PTR ELEMENT_TYPE_I1   -> 'const char*'
           ELEMENT_TYPE_PTR ELEMENT_TYPE_CHAR -> 'const wchar_t*' or
                                                 'const unsigned short*' */
        if (peek_element_type() == ELEMENT_TYPE_I1) {
          /* ELEMENT_TYPE_I1 would normally be converted to "signed char", but
             we only want "char", so we don't recursively call decode_type
             in this case. */
          (void)get_element_type();
          buffer << L"const char*";
          break;
        } else if (peek_element_type() == ELEMENT_TYPE_CHAR) {
          buffer << L"const ";
        }  /* if */
      }  /* if */
      /* Decode the pointee type and append a '*'. */
      buffer << decode_type() << L'*';
      break;
    case ELEMENT_TYPE_BYREF:
      /* Decode the referenced type and append a "%". */
      buffer << decode_type() << L'%';
      break;
    case ELEMENT_TYPE_VALUETYPE:
    case ELEMENT_TYPE_CLASS:
      { mdToken token = read_token();
        check_assertion(TypeFromToken(token) == mdtTypeDef ||
                        TypeFromToken(token) == mdtTypeRef);
        buffer << import_scope_.resolve_type_token(
                                       token,
                                       is_generic ? decode_generic_arguments()
                                                  : generic_type_parameters_,
                                       generic_method_parameters_,
                                       /*replaces_dots=*/true);
        break;
      }  /* case */
    case ELEMENT_TYPE_VAR:
      buffer << generic_type_parameters_[read_one_byte()];
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
             However, we need to consume these bytes in the signature
             blob. */
          (void)read_four_bytes();
        }  /* if */
        buffer << L">^";
        break;
      }  /* ELEMENT_TYPE_ARRAY */
    case ELEMENT_TYPE_MVAR:
      buffer << generic_method_parameters_[read_one_byte()];
      break;
    case ELEMENT_TYPE_CMOD_REQD:
      buffer << "/* CMOD_REQD ";
      buffer << import_scope_.resolve_type_token(read_token(),
                                                 generic_type_parameters_,
                                                 generic_method_parameters_,
                                                 /*replaces_dots=*/true);
      buffer << " */ ";
      buffer << decode_type();
      break;
    case ELEMENT_TYPE_CMOD_OPT:
      buffer << "/* CMOD_OPT ";
      buffer << import_scope_.resolve_type_token(read_token(),
                                                 generic_type_parameters_,
                                                 generic_method_parameters_,
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
  if (element_type == ELEMENT_TYPE_CLASS && add_handle_to_class) {
    buffer << L'^';
  }  /* if */
  return buffer.str();
}  /* a_signature_decoder::decode_type */


wstring a_signature_decoder::decode_method_signature(
                           const wstring                  &name,
                           DWORD                          method_attributes,
                           bool                           omit_return_type,
                           bool                           is_for_property)
/*
Decode a function signature and then combine the various elements along with
the name of the function to create a declaration which we return as a
std::wstring.  Note: this function also handles decoding a property signature
which is almost the same as a method signature.
*/
{
  BYTE           first_byte;
  BYTE           calling_convention;
  wostringstream declaration;
  ULONG          number_of_parameters;
  wstring        return_type;
  BYTE           generic_arity;

  /* The return type should never be omitted for property methods. */
  check_assertion(!omit_return_type || !is_for_property);
  first_byte = read_one_byte();
  calling_convention = first_byte & IMAGE_CEE_CS_CALLCONV_MASK;
  check_assertion(!is_for_property ||
                  (calling_convention == IMAGE_CEE_CS_CALLCONV_PROPERTY));
  /* If this is a generic method, read the count of generic parameters.
     Note that this will differ from generic_parameters_.size() for
     generic types or methods nested within generic types. */
  if ((first_byte & IMAGE_CEE_CS_CALLCONV_GENERIC) != 0) {
    generic_arity = read_one_byte();
    a_constraint_clause_list generic_method_constraints;
    check_assertion(generic_method_parameters_.empty());
    import_scope_.get_generic_parameters_and_constraints(
                                                  token_,
                                                  generic_type_parameters_,
                                                  generic_arity,
                                                  generic_method_parameters_,
                                                  generic_method_constraints);
    check_assertion(generic_arity == generic_method_parameters_.size());
    declaration << form_generic_method_header(generic_arity,
                                              generic_method_parameters_,
                                              generic_method_constraints);
  }  /* if */
  number_of_parameters = read_four_bytes();
  if ((import_scope_.containing_assembly().import_flags()
                                       & cpp_cli_declspec_member_info) != 0) {
    declaration << L"__declspec(member_info(";
    declaration << L"0x" << setw(8) << setfill(L'0') << hex << token_;
    declaration << L")) ";
  }  /* if */
  /* Emit any decl specifiers. */
  if (IsMdStatic(method_attributes)) {
    declaration << L"static ";
    check_assertion(!IsMdVirtual(method_attributes));
  } else if (IsMdVirtual(method_attributes)) {
    declaration << L"virtual ";
  }  /* if */
  if (is_for_property) declaration << "property ";
  return_type = decode_type();
  /* Constructors, destructors, and finalizers don't have a (visible) return
     type. */
  if (omit_return_type) {
    check_assertion(return_type == L"void");
  } else {
    declaration << return_type << L' ';
  }  /* if */
  if (name.find(L'.') != wstring::npos) {
    /* The type name contains a dot; use __identifier to emit it. */
    declaration << L"__identifier(\"" << name << L"\")";
  } else {
    declaration << name;
  }  /* if */
  /* We only emit parameters for methods and parameterized properties (and we
     use "[]" instead of "()" for parameterized properties).  Simple
     properties do not have any parameters. */
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


  an_assembly_index import_assembly(
                                char                      *assembly_full_name,
                                a_cpp_cli_import_flag_set import_flags,
                                bool                      *is_duplicated);
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
  an_import_interface *import_interface(an_assembly_index assembly_index)
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
                                char                      *assembly_full_name,
                                a_cpp_cli_import_flag_set import_flags,
                                bool                      *is_duplicated)
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
                                                        import_flags)));
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
                                            no_generic_type_parameters,
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
  an_import_interface             *import_interface;
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
    /* Re-write the header with the proper information now that it is
       known. */
    (void)fseek(f_pa, 0L, SEEK_SET);
    (void)fprintf(f_pa, PORTABLE_ASSEMBLY_TABLE_FORMAT,
                  header.magic, header.num_entries, header.table_offset);
    (void)fclose(f_pa);
  }  /* if */
}  /* create_portable_assembly */

#endif /* WRITE_CPPCLI_PORTABLE_ASSEMBLIES */

EXTERN_C_IN_CPP_FILE
an_assembly_index import_metadata_file(
                                char                      *assembly_full_name,
                                a_cpp_cli_import_flag_set import_flags,
                                a_boolean                 *is_duplicated)
/*
Import the assembly and return an unique assembly index.  *is_duplicated is
set to true if this assembly have been imported before.  If so, the previous
assembly will be returned.
*/
{
  an_assembly_index result;
  bool              is_dup= false;

  check_assertion(is_duplicated);
  check_assertion(metadata_reader->is_initialized());
  *is_duplicated = FALSE;
  /* Attempt to import this assembly. */
  result = metadata_reader->import_assembly(assembly_full_name, import_flags,
                                            &is_dup);
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
Import all the types defined in the specified assembly.  *buffer_size
describes the allocated size of *buffer.  If there is enough space, the
generated code is returned in *buffer and the amount of buffer used is
returned in *buffer_size.  Otherwise, *buffer is null terminated and
*buffer_size contains the required size.
*/
{
  string        str;
  ostringstream os;

  check_assertion(metadata_reader != nullptr);
  check_assertion(metadata_reader->is_initialized());
  metadata_reader->import_all_types(os, assembly_index);
  str = os.str();
  /* '+1' for the NULL terminator. */
  if (str.size() + 1 <= *buffer_size) {
    /* The buffer fits.  Copy it. */
    strcpy_s(buffer, *buffer_size, str.c_str());
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
a_cpp_cli_import_flag_set
                default_cpp_cli_import_flags = cpp_cli_declspec_assemby_info;

#endif /* CPPCLI_ENABLING_POSSIBLE */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2010-2011 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
