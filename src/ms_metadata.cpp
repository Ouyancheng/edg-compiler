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

This is a C++ file that relies on Microsoft Windows APIs.
As a result, it can only be compiled on a Windows platform.
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
#include <set>
#include <vector>
#include <stack>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <memory>
#include <locale>

EXTERN_C_BLOCK_IN_CPP_FILE
#include "fe_common.h"
END_EXTERN_C_BLOCK_IN_CPP_FILE

using namespace std;

#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define MAKE_CLASS_STRING(name) (L"System::" WIDEN(#name))
#define CLI_NAMESPACE (L"cli::")
#define DEFAULT_MEMBER_ATTRIBUTE L"System::Reflection::DefaultMemberAttribute"

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
  if (hr == CLDB_S_TRUNCATION || characters_required > characters_in_name) { \
    check_assertion(characters_required > characters_in_name); \
    name_buffer_heap.reset(new WCHAR[characters_required]); \
    name_buffer = name_buffer_heap.get(); \
    characters_in_name = characters_required; \
    hr = WRAPPED_FUNCTION; \
    check_assertion(hr != CLDB_S_TRUNCATION); \
  }  /* if */ \
  if (FAILED(hr) || characters_required == 0) { \
    name.clear(); \
  } else { \
    name.assign(name_buffer, characters_required - 1); \
  }  /* if */ \
  return hr; \
}


/*
A wrapper for the IMetaDataImport2 interface that facilitates invoking the
functions that require a string buffer of unknown length.
*/
class an_import_interface : public IMetaDataImport2
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
                          DWORD     *pdwTypeDefFlags,
                          mdToken   *ptkExtends = nullptr) {
    return GetTypeDefProps(td, /*szTypeDef=*/nullptr, /*cchTypeDef=*/0,
                           /*pchTypeDef=*/0, pdwTypeDefFlags,
                           ptkExtends);
  }  /* GetTypeDefProps */
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
                         DWORD           *pdwAttr,
                         PCCOR_SIGNATURE *ppvSigBlob,
                         ULONG           *pcbSigBlob,
                         ULONG           *pulCodeRVA,
                         DWORD           *pdwImplFlags) {
    return GetMethodProps(mb, pClass, /*szMethod=*/nullptr, /*cchMethod=*/0,
                          /*pchMethod=*/nullptr, pdwAttr, ppvSigBlob,
                          pcbSigBlob, pulCodeRVA, pdwImplFlags);
  }  /* GetMethodProps */
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
                            PCCOR_SIGNATURE *ppvSigBlob,
                            ULONG           *pbSig) {
    return GetMemberRefProps(mr, ptk, /*szMember=*/nullptr, /*cchMember=*/0,
                             /*pchMember=*/nullptr, ppvSigBlob, pbSig);
  }  /* GetMemberRefProps */
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
}; /* a_generic_parameter_info */

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
/* A list of generic types that have been declared with a pending constraint
   clause. */
typedef vector<mdTypeDef> a_pending_constraint_type_list;

void escape_invalid_identifier(wstring            &identifier,
                               bool               force = false,
                               wstring::size_type chars_to_skip = 0)
