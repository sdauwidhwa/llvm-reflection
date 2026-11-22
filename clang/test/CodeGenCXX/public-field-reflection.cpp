// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -emit-llvm -o - %s | FileCheck %s

// The reflection builtins become ordinary member accesses and string
// literals, so code generation needs no special handling.

#include <clang_reflection.hpp>

struct S { char a; int b; long c; unsigned d : 5; };

// CHECK: @[[NAME:.+]] = private unnamed_addr constant [2 x i8] c"b\00"

// CHECK-LABEL: define {{.*}}i64 @get_c(
// CHECK: %[[C:.+]] = getelementptr inbounds nuw %struct.S, ptr %{{.+}}, i32 0, i32 2
// CHECK: load i64, ptr %[[C]]
extern "C" long get_c(S &s) { return __builtin_public_field_ref(s, 2); }

// A bit-field write through the builtin keeps the bit-field's width.
// CHECK-LABEL: define {{.*}}void @set_d(
// CHECK: getelementptr inbounds nuw %struct.S, ptr %{{.+}}, i32 0, i32 3
// CHECK: and i8 %{{.+}}, 31
// CHECK: and i8 %{{.+}}, -32
extern "C" void set_d(S &s, unsigned v) { __builtin_public_field_ref(s, 3) = v; }

// CHECK-LABEL: define {{.*}}ptr @name_b(
// CHECK: ret ptr @[[NAME]]
extern "C" const char *name_b() { return reflection<S>::variable_name<1>; }

// CHECK-LABEL: define {{.*}}i32 @via_get(
// CHECK: call {{.*}}ptr @[[GET:_ZN10reflectionI1SE3getILm1ERKS0_EEDcOT0_]](
// CHECK: define linkonce_odr {{.*}}ptr @[[GET]](
// CHECK: %[[B:.+]] = getelementptr inbounds nuw %struct.S, ptr %{{.+}}, i32 0, i32 1
// CHECK: ret ptr %[[B]]
extern "C" int via_get(const S &s) { return reflection<S>::get<1>(s); }
