//Copyright (C) 2025 Hongyi Chen (BeanPieChen)
//Licensed under the MIT License

#include "TextEngine.h"
#include "FontCollection.h"
#include <ft2build.h>
#include <hb.h>
#include <hb-ft.h>
#include <LineBreaker.h>
#include "UTF8Codec.h"
#include "List.h"


////////////
//TextLine//
////////////
bool FetchGlyph(uint32_t glyph_index, Font* font, GlyphInfo& gi, FontCollection* ff) {
	if (glyph_index == 0 || font == nullptr) {
		if (!ff->GetDummyGlyph(gi))
			return false;
	}
	else if (!ff->GetGlyph(glyph_index, font, gi)) {
		Font* _font = ff->GetFirstFont();
		while (_font != nullptr) {
			if (_font != font)
				if (ff->GetGlyph(glyph_index, _font, gi))
					break;
			_font = _font->Next();
		}
		if (_font == nullptr)
			if (!ff->GetDummyGlyph(gi))
				return false;
	}

	return true;
}


void TextLine::Append(uint32_t glyph_index, size_t map, uint32_t codepoint, bool is_ltr, Font* font, FontCollection* ff) {
	if (glyph_index == 0) {
		Font* fp = ff->GetFirstFont();
		while (fp != nullptr) {
			if (fp != font) {
				glyph_index = fp->GetGlyphIndex(codepoint);
				font = fp;
				if (glyph_index != 0)
					break;
			}
			fp = fp->Next();
		}
	}
	GlyphInfo gi;
	if (FetchGlyph(glyph_index, font, gi, ff)) {
		this->glyphs.Push({ map,gi,is_ltr });
		this->width += gi.advance_x;
	}
	else {
		ff->GetDummyGlyph(gi);
		this->glyphs.Push({ map,gi,is_ltr });
		this->width += ff->GetMaxAdvance();
	}
}


/////////////
//Paragraph//
/////////////
uint32_t CodepointAt(const void* cps, size_t index) {
	return reinterpret_cast<const Array<CPInfo>*>(cps)->Get(index).codepoint;
}


TextLine* Paragraph::_GetLastLine() {
	if (this->lines.GetSize() == 0)
		this->lines.Push(new TextLine(0));
	return this->lines.Get(this->lines.GetSize() - 1);
}


void Paragraph::_AppendNewLine() {
	this->lines.Push(new TextLine(this->lines.GetSize()));
}


Paragraph::Paragraph(FontCollection* ff, float warp_width) :ff(ff), warp_width(warp_width) {}


void Paragraph::Insert(size_t pos, const Array<CPInfo>& cps, size_t start, size_t len) {
	if (len == 0)
		return;
	size_t old_size = this->cps.GetSize();
	if (this->cps.GetSize() > 0)
		for (size_t i = this->cps.GetSize() - 1;i >= pos;--i)
			this->cps.Set(i + len, this->cps.Get(i));
	for (size_t i = 0;i < len;++i)
		this->cps.Set(pos + i, cps.Get(start + i));
}


void Paragraph::SloveBidi() {
	if (this->sba != nullptr) {
		SBLineRelease(this->sbl);
		SBParagraphRelease(this->sbp);
		SBAlgorithmRelease(this->sba);
		this->sbl = nullptr;
		this->sbp = nullptr;
		this->sba = nullptr;
		this->sbpl = 0;
	}
	if (this->cps.GetSize() > 0) {
		SBCodepointSequence sbs = { CodepointAt,&this->cps,this->cps.GetSize() };
		this->sba = SBAlgorithmCreate(&sbs);
		this->sbp = SBAlgorithmCreateParagraph(sba, 0, INT32_MAX, SBLevelDefaultLTR);
		this->sbpl = SBParagraphGetLength(sbp);
		this->sbl = SBParagraphCreateLine(sbp, 0, sbpl);
	}
}


struct TextSegment {
	size_t index;
	size_t start;
	size_t len;
	hb_script_t script;
	Font* font;

	TextSegment(
		size_t index,
		size_t start,
		size_t len,
		hb_script_t script,
		Font* font
	):index(index),start(start),len(len),script(script),font(font){ }
};


void AppendNewSegment(
	List<TextSegment>& segments,
	Array<CPInfo>& cps,
	size_t start, size_t len, size_t script_start,
	hb_script_t script,
	FontCollection* ff
) {
	Font* font = nullptr;
	if (len > 0) {
		font = ff->GetFirstFont();
		while (font != nullptr) {
			if (font->GetGlyphIndex(cps.Get(start + script_start).codepoint) != 0)
				break;
			font = font->Next();
		}
	}

	segments.PushBack(TextSegment(segments.GetSize(), start, len, script, font));
}


