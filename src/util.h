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

util.h -- General utility components (mostly templates).

*/

#ifndef EDG_UTIL_H
#define EDG_UTIL_H 1

#include <new>

#include "mem_manage.h"

#ifndef EDG_HEADER_UTIL_H
#include "header_util.h"
#endif /* ifndef EDG_HEADER_UTIL_H */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

typedef decltype(nullptr) a_nullptr;


/*
Enable_if<cond, T> is invalid (causing deduction failure) if cond is FALSE.
Otherwise, it produces T.
*/
template<a_boolean cond, typename a_Thing>
struct Enable_if_helper;

/*lint -esym(758,Enable_if_helper)*/
template<typename a_Thing>
struct Enable_if_helper<true, a_Thing> {
  typedef a_Thing a_thing;
};  /* Enable_if_helper<true, a_Thing> */

template<a_boolean cond, typename a_Thing>
using Enable_if = typename Enable_if_helper<cond, a_Thing>::a_thing;


/*
Is_same<A, B, C> is invalid (causing deduction failure) if A is not the same
type as B.  Otherwise, it produces C.

By default, C is the same type as A.
*/
template<typename a_Type_A, typename a_Type_B, typename a_Ret_type>
struct Is_same_helper;

template<typename a_Type_A, typename a_Ret_type>
struct Is_same_helper<a_Type_A, a_Type_A, a_Ret_type> {
  typedef a_Ret_type ret_ty;
};  /* Is_same_helper<a_type_A, a_type_A> */

template<typename a_Type_A, typename a_Type_B, typename a_Ret_type = a_Type_A>
using Is_same = typename Is_same_helper<a_Type_A, a_Type_B,a_Ret_type>::ret_ty;


/*
Overload_priority is a helper type for controlling overload priority by using
nested parent types to provide a gradient of conversions (priorities).  This
results in Overload_priority<N> having a higher priority than
Overload_priority<N - 1>.

Callers should use the highest Overload_priority<N> object to allow resolution
of all represented priorities.

Overload_priority can be used to guide overload resolution when multiple
candidates have otherwise equivalent overload resolution priority.  This is
useful particularly when SFINAE is being used to enable or disable one or more
candidates during overload resolution.

As an example, consider one or more function templates that have conditionally
conflicting overloading resolutions:

  // I want this to be called as fallback logic.
  // All things equal, this is the least important candidate.
  template<typename a_Type>
  auto foo(a_Type value) -> Foo;

  // I want this to be called when value has a member "m1" of type Bar.
  // All things equal, this is a more important candidate.
  template<typename a_Type>
  auto foo(a_Type value) -> Is_same<value.m1, Bar>;

  // I want this to be called when value has a member "m2" of type Bar.
  // All things equal, this is the most important candidate.
  template<typename a_Type>
  auto foo(a_Type value) -> Is_same<value.m2, Bar>;

These functions as written have an undecidable overload resolution when
instantiated with "a_Type" that has a member "m1" and/or "m2" of type "Bar".
Overload_priority can be used to resolve this and apply the intended candidate
resolution.

An example rewrite of the above functions using Overload_priority:

  // I want this to be called as fallback logic.
  // All things equal, this is the least important candidate.
  template<typename a_Type>
  auto foo(a_Type value, Overload_priority<0>) -> Foo;

  // I want this to be called when value has a member "m1" of type Bar.
  // All things equal, this is a more important candidate.
  template<typename a_Type>
  auto foo(a_Type value, Overload_priority<1>) -> Is_same<value.m1, Bar>;

  // I want this to be called when value has a member "m2" of type Bar.
  // All things equal, this is the most important candidate.
  template<typename a_Type>
  auto foo(a_Type value, Overload_priority<2>) -> Is_same<value.m2, Bar>;

An API that relies on Overload_priority will typically be wrapped behind an
interface that makes that aspect invisible. For example:

  template<typename a_Type>
  auto foo(a_Type value) -> auto
    { return foo(value, Overload_priority<2>())); }

Thus, creating the following API:

   struct Baz1 {
   };
   foo(Baz1()); // Calls the first function template.

   struct Baz2 {
     Bar m1;
   };
   foo(Baz2(...)); // Calls the second function template.

   struct Baz3 {
     Bar m2;
   };
   foo(Baz3(...)); // Calls the third function template.

   struct Baz4 {
     Bar m1;
     Bar m2;
   };
   foo(Baz4(...)); // Calls the third function template.

*/
template<int a_Depth>
struct Overload_priority : Overload_priority<a_Depth - 1> {
};  /* Overload_priority */

template<>
struct Overload_priority<0> {
};  /* Overload_priority<0> */


/*
Remove_ref<T> produces T if T is not a reference type, or the type underlying
the reference type otherwise.
*/
template<typename an_Object>
struct Remove_ref_helper {
  typedef an_Object an_object;
};  /* Remove_ref_helper */

/*lint -esym(758,Remove_ref_helper)*/
template<typename an_Object>
struct Remove_ref_helper<an_Object&> {
  typedef an_Object an_object;
};  /* Remove_ref_helper<an_Object&> */

/*lint -esym(758,Remove_ref_helper)*/
template<typename an_Object>
struct Remove_ref_helper<an_Object&&> {
  typedef an_Object an_object;
};  /* Remove_ref_helper<an_Object&&> */

template<typename an_Object>
using Remove_ref = typename Remove_ref_helper<an_Object>::an_object;


/*
dummy_val<T> is not meant to be evaluated.  It is a convenience function to
produce a value of type T in unevaluated operands, or, if T is a reference
type, a glvalue of the type underlying the reference.
*/
template<typename an_Object>
an_Object dummy_val();


/*
Value_for_ptr<T> for a pointer-like type T produces the type pointed to.
*/
template<typename a_Ptr>
using Value_for_ptr = Remove_ref<decltype(*dummy_val<a_Ptr>())>;

#ifdef __EDG__
/* Don't warn on noexcept if exceptions are disabled. */
#pragma diag_suppress 540
#endif /* ifdef __EDG__ */
template<typename an_Object>
an_Object&& fwd(Remove_ref<an_Object>&  arg) noexcept
/*
This function should only be applied to "forwarding references".  It is used
to forward parameters.  For example:

    template<a_Thing> void f(a_Thing &&p) {
      g(fwd<a_Thing>(p));
    }

If f is called with an rvalue, a_Thing will be deduced to a non-reference type
and fwd<a_Thing>(p) will produce an xvalue.  If f is called with an lvalue,
a_Thing will be deduced to an lvalue reference type, and fwd<a_Thing>(p) will
pass through the lvalue.
*/
{
  return (an_Object&&)arg;
}  /* fwd */


template<typename a_Ptr>
Value_for_ptr<a_Ptr>&& move_from(a_Ptr  p_object)
/*
Return *p_object as an xvalue, so that it can be moved from.
*/
{
  return (Value_for_ptr<a_Ptr>&&)*p_object;
}  /* move_from */


template<typename a_Ptr, typename ...an_Arg_pack>
void construct(a_Ptr          p_object,
               an_Arg_pack&&  ...args)
/*
Construct *p_object with the given arguments.
*/
{
  typedef Value_for_ptr<a_Ptr> an_object;
  /*lint -e1556*/
  ::new((void*)p_object) an_object(fwd<an_Arg_pack>(args)...);
}  /* construct */


template<typename a_Ptr, typename a_Functor>
void functional_init(a_Ptr        p_object,
                     a_Functor&&  fn)
/*
Initialize *p_object with the return value of fn().  Usually fn() should return
a prvalue and the initialization can occur without copy/move (through copy
elision).
*/
{
  typedef Value_for_ptr<a_Ptr> an_object;
  ::new((void*)p_object) an_object(fn());
}  /* functional_init */


template<typename a_Ptr>
void destroy(a_Ptr  p_object)
/*
Destroy the given object.
*/
{
  typedef Value_for_ptr<a_Ptr> an_object;
  p_object->~an_object();
}  /* destroy */


template<typename a_Ptr>
void swap_at(a_Ptr  p1,
             a_Ptr  p2)
/*
Swap the values pointed to by p1 and p2.
*/
{
  Value_for_ptr<a_Ptr>  tmp = move_from(p1);
  *p1 = move_from(p2);
  *p2 = move_from(&tmp);
}  /* swap_at */


template<typename an_Object>
void reverse_array(an_Object  *arr,
                   a_ptrdiff  length)
/*
Reverse the length elements in the given array (or sub-array).
*/
{
  if (length > 1) {
    an_Object  *left = arr, *right = arr+(length-1);
    for (; left < right; ++left, --right) {
      swap_at(left, right);
    }  /* for */
  }  /* if */
}  /* reverse_array */


template<typename a_List_elem>
a_List_elem* reverse_simple_list(a_List_elem  *list)
/*
list points to a singly-linked list of elements connected through an accessible
"next" pointer field.  Reverse the list and return a pointer to the new start
of the list.  list can be NULL.
*/
{
  a_List_elem  *new_list = NULL, *next;

  while (list) {
    next = list->next;
    list->next = new_list;
    new_list = list;
    list = next;
  }
  return new_list;
}  /* reverse_simple_list */


template<typename a_List_elem>
a_List_elem** get_last_simple_list_link(a_List_elem  **p_list)
/*
p_list is non-NULL and *p_list points to a possibly empty singly-linked list,
whose elements are connected through accessible "next" pointer fields.  Return
a pointer to the last such "next" field (or p_list itself if there are none).
*/
{
  while (*p_list != NULL) {
    p_list = &(*p_list)->next;
  }  /* while */
  return p_list;
}  /* get_last_simple_list_link */


/*lint -e{1537}*/
template<typename a_Ptr>
struct Ptr_with_flag {
  /* An iterator and boolean value combined.  This is meant to be returnable
     through registers and therefore does not include constructors or
     destructors.  Instead, it's just a pair of data members.  The function
     ptr_with_flag should be used to construct an object of this type (with
     type deduction as a bonus).  The case where a_Ptr is a native pointer
     could potentially be optimized by encoding the flag in the pointer
     value. */
  typedef a_Ptr a_ptr;
  typedef Value_for_ptr<a_ptr> a_value;
  auto operator->() const -> a_ptr
    { return this->ptr_value; }
  auto  operator*() const -> a_value&
    { return *this->ptr_value; }
  auto ptr() const -> a_ptr
    { return this->ptr_value; }
  auto flagged() const -> a_boolean
    { return this->flag_value; }

  a_ptr		ptr_value;
			/* Embedded pointer. */
  a_boolean	flag_value;
			/* Embedded flag. */
};  /* Ptr_with_flag */

template<typename a_Ptr>
inline Ptr_with_flag<a_Ptr> ptr_with_flag(a_Ptr      ptr,
                                          a_boolean  flag)
/*
Return a Ptr_with_flag initialized with the given values.
*/
{
  return Ptr_with_flag<a_Ptr>{ ptr, flag };
}  /* ptr_with_flag */


template<typename an_Elem>
struct Allocation {
  /* A representation for the result of an allocation.  This is meant to be
     returnable through registers and therefore does not include constructors
     or destructors.  Instead, it's just a pair of immutable data members. */
  typedef an_Elem an_elem;
  an_elem* const
		start;
			/* Pointer to the first allocated element.  This should
			   never be NULL: An allocator should abort the front
			   end's execution if no storage can be allocated. */
  const a_ptrdiff
		n_allocated;
			/* Number of allocated elements. */
};  /* Allocation */


template<typename an_Elem>
struct FE_allocator {
  /* A general allocator for front end memory. */
  typedef an_Elem an_elem;
  typedef a_ptrdiff a_size;
  typedef Allocation<an_elem> an_allocation;
  typedef FE_allocator<an_elem> an_allocator;
  typedef FE_allocator<an_elem> a_deallocator;
  inline static auto alloc(a_size n) -> an_allocation;
  inline static auto realloc(an_allocation  a,
                             a_size         new_capacity,
                             a_size         n_to_move)
                     -> an_allocation;
  inline static void dealloc(an_allocation allocation);
};  /* FE_allocator */


template<typename an_Elem>
inline auto FE_allocator<an_Elem>::alloc(a_size n) -> an_allocation
/*
Allocate at least n elements of type an_Elem and return the resulting
allocation (which reflects the actual number of allocated elements).
*/
{
  return an_allocation{ (an_elem*)alloc_fe(n*sizeof(an_elem)),
                        (a_ptrdiff)n };
}  /* FE_allocator::alloc */


template<typename an_Elem>
inline auto FE_allocator<an_Elem>::realloc(an_allocation a,
                                           a_size        new_capacity,
                                           a_size        n_to_move)
            -> an_allocation
/*
Replace the given allocation -- which was allocated by the same allocator -- by
a new one with at least new_capacity elements.  The first n_to_move elements in
the original allocation are initialized and should therefore be moved to the
new allocation.
*/
{
  an_elem  *old_start = a.start,
           *new_start = (an_elem*)alloc_fe(new_capacity*sizeof(an_elem));
  for (a_size k = 0; k < n_to_move; ++k) {
    construct(new_start+k, move_from(old_start+k));
    destroy(old_start+k);
  }  /* for */
  free_fe((void*)old_start, a.n_allocated*sizeof(an_elem));
  return an_allocation{ new_start, (a_ptrdiff)new_capacity };
}  /* FE_allocator::realloc */


