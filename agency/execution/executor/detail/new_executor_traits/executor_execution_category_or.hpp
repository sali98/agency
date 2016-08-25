#pragma once

#include <agency/detail/config.hpp>
#include <agency/execution/execution_categories.hpp>
#include <agency/detail/type_traits.hpp>

namespace agency
{
namespace detail
{
namespace new_executor_traits_detail
{


template<class T, class Default = unsequenced_execution_tag>
struct executor_execution_category_or
{
  template<class U>
  using helper = typename U::execution_category;

  using type = detected_or_t<Default, helper, T>;
};

template<class T, class Default = unsequenced_execution_tag>
using executor_execution_category_or_t = typename executor_execution_category_or<T,Default>::type;


} // end new_executor_traits_detail
} // end detail
} // end agency

