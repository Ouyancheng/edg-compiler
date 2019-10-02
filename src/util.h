/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2019 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

util.h -- General utility components (mostly templates).

*/

#ifndef EDG_UTIL_H
#define EDG_UTIL_H 1

#include <new>

#include "mem_manage.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

typedef uintptr_t a_uintptr;

typedef decltype(nullptr) a_nullptr;


/*
Enable_if<cond, T> is invalid (causing deduction failure) if cond is FALSE.
Otherwise, it produces T.
*/
template<a_boolean cond, typename a_Thing>
struct Enable_if_helper;

template<typename a_Thing>
struct Enable_if_helper<true, a_Thing> {
  typedef a_Thing a_thing;
};

template<a_boolean cond, typename a_Thing>
using Enable_if = typename Enable_if_helper<cond, a_Thing>::a_thing;


/*
Remove_ref<T> produces T if T is not a reference type, or the type underlying
the reference type otherwise.
*/
template<typename an_Object>
struct Remove_ref_helper {
  typedef an_Object an_object;
};

template<typename an_Object>
struct Remove_ref_helper<an_Object&> {
  typedef an_Object an_object;
};

template<typename an_Object>
struct Remove_ref_helper<an_Object&&> {
  typedef an_Object an_object;
};

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
Value_ptr<T> for a pointer-like type T produces the type pointed to.
*/
template<typename a_Ptr>
using Value_for_ptr = Remove_ref<decltype(*dummy_val<a_Ptr>())>;


template<typename an_Object>
an_Object&& forward(Remove_ref<an_Object>&  arg) noexcept
/*
This function should only be applied to "forwarding references".  It is used
to forward parameters.  For example:

    template<a_Thing> void f(a_Thing &&p) {
      g(forward<a_Thing>(p));
    }

If f is called with an rvalue, a_Thing will be deduced to a non-reference type
and forward<a_Thing>(p) will produce an xvalue.  If f is called with an lvalue,
a_Thing will be deduced to an lvalue reference type, and forward<a_Thing>(p)
will pass through the lvalue.
*/
{
  return (an_Object&&)arg;
}  /* forward */


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
  ::new((void*)p_object) an_object(forward<an_Arg_pack>(args)...);
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
  typedef Value_for_ptr<a_Ptr> a_value;
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
};

template<typename a_Ptr>
inline Ptr_with_flag<a_Ptr> ptr_with_flag(a_Ptr      ptr,
                                          a_boolean  flag)
/*
Return a Ptr_with_flag initialized with the given values.
*/
{
  return Ptr_with_flag<a_Ptr>{ ptr, flag };
}  /* ptr_with_flag  */


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
  a_ptrdiff const
		n_allocated;
			/* Number of allocated elements. */
};


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
};


template<typename an_Elem>
inline auto FE_allocator<an_Elem>::alloc(a_size n) -> an_allocation
/*
Allocate at least n elements of type an_Elem and return the resulting
allocation (which reflects the actual number of allocated elements.
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
  free_fe(old_start, a.n_allocated*sizeof(an_elem));
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
  free_fe(a.start, a.n_allocated*sizeof(an_elem));
}  /* FE_allocator::dealloc */


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
};


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
  construct(p, forward<an_Arg_pack>(args)...);
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


template<typename an_Elem, typename an_Allocator = FE_allocator<an_Elem>>
struct Dyn_array: private an_Allocator {
  /* A dynamically growable array-like class type. */
  typedef an_Elem an_elem;
  typedef an_Allocator an_allocator;
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
  inline auto operator=(Dyn_array const&) -> Dyn_array&;
  inline auto operator=(Dyn_array&&) -> Dyn_array&;
  inline auto operator[](an_index i) -> an_elem&
    { return this->elems[i]; }
  inline auto operator[](an_index i) const -> const an_elem& 
    { return this->elems[i]; }
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
  inline void remove(an_index i);
  void reserve(a_size);
  void shrink_wrap();
  /* Interfaces to allow range-based for loop. */
  inline auto begin() -> an_Elem*
    { return this->elems; }
  inline auto begin() const -> an_Elem const*
    { return this->elems; }
  inline auto end() -> an_Elem*
    { return this->elems+this->n_elems; }
  inline auto end() const -> an_Elem const*
    { return this->elems+this->n_elems; }
private:
  an_elem	*elems;
			/* Pointer to the allocated elements. */
  a_size	n_allocated;
			/* Number of elements allocated.  This is also known
			   as the "capacity". */
  a_size	n_elems;
			/* Number of initialized elements.  This is also known
			   as the "length". */
  typedef typename an_allocator::an_allocation an_allocation;
  void grow();
};  /* Dyn_array */


