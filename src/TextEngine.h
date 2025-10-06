#ifndef TEXT_ENGINE_H
#define TEXT_ENGINE_H

//Copyright (C) 2025 Hongyi Chen (BeanPieChen)
//Licensed under the MIT License

#include <cstddef>
#include <cstdint>
#include <SheenBidi/SheenBidi.h>
#include "Array.h"
#include "FontCollection.h"

#define TEXT_ALIGN_AUTO		0
#define TEXT_ALIGN_LEFT		1
#define TEXT_ALIGN_CENTER	2
#define TEXT_ALIGN_RIGHT	3

#define CP_FLAG_IS_RTL		(0x1U<<0)
#define CP_FLAG_CAN_BREAK	(0x1U<<1)
#define CP_FLAG_MAPPED		(0x1U<<2)

#define CP_FLAG_GET(flags,mask) ((flags&mask)!=0)
#define CP_FLAG_SET(flags,mask,value) ((value)?(flags|=mask):(flags&=(~mask)))

struct CPInfo {
	uint32_t codepoint;
	uint8_t flags;
	size_t line;
	size_t start;
	size_t len;

	CPInfo(
		uint32_t codepoint = 0,
		uint8_t flags = 0x0U,
		size_t line = 0,
		size_t start = 0,
		size_t len = 0
		):codepoint(codepoint),flags(flags),line(line),start(start),len(len){ }
};


struct MappedGlyph {
	size_t map;
	GlyphInfo gi;
	bool is_ltr;
};


struct TextLine {
	size_t index;
	Array<MappedGlyph> glyphs;
	float width = 0;

	TextLine(size_t index):index(index){}
	void Append(uint32_t glyph_index, size_t map, uint32_t codepoint, bool is_ltr, Font* font, FontCollection* ff);
};


class Paragraph {
	SBAlgorithmRef sba = nullptr;
	SBParagraphRef sbp = nullptr;
	SBUInteger sbpl = 0;
	SBLineRef sbl = nullptr;

	TextLine* _GetLastLine();
	void _AppendNewLine();

public:
	float warp_width = -1;
	FontCollection* ff;
	Array<CPInfo> cps;
	Array<TextLine*> lines;

	Paragraph(FontCollection* ff, float warp_width = -1);
	void Insert(size_t pos, const Array<CPInfo>& cps, size_t start, size_t len);
	void SloveBidi();
	void SloveLayout();
	void ClearLines();
	~Paragraph();
};


struct GlyphPos {
	size_t paragraph;
	size_t line;
	size_t glyph;
	bool after;

	GlyphPos(
		size_t paragraph = 0,
		size_t line = 0,
		size_t glyph = 0,
		bool after = false
	) :paragraph(paragraph), line(line), glyph(glyph), after(after) {}
};


struct CPPos {
	size_t paragraph;
	size_t cp;
	bool eol;
	
	CPPos(size_t paragraph = 0, size_t cp = 0, bool eol = false) :paragraph(paragraph), cp(cp), eol(eol){}

	bool operator==(const CPPos& right) const {
		if (
			this->paragraph == right.paragraph 
			&& this->cp == right.cp 
			&& this->eol == right.eol
			)
			return true;
		return false;
	}

	bool operator!=(const CPPos& right) const {
		return !this->operator==(right);
	}

	bool operator>(const CPPos& right)const {
		if (this->paragraph > right.paragraph)
			return true;
		if (this->paragraph == right.paragraph) {
			if (this->cp > right.cp)
				return true;
			if (this->cp == right.cp && this->eol && !right.eol)
				return true;
		}
		return false;
	}
};


#define CP_SWAP(a,b) {CPPos temp=a;a=b;b=temp;}


class TextEngine {
	int align_mode = TEXT_ALIGN_AUTO;
	float warp_width = -1;
	FontCollection* ff;
	Array<Paragraph*> paragraphs;

	Paragraph* _GetLastParagraph();
	void _DecodeUtf8(const char* utf8_str, size_t len, Array<CPInfo>& cps);
	void _Append(const Array<CPInfo>& cps);
	void _DeleteParagraphs(size_t start, size_t len);
	CPPos _GPos2CPPos(const GlyphPos& gp);
 	bool _CPPos2GPos(const CPPos& cpp, GlyphPos& gp);
	CPPos _NextCodepoint(const CPPos& cpp);
	CPPos _PreNextMappedCP(const CPPos& cpp, bool is_next);
	void _FindNearestGlyph(const GlyphPos& before, GlyphPos& after);
	GlyphPos _FindGlyphPos(size_t paragraph, size_t line, float x);
	float _ComputeCursorPosX(const GlyphPos& gp);

public:
	TextEngine(FontCollection* ff, float warp_width = -1);
	void SetWarpWidth(float width);
	float GetWarpWidth();
	void SetAlignMode(int mode);
	int GetAlignMode();
	void Clear();
	void Append(const char* utf8_str, size_t len);
	CPPos Insert(const char* utf8_str, size_t len, const CPPos& pos);
	void Delete(const CPPos& start, const CPPos& end);
	CPPos Replace(const char* utf8_str, size_t len, const CPPos& start, const CPPos& end);
	size_t GetParagarphNum();
	Paragraph* GetParagraph(size_t index);
	CPPos Hit(float lh, float x, float y);
	bool ComputeCursorPos(const CPPos& pos, float lh, float& x, float& y);
	CPPos CheckCursorPos(const CPPos& pos);
	CPPos PreCodepoint(const CPPos& cpp);
	CPPos CursorUp(const CPPos& pos);
	CPPos CursorDown(const CPPos& pos);
	CPPos CursorLeft(const CPPos& pos);
	CPPos CursorRight(const CPPos& pos);
	bool IsGlyphInRange(const GlyphPos& gp, const CPPos& a, const CPPos& b);
};

#endif