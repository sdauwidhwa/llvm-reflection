// RUN: %clang_cc1 -std=c++17 -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++20 -fsyntax-only -verify %s
// RUN: %clang_cc1 -std=c++20 -fexperimental-new-constant-interpreter -fsyntax-only -verify %s

#include <clang_reflection.hpp>

#if !__has_builtin(__builtin_public_field_type) ||                            \
    !__has_builtin(__builtin_public_field_name) ||                            \
    !__has_builtin(__builtin_public_field_is_bitfield) ||                     \
    !__has_builtin(__builtin_public_field_ref)
#error missing field reflection builtins
#endif

using size_t = decltype(sizeof(0));

template <class T, class U> constexpr bool same = __is_same(T, U);

constexpr bool equal(const char *a, const char *b) {
  while (*a && *a == *b) {
    ++a;
    ++b;
  }
  return *a == *b;
}

struct Base { int inherited; };

class Mixed : public Base {
  int hidden;
public:
  int first;
protected:
  int guarded;
public:
  const char *second;
  static int shared;
  int &ref;
  unsigned flags : 3;
  unsigned : 0;
  union { int injected; };
  mutable long last;

  constexpr Mixed(int &r)
      : Base{0}, hidden(1), first(2), guarded(3), second("two"), ref(r),
        flags(5), injected(6), last(7) {}
};

using R = reflection<Mixed>;
static_assert(R::variable_count == 5);

// Index I names the same member in every query.
static_assert(same<R::variable_type<0>, int>);
static_assert(same<R::variable_type<1>, const char *>);
static_assert(same<R::variable_type<2>, int &>);
static_assert(same<R::variable_type<3>, unsigned>);
static_assert(same<R::variable_type<4>, long>);
static_assert(same<reflection<const Mixed>::variable_type<0>, int>);

static_assert(equal(R::variable_name<0>, "first"));
static_assert(equal(R::variable_name<1>, "second"));
static_assert(equal(R::variable_name<2>, "ref"));
static_assert(equal(R::variable_name<3>, "flags"));
static_assert(equal(R::variable_name<4>, "last"));
static_assert(equal(reflection<const volatile Mixed>::variable_name<1>, "second"));
static_assert(sizeof(__builtin_public_field_name((Mixed *)nullptr, 0)) == 6);

static_assert(!R::is_bit_field<0> && !R::is_bit_field<2> && R::is_bit_field<3>);

// Reading members in constant expressions.
int global = 42;
constexpr Mixed constant(global);
static_assert(R::ref<0>(constant) == 2);
static_assert(&R::ref<2>(constant) == &global);
static_assert(R::get<0>(constant) == 2);
static_assert(equal(R::get<1>(constant), "two"));
static_assert(R::get<3>(constant) == 5);

// ref<I> follows std::get: the value category and constness of the object
// carry over, a reference member is always an lvalue, and 'mutable' drops
// const.
Mixed &lvalue();
const Mixed &const_lvalue();
Mixed &&xvalue();
static_assert(same<decltype(R::ref<0>(lvalue())), int &>);
static_assert(same<decltype(R::ref<0>(const_lvalue())), const int &>);
static_assert(same<decltype(R::ref<0>(xvalue())), int &&>);
static_assert(same<decltype(R::ref<2>(xvalue())), int &>);
static_assert(same<decltype(R::ref<4>(const_lvalue())), long &>);

// A bit-field has no ref.
template <size_t I, class = void> constexpr bool has_ref = false;
template <size_t I>
constexpr bool has_ref<I, decltype((void)R::ref<I>(lvalue()))> = true;
static_assert(has_ref<0> && has_ref<1> && has_ref<2> && !has_ref<3> &&
              has_ref<4>);

// get<I> returns a copy without const or reference, bit-fields included.
static_assert(same<decltype(R::get<0>(const_lvalue())), int>);
static_assert(same<decltype(R::get<1>(xvalue())), const char *>);
static_assert(same<decltype(R::get<2>(lvalue())), int>);
static_assert(same<decltype(R::get<3>(lvalue())), unsigned>);
static_assert(same<decltype(R::get<4>(const_lvalue())), long>);

