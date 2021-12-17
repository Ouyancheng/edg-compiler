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
  a_Var_type    saved_value;
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
