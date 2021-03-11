// Copyright (C) 2010 Barry Duncan. All Rights Reserved.
// The original author of this code can be contacted at: bduncan22@hotmail.com

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// DEVELOPERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef _included_Fonts_h_
#define _included_Fonts_h_

#include <string>
#include <stdint.h>
#include "renderer.h"

enum eFontTypes
{
	kFontSmall,
	kFontBig,
	kFontConsole
};

const int kNumFontTypes = 3;

bool Font_Load(const std::string &fontFile, enum eFontTypes fontType);
void Font_Release();
uint32_t Font_DrawText(const std::string &text, uint32_t x, uint32_t y, RCOLOR colour, enum eFontTypes fontType);
uint32_t Font_GetCharWidth(char c, enum eFontTypes fontType);
uint32_t Font_GetCharHeight(enum eFontTypes fontType);
uint32_t Font_GetStringWidth(const std::string &text, enum eFontTypes fontType);
uint32_t Font_DrawCenteredText(const std::string &text, enum eFontTypes fontType);


#endif