void SplitByScript(
	Array<CPInfo>& cps, 
	size_t start, 
	size_t len, 
	List<TextSegment>& segments, 
	FontCollection* ff
) {
	hb_script_t curr_script = HB_SCRIPT_COMMON;
	bool curr_is_digit = false;
	size_t last = start;
	size_t script_start = 0;
	for (size_t i = 0;i < len;++i) {
		uint32_t cp = cps.Get(start + i).codepoint;
		if (i == 0) {
			curr_script = hb_unicode_script(hb_unicode_funcs_get_default(), cp);
			curr_is_digit = (cp >= 0x30 && cp <= 0x39);
			continue;
		}
		hb_script_t script = hb_unicode_script(hb_unicode_funcs_get_default(), cp);
		bool is_digit = (cp >= 0x30 && cp <= 0x39);
		if (script == curr_script && curr_is_digit == is_digit)
			continue;
		if ((script == HB_SCRIPT_COMMON || script == HB_SCRIPT_INHERITED) && !is_digit)
			continue;
		if ((curr_script == HB_SCRIPT_COMMON || curr_script == HB_SCRIPT_INHERITED) && !curr_is_digit) {
			curr_script = script;
			curr_is_digit = is_digit;
			script_start = i;
			continue;
		}
		AppendNewSegment(segments, cps, last, start + i - last, script_start, curr_script, ff);
		last = start + i;
		curr_script = script;
		curr_is_digit = is_digit;
	}
	AppendNewSegment(segments, cps, last, start + len - last, script_start, curr_script, ff);
}


void ReverseMap(const Array<MappedGlyph>& mapped, Array<CPInfo>& cps, size_t begin, size_t line_index, bool is_ltr) {
	size_t gn = mapped.GetSize();
	if (gn == 0)
		return;
	size_t cluster;
	size_t start;
	for (size_t i = begin;i <= gn;++i) {
		if (i == begin) {
			cluster = mapped.Get(i).map;
			start = i;
		}
		else if (i == gn || mapped.Get(i).map != cluster) {
			CPInfo& cpi = cps.Get(cluster);
			CP_FLAG_SET(cpi.flags, CP_FLAG_IS_RTL, !is_ltr);
			CP_FLAG_SET(cpi.flags, CP_FLAG_MAPPED, true);
			cpi.line = line_index;
			cpi.start = start;
			cpi.len = i - start;
			if (i < gn) {
				cluster = mapped.Get(i).map;
				start = i;
			}
		}
	}
}


