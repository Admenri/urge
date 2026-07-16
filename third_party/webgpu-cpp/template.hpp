// Copyright 2026 Admenri
// Copyright 2017 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef WEBGPU_CPP_HPP_
#define WEBGPU_CPP_HPP_

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "webgpu.h"

#include <type_traits>

// The operators in wgpu:: namespace need be introduced into other namespaces with
// using-declarations for C++ Argument Dependent Lookup to work.
#define WGPU_IMPORT_BITMASK_OPERATORS \
    using wgpu::operator|;            \
    using wgpu::operator&;            \
    using wgpu::operator^;            \
    using wgpu::operator~;            \
    using wgpu::operator&=;           \
    using wgpu::operator|=;           \
    using wgpu::operator^=;           \
    using wgpu::HasZeroOrOneBits;

namespace wgpu {

template <typename T>
struct IsWGPUBitmask {
    static constexpr bool enable = false;
};

template <typename T, typename Enable = void>
struct LowerBitmask {
    static constexpr bool enable = false;
};

template <typename T>
struct LowerBitmask<T, typename std::enable_if<IsWGPUBitmask<T>::enable>::type> {
    static constexpr bool enable = true;
    using type = T;
    constexpr static T Lower(T t) { return t; }
};

template <typename T>
struct BoolConvertible {
    using Integral = typename std::underlying_type<T>::type;

    explicit constexpr BoolConvertible(Integral value) : value(value) {}
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr operator bool() const { return value != 0; }
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr operator T() const { return static_cast<T>(value); }

    Integral value;
};

template <typename T>
struct LowerBitmask<BoolConvertible<T>> {
    static constexpr bool enable = true;
    using type = T;
    static constexpr type Lower(BoolConvertible<T> t) { return t; }
};

template <
    typename T1,
    typename T2,
    typename = typename std::enable_if<LowerBitmask<T1>::enable && LowerBitmask<T2>::enable>::type>
constexpr BoolConvertible<typename LowerBitmask<T1>::type> operator|(T1 left, T2 right) {
    using T = typename LowerBitmask<T1>::type;
    using Integral = typename std::underlying_type<T>::type;
    return BoolConvertible<T>(static_cast<Integral>(LowerBitmask<T1>::Lower(left)) |
                              static_cast<Integral>(LowerBitmask<T2>::Lower(right)));
}

template <
    typename T1,
    typename T2,
    typename = typename std::enable_if<LowerBitmask<T1>::enable && LowerBitmask<T2>::enable>::type>
constexpr BoolConvertible<typename LowerBitmask<T1>::type> operator&(T1 left, T2 right) {
    using T = typename LowerBitmask<T1>::type;
    using Integral = typename std::underlying_type<T>::type;
    return BoolConvertible<T>(static_cast<Integral>(LowerBitmask<T1>::Lower(left)) &
                              static_cast<Integral>(LowerBitmask<T2>::Lower(right)));
}

template <
    typename T1,
    typename T2,
    typename = typename std::enable_if<LowerBitmask<T1>::enable && LowerBitmask<T2>::enable>::type>
constexpr BoolConvertible<typename LowerBitmask<T1>::type> operator^(T1 left, T2 right) {
    using T = typename LowerBitmask<T1>::type;
    using Integral = typename std::underlying_type<T>::type;
    return BoolConvertible<T>(static_cast<Integral>(LowerBitmask<T1>::Lower(left)) ^
                              static_cast<Integral>(LowerBitmask<T2>::Lower(right)));
}

template <typename T1>
constexpr BoolConvertible<typename LowerBitmask<T1>::type> operator~(T1 t) {
    using T = typename LowerBitmask<T1>::type;
    using Integral = typename std::underlying_type<T>::type;
    return BoolConvertible<T>(~static_cast<Integral>(LowerBitmask<T1>::Lower(t)));
}

template <
    typename T,
    typename T2,
    typename = typename std::enable_if<IsWGPUBitmask<T>::enable && LowerBitmask<T2>::enable>::type>
constexpr T& operator&=(T& l, T2 right) {
    T r = LowerBitmask<T2>::Lower(right);
    l = l & r;
    return l;
}

template <
    typename T,
    typename T2,
    typename = typename std::enable_if<IsWGPUBitmask<T>::enable && LowerBitmask<T2>::enable>::type>
constexpr T& operator|=(T& l, T2 right) {
    T r = LowerBitmask<T2>::Lower(right);
    l = l | r;
    return l;
}

template <
    typename T,
    typename T2,
    typename = typename std::enable_if<IsWGPUBitmask<T>::enable && LowerBitmask<T2>::enable>::type>
constexpr T& operator^=(T& l, T2 right) {
    T r = LowerBitmask<T2>::Lower(right);
    l = l ^ r;
    return l;
}

template <typename T>
constexpr bool HasZeroOrOneBits(T value) {
    using Integral = typename std::underlying_type<T>::type;
    return (static_cast<Integral>(value) & (static_cast<Integral>(value) - 1)) == 0;
}

}  // namespace wgpu

