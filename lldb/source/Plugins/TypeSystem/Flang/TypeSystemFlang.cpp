#include "lldb/Core/PluginManager.h"
#include "lldb/lldb-enumerations.h"
#include "Plugins/SymbolFile/DWARF/DWARFASTParserFlang.h"
#include "TypeSystemFlang.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::plugin::dwarf;

LLDB_PLUGIN_DEFINE(TypeSystemFlang);


void TypeSystemFlang::Finalize() {
    m_types.clear();
}

char TypeSystemFlang::ID;

DWARFASTParser *TypeSystemFlang::GetDWARFParser() {
  if (!m_dwarf_ast_parser_up)
    m_dwarf_ast_parser_up = std::make_unique<DWARFASTParserFlang>(*this);
  return m_dwarf_ast_parser_up.get();
}

TypeSystemFlang::~TypeSystemFlang() { Finalize(); }

LanguageSet TypeSystemFlang::GetSupportedLanguagesForTypes() {
  LanguageSet languages;
  languages.Insert(lldb::eLanguageTypeFortran77);
  languages.Insert(lldb::eLanguageTypeFortran90);
  languages.Insert(lldb::eLanguageTypeFortran95);
  return languages;
}

LanguageSet TypeSystemFlang::GetSupportedLanguagesForExpressions() {
  LanguageSet languages;
  languages.Insert(lldb::eLanguageTypeFortran77);
  languages.Insert(lldb::eLanguageTypeFortran90);
  languages.Insert(lldb::eLanguageTypeFortran95);
  return languages;
}


static inline bool
TypeSystemFlangSupportsLanguage(lldb::LanguageType language) {
  return language == lldb::eLanguageTypeFortran77 || 
          language == lldb::eLanguageTypeFortran90 || 
          language == lldb::eLanguageTypeFortran95; 
}

  // TypeSystems can support more than one language
bool TypeSystemFlang::SupportsLanguage(lldb::LanguageType language) { return TypeSystemFlangSupportsLanguage(language); };

lldb::TypeSystemSP TypeSystemFlang::CreateInstance(lldb::LanguageType language,
                                                   lldb_private::Module *module,
                                                   Target *target) {
  if (!TypeSystemFlangSupportsLanguage(language))
    return lldb::TypeSystemSP();

  return std::make_shared<TypeSystemFlang>();
}


void TypeSystemFlang::Initialize() {
  PluginManager::RegisterPlugin(
      GetPluginNameStatic(), "flang base AST context plug-in", CreateInstance,
      GetSupportedLanguagesForTypes(), GetSupportedLanguagesForExpressions());
}

void TypeSystemFlang::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}


FlangType *TypeSystemFlang::GetOrCreateType(FlangTypeKind kind,
                                            uint32_t bit_size,
                                            ConstString name) {
  for (auto &t : m_types) {
    if (t->kind == kind && t->bit_size == bit_size && t->name == name)
      return t.get();
  }
  m_types.push_back(std::make_unique<FlangType>(
      FlangType{kind, bit_size, name}));
  return m_types.back().get();
}

CompilerType TypeSystemFlang::GetBuiltinTypeForDWARFEncodingAndBitSize(
    llvm::StringRef type_name, uint32_t dw_ate, uint32_t bit_size) {
  FlangTypeKind kind = FlangTypeKind::eInvalid;

  switch (dw_ate) {
  case llvm::dwarf::DW_ATE_boolean:
    kind = FlangTypeKind::eLogical;
    break;
  case llvm::dwarf::DW_ATE_signed:
  case llvm::dwarf::DW_ATE_unsigned:
    kind = FlangTypeKind::eInteger;
    break;
  case llvm::dwarf::DW_ATE_float:
    kind = FlangTypeKind::eReal;
    break;
  default:
    return CompilerType();
  }

  FlangType *ft =
      GetOrCreateType(kind, bit_size, ConstString(type_name));
  return CompilerType(weak_from_this(), static_cast<void *>(ft));
}

#ifndef NDEBUG
bool TypeSystemFlang::Verify(lldb::opaque_compiler_type_t type) {
  if (!type)
    return true;
  for (auto &t : m_types) {
    if (t.get() == static_cast<FlangType *>(type))
      return true;
  }
  return false;
}
#endif

bool TypeSystemFlang::IsCompleteType(lldb::opaque_compiler_type_t type) {
  return type != nullptr;
}

bool TypeSystemFlang::IsDefined(lldb::opaque_compiler_type_t type) {
  return type != nullptr;
}

bool TypeSystemFlang::IsFloatingPointType(lldb::opaque_compiler_type_t type) {
  if (auto *ft = GetFlangType(type))
    return ft->kind == FlangTypeKind::eReal;
  return false;
}

