// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef __CLANG_REFLECTION_HPP
#define __CLANG_REFLECTION_HPP

#if __cplusplus < 201703L
#error "<clang_reflection.hpp> requires C++17 or later"
#else
namespace __clang_reflection {
// Named enable_if so that clang reports which requirement removed a candidate.
template <bool, class _Tp = void> struct enable_if {};
template <class _Tp> struct enable_if<true, _Tp> { using type = _Tp; };
} // namespace __clang_reflection

// reflection<T> describes the public non-static data members declared
// directly in the struct or class T, in declaration order. Index I names the
// same member in every query, and every I below variable_count is valid, so
// the queries can be driven by std::make_index_sequence<variable_count>.
//
// Members are accessed through ref, get and set. A bit-field has no address,
// so it has no ref: use is_bit_field to choose between ref and get.
template <class T> struct reflection {
  // The number of members.
  static constexpr auto variable_count = __builtin_public_field_count(T);

  // The declared type of member I, e.g. 'int &' for a reference member.
  template <decltype(sizeof(0)) I>
  using variable_type = __builtin_public_field_type<T, I>;

  // The name of member I as a null-terminated string literal.
  template <decltype(sizeof(0)) I>
  static constexpr const char *variable_name =
      __builtin_public_field_name(static_cast<T *>(nullptr), I);

  // Whether member I is a bit-field.
  template <decltype(sizeof(0)) I>
  static constexpr bool is_bit_field =
      __builtin_public_field_is_bitfield(static_cast<T *>(nullptr), I);

  // A reference to member I of 'object', with the type std::get would give a
  // tuple element: T& for an lvalue, T&& for an rvalue, const-qualified for a
  // const object unless the member is mutable. A reference member is always
  // an lvalue. Not declared for a bit-field, which cannot be bound to a
  // reference.
  template <decltype(sizeof(0)) I, class U,
            typename __clang_reflection::enable_if<!is_bit_field<I>, int>::type
                = 0>
  static constexpr decltype(auto) ref(U &&object) noexcept {
    static_assert(__is_same(__remove_cvref(U), __remove_cv(T)),
                  "reflection<T>::ref requires an object of type T");
    return (__builtin_public_field_ref(static_cast<U &&>(object), I));
  }

  // A copy of member I of 'object', without const or reference: 'int' for an
  // 'int &' or 'const int' member. Moves the member out when 'object' is an
  // rvalue. An array cannot be returned by value, so use ref for one.
  template <decltype(sizeof(0)) I, class U>
  static constexpr auto get(U &&object) {
    static_assert(__is_same(__remove_cvref(U), __remove_cv(T)),
                  "reflection<T>::get requires an object of type T");
    static_assert(!__is_array(variable_type<I>),
                  "reflection<T>::get cannot copy an array member; use ref");
    return __builtin_public_field_ref(static_cast<U &&>(object), I);
  }

  // Assigns 'value' to member I of 'object', as 'object.member = value' does:
  // a bit-field keeps its width, and a reference member assigns the object it
  // refers to.
  template <decltype(sizeof(0)) I, class V>
  static constexpr void set(T &object, V &&value) {
    __builtin_public_field_ref(object, I) = static_cast<V &&>(value);
  }
};
#endif

#endif
