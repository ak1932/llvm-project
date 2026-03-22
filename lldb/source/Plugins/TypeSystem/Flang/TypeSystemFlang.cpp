#include "lldb/Core/PluginManager.h"
#include "TypeSystemFlang.h"
#include "lldb/lldb-enumerations.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::plugin::dwarf;

LLDB_PLUGIN_DEFINE(TypeSystemFlang);

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

lldb::TypeSystemSP TypeSystemFlang::CreateInstance(lldb::LanguageType language,
                                                   lldb_private::Module *module,
                                                   Target *target) {
  if (!TypeSystemFlangSupportsLanguage(language))
    return lldb::TypeSystemSP();

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