namespace wgpu {

///
/// Enums
///

{% for enum_name, enum_members in json_data["enum"].items() if enum_name != "OptionalBool" %}
  enum class {{enum_name}} : uint32_t {
    {% for member in enum_members %}
      {{member if not member[0].isdigit() else "e" + member}} = WGPU{{enum_name}}_{{member}},
    {% endfor %}
  };
  static_assert(sizeof({{enum_name}}) == sizeof(WGPU{{enum_name}}), "sizeof mismatch for {{enum_name}}");
  static_assert(alignof({{enum_name}}) == alignof(WGPU{{enum_name}}), "alignof mismatch for {{enum_name}}");

{% endfor %}

///
/// Bitmasks
///
{% for bitmask_name, bitmask_members in json_data["bitmask"].items() %}
  enum class {{bitmask_name}} : uint64_t {
    {% for member in bitmask_members %}
      {{member if not member[0].isdigit() else "e" + member}} = WGPU{{bitmask_name}}_{{member}},
    {% endfor %}
  };
  static_assert(sizeof({{bitmask_name}}) == sizeof(WGPU{{bitmask_name}}), "sizeof mismatch for {{bitmask_name}}");
  static_assert(alignof({{bitmask_name}}) == alignof(WGPU{{bitmask_name}}), "alignof mismatch for {{bitmask_name}}");

  template<>
  struct IsWGPUBitmask<wgpu::{{bitmask_name}}> {
    static constexpr bool enable = true;
  };

{% endfor %}

///
/// Classes
///

// Special class for booleans in order to allow implicit conversions.
class Bool {
 public:
  constexpr Bool() = default;
  // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
  constexpr Bool(bool value) : mValue(static_cast<WGPUBool>(value)) {}
  // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
  Bool(WGPUBool value) : mValue(value) {}

  constexpr operator bool() const { return static_cast<bool>(mValue); }

 private:
  friend struct std::hash<Bool>;
  // Default to false.
  WGPUBool mValue = static_cast<WGPUBool>(false);
};

// Special class for optional booleans in order to allow conversions.
class OptionalBool {
 public:
  constexpr OptionalBool() = default;
  // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
  constexpr OptionalBool(bool value)
      : mValue(static_cast<WGPUOptionalBool>(value)) {}
  // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
  constexpr OptionalBool(std::optional<bool> value)
      : mValue(value ? static_cast<WGPUOptionalBool>(*value)
                     : WGPUOptionalBool_Undefined) {}
  // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
  constexpr OptionalBool(WGPUOptionalBool value) : mValue(value) {}

  // Define the values that are equivalent to the enums.
  static const OptionalBool False;
  static const OptionalBool True;
  static const OptionalBool Undefined;

  // Assignment operators.
  OptionalBool& operator=(const bool& value) {
    mValue = static_cast<WGPUOptionalBool>(value);
    return *this;
  }
  OptionalBool& operator=(const std::optional<bool>& value) {
    mValue = value ? static_cast<WGPUOptionalBool>(*value)
                   : WGPUOptionalBool_Undefined;
    return *this;
  }
  OptionalBool& operator=(const WGPUOptionalBool& value) {
    mValue = value;
    return *this;
  }

  // Conversion functions.
  operator WGPUOptionalBool() const { return mValue; }
  operator std::optional<bool>() const {
    if (mValue == WGPUOptionalBool_Undefined) {
      return std::nullopt;
    }
    return static_cast<bool>(mValue);
  }

  // Comparison functions.
  friend bool operator==(const OptionalBool& lhs, const OptionalBool& rhs) {
    return lhs.mValue == rhs.mValue;
  }
  friend bool operator!=(const OptionalBool& lhs, const OptionalBool& rhs) {
    return lhs.mValue != rhs.mValue;
  }

