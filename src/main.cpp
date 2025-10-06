//Copyright (C) 2025 Hongyi Chen (BeanPieChen)
//Licensed under the MIT License

#include <cstddef>
#include <SDL3/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <LineBreaker.h>
#include "UTF8Codec.h"
#include "FontCollection.h"
#include "TextEngine.h"
#include <iostream>


#define FONT_SIZE 64
#define LINE_GAP 8


uint8_t* LoadFile(const char* path, size_t* size) {
	FILE* fp = fopen(path, "rb");
	size_t file_size;
	uint8_t* buffer;
	if (!fp)
		goto open_fail;
	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	if (file_size == 0)
		goto read_fail;
	buffer = new uint8_t[file_size];
	if (buffer == nullptr)
		goto read_fail;
	fseek(fp, 0, SEEK_SET);
	fread(buffer, file_size, 1, fp);
	fclose(fp);
	*size = file_size;
	return buffer;

read_fail:
	fclose(fp);
open_fail:
	*size = 0;
	return nullptr;
}

float text_height = 0;
float offset = 0;
float offset_max = 0;

CPPos spos = { 0,0,false };
CPPos ipos = { 0,0,false };

bool mouse_down = false;
float m_x, m_y;

float ComputeTextHeight(TextEngine* te) {
	float height = 0;
	size_t paragraph_num = te->GetParagarphNum();
	for (size_t i = 0;i < paragraph_num;++i) {
		Paragraph* paragraph = te->GetParagraph(i);
		size_t line_num = paragraph->lines.GetSize();
		if (line_num == 0)
			height += FONT_SIZE + LINE_GAP;
		for (int j = 0;j < line_num;++j)
			height += FONT_SIZE + LINE_GAP;
	}
	return height;
}


void UpdateOffsetMax(TextEngine* te, float win_h) {
	text_height = ComputeTextHeight(te);
	float a_offset_old = offset * offset_max;
	offset_max = text_height > win_h ? text_height - win_h : 0;
	offset = a_offset_old / offset_max;
	offset = offset > 0 ? (offset < 1 ? offset : 1) : 0;
}


void CheckCursor(float cursor_y, float win_h) {
	if (cursor_y + (FONT_SIZE + LINE_GAP) / 2 - offset * offset_max < 0)
		offset = cursor_y / offset_max;
	else if (cursor_y + (FONT_SIZE + LINE_GAP)/2 - offset * offset_max > win_h)
		offset = (cursor_y - win_h + FONT_SIZE + LINE_GAP) / offset_max;
	offset = offset > 0 ? (offset < 1 ? offset : 1) : 0;
}


void DrawBackground(
	uint8_t* dst, 
	int dst_w, int dst_h, 
	int x, int y, 
	int w, int h, 
	uint32_t rgba = 0xFF0000FFU
) {
	uint8_t r = (rgba >> 24) & 0xFFU;
	uint8_t g = (rgba >> 16) & 0xFFU;
	uint8_t b = (rgba >> 8) & 0xFFU;
	float a = static_cast<float>((rgba >> 0) & 0xFFU) / 255;
	for (int j = 0;j < h;++j) {
		if (y + j >= dst_h || y + j < 0)
			continue;
		uint8_t* target = dst + (y + j) * dst_w * 4 + x * 4;
		for (int i = 0;i < w;++i) {
			if (x + i >= dst_w || x + i < 0)
				continue;
			uint8_t* p = target + i * 4;
			p[0] = p[0] * (1 - a) + r * a;
			p[1] = p[1] * (1 - a) + g * a;
			p[2] = p[2] * (1 - a) + b * a;
		}
	}
}


void BlitGrayFont(
	const uint8_t* font_atlas,
	int x, int y,
	int w, int h,
	int atlas_w,
	uint8_t* dst,
	int dst_x, int dst_y,
	int dst_w, int dst_h,
	uint32_t rgba = 0xFFFFFFFFU
) {
	uint8_t r = (rgba >> 24) & 0xFFU;
	uint8_t g = (rgba >> 16) & 0xFFU;
	uint8_t b = (rgba >> 8) & 0xFFU;
	float a = static_cast<float>((rgba >> 0) & 0xFFU) / 255;

	for (int j = 0;j < h;++j) {
		if (dst_y + j >= dst_h || dst_y+j < 0)
			continue;
		uint8_t* target = dst + (dst_y + j) * dst_w * 4 + dst_x * 4;
		const uint8_t* src = font_atlas + (y + j) * atlas_w + x;
		for (int i = 0;i < w;++i) {
			if (dst_x + i >= dst_w || dst_x+i < 0)
				continue;
			float c = static_cast<float>(*(src + i)) / 255;
			c *= a;
			uint8_t* p = target + i * 4;
			p[0] = p[0] * (1 - c) + r * c;
			p[1] = p[1] * (1 - c) + g * c;
			p[2] = p[2] * (1 - c) + b * c;
		}
	}
}


