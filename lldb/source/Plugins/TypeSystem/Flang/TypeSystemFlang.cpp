#include "lldb/Core/PluginManager.h"
#include "lldb/lldb-enumerations.h"
#include "Plugins/SymbolFile/DWARF/DWARFASTParserFlang.h"
#include "TypeSystemFlang.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::plugin::dwarf;

LLDB_PLUGIN_DEFINE(TypeSystemFlang);


void TypeSystemFlang::Finalize() {
    // TODO: Free memory here
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
