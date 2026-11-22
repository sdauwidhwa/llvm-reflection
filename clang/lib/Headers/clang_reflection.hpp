// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef __CLANG_REFLECTION_HPP
#define __CLANG_REFLECTION_HPP

#if __cplusplus < 201703L
#error "<clang_reflection.hpp> requires C++17 or later"
#else
// reflection<T> describes the public non-static data members declared
// directly in the struct or class T, in declaration order. Index I names the
// same member in every query, and every I below variable_count is valid, so
// the queries can be driven by std::make_index_sequence<variable_count>.
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

  // Member I of 'object', with the reference type std::get would give a tuple
  // element: T& for an lvalue, T&& for an rvalue, const-qualified for a const
  // object. A bit-field has no address, so it is returned by value instead.
  // To assign a bit-field, use __builtin_public_field_ref(object, I) = value.
  template <decltype(sizeof(0)) I, class U>
  static constexpr decltype(auto) get(U &&object) noexcept {
    static_assert(__is_same(__remove_cvref(U), __remove_cv(T)),
                  "reflection<T>::get requires an object of type T");
    if constexpr (is_bit_field<I>)
      return static_cast<variable_type<I>>(
          __builtin_public_field_ref(object, I));
    else
      return (__builtin_public_field_ref(static_cast<U &&>(object), I));
  }
};
#endif

#endif
