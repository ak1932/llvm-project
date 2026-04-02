#include "lldb/Core/DumpDataExtractor.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Host/StreamFile.h"
#include "lldb/Symbol/CompilerType.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include "Plugins/SymbolFile/DWARF/DWARFASTParserFlang.h"
#include "TypeSystemFlang.h"
#include "lldb/Utility/Stream.h"
#include "lldb/lldb-enumerations.h"
#include "lldb/lldb-types.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include <cassert>

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

bool TypeSystemFlang::SupportsLanguage(lldb::LanguageType language) { return TypeSystemFlangSupportsLanguage(language); }

llvm::Triple TypeSystemFlang::m_triple;
lldb::TargetWP TypeSystemFlang::m_target_wp;
lldb::TypeSystemSP TypeSystemFlang::CreateInstance(lldb::LanguageType language,
                                                   lldb_private::Module *module,
                                                   Target *target) {
  if (!TypeSystemFlangSupportsLanguage(language))
    return lldb::TypeSystemSP();
  ArchSpec arch;
  if (module)
    arch = module->GetArchitecture();
  else if (target)
    arch = target->GetArchitecture();

  if (!arch.IsValid())
    return lldb::TypeSystemSP();

  llvm::Triple triple = arch.GetTriple();
  if (triple.getVendor() == llvm::Triple::Apple &&
      triple.getOS() == llvm::Triple::UnknownOS) {
    if (triple.getArch() == llvm::Triple::arm ||
        triple.getArch() == llvm::Triple::aarch64 ||
        triple.getArch() == llvm::Triple::aarch64_32 ||
        triple.getArch() == llvm::Triple::thumb) {
      triple.setOS(llvm::Triple::IOS);
    } else {
      triple.setOS(llvm::Triple::MacOSX);
    }
  }

  if (module) {
    return std::make_shared<TypeSystemFlang>();
  } else if (target && target->IsValid()) {
    m_target_wp = target->shared_from_this();
    return std::make_shared<TypeSystemFlang>();
  }

  return lldb::TypeSystemSP();
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

CompilerType TypeSystemFlang::GetCompilerType(FlangType *ft) {
  if (!ft)
    return CompilerType();
  return CompilerType(weak_from_this(), static_cast<void *>(ft));
}

FlangType *TypeSystemFlang::CreateCharacterType(uint64_t length,
                                                uint32_t bit_size,
                                                ConstString name) {
  auto ft = std::make_unique<FlangType>();
  ft->kind = FlangTypeKind::eCharacter;
  ft->bit_size = bit_size;
  ft->name = name;
  ft->string_length = length;
  FlangType *ptr = ft.get();
  m_types.push_back(std::move(ft));
  return ptr;
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
  if (auto *ft = GetFlangType(type))
    return ft->is_complete;
  return false;
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


bool TypeSystemFlang::IsCharType(lldb::opaque_compiler_type_t type) {
  auto *ft = GetFlangType(type);
  if (!ft)
    return false;
  return ft->kind == FlangTypeKind::eCharacter;
}

bool TypeSystemFlang::GetCompleteType(lldb::opaque_compiler_type_t type) {
  if (auto *ft = GetFlangType(type))
    return ft->is_complete;
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
  auto *ft = GetFlangType(type);
  if (!ft)
    return lldb::eTypeClassInvalid;
  return lldb::eTypeClassBuiltin;
}

uint32_t TypeSystemFlang::GetTypeInfo(
    lldb::opaque_compiler_type_t type,
    CompilerType *pointee_or_element_compiler_type) {
  if (pointee_or_element_compiler_type)
    pointee_or_element_compiler_type->Clear();

  auto *ft = GetFlangType(type);
  if (!ft)
    return 0;

  switch (ft->kind) {
  case FlangTypeKind::eInteger:
    return eTypeIsBuiltIn | eTypeHasValue | eTypeIsScalar | eTypeIsInteger |
           eTypeIsSigned;
  case FlangTypeKind::eReal:
    return eTypeIsBuiltIn | eTypeHasValue | eTypeIsScalar | eTypeIsFloat;
  case FlangTypeKind::eLogical:
    return eTypeIsBuiltIn | eTypeHasValue | eTypeIsScalar;
  case FlangTypeKind::eCharacter:
    return eTypeIsBuiltIn | eTypeHasValue | eTypeHasChildren;
  default:
    return 0;
  }
}

CompilerType
TypeSystemFlang::GetCanonicalType(lldb::opaque_compiler_type_t type) {
    if (type) {
        return CompilerType(weak_from_this(), type);
    }
    return CompilerType();
}

CompilerType
TypeSystemFlang::GetFullyUnqualifiedType(lldb::opaque_compiler_type_t type) {
  if (type)
    return CompilerType(weak_from_this(), type);
  return CompilerType();
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
  auto *ft = GetFlangType(type);
  if (!ft)
    return lldb::eEncodingInvalid;
  switch (ft->kind) {
  case FlangTypeKind::eInteger:
    return lldb::eEncodingSint;
  case FlangTypeKind::eReal:
    return lldb::eEncodingIEEE754;
  case FlangTypeKind::eLogical:
    return lldb::eEncodingUint;
  case FlangTypeKind::eCharacter:
    return lldb::eEncodingUint;
  default:
    return lldb::eEncodingInvalid;
  }
}

lldb::Format TypeSystemFlang::GetFormat(lldb::opaque_compiler_type_t type) {
  auto *ft = GetFlangType(type);
  if (!ft)
    return lldb::eFormatDefault;
  switch (ft->kind) {
  case FlangTypeKind::eInteger:
    return lldb::eFormatDecimal;
  case FlangTypeKind::eReal:
    return lldb::eFormatFloat;
  case FlangTypeKind::eLogical:
    return lldb::eFormatBoolean;
  case FlangTypeKind::eCharacter:
    return lldb::eFormatChar;
  default:
    return lldb::eFormatDefault;
  }
}

llvm::Expected<uint32_t>
TypeSystemFlang::GetNumChildren(lldb::opaque_compiler_type_t type,
                                bool omit_empty_base_classes,
                                const ExecutionContext *exe_ctx) {
  return 0;
}

lldb::BasicType
TypeSystemFlang::GetBasicTypeEnumeration(lldb::opaque_compiler_type_t type) {
  auto *ft = GetFlangType(type);
  if (!ft)
    return lldb::eBasicTypeInvalid;
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
    return lldb::eBasicTypeInvalid;
  }
}


std::optional<size_t>
TypeSystemFlang::GetTypeBitAlign(lldb::opaque_compiler_type_t type,
                                 ExecutionContextScope *exe_scope) {
  if (auto *ft = GetFlangType(type))
    return ft->bit_size;
  return std::nullopt;
}

const llvm::fltSemantics &
TypeSystemFlang::GetFloatTypeSemantics(size_t byte_size, lldb::Format format) {
  switch (byte_size) {
  case 2:
    return llvm::APFloat::IEEEhalf();
  case 4:
    return llvm::APFloat::IEEEsingle();
  case 8:
    return llvm::APFloat::IEEEdouble();
  case 16:
    return llvm::APFloat::IEEEquad();
  default:
    return llvm::APFloat::IEEEdouble();
  }
}
bool TypeSystemFlang::DumpTypeValue(
    lldb::opaque_compiler_type_t type, Stream &s, lldb::Format format,
    const DataExtractor &data, lldb::offset_t data_offset,
    size_t data_byte_size, uint32_t bitfield_bit_size,
    uint32_t bitfield_bit_offset, ExecutionContextScope *exe_scope) {
  if (!type)
    return false;

  if (format == lldb::eFormatDefault)
    format = GetFormat(type);

  uint32_t item_count = 1;
  DumpDataExtractor(data, &s, data_offset, format, data_byte_size, item_count,
                    UINT32_MAX, LLDB_INVALID_ADDRESS, bitfield_bit_size,
                    bitfield_bit_offset, exe_scope);
  return true;
}

void TypeSystemFlang::DumpTypeDescription(lldb::opaque_compiler_type_t type,
                                          lldb::DescriptionLevel level) {
  StreamFile s(stdout, false);
  DumpTypeDescription(type, s, level);
}

void TypeSystemFlang::DumpTypeDescription(lldb::opaque_compiler_type_t type,
                                          Stream &s,
                                          lldb::DescriptionLevel level) {
  if (auto *ft = GetFlangType(type))
    s.PutCString(ft->name.GetStringRef());
}
