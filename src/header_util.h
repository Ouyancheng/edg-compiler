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

header_util.h -- General utility components (mostly templates) intended for use
in both headers and compilation units.  Utilities declared here are guaranteed
to be free from dependence on undefined entities in the front end.

*/

#ifndef EDG_HEADER_UTIL_H
#define EDG_HEADER_UTIL_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
A generic utility for saving and automatically restoring the original value of
a variable whose value is temporarily being changed.
*/
template<typename a_Var_type>
struct Value_saver {
  inline Value_saver(a_Var_type *const var_to_be_saved);
  inline Value_saver(a_Var_type *const var_to_be_saved,
                     const a_Var_type  &new_value);
  inline ~Value_saver();
private:
  a_Var_type *const
                saved_var;
                        /* A pointer to the variable where the saved value
                           should be restored upon destruction. */
  a_Var_type
                saved_value;
                        /* The value captured during construction to be
                           restored upon destruction. */
};  /* Value_saver */


template<typename a_Var_type>
inline Value_saver<a_Var_type>::Value_saver(a_Var_type *const var_to_be_saved)
  : saved_var(var_to_be_saved), saved_value(*var_to_be_saved)
/*
Save the current value of the given variable (var_to_be_saved).  The saved
value will be restored upon destruction.
*/
{
}  /* Value_saver::Value_saver */


template<typename a_Var_type>
inline Value_saver<a_Var_type>::Value_saver(a_Var_type *const var_to_be_saved,
                                            const a_Var_type  &new_value)
  : Value_saver(var_to_be_saved)
/*
Save the current value of the given variable (var_to_be_saved).  Then, update
the variable's value to the given new value.  The saved value will be restored
upon destruction.
*/
{
  *saved_var = new_value;
}  /* Value_saver::Value_saver */


template<typename a_Var_type>
inline Value_saver<a_Var_type>::~Value_saver()
/*
Restore the variable given during construction to its saved state.
*/
{
  *saved_var = saved_value;
}  /* Value_saver::~Value_saver */


/*
An implementation of an "optional" type.  This type allows representing
a value that may or may not be present and is thus "optional."

Before dereferencing to retrieve the value, consuming code should check that
the optional has a stored value (via a call to "has_value").  When checking is
enabled, this contract is strictly enforced to ensure that the calling logic
doesn't accidentally forget a check.
*/
template<typename a_Value_type>
struct Opt {
  Opt() : storing_value(FALSE)
    {}
  Opt(const a_Value_type &value)
    : storing_value(TRUE), stored_value(value)
    {}
  Opt(a_Value_type &&value)
    : storing_value(TRUE), stored_value(static_cast<a_Value_type &&>(value))
    {}
  inline Opt(const Opt<a_Value_type> &other);
  inline Opt(Opt<a_Value_type> &&other);
  inline ~Opt();
  a_boolean has_value() const;
  /* Value retrieval functions. */
  inline const a_Value_type* operator->() const;
  inline const a_Value_type& operator*() const;
  /* Value update functions. */
  inline Opt<a_Value_type>& operator=(const a_Value_type &value);
  inline Opt<a_Value_type>& operator=(a_Value_type &&value);
  inline Opt<a_Value_type>& operator=(const Opt<a_Value_type> &other);
  inline Opt<a_Value_type>& operator=(Opt<a_Value_type> &&other);
  inline void clear();
private:
  a_boolean     storing_value;
                        /* TRUE if there is a value stored, FALSE otherwise. */
#ifdef UNION_AS_STRUCT
/* FIXME: Workaround for union-as-struct build issue. */
#undef union
#endif /* indef UNION_AS_STRUCT */
  union {
    a_Value_type
                stored_value;
                        /* The value stored.  Represented as a union so the
                           value can be uninitialized, and construction and
                           destruction are manually managed. */
#ifdef UNION_AS_STRUCT
#define union struct
#endif /* indef UNION_AS_STRUCT */
  };
#if CHECKING
  a_boolean     value_presence_checked = FALSE;
                        /* TRUE if there was a call to has_value for this
                           instance of Opt, FALSE otherwise. */
#endif /* CHECKING */
};


template<typename a_Value_type>
inline Opt<a_Value_type>::Opt(const Opt<a_Value_type> &other)
  : storing_value(other.storing_value)
/*
Copy construct an optional from another optional.
*/
{
  /* A value was stored, copy it. */
  if (storing_value) {
    ::new (&stored_value) a_Value_type(other.stored_value);
  }  /* if */
}  /* Opt */


template<typename a_Value_type>
inline Opt<a_Value_type>::Opt(Opt<a_Value_type> &&other)
  : storing_value(other.storing_value)
/*
Move construct an optional from another optional.
*/
{
  /* A value was stored, move it. */
  if (storing_value) {
    ::new (&stored_value) a_Value_type(
                             static_cast<a_Value_type &&>(other.stored_value));
  }  /* if */
}  /* Opt */