void DrawText(
	SDL_Surface* surface,
	FontCollection* ff,
	TextEngine* te,
	float offset,
	bool is_ltr
) {
	SDL_LockSurface(surface);

	float pen_x = 0;
	float pen_y = FONT_SIZE - offset;
	GlyphInfo gi;
	float line_width = 0;

	size_t paragraph_num = te->GetParagarphNum();
	for (size_t i = 0;i < paragraph_num;++i) {
		Paragraph* paragraph = te->GetParagraph(i);
		size_t line_num = paragraph->lines.GetSize();
		if(line_num==0)
			pen_y += FONT_SIZE + LINE_GAP;
		for (size_t j = 0;j < line_num;++j) {
			if (pen_y - FONT_SIZE > surface->h)
				break;
			TextLine* line = paragraph->lines.Get(j);
			size_t gn = line->glyphs.GetSize();
			if (!is_ltr)
				pen_x = surface->w - line->width;
			for (size_t k = 0;k < gn;++k) {
				MappedGlyph mg = line->glyphs.Get(k);
				AtlasRegion ar = ff->GetAtlasRegion(mg.gi);
				float gx = pen_x + mg.gi.offset_x;
				float gy = pen_y - mg.gi.offset_y;
				if (gx < surface->w && gx + ar.width >= 0) {
					if (te->IsGlyphInRange({ i,j,k }, ipos, spos))
						DrawBackground(reinterpret_cast<uint8_t*>(surface->pixels),
							surface->w, surface->h,
							pen_x, pen_y - FONT_SIZE,
							mg.gi.advance_x, FONT_SIZE + LINE_GAP
						);
					BlitGrayFont(
						ff->GetAtlasBuffer(mg.gi, ar.face_index),
						ar.x, ar.y,
						ar.width, ar.height,
						ff->GetAtlasSize(mg.gi),
						reinterpret_cast<uint8_t*>(surface->pixels),
						pen_x + mg.gi.offset_x,
						pen_y - mg.gi.offset_y,
						surface->w, surface->h,
						0xFFFFFFFF
					);
				}
				pen_x += mg.gi.advance_x;
			}
			pen_x = 0;
			pen_y += FONT_SIZE + LINE_GAP;
		}
	}

	SDL_UnlockSurface(surface);
}


void DrawCursor(SDL_Surface* surface, float x, float y) {
	SDL_LockSurface(surface);
	if (x < 0 || x >= surface->w)
		return;
	uint8_t* dst = reinterpret_cast<uint8_t*>(surface->pixels);
	for (int i = y;i < y + FONT_SIZE + LINE_GAP;++i) {
		if (i > 0 && i < surface->h) {
			dst[(int)x * 4 + i * surface->w * 4 + 1] = 255;
			dst[(int)x * 4 + i * surface->w * 4 + 2] = 255;
			dst[(int)x * 4 + i * surface->w * 4 + 3] = 255;
			dst[(int)x * 4 + i * surface->w * 4 + 4] = 255;
		}
	}
	SDL_UnlockSurface(surface);
}


void UpdateText(SDL_Window* win, FontCollection* ff, TextEngine* te, bool check_cursor_pos) {
	SDL_Surface* surface = SDL_GetWindowSurface(win);
	SDL_ClearSurface(surface, 0, 0, 0, 0);	float cursor_x = 0, cursor_y = 0;
	te->ComputeCursorPos(ipos, FONT_SIZE + LINE_GAP, cursor_x, cursor_y);
	int win_w, win_h;
	SDL_GetWindowSizeInPixels(win, &win_w, &win_h);
	if(check_cursor_pos)
		CheckCursor(cursor_y, win_h);
	SDL_Rect input_area = {
		0,cursor_y,win_w,FONT_SIZE + LINE_GAP
	};
	SDL_SetTextInputArea(win, &input_area, cursor_x);
	DrawText(surface, ff, te, offset * offset_max, true);
	DrawCursor(surface, cursor_x, cursor_y - offset * offset_max);
	SDL_UpdateWindowSurface(win);
}


const char* font_path[] = {
	"fonts/special_elite.ttf",
	"fonts/NotoSansArabic-Regular.ttf",
};


uint8_t* font_file_buffer[] = {
	nullptr,
	nullptr,
};


unsigned int font_num = 2;


