//Copyright (C) 2025 Hongyi Chen (BeanPieChen)
//Licensed under the MIT License

#include "FontCollection.h"
#include <hb-ft.h>

float FT_Fix26ToFloat(FT_Pos val) {
	long i = val >> 6;
	long f = val - (i << 6);
	return (i + (float)f / 64);
}


void FontCollection::_UpdateMaxMetrics() {
	if (this->max_dirty) {
		this->max_height = 0;
		this->max_ascender = 0;
		this->max_descender = 0;
		this->max_advance = 0;

		Font* p = this->head;
		while (p != nullptr) {
			float fmh = FT_Fix26ToFloat(p->face->height);
			float fma = FT_Fix26ToFloat(p->face->ascender);
			float fmd = FT_Fix26ToFloat(p->face->descender);
			float fmadv = FT_Fix26ToFloat(p->face->height);

			if (fmh > this->max_height)
				this->max_height = fmh;
			if (fma > this->max_ascender)
				this->max_ascender = fmh;
			if (fmd > this->max_descender)
				this->max_descender = fmh;
			if (fmadv > this->max_advance)
				this->max_advance = fmh;
			p = p->next;
		}
		this->max_dirty = false;
	}
}


void FontCollection::_ClearGlyphCache() {
	Font* p = this->head;
	while (p != nullptr) {
		p->glyph_cache.Clear();
		p = p->next;
	}
	for (size_t i = 0;i < this->atlases_gray.GetSize();++i)
		delete this->atlases_gray.Get(i);
	this->atlases_gray.Clear();
	for (size_t i = 0;i < this->atlases_bgra.GetSize();++i)
		delete this->atlases_bgra.Get(i);
	this->atlases_bgra.Clear();
}


void FontCollection::_NewAtlas(bool is_bgra) {
	Atlas* new_atlas = new Atlas(is_bgra ? ATLAS_SIZE_RGBA : ATLAS_SIZE_GRAY, 4096U, is_bgra);
	if(is_bgra)
		this->atlases_bgra.Push(new_atlas);
	else
		this->atlases_gray.Push(new_atlas);
}


void FontCollection::_AppendFont(Font* font) {
	font->pre = tail;
	if (tail != nullptr)
		tail->next = font;
	else 
		head = font;
	tail = font;
	this->max_dirty = true;
}


bool FontCollection::_AtlasAdd(uint8_t* buffer, int w, int h, bool is_bgra, GlyphInfo& info) {
	Array<Atlas*>* atlases;
	if (is_bgra)
		atlases = &this->atlases_bgra;
	else 
		atlases = &this->atlases_gray;

	if (atlases->GetSize() == 0)
		this->_NewAtlas(info.pixel_type == GLYPH_PIXEL_TYPE_BGRA);
	int i = 0;
	info.region_index = UINT16_MAX;
	while (info.region_index >= 4096 && i < atlases->GetSize()) {
		info.atlas_index = i;
		info.region_index = atlases->Get(i)->addRegion(w, h, buffer, 1);
		++i;
	}
	if (info.region_index >= 4096) {
		this->_NewAtlas(info.pixel_type == GLYPH_PIXEL_TYPE_BGRA);
		info.region_index = atlases->Get(atlases->GetSize() - 1)->addRegion(w, h, buffer, 1);
		if (info.region_index >= 4096) {
			Atlas* temp = atlases->Get(atlases->GetSize() - 1);
			atlases->Pop();
			delete temp;
		}
		else
			info.atlas_index = i;
	}

	if (info.region_index >= 4096)
		return false;

	return true;
}


void FontCollection::_AddDummyGlyph() {
	uint32_t dummy_size = this->pixel_height * 0.5;
	uint8_t* dummy_bitmap = new uint8_t[dummy_size * dummy_size];
	for (int j = 0;j < dummy_size;++j)
		for (int i = 0;i < dummy_size;++i)
			dummy_bitmap[j * dummy_size + i] = 255;
	this->dummy_info.offset_x = (this->GetMaxAdvance() - dummy_size) / 2;
	this->dummy_info.offset_y = dummy_size;
	this->dummy_info.advance_x = this->GetMaxAdvance();
	this->dummy_info.advance_y = this->GetMaxAdvance();
	this->_AtlasAdd(dummy_bitmap, dummy_size, dummy_size, false, this->dummy_info); 
}


FontCollection::FontCollection(FT_Library ft_lib, uint32_t pixel_height, bool use_sdf) {
	this->ft_lib = ft_lib;
	this->SetHeightInPixel(pixel_height);
	this->use_sdf = use_sdf;
}


void FontCollection::SetHeightInPixel(uint32_t pixel_height) {
	if (this->pixel_height == pixel_height)
		return;
	this->pixel_height = pixel_height;
	Font* p = this->head;
	while (p != nullptr) {
		FT_Set_Pixel_Sizes(p->face, 0, pixel_height);
		p = p->next;
	}
	this->max_dirty = true;
	this->_ClearGlyphCache();
	this->_AddDummyGlyph();
}