 private:
  friend struct std::hash<OptionalBool>;
  // Default to undefined.
  WGPUOptionalBool mValue = WGPUOptionalBool_Undefined;
};
inline const OptionalBool OptionalBool::False =
    OptionalBool(WGPUOptionalBool_False);
inline const OptionalBool OptionalBool::True =
    OptionalBool(WGPUOptionalBool_True);
inline const OptionalBool OptionalBool::Undefined =
    OptionalBool(WGPUOptionalBool_Undefined);

// Helper class to wrap Status which allows implicit conversion to bool.
// Used while callers switch to checking the Status enum instead of booleans.
// TODO(crbug.com/42241199): Remove when all callers check the enum.
struct ConvertibleStatus {
  // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
  constexpr ConvertibleStatus(Status status) : status(status) {}
  // NOLINTNEXTLINE(runtime/explicit) allow implicit conversion
  constexpr operator bool() const { return status == Status::Success; }
  // NOLINTNEXTLINE(runtime/explicit) allow implicit conversion
  constexpr operator Status() const { return status; }
  Status status;
};

template <typename Derived, typename CType>
class ObjectBase {
 public:
  ObjectBase() = default;
  ObjectBase(CType handle) : mHandle(handle) {
    if (mHandle)
      Derived::WGPUAddRef(mHandle);
  }
  ~ObjectBase() {
    if (mHandle)
      Derived::WGPURelease(mHandle);
  }

  ObjectBase(ObjectBase const& other) : ObjectBase(other.Get()) {}
  Derived& operator=(ObjectBase const& other) {
    if (&other != this) {
      if (mHandle)
        Derived::WGPURelease(mHandle);
      mHandle = other.mHandle;
      if (mHandle)
        Derived::WGPUAddRef(mHandle);
    }

    return static_cast<Derived&>(*this);
  }

  ObjectBase(ObjectBase&& other) {
    mHandle = other.mHandle;
    other.mHandle = 0;
  }
  Derived& operator=(ObjectBase&& other) {
    if (&other != this) {
      if (mHandle)
        Derived::WGPURelease(mHandle);
      mHandle = other.mHandle;
      other.mHandle = 0;
    }

    return static_cast<Derived&>(*this);
  }

  ObjectBase(std::nullptr_t) {}
  Derived& operator=(std::nullptr_t) {
    if (mHandle != nullptr) {
      Derived::WGPURelease(mHandle);
      mHandle = nullptr;
    }
    return static_cast<Derived&>(*this);
  }

  bool operator==(std::nullptr_t) const { return mHandle == nullptr; }
  bool operator!=(std::nullptr_t) const { return mHandle != nullptr; }

  explicit operator bool() const { return mHandle != nullptr; }
  CType Get() const { return mHandle; }
  CType MoveToCHandle() {
    CType result = mHandle;
    mHandle = 0;
    return result;
  }
  static Derived Acquire(CType handle) {
    Derived result;
    result.mHandle = handle;
    return result;
  }

 protected:
  CType mHandle = nullptr;
};

///
/// Forward Declaration
///

{% for class_name in json_data["class"].keys() %}
  class {{class_name}};
{% endfor %}

{% for struct_name in json_data["struct"].keys() %}
  {% if struct_name.endswith("CallbackInfo") %}{% continue %}{% endif %}
  struct {{struct_name}};
{% endfor %}

///
/// Utility
///

// TODO(42241188): Remove once all clients use StringView versions of the
// callbacks. To make MSVC happy we need a StringView constructor from the
// adapter, so we first need to forward declare StringViewAdapter here.
// Otherwise MSVC complains about an ambiguous conversion.
namespace detail {
struct StringViewAdapter;
}  // namespace detail

struct StringView {
  char const* data = nullptr;
  size_t length = WGPU_STRLEN;

  inline constexpr StringView() noexcept = default;

  // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
  inline constexpr StringView(const std::string_view& sv) noexcept {
    this->data = sv.data();
    this->length = sv.length();
  }

  // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
  inline constexpr StringView(const char* s) {
    this->data = s;
    this->length = WGPU_STRLEN;  // use strlen
  }

  // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
  inline constexpr StringView(WGPUStringView s) {
    this->data = s.data;
    this->length = s.length;
  }

  inline constexpr StringView(const char* data, size_t length) {
    this->data = data;
    this->length = length;
  }

  // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
  inline constexpr StringView(std::nullptr_t) {
    this->data = nullptr;
    this->length = WGPU_STRLEN;
  }

  // NOLINTNEXTLINE(runtime/explicit) allow implicit construction
  inline constexpr StringView(std::nullopt_t) {
    this->data = nullptr;
    this->length = WGPU_STRLEN;
  }