/*
If 'identifier' is not a valid C++ identifier, wrap it with
"__identifier("...")".  If chars_to_skip is nonzero, only wrap the portion
of the given string that starts at the position indicated by chars_to_skip.
If force is true, do the wrapping regardless of the contents of the string
(this is used in contexts where identifiers spelled like C++ keywords can
appear, because this function does not currently recognize such keywords).
*/
{
  if (identifier.length() > 0) {
    check_assertion(identifier.length() > chars_to_skip);
    auto skipped_chars_end = identifier.begin() + chars_to_skip;
    auto iter = skipped_chars_end;
    if (!force) {
      auto ch = *iter;
      if (ch == L'_' ||
          (ch >= L'a' && ch <= L'z') ||
          (ch >= L'A' && ch <= L'Z')) {
        for (++iter; iter != identifier.end(); ++iter) {
          ch = *iter;
          if (!(ch == L'_' ||
                (ch >= L'a' && ch <= L'z') ||
                (ch >= L'A' && ch <= L'Z') ||
                (ch >= L'0' && ch <= L'9'))) {
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    if (iter != identifier.end()) {
      /* At this point 'iter' addresses the first invalid character in
         'identifier'. */
      wstring escaped_identifier;
      escaped_identifier.reserve(sizeof("__identifier(\"")-1 + 
                                       identifier.length() + sizeof("\")")-1);
      /* Copy the skipped characters. */
      escaped_identifier.append(identifier.begin(), skipped_chars_end);
      escaped_identifier += L"__identifier(\"";
      /* Copy the characters that are known to be valid. */
      escaped_identifier.append(skipped_chars_end, iter);
      /* Copy the remaining characters, escaping special characters that
         aren't valid within an "__identifier("...")" construct. */
      for_each(iter, identifier.end(),
               [&](wstring::const_reference ch) {
                 switch (ch) {
                   case L'\\': escaped_identifier += L"\\\\"; break;
                   case L'\n': escaped_identifier += L"\\n";  break;
                   case L'\t': escaped_identifier += L"\\t";  break;
                   case L'\v': escaped_identifier += L"\\v";  break;
                   case L'\b': escaped_identifier += L"\\b";  break;
                   case L'\r': escaped_identifier += L"\\r";  break;
                   case L'\f': escaped_identifier += L"\\f";  break;
                   case L'\a': escaped_identifier += L"\\a";  break;
                   case L'"':  escaped_identifier += L"\\\""; break;
                   default:    escaped_identifier += ch;      break;
                 }  /* switch */
               });
      escaped_identifier += L"\")";
      identifier = move(escaped_identifier);
    }  /* if */
  }  /* if */
}  /* escape_invalid_identifier */


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


/*
A qualified name.
*/
class a_qualified_name {
public:
  a_qualified_name() {
  }  /* Default constructor */

  a_qualified_name(const a_qualified_name &other)
    : name_(other.name_)
    , separator_offsets_(other.separator_offsets_)
  {
  }  /* Copy constructor */

  a_qualified_name(a_qualified_name &&other) {
    other.swap(*this);
  }  /* Move constructor. */

  a_qualified_name &operator=(a_qualified_name other) {
    other.swap(*this);
    return *this;
  }  /* Assignment operator. */

  void swap(a_qualified_name &other) {
    std::swap(name_, other.name_);
    std::swap(separator_offsets_, other.separator_offsets_);
  }  /* swap */

  static a_qualified_name from_dotted_name(const wstring &dotted_name)
  /*
  Construct a qualified name from a dotted name, ensuring that every
  identifier is a valid C++ identifier.
  */
  {
    a_qualified_name   qualified_name;
    wstring::size_type start = 0;
    auto               dot_index = dotted_name.find(L'.', start);

    if (dot_index != wstring::npos) {
      while (dot_index != wstring::npos) {
        check_assertion(dot_index > start &&
                        dot_index < dotted_name.length() - 1);
        wstring identifier = dotted_name.substr(start, dot_index - start);
        qualified_name.append_identifier(move(identifier));
        start = dot_index + 1;
        dot_index = dotted_name.find(L'.', start);
      }  /* while */
      wstring identifier = dotted_name.substr(start);
      qualified_name.append_identifier(move(identifier));
    } else if (!dotted_name.empty()) {
      qualified_name.append_identifier(dotted_name);
    }  /* if */
    return qualified_name;
  }  /* from_dotted_name */

  const wstring &as_string() const {
    return name_;
  }  /* as_string */

  bool operator==(const wchar_t *other) const {
    return name_ == other;
  }  /* operator== */

  bool operator!=(const wchar_t *other) const {
    return name_ != other;
  }  /* operator!= */

  wstring unqualified_name() const
  /*
  Return the last identifier in the qualified name.
  */
  {
    wstring name;
    if (separator_offsets_.empty()) {
      name = name_;
    } else {
      name = name_.substr(separator_offsets_.back() + separator_length);
    }  /* if */
    return name;
  }  /* unqualified_name */

  a_qualified_name namespace_name() const
  /*
  Return the enclosing namespace name.
  */
  {
    a_qualified_name name;
    if (!separator_offsets_.empty()) {
      name.name_ = name_.substr(0, separator_offsets_.back());
      if (separator_offsets_.size() > 1) {
        name.separator_offsets_ = vector<wstring::size_type>(
                                                separator_offsets_.begin(),
                                                separator_offsets_.end() - 1);
      }  /* if */
    }  /* if */
    return name;
  }  /* namespace_name */

  bool empty() const {
    return name_.empty();
  }  /* empty */

  bool is_unqualified() const {
    return separator_offsets_.empty();
  }  /* is_unqualified */

  const vector<wstring::size_type> &separator_offsets() const {
    return separator_offsets_;
  }  /* separator_offsets */

  a_qualified_name &append_identifier(wstring identifier)
  /*
  Append an identifier to the qualified name, wrapping it with
  "__identifier("...")" if it is not a valid C++ identifier.
  */
  {
    check_assertion(!identifier.empty());
    escape_invalid_identifier(identifier);
    if (empty()) {
      name_ = identifier;
    } else {
      separator_offsets_.push_back(name_.length());
      name_ += separator;
      name_ += identifier;
    }  /* if */
    return *this;
  }  /* append_identifier */

  a_qualified_name &operator+=(const wstring &identifier) {
    return append_identifier(identifier);
  }  /* operator+= */

  a_qualified_name &append_qualified_name(const a_qualified_name &other)
  /*
  Append a qualified name to this one.
  */
  {
    if (empty() && !other.empty()) {
      *this = other;
    } else if (!other.empty()) {
      separator_offsets_.reserve(separator_offsets_.size() + 1 +
                                             other.separator_offsets_.size());
      /* Append the separator. */
      separator_offsets_.push_back(name_.length());
      name_ += separator;
      /* Append the other name, adjusting its separator offsets
         accordingly. */
      auto offset_adjustment = name_.length();
      for_each(other.separator_offsets_.begin(),
               other.separator_offsets_.end(),
               [&](const wstring::size_type &offset) {
                 separator_offsets_.push_back(offset + offset_adjustment);
               });
      name_ += other.name_;
    }  /* if */
    return *this;
  }  /* append_qualified_name */

  a_qualified_name &operator+=(const a_qualified_name &other) {
    return append_qualified_name(other);
  }  /* operator+= */

  void append_generic_params_or_args(
                 a_generic_param_or_arg_iterator generic_params_or_args_begin,
                 a_generic_param_or_arg_iterator generic_params_or_args_end)
  /*
  Append the generic parameters or arguments to the qualified name.
  */
  {
    check_assertion(!empty());
    check_assertion(distance(generic_params_or_args_begin,
                             generic_params_or_args_end) > 0);
    name_ += L'<';
    for (auto iter = generic_params_or_args_begin;
         iter != generic_params_or_args_end;
         ++iter) {
      const wstring &param_or_arg_name = *iter;
      name_ += param_or_arg_name;
      if (iter + 1 != generic_params_or_args_end) {
        name_ += L", ";
      }  /* if */
    }  /* for */
    name_ += L'>';
  }  /* append_generic_params_or_args */

  static const wchar_t *separator;
  static const auto separator_length = sizeof("::")-1;
private:
  wstring name_;        /* The fully-qualified name in C++ syntax. */
  vector<wstring::size_type> separator_offsets_;
                        /* The indices of the namespace separators. */
}; /* a_qualified_name */


const wchar_t *a_qualified_name::separator = L"::";

class a_type_wrapper;
typedef std::shared_ptr<a_type_wrapper> a_type_wrapper_ptr;
typedef std::shared_ptr<const a_type_wrapper> a_const_type_wrapper_ptr;
class a_class_type_wrapper;
typedef std::shared_ptr<a_class_type_wrapper> a_class_type_wrapper_ptr;
typedef std::shared_ptr<const a_class_type_wrapper>
                                               a_const_class_type_wrapper_ptr;
class an_array_type_wrapper;
typedef std::shared_ptr<an_array_type_wrapper> an_array_type_wrapper_ptr;
typedef std::shared_ptr<const an_array_type_wrapper>
                                               a_const_array_type_wrapper_ptr;
class a_type_indirection;
typedef std::shared_ptr<a_type_indirection> a_type_indirection_ptr;
typedef std::shared_ptr<const a_type_indirection>
                                                 a_const_type_indirection_ptr;
class a_function_type_wrapper;
typedef std::shared_ptr<a_function_type_wrapper> a_function_type_wrapper_ptr;
typedef std::shared_ptr<const a_function_type_wrapper>
                                            a_const_function_type_wrapper_ptr;

/*
A class that represents types imported from metadata.  Instances of the
a_type_wrapper class represent fundamental types.  Instances of derived
classes are used to represent other kinds of types.
*/
class a_type_wrapper
{
public:
  enum a_kind
  {
    twk_invalid,
    twk_void,
    twk_cxx_udt_return,
    twk_copy_ctor,
    twk_bool,
    twk_char,
    twk_signed_char,
    twk_unsigned_char,
    twk_short,
    twk_unsigned_short,
    twk_wchar_t,
    twk_int,
    twk_unsigned_int,
    twk_long,
    twk_unsigned_long,
    twk_long_long,
    twk_unsigned_long_long,
    twk_float,
    twk_double,
    twk_long_double,
    twk_class,
    twk_array,
    twk_indirection,
    twk_function
  };

  typedef unsigned char a_qualifier_flag_set;
  enum a_qualifier_flag : a_qualifier_flag_set
  {
    qf_none     = 0x0,
    qf_const    = 0x1,
    qf_volatile = 0x2
  };

  a_type_wrapper(a_kind kind)
    : kind_(kind)
    , qualifier_flags_(qf_none)
  {
  }  /* Constructor. */

  a_type_wrapper(const a_type_wrapper &other)
    : kind_(other.kind_)
    , qualifier_flags_(other.qualifier_flags_)
  {
  }  /* Copy constructor. */

  a_type_wrapper(a_type_wrapper &&other)
    : kind_(twk_invalid)
    , qualifier_flags_(qf_none)
  {
    other.swap(*this);
  }  /* Move constructor. */

  virtual ~a_type_wrapper()
  {
  }  /* Destructor. */

  a_type_wrapper &operator=(a_type_wrapper other) {
    other.swap(*this);
    return *this;
  }  /* Assignment operator. */

  void swap(a_type_wrapper &other) {
    std::swap(kind_, other.kind_);
    std::swap(qualifier_flags_, other.qualifier_flags_);
  }  /* swap */

  virtual a_type_wrapper_ptr copy() const {
    return make_shared<a_type_wrapper>(*this);
  }  /* copy */

  a_kind kind() const { return kind_; }
  void set_kind(a_kind kind) { kind_ = kind; }
  bool is_of_kind(a_kind kind) const { return kind_ == kind; }

  a_qualifier_flag_set qualifier_flags() const { return qualifier_flags_; }
  void set_qualifier_flags(a_qualifier_flag_set qualifier_flags) {
    qualifier_flags_ = qualifier_flags;
  } /* set_qualifier_flags */
  void add_qualifier_flags(a_qualifier_flag_set qualifier_flags) {
    qualifier_flags_ |= qualifier_flags;
  } /* set_qualifier_flags */

  virtual a_class_type_wrapper_ptr as_class() {
    check_assertion(!is_of_kind(twk_class));
    return nullptr;
  }  /* as_class */

  virtual a_const_class_type_wrapper_ptr as_class() const {
    check_assertion(!is_of_kind(twk_class));
    return nullptr;
  }  /* as_class */

  virtual an_array_type_wrapper_ptr as_array() {
    check_assertion(!is_of_kind(twk_array));
    return nullptr;
  }  /* as_array */

  virtual a_const_array_type_wrapper_ptr as_array() const {
    check_assertion(!is_of_kind(twk_array));
    return nullptr;
  }  /* as_array */

  virtual an_array_type_wrapper_ptr as_handle_to_array() {
    check_assertion(!is_of_kind(twk_indirection));
    return nullptr;
  }  /* as_handle_to_array */

  virtual a_const_array_type_wrapper_ptr as_handle_to_array() const {
    check_assertion(!is_of_kind(twk_indirection));
    return nullptr;
  }  /* as_handle_to_array */

  virtual a_type_indirection_ptr as_indirection() {
    check_assertion(!is_of_kind(twk_indirection));
    return nullptr;
  }  /* as_pointer */

  virtual a_const_type_indirection_ptr as_indirection() const {
    check_assertion(!is_of_kind(twk_indirection));
    return nullptr;
  }  /* as_pointer */

  virtual a_function_type_wrapper_ptr as_function() {
    check_assertion(!is_of_kind(twk_function));
    return nullptr;
  }  /* as_function */

  virtual a_const_function_type_wrapper_ptr as_function() const {
    check_assertion(!is_of_kind(twk_function));
    return nullptr;
  }  /* as_function */

  wstring get_string(const wstring &declarator = wstring()) const {
    wostringstream buffer;
    write_first_part(buffer);
    if (!declarator.empty()) {
      buffer << L' ' << declarator;
    }  /* if */
    write_second_part(buffer);
    return buffer.str();
  }  /* get_string */

  void write_qualifiers(wostringstream &buffer) const {
    if ((qualifier_flags_ & qf_const) != 0) {
      buffer << L"const";
    }  /* if */
    if ((qualifier_flags_ & qf_volatile) != 0) {
      buffer << L"volatile";
    }  /* if */
  }  /* write_qualifiers */

  virtual void write_first_part(wostringstream &buffer) const {
    if (qualifier_flags() != qf_none) {
      write_qualifiers(buffer);
      buffer << L' ';
    }  /* if */
    switch (kind_) {
      case twk_void:               buffer << L"void"; break;
      case twk_bool:               buffer << L"bool"; break;
      case twk_char:               buffer << L"char"; break;
      case twk_signed_char:        buffer << L"signed char"; break;
      case twk_unsigned_char:      buffer << L"unsigned char"; break;
      case twk_short:              buffer << L"short"; break;
      case twk_unsigned_short:     buffer << L"unsigned short"; break;
      case twk_wchar_t:            buffer << L"__wchar_t"; break;
      case twk_int:                buffer << L"int"; break;
      case twk_unsigned_int:       buffer << L"unsigned int"; break;
      case twk_long:               buffer << L"long"; break;
      case twk_unsigned_long:      buffer << L"unsigned long"; break;
      case twk_long_long:          buffer << L"long long"; break;
      case twk_unsigned_long_long: buffer << L"unsigned long long"; break;
      case twk_float:              buffer << L"float"; break;
      case twk_double:             buffer << L"double"; break;
      case twk_long_double:        buffer << L"long double"; break;
      default:
        unexpected_condition();
        buffer << L"__error_type";
        break;
    }  /* switch */
  }  /* write_first_part */

  virtual void write_second_part(wostringstream &buffer) const {
  }  /* write_second_part */

private:
  a_kind kind_;
  a_qualifier_flag_set qualifier_flags_;
};  /* a_type_wrapper */


/*
A class that represents types imported from metadata that aren't fundamental,
array, function, pointer, or reference types.
*/
class a_class_type_wrapper
  : public a_type_wrapper
  , public enable_shared_from_this<a_class_type_wrapper>
{
public:
  enum a_class_kind
  {
    ck_invalid,
    ck_class,
    ck_value_class,
    ck_generic_parameter
  };

  static a_class_type_wrapper_ptr create_system_class(
                                                   a_class_kind  kind,
                                                   const wstring &dotted_name)
  {
    a_qualified_name qualified_name;
    qualified_name.append_identifier(L"System");
    qualified_name.append_qualified_name(
                             a_qualified_name::from_dotted_name(dotted_name));
    return make_shared<a_class_type_wrapper>(kind, move(qualified_name));
  }  /* create_system_class*/

  a_class_type_wrapper(a_class_kind kind, a_qualified_name name)
    : a_type_wrapper(twk_class)
    , class_kind_(kind)
    , name_(move(name))
  {
  }  /* Constructor. */

  a_class_type_wrapper(const a_class_type_wrapper &other)
    : a_type_wrapper(other)
    , class_kind_(other.class_kind_)
    , name_(other.name_)
  {
  }  /* Copy constructor. */

  a_class_type_wrapper(a_class_type_wrapper &&other)
    : a_type_wrapper(twk_class)
    , class_kind_(ck_invalid)
  {
    other.swap(*this);
  }  /* Move constructor. */

  a_class_type_wrapper &operator=(a_class_type_wrapper other) {
    other.swap(*this);
    return *this;
  }  /* Assignment operator. */

  void swap(a_class_type_wrapper &other) {
    a_type_wrapper::swap(other);
    std::swap(class_kind_, other.class_kind_);
    std::swap(name_, other.name_);
  }  /* swap */

  virtual a_type_wrapper_ptr copy() const {
    return make_shared<a_class_type_wrapper>(*this);
  }  /* copy */

  a_class_kind class_kind() const { return class_kind_; }
  void set_class_kind(a_class_kind kind) { class_kind_ = kind; }
  bool is_of_class_kind(a_class_kind kind) const
  { return class_kind_ == kind; }

  const a_qualified_name &name() const { return name_; }
  void set_name(a_qualified_name name) { name_ = move(name); }

  virtual a_class_type_wrapper_ptr as_class() {
    check_assertion(is_of_kind(twk_class));
    return shared_from_this();
  }  /* as_class */

  virtual a_const_class_type_wrapper_ptr as_class() const {
    check_assertion(is_of_kind(twk_class));
    return shared_from_this();
  }  /* as_class */

  virtual void write_first_part(wostringstream &buffer) const {
    if (!is_of_class_kind(ck_invalid)) {
      if (qualifier_flags() != qf_none) {
        write_qualifiers(buffer);
        buffer << L' ';
      }  /* if */
      buffer << name_.as_string();
    } else {
      unexpected_condition();
      buffer << L"__error_type";
    }  /* if */
  }  /* write_first_part */

private:
  a_class_kind      class_kind_;
  a_qualified_name  name_;
};  /* a_class_type_wrapper */


/*
A class that represents pointer and reference types imported from metadata.
*/
class a_type_indirection
  : public a_type_wrapper
  , public enable_shared_from_this<a_type_indirection>
{
public:
  enum an_indirection_kind
  {
    tik_invalid,
    tik_pointer,
    tik_interior_pointer,
    tik_handle,
    tik_reference,
    tik_rvalue_reference,
    tik_tracking_reference
  };

  a_type_indirection(an_indirection_kind indirection_kind,
                     a_type_wrapper_ptr  underlying_type)
    : a_type_wrapper(twk_indirection)
    , indirection_kind_(indirection_kind)
    , underlying_type_(move(underlying_type))
  {
    check_assertion(underlying_type_ != nullptr);
  }  /* Constructor. */

  a_type_indirection(const a_type_indirection &other)
    : a_type_wrapper(other)
    , indirection_kind_(other.indirection_kind_)
    , underlying_type_(other.underlying_type_)
  {
  }  /* Copy constructor. */

  a_type_indirection(a_type_indirection &&other)
    : a_type_wrapper(twk_indirection)
    , indirection_kind_(tik_invalid)
  {
    other.swap(*this);
  }  /* Move constructor. */

  a_type_indirection &operator=(a_type_indirection other) {
    other.swap(*this);
    return *this;
  }  /* Assignment operator. */

  void swap(a_type_indirection &other) {
    a_type_wrapper::swap(other);
    std::swap(indirection_kind_, other.indirection_kind_);
    std::swap(underlying_type_, other.underlying_type_);
  }  /* swap */

  virtual a_type_wrapper_ptr copy() const {
    return make_shared<a_type_indirection>(*this);
  }  /* copy */

  an_indirection_kind indirection_kind() const { return indirection_kind_; }
  void set_indirection_kind(an_indirection_kind indirection_kind)
  { indirection_kind_ = indirection_kind; }
  bool is_of_indirection_kind(an_indirection_kind indirection_kind) const
  { return indirection_kind_ == indirection_kind; }

  a_type_wrapper_ptr underlying_type() {
    return underlying_type_;
  }  /* underlying_type */

  a_const_type_wrapper_ptr underlying_type() const {
    return underlying_type_;
  }  /* underlying_type */

  a_type_indirection_ptr as_indirection() {
    check_assertion(is_of_kind(twk_indirection));
    return shared_from_this();
  }  /* as_indirection */

  a_const_type_indirection_ptr as_indirection() const {
    check_assertion(is_of_kind(twk_indirection));
    return shared_from_this();
  }  /* as_indirection */

  virtual an_array_type_wrapper_ptr as_handle_to_array() {
    check_assertion(is_of_kind(twk_indirection));
    return is_of_indirection_kind(a_type_indirection::tik_handle) ?
                                      underlying_type()->as_array() : nullptr;
  }  /* as_handle_to_array */

  virtual a_const_array_type_wrapper_ptr as_handle_to_array() const {
    check_assertion(is_of_kind(twk_indirection));
    return is_of_indirection_kind(a_type_indirection::tik_handle) ?
                                      underlying_type()->as_array() : nullptr;
  }  /* as_handle_to_array */

  virtual void write_first_part(wostringstream &buffer) const {
    if (underlying_type_) {
      wstring kind_string;
      switch (indirection_kind_) {
        case tik_interior_pointer:
          if (!underlying_type_->is_of_kind(a_type_wrapper::twk_function) &&
              qualifier_flags() == qf_none) {
            buffer << L"interior_ptr<" << underlying_type_->get_string()
                   << L'>';
          } else {
            unexpected_condition();
            buffer << L"__error_type";
          }  /* if */
          break;
        case tik_pointer:            kind_string = L'*';  goto have_string;
        case tik_handle:             kind_string = L'^';  goto have_string;
        case tik_reference:          kind_string = L'&';  goto have_string;
        case tik_rvalue_reference:   kind_string = L"&&"; goto have_string;
        case tik_tracking_reference: kind_string = L'%';
have_string:
          underlying_type_->write_first_part(buffer);
          if (underlying_type_->is_of_kind(a_type_wrapper::twk_function)) {
            buffer << L" (";
          }  /* if */
          buffer << kind_string;
          if (qualifier_flags() != qf_none) {
            buffer << L' ';
            write_qualifiers(buffer);
          }  /* if */
          break;
        default:
          unexpected_condition();
          buffer << L"__error_type";
          break;
      }  /* switch */
    } else {
      unexpected_condition();
      buffer << L"__error_type";
    }  /* if */
  }  /* write_first_part */

  virtual void write_second_part(wostringstream &buffer) const {
    if (underlying_type_) {
      switch (indirection_kind_) {
        case tik_interior_pointer:
          break;
        case tik_pointer:
        case tik_handle:
        case tik_reference:
        case tik_rvalue_reference:
        case tik_tracking_reference:
          if (underlying_type_->is_of_kind(a_type_wrapper::twk_function)) {
            buffer << L")";
          }  /* if */
          underlying_type_->write_second_part(buffer);
          break;
        default:
          unexpected_condition();
          break;
      }  /* switch */
    } else {
      unexpected_condition();
    }  /* if */
  }  /* write_second_part */

private:
  an_indirection_kind indirection_kind_;
  a_type_wrapper_ptr  underlying_type_;
};  /* a_type_indirection */


/*
A class that represents arrays imported from metadata.
*/
class an_array_type_wrapper
  : public a_type_wrapper
  , public enable_shared_from_this<an_array_type_wrapper>
{
public:
  enum an_array_kind
  {
    ak_invalid,
    ak_array,
    ak_param_array,
  };

  static a_type_indirection_ptr create_handle_to_array(
                                    const a_type_wrapper_ptr &underlying_type,
                                    ULONG                    rank = 1,
                                    an_array_kind            kind = ak_array)
  {
    auto array_type = make_shared<an_array_type_wrapper>(underlying_type,
                                                         rank, kind);
    return make_shared<a_type_indirection>(a_type_indirection::tik_handle,
                                           move(array_type));
  }  /* create_handle_to_array */

  an_array_type_wrapper(a_type_wrapper_ptr underlying_type,
                        ULONG              rank = 1,
                        an_array_kind      kind = ak_array)
    : a_type_wrapper(twk_array)
    , array_kind_(kind)
    , underlying_type_(move(underlying_type))
    , rank_(rank)
  {
  }  /* Constructor. */

  an_array_type_wrapper(const an_array_type_wrapper &other)
    : a_type_wrapper(other)
    , array_kind_(other.array_kind_)
    , underlying_type_(other.underlying_type_)
    , rank_(other.rank_)
  {
  }  /* Copy constructor. */

  an_array_type_wrapper(an_array_type_wrapper &&other)
    : a_type_wrapper(twk_array)
    , array_kind_(ak_invalid)
  {
    other.swap(*this);
  }  /* Move constructor. */

  an_array_type_wrapper &operator=(an_array_type_wrapper other) {
    other.swap(*this);
    return *this;
  }  /* Assignment operator. */

  void swap(an_array_type_wrapper &other) {
    a_type_wrapper::swap(other);
    std::swap(array_kind_, other.array_kind_);
    std::swap(underlying_type_, other.underlying_type_);
    std::swap(rank_, other.rank_);
  }  /* swap */

  virtual a_type_wrapper_ptr copy() const {
    return make_shared<an_array_type_wrapper>(*this);
  }  /* copy */

  an_array_kind array_kind() const { return array_kind_; }
  void set_array_kind(an_array_kind kind) { array_kind_ = kind; }
  bool is_of_array_kind(an_array_kind kind) const
  { return array_kind_ == kind; }

  a_type_wrapper_ptr underlying_type() {
    return underlying_type_;
  }  /* underlying_type */

  a_const_type_wrapper_ptr underlying_type() const {
    return underlying_type_;
  }  /* underlying_type */

  UINT rank() const {
    return rank_;
  }  /* rank */

  virtual an_array_type_wrapper_ptr as_array() {
    check_assertion(is_of_kind(twk_array));
    return shared_from_this();
  }  /* as_array */

  virtual a_const_array_type_wrapper_ptr as_array() const {
    check_assertion(is_of_kind(twk_array));
    return shared_from_this();
  }  /* as_array */

  virtual void write_first_part(wostringstream &buffer) const {
    if (!is_of_array_kind(ak_invalid) && underlying_type_ != nullptr) {
      if (array_kind_ == ak_param_array) {
        buffer << L"... ";
      }  /* if */
      if (qualifier_flags() != qf_none) {
        write_qualifiers(buffer);
        buffer << L' ';
      }  /* if */
      switch (array_kind_) {
        case ak_array:
        case ak_param_array:
          buffer << L"cli::array<";
          break;
        default:
          unexpected_condition();
          buffer << L"__error_type<";
          break;
      }  /* switch */
      buffer << underlying_type_->get_string();
      if (rank_ > 1) {
        buffer << L", " << to_wstring((unsigned long long)rank_);
      }  /* if */
      buffer << L'>';
    } else {
      unexpected_condition();
      buffer << L"__error_type";
    }  /* if */
  }  /* write_first_part */

private:
  an_array_kind      array_kind_;
  a_type_wrapper_ptr underlying_type_;
  ULONG              rank_;
};  /* an_array_type_wrapper */


/*
The representation of a single method parameter.
*/
class a_method_parameter {
public:
  a_method_parameter()
    : token_(mdParamDefNil)
    , attributes_(0)
  {
  }  /* Default constructor. */

  a_method_parameter(const an_import_scope &import_scope,
                     a_type_wrapper_ptr type, mdParamDef token);

  a_method_parameter(a_method_parameter&& other)
    : token_(mdParamDefNil)
    , attributes_(0)
  {
    other.swap(*this);
  }  /* Move constructor. */

  a_method_parameter& operator=(a_method_parameter other) {
    other.swap(*this);
    return *this;
  }  /* Assignment operator. */

  void swap(a_method_parameter& other) {
    std::swap(type_, other.type_);
    std::swap(token_, other.token_);
    std::swap(name_, other.name_);
    std::swap(attributes_, other.attributes_);
  }  /* swap */

  wstring get_string() const {
    return type_->get_string(name_);
  }  /* get_string */

  a_type_wrapper_ptr       type() { return type_; }
  a_const_type_wrapper_ptr type() const { return type_; }
  mdParamDef token() const { return token_; }
  wstring    name() const { return name_; }
  DWORD      attributes() const { return attributes_; }
  a_boolean  is_parameter_array(const an_import_scope &import_scope) const;
private:
  a_type_wrapper_ptr type_;       /* The type of this parameter. */
  mdParamDef         token_;      /* The token for this parameter. */
  wstring            name_;       /* The name of this parameter. */
  DWORD              attributes_; /* Attributes associated with this
                                     parameter. */
}; /* a_method_parameter */


/* A list of method parameters. */
typedef vector<a_method_parameter> a_method_parameter_list;


/*
A class that represents function types imported from metadata.
*/
class a_function_type_wrapper
  : public a_type_wrapper
  , public enable_shared_from_this<a_function_type_wrapper>
{
public:
  a_function_type_wrapper(BYTE                    calling_convention,
                          a_type_wrapper_ptr      return_type,
                          a_method_parameter_list parameter_list,
                          wstring                 generic_header)
    : a_type_wrapper(twk_function)
    , calling_convention_(calling_convention)
    , return_type_(move(return_type))
    , parameter_list_(move(parameter_list))
    , generic_header_(move(generic_header))
  {
  }  /* Constructor. */

  a_function_type_wrapper(const a_function_type_wrapper &other)
    : a_type_wrapper(other)
    , calling_convention_(other.calling_convention_)
    , return_type_(other.return_type_)
    , parameter_list_(other.parameter_list_)
    , generic_header_(other.generic_header_)
  {
  }  /* Copy constructor. */

  a_function_type_wrapper(a_function_type_wrapper &&other)
    : a_type_wrapper(twk_function)
    , calling_convention_(IMAGE_CEE_CS_CALLCONV_DEFAULT)
  {
    other.swap(*this);
  }  /* Move constructor. */

  a_function_type_wrapper &operator=(a_function_type_wrapper other) {
    other.swap(*this);
    return *this;
  }  /* Assignment operator. */

  void swap(a_function_type_wrapper &other) {
    a_type_wrapper::swap(other);
    std::swap(calling_convention_, other.calling_convention_);
    std::swap(return_type_, other.return_type_);
    std::swap(parameter_list_, other.parameter_list_);
    std::swap(generic_header_, other.generic_header_);
  }  /* swap */

  virtual a_type_wrapper_ptr copy() const {
    return make_shared<a_function_type_wrapper>(*this);
  }  /* copy */

  BYTE calling_convention() const {
    return calling_convention_;
  }  /* calling_convention */

  a_const_type_wrapper_ptr return_type() const {
    return return_type_;
  }  /* underlying_type */

  const a_method_parameter_list &parameter_list() const {
    return parameter_list_;
  }  /* parameter_list */

  wstring generic_header() const { return generic_header_; }

  virtual a_function_type_wrapper_ptr as_function() {
    check_assertion(is_of_kind(twk_function));
    return shared_from_this();
  }  /* as_function */

  virtual a_const_function_type_wrapper_ptr as_function() const {
    check_assertion(is_of_kind(twk_function));
    return shared_from_this();
  }  /* as_function */

protected:
  virtual void write_first_part(wostringstream &buffer) const {
    if (return_type_ != nullptr) {
      buffer << return_type_->get_string();
    }  /* if */
#if 0
    /* FIXME: Calling conventions are not currently emitted. */
    switch (calling_convention_) {
      case IMAGE_CEE_CS_CALLCONV_C:
        buffer << L" __cdecl ";
        break;
      case IMAGE_CEE_CS_CALLCONV_STDCALL:
        buffer << L" __stdcall ";
        break;
      case IMAGE_CEE_CS_CALLCONV_THISCALL:
        buffer << L" __thiscall ";
        break;
      case IMAGE_CEE_CS_CALLCONV_FASTCALL:
        buffer << L" __fastcall ";
        break;
      default:
        break;
    }  /* switch */
#endif /* 0 */
  }  /* write_first_part */

  virtual void write_second_part(wostringstream &buffer) const {
    /* Open the parameter list. */
    if (calling_convention_ == IMAGE_CEE_CS_CALLCONV_PROPERTY) {
      check_assertion(!parameter_list_.empty());
      buffer << L'[';
    } else {
      buffer << L'(';
    }  /* if */
    /* Write the parameters. */
    auto iter = parameter_list_.begin();
    for (; iter != parameter_list_.end(); ++iter) {
      const a_method_parameter &parameter = *iter;
      if (iter != parameter_list_.begin()) {
        buffer << L", ";
      }  /* if */
      buffer << parameter.get_string();
    }  /* for */
    /* Write the vararg parameter. */
    if (calling_convention_ == IMAGE_CEE_CS_CALLCONV_VARARG) {
      if (iter != parameter_list_.begin()) {
        buffer << L", ";
      }  /* if */
      buffer << L"...";
    }  /* if */
    /* Close the parameter list. */
    if (calling_convention_ == IMAGE_CEE_CS_CALLCONV_PROPERTY) {
      buffer << L']';
    } else {
      buffer << L')';
    }  /* if */
    /* Close the parameter list. */
    if (qualifier_flags() != qf_none) {
      buffer << L' ';
      write_qualifiers(buffer);
    }  /* if */
  }  /* write_second_part */

private:
  BYTE                    calling_convention_;
  a_type_wrapper_ptr      return_type_;
  a_method_parameter_list parameter_list_;
  wstring                 generic_header_;
};  /* a_function_type_wrapper */


/*
A class used to decode data associated with a custom attribute.
*/
class a_custom_attribute_data
{
public:
  a_custom_attribute_data()
    : begin_(nullptr)
    , end_(nullptr)
  {
  }  /* Default constructor. */

  a_custom_attribute_data(const BYTE *data, ULONG bytes_in_data)
    : begin_(data)
    , end_(data + bytes_in_data)
  {
  }  /* Constructor. */

  a_custom_attribute_data(const a_custom_attribute_data& other)
    : begin_(other.begin_)
    , end_(other.end_)
  {
  }  /* Copy constructor. */

  a_custom_attribute_data(a_custom_attribute_data&& other)
  {
    other.swap(*this);
  }  /* Move constructor. */

  a_custom_attribute_data& operator=(a_custom_attribute_data other) {
    other.swap(*this);
    return *this;
  }  /* Assignment operator. */

  void swap(a_custom_attribute_data& other) {
    std::swap(begin_, other.begin_);
    std::swap(end_, other.end_);
  }  /* swap */

  template<typename T>
  ULONG read(T &value) const
  {
    ULONG size = sizeof(T);
    if (begin_ + size <= end_) {
      const T *value_ptr = reinterpret_cast<const T*>(begin_);
      value = *value_ptr;
    } else {
      unexpected_condition();
      size = 0;
    }  /* if */
    return size;
  }  /* read */

  ULONG read(shared_ptr<wstring> &value) {
    ULONG size = 0;
    BYTE next_byte;
    read(next_byte);
    if (next_byte == 0xFF) {
      /* A NULL string is encoded as a single byte. */
      size = 1;
    } else {
      PCCOR_SIGNATURE signature = begin_;
      ULONG string_length = CorSigUncompressData(signature);
      size = signature - begin_ + string_length;
      /* The utf8 string is not NULL terminated, so copy it to a string
         object. */
      auto utf8_string = string(reinterpret_cast<const char*>(signature),
                                string_length);
      value = make_shared<wstring>(conv_utf8_to_wchar(
                                     const_cast<char*>(utf8_string.c_str())));
    }  /* if */
    return size;
  }  /* read */

  ULONG read(CorSerializationType &type) {
    BYTE next_byte;
    read(next_byte);
    type = static_cast<CorSerializationType>(next_byte);
    return 1;
  }  /* read */

  void advance(ULONG byte_count) {
    check_assertion(begin_ + byte_count <= end_);
    begin_ += byte_count;
  }  /* advance */

  void advance(const a_const_type_wrapper_ptr &type) {
    ULONG size = 0;
    if (begin_ + size <= end_) {
      begin_ += size;
    } else {
      unexpected_condition();
    }  /* if */
  }  /* advance */

  template<typename T>
  void read_and_advance(T &value) {
    advance(read(value));
  }  /* read_and_advance */

  void read_and_advance(a_type_wrapper_ptr &type)
  {
    CorSerializationType serialization_type;

    read_and_advance(serialization_type);
    if (serialization_type == SERIALIZATION_TYPE_FIELD ||
        serialization_type == SERIALIZATION_TYPE_PROPERTY) {
      /* The CLI allows fields and properties to have the same name, so two
         serialization types are encoded in succession for named arguments.
         The first one provides a means to disambiguate them and the second
         one is the type of the field or property. */
      read_and_advance(serialization_type);
    }  /* if */
    switch (serialization_type) {
      case SERIALIZATION_TYPE_BOOLEAN:
        type = make_shared<a_type_wrapper>(a_type_wrapper::twk_bool);
        break;
      case SERIALIZATION_TYPE_CHAR:
        type = make_shared<a_type_wrapper>(a_type_wrapper::twk_char);
        break;
      case SERIALIZATION_TYPE_I1:
        type = make_shared<a_type_wrapper>(a_type_wrapper::twk_signed_char);
        break;
      case SERIALIZATION_TYPE_U1:
        type = make_shared<a_type_wrapper>(a_type_wrapper::twk_unsigned_char);
        break;
      case SERIALIZATION_TYPE_I2:
        type = make_shared<a_type_wrapper>(a_type_wrapper::twk_short);
        break;
      case SERIALIZATION_TYPE_U2:
        type = make_shared<a_type_wrapper>(
                                          a_type_wrapper::twk_unsigned_short);
        break;
      case SERIALIZATION_TYPE_I4:
        type = make_shared<a_type_wrapper>(a_type_wrapper::twk_int);
        break;
      case SERIALIZATION_TYPE_U4:
        type = make_shared<a_type_wrapper>(a_type_wrapper::twk_unsigned_int);
        break;
      case SERIALIZATION_TYPE_I8:
        type = make_shared<a_type_wrapper>(a_type_wrapper::twk_long_long);
        break;
      case SERIALIZATION_TYPE_U8:
        type = make_shared<a_type_wrapper>(
                                      a_type_wrapper::twk_unsigned_long_long);
        break;
      case SERIALIZATION_TYPE_R4:
        type = make_shared<a_type_wrapper>(a_type_wrapper::twk_float);
        break;
      case SERIALIZATION_TYPE_R8:
        type = make_shared<a_type_wrapper>(a_type_wrapper::twk_double);
        break;
      case SERIALIZATION_TYPE_STRING:
        { /* Create the System::String^ wrapper. */
          auto string_type = a_class_type_wrapper::create_system_class(
                                   a_class_type_wrapper::ck_class, L"String");
          type = make_shared<a_type_indirection>(
                                               a_type_indirection::tik_handle,
                                               move(string_type));
          break;
        }
      case SERIALIZATION_TYPE_SZARRAY:
        { /* The argument is an array type.  Determine is underlying type. */
          a_type_wrapper_ptr underlying_type;
          read_and_advance(underlying_type);
          /* Create the System::Array^ wrapper. */
          type = an_array_type_wrapper::create_handle_to_array(
                                                             underlying_type);
          break;
        }
      case SERIALIZATION_TYPE_TYPE:
        { /* Create the System::Type^ wrapper. */
          auto type_type = a_class_type_wrapper::create_system_class(
                                     a_class_type_wrapper::ck_class, L"Type");
          type = make_shared<a_type_indirection>(
                                               a_type_indirection::tik_handle,
                                               move(type_type));
          break;
        }
      case SERIALIZATION_TYPE_TAGGED_OBJECT:
        { /* Create the System::Object^ wrapper. */
          auto object_type = a_class_type_wrapper::create_system_class(
                                   a_class_type_wrapper::ck_class, L"Object");
          type = make_shared<a_type_indirection>(
                                               a_type_indirection::tik_handle,
                                               move(object_type));
        }
      case SERIALIZATION_TYPE_ENUM:
        { shared_ptr<wstring> type_name;
          read_and_advance(type_name);
          check_assertion(type_name != nullptr);
          /* FIXME: Attribute arguments of Enum type are not yet supported.
             We need to resolve the enum type name and create a type wrapper
             for it which includes its name and underlying type. */
          unexpected_condition();
          type.reset();
          break;
        }
        break;
      case SERIALIZATION_TYPE_FIELD:
      case SERIALIZATION_TYPE_PROPERTY:
      default:
        unexpected_condition();
        type.reset();
        break;
    }  /* switch */
  }  /* serialization_type_to_type. */

  bool empty() { return begin_ == end_; }

private:
  const BYTE    *begin_;
                        /* A pointer to the current location in the data that
                           encodes the arguments to the custom attribute. */
  const BYTE    *end_;  /* A pointer to the end of the data. */
};  /* a_custom_attribute_data */


class an_attribute_argument;
typedef const an_attribute_argument a_const_attribute_argument;
typedef vector<a_const_attribute_argument> an_attribute_argument_list;
typedef const an_attribute_argument_list a_const_attribute_argument_list;
typedef std::shared_ptr<an_attribute_argument_list>
                                               an_attribute_argument_list_ptr;
typedef std::shared_ptr<a_const_attribute_argument_list>
                                          a_const_attribute_argument_list_ptr;

/*
A single argument to a custom attribute.
*/
class an_attribute_argument
{
public:
  an_attribute_argument()
    : serialization_type_(SERIALIZATION_TYPE_UNDEFINED)
  {
  }  /* Default constructor. */

  an_attribute_argument(a_custom_attribute_data  &data,
                        a_const_type_wrapper_ptr type,
                        wstring                  name = wstring())
    : type_(move(type))
    , serialization_type_(SERIALIZATION_TYPE_UNDEFINED)
    , name_(move(name))
  {
    check_assertion(type_ != nullptr && !data.empty());
    if (type_->kind() == a_type_wrapper::twk_indirection) {
      auto indirection = type_->as_indirection();
      if (indirection != nullptr &&
          indirection->is_of_indirection_kind(
                                        a_type_indirection::tik_handle)) {
        auto underlying_type = indirection->underlying_type();
        if (underlying_type != nullptr) {
          if (underlying_type->kind() == a_type_wrapper::twk_class) {
            auto class_type = indirection->underlying_type()->as_class();
            if (class_type->name() == MAKE_CLASS_STRING(String)) {
              /* The argument is a System::String^. */
              data.read_and_advance(string_value_);
              serialization_type_ = SERIALIZATION_TYPE_STRING;
            } else if (class_type->name() == MAKE_CLASS_STRING(Type)) {
              /* The argument is a System::Type^. */
              shared_ptr<wstring> type_name;
              data.read_and_advance(type_name);
              check_assertion(type_name != nullptr);
              /* FIXME: System::Type^ attribute arguments are not yet
                 supported.  We need to resolve the type name and create a
                 'typeid' string for it (stored in string_value_). */
              unexpected_condition();
              serialization_type_ = SERIALIZATION_TYPE_TYPE;
            } else if (class_type->name() == MAKE_CLASS_STRING(Object)) {
              /* The argument is a boxed value type. */
              a_type_wrapper_ptr boxed_type;
              data.read_and_advance(boxed_type);
              auto init_list = make_shared<an_attribute_argument_list>();
              init_list->push_back(an_attribute_argument(data, boxed_type));
              init_list_ = move(init_list);
              serialization_type_ = SERIALIZATION_TYPE_TAGGED_OBJECT;
            } else {
              unexpected_condition();
            }  /* if */
          } else if (underlying_type->kind() == a_type_wrapper::twk_array) {
            /* The argument is an array type.  Determine is underlying
               type. */
            auto array_type = underlying_type->as_handle_to_array();
            check_assertion(array_type->underlying_type() != nullptr);
            /* Determine how many elements are contained in the array. */
            USHORT array_size;
            data.read_and_advance(array_size);
            if (array_size == static_cast<USHORT>(-1)) {
              /* The array is NULL as opposed to empty. */
            } else {
              auto init_list = make_shared<an_attribute_argument_list>();
              init_list->reserve(array_size);
              for (USHORT index = 0; index < array_size; ++index) {
                init_list->push_back(an_attribute_argument(data,
                                                           underlying_type));
              }  /* for */
              init_list_ = move(init_list);
            }  /* if */
            serialization_type_ = SERIALIZATION_TYPE_SZARRAY;
          } else {
            unexpected_condition();
          }  /* if */
        } else {
          unexpected_condition();
        }  /* if */
      } else {
        unexpected_condition();
      }  /* if */
    } else {
      /* The parameter is a value type. */
      switch (type_->kind()) {
        case a_type_wrapper::twk_bool:
          data.read_and_advance(value_.bool_value);
          serialization_type_ = SERIALIZATION_TYPE_BOOLEAN;
          break;
        case a_type_wrapper::twk_char:
          data.read_and_advance(value_.char_value);
          serialization_type_ = SERIALIZATION_TYPE_CHAR;
          break;
        case a_type_wrapper::twk_signed_char:
          data.read_and_advance(value_.signed_char_value);
          serialization_type_ = SERIALIZATION_TYPE_I1;
          break;
        case a_type_wrapper::twk_unsigned_char:
          data.read_and_advance(value_.unsigned_char_value);
          serialization_type_ = SERIALIZATION_TYPE_U1;
          break;
        case a_type_wrapper::twk_short:
          data.read_and_advance(value_.short_value);
          serialization_type_ = SERIALIZATION_TYPE_I2;
          break;
        case a_type_wrapper::twk_unsigned_short:
        case a_type_wrapper::twk_wchar_t:
          data.read_and_advance(value_.unsigned_short_value);
          serialization_type_ = SERIALIZATION_TYPE_U2;
          break;
        case a_type_wrapper::twk_int:
        case a_type_wrapper::twk_long:
          data.read_and_advance(value_.int_value);
          serialization_type_ = SERIALIZATION_TYPE_I4;
          break;
        case a_type_wrapper::twk_unsigned_int:
        case a_type_wrapper::twk_unsigned_long:
          data.read_and_advance(value_.unsigned_int_value);
          serialization_type_ = SERIALIZATION_TYPE_U4;
          break;
        case a_type_wrapper::twk_long_long:
          data.read_and_advance(value_.long_long_value);
          serialization_type_ = SERIALIZATION_TYPE_I8;
          break;
        case a_type_wrapper::twk_unsigned_long_long:
          data.read_and_advance(value_.unsigned_long_long_value);
          serialization_type_ = SERIALIZATION_TYPE_U8;
          break;
        case a_type_wrapper::twk_float:
          data.read_and_advance(value_.float_value);
          serialization_type_ = SERIALIZATION_TYPE_R4;
          break;
        case a_type_wrapper::twk_double:
        case a_type_wrapper::twk_long_double:
          data.read_and_advance(value_.double_value);
          serialization_type_ = SERIALIZATION_TYPE_R8;
          break;
        case a_type_wrapper::twk_class:
          { auto class_type = type_->as_class();
            if (class_type != nullptr &&
                class_type->is_of_class_kind(
                                      a_class_type_wrapper::ck_value_class)) {
              /* FIXME: Arguments of Enum type are not yet supported.  We need
                 to know its underlying integer type to know its size.  To fix
                 this, we'll need to create a new type wrapper for enum types
                 which provides access to its name and underlying type. */
              unexpected_condition();
              serialization_type_ = SERIALIZATION_TYPE_ENUM;
            } else {
              unexpected_condition();
            }  /* if */
            break;
          }
        default:
          unexpected_condition();
          break;
      }  /* switch */
    }  /* if */
  }  /* Constructor. */

  an_attribute_argument(const an_attribute_argument& other)
    : type_(other.type_)
    , serialization_type_(other.serialization_type_)
    , value_(other.value_)
    , string_value_(other.string_value_)
    , init_list_(other.init_list_)
    , name_(other.name_)
  {
  }  /* Copy constructor. */

  an_attribute_argument(an_attribute_argument&& other)
    : serialization_type_(SERIALIZATION_TYPE_UNDEFINED)
  {
    other.swap(*this);
  }  /* Move constructor. */

  an_attribute_argument& operator=(an_attribute_argument other) {
    other.swap(*this);
    return *this;
  }  /* Assignment operator. */

  void swap(an_attribute_argument& other) {
    std::swap(type_, other.type_);
    std::swap(serialization_type_, other.serialization_type_);
    std::swap(value_, other.value_);
    std::swap(string_value_, other.string_value_);
    std::swap(init_list_, other.init_list_);
    std::swap(name_, other.name_);
  }  /* swap */

  const a_const_type_wrapper_ptr &type() const { return type_; }
  const wstring &name() const { return name_; }

  CorSerializationType serialization_type() const {
    return serialization_type_;
  }  /* serialization_type */

#define DEFINE_GET_VALUE(NAME, TYPE, SERIALIZATION_TYPE) const \
  bool NAME(TYPE &value) {                                       \
    bool result = serialization_type_ == (SERIALIZATION_TYPE);   \
    if (result) {                                                \
      value = value_.NAME;                                       \
    }  /* if */                                                  \
    return result;                                               \
  }

  DEFINE_GET_VALUE(bool_value, bool, SERIALIZATION_TYPE_BOOLEAN);
  DEFINE_GET_VALUE(char_value, char, SERIALIZATION_TYPE_CHAR);
  DEFINE_GET_VALUE(signed_char_value, signed char, SERIALIZATION_TYPE_I1);
  DEFINE_GET_VALUE(unsigned_char_value, unsigned char,
                   SERIALIZATION_TYPE_U1);
  DEFINE_GET_VALUE(short_value, short, SERIALIZATION_TYPE_I2);
  DEFINE_GET_VALUE(unsigned_short_value, unsigned short,
                   SERIALIZATION_TYPE_U2);
  DEFINE_GET_VALUE(int_value, int, SERIALIZATION_TYPE_I4);
  DEFINE_GET_VALUE(unsigned_int_value, unsigned int, SERIALIZATION_TYPE_U4);
  DEFINE_GET_VALUE(long_long_value, long long, SERIALIZATION_TYPE_I8);
  DEFINE_GET_VALUE(unsigned_long_long_value, unsigned long long,
                   SERIALIZATION_TYPE_U8);
  DEFINE_GET_VALUE(float_value, float, SERIALIZATION_TYPE_R4);
  DEFINE_GET_VALUE(double_value, double, SERIALIZATION_TYPE_R8);

#undef DEFINE_GET_VALUE

  a_const_attribute_argument_list_ptr array_init_list() const {
    return serialization_type_ == SERIALIZATION_TYPE_SZARRAY ? init_list_
                                                             : nullptr;
  }  /* array_init_list */

  a_const_attribute_argument *boxed_argument() const {
    a_const_attribute_argument *argument = nullptr;
    if (serialization_type_ == SERIALIZATION_TYPE_TAGGED_OBJECT) {
      check_assertion(init_list_->size() == 1);
      argument = &init_list_->front();
    }  /* if */
    return argument;
  }  /* boxed_argument */

  bool string_value(shared_ptr<const wstring> &value) const
  {
    bool result = serialization_type_ == SERIALIZATION_TYPE_STRING;
    if (result) {
      value = string_value_;
    }  /* if */
    return result;
  }  /* string_value */

  bool typeid_value(wstring &value) const
  {
    bool result = serialization_type_ == SERIALIZATION_TYPE_TYPE;
    if (result) {
      check_assertion(string_value_ != nullptr);
      value = *string_value_;
    }  /* if */
    return result;
  }  /* typeid_value */

private:
  typedef union {
    bool               bool_value;
    char               char_value;
    signed char        signed_char_value;
    unsigned char      unsigned_char_value;
    short              short_value;
    unsigned short     unsigned_short_value;
    int                int_value;
    unsigned int       unsigned_int_value;
    long long          long_long_value;
    unsigned long long unsigned_long_long_value;
    float              float_value;
    double             double_value;
  } an_attribute_value;

  a_const_type_wrapper_ptr
                type_;
                        /* The type of the argument. */
  CorSerializationType
                serialization_type_;
                        /* The serialization type of the argument. */
  an_attribute_value
                value_; /* If the argument is a simple value type, this
                           union contains the argument's value. */
  shared_ptr<wstring>
                string_value_;
                        /* If the argument is of System::String^ or
                           System::Type^ type, the string value for the
                           argument, or nullptr for a NULL string as opposed
                           to an empty one. */
  an_attribute_argument_list_ptr
                init_list_;
                        /* If the argument is an array type, the list of array
                           initializers, or nullptr for a NULL array as
                           opposed to an empty one.  If the argument is of
                           System::Object^ type, the single initializer for
                           the unboxed argument. */
  wstring       name_;
                        /* The argument's name if it is a named argument, or
                           the empty string if it is a fixed argument. */
};  /* an_attribute_argument */


class a_custom_attribute;
typedef vector<const a_custom_attribute> a_custom_attribute_list;
typedef const a_custom_attribute_list a_const_custom_attribute_list;

/*
The representation of a single custom attribute.
*/
class a_custom_attribute {
public:
  a_custom_attribute()
    : import_scope_(nullptr)
    , token_(mdCustomAttributeNil)
    , ctor_token_(mdTokenNil)
    , signature_(nullptr)
    , bytes_in_signature_(0)
  {
  }  /* Default constructor. */

  a_custom_attribute(an_import_scope   *import_scope,
                     mdCustomAttribute token)
    : import_scope_(import_scope)
    , token_(token)
    , ctor_token_(mdTokenNil)
    , signature_(nullptr)
    , bytes_in_signature_(0)
  {
  }  /* Constructor. */

  a_custom_attribute(const a_custom_attribute &other)
    : import_scope_(other.import_scope_)
    , token_(other.token_)
    , ctor_token_(other.ctor_token_)
    , signature_(other.signature_)
    , bytes_in_signature_(other.bytes_in_signature_)
    , data_(other.data_)
    , type_name_(other.type_name_)
    , fixed_args_(other.fixed_args_)
    , named_args_(other.named_args_)
  {
  }  /* Copy constructor. */

  a_custom_attribute(a_custom_attribute&& other)
    : import_scope_(nullptr)
    , token_(mdCustomAttributeNil)
    , ctor_token_(mdTokenNil)
    , signature_(nullptr)
    , bytes_in_signature_(0)
  {
    other.swap(*this);
  }  /* Move constructor. */

  a_custom_attribute& operator=(a_custom_attribute other) {
    other.swap(*this);
    return *this;
  }  /* Assignment operator. */

  void swap(a_custom_attribute& other) {
    std::swap(import_scope_, other.import_scope_);
    std::swap(token_, other.token_);
    std::swap(ctor_token_, other.ctor_token_);
    std::swap(signature_, other.signature_);
    std::swap(bytes_in_signature_, other.bytes_in_signature_);
    std::swap(data_, other.data_);
    std::swap(type_name_, other.type_name_);
    std::swap(fixed_args_, other.fixed_args_);
    std::swap(named_args_, other.named_args_);
  }  /* swap */

  a_const_attribute_argument_list &fixed_args() const {
    decode_fixed_args();
    return *fixed_args_;
  } /* fixed_args */

  a_const_attribute_argument_list &named_args() const {
    decode_named_args();
    return *named_args_;
  } /* named_args */

  mdCustomAttribute token() const { return token_; }
  mdToken ctor_token() const { decode_type(); return ctor_token_; }
  a_qualified_name type_name() const { decode_type(); return type_name_; }

private:
  an_import_interface *import_interface() const;
  void decode_type() const;
  void decode_fixed_args() const;
  void decode_named_args() const;

  an_import_scope
                *import_scope_;
                        /* The import scope associated with this attribute. */
  mdCustomAttribute
                token_;
                        /* The token of the custom attribute. */
  mutable mdToken
                ctor_token_;
                        /* The mdMethodDef or mdMemberRef token of the custom
                           attribute's constructor. */
  mutable PCCOR_SIGNATURE 
                signature_;
                        /* The signature of the custom attribute
                           constructor. */
  mutable ULONG bytes_in_signature_;
                        /* The number of bytes in the signature of the custom
                           attribute constructor. */
  mutable a_custom_attribute_data
                data_;
                        /* The data that encodes custom attribute's
                           arguments. */
  mutable a_qualified_name
                type_name_;
                        /* The name of the custom attribute class type. */
  mutable an_attribute_argument_list_ptr
                fixed_args_;
                        /* The fixed arguments to the custom attribute's
                           constructor. */
  mutable an_attribute_argument_list_ptr
                named_args_;
                        /* The custom attribute's named arguments. */
};  /* a_custom_attribute */


class a_custom_attribute_processor;
typedef const a_custom_attribute_processor a_const_custom_attribute_processor;
typedef shared_ptr<a_custom_attribute_processor>
                                             a_custom_attribute_processor_ptr;
typedef shared_ptr<const a_custom_attribute_processor>
                                       a_const_custom_attribute_processor_ptr;

/*
A class that handles obtaining the list of custom attributes for a given
token and extracting information from them as required.
*/
class a_custom_attribute_processor {
public:
  a_custom_attribute_processor()
    : import_scope_(nullptr)
    , token_(mdTokenNil)
  {
  }  /* Default constructor. */

  a_custom_attribute_processor(an_import_scope *import_scope,
                               mdToken         token)
    : import_scope_(import_scope)
    , token_(token)
  {
    HRESULT           hr;
    HCORENUM          enum_custom_attributes = nullptr;
    mdCustomAttribute custom_attributes[16];
    ULONG             count_of_custom_attributes;

    do {
      hr = import_interface()->EnumCustomAttributes(
                                                &enum_custom_attributes,
                                                token_,
                                                /*tkType=*/0,
                                                custom_attributes,
                                                _countof(custom_attributes),
                                                &count_of_custom_attributes);
      CHECK_API_RESULT(hr, EnumProperties);
      for (ULONG i = 0; i < count_of_custom_attributes; ++i) {
        a_custom_attribute custom_attribute(import_scope_,
                                            custom_attributes[i]);
        process_attribute(custom_attribute);
        custom_attributes_.emplace_back(custom_attribute);
      }  /* for */
    } while (count_of_custom_attributes > 0);
    import_interface()->CloseEnum(enum_custom_attributes);
  }  /* Constructor. */

  a_custom_attribute_processor(const a_custom_attribute_processor &other)
    : import_scope_(other.import_scope_)
    , token_(other.token_)
    , custom_attributes_(other.custom_attributes_)
    , default_member_name_(other.default_member_name_)
  {
  }  /* Copy constructor. */

  a_custom_attribute_processor(a_custom_attribute_processor&& other)
    : import_scope_(nullptr)
    , token_(mdTokenNil)
  {
    other.swap(*this);
  }  /* Move constructor. */

  a_custom_attribute_processor& operator=(a_custom_attribute_processor other) {
    other.swap(*this);
    return *this;
  }  /* Assignment operator. */

  void swap(a_custom_attribute_processor& other) {
    std::swap(import_scope_, other.import_scope_);
    std::swap(token_, other.token_);
    std::swap(custom_attributes_, other.custom_attributes_);
    std::swap(default_member_name_, other.default_member_name_);
  }  /* swap */

  mdToken token() const { return token_; }

  a_const_custom_attribute_list &custom_attributes() const {
    return custom_attributes_;
  } /* custom_attributes */

  const wstring &default_member_name() const {
    return default_member_name_;
  }  /* default_member_name */

private:
  void process_attribute(const a_custom_attribute &custom_attribute) {
    if (TypeFromToken(token_) == mdtTypeDef) {
      /* Process the DefaultMemberAttribute. */
      if (custom_attribute.type_name() == DEFAULT_MEMBER_ATTRIBUTE) {
        check_assertion(custom_attribute.fixed_args().size() == 1);
        shared_ptr<const wstring> default_member_name;
        if (custom_attribute.fixed_args().front().string_value(
                                                       default_member_name)) {
          if (default_member_name != nullptr) {
            default_member_name_ = *default_member_name;
          }  /* if */
        } else {
          unexpected_condition();
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* process_attribute */

  an_import_interface *import_interface() const;

  an_import_scope
                *import_scope_;
                        /* The import scope associated with this custom
                           attribute processor. */
  mdToken       token_;
                        /* The token associated with the custom attributes. */
  a_custom_attribute_list
                custom_attributes_;
                        /* The list of custom attributes. */
  wstring
                default_member_name_;
                        /* The string passed to the DefaultMemberAttribute
                           constructor. */
};  /* a_custom_attribute_processor */


/*
A class to decode a CLR signature.  It returns the result as a std::wstring.
*/
class a_signature_decoder {
public:
  a_signature_decoder(
                   an_import_scope                &import_scope,
                   PCCOR_SIGNATURE                signature,
                   ULONG                          bytes_in_signature,
                   const a_generic_parameter_list &generic_type_parameters,
                   const a_generic_parameter_list &generic_method_parameters,
                   a_boolean                      is_system_string_member)
    : import_scope_(import_scope),
      signature_(signature),
      bytes_in_signature_(bytes_in_signature),
      generic_type_parameters_(generic_type_parameters),
      generic_method_parameters_(generic_method_parameters),
      is_system_string_member_(is_system_string_member),
      index_(0),
      contains_unknown_optional_type_modifiers_(false)
  {
  }  /* constructor */

  a_type_wrapper_ptr decode_modified_type(CorElementType element_type);

  a_type_wrapper_ptr decode_type();

  wstring decode_return_type(mdToken token) {
    auto return_type = decode_method_signature(token,
                                               /*return_type_only=*/true);
    return return_type != nullptr ? return_type->get_string()
                                  : L"__error_type";
  }  /* decode_return_type */
  
  a_type_wrapper_ptr decode_method_signature(
                                         mdProperty token,
                                         bool       return_type_only = false);

  a_type_wrapper_ptr decode_field_signature()
  {
    skip_calling_convention(IMAGE_CEE_CS_CALLCONV_FIELD);
    return decode_type();
  }  /* decode_field_signature */

  a_boolean contains_unknown_optional_type_modifiers()
  {
    return contains_unknown_optional_type_modifiers_;
  }  /* contains_unknown_optional_type_modifiers */

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
  an_import_scope
                &import_scope_;
                        /* The import scope associated with this signature. */
  const PCCOR_SIGNATURE
                signature_;
                        /* The signature we want to decode. */
  const ULONG   bytes_in_signature_;
                        /* The number of bytes in the signature. */
  const a_generic_parameter_list
                &generic_type_parameters_;
                        /* Any generic type parameters associated with this
                           signature. */
  a_generic_parameter_list
                generic_method_parameters_;
                        /* Any generic method parameters associated with this
                           signature. */
  a_boolean     is_system_string_member_;
                        /* TRUE if this signature is for a member of
                           System::String. */
  ULONG         index_;
                        /* The current index into the signature. */
  a_boolean     contains_unknown_optional_type_modifiers_;
                        /* TRUE if this signature contains any unknown
                           optional type modifiers (modopts). */
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
    case ELEMENT_TYPE_CLASS:
      /* The nullptr constant. */
      check_assertion(convert_to<unsigned int>(constant_value_) == 0);
      buffer << L"nullptr";
      break;
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
a_type_definition forward declaration.
*/
class a_type_definition;

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
      active_namespace_(move(other.active_namespace_)),
      map_of_tokens_to_names_(move(other.map_of_tokens_to_names_)),
      custom_attribute_processors_(move(other.custom_attribute_processors_))
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
                const a_type_definition        *enclosing_type_definition,
                bool                           want_definition,
                bool                           class_body_only,
                a_pending_constraint_type_list *pending_constraint_types);

private:
  a_qualified_name resolve_typedef_token(
       mdTypeDef                         token,
       const a_generic_param_or_arg_list &generic_type_params_or_args,
       a_generic_param_or_arg_iterator   generic_type_params_or_args_end,
       bool                              omit_generic_params_or_args = false);
  a_qualified_name resolve_typeref_token(
              mdTypeRef                         token,
              const a_generic_param_or_arg_list &generic_type_params_or_args);
public:
  a_qualified_name resolve_type_token(
      mdToken                           token,
      const a_generic_param_or_arg_list &generic_type_params_or_args,
      const a_generic_param_or_arg_list &generic_method_params_or_args,
      bool                              omit_generic_params_or_args = false);

  BYTE get_generic_parameter_count(mdTypeDef token) const;
  BYTE get_generic_parameters_and_constraints(
           mdToken                        token,
           const a_generic_parameter_list &generic_type_parameters_for_method,
           BYTE                           enclosing_type_generic_params_count,
           a_generic_parameter_list       &generic_parameters,
           a_constraint_clause_list       &generic_constraints,
           a_pending_constraint_type_list *pending_constraint_types);
  void get_generic_constraints(
      mdToken                             token,
      const a_generic_parameter_list      &generic_type_parameters_for_method,
      BYTE                                generic_arity,
      const a_generic_parameter_info_list &generic_parameters_info,
      const a_generic_parameter_list      &generic_parameters,
      a_constraint_clause_list            &generic_constraints,
      a_pending_constraint_type_list      *pending_constraint_types);
  wstring form_full_generic_parameter_list(
                mdTypeDef                       typedef_token,
                DWORD                           type_attributes,
                a_generic_param_or_arg_iterator generic_parameters_begin,
                a_generic_param_or_arg_iterator generic_parameters_end) const;
  wstring form_generic_type_header(
        mdTypeDef                         typedef_token,
        DWORD                             type_attributes,
        BYTE                              generic_arity,
        mdTypeDef                         enclosing_type_token,
        DWORD                             enclosing_type_attributes,
        const a_generic_param_or_arg_list &generic_parameters,
        const a_constraint_clause_list    &generic_constraints,
        a_boolean                         out_of_class_definition) const;
  void cleanup();

  a_const_custom_attribute_processor_ptr get_custom_attribute_processor(
                                                              mdToken token) {
    a_custom_attribute_processor_ptr processor;

    auto iter = custom_attribute_processors_.find(token);
    if (iter != custom_attribute_processors_.end()) {
      /* We found a previously cached processor. */
      processor = iter->second;
    } else {
      /* This is the first request for a processor for this token. */
      processor = make_shared<a_custom_attribute_processor>(this, token);
      custom_attribute_processors_[token] = processor;
    }  /* if */
    return processor;
  }  /* get_custom_attribute_processor */

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

  void set_namespace_scope(ostringstream    &buffer,
                           a_qualified_name namespace_name);
  void open_namespace(ostringstream           &buffer,
                      wstring::const_iterator namespace_begin,
                      wstring::const_iterator namespace_end);
  void close_namespace(ostringstream           &buffer,
                       wstring::const_iterator namespace_begin,
                       wstring::const_iterator namespace_end);
  void close_all_namespace_scopes(ostringstream &buffer) {
    set_namespace_scope(buffer, a_qualified_name());
  }  /* close_all_namespace_scopes */

private:
  wstring       scope_name_;
                        /* The name of this scope. */
  an_assembly   &containing_assembly_;
                        /* The assembly that contains this scope. */
  an_import_interface
                *import_interface_;
                        /* The IMetaDataImport2 interface. */
  a_qualified_name
                active_namespace_;
                        /* The stack of active namespaces. */
  map<mdToken, a_qualified_name>
                map_of_tokens_to_names_;
                        /* A mapping from a mdToken to the name of the type:
                           useful when we want the name of a type we have
                           already imported. */
  map<mdToken, a_custom_attribute_processor_ptr>
                custom_attribute_processors_;
                        /* A mapping from an mdToken to its corresponding
                           custom attribute processor. */
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

  import_interface_->AddRef();
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
  HCORENUM                       enum_typedefs = nullptr;
  mdTypeDef                      typedefs[64];
  ULONG                          count_of_typedefs;
  auto                           import_flags =
                                         containing_assembly_.import_flags();
  bool                           use_pending_constraint_clauses;
  a_pending_constraint_type_list pending_constraint_types;

  /* Only use the pending constraint clause for generic types if we're not
     defining all types.  Doing so wouldn't have any benefit because the
     constraint clause on nested generic types or methods may refer to
     other types that have not been imported. */
  use_pending_constraint_clauses =
                               (import_flags & cpp_cli_define_all_types) == 0;
  do {
    HRESULT hr = import_interface_->EnumTypeDefs(&enum_typedefs, typedefs,
                                                 _countof(typedefs),
                                                 &count_of_typedefs);

    CHECK_API_RESULT(hr, EnumTypeDefs);
    for (ULONG i = 0; i < count_of_typedefs; ++i) {
      import_one_type(buffer, typedefs[i],
                      /*enclosing_type_definition=*/nullptr,
                      /*want_definition=*/false,
                      /*class_body_only=*/false,
                      use_pending_constraint_clauses ?
                                         &pending_constraint_types : nullptr);
    }  /* for */
  } while (count_of_typedefs > 0);
  import_interface_->CloseEnum(enum_typedefs);
  /* Now that all types have been imported, re-declare all generic types that
     were declared with a pending constraint clause, this time with the
     complete constraint clause. */
  for (auto pending_constraint_types_iter = pending_constraint_types.begin();
       pending_constraint_types_iter != pending_constraint_types.end();
       ++pending_constraint_types_iter) {
    import_one_type(buffer, *pending_constraint_types_iter,
                    /*enclosing_type_definition=*/nullptr,
                    /*want_definition=*/false,
                    /*class_body_only=*/false,
                    /*pending_constraint_types=*/nullptr);
  }  /* for */
  close_all_namespace_scopes(buffer);
}  /* an_import_scope::import_all_types */


void an_import_scope::open_namespace(ostringstream           &buffer,
                                     wstring::const_iterator namespace_begin,
                                     wstring::const_iterator namespace_end)
/*
Emit the text to open a namespace scope.
*/
{
  buffer << "namespace " << wstring(namespace_begin, namespace_end)
         << " {" << END_OF_LINE;
}  /* an_import_scope::open_namespace */


void an_import_scope::close_namespace(ostringstream           &buffer,
                                      wstring::const_iterator namespace_begin,
                                      wstring::const_iterator namespace_end)
/*
Emit the text to close a namespace scope.
*/
{
  buffer << '}';
#if DEBUG
  buffer << "  /* namespace " << wstring(namespace_begin, namespace_end)
         << " */";
#endif /* DEBUG */
  buffer << END_OF_LINE;
}  /* an_import_scope::close_namespace */


void an_import_scope::set_namespace_scope(ostringstream    &buffer,
                                          a_qualified_name namespace_name)
/*
Open or close namespace scopes to set the current namespace to namespace_name.
*/
{
  auto old_name_begin = active_namespace_.as_string().begin();
  auto old_name_end = active_namespace_.as_string().end();
  auto old_name_iter = old_name_begin;
  auto old_separators_iter = active_namespace_.separator_offsets().begin();
  auto old_separators_end = active_namespace_.separator_offsets().end();
  auto new_name_begin = namespace_name.as_string().begin();
  auto new_name_end = namespace_name.as_string().end();
  auto new_name_iter = new_name_begin;
  auto new_separators_iter = namespace_name.separator_offsets().begin();
  auto new_separators_end = namespace_name.separator_offsets().end();
  if (old_name_iter != old_name_end && new_name_iter != new_name_end) {
    for (;;) {
      auto old_component_end = (old_separators_iter == old_separators_end)
                                       ? old_name_end
                                       : old_name_begin + *old_separators_iter;
      auto new_component_end = (new_separators_iter == new_separators_end)
                                       ? new_name_end
                                       : new_name_begin + *new_separators_iter;
      if (distance(old_name_iter, old_component_end) != 
                                 distance(new_name_iter, new_component_end) ||
          !equal(old_name_iter, old_component_end, new_name_iter)) {
        /* The namespaces differ. */
        break;
      }  /* if */
      /* Advance to the next component. */
      old_name_iter = old_component_end;
      if (old_name_iter != old_name_end) {
        ++old_separators_iter;
        old_name_iter += a_qualified_name::separator_length;
      }  /* if */
      new_name_iter = new_component_end;
      if (new_name_iter != new_name_end) {
        ++new_separators_iter;
        new_name_iter += a_qualified_name::separator_length;
      }  /* if */
      if (old_name_iter == old_name_end || new_name_iter == new_name_end) {
        /* We have reached the end of at least one of the namespaces. */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  /* [old_name_iter, old_name_end) addresses the (possibly empty) range of
     characters of the namespace(s) that should be closed.
     [old_separators_iter, old_separators_end) addresses the (possibly
     empty) range of separator offsets, relative to old_name_begin, of any
     separators within that range.  The new_* iterators correspond in a
     similar manner to the namespace(s) that should be opened. */
  if (old_name_iter != old_name_end) {
    for (;;) {
      auto old_component_begin = (old_separators_iter == old_separators_end)
                                         ? old_name_iter
                                         : old_name_begin +
                                           *(old_separators_end - 1) +
                                           a_qualified_name::separator_length;
      close_namespace(buffer, old_component_begin, old_name_end);
      if (old_name_iter == old_component_begin) {
        break;
      }  /* if */
      --old_separators_end;
      old_name_end = old_component_begin - a_qualified_name::separator_length;
    }  /* for */
  }  /* if */
  if (new_name_iter != new_name_end) {
    for (;;) {
      auto new_component_end = (new_separators_iter == new_separators_end)
                                       ? new_name_end
                                       : new_name_begin + *new_separators_iter;
      open_namespace(buffer, new_name_iter, new_component_end);
      if (new_component_end == new_name_end) {
        break;
      }  /* if */
      ++new_separators_iter;
      new_name_iter = new_component_end + a_qualified_name::separator_length;
    }  /* for */
  }  /* if */
  active_namespace_ = move(namespace_name);
}  /* an_import_scope::set_namespace_scope */


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
  } else if (full_type_name == MAKE_CLASS_STRING(Enum) ||
             full_type_name == MAKE_CLASS_STRING(MulticastDelegate)) {
    kind = tlk_ref_class;
  } else if (!IsNilToken(extends_token)) {
    a_qualified_name extends_class_name = resolve_type_token(
                                                extends_token,
                                                generic_type_parameters,
                                                no_generic_method_parameters);
    if (extends_class_name == MAKE_CLASS_STRING(ValueType)) {
      kind = tlk_value_type;
    } else if (extends_class_name == MAKE_CLASS_STRING(Enum)) {
      kind = tlk_enumeration;
    } else if (extends_class_name == MAKE_CLASS_STRING(Delegate) ||
               extends_class_name == MAKE_CLASS_STRING(MulticastDelegate)) {
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


an_import_interface *a_custom_attribute::import_interface() const {
  return import_scope_->import_interface();
}  /* a_custom_attribute::import_interface */

void a_custom_attribute::decode_type() const
{
  if (IsNilToken(ctor_token_)) {
    HRESULT    hr;
    const BYTE *data_ptr;
    ULONG      bytes_in_data;
    USHORT     prolog;

    check_assertion(import_interface() != nullptr && !IsNilToken(token_));
    hr = import_interface()->GetCustomAttributeProps(
                                    token_, /*ptkObj=*/nullptr, &ctor_token_,
                                    reinterpret_cast<const void**>(&data_ptr),
                                    &bytes_in_data);
    CHECK_API_RESULT(hr, GetCustomAttributeProps);
    if (IsNilToken(ctor_token_)) {
      unexpected_condition();
    } else if (data_ptr != nullptr && bytes_in_data > 0) {
      mdToken         type_token;
      data_ = a_custom_attribute_data(data_ptr, bytes_in_data);
      /* Read the prolog that starts the custom attribute data. */
      data_.read_and_advance(prolog);
      if (prolog != 0x0001) {
        unexpected_condition();
      }  /* if */
      /* Determine the function type of the attribute's constructor. */
      switch (TypeFromToken(ctor_token_)) {
        case mdtMethodDef:
          hr = import_interface()->GetMethodProps(ctor_token_,
                                                  &type_token,
                                                  /*pdwAttr=*/nullptr,
                                                  &signature_,
                                                  &bytes_in_signature_,
                                                  /*pulCodeRVA=*/nullptr,
                                                  /*pdwImplFlags=*/nullptr);
          CHECK_API_RESULT(hr, GetMethodProps);
          break;
        case mdtMemberRef:
          hr = import_interface()->GetMemberRefProps(ctor_token_,
                                                     &type_token,
                                                     &signature_,
                                                     &bytes_in_signature_);
          CHECK_API_RESULT(hr, GetMethodProps);
          break;
        default:
          unexpected_condition();
          break;
      }  /* switch */
      type_name_ = import_scope_->resolve_type_token(
                                                type_token,
                                                no_generic_type_parameters,
                                                no_generic_method_parameters);
    } /* if */
  }  /* if */
}  /* a_custom_attribute::decode_type */


void a_custom_attribute::decode_fixed_args() const
{
  /* Ensure the type has been decoded. */
  decode_type();
  if (fixed_args_ == nullptr && !data_.empty()) {
    a_signature_decoder decoder(*import_scope_,
                                signature_, bytes_in_signature_,
                                no_generic_type_parameters,
                                no_generic_method_parameters,
                                /*is_system_string_member=*/FALSE);
    auto type = decoder.decode_method_signature(ctor_token_);
    if (type != nullptr) {
      a_function_type_wrapper_ptr ctor_type = type->as_function();
      check_assertion(ctor_type != nullptr);
      auto parameter_list = ctor_type->parameter_list();
      /* The number of fixed arguments is equal to the number of
         parameters in the constructor. */
      fixed_args_ = make_shared<an_attribute_argument_list>();
      fixed_args_->reserve(parameter_list.size());
      for (auto iter = parameter_list.begin();
           iter != parameter_list.end();
           ++iter) {
        auto param = *iter;
        auto param_type = param.type();
        if (param_type != nullptr) {
          fixed_args_->push_back(an_attribute_argument(data_, param_type));
        } else {
          unexpected_condition();
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* a_custom_attribute::decode_fixed_args */


void a_custom_attribute::decode_named_args() const
{
  /* Ensure the fixed arguments have been decoded. */
  decode_fixed_args();
  if (named_args_ == nullptr && !data_.empty()) {
    UINT num_named;
    data_.read_and_advance(num_named);
    named_args_ = make_shared<an_attribute_argument_list>();
    named_args_->reserve(num_named);
    for (UINT index = 0; index < num_named; ++index) {
      a_type_wrapper_ptr type;
      data_.read_and_advance(type);
      /* Read the field or property name. */
      shared_ptr<wstring> name;
      data_.read_and_advance(name);
      if (name != nullptr) {
        named_args_->push_back(an_attribute_argument(data_, type, *name));
      } else {
        unexpected_condition();
        break;
      }  /* if */
    }  /* for */
    /* Ensure we have consumed all of the custom attribute data. */
    check_assertion(data_.empty());
  }  /* if */
}  /* a_custom_attribute::decode_named_args */


an_import_interface *a_custom_attribute_processor::import_interface() const
{
  return import_scope_->import_interface();
}  /* a_custom_attribute_processor::import_interface */


an_assembly_index an_assembly::index = 0;

void an_assembly::cleanup()
/*
Clean up an assembly once we have finished processing.
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
  bool result = false;

  for (DWORD scope_index = 0; scope_index < count_of_scopes_; ++scope_index) {
    IMetaDataImport  *md_import_inferface  = nullptr;
    IMetaDataImport2 *md_import2_inferface = nullptr;
    HRESULT          hr;

    hr = alink_interface_->GetScope(AssemblyIsUBM, alink_token_, scope_index,
                                    &md_import_inferface);
    if (FAILED(hr)) {
      result = false;
      goto next;
    } else if ((hr == S_FALSE) || (md_import_inferface == nullptr)) {
      /* There are no types in this scope.  Skip it. */
      goto next;
    }  /* if */
    check_assertion(!processed_an_interesting_scope);
    /* Query interface to the new, improved interface. */
    hr = md_import_inferface->QueryInterface(IID_IMetaDataImport2,
                                             reinterpret_cast<void**>(
                                                      &md_import2_inferface));
    if (FAILED(hr)) {
      result = false;
      goto next;
    }  /* if */
    /* Create an import scope and import all the types. */
    imported_scopes_.push_back(an_import_scope(
             static_cast<an_import_interface*>(md_import2_inferface), *this));
    processed_an_interesting_scope = true;
    result = true;
next:
    if (md_import_inferface != NULL) md_import_inferface->Release();
    if (md_import2_inferface != NULL) md_import2_inferface->Release();
    if (!result) break;
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
      if (member_name == COR_ENUM_FIELD_NAME_W) {
        /* This is the special member: its type is the underlying type of the
           enumeration. */
        a_signature_decoder decoder(*this, signature, bytes_in_signature,
                                    generic_type_parameters,
                                    no_generic_method_parameters,
                                    /*is_system_string_member=*/FALSE);
        check_assertion(IsFdRTSpecialName(attributes) != 0);
        check_assertion(constant_type == ELEMENT_TYPE_VOID);
        check_assertion(underlying_type.empty());
        underlying_type = decoder.decode_field_signature()->get_string();
        if (underlying_type.empty()) {
          break;
        }  /* if */
      } else {
        a_constant_decoder decoder(constant_type, constant_value,
                                   characters_in_constant);

        /* Get the constant value associated with it. */
        escape_invalid_identifier(member_name);
        enumerators.push_back(an_enumerator(member_name, decoder.decode()));
      }  /* if */
    }  /* for */
  } while (count_of_members > 0);
  import_interface_->CloseEnum(enum_members);
  /* Now we have all the information we need, we can emit the definition of
     the enumeration.  Note, we emit the value of an enumerator as a
     hexadecimal constant cast to the underlying type of the enumeration. */
  if (!underlying_type.empty()) {
    buffer << enumeration_name << " : " << underlying_type << " {"
           << END_OF_LINE;
    for (auto enum_iter = enumerators.begin();
         enum_iter != enumerators.end();
         ++enum_iter) {
      const an_enumerator &enumerator = *enum_iter;
      buffer << enumerator.first << " = static_cast<" << underlying_type;
      buffer << ">(" << enumerator.second << ')';
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
  }  /* if */
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
    a_signature_decoder decoder(*this, signature, bytes_in_signature,
                                generic_type_parameters,
                                no_generic_method_parameters,
                                /*is_system_string_member=*/FALSE);
    a_type_wrapper_ptr type = decoder.decode_method_signature(methods[0]);
    if (type != nullptr) {
      auto method_type = type->as_function();
      check_assertion(method_type != nullptr);
      buffer << method_type->generic_header() << END_OF_LINE;
      buffer << method_type->get_string(delegate_name) << ';' << END_OF_LINE;
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  import_interface_->CloseEnum(enum_methods);
}  /* an_import_scope::import_delegate_definition */


BYTE an_import_scope::get_generic_parameter_count(mdTypeDef token) const
/*
Return the number of generic parameters associated with the specified type
token.  For nested types, this may differ from the generic arity, as generic
parameters from enclosing types are included in the count.
*/
{
  BYTE           generic_param_count = 0;
  HRESULT        hr;
  HCORENUM       enum_parameters = nullptr;
  mdGenericParam parameters[8];
  ULONG          count_of_parameters;

  check_assertion(TypeFromToken(token) == mdtTypeDef);
  do {
    hr = import_interface_->EnumGenericParams(&enum_parameters,
                                              token, parameters,
                                              _countof(parameters),
                                              &count_of_parameters);
    CHECK_API_RESULT(hr, EnumGenericParams);
    generic_param_count += static_cast<BYTE>(count_of_parameters);
  } while (count_of_parameters > 0);
  import_interface_->CloseEnum(enum_parameters);
  return generic_param_count;
}  /* an_import_scope::get_generic_parameter_count */


BYTE an_import_scope::get_generic_parameters_and_constraints(
           mdToken                        token,
           const a_generic_parameter_list &generic_type_parameters_for_method,
           BYTE                           enclosing_type_generic_params_count,
           a_generic_parameter_list       &generic_parameters,
           a_constraint_clause_list       &generic_constraints,
           a_pending_constraint_type_list *pending_constraint_types)
/*
Fill-in the generic parameters and constraints associated with this type or
method and return its generic arity.
*/
{
  BYTE                          generic_arity = 0;
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
        escape_invalid_identifier(param_name);
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
    check_assertion(enclosing_type_generic_params_count <=
                                                   generic_parameters.size());
    generic_arity = generic_parameters.size() -
                                          enclosing_type_generic_params_count;
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
    generic_arity = generic_parameters.size();
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
  if (generic_arity > 0) {
    get_generic_constraints(token, generic_type_parameters_for_method,
                            generic_arity, generic_parameters_info,
                            generic_parameters, generic_constraints,
                            pending_constraint_types);
  }
  return generic_arity;
}  /* an_import_scope::get_generic_parameters_and_constraints */


void an_import_scope::get_generic_constraints(
     mdToken                              token,
     const a_generic_parameter_list       &generic_type_parameters_for_method,
     BYTE                                 generic_arity,
     const a_generic_parameter_info_list  &generic_parameters_info,
     const a_generic_parameter_list       &generic_parameters,
     a_constraint_clause_list             &generic_constraints,
     a_pending_constraint_type_list       *pending_constraint_types)
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
  check_assertion(TypeFromToken(token) == mdtTypeDef ||
                  pending_constraint_types == nullptr);
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
    bool use_pending_constraint_clause = false;
    do {
      hr = import_interface_->EnumGenericParamConstraints(
                                                       &enum_constraints,
                                                       param_info.token(),
                                                       constraints,
                                                       _countof(constraints),
                                                       &count_of_constraints);
      CHECK_API_RESULT(hr, EnumGenericParamConstraints);
      if (pending_constraint_types != nullptr && count_of_constraints > 0) {
        use_pending_constraint_clause = true;
        break;
      }  /* if */
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
        a_qualified_name constraint_name = resolve_type_token(
                                                   constraint_item,
                                                   generic_type_parameters,
                                                   generic_method_parameters);
        constraint_items += constraint_name.as_string();
        constraint_items += L", ";
      }  /* for */
    } while (count_of_constraints > 0);
    import_interface_->CloseEnum(enum_constraints);
    if (use_pending_constraint_clause) {
      /* This type has a generic constraint that refers to another type that
         may not yet have been imported.  Declare the type with a pending
         constraint clause and add it to the list of generic types to
         re-declare after all other types have been imported. */
      generic_constraints.clear();
      generic_constraints.push_back(L"...");
      pending_constraint_types->push_back(token);
      break;
    }  /* if */
    if (!constraint_items.empty()) {
      wstring constraint_clause = L"where " + param_name + L" : ";
      constraint_clause.append(constraint_items, 0,
                               constraint_items.size() - (_countof(L", ")-1));
      generic_constraints.push_back(move(constraint_clause));
    }  /* if */
  }  /* for */
  generic_constraints.shrink_to_fit();
}  /* an_import_scope::get_generic_constraints */


a_qualified_name an_import_scope::resolve_typedef_token(
      mdTypeDef                         token,
      const a_generic_param_or_arg_list &generic_type_params_or_args,
      a_generic_param_or_arg_iterator   generic_type_params_or_args_end,
      bool                              omit_generic_params_or_args/*=false*/)
{
  a_qualified_name qualified_name;
  wstring          type_name;
  HRESULT          hr;
  DWORD            type_flags;
  BYTE             generic_arity = get_generic_parameter_count(token);

  hr = import_interface_->GetTypeDefProps(token, type_name,
                                          &type_flags,
                                          /*ptkExtends=*/nullptr);
  CHECK_API_RESULT(hr, GetTypeDefProps);
  if (IsTdNested(type_flags)) {
    mdTypeDef enclosing_typedef;
    BYTE      enclosing_type_generic_param_count;
    hr = import_interface_->GetNestedClassProps(token,
                                                &enclosing_typedef);
    CHECK_API_RESULT(hr, GetNestedClassProps);
    enclosing_type_generic_param_count =
                               get_generic_parameter_count(enclosing_typedef);
    /* Adjust the generic arity to account for the params/args
       consumed by the enclosing type. */
    check_assertion(generic_arity >= enclosing_type_generic_param_count);
    generic_arity -= enclosing_type_generic_param_count;
    qualified_name = resolve_typedef_token(
                             enclosing_typedef,
                             generic_type_params_or_args,
                             generic_type_params_or_args_end - generic_arity);
  }  /* if */
  if (generic_arity > 0) {
    strip_generic_arity(type_name);
  }  /* if */
  if (IsTdNested(type_flags)) {
    qualified_name.append_identifier(move(type_name));
  } else {
    qualified_name = a_qualified_name::from_dotted_name(type_name);
  }  /* if */
  if (generic_arity > 0 && !omit_generic_params_or_args) {
    qualified_name.append_generic_params_or_args(
                              generic_type_params_or_args_end - generic_arity,
                              generic_type_params_or_args_end);
  }  /* if */
  return qualified_name;
}  /* an_import_scope::resolve_typedef_token */


a_qualified_name an_import_scope::resolve_typeref_token(
               mdTypeRef                         token,
               const a_generic_param_or_arg_list &generic_type_params_or_args)
{
  a_qualified_name qualified_name;
  wstring          type_name;
  HRESULT          hr;
  mdToken          resolution_scope;
  bool             is_nested = false;

  hr = import_interface_->GetTypeRefProps(token,
                                          &resolution_scope,
                                          type_name);
  CHECK_API_RESULT(hr, GetTypeRefProps);
  if (IsNilToken(resolution_scope)) {
    /* FIXME: Exported types are not yet supported.  In this case,
       there shall be a row in the ExportedType table for this Type.
       Its Implementation field shall contain a File token or an
       AssemblyRef token that says where the type is defined.  */
  } else {
    switch (TypeFromToken(resolution_scope)) {
      case mdtTypeRef:
        { /* The type is a nested type and resolution_scope indicates
             the enclosing type. */
          qualified_name = resolve_type_token(resolution_scope,
                                              generic_type_params_or_args,
                                              no_generic_method_parameters);
          is_nested = true;
          break;
        }  /* case mdtTypeRef */
      case mdtModuleRef:
        /* The type is defined in another module within the same
           assembly; no special handling required. */
        break;
      case mdtModule:
        /* The type is defined in the current module; no special
           handling required. */
        break;
      case mdtAssemblyRef:
        /* FIXME: The type is defined in a different assembly;
           additional work needs to be done to issue an error if the
           assembly isn't referenced. */
        break;
      default:
        unexpected_condition();
        break;
    }  /* switch */
  }  /* if */
  BYTE generic_arity = strip_generic_arity(type_name);
  if (is_nested) {
    qualified_name.append_identifier(move(type_name));
  } else {
    qualified_name = a_qualified_name::from_dotted_name(type_name);
  }  /* if */
  if (generic_arity > 0) {
    qualified_name.append_generic_params_or_args(
                            generic_type_params_or_args.end() - generic_arity,
                            generic_type_params_or_args.end());
  }  /* if */
  return qualified_name;
}  /* an_import_scope::resolve_typeref_token */


a_qualified_name an_import_scope::resolve_type_token(
      mdToken                           token,
      const a_generic_param_or_arg_list &generic_type_params_or_args,
      const a_generic_param_or_arg_list &generic_method_params_or_args,
      bool                              omit_generic_params_or_args/*=false*/)
/*
Resolve the type given by the token and return its qualified name.
*/
{
  a_qualified_name qualified_name;
  check_assertion(!omit_generic_params_or_args ||
                  TypeFromToken(token) == mdtTypeDef);
  /* First check if this token is a token cached by a previous call.
     If it is a generic type, we can't cache the name because the generic
     parameters and arguments may differ in different contexts. */
  bool can_cache_name = (TypeFromToken(token) != mdtTypeSpec &&
                         generic_type_params_or_args.begin()
                                        == generic_type_params_or_args.end());
  auto iter = can_cache_name ? map_of_tokens_to_names_.find(token)
                             : map_of_tokens_to_names_.end();
  if (iter != map_of_tokens_to_names_.end()) {
    qualified_name = iter->second;
  } else {
    HRESULT hr;
    switch (TypeFromToken(token)) {
      case mdtTypeDef:
        qualified_name = resolve_typedef_token(
                                            token,
                                            generic_type_params_or_args,
                                            generic_type_params_or_args.end(),
                                            omit_generic_params_or_args);
        break;
      case mdtTypeRef:
        qualified_name = resolve_typeref_token(token,
                                               generic_type_params_or_args);
        break;
      case mdtInterfaceImpl:
        /* If this is an interface-impl token then get the token for the
           interface definition and then attempt to resolve that token. */
        hr = import_interface_->GetInterfaceImplProps(token,
                                                      /*mdTypeDef=*/nullptr,
                                                      &token);
        CHECK_API_RESULT(hr, GetInterfaceImplProps);
        qualified_name = resolve_type_token(token,
                                            generic_type_params_or_args,
                                            generic_method_params_or_args);
        break;
      case mdtTypeSpec:
        { PCCOR_SIGNATURE signature;
          ULONG           bytes_in_signature;
          hr = import_interface_->GetTypeSpecFromToken(token, &signature,
                                                       &bytes_in_signature);
          CHECK_API_RESULT(hr, GetTypeSpecFromToken);
          a_signature_decoder decoder(*this, signature, bytes_in_signature,
                                      generic_type_params_or_args,
                                      generic_method_params_or_args,
                                      /*is_system_string_member=*/FALSE);
          a_type_wrapper_ptr type = decoder.decode_type();
          if (type != nullptr) {
            a_const_class_type_wrapper_ptr class_type = type->as_class();
            if (class_type == nullptr) {
              a_type_indirection_ptr indirection = type->as_indirection();
              if (indirection != nullptr) {
                /* Ref class types are decoded as handle types, so we must get
                   the class type from the underlying type. */
                check_assertion(indirection->is_of_indirection_kind(
                                             a_type_indirection::tik_handle));
                class_type = indirection->underlying_type()->as_class();
              }  /* if */
            }  /* if */
            if (class_type != nullptr) {
              qualified_name = class_type->name();
            } else {
              unexpected_condition();
            }  /* if */
          } else {
            unexpected_condition();
          }  /* if */
          break;
        }  /* case mdtTypeSpec */
      default:
        unexpected_condition();
        break;
    }  /* switch */
    if (can_cache_name) {
      map_of_tokens_to_names_[token] = qualified_name;
    }  /* if */
  }  /* if */
  return qualified_name;
}  /* an_import_scope::resolve_type_token */


an_import_scope& an_import_scope::operator=(an_import_scope&& other)
{
  scope_name_ = move(other.scope_name_);
  containing_assembly_ = move(other.containing_assembly_);
  import_interface_ = move(other.import_interface_);
  active_namespace_ = move(other.active_namespace_);
  map_of_tokens_to_names_ = move(other.map_of_tokens_to_names_);
  custom_attribute_processors_ = move(other.custom_attribute_processors_);
  return *this;
}  /* an_import_scope::operator= */


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
    a_method_def()
      : type_definition_(nullptr)
      , token_(mdMethodDefNil)
      , attributes_(0)
    {
    }  /* Default constructor */

    a_method_def(a_type_definition *type_definition, mdMethodDef token,
                 DWORD attributes)
      : type_definition_(type_definition),
        token_(token),
        attributes_(attributes)
    {
      check_assertion(!IsNilToken(token) &&
                      TypeFromToken(token) == mdtMethodDef);
    }  /* Constructor */

    a_method_def(a_method_def &&other)
      : type_definition_(nullptr)
      , token_(mdMethodDefNil)
      , attributes_(0)
    {
      other.swap(*this);
    }  /* Move constructor */

    a_method_def& operator=(a_method_def other)
    {
      other.swap(*this);
      return *this;
    }  /* Assignment operator */

    void swap(a_method_def &other) {
      std::swap(type_definition_, other.type_definition_);
      std::swap(token_, other.token_);
      std::swap(attributes_, other.attributes_);
    }  /* swap */

    mdMethodDef token() const
    {
      return token_;
    }  /* token */

    class an_accessibility accessibility() const;

    DWORD attributes() const
    {
      return exists() ? attributes_ : 0;
    } /* attributes */

    bool exists() const
    {
      return !IsNilToken(token_);
    }  /* exists */

  private:
    a_type_definition *type_definition_;
                        /* The type_definition containing this method. */
    mdMethodDef       token_;
                        /* The token associated with this method. */
    DWORD             attributes_;
                        /* The attributes for this method. */
  };  /* a_method_def */

public:
  a_type_definition(mdTypeDef                      typedef_token,
                    const a_qualified_name         &full_type_name,
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
      is_system_string_type_(full_type_name == MAKE_CLASS_STRING(String))
  {
    check_assertion(!IsNilToken(typedef_token_));
    a_cpp_cli_import_flag_set import_flags = import_scope_.
                                         containing_assembly().import_flags();

    import_as_friend_ = (import_flags & cpp_cli_as_friend_assembly) != 0;
  }  /* constructor */

  void import_definition(ostringstream& buffer);

  mdMethodDef token() const
  {
    return typedef_token_;
  }  /* token */

  DWORD attributes() const
  {
    return attributes_;
  }  /* attributes */

  const a_generic_parameter_list &generic_parameters() const
  {
    return generic_parameters_;
  }  /* generic_parameters */

  bool is_named_override(mdToken method_token) const
  /*
  Returns TRUE if the method is a named override for a method from a base
  class or interface.
  */
  {
     auto method_impls_range = method_impls_.equal_range(method_token);
     return method_impls_range.first != method_impls_range.second;
  }  /* is_named_override */

private:
  string process_base_class_list(ostringstream& buffer);
  bool process_extends(ostringstream& buffer);
  void process_interfaces(ostringstream& buffer);
  void get_method_impls();
  void import_nested_classes(ostringstream& buffer);
  void import_all_methods(ostringstream &buffer);
  void write_method_decl_specifiers(ostringstream &buffer,
                                    mdToken       token,
                                    DWORD         method_attributes);
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
  mdToken get_associated_event_or_property(mdToken method_token);
  wstring get_overridden_name(mdToken member_token);
  a_cli_operator_kind rename_cli_operator(wstring &method_name,
                                          DWORD   method_attributes);

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
  set<mdMethodDef>
                property_and_event_methods_;
                        /* If this type definition is a class, this set
                           contains the methods associated with any properties
                           or events.  It is used to skip importing those
                           methods as ordinary member functions. */
  bool          import_as_friend_;
                        /* True if this type should be imported as a
                           friend. */
};  /* a_type_definition */


class an_accessibility
{
public:
  an_accessibility() : access_(access_none), import_as_friend_(false) {}
  an_accessibility(const an_assembly &containing_assembly, mdToken token,
                   DWORD attributes, const a_type_definition *type_definition)
  {
    auto import_flags = containing_assembly.import_flags();
    import_as_friend_ = (import_flags & cpp_cli_as_friend_assembly) != 0;
    bool import_inaccessible = false;
    check_assertion(!IsNilToken(token));
    switch (TypeFromToken(token)) {
      case mdtTypeDef:
        switch (attributes & tdVisibilityMask) {
          case tdNotPublic:
            access_ = import_as_friend_ ? access_private_as_friend
                                        : access_private;
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
            access_ = import_as_friend_ ? access_assembly_as_friend
                                        : access_assembly;
            break;
          case tdNestedFamANDAssem:
            access_ = import_as_friend_ ? access_family_and_assembly_as_friend
                                        : access_family_and_assembly;
            break;
          case tdNestedFamORAssem:
            access_ = import_as_friend_ ? access_family_or_assembly_as_friend
                                        : access_family_or_assembly;
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
            access_ = import_as_friend_ ? access_family_and_assembly_as_friend
                                        : access_family_and_assembly;
            break;
          case fdAssembly:
            access_ = import_as_friend_ ? access_assembly_as_friend
                                        : access_assembly;
            break;
          case fdFamily:
            access_ = access_family;
            break;
          case fdFamORAssem:
            access_ = import_as_friend_ ? access_family_or_assembly_as_friend
                                        : access_family_or_assembly;
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
            access_ = import_as_friend_ ? access_family_and_assembly_as_friend
                                        : access_family_and_assembly;
            break;
          case mdAssem:
            access_ = import_as_friend_ ? access_assembly_as_friend
                                        : access_assembly;
            break;
          case mdFamily:
            access_ = access_family;
            break;
          case mdFamORAssem:
            access_ = import_as_friend_ ? access_family_or_assembly_as_friend
                                        : access_family_or_assembly;
            break;
          case mdPublic:
            access_ = access_public;
            break;
          default:
            unexpected_condition();
            break;
        }  /* switch */
        if (!import_inaccessible) {
          switch (access_) {
            case access_private:
            case access_family_and_assembly:
            case access_assembly:
              /* Inaccessible methods are not usually imported; however, a
                 method that is a named override of a method from a base
                 interface must still be imported to satisfy the interface's
                 contract. */
              /* FIXME: There still seems to be a problem with this approach;
                 see test cases decl_security_regress_00[34].C.  This is
                 Microsoft issue #344396. */
              import_inaccessible = type_definition->is_named_override(token);
              break;
            default:
              break;
          }  /* switch */
        }  /* if */
        break;
      default:
        unexpected_condition();
        break;
    }  /* switch */
    if (import_inaccessible) {
      switch (access_) {
        case access_private:
          access_ = access_imported_private;
          break;
        case access_family_and_assembly:
          access_ = access_imported_family_and_assembly;
          break;
        case access_assembly:
          access_ = access_imported_assembly;
          break;
        default:
          break;
      }  /* switch */
    }  /* if */
  }  /* constructor */

  wstring get_string() const
  {
    wstring result;
    switch (access_) {
      case access_none:
        /* We should not be emitting this accessibility. */
        unexpected_condition();
        break;
      case access_private:
      case access_imported_private:
        /* Accessible only by the parent type */
        result = L"private";
        break;
      case access_private_as_friend:
        result = L"public";
        break;
      case access_family_and_assembly:
      case access_imported_family_and_assembly:
        /* Accessible by subtypes only in the assembly. */
        result = L"private protected";
        break;
      case access_family_and_assembly_as_friend:
        result = L"protected";
        break;
      case access_assembly:
      case access_imported_assembly:
        /* Accessibly by anyone in the assembly. */
        result = L"internal";
        break;
      case access_assembly_as_friend:
        result = L"public";
        break;
      case access_family:
        /* Accessible only by type and subtypes. */
        result = L"protected";
        break;
      case access_family_or_assembly:
        /* Accessible by derived classes and by other types in the
            assembly. */
        result = L"protected public";
        break;
      case access_family_or_assembly_as_friend:
        result = L"public";
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

  bool is_accessible() const
  {
    return access_ >= access_family ||
           access_ == access_imported_private ||
           access_ == access_imported_family_and_assembly ||
           access_ == access_imported_assembly ||
           (import_as_friend_ && access_ >= access_private_as_friend);
  }  /* is_accessible */

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
    access_none,                       /* none             none             */
    access_private,                    /* private          private          */
    access_imported_private,
    /* A special case of access_private used to indicate that the type or
       member will be imported even though it is inaccessible. */
    access_private_as_friend,
    access_family_and_assembly,        /* protected        private          */
    access_imported_family_and_assembly,
    /* A special case of access_family_and_assembly used to indicate that the
       type or member will be imported even though it is inaccessible. */
    access_family_and_assembly_as_friend,
    access_assembly,                   /* public           private          */
    access_imported_assembly,
    /* A special case of access_assembly used to indicate that the type or
       member will be imported even though it is inaccessible. */
    access_assembly_as_friend,
    access_family,                     /* protected        protected        */
    access_family_or_assembly,         /* public           protected        */
    access_family_or_assembly_as_friend,
    access_public                      /* public           public           */
  } access_;

  an_accessibility(access_kind access) : access_(access) {}

  bool import_as_friend_;
};  /* an_accessibility */


an_accessibility a_type_definition::a_method_def::accessibility() const
{
  return exists() ? an_accessibility(type_definition_->import_scope_.
                                                        containing_assembly(),
                                     token_, attributes_, type_definition_)
                  : an_accessibility();
}  /* a_type_definition::a_method_def::accessibility */


string a_type_definition::process_base_class_list(ostringstream& buffer)
{
  ostringstream interface_list;
  auto import_flags = import_scope_.containing_assembly().import_flags();
  bool use_pending_implements_clause = false &&
                               (import_flags & cpp_cli_define_all_types) == 0;
  bool base_class_processed = process_extends(buffer);

  process_interfaces(interface_list);
  string pending_interface_list = interface_list.str();
  if (!pending_interface_list.empty()) {
    buffer << (base_class_processed ? ", " : " : ");
    if (use_pending_implements_clause) {
      buffer << "__implements ...";
    } else {
      buffer << pending_interface_list;
      pending_interface_list.clear();
    }  /* if */
  }  /* if */
  return pending_interface_list;
}  /* a_type_definition::process_base_class_list */


bool a_type_definition::process_extends(ostringstream& buffer)
/*
Decode the extends token and emit the appropriate text.
*/
{
  bool base_class_processed = false;
  if (!IsNilToken(extends_token_)) {
    a_qualified_name extends_name = import_scope_.resolve_type_token(
                                                extends_token_,
                                                generic_parameters_,
                                                no_generic_method_parameters);
    if (kind_ == tlk_value_type) {
      /* By definition all value types extend System.ValueType so there is no
         need to explicitly add it as a base-class. */
      check_assertion(extends_name == MAKE_CLASS_STRING(ValueType));
    } else if (!extends_name.empty()) {
      /* Similarly with ref classes: by definition they all extend (directly
         or indirectly) System.Object. */
      if ((kind_ != tlk_ref_class) ||
          (extends_name != MAKE_CLASS_STRING(Object))) {
        buffer << " : " << extends_name.as_string();
        base_class_processed = true;
      }  /* if */
    }  /* if */
  }  /* if */
  return base_class_processed;
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
      a_qualified_name interface_name = import_scope_.resolve_type_token(
                                                interfaces[i],
                                                generic_parameters_,
                                                no_generic_method_parameters);
      if (i != 0) {
        buffer << ", ";
      }  /* if */
      buffer << interface_name.as_string();
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


mdToken a_type_definition::get_associated_event_or_property(
                                                     mdMethodDef method_token)
/*
Return the mdProperty or mdEvent to which this method is associated, or
mdTokenNil if the method is not associated with an event or property.
*/
{
  HRESULT            hr;
  HCORENUM           enum_method_semantics = NULL;
  static const ULONG max_tokens = 2;
  mdToken            tokens[max_tokens];
  ULONG              count_tokens = _countof(tokens);
  mdToken            token = mdTokenNil;

  check_assertion(TypeFromToken(method_token) == mdtMethodDef);
  hr = import_interface_->EnumMethodSemantics(&enum_method_semantics,
                                              method_token,
                                              tokens,
                                              max_tokens,
                                              &count_tokens);
  CHECK_API_RESULT(hr, EnumMethodSemantics);
  import_interface_->CloseEnum(enum_method_semantics);
  if (count_tokens == 1) {
    token = tokens[0];
  } else if (count_tokens > 0) {
    /* A specific member should only map to a single event or property. */
    unexpected_condition();
  }  /* if */
  return token;
}  /* get_associated_event_or_property */


wstring a_type_definition::get_overridden_name(mdToken method_token)
/*
Get the name for the specified member for use in the overridden name list of
an override specifier.
*/
{
  HRESULT         hr;
  mdToken         parent_token;
  wstring         method_name;
  DWORD           method_attributes;
  PCCOR_SIGNATURE signature;
  ULONG           bytes_in_signature;

  switch (TypeFromToken(method_token)) {
    case mdtMethodDef:
      { hr = import_interface_->GetMethodProps(method_token,
                                               &parent_token,
                                               method_name,
                                               &method_attributes,
                                               &signature,
                                               &bytes_in_signature,
                                               /*pulCodeRVA=*/nullptr,
                                               /*pdwImplFlags=*/nullptr);
        CHECK_API_RESULT(hr, GetMethodProps);
        /* Determine if this member is an event or property method.  Note
           that, although CLS Rules 24 and 29 in ECMA-335 indicate that
           methods that implement a property or event shall be marked
           SpecialName in the metadata, this has been shown to not always be
           the case; hence no IsMdSpecialName check. */
        mdToken event_or_property_token =
                               get_associated_event_or_property(method_token);
        if (!IsNilToken(event_or_property_token)) {
          /* Get the name of the event or property. */
          wstring event_or_property_name;
          bool    is_default_indexed_property = false;
          switch (TypeFromToken(event_or_property_token)) {
            case mdtEvent:
              hr = import_interface_->GetEventProps(
                                                   event_or_property_token,
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
              { mdTypeDef       property_class_token;
                PCCOR_SIGNATURE property_signature;
                ULONG           bytes_in_property_signature;
                hr = import_interface_->GetPropertyProps(
                                                event_or_property_token,
                                                &property_class_token,
                                                event_or_property_name,
                                                /*pdwPropFlags=*/nullptr,
                                                &property_signature,
                                                &bytes_in_property_signature,
                                                /*pdwCPlusTypeFlag=*/nullptr,
                                                /*ppDefaultValue=*/nullptr,
                                                /*pcchDefaultValue=*/nullptr,
                                                /*pmdSetter=*/nullptr,
                                                /*pmdGetter*/nullptr,
                                                /*rmdOtherMethod=*/nullptr,
                                                /*cMax=*/0,
                                                /*pcOtherMethod =*/nullptr);
                CHECK_API_RESULT(hr, GetPropertyProps);
                /* Get the property signature to determine if it is an indexed
                   property. */
                a_signature_decoder decoder(import_scope_,
                                            property_signature,
                                            bytes_in_property_signature,
                                            generic_parameters_,
                                            no_generic_method_parameters,
                                            /*is_system_string_type*/false);
                auto type = decoder.decode_method_signature(
                                                     event_or_property_token);
                if (type != nullptr && type->as_function()) {
                  /* The property is an indexed property, determine if it is
                     the default-indexed property. */
                  auto processor = import_scope_.
                        get_custom_attribute_processor(property_class_token);
                  auto default_member_name = processor->default_member_name();
                  if (event_or_property_name == default_member_name) {
                    /* The property matches the default member name of the
                       class of which it is a member. */
                    is_default_indexed_property = true;
                  }  /* if */
                }
                break;
              }
            default:
              unexpected_condition();
              break;
          }  /* switch */
          check_assertion(!event_or_property_name.empty());
          /* Modify the name of this property if it is the default-indexed
             property. */
          if (is_default_indexed_property) {
            event_or_property_name = L"default";
          } else {
            escape_invalid_identifier(event_or_property_name);
          }  /* if */
          /* Determine which kind of event or property method it is. */
          DWORD method_semantics;
          hr = import_interface_->GetMethodSemantics(
                                                    method_token,
                                                    event_or_property_token,
                                                    &method_semantics);
          CHECK_API_RESULT(hr, GetMethodSemantics);
          method_name = event_or_property_name + L"::" +
                                name_from_method_semantics(method_semantics);
        } else if (IsMdSpecialName(method_attributes)) {
          /* The overridden name of a CLI operator is the corresponding
              C++/CLI operator name. */
          a_cli_operator_kind cok = rename_cli_operator(method_name,
                                                        method_attributes);
          if (cok == cok_implicit || cok == cok_explicit) {
            /* Obtain the method's return type to handle user-defined
               conversion operators. */
            a_signature_decoder decoder(import_scope_,
                                        signature, bytes_in_signature,
                                        generic_parameters_,
                                        no_generic_method_parameters,
                                        /*is_system_string_type=*/false);
            method_name = L"operator " + decoder.decode_return_type(
                                                                method_token);
          }  /* if */
        }  /* if */
      }  /* case mdtMethodDef */
      break;
    case mdtMemberRef:
      { hr = import_interface_->GetMemberRefProps(method_token, &parent_token,
                                                  method_name,
                                                  &signature,
                                                  &bytes_in_signature);
        CHECK_API_RESULT(hr, GetMemberRefProps);
        /* FIXME: There needs to be an extra level of indirection here to
           match a member function of an instantiated generic type back to the
           corresponding method in the generic type so that we can determine
           if it is the get/set/add/remove/raise method of a property/event
           or a CLI operator name.  The following is a workaround for the lack
           of this functionality.  Also note that this code doesn't correctly
           call escape_invalid_identifier on the property's name to handle
           invalid C++ identifiers. */
        /* The overridden name of a CLI operator is the corresponding
           C++/CLI operator name. */
        a_cli_operator_kind cok = rename_cli_operator(method_name,
                                                      mdSpecialName);
        if (cok == cok_implicit || cok == cok_explicit) {
          /* Obtain the method's return type to handle user-defined
             conversion operators. */
          a_signature_decoder decoder(import_scope_,
                                      signature, bytes_in_signature,
                                      generic_parameters_,
                                      no_generic_method_parameters,
                                      /*is_system_string_type=*/false);
          method_name = L"operator " + decoder.decode_return_type(
                                                                method_token);
        } else if (cok == cok_none) {
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
        }  /* if */
        break;
      }  /* mdtMemberRef */
    default:
      unexpected_condition();
      break;
  }  /* switch */
  a_qualified_name type_name = import_scope_.resolve_type_token(
                                            parent_token, generic_parameters_,
                                            no_generic_method_parameters);
  check_assertion(!type_name.empty());
  return type_name.as_string() + L"::" + method_name;
}  /* a_type_definition::get_overridden_name */


a_cli_operator_kind a_type_definition::rename_cli_operator(
                                                    wstring &method_name,
                                                    DWORD   method_attributes)
/*
Rename any CLI operators to their corresponding C++/CLI operator name and
return the CLI operator kind of the operator, or cok_none if it is not a CLI
operator.  User-defined conversion operators (cok_implicit and cok_explicit)
require additional processing, as their names contain the method's return type.
*/
{
  a_cli_operator_kind cok = cok_none;

  if (IsMdSpecialName(method_attributes) &&
      wcsncmp(method_name.c_str(), L"op_", sizeof("op_")-1) == 0) {
    /* This might be a CLI operator that needs to be converted to a C++
       operator. */
    string utf8_method_name = conv_wide_to_utf8(
                                   const_cast<wchar_t*>(method_name.c_str()));
    if (method_name.length() != utf8_method_name.length()) {
      /* The method name contains non-ASCII characters.  Because all of the
         CLI operator names are comprised only of ASCII characters, don't
         bother calling find_cli_operator_kind. */
      cok = cok_none;
    } else {
      cok = find_cli_operator_kind(
                                 const_cast<char*>(utf8_method_name.c_str()));
    }  /* if */
    switch (cok) {
      case cok_none:
        /* Not a CLI operator. */
        break;
      case cok_implicit:
      case cok_explicit:
        /* Implicit or explicit user-defined conversion operator. */
        method_name.clear();
        break;
      default:
        { a_cli_operator_info_ptr info = cli_operator_info_from_kind(cok);
          if (IsMdStatic(method_attributes) &&
              info->is_assignment_operator) {
            /* This is a static CLI assignment operator.  Import it using
               the CLI operator name.  We can't consume assignment operators
               with CLR semantics, like static R^ op_Assign(R^, R^). */
          } else if (info->cpp_name == NULL) {
            /* This is a CLI operator for which there is no C++ mapping.
               Import it using the CLI operator name. */
          } else {
            /* This is a CLI operator for which there is a C++ mapping.
               Import it using the C++ operator name. */
            locale   loc;
            wchar_t  cpp_name[100];
            size_t   len = strlen(info->cpp_name);
            check_assertion(len < sizeof(cpp_name)/sizeof(cpp_name[0]));
            use_facet< ctype<wchar_t> >(loc).widen(
                              info->cpp_name, info->cpp_name+len+1, cpp_name);
            method_name = cpp_name;
          }  /* if */
          break;
        }
    }  /* switch */
  }  /* if */
  return cok;
}  /* rename_cli_operator */


void a_type_definition::write_method_decl_specifiers(
                                             ostringstream &buffer,
                                             mdToken       token,
                                             DWORD         method_attributes)
/*
Write the decl specifiers for a property or method.
*/
{
  if ((import_scope_.containing_assembly().import_flags()
                                       & cpp_cli_declspec_member_info) != 0) {
    buffer << "__declspec(member_info(";
    buffer << "0x" << setw(8) << setfill('0') << hex << token;
    buffer << ")) ";
  }  /* if */
  /* Emit any decl specifiers. */
  if (IsMdStatic(method_attributes)) {
    buffer << "static ";
    check_assertion(!IsMdVirtual(method_attributes));
  } else if (IsMdVirtual(method_attributes)) {
    buffer << "virtual ";
  }  /* if */
}  /* a_type_definition::write_method_decl_specifiers */


bool fixup_destructor_or_finalizer_name(const wstring &type_name,
                                        wstring       &method_name)
/*
Returns TRUE if 'method_name' is a destructor or finalizer for the type named
type_name and adjusts 'method_name' accordingly if the type name is an invalid
C++ identifer.
*/
{
  bool is_destructor_or_finalizer = false;
  wchar_t first_char = method_name[0];
  if (first_char == L'~' || first_char == L'!') {
    wstring::size_type chars_to_skip = 1;
    if (type_name.compare(0, _countof(L"__identifier(") - 1,
                          L"__identifier(")) {
      wstring temp_method_name = method_name;
      escape_invalid_identifier(temp_method_name, /*force=*/false,
                                chars_to_skip);
      if (temp_method_name.compare(chars_to_skip, wstring::npos,
                                   type_name) == 0) {
        is_destructor_or_finalizer = true;
        /* Modify the name to be of the form "~__identifier(...)" or
           "!__identifier(...)". */
        method_name = temp_method_name;
      }  /* if */
    } else {
      if (method_name.compare(chars_to_skip, wstring::npos,
                              type_name) == 0) {
        is_destructor_or_finalizer = true;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_destructor_or_finalizer;
}  /* fixup_destructor_or_finalizer_name */


void a_type_definition::import_one_method(ostringstream &buffer,
                                          mdMethodDef   method_token,
                                          DWORD         method_semantics)
/*
Import a single member of a type.
*/
{
  bool                  omit_return_type = false;
  bool                  skip_member = false;
  a_cli_operator_kind   cok = cok_none;
  HRESULT               hr;
  ULONG                 bytes_in_signature;
  DWORD                 method_attributes;
  PCCOR_SIGNATURE       signature;
  wstring               method_name;
  an_accessibility      accessibility;

  hr = import_interface_->GetMethodProps(method_token, /*pClass=*/nullptr,
                                         method_name, &method_attributes,
                                         &signature, &bytes_in_signature,
                                         /*pulCodeRVA=*/nullptr,
                                         /*pdwImplFlags=*/nullptr);
  CHECK_API_RESULT(hr, GetMethodProps);
  /* Get the accessibility.  Skip those methods that are not accessible. */
  accessibility = an_accessibility(import_scope_.containing_assembly(),
                                   method_token, method_attributes, this);
  skip_member = !accessibility.is_accessible();
  if (!skip_member) {
    /* Handle special members methods such as constructors (both instance and
       class) and property accessor methods. */
    if (IsMdInstanceInitializerW(method_attributes, method_name.c_str()) ||
        IsMdClassConstructorW(method_attributes, method_name.c_str())) {
      method_name = type_name_;
      omit_return_type = true;
    } else if (fixup_destructor_or_finalizer_name(type_name_, method_name)) {
      /* A return type should not be emitted for destructors or finalizers. */
      omit_return_type = true;
    } else if (method_semantics != 0) {
      /* Record that this method was imported as a property/event method. */
      property_and_event_methods_.insert(method_token);
      method_name = name_from_method_semantics(method_semantics);
    } else if (IsMdSpecialName(method_attributes)) {
      /* Determine if this member is an event or property method. */
      mdToken event_or_property_token =
                               get_associated_event_or_property(method_token);
      if (!IsNilToken(event_or_property_token)) {
        /* This method is associated with an event or property.  These methods
           are skipped here because import_one_event and import_one_property
           generate the appropriate add/remove/raise or get/set methods. */
        skip_member = true;
      } else {
        /* Rename any CLI operators to their corresponding C++/CLI operator
           name. */
        cok = rename_cli_operator(method_name, method_attributes);
        if (cok == cok_none) {
          escape_invalid_identifier(method_name);
        }  /* if */
      }  /* if */
    } else if (property_and_event_methods_.find(method_token)
                                       != property_and_event_methods_.end()) {
      /* Skip any members that were already imported as a property/event
         method but weren't also marked with the mdSpecialName attribute. */
      skip_member = true;
    } else {
      escape_invalid_identifier(method_name);
    }  /* if  */
  }  /* if */
  if (!skip_member) {
    /* Decode the signature and create the appropriate declaration.  This
       can either be a method or a field. */
    a_signature_decoder decoder(import_scope_, signature, bytes_in_signature,
                                generic_parameters_,
                                no_generic_method_parameters,
                                is_system_string_type_);
    a_type_wrapper_ptr type = decoder.decode_method_signature(method_token);
    if (type != nullptr) {
      auto method_type = type->as_function();
      check_assertion(method_type != nullptr);
      buffer << accessibility.get_string() << ": ";
      buffer << method_type->generic_header() << END_OF_LINE;
      write_method_decl_specifiers(buffer, method_token,
                                   method_attributes);
      if (cok == cok_implicit || cok == cok_explicit) {
        /* Emit an implicit or explicit user-defined conversion operator. */
        if (cok == cok_explicit) buffer << "explicit ";
        /* Obtain the method's return type to handle user-defined conversion
           operators. */
        auto return_type = method_type->return_type();
        if (return_type != nullptr) {
          method_name = L"operator " + return_type->get_string();
        } else {
          method_name = L"operator __error_type";
        }  /* if */
        omit_return_type = true;
      }  /* if */
      if (omit_return_type) {
        method_type = make_shared<a_function_type_wrapper>(
                                            method_type->calling_convention(),
                                            a_type_wrapper_ptr(),
                                            method_type->parameter_list(),
                                            method_type->generic_header());
      }  /* if */
      buffer << method_type->get_string(method_name);
      if (IsMdFinal(method_attributes)) {
        buffer << " sealed";
      }  /* if */
      if (IsMdNewSlot(method_attributes)) {
        if (kind_ != tlk_interface) {
          buffer << " new";
        }  /* if */
      } else if (IsMdVirtual(method_attributes)) {
        buffer << " override";
      }  /* if */
      /* Emit any named overrides. */
      if (IsMdVirtual(method_attributes)) {
        auto method_impls_range = method_impls_.equal_range(method_token);
        for (auto method_impls_iterator = method_impls_range.first;
              method_impls_iterator != method_impls_range.second;
              ++method_impls_iterator) {
          if (method_impls_iterator == method_impls_range.first) {
              buffer << " = ";
          } else {
              buffer << ", ";
          }  /* if */
          buffer << get_overridden_name(method_impls_iterator->second);
        }  /* for */
      }  /* if */
      buffer << ';' << END_OF_LINE;
    }
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
  accessibility = an_accessibility(import_scope_.containing_assembly(),
                                   field_token, field_attributes, this);
  skip_member = !accessibility.is_accessible();
  if (!skip_member && IsFdSpecialName(field_attributes)) {
    skip_member = true;
  }  /* if  */
  if (!skip_member) {
    /* Decode the signature and create the appropriate declaration.  This
       can either be a method or a field. */
    a_signature_decoder decoder(import_scope_, signature, bytes_in_signature,
                                generic_parameters_,
                                no_generic_method_parameters,
                                is_system_string_type_);
    a_type_wrapper_ptr field_type = decoder.decode_field_signature();
    if (field_type) {
      escape_invalid_identifier(field_name, /*force=*/true);
      buffer << accessibility.get_string() << ": ";
      if ((import_flags & cpp_cli_declspec_member_info) != 0) {
        buffer << "__declspec(member_info(";
        buffer << "0x" << setw(8) << setfill('0') << hex << field_token;
        buffer << ")) ";
      }  /* if */
      if (IsFdInitOnly(field_attributes)) {
        buffer << "initonly ";
      } else if (IsFdLiteral(field_attributes)) {
        buffer << "literal ";
      }  /* if */
      /* Emit the storage class.  "literal" implies "static". */
      if (IsFdStatic(field_attributes) && !IsFdLiteral(field_attributes)) {
        buffer << "static ";
      }  /* if */
      buffer << field_type->get_string(field_name);
      if (IsFdHasDefault(field_attributes)) {
        /* Note: we emit the value as a hexadecimal constant cast to the
           appropriate type.  This seems to work best for some corner
           cases. */
        a_constant_decoder decoder(constant_type, constant_value,
                                   characters_in_constant);
        if (constant_type == ELEMENT_TYPE_STRING ||
            constant_type == ELEMENT_TYPE_CLASS) {
          buffer << " = " << decoder.decode();
        } else {
          buffer << " = static_cast<" << field_type->get_string() << ">(";
          buffer << decoder.decode() << ')';
        }  /* if */
      }  /* if */
      buffer << ';' << END_OF_LINE;
    }  /* if */
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
  DWORD   attributes;

  hr = import_interface_->GetMethodProps(method_token, /*pClass=*/nullptr,
                                         /*szMethod=*/nullptr,
                                         /*cchMethod=*/0,
                                         /*pchMethod=*/0, &attributes,
                                         /*ppvSigBlob=*/nullptr,
                                         /*pcbSigBlob=*/nullptr,
                                         /*pulCodeRVA=*/nullptr,
                                         /*pdwImplFlags=*/nullptr);
  CHECK_API_RESULT(hr, GetMethodProps);
  return a_method_def(this, method_token, attributes);
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
  an_accessibility    accessibility;

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
  /* If there is a set method and/or a get method then import the necessary
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
  accessibility = an_accessibility::wider_accessibility(
                                                  get_method.accessibility(),
                                                  set_method.accessibility());
  if (accessibility.is_accessible()) {
    /* Now that we have everything we need emit the definition of the
       property. */
    a_signature_decoder decoder(import_scope_, signature, bytes_in_signature,
                                generic_parameters_,
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
    a_type_wrapper_ptr type = decoder.decode_method_signature(property_token);
    if (type != nullptr) {
      buffer << accessibility.get_string() << ": ";
      write_method_decl_specifiers(buffer, property_token, method_attributes);
      /* Modify the name of this property if it is the default-indexed
         property. */
      bool is_default_indexed_property = false;
      if (type->as_function() != nullptr) {
        /* The property is an indexed property, determine if it is the
           default-indexed property. */
        auto processor = import_scope_.
                               get_custom_attribute_processor(typedef_token_);
        auto default_member_name = processor->default_member_name();
        if (property_name == default_member_name) {
          /* The property matches the default member name of this class. */
          is_default_indexed_property = true;
        }  /* if */
      }  /* if */
      if (is_default_indexed_property) {
        property_name = L"default";
      } else {
        escape_invalid_identifier(property_name);
      }  /* */
      buffer << "property " << type->get_string(property_name);
      buffer << " {" << END_OF_LINE;
      /* Import the get and/or set method. */
      if (get_method.exists()) {
        import_one_method(buffer, get_method.token(), msGetter);
      }  /* if */
      if (set_method.exists()) {
        import_one_method(buffer, set_method.token(), msSetter);
      }  /* if */
      buffer << '}' << END_OF_LINE;
    }  /* if */
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
  DWORD   attributes;

  hr = import_interface_->GetMethodProps(method_token, /*pClass=*/nullptr,
                                         /*szMethod=*/nullptr,
                                         /*cchMethod=*/0,
                                         /*pchMethod=*/0, &attributes,
                                         /*ppvSigBlob=*/nullptr,
                                         /*pcbSigBlob=*/nullptr,
                                         /*pulCodeRVA=*/nullptr,
                                         /*pdwImplFlags=*/nullptr);
  CHECK_API_RESULT(hr, GetMethodProps);
  return a_method_def(this, method_token, attributes);
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
  /* If there is an add/remove/raise method then import the necessary
     information about the method. */
  add_method = import_event_method(add_method_token);
  remove_method = import_event_method(remove_method_token);
  if (!IsNilToken(raise_method_token)) {
    raise_method = import_event_method(raise_method_token);
  }  /* if */
  /* Set the accessibility of the event itself.  While is it expected that the
     add and remove methods have the same accessibility, if that isn't the
     case, we'll use the wider accessibility of the two. */
  check_assertion(add_method.accessibility() == remove_method.accessibility());
  accessibility = an_accessibility::wider_accessibility(
                                               add_method.accessibility(),
                                               remove_method.accessibility());
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
    a_qualified_name event_type = import_scope_.resolve_type_token(
                                                event_type_token,
                                                generic_parameters_,
                                                no_generic_method_parameters);
    if (!event_type.empty()) {
      escape_invalid_identifier(event_name);
      buffer << accessibility.get_string() << ": ";
      if (IsMdStatic(method_attributes)) {
        check_assertion(!IsMdVirtual(method_attributes));
        buffer << "static ";
      } else if (IsMdVirtual(method_attributes)) {
        buffer << "virtual ";
      }  /* if */
      buffer << "event ";
      buffer << event_type.as_string() << "^ " << event_name;
      buffer << " {" << END_OF_LINE;
      import_one_method(buffer, add_method.token(), msAddOn);
      import_one_method(buffer, remove_method.token(), msRemoveOn);
      if (raise_method.exists()) {
        import_one_method(buffer, raise_method.token(), msFire);
      }  /* if */
      buffer << '}' << END_OF_LINE;
    }  /* if */
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
  HCORENUM                       enum_typedefs = nullptr;
  mdTypeDef                      typedefs[64];
  ULONG                          count_of_typedefs;
  a_pending_constraint_type_list pending_constraint_types;

  do {
    HRESULT hr = import_interface_->EnumTypeDefs(&enum_typedefs, typedefs,
                                                 _countof(typedefs),
                                                 &count_of_typedefs);
    CHECK_API_RESULT(hr, EnumTypeDefs);
    for (ULONG i = 0; i < count_of_typedefs; ++i) {
      mdTypeDef enclosing_typedef;
      hr = import_interface_->GetNestedClassProps(typedefs[i],
                                                  &enclosing_typedef);
      if (SUCCEEDED(hr) && enclosing_typedef == typedef_token_) {
        import_scope_.import_one_type(buffer,
                                      typedefs[i],
                                      this,
                                      /*want_definition=*/false,
                                      /*class_body_only=*/false,
                                      &pending_constraint_types);
      }  /* if */
    }  /* for */
  } while (count_of_typedefs > 0);
  import_interface_->CloseEnum(enum_typedefs);
  /* Now that all nested types have been imported, re-declare all nested
     generic types that were declared with a pending constraint clause, this
     time with the complete constraint clause. */
  for (auto pending_constraint_types_iter = pending_constraint_types.begin();
       pending_constraint_types_iter != pending_constraint_types.end();
       ++pending_constraint_types_iter) {
    import_scope_.import_one_type(buffer, *pending_constraint_types_iter,
                                  this, /*want_definition=*/false,
                                  /*class_body_only=*/false,
                                  /*pending_constraint_types=*/nullptr);
  }  /* for */
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
  string pending_interface_list = process_base_class_list(buffer);
  buffer << " {" << END_OF_LINE;
  import_nested_classes(buffer);
  if (!pending_interface_list.empty())
  {
    buffer << "__implements "
           << pending_interface_list << ";" << END_OF_LINE;
  }  /* if */
  import_all_fields(buffer);
  get_method_impls();
  import_properties(buffer);
  import_events(buffer);
  /* Import the methods after all properties and events have been imported
     so that property/event methods that are not marked with the mdSpecialName
     attribute are not imported as ordinary member functions; see
     property_and_event_methods_. */
  import_all_methods(buffer);
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
                     a_generic_param_or_arg_iterator generic_parameters_begin,
                     a_generic_param_or_arg_iterator generic_parameters_end)
/*
Return a generic parameter list for a generic type or method.
*/
{
  wstring parameter_list;

  parameter_list = L"generic<";
  for (auto iter = generic_parameters_begin;
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

  check_assertion(generic_parameters.size() >= generic_arity);
  generic_header = form_generic_parameter_list(
                                     generic_parameters.end() - generic_arity,
                                     generic_parameters.end());
  if (!generic_constraints.empty()) {
    generic_header += form_generic_constraint_clause_list(
                                                         generic_constraints);
  }  /* if */
  return generic_header;
}  /* form_generic_method_header */


wstring an_import_scope::form_full_generic_parameter_list(
                 mdTypeDef                       typedef_token,
                 DWORD                           type_attributes,
                 a_generic_param_or_arg_iterator generic_parameters_begin,
                 a_generic_param_or_arg_iterator generic_parameters_end) const
/*
Return the generic header associated with this generic type.
*/
{
  wstring generic_header;
  BYTE    generic_arity = distance(generic_parameters_begin,
                                   generic_parameters_end);

  if (IsTdNested(type_attributes)) {
    HRESULT   hr;
    mdTypeDef enclosing_type_token;
    BYTE      enclosing_type_generic_param_count = 0;
    hr = import_interface_->GetNestedClassProps(typedef_token,
                                                &enclosing_type_token);
    CHECK_API_RESULT(hr, GetNestedClassProps);
    enclosing_type_generic_param_count =
                            get_generic_parameter_count(enclosing_type_token);
    if (enclosing_type_generic_param_count > 0) {
      DWORD enclosing_type_attributes;
      check_assertion(enclosing_type_generic_param_count <= generic_arity);
      generic_arity -= enclosing_type_generic_param_count;
      hr = import_interface_->GetTypeDefProps(enclosing_type_token,
                                              &enclosing_type_attributes);
      CHECK_API_RESULT(hr, GetTypeDefProps);
      generic_header = form_full_generic_parameter_list(
                                      enclosing_type_token,
                                      enclosing_type_attributes,
                                      generic_parameters_begin,
                                      generic_parameters_end - generic_arity);
    }  /* if */
  }  /* if */
  if (generic_arity > 0) {
    generic_header += form_generic_parameter_list(
                                       generic_parameters_end - generic_arity,
                                       generic_parameters_end);
  }  /* if */
  return generic_header;
}  /* an_import_scope::form_full_generic_parameter_list */


wstring an_import_scope::form_generic_type_header(
        mdTypeDef                         typedef_token,
        DWORD                             type_attributes,
        BYTE                              generic_arity,
        mdTypeDef                         enclosing_type_token,
        DWORD                             enclosing_type_attributes,
        const a_generic_param_or_arg_list &generic_parameters,
        const a_constraint_clause_list    &generic_constraints,
        a_boolean                         out_of_class_definition) const
/*
Return the generic header associated with this generic type.
*/
{
  wstring generic_header;

  /* When obtaining the definition of a type that is directly or indirectly a
     generic type, the generated code should be in the form of an out-of-class
     definition.  If this is a nested type and all of the generic parameters
     are not directly associated with it, then generate the generic parameter
     list for all enclosing generic type(s). */
  check_assertion(IsTdNested(type_attributes) ||
                  generic_arity == generic_parameters.size());
  if (out_of_class_definition && generic_arity < generic_parameters.size()) {
    generic_header = form_full_generic_parameter_list(
                                    enclosing_type_token,
                                    enclosing_type_attributes,
                                    generic_parameters.begin(),
                                    generic_parameters.end() - generic_arity);
  }  /* if */
  if (generic_arity > 0) {
    generic_header += form_generic_parameter_list(
                                     generic_parameters.end() - generic_arity,
                                     generic_parameters.end());
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
                    const a_type_definition        *enclosing_type_definition,
                    bool                           want_definition,
                    bool                           class_body_only,
                    a_pending_constraint_type_list *pending_constraint_types)
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
  HRESULT          hr;
  DWORD            attributes;
  mdToken          extends_token;
  a_top_level_kind kind;
  auto             import_flags = containing_assembly_.import_flags();
  bool             define_all_types =
                               (import_flags & cpp_cli_define_all_types) != 0;
  bool             import_as_friend =
                             (import_flags & cpp_cli_as_friend_assembly) != 0;
  bool             at_top_level = enclosing_type_definition == nullptr;

  hr = import_interface_->GetTypeDefProps(typedef_token,
                                          &attributes, &extends_token);
  CHECK_API_RESULT(hr, GetTypeDefProps);
  if (IsTdNested(attributes) && at_top_level && !class_body_only) {
    /* Do not emit nested types at top level scopes.  Nested types are
       emitted in their enclosing type. */
  } else {
    an_accessibility accessibility(containing_assembly_, typedef_token,
                                   attributes, enclosing_type_definition);
    if (!accessibility.is_accessible()) {
      /* Do not emit private types. */
      goto done;
    } /* if */
    mdTypeDef enclosing_type_token = mdTypeDefNil;
    DWORD     enclosing_type_attributes = 0;
    BYTE      enclosing_type_generic_param_count = 0;
    if (at_top_level) {
      if (IsTdNested(attributes)) {
        hr = import_interface_->GetNestedClassProps(typedef_token,
                                                    &enclosing_type_token);
        CHECK_API_RESULT(hr, GetNestedClassProps);
        enclosing_type_generic_param_count =
                            get_generic_parameter_count(enclosing_type_token);
        hr = import_interface_->GetTypeDefProps(enclosing_type_token,
                                                &enclosing_type_attributes);
        CHECK_API_RESULT(hr, GetTypeDefProps);
      }  /* if */
    } else {
      enclosing_type_token = enclosing_type_definition->token();
      enclosing_type_attributes = enclosing_type_definition->attributes();
      enclosing_type_generic_param_count = enclosing_type_definition
                                                ->generic_parameters().size();
    }  /* if */
    /* Get the generic parameters and constraints for this type. */
    a_generic_parameter_list generic_type_parameters;
    a_constraint_clause_list generic_constraints;
    BYTE                     generic_arity;
    generic_arity = get_generic_parameters_and_constraints(
                                           typedef_token,
                                           no_generic_type_parameters,
                                           enclosing_type_generic_param_count,
                                           generic_type_parameters,
                                           generic_constraints,
                                           pending_constraint_types);
    a_qualified_name full_type_name = resolve_type_token(
                                        typedef_token,
                                        generic_type_parameters,
                                        no_generic_method_parameters,
                                        /*omit_generic_params_or_args=*/true);
    if (full_type_name == L"_GUID") {
      /* _GUID is a built-in type in Microsoft mode.  Skip it. */
      goto done;
    }  /* if */
    /* Classify the type - ref class, value class, interface etc. */
    kind = classify_type(full_type_name.as_string(), attributes,
                         generic_type_parameters, extends_token);
    if (kind == tlk_delegate && !want_definition && !define_all_types) {
      /* If no definition is required, treat the delegate as a ref class
         since "delegate ..." is always a definition.  Doing so avoids
         declaration ordering problems. */
      kind = tlk_ref_class;
    }  /* if */
    wstring type_name = full_type_name.unqualified_name();
    /* Emit the namespace scopes and class head if required.  These are
       present on the original declaration and also on the definitions of
       generics. */
    if (!class_body_only || !generic_type_parameters.empty()) {
      if (at_top_level && !class_body_only) {
        /* Make sure that the correct namespace scopes are opened. */
        set_namespace_scope(buffer, full_type_name.namespace_name());
      }  /* if */
      if (!at_top_level) {
        /* Emit the access specifier for nested types. */
        buffer << accessibility.get_string() << ": ";
      }  /* if */
      /* Emit the generic header if this is directly or indirectly a generic
         type.  When obtaining the body of such a type, the generated code
         should be in the form of an out-of-class definition. */
      if (!generic_type_parameters.empty()) {
        a_boolean out_of_class_definition = at_top_level && class_body_only;
        buffer << form_generic_type_header(typedef_token, attributes,
                                           generic_arity,
                                           enclosing_type_token,
                                           enclosing_type_attributes,
                                           generic_type_parameters,
                                           generic_constraints,
                                           out_of_class_definition);
      }  /* if */
      /* Emit the assembly level visibility - either public or private. */
      if (at_top_level && want_definition && !IsTdNested(attributes)) {
        buffer << accessibility.get_string() << ' ';
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
         definition.  This is not needed in the case of a generic delegate
         since its definition is a complete declaration (including the
         generic<...> header and the keyword "delegate"). */
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
          buffer << full_type_name.as_string();
        } else {
          buffer << type_name;
        }  /* if */
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
  }  /* if */
done:
  return;
}  /* an_import_scope::import_one_type */


a_method_parameter::a_method_parameter(
                                    const an_import_scope &import_scope,
                                    a_type_wrapper_ptr type, mdParamDef token)
  : token_(token)
  , type_(move(type))
  , attributes_(0)
{
  HRESULT hr;
  auto    *import_interface = import_scope.import_interface();

  if (!IsNilToken(token_)) {
    hr = import_interface->GetParamProps(token_,
                                         /*method_token=*/nullptr,
                                         /*param_index=*/nullptr,
                                         name_,
                                         &attributes_,
                                         /*constant_type=*/nullptr,
                                         /*constant_value=*/nullptr,
                                         /*characters_in_constant=*/nullptr);
    escape_invalid_identifier(name_, /*force=*/true);

    CHECK_API_RESULT(hr, GetParamProps);
  }  /* if */
}  /* a_method_parameter::a_method_parameter. */


a_boolean a_method_parameter::is_parameter_array(
                                  const an_import_scope &import_scope) const
/*
Return TRUE if the method parameter is a parameter array.
*/
{
  HRESULT hr = S_FALSE;
  if (!IsNilToken(token_)) {
    hr = import_scope.import_interface()->GetCustomAttributeByName(
                                                token_,
                                                L"System.ParamArrayAttribute",
                                                /*ppData=*/nullptr,
                                                /*pcbData=*/nullptr);
    CHECK_API_RESULT(hr, GetCustomAttributeByName);
  }  /* if */
  return hr == S_OK;
}  /* a_method_parameter::is_parameter_array */


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
    a_type_wrapper_ptr type = decode_type();
    generic_arguments.push_back(type != nullptr ? type->get_string()
                                                : L"__error_type");
  }  /* for */
  return generic_arguments;
}  /* a_signature_decoder::decode_generic_arguments */


typedef unsigned int a_type_modifier_flag_set;
enum a_type_modifier_flag : a_type_modifier_flag_set
{
  tmf_none                       = 0x0000,
  tmf_compiler_marshal_override  = 0x0001,
  tmf_is_boxed                   = 0x0002,
  tmf_is_by_value                = 0x0004,
  tmf_is_const                   = 0x0008,
  tmf_is_copy_ctor               = 0x0010,
  tmf_is_cxx_reference           = 0x0020,
  tmf_is_cxx_udt_return          = 0x0040,
  tmf_is_explicitly_dereferenced = 0x0080,
  tmf_is_implicitly_dereferenced = 0x0100,
  tmf_is_long                    = 0x0200,
  tmf_is_rvalue_reference        = 0x0400,
  tmf_is_signed                  = 0x0800,
  tmf_is_sign_unspecified_byte   = 0x1000,
  tmf_is_volatile                = 0x2000,
  tmf_unknown                    = 0x4000,
};


static a_type_modifier_flag type_name_to_modifier_flag(const wstring &name)
/*
Return the type modifier flag that corresponds to the specified class name, or
tmf_unknown if no such mapping exists.
*/
{
  static const struct a_type_name_to_modifier_flag_map
  {
    LPCWSTR              name;
    a_type_modifier_flag modifier_flag;
  } type_name_to_modifier_flag_map[] = {
    { L"Microsoft::VisualC::IsMarshalWorkaround",
                                              tmf_compiler_marshal_override },
    { L"System::Runtime::CompilerServices::CompilerMarshalOverride",
                                              tmf_compiler_marshal_override },
    { L"Microsoft::VisualC::IsBoxedModifier",       tmf_is_boxed },
    { L"System::Runtime::CompilerServices::IsBoxed", tmf_is_boxed },
    { L"Microsoft::VisualC::IsByValueModifier",       tmf_is_by_value },
    { L"System::Runtime::CompilerServices::IsByValue", tmf_is_by_value },
    { L"Microsoft::VisualC::IsConstModifier",       tmf_is_const },
    { L"System::Runtime::CompilerServices::IsConst", tmf_is_const },
    { L"Microsoft::VisualC::IsCopyCtorModifier", tmf_is_copy_ctor },
    { L"Microsoft::VisualC::IsCXXReferenceModifier", tmf_is_cxx_reference },
    { L"Microsoft::VisualC::CxxUdtReturnStyleModifier",
                                                      tmf_is_cxx_udt_return },
    { L"System::Runtime::CompilerServices::IsUdtReturn",
                                                      tmf_is_cxx_udt_return },
    { L"Microsoft::VisualC::IsCXXPointerModifier",
                                             tmf_is_explicitly_dereferenced },
    { L"System::Runtime::CompilerServices::IsExplicitlyDereferenced",
                                             tmf_is_explicitly_dereferenced },
    { L"Microsoft::VisualC::IsImplicitlyDereferencedModifier",
                                             tmf_is_implicitly_dereferenced },
    { L"System::Runtime::CompilerServices::IsImplicitlyDereferenced",
                                             tmf_is_implicitly_dereferenced },
    { L"Microsoft::VisualC::IsLongModifier",       tmf_is_long },
    { L"System::Runtime::CompilerServices::IsLong", tmf_is_long },
    { L"Microsoft::VisualC::IsSignedModifier", tmf_is_signed },
    { L"Microsoft::VisualC::NoSignSpecifiedModifier",
                                               tmf_is_sign_unspecified_byte },
    { L"System::Runtime::CompilerServices::IsSignUnspecifiedByte",
                                               tmf_is_sign_unspecified_byte },
    { L"Microsoft::VisualC::IsVolatileModifier",       tmf_is_volatile },
    { L"System::Runtime::CompilerServices::IsVolatile", tmf_is_volatile },
  };
  a_type_modifier_flag modifier_flag = tmf_unknown;

  for (int i = 0; i < _countof(type_name_to_modifier_flag_map); ++i) {
    if (name == type_name_to_modifier_flag_map[i].name) {
      modifier_flag = type_name_to_modifier_flag_map[i].modifier_flag;
      break;
    }  /* if */
  }  /* for */
  return modifier_flag;
}  /* type_name_to_modifier_flag */

a_type_wrapper_ptr a_signature_decoder::decode_modified_type(
                                                  CorElementType element_type)
/*
Decode a type signature that is modified with a custom type modifier.
*/
{
  a_type_modifier_flag_set modifier_flags = tmf_none;
  a_type_wrapper_ptr       type;
  a_type_wrapper_ptr       boxed_type;

  /* Accumulate the type modifiers. */
  check_assertion(element_type == ELEMENT_TYPE_CMOD_REQD ||
                  element_type == ELEMENT_TYPE_CMOD_OPT);
  for(;;) {
    a_type_modifier_flag modifier_flag = tmf_unknown;
    mdToken type_token = read_token();
    if (TypeFromToken(type_token) == mdtTypeDef ||
        TypeFromToken(type_token) == mdtTypeRef) {
      a_qualified_name type_name = import_scope_.resolve_type_token(
                                                  type_token,
                                                  generic_type_parameters_,
                                                  generic_method_parameters_);
      modifier_flag = type_name_to_modifier_flag(type_name.as_string());
    }  /* if */
    if (modifier_flag == tmf_unknown) {
      if (element_type == ELEMENT_TYPE_CMOD_REQD) {
        /* This is an unknown required type modifier. */
        goto done;
      } else {
        /* Ignore any unknown optional type modifiers, but keep track of the
           fact that one was encountered. */
        contains_unknown_optional_type_modifiers_ = true;
      }  /* if */
    } else if (modifier_flag == tmf_is_implicitly_dereferenced &&
               (modifier_flags & tmf_is_implicitly_dereferenced) != 0) {
      /* This is the second "IsImplicitlyDereferenced" modifier we have
         encountered, which indicates it is an rvalue reference. */
      modifier_flags |= tmf_is_rvalue_reference;
    } else {
      modifier_flags |= modifier_flag;
    }  /* if */
    if (modifier_flag == tmf_is_boxed) {
      /* This is the "IsBoxed" modifier.  The next modifier is the boxed enum
         or value class type. */
      element_type = get_element_type();
      check_assertion(element_type == ELEMENT_TYPE_CMOD_OPT);
      mdToken boxed_type_token = read_token();
      a_qualified_name boxed_type_name = import_scope_.resolve_type_token(
                                                  boxed_type_token,
                                                  generic_type_parameters_,
                                                  generic_method_parameters_);
      boxed_type = make_shared<a_class_type_wrapper>(
                                         a_class_type_wrapper::ck_value_class,
                                         move(boxed_type_name));
      /* All of the modifiers that apply to the handle to System::Object,
         System::ValueType, or System::Enum that follows have already been
         accumulated.  Break out of the loop now so that any other modifiers
         are applied to the boxed type itself.  This is necessary to
         differentiate "const V^" from "V^ const":
         const V^:
           modopt(Boxed) modopt(V) modopt(Const) Class System::ValueType
         V^ const:
           modopt(Const) modopt(Boxed) modopt(V) Class System::ValueType */
      break;
    } /* if */
    element_type = peek_element_type();
    if (element_type != ELEMENT_TYPE_CMOD_REQD &&
        element_type != ELEMENT_TYPE_CMOD_OPT) {
      break;
    }  /* if */
    element_type = get_element_type();
  }  /* for */
  /* Decode the modified type. */
  type = decode_type();
  if (type == nullptr) goto done;
  /* Apply the modifiers to the decoded type. */
  if ((modifier_flags & tmf_is_boxed) != 0) {
    check_assertion(boxed_type);
    auto indirection = type->as_indirection();
    if (indirection != nullptr &&
        indirection->is_of_indirection_kind(a_type_indirection::tik_handle)) {
      auto class_type = indirection->underlying_type()->as_class();
      if (class_type != nullptr &&
          (class_type->name() == MAKE_CLASS_STRING(Object) ||
           class_type->name() == MAKE_CLASS_STRING(ValueType) ||
           class_type->name() == MAKE_CLASS_STRING(Enum))) {
        /* Any cv-qualifiers on the System::Object, System::ValueType, or
           System::Enum handle apply to the boxed handle type. */
        auto qualifier_flags = type->qualifier_flags();
        type = make_shared<a_type_indirection>(a_type_indirection::tik_handle,
                                               move(boxed_type));
        type->set_qualifier_flags(qualifier_flags);
      } else {
        unexpected_condition();
        type.reset();
        goto done;
      }  /* if */
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
  }  /* if */
  if ((modifier_flags & tmf_is_long) != 0) {
    if ((modifier_flags &
         (tmf_is_signed | tmf_is_sign_unspecified_byte)) != 0) {
      unexpected_condition();
      type.reset();
      goto done;
    } else if (type->is_of_kind(a_type_wrapper::twk_int)) {
      type->set_kind(a_type_wrapper::twk_long);
    } else if (type->is_of_kind(a_type_wrapper::twk_unsigned_int)) {
      type->set_kind(a_type_wrapper::twk_unsigned_long);
    } else if (type->is_of_kind(a_type_wrapper::twk_double)) {
      type->set_kind(a_type_wrapper::twk_long_double);
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* */
  } else if ((modifier_flags & tmf_is_signed) != 0) {
    if ((modifier_flags & tmf_is_sign_unspecified_byte) != 0) {
      unexpected_condition();
      type.reset();
      goto done;
    } else if (type->is_of_kind(a_type_wrapper::twk_char)) {
      type->set_kind(a_type_wrapper::twk_signed_char);
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* */
  } else if ((modifier_flags & tmf_is_sign_unspecified_byte) != 0) {
    if (type->is_of_kind(a_type_wrapper::twk_signed_char) ||
        type->is_of_kind(a_type_wrapper::twk_unsigned_char)) {
      type->set_kind(a_type_wrapper::twk_char);
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* */
  }  /* if */
  if ((modifier_flags & tmf_is_implicitly_dereferenced) != 0) {
    /* Reference types are encoded as implicitly dereferenced handles or
       pointers. */
    auto indirection = type->as_indirection();
    if (indirection != nullptr &&
        (indirection->is_of_indirection_kind(
                                           a_type_indirection::tik_pointer) ||
         indirection->is_of_indirection_kind(
                                           a_type_indirection::tik_handle))) {
      a_type_wrapper_ptr underlying_type = indirection->underlying_type();
      /* Any cv-qualifiers apply to the underlying type, not the type
         indirection, so apply them here rather than below. */
      if ((modifier_flags & tmf_is_const) != 0) {
        underlying_type->add_qualifier_flags(a_type_wrapper::qf_const);
        modifier_flags &= ~tmf_is_const;
      }  /* if */
      if ((modifier_flags & tmf_is_volatile) != 0) {
        underlying_type->add_qualifier_flags(a_type_wrapper::qf_volatile);
        modifier_flags &= ~tmf_is_volatile;
      }  /* if */
      if (indirection->is_of_indirection_kind(
                                            a_type_indirection::tik_handle)) {
        /* A tracking reference is encoded as an implicitly dereferenced
           handle. */
        indirection->set_indirection_kind(
                                  a_type_indirection::tik_tracking_reference);
      } else {
        check_assertion(indirection->is_of_indirection_kind(
                                            a_type_indirection::tik_pointer));
        if ((modifier_flags & tmf_is_rvalue_reference) != 0) {
          /* An rvalue reference is encoded as a twice implicitly dereferenced
             pointer. */
          indirection->set_indirection_kind(
                                    a_type_indirection::tik_rvalue_reference);
        } else {
          /* A reference is encoded as an implicitly dereferenced pointer. */
          indirection->set_indirection_kind(
                                           a_type_indirection::tik_reference);
        }  /* if */
      }  /* if */
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
  }  /* if */
  if ((modifier_flags & tmf_is_by_value) != 0) {
    /* A ref class passed by value is encoded as a handle with the
       IsByValue modifier. */
    auto indirection = type->as_indirection();
    if (indirection != nullptr &&
        indirection->is_of_indirection_kind(a_type_indirection::tik_handle) &&
        indirection->underlying_type()->is_of_kind(
                                                 a_type_wrapper::twk_class)) {
      type = indirection->underlying_type();
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
  }  /* if */
  if ((modifier_flags & tmf_is_const) != 0) {
    type->add_qualifier_flags(a_type_wrapper::qf_const);
  }  /* if */
  if ((modifier_flags & tmf_is_volatile) != 0) {
    type->add_qualifier_flags(a_type_wrapper::qf_volatile);
  }  /* if */
  if ((modifier_flags & tmf_is_cxx_reference) != 0) {
    /* FIXME: This modifier was formerly used to encode reference types when
       compiling with Microsoft's Managed Extensions for C++ (now superseded
       by C++/CLI).  Importing such metadata is not yet supported. */
  }  /* if */
  if ((modifier_flags & tmf_is_explicitly_dereferenced) != 0) {
    /* An interior_ptr is encoded as an explicitly dereferenced tracking
       reference. */
    auto indirection = type->as_indirection();
    if (indirection != nullptr &&
        indirection->is_of_indirection_kind(
                                a_type_indirection::tik_tracking_reference)) {
      indirection->set_indirection_kind(
                                    a_type_indirection::tik_interior_pointer);
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
  }  /* if */
  if ((modifier_flags & tmf_compiler_marshal_override) != 0) {
    if (type->is_of_kind(a_type_wrapper::twk_unsigned_char)) {
      type->set_kind(a_type_wrapper::twk_bool);
    } else if (type->is_of_kind(a_type_wrapper::twk_unsigned_short)) {
      type->set_kind(a_type_wrapper::twk_wchar_t);
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
  }  /* if */
  if ((modifier_flags & tmf_is_cxx_udt_return) != 0) {
    if (type->is_of_kind(a_type_wrapper::twk_void)) {
      type->set_kind(a_type_wrapper::twk_cxx_udt_return);
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
  }  /* if */
  if ((modifier_flags & tmf_is_copy_ctor) != 0) {
    if (type->is_of_kind(a_type_wrapper::twk_void)) {
      type->set_kind(a_type_wrapper::twk_copy_ctor);
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
  }  /* if */
done:
  return type;
}  /* a_signature_decoder::decode_modified_type */


a_type_wrapper_ptr a_signature_decoder::decode_type()
/*
Decode a type signature and return it as a std::wstring.
*/
{
  a_type_wrapper_ptr type;
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
      type = make_shared<a_type_wrapper>(a_type_wrapper::twk_void);
      break;
    case ELEMENT_TYPE_BOOLEAN:
      type = make_shared<a_type_wrapper>(a_type_wrapper::twk_bool);
      break;
    case ELEMENT_TYPE_CHAR:
      if ((import_scope_.containing_assembly().import_flags()
                                         & cpp_cli_wchar_t_is_keyword) != 0) {
        type = make_shared<a_type_wrapper>(a_type_wrapper::twk_wchar_t);
      } else {
        type = make_shared<a_type_wrapper>(
                                          a_type_wrapper::twk_unsigned_short);
      }  /* if */
      break;
    case ELEMENT_TYPE_I1:
      type = make_shared<a_type_wrapper>(a_type_wrapper::twk_signed_char);
      break;
    case ELEMENT_TYPE_U1:
      type = make_shared<a_type_wrapper>(a_type_wrapper::twk_unsigned_char);
      break;
    case ELEMENT_TYPE_I2:
      type = make_shared<a_type_wrapper>(a_type_wrapper::twk_short);
      break;
    case ELEMENT_TYPE_U2:
      type = make_shared<a_type_wrapper>(a_type_wrapper::twk_unsigned_short);
      break;
    case ELEMENT_TYPE_I4:
      type = make_shared<a_type_wrapper>(a_type_wrapper::twk_int);
      break;
    case ELEMENT_TYPE_U4:
      type = make_shared<a_type_wrapper>(a_type_wrapper::twk_unsigned_int);
      break;
    case ELEMENT_TYPE_I8:
      type = make_shared<a_type_wrapper>(a_type_wrapper::twk_long_long);
      break;
    case ELEMENT_TYPE_U8:
      type = make_shared<a_type_wrapper>(
                                      a_type_wrapper::twk_unsigned_long_long);
      break;
    case ELEMENT_TYPE_R4:
      type = make_shared<a_type_wrapper>(a_type_wrapper::twk_float);
      break;
    case ELEMENT_TYPE_R8:
      type = make_shared<a_type_wrapper>(a_type_wrapper::twk_double);
      break;
    case ELEMENT_TYPE_STRING:
      { auto string_type = a_class_type_wrapper::create_system_class(
                                   a_class_type_wrapper::ck_class, L"String");
        type = make_shared<a_type_indirection>(a_type_indirection::tik_handle,
                                               move(string_type));
        break;
      }
    case ELEMENT_TYPE_PTR:
      { a_type_wrapper_ptr underlying_type;
        /* If we are decoding a type signature associated with a member of
           System::String, then we should perform the following conversions:
            ELEMENT_TYPE_PTR ELEMENT_TYPE_I1   -> 'const char*'
            ELEMENT_TYPE_PTR ELEMENT_TYPE_CHAR -> 'const __wchar_t*' */
        if (is_system_string_member_ &&
            peek_element_type() == ELEMENT_TYPE_I1) {
          /* ELEMENT_TYPE_I1 would normally be converted to "signed char", but
              we only want "char", so we don't recursively call decode_type
              in this case. */
          (void)get_element_type();
          underlying_type = make_shared<a_type_wrapper>(
                                                    a_type_wrapper::twk_char);
          underlying_type->add_qualifier_flags(a_type_wrapper::qf_const);
        } else if (is_system_string_member_ &&
                   peek_element_type() == ELEMENT_TYPE_CHAR) {
          underlying_type = decode_type();
          underlying_type->add_qualifier_flags(a_type_wrapper::qf_const);
        } else {
          underlying_type = decode_type();
        }  /* if */
        if (underlying_type != nullptr) {
          type = make_shared<a_type_indirection>(
                                              a_type_indirection::tik_pointer,
                                              move(underlying_type));
        }  /* if */
        break;
      }
    case ELEMENT_TYPE_BYREF:
      { a_type_wrapper_ptr underlying_type = decode_type();
        if (underlying_type != nullptr) {
          type = make_shared<a_type_indirection>(
                                   a_type_indirection::tik_tracking_reference,
                                   move(underlying_type));
        }  /* if */
        break;
      }
    case ELEMENT_TYPE_VALUETYPE:
    case ELEMENT_TYPE_CLASS:
      { mdToken token = read_token();
        check_assertion(TypeFromToken(token) == mdtTypeDef ||
                        TypeFromToken(token) == mdtTypeRef);
        a_qualified_name name = import_scope_.resolve_type_token(
                                       token,
                                       is_generic ? decode_generic_arguments()
                                                  : generic_type_parameters_,
                                       generic_method_parameters_);
        if (element_type == ELEMENT_TYPE_CLASS) {
          auto class_type = make_shared<a_class_type_wrapper>(
                                               a_class_type_wrapper::ck_class,
                                               move(name));
          type = make_shared<a_type_indirection>(
                                               a_type_indirection::tik_handle,
                                               move(class_type));
        } else {
          type = make_shared<a_class_type_wrapper>(
                                         a_class_type_wrapper::ck_value_class,
                                         move(name));
        }  /* if */
        break;
      }
    case ELEMENT_TYPE_VAR:
      { a_qualified_name generic_type_parameter;
        generic_type_parameter.append_identifier(
                                   generic_type_parameters_[read_one_byte()]);
        type = make_shared<a_class_type_wrapper>(
                                   a_class_type_wrapper::ck_generic_parameter,
                                   move(generic_type_parameter));
        break;
      }
    case ELEMENT_TYPE_TYPEDBYREF:
      type = a_class_type_wrapper::create_system_class(
                     a_class_type_wrapper::ck_value_class, L"TypedReference");
      break;
    case ELEMENT_TYPE_I:
      type = a_class_type_wrapper::create_system_class(
                             a_class_type_wrapper::ck_value_class, L"IntPtr");
      break;
    case ELEMENT_TYPE_U:
      type = a_class_type_wrapper::create_system_class(
                            a_class_type_wrapper::ck_value_class, L"UIntPtr");
      break;
    case ELEMENT_TYPE_OBJECT:
      { auto object_type = a_class_type_wrapper::create_system_class(
                                   a_class_type_wrapper::ck_class, L"Object");
        type = make_shared<a_type_indirection>(a_type_indirection::tik_handle,
                                               move(object_type));
        break;
      }
    case ELEMENT_TYPE_SZARRAY:
      { a_type_wrapper_ptr underlying_type = decode_type();
        if (underlying_type != nullptr) {
          type = an_array_type_wrapper::create_handle_to_array(
                                                             underlying_type);
        }  /* if */
        break;
      }
    case ELEMENT_TYPE_ARRAY:
      { /* Get the underlying type. */
        a_type_wrapper_ptr underlying_type = decode_type();
        if (underlying_type != nullptr) {
          /* Get the Rank of the array. */
          ULONG rank, num_of_sizes, num_of_lower_bounds;
          rank = read_four_bytes();
          num_of_sizes = read_four_bytes();
          for (ULONG i = 0; i < num_of_sizes; ++i) {
            /* We don't need the sizes.  They don't affect the type name.
               However, we need to consume these bytes in the signature
               blob. */
            (void)read_four_bytes();
          }  /* if */
          num_of_lower_bounds = read_four_bytes();
          for (ULONG i = 0; i < num_of_lower_bounds; ++i) {
            /* We don't need the lower bounds.  They don't affect the
               type name.  However, we need to consume these bytes in the
               signature blob. */
            (void)read_four_bytes();
          }  /* if */
          type = an_array_type_wrapper::create_handle_to_array(
                                                       underlying_type, rank);
        }  /* if */
        break;
      }
    case ELEMENT_TYPE_MVAR:
      { a_qualified_name generic_method_parameter;
        generic_method_parameter.append_identifier(
                                 generic_method_parameters_[read_one_byte()]);
        type = make_shared<a_class_type_wrapper>(
                                   a_class_type_wrapper::ck_generic_parameter,
                                   move(generic_method_parameter));
        break;
      }
    case ELEMENT_TYPE_CMOD_REQD:
    case ELEMENT_TYPE_CMOD_OPT:
      type = decode_modified_type(element_type);
      break;
    case ELEMENT_TYPE_INTERNAL:
      break;
    case ELEMENT_TYPE_FNPTR:
      { a_type_wrapper_ptr function_type = decode_method_signature(mdTokenNil);
        type = make_shared<a_type_indirection>(
                                              a_type_indirection::tik_pointer,
                                              move(function_type));
        break;
      }
    default:
      unexpected_condition();
      break;
  }  /* switch */
  return type;
}  /* a_signature_decoder::decode_type */


a_type_wrapper_ptr a_signature_decoder::decode_method_signature(
                                                     mdToken token,
                                                     bool    return_type_only)
/*
Decode a function signature and then combine the various elements along with
the name of the function to create a declaration which we return as an
a_function_type_wrapper_ptr.  Note: this function also handles decoding a
property signature which is almost the same as a method signature.
*/
{
  a_type_wrapper_ptr      type;
  a_type_wrapper_ptr      return_type;
  a_method_parameter_list parameter_list;
  BYTE                    first_byte;
  BYTE                    calling_convention;
  ULONG                   number_of_parameters;
  ULONG                   param_index = 1;
  BYTE                    generic_arity;
  bool                    is_method_def;
  bool                    is_member_ref;
  bool                    is_property;
  wstring                 generic_header;
  HRESULT                 hr;
  an_import_interface     *import_interface= import_scope_.import_interface();

  is_method_def = TypeFromToken(token) == mdtMethodDef;
  is_member_ref = TypeFromToken(token) == mdtMemberRef;
  is_property = TypeFromToken(token) == mdtProperty;
  first_byte = read_one_byte();
  calling_convention = first_byte & IMAGE_CEE_CS_CALLCONV_MASK;
  check_assertion(IsNilToken(token) || is_method_def || is_member_ref ||
                  (is_property &&
                   calling_convention == IMAGE_CEE_CS_CALLCONV_PROPERTY));

  /* If this is a generic method, read the count of generic parameters.
     Note that this will differ from generic_parameters_.size() for
     generic types or methods nested within generic types. */
  if ((first_byte & IMAGE_CEE_CS_CALLCONV_GENERIC) != 0) {
    check_assertion(is_method_def);
    generic_arity = read_one_byte();
    a_constraint_clause_list generic_method_constraints;
    check_assertion(generic_method_parameters_.empty());
    import_scope_.get_generic_parameters_and_constraints(
                                        token,
                                        generic_type_parameters_,
                                        generic_type_parameters_.size(),
                                        generic_method_parameters_,
                                        generic_method_constraints,
                                        /*pending_constraint_types=*/nullptr);
    check_assertion(generic_arity == generic_method_parameters_.size());
    generic_header = form_generic_method_header(generic_arity,
                                                generic_method_parameters_,
                                                generic_method_constraints);
  }  /* if */
  number_of_parameters = read_four_bytes();
  parameter_list.reserve(number_of_parameters);
  return_type = decode_type();
  if (return_type != nullptr) {
    if (return_type->is_of_kind(a_type_wrapper::twk_copy_ctor)) {
      /* Legacy encodings of the copy constructor encoded it as a regular
         function (not a .ctor) with a void return type marked with the
         IsCopyCtorModifier modifier. */
      return_type.reset();
    } else if (return_type->is_of_kind(a_type_wrapper::twk_cxx_udt_return)) {
      /* A function that returns a ref class by value is encoded as a
         function with a void return type (marked with the IsUdtReturn
         modifier) and who's first parameter is a tracking reference to
         a handle to the ref class.  */
      if (number_of_parameters > 0) {
        ++param_index;
        a_type_wrapper_ptr param_type = decode_type();
        if (param_type == nullptr) {
          type.reset();
          goto done;
        }  /* if */
        auto indirection = param_type->as_indirection();
        if (indirection != nullptr &&
            indirection->is_of_indirection_kind(
                                a_type_indirection::tik_tracking_reference)) {
          auto handle_type = indirection->underlying_type()->as_indirection();
          if (handle_type != nullptr &&
              handle_type->is_of_indirection_kind(
                                            a_type_indirection::tik_handle)) {
            auto class_type = handle_type->underlying_type()->as_class();
            if (class_type != nullptr &&
                class_type->is_of_class_kind(
                                            a_class_type_wrapper::ck_class)) {
              return_type = move(class_type);
              goto have_return_type;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
have_return_type:
    if (return_type_only) {
      type = move(return_type);
      goto done;
    }  /* if */
    for (; param_index <= number_of_parameters; ++param_index) {
      mdParamDef         param_token = mdParamDefNil;
      a_type_wrapper_ptr param_type = decode_type();
      if (param_type == nullptr) {
        type.reset();
        goto done;
      }  /* if */
      if (is_method_def) {
        hr = import_interface->GetParamForMethodIndex(token, param_index,
                                                      &param_token);
        CHECK_API_RESULT(hr, GetParamForMethodIndex);
      }  /* if */
      parameter_list.push_back(a_method_parameter(import_scope_,
                                                  move(param_type),
                                                  param_token));
    }  /* for */
    /* Mark the last parameter as a parameter array if appropriate. */
    if (!parameter_list.empty()) {
      auto last_parameter = parameter_list.back();
      auto array_type = last_parameter.type()->as_handle_to_array();
      if (array_type != nullptr &&
          last_parameter.is_parameter_array(import_scope_)) {
        array_type->set_array_kind(an_array_type_wrapper::ak_param_array);
      }  /* if */
    }  /* if */
    if (is_property && parameter_list.empty()) {
      /* An ordinary property. */
      type = move(return_type);
    } else {
      /* A method or parameterized property. */
      type = make_shared<a_function_type_wrapper>(calling_convention,
                                                  move(return_type),
                                                  move(parameter_list),
                                                  move(generic_header));
    }  /* if */
  }  /* if */
done:
  return type;
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
  HRESULT hr;

  hr = CLRCreateInstance(CLSID_CLRMetaHostPolicy, IID_ICLRMetaHostPolicy,
                         reinterpret_cast<LPVOID*>(&clr_metahost_policy_));
  CHECK_API_RESULT(hr, CLRCreateInstance);
  return (clr_metahost_policy_ != nullptr);
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
  return (clr_runtime_info_ != nullptr);
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
  iter->second.find_scope().import_one_type(
                                        buffer, typedef_token,
                                        /*enclosing_type_definition=*/nullptr,
                                        /*want_definition=*/true,
                                        class_body_only,
                                        /*pending_constraint_types=*/nullptr);
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
