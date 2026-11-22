// RUN: %clang_cc1 -std=c++17 -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++20 -fexperimental-new-constant-interpreter -fsyntax-only -verify %s

#include <clang_reflection.hpp>

#if !__has_builtin(__builtin_public_field_count)
#error missing field-count builtin
#endif

struct Empty {};
struct Base { int inherited; };
struct Derived : Base {};

class Mixed : public Base {
  int hidden;
protected:
  int protected_field;
public:
  explicit Mixed(int &);
  static int shared;
  void method();
  struct Nested { int a, b; };
  Nested nested;
  int array[8];
  int &ref;
  unsigned flags : 3;
  unsigned : 0;
  union { int injected; };
};

static_assert(reflection<Empty>::variable_count == 0);
static_assert(reflection<Derived>::variable_count == 0);
static_assert(reflection<const volatile Mixed>::variable_count == 4);
static_assert(__is_same(decltype(__builtin_public_field_count(Mixed)),
                        decltype(sizeof(0))));

template <class T> struct Box { T value; };
template <> struct Box<void> {};
static_assert(reflection<Box<Mixed>>::variable_count == 1);
static_assert(reflection<Box<void>>::variable_count == 0);

struct Forward;
struct Forward { int one; };
static_assert(reflection<Forward>::variable_count == 1);

struct Incomplete; // expected-note {{forward declaration of 'Incomplete'}}
union U { int one; };
int incomplete = __builtin_public_field_count(Incomplete); // expected-error {{requires a complete struct or class type, got 'Incomplete'}}
int scalar = __builtin_public_field_count(int); // expected-error {{requires a complete struct or class type, got 'int'}}
int ref = __builtin_public_field_count(Base &); // expected-error {{requires a complete struct or class type, got 'Base &'}}
int variant = __builtin_public_field_count(U); // expected-error {{requires a complete struct or class type, got 'U'}}
