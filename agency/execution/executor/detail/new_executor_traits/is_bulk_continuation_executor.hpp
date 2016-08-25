#pragma once

#include <agency/detail/config.hpp>
#include <agency/detail/type_traits.hpp>
#include <agency/detail/integer_sequence.hpp>
#include <agency/execution/executor/executor_traits.hpp>
#include <agency/execution/executor/detail/new_executor_traits/executor_execution_depth_or.hpp>
#include <agency/execution/executor/detail/new_executor_traits/executor_shape_or.hpp>
#include <agency/execution/executor/detail/new_executor_traits/executor_future_or.hpp>
#include <agency/execution/executor/detail/new_executor_traits/executor_index_or.hpp>
#include <future>
#include <type_traits>
#include <utility>

namespace agency
{
namespace detail
{
namespace new_executor_traits_detail
{


template<class Executor, class Function, class Shape,
         class Future,
         class ResultFactory,
         class... SharedFactories
        >
struct has_bulk_then_execute_impl
{
  using expected_return_type = executor_future_or_t<Executor,result_of_t<ResultFactory(Shape)>>;

  template<class Executor1,
           class ReturnType = decltype(
             std::declval<Executor1>().bulk_then_execute(
               std::declval<Function>(),
               std::declval<Shape>(),
               std::declval<Future&>(),
               std::declval<ResultFactory>(),
               std::declval<SharedFactories>()...
             )
           ),
           class = typename std::enable_if<
             std::is_same<ReturnType,expected_return_type>::value
           >::type>
  static std::true_type test(int);

  template<class>
  static std::false_type test(...);

  using type = decltype(test<Executor>(0));
};


template<class Executor, class Function, class Shape,
         class Future,
         class ResultFactory,
         class... SharedFactories
        >
using has_bulk_then_execute = typename has_bulk_then_execute_impl<Executor, Function, Shape, Future, ResultFactory, SharedFactories...>::type;


template<class T, class IndexSequence>
struct is_bulk_continuation_executor_impl;

template<class T, size_t... Indices>
struct is_bulk_continuation_executor_impl<T, index_sequence<Indices...>>
{
  // executor properties
  using shape_type = executor_shape_or_t<T>;
  using index_type = executor_index_or_t<T>;

  // types related to functions passed to .bulk_async_execute()
  using result_type = int;
  using predecessor_type = int;
  using predecessor_future_type = std::future<predecessor_type>;
  template<size_t>
  using shared_type = int;

  // the functions we'll pass to .bulk_then_execute() to test
  using test_function = std::function<void(index_type, predecessor_type&, result_type&, shared_type<Indices>&...)>;
  using test_result_factory = std::function<result_type(shape_type)>;

  // XXX we're passing the wrong shape_type -- we need to pick out the Ith shape_type
  template<size_t I>
  using test_shared_factory = std::function<shared_type<I>(shape_type)>;

  using type = has_bulk_then_execute<
    T,
    test_function,
    shape_type,
    predecessor_future_type,
    test_result_factory,
    test_shared_factory<Indices>...
  >;
};

template<class T>
using is_bulk_continuation_executor = typename is_bulk_continuation_executor_impl<
  T,
  make_index_sequence<
    executor_execution_depth_or<T>::value
  >
>::type;


} // end new_executor_traits_detail
} // end detail
} // end agency