template<typename an_Elem>
inline void FE_allocator<an_Elem>::dealloc(an_allocation a)
/*
Release the given allocation -- which was allocated by the same allocator.
The caller is responsible for ensuring the allocation contains no live
objects.
*/
{
  free_fe((void*)a.start, a.n_allocated*sizeof(an_elem));
}  /* FE_allocator::dealloc */


template<typename an_Object, typename ...an_Arg_pack>
inline an_Object *new_fe(an_Arg_pack ...args)
/*
Allocate in front-end memory and construct an object of type an_Object with
the constructor arguments specified by args.  Return a pointer to the object.
*/
{
  an_Object  *p = FE_allocator<an_Object>::alloc(1).start;
  construct(p, fwd<an_Arg_pack>(args)...);
  return p;
}  /* new_fe */


template<typename an_Object>
inline void delete_fe(an_Object *p)
/*
Destroy and delete an object of type an_Object that was allocated in front end
memory.
*/
{
  destroy(p);
  FE_allocator<an_Object>::dealloc(Allocation<an_Object>{p, 1});
}  /* delete_fe */


template<typename an_Elem>
struct General_allocator {
  /* A general allocator for general memory (i.e., as allocated by
     alloc_general in mem_manage.c). */
  typedef an_Elem an_elem;
  typedef a_ptrdiff a_size;
  typedef Allocation<an_elem> an_allocation;
  typedef General_allocator<an_elem> an_allocator;
  typedef General_allocator<an_elem> a_deallocator;
  inline static auto alloc(a_size n) -> an_allocation;
  inline static auto realloc(an_allocation  a,
                             a_size         new_capacity,
                             a_size         n_to_move)
                     -> an_allocation;
  inline static void dealloc(an_allocation allocation);
};  /* General_allocator */


template<typename an_Elem>
inline auto General_allocator<an_Elem>::alloc(a_size n) -> an_allocation
/*
Allocate at least n elements of type an_Elem and return the resulting
allocation (which reflects the actual number of allocated elements).
*/
{
  return an_allocation{ (an_elem*)alloc_general(n*sizeof(an_elem)),
                        (a_ptrdiff)n };
}  /* General_allocator::alloc */


template<typename an_Elem>
inline auto General_allocator<an_Elem>::realloc(an_allocation a,
                                                a_size        new_capacity,
                                                a_size        n_to_move)
            -> an_allocation
/*
Replace the given allocation -- which was allocated by the same allocator -- by
a new one with at least new_capacity elements.  The first n_to_move elements in
the original allocation are initialized and should therefore be moved to the
new allocation.
*/
{
  an_elem  *old_start = a.start,
           *new_start = (an_elem*)alloc_general(new_capacity*sizeof(an_elem));
  for (a_size k = 0; k < n_to_move; ++k) {
    construct(new_start+k, move_from(old_start+k));
    destroy(old_start+k);
  }  /* for */
  free_general(old_start, a.n_allocated*sizeof(an_elem));
  return an_allocation{ new_start, (a_ptrdiff)new_capacity };
}  /* General_allocator::realloc */


template<typename an_Elem>
inline void General_allocator<an_Elem>::dealloc(an_allocation a)
/*
Release the given allocation -- which was allocated by the same allocator.
The caller is responsible for ensuring the allocation contains no live
objects.
*/
{
  free_general(a.start, a.n_allocated*sizeof(an_elem));
}  /* General_allocator::dealloc */


template<typename an_Object, typename ...an_Arg_pack>
inline an_Object *new_general(an_Arg_pack ...args)
/*
Allocate in general memory and construct an object of type an_Object with
the constructor arguments specified by args.  Return a pointer to the object.
*/
{
  an_Object  *p = General_allocator<an_Object>::alloc(1).start;
  construct(p, fwd<an_Arg_pack>(args)...);
  return p;
}  /* new_general */


template<typename an_Object>
inline void delete_general(an_Object *p)
/*
Destroy and delete an object of type an_Object that was allocated in
general memory.
*/
{
  destroy(p);
  General_allocator<an_Object>::dealloc(Allocation<an_Object>{p, 1});
}  /* delete_general */


/*
The Dyn_array template
======================
The Dyn_array<E, A> template defined below implements a dynamic array construct
not unlike std::vector<E, A>.  E is the element type and A is the allocator
type (which defaults to the front end memory allocator).

The most common std::vector operators are also applicable to Dyn_array.  Things
like operator[], push_back, begin(), end(), etc., work as expected (which,
e.g., means that the C++11 range-based for-statement works for Dyn_array also).
*/

/*lint -esym(1510,*Dyn_array)*/
template<typename an_Elem, template<typename> class Allocator = FE_allocator>
struct Dyn_array: private Allocator<an_Elem> {
  /* A dynamically growable array-like class type. */
  typedef an_Elem an_elem;
  typedef Allocator<an_Elem> an_allocator;
  typedef typename an_allocator::a_size a_size;
  typedef a_ptrdiff an_index;
  inline Dyn_array(a_size       cap = 0,
                   an_allocator a = an_allocator());
  inline Dyn_array(a_size         cap,
                   const an_elem& v,
                   an_allocator   a = an_allocator());
  inline Dyn_array(const Dyn_array&);
  inline Dyn_array(Dyn_array&&);
  inline ~Dyn_array();
  inline auto operator=(const Dyn_array&) -> Dyn_array&;
  inline auto operator=(Dyn_array&&) -> Dyn_array&;
  inline auto operator[](an_index i) -> an_elem&
    { return this->elems[i]; }
  inline auto operator[](an_index i) const -> const an_elem& 
    { return this->elems[i]; }
  inline auto is_empty() const -> a_boolean 
    { return this->n_elems == 0; }
  inline auto length() const -> a_size 
    { return this->n_elems; }
  inline auto capacity() const -> a_size
    { return this->n_allocated; }
  inline auto back_elem() -> an_elem&
    { return this->elems[this->n_elems-1]; }
  inline auto back_elem() const -> const an_elem&
    { return this->elems[this->n_elems-1]; }
  inline void push_back(const an_elem  &value);
  inline void push_back(an_elem  &&value);
  inline void pop_back()
    { destroy(&this->elems[--this->n_elems]); }
  inline void insert(an_index i, const an_elem  &value);
  inline void insert(an_index i, an_elem  &&value);
  template<typename an_Input_iterator>
  inline void insert(an_index          i,
                     an_Input_iterator begin,
                     size_t            len);
  inline void remove(an_index i);
  inline void clear();
  void resize(a_size new_n, const an_elem  &value);
  void resize(a_size new_n, an_elem  &&value);
  void reserve(a_size);
  void shrink_wrap();
  /* Interfaces to allow range-based for loop. */
  /*lint -e{1535}*/
  inline auto begin() -> an_elem*
    { return this->elems; }
  inline auto begin() const -> const an_elem*
    { return this->elems; }
  inline auto end() -> an_elem*
    { return this->elems+this->n_elems; }
  inline auto end() const -> const an_elem*
    { return this->elems+this->n_elems; }
private:
  typedef typename an_allocator::an_allocation an_allocation;
  an_elem	*elems;
			/* Pointer to the allocated elements. */
  a_size	n_allocated;
			/* Number of elements allocated.  This is also known
			   as the "capacity". */
  a_size	n_elems;
			/* Number of initialized elements.  This is also known
			   as the "length". */
  void grow();
};  /* Dyn_array */


template<typename an_Elem, template<typename> class Allocator>
inline Dyn_array<an_Elem, Allocator>::Dyn_array(a_size       cap,
                                                an_allocator a)
/*
Initialize a Dyn_array with a minimum of cap elements whose allocation is
managed by the given allocator.
*/
  : an_allocator(a)
  , elems()
  , n_allocated()
  , n_elems(0)
{
  an_allocation  allocation = this->alloc(cap);
  this->elems = allocation.start;
  this->n_allocated = (a_size)allocation.n_allocated;
}  /* Dyn_array::Dyn_array */


template<typename an_Elem, template<typename> class Allocator>
inline Dyn_array<an_Elem, Allocator>::Dyn_array(a_size         cap,
                                                const an_elem& v,
                                                an_allocator   a)
/*
Initialize a Dyn_array with a minimum of cap elements whose allocation is
managed by the given allocator.  Initialize the first cap elements to v.
*/
  : an_allocator(a)
  , elems()
  , n_allocated()
  , n_elems(cap)
{
  an_allocation  allocation = this->alloc(cap);
  this->elems = allocation.start;
  this->n_allocated = (a_size)allocation.n_allocated;
  for (a_size k = 0; k<cap; ++k) {
    construct(this->elems+k, v);
  }  /* for */
}  /* Dyn_array::Dyn_array */


template<typename an_Elem, template<typename> class Allocator>
inline Dyn_array<an_Elem, Allocator>::Dyn_array(const Dyn_array&  src)
/*
Copy constructor.
*/
  : an_allocator(src)
  , elems()
  , n_allocated()
  , n_elems(src.n_elems)
{
  /* Allocate new storage. */
  an_allocation  allocation = this->alloc(src.n_allocated);
  this->elems = allocation.start;
  this->n_allocated = (a_size)allocation.n_allocated;
  /* Copy-construct the elements from the source into the newly-allocated
     storage. */
  an_elem  *src_elems = src.elems;
  a_size   n = this->n_elems;

  for (a_size k = 0; k < n; ++k) {
    construct(elems+k, src_elems[k]);
  }  /* for */
}  /* Dyn_array::Dyn_array */


template<typename an_Elem, template<typename> class Allocator>
inline Dyn_array<an_Elem, Allocator>::Dyn_array(Dyn_array&&  src)
/*
Move constructor.
*/
  : an_allocator(move_from(&src))
  , elems(src.elems)
  , n_allocated(src.n_allocated)
  , n_elems(src.n_elems)
{
  src.elems = NULL;
  src.n_allocated = 0;
  src.n_elems = 0;
}  /* Dyn_array::Dyn_array */


template<typename an_Elem, template<typename> class Allocator>
inline Dyn_array<an_Elem, Allocator>::~Dyn_array()
/*
Destructor.
*/
{
  an_elem  *arr_elems = this->elems;
  a_size   n = this->n_elems;

  for (a_size k = 0; k < n; ++k) {
    destroy(arr_elems+k);
  }  /* for */
  this->dealloc(an_allocation{ arr_elems, this->n_allocated });
  this->elems = NULL;
}  /* Dyn_array::~Dyn_array */


/*lint -e{1529}*/
template<typename an_Elem, template<typename> class Allocator>
inline auto Dyn_array<an_Elem, Allocator>::operator=(const Dyn_array &b)
            -> Dyn_array&
/*
Copy assignment operator.  
*/
{
  a_size  n = this->n_elems;

  if (b.n_elems == n) {
    /* Straightforward element-to-element assignment.  Note that this covers
       self-assignment. */
    an_elem  *dst_elems = this->elems, *src_elems = b.elems;
    for (a_size k = 0; k < n; ++k) {
      dst_elems[k] = src_elems[k];
    }  /* for */
  } else {
    /* A change in size (and possibly capacity) is needed.  Destroy the
       original elements, and then construct the new ones. */
    an_elem  *dst_elems = this->elems, *src_elems = b.elems;
    for (a_size k = 0; k < n; ++k) {
      destroy(dst_elems+k);
    }  /* for */
    a_size   new_n = b.n_elems;
    if (this->n_allocated < new_n) {
      this->n_elems = 0;
      this->reserve(new_n);
      dst_elems = this->elems;
    }  /* if */
    for (a_size k = 0; k < new_n; ++k) {
      construct(dst_elems+k, src_elems[k]);
    }  /* for */
    this->n_elems = new_n;
  }  /* if */
  return *this;
}  /* Dyn_array::operator= */


template<typename an_Elem, template<typename> class Allocator>
inline auto Dyn_array<an_Elem, Allocator>::operator=(Dyn_array &&b)
            -> Dyn_array&
/*
Move assignment operator.  
*/
{
  if (this != &b) {
    destroy(this);
    construct(this, move_from(&b));
  }  /* if */
  return *this;
}  /* Dyn_array::operator= */


template<typename an_Elem, template<typename> class Allocator>
inline void Dyn_array<an_Elem, Allocator>::push_back(const an_elem &value)
/*
Copy the given value into the position after the currently-last element.
Allocate new storage if needed.
*/
{
  a_size  n = this->n_elems;

  if (n == this->n_allocated) {
    this->grow();
  }  /* if */
  construct(this->elems+n, value);
  this->n_elems = n+1;
}  /* Dyn_array::push_back */


template<typename an_Elem, template<typename> class Allocator>
inline void Dyn_array<an_Elem, Allocator>::push_back(an_elem &&value)
/*
Move the given value into the position after the currently-last element.
Allocate new storage if needed.
*/
{
  a_size  n = this->n_elems;

  if (n == this->n_allocated) {
    this->grow();
  }  /* if */
  construct(this->elems+n, move_from(&value));
  this->n_elems = n+1;
}  /* Dyn_array::push_back */


template<typename an_Elem, template<typename> class Allocator>
inline void Dyn_array<an_Elem, Allocator>::insert(an_index      i,
                                                  const an_elem &value)