int main(int argc, char** argv) {
	int ret = 0;
	FT_Library ftl;
	SDL_Window* main_win;
	FT_Face face;
	FontCollection* ff;

	FT_Error err = FT_Init_FreeType(&ftl);
	if (err) {
		std::cout << "FreeType Init Failed" << std::endl;
		goto freetype_init_fail;
	}

	ff = new FontCollection(ftl, FONT_SIZE, false);

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cout << "SDL Init Failed" << std::endl;
		goto sdl_init_fail;
	}

	main_win = SDL_CreateWindow("TextEngineDemo", 640, 480, SDL_WINDOW_RESIZABLE);
	if (!main_win) {
		std::cout << "Main Window Creation Failed" << std::endl;
		goto win_init_fail;
	}

	{
		int i = 0;
		for (;i < font_num;++i) {
			size_t size;
			font_file_buffer[i] = LoadFile(font_path[i], &size);
			if (font_file_buffer[i] == nullptr)
				break;
			if (ff->AddFont(font_file_buffer[i], size) == nullptr)
				break;
		}
		if (i < font_num) {
			std::cout << "Font File Load Failed" << std::endl;
			goto font_load_fail;
		}
	}

	LineBreakInit();

	{
		const char* str = u8"The title is مفتاح معايير الويب in Arabic.";

		int w, h;
		SDL_GetWindowSizeInPixels(main_win, &w, &h);

		TextEngine te(ff, w);
		te.Append(str, strlen(str));
		text_height = ComputeTextHeight(&te);
		offset_max = text_height > h ? text_height - h : 0;

		UpdateText(main_win, ff, &te, false);

		bool loop = true;
		SDL_Event e;
		while (loop) {
			while (SDL_WaitEvent(&e) && loop) {
				if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
					loop = false;
				else if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
					te.SetWarpWidth(e.window.data1);
					ipos = te.CheckCursorPos(ipos);
					UpdateOffsetMax(&te, e.window.data1);
					UpdateText(main_win, ff, &te, false);
				}
				else if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
					SDL_StartTextInput(main_win);
				}
				else if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
					SDL_StopTextInput(main_win);
				}
				else if (e.type == SDL_EVENT_MOUSE_WHEEL) {
					if (offset_max > 0)
						offset += -e.wheel.y * 10 / offset_max;
					if (offset < 0)
						offset = 0;
					else if (offset > 1)
						offset = 1;
					if (mouse_down) {
						float cursor_x = 0, cursor_y = 0;
						ipos = te.Hit(FONT_SIZE + LINE_GAP, m_x, m_y + offset * offset_max);
					}
					UpdateText(main_win, ff, &te, false);
				}
				else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
					if (e.button.button == SDL_BUTTON_LEFT) {
						float cursor_x = 0, cursor_y = 0;
						ipos = te.Hit(FONT_SIZE + LINE_GAP, e.button.x, e.button.y + offset * offset_max);
						spos = ipos;
						UpdateText(main_win, ff, &te, false);
						mouse_down = true;
						m_x = e.button.x;
						m_y = e.button.y;
					}
				}
				else if (e.type == SDL_EVENT_MOUSE_MOTION) {
					if (mouse_down) {
						float cursor_x = 0, cursor_y = 0;
						ipos = te.Hit(FONT_SIZE + LINE_GAP, e.motion.x, e.motion.y + offset * offset_max);
						UpdateText(main_win, ff, &te, true);
						m_x = e.motion.x;
						m_y = e.motion.y;
					}
				}
				else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
					if (e.button.button == SDL_BUTTON_LEFT)
						mouse_down = false;
				}
				else if (e.type == SDL_EVENT_KEY_DOWN) {
					if (e.key.key == SDLK_UP)
						ipos = te.CursorUp(ipos);
					else if (e.key.key == SDLK_DOWN)
						ipos = te.CursorDown(ipos);
					else if (e.key.key == SDLK_LEFT)
						ipos = te.CursorLeft(ipos);
					else if (e.key.key == SDLK_RIGHT)
						ipos = te.CursorRight(ipos);
					else if (e.key.key == SDLK_BACKSPACE) {
						if (spos == ipos)
							spos = te.PreCodepoint(ipos);
						te.Delete(spos, ipos);
						if (ipos > spos)
							ipos = spos;
					}
					else if (e.key.key == SDLK_RETURN) {
						ipos = te.Insert("\n", 1, ipos);
					}
					else
						continue;
					spos = ipos;
					int win_w, win_h;
					SDL_GetWindowSizeInPixels(main_win, &win_w, &win_h);
					UpdateOffsetMax(&te, win_h);
					UpdateText(main_win, ff, &te, true);
				}
				else if (e.type == SDL_EVENT_TEXT_INPUT){
					if (ipos == spos)
						ipos = te.Insert(e.text.text, strlen(e.text.text), ipos);
					else
						ipos = te.Replace(e.text.text, strlen(e.text.text), ipos, spos);
					spos = ipos;
					int win_w, win_h;
					SDL_GetWindowSizeInPixels(main_win, &win_w, &win_h);
					UpdateOffsetMax(&te, win_h);
					UpdateText(main_win, ff, &te, true);
					std::cout << "in:\"" << e.text.text << "\"" << std::endl;
				}
			}
		}
	}

	LineBreakExit();

	ff->ClearFonts();
	for (int i = 0;i < font_num;++i)
		if (font_file_buffer[i] != nullptr)
			delete[] font_file_buffer[i];
font_load_fail:
	delete ff;
	SDL_DestroyWindow(main_win);
win_init_fail:
	SDL_Quit();
sdl_init_fail:
	FT_Done_FreeType(ftl);
freetype_init_fail:
	return 0;
}