template<typename an_Elem, typename an_Allocator>
inline Dyn_array<an_Elem, an_Allocator>::Dyn_array(a_size       cap,
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
}  /* Dyn_array */


template<typename an_Elem, typename an_Allocator>
inline Dyn_array<an_Elem, an_Allocator>::Dyn_array(a_size         cap,
                                                   const an_elem& v,
                                                   an_allocator   a)
/*
Initialize a Dyn_array with a minimum of cap elements whose allocation is
managed by the given allocator.  Initialize the first cap element to v.
*/
  : an_allocator(a)
  , elems()
  , n_allocated()
  , n_elems(0)
{
  an_allocation  allocation = this->alloc(cap);
  this->elems = allocation.start;
  this->n_allocated = (a_size)allocation.n_allocated;
  for (a_size k = 0; k<cap; ++k) {
    construct(this->elems+k, v);
  }  /* for */
}  /* Dyn_array */


template<typename an_Elem, typename an_Allocator>
inline Dyn_array<an_Elem, an_Allocator>::Dyn_array(const Dyn_array&  src)
/*
Copy constructor.
*/
  : an_allocator(src)
  , elems()
  , n_allocated()
  , n_elems(src.n_elems)
{
  /* ALlocate new storage. */
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


template<typename an_Elem, typename an_Allocator>
inline Dyn_array<an_Elem, an_Allocator>::Dyn_array(Dyn_array&&  src)
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


template<typename an_Elem, typename an_Allocator>
inline Dyn_array<an_Elem, an_Allocator>::~Dyn_array()
/*
Destructor.
*/
{
  an_elem  *elems = this->elems;
  a_size   n = this->n_elems;

  for (a_size k = 0; k < n; ++k) {
    destroy(elems+k);
  }  /* for */
  this->dealloc(an_allocation{ this->elems, this->n_allocated });
}  /* Dyn_array::Dyn_array */


template<typename an_Elem, typename an_Allocator>
inline auto Dyn_array<an_Elem, an_Allocator>::operator=(Dyn_array const &b)
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


template<typename an_Elem, typename an_Allocator>
inline auto Dyn_array<an_Elem, an_Allocator>::operator=(Dyn_array &&b)
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


template<typename an_Elem, typename an_Allocator>
inline void Dyn_array<an_Elem, an_Allocator>::push_back(const an_elem &value)
/*
Copy the given value into the position after the currently-last element.
Allocate new storage if needed.
*/
{
  a_size  n_elems = this->n_elems;

  if (n_elems == this->n_allocated) {
    this->grow();
    n_elems = this->n_elems;
  }  /* if */
  construct(this->elems+n_elems, value);
  this->n_elems = n_elems+1;
}  /* Dyn_array::push_back */


template<typename an_Elem, typename an_Allocator>
inline void Dyn_array<an_Elem, an_Allocator>::push_back(an_elem &&value)
/*
Move the given value into the position after the currently-last element.
Allocate new storage if needed.
*/
{
  a_size  n_elems = this->n_elems;

  if (n_elems == this->n_allocated) {
    this->grow();
  }  /* if */
  construct(this->elems+n_elems, move_from(&value));
  this->n_elems = n_elems+1;
}  /* Dyn_array::push_back */


template<typename an_Elem, typename an_Allocator>
inline void Dyn_array<an_Elem, an_Allocator>::insert(an_index      i,
                                                     const an_elem &value)
/*
Copy-insert the given value at the given index.  All subsequent values (if any)
are first moved one position up.
*/
{
  a_size  n_elems = this->n_elems;

  if (n_elems == this->n_allocated) {
    this->grow();
  }  /* if */
  an_elem  *elems = this->elems;
  for (an_index k = n_elems; k>i; --k) {
    construct(elems+k, move_from(elems+k-1));
    destroy(elems+k-1);
  }  /* for */
  construct(elems+i, value);
  this->n_elems = n_elems+1;
}  /* Dyn_array::insert */


template<typename an_Elem, typename an_Allocator>
inline void Dyn_array<an_Elem, an_Allocator>::insert(an_index  i,
                                                     an_elem   &&value)
/*
Move-insert the given value at the given index.  All subsequent values (if any)
are first moved one position up.
*/
{
  a_size  n_elems = this->n_elems;

  if (n_elems == this->n_allocated) {
    this->grow();
  }  /* if */
  an_elem  *elems = this->elems;
  for (an_index k = n_elems; k>i; --k) {
    construct(elems+k, move_from(elems+k-1));
    destroy(elems+k-1);
  }  /* for */
  construct(elems+i, move_from(&value));
  this->n_elems = n_elems+1;
}  /* Dyn_array::insert */


template<typename an_Elem, typename an_Allocator>
inline void Dyn_array<an_Elem, an_Allocator>::remove(an_index  i)
/*
Destroy the entry at the given index.  All subsequent values (if any) are moved
one position down.
*/
{
  an_elem  *elems = this->elems;

  destroy(elems+i);
  a_size  n_elems = --this->n_elems;
  for (an_index k = i; k<n_elems; ++k) {
    construct(elems+k, move_from(elems+k+1));
    destroy(elems+k+1);
  }  /* for */
}  /* Dyn_array::remove */


template<typename an_Elem, typename an_Allocator>
void Dyn_array<an_Elem, an_Allocator>::reserve(a_size  new_cap)
/*
Increase the capacity to the given value (which must be larger than the
current capacity.
*/
{
  a_size  old_cap = this->n_allocated;
  check_assertion(new_cap > old_cap);
  an_allocation  a = this->realloc(an_allocation{ this->elems, old_cap },
                                   new_cap, this->n_elems);
  this->elems = a.start;
  this->n_allocated = (a_size)a.n_allocated;
}  /* Dyn_array::reserve */


template<typename an_Elem, typename an_Allocator>
void Dyn_array<an_Elem, an_Allocator>::grow()
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


template<typename an_Object, typename a_Deallocator = FE_allocator<an_Object>>
struct Owning_ptr: private a_Deallocator {
  /* A smart pointer managing an object it owns. */
  typedef an_Object an_object;
  typedef a_Deallocator a_deallocator;
  inline Owning_ptr()
    : ptr(NULL) {}
  inline Owning_ptr(an_object *ptr, const a_deallocator &d = a_deallocator())
    : a_deallocator(d), ptr(ptr) {}
  inline Owning_ptr(a_nullptr, const a_deallocator &d = a_deallocator())
    : a_deallocator(d), ptr(NULL) {}
  inline Owning_ptr(const Owning_ptr&) = delete;
  inline Owning_ptr(Owning_ptr&& src)
    : a_deallocator(move_from(&src)), ptr(src.ptr) { src.ptr = NULL; }
  inline ~Owning_ptr();
  inline auto operator=(const Owning_ptr&) -> Owning_ptr& = delete;
  inline auto operator=(Owning_ptr&& src) -> Owning_ptr&
    { this->ptr = src.ptr; src.ptr = NULL; return *this; }
  inline auto operator=(a_nullptr) -> Owning_ptr&;
  inline auto operator->() const -> an_object*
    { return this->ptr; }
  inline auto operator*() const -> an_object&
    { return *this->ptr; }
private:
  an_object	*ptr;	/* Pointer to the owned object. */
};


template<typename an_Object, typename a_Deallocator>
inline Owning_ptr<an_Object, a_Deallocator>::~Owning_ptr()
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


template<typename an_Object, typename a_Deallocator>
inline auto Owning_ptr<an_Object, a_Deallocator>::operator=(a_nullptr)
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
  construct(p, forward<an_Arg_pack>(args)...);
  return Owning_ptr<an_Object>(p);
}  /* owning_ptr */




