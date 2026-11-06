// Runtime UI language for the fork's display layer.
//
// The code carries the English text inline (readable diffs, no id soup); the
// Russian counterpart lives in one table in Localize.cpp and is returned by
// trStr() when the language preference is RU. Anything not in the table is
// returned unchanged, so protocol strings, node names and message text are
// never touched.
#pragma once

#include <stdint.h>

#ifndef UI_LANG_RU
#define UI_LANG_RU 0
#endif
#ifndef UI_LANG_EN
#define UI_LANG_EN 1
#endif

uint8_t& uiLangRef();
inline void setUiLang(uint8_t lang) { uiLangRef() = lang; }
inline uint8_t uiLang() { return uiLangRef(); }

// English in, Russian out (only for strings present in the table).
const char* trStr(const char* en);

// Wrap a UI literal: _("Cancel") -> "Отмена" when the UI language is RU.
#define _(s) trStr(s)
