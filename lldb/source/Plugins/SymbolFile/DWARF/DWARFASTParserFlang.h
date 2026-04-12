//===-- DWARFASTParserFlang.h -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_SYMBOLFILE_DWARF_DWARFASTPARSERFLANG_H
#define LLDB_SOURCE_PLUGINS_SYMBOLFILE_DWARF_DWARFASTPARSERFLANG_H

#include "DWARFDIE.h"
#include "Plugins/SymbolFile/DWARF/DWARFASTParser.h"
#include "lldb/Symbol/CompilerType.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/lldb-forward.h"

namespace lldb_private {
class TypeSystemFlang;
}

class DWARFASTParserFlang : public lldb_private::plugin::dwarf::DWARFASTParser {
public:
  DWARFASTParserFlang(lldb_private::TypeSystemFlang &type_system);

  ~DWARFASTParserFlang() override;

  // DWARFASTParser interface.
  lldb::TypeSP
  ParseTypeFromDWARF(const lldb_private::SymbolContext &sc,
                     const lldb_private::plugin::dwarf::DWARFDIE &die,
                     bool *type_is_new_ptr) override;

  lldb_private::Function *
  ParseFunctionFromDWARF(lldb_private::CompileUnit &comp_unit,
                         const lldb_private::plugin::dwarf::DWARFDIE &die,
                         lldb_private::AddressRanges func_ranges) override;

  bool CompleteTypeFromDWARF(
      const lldb_private::plugin::dwarf::DWARFDIE &die,
      lldb_private::Type *type,
      const lldb_private::CompilerType &compiler_type) override {
    return false;
  }

  lldb_private::ConstString ConstructDemangledNameFromDWARF(
      const lldb_private::plugin::dwarf::DWARFDIE &die) override {
    return lldb_private::ConstString{""};
  }

  lldb_private::CompilerDecl GetDeclForUIDFromDWARF(
      const lldb_private::plugin::dwarf::DWARFDIE &die) override {
    return lldb_private::CompilerDecl{};
  }

  lldb_private::CompilerDeclContext GetDeclContextForUIDFromDWARF(
      const lldb_private::plugin::dwarf::DWARFDIE &die) override {
    return lldb_private::CompilerDeclContext{};
  }

  lldb_private::CompilerDeclContext GetDeclContextContainingUIDFromDWARF(
      const lldb_private::plugin::dwarf::DWARFDIE &die) override {
    return lldb_private::CompilerDeclContext{};
  }

  void EnsureAllDIEsInDeclContextHaveBeenParsed(
      lldb_private::CompilerDeclContext decl_context) override {}

  std::string GetDIEClassTemplateParams(
      lldb_private::plugin::dwarf::DWARFDIE die) override {
    return "";
  }

protected:
  lldb_private::TypeSystemFlang &m_ast;

private:
  lldb::TypeSP ParseBaseType(const lldb_private::plugin::dwarf::DWARFDIE &die);
  lldb::TypeSP
  ParseStringType(const lldb_private::plugin::dwarf::DWARFDIE &die);
  lldb::TypeSP
  ParsePointerType(const lldb_private::plugin::dwarf::DWARFDIE &die);
  lldb::TypeSP
  ParseStructureType(const lldb_private::SymbolContext &sc,
                     const lldb_private::plugin::dwarf::DWARFDIE &die);
  lldb::TypeSP ParseArrayType(const lldb_private::plugin::dwarf::DWARFDIE &die);
};

#endif
