#pragma once

#include <agency/detail/config.hpp>
#include <type_traits>

namespace agency
{
namespace detail
{
namespace allocator_traits_detail
{


template<class Alloc, class Pointer, class... Args>
struct has_construct_impl
{
  template<
    class Alloc1,
    class = decltype(
      std::declval<Alloc1*>()->construct(
        std::declval<Pointer>(),
        std::declval<Args>()...
      )
    )
  >
  static std::true_type test(int);

  template<class>
  static std::false_type test(...);

  using type = decltype(test<Alloc>(0));
};

template<class Alloc, class Pointer, class... Args>
using has_construct = typename has_construct_impl<Alloc,Pointer,Args...>::type;


template<class Alloc, class Iterator, class... Iterators>
struct has_construct_n_impl
{
  template<
    class Alloc1,
    class Result = decltype(
      std::declval<Alloc1&>().construct_n(
        std::declval<Iterator>(),
        std::declval<size_t>(),
        std::declval<Iterators>()...
      )
    )
    // XXX really ought to check that the result is convertible to tuple<Iterator,Iterators...>
    //, class = typename std::enable_if<
    //  std::is_convertible<...>::value
    //>::type
  >
  static std::true_type test(int);

  template<class>
  static std::false_type test(...);

  using type = decltype(test<Alloc>(0));
};

template<class Alloc, class Iterator, class... Iterators>
using has_construct_n = typename has_construct_n_impl<Alloc,Iterator,Iterators...>::type;


template<class Alloc, class Pointer>
struct has_destroy_impl
{
  template<
    class Alloc1,
    class = decltype(
      std::declval<Alloc1&>().destroy(
        std::declval<Pointer>()
      )
    )
  >
  static std::true_type test(int);

  template<class>
  static std::false_type test(...);

  using type = decltype(test<Alloc>(0));
};

template<class Alloc, class Pointer>
using has_destroy = typename has_destroy_impl<Alloc,Pointer>::type;


template<class Alloc>
struct has_max_size_impl
{
  template<
    class Alloc1,
    class = decltype(
      std::declval<Alloc1>().max_size()
    )
  >
  static std::true_type test(int);

  template<class>
  static std::false_type test(...);

  using type = decltype(test<Alloc>(0));
};

template<class Alloc>
using has_max_size = typename has_max_size_impl<Alloc>::type;


} // end allocator_traits_detail
} // end detail
} // end agency

