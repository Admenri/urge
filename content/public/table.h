// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TABLE_H_
#define CONTENT_PUBLIC_TABLE_H_

#include <vector>

#include "base/bind/callback_list.h"
#include "base/memory/ref_counted.h"
#include "content/public/serializable.h"

namespace content {

class Table final : public base::RefCounted<Table>, public Serializable {
 public:
  Table(int x_size, int y_size = 1, int z_size = 1);
  ~Table();

  Table(const Table& other);
  Table& operator=(const Table& other);

  int16_t Get(int x, int y = 0, int z = 0) const;
  void Set(int16_t value, int x, int y = 0, int z = 0);

  int GetXSize() const { return x_size_; }
  int GetYSize() const { return y_size_; }
  int GetZSize() const { return z_size_; }

  void Resize(int x);
  void Resize(int x, int y);
  void Resize(int x, int y, int z);

  std::string Serialize() override;
  static scoped_refptr<Table> Deserialize(const std::string& data);

  // Internal
  inline int16_t& At(int x, int y = 0, int z = 0) {
    return data_[x + y * x_size_ + z * y_size_ * x_size_];
  }

  inline const int16_t& At(int x, int y = 0, int z = 0) const {
    return data_[x + y * x_size_ + z * y_size_ * x_size_];
  }

  base::CallbackListSubscription AddObserver(base::RepeatingClosure observer) {
    return observers_.Add(std::move(observer));
  }

 private:
  base::RepeatingClosureList observers_;

  int x_size_, y_size_, z_size_;
  std::vector<int16_t> data_;
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_TABLE_H_