  bool IsUndefined() const {
    return this->data == nullptr && this->length == WGPU_STRLEN;
  }

  // NOLINTNEXTLINE(runtime/explicit) allow implicit conversion
  operator std::string_view() const {
    if (this->length == WGPU_STRLEN) {
      if (IsUndefined()) {
        return {};
      }
      return {this->data};
    }
    return {this->data, this->length};
  }

  template <typename View,
            typename = std::enable_if_t<
                std::is_constructible_v<View, const char*, size_t>>>
  explicit operator View() const {
    if (this->length == WGPU_STRLEN) {
      if (IsUndefined()) {
        return {};
      }
      return {this->data};
    }
    return {this->data, this->length};
  }

  StringView(const detail::StringViewAdapter& s);
};

namespace detail {
constexpr size_t ConstexprMax(size_t a, size_t b) {
  return a > b ? a : b;
}

template <typename T>
static T& AsNonConstReference(const T& value) {
  return const_cast<T&>(value);
}

// A wrapper around StringView that can be implicitly converted to const char*
// with temporary storage that adds the \0 for output strings that are all
// explicitly-sized.
// TODO(42241188): Remove once all clients use StringView versions of the
// callbacks.
struct StringViewAdapter {
  WGPUStringView sv;
  char* nullTerminated = nullptr;

  StringViewAdapter(WGPUStringView sv) : sv(sv) {}
  ~StringViewAdapter() { delete[] nullTerminated; }
  operator ::WGPUStringView() { return sv; }
  operator StringView() { return {sv.data, sv.length}; }
  operator const char*() {
    assert(sv.length != WGPU_STRLEN);
    assert(nullTerminated == nullptr);
    nullTerminated = new char[sv.length + 1];
    for (size_t i = 0; i < sv.length; i++) {
      nullTerminated[i] = sv.data[i];
    }
    nullTerminated[sv.length] = 0;
    return nullTerminated;
  }
};
}  // namespace detail

inline StringView::StringView(const detail::StringViewAdapter& s)
    : data(s.sv.data), length(s.sv.length) {}

namespace detail {
// For callbacks, we support two modes:
//   1) No userdata where we allow a std::function type that can include
//   argument captures. 2) Explicit typed userdata where we only allow
//   non-capturing lambdas or function pointers.
template <typename... Args>
struct CallbackTypeBase;
template <typename... Args>
struct CallbackTypeBase<std::tuple<Args...>> {
  using Callback = std::function<void(Args...)>;
};
template <typename... Args>
struct CallbackTypeBase<std::tuple<Args...>, void> {
  using Callback = void(Args...);
};
template <typename... Args, typename T>
struct CallbackTypeBase<std::tuple<Args...>, T> {
  using Callback = void(Args..., T);
};
}  // namespace detail

///
/// Callbacks
///

// TODO

///
/// Classes
///

{%- macro render_function_params_declaration(params) %}
  {%- for param in params -%}
    {%- if not loop.first %}, {% endif -%}
    {{as_cpptype(param["type"])}} {{param["name"]}}
  {%- endfor -%}
{%- endmacro %}

{% for class_name, class_methods in json_data["class"].items() %}
  class {{class_name}} : public ObjectBase<{{class_name}}, WGPU{{class_name}}> {
   public:
    using ObjectBase::ObjectBase;
    using ObjectBase::operator=;

  {% for method_name, method_data in class_methods.items() %}
    {% if method_name == "AddRef" or method_name == "Release" %}{% continue %}{% endif %}
    inline {{as_cpptype(method_data["return"], in_struct=True)}} {{method_name}}({{render_function_params_declaration(method_data["params"])}}) const;
  {% endfor %}