void Paragraph::SloveLayout() {
	for (size_t i = 0;i < cps.GetSize();++i)
		CP_FLAG_SET(cps.Get(i).flags,CP_FLAG_MAPPED,false);
	this->ClearLines();
	if (this->sba == nullptr)
		return;
	float line_width = 0;
	GlyphInfo gi;
	float real_adv;
	if (this->sba != nullptr) {
		SBUInteger run_num = SBLineGetRunCount(this->sbl);
		const SBRun* runs = SBLineGetRunsPtr(this->sbl);
		float max_adv = this->ff->GetMaxAdvance();
		for (SBUInteger i = 0; i < run_num; i++) {
			bool is_ltr = runs[i].level % 2 == 0;
			List<TextSegment> segments;
			//Split text into segment with same direction and script
			SplitByScript(this->cps, runs[i].offset, runs[i].length, segments, this->ff);
			bool next;
			for (auto j = segments.GetFront();!j.IsNull();j.Next()) {
				next = true;
				TextSegment& segment = j.Data();
				if (segment.font == nullptr) {
					for (size_t x = segment.start;x < segment.len;++x) {
						if (this->warp_width > 0 && line_width + max_adv > warp_width)
							this->_AppendNewLine();
						this->_GetLastLine()->Append(0, x, this->cps.Get(x).codepoint, is_ltr, nullptr, ff);
					}
				}
				else {
					size_t lb = 0;
					hb_buffer_t* hb_buffer = hb_buffer_create();
					hb_buffer_set_content_type(hb_buffer, HB_BUFFER_CONTENT_TYPE_UNICODE);
					for (size_t k = 0;k < segment.len; ++k)
						hb_buffer_add(hb_buffer, this->cps.Get(segment.start + k).codepoint, segment.start + k);
					hb_buffer_set_script(hb_buffer, segment.script);
					hb_buffer_guess_segment_properties(hb_buffer);
					uint32_t gn;
					hb_shape(segment.font->GetHBFont(), hb_buffer, NULL, 0);
					hb_glyph_info_t* gis = hb_buffer_get_glyph_infos(hb_buffer, &gn);
					hb_glyph_position_t* gps = hb_buffer_get_glyph_positions(hb_buffer, &gn);
					size_t k = 0;
					float segment_width = 0;
					TextLine* last_line;
					auto temp = j;
					temp.Next();
					bool incomplete = (!temp.IsNull() && j.Data().index == temp.Data().index);
					bool skip = false;
					while (k < gn && !skip) {
						if (!FetchGlyph(gis[is_ltr ? k : gn - 1 - k].codepoint, segment.font, gi, ff))
							real_adv = max_adv;
						else {
							if (is_ltr)
								real_adv = gi.advance_x;
							else
								real_adv = gi.advance_x - gi.offset_x;
						}

						if (
							this->warp_width > 0 
							&& line_width + segment_width + real_adv >= this->warp_width 
							&& this->cps.Get(gis[is_ltr ? k : gn - 1 - k].cluster).codepoint != ' '
							) {
							size_t bk = k;
							while (bk > lb) {
								if (CP_FLAG_GET(this->cps.Get(gis[is_ltr ? bk : gn - 1 - bk].cluster).flags,CP_FLAG_CAN_BREAK))
									break;
								--bk;
							}

							TextLine* last_line = this->_GetLastLine();
							if (bk == lb) {
								if (last_line->glyphs.GetSize() == 0) {
									if (k == lb)
										k = lb + 1;
									//Emergency break
									hb_glyph_flags_t break_flag = (k < gn ? hb_glyph_info_get_glyph_flags(&gis[is_ltr ? k : gn - 1 - k]) : (hb_glyph_flags_t)0);
									if (break_flag & HB_GLYPH_FLAG_UNSAFE_TO_BREAK) {
										//Split segment
										size_t lb_unmap = gis[is_ltr ? lb : gn - 1 - lb].cluster;
										size_t k_unmap = gis[is_ltr ? k : gn - 1 - k].cluster;
										auto temp = j;
										temp.Next();
										if (!temp.IsNull() && j.Data().index == temp.Data().index) {
											temp.Data().len = temp.Data().start + temp.Data().len - k_unmap;
											temp.Data().start = k_unmap;
										}
										else {
											segments.InsertAfter(j, {
												segment.index,
												k_unmap,
												segment.len - (k_unmap - segment.start),
												segment.script,
												segment.font
												});
										}
										segment.len = k_unmap - lb_unmap;
										segment.start = lb_unmap;
										skip = true;
										next = false;
										continue;
									}
									else {
										bk = k;
										if (incomplete) {
											size_t bk_unmap = bk < gn ? gis[is_ltr ? bk : gn - 1 - bk].cluster : segment.start + segment.len;
											temp.Data().len = temp.Data().start + temp.Data().len - bk_unmap;
											temp.Data().start = bk_unmap;
											skip = true;
										}
									}
								}
								else {
									//Break line in the begining of this segment
									k = lb;
									line_width = 0;
									segment_width = 0;
									this->_AppendNewLine();
									continue;
								}
							}
							else if (incomplete) {
								size_t bk_unmap= gis[is_ltr ? bk : gn - 1 - bk].cluster;
								temp.Data().len = temp.Data().start + temp.Data().len - bk_unmap;
								temp.Data().start = bk_unmap;
								skip = true;
							}

							//Append to last line
							size_t begin = last_line->glyphs.GetSize();
							for (size_t x = is_ltr ? lb : gn - bk; x <(is_ltr ? bk : gn-lb); ++x)
								last_line->Append(gis[x].codepoint, gis[x].cluster, this->cps.Get(gis[x].cluster).codepoint, is_ltr, segment.font, this->ff);
							ReverseMap(last_line->glyphs, this->cps, begin, last_line->index, is_ltr);
							k = bk;
							lb = k;
							segment_width = 0;
							line_width = 0;
							this->_AppendNewLine();
							continue;
						}
						else
							segment_width += gi.advance_x;
						++k;
					}
					if (!skip) {
						last_line = this->_GetLastLine();
						size_t begin = last_line->glyphs.GetSize();
						for (size_t x = is_ltr ? lb : gn - k;x < (is_ltr ? k : gn - lb); ++x)
							last_line->Append(gis[x].codepoint, gis[x].cluster, this->cps.Get(gis[x].cluster).codepoint, is_ltr, segment.font, this->ff);
						ReverseMap(last_line->glyphs, this->cps, begin, last_line->index, is_ltr);
						line_width += segment_width;
						if (incomplete) {
							this->_AppendNewLine();
							line_width = 0;
						}
					}
					hb_buffer_destroy(hb_buffer);
				}
			}
		}
	}
}


void Paragraph::ClearLines() {
	for (size_t i = 0;i < this->lines.GetSize();++i) {
		delete this->lines.Get(i);
	}
	this->lines.Clear();
}


Paragraph::~Paragraph() {
	this->cps.Clear();
	this->SloveBidi();
	this->ClearLines();
}


//////////////
//TextEngine//
//////////////
Paragraph* TextEngine::_GetLastParagraph() {
	if (this->paragraphs.GetSize() == 0)
		this->paragraphs.Push(new Paragraph(this->ff, this->warp_width));
	return this->paragraphs.Get(this->paragraphs.GetSize() - 1);
}


void TextEngine::_DecodeUtf8(const char* utf8_str, size_t len, Array<CPInfo>& cps) {
	size_t i = 0;
	while (i < len) {
		uint32_t code;
		int nb = UTF8Decode((const unsigned char*)utf8_str, i, len, code);
		if (nb > 0) {
			cps.Push(CPInfo(code));
			i += nb;
		}
		else
			++i;
	}
}


void TextEngine::_DeleteParagraphs(size_t start, size_t len) {
	if (len > 0) {
		for (size_t j = 0;j < len;++j)
			delete this->paragraphs.Get(start + j);
		size_t old_size = this->paragraphs.GetSize();
		for (size_t j = start + len;j < old_size;++j)
			this->paragraphs.Set(j - len, this->paragraphs.Get(j));
		this->paragraphs.SetCapacity(old_size - len);
	}
}


