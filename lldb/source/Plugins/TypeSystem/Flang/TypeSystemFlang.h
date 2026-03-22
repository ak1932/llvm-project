//===-- TypeSystemFlang.h ---------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_TYPESYSTEM_FLANG_TYPESYSTEMFLANG_H
#define LLDB_SOURCE_PLUGINS_TYPESYSTEM_FLANG_TYPESYSTEMFLANG_H

#include "lldb/Symbol/TypeSystem.h"

namespace lldb_private {
class TypeSystemFlang : public TypeSystem {
    public:
    TypeSystemFlang() = default;
    ~TypeSystemFlang() override = default;

    static void Initialize();
    static void Terminate();

    // PluginInterface functions
    llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }
    static llvm::StringRef GetPluginNameStatic() { return "flang"; }

    static LanguageSet GetSupportedLanguagesForTypes();
    static LanguageSet GetSupportedLanguagesForExpressions();

    static lldb::TypeSystemSP CreateInstance(lldb::LanguageType language,
                                             Module *module, Target *target);
};
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_TYPESYSTEM_FLANG_TYPESYSTEMFLANG_H