   private:
    friend ObjectBase<{{class_name}}, WGPU{{class_name}}>;
    static inline void WGPUAddRef(WGPU{{class_name}} handle);
    static inline void WGPURelease(WGPU{{class_name}} handle);
  };

{% endfor %}

///
/// Structs
///

// ChainedStruct
struct ChainedStruct {
  ChainedStruct const* nextInChain = nullptr;
  SType sType = SType(0u);
};
static_assert(sizeof(ChainedStruct) == sizeof(WGPUChainedStruct),
              "sizeof mismatch for ChainedStruct");
static_assert(alignof(ChainedStruct) == alignof(WGPUChainedStruct),
              "alignof mismatch for ChainedStruct");
static_assert(offsetof(ChainedStruct, nextInChain) ==
                  offsetof(WGPUChainedStruct, next),
              "offsetof mismatch for ChainedStruct::nextInChain");
static_assert(offsetof(ChainedStruct, sType) ==
                  offsetof(WGPUChainedStruct, sType),
              "offsetof mismatch for ChainedStruct::sType");

{%- macro render_member_declaraction(member) %}
  {{as_cpptype(member["type"], in_struct=True)}} {{member["name"]}}{% if member.get("default") != None %} = {{as_cppvalue(member["default"])}}{% endif %}
{%- endmacro %}

{% for struct_name, struct_members in json_data["struct"].items() %}
  {% if struct_name == "StringView" or struct_name == "ChainedStruct" %}{% continue %}{% endif %}
  {% if struct_name.endswith("CallbackInfo") %}{% continue %}{% endif %}
  {% set is_chained_struct = (struct_members[0]["type"] == "WGPUChainedStruct") %}
  {% set is_manual_free = (struct_name + "FreeMembers") in json_data["function"].keys() %}
  struct {{struct_name}}{{" : ChainedStruct" if is_chained_struct else ""}} {
  {% if is_chained_struct %}
    inline {{struct_name}}();
  {% endif %}
    inline operator const WGPU{{struct_name}}&() const noexcept;
  {% if is_manual_free %}
    inline {{struct_name}}();
    inline ~{{struct_name}}();
    {{struct_name}}(const {{struct_name}}&) = delete;
    {{struct_name}}& operator=(const {{struct_name}}&) = delete;
    inline {{struct_name}}({{struct_name}}&&);
    inline {{struct_name}}& operator=({{struct_name}}&&);
  {% endif %}

  {% set processed_members = list(struct_members) %}
  {% if is_chained_struct %}
    {% do processed_members.pop(0) %}
  {% endif %}
  {% for member in processed_members %}
    {% if is_chained_struct and loop.first %}
      {% set member_type = as_cpptype(member["type"], in_struct=True) %}
      static constexpr size_t kFirstMemberAlignment =
          detail::ConstexprMax(alignof(ChainedStruct), alignof({{member_type}}));
      alignas(kFirstMemberAlignment) {{render_member_declaraction(member)}};
    {% else %}
      {{render_member_declaraction(member)}};
    {% endif %}
  {% endfor %}

  {% if is_manual_free %}
   private:
    inline void FreeMembers();
    static inline void Reset({{struct_name}}& value);
  {% endif %}
  };

{% endfor %}

///
/// Struct Implementations
///

{% for struct_name, struct_members in json_data["struct"].items() %}
  {% if struct_name == "StringView" or struct_name == "ChainedStruct" %}{% continue %}{% endif %}
  {% if struct_name.endswith("CallbackInfo") %}{% continue %}{% endif %}
  {% set is_chained_struct = (struct_members[0]["type"] == "WGPUChainedStruct") %}
  {% set is_manual_free = (struct_name + "FreeMembers") in json_data["function"].keys() %}
  // {{struct_name}} implementation

  {% if is_chained_struct %}
    {{struct_name}}::{{struct_name}}()
        : ChainedStruct{nullptr, SType::{{struct_name}}} {}
  {% endif %}

  {% if is_manual_free %}
    {{struct_name}}::{{struct_name}}() = default;

    {{struct_name}}::~{{struct_name}}() {
      FreeMembers();
    }

    {{struct_name}}::{{struct_name}}({{struct_name}}&& rhs)
        : {%- for member in struct_members -%}
            {% if not loop.first%}, {% endif %}
            {{member["name"]}}(rhs.{{member["name"]}})
          {%- endfor -%} {
      Reset(rhs);
    }

    {{struct_name}}& {{struct_name}}::operator=({{struct_name}}&& rhs) {
      if (&rhs == this)
        return *this;
      FreeMembers();
      {% for member in struct_members %}
        detail::AsNonConstReference(this->{{member["name"]}}) = std::move(rhs.{{member["name"]}});
      {% endfor %}
      Reset(rhs);
      return *this;
    }

    void {{struct_name}}::FreeMembers() {
      wgpu{{struct_name}}FreeMembers(*reinterpret_cast<WGPU{{struct_name}}*>(this));
    }

    // static
    void {{struct_name}}::Reset({{struct_name}}& value) {
      {{struct_name}} defaultValue = {};
      {% for member in struct_members %}
        detail::AsNonConstReference(value.{{member["name"]}}) = defaultValue.{{member["name"]}};
      {% endfor %}
    }
  {% endif %}