void SloveLineBreak(Array<CPInfo>& cps) {
	size_t cp_num = cps.GetSize();
	if (cp_num == 0)
		return;
	for (size_t i = 0;i < cp_num;++i)
		CP_FLAG_SET(cps.Get(i).flags, CP_FLAG_CAN_BREAK, false);
	LineBreaker lb(&cps, cp_num, CodepointAt);
	LineBreaker::Break br;
	while (lb.NextBreak(br))
		if (br.position < cp_num)
			CP_FLAG_SET(cps.Get(br.position).flags, CP_FLAG_CAN_BREAK, true);
}


void TextEngine::_Append(const Array<CPInfo>& cps) {
	size_t cp_num = cps.GetSize();
	LineBreaker lb(reinterpret_cast<const void*>(&cps), cp_num, CodepointAt);
	LineBreaker::Break br;
	size_t begin = 0;
	while (lb.NextBreak(br)) {
		if (br.position < cp_num)
			CP_FLAG_SET(cps.Get(br.position).flags, CP_FLAG_CAN_BREAK, true);
		if (br.required || br.position == cp_num) {
			bool temp = cps.Get(cp_num - 1).codepoint == '\n';
			Paragraph* last = this->_GetLastParagraph();
			bool lb_reslove = last->cps.GetSize() > 0;
			for (size_t i = begin;i < br.position;++i)
				if (cps.Get(i).codepoint != '\n')
					last->cps.Push(cps.Get(i));
			if (lb_reslove)
				SloveLineBreak(last->cps);
			last->SloveBidi();
			last->SloveLayout();
			if((br.position == cp_num && cps.Get(cp_num - 1).codepoint == '\n') || br.required)
				this->paragraphs.Push(new Paragraph(this->ff, this->warp_width));
			begin = br.position;
		}
	}
}


#define PARAGRAPH_NUM this->paragraphs.GetSize()
#define PARAGRAPH(p) this->paragraphs.Get(p)
#define LINE_NUM(p) this->paragraphs.Get(p)->lines.GetSize()
#define GLYPH_NUM(p,l) this->paragraphs.Get(p)->lines.Get(l)->glyphs.GetSize()
#define CP_NUM(p) this->paragraphs.Get(p)->cps.GetSize()
#define CP(p,i) this->paragraphs.Get(p)->cps.Get(i)
#define SET_CP(p,i,v) this->paragraphs.Get(p)->cps.Set(i,v)
#define LINE(p,l) this->paragraphs.Get(p)->lines.Get(l)
#define GLYPH(p,l,g) this->paragraphs.Get(p)->lines.Get(l)->glyphs.Get(g)
#define UINT_DECREASE(exp) (exp>0?exp-1:0)


CPPos TextEngine::_GPos2CPPos(const GlyphPos& gp) {
	if (PARAGRAPH_NUM == 0)
		return CPPos();
	if (LINE_NUM(gp.paragraph) == 0 || GLYPH_NUM(gp.paragraph, gp.line) == 0)
		return CPPos(gp.paragraph);
	if (gp.glyph < GLYPH_NUM(gp.paragraph, gp.line)) {
		if (GLYPH(gp.paragraph, gp.line, gp.glyph).is_ltr) {
			CPPos ret(gp.paragraph, GLYPH(gp.paragraph, gp.line, gp.glyph).map, false);
			if (gp.after) {
				if (gp.glyph + 1 < GLYPH_NUM(gp.paragraph, gp.line))
					ret = this->_PreNextMappedCP(ret, true);
				else if(gp.line+1<LINE_NUM(gp.paragraph))
					ret.eol = true;
				else
					ret = this->_PreNextMappedCP(ret, true);
			}
			return ret;
		}
		else {
			CPPos ret(gp.paragraph, GLYPH(gp.paragraph, gp.line, gp.glyph).map, false);
			if (!gp.after) {
				if (gp.glyph == 0) {
					if (gp.line + 1 < LINE_NUM(gp.paragraph))
						ret.eol = true;
					else
						ret = this->_PreNextMappedCP(ret, true);
				}
				else
					ret = this->_PreNextMappedCP(ret, true);
			}

			return ret;
		}
	}
	else {
		CPPos ret(gp.paragraph, GLYPH(gp.paragraph, gp.line, GLYPH_NUM(gp.paragraph, gp.line) - 1).map, false);
		if (GLYPH(gp.paragraph, gp.line, GLYPH_NUM(gp.paragraph, gp.line) - 1).is_ltr) {
			if (gp.line + 1 < LINE_NUM(gp.paragraph))
				ret.eol = true;
			else
				ret = this->_PreNextMappedCP(ret, true);
		}
		return ret;
	}
}


