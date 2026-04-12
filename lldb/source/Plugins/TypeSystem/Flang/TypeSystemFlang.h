//===-- TypeSystemFlang.h ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_TYPESYSTEM_FLANG_TYPESYSTEMFLANG_H
#define LLDB_SOURCE_PLUGINS_TYPESYSTEM_FLANG_TYPESYSTEMFLANG_H

#include "lldb/Symbol/CompilerType.h"
#include "lldb/Symbol/Type.h"
#include "lldb/Symbol/TypeSystem.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/lldb-enumerations.h"
#include "lldb/lldb-types.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/Support/Error.h"
#include <optional>

class DWARFASTParserFlang;

namespace lldb_private {

enum class FlangTypeKind {
  eInvalid,
  eVoid,
  eInteger,
  eReal,
  eLogical,
  eComplex,
  eCharacter,
  ePointer,
  eStructure,
  eArray,
};

struct FlangType;

struct FlangFieldInfo {
  ConstString name;
  FlangType *type = nullptr;
  uint64_t bit_offset = 0;
};

struct FlangArrayDimension {
  int64_t lower_bound = 1; // fortran arrays default to 1-based
  uint64_t count = 0;
};

struct FlangType {
  FlangTypeKind kind = FlangTypeKind::eInvalid;
  uint32_t bit_size = 0;
  ConstString name;

  FlangType *element_type = nullptr;
  std::vector<FlangFieldInfo> fields;
  std::vector<FlangArrayDimension> dims;
  uint64_t string_length = 0;
  bool is_complete = true;

  FlangType() = default;

  FlangType(FlangTypeKind kind, uint32_t bit_size, ConstString name)
      : kind(kind), bit_size(bit_size), name(name) {}
};

class TypeSystemFlang : public TypeSystem {
  static char ID;

public:
  TypeSystemFlang() = default;
  ~TypeSystemFlang() override;

  static void Initialize();
  static void Terminate();

  // PluginInterface functions
  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }
  static llvm::StringRef GetPluginNameStatic() { return "flang"; }

  static LanguageSet GetSupportedLanguagesForTypes();
  static LanguageSet GetSupportedLanguagesForExpressions();

  static lldb::TypeSystemSP CreateInstance(lldb::LanguageType language,
                                           Module *module, Target *target);

  CompilerType
  GetBuiltinTypeForDWARFEncodingAndBitSize(llvm::StringRef type_name,
                                           uint32_t dw_ate, uint32_t bit_size);

  FlangType *CreateArrayType(FlangType *element,
                             std::vector<FlangArrayDimension> dims,
                             uint32_t bit_size, ConstString name);
  FlangType *CreateStructureType(ConstString name, uint32_t bit_size);
  void AddFieldToStructure(FlangType *struct_type, ConstString name,
                           FlangType *field_type, uint64_t bit_offset);
  FlangType *CreatePointerType(FlangType *pointee, uint32_t pointer_bit_size);
  FlangType *CreateCharacterType(uint64_t length, uint32_t bit_size,
                                 ConstString name);
  FlangType *CreateComplexType(uint32_t bit_size, ConstString name);

  CompilerType GetCompilerType(FlangType *ft);

  plugin::dwarf::DWARFASTParser *GetDWARFParser() override;

  bool SupportsLanguage(lldb::LanguageType language) override;

  // LLVM RTTI support
  // llvm casting support
  bool isA(const void *ClassID) const override { return ClassID == &ID; }
  static bool classof(const TypeSystem *ts) { return ts->isA(&ID); }

  /// Free up any resources associated with this TypeSystem.  Done before
  /// removing all the TypeSystems from the TypeSystemMap.
  void Finalize() override;

  // CompilerDecl functions
  ConstString DeclGetName(void *opaque_decl) override {
    return ConstString{"decl-get-name"};
  }

  ConstString DeclGetMangledName(void *opaque_decl) override {
    return ConstString{"decl-get-mangled-name"};
  }

  CompilerType GetTypeForDecl(void *opaque_decl) override {
    return CompilerType();
  }

  // CompilerDeclContext functions

  ConstString DeclContextGetName(void *opaque_decl_ctx) override {
    return ConstString{"decl-ctx-get-name"};
  }