  {{struct_name}}::operator const WGPU{{struct_name}}&() const noexcept {
    return *reinterpret_cast<const WGPU{{struct_name}}*>(this);
  }

  static_assert(sizeof({{struct_name}}) == sizeof(WGPU{{struct_name}}), "sizeof mismatch for {{struct_name}}");
  static_assert(alignof({{struct_name}}) == alignof(WGPU{{struct_name}}), "alignof mismatch for {{struct_name}}");
  {% for member in struct_members %}
    {% if loop.first and is_chained_struct %}{% continue %}{% endif %}
    static_assert(offsetof({{struct_name}}, {{member["name"]}}) == offsetof(WGPU{{struct_name}}, {{member["name"]}}), "offsetof mismatch for {{struct_name}}::{{member["name"]}}");
  {% endfor %}

{% endfor %}

///
/// Class Implementations
///

{%- macro render_class_method_params(params) %}
  {%- for param in params -%}
    {{", "}}
    {%- set param_type = param["type"].replace("WGPU_NULLABLE", "").strip() -%}
    {%- set param_name = param["name"] -%}
    {%- if param_type.endswith("*") -%}
      reinterpret_cast<{{param_type}}>({{param_name}})
    {%- elif param_type[4:] in json_data["enum"].keys() -%}
      static_cast<{{param_type}}>({{param_name}})
    {%- elif param_type[4:] in json_data["bitmask"].keys() -%}
      static_cast<{{param_type}}>({{param_name}})
    {%- elif param_type[4:] in json_data["class"].keys() -%}
      {{param_name}}.Get()
    {%- elif param_type == "WGPUStringView" -%}
      *reinterpret_cast<WGPUStringView const*>(&{{param_name}})
    {%- else -%}
      {{param_name}}
    {%- endif -%}
  {%- endfor -%}
{%- endmacro %}

{%- macro render_class_method_return_cast(ctype) %}
  {%- if ctype == "WGPUFuture" -%}
    Future{result.id}
  {%- elif ctype[4:] in json_data["enum"].keys() -%}
    static_cast<{{ctype[4:]}}>(result)
  {%- elif ctype[4:] in json_data["bitmask"].keys() -%}
    static_cast<{{ctype[4:]}}>(result)
  {%- elif ctype[4:] in json_data["class"].keys() -%}
    {{ctype[4:]}}::Acquire(result)
  {%- else -%}
    result
  {%- endif -%}
{%- endmacro %}

{% for class_name, class_methods in json_data["class"].items() %}
  // {{class_name}} implementation
  {% for method_name, method_data in class_methods.items() %}
  {% if method_name == "AddRef" or method_name == "Release" %}{% continue %}{% endif %}
  {{as_cpptype(method_data["return"], in_struct=True)}} {{class_name}}::{{method_name}}({{render_function_params_declaration(method_data["params"])}}) const {
    {{"  "}}{% if method_data["return"] != "void" %}auto result = {% endif %}wgpu{{class_name}}{{method_name}}(Get(){{render_class_method_params(method_data["params"])}});
    {% if method_data["return"] != "void" %}
      return {{render_class_method_return_cast(method_data["return"])}};
    {% endif %}
  }
  {% endfor %}

  void {{class_name}}::WGPUAddRef(WGPU{{class_name}} handle) {
    if (handle != nullptr)
      wgpu{{class_name}}AddRef(handle);
  }
  void {{class_name}}::WGPURelease(WGPU{{class_name}} handle) {
    if (handle != nullptr)
      wgpu{{class_name}}Release(handle);
  }
  static_assert(sizeof({{class_name}}) == sizeof(WGPU{{class_name}}), "sizeof mismatch for {{class_name}}");
  static_assert(alignof({{class_name}}) == alignof(WGPU{{class_name}}), "alignof mismatch for {{class_name}}");

{% endfor %}

///
/// Global Functions
///

{% for function_name, function_data in json_data["function"].items() %}
  {% if function_name.startswith("FreeMembers") %}{% continue %}{% endif %}
  static inline {{as_cpptype(function_data["return"], in_struct=True)}} {{function_name}}({{render_function_params_declaration(function_data["params"])}}) {
    {{"  "}}{% if function_data["return"] != "void" %}auto result = {% endif %}wgpu{{function_name}}({{render_class_method_params(function_data["params"]).strip(",").strip()}});
    {% if function_data["return"] != "void" %}
      return {{render_class_method_return_cast(function_data["return"])}};
    {% endif %}
  }
{% endfor %}

}  // namespace wgpu

#endif  // WEBGPU_CPP_HPP_