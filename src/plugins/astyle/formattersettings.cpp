/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk.h"
#include "formattersettings.h"
#include "astylepredefinedstyles.h"
#include "configmanager.h"

FormatterSettings::FormatterSettings()
{
  //ctor
}

FormatterSettings::~FormatterSettings()
{
  //dtor
}

void FormatterSettings::ApplyTo(astyle::ASFormatter& formatter)
{
  // NOTE: Keep this in sync with DlgFormatterSettings::ApplyTo
  ConfigManager* cfg = Manager::Get()->GetConfigManager("astyle");

  int style = cfg->ReadInt("/style", 0);
  switch (style)
  {
    case aspsAllman: // Allman (ANSI)
      formatter.setFormattingStyle(astyle::STYLE_ALLMAN);
      break;

    case aspsJava: // Java
      formatter.setFormattingStyle(astyle::STYLE_JAVA);
      break;

    case aspsKr: // K&R
      formatter.setFormattingStyle(astyle::STYLE_KR);
      break;

    case aspsStroustrup: // Stroustrup
      formatter.setFormattingStyle(astyle::STYLE_STROUSTRUP);
      break;

    case aspsWhitesmith: // Whitesmith
      formatter.setFormattingStyle(astyle::STYLE_WHITESMITH);
      break;

    case aspsVTK: // VTK
      formatter.setFormattingStyle(astyle::STYLE_VTK);
      break;

    case aspsRatliff: // Ratliff
      formatter.setFormattingStyle(astyle::STYLE_RATLIFF);
      break;

    case aspsGnu: // GNU
      formatter.setFormattingStyle(astyle::STYLE_GNU);
      break;

    case aspsLinux: // Linux
      formatter.setFormattingStyle(astyle::STYLE_LINUX);
      break;

    case aspsHorstmann: // Horstmann
      formatter.setFormattingStyle(astyle::STYLE_HORSTMANN);
      break;

    case asps1TBS: // 1TBS
      formatter.setFormattingStyle(astyle::STYLE_1TBS);
      break;

    case aspsGoogle: // Google
      formatter.setFormattingStyle(astyle::STYLE_GOOGLE);
      break;

    case aspsMozilla: // Mozilla
      formatter.setFormattingStyle(astyle::STYLE_MOZILLA);
      break;

    case aspsPico: // Pico
      formatter.setFormattingStyle(astyle::STYLE_PICO);
      break;

    case aspsLisp: // Lisp
      formatter.setFormattingStyle(astyle::STYLE_LISP);
      break;

    default: // Custom
      break;
  }

  formatter.setAttachClass(cfg->ReadBool("/attach_classes"));
  formatter.setAttachExternC(cfg->ReadBool("/attach_extern_c"));
  formatter.setAttachNamespace(cfg->ReadBool("/attach_namespaces"));
  formatter.setAttachInline(cfg->ReadBool("/attach_inlines"));

  int spaceNum = cfg->ReadInt("/indentation", 4);
  bool value = cfg->ReadBool("/force_tabs");
  if (cfg->ReadBool("/use_tabs"))
    formatter.setTabIndentation(spaceNum, value);
  else
    formatter.setSpaceIndentation(spaceNum);

  int contNum = cfg->ReadInt("/continuation", 0);
  if (contNum>0 && contNum<=4)
    formatter.setContinuationIndentation(contNum);

  formatter.setCaseIndent(cfg->ReadBool("/indent_case"));
  formatter.setClassIndent(cfg->ReadBool("/indent_classes"));
  formatter.setLabelIndent(cfg->ReadBool("/indent_labels"));
  formatter.setModifierIndent(cfg->ReadBool("/indent_modifiers"));
  formatter.setNamespaceIndent(cfg->ReadBool("/indent_namespaces"));
  formatter.setSwitchIndent(cfg->ReadBool("/indent_switches"));
  formatter.setPreprocBlockIndent(cfg->ReadBool("/indent_preproc_block"));
  formatter.setPreprocDefineIndent(cfg->ReadBool("/indent_preproc_define"));
  formatter.setPreprocConditionalIndent(cfg->ReadBool("/indent_preproc_cond"));
  formatter.setIndentCol1CommentsMode(cfg->ReadBool("/indent_col1_comments"));
  formatter.setMinConditionalIndentOption(cfg->ReadInt("/min_conditional_indent", 2));
  formatter.setMaxInStatementIndentLength(cfg->ReadInt("/max_instatement_indent", 40));

  formatter.setBreakClosingHeaderBracesMode(cfg->ReadBool("/break_closing"));
  formatter.setBreakElseIfsMode(cfg->ReadBool("/break_elseifs"));
  formatter.setAddBracketsMode(cfg->ReadBool("/add_brackets"));
  formatter.setAddOneLineBracketsMode(cfg->ReadBool("/add_one_line_brackets"));
  formatter.setRemoveBracketsMode(cfg->ReadBool("/remove_brackets"));
  formatter.setBreakOneLineBlocksMode(!cfg->ReadBool("/keep_blocks"));
  formatter.setBreakOneLineHeadersMode(cfg->ReadBool("/keep_headers"));
  formatter.setBreakOneLineStatementsMode(!cfg->ReadBool("/keep_statements"));
  formatter.setTabSpaceConversionMode(cfg->ReadBool("/convert_tabs"));
  formatter.setCloseTemplatesMode(cfg->ReadBool("/close_templates"));
  formatter.setStripCommentPrefix(cfg->ReadBool("/remove_comment_prefix"));

  if (cfg->ReadBool("/break_lines"))
  {
    formatter.setMaxCodeLength( wxAtoi(cfg->Read("/max_line_length")) );
    formatter.setBreakAfterMode(cfg->ReadBool("/break_after_mode"));
  }
  else
    formatter.setMaxCodeLength(INT_MAX);

  formatter.setBreakBlocksMode(cfg->ReadBool("/break_blocks"));
  formatter.setBreakClosingHeaderBlocksMode(cfg->ReadBool("/break_blocks_all"));
  formatter.setOperatorPaddingMode(cfg->ReadBool("/pad_operators"));
  formatter.setParensOutsidePaddingMode(cfg->ReadBool("/pad_parentheses_out"));
  formatter.setParensInsidePaddingMode(cfg->ReadBool("/pad_parentheses_in"));
  formatter.setParensFirstPaddingMode(cfg->ReadBool("/pad_first_paren_out"));
  formatter.setParensHeaderPaddingMode(cfg->ReadBool("/pad_header"));
  formatter.setParensUnPaddingMode(cfg->ReadBool("/unpad_parentheses"));
  formatter.setCommaPaddingMode(cfg->ReadBool("/pad_comma"));
  formatter.setDeleteEmptyLinesMode(cfg->ReadBool("/delete_empty_lines"));
  formatter.setEmptyLineFill(cfg->ReadBool("/fill_empty_lines"));

  wxString pointerAlign = cfg->Read("/pointer_align");
  if      (pointerAlign == "Type")
    formatter.setPointerAlignment(astyle::PTR_ALIGN_TYPE);
  else if (pointerAlign == "Middle")
    formatter.setPointerAlignment(astyle::PTR_ALIGN_MIDDLE);
  else if (pointerAlign == "Name")
    formatter.setPointerAlignment(astyle::PTR_ALIGN_NAME);
  else
    formatter.setPointerAlignment(astyle::PTR_ALIGN_NONE);

  wxString referenceAlign = cfg->Read("/reference_align");
  if      (referenceAlign == "Type")
    formatter.setReferenceAlignment(astyle::REF_ALIGN_TYPE);
  else if (referenceAlign == "Middle")
    formatter.setReferenceAlignment(astyle::REF_ALIGN_MIDDLE);
  else if (referenceAlign == "Name")
    formatter.setReferenceAlignment(astyle::REF_ALIGN_NAME);
  else
    formatter.setReferenceAlignment(astyle::REF_ALIGN_NONE);
}
