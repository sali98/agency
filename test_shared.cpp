#include <agency/executor_traits.hpp>
#include <agency/execution_agent.hpp>
#include <agency/detail/tuple.hpp>
#include <agency/detail/index_cast.hpp>
#include <agency/detail/shape_cast.hpp>
#include <agency/execution_policy.hpp>
#include <agency/detail/shared_parameter.hpp>
#include <agency/detail/is_call_possible.hpp>
#include <iostream>
#include <cassert>
#include <functional>


template<size_t level, class T, class... Args>
agency::detail::shared_parameter<level, T,Args...> share(Args&&... args)
{
  return agency::detail::shared_parameter<level, T,Args...>{agency::detail::make_tuple(std::forward<Args>(args)...)};
}

template<size_t level, class T>
agency::detail::shared_parameter<level,T,T> share(const T& val)
{
  return agency::detail::shared_parameter<level,T,T>{agency::detail::make_tuple(val)};
}


template<class T>
struct decay_parameter : std::decay<T> {};


template<size_t level, class T, class... Args>
struct decay_parameter<
  agency::detail::shared_parameter<level, T, Args...>
>
{
  // shared parameters are passed by reference
  using type = T&;
};


template<class T>
using decay_parameter_t = typename decay_parameter<T>::type;


template<class Executor, class Function, class... Args>
typename agency::detail::enable_if_call_possible<
  Function(
    typename agency::executor_traits<Executor>::index_type,
    decay_parameter_t<Args>...
  )
>::type
  bulk_invoke_executor(Executor& exec, Function f, typename agency::executor_traits<typename std::decay<Executor>::type>::shape_type shape, Args&&... args)
{
  // the _1 is for the executor idx parameter, which is the first parameter passed to f
  auto g = bind_unshared_parameters(f, std::placeholders::_1, std::forward<Args>(args)...);

  // make a tuple of the shared args
  auto shared_arg_tuple = forward_shared_parameters_as_tuple(std::forward<Args>(args)...);

  using traits = agency::executor_traits<Executor>;

  // package up the shared parameters for the executor
  const size_t executor_depth = agency::detail::execution_depth<
    typename traits::execution_category
  >::value;

  // construct shared arguments and package them for the executor
  auto shared_init = agency::detail::pack_shared_parameters_for_executor<executor_depth>(shared_arg_tuple);

  using shared_param_type = typename traits::template shared_param_type<decltype(shared_init)>;

  traits::bulk_invoke(exec, [=](typename traits::index_type idx, shared_param_type& packaged_shared_params)
  {
    auto shared_params = agency::detail::unpack_shared_parameters_from_executor(packaged_shared_params);

    // XXX the following is the moral equivalent of:
    // g(idx, shared_params...);

    // create one big tuple of the arguments so we can just call tuple_apply
    auto idx_and_shared_params = __tu::tuple_prepend_invoke(shared_params, idx, agency::detail::forwarder{});

    __tu::tuple_apply(g, idx_and_shared_params);
  },
  shape,
  std::move(shared_init));
}


size_t rank(agency::size2 shape, agency::size2 idx)
{
  return agency::get<1>(shape) * agency::get<0>(idx) + agency::get<1>(idx);
}

size_t rank(agency::size3 shape, agency::size3 idx)
{
  agency::size2 idx2{agency::get<0>(idx), agency::get<1>(idx)};
  agency::size2 shape2{agency::get<0>(shape), agency::get<1>(shape)};

  auto rank2 = rank(shape2, idx2);

  return agency::get<2>(idx) + agency::get<2>(shape) * rank2;
}

void test1()
{
  using executor_type1 = agency::nested_executor<agency::sequential_executor,agency::sequential_executor>;

  using executor_type2 = agency::nested_executor<agency::sequential_executor,executor_type1>;

  executor_type2 exec;
  executor_type2::shape_type shape{2,2,2};

  auto lambda = [=](executor_type2::index_type idx, int& outer_shared, int& middle_shared, int& inner_shared)
  {
    std::cout << "idx: " << idx << std::endl;
    std::cout << "outer_shared: " << outer_shared << std::endl;
    std::cout << "middle_shared: " << middle_shared << std::endl;
    std::cout << "inner_shared:  " << inner_shared << std::endl;

    auto outer_shape_ = shape;
    agency::size3 outer_shape{std::get<0>(outer_shape_), std::get<1>(outer_shape_), std::get<2>(outer_shape_)};
    auto outer_idx_   = idx;
    agency::size3 outer_idx{std::get<0>(outer_idx_), std::get<1>(outer_idx_), std::get<2>(outer_idx_)};

    assert(outer_shared  == rank(outer_shape, outer_idx) + 1);


    auto middle_shape_ = agency::detail::tuple_tail(shape);
    agency::size2 middle_shape{std::get<0>(middle_shape_), std::get<1>(middle_shape_)};
    auto middle_idx_   = agency::detail::tuple_tail(idx);
    agency::size2 middle_idx{std::get<0>(middle_idx_), std::get<1>(middle_idx_)};

    assert(middle_shared == rank(middle_shape, middle_idx) + 2);

    auto inner_shape  = agency::detail::tuple_tail(middle_shape);
    auto inner_idx    = agency::detail::tuple_tail(middle_idx);

    assert(inner_shared  == agency::detail::get<0>(inner_idx) + 3);

    ++outer_shared;
    ++middle_shared;
    ++inner_shared;
  };

  ::bulk_invoke_executor(exec, lambda, shape, share<0>(1), share<1>(2), share<2>(3));
}