bool TextEngine::_CPPos2GPos(const CPPos& cpp, GlyphPos& gp) {
	if (PARAGRAPH_NUM == 0)
		return false;
	if (cpp.cp < CP_NUM(cpp.paragraph)) {
		if (!CP_FLAG_GET(CP(cpp.paragraph, cpp.cp).flags, CP_FLAG_MAPPED))
			return false;
		gp.after = false;
		gp.paragraph = cpp.paragraph;
		gp.line = CP(cpp.paragraph, cpp.cp).line;
		gp.glyph = CP(cpp.paragraph, cpp.cp).start;
		if (!CP_FLAG_GET(CP(cpp.paragraph, cpp.cp).flags,CP_FLAG_IS_RTL)) {
			if (cpp.eol) {
				gp.glyph += UINT_DECREASE(CP(cpp.paragraph, cpp.cp).len);
				gp.after = true;
			}
		}
		else {
			CPPos pre = _PreNextMappedCP(cpp, false);
			if (!cpp.eol) {
				if (
					pre.paragraph == cpp.paragraph
					&& CP(pre.paragraph, pre.cp).line == CP(cpp.paragraph, cpp.cp).line
					&& pre.cp != cpp.cp
					&& !CP_FLAG_GET(CP(pre.paragraph, pre.cp).flags, CP_FLAG_IS_RTL)
					)
					gp.glyph = CP(pre.paragraph, pre.cp).start + UINT_DECREASE(CP(pre.paragraph, pre.cp).len);
				else
					gp.glyph+= UINT_DECREASE(CP(cpp.paragraph, cpp.cp).len);
				gp.after = true;
			}
		}
	}
	else {
		gp.paragraph = cpp.paragraph;
		gp.line = 0;
		gp.glyph = 0;
		gp.after = false;
		if (CP_NUM(cpp.paragraph) > 0) {
			size_t pre = cpp.cp - 1;
			while (!CP_FLAG_GET(CP(cpp.paragraph, pre).flags, CP_FLAG_MAPPED) && pre > 0)
				--pre;
			if (CP_FLAG_GET(CP(cpp.paragraph, pre).flags,CP_FLAG_MAPPED)) {
				gp.line = CP(cpp.paragraph, pre).line;
				gp.glyph = CP(cpp.paragraph, pre).start;
				if (!CP_FLAG_GET(CP(cpp.paragraph, pre).flags, CP_FLAG_IS_RTL)) {
					gp.glyph += UINT_DECREASE(CP(cpp.paragraph, pre).len);
					gp.after = true;
				}
			}
		}
	}
	return true;
}


CPPos TextEngine::_NextCodepoint(const CPPos& cpp) {
	CPPos ret;
	if (PARAGRAPH_NUM == 0)
		return ret;
	if (cpp.cp < CP_NUM(cpp.paragraph)) {
		ret.paragraph = cpp.paragraph;
		ret.cp = cpp.cp + 1;
	}
	else {
		if (cpp.paragraph + 1 < this->paragraphs.GetSize()) {
			ret.paragraph = cpp.paragraph + 1;
			ret.cp = 0;
		}
		else
			return CPPos(cpp.paragraph, cpp.cp, false);
	}

	return cpp.eol ? _NextCodepoint(ret) : ret;
}


CPPos TextEngine::PreCodepoint(const CPPos& cpp) {
	if (PARAGRAPH_NUM == 0)
		return CPPos();
	if (cpp.eol)
		return CPPos(cpp.paragraph, cpp.cp, false);
	if (cpp.cp > 0)
		return CPPos(cpp.paragraph, cpp.cp - 1, false);
	else{
		if (cpp.paragraph == 0)
			return cpp;
		else
			return CPPos(cpp.paragraph - 1, CP_NUM(cpp.paragraph - 1), false);
	}
}


CPPos TextEngine::_PreNextMappedCP(const CPPos& cpp, bool is_next) {
	CPPos ret;
	if (PARAGRAPH_NUM == 0)
		return ret;
	CPPos old = cpp;
	while (true) {
		ret = is_next ? this->_NextCodepoint(old) : this->PreCodepoint(old);
		if (ret == old)
			break;
		if (ret.cp < CP_NUM(ret.paragraph)) {
			if (CP_FLAG_GET(CP(ret.paragraph, ret.cp).flags, CP_FLAG_MAPPED))
				break;
		}
		else
			break;
		old = ret;
	}
	return ret;
}


GlyphPos TextEngine::_FindGlyphPos(size_t paragraph, size_t line, float x) {
	if (LINE_NUM(paragraph) == 0)
		return GlyphPos(paragraph, 0, 0, false);
	float w = 0;
	size_t g = 0;
	size_t cluster_start = g;
	float cluster_adv = 0;
	while (g < GLYPH_NUM(paragraph, line)) {
		if (GLYPH(paragraph, line, g).map == GLYPH(paragraph, line, cluster_start).map)
			cluster_adv += GLYPH(paragraph, line, g).gi.advance_x;
		else {
			if (x >= w && x < w + cluster_adv) {
				if (x < w + cluster_adv / 2)
					return GlyphPos(paragraph, line, cluster_start, false);
				else
					return GlyphPos(paragraph, line, cluster_start, true);
			}
			w += cluster_adv;
			cluster_start = g;
			cluster_adv = 0;
			continue;
		}
		++g;
	}
	if (x < w + cluster_adv / 2)
		return GlyphPos(paragraph, line, cluster_start, false);
	else
		return GlyphPos(paragraph, line, cluster_start, true);
}


