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

template<typename a_Var_type>
struct Value_saver {
  /* A generic utility for saving and automatically restoring the original
     value of a variable whose value is temporarily being changed. */
  inline Value_saver(a_Var_type *const saved_var_val);
  inline Value_saver(a_Var_type *const saved_var_val,
                     const a_Var_type  &new_value);
  inline ~Value_saver();
private:
  a_Var_type *const
                saved_var;
                        /* A pointer to the variable being manipulated. */
  a_Var_type    saved_value;
                        /* Original value of the saved variable, to be restored
                           in the destructor. */
};  /* Value_saver */


template<typename a_Var_type>
inline Value_saver<a_Var_type>::Value_saver(a_Var_type *const saved_var_val)
  : saved_var(saved_var_val), saved_value(*saved_var)
/*
Save the value of saved_var, but otherwise leave saved_var unchanged.
*/
{
}  /* Value_saver::Value_saver */


template<typename a_Var_type>
inline Value_saver<a_Var_type>::Value_saver(a_Var_type *const saved_var_val,
                                            const a_Var_type  &new_value)
  : saved_var(saved_var_val), saved_value(*saved_var)
/*
Save the value of saved_var and update it to new_value.
*/
{
  *saved_var = new_value;
}  /* Value_saver::Value_saver */


template<typename a_Var_type>
inline Value_saver<a_Var_type>::~Value_saver()
/*
Restore the original value of saved_var.
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