/*
Copy-insert the given value at the given index.  All subsequent values (if any)
are first moved one position up.
*/
{
  a_size  n = this->n_elems;

  if (n_elems == this->n_allocated) {
    this->grow();
  }  /* if */
  an_elem  *arr_elems = this->elems;
  for (an_index k = n; k>i; --k) {
    construct(arr_elems+k, move_from(arr_elems+k-1));
    destroy(arr_elems+k-1);
  }  /* for */
  construct(arr_elems+i, value);
  this->n_elems = n+1;
}  /* Dyn_array::insert */


template<typename an_Elem, template<typename> class Allocator>
inline void Dyn_array<an_Elem, Allocator>::insert(an_index  i,
                                                  an_elem   &&value)
/*
Move-insert the given value at the given index.  All subsequent values (if any)
are first moved one position up.
*/
{
  a_size  n = this->n_elems;

  if (n == this->n_allocated) {
    this->grow();
  }  /* if */
  an_elem  *arr_elems = this->elems;
  for (an_index k = n; k>i; --k) {
    construct(arr_elems+k, move_from(arr_elems+k-1));
    destroy(arr_elems+k-1);
  }  /* for */
  construct(arr_elems+i, move_from(&value));
  this->n_elems = n+1;
}  /* Dyn_array::insert */


template<typename an_Elem, template<typename> class Allocator>
template<typename an_Input_iterator>
inline void Dyn_array<an_Elem, Allocator>::insert(an_index          i,
                                                  an_Input_iterator start,
                                                  size_t            len)
/*
Copy-insert len number of values copying sequentially from the given iterator
into the array beginning at the given index i.  All existing values from index
i through the end of the array are first moved len positions back.
*/
{
  a_size orig_count = this->n_elems;

  /* Ensure adequate capacity for the bulk insert operation. */
  this->reserve(orig_count + len);

  an_elem  *arr_elems = this->elems;
  /* Move the existing elements past the inserted sequence. */
  for (an_index k = orig_count; k > i; --k) {
    construct(arr_elems + k + len - 1, move_from(arr_elems + k - 1));
    destroy(arr_elems + k - 1);
  }  /* for */

  /* Insert the sequence of elements. */
  an_Input_iterator curr = start;
  for (an_index k = 0; k < (an_index)len; ++k) {
    construct(arr_elems + i + k, *curr);
    ++curr;
  }  /* for */
  this->n_elems += len;
}  /* Dyn_array::insert */


template<typename an_Elem, template<typename> class Allocator>
inline void Dyn_array<an_Elem, Allocator>::remove(an_index  i)
/*
Destroy the entry at the given index.  All subsequent values (if any) are moved
one position down.
*/
{
  an_elem  *arr_elems = this->elems;

  destroy(arr_elems+i);
  a_size  n = --this->n_elems;
  for (an_index k = i; k<n; ++k) {
    construct(arr_elems+k, move_from(arr_elems+k+1));
    destroy(arr_elems+k+1);
  }  /* for */
}  /* Dyn_array::remove */


template<typename an_Elem, template<typename> class Allocator>
inline void Dyn_array<an_Elem, Allocator>::clear()
/*
Remove all the elements in the array.
*/
{
  a_size   n = this->n_elems;

  for (an_index k = 0; k<n; ++k) {
    this->pop_back();
  }  /* for */
}  /* Dyn_array::clear */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::resize(a_size        new_n,
                                           const an_elem &value)
/*
Resize the array to the given length.  Any new elements are copy-inserted from
the given value.
*/
{
  a_size  old_n = this->n_elems;

  if (new_n > old_n) {
    this->reserve(new_n);
    an_elem  *arr_elems = this->elems;
    for (an_index k = old_n; k<new_n; ++k) {
      construct(arr_elems+k, value);
      ++this->n_elems;
    }  /* for */
  } else if (new_n < old_n) {
    for (an_index k = old_n; k>new_n; --k) {
      this->pop_back();
    }  /* for */
  }  /* if */
}  /* Dyn_array::resize */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::resize(a_size   new_n,
                                           an_elem  &&value)
/*
Resize the array to the given length.  Any new elements are move-inserted from
the given value.
*/
{
  a_size  old_n = this->n_elems;

  if (new_n > old_n) {
    this->reserve(new_n);
    an_elem  *arr_elems = this->elems;
    for (an_index k = old_n; k<new_n; ++k) {
      construct(arr_elems+k, move_from(&value));
      ++this->n_elems;
    }  /* for */
  } else if (new_n < old_n) {
    for (an_index k = old_n; k>new_n; --k) {
      this->pop_back();
    }  /* for */
  }  /* if */
}  /* Dyn_array::resize */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::reserve(a_size  new_cap)
/*
Increase the capacity to the given value if that given value is larger than
the current capacity.
*/
{
  a_size  old_cap = this->n_allocated;

  if (new_cap > old_cap) {
    an_allocation  a = this->realloc(an_allocation{ this->elems, old_cap },
                                     new_cap, this->n_elems);
    this->elems = a.start;
    this->n_allocated = (a_size)a.n_allocated;
  }  /* if */
}  /* Dyn_array::reserve */


template<typename an_Elem, template<typename> class Allocator>
void Dyn_array<an_Elem, Allocator>::grow()
/*
Grow the capacity of the array by about half, unless the capacity is less than
2, in which case the capacity is set to 2.
*/
{
  a_size  old_cap = this->n_allocated,
          new_cap = old_cap < 2 ? 2 : old_cap + old_cap/2 + 1;
  an_allocation  a = this->realloc(an_allocation{ this->elems, old_cap },
                                   new_cap, this->n_elems);
  this->elems = a.start;
  this->n_allocated = (a_size)a.n_allocated;
}  /* Dyn_array::grow */


template<typename an_Object,
         template<typename> class Deallocator = FE_allocator>
struct Owning_ptr: private Deallocator<an_Object> {
  /* A smart pointer managing an object it owns. */
  typedef an_Object an_object;
  typedef Deallocator<an_Object> a_deallocator;
  inline Owning_ptr()
    : a_deallocator(), ptr(NULL) {}
  inline Owning_ptr(an_object *p, const a_deallocator &d = a_deallocator())
    : a_deallocator(d), ptr(p) {}
  inline Owning_ptr(a_nullptr, const a_deallocator &d = a_deallocator())
    : a_deallocator(d), ptr(NULL) {}
  inline Owning_ptr(const Owning_ptr&) = delete;
  inline Owning_ptr(Owning_ptr&& src)
    : a_deallocator(move_from(&src)), ptr(src.ptr) { src.ptr = NULL; }
  inline ~Owning_ptr();
  inline auto operator=(const Owning_ptr&) -> Owning_ptr& = delete;
  inline auto operator=(Owning_ptr&& src) -> Owning_ptr&;
  inline auto operator=(a_nullptr) -> Owning_ptr&;
  inline auto operator->() const -> an_object*
    { return this->ptr; }
  inline auto operator*() const -> an_object&
    { return *this->ptr; }
private:
  an_object	*ptr;	/* Pointer to the owned object. */
};  /* Owning_ptr */


template<typename an_Object, template<typename> class Deallocator>
inline Owning_ptr<an_Object, Deallocator>::~Owning_ptr()
/*
Destroy and deallocate the pointed-to object, if any.
*/
{
  an_Object  *p = this->ptr;

  if (p != NULL) {
    destroy(p);
    typedef typename a_deallocator::an_allocation an_allocation;
    this->dealloc(an_allocation{ p, 1 });
  }  /* if */
}  /* Owning_ptr::~Owning_ptr */


template<typename an_Object, template<typename> class Deallocator>
inline auto Owning_ptr<an_Object, Deallocator>::operator=(Owning_ptr &&src)
                                                -> Owning_ptr&
/*
Move src to *this, then return *this.
*/
{
  if (this != &src) {
    this->ptr = src.ptr;
    src.ptr = NULL;
  }  /* if */
  return *this;
}  /* Owning_ptr::operator= */


template<typename an_Object, template<typename> class Deallocator>
inline auto Owning_ptr<an_Object, Deallocator>::operator=(a_nullptr)
                                                -> Owning_ptr&
/*
Destroy and deallocate the pointed-to object, if any.  Then, set the owning
pointer to a null value.  Return *this.
*/
{
  an_Object  *p = this->ptr;

  if (p != NULL) {
    destroy(p);
    typedef typename a_deallocator::an_allocation an_allocation;
    this->dealloc(an_allocation{ p, 1 });
    this->ptr = NULL;
  }  /* if */
  return *this;
}  /* Owning_ptr::operator= */


template<typename an_Object, typename ...an_Arg_pack>
inline Owning_ptr<an_Object> owning_ptr(an_Arg_pack ...args)
/*
Convenience function to create an owning pointer to an object allocated in
front-end memory.
*/
{
  an_Object  *p = FE_allocator<an_Object>::alloc(1).start;
  construct(p, fwd<an_Arg_pack>(args)...);
  return Owning_ptr<an_Object>(p);
}  /* owning_ptr */




template<typename an_Object>
inline an_Object min_val(const an_Object &x,
                         const an_Object &y)
/*
Return the smaller of the given values.  If the values are equal, return the
first one.
*/
{
  return y < x ? y : x;
}  /* min_val */


template<typename an_Object>
inline an_Object max_val(const an_Object &x,
                         const an_Object &y)
/*
Return the larger of the given values.  If the values are equal, return the
first one.
*/
{
  return x < y ? y : x;
}  /* max_val */


template<typename an_Object>
inline an_Object& min_ref(an_Object &x,
                          an_Object &y)
/*
Return a reference to the smaller of the given referenced values.  If the
values are equal, return the first one.
*/
{
  return y < x ? y : x;
}  /* min_ref */


template<typename an_Object>
inline an_Object& max_ref(an_Object &x,
                          an_Object &y)
/*
Return a reference to the larger of the given referenced values.  If the
values are equal, return the first one.
*/
{
  return y < x ? y : x;
}  /* max_ref */


template<typename an_Integer>
inline int log2(an_Integer n)
/*
Return floor(log2(n)), assuming n > 0.
*/
{
  int result = 0;

  while (n >>= 1) ++result;
  return result;
}  /* log2 */


template<typename an_Unsigned_integer>
int count_ones(an_Unsigned_integer  n)
/*
Return the number of trailing "ones" in the binary representation of n.
Note: Recent versions of Clang and GCC optimize this function to just a popcnt
instruction when targeting x86-64 with SSE4 extensions (option -msse4).
*/
{
  int r = 0;

  while (n != 0) {
    ++r;
    /* If the bit representation of n is ...10...0 (all trailing zeroes), then
       n-1 is ...01...1 (all trailing ones), and the line below has the net
       effect of clearing the least significant "1". */
    n &= n-1;
  }  /* while */
  return r;
}  /* count_ones */


template<typename a_Ptr, typename a_Comparison>
inline void sort_args(a_Comparison cmp,
                      a_Ptr        p_a,
                      a_Ptr        p_b)
/*
Sort the sequence [*p_a, *p_b] according to the given comparison function.
*/
{
  if (cmp(*p_b, *p_a)) {
    swap_at(p_a, p_b);
  }  /* if */
}  /* sort_args */


template<typename a_Ptr, typename a_Comparison>
inline void sort_args(a_Comparison cmp,
                      a_Ptr        p_a,
                      a_Ptr        p_b,
                      a_Ptr        p_c)
/*
Sort the sequence [*p_a, *p_b, *p_c] according to the given comparison
function.  This implementation uses a decision tree to achieve an optimal
number of comparisons (2 or 3) and moves (0, 3, or 4).
*/
{
  typedef Value_for_ptr<a_Ptr> a_value;

  if (cmp(*p_a, *p_b)) {
    if (cmp(*p_c, *p_b)) {
      a_boolean  a_lt_c = cmp(*p_a, *p_c);
      a_value    tmp = move_from(a_lt_c ? p_c : p_a);
      if (a_lt_c) {
        /* a < c < b: */
        /* Finish swap_at(c, b) below. */
      } else {
        /* c <= a < b: */
        *p_a = move_from(p_c);
      }  /* if */
      *p_c = move_from(p_b);
      *p_b = move_from(&tmp);
    } else {
      /* a < b <= c: Nothing to do. */
    }  /* if */
  } else {
    a_value  tmp = move_from(p_a);
    if (cmp(*p_b, *p_c)) {
      *p_a = move_from(p_b);
      if (cmp(*p_a, *p_c)) {
        /* b <= a < c: */
        *p_b = move_from(&tmp);
      } else {
        /* b < c <= a: */
        *p_b = move_from(p_c);
        *p_c = move_from(&tmp);
      }  /* if */
    } else {
      *p_a = move_from(p_c);
      *p_c = move_from(&tmp);
    }  /* if */
  }  /* if */
}  /* sort_args */


template<typename a_Ptr, typename a_Comparison>
inline void sort_args(a_Comparison cmp,
                      a_Ptr        p_a,
                      a_Ptr        p_b,
                      a_Ptr        p_c,
                      a_Ptr        p_d)
