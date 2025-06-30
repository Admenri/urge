// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_VALUE_OBSERVER_H_
#define CONTENT_COMMON_VALUE_OBSERVER_H_

#include "base/bind/callback_list.h"

namespace content {

class ValueNotification {
 public:
  virtual ~ValueNotification() = default;

  // Add a observer to host,
  // keep return value for observer lifespan managing.
  base::CallbackListSubscription AddObserver(
      const base::RepeatingClosure& handler) {
    return observers_.Add(std::move(handler));
  }

 protected:
  // Notify clousure in list,
  // it is not thread-safe
  virtual void NotifyObservers() { observers_.Notify(); }

 private:
  base::RepeatingClosureList observers_;
};

}  // namespace content

#endif  //! CONTENT_COMMON_VALUE_OBSERVER_H_