bool TypeSystemFlang::IsIntegerType(lldb::opaque_compiler_type_t type,
                                    bool &is_signed) {
  if (auto *ft = GetFlangType(type)) {
    if (ft->kind == FlangTypeKind::eInteger) {
      is_signed = true;
      return true;
    }
  }
  return false;
}

bool TypeSystemFlang::IsScalarType(lldb::opaque_compiler_type_t type) {
  if (auto *ft = GetFlangType(type)) {
    switch (ft->kind) {
    case FlangTypeKind::eInteger:
    case FlangTypeKind::eReal:
    case FlangTypeKind::eLogical:
      return true;
    default:
      return false;
    }
  }
  return false;
}
bool TypeSystemFlang::IsVoidType(lldb::opaque_compiler_type_t type) {
  if (auto *ft = GetFlangType(type))
    return ft->kind == FlangTypeKind::eVoid;
  return false;
}


ConstString TypeSystemFlang::GetTypeName(lldb::opaque_compiler_type_t type,
                                         bool BaseOnly) {
  if (auto *ft = GetFlangType(type))
    return ft->name;
  return ConstString{""};
}
ConstString
TypeSystemFlang::GetDisplayTypeName(lldb::opaque_compiler_type_t type) {
  if (auto *ft = GetFlangType(type))
    return ft->name;
  return ConstString{""};
}

ConstString
TypeSystemFlang::GetMangledTypeName(lldb::opaque_compiler_type_t type) {
  return GetTypeName(type, false);
}

lldb::TypeClass
TypeSystemFlang::GetTypeClass(lldb::opaque_compiler_type_t type) {
  if (type)
    return lldb::eTypeClassBuiltin;
  return lldb::eTypeClassInvalid;
}


llvm::Expected<uint64_t>
TypeSystemFlang::GetBitSize(lldb::opaque_compiler_type_t type,
                            ExecutionContextScope *exe_scope) {
  if (auto *ft = GetFlangType(type))
    return ft->bit_size;
  return llvm::createStringError("invalid type");
}

lldb::Encoding
TypeSystemFlang::GetEncoding(lldb::opaque_compiler_type_t type) {
  if (auto *ft = GetFlangType(type)) {
    switch (ft->kind) {
    case FlangTypeKind::eInteger:
      return lldb::eEncodingSint;
    case FlangTypeKind::eReal:
      return lldb::eEncodingIEEE754;
    case FlangTypeKind::eLogical:
      return lldb::eEncodingUint;
    default:
      break;
    }
  }
  return lldb::eEncodingInvalid;
}

lldb::Format TypeSystemFlang::GetFormat(lldb::opaque_compiler_type_t type) {
  if (auto *ft = GetFlangType(type)) {
    switch (ft->kind) {
    case FlangTypeKind::eInteger:
      return lldb::eFormatDecimal;
    case FlangTypeKind::eReal:
      return lldb::eFormatFloat;
    case FlangTypeKind::eLogical:
      return lldb::eFormatBoolean;
    default:
      break;
    }
  }
  return lldb::eFormatDefault;
}

llvm::Expected<uint32_t>
TypeSystemFlang::GetNumChildren(lldb::opaque_compiler_type_t type,
                                bool omit_empty_base_classes,
                                const ExecutionContext *exe_ctx) {
  return 0;
}

lldb::BasicType
TypeSystemFlang::GetBasicTypeEnumeration(lldb::opaque_compiler_type_t type) {
  if (auto *ft = GetFlangType(type)) {
    switch (ft->kind) {
    case FlangTypeKind::eInteger:
      switch (ft->bit_size) {
      case 8:
        return lldb::eBasicTypeChar;
      case 16:
        return lldb::eBasicTypeShort;
      case 32:
        return lldb::eBasicTypeInt;
      case 64:
        return lldb::eBasicTypeLongLong;
      default:
        return lldb::eBasicTypeInt;
      }
    case FlangTypeKind::eReal:
      switch (ft->bit_size) {
      case 32:
        return lldb::eBasicTypeFloat;
      case 64:
        return lldb::eBasicTypeDouble;
      default:
        return lldb::eBasicTypeFloat;
      }
    case FlangTypeKind::eLogical:
      return lldb::eBasicTypeBool;
    default:
      break;
    }
  }
  return lldb::eBasicTypeInvalid;
}


std::optional<size_t>
TypeSystemFlang::GetTypeBitAlign(lldb::opaque_compiler_type_t type,
                                 ExecutionContextScope *exe_scope) {
  if (auto *ft = GetFlangType(type))
    return ft->bit_size;
  return std::nullopt;
}
void TypeSystemFlang::DumpTypeDescription(lldb::opaque_compiler_type_t type,
                                          Stream &s,
                                          lldb::DescriptionLevel level) {
  if (auto *ft = GetFlangType(type))
    s.PutCString(ft->name.GetStringRef());
}