float TextEngine::_ComputeCursorPosX(const GlyphPos& gp) {
	float x = 0;
	size_t g_max = gp.glyph;
	if (gp.after)
		++g_max;
	if (LINE_NUM(gp.paragraph) > 0)
		g_max = g_max > GLYPH_NUM(gp.paragraph, gp.line) ? GLYPH_NUM(gp.paragraph, gp.line) : g_max;
	else
		g_max = 0;
	for (size_t i = 0;i < g_max;++i)
		x += GLYPH(gp.paragraph, gp.line, i).gi.advance_x;
	return x;
}


TextEngine::TextEngine(FontCollection* ff, float warp_width):ff(ff),warp_width(warp_width){}


void TextEngine::SetWarpWidth(float width) {
	this->warp_width = warp_width;
	for (size_t i = 0;i < this->paragraphs.GetSize();++i) {
		paragraphs.Get(i)->warp_width = width;
		paragraphs.Get(i)->SloveLayout();
	}
}


float TextEngine::GetWarpWidth() {
	return this->warp_width;
}


void TextEngine::SetAlignMode(int mode) {
	this->align_mode = mode;
}


int TextEngine::GetAlignMode() {
	return this->align_mode;
}


void TextEngine::Clear() {
	for (size_t i = 0;i < this->paragraphs.GetSize();++i)
		delete paragraphs.Get(i);
	paragraphs.Clear();
}


void TextEngine::Append(const char* utf8_str, size_t len) {
	Array<CPInfo> cps;
	this->_DecodeUtf8(utf8_str, len, cps);
	if (cps.GetSize() == 0)
		return;
	this->_Append(cps);
}


CPPos TextEngine::Insert(const char* utf8_str, size_t len, const CPPos& pos) {
	CPPos _pos = pos.eol ? this->_PreNextMappedCP(CPPos(pos.paragraph, pos.cp, false), true) : pos;
	Array<CPInfo> cps;
	this->_DecodeUtf8(utf8_str, len, cps);
	size_t cp_num = cps.GetSize();
	if (cp_num == 0)
		return pos;

	Array<size_t> segments;
	LineBreaker lb(&cps, cp_num, CodepointAt);
	LineBreaker::Break br;
	size_t begin = 0;
	while (lb.NextBreak(br)) {
		if (br.position < cp_num)
			CP_FLAG_SET(cps.Get(br.position).flags, CP_FLAG_CAN_BREAK, true);
		if (br.required || br.position == cp_num) {
			segments.Push(br.position);
			if (br.position == cp_num && cps.Get(br.position - 1).codepoint == '\n')
				segments.Push(br.position);
		}

	}

	size_t pn = this->paragraphs.GetSize();
	CPPos ret;

	if (_pos.paragraph >= pn) {
		this->_Append(cps);
		ret.paragraph = UINT_DECREASE(this->paragraphs.GetSize());
		ret.cp = PARAGRAPH_NUM > 0 ? CP_NUM(PARAGRAPH_NUM - 1) : 0;
	}
	else {
		if (segments.GetSize() == 1) {
			Paragraph* pi = PARAGRAPH(_pos.paragraph);
			if (CP_NUM(_pos.paragraph) > 0) {
				for (size_t i = CP_NUM(_pos.paragraph) - 1;i >= _pos.cp;--i) {
					pi->cps.Set(i + segments.Get(0), pi->cps.Get(i));
					if (i == 0)
						break;
				}
			}
			for (size_t i = 0;i < segments.Get(0);++i)
				pi->cps.Set(_pos.cp + i, cps.Get(i));
			ret.paragraph = _pos.paragraph;
			ret.cp = _pos.cp + segments.Get(0);
			SloveLineBreak(pi->cps);
			pi->SloveBidi();
			pi->SloveLayout();
		}
		else {
			Array<Paragraph*> ps;
			size_t sn = segments.GetSize();
			Paragraph* last = new Paragraph(this->ff, this->warp_width);
			for (size_t i = 0;i < sn;++i) {
				if (i == 0) {
					size_t last_s = segments.GetSize() - 1;
					for (size_t j = segments.Get(last_s - 1);j < segments.Get(last_s);++j)
						if (cps.Get(j).codepoint != '\n')
							last->cps.Push(cps.Get(j));
					ret.cp = last->cps.GetSize();
					Paragraph* pi = PARAGRAPH(_pos.paragraph);
					for (size_t j = _pos.cp;j < pi->cps.GetSize();++j)
						last->cps.Push(pi->cps.Get(j));
					pi->cps.SetCapacity(_pos.cp);
					for (size_t j = 0;j < segments.Get(0);++j)
						if (cps.Get(j).codepoint != '\n')
							pi->cps.Push(cps.Get(j));
					SloveLineBreak(pi->cps);
					pi->SloveBidi();
					pi->SloveLayout();
				}
				else if (i == sn - 1) {
					SloveLineBreak(last->cps);
					last->SloveBidi();
					last->SloveLayout();
					ps.Push(last);
				}
				else {
					Paragraph* np = new Paragraph(this->ff, this->warp_width);
					for (size_t j = segments.Get(i - 1);j < segments.Get(i);++j)
						if (cps.Get(j).codepoint != '\n')
							np->cps.Push(cps.Get(j));
					np->SloveBidi();
					np->SloveLayout();
					ps.Push(np);
				}
			}
			size_t i = PARAGRAPH_NUM - 1;
			while (i > _pos.paragraph) {
				this->paragraphs.Set(i + ps.GetSize(), this->paragraphs.Get(i));
				if (i == 0)
					break;
				--i;
			}
			for (size_t i = 0;i < ps.GetSize();++i)
				this->paragraphs.Set(_pos.paragraph + 1 + i, ps.Get(i));
			ret.paragraph = _pos.paragraph + ps.GetSize();
		}
	}
	return ret;
}


