//===--- PublicFieldReflection.cpp - Public field reflection builtins -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the semantic analysis shared by the public field reflection
// builtins. See PublicFieldReflection.h for the list of builtins.
//
//===----------------------------------------------------------------------===//

#include "PublicFieldReflection.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Sema/DeclSpec.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/STLExtras.h"

using namespace clang;

bool public_field_reflection::isReflected(const FieldDecl *FD) {
  return FD->getIdentifier() && FD->getAccess() == AS_public;
}

unsigned public_field_reflection::countFields(const CXXRecordDecl *RD) {
  return llvm::count_if(RD->fields(), isReflected);
}

const CXXRecordDecl *
public_field_reflection::checkRecordType(Sema &S, QualType T,
                                         SourceLocation Loc,
                                         StringRef BuiltinName) {
  if (!T->isStructureOrClassType()) {
    S.Diag(Loc, diag::err_public_field_reflection_type) << BuiltinName << T;
    return nullptr;
  }
  if (S.RequireCompleteType(Loc, T, diag::err_public_field_reflection_type,
                            BuiltinName))
    return nullptr;

  const CXXRecordDecl *RD = T->getAsCXXRecordDecl()->getDefinition();
  // The definition had errors, which were already diagnosed. Don't report a
  // count or a field for a class whose members may be missing.
  if (RD->isInvalidDecl())
    return nullptr;
  return RD;
}

const FieldDecl *public_field_reflection::getField(Sema &S,
                                                   const CXXRecordDecl *RD,
                                                   const llvm::APSInt &Index,
                                                   SourceLocation Loc) {
  if (!Index.isNegative()) {
    uint64_t Remaining = Index.getLimitedValue();
    for (const FieldDecl *FD : RD->fields())
      if (isReflected(FD) && Remaining-- == 0)
        return FD;
  }
  S.Diag(Loc, diag::err_public_field_index_out_of_range)
      << toString(Index, 10) << RD << countFields(RD);
  return nullptr;
}

ExprResult public_field_reflection::checkBuiltinCall(Sema &S,
                                                     unsigned BuiltinID,
                                                     CallExpr *TheCall) {
  ASTContext &Context = S.getASTContext();
  if (S.checkArgCount(TheCall, 2))
    return ExprError();

  for (unsigned I = 0; I != 2; ++I) {
    ExprResult Arg = S.CheckPlaceholderExpr(TheCall->getArg(I));
    if (Arg.isInvalid())
      return ExprError();
    TheCall->setArg(I, Arg.get());
  }
  Expr *Object = TheCall->getArg(0);
  Expr *IndexArg = TheCall->getArg(1);

  // In a template the index can be a template parameter, such as one element
  // of an index_sequence pack. Keep the call but give it a dependent type, so
  // nothing is deduced from it yet. Instantiation rebuilds the call and checks
  // it again once the index is known.
  if (Object->isTypeDependent() || IndexArg->isTypeDependent() ||
      IndexArg->isValueDependent())
    return CallExpr::Create(
        Context, TheCall->getCallee(),
        ArrayRef<Expr *>(TheCall->getArgs(), TheCall->getNumArgs()),
        Context.DependentTy, VK_PRValue, TheCall->getRParenLoc(),
        TheCall->getStoredFPFeaturesOrDefault(), /*MinNumArgs=*/0,
        TheCall->getADLCallKind());

  std::string BuiltinName = Context.BuiltinInfo.getName(BuiltinID);

  // _ref takes the object itself. _name and _is_bitfield take a pointer that
  // only names the class: it is never evaluated.
  QualType RecordTy = Object->getType();
  if (BuiltinID != Builtin::BI__builtin_public_field_ref) {
    const auto *PT = RecordTy->getAs<PointerType>();
    if (!PT) {
      S.Diag(Object->getBeginLoc(), diag::err_public_field_reflection_pointer)
          << BuiltinName << RecordTy << Object->getSourceRange();
      return ExprError();
    }
    RecordTy = PT->getPointeeType();
  }

  const CXXRecordDecl *RD =
      checkRecordType(S, RecordTy, Object->getBeginLoc(), BuiltinName);
  if (!RD)
    return ExprError();

  llvm::APSInt Index;
  if (S.VerifyIntegerConstantExpression(IndexArg, &Index).isInvalid())
    return ExprError();

  const FieldDecl *FD = getField(S, RD, Index, IndexArg->getBeginLoc());
  if (!FD)
    return ExprError();

  SourceLocation Loc = TheCall->getBeginLoc();
  switch (BuiltinID) {
  case Builtin::BI__builtin_public_field_name: {
    StringRef Name = FD->getName();
    return StringLiteral::Create(
        Context, Name, StringLiteralKind::Ordinary, /*Pascal=*/false,
        Context.getStringLiteralArrayType(Context.CharTy, Name.size()), Loc);
  }

  case Builtin::BI__builtin_public_field_is_bitfield:
    return CXXBoolLiteralExpr::Create(Context, FD->isBitField(), Context.BoolTy,
                                      Loc);

  case Builtin::BI__builtin_public_field_ref: {
    // Build exactly what 'Object.field' would build, so constant evaluation,
    // code generation and bit-field writes need no special handling.
    auto *Field = const_cast<FieldDecl *>(FD);
    if (S.DiagnoseUseOfDecl(Field, Loc))
      return ExprError();
    if (Object->isPRValue()) {
      ExprResult Materialized = S.TemporaryMaterializationConversion(Object);
      if (Materialized.isInvalid())
        return ExprError();
      Object = Materialized.get();
    }
    return S.BuildFieldReferenceExpr(
        Object, /*IsArrow=*/false, Loc, CXXScopeSpec(), Field,
        DeclAccessPair::make(Field, Field->getAccess()),
        DeclarationNameInfo(Field->getDeclName(), Loc));
  }
  }
  llvm_unreachable("not a public field reflection builtin");
}
