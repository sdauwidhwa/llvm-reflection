//===--- PublicFieldReflection.h - Public field reflection builtins -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Semantic analysis shared by the builtins behind <clang_reflection.hpp>:
//
//   __builtin_public_field_count(T)             type trait
//   __builtin_public_field_type<T, I>           builtin template
//   __builtin_public_field_name(T *, I)         builtin function
//   __builtin_public_field_is_bitfield(T *, I)  builtin function
//   __builtin_public_field_ref(Object, I)       builtin function
//
// All of them see a class through the same list of fields, so index I names
// the same member everywhere and every index below the count is valid.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SEMA_PUBLICFIELDREFLECTION_H
#define LLVM_CLANG_LIB_SEMA_PUBLICFIELDREFLECTION_H

#include "clang/AST/TypeBase.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Sema/Ownership.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/StringRef.h"

namespace clang {
class CallExpr;
class CXXRecordDecl;
class FieldDecl;
class Sema;

namespace public_field_reflection {

/// Returns true if \p FD is one of the fields the builtins expose: a named,
/// public, non-static data member. Unnamed bit-fields and the unnamed members
/// that hold anonymous structs, anonymous unions and lambda captures are not.
bool isReflected(const FieldDecl *FD);

/// Returns the number of reflected fields declared directly in \p RD.
unsigned countFields(const CXXRecordDecl *RD);

/// Checks that \p T is a complete struct or class type (not a union) and
/// returns its definition. Otherwise diagnoses at \p Loc, naming the builtin
/// \p BuiltinName, and returns null.
const CXXRecordDecl *checkRecordType(Sema &S, QualType T, SourceLocation Loc,
                                     StringRef BuiltinName);

/// Returns the reflected field at \p Index in \p RD. If \p Index is out of
/// range, diagnoses at \p Loc and returns null.
const FieldDecl *getField(Sema &S, const CXXRecordDecl *RD,
                          const llvm::APSInt &Index, SourceLocation Loc);

/// Checks a call to __builtin_public_field_name, _is_bitfield or _ref and
/// returns the expression that replaces it: a string literal, a bool literal,
/// or the member access 'Object.field'. Calls whose index is not yet known
/// are returned with a dependent type and checked again on instantiation.
ExprResult checkBuiltinCall(Sema &S, unsigned BuiltinID, CallExpr *TheCall);

} // namespace public_field_reflection
} // namespace clang

#endif // LLVM_CLANG_LIB_SEMA_PUBLICFIELDREFLECTION_H
