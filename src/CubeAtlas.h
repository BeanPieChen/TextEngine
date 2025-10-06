/*
 * Copyright 2013 Jeremie Roy. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#ifndef CUBE_ATLAS_H
#define CUBE_ATLAS_H

#include <cstdint>

#define MAX_REGION_NUM 4096

/// Inspired from texture-atlas from freetype-gl (http://code.google.com/p/freetype-gl/)
/// by Nicolas Rougier (Nicolas.Rougier@inria.fr)
/// The actual implementation is based on the article by Jukka Jylänki : "A
/// Thousand Ways to Pack the Bin - A Practical Approach to Two-Dimensional
/// Rectangle Bin Packing", February 27, 2010.
/// More precisely, this is an implementation of the Skyline Bottom-Left
/// algorithm based on C++ sources provided by Jukka Jylänki at:
/// http://clb.demon.fi/files/RectangleBinPack/

struct AtlasRegion
{
	uint16_t x, y;
	uint16_t width, height;
	uint8_t face_index;

};

class Atlas
{
	AtlasRegion error_region = {
		0,0,0,0,0
	};
public:
	/// create an empty dynamic atlas (region can be updated and added)
	/// @param textureSize an atlas creates a texture cube of 6 faces with size equal to (textureSize*textureSize * sizeof(RGBA) )
	/// @param maxRegionCount maximum number of region allowed in the atlas
	Atlas(uint16_t atlas_size, uint16_t max_regions_count = MAX_REGION_NUM, bool is_rgba = false);

	~Atlas();

	/// add a region to the atlas, and copy the content of mem to the underlying texture
	uint16_t addRegion(uint16_t width, uint16_t height, const uint8_t* bitmap_buffer, uint16_t outline = 0);

	/// update a preallocated region
	void updateRegion(const AtlasRegion& region, const uint8_t* bitmap_buffer);

	//retrieve a region info
	const AtlasRegion& getRegion(uint16_t region_index) const {
		if (region_index < MAX_REGION_NUM)
			return regions[region_index];
		else
			return this->error_region;
	}

	/// retrieve the size of side of a texture in pixels
	uint16_t getAtlasSize() const {
		return this->atlas_size;
	}

	/// retrieve the usage ratio of the atlas
	//float getUsageRatio() const { return 0.0f; }

	/// retrieve the numbers of region in the atlas
	uint16_t getRegionNum() const {
		return this->region_num;
	}

	/// retrieve a pointer to the region buffer (in order to serialize it)
	const AtlasRegion* getRegionBuffer() const {
		return this->regions;
	}

	/// retrieve the byte size of the texture
	uint32_t getTextureBufferSize() const {
		uint32_t ret = 6 * this->atlas_size * this->atlas_size;
		if (this->is_bgra)
			ret *= 4;
		return ret;
	}

	/// retrieve the mirrored texture buffer (to serialize it)
	const uint8_t* getAtlasBuffer() const {
		return this->atlas_buffer;
	}

private:
	struct PackedLayer;
	PackedLayer* layers;
	AtlasRegion* regions;
	uint8_t* atlas_buffer;

	uint32_t used_faces_num;

	uint16_t atlas_size;
	float texel_size;

	uint16_t region_num;
	uint16_t max_region_num;

	bool is_bgra;
};

#endif // CUBE_ATLAS_H
