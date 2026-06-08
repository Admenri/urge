// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

// Using for parser binding signature
#define URGE_BINDING(...)

// Using for exception raising
#define URGE_EXCEPTION ExceptionState& exception_state

// Using for attribute getter/setter
#define URGE_ATTRIBUTE(Name, Type, Getter, Setter) \
  Type Get_##Name(URGE_EXCEPTION) Getter;          \
  void Put_##Name(const Type& value, URGE_EXCEPTION) Setter;
#define URGE_ATTRIBUTE_DECLARE(Name, Type) \
  Type Get_##Name(URGE_EXCEPTION);         \
  void Put_##Name(const Type& value, URGE_EXCEPTION);
#define URGE_ATTRIBUTE_DEFINE(Klass, Name, Type, Getter, Setter) \
  Type Klass::Get_##Name(URGE_EXCEPTION) Getter;                 \
  void Klass::Put_##Name(const Type& value, URGE_EXCEPTION) Setter;

// Binding type
template <typename Ty>
using earray = std::span<Ty>;
template <typename Ty>
using estruct = scoped_refptr<Ty>;
template <typename K, typename V>
using emap = std::unordered_map<K, V>;
using estring = std::string_view;
using epointer = void*;
