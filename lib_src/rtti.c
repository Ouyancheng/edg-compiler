/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++ Runtime                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1995 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

Run-time type identification.

*/

#include "basics.h"
#include "config.h"
#include "runtime.h"
#include "rtti.h"

#if ABI_CHANGES_FOR_RTTI
#include "typeinfo.h"
#include "vtbl.h"
#endif /* ABI_CHANGES_FOR_RTTI */

a_boolean derived_to_base_conversion(void**		   p_ptr,
				     void**                p_new_ptr,
				     a_type_info_impl_ptr  class_info,
				     a_type_info_impl_ptr  base_info,
				     an_access_flag_string *access_flags,
				     a_boolean             use_access_flags)
/*
Converts p_ptr from a pointer to a derived class (described by class_info)
to a pointer to a base class (described by base_info) and stores
the resulting pointer in p_new_ptr.  Returns TRUE if the base class was
found and the conversion was done; otherwise returns FALSE. 

p_ptr may be NULL when this routine is called simply to determine whether
the conversion is possible.  This is the case when exception specifications
are being tested.

The access_flags string contains one byte for each base class.  The
byte contains either "Y" (the base class is accessible) or "N" (the
base class is not accessible).  The base class may be inaccessible
either because of access protection or because the base class
is ambiguous.

use_access_flags indicates whether the access_flags string should be used
when checking for accessible (and unambiguous) bases.  If use_access_flags
is FALSE, the BCS_PUBLIC and BCS_AMBIGUOUS flags in the base class entry
are used.  These flags were added when RTTI was implemented in version 2.29.
The access_flags string was retained for backward compatibility.
*/
{
  a_boolean		result = FALSE;
  a_base_class_spec_ptr	bcsp = class_info->base_class_entries;
  void                  *ptr;

  /* Get the actual derived class pointer.  If no pointer was provided,
     use NULL. */
  ptr = p_ptr == NULL ? NULL : *p_ptr;
  *p_new_ptr = NULL;
  if (bcsp != NULL) {
    /* A base class list is present. */
    a_boolean	done = FALSE;
    /* Loop through the direct base classes and look for one that matches
       the specified base class.  We look through all of the direct bases
       first because the direct base list also includes any virtual bases.
       We want to make sure that we find the virtual base classes at
       the top level when possible. */
    do {
      void*		   new_ptr = NULL;
      a_type_info_impl_ptr test_info = bcsp->type_info;
      a_boolean            is_accessible;
      if (ptr != NULL) {
        /* Adjust the pointer by the offset provided in the base class
           specification. */
        new_ptr = (void*) (((char *) ptr) + bcsp->offset);
      }  /* if */
      if (use_access_flags) {
        /* See if this base class is accessible.  *access_flags either points
           to a string of characters associated with each base class in the
  	   tree, or is NULL if none of the base classes are accessible. */
        if (*access_flags != NULL) {
          is_accessible = **access_flags == BASE_ACCESSIBLE;
          (*access_flags)++;
        } else {
          is_accessible = FALSE;
        }  /* if */
      } else {
        /* The base is accessible if it is public and not ambiguous. */
        is_accessible = ((bcsp->flags & BCS_PUBLIC) != 0) &&
                        ((bcsp->flags & BCS_AMBIGUOUS) == 0);
      }  /* if */
      if (is_accessible &&
	  matching_type_info(test_info, base_info)) {
        /* We have found a match. */
        result = TRUE;
        if (ptr != NULL) {
          if (bcsp->flags & BCS_VIRTUAL) {
            /* If this is a virtual base class then the offset provides the
               location of a pointer to the base class.  Dereference the
               pointer and return that value. */
            *p_new_ptr = *((void **)new_ptr);
          } else {
	    /* A nonvirtual base class.  new_ptr has already been adjusted to
	       point to the start of the base class.  Return this value
	       to the caller. */
	    *p_new_ptr = new_ptr;
	  }  /* if */
	}  /* if */
      }  /* if */
      /* The last entry in the array will have the BCS_LAST flag set. */
      done = bcsp->flags & BCS_LAST;
      /* Advance the pointer to the next element in the array of base
         class specifications. */
      bcsp++;
    } while (!done);
    if (!result) {
      /* The specified base class is not one of the direct or virtual bases.
         Search the indirect base classes. */
      bcsp = class_info->base_class_entries;
      do {
        void*		     new_ptr = NULL;
        a_type_info_impl_ptr test_info = bcsp->type_info;
	if (ptr != NULL) {
	  /* Adjust the pointer by the offset provided in the base class
	     specification. */
	  new_ptr = (void*) (((char *) ptr) + bcsp->offset);
	}  /* if */
        /* This is not the base class we are looking for.  Look at the
           base classes of this base class. */
        if (test_info->base_class_entries != NULL) {
          /* This base class has its own bases.  Call this routine
             recursively. */
	  void* local_new_ptr;
          if (derived_to_base_conversion(&new_ptr, &local_new_ptr, test_info,
					 base_info, access_flags,
                                         use_access_flags)) {
	    if (ptr != NULL) {
	      /* We have found a match.  Update the pointer passed to us
		 to reflect the value found by the recursive call. */
	      *p_new_ptr = local_new_ptr;
	    }  /* if */
            result = TRUE;
            break;
          }  /* if */
        }  /* if */
        /* The last entry in the array will have the BCS_LAST flag set. */
        done = bcsp->flags & BCS_LAST;
        /* Advance the pointer to the next element in the array of base
           class specifications. */
        bcsp++;
      } while (!done);
    }  /* if */
  }  /* if */
  return result;
}  /* derived_to_base_conversion */

