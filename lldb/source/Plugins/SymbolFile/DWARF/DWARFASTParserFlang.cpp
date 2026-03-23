#include "DWARFASTParser.h"
#include "DWARFASTParserFlang.h"
#include "DWARFDIE.h"
#include "SymbolFileDWARF.h"

#include "Plugins/TypeSystem/Flang/TypeSystemFlang.h"
using namespace lldb_private::plugin::dwarf;

DWARFASTParserFlang::DWARFASTParserFlang(TypeSystemFlang &ast)
    : DWARFASTParser(Kind::DWARFASTParserFlang), m_ast(ast) {}

DWARFASTParserFlang::~DWARFASTParserFlang() = default;

lldb::TypeSP DWARFASTParserFlang::ParseTypeFromDWARF(
    const SymbolContext &sc,
    const DWARFDIE &die, bool *type_is_new_ptr) {
  return nullptr;
}