  ConstString DeclContextGetScopeQualifiedName(void *opaque_decl_ctx) override {
    return ConstString{"decl-ctx-scope-qual-name"};
  }

  bool DeclContextIsClassMethod(void *opaque_decl_ctx) override {
    return false;
  }

  bool DeclContextIsContainedInLookup(void *opaque_decl_ctx,
                                      void *other_opaque_decl_ctx) override {
    return false;
  }

  lldb::LanguageType DeclContextGetLanguage(void *opaque_decl_ctx) override {
    return lldb::eLanguageTypeFortran95;
  }

  // Tests
#ifndef NDEBUG
  /// Verify the integrity of the type to catch CompilerTypes that mix
  /// and match invalid TypeSystem/Opaque type pairs.
  bool Verify(lldb::opaque_compiler_type_t type) override;
#endif

  bool IsArrayType(lldb::opaque_compiler_type_t type,
                   CompilerType *element_type, uint64_t *size,
                   bool *is_incomplete) override;

  bool IsAggregateType(lldb::opaque_compiler_type_t type) override;

  bool IsCharType(lldb::opaque_compiler_type_t type) override;

  bool IsCompleteType(lldb::opaque_compiler_type_t type) override;
  bool IsDefined(lldb::opaque_compiler_type_t type) override;
  bool IsFloatingPointType(lldb::opaque_compiler_type_t type) override;