/*
Sort the sequence [*p_a, *p_b, *p_c, *p_d] according to the given comparison
function.  This implementation uses a decision tree to achieve an optimal
number of comparisons (4 or 5) and moves (at most 6).
*/
{
  typedef Value_for_ptr<a_Ptr> a_value;

  if (cmp(*p_a, *p_c)) {
    if (cmp(*p_b, *p_d)) {
      if (cmp(*p_a, *p_b)) {
        if (cmp(*p_c, *p_d)) {
          if (cmp(*p_b, *p_c)) {
            /* a < b < c < d: Nothing to do. */
          } else {
            /* a < c <= b < d: */
            swap_at(p_c, p_b);
          }  /* if */
        } else {
          /* a < b < d <= c: */
          swap_at(p_c, p_d);
        }  /* if */
      } else {
        a_value tmp = move_from(p_a);
        *p_a = move_from(p_b);
        if (cmp(*p_c, *p_d)) {
          /* b <= a < c < d: */
          *p_b = move_from(&tmp);
        } else {
          if (cmp(*p_a, *p_d)) {
            /* b <= a < d <= c: */
            *p_b = move_from(&tmp);
            tmp = move_from(p_d);
          } else {
            /* b < d <= a < c: */
            *p_b = move_from(p_d);
          }  /* if */
          *p_d = move_from(p_c);
          *p_c = move_from(&tmp);
        }  /* if */
      }  /* if */
    } else {
      if (cmp(*p_a, *p_d)) {
        a_value tmp = move_from(p_b);
        if (cmp(*p_c, *p_b)) {
          if (cmp(*p_d, *p_c)) {
            /* a < d < c < b: */
            *p_b = move_from(p_d);
          } else {
            /* a < c <= d <= b: */
            *p_b = move_from(p_c);
            *p_c = move_from(p_d);
          }  /* if */
          *p_d = move_from(&tmp);
        } else {
          /* a < d <= b <= c: */
          *p_b = move_from(p_d);
          *p_d = move_from(p_c);
          *p_c = move_from(&tmp);
        }  /* if */
      } else {
        a_value  tmp = move_from(p_a);
        *p_a = move_from(p_d);
        if (cmp(*p_c, *p_b)) {
          /* d <= a < c < b: */
          *p_d = move_from(p_b);
          *p_b = move_from(&tmp);
        } else {
          *p_d = move_from(p_c);
          if (cmp(*p_a, *p_b)) {
            /* d <= a < b <= c: */
            *p_c = move_from(p_b);
            *p_b = move_from(&tmp);
          } else {
            /* d <= b <= a < c: */
            *p_c = move_from(&tmp);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  } else {
    if (cmp(*p_b, *p_d)) {
      if (cmp(*p_c, *p_b)) {
        a_value  tmp = move_from(p_a);
        *p_a = move_from(p_c);
        if (cmp(*p_a, *p_d)) {
          if (cmp(*p_b, *p_a)) {
            /* c < b < a < d: */
            *p_c = move_from(&tmp);
          } else {
            /* c <= a <= b < d: */
            *p_c = move_from(p_b);
            *p_b = move_from(&tmp);
          }  /* if */
        } else {
          /* c <= a, c < b < d <= a: */
          *p_c = move_from(p_d);
          *p_d = move_from(&tmp);
        }  /* if */
      } else {
        a_value  tmp = move_from(p_a);
        *p_a = move_from(p_b);
        if (cmp(*p_a, *p_d)) {
          /* b <= c <= a < d: */
          *p_b = move_from(p_c);
          *p_c = move_from(&tmp);
        } else {
          if (cmp(*p_c, *p_d)) {
            /* b <= c < d <= a: */
            *p_b = move_from(p_c);
            *p_c = move_from(p_d);
          } else {
            /* b < d <= c <= a: */
            *p_b = move_from(p_d);
          }  /* if */
          *p_d = move_from(&tmp);
        }  /* if */
      }  /* if */
    } else {
      if (cmp(*p_c, *p_d)) {
        a_value  tmp = move_from(p_a);
        *p_a = move_from(p_c);
        if (cmp(*p_a, *p_b)) {
          if (cmp(*p_d, *p_a)) {
            /* c < d < a < b: */
  	    *p_c = move_from(&tmp);
            tmp = move_from(p_d);
          } else {
            /* c <= a <= d <= b: */
            *p_c = move_from(p_d);
          }  /* if */
          *p_d = move_from(p_b);
          *p_b = move_from(&tmp);
        } else {
          /* c < d <= b <= a: */
          *p_c = move_from(p_b);
          *p_b = move_from(p_d);
  	  *p_d = move_from(&tmp);
        }  /* if */
      } else {
        a_value  tmp = move_from(p_a);
        *p_a = move_from(p_d);
        if (cmp(*p_a, *p_b)) {
          /* d <= c <= a < b: */
          *p_d = move_from(p_b);
          *p_b = move_from(p_c);
  	  *p_c = move_from(&tmp);
        } else {
  	  *p_d = move_from(&tmp);
          if (cmp(*p_c, *p_b)) {
            /* d <= c < b <= a: */
            swap_at(p_c, p_b);
          } else {
            /* d <= b <= c <= a: */
            /* swap_at(a, d) already done by the moves above. */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* sort_args */


template<a_boolean unguarded = FALSE, int moves_limit = 0,
         typename a_Ptr, typename a_Comparison>
inline a_boolean insertion_sort(a_Ptr        first,
                                a_Ptr        last,
                                a_Comparison cmp)
/*
Sort the given sequence using insertion sort.  If unguarded is TRUE, assume
*(first-1) is valid and already in a valid position for the sorted version of
[first-1, last).  If moves_limit is nonzero and more than moves_limit moves
are performed, the sort may be abandoned and FALSE returned.  Otherwise, TRUE
is returned.
*/
{
  typedef Remove_ref<decltype(*first)> a_value;

  a_boolean result = TRUE;
  if (last-first > 1) {
    int moves = 0;
    
    for (a_Ptr it = first;;) {
      a_Ptr sift_1 = it;
      a_Ptr sift = ++it;
      if (it == last) break;

      if (cmp(*sift, *sift_1)) {
        /* Use a move-chain to move *sift to the correct position in the
           sequence sorted thus far. */
        a_value tmp = move_from(sift);
        do {
          *sift-- = move_from(sift_1);
          if (moves_limit != 0) ++moves;
        } while ((unguarded || sift != first) && cmp(tmp, *--sift_1));
        *sift = move_from(&tmp);
        if (moves_limit != 0 && moves > moves_limit) {
          result = FALSE;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* insertion_sort */


template<typename a_Ptr, typename a_Comparison>
inline void heapify(a_Ptr        ptr,
                    a_ptrdiff    len,
                    a_ptrdiff    i,
                    a_Comparison cmp)
/*
This is an adaptation of the "heapify" procedure as described in "Introduction
To Algorithms" by Cormen, Leiserson, and Rivest (CLR, first edition).  Some
changes were made:
    (1) the comparison was changed from "x > y" to "cmp(y, x)", and the name
        "largest" was replaced by "extreme".
    (2) the indexing of the array is changed to "base 0" (from "base 1")
    (3) the logic was changed from "recursive" to "iterative"
    (4) the resulting loop was unrolled once so that a chain of "exchange"
        operations (aka. swap_at) could be replaced by a chain of "move"
        operations.

ptr (called "A" in CLR) is a random access iterator ("pointer") to the start of
a sequence of length len, treated as a binary tree (the children of an element
at index k are at indices 2*k+1 and 2*k+2).  The tree rooted at index i
satisfies the heap condition, except perhaps for the root element itself.  This
function fixes the root element my moving it down the tree if needed.
*/
{
  typedef Remove_ref<decltype(*ptr)> a_value;

  a_ptrdiff  l = 2*i+1, r = l+1, extreme;

  if (l<len && cmp(*(ptr+i), *(ptr+l))) {
    extreme = l;
  } else {
    extreme = i;
  }  /* if */
  if (r<len && cmp(*(ptr+extreme), *(ptr+r))) {
    extreme = r;
  }  /* if */

  if (extreme != i) {
    /* Move the i element to a temporary, until we know where
       it fits. */
    a_value  tmp = move_from(ptr+i);
    *(ptr+i) = move_from(ptr+extreme);
    i = extreme;
    for (;;) {
      a_Ptr  p_extreme;
      l = 2*i+1, r = l+1;
      if (l<len && cmp(tmp, *(ptr+l))) {
        extreme = l;
        p_extreme = ptr+l;
      } else {
        extreme = i;
        p_extreme = &tmp;
      }  /* if */
      if (r<len && cmp(*p_extreme, *(ptr+r))) {
        extreme = r;
        p_extreme = ptr+r;
      }  /* if */
      if (extreme != i) {
        *(ptr+i) = move_from(p_extreme);
        i = extreme;
      } else {
        break;
      }  /* if */
    }  /* for */
    /* i is the index of the last "extreme" that was moved up.  Move the top
       (misplaced) element to this position. */
    *(ptr+i) = move_from(&tmp);
  }  /* if */
}  /* heapify */


template<typename a_Ptr, typename a_comparison>
void build_heap(a_Ptr        first,
                a_ptrdiff    len,
                a_comparison cmp)
/*
This is an adaptation of the "Build-Heap" procedure as described in
"Introduction To Algorithms" by Cormen, Leiserson, and Rivest (CLR, first
edition).
*/
{
  for (a_ptrdiff i = len/2; i > 0;) {
    --i;
    heapify(first, len, i, cmp);
  }  /* for */
}  /* build_heap */


template<typename a_Ptr, typename a_comparison>
void heap_sort(a_Ptr        first,
               a_Ptr        last,
               a_comparison cmp)
/*
This is an adaptation of the "Heapsort" procedure as described in "Introduction
To Algorithms" by Cormen, Leiserson, and Rivest (CLR, first edition).
*/
{
  a_ptrdiff  len = last-first;

  if (len > 1) {
    build_heap(first, len, cmp);
    for (a_ptrdiff i = len; i>1;) {
      --i;
      swap_at(first, first+i);
      --len;
      heapify(first, len, 0, cmp);
    }  /* for */
  }  /* if */
}  /* heap_sort */


namespace pdqsort_impl {
/* An adaptation of Orson Peters' "pattern-defeating quicksort" (PDQSort).
   This is not the original code, but an adaptation of it.  The original
   copyright notice read as follows:

    pdqsort.h - Pattern-defeating quicksort.

    Copyright (c) 2015 Orson Peters

    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from
    the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software in
       a product, an acknowledgment in the product documentation would be
       appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.

    3. This notice may not be removed or altered from any source distribution.
*/

/*
The maximum length of a sequence for which insertion sort is used.
*/
#define MAX_INSERTION_SORT_LENGTH 15

/*
The minimum length of a sequence to cause the pivot to be selected using
Tukey's ninther.
*/
#define MIN_NINTHER_PIVOT_LENGTH 129

/*
Length in bytes of a cache line (must be a power of 2).
*/
#define CACHE_LINE_SIZE 64

/*
The input is split up in blocks of the following length.  This follows the
approach of "BlockQuicksort: How Branch Mispredictions don't affect Quicksort"
by Stefan Edelkamp and Armin Weiss.  The block size should be a multiple of 8,
and no more than 248 (it must fit in a byte).
*/
#define BLOCK_SIZE ((a_byte)64)


template<typename an_Object>
inline an_Object* align_to_cache_line(an_Object *p)
/*
Return p minimally advanced to be aligned to a cache line.
*/
{
  return (an_Object*)
             (((uintptr_t)p+CACHE_LINE_SIZE-1) & (uintptr_t)-CACHE_LINE_SIZE);
}  /* align_to_cache_line */


template<typename a_Ptr>
inline void move_using_offsets(a_Ptr     first,
                               a_Ptr     last,
                               a_byte*   offsets_l,
                               a_byte*   offsets_r,
                               int       num,
                               a_boolean use_swaps)
/*
Given a sequence [first, last], offset_l[0..num-1] are forward offsets wrt.
first and offset_r[0..num-1] are backward offsets wrt. last.  Let l0, l1, ...
be the elements designated by offset_l and r0, r1, ... be the elements
designated by offset_r.  If use_swaps is TRUE, swap (l0, r0), then (l1, r1),
then (l2, r2), etc.  Otherwise, perform a circular move chain:
    tmp <- l0 <- r0 <- l1 <- r1 <- l2 ... < r[num-1] <- tmp
Either way, the ln values will be swapped with the rn values.
*/
{
  typedef Value_for_ptr<a_Ptr> a_value;
  if (num != 0) {
    if (use_swaps) {
      /* This case is needed for the descending distribution, where we need
         to have proper swapping for pdqsort to remain O(n). */
      for (int i = 0; i < num; ++i) {
        swap_at(first+offsets_l[i], last-offsets_r[i]);
      }  /* for */
    } else {
      a_Ptr   l = first + offsets_l[0],
              r = last - offsets_r[0];
      a_value tmp = move_from(l);
      *l = move_from(r);
      for (int i = 1; i < num; ++i) {
        l = first+offsets_l[i];
        *r = move_from(l);
        r = last-offsets_r[i];
        *l = move_from(r);
      }  /* for */
      *r = move_from(&tmp);
    }  /* if */
  }  /* if */
}  /* move_using_offsets */


template<typename a_Ptr, typename a_Comparison>
inline Ptr_with_flag<a_Ptr> partition_right_branchless(a_Ptr        begin,
                                                       a_Ptr        end,
                                                       a_Comparison cmp)
/*
Partition the sequence [begin, end) around pivot *begin using comparison
function cmp.  Elements equal to the pivot are put in the right-hand partition.
Return as a "pointer with flag" (a) the position of the pivot after
partitioning and (b) whether the passed sequence already was correctly
partitioned.  This function assumes the pivot is a median of at least 3
elements and that [begin, end) is at least MAX_INSERTION_SORT_LENGTH+1 long.
Uses branchless partitioning.
*/
{
  typedef Value_for_ptr<a_Ptr> a_value;

  /* Cache the pivot in a local variable. */
  a_value pivot = move_from(begin);
  a_Ptr first = begin, last = end;

  /* Find the first element greater than or equal to the pivot (the median of
     3 guarantees this exists). */
  while (cmp(*++first, pivot)) {}
  /* Find the first element strictly smaller than the pivot. We have to guard
     this search if there was no element before *first. */
  if (first-1 == begin) {
    while (first < last && !cmp(*--last, pivot)) {}
  } else {
    while (!cmp(*--last, pivot)) {}
  }  /* if */

  /* If the first pair of elements that should be swapped to partition are
     the same element, the passed in sequence already was correctly
     partitioned. */
  a_boolean already_partitioned = first >= last;
  if (!already_partitioned) {
    swap_at(first, last);
    ++first;
  }  /* if */

  /* The following branchless partitioning is derived from "BlockQuicksort:
     How Branch Mispredictions don't affect Quicksort" by Stefan Edelkamp
     and Armin Weiss. */
  a_byte  offsets_l_storage[BLOCK_SIZE+CACHE_LINE_SIZE],
          offsets_r_storage[BLOCK_SIZE+CACHE_LINE_SIZE];
  a_byte* offsets_l = align_to_cache_line(offsets_l_storage);
  a_byte* offsets_r = align_to_cache_line(offsets_r_storage);
  int     num_l = 0, num_r = 0, start_l = 0, start_r = 0;
  while (last-first > 2*BLOCK_SIZE) {
    /* Fill up offset blocks with elements that are on the wrong side. */
    if (num_l == 0) {
      start_l = 0;
      a_Ptr it = first;
      for (a_byte i = 0; i < BLOCK_SIZE;) {
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
        offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
      }  /* for */
    }  /* if */
    if (num_r == 0) {
      start_r = 0;
      a_Ptr it = last;
      for (a_byte i = 0; i < BLOCK_SIZE;) {
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
        offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
      }  /* for */
    }  /* if */

    /* Move the misplaced elements to the other side and update block sizes
       and first/last boundaries. */
    int num = min_val(num_l, num_r);
    move_using_offsets(first, last, offsets_l+start_l, offsets_r+start_r,
                       num, num_l == num_r);
    num_l -= num; num_r -= num;
    start_l += num; start_r += num;
    if (num_l == 0) first += BLOCK_SIZE;
    if (num_r == 0) last -= BLOCK_SIZE;
  }  /* while */

  int l_size = 0, r_size = 0,
      unknown_left = (last-first) - ((num_r || num_l) ? BLOCK_SIZE : 0);
  /* Handle a leftover block by assigning the unknown elements to the other
     block. */
  if (num_r) {
    l_size = unknown_left;
    r_size = BLOCK_SIZE;
  } else if (num_l) {
    l_size = BLOCK_SIZE;
    r_size = unknown_left;
  } else {
    /* No leftover block: Split the unknown elements in two blocks. */
    l_size = unknown_left/2;
    r_size = unknown_left-l_size;
  }  /* if */

  /* Fill offset buffers if needed. */
  if (unknown_left && !num_l) {
    start_l = 0;
    a_Ptr it = first;
    for (unsigned char i = 0; i < l_size;) {
      offsets_l[num_l] = i++; num_l += !cmp(*it, pivot); ++it;
    }  /* for */
  }  /* if */
  if (unknown_left && !num_r) {
    start_r = 0;
    a_Ptr it = last;
    for (unsigned char i = 0; i < r_size;) {
      offsets_r[num_r] = ++i; num_r += cmp(*--it, pivot);
    }  /* for */
  }  /* if */

  int num = min_val(num_l, num_r);
  move_using_offsets(first, last, offsets_l+start_l, offsets_r+start_r,
                     num, num_l == num_r);
  num_l -= num; num_r -= num;
  start_l += num; start_r += num;
  if (num_l == 0) first += l_size;
  if (num_r == 0) last -= r_size;
      
  /* We have now fully identified [first, last)'s proper position: Swap the
     last elements. */
  if (num_l) {
    offsets_l += start_l;
    while (num_l--) {
      swap_at(first+offsets_l[num_l], --last);
    }  /* while */
    first = last;
  }  /* if */
  if (num_r) {
    offsets_r += start_r;
    while (num_r--) {
      swap_at(last-offsets_r[num_r], first);
      ++first;
    }  /* while */
    last = first;
  }  /* if */

  /* Put the pivot in the right place. */
  a_Ptr pivot_pos = first-1;
  *begin = move_from(pivot_pos);
  *pivot_pos = move_from(&pivot);
  return ptr_with_flag(pivot_pos, already_partitioned);
}  /* partition_right_branchless */


template<typename a_Ptr, typename a_Comparison>
inline Ptr_with_flag<a_Ptr> partition_right(a_Ptr        begin,
                                            a_Ptr        end,
                                            a_Comparison cmp)
/*
Partition [begin, end) around pivot *begin using comparison function cmp.
Elements equal to the pivot are put in the right-hand partition.  Return the
position of the pivot after partitioning and whether the passed sequence
already was correctly partitioned.  This function assumes the pivot is a
median of at least 3 elements and that [begin, end) is at least
MAX_INSERTION_SORT_LENGTH+1 long.
*/
{
  typedef Value_for_ptr<a_Ptr> a_value;

  /* Cache the pivot in a local variable. */
  a_value pivot = move_from(begin);
  a_Ptr first = begin, last = end;

  /* Find the first element greater than or equal to the pivot (the median of
     3 guarantees this exists). */
  while (cmp(*++first, pivot)) {}
  /* Find the first element strictly smaller than the pivot.  We have to guard
     this search if there was no element before *first. */
  if (first-1 == begin) {
    while (first < last && !cmp(*--last, pivot)) {}
  } else {
    while (!cmp(*--last, pivot)) {}
  }  /* if */

  /* If the first pair of elements that should be swapped to partition are
     the same element, the passed-in sequence already was correctly
     partitioned. */
  a_boolean already_partitioned = first >= last;
    
  /* Keep swapping pairs of elements that are on the wrong side of the pivot.
     Previously swapped pairs guard the searches, which is why the first
     iteration is special-cased above. */
  while (first < last) {
    swap_at(first, last);
    while (cmp(*++first, pivot)) {}
    while (!cmp(*--last, pivot)) {}
  }  /* while */

  /* Put the pivot in the right place. */
  a_Ptr pivot_pos = first-1;
  *begin = move_from(pivot_pos);
  *pivot_pos = move_from(&pivot);

  return ptr_with_flag(pivot_pos, already_partitioned);
}  /* partition_right */


template<typename a_Ptr, typename a_Comparison>
inline a_Ptr partition_left(a_Ptr        begin,
                            a_Ptr        end,
                            a_Comparison cmp)
/*
Same as partition_right, except elements equal to the pivot are put to the
left of the pivot and only the pivot position is returned.  Since this is only
used for the "many equal" case -- which is somewhat rare -- and in that case
pdqsort already has O(n) performance, no block quicksort is applied here for
simplicity.
*/
{
  typedef Value_for_ptr<a_Ptr> a_value;

  /* Cache the pivot in a local variable. */
  a_value pivot = move_from(begin);
  a_Ptr first = begin, last = end;

  while (cmp(pivot, *--last)) {}

  if (last+1 == end) {
    while (first < last && !cmp(pivot, *++first)) {}
  } else {
    while (!cmp(pivot, *++first)) {}
  }  /* if */
  while (first < last) {
    swap_at(first, last);
    while (cmp(pivot, *--last)) {}
    while (!cmp(pivot, *++first)) {}
  }  /* while */

  a_Ptr pivot_pos = last;
  *begin = move_from(pivot_pos);
  *pivot_pos = move_from(&pivot);

  return pivot_pos;
}  /* partition_left */


template<a_boolean branchless, typename a_Ptr, typename a_Comparison>
void pdqsort_loop(a_Ptr        begin,
                  a_Ptr        end,
                  a_Comparison cmp,
                  int          bad_allowed,
                  a_boolean    leftmost)
/*
Sort the elements in [begin, end) according to the comparison object cmp.  The
primary algorithm is Quicksort.  If branchless is TRUE, a variation of
Quicksort is used to reduce the number of mispredicted branches.  If too many
partition pairs are highly unbalanced, the algorithm switches to heapsort to
sort the leaf partitions.  "Too many" in that context is determined by
bad_allowed.  This function is called recursively on left partitions.  For
recursive calls that do not include the original left-most element, leftmost
is FALSE.
*/
{
  /* Use a loop to eliminate tail-recursion on the right partition. */
  for (;;) {
    a_ptrdiff length = end-begin;

    /* Use insertion_sort for small arrays. */
    if (length < MAX_INSERTION_SORT_LENGTH+1) {
      if (leftmost) {
        insertion_sort(begin, end, cmp);
      } else {
        insertion_sort</*unguarded=*/TRUE>(begin, end, cmp);
      }  /* if */
      break;
    }  /* if */

    /* Choose a pivot as the median of 3 or the Tukey ninther (pseudo-median
       of 9). */
    a_ptrdiff half_length = length/2;
    a_Ptr     mid = begin+half_length;
    if (length >= MIN_NINTHER_PIVOT_LENGTH) {
      sort_args(cmp, begin, mid, end-1);
      sort_args(cmp, begin+1, mid-1, end-2);
      sort_args(cmp, begin+2, mid+1, end-3);
      sort_args(cmp, mid-1, mid, mid+1);
      swap_at(begin, mid);
    } else {
      sort_args(cmp, mid, begin, end-1);
    }  /* if */

    /* If *(begin-1) is the end of the right partition of a previous partition
       operation there is no element in [begin, end) that is smaller than
       *(begin-1).  Then if our pivot compares equal to *(begin-1) we change
       strategy, putting equal elements in the left partition and greater
       elements in the right partition.  In that case, we do not have to
       process the left partition any further because all its values are equal
       and therefore already sorted. */
    if (!leftmost && !cmp(*(begin-1), *begin)) {
      begin = partition_left(begin, end, cmp)+1;
      continue;
    }  /* if */

    /* Partition [begin, end) around the pivot *begin. */
    Ptr_with_flag<a_Ptr> part_result =
                    branchless ? partition_right_branchless(begin, end, cmp)
                               : partition_right(begin, end, cmp);
    a_Ptr      pivot_pos = part_result.ptr();
    a_boolean  already_partitioned = part_result.flagged();

    /* If the partitions are highly unbalanced, shuffle some elements to break
       many patterns. */
    a_ptrdiff  l_size = pivot_pos-begin;
    a_ptrdiff  r_size = end-(pivot_pos+1);
    a_boolean  highly_unbalanced = (l_size < length/8) || (r_size < length/8);
    if (highly_unbalanced) {
      /* If we have seen too many highly-unbalanced partitions, switch to
         heapsort to guarantee O(n log n). */
      if (--bad_allowed == 0) {
          heap_sort(begin, end, cmp);
          break;
      }  /* if */
      if (l_size >= MAX_INSERTION_SORT_LENGTH+1) {
        /* Shuffle some elements around, unless we will use insertion sort on
           the next level. */
        swap_at(begin, begin+l_size/4);
        swap_at(pivot_pos-1, pivot_pos-l_size/4);

        if (l_size > MIN_NINTHER_PIVOT_LENGTH-1) {
          swap_at(begin+1, begin+(l_size/4+1));
          swap_at(begin+2, begin+(l_size/4+2));
          swap_at(pivot_pos-2, pivot_pos-(l_size/4+1));
          swap_at(pivot_pos-3, pivot_pos-(l_size/4+2));
        }  /* if */
      }  /* if */
      if (r_size >= MAX_INSERTION_SORT_LENGTH+1) {
        /* Shuffle some elements around, unless we will use insertion sort on
           the next level. */
        swap_at(pivot_pos+1, pivot_pos+(1+r_size/4));
        swap_at(end-1, end-r_size/4);
            
        if (r_size > MIN_NINTHER_PIVOT_LENGTH-1) {
          swap_at(pivot_pos+2, pivot_pos+(2+r_size/4));
          swap_at(pivot_pos+3, pivot_pos+(3+r_size/4));
          swap_at(end-2, end-(1+r_size/4));
          swap_at(end-3, end-(2+r_size/4));
        }  /* if */
      }  /* if */
    } else {
      /* If the partitions were somewhat balanced and we tried to sort an
         already-partitioned sequence try to use insertion sort, but give up
         after more than about 8 moves. */
      if (already_partitioned &&
          insertion_sort</*unguarded=*/FALSE, /*moves_limit=*/8>(
                                                     begin, pivot_pos, cmp) &&
          insertion_sort</*unguarded=*/FALSE, /*moves_limit=*/8>(
                                                   pivot_pos + 1, end, cmp)) {
           break;
      }  /* if */
    }  /* if */
        
    /* Recurse to sort the left partition.  The right partition is handled
       by the loop (i.e., we eliminated tail recursion). */
    pdqsort_loop<branchless>(begin, pivot_pos, cmp, bad_allowed, leftmost);
    begin = pivot_pos+1;
    leftmost = FALSE;
  }  /* for */
}  /* pdqsort_loop */

#undef BLOCK_SIZE
#undef CACHE_LINE_SIZE
#undef MIN_NINTHER_PIVOT_LENGTH
#undef MAX_INSERTION_SORT_LENGTH

}  /* namespace pdqsort_impl */

template<typename a_Ptr, typename a_Comparison>
inline void sort(a_Ptr        begin,
                 a_Ptr        end,
                 a_Comparison cmp)
/*
Sort [begin, end) with Orson Peters' "pattern-defeating quicksort" (PDQSort).
a_Ptr must be a random-access iterator.  cmp must induce a weak ordering on
the elements of the sequence.
*/
{
  if (begin != end) {
    pdqsort_impl::pdqsort_loop</*branchless=*/TRUE>(
                         begin, end, cmp, log2(end-begin), /*left_most=*/TRUE);
  }  /* if */
}  /* sort */


template<typename a_Sequence, typename a_Comparison>
inline void sort(a_Sequence    *p_seq,
                 a_Comparison  cmp)
/*
Sort the elements of the given sequence.
*/
{
  sort(p_seq->begin(), p_seq->end(), cmp);
}  /* sort */


#if DEBUG
template<typename T>
void db_f_print_t(FILE *stream, ARG_UNUSED const T &value)
/*
Provide a generic printing interface for generic function diagnostics.
*/
{
  fprintf(stream, "unspecified");
}  /* db_f_print_t */
#endif /* DEBUG */


#if EXPENSIVE_CHECKING
template<typename a_Value_Fn>
inline void validate_elements_in_order(ptrdiff_t  num_elements,
                                       a_Value_Fn value_fn)
/*
Validate that the input container of num_elements elements is in order for a
binary search.
*/
{
  a_boolean any_out_of_order = FALSE;
  auto &&last_value = value_fn(0);

  for (ptrdiff_t i = 1; i < num_elements; ++i) {
    auto &&curr_value = value_fn(i);
    if (!(last_value < curr_value)) {
      fprintf(stderr, "Binary search element %td (", i);
#if DEBUG
      db_f_print_t(stderr, curr_value);
#else /* !DEBUG */
      fprintf(stderr, "unspecified");
#endif /* DEBUG */
      fprintf(stderr, ") is out of order\n");
      any_out_of_order = TRUE;
    }  /* if */
    last_value = curr_value;
  }  /* for */
  /* Fail loudly if anything is not ordered correctly, delayed so multiple
     order errors can be reported first. */
  check_assertion(!any_out_of_order);
}  /* validate_elements_in_order */
#endif /* EXPENSIVE_CHECKING */


template<typename T, typename a_Value_Fn>
inline ptrdiff_t lower_bound(ptrdiff_t  num_elements,
                             const T    &value,
                             a_Value_Fn value_fn)
/*
Search for the first element in a container of num_elements elements that is
not less than (i.e., greater or equal to) value using value_fn to retrieve
values at a given index.  value_fn should take a ptrdiff_t argument
representing a given index into the container, and return the value of type T
at that index.  Return the index of said element or -1 if no such element is
found.
*/
{
  ptrdiff_t begin_idx = 0;
  ptrdiff_t curr_size = num_elements;

#if EXPENSIVE_CHECKING
  if (num_elements >= 2) {
    validate_elements_in_order(num_elements, value_fn);
  }  /* if */
#endif /* EXPENSIVE_CHECKING */
  while (curr_size > 0) {
    /* Calculate the current index.  First compute an index relative to the
       amount of data we have.  Then add the relative index to our starting
       index to get the true index into the unknown container. */
    ptrdiff_t midpoint_idx = curr_size / 2;
    ptrdiff_t curr_idx = begin_idx + midpoint_idx;
    /* Retrieve the value at our current index from the unknown container.
       Extracted to a variable to ease debugging. */
    auto &&curr_value = value_fn(curr_idx);
    if (curr_value < value) {
      /* The value was smaller than the value we're searching for (we want the
         first value greater than or equal to our target value) so consider the
         value to our immediate right the new best candidate.

         The best candidate and starting search positions are algorithmically
         equivalent. begin_idx is the largest value we know of that was
         preceded by a value smaller than our target value.  Thus, it's both
         our best current answer to the lower bound, and the earliest point we
         will need to check going forward.  Therefore, update begin_idx with
         this new best candidate index.

         Since this algorithm works with a rolling count of elements rather
         than indexes, we need to subtract the number of elements we just
         removed (i.e., our midpoint index + 1) to make the count inclusive.
         Note that since we may have an even or odd count, we can't do a blind
         assignment of curr_size / 2 (inflates the count when even) or
         curr_size / 2 - 1 (deflates the count when odd) without adding an
         additional branch.

         Additionally, note that if the index we've moved onto is past the end
         of the array that's okay as we will end up with num_elements (which
         then gets transformed to -1 at the end of the function) as intended,
         and we will not reenter as curr_size will be 0. */
      begin_idx = curr_idx + 1;
      curr_size -= midpoint_idx + 1;
    } else {
      /* The value was greater than or equal to the value we're searching for,
         so we need to consider earlier elements.

         Since this algorithm works with a rolling count of elements rather
         than indexes, we need to subtract the number of elements we just
         removed.  Since our midpoint already in effect represents the number
         of elements in the left half that remain to be examined, simply use
         its value. */
      curr_size = midpoint_idx;
    }  /* if */
  }  /* while */
  /* We ran out of elements and ran past the end of the container, detect this
     and return -1 (to better to conform to developer expectations of "no
     result"). */
  if (begin_idx == num_elements) {
    begin_idx = -1;
  }  /* if */
  return begin_idx;
}  /* lower_bound */


template<typename T>
inline ptrdiff_t array_lower_bound(T         *t_start,
                                   ptrdiff_t num_elements,
                                   const T   &value)
/*
Search for the first element in an array of num_elements elements beginning at
t_start, that is not less than (i.e., greater or equal to) value.  Return the
index of said element or -1 if no such element is found.
*/
{
  auto read_array_element_at = [t_start](ptrdiff_t idx) {
    return *(t_start + idx);
  };
  return lower_bound(num_elements, value, read_array_element_at);
}  /* array_lower_bound */


template<typename T, typename a_Value_Fn>
inline ptrdiff_t bin_search(ptrdiff_t  num_elements,
                            const T    &value,
                            a_Value_Fn value_fn)
/*
Search for the first element in a container of num_elements elements that is
equal to value using value_fn to retrieve values at a given index.  value_fn
should take a ptrdiff_t argument representing a given index into the container,
and return the value of type T at that index.  Return the index of said element
or -1 if no such element is found.
*/
{
  ptrdiff_t result_idx = lower_bound(num_elements, value, value_fn);

  /* If we received a valid result index into our container, check to see if
     the value at the result index matches the value we were searching for.

     Use !(x == y) rather than x != y to simplify implementing wrapper types we
     may want to feed to bin_search. */
  if (result_idx != -1 && !(value_fn(result_idx) == value)) {
    /* The value didn't match so invalidate the result index. */
    result_idx = -1;
  }  /* if */
  return result_idx;
}  /* bin_search */


template<typename T>
inline ptrdiff_t array_bin_search(T         *t_start,
                                  ptrdiff_t num_elements,
                                  const T   &value)
/*
Search for the first element in an array of num_elements elements beginning at
t_start, that is equal to value.  Return the index of said element or -1 if no
such element is found.
*/
{
  auto read_array_element_at = [t_start](ptrdiff_t idx) {
    return *(t_start + idx);
  };
  return bin_search(num_elements, value, read_array_element_at);
}  /* array_bin_search */


template<template<typename> class Allocator>
struct Allocated_string;

namespace detail {

/*
A forward declaration of a type specialized to handle converting different
values to strings.

Each specialization should implement two functions:

  static size_t size_hint_of(a_Type value);

  template<typename a_Dyn_array>
  static inline void append_into(a_Dyn_array &underlying_array,
                                 a_Type      value,
                                 size_t      size_hint;
*/
template<typename a_Type>
struct String_formatter;

/*
A string formatter for a_const_char* (C-string) values.
*/
template<>
struct String_formatter<a_const_char*> {
  static size_t size_hint_of(a_const_char *value)
    { return strlen(value); }
  template<typename a_Dyn_array>
  static inline void append_into(a_Dyn_array  &underlying_array,
                                 a_const_char *chars,
                                 size_t       size_hint);
};  /* String_formatter */


template<typename a_Dyn_array>
void
String_formatter<a_const_char*>::append_into(a_Dyn_array  &underlying_array,
                                             a_const_char *chars,
                                             size_t       size_hint)
/*
Append the given characters into the underlying array.  size_hint is the number
of characters to append.
*/
{
  underlying_array.insert(underlying_array.length(), chars, size_hint);
}  /* String_formatter::append_into */


/*
A string formatter for Allocated_string values.
*/
template<template<typename> class Allocator>
struct String_formatter<Allocated_string<Allocator>> {
  static size_t size_hint_of(Allocated_string<Allocator> str)
    { return str.length(); }

  template<typename a_Dyn_array>
  static inline void
  append_into(a_Dyn_array                       &underlying_array,
              const Allocated_string<Allocator> &str,
              size_t                            size_hint);
};  /* String_formatter */


template<template<typename> class Allocator>
template<typename a_Dyn_array>
void String_formatter<Allocated_string<Allocator>>::append_into(
                          a_Dyn_array                       &underlying_array,
                          const Allocated_string<Allocator> &str,
                          ARG_UNUSED size_t                 size_hint)
/*
Convert the given Allocated_string value into its character representation, and
append the characters into the underlying array.  size_hint is unused.
*/
{
  underlying_array.insert(underlying_array.length(), str.as_temp_characters(),
                          str.length());
}  /* append_into */


/*
A string formatter (and associated recursive specializations) for unsigned long
long, unsigned long, and unsigned values.
*/
template<>
struct String_formatter<unsigned long long> {
  constexpr static size_t size_hint_of(unsigned long long value)
    { return 21; }
  template<typename a_Dyn_array>
  static inline void append_into(a_Dyn_array        &underlying_array,
                                 unsigned long long value,
                                 size_t             size_hint);
};  /* String_formatter */


template<>
struct String_formatter<unsigned long> : String_formatter<unsigned long long> {
};  /* String_formatter */


template<>
struct String_formatter<unsigned> : String_formatter<unsigned long long> {
};  /* String_formatter */


template<typename a_Dyn_array>
void String_formatter<unsigned long long>::append_into(
                                          a_Dyn_array        &underlying_array,
                                          unsigned long long value,
                                          ARG_UNUSED size_t  size_hint)
/*
Convert the given unsigned integer value into its character representation, and
append the characters representing the value into the underlying array.
size_hint is unused.
*/
{
  constexpr size_t buff_size =
                      String_formatter<unsigned long long>::size_hint_of(0ull);
  char             string_buffer[buff_size] = {};

  snprintf(string_buffer, buff_size, "%llu", value);
  String_formatter<a_const_char*>::append_into(underlying_array, string_buffer,
                                               strlen(string_buffer));
}  /* append_into */


/*
A string formatter (and associated recursive specializations) for long long,
long, and int values.
*/
template<>
struct String_formatter<long long> {
  constexpr static size_t size_hint_of(long long value)
    { return 20; }
  template<typename a_Dyn_array>
  static inline void append_into(a_Dyn_array &underlying_array,
                                 long long   value,
                                 size_t      size_hint);
};  /* String_formatter */


template<>
struct String_formatter<long> : String_formatter<long long> {
};  /* String_formatter */


template<>
struct String_formatter<int> : String_formatter<long long> {
};  /* String_formatter */


template<typename a_Dyn_array>
void String_formatter<long long>::append_into(
                                           a_Dyn_array       &underlying_array,
                                           long long         value,
                                           ARG_UNUSED size_t size_hint)
/*
Convert the given signed integer value into its character representation, and
append the characters representing the value into the underlying array.
size_hint is unused.
*/
{
  constexpr size_t buff_size = String_formatter<long long>::size_hint_of(0ll);
  char             string_buffer[buff_size] = {};

  snprintf(string_buffer, buff_size, "%lli", value);
  String_formatter<a_const_char*>::append_into(underlying_array, string_buffer,
                                               strlen(string_buffer));
}  /* append_into */


template<typename a_Reserve_fn, typename... a_Text_convertible_type>
void append_with_custom_reserve(a_Reserve_fn               reserve_func,
                                a_Text_convertible_type... args)
/*
The "reserve_func" should be a function that takes a character count estimate
and returns a pointer to a Dyn_array.  Said Dyn_array should be returned with
an appropriate capacity allocated for the given estimate.  The arguments (i.e.,
"args") provided are mapped to a
detail::String_formatter<a_Text_convertible_type> (abbreviated "formatter").
The arguments compose the character count estimate via the sum of the
respective formatter::size_hint_of functions.  Once the character count
estimate is computed, reserve_func is called with the given estimate, and then
each argument is sequentially appended using the respective
formatter::append_into functions.
*/
{
  /* Gather size estimates of each pack element. */
  size_t element_sizes [] = {
    detail::String_formatter<a_Text_convertible_type>::size_hint_of(args)...
  };
  /* Start at an initial size of 1 to account for the null terminator. */
  size_t total_size = 1;

  /* Calculate the total size estimate off of the element sizes, and get a
     backing Dyn_array instance (with the appropriate space reserved based on
     the estimate). */
  for (size_t i = 0; i < sizeof...(args); ++i) {
    total_size += element_sizes[i];
  }  /* for */
  auto *backing_array = reserve_func(total_size);

  /* Use a braced-init-list to provide ordered evaluation of each pack element,
     calling the appropriate append function.  The created array's values are
     irrelevant and discarded. */
  size_t counter = 0;
  ARG_UNUSED unsigned discarded[] = {
    (detail::String_formatter<a_Text_convertible_type>::append_into(
                                              *backing_array,
                                              args,
                                              element_sizes[counter++]), 0u)...
  };
  /* Ensure the underlying array is always null-terminated. */
  if (backing_array->length() < 1 ||
      (*backing_array)[backing_array->length() - 1] != '\0') {
    backing_array->insert(backing_array->length(), '\0');
  }  /* if */
}  /* append_with_custom_reserve */

}  /* detail */

/*
The fundamental string type, which can be instantiated with different
allocators as necessary.
*/
template<template<typename> class Allocator>
struct Allocated_string {
  typedef Allocator<char> an_allocator;
  typedef typename an_allocator::a_size a_size;

  template<typename... a_Text_convertible_type>
  inline Allocated_string(an_allocator a, a_Text_convertible_type... args);
  template<typename... a_Text_convertible_type>
  inline Allocated_string(a_Text_convertible_type... args)
    : Allocated_string(an_allocator{}, args...)
    { }

  a_const_char *as_temp_characters() const
    { return backing_array.begin(); }
  a_size length() const
    { return backing_array.length() - 1; }

  template<typename... a_Text_convertible_type>
  inline Allocated_string<Allocator>& append(a_Text_convertible_type... args);
private:
  Dyn_array<char, Allocator>
                backing_array;
                        /* The character array which is responsible for holding
                           the actual characters composing the string. */
};  /* Allocated_string */


template<template<typename> class Allocator>
template<typename... a_Text_convertible_type>
inline Allocated_string<Allocator>::Allocated_string(
                                               an_allocator               a,
                                               a_Text_convertible_type... args)
/*
Construct a new string using the given allocator.  The passed arguments are
appended in the fashion described in detail::append_with_custom_reserve.
*/
{
  /* Delay initialization of the backing array until size hints are computed so
     that only one allocation is performed. */
  auto reserve_func = [this, &a](a_size total_size) {
    this->backing_array = {total_size, a};
    return &this->backing_array;
  };
  detail::append_with_custom_reserve(reserve_func, args...);
}  /* Allocated_string */


template<template<typename> class Allocator>
template<typename... a_Text_convertible_type>
inline Allocated_string<Allocator>&
Allocated_string<Allocator>::append(a_Text_convertible_type... args)
/*
The passed arguments are appended in the fashion described in
detail::append_with_custom_reserve, and a reference to this Allocated_string
is returned.
*/
{
  auto reserve_func = [this](a_size total_size) {
    this->backing_array.reserve(this->backing_array.length() + total_size);
    return &this->backing_array;
  };
  detail::append_with_custom_reserve(reserve_func, args...);
  return *this;
}  /* Allocated_string::append */


/*
An alias for the "normal" usage of Allocated_string (i.e., with a dynamically
allocating fe_alloc-backed allocator).
*/
typedef Allocated_string<FE_allocator> a_string;

/*
The Ptr_map template
====================
The Ptr_map<K, V, A> template defined below is a flat hash-based map of keys of
type K to values of type V, using A as an allocator.  It is called Ptr_map
because it works well to map non-null pointers, but the only notable key-type
requirement is that K{} (i.e., the default-constructed value of K) not be used
as a key value.  So mapping nonzero integers works very well, also, as do other
types for which the default-constructed value is never a valid key (the
default-constructed value is used to denote "empty" slots in the table).

Ptr_map uses unqualified calls to "hash_ptr" to compute hash values.  For keys
that aren't native pointers or integers, add an overloaded function that covers
that key type.  The function should return type uintptr_t.

New (key, value) pairs can be added with the map(...) member and a value
associated with a given key can be retrieved with get(k).  If the hash of a
key is already known, variants map_with_hash(...) and get_with_hash(...) are
available.  Removing a key is achieved by calling the unmap(...) member.

This is not a multi-map: Client code has to ensure that specific keys are not
matched twice.  An existing key can have its associated value replaced by
invoking the members replace(...) or replace_with_hash(...).  If it is not
known whether a key is present in the map, the members map_or_replace(...) and
map_or_replace_with_hash(...) will efficiently map the key if it is not yet
present or replace the associated value if it is present.

This implementation limits the load factor to 0.5.  That makes for efficient
lookups in most cases, but can be wasteful of storage.  It is therefore best
to keep the (key, value) size small.  In some cases, it may therefore be
useful to have the key and/or value be a handle to the associated data instead
of the data itself.

Ptr_map does not currently provide an interface to traverse all the elements
in the map.
*/

inline uintptr_t hash_ptr(void  *ptr)
/*
Return a hash value for the given pointer value.
*/
{
#if HOST_ALIGNMENT_REQUIRED == 1
#define HASH_PTR_SHIFT 0
#else /* HOST_ALIGNMENT_REQUIRED > 1 */
#if HOST_ALIGNMENT_REQUIRED == 2
#define HASH_PTR_SHIFT 1
#else /* HOST_ALIGNMENT_REQUIRED > 2 */
#if HOST_ALIGNMENT_REQUIRED == 4
#define HASH_PTR_SHIFT 2
#else /* HOST_ALIGNMENT_REQUIRED > 4 */
#if HOST_ALIGNMENT_REQUIRED == 8
#define HASH_PTR_SHIFT 3
#else /* HOST_ALIGNMENT_REQUIRED > 8 */
#if HOST_ALIGNMENT_REQUIRED == 16
#define HASH_PTR_SHIFT 4
#else /* HOST_ALIGNMENT_REQUIRED > 16 */
#if HOST_ALIGNMENT_REQUIRED == 32
#define HASH_PTR_SHIFT 5
#else /* HOST_ALIGNMENT_REQUIRED > 32 */
#define HASH_PTR_SHIFT 6
#endif /* == 32 */
#endif /* == 16 */
#endif /* == 8 */
#endif /* == 4 */
#endif /* == 2 */
#endif /* == 1 */
  return (uintptr_t)ptr >> HASH_PTR_SHIFT;
#undef HASH_PTR_SHIFT
}  /* hash_ptr */


template<typename T> uintptr_t hash_ptr(T p)
/*
Generic version of hash_ptr for types that aren't native pointers.
*/
{
  return (uintptr_t)p;
}  /* hash_ptr */


template<typename T> uintptr_t hash_ptr(T *p)
/*
Version of hash_ptr for native pointers.  This assumes IL-aligned pointers.
*/
{
  return hash_ptr((void*)p);
}  /* hash_ptr */


/*lint -esym(758,Ptr_map_entry<*, *>::(anonymous))*/
template<typename a_Ptr_key, typename a_Value>
struct Ptr_map_entry {
  typedef a_Ptr_key a_key;
  typedef a_Value a_value;
  a_key		ptr;
			/* The pointer value mapped by this entry.  (A "key" in
			   the hash table.) */
  a_value	value;
			/* A value associated with ptr. */
  inline a_boolean key_set() const
    { return ptr != a_key(); }
};  /* Ptr_map_entry */


/*lint -esym(1510,*Ptr_map)*/
template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator = FE_allocator>
struct Ptr_map: private Allocator<Ptr_map_entry<a_Ptr_key, a_Value>> {
  /* A flat hash table whose keys are non-null scalar values (usually native
     pointers, but integers can be used too).  This implementation is optimized
     for lookups that generally succeed and small associated values (i.e., the
     key and value are kept together).  The allocator must allocate exactly the
     number of requested elements. */
  typedef a_Ptr_key a_key;
  typedef a_Value a_value;
  typedef Allocator<Ptr_map_entry<a_Ptr_key, a_Value>> an_allocator;
  typedef unsigned int an_index;
  typedef Ptr_map_entry<a_key, a_value> an_entry;
  inline Ptr_map(unsigned int mask_width, an_allocator a = an_allocator());
  inline ~Ptr_map();
  inline auto get_with_hash(a_key  key, uintptr_t  hash) const -> a_value;
  inline auto get(a_key  key) const -> a_value
    { return this->get_with_hash(key, hash_ptr(key)); }
  inline void map_with_hash(a_key  key, const a_value &value, uintptr_t  hash);
  inline void map(a_key  key, const a_value &value)
    { this->map_with_hash(key, value, hash_ptr(key)); }
  inline void replace_with_hash(a_key          key,
                                const a_value  &value,
                                uintptr_t      hash);
  inline void replace(a_key  key, const a_value  &value)
    { this->replace_with_hash(key, value, hash_ptr(key)); }
  inline auto map_or_replace_with_hash(a_key          key,
                                       const a_value  &value,
                                       uintptr_t      hash)
              -> a_value;
  inline auto map_or_replace(a_key  key, const a_value  &value) -> a_value
    { return this->map_or_replace_with_hash(key, value, hash_ptr(key)); }
  inline void unmap(a_key  key);
  inline auto number_of_elements() const -> an_index
    { return this->n_elements; }
#if DEBUG
  void db_ptrs() const;
#endif /* DEBUG */
  inline an_entry const *begin() const
    /*lint -e{1535}*/
    { return table; }
  inline an_entry const *end() const
    /*lint -e{1535}*/
    { return &table[hash_mask+1]; }
private:
  typedef typename an_allocator::an_allocation an_allocation;
  an_entry	*table;
			/* Pointer to the hash table. */
  an_index	hash_mask;
			/* The mask to apply to the hash value before indexing
			   in the table.  This mask is increased as the table
			   grows. */
  an_index	n_elements;
			/* The number of elements stored in the table. */
  void map_colliding_key(a_key          new_key,
                         const a_value  &new_value,
                         an_index       idx);
  void expand_table();
  void check_deleted_slot(an_index  idx0);
};  /* Ptr_map */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
inline Ptr_map<a_Ptr_key, a_Value, Allocator>::Ptr_map(
                                                     unsigned int  mask_width,
                                                     an_allocator  a)
/*
Initialize the given pointer map with a capacity for 1<<mask_width slots.
*/
  : an_allocator(a)
{
  unsigned       n_slots = (1<<mask_width);
  an_index       size = (an_index)(n_slots*sizeof(an_entry));
  an_allocation  allocation = this->alloc(n_slots);

  check_assertion(allocation.n_allocated == (a_ptrdiff)n_slots);
  this->table = allocation.start;
  memzero((char*)this->table, size_t_arg(size));
  this->hash_mask = n_slots-1;
  this->n_elements = 0;
}  /* Ptr_map::Ptr_map */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
inline Ptr_map<a_Ptr_key, a_Value, Allocator>::~Ptr_map()
/*
Release the storage for the map.
*/
{
  an_index  mask = this->hash_mask;
  an_index  n_slots = mask+1;

  for (an_index k = 0; k<n_slots; ++k) {
    if (table[k].ptr != a_key()) destroy(&table[k].value);
  }  /* for */
  this->dealloc(an_allocation{ this->table, (a_ptrdiff)n_slots });
  this->table = NULL;
}  /* Ptr_map::~Ptr_map */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
inline auto Ptr_map<a_Ptr_key, a_Value, Allocator>::get_with_hash(
                                                         a_key      key,
                                                         uintptr_t  hash) const
                                                    -> a_value
/*
Look up key in the map and return the associated value if found, or a_value()
if not found.  hash is the precomputed hash value for the key.
*/
{
  an_index   mask = this->hash_mask;
  an_index   idx = hash & mask;
  an_entry   *tbl = this->table;
  a_key      tptr;
  a_value    result = a_value();

  for (;;) {
    tptr = tbl[idx].ptr;
    if (tptr == key) {
      result = tbl[idx].value;
      break;
    } else if (tptr == a_key()) {
      break;
    }  /* if */
    idx = (idx+1) & mask;
  }  /* for */       
  return result;
}  /* Ptr_map::get_with_hash */


#ifdef TRACE_PTR_MAP
static void	*traced_key_ptr = NULL;
			/* Pointer that is checked for mapping activity.
			   Intended to be set from within a debugger and
			   watched by setting a breakpoint on function
			   ptr_map_intercept. */

inline void ptr_map_intercept(a_const_char  *msg)
/*
Function called when a map key equal to traced_key_ptr is entered into or
removed from a Ptr_map instance.
*/
{
  fprintf(f_debug, "\nMap activity for %p: %s\n", traced_key_ptr, msg);
}  /* ptr_map_intercept */

#define check_traced_key_ptr(ptr, msg)                                        \
  if ((a_byte*)(ptr) == traced_key_ptr) ptr_map_intercept(msg);

#else /* !defined(TRACE_PTR_MAP) */
#define check_traced_key_ptr(ptr, msg) /* Nothing */
#endif /* ifdef TRACE_PTR_MAP */

template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
inline void Ptr_map<a_Ptr_key, a_Value, Allocator>::map_with_hash(
                                                        a_key          key,
                                                        const a_value  &value,
                                                        uintptr_t      hash)
/*
Associate a copy of value with the given key.  hash is the precomputed hash
value of that key.
*/
{
  an_index   mask = this->hash_mask;
  an_index   idx = hash & mask;
  an_entry   *tbl = this->table;

  check_traced_key_ptr(key, "mapped");
  if (tbl[idx].ptr == a_key()) {
    tbl[idx].ptr = key;
    tbl[idx].value = value;
  } else {
    this->map_colliding_key(key, value, idx);
  }  /* if */
  this->n_elements += 1;
  if (this->n_elements*2 > mask) {
    this->expand_table();
  }  /* if */ 
}  /* Ptr_map::map_with_hash */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
inline void Ptr_map<a_Ptr_key, a_Value, Allocator>::replace_with_hash(
                                                        a_key          key,
                                                        const a_value  &value,
                                                        uintptr_t      hash)
/*
Replace the value associated with the given key by the given value.  hash is
the precomputed hash of that key.
*/
{
  an_index   mask = this->hash_mask;
  an_index   idx = hash & mask;
  an_entry   *tbl = this->table;
  a_key      ptr = tbl[idx].ptr;

  check_traced_key_ptr(key, "replaced");
  for (;;) {
    if (ptr == key) {
      tbl[idx].value = value;
      break;
    } else {
      idx = (idx+1) & mask;
      ptr = tbl[idx].ptr;
    }  /* if */
  }  /* for */                                                               
}  /* Ptr_map::replace_with_hash */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
inline auto Ptr_map<a_Ptr_key, a_Value, Allocator>::map_or_replace_with_hash(
                                                        a_key          key,
                                                        const a_value  &value,
                                                        uintptr_t      hash)
                                                    -> a_value
/*
If the given key is already mapped, replace its associated value by the given
value and return the previously associated value.  Otherwise, record a new key,
associate it with the given value, and return a_value().  hash is the
precomputed hash of that key.
*/
{
  an_index   mask = this->hash_mask;
  an_index   idx = hash & mask, idx0 = idx;
  an_entry   *tbl = this->table;
  a_key      ptr = tbl[idx].ptr;
  a_value    old_value = a_value();

  check_traced_key_ptr(key, "mapped or replaced");
  if (ptr == a_key()) {
    tbl[idx].ptr = key;
    tbl[idx].value = value;
    this->n_elements += 1;
    if (this->n_elements*2 > mask) {
      this->expand_table();
    }  /* if */
  } else {
    for (;;) {
      if (ptr == key) {
        old_value = tbl[idx].value;
        tbl[idx].value = value;
        break;
      } else {
        idx = (idx+1) & mask;
        ptr = tbl[idx].ptr;
        if (ptr == a_key()) {
          tbl[idx] = tbl[idx0];
          tbl[idx].ptr = key;
          tbl[idx].value = value;
          this->n_elements += 1;
          if (this->n_elements*2 > mask) {
            this->expand_table();
          }  /* if */
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */ 
  return old_value;
}  /* Ptr_map::map_or_replace_with_hash */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
inline void Ptr_map<a_Ptr_key, a_Value, Allocator>::unmap(a_key  key)
/*
Remove the given key from the table (it must exist).
*/
{
  uintptr_t  hash = hash_ptr(key);
  an_index   mask = this->hash_mask;
  an_index   idx = hash & mask;
  an_entry   *tbl = this->table;

  check_traced_key_ptr(key, "UNmapped");
  /* Find the item to delete (we're assuming it exists). */
  while (tbl[idx].ptr != key) {
    idx = (idx+1) & mask;
  }  /* while */
  /* Delete the entry. */
  tbl[idx].ptr = a_key();
  destroy(&tbl[idx].value);
  /* If the next slot is empty, we're done.  Otherwise, we may have to move
     another element into the emptied slot. */
  if (tbl[(idx+1) & mask].ptr != a_key()) {
    this->check_deleted_slot(idx);
  }  /* if */
  this->n_elements -= 1;                                                    
}  /* Ptr_map::unmap */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::map_colliding_key(
                                                    a_key          new_key,
                                                    const a_value  &new_value,
                                                    an_index       idx)
/*
The given key has a hash value that collides with an existing mapping.  Move
that existing mapping to the next free entry, and record the given new key and
new value at the given location.
*/
{
  an_index  idx0 = idx;
  an_index  mask = this->hash_mask;
  an_entry  *tbl = this->table;

#if EXPENSIVE_CHECKING
  { a_value  old_val = this->get(new_key);
    if (old_val != a_value()) {
      unexpected_condition_str("duplicate map key in Ptr_map");
    }  /* if */
  }
#endif /* EXPENSIVE_CHECKING */
  /* Move the existing mapping to the next available spot. */
  for (;;) {
    idx = (idx+1) & mask;
    if (tbl[idx].ptr == a_key()) {
      tbl[idx].ptr = tbl[idx0].ptr;
      tbl[idx].value = move_from(&tbl[idx0].value);
      break;
    }  /* if */
  }  /* for */
  /* Record the new mapping. */
  tbl[idx0].ptr = new_key;
  tbl[idx0].value = new_value;
}  /* Ptr_map::map_colliding_key */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::expand_table()
/*
Double the size of the hash table (and rehash entries as needed).
*/
{
  an_entry       *new_table, *old_table = this->table;
  an_index       mask = this->hash_mask;
  an_index       n_slots = mask+1;
  an_index       old_size = n_slots*(an_index)sizeof(an_entry);
  an_allocation  allocation = this->alloc(2*n_slots);

  check_assertion(allocation.n_allocated == 2*n_slots);
  new_table = allocation.start;
  memzero((char*)new_table, size_t_arg(2*old_size));
  mask = mask*2+1;
  for (an_index k = 0; k<n_slots; ++k) {
    a_key  ptr = old_table[k].ptr;
    if (ptr != a_key()) {
      an_index  idx = hash_ptr(ptr) & mask;
      while (new_table[idx].ptr != a_key()) {
        idx = (idx+1) & mask;
      }  /* while */
      new_table[idx] = old_table[k];
    }  /* if */
  }  /* for */
  this->table = new_table;
  this->hash_mask = mask;
  this->dealloc(an_allocation{ old_table, (a_ptrdiff)n_slots });
}  /* Ptr_map::expand_table */


template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::check_deleted_slot(an_index  idx0)
/*
Slot idx0 has been cleared (i.e., this->table[idx0].ptr has been set to null).
The next slot is not empty.  There may therefore exist entries that are
associated with that slot (i.e., have the same hash index).  This function
makes sure that such entries can be found, by moving up entries as needed.

This corresponds to Algorithm R in section 6.4 of volume 3 of Donald E. Knuth's
"The Art of Computer Programming" (Sorting and Searching -- Second Edition),
with the assumption that step R1 has already been performed (idx0 is "j") and
we know that the subsequent slot is not empty.
*/
{
  an_entry  *tbl = this->table;
  an_index  mask = this->hash_mask;
  an_index  idx, ridx;
  a_key     rptr;
  
  idx = (idx0+1) & mask;
  rptr = tbl[idx].ptr;
  for (;;) {
    for (;;) {
      ridx = hash_ptr(rptr) & mask;
      /* See if we can move the entry at idx to idx0.  ridx is its "ideal"
         slot: the place from where probing will start.  So we cannot move it
         ahead of there.  I.e., if idx0 lies outside [ridx, idx-1] (considering
         "wrap-around"), do not move the entry and try the next entry
         instead. */
      if ((ridx <= idx0 && idx0 < idx) ||
          (idx0 >= ridx && idx < ridx) ||
          (idx0 < idx && idx < ridx)) {
        /* idx0 is in [ridx, idx-1]: Move the entry. */
        break;
      } else {
        idx = (idx+1) & mask;
        rptr = tbl[idx].ptr;
        if (rptr == a_key()) goto done;
      }  /* if */
    }  /* for */
    tbl[idx0].ptr = tbl[idx].ptr;
    tbl[idx0].value = move_from(&tbl[idx].value);
    tbl[idx].ptr = a_key();
    idx0 = idx;
    idx = (idx0+1) & mask;
    rptr = tbl[idx].ptr;
    if (rptr == a_key()) goto done;
  }  /* for */
done:;
}  /* Ptr_map::check_deleted_slot */

#if DEBUG

template<typename a_Ptr_key, typename a_Value,
         template<typename> class Allocator>
void Ptr_map<a_Ptr_key, a_Value, Allocator>::db_ptrs() const
/*
Output some information about the map's key contents to f_debug.
*/
{
  an_entry  *tbl = this->table;
  an_index  mask = this->hash_mask;
  an_index  n_slots = mask+1;

  for (an_index k = 0; k<n_slots; ++k) {
    a_key  ptr = tbl[k].ptr;
    fprintf(f_debug, "[%2u] ", k);
    if (ptr == a_key()) {
      fprintf(f_debug, "(empty)\n");
    } else {
      fprintf(f_debug, "h = %2u  %p\n",
              (an_index)hash_ptr(ptr) & mask, (void*)ptr);
    }  /* if */
  }  /* for */
}  /* Ptr_map::db_ptrs */

#endif /* DEBUG */


/*
A structure that wraps a C-string to enable operations such as equality and
hashing to operate with string semantics as opposed to raw pointer semantics.
*/
struct a_C_str_handle {
  a_const_char
		*ptr = NULL;
			/* Pointer to the C-string. */
  a_C_str_handle() = default;
  a_C_str_handle(a_const_char *str) : ptr(str) {}
};


inline a_boolean operator==(const a_C_str_handle str1,
                            const a_C_str_handle str2)
/*
Return TRUE if the string contained by str1 is the same as the string contained
by str2.  If both handles contain NULL pointers then the "strings" are
considered the same.
*/
{
  a_boolean result;
  if (str1.ptr == NULL || str2.ptr == NULL) {
      result = (str1.ptr == str2.ptr);
  } else {
    result = (strcmp(str1.ptr, str2.ptr) == 0);
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const a_C_str_handle str1,
                            const a_C_str_handle str2)
/*
Return TRUE if the string contained by str1 is not the same as the string
contained by str2.  If both handles contain NULL pointers then the "strings"
are considered the same.
*/
{
  return !(str1 == str2);
}  /* operator!= */

#if !STANDALONE_UTILITY_PROGRAM

/*
A structure that wraps a path stored in a C-string to enable operations such
as equality and hashing to operate with path semantics as opposed to raw
pointer (or string) semantics.
*/
struct a_path_handle {
  a_const_char
		*ptr = NULL;
			/* Pointer to the path. */
  a_path_handle() = default;
  a_path_handle(a_const_char *path) : ptr(path) {}
};


inline a_boolean operator==(const a_path_handle path1,
                            const a_path_handle path2)
/*
Return TRUE if the path contained by path1 is the same as the path contained by
path2.  If both handles contain NULL pointers then the "paths" are considered
the same.
*/
{
  a_boolean result;
  if (path1.ptr == NULL || path2.ptr == NULL) {
    result = (path1.ptr == path2.ptr);
  } else {
    result = (compare_file_names_general(path1.ptr, path2.ptr) == 0);
  }  /* if */
  return result;
}  /* operator== */


inline a_boolean operator!=(const a_path_handle path1,
                            const a_path_handle path2)
/*
Return TRUE if the path contained by path1 is not the same as the path
contained by path2.  If both handles contain NULL pointers then the "paths" are
considered the same.
*/
{
  return !(path1 == path2);
}  /* operator!= */

#endif /* !STANDALONE_UTILITY_PROGRAM */

/*
This is declared in symbol_tbl.h.  To avoid potential circular references,
declare it here as well.
*/
extern a_hash_value hash_source_string(a_void_ptr  key);


inline uintptr_t hash_ptr(const a_C_str_handle str)
/*
Compute a hash for the given C-string.  The hash must be appropriate for
Ptr_map.
*/
{
  return (uintptr_t)hash_source_string((a_void_ptr)str.ptr);
}  /* hash_ptr */

#if !STANDALONE_UTILITY_PROGRAM

inline uintptr_t hash_ptr(const a_path_handle path)
/*
Compute a hash for the given path.  The hash must be appropriate for Ptr_map.
*/
{
  a_const_char *norm_path = normalize_file_name(path.ptr);

  return (uintptr_t)hash_source_string((a_void_ptr)norm_path);
}  /* hash_ptr */

#endif /* !STANDALONE_UTILITY_PROGRAM */

template<typename a_Linked_list_type, typename a_Predicate>
inline unsigned count_list_elements(a_Linked_list_type list_head,
                                    a_Predicate        predicate)
/*
Given the head of a linked list and a predicate, traverse the list and return
the number of elements in the list where the predicate returns TRUE.
*/
{
  unsigned count = 0;

  for (a_Linked_list_type el = list_head; el != NULL; el = el->next) {
    if (predicate(el)) {
      ++count;
    }  /* if */
  }  /* if */
  return count;
}  /* count_list_elements */


template<typename a_Linked_list_type>
inline unsigned count_list_elements(a_Linked_list_type list_head)
/*
Given the head of a linked list, traverse the list and return the number of
elements in the list.
*/
{
  auto always_true = [](const a_Linked_list_type &el) -> a_boolean {
    return TRUE;
  };

  return count_list_elements(list_head, always_true);
}  /* count_list_elements */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef EDG_UTIL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2022 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