template<typename a_Value_type>
inline Opt<a_Value_type>::~Opt()
/*
Destruct an optional invoking the destructor for the stored value if there is
one.
*/
{
  /* A value was stored, make sure its destructor is invoked. */
  if (storing_value) {
    stored_value.~a_Value_type();
  }  /* if */
}  /* ~Opt */


template<typename a_Value_type>
inline a_boolean Opt<a_Value_type>::has_value() const
/*
Return TRUE if this optional is storing a value, otherwise return FALSE.
*/
{
#if CHECKING
  /* In checking builds break constness to record that the presence of a value
     was checked for before being accessed. */
  const_cast<Opt<a_Value_type>*>(this)->value_presence_checked = TRUE;
#endif /* CHECKING */
  return storing_value;
}  /* ~Opt */


template<typename a_Value_type>
inline const a_Value_type* Opt<a_Value_type>::operator->() const
/*
This function is only valid when the Opt is not empty.  The stored
value is returned.
*/
{
  /* Check that the caller previously checked for a value. */
  check_assertion_str(value_presence_checked, "missing call to has_value");
  /* Check that a value is present. */
  check_assertion_str(storing_value, "the optional was empty");
  return &stored_value;
}  /* operator-> */


template<typename a_Value_type>
inline const a_Value_type& Opt<a_Value_type>::operator*() const
/*
This function is only valid when the Opt is not empty.  The stored
value is returned.
*/
{
  /* Check that the caller previously checked for a value. */
  check_assertion_str(value_presence_checked, "missing call to has_value");
  /* Check that a value is present. */
  check_assertion_str(storing_value, "the optional was empty");
  return stored_value;
}  /* operator* */


template<typename a_Value_type>
inline Opt<a_Value_type>& Opt<a_Value_type>::operator=(
                                                     const a_Value_type &value)
/*
Store the given value as the new stored value via a copy.  The updated optional
is returned.
*/
{
  /* If a value was stored previously, use the value type's normal copy
     assignment operator; otherwise, use placement new to initialize the memory
     and update the storage flag. */
  if (storing_value) {
    stored_value = value;
  } else {
    storing_value = TRUE;
    ::new (&stored_value) a_Value_type(value);
  }  /* if */
  return *this;
}  /* operator= */


template<typename a_Value_type>
inline Opt<a_Value_type>& Opt<a_Value_type>::operator=(a_Value_type &&value)
/*
Store the given value as the new stored value via a move.  The updated optional
is returned.
*/
{
  /* If a value was stored previously, use the value type's normal move
     assignment operator; otherwise, use placement new to initialize the memory
     and update the storage flag. */
  if (storing_value) {
    stored_value = static_cast<a_Value_type &&>(value);
  } else {
    storing_value = TRUE;
    ::new (&stored_value) a_Value_type(static_cast<a_Value_type &&>(value));
  }  /* if */
  return *this;
}  /* operator= */


template<typename a_Value_type>
inline Opt<a_Value_type>& Opt<a_Value_type>::operator=(
                                                const Opt<a_Value_type> &other)
/*
Store the given value as the new stored value via a copy.  The updated optional
is returned.
*/
{
  /* If the copied optional has a value, invoke this optional's copy assignment
     operator with the value to be stored, to copy the value; otherwise, invoke
     clear to remove any current value. */
  if (other.storing_value) {
    *this = other.stored_value;
  } else {
    this->clear();
  }  /* if */
#if CHECKING
  /* Reset the "checked" status as the value has been updated by an operation
     where the previous answer to has_value may have changed. */
  value_presence_checked = FALSE;
#endif /* CHECKING */
  return *this;
}  /* operator= */


template<typename a_Value_type>
inline Opt<a_Value_type>& Opt<a_Value_type>::operator=(
                                                     Opt<a_Value_type> &&other)
/*
Store the given value as the new stored value via a copy.  The updated optional
is returned.
*/
{
  /* If the copied optional has a value, invoke this optional's move assignment
     operator with the value to be stored, to move the value; otherwise, invoke
     clear to remove any current value. */
  if (other.storing_value) {
    *this = static_cast<a_Value_type &&>(other.stored_value);
  } else {
    this->clear();
  }  /* if */
#if CHECKING
  /* Reset the "checked" status as the value has been updated by an operation
     where the previous answer to has_value may have changed. */
  value_presence_checked = FALSE;
#endif /* CHECKING */
  return *this;
}  /* operator= */


template<typename a_Value_type>
inline void Opt<a_Value_type>::clear()
/*
Reset the optional to an empty state.
*/
{
  /* A value was stored, make sure its destructor is invoked. */
  if (storing_value) {
    stored_value.~a_Value_type();
  }  /* if */
  storing_value = FALSE;
#if CHECKING
  /* Reset the "checked" status as the value has been updated by an operation
     where the previous answer to has_value may have changed. */
  value_presence_checked = FALSE;
#endif /* CHECKING */
}  /* clear */


/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef EDG_HEADER_UTIL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2022 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
