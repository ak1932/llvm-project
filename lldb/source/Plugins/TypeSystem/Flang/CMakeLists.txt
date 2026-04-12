add_lldb_library(lldbPluginTypeSystemFlang PLUGIN
  TypeSystemFlang.cpp

  LINK_COMPONENTS
    Support
  LINK_LIBS
    lldbCore
    lldbSymbol
    lldbTarget
    lldbUtility
    lldbPluginSymbolFileDWARF
    lldbPluginSymbolFilePDB
    lldbPluginObjCRuntime
)

