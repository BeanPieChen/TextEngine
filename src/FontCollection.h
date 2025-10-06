#ifndef FONT_LIST_H
#define FONT_LIST_H

//Copyright (C) 2025 Hongyi Chen (BeanPieChen)
//Licensed under the MIT License

#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include "Array.h"
#include "Map.h"
#include "CubeAtlas.h"

#define GLYPH_PIXEL_TYPE_GRAY	0
#define GLYPH_PIXEL_TYPE_BGRA	1
#define GLYPH_PIXEL_TYPE_SDF	2

#define ATLAS_SIZE_GRAY	512
#define ATLAS_SIZE_RGBA 256

struct GlyphInfo {
	float offset_x;
	float offset_y;
	float advance_x;
	float advance_y;
	uint32_t atlas_index;
	uint16_t region_index;
	uint8_t pixel_type;
};


class FontCollection;

class Font {
	friend class FontCollection;
	FT_Face face;
	hb_font_t* hb_font;
	Font* pre = nullptr;
	Font* next = nullptr;

	FontCollection* ff;

	Map<FT_UInt, GlyphInfo> glyph_cache;

public:
	Font* Next() {
		return this->next;
	}
	Font* Pre() {
		return this->pre;
	}
	FT_Face GetFTFont() {
		return this->face;
	}
	hb_font_t* GetHBFont() {
		return this->hb_font;
	}
	FT_UInt GetGlyphIndex(uint32_t codepoint) {
		return FT_Get_Char_Index(this->face, codepoint);
	}
};


class FontCollection {
	FT_Library ft_lib;
	uint32_t pixel_height = 0;
	bool use_sdf;

	Font* head = nullptr;
	Font* tail = nullptr;

	float max_height;
	float max_ascender;
	float max_descender;
	float max_advance;

	Array<Atlas*> atlases_gray;
	Array<Atlas*> atlases_bgra;

	GlyphInfo dummy_info;

	bool max_dirty = true;
	void _UpdateMaxMetrics();
	void _ClearGlyphCache();
	void _NewAtlas(bool is_bgra);
	void _AppendFont(Font* font);
	bool _AtlasAdd(uint8_t* buffer, int w, int h, bool is_rgba, GlyphInfo& info);
	void _AddDummyGlyph();

public:
	FontCollection(FT_Library ft_lib, uint32_t pixel_height = 16, bool use_sdf = false);
	void SetHeightInPixel(uint32_t pixel_height);
	uint32_t GetFontHeightInPixel() {
		this->_UpdateMaxMetrics();
		return this->pixel_height;
	}
	float GetMaxHeight() {
		this->_UpdateMaxMetrics();
		return max_height;
	}
	float GetMaxAscender() {
		this->_UpdateMaxMetrics();
		return this->max_ascender;
	}
	float GetMaxDescender() {
		this->_UpdateMaxMetrics();
		return this->max_descender;
	}
	float GetMaxAdvance() {
		this->_UpdateMaxMetrics();
		return this->max_advance;
	}
	Font* AddFont(uint8_t* buffer,size_t size);
	void FontMoveForward(Font* font);
	void FontMoveBackward(Font* font);
	void RemoveFont(Font* font);
	void ClearFonts();

	Font* GetFirstFont() {
		return this->head;
	}
	bool GetGlyph(FT_UInt glyph_index , Font* font, GlyphInfo& glyph_info);
	bool GetDummyGlyph(GlyphInfo& glyph_info);
	const uint8_t* GetAtlasBuffer(const GlyphInfo& glyph_info, int face_index);
	const AtlasRegion& GetAtlasRegion(const GlyphInfo& glyph_info);
	static size_t GetAtlasSize(const GlyphInfo& glyph_info) {
		return glyph_info.pixel_type == GLYPH_PIXEL_TYPE_BGRA ? ATLAS_SIZE_RGBA : ATLAS_SIZE_GRAY;
	}

	~FontCollection();
};

#endif