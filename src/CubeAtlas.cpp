/*
* Copyright 2013 Jeremie Roy. All rights reserved.
* License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
*/

#include <limits.h> // INT_MAX

#include "CubeAtlas.h"
#include "Array.h"

class RectanglePacker
{
public:
	RectanglePacker();
	RectanglePacker(uint32_t _width, uint32_t _height);

	/// non constructor initialization
	void init(uint32_t _width, uint32_t _height);

	/// find a suitable position for the given rectangle
	/// @return true if the rectangle can be added, false otherwise
	bool addRectangle(uint16_t _width, uint16_t _height, uint16_t& _outX, uint16_t& _outY);

	/// return the used surface in squared unit
	uint32_t getUsedSurface()
	{
		return m_usedSpace;
	}

	/// return the total available surface in squared unit
	uint32_t getTotalSurface()
	{
		return m_width * m_height;
	}

	/// return the usage ratio of the available surface [0:1]
	float getUsageRatio();

	/// reset to initial state
	void clear();

private:
	int32_t fit(uint32_t _skylineNodeIndex, uint16_t _width, uint16_t _height);

	/// Merges all skyline nodes that are at the same level.
	void merge();

	struct Node
	{
		Node(int16_t _x, int16_t _y, int16_t _width) : x(_x), y(_y), width(_width)
		{
		}

		int16_t x;     //< The starting x-coordinate (leftmost).
		int16_t y;     //< The y-coordinate of the skyline level line.
		int32_t width; //< The line _width. The ending coordinate (inclusive) will be x+width-1.
	};


	uint32_t m_width;            //< width (in pixels) of the underlying texture
	uint32_t m_height;           //< height (in pixels) of the underlying texture
	uint32_t m_usedSpace;        //< Surface used in squared pixel
	Array<Node> m_skyline; //< node of the skyline algorithm
};

RectanglePacker::RectanglePacker()
	: m_width(0)
	, m_height(0)
	, m_usedSpace(0)
{
}

RectanglePacker::RectanglePacker(uint32_t _width, uint32_t _height)
	: m_width(_width)
	, m_height(_height)
	, m_usedSpace(0)
{
	// We want a one pixel border around the whole atlas to avoid any artefact when
	// sampling texture
	m_skyline.Push(Node(1, 1, uint16_t(_width - 2) ) );
}

void RectanglePacker::init(uint32_t _width, uint32_t _height)
{
	m_width = _width;
	m_height = _height;
	m_usedSpace = 0;

	m_skyline.Clear();
	// We want a one pixel border around the whole atlas to avoid any artifact when
	// sampling texture
	m_skyline.Push(Node(1, 1, uint16_t(_width - 2) ) );
}