#if ABI_CHANGES_FOR_RTTI

EXTERN_C void *__dynamic_cast(void                  *class_ptr,
			      a_vtbl_entry_ptr      vtbl_ptr,
			      a_type_info_impl_ptr  tiip)
/*
Runtime support for dynamic_cast operations.  This routine handles

  - casts of a polymorphic objects type to void*, which is defined as
    returning a pointer to the complete object type

  - polymorphic base to derived casts

  - polymorphic cross casts

The information about the dynamic type of the source object is obtained
from entry zero of the virtual function table.

Entry zero of the virtual function table is organized differently than the
other table entries.  The information in that entry is provided to support
the dynamic_cast and typeinfo operations.  Entry zero contains the
following information:

	delta:		The offset from the complete object pointer to
			class_ptr (i.e., the value to be subtracted from
			class_ptr to get the complete object pointer.

	index:		Unused.

	function:	Pointer to the typeinfo_impl structure that
		        for the dynamic type of class_ptr.
*/
{
  void			*complete_object_ptr;
  size_t		offset_to_complete_object;
  a_type_info_impl_ptr	object_tiip;
  void			*result = NULL;

  /* Get a pointer to the complete object. */
  offset_to_complete_object = vtbl_ptr->delta;
  complete_object_ptr =
                     (void*)(((char *)class_ptr) - offset_to_complete_object);
  /* Get the pointer to the type_info associated with the source object. 
     This is stored in the function pointer field of the vtbl entry. */
  object_tiip = (a_type_info_impl_ptr)vtbl_ptr->function;
  if (tiip == NULL) {
    /* When tiip is NULL, the pointer is being cast to void*.  This
       means that class_ptr is to be converted to a pointer to the
       complete object type. */
    result = complete_object_ptr;
  } else if (matching_type_info(object_tiip, tiip)) {
    /* The object is being cast to the type it actually is.  For example,
       a Base* that actually points to a Derived is being cast to a
       Derived*.  Simply return the complete object pointer. */
    result = complete_object_ptr;
  } else {
    a_boolean	conversion_done;
    void	*new_ptr = NULL;
    conversion_done = derived_to_base_conversion(&complete_object_ptr,
						 &new_ptr,
						 object_tiip,
						 tiip,
						 (an_access_flag_string*)NULL,
						 /*use_access_flags=*/FALSE);
    if (conversion_done) result = new_ptr;
  }  /* if */
  return result;
}  /* __dynamic_cast */


void __throw_bad_cast(void)
/*
Throw a bad cast exception.  If exception handling is not supported in
this version of the runtime, then simply abort.
*/
{
#if EXCEPTION_HANDLING
  throw bad_cast();
#else /* !EXCEPTION_HANDLING */
  abort();
#endif /* EXCEPTION_HANDLING */
}  /* __throw_bad_cast */


void __throw_bad_typeid(void)
/*
Throw a bad typeid exception.  If exception handling is not supported in
this version of the runtime, then simply abort.
*/
{
#if EXCEPTION_HANDLING
  throw bad_typeid();
#else /* !EXCEPTION_HANDLING */
  abort();
#endif /* EXCEPTION_HANDLING */
}  /* __throw_bad_typeid */


EXTERN_C void *__dynamic_cast_ref(void                  *class_ptr,
			          a_vtbl_entry_ptr      vtbl_ptr,
			          a_type_info_impl_ptr  tiip)
/*
Interface to __dynamic_cast used when casting references.  This calls
__dynamic_cast and throws an exception if the cast failed.
*/
{
  void*		result;

  result = __dynamic_cast(class_ptr, vtbl_ptr, tiip);
  if (result == NULL) {
    __throw_bad_cast();
  }  /* if */
  return result;
}  /* __dynamic_cast_ref */


EXTERN_C void *__get_typeid(a_vtbl_entry_ptr	vtbl_ptr)
/*
Return the user type_info pointer from the specified virtual function
table.  If the pointer to the vtable is NULL, throw a bad_typeid
execption.
*/
{
  a_type_info_impl_ptr	tiip;

  if (vtbl_ptr == NULL) __throw_bad_typeid();
  /* Get the pointer to the type_info_impl associated with the source object. 
     This is stored in the function pointer field of the vtbl entry. */
  tiip = (a_type_info_impl_ptr)vtbl_ptr->function;
  /* Return the address of the user type_info. */
  return (void*)&tiip->user_type_info;
}  /* __get_typeid */


#endif /* ABI_CHANGES_FOR_RTTI */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++ Runtime                            - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1995 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