  bool IsFunctionType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  size_t
  GetNumberOfFunctionArguments(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  CompilerType GetFunctionArgumentAtIndex(lldb::opaque_compiler_type_t type,
                                          const size_t index) override {
    return CompilerType();
  }

  bool IsFunctionPointerType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsMemberFunctionPointerType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsBlockPointerType(lldb::opaque_compiler_type_t type,
                          CompilerType *function_pointer_type_ptr) override {
    return false;
  }

  bool IsIntegerType(lldb::opaque_compiler_type_t type,
                     bool &is_signed) override;

  bool IsScopedEnumerationType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsPossibleDynamicType(lldb::opaque_compiler_type_t type,
                             CompilerType *target_type, bool check_cplusplus,
                             bool check_objc) override {
    return false;
  }

  bool IsPointerType(lldb::opaque_compiler_type_t type,
                     CompilerType *pointee_type) override;

  bool IsScalarType(lldb::opaque_compiler_type_t type) override;

  bool IsVoidType(lldb::opaque_compiler_type_t type) override;

  bool CanPassInRegisters(const CompilerType &type) override { return true; }

  // Type Completion

  bool GetCompleteType(lldb::opaque_compiler_type_t type) override;

  bool IsForcefullyCompleted(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  // AST related queries

  uint32_t GetPointerByteSize() override { return 8; }

  CompilerType GetPointerDiffType(bool is_signed) override {
    return CompilerType();
  }

  unsigned GetPtrAuthKey(lldb::opaque_compiler_type_t type) override {
    return 0;
  }

  unsigned GetPtrAuthDiscriminator(lldb::opaque_compiler_type_t type) override {
    return 0;
  }

  bool GetPtrAuthAddressDiversity(lldb::opaque_compiler_type_t type) override {
    return 0;
  }

  // Accessors

  ConstString GetTypeName(lldb::opaque_compiler_type_t type,
                          bool BaseOnly) override;

  ConstString GetDisplayTypeName(lldb::opaque_compiler_type_t type) override;

  ConstString GetMangledTypeName(lldb::opaque_compiler_type_t type) override;

  uint32_t GetTypeInfo(lldb::opaque_compiler_type_t type,
                       CompilerType *pointee_or_element_compiler_type) override;

  lldb::LanguageType
  GetMinimumLanguage(lldb::opaque_compiler_type_t type) override {
    return lldb::eLanguageTypeFortran95;
  }

  lldb::TypeClass GetTypeClass(lldb::opaque_compiler_type_t type) override;

  CompilerType GetArrayElementType(lldb::opaque_compiler_type_t type,
                                   ExecutionContextScope *exe_scope) override;

  CompilerType GetCanonicalType(lldb::opaque_compiler_type_t type) override;

  CompilerType
  GetEnumerationIntegerType(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  // Returns -1 if this isn't a function of if the function doesn't have a
  // prototype Returns a value >= 0 if there is a prototype.
  int GetFunctionArgumentCount(lldb::opaque_compiler_type_t type) override {
    return -1;
  }

  CompilerType GetFunctionArgumentTypeAtIndex(lldb::opaque_compiler_type_t type,
                                              size_t idx) override {
    return CompilerType();
  }

  CompilerType
  GetFunctionReturnType(lldb::opaque_compiler_type_t type) override {
    return CompilerType();
  }

  size_t GetNumMemberFunctions(lldb::opaque_compiler_type_t type) override {
    return 0;
  }

  TypeMemberFunctionImpl
  GetMemberFunctionAtIndex(lldb::opaque_compiler_type_t type,
                           size_t idx) override {
    return TypeMemberFunctionImpl{};
  }

  CompilerType GetPointeeType(lldb::opaque_compiler_type_t type) override;

  CompilerType GetPointerType(lldb::opaque_compiler_type_t type) override;

  // Exploring the type

  const llvm::fltSemantics &GetFloatTypeSemantics(size_t byte_size,
                                                  lldb::Format format) override;

  llvm::Expected<uint64_t>
  GetBitSize(lldb::opaque_compiler_type_t type,
             ExecutionContextScope *exe_scope) override;

  lldb::Encoding GetEncoding(lldb::opaque_compiler_type_t type) override;

  lldb::Format GetFormat(lldb::opaque_compiler_type_t type) override;

  llvm::Expected<uint32_t>
  GetNumChildren(lldb::opaque_compiler_type_t type,
                 bool omit_empty_base_classes,
                 const ExecutionContext *exe_ctx) override;

  lldb::BasicType
  GetBasicTypeEnumeration(lldb::opaque_compiler_type_t type) override;

  uint32_t GetNumFields(lldb::opaque_compiler_type_t type) override;

  CompilerType GetFieldAtIndex(lldb::opaque_compiler_type_t type, size_t idx,
                               std::string &name, uint64_t *bit_offset_ptr,
                               uint32_t *bitfield_bit_size_ptr,
                               bool *is_bitfield_ptr) override;

  uint32_t GetNumDirectBaseClasses(lldb::opaque_compiler_type_t type) override {
    return 0;
  }

  uint32_t
  GetNumVirtualBaseClasses(lldb::opaque_compiler_type_t type) override {
    return 0;
  }

  CompilerType GetDirectBaseClassAtIndex(lldb::opaque_compiler_type_t type,
                                         size_t idx,
                                         uint32_t *bit_offset_ptr) override {
    return CompilerType{};
  }

  CompilerType GetVirtualBaseClassAtIndex(lldb::opaque_compiler_type_t type,
                                          size_t idx,
                                          uint32_t *bit_offset_ptr) override {
    return CompilerType{};
  }

  llvm::Expected<CompilerType>
  GetDereferencedType(lldb::opaque_compiler_type_t type,
                      ExecutionContext *exe_ctx, std::string &deref_name,
                      uint32_t &deref_byte_size, int32_t &deref_byte_offset,
                      ValueObject *valobj, uint64_t &language_flags) override;

  llvm::Expected<CompilerType> GetChildCompilerTypeAtIndex(
      lldb::opaque_compiler_type_t type, ExecutionContext *exe_ctx, size_t idx,
      bool transparent_pointers, bool omit_empty_base_classes,
      bool ignore_array_bounds, std::string &child_name,
      uint32_t &child_byte_size, int32_t &child_byte_offset,
      uint32_t &child_bitfield_bit_size, uint32_t &child_bitfield_bit_offset,
      bool &child_is_base_class, bool &child_is_deref_of_parent,
      ValueObject *valobj, uint64_t &language_flags) override;

  // Lookup a child given a name. This function will match base class names and
  // member member names in "clang_type" only, not descendants.
  llvm::Expected<uint32_t>
  GetIndexOfChildWithName(lldb::opaque_compiler_type_t type,
                          llvm::StringRef name,
                          bool omit_empty_base_classes) override;

  size_t
  GetIndexOfChildMemberWithName(lldb::opaque_compiler_type_t type,
                                llvm::StringRef name,
                                bool omit_empty_base_classes,
                                std::vector<uint32_t> &child_indexes) override;

  bool IsTemplateType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  // TODO: what here?
  void dump(lldb::opaque_compiler_type_t) const override {}

  bool DumpTypeValue(lldb::opaque_compiler_type_t type, Stream &s,
                     lldb::Format format, const DataExtractor &data,
                     lldb::offset_t data_offset, size_t data_byte_size,
                     uint32_t bitfield_bit_size, uint32_t bitfield_bit_offset,
                     ExecutionContextScope *exe_scope) override;

  /// Dump the type to stdout.
  void DumpTypeDescription(
      lldb::opaque_compiler_type_t type,
      lldb::DescriptionLevel level = lldb::eDescriptionLevelFull) override;

  /// Print a description of the type to a stream. The exact implementation
  /// varies, but the expectation is that eDescriptionLevelFull returns a
  /// source-like representation of the type, whereas eDescriptionLevelVerbose
  /// does a dump of the underlying AST if applicable.
  void DumpTypeDescription(
      lldb::opaque_compiler_type_t type, Stream &s,
      lldb::DescriptionLevel level = lldb::eDescriptionLevelFull) override;

  /// Dump a textual representation of the internal TypeSystem state to the
  /// given stream.
  ///
  /// This should not modify the state of the TypeSystem if possible.
  ///
  /// \param[out] output Stream to dup the AST into.
  /// \param[in] filter If empty, dump whole AST. If non-empty, will only
  /// dump decls whose names contain \c filter.
  /// \param[in] show_color If true, prints the AST color-highlighted.
  void Dump(llvm::raw_ostream &output, llvm::StringRef filter,
            bool show_color) override {}

  bool IsRuntimeGeneratedType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsPointerOrReferenceType(lldb::opaque_compiler_type_t type,
                                CompilerType *pointee_type) override;

  unsigned GetTypeQualifiers(lldb::opaque_compiler_type_t type) override {
    return 0;
  }

  std::optional<size_t>
  GetTypeBitAlign(lldb::opaque_compiler_type_t type,
                  ExecutionContextScope *exe_scope) override;

  CompilerType GetBasicTypeFromAST(lldb::BasicType basic_type) override {
    return CompilerType{};
  };

  CompilerType GetBuiltinTypeForEncodingAndBitSize(lldb::Encoding encoding,
                                                   size_t bit_size) override {
    return CompilerType{};
  }

  bool IsBeingDefined(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsConst(lldb::opaque_compiler_type_t type) override { return false; }

  uint32_t IsHomogeneousAggregate(lldb::opaque_compiler_type_t type,
                                  CompilerType *base_type_ptr) override {
    return 0;
  }

  bool IsPolymorphicClass(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  bool IsTypedefType(lldb::opaque_compiler_type_t type) override {
    return false;
  }

  // If the current object represents a typedef type, get the underlying type
  CompilerType GetTypedefedType(lldb::opaque_compiler_type_t type) override {
    return CompilerType{};
  }

  bool IsVectorType(lldb::opaque_compiler_type_t type,
                    CompilerType *element_type, uint64_t *size) override {
    return false;
  }

  CompilerType
  GetFullyUnqualifiedType(lldb::opaque_compiler_type_t type) override;

  CompilerType GetNonReferenceType(lldb::opaque_compiler_type_t type) override {
    return CompilerType{};
  }

  bool IsReferenceType(lldb::opaque_compiler_type_t type,
                       CompilerType *pointee_type, bool *is_rvalue) override {
    return false;
  }

  static llvm::Triple m_triple;
  static lldb::TargetWP m_target_wp;

private:
  std::unique_ptr<DWARFASTParserFlang> m_dwarf_ast_parser_up;
  std::vector<std::unique_ptr<FlangType>> m_types;

  FlangType *GetOrCreateType(FlangTypeKind kind, uint32_t bit_size,
                             ConstString name);

  static FlangType *GetFlangType(lldb::opaque_compiler_type_t type) {
    return static_cast<FlangType *>(type);
  }
};
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_TYPESYSTEM_FLANG_TYPESYSTEMFLANG_H