void TextEngine::Delete(const CPPos& a, const CPPos& b) {
	CPPos _a = a.eol ? this->_PreNextMappedCP(CPPos(a.paragraph, a.cp, false), true) : a;
	CPPos _b = b.eol ? this->_PreNextMappedCP(CPPos(b.paragraph, b.cp, false), true) : b;
	if (_a == _b)
		return;
	if (_b.paragraph < _a.paragraph)
		CP_SWAP(_a, _b)
	else if (_a.paragraph == _b.paragraph && _b.cp < _a.cp)
		CP_SWAP(_a, _b);

	if (_a.paragraph == _b.paragraph) {
		for (size_t i = _b.cp;i < CP_NUM(_a.paragraph);++i)
			SET_CP(_a.paragraph, i - (_b.cp - _a.cp), CP(_a.paragraph, i));
		PARAGRAPH(_a.paragraph)->cps.SetCapacity(CP_NUM(_a.paragraph) - (_b.cp - _a.cp));
	}
	else {
		for (size_t p = _a.paragraph;p <= _b.paragraph;++p) {
			if (p == _a.paragraph)
				PARAGRAPH(p)->cps.SetCapacity(_a.cp);
			else if (p == _b.paragraph) {
				for (size_t i = _b.cp;i < CP_NUM(_b.paragraph);++i)
					PARAGRAPH(_a.paragraph)->cps.Push(CP(_b.paragraph, i));
			}
		}
		this->_DeleteParagraphs(_a.paragraph + 1, _b.paragraph - _a.paragraph);
	}
	SloveLineBreak(PARAGRAPH(_a.paragraph)->cps);
	PARAGRAPH(_a.paragraph)->SloveBidi();
	PARAGRAPH(_a.paragraph)->SloveLayout();
}


CPPos TextEngine::Replace(const char* utf8_str, size_t len, const CPPos& a, const CPPos& b) {
	CPPos _a = a.eol ? this->_PreNextMappedCP(CPPos(a.paragraph, a.cp, false), true) : a;
	CPPos _b = b.eol ? this->_PreNextMappedCP(CPPos(b.paragraph, b.cp, false), true) : b;
	if (_a == _b)
		return this->Insert(utf8_str, len, _a);
	if (_b.paragraph < _a.paragraph)
		CP_SWAP(_a, _b)
	else if (_a.paragraph == _b.paragraph && _b.cp < _a.cp)
		CP_SWAP(_a, _b);

	this->Delete(_a, _b);
	return this->Insert(utf8_str, len, _a);
}


size_t TextEngine::GetParagarphNum() {
	return this->paragraphs.GetSize();
}


Paragraph* TextEngine::GetParagraph(size_t index) {
	return this->paragraphs.Get(index);
}


CPPos TextEngine::Hit(float lh, float x, float y) {
	x = x > 0 ? x : 0;
	y = y > 0 ? y : 0;
	float h = 0;
	size_t pn = this->GetParagarphNum();
	if (pn == 0)
		return CPPos();
	GlyphPos hit_pos;
	CPPos ret;
	for (size_t p = 0;p < pn;++p) {
		if (LINE_NUM(p) == 0) {
			if (y >= h && y < h + lh)
				return CPPos(p);
			else
				h += lh;
		}
		for (size_t l = 0;l < LINE_NUM(p);) {
			if (y >= h && y < h + lh) {
				return this->_GPos2CPPos(this->_FindGlyphPos(p, l, x));
			}
			else {
				if (l + 1 < LINE_NUM(p) || p + 1<pn) {
					h += lh;
					++l;
				}
				else
					h = y;
			}
		}
	}
	return CPPos(UINT_DECREASE(pn));
}


bool TextEngine::ComputeCursorPos(const CPPos& pos, float lh, float& x, float& y) {
	GlyphPos gp;
	if (!this->_CPPos2GPos(pos, gp))
		return false;
	x = 0;y = 0;
	if (this->GetParagarphNum() > 0) {
		for (size_t i = 0;i < gp.paragraph;++i)
			y += LINE_NUM(i) == 0 ? lh : LINE_NUM(i) * lh;
		if (gp.line < LINE_NUM(gp.paragraph) && gp.glyph <= GLYPH_NUM(gp.paragraph, gp.line)) {
			y += gp.line * lh;
			x = this->_ComputeCursorPosX(gp);
		}
		else
			return false;
	}
	return true;
}