template<class ExecutorTraits, class AgentTraits, class Function, size_t... UserArgIndices>
struct execute_agent_functor
{
  using agent_type        = typename AgentTraits::execution_agent_type;
  using agent_param_type  = typename AgentTraits::param_type;
  using agent_domain_type = typename AgentTraits::domain_type;
  using agent_shape_type  = decltype(std::declval<agent_domain_type>().shape());

  using executor_shape_type = typename ExecutorTraits::shape_type;

  agent_param_type    agent_param_;
  agent_shape_type    agent_shape_;
  executor_shape_type executor_shape_;
  Function            f_;

  using agent_index_type    = typename AgentTraits::index_type;
  using executor_index_type = typename ExecutorTraits::index_type;

  template<class... Args>
  void operator()(const executor_index_type& executor_idx, Args&&... args)
  {
    // collect all parameters into a tuple of references
    auto args_tuple = agency::detail::forward_as_tuple(std::forward<Args>(args)...);

    // split the parameters into user parameters and agent parameters
    auto user_args         = agency::detail::tuple_take_view<sizeof...(UserArgIndices)>(args_tuple);
    auto agent_shared_args = agency::detail::tuple_drop_view<sizeof...(UserArgIndices)>(args_tuple);

    // turn the executor index into an agent index
    using agent_index_type = typename AgentTraits::index_type;
    auto agent_idx = agency::detail::index_cast<agent_index_type>(executor_idx, executor_shape_, agent_shape_);

    // AgentTraits::execute expects a function whose only parameter is agent_type
    // so we have to 
    auto invoke_f = [&user_args,this](agent_type& self)
    {
      // invoke f by passing the agent, then the user's parameters
      f_(self, agency::detail::get<UserArgIndices>(user_args)...);
    };

    AgentTraits::execute(invoke_f, agent_idx, agent_param_, agent_shared_args);
  }
};


template<size_t... UserArgIndices, size_t... SharedArgIndices, class ExecutionPolicy, class Function, class... Args>
void bulk_invoke_new_impl(agency::detail::index_sequence<UserArgIndices...>,
                          agency::detail::index_sequence<SharedArgIndices...>,
                          ExecutionPolicy& policy, Function f, Args&&... args)
{
  using agent_type = typename ExecutionPolicy::execution_agent_type;
  using agent_traits = agency::execution_agent_traits<agent_type>;
  using execution_category = typename agent_traits::execution_category;

  // get the parameters of the agent
  auto param = policy.param();
  auto agent_shape = agent_traits::domain(param).shape();

  // XXX if the agent is not nested, make_shared_initializer returns a single value
  //     but the get() below expects it to be a tuple, so wrap if execution is not nested
  // XXX eliminate this once #20 is resolved
  auto agent_shared_params = agency::detail::make_tuple_if_not_nested<execution_category>(agent_traits::make_shared_initializer(param));

  using executor_type = typename ExecutionPolicy::executor_type;
  using executor_traits = agency::executor_traits<executor_type>;

  // convert the shape of the agent into the type of the executor's shape
  using executor_shape_type = typename executor_traits::shape_type;
  executor_shape_type executor_shape = agency::detail::shape_cast<executor_shape_type>(agent_shape);

  // create the function that will marshal parameters received from bulk_invoke(executor) and execute the agent
  auto lambda = execute_agent_functor<executor_traits,agent_traits,Function,UserArgIndices...>{param, agent_shape, executor_shape, f};

  ::bulk_invoke_executor(policy.executor(), lambda, executor_shape, std::forward<Args>(args)..., share<SharedArgIndices>(std::get<SharedArgIndices>(agent_shared_params))...);
}


template<class ExecutionPolicy, class Function, class... Args>
typename agency::detail::enable_if_call_possible<
    Function(
      typename ExecutionPolicy::execution_agent_type&,
      decay_parameter_t<Args>...
    )
>::type
  bulk_invoke_new(ExecutionPolicy& policy, Function f, Args&&... args)
{
  using agent_traits = agency::execution_agent_traits<typename ExecutionPolicy::execution_agent_type>;
  const size_t num_shared_params = agency::detail::execution_depth<typename agent_traits::execution_category>::value;

  bulk_invoke_new_impl(agency::detail::index_sequence_for<Args...>(), agency::detail::make_index_sequence<num_shared_params>(), policy, f, std::forward<Args>(args)...);
}

void test2()
{
  auto lambda = [](agency::sequential_group<agency::sequential_agent>& self, int local_param)
  {
    std::cout << "index: " << self.index() << " local_param: " << local_param << std::endl;
  };

  auto policy = agency::seq(2,agency::seq(2));
  bulk_invoke_new(policy, lambda, 13);
}


void test3()
{
  auto lambda = [](agency::sequential_agent& self)
  {
    std::cout << "index: " << self.index() << std::endl;
  };

  auto policy = agency::seq(10);
  ::bulk_invoke_new(policy, lambda);
}


int main()
{
//  test1();
  test2();
//  test3();

  return 0;
}

