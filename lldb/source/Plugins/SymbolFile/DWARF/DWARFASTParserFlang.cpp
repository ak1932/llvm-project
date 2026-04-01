#include "DWARFASTParser.h"
#include "DWARFASTParserFlang.h"
#include "DWARFDIE.h"
#include "DWARFUnit.h"
#include "LogChannelDWARF.h"
#include "SymbolFileDWARF.h"

#include "Plugins/TypeSystem/Flang/TypeSystemFlang.h"
#include "lldb/Symbol/CompileUnit.h"
#include "lldb/Symbol/SymbolFile.h"
#include "lldb/Symbol/Type.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include <optional>
using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::plugin::dwarf;
using namespace llvm::dwarf;

DWARFASTParserFlang::DWARFASTParserFlang(TypeSystemFlang &ast)
    : DWARFASTParser(Kind::DWARFASTParserFlang), m_ast(ast) {}

DWARFASTParserFlang::~DWARFASTParserFlang() = default;

Function *DWARFASTParserFlang::ParseFunctionFromDWARF(
    CompileUnit &comp_unit, const DWARFDIE &die, AddressRanges func_ranges) {
  llvm::DWARFAddressRangesVector unused_func_ranges;
  const char *name = nullptr;
  const char *mangled = nullptr;
  std::optional<int> decl_file;
  std::optional<int> decl_line;
  std::optional<int> decl_column;
  std::optional<int> call_file;
  std::optional<int> call_line;
  std::optional<int> call_column;
  DWARFExpressionList frame_base;

  const dw_tag_t tag = die.Tag();

  if (tag != DW_TAG_subprogram)
    return nullptr;

  if (die.GetDIENamesAndRanges(name, mangled, unused_func_ranges, decl_file,
                               decl_line, decl_column, call_file, call_line,
                               call_column, &frame_base)) {
    Mangled func_name;
    if (mangled)
      func_name.SetValue(ConstString(mangled));
    // TODO: This had guard for CPlusPlus and Obj C, check if
    // there are any versions of fortran which may or may not require mangling
    else if ((die.GetParent().Tag() == DW_TAG_compile_unit ||
              die.GetParent().Tag() == DW_TAG_partial_unit) &&
             name && strcmp(name, "main") != 0) {
      // If the mangled name is not present in the DWARF, generate the
      // demangled name using the decl context. We skip if the function is
      // "main" as its name is never mangled.
      func_name.SetDemangledName(ConstructDemangledNameFromDWARF(die));
      // Ensure symbol is preserved (as the mangled name).
      func_name.SetMangledName(ConstString(name));
    } else
      func_name.SetValue(ConstString(name));

    FunctionSP func_sp;
    std::unique_ptr<Declaration> decl_up;
    if (decl_file || decl_line || decl_column)
      decl_up = std::make_unique<Declaration>(
          die.GetCU()->GetFile(decl_file.value_or(0)), decl_line.value_or(0),
          decl_column.value_or(0));

    SymbolFileDWARF *dwarf = die.GetDWARF();
    // Supply the type _only_ if it has already been parsed
    Type *func_type = dwarf->GetDIEToType().lookup(die.GetDIE());

    assert(func_type == nullptr || func_type != DIE_IS_BEING_PARSED);

    const user_id_t func_user_id = die.GetID();

    // The base address of the scope for any of the debugging information
    // entries listed above is given by either the DW_AT_low_pc attribute or the
    // first address in the first range entry in the list of ranges given by the
    // DW_AT_ranges attribute.
    //   -- DWARFv5, Section 2.17 Code Addresses, Ranges and Base Addresses
    //
    // If no DW_AT_entry_pc attribute is present, then the entry address is
    // assumed to be the same as the base address of the containing scope.
    //   -- DWARFv5, Section 2.18 Entry Address
    //
    // We currently don't support Debug Info Entries with
    // DW_AT_low_pc/DW_AT_entry_pc and DW_AT_ranges attributes (the latter
    // attributes are ignored even though they should be used for the address of
    // the function), but compilers also don't emit that kind of information. If
    // this becomes a problem we need to plumb these attributes separately.
    Address func_addr = func_ranges[0].GetBaseAddress();

    func_sp = std::make_shared<Function>(
        &comp_unit,
        func_user_id, // UserID is the DIE offset
        func_user_id, func_name, func_type, std::move(func_addr),
        std::move(func_ranges));

    if (func_sp.get() != nullptr) {
      if (frame_base.IsValid())
        func_sp->GetFrameBaseExpression() = frame_base;
      comp_unit.AddFunction(func_sp);
      return func_sp.get();
    }
  }
  return nullptr;
}

lldb::TypeSP DWARFASTParserFlang::ParseTypeFromDWARF(
    const SymbolContext &sc,
    const DWARFDIE &die, bool *type_is_new_ptr) {
  if (type_is_new_ptr)
    *type_is_new_ptr = false;

  if (!die)
    return nullptr;

  SymbolFileDWARF *dwarf = die.GetDWARF();
  if (!dwarf)
    return nullptr;

  Log *log = GetLog(DWARFLog::TypeCompletion | DWARFLog::Lookups);
  if (log) {
    dwarf->GetObjectFile()->GetModule()->LogMessage(
        log,
        "DWARFASTParserFlang::ParseTypeFromDWARF "
        "(die = {0:x16}) {1} ({2}) name = '{3}'",
        die.GetOffset(), DW_TAG_value_to_name(die.Tag()), die.Tag(),
        die.GetName());
  }

  // Check if we already parsed this DIE
  Type *type_ptr = dwarf->GetDIEToType().lookup(die.GetDIE());
  if (type_ptr && type_ptr != DIE_IS_BEING_PARSED)
    return type_ptr->shared_from_this();

  // Mark as being parsed to prevent recursion
  dwarf->GetDIEToType()[die.GetDIE()] = DIE_IS_BEING_PARSED;

  TypeSP type_sp;
  const dw_tag_t tag = die.Tag();

  switch (tag) {
  case DW_TAG_base_type:
    type_sp = ParseBaseType(die);
    break;
  default:
    break;
  }

  if (type_sp) {
    dwarf->GetDIEToType()[die.GetDIE()] = type_sp.get();
    if (type_is_new_ptr)
      *type_is_new_ptr = true;
  } else {
    dwarf->GetDIEToType().erase(die.GetDIE());
  }

  return type_sp;
}
lldb::TypeSP
DWARFASTParserFlang::ParseBaseType(const DWARFDIE &die) {
  SymbolFileDWARF *dwarf = die.GetDWARF();

  const char *name_cstr =
      die.GetAttributeValueAsString(DW_AT_name, nullptr);
  ConstString type_name(name_cstr ? name_cstr : "");

  uint64_t byte_size =
      die.GetAttributeValueAsUnsigned(DW_AT_byte_size, 0);
  uint32_t encoding =
      die.GetAttributeValueAsUnsigned(DW_AT_encoding, 0);
  uint32_t bit_size = byte_size * 8;

  CompilerType compiler_type =
      m_ast.GetBuiltinTypeForDWARFEncodingAndBitSize(
          type_name.GetStringRef(), encoding, bit_size);

  if (!compiler_type)
    return nullptr;

  Declaration decl;
  return dwarf->MakeType(die.GetID(), type_name,
                         std::optional<uint64_t>(byte_size), nullptr,
                         LLDB_INVALID_UID, Type::eEncodingIsUID, decl,
                         compiler_type, Type::ResolveState::Full);
}

