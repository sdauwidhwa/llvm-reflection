// RUN: split-file %s %t
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -emit-llvm-bc %t/a.cpp -o %t/a.bc
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -emit-llvm-bc %t/b.cpp -o %t/b.bc
// RUN: llvm-link %t/a.bc %t/b.bc -S -o - | FileCheck %s

// CHECK: @[[COUNT:[^ ]+]] = linkonce_odr {{.*}}constant i64 2
// CHECK-LABEL: define {{.*}}@first(
// CHECK: ret ptr @[[COUNT]]
// CHECK-LABEL: define {{.*}}@second(
// CHECK: ret ptr @[[COUNT]]

//--- shared.hpp
#ifndef SHARED_HPP
#define SHARED_HPP
#include <clang_reflection.hpp>
struct Shared { int a, b; };
#endif

//--- a.cpp
#include "shared.hpp"
extern "C" const void *first() { return &reflection<Shared>::variable_count; }

//--- b.cpp
#include "shared.hpp"
extern "C" const void *second() { return &reflection<Shared>::variable_count; }
