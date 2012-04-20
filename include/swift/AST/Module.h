//===--- Module.h - Swift Language Module ASTs ------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file defines the Module class and its subclasses.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_MODULE_H
#define SWIFT_MODULE_H

#include "swift/AST/DeclContext.h"
#include "swift/AST/Identifier.h"
#include "swift/Basic/SourceLoc.h"
#include "llvm/ADT/ArrayRef.h"

namespace swift {
  class ASTContext;
  class BraceStmt;
  class Component;
  class Decl;
  class ExtensionDecl;
  class OneOfElementDecl;
  class NameAliasType;
  class Type;
  class TypeAliasDecl;
  class LookupCache;
  class ValueDecl;
  class IdentifierType;
  
  /// NLKind - This is a specifier for the kind of name lookup being performed
  /// by various query methods.
  enum class NLKind {
    UnqualifiedLookup,
    QualifiedLookup
  };
 
/// Module - A unit of modularity.  The current translation unit is a
/// module, as is an imported module.
class Module : public DeclContext {
protected:
  void *LookupCachePimpl;
  void *ExtensionCachePimpl;
  Component *Comp;
public:
  ASTContext &Ctx;
  Identifier Name;
  bool IsMainModule;
  bool IsReplModule;
  
  //===--------------------------------------------------------------------===//
  // AST Phase of Translation
  //===--------------------------------------------------------------------===//
  
  /// ASTStage - Defines what phases of parsing and semantic analysis are
  /// complete for the given AST.  This should only be used for assertions and
  /// verification purposes.
  enum {
    /// Parsing is underway.
    Parsing,
    /// Parsing has completed.
    Parsed,
    /// Name binding has completed.
    NameBound,
    /// Type checking has completed.
    TypeChecked
  } ASTStage;

protected:
  Module(DeclContextKind Kind, Identifier Name, Component *C, ASTContext &Ctx,
         bool IsMainModule, bool IsReplModule)
  : DeclContext(Kind, nullptr), LookupCachePimpl(0), ExtensionCachePimpl(0),
    Comp(C), Ctx(Ctx), Name(Name), IsMainModule(IsMainModule),
    IsReplModule(IsReplModule), ASTStage(Parsing) {
    assert(Comp != nullptr || Kind == DeclContextKind::BuiltinModule);
  }

public:
  typedef ArrayRef<std::pair<Identifier, SourceLoc>> AccessPathTy;

  Component *getComponent() const {
    assert(Comp && "fetching component for the builtin module");
    return Comp;
  }

  /// lookupType - Look up a type at top-level scope (but with the specified 
  /// access path, which may come from an import decl) within the current
  /// module. This does a simple local lookup, not recursively looking  through
  /// imports.  
  TypeAliasDecl *lookupType(AccessPathTy AccessPath, Identifier Name,
                            NLKind LookupKind);
  
  /// lookupValue - Look up a (possibly overloaded) value set at top-level scope
  /// (but with the specified access path, which may come from an import decl)
  /// within the current module. This does a simple local lookup, not
  /// recursively looking through imports.  
  void lookupValue(AccessPathTy AccessPath, Identifier Name, NLKind LookupKind, 
                   SmallVectorImpl<ValueDecl*> &Result);

  /// lookupExtensions - Look up all of the extensions in the module that are
  /// extending the specified type and return a list of them.
  ArrayRef<ExtensionDecl*> lookupExtensions(Type T);
  
  /// lookupGlobalType - Perform a type lookup within the current Module.
  /// Unlike lookupType, this does look through import declarations to resolve
  /// the name.
  TypeAliasDecl *lookupGlobalType(Identifier Name, NLKind LookupKind);

  /// lookupGlobalValue - Perform a value lookup within the current Module.
  /// Unlike lookupValue, this does look through import declarations to resolve
  /// the name.
  void lookupGlobalValue(Identifier Name, NLKind LookupKind, 
                         SmallVectorImpl<ValueDecl*> &Result);