struct ConstMember { const int value; };
static_assert(same<decltype(reflection<ConstMember>::get<0>(ConstMember{1})),
                   int>);

// get<I> moves the member out of an rvalue.
struct MoveOnly {
  constexpr MoveOnly(int v) : v(v) {}
  MoveOnly(const MoveOnly &) = delete;
  constexpr MoveOnly(MoveOnly &&) = default;
  constexpr MoveOnly &operator=(MoveOnly &&) = default;
  int v;
};
struct Holder { MoveOnly item; };
static_assert(reflection<Holder>::get<0>(Holder{MoveOnly(8)}).v == 8);

// An array member is reached through ref.
struct WithArray { int values[2]; };
constexpr WithArray with_array{{3, 4}};
static_assert(same<decltype(reflection<WithArray>::ref<0>(with_array)),
                   const int (&)[2]>);
static_assert(reflection<WithArray>::ref<0>(with_array)[1] == 4);

// The builtin itself is a member access expression, including for decltype.
static_assert(same<decltype(__builtin_public_field_ref(lvalue(), 0)), int>);
static_assert(same<decltype((__builtin_public_field_ref(lvalue(), 0))), int &>);
static_assert(same<decltype((__builtin_public_field_ref(Mixed(global), 0))), int &&>);

// Writing members through ref and set. set reaches a bit-field, keeping its
// width, and assigns through a reference member.
constexpr int write_members() {
  int target = 0;
  Mixed m(target);
  R::ref<0>(m) = 10;
  R::set<2>(m, 20);
  R::set<3>(m, 6);
  __builtin_public_field_ref(m, 3) += 1;
  return R::get<0>(m) + target + R::get<3>(m);
}
static_assert(write_members() == 37);

constexpr unsigned truncated_bit_field() {
  int target = 0;
  Mixed m(target);
  R::set<3>(m, 9u); // 0b1001 in a 3-bit field
  return R::get<3>(m);
}
static_assert(truncated_bit_field() == 1);

constexpr int set_moves() {
  Holder h{MoveOnly(1)};
  reflection<Holder>::set<0>(h, MoveOnly(5));
  return h.item.v;
}
static_assert(set_moves() == 5);

// Indices unpacked from an integer sequence.
template <class T, T... Is> struct sequence {};
template <class T>
using indices =
    __make_integer_seq<sequence, size_t, reflection<T>::variable_count>;

struct Point { int x; long y; short z; };

template <class T, size_t... Is>
constexpr long sum(const T &object, sequence<size_t, Is...>) {
  return (0 + ... + reflection<T>::template get<Is>(object));
}
static_assert(sum(Point{1, 2, 3}, indices<Point>{}) == 6);

template <class T, size_t... Is>
constexpr size_t name_lengths(sequence<size_t, Is...>) {
  return (0 + ... + __builtin_strlen(reflection<T>::template variable_name<Is>));
}
static_assert(name_lengths<Point>(indices<Point>{}) == 3);
static_assert(name_lengths<Mixed>(indices<Mixed>{}) == 23);

template <class... Ts> struct type_list {};
template <class T, size_t... Is>
type_list<typename reflection<T>::template variable_type<Is>...>
types_of(sequence<size_t, Is...>);
static_assert(same<decltype(types_of<Point>(indices<Point>{})),
                   type_list<int, long, short>>);
static_assert(same<decltype(types_of<Mixed>(indices<Mixed>{})),
                   type_list<int, const char *, int &, unsigned, long>>);

// Only the index is dependent: the call is checked again on instantiation.
template <size_t... Is>
constexpr long point_sum(Point p, sequence<size_t, Is...>) {
  return (0 + ... + __builtin_public_field_ref(p, Is));
}
static_assert(point_sum(Point{4, 5, 6}, sequence<size_t, 0, 1, 2>{}) == 15);

template <size_t I> constexpr auto &member(Point &p) {
  return __builtin_public_field_ref(p, I);
}
template <size_t I> constexpr const char *name() {
  return __builtin_public_field_name((Point *)nullptr, I);
}
constexpr long write_y() {
  Point p{1, 2, 3};
  member<1>(p) = 9;
  return p.y;
}
static_assert(write_y() == 9);
static_assert(equal(name<2>(), "z"));