bool RectanglePacker::addRectangle(uint16_t _width, uint16_t _height, uint16_t& _outX, uint16_t& _outY)
{
	int best_height, best_index;
	int32_t best_width;
	Node* node;
	Node* prev;
	_outX = 0;
	_outY = 0;

	best_height = INT_MAX;
	best_index = -1;
	best_width = INT_MAX;
	for (uint16_t ii = 0, num = uint16_t(m_skyline.GetSize() ); ii < num; ++ii)
	{
		int32_t yy = fit(ii, _width, _height);
		if (yy >= 0)
		{
			node = &m_skyline.Get(ii);
			if ( ( (yy + _height) < best_height)
			|| ( ( (yy + _height) == best_height) && (node->width < best_width) ) )
			{
				best_height = uint16_t(yy) + _height;
				best_index = ii;
				best_width = node->width;
				_outX = node->x;
				_outY = uint16_t(yy);
			}
		}
	}

	if (best_index == -1)
	{
		return false;
	}

	Node newNode(_outX, _outY + _height, _width);
	m_skyline.Insert(best_index, newNode);

	for (uint16_t ii = uint16_t(best_index + 1), num = uint16_t(m_skyline.GetSize() ); ii < num; ++ii)
	{
		node = &m_skyline.Get(ii);
		prev = &m_skyline.Get(ii - 1);
		if (node->x < (prev->x + prev->width) )
		{
			uint16_t shrink = uint16_t(prev->x + prev->width - node->x);
			node->x += shrink;
			node->width -= shrink;
			if (node->width <= 0)
			{
				m_skyline.Remove(ii);
				--ii;
				--num;
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

	merge();
	m_usedSpace += _width * _height;
	return true;
}

float RectanglePacker::getUsageRatio()
{
	uint32_t total = m_width * m_height;
	if (total > 0)
	{
		return (float)m_usedSpace / (float)total;
	}

	return 0.0f;
}

void RectanglePacker::clear()
{
	m_skyline.Clear();
	m_usedSpace = 0;

	// We want a one pixel border around the whole atlas to avoid any artefact when
	// sampling texture
	m_skyline.Push(Node(1, 1, uint16_t(m_width - 2) ) );
}

int32_t RectanglePacker::fit(uint32_t _skylineNodeIndex, uint16_t _width, uint16_t _height)
{
	int32_t width = _width;
	int32_t height = _height;

	const Node& baseNode = m_skyline.Get(_skylineNodeIndex);

	int32_t xx = baseNode.x, yy;
	int32_t widthLeft = width;
	int32_t ii = _skylineNodeIndex;

	if ( (xx + width) > (int32_t)(m_width - 1) )
	{
		return -1;
	}

	yy = baseNode.y;
	while (widthLeft > 0)
	{
		const Node& node = m_skyline.Get(ii);
		if (node.y > yy)
		{
			yy = node.y;
		}

		if ( (yy + height) > (int32_t)(m_height - 1) )
		{
			return -1;
		}

		widthLeft -= node.width;
		++ii;
	}

	return yy;
}

void RectanglePacker::merge()
{
	Node* node;
	Node* next;
	uint32_t ii;

	for (ii = 0; ii < m_skyline.GetSize() - 1; ++ii)
	{
		node = (Node*) &m_skyline.Get(ii);
		next = (Node*) &m_skyline.Get(ii + 1);
		if (node->y == next->y)
		{
			node->width += next->width;
			m_skyline.Remove(ii + 1);
			--ii;
		}
	}
}


struct Atlas::PackedLayer
{
	RectanglePacker packer;
	AtlasRegion faceRegion;
};


void MemSet(uint8_t* mem, uint8_t value, size_t len) {
	for (size_t i = 0;i < len;++i)
		mem[i] = value;
}


Atlas::Atlas(uint16_t atlas_size, uint16_t max_region_num, bool is_bgra)
	: used_faces_num(0)
	, atlas_size(atlas_size)
	, region_num(0)
	, max_region_num(max_region_num)
	, is_bgra(is_bgra) {

	this->texel_size = float(UINT16_MAX) / float(atlas_size);

	this->layers = new PackedLayer[6];
	for (int ii = 0; ii < 6; ++ii)
		this->layers[ii].packer.init(atlas_size, atlas_size);

	this->regions = new AtlasRegion[max_region_num];

	if (is_bgra) {
		this->atlas_buffer = new uint8_t[atlas_size * atlas_size * 6 * 4];
		MemSet(this->atlas_buffer, 0, atlas_size * atlas_size * 6 * 4);
	}
	else {
		this->atlas_buffer = new uint8_t[atlas_size * atlas_size * 6];
		MemSet(this->atlas_buffer, 0, atlas_size * atlas_size * 6);
	}
}


Atlas::~Atlas() {
	delete [] this->layers;
	delete [] this->regions;
	delete [] this->atlas_buffer;
}


uint16_t Atlas::addRegion(uint16_t width, uint16_t height, const uint8_t* bitmap_buffer, uint16_t margin) {
	if (this->region_num >= this->max_region_num)
		return UINT16_MAX;

	uint16_t xx = 0;
	uint16_t yy = 0;
	uint32_t idx = 0;
	while (idx < 6) {
		if (this->layers[idx].packer.addRectangle(width + margin * 2, height + margin * 2, xx, yy))
			break;
		++idx;
	}

	if (idx >= 6)
		return UINT16_MAX;

	AtlasRegion& region = this->regions[this->region_num];
	region.x = xx + margin;
	region.y = yy + margin;
	region.width = width;
	region.height = height;
	region.face_index = idx;

	this->updateRegion(region, bitmap_buffer);

	return this->region_num++;
}


void MemCopy(uint8_t* dst, const uint8_t* src, size_t len) {
	for (size_t i = 0;i < len;++i)
		dst[i] = src[i];
}


void Atlas::updateRegion(const AtlasRegion& region, const uint8_t* bitmap_buffer) {
	uint32_t size = region.width * region.height;
	if (size == 0)
		return;
	if (this->is_bgra)
		size *= 4;
	const uint8_t* src_buffer = bitmap_buffer;
	uint8_t* dst_buffer = this->atlas_buffer;
	if (this->is_bgra) {
		dst_buffer += region.face_index * (this->atlas_size * this->atlas_size * 4) + (((region.y * this->atlas_size * 4) + region.x * 4));
		for (int yy = 0; yy < region.height; ++yy) {
			MemCopy(dst_buffer, src_buffer, region.width * 4);
			src_buffer += region.width * 4;
			dst_buffer += this->atlas_size * 4;
		}
	}
	else {
		dst_buffer += region.face_index * (this->atlas_size * this->atlas_size) + (((region.y * this->atlas_size) + region.x));
		for (int yy = 0; yy < region.height; ++yy) {
			MemCopy(dst_buffer, src_buffer, region.width);
			src_buffer += region.width;
			dst_buffer += this->atlas_size;
		}
	}
}