  /// lookupGlobalExtensionMethods - Lookup the extensions members for the
  /// specified BaseType with the specified type, and return them in Result.
  void lookupGlobalExtensionMethods(Type BaseType, Identifier Name,
                                    SmallVectorImpl<ValueDecl*> &Result);

  static bool classof(const Module *M) {
    return true;
  }
  static bool classof(const DeclContext *DC) {
    return DC->isModuleContext();
  }

private:
  // Make placement new and vanilla new/delete illegal for DeclVarNames.
  void *operator new(size_t Bytes) throw() = delete;
  void operator delete(void *Data) throw() = delete;
  void *operator new(size_t Bytes, void *Mem) throw() = delete;
public:
  // Only allow allocation of Modules using the allocator in ASTContext
  // or by doing a placement new.
  void *operator new(size_t Bytes, ASTContext &C,
                     unsigned Alignment = 8);
};
  
/// TranslationUnit - This contains information about all of the decls and
/// external references in a translation unit, which is one file.
class TranslationUnit : public Module {
public:
  typedef std::pair<Module::AccessPathTy, Module*> ImportedModule;
private:
  /// UnresolvedIdentifierTypes - This is a list of scope-qualified dotted types
  /// that were unresolved at the end of the translation unit's parse
  /// phase.
  ArrayRef<IdentifierType*> UnresolvedIdentifierTypes;
  
  /// ImportedModules - This is the list of modules that are imported by this
  /// module.  This is filled in by the Name Binding phase.
  ArrayRef<ImportedModule> ImportedModules;

public:

  /// Decls; the list of top-level declarations for a translation unit.
  std::vector<Decl*> Decls;
  
  TranslationUnit(Identifier Name, Component *Comp, ASTContext &C,
                  bool IsMainModule, bool IsReplModule)
    : Module(DeclContextKind::TranslationUnit, Name, Comp, C, IsMainModule,
             IsReplModule) { }
  
  /// getUnresolvedIdentifierTypes - This is a list of scope-qualified types
  /// that were unresolved at the end of the translation unit's parse
  /// phase.
  ArrayRef<IdentifierType*> getUnresolvedIdentifierTypes() const {
    assert(ASTStage == Parsed);
    return UnresolvedIdentifierTypes;
  }
  void setUnresolvedIdentifierTypes(ArrayRef<IdentifierType*> T) {
    assert(ASTStage == Parsing);
    UnresolvedIdentifierTypes = T;
  }

  /// ImportedModules - This is the list of modules that are imported by this
  /// module.  This is filled in as the first thing that the Name Binding phase
  /// does.
  ArrayRef<ImportedModule> getImportedModules() const {
    assert(ASTStage >= Parsed);
    return ImportedModules;
  }
  void setImportedModules(ArrayRef<ImportedModule> IM) {
    assert(ASTStage == Parsed);
    ImportedModules = IM;
  }

  void clearLookupCache();

  void dump() const;
  
  // Implement isa/cast/dyncast/etc.
  static bool classof(const TranslationUnit *TU) { return true; }
  static bool classof(const DeclContext *DC) {
    return DC->getContextKind() == DeclContextKind::TranslationUnit;
  }
};

  
/// BuiltinModule - This module represents the compiler's implicitly generated
/// declarations in the builtin module.
class BuiltinModule : public Module {
public:
  BuiltinModule(Identifier Name, ASTContext &Ctx)
    : Module(DeclContextKind::BuiltinModule, Name, nullptr, Ctx, false, false) {
    // The Builtin module is always well formed.
    ASTStage = TypeChecked;
  }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const BuiltinModule *TU) { return true; }
  static bool classof(const DeclContext *DC) {
    return DC->getContextKind() == DeclContextKind::BuiltinModule;
  }
};
  
} // end namespace swift

#endif