// Class template specializations are instantiated on demand.
template <class T> struct Box { T value; int extra; };
static_assert(same<reflection<Box<Point>>::variable_type<0>, Point>);
static_assert(equal(reflection<Box<Point>>::variable_name<1>, "extra"));

// An invalid type or index is a substitution failure.
template <class T, size_t I, class = __builtin_public_field_type<T, I>>
constexpr bool has_field(int) { return true; }
template <class T, size_t I> constexpr bool has_field(...) { return false; }
static_assert(has_field<Point, 2>(0));
static_assert(!has_field<Point, 3>(0));
static_assert(!has_field<int, 0>(0));

// Errors.
struct Incomplete; // expected-note 2 {{forward declaration of 'Incomplete'}}
union Union { int a; };
struct Empty {};

void errors(Point p, Point *pp, int i) { // expected-note {{declared here}}
  (void)__builtin_public_field_ref(p, 3); // expected-error {{index 3 is out of range for 'Point', which has 3 public fields}}
  (void)__builtin_public_field_ref(p, -1); // expected-error {{index -1 is out of range for 'Point', which has 3 public fields}}
  (void)__builtin_public_field_ref(p, i); // expected-error {{expression is not an integral constant expression}} \
                                          // expected-note {{function parameter 'i' with unknown value cannot be used in a constant expression}}
  (void)__builtin_public_field_ref(p); // expected-error {{too few arguments to function call, expected 2, have 1}}
  (void)__builtin_public_field_ref(pp, 0); // expected-error {{'__builtin_public_field_ref' requires a complete struct or class type, got 'Point *'}}
  (void)__builtin_public_field_name(p, 0); // expected-error {{first argument to '__builtin_public_field_name' must be a pointer to a struct or class type, got 'Point'}}
  (void)__builtin_public_field_name((Union *)nullptr, 0); // expected-error {{'__builtin_public_field_name' requires a complete struct or class type, got 'Union'}}
  (void)__builtin_public_field_is_bitfield((Incomplete *)nullptr, 0); // expected-error {{'__builtin_public_field_is_bitfield' requires a complete struct or class type, got 'Incomplete'}}
  (void)__builtin_public_field_name((Empty *)nullptr, 0); // expected-error {{index 0 is out of range for 'Empty', which has 0 public fields}}
}

using incomplete_type = __builtin_public_field_type<Incomplete, 0>; // expected-error {{'__builtin_public_field_type' requires a complete struct or class type, got 'Incomplete'}}
using scalar_type = __builtin_public_field_type<int, 0>; // expected-error {{'__builtin_public_field_type' requires a complete struct or class type, got 'int'}}
using out_of_range_type = __builtin_public_field_type<Point, 3>; // expected-error {{index 3 is out of range for 'Point', which has 3 public fields}}

template <class T> constexpr int first_member(const T &object) {
  return __builtin_public_field_ref(object, 0); // expected-error {{index 0 is out of range for 'Empty', which has 0 public fields}}
}
int from_empty = first_member(Empty{}); // expected-note {{in instantiation of function template specialization 'first_member<Empty>' requested here}}

struct Other { int x; };
int wrong_object = R::get<0>(Other{1}); // expected-note {{in instantiation of function template specialization 'reflection<Mixed>::get<0}}
// expected-error@clang_reflection.hpp:* {{reflection<T>::get requires an object of type T}}
int wrong_ref_object = R::ref<0>(Other{1}); // expected-note {{in instantiation of function template specialization 'reflection<Mixed>::ref<0}}
// expected-error@clang_reflection.hpp:* {{reflection<T>::ref requires an object of type T}}

void accessor_errors(Mixed &m, WithArray &a) {
  (void)R::ref<3>(m); // expected-error {{no matching function for call to 'ref'}}
  // expected-note@clang_reflection.hpp:* {{candidate template ignored: requirement '!is_bit_field<3}}
  (void)reflection<WithArray>::get<0>(a); // expected-note {{in instantiation of function template specialization 'reflection<WithArray>::get<0}}
  // expected-error@clang_reflection.hpp:* {{reflection<T>::get cannot copy an array member; use ref}}
}
