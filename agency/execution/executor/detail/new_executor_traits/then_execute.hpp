#pragma once

#include <agency/detail/config.hpp>
#include <agency/detail/requires.hpp>
#include <agency/detail/invoke.hpp>
#include <agency/execution/executor/detail/new_executor_traits/is_continuation_executor.hpp>
#include <agency/execution/executor/detail/new_executor_traits/is_bulk_continuation_executor.hpp>
#include <agency/execution/executor/detail/new_executor_traits/executor_shape.hpp>


namespace agency
{
namespace detail
{
namespace new_executor_traits_detail
{


// this case handles executors which have .then_execute()
__agency_exec_check_disable__
template<class E, class Function, class Future,
         __AGENCY_REQUIRES(ContinuationExecutor<E>())>
__AGENCY_ANNOTATION
executor_future_t<
  E,
  result_of_continuation_t<decay_t<Function>,Future>
>
then_execute(E& exec, Function&& f, Future& predecessor)
{
  return exec.then_execute(std::forward<Function>(f), predecessor);
}


namespace then_execute_detail
{


// this handles the general case when the predecessor future is non-void
template<class Function, class Predecessor>
struct functor
{
  mutable Function f;

  template<class Index, class Result>
  __AGENCY_ANNOTATION
  void operator()(const Index&, Predecessor& predecessor, Result& result, unit) const
  {
    result = invoke_and_return_unit_if_void_result(f, predecessor);
  }
};

// this specialization handles the case when the predecessor future is void
template<class Function>
struct functor<Function,void>
{
  mutable Function f;

  template<class Index, class Result>
  __AGENCY_ANNOTATION
  void operator()(const Index&, Result& result, unit) const
  {
    result = invoke_and_return_unit_if_void_result(f);
  }
};


} // end then_execute_detail


// this case handles executors which have .bulk_then_execute() but not .then_execute()
__agency_exec_check_disable__
template<class E, class Function, class Future,
         __AGENCY_REQUIRES(!ContinuationExecutor<E>()),
         __AGENCY_REQUIRES(BulkContinuationExecutor<E>())>
__AGENCY_ANNOTATION
executor_future_t<
  E,
  result_of_continuation_t<decay_t<Function>,Future>
>
then_execute(E& exec, Function f, Future& predecessor)
{
  using result_of_function = result_of_continuation_t<Function,Future>;
  using predecessor_type = future_value_t<Future>;

  // if f returns void, then return a unit from bulk_then_execute()
  using result_type = typename std::conditional<
    std::is_void<result_of_function>::value,
    unit,
    result_of_function
  >::type;

  // XXX should really move f into this functor, but it's not clear how to make move-only
  //     parameters to CUDA kernels
  auto execute_me = then_execute_detail::functor<Function,predecessor_type>{f};

  using shape_type = executor_shape_t<E>;

  // call .bulk_then_execute() to get an intermediate future
  auto intermediate_future = exec.bulk_then_execute(
    execute_me,                // the functor to execute
    shape_cast<shape_type>(1), // create only a single agent
    predecessor,               // the incoming argument to f
    construct<result_type>(),  // a factory for creating f's result
    unit_factory()             // a factory for creating a unit shared parameter which execute_me will ignore
  );

  // cast the intermediate future into the right type of future for the result
  return future_traits<decltype(intermediate_future)>::template cast<result_of_function>(intermediate_future);
}


// XXX introduce a case to handle executors which only have .async_execute() ?
// XXX introduce a worst case which uses predecessor.then() and ignores the executor entirely?

  
} // end new_executor_traits_detail
} // end detail
} // end agency

