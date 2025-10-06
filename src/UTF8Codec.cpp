//Copyright (C) 2025 Hongyi Chen (BeanPieChen)
//Licensed under the MIT License

#include <cstdint>

int UTF8Encode(uint32_t codepoint, unsigned char ret[4]) {
	uint32_t temp = codepoint;
	for (int i = 0; i < 3; i++) {
		temp >>= 8;
		if (temp == 0) {
			if (i == 0) {
				ret[0] = codepoint;
				return 1;
			}
			else {
				ret[0] = 0x80u;
				for (int j = i + 1; j >= 0; j--) {
					if (j == 0) {
						ret[0] |= codepoint;
					}
					else {
						ret[j] = (0x80u | (codepoint & 0x3fu));
						codepoint >>= 6;
						ret[0] >>= 1;
						ret[0] |= 0x80u;
					}
				}
				return i + 2;
			}
		}
	}

	return 0;
}



int UTF8Decode(const unsigned char* utf8_str, int offset, int len, uint32_t& codepoint) {
	if (utf8_str[offset] <= 0x7fu) {
		codepoint = utf8_str[offset];
		return 1;
	}
	int byte_num = 4;
	int byte = 0;
	uint32_t ret = 0;
	for (int i = offset; i < len && byte < byte_num; i++, byte++) {
		if (i == offset) {
			unsigned char temp = utf8_str[i];
			for (byte_num = 0; byte_num < 4; byte_num++) {
				if ((temp & 0x80u) == 0)
					break;
				temp <<= 1;
			}
			if (byte_num == 4)
				return 0;
			ret = 0;
			ret |= temp >> byte_num;
		}
		else {
			ret <<= 6;
			ret |= (utf8_str[i] & 0x3fu);
		}
	}

	if (byte != byte_num)
		return 0;
	else {
		codepoint = ret;
		return byte_num;
	}
}