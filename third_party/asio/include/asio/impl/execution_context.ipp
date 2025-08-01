//
// impl/execution_context.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2025 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_IMPL_EXECUTION_CONTEXT_IPP
#define ASIO_IMPL_EXECUTION_CONTEXT_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "asio/detail/config.hpp"
#include "asio/execution_context.hpp"
#include "asio/detail/service_registry.hpp"

#include "asio/detail/push_options.hpp"

namespace asio {

execution_context::execution_context()
  : execution_context(
      detail::allocate_object<allocator_impl<std::allocator<void>>>(
        std::allocator<void>(), std::allocator<void>()))
{
}

execution_context::execution_context(allocator_impl_base* alloc)
  : allocator_{alloc},
    service_registry_(
        detail::allocate_object<detail::service_registry>(
          allocator<void>(*this), *this))
{
}

execution_context::execution_context(
    const execution_context::service_maker& initial_services)
  : execution_context(
      detail::allocate_object<allocator_impl<std::allocator<void>>>(
        std::allocator<void>(), std::allocator<void>()),
      initial_services)
{
}

execution_context::execution_context(allocator_impl_base* alloc,
    const execution_context::service_maker& initial_services)
  : allocator_{alloc},
    service_registry_(
        detail::allocate_object<detail::service_registry>(
          allocator<void>(*this), *this))
{
  initial_services.make(*this);
}

execution_context::~execution_context()
{
  shutdown();
  destroy();
  detail::deallocate_object(allocator<void>(*this), service_registry_);
}

void execution_context::shutdown()
{
  service_registry_->shutdown_services();
}

void execution_context::destroy()
{
  service_registry_->destroy_services();
}

void execution_context::notify_fork(
    asio::execution_context::fork_event event)
{
  service_registry_->notify_fork(event);
}

execution_context::allocator_impl_base::~allocator_impl_base()
{
}

execution_context::service::service(execution_context& owner)
  : owner_(owner),
    next_(0),
    destroy_(0)
{
}

execution_context::service::~service()
{
}

void execution_context::service::notify_fork(execution_context::fork_event)
{
}

execution_context::service_maker::~service_maker()
{
}

service_already_exists::service_already_exists()
  : std::logic_error("Service already exists.")
{
}

invalid_service_owner::invalid_service_owner()
  : std::logic_error("Invalid service owner.")
{
}

} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_IMPL_EXECUTION_CONTEXT_IPP
