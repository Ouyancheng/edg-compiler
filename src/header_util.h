/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2021 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

header_util.h -- General utility components (mostly templates) intended for use
in both headers and compilation units.  Utilities declared here are guaranteed
to be free from dependence on undefined entities in the frontend.

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
  a_boolean
                storing_value;
                        /* TRUE if there is a value stored, FALSE otherwise. */
  union {
    a_Value_type
                stored_value;
                        /* The value stored.  Represented as a union so the
                           value can be uninitialized, and construction
                           destruction is manually managed. */
  };
#if CHECKING
  a_boolean
                value_presence_checked = FALSE;
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


template<typename a_Value_type, typename a_Container_type>
struct Random_access_container_iter {
  using Self = Random_access_container_iter<a_Value_type, a_Container_type>;

  a_Container_type *ptr;
  unsigned         idx;

  Random_access_container_iter(a_Container_type *container_ptr,
                               unsigned         initial_idx)
    : ptr(container_ptr), idx(initial_idx)
    { }

  Random_access_container_iter() : ptr(NULL), idx(0)
    { }

  a_Value_type &operator *()
    { return (*ptr)[idx]; }

  const a_Value_type &operator *() const
    { return (*ptr)[idx]; }

  Self operator++()
  {
    increment_with_bounds_check();
    return *this;
  }

  Self operator++(int)
  {
    Self result = Self(ptr, idx);
    increment_with_bounds_check();
    return result;
  }

  Self operator--()
  {
    decrement_with_bounds_check();
    return *this;
  }

  Self operator--(int)
  {
    Self result = Self(ptr, idx);
    decrement_with_bounds_check();
    return result;
  }

  a_boolean operator==(Self other) const
    { return ptr == other.ptr && idx == other.idx; }

  a_boolean operator!=(Self other) const
    { return !(*this == other); }

private:
  inline void increment_with_bounds_check() {
    if (idx == (*ptr).size() - 1) {
      *this = Self();
    } else {
      ++idx;
    }  /* if */
  }

  inline void decrement_with_bounds_check() {
    if (idx == 1) {
      *this = Self();
    } else {
      --idx;
    }  /* if */
  }
};  /* Random_access_container_iter */


template<typename a_Value_type, size_t a_Default_alloc>
struct Bi_vec {
  using Self = Bi_vec<a_Value_type, a_Default_alloc>;
  using Iter = Random_access_container_iter<a_Value_type, Self>;

  inline ~Bi_vec() {
    destruct_all();
    if (dyn_storage_mem != NULL) {
      ::operator delete(dyn_storage_mem);
    }  /* if */
  }  /* ~Bi_vec */

  inline a_Value_type &operator [](unsigned idx) {
    a_Value_type *result;

    if (idx < a_Default_alloc) {
      result = &local_storage[idx];
    } else {
      a_Value_type *dyn_storage = static_cast<a_Value_type *>(dyn_storage_mem);
      result = &dyn_storage[idx - a_Default_alloc];
    }  /* if */
    return *result;
  }  /* operator [] */

  inline void push_back(const a_Value_type &val) {
    if (size() < a_Default_alloc) {
      ::new (&local_storage[element_count]) a_Value_type(val);
    } else if (dyn_capacity == 0) {
      dyn_storage_mem = ::operator new(a_Default_alloc * sizeof(a_Value_type));
      add_dyn_element(val);
    } else if (dyn_size() < dyn_capacity) {
      add_dyn_element(val);
    } else {
      double_dyn_capacity();
      add_dyn_element(val);
    }  /* if */
    ++element_count;
  }  /* push_back */

  inline void pop_back() {
    --element_count;
    if (size() < a_Default_alloc) {
      local_storage[size].~a_Value_type();
    } else {
      a_Value_type *dyn_storage = static_cast<a_Value_type *>(dyn_storage_mem);

      dyn_storage[size].~a_Value_type();
    }  /* if */
  }  /* pop_back */

  inline void clear() {
    destruct_all();
    element_count = 0;
  }  /* pop_back */

  a_boolean is_empty() {
    return size() == 0;
  }  /* is_empty */

  unsigned size() {
    return element_count;
  }  /* size */

  Iter begin() {
    Iter it;
    if (!is_empty()) {
      it = Iter(this, 0);
    }  /* if */
    return it;
  }  /* begin */

  Iter end() {
    return Iter();
  }  /* end */
private:
  unsigned dyn_size() {
    return element_count - a_Default_alloc;
  }  /* dyn_size */

  inline void double_dyn_capacity() {
    void         *old_storage_mem = dyn_storage_mem;
    a_Value_type *old_storage = static_cast<a_Value_type *>(old_storage_mem);

    dyn_capacity *= 2;
    dyn_storage_mem = ::operator new(dyn_capacity * sizeof(a_Value_type));

    a_Value_type *dyn_storage = static_cast<a_Value_type *>(dyn_storage_mem);
    for (unsigned i = 0; i < dyn_size(); ++i) {
      ::new (&dyn_storage[i]) a_Value_type(old_storage[i]);
    }  /* for */
    ::operator delete(old_storage_mem);
  }  /* double_dyn_capacity */

  inline void add_dyn_element(const a_Value_type &val) {
    a_Value_type *dyn_storage = static_cast<a_Value_type *>(dyn_storage_mem);

    check_assertion(dyn_size() < dyn_capacity);
    ::new (&dyn_storage[dyn_size()]) a_Value_type(val);
  }  /* add_dyn_element */

  inline void destruct_all() {
    for (unsigned i = 0; i < size(); ++i) {
      (*this)[i].~a_Value_type();
    }  /* for */
  }  /* destroy_all */

  union {
    a_Value_type local_storage[a_Default_alloc];
  };
  unsigned     element_count = 0;
  void         *dyn_storage_mem = NULL;
  unsigned     dyn_capacity = 0;
};  /* Bi_vec */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef EDG_HEADER_UTIL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2021 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