template<typename an_Object>
inline an_Object min(an_Object const &x,
                     an_Object const &y)
/*
Return the smaller of the given values.  If the values are equal, return the
first one.
*/
{
  return y < x ? y : x;
}  /* min */


template<typename an_Object>
inline an_Object max(an_Object const &x,
                     an_Object const &y)
/*
Return the larger of the given values.  If the values are equal, return the
first one.
*/
{
  return x < y ? y : x;
}  /* max */


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
FIXME: Consider __builtin_clz.
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
FIXME: Consider __builtin_popcount.
*/
{
  int r = 0;

  while (n != 0) {
    if (n&1) {
      ++r;
    }  /* if */
    n >>= 1;
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
number of comparisons (2 or 3) and moves (0, 3, or 4).
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
          // b <= c <= a, b < d <= a, 
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
[first-1, last).  If moves_limit is nonzero and move than moves_limit moves
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
an no more than 248 (it must fit in a byte).
*/
#define BLOCK_SIZE ((a_byte)64)


template<typename an_Object>
inline an_Object* align_to_cache_line(an_Object *p)
/*
Return p minimally advanced to be aligned to a cache line.
FIXME: Should move to namespace edg after CACHE_LINE_SIZE is moved to
       host_envir.h.
*/
{
  return (an_Object*)
             (((a_uintptr)p+CACHE_LINE_SIZE-1) & (a_uintptr)-CACHE_LINE_SIZE);
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

  /* Find the first element greater than or equal than the pivot (the median
     of 3 guarantees this exists). */
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
     How Branch Mispredictions don’t affect Quicksort" by Stefan Edelkamp
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
    int num = min(num_l, num_r);
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

  int num = min(num_l, num_r);
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
Elements equal to the pivot are put in the right-hand partition. Return the
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

  /* Find the first element greater than or equal than the pivot (the median
     of 3 guarantees this exists). */
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
       *(begin-1). Then if our pivot compares equal to *(begin-1) we change
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
// FIXME: CACHE_LINE_SIZE should move to host_envir.h (and not be undef'd)
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


inline a_uintptr hash_ptr(void  *ptr)
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
  return (a_uintptr)ptr >> HASH_PTR_SHIFT;
#undef HASH_PTR_SHIFT
}  /* hash_ptr */


template<typename T> a_uintptr hash_ptr(T p)
/*
Generic version of hash_ptr for types that aren't native pointers.
*/
{
  return (a_uintptr)p;
}  /* hash_ptr */


template<typename T> a_uintptr hash_ptr(T *p)
/*
Version of hash_ptr for native pointers.  This assumes IL-aligned pointers.
*/
{
  return hash_ptr((void*)p);
}  /* hash_ptr */


#define MAX_WIDTH_REUSABLE_PTR_MAP_TABLE 10
template<int  entry_size>
struct Free_ptr_map_tables {
  static void	*list[MAX_WIDTH_REUSABLE_PTR_MAP_TABLE+1];
			/* A array of pointers to map tables available for
			   reuse.  list[n] points to a list of tables allocated
			   for 1<<n entries of size entry_size.  Larger tables
			   use alloc_general and free_general. */
};

template<int  entry_size>
void *Free_ptr_map_tables<entry_size>::list[
                                           MAX_WIDTH_REUSABLE_PTR_MAP_TABLE+1];


template<typename a_Ptr_key, typename a_Value>
struct Ptr_map_entry {
  typedef a_Ptr_key a_key;
  typedef a_Value a_value;
  union {
    a_key	ptr;
			/* The pointer value mapped by this entry.  (A "key" in
			   the hash table.) */
    void	*next;
			/* Pointer to the next free block when this is the
			   leading entry in a table currently available for
			   reuse. */
  };
  a_value	value;
			/* A value associated with ptr. */
};


// FIXME: Parameterize the hash function.
template<typename a_Ptr_key, typename a_Value>
struct Ptr_map {
  /* A flat hash table whose keys are non-null scalar values (usually native
     pointers, but integers can be used too).  This
     implementation is optimized for lookups that generally succeed and
     small associated values (i.e., the key and value are kept together). */
  typedef a_Ptr_key a_key;
  typedef a_Value a_value;
  typedef unsigned int an_index;
  inline Ptr_map(unsigned int mask_width);
  inline ~Ptr_map();
  inline auto get(a_key  key) const -> a_value;
  inline void map(a_key  key, a_value const &value);
  inline void replace(a_key  key, a_value const &value);
  inline auto map_or_replace(a_key  key, a_value const &value) -> a_value;
  inline void unmap(a_key  key);
#if DEBUG
  void db_ptrs() const;
#endif /* DEBUG */
private:
  typedef Ptr_map_entry<a_key, a_value> an_entry;
  an_entry	*table;
			/* Pointer to the hash table. */
  an_index	hash_mask;
			/* The mask to apply to the hash value before indexing
			   in the table.  This mask is increased as the table
			   grows. */
  an_index	n_elements;
			/* The number of elements stored in the table. */
  void map_colliding_key(a_key          new_key,
                         a_value const  &new_value,
                         an_index       idx);
  void expand_table();
  void check_deleted_slot(an_index  idx0);
};


template<typename a_Ptr_key, typename a_Value>
inline Ptr_map<a_Ptr_key, a_Value>::Ptr_map(unsigned int mask_width)
/*
Initialize the given pointer map with a capacity for 1<<mask_width slots.
*/
{
  unsigned  n_slots = (1<<mask_width);
  an_index  size = (an_index)(n_slots*sizeof(an_entry));

  if (mask_width > MAX_WIDTH_REUSABLE_PTR_MAP_TABLE) {
    this->table = (an_entry*)alloc_general(size);
  } else {
    typedef Free_ptr_map_tables<(int)sizeof(an_entry)>  a_free_table_cache;
    if (a_free_table_cache::list[mask_width] != NULL) {
      this->table = (an_entry*)a_free_table_cache::list[mask_width];
      a_free_table_cache::list[mask_width] = this->table->next;
    } else {
      this->table = (an_entry*)alloc_general(size);
    }  /* if */
  }  /* if */
  memzero((char*)this->table, size_t_arg(size));
  this->hash_mask = n_slots-1;
  this->n_elements = 0;
}  /* Ptr_map::Ptr_map */


template<typename a_Ptr_key, typename a_Value>
inline Ptr_map<a_Ptr_key, a_Value>::~Ptr_map()
/*
Release the storage for the map.
*/
{
  an_index  mask = this->hash_mask;
  an_index  n_slots = mask+1;
  an_index  size = (an_index)(n_slots*sizeof(an_entry));
  an_index  mask_width = count_ones(mask);

  for (an_index k = 0; k<n_slots; ++k) {
    if (table[k].ptr != a_key()) destroy(&table[k].value);
  }  /* for */
  if (mask_width > MAX_WIDTH_REUSABLE_PTR_MAP_TABLE) {
    free_general(this->table, size);
  } else {
    typedef Free_ptr_map_tables<(int)sizeof(an_entry)>  a_free_table_cache;
    this->table[0].next = a_free_table_cache::list[mask_width];
    a_free_table_cache::list[mask_width] = this->table;
  }  /* if */
}  /* Ptr_map::~Ptr_map */


template<typename a_Ptr_key, typename a_Value>
inline auto Ptr_map<a_Ptr_key, a_Value>::get(a_key  key) const -> a_value
/*
Look up key in the map and return the associated value if found, or a_value()
if not found.
*/
{
  a_uintptr  hash = hash_ptr(key);
  an_index   mask = this->hash_mask;
  an_index   idx = hash & mask;
  an_entry   *table = this->table;
  a_key      tptr;
  a_value    result = a_value();

  for (;;) {
    tptr = table[idx].ptr;
    if (tptr == key) {
      result = table[idx].value;
      break;
    } else if (tptr == a_key()) {
      break;
    }  /* if */
    idx = (idx+1) & mask;
  }  /* for */       
  return result;
}  /* Ptr_map::get */

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

#else /* TRACE_PTR_MAP  */
#define check_traced_key_ptr(ptr, msg) /* Nothing */
#endif /* TRACE_PTR_MAP */

template<typename a_Ptr_key, typename a_Value>
inline void Ptr_map<a_Ptr_key, a_Value>::map(a_key          key,
                                             a_value const  &value)
/*
Associate a copy of value with the given key.
*/
{
  a_uintptr  hash = hash_ptr(key);
  an_index   mask = this->hash_mask;
  an_index   idx = hash & mask;
  an_entry   *table = this->table;

  check_traced_key_ptr(key, "mapped");
  if (table[idx].ptr == a_key()) {
    table[idx].ptr = key;
    table[idx].value = value;
  } else {
    this->map_colliding_key(key, value, idx);
  }  /* if */
  this->n_elements += 1;
  if (this->n_elements*2 > mask) {
    this->expand_table();
  }  /* if */ 
}  /* Ptr_map::map */


template<typename a_Ptr_key, typename a_Value>
inline void Ptr_map<a_Ptr_key, a_Value>::replace(a_key          key,
                                                 a_value const  &value)
/*
Replace the value associated with the given key by the given value.
*/
{
  a_uintptr  hash = hash_ptr(key);
  an_index   mask = this->hash_mask;
  an_index   idx = hash & mask;
  an_entry   *table = this->table;
  a_key      ptr = table[idx].ptr;

  check_traced_key_ptr(key, "replaced");
  for (;;) {
    if (ptr == key) {
      table[idx].value = value;
      break;
    } else {
      idx = (idx+1) & mask;
      ptr = table[idx].ptr;
    }  /* if */
  }  /* for */                                                               
}  /* Ptr_map::replace */


template<typename a_Ptr_key, typename a_Value>
inline auto Ptr_map<a_Ptr_key, a_Value>::map_or_replace(a_key          key,
                                                        a_value const  &value)
                                         -> a_value
/*
If the given key is already mapped, replace its associated value by the given
value and return the previously associated value.  Otherwise, record a new key,
associate it with the given value, an return a_value().
*/
{
  a_uintptr  hash = hash_ptr(key);
  an_index   mask = this->hash_mask;
  an_index   idx = hash & mask, idx0 = idx;
  an_entry   *table = this->table;
  a_key      ptr = table[idx].ptr;
  a_value    old_value = a_value();

  check_traced_key_ptr(key, "mapped or replaced");
  if (ptr == a_key()) {
    table[idx].ptr = key;
    table[idx].value = value;
    this->n_elements += 1;
    if (this->n_elements*2 > mask) {
      this->expand_table();
    }  /* if */
  } else {
    for (;;) {
      if (ptr == key) {
        old_value = table[idx].value;
        table[idx].value = value;
        break;
      } else {
        idx = (idx+1) & mask;
        ptr = table[idx].ptr;
        if (ptr == a_key()) {
          table[idx] = table[idx0];
          table[idx].ptr = key;
          table[idx].value = value;
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
}  /* Ptr_map::map_or_replace */


template<typename a_Ptr_key, typename a_Value>
inline void Ptr_map<a_Ptr_key, a_Value>::unmap(a_key  key)
/*
Remove the given key from the table (it must exist).
*/
{
  a_uintptr  hash = hash_ptr(key);
  an_index   mask = this->hash_mask;
  an_index   idx = hash & mask;
  an_entry   *table = this->table;

  check_traced_key_ptr(key, "UNmapped");
  /* Find the item to delete (we're assuming it exists). */
  while (table[idx].ptr != key) {
    idx = (idx+1) & mask;
  }  /* while */
  /* Delete the entry. */
  table[idx].ptr = a_key();
  destroy(&table[idx].value);
  /* If the next slot is empty, we're done.  Otherwise, we may have to */
  /* move another element into the emptied slot. */
  if (table[(idx+1) & mask].ptr != a_key()) {
    this->check_deleted_slot(idx);
  }  /* if */
  this->n_elements -= 1;                                                    
}  /* Ptr_map::unmap */


template<typename a_Ptr_key, typename a_Value>
void Ptr_map<a_Ptr_key, a_Value>::map_colliding_key(a_key          new_key,
                                                    a_value const  &new_value,
                                                    an_index       idx)
/*
The given key has a hash value that collides with an existing mapping.  Move
that existing mapping to the next free entry, and record the given new key and
new value at the given location.
*/
{
  an_index  idx0 = idx;
  an_index  mask = this->hash_mask;
  an_entry  *table = this->table;

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
    if (table[idx].ptr == a_key()) {
      table[idx].ptr = table[idx0].ptr;
      table[idx].value = move_from(&table[idx0].value);
      break;
    }  /* if */
  }  /* for */
  /* Record the new mapping. */
  table[idx0].ptr = new_key;
  table[idx0].value = new_value;
}  /* Ptr_map::map_colliding_key */


template<typename a_Ptr_key, typename a_Value>
void Ptr_map<a_Ptr_key, a_Value>::expand_table()
/*
Double the size of the hash table (and rehash entries as needed).
*/
{
  an_entry  *new_table, *old_table = this->table;
  an_index  mask = this->hash_mask;
  an_index  n_slots = mask+1;
  an_index  old_size = n_slots*(an_index)sizeof(an_entry);
  an_index  new_size = 2*old_size;
  int       new_width = count_ones(mask)+1, old_width;

  typedef Free_ptr_map_tables<(int)sizeof(an_entry)>  a_free_table_cache;
  if (new_width > MAX_WIDTH_REUSABLE_PTR_MAP_TABLE) {
    new_table = (an_entry*)alloc_general(new_size);
  } else if (a_free_table_cache::list[new_width] != NULL) {
    new_table = (an_entry*)a_free_table_cache::list[new_width];
    a_free_table_cache::list[new_width] =
                       ((an_entry*)a_free_table_cache::list[new_width])->next;
  } else {
    new_table = (an_entry*)alloc_general(new_size);
  }  /* if */
  memzero((char*)new_table, size_t_arg(new_size));
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
  old_width = new_width-1;
  if (old_width > MAX_WIDTH_REUSABLE_PTR_MAP_TABLE) {
    free_general(old_table, old_size);
  } else {
    old_table[0].next = a_free_table_cache::list[old_width];
    a_free_table_cache::list[old_width] = old_table;
  }  /* if */
}  /* Ptr_map::expand_table */


template<typename a_Ptr_key, typename a_Value>
void Ptr_map<a_Ptr_key, a_Value>::check_deleted_slot(an_index  idx0)
/*
Slot idx has been cleared (i.e., this->table[idx].ptr has been set to null).
The next slot is not empty.  There may therefore exist entries that are
associated with that slot (i.e., have the same hash index).  This function
makes sure that such entries can be found, by moving up entries as needed.

This corresponds to Algorithm R in section 6.4 of volume 3 of Donald E. Knuth's
"The Art of Computer Programming" (Sorting and Searching -- Second Edition),
with the assumption that step R1 has already been performed (idx0 is "j") and
we know that the subsequent slot is not empty.
*/
{
  an_entry  *table = this->table;
  an_index  mask = this->hash_mask;
  an_index  idx, ridx;
  a_key     rptr;
  
  idx = (idx0+1) & mask;
  rptr = table[idx].ptr;
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
        rptr = table[idx].ptr;
        if (rptr == a_key()) goto done;
      }  /* if */
    }  /* for */
    table[idx0].ptr = table[idx].ptr;
    table[idx0].value = move_from(&table[idx].value);
    table[idx].ptr = a_key();
    idx0 = idx;
    idx = (idx0+1) & mask;
    rptr = table[idx].ptr;
    if (rptr == a_key()) goto done;
  }  /* for */
done:;

}  /* Ptr_map::check_deleted_slot */

#if DEBUG

template<typename a_Ptr_key, typename a_Value>
void Ptr_map<a_Ptr_key, a_Value>::db_ptrs() const
/*
Output some information about the map's key contents to f_debug.
*/
{
  an_entry  *table = this->table;
  an_index  mask = this->hash_mask;
  an_index  n_slots = mask+1;

  for (an_index k = 0; k<n_slots; ++k) {
    a_key  ptr = table[k].ptr;
    fprintf(f_debug, "[%2u] ", k);
    if (ptr == a_key()) {
      fprintf(f_debug, "(empty)\n");
    } else {
      fprintf(f_debug, "h = %2u  %p\n",
              (an_index)hash_ptr(ptr) & mask, ptr);
    }  /* if */
  }  /* for */
}  /* Ptr_map::db_ptrs */

#endif /* DEBUG */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef EDG_UTIL_H */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 1988-2018 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