Font* FontCollection::AddFont(uint8_t* buffer, size_t size) {
	FT_Face face;
	if(FT_New_Memory_Face(this->ft_lib, buffer, size, 0, &face))
		goto ft_create_fail;
	if(FT_Set_Pixel_Sizes(face, 0, this->pixel_height))
		goto ft_face_set_fail;
	if (FT_Select_Charmap(face, FT_ENCODING_UNICODE))
		goto ft_face_set_fail;
	Font* font = new Font;
	font->ff = this;
	font->face = face;
	font->hb_font = hb_ft_font_create_referenced(face);
	this->_AppendFont(font);
	this->max_dirty = true;
	this->dummy_info.offset_x = (this->GetMaxAdvance() - this->pixel_height * 0.5) / 2;
	this->dummy_info.advance_x = this->GetMaxAdvance();
	this->dummy_info.advance_y = this->GetMaxAdvance();
	return font;
ft_face_set_fail:
	FT_Done_Face(face);
ft_create_fail:
	return nullptr;
}


void FontCollection::FontMoveForward(Font* font) {
	if (font->pre == nullptr)
		return;
	if (font->next == nullptr)
		this->tail = font->pre;
	else
		font->next->pre = font->pre;
	font->pre->next = font->next;
	Font* temp = font->pre->pre;
	font->pre->pre = font;
	font->next = font->pre;
	font->pre = temp;
	if (font->pre == nullptr)
		this->head = font;
	else
		font->pre->next = font;
	this->_ClearGlyphCache();
	this->_AddDummyGlyph();
}


void FontCollection::FontMoveBackward(Font* font) {
	if (font->next = nullptr)
		return;
	if (font->pre = nullptr)
		this->head = font->next;
	else
		font->pre->next = font->next;
	font->next->pre = font->pre;
	Font* temp = font->next->next;
	font->next->next = font;
	font->pre = font->next;
	font->next = temp;
	if (font->next == nullptr)
		this->tail = font;
	else
		font->next->pre = font;
	this->_ClearGlyphCache();
	this->_AddDummyGlyph();
}


void FontCollection::RemoveFont(Font* font) {
	if (font->pre != nullptr)
		font->pre->next = font->next;
	else
		head = font->next;
	if (font->next != nullptr)
		font->next->pre = font->pre;
	else
		tail = font->pre;
	hb_font_destroy(font->hb_font);
	FT_Done_Face(font->face);
	delete font;
	this->max_dirty = true;
	this->_ClearGlyphCache();
	this->_AddDummyGlyph();
}


void FontCollection::ClearFonts() {
	Font* p = this->head;
	while (p != nullptr) {
		Font* temp = p;
		p = p->next;
		hb_font_destroy(temp->hb_font);
		FT_Done_Face(temp->face);
		delete temp;
	}
	this->head = nullptr;
	this->tail = nullptr;
	this->_ClearGlyphCache();
	this->_AddDummyGlyph();
}


bool FontCollection::GetGlyph(FT_UInt glyph_index, Font* font, GlyphInfo& glyph_info) {
	if (glyph_index == 0)
		return false;
	if (font->ff != this)
		return false;
	auto cached = font->glyph_cache.Find(glyph_index);
	if (!cached.IsNull()) {
		glyph_info = cached.Value();
		return true;
	}

	if (FT_Load_Glyph(font->face, glyph_index, 0))
		return false;

	if (FT_Render_Glyph(font->face->glyph, this->use_sdf ? FT_RENDER_MODE_SDF : FT_RENDER_MODE_NORMAL))
		return false;

	FT_GlyphSlot glyph = font->face->glyph;
	glyph_info.offset_x = glyph->bitmap_left;
	glyph_info.offset_y = glyph->bitmap_top;
	glyph_info.advance_x = FT_Fix26ToFloat(glyph->advance.x);
	glyph_info.advance_y = FT_Fix26ToFloat(glyph->advance.y);
	if (glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
		glyph_info.pixel_type = this->use_sdf ? GLYPH_PIXEL_TYPE_SDF : GLYPH_PIXEL_TYPE_GRAY;
	else if (glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA)
		glyph_info.pixel_type = GLYPH_PIXEL_TYPE_BGRA;
	else
		return false;

	if (this->_AtlasAdd(
		glyph->bitmap.buffer,
		glyph->bitmap.width,
		glyph->bitmap.rows,
		glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA,
		glyph_info)) {
		font->glyph_cache.Set(glyph_index, glyph_info);
		return true;
	}
	else
		return false;
}


bool FontCollection::GetDummyGlyph(GlyphInfo& glyph_info) {
	if (this->dummy_info.region_index >= MAX_REGION_NUM)
		return false;
	glyph_info = this->dummy_info;
	return true;
}


const uint8_t* FontCollection::GetAtlasBuffer(const GlyphInfo& glyph_info, int face_index) {
	if (glyph_info.pixel_type == GLYPH_PIXEL_TYPE_BGRA)
		return this->atlases_bgra.Get(glyph_info.atlas_index)->getAtlasBuffer() + ATLAS_SIZE_RGBA * ATLAS_SIZE_RGBA * face_index;
	else
		return this->atlases_gray.Get(glyph_info.atlas_index)->getAtlasBuffer() + ATLAS_SIZE_GRAY * ATLAS_SIZE_GRAY * face_index;
}


const AtlasRegion& FontCollection::GetAtlasRegion(const GlyphInfo& glyph_info) {
	if (glyph_info.pixel_type == GLYPH_PIXEL_TYPE_BGRA)
		return this->atlases_bgra.Get(glyph_info.atlas_index)->getRegion(glyph_info.region_index);
	else
		return this->atlases_gray.Get(glyph_info.atlas_index)->getRegion(glyph_info.region_index);
}


FontCollection::~FontCollection() {
	this->ClearFonts();
}