CPPos TextEngine::CheckCursorPos(const CPPos& cpp) {
	if (this->paragraphs.GetSize() == 0)
		return CPPos();
	if (cpp.cp > CP_NUM(cpp.paragraph))
		return { cpp.paragraph,CP_NUM(cpp.paragraph)};
	if (cpp.cp < CP_NUM(cpp.paragraph)) {
		if (!CP_FLAG_GET(CP(cpp.paragraph, cpp.cp).flags, CP_FLAG_MAPPED))
			return { cpp.paragraph,0 };
		if (cpp.eol) {
			if (!CP_FLAG_GET(CP(cpp.paragraph, cpp.cp).flags, CP_FLAG_IS_RTL)) {
				if (CP(cpp.paragraph, cpp.cp).start + CP(cpp.paragraph, cpp.cp).len < GLYPH_NUM(cpp.paragraph, CP(cpp.paragraph, cpp.cp).line))
					return this->_PreNextMappedCP(CPPos(cpp.paragraph, cpp.cp, false), true);
			}
			else {
				if (CP(cpp.paragraph, cpp.cp).start > 0)
					return this->_PreNextMappedCP(CPPos(cpp.paragraph, cpp.cp, false), true);
			}
		}
	}
	return cpp;
}


float Abs(float value) {
	return value > 0 ? value : -value;
}


void TextEngine::_FindNearestGlyph(const GlyphPos& before, GlyphPos& after) {
	size_t w_before = this->_ComputeCursorPosX(before);

	after = this->_FindGlyphPos(after.paragraph, after.line, w_before);
}


CPPos TextEngine::CursorUp(const CPPos& pos) {
	GlyphPos gp;
	if (!this->_CPPos2GPos(pos, gp))
		return pos;
	GlyphPos ret = { 0,0,0 };
	if (gp.line > 0) {
		ret.line = gp.line - 1;
		ret.paragraph = pos.paragraph;
	}
	else if (pos.paragraph > 0) {
		ret.paragraph = pos.paragraph - 1;
		if (LINE_NUM(ret.paragraph) > 0)
			ret.line = LINE_NUM(ret.paragraph) - 1;
		else
			ret.line = 0;
	}
	else
		ret = gp;

	if (ret.line != gp.line || ret.paragraph != gp.paragraph)
		this->_FindNearestGlyph(gp, ret);
	else
		return pos;

	return this->_GPos2CPPos(ret);
}


CPPos TextEngine::CursorDown(const CPPos& pos) {
	GlyphPos gp;
	if (!this->_CPPos2GPos(pos, gp))
		return pos;
	GlyphPos ret = { 0,0,0 };
	if (this->paragraphs.GetSize() == 0)
		return pos;
	if (
		this->paragraphs.Get(gp.paragraph)->lines.GetSize()>0 
		&& gp.line < this->paragraphs.Get(gp.paragraph)->lines.GetSize() - 1
		) {
		ret.line = gp.line + 1;
		ret.paragraph = gp.paragraph;
	}
	else if (gp.paragraph < this->paragraphs.GetSize() - 1) {
		ret.paragraph = gp.paragraph + 1;
		ret.line = 0;
	}
	else
		ret = gp;

	if (ret.line != gp.line || ret.paragraph != gp.paragraph)
		this->_FindNearestGlyph(gp, ret);
	else
		return pos;

	return this->_GPos2CPPos(ret);
}


CPPos TextEngine::CursorLeft(const CPPos& pos) {
	return this->_PreNextMappedCP(pos, false);
}


CPPos TextEngine::CursorRight(const CPPos& pos) {
	return this->_PreNextMappedCP(pos, true);
}


void SwapCP(CPPos& a, CPPos& b) {
	CPPos temp = a;
	a = b;
	b = temp;
}


bool TextEngine::IsGlyphInRange(const GlyphPos& gp, const CPPos& a, const CPPos& b) {
	CPPos _a = a.eol ? this->_PreNextMappedCP(CPPos(a.paragraph, a.cp, false), true) : a;
	CPPos _b = b.eol ? this->_PreNextMappedCP(CPPos(b.paragraph, b.cp, false), true) : b;
	if (_a == _b)
		return false;
	if (_b.paragraph < _a.paragraph)
		CP_SWAP(_a, _b)
	else if (_a.paragraph == _b.paragraph && _b.cp < _a.cp)
		CP_SWAP(_a, _b);
	CPPos cpp = this->_GPos2CPPos(GlyphPos(gp.paragraph, gp.line, gp.glyph, false));
	if (
		gp.paragraph < this->paragraphs.GetSize()
		&& gp.line < LINE_NUM(gp.paragraph)
		&& gp.glyph < GLYPH_NUM(gp.paragraph, gp.line)
		&& !GLYPH(gp.paragraph, gp.line, gp.glyph).is_ltr
		)
		cpp = this->_GPos2CPPos(GlyphPos(gp.paragraph, gp.line, gp.glyph, true));
	if (cpp.paragraph < _a.paragraph || cpp.paragraph > _b.paragraph)
		return false;
	bool ret = true;
	if (cpp.paragraph == _a.paragraph) {
		if (cpp.cp >= _a.cp)
			ret &= true;
		else
			ret &= false;
	}
	if (cpp.paragraph == _b.paragraph) {
		if (cpp.cp < _b.cp)
			ret &= true;
		else
			ret &= false;
	}
	return ret